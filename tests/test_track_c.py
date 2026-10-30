#!/usr/bin/env python3
"""Differential tests against the organizer's unchanged public generator/reference."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / "tests/oracle/generator.py"
REF = ROOT / "tests/oracle/reference.py"
BIN = ROOT / "track-c/join.cpp"
DATA = Path("/data")
OUT = Path("/out")
FULL_HASH = "d8088b1cd42ba1338b1345b107b55db7b96ef1f5066f3ff64fd26a8912a0f536"
SAMPLE_HASH = "9035fed36823232dd41ae2d1b9d6d21aa7c73f49d8e95647e953a89d5ebe9a66"


def command(*args):
    subprocess.run(args, check=True)


def check_case(label, reference_hash=None):
    with tempfile.TemporaryDirectory() as temp:
        expected = Path(temp) / "expected"
        command(sys.executable, str(REF), "--data", str(DATA), "--out", str(expected))
        command("/tmp/sd-track-c",)
        got = (OUT / "aggregates.txt").read_bytes()
        want = (expected / "aggregates.txt").read_bytes()
        if got != want:
            raise AssertionError(f"{label}: output differs from official reference")
        digest = hashlib.sha256(got).hexdigest()
        if reference_hash and digest != reference_hash:
            raise AssertionError(f"{label}: wrong published hash: {digest}")
        print(f"{label}: exact, {len(got)} output bytes, sha256={digest}", flush=True)


def handwritten():
    bom = b"\xef\xbb\xbf"
    DATA.joinpath("regions.csv").write_bytes(
        b"region_id,country\n3,BBB\n7,AAA\n12,AAA\n20,CCC\n21,DDD\n"
    )
    DATA.joinpath("records.csv").write_bytes(
        bom + b"id,name,note,region_id,amount\r\n"
        b'1,"Kaphar, Titus","said ""no"" twice",7,1420.00\r\n'
        b'2,"Betts","a note that\nspans two lines",7,88.50\n'
        b'3,,"",12,0.00\r\n'
        b'4,"trailing space ","comma, quote "" and newline\ntogether",3,-5.25\n'
        + bom + b'5,"Betts ","one\ntwo\nthree",12,-0.75\r\n'
        b'6,"",,12,10.00\r\n'
        + bom + b'7,"Ann ""Annie"" Lee","x",5,2.00\n'
        b'8,"Nobody","",4000000,-1.00\r\n'
        b'9,"Neg","n",20,-0.05\r\n'
    )
    check_case("worked-example")
    DATA.joinpath("regions.csv").write_bytes(b"region_id,country\n7,AAA\n8,BBB\n")
    DATA.joinpath("records.csv").write_bytes(
        b"id,name,note,region_id,amount\n"
        b'1,"a' + bom + b'b","x\n' + bom + b'y",7,1.00\n'
        b'2,"' + bom + b'","",7,-1.00\r\n'
        b'4,"d","",8,5.00\n'
        b'5,"d ","",8,-5.00\r\n'
        + bom + b'3,"c",,7,0.50'
    )
    check_case("quoted-bom-no-final-terminator")
    DATA.joinpath("regions.csv").write_bytes(b"region_id,country\n7,AAA\n")
    DATA.joinpath("records.csv").write_bytes(
        bom + b"id,name,note,region_id,amount\r\n"
        b'1,"kawjkkxlukqs","",7,1.00\n'
        b'2,"sttpibhnmhzv","",7,2.00\n'
        b'3,"kawjkkxlukqs","",7,3.00\r\n'
    )
    check_case("colliding-name-hashes")


def split_boundary_stress():
    bom = b"\xef\xbb\xbf"
    header = bom + b"id,name,note,region_id,amount\r\n"
    DATA.joinpath("regions.csv").write_bytes(b"region_id,country\n7,AAA\n8,BBB\n")
    rows = bytearray(header)
    for i in range(1, 24001):
        rows.extend(f'{i},"name {i % 10}","short",7,1.00'.encode())
        rows.extend(b"\r\n" if i % 2 else b"\n")
    long_start = len(rows)
    rows.extend(b'24001,"long","' + b'alpha\nbeta,said ""no""\n' * 5500
                + b'omega",8,-0.05\r\n')
    long_end = len(rows)
    rows.extend(bom)
    for i in range(24002, 48002):
        rows.extend(f'{i},"name {i % 10}","short",7,1.00'.encode())
        rows.extend(b"\r\n" if i % 2 else b"\n")
    start = len(header)
    if not any(long_start < start + (len(rows) - start) * i // 6 < long_end
               for i in range(1, 6)):
        raise AssertionError("long quoted record missed partition boundary")
    DATA.joinpath("records.csv").write_bytes(rows)
    check_case("long-quoted-record-crosses-worker-boundary")


def main():
    DATA.mkdir(exist_ok=True)
    OUT.mkdir(exist_ok=True)
    command("g++", "-O3", "-std=c++17", "-DNDEBUG", "-flto", "-Wall", "-Wextra",
            "-Werror", "-o", "/tmp/sd-track-c", str(BIN))
    handwritten()
    split_boundary_stress()
    for seed, scale in [(1, "tiny"), (2, "tiny"), (3, "tiny"),
                        (2, "sample"), (1, "sample"), (1, "full")]:
        command(sys.executable, str(GEN), "--seed", str(seed), "--scale", scale,
                "--out", str(DATA))
        check_case(f"{scale}-seed{seed}",
                   FULL_HASH if scale == "full" else SAMPLE_HASH
                   if scale == "sample" and seed == 1 else None)


if __name__ == "__main__":
    main()
