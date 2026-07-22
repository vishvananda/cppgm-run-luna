# PA21 checkpoint audit

## Scope Reviewed

Reviewed the latest `Checkpoint Scope` and failure map in `pa21/plan.md`, the
PA21 contract in `pa21/README.md`, `TESTING_AND_REFERENCES.md`, the recent
checkpoint and audit commits (`8e50532`, `97b3156`, `d866a74`, and `feaf491`),
the complete current-PA log, and every source file changed by the checkpoint
and this audit.  The scope is the template-entity/non-type replay checkpoint:
typed template parameters, partial-specialization replay, pack-aware member
lookup, dependent integral evaluation, and ordinary PA14-PA20 LowIR lowering.

## Findings

- The compiler still follows preprocessing, tokenization, parsing, semantic
  collection/rewrite, and ordinary LowIR lowering.  The checkpoint contains no
  skipped phase, dummy output, embedded test payload, reference/host-compiler
  invocation, interpreter/VM/trampoline substitute, timeout workaround, or
  source/test-specific acceptance gate.
- `EvaluateActivePackSize` had an unsafe sole-active-pack fallback: an
  unrelated `sizeof...(name)` could succeed by inference.  This was a real
  checkpoint blocker and is removed.  Resolution now requires a named active
  typed/named pack, with only an exact first-element compatibility bridge for
  legacy AST nodes that already lost the pack identifier.
- Scalar substitution could rewrite the operand of `sizeof...(pack)` before
  pack resolution.  Pack-size ranges are now protected during identifier
  replacement, so the source pack fact is consumed before scalar replay.
- Two new hot paths performed avoidable registry-wide work: every template-id
  pack expansion walked all definitions/specializations, and every unqualified
  constant replay walked all class declarations and their members.  Collection
  now builds pack-name and constant-member-owner indexes once; replay performs
  indexed lookups instead of repeated full-registry/full-class scans.
- Constant-member ownership is recorded while class and generated-class ASTs
  are collected.  The implementation does not recover it by reparsing emitted
  LowIR or maintain a hidden output fragment.  Existing typed parameter,
  specialization, integral-value, and generated-node maps remain the owners of
  their respective facts.
- The first validation exposed a one-line file-size violation in
  `pa18_templates_rewrite.h` (1201 lines); the declaration was compacted to
  restore the 1200-line limit.  The file audit now has only the repository's
  eight warning-only header-division findings and no fatal finding.
- The refreshed PA21 report is 82/215: 117 exit-status mismatches, 15 relaxed
  LowIR mismatches, and one invalid-LowIR result, with no timeout failures.
  The audit began and ended at 82/215, preserving the current-PA baseline;
  earlier PAs remain passing.  These are remaining PA21 behavioral cases in
  the refreshed work map, not accepted shortcut, performance, ownership, or
  audit defects in this checkpoint.

## Changes Made

- Added collection-time `template_pack_names_` and
  `constant_member_owners_` indexes and the pack-size-preserving replacement
  helper in `pa18_templates_collection.cpp/.h`.
- Replaced registry-wide pack discovery in `pa18_templates_rewrite.cpp` with
  indexed lookup and preserved `sizeof...` operands during final rewriting.
- Removed arbitrary pack-size inference and reordered typed pack-size
  evaluation before scalar substitution in
  `pa18_templates_rewrite_instantiate.cpp/.h`.
- Indexed generated constant members during registration and added explicit
  parentheses to the existing decltype operator-precedence expression in
  `pa18_templates_rewrite_decltype.h`.
- Refreshed this audit and the concise current failure/next-checkpoint map in
  `pa21/plan.md`.

## Validation

- `make -C dev cppgm++`: passed.
- Focused checkpoint/regression set: `pa21 check` passed **7/7**.
- Required full report: **82/215** passed; the report log is
  `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.
- Required prior-through gate: **1635/1635** through PA20 passed.
- `perl scripts/cppgm_file_audit.pl --stage pa21 --paths dev/src`: passed with
  8 warning-only findings and no fatal finding.
- `git diff --check`: passed; the audit fixes and documentation are committed
  together, with the final worktree left clean.
