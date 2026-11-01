#!/usr/bin/env python3
"""Source-immutable, same-runner exact full-solver PGO matrix; never an official board score."""
import hashlib, json, os, pathlib, shutil, statistics, subprocess, time
REFS = {
    "frozen": "f46c57e795e06ae8e69caf842f6d72aa9f74b4a2",
    "tiles": "a137469cf6ebd18bed7e4f61375c283798b55493",
    "singlewindow": "3a6ff974df1882c97293257b9def1aa2d9af7555",
    "fourstream": "c68260dea473d851890a783b147006cc1a39f7a2",
    "combined": "f876f58dae2800c5d031e5f78d81c3b440cc6e78",
}
EXPECTED = "d8088b1cd42ba1338b1345b107b55db7b96ef1f5066f3ff64fd26a8912a0f536"
build = {}
for name,sha in REFS.items():
    root = pathlib.Path("/tmp/c-arch-matrix")/name
    root.mkdir(parents=True, exist_ok=True)
    source=subprocess.check_output(["git","show",sha+":track-c/join.cpp"])
    (root/"join.cpp").write_bytes(source)
    shutil.copyfile("track-c/Dockerfile",root/"Dockerfile")
    shutil.copyfile("track-c/generator.py",root/"generator.py")
    build[name]={"commit":sha,"sha256":hashlib.sha256(source).hexdigest(),"physical_lines":len(source.splitlines())}
    subprocess.run(["docker","build","--network=none","-t","carch-"+name,str(root)],check=True,stdout=subprocess.DEVNULL)
    cid=subprocess.check_output(["docker","create","carch-"+name],text=True).strip()
    subprocess.run(["docker","cp",cid+":/join","/tmp/carch-"+name],check=True)
    subprocess.run(["docker","rm",cid],stdout=subprocess.DEVNULL,check=True)
affinity=sorted(os.sched_getaffinity(0))
quota=pathlib.Path("/sys/fs/cgroup/cpu.max")
cpu_info=subprocess.check_output(["bash","-c","grep -m1 'model name' /proc/cpuinfo"],text=True).strip()
print("ARCH_ENV="+json.dumps({"visible":affinity,"quota":quota.read_text().strip() if quota.exists() else None,"cpu":cpu_info}),flush=True)
os.sched_setaffinity(0,affinity[:2])
runs={k:[] for k in REFS}
for round_no in range(11):
    names=list(REFS)
    if round_no%2:names.reverse()
    for name in names:
        exe="/tmp/carch-"+name
        output=pathlib.Path("/out/aggregates.txt")
        output.unlink(missing_ok=True)
        start=time.perf_counter_ns()
        pid=os.posix_spawn(exe,[exe],os.environ)
        _,status,ru=os.wait4(pid,0)
        ms=(time.perf_counter_ns()-start)/1e6
        if os.waitstatus_to_exitcode(status):raise RuntimeError((name,round_no,status))
        digest=hashlib.sha256(output.read_bytes()).hexdigest()
        if digest!=EXPECTED:raise AssertionError((name,round_no,digest))
        runs[name].append({"ms":round(ms,3),"rss_kib":ru.ru_maxrss})
summary={name:{"median_ms":statistics.median(x["ms"] for x in result),
               "min_ms":min(x["ms"] for x in result),
               "max_ms":max(x["ms"] for x in result),
               "median_rss_kib":statistics.median(x["rss_kib"] for x in result)} for name,result in runs.items()}
print("ARCH_MATRIX_RESULTS_JSON="+json.dumps({"build":build,"summary":summary,"raw":runs},sort_keys=True),flush=True)
