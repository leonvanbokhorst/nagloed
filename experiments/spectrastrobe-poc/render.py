#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""NAGLOED v2 — SpectraStrobe proof-of-concept: deterministische renderer.

EXPERIMENT. Geen deel van NAGLOED v1.0.4, geen integratie, geen DSP-contract.
Offline en alleen Python-stdlib.

Leest score.json en schrijft (standaard naar out/):
  spectrastrobe-48k-24bit.wav   volledige mix (programma + dragers), 48 kHz/24-bit
  spectrastrobe-44k1-16bit.wav  idem, native gerenderd op 44,1 kHz/16-bit
  program-only-48k-24bit.wav    A/B: alleen programma, zonder dragers
  events.csv                    declaratieve gebeurtenissen met relaties
  timeline.svg                  tijdelijk (licht/audio/draaggolf)
  render-manifest.json          sha256 per output (determinisme)

Volgorde is bindend (docs/v2/SPECTRASTROBE.md §6): programma eerst volledig
verwerken en bandbegrenzen, daarna dragers optellen, daarna niets meer.
"""

from __future__ import annotations

import argparse
import array
import csv
import hashlib
import json
import math
import random
import sys
import wave
from pathlib import Path

TAU = 2.0 * math.pi
EXPERIMENT_TITLE = "Eerste Gloed — SpectraStrobe proof-of-concept (EXPERIMENT)"

MASTER_NAME = "spectrastrobe-48k-24bit.wav"
VARIANT_NAME = "spectrastrobe-44k1-16bit.wav"
PROGRAM_NAME = "program-only-48k-24bit.wav"

LIGHT_KEYS = ("red", "green", "blue")


# --------------------------------------------------------------------------- #
# Basisberekeningen
# --------------------------------------------------------------------------- #
def db_to_lin(db: float) -> float:
    return 10.0 ** (db / 20.0)


def lin_to_db(x: float) -> float:
    if x <= 0.0:
        return float("-inf")
    return 20.0 * math.log10(x)


def stable_seed(seed: int, tag: str) -> int:
    digest = hashlib.sha256(f"{seed}:{tag}".encode("utf-8")).digest()
    return int.from_bytes(digest[:8], "big")


def smoothstep(u: float) -> float:
    u = 0.0 if u < 0.0 else (1.0 if u > 1.0 else u)
    return u * u * (3.0 - 2.0 * u)


def segments_array(points, count: int, fs: float) -> array.array:
    """Breakpoint-envelope met smoothstep-interpolatie; klikvrij.

    points = [[t_seconden, waarde], ...] met t relatief t.o.v. het event.
    Waarden voor het eerste punt worden op de eerste waarde gehouden, na het
    laatste punt op de laatste waarde. De score laat envelopes op 0 eindigen.
    """
    pts = [(float(t), float(v)) for t, v in points]
    if not pts:
        raise ValueError("leeg breakpoint-pad")
    arr = array.array("d", [pts[0][1]]) * count
    for (t0, v0), (t1, v1) in zip(pts, pts[1:]):
        i0 = int(round(t0 * fs))
        i1 = int(round(t1 * fs))
        i0 = 0 if i0 < 0 else (count if i0 > count else i0)
        i1 = 0 if i1 < 0 else (count if i1 > count else i1)
        span = i1 - i0
        if span <= 0:
            continue
        dv = v1 - v0
        for i in range(i0, i1):
            s = smoothstep((i - i0) / span)
            arr[i] = v0 + dv * s
    last = int(round(pts[-1][0] * fs))
    for i in range(max(0, last), count):
        arr[i] = pts[-1][1]
    return arr


def fade_array(count: int, fs: float, fade_in_ms: float, fade_out_ms: float) -> array.array:
    """Klikvrije raised-cosine in-/uitfade voor de dragers op de trackranden."""
    fade = array.array("d", [1.0]) * count
    k_in = max(0, min(count // 2, int(round(fade_in_ms * fs / 1000.0))))
    for i in range(k_in):
        w = 0.5 - 0.5 * math.cos(math.pi * i / k_in) if k_in else 1.0
        fade[i] = w
    k_out = max(0, min(count // 2, int(round(fade_out_ms * fs / 1000.0))))
    for i in range(k_out):
        w = 0.5 - 0.5 * math.cos(math.pi * i / k_out) if k_out else 1.0
        fade[count - 1 - i] = w
    return fade


# --------------------------------------------------------------------------- #
# Panning
# --------------------------------------------------------------------------- #
def pan_equal_power(p: float):
    """Gelijkvermogen-panning (referentie): totale dragerenergie blijft constant."""
    p = -1.0 if p < -1.0 else (1.0 if p > 1.0 else p)
    theta = 0.25 * math.pi + 0.25 * math.pi * p
    return math.cos(theta), math.sin(theta)


def pan_linear_0db(p: float):
    """Lineaire panning met 0 dB in het midden (kleuren).

    Bewust géén -3 dB center-pan law: die zou de gedecodeerde helderheid per oor
    halveren. p=-1: alleen links; p=0: beide oren vol; p=+1: alleen rechts.
    """
    p = -1.0 if p < -1.0 else (1.0 if p > 1.0 else p)
    return (1.0 - max(0.0, p), 1.0 + min(0.0, p))


# --------------------------------------------------------------------------- #
# Biquads (RBJ) voor programma-bandbegrenzing en ruisvorming
# --------------------------------------------------------------------------- #
def biquad_coeffs(fs: float, kind: str, f0: float, q: float):
    w0 = TAU * f0 / fs
    cw = math.cos(w0)
    sw = math.sin(w0)
    alpha = sw / (2.0 * q)
    if kind == "lowpass":
        b0 = (1.0 - cw) * 0.5
        b1 = 1.0 - cw
        b2 = b0
    elif kind == "highpass":
        b0 = (1.0 + cw) * 0.5
        b1 = -(1.0 + cw)
        b2 = b0
    elif kind == "bandpass":
        b0 = alpha
        b1 = 0.0
        b2 = -alpha
    else:
        raise ValueError(f"onbekend filtertype: {kind}")
    a0 = 1.0 + alpha
    a1 = -2.0 * cw
    a2 = 1.0 - alpha
    return (b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0)


def apply_biquad(buf: array.array, coeffs) -> None:
    b0, b1, b2, a1, a2 = coeffs
    z1 = 0.0
    z2 = 0.0
    for i in range(len(buf)):
        x = buf[i]
        y = b0 * x + z1
        z1 = b1 * x - a1 * y + z2
        z2 = b2 * x - a2 * y
        buf[i] = y


def programme_lowpass(buf: array.array, fs: float, cutoff_hz: float, order: int) -> array.array:
    """Butterworth-cascade (standaard: 8e orde, 48 dB/oct) op het programma."""
    if cutoff_hz <= 0 or cutoff_hz >= fs * 0.5:
        return buf
    pairs = max(1, order // 2)
    for k in range(1, pairs + 1):
        q = 1.0 / (2.0 * math.sin((2 * k - 1) * math.pi / (2.0 * order)))
        apply_biquad(buf, biquad_coeffs(fs, "lowpass", cutoff_hz, q))
    return buf


# --------------------------------------------------------------------------- #
# Programma-synthese (schaars, gesynthetiseerd "gevonden" materiaal)
# --------------------------------------------------------------------------- #
def synth_impact(ev: dict, fs: float, seed: int) -> array.array:
    """Lage impact: sinus-sweep met harmonischen + resonant gefilterde ruis."""
    count = int(round(float(ev["duration_s"]) * fs))
    rng = random.Random(stable_seed(seed, ev["id"]))
    env = segments_array(ev["env"], count, fs)
    sweep = ev["sweep"]
    f0 = float(sweep["f_start_hz"])
    f1 = float(sweep["f_end_hz"])
    sweep_s = float(sweep["sweep_s"])
    harmonics = [float(g) for g in sweep["harmonics"]]
    sweep_gain = float(sweep["gain"])
    ratio = f1 / f0
    out = array.array("d", [0.0]) * count
    phase = 0.0
    for j in range(count):
        t = j / fs
        f = f0 * (ratio ** (t / sweep_s)) if t < sweep_s else f1
        phase += TAU * f / fs
        value = harmonics[0] * math.sin(phase)
        for h in range(1, len(harmonics)):
            value += harmonics[h] * math.sin(phase * (h + 1))
        out[j] = sweep_gain * value
    noise_spec = ev.get("noise") or {}
    filters = noise_spec.get("filters") or []
    if filters:
        noise = array.array("d", [rng.gauss(0.0, 1.0) for _ in range(count)])
        for filt in filters:
            apply_biquad(noise, biquad_coeffs(fs, filt["type"], float(filt["hz"]), float(filt["q"])))
        gain = float(noise_spec.get("gain", 0.0))
        for j in range(count):
            out[j] += gain * noise[j]
    peak = max(max(out), -min(out)) or 1.0
    inv = 1.0 / peak
    for j in range(count):
        out[j] = out[j] * inv * env[j]
    return out


def transform_buffer(ev: dict, source: array.array, fs: float) -> array.array:
    """Herinneringstransformatie: omkeren, verlagen (pitch_ratio < 1) en dimmen.

    De bron is al bandbegrensd en laagfrequent; lineaire interpolatie bij het
    vertragen is daardoor veilig (geen aliasing in de hoorbare band).
    """
    tr = ev.get("transform") or {}
    buf = list(source)
    if tr.get("reverse"):
        buf.reverse()
    ratio = float(tr.get("pitch_ratio", 1.0))
    if abs(ratio - 1.0) > 1e-12:
        out_len = max(1, int(round(len(buf) / ratio)))
        out = array.array("d", [0.0]) * out_len
        for i in range(out_len):
            pos = i * ratio
            k = int(pos)
            frac = pos - k
            v0 = buf[k] if k < len(buf) else 0.0
            v1 = buf[k + 1] if (k + 1) < len(buf) else 0.0
            out[i] = v0 + (v1 - v0) * frac
        buf = out
    return buf


def scale_to_peak(buf: array.array, target: float) -> array.array:
    peak = max(max(buf), -min(buf)) or 1.0
    gain = target / peak
    for i in range(len(buf)):
        buf[i] *= gain
    return buf


def render_program(score: dict, fs: float, count: int):
    """Rendert het programma op een eigen bus. Geen dragers, geen processing erna."""
    left = array.array("d", [0.0]) * count
    right = array.array("d", [0.0]) * count
    cache = {}
    seed = int(score["meta"]["seed"])
    config = score.get("program") or {}
    cutoff = float(config.get("band_limit_hz", 15500.0))
    order = int(config.get("lowpass_order", 8))
    for ev in config.get("events", []):
        kind = ev["type"]
        if kind == "impact":
            mono = synth_impact(ev, fs, seed)
        elif kind == "transform":
            src_id = ev["source"]
            if src_id not in cache:
                raise ValueError(f"transform-bron {src_id!r} is nog niet gerenderd")
            mono = transform_buffer(ev, cache[src_id], fs)
        else:
            raise ValueError(f"onbekend programmatype: {kind}")
        mono = programme_lowpass(mono, fs, cutoff, order)
        mono = scale_to_peak(mono, db_to_lin(float(ev["peak_dbfs"])))
        cache[ev["id"]] = mono
        i0 = int(round(float(ev["start_s"]) * fs))
        gl, gr = pan_equal_power(float(ev.get("pan", 0.0)))
        for j in range(min(len(mono), count - i0)):
            left[i0 + j] += mono[j] * gl
            right[i0 + j] += mono[j] * gr
    return left, right


# --------------------------------------------------------------------------- #
# SpectraStrobe-dragers
# --------------------------------------------------------------------------- #
def render_carriers(score: dict, fs: float, count: int):
    """Rendert referentie (continu, 20 Hz pan-LFO) + kleurkanalen per lichtgrain."""
    car = score["carriers"]
    ref = car["reference"]
    amp_ref = db_to_lin(float(ref["level_dbfs"]))
    fade = fade_array(count, fs, float(ref["fade_in_ms"]), float(ref["fade_out_ms"]))
    shape = ref.get("pan_lfo_shape", "sine")
    left = array.array("d", [0.0]) * count
    right = array.array("d", [0.0]) * count

    # Referentie: sinusdrager, per sample gepande met een 20 Hz sinus-LFO.
    d_ref = TAU * float(ref["hz"]) / fs
    d_lfo = TAU * float(ref["pan_lfo_hz"]) / fs
    depth = float(ref.get("pan_lfo_depth", 1.0))
    sin = math.sin
    cos = math.cos
    phase_ref = 0.0
    phase_lfo = 0.0
    quarter_pi = 0.25 * math.pi
    for i in range(count):
        p = sin(phase_lfo) * depth
        if shape == "square":
            p = 1.0 if p >= 0.0 else -1.0
        elif shape == "pulse":
            p = 1.0 if p >= 0.0 else -1.0
        theta = quarter_pi + quarter_pi * p
        sample = amp_ref * sin(phase_ref) * fade[i]
        left[i] = sample * cos(theta)
        right[i] = sample * sin(theta)
        phase_ref += d_ref
        phase_lfo += d_lfo

    # Kleurkanalen: eigen envelope en lineaire 0-dB-panning per lichtgrain.
    for ev in score["light"]["events"]:
        i0 = int(round(float(ev["start_s"]) * fs))
        dur = float(ev["end_s"]) - float(ev["start_s"])
        nev = int(round(dur * fs))
        if i0 < 0 or i0 >= count or nev <= 0:
            continue
        nev = min(nev, count - i0)
        env = segments_array(ev["env"], nev, fs)
        pan = segments_array(ev["pan"], nev, fs)
        gain_l = array.array("d", [0.0]) * nev
        gain_r = array.array("d", [0.0]) * nev
        for j in range(nev):
            p = pan[j]
            gain_l[j] = 1.0 - max(0.0, p)
            gain_r[j] = 1.0 + min(0.0, p)
        rgb = ev["rgb"]
        for idx, key in enumerate(LIGHT_KEYS):
            brightness = float(rgb[idx])
            if brightness <= 0.0:
                continue
            spec = car["colours"][key]
            amp = amp_ref * brightness
            d_col = TAU * float(spec["hz"]) / fs
            phase = float(spec.get("phase_rad", 0.0))
            for j in range(nev):
                s = amp * env[j] * sin(phase) * fade[i0 + j]
                left[i0 + j] += s * gain_l[j]
                right[i0 + j] += s * gain_r[j]
                phase += d_col
    return left, right


def render_mix(score: dict, fs: float, with_programme: bool = True):
    """Retourneert ((mix_l, mix_r), (prog_l, prog_r)) voor de gevraagde rate."""
    count = int(round(float(score["meta"]["duration_s"]) * fs))
    if with_programme:
        prog_l, prog_r = render_program(score, fs, count)
    else:
        prog_l = array.array("d", [0.0]) * count
        prog_r = array.array("d", [0.0]) * count
    car_l, car_r = render_carriers(score, fs, count)
    mix_l = array.array("d", [0.0]) * count
    mix_r = array.array("d", [0.0]) * count
    for i in range(count):
        mix_l[i] = prog_l[i] + car_l[i]
        mix_r[i] = prog_r[i] + car_r[i]
    return (mix_l, mix_r), (prog_l, prog_r)


# --------------------------------------------------------------------------- #
# WAV-export (PCM, stereo, geen metadata, geen dither)
# --------------------------------------------------------------------------- #
def write_pcm(path, fs: int, bits: int, left, right) -> float:
    """Schrijft 16/24-bit PCM WAV en geeft de samplepiek in dBFS terug."""
    count = len(left)
    peak = max(max(left), -min(left), max(right), -min(right))
    if bits == 24:
        scale = 8388607.0
        payload = bytearray(count * 6)
        j = 0
        for i in range(count):
            v = int(round(left[i] * scale))
            v = 8388607 if v > 8388607 else (-8388608 if v < -8388608 else v)
            payload[j] = v & 0xFF
            payload[j + 1] = (v >> 8) & 0xFF
            payload[j + 2] = (v >> 16) & 0xFF
            v = int(round(right[i] * scale))
            v = 8388607 if v > 8388607 else (-8388608 if v < -8388608 else v)
            payload[j + 3] = v & 0xFF
            payload[j + 4] = (v >> 8) & 0xFF
            payload[j + 5] = (v >> 16) & 0xFF
            j += 6
    elif bits == 16:
        payload = bytearray(count * 4)
        j = 0
        for i in range(count):
            v = int(round(left[i] * 32767.0))
            v = 32767 if v > 32767 else (-32768 if v < -32768 else v)
            payload[j] = v & 0xFF
            payload[j + 1] = (v >> 8) & 0xFF
            v = int(round(right[i] * 32767.0))
            v = 32767 if v > 32767 else (-32768 if v < -32768 else v)
            payload[j + 2] = v & 0xFF
            payload[j + 3] = (v >> 8) & 0xFF
            j += 4
    else:
        raise ValueError("alleen 16- of 24-bit PCM")
    with wave.open(str(path), "wb") as handle:
        handle.setnchannels(2)
        handle.setsampwidth(bits // 8)
        handle.setframerate(fs)
        handle.writeframes(bytes(payload))
    return lin_to_db(peak)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


# --------------------------------------------------------------------------- #
# events.csv en timeline.svg
# --------------------------------------------------------------------------- #
def _transform_duration(score: dict, ev: dict) -> float:
    source = next(e for e in score["program"]["events"] if e["id"] == ev["source"])
    ratio = float((ev.get("transform") or {}).get("pitch_ratio", 1.0))
    return float(source["duration_s"]) / ratio


def build_events_rows(score: dict):
    rows = []
    for ev in score["light"]["events"]:
        rgb = ev["rgb"]
        brightest = max(rgb)
        peak_dbfs = lin_to_db(brightest * db_to_lin(float(score["carriers"]["reference"]["level_dbfs"])))
        pans = [float(p) for _, p in ev["pan"]]
        rows.append({
            "id": ev["id"],
            "domein": "licht",
            "rol": ev["role"],
            "start_s": f"{float(ev['start_s']):.3f}",
            "einde_s": f"{float(ev['end_s']):.3f}",
            "duur_s": f"{float(ev['end_s']) - float(ev['start_s']):.3f}",
            "piek_dbfs": f"{peak_dbfs:.1f}",
            "rgb": "/".join(f"{c:.5f}" for c in rgb),
            "pan_van": f"{pans[0]:.2f}",
            "pan_naar": f"{pans[-1]:.2f}",
            "relatie": ev.get("relation", ""),
        })
    for ev in score["program"]["events"]:
        dur = _transform_duration(score, ev) if ev["type"] == "transform" else float(ev["duration_s"])
        start = float(ev["start_s"])
        rows.append({
            "id": ev["id"],
            "domein": "audio",
            "rol": ev["role"],
            "start_s": f"{start:.3f}",
            "einde_s": f"{start + dur:.3f}",
            "duur_s": f"{dur:.3f}",
            "piek_dbfs": f"{float(ev['peak_dbfs']):.1f}",
            "rgb": "",
            "pan_van": f"{float(ev.get('pan', 0.0)):.2f}",
            "pan_naar": f"{float(ev.get('pan', 0.0)):.2f}",
            "relatie": ev.get("relation", ""),
        })
    rows.sort(key=lambda r: (float(r["start_s"]), 0 if r["domein"] == "licht" else 1))
    return rows


def write_events_csv(path: Path, score: dict) -> None:
    fields = ["id", "domein", "rol", "start_s", "einde_s", "duur_s",
              "piek_dbfs", "rgb", "pan_van", "pan_naar", "relatie"]
    with open(path, "w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for row in build_events_rows(score):
            writer.writerow(row)


def _hex_colour(rgb) -> str:
    scale = 1.0 / 0.25
    parts = []
    for c in rgb:
        v = max(0, min(255, int(round(255.0 * min(1.0, c * scale)))))
        parts.append(f"{v:02x}")
    return "#" + "".join(parts)


def write_timeline_svg(path: Path, score: dict) -> None:
    duration = float(score["meta"]["duration_s"])
    width, height = 1280, 500
    left, right = 150, 1230
    lane_light, lane_audio, lane_ref = 120, 235, 350
    bar_h = 52

    def x(t: float) -> float:
        return left + (right - left) * max(0.0, min(1.0, t / duration))

    sections = [(0.0, 8.0, "kamer"), (8.0, 14.0, "eerste gloed"),
                (14.0, 28.0, "naleven"), (28.0, 46.0, "terughoudendheid"),
                (46.0, 52.0, "herinnering"), (52.0, duration, "uitdoving")]
    out = []
    out.append('<?xml version="1.0" encoding="UTF-8"?>')
    out.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
               f'viewBox="0 0 {width} {height}" font-family="Helvetica, Arial, sans-serif">')
    out.append(f'<rect width="{width}" height="{height}" fill="#f7f5f0"/>')
    out.append(f'<text x="{left}" y="34" font-size="19" font-weight="bold" fill="#1c1a17">'
               f'{EXPERIMENT_TITLE}</text>')
    out.append(f'<text x="{left}" y="56" font-size="12" fill="#5a544c">'
               f'seed {score["meta"]["seed"]} · {duration:.1f} s · '
               f'4 lichtgebeurtenissen · 3 audiogebeurtenissen · schaars</text>')
    out.append(f'<text x="{left}" y="74" font-size="11" fill="#8a2f1f">'
               f'EXPERIMENT — geen deel van NAGLOED v1.0.4</text>')

    # secties
    for t0, t1, label in sections:
        out.append(f'<rect x="{x(t0):.1f}" y="88" width="{x(t1) - x(t0):.1f}" height="22" '
                   f'fill="#e7e1d6" stroke="#d4ccbe"/>')
        out.append(f'<text x="{x(t0) + 6:.1f}" y="104" font-size="10" fill="#6b645a">{label}</text>')
    # tijdas
    t = 0.0
    while t <= duration + 0.01:
        out.append(f'<line x1="{x(t):.1f}" y1="88" x2="{x(t):.1f}" y2="410" '
                   f'stroke="#d9d2c6" stroke-width="1"/>')
        out.append(f'<text x="{x(t):.1f}" y="428" font-size="10" fill="#6b645a" '
                   f'text-anchor="middle">{t:.0f}</text>')
        t += 5.0
    out.append(f'<text x="{right}" y="448" font-size="10" fill="#6b645a" '
               f'text-anchor="end">tijd (s)</text>')

    lanes = [(lane_light, "LICHT"), (lane_audio, "AUDIO"), (lane_ref, "DRAAGGOLF 18,2 kHz")]
    for y, label in lanes:
        out.append(f'<text x="{left - 12}" y="{y + 32}" font-size="11" font-weight="bold" '
                   f'fill="#3e3a34" text-anchor="end">{label}</text>')
        out.append(f'<line x1="{left}" y1="{y + 26}" x2="{right}" y2="{y + 26}" '
                   f'stroke="#c9c1b4"/>')

    tags = {"L1": "L1 · amber, links→rechts", "L2": "L2 · diffuse gloed",
            "L3": "L3 · onbeantwoord", "L4": "L4 · echo van L1"}
    for ev in score["light"]["events"]:
        x0 = x(float(ev["start_s"]))
        x1 = x(float(ev["end_s"]))
        fill = _hex_colour(ev["rgb"])
        out.append(f'<rect x="{x0:.1f}" y="{lane_light}" width="{max(3.0, x1 - x0):.1f}" '
                   f'height="{bar_h}" rx="3" fill="{fill}" stroke="#3a352e" stroke-width="0.7"/>')
        out.append(f'<text x="{x0 + 4:.1f}" y="{lane_light + 18}" font-size="10" fill="#1c1a17">'
                   f'{tags.get(ev["id"], ev["id"])}</text>')
        out.append(f'<text x="{x1:.1f}" y="{lane_light + 45}" font-size="9" fill="#5a544c" '
                   f'text-anchor="end">{float(ev["start_s"]):.1f}–{float(ev["end_s"]):.1f} s</text>')

    audio_tags = {"A0": "A0 · ver fragment", "A1": "A1 · impact (180 ms later)",
                  "A2": "A2 · omgekeerd, verlaagd"}
    for ev in score["program"]["events"]:
        start = float(ev["start_s"])
        dur = _transform_duration(score, ev) if ev["type"] == "transform" else float(ev["duration_s"])
        x0, x1 = x(start), x(start + dur)
        out.append(f'<rect x="{x0:.1f}" y="{lane_audio}" width="{max(3.0, x1 - x0):.1f}" '
                   f'height="{bar_h}" rx="3" fill="#3b4a52" stroke="#263038" stroke-width="0.7"/>')
        out.append(f'<text x="{x0 + 4:.1f}" y="{lane_audio + 18}" font-size="10" fill="#f2efe8">'
                   f'{audio_tags.get(ev["id"], ev["id"])}</text>')
        out.append(f'<text x="{x0 + 4:.1f}" y="{lane_audio + 34}" font-size="9" fill="#cfd8dc">'
                   f'{float(ev["peak_dbfs"]):.0f} dBFS</text>')

    out.append(f'<rect x="{left}" y="{lane_ref}" width="{right - left}" height="{bar_h}" rx="3" '
               f'fill="#6d4a2f" stroke="#3a2818" stroke-width="0.7"/>')
    out.append(f'<text x="{left + 8}" y="{lane_ref + 20}" font-size="10" fill="#f6efe6">'
               f'18.200 Hz · −30 dBFS · 20 Hz sinus-pan-LFO · continu</text>')
    out.append(f'<text x="{left + 8}" y="{lane_ref + 37}" font-size="9" fill="#e2d3c2">'
               f'18.700 R · 19.200 G · 19.700 B (kleuren volgen de lichtsporen hierboven)</text>')

    # relatie-annotaties
    out.append(f'<text x="{x(14.18):.1f}" y="{lane_audio - 8}" font-size="9" fill="#8a2f1f" '
               f'text-anchor="middle">180 ms</text>')
    out.append(f'<text x="{x(46.4) + 30:.1f}" y="{lane_audio - 8}" font-size="9" fill="#8a2f1f" '
               f'text-anchor="middle">echo +400 ms</text>')
    out.append(f'<text x="{x(38.0) + 4:.1f}" y="{lane_light - 8}" font-size="9" fill="#6b645a">'
               f'geen antwoord</text>')
    out.append(f'<text x="{left}" y="{height - 18}" font-size="10" fill="#6b645a">'
               f'Gebruik: ./run.sh · verificatierapport in out/verify-report.md · '
               f'niet op de hardware zonder HARDWARE-PROTOCOL.md</text>')
    out.append("</svg>")
    path.write_text("\n".join(out) + "\n", encoding="utf-8")


# --------------------------------------------------------------------------- #
# CLI
# --------------------------------------------------------------------------- #
def load_score(path: Path) -> dict:
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="NAGLOED v2 SpectraStrobe PoC renderer (EXPERIMENT)")
    parser.add_argument("--score", default=str(Path(__file__).with_name("score.json")))
    parser.add_argument("--out-dir", default=str(Path(__file__).with_name("out")))
    parser.add_argument("--pan-lfo-hz", type=float, default=None,
                        help="alleen voor corruptietests: overschrijf de referentie-pan-LFO")
    parser.add_argument("--pan-lfo-shape", default=None,
                        help="alleen voor corruptietests: sine/square/pulse")
    parser.add_argument("--carriers-only", action="store_true",
                        help="alleen voor corruptietests: programma overslaan")
    args = parser.parse_args(argv)

    score_path = Path(args.score)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    score = load_score(score_path)
    if args.pan_lfo_hz is not None:
        score["carriers"]["reference"]["pan_lfo_hz"] = args.pan_lfo_hz
    if args.pan_lfo_shape is not None:
        score["carriers"]["reference"]["pan_lfo_shape"] = args.pan_lfo_shape

    duration = float(score["meta"]["duration_s"])
    outputs = {}

    (mix48, _) = render_mix(score, 48000, with_programme=not args.carriers_only)
    master = out_dir / MASTER_NAME
    peak_master = write_pcm(master, 48000, 24, mix48[0], mix48[1])
    outputs[MASTER_NAME] = peak_master

    (mix44, _) = render_mix(score, 44100, with_programme=not args.carriers_only)
    variant = out_dir / VARIANT_NAME
    peak_variant = write_pcm(variant, 44100, 16, mix44[0], mix44[1])
    outputs[VARIANT_NAME] = peak_variant

    (_, program) = render_mix(score, 48000, with_programme=True)
    program_only = out_dir / PROGRAM_NAME
    peak_program = write_pcm(program_only, 48000, 24, program[0], program[1])
    outputs[PROGRAM_NAME] = peak_program

    write_events_csv(out_dir / "events.csv", score)
    write_timeline_svg(out_dir / "timeline.svg", score)

    car = score["carriers"]
    manifest = {
        "generator": "render.py",
        "experiment": EXPERIMENT_TITLE,
        "version": score["meta"]["version"],
        "score_file": score_path.name,
        "score_sha256": sha256_file(score_path),
        "seed": score["meta"]["seed"],
        "duration_s": duration,
        "carriers": {
            "reference_hz": car["reference"]["hz"],
            "reference_dbfs": car["reference"]["level_dbfs"],
            "pan_lfo_hz": car["reference"]["pan_lfo_hz"],
            "pan_lfo_shape": car["reference"]["pan_lfo_shape"],
            "colours_hz": {k: v["hz"] for k, v in car["colours"].items()},
            "max_colour_ratio": car["max_colour_ratio"],
        },
        "outputs": {},
    }
    for name, peak in outputs.items():
        path = out_dir / name
        manifest["outputs"][name] = {
            "sha256": sha256_file(path),
            "bytes": path.stat().st_size,
            "peak_dbfs": round(peak, 2),
        }
    with open(out_dir / "render-manifest.json", "w", encoding="utf-8", newline="\n") as handle:
        json.dump(manifest, handle, indent=2, sort_keys=True, ensure_ascii=False)
        handle.write("\n")

    light_count = len(score["light"]["events"])
    audio_count = len(score["program"]["events"])
    audio_seconds = sum(
        (_transform_duration(score, ev) if ev["type"] == "transform" else float(ev["duration_s"]))
        for ev in score["program"]["events"]
    )
    print("NAGLOED v2 — SpectraStrobe PoC render (EXPERIMENT, niet v1.0.4)")
    print(f"score         : {score_path.name}  seed {score['meta']['seed']}  duur {duration:.1f} s")
    print(f"48 kHz/24-bit : {MASTER_NAME}  piek {peak_master:+.2f} dBFS")
    print(f"44,1 kHz/16-bit: {VARIANT_NAME}  piek {peak_variant:+.2f} dBFS")
    print(f"programma-only: {PROGRAM_NAME}  piek {peak_program:+.2f} dBFS")
    print(f"draaggolven   : 18200,0 Hz referentie {car['reference']['level_dbfs']:+.1f} dBFS "
          f"(pan-LFO {car['reference']['pan_lfo_hz']:.1f} Hz, {car['reference']['pan_lfo_shape']})")
    print(f"                kleuren 18700/19200/19700 Hz, max {car['max_colour_ratio'] * 100:.0f}% "
          f"van referentie (score gebruikt {score['safety']['max_colour_ratio_used'] * 100:.0f}%)")
    print(f"events        : {light_count} licht, {audio_count} audio; "
          f"programma {audio_seconds:.1f} s van {duration:.1f} s "
          f"({100.0 * (1.0 - audio_seconds / duration):.0f}% stilte)")
    print(f"uitvoer       : {out_dir.resolve()}")
    for name in outputs:
        print(f"  - {name}")
    print("  - events.csv, timeline.svg, render-manifest.json")
    return 0


if __name__ == "__main__":
    sys.exit(main())
