# Exact byte table plus fused short paths

Experimental combination, not promoted. Qualified components: byte-control table
058574f, short-name/money pipeline90da546 (PR12), unchanged pinned PGO context.
Full byte comparisons and long pointers remain exact; atomic control accesses
are scalar byte operations. Country-major membership; three-stage lag16 ring;
THP off and keys/controls prefaulted. Source S711/D1 vs PR12 S622/D1.

Local pinned GCC15 PGO (seed37), random12-trial restricted2CPU containers:
comparison1 PR12124.004ms/10,160,128B ->byte121.542ms/9,041,920B;
comparison2 PR12140.138ms/10,100,736B ->byte124.754ms/9,078,784B;
comparison3 PR12132.204ms/10,113,024B ->byte125.711ms/9,146,368B.
Complete local K ratios including S: about+1.4%,+6.1%,+2.7% respectively.
All outputs exact. Significant local runtime variation; no official K claim.

Added CI cgroup memory comparison avoids treating mapped-file RSS as official M.
Full PGO comparisons include main, official BEST and qualified PR12 controls.
Default global counter/capacity protocol remains the previously qualified one.
Independent sharded/no-global-counter prototype is still exploratory: randomized
comparison3 yielded131.450ms/9,115,648B (64shards) and130.849/9,207,808B
(128shards), slower than global byte125.711ms. Do not select merely for fewer lines.
Compact-ring and prefill variants did not give stable complete gains; see receipts.

## Pinned Docker CI,9208a2b

https://github.com/insightlabs38-pixel/sd-tc/actions/runs/41254034309 passed
all correctness/sanitizer/sandbox gates and full11/29 x3.
FullPGO vs main127.478->125.691ms; vs official BEST source132.764->125.676ms.
Direct qualified PR12 control118.738->126.367ms, cgroup10,489,856->9,410,560B,
S622->711,D1. Complete runnerK ratio approximately0.971 (-2.9%); rejected
versus PR12 despite lower memory. No PR or optimistic promotion.
Local data had favored this combination; actual pinned CI overturns that.
Prefetch2/L2 and sharded forms remain experiments, not qualified replacements.

Board refreshed23:53 UTC: BEST run-df2932b2ba2d, official commitc0969507,
T73.738ms,M14,114,816B,S552,D1,K21.029207. Source blob remains
59974fd6328c2fabb1154ac4cc31bda1036bc6ca, same immutable59e47be rollback.
