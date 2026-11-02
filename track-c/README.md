# Track C: Join

A byte-oriented C++17 parser reads the hostile CSV, maps region ids through a dense table, and aggregates country statistics. Global exact-name interning uses CRC32C over canonical raw CSV name bytes, exact raw-byte collision checks, and four 64-bit country-presence words per name. Escaped quotes are counted for logical byte lengths without per-row allocations. Integer fields use a guarded eight-digit SWAR fast path with a scalar fallback for longer values. Input files are memory-mapped read-only, and output is written only to `/out/aggregates.txt`.

Six workers use fast record-start candidates, retaining exact-aligned cuts; if a guessed boundary truncates a quoted record, a clean single-worker restart validates the entire input before any output is published. Adversarial split cases, repeated full seeds, sanitizers and read-only Docker are CI-qualified. No third-party runtime dependencies. Official six-P-core K remains to be measured by the integrator.

## Integration

Copy this complete directory, including Dockerfile and generator.py, to preserve offline PGO. Source-only builds require `-march=x86-64-v3` for CRC32C intrinsics, as in the pinned Dockerfile and CI; an unmodified baseline x86-64 compile fails. Source-only builds do not reproduce the trained PGO binary.
