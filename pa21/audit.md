# PA21 checkpoint audit

## Scope Reviewed

Reviewed the latest PA21 checkpoint scope in `pa21/plan.md`, the PA21
assignment contract in `pa21/README.md`, the testing/reference rules, the
recent PA20 audit and PA21 implementation commits (`feaf491` and `d866a74`),
the complete current-PA report, and every changed PA18 template source file.
The audit covers the Group A partial-specialization identity, matching,
ordering, current-owner materialization, nested-pack replay, generated
declaration ordering, and the source-shape changes needed to keep those paths
maintainable.

## Findings

The checkpoint is architecturally sound and has no checkpoint-level blocker.
Partial-specialization matching is structural: typed patterns, alpha-renamed
parameters, pack bindings, cv/ref/pointer shells, and non-type values are
retained in `TemplateDefinition` and the specialization maps.  Selection
compares the matching candidate set instead of accepting the first source or
test-specific name.  Generated specialization names are indexed by primary
template, so current-specialization and member lookup do not repeatedly walk
the entire specialization registry.

The former empty-pack timeout path is fixed by preserving active typed pack
bindings and resolving concrete nested owners before member lookup.  It is not
a timeout guard, fallback success path, dummy output, embedded payload,
interpreter/VM/trampoline substitute, or source/test-specific acceptance gate.
The compiler continues through preprocessing, parsing, semantic expansion, and
the ordinary PA14-PA20 LowIR lowering path; no reference binary, host compiler,
fixture, or emitted-text oracle is invoked.

Generated dependency and forward ordering are represented as compiler state and
resolved before LowIR lowering.  The implementation does not duplicate
ownership in a hidden fragment or make downstream code recover facts by
reparsing emitted output.  The audit refactor moved oversized bodies into
linked implementation units and split the large instantiation/member-lookup
operations at semantic boundaries; it did not weaken the file-audit checks or
move code into an unchecked path.

## Changes Made

- Kept the typed partial-specialization matcher, pairwise candidate ordering,
  active-pack propagation, concrete nested-owner lookup, and indexed generated
  specialization lookup from the checkpoint.
- Moved elaborated-spelling normalization, namespace occurrence collection,
  translation-unit generated-forward ordering, and template-member lookup
  helpers into owned `.cpp` implementations.
- Split `Instantiate` into argument preparation, cache replay, registration,
  materialization, and emission stages; added the new decltype implementation
  source to `dev/frontend_source_sets.mk`.
- Refreshed `pa21/plan.md` from the complete post-audit PA21 failure set and
  selected Group B as the next substantial checkpoint.

## Validation

- `make -C dev cppgm++`: passed.
- Exact through-PA20 gate: `1635/1635` tests passed.
- `perl scripts/cppgm_file_audit.pl --stage pa21 --paths dev/src`: passed
  with the repository's eight pre-existing body-division warnings and no fatal
  findings.
- Required full PA21 report: `62/215` passed, above the checkpoint baseline of
  `57/215`; the remaining 153 failures are 135 expected-success exit
  mismatches, three expected-failure exit mismatches, 14 relaxed-LowIR
  mismatches, and one invalid-LowIR result.  Timeout failures: zero.
- `git diff --check`: passed.
