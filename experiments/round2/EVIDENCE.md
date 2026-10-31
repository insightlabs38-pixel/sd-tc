# Track C round 2 checkpoint

Official baseline: control-byte inline table, source d1189c62ac6d6e56ba034e57919f0de3dc0f5128. BEST run-955f007ac420: K=20.321199, T=84.295 ms, M=12,238,848 B, S=537, D=1. Refreshed board at 18:10 UTC; official source unchanged. B BEST K=23.169218. C leader NickTrapp K=47.456703, T=18.032 ms, M=12,787,712 B.

No frontier source in this checkpoint has an official scoring receipt. Luna alone controls integration/submission. Developer main was refreshed to 646c0e3; its wide CRC tail is independently present in the frontier combination. No main merge, force-push or sd-sm write occurred.

## Qualified candidates

| Source commit | Change | CI | Paired runner ms | Canonical S | D |
|---|---|---|---|---|---|
| 658d060228e1668b9d576f8247e657e311d4cdfe | Certified cuts + CRC, official compact table | 41227488100; sanitizers 41227734232 | 149.626 -> 133.981 | 537 | 1 |
| dcb50168fcfb8b8b75b6eebc8c03976c589d056e | SWAR + wide CRC tails | 41227918470 | 97.164 -> 85.725 | 570 | 1 |
| b7bb0d5c32b2aa06cd1d046d4dabf31d65cd0389 | Dense 56-byte inline pools and compact index | 41228157740 | 148.377 -> 129.688 | 545 | 1 |
| 8bb0bfe | Fused quote masks over dense pools | 41229296022 | 87.050 -> 75.863 | pending registry count | 1 |
| 8f0acff | Twelve-worker dense pools | 41229385026 | 139.039 -> 114.767 | 545 | 1 |

Each listed CI passed tiny/sample/full differential references, long/collision/overflow cases, pinned offline GCC 15 Docker and sandbox. Separate ASan/UBSan and TSan jobs passed independent tiny seed 19/sample seed 37 and hostile/overflow/long records. Seed 1 full hash remains d8088b1cd42ba1338b1345b107b55db7b96ef1f5066f3ff64fd26a8912a0f536.

Full seeds 11 and 29: three repeated comparisons per source for champion, CRC, wide-tail SWAR, dense, dense+quotes, structural/SWAR, compact radix and inline-owner designs; all exact. The independently trained PGO binary also matched both. Full reference hashes and source SHA-256 values are in full-independent.log. Experimental radix/owner sources are not sanitizer-qualified or recommended for submission.

## Complete-K selection

K proxy multiplies paired T^-0.50, M^-0.20 and S^-0.15; D is unchanged. Canonical S uses the exact organizer clang-format 23.1.3 and scc 4.1.0, whose published download checksums matched. It reproduces official baseline S=537. Runner RSS is not official cgroup M.

Local development is AMD EPYC with a two-CPU quota and three visible CPUs, not six physical Intel cores/twelve threads. Docker results below use static host-compiler binaries in the pinned Debian image, network disabled, root read-only, nobody user and a two-CPU quota. A parent launcher adds small common overhead. These measurements are exploratory and require official confirmation; CPU quota scheduling causes material variance. Pinned GCC 15 CI independently resolves the prior VFS Docker qualification gap.

| Variant | Median T ms | Cgroup memory.peak B | S |
|---|---|---|---|
| Official-source baseline | 161.946 | 11,608,064 | 537 |
| Direct SWAR/wide CRC | 150.736 | 11,470,848 | 570 |
| Dense inline pool | 153.183 | 9,719,808 | 545 |
| Dense 12-byte inline keys | 160.189 | 9,111,552 | 544 |
| Development main rollback | 155.712 | 10,158,080 | not used |

Dense pools win this complete local comparison: +6.3% K proxy versus official source and about +2.7% versus direct SWAR. The paired CI proxy is about +9.2%, using RSS. Neither reaches B on a reliable official estimate. Rollback remains d1189c62; PRs #6/#7/#8 preserve alternatives. Twelve-worker source needs controlled official SMT measurement; runner scaling is not conclusive.

A separate paired run measured dense 151.037 ms/9,736,192 B, dense+quote masks 158.008 ms/9,523,200 B, sample-trained host-GCC PGO 149.308 ms/9,566,208 B. Quote masks lose locally after source cost; keep as a qualified alternative rather than automatically promote. Full-seed, pinned-GCC PGO is being checked independently.

## Architectural experiments and causes

- Cached quote masks and full structural scanning did not beat the simpler parser after source cost. Structural+bounded SWAR reached 176.181 vs 173.043 ms for direct SWAR; duplicated delimiter processing/popcounts did not pay for themselves.
- Initial hash-owner queues used 32-byte pointer messages and active polling. Local 2-parser/4-owner throughput was 306.641 vs 183.635 ms. Fewer workers and index/entry prefetching did not rescue it.
- A direct owner table with prefetching still lost: 326.742/509.365 ms for 1+1/2+2 versus 165.818 ms shared two-worker parsing.
- Blocking queues and compact eight-byte messages improved ownership to 213.061 ms vs 180.432, then 207.678 vs 179.751 with prefetching. Inline messages did not reverse the outcome. This removes polling as the sole explanation; payload handoff and owner lookup remain expensive.
- Eight-byte references plus in-place radix partitioning were exact but slow: 306.649 ms, improved to 255.736 with packed field lengths/inline keys, versus 187.739 ms direct SWAR. Thread CPU profiling measured only ~22 ms partitioning; deferred dictionaries cost 391 ms, reduced to 268 ms. Cold rereads of record payloads are the likely cache cost, not partition bandwidth alone.
- Separate immutable keys/mutable membership measured 200.469 vs 188.286 ms for dense inline pools. Additional dependent loads outweighed the immutable-key cache benefit.

Hardware perf_event_open returned ENOENT, so cache-miss/branch/stall counters were unavailable. Cache explanations above are hypotheses supported by phase timing and representation changes, not measured hardware events. No claim of official performance or exhaustive microarchitectural profiling is made.

## Continuation

1. Finish pinned same-compiler full-seed PGO comparison and preserve both plain/PGO results.
2. Ask Luna to score dense six/twelve-worker variants; do not substitute local results for official T/M/S/D/K.
3. Use the best official source as the next baseline. Preserve the current champion if controlled scoring rejects candidates.
4. Owner/radix implementations remain recoverable in this directory, with exact source blobs in manifest.json. They need long-record/sanitizer qualification before any promotion.

## Final checkpoint after user requested wrap-up

All work remains on independent frontier branches. No incomplete experiment PR was opened. New incomplete sources were pushed separately as `frontier/round2-quote-end-wip`, `frontier/round2-exact-key-wip`, and `frontier/round2-owner24-wip`, each with WIP.md and no PR. Their prototype measurements are not qualification of the final pragma-free WIP revisions.

**Official best achieved:** `run-15adbc1f561c`, official commit `8df4a7a5c066828cbd667528fd4a4ce26ed11c8b`, packed source blob `59974fd6328c2fabb1154ac4cc31bda1036bc6ca`: T=76.727 ms, M=14,487,552 B, S=552, D=1, K=20.508340. All five gates passed. This is a modest official gain; the larger local/CI proxy did not transfer, and C remains below B's recorded BEST.

The source CPU pragma trial (`23bf44f`, blob `78503e708dccdbf484bb5dc1223a02705cfabb81`) passed all gates but officially regressed to T=96.269 ms, M=13,807,616 B, S=553, K=18.480714 in `run-9722acb5cb38`. It was rejected. PR #11 is restored at **c88f0f6e5403e9a7b6472e68560ddd9ee6fafa21**: join.cpp, Dockerfile and generator.py match board-tested 59e47be byte for byte. No CPU pragma remains in that runtime context. Complete-context handoff remains mandatory.

Earlier official PR #7/#8 failures were reproduced: Luna had retained the rollback Dockerfile without `-march=x86-64-v3` while copying CRC-intrinsic source. The qualified Dockerfile already carried the flag. Diagnosis and reproduction were sent to PRs #7/#8/#9 and issue #10; Luna subsequently queued #11 with its complete PGO context, which built and scored. The pragma workaround did not earn promotion.

**Next qualified but officially unscored source:** `frontier/round2-packet40-pgo`, final checkpoint **b6e48dff0573a4cd28894c030df89b9dd8db3f6f**. Runtime files match CI-tested a99ff4d byte for byte; the rejected pragma and its default-flags check were removed. No PR was opened for this branch during wrap-up. CI [41232711351](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41232711351) passed pinned PGO, sandbox, ASan/UBSan, TSan, large signed-money/ID fallback cases, full-hash/metadata collisions, and final full seeds 11/29 (three repeats each). Same pinned-compiler full-PGO comparison versus packed slots: **110.493 -> 104.476 ms**, RSS **16,764 -> 16,828 KiB**, S **552 -> 565**, D=1, complete runner K proxy about **+2.3%**. Individual packet money is narrowed only after a representability check; fallback and every aggregate remain full width.

Full-seed PGO was independently supported by CI 41230039999: plain 113.757 -> PGO 109.513 ms, RSS 17,832 -> 17,660 KiB. The earlier same-compiler run was 105.100 -> 101.255 ms. Training uses unchanged organizer generator, full seed 37; generator copies are exempt from S under the published-copy rule. Plain dense PGO S=550. Packed PGO S=552.

Qualified high-core checkpoints remain available: dense 8-worker **b9d97c4** (CI 41229936187), dense 12-worker **8f0acff** (CI 41229385026), PGO 12-worker **9081484** (CI 41230172608). Twelve-worker PGO passed full seeds 11/29 and all sanitizer/container gates; same-compiler PGO 97.118 -> 94.962 ms, RSS 23,742 -> 23,902 KiB. None is an official high-core performance receipt. The reconciled main-SSE2 scanner (324c4d5) also qualified, but direct full-PGO 92.773 -> 92.820 ms did not justify automatic promotion.

Additional investigations were rejected or left experimental:

- Staged lookup/membership/group updates lost on replication; skipping already-seen bits did not yield a stable complete-K win.
- Private worker bitmaps plus an exact ID/bitmap union lost: 254.804 vs 201.586 ms, with substantially more memory.
- Removing record madvise was neutral in T (159.639 -> 159.802 ms) and slightly higher cgroup M; no promotion.
- Hand-written short equality lost. Sparse metadata indices and other layouts lacked a reliable complete-K win.
- Forty-eight-byte packed metadata did earn qualification and an official score; bit lengths remain full-width for long names, and short/long collision regressions were added.
- Owner dictionaries were revisited with compact fingerprints, dense slots, fixed storage, cold insertion, and 24-byte messages. They still lost locally; no official queue recommendation or PR was made.
- The exact prefix-plus-CRC encoding is mathematically injective at a fixed length <=16: equality of the first 12 bytes and full CRC32C leaves at most four unknown suffix bytes; that CRC suffix transform is invertible. Long names retain bytewise comparison. **1,569 hardware/software encodings and full-rank suffix checks for all lengths 0..16 passed**, but end-to-end benefit was not established. It remains WIP, not a probabilistic hash shortcut or a submission candidate.

Software sampling became available via gprofng with DWARF-4, without instrumentation calls. Sixteen runs highlighted dependent slot accesses, libc scanning, SWAR numeric decoding and pending-row writes. Hardware counters remain unavailable; these are PC samples rather than cache/stall measurements. Source and raw exploratory measurements are preserved in this directory; the source manifest records exact content hashes.

Future work: score the qualified packet context through Luna, investigate the official memory increase and pinned-binary allocation phases, and only then choose high-core combinations. Preserve the measured PR #11 context as rollback. No frontier edits or submissions were made to sd-sm.
