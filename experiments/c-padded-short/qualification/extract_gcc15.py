import urllib.request,json,tarfile,pathlib,hashlib
root=pathlib.Path('/workspace/work/gcc15');root.mkdir(exist_ok=True)
base='https://registry-1.docker.io/v2/library/gcc/'
token=json.load(urllib.request.urlopen('https://auth.docker.io/token?service=registry.docker.io&scope=repository:library/gcc:pull'))['token']
def get(path):return urllib.request.urlopen(urllib.request.Request(base+path,headers={'Authorization':'Bearer '+token}))
x=json.load(open('/workspace/work/public-research/gcc15-manifest.json'))
c=json.load(get('blobs/'+x['config']['digest']))
for h in c['history']:
 if not h.get('empty_layer'):print(h.get('created_by','')[:150],flush=True)
layer=x['layers'][5];p=root/'gcc-layer.tar.gz'
if not p.exists():
 with get('blobs/'+layer['digest'])as src,p.open('wb')as out:
  while chunk:=src.read(1024*1024):out.write(chunk)
assert hashlib.file_digest(p.open('rb'),'sha256').hexdigest()==layer['digest'].split(':')[1]
with tarfile.open(p) as archive:
 members=[m for m in archive if m.name.startswith('usr/local/')]
 print('selected files',len(members),'bytes',sum(m.size for m in members),flush=True)
 archive.extractall(root,members=members,filter='tar')
p.unlink()
print('compiler extracted',flush=True)
