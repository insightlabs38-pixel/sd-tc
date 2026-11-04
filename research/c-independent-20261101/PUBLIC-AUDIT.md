# Public architecture audit, 2026-11-01

Actual Track C sources were inspected. Public availability is not evidence of
leaderboard rank or general correctness. No substantial external source was
copied; challenger implementations are independent. All four repositories
publish MIT licenses. The third and fourth references were inspected once,
not treated as demonstrated superior engines.

## Verified associations

Snapshot: `receipts/board-public-audit.json`, generated 08:26:13 UTC.
Association uses the board watch HEAD matching the repository HEAD, then
retrieves the immutable BEST source and compares its SHA256 with current C.
Default branches are `speed-demon`, not the supplied `main` URLs.

| Repository | Verified team | BEST K | T ms | M bytes | S |
|---|---|---:|---:|---:|---:|
| [manusingh090](https://github.com/manusingh090/speed-demon-2026-entry/tree/speed-demon/track-c) | Cache Me | 38.755098 | 28.594 | 9,011,200 | 401 |
| [rj-karan](https://github.com/rj-karan/speed/tree/speed-demon/track-c) | BeyondBug | 27.280554 | 51.316 | 12,656,640 | 377 |
| HighVelocity official PR14 source | HighVelocity | 25.979773 | 50.141 | 11,288,576 | 657 |

Manu BEST source commit `1a2e9eddcc679e2cec50c376b13f398638294bfe`,
SHA256 `f5e7c90f2f205df10eba379a21a8249653e1c8b69e4b1c2faef0b6c8daa9d900`.
Repository HEAD `c1ec48304e3f2de5eafd73312caecd53cb494455` has identical C.
Rj BEST `a3babdab6e6e9d5445903d1875279ca0af5b282d`,
SHA256 `be00d2383f2c64a7e6cf574f30252e522de7101e1a1bf9fb364982b0ec3aca3f`;
HEAD `8ff7a71f4545d5194c85721dbf35939d8e65ca9d` has identical C.

The K43–47 leaders' source associations remain unverified. Their 18–20 ms
results must not be attributed to these public repositories. Rj's source
advantage explains its higher K despite slightly slower runtime than ours.

## Mechanism comparison

| Mechanism | Actual public implementation | Our previous state | New confrontation |
|---|---|---|---|
| Structural masks | Manu: field-local 32B quote masks, reused escape counts; not a 64B whole-CSV PCLMUL scanner. Rj: AVX quote searches. | Fused name/note 64B masks with quote parity. | `compact-fields*`: independent field masks, SWAR delimiters and known-width digits. `tail-vector`: 64B record scanner plus four numeric records, no second note traversal. |
| Segmentation | Generator-valid LF followed by ID, or CRLF boundaries; no full quote prepass. | Already generator-valid segmentation. | Dynamic 8*T morsels with work-completion gates; fallback and BOM retained. |
| Region join | Byte-indexed direct region table. | Already byte-indexed. | Prefetch region earlier in pipeline; no new join assumption. |
| Numeric state | Worker-local statistics. | Already worker-local. | SIMD columns/BCD qualified but no win. Four-record numeric decode in tail parser remains promising. |
| Name identity | Manu: 64-bit probabilistic identity; Rj: exact prefix plus full tail. | Exact inline32 keys, CRC locator and fallback. | `compact-fields-verify`: CRC+full length locates canonical input bytes; every equality verifies full bytes. `compact-fields`: also has injective ASCII<=8 encoding, never a probabilistic equality shortcut. |
| Atomic publication | Manu vector-loads atomic key storage. | Atomic-safe tags and immutable exact slots. | Four acquire scalar atomic loads assembled into SIMD registers; canonical bytes published by release and read after acquire. No vector load across atomic storage. |
| Membership | Country-major bitsets, delayed reduction, pipeline prefetch. | Name-major seen words, fresh count per record. | Separate country-major bits and final popcount; private-owner 4B ID streams also implemented. Their producer lookup/emission costs dominate, not consumer bitmap CPU. |
| Canonical storage | Manu avoids canonical bytes; Rj retains input tail references. | Stable copied inline/long strings. | Input-backed exact canonical pointers, no input reclamation until every query has completed. Prior borrowed/flat designs measured and preserved. |
| Pipelining | Manu: two 16-record rings separating memory stages; prefetch input 2048B ahead. | Partial prefetch and fused immediate updates. | Three bounded 16-record rings separate numeric join, canonical lookup/verification and membership. No full record tape. |
| Scheduling/pages | Manu: up to12 hardware threads, work phases, parallel page reclamation. | Fixed6 workers, mostly serial final cleanup. | Affinity-clamped12 with dynamic tasks and parallel prefault/count/drop. Fixed6 and affinity-only controls separate scheduling from architecture. Official 6P/12HT scaling still requires CI/scoring evidence. |
| State/source | Manu compact 8B locator avoids exactness costs. | ~10–11MB anonymous state. | New locator1MB +canonical1MB +membership4MB, exact overflow map. Source/memory penalties must be measured; none promoted from native timings. |

## Other references, bounded audit

Pratyux HEAD `423e5bf5e78f5ff7d72202157e8e79a7e8add8b9`: whole64B
PCLMUL structure masks, exact prefix24/tail metadata, delayed32 prefetch,
country bitsets, affinity-based workers and 1MiB tasks. No demonstrated
frontier association. Its narrow numeric and EOF paths and concurrent growth
flag need repair for our adversarial requirements. Earlier local comparisons
with this code used different thread counts and cannot attribute the result
to an algorithm.

Corey HEAD `2e7b09115d4795c2595ff2b1acdf7cf755938713`: field searches,
16-record ring, CRC state, short inline key and long arena, static12 CRLF
slices. Packed length and fixed arena/name capacity need exact wide/growth
fallbacks. No demonstrated frontier association. Neither reference substitutes
for a qualified engine.

## Remaining independent hypotheses

The tail parser reframes note decoding as quote-count arithmetic: locate final
numeric columns from the record tail, decode four records, then visit only
name bytes. Numeric work and exact identity can follow distinct pipelines
without a large tape. Combine this with the best exact compact registry only
after isolating their costs. Bounded private radix streams remain a separate
ownership architecture: current CPU measurements direct repairs at owner
lookup/descriptor processing, rather than repeating larger hash tables or
assuming sorting as a family cannot win.
