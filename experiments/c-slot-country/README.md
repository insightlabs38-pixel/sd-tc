# Exact slot-major keys and country-major membership

Experimental challenger based on f294a882897e2ceb940f8878306b30b80c5e9829.
24-byte exact immutable name slots publish full hash/length with acquire/release.
Slot positions index country-major membership directly; no key-ID dependent load
or dense-ID allocation. Worker-private arenas retain complete long names.
Three stages parse/prefetch, intern/aggregate/prefetch membership, and atomic OR.
Insertion claims an empty slot with CAS, writes its key, then publishes once.
Busy slots are always waited on. At 7/8 occupancy, exact locked overflow takes
new names. Accepted insertions claim slots before the occupancy RMW release;
full-mode readers acquire the occupancy counter and reprobe from the home slot,
waiting for all encountered claims. This prevents primary/overflow duplicates
at the capacity transition. Long allocations precede claiming, so throwing
allocation cannot leave a slot busy. No shared payload is changed after publish.

Initial GCC14 non-PGO full seed1 local12 paired observations:
control186.248ms/341.694CPUms, challenger189.226ms/354.580CPUms;
RSS35,934/35,704KiB; all24 outputs exact. Counter saturation logic was
subsequently strengthened; this initial run never reached saturation.
Local runner variation is large; no official K projection or promotion claim.
Pinned PGO, sanitizers, independent full seeds and canonical S pending CI.
Default C_LAG16, C_PREFILL1(table only), C_THPOFF1, C_NAME_BITS17.

CI365b8f3: https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41251416312
All gates passed, including randomized concurrent primary/overflow cutover,
ASan/UBSan, TSan, pinned offline GCC15 PGO/sandbox, full11/29 x3.
FullPGO main100.237->114.803ms, RSS16,620->16,620KiB, S577->654,D1.
Runner completeK ratio0.917 (RSS proxy for M); rejected versus main.
OfficialBEST110.913->114.157ms, RSS16,892->16,624KiB,S552->654.
Do not promote. Hypotheses: high occupancy creates multiple24-byte key probes;
unlike separate compact control bytes, unsuccessful probes read full key lines.
Next realization separates byte atomic controls from exact16-byte keys, retaining
physical-slot bitsets and prefetching both home control and home payload.
