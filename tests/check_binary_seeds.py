#!/usr/bin/env python3
"""Check an already built binary against independent full-scale references."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', required=True)
    parser.add_argument('--seeds', type=int, nargs='+', required=True)
    args = parser.parse_args()
    for seed in args.seeds:
        subprocess.run([sys.executable, str(ROOT / 'tests/oracle/generator.py'), '--seed',
                        str(seed), '--scale', 'full', '--out', '/data'], check=True)
        with tempfile.TemporaryDirectory() as expected:
            subprocess.run([sys.executable, str(ROOT / 'tests/oracle/reference.py'),
                            '--data', '/data', '--out', expected], check=True)
            want = (Path(expected) / 'aggregates.txt').read_bytes()
        for repeat in range(3):
            Path('/out/aggregates.txt').unlink(missing_ok=True)
            subprocess.run([args.binary], check=True)
            assert Path('/out/aggregates.txt').read_bytes() == want, (seed, repeat)
        print(f'full seed {seed}: three exact comparisons of {args.binary}', flush=True)


if __name__ == '__main__':
    main()
