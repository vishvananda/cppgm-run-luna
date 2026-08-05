# PA26 Checkpoint Audit

## Checkpoint

This audit covers the latest PA26 checkpoint scope landed by `814aafc`
(`Complete PA26 template replay increment`) together with its preceding
member-pointer replay commits `21f80f9`, `e4c1f96`, and `0a80a4f`.  It was
performed on 2026-08-05 against the checkpoint state reported in
`/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.

## Scope Reviewed

- `pa26/README.md`, the latest `Checkpoint Scope` and `Remaining Work Map` in
  `pa26/plan.md`, the PA26 tests and checked-in LowIR fixtures.
- The recent commit chain, all source files changed by the checkpoint, the
  frontend source-set ownership, and the PA26 file-audit result.
- PA11-PA25 preservation, including member-pointer formation/application,
  friend ADL, dependent `sizeof`, nested static-data replay, anonymous
  namespace identity, and the PA26 single-vptr `dynamic_cast<void*>` boundary.
- Shortcut and architecture checks for skipped phases, fabricated success or
  output, host/reference execution, interpreters/VMs/trampolines, timeout
  workarounds, stringly semantic facts, ownership duplication, reparsing,
  repeated scans, copying, and unchecked/file-audit paths.

## Findings

- No compiler phase is skipped.  No reference binary, host compiler,
  interpreter, VM, trampoline, embedded payload, timeout workaround, or
  source/test-specific acceptance gate is used by the PA26 compiler path.
  Unsupported or failed semantic cases still fail; they are not converted to
  successful LowIR or dummy output.
- The checkpoint's aggregate replay used one mutable expected-type string and
  selected the first non-static field, while the downstream unary rewrite
  recognized qualified template addresses from emitted spelling.  That was a
  real stringly fact and ownership/field-selection blocker: later aggregate
  elements could be assigned the wrong overload destination, and ordinary
  qualified function-template addresses could enter the member-template path.
- The final PA26 failure was in the shared comparison parser, not in sidecar
  generation.  Nested parameter/metadata parsing clobbered the enclosing
  regular-expression captures, and comma-bearing `object=` metadata was split
  as separate fields.  This caused a valid LowIR comparison to abort with an
  undefined signature/sidecar error.  It was fixed and is no longer a current
  failure.
- The replay fix performs one declaration-order field walk per relevant
  aggregate and stores expected types by AST node identity.  The binding scope
  swaps maps only when a declaration contributes a binding, then restores it
  across recursive transforms and setup-time exceptions; this keeps scope
  management O(1) rather than copying the active map at every AST node.  No
  new full-suite walk, repeated cast evaluation, avoidable quadratic lookup,
  or unbounded copying was introduced.
- The generated static-data owner path preserves namespace and anonymous
  namespace identity while queuing replayed definitions at namespace scope.
  The focused static-table and unnamed-namespace regressions pass, and the
  generated owners remain deterministic.  PA26's single-vptr RTTI boundary
  remains explicit; virtual inheritance and multi-vtable RTTI are PA27's
  assignment boundary, not hidden PA26 success paths.
- The file audit has no fatal finding, no hidden implementation fragment, no
  weakened check, and no unchecked source path.  The collection header remains
  at the 1200-line limit after the new declaration was folded into its
  existing compact declaration line.  The 12 remaining audit messages are
  warning-level existing header-division/catch-all/complexity advisories.

## Changes Made

- In `dev/src/pa18_templates_collection.h`, replaced the single mutable
  initializer expected type with a scoped AST-node-to-type map and declared a
  structural qualified member-template-address predicate.
- In `dev/src/pa18_templates_rewrite_helpers.cpp`, replaced the broad spelling
  gate with a parsed template-range check that only selects a non-type
  qualified member address such as `wrap<&T::call>`.  Aggregate replay now
  resolves each non-static field in declaration order and associates that
  field's rewritten type with the corresponding address node.
- In `dev/src/pa18_templates_rewrite_expressions.cpp`, use the AST predicate
  and node-specific expected type when materializing the synthetic member
  call.  Ordinary addresses such as `create<Service, context>` stay on the
  established function-address path.
- In `scripts/compare_results_common.pl`, preserve outer declaration captures
  across nested parsers, parse metadata values containing commas, canonicalize
  those groups consistently, and report missing signatures as a comparison
  error instead of dereferencing an undefined value.
- Refreshed `pa26/plan.md` from the complete zero-failure PA26 set and selected
  the PA27 virtual/RTTI ABI group as the next checkpoint.  No tests or `.ref`
  fixtures were edited.

## Validation

- `make -C dev cppgm++`: pass.
- Focused PA21 regressions (2), PA23 explicit-pack replay (1), and PA26
  friend-ADL, using-directive, pack/decltype, and anonymous-namespace replay
  cases: pass.
- `make test-pa26`: **66/66** pass.
- Required `make test-report ACTIVE_TEST_REPORT_PAS='pa26'`: **66/66** pass.
- Required prior-through command (`n=26; ... make test-report-through-pa25`):
  **2669/2669** pass.
- Standard root `make test-report-through-pa26`: **2735/2735** pass.
- `perl scripts/cppgm_file_audit.pl --stage pa26 --paths dev/src`: pass with
  12 warning-level findings and no fatal findings.
- `git diff --check`: pass.  The final commit leaves `git status --short`
  empty.
