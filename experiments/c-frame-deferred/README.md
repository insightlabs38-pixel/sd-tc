# Projected records and deferred exact lookup

Experimental challenger based on qualified PR12 (8bee2ee3a2cb027098f5b95a4279bfb9a30da785).
Raw-name metadata shrinks24->16 bytes; decoded length moves to the pending row.
Money and all aggregates stay64-bit, without a narrowed-record fallback.
Pending rows shrink56->48 bytes. A separate pointer array holds prepared name
slots: acquire-probe fingerprints, prefetch immutable keys/membership, then
verify full bytes and claim membership. A false fingerprint calls the same
locked exact slow path. Published payloads remain immutable; overflow unchanged.

Local pinned GCC15 PGO seed37, randomized12-trial restricted2CPU containers:
PR12118.843ms/273.195CPUms/10,100,736B;
combined99.384ms/232.811CPUms/10,201,088B, canonical S622->637,D1.
Complete local K proxy+8.7% using wall, +7.8% using CPU as sensitivity only.
THP-disabled counterpart108.698ms/234.019CPUms/10,164,224B,S639.
All full seed1 output hashes exact; qualification pending CI. No official K claim.

Pinned CPU sampling of a dynamic non-LTO PGO proxy put~19% of sampled CPU after
exact-key vector loads and~7% after membership atomics. These are PC samples,
not hardware cache-miss counters. The layout and deferred lookup address these
loads and temporary-structure traffic. Profile receipts live in campaign archive.
Main d3b9a32 runtime remains f294a88; official BEST c0969507 still source59974fd6,
K21.029207,T73.738ms,M14,114,816B,S552,D1. Rollbacks retained.

## Qualified sourcef0b0434

https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41255591511 passed all
gates: full1 hash, independent full11/29 x3, collisions/concentrated overflow,
long records, page-tail/money boundaries, ASan/UBSan, TSan, pinned offline Docker
and restricted unprivileged execution.
FullPGO main101.952->83.448ms; official BEST source101.709->83.303ms.
Direct qualified PR12 control97.498->85.557ms; cgroup10,473,472->10,498,048B;
S622->637,D1, complete runnerK ratio1.063 (+6.3%). Not official K.
Same-source untrained GCC15 was78.309ms versus PGO82.951ms in a separate paired
step; release configuration is being investigated, not assumed universally best.
This source includes PR12; select one complete runtime context, not both patches.
Rollback: PR12 runtime blob8bee2ee3a2cb027098f5b95a4279bfb9a30da785,
main f294a88/a9b2c765, official59e47be/59974fd6.
