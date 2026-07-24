# PA21 checkpoint audit

## Checkpoint 63 audit — 2026-07-24

### Scope Reviewed

Reviewed the latest Checkpoint 63 Scope, Result, Remaining Work Map, and next
group in `pa21/plan.md`; `pa21/README.md`; `TESTING_AND_REFERENCES.md`; the
recent commits `e0849ed`, `5ddf6bc`, `0e88087`, `6f71b3c`, and their PA21
predecessors; every changed PA18 source file and
`dev/frontend_source_sets.mk`; the scoped explicit-specialization and
explicit-instantiation tests; and the complete primary log at
`/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.

The review followed the specialization registry through declaration
collection, replay and lookup, explicit materialization, ordinary AST
transformation, and LowIR emission.  It also checked the normal preprocessing,
parsing, PA11 semantic, PA14 lowering, PA18 template, and final LowIR stages,
including extern-template and constructor paths.

### Findings

- Class-specialization instantiation history was keyed by concatenated
  qualified-name and argument strings in both replay and explicit-specialization
  checking.  That duplicated ownership recovery and made a delimiter-packed
  string carry an entity fact that belongs in the template registry.
- Explicit-instantiation validation ran before candidate/entity lookup and
  could reject valid member or conversion operators.  More importantly,
  `TransformNode` could fall through to ordinary transformation when explicit
  materialization returned false, allowing an invalid declaration to succeed
  (and silently dropping an invalid extern declaration).
- The fallback needed for a fully explicit extern template-id was initially
  too broad: matching only argument counts could select the first overload
  rather than the declaration denoted by the source.  This was an architectural
  correctness blocker, not a test-specific exception.
- Moving the operator helpers and the using-directive implementation exposed
  two source-audit limits (`pa18_templates_calls.cpp` and
  `pa18_templates_rewrite.h`).  Leaving either packed or relying on an
  unchecked fragment would have been a file-audit bypass.
- No skipped compiler phase, dummy or embedded output, interpreter/VM/trampoline
  substitute, host/reference-binary invocation, timeout workaround,
  source/test-specific acceptance gate, weakened check, emitted-text reparse,
  or new avoidable full-suite/hot-path scan was found after these issues were
  corrected.

### Changes Made

- Added `ClassSpecializationIdentity`, keyed by the canonical primary
  `TemplateDefinition*` and canonical argument vector, and used it for both
  specialization-order checking and recorded class instantiation history.
  This keeps ownership and identity typed at the registry boundary.
- Moved builtin-operator validation after entity lookup, distinguished
  non-member operators from valid member/conversion operators, and used the
  existing typed class/entity lookup for operand validation.
- Made explicit and extern instantiation handling terminal in `TransformNode`:
  invalid targets now fail, valid extern declarations suppress materialization,
  and class-template constructors validate and materialize through their typed
  owner.  The explicit-argument fallback now requires complete non-pack
  bindings and an exact substituted parameter-type match for the selected
  candidate.
- Moved builtin operator inference to the new checked
  `pa18_templates_calls_operator.cpp`, registered it in
  `dev/frontend_source_sets.mk`, and moved `RecordUsingDirective` out of the
  header.  The resulting files remain below the audit size limits.
- No tests, reference fixtures, or acceptance scripts were changed.

### Validation

- `make -C dev cppgm++` — passed after the source split.
- The six focused extern-template and specialization-order fixtures — **6/6
  passed**.
- `make test-report ACTIVE_TEST_REPORT_PAS='pa21'` — **182/215 passed**;
  the complete 33-case failure set is refreshed in `pa21/plan.md`, and the
  current result is at the turn-start baseline.
- Required prior-through command for `n=21` — **1635/1635 passed** through
  PA20.
- `perl scripts/cppgm_file_audit.pl --stage pa21 --paths dev/src` — passed
  with nine pre-existing header-division warnings and no fatal finding.
- `git diff --check` — passed.  The checkpoint-level shortcut, fallback,
  ownership, performance, and file-audit blockers found in this review are
  fixed; the remaining current-PA failures are the planned next semantic
  groups, not regressions from this checkpoint.

## Checkpoint 62 audit — 2026-07-24

### Scope Reviewed

Reviewed the latest Checkpoint 62 Scope, Result, Remaining Work Map, and Next
Checkpoint in `pa21/plan.md`; `pa21/README.md`; `TESTING_AND_REFERENCES.md`;
the current `HEAD` (`0e88087`) and recent PA21 commits `6f71b3c`, `4b7af81`,
`ab20a8b`, `bc9f345`, and `08e28c8`; the complete PA11/PA18 source diff and
`dev/frontend_source_sets.mk`; the nine owner/pack/operator probes; and the
full primary log at
`/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.

The review covered deferred using resolution, typed owner propagation through
qualified calls and member-template addresses, nested specialization replay,
static/local member classification, generated declaration ownership, bound
pack hints, and the resulting LowIR path.  It also checked that the normal
preprocessing, parsing, PA11, PA14, PA18, and LowIR stages remain in the
pipeline.

### Findings

- `pa18_templates_rewrite.cpp` and `pa11_semantics_analyzer.cpp` were each at
  the exact 1500-line source-audit ceiling.  The inclusive limit hid packed
  responsibilities and was a file-audit bypass risk, so it was a checkpoint
  blocker.
- Replay and emission repeatedly recovered static-member status by serializing
  declaration ASTs and searching for the string `static`.  That was stringly
  semantic state and repeated work on lookup, generation, and object-member
  evaluation paths.  It also missed static declarations wrapped in a member
  template, which made the out-of-class generated-name path depend on a later
  recovery.
- The active concrete owner was carried as only a name.  Nested replay then
  re-derived its definition and arguments from short-name maps, duplicating
  ownership recovery and making partial-specialization selection fragile.
- Member-call pack hints walked the enclosing parameters a second time after
  the same bound-pack information had already been computed.  This was
  avoidable hot-path work.
- No skipped compiler phase, dummy or embedded output, interpreter/VM/trampoline
  substitute, reference or host-compiler invocation, timeout workaround,
  fallback-success path, source/test-specific acceptance gate, weakened check,
  hidden implementation fragment, or unchecked source-set path was found.

### Changes Made

- Moved the deferred using-declaration implementation into
  `pa11_semantics_analyzer_using.cpp` and unary expression lowering into
  `pa18_templates_rewrite_expressions.cpp`, registering both translation units
  in `dev/frontend_source_sets.mk`.  The implementation is now in cohesive
  checked source files below the audit ceiling.
- Added structured `TemplateDefinition::static_member` metadata and indexed
  static members by class during collection.  Member-template declarations,
  generated class aliases, and concrete-owner paths use the same typed index;
  emission, replay, and source-object evaluation no longer serialize ASTs to
  recover static status.  Generated static-member facts are indexed once and
  reused for each registered path.
- Replaced the string-only active owner with `ConcreteOwnerContext`, carrying
  the selected definition and template arguments alongside its presentation
  name.  Qualified-call, address, nested-replay, and exception-restoration
  paths now set and restore that context as one value.
- Reused the already computed bound-pack map when constructing instantiation
  hints, removing the duplicate enclosing-parameter walk.
- No tests, reference fixtures, or acceptance scripts were changed.

### Validation

- `make -C dev cppgm++` — passed.
- Required current-PA check,
  `make test-report ACTIVE_TEST_REPORT_PAS='pa21'` — **179/215 passed**, equal
  to the turn-start/checkpoint baseline; the complete 36-case failure set is
  recorded in `pa21/plan.md` and drives the next group.
- Required prior-through command for `n=21` — **1635/1635 passed** through
  PA20.
- Required file audit,
  `perl scripts/cppgm_file_audit.pl --stage pa21 --paths dev/src` — passed
  with nine warnings and no fatal finding.
- `git diff --check` passed.  The focused nine-probe run remains 1/9 because
  eight probes are part of the documented remaining owner/replay set; no
  checkpoint regression below the 179/215 baseline was introduced.
- The refreshed plan selects the 13-case generated owner/member replay and
  LowIR-body-ownership group next, with the 15/5/3 smaller groups bundled
  afterward.  No checkpoint-level architecture, performance, shortcut, or
  audit blocker remains.

## Superseded audit (checkpoint 61)

### Scope Reviewed

Reviewed the latest `Checkpoint Scope` in `pa21/plan.md`, commit `4b7af81`
and its predecessor `ab20a8b`, the PA21 contract in `pa21/README.md`,
`TESTING_AND_REFERENCES.md`, all changed compiler/source-set files, the three
focused tests for hidden-friend ADL, anonymous-union storage, and out-of-class
nested member classes, the six planned owner-replay tests, and the complete
current-PA result in
`/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.

The reviewed checkpoint covers generated hidden-friend association,
constructor ownership during specialization replay, unnamed aggregate storage,
enclosing substitutions for nested classes, plain nested template-ids, and
static nested-member expression preservation.

### Findings

- The checkpoint had compressed `pa18_templates_rewrite.cpp` to the exact
  1500-line audit limit.  That was a file-audit workaround and obscured the
  constructor replay implementation; it was a blocker.
- Nested static-member detection serialized declaration ASTs with repeated
  `SpellNode(...).find(...)` calls during replay.  This was an avoidable
  stringly fact and repeated work on a lookup path; it was a blocker.
- Generated hidden-friend ADL scanned every binding in an owner scope on each
  operator lookup and did not use the binding's typed `friend_owner` relation
  to select the candidates.  This duplicated ownership recovery and could
  admit unrelated generated names; it was a blocker.
- Constructor replay recovered the source constructor by bare last-component
  name and searched the global constructor index without checking the source
  owner.  Same-named constructors in another class could therefore be
  selected; it was a blocker.
- No skipped compiler phase, dummy or embedded output, interpreter/VM/trampoline
  substitute, timeout workaround, reference/host-compiler invocation,
  source/test-specific acceptance gate, weakened check, or unchecked
  implementation fragment was found in the reviewed scope.

### Changes Made

- Moved `MaterializeInitializerConstructor` into the dedicated compiled
  `pa18_templates_rewrite_constructor.cpp` translation unit and registered it
  in `dev/frontend_source_sets.mk`; `pa18_templates_rewrite.cpp` is now 1432
  lines rather than being packed to the audit ceiling.
- Added `TemplateDefinition::static_members`, populated it once from structured
  declaration-specifier nodes during template collection, and replaced the
  replay-time AST serialization with typed set membership checks.
- Added a one-time PA14 hidden-friend binding index keyed by the typed owner
  `Type*`; generated suffix matching now runs only over that owner’s indexed
  bindings.
- Kept the generated-to-source constructor mapping but enforced the qualified
  source owner, including the compiler’s canonical nested `Node::Node` owner
  spelling, before selecting a member-template constructor.
- No tests, references, or acceptance scripts were changed.

### Validation

- Required PA21 report: `make test-report ACTIVE_TEST_REPORT_PAS='pa21'` —
  **176/215 passing**, unchanged from the audit turn-start baseline; the
  complete non-passing set is refreshed in `pa21/plan.md`.
- Focused checkpoint tests — **3/3 passed**:
  `spec/300-hidden-friend-template-operator-adl.t`,
  `general/300-anonymous-union-storage-constructor-noop.t`, and
  `spec/300-member-class-template-out-of-class.t`.
- Required prior-through command for `n=21`, `make test-report-through-pa20` —
  **1635/1635 passed**.
- Required file audit, `perl scripts/cppgm_file_audit.pl --stage pa21 --paths
  dev/src` — passed with the existing ten warnings and no fatal finding.
- `git diff --check` and the no-debug/source-set checks passed; the audit
  changes are ready to commit with a clean worktree handoff.

## Superseded audit (earlier checkpoint)

### Scope Reviewed

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

### Findings

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

### Changes Made

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

### Validation

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
