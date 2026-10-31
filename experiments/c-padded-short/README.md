# Padded mappings and exact short-name paths

Experimental. Base f294a88. Guard page permits bounded-width reads at file tail.
Short names are zero-masked to16 bytes, CRC32C hashed with two fixed eight-byte
operations and compared with one masked SSE equality. Full bytes still decide
identity, including all CRC collisions; length filtering and long fallback remain.
This changes the hash for short names; equal-length existing CRC collision
fixtures remain collisions after applying the same zero-padding transform.
No probabilistic equality. New mapping has explicit anonymous guard and cleanup.

Local single-worker GCC14 non-PGO12 interleaved runs:
control284.082ms/282.239CPUms, padded296.391/293.213,
padded+SSE295.995/293.822, padded+fixedCRC268.213/267.039,
combined259.739/257.882. RSS10,944->10,952KiB.
All full seed1 hashes exact. PGO/sanitizer/independent gates pending.
Multiworker initial mask caching and padding-only did not win; see raw receipts.
The full structural-mask scanner separately lost substantially; an exact guard
and fixed short-key realization is a new hypothesis, not a tokenizer benchmark.
