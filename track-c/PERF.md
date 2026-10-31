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
