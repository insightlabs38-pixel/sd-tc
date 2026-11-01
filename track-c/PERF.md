# Track C performance log

No official board submission or measured scoring-box T/M/S/D yet. Local correctness tests are not official performance receipts.

| Step | Change | Board run ID | T | M | K |
|---|---|---|---|---|---|
| 1 | Scalar byte parser, dense region lookup, exact name interning | pending | pending | pending | pending |

## Unscored development verification (2026-10-31)

- [CI 41176978826](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41176978826): exact full seed-1 SHA-256, additional seeds, hostile cases, offline Docker, sandbox passed.
- Paired GitHub-runner medians (12 trials): baseline 184.796 ms / 24,642 KiB, source-reduced champion 183.965 ms / 24,726 KiB; 499 to 467 physical source lines.
- These are **not** official scoring-box T/M/S/D or board PERF receipts; final scoring remains Luna's responsibility.

## Shared-name four-worker champion (2026-10-31)

- [CI 41185663840](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41185663840) passed exact full seed-1 hash, other seeds, adversarial overflow/collisions, offline Docker and sandbox.
- Global sharded atomic name slots remove cross-worker distinct-name merge; a locked overflow path preserves correctness under concentrated collisions.
- Paired old two-worker → shared: 176.893 → 113.186 ms, 24,500 → 29,732 KiB RSS, 460 → 410 physical source lines.
- Paired shared → compact slots: 91.498 → 90.723 ms, 29,860 → 27,858 KiB RSS, 410 → 421 lines.
- Paired compact slots → overflow-safe: 148.176 → 150.092 ms, 27,812 → 27,842 KiB RSS, 421 → 442 lines.
- Chained unscored complete-K proxy is approximately +22% versus the prior two-worker champion; CI hardware varied, and official cgroup M, formatter S and scoring-box T/K remain unmeasured.

## Indexed shared-name champion (2026-10-31)

- [CI 41189202155](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41189202155): exact seed-1 hash, additional seeds, collision/overflow/large-field cases, offline Docker and sandbox all passed.
- Dense per-shard name slots plus 16-bit atomic hash indices: paired 12-trial medians **149.363 → 148.877 ms**, **27,430 → 19,136 KiB RSS**, source **442 → 452** physical lines.
- Complete-K development proxy **+7.28%** from T/M/source-line S, unchanged D=1; official scoring-box T/M and formatter S remain unmeasured.
- [CI 41188845400](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41188845400): lazy zero-page slot variant passed correctness but regressed **155.743 → 163.149 ms** with unchanged RSS; reverted to verified champion.
- [CI 41188708497](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41188708497) tested static eager slots (119.690 → 117.552 ms); [CI 41189003933](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41189003933) tested 32 KiB arenas (185.604 → 182.034 ms), both paired runner results subject to noise.

## Compact name-table probe variants (2026-10-31)

All times are 12-trial paired GitHub-runner medians against source-identical verified baseline `6dc125e`, not official box measurements. Each CI passed full seed-1 hash, holdout seeds, adversarial cases, offline Docker and sandbox.

| Variant | CI | Baseline -> candidate ms | Baseline -> candidate KiB RSS | Physical lines | K proxy |
|---|---|---|---|---|---|
| 32-bit fingerprint index, 56-byte slots | [41192746039](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41192746039) | 148.206 -> 152.037 | 19222 -> 19858 | 452 -> 456 | -2.04% |
| 32-bit fingerprint index, 48-byte slots | [41192875572](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41192875572) | 88.597 -> 84.398 | 19600 -> 19112 | 452 -> 455 | +2.87% |
| 16-bit index, direct exact comparison | [41192980216](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41192980216) | 149.869 -> 149.106 | 19344 -> 18646 | 452 -> 451 | +1.03% |
| 16-bit index, packed 8-bit fingerprint | [41193152399](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41193152399) | 116.597 -> 117.756 | 19324 -> 18788 | 452 -> 453 | +0.03% |
| 16-bit index, packed 16-bit fingerprint | [41193302355](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41193302355) | 147.452 -> 147.367 | 19496 -> 18710 | 452 -> 453 | +0.85% |

**Selected:** 32-bit indexed fingerprints with full-length 48-byte slots, `26836f7` source blob. This removes redundant per-slot hashes without compromising bytewise collision checks. Packed variants additionally passed a 16 MiB logical-name regression. K proxy uses T^0.5, M^0.2 and physical-line S^0.15; D unchanged. CI runner speeds varied materially; verify selection with an independent paired run. Official box T/M/S/D and K remain pending.

- **Independent restoration/replication:** [CI 41193433512](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41193433512) passed all gates, including a 16 MiB logical-name regression; paired baseline **147.544 ms / 19,286 KiB** vs selected **141.674 ms / 19,108 KiB** (12 trials). Approximate complete-K proxy **+2.2%**, using physical lines 452 -> 455 and unchanged D. This independently supports promotion but is not an official scoring-box receipt. Source blob `4e5e22a28ca0efb4884975d925fc204903cf21c6` is the CI-verified champion.

## Reclamation and parser trials (2026-10-31)

All listed CI runs passed exact seed-1 SHA-256, holdout seeds, adversarial cases, offline Docker and sandbox. The benchmark workflow now compares directly against verified source `b2cd31a`.

- [41197026344](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41197026344): 64-record batches were effectively neutral after normalization against the earlier benchmark baseline; reverted.
- [41197198393](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41197198393): quote-segment `memchr` hashing, 148.716 → 149.391 ms and 19,112 → 19,038 KiB; estimated K proxy -0.28%, reverted.
- [41197343487](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41197343487): reclaim lookup pages plus 256 KiB record-page threshold, 142.311 → 145.831 ms, 18,956 → 16,636 KiB; K proxy +1.37%.
- [41197476950](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41197476950): **selected** reclaim lookup pages only, 148.067 → 147.923 ms, 18,980 → 16,920 KiB; K proxy **+2.34%**.
- [41197613662](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41197613662): lookup reclamation plus 512 KiB threshold, 140.949 → 143.511 ms, 18,984 → 16,744 KiB; K proxy +1.59%, rejected.

The selected source blob is `3cecd2945c284b4ae1684f9f1779bb0607cc1e33`, byte-identical to the fully CI-verified implementation; original 1 MiB record-page reclamation threshold retained. K proxies use paired runner T, process RSS, physical source lines 455 → 456 and unchanged D=1. Official cgroup M, formatter-adjusted S and scoring-box T/K remain unmeasured.

## Name-probe locality trials (2026-10-31)

All trials passed full seed-1 SHA-256, holdout seeds, hostile cases, offline Docker and sandbox checks. Measurements are 12-trial paired GitHub-runner medians, not official scoring-box T/M/S/D.

- [CI 41201287339](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41201287339): **promoted early name-index prefetch**, verified against `eb1ab4a`: 146.629 → 140.258 ms, 16,918 → 16,916 KiB RSS; 456 → 460 physical source lines. Complete-K proxy **+2.11%**.
- [CI 41201447448](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41201447448): early name-slot prefetch 138.429 → 139.197 ms, 16,856 → 16,902 KiB; 460 → 463 lines, K proxy **-0.43%**. Rejected.
- [CI 41201618354](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41201618354): per-thread exact-name cache 139.972 → 153.091 ms, 16,850 → 19,544 KiB; 460 → 469 lines, K proxy **-7.44%**. Rejected.

Final `join.cpp` is byte-identical to the fully verified early-index-prefetch champion `b29de1927e809a2d0bf89ea0d6e236dcef3d2e85` (blob `dad49158dad5f90d32ab5eb307eb9a62e138723e`). Restoring that verified source requires no redundant CI. K proxies use physical source lines and process RSS; official formatter S and cgroup M remain unmeasured.

## CSV hot-path trials (2026-10-31)

All four experimental CI runs passed full seed-1 SHA-256, holdout seeds, hostile cases, offline Docker and sandbox checks. Medians are paired GitHub-runner measurements against verified source `b29de192`, not official scoring-box values.

- [41205457751](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41205457751): 8-row batches, 139.613 → 144.153 ms; RSS 16,982 → 16,896 KiB; estimated complete-K proxy -1.5%. Rejected.
- [41205606558](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41205606558): canonical raw-name CRC32C hashing, 92.524 → 92.687 ms; RSS 17,144 → 17,094 KiB; source 460 → 454 lines; K proxy +0.17%, inconclusive alone.
- [41205739218](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41205739218): CRC32C plus guarded BOM fast path, 137.582 → 134.553 ms; RSS 16,856 → 16,984 KiB; 460 → 454 lines; K proxy +1.17%. Ten of twelve paired trials favored candidate.
- [41205896010](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41205896010): adding a relaxed seen-bit load, 135.936 → 135.775 ms; RSS 16,994 → 16,856 KiB; 460 → 456 lines; K proxy +0.35% total. Rejected versus the guarded-BOM candidate.

Guarded-BOM CRC source blob `efcc37be6d82c3e341eaa3d66f2e7af82338ee07` restored byte-for-byte. Independent paired replication pending. All K proxies use process RSS and physical source lines; official cgroup M, formatter S and scoring-box K remain unmeasured.

- **Independent replication:** [CI 41206065495](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41206065495) passed all gates with identical source blob; paired median 136.822 → 134.671 ms, RSS 16,920 → 16,852 KiB, physical source 460 → 454 lines. Complete-K proxy **+1.08%** (first run +1.17%). Eight of twelve pairs favored candidate; keep the guarded-BOM CRC champion. Official T/M/S/D remains pending through Luna.

## Canonical raw-name equality (2026-10-31)

- [CI 41210233565](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41210233565) verified corrected CRC32C collision and concurrent 4,200-name shard-overflow regressions; previous hash-specific fixtures did not exercise these paths.
- [CI 41210377543](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41210377543) passed exact full seed-1 SHA-256, other seeds, canonical quoted/unquoted names, long names, adversarial tests, offline Docker and sandbox.
- Under the frozen valid CSV grammar, raw bytes inside the delimiters uniquely encode logical names. Exact raw comparison replaces decoded equality and removes unused slot metadata.
- Paired 12-trial median: **134.364 → 125.213 ms** (6.81% faster), peak RSS **16,792 → 16,920 KiB**, physical source **454 → 440 lines**. All 12 paired trials favored the candidate.
- Complete-K proxy **+3.92%** using runner T/RSS, physical lines and unchanged D=1; official formatter S, cgroup M and scoring-box T/K remain pending.
- Champion source blob: `820ff763699cb5c662fed388e49e3998f49e094e`; no official submission.

## Guarded eight-digit SWAR numeric parsing (2026-10-31)

- [CI 41210692166](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41210692166) passed full seed-1 SHA-256, holdout seeds, collision/overflow/long-integer regressions, offline Docker and sandbox. Paired medians **129.784 → 123.745 ms**, RSS **16,902 → 17,070 KiB**; 10/12 pairs faster.
- [CI 41210875048](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41210875048) independently passed all gates including a new eight-digit-id fixture. Paired medians **79.544 → 77.514 ms**, RSS **17,094 → 17,172 KiB**; 9/12 pairs faster.
- The SWAR fast path validates 1–8 ASCII digits, folds 2/4/8-digit values and retains scalar parsing for longer tokens. Source grows **440 → 458 physical lines**, D remains 1.
- Complete-K proxy **+1.60%** first run and **+0.60%** replication, accounting for runtime, RSS and physical source lines. Official scoring-box T/M/S/D and K remain unmeasured.
- Promoted champion source blob `b5e2c905648d759a15c34ea2e5bd6eed130e9386`; no official submission.

## Name lookup and scheduling experiments (2026-10-31)

All candidates passed exact full seed-1 SHA-256, holdout seeds, adversarial collisions/overflow, offline Docker and sandbox checks. Paired medians are 12-trial GitHub-runner results against the verified `b5e2c905` source, not official scoring-box T/M/S/D. K proxies use T^0.5, process RSS^0.2, physical source lines^0.15 and unchanged D.

- [CI 41215190991](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41215190991): inline 8–16-byte name comparisons regressed 77.786 → 79.835 ms, K proxy -1.43%; reverted.
- [CI 41215326526](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41215326526): pre-spread CRC32C hashes 121.780 → 121.673 ms, RSS 17,028 → 16,890 KiB, K proxy +0.24%; 6/12 pairs faster, inconclusive.
- [CI 41215515777](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41215515777): pre-spread hash plus early region classification 93.141 → 94.474 ms, K proxy -0.45%; reverted.
- [CI 41215678574](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41215678574): 48-byte pending-row layout 118.476 → 118.002 ms, K proxy +0.21%; 6/12 pairs faster, inconclusive.
- [CI 41215841709](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41215841709): compact rows plus aggregate-before-name-interning 117.007 → 116.205 ms, RSS 17,084 → 16,940 KiB, K proxy +0.48%; 9/12 pairs faster. Independent [attempt 2](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41215841709/attempts/2) reversed to 94.272 → 95.432 ms, K proxy -0.54%; rejected.
- Restored byte-identical, previously fully qualified source blob `b5e2c905648d759a15c34ea2e5bd6eed130e9386`. No redundant restoration CI. Four workers remained fastest in the available 1/2/3/4/5/6/12-worker runner profiles. Official scoring-box axes still pending through Luna.

## Direct-row, cacheline, and cold-path trials (2026-10-31)

Each experiment passed exact full seed-1 SHA-256, additional seeds, adversarial collision/overflow and long-name cases, offline Docker and sandbox. Paired 12-trial runner measurements compare against verified source `b5e2c905648d759a15c34ea2e5bd6eed130e9386`, not the official scoring box.

- [CI 41219761977](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41219761977): direct per-row aggregation without the 32-row pending batch regressed **93.764 → 112.372 ms**, RSS 17,112 → 17,112 KiB, physical lines 458 → 442; complete-K proxy **-8.17%**. Reject: pending batches hide name-index latency.
- [CI 41219893713](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41219893713): 64-byte aligned name slots regressed **77.709 → 80.523 ms**, RSS 17,072 → 18,516 KiB; K proxy **-3.35%**. Reject: false-sharing savings do not offset larger slot footprint.
- [CI 41220040110](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41220040110): isolate rare name insertion/overflow in a noinline cold path, **124.352 → 120.685 ms**, RSS 17,064 → 16,984 KiB, 458 → 460 physical lines; K proxy **+1.54%**. **Promoted after independent replication:** source blob `f875b5d3d51011fe0cc710fab851b78471483cbb`.

K proxies use runner T, process RSS, physical lines and unchanged D=1. Official formatter-adjusted S, cgroup M and scoring-box K remain pending through Luna.
- [Independent CI 41220204312](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41220204312) passed all correctness, offline Docker and sandbox gates with the identical source blob; paired median **116.768 → 113.902 ms**, RSS 16,958 → 16,856 KiB; complete-K proxy **+1.31%** (7/12 pairs faster). Initial run was +1.54% (9/12 pairs faster). Both runs favor cold-path separation, with runner noise acknowledged.
- The qualified champion is the noinline-cold insertion implementation. Future work: pursue substantive reductions in shared-name probe/cache-miss cost rather than more batch-size tuning. Official scoring-box T/M/S/D and K remain unmeasured.

## Name-table architecture trials (2026-10-31)

- [CI 41221213789](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41221213789): experimental 64-byte direct slots passed exact seed-1 hash but failed stale profiler instrumentation; paired 118.250 -> 139.429 ms and RSS 16,966 -> 22,670 KiB. Rejected.
- [CI 41224409235](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41224409235): 2,048-entry bounded pools per shard passed all exactness, seed, hostile, Docker and sandbox gates. Paired baseline 93.845 -> 94.001 ms, RSS 17,106 -> 17,108 KiB; K proxy -0.18%. Sparse pool pages were already nonresident; rejected.
- [CI 41224625062](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41224625062): direct 48-byte atomic slots passed all gates, but paired baseline 118.604 -> 118.601 ms and RSS 16,930 -> 23,286 KiB, source 458 -> 456 lines; K proxy -6.11%. The direct table touches many otherwise cold pages; rejected.
- Restored verified cold-insertion champion source blob `f875b5d3d51011fe0cc710fab851b78471483cbb`, independently qualified in [CI 41220204312](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41220204312). The workflow's comparison baseline now points to that champion instead of an older pre-promotion source. Official scoring-box T/M/S/D still pending.

## CRC32C remainder packing trial (2026-10-31)

- [CI 41229565059](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41229565059) passed exact full seed-1 SHA-256, holdout seeds, adversarial collision/overflow, offline Docker and sandbox.
- Replacing 1-byte remainder CRC steps with 4/2/1-byte hardware CRC reduced instruction count but regressed paired median **115.957 -> 116.964 ms**; RSS **17,018 -> 17,014 KiB**, physical source **460 -> 474 lines**.
- Estimated full-K proxy **-0.87%** with T/RSS/source/D=1; reject and restore verified `f875b5d3` source. Official cgroup M, formatter S and scoring-box K remain unmeasured.

## Inline SSE2 quote scanner (2026-10-31)

- [CI 41229750254](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41229750254), attempts 1 and 2, passed full seed-1 SHA-256, holdout seeds, collision/overflow, long names, offline Docker and sandbox.
- Replace repeated library `memchr` quote searches with guarded 16-byte SSE2 comparisons and scalar tail fallback; escaped quotes and decoded lengths retain existing semantics.
- Paired 12-trial medians: **103.962 -> 100.798 ms**, RSS **17,146 -> 17,158 KiB**, estimated K proxy **+1.18%**; independent replication **140.930 -> 132.367 ms**, RSS **17,070 -> 17,112 KiB**, estimated K proxy **+2.77%** (all 12 pairs faster).
- Source **460 -> 471 physical lines**, D=1 unchanged. Promoted source blob `4ca129b0ca9596554d052aadb9d5e751496cc754`; official cgroup M, formatter S and scoring-box K still pending.

## 256-shard name-index locality trial (2026-10-31)

- [CI 41230207274](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41230207274) passed exact full seed-1 hash, holdouts, SIMD quote-boundary regression, concentrated-shard overflow, offline Docker and sandbox.
- Increasing shards **64 -> 256** while reducing index slots/shard **4096 -> 1024** retained the same total index/pool capacity but regressed paired median **93.916 -> 95.517 ms**, RSS **17,108 -> 18,356 KiB**.
- Estimated complete-K proxy **-2.23%**, unchanged physical source lines and D=1; rejected and restored SSE2 champion `4ca129b0`. Official cgroup M, formatter S and scoring-box K remain pending.

## AVX2 quote scanning and CPU-budget trial (2026-10-31)

- [CI 41233936842](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41233936842) passed full seed-1 SHA-256, other seeds, escaped-quote/vector-boundary, long-name, collision/overflow, offline Docker and sandbox checks. A guarded AVX2 32-byte quote scan, SSE2 16-byte remainder and inline scalar tail improved 12-pair runner median **112.111 -> 110.822 ms**, RSS **16,832 -> 17,030 KiB**, source **471 -> 479 lines**, estimated complete-K proxy **+0.09%** (9/12 faster).
- [Independent attempt 2](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41233936842/attempts/2) passed all gates again; paired median **143.111 -> 139.654 ms**, RSS **17,026 -> 17,076 KiB**, K proxy **+0.92%** (10/12 faster). **Promote** qualified AVX2 source blob `54308da7af86c24c7695963f7762d28368f94367`.
- [CI 41234316750](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41234316750) tested dynamic workers capped at six by Linux affinity and cgroup CPU quota. All gates passed. On the 4-vCPU runner, paired median **72.914 -> 72.822 ms**, RSS **17,112 -> 17,170 KiB**, source **479 -> 498 lines**, estimated K proxy **-0.59%**. **Not promoted** without official six-P-core timing; candidate remains recoverable as commit `3b297da` for Luna to evaluate. Runner profiling: four workers 72.736 ms, six 73.724 ms.
- Official scoring-box T/M/S/D and K remain unmeasured. All proxy comparisons use runner wall/RSS and physical source lines, not cgroup peak or formatter-adjusted S.


## Official promotion and PR #11 conflict resolution

Official run `run-15adbc1f561c` passed all gates on this packed-metadata source: T=0.076727 s, M=14,487,552 B, S=552, D=1, K=20.508340, points=65.596525, improving prior BEST `run-8f98e6ec4091` (K=20.413289). The later PR-head pragma revision `78503e708dccdbf484bb5dc1223a02705cfabb81` passed all gates but scored only K=18.480714 in `run-9722acb5cb38`; it is excluded. Merge PR #11 with the tested predecessor source `59974fd6328c2fabb1154ac4cc31bda1036bc6ca`, preserving the later `main` source in the merge parent/history.

## Early lookup-map reclamation on official-scored packed champion (2026-10-31)

- Restored one safe `MADV_DONTNEED` after the complete `regions.csv` lookup parse, leaving the qualified official-scored baseline source blob `59974fd6328c2fabb1154ac4cc31bda1036bc6ca` recoverable. Candidate source blob `bb79a53e6956b175c65c28943d2b175eeb1f2985`.
- [CI 41238941731](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41238941731), attempts 1 and 2, passed exact seed-1 full SHA-256, full holdout seeds 11/29, hostile/overflow cases, ASan/UBSan/TSan, offline Docker and sandbox. [Baseline repair CI 41238737277](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41238737277) also passed.
- **Paired full-PGO runner medians**, against the actual official-scored source: **110.927 → 110.524 ms**, RSS **17,212 → 16,572 KiB** (attempt 1); **108.627 → 108.363 ms**, RSS **17,020 → 16,708 KiB** (attempt 2).
- Estimated full-K runner proxies **+0.92% / +0.47%**, assuming one additional scored source line and unchanged D=1; source-only comparisons yielded **+1.65% / +2.17%**. Both independent PGO runs favored the candidate, but the official scoring-box cgroup M and T/M/S/D/K still require Luna validation.

## AVX2 quote scanning on packed-name PGO champion (2026-10-31)

- [CI 41243264138](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41243264138) passed seed-1 full SHA-256, holdout full seeds 11/29, hostile parser tests, ASan/UBSan/TSan, pinned offline Docker and sandbox checks.
- Transplanted the previously qualified guarded AVX2/SSE2 quote scanner onto the official-scored packed-name architecture; the scalar tail handles short fields and vector boundaries.
- Paired 12-trial **full-PGO** medians versus the official-scored baseline: **109.171 → 104.134 ms**, peak process RSS **16,892 → 16,592 KiB**. Pinned-compiler PGO vs untrained binary: **107.836 → 103.797 ms**.
- Physical source lines **469 → 488**, D=1 unchanged. Estimated complete-K runner proxy **+2.15%** using T^0.5, RSS^0.2, source lines^0.15; formatter S and official cgroup M/K still require Luna scoring.
- Source blob `6b3f574b308b97bab15a23930ec9a09f6d18a666` is the qualified candidate; independently replicated in [CI 41243555329](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41243555329), which passed all gates and measured **110.434 → 105.698 ms** full-PGO, RSS **17,020 → 16,572 KiB**, complete-K runner proxy **+2.15%**. **Promoted** after two independent +2.15% trials; official T/M/S/D/K remains pending through Luna.

## Finer-grained mmap reclamation trial (2026-10-31)

- [CI 41248226794](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41248226794) passed full seed-1 SHA-256, holdout seeds, adversarial cases, sanitizers, offline Docker and sandbox.
- Trialed 256 KiB record-page reclamation and final-tail discard. Full-PGO runner comparison against the older official-scored baseline: 74.346 -> 75.236 ms, RSS 16,924 -> 16,436 KiB. The baseline also predates AVX2, so this does not isolate the reclamation change.
- No robust complete-K gain established; restored the previously twice-qualified AVX2 champion before the next architecture trial. Future CI comparisons use that champion as baseline.

## Lower-load 128-shard name index (2026-10-31)

- Increased shards 64 -> 128 at 2,048 slots/shard, preserving exact CRC32C and byte comparisons. The workflow now compares directly with the twice-qualified AVX2 source blob `6b3f574b308b97bab15a23930ec9a09f6d18a666`.
- [CI 41248440108](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41248440108), attempts 1 and 2: exact full seed-1 SHA-256, full holdout seeds 11/29, adversarial overflow/collisions, sanitizers, pinned offline Docker and sandbox all passed.
- Paired 12-trial **full-PGO** runner medians: **105.302 -> 101.869 ms**, RSS **16,496 -> 16,496 KiB**, estimated complete-K proxy **+1.67%**; independent attempt 2: **105.087 -> 100.343 ms**, RSS **16,756 -> 16,756 KiB**, proxy **+2.34%**.
- Source-only trials: 113.357 -> 109.195 ms, RSS 17,500 -> 18,504 KiB (K +0.76%); replication 109.487 -> 107.327 ms, RSS 17,244 -> 18,684 KiB (K -0.61%). Release uses full-PGO, not source-only. Scored source lines unchanged, D=1.
- **Promoted** source blob `a9b2c765efe2a5fedec7671d9df9be77a700c853`. Official scoring-box T/M/S/D/K still pending through Luna; runner RSS is not cgroup memory. Preserve AVX2 predecessor as rollback.

## Guarded fused money SWAR trial (2026-10-31)

- [CI 41252674260](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41252674260) passed full seed-1 SHA-256, holdouts 11/29, new money width/sign/EOF fixture, sanitizers, offline Docker and sandbox.
- Compared with verified 128-shard source `a9b2c765efe2a5fedec7671d9df9be77a700c853`, source-only 12-pair median **86.485 -> 86.149 ms**; full-PGO **81.938 -> 84.812 ms**, RSS **16,776 KiB** both.
- Physical source **487 -> 504 lines**, unchanged D=1; full-PGO K proxy approximately **-2.2%** including source growth. Reject; the PGO-vs-untrained comparison is not a champion comparison.
- Restored the exact qualified 128-shard source. The independent adversarial money fixture and corrected main-to-main CI baseline remain; no redundant restoration CI.

## Separate frontier candidate for official evaluation (2026-10-31)

- Qualified public branch commit `90da5461b0c806fafd172ef2f5ca42feb9e0fff8` ([CI 41251942615](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41251942615)) uses guarded short-key hashing and a fused numeric path. All exactness, sanitizer, Docker and holdout gates passed.
- Paired full-PGO runner versus main `f294a882`: **68.679 -> 59.927 ms**; process RSS **16,776 -> 35,752 KiB**. Canonical scored S **577 -> 622**, D=1.
- Its separate pinned-container cgroup comparison reports **145.858 -> 129.350 ms**, **10,168,320 -> 10,149,888 bytes**, estimated complete K **+5.04%**. Runner RSS is not official cgroup M; these are not official scoring-box measurements.
- **Do not auto-promote** without official T/M/S/D/K through Luna. The verified main champion `a9b2c765` remains authoritative; the frontier branch is an optional isolated scoring trial.

## 256-shard, 2,048-slot architecture trial (2026-11-01)

- [CI 41257048767](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41257048767) passed exact full SHA-256, holdout seeds, concentrated-shard overflow, sanitizers, offline Docker and sandbox.
- Source-only paired median **87.750 -> 89.404 ms**, RSS **18,424 -> 20,330 KiB**.
- Full-PGO paired median **83.476 -> 84.294 ms**, RSS **16,808 -> 18,156 KiB**. Estimated runner complete-K proxy **-2.01%**, unchanged S and D=1.
- Rejected: restore exact qualified champion blob `a9b2c765efe2a5fedec7671d9df9be77a700c853`. CI architecture baseline repaired to this champion via commit `d3b9a32`.
- Additional safety gate: page-aligned EOF fixture protects against speculative short-name SIMD or money loads crossing the mapped file end. Frontier candidate `90da546` has unguarded 16-byte/8-byte loads and requires explicit end guards before promotion.

## EOF-safe short-key hash and PR #13 integration (2026-11-01)

- [CI 41257509982](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41257509982), attempts 1/2, qualified end-guarded padded CRC32C short-name hashing on the 128-shard champion. Full-PGO medians **127.008 -> 114.708 ms** and **100.468 -> 94.057 ms**, unchanged RSS, physical source **487 -> 507** lines; runner complete-K proxies **+4.59% / +2.73%**. Source blob `94a3f1dda129c91eb4d31f01957ae25fb7375473` remains recoverable.
- Concurrent merge [PR #13](https://github.com/insightlabs38-pixel/sd-tc/pull/13) at `da7fe7b` introduced compact 16-byte name metadata, 48-byte pending rows and deferred lookup prefetch. Its source `c4b41459` passed [CI 41257924075](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41257924075), but contained unguarded 16-byte hash/name-compare and 8-byte money reads near mapped EOF.
- [Safety CI 41258246960](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41258246960) passed full seed-1 hash, holdout seeds 11/29, page-aligned duplicate-name/short-money EOF, hostile collisions/overflow, ASan/UBSan/TSan, offline Docker and sandbox. Guarded hash fallback preserves the exact same padded CRC; SIMD equality uses exact-length memcmp only near EOF; cents reads require eight available bytes.
- Qualified safe PR #13 source blob `e070c9f03bd5261bc44cfdd85600a92f493f0263`. Paired full-PGO runner against unguarded PR #13: **80.050 -> 81.602 ms**, process RSS **41,302 -> 35,088 KiB**, physical source **534 -> 545** lines. Process RSS is not official cgroup M; official T/M/S/D/K and direct cgroup comparison remain pending.
- Preserve the safe PR #13 implementation for Luna; do not reintroduce unguarded mapped-file loads merely to recover a small local timing delta.

## Demand-pool, membership-prefetch and newline trials (2026-11-01)

- [CI 41259053272](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41259053272) passed all gates for lazy second-half name slots; no isolated complete-K gain established.
- [CI 41259282478](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41259282478) passed all gates for half-slots plus write-prefetch; direct safe-champion full-PGO **80.452 -> 81.179 ms**, cgroup **10,487,808 -> 10,485,760 B**, source +3 lines, D=1; complete-K runner proxy **-0.53%**. Rejected half-slots.
- [CI 41259602337](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41259602337) passed write-prefetch alone; direct safe-champion PGO **81.342 -> 80.456 ms**, cgroup **10,493,952 -> 10,496,000 B**, unchanged S/D; proxy **+0.55%**.
- [CI 41259948744](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41259948744), attempts 1 and 2, passed full seed-1 SHA-256, full holdouts 11/29, hostile cases, ASan/UBSan/TSan, offline Docker and sandbox. Branchless LF/CRLF versus prefetch-only: full-PGO **80.444 -> 78.670 ms** and **80.807 -> 79.011 ms**; cgroup **10,485,760 -> 10,485,760 B** and **10,496,000 -> 10,498,048 B**; physical source **546 -> 544 lines**, D=1. Complete-K runner proxies **+1.18% / +1.18%**. Promoted source blob `3519869c67493e3e5da7c4ced30b38be438cfeca`.
- All ratios are runner proxies, not official scoring-box measurements. Official C BEST remains PR #13 `run-f24670761e6b`, K=21.757571; Luna must validate the new T/M/S/D/K before official promotion.

## Parse-stage exact name lookup overlap (2026-11-01)

- [CI 41261183692](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41261183692), attempts 1 and 2, passed full seed-1 SHA-256, holdout seeds 11/29, hostile cases, ASan/UBSan/TSan, pinned offline Docker and sandbox.
- Moved exact shared-name probe and membership write-prefetch into the parse stage after amount/region extraction, while retaining the 32-row batch and deferred atomic aggregation.
- Paired full-PGO 12-trial medians versus qualified `3519869c`: **65.070 -> 64.236 ms** and **78.833 -> 77.788 ms**; cgroup peaks **10,487,808 -> 10,473,472 B** and **10,498,048 -> 10,467,328 B**.
- Physical source lines **544 -> 543**, D=1; complete-K runner proxies **+0.70% / +0.76%**. Source-only binaries regressed 71.330 -> 72.548 ms and 87.376 -> 88.580 ms, so promotion is specific to the pinned full-PGO release.
- Promoted twice-qualified source blob `b31de39258eec335f79e14a0568a325d4ad2c732` (commit `981d23c`). Official six-P-core T/M/S/D/K still requires Luna; official BEST remains K=21.757571.

## Compact 48-byte aggregate-cell trial (2026-11-01)

- [CI 41261785510](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41261785510) passed full seed-1 SHA-256, holdout seeds 11/29, hostile cases, ASan/UBSan/TSan, pinned offline Docker and sandbox.
- Narrowed spec-bounded count, distinct and byte-length accumulators to 32 bits, reducing each worker's aggregate cell from 64 to 48 bytes without narrowing 64-bit id or money totals.
- Direct qualified-champion full-PGO paired median **78.186 -> 78.993 ms**, cgroup **10,500,096 -> 10,498,048 B**, physical source **543 -> 544 lines**, D=1; estimated complete-K proxy **-0.54%**.
- Source-only **87.537 -> 87.603 ms**. Rejected: aggregate cells are not the dominant cache-pressure source; restored byte-identical qualified source blob `b31de39258eec335f79e14a0568a325d4ad2c732`. No redundant restoration CI.

## Deferred aggregation and long-name prefetch trials (2026-11-01)

- [CI 41265183196](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41265183196) passed full seed-1 SHA-256, holdouts 11/29, hostile cases, sanitizers, offline Docker and sandbox. Moving private group arithmetic before the atomic name membership check: full-PGO **83.060 -> 82.019 ms**, cgroup **10,244,096 -> 10,489,856 B**, physical source **543 -> 545 lines**, D=1; complete-K runner proxy **+0.10%**, too small relative to variability to promote.
- [CI 41265503908](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41265503908) passed the same gates for added long-name arena prefetch. Direct qualified-champion full-PGO **83.035 -> 83.833 ms**, cgroup **10,483,712 -> 10,485,760 B**, source **543 -> 550 lines**, D=1; complete-K proxy **-0.67%**. Source-only **92.265 -> 94.333 ms**. Rejected.
- Restored exact twice-qualified source blob `b31de39258eec335f79e14a0568a325d4ad2c732`; CI comparison baseline remains commit `981d23c`. No redundant restoration CI.

## Name-slot pipeline depth experiments (2026-11-01)

- [CI 41268916263](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41268916263): moving name-slot lookup before region/amount parsing passed all correctness, sanitizer, Docker and holdout gates. Full-PGO paired 78.001 -> 84.571 ms, cgroup peak 10,471,424 -> 10,459,136 bytes; T/M-only K proxy -3.94%. Rejected.
- [CI 41269285736](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41269285736): 64-row instead of 32-row name prefetch batches passed the same gates. Full-PGO paired 77.523 -> 77.474 ms, cgroup peak 10,452,992 -> 10,510,336 bytes; complete-K proxy -0.08% (unchanged S and D). Rejected.
- Restored exact twice-qualified 32-row champion source blob `b31de39258eec335f79e14a0568a325d4ad2c732`; no redundant restoration CI. Official scoring-box T/M/S/D for these trials was not measured.
- [CI 41269631316](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41269631316): 16-byte unrolled CRC32C on long names passed full exactness, sanitizers, Docker and holdouts. Full-PGO 77.428 -> 77.195 ms, cgroup peak 10,489,856 -> 10,467,328 bytes, source 542 -> 551 lines; complete-K proxy -0.05%. Source-only build regressed 86.150 -> 87.844 ms. Rejected; original source restored.

## Seen-bit and bounded delimiter trials (2026-11-01)

- [CI 41273291219](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41273291219) passed full reference SHA-256, holdouts 11/29, sanitizers, offline Docker and sandbox. Relaxed seen-bit precheck versus qualified `981d23c`: full-PGO 78.850 -> 78.772 ms; cgroup peak 10,455,040 -> 10,405,888 B; source 543 -> 545 lines; complete-K runner proxy **+0.09%**, below reliable promotion threshold.
- [CI 41273543446](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41273543446) passed the same gates. Adding bounded `memchr` for unquoted non-final fields to the seen-bit trial: full-PGO 54.534 -> 54.843 ms; cgroup 10,485,760 -> 10,418,176 B; source 543 -> 545 lines; complete-K proxy **-0.21%**. Rejected.
- The generator quotes every nonempty name and note, so the unquoted-field change primarily touches empty fields. Isolated `memchr` source `3c08ed11204778edd0e94e45b8f873c01d6d6e42` is being qualified in [CI 41273584148](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41273584148); do not promote without its full results.
- Restored exact qualified source blob `b31de39258eec335f79e14a0568a325d4ad2c732` on `main`; no redundant restoration CI. Official BEST remains K=21.757571 pending Luna's newer official measurements.

## Independent architecture campaign (2026-11-01)

- Implemented private dictionary/reconciliation, bounded owner streams, exact
  fingerprint radix reduction, whole-range structural scanning, country-major
  membership and flat publication; concrete sources and receipts are retained
  on `frontier/c-independent-architecture-20261101` under research/.
- Selected hybrid: fused quote-mask scanning of name/note, exact 32-byte inline
  keys with full long-name fallback, and explicit THP disable before allocation.
  No probabilistic identity, new runtime dependency or Dockerfile change.
- Two fresh-container paired PGO batches, full seed 1, local Xeon VM pinned to
  two CPUs: main 174.126/168.193 ms -> candidate 156.334/155.548 ms;
  maximum cgroup peaks 10,469,376/10,280,960 -> 10,878,976/10,821,632 B.
  Canonical complete S 644 -> 657, D=1. Relative complete K +4.4%/+2.6%.
- Against immutable official BEST source c4b41459: local 171.232/173.791 ms,
  M 10,227,712/10,297,344 B, S637; candidate K proxies +2.9%/+4.2%.
  These are development comparisons, not official scoring-box results.
- Exact source qualification, independent full holdouts and CI receipts are
  recorded in the campaign report. Luna alone evaluates official T/M/S/D/K;
  preserve b31de392 main and c4b41459 official sources as rollback.

## Fused-scanner boundary qualification and reclaim trial (2026-11-01)

- [CI 41277146572](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41277146572) passed full hashes, holdouts, sanitizers and offline Docker after exhaustive 64-bit quote-mask boundary regression; source remained qualified fused scanner `0772ace04e358868f0b060b178d6ab77e93a15ab`.
- [CI 41277229285](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41277229285) passed the same gates for 512 KiB record-page reclamation, compared directly with fused-scanner commit `d459dcc`.
- Direct full-PGO paired median **66.697 -> 67.051 ms**, cgroup median **11,003,904 -> 11,030,528 bytes**, unchanged S/D; complete-K proxy **-0.31%**. Source-only **71.113 -> 72.534 ms**. Reject.
- Restored exact qualified fused-scanner source; retained quote-mask regression and direct-champion CI baseline. Official T/M/S/D/K remains Luna's responsibility.

## Bounded long-name CRC trial (2026-11-01)

- Qualified source commit `9b6c256`, source blob `1cfd7405b24cfff6f855f16ef0ad20dda38ad5b7`: hash first/last eight bytes plus length for names >16 bytes, with unchanged exact full-byte collision checks.
- [CI 41284775090](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41284775090) and [CI 41285099490](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41285099490) passed full seed-1 reference SHA, holdout seeds 11/29, hostile tests, ASan/UBSan/TSan, offline Docker and sandbox. Second run adds exact equal-head/tail collision cases for inline and arena names.
- Direct qualified fused-scanner full-PGO paired medians: **78.263 -> 77.383 ms** and **79.845 -> 78.929 ms** (1.12% / 1.15% faster).
- Paired cgroup peaks: **11,034,624 -> 11,026,432 B** and **11,034,624 -> 11,022,336 B**. Physical source 546 -> 540 lines; D=1. Approximate complete-K proxy **+0.72% / +0.74%**, assuming scored S changes by six code lines; not an official measurement.
- Source-only paired medians were 86.194 -> 87.171 ms and 88.079 -> 87.026 ms; source-only benefit is not established. Release uses pinned PGO. Preserve prior qualified fused-scanner blob `0772ace04e358868f0b060b178d6ab77e93a15ab` as rollback.
- Promoted to development main after independent qualification. Luna must measure official scoring-box T/M/S/D/K before official integration.
