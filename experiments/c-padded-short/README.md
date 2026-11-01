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

Second12-pair single-worker run: control292.400/290.104CPUms,
combined256.762/254.155, guardedquotes254.216/251.953,
guardedquotes+money241.357/240.909. All outputs exact.
Current candidate uses guarded quote scanner and one-fold SWAR money path
for whole parts1..5 digits; larger amounts preserve original int64 fallback.
Page-aligned EOF tests exercise empty names and amounts without a final LF.

## Qualified source90da546 (runtime unchanged from4e915db)

CI https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41251942615 passed
all gates: tiny/sample/full reference, concentrated CRC shards (updated fixture
CRC for padding), full hash collisions, wide money/IDs, long records, guarded
page-tail EOF cases, ASan/UBSan, TSan, pinned offline Docker/sandbox, and final
PGO full seeds11/29 x3. Worker profile1/2/3/4/6/8/12 x5 full hashes exact.
FullPGO paired vs main f294a88: 68.679->59.927ms, RSS16,776->35,752KiB.
Vs official BEST59e47: 72.254->60.703ms, RSS16,924->40,772KiB.
Canonical LLVM-format/scc S622 vs main577/official552, D1.
RSS reflects mapped-file PTE residency and is not cgroup peak M.

Local pinned-image GCC15.3.0 extracted from verified amd64 manifest
df8a4d9d058f59c2a4ee8cca45e59cea987f442fdb348aa6258357886a13afa8
of frozen GCC digest eb5e64286ad47187b32bda75c6b6916166ecb41da56559b8b09e0001748d2cce.
Only compiler layer89609dd4f2480f888a687c4a426a0da6df53ce8433ff2d51962984ed305565ea
was extracted (SHA verified), avoiding Docker VFS multi-layer duplication.
Local static PGO seed37; host libc/linker, final package gates use actual CI Docker.
Twelve paired pinned-Debian restricted containers, quota2CPU, seed1:
main145.858ms / cgroup10,168,320B / RSS28,792KiB;
challenger129.350ms / cgroup10,149,888B / RSS30,818KiB.
Complete local K ratio=(145.858/129.350)^.5*(10168320/10149888)^.2
*(577/622)^.15 = approximately1.050; this is NOT official K.
CI proxy with M assumed flat approximately1.058 vs main,1.073 vs official BEST.
No actual official challenger result yet. Main and official BEST rollback retained.

Integration: copy complete track-c runtime context including PGO Dockerfile and
unchanged generator; preserve -march=x86-64-v3 compiler flag. Do not add a source
target pragma. No changes to other tracks or sd-sm. PR8/PR2 are weaker alternatives;
this change is based on current main and preserves its128-shard exact dictionary.
Next: combine with compact pending records, cache-line alignment, byte-control
country-major pipeline, and dynamic scheduling; test six/twelve on official CPU.
