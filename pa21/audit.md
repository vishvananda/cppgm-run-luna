# PA21 checkpoint audit

## Scope Reviewed

Reviewed the latest Checkpoint Scope and complete failure map in
`pa21/plan.md`, the PA21 contract in `pa21/README.md`,
`TESTING_AND_REFERENCES.md`, the changed source files and source-set entries,
recent commits `92c2a21`, `d1c2508`, `297acef`, `a74b4e5`, `a916cd1`,
`64aa678`, `6b2c6fc`, and `ff9d4f3`, and the full primary log at
`/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.

The checkpoint scope is the typed friend-owner/access slice: ordinary and
qualified friend function templates, nested friend replay, existing-template
friends, friend class templates, private/protected access, dependent
`typename` construction, and generated friend-class inheritance ordering.  The
focused set is the nine tests listed in the current Checkpoint Scope.  The
review also covered the PA11 semantic model, PA14 access and lowering paths,
PA18 collection/replay, and the complete current-PA failure map.

## Findings

- The landed replay used a synthetic `friend_owner_only` declaration with an
  empty parameter clause.  PA11 and PA14 explicitly skipped that node, then
  recovered access from an untyped name and stripped generated-name markers.
  That was a real phase bypass, signature loss, and stringly access shortcut;
  it was a checkpoint-level blocker and is removed.

- Friend declarations now retain their complete transformed declarator and go
  through normal template-parameter processing and PA11 analysis.  Friend
  edges carry entity kind and a `TypePtr` target in `FriendAccess`; PA14 checks
  the selected function/class type and canonical function identity.  No
  emitted LowIR or generated source text is reparsed to recover these facts.

- Flattened `SpellNode(...).find("friend")` gates were removed.  Friend status
  is recognized from structured declaration nodes and cached as
  `TemplateDefinition::friend_declaration`.  Generated layout ordering is
  restricted to an actual structured friend declaration and typed layout
  dependencies; this is an ordering applicability check, not a source- or
  test-specific success gate.

- The friend-class access path had an avoidable full walk of the analyzer's
  class-type map for every private/protected access.  A typed reverse index is
  built once after PA11 analysis, so hot-path access checks visit only friend
  owners associated with the member's owner class.

- The reviewed pipeline still executes preprocessing, tokenization, parsing,
  PA11 semantics, PA14 lowering, PA18 replay, and ordinary LowIR emission.  No
  dummy or embedded output, interpreter/VM/trampoline substitute, reference or
  host-compiler invocation, timeout workaround, fallback-success path, or
  weakened test acceptance gate remains in the checkpoint paths.

- The file audit initially exposed two size regressions caused by the new
  implementation.  Dependent type resolution and structured friend-specifier
  traversal were moved to their owning `.cpp` files.  The final audit has no
  fatal size, source-set, hidden-fragment, or unchecked-path finding; its ten
  existing division/complexity results are warnings only.

## Changes Made

- Removed `friend_owner_only`, its synthetic declaration builder, PA11/PA14
  skip paths, and untyped `friend_names`/generated-symbol matching.
- Added typed `FriendAccess` relations, dependent template-id type
  construction, complete friend-template replay, structured friend metadata,
  and typed function/class access matching.
- Added the one-time derived friend-owner index to eliminate the class-registry
  scan from the access hot path.
- Moved the new dependent type-resolution and friend-specifier implementations
  out of oversized headers so file-audit limits remain healthy.
- Refreshed `pa21/plan.md` with the complete 62-test failure map and the next
  inherited/using member-template owner-replay checkpoint.

## Validation

- The nine-test focused run: **7/9** relaxed reference comparisons passed; all
  nine compile/status checks passed.  The two non-passing comparisons are the
  known presentation-only LowIR differences for existing-template friend
  operator and qualified friend member cases.
- Required current-PA report, `make test-report ACTIVE_TEST_REPORT_PAS='pa21'`:
  **153/215 passed**, **62 failures**, exactly matching the refreshed map and
  preserving the checkpoint baseline of 153/215 (above the prior 146/215
  turn-start baseline).
- Required prior-through command for `n=21`:
  `make test-report-through-pa20` — **1635/1635 passed**.
- Required file audit, `perl scripts/cppgm_file_audit.pl --stage pa21 --paths
  dev/src` — passed with ten warnings and no fatal finding.
- `make -C dev cppgm++` and `git diff --check` passed.  The cohesive audit
  changes were committed, and the final `git status --short` is empty.
