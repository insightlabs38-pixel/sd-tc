# Track C: Join

A byte-oriented C++17 parser reads the hostile CSV, maps region ids through a dense table, and aggregates country statistics. Global exact-name interning uses CRC32C over canonical raw CSV name bytes, exact raw-byte collision checks, and four 64-bit country-presence words per name. Escaped quotes are counted for logical byte lengths without per-row allocations. Integer fields use a guarded eight-digit SWAR fast path with a scalar fallback for longer values. Input files are memory-mapped read-only, and output is written only to `/out/aggregates.txt`.

No third-party runtime dependencies. Further work: benchmark full-scale seeds and evaluate a streaming name representation, SIMD parsing, and safe parallel record partitioning against the complete K coefficient.

## Integration

Copy this complete directory, including Dockerfile and generator.py, to preserve offline PGO. The source also declares its required x86-64-v3 target so a source-only import can compile with the rollback compiler command, but that command does not reproduce the trained PGO build.
