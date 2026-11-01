# Valid-CSV delimiter path

Based on qualified main da7fe7b/PR13, runtimec4b41459.
Frozen sections2.1/2.2 promise well-formed records. Delimiters, nonempty integers,
closing text quotes and exact two-digit fractions are invariants at these call
sites. Keep debug assertions, consume valid separators directly in release,
and preserve optional final EOF, LF/CRLF, BOM, all quote decoding and full-width
numeric fallback. This does not skip name comparison or assume any visible seed.
Malformed-input rejection outside the promised format is not a release contract.

Equivalence on valid input: number consumes at least one digit; comma follows
id/text/region; text columns1/2 always have following fields, so a found quote has
a following byte; cents has a dot plus two fraction bytes; a footer is LF, CRLF
or final EOF. The same bytes/pointer advances remain; only failure checks change.
Boundary ownership and exact collision handling are unchanged.

Local pinned GCC15 PGO randomized12-trial restricted2CPU containers:
Frame106.682ms/238.778CPUms/10,153,984B;
candidate105.738ms/217.303CPUms/10,170,368B. All full1 hashes exact.
CPU improved9.0%, wallonly0.9% under quota-sensitive scheduling; no official claim.
Actual pinned release, independent full references and sanitizers pending CI.
Rollback is current main/PR13 complete PGO context. No PR before results.
