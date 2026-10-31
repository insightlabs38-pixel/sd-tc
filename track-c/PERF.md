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
