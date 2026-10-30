# Track C: Join

A byte-oriented C++17 parser reads the hostile CSV, maps region ids through a dense table, and aggregates country statistics. Global exact-name interning uses an open-addressed table, with four 64-bit country-presence words per distinct name; quotes are decoded logically for hashing, equality, and byte counts without per-row allocations. Input files are memory-mapped read-only, and output is written only to `/out/aggregates.txt`.

No third-party runtime dependencies. Further work: benchmark full-scale seeds and evaluate a streaming name representation, SIMD parsing, and safe parallel record partitioning against the complete K coefficient.
