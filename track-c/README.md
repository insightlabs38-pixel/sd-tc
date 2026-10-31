# Track C: Join

A byte-oriented C++17 parser reads the hostile CSV, maps region ids through a dense table, and aggregates country statistics. Global exact-name interning uses CRC32C over canonical raw CSV name bytes, exact raw-byte collision checks, and four 64-bit country-presence words per name. Escaped quotes are counted for logical byte lengths without per-row allocations. Integer fields use a guarded eight-digit SWAR fast path with a scalar fallback for longer values. Input files are memory-mapped read-only, and output is written only to `/out/aggregates.txt`.

No third-party runtime dependencies. Further work: benchmark full-scale seeds and evaluate a streaming name representation, SIMD parsing, and safe parallel record partitioning against the complete K coefficient.

## Frontier handoff

Copy the entire `track-c/` context from one qualified commit. The official rollback Dockerfile does not enable CRC32C; source-only cherry-picks fail to compile. This Dockerfile requires `-march=x86-64-v3`. PGO branches additionally require their unchanged `generator.py` and three-stage Dockerfile. Keep the official champion until all board gates and complete K improve.
