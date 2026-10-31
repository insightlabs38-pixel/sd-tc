# Structural64 initial realization

Experimental, not promoted. Control main f294a882897e2ceb940f8878306b30b80c5e9829.
Original 64-byte AVX2 masks for quote/comma/LF with software prefix parity;
quoted-byte counts derive decoded lengths without rescanning each escaped pair.
Guarded tail copy supports arbitrary file ends, long fields and missing final LF.
Exact shared dictionary and PGO Docker context unchanged.

GCC14 non-PGO local12 paired runs: control221.347ms/443.643CPUms;
candidate279.476ms/525.481CPUms. RSS36,124/40,330KiB, highly variable.
Both passed full seed1 every run. Candidate also passed the entire existing
adversarial/reference suite (correctness.txt). Not sanitizer or Docker qualified.
No official K claim; source growth plus runtime loss makes promotion unjustified.
Possible costs to investigate: six SIMD comparisons/block, software prefix XOR,
per-delimiter quote popcounts and five field descriptors per row. These are cost
hypotheses, not hardware-counter conclusions. Profile and test a quote-mask cache
that reuses structural work while avoiding full delimiter enumeration.
