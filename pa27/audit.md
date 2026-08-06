# PA27 Final Stage Audit

## Stage

This is the final audit of the PA27 full-stage implementation, including the
checkpoint that completed the assignment.  The implementation was reviewed
against `pa27/README.md`, `pa27/plan.md`, the relevant PA27 and earlier-stage
tests, the recent commits `0362662` and `7b8c2c7`, and the primary test log at
`/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.

No earlier `pa27/audit.md` was present in the checkout, so this file records
the consolidated stage audit rather than replacing a prior PA27 audit.

## Audit Plan

1. Re-read the repository testing/reference rules and the PA27 contract.
2. Compare the checkpoint plan, recent source changes, source-set ownership,
   and PA27 regressions with the implementation actually in `dev/src`.
3. Trace the semantic ownership of virtual-base layout, multiple-base
   polymorphism, hidden ABI carriers, construction/destruction, dispatch,
   RTTI, and casts through the PA11, PA14, PA17, and PA25 layers.
4. Check for stale derived state, pointer/container ownership hazards,
   first-base or source-spelling shortcuts, reference/host-tool shortcuts,
   unnecessary repeated work, and accidental test or reference changes.
5. Apply only cohesive architectural cleanup, then run the required focused
   and through-stage validation, the file audit, whitespace checks, and a
   final clean-worktree check before committing.

## Findings

The PA27 contract is implemented as a monotonic extension of PA26.  The
semantic model stores ordered typed direct bases and per-edge virtual/access
facts, computes a deterministic visited virtual-base closure and offsets, and
records non-virtual size, primary-base selection, virtual-table views, and
final overriders.  This gives layout and lowering one source of truth for
shared virtual bases and non-primary polymorphic subobjects.

The PA14 lowering consumes those typed facts consistently.  Address
projection follows typed base paths and virtual offsets; ABI construction
adds typed hidden virtual-base carriers; parameter naming and source mapping
match the same carrier order; and constructor entries, complete/base
constructors, destructors, object cleanup, member calls, and virtual dispatch
share the same projection and ownership rules.  Complete construction owns a
virtual base exactly once, while nested base entries forward the view needed
by the already-owned subobject.

The PA17/PA25 path renders primary and secondary vtable views, final-overrider
adjustments, VTTs, RTTI, adjustor thunks, `typeid`, and the PA27 cast cases
from the semantic graph.  The implementation has no reference-binary,
previous-solution, host-compiler, interpreter, or test-name output shortcut.
The changed source files are registered in
`dev/frontend_source_sets.mk`; no tests or `.ref` files were modified.

The only source cleanup required by this audit was derived-state hygiene:
class rematerialization already cleared most layout and polymorphism state,
but `ProcessClass` did not explicitly clear `virtual_base_roots` and
`virtual_table_views`.  Those vectors are now cleared at the same ownership
boundary before the class is rebuilt.  This does not alter the established
layout algorithm; it prevents stale derived facts if a class is reprocessed
before the later layout/finalization helpers run.

The file audit reports 14 non-fatal warnings in existing staged code (header
division, two catch-all helpers, one complexity warning, and the existing
PA25/PA14 duplication warning).  It reports no fatal file-audit violation.

## Changes Made

- Added explicit clearing of `virtual_base_roots` and
  `virtual_table_views` in `Analyzer::ProcessClass`.
- Added this final PA27 audit and the implementation-grounded Architecture
  Review and Final Architecture Review sections to `pa27/plan.md`.
- Preserved all PA27 tests, checked-in references, and earlier assignment
  sources.

## Validation

- `make test-pa27`: PASS, 34/34 PA27 tests.
- `make test-report-through-pa27`: PASS, 2769/2769 tests and 27/27 stages;
  all tracked stages pass.
- `perl scripts/cppgm_file_audit.pl --stage pa27 --paths dev/src`: PASS,
  with 14 non-fatal warnings.
- `git diff --check`: PASS.
- Final commit and `git status --short`: clean.
