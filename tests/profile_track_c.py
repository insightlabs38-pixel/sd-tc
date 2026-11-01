#!/usr/bin/env python3
"""Unscored, full-dataset phase and worker-count comparison for Track C."""
import hashlib
from pathlib import Path
import re
import statistics
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "track-c/join.cpp"
EXPECTED = "d8088b1cd42ba1338b1345b107b55db7b96ef1f5066f3ff64fd26a8912a0f536"


def instrument(source, workers):
    match = re.search(r"unsigned threads = end - p < \(1 << 20\) \? 1 : \d+;", source)
    assert match is not None
    needle = match.group(0)
    assert source.count(needle) == 1
    source = source.replace(needle, f"unsigned threads = end - p < (1 << 20) ? 1 : {workers};")
    source = re.sub(r"^\s*threads = std::min\(threads, allowed_threads\(\)\);\n", "\n        (void)allowed_threads;\n", source, flags=re.M)
    source = "#include <chrono>\n" + source
    markers = [
        ("int main() {\n    try {", "\n        const auto t0 = std::chrono::steady_clock::now();"),
        ('        File records("/data/records.csv");', "        const auto t_lookup = std::chrono::steady_clock::now();\n"),
        ("        std::vector<Worker> workers;", "        const auto t_boundary = std::chrono::steady_clock::now();\n"),
        ("        std::vector<Group> groups(unmatched + 1);", "        const auto t_parse = std::chrono::steady_clock::now();\n"),
        ("        std::vector<unsigned> order;", "        const auto t_merge = std::chrono::steady_clock::now();\n"),
    ]
    for needle, insertion in markers:
        assert source.count(needle) == 1, needle
        source = source.replace(needle, needle + insertion if needle.startswith("int main") else insertion + needle)
    needle = re.search(r'        if \(fclose\(out\)\)\s+throw std::runtime_error\("close output"\);', source).group(0)
    assert source.count(needle) == 1
    report = r'''
        const auto t_output = std::chrono::steady_clock::now();
        auto ms = [](auto a, auto b) {
            return std::chrono::duration<double, std::milli>(b - a).count();
        };
        fprintf(stderr, "SD_PROFILE lookup=%.3f boundary=%.3f parse=%.3f merge=%.3f output=%.3f total=%.3f\n",
                ms(t0, t_lookup), ms(t_lookup, t_boundary), ms(t_boundary, t_parse),
                ms(t_parse, t_merge), ms(t_merge, t_output), ms(t0, t_output));'''
    return source.replace(needle, needle + report)


def main():
    source = SOURCE.read_text()
    counts = (1, 2, 3, 4, 6, 8, 12)
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        for workers in counts:
            src = root / f"join-{workers}.cpp"
            src.write_text(instrument(source, workers))
            subprocess.run(["g++", "-O3", "-march=x86-64-v3", "-std=c++17", "-DNDEBUG", "-flto", "-pthread",
                            "-Wall", "-Wextra", "-Werror", "-o", str(root / f"join-{workers}"),
                            str(src)], check=True)
        results = {count: [] for count in counts}
        for trial in range(5):
            for workers in (counts if trial % 2 == 0 else tuple(reversed(counts))):
                run = subprocess.run(["/usr/bin/time", "-f", "SD_RSS=%M", str(root / f"join-{workers}")],
                                     capture_output=True, text=True, check=True)
                digest = hashlib.sha256(Path("/out/aggregates.txt").read_bytes()).hexdigest()
                assert digest == EXPECTED, (workers, trial, digest)
                match = re.search(r"SD_PROFILE (.+)\nSD_RSS=(\d+)", run.stderr)
                assert match, run.stderr
                metrics = {k: float(v) for k, v in re.findall(r"(\w+)=([\d.]+)", match.group(1))}
                metrics["rss_kib"] = int(match.group(2))
                results[workers].append(metrics)
        for workers in counts:
            summary = {key: round(statistics.median(row[key] for row in results[workers]), 3)
                       for key in results[workers][0]}
            print(f"workers={workers} median_of_5 {summary}", flush=True)


if __name__ == "__main__":
    main()
