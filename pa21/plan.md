# PA21 checkpoint plan

## Latest checkpoint and next scope

### Turn-start audit

The turn-start stage baseline was **96/215 PA21 tests**.  The complete
current-PA failure set was inspected before implementation and grouped by
shared behavior: dependent value/member replay, generated pack and base
layout, alias/template-template entities, member/friend owner lookup and
overload/ADL, explicit instantiation ownership, and ordinary lowering/layout.

### Checkpoint Scope

This turn completes the dependent value and generated-layout increment:

- preserve typed non-type values while recursively replaying variadic static
  members and global typedefs;
- retain reference/array declarator shells through alias and nested-member
  lookup;
- handle empty pack completion and concrete global template-member replay;
- carry all direct template bases into semantic layout and lower base-owner
  address adjustments; and
- materialize local class addresses for deferred `sizeof` layout queries.

Validation covered the recursive 64-element depth sum, nested outer-type pack,
reference-array alias, template-template arity, deferred incomplete-layout,
conditional/dependent owner, variadic-base, empty-pack, explicit-specialization
and generated-base probes.  The complete report was rerun after cleanup.

### Checkpoint Result

The focused depth-sum fixture now passes with generated value **64**
(previously **1**), and the empty-pack/generated-base and alias declarator
probes remain passing.  The current PA report is **118/215**, an improvement
of **22 tests** over the turn-start baseline and **2 tests** over the prior
checkpoint.

The two through-PA20 regressions found during validation were repaired as
well: qualified generated-owner member replay now preserves the resolved
member type, and dependent static initializers remain deferred until concrete
	substitutions exist.  Focused PA18/PA19 validation passes, the full
	through-PA20 gate is **1635/1635**, and the PA21 report is **118/215**.  The
	required file audit now passes with only the repository's existing
	non-fatal header-shape warnings.  The layout pass, template owner lookup,
	specialization-ordering fragment, and template type helpers were split into
	cohesive source units without changing the passing report.

### Remaining Work Map

The post-checkpoint report has **97** failures, grouped as follows:

- **Member/friend owner replay and lookup (52):** out-of-class and nested
  member-template definitions, using imports, hidden friends/ADL, inherited
  members, explicit member calls, operators, and overload selection.
- **Alias/template-template and dependent non-type replay (17):** alias
  partials, template-template arity/entity binding, pointer/cv distinctions,
  pack expressions, declaration-scope values, and inline-namespace aliases.
- **Explicit specialization/instantiation ownership (18):** explicit and
  extern instantiation materialization, specialization ordering, constructors,
  static data/functions, and use-location replay.
- **Partial-selection and ordinary lowering/layout (10):** remaining
  reference/cv/pack selection, incomplete-class checks, anonymous storage,
  static-constexpr array arguments, and LowIR/ABI presentation.

### Next Checkpoint Group

Take the member/friend owner replay group, starting with explicit and
qualified member-template calls plus out-of-class owner reconstruction; then
validate the related using/inherited/ADL fixtures together.  Keep the alias
and explicit-instantiation groups separate unless a shared owner-state fix
proves to cover them without changing the completed value/layout behavior.


## Historical 62/215 failure map and prior checkpoint scope

The superseded pre-entity report was **62/215 passing** with assignments
through PA20 passing.  Its complete **153-test** failure set was inspected
before implementation: 138 exit-status mismatches, 14 relaxed-LowIR
mismatches, and one invalid-LowIR result.  There were no timeout failures.  The
paths below are retained as historical provenance for that earlier checkpoint,
not as the current PA21 result.

### A — remaining class partial-specialization matching and ordering (34)

`general/100-partial-specialization-pack-expansion-value-pattern`,
`general/100-reference-shell-out-of-class-current-specialization-iterator`,
`general/100-relative-qualified-partial-specialization`,
`general/100-rvalue-reference-binds-converted-temporary`,
`general/200-partial-specialization-nested-template-id-pack-expansion`,
`general/400-bool-or-dependent-member-type-conditional-base`,
`general/400-cv-qualified-template-id-wrapper-class-partial-specialization`,
`general/400-defaulted-nested-cv-template-template-partial-specialization`,
`general/400-dependent-owner-member-class-template-partial-specialization`,
`general/400-forward-function-type-partial-specialization-pack-arity`,
`general/400-forward-primary-partial-switch-value`,
`general/400-function-type-cv-partial-specialization`,
`general/400-function-type-partial-specialization-preference`,
`general/400-function-type-ref-qualified-partial-specialization`,
`general/400-nested-function-type-argument-partial-specialization`,
`general/400-nontype-pack-fixed-tail-partial-specialization-ordering`,
`general/400-partial-specialization-concrete-namespace-argument-order`,
`general/400-partial-specialization-conversion-operator-pointer-binding`,
`general/400-partial-specialization-cv-ref-ordering`,
`general/400-partial-specialization-fixed-nontype-cv-ref`,
`general/400-partial-specialization-member-template-id-cv-mismatch`,
`general/400-partial-specialization-nontype-pattern-order`,
`general/400-repeated-pack-partial-specialization-ordering`,
`general/400-trailing-pack-partial-specialization-common-type-member`,
`general/400-void-head-pack-partial-specialization-ordering`,
`spec/100-function-type-pack-partial-specialization-replay`,
`spec/100-function-type-top-level-cv-partial-specialization`,
`spec/100-inline-namespace-qualified-template-id-pack-expansion`,
`spec/100-nontype-static-outdef-value-member-preserves-type`,
`spec/100-partial-specialization-cv-pointer-selection`,
`spec/100-qualified-function-type-partial-specialization`,
`spec/100-variadic-base-pack-expansion`,
`spec/300-defaulted-type-arg-specialization-nontype-value`, and
`spec/300-variable-template-defaulted-trailing-specialization`.

### B — alias/variable-template entities and template-template binding (34)

`general/200-adl-template-template-argument-namespace`,
`general/200-deferred-incomplete-member-layout-alias-value-type`,
`general/200-dependent-remove-cv-transform-alias-substitution`,
`general/200-qualified-template-template-variable-partial`,
`general/300-member-template-as-template-template-argument`,
`general/300-template-template-trailing-pack-rebind-function-pointer`,
`general/300-variable-template-forwarding-partial-top-cv`,
`general/300-variable-template-run-specialization-selection`,
`general/400-alias-nontype-expression-declaration-scope`,
`general/400-alias-nontype-pack-partial-specialization-pattern`,
`general/400-alias-pack-nontype-expression-fast-path`,
`general/400-alias-rebind-partial-specialization-shadow`,
`general/400-alias-template-decltype-greater-type-argument`,
`general/400-alias-template-decltype-member-type-argument`,
`general/400-alias-template-decltype-shift-type-argument`,
`general/400-alias-template-nontype-shadowed-by-inner-value`,
`general/400-alias-template-pack-id-preserves-syntax`,
`general/400-alias-template-pointer-cv-cache-distinction`,
`general/400-alias-value-expression-type-argument`,
`general/400-concrete-template-head-beats-template-template-pack`,
`general/400-dependent-alias-helper-partial-specialization`,
`general/400-dependent-alias-member-template-id-defer`,
`general/400-dependent-pack-typename-nontype-expression`,
`general/400-function-parameter-pack-alias-expansion`,
`general/400-member-alias-template-owner-rebind-cache`,
`general/400-member-alias-template-template-dependent-replay`,
`general/400-member-template-nontype-shadowed-global-replay`,
`general/400-nontype-pack-comma-expression-syntax`,
`general/400-nttp-pack-void-comma-expression`,
`general/400-qualified-base-type-alias-from-nontype-pack`,
`general/400-qualified-template-id-current-scope-alias-shadow`,
`general/400-template-template-arity-incomplete-partial`,
`general/400-template-template-fixed-prefix-pack-order`, and
`spec/200-template-template-parameter-arity-mismatch`.

### C — member/friend-template owner and lookup replay (60)

The 59 current paths in the historical C group remain failures, plus
`spec/300-friend-class-template-nested-private-typedef-access`; they cover
member/friend declarations, inherited lookup, ADL/overload selection, nested
owners, and out-of-class definitions.

### D — explicit specialization/instantiation ownership (12)

The 12 current paths in the historical D group remain failures; they cover
explicit specialization ordering, explicit instantiation, and extern-template
materialization.

### E — dependent constants, layout, and ordinary lowering (13)

The 13 current paths in the historical E group remain failures; they cover
dependent constant replay, object layout, and LowIR ownership.

### Checkpoint Scope

This turn takes Group B as one substantial coherent increment: represent
template-template parameters and arguments as typed template entities with
arity/pack/default matching; make alias templates resolve through the same
argument-normalization path as class templates; select and materialize
variable-template partial specializations; and preserve typed non-type values
and dependent `decltype`/pack expressions while replaying aliases.  Validation
will cover the Group B fixtures, then the full PA21 report, through-PA20
report, and file audit.

## Turn-start baseline

The turn-start PA21 report was **47/215 tests passing**, with assignments
through PA20 passing.  The complete current-PA failure set was read from the
required report before implementation: **168 failures** consisting of 141
exit-status mismatches, 24 relaxed-LowIR mismatches, one invalid LowIR, and
two timeouts.  The paths below are grouped by the shared compiler behavior
they expose; each failing path appears once.

## Remaining Work Map recorded before implementation

### A — class partial-specialization identity, matching, and ordering

These fixtures exercise the class-template declaration graph, typed
specialization keys, current-specialization rewriting, pack/reference/cv
pattern matching, and selection of the owning partial-specialization body:

`general/100-nested-pack-expansion-outer-type-pack`,
`general/100-partial-specialization-member-typedef-outdef`,
`general/100-partial-specialization-nested-template-id-member-outdef`,
`general/100-partial-specialization-pack-expansion-value-pattern`,
`general/100-reference-shell-out-of-class-current-specialization-iterator`,
`general/100-relative-qualified-partial-specialization`,
`general/100-rvalue-reference-binds-converted-temporary`,
`general/100-top-cv-pointer-does-not-match-unqualified-pointer-partial`,
`general/200-partial-specialization-nested-template-id-pack-expansion`,
`general/400-bool-or-dependent-member-type-conditional-base`,
`general/400-cv-qualified-template-id-wrapper-class-partial-specialization`,
`general/400-defaulted-nested-cv-template-template-partial-specialization`,
`general/400-dependent-owner-member-class-template-partial-specialization`,
`general/400-forward-function-type-partial-specialization-pack-arity`,
`general/400-forward-primary-partial-switch-value`,
`general/400-function-type-cv-partial-specialization`,
`general/400-function-type-partial-specialization-preference`,
`general/400-function-type-ref-qualified-partial-specialization`,
`general/400-nested-function-type-argument-partial-specialization`,
`general/400-nontype-pack-fixed-tail-partial-specialization-ordering`,
`general/400-partial-specialization-concrete-namespace-argument-order`,
`general/400-partial-specialization-conversion-operator-pointer-binding`,
`general/400-partial-specialization-cv-ref-ordering`,
`general/400-partial-specialization-fixed-nontype-cv-ref`,
`general/400-partial-specialization-member-template-id-cv-mismatch`,
`general/400-partial-specialization-nontype-pattern-order`,
`general/400-partial-specialization-redecl-member-template-empty-pack`,
`general/400-repeated-argument-partial-specialization-ordering`,
`general/400-repeated-pack-partial-specialization-ordering`,
`general/400-variadic-template-pack-under-lvalue-reference-partial`,
`general/400-void-head-pack-partial-specialization-ordering`,
`spec/100-class-partial-specialization-array-size`,
`spec/100-class-partial-specialization-unbounded-array-mismatch`,
`spec/100-function-type-pack-partial-specialization-replay`,
`spec/100-function-type-top-level-cv-partial-specialization`,
`spec/100-inline-namespace-qualified-template-id-pack-expansion`,
`spec/100-nontype-static-outdef-value-member-preserves-type`,
`spec/100-partial-specialization-concrete-type-shadows-parameter-name`,
`spec/100-partial-specialization-cv-pointer-selection`,
`spec/100-qualified-function-type-partial-specialization`,
`spec/100-variadic-base-pack-expansion`,
`spec/200-template-template-parameter-nested-partial-specialization`,
`spec/300-defaulted-type-arg-specialization-nontype-value`, and
`spec/300-variable-template-defaulted-trailing-specialization`.

### B — alias/variable-template entities and template-template arguments

These expose missing canonical template entities, template-template arity and
pack binding, alias expansion, dependent non-type expression scope, and
variable-template specialization selection:

`general/200-adl-template-template-argument-namespace`,
`general/200-deferred-incomplete-member-layout-alias-value-type`,
`general/200-dependent-remove-cv-transform-alias-substitution`,
`general/200-qualified-template-template-variable-partial`,
`general/300-member-template-as-template-template-argument`,
`general/300-template-template-trailing-pack-rebind-function-pointer`,
`general/300-variable-template-forwarding-partial-top-cv`,
`general/300-variable-template-run-specialization-selection`,
`general/400-alias-nontype-expression-declaration-scope`,
`general/400-alias-nontype-pack-partial-specialization-pattern`,
`general/400-alias-pack-expansion-through-alias`,
`general/400-alias-pack-nontype-expression-fast-path`,
`general/400-alias-rebind-partial-specialization-shadow`,
`general/400-alias-template-decltype-greater-type-argument`,
`general/400-alias-template-decltype-member-type-argument`,
`general/400-alias-template-decltype-shift-type-argument`,
`general/400-alias-template-nontype-shadowed-by-inner-value`,
`general/400-alias-template-pack-id-preserves-syntax`,
`general/400-alias-template-pointer-cv-cache-distinction`,
`general/400-alias-value-expression-type-argument`,
`general/400-concrete-template-head-beats-template-template-pack`,
`general/400-dependent-alias-helper-partial-specialization`,
`general/400-dependent-alias-member-template-id-defer`,
`general/400-dependent-pack-typename-nontype-expression`,
`general/400-function-parameter-pack-alias-expansion`,
`general/400-member-alias-template-owner-rebind-cache`,
`general/400-member-alias-template-template-dependent-replay`,
`general/400-member-alias-template-template-empty-template-id-argument`,
`general/400-member-template-nontype-shadowed-global-replay`,
`general/400-nontype-pack-comma-expression-syntax`,
`general/400-nttp-pack-void-comma-expression`,
`general/400-qualified-base-type-alias-from-nontype-pack`,
`general/400-qualified-template-id-current-scope-alias-shadow`,
`general/400-template-template-arity-incomplete-partial`,
`general/400-template-template-fixed-prefix-pack-order`,
`spec/200-template-template-parameter-arity-mismatch`.

### C — member-template, friend-template, and owner/lookup replay

These require member declarations and namespace-scope friends to retain their
active class specialization, lexical scope, overload identity, and generated
owner through out-of-class definitions and calls:

`general/300-basic-template-operator-overloads`,
`general/300-class-template-hidden-friend-body-lexical-scope`,
`general/300-class-template-member-overload-braced-call-operator`,
`general/300-dependent-base-qualified-rvalue-assignment`,
`general/300-dependent-hidden-friend-static-member-definition`,
`general/300-explicit-member-call-function-template-id`,
`general/300-explicit-member-function-template-call`,
`general/300-explicit-member-template-id-shares-ordinary-overload`,
`general/300-explicit-type-arg-decltype-member-access`,
`general/300-friend-existing-template-private-ctor-access`,
`general/300-friend-function-template-access`,
`general/300-function-pack-template-id-deduction-decltype`,
`general/300-inherited-member-template-subscript-action`,
`general/300-local-qualified-argument-replay`,
`general/300-member-class-explicit-specialization-owner-lookup`,
`general/300-member-template-assignment-operator-value`,
`general/300-member-template-class-pack-forward-before-token`,
`general/300-member-template-local-using-does-not-suppress-adl`,
`general/300-namespace-function-template-hides-outer-callable-object`,
`general/300-nested-class-friend-template-namespace-scope`,
`general/300-nested-class-template-current-owner-lookup`,
`general/300-nested-class-template-reference-reset`,
`general/300-nested-member-partial-specialization-apply-scope`,
`general/300-nested-member-partial-specialization-survives-reference-reset`,
`general/300-nondependent-member-template-id-call`,
`general/300-out-of-class-ctor-using-imported-member-template`,
`general/300-out-of-class-member-function-template-definition`,
`general/300-out-of-class-member-template-namespace-typedef`,
`general/300-parenthesized-qualified-template-functional-call`,
`general/300-qualified-friend-member-template-access`,
`general/300-qualified-member-class-explicit-specialization`,
`general/300-qualified-preselected-member-template-call`,
`general/300-reference-member-class-template-visible`,
`general/300-reference-shell-nested-class-template-reuse`,
`general/300-sibling-namespace-dependent-member-template-id-owner`,
`general/300-single-pack-cast-target`,
`general/300-template-friend-class-constructor-access`,
`general/300-templated-constructor-special-member-collection`,
`general/300-unary-member-operator-template-default`,
`general/300-using-base-same-signature-derived-template-preferred`,
`spec/300-const-member-function-template-overload`,
`spec/300-friend-class-template-protected-base-access`,
`spec/300-hidden-friend-template-call-adl`,
`spec/300-hidden-friend-template-operator-adl`,
`spec/300-member-call-template-hides-inherited-instantiation`,
`spec/300-member-class-template-out-of-class`,
`spec/300-member-operator-template-active-owner`,
`spec/300-member-operator-template-in-class-template`,
`spec/300-member-template-cache-hit-concrete-scope`,
`spec/300-noexcept-member-template-call-operator`,
`spec/300-out-of-class-member-template-owner-param-rename`,
`spec/300-out-of-class-member-template-tag-dispatch-definition`,
`spec/300-out-of-class-overloaded-member-template-definition`,
`spec/300-qualified-friend-function-template-member-access`,
`spec/300-qualified-member-template-hides-base-function`,
`spec/300-rooted-qualified-static-data-member-template-definition`,
`spec/300-static-member-function-template-out-of-class`,
`spec/300-template-friend-inside-class-template`,
`spec/300-using-imported-member-template-active-owner`, and
`spec/300-using-inherited-alias-operator-template`.

### D — explicit specialization, explicit instantiation, and extern ownership

These require declaration-state ownership and ordering to distinguish primary,
explicit-specialization, explicit-instantiation, and extern-template forms:

`general/300-template-instantiation-use-location-explicit-specialization`,
`spec/300-explicit-instantiation-class`,
`spec/300-explicit-instantiation-deduced-member-function-template`,
`spec/300-explicit-instantiation-free-function-emits-definition`,
`spec/300-explicit-instantiation-function`,
`spec/300-explicit-instantiation-static-member-function`,
`spec/300-explicit-specialization-after-instantiation`,
`spec/300-explicit-specialization-member-function`,
`spec/300-explicit-specialization-out-of-class-member-overrides-primary`,
`spec/300-explicit-specialization-static-data-member`,
`spec/300-explicit-specialized-ctor-template-header-bad`,
`spec/300-extern-template-constructor-declaration`,
`spec/300-extern-template-function-call-suppresses-materialization`, and
`spec/300-extern-template-operator-function-declaration`.

### E — dependent expression, constant/value replay, and ordinary lowering

These are the remaining cross-cutting cases where the template graph reaches
constant evaluation, object layout, overload deduction, or LowIR ownership:

`general/200-lazy-header-constexpr-static-assert`,
`general/300-anonymous-union-storage-constructor-noop`,
`general/300-array-functional-cast-pack-call`,
`general/300-constexpr-static-fn-template-address-pack`,
`general/300-crtp-static-cast-reference-before-constructor-template`,
`general/300-function-signature-partial-specialization-functor-assignment`,
`general/300-function-template-local-static-per-specialization`,
`general/300-hidden-friend-template-call-adl`,
`general/300-static-constexpr-function-template-pointer-array`,
`general/400-local-value-shadows-template-relational`,
`general/400-out-of-namespace-class-template-member-result`,
`general/400-reference-member-depth-pack-sum`,
`general/400-reference-member-lookup-in-progress-base-typedef`, and
`spec/300-dependent-super-member-template-chain`.

The two timeout entries are retained in their owning groups above rather than
hidden: one is an owner/replay cycle and one is a specialization-ordering
cycle.  The invalid-LowIR entry is also retained in Group E.

## Checkpoint Scope

This checkpoint implements Group A's first substantial, coherent increment:

- canonicalize and retain typed class-partial-specialization patterns and
  their primary-template parameter contract;
- match nested template-ids, cv/ref/pointer forms, fixed and variadic packs,
  and non-type values without treating a concrete type as a deduced parameter;
- rank all matching partial-specialization candidates by specificity instead
  of selecting the first registered candidate;
- use the selected specialization as the active owner when materializing its
  class body and out-of-class members; and
- preserve current-specialization identity when rewriting qualified members,
  aliases, and nested declarations.

Validation scope is the Group A 100/200/spec probes plus the two repeated
argument/pack ordering fixtures listed above, followed by the full PA21 report
and the through-PA20 report.  The implementation stays in
the existing typed template/entity graph and ordinary LowIR path; it does not
special-case fixture names or invoke another compiler.

## Checkpoint Result

Completed the first Group A increment.  The matcher now retains typed array
and bound structure, distinguishes pointer pointee cv from top-level cv, and
supports repeated nested packs, template-template base bindings, and
defaulted trailing primary arguments.  Candidate partials are ranked by
structural specificity; the selected class declaration and its out-of-class
member owner are then used for materialization, with current-specialization
identity preserved during qualified rewrites, aliases, and nested declarations.

Validation is green for the six targeted Group A probes (6/6), the exact
through-PA20 report (1635/1635), and the PA21 file audit.  The required full
PA21 report is now 62/215, above the checkpoint baseline of 57/215.  Its 153
failures are 135 expected-success exit mismatches, three expected-failure
exit mismatches, 14 relaxed-LowIR mismatches, and one invalid-LowIR result.
There are no timeout failures.  The audit refactor also preserves the two
additional semantic passes already present after the checkpoint:
general/300-dependent-friend-alias-private-constructor-access and
general/300-dependent-friend-self-private-constructor-access.

## Refreshed Remaining Work Map

The complete current-PA21 report has 153 failing paths, classified exhaustively
by the next semantic owner:

- Group A — 34: remaining partial-specialization matching and ordering,
  function/reference/cv patterns, pack/value tails, and nested current-owner
  replay.
- Group B — 34: alias and variable-template entities, template-template
  binding/arity, dependent non-type expressions, and argument normalization.
- Group C — 60: member/friend templates, inherited lookup, overload/ADL
  selection, nested owners, and out-of-class definition replay.  This includes
  the two friend/private-typedef paths that were absent from the historical
  inventory.
- Group D — 12: explicit specialization, explicit instantiation, and
  extern-template ownership/order.
- Group E — 13: dependent constant/expression replay, layout, and ordinary
  lowering/LowIR ownership.

The failure counts include every path in the required report; no timeout or
architecture/audit blocker is being carried forward as assignment work.

## Next Checkpoint Group

The next substantial checkpoint is Group B: alias/variable-template entities
and template-template-parameter binding, with the shared typed argument
normalization and dependent non-type expression paths.
