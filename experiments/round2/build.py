from pathlib import Path
import subprocess,sys
p=Path(sys.argv[1]); out=Path(sys.argv[2]); s=p.read_text().replace('"/data/', '"/workspace/work/data/').replace('"/out/', '"/workspace/work/out/')
Path(str(out)+'.local.cpp').write_text(s)
subprocess.run(['g++','-O3','-march=x86-64-v3','-std=c++17','-DNDEBUG','-flto','-pthread','-Wall','-Wextra','-Werror','-g','-o',str(out),str(out)+'.local.cpp'],check=True)
