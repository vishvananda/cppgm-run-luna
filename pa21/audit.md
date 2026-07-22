# PA21 checkpoint audit

## Scope Reviewed

Reviewed the latest Checkpoint Scope and failure map in pa21/plan.md, the
PA21 contract in pa21/README.md, TESTING_AND_REFERENCES.md, recent commits
8e50532, c4e148e, 07fc383, and a2afd2e, every source file changed by
the checkpoint, and the complete current-PA log at
/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log.
The checkpoint under review is the template-template/typed matching increment:
typed template entities and arguments, pack-aware replay, dependent integral
evaluation, generated specialization matching, and incomplete alias-class
deferral over the existing LowIR path.

## Findings

- The compiler still executes preprocessing, tokenization, parsing, semantic
  collection/rewrite, and ordinary LowIR lowering. There is no skipped phase,
  dummy output, embedded payload, reference/host-compiler invocation,
  interpreter/VM/trampoline substitute, source/test-specific acceptance gate,
  or fallback success path.
- GeneratedSpecializationName indexed the definition parameter list without
  proving that a flattened argument still had a corresponding definition
  parameter. Partial definitions with an unnamed primary pack could trigger
  out-of-bounds access and runaway replay. The access is now bounds-checked.
- The same unnamed primary pack was also entering active pack maps under an
  empty key. Text replay then interpreted that key as a wildcard for every
  ..., causing recursive argument growth and the two observed 10-second
  timeouts. Named-pack ownership is now enforced across argument resolution,
  member/decltype replay, nested-owner replay, constant evaluation, and text
  expansion; empty keys are never installed or expanded. This is an
  algorithmic termination fix, not a timeout workaround.
- Dependent variable-template detection used the string suffix heuristic
  "_v". It now scans parsed template-id ranges and asks the typed definition
  graph whether the resolved base is a variable template.
- FindFunctionDefinitions and ordinary-template-using lookup performed
  avoidable full registry walks. Both now use the collection-time
  definitions_by_name_ index. The checkpoint's pack-name and
  constant-member-owner indexes remain the semantic owners for their facts;
  replay does not recover them by reparsing emitted text or LowIR.
- The file audit caught a one-line crossing of the pa18_templates.cpp
  limit while adding the unnamed-parameter guard. The guard remains in
  checked source and the simple statement was compacted, restoring the
  1500-line limit. No implementation was hidden, moved to an unchecked path,
  or used to weaken the audit.

## Changes Made

- Added the specialization-parameter bound check and filtered unnamed
  parameters from scalar, integral, pack, member, decltype, and inferred
  substitution maps.
- Removed empty pack keys at every active replay boundary and skipped empty
  pack expansion tokens.
- Replaced suffix-based variable-template detection with typed
  TemplateDefinition::variable_template lookup.
- Replaced repeated function-template registry scans with indexed lookup.
- Updated this audit and refreshed the concise current failure/next-checkpoint
  map in pa21/plan.md.

## Validation

- make -C dev cppgm++: passed.
- Focused regression
  tests/general/400-template-template-fixed-prefix-pack-order.t: passed
  (1/1).
- The two former timeout fixtures now terminate in about 0.2 seconds each;
  they remain ordinary current-PA semantic status failures rather than
  timeouts. No timeout failure remains in the full report.
- Required make test-report ACTIVE_TEST_REPORT_PAS='pa21': 96/215 passed,
  preserving the audit-turn baseline of 96/215. The remaining set is
  104 exit-status mismatches, 14 relaxed LowIR mismatches, and one invalid
  LowIR result; the command is nonzero because PA21 is not complete yet.
- Required prior-through gate: 1635/1635 through PA20 passed.
- perl scripts/cppgm_file_audit.pl --stage pa21 --paths dev/src: passed with
  eight warning-only header-division findings and no fatal finding.
- git diff --check: passed. The audit fixes and documentation are committed
  together, and the final worktree is clean.
