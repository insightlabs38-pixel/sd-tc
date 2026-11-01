# Twelve-worker THP-disabled Frame challenger

Based on current main da7fe7b (PR13/runtimec4b41459), not old PR12.
Only runtime differences: twelve large-file workers and THP disabled before
mapping/name pages. S639/D1, complete PGO context unchanged.
This isolates a scoring-host6P/12T/THP policy opportunity; the local2CPU VM
cannot establish the performance result. No PR before measured complete K.
Current main/PR13 PGO context is the runtime rollback; official59974fd6 remains.
CI compares the actual Frame control, all hostile/sanitizer/package gates and
actual final12-worker PGO binary full seeds11/29 x3. Performance pending.
