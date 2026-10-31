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
