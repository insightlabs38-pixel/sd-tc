# Public frontier campaign checkpoint, Nov1 2026

Work continues; this is an archive, not a promoted runtime or a PR.
All owned prototypes, source SHA256/git blobs, local build-source hashes/flags,
raw paired receipts and software-PC profiles are retained here. No competitor
implementation is copied into this archive. MIT research URLs and observations
are recorded in experiments/c-public-frontier/README.md.

Verified pack SHA256 b0eed0f977beacb0e60cb8974edba5937bc0500b12af1a7b20fcc964e3f79a4a.
Authoritative development control: d3b9a32, runtime a9b2c765 (same as f294a88).
Official BEST at23:53UTC: c0969507, run-df2932b2ba2d, K21.029207,
T73.738ms,M14,114,816B,S552,D1; runtime59974fd6, same59e47be rollback.
B BEST23.250490; D24.163880. No new frontier source officially scored yet.

Qualified winners:
- PR12 frontier/c-padded-short-paths, runtime8bee2ee3, S622/D1:
  exact fixed-width short CRC/equality and one-fold money. CI41251942615;
  offline Docker, ASan/UBSan/TSan and full11/29 x3. Local cgroup K proxy+5.0%
  versus main; runner memory RSS increase was file-backed residency, not M.
- frontier/c-frame-deferred-lookup f0b0434 (runtime unchanged by evidence commits),
  S637/D1, Frame16/Pending48 and deferred candidate lookup/prefetch. CI41255591511:
  PR12 97.498->85.557ms; cgroup10,473,472->10,498,048B; runnerK+6.3%.
  All gates passed. Same-source plain GCC15 beat PGO in this run; investigating.
- Separate12-worker4327a90 and THP-off0549cfb PR12 challengers passed all gates,
  CI41252568866/41252805537. No official performance claim or separate PR.

Qualified losers, retained without PR:
- Dense country pipeline3c5e2f0: fullPGO main77.180->78.224ms, S577->671.
- Direct24-byte slots365b8f3: fullPGO main100.237->114.803ms, S654.
- Byte-control combination9208a2b: PR12118.738->126.367ms,
  cgroup10,489,856->9,410,560B,S622->711: completeK-2.9%, CI41254034309.
- Initial full structural64 scanner3548cac: full hash/adversarial exact,
  35% slower single-worker CPU; not sanitizer/container qualified.

Current experiments (not promotion evidence): source/scripts and receipts cover
short-name/hash paths, quote-mask caching, full structural masks, guarded streaming
cursor with bounded numeric fields, compact pending records, staged exact lookup,
country-major bitsets, sharded/direct dictionaries, prefault variants, and exact
17/18-byte ASCII packing with full-byte fallback. New direct64 cache-line slots
and explicit PREFETCHW builds are being measured. -march=x86-64-v3 alone disables
-mprfchw: earlier write requests emitted read-prefetch; actual instructions matter.

Local GCC15.3 compiler extracted from the checksum-verified pinned GCC image layer
(manifest/script included), saving VFS disk. Local static links use host libc;
final package gates use the actual digest-pinned offline Docker build in CI.
PGO uses official seed37. Local binaries rewrite only data/output paths; manifest
records both original and compiled-source hashes. Full seed1 every timed run.
Random restricted-container receipts measure actual cgroup memory, not RSS.
The2CPU quota/3-visible-CPU VM and4-vCPU CI cannot settle scoring6P/12T behavior.

Software profile is a dynamic non-LTO PGO proxy, not scored T or hardware counters.
It identifies sampled PCs after key loads (~19%) and atomics (~7%), not exact cache
miss percentages. Prior Round2 work remains at99c0ab29418a independently.
