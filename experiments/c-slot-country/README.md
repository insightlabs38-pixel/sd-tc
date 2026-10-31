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
