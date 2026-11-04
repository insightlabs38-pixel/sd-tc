"""Independent valid-CSV differential tests; uses the unchanged Python oracle."""
import argparse
import hashlib
from pathlib import Path
import random
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--source', type=Path, required=True)
p.add_argument('--cases', type=int, default=64)
p.add_argument('--sanitizer', default='address,undefined')
a = p.parse_args()
scratch = ROOT/'work/independent-fuzz'
scratch.mkdir(parents=True, exist_ok=True)
data, out, expected = [scratch/n for n in ['data', 'out', 'expected']]
for d in [data, out, expected]: d.mkdir(exist_ok=True)
source = scratch/'join.cpp'
source.write_text(a.source.read_text().replace('/data/',str(data)+'/').replace('/out/',str(out)+'/'))
binary = scratch/'join'
flags = ['-O1','-g','-std=c++17','-march=x86-64-v3','-pthread','-fno-pie','-no-pie']
if a.sanitizer: flags += ['-fsanitize='+a.sanitizer,'-fno-omit-frame-pointer']
subprocess.run(['g++',*flags,str(source),'-o',str(binary)],check=True)

def quoted(s): return '"'+s.replace('"','""')+'"'
def text(s,rng): return quoted(s) if s or rng.randrange(2) else ''
def money(n): return ('-' if n<0 else '')+str(abs(n)//100)+'.'+str(abs(n)%100).zfill(2)

receipts=[]
for seed in range(a.cases):
    rng=random.Random(202611010000+seed)
    names=['','Betts','Betts ','Ann "Annie" Lee', 'Comma, Name']
    for _ in range(200):
        n=rng.randrange(1,140)
        names.append(rng.choice('ABCDEFGHIJKLMNOPQRSTUVWXYZ')+''.join(rng.choice('abcXYZ ,"')for _ in range(n))+' '*rng.randrange(9))
    notes=['','"','"quoted"','one\ntwo\nthree\nfour','comma, " and newline\ntogether']
    for _ in range(100):
        notes.append('\n'.join(' '.join(rng.choice(['hello','no','"','word,','a"b'])for _ in range(rng.randrange(1,12)))for _ in range(rng.randrange(1,5))))
    (data/'regions.csv').write_text('region_id,country\n1,AAA\n7,AAA\n19,BBB\n999,CCC\n1001,ZZZ\n')
    n=100000 if seed%16==0 else rng.randrange(100,8000)
    ident=0
    with (data/'records.csv').open('wb') as f:
        f.write(b'\xef\xbb\xbfid,name,note,region_id,amount\r\n')
        for row in range(n):
            ident+=rng.randrange(1,12)
            region=rng.choice([1,7,19,999,888,9999999])
            amount=-rng.randrange(1,100) if region==999 else rng.randrange(-99999,10000000)
            fields=[str(ident),text(rng.choice(names),rng),text(rng.choice(notes),rng),str(region),money(amount)]
            bom=b'\xef\xbb\xbf' if row and rng.randrange(35)==0 else b''
            terminator=rng.choice([b'\n',b'\r\n'])
            if row==n-1 and seed%3==0: terminator=b''
            f.write(bom+','.join(fields).encode()+terminator)
    subprocess.run([sys.executable,str(ROOT/'tests/oracle/reference.py'),'--data',str(data),'--out',str(expected)],check=True)
    subprocess.run([str(binary)],check=True)
    want=(expected/'aggregates.txt').read_bytes();got=(out/'aggregates.txt').read_bytes()
    assert got==want,(seed,got,want)
    receipts.append({'seed':seed,'records':n,'input_bytes':(data/'records.csv').stat().st_size,'sha256':hashlib.sha256(got).hexdigest()})
    print(seed,n,'exact',flush=True)
import json
(scratch/'receipts.json').write_text(json.dumps(receipts,indent=2))
