# Independent Track C architecture campaign

This is development evidence, not an official score or permission to integrate
into sd-sm. The independently written discovery pass is in PASS-I.md.

## Current verified official champion

Board snapshot 2026-11-01 06:51:08 UTC: HighVelocity best is run-e7eab9673214,
official commit6917275dbd2ee12fd19df6fe3b87a60920c86ef8, T50.141 ms,
M11,288,576 B, S657, D1, K25.979773. Retrieved its immutable source and
Dockerfile read-only: both are byte-identical to campaign PR14. Join blob
0772ace04e358868f0b060b178d6ab77e93a15ab, SHA256
90ff49b2dbf26bc3d33e62c6e2e86ed990f19280e6b99d3e8dfca7426ec7ac65.
The new source is preserved under baselines/ and public rollback branch
`rollback/c-independent-official-6917275`, pointing to source-equivalent sd-tc
commit e253190. Luna performed the official integration/scoring; this campaign
only verified the published result. NickTrapp remains K47.499505 at this
snapshot. Historical baselines below remain preserved for reproducibility.

## Frozen baselines and rules

- Official specifications/generator/harness: upstream e0982830d2d86cee090ec6d8d332d55450dba455.
- Development base: sd-tc e0982830d2d86cee090ec6d8d332d55450dba455,
  join blob b31de39258eec335f79e14a0568a325d4ad2c732.
- Board snapshot 2026-11-01 04:07:04 UTC: HighVelocity BEST
  run-181942cb429e, official commit 033819b04f41e7a02d02ab2b1e86241db716fc20,
  T=0.055956 s, M=21,192,704 B, S=637, D=1, K=21.782763.
- Retrieved that immutable commit's source and Dockerfile read-only. Its join
  is byte-identical to sd-tc da7fe7b's c4b41459c8ce1ea8d47e834a136cb9d11b73cd2a;
  its Dockerfile is byte-identical to this campaign's pinned PGO Dockerfile.
- Preserved exact files, hashes and rollback refs; see baselines/manifest.json.
  No sd-sm files, refs, PRs or workflows were changed.
- Board leader at that snapshot: NickTrapp K=47.499505, T=18.018 ms,
  M=12,754,944 B, S=303, D=1. Board gates do not prove exactness for all inputs.
- Official K caps memory ratio at 50 and source ratio at 1. D=1+direct deps.
  Our relative calculations use the official capped formula and pinned
  clang-format 23.1.3 / scc 4.1.0, including 13 Dockerfile code lines.
  The upstream-identical generator is free. Current official S reproduces
  exactly: 624 C++ lines + 13 Dockerfile lines = 637.

## Pass II: confrontation with existing evidence

PERF.md and the older research branch are evidence, not decision rules. The
research branch 477a5cdd1162b28f7c9046df02678ba16a956c0a reports isolated
presence-update comparisons: shared atomics ~5.9 ms, private bitsets ~8.5 ms,
and four-pass sorted preassigned integer IDs ~29.7 ms. Those kernels omit
string resolution and parsing and cannot decide an integrated architecture.

The round2-owner24-wip branch e73cfcf66b8c92cc66c47a1106d24f41e68cb4ff
contains an actual owner pipeline with mutex/condition-variable notification.
It is more than an estimate, but its PERF ends at earlier shared-name trials;
no completed owner-engine qualification/benchmark receipt was located. The
round2-reconciled-sse branch still uses SharedNames in its hot path and is not
a private-dictionary reconciliation engine.

A local per-thread exact-name cache was previously measured and lost, but that
is distinct from fully private dictionaries and deferred country-set union.
Whole-engine owner streams, private reconciliation and compact fingerprint
radix reduction were therefore implemented here rather than rejected from
those older results. Structural64 and fused-quote experiments existed; the
new block scanner was independently built before comparing its results, and
the selected text-pair scanner is a different decomposition again.

No private research ZIP was present in this repository or the attached upload
folders. The archived research summary and all fetched development branches
were available and inspected; this report does not claim a review of missing
archive contents.

## Public implementations and research

All three requested repositories contain Track C code and MIT licenses:

| Repository | inspected HEAD | relevant mechanism |
|---|---|---|
| manusingh090/speed-demon-2026-entry | 47b3afd09d4a556bf299b5dc9c41efc0ef4a2323 | whole-file structural masks, one worker pool, country-major membership, 16-byte keys |
| pratyuxxhh/speed-demon | 9b0be333f405a2d6a07d5d9b25097abc90eb701d | exact 24-byte prefixes/full tails, published tags, dynamic chunks, bulk page population, final distinct reduction |
| rj-karan/speed | 8ff7a71f4545d5194c85721dbf35939d8e65ca9d | exact globally shared 64-byte slots and worker-local arithmetic |

The first explicitly substitutes a 63-bit hash for long-name tails and is not
an exact identity representation. That shortcut was not adopted. The second
exact implementation was built locally as an external control: ~141-143 ms
on two CPUs versus ~163-169 ms for our baseline in the first trials, with the
same full output hash. It was not treated as a validated submission or copied
into this solution. Its retained input mapping explains its ~209 MB RSS;
RSS is not the fresh cgroup memory charge.

Mechanisms were cross-checked against Geoff Langdale's simdcsv README and
its explanation of quote parity, including doubled quotes, and the referenced
2019 carry-less prefix-XOR article. Prefix parity was already an independent
hypothesis in Pass I. No external library or substantial competitor source
was copied into the submitted implementation; D remains 1. Ideas about
partition ownership and radix reduction were implemented specifically for
this relation and its exact output semantics.

References:
- https://github.com/geofflangdale/simdcsv/blob/master/README.md
- https://branchfree.org/2019/03/06/code-fragment-finding-quote-pairs-with-carry-less-multiply-pclmulqdq/
- https://www.microsoft.com/en-us/research/publication/speculative-distributed-csv-data-parsing-for-big-data-analytics/

## Measured cost model

Host: Xeon Platinum 8573C VM, three visible vCPUs but cpu.max=200000/100000:
**two CPUs of quota**. Clean comparisons pin to CPUs 0-1, avoiding bursts on
three CPUs followed by quota sleeps. This differs from six physical P-cores /
twelve hardware threads on the scoring box. Trials with 4/6/12 workers here
are oversubscribed and do not establish official scaling.

Untrained baseline phase profile, median of three, six software workers:
lookup ~2.6 ms; mapping/partition discovery ~0.48 ms; fused processing
~172.9 ms; group reduction ~0.023 ms; output ~0.30 ms. The timing ends before
some destructors; complete-process comparisons below use exec-to-exit.
The fused phase accounts for roughly 98% of this profile's wall time.

One worker: ~276-278 ms full process and ~276-282 ms CPU. Two workers:
~152-170 ms wall and ~289-301 ms CPU. Six workers on the same quota increase
CPU work through scheduling/coherence; official scaling cannot be inferred.

PGO ablations retain parsing, join and arithmetic but intentionally do not
produce a valid distinct column. They are cost measurements, never candidates:
removing dictionary/seen handling reduces CPU from roughly 305-315 ms to
~208 ms; additionally removing name hashing reduces it to ~198 ms. Thus
integrated dictionary/distinct work is around one third of CPU, hashing a
few percent, and parse/join/arithmetic the remainder. These differences include
allocation/cache interactions and are not an additive per-instruction profile.

Full baseline has ~5,250-5,600 minor faults and zero major faults on warm
inputs. Private dictionaries incur ~16,500-25,000 minor faults; radix ~14,400.
Deferred radix equality revisits the file, bringing RSS to ~226-228 MB while
fresh-container peak remains ~35 MB. Baseline peak is ~10 MB, not its ~14 MB
RSS. Cgroup page-cache attribution is why RSS cannot be used on the M axis.
Hardware PMU cycles via perf_event_open were unavailable in this VM. Sampled
RDTSC measurements were contaminated by descheduling and instrumentation;
they were not used as quantitative phase percentages.

## Serious implementations and recoveries

1. Private dictionaries: exact copied keys and private four-word memberships;
   union only local distinct names after parsing. A second version reserves
   storage and reconciles disjoint hash owners in parallel, reducing allocation
   faults and the serial merge. Replicated dictionaries still dominate M.
2. Bounded owner routing: SPSC rings route 24-byte name events to dictionary
   owners; no record tape and no concurrent dictionary writes. An inline-key
   repair removes extra name-pointer dereferences and uses two parsers / one
   owner for this host. Failure signaling prevents producer deadlock. It saves
   memory but adds routing, polling and a consumer critical path.
3. Radix relation: 8-byte (24-bit fingerprint + country, input offset) rows,
   in-place MSD radix partitioning, exact comparison within repeated-key runs.
   Singleton keys need no second name access. The first version sorts worker
   runs then merges; the repair gives each owner small cache-resident radix
   buckets, eliminating the per-record multiway merge. Hash collisions are
   resolved by full exact bytes, never accepted as identity. Files above 4 GiB
   use the original exact engine, avoiding a truncated-offset assumption.
4. Whole-range structural scanner: quote/comma/LF masks, carry-less parity,
   quote-count length decoding, integer conversion from known field lengths.
   Fixed-size field/record limits are absent; long fields stream over blocks.
   An end sentinel handles an unterminated last record. Repair removes the
   heavy iterator and counts quotes only at text boundaries.
5. Country-major membership: exact 16-byte name slots, separate country rows
   of bits, no-return atomic OR, final popcounts. A wider index keeps a low
   probe load while bounding each dense pool independently. It trades inline
   bitmap locality for a second random memory stream and loses here.
6. Flat lock-free publication: one table, exact keys, busy/published control
   words and exact overflow. Measured separately with post-reduction; it does
   not beat the shared sharded incumbent on this host.
7. Fused text-pair scanner: only the middle two columns use quote/comma masks.
   Integer fields retain the incumbent numeric parser. Name hashing/prefetch
   starts at the first text delimiter while the same mask resolves the note.
   This removes repeated escape-pair search without imposing five-column
   structural bookkeeping on every record.

The hybrid follow-ups investigate larger inline exact keys and bulk read-PTE
population after the independent families above, not instead of that research.
Results and final candidate choice are recorded below after qualification.

## Interpretation and remaining work

No result here establishes that the incumbent architecture is close to optimal.
The measured failures apply to these complete representations and schedules.
They do not exhaust sorting, ownership or private computation.

High-upside directions still open: compact immutable exact-key publication
with country-separated membership at twelve hardware threads; owner pipelines
with a genuinely latency-hiding table probe stage; radix partitioning before
field projection rather than after materializing descriptors; SIMD processing
of batches of key descriptors; compressed local membership sets with selective
reconciliation; and a two-pass immutable dictionary with exact miss fallback.
None is advertised as a measured win or dismissed from a naive estimate.

## First complete-engine comparison

Five interleaved full seed-1 runs per image, CPUs 0-1; middle-three mean T,
highest fresh-container kernel peak M, canonical complete S, D=1.

| engine | T ms | M bytes | S | relative complete K |
|---|---:|---:|---:|---:|
| baseline | 171.440 | 10,354,688 | 644 | 1.0000 |
| block | 170.098 | 10,199,040 | 631 | 1.0101 |
| owner | 223.331 | 9,412,608 | 673 | 0.8872 |
| pair | 164.022 | 10,362,880 | 648 | 1.0213 |
| pair12 | 176.513 | 10,620,928 | 650 | 0.9792 |
| private | 236.528 | 56,332,288 | 571 | 0.6178 |
| radix | 261.032 | 34,844,672 | 805 | 0.6149 |

These are local relative K values, not official scores. All output hashes
matched and all writes stayed within the published sandbox mounts. The timer
reads the fresh container cgroup peak after child exit and emits trusted local
measurement JSON on stdout. Official scoring reads its enclosing parent cgroup
and uses a protected timing file; this harness is a development comparison.

## Hybrid comparison against immutable official source

Five interleaved full seed-1 runs per image, CPUs 0-1; middle-three mean T,
highest fresh-container kernel peak M, canonical complete S, D=1.

| engine | T ms | M bytes | S | relative complete K |
|---|---:|---:|---:|---:|
| baseline | 174.126 | 10,469,376 | 644 | 0.9854 |
| inline32 | 156.334 | 10,878,976 | 657 | 1.0289 |
| official | 171.232 | 10,227,712 | 637 | 1.0000 |
| pair | 177.268 | 10,227,712 | 648 | 0.9803 |
| populate | 158.274 | 10,739,712 | 666 | 1.0232 |

These are local relative K values, not official scores. All output hashes
matched and all writes stayed within the published sandbox mounts. The timer
reads the fresh container cgroup peak after child exit and emits trusted local
measurement JSON on stdout. Official scoring reads its enclosing parent cgroup
and uses a protected timing file; this harness is a development comparison.

## Public checkpoint, ongoing campaign

PR #14 is an optional hybrid, not an architectural breakthrough or an official
promotion. CI 41274929908 passed all correctness, sanitizer and Docker gates.
Its direct full-PGO comparison was 78.185 -> 77.986 ms; cgroup memory median
10,502,144 -> 11,022,336 B. After canonical source penalties that runner does
not demonstrate a complete-K win. This disagrees with the two-CPU VM's gain
and makes scoring-box validation necessary before promotion.

The selected hybrid also passes exact full seeds 11, 29 and previously unused
137 in production Docker, five runs each against fresh Python references;
64 independent hostile CSV fuzz cases pass ASan/UBSan. Full receipts are saved.

Two additional complete solvers are actively under investigation:
- Two-pass repeat filter: a shared exact-conservative fingerprint bitmap marks
  possible repeated country/name pairs; the second pass performs exact
  interning only for possible repeats. No fingerprint is accepted as identity.
  Full seed 1 is exact; first prototype 306.17 ms, roughly 574 ms CPU, versus
  baseline 165.32 ms / 310 ms CPU. It is not rejected as a family: private
  sketches with batched reconciliation and a projected structural replay are
  the next concrete repair.
- Owner repair: futex sleeping replaces yield polling, lookups have a separate
  slot-prefetch stage, and input remains mapped until consumers finish. CPU
  falls from about 483 ms to 356 ms, system CPU 135 -> 53 ms, but wall remains
  ~244 ms. Stage imbalance and owner throughput require direct profiling;
  the initial one-owner schedule is not a conclusion about owner partitioning.

These new prototypes are experimental, not fully qualified candidates. The
research branch is a checkpoint; subsequent results will extend it.

## Repeat-filter completion and further solver work

The repeat filter has now been implemented with worker-private first-pass
sketches and batched reconciliation. Each worker records first bits privately
and logs repeated indices; reconciliation ORs local duplicate logs and
intersections of the per-worker seen sets, then frees those sets before replay.
A repeated exact country/name key cannot be lost: it either repeats in one
worker or appears in at least two worker sets. Fingerprint collisions only
add replay candidates; full bytes still determine identity in replay.

A second complete replay implementation uses whole-block structural masks
to project only name and region, omitting arithmetic and note decoding. Both
versions match full seed 1. The projected version passed the complete hostile
suite under ASan/UBSan and TSan plus independent tiny/sample cases.

Seven interleaved local PGO trials: baseline 166.08 ms; shared repeat filter
299.21 ms; private filter 263.84 ms; projected replay 266.64 ms. CPU falls
from roughly 566 to 488/506 ms through the private repair, but this remains
behind baseline. Phase profiling and complete Docker M/S measurements are
in progress; the family is not being dismissed from this first repair.

A new epoch-partition solver is also fully implemented. It partitions compact
exact-verifiable name descriptors into bounded batches, then reconciles them
through cache-sized worker-owned dictionaries. No dictionary or membership
update is shared. Persistent per-bucket sets preserve identity across epochs;
wide descriptors handle long names and offsets without truncation. Its first
full output matches; phase profiling is underway before further repair.


## 06:20 UTC checkpoint: complete repeat filter and serious redistribution repairs

All measurements below are development-machine results on two pinned CPUs
under a two-CPU quota, not official scoring-machine claims. Sources are in
`prototypes/`, including diagnostic phase builds; raw receipts are in
`receipts/`. Each new engine retains exact full-byte collision verification.

### Completed two-pass repeat filter

The private filter passed the existing hostile suite under ASan/UBSan and
TSan, 64 independently generated differential cases, and the unchanged
production Docker build with no-network training. Five paired, interleaved
container runs per independent full seed produced these trimmed means and
maximum fresh-container cgroup peaks:

| Full seed | Baseline T ms / M bytes | Private filter T ms / M bytes | Relative complete K |
| --- | --- | --- | --- |
| 11 | 169.678 / 10,362,880 | 281.195 / 13,340,672 | 0.7210 |
| 29 | 184.625 / 10,293,248 | 283.558 / 12,857,344 | 0.7535 |
| 137 | 172.082 / 10,285,056 | 281.002 / 12,845,056 | 0.7307 |

All 30 executions matched their independent reference SHA256. S is 644 for
baseline and 756 for private filter; D=1 for both. Relative K uses the
complete T/M/S/D exponents, not throughput alone. The filter is complete,
correct on the tested cases, and not promoted. Its artifact SHA256 is
`d0d538eed5790ba53e8bf64ae8ea1baaaeb7a7056d882b15d93032992b2aea3c`.

Thread-clock profiling explains the loss. Shared first-pass sketches cost
about 299–328 ms CPU; private sketches reduce this to 218–240 ms. Replay
still costs 285–298 ms CPU with 1,415,459 exact candidates, about 42.6% of
records. Whole-block projected replay costs about 290–301 ms CPU, despite
omitting arithmetic and note decoding. This rejects these implementations,
not every possible two-pass strategy. A compact projection tape might avoid
reparse, but its allocation/write/read and peak-memory penalties remain to
be measured; it has not been implemented or claimed as a result.

### Persistent, exclusively owned dictionary reconciliation

The epoch solver parses compact 8-byte descriptors into hash buckets and
reconciles each bucket through one persistent private exact dictionary.
There are no shared dictionary or membership updates. It handles large raw
names and offsets through a wide-descriptor path. Original phase CPU is
about 194–196 ms parsing and 104–108 ms reconciliation. Whole-epoch barriers,
repeated thread creation, allocator fragmentation and descriptor capacity
were avoidable costs, rather than reasons to reject the architecture.

Repairs pool long-name storage per owner, reduce global epochs from 48 to
16 MiB, and retain a persistent pthread-barrier worker team. Eight interleaved
PGO runs give median milliseconds:

| Engine | T ms |
| --- | --- |
| Preserved baseline | 167.94 |
| PR14 shared-dictionary hybrid | 157.48 |
| Original epoch private solver | 188.21 |
| Pooled arenas, original epoch size | 179.25 |
| Compact epochs, recreated workers | 192.14 |
| Compact epochs, persistent workers | 170.66 |
| Deferred numeric columns, recreated workers | 199.54 |
| Deferred numeric columns, persistent workers | 172.04 |

The persistent compact solver passed the complete hostile ASan/UBSan and
TSan suites. Added independent reconciliation cases verify a name repeated
only across workers, one repeated only across epochs, quoted/unquoted empty
identity, negative-only groups, and a 128 MiB field crossing the compact
24-bit offset limit. All matched the unchanged Python oracle. Its production
Docker measurement is still pending. Short-lived-thread rusage CPU totals
under-counted work in earlier diagnostics; thread-clock phase sums, rather
than those rusage totals, support the model.

A further bounded tile pipeline is implemented: three producers partition
2 MiB tiles into compact streams, while three consumers own dictionary
buckets. Two tiles per producer bound buffering; input reclamation waits
for every consumer. This overlaps the two computational stages and removes
per-epoch global barriers. Qualification and benchmarking are pending.

### Name-major exact radix reduction and sequential scatter

The existing radix solver sorts country/name fingerprints and verifies
repeated runs. The new `radix-name.cpp` instead groups name fingerprints and
raw lengths, verifies each complete name exactly, then reduces its country
membership through a temporary 256-bit set. Hash collisions create multiple
exact names within a run; no fingerprint is treated as identity.

Seven interleaved native PGO trials measured baseline 165.77 ms, PR14 156.16,
country/name radix 263.61, name-major radix 253.70. Thread-clock profiles show
country/name radix: parsing ~214–229 ms CPU, initial cyclic partition ~55–58,
remaining sort ~138–143, exact reduction ~56–58. Name-major grouping reduces
remaining sort to ~66–67 ms but increases exact reduction to ~135 ms because
all occurrences now require verification. This is a measured tradeoff, not a
claim that sorting is fundamentally uncompetitive.

Both engines now also have complete stream variants. Parsing writes directly
to 256 bounded-frontier bucket vectors, eliminating the initial cyclic
partition; bucket-local radix passes use counting and sequential scatter
instead of in-place permutation cycles. Their end-to-end evaluation is
pending. The vectors retain the full descriptor relation, so a memory
penalty is expected and must be included in complete K.

PR14 was merged into sd-tc by external integration during this campaign.
Its four-vCPU CI full-PGO comparison was almost flat (78.185 -> 77.986 ms)
and memory increased, so the local speedup does not establish an official
complete-K win. This research branch changes no submission source. Luna
alone decides official integration and scoring-box validation.


### Completed next comparison

Eight new interleaved native PGO trials: baseline 161.14 ms; PR14 153.89;
country/name radix stream 165.99; name-major radix stream 203.51;
persistent private epochs 163.40; tile pipeline 168.77. Sequential scatter
therefore repairs much of the country/name radix loss (previously ~264 ms).
The new name-major stream passes the hostile ASan/UBSan suite. The tile
pipeline passes both ASan/UBSan and TSan suites. These are substantial repairs,
but none has yet demonstrated a complete-K win.

Production Docker, five paired interleaved runs, full seed 1:

| Engine | Trimmed T ms | Maximum cgroup M bytes | S | Relative K |
| --- | --- | --- | --- | --- |
| Preserved baseline | 168.249 | 10,260,480 | 644 | 1 |
| PR14 | 161.395 | 10,805,248 | 657 | 1.007 |
| Persistent private epochs | 169.923 | 14,221,312 | 725 | 0.916 |

D=1 throughout; all hashes match. The private engine's arena/buffering repair
reduces peak from the original ~20.4 MB to ~14.2 MB. Its source/peak penalties
still outweigh its comparable runtime on this two-CPU host. A separate
four-vCPU CI qualification is useful before deciding its scaling limit.


## 06:45 UTC checkpoint: pure bounded sort-reduce, pipelines and CI confrontation

The persistent private engine is preserved as draft PR16, commit
07cf45ba991f001790053671410ca3d4b6f63962. CI run 41280672480 passed all gates,
including full seeds 11/29, ASan/UBSan, TSan, pinned offline Docker and sandbox.
Four-vCPU direct full-PGO fused control 65.212 -> 112.747 ms, median fresh
cgroup peak 11,042,816 -> 14,295,040 B, S657 -> 725, D1: relative K 0.712.
Thus comparable two-CPU runtime did not predict four-vCPU scaling. Do not
promote this implementation. Profile worker counts 4/6/12 gave untrained
97.897/103.596/94.347 ms. Official-machine scaling remains unmeasured.

The bounded pipeline repair is separately preserved as draft PR17, commit
9603aa6ab7a1d92b6d722471dea497f4cb981d95. Four parsers and two consumers
balance the measured 2:1 stage CPU ratio; 24-byte inline keys reduce pointer
lookups. Local eight-trial native PGO medians: b31 162.336 ms, fused154.347,
original 3+3 pipeline169.114, 4+2 pipeline165.884, 4+2/inline24 pipeline161.933.
CI 41281228501 passed every gate but direct fused full-PGO76.962 ->103.837 ms,
median cgroup11,046,912 ->14,960,640 B. Counts in its profile denote parsers
plus the explicitly listed consumers. Final serial distinct construction
costs about6 ms there; this is a repair target, not a fundamental lower bound.
No source from either draft replaced main.

### Full-tape radix and bounded pipeline complete-process memory

Five interleaved production containers, full seed1:

| Engine | Trimmed T ms | Maximum M bytes | S |
| --- | --- | --- | --- |
| b31 baseline |171.652|10,309,632|644|
| fused scanner |155.902|10,940,416|657|
| sequential-scatter country/name radix |183.886|36,679,680|788|
| bounded original tile pipeline |176.794|13,348,864|756|

All D=1 and exact output hashes. The tape's allocation/capacity penalty
outweighs its recovered sorting throughput. Separate production container
checks on full seeds11/29/137 passed for radix stream, tile stream and private
epoch engines. Their one-run receipts are correctness evidence, not paired
performance claims.

### Pure exact sorted catalogs: a genuinely different complete solver

`epoch-sort.cpp` replaces every name hash table with radix-sorted batches and
persistent sorted exact-name catalogs. Within a bucket, merge scans the old
catalog once, verifies all candidate names by full bytes, and combines country
sets. New storage is compacted per bucket before proceeding, bounding the
peak old/new coexistence. Wide descriptors and exact hash-collision chains
retain the same correctness guarantees as the private epoch parser.

Original phase CPU: collect26–27 ms, sort38, exact catalog merge107–111.
The serious repair sorts 8-byte references rather than 24-byte fields,
prefetches input ahead, and inlines24 canonical bytes. Sorting falls to21–23
ms and merging91–100 ms. Eight interleaved PGO medians: b31163.001 ms,
fused154.694, pure sort204.953, repaired sort189.404. The repaired engine
passes the complete hostile ASan/UBSan suite plus tiny19/sample37. Production
Docker memory/source qualification is still underway. This quantifies the
remaining sorting/merge overhead without rejecting cache-efficient sort-reduce
as a family.

### Additional complete mechanisms

`membership-pipeline.cpp` resolves exact names into compact IDs, then routes
four-byte(country,ID) records to exclusive country owners with ordinary
membership bitmaps. It removes shared membership atomics but retains shared
canonical key publication. It passes hostile ASan/UBSan and TSan suites.
Eight native PGO medians:170.240 ms versus b31162.336/fused154.347. Queue and
stage costs still offset the saved atomic work; no promotion.

`cursor.cpp` replaces the structural parser's field-state machine with five
unrolled cursor pulls per record. It gives157.283 ms versus b31163.001 and
fused154.694, and passes hostile ASan/UBSan. Explicit PCLMUL inlining gives
161.591 ms and four-at-a-time numeric columns159.452: these repairs do not
beat the fused control. One earlier cursor benchmark overlapped a sanitizer
process and was discarded, not used in the comparisons above.

`borrowed.cpp` eliminates canonical string copying by retaining the mapped
input. Up to8 exact bytes live inline; longer names reference immutable CSV
bytes, with full length metadata for long-key tag collisions. Published index
length tags establish exact lengths through32 bytes. No record pages are
reclaimed while canonical references remain live. The full hostile ASan/UBSan
and TSan suites pass. Initial native PGO:171.927 ms versus b31163.298 and
fused162.252. Its potential advantage is lower allocation and peak private
memory, not an established runtime win; production cgroup measurement is
pending. The unchanged official harness explicitly prewarms data before the
run cgroup exists, so RSS and cgroup M must be reported separately.

The same input-backed representation has also been implemented in exclusively
owned private dictionaries (`epoch-borrowed*`, `tile-borrowed`). These sources
are preserved before results; qualification/benchmarking is pending. They
retain full exact lengths/hashes in each private slot and do not depend on a
shared dictionary.

## Completed BCD and deferred decimal columns

The monetary extrema experiment is now complete, not silently shelved.
`linear.cpp` accumulates right-aligned decimal columns in 16-bit lanes with
bounded2048-record flushes; monetary signs are applied to the lanes and
weights only at flush. A signed monotonic BCD key supplies extrema. Wider
values flush the group and permanently use exact scalar arithmetic, including
conversion of existing extrema. ID columns are unsigned; signed money column
magnitudes remain within16-bit bounds at the chosen flush interval.

`linear-byte-key.cpp` replaces PEXT extrema with a spaced-byte big-endian key.
Both pass the full hostile ASan/UBSan and TSan suites. Original BCD production
Docker passes full1/11/29/137 exactly; full1 has five interleaved timing pairs.
The byte-key repair has five interleaved pairs on every seed. All builds use
the unchanged pinned offline PGO Dockerfile, training37, D1. Canonical S752.
M below is the maximum fresh-container cgroup peak, not RSS.

| Prototype / full seed | Champion T ms | Candidate T ms | Champion M | Candidate M | Relative complete K |
|---|---:|---:|---:|---:|---:|
| PEXT BCD /1 |155.160|157.622|10,883,072|11,018,240|0.9699|
| byte-key /1 |155.108|161.852|10,825,728|10,969,088|0.9568|
| byte-key /11 |160.634|158.655|10,825,728|10,997,760|0.9829|
| byte-key /29 |159.187|161.071|10,985,472|10,817,536|0.9772|
| byte-key /137 |161.038|165.422|10,813,440|10,919,936|0.9650|

Runtime is the trimmed middle3 of5. These are two-quota-CPU development
measurements, not official scores. Original BCD holdouts have correctness
receipts only, not paired performance estimates. `linear-packed.cpp` tries
carry-free packed byte sums with8-record flushes; `linear-id.cpp` retains only
ID columns. Both pass hostile ASan/UBSan but lose in native PGO comparisons.
The added lanes/flushes/key work do not remove enough critical-path work to
pay for their source and runtime cost. No member of this tested set should
replace the champion. This does not establish a lower bound for all fused
numeric pipelines.

## Public amplification and new exact engines

See PUBLIC-AUDIT.md for immutable source identities, verified board
associations, and mechanism-by-mechanism comparison. Manu is the primary
public performance reference (K38.755,28.594ms); Rj is secondary (K27.281,
51.316ms). The K43–47 leaderboard leaders are not attributed to either.

`compact-exact.cpp` replaces copied32-byte keys and name-major seen words
with a compact immutable fingerprint locator, canonical input references,
country-major bitsets and parallel final reduction. Equality remains exact:
full byte verification, or an injective ASCII<=8 encoding. It separates
region/numeric aggregation, canonical verification and membership updates in
three bounded16-record rings. Atomic key loads are individually acquire;
SIMD compares register copies, never plain-loads atomic storage. Release
publication orders canonical pointer reads. Exact overflow maps handle
capacity and pathological collisions. Work-based gates and dynamic morsels
support late workers, parallel prefaulting and final page reclamation.
Canonical references remain mapped until every query completes.

`compact-fields.cpp` adds independent32-byte field masks with reused escape
counts, delimiter SWAR, known-width numeric decoding and input prefetch.
`compact-verify.cpp` and `compact-fields-verify.cpp` remove the short exact
encoding and always verify complete bytes, isolating representation costs.
All primary variants' full hostile sanitizer receipts are preserved; the
field/full-verification solver passes ASan/UBSan and TSan, including deliberate
CRC collisions, very long names and capacity overflow. Native comparison
includes affinity-only champion and fixed6-worker challenger controls. These
are challengers under evaluation, not qualified promotions.

`tail-vector.cpp` is an independent parser decomposition: scan record endings
and total quote counts, locate numeric fields from the tail, decode four
records together, parse only names, and derive note bytes from quote-count
arithmetic. It keeps only32 stack records and avoids a second note traversal.
Its complete hostile ASan/UBSan and TSan suites pass; native PGO initially
improves roughly7%, but canonical S860 and production cgroup measurements
must be included before judging K.

### Repaired private streams and ownership diagnosis

`private-stream.cpp` uses sequential8-byte parser packets and exclusive
consumer dictionaries; full pointers/lengths use a wide escape. Consumers
radix-partition each bounded packet before exact dictionary lookup. No global
name dictionary or shared membership updates exist. It passes full hostile
ASan/UBSan and TSan. CPU profiling separates parser185–187ms and owner143–146ms,
with native wall181ms versus fused150ms. `private-radix-stream.cpp` seriously
repairs the owner side with direct radix heads, compact collision chains,
16-bit heads promoted exactly on growth, and contiguous owner hash ranges.
It is preserved before qualification; do not promote it from a proposal.

Country-owned membership streams were also repaired using grouped country
packets, cacheline-sized staging and borrowed/SIMD canonical keys. Measured
phase CPU (`pipeline-cpu-phases.json`) is parser290–297ms versus owner17–20ms;
grouping gives284–292 versus16–17. Borrowed grouping worsens producer CPU to
321–327ms. The consumer bitmap is not the dominant cost. Queue emission and
exact key producer work must change before another consumer-only tuning pass.
Sources, phase instrumentation and sanitizer receipts are preserved.

### Completed earlier pending production measurements

The pure exact sort-catalog repair gives Docker194.809ms, M15,577,088, S738
versus fused156.841ms; its original gives207.092ms/M14,729,216/S707. Sorting
reference descriptors reduced sort CPU38ms to21–23ms, but exact catalog merge
still costs91–100ms. This is a measured remaining bottleneck, not a dismissal
of all sort/reduce hybrids. Input-backed flat dictionary Docker relative K
across1/11/29/137 is1.0046/0.9749/1.0116/0.9125: seed sensitivity prevents
promotion. Its probe instrumentation and optimistic/SoA repairs are preserved.
