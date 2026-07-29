# PA23 checkpoint plan

## Checkpoint 1 scope — 2026-07-28 (before implementation)

### Baseline

The turn-start required PA23 report is **267/396**.  Assignments through
PA22 pass.  The complete current-PA failure set is **129 fixtures**: 106
exit-status mismatches, 21 LowIR comparison mismatches, and two timeouts.
The primary failure map below names every failing fixture and groups it by the
shared compiler behavior that owns the next fix; some fixtures exercise more
than one group.

### Remaining Work Map

- **Dependent qualified lookup, owner propagation, and member-template replay:**
  the compiler loses a typed owner or source namespace while resolving a
  qualified alias/member/template-id, or falls back to a short name after a
  specialization is materialized.  This includes:

  `general/100-default-nontype-qualified-function-lookup.t`,
  `general/100-dependent-bool-partial-static-value-storage.t`,
  `general/100-direct-namespace-wins-over-using-directive.t`,
  `general/100-local-qualified-argument-replay.t`,
  `general/100-member-template-specialization-return-prefers-member-call.t`,
  `general/100-using-directive-template-member-type-typedef.t`,
  `general/100-dependent-qualified-nontype-base-argument.t`,
  `general/100-inherited-using-alias-out-of-class-specialization-member.t`,
  `general/200-empty-pack-member-template-owner-key.t`,
  `general/200-member-function-template-address-explicit-pack.t`,
  `general/200-member-template-implicit-instantiation-not-overload.t`,
  `general/200-nested-template-id-partial-specialization-deduction.t`,
  `general/300-array-qualified-member-type-sfinae.t`,
  `general/300-hidden-friend-current-specialization-enable-if.t`,
  `general/300-qualified-explicit-template-alias-return-sfinae.t`,
  `general/400-current-specialization-display-name-member-alias.t`,
  `general/400-dependent-nontype-member-template-owner.t`,
  `general/400-inherited-qualified-member-template-type.t`,
  `general/400-member-alias-template-template-owner-argument.t`,
  `general/400-member-template-result-pack-preserves-nested-function-pointer-owner.t`,
  `general/400-nested-member-template-base-param-shadow-value.t`,
  `general/400-out-of-class-ctor-using-imported-member-template.t`,
  `general/400-qualified-member-variable-template-class-value.t`,
  `general/400-recursive-member-template-concrete-owner.t`,
  `general/400-unused-static-member-template-return-type.t`,
  `general/500-concrete-enable-if-nontype-parameter-type-sfinae.t`,
  `general/500-constructor-sfinae-owner-pack-function-type.t`,
  `general/500-dependent-decltype-expression-formation-scope.t`,
  `general/500-dependent-function-type-pack-expansion-ctor-init.t`,
  `general/500-dependent-qualified-member-template-result-bool.t`,
  `general/500-dependent-qualified-sizeof-static-member.t`,
  `general/500-member-template-conditional-alias-trailing-return.t`,
  `general/500-mp11-append-alias-template-sfinae.t`,
  `general/500-out-of-class-member-template-dependent-owner-type.t`,
  `general/500-partial-member-template-trailing-result-scope.t`,
  `general/500-recursive-qualified-member-template-bool-arg.t`,
  `general/500-source-namespace-base-sfinae-chain.t`,
  `spec/100-dependent-template-id-qualified-member-source-owner.t`,
  `spec/200-inherited-template-param-shadow-forward.t`,
  `spec/400-defaulted-nested-class-argument-partial-specialization.t`,
  `spec/400-dependent-member-template-call.t`,
  `spec/400-explicit-pack-type-argument-uses-bound-type.t`,
  `spec/400-explicit-type-arg-dependent-qualified-member-template-id.t`,
  `spec/400-function-type-pack-template-argument.t`,
  `spec/500-conditional-alias-index-sequence-member-template-call.t`, and
  `spec/500-standard-enable-if-member-use-scope.t`.

- **Function-template deduction, reference/array normalization, and overload
  ranking:** typed argument categories, function signatures, explicit prefixes,
  packs, defaults, conversions, and partial ordering do not remain intact
  across the combined call path.  This includes:

  `general/100-explicit-function-specialization-overload-parameter-match.t`,
  `general/100-function-type-not-pointer-partial-specialization.t`,
  `general/100-nontype-function-parameter-adjustment.t`,
  `general/200-adl-explicit-template-id-call.t`,
  `general/200-constructor-template-parameter-shadows-instantiated-type.t`,
  `general/200-forwarding-pack-cast-trailing-return.t`,
  `general/200-forwarding-pack-pointer-cast-trailing-return.t`,
  `general/200-forwarding-reference-deduction.t`,
  `general/200-function-template-forwarding-pack-array-ref.t`,
  `general/200-function-template-named-parameter-sfinae.t`,
  `general/200-function-template-reference-cv-alias-partial-order.t`,
  `general/200-function-template-template-parameter-deduction.t`,
  `general/200-member-operator-template-reference-pattern-partial-order.t`,
  `general/200-pack-not-at-end-nondeduced-bad.t`,
  `general/200-template-template-qualified-default-arg-deduction.t`,
  `general/300-constructor-template-const-ref-enable-if-conversion.t`,
  `general/300-decltype-member-template-callable-pack-result-cache.t`,
  `general/400-function-type-partial-specialization-fixed-arity-over-pack.t`,
  `general/400-function-type-tail-pack-recursive-specialization.t`,
  `general/400-static-cast-rvalue-ref-skips-conversion-operator.t`,
  `spec/100-function-template-nontype-function-pointer-call.t`,
  `spec/100-function-template-nontype-function-pointer-specialization-call.t`,
  `spec/100-local-member-call-constructor-template-instantiation.t`,
  `spec/100-nontype-function-pointer-argument.t`,
  `spec/300-constructor-default-pack-partial-ordering.t`,
  `spec/300-trailing-return-expression-sfinae-default-param.t`,
  `spec/400-conversion-function-template-call-argument.t`,
  `spec/400-conversion-function-template-copy-init.t`, and
  `spec/400-conversion-function-template-selection.t`.

- **Substitution failure, deferred instantiation, and partial-specialization
  viability:** invalid immediate-context forms must discard only the candidate
  being tested, while dependent `decltype`, `enable_if`, detector, deleted
  function, and no-eager class probes retain their typed substitution scope.
  This includes:

  `general/100-decltype-value-category.t`,
  `general/100-nested-template-static-value-nontype-expression.t`,
  `general/100-selected-specialization-special-member-body.t`,
  `general/100-structured-bool-boost-convertible-mpl-overload.t`,
  `general/200-class-partial-specialization-no-derived-base-deduction.t`,
  `general/300-sfinae-primary-static-const-value-fold.t`,
  `general/300-variadic-disjunction-conjunction-nontype-bool.t`,
  `general/400-dependent-alias-nontype-sequence-filter.t`,
  `general/400-dependent-less-than-enable-if-alias-type-id.t`,
  `general/400-dependent-less-than-enable-if-nontype-parameter.t`,
  `general/400-elaborated-type-template-arg-false-branch.t`,
  `general/400-elaborated-type-template-arg-true-branch.t`,
  `general/400-local-class-default-member-variable-template-nontype-type.t`,
  `general/400-out-of-class-member-template-cache-reset.t`,
  `general/500-array-type-argument-sfinae-static-value.t`,
  `general/500-async-initiate-cached-sfinae-pack-return.t`,
  `general/500-const-reference-alias-default-sfinae.t`,
  `general/500-dependent-std-or-enable-if-defers.t`,
  `general/500-nontype-alias-reinstantiation-structural-state.t`,
  `general/500-reentrant-static-query-callable-enable-if-cache.t`,
  `general/500-reentrant-static-query-enable-if-partial.t`,
  `general/500-type-pack-rejects-value-pack-bad.t`,
  `spec/100-extern-template-member-function-declaration.t`,
  `spec/100-extern-template-static-data-declaration.t`,
  `spec/300-deleted-function-template-callee-detector-sfinae.t`, and
  `spec/300-deleted-function-template-expression-sfinae.t`.

- **Alias/template-template/pack and typed non-type values:** aliases and
  packs sometimes lose their parameter kind, while integral, enum, pointer,
  reference, `sizeof`, and variable-template values are evaluated with the
  wrong typed state.  The remaining fixtures in this band are:

  `general/100-explicit-specialization-pointer-member-definition.t`,
  `general/100-intermediate-type-transform-value-nontype.t`,
  `general/100-sizeof-call-result-nontype-template-argument.t`,
  `general/200-function-template-template-parameter-deduction.t`,
  `general/300-variable-template-default-enable-if-viability.t`,
  `general/400-explicit-function-template-type-arg-drops-nontype-overload.t`,
  `general/400-local-class-default-member-variable-template-nontype-type.t`,
  `general/400-member-variable-template-leaf-sfinae.t`,
  `general/400-nonmember-template-compound-assignment-const-lhs.t`,
  `general/400-variable-template-specializations.t`,
  `general/500-concrete-enable-if-nontype-parameter-type-sfinae.t`,
  `general/500-dependent-qualified-sizeof-static-member.t`,
  `general/500-type-pack-rejects-value-pack-bad.t`,
  `spec/100-sizeof-union-type-nttp.t`,
  `spec/200-defaulted-class-template-argument-pack-prefix-deduction.t`,
  `spec/400-class-template-nttp-scope-value.t`,
  `spec/400-qualified-member-template-id-bool-constant.t`.

- **Explicit specialization, extern/constructor replay, and LowIR
  materialization:** the semantic selection is often close, but generated
  declarations, object roots, hidden constructors, argument slots, and signed
  or unsigned LowIR facts still differ.  The comparison failures are:

  `general/100-current-specialization-member-body-cast-compare.t`,
  `general/100-explicit-specialization-out-of-class-ctor-replay.t`,
  `general/100-explicit-specialization-pointer-member-definition.t`,
  `general/100-inherited-using-alias-out-of-class-specialization-member.t`,
  `general/100-intermediate-type-transform-value-nontype.t`,
  `general/100-sizeof-call-result-nontype-template-argument.t`,
  `general/200-adl-explicit-template-id-call.t`,
  `general/200-function-template-reference-cv-alias-partial-order.t`,
  `general/200-function-template-template-parameter-deduction.t`,
  `general/200-member-operator-template-reference-pattern-partial-order.t`,
  `general/300-current-specialization-constructor-template-canonical-owner.t`,
  `general/300-dependent-bool-base-trait-type-argument.t`,
  `general/300-variable-template-default-enable-if-viability.t`,
  `general/400-explicit-function-template-type-arg-drops-nontype-overload.t`,
  `general/400-function-type-pack-out-of-class-constructor.t`,
  `general/400-member-variable-template-leaf-sfinae.t`,
  `general/400-nonmember-template-compound-assignment-const-lhs.t`,
  `general/400-out-of-class-partial-member-template-owner-parameter-alias.t`,
  `general/400-variable-template-specializations.t`,
  `general/500-dependent-typename-enable-if-candidate.t`,
  `general/500-dependent-typename-member-enable-if-return.t`,
  `spec/100-explicit-instantiation-after-explicit-specialization-no-effect.t`,
  `spec/100-explicit-instantiation-class-prior-member-definitions.t`,
  `spec/100-explicit-specialization-out-of-class-ctor-replay.t`,
  `spec/100-explicit-specialization-out-of-class-member-emits.t`,
  `spec/100-out-of-class-conversion-operator-definition.t`,
  `spec/100-partial-specialization-member-primary-param-name.t`,
  `spec/200-defaulted-class-template-argument-pack-prefix-deduction.t`,
  `spec/300-constructor-default-pack-partial-ordering.t`,
  `spec/300-deleted-function-template-callee-detector-sfinae.t`,
  `spec/300-deleted-function-template-expression-sfinae.t`,
  `spec/400-defaulted-template-arg-partial-base-completion.t`, and
  `spec/400-template-template-member-alias-owner-shadow.t`.

The last two maps intentionally overlap at feature boundaries; the union is
the exact 129-fixture failure set from the baseline report, including the
two timeout fixtures.  The next checkpoints will consume the remaining
deduction/ranking band, then the SFINAE/deferred band, then the alias/owner/
typed-NTTP and final LowIR materialization band.

### Checkpoint Scope

Implement cycle-safe dependent member-type resolution for reentrant class
partial-specialization matching.  A typed lookup request must be guarded by a
stable semantic identity across `FindClassMemberType`, `ResolveAlias`, and
`SelectClassTemplateDefinition`; when the same dependent query re-enters, it
must return an unresolved candidate state or substitution failure instead of
recursively growing a generated spelling.  Preserve normal alias expansion,
partial selection, and concrete owner propagation for non-reentrant queries.

Validate this scope with both timeout fixtures, the related
`general/500-dependent-std-or-enable-if-defers.t`,
`general/500-nontype-alias-reinstantiation-structural-state.t`,
`general/500-recursive-qualified-member-template-bool-arg.t`,
`general/500-dependent-qualified-member-template-result-bool.t`, and
`general/300-decltype-member-template-callable-pack-result-cache.t`, plus the
full PA23 report, the required through-PA22 report, and the PA23 file audit.
The next checkpoint group is the broader general/300 and spec/300
substitution/deferred-instantiation band, bundled with any newly exposed
reentrant owner cases.

## Checkpoint 1 result — 2026-07-28

The originally proposed reentrant lookup guard was investigated against both
timeout traces but not forced into the compiler: the attempted guards did not
cover the changing generated spellings, and one variant regressed integral
evaluation.  That timeout group remains deferred for a later checkpoint.

The completed scope was narrowed to two stable semantic facts:

- `EvaluateDecltypeExpression` now carries the lvalue category through an
  outer parenthesized operand, so `decltype((object))` produces the reference
  type required by the expression category.
- Class specialization matching now preserves reference identity for concrete
  arguments.  A concrete `X<int>` specialization no longer also matches
  `X<int&>` or `X<int&&>`, while dependent patterns retain the existing
  deduction behavior.

Validation of the focused scope passed:

`general/100-decltype-value-category.t`,
`general/200-forwarding-reference-deduction.t`,
`general/100-explicit-specialization-pointer-cv-distinction.t`,
`general/400-dependent-default-nontype-argument-eval.t`, and
`general/400-dependent-member-alias-template-argument-scope.t`.

The build-backed PA23 report is **269/396** (up from 267/396), with no new
failures.  The remaining set is the baseline map minus
`general/100-decltype-value-category.t` and
`general/200-forwarding-reference-deduction.t`, for **127 fixtures**.  Earlier
PAs remain the required next validation gate.

### Remaining Work Map after Checkpoint 1

- **Reentrant/deferred SFINAE:** the two timeout fixtures and the dependent
  `enable_if`/detector/static-query cases still need a stable semantic query
  identity and scoped substitution failure.
- **Function-template packs and overloads:** forwarding-pack trailing-return,
  array-reference deduction, function-type partial ordering, conversions, and
  explicit function specialization selection remain grouped for the next
  deduction increment.
- **Qualified owner/member replay:** dependent aliases, inherited member
  templates, using directives, nested template ids, and current-specialization
  owner propagation remain the largest exit-status group.
- **Typed alias/NTTP state:** variable templates, integral/pointer/reference
  arguments, `sizeof`, default arguments, and pack-kind validation remain
  unresolved.
- **LowIR/materialization:** the checked comparison failures are concentrated
  in explicit/extern specialization replay, constructors, member emissions,
  function pointers, and generated declaration ordering.

### Next Checkpoint Group

Implement the function-template deduction increment covering forwarding packs,
array/reference adjustment, and trailing-return replay.  Validate it first with
the three remaining forwarding-pack fixtures and the related function-type and
conversion fixtures, then rerun the full PA23 report and the through-PA22
report before moving to the deferred-SFINAE group.

## Checkpoint 2 result — 2026-07-28

### Checkpoint Scope

Completed the forwarding-pack/trailing-return integration group.  The compiler
now preserves the typed distinction between a forwarding `T&&` and a deduced
lvalue `T&`, including array references and nested pointer cv-qualification;
binds an explicit template prefix when a type pack precedes a deduced fixed
parameter; expands dependent call and cast operands that carry a pack; and
replays trailing return types in the function's substitution context.  The
same path now keeps qualified `async_result<...>::initiate` calls as member
calls, queues generated free functions in their lexical owner, and retains
dependent `decltype` expression children until semantic evaluation rather than
turning them into an empty declaration node.

### Validation

The focused forwarding-pack, pointer-cast, array-reference, and cached async
SFINAE fixtures pass.  The PA22 trailing-pack regression fixture also passes.
The required through-PA22 report is **2100/2100**, and the full PA23 report is
**274/396** (up from the turn-start **267/396**).  The remaining PA23 set is
**122 fixtures**: 87 exit-status mismatches, 35 LowIR mismatches, and the two
reentrant static-query timeouts.

### Remaining Work Map

- **Qualified owner/member replay:** the largest status group still loses
  dependent owners or source namespaces across aliases, inherited/using member
  templates, out-of-class definitions, and nested specialization replay.
- **SFINAE and deferred instantiation:** detector/`enable_if`, dependent
  `decltype`, cached result queries, deleted candidates, and the two reentrant
  static-query cases still need scoped substitution state and cycle-safe query
  identity.
- **Deduction and overload composition:** conversion candidates, function-type
  partial ordering, template-template arguments, defaults, and remaining
  reference/cv normalization cases still fail in selection or LowIR replay.
- **Typed aliases and non-type values:** variable templates, `sizeof`, pointer/
  reference/integral values, pack-kind validation, and defaulted arguments still
  lose typed semantic facts.
- **Materialization:** the remaining comparison failures are concentrated in
  explicit/extern specialization replay, constructors, member emission, and
  generated declaration/argument metadata.

### Next Checkpoint Group

Take the non-timeout general/spec SFINAE and deferred-instantiation fixtures as
the next substantial group, preserving candidate-local substitution failure and
no-eager body instantiation.  Revisit the reentrant static-query timeout pair
only with a stable semantic query identity, then rerun the complete PA23 report
and the through-PA22 gate.

## Checkpoint 3 result — 2026-07-28

### Remaining Work Map

The current report has **112 failures** (including the two reentrant
static-query timeouts), down from the turn-start **129 failures**.  The shared
remaining behaviors are qualified owner/member replay, candidate-local SFINAE
and deferred queries, function deduction/conversion composition, typed alias
and non-type value preservation, and final specialization/materialization
metadata.  The complete fixture-level map remains in the first checkpoint;
this increment completed the stable SFINAE/type-substitution sub-group.

### Completed Scope

The compiler now preserves typed arguments while forming dependent aliases and
`enable_if` probes, distinguishes relational `<` from nested template angles in
dependent argument lists, validates candidate-local `enable_if` and default
parameter substitution, and replays complex dependent member calls through
their concrete owner.  The focused validation passed 6/8 fixtures:

`general/300-variable-template-default-enable-if-viability.t`,
`general/400-dependent-less-than-enable-if-alias-type-id.t`,
`general/400-dependent-less-than-enable-if-nontype-parameter.t`,
`general/500-concrete-enable-if-nontype-parameter-type-sfinae.t`,
`spec/300-trailing-return-expression-sfinae-default-param.t`, and
`spec/500-standard-enable-if-member-use-scope.t`.

The two dependent-typename LowIR fixtures remain unchanged baseline failures
and are explicitly deferred to the next checkpoint.  The required
through-PA22 report is **2100/2100**, the full PA23 report is **284/396** (up
from **267/396**), and the PA23 file audit passes.

### Remaining Work Map

- **Dependent-typename candidate replay:**
  `general/500-dependent-typename-enable-if-candidate.t` and
  `general/500-dependent-typename-member-enable-if-return.t` still select or
  materialize the wrong dependent `enable_if` candidate.
- **Reentrant/deferred SFINAE:** the two timeout fixtures and the remaining
  detector, deleted-candidate, cached-query, and no-eager-instantiation cases
  still need stable query identity and scoped substitution state.
- **Qualified owner/member replay:** dependent aliases, inherited/using member
  templates, nested template ids, and current-specialization owner propagation
  remain the largest exit-status group.
- **Deduction and typed values:** forwarding/conversion composition,
  function-type partial ordering, variable templates, `sizeof`, pointer and
  reference non-type arguments, defaults, and pack-kind validation remain
  unresolved.
- **LowIR/materialization:** explicit/extern specialization replay,
  constructors, member emission, function pointers, and declaration metadata
  still account for the remaining comparison failures.

### Next Checkpoint Group

Resolve the two dependent-typename overload/materialization cases first, then
take the broader non-timeout deferred-SFINAE group.  Validate the focused pair,
the full PA23 report, the through-PA22 gate, and the file audit.

## Checkpoint 4 audit — 2026-07-29

### Checkpoint Scope

Audited the typed-SFINAE checkpoint that landed in `b7f7787`, including its
owner/member replay, specialization matching, materialization protection, and
cycle-identity boundaries.  The audit changes stay in the existing typed AST,
substitution, and registry paths; no test or reference fixture changes were
made.

### Validation

The final PA23 report is **285/396**: 76 exit-status failures, 33 LowIR
comparisons, and the same two reentrant static-query timeout fixtures.  This is
one test above the checkpoint baseline of **284/396**, with no current-only
failures against the clean checkpoint comparison.  The exact through-PA22
gate passes at **2100/2100**, and the PA23 file audit passes with the existing
repository warnings.

### Remaining Work Map

The complete 111-fixture set is recorded in `pa23/audit.md`.  The concise
behavior map is:

- **Qualified owner/member replay (general/100, general/400, and spec/100):**
  namespace and inherited-owner propagation, current-specialization member
  lookup, out-of-class member/constructor replay, and generated declaration
  metadata remain the largest status/materialization band.
- **Deferred substitution and SFINAE (general/300, general/500, spec/300,
  plus the two dependent-typename cases):** candidate-local detection,
  deleted-candidate handling, no-eager instantiation, cached queries, and
  dependent member results remain grouped around typed query state.
- **Deduction and overload composition (general/200, function-oriented
  general/400, and spec/200/spec/400):** packs, defaults, conversion
  candidates, template-template arguments, function-type partial ordering,
  and non-deduced contexts remain grouped for the next deduction increment.
- **Typed aliases and non-type values:** variable/member templates, integral,
  pointer/reference, `sizeof`, boolean, and pack-kind arguments still need
  their existing typed facts carried through replay.
- **LowIR/materialization:** the remaining LowIR comparisons are concentrated
  in explicit/extern specialization replay, constructors, member emission,
  function pointers, and declaration ordering/metadata.

### Next Checkpoint Group

Bundle the two dependent-typename fixtures with the non-timeout deferred-SFINAE
set: the detector/deleted-candidate cases, cached/no-eager query cases, and
the dependent member-result cases.  Keep the two timeout fixtures as stress
witnesses for that same typed query-identity path; do not alter their harness
timeout or add a timeout-specific acceptance path.

## Checkpoint 5 scope — 2026-07-29 (before implementation)

### Current failure baseline and complete grouping

The required current-PA report is **285/396**, with **111** failing
fixtures: 76 exit-status mismatches, 33 LowIR comparisons, and the two
reentrant static-query timeouts.  The exact fixture inventory is checked into
`pa23/audit.md`; the current report was re-read from the primary log before
this checkpoint.  The complete set is grouped by shared behavior below
(groups intentionally overlap where a semantic fact is later materialized).

- **Candidate-local deferred substitution and expression SFINAE (21):**
  `general/200-function-template-named-parameter-sfinae.t`,
  `general/300-array-qualified-member-type-sfinae.t`,
  `general/300-decltype-member-template-callable-pack-result-cache.t`,
  `general/300-hidden-friend-current-specialization-enable-if.t`,
  `general/300-qualified-explicit-template-alias-return-sfinae.t`,
  `general/400-dependent-alias-nontype-sequence-filter.t`,
  `general/400-elaborated-type-template-arg-false-branch.t`,
  `general/400-elaborated-type-template-arg-true-branch.t`,
  `general/400-member-variable-template-leaf-sfinae.t`,
  `general/500-array-type-argument-sfinae-static-value.t`,
  `general/500-const-reference-alias-default-sfinae.t`,
  `general/500-dependent-qualified-member-template-result-bool.t`,
  `general/500-dependent-std-or-enable-if-defers.t`,
  `general/500-dependent-typename-enable-if-candidate.t`,
  `general/500-dependent-typename-member-enable-if-return.t`,
  `general/500-member-template-conditional-alias-trailing-return.t`,
  `general/500-mp11-append-alias-template-sfinae.t`,
  `general/500-nontype-alias-reinstantiation-structural-state.t`,
  `general/500-source-namespace-base-sfinae-chain.t`,
  `spec/300-deleted-function-template-callee-detector-sfinae.t`, and
  `spec/300-deleted-function-template-expression-sfinae.t`.
- **Qualified owner/member replay (31):** the general/100 namespace and
  out-of-class cases; general/200 empty-pack, member-template, and nested-id
  cases; general/300 current-specialization cases; general/400 inherited,
  alias-owner, nested-owner, out-of-class, qualified-variable, recursive,
  and unused-member-template cases; general/500 dependent/out-of-class
  owner cases; and spec/100, spec/200, spec/400, and spec/500 qualified or
  inherited member cases.  The exact names are the corresponding entries in
  the audit inventory, not a filename-pattern acceptance rule.
- **Deduction, overload ranking, conversion, and pack normalization (25):**
  the remaining general/100 function-parameter cases, general/200 ADL,
  constructor, reference/cv, template-template, operator, pack, and
  non-deduced-context cases, general/300 constructor conversion,
  general/400 function-type/static-cast cases, and spec/100, spec/200,
  spec/300, and spec/400 function/conversion cases.
- **Typed aliases and non-type values (20):** dependent boolean/static values,
  qualified non-type bases, intermediate transforms, `sizeof`, variable and
  member templates, pointer/reference/function-pointer arguments, integral
  and enum scope values, class-template argument packs, and pack-kind
  rejection cases across the exact general/100, general/400, general/500,
  spec/100, spec/200, and spec/400 entries in the inventory.
- **Specialization/extern/constructor LowIR materialization (14):** the
  comparison failures for current and explicit specializations, out-of-class
  constructors and members, extern/explicit instantiation, function-pointer
  declarations, and generated declaration metadata.  These are downstream
  consumers of the preceding typed facts, not separate output fixtures.
- **Reentrant query stress (2):**
  `general/500-reentrant-static-query-callable-enable-if-cache.t` and
  `general/500-reentrant-static-query-enable-if-partial.t` still time out;
  they remain stress witnesses for the same scoped query identity and are not
  permitted to gain timeout-specific handling.

### Checkpoint Scope

Complete the deferred candidate-replay group as one typed semantic increment:

1. Preserve candidate-local substitution through dependent `typename` return
   types and nested trait/alias probes, so the two dependent-typename cases
   select the correct overload and replay only the selected argument AST.
2. Treat a selected deleted function as an invalid expression-SFINAE probe,
   allowing the fallback overload in both deleted-function detector cases;
   keep the deleted declaration out of materialized LowIR.
3. Preserve fresh pack elements and dependent `decltype` results across cached
   member-template queries and std-shaped disjunction/`enable_if` defaults,
   validating `general/300-decltype-member-template-callable-pack-result-cache.t`
   and `general/500-dependent-std-or-enable-if-defers.t` alongside the four
   primary cases above.

Validation for this scope is the six named fixtures, the two reentrant timeout
witnesses, the full PA23 report, the required through-PA22 report, and the
PA23 file audit.  The next checkpoint group is qualified owner/member replay
bundled with typed non-type values, followed by the remaining deduction and
materialization comparison band.

## Checkpoint 5 result — 2026-07-29

The selected deferred candidate-replay scope is complete: all 6/6 focused
fixtures pass. The increment now keeps immediate return-type facts in typed
candidate state, rejects known-false `enable_if` results only while
materializing candidates whose result spelling contains that substitution
context, and keeps deleted candidates out of both decltype probes and emitted
calls. It also preserves direct `const T` reference collapsing without
discarding the cv-qualification of ordinary reference results, binds partial
specialization packs from their matched patterns, and normalizes only the
concrete `std::std::...` owner path needed by dependent replay. PA14 no longer
precomputes dead addresses for trivially initialized class arrays.

Validation completed for this checkpoint:

- `make test-report-through-pa22`: 2100/2100 tests passed.
- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'`: 292/396 tests passed,
  improving the turn-start baseline of 285/396 by 7; the two reentrant
  static-query fixtures still time out.
- The six focused checkpoint fixtures all pass, including both dependent
  typename cases, both deleted-function SFINAE cases, the cached callable-pack
  case, and the dependent std/or-enable_if case.

### Remaining Work Map

- **Qualified owner/member replay and materialization:** the largest remaining
  status and LowIR group, including generated member owners, inherited and
  out-of-class members, current-specialization names, and specialization
  declaration/definition pairing.
- **Typed non-type values and partial-pack state:** dependent bool/integral,
  `sizeof`, variable/member-template values, qualified scopes, and remaining
  pack-kind rejection cases.
- **Deduction and overload semantics:** the remaining function-template
  deduction, conversion, ADL, operator, reference/cv, template-template, and
  non-deduced-context cases.
- **Specialization, extern, constructor, and LowIR metadata:** downstream
  comparison failures after the typed owner/value facts are available.
- **Reentrant query fixed points:** the two timeout witnesses remain; they need
  query identity/cycle handling rather than timeout-specific behavior.

### Next Checkpoint Group

Take the qualified owner/member replay group bundled with the typed non-type
value group. Validate one shared owner-identity path across inherited,
out-of-class, alias-owner, and current-specialization queries, then verify the
corresponding integral/pack facts and their generated LowIR consumers. Keep
the 292/396 PA23 count and the 2100/2100 through-PA22 gate as the regression
baseline for that checkpoint.

## Checkpoint 5 audit — 2026-07-29

The Checkpoint 5 deferred-replay implementation and its follow-on audit fixes
are complete. Candidate deleted state, immediate return constraints, and
reference-alias qualification are now typed facts collected with declarations;
the call and emission paths no longer recover them from generated spelling.
Qualified member replay uses registered generated-specialization identity rather
than a `std::std::` spelling exception or an `enable_if`/`disable_if` name gate.
The helper ownership was kept in existing source modules and the file audit
line limits remain satisfied.

Validation for this audit is complete: the six focused fixtures pass, PA23 is
`292 / 396` with 72 exit-status mismatches, 30 LowIR comparisons, and the same
two reentrant timeout witnesses, the through-PA22 gate is `2100 / 2100`, and
the through-PA23 report is `2392 / 2496` overall.  The PA23 file audit passes
with its existing 13 warnings.  No current-only regressions were found
relative to the clean `804b32b` state, and no tests or reference fixtures were
changed.

The remaining work is grouped into qualified owner/member replay and
materialization, typed non-type values and partial-pack state, deduction and
overload composition, explicit/extern/constructor LowIR metadata, and the two
reentrant query fixed-point witnesses.  The next substantial checkpoint is the
shared registered-owner path bundled with typed integral/boolean/`sizeof` and
pack facts, followed by the remaining deduction comparison band.
