# Twelve-worker scoring challenger

Only runtime difference from qualified PR12 blob8bee2ee3a2cb027098f5b95a4279bfb9a30da785:
the large-file worker count is12 instead of6. This is a scoring-host SMT trial,
not a proven performance improvement on a quota2CPU VM or4-vCPU CI runner.
Six-worker control remains PR12; official BEST source59e47be remains rollback.
The regular qualification workflow covers exactness, sanitizer and final PGO
holdouts for this actual12-worker binary. Copy full PGO Docker context.
No separate PR until complete-K performance evidence justifies promotion.
