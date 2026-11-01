#!/usr/bin/env python3
"""Interleaved full-PGO same-run comparison, pinned official Dockerfile and oracle."""
import hashlib, json, os, pathlib, statistics, subprocess, tempfile, time

SHAS = {
    "incumbent": "f46c57e795e06ae8e69caf842f6d72aa9f74b4a2",
    "tile12_batch64": "0838243fed1841191a7bea3f50fb723a1a571038",
    "tile12_batch128": "26681cae0359f037f5974a046d93553116c60cb7",
    "fieldscan_batch128": "52b9e62558b68db8e9e49c4a4a00227f238447d2",
    "tile12_batch256": "7eebdd42392226908b066a430ca23822931d4534",
}
EXPECTED = "d8088b1cd42ba1338b1345b107b55db7b96ef1f5066f3ff64fd26a8912a0f536"
ROOT = pathlib.Path(__file__).resolve().parents[2]
os.makedirs("/data", exist_ok=True)
os.makedirs("/out", exist_ok=True)
subprocess.run(["python3", "tests/oracle/generator.py", "--seed", "1", "--scale", "full", "--out", "/data"], check=True)
for name, sha in SHAS.items():
    with tempfile.TemporaryDirectory(prefix="c-pgo-") as d:
        staging = pathlib.Path(d)
        staging.joinpath("join.cpp").write_bytes(subprocess.check_output(["git", "show", sha + ":track-c/join.cpp"]))
        for p in ("Dockerfile", "generator.py"):
            staging.joinpath(p).write_bytes(ROOT.joinpath("track-c", p).read_bytes())
        tag = "c-research-pgo-" + name
        subprocess.run(["docker", "build", "--network=none", "-t", tag, str(staging)], check=True)
        container = subprocess.check_output(["docker", "create", tag], text=True).strip()
        try:
            subprocess.run(["docker", "cp", container + ":/join", "/tmp/" + name], check=True)
        finally:
            subprocess.run(["docker", "rm", container], check=True, stdout=subprocess.DEVNULL)
cpus = sorted(os.sched_getaffinity(0))[:2]
os.sched_setaffinity(0, cpus)
data = {name: [] for name in SHAS}
for trial in range(9):
    order = list(SHAS)
    if trial & 1: order.reverse()
    for name in order:
        path = "/tmp/" + name
        pathlib.Path("/out/aggregates.txt").unlink(missing_ok=True)
        t0 = time.perf_counter_ns()
        pid = os.posix_spawn(path, [path], os.environ)
        _, status, usage = os.wait4(pid, 0)
        dt = (time.perf_counter_ns() - t0) / 1e6
        if os.waitstatus_to_exitcode(status): raise RuntimeError((name, trial, status))
        digest = hashlib.sha256(pathlib.Path("/out/aggregates.txt").read_bytes()).hexdigest()
        if digest != EXPECTED: raise AssertionError((name, trial, digest))
        data[name].append({"ms": round(dt, 3), "rss_kib": usage.ru_maxrss})
summary = {name: {"median_ms": statistics.median(r["ms"] for r in rows),
                  "median_rss_kib": statistics.median(r["rss_kib"] for r in rows),
                  "min_ms": min(r["ms"] for r in rows), "max_ms": max(r["ms"] for r in rows)}
           for name, rows in data.items()}
print("PGO_MATRIX_RESULTS_JSON=" + json.dumps({"sha": SHAS, "cpu": cpus, "results": summary, "raw": data},
                                              sort_keys=True), flush=True)
