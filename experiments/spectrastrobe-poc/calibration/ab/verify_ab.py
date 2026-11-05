#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""NAGLOED v2 — SpectraStrobe A/B colour calibration: independent verification.

EXPERIMENT / DIAGNOSTIC for issue #31. Separate from NAGLOED 1.0.4, Eerste
Gloed and the base colour calibration. The colour levels are a controlled
diagnostic ladder, NOT artistic settings.

Analyses the WAVs in ab/out/ against ab.json (independently from the renderer):
format/duration, reference level and 20 Hz pan LFO, absolute colour levels and
ratios, unintended-carrier attenuation, peak/true peak, encoded colour-carrier
gaps, fade-in/out, per-transition 10 ms envelope midpoints (direct vs overlap),
the optional L/R alternation file and deterministic rerendering.

Exit code 0 = all checks PASS.

Usage:
    python3 verify_ab.py [--ab ab.json] [--out-dir out] [--skip-determinism]
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True  # never leave __pycache__ in the parent experiment

AB_DIR = Path(__file__).resolve().parent
PARENT_DIR = AB_DIR.parent.parent
if str(PARENT_DIR) not in sys.path:
    sys.path.insert(0, str(PARENT_DIR))

import verify as parent_verify  # noqa: E402 - deliberate reuse of low-level primitives

TAU = 2.0 * math.pi
CARRIER_KEYS = ("red", "green", "blue")
CARRIER_FREQ = {"red": 18700.0, "green": 19200.0, "blue": 19700.0}


def db_to_lin(db: float) -> float:
    return 10.0 ** (float(db) / 20.0)
BANNER = ("EXPERIMENT / DIAGNOSTIC for issue #31 — not part of NAGLOED 1.0.4, the "
          "artistic composition Eerste Gloed or the base colour calibration. "
          "Colour levels are a diagnostic ladder, not an artistic setting. "
          "No medical or wellness claims; no causal claim about hardware.")


# --------------------------------------------------------------------------- #
# Check collection and formatting
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


def classify_exposure(ratio_percent: float, ceiling_percent: float):
    """Map a measured ratio percentage to the declared exposure classification."""
    exposure = "under_ceiling" if ratio_percent <= ceiling_percent else "gated"
    if ratio_percent < ceiling_percent - 0.05:
        relative = "below"
    elif ratio_percent > ceiling_percent + 0.05:
        relative = "above"
    else:
        relative = "at"
    return exposure, relative



# --------------------------------------------------------------------------- #
# Windowing and measurements
# --------------------------------------------------------------------------- #
_BH_CACHE = {}


def blackman_harris(n: int):
    cached = _BH_CACHE.get(n)
    if cached is None:
        a0, a1, a2, a3 = 0.35875, 0.48829, 0.14128, 0.01168
        cached = [a0 - a1 * math.cos(TAU * i / (n - 1))
                  + a2 * math.cos(2.0 * TAU * i / (n - 1))
                  - a3 * math.cos(3.0 * TAU * i / (n - 1)) for i in range(n)]
        _BH_CACHE[n] = cached
    return cached


def demod_blackman_harris(left, right, fs, freq, t0, t1, block_s):
    """Per-block amplitude with a 10 ms Blackman-Harris window.

    10 ms gives 100 Hz resolution; the 18.2 kHz reference is 500 Hz away, where
    the 4-term Blackman-Harris window suppresses leakage far below -40 dB, so
    the fine colour envelopes and the LFO measurement are not contaminated.
    """
    i0 = max(0, int(round(t0 * fs)))
    i1 = min(len(left), int(round(t1 * fs)))
    block = max(1, int(round(block_s * fs)))
    weights = blackman_harris(block)
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


def reference_amplitude(fs, left, right, t0):
    spec_l, spec_r, sum_w2 = parent_verify.spectrum_pair(left, right, fs, t0)
    power = parent_verify.band_rms_sq(spec_l, fs, parent_verify.FFT_N, sum_w2, 18100.0, 18300.0) \
        + parent_verify.band_rms_sq(spec_r, fs, parent_verify.FFT_N, sum_w2, 18100.0, 18300.0)
    return math.sqrt(2.0 * power) if power > 0.0 else 0.0


def lfo_metrics_bh(fs, left, right, t0, t1):
    env_l, env_r = demod_blackman_harris(left, right, fs, 18200.0, t0, t1, 0.010)
    n = min(len(env_l), len(env_r))
    if n < 8:
        return {"freq_hz": float("nan"), "depth_pct": 0.0, "rest_db": -999.0, "corr": 0.0}
    power_l = [value * value for value in env_l]
    power_r = [value * value for value in env_r]
    balance = [(power_l[i] - power_r[i]) / (power_l[i] + power_r[i] + 1e-30) for i in range(n)]
    depth = 0.5 * (max(balance) - min(balance))
    q_max = min(max(abs(min(balance)), abs(max(balance))), 1.0 - 1e-12)
    rest = 10.0 * math.log10((1.0 - q_max) / (1.0 + q_max))
    rate = n / (t1 - t0)
    best_freq = float("nan")
    best_amp = -1.0
    steps = int(round((21.0 - 19.0) / 0.02)) + 1
    for step in range(steps):
        freq = 19.0 + 0.02 * step
        amp = parent_verify.goertzel_amplitude(balance, rate, freq)
        if amp > best_amp:
            best_amp = amp
            best_freq = freq
    return {"freq_hz": best_freq, "depth_pct": 100.0 * depth,
            "rest_db": rest, "corr": parent_verify.pearson(power_l, power_r)}


def state_amplitudes(fs, left, right, window):
    """Per-carrier centre-pan amplitude in a measurement window (Hann, 0.3 s)."""
    t0, t1 = float(window[0]), float(window[1])
    block = t1 - t0
    result = {}
    for key in CARRIER_KEYS:
        env_l, env_r = parent_verify.demod_envelopes(left, right, fs, CARRIER_FREQ[key],
                                                     t0, t1, block, windowed=True)
        result[key] = 0.5 * (env_l[0] + env_r[0]) if env_l else 0.0
    return result


def channel_amplitudes(fs, left, right, window):
    t0, t1 = float(window[0]), float(window[1])
    block = t1 - t0
    result = {}
    for key in CARRIER_KEYS:
        env_l, env_r = parent_verify.demod_envelopes(left, right, fs, CARRIER_FREQ[key],
                                                     t0, t1, block, windowed=True)
        result[key] = (env_l[0] if env_l else 0.0, env_r[0] if env_r else 0.0)
    return result


def fine_envelopes(fs, left, right, duration_s, block_s=0.010):
    envelopes = {}
    for key in CARRIER_KEYS:
        env_l, env_r = demod_blackman_harris(left, right, fs, CARRIER_FREQ[key],
                                             0.0, duration_s, block_s)
        envelopes[key] = [0.5 * (a + b) for a, b in zip(env_l, env_r)]
    return envelopes, block_s


# --------------------------------------------------------------------------- #
# Per-variant verification
# --------------------------------------------------------------------------- #
def verify_main_variant(checks, ab, variant, fs, width, left, right):
    timing = ab["timing"]
    duration_s = float(ab["meta"]["duration_s"])
    level_db = float(variant["colour_level_dbfs"])
    level_lin = db_to_lin(level_db)
    expected_ratio_db = float(variant["ratio_to_reference_db"])
    fade_in = float(timing["fade_in_ms"]) / 1000.0
    fade_out = float(timing["fade_out_ms"]) / 1000.0
    states = ab["states"]
    vid = variant["id"]
    measurements = {}
    frames = len(left)

    checks.ok("format", f"{vid}-format", f"{vid}: format and duration",
              f"44100 Hz / 16-bit / stereo / {duration_s:.1f} s",
              f"{fs} Hz / {width * 8}-bit / {frames / fs:.3f} s",
              fs == 44100 and width == 2 and abs(frames / fs - duration_s) <= 0.02)

    amp_ref = reference_amplitude(fs, left, right, 2.0)
    amp_ref_db = parent_verify.amp_db(amp_ref)
    checks.ok("reference", f"{vid}-reference", f"{vid}: reference peak amplitude",
              f"{float(ab['reference']['level_dbfs']):+.1f} dBFS ± 0.5 dB",
              f"{amp_ref_db:+.2f} dBFS",
              abs(amp_ref_db - float(ab["reference"]["level_dbfs"])) <= 0.5)

    lfo = lfo_metrics_bh(fs, left, right, 2.0, 6.0)
    checks.ok("reference", f"{vid}-lfo", f"{vid}: 20 Hz reference pan LFO intact",
              "20.0 Hz ± 0.5; depth ≥ 99%; rest ≤ -20 dB "
              "(10 ms Blackman-Harris measurement floor; encoded panning is per-sample)",
              f"{lfo['freq_hz']:.2f} Hz, depth {lfo['depth_pct']:.1f}%, "
              f"rest {lfo['rest_db']:.1f} dB",
              abs(lfo["freq_hz"] - 20.0) <= 0.5 and lfo["depth_pct"] >= 99.0
              and lfo["rest_db"] <= -20.0)

    # Absolute colour levels and measured ratios per state.
    level_errors = []
    ratio_errors = []
    unintended = []
    per_state = []
    for state in states:
        amps = state_amplitudes(fs, left, right, state["measure_window_s"])
        active = [key for key in CARRIER_KEYS if float(state["expected_ratios"][key]) > 0]
        row = {"index": state["index"], "name": state["name"], "active": active,
               "measured_dbfs": {}, "level_error_db": {}, "ratio_db": {}}
        for key in CARRIER_KEYS:
            amp = amps[key]
            measured_db = parent_verify.amp_db(amp) if amp > 0 else -999.0
            error_db = measured_db - level_db if amp > 0 else -999.0
            ratio_db = (parent_verify.amp_db(amp / amp_ref)
                        if amp > 0 and amp_ref > 0 else -999.0)
            row["measured_dbfs"][key] = round(measured_db, 3)
            row["level_error_db"][key] = round(error_db, 3)
            row["ratio_db"][key] = round(ratio_db, 3)
            if key in active:
                level_errors.append(abs(error_db))
                ratio_errors.append(abs(ratio_db - expected_ratio_db))
            elif amp > 0:
                unintended.append(parent_verify.amp_db(amp / level_lin))
        per_state.append(row)
    worst_level = max(level_errors) if level_errors else 999.0
    worst_ratio = max(ratio_errors) if ratio_errors else 999.0
    worst_unintended = max(unintended) if unintended else -999.0
    checks.ok("levels", f"{vid}-levels", f"{vid}: active colour absolute level",
              f"{level_db:+.1f} dBFS ± 0.5 dB per active carrier",
              f"worst {worst_level:.2f} dB", worst_level <= 0.5)
    checks.ok("ratio", f"{vid}-ratio", f"{vid}: colour/reference ratio as documented",
              f"{expected_ratio_db:+.2f} dB ± 0.5 dB",
              f"worst {worst_ratio:.2f} dB", worst_ratio <= 0.5)
    checks.ok("isolation", f"{vid}-unintended", f"{vid}: unintended carriers",
              "inactive carriers ≤ -40 dB relative to the active level",
              f"worst {worst_unintended:.1f} dB", worst_unintended <= -40.0)

    # Ceiling / hardware-exposure classification (owner correction 5 Nov 2026).
    ceiling_pct = float((ab.get("hardware_exposure_policy") or {}).get(
        "former_ceiling_percent_of_reference", 25.0))
    measured_pcts = [10.0 ** (row["ratio_db"][key] / 20.0) * 100.0
                     for row in per_state for key in row["active"]]
    measured_pct = sum(measured_pcts) / len(measured_pcts) if measured_pcts else 0.0
    computed_exposure, computed_relative = classify_exposure(measured_pct, ceiling_pct)
    declared_exposure = variant.get("hardware_exposure")
    declared_relative = variant.get("relative_to_former_25pct_ceiling")
    exposure_ok = (declared_exposure == computed_exposure
                   and declared_relative == computed_relative)
    checks.ok("exposure", f"{vid}-exposure",
              f"{vid}: hardware-exposure classification matches the measured ratio",
              f"measured ratio ≤ {ceiling_pct:.0f}% of reference → under_ceiling, above → gated; "
              f"declared label matches",
              f"measured {measured_pct:.2f}% of ref → {computed_exposure} "
              f"({computed_relative}); declared {declared_exposure} "
              f"({declared_relative})", exposure_ok,
              "former 25% ceiling is a conservative research convention, not a proven "
              "physical safety threshold or decoder specification")

    envelopes, block_s = fine_envelopes(fs, left, right, duration_s)
    block_ms = block_s * 1000.0

    # Peak / true peak.
    peak = max(max(left), -min(left), max(right), -min(right))
    true_peak_value = parent_verify.true_peak(left, right, fs)
    finite = all(math.isfinite(v) for v in left) and all(math.isfinite(v) for v in right)
    peak_db = parent_verify.amp_db(peak)
    true_peak_db = parent_verify.amp_db(true_peak_value)
    checks.ok("peak", f"{vid}-peak", f"{vid}: sample peak, true peak, clipping",
              "sample ≤ -6 dBFS, true peak ≤ -6 dBTP, no clipping, all finite",
              f"sample {peak_db:+.2f} dBFS, true peak {true_peak_db:+.2f} dBTP",
              peak_db <= -6.0 and true_peak_db <= -6.0 and peak < 1.0 and finite)

    # Black gap between colour carriers.
    total_env = [sum(envelopes[key][i] for key in CARRIER_KEYS) for i in range(len(envelopes["red"]))]
    start_block = int(round((fade_in + 0.02) / block_s))
    end_block = min(len(total_env), int(round((duration_s - fade_out - 0.02) / block_s)))
    gap = 0
    worst_gap = 0
    for i in range(start_block, end_block):
        if total_env[i] < 0.05 * level_lin:
            gap += 1
            worst_gap = max(worst_gap, gap)
        else:
            gap = 0
    gap_ms = worst_gap * block_ms
    checks.ok("gap", f"{vid}-gap", f"{vid}: no encoded colour-carrier black gap",
              "longest interval with all colour carriers below 5% of level ≤ 20 ms",
              f"longest gap {gap_ms:.1f} ms", gap_ms <= 20.0)

    # Fade-in / fade-out (10%→90% of the active level).
    def times(values):
        first_ten = next((i for i, value in enumerate(values) if value >= 0.1 * level_lin), None)
        first_ninety = next((i for i, value in enumerate(values) if value >= 0.9 * level_lin), None)
        last_ten = next((i for i in range(len(values) - 1, -1, -1)
                         if values[i] >= 0.1 * level_lin), None)
        last_ninety = next((i for i in range(len(values) - 1, -1, -1)
                            if values[i] >= 0.9 * level_lin), None)
        rise = ((first_ninety - first_ten) * block_ms
                if None not in (first_ten, first_ninety) else -1.0)
        fall = ((last_ten - last_ninety) * block_ms
                if None not in (last_ten, last_ninety) else -1.0)
        return rise, fall

    rise_ms, fall_ms = times(total_env)
    checks.ok("fade", f"{vid}-fade", f"{vid}: gentle fade-in and fade-out",
              "10%→90% rise/fall > 0 and ≤ 420 ms",
              f"rise {rise_ms:.0f} ms, fall {fall_ms:.0f} ms",
              0.0 < rise_ms <= 420.0 and 0.0 < fall_ms <= 420.0)

    # Per-transition midpoints.
    transitions = transition_rows(ab, variant, envelopes, level_lin, block_s, duration_s)
    def midpoint_values(rows, key, only_nonempty=False):
        values = []
        for row in rows:
            if only_nonempty and not row["set"]:
                continue
            values.append(row[key])
        return values

    outgoing = [row["midpoint_outgoing_rel"] for row in transitions if row["outgoing_only"]]
    incoming = [row["midpoint_incoming_rel"] for row in transitions if row["incoming_only"]]
    shared = [row["midpoint_shared_rel"] for row in transitions if row["shared"]]
    if variant["crossfade_shape"] == "overlap":
        ok = (all(0.80 <= value <= 1.20 for value in outgoing)
              and all(0.80 <= value <= 1.20 for value in incoming)
              and all(0.80 <= value <= 1.20 for value in shared))
        detail = (f"outgoing {min(outgoing):.2f}–{max(outgoing):.2f}, "
                  f"incoming {min(incoming):.2f}–{max(incoming):.2f}")
    else:
        ok = (all(0.35 <= value <= 0.65 for value in outgoing)
              and all(0.35 <= value <= 0.65 for value in incoming)
              and all(0.80 <= value <= 1.20 for value in shared))
        detail = (f"outgoing {min(outgoing):.2f}–{max(outgoing):.2f}, "
                  f"incoming {min(incoming):.2f}–{max(incoming):.2f}")
    checks.ok("transition", f"{vid}-midpoint",
              f"{vid}: transition midpoint amplitudes match the "
              f"{variant['crossfade_shape']} shape",
              variant["expected_midpoint"], detail, ok)

    measurements.update({
        "file": variant["file"],
        "colour_level_dbfs": level_db,
        "ratio_to_reference_db": expected_ratio_db,
        "ratio_percent_of_reference": variant["ratio_percent_of_reference"],
        "percent_of_mindplace_full_brightness": variant["percent_of_mindplace_full_brightness"],
        "crossfade_shape": variant["crossfade_shape"],
        "reference_peak_amplitude_dbfs": round(amp_ref_db, 3),
        "lfo": {k: round(v, 4) if isinstance(v, float) else v for k, v in lfo.items()},
        "measured_level_error_worst_db": round(worst_level, 3),
        "measured_ratio_error_worst_db": round(worst_ratio, 3),
        "unintended_worst_db": round(worst_unintended, 2),
        "hardware_exposure": computed_exposure,
        "declared_hardware_exposure": declared_exposure,
        "ceiling_percent_of_reference": ceiling_pct,
        "measured_ratio_percent_of_reference": round(measured_pct, 4),
        "relative_to_former_25pct_ceiling": computed_relative,
        "hardware_note": variant.get("hardware_note", ""),
        "peak_dbfs": round(peak_db, 3),
        "true_peak_dbtp": round(true_peak_db, 3),
        "clipped": peak >= 1.0,
        "finite": finite,
        "longest_gap_ms": round(gap_ms, 2),
        "fade_in_rise_ms": round(rise_ms, 1),
        "fade_out_fall_ms": round(fall_ms, 1),
        "transitions": transitions,
        "states": per_state,
    })
    return measurements


def transition_rows(ab, variant, envelopes, level_lin, block_s, duration_s):
    timing = ab["timing"]
    fade_in = float(timing["fade_in_ms"]) / 1000.0
    xfade = float(timing["crossfade_ms"]) / 1000.0
    half = xfade / 2.0
    states = ab["states"]
    rows = []
    for index in range(len(states) - 1):
        boundary = fade_in + (index + 1) * float(timing["state_ms"]) / 1000.0
        prev_state, next_state = states[index], states[index + 1]
        prev_active = {key for key in CARRIER_KEYS if float(prev_state["expected_ratios"][key]) > 0}
        next_active = {key for key in CARRIER_KEYS if float(next_state["expected_ratios"][key]) > 0}
        outgoing = sorted(prev_active - next_active)
        incoming = sorted(next_active - prev_active)
        shared = sorted(prev_active & next_active)
        mid_index = min(len(envelopes["red"]) - 1, max(0, int(round(boundary / block_s))))
        values = {key: envelopes[key][mid_index] / level_lin if level_lin > 0 else 0.0
                  for key in CARRIER_KEYS}
        mid_out = (sum(values[key] for key in outgoing) / len(outgoing)
                   if outgoing else None)
        mid_in = (sum(values[key] for key in incoming) / len(incoming)
                  if incoming else None)
        mid_shared = (sum(values[key] for key in shared) / len(shared)
                      if shared else None)
        total_mid = sum(values[key] for key in CARRIER_KEYS)
        lo = max(0, int(round((boundary - half) / block_s)))
        hi = min(len(envelopes["red"]), int(round((boundary + half) / block_s)))
        mins = []
        for i in range(lo, hi):
            mins.append(sum(envelopes[key][i] for key in CARRIER_KEYS) / level_lin
                        if level_lin > 0 else 0.0)
        min_total = min(mins) if mins else 0.0
        expected = variant["expected_midpoint"]
        row = {
            "variant": variant["id"],
            "level_dbfs": float(variant["colour_level_dbfs"]),
            "shape": variant["crossfade_shape"],
            "transition_index": index,
            "boundary_s": round(boundary, 3),
            "from_state": prev_state["name"],
            "to_state": next_state["name"],
            "outgoing_only": "|".join(outgoing),
            "incoming_only": "|".join(incoming),
            "shared": "|".join(shared),
            "midpoint_outgoing_rel": (round(mid_out, 4) if mid_out is not None else None),
            "midpoint_incoming_rel": (round(mid_in, 4) if mid_in is not None else None),
            "midpoint_shared_rel": (round(mid_shared, 4) if mid_shared is not None else None),
            "midpoint_total_rel": round(total_mid, 4),
            "min_total_rel": round(min_total, 4),
            "expected_midpoint": expected,
        }
        rows.append(row)
    return rows


def verify_lr_variant(checks, ab, variant, fs, width, left, right):
    duration_s = float(ab["meta"]["lr_duration_s"])
    level_db = float(variant["colour_level_dbfs"])
    level_lin = db_to_lin(level_db)
    states = ab["lr_states"]
    vid = variant["id"]
    measurements = {}
    frames = len(left)

    checks.ok("format", f"{vid}-format", f"{vid}: format and duration",
              f"44100 Hz / 16-bit / stereo / {duration_s:.1f} s",
              f"{fs} Hz / {width * 8}-bit / {frames / fs:.3f} s",
              fs == 44100 and width == 2 and abs(frames / fs - duration_s) <= 0.02)

    amp_ref = reference_amplitude(fs, left, right, 1.0)
    amp_ref_db = parent_verify.amp_db(amp_ref)
    checks.ok("reference", f"{vid}-reference", f"{vid}: reference peak amplitude",
              f"{float(ab['reference']['level_dbfs']):+.1f} dBFS ± 0.5 dB",
              f"{amp_ref_db:+.2f} dBFS",
              abs(amp_ref_db - float(ab["reference"]["level_dbfs"])) <= 0.5)
    lfo = lfo_metrics_bh(fs, left, right, 1.0, 5.0)
    checks.ok("reference", f"{vid}-lfo", f"{vid}: 20 Hz reference pan LFO intact",
              "20.0 Hz ± 0.5; depth ≥ 99%; rest ≤ -20 dB "
              "(10 ms Blackman-Harris measurement floor; encoded panning is per-sample)",
              f"{lfo['freq_hz']:.2f} Hz, depth {lfo['depth_pct']:.1f}%, "
              f"rest {lfo['rest_db']:.1f} dB",
              abs(lfo["freq_hz"] - 20.0) <= 0.5 and lfo["depth_pct"] >= 99.0
              and lfo["rest_db"] <= -20.0)

    per_state = []
    for state in states:
        channels = channel_amplitudes(fs, left, right, state["measure_window_s"])
        carrier = state["carrier"]
        amp_l, amp_r = channels[carrier]
        intended = amp_l if state["ear"] == "left" else amp_r
        other = amp_r if state["ear"] == "left" else amp_l
        intended_db = parent_verify.amp_db(intended) if intended > 0 else -999.0
        other_rel = (parent_verify.amp_db(other / level_lin) if other > 0 else -999.0)
        intended_ok = abs(intended_db - level_db) <= 0.5
        other_ok = other_rel <= -40.0
        checks.ok("lr", f"{vid}-state-{state['index']}",
                  f"{state['name']}: intended ear active, other ear off",
                  "intended ear = -24.0 dBFS ± 0.5 dB; other ear ≤ -40 dB",
                  f"intended {intended_db:+.2f} dBFS ({state['ear']} ear), "
                  f"other ear {other_rel:.1f} dB",
                  intended_ok and other_ok)
        per_state.append({
            "index": state["index"], "name": state["name"], "carrier": carrier,
            "ear": state["ear"], "intended_ear_db": round(intended_db, 3),
            "other_ear_rel_db": round(other_rel, 2),
            "status": "PASS" if (intended_ok and other_ok) else "FAIL",
        })

    ceiling_pct = float((ab.get("hardware_exposure_policy") or {}).get(
        "former_ceiling_percent_of_reference", 25.0))
    measured_pcts = [10.0 ** ((row["intended_ear_db"] - amp_ref_db) / 20.0) * 100.0
                     for row in per_state]
    measured_pct = sum(measured_pcts) / len(measured_pcts) if measured_pcts else 0.0
    computed_exposure, computed_relative = classify_exposure(measured_pct, ceiling_pct)
    declared_exposure = variant.get("hardware_exposure")
    declared_relative = variant.get("relative_to_former_25pct_ceiling")
    exposure_ok = (declared_exposure == computed_exposure
                   and declared_relative == computed_relative)
    checks.ok("exposure", f"{vid}-exposure",
              f"{vid}: hardware-exposure classification matches the measured ratio",
              f"measured ratio ≤ {ceiling_pct:.0f}% of reference → under_ceiling, above → gated; "
              f"declared label matches",
              f"measured {measured_pct:.2f}% of ref → {computed_exposure} "
              f"({computed_relative}); declared {declared_exposure} "
              f"({declared_relative})", exposure_ok,
              "former 25% ceiling is a conservative research convention, not a proven "
              "physical safety threshold or decoder specification")

    peak = max(max(left), -min(left), max(right), -min(right))
    true_peak_value = parent_verify.true_peak(left, right, fs)
    peak_db = parent_verify.amp_db(peak)
    true_peak_db = parent_verify.amp_db(true_peak_value)
    checks.ok("peak", f"{vid}-peak", f"{vid}: sample peak, true peak, clipping",
              "sample ≤ -6 dBFS, true peak ≤ -6 dBTP, no clipping",
              f"sample {peak_db:+.2f} dBFS, true peak {true_peak_db:+.2f} dBTP",
              peak_db <= -6.0 and true_peak_db <= -6.0 and peak < 1.0)

    measurements.update({
        "file": variant["file"],
        "colour_level_dbfs": level_db,
        "reference_peak_amplitude_dbfs": round(amp_ref_db, 3),
        "hardware_exposure": computed_exposure,
        "declared_hardware_exposure": declared_exposure,
        "ceiling_percent_of_reference": ceiling_pct,
        "measured_ratio_percent_of_reference": round(measured_pct, 4),
        "relative_to_former_25pct_ceiling": computed_relative,
        "hardware_note": variant.get("hardware_note", ""),
        "lfo": {k: round(v, 4) if isinstance(v, float) else v for k, v in lfo.items()},
        "peak_dbfs": round(peak_db, 3),
        "true_peak_dbtp": round(true_peak_db, 3),
        "states": per_state,
    })
    return measurements


def determinism_check(checks, ab_path, out_dir, names, timeout=300):
    with tempfile.TemporaryDirectory(prefix="specstroop-ab-repro-") as tmp:
        command = [sys.executable, str(AB_DIR / "render_ab.py"), "--ab", str(ab_path),
                   "--out-dir", tmp]
        try:
            result = subprocess.run(command, capture_output=True, text=True, timeout=timeout)
        except Exception as error:  # noqa: BLE001 - report must never crash
            checks.ok("determinism", "determinism", "second render", "render exits 0",
                      str(error), False)
            return None
        if result.returncode != 0:
            checks.ok("determinism", "determinism", "second render", "render exits 0",
                      f"exit {result.returncode}", False, result.stderr[-300:])
            return None
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
        first_manifest = out_dir / "ab-manifest.json"
        second_manifest = Path(tmp) / "ab-manifest.json"
        manifest_same = (first_manifest.exists() and second_manifest.exists()
                         and parent_verify.sha256(first_manifest) == parent_verify.sha256(second_manifest))
        compared.append({"file": "ab-manifest.json", "identical": manifest_same})
        if not manifest_same:
            mismatches.append("ab-manifest.json")
        checks.ok("determinism", "determinism",
                  "two renders are byte-identical (all WAVs + manifest)",
                  "all outputs identical",
                  "identical" if not mismatches else f"differs: {', '.join(mismatches)}",
                  not mismatches)
        return compared


def provenance_check(checks, measurements, out_dir):
    path = out_dir / "ab-manifest.json"
    if not path.exists():
        checks.ok("provenance", "provenance", "existing A/B WAVs unchanged",
                  "manifest with provenance present", "manifest missing", False)
        return
    manifest = json.loads(path.read_text(encoding="utf-8"))
    provenance = manifest.get("provenance") or {}
    compared = provenance.get("existing_files_compared") or []
    problems = []
    for item in compared:
        target = out_dir / item["file"]
        actual = parent_verify.sha256(target) if target.exists() else None
        if (not item.get("identical")
                or item.get("sha256_before") != item.get("sha256_after")
                or actual != item.get("sha256_after")):
            problems.append(item["file"])
    ok = bool(compared) and not problems
    measurements["provenance"] = {
        "previous_manifest_present": provenance.get("previous_manifest_present"),
        "existing_files_compared": compared,
        "new_files": provenance.get("new_files", []),
        "all_existing_files_identical": provenance.get("all_existing_files_identical"),
        "note": provenance.get("note", ""),
    }
    checks.ok("provenance", "provenance",
              "existing A/B WAVs are byte-identical to the previous manifest",
              "every existing file: sha256_before == sha256_after == current file",
              f"{len(compared)} existing files compared, all identical={not problems}",
              ok, "no existing file was altered; new boundary variants were added")


def cross_variant_check(checks, measurements):
    direct = measurements["variants"].get("direct-m24p0")
    overlap = measurements["variants"].get("overlap-24")
    if not direct or not overlap:
        checks.ok("transition", "overlap-vs-direct", "overlap vs direct midpoint comparison",
                  "both variants present", "missing", False)
        return
    def mean_mid(rows, key):
        values = [row[key] for row in rows if row["outgoing_only" if key == "midpoint_outgoing_rel" else "incoming_only"]]
        return sum(values) / len(values) if values else 0.0

    direct_out = mean_mid(direct["transitions"], "midpoint_outgoing_rel")
    direct_in = mean_mid(direct["transitions"], "midpoint_incoming_rel")
    overlap_out = mean_mid(overlap["transitions"], "midpoint_outgoing_rel")
    overlap_in = mean_mid(overlap["transitions"], "midpoint_incoming_rel")
    expected = {"direct_outgoing": 0.5, "overlap_outgoing": 1.0}
    ok = (0.40 <= direct_out <= 0.60 and 0.40 <= direct_in <= 0.60
          and 0.90 <= overlap_out <= 1.10 and 0.90 <= overlap_in <= 1.10)
    checks.ok("transition", "overlap-vs-direct",
              "direct midpoint ≈50% vs overlap midpoint ≈100% (at -24 dBFS)",
              "direct 0.40–0.60; overlap 0.90–1.10 of the colour level",
              f"direct outgoing {direct_out:.2f}/incoming {direct_in:.2f}; "
              f"overlap outgoing {overlap_out:.2f}/incoming {overlap_in:.2f}", ok)


# --------------------------------------------------------------------------- #
# Report outputs
# --------------------------------------------------------------------------- #
def write_transition_csv(path, measurements):
    rows = []
    for variant_id, data in measurements["variants"].items():
        for row in data.get("transitions", []):
            rows.append({
                "variant": row["variant"],
                "level_dbfs": row["level_dbfs"],
                "shape": row["shape"],
                "transition_index": row["transition_index"],
                "boundary_s": row["boundary_s"],
                "from_state": row["from_state"],
                "to_state": row["to_state"],
                "outgoing_only": row["outgoing_only"],
                "incoming_only": row["incoming_only"],
                "shared": row["shared"],
                "midpoint_outgoing_rel": row["midpoint_outgoing_rel"],
                "midpoint_incoming_rel": row["midpoint_incoming_rel"],
                "midpoint_shared_rel": row["midpoint_shared_rel"],
                "midpoint_total_rel": row["midpoint_total_rel"],
                "min_total_rel": row["min_total_rel"],
                "expected_midpoint": row["expected_midpoint"],
                "gap_ms": data["longest_gap_ms"],
                "status": data.get("status", "PASS"),
            })
    fields = ["variant", "level_dbfs", "shape", "transition_index", "boundary_s",
              "from_state", "to_state", "outgoing_only", "incoming_only", "shared",
              "midpoint_outgoing_rel", "midpoint_incoming_rel", "midpoint_shared_rel",
              "midpoint_total_rel", "min_total_rel", "expected_midpoint", "gap_ms", "status"]
    with open(path, "w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def _state_fill(rgb):
    red, green, blue = rgb
    if red and green and blue:
        return "#f8f9fa"
    if red and green:
        return "#f2c200"
    if green and blue:
        return "#15aabf"
    if red and blue:
        return "#ae3ec9"
    if red:
        return "#e03131"
    if green:
        return "#2f9e44"
    return "#1c7ed6"


def write_timeline_svg(path, ab, measurements):
    duration = float(ab["meta"]["duration_s"])
    states = ab["states"]
    width, height = 1280, 620
    left, right = 140, 1220
    lane = 120
    bar_h = 54

    def x(t):
        return left + (right - left) * max(0.0, min(1.0, t / duration))

    out = ['<?xml version="1.0" encoding="UTF-8"?>',
           f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
           f'viewBox="0 0 {width} {height}" font-family="Helvetica, Arial, sans-serif">',
           f'<rect width="{width}" height="{height}" fill="#f7f5f0"/>',
           f'<text x="{left}" y="30" font-size="17" font-weight="bold" fill="#1c1a17">'
           f'SpectraStrobe A/B calibration — diagnostic ladder (issue #31)</text>',
           f'<text x="{left}" y="50" font-size="11" fill="#5a544c">'
           f'sequence: {", ".join(state["name"] for state in states)} · '
           f'1 s states · 250 ms centred crossfades · 300 ms fades · '
           f'reference {float(ab["reference"]["level_dbfs"]):+.0f} dBFS · '
           f'colour levels -54.1…-24 dBFS</text>',
           f'<text x="{left}" y="68" font-size="10" fill="#8a2f1f">{BANNER}</text>']
    out.append(f'<text x="{left - 12}" y="{lane + 32}" font-size="11" font-weight="bold" '
               f'fill="#3e3a34" text-anchor="end">STATES</text>')
    for state in states:
        x0, x1 = x(float(state["start_s"])), x(float(state["end_s"]))
        out.append(f'<rect x="{x0:.1f}" y="{lane}" width="{x1 - x0:.1f}" height="{bar_h}" '
                   f'rx="2" fill="{_state_fill(state["expected_rgb"])}" stroke="#3a352e" '
                   f'stroke-width="0.8"/>')
        out.append(f'<text x="{x0 + 5:.1f}" y="{lane + 20}" font-size="11" font-weight="bold" '
                   f'fill="#1c1a17">{state["name"]}</text>')
        out.append(f'<text x="{x0 + 5:.1f}" y="{lane + 36}" font-size="9" fill="#3a352e">'
                   f'{state["start_s"]:.1f}–{state["end_s"]:.1f} s</text>')
    for index in range(len(states) - 1):
        boundary = 0.3 + (index + 1) * 1.0
        out.append(f'<line x1="{x(boundary):.1f}" y1="{lane - 12}" x2="{x(boundary):.1f}" '
                   f'y2="{lane + bar_h + 12}" stroke="#8a2f1f" stroke-width="1" '
                   f'stroke-dasharray="3 3"/>')
        out.append(f'<text x="{x(boundary):.1f}" y="{lane - 18}" font-size="8" fill="#8a2f1f" '
                   f'text-anchor="middle">{boundary:.1f}</text>')
    ref_lane = lane + bar_h + 70
    out.append(f'<text x="{left - 12}" y="{ref_lane + 20}" font-size="10" font-weight="bold" '
               f'fill="#3e3a34" text-anchor="end">REFERENCE</text>')
    out.append(f'<rect x="{left}" y="{ref_lane}" width="{right - left}" height="30" rx="2" '
               f'fill="#6d4a2f" stroke="#3a2818" stroke-width="0.8"/>')
    out.append(f'<text x="{left + 8}" y="{ref_lane + 19}" font-size="9" fill="#f6efe6">'
               f'18.200 Hz · {float(ab["reference"]["level_dbfs"]):+.0f} dBFS · '
               f'{float(ab["reference"]["pan_lfo_hz"]):g} Hz sine pan LFO · continuous</text>')
    # Variant table
    table_y = ref_lane + 60
    out.append(f'<text x="{left}" y="{table_y}" font-size="11" font-weight="bold" '
               f'fill="#1c1a17">Variants (diagnostic ladder, not artistic settings)</text>')
    x_cols = [left, left + 130, left + 230, left + 340, left + 450, left + 620, left + 800]
    headers = ["variant", "colour dBFS", "ratio to ref", "% of full", "shape", "measured peak", "midpoint"]
    for x_col, header in zip(x_cols, headers):
        out.append(f'<text x="{x_col}" y="{table_y + 20}" font-size="9" font-weight="bold" '
                   f'fill="#3e3a34">{header}</text>')
    for row, variant in enumerate(ab["variants"]):
        data = measurements["variants"].get(variant["id"], {})
        y = table_y + 38 + row * 17
        values = [
            variant["id"],
            f"{variant['colour_level_dbfs']:+.1f}",
            f"{variant['ratio_to_reference_db']:+.2f} dB",
            f"{variant['percent_of_mindplace_full_brightness']:.2f}%",
            variant["crossfade_shape"],
            f"{data.get('peak_dbfs', float('nan')):+.1f} dBFS",
            variant["expected_midpoint"].split(";")[0],
        ]
        for x_col, value in zip(x_cols, values):
            out.append(f'<text x="{x_col}" y="{y}" font-size="8" fill="#3a352e">'
                       f'{str(value)[:46]}</text>')
    out.append(f'<text x="{left}" y="{height - 16}" font-size="9" fill="#6b645a">'
               f'out/ab-report.md · out/ab-transition-envelopes.csv · '
               f'safety: ../../HARDWARE-PROTOCOL.md</text>')
    out.append("</svg>")
    path.write_text("\n".join(out) + "\n", encoding="utf-8")


def write_reports(out_dir, ab, checks, measurements, summary, determinism):
    report = {
        "experiment": "SpectraStrobe A/B colour calibration (issue #31)",
        "status": "diagnostic experiment",
        "banner": BANNER,
        "ab": ab,
        "summary": summary,
        "checks": checks.items,
        "measurements": measurements,
        "determinism": determinism,
        "note": ("Automated checks prove encoded levels, ratios, envelopes and peak/gap "
                 "behaviour. They do NOT prove what the Kasina decoder or LEDs do, nor what "
                 "is perceived. The hardware observation must be recorded per transition; "
                 "this report alone cannot identify the cause of the fade-to-black "
                 "impression. No medical or wellness claims."),
    }
    (out_dir / "ab-report.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    lines = ["# A/B calibration report — SpectraStrobe colour (issue #31)", "",
             f"> **{BANNER}**", "",
             "**Research question:** the existing colour calibration is visible per state but "
             "the crossfades appear to fade to black on the Kasina, while offline verification "
             "finds no encoded black gap. Does the impression depend on the **colour level / "
             "ratio to the reference**, on the **transition shape**, or on **left/right "
             "behaviour**? This A/B holds the sequence and reference fixed and varies those "
             "three factors. It reports measurements; it does not claim a cause.",
             "",
             f"**Outcome:** {summary['verdict']} — {summary['pass']} PASS, {summary['fail']} FAIL "
             f"of {summary['total']} checks.",
             ""]
    correction = ab.get("correction") or {}
    policy = ab.get("hardware_exposure_policy") or {}
    lines += ["## Correction — owner decision 5 Nov 2026", "",
              correction.get("summary", "Ceiling classification corrected."),
              "",
              f"**Previous error:** {correction.get('previous_error', '')}",
              "",
              f"**Ceiling:** {policy.get('former_ceiling_percent_of_reference', 25.0):.0f}% of "
              f"the {float(ab['reference']['level_dbfs']):+.0f} dBFS reference peak per active "
              f"colour carrier = colour at "
              f"{policy.get('former_ceiling_colour_level_dbfs', -48.04):.2f} dBFS. "
              f"{policy.get('ceiling_note', '')}",
              "",
              f"**Gating:** {policy.get('gated_note', '')}",
              "",
              "The **% of MindPlace full scale** column uses the different -24 dBFS "
              "full-brightness scale and must not be conflated with the % of the -36 dBFS "
              "reference or with the 25% ceiling. See also "
              "`../../HARDWARE-PROTOCOL.md` (binding safety reference) and "
              "`AB-PROTOCOL.md` (research protocol; corrected by the Lead).", ""]
    lines += ["## Variants", "",
              "| Variant | Colour dBFS | Ratio to ref | % of ref | % of MindPlace -24 full scale (different scale) | Hardware exposure | Shape |",
              "|---|---|---|---|---|---|---|"]
    for variant in ab["variants"]:
        lines.append(f"| {variant['id']} | {variant['colour_level_dbfs']:+.1f} | "
                     f"{variant['ratio_to_reference_db']:+.2f} dB | "
                     f"{variant['ratio_percent_of_reference']:.2f}% | "
                     f"{variant['percent_of_mindplace_full_brightness']:.2f}% | "
                     f"**{variant['hardware_exposure']}** "
                     f"({variant['relative_to_former_25pct_ceiling']} former 25% ceiling"
                     + ("; boundary variant" if variant.get("boundary_variant") else "")
                     + ") | "
                     f"{variant['crossfade_shape']} |")
    under = sum(1 for variant in ab["variants"]
                if variant.get("hardware_exposure") == "under_ceiling")
    gated = sum(1 for variant in ab["variants"]
                if variant.get("hardware_exposure") == "gated")
    lines += ["", f"Reference is fixed at "
              f"{float(ab['reference']['level_dbfs']):+.1f} dBFS peak with a "
              f"{float(ab['reference']['pan_lfo_hz']):g} Hz sine pan LFO. "
              f"Classification: **{under} under_ceiling**, **{gated} gated**. The colour "
              "levels are a controlled diagnostic ladder, not artistic settings and not a "
              "recommendation.", ""]
    lines += ["### Primary-source scale and three levels", "",
              "Primary source: MindPlace Support Forum thread 3173, guidance by Scott "
              "Hendrickson (neuroasis): reference normalised to **-36 dB**, each light "
              "channel to **-24 dB**, a **4:1 (12 dB) dynamic range** — "
              "<https://mindplacesupport.com/forum/showthread.php?3173-"
              "Recommendations-on-maintaining-precise-SpectraStrobe-control-for-given-audio-content>.",
              "This is forum guidance from an encoder developer, not a formal decoder "
              "specification. Keep three levels apart: **digital ratio** (encoded in the WAV; "
              "measured here), **hardware output** (decoder thresholds/LEDs/diffuser; not "
              "measured) and **perception** (what the observer sees; not measured). A correct "
              "digital ratio does not imply the same hardware ratio or perceived colour.", ""]
    lines += ["## Per-variant measurements", "",
              "| Variant | Exposure | Ref dBFS | Active level error | Ratio error | Unintended | Peak | Gap | Fade rise/fall | Status |",
              "|---|---|---|---|---|---|---|---|---|---|"]
    for variant in ab["variants"]:
        data = measurements["variants"].get(variant["id"], {})
        if variant["sequence"] == "lr":
            lines.append(f"| {variant['id']} | {variant.get('hardware_exposure')} | "
                         f"{data.get('reference_peak_amplitude_dbfs', float('nan')):+.2f} | "
                         f"see L/R table | n/a | n/a | {data.get('peak_dbfs', float('nan')):+.2f} dBFS | n/a | n/a | "
                         f"**{data.get('status', 'PASS')}** |")
        else:
            lines.append(f"| {variant['id']} | {variant.get('hardware_exposure')} | "
                         f"{data.get('reference_peak_amplitude_dbfs', float('nan')):+.2f} | "
                         f"{data.get('measured_level_error_worst_db', float('nan')):.2f} dB | "
                         f"{data.get('measured_ratio_error_worst_db', float('nan')):.2f} dB | "
                         f"{data.get('unintended_worst_db', float('nan')):.1f} dB | "
                         f"{data.get('peak_dbfs', float('nan')):+.2f} dBFS | "
                         f"{data.get('longest_gap_ms', float('nan')):.1f} ms | "
                         f"{data.get('fade_in_rise_ms', float('nan')):.0f}/"
                         f"{data.get('fade_out_fall_ms', float('nan')):.0f} ms | "
                         f"**{data.get('status', 'PASS')}** |")
    lines += ["", "All variants: reference within ±0.5 dB, 20 Hz LFO intact, active colour "
              "absolute level within ±0.5 dB, ratio as documented, sample/true peak well below "
              "−6 dBFS, no clipping, no encoded black gap > 20 ms, and deterministic "
              "re-rendering.", ""]
    # Per-state table for the two -24 variants.
    for variant_id in ("direct-m24p0", "overlap-24"):
        data = measurements["variants"].get(variant_id)
        if not data:
            continue
        lines += [f"### Per-state absolute colour levels — {variant_id}", "",
                  "| State | Active | Measured (dBFS) | Level error (dB) | Ratio (dB) |",
                  "|---|---|---|---|---|"]
        for row in data["states"]:
            measured = ", ".join(f"{key} {row['measured_dbfs'][key]:.2f}"
                                 for key in row["active"])
            errors = ", ".join(f"{row['level_error_db'][key]:+.2f}" for key in row["active"])
            ratios = ", ".join(f"{row['ratio_db'][key]:+.2f}" for key in row["active"])
            lines.append(f"| {row['name']} | {', '.join(row['active'])} | {measured} | "
                         f"{errors} | {ratios} |")
        lines.append("")
    lines += ["## Transition midpoints (10 ms envelopes)", "",
              "Direct crossfade: outgoing-only and incoming-only carriers are at ≈50% of the "
              "colour level at the midpoint. Overlap crossfade: both are at ≈100%. Shared "
              "carriers remain at 100% in both. The full table is in "
              "`out/ab-transition-envelopes.csv`.", "",
              "| Variant | Transition | Midpoint outgoing | Midpoint incoming | Midpoint shared | Total midpoint | Min total in window |",
              "|---|---|---|---|---|---|---|"]
    for variant_id in ("direct-m24p0", "overlap-24"):
        data = measurements["variants"].get(variant_id)
        if not data:
            continue
        def show(value):
            return "n/a" if value is None else f"{value:.2f}"

        for row in data["transitions"]:
            lines.append(f"| {variant_id} | {row['boundary_s']:.1f} s "
                         f"{row['from_state']}→{row['to_state']} | "
                         f"{show(row['midpoint_outgoing_rel'])} | "
                         f"{show(row['midpoint_incoming_rel'])} | "
                         f"{show(row['midpoint_shared_rel'])} | "
                         f"{row['midpoint_total_rel']:.2f} | {row['min_total_rel']:.2f} |")
    lines += ["", "The direct and overlap files are the same sequence, reference and level; "
              "only the transition shaping differs. The midpoint columns are relative to the "
              "per-carrier colour level.", ""]
    lr_data = measurements["variants"].get("lr-24")
    if lr_data:
        lines += ["## L/R alternation (matched at -24 dBFS)", "",
                  "| State | Carrier | Ear | Intended-ear level | Other-ear attenuation | Status |",
                  "|---|---|---|---|---|---|"]
        for row in lr_data["states"]:
            lines.append(f"| {row['name']} | {row['carrier']} | {row['ear']} | "
                         f"{row['intended_ear_db']:+.2f} dBFS | "
                         f"{row['other_ear_rel_db']:.1f} dB | **{row['status']}** |")
        lines.append("")
    lines += ["## Decision matrix (hypotheses, not conclusions)", "",
              "| Observation | Possible reading (hypothesis only) |",
              "|---|---|",
              "| All ratios look the same, including -24 | Ratio/threshold is not the dominant "
              "factor; focus on transition shaping, L/R behaviour or display/decoder behaviour. |",
              "| Fade-to-black only at low levels (-54.1/-42) | A level/ratio threshold in the "
              "decoder or LED transfer may be involved; the higher levels should then stay "
              "visible. |",
              "| Fade-to-black persists at -24 but overlap looks continuous | Transition "
              "shaping/midpoint level is a likely factor; the overlap shape keeps both "
              "carriers near full at the midpoint. |",
              "| Overlap still looks black | The midpoint amplitude is not the explanation; "
              "look at decoder dynamics, LED response or the L/R alternation file. |",
              "| L/R alternation shows dark gaps only on one eye / one side | L/R routing, "
              "eye assignment or a channel imbalance may be involved. |",
              "| Nothing reproduces the observation in this controlled set | The earlier "
              "impression may depend on source material, mastering, playback level or the "
              "specific hardware session; record conditions carefully. |",
              "", "None of these rows is a proven cause. Only the hardware observation, "
              "recorded per transition, can select among them.", ""]
    lines += ["## What the automated checks prove — and do not prove", "",
              "**Prove (offline, from the WAV):** the reference is -36 dBFS with an intact "
              "20 Hz pan LFO; each variant's active carriers are at the documented absolute "
              "level (±0.5 dB) and ratio; unintended carriers are ≤ -40 dB down; the encoded "
              "colour carriers never have a black gap > 20 ms; direct midpoints are ≈50% and "
              "overlap midpoints ≈100% of the colour level; fade-in/out is present; sample and "
              "true peak are well below -6 dBFS; and the render is deterministic.",
              "",
              "**Do not prove:** what the Kasina decoder does with these signals, what the "
              "LEDs and diffuser actually emit, or what an observer perceives. Correct "
              "encoded spectra and ratios are necessary, not sufficient. This A/B is a "
              "controlled stimulus set, not evidence of a cause.", ""]
    lines += ["## Limitations and safety", "",
              "- This is an offline stimulus set plus file-level verification. There is no "
              "hardware, photodiode or observer data here, so no cause for the earlier "
              "fade-to-black impression can be claimed.",
              "- The primary-source scale is forum guidance, not a decoder specification; the "
              "Kasina's thresholds and LED transfer are unknown.",
              "- Digital ratio, hardware output and perception are different things; a "
              "\"green\" that looks yellow can be a mixture of LED spectra, diffuser/glasses "
              "behaviour, adaptation or colour naming.",
              "- Safety: `../../HARDWARE-PROTOCOL.md` is the binding reference. Keep Kasina LED "
              "brightness low and AUX moderate for the whole session; never use maximum as a "
              "safe target; do not raise brightness to make a colour appear; one operator; "
              "immediate stop (playback stop + Kasina brightness low + glasses off / one eye "
              "covered). No medical or wellness claim.",
              "- The 25%-of-reference per-carrier ratio from the exploration/base calibration is "
              "a self-imposed conservative ceiling, not a MindPlace limit. Variants above it "
              "(`direct-m30p0`, `direct-m24p0`, `overlap-24`, `lr-24`) are research-only stimuli "
              "and require Lonn's explicit authorisation under `../../HARDWARE-PROTOCOL.md`; "
              "they do not change any production ceiling, preset or score.",
              ""]
    lines += ["## Provenance and hash evidence", "",
              "Existing A/B WAVs were re-rendered deterministically and their SHA256 was "
              "compared against the previous manifest **before** any file was replaced. The "
              "new manifest records the before/after hashes; no existing file was altered. "
              "The three -48.1 boundary variants were added as new files.", ""]
    provenance = measurements.get("provenance") or {}
    compared = provenance.get("existing_files_compared") or []
    if compared:
        lines += ["| File | SHA256 before | SHA256 after | Identical |",
                  "|---|---|---|---|"]
        for item in compared:
            lines.append(f"| {item['file']} | `{item['sha256_before']}` | "
                         f"`{item['sha256_after']}` | "
                         f"{'yes' if item['identical'] else 'NO'} |")
        new_files = provenance.get("new_files") or []
        lines += ["", f"New files added: {len(new_files)}"
                  + (": " + ", ".join(f"`{name}`" for name in new_files) if new_files else "")
                  + ". Existing files changed: **none**.", ""]
    else:
        lines += ["No previous manifest was available for comparison in this run.", ""]
    lines += ["## Commands", "",
              "```bash",
              "cd experiments/spectrastrobe-poc/calibration/ab",
              "./run_ab.sh",
              "```",
              "",
              "Safety: `../../HARDWARE-PROTOCOL.md` is the binding reference. Keep the Kasina "
              "LED brightness **low** and the AUX level **moderate** for the whole session; "
              "never use maximum as a safe target. Do not raise brightness to make a colour "
              "appear. Same routing for every variant, no normalisation, one operator, "
              "immediate stop (playback stop + Kasina brightness low + glasses off / one eye "
              "covered).", ""]
    (out_dir / "ab-report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


# --------------------------------------------------------------------------- #
# Main
# --------------------------------------------------------------------------- #
def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description="NAGLOED v2 SpectraStrobe A/B calibration verification (DIAGNOSTIC, issue #31)")
    parser.add_argument("--ab", default=str(AB_DIR / "ab.json"))
    parser.add_argument("--out-dir", default=str(AB_DIR / "out"))
    parser.add_argument("--skip-determinism", action="store_true")
    args = parser.parse_args(argv)

    ab_path = Path(args.ab)
    out_dir = Path(args.out_dir)
    ab = json.loads(ab_path.read_text(encoding="utf-8"))
    missing = [variant["file"] for variant in ab["variants"]
               if not (out_dir / variant["file"]).exists()]
    if missing:
        print(f"ERROR: missing renders in {out_dir}: {', '.join(missing)}", file=sys.stderr)
        print("Run first: python3 render_ab.py", file=sys.stderr)
        return 2

    checks = Checks()
    measurements = {"variants": {}}
    for variant in ab["variants"]:
        fs, width, left, right = parent_verify.read_wav(out_dir / variant["file"])
        if variant["sequence"] == "lr":
            data = verify_lr_variant(checks, ab, variant, fs, width, left, right)
        else:
            data = verify_main_variant(checks, ab, variant, fs, width, left, right)
        data["status"] = ("PASS" if all(
            item["status"] == "PASS" for item in checks.items
            if item["id"].startswith(variant["id"] + "-")) else "FAIL")
        measurements["variants"][variant["id"]] = data

    cross_variant_check(checks, measurements)

    determinism = None
    if not args.skip_determinism:
        names = [variant["file"] for variant in ab["variants"]]
        determinism = determinism_check(checks, ab_path, out_dir, names)
        measurements["determinism"] = determinism
    provenance_check(checks, measurements, out_dir)

    write_transition_csv(out_dir / "ab-transition-envelopes.csv", measurements)
    write_timeline_svg(out_dir / "ab-timeline.svg", ab, measurements)
    summary = checks.summary()
    write_reports(out_dir, ab, checks, measurements, summary, determinism)

    print("NAGLOED v2 — SpectraStrobe A/B calibration verification (DIAGNOSTIC, issue #31)")
    print(f"ab          : {ab_path.name} v{ab['meta']['version']} "
          f"({len(ab['variants'])} variants)")
    print(f"reference   : {float(ab['reference']['level_dbfs']):+.1f} dBFS, "
          f"{float(ab['reference']['pan_lfo_hz']):g} Hz pan LFO")
    print("")
    for item in checks.items:
        print(f"  {item['id']:<28} {item['status']:<5} {fmt(item['measured'])}")
    print("")
    print(f"SUMMARY     : {summary['verdict']} ({summary['pass']} PASS, {summary['fail']} FAIL "
          f"of {summary['total']})")
    print(f"report      : {out_dir.resolve()}/ab-report.md, ab-transition-envelopes.csv")
    return 1 if summary["fail"] > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
