from pathlib import Path
import argparse, importlib.util, subprocess, os, sys

ROOT = Path(__file__).resolve().parents[2]
r = ROOT/'work/independent-qualification'
r.mkdir(parents=True,exist_ok=True)
p = argparse.ArgumentParser()
p.add_argument('--source',type=Path,required=True)
p.add_argument('--sanitizer', default='')
a = p.parse_args()
a.name=a.source.stem
spec = importlib.util.spec_from_file_location('cases', ROOT / 'tests/test_track_c.py')
cases = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cases)
data, out = r/'qualification-data', r/'qualification-out'
data.mkdir(exist_ok=True)
out.mkdir(exist_ok=True)
cases.DATA, cases.OUT = data, out
source = r/(a.name+'-qualify.cpp')
source.write_text(a.source.read_text().replace('/data/',str(data)+'/').replace('/out/',str(out)+'/'))
binary = r/(a.name+'-qualify')
flags = ['-O1','-g','-pthread','-march=x86-64-v3','-std=c++17','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if a.sanitizer else ['-O3','-pthread','-march=x86-64-v3','-std=c++17','-DNDEBUG']
if a.sanitizer: flags += ['-fsanitize='+a.sanitizer]
subprocess.run(['g++',*flags,str(source),'-o',str(binary)],check=True)
original = cases.command
def command(*args):
    if args[0] == '/tmp/sd-track-c': args = (str(binary),*args[1:])
    original(*args)
cases.command = command
os.environ['ASAN_OPTIONS'] = 'detect_leaks=1:halt_on_error=1'
os.environ['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
os.environ['TSAN_OPTIONS'] = 'halt_on_error=1'
for name in ['handwritten','page_tail_stress','money_swar_boundaries','page_boundary_tail_stress',
             'quote_vector_boundary_stress','packed_metadata_stress','split_boundary_stress',
             'huge_name_arena_stress','grow_stress','capacity_cutover_stress',
             'concentrated_shard_stress','merge_collision_stress']:
    getattr(cases,name)()
for seed, scale in [(19,'tiny'),(37,'sample')]:
    command(sys.executable,str(cases.GEN),'--seed',str(seed),'--scale',scale,'--out',str(data))
    cases.check_case(a.name+'-'+a.sanitizer+'-'+scale+str(seed))
