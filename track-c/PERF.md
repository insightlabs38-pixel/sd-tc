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
