# Track C breakthrough campaign, 2026-11-01

This is an isolated, executable research campaign, **not** an official scoring submission.
No writes were made to `main`, `sd-sm`, or scheduled development branches.

## Frozen context and boundary
- Current official BEST as checked from board snapshot 08:24 UTC: HighVelocity K 25.979773, T 50.141 ms, M 11,288,576 B, commit `6917275`.
- NickTrapp BEST K 47.499505, T 18.018 ms, M 12,754,944 B. Source not publicly verified.
- GitHub `main` source baseline blob `1cfd7405b24cfff6f855f16ef0ad20dda38ad5b7`; commit `f46c57e`.
- All experiments verify the independent reference's full seed-1 SHA256 `d8088b1cd42ba1338b1345b107b55db7b96ef1f5066f3ff64fd26a8912a0f536`.
- Keep developer/Luna in charge of integration and official qualification, and keep the research branches isolated.

## Read-only evidence reviewed
- Official `specs/c-join.md` and source, generator, pinned harness, scoring formula.
- `research/c-architecture-breakthroughs-20261031`: PCLMUL64 scan/CRC isolated gain of 27.6%, no whole-system proof.
- Existing PRs #15, #16, #17 and detailed `research/c-independent-20261101/REPORT.md`: private dictionaries, owner routing, radix, country-major bitsets, etc.
- Public `manusingh090/speed-demon-2026-entry` Track C source/PERF, verified faster on historical official receipts; uses a 64-bit probabilistic key for long names, **not acceptable as exact equality**.
- `rj-karan/speed` source/PERF with exact shared keys, official historical best K 27.280554; no invented frontier claim.
- The original MIT competitor implementation was adapted only for isolated independent research, with attribution in source.

## Complete executable candidates
| Branch suffix | Source commit | Source blob | Architecture |
| --- | --- | --- | --- |
| (root) | 0838243 | 47fa8bbb4c93164b2d9e86d2a7ea758ee5275bd1 | 8 dynamic tiles/worker, adaptive up to 12 workers, 64-row parse then exact probe |
| -tiles6 | d59c61d | 0624b93f73b4faa6aede444ffd44141cb61bc286 | Same with max 6 workers |
| -batch128 | 26681ca | 486a9af952b63536fb1fa341e85e1a9c788e4cba | 128-row deferred-probe window |
| -batch256 | 7eebdd4 | 30301a2aa6d4e4afdf30488536bba5be318db7ab | 256-row deferred-probe window |
| -batch512 | 06c9781 | 5d64f0dc3826730fba97dc7d413690ee522cedb3 | 512-row deferred-probe window |
| -fieldscan | 51c5643 | f2822153572c23ad7380b7a90f7d9277f6f13f4b | Field-by-field exact memchr quote decoder, replacing PCLMUL text-pair scanner |
| -fieldscan128 | 52b9e62 | ecc9537d0f8c871c0d1238fb653a56033eadd55d | Field decoder plus batch 128 |
| -exact-pipeline | 36e0f12 | 46209970ae85ae1690a3aea4aa972c3fcd2c8e86 | Competitor-inspired worker phases + exact 64-bit tags, copied full keys/tails, overflow identities |
| -exact-quadprobe | f766a5c | 00144b5d08241bd910a8bd7d9dbd2053ce946121 | Four independent acquire tag loads, full-byte equality, same pipeline |

The exact compact pipeline caught two genuine assumptions in the public code during sanitizer/adversarial checks: unsafe numeric length shifts and an eight-byte-only delimiter scan. Both were fixed; the completed pipeline passed ASan/UBSan and TSan. Later performance regressions are measured below, not misrepresented as impossibility.

## Qualification
- Root: full CI PASS at https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41289093589
- Six-worker: PASS at https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41289747986
- Batch128: PASS at https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41289807251
- Field-specific decoder: PASS at https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41289938209
- Exact compact pipeline: PASS at https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41289722060
- Full CI includes seed-1 full, two extra full seeds, adversarial oracle, independently generated data, ASan/UBSan, TSan, pinned offline GCC15 PGO Docker and output/sandbox checks.
- For the independent exact pipeline only, the incumbent-specific source rewriting profiler is skipped in CI; the rest of its qualification remains intact.

## Nine-run interleaved local whole-solver matrices
These are **development** wall times on pinned CPUs 0,1, NOT official K or official CPU measurements.
All variants produced the same full output hash on every measured run.

| Candidate | Matrix 1 ms | Matrix 2 ms | Matrix 3 ms |
| --- | ---: | ---: | ---: |
| incumbent | 116.405 | 99.647 | 154.768 |
| root batch64 | 106.547 | 96.069 | 144.402 |
| six-worker batch64 | 106.301 | 95.352 | 143.957 |
| batch128 | 105.571 | 95.775 | 144.069 |
| batch256 | — | 96.152 | 146.723 |
| batch512 | — | 96.652 | 150.970 |
| fieldscan | 111.897 | 105.595 | 163.444 |
| fieldscan128 | — | 105.768 | 164.323 |
| exact compact pipeline | 134.903 | 126.793 | 192.857 |
| four-way exact probe | — | — | 199.329 |

Runs: https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41290081951 ,
https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41290216851 ,
https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41290327422 .
Machine interference changed absolute times substantially; only same-run *paired ratios* are meaningful. Batch128/root/6-worker repeatedly beat incumbent by ~4–9%. Fieldscan/exact quad are inferior to the leading batch design. Batch256/512 do not offer reliable additional speed, with matrix RSS ~65–68 MiB versus ~40 MiB for batch128. No matrix uses the official 12 P-core scoring host; thread-count comparisons on a two-CPU-quota CI runner do not establish official scaling.

## Pinned PGO independent decision gate
PASS: https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41290459067
`research/c-breakthrough-20261101/pgo_matrix.py` built five immutable full executables with the pinned official Dockerfile, including seed-37 full PGO, and completed nine interleaved runs each with exact hash on two pinned CI CPUs.

| Candidate | Pinned PGO median ms | GHA process median RSS KiB |
| --- | ---: | ---: |
| incumbent | 176.532 | 41,676 |
| root batch64 | 167.503 | 29,792 |
| batch128 | **167.384** | 30,916 |
| batch256 | 167.669 | 56,384 |
| fieldscan128 | 168.226 | 30,792 |

Batch128 reduces wall time by 5.18% versus that immutable incumbent on the same runner. The batch64 difference is within noise. These figures are not official board K, and per-process RSS on Linux includes page-fault noise; use fresh Docker cgroup measurements for official M extrapolation.

All complete-solver variants described here have fully passing adversarial, sanitizer and Docker workflows, including batch256 https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41290181197 , batch512 https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41290191972 , fieldscan128 https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41290199500 , and quadprobe https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41290315988 .

## Conclusions / remaining high-upside work
- Strongest qualified default: **batch128** at `research/c-breakthrough-20261101-batch128` / `26681ca`. Root batch64 is equally reasonable within noise. Neither is an official submission.
- A fundamentally different exact compact dictionary was built and qualified, but loses end-to-end on this test host. Four-way acquire probes did not recover the lost cycles. The loss is not evidence all exact compact or owner architectures are impossible.
- Remaining architectural frontier: exact key verification fused with a compact control table and fewer serial random pointer loads; decouple metadata probing from membership cacheline writes without a second random access; batch multiple independent exact lookup chains; reorder name identification relative to parsing; test on actual 12 P cores.
- Do **not** claim K 43–47 or an 18ms solution. The current rigorous measurements only establish a modest relative full-system improvement. Preserve failure evidence.
