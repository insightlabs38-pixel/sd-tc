#!/usr/bin/env python3
"""Paired, millisecond-resolution full-seed benchmark (unscored runner)."""
import hashlib
import os
from pathlib import Path
import statistics
import time

EXPECTED = "d8088b1cd42ba1338b1345b107b55db7b96ef1f5066f3ff64fd26a8912a0f536"
BINS = {"baseline": "/tmp/sd-track-c-baseline", "current": "/tmp/sd-track-c"}
samples = {key: [] for key in BINS}
rss = {key: [] for key in BINS}

for trial in range(12):
    order = tuple(BINS) if trial % 2 == 0 else tuple(reversed(BINS))
    for variant in order:
        path = BINS[variant]
        start = time.perf_counter_ns()
        pid = os.posix_spawn(path, [path], os.environ)
        _, status, usage = os.wait4(pid, 0)
        elapsed = (time.perf_counter_ns() - start) / 1e6
        assert os.waitstatus_to_exitcode(status) == 0, (variant, trial, status)
        digest = hashlib.sha256(Path("/out/aggregates.txt").read_bytes()).hexdigest()
        assert digest == EXPECTED, (variant, trial, digest)
        samples[variant].append(elapsed)
        rss[variant].append(usage.ru_maxrss)
        print(f"{variant} trial={trial + 1} elapsed_ms={elapsed:.3f} "
              f"peak_rss_kib={usage.ru_maxrss}", flush=True)

for variant in BINS:
    print(f"{variant} median_ms={statistics.median(samples[variant]):.3f} "
          f"median_rss_kib={statistics.median(rss[variant]):.0f}", flush=True)
