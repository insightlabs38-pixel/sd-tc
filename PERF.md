
## Exact compact field pipeline, 2026-11-01

Independent fingerprint locator with full-byte canonical verification, separate
country bitsets, three bounded record rings, field masks and work-based phases.
Every matching locator verifies full bytes. SIMD compares copies of acquire
atomic loads. Canonical input stays mapped until every lookup completes.
No probabilistic identity, external code or direct dependencies were added.

Local pinned offline PGO full1: champion153.782ms/M10,936,320/S657 versus
candidate142.523ms/M8,691,712/S784; relative complete K1.0591. Seven
interleaved runs, middle5 runtime, maximum cgroup peak, two-quota-CPU host.
Full11 five pairs:158.603ms/M10,850,304 versus143.124ms/M8,626,176.
Full1/11/29/137 hashes match. Hostile ASan/UBSan and TSan pass locally.
Public research source/receipts: PR15, commit6522767; further receipts pending.
This is a challenger for CI/scoring-host validation, not an official score.

Rollback: source-equivalent official champion e253190 (PR14), immutable
official6917275, source SHA25690ff49b2dbf26bc3d33e62c6e2e86ed990f19280e6b99d3e8dfca7426ec7ac65.
Luna alone handles official integration; do not promote on local speed alone.

### Inline16 canonical repair

PR18 original is not a winner: CI41291768401 passes all gates, but direct
fullPGO champion84.839ms versus114.479ms. Median fresh-container cgroup
11,028,480 versus8,925,184; relative complete K approximately0.87.

This branch stores all name bytes inline through16, retaining full-byte
verification for longer names. The locator still includes the full length.
Publication and all-byte identity are unchanged. Hostile ASan/UBSan/TSan pass.
Seven interleaved pinned Docker full1 runs: champion153.562ms/M11,005,952
versus inline16131.101ms/M9,601,024 (middle5, maximum cgroup peak). Canonical
S and independent-seed/CI comparisons are pending; no official-score claim.

### Dense anonymous pages

Dense control/canonical/membership storage requests huge pages for the used
first6MiB; the hint may be declined without affecting correctness. Ready hints
refer to immutable, acquire-published keys and still verify every byte.
Both hostile sanitizer suites pass locally. Seven-pair pinned Docker full1:
champion159.197ms/M10,821,632 versus119.373ms/M11,227,136; S657→807/D1.
This local gain needs CI validation; PR19 still loses CI and is not promoted.
No official repository change is authorized by this experimental PR.
