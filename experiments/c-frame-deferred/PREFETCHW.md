# Explicit ownership-prefetch challenger

PR13 source with seen-word prefetch changed read->write, plus -mprfchw in the
pinned Docker compiler flags. -march=x86-64-v3 alone had compiled write requests
as prefetcht0; objdump now confirms prefetchw. No source target pragma.
Name equality/concurrency are unchanged. Tests/sanitizers compile the same feature.
Baselines use their own immutable Docker/generator contexts, not candidate flags.
Cgroup memory and final actual PGO holds remain required. No PR before evidence.

Local randomized12-pair restricted2CPU containers:
PR13 115.650ms/235.160CPUms/10,219,520B;
write+PRFCHW110.401ms/228.844CPUms/10,186,752B,S637/D1 unchanged.
Complete local K proxy+2.4%; official6P/12T transfer not established.
All full seed1 hashes exact. Actual release qualification/performance pending CI.
