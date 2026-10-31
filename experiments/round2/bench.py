import subprocess,time,hashlib,statistics,sys,json,os
bins=sys.argv[1:]; rows={b:[] for b in bins}
for i in range(12):
 for b in bins if i%2==0 else bins[::-1]:
  t=time.perf_counter(); pid=os.fork()
  if pid==0: os.execl(b,b)
  _,status,r=os.wait4(pid,0); assert status==0
  wall=(time.perf_counter()-t)*1000
  assert hashlib.sha256(open('/workspace/work/out/aggregates.txt','rb').read()).hexdigest()=='d8088b1cd42ba1338b1345b107b55db7b96ef1f5066f3ff64fd26a8912a0f536'
  rows[b].append([wall,(r.ru_utime+r.ru_stime)*1000,r.ru_maxrss])

print(json.dumps({b:{'wall_ms':statistics.median(x[0] for x in rs),'cpu_ms':statistics.median(x[1] for x in rs),'rss_kib':statistics.median(x[2] for x in rs),'runs':rs} for b,rs in rows.items()},indent=2))
