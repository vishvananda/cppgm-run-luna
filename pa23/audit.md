# PA23 checkpoint audit

## Checkpoint 4 audit — 2026-07-29

### Scope Reviewed

- The latest `Checkpoint Scope` and Checkpoint 3 result in `pa23/plan.md`.
- The PA23 assignment contract in `pa23/README.md`, the repository testing and
  reference rules, and the focused PA21/PA22/PA23 witnesses for this replay
  path.
- Recent commits `b7f7787` (typed SFINAE checkpoint), `bc58f85` (forwarding
  pack checkpoint), `2608080` (reference-category checkpoint), and `af6768a`
  (PA22 architecture audit).
- The changed implementation files under `dev/src`, especially the PA18
  collection, typed replay, member lookup, selection, specialization, and
  materialization helpers.
- The full primary checkpoint log at
  `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`, plus
  a clean `b7f7787` comparison checkout.

### Findings

- The checkpoint remains on the real compiler pipeline required by the PA23
  README.  There is no skipped compiler phase, dummy or embedded output,
  interpreter/VM/trampoline substitute, reference-binary invocation, or
  source/test-specific acceptance gate in the audited changes.
- Recursive semantic re-entry is now represented as scoped substitution
  failure.  Class-specialization matching and class-template selection use
  normalized semantic identities and typed registry ownership; an exact
  re-entry does not succeed by silently returning a primary-template fallback.
- No timeout cap, timeout retry, or timeout-only acceptance path was added.
  The clean checkpoint compiler independently reproduces exit status 124 for
  both reentrant static-query inputs, and the final harness still reports only
  those two timeout fixtures.
- Owner and argument facts stay in typed substitutions, AST declaration kinds,
  `TemplateParameter` kinds, and the specialization registry.  The changed
  member path no longer uses `enable_if`/`disable_if` spelling as a dependency
  gate, direct nested-member recovery requires an AST-declared member and a
  matching non-type parameter, and changed typedef probes use AST specifiers
  rather than reparsing emitted text.
- Materialized template bases are protected only when the typed specialization
  registry confirms their ownership.  The protection helper tokenizes each
  source spelling once, eliminating the prior full-spelling scan for every
  substitution binding.  Selection arguments are normalized once and the
  repeated qualified-path normalizer is shared instead of duplicated in a hot
  lookup function.
- No new full-suite walk, dummy success path, unchecked implementation
  fragment, weakened check, or fixture/ref modification was introduced.  The
  helper definitions were moved into existing source modules when the file
  audit exposed the checkpoint line-limit boundary; no new source file or
  unchecked path was used.
- The final current-PA failure set is exactly the clean checkpoint set minus
  `spec/400-function-type-pack-template-argument.t`; there are no current-only
  regressions.  PA23 is 285/396 versus the turn-start/checkpoint baseline of
  284/396, while all earlier assignments pass through PA22.

### Changes Made

- Added shared semantic qualified-path normalization and scoped selection and
  specialization identities in the PA18 collection/selection paths.
- Added registry-backed materialized-base protection to dependent owner and
  specialization replay, with a single token scan on the hot path.
- Replaced changed text-based typedef/dependency probes with AST declaration
  and template-parameter checks, and constrained concrete nested-member replay
  to an actually declared member with matching typed arity.
- Removed the helper-name dependency gate from member-owner lookup and removed
  the helper-name gate from integral specialization matching.
- Kept the changes within the existing PA18 source ownership modules and
  updated this audit and the PA23 remaining-work map.  No tests or `.ref`
  files were changed.

### Validation

- `make -C dev cppgm++` — pass.
- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'` — expected nonzero report
  status with `285 / 396` passing; 76 exit-status failures, 33 LowIR
  mismatches, and 2 inherited timeouts.
- Required prior-through test command — pass:
  `===== ALL TESTS PASSED SUCCESSFULLY! (2100 / 2100) =====`.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src` — pass
  with 13 existing warnings and no fatal size/audit issue.
- Focused PA21/PA22 witnesses and the focused PA23 owner/SFINAE cluster — all
  pass.  The two reentrant timeout witnesses remain unchanged stress inputs;
  they were not made to pass by a timeout workaround.
- The complete current-PA failure set below contains 111 fixtures.  The
  checkpoint therefore preserves earlier assignments and stays above its
  PA23 baseline while leaving the next typed deferred-SFINAE group selected in
  `pa23/plan.md`.
- After committing these cohesive audit fixes, `git status --short` is empty.

### Remaining Work Map

This is a fixture behavior map for the next implementation checkpoint, not
open checkpoint-audit debt.  The next group is the two dependent-typename
fixtures bundled with non-timeout deferred-SFINAE, detector/deleted-candidate,
cached/no-eager, and dependent-member-result cases.  The timeout pair remains
as stress coverage for the same typed query-identity path.

The full current-PA failure set is:

```text
general/100-current-specialization-member-body-cast-compare.t
general/100-default-nontype-qualified-function-lookup.t
general/100-dependent-bool-partial-static-value-storage.t
general/100-dependent-qualified-nontype-base-argument.t
general/100-direct-namespace-wins-over-using-directive.t
general/100-explicit-function-specialization-overload-parameter-match.t
general/100-explicit-specialization-out-of-class-ctor-replay.t
general/100-explicit-specialization-pointer-member-definition.t
general/100-function-type-not-pointer-partial-specialization.t
general/100-inherited-using-alias-out-of-class-specialization-member.t
general/100-intermediate-type-transform-value-nontype.t
general/100-local-qualified-argument-replay.t
general/100-member-template-specialization-return-prefers-member-call.t
general/100-nested-template-static-value-nontype-expression.t
general/100-nontype-function-parameter-adjustment.t
general/100-selected-specialization-special-member-body.t
general/100-sizeof-call-result-nontype-template-argument.t
general/100-structured-bool-boost-convertible-mpl-overload.t
general/100-using-directive-template-member-type-typedef.t
general/200-adl-explicit-template-id-call.t
general/200-class-partial-specialization-no-derived-base-deduction.t
general/200-constructor-template-parameter-shadows-instantiated-type.t
general/200-empty-pack-member-template-owner-key.t
general/200-function-template-named-parameter-sfinae.t
general/200-function-template-reference-cv-alias-partial-order.t
general/200-function-template-template-parameter-deduction.t
general/200-member-function-template-address-explicit-pack.t
general/200-member-operator-template-reference-pattern-partial-order.t
general/200-member-template-implicit-instantiation-not-overload.t
general/200-nested-template-id-partial-specialization-deduction.t
general/200-pack-not-at-end-nondeduced-bad.t
general/200-template-template-qualified-default-arg-deduction.t
general/300-array-qualified-member-type-sfinae.t
general/300-constructor-template-const-ref-enable-if-conversion.t
general/300-current-specialization-constructor-template-canonical-owner.t
general/300-decltype-member-template-callable-pack-result-cache.t
general/300-dependent-bool-base-trait-type-argument.t
general/300-hidden-friend-current-specialization-enable-if.t
general/300-qualified-explicit-template-alias-return-sfinae.t
general/400-current-specialization-display-name-member-alias.t
general/400-dependent-alias-nontype-sequence-filter.t
general/400-dependent-nontype-member-template-owner.t
general/400-elaborated-type-template-arg-false-branch.t
general/400-elaborated-type-template-arg-true-branch.t
general/400-explicit-function-template-type-arg-drops-nontype-overload.t
general/400-function-type-pack-out-of-class-constructor.t
general/400-function-type-partial-specialization-fixed-arity-over-pack.t
general/400-function-type-tail-pack-recursive-specialization.t
general/400-inherited-qualified-member-template-type.t
general/400-local-class-default-member-variable-template-nontype-type.t
general/400-member-alias-template-template-owner-argument.t
general/400-member-template-result-pack-preserves-nested-function-pointer-owner.t
general/400-member-variable-template-leaf-sfinae.t
general/400-nested-member-template-base-param-shadow-value.t
general/400-nonmember-template-compound-assignment-const-lhs.t
general/400-out-of-class-ctor-using-imported-member-template.t
general/400-out-of-class-partial-member-template-owner-parameter-alias.t
general/400-qualified-member-variable-template-class-value.t
general/400-recursive-member-template-concrete-owner.t
general/400-static-cast-rvalue-ref-skips-conversion-operator.t
general/400-unused-static-member-template-return-type.t
general/400-variable-template-specializations.t
general/500-array-type-argument-sfinae-static-value.t
general/500-const-reference-alias-default-sfinae.t
general/500-constructor-sfinae-owner-pack-function-type.t
general/500-dependent-function-type-pack-expansion-ctor-init.t
general/500-dependent-qualified-member-template-result-bool.t
general/500-dependent-qualified-sizeof-static-member.t
general/500-dependent-std-or-enable-if-defers.t
general/500-dependent-typename-enable-if-candidate.t
general/500-dependent-typename-member-enable-if-return.t
general/500-member-template-conditional-alias-trailing-return.t
general/500-mp11-append-alias-template-sfinae.t
general/500-nontype-alias-reinstantiation-structural-state.t
general/500-out-of-class-member-template-dependent-owner-type.t
general/500-recursive-qualified-member-template-bool-arg.t
general/500-reentrant-static-query-callable-enable-if-cache.t
general/500-reentrant-static-query-enable-if-partial.t
general/500-source-namespace-base-sfinae-chain.t
general/500-type-pack-rejects-value-pack-bad.t
spec/100-dependent-template-id-qualified-member-source-owner.t
spec/100-explicit-instantiation-after-explicit-specialization-no-effect.t
spec/100-explicit-instantiation-class-prior-member-definitions.t
spec/100-explicit-specialization-out-of-class-ctor-replay.t
spec/100-explicit-specialization-out-of-class-member-emits.t
spec/100-extern-template-member-function-declaration.t
spec/100-extern-template-static-data-declaration.t
spec/100-function-template-nontype-function-pointer-call.t
spec/100-function-template-nontype-function-pointer-specialization-call.t
spec/100-local-member-call-constructor-template-instantiation.t
spec/100-nontype-function-pointer-argument.t
spec/100-out-of-class-conversion-operator-definition.t
spec/100-partial-specialization-member-primary-param-name.t
spec/100-sizeof-union-type-nttp.t
spec/200-defaulted-class-template-argument-pack-prefix-deduction.t
spec/200-inherited-template-param-shadow-forward.t
spec/300-constructor-default-pack-partial-ordering.t
spec/300-deleted-function-template-callee-detector-sfinae.t
spec/300-deleted-function-template-expression-sfinae.t
spec/400-class-template-nttp-scope-value.t
spec/400-conversion-function-template-call-argument.t
spec/400-conversion-function-template-copy-init.t
spec/400-conversion-function-template-selection.t
spec/400-defaulted-nested-class-argument-partial-specialization.t
spec/400-defaulted-template-arg-partial-base-completion.t
spec/400-dependent-member-template-call.t
spec/400-explicit-pack-type-argument-uses-bound-type.t
spec/400-explicit-type-arg-dependent-qualified-member-template-id.t
spec/400-qualified-member-template-id-bool-constant.t
spec/400-template-template-member-alias-owner-shadow.t
spec/500-conditional-alias-index-sequence-member-template-call.t
```

## Checkpoint 5 audit — 2026-07-29

### Scope Reviewed

- The PA23 Checkpoint 5 implementation and result in `pa23/plan.md`, the
  PA23 contract, and the required testing/reference rules.
- The deferred replay changes in `804b32b`, including candidate viability,
  deleted-function probes, dependent return constraints, pack replay, and
  generated-owner lookup.
- The uncommitted audit fixes in the PA18 collection, call, decltype,
  function-argument, member-lookup, and emission paths.

### Findings and Changes

- Candidate viability no longer discovers deleted declarations or immediate
  `enable_if` constraints by scanning a rewritten return spelling.  Those
  facts are collected once in `TemplateDefinition` and `FunctionSignature`
  state and reused by materialization and unevaluated-call lookup.
- Alias reference collapsing now consumes the collected typed
  `reference_alias_cv_parameter` fact instead of reparsing generated text.
- Generated qualified-owner normalization is no longer tied to the literal
  `std::std::` spelling, and source lexical paths remain distinct unless the
  normalized path is a registered generated specialization.  The member
  replay path no longer uses an `enable_if`/`disable_if` name gate.
- The helper implementations were moved into the existing responsibility-
  specific `.cpp` modules and the new declarations were compacted only to
  satisfy the existing file-size audit.  No source file, test, or reference
  fixture was added or modified.
- The implementation remains on the normal parser, typed template registry,
  substitution, and LowIR materialization pipeline.  No reference binary,
  host compiler, timeout acceptance path, or test-specific output path is
  used.

### Validation

- `make build` — pass.
- The six Checkpoint 5 witnesses — pass (`6 / 6`), including both dependent
  typename cases, both deleted-function detector cases, the cached callable
  pack case, and the dependent `std`/`enable_if` case.
- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'` — `292 / 396` passed;
  72 exit-status mismatches, 30 LowIR comparisons, and the same two reentrant
  static-query timeouts remain.  There are no current-only regressions against
  the clean `804b32b` checkpoint state.
- `make test-report-through-pa22` — pass (`2100 / 2100`).
- `make test-report-through-pa23` — expected current-stage report with
  `2392 / 2496` overall: the `2100 / 2100` through-PA22 gate plus the
  `292 / 396` PA23 result.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src` — pass with
  13 non-fatal repository warnings and no fatal issue.
- The post-804 failure inventory is exactly the preceding Checkpoint 4
  inventory minus these seven passing fixtures: the cached callable-pack
  result, const-reference alias default, dependent `std`/`enable_if`, the two
  dependent typename cases, and the two deleted-function detector cases.

### Remaining Work Map

The remaining 104 fixtures stay grouped as follows:

- qualified owner/member replay and generated declaration/materialization;
- typed integral, boolean, `sizeof`, variable/member-template, pointer/
  reference, and pack-kind values;
- deduction, overload ranking, conversion, ADL, template-template, and
  non-deduced-context composition;
- explicit/extern specialization, constructor replay, function pointers, and
  downstream LowIR metadata; and
- the two reentrant static-query witnesses, which still require a semantic
  query fixed point rather than timeout-specific handling.

The next implementation checkpoint is the qualified owner/member replay
group bundled with typed non-type values, using one registered owner identity
across inherited, out-of-class, alias-owner, and current-specialization
queries before taking the remaining deduction/materialization comparisons.

## Checkpoint 9 audit — 2026-07-29

### Scope Reviewed

- The latest `Checkpoint Scope`, result, validation, and next-group plan in
  `pa23/plan.md`, together with the PA23 contract in `pa23/README.md` and the
  repository testing/reference rules.
- The latest checkpoint commit `a24fe8a` and its replay predecessors
  `ab390d2`, `d363461`, `0ad8be2`, and `804b32b`; the 25-file HEAD delta,
  including the new PA14 member-lookup and PA18 collection/rewrite modules.
- The full primary report at
  `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`, the
  focused typed replay witnesses, and the complete current PA23 failure set.

### Findings

- The checkpoint stays on the normal parser, typed semantic collection,
  substitution/replay, and LowIR pipeline.  There is no skipped phase, dummy
  or embedded output, interpreter/VM/trampoline substitute, reference/host
  compiler invocation, or source/test-specific acceptance gate.  The two
  reentrant fixtures remain genuine timeout failures; no timeout cap, retry,
  or timeout-only success path was added.
- Template parameter kinds, typed integral values, AST declaration facts,
  `TypePtr` specialization arguments, and the existing generated-specializa-
  tion registry remain the semantic sources of truth.  The PA14 fallback is
  bounded to typed `TypePtr` specialization candidates indexed by primary
  name; it does not recover success by scanning emitted output.  The generated
  namespace ordering pass only orders source AST nodes and now caches each
  generated node's spelling once per local pass.
- The audit found two checkpoint-level performance hazards.  `HasStaticMember`
  rescanned the complete specialization and class-declaration registries for
  each inherited static query, and its recursion guard was inserted only after
  dependent owner recovery.  Generated namespace ordering also recomputed
  `SpellNode` inside a namespace/class cross-scan.  Both were avoidable hot-
  path recomputations; they were fixed below.
- Dependent nested-class materialization is intentionally retained.  Removing
  it causes the focused function-type-tail-pack recursive-specialization case
  to regress, so this is semantic replay needed to form a typed owner, not a
  timeout workaround or a fallback success path.
- No duplicated ownership, embedded payload, unchecked source fragment, or
  file-audit bypass was found in the checkpoint.  The new implementation
  files are in `dev/frontend_source_sets.mk`, and no tests or reference
  fixtures were changed.

### Changes Made

- In `dev/src/pa18_templates_collection_static.cpp`, static-owner recovery now
  uses `specialization_names_by_base_` instead of scanning every registered
  specialization, resolves the generated declaration through its exact typed
  owner/lexical path instead of a last-component declaration scan, and guards
  recursion by `(owner, member)` before alias recovery or materialization.
- In `dev/src/pa18_templates.cpp`, generated namespace dependency ordering
  caches each generated class/forward declaration's canonical AST spelling
  once while preserving the existing source-order decision.  This removes
  repeated AST serialization from the nested scan without changing semantic
  ordering.

### Validation

- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'` — expected nonzero current-
  stage result, **301 / 396** passed.  The complete remainder is 61 exit-status
  mismatches, 32 LowIR comparison mismatches, and 2 genuine timeouts.  The
  result remains at the turn-start baseline and above the Checkpoint 9 baseline
  of 300; the tail-pack witness stayed passing after both audit fixes.
- Focused post-fix replay/materialization checks — pass (`3 / 3`).
- The required prior-through command (`make test-report-through-pa22`) — pass
  (`2100 / 2100`).
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src` — pass with
  13 existing non-fatal repository warnings; no new file-audit warning or
  unchecked implementation path was introduced.
- `git diff --check` — pass.  No test or `.ref` file changed.

### Remaining Work Map

The complete current-PA remainder is 63 fixtures: 61 status mismatches, 2
timeouts, and 32 LowIR comparisons.  The exact inventory is grouped below so
the next checkpoint can be selected from the whole failure set rather than a
focused subset.

Status mismatches:

```text
general/100-direct-namespace-wins-over-using-directive.t
general/100-explicit-function-specialization-overload-parameter-match.t
general/100-function-type-not-pointer-partial-specialization.t
general/100-local-qualified-argument-replay.t
general/100-member-template-specialization-return-prefers-member-call.t
general/100-nontype-function-parameter-adjustment.t
general/100-selected-specialization-special-member-body.t
general/100-structured-bool-boost-convertible-mpl-overload.t
general/100-using-directive-template-member-type-typedef.t
general/200-class-partial-specialization-no-derived-base-deduction.t
general/200-constructor-template-parameter-shadows-instantiated-type.t
general/200-empty-pack-member-template-owner-key.t
general/200-function-template-named-parameter-sfinae.t
general/200-member-function-template-address-explicit-pack.t
general/200-member-template-implicit-instantiation-not-overload.t
general/200-nested-template-id-partial-specialization-deduction.t
general/200-template-template-qualified-default-arg-deduction.t
general/300-array-qualified-member-type-sfinae.t
general/300-constructor-template-const-ref-enable-if-conversion.t
general/300-hidden-friend-current-specialization-enable-if.t
general/300-qualified-explicit-template-alias-return-sfinae.t
general/400-current-specialization-display-name-member-alias.t
general/400-dependent-alias-nontype-sequence-filter.t
general/400-dependent-nontype-member-template-owner.t
general/400-elaborated-type-template-arg-false-branch.t
general/400-elaborated-type-template-arg-true-branch.t
general/400-function-type-partial-specialization-fixed-arity-over-pack.t
general/400-inherited-qualified-member-template-type.t
general/400-local-class-default-member-variable-template-nontype-type.t
general/400-member-alias-template-template-owner-argument.t
general/400-member-template-result-pack-preserves-nested-function-pointer-owner.t
general/400-nested-member-template-base-param-shadow-value.t
general/400-out-of-class-ctor-using-imported-member-template.t
general/400-static-cast-rvalue-ref-skips-conversion-operator.t
general/400-unused-static-member-template-return-type.t
general/500-array-type-argument-sfinae-static-value.t
general/500-dependent-function-type-pack-expansion-ctor-init.t
general/500-dependent-qualified-member-template-result-bool.t
general/500-member-template-conditional-alias-trailing-return.t
general/500-mp11-append-alias-template-sfinae.t
general/500-nontype-alias-reinstantiation-structural-state.t
general/500-out-of-class-member-template-dependent-owner-type.t
general/500-recursive-qualified-member-template-bool-arg.t
general/500-source-namespace-base-sfinae-chain.t
general/500-type-pack-rejects-value-pack-bad.t
spec/100-dependent-template-id-qualified-member-source-owner.t
spec/100-extern-template-member-function-declaration.t
spec/100-extern-template-static-data-declaration.t
spec/100-function-template-nontype-function-pointer-call.t
spec/100-function-template-nontype-function-pointer-specialization-call.t
spec/100-local-member-call-constructor-template-instantiation.t
spec/100-nontype-function-pointer-argument.t
spec/200-inherited-template-param-shadow-forward.t
spec/400-conversion-function-template-call-argument.t
spec/400-conversion-function-template-copy-init.t
spec/400-conversion-function-template-selection.t
spec/400-defaulted-nested-class-argument-partial-specialization.t
spec/400-explicit-pack-type-argument-uses-bound-type.t
spec/400-explicit-type-arg-dependent-qualified-member-template-id.t
spec/400-qualified-member-template-id-bool-constant.t
spec/500-conditional-alias-index-sequence-member-template-call.t
```

Timeouts:

```text
general/500-reentrant-static-query-callable-enable-if-cache.t
general/500-reentrant-static-query-enable-if-partial.t
```

LowIR comparison mismatches:

```text
general/100-current-specialization-member-body-cast-compare.t
general/100-dependent-bool-partial-static-value-storage.t
general/100-dependent-qualified-nontype-base-argument.t
general/100-explicit-specialization-out-of-class-ctor-replay.t
general/100-explicit-specialization-pointer-member-definition.t
general/100-inherited-using-alias-out-of-class-specialization-member.t
general/100-intermediate-type-transform-value-nontype.t
general/100-sizeof-call-result-nontype-template-argument.t
general/200-adl-explicit-template-id-call.t
general/200-function-template-reference-cv-alias-partial-order.t
general/200-function-template-template-parameter-deduction.t
general/200-member-operator-template-reference-pattern-partial-order.t
general/300-current-specialization-constructor-template-canonical-owner.t
general/300-dependent-bool-base-trait-type-argument.t
general/400-explicit-function-template-type-arg-drops-nontype-overload.t
general/400-function-type-pack-out-of-class-constructor.t
general/400-member-variable-template-leaf-sfinae.t
general/400-nonmember-template-compound-assignment-const-lhs.t
general/400-out-of-class-partial-member-template-owner-parameter-alias.t
general/400-variable-template-specializations.t
spec/100-explicit-instantiation-after-explicit-specialization-no-effect.t
spec/100-explicit-instantiation-class-prior-member-definitions.t
spec/100-explicit-specialization-out-of-class-ctor-replay.t
spec/100-explicit-specialization-out-of-class-member-emits.t
spec/100-out-of-class-conversion-operator-definition.t
spec/100-partial-specialization-member-primary-param-name.t
spec/100-sizeof-union-type-nttp.t
spec/200-defaulted-class-template-argument-pack-prefix-deduction.t
spec/300-constructor-default-pack-partial-ordering.t
spec/400-class-template-nttp-scope-value.t
spec/400-defaulted-template-arg-partial-base-completion.t
spec/400-template-template-member-alias-owner-shadow.t
```

Group the next implementation checkpoint around qualified owner/member replay
and candidate-local deferral, bundled with the typed variable-template,
`sizeof`, alias, and pack consumers.  That group covers the largest status
cluster and several small typed-value clusters; it should establish one
concrete owner identity before the remaining deduction/overload and
specialization/constructor LowIR groups.  Keep the two reentrant cases as
fixed-point witnesses, not timeout-specific work.

## Checkpoint 10 audit — 2026-07-29

### Scope Reviewed

- The latest `Checkpoint Scope`, result, validation, and next-group plan in
  `pa23/plan.md`, the PA23 contract in `pa23/README.md`, and the repository
  testing/reference rules.
- The landed checkpoint commits `bb4b8d1` (`Implement PA23 nested template
  owner replay`) and `c48a448` (`Audit PA23 checkpoint lookup paths`), their
  relevant parent changes, and the changed source set in `dev/src`.
- The full primary report at
  `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`, the
  focused owner/value witnesses, the required through-PA22 report, and the
  complete current PA23 failure inventory.

### Findings

- The checkpoint remains on the normal parser, typed semantic collection,
  substitution/replay, and LowIR pipeline.  No compiler phase is skipped, and
  there is no dummy output, embedded payload, interpreter/VM/trampoline
  substitute, reference-binary or host-compiler invocation, or
  source/test-specific acceptance gate.
- The two reentrant fixtures still time out as genuine semantic fixed-point
  stress cases.  No timeout cap, retry loop, timeout-only success path, or
  other timing workaround was added; they remain reported as timeouts.
- The landed immediate template-argument validation used stringly facts: a
  qualified member ending in names such as `value`, `size`, `length`, or
  `count` was treated as a value, while a suffix of `::type` was treated as a
  type.  That can misclassify a user-defined type member or value member.  This
  was a checkpoint-level semantic blocker and is fixed below with typed
  registries.
- The validation entry point also repeated the same template-argument scan:
  the outer AST walk classified arguments and then called the recursive helper
  that performed the walk again.  This was an avoidable repeated full-string
  scan in a semantic hot path and is now one walk.
- Nested owner replay preserves the enclosing typed bindings while retaining
  the nested declaration’s own parameters and dependent expressions until its
  own template-id is supplied.  The generated owner and pack facts remain
  represented in the existing typed registries; downstream code does not
  recover them by reparsing emitted text.
- The final validation path consults direct typed class, alias, named-type,
  and static-member indexes.  It does not call the side-effectful inherited
  static-member materialization query merely to classify an argument, so
  validation cannot introduce reentrant state mutation.  The common path does
  not add a registry-wide scan or repeated full-suite walk.
- No duplicated ownership, hidden implementation fragment, weakened check,
  unchecked source path, or file-audit bypass was found.  The validation
  implementation is in the checked-in frontend source set, and no test or
  reference fixture was changed.

### Changes Made

- `dev/src/pa18_templates_collection.cpp`: removed the old context-free
  `ValidationTypeArgument` body after moving validation ownership to the
  validation module.
- `dev/src/pa18_templates_collection.h`: made template-argument validation
  context-aware and declared the typed member lookup helper.
- `dev/src/pa18_templates_validation.cpp`:
  - replaced member-name suffix heuristics with `type_aliases_`,
    `class_declarations_`, `named_type_contexts_`,
    `static_members_by_class_`, and `TemplateDefinition::static_members`;
  - checks direct typed static-member indexes before contextual definition
    lookup, avoiding unnecessary work on the common path;
  - preserves the existing conservative handling of literals, casts,
    `sizeof`/`alignof`, dependent parameters, and `decltype`; and
  - removes the duplicate outer argument-kind walk so each node is validated
    through `ValidateTemplateArgumentSpelling` once.

### Validation

- `make -C dev cppgm++` — pass.
- Focused PA23 owner/value checks — pass, **4/4**:
  `400-inherited-qualified-member-template-type`,
  `400-dependent-nontype-member-template-owner`,
  `400-nested-member-template-base-param-shadow-value`, and
  `500-type-pack-rejects-value-pack-bad`.
- Required prior-through command — pass, **2100/2100**:
  `n=23; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`
- Required full current-PA report — expected nonzero because residual PA23
  fixtures remain; **304/396** passed.  This is unchanged from the turn-start
  baseline and is +3 over the checkpoint’s recorded 301/396 baseline.  The
  complete remainder is **57 status**, **33 LowIR**, and **2 timeout**
  fixtures.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src` — pass with
  13 existing non-fatal warnings and no fatal or unchecked-path finding.
- `git diff --check` — pass.  Earlier PAs pass, current PA progress is
  preserved, and no tests or `.ref` files changed.

### Remaining Work Map

The full current-PA remainder is 92 fixtures.  The status group is the next
semantic implementation surface: qualified owner/member replay and deferred
candidate selection, typed values/aliases/packs, deduction, conversion, and
specialization interactions.  The LowIR group is downstream representation
comparison coverage; the timeout group is fixed-point stress coverage.  The
exact inventory from the final report follows.

Status mismatches (57):

```text
general/100-direct-namespace-wins-over-using-directive.t
general/100-explicit-function-specialization-overload-parameter-match.t
general/100-function-type-not-pointer-partial-specialization.t
general/100-member-template-specialization-return-prefers-member-call.t
general/100-nontype-function-parameter-adjustment.t
general/100-selected-specialization-special-member-body.t
general/100-structured-bool-boost-convertible-mpl-overload.t
general/100-using-directive-template-member-type-typedef.t
general/200-class-partial-specialization-no-derived-base-deduction.t
general/200-constructor-template-parameter-shadows-instantiated-type.t
general/200-empty-pack-member-template-owner-key.t
general/200-function-template-named-parameter-sfinae.t
general/200-member-function-template-address-explicit-pack.t
general/200-member-template-implicit-instantiation-not-overload.t
general/200-nested-template-id-partial-specialization-deduction.t
general/200-template-template-qualified-default-arg-deduction.t
general/300-array-qualified-member-type-sfinae.t
general/300-constructor-template-const-ref-enable-if-conversion.t
general/300-hidden-friend-current-specialization-enable-if.t
general/300-qualified-explicit-template-alias-return-sfinae.t
general/400-current-specialization-display-name-member-alias.t
general/400-dependent-alias-nontype-sequence-filter.t
general/400-elaborated-type-template-arg-false-branch.t
general/400-elaborated-type-template-arg-true-branch.t
general/400-function-type-partial-specialization-fixed-arity-over-pack.t
general/400-function-type-tail-pack-recursive-specialization.t
general/400-local-class-default-member-variable-template-nontype-type.t
general/400-member-alias-template-template-owner-argument.t
general/400-member-template-result-pack-preserves-nested-function-pointer-owner.t
general/400-out-of-class-ctor-using-imported-member-template.t
general/400-static-cast-rvalue-ref-skips-conversion-operator.t
general/400-unused-static-member-template-return-type.t
general/500-array-type-argument-sfinae-static-value.t
general/500-dependent-function-type-pack-expansion-ctor-init.t
general/500-dependent-qualified-member-template-result-bool.t
general/500-member-template-conditional-alias-trailing-return.t
general/500-mp11-append-alias-template-sfinae.t
general/500-nontype-alias-reinstantiation-structural-state.t
general/500-out-of-class-member-template-dependent-owner-type.t
general/500-recursive-qualified-member-template-bool-arg.t
general/500-source-namespace-base-sfinae-chain.t
spec/100-dependent-template-id-qualified-member-source-owner.t
spec/100-extern-template-member-function-declaration.t
spec/100-extern-template-static-data-declaration.t
spec/100-function-template-nontype-function-pointer-call.t
spec/100-function-template-nontype-function-pointer-specialization-call.t
spec/100-local-member-call-constructor-template-instantiation.t
spec/100-nontype-function-pointer-argument.t
spec/200-inherited-template-param-shadow-forward.t
spec/400-conversion-function-template-call-argument.t
spec/400-conversion-function-template-copy-init.t
spec/400-conversion-function-template-selection.t
spec/400-defaulted-nested-class-argument-partial-specialization.t
spec/400-explicit-pack-type-argument-uses-bound-type.t
spec/400-explicit-type-arg-dependent-qualified-member-template-id.t
spec/400-qualified-member-template-id-bool-constant.t
spec/500-conditional-alias-index-sequence-member-template-call.t
```

LowIR comparisons (33):

```text
general/100-current-specialization-member-body-cast-compare.t
general/100-dependent-bool-partial-static-value-storage.t
general/100-dependent-qualified-nontype-base-argument.t
general/100-explicit-specialization-out-of-class-ctor-replay.t
general/100-explicit-specialization-pointer-member-definition.t
general/100-inherited-using-alias-out-of-class-specialization-member.t
general/100-intermediate-type-transform-value-nontype.t
general/100-local-qualified-argument-replay.t
general/100-sizeof-call-result-nontype-template-argument.t
general/200-adl-explicit-template-id-call.t
general/200-function-template-reference-cv-alias-partial-order.t
general/200-function-template-template-parameter-deduction.t
general/200-member-operator-template-reference-pattern-partial-order.t
general/300-current-specialization-constructor-template-canonical-owner.t
general/300-dependent-bool-base-trait-type-argument.t
general/400-explicit-function-template-type-arg-drops-nontype-overload.t
general/400-function-type-pack-out-of-class-constructor.t
general/400-member-variable-template-leaf-sfinae.t
general/400-nonmember-template-compound-assignment-const-lhs.t
general/400-out-of-class-partial-member-template-owner-parameter-alias.t
general/400-variable-template-specializations.t
spec/100-explicit-instantiation-after-explicit-specialization-no-effect.t
spec/100-explicit-instantiation-class-prior-member-definitions.t
spec/100-explicit-specialization-out-of-class-ctor-replay.t
spec/100-explicit-specialization-out-of-class-member-emits.t
spec/100-out-of-class-conversion-operator-definition.t
spec/100-partial-specialization-member-primary-param-name.t
spec/100-sizeof-union-type-nttp.t
spec/200-defaulted-class-template-argument-pack-prefix-deduction.t
spec/300-constructor-default-pack-partial-ordering.t
spec/400-class-template-nttp-scope-value.t
spec/400-defaulted-template-arg-partial-base-completion.t
spec/400-template-template-member-alias-owner-shadow.t
```

Timeouts (2):

```text
general/500-reentrant-static-query-callable-enable-if-cache.t
general/500-reentrant-static-query-enable-if-partial.t
```

### Next Substantial Checkpoint Group

Bundle qualified owner/member replay with candidate-local deferred SFINAE,
then carry the same typed owner identity through variable-template,
`sizeof`, alias, and pack consumers.  This is the largest connected status
cluster and the most useful next checkpoint boundary; deduction/conversion and
specialization/constructor LowIR groups follow it.  The reentrant cases stay
as fixed-point witnesses and are not handled by timing-specific acceptance.

## Checkpoint 11 audit — 2026-07-30

### Scope Reviewed

- The latest `Checkpoint 11 scope`, result, validation, and next-group notes in
  `pa23/plan.md`, together with the PA23 contract in `pa23/README.md` and the
  repository testing/reference rules.
- The landed owner-replay commits `7b75bb4` (`Improve PA23 materialized owner
  lookup`), `7126655` (`Audit PA23 nested owner checkpoint`), `bb4b8d1`
  (`Implement PA23 nested template owner replay`), and `c48a448` (`Audit PA23
  checkpoint lookup paths`), plus their changed `dev/src` files.
- The complete primary report at
  `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`, the
  PA21 inline-namespace witness, the PA22 hidden-friend witness, the full
  current-PA report, the required through-PA22 report, and the PA23 file audit.

### Findings

- The checkpoint stays on the normal parser, typed semantic collection,
  substitution/replay, and LowIR pipeline.  No compiler phase is skipped, and
  there is no dummy output, embedded payload, interpreter/VM/trampoline
  substitute, reference-binary or host-compiler invocation, or
  source/test-specific acceptance gate.
- The two reentrant fixtures remain genuine semantic fixed-point stress cases.
  No timeout cap, retry loop, timeout-only success path, or other timing
  workaround is present; both remain reported as timeouts.
- A real checkpoint-level performance and ownership blocker was present in
  `FindClassMemberType`: its materialized-owner fast path walked all
  `specialization_bases_` entries and then all `class_declarations_` entries,
  comparing short-name components.  That made each lookup dependent on the
  entire accumulated registry and allowed unrelated same-name declarations to
  participate in owner recovery.
- The final implementation removes those registry-wide walks without changing
  their deterministic selection semantics.  Specialization names are narrowed
  through a typed base index, and declaration paths are narrowed through the
  existing typed class-path index.  The sorted name/path selection preserves
  the prior map-order behavior; no emitted text is reparsed to recover
  ownership, and no ownership fact is duplicated downstream.
- The new index is maintained at specialization registration time, so the hot
  lookup path does not copy or sort a vector on every query.  It performs only
  candidate-local argument checks and indexed path checks.  No full-suite walk,
  fallback-success shortcut, weakened check, hidden implementation fragment,
  or unchecked source path was introduced.

### Changes Made

- `dev/src/pa18_templates_collection.h` adds the sorted typed specialization
  name index while keeping the header at its 1200-line audit limit.
- `dev/src/pa18_templates_rewrite_instantiate.cpp` populates that index when a
  generated specialization is registered.
- `dev/src/pa18_templates_rewrite_members.cpp` replaces the two full-map owner
  scans with the typed specialization-name and class-path indexes, preserving
  the existing exact argument matching and resolver fallback.

### Validation

- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'` — **306/396** passed; the
  complete residual set is **90** fixtures (55 ordinary status failures, 33
  LowIR comparisons, and 2 timeouts).  This is at the turn-start baseline and
  preserves the checkpoint's +2 improvement over its 304/396 pre-checkpoint
  baseline.
- The PA21 inline-namespace and PA22 hidden-friend witnesses both pass.
- Required prior-through check — **2100/2100**:
  `n=23; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src` — pass with
  13 pre-existing non-fatal warnings; no size fatal, unchecked path, or
  weakened audit check remains.
- `git diff --check` — pass.  No tests or reference fixtures were changed.

### Remaining Work Map

The complete current-PA failure set from the final report is below.  `STATUS`
means an ordinary exit-status mismatch; the two timeout cases are listed
separately because they require fixed-point semantic work rather than timing
handling.

Status mismatches (55):

```text
general/100-direct-namespace-wins-over-using-directive.t
general/100-explicit-function-specialization-overload-parameter-match.t
general/100-function-type-not-pointer-partial-specialization.t
general/100-member-template-specialization-return-prefers-member-call.t
general/100-nontype-function-parameter-adjustment.t
general/100-selected-specialization-special-member-body.t
general/100-structured-bool-boost-convertible-mpl-overload.t
general/200-class-partial-specialization-no-derived-base-deduction.t
general/200-constructor-template-parameter-shadows-instantiated-type.t
general/200-empty-pack-member-template-owner-key.t
general/200-function-template-named-parameter-sfinae.t
general/200-member-function-template-address-explicit-pack.t
general/200-member-template-implicit-instantiation-not-overload.t
general/200-nested-template-id-partial-specialization-deduction.t
general/200-template-template-qualified-default-arg-deduction.t
general/300-array-qualified-member-type-sfinae.t
general/300-constructor-template-const-ref-enable-if-conversion.t
general/300-hidden-friend-current-specialization-enable-if.t
general/300-qualified-explicit-template-alias-return-sfinae.t
general/400-current-specialization-display-name-member-alias.t
general/400-dependent-alias-nontype-sequence-filter.t
general/400-elaborated-type-template-arg-false-branch.t
general/400-elaborated-type-template-arg-true-branch.t
general/400-function-type-partial-specialization-fixed-arity-over-pack.t
general/400-function-type-tail-pack-recursive-specialization.t
general/400-local-class-default-member-variable-template-nontype-type.t
general/400-member-alias-template-template-owner-argument.t
general/400-member-template-result-pack-preserves-nested-function-pointer-owner.t
general/400-out-of-class-ctor-using-imported-member-template.t
general/400-static-cast-rvalue-ref-skips-conversion-operator.t
general/400-unused-static-member-template-return-type.t
general/500-array-type-argument-sfinae-static-value.t
general/500-dependent-function-type-pack-expansion-ctor-init.t
general/500-dependent-qualified-member-template-result-bool.t
general/500-member-template-conditional-alias-trailing-return.t
general/500-mp11-append-alias-template-sfinae.t
general/500-nontype-alias-reinstantiation-structural-state.t
general/500-out-of-class-member-template-dependent-owner-type.t
general/500-recursive-qualified-member-template-bool-arg.t
general/500-source-namespace-base-sfinae-chain.t
spec/100-extern-template-member-function-declaration.t
spec/100-extern-template-static-data-declaration.t
spec/100-function-template-nontype-function-pointer-call.t
spec/100-function-template-nontype-function-pointer-specialization-call.t
spec/100-local-member-call-constructor-template-instantiation.t
spec/100-nontype-function-pointer-argument.t
spec/200-inherited-template-param-shadow-forward.t
spec/400-conversion-function-template-call-argument.t
spec/400-conversion-function-template-copy-init.t
spec/400-conversion-function-template-selection.t
spec/400-defaulted-nested-class-argument-partial-specialization.t
spec/400-explicit-pack-type-argument-uses-bound-type.t
spec/400-explicit-type-arg-dependent-qualified-member-template-id.t
spec/400-qualified-member-template-id-bool-constant.t
spec/500-conditional-alias-index-sequence-member-template-call.t
```

LowIR comparisons (33):

```text
general/100-current-specialization-member-body-cast-compare.t
general/100-dependent-bool-partial-static-value-storage.t
general/100-dependent-qualified-nontype-base-argument.t
general/100-explicit-specialization-out-of-class-ctor-replay.t
general/100-explicit-specialization-pointer-member-definition.t
general/100-inherited-using-alias-out-of-class-specialization-member.t
general/100-intermediate-type-transform-value-nontype.t
general/100-local-qualified-argument-replay.t
general/100-sizeof-call-result-nontype-template-argument.t
general/200-adl-explicit-template-id-call.t
general/200-function-template-reference-cv-alias-partial-order.t
general/200-function-template-template-parameter-deduction.t
general/200-member-operator-template-reference-pattern-partial-order.t
general/300-current-specialization-constructor-template-canonical-owner.t
general/300-dependent-bool-base-trait-type-argument.t
general/400-explicit-function-template-type-arg-drops-nontype-overload.t
general/400-function-type-pack-out-of-class-constructor.t
general/400-member-variable-template-leaf-sfinae.t
general/400-nonmember-template-compound-assignment-const-lhs.t
general/400-out-of-class-partial-member-template-owner-parameter-alias.t
general/400-variable-template-specializations.t
spec/100-explicit-instantiation-after-explicit-specialization-no-effect.t
spec/100-explicit-instantiation-class-prior-member-definitions.t
spec/100-explicit-specialization-out-of-class-ctor-replay.t
spec/100-explicit-specialization-out-of-class-member-emits.t
spec/100-out-of-class-conversion-operator-definition.t
spec/100-partial-specialization-member-primary-param-name.t
spec/100-sizeof-union-type-nttp.t
spec/200-defaulted-class-template-argument-pack-prefix-deduction.t
spec/300-constructor-default-pack-partial-ordering.t
spec/400-class-template-nttp-scope-value.t
spec/400-defaulted-template-arg-partial-base-completion.t
spec/400-template-template-member-alias-owner-shadow.t
```

Timeouts (2):

```text
general/500-reentrant-static-query-callable-enable-if-cache.t
general/500-reentrant-static-query-enable-if-partial.t
```

### Next Substantial Checkpoint Group

Bundle qualified owner/member replay with candidate-local deferred SFINAE,
then carry the same typed owner identity through variable-template,
`sizeof`, alias, and pack consumers.  This is the largest connected status
cluster; deduction/conversion and specialization/constructor LowIR groups
follow it.  Keep the two reentrant cases as fixed-point witnesses, with no
timeout-specific acceptance logic.
