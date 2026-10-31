import pathlib,subprocess,sys,json,os
root=pathlib.Path('/workspace/work/pgo15');root.mkdir(exist_ok=True)
link=pathlib.Path('/workspace/work/pgo-data')
def point(path):
 link.unlink(missing_ok=True);link.symlink_to(path,target_is_directory=True)
gcc='/workspace/work/gcc15/usr/local/bin/g++'
flags=['-O3','-march=x86-64-v3','-std=c++17','-DNDEBUG','-flto','-pthread','-static']
point('/workspace/work/training37')
try:
 for arg in sys.argv[1:]:
  src=pathlib.Path(arg);target=root/src.stem;target.mkdir(exist_ok=True)
  code=src.read_text().replace('"/data/','"/workspace/work/pgo-data/').replace('"/out/','"/workspace/work/out/')
  p=target/'join.cpp';p.write_text(code);out=target/'join';profile=target/'profile'
  cmd=[gcc,*flags,'-o',str(out),str(p)]
  subprocess.run([*cmd,'-fprofile-generate='+str(profile),'-fprofile-update=atomic'],check=True)
  subprocess.run([str(out)],check=True)
  subprocess.run([*cmd,'-fprofile-use='+str(profile),'-fprofile-correction','-Werror=missing-profile','-Werror=coverage-mismatch'],check=True)
  (target/'flags.json').write_text(json.dumps(cmd))
  print(out,flush=True)
finally:point('/workspace/work/data')
