# Scoring-host page-size challenger

Only runtime difference from PR12 blob8bee2ee3a2cb027098f5b95a4279bfb9a30da785:
PR_SET_THP_DISABLE before input mapping or dictionary pages are touched.
Hypothesis: on the official THP=always host, avoiding2MB rounding of the sparse
12MB virtual key pool may reduce cgroup peak M. Local hostTHP=madvise cannot
settle this. No projected official M/K claim. Output/concurrency unchanged.
Keep PR12 and official BEST59e47be rollback. Actual variant gated by full CI.
No PR until measured complete K supports promotion; Luna owns official trial.
