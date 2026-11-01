# Private radix field solver

Bounded8-byte descriptors, exact wide escapes, exclusively owned radix-head
dictionaries with dynamically promoted16-bit chains and contiguous canonical
slots. No shared name dictionary or shared membership writes. All quoted
fields are decoded by the independently implemented field-mask parser.

The full hostile ASan/UBSan and TSan suites pass locally. Native repaired
solver140.758ms versus149.572 champion. Pinned offline PGO is152.026ms
versus156.468; fresh-container peaks12,451,840 versus10,952,704 bytes.
Runtime gains alone do not establish complete K; source penalty and CI
scaling remain under evaluation. Do not promote before those measurements.

Profile worker counts2/3/4/6/8/12 are total parser+owner budgets. Tiny input
still needs one parser plus one consumer. Six means4parsers/2owners;12 means
8parsers/4owners. This is an ownership architecture, not private interning
followed by a global shared dictionary. Sources and repair receipts: PR15.

Rollback e253190 (official source-equivalent6917275); Luna owns integration.
