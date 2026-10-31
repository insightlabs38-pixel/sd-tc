import subprocess,statistics,json,hashlib,sys
from pathlib import Path
Path("/out/aggregates.txt").unlink(missing_ok=True)
bins=sys.argv[1:]; rows={b:[] for b in bins}
image='debian@sha256:62bd423aeee912639ca8ee681c2b762cb3e25137e0a92b20be455898a9f034ad'
for trial in range(12):
 for b in bins if trial%2==0 else bins[::-1]:
  cmd=['docker','run','--rm','--network=none','--read-only','--memory=8g','--cpus=2','--user','65534:65534','-v','/workspace/work/runtime:/bench:ro','-v','/workspace/work/data:/data:ro','-v','/out:/out:rw',image,'/bench/launch','/bench/'+b]
  r=subprocess.run(cmd,capture_output=True,text=True,check=True)
  assert hashlib.sha256(open('/out/aggregates.txt','rb').read()).hexdigest()=='d8088b1cd42ba1338b1345b107b55db7b96ef1f5066f3ff64fd26a8912a0f536'
  v=list(map(float,r.stdout.split()));rows[b].append(v)
print(json.dumps({b:{'wall_ms':statistics.median(r[0] for r in rs),'memory_peak':statistics.median(r[1] for r in rs),'rss_kib':statistics.median(r[2] for r in rs),'cpu_ms':statistics.median(r[3] for r in rs),'runs':rs} for b,rs in rows.items()},indent=2))
