#!/usr/bin/env python3
"""Summarise a nagl_preview --frames metrics.csv as a markdown table."""
import csv, sys, statistics

path = sys.argv[1]
label = sys.argv[2] if len(sys.argv) > 2 else ""
rows = []
with open(path, newline="") as f:
    for r in csv.DictReader(f):
        if not r.get("frame"):
            continue
        rows.append({
            "frame": int(r["frame"]),
            "phase": r["phase"],
            "grains": int(r["numGrains"]),
            "lum": float(r["meanLum"]),
            "delta": float(r["meanAbsDelta"]),
            "cov": float(r["coverage"]),
            "active": float(r["activeCoverage"]),
            "cx": float(r["centroidX"]),
            "cy": float(r["centroidY"]),
        })

print(f"### {label}" if label else "###")
print("| phase | frames | mean grains | meanLum | mean abs delta | median delta | p95 delta | static frames (<1e-4) | coverage | active coverage |")
print("|---|---|---|---|---|---|---|---|---|---|")
for phase in ["dense", "sparse", "silence", "return"]:
    ph = [r for r in rows if r["phase"] == phase]
    if not ph:
        continue
    d = [r["delta"] for r in ph]
    ds = sorted(d)
    med = ds[len(ds)//2]
    p95 = ds[min(len(ds)-1, int(len(ds)*0.95))]
    static = sum(1 for x in ds if x < 1e-4)
    print("| %s | %d | %.1f | %.4f | %.5f | %.6f | %.6f | %d (%.1f%%) | %.4f | %.4f |" % (
        phase, len(ph), statistics.mean(r["grains"] for r in ph),
        statistics.mean(r["lum"] for r in ph), statistics.mean(d),
        med, p95, static, 100.0*static/len(ph),
        statistics.mean(r["cov"] for r in ph), statistics.mean(r["active"] for r in ph)))
