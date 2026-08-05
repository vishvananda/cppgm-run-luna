# PA26 Final Stage Audit

## Stage

This is the final full-stage audit for `pa26 full-stage`, performed on
2026-08-05.  It consolidates the checkpoint audit recorded in this file with
the complete PA26 implementation and the cleanup performed during this audit.
The required stage gate is `make test-report-through-pa26`; the primary log
under review is
`/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.

The review preserved PA11-PA25 behavior and covered the checkpoint commits
`21f80f9`, `e4c1f96`, `0a80a4f`, and `814aafc`, the checkpoint audit
`d013cf2`, and the integrated changes since the PA25 audit `d98f3ca`.

## Audit Plan

1. Read `TESTING_AND_REFERENCES.md`, `pa26/README.md`,
   `pa26/plan.md`, the PA26 tests and earlier through-stage test contract.
2. Review the recent commit chain, every changed implementation source,
   `dev/frontend_source_sets.mk`, and all checkpoint findings.
3. Trace actual ownership and data flow for canonical direct-base layout,
   inherited lookup/access, receiver adjustment, generated special members,
   member-pointer formation/conversion/application, constant evaluation,
   template replay, and the PA27 RTTI boundary.
4. Look for shortcut behavior, duplicated semantic facts, first-base regressions,
   ambiguous-resolution leaks, lifetime/unwind omissions, avoidable copying or
   repeated scans, and source-set/file-audit gaps.
5. Run the focused PA26 suite, the required file audit, and the required root
   through-stage report; leave a cohesive commit and a clean worktree.

## Findings

- PA26's intended architecture is present.  PA11 owns typed classes, ordered
  `direct_bases`, direct-base offsets, stable member ownership/index facts,
  access, constant evaluation, and class layout.  PA14 consumes those facts
  for lookup, base-path adjustment, LowIR addresses, method receivers, and
  generated lifetime operations.  PA25 transfer/constructor paths use the
  same direct-base helpers.
- The original first-base assumptions that affected the PA26 contract were
  corrected across semantic queries, pointer conversions, using/access paths,
  constants, empty-base/triviality checks, default construction, copy/assignment,
  and destruction.  Generated base lifetime actions use declaration order for
  construction/copy/assignment and reverse order for destruction.
- Member lookup does not silently choose one of multiple inherited candidates.
  Direct declarations hide inherited declarations, using-declarations are
  explicit, and distinct base paths remain distinguishable.  Base adjustment
  enumerates typed paths and rejects missing or ambiguous member-pointer owner
  conversions.
- The non-primary member-pointer conversion gap found during this audit was a
  real correctness issue not covered by the checked-in PA26 primary suite.
  Data-member conversion now adds the non-primary base offset while preserving
  null.  Member-function conversion carries the adjustment in the high half of
  the existing i128 representation; `.*`/`->*` decodes it and the call
  emitter adjusts the receiver.  A temporary focused LowIR probe confirmed the
  generated non-primary data and function paths carry the expected offset.
- The first full through-stage run exposed one PA15 preservation mismatch: the
  new nested-base constructor loop projected a zero-offset primary base.  The
  adjustment is now emitted only for a nonzero base offset, preserving the
  established PA15 LowIR shape while retaining non-primary PA26 adjustment.
- The checkpoint's template replay changes use AST-node identity and structural
  predicates rather than a mutable expected-type string or broad spelling gate.
  Nested static-data replay preserves namespace/anonymous-namespace ownership.
  The shared comparator preserves outer captures and parses comma-bearing
  metadata without weakening the semantic comparison.
- The file audit initially exposed five fatal size/function-size thresholds
  after full-stage growth.  They were resolved by moving member-pointer
  conversion, inherited-constructor synthesis, and constant-call noexcept
  analysis into responsibility-named translation units, adding both to the
  cppgm++ source set, and reducing one resolver's oversized body.  The final
  audit has no fatal finding; its twelve remaining messages are warning-level
  pre-existing ownership/complexity advisories.
- No compiler stage is skipped and no reference binary, host compiler,
  interpreter, VM, trampoline, embedded answer, timeout workaround, or
  source/test-specific acceptance gate is used.  No tests or `.ref` files
  were changed.  The only intentional deferrals are the PA27 virtual/RTTI
  cases explicitly listed by the README: virtual inheritance, polymorphic
  multiple inheritance, and RTTI requiring multiple vtable views.

## Changes Made

- Centralized all-base compatibility and closure traversal in
  `DirectBaseTypes` and `BaseTypeClosure`, and updated PA11 semantic,
  lookup, access, constant, and layout queries to use the canonical model.
- Updated PA14 member collection, inherited constructors, base distance/path
  adjustment, empty-base handling, constructor/copy/assignment/destructor
  emission, and friend/access ownership for non-virtual multiple inheritance.
- Kept zero-offset primary nested-base construction on its established direct
  address path; only nonzero nested-base offsets request a projection.
- Added typed non-primary member-pointer conversion and call-receiver
  adjustment while preserving primary/single-inheritance output.
- Split `pa14_lowering_member_pointer.cpp`,
  `pa14_lowering_inherited_constructors.cpp`, and
  `pa11_semantics_constants_calls.cpp` into the frontend source set so
  implementation ownership matches responsibility and file-audit limits.
- Retained the PA18 typed replay and comparator fixes from the checkpoint and
  documented the final architecture in `pa26/plan.md`.
- Did not modify tests, checked-in references, or handout files.

## Validation

- `make -C dev cppgm++`: pass.
- `make test-pa15`: **200/200** pass after the preservation fix.
- `make test-pa26`: **66/66** pass.
- `perl scripts/cppgm_file_audit.pl --stage pa26 --paths dev/src`:
  pass with 12 warning-level findings and no fatal findings.
- `make test-report-through-pa26`: **2735/2735** pass.
- `git diff --check`: pass.
- Final exit condition: commit the cohesive cleanup and confirm
  `git status --short` is empty.
