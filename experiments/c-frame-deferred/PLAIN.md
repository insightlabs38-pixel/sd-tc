# No-PGO release challenger

Runtime source is byte-identical to qualifiedf0b0434/c4b41459 (PR13).
Only release compilation changes: same pinned GCC15/O3/v3/LTO/static, no PGO.
CI must compare each baseline's actual Docker/generator context; copying the
plain context onto a PGO champion would be an invalid comparison.
The actual shipped binary is explicitly extracted before final holdout checks.
Source S637->632,D1; final gates/performance pending. No new PR.
CI41255591511 same-source pair favored untrained78.309 vs PGO82.951ms;
local restricted containers conflicted: plain244.223CPUms vs PGO235.160CPUms.
Do not promote on that mixed evidence; qualify actual release choices directly.
Rollback is complete PGO contextf0b0434/e5d6fad, not only its source file.
