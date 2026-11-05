#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""NAGLOED v2 — SpectraStrobe A/B colour calibration: deterministic renderer.

EXPERIMENT / DIAGNOSTIC for issue #31. Separate from NAGLOED 1.0.4, from the
artistic composition Eerste Gloed and from the base colour calibration.
The colour levels rendered here are a controlled diagnostic ladder, NOT
artistic settings and NOT a recommendation.

Reads ab.json and renders one 44.1 kHz/16-bit stereo WAV per variant into
ab/out/, plus ab/out/ab-manifest.json with absolute levels, ratios, peaks,
hashes and timings. No normalisation, no dither, no metadata.

Variants:
  direct-<level>   the same 9-state sequence with a direct smoothstep crossfade
                   (outgoing ~50% / incoming ~50% at the midpoint)
  overlap-24       same sequence at -24 dBFS with the overlap shape: outgoing
                   carrier held while the incoming rises (both near full at the
                   midpoint), then the outgoing falls
  lr-24            R-L, R-R, G-L, G-R, B-L, B-R alternation at -24 dBFS
"""

from __future__ import annotations

import argparse
import array
import hashlib
import json
import math
import sys
from pathlib import Path

sys.dont_write_bytecode = True  # never leave __pycache__ in the parent experiment

AB_DIR = Path(__file__).resolve().parent
CAL_DIR = AB_DIR.parent
PARENT_DIR = CAL_DIR.parent
if str(PARENT_DIR) not in sys.path:
    sys.path.insert(0, str(PARENT_DIR))

import render as parent_render  # noqa: E402 - deliberate reuse of low-level helpers

TAU = 2.0 * math.pi
CARRIER_KEYS = ("red", "green", "blue")
CARRIER_FREQ = {"red": 18700.0, "green": 19200.0, "blue": 19700.0}


def load_ab(path: Path) -> dict:
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def ramp_array(count: int, fs: float, fade_in_s: float, fade_out_s: float) -> array.array:
    ramp = array.array("d", [1.0]) * count
    fade_in = max(0, int(round(fade_in_s * fs)))
    fade_out = max(0, int(round(fade_out_s * fs)))
    for i in range(min(fade_in, count)):
        ramp[i] = parent_render.smoothstep(i / fade_in) if fade_in else 1.0
    for i in range(min(fade_out, count)):
        ramp[count - 1 - i] = parent_render.smoothstep(i / fade_out) if fade_out else 1.0
    return ramp


def unit_sine(count: int, fs: float, freq: float, phase_rad: float) -> array.array:
    out = array.array("d", [0.0]) * count
    phase = float(phase_rad)
    step = TAU * freq / fs
    sin = math.sin
    for i in range(count):
        out[i] = sin(phase)
        phase += step
    return out


def reference_arrays(ab: dict, fs: float, count: int):
    ref = ab["reference"]
    amp = parent_render.db_to_lin(float(ref["level_dbfs"]))
    timing = ab["timing"]
    ramp = ramp_array(count, fs, float(timing["fade_in_ms"]) / 1000.0,
                      float(timing["fade_out_ms"]) / 1000.0)
    left = array.array("d", [0.0]) * count
    right = array.array("d", [0.0]) * count
    phase_ref = 0.0
    phase_lfo = 0.0
    step_ref = TAU * float(ref["hz"]) / fs
    step_lfo = TAU * float(ref["pan_lfo_hz"]) / fs
    quarter_pi = 0.25 * math.pi
    sin = math.sin
    cos = math.cos
    for i in range(count):
        theta = quarter_pi + quarter_pi * sin(phase_lfo)
        sample = amp * sin(phase_ref) * ramp[i]
        left[i] = sample * cos(theta)
        right[i] = sample * sin(theta)
        phase_ref += step_ref
        phase_lfo += step_lfo
    return left, right


def _state_flags(states):
    return [{key: (1.0 if float(state["expected_ratios"][key]) > 0.0 else 0.0)
             for key in CARRIER_KEYS} for state in states]


def _transition_activation(t: float, boundary: float, prev: float, next: float,
                           shape: str, xfade: float) -> float:
    half = xfade / 2.0
    if shape == "overlap":
        if t < boundary:
            w_in = parent_render.smoothstep((t - (boundary - half)) / half)
            return max(prev, w_in * next)
        w_out = 1.0 - parent_render.smoothstep((t - boundary) / half)
        return max(w_out * prev, next)
    s = parent_render.smoothstep((t - (boundary - half)) / xfade)
    return (1.0 - s) * prev + s * next


def build_main_envelopes(ab: dict, fs: float, count: int, shape: str):
    timing = ab["timing"]
    fade_in = float(timing["fade_in_ms"]) / 1000.0
    fade_out = float(timing["fade_out_ms"]) / 1000.0
    state_dur = float(timing["state_ms"]) / 1000.0
    xfade = float(timing["crossfade_ms"]) / 1000.0
    half = xfade / 2.0
    states = ab["states"]
    n_states = len(states)
    flags = _state_flags(states)
    ramp = ramp_array(count, fs, fade_in, fade_out)
    envelopes = {key: array.array("d", [0.0]) * count for key in CARRIER_KEYS}
    for i in range(count):
        t = i / fs
        idx = int((t - fade_in) // state_dur) if t >= fade_in else 0
        idx = 0 if idx < 0 else (n_states - 1 if idx >= n_states else idx)
        b_prev = fade_in + idx * state_dur
        b_next = fade_in + (idx + 1) * state_dur
        for key in CARRIER_KEYS:
            alpha = flags[idx][key]
            if idx > 0 and b_prev - half < t < b_prev + half:
                alpha = _transition_activation(t, b_prev, flags[idx - 1][key],
                                               flags[idx][key], shape, xfade)
            elif idx < n_states - 1 and b_next - half < t < b_next + half:
                alpha = _transition_activation(t, b_next, flags[idx][key],
                                               flags[idx + 1][key], shape, xfade)
            envelopes[key][i] = alpha * ramp[i]
    return envelopes


def build_lr_envelopes(ab: dict, fs: float, count: int):
    timing = ab["timing"]
    fade_in = float(timing["fade_in_ms"]) / 1000.0
    fade_out = float(timing["fade_out_ms"]) / 1000.0
    state_dur = float(timing["state_ms"]) / 1000.0
    xfade = float(timing["crossfade_ms"]) / 1000.0
    half = xfade / 2.0
    states = ab["lr_states"]
    n_states = len(states)
    flags = {key: [] for key in CARRIER_KEYS}
    gain_l = {key: [] for key in CARRIER_KEYS}
    gain_r = {key: [] for key in CARRIER_KEYS}
    for state in states:
        g_l, g_r = parent_render.pan_linear_0db(float(state["pan"]))
        for key in CARRIER_KEYS:
            active = 1.0 if state["carrier"] == key else 0.0
            flags[key].append(active)
            gain_l[key].append(g_l if active else 0.0)
            gain_r[key].append(g_r if active else 0.0)
    ramp = ramp_array(count, fs, fade_in, fade_out)
    env = {key: array.array("d", [0.0]) * count for key in CARRIER_KEYS}
    gl_env = {key: array.array("d", [0.0]) * count for key in CARRIER_KEYS}
    gr_env = {key: array.array("d", [0.0]) * count for key in CARRIER_KEYS}
    for i in range(count):
        t = i / fs
        idx = int((t - fade_in) // state_dur) if t >= fade_in else 0
        idx = 0 if idx < 0 else (n_states - 1 if idx >= n_states else idx)
        b_prev = fade_in + idx * state_dur
        b_next = fade_in + (idx + 1) * state_dur
        for key in CARRIER_KEYS:
            active = flags[key][idx]
            gl = gain_l[key][idx]
            gr = gain_r[key][idx]
            if idx > 0 and b_prev - half < t < b_prev + half:
                s = parent_render.smoothstep((t - (b_prev - half)) / xfade)
                active = (1.0 - s) * flags[key][idx - 1] + s * flags[key][idx]
                gl = (1.0 - s) * gain_l[key][idx - 1] + s * gain_l[key][idx]
                gr = (1.0 - s) * gain_r[key][idx - 1] + s * gain_r[key][idx]
            elif idx < n_states - 1 and b_next - half < t < b_next + half:
                s = parent_render.smoothstep((t - (b_next - half)) / xfade)
                active = (1.0 - s) * flags[key][idx] + s * flags[key][idx + 1]
                gl = (1.0 - s) * gain_l[key][idx] + s * gain_l[key][idx + 1]
                gr = (1.0 - s) * gain_r[key][idx] + s * gain_r[key][idx + 1]
            env[key][i] = active * ramp[i]
            gl_env[key][i] = gl * ramp[i]
            gr_env[key][i] = gr * ramp[i]
    return env, gl_env, gr_env


def render_main(ab: dict, fs: int, reference, units, envelopes, level_dbfs: float):
    level = parent_render.db_to_lin(float(level_dbfs))
    count = len(reference[0])
    left = array.array("d", reference[0])
    right = array.array("d", reference[1])
    for key in CARRIER_KEYS:
        env = envelopes[key]
        unit = units[key]
        for i in range(count):
            value = level * env[i] * unit[i]
            left[i] += value
            right[i] += value
    return left, right


def render_lr(ab: dict, fs: int, reference, units, envelopes, level_dbfs: float):
    level = parent_render.db_to_lin(float(level_dbfs))
    env, gl_env, gr_env = envelopes
    count = len(reference[0])
    left = array.array("d", reference[0])
    right = array.array("d", reference[1])
    for key in CARRIER_KEYS:
        e = env[key]
        gl = gl_env[key]
        gr = gr_env[key]
        unit = units[key]
        for i in range(count):
            value = level * e[i] * unit[i]
            left[i] += value * gl[i]
            right[i] += value * gr[i]
    return left, right


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description="NAGLOED v2 SpectraStrobe A/B calibration renderer (DIAGNOSTIC, issue #31)")
    parser.add_argument("--ab", default=str(AB_DIR / "ab.json"))
    parser.add_argument("--out-dir", default=str(AB_DIR / "out"))
    args = parser.parse_args(argv)

    ab_path = Path(args.ab)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    ab = load_ab(ab_path)
    fs = int(ab["audio"]["sample_rate_hz"])
    bits = int(ab["audio"]["bit_depth"])
    duration_s = float(ab["meta"]["duration_s"])
    lr_duration_s = float(ab["meta"]["lr_duration_s"])
    timing = ab["timing"]
    count_main = int(round(duration_s * fs))
    count_lr = int(round(lr_duration_s * fs))

    ref_main = reference_arrays(ab, fs, count_main)
    ref_lr = reference_arrays(ab, fs, count_lr)
    units_main = {key: unit_sine(count_main, fs, CARRIER_FREQ[key],
                                 float(timing["phase_rad"][key])) for key in CARRIER_KEYS}
    units_lr = {key: unit_sine(count_lr, fs, CARRIER_FREQ[key],
                               float(timing["phase_rad"][key])) for key in CARRIER_KEYS}
    envelopes_direct = build_main_envelopes(ab, fs, count_main, "direct")
    envelopes_overlap = build_main_envelopes(ab, fs, count_main, "overlap")
    envelopes_lr = build_lr_envelopes(ab, fs, count_lr)

    outputs = {}
    for variant in ab["variants"]:
        sequence = variant["sequence"]
        level = float(variant["colour_level_dbfs"])
        if sequence == "main":
            envelopes = envelopes_overlap if variant["crossfade_shape"] == "overlap" else envelopes_direct
            left, right = render_main(ab, fs, ref_main, units_main, envelopes, level)
            expected_duration = duration_s
        else:
            left, right = render_lr(ab, fs, ref_lr, units_lr, envelopes_lr, level)
            expected_duration = lr_duration_s
        peak = max(max(left), -min(left), max(right), -min(right))
        peak_dbfs = parent_render.lin_to_db(peak)
        peak_limit = float((ab.get("peak_policy") or {}).get("max_total_sample_peak_dbfs", -6.0))
        if peak_dbfs > peak_limit:
            raise RuntimeError(f"{variant['id']}: rendered peak {peak_dbfs:.2f} dBFS "
                               f"exceeds {peak_limit:.1f} dBFS")
        file_name = variant["file"]
        written_peak = parent_render.write_pcm(out_dir / file_name, fs, bits, left, right)
        outputs[file_name] = {
            "sha256": sha256_file(out_dir / file_name),
            "bytes": (out_dir / file_name).stat().st_size,
            "peak_dbfs": round(written_peak, 2),
            "duration_s": round(len(left) / fs, 6),
        }

    manifest = {
        "generator": "render_ab.py",
        "experiment": "SpectraStrobe A/B colour calibration (DIAGNOSTIC, issue #31)",
        "separate_from": ab["meta"]["separate_from"],
        "version": ab["meta"]["version"],
        "ab_file": ab_path.name,
        "ab_sha256": sha256_file(ab_path),
        "scripts": {
            "render_ab.py": sha256_file(AB_DIR / "render_ab.py"),
            "verify_ab.py": (sha256_file(AB_DIR / "verify_ab.py")
                             if (AB_DIR / "verify_ab.py").exists() else ""),
        },
        "command": "python3 render_ab.py --ab ab.json --out-dir out",
        "audio": {"sample_rate_hz": fs, "bit_depth": bits, "channels": 2,
                  "no_metadata": True, "no_dither": True, "no_normalisation": True},
        "timing": timing,
        "reference": ab["reference"],
        "primary_source_guidance": ab["primary_source_guidance"],
        "level_documentation": ab["level_documentation"],
        "variants": ab["variants"],
        "states": ab["states"],
        "lr_states": ab["lr_states"],
        "outputs": outputs,
        "deterministic": True,
    }
    (out_dir / "ab-manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    print("NAGLOED v2 — SpectraStrobe A/B calibration render (DIAGNOSTIC, issue #31)")
    print(f"ab definition : {ab_path.name} v{ab['meta']['version']}  "
          f"{len(ab['variants'])} variants  ({duration_s:.1f} s main / {lr_duration_s:.1f} s LR)")
    print(f"reference     : {ab['reference']['hz']:.0f} Hz at "
          f"{ab['reference']['level_dbfs']:+.1f} dBFS peak, "
          f"{ab['reference']['pan_lfo_hz']:g} Hz pan LFO")
    for variant in ab["variants"]:
        info = outputs[variant["file"]]
        print(f"  {variant['id']:<14} colour {variant['colour_level_dbfs']:>6.1f} dBFS  "
              f"ratio {variant['ratio_to_reference_db']:>+6.2f} dB ({variant['ratio_percent_of_reference']:7.2f}% of ref)  "
              f"{variant['percent_of_mindplace_full_brightness']:6.2f}% of full  "
              f"peak {info['peak_dbfs']:+6.2f} dBFS")
    print(f"output        : {out_dir.resolve()}")
    print("  - ab-manifest.json")
    return 0


if __name__ == "__main__":
    sys.exit(main())
