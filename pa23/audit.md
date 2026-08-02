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
  `FindClassMemberType`: its materialized-owner path and its adjacent generated
  suffix/nested-owner recovery walked all `specialization_bases_` entries and
  then all `class_declarations_` entries, comparing short-name components.
  That made each recursive lookup dependent on the entire accumulated registry
  and allowed unrelated same-name declarations to participate in owner
  recovery.
- The final implementation removes those registry-wide walks without changing
  their deterministic selection semantics.  Materialized names are narrowed
  through a typed base index, generated-name recovery uses the direct typed
  specialization key, and declaration paths are narrowed through the existing
  typed class-path index.  The indexed candidate selection preserves the prior
  map-order behavior; no emitted text is reparsed to recover ownership, and no
  ownership fact is duplicated downstream.
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
- `dev/src/pa18_templates_rewrite_members.cpp` replaces the materialized-owner,
  generated-suffix, and nested-owner full-map scans with the typed
  specialization-name and class-path indexes, preserving exact argument
  matching, deterministic path selection, and the resolver fallback.

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

## Checkpoint 16 audit — 2026-07-30

### Scope Reviewed

- The latest Checkpoint 16 scope, result, Remaining Work Map, and next
  checkpoint in `pa23/plan.md`, together with the PA23 contract in
  `pa23/README.md` and `TESTING_AND_REFERENCES.md`.
- The landed checkpoint commits `150da1c` (dependent-owner replay),
  `96e611a` (function-template deduction), `1f525a8` (function-type pack
  replay), `e3fd972` (dependent replay), and `8162530` (owner/dependent
  lookup), plus the preceding owner-index audit commits.
- The changed source paths under `dev/src`, especially anonymous-namespace
  scope predeclaration, `Analyzer` path resolution, class-template selection
  and matching, dependent member replay, type qualification, constructor
  lowering, and LowIR materialization.
- The authoritative current-stage log at
  `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`, the
  six intended owner/class-partial witnesses, the exact prior-through command,
  and the PA23 file audit.

### Findings

- The checkpoint remains on the required parser, typed semantic collection,
  substitution/replay, and LowIR lowering pipeline.  The audit found no
  skipped phase, dummy or embedded output, interpreter/VM/trampoline
  substitute, reference-binary or host-compiler invocation, fixture-name gate,
  or source-specific acceptance path.
- The two reentrant static-query cases are still genuine semantic fixed-point
  failures.  No timeout cap, retry, timeout-only success path, or timing-based
  acceptance logic was added; they remain visible as the two timeout witnesses
  in the current report.
- The new `HasDeferredTypeMember` path duplicated the existing dependent-member
  walk and rescanned a complete declaration tree on every cached class
  materialization.  Its `LastComponent(...)=type` test also recovered a
  semantic fact late from AST spelling.  This was a real hot-path ownership
  and recomputation blocker, not merely a formatting concern.
- Generated anonymous-namespace predeclaration scanned the entire accumulated
  `namespace_scopes_` registry for every generated anonymous namespace.  The
  lookup was candidate-independent and could become quadratic as replay added
  scopes.  The checkpoint also expanded path resolution inside a large header,
  which crossed the fatal 1200-line file-audit limit when made readable.
- The dependent-base constructor branch was traced as a possible fallback
  success path.  It is keyed by the existing typed `Type::dependent_base_lookup`
  fact, preserves demand for concrete member constructors, and does not turn a
  failed lookup into a successful translation.  Removing it causes two valid
  checkpoint LowIR regressions, so it remains a typed deferral in the ordinary
  lowering path rather than a timeout or test acceptance workaround.

### Changes Made

- Added `TemplateDefinition::dependent_member_type_nodes` together with the
  separately owned `dependent_type_member_nodes` subset, indexing both the
  relevant AST type nodes and the `::type` member fact once during template
  registration.  Both deferred member checks now consume those owned indexes,
  so materialization performs only candidate-local substitution checks and does
  not repeatedly walk the whole declaration or reparse emitted text.
- Moved `Analyzer::SplitPath`, `ResolvePath`, `ResolveNamespace`, and
  `ResolveBinding` into the existing `pa11_semantics_analyzer_resolve.cpp`
  responsibility module.  The anonymous-namespace fallback remains typed on
  `Scope*` candidates and the header is no longer kept under its size limit by
  compressed implementation lines.
- Replaced the registry-wide anonymous-scope scan with a local typed
  `set<Scope*>` of synthetic anonymous scopes.  This preserves the generated
  namespace identity while bounding each predeclaration decision to the
  current scope's children.
- Kept all audit changes in existing responsibility-named files.  No test,
  `.ref`, reference binary, unchecked include fragment, or weakened audit rule
  was added or modified.

### Validation

- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'` — **325/396** passed:
  38 ordinary exit-status mismatches, 2 reentrant static-query timeouts, and
  31 LowIR comparisons.  This equals the checkpoint baseline and introduces no
  current-PA regression.
- The exact required prior-through command — **pass, 2100/2100** through PA22:
  `n=23; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`.
- The six checkpoint owner/class-partial preservation witnesses — **pass,
  6/6** — including class partial matching, nested template-id deduction,
  source-namespace base SFINAE, dependent qualified base arguments, the
  out-of-class dependent-owner case, and inherited template-parameter shadowing.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src` — **pass**
  with the same 13 non-fatal repository warnings and no fatal size, hidden
  implementation, compressed-line, or unchecked-path finding.
- `git diff --check` — pass.  The current-PA count remains at or above the
  turn-start/checkpoint baseline while earlier assignments remain green.

### Complete Current-PA Failure Inventory

The refreshed inventory below is the complete 71-fixture set from the final
current-stage report.  The two timeout witnesses are separated from the 38
ordinary status mismatches; the remaining 31 are LowIR comparisons.

Ordinary status mismatches (38):

```text
general/100-intermediate-type-transform-value-nontype.t
general/100-member-template-specialization-return-prefers-member-call.t
general/100-selected-specialization-special-member-body.t
general/100-structured-bool-boost-convertible-mpl-overload.t
general/200-constructor-template-parameter-shadows-instantiated-type.t
general/200-function-template-named-parameter-sfinae.t
general/200-member-function-template-address-explicit-pack.t
general/200-member-template-implicit-instantiation-not-overload.t
general/200-template-template-qualified-default-arg-deduction.t
general/300-constructor-template-const-ref-enable-if-conversion.t
general/400-anonymous-namespace-partial-specialization.t
general/400-current-specialization-display-name-member-alias.t
general/400-elaborated-type-template-arg-false-branch.t
general/400-elaborated-type-template-arg-true-branch.t
general/400-local-class-default-member-variable-template-nontype-type.t
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
general/500-recursive-qualified-member-template-bool-arg.t
spec/100-extern-template-member-function-declaration.t
spec/100-extern-template-static-data-declaration.t
spec/100-local-member-call-constructor-template-instantiation.t
spec/400-conversion-function-template-call-argument.t
spec/400-conversion-function-template-copy-init.t
spec/400-conversion-function-template-selection.t
spec/400-defaulted-nested-class-argument-partial-specialization.t
spec/400-explicit-pack-type-argument-uses-bound-type.t
spec/400-explicit-type-arg-dependent-qualified-member-template-id.t
spec/400-function-type-pack-template-argument.t
spec/400-qualified-member-template-id-bool-constant.t
spec/500-conditional-alias-index-sequence-member-template-call.t
```

Timeout witnesses (2):

```text
general/500-reentrant-static-query-callable-enable-if-cache.t
general/500-reentrant-static-query-enable-if-partial.t
```

LowIR comparisons (31):

```text
general/100-current-specialization-member-body-cast-compare.t
general/100-dependent-bool-partial-static-value-storage.t
general/100-explicit-specialization-out-of-class-ctor-replay.t
general/100-explicit-specialization-pointer-member-definition.t
general/100-inherited-using-alias-out-of-class-specialization-member.t
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

### Next Substantial Checkpoint Group

Take the typed-value/deferred-SFINAE cluster next: non-type storage,
dependent `sizeof`, array/static-value probes, detector and alias SFINAE, and
dependent member-template result evaluation. Keep the two reentrant cases as
fixed-point witnesses, with no timeout-specific acceptance logic, and retain
the six owner/class-partial witnesses as regression coverage. Deduction and
conversion viability plus explicit/extern and LowIR materialization remain the
following bundled groups.

## Checkpoint 17 audit — 2026-07-30

### Scope Reviewed

- The latest Checkpoint 17 scope, result, complete failure grouping, and next
  checkpoint in `pa23/plan.md`, together with the PA23 contract in
  `pa23/README.md` and the repository testing/reference rules.
- The landed checkpoint commit `4d7484b` and its dependent-owner, deduction,
  pack-replay, and typed-replay predecessors, with particular attention to
  `pa18_templates_collection*`, declaration replay, type qualification, and
  LowIR emission.
- The authoritative full-stage log at
  `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`, the
  focused checkpoint witnesses, the exact through-PA22 command, and the PA23
  file audit.

### Findings

- The checkpoint remains on the parser, typed semantic collection,
  substitution/replay, and LowIR pipeline.  There is no skipped compiler
  phase, dummy or embedded output, interpreter/VM/trampoline substitute,
  reference-binary or host-compiler invocation, fixture-name acceptance gate,
  or unchecked output path.  The two reentrant static-query fixtures remain
  genuine semantic fixed-point failures; no timeout cap, retry, or
  timing-based success path was added.
- The new implicit nested-owner helper rescanned each class-template AST on
  both sides of every materialization and used the class-declaration map as a
  replaceable forward cache.  That was an avoidable hot-path walk and could
  overwrite a complete nested declaration with an incomplete shell.
- The new pre-transform dependency preservation also walked the complete
  source declaration on every materialization.  The AST's type spelling is a
  syntax-boundary input, but repeatedly rediscovering its dependency facts at
  replay time was both recomputation and late ownership recovery.
- The added indexing implementation briefly crossed the 1500-line source
  limit in `pa18_templates_collection.cpp`; the file audit caught this as a
  fatal structural regression.  No audit threshold or checker rule was
  weakened.
- Comparing the complete Checkpoint 17 baseline inventory with the final
  report found no newly failing fixture.  The current stage is 329/396,
  four above the 325 checkpoint baseline: 35 exit-status mismatches,
  including the two fixed-point timeout witnesses, and 32 LowIR comparisons.

### Changes Made

- Added `TemplateTypeDependency` records and indexed declaration type
  dependencies once when each template is registered.  Pre-transform replay
  now consumes that owned index; the post-transform walk remains only for
  concrete dependencies introduced by substitution.
- Indexed implicit elaborated nested class names once in the corresponding
  `TemplateDefinition`, so materialization installs only the already-known
  names.  The syntax-level scan consumes PA10 AST spellings at registration;
  it does not reparse emitted LowIR or use fixture-specific strings.
- Preserved an existing complete `class-specifier` declaration when a later
  replay only supplies a forward shell, while still allowing a newly generated
  complete nested declaration to replace an incomplete one.
- Kept the new indexing implementation in
  `pa18_templates_collection_nested.cpp`, restoring the source-size audit
  boundary.  No tests, reference fixtures, checker rules, or external tools
  were modified or invoked by the compiler implementation.

### Validation

- `make build` — pass.
- The nine named Checkpoint 17 witnesses — pass (`9/9`).
- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'` — **329/396**; the
  complete current-stage failure set is unchanged by the audit fixes and is
  above the 325 turn-start baseline.  The full failure inventory and grouped
  Remaining Work Map are recorded in the Checkpoint 17 result in
  `pa23/plan.md`.
- Exact required prior-through command — pass, **2100/2100** through PA22:
  `n=23; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src` — pass
  with 13 non-fatal pre-existing warnings and no fatal finding.
- `git diff --check` — pass.  The next substantial group is the remaining
  dependent member-result and candidate-local SFINAE band, bundled with the
  conversion-function selection witnesses; the two reentrant cases remain
  fixed-point regression coverage only.

## Checkpoint 18 audit — 2026-07-30

### Scope Reviewed

- The latest Checkpoint 18 scope and result in `pa23/plan.md`, including its
  three ordinary conversion-function-template witnesses, the 332/396 stage
  result, and the proposed remaining-work map.
- `pa23/README.md` and `TESTING_AND_REFERENCES.md`, with their requirements
  for normal parser/semantic/template-replay/LowIR behavior, fixture-based
  validation, and prior-PA preservation.
- The checkpoint head `cc99f03` (`Implement PA23 conversion template replay`),
  its preceding typed-replay audit checkpoint `78adbf7`, and the audit diff in
  `dev/src/pa18_templates_calls_conversions.cpp`,
  `dev/src/pa18_templates_collection.cpp`,
  `dev/src/pa18_templates_collection.h`, and
  `dev/src/pa18_templates_rewrite_instantiate.h`.
- The authoritative active-stage log at
  `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`, the
  focused PA23 conversion witnesses, the qualified same-name conversion
  regression witness from PA22, the exact through-PA22 report, and the PA23
  file audit.

### Findings

- The checkpoint stays on the ordinary compiler pipeline: parsed AST input,
  typed semantic collection and lookup, template substitution/replay, and
  LowIR emission.  There is no skipped compiler phase, dummy output,
  embedded payload, interpreter/VM/trampoline substitute, reference-binary or
  host-compiler invocation, source/test-specific acceptance gate, or unchecked
  output path.  The two reentrant static-query fixtures remain genuine
  fixed-point witnesses; no timeout cap, retry, timing-based acceptance, or
  fallback success path was added.
- The checkpoint implementation had a material replay hot-path problem.
  Every ordinary conversion replay walked all `definitions_` entries, and a
  successful instantiation then walked every generated owner and node to find
  an `operator` spelling.  That made the work proportional to the complete
  template registry and generated-owner set for each conversion, and could
  append an unrelated special-member node to the source declaration.  It was
  both avoidable repeated scanning and duplicated/late ownership of generated
  facts.
- Conversion-operator status and destination spelling were being recovered
  from raw declaration/name text during replay.  Qualified out-of-class
  conversion templates whose target name resembles the source owner exposed
  an owner-path ambiguity.  This was a stringly semantic fact that belonged
  in the collected template definition, with generated-node provenance kept
  separately.
- The first audit narrowing exposed a real PA22 regression in the qualified
  same-name target case: the generated owner spelling is not always a direct
  key for the source declaration.  The through-PA22 gate caught it; indexing
  generated top-level nodes by their typed `template_primary` restored the
  witness without returning to a global owner scan.
- The file audit found no bypass, weakened check, hidden implementation path,
  or new fatal size/duplication issue.  Its 13 warnings remain the existing
  non-fatal division-header, complexity, and duplication observations; the
  changed implementation remains under the checked `dev/src` audit paths.

### Changes Made

- Added `conversion_operator` and `conversion_target` to `TemplateDefinition`.
  `RegisterTemplate` records the normalized conversion target once at the
  syntax/collection boundary, including the qualified-declarator spelling
  needed by the same-name case.  Replay now consumes this typed metadata
  instead of reparsing declaration text.
- Added `conversion_operator_definitions_by_owner_`, populated once after
  collection and using-resolution.  Ordinary conversion replay now visits
  only the owner-indexed candidates rather than rescanning the full
  definition registry.
- Added `generated_by_primary_` provenance.  `MarkGeneratedNode` indexes only
  top-level materialized declarations by their `template_primary`; recursive
  children are not duplicated into the index.  Conversion replay selects the
  exact generated member from that provenance map and attaches it only to
  the source declaration, removing the old all-owner scan and last-node
  fallback.
- Passed the already resolved source declaration into conversion replay and
  used its `template_primary` plus direct specialization-map lookups for
  source/target ownership.  This removes repeated full-map walks while
  retaining the normal `InstantiateMemberCall` and LowIR path.
- No test, reference fixture, checker, timeout policy, output format, or
  external tool was changed.  The temporary diagnostic code used while
  isolating the PA22 regression was removed before validation.

### Validation

- The compiler build passed.
- The three Checkpoint 18 PA23 witnesses passed, and the qualified PA22
  same-name conversion witness passed 1/1 after the audit caught and repaired
  its intermediate regression.
- The exact required prior-through command passed all earlier assignments:
  `2100/2100` through PA22.
- The required active report
  `make test-report ACTIVE_TEST_REPORT_PAS='pa23'` completed with the
  expected current-stage failures and reported **332/396**.  This is equal
  to the turn-start 332/396 and above the pre-Checkpoint-18 329/396
  baseline; the conversion witnesses remain passing and no earlier-PA
  fixture was lost.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src` passed with
  13 non-fatal pre-existing warnings and no fatal finding.
- `git diff --check` passed.

### Complete Current-PA Failure Inventory

The active report has 64 remaining fixtures: 32 exit-status mismatches and
32 LowIR comparisons.  These are semantic implementation work, not audit
debt; no shortcut or performance blocker remains from Checkpoint 18.

Status failures (32):

```text
general/100-member-template-specialization-return-prefers-member-call.t
general/100-selected-specialization-special-member-body.t
general/100-structured-bool-boost-convertible-mpl-overload.t
general/200-constructor-template-parameter-shadows-instantiated-type.t
general/200-function-template-named-parameter-sfinae.t
general/200-member-function-template-address-explicit-pack.t
general/200-member-template-implicit-instantiation-not-overload.t
general/200-template-template-qualified-default-arg-deduction.t
general/300-constructor-template-const-ref-enable-if-conversion.t
general/400-anonymous-namespace-partial-specialization.t
general/400-current-specialization-display-name-member-alias.t
general/400-member-template-result-pack-preserves-nested-function-pointer-owner.t
general/400-static-cast-rvalue-ref-skips-conversion-operator.t
general/400-unused-static-member-template-return-type.t
general/500-dependent-function-type-pack-expansion-ctor-init.t
general/500-dependent-qualified-member-template-result-bool.t
general/500-member-template-conditional-alias-trailing-return.t
general/500-mp11-append-alias-template-sfinae.t
general/500-nontype-alias-reinstantiation-structural-state.t
general/500-recursive-qualified-member-template-bool-arg.t
general/500-reentrant-static-query-callable-enable-if-cache.t
general/500-reentrant-static-query-enable-if-partial.t
general/500-source-namespace-base-sfinae-chain.t
spec/100-extern-template-member-function-declaration.t
spec/100-extern-template-static-data-declaration.t
spec/100-local-member-call-constructor-template-instantiation.t
spec/400-defaulted-nested-class-argument-partial-specialization.t
spec/400-explicit-pack-type-argument-uses-bound-type.t
spec/400-explicit-type-arg-dependent-qualified-member-template-id.t
spec/400-function-type-pack-template-argument.t
spec/400-qualified-member-template-id-bool-constant.t
spec/500-conditional-alias-index-sequence-member-template-call.t
```

LowIR comparisons (32):

```text
general/100-current-specialization-member-body-cast-compare.t
general/100-dependent-bool-partial-static-value-storage.t
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

### Refreshed Remaining Work Map

- **Dependent member-result and candidate-local SFINAE (11 status):**
  structured boolean overloads, dependent function-type packs, qualified
  boolean results, trailing-return aliases, MP11 append, non-type alias
  reinstantiation, recursive member queries, the two reentrant fixed-point
  queries, qualified boolean member-template lookup, and the conditional
  alias index-sequence call.
- **Owner and specialization replay (11 status):** current and inherited
  owners, anonymous scopes, selected/special-member replay, static-member
  state, source-namespace SFINAE, extern/template declarations, local member
  calls, and defaulted nested-class partial specialization.
- **Deduction and overload viability (10 status):** constructor and named-
  parameter SFINAE, explicit-pack and template-template deduction, member
  template instantiation/overload selection, rvalue-reference conversion
  ranking, and the remaining explicit type/function-pack witnesses.
- **LowIR materialization (32 comparisons):** generated current-specialization
  and constructor bodies, static storage, explicit/extern specialization,
  owner replay, ADL and partial ordering, variable-template state, and typed
  ABI-visible values.

### Next Substantial Checkpoint Group

Take the dependent member-result/candidate-local SFINAE group next, bundling
MP11 append, qualified boolean results, trailing-return aliases, and the
conditional-alias index-sequence member call. Keep the two reentrant cases as
fixed-point regression coverage with no timing-based acceptance. Follow with
the owner/specialization replay, deduction/overload, and LowIR groups.

## Checkpoint 23 audit — 2026-07-31

### Scope Reviewed

- The Checkpoint 23 scope, result, and refreshed complete failure inventory in
  `pa23/plan.md`, including the dependent candidate-viability contract and
  the selected next checkpoint group.
- `pa23/README.md`, `TESTING_AND_REFERENCES.md`, the relevant PA23 general and
  specification tests, and the through-PA22 validation contract.
- Commit `1246a1b` (`Implement PA23 dependent candidate viability checkpoint`),
  its recent parents, and the changed implementation in
  `pa14_lowering_constructors.cpp`, `pa14_lowering_control.cpp`,
  `pa14_lowering_collection.cpp`, `pa14_lowering.h`,
  `pa18_templates_rewrite.cpp`, `pa18_templates_rewrite_emit.cpp`,
  `pa18_templates_rewrite_match.cpp`,
  `pa18_templates_rewrite_specialization.cpp`, and the related PA18 headers.
- The authoritative active log at
  `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`, the
  four checkpoint witnesses, the exact through-PA22 report, and the PA23
  source file audit.

### Findings

- The checkpoint remains on the normal compiler pipeline: parsed AST,
  semantic collection and lookup, typed template substitution/replay, and
  LowIR emission.  There is no skipped phase, dummy or embedded output,
  interpreter/VM/trampoline substitute, reference-binary or host-compiler
  invocation, source/test-specific acceptance gate, or unchecked output path.
- The landed replay exception used a flattened `SpellNode(...).find("static")`
  test.  That was a stringly declaration fact and could silently apply the
  candidate-local skip to the wrong declaration.  It now uses the AST
  declaration-specifier fact, and the catch remains limited to the intended
  static member replay substitution boundary.
- The landed constructor duplicate-entry suppression rediscovered a
  function-template frame at a downstream call site from template-argument
  counts.  That was replaced with explicit `member_template_frame` provenance
  collected for both ordinary and special-member records, so the base-entry
  demand decision consumes an owned semantic fact.
- The landed scalar matching changes duplicated substring replacements in
  two replay paths.  Those replacements were both stringly and unsafe around
  identifier text.  Scalar spelling normalization is now centralized and
  token-aware, with invalid combinations left untouched.
- Temporary generated-class declaration indexing now has scoped restoration,
  including exception paths.  The bounded probe covers only the generated,
  logical, and concrete owner paths; it does not scan the registry or retain a
  failed declaration after substitution.
- Constructor initializer matching now asks the semantic analyzer and
  `PA12SameType` first.  The compact generated-name spelling comparison is
  only a fallback for lowered identities that intentionally differ from the
  source template-id, rather than the primary semantic fact.
- No timeout workaround, retry budget, timing-based acceptance, or fallback
  success path was added.  The two reentrant static-query status failures in
  the complete residual set remain ordinary fixed-point semantic coverage and
  are not accepted through a timeout or substitute execution path.
- The first post-fix file audit caught two real structural limit violations:
  one source file was 1501 lines and the replay helper was 123 lines.  The
  implementation was compacted without weakening the audit, and the final
  audit has no fatal finding and the same 13 repository warnings as the
  checkpoint baseline.

### Changes Made

- Added explicit member-template-frame ownership to PA14 function records and
  populated it at collection time; changed constructor base-entry emission to
  consume that field instead of recomputing context from the active call.
- Added semantic-first constructor base matching with a generated-identity
  fallback, preserving the constructor-shadow witness while removing the
  downstream text-only decision.
- Replaced raw declaration-text static detection with
  `HasDeclarationSpecifier`, centralized builtin scalar canonicalization for
  both match and specialization inference, and removed the duplicate
  substring-replacement helpers.
- Added RAII restoration for the temporary generated-class declaration index
  and kept the source/function-size audit limits intact.
- Updated `pa23/plan.md` with the exact 28 status and 32 LowIR residual
  fixtures, the grouped Remaining Work Map, and the next bundled checkpoint
  group.  No tests, references, checker rules, or external tools were
  modified for acceptance.

### Validation

- Serial compiler build: **PASS**.
- Named-parameter SFINAE, unused static-member return, constructor
  const-reference conversion, and constructor-parameter shadowing: **4/4**.
- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'`: **336/396**, preserving
  the audit-turn baseline and introducing no new failure.
- Required prior-through command: **PASS, 2100/2100 through PA22**.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src`: **PASS**,
  with 13 non-fatal repository warnings and no fatal issue.
- `git diff --check`: **PASS**.  The remaining PA23 failures are recorded as
  the semantic work frontier in `pa23/plan.md`, not as unresolved checkpoint
  architecture, performance, shortcut, regression, or audit debt.

## Checkpoint 26 audit — 2026-07-31

### Scope Reviewed

- The latest `Checkpoint Scope` in `pa23/plan.md` (the Checkpoint 24
  template-template/default-pack scope), its Checkpoint 25/26 results, and
  the newly refreshed complete PA23 failure inventory and next group.
- `pa23/README.md`, `TESTING_AND_REFERENCES.md`, the focused PA23 witnesses,
  the related PA18–PA22 tests, and the required through-PA22 contract.
- Recent commits `56663fb` (the landed pack and specialization replay
  checkpoint), `9f8a4fa` (the preceding audit), and `1246a1b` (the preceding
  candidate-viability implementation), including all changed PA14/PA18
  source modules and their ownership/indexing changes.
- The authoritative full-run log at
  `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`, the
  seven checkpoint witnesses, the exact through-PA22 report, and the PA23
  source file audit.

### Findings

- The checkpoint remains on the normal compiler pipeline: parsed AST,
  collection and lookup, typed template argument deduction/substitution and
  replay, PA14 lowering, and LowIR emission.  There is no skipped compiler
  phase, dummy or embedded output, interpreter/VM/trampoline substitute,
  reference-binary or host-compiler invocation, source/test-specific
  acceptance gate, or unchecked output path.
- The landed checkpoint preserves semantic facts in the existing typed
  records and maps: template definitions own conversion-operator identity,
  `conversion_operator_definitions_by_owner_` indexes those definitions by
  owner, `active_pack_substitutions_` carries pack elements, and the explicit
  specialization-argument count records preserve use-site arity.  No new
  duplicated owner object or downstream recovery of those facts was found.
- One avoidable performance issue was present in constructor replay: each
  initializer argument could scan the entire `definitions_` registry twice to
  rediscover conversion operators from their printed names.  That was a
  repeated full-registry walk and a stringly recovery of an already-indexed
  fact; it is fixed below by consuming the owner-indexed conversion records.
- Active-pack ellipsis expansion had two overlapping scans: the new helper
  expanded a scalar substitution while `RewriteText` separately walked every
  active pack and expanded its source token.  The work is now one centralized,
  identifier-boundary-aware pass.  It remains a source-spelling operation
  before AST transformation, not reparsing of emitted text or a semantic
  acceptance path.
- No timeout workaround, retry budget, timing-based acceptance, or fallback
  success path was added.  The two reentrant static-query failures in the
  complete residual set remain ordinary fixed-point semantic coverage and are
  not converted into success by timeout or substitute execution.
- The file audit has no fatal finding and retains the same 13 non-fatal
  repository warnings as the checkpoint baseline.  The fixes stayed in the
  checked `dev/src` modules; no tests, references, checker rules, generated
  payloads, hidden fragments, or unchecked paths were added or modified.
- The current PA23 result is above the turn-start baseline (343/396 versus
  336/396), and the exact earlier-PA report remains clean.  No checkpoint-level
  architecture, performance, shortcut, regression, or file-audit blocker
  remains.

### Changes Made

- Changed `MaterializeInitializerConstructor` to consume the existing
  `conversion_operator_definitions_by_owner_` vector in both conversion
  replay paths.  The index is built once from typed `TemplateDefinition`
  records, so replay no longer scans all definitions or reconstructs
  conversion-operator status from name spelling for each argument.
- Consolidated active-pack ellipsis handling in
  `ExpandActivePackEllipsis`.  It expands both the active pack name and its
  scalar substitution with identifier boundaries, preserves empty-pack comma
  cleanup, and removes the duplicate full active-pack loop from `RewriteText`.
- Added this audit and refreshed `pa23/plan.md` with the exact 24 status and
  29 LowIR residual fixtures, the concise grouped Remaining Work Map, and the
  next bundled owner/specialization materialization checkpoint.
- No tests, `.ref` files, harnesses, acceptance checks, or external tools
  were changed.

### Validation

- `make build` through the required validation path: **PASS**.
- The seven checkpoint witnesses: **7/7 PASS**.
- Required prior-through command (`make test-report-through-pa22`):
  **PASS, 2100/2100**.
- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'`: **343/396**, exit 2 only
  because the 53 remaining PA23 fixtures are not complete; the fresh log
  contains exactly **24 status** and **29 LowIR** failures.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src`: **PASS**,
  with 13 non-fatal warnings and no fatal finding.
- `git diff --check`: **PASS**.
- The final committed worktree was checked with `git status --short` and is
  clean.

## Checkpoint 27 audit — 2026-07-31

### Scope Reviewed

- The latest `Checkpoint 27 Scope` and result in `pa23/plan.md`, including
  the explicit/extern specialization-materialization contract, its 343/396
  baseline, and the recorded 351/396 result.
- `pa23/README.md`, `TESTING_AND_REFERENCES.md`, the PA23 owner/replay
  witnesses, and the earlier PA18–PA22 report contract.
- HEAD `053b3dd` (`Implement PA23 explicit specialization replay`), the
  preceding audit/implementation commits, and every changed PA14, parser,
  ABI, and PA18 replay module in the checkpoint diff.
- The authoritative full-run log at
  `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`, the
  fresh through-PA22 report, and the PA23 source file audit.

### Findings

- The checkpoint stays on the required pipeline: parser AST, semantic
  collection and lookup, typed template replay, PA14 lowering, and LowIR
  emission.  No compiler phase is skipped, and there is no dummy output,
  embedded payload, interpreter/VM/trampoline substitute, reference-binary or
  host-compiler call, source/test-specific acceptance gate, or unchecked
  output path.
- Explicit and extern instantiation handling uses the existing typed class,
  template-definition, `FunctionRecord`, and `GlobalRecord` facts.  Extern
  declarations do not materialize a body; explicit instantiations are
  constrained by the parsed source-token boundary; and explicit function
  specializations remain selected as typed entities.  These are not fallback
  success paths.
- The owner-registry fallback had one checkpoint-level ownership defect: its
  specialization match compared only the terminal template-name component.
  Two source-qualified templates such as `a::Box<T>` and `b::Box<T>` could
  therefore select the wrong typed class when their arguments matched.  This
  was fixed in the audit by requiring exact equality when both primary paths
  are source-qualified, while retaining terminal-name compatibility only when
  one side is an unqualified generated primary.
- The ABI correction consumes typed specialization primary/argument fields;
  its generated-name compatibility branch is confined to symbol encoding and
  cannot accept a declaration or choose a candidate.  No new semantic fact is
  recovered by reparsing emitted LowIR text.
- No timeout workaround, retry budget, timing-based acceptance, broad catch-all
  success path, repeated full-suite walk, or new hot-path quadratic scan was
  introduced.  The one reentrant timeout remains an ordinary failing semantic
  fixture and is still visible in the complete report.
- The file audit has no fatal finding and the same 13 pre-existing warnings as
  the checkpoint baseline.  All implementation changes remain in checked
  `dev/src`; no tests, references, harnesses, checker rules, generated
  payloads, or hidden implementation fragments were changed.
- The current PA23 count is above the turn-start baseline (351/396 versus
  343/396), and the earlier assignments remain clean.  No checkpoint-level
  shortcut, ownership, performance, regression, or file-audit blocker remains.

### Changes Made

- Added `PA14OwnerPrimaryMatches` and used it in `ResolveClassOwner` so the
  typed class-registry fallback cannot conflate source-qualified owners that
  share a terminal template name.
- Kept unqualified generated-primary compatibility for materialized replay,
  so the fix does not turn the existing generated-owner handoff into a
  spelling mismatch.
- Refreshed `pa23/plan.md` with the exact fresh 22 status failures and 23
  LowIR comparisons, the grouped Remaining Work Map, and the next bundled
  checkpoint group.
- No tests or `.ref` files were modified.

### Validation

- `make build`: **PASS**.
- Required prior-through command (`n=23; ... make test-report-through-pa22`):
  **PASS, 2100/2100**.
- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'`: **351/396**, with the
  expected residual **22 status failures**, **23 LowIR comparisons**, and
  **one timeout**; no new failure appeared.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src`:
  **PASS**, with 13 non-fatal pre-existing warnings.
- `git diff --check`: **PASS**.
- The final post-commit `git status --short` check is required to be empty.

### Refreshed Remaining Work Map

- **Dependent owner and candidate replay:** qualified function/member lookup,
  current/inherited/anonymous owners, member-template selection, alias-based
  SFINAE, local constructor replay, and qualified member-template calls remain
  the status frontier.
- **Dependent result formation and conversion ranking:** dependent function and
  boolean results, trailing aliases, MP11/recursive queries, rvalue-reference
  conversion selection, and the remaining template-template/member-operator
  comparisons remain the next semantic group.
- **Typed values and LowIR materialization:** static/variable-template
  storage, non-type alias state, `sizeof`, explicit type arguments, partial
  member owners, and generated body/storage comparisons remain after candidate
  selection is stable.
- **Fixed-point coverage:** the two reentrant static-query fixtures remain
  explicit termination regressions and are not accepted through timing or
  fallback behavior.

## Checkpoint 28 audit — 2026-07-31

### Scope Reviewed

- The latest `Checkpoint 28 Scope` and result in `pa23/plan.md`, including
  the 351/396 start, the 354/396 checkpoint result, the exact residual
  fixture inventory, and the next checkpoint group.
- `pa23/README.md`, `TESTING_AND_REFERENCES.md`, the three direct replay
  witnesses, the related PA18–PA22 report contract, and the full primary log
  at `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.
- HEAD `919117b` (`Advance PA23 member-template replay`), its preceding
  audit/implementation commits, and the changed replay modules in the
  checkpoint diff: `pa18_templates_calls_member.cpp`,
  `pa18_templates_calls_transform.cpp`,
  `pa18_templates_rewrite_infer_call.cpp`, and
  `pa18_templates_rewrite_infer_types.cpp`.
- The required through-PA22 report, the PA23 source file audit, and the
  focused witness output before and after the audit fix.

### Findings

- The checkpoint remains on the normal compiler pipeline: parsed AST,
  semantic collection and lookup, typed template deduction/substitution and
  replay, PA14 lowering, and LowIR emission.  No compiler phase is skipped;
  there is no dummy or embedded output, interpreter/VM/trampoline substitute,
  reference-binary or host-compiler invocation, source/test-specific
  acceptance gate, or unchecked output path.
- The member-call parser's new `TemplateRange` use correctly distinguishes
  `::` inside a globally qualified explicit template argument from the
  owner separator after a template-id.  That source-spelling parse only
  locates the AST operation; candidate identity and ownership still come from
  the existing typed member lookup and replay records.
- One checkpoint-level architecture defect was present in the new nested
  explicit-template result fallback.  It reparsed the callee spelling,
  matched only equal parameter counts, substituted raw argument text into a
  declaration, and returned the first non-empty result.  That could select an
  unrelated overload, mishandle packs/defaults or an explicit specialization,
  and recover a semantic return fact downstream from text.  It also duplicated
  candidate lookup work outside the normal explicit-call path.  This was fixed
  below; the fallback now uses the existing typed candidate ranking, explicit
  viability, completed argument deduction/default validation, and
  `FunctionResultType` path.
- The visible-function priority change is scoped to replayed identifier
  inference: a function-local parameter or an owner-indexed object remains a
  typed variable fact, while a leaked translation-unit parameter spelling no
  longer hides a visible function signature.  The owner walk is bounded by
  lexical context; it does not scan the definition registry or reparse emitted
  LowIR.
- The implicit-member fallback only marks a call as handled after typed
  `InferArgument` and `FindClassMemberType` succeed.  It does not manufacture
  output or turn a failed lookup into success.  The two reentrant static-query
  fixtures remain ordinary fixed-point failures in the report; one is observed
  as `EXIT_TIMEOUT` under the current harness, and the same isolated timeout
  reproduces at the pre-audit `919117b` tree.  No timeout, retry budget, timing
  acceptance, or substitute execution path was added.
- The source file audit has no fatal finding and retains the same 13
  repository-level advisory warnings.  No tests, references, harnesses,
  checker rules, generated payloads, hidden implementation fragments, or
  unchecked paths were added or modified.
- The audit fix leaves the current-PA count at the checkpoint baseline
  (**354/396**) and preserves the earlier assignments (**2100/2100 through
  PA22**).  The complete current-PA residual is exactly **19 status failures**
  (including one timeout in the fixed-point pair) and **23 LowIR comparisons**.
  Reproducing that timeout at the landed pre-audit tree confirms it is the
  existing fixed-point semantic frontier, not a checkpoint-level performance
  regression.  These are not an unresolved checkpoint shortcut, ownership,
  regression, or file-audit blocker.

### Changes Made

- Added `ResolveExplicitTemplateCallResult` to the shared call-transformation
  module.  It selects the nested explicit call with
  `SelectExplicitCallDefinition`, completes its typed template arguments with
  `InferFunctionArguments`, validates defaults, and computes its result with
  `FunctionResultType`.
- Replaced the checkpoint's first-match raw-text return inference in
  `InferCallIdentifierArgument` with that shared resolver.  The outer
  deduction probe now consumes the same candidate and result semantics as
  actual call materialization, with no downstream recovery from an emitted
  spelling.
- Refreshed `pa23/plan.md` with this audit result, the exact 42-fixture
  residual set, the grouped Remaining Work Map, and the next substantial
  checkpoint group.  No tests or `.ref` files were changed.

### Validation

- `make build`: **PASS**.
- Focused checkpoint witnesses: **3/3 PASS** after a serial rerun (the only
  failed attempt was a parallel local-check build-config race, not a compiler
  result).
- Required prior-through command
  (`n=23; ... make test-report-through-pa22`): **PASS, 2100/2100**.
- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'`: **354/396**, exit 2 only
  for the 42 checked-in PA23 residual fixtures; the primary log reports
  exactly **19 status** (one timeout in the fixed-point pair) and **23 LowIR**
  failures.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src`:
  **PASS**, with 13 non-fatal advisory warnings and no fatal finding.
- `git diff --check`: **PASS**.

### Remaining Work Map

- **Member-template replay and owner identity — 11 status failures:**
  qualified/defaulted lookup, current/inherited/anonymous/local owners,
  partial-specialization owner completion, and qualified member-template
  calls remain the status frontier.
- **Dependent result, SFINAE, and conversion composition — 6 status
  failures:** alias substitution, pack-expanded constructor arguments,
  dependent boolean/trailing results, MP11/recursive queries, and
  rvalue-reference conversion selection remain after candidate replay.
- **Fixed-point query stress — 2 status failures:** both reentrant static
  query fixtures remain explicit termination coverage; one currently reaches
  the harness timeout, and both must be solved with stable typed query
  identity, not timing or fallback acceptance.
- **Typed value and LowIR materialization — 23 comparisons:** static and
  variable-template storage, non-type alias state, `sizeof`, explicit type
  arguments, partial member owners, and generated body/storage metadata still
  differ after selection.

### Next Substantial Checkpoint Group

Bundle the 23 typed-value/LowIR materialization comparisons with the six
dependent-result/SFINAE/conversion status cases.  Preserve the 11 owner and
candidate replay status fixtures and both fixed-point stress fixtures as
regressions.  The next checkpoint should carry typed owner, non-type value,
selected overload, and materialized function identity through lowering, then
rerun the PA23 report, the exact through-PA22 command, and the file audit.

## Checkpoint 33 audit — 2026-08-01

### Scope Reviewed

- The latest Checkpoint 33 result and Checkpoint 34 scope in
  [pa23/plan.md](plan.md), including the 372/396 baseline and the intended
  dependent-template-qualifier replay boundary.
- The recent checkpoint commits `5b1d26b`, `3949446`, `cccecd5`, and
  `6630c22`, their changed PA18/PA11 sources, and the current implementation
  diff.
- [pa23/README.md](README.md), [TESTING_AND_REFERENCES.md](../TESTING_AND_REFERENCES.md),
  the through-PA22 report, the PA23 file audit, and the authoritative primary
  log at `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.

### Findings

- The checkpoint remains on the required compiler path: parsed AST, semantic
  collection and lookup, typed template substitution/replay, PA14 lowering,
  and LowIR emission.  There is no skipped phase, dummy output, embedded
  payload, interpreter/VM/trampoline substitute, reference-binary or host
  compiler invocation, source/test-specific acceptance gate, or unchecked
  output path.
- The dependent-`template` qualifier change is a bounded lexical operation
  used during identifier substitution.  It preserves the identifier for the
  existing typed lookup/replay path and does not reparse emitted LowIR or
  manufacture a successful result.
- A checkpoint-level shortcut was present in the preceding owner replay: PA11
  `TypeSize` returned a recorded size (or `1`) for a complete-but-unlaid-out
  generated specialization shell.  That made an incomplete class succeed
  through `sizeof`.  The fallback is removed.  Valid storage-demand cases now
  promote the recorded deferred specialization through its typed primary and
  arguments before PA11 layout; typedefs, aliases, pointers, references, and
  template arguments remain deferred, so this is not broad eager materialization.
- CP32 early integral-member registration had avoidable string-wide sibling
  scans, copied and then walked the entire constant-value map to clean up
  temporary entries, and leaked its member set across nested replay.  It now
  builds one structural class-scope identifier index, removes only entries it
  inserted, and scopes the set across replay.  The active integral recursion
  guard is a structured expression/context/owner key, preventing one active
  specialization from suppressing another.
- The deferred-specialization cache cleanup uses a direct generated-name to
  cache-key index rather than an ownership-blind full-map search.  Replay-state
  restoration uses swaps, avoiding a hot-path set copy.  No repeated full-suite
  walk, timing/retry timeout workaround, catch-all success path, or downstream
  recovery from emitted text was added.
- The audit fixes themselves initially crossed the file and function limits;
  the final change keeps the replay state in a responsibility-named header,
  moves deferred promotion beside materialization, and leaves
  `FinishRegularNode` within the 120-line limit.  The file audit has no fatal
  finding.  No tests, `.ref` files, harnesses, checker rules, or unchecked
  implementation fragments were changed.
- The complete current-PA result is unchanged from the checkpoint baseline at
  372/396, while the required earlier assignments remain 2100/2100.  The 24
  reported PA23 cases are the exact next owner/replay, materialization, and
  fixed-point work boundary; none was hidden or converted into acceptance.

### Changes Made

- Removed the incomplete-layout success fallback from
  `dev/src/pa11_semantics_analyzer.cpp`.
- Added demand-driven deferred-class promotion and direct deferred cache-key
  tracking in `dev/src/pa18_templates_rewrite.cpp`,
  `dev/src/pa18_templates_rewrite_emit.cpp`, and the corresponding headers.
- Replaced duplicate dependent-template qualifier logic with one shared helper
  in `dev/src/pa18_templates_collection.cpp` and its header declaration.
- Reworked early integral-member indexing in
  `dev/src/pa18_templates_rewrite_integral.cpp`; added owner-aware integral
  evaluation identity and replay-state scoping in
  `dev/src/pa18_templates_rewrite_instantiate.cpp`.
- Split the typed replay state into
  `dev/src/pa18_templates_replay_state.h` to keep source/header ownership
  within the file-audit limits.  No new compiler phase or alternate output
  path was introduced.

### Validation

- `make build`: **pass**.
- Required prior-through command (`n=23; ... make test-report-through-pa22`):
  **pass, 2100/2100**.
- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'`: **372/396**, exit 2 only
  for the complete checked-in PA23 residual set: 16 LowIR comparisons, six
  ordinary status failures, and two timeout failures.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src`: **pass**
  with 12 non-fatal advisory warnings and no fatal finding.
- `git diff --check`: **pass**.
- Current PA remains at the 372/396 checkpoint baseline, and earlier PAs pass
  through PA22; the implementation is ready for the planned Checkpoint 34
  owner/replay group.

### Refreshed Remaining Work Map

- **LowIR owner/replay — 16:** current-specialization body/canonical owner,
  qualified argument replay, ADL and template deduction, member-operator
  partial ordering, explicit type-argument overload selection,
  member-variable/compound-assignment replay, out-of-class owner aliases,
  conversion, and defaulted specialization cases (the exact fixture list is
  recorded in the matching plan section).
- **Status/materialization — 6 ordinary failures:** member-template result
  packs, dependent function-type packs, qualified bool results, MP11 alias
  SFINAE, recursive qualified bool arguments, and the reentrant callable query.
- **Fixed-point/termination — 2 timeouts:** dependent default non-type
  evaluation and reentrant static-query partial selection.

### Next Substantial Checkpoint Group

Checkpoint 34 is the 16-case LowIR owner/replay group, beginning with shared
qualified-member/template-id canonicalization. The six status cases and two
fixed-point cases are retained as regression gates and will be bundled into
the next larger materialization/fixed-point checkpoint once owner identity is
stable.

## Checkpoint 34 audit — 2026-08-01

### Scope Reviewed

- The latest Checkpoint 34 scope and result in [pa23/plan.md](plan.md), the
  PA23 contract in [pa23/README.md](README.md), and
  [TESTING_AND_REFERENCES.md](../TESTING_AND_REFERENCES.md).
- Recent commits `75e741a` and `3f06b6a`, the checkpoint's PA18 source diff,
  and the shared typed replay, candidate-ranking, and PA14 lowering paths.
- The five scoped overload fixtures, the full current-PA report, the required
  through-PA22 report, the file audit, and
  `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.

### Findings

- The checkpoint stays on the normal parser, semantic collection, typed
  deduction/substitution and replay, PA14 lowering, and LowIR emission path.
  There is no skipped phase, dummy or embedded output, interpreter/VM/
  trampoline substitute, reference-binary or host-compiler invocation,
  source/test-specific acceptance gate, or unchecked output path.
- A real checkpoint-level shortcut existed in the deferred operator probe:
  it rebuilt a return type from declaration spelling, parsed that spelling as
  a class template, and eagerly called `Instantiate` merely to discover a
  type-only result. That duplicated semantic ownership, could re-enter
  template replay, and recovered a selected result downstream from raw text.
  The probe now uses the selected candidate's shared typed
  `FunctionResultType` path and does not materialize a result class.
- The new static-member receiver rewrite needed an explicit child-loop stop
  after canonicalizing the binary receiver. Without it, the next source child
  could be appended after the canonical node. The fix preserves the
  transformed receiver operands as typed lowering provenance—required by the
  PA14 object-use/lifetime path—while preventing unrelated child recovery.
- Candidate ranking now shares the AST lvalue classifier between deduction
  and member-operator reference ranking. Template-parameter kind remains the
  owner of explicit type, non-type, and template-template facts; no new
  stringly fact or emitted-text reparsing was added.
- The ranking and explicit-kind checks are bounded per candidate and argument.
  They do not add repeated full-suite walks, broad registry scans, excessive
  copying, retry/timing logic, or a timeout workaround. The two timeout
  fixtures remain genuine fixed-point semantic failures.
- The file audit has no fatal finding and retains its 12 advisory warnings.
  No tests, references, harnesses, checker rules, embedded payloads, hidden
  implementation fragments, or weakened checks were changed.

### Changes Made

- Replaced the deferred `EmitMemberCandidate` raw return-spelling and eager
  class-instantiation branch with `FunctionResultType` in
  `dev/src/pa18_templates_calls_member.cpp`.
- Made binary-receiver canonicalization stop the regular child walk after it
  has installed the selected typed static owner, while retaining the
  transformed operand provenance in `dev/src/pa18_templates_rewrite.cpp`.
- Moved `IsLvalueTemplateArgument` into the shared PA18 template namespace and
  reused it from `dev/src/pa18_templates_calls_class_rank.cpp`, with its
  declaration in `dev/src/pa18_templates_collection.h`.
- Refreshed the plan and this audit only; no tests or `.ref` files were
  modified.

### Validation

- `make build`: **PASS**.
- Scoped Checkpoint 34 overload fixtures: **5/5 PASS**.
- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'`: **377/396**; exit 2 is
  only the complete checked-in PA23 residual set, with 19 failures and the
  same two timeout failures as the checkpoint baseline.
- Required prior-through command (`n=23; ... make
  test-report-through-pa22`): **PASS, 2100/2100**.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src`:
  **PASS**, with 12 non-fatal advisory warnings and no fatal finding.
- `git diff --check`: **PASS**. The current-PA result is at or above the
  turn-start baseline and earlier PAs remain clean.

### Remaining Work Map

- **Owner/declaration canonicalization — 8 LowIR comparisons:**
  `general/100-current-specialization-member-body-cast-compare.t`,
  `general/100-local-qualified-argument-replay.t`,
  `general/200-adl-explicit-template-id-call.t`,
  `general/300-current-specialization-constructor-template-canonical-owner.t`,
  `general/400-out-of-class-partial-member-template-owner-parameter-alias.t`,
  `spec/100-out-of-class-conversion-operator-definition.t`,
  `spec/400-defaulted-nested-class-argument-partial-specialization.t`, and
  `spec/400-defaulted-template-arg-partial-base-completion.t`.
- **Typed expression/value lowering — 3 LowIR comparisons:**
  `general/300-dependent-bool-base-trait-type-argument.t`,
  `general/400-member-variable-template-leaf-sfinae.t`, and
  `general/400-nonmember-template-compound-assignment-const-lhs.t`.
- **Deferred result materialization — 6 status failures:**
  `general/400-member-template-result-pack-preserves-nested-function-pointer-owner.t`,
  `general/500-dependent-function-type-pack-expansion-ctor-init.t`,
  `general/500-dependent-qualified-member-template-result-bool.t`,
  `general/500-mp11-append-alias-template-sfinae.t`,
  `general/500-recursive-qualified-member-template-bool-arg.t`, and
  `general/500-reentrant-static-query-callable-enable-if-cache.t`.
- **Fixed-point query identity — 2 timeouts:**
  `general/400-dependent-default-nontype-argument-eval.t` and
  `general/500-reentrant-static-query-enable-if-partial.t`.

### Next Substantial Checkpoint Group

Bundle the 8 owner/declaration comparisons with the 3 typed expression/value
comparisons. Once owner identity is stable, take the 6 deferred-result status
cases together; keep the 2 fixed-point timeout witnesses as a separate
bounded semantic group and retain all 19 fixtures as regression gates.

## Checkpoint 45 audit — 2026-08-02

### Scope Reviewed

- The latest `Checkpoint 44 scope` and `Checkpoint 45 result` in
  [pa23/plan.md](plan.md), the assignment contract in
  [pa23/README.md](README.md), and the repository rules in
  [TESTING_AND_REFERENCES.md](../TESTING_AND_REFERENCES.md).
- Recent checkpoint commits `369a2db`, `682e113`, `96f41aa`, `1e8408`, and
  `82d3695`, plus the current PA18 collection, replay-state, selection,
  instantiation, member, emission, and text-suffix changes under `dev/src`.
- The complete seven-case status scope and three LowIR comparisons from the
  latest plan, the focused source-base/nested-owner witnesses, the full
  current-PA report, the required through-PA22 report, the file audit, and
  `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.

### Findings

- The checkpoint remains on the required compiler pipeline: parser and AST
  collection, typed semantic lookup/deduction/substitution and replay, PA14
  lowering, and LowIR emission.  There is no skipped phase, dummy or embedded
  output, interpreter/VM/trampoline substitute, reference-binary or host
  compiler invocation, source/test-specific acceptance gate, or unchecked
  output path.
- Source-base and nested-owner completion is bounded to a typed nested class
  template with no own template parameters and only the member/expressions
  needed for lookup.  The typedef guard prevents construction of a typedef or
  a recursive nominal chain, direct function-type arguments stay out of class
  template selection, and templated nested arguments are not eagerly
  materialized as a general fallback.
- The audit found and removed a stringly recursive-selection fallback.  A
  recursive selection now raises the typed
  `PA18RecursiveClassTemplateSelection` substitution failure from replay state;
  `FindClassDeclaration` no longer catches an error message and silently
  returns the primary declaration.  This keeps recursion failure in semantic
  selection rather than turning it into a success path.
- The audit found avoidable registry work in the changed replay paths.  Alias
  suffix recovery now uses `type_aliases_by_name_` and the canonical alias
  registry; generated declaration-suffix detection uses
  `class_paths_by_name_`; and concrete-owner resolution uses that same index
  instead of scanning every class context for each candidate.  Successful
  materialized owners are retained in locals, removing duplicate index
  lookups.  These changes preserve typed ownership and avoid quadratic
  registry/context walks and hot-path recomputation.
- No LowIR text is reparsed to recover semantic facts.  Source spelling is
  used only for bounded name/range location; ownership, specialization bases,
  aliases, parameter kinds, and substitution failures remain represented by
  typed registries and state.  No timeout cap, retry, sleep, or timeout-only
  acceptance workaround was added; the one current timeout remains a genuine
  reentrant semantic failure in the next implementation map.
- The file audit passes with the same 12 non-fatal advisory warnings and no
  fatal size, hidden-fragment, or unchecked-path finding.  No tests, `.ref`
  files, harnesses, checker rules, or reference outputs were changed.
- The audit turn began at 388 / 396 and ended at 388 / 396.  The earlier
  assignments remain clean at 2100 / 2100 through PA22, so the checkpoint is
  regression-safe and ready for the next eight-fixture group.

### Changes Made

- Moved substitution-failure ownership into
  `dev/src/pa18_templates_replay_state.h`, added the typed recursive-selection
  failure, and removed the message-based catch/fallback from
  `dev/src/pa18_templates_rewrite.h` and
  `dev/src/pa18_templates_rewrite_selection.cpp`.
- Replaced full alias-registry and class-declaration scans with existing
  name/path indexes in `dev/src/pa18_templates_rewrite_text_suffix.cpp`,
  `dev/src/pa18_templates_rewrite_members.cpp`, and
  `dev/src/pa18_templates_rewrite_emit.cpp`.
- Indexed concrete-owner materialization and removed duplicate successful
  lookups in `dev/src/pa18_templates_rewrite_instantiate.cpp`.
- Refreshed this audit and the concise plan map only; no test or reference
  fixture was modified.

### Validation

- `make build` — **PASS**.
- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'` — **388 / 396**; the
  complete residual set is five status failures, one timeout, and three LowIR
  comparisons, with no new failure.
- Required prior-through command (`n=23; ... make
  test-report-through-pa22`) — **PASS, 2100 / 2100**.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src` — **PASS**,
  with 12 advisory warnings and no fatal finding.
- `git diff --check` — **PASS**.  The largest changed-path files remain within
  the file-audit limits, including `pa18_templates_rewrite_instantiate.cpp`
  at 1496 lines.

### Remaining Work Map

- **Qualified dependent-result replay — 2 status failures:**
  `general/500-dependent-qualified-member-template-result-bool.t` and
  `general/500-recursive-qualified-member-template-bool-arg.t`.
- **Alias-template SFINAE — 1 status failure:**
  `general/500-mp11-append-alias-template-sfinae.t`.
- **Reentrant static-query selection/cache — 2 cases:**
  `general/500-reentrant-static-query-callable-enable-if-cache.t` and
  `general/500-reentrant-static-query-enable-if-partial.t` (timeout).
- **LowIR owner/declaration comparisons — 3 cases:**
  `spec/100-out-of-class-conversion-operator-definition.t`,
  `spec/400-defaulted-nested-class-argument-partial-specialization.t`, and
  `spec/400-defaulted-template-arg-partial-base-completion.t`.

### Next Substantial Checkpoint Group

Bundle these five status cases and three LowIR comparisons as one eight-fixture
checkpoint group.  Begin with the shared typed result/query identity boundary
in the two qualified-result cases, then validate the declaration-owner
emission comparisons; keep the timeout as a regression gate and do not add a
timing-based acceptance path.

## Checkpoint 46 audit — 2026-08-02

### Scope Reviewed

- The latest Checkpoint 46 scope and result in [pa23/plan.md](plan.md), the
  PA23 contract in [pa23/README.md](README.md), and the repository test and
  reference rules in [TESTING_AND_REFERENCES.md](../TESTING_AND_REFERENCES.md).
- The latest implementation commit `59c93fb` (`Fix PA23 template partial
  ordering`), its preceding audit checkpoint `508a32f`, and the changed
  `dev/src/pa18_templates_rewrite.cpp` partial-ordering path, together with
  its typed `TemplateDefinition` and class-specialization selection callers.
- The complete current-PA report and primary log at
  `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`, the
  focused template-template ordering and deduction witnesses, the required
  through-PA22 report, and the PA23 source file audit.

### Findings

- The checkpoint remains on the normal parser/AST collection, typed template
  selection and substitution, replay, PA14 lowering, and LowIR emission path.
  No compiler phase was skipped; there is no dummy or embedded output,
  interpreter/VM/trampoline substitute, reference-binary or host-compiler
  invocation, source/test-specific acceptance gate, or unchecked output path.
- The new concrete-vs-template-template ordering branch had a real semantic
  defect: its final `return lhs_template_head == 0` also declared every
  no-template-headed pattern more specialized than a concrete class-template
  head.  Unrelated shapes such as a pointer pattern versus a class-template
  pattern must be decided by the existing structural matcher, not by that
  category heuristic.
- The new head classification also canonicalized and rescanned pattern and
  parameter data separately for each pairwise partial-order comparison.  That
  was avoidable hot-path recomputation.  Template-parameter kind remains
  sourced from typed `TemplateParameter` metadata; pattern spelling is used
  only once per comparator to locate the structural head and repeated shape.
- No emitted LowIR text is reparsed for semantic facts.  The checkpoint does
  not add a fallback-success path, timeout cap/retry, sleep, or timeout-only
  acceptance rule.  The reentrant static-query timeout remains a genuine
  semantic termination failure in the refreshed work map.
- The file audit reports the same 12 non-fatal repository advisories and no
  fatal size, hidden-fragment, weakened-check, or unchecked-path finding.  No
  test, `.ref` file, harness, checker rule, or reference output was changed.

### Changes Made

- Replaced the integer template-head classification and duplicate rescans in
  `dev/src/pa18_templates_rewrite.cpp` with a local enum and one
  `TemplateHeadShape` summary built from typed template-parameter details and
  one canonical pattern walk.
- Restricted the immediate ordering rule to the two sound relations:
  concrete class-template head over template-template head, and the existing
  repeated template-template shape over an unconstrained head.  Concrete
  versus unrelated no-head patterns now falls through to structural partial
  ordering instead of accepting a heuristic result.
- Refreshed the checkpoint plan and this audit only; no test or reference
  fixture was modified.

### Validation

- `make build` — **PASS**.
- Focused witnesses — **5/5 PASS**: the template-template partial-ordering
  case, its function-template deduction companion, and three earlier PA23
  regression witnesses.
- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'` — **389/396**.  The
  result equals the audit-turn baseline and the complete residual set is
  three status failures, one genuine timeout, and three LowIR comparisons.
- Required prior-through command (`n=23; ... make
  test-report-through-pa22`) — **PASS, 2100/2100**.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src` —
  **PASS**, with the unchanged 12 advisory warnings and no fatal finding.
- `git diff --check` — **PASS**.  The current-PA result stayed at the
  turn-start baseline while earlier assignments remained clean.

### Remaining Work Map

- **Qualified dependent-result replay — 2 status failures:**
  `general/500-dependent-qualified-member-template-result-bool.t` and
  `general/500-recursive-qualified-member-template-bool-arg.t` still lose the
  concrete nested owner/result type during recursive member-template replay.
- **Reentrant static-query identity/cache — 2 cases:**
  `general/500-reentrant-static-query-callable-enable-if-cache.t` still fails
  to reuse the dependent result query, and
  `general/500-reentrant-static-query-enable-if-partial.t` remains a timeout
  during recursive partial-specialization selection.
- **Owner/declaration LowIR materialization — 3 comparisons:**
  `spec/100-out-of-class-conversion-operator-definition.t`,
  `spec/400-defaulted-nested-class-argument-partial-specialization.t`, and
  `spec/400-defaulted-template-arg-partial-base-completion.t` still differ
  in generated declaration/owner materialization and call identity.

### Next Substantial Checkpoint Group

Bundle all seven remaining fixtures into one final PA23 integration group:
first trace the four qualified-result/reentrant cases through the typed
owner/result and semantic-query identity boundary, then validate and close the
three owner/declaration LowIR comparisons against that same identity.  Keep
the timeout as a termination regression gate; do not add a timing-based,
fixture-specific, or emitted-text acceptance path.
