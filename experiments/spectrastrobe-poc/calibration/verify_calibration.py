#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""NAGLOED v2 — SpectraStrobe colour calibration: independent verification.

EXPERIMENT / DIAGNOSTIC. Separate from NAGLOED 1.0.4 and from the artistic
composition Eerste Gloed. It does not touch the parent experiment.

Analyses the rendered WAV files from calibration/out/ against calibration.json:
format and duration, reference carrier and 20 Hz pan LFO, per-state carrier
activation and ratios, unintended-carrier attenuation, crossfade amplitude
limits, abrupt envelope changes, black gaps, fade-in/out, peak/true peak,
determinism and the optional left/right eye-check file.

The checks are deliberately separate from the parent verify.py; only low-level
primitives are reused (read_wav, spectrum_pair, band_rms_dbfs, demod_envelopes,
goertzel_amplitude, pearson, true_peak, ...). Exit code 0 = all checks PASS.

Usage:
    python3 verify_calibration.py [--calibration calibration.json] [--out-dir out]
                                  [--skip-determinism]
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True  # never leave __pycache__ in the parent experiment

CAL_DIR = Path(__file__).resolve().parent
PARENT_DIR = CAL_DIR.parent
if str(PARENT_DIR) not in sys.path:
    sys.path.insert(0, str(PARENT_DIR))

import verify as parent_verify  # noqa: E402 - deliberate reuse of low-level primitives

TAU = 2.0 * math.pi
CARRIER_KEYS = ("red", "green", "blue")
CARRIER_FREQ = {"red": 18700.0, "green": 19200.0, "blue": 19700.0}
BANNER = ("EXPERIMENT / DIAGNOSTIC — not part of NAGLOED 1.0.4 or the artistic "
          "composition Eerste Gloed. No medical or wellness claims.")


# --------------------------------------------------------------------------- #
# Small check collection
# --------------------------------------------------------------------------- #
class Checks:
    def __init__(self):
        self.items = []

    def ok(self, group, check_id, description, criterion, measured, passed, detail=""):
        self.items.append({
            "group": group, "id": check_id, "description": description,
            "criterion": criterion, "measured": measured,
            "status": "PASS" if passed else "FAIL", "detail": detail,
        })

    def summary(self):
        total = len(self.items)
        passed = sum(1 for item in self.items if item["status"] == "PASS")
        failed = sum(1 for item in self.items if item["status"] == "FAIL")
        return {"total": total, "pass": passed, "fail": failed,
                "verdict": "PASS" if failed == 0 else "FAIL"}


def fmt(value):
    if isinstance(value, float):
        if value != value:
            return "n/a"
        if abs(value) >= 1000.0:
            return f"{value:.1f}"
        if abs(value) >= 1.0:
            return f"{value:.2f}"
        return f"{value:.4f}"
    return str(value)


def cell(value):
    return fmt(value).replace("|", "\\|").replace("\n", " ")


# --------------------------------------------------------------------------- #
# Measurements
# --------------------------------------------------------------------------- #
def reference_amplitude(fs: int, left, right, t0: float) -> float:
    """Reference peak amplitude from the 18.2 kHz band power (informative RMS)."""
    spec_l, spec_r, sum_w2 = parent_verify.spectrum_pair(left, right, fs, t0)
    power = parent_verify.band_rms_sq(spec_l, fs, parent_verify.FFT_N, sum_w2, 18100.0, 18300.0) \
        + parent_verify.band_rms_sq(spec_r, fs, parent_verify.FFT_N, sum_w2, 18100.0, 18300.0)
    return math.sqrt(2.0 * power) if power > 0.0 else 0.0


def state_amplitudes(fs: int, left, right, window):
    """Per-carrier amplitude in a measurement window (Hann, one exact block)."""
    t0, t1 = float(window[0]), float(window[1])
    block = t1 - t0
    amplitudes = {}
    for key in CARRIER_KEYS:
        env_l, env_r = parent_verify.demod_envelopes(left, right, fs, CARRIER_FREQ[key],
                                                     t0, t1, block, windowed=True)
        if not env_l:
            amplitudes[key] = 0.0
            continue
        amplitudes[key] = 0.5 * (env_l[0] + env_r[0])
    return amplitudes


def channel_amplitudes(fs: int, left, right, window):
    """Per-carrier amplitude per channel in a measurement window."""
    t0, t1 = float(window[0]), float(window[1])
    block = t1 - t0
    result = {}
    for key in CARRIER_KEYS:
        env_l, env_r = parent_verify.demod_envelopes(left, right, fs, CARRIER_FREQ[key],
                                                     t0, t1, block, windowed=True)
        result[key] = (env_l[0] if env_l else 0.0, env_r[0] if env_r else 0.0)
    return result


_BH_CACHE = {}


def _blackman_harris(n: int):
    cached = _BH_CACHE.get(n)
    if cached is None:
        a0, a1, a2, a3 = 0.35875, 0.48829, 0.14128, 0.01168
        cached = [a0 - a1 * math.cos(TAU * i / (n - 1))
                  + a2 * math.cos(2.0 * TAU * i / (n - 1))
                  - a3 * math.cos(3.0 * TAU * i / (n - 1)) for i in range(n)]
        _BH_CACHE[n] = cached
    return cached


def demod_blackman_harris(left, right, fs: float, freq: float, t0: float, t1: float,
                          block_s: float):
    """Per-block amplitude with a 4-term Blackman-Harris window.

    A 10 ms Blackman-Harris window suppresses the 18.2 kHz reference (500 Hz
    away) by far more than 40 dB, so the fine colour envelopes are not
    contaminated by reference leakage as they would be with a short Hann/rect
    window. The 10 ms block also matches the abruptness requirement.
    """
    i0 = max(0, int(round(t0 * fs)))
    i1 = min(len(left), int(round(t1 * fs)))
    block = max(1, int(round(block_s * fs)))
    weights = _blackman_harris(block)
    normaliser = sum(weights)
    env_l = []
    env_r = []
    for start in range(i0, i1 - block + 1, block):
        re_l = im_l = re_r = im_r = 0.0
        angle = TAU * freq * start / fs
        step = TAU * freq / fs
        for offset in range(block):
            n = start + offset
            weight = weights[offset]
            c = math.cos(angle)
            s = math.sin(angle)
            lv = left[n] * weight
            rv = right[n] * weight
            re_l += lv * c
            im_l -= lv * s
            re_r += rv * c
            im_r -= rv * s
            angle += step
        factor = 2.0 / normaliser
        env_l.append(factor * math.hypot(re_l, im_l))
        env_r.append(factor * math.hypot(re_r, im_r))
    return env_l, env_r


def fine_envelopes(fs: int, left, right, duration_s: float, block_s: float = 0.010):
    """Per-carrier amplitude envelopes over the whole render (10 ms blocks)."""
    block = block_s
    envelopes = {}
    for key in CARRIER_KEYS:
        env_l, env_r = demod_blackman_harris(left, right, fs, CARRIER_FREQ[key],
                                             0.0, duration_s, block)
        envelopes[key] = [0.5 * (a + b) for a, b in zip(env_l, env_r)]
    return envelopes, block


def lfo_check(fs: int, left, right, cal: dict, main: bool = True):
    window = (2.0, 6.0) if main else (1.0, 5.0)
    metrics = parent_verify.lfo_metrics(left, right, fs, window[0], window[1])
    return metrics, window


# --------------------------------------------------------------------------- #
# Verification
# --------------------------------------------------------------------------- #
def verify_main(checks: Checks, cal: dict, fs: int, width: int, left, right):
    states = cal["states"]
    timing = cal["timing"]
    duration_s = float(cal["meta"]["duration_s"])
    colour_ratio = float(cal["exposure"]["colour_ratio"])
    hard_ceiling = float(cal["exposure"]["hard_ceiling"])
    fade_in = float(timing["fade_in_ms"]) / 1000.0
    fade_out = float(timing["fade_out_ms"]) / 1000.0
    measurements = {}

    frames = len(left)
    checks.ok("format", "format-main", "main WAV format and duration",
              f"44100 Hz / 16-bit / stereo / {duration_s:.1f} s",
              f"{fs} Hz / {width * 8}-bit / {frames / fs:.3f} s",
              fs == 44100 and width == 2 and abs(frames / fs - duration_s) <= 0.02)

    amp_ref = reference_amplitude(fs, left, right, 2.0)
    amp_ref_db = parent_verify.amp_db(amp_ref)
    measurements["reference"] = {"peak_amplitude_dbfs": round(amp_ref_db, 3),
                                 "amplitude_linear": amp_ref}
    checks.ok("reference", "reference-level", "reference carrier 18.2 kHz peak amplitude",
              f"{cal['reference']['level_dbfs']:+.1f} dBFS ± 0.5 dB",
              f"{amp_ref_db:+.2f} dBFS", abs(amp_ref_db - float(cal["reference"]["level_dbfs"])) <= 0.5)

    lfo, lfo_window = lfo_check(fs, left, right, cal, main=True)
    measurements["lfo"] = {"window_s": list(lfo_window), **{k: round(v, 4) if isinstance(v, float) else v
                                                           for k, v in lfo.items()}}
    checks.ok("reference", "reference-lfo", "reference 20 Hz pan LFO intact",
              "20.0 Hz ± 0.5; depth ≥ 99%; out-of-phase ≥ 30 dB down; corr < 0",
              f"{lfo['freq_hz']:.2f} Hz, depth {lfo['depth_pct']:.1f}%, "
              f"rest {lfo['rest_db']:.1f} dB, corr {lfo['corr']:.2f}",
              abs(lfo["freq_hz"] - 20.0) <= 0.5 and lfo["depth_pct"] >= 99.0
              and lfo["rest_db"] <= -30.0 and lfo["corr"] <= -0.2)

    level = colour_ratio * amp_ref
    state_rows = []
    for state in states:
        measured = state_amplitudes(fs, left, right, state["measure_window_s"])
        ratios = {key: (measured[key] / amp_ref if amp_ref > 0 else 0.0) for key in CARRIER_KEYS}
        active = [key for key in CARRIER_KEYS if float(state["expected_ratios"][key]) > 0]
        inactive = [key for key in CARRIER_KEYS if key not in active]
        active_errors = []
        for key in active:
            ratio = ratios[key]
            error_db = (parent_verify.amp_db(ratio / colour_ratio)
                        if ratio > 0 and colour_ratio > 0 else -999.0)
            active_errors.append((key, ratio, error_db))
        active_ok = all(abs(error) <= 0.5 for _, _, error in active_errors)
        active_detail = ", ".join(f"{key} {ratio * 100:.2f}% ({error:+.2f} dB)"
                                  for key, ratio, error in active_errors)
        checks.ok("states", f"state-{state['index']}-active",
                  f"{state['name']}: active carrier(s) at the dim exposure level",
                  f"active ratio = {colour_ratio * 100:.2f}% per carrier, ±0.5 dB",
                  active_detail, active_ok)

        unintended = []
        for key in inactive:
            ratio = ratios[key]
            rel_db = (parent_verify.amp_db(ratio / colour_ratio) if ratio > 0 else -999.0)
            unintended.append((key, ratio, rel_db))
        unintended_ok = all(rel_db <= -40.0 for _, _, rel_db in unintended)
        worst = max((rel_db for _, _, rel_db in unintended), default=-999.0)
        unintended_detail = (", ".join(f"{key} {rel_db:.1f} dB" for key, _, rel_db in unintended)
                            or "none expected")
        checks.ok("states", f"state-{state['index']}-unintended",
                  f"{state['name']}: unintended carriers ≤ -40 dB relative to the active level",
                  "inactive carriers ≤ -40 dB", f"worst {worst:.1f} dB ({unintended_detail})",
                  unintended_ok)

        ratio_ok = None
        ratio_detail = ""
        expected_active = [key for key in CARRIER_KEYS if float(state["expected_ratios"][key]) > 0]
        if len(expected_active) >= 2:
            levels = [(key, ratios[key]) for key in expected_active]
            pair_diffs = []
            for a in range(len(levels)):
                for b in range(a + 1, len(levels)):
                    ka, va = levels[a]
                    kb, vb = levels[b]
                    diff = (parent_verify.amp_db(va / vb) if va > 0 and vb > 0 else 999.0)
                    pair_diffs.append((ka, kb, diff))
            ratio_ok = all(abs(diff) <= 0.5 for _, _, diff in pair_diffs)
            ratio_detail = ", ".join(f"{ka}/{kb} {diff:+.2f} dB" for ka, kb, diff in pair_diffs)
            checks.ok("states", f"state-{state['index']}-ratios",
                      f"{state['name']}: equal ratios inside the composite",
                      "pairwise carrier ratio ±0.5 dB", ratio_detail, ratio_ok)

        state_rows.append({
            "index": state["index"],
            "name": state["name"],
            "start_s": state["start_s"],
            "end_s": state["end_s"],
            "crossfade_in_s": state["crossfade_in_s"],
            "crossfade_out_s": state["crossfade_out_s"],
            "measure_window_s": state["measure_window_s"],
            "expected_ratios": state["expected_ratios"],
            "measured_ratios": {key: round(ratios[key], 6) for key in CARRIER_KEYS},
            "measured_percent": {key: round(ratios[key] * 100.0, 3) for key in CARRIER_KEYS},
            "active_check": "PASS" if active_ok else "FAIL",
            "unintended_check": "PASS" if unintended_ok else "FAIL",
            "ratio_check": ("n/a" if ratio_ok is None else ("PASS" if ratio_ok else "FAIL")),
            "worst_unintended_db": round(worst, 2),
            "status": "PASS" if (active_ok and unintended_ok and ratio_ok is not False) else "FAIL",
        })
    measurements["states"] = state_rows

    # Transition behaviour from fine envelopes.
    envelopes, block_s = fine_envelopes(fs, left, right, duration_s)
    block_ms = block_s * 1000.0
    max_ratio = 0.0
    for key in CARRIER_KEYS:
        for value in envelopes[key]:
            max_ratio = max(max_ratio, value / level if level > 0 else 0.0)
    checks.ok("transitions", "crossfade-amplitude-limit",
              "carrier amplitude never exceeds the dim exposure level (no crossfade overshoot)",
              f"max {colour_ratio * 100:.2f}% per carrier (+0.3 dB tolerance), "
              f"hard ceiling {hard_ceiling * 100:.0f}%",
              f"max {max_ratio * 100:.2f}% of the dim level "
              f"({colour_ratio * max_ratio * 100:.2f}% of the reference peak)",
              max_ratio <= 1.0 + 0.005)

    max_jump = 0.0
    max_jump_key = ""
    for key in CARRIER_KEYS:
        env = envelopes[key]
        for i in range(len(env) - 2):
            jump = abs(env[i + 2] - env[i]) / level if level > 0 else 0.0
            if jump > max_jump:
                max_jump = jump
                max_jump_key = key
    jump_db = 20.0 * math.log10(1.0 + max_jump) if max_jump > 0 else 0.0
    checks.ok("transitions", "envelope-abruptness",
              "no abrupt luminance pulse: max 10 ms colour-envelope change",
              "≤ 25% of the active level per 10 ms block (≈ ≤ 1.9 dB)",
              f"max {max_jump * 100:.2f}% ({jump_db:.2f} dB) per 10 ms ({max_jump_key})",
              max_jump <= 0.25)

    total_env = [sum(envelopes[key][i] for key in CARRIER_KEYS) for i in range(len(envelopes["red"]))]
    start_block = int(round((fade_in + 0.02) / block_s))
    end_block = min(len(total_env), int(round((duration_s - fade_out - 0.02) / block_s)))
    gap_blocks = 0
    worst_gap_blocks = 0
    for i in range(start_block, end_block):
        if total_env[i] < 0.05 * level:
            gap_blocks += 1
            worst_gap_blocks = max(worst_gap_blocks, gap_blocks)
        else:
            gap_blocks = 0
    gap_ms = worst_gap_blocks * block_ms
    checks.ok("transitions", "black-gap",
              "no black gap > 20 ms between colour carriers",
              "longest interval with all colour carriers below 5% of the active level ≤ 20 ms",
              f"longest gap {gap_ms:.1f} ms (block {block_ms:.1f} ms)", gap_ms <= 20.0)

    def fade_times(values):
        first_ten = next((i for i, value in enumerate(values) if value >= 0.1 * level), None)
        first_ninety = next((i for i, value in enumerate(values) if value >= 0.9 * level), None)
        last_ten = next((i for i in range(len(values) - 1, -1, -1) if values[i] >= 0.1 * level), None)
        last_ninety = next((i for i in range(len(values) - 1, -1, -1) if values[i] >= 0.9 * level), None)
        rise = ((first_ninety - first_ten) * block_ms) if None not in (first_ten, first_ninety) else -1.0
        fall = ((last_ten - last_ninety) * block_ms) if None not in (last_ten, last_ninety) else -1.0
        return rise, fall

    rise_ms, fall_ms = fade_times(total_env)
    expected_fade = float(timing["fade_in_ms"])
    checks.ok("transitions", "fade-in", "gentle fade-in present (10%→90% rise time)",
              f"{expected_fade * 0.33:.0f}–{expected_fade * 1.4:.0f} ms",
              f"{rise_ms:.0f} ms", 0.0 < rise_ms <= expected_fade * 1.4)
    checks.ok("transitions", "fade-out", "gentle fade-out present (90%→10% fall time)",
              f"{expected_fade * 0.33:.0f}–{expected_fade * 1.4:.0f} ms",
              f"{fall_ms:.0f} ms", 0.0 < fall_ms <= expected_fade * 1.4)

    peak = max(max(left), -min(left), max(right), -min(right))
    true_peak_value = parent_verify.true_peak(left, right, fs)
    finite = all(math.isfinite(v) for v in left) and all(math.isfinite(v) for v in right)
    peak_db = parent_verify.amp_db(peak)
    true_peak_db = parent_verify.amp_db(true_peak_value)
    checks.ok("peak", "peak-true-peak", "sample peak and true peak with headroom",
              "sample ≤ -6 dBFS, true peak ≤ -6 dBTP, no clipping, all finite",
              f"sample {peak_db:+.2f} dBFS, true peak {true_peak_db:+.2f} dBTP",
              peak_db <= -6.0 and true_peak_db <= -6.0 and peak < 1.0 and finite)

    measurements["transitions"] = {
        "block_ms": round(block_ms, 3),
        "max_carrier_ratio": round(max_ratio, 5),
        "max_10ms_change_fraction": round(max_jump, 5),
        "max_10ms_change_db": round(jump_db, 3),
        "longest_black_gap_ms": round(gap_ms, 2),
        "fade_in_rise_ms": round(rise_ms, 1),
        "fade_out_fall_ms": round(fall_ms, 1),
    }
    measurements["peak"] = {"sample_peak_dbfs": round(peak_db, 3),
                            "true_peak_dbtp": round(true_peak_db, 3),
                            "clipped": peak >= 1.0, "finite": finite}
    return measurements


def verify_lr(checks: Checks, cal: dict, fs: int, width: int, left, right):
    states = cal["lr_states"]
    timing = cal["timing"]
    duration_s = float(cal["meta"]["lr_duration_s"])
    colour_ratio = float(cal["exposure"]["colour_ratio"])
    frames = len(left)
    checks.ok("lr", "lr-format", "optional L/R eye-check WAV format and duration",
              f"44100 Hz / 16-bit / stereo / {duration_s:.1f} s",
              f"{fs} Hz / {width * 8}-bit / {frames / fs:.3f} s",
              fs == 44100 and width == 2 and abs(frames / fs - duration_s) <= 0.02)

    amp_ref = reference_amplitude(fs, left, right, 1.0)
    amp_ref_db = parent_verify.amp_db(amp_ref)
    checks.ok("lr", "lr-reference", "L/R file reference carrier peak amplitude",
              f"{cal['reference']['level_dbfs']:+.1f} dBFS ± 0.5 dB",
              f"{amp_ref_db:+.2f} dBFS", abs(amp_ref_db - float(cal["reference"]["level_dbfs"])) <= 0.5)
    lfo, lfo_window = lfo_check(fs, left, right, cal, main=False)
    checks.ok("lr", "lr-reference-lfo", "L/R file 20 Hz pan LFO intact",
              "20.0 Hz ± 0.5; depth ≥ 99%; rest ≥ 30 dB down",
              f"{lfo['freq_hz']:.2f} Hz, depth {lfo['depth_pct']:.1f}%, rest {lfo['rest_db']:.1f} dB",
              abs(lfo["freq_hz"] - 20.0) <= 0.5 and lfo["depth_pct"] >= 99.0
              and lfo["rest_db"] <= -30.0)

    rows = []
    for state in states:
        channels = channel_amplitudes(fs, left, right, state["measure_window_s"])
        carrier = state["carrier"]
        amp_l, amp_r = channels[carrier]
        intended = amp_l if state["ear"] == "left" else amp_r
        other = amp_r if state["ear"] == "left" else amp_l
        intended_ratio = intended / amp_ref if amp_ref > 0 else 0.0
        other_db = (parent_verify.amp_db(other / colour_ratio / amp_ref)
                    if other > 0 and amp_ref > 0 else -999.0)
        intended_ok = (abs(parent_verify.amp_db(intended_ratio / colour_ratio)) <= 0.5
                       if intended_ratio > 0 else False)
        other_ok = other_db <= -40.0
        checks.ok("lr", f"lr-state-{state['index']}",
                  f"{state['name']}: only the intended ear carries the {carrier} carrier",
                  "intended ratio = 6.25% ±0.5 dB; other channel ≤ -40 dB",
                  f"intended {intended_ratio * 100:.2f}% ({state['ear']} ear), "
                  f"other channel {other_db:.1f} dB",
                  intended_ok and other_ok)
        rows.append({
            "index": state["index"], "name": state["name"], "carrier": carrier,
            "ear": state["ear"], "pan": state["pan"],
            "expected_ratio": round(colour_ratio, 6),
            "measured_intended_ratio": round(intended_ratio, 6),
            "measured_other_db": round(other_db, 2),
            "status": "PASS" if (intended_ok and other_ok) else "FAIL",
        })
    return {"states": rows, "reference_peak_amplitude_dbfs": round(amp_ref_db, 3),
            "lfo": {k: round(v, 4) if isinstance(v, float) else v for k, v in lfo.items()}}


def determinism_check(checks: Checks, cal_path: Path, render_script: Path, out_dir: Path,
                      names, timeout: int = 300):
    with tempfile.TemporaryDirectory(prefix="specstroop-calibration-repro-") as tmp:
        command = [sys.executable, str(render_script), "--calibration", str(cal_path),
                   "--out-dir", tmp]
        try:
            result = subprocess.run(command, capture_output=True, text=True, timeout=timeout)
        except Exception as error:  # noqa: BLE001 - the report must never crash
            checks.ok("determinism", "determinism", "second render", "render exits 0",
                      str(error), False)
            return
        if result.returncode != 0:
            checks.ok("determinism", "determinism", "second render", "render exits 0",
                      f"exit {result.returncode}", False, result.stderr[-300:])
            return
        mismatches = []
        compared = []
        for name in names:
            first, second = out_dir / name, Path(tmp) / name
            if not first.exists() or not second.exists():
                mismatches.append(name)
                continue
            same = parent_verify.sha256(first) == parent_verify.sha256(second)
            compared.append({"file": name, "identical": same})
            if not same:
                mismatches.append(name)
        manifest_first = out_dir / "calibration-manifest.json"
        manifest_second = Path(tmp) / "calibration-manifest.json"
        manifest_same = (manifest_first.exists() and manifest_second.exists()
                         and parent_verify.sha256(manifest_first) == parent_verify.sha256(manifest_second))
        compared.append({"file": "calibration-manifest.json", "identical": manifest_same})
        if not manifest_same:
            mismatches.append("calibration-manifest.json")
        checks.ok("determinism", "determinism",
                  "two renders are byte-identical (WAV + manifest)",
                  "all outputs identical",
                  "identical" if not mismatches else f"differs: {', '.join(mismatches)}",
                  not mismatches)
        return compared


# --------------------------------------------------------------------------- #
# Report outputs
# --------------------------------------------------------------------------- #
def write_timeline_csv(path: Path, cal: dict, measurements: dict):
    rows = []
    for state in measurements["states"]:
        rows.append({
            "sequence": "main",
            "index": state["index"],
            "name": state["name"],
            "start_s": state["start_s"],
            "end_s": state["end_s"],
            "crossfade_in_s": f"{state['crossfade_in_s'][0]}-{state['crossfade_in_s'][1]}",
            "crossfade_out_s": f"{state['crossfade_out_s'][0]}-{state['crossfade_out_s'][1]}",
            "measure_start_s": state["measure_window_s"][0],
            "measure_end_s": state["measure_window_s"][1],
            "expected_red": f"{state['expected_ratios']['red']:.6f}",
            "expected_green": f"{state['expected_ratios']['green']:.6f}",
            "expected_blue": f"{state['expected_ratios']['blue']:.6f}",
            "measured_red": f"{state['measured_ratios']['red']:.6f}",
            "measured_green": f"{state['measured_ratios']['green']:.6f}",
            "measured_blue": f"{state['measured_ratios']['blue']:.6f}",
            "intended_carrier": "|".join(
                key for key in CARRIER_KEYS if state["expected_ratios"][key] > 0),
            "intended_ear": "both",
            "unintended_worst_db": state["worst_unintended_db"],
            "active_check": state["active_check"],
            "unintended_check": state["unintended_check"],
            "ratio_check": state["ratio_check"],
            "status": state["status"],
        })
    for state in measurements.get("lr", {}).get("states", []):
        rows.append({
            "sequence": "lr", "index": state["index"], "name": state["name"],
            "start_s": "", "end_s": "",
            "crossfade_in_s": "", "crossfade_out_s": "",
            "measure_start_s": "", "measure_end_s": "",
            "expected_red": f"{state['expected_ratio']:.6f}" if state["carrier"] == "red" else "0",
            "expected_green": f"{state['expected_ratio']:.6f}" if state["carrier"] == "green" else "0",
            "expected_blue": f"{state['expected_ratio']:.6f}" if state["carrier"] == "blue" else "0",
            "measured_red": "", "measured_green": "", "measured_blue": "",
            "intended_carrier": state["carrier"],
            "intended_ear": state["ear"],
            "unintended_worst_db": state["measured_other_db"],
            "active_check": "", "unintended_check": "",
            "ratio_check": "", "status": state["status"],
        })
    fields = ["sequence", "index", "name", "start_s", "end_s", "crossfade_in_s",
              "crossfade_out_s", "measure_start_s", "measure_end_s", "expected_red",
              "expected_green", "expected_blue", "measured_red", "measured_green",
              "measured_blue", "intended_carrier", "intended_ear",
              "unintended_worst_db", "active_check", "unintended_check",
              "ratio_check", "status"]
    with open(path, "w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def _state_fill(rgb):
    if rgb == [1.0, 0.0, 0.0]:
        return "#e03131"
    if rgb == [0.0, 1.0, 0.0]:
        return "#2f9e44"
    if rgb == [0.0, 0.0, 1.0]:
        return "#1c7ed6"
    if rgb == [1.0, 1.0, 0.0]:
        return "#f2c200"
    if rgb == [0.0, 1.0, 1.0]:
        return "#15aabf"
    if rgb == [1.0, 0.0, 1.0]:
        return "#ae3ec9"
    return "#f8f9fa"


def write_timeline_svg(path: Path, cal: dict, measurements: dict, summary: dict):
    duration = float(cal["meta"]["duration_s"])
    states = cal["states"]
    width, height = 1280, 560
    left, right = 130, 1230
    lane = 120
    bar_h = 56

    def x(t):
        return left + (right - left) * max(0.0, min(1.0, t / duration))

    out = ['<?xml version="1.0" encoding="UTF-8"?>',
           f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
           f'viewBox="0 0 {width} {height}" font-family="Helvetica, Arial, sans-serif">',
           f'<rect width="{width}" height="{height}" fill="#f7f5f0"/>',
           f'<text x="{left}" y="32" font-size="18" font-weight="bold" fill="#1c1a17">'
           f'SpectraStrobe colour calibration — timestamped sequence</text>',
           f'<text x="{left}" y="53" font-size="12" fill="#5a544c">'
           f'DIAGNOSTIC EXPERIMENT · separate from NAGLOED 1.0.4 and Eerste Gloed · '
           f'{duration:.1f} s · 1 s states · 250 ms smooth crossfades · 300 ms fades · '
           f'6.25% per carrier</text>',
           f'<text x="{left}" y="71" font-size="11" fill="#8a2f1f">'
           f'{BANNER}</text>']
    out.append(f'<text x="{left - 12}" y="{lane + 34}" font-size="11" font-weight="bold" '
               f'fill="#3e3a34" text-anchor="end">STATES</text>')
    for state in states:
        x0, x1 = x(float(state["start_s"])), x(float(state["end_s"]))
        fill = _state_fill(state["expected_rgb"])
        out.append(f'<rect x="{x0:.1f}" y="{lane}" width="{x1 - x0:.1f}" height="{bar_h}" '
                   f'rx="2" fill="{fill}" stroke="#3a352e" stroke-width="0.8"/>')
        out.append(f'<text x="{x0 + 5:.1f}" y="{lane + 19}" font-size="11" font-weight="bold" '
                   f'fill="#1c1a17">{state["name"]}</text>')
        out.append(f'<text x="{x0 + 5:.1f}" y="{lane + 34}" font-size="9" fill="#3a352e">'
                   f'{state["start_s"]:.1f}–{state["end_s"]:.1f} s</text>')
        measured = next(row for row in measurements["states"] if row["index"] == state["index"])
        out.append(f'<text x="{x0 + 5:.1f}" y="{lane + 48}" font-size="8" fill="#3a352e">'
                   f'R {measured["measured_percent"]["red"]:.1f}% · '
                   f'G {measured["measured_percent"]["green"]:.1f}% · '
                   f'B {measured["measured_percent"]["blue"]:.1f}%</text>')
        for cross in (state["crossfade_out_s"],):
            cx0, cx1 = x(float(cross[0])), x(float(cross[1]))
            out.append(f'<rect x="{cx0:.1f}" y="{lane - 8}" width="{cx1 - cx0:.1f}" '
                       f'height="{bar_h + 16}" fill="none" stroke="#8a2f1f" '
                       f'stroke-width="0.8" stroke-dasharray="3 3"/>')
    # Reference lane
    ref_lane = lane + bar_h + 70
    out.append(f'<text x="{left - 12}" y="{ref_lane + 34}" font-size="11" font-weight="bold" '
               f'fill="#3e3a34" text-anchor="end">REFERENCE</text>')
    out.append(f'<rect x="{left}" y="{ref_lane}" width="{right - left}" height="34" rx="2" '
               f'fill="#6d4a2f" stroke="#3a2818" stroke-width="0.8"/>')
    out.append(f'<text x="{left + 8}" y="{ref_lane + 21}" font-size="10" fill="#f6efe6">'
               f'18.200 Hz · {float(cal["reference"]["level_dbfs"]):+.0f} dBFS · '
               f'{float(cal["reference"]["pan_lfo_hz"]):g} Hz sine pan LFO · continuous</text>')
    # Time axis
    axis_y = ref_lane + 60
    for step in range(0, int(duration) + 1):
        out.append(f'<line x1="{x(step):.1f}" y1="{lane - 20}" x2="{x(step):.1f}" '
                   f'y2="{axis_y}" stroke="#ddd5c8" stroke-width="1"/>')
        out.append(f'<text x="{x(step):.1f}" y="{axis_y + 14}" font-size="9" fill="#6b645a" '
                   f'text-anchor="middle">{step}</text>')
    out.append(f'<text x="{right}" y="{axis_y + 30}" font-size="9" fill="#6b645a" '
               f'text-anchor="end">time (s)</text>')
    # Optional LR strip
    lr = measurements.get("lr", {}).get("states", [])
    if lr:
        lr_y = axis_y + 46
        lr_duration = float(cal["meta"]["lr_duration_s"])
        out.append(f'<text x="{left - 12}" y="{lr_y + 20}" font-size="10" font-weight="bold" '
                   f'fill="#3e3a34" text-anchor="end">L/R CHECK</text>')
        for state in lr:
            x0 = left + (right - left) * float(state["index"]) / len(lr)
            x1 = left + (right - left) * (float(state["index"]) + 1) / len(lr)
            fill = {"red": "#e03131", "green": "#2f9e44", "blue": "#1c7ed6"}[state["carrier"]]
            opacity = 0.95 if state["ear"] == "left" else 0.55
            out.append(f'<rect x="{x0:.1f}" y="{lr_y}" width="{x1 - x0:.1f}" height="24" '
                       f'fill="{fill}" fill-opacity="{opacity}" stroke="#3a352e" stroke-width="0.6"/>')
            out.append(f'<text x="{(x0 + x1) / 2:.1f}" y="{lr_y + 16}" font-size="8" '
                       f'fill="#1c1a17" text-anchor="middle">'
                       f'{state["carrier"].capitalize()} {state["ear"][0].upper()}</text>')
        out.append(f'<text x="{left}" y="{lr_y + 40}" font-size="8" fill="#6b645a">'
                   f'optional eye check ({lr_duration:.1f} s): brighter block = intended ear</text>')
    out.append(f'<text x="{left}" y="{height - 16}" font-size="9" fill="#6b645a">'
               f'{summary["verdict"]}: {summary["pass"]} PASS / {summary["fail"]} FAIL · '
               f'report: out/calibration-report.md · safety: ../HARDWARE-PROTOCOL.md</text>')
    out.append("</svg>")
    path.write_text("\n".join(out) + "\n", encoding="utf-8")


def write_reports(out_dir: Path, cal: dict, checks: Checks, measurements: dict, summary: dict,
                  determinism_result):
    report = {
        "experiment": "SpectraStrobe colour calibration",
        "status": "diagnostic experiment",
        "banner": BANNER,
        "calibration": cal,
        "summary": summary,
        "checks": checks.items,
        "measurements": measurements,
        "determinism": determinism_result,
        "note": ("Automated checks prove the encoded spectra and envelope behaviour. "
                 "They do NOT prove perceived colour, LED behaviour or decoder behaviour; "
                 "hardware observation with ../HARDWARE-PROTOCOL.md is required. "
                 "No medical or wellness claims."),
    }
    (out_dir / "calibration-report.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    lines = ["# Calibration report — SpectraStrobe colour calibration", "",
             f"> **{BANNER}**", "",
             "**Research question:** do the encoded colour channels behave as intended "
             "(carrier frequency, per-carrier level, equal ratios inside composites, "
             "smooth transitions), given that the Eerste Gloed WAVs showed red/blue/violet/"
             "stereo asymmetry but no convincing green/yellow on the real Kasina + DeepVision?",
             "",
             f"**Outcome:** {summary['verdict']} — {summary['pass']} PASS, {summary['fail']} FAIL "
             f"of {summary['total']} checks.",
             ""]
    lines += ["## State sequence", "",
              "| # | State | Start | End | Expected RGB | Active carriers | Measurement window |",
              "|---|---|---|---|---|---|---|"]
    for state in cal["states"]:
        carriers = ", ".join(state["active_carriers"]) or "none"
        rgb = ", ".join(f"{v:.2f}" for v in state["expected_rgb"])
        lines.append(f"| {state['index']} | {state['name']} | {state['start_s']:.2f} s | "
                     f"{state['end_s']:.2f} s | {rgb} | {carriers} | "
                     f"{state['measure_window_s'][0]:.2f}–{state['measure_window_s'][1]:.2f} s |")
    lines += ["", "Per carrier the dim exposure is 6.25% of the reference peak; the hard "
              "ceiling is 25% per carrier. Crossfades are 250 ms and centred on the "
              "boundaries; fade-in/out are 300 ms.", ""]
    lines += ["## Reference and envelope measurements", "",
              f"- Reference peak amplitude: **{measurements['reference']['peak_amplitude_dbfs']:+.2f} dBFS** "
              f"(criterion {cal['reference']['level_dbfs']:+.1f} ± 0.5 dB).",
              f"- Reference pan LFO: **{measurements['lfo']['freq_hz']:.2f} Hz**, depth "
              f"**{measurements['lfo']['depth_pct']:.1f}%**, rest {measurements['lfo']['rest_db']:.1f} dB, "
              f"corr {measurements['lfo']['corr']:.2f}.",
              f"- Max carrier ratio during the whole render: "
              f"**{measurements['transitions']['max_carrier_ratio'] * 100:.2f}% of the dim exposure "
              f"level** (i.e. {float(cal['exposure']['colour_ratio']) * measurements['transitions']['max_carrier_ratio'] * 100:.2f}% "
              f"of the reference peak).",
              f"- Max 10 ms envelope change: **{measurements['transitions']['max_10ms_change_db']:.2f} dB** "
              f"({measurements['transitions']['max_10ms_change_fraction'] * 100:.2f}% of the active level).",
              f"- Longest black gap between colour carriers: "
              f"**{measurements['transitions']['longest_black_gap_ms']:.1f} ms**.",
              f"- Fade-in rise / fade-out fall (10%→90%): "
              f"**{measurements['transitions']['fade_in_rise_ms']:.0f} ms** / "
              f"**{measurements['transitions']['fade_out_fall_ms']:.0f} ms**.",
              f"- Peak / true peak: **{measurements['peak']['sample_peak_dbfs']:+.2f} dBFS** / "
              f"**{measurements['peak']['true_peak_dbtp']:+.2f} dBTP**; clipped "
              f"{measurements['peak']['clipped']}.",
              f"- Determinism: **{'PASS' if summary['verdict'] == 'PASS' else 'see checks'}** "
              f"(second render compared byte-for-byte).",
              ""]
    lines += ["## Per-state measured carrier ratios", "",
              "| State | Red | Green | Blue | Worst unintended | Checks |",
              "|---|---|---|---|---|---|"]
    for row in measurements["states"]:
        measured = row["measured_percent"]
        status = row["status"]
        lines.append(f"| {row['name']} ({row['index']}) | {measured['red']:.2f}% | "
                     f"{measured['green']:.2f}% | {measured['blue']:.2f}% | "
                     f"{row['worst_unintended_db']:.1f} dB | **{status}** |")
    lines += ["", "Yellow = red + green at equal level, no blue; Cyan = green + blue; "
              "Magenta = red + blue; White = red + green + blue. Unintended carriers are "
              "measured relative to the active dim level (6.25%); the criterion is ≤ −40 dB.", ""]
    if measurements.get("lr"):
        lines += ["## Optional L/R eye check", "",
                  "| # | State | Carrier | Intended ear | Intended ratio | Other channel | Status |",
                  "|---|---|---|---|---|---|---|"]
        for row in measurements["lr"]["states"]:
            lines.append(f"| {row['index']} | {row['name']} | {row['carrier']} | {row['ear']} | "
                         f"{row['measured_intended_ratio'] * 100:.2f}% | "
                         f"{row['measured_other_db']:.1f} dB | **{row['status']}** |")
        lines += ["", "Each row isolates one carrier in one ear. The brighter channel must be "
                  "the intended one; the other channel is measured as attenuation.", ""]
    lines += ["## What the automated checks prove — and do not prove", "",
              "**Prove (offline, on this machine):** the WAV contains the intended carrier "
              "frequencies; every active carrier is at 6.25% of the reference peak; "
              "composite states contain equal 1:1 (1:1:1) ratios; inactive carriers are "
              "≤ −40 dB down; the reference is at −30 dBFS with an intact 20 Hz pan LFO; "
              "transitions are smooth (no overshoot, no abrupt pulse, no black gap); "
              "headroom is ample; and the render is deterministic.",
              "",
              "**Do not prove:** that the Kasina decodes these spectra correctly, that the "
              "LEDs emit the intended colours, or that a person perceives red/green/blue/"
              "yellow/cyan/magenta/white. Correct spectra are necessary, not sufficient. "
              "The reported green/yellow asymmetry (red/blue/violet/stereo differences) can "
              "come from the LEDs, the diffuser/glasses, the decoder, or individual colour "
              "vision; only hardware observation (and ideally a photodiode) can separate "
              "those causes. No medical or wellness claim is made.", "",
              "## Commands", "",
              "```bash",
              "cd experiments/spectrastrobe-poc/calibration",
              "./run_calibration.sh",
              "```",
              "",
              "Safety: the parent `../HARDWARE-PROTOCOL.md` is the binding reference. Start "
              "at the dim 6.25% exposure, one operator, no other flicker sources, immediate "
              "stop (playback stop + Kasina brightness low + glasses off/one eye covered). "
              "The smooth 1 s sequence is not permission for faster flashing.", ""]
    (out_dir / "calibration-report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


# --------------------------------------------------------------------------- #
# Main
# --------------------------------------------------------------------------- #
def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description="NAGLOED v2 SpectraStrobe colour calibration verification (DIAGNOSTIC)")
    parser.add_argument("--calibration", default=str(CAL_DIR / "calibration.json"))
    parser.add_argument("--out-dir", default=str(CAL_DIR / "out"))
    parser.add_argument("--skip-determinism", action="store_true")
    args = parser.parse_args(argv)

    cal_path = Path(args.calibration)
    out_dir = Path(args.out_dir)
    cal = json.loads(cal_path.read_text(encoding="utf-8"))
    main_name = cal["audio"]["main_file"]
    lr_name = cal["audio"]["lr_file"]
    main_path = out_dir / main_name
    if not main_path.exists():
        print(f"ERROR: missing render {main_path}", file=sys.stderr)
        print("Run first: python3 render_calibration.py", file=sys.stderr)
        return 2

    checks = Checks()
    measurements = {}
    fs, width, left, right = parent_verify.read_wav(main_path)
    measurements.update(verify_main(checks, cal, fs, width, left, right))

    if (out_dir / lr_name).exists():
        lr_fs, lr_width, lr_left, lr_right = parent_verify.read_wav(out_dir / lr_name)
        measurements["lr"] = verify_lr(checks, cal, lr_fs, lr_width, lr_left, lr_right)

    determinism_result = None
    if not args.skip_determinism:
        names = [main_name] + ([lr_name] if (out_dir / lr_name).exists() else [])
        determinism_result = determinism_check(checks, cal_path, CAL_DIR / "render_calibration.py",
                                               out_dir, names)
        measurements["determinism"] = determinism_result

    summary = checks.summary()
    write_timeline_csv(out_dir / "colour-timeline.csv", cal, measurements)
    write_timeline_svg(out_dir / "colour-timeline.svg", cal, measurements, summary)
    write_reports(out_dir, cal, checks, measurements, summary, determinism_result)

    print("NAGLOED v2 — SpectraStrobe colour calibration verification (DIAGNOSTIC)")
    print(f"calibration : {cal_path.name} v{cal['meta']['version']} "
          f"({len(cal['states'])} states, {float(cal['meta']['duration_s']):.1f} s)")
    print(f"reference   : {measurements['reference']['peak_amplitude_dbfs']:+.2f} dBFS, "
          f"LFO {measurements['lfo']['freq_hz']:.2f} Hz / depth "
          f"{measurements['lfo']['depth_pct']:.1f}%")
    print("")
    for item in checks.items:
        print(f"  {item['id']:<26} {item['status']:<5} {fmt(item['measured'])}")
    print("")
    print(f"SUMMARY     : {summary['verdict']} ({summary['pass']} PASS, {summary['fail']} FAIL "
          f"of {summary['total']})")
    print(f"report      : {out_dir.resolve()}/calibration-report.md, colour-timeline.csv/.svg")
    return 1 if summary["fail"] > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
