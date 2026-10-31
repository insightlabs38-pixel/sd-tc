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


def _crc32c_table():
    table = []
    for i in range(256):
        value = i
        for _ in range(8):
            value = (value >> 1) ^ (0x82F63B78 if value & 1 else 0)
        table.append(value)
    return table


CRC32C_TABLE = _crc32c_table()


def crc32c(data):
    value = 5381
    for byte in data:
        value = CRC32C_TABLE[(value ^ byte) & 255] ^ (value >> 8)
    return value


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
        b'1,"HWIGCKWZQ","",7,1.00\n'
        b'2,"BUBZVTFQS","",7,2.00\n'
        b'3,"HWIGCKWZQ","",7,3.00\r\n'
    )
    assert crc32c(b"HWIGCKWZQ") == crc32c(b"BUBZVTFQS")
    check_case("colliding-name-hashes")
    DATA.joinpath("regions.csv").write_bytes(b"region_id,country\n7,AAA\n")
    DATA.joinpath("records.csv").write_bytes(
        b"id,name,note,region_id,amount\n"
        b'1,plain,"",7,1.00\n'
        b'2,"plain",,7,2.00\n'
        b'3,"a""b","",7,3.00\n'
        b'4,"a""b",,7,4.00\n'
        b'5,"comma,inside","",7,5.00\n'
        b'6,"comma,inside",,7,6.00\n'
    )
    check_case("canonical-raw-name-equivalence")
    DATA.joinpath("regions.csv").write_bytes(b"region_id,country\n7,AAA\n")
    DATA.joinpath("records.csv").write_bytes(
        b"id,name,note,region_id,amount\n"
        b'1,"known","",7,1.00\n'
        b'2,"wrapped","",4294967303,-0.05\n'
    )
    check_case("wide-unknown-region-id")
    DATA.joinpath("regions.csv").write_bytes(b"region_id,country\n7,AAA\n")
    DATA.joinpath("records.csv").write_bytes(
        b"id,name,note,region_id,amount\n"
        b'12345678,"eight","",7,1.00\n'
        b'12345679,"eight",,7,-0.01\r\n'
    )
    check_case("eight-digit-id-fast-path")



def money_swar_boundaries():
    DATA.joinpath("regions.csv").write_bytes(b"region_id,country\\n7,AAA\\n")
    DATA.joinpath("records.csv").write_bytes(
        b"id,name,note,region_id,amount\\n"
        b'1,"a","",7,0.00\\n'
        b'2,"a","",7,-0.01\\r\\n'
        b'3,"b","",7,99999.99\\n'
        b'4,"b","",7,-999.99\\r\\n'
        b'5,"c","",7,123456.78\\n'
        b'6,"c","",7,-123456.78\\r\\n'
        b'7,"d","",7,1.00'
    )
    check_case("money-swar-widths-sign-and-eof")


def quote_vector_boundary_stress():
    DATA.joinpath("regions.csv").write_bytes(b"region_id,country\n7,AAA\n8,BBB\n")
    rows = bytearray(b"id,name,note,region_id,amount\n")
    for i, width in enumerate((0, 1, 14, 15, 16, 17, 30, 31, 32, 33, 46, 47, 48, 49), 1):
        name = b'"' + b"A" * width + b'""Z"'
        note = b'"' + b"B" * width + b'""Q\nR"'
        rows.extend(str(i).encode() + b"," + name + b"," + note + b",7,-0.05\r\n")
        rows.extend(str(i + 100).encode() + b"," + name + b"," + note + b",8,1.00\n")
    DATA.joinpath("records.csv").write_bytes(rows)
    check_case("quote-vector-boundary-and-escaped-pairs")


def packed_metadata_stress():
    prefix = b"A" * 32
    left, right = prefix + b"HWIGCKWZQ", prefix + b"BUBZVTFQS"
    assert crc32c(left) == crc32c(right)
    short = b"A" * 17
    target = crc32c(short) & 0xfffe0000
    longer = None
    for i in range(300000):
        candidate = short + str(i).encode()
        if crc32c(candidate) & 0xfffe0000 == target:
            longer = candidate
            break
    assert longer is not None
    names = [left, right, short, longer] + [b"Z" * n for n in range(34)]
    DATA.joinpath("regions.csv").write_bytes(b"region_id,country\n7,AAA\n8,BBB\n")
    rows = bytearray(b"id,name,note,region_id,amount\n")
    for i in range(70000):
        name = names[i % len(names)]
        rows.extend(str(i + 1).encode() + b',"' + name + b'","x",' +
                    (b"7" if i % 3 else b"8") + b",-0.05\r\n")
    DATA.joinpath("records.csv").write_bytes(rows)
    check_case("packed-metadata-lengths-and-full-hash-collisions")


def huge_name_arena_stress():
    # Both large-arena storage and >24-bit logical lengths must remain exact.
    bom = b"\xef\xbb\xbf"
    DATA.joinpath("regions.csv").write_bytes(b"region_id,country\n7,AAA\n8,BBB\n")
    for width in ((1 << 20) + 17, (1 << 24) + 17):
        name = b'"' + b"A" * width + b'""B"'
        DATA.joinpath("records.csv").write_bytes(
            bom + b"id,name,note,region_id,amount\r\n"
            + b'1,' + name + b',"note",7,1.00\n'
            + bom + b'2,' + name + b',"",7,-0.05\r\n'
            + b'3,"",,8,0.00\n'
        )
        command("/tmp/sd-track-c")
        expected = (f"AAA 2 3 0.95 -0.05 1.00 1 {2 * (width + 2)} 4\n".encode()
                    + b"BBB 1 3 0.00 0.00 0.00 1 0 0\n")
        if (OUT / "aggregates.txt").read_bytes() != expected:
            raise AssertionError(f"huge-name-arena-{width}: output differs from exact bytes")
        print(f"huge-name-arena-{width}: exact, long name and worker boundaries", flush=True)


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
    # No mid-file BOM: exercise parity fallback across the long quoted record.
    for i in range(24002, 48002):
        rows.extend(f'{i},"name {i % 10}","short",7,1.00'.encode())
        rows.extend(b"\r\n" if i % 2 else b"\n")
    start = len(header)
    workers = int(BIN.read_text().split("unsigned threads = end - p < (1 << 20) ? 1 : ")[1].split(";")[0])
    if not any(long_start < start + (len(rows) - start) * i // workers < long_end
               for i in range(1, workers)):
        raise AssertionError("long quoted record missed actual worker partition boundary")
    DATA.joinpath("records.csv").write_bytes(rows)
    check_case("long-quoted-record-crosses-worker-boundary")


def grow_stress():
    DATA.joinpath("regions.csv").write_bytes(b"region_id,country\n7,AAA\n")
    rows = bytearray(b"id,name,note,region_id,amount\n")
    for i in range(1, 220001):
        rows.extend(f'{i},"unique {i}","",7,1.00\n'.encode())
    DATA.joinpath("records.csv").write_bytes(rows)
    check_case("sharded-name-table-grow")


def merge_collision_stress():
    bom = b"\xef\xbb\xbf"
    DATA.joinpath("regions.csv").write_bytes(b"region_id,country\n7,AAA\n8,BBB\n")
    rows = bytearray(b"id,name,note,region_id,amount\n")
    names = ("kawjkkxlukqs", "sttpibhnmhzv", "mduogtndfxub", "hfjiuhcjyfgb",
             "eqojekuinlac", "xwawwbtxtomu")
    for i in range(1, 50001):
        if i % 10000 == 1 and i > 1:
            rows.extend(bom)
        name = names[i % len(names)]
        note = "a\nb" if i % 4096 == 0 else "short"
        ending = "\r\n" if i % 2 else "\n"
        rows.extend(f'{i},"{name}","{note}",{7 if i % 3 else 8},1.00{ending}'.encode())
    DATA.joinpath("records.csv").write_bytes(rows)
    check_case("cross-worker-colliding-names")


def concentrated_shard_stress():
    # Exercise the collision-safe overflow path with >4096 names in one shard.
    names = []
    for i in range(400000):
        value = f"n{i}"
        h = crc32c(value.encode())
        spread = h ^ (h >> 11) ^ (h >> 22)
        if (spread & 63) == 0:
            names.append(value)
            if len(names) == 4200:
                break
    assert len(names) == 4200
    DATA.joinpath("regions.csv").write_bytes(b"region_id,country\n7,AAA\n")
    rows = bytearray(b"id,name,note,region_id,amount\n")
    for i in range(42000):
        rows.extend(f'{i + 1},"{names[i % len(names)]}","",7,1.00\n'.encode())
    assert len(rows) > (1 << 20), "exercise concurrent four-worker overflow"
    DATA.joinpath("records.csv").write_bytes(rows)
    check_case("concentrated-name-shard-overflow")


def main():
    DATA.mkdir(exist_ok=True)
    OUT.mkdir(exist_ok=True)
    command("g++", "-O3", "-march=x86-64-v3", "-std=c++17", "-DNDEBUG", "-flto", "-Wall", "-Wextra",
            "-Werror", "-o", "/tmp/sd-track-c", str(BIN))
    handwritten()
    money_swar_boundaries()
    quote_vector_boundary_stress()
    packed_metadata_stress()
    split_boundary_stress()
    huge_name_arena_stress()
    grow_stress()
    concentrated_shard_stress()
    merge_collision_stress()
    for seed, scale in [(1, "tiny"), (2, "tiny"), (3, "tiny"),
                        (2, "sample"), (1, "sample"), (1, "full")]:
        command(sys.executable, str(GEN), "--seed", str(seed), "--scale", scale,
                "--out", str(DATA))
        check_case(f"{scale}-seed{seed}",
                   FULL_HASH if scale == "full" else SAMPLE_HASH
                   if scale == "sample" and seed == 1 else None)


if __name__ == "__main__":
    main()
