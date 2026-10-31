# Byte controls and exact physical-slot keys

Experimental revision of qualified365b8f3, not a champion.
Atomic byte controls hold short length and three hash bits; sixteen-byte keys
carry exact short bytes or long pointer+full length+prefix. Both arrays are
prefetched by home slot during parsing. Identity always compares full bytes.
This removes unneeded key-line reads for rejected probes, reduces key storage
from3MB to2MB, and preserves the tested busy/capacity/overflow protocol.
Country-major membership uses physical slot IDs. No vector access to atomic
control bytes. Three-stage lag16, six workers, THP disabled, prefault keys/control.

Local GCC14 non-PGO12 alternating full-seed1 runs:
main169.574ms/320.004CPUms, slot24167.313/324.375,
byte177.009/333.383. RSS41,196/30,612/29,636KiB varies with reclaimed input.
All36 output hashes exact. PGO/sanitizers/holdouts pending CI; do not project K.
