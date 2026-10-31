from pathlib import Path
import json,random,subprocess,tempfile
source=Path(__file__).resolve().parents[1] / 'track-c/join.cpp'
table=[]
for i in range(256):
 x=i
 for _ in range(8): x=(x>>1)^(0x82f63b78 if x&1 else 0)
 table.append(x)
def crc(data):
 h=5381
 for b in data:h=table[(h^b)&255]^(h>>8)
 return h
rng=random.Random(93271);rows=[];rank_groups=[]
for n in range(17):
 for _ in range(64): rows.append((n,rng.randbytes(64)))
 length=min(4,n);base=bytearray(rng.randbytes(64));base[n-length:n]=bytes(length)
 start=len(rows);rows.append((n,bytes(base)))
 for bit in range(length*8):
  raw=bytearray(base);raw[n-length+bit//8]^=1<<(bit%8);rows.append((n,bytes(raw)))
 rank_groups.append((n,start,length*8))
driver='''#define main solution_main
#include SOURCE
#undef main
int main(){unsigned n;char hex[129];while(scanf("%u %128s",&n,hex)==2){char raw[64];for(unsigned i=0;i<64;++i){unsigned x;sscanf(hex+2*i,"%2x",&x);raw[i]=static_cast<char>(x);}auto k=encode_name(raw,n);printf("%llu %u %u\\n",static_cast<unsigned long long>(k.first),k.middle,k.hash);}return 0;}
'''.replace('SOURCE',json.dumps(str(source)))
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'driver.cpp').write_text(driver)
 subprocess.run(['g++','-O3','-march=x86-64-v3','-std=c++17','-pthread','-o',str(p/'check'),str(p/'driver.cpp')],check=True)
 data=''.join(f'{n} {raw.hex()}\n' for n,raw in rows)
 output=subprocess.check_output([str(p/'check')],input=data,text=True)
keys=[tuple(map(int,line.split())) for line in output.splitlines()]
assert len(keys)==len(rows)
for (n,raw),key in zip(rows,keys):
 prefix=raw[:min(n,12)].ljust(12,b'\0')
 want=(int.from_bytes(prefix[:8],'little'),int.from_bytes(prefix[8:12],'little'),crc(raw[:n]))
 assert key==want,(n,key,want)
for n,start,bits in rank_groups:
 basis={};base=keys[start][2]
 for key in keys[start+1:start+1+bits]:
  v=key[2]^base
  while v:
   b=v.bit_length()-1
   if b not in basis:basis[b]=v;break
   v^=basis[b]
 assert len(basis)==bits,(n,len(basis),bits)
print(f'{len(rows)} hardware/software encodings exact; suffix transforms have full rank for all lengths 0..16')
