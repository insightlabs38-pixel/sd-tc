from pathlib import Path
import argparse, subprocess, json, hashlib, random, statistics, uuid

ROOT = Path(__file__).resolve().parents[2]
r = ROOT/'work/independent-bench'
r.mkdir(parents=True,exist_ok=True)
p = argparse.ArgumentParser()
p.add_argument('images', nargs='+')
p.add_argument('--official',type=Path,required=True)
p.add_argument('--runs', type=int, default=5)
p.add_argument('--order-seed',type=int,default=20261101)
p.add_argument('--data',type=Path,default=r/'data')
p.add_argument('--expected',default='d8088b1cd42ba1338b1345b107b55db7b96ef1f5066f3ff64fd26a8912a0f536')
p.add_argument('--receipt',type=Path,default=r/'container-bench.json')
a = p.parse_args()
timer = r/'timer-bin/sd-timer'
timer.parent.mkdir(exist_ok=True)
timer.parent.chmod(0o755)
source = (a.official/'harness/timer/sd-timer.c').read_text()
needle = '    FILE *f = fopen(timing_path, "w");'
source = source.replace(needle, '''    unsigned long long peak = 0;
    FILE *mf = fopen("/sys/fs/cgroup/memory.peak", "r");
    if (mf) { (void)fscanf(mf, "%llu", &peak); fclose(mf); }
'''+needle)
source = source.replace('\\"signal\\": %d}', '\\"signal\\": %d, \\"peak_bytes\\": %llu}')
source = source.replace('    FILE *f = fopen(timing_path, \"w\");', '    FILE *f = fdopen(dup(STDOUT_FILENO), \"w\");')
source = source.replace('elapsed, exit_code, term_signal);', 'elapsed, exit_code, term_signal, peak);')
(r/'timer.c').write_text(source)
subprocess.run(['gcc','-O2','-static',str(r/'timer.c'),'-o',str(timer)],check=True)
timer.chmod(0o755)
(r/'docker-out').mkdir(exist_ok=True)
(r/'docker-out').chmod(0o777)
a.data.resolve().chmod(0o755)
for f in a.data.glob('*.csv'): f.chmod(0o644)
rows=[]
rng=random.Random(a.order_seed)
for trial in range(a.runs):
    order=a.images[:];rng.shuffle(order)
    for image in order:
        run=r/'container-runs'/uuid.uuid4().hex
        run.mkdir(parents=True)
        run.chmod(0o700)
        # Warm outside this fresh container, as the scoring harness does.
        for path in a.data.glob('*.csv'):
            with path.open('rb') as f:
                while f.read(4<<20): pass
        (r/'docker-out/aggregates.txt').unlink(missing_ok=True)
        name='frontier-'+uuid.uuid4().hex[:12]
        cmd=['docker','run','--name',name,'--network=none','--read-only','--cpuset-cpus=0-1',
             '--memory=8g','--memory-swap=8g','--pids-limit=512','--tmpfs','/tmp:rw,size=1g,exec',
             '--cap-drop=ALL','--cap-add=SETUID','--cap-add=SETGID','--security-opt=no-new-privileges',
             '--user=0:0','-v',str(a.data.resolve())+':/data:ro','-v',str(r/'docker-out')+':/out:rw',
             '-v',str(run)+':/sd:rw','-v',str(timer.parent)+':/sd-bin:ro','--entrypoint','/sd-bin/sd-timer',
             image,'/sd/timing.json','65534','65534','--','/join']
        try:
            result = subprocess.run(cmd,capture_output=True)
            if result.returncode: raise RuntimeError(result.stderr.decode())
            x=json.loads(result.stdout)
            assert x['exit_code']==0 and not x['signal'],x
            digest=hashlib.sha256((r/'docker-out/aggregates.txt').read_bytes()).hexdigest()
            assert digest==a.expected,(image,digest)
            assert list((r/'docker-out').iterdir())==[r/'docker-out/aggregates.txt']
            diff = subprocess.check_output(['docker','diff',name]).decode().splitlines()
            assert not [line for line in diff if line.split(maxsplit=1)[-1] not in {'/data','/out','/sd','/sd-bin','/tmp'}],diff
            x.update(image=image,trial=trial,sha256=digest)
            rows.append(x);print(json.dumps(x),flush=True)
        finally: subprocess.run(['docker','rm','-f',name],check=True,stdout=subprocess.DEVNULL)
for image in a.images:
    x=[v for v in rows if v['image']==image]
    times=sorted(v['wall_ns']/1e6 for v in x)
    print(image, 'trimmed_ms', statistics.mean(times[1:-1]) if len(times)>2 else statistics.mean(times),
          'peak_bytes',max(v['peak_bytes']for v in x),flush=True)
a.receipt.write_text(json.dumps(rows,indent=2))
