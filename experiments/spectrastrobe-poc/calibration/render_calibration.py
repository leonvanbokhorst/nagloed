#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""NAGLOED v2 — SpectraStrobe colour calibration: deterministic renderer.

EXPERIMENT / DIAGNOSTIC. Separate from NAGLOED 1.0.4 and from the artistic
composition Eerste Gloed. It does not touch the parent experiment.

Reads calibration.json and renders, in 44.1 kHz/16-bit stereo PCM:
  out/calibration-44k1-16bit.wav      the 9-state colour sequence
  out/calibration-lr-44k1-16bit.wav   optional 6-state left/right eye check
  out/calibration-manifest.json       hashes, timings, exposure and command

Each state is ~1 s; crossfades of 250 ms are centred on the state boundaries;
a 300 ms fade-in and fade-out wrap the sequence. The reference carrier
(18.2 kHz, -30 dBFS peak, 20 Hz sine equal-power panning) runs throughout.
Every active colour carrier is at 6.25% of the reference peak (dim exposure);
the hard ceiling is 25% per carrier. Smooth crossfades mean there is no black
gap between colour carriers; they are NOT permission for faster flashing.

Reuses low-level helpers from the parent render.py (no edits to it).
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

CAL_DIR = Path(__file__).resolve().parent
PARENT_DIR = CAL_DIR.parent
if str(PARENT_DIR) not in sys.path:
    sys.path.insert(0, str(PARENT_DIR))

import render as parent_render  # noqa: E402 - deliberate reuse of low-level helpers

TAU = 2.0 * math.pi
CARRIER_KEYS = ("red", "green", "blue")


def load_calibration(path: Path) -> dict:
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def ramp_array(count: int, fs: float, fade_in_s: float, fade_out_s: float) -> array.array:
    """Global 300 ms smoothstep fade-in/out applied to carriers and colours."""
    ramp = array.array("d", [1.0]) * count
    fade_in = max(0, int(round(fade_in_s * fs)))
    fade_out = max(0, int(round(fade_out_s * fs)))
    for i in range(min(fade_in, count)):
        ramp[i] = parent_render.smoothstep(i / fade_in) if fade_in else 1.0
    for i in range(min(fade_out, count)):
        ramp[count - 1 - i] = parent_render.smoothstep(i / fade_out) if fade_out else 1.0
    return ramp


def _state_flags(states, key: str = "expected_ratios"):
    return [{c: (1.0 if float(state[key][c]) > 0.0 else 0.0) for c in CARRIER_KEYS}
            for state in states]


def build_colour_envelopes(cal: dict, states, fs: float, duration_s: float):
    """Per-carrier envelope from the state table with 250 ms crossfades."""
    timing = cal["timing"]
    fade_in = float(timing["fade_in_ms"]) / 1000.0
    fade_out = float(timing["fade_out_ms"]) / 1000.0
    state_dur = float(timing["state_ms"]) / 1000.0
    xfade = float(timing["crossfade_ms"]) / 1000.0
    count = int(round(duration_s * fs))
    n_states = len(states)
    flags = _state_flags(states)
    env = {key: array.array("d", [0.0]) * count for key in CARRIER_KEYS}
    ramp = ramp_array(count, fs, fade_in, fade_out)
    half = xfade / 2.0
    for i in range(count):
        t = i / fs
        idx = int((t - fade_in) // state_dur) if t >= fade_in else 0
        idx = 0 if idx < 0 else (n_states - 1 if idx >= n_states else idx)
        b_prev = fade_in + idx * state_dur
        b_next = fade_in + (idx + 1) * state_dur
        for key in CARRIER_KEYS:
            alpha = flags[idx][key]
            if idx > 0 and b_prev - half < t < b_prev + half:
                s = parent_render.smoothstep((t - (b_prev - half)) / xfade)
                alpha = (1.0 - s) * flags[idx - 1][key] + s * flags[idx][key]
            elif idx < n_states - 1 and b_next - half < t < b_next + half:
                s = parent_render.smoothstep((t - (b_next - half)) / xfade)
                alpha = (1.0 - s) * flags[idx][key] + s * flags[idx + 1][key]
            env[key][i] = alpha * ramp[i]
    return env


def build_lr_gains(cal: dict, lr_states, fs: float, duration_s: float):
    """Per-carrier, per-channel gain envelopes for the optional eye check."""
    timing = cal["timing"]
    fade_in = float(timing["fade_in_ms"]) / 1000.0
    fade_out = float(timing["fade_out_ms"]) / 1000.0
    state_dur = float(timing["state_ms"]) / 1000.0
    xfade = float(timing["crossfade_ms"]) / 1000.0
    count = int(round(duration_s * fs))
    n_states = len(lr_states)
    flags = {key: [] for key in CARRIER_KEYS}
    gain_l = {key: [] for key in CARRIER_KEYS}
    gain_r = {key: [] for key in CARRIER_KEYS}
    for state in lr_states:
        gl, gr = parent_render.pan_linear_0db(float(state["pan"]))
        for key in CARRIER_KEYS:
            active = 1.0 if state["carrier"] == key else 0.0
            flags[key].append(active)
            gain_l[key].append(gl if active else 0.0)
            gain_r[key].append(gr if active else 0.0)
    env = {key: array.array("d", [0.0]) * count for key in CARRIER_KEYS}
    gl_env = {key: array.array("d", [0.0]) * count for key in CARRIER_KEYS}
    gr_env = {key: array.array("d", [0.0]) * count for key in CARRIER_KEYS}
    ramp = ramp_array(count, fs, fade_in, fade_out)
    half = xfade / 2.0
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


def add_reference(cal: dict, fs: float, count: int, left, right):
    ref = cal["reference"]
    amp = parent_render.db_to_lin(float(ref["level_dbfs"]))
    timing = cal["timing"]
    ramp = ramp_array(count, fs, float(timing["fade_in_ms"]) / 1000.0,
                      float(timing["fade_out_ms"]) / 1000.0)
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
        left[i] += sample * cos(theta)
        right[i] += sample * sin(theta)
        phase_ref += step_ref
        phase_lfo += step_lfo


def render_sequence(cal: dict, states, fs: int, duration_s: float, lr: bool = False):
    count = int(round(duration_s * fs))
    left = array.array("d", [0.0]) * count
    right = array.array("d", [0.0]) * count
    ratio = float(cal["exposure"]["colour_ratio"])
    amp = parent_render.db_to_lin(float(cal["reference"]["level_dbfs"])) * ratio
    add_reference(cal, fs, count, left, right)
    if lr:
        env, gl_env, gr_env = build_lr_gains(cal, states, fs, duration_s)
        for key in CARRIER_KEYS:
            freq = float(cal["carriers_hz"][key])
            phase = float(cal["timing"]["phase_rad"][key])
            count_samples = count
            step = TAU * freq / fs
            phase_cur = phase
            for i in range(count_samples):
                value = amp * env[key][i] * math.sin(phase_cur)
                left[i] += value * gl_env[key][i]
                right[i] += value * gr_env[key][i]
                phase_cur += step
    else:
        env = build_colour_envelopes(cal, states, fs, duration_s)
        for key in CARRIER_KEYS:
            freq = float(cal["carriers_hz"][key])
            phase = float(cal["timing"]["phase_rad"][key])
            step = TAU * freq / fs
            phase_cur = phase
            for i in range(count):
                value = amp * env[key][i] * math.sin(phase_cur)
                left[i] += value
                right[i] += value
                phase_cur += step
    return left, right


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description="NAGLOED v2 SpectraStrobe colour calibration renderer (DIAGNOSTIC, EXPERIMENT)")
    parser.add_argument("--calibration", default=str(CAL_DIR / "calibration.json"))
    parser.add_argument("--out-dir", default=str(CAL_DIR / "out"))
    parser.add_argument("--skip-lr", action="store_true",
                        help="do not render the optional left/right eye-check WAV")
    args = parser.parse_args(argv)

    cal_path = Path(args.calibration)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    cal = load_calibration(cal_path)
    fs = int(cal["audio"]["sample_rate_hz"])
    bits = int(cal["audio"]["bit_depth"])
    duration_s = float(cal["meta"]["duration_s"])
    lr_duration_s = float(cal["meta"]["lr_duration_s"])
    outputs = {}

    main_name = cal["audio"]["main_file"]
    left, right = render_sequence(cal, cal["states"], fs, duration_s)
    peak = parent_render.write_pcm(out_dir / main_name, fs, bits, left, right)
    outputs[main_name] = {"sha256": sha256_file(out_dir / main_name),
                          "bytes": (out_dir / main_name).stat().st_size,
                          "peak_dbfs": round(peak, 2),
                          "duration_s": round(len(left) / fs, 6)}

    lr_name = cal["audio"]["lr_file"]
    if not args.skip_lr:
        lr_left, lr_right = render_sequence(cal, cal["lr_states"], fs, lr_duration_s, lr=True)
        lr_peak = parent_render.write_pcm(out_dir / lr_name, fs, bits, lr_left, lr_right)
        outputs[lr_name] = {"sha256": sha256_file(out_dir / lr_name),
                            "bytes": (out_dir / lr_name).stat().st_size,
                            "peak_dbfs": round(lr_peak, 2),
                            "duration_s": round(len(lr_left) / fs, 6)}

    manifest = {
        "generator": "render_calibration.py",
        "experiment": "SpectraStrobe colour calibration (DIAGNOSTIC, EXPERIMENT)",
        "separate_from": cal["meta"]["separate_from"],
        "version": cal["meta"]["version"],
        "calibration_file": cal_path.name,
        "calibration_sha256": sha256_file(cal_path),
        "scripts": {
            "render_calibration.py": sha256_file(CAL_DIR / "render_calibration.py"),
            "verify_calibration.py": (sha256_file(CAL_DIR / "verify_calibration.py")
                                      if (CAL_DIR / "verify_calibration.py").exists() else ""),
        },
        "command": "python3 render_calibration.py --calibration calibration.json --out-dir out",
        "audio": {"sample_rate_hz": fs, "bit_depth": bits, "channels": 2,
                  "no_metadata": True, "no_dither": True},
        "timing": cal["timing"],
        "exposure": cal["exposure"],
        "reference": cal["reference"],
        "states": cal["states"],
        "lr_states": cal["lr_states"],
        "outputs": outputs,
        "deterministic": True,
    }
    (out_dir / "calibration-manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    print("NAGLOED v2 — SpectraStrobe colour calibration render (DIAGNOSTIC)")
    print(f"calibration   : {cal_path.name} v{cal['meta']['version']}  "
          f"{len(cal['states'])} states, {duration_s:.1f} s")
    print(f"timing        : {cal['timing']['state_ms']} ms states, "
          f"{cal['timing']['crossfade_ms']} ms crossfades, "
          f"{cal['timing']['fade_in_ms']} ms fade-in/out")
    print(f"exposure      : {cal['exposure']['colour_ratio'] * 100:.2f}% per carrier "
          f"(hard ceiling {cal['exposure']['hard_ceiling'] * 100:.0f}%)")
    print(f"reference     : {cal['reference']['hz']:.0f} Hz at "
          f"{cal['reference']['level_dbfs']:+.1f} dBFS, "
          f"{cal['reference']['pan_lfo_hz']:g} Hz pan LFO")
    for name, info in outputs.items():
        print(f"  {name:<34} {info['peak_dbfs']:+6.2f} dBFS  {info['duration_s']:.3f} s")
    print(f"output        : {out_dir.resolve()}")
    print("  - calibration-manifest.json")
    return 0


if __name__ == "__main__":
    sys.exit(main())
