# Local reproduction: A+C research kernels

Environment used: x86_64 Linux, AMD EPYC 9V74, container exposes 5 CPUs (0–4), g++ (Debian) 14.2.0, Python 3.13.5. `-march=native` is available locally, but C scoring Docker uses GCC 15 `-march=x86-64-v3` with PGO. No official dataset or full CI was used.

Build:

```sh
cd prototypes
g++ -O3 -march=native -std=c++20 -pthread a_select.cpp -o a_select
g++ -O3 -march=native -std=c++20 a_record5.cpp -o a_record5
g++ -O3 -march=native -std=c++20 a_output_copy.cpp -o a_output_copy
g++ -O3 -march=native -std=c++20 -pthread c_distinct.cpp -o c_distinct
g++ -O3 -march=x86-64-v3 -std=c++20 c_structural.cpp -o c_structural_v3
```

Test (synthetic only):

```sh
taskset -c 0 ./a_select 3100000 11
./a_record5 3100000 44
taskset -c 0 ./a_output_copy 126908
./c_distinct 3320000 4 19
python3 gen_hostile_csv.py 500000 61 /tmp/hostile.csv
./c_structural_v3 /tmp/hostile.csv --verify
taskset -c 0 ./c_structural_v3 /tmp/hostile.csv
```

`c_structural_v3 --verify` checks every decoded field including name CRC, not merely one checksum. The exact synthetic generator is a deliberately targeted proxy: not a reproduction of the organizer's seed generator. The SIMD structural parser needs an unbounded fallback when integrated; its fixed 64-mask prototype capacity is NOT a production size guarantee. The v3 build uses function-specific `target("pclmul")`, not a global compiler change. Software-only 64-bit parity implementation is also provided.

Median receipts: `receipts/pinned_summary.txt` (nine repeats A selector, C CSV without name hash, C name-ID distinctness); `receipts/c_lexical_v3_with_hash.txt` (five pinned parser+CRC repeats); `receipts/a_record_pack.txt`, `receipts/a_output_copy.txt`. All recorded timings are kernel times only and not official complete-K qualification.

Full data generation details are in the research report; `gen_hostile_csv.py` randomizes comma/quote/newline content, BOMs, signed cents and empty fields. The canonical organizer reference must be used by the developers before any promotion.