# Independent discovery, before examining sd-tc implementation

Official upstream e098283 (fetched 2026-11-01). 3.32M records, ~202MB,
~100k names, ~240 groups. Notes need only decoded lengths. Group arithmetic
is associative; exact distinctness is set cardinality of (country,name).
Names have no newline; note newlines require quote-aware boundaries. CSV
encoding of nonempty generated names is canonical, so raw interiors can be
compared exactly; both empty forms must normalize. Full-byte collision checks
remain necessary. Pool size/popularity are planning distributions, not caps.

## Distinct computational families

1. **Owner partitioned relation.** Parse independently into compact descriptors
   (hash, input offset, country); radix-scatter to name owners. Owners build
   private exact dictionaries with country membership bits. Arithmetic stays
   parser-local. O(bytes+records), ~16-24B/row intermediate, no shared updates.
   Cost: one scatter/barrier and rereading names. Potential: eliminates all
   contested dictionary and membership traffic, potentially >1.3x CPU throughput.
2. **Sort and segmented reduction.** Parse arithmetic locally; sort descriptors
   by name hash then exact bytes, union countries within equal-name runs. Radix
   sorting avoids comparison-sort's O(N log N); sort country/name pairs only
   after dictionary compression. ~2-6 sequential descriptor passes, 100-300MB
   extra traffic. No shared dictionaries. Can win when random probes dominate.
3. **Private dictionaries, late reconciliation.** Each parser interns names
   privately and maintains local country bitsets. Merge only distinct local
   names (~workers*100k), OR membership, then count. O(N) private probes plus
   O(workers*U) reconciliation. ~4MB country matrix per worker plus dictionary.
   Potential: removes synchronization without materializing all record keys.
4. **Country-owner aggregation.** Parse and partition records by country, then
   independently aggregate and deduplicate exact names in cache-sized per-group
   dictionaries. No global name ID needed. Distinct pairs approach N (~3M),
   so state and repeated string equality can be expensive; bounded batches and
   sorted group runs can reduce residency. Potential: removes global dictionary.
5. **Prebuilt exact dictionary then immutable probes.** First extract only name
   descriptors and construct exact dictionary in parallel; second parser pass
   uses immutable lookup and thread-private membership bitsets. Pays another
   input traversal but removes insertion/locking and per-record descriptor
   storage. Sampling may seed dictionary but requires exact miss fallback.
6. **Structural column pipeline.** Parallel quote parity produces record and
   field masks, stages decode numeric columns in batches, then vectorized join
   and local arithmetic. Name descriptors flow separately to any distinct
   backend. O(bytes), mask/intermediate traffic 4-24B/record. Extra passes may
   beat scalar dependent parsing by exploiting instruction and memory parallelism.
7. **Region-major partial reduction.** Accumulate arithmetic and name sets per
   region or radix-partition numeric region IDs before joining, then reduce to
   country. Avoid repeated joins but 200k arithmetic states and many sets cost
   memory. Hybrid: precompute direct numeric lookup and batch joins; reserve
   full region-major plan for strong measured join bottleneck.
8. **Exact text-key trie / prefix compression.** Build adaptive radix/trie or
   prefix-partition names, store membership on leaves, reuse common prefixes.
   No hashes/collision overhead. Pointer chasing and insertion synchronization
   are risks; succinct prefix buckets or sorted front-coded keys offer better
   cache behavior than a naive trie. Potential depends on shared name prefixes.

Ranking before existing evidence: (3) lowest-risk large mechanism; (1) best
synchronization-free single-parse alternative; (2) serious sequential baseline;
(6) composable high-upside parser; (5) speculative two-pass challenger; (4,8,7)
need measured evidence. No family is rejected from a simple prototype estimate.

## Complete-K model

Official formula additionally caps memory ratio at 50 and source ratio at 1;
D=1+direct dependencies. For relative candidates (uncapped memory),
Knew/Kold=sqrt(Told/Tnew)*(Mold/Mnew)^.2*(Sold/Snew)^.15.
Doubling memory requires >24.2% faster time at equal S to break even; doubling
source requires >18.8% faster time. Mapping shared warmed input may have a
large RSS but does not necessarily charge its page cache to a fresh cgroup.
Measure cgroup peak; RSS must be explicitly labeled a proxy.

Host has only 3 vCPUs, Xeon 8573C, unlike official 6 P-cores/12 threads.
4/6/12 software thread trials here are oversubscribed and cannot establish
scoring-box scaling. Official qualification remains Luna's responsibility.
