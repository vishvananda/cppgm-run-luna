# PA26 Checkpoint Audit

## Checkpoint

This audit covers the checkpoint landed by `89342df` (`Implement PA26
member-pointer template replay`) and the current `Checkpoint Scope` in
`pa26/plan.md`.  It reviewed `pa26/README.md`, the PA26 tests and checked-in
LowIR fixtures, the complete primary test log, the recent PA25 audit commit,
and every source file changed by the checkpoint.

## Scope Reviewed

- PA11 parsing and semantic preservation of data/function member-pointer
  types, owners, cv/ref qualifiers, packs, and declarators.
- PA14/PA17/PA25 lowering for member-pointer formation, `.*`/`->*`, object
  adjustment, multi-base facts, and the PA26 single-vptr RTTI boundary.
- PA18 template deduction, partial-specialization ordering, nested-owner
  replay, non-type member-pointer substitution, and generated AST replay.
- Build/source-set ownership, file-size and division checks, prior-PA
  compatibility, the checkpoint PA26 baseline, and the complete current-PA
  failure set.

## Findings

- No compiler phase was skipped, and no reference binary, host compiler,
  interpreter, VM, trampoline, embedded test payload, timeout workaround, or
  source/test-specific acceptance gate was found in the checkpoint.  Failed
  semantic cases continue to fail rather than being converted into success.
- The replay path carried member-pointer values in a string map and then
  reclassified any downstream `&...::...` spelling as a member pointer.  That
  was a stringly semantic fact and could misclassify an ordinary function
  pointer.  This was a checkpoint-level blocker.
- Nested concrete-owner recovery accepted the first indexed candidate when
  context matching failed.  That duplicated ownership inference downstream
  and could silently bind a generated member to the wrong specialization.  It
  was a checkpoint-level blocker.
- `FindContextConversionOperator` recomputed the full conversion-binding
  graph once per candidate.  After PA26 added multi-base graph traversal this
  was avoidable hot-path recomputation.  The function-parameter expansion
  logic was also duplicated in two type-spelling paths.
- The `dynamic_cast<void*>` path was checked for fallback success behavior.
  The supported PA26 path performs the null check, reads the single-vptr
  ABI's complete-object offset, adjusts the source address, and returns it.
  The broader multi-vptr/virtual cases remain outside the PA26 contract.  The
  materialized runtime block has no incoming edge in the supported path and is
  part of the checked-in PA26 LowIR shape; it is not used to accept an
  unsupported source or to manufacture a success result.
- File audit found no fatal size, hidden-file, weakened-check, or unchecked
  implementation path.  The duplicate type-spelling block was removed; the
  audit remains within the header-size limit.  The remaining 11 file-audit
  messages are nonfatal existing division warnings, not new bypasses.

## Changes Made

- Replaced string member-pointer replay values with validated AST address
  expressions, cloned on replay.  The persistent replay cache now uses a
  structured `(context, spelling)` key, and the downstream rewrite no longer
  reparses or heuristically classifies emitted text.
- Changed nested-owner recovery to accept only an exact context match or one
  uniquely validated indexed candidate; ambiguous candidates now fail the
  recovery instead of selecting an arbitrary owner.
- Snapshot `ConversionBindings(source)` once per lookup, consolidated the
  duplicate function-parameter expansion logic, and removed the duplicate
  type-spelling block.
- Preserved the existing PA26/PA25 interfaces and source-set ownership; no
  tests, references, or unchecked source paths were changed.

## Validation

- `make build`: pass.
- `make test-report ACTIVE_TEST_REPORT_PAS='pa26'`: exit 2 with the expected
  complete current-PA result of **41/66**; the 25 failures are unchanged from
  the audit-start baseline, so `stageProgressPreserved` passes.
- Required prior-through command (`n=26; ... make test-report-through-pa25`):
  pass, **2669/2669**.
- `perl scripts/cppgm_file_audit.pl --stage pa26 --paths dev/src`: pass with
  11 nonfatal warnings and no fatal findings.
- `git diff --check`: pass.  The refreshed `Remaining Work Map` in
  `pa26/plan.md` contains all 25 current-PA failures and selects the
  pack-expanded-base plus dependent replay/lookup group as the next
  substantial checkpoint.
