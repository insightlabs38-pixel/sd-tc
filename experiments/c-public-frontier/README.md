# Public implementation frontier, 2026-10-31

Experimental challenger; not a promoted champion or an official receipt.
Rollback: main f294a882897e2ceb940f8878306b30b80c5e9829.
Official BEST: source 59e47be4236db4fc246673fec87e25bbd7231780,
blob 59974fd6328c2fabb1154ac4cc31bda1036bc6ca; K20.892890,
T74.402ms, M14,258,176B, S552, D1, run-804f529ac682.
Latest board at reconciliation: run-ac0bb7bc352e K20.728546 (not BEST).
Research pack SHA256 b0eed0f977beacb0e60cb8974edba5937bc0500b12af1a7b20fcc964e3f79a4a verified again.

## Exact country-major challenger

Original implementation informed by public architectural descriptions, no copied code.
Eight-byte atomic index entries publish immutable 16-byte exact keys; long names
retain full bytes. Full CRC32 and length shape filter candidates; every match
still receives byte equality verification. Dense IDs allocated in worker blocks
of 32 index country-major atomic bitsets. Shard mutexes serialize only insertion
and an exact overflow path. Published entries and payloads never change.
Four ring stages parse/prefetch, resolve index/aggregate, verify key/prefetch bit,
and update membership. Default distance16, six workers. THP disabled by default;
prefaulting and alternate distances remain experiments (C_THPOFF/C_PREFILL/C_LAG).
No non-atomic vector reads of concurrently written atomic index storage.

Local GCC14 non-PGO, quota2CPU, 12 interleaved full-seed1 runs:
main197.261ms / 370.453CPUms / 30,628KiB medianRSS;
challenger196.352ms / 377.555CPUms / 30,782KiB.
Large wall/RSS variation prevents a performance conclusion. Full hash identical
on all24 runs. Raw values in local-country.json. Do not project official K from this.
Pinned GCC15 PGO, source counting, independent full references, sanitizer and
restricted container gates pending CI. Complete source/context required; retain
Dockerfile -march=x86-64-v3. No source target pragma.

## Public research

Cache Me main.cpp/current history: 4c14e7d9da15, 1a34f09f5d8b,
b59a69495f9e, eb2259163834, 521af234b816. Current hash-only table is
probabilistic; its atomic-array SIMD reads also need C++ race scrutiny. We adopt
neither. Useful mechanisms: country-major bitsets, delayed prefetch stages,
page-table fault prevention, disabling THP, and smaller scheduling chunks.
FAHHHH main.c: AVX2/PCLMUL structural parity, dynamic1MB chunks, exact keys;
its shared overflow flag needs atomic synchronization before reuse.
rj-karan src/main.rs: exact prefix+input offset and shared membership, akin to
our earlier co-located design; retaining input pages has a memory tradeoff.
Mumuksh-Jain main.cpp: shard-lock-every-record and serial quote prepass;
both are less promising than our current lookup path.
coreyjlord-dev solve.cpp: exact24-byte slots, dense IDs, country-major bits;
fixed capacity and long-name length/arena bounds require general fallback.
All five repositories declare MIT; source/PERF read, research references:
https://github.com/manusingh090/speed-demon-2026-entry/tree/speed-demon/track-c
https://github.com/pratyuxxhh/speed-demon/tree/speed-demon/track-c
https://github.com/rj-karan/speed/tree/speed-demon/track-c
https://github.com/Mumuksh-Jain/speed-demon/tree/speed-demon/track-c
https://github.com/coreyjlord-dev/speed-demon/tree/speed-demon/track-c

Prior ownership queues, radix references and worker-private membership are
archived at frontier/campaign-round2-checkpoint 99c0ab29418a; no reason to repeat
them without addressing deferred random reads or their additional memory.
PR8 dense-inline was officially rejected (K19.435391); PR2 speculative splitting
was officially rejected (K11.387223). Neither is a stronger baseline.

Runtime SHA256: 709031335ef0d445c510bddca94e596004b74140a0ae64eff8ad7a1271ff65b4
Runtime git blob: ce1ad13e2bf957474337b6114ede8338929171c6

## Qualification result, 3c5e2f0

CI https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41250735862 passed
all gates: ASan/UBSan, TSan, adversarial oracle, pinned offline GCC15 Docker,
unprivileged sandbox and final PGO full seeds11/29 x3.
Paired full-PGO vs f294a88: 77.180 ->78.224ms, RSS16,636->16,572KiB.
Vs official BEST59e47: 84.010->76.668ms, RSS16,922->16,824KiB.
These are runner observations, not official T/M; source cost remains to be counted.
No PR: loses runtime to qualified main and adds source; investigate cheaper
exact lookup and collision handling before considering an official trial.
