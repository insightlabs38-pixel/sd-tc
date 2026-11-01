#!/usr/bin/env python3
"""Immutable GCC15 PGO worker-count controls, full 3.32M-record exactness, interleaved timing."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import statistics
import subprocess
import time

FROZEN = "f46c57e795e06ae8e69caf842f6d72aa9f74b4a2"
TILES = "a137469cf6ebd18bed7e4f61375c283798b55493"
PREFETCH = "e5c0a8923eefff3a95942b5aac4d95ed6dafc044"
THREAD_LINE = "        unsigned threads = end - p < (1 << 20) ? 1 : 6;"
CAP_LINE = "        threads = std::min(threads, std::max(1u, std::thread::hardware_concurrency()));"
EXPECTED = "d8088b1cd42ba1338b1345b107b55db7b96ef1f5066f3ff64fd26a8912a0f536"

def source(ref):
    return subprocess.check_output(["git", "show", ref + ":track-c/join.cpp"]).decode("utf-8")

def replace_once(code, old, new):
    assert code.count(old) == 1, (old, code.count(old))
    return code.replace(old, new, 1)

base = source(FROZEN)
tile = source(TILES)
prefetch = source(PREFETCH)
variants = {
    "incumbent6": base,
    "incumbent_cap": replace_once(base, THREAD_LINE, THREAD_LINE + "\n" + CAP_LINE),
    "tile_cap": tile,
    "tile6": replace_once(tile, CAP_LINE + "\n", ""),
    "tile12": replace_once(replace_once(tile, CAP_LINE + "\n", ""),
                           THREAD_LINE, THREAD_LINE.replace(": 6;", ": 12;")),
    "tile_prefetch6": replace_once(prefetch, CAP_LINE + "\n", ""),
}
sources = {}
for name, code in variants.items():
    directory = Path("/tmp/c-scaling") / name
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "join.cpp").write_text(code)
    shutil.copyfile("track-c/Dockerfile", directory / "Dockerfile")
    shutil.copyfile("track-c/generator.py", directory / "generator.py")
    sources[name] = dict(sha256=hashlib.sha256(code.encode()).hexdigest(), lines=len(code.splitlines()))
    subprocess.run(["docker", "build", "--network=none", "-t", "c-scaling-" + name, str(directory)],
                   check=True, stdout=subprocess.DEVNULL)
    container = subprocess.check_output(["docker", "create", "c-scaling-" + name]).decode().strip()
    executable = "/tmp/c-scaling-" + name
    subprocess.run(["docker", "cp", container + ":/join", executable], check=True)
    subprocess.run(["docker", "rm", container], check=True, stdout=subprocess.DEVNULL)

cpus = sorted(os.sched_getaffinity(0))[:2]
os.sched_setaffinity(0, cpus)
print("SCALING_ENV=" + json.dumps(dict(cpus=cpus, visible_cores=os.cpu_count(),
                                         reference=FROZEN, tile=TILES, prefetch=PREFETCH)), flush=True)
samples = {name: [] for name in variants}
for round_id in range(9):
    order = list(variants)
    if round_id % 2: order.reverse()
    for name in order:
        executable = "/tmp/c-scaling-" + name
        Path("/out/aggregates.txt").unlink(missing_ok=True)
        start = time.perf_counter_ns()
        pid = os.posix_spawn(executable, [executable], os.environ)
        _, status, usage = os.wait4(pid, 0)
        duration = (time.perf_counter_ns() - start) / 1e6
        assert os.waitstatus_to_exitcode(status) == 0, (name, round_id, status)
        digest = hashlib.sha256(Path("/out/aggregates.txt").read_bytes()).hexdigest()
        assert digest == EXPECTED, (name, round_id, digest)
        samples[name].append(dict(ms=round(duration, 3), rss_kib=usage.ru_maxrss))
summary = {name: {
    "median_ms": statistics.median(x["ms"] for x in rows),
    "median_rss_kib": statistics.median(x["rss_kib"] for x in rows),
    "min_ms": min(x["ms"] for x in rows), "max_ms": max(x["ms"] for x in rows)
} for name, rows in samples.items()}
print("SCALING_MATRIX_JSON=" + json.dumps(dict(sources=sources, cpu=cpus, summary=summary, raw=samples),
                                           sort_keys=True), flush=True)
