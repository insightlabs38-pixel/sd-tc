"""Cross-worker/epoch distinctness and exact compact-descriptor fallbacks."""
import argparse
import csv
import hashlib
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser()
p.add_argument('--source',type=Path,required=True)
p.add_argument('--sanitizer',default='address,undefined')
p.add_argument('--wide',action='store_true')
a=p.parse_args()
work=ROOT/'work/reconciliation-cases'
data,out,expected=[work/n for n in ['data','out','expected']]
for d in [data,out,expected]:d.mkdir(parents=True,exist_ok=True)
source=work/'join.cpp'
source.write_text(a.source.read_text().replace('/data/',str(data)+'/').replace('/out/',str(out)+'/'))
binary=work/'join'
flags=['-O1','-g','-std=c++17','-march=x86-64-v3','-pthread','-fno-pie','-no-pie']
if a.sanitizer:flags+=['-fsanitize='+a.sanitizer,'-fno-omit-frame-pointer']
subprocess.run(['g++',*flags,str(source),'-o',str(binary)],check=True)
header=b'\xef\xbb\xbfid,name,note,region_id,amount\r\n'
lookup='region_id,country\n7,AAA\n8,BBB\n9,CCC\n10,DDD\n'

def letters(n):
    s=''
    while True:
        s=chr(65+n%26)+s
        n//=26
        if not n:return s

def run(label):
    code='import csv,runpy,sys;csv.field_size_limit(1<<30);sys.argv=sys.argv[1:];runpy.run_path(sys.argv[0],run_name="__main__")'
    subprocess.run([sys.executable,'-c',code,str(ROOT/'tests/oracle/reference.py'),'--data',str(data),'--out',str(expected)],check=True)
    subprocess.run([str(binary)],check=True)
    want=(expected/'aggregates.txt').read_bytes();got=(out/'aggregates.txt').read_bytes()
    assert want==got,(label,want,got)
    print(label,(data/'records.csv').stat().st_size,hashlib.sha256(got).hexdigest(),'exact',flush=True)

def row(i,name,note,region=8,bom=False):
    fields=[str(i),name,'"'+note.replace('"','""')+'"',str(region),'-0.05' if region!=8 else '1.00']
    return (b'\xef\xbb\xbf' if bom else b'')+','.join(fields).encode()+ (b'\r\n' if i%2 else b'\n')

(data/'regions.csv').write_text(lookup)
for label,n,width in [('cross-worker-only-repeat',10000,200),('cross-epoch-only-repeat',13000,8192)]:
    with (data/'records.csv').open('wb')as f:
        f.write(header)
        f.write(row(1,'','word',7))
        for i in range(2,n+2):
            name='"Name '+letters(i)+'"'
            if i==n//2:f.write(row(i,'""','middle',9,True))
            else:f.write(row(i,name,'a'*width,bom=i%317==0))
        f.write(row(n+2,'""','last',7,True))
    run(label)

if a.wide:
    with (data/'records.csv').open('wb')as f:
        f.write(header);body=f.tell();i=1
        f.write(row(i,'','first',7));i+=1
        target=(1<<24)-4
        while f.tell()-body<target-4096:
            f.write(row(i,'"Name '+letters(i)+'"','a'*1024));i+=1
        blank=row(i,'"Padding"','')
        padding=target-(f.tell()-body)-len(blank)
        assert padding>=0
        f.write(row(i,'"Padding"','a'*padding));i+=1
        assert f.tell()-body==target
        f.write(row(i,'"Wide offset"','a'*(128<<20),9));i+=1
        f.write(row(i,'""','last',7,True))
    run('wide-offset-and-large-field-fallback')
