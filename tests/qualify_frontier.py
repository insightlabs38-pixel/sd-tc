#!/usr/bin/env python3
"""Sanitizer and independent-seed qualification; never an official timing receipt."""
import argparse
import os
import subprocess
import sys
import test_track_c as cases


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--sanitizer', choices=('address,undefined', 'thread'), required=True)
    args = parser.parse_args()
    flags = ['-O1', '-g', '-march=x86-64-v3', '-std=c++17', '-pthread',
             '-fno-omit-frame-pointer', '-fno-pie', '-no-pie', '-fsanitize=' + args.sanitizer]
    subprocess.run(['g++', *flags, str(cases.BIN), '-o', '/tmp/sd-track-c'], check=True)
    os.environ['ASAN_OPTIONS'] = 'detect_leaks=1:halt_on_error=1'
    os.environ['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
    os.environ['TSAN_OPTIONS'] = 'halt_on_error=1'
    cases.DATA.mkdir(exist_ok=True)
    cases.OUT.mkdir(exist_ok=True)
    cases.handwritten()
    cases.quote_vector_boundary_stress()
    cases.packed_metadata_stress()
    cases.split_boundary_stress()
    cases.huge_name_arena_stress()
    cases.grow_stress()
    cases.merge_collision_stress()
    for seed, scale in [(19, 'tiny'), (37, 'sample')]:
        cases.command(sys.executable, str(cases.GEN), '--seed', str(seed), '--scale', scale,
                      '--out', str(cases.DATA))
        cases.check_case(f'{args.sanitizer}-{scale}-seed{seed}')


if __name__ == '__main__':
    main()
