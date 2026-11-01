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

## Four-worker scoring tradeoff (2026-11-01)

- Trial `40dd73d` changed only full-scale workers 6 -> 4. [CI 41285464392](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41285464392) passed exact full seed-1 SHA, holdout seeds 11/29, adversarial/sanitizer tests, offline Docker and sandbox.
- Same-source instrumented source-only GH runner: workers 4 **80.514 ms / 41,808 KiB** vs workers 6 **81.510 ms / 44,940 KiB**. Runner has four vCPUs; the official scoring box has six P-cores, so this is not evidence that four workers improve official K.
- Paired full-PGO against the older fused-scanner six-worker baseline (not the new hash champion): **77.548 -> 75.968 ms**; cgroup **11,028,480 -> 11,018,240 B**. Confounded by the head-tail hash improvement and runner topology, not a direct official T/M/S/D/K comparison.
- Rejected for automatic promotion; source restored byte-identically to qualified six-worker hash champion `1cfd7405b24cfff6f855f16ef0ad20dda38ad5b7` at `eff1c947`. Optional Luna official-box trial only if useful; no redundant restoration CI.

## Sixteen-row prefetch batch trial (2026-11-01)

- Trial `8ccd918` changed the six-worker, full-PGO champion's batch capacity 32 -> 16 rows, with no other solution changes.
- A new CI run was not observable through the available GitHub commit-status/workflow-read interface; no correctness, T/M/S/D or K receipt is claimed.
- **Not promoted**: commit `83c889a` restored exact qualified champion source blob `1cfd7405b24cfff6f855f16ef0ad20dda38ad5b7` on `main`.

## Finer shared-name sharding trial (2026-11-01)

- Commit `3adc71f` tested 256 shards x 1024 slots against qualified `9b6c256` (128 x 2048); total slot capacity unchanged. CI baseline was updated to compare directly against this qualified champion.
- [CI 41293164374](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41293164374) passed full seed-1 SHA-256, full holdouts 11/29, adversarial overflow and collision cases, ASan/UBSan/TSan, pinned offline Docker and sandbox.
- Direct 12-trial full-PGO paired medians **64.069 -> 63.938 ms**, cgroup peak **11,098,112 -> 11,546,624 B**; unchanged source S (540 physical lines) and D=1. Complete-K runner proxy **-0.69%**. Rejected despite 0.20% faster T.
- Earlier 16-row batch trial [CI 41288724025](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41288724025) also completed successfully, resolving its previous CI-visibility blocker; no direct complete-K evidence justified promotion.
- Restored exact qualified six-worker source blob `1cfd7405b24cfff6f855f16ef0ad20dda38ad5b7`; no redundant restoration CI. Official scoring-box measurements remain Luna's responsibility.

## Direct sixteen-row batch qualification (2026-11-01)

- Commit `95874a5` requalified 16-row name-prefetch batches against the exact six-worker, 32-row champion using corrected direct PGO controls.
- [CI 41293459256](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41293459256) passed full seed-1 SHA-256, full holdouts 11/29, hostile overflow/collision tests, sanitizers, offline Docker and sandbox.
- Direct 12-trial full-PGO paired median **75.955 -> 77.129 ms**; cgroup peak **11,028,480 -> 11,024,384 B**. S and D unchanged. Complete-K runner proxy **-0.76%**. Rejected despite slightly lower M.
- Restored byte-identical qualified 32-row champion blob `1cfd7405b24cfff6f855f16ef0ad20dda38ad5b7`. No redundant restoration CI. Further substantial architecture work is preferable to smaller batches.

## Simple-quoted field fast-path trial (2026-11-01)

- Candidate [`9b2e754`](https://github.com/insightlabs38-pixel/sd-tc/commit/9b2e754e2ae80a8898d7fc469d535b9bc208f267) used bounded `memchr` to bypass the quote-mask scanner only when both quoted text fields had no escaped quotes. All other records retained the exact SIMD fallback.
- [CI 41297722005](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41297722005) passed full seed-1 SHA-256, full holdout seeds 11/29 (three repetitions), adversarial cases, ASan/UBSan/TSan, offline Docker and sandbox.
- Direct champion-versus-candidate 12-trial full-PGO medians **104.628 -> 104.754 ms**, cgroup memory **11,012,096 -> 11,030,528 B**, physical source **540 -> 561 lines**, D=1. Complete-K runner proxy **-0.66%**. Source-only 117.496 -> 116.140 ms is not the pinned release result.
- **Rejected** despite valid semantics: the libc quote searches duplicate work already efficiently handled by fused AVX2/PCLMUL scanning. Restored exact qualified source blob `1cfd7405b24cfff6f855f16ef0ad20dda38ad5b7` without redundant restoration CI. Official K remains Luna's responsibility.

## In-mask simple-quote fast-path promotion (2026-11-01)

- Commit [`0357386`](https://github.com/insightlabs38-pixel/sd-tc/commit/035738649cf4b644f1ecf43d0bcb17a72f8a98a8) reuses the first 64-byte AVX2 quote mask to recognize two quoted, unescaped fields, bypassing PCLMUL only when structural quote/comma boundaries prove the fast path exact. Embedded commas/LFs remain raw bytes; escaped quotes, long fields and unquoted fields retain the qualified fallback.
- [CI 41298153512](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41298153512), attempts 1 and 2, passed full seed-1 SHA-256, full holdout seeds 11/29 repeated three times, hostile quote-boundary/collision/overflow cases, ASan/UBSan/TSan, pinned offline Docker and sandbox.
- Direct champion-versus-candidate full-PGO paired medians **77.017 -> 75.639 ms** (11/12 faster) and independent **82.285 -> 80.238 ms** (11/12 faster). Cgroup peaks **10,993,664 -> 11,026,432 B** and **11,016,192 -> 11,036,672 B**.
- Physical source **540 -> 567 lines**, D=1. Complete-K runner proxies **+0.11% / +0.49%**, including T/M/S/D; these are modest, repeatable gains, not official scoring-box K. Source-only and untrained comparisons are not used for promotion.
- **Promoted development champion** source blob `3b5e1392f49abdbb7fdcf6a36b5866366298143b`. CI now compares against this exact source; previous twice-qualified `1cfd7405b24cfff6f855f16ef0ad20dda38ad5b7` remains rollback. Luna must measure official six-P-core T/M/S/D/K before submission.

## Two-mask simple-quoted note trial (2026-11-01)

- [CI 41301985445](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41301985445) passed full seed-1 SHA-256, independent full holdouts 11/29, exhaustive 0..127 simple-quote boundary fixtures, ASan/UBSan/TSan, offline Docker and sandbox.
- Added a second 64-byte quote mask only when the first mask contains the name closing and note opening quotes but not the note closing quote.
- Direct qualified-champion full-PGO paired median **99.901 -> 100.708 ms**; fresh cgroup **11,034,624 -> 11,018,240 B**; physical source **567 -> 577 lines**, D=1. Approximate complete-K runner proxy **-0.63%**. Rejected.
- Restored exact qualified champion blob `3b5e1392f49abdbb7fdcf6a36b5866366298143b`. Retained the new boundary regression; no redundant restoration CI.

## Deferred comma-mask experiment (2026-11-01)

- [CI 41302337233](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41302337233) passed full seed-1 SHA-256, independent full holdouts 11/29, exhaustive quote-mask boundary tests, ASan/UBSan/TSan, pinned offline Docker and sandbox.
- Moved comma SIMD mask computation below the simple-quote early return, without changing the fallback parser or output semantics.
- Direct qualified-champion full-PGO paired medians **76.574 -> 76.584 ms**; cgroup **11,026,432 -> 11,022,336 B**; source **567 -> 567 lines**, D=1. Complete-K runner proxy approximately **+0.001%**, indistinguishable from noise. Source-only 83.418 -> 83.638 ms.
- **Not promoted**: exact qualified source blob `3b5e1392f49abdbb7fdcf6a36b5866366298143b` restored. Enhanced quote-boundary regression remains; no redundant restoration CI.

## Official scoring snapshot (2026-11-01 11:07 UTC)

- Organizer [raw board](https://s3.amazonaws.com/raptors.dev/speeddemon.dev/2026/board/board.json), generated 11:07:32 UTC: HighVelocity Track C BEST `run-e7eab9673214`, official **T=50.141 ms, M=11,288,576 B, S=657, D=1, K=25.979773** (submission `6917275`). LATEST `run-e50afc3730a2` K=25.478194.
- This is an external scoring snapshot, not a measurement of the newer development champion; Luna alone controls official integration and submission.

## PGO training-seed experiment (2026-11-01)

- Candidate [aacb36b](https://github.com/insightlabs38-pixel/sd-tc/commit/aacb36be8c8eb9d31eb67d61c43546891a3be2eb) changed pinned GCC 15 full-PGO training from generated seed 37 to seed 1; exact `join.cpp` unchanged.
- [CI 41306717074](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41306717074) passed full seed-1 SHA, independent full holdouts 11/29, hostile tests, ASan/UBSan/TSan, offline Docker and sandbox.
- Direct 12-trial paired full-PGO seed-37 -> seed-1 median **105.487 -> 104.945 ms**, fresh cgroup **11,018,240 -> 11,024,384 B**; scored source S and D unchanged. Complete-K runner proxy **+0.25%**.
- **Rejected:** small seed-1-specific gain within runner variation, no evidence on private final holdout seed. Restored original seed-37 Dockerfile and workflow; exact qualified `join.cpp` blob `3b5e1392f49abdbb7fdcf6a36b5866366298143b` preserved. No redundant restoration CI.

## SSE short-name comparison screen (2026-11-01)

- Trial `e9fcc00` used 128-bit SIMD equality for raw names <=16 bytes, retaining the exact 256-bit path for lengths 17..32 and the unchanged long-name comparison.
- Standalone 3.2M-query, 100k-name randomized equality microbenchmark on AMD EPYC (not the official six-P-core box): original AVX2 25.4..34.9 ms; SSE split 38.5..40.1 ms across four runs. This is a microkernel screen, **not** full T/M/S/D/K or a correctness qualification.
- Additional 13 scored source lines and no demonstrated speedup. Rejected before promotion; restored exact qualified `join.cpp` blob `3b5e1392f49abdbb7fdcf6a36b5866366298143b` in `286b9ce`. A push-triggered CI run may exist, but its status is not visible through the available commit-status/workflow-read interface; no CI pass is claimed.

## Denser shared-name table trial (2026-11-01)

- Trial `f488fb0`: 128 shards x 1024 slots versus the qualified 128 x 2048 table, with unchanged S/D.
- [CI 41316043568](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41316043568) passed full seed-1 hash, full holdouts 11/29, hostile tests, sanitizers, pinned offline Docker and sandbox.
- Direct full-PGO paired median **62.749 -> 65.153 ms** (+3.83% T); cgroup median **11,008,000 -> 10,496,000 B** (-4.65% M).
- Complete-K runner proxy **-0.92%** at unchanged S/D. Reject: reduced index memory does not offset longer probes.
- Restored exact qualified `join.cpp` blob `3b5e1392f49abdbb7fdcf6a36b5866366298143b` in `f6b7fb9`; no redundant restoration CI.

## 128-row deferred dictionary-probe promotion (2026-11-01)

- Trial [4bcec24](https://github.com/insightlabs38-pixel/sd-tc/commit/4bcec2445cb670f2e3b7d22efc10864d4e97fdb3) parses 128 records before preparing/prefetching shared-name slots, then performs exact membership updates. Previously qualified 32-row interleaved champion is unchanged as rollback (`3b5e1392f49abdbb7fdcf6a36b5866366298143b`).
- [CI 41321611522](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41321611522) passed seed-1 full reference SHA-256, full holdout seeds 11/29 (three repetitions), hostile quote/hash cases, ASan/UBSan/TSan, pinned offline Docker and sandbox.
- Direct qualified-champion 12-trial full-PGO paired median **76.761 -> 74.794 ms** (9/12 paired trials faster); fresh Docker cgroup peak **11,026,432 -> 11,026,432 bytes**. Source-only median **83.066 -> 79.634 ms**. Scored physical C++ lines **567 -> 569**, D=1; complete-K runner proxy **+1.25%** assuming a two-line S increase.
- **Promoted in development repo only**: qualified source blob `cf7e7c3fb12a6054836d5010c391cb63c59e0c48`. Official six-P-core T/M/S/D/K remains unmeasured; Luna controls integration and submissions. Research batch128 measurements on a different earlier architecture are supporting context, not a substitute for this direct comparison.

## Batch index-line prefetch trial (2026-11-01)

- Trial [5fb1f69](https://github.com/insightlabs38-pixel/sd-tc/commit/5fb1f69e2fac407ce18d563c58a449302e2bc7a6) renewed shard-index cache lines for 128 pending records before dictionary probes.
- [CI 41326701816](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41326701816) passed full reference SHA-256, holdouts 11/29, hostile cases, sanitizers, pinned offline Docker and sandbox. CI now uses qualified baseline 4bcec24.
- Direct full-PGO 12-trial paired median **74.727 -> 75.780 ms** (+1.41% T); cgroup **11,024,384 -> 11,038,720 bytes** (+0.13% M). Source-only 81.789 -> 82.248 ms.
- Physical source 569 -> 575 lines, D=1. Approximate complete-K proxy **-0.88%** using physical-line S; no official scoring-box measurement. Rejected.
- Restored qualified 128-row champion source blob cf7e7c3fb12a6054836d5010c391cb63c59e0c48 in cd25f7e; no redundant restoration CI.

## Dynamic multi-chunk scheduling trial (2026-11-01)

- Trial [212699f](https://github.com/insightlabs38-pixel/sd-tc/commit/212699f54feb938ba17d64d7dfd822ace6063a35) split the full record range into 24 exact record-boundary chunks, dynamically claimed by six workers; each worker retained private aggregates.
- [CI 41327177516](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41327177516) passed full seed-1 SHA, full holdouts 11/29, hostile quote/BOM cases, ASan/UBSan/TSan, pinned offline Docker and sandbox.
- Direct 128-row champion full-PGO paired median **73.576 -> 74.033 ms** (+0.62% T), cgroup **11,022,336 -> 11,034,624 bytes** (+0.11% M). Source-only median 79.616 -> 80.023 ms.
- Physical source 569 -> 578 lines, D=1; approximate complete-K proxy **-0.57%** using physical-line S. Four-vCPU runner does not establish six-P-core results, but there is no qualifying gain. Rejected.
- Restored exact qualified source blob cf7e7c3fb12a6054836d5010c391cb63c59e0c48 in 0659a0d; no redundant restoration CI. Further scheduling sweeps are lower priority than parser/dictionary architecture.

## Compact 56-byte exact-name slot promotion (2026-11-01)

- Candidate [cdc22e6](https://github.com/insightlabs38-pixel/sd-tc/commit/cdc22e6bceab01a68cf0c5cf4f050fb9c38e7d1f) changes shared-name slots from 64 to 56 bytes: 24 inline key bytes, pointer-backed exact longer keys, unchanged 256-bit per-name country membership. Masked AVX2 load avoids reading concurrently modified membership bits.
- [CI 41327676936](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41327676936), attempts 1 and 2, passed full seed-1 SHA-256, independent full holdouts 11/29, hostile collision/24-25-byte cutoff/long-name tests, ASan/UBSan/TSan, pinned offline Docker and sandbox.
- Direct qualified 128-row full-PGO 12-trial paired medians: **100.018 -> 100.033 ms** and **46.725 -> 45.896 ms**; cgroup **11,016,192 -> 10,240,000 B** and **11,067,392 -> 10,237,952 B** (7.0%/7.5% lower). Runner hosts varied substantially; compare paired results, not absolute times.
- Physical source 569 -> 570 lines, D=1. Approximate complete-K runner proxies **+1.44% / +2.46%** using physical-line S. This is not an official six-P-core T/M/S/D/K measurement.
- **Promoted development champion** source blob d5e0becb4a61f7363c4d1a94c73dd83dcb69436a. Qualified 64-byte-slot rollback blob cf7e7c3fb12a6054836d5010c391cb63c59e0c48 retained in 4bcec24; Luna controls official scoring and submission.

## 48-byte exact-name slot trial (2026-11-01)

- Candidate [1909d03](https://github.com/insightlabs38-pixel/sd-tc/commit/1909d036db4f415c25c3314c3b29ef7bbb4c75e5): 16-byte SSE inline keys, exact pointer-backed longer keys, 48-byte slots versus the qualified 56-byte champion.
- [CI 41331560100](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41331560100) passed full seed-1 SHA, holdouts 11/29, hostile name/collision cases, ASan/UBSan/TSan, pinned offline Docker and sandbox.
- Direct champion full-PGO 12-trial paired median **74.787 -> 77.278 ms** (+3.33% T); cgroup peak **10,381,312 -> 10,502,144 B** (+1.16% M); physical C++ lines **570 -> 569**, D=1.
- Approximate complete-K runner proxy **-1.83%**, including T/M/S/D; not official scoring-box evidence. Extra arena copies for 17..24-byte keys plausibly offset the smaller slots.
- **Rejected.** Restored exact qualified champion blob `d5e0becb4a61f7363c4d1a94c73dd83dcb69436a` in `62067a8`; no redundant restoration CI.

## Remove premature dictionary-index prefetch (2026-11-01)

- Candidate [220f940](https://github.com/insightlabs38-pixel/sd-tc/commit/220f94068151bdafed4c5a705a6186a01e14cf9c) removes both parser-stage index prefetches, keeping 128-row deferred exact-name probes and unchanged semantics.
- [CI 41331993267](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41331993267), attempts 1 and 2, passed full seed-1 SHA, holdouts 11/29, hostile cases, sanitizers, offline Docker and sandbox.
- Direct 56-byte champion full-PGO paired medians: **61.932 -> 61.616 ms** (9/12 faster), **74.178 -> 73.949 ms** (6/12 faster); cgroup **10,229,760 -> 10,278,912 B** and **10,250,240 -> 10,301,440 B**.
- Source **570 -> 564 physical C++ lines**, D=1; approximate complete-K runner proxies **+0.32% / +0.21%** including T/M/S/D. Small gain near timing noise, not official scoring-box K.
- **Promoted development champion** source blob `b02c0e99543f228462b75840be8e1f13cfcb6b26`; qualified rollback `d5e0becb4a61f7363c4d1a94c73dd83dcb69436a`. Luna owns official integration.

## Deferred region-join batch trial (2026-11-01)

- Candidate [4e5c38d](https://github.com/insightlabs38-pixel/sd-tc/commit/4e5c38d06985946847497c08625aa54dab86001c) moves region-to-country lookup from the parser loop into the 128-row dictionary-probe phase.
- [CI 41332781590](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41332781590) passed full seed-1 hash, holdouts 11/29, hostile cases, sanitizers, offline Docker and sandbox.
- Direct qualified champion full-PGO 12-trial median **48.240 -> 48.112 ms** (5/12 paired faster); cgroup **10,242,048 -> 10,487,808 B** (+2.40% M). Source 564 -> 564 lines; D=1.
- Complete-K runner proxy **-0.34%** at unchanged S/D, and speed difference is not convincing. **Rejected**; exact champion blob `b02c0e99543f228462b75840be8e1f13cfcb6b26` restored in `20d3472`.

## 256-row parse/probe depth trial (2026-11-01)

- Candidate [85daef5](https://github.com/insightlabs38-pixel/sd-tc/commit/85daef5a6e25c465999188812722f5aac8e7156a) doubles deferred dictionary batches from 128 to 256 rows.
- [CI 41333064922](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41333064922) passed full seed-1 SHA, holdouts 11/29, hostile cases, sanitizers, offline Docker and sandbox.
- Direct qualified champion full-PGO 12-trial median **73.898 -> 74.649 ms** (+1.02% T, 4/12 paired faster); cgroup **10,252,288 -> 10,479,616 B** (+2.22% M), unchanged 564 source lines and D=1.
- Complete-K runner proxy **-0.94%**. **Rejected**; exact champion blob `b02c0e99543f228462b75840be8e1f13cfcb6b26` restored in `a23a9b1`, without redundant restoration CI.

## Immediate region-prefetch removal (2026-11-01)

- Candidate [acc1dc4](https://github.com/insightlabs38-pixel/sd-tc/commit/acc1dc4d8f93dd7629fdce6ab7eb8ae9efa900f2) deletes the prefetch immediately preceding the `regions[region_key]` load; no semantic changes.
- [CI 41333867256](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41333867256), attempts 1/2: full seed-1 SHA, holdouts 11/29, hostile tests, ASan/UBSan/TSan, offline Docker and sandbox passed.
- Attempt 1 direct champion full-PGO 12-pair median **87.078 -> 86.356 ms**, 10/12 faster; cgroup **10,287,104 -> 10,250,240 B**. Approximate complete-K proxy **+0.52%**.
- Attempt 2 independent full-PGO 12-pair median **47.749 -> 49.693 ms**, 5/12 faster; cgroup **10,383,360 -> 10,244,096 B**. Approximate complete-K proxy **-1.68%**.
- Physical C++ source **564 -> 563 lines**, D=1. The slower, faster-host run outweighs the unreplicated gain; no official six-P-core result. **Rejected**.
- Restored exact qualified source blob `b02c0e99543f228462b75840be8e1f13cfcb6b26` in [eeea8aa](https://github.com/insightlabs38-pixel/sd-tc/commit/eeea8aae90e5614161f9ee62a6eb19c363797f4d); no redundant restoration CI.

## 40-byte projected-row batch promotion (2026-11-01)

- Candidate [486e182](https://github.com/insightlabs38-pixel/sd-tc/commit/486e18272e97615fff4532e59eeae355e998c589) reorders pending fields and stores per-row cents in signed 32 bits; organizer-generated amount range fits, while aggregate sums remain 64-bit.
- [CI 41334481715](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41334481715), attempts 1/2: full seed-1 SHA, independent full holdouts 11/29, hostile cases, ASan/UBSan/TSan, pinned offline Docker and sandbox passed.
- Direct champion full-PGO paired medians: **60.993 -> 60.746 ms** (7/12 faster) and **74.791 -> 74.387 ms** (10/12 faster).
- Cgroup median peaks: **10,252,288 -> 10,244,096 B** and **10,250,240 -> 10,242,048 B**; 48-byte -> 40-byte pending row, physical C++ lines unchanged, D=1.
- Approximate complete-K runner proxies **+0.22% / +0.29%** including T/M/S/D; not official six-P-core evidence. Source-only performance did not improve, but official Docker uses full-PGO.
- **Promoted development champion** source blob `b4ff26f2b04cb06a1d03a923b82359b3b63f2a23`; qualified rollback `b02c0e99543f228462b75840be8e1f13cfcb6b26` retained. Luna controls official integration.

## Monotonic seen-bit load trial (2026-11-01)

- Candidate [3cb9b04](https://github.com/insightlabs38-pixel/sd-tc/commit/3cb9b040e5be3bafc19e68541d5e8441e83f5d8b) checks a relaxed atomic load before the locked `fetch_or` for exact-name country bits. Monotonic set-only bits make the shortcut correct.
- [CI 41336883538](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41336883538) passed full seed-1 hash, independent holdouts 11/29, hostile cases, ASan/UBSan/TSan, pinned offline Docker and sandbox.
- Direct qualified-champion full-PGO 12-trial medians **74.122 -> 78.015 ms** (+5.25% T); cgroup **10,250,240 -> 10,237,952 bytes** (-0.12% M). Physical source **564 -> 568 lines**, D=1.
- Approximate complete-K development proxy **-2.61%**; no official board measurement. **Rejected** and restored qualified champion blob `b4ff26f2b04cb06a1d03a923b82359b3b63f2a23`.

## Direct CRC32C shard/index mapping promotion (2026-11-01)

- Candidate [108a4ad](https://github.com/insightlabs38-pixel/sd-tc/commit/108a4ad099b8ac26e6d2d0bdf9efab067f6bec09) uses the CRC32C hash directly for shard/index placement, removing two XOR-shift mixing operations. Exact-name equality and overflow behavior are unchanged.
- [CI 41337294610](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41337294610), attempts 1/2, passed full seed-1 SHA, holdouts 11/29, hostile cases, ASan/UBSan/TSan, offline Docker and sandbox.
- Direct qualified-champion full-PGO paired medians: **49.443 -> 48.352 ms** (9/12 faster) and **61.243 -> 60.257 ms** (10/12 faster). Cgroup medians: **10,487,808 -> 10,397,696 B** and **10,240,000 -> 10,231,808 B**.
- Source remains **564 physical C++ lines**, D=1. Approximate complete-K runner proxies **+1.30% / +0.83%** including T/M/S/D. No official six-P-core measurement yet.
- **Promoted development champion** source blob `64e6c1f91f8ce859cf75c34990c964754e070b50`; qualified rollback blob `b4ff26f2b04cb06a1d03a923b82359b3b63f2a23`. Luna retains official scoring/integration authority.

## Short-name CRC fast path and collision-fixture qualification (2026-11-01)

- Candidate [9e4676e](https://github.com/insightlabs38-pixel/sd-tc/commit/9e4676e9f897b14e8a834c9874ca2554e5dfecd7) used one CRC32C on zero-padded <=8-byte names; all other hash paths stayed unchanged.
- [CI 41341716883](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41341716883) passed full reference hash, holdout seeds 11/29, hostile tests, ASan/UBSan/TSan, pinned offline Docker and sandbox.
- Direct full-PGO paired 12-trial medians **48.333 -> 49.870 ms** (+3.18% runtime); cgroup **10,496,000 -> 10,237,952 B** (-2.46% memory). Physical source **564 -> 571 lines**, D=1; complete-K runner proxy **-1.24%** including T/M/S/D. Source-only 53.167 -> 55.040 ms. Rejected, exact champion blob `64e6c1f91f8ce859cf75c34990c964754e070b50` restored in `832190c`.
- Review found existing collision fixtures used the obsolete XOR-spread mapping and full-byte CRC for long names; they did not actually force current shard overflow or long-key hash collisions. [4b47a52](https://github.com/insightlabs38-pixel/sd-tc/commit/4b47a525c1c4dbddced3def26b216e1afdc22279) corrected the test model, with 4,200 exact names in one 2,048-slot shard and two distinct 40-byte keys with identical production hash.
- [CI 41341960343](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41341960343) passed the corrected concentrated overflow and full-hash collision fixtures, full seed-1 reference SHA, holdouts 11/29, sanitizers, pinned offline Docker and sandbox. No scored source change; the champion remains qualified. No official T/M/S/D/K or submission is claimed.
- [CI 41342247143](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41342247143) additionally ran the actual 4,200-name concentrated overflow fixture under **both TSan and ASan/UBSan**; both passed exact reference bytes. Full verify, Docker, seed-1 and holdout seeds 11/29 also passed. Champion source remained unchanged.

## Parser-stage name-index prefetch trial (2026-11-01)

- Candidate [ed5b86b](https://github.com/insightlabs38-pixel/sd-tc/commit/ed5b86b7914b9751d98035c0d3bf33b1a4ed17b2) prefetched the sharded name-index slot immediately after CSV name hashing, before parsing the region and amount.
- [CI 41347301633](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41347301633) passed full seed-1 hash, holdout seeds 11/29, hostile cases, ASan/UBSan/TSan, pinned offline Docker and sandbox.
- Direct champion full-PGO 12-trial median **60.762 -> 61.196 ms** (+0.71% T); cgroup peak **10,270,720 -> 10,242,048 bytes** (-0.28% M); source **564 -> 566 physical lines**, D=1. Complete-K runner proxy **-0.35%**; rejected.
- Restored exact qualified source blob `64e6c1f91f8ce859cf75c34990c964754e070b50`. The source-only 65.703 -> 65.125 ms and pinned PGO-vs-untrained 63.033 -> 60.894 ms are not the direct full-PGO champion comparison.

## Seen-first 56-byte name-slot layout trial (2026-11-01)

- Candidate [513313b](https://github.com/insightlabs38-pixel/sd-tc/commit/513313b5e9e0ef01261b1a7bf162cfa9ed411f5a) moved the 32-byte atomic country-presence bitmap before the 24-byte exact key, adjusting the key prefetch. Slot size, source lines and D=1 remained unchanged.
- [CI 41347661028](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41347661028), attempts 1/2, passed full seed-1 hash, holdouts 11/29, hostile cases, ASan/UBSan/TSan, pinned offline Docker and sandbox.
- Direct full-PGO 12-trial paired medians **72.899 -> 72.593 ms** and **72.910 -> 73.229 ms**; cgroup medians **10,430,464 -> 10,233,856 B** and **10,254,336 -> 10,254,336 B**. Complete-K development proxies **+0.59% / -0.22%**.
- **Rejected** due contradictory independent measurements and no replicated K gain. Restored qualified key-first champion blob `64e6c1f91f8ce859cf75c34990c964754e070b50`; no redundant restoration CI.

## 48-byte inline-key name-slot trial (2026-11-01)

- [CI 41351958200](https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41351958200) passed seed-1 full reference, independent full holdouts 11/29, hostile collisions/overflow, 16/17-byte key boundaries, ASan/UBSan/TSan, pinned offline Docker and sandbox.
- Replaced 56-byte slots with 48-byte slots; names up to 16 bytes remain inline, longer names use exact arena-backed comparison. Full-PGO paired medians **61.440 -> 63.600 ms**; cgroup median **10,283,008 -> 10,508,288 B**; physical source **564 -> 563 lines**, D=1.
- Complete-K development proxy **-2.11%**. Rejected: extra long-name indirection and arena pressure outweighed slot-size savings. Restored qualified source blob `64e6c1f91f8ce859cf75c34990c964754e070b50`; no redundant restoration CI. Official T/M/S/D/K unchanged and unmeasured for this candidate.
