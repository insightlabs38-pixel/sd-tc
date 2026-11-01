#!/usr/bin/env python3
"""Unscored paired container memory; elapsed Docker launch time is not Track C T."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import statistics
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("baseline")
parser.add_argument("current")
args = parser.parse_args()
images = {"baseline": args.baseline, "current": args.current}
rows = {name: [] for name in images}
rng = random.Random(20261031)
for trial in range(12):
    order = list(images)
    rng.shuffle(order)
    for name in order:
        Path("/out/aggregates.txt").unlink(missing_ok=True)
        run = subprocess.run([
            "docker", "run", "--rm", "--network=none", "--read-only", "--memory=8g", "--cpus=4",
            "--user", "65534:65534", "-v", "/data:/data:ro", "-v", "/out:/out:rw",
            "--entrypoint", "/bin/sh", images[name], "-c",
            "/join && cat /sys/fs/cgroup/memory.peak",
        ], capture_output=True, text=True, check=True)
        digest = hashlib.sha256(Path("/out/aggregates.txt").read_bytes()).hexdigest()
        assert digest == "d8088b1cd42ba1338b1345b107b55db7b96ef1f5066f3ff64fd26a8912a0f536", (name, trial, digest)
        peak = int(run.stdout.strip())
        assert peak > 0
        rows[name].append(peak)
        print(f"{name} trial={trial + 1} cgroup_peak_bytes={peak}", flush=True)
print(json.dumps({name: {"median_peak_bytes": statistics.median(values), "runs": values}
                  for name, values in rows.items()}, indent=2))
