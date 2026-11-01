
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
