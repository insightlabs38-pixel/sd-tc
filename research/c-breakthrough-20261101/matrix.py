#!/usr/bin/env python3
"""Same-run candidate comparison. Development measurements, never official K."""
import hashlib, json, os, pathlib, statistics, subprocess, time

SHAS = {
    "incumbent": "f46c57e795e06ae8e69caf842f6d72aa9f74b4a2",
    "tile12x8_batch64": "0838243fed1841191a7bea3f50fb723a1a571038",
    "tile6x8_batch64": "d59c61da2bec33d3cf10e098fbefb12bd6946a07",
    "tile12x8_batch128": "26681cae0359f037f5974a046d93553116c60cb7",
    "field_scanner": "51c56434782951c8a52f1543fa03ed6185569bcb",
    "exact_compact_pipeline": "36e0f129cb14da2d033576f36db028e4f94514c5",
    "tile12x8_batch256": "7eebdd42392226908b066a430ca23822931d4534",
    "tile12x8_batch512": "06c97813bd0b36060a9fb9c6405e11795b2a56b7",
    "field_scanner_batch128": "52b9e62558b68db8e9e49c4a4a00227f238447d2",
}
EXPECTED = "d8088b1cd42ba1338b1345b107b55db7b96ef1f5066f3ff64fd26a8912a0f536"
os.makedirs("/data", exist_ok=True)
os.makedirs("/out", exist_ok=True)
subprocess.run(["python3", "tests/oracle/generator.py", "--seed", "1", "--scale", "full", "--out", "/data"], check=True)
cpus = sorted(os.sched_getaffinity(0))[:2]
os.sched_setaffinity(0, cpus)
print("CPUs", cpus, flush=True)
for name, sha in SHAS.items():
    source = subprocess.check_output(["git", "show", sha + ":track-c/join.cpp"])
    path = pathlib.Path("/tmp") / (name + ".cpp")
    path.write_bytes(source)
    subprocess.run(["g++", "-O3", "-march=x86-64-v3", "-std=c++20", "-DNDEBUG", "-flto", "-pthread",
                    "-o", str(path.with_suffix("")), str(path)], check=True)
data = {name: [] for name in SHAS}
for round_id in range(9):
    names = list(SHAS)
    if round_id % 2: names.reverse()
    for name in names:
        path = "/tmp/" + name
        pathlib.Path("/out/aggregates.txt").unlink(missing_ok=True)
        t0 = time.perf_counter_ns()
        pid = os.posix_spawn(path, [path], os.environ)
        _, status, usage = os.wait4(pid, 0)
        dt = (time.perf_counter_ns() - t0) / 1e6
        if os.waitstatus_to_exitcode(status): raise RuntimeError((name, round_id, status))
        digest = hashlib.sha256(pathlib.Path("/out/aggregates.txt").read_bytes()).hexdigest()
        if digest != EXPECTED: raise AssertionError((name, round_id, digest))
        data[name].append({"ms": round(dt, 3), "rss_kib": usage.ru_maxrss})
summary = {name: {"median_ms": statistics.median(r["ms"] for r in rows),
                  "median_rss_kib": statistics.median(r["rss_kib"] for r in rows),
                  "min_ms": min(r["ms"] for r in rows), "max_ms": max(r["ms"] for r in rows)}
           for name, rows in data.items()}
print("MATRIX_RESULTS_JSON=" + json.dumps({"sha": SHAS, "cpu": cpus, "results": summary, "raw": data},
                                          sort_keys=True), flush=True)
