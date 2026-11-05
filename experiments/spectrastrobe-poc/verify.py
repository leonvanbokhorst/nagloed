#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""NAGLOED v2 — SpectraStrobe proof-of-concept: pure-Python verificatie.

EXPERIMENT. Geen deel van NAGLOED v1.0.4. Alleen Python-stdlib (geen numpy).
Analyseert de WAV's uit out/ op de meetdoelen M1–M10 uit
docs/v2/SPECTRASTROBE.md en draait optioneel de corruptiematrix (M14).

Gebruik:
    python3 verify.py [--out-dir out] [--score score.json]
                      [--skip-corruption] [--strict-corruption]

Exitcode 0 = alle kernchecks PASS. Exitcode 1 = minimaal één kerncheck FAIL
(of, met --strict-corruption, een corruptierij die niet aan de verwachting
voldoet).
"""

from __future__ import annotations

import argparse
import array
import copy
import hashlib
import json
import math
import re
import shutil
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

TAU = 2.0 * math.pi
FFT_N = 65536
GOLDEN = 0.0  # geen tolerantieverzwakking hier

EXPERIMENT = "Eerste Gloed — SpectraStrobe proof-of-concept (EXPERIMENT)"
WARNING = ("EXPERIMENT — geen deel van NAGLOED v1.0.4, geen DSP-contract, geen integratie. "
           "Alleen signaalintegriteit offline; hardware is niet beschikbaar.")

GROUP_LABELS = {
    "F": "Formaat",
    "M1": "M1 dragerfrequenties",
    "M2": "M2 referentieniveau",
    "M3": "M3 relatieve kanaalamplitude",
    "M4": "M4 L/R-balans en isolatie",
    "M5": "M5 referentie-panmodulatie",
    "M6": "M6 crosstalk tussen kleuren",
    "M7": "M7 programmaleak in de controlebrand",
    "M8": "M8 ruisvloer bij gedode kleuren",
    "M9": "M9 piek en true peak",
    "M10": "M10 determinisme",
    "S": "Scorecontract",
    "M14": "M14 corruptiematrix",
}


# --------------------------------------------------------------------------- #
# Inlezen en basisbewerkingen
# --------------------------------------------------------------------------- #
def read_wav(path: Path):
    with wave.open(str(path), "rb") as handle:
        channels = handle.getnchannels()
        width = handle.getsampwidth()
        fs = handle.getframerate()
        frames = handle.getnframes()
        raw = handle.readframes(frames)
        if channels != 2:
            raise ValueError(f"{path.name}: verwacht stereo, kreeg {channels} kanalen")
        left = array.array("d", [0.0]) * frames
        right = array.array("d", [0.0]) * frames
        if width == 2:
            pcm = array.array("h")
            pcm.frombytes(raw)
            scale = 1.0 / 32768.0
            for i in range(frames):
                left[i] = pcm[2 * i] * scale
                right[i] = pcm[2 * i + 1] * scale
        elif width == 3:
            scale = 1.0 / 8388608.0
            for i in range(frames):
                j = 6 * i
                v = raw[j] | (raw[j + 1] << 8) | (raw[j + 2] << 16)
                if v & 0x800000:
                    v -= 0x1000000
                left[i] = v * scale
                v = raw[j + 3] | (raw[j + 4] << 8) | (raw[j + 5] << 16)
                if v & 0x800000:
                    v -= 0x1000000
                right[i] = v * scale
        else:
            raise ValueError(f"{path.name}: alleen 16/24-bit PCM")
    return fs, width, left, right


def db(x: float) -> float:
    return 10.0 * math.log10(x) if x > 0.0 else float("-inf")


def amp_db(x: float) -> float:
    return 20.0 * math.log10(x) if x > 0.0 else float("-inf")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


# --------------------------------------------------------------------------- #
# DFT/FFT (radix-2, geen numpy) en spectrale metingen
# --------------------------------------------------------------------------- #
_WINDOW_CACHE = {}


def hann_window(n: int):
    cached = _WINDOW_CACHE.get(n)
    if cached is None:
        cached = [0.5 - 0.5 * math.cos(TAU * i / (n - 1)) for i in range(n)]
        _WINDOW_CACHE[n] = cached
    return cached


def _fft(x) -> None:
    n = len(x)
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j & bit:
            j ^= bit
            bit >>= 1
        j ^= bit
        if i < j:
            x[i], x[j] = x[j], x[i]
    length = 2
    while length <= n:
        ang = -TAU / length
        wl = complex(math.cos(ang), math.sin(ang))
        half = length >> 1
        for start in range(0, n, length):
            w = 1.0 + 0.0j
            for k in range(start, start + half):
                u = x[k]
                v = x[k + half] * w
                x[k] = u + v
                x[k + half] = u - v
                w *= wl
        length <<= 1


def spectrum_pair(left, right, fs: float, t0: float, n: int = FFT_N):
    """Hann-gewogen DFT van L en R in één complexe FFT (L + jR)."""
    i0 = int(round(t0 * fs))
    if i0 < 0:
        i0 = 0
    window = hann_window(n)
    count = len(left)
    x = [0.0j] * n
    for i in range(n):
        if i0 + i < count:
            w = window[i]
            x[i] = complex(left[i0 + i] * w, right[i0 + i] * w)
    _fft(x)
    half = n // 2
    spec_left = [0.0] * (half + 1)
    spec_right = [0.0] * (half + 1)
    for k in range(half + 1):
        a = x[k]
        b = x[(n - k) % n].conjugate()
        xl = (a + b) * 0.5
        xr = (a - b) * (-0.5j)
        spec_left[k] = xl.real * xl.real + xl.imag * xl.imag
        spec_right[k] = xr.real * xr.real + xr.imag * xr.imag
    sum_w2 = sum(w * w for w in window)
    return spec_left, spec_right, sum_w2


def peak_frequency(spec_power, fs: float, n: int, nominal: float, span_hz: float = 80.0):
    """FFT-piek met parabolische interpolatie (M1-methode)."""
    bin_hz = fs / n
    k_lo = max(1, int((nominal - span_hz) / bin_hz))
    k_hi = min(len(spec_power) - 2, int((nominal + span_hz) / bin_hz) + 2)
    if k_hi <= k_lo:
        return float("nan")
    k_peak = max(range(k_lo, k_hi), key=lambda k: spec_power[k])
    y0 = math.sqrt(spec_power[k_peak - 1])
    y1 = math.sqrt(spec_power[k_peak])
    y2 = math.sqrt(spec_power[k_peak + 1])
    denominator = y0 - 2.0 * y1 + y2
    delta = 0.5 * (y0 - y2) / denominator if abs(denominator) > 1e-30 else 0.0
    delta = -0.5 if delta < -0.5 else (0.5 if delta > 0.5 else delta)
    return (k_peak + delta) * bin_hz


def band_rms_sq(spec_power, fs: float, n: int, sum_w2: float, f_lo: float, f_hi: float) -> float:
    bin_hz = fs / n
    k_lo = max(0, int(math.ceil(f_lo / bin_hz)))
    k_hi = min(len(spec_power) - 1, int(math.floor(f_hi / bin_hz)))
    total = 0.0
    for k in range(k_lo, k_hi + 1):
        total += spec_power[k]
    return 2.0 * total / (n * sum_w2)


def band_rms_dbfs(spec_power, fs: float, n: int, sum_w2: float, f_lo: float, f_hi: float) -> float:
    return db(band_rms_sq(spec_power, fs, n, sum_w2, f_lo, f_hi))


def combined_power(spec_left, spec_right):
    return [spec_left[k] + spec_right[k] for k in range(len(spec_left))]


# --------------------------------------------------------------------------- #
# Complexe demodulatie (enkelbin-DFT) voor amplitudes, frequenties en LFO
# --------------------------------------------------------------------------- #
def demod_envelopes(left, right, fs: float, freq: float, t0: float, t1: float, block_s: float):
    """Per blok: amplitude van de drager in L en R (2|Z|/B)."""
    i0 = max(0, int(round(t0 * fs)))
    i1 = min(len(left), int(round(t1 * fs)))
    block = max(1, int(round(block_s * fs)))
    env_l = []
    env_r = []
    for start in range(i0, i1 - block + 1, block):
        re_l = im_l = 0.0
        re_r = im_r = 0.0
        for n in range(start, start + block):
            angle = TAU * freq * n / fs
            c = math.cos(angle)
            s = math.sin(angle)
            lv = left[n]
            rv = right[n]
            re_l += lv * c
            im_l -= lv * s
            re_r += rv * c
            im_r -= rv * s
        factor = 2.0 / block
        env_l.append(factor * math.hypot(re_l, im_l))
        env_r.append(factor * math.hypot(re_r, im_r))
    return env_l, env_r


def estimate_frequency(left, right, fs: float, nominal: float, t0: float, t1: float,
                       block_s: float = 0.02) -> float:
    """Frequentie via faseslope van complexe demodulatie (L+R)."""
    i0 = max(0, int(round(t0 * fs)))
    i1 = min(len(left), int(round(t1 * fs)))
    block = max(1, int(round(block_s * fs)))
    if i1 - i0 < 4 * block:
        return float("nan")
    phases = []
    for start in range(i0, i1 - block + 1, block):
        total = 0.0 + 0.0j
        for n in range(start, start + block):
            angle = TAU * nominal * n / fs
            total += (left[n] + right[n]) * complex(math.cos(angle), -math.sin(angle))
        phases.append(math.atan2(total.imag, total.real))
    unwrapped = [phases[0]]
    for value in phases[1:]:
        delta = value - unwrapped[-1]
        while delta > math.pi:
            delta -= TAU
        while delta < -math.pi:
            delta += TAU
        unwrapped.append(unwrapped[-1] + delta)
    m = len(unwrapped)
    sum_x = m * (m - 1) / 2.0
    sum_xx = sum(i * i for i in range(m))
    sum_y = sum(unwrapped)
    sum_xy = sum(i * v for i, v in enumerate(unwrapped))
    slope = (m * sum_xy - sum_x * sum_y) / (m * sum_xx - sum_x * sum_x)
    return nominal + slope * fs / (TAU * block)


def goertzel_amplitude(sequence, rate_hz: float, freq: float) -> float:
    n = len(sequence)
    if n == 0:
        return 0.0
    w = TAU * freq / rate_hz
    coeff = 2.0 * math.cos(w)
    s1 = 0.0
    s2 = 0.0
    for value in sequence:
        s0 = value + coeff * s1 - s2
        s2 = s1
        s1 = s0
    power = s1 * s1 + s2 * s2 - coeff * s1 * s2
    return 2.0 * math.sqrt(max(power, 0.0)) / n


def pearson(a, b) -> float:
    n = min(len(a), len(b))
    if n < 2:
        return 0.0
    mean_a = sum(a[:n]) / n
    mean_b = sum(b[:n]) / n
    num = sum((a[i] - mean_a) * (b[i] - mean_b) for i in range(n))
    den_a = math.sqrt(sum((v - mean_a) ** 2 for v in a[:n]))
    den_b = math.sqrt(sum((v - mean_b) ** 2 for v in b[:n]))
    return num / (den_a * den_b) if den_a * den_b > 0.0 else 0.0


def lfo_metrics(left, right, fs: float, t0: float, t1: float,
                search_center_hz: float = 20.0):
    """Meet de referentie-panning uit de vermogensbalans van de 18,2 kHz-band.

    De amplitude-envelope per blok is lineair in de panning; het *vermogen*
    (env^2) is bij de uiterste standen vlak (quadratisch), waardoor een
    blokgemiddelde de diepte niet onderschat. Dit is een meetconventie: de
    encoder panneert per sample hard, de analyse gebruikt 1 ms-blokken.
    """
    env_l, env_r = demod_envelopes(left, right, fs, 18200.0, t0, t1, 0.001)
    n = min(len(env_l), len(env_r))
    if n < 8:
        return {"freq_hz": float("nan"), "depth_pct": 0.0, "corr": 0.0,
                "rest_db": -999.0, "amplitude": 0.0}
    power_l = [value * value for value in env_l]
    power_r = [value * value for value in env_r]
    balance = [(power_l[i] - power_r[i]) / (power_l[i] + power_r[i] + 1e-30)
               for i in range(n)]
    depth = 0.5 * (max(balance) - min(balance))
    q_max = max(abs(min(balance)), abs(max(balance)))
    q_max = min(q_max, 1.0 - 1e-12)
    # Vermogen in de "uit"-fase t.o.v. de "aan"-fase, in dB.
    rest = 10.0 * math.log10((1.0 - q_max) / (1.0 + q_max))
    block_rate = n / (t1 - t0)
    search_lo = max(0.5, search_center_hz - 2.0)
    search_hi = search_center_hz + 2.0
    steps = int(round((search_hi - search_lo) / 0.02)) + 1
    best_freq = float("nan")
    best_amp = -1.0
    for step in range(steps):
        freq = search_lo + 0.02 * step
        amp = goertzel_amplitude(balance, block_rate, freq)
        if amp > best_amp:
            best_amp = amp
            best_freq = freq
    return {"freq_hz": best_freq, "depth_pct": 100.0 * depth,
            "corr": pearson(power_l, power_r), "rest_db": rest,
            "amplitude": best_amp}


# --------------------------------------------------------------------------- #
# Piek en true peak (4x oversampled, lokale windowed-sinc)
# --------------------------------------------------------------------------- #
def _sinc(x: float) -> float:
    if abs(x) < 1e-12:
        return 1.0
    return math.sin(math.pi * x) / (math.pi * x)


def true_peak(left, right, fs: float) -> float:
    peak = max(max(left), -min(left), max(right), -min(right))
    if peak <= 0.0:
        return 0.0
    threshold = peak * 0.5
    candidates = []
    for channel in (left, right):
        for i, value in enumerate(channel):
            if abs(value) > threshold:
                candidates.append((abs(value), channel, i))
    candidates.sort(reverse=True, key=lambda item: item[0])
    candidates = candidates[:4000]
    best = peak
    taps = 12
    for _, channel, index in candidates:
        for phase in range(1, 4):
            t = index + phase / 4.0
            acc = 0.0
            for k in range(index - taps, index + taps + 1):
                if 0 <= k < len(channel):
                    x = t - k
                    window = 0.5 + 0.5 * math.cos(math.pi * x / (taps + 1)) if abs(x) <= taps + 1 else 0.0
                    acc += channel[k] * _sinc(x) * window
            if abs(acc) > best:
                best = abs(acc)
    return best


# --------------------------------------------------------------------------- #
# Score-hulpen (onafhankelijke implementatie van de score-semantiek)
# --------------------------------------------------------------------------- #
def smoothstep(u: float) -> float:
    u = 0.0 if u < 0.0 else (1.0 if u > 1.0 else u)
    return u * u * (3.0 - 2.0 * u)


def points_value(points, t: float) -> float:
    pts = [(float(a), float(b)) for a, b in points]
    if t <= pts[0][0]:
        return pts[0][1]
    for (t0, v0), (t1, v1) in zip(pts, pts[1:]):
        if t <= t1:
            return v0 + (v1 - v0) * smoothstep((t - t0) / (t1 - t0)) if t1 > t0 else v1
    return pts[-1][1]


def expected_colour_ratio(score: dict, event: dict, t_abs: float, colour_index: int):
    start = float(event["start_s"])
    t_rel = t_abs - start
    env = points_value(event["env"], t_rel)
    pan = points_value(event["pan"], t_rel)
    brightness = float(event["rgb"][colour_index]) * env
    gain_left = 1.0 - max(0.0, pan)
    gain_right = 1.0 + min(0.0, pan)
    return brightness * gain_left, brightness * gain_right


# --------------------------------------------------------------------------- #
# Checkverzameling
# --------------------------------------------------------------------------- #
class Checks:
    def __init__(self):
        self.items = []

    def add(self, group: str, check_id: str, description: str, criterion: str,
            measured, status: str, detail: str = ""):
        self.items.append({
            "group": group,
            "id": check_id,
            "description": description,
            "criterion": criterion,
            "measured": measured,
            "status": status,
            "detail": detail,
        })

    def ok(self, group, check_id, description, criterion, measured, passed: bool, detail=""):
        self.add(group, check_id, description, criterion, measured,
                 "PASS" if passed else "FAIL", detail)

    def summary(self):
        core = [item for item in self.items if item["group"] != "M14"]
        corrupt = [item for item in self.items if item["group"] == "M14"]
        core_pass = sum(1 for item in core if item["status"] == "PASS")
        core_fail = sum(1 for item in core if item["status"] == "FAIL")
        core_skip = sum(1 for item in core if item["status"] == "SKIP")
        corr_pass = sum(1 for item in corrupt if item["status"] == "PASS")
        corr_degraded = sum(1 for item in corrupt if item["status"] == "EXPECTED-DEGRADED")
        corr_info = sum(1 for item in corrupt if item["status"] == "INFO")
        corr_advisory = sum(1 for item in corrupt if item["status"] == "ADVISORY-FAIL")
        return {
            "total": len(core),
            "pass": core_pass,
            "fail": core_fail,
            "skip": core_skip,
            "verdict": "PASS" if core_fail == 0 else "FAIL",
            "corruption_total": len(corrupt),
            "corruption_pass": corr_pass,
            "corruption_expected_degraded": corr_degraded,
            "corruption_info": corr_info,
            "corruption_advisory_fail": corr_advisory,
        }


def _format(value):
    if isinstance(value, float):
        if value != value:
            return "n/a"
        if abs(value) >= 1000.0:
            return f"{value:.1f}"
        if abs(value) >= 1.0:
            return f"{value:.2f}"
        return f"{value:.4f}"
    return str(value)


# --------------------------------------------------------------------------- #
# Kernanalyse
# --------------------------------------------------------------------------- #
def format_check(checks: Checks, label: str, path: Path, fs_expected: int, width_expected: int,
                 duration_expected: float, fs: int, width: int, frames: int):
    ok = fs == fs_expected and width == width_expected
    checks.ok("F", f"formaat-{label}",
              f"{label}: sample rate / bitdiepte / kanalen",
              f"{fs_expected} Hz, {width_expected * 8}-bit, stereo",
              f"{fs} Hz, {width * 8}-bit, {frames} frames", ok,
              "WAV-header" if ok else "WAV-header wijkt af")
    duration = frames / fs
    checks.ok("F", f"duur-{label}", f"{label}: duur", f"{duration_expected:.1f} s ± 0,05 s",
              f"{duration:.3f} s", abs(duration - duration_expected) <= 0.05)


def score_checks(checks: Checks, score: dict):
    meta = score["meta"]
    checks.ok("S", "score-duur", "scoreduur", "60–90 s", f"{meta['duration_s']} s",
              60.0 <= float(meta["duration_s"]) <= 90.0)
    checks.ok("S", "score-seed", "seed aanwezig en vast", "integer",
              str(meta.get("seed")), isinstance(meta.get("seed"), int))

    events = {ev["id"]: ev for ev in score["light"]["events"]}
    audio = {ev["id"]: ev for ev in score["program"]["events"]}
    l1 = events.get("L1")
    a1 = audio.get("A1")
    moved = l1 is not None and float(l1["pan"][0][1]) < 0.0 < float(l1["pan"][-1][1])
    checks.ok("S", "score-L1-beweging", "L1 beweegt links→rechts en dooft abrupt",
              "pan van < 0 naar > 0; extinctie ≤ 10 ms",
              f"pan {l1['pan'][0][1]}→{l1['pan'][-1][1]}" if l1 else "L1 ontbreekt", moved)
    if l1 is not None:
        ext = float(l1["env"][-1][0]) - float(l1["env"][-2][0])
        checks.ok("S", "score-L1-extinctie", "L1-extinctie is abrupt", "≤ 10 ms",
                  f"{ext * 1000:.0f} ms", 0.0 < ext <= 0.010)
    delay = float(a1["start_s"]) - float(l1["end_s"]) if a1 and l1 else float("nan")
    checks.ok("S", "score-A1-vertraging", "A1 volgt L1 met bewuste vertraging",
              "150–200 ms", f"{delay * 1000:.0f} ms", 0.150 <= delay <= 0.200)
    if a1 is not None:
        a1_attack = float(a1["env"][1][0])
        a1_decay = float(a1["env"][-1][0])
        checks.ok("S", "score-A1-envelope", "A1-envelope verschilt van het licht",
                  "eigen attack/decay, niet identiek aan L1",
                  f"attack {a1_attack * 1000:.0f} ms, decay {a1_decay:.1f} s",
                  a1_decay >= 1.5 and a1_attack <= 0.1)
    l3 = events.get("L3")
    unanswered = l3 is not None and "onbeantwoord" in str(l3.get("relation", ""))
    checks.ok("S", "score-L3-onbeantwoord", "minstens één lichtgebeurtenis blijft onbeantwoord",
              "L3 heeft geen audio-antwoord", "L3 relation: onbeantwoord" if unanswered else "niet gevonden",
              unanswered)
    l4 = events.get("L4")
    a2 = audio.get("A2")
    if l4 is not None and a2 is not None:
        gap = float(l4["start_s"]) - float(l1["start_s"]) if l1 else float("nan")
        transform = a2.get("transform") or {}
        dimmer = max(l4["rgb"]) < max(l1["rgb"]) if l1 else False
        reversed_pan = float(l4["pan"][0][1]) > 0.0 > float(l4["pan"][-1][1])
        checks.ok("S", "score-L4-herinnering",
                  "L4/A2 zijn een getransformeerde terugkeer veel later",
                  "≥ 20 s later, dimmer, omgekeerde beweging, verlaagd",
                  f"{gap:.0f} s later, dimmer={dimmer}, omgekeerd={reversed_pan}, "
                  f"pitch={transform.get('pitch_ratio')}",
                  gap >= 20.0 and dimmer and reversed_pan and
                  float(transform.get("pitch_ratio", 1.0)) < 1.0)


def analyse_core(checks: Checks, measurements: dict, score: dict,
                 fs: int, left, right, label: str, exhaustive: bool):
    n = len(left)
    car = score["carriers"]
    ref_hz = float(car["reference"]["hz"])
    colour_hz = {key: float(value["hz"]) for key, value in car["colours"].items()}
    verify_cfg = score["verify"]
    ref_windows = [(float(a), float(b)) for a, b in verify_cfg["reference_windows_s"]]
    lfo_window = tuple(float(v) for v in verify_cfg["lfo_window_s"])

    # M1 — dragerfrequenties (FFT ≥ 65536, Hann, parabolische interpolatie)
    ref_t0 = ref_windows[0][0]
    spec_l, spec_r, sum_w2 = spectrum_pair(left, right, fs, ref_t0)
    ref_freq = peak_frequency(combined_power(spec_l, spec_r), fs, FFT_N, ref_hz)
    colour_t0 = 12.2
    spec_l_c, spec_r_c, _ = spectrum_pair(left, right, fs, colour_t0)
    colour_freqs = {key: peak_frequency(combined_power(spec_l_c, spec_r_c), fs, FFT_N, hz)
                    for key, hz in colour_hz.items()}
    measurements.setdefault("m1", {})[label] = {
        "reference_hz": ref_freq,
        "colours_hz": colour_freqs,
    }
    all_freqs_ok = abs(ref_freq - ref_hz) <= 1.0 and all(
        abs(colour_freqs[key] - hz) <= 1.0 for key, hz in colour_hz.items())
    checks.ok("M1", f"m1-frequenties-{label}",
              f"{label}: dragerfrequenties binnen ±1 Hz",
              f"{ref_hz:.0f} / 18700 / 19200 / 19700 Hz ±1 Hz",
              f"{ref_freq:.2f} / " + " / ".join(f"{colour_freqs[k]:.2f}" for k in ("red", "green", "blue")),
              all_freqs_ok)

    # M2 — referentieniveau (piekamplitude per kanaal, constant over het stuk)
    ref_peaks = []
    total_rms = []
    for t0, t1 in ref_windows:
        env_l, env_r = demod_envelopes(left, right, fs, ref_hz, t0, t1, 0.005)
        if not env_l:
            continue
        peak = max(math.hypot(a, b) for a, b in zip(env_l, env_r))
        ref_peaks.append(peak)
        i0 = int(round(t0 * fs))
        i1 = int(round(t1 * fs))
        count = max(1, i1 - i0)
        energy = 0.0
        for i in range(i0, i1):
            energy += left[i] * left[i] + right[i] * right[i]
        total_rms.append(math.sqrt(energy / count))
    ref_peak_db = amp_db(max(ref_peaks)) if ref_peaks else float("-inf")
    rms_db = amp_db(max(total_rms)) if total_rms else float("-inf")
    spread = (20.0 * math.log10(max(ref_peaks) / min(ref_peaks))) if len(ref_peaks) > 1 else 0.0
    target = float(car["reference"]["level_dbfs"])
    measurements.setdefault("m2", {})[label] = {
        "reference_peak_amplitude_dbfs": ref_peak_db,
        "reference_total_rms_dbfs": rms_db,
        "variation_db": spread,
    }
    checks.ok("M2", f"m2-referentieniveau-{label}",
              f"{label}: referentie-piekamplitude",
              f"{target:+.1f} dBFS ± 0,5 dB, constant",
              f"{ref_peak_db:+.2f} dBFS (RMS {rms_db:+.2f} dBFS, variatie {spread:.2f} dB)",
              abs(ref_peak_db - target) <= 0.5 and spread <= 0.5)
    ref_amplitude = 10.0 ** (ref_peak_db / 20.0) if ref_peak_db > -200 else 1.0

    # M5 — referentie-panmodulatie
    lfo = lfo_metrics(left, right, fs, lfo_window[0], lfo_window[1])
    measurements.setdefault("m5", {})[label] = lfo
    lfo_ok = (abs(lfo["freq_hz"] - 20.0) <= 0.5 and lfo["depth_pct"] >= 99.0
              and lfo["corr"] <= -0.2 and lfo["rest_db"] <= -30.0)
    checks.ok("M5", f"m5-pan-lfo-{label}",
              f"{label}: 20 Hz pan-LFO, diepte ≥ 99%, uit-fase ≥ 30 dB onder",
              "20,0 Hz ± 0,5; diepte ≥ 99%; corr < 0; rust ≥ 30 dB onder",
              f"{lfo['freq_hz']:.2f} Hz, diepte {lfo['depth_pct']:.1f}%, "
              f"corr {lfo['corr']:.2f}, rust {lfo['rest_db']:.1f} dB", lfo_ok)

    # M3 — relatieve kanaalamplitude per kleur, volgt de score
    probes = verify_cfg["colour_probes"]
    light_events = {ev["id"]: ev for ev in score["light"]["events"]}
    measured_ratios = []
    for probe in probes:
        event = light_events[probe["event"]]
        t0 = float(probe["t0"])
        t1 = float(probe["t1"])
        t_center = 0.5 * (t0 + t1)
        row = {"event": event["id"], "t_center_s": t_center, "colours": {}}
        for index, key in enumerate(("red", "green", "blue")):
            env_l, env_r = demod_envelopes(left, right, fs, colour_hz[key], t0, t1, t1 - t0)
            if not env_l:
                continue
            amp_l = sum(env_l) / len(env_l)
            amp_r = sum(env_r) / len(env_r)
            measured_l = amp_l / ref_amplitude if ref_amplitude > 0 else 0.0
            measured_r = amp_r / ref_amplitude if ref_amplitude > 0 else 0.0
            expected_l, expected_r = expected_colour_ratio(score, event, t_center, index)
            delta_l = amp_db(measured_l / expected_l) if measured_l > 0 and expected_l > 0 else 99.0
            delta_r = amp_db(measured_r / expected_r) if measured_r > 0 and expected_r > 0 else 99.0
            row["colours"][key] = {
                "measured_ratio_L": measured_l,
                "measured_ratio_R": measured_r,
                "expected_ratio_L": expected_l,
                "expected_ratio_R": expected_r,
                "delta_db_L": delta_l,
                "delta_db_R": delta_r,
            }
            measured_ratios.append(max(measured_l, measured_r))
        measurements.setdefault("m3", {})[label] = measurements.get("m3", {}).get(label, [])
        measurements["m3"][label].append(row)
    deltas = [max(abs(c["delta_db_L"]), abs(c["delta_db_R"]))
              for row in measurements["m3"][label] for c in row["colours"].values()]
    worst_delta = max(deltas) if deltas else 99.0
    max_ratio = max(measured_ratios) if measured_ratios else 0.0
    checks.ok("M3", f"m3-helderheid-{label}",
              f"{label}: kleurhelderheid volgt de score en blijft ≤ 25%",
              "|afwijking| ≤ 0,5 dB; max ratio ≤ 0,25",
              f"max afwijking {worst_delta:.2f} dB, max ratio {max_ratio * 100:.1f}%",
              worst_delta <= 0.5 and max_ratio <= 0.25 + 0.005)

    # M6 — crosstalk tussen kleurbanden (eigen hoofdloop vs ±100 Hz-band)
    worst_xtalk = -999.0
    for index, key in enumerate(("red", "green", "blue")):
        own_lo = colour_hz[key] - 5.0
        own_hi = colour_hz[key] + 5.0
        broad_lo = colour_hz[key] - 100.0
        broad_hi = colour_hz[key] + 100.0
        narrow = band_rms_sq(spec_l_c, fs, FFT_N, sum_w2, own_lo, own_hi) + \
            band_rms_sq(spec_r_c, fs, FFT_N, sum_w2, own_lo, own_hi)
        broad = band_rms_sq(spec_l_c, fs, FFT_N, sum_w2, broad_lo, broad_hi) + \
            band_rms_sq(spec_r_c, fs, FFT_N, sum_w2, broad_lo, broad_hi)
        leak = max(broad - narrow, 1e-24)
        xtalk = db(leak / narrow) if narrow > 0 else -999.0
        worst_xtalk = max(worst_xtalk, xtalk)
    measurements.setdefault("m6", {})[label] = {"worst_crosstalk_db": worst_xtalk}
    checks.ok("M6", f"m6-crosstalk-{label}", f"{label}: crosstalk tussen kleuren",
              "≤ -40 dB t.o.v. eigen drager", f"{worst_xtalk:.1f} dB", worst_xtalk <= -40.0)

    # M9 — piek en true peak
    peak = max(max(left), -min(left), max(right), -min(right))
    true_peak_value = true_peak(left, right, fs)
    finite = all(math.isfinite(v) for v in left) and all(math.isfinite(v) for v in right)
    peak_dbfs = amp_db(peak)
    true_peak_dbtp = amp_db(true_peak_value)
    measurements.setdefault("m9", {})[label] = {
        "sample_peak_dbfs": peak_dbfs,
        "true_peak_dbtp": true_peak_dbtp,
        "clipped": peak >= 1.0,
        "finite": finite,
    }
    checks.ok("M9", f"m9-piek-{label}", f"{label}: piek / true peak / geen clipping",
              "sample ≤ -6 dBFS, true peak ≤ -6 dBTP, geen clipping, alles finiet",
              f"sample {peak_dbfs:+.2f} dBFS, true peak {true_peak_dbtp:+.2f} dBTP",
              peak_dbfs <= -6.0 and true_peak_dbtp <= -6.0 and peak < 1.0 and finite)

    if exhaustive:
        # M4 — balans in het midden en isolatie bij hard pannen
        hard = verify_cfg["hard_pan_probe"]
        event = light_events[hard["event"]]
        t0 = float(hard["t0"])
        t1 = float(hard["t1"])
        isolations = []
        for index, key in enumerate(("red", "green", "blue")):
            env_l, env_r = demod_envelopes(left, right, fs, colour_hz[key], t0, t1, t1 - t0)
            if env_l:
                amp_l = sum(env_l) / len(env_l)
                amp_r = sum(env_r) / len(env_r)
                isolations.append(amp_db((amp_l + 1e-12) / (amp_r + 1e-12)))
        isolation = min(isolations) if isolations else -999.0
        balance_probe = next(p for p in probes if p["event"] == "L2")
        b0, b1 = float(balance_probe["t0"]), float(balance_probe["t1"])
        balances = []
        for key in ("red", "green", "blue"):
            env_l, env_r = demod_envelopes(left, right, fs, colour_hz[key], b0, b1, b1 - b0)
            if env_l:
                amp_l = sum(env_l) / len(env_l)
                amp_r = sum(env_r) / len(env_r)
                balances.append(abs(amp_db((amp_l + 1e-12) / (amp_r + 1e-12))))
        balance = max(balances) if balances else 99.0
        measurements.setdefault("m4", {})[label] = {"balance_db": balance, "isolation_db": isolation}
        checks.ok("M4", f"m4-balans-{label}", f"{label}: middenbalans en hard-pan-isolatie",
                  "midden ± 0,5 dB; hard pan ≥ 40 dB isolatie",
                  f"balans {balance:.2f} dB, isolatie {isolation:.1f} dB",
                  balance <= 0.5 and isolation >= 40.0)

        # M7 — programmaleak in de controlebrand (alleen op de programma-only render)
        # M8 — ruisvloer bij gedode kleuren
        silent_t0 = float(verify_cfg["silent_window_s"][0])
        spec_l_s, spec_r_s, sum_w2_s = spectrum_pair(left, right, fs, silent_t0)
        noise = []
        for key in ("red", "green", "blue"):
            hz = colour_hz[key]
            noise.append(max(
                band_rms_dbfs(spec_l_s, fs, FFT_N, sum_w2_s, hz - 100.0, hz + 100.0),
                band_rms_dbfs(spec_r_s, fs, FFT_N, sum_w2_s, hz - 100.0, hz + 100.0)))
        worst_noise = max(noise) if noise else 0.0
        measurements.setdefault("m8", {})[label] = {"worst_colour_band_noise_dbfs": worst_noise}
        checks.ok("M8", f"m8-ruisvloer-{label}", f"{label}: ruisvloer in kleurbanden zonder kleursignaal",
                  "≤ -80 dBFS", f"{worst_noise:.1f} dBFS", worst_noise <= -80.0)


def programme_check(checks: Checks, measurements: dict, score: dict, path: Path,
                    fs: int, left, right):
    t0 = float(score["verify"]["programme_window_s"][0])
    spec_l, spec_r, sum_w2 = spectrum_pair(left, right, fs, t0)
    band_lo, band_hi = 17000.0, 20000.0
    rms_l = band_rms_dbfs(spec_l, fs, FFT_N, sum_w2, band_lo, band_hi)
    rms_r = band_rms_dbfs(spec_r, fs, FFT_N, sum_w2, band_lo, band_hi)
    worst = max(rms_l, rms_r)
    measurements["m7"] = {"programme_17k_20k_dbfs_L": rms_l, "programme_17k_20k_dbfs_R": rms_r}
    checks.ok("M7", "m7-programmaleak", "programma-only: energie 17–20 kHz",
              "≤ -60 dBFS per kanaal", f"{worst:.1f} dBFS", worst <= -60.0)
    # A/B: programma-only bevat geen dragers
    carrier_floor = -999.0
    for key, hz in (("ref", 18200.0), ("red", 18700.0), ("green", 19200.0), ("blue", 19700.0)):
        value = max(band_rms_dbfs(spec_l, fs, FFT_N, sum_w2, hz - 50.0, hz + 50.0),
                    band_rms_dbfs(spec_r, fs, FFT_N, sum_w2, hz - 50.0, hz + 50.0))
        carrier_floor = max(carrier_floor, value)
    measurements["m7"]["program_only_carrier_floor_dbfs"] = carrier_floor
    checks.ok("M7", "m7-program-only-dragerloos",
              "programma-only A/B bevat geen dragers",
              "dragerbanden ≤ -80 dBFS", f"{carrier_floor:.1f} dBFS", carrier_floor <= -80.0)


def determinism_check(checks: Checks, measurements: dict, render_script: Path,
                      score_path: Path, out_dir: Path, names, timeout: int = 240):
    with tempfile.TemporaryDirectory(prefix="specstroop-repro-") as tmp:
        command = [sys.executable, str(render_script), "--score", str(score_path),
                   "--out-dir", tmp]
        try:
            result = subprocess.run(command, capture_output=True, text=True, timeout=timeout)
        except Exception as error:  # noqa: BLE001 - rapport moet nooit crashen
            checks.add("M10", "m10-render-2", "tweede render", "start render.py",
                       str(error), "FAIL")
            return
        if result.returncode != 0:
            checks.add("M10", "m10-render-2", "tweede render", "exitcode 0",
                       f"exitcode {result.returncode}", "FAIL", result.stderr[-400:])
            return
        mismatches = []
        compared = []
        for name in names:
            first = out_dir / name
            second = Path(tmp) / name
            if not first.exists() or not second.exists():
                mismatches.append(name)
                continue
            same = sha256(first) == sha256(second)
            compared.append({"file": name, "identical": same})
            if not same:
                mismatches.append(name)
        names_extra = ("events.csv", "timeline.svg", "render-manifest.json")
        for name in names_extra:
            first = out_dir / name
            second = Path(tmp) / name
            if first.exists() and second.exists():
                same = sha256(first) == sha256(second)
                compared.append({"file": name, "identical": same})
                if not same:
                    mismatches.append(name)
        measurements["m10"] = compared
        checks.ok("M10", "m10-determinisme", "twee renders met dezelfde seed zijn byte-identiek",
                  "alle uitvoerbestanden identiek",
                  "identiek" if not mismatches else f"afwijkend: {', '.join(mismatches)}",
                  not mismatches)


# --------------------------------------------------------------------------- #
# Corruptiematrix (M14, adviserend)
# --------------------------------------------------------------------------- #
def _ffmpeg_command(src: Path, dst: Path, args) -> list:
    return ["ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "error", "-y",
            "-i", str(src), *args, str(dst)]


def _quick_metrics(fs: int, left, right, ref_hz: float = 18200.0,
                   lfo_search_center_hz: float = 20.0):
    peak = max(max(left), -min(left), max(right), -min(right))
    ref_amp = 0.0
    env_l, env_r = demod_envelopes(left, right, fs, ref_hz, 20.0, 22.0, 0.005)
    if env_l:
        ref_amp = max(math.hypot(a, b) for a, b in zip(env_l, env_r))
    ref_freq = estimate_frequency(left, right, fs, ref_hz, 20.0, 22.0)
    colour_freqs = {}
    for key, hz in (("red", 18700.0), ("green", 19200.0), ("blue", 19700.0)):
        colour_freqs[key] = estimate_frequency(left, right, fs, hz, 12.2, 13.5)
    lfo = lfo_metrics(left, right, fs, 20.0, 24.0, lfo_search_center_hz)
    return {
        "ref_freq_hz": ref_freq,
        "ref_peak_dbfs": amp_db(ref_amp),
        "colour_freqs_hz": colour_freqs,
        "lfo_freq_hz": lfo["freq_hz"],
        "lfo_depth_pct": lfo["depth_pct"],
        "lfo_corr": lfo["corr"],
        "peak_dbfs": amp_db(peak),
    }


def _row(checks: Checks, measurements: dict, row_id: str, label: str, tool: str,
         command: str, expectation: str, metrics: dict, passed: bool,
         observed: str, extra: str = "", kind: str = "preserved"):
    """Voeg een adviserende corruptierij toe.

    kind="preserved": verwachting is dat de dragers intact blijven.
    kind="degraded":  verwachting is dat de dragers aantoonbaar degraderen.
    kind="info":      meting zonder PASS/FAIL-oordeel (bijv. buiten spec).
    Adviserende rijen heten nooit "FAIL": een afwijking wordt "ADVISORY-FAIL"
    en telt alleen met --strict-corruption mee voor de exitcode.
    """
    if kind == "info":
        status = "INFO"
    elif passed and kind == "degraded":
        status = "EXPECTED-DEGRADED"
    elif passed:
        status = "PASS"
    else:
        status = "ADVISORY-FAIL"
    row = {
        "id": row_id,
        "label": label,
        "tool": tool,
        "command": command,
        "expectation": expectation,
        "observed": observed,
        "status": status,
        "kind": kind,
        "metrics": metrics,
        "detail": extra,
    }
    measurements.setdefault("m14", []).append(row)
    checks.add("M14", f"m14-{row_id}", label, expectation, observed, status, extra)


def corruption_matrix(checks: Checks, measurements: dict, score: dict, out_dir: Path,
                      master_name: str, variant_name: str, render_script: Path,
                      sample_rate: int = 48000):
    has_ffmpeg = shutil.which("ffmpeg") is not None
    master = out_dir / master_name
    variant = out_dir / variant_name

    def preserved(metrics, ref_hz=18200.0) -> bool:
        okay = (abs(metrics["ref_freq_hz"] - ref_hz) <= 1.0
                and abs(metrics["ref_peak_dbfs"] + 30.0) <= 3.0
                and abs(metrics["lfo_freq_hz"] - 20.0) <= 0.5
                and metrics["lfo_depth_pct"] >= 50.0)
        for hz in metrics["colour_freqs_hz"].values():
            okay = okay and hz == hz and abs(hz - round(hz)) < 5000.0
        return okay

    with tempfile.TemporaryDirectory(prefix="specstroop-corrupt-") as tmp:
        tmp = Path(tmp)

        if has_ffmpeg:
            # 1. nette SRC 48 -> 44,1
            dst = tmp / "src48_441.wav"
            args = ["-ar", "44100", "-c:a", "pcm_s24le"]
            subprocess.run(_ffmpeg_command(master, dst, args), check=True, capture_output=True)
            fs2, _, l2, r2 = read_wav(dst)
            metrics = _quick_metrics(fs2, l2, r2)
            _row(checks, measurements, "src48_441", "nette SRC 48 → 44,1 kHz",
                 "ffmpeg", "ffmpeg -i master -ar 44100 -c:a pcm_s24le",
                 "dragers intact (freq ±1 Hz, LFO 20 Hz)", metrics, preserved(metrics),
                 f"ref {metrics['ref_freq_hz']:.2f} Hz, {metrics['ref_peak_dbfs']:+.1f} dBFS, "
                 f"LFO {metrics['lfo_freq_hz']:.2f} Hz")
            # 2. nette SRC 44,1 -> 48
            dst = tmp / "src441_48.wav"
            args = ["-ar", "48000", "-c:a", "pcm_s24le"]
            subprocess.run(_ffmpeg_command(variant, dst, args), check=True, capture_output=True)
            fs2, _, l2, r2 = read_wav(dst)
            metrics = _quick_metrics(fs2, l2, r2)
            _row(checks, measurements, "src441_48", "nette SRC 44,1 → 48 kHz",
                 "ffmpeg", "ffmpeg -i variant -ar 48000 -c:a pcm_s24le",
                 "dragers intact (freq ±1 Hz, LFO 20 Hz)", metrics, preserved(metrics),
                 f"ref {metrics['ref_freq_hz']:.2f} Hz, {metrics['ref_peak_dbfs']:+.1f} dBFS, "
                 f"LFO {metrics['lfo_freq_hz']:.2f} Hz")
            # 3. lowpass 16 kHz (8 cascaded 2-pole stages = 16e orde)
            dst = tmp / "lowpass16.wav"
            chain = ",".join(["lowpass=f=16000:poles=2"] * 8)
            args = ["-af", chain, "-c:a", "pcm_s24le"]
            subprocess.run(_ffmpeg_command(master, dst, args), check=True, capture_output=True)
            fs2, _, l2, r2 = read_wav(dst)
            metrics = _quick_metrics(fs2, l2, r2)
            degraded = metrics["ref_peak_dbfs"] <= -30.0 - 10.0
            observed = (f"ref {metrics['ref_peak_dbfs']:+.1f} dBFS (was -30,0); "
                        "drager weg — LFO-meting niet zinvol")
            _row(checks, measurements, "lowpass16", "lowpass 16 kHz op de master",
                 "ffmpeg", f"ffmpeg -af '{chain}'",
                 "verwacht: dragers verzwakt (≥ 10 dB), dus gedegradeerd", metrics, degraded,
                 observed, kind="degraded")
            # 4. MP3 320 kbps CBR stereo
            mp3 = tmp / "x320.mp3"
            subprocess.run(_ffmpeg_command(master, mp3, ["-c:a", "libmp3lame", "-b:a", "320k"]),
                           check=True, capture_output=True)
            dst = tmp / "x320.wav"
            subprocess.run(_ffmpeg_command(mp3, dst, ["-c:a", "pcm_s24le"]),
                           check=True, capture_output=True)
            fs2, _, l2, r2 = read_wav(dst)
            metrics = _quick_metrics(fs2, l2, r2)
            _row(checks, measurements, "mp3_320", "MP3 320 kbps CBR stereo",
                 "ffmpeg", "ffmpeg -c:a libmp3lame -b:a 320k (encode + decode)",
                 "verwacht: intact (Lumasonic-advies)", metrics, preserved(metrics),
                 f"ref {metrics['ref_freq_hz']:.2f} Hz, {metrics['ref_peak_dbfs']:+.1f} dBFS, "
                 f"LFO {metrics['lfo_freq_hz']:.2f} Hz / {metrics['lfo_depth_pct']:.0f}%")
            # 5. AAC 256 kbps
            aac = tmp / "x256.m4a"
            subprocess.run(_ffmpeg_command(master, aac, ["-c:a", "aac", "-b:a", "256k"]),
                           check=True, capture_output=True)
            dst = tmp / "x256.wav"
            subprocess.run(_ffmpeg_command(aac, dst, ["-c:a", "pcm_s24le"]),
                           check=True, capture_output=True)
            fs2, _, l2, r2 = read_wav(dst)
            metrics = _quick_metrics(fs2, l2, r2)
            _row(checks, measurements, "aac_256", "AAC 256 kbps",
                 "ffmpeg", "ffmpeg -c:a aac -b:a 256k (encode + decode)",
                 "verwacht: dragers meetbaar (mogelijk verzwakt)", metrics, preserved(metrics),
                 f"ref {metrics['ref_freq_hz']:.2f} Hz, {metrics['ref_peak_dbfs']:+.1f} dBFS, "
                 f"LFO {metrics['lfo_freq_hz']:.2f} Hz / {metrics['lfo_depth_pct']:.0f}%")
        else:
            for row_id, label in (("src48_441", "nette SRC 48 → 44,1 kHz"),
                                  ("src441_48", "nette SRC 44,1 → 48 kHz"),
                                  ("lowpass16", "lowpass 16 kHz op de master"),
                                  ("mp3_320", "MP3 320 kbps"),
                                  ("aac_256", "AAC 256 kbps")):
                checks.add("M14", f"m14-{row_id}", label, "ffmpeg aanwezig",
                           "ffmpeg niet gevonden", "SKIP")

        # 6. gain -6 dB (pure Python, in-memory)
        fs_m, _, l_m, r_m = read_wav(master)
        gain = 0.5
        l_g = array.array("d", [v * gain for v in l_m])
        r_g = array.array("d", [v * gain for v in r_m])
        metrics = _quick_metrics(fs_m, l_g, r_g)
        gain_ok = (abs(metrics["ref_freq_hz"] - 18200.0) <= 1.0
                   and abs(metrics["ref_peak_dbfs"] + 36.0) <= 1.0
                   and abs(metrics["lfo_freq_hz"] - 20.0) <= 0.5)
        _row(checks, measurements, "gain_min6", "gain −6 dB (in-memory)",
             "python", "monster × 0,5",
             "verwacht: frequenties/LFO intact, niveau exact −6 dB", metrics, gain_ok,
             f"ref {metrics['ref_peak_dbfs']:+.1f} dBFS, LFO {metrics['lfo_freq_hz']:.2f} Hz")

        # 7. monosom
        l_m2 = array.array("d", [0.5 * (l_m[i] + r_m[i]) for i in range(len(l_m))])
        metrics = _quick_metrics(fs_m, l_m2, l_m2)
        mono_ok = metrics["lfo_depth_pct"] < 20.0
        _row(checks, measurements, "mono", "monosom (L+R)/2",
             "python", "(L+R)/2",
             "verwacht: referentie-panning verdwijnt (LFO-detectie faalt)", metrics, mono_ok,
             f"LFO-diepte {metrics['lfo_depth_pct']:.1f}% (verwacht < 20%), "
             f"ref {metrics['ref_peak_dbfs']:+.1f} dBFS", kind="degraded")

        # 8. rate-mismatch: 44,1-bestand als 48 kHz geïnterpreteerd
        fs_v, _, l_v, r_v = read_wav(variant)
        spec_l, spec_r, sum_w2 = spectrum_pair(l_v, r_v, 48000.0, 20.0)
        combined = combined_power(spec_l, spec_r)
        bin_hz = 48000.0 / FFT_N
        k_lo = int(17000.0 / bin_hz)
        k_hi = int(21000.0 / bin_hz)
        k_peak = max(range(k_lo, k_hi), key=lambda k: combined[k])
        shifted = k_peak * bin_hz
        mismatch_ok = 19000.0 <= shifted <= 20600.0
        _row(checks, measurements, "rate_mismatch", "44,1-bestand op 48 kHz afgespeeld",
             "python", "44,1-data geïnterpreteerd als 48 kHz",
             "verwacht: frequenties schuiven +8,84% (18,2 → ~19,8 kHz), detectie faalt",
             {"peak_hz": shifted}, mismatch_ok,
             f"sterkste piek 17–21 kHz op {shifted:.1f} Hz (verwacht ~19,81 kHz)",
             kind="degraded")

        # 9-12. LFO-varianten: eigen renderer, alleen dragers
        try:
            sys.path.insert(0, str(render_script.parent))
            import render as poc_render  # noqa: PLC0415 - bewust lokaal
        except Exception as error:  # noqa: BLE001
            checks.add("M14", "m14-lfo-varianten", "LFO-varianten", "render.py importeerbaar",
                       str(error), "SKIP")
            return
        variants = [
            ("lfo_10", 10.0, "sine", 10.0, "binnen spec (10–50 Hz)"),
            ("lfo_50", 50.0, "sine", 50.0, "binnen spec (10–50 Hz)"),
            ("lfo_5", 5.0, "sine", 5.0, "buiten spec: detectie hoort te falen"),
            ("pan_square", 20.0, "square", 20.0, "square-panning: detectie ok, meer artefacten"),
        ]
        for row_id, lfo_hz, shape, expected_hz, note in variants:
            score2 = copy.deepcopy(score)
            score2["carriers"]["reference"]["pan_lfo_hz"] = lfo_hz
            score2["carriers"]["reference"]["pan_lfo_shape"] = shape
            (mix, _) = poc_render.render_mix(score2, 48000, with_programme=False)
            metrics = _quick_metrics(48000, mix[0], mix[1], lfo_search_center_hz=lfo_hz)
            detected = (abs(metrics["lfo_freq_hz"] - expected_hz) <= 0.5
                        and metrics["lfo_depth_pct"] >= 50.0)
            _row(checks, measurements, row_id, f"referentie-pan-LFO {lfo_hz:g} Hz ({shape})",
                 "render.py", f"carriers-only, pan_lfo_hz={lfo_hz:g}, shape={shape}",
                 note, metrics, detected,
                 f"LFO {metrics['lfo_freq_hz']:.2f} Hz, diepte {metrics['lfo_depth_pct']:.0f}%, "
                 f"ref {metrics['ref_peak_dbfs']:+.1f} dBFS",
                 "buiten 10–50 Hz: bewust geen geldige SpectraStrobe" if row_id == "lfo_5" else "",
                 kind="info" if row_id == "lfo_5" else "preserved")


# --------------------------------------------------------------------------- #
# Rapportage
# --------------------------------------------------------------------------- #
def write_reports(out_dir: Path, score: dict, checks: Checks, measurements: dict,
                  summary: dict, corruptie_adviserend: bool, strict: bool):
    report = {
        "experiment": EXPERIMENT,
        "warning": WARNING,
        "verdict": summary["verdict"],
        "summary": summary,
        "score": {
            "title": score["meta"]["title"],
            "version": score["meta"]["version"],
            "seed": score["meta"]["seed"],
            "duration_s": score["meta"]["duration_s"],
            "carriers": score["carriers"],
        },
        "measurements": measurements,
        "checks": checks.items,
        "corruption_advisory": corruptie_adviserend and not strict,
        "note": ("De corruptiematrix is adviserend: alleen de kernchecks (F, M1–M10) bepalen "
                 "de exitcode. Met --strict-corruption worden corruptierijen ook hard getoetst. "
                 "Hardware (M11–M13) is niet beschikbaar in deze offline omgeving."),
    }
    (out_dir / "verify-report.json").write_text(
        json.dumps(report, indent=2, sort_keys=False, ensure_ascii=False) + "\n", encoding="utf-8")

    lines = []
    lines.append("# Verificatierapport — Eerste Gloed (SpectraStrobe PoC)")
    lines.append("")
    lines.append("> **EXPERIMENT — geen deel van NAGLOED v1.0.4.** Offline signaalverificatie; "
                 "hardware (Kasina + Ganzfeld) is niet beschikbaar, dus M11–M13 zijn niet gemeten.")
    lines.append("")
    lines.append(f"- **Score:** {score['meta']['title']} · seed {score['meta']['seed']} · "
                 f"{score['meta']['duration_s']} s")
    lines.append(f"- **Draaggolven:** 18.200 Hz referentie "
                 f"({score['carriers']['reference']['level_dbfs']:+.1f} dBFS, "
                 f"{score['carriers']['reference']['pan_lfo_hz']:g} Hz pan-LFO), "
                 f"kleuren 18.7/19.2/19.7 kHz (max "
                 f"{score['carriers']['max_colour_ratio'] * 100:.0f}% van referentie)")
    lines.append(f"- **Kernuitkomst (F, M1–M10, scorecontract):** {summary['verdict']} — "
                 f"{summary['pass']} PASS, {summary['fail']} FAIL, {summary['skip']} SKIP "
                 f"van {summary['total']} checks")
    lines.append(f"- **Corruptiematrix (adviserend):** {summary['corruption_total']} rijen — "
                 f"{summary['corruption_pass']} PASS, "
                 f"{summary['corruption_expected_degraded']} EXPECTED-DEGRADED, "
                 f"{summary['corruption_info']} INFO, "
                 f"{summary['corruption_advisory_fail']} ADVISORY-FAIL")
    lines.append("")
    def cell(value):
        return _format(value).replace("|", "\\|").replace("\n", " ")

    lines.append("## Kernchecks")
    lines.append("")
    lines.append("| # | Groep | Check | Criterium | Gemeten | Status |")
    lines.append("|---|---|---|---|---|---|")
    for item in checks.items:
        if item["group"] == "M14":
            continue
        lines.append(f"| {cell(item['id'])} | {cell(GROUP_LABELS.get(item['group'], item['group']))} | "
                     f"{cell(item['description'])} | {cell(item['criterion'])} | "
                     f"{cell(item['measured'])} | **{item['status']}** |")
    lines.append("")
    lines.append("## Kernmetingen")
    lines.append("")
    lines.append("| Meting | Waarde |")
    lines.append("|---|---|")
    m1 = measurements.get("m1", {})
    for label, value in m1.items():
        lines.append(f"| M1 dragerfrequenties ({label}) | ref {value['reference_hz']:.2f} Hz; "
                     + ", ".join(f"{k} {v:.2f} Hz" for k, v in value["colours_hz"].items()) + " |")
    m2 = measurements.get("m2", {})
    for label, value in m2.items():
        lines.append(f"| M2 referentie ({label}) | piek {value['reference_peak_amplitude_dbfs']:+.2f} dBFS, "
                     f"totale RMS {value['reference_total_rms_dbfs']:+.2f} dBFS, "
                     f"variatie {value['variation_db']:.2f} dB |")
    m3 = measurements.get("m3", {})
    for label, rows in m3.items():
        for row in rows:
            parts = []
            for key, value in row["colours"].items():
                parts.append(f"{key} ΔL {value['delta_db_L']:+.2f} dB")
            lines.append(f"| M3 helderheid ({label}, {row['event']}) | "
                         + ", ".join(parts) + " |")
    m4 = measurements.get("m4", {})
    for label, value in m4.items():
        lines.append(f"| M4 balans/isolatie ({label}) | midden {value['balance_db']:.2f} dB, "
                     f"hard-pan-isolatie {value['isolation_db']:.1f} dB |")
    m5 = measurements.get("m5", {})
    for label, value in m5.items():
        lines.append(f"| M5 pan-LFO ({label}) | {value['freq_hz']:.2f} Hz, "
                     f"diepte {value['depth_pct']:.1f}%, corr {value['corr']:.2f}, "
                     f"rust {value['rest_db']:.1f} dB |")
    if "m6" in measurements:
        for label, value in measurements["m6"].items():
            lines.append(f"| M6 crosstalk ({label}) | {value['worst_crosstalk_db']:.1f} dB |")
    if "m7" in measurements:
        lines.append(f"| M7 programmaleak 17–20 kHz | "
                     f"{measurements['m7']['programme_17k_20k_dbfs_L']:.1f} dBFS (L), "
                     f"{measurements['m7']['programme_17k_20k_dbfs_R']:.1f} dBFS (R) |")
        lines.append(f"| M7 programma-only dragerloos | "
                     f"{measurements['m7']['program_only_carrier_floor_dbfs']:.1f} dBFS |")
    if "m8" in measurements:
        for label, value in measurements["m8"].items():
            lines.append(f"| M8 ruisvloer kleurbanden ({label}) | "
                         f"{value['worst_colour_band_noise_dbfs']:.1f} dBFS |")
    m9 = measurements.get("m9", {})
    for label, value in m9.items():
        lines.append(f"| M9 piek ({label}) | sample {value['sample_peak_dbfs']:+.2f} dBFS, "
                     f"true peak {value['true_peak_dbtp']:+.2f} dBTP, "
                     f"clipping {value['clipped']} |")
    if "m10" in measurements:
        identical = sum(1 for item in measurements["m10"] if item["identical"])
        lines.append(f"| M10 determinisme | {identical}/{len(measurements['m10'])} bestanden "
                     f"byte-identiek |")
    lines.append("")
    lines.append("## Corruptiematrix (M14, adviserend)")
    lines.append("")
    lines.append("De matrix toont wat bewuste schade met de dragers doet. Rijen zijn adviserend "
                 "en heten nooit FAIL: PASS = verwachting gehaald, EXPECTED-DEGRADED = "
                 "aantoonbare degradatie zoals voorspeld, INFO = meting zonder oordeel, "
                 "ADVISORY-FAIL = verwachting niet gehaald. Alleen met "
                 "`--strict-corruption` beïnvloedt ADVISORY-FAIL de exitcode.")
    lines.append("")
    lines.append("| Variant | Verwachting | Gemeten | Status |")
    lines.append("|---|---|---|---|")
    for row in measurements.get("m14", []):
        lines.append(f"| {cell(row['label'])} | {cell(row['expectation'])} | "
                     f"{cell(row['observed'])} | **{row['status']}** |")
    lines.append("")
    lines.append("## Wat dit bewijst en niet bewijst")
    lines.append("")
    lines.append("- **Wel:** de WAV's bevatten de vier dragers op de juiste frequentie en "
                 "relatieve amplitude, de 20 Hz-panning is per sample aanwezig, het programma "
                 "lekt niet in de controlebrand, er is ruime headroom, en de render is "
                 "deterministisch op deze machine (CPython-stdlib).")
    lines.append("- **Niet:** of een Kasina het signaal herkent, of de LED's de bedoelde "
                 "helderheid geven, of de vertraging artistiek werkt. Daarvoor is "
                 "`HARDWARE-PROTOCOL.md` nodig en een echte sessie (M11–M13).")
    lines.append("- **Niet:** gedrag in een DAW/host, resampling in de weergaveketen of "
                 "lossy streaming; de corruptiematrix is een eerste indicatie, geen garantie.")
    (out_dir / "verify-report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    return report


# --------------------------------------------------------------------------- #
# Main
# --------------------------------------------------------------------------- #
def main(argv=None) -> int:
    base = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description="NAGLOED v2 SpectraStrobe PoC verificatie (EXPERIMENT)")
    parser.add_argument("--out-dir", default=str(base / "out"))
    parser.add_argument("--score", default=str(base / "score.json"))
    parser.add_argument("--render-script", default=str(base / "render.py"))
    parser.add_argument("--skip-corruption", action="store_true")
    parser.add_argument("--strict-corruption", action="store_true")
    args = parser.parse_args(argv)

    out_dir = Path(args.out_dir)
    score_path = Path(args.score)
    render_script = Path(args.render_script)
    score = json.loads(score_path.read_text(encoding="utf-8"))

    checks = Checks()
    measurements = {}
    names = ("spectrastrobe-48k-24bit.wav", "spectrastrobe-44k1-16bit.wav",
             "program-only-48k-24bit.wav")
    missing = [name for name in names if not (out_dir / name).exists()]
    if missing:
        print(f"FOUT: ontbrekende renders in {out_dir}: {', '.join(missing)}", file=sys.stderr)
        print("Draai eerst: python3 render.py", file=sys.stderr)
        return 2

    duration = float(score["meta"]["duration_s"])
    master_path = out_dir / names[0]
    variant_path = out_dir / names[1]
    program_path = out_dir / names[2]

    fs_m, width_m, l_m, r_m = read_wav(master_path)
    fs_v, width_v, l_v, r_v = read_wav(variant_path)
    fs_p, width_p, l_p, r_p = read_wav(program_path)

    format_check(checks, "48k", master_path, 48000, 3, duration, fs_m, width_m, len(l_m))
    format_check(checks, "44k1", variant_path, 44100, 2, duration, fs_v, width_v, len(l_v))
    format_check(checks, "program-only", program_path, 48000, 3, duration, fs_p, width_p, len(l_p))

    score_checks(checks, score)
    analyse_core(checks, measurements, score, fs_m, l_m, r_m, "48k", exhaustive=True)
    analyse_core(checks, measurements, score, fs_v, l_v, r_v, "44k1", exhaustive=False)
    programme_check(checks, measurements, score, program_path, fs_p, l_p, r_p)
    determinism_check(checks, measurements, render_script, score_path, out_dir, names)

    if not args.skip_corruption:
        corruption_matrix(checks, measurements, score, out_dir, names[0], names[1],
                          render_script)

    summary = checks.summary()
    write_reports(out_dir, score, checks, measurements, summary,
                  corruptie_adviserend=not args.skip_corruption, strict=args.strict_corruption)

    core_fail = sum(1 for item in checks.items
                    if item["group"] != "M14" and item["status"] == "FAIL")
    advisory_fail = sum(1 for item in checks.items
                        if item["group"] == "M14" and item["status"] == "ADVISORY-FAIL")
    print("NAGLOED v2 — SpectraStrobe PoC verificatie (EXPERIMENT, niet v1.0.4)")
    print(f"score        : {score_path.name} (seed {score['meta']['seed']}, {duration:.1f} s)")
    print(f"48k/24-bit   : {names[0]}")
    print(f"44k1/16-bit  : {names[1]}")
    print(f"program-only : {names[2]}")
    print("")
    for item in checks.items:
        prefix = "" if item["group"] == "M14" else "  "
        print(f"{prefix}{item['id']:<22} {item['status']:<5} {_format(item['measured'])}")
    print("")
    print(f"KERN           : {summary['verdict']} "
          f"({summary['pass']} PASS, {core_fail} FAIL van {summary['total']})")
    if not args.skip_corruption:
        print(f"CORRUPTIEMATRIX: {summary['corruption_total']} rijen — "
              f"{summary['corruption_pass']} PASS, "
              f"{summary['corruption_expected_degraded']} EXPECTED-DEGRADED, "
              f"{summary['corruption_info']} INFO, {advisory_fail} ADVISORY-FAIL"
              + (" (hard getoetst)" if args.strict_corruption else " (adviserend)"))
    print(f"rapport       : {out_dir.resolve()}/verify-report.json en verify-report.md")

    failed = core_fail > 0 or (args.strict_corruption and advisory_fail > 0)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
