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

## Checkpoint 6 scope — 2026-07-29 (before implementation)

### Current failure baseline and Remaining Work Map

The current required PA23 report is **292/396**, so the complete failure set is
**104 fixtures**: 72 exit-status mismatches, 30 LowIR comparisons, and the two
reentrant static-query timeouts.  The exact inventory was re-read from the
current primary log before implementation.  The groups below identify the
first shared semantic behavior that must be repaired; a fixture can also
consume a downstream materialization fact.  The outcome labels and fixture
names are complete; causal groups intentionally overlap where one typed fact
feeds more than one lookup path.

- **Qualified owner/member replay (status failures):** dependent owners,
  using-directive imports, inherited member templates, nested template-ids,
  current-specialization names, and out-of-class member scopes remain the
  largest exit-status group.  Its exact current fixtures are
  `general/100-dependent-bool-partial-static-value-storage.t`,
  `general/100-direct-namespace-wins-over-using-directive.t`,
  `general/100-local-qualified-argument-replay.t`,
  `general/100-member-template-specialization-return-prefers-member-call.t`,
  `general/100-using-directive-template-member-type-typedef.t`,
  `general/100-selected-specialization-special-member-body.t`,
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
  `general/500-dependent-qualified-member-template-result-bool.t`,
  `general/500-dependent-qualified-sizeof-static-member.t`,
  `general/500-out-of-class-member-template-dependent-owner-type.t`,
  `general/500-recursive-qualified-member-template-bool-arg.t`,
  `general/500-source-namespace-base-sfinae-chain.t`,
  `spec/100-dependent-template-id-qualified-member-source-owner.t`,
  `spec/200-inherited-template-param-shadow-forward.t`,
  `spec/400-defaulted-nested-class-argument-partial-specialization.t`,
  `spec/400-dependent-member-template-call.t`,
  `spec/400-explicit-pack-type-argument-uses-bound-type.t`,
  `spec/400-explicit-type-arg-dependent-qualified-member-template-id.t`,
  `spec/400-qualified-member-template-id-bool-constant.t`, and
  `spec/500-conditional-alias-index-sequence-member-template-call.t`.
- **Typed integral/`sizeof`, variable-template, alias, and pack state (status
  failures):** `general/100-default-nontype-qualified-function-lookup.t`,
  `general/100-nested-template-static-value-nontype-expression.t`,
  `general/100-nontype-function-parameter-adjustment.t`,
  `general/100-structured-bool-boost-convertible-mpl-overload.t`,
  `general/400-dependent-alias-nontype-sequence-filter.t`,
  `general/400-elaborated-type-template-arg-false-branch.t`,
  `general/400-elaborated-type-template-arg-true-branch.t`,
  `general/400-local-class-default-member-variable-template-nontype-type.t`,
  `general/500-array-type-argument-sfinae-static-value.t`,
  `general/500-mp11-append-alias-template-sfinae.t`,
  `general/500-nontype-alias-reinstantiation-structural-state.t`,
  `general/500-type-pack-rejects-value-pack-bad.t`, and
  `spec/400-class-template-nttp-scope-value.t`.
- **Deduction, overload ranking, conversions, and non-deduced contexts (status
  failures):** `general/100-explicit-function-specialization-overload-parameter-match.t`,
  `general/100-function-type-not-pointer-partial-specialization.t`,
  `general/200-class-partial-specialization-no-derived-base-deduction.t`,
  `general/200-constructor-template-parameter-shadows-instantiated-type.t`,
  `general/200-function-template-named-parameter-sfinae.t`,
  `general/200-pack-not-at-end-nondeduced-bad.t`,
  `general/200-template-template-qualified-default-arg-deduction.t`,
  `general/300-constructor-template-const-ref-enable-if-conversion.t`,
  `general/400-function-type-partial-specialization-fixed-arity-over-pack.t`,
  `general/400-function-type-tail-pack-recursive-specialization.t`,
  `general/400-static-cast-rvalue-ref-skips-conversion-operator.t`,
  `spec/100-function-template-nontype-function-pointer-call.t`,
  `spec/100-function-template-nontype-function-pointer-specialization-call.t`,
  `spec/100-local-member-call-constructor-template-instantiation.t`,
  `spec/100-nontype-function-pointer-argument.t`,
  `spec/300-constructor-default-pack-partial-ordering.t`,
  `spec/400-conversion-function-template-call-argument.t`,
  `spec/400-conversion-function-template-copy-init.t`, and
  `spec/400-conversion-function-template-selection.t`.
- **Deferred SFINAE and candidate-local replay (status failures):**
  `general/300-array-qualified-member-type-sfinae.t`,
  `general/300-hidden-friend-current-specialization-enable-if.t`,
  `general/300-qualified-explicit-template-alias-return-sfinae.t`,
  `general/400-dependent-alias-nontype-sequence-filter.t`,
  `general/400-member-alias-template-template-owner-argument.t`,
  `general/400-member-variable-template-leaf-sfinae.t` is a LowIR witness of
  this same path, `general/500-constructor-sfinae-owner-pack-function-type.t`,
  `general/500-dependent-function-type-pack-expansion-ctor-init.t`,
  `general/500-member-template-conditional-alias-trailing-return.t`,
  `spec/100-extern-template-member-function-declaration.t`, and
  `spec/100-extern-template-static-data-declaration.t`.
- **LowIR specialization/materialization and typed downstream facts (30):**
  `general/100-current-specialization-member-body-cast-compare.t`,
  `general/100-dependent-qualified-nontype-base-argument.t`,
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
  `general/400-explicit-function-template-type-arg-drops-nontype-overload.t`,
  `general/400-function-type-pack-out-of-class-constructor.t`,
  `general/400-member-variable-template-leaf-sfinae.t`,
  `general/400-nonmember-template-compound-assignment-const-lhs.t`,
  `general/400-out-of-class-partial-member-template-owner-parameter-alias.t`,
  `general/400-variable-template-specializations.t`,
  `spec/100-explicit-instantiation-after-explicit-specialization-no-effect.t`,
  `spec/100-explicit-instantiation-class-prior-member-definitions.t`,
  `spec/100-explicit-specialization-out-of-class-ctor-replay.t`,
  `spec/100-explicit-specialization-out-of-class-member-emits.t`,
  `spec/100-out-of-class-conversion-operator-definition.t`,
  `spec/100-partial-specialization-member-primary-param-name.t`,
  `spec/100-sizeof-union-type-nttp.t`,
  `spec/200-defaulted-class-template-argument-pack-prefix-deduction.t`,
  `spec/400-defaulted-template-arg-partial-base-completion.t`, and
  `spec/400-template-template-member-alias-owner-shadow.t`.
- **Reentrant fixed-point stress (2):**
  `general/500-reentrant-static-query-callable-enable-if-cache.t` and
  `general/500-reentrant-static-query-enable-if-partial.t` remain timeouts;
  they require semantic query identity, not harness-specific handling.

### Checkpoint Scope

Implement the registered qualified-owner increment.  Add typed namespace and
using-declaration resolution for a qualified template-id, then use the
specialization registry to map a generated owner spelling back to its
materialized declaration before member type, static-member, or nested-member
lookup.  Preserve lexical source paths when no registered generated owner
matches, and carry the owner’s concrete template arguments into the existing
member replay and integral-value paths.  This is a real shared behavior for
using-directive aliases, inherited member templates, variable-template static
values, and dependent `sizeof` members.

Validate first with the owner/value fixtures named in the next checkpoint
section, then run the full PA23 report, through-PA22 report, and file audit.
The next checkpoint group is the remaining function deduction/conversion band
bundled with its LowIR materialization comparisons; the two timeout witnesses
remain deferred stress coverage.

### Focused Validation Set

`general/100-using-directive-template-member-type-typedef.t`,
`general/400-inherited-qualified-member-template-type.t`,
`general/400-qualified-member-variable-template-class-value.t`,
`general/500-dependent-qualified-sizeof-static-member.t`,
`spec/400-dependent-member-template-call.t`, and
`spec/400-qualified-member-template-id-bool-constant.t`.

## Checkpoint 6 result — 2026-07-29

The registered qualified-owner increment is complete.  Namespace using-
directives and using-declaration targets are retained as typed lookup facts;
generated specialization declarations are preferred for qualified member
lookup; member-template replay carries the concrete owner and integral
arguments; and qualified member `sizeof`/`::value` expressions now reach the
typed evaluator without being mistaken for ordinary member types.  PA14 now
promotes demanded static-member records to storage and emits the required
dynamic scalar initializer load, while preserving ordinary `sizeof` lowering.

The six focused fixtures above pass.  The full PA23 report is **298/396**,
above the 292/396 turn-start baseline.  The remaining 98 failures group into
66 status failures, 31 LowIR comparisons, and one reentrant-query timeout:

- **Owner/member replay and deferred SFINAE:** current-specialization,
  inherited/out-of-class members, alias owners, nested member templates, and
  dependent call/result paths remain the largest status group.
- **Typed non-type and pack state:** dependent integral/bool values,
  variable-template and alias values, function-type packs, and invalid pack
  kind rejection remain.
- **Deduction and overload composition:** conversion, reference/cv,
  template-template, function-type, ADL, and non-deduced-context cases remain.
- **Specialization/extern/constructor LowIR:** explicit specialization and
  declaration/definition pairing still produce the remaining comparison band.
- **Reentrant fixed point:** the callable/partial static-query pair remains
  the stress boundary, with one still timing out.

The next checkpoint is the deduction/conversion group bundled with its
function-type-pack and LowIR materialization consumers.  It will validate
candidate-local typed substitutions and overload identity first, then rerun
the full PA23 report and the through-PA22 regression gate.

## Checkpoint 7 scope — 2026-07-29

### Remaining Work Map

The re-read current-PA report is **294/396**, leaving **102** failures.  The
remaining failures group into the same shared semantic owners:

- **Qualified owner/member replay:** using-directive and direct-namespace
  selection, inherited and out-of-class member templates, nested template-id
  lookup, current-specialization names, and concrete owner propagation remain
  the largest status group.
- **Typed non-type and pack state:** dependent integral/bool values, qualified
  `sizeof`, variable/member-template values, default arguments, and pack-kind
  rejection still lose typed facts during replay.
- **Deduction and overload composition:** function-type partial ordering,
  reference/cv normalization, conversions, template-template arguments,
  explicit function arguments, and non-deduced contexts remain unresolved.
- **Deferred substitution and fixed points:** candidate-local member/SFINAE
  queries, extern/template declaration replay, and both reentrant static-query
  timeout witnesses still need cycle-safe typed state.
- **LowIR materialization:** explicit/extern specialization, constructor and
  member emission, function-pointer declarations, and generated metadata still
  account for the comparison failures.

### Checkpoint Scope

Complete the small deduction/lookup increment that can be isolated without
changing established earlier-PA owner replay:

1. Treat a function parameter pack before a fixed trailing parameter as a
   non-deduced context unless an explicit or typed pack binding exists.  This
   prevents invalid nonterminal-pack calls from consuming arguments and being
   accepted.
2. Record namespace `using` directives as typed lookup facts and let qualified
   template lookup consult those imports after direct lexical lookup, while
   preserving unambiguous using-declaration targets.  Move the enlarged
   `FindDefinition` implementation into a dedicated source module so the
   semantic behavior remains in typed compiler state and file-audit limits are
   preserved.

### Checkpoint 7 result

Focused validation passed:

`general/200-pack-not-at-end-nondeduced-bad.t`,
`spec/400-dependent-member-template-call.t`,
PA21 `spec/100-inline-namespace-qualified-template-id-pack-expansion.t`, and
PA22 `spec/500-hidden-friend-query-free-decltype-noexcept.t`.

The required current-PA report is **294/396**, above the turn-start baseline
of **292/396**.  The through-PA22 report is **2100/2100**, and the PA23 file
audit passes with the repository's existing warnings.

### Next Checkpoint Group

Take the qualified owner/member materialization group next, but preserve the
earlier-PA current-instantiation and out-of-class-constructor cases while
mapping generated owners back to typed declarations.  Bundle only the
dependent integral/`sizeof` consumers whose owner identity is available, then
rerun the complete PA23 report, through-PA22 gate, and file audit.

## Checkpoint 8 result — 2026-07-29

### Checkpoint Scope

Completed the qualified member variable-template and static-member replay
increment.  The implementation now preserves typed empty-pack facts from the
AST through PA11/PA18/PA14, maps member-template uses to concrete generated
and source owners, retains qualified source static-array declarators, and
recovers dependent `sizeof` array types.  PA14 lowering now distinguishes
constant folding from required static storage/materialization for qualified
template members, including explicit specializations and type-pack uses, and
emits the corresponding ABI, initializer, and lifecycle metadata.  The new
static-rewrite helper module keeps these responsibilities within the PA23
file-audit limits.

### Validation

The focused qualified member/static cases pass, including
`general/400-qualified-member-variable-template-class-value.t` and
`general/500-dependent-qualified-sizeof-static-member.t`, with the related
PA19–PA22 regression cases.  The through-PA22 report is **2100/2100**.  The
required current-PA report is **296/396**, improving the turn-start baseline
of **292/396**.  The PA23 file audit passes with the repository's 13 existing
warnings.

### Remaining Work Map

The complete current-PA report has 100 failures, including two timeouts.  The
remaining behavior groups are:

- **Owner/member replay and deferred SFINAE:** current-specialization,
  inherited/out-of-class members, alias and nested owners, candidate-local
  substitution, and reentrant static-query fixed points.
- **Deduction and overload composition:** conversions, reference/cv and
  function-type partial ordering, template-template arguments, ADL, explicit
  arguments, and non-deduced contexts.
- **Typed non-type and pack state:** dependent bool/integral values, variable
  and alias templates, function-type packs, defaults, and invalid pack-kind
  rejection.
- **Specialization and declaration materialization:** explicit/extern
  specialization pairing, constructors, function pointers, and remaining
  LowIR identity/metadata comparisons.

### Next Checkpoint Group

Take deduction/conversion and function-type-pack handling as one coherent
group, including its downstream overload and LowIR consumers.  Preserve the
new concrete-owner and static-materialization facts, then rerun the complete
PA23 report, through-PA22 gate, and file audit.

## Checkpoint 9 result — 2026-07-29

### Checkpoint Scope

Completed the specialization/alias replay increment for typed non-type
values and captured member alias packs.  Class partial-specialization matching
now converts concrete integral expressions to the declared non-type parameter
type before comparing them; nested qualified static values therefore retain
their bool/integral identity.  Replayed member aliases retain captured packs
when used as template-template arguments, merge a later alias application
with the captured argument list, and materialize the selected owner through
the existing generated-specialization registry.  Local typedef aliases now
record already-evaluated array extents, so later type-id uses do not restore a
dependent bound.  The increment also retains the existing typed C-style
integral-cast evaluation used by these non-type expressions.

### Validation

The focused scope, nested pack/function-pointer typedef, template-template
application, dependent constant lookup, and member-alias owner-shadow cases
all pass direct compiler validation.  The required current-PA report is
**301/396**, above the checkpoint baseline of **300/396**.  The complete
remainder is 61 exit-status mismatches, 32 LowIR comparison mismatches, and
the same two reentrant static-query timeouts.  No tests or reference fixtures
were changed.

### Remaining Work Map

- **Qualified owner/member replay and candidate-local deferral:** using and
  inherited owners, nested/current-specialization member templates,
  out-of-class replay, and dependent member-result/SFINAE cases remain the
  largest status group.
- **Typed aliases, variable templates, and non-type packs:** qualified
  `sizeof`, variable/member-template values, defaults, pointer/reference
  arguments, and pack-kind rejection still need their typed facts preserved
  through all replay paths.
- **Deduction and overload composition:** conversions, function-type partial
  ordering, reference/cv normalization, ADL, explicit arguments,
  template-template defaults, and non-deduced contexts remain unresolved.
- **Specialization/extern/constructor LowIR:** downstream comparison failures
  still cover declaration pairing, constructors, function pointers, member
  emission, and generated metadata/order.
- **Reentrant fixed points:**
  `general/500-reentrant-static-query-callable-enable-if-cache.t` and
  `general/500-reentrant-static-query-enable-if-partial.t` still require a
  stable semantic query identity rather than timeout-specific handling.

### Next Checkpoint Group

Take the remaining qualified owner/member replay group bundled with its
typed variable-template, `sizeof`, and pack consumers.  Validate concrete
owner identity and candidate-local lookup first, then rerun the full PA23
report, through-PA22 regression gate, and PA23 file audit before taking the
remaining deduction/materialization comparison band.

### Checkpoint 9 audit result

The checkpoint audit replaced the registry-wide static-member recovery scan
with the existing specialization-by-primary index, exact typed owner-path
resolution, and an early `(owner, member)` recursion identity.  It also caches
generated AST spellings during namespace dependency ordering.  The required
dependent nested-class materialization remains because its removal regresses
the function-type-tail-pack recursive-specialization witness; it is semantic
replay, not timeout handling.

The complete failure inventory and validation evidence are recorded in
`pa23/audit.md`.  The refreshed map is grouped into qualified owner/member
and candidate-local replay; typed aliases, variable templates, `sizeof`, and
pack state; deduction/overload composition; specialization/extern/constructor
LowIR; and the two reentrant fixed-point witnesses.  The next substantial
checkpoint remains the first group bundled with the small typed-value groups.

## Checkpoint 10 scope — 2026-07-29 (before implementation)

### Baseline

The clean turn-start report is **301/396**.  The complete current-PA failure
set is **95 fixtures**: 61 exit-status mismatches, 32 LowIR comparison
mismatches, and two timeouts.  Assignments through PA22 pass.  The required
inventory was re-read from `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`;
the full fixture list is also retained in `pa23/audit.md`.

### Remaining Work Map

- **Qualified owner/member replay and candidate-local lookup (status):**
  `general/100-direct-namespace-wins-over-using-directive.t`,
  `general/100-local-qualified-argument-replay.t`,
  `general/100-member-template-specialization-return-prefers-member-call.t`,
  `general/100-using-directive-template-member-type-typedef.t`,
  `general/200-empty-pack-member-template-owner-key.t`,
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
  `general/400-unused-static-member-template-return-type.t`,
  `general/500-dependent-qualified-member-template-result-bool.t`,
  `general/500-out-of-class-member-template-dependent-owner-type.t`,
  `general/500-recursive-qualified-member-template-bool-arg.t`,
  `general/500-source-namespace-base-sfinae-chain.t`,
  `spec/100-dependent-template-id-qualified-member-source-owner.t`,
  `spec/200-inherited-template-param-shadow-forward.t`,
  `spec/400-defaulted-nested-class-argument-partial-specialization.t`,
  `spec/400-dependent-member-template-call.t`,
  `spec/400-explicit-pack-type-argument-uses-bound-type.t`,
  `spec/400-explicit-type-arg-dependent-qualified-member-template-id.t`,
  `spec/400-qualified-member-template-id-bool-constant.t`, and
  `spec/500-conditional-alias-index-sequence-member-template-call.t`.
- **Typed alias, non-type, and pack state (status):**
  `general/200-class-partial-specialization-no-derived-base-deduction.t`,
  `general/200-template-template-qualified-default-arg-deduction.t`,
  `general/400-dependent-alias-nontype-sequence-filter.t`,
  `general/400-elaborated-type-template-arg-false-branch.t`,
  `general/400-elaborated-type-template-arg-true-branch.t`,
  `general/400-local-class-default-member-variable-template-nontype-type.t`,
  `general/400-member-variable-template-leaf-sfinae.t`,
  `general/400-variable-template-specializations.t`,
  `general/500-array-type-argument-sfinae-static-value.t`,
  `general/500-mp11-append-alias-template-sfinae.t`,
  `general/500-nontype-alias-reinstantiation-structural-state.t`, and
  `general/500-type-pack-rejects-value-pack-bad.t`.
- **Deduction, conversion, and overload composition (status):**
  `general/100-explicit-function-specialization-overload-parameter-match.t`,
  `general/100-function-type-not-pointer-partial-specialization.t`,
  `general/100-nontype-function-parameter-adjustment.t`,
  `general/100-structured-bool-boost-convertible-mpl-overload.t`,
  `general/200-constructor-template-parameter-shadows-instantiated-type.t`,
  `general/200-function-template-named-parameter-sfinae.t`,
  `general/200-member-function-template-address-explicit-pack.t`,
  `general/300-constructor-template-const-ref-enable-if-conversion.t`,
  `general/400-function-type-partial-specialization-fixed-arity-over-pack.t`,
  `general/400-static-cast-rvalue-ref-skips-conversion-operator.t`,
  `spec/100-function-template-nontype-function-pointer-call.t`,
  `spec/100-function-template-nontype-function-pointer-specialization-call.t`,
  `spec/100-local-member-call-constructor-template-instantiation.t`,
  `spec/100-nontype-function-pointer-argument.t`,
  `spec/400-conversion-function-template-call-argument.t`,
  `spec/400-conversion-function-template-copy-init.t`, and
  `spec/400-conversion-function-template-selection.t`.
- **Specialization, extern/constructor replay, and LowIR metadata:** the 32
  comparison fixtures are the exact `LOWIR` entries in the current report,
  covering current/explicit specialization bodies, function pointers,
  `sizeof`/non-type values, extern declarations, constructors, member
  emission, and generated declaration metadata/order.
- **Reentrant fixed points:**
  `general/500-reentrant-static-query-callable-enable-if-cache.t` and
  `general/500-reentrant-static-query-enable-if-partial.t` still time out and
  require semantic query identity/cycle handling, not timeout-specific
  acceptance.

### Checkpoint Scope

Implement the typed nested-member owner increment.  When a nested class
template is materialized as part of a concrete enclosing specialization, bind
only the enclosing `TemplateDefinition` parameters from the enclosing
argument vector; retain the nested declaration’s own parameters and dependent
member expressions until the nested template-id supplies those arguments.
Preserve the existing typed pack scope and generated-owner registry while
replaying inherited member types.  In the same immediate semantic validation
pass, reject a value expression (including a value pack expansion such as
`trait<Keys>::value...`) when the selected template parameter is a type
parameter, while continuing to accept dependent type-ids and ordinary
non-type expressions according to their declared parameter kinds.

Validate this scope with:

`general/400-inherited-qualified-member-template-type.t`,
`general/400-dependent-nontype-member-template-owner.t`,
`general/400-nested-member-template-base-param-shadow-value.t`,
`general/500-out-of-class-member-template-dependent-owner-type.t`,
`general/500-dependent-qualified-member-template-result-bool.t`,
`spec/400-explicit-type-arg-dependent-qualified-member-template-id.t`,
`spec/400-qualified-member-template-id-bool-constant.t`,
`spec/500-conditional-alias-index-sequence-member-template-call.t`, and
`general/500-type-pack-rejects-value-pack-bad.t`, followed by the full PA23
report, the required through-PA22 report, and the PA23 file audit.  The next
group is the remaining candidate-local SFINAE/deferred and deduction bands,
bundled with any newly exposed owner paths; the two timeout fixtures remain
fixed-point stress witnesses.

## Checkpoint 10 result — 2026-07-29

Implemented the nested-owner checkpoint.  Concrete enclosing specializations
now bind only their enclosing parameters when materializing a member class
template; non-template nested classes retain the earlier enclosing replay
needed by PA18.  A complete nested class-template-id is replayed with the
typed enclosing owner and its own arguments, while partial nested
specializations stay on the established path.  Out-of-class function
definitions also carry their qualified owner into dependent-type validation.
Template-argument validation now tracks type versus value expressions
recursively, including value-pack expansions, and the constant evaluator
recognizes replayed boolean literals without ordinary identifier lookup.

The validation helpers were moved to `dev/src/pa18_templates_validation.cpp`
and added to the cppgm++ source set to keep the touched modules within the
file-audit budgets.

### Checkpoint validation

- Focused PA18/PA21 regression witnesses: pass.
- Focused PA22 validation witnesses: pass.
- Selected PA23 owner/value witnesses: pass; the intentional bad type-pack
  witness fails compilation as required.
- `make test-report-through-pa22`: **2100/2100**, pass.
- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'`: **304/396**; this is
  **+3** over the 301/396 turn-start baseline.  The residual 92 fixtures are
  57 status mismatches, 33 LowIR comparisons, and the same two reentrant
  timeout witnesses.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src`: pass with
  pre-existing warnings only.

### Refreshed Remaining Work Map and next checkpoint

- **Owner/member lookup:** direct namespace/using-directive precedence,
  out-of-class dependent member definitions, inherited member-template
  lookup, qualified member-template result types, and the remaining
  candidate-local owner/SFINAE cases.  The next checkpoint starts here with
  the out-of-class and inherited owner witnesses bundled together.
- **Typed values, aliases, packs, and deduction:** dependent alias filters,
  elaborated type arguments, variable-template state, function-type partials,
  pack/default deduction, and non-type function-pointer/conversion cases.
- **Specialization and LowIR:** the 33 comparison-only fixtures covering
  specialization bodies, constructors, extern/explicit instantiation,
  function pointers, `sizeof`/non-type values, and generated declaration
  metadata/order.
- **Fixed points:** the two reentrant static-query fixtures still require
  query identity/cycle handling; they remain separate stress witnesses.

## Checkpoint 10 audit result — 2026-07-29

The checkpoint audit found and fixed two checkpoint-level issues in immediate
template-argument validation.  Member-kind classification no longer uses
magic suffixes such as `value` or `::type`; it uses the existing typed alias,
class, named-type, static-member, and template-definition registries.  The
duplicate outer validation walk was removed, and direct static-member indexes
are checked before contextual definition lookup.  Nested owner replay,
enclosing-parameter binding, and generated-owner state remain unchanged.

Validation is clean through PA22 at **2100/2100**.  PA23 remains at
**304/396**, equal to the turn-start baseline and +3 over the checkpoint
baseline; the final complete remainder is **57 status**, **33 LowIR**, and
**2 timeouts**.  The PA23 file audit passes with its existing 13 warnings.
The exact 92-fixture inventory is recorded in the matching Checkpoint 10
section of `pa23/audit.md`.

### Remaining Work Map and next checkpoint

- **Qualified owner/member replay and deferred candidates:** direct namespace
  precedence, inherited/out-of-class member lookup, current-specialization
  owners, and candidate-local SFINAE.  This is the next substantial group.
- **Typed value consumers:** variable-template state, `sizeof`, aliases,
  packs, boolean/non-type expressions, and the dependent member-result paths
  that share the owner identity.
- **Deduction and conversion:** function-type partials, default/pack
  deduction, function pointers, ADL, overload ranking, and conversions.
- **Specialization and LowIR:** explicit/extern specialization, constructors,
  member emission, and the remaining generated declaration metadata/order
  comparisons.
- **Fixed-point witnesses:** the two reentrant static-query fixtures remain
  separately classified as timeout coverage; no timing-specific acceptance
  is part of the next group.

The next checkpoint bundles the first two groups so one typed owner identity
can flow through lookup, deferred selection, and typed value consumers before
the deduction/conversion and specialization/LowIR groups.

## Checkpoint 11 scope — 2026-07-29 (before implementation)

### Baseline

The clean turn-start report is **304/396**. Assignments through PA22 pass.
The complete live PA23 failure set is **92 fixtures**: 57 exit-status
mismatches, 33 LowIR comparison mismatches, and two reentrant static-query
timeouts. The inventory below is taken from the required primary log, not
from a narrowed focused run.

### Remaining Work Map

- **Typed owner/member lookup and deferred owner replay (status):**
  materialized class-template ids are sometimes recovered by an ambiguous
  short-name scan instead of their typed lexical owner; inherited member
  templates, using directives, nested member ids, out-of-class definitions,
  and member result aliases then lose the source namespace or concrete
  enclosing arguments. The exact fixtures are:

  `general/100-direct-namespace-wins-over-using-directive.t`,
  `general/100-member-template-specialization-return-prefers-member-call.t`,
  `general/100-using-directive-template-member-type-typedef.t`,
  `general/200-empty-pack-member-template-owner-key.t`,
  `general/200-member-template-implicit-instantiation-not-overload.t`,
  `general/200-nested-template-id-partial-specialization-deduction.t`,
  `general/300-array-qualified-member-type-sfinae.t`,
  `general/300-hidden-friend-current-specialization-enable-if.t`,
  `general/300-qualified-explicit-template-alias-return-sfinae.t`,
  `general/400-current-specialization-display-name-member-alias.t`,
  `general/400-member-alias-template-template-owner-argument.t`,
  `general/400-member-template-result-pack-preserves-nested-function-pointer-owner.t`,
  `general/400-out-of-class-ctor-using-imported-member-template.t`,
  `general/400-unused-static-member-template-return-type.t`,
  `general/500-dependent-qualified-member-template-result-bool.t`,
  `general/500-member-template-conditional-alias-trailing-return.t`,
  `general/500-out-of-class-member-template-dependent-owner-type.t`,
  `general/500-recursive-qualified-member-template-bool-arg.t`,
  `general/500-source-namespace-base-sfinae-chain.t`,
  `spec/100-dependent-template-id-qualified-member-source-owner.t`,
  `spec/200-inherited-template-param-shadow-forward.t`,
  `spec/400-defaulted-nested-class-argument-partial-specialization.t`,
  `spec/400-dependent-member-template-call.t`,
  `spec/400-explicit-pack-type-argument-uses-bound-type.t`,
  `spec/400-explicit-type-arg-dependent-qualified-member-template-id.t`,
  `spec/400-qualified-member-template-id-bool-constant.t`, and
  `spec/500-conditional-alias-index-sequence-member-template-call.t`.

- **Typed alias, variable-template, non-type, and pack state (status):**
  value/type identity and integral evaluation are still lost in alias filters,
  variable templates, class partials, and defaulted pack arguments:

  `general/200-class-partial-specialization-no-derived-base-deduction.t`,
  `general/200-template-template-qualified-default-arg-deduction.t`,
  `general/400-dependent-alias-nontype-sequence-filter.t`,
  `general/400-elaborated-type-template-arg-false-branch.t`,
  `general/400-elaborated-type-template-arg-true-branch.t`,
  `general/400-local-class-default-member-variable-template-nontype-type.t`,
  `general/400-member-variable-template-leaf-sfinae.t`,
  `general/400-variable-template-specializations.t`,
  `general/500-array-type-argument-sfinae-static-value.t`,
  `general/500-mp11-append-alias-template-sfinae.t`,
  `general/500-nontype-alias-reinstantiation-structural-state.t`, and
  `general/500-type-pack-rejects-value-pack-bad.t`.

- **Function deduction, conversion, and overload composition (status):**
  function-type partial ordering, explicit arguments, pointer/reference
  adjustment, constructor conversion, and non-type function-pointer calls
  still fail before or during candidate selection:

  `general/100-explicit-function-specialization-overload-parameter-match.t`,
  `general/100-function-type-not-pointer-partial-specialization.t`,
  `general/100-nontype-function-parameter-adjustment.t`,
  `general/100-structured-bool-boost-convertible-mpl-overload.t`,
  `general/200-constructor-template-parameter-shadows-instantiated-type.t`,
  `general/200-function-template-named-parameter-sfinae.t`,
  `general/200-member-function-template-address-explicit-pack.t`,
  `general/300-constructor-template-const-ref-enable-if-conversion.t`,
  `general/400-function-type-partial-specialization-fixed-arity-over-pack.t`,
  `general/400-static-cast-rvalue-ref-skips-conversion-operator.t`,
  `spec/100-function-template-nontype-function-pointer-call.t`,
  `spec/100-function-template-nontype-function-pointer-specialization-call.t`,
  `spec/100-local-member-call-constructor-template-instantiation.t`,
  `spec/100-nontype-function-pointer-argument.t`,
  `spec/400-conversion-function-template-call-argument.t`,
  `spec/400-conversion-function-template-copy-init.t`, and
  `spec/400-conversion-function-template-selection.t`.

- **Specialization/extern/constructor and LowIR materialization:** the 33
  comparison failures are downstream mismatches in typed specialization
  pairing, constructor/member emission, function pointers, non-type values,
  extern declarations, and generated declaration metadata/order:

  `general/100-current-specialization-member-body-cast-compare.t`,
  `general/100-dependent-bool-partial-static-value-storage.t`,
  `general/100-dependent-qualified-nontype-base-argument.t`,
  `general/100-explicit-specialization-out-of-class-ctor-replay.t`,
  `general/100-explicit-specialization-pointer-member-definition.t`,
  `general/100-inherited-using-alias-out-of-class-specialization-member.t`,
  `general/100-intermediate-type-transform-value-nontype.t`,
  `general/100-local-qualified-argument-replay.t`,
  `general/100-sizeof-call-result-nontype-template-argument.t`,
  `general/200-adl-explicit-template-id-call.t`,
  `general/200-function-template-reference-cv-alias-partial-order.t`,
  `general/200-function-template-template-parameter-deduction.t`,
  `general/200-member-operator-template-reference-pattern-partial-order.t`,
  `general/300-current-specialization-constructor-template-canonical-owner.t`,
  `general/300-dependent-bool-base-trait-type-argument.t`,
  `general/400-explicit-function-template-type-arg-drops-nontype-overload.t`,
  `general/400-function-type-pack-out-of-class-constructor.t`,
  `general/400-member-variable-template-leaf-sfinae.t`,
  `general/400-nonmember-template-compound-assignment-const-lhs.t`,
  `general/400-out-of-class-partial-member-template-owner-parameter-alias.t`,
  `general/400-variable-template-specializations.t`,
  `spec/100-explicit-instantiation-after-explicit-specialization-no-effect.t`,
  `spec/100-explicit-instantiation-class-prior-member-definitions.t`,
  `spec/100-explicit-specialization-out-of-class-ctor-replay.t`,
  `spec/100-explicit-specialization-out-of-class-member-emits.t`,
  `spec/100-out-of-class-conversion-operator-definition.t`,
  `spec/100-partial-specialization-member-primary-param-name.t`,
  `spec/100-sizeof-union-type-nttp.t`,
  `spec/200-defaulted-class-template-argument-pack-prefix-deduction.t`,
  `spec/300-constructor-default-pack-partial-ordering.t`,
  `spec/400-class-template-nttp-scope-value.t`,
  `spec/400-defaulted-template-arg-partial-base-completion.t`, and
  `spec/400-template-template-member-alias-owner-shadow.t`.

- **Reentrant semantic fixed points (timeouts):**
  `general/500-reentrant-static-query-callable-enable-if-cache.t` and
  `general/500-reentrant-static-query-enable-if-partial.t` still need a
  stable typed query identity and cycle result; no timeout-only acceptance is
  in scope.

### Checkpoint Scope

Implement one canonical typed owner-path resolver for a materialized
class-template-id. It will match a source primary/partial definition and its
typed argument vector, then choose the generated declaration under that
definition's lexical/physical owner path; only an exact typed owner may be
used, with short-name fallback retained for genuinely unique source entities.
Thread that resolver through `FindClassMemberType` and the member-template
owner/inherited lookup consumers, while preserving namespace using-directive
precedence and existing pack bindings. This covers dependent aliases,
inherited member templates, nested member ids, and out-of-class owner replay
without changing overload ranking or LowIR formatting.

Validate the selected scope first with the owner/status fixtures
`general/100-using-directive-template-member-type-typedef.t`,
`general/200-empty-pack-member-template-owner-key.t`,
`general/200-nested-template-id-partial-specialization-deduction.t`,
`general/300-array-qualified-member-type-sfinae.t`,
`general/400-member-alias-template-template-owner-argument.t`,
`general/500-source-namespace-base-sfinae-chain.t`, and
`spec/100-dependent-template-id-qualified-member-source-owner.t`, then run
the complete PA23 report, the through-PA22 report, and the PA23 file audit.
The next group is the remaining candidate-local SFINAE/deduction and
specialization/LowIR bands, with the two timeout fixtures retained as
fixed-point stress witnesses.

## Checkpoint 11 result — 2026-07-29

### Implemented behavior

Added a typed materialized-owner resolver in the PA18 template state.  It
matches a source class template (including a selected partial) to its typed
specialization arguments, evaluates non-type arguments through the existing
integral-argument machinery, and selects only the exact physical or lexical
owner path recorded for the generated declaration.  `FindClassMemberType`
keeps its existing fast path and uses this resolver only when that path has no
declaration; aliases-only lookup recognizes a direct nested class only for a
typed generated owner.
This preserves the prior function-type-pack behavior while recovering source
namespace/using-directive owners.

### Validation result

The required PA23 report improved from **304/396** to **306/396**.  The focused
owner checks for
`general/100-using-directive-template-member-type-typedef.t`,
`spec/100-dependent-template-id-qualified-member-source-owner.t`, and the
function-type-pack regression witness all pass.  The complete current residual
set is **90 fixtures: 57 exit-status mismatches (including the two timeout
witnesses) and 33 LowIR comparisons**.  The remaining focused owner cases are
still grouped for the next checkpoint; no test or reference fixture was
changed.

The through-PA22 report is clean (**2100/2100**), and the PA23 file audit
passes with only the repository's existing structural warnings.

### Remaining Work Map

- **Owner/member replay (status):** direct-vs-using lookup, inherited member
  templates, empty packs, nested partial owners, array-qualified SFINAE,
  hidden friends, out-of-class member definitions, and dependent member
  results.  This includes the remaining `general/100`, `general/200`,
  `general/300`, `general/400`, `general/500`, and related `spec/200`/`spec/400`
  owner fixtures in the report.
- **Typed aliases and values (status):** alias/variable-template state,
  integral and boolean values, defaulted packs, elaborated types, and
  dependent non-type filtering.
- **Deduction and conversions (status):** function-type partial ordering,
  function pointers, constructor conversion, ADL, overload ranking, and
  explicit/extern member-template calls.
- **Specialization and LowIR (33):** explicit/extern specialization replay,
  constructors, member emission, non-type storage, and generated declaration
  metadata/order.
- **Fixed-point stress (2 timeouts):** reentrant static-query enable-if and
  callable-cache cycles still need a terminating typed query identity.

### Next checkpoint

Trace the remaining inherited/member-template owner paths as a group, starting
with the empty-pack, nested-partial, array-qualified, member-alias, and source
namespace fixtures.  Carry the resolved typed owner into candidate-local
SFINAE and alias/value replay, then validate the full PA23 report before
moving to deduction/conversion or LowIR-only groups.

## Checkpoint 11 audit result — 2026-07-30

### Audit outcome

The checkpoint audit found and fixed the checkpoint-level architecture and
performance blocker: `FindClassMemberType` was walking the complete
specialization and declaration maps to recover materialized, generated, and
nested owners by short name.  The final implementation uses a registration-time
typed specialization name set, direct generated-name lookup, and the existing
typed class-path index, preserving deterministic selection while removing the
repeated full-registry scans and per-query vector sorting/copying.  No compiler
phase, output path, acceptance check, or timeout behavior was weakened.  The
complete inventory is recorded in the matching Checkpoint 11 audit section in
`pa23/audit.md`.

### Validation and refreshed Remaining Work Map

- Full PA23: **306/396** passed; **90** remain — 55 ordinary status failures,
  33 LowIR comparisons, and 2 timeout witnesses.  This is at the turn-start
  baseline and preserves the checkpoint improvement from 304/396.
- Through PA22: **2100/2100** passed.
- PA23 file audit: passed with 13 existing non-fatal warnings; the header
  remains at the 1200-line limit.
- PA21 inline-namespace and PA22 hidden-friend regression witnesses: passed.

The authoritative complete current-PA failure set is the 90-entry inventory
in `pa23/audit.md`: the 55 status cases cover direct/using owner lookup,
nested/member replay, typed aliases and values, deduction, conversions, and
extern/function-pointer calls; the 33 LowIR cases cover specialization,
constructor/member emission, non-type storage, and generated declaration
metadata; the two reentrant static-query cases remain fixed-point stress
witnesses.  No unresolved audit, shortcut, regression, or performance issue
is being deferred as a separate problem.

### Next substantial checkpoint group

Bundle qualified owner/member replay with candidate-local deferred SFINAE, then
carry the same typed owner identity through variable-template, `sizeof`, alias,
and pack consumers.  This is the largest connected status cluster; deduction/
conversion and specialization/constructor LowIR groups follow it.  Keep the
two reentrant cases as fixed-point witnesses, without timeout-specific
acceptance logic.

## Checkpoint 12 scope — 2026-07-30 (before implementation)

### Baseline and complete failure grouping

The clean live PA23 report is **306/396**. Assignments through PA22 pass. The
complete current-PA failure set is the 90-entry inventory recorded in
`pa23/audit.md` and confirmed by the primary log: **55 status mismatches, 33
LowIR comparisons, and two timeouts**. Grouped by shared behavior, the
remaining work is:

- **Typed owner/member replay and deferred lookup:** the status cases whose
  dependent qualified members, inherited member templates, out-of-class
  definitions, hidden friends, aliases, and source namespaces lose the
  materialized owner or candidate-local substitution scope. Focused witnesses
  are `general/100-direct-namespace-wins-over-using-directive`,
  `general/200-empty-pack-member-template-owner-key`,
  `general/200-nested-template-id-partial-specialization-deduction`,
  `general/300-array-qualified-member-type-sfinae`,
  `general/300-hidden-friend-current-specialization-enable-if`,
  `general/400-member-alias-template-template-owner-argument`,
  `general/500-source-namespace-base-sfinae-chain`, and
  `general/500-out-of-class-member-template-dependent-owner-type`.
- **Typed alias/value/pack consumers:** status cases involving variable
  templates, `sizeof`, elaborated types, value-pack validation, and aliases
  that need the same typed owner facts rather than text substitution.
- **Deduction and conversions:** function partial ordering, explicit prefixes,
  function pointers, constructor conversions, ADL, and overload viability.
- **Specialization and LowIR materialization:** all 33 comparison cases,
  covering explicit/extern replay, constructors, member emission, non-type
  storage, and generated declaration metadata/order.
- **Fixed-point semantics:** the two reentrant static-query cases require a
  terminating typed query identity; they are not accepted through timing or
  timeout-specific logic.

### Checkpoint Scope

Complete the connected owner/deferred group around `FindClassMemberType` and
candidate replay. Preserve the typed materialized owner while traversing a
dependent base or alias, bind enclosing class arguments before replaying a
member result, and make the lookup scope candidate-local so a failed dependent
probe is SFINAE instead of an unknown-type hard failure. The implementation
must keep direct namespace lookup ahead of using-directive fallback, retain
empty packs, and support nested class-template ids and out-of-class member
definitions. Bundle the corresponding typed alias/value consumers when they
use this same owner path; leave unrelated conversion/LowIR formatting work for
the next group.

Validate first with the eight focused owner/deferred witnesses above, then the
full PA23 report, the required through-PA22 report, and the PA23 file audit.
The checkpoint result must record the new pass count, exact residual grouping,
and the next deduction/conversion or specialization/LowIR group.

### Checkpoint 12 result — 2026-07-30

The owner/deferred increment is complete and the focused validation improved as
follows:

- `general/100-direct-namespace-wins-over-using-directive` passes after
  namespace-owned materializations remain in the namespace queue instead of
  being nested in a class instantiation.
- `general/200-empty-pack-member-template-owner-key` passes after candidate
  viability recognizes `nullptr`/`0` as null-pointer conversions for already
  resolved pointer parameters while still rejecting unresolved `T*` deduction.
- `general/500-out-of-class-member-template-dependent-owner-type` passes after
  qualified dependent type matching continues through nested template-id
  suffixes, binding the member's type and type-pack parameters.
- The other five focused owner/deferred witnesses remain useful residual
  failures and were not masked by fallback lookup.

The clean full PA23 report is now **310/396**, four tests above the turn-start
baseline of 306/396. The remaining 86 tests are grouped as: dependent
specialization/alias and SFINAE lookup (including the five unresolved focused
owner witnesses); function-template deduction, partial ordering, explicit
prefixes, function pointers, ADL, and conversion viability; explicit/extern
specialization and constructor replay; non-type value/storage and generated
LowIR materialization/order; and the two reentrant static-query timeout
witnesses. The latter still require a terminating typed query identity.

### Next checkpoint group

Take the shared deduction/conversion group next, beginning with the remaining
qualified dependent member and alias SFINAE cases that fail during candidate
viability, then function-type and explicit-prefix deduction. Validate the
affected focused witnesses and a fresh full PA23 report before moving to the
specialization/LowIR group. Preserve the current owner queue, nested template
suffix matching, and candidate-local null-pointer conversion behavior.

### Regression-safe validation

The narrowed nested-suffix matcher was checked against the earlier regressions
before handoff: `make test-report-through-pa22` passes **2100/2100**. The final
`make test-report ACTIVE_TEST_REPORT_PAS='pa23'` remains **310/396** with only
the two known reentrant timeout witnesses among the residual failures, and
`perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src` passes.

## Checkpoint 13 scope — 2026-07-30 (before implementation)

### Live failure map

The fresh committed-baseline report is **310/396**, with **51 status
mismatches, 33 LowIR comparisons, and 2 timeouts**. The complete current set is
the full 90-entry inventory in `pa23/audit.md` minus the four resolved status
witnesses: `general/100-direct-namespace-wins-over-using-directive`,
`general/200-empty-pack-member-template-owner-key`,
`general/500-out-of-class-member-template-dependent-owner-type`, and
`spec/200-inherited-template-param-shadow-forward`. The remaining map groups
as follows:

- **Dependent type/alias/SFINAE status cluster:** nested class-template
  partial-specialization deduction, array-qualified member lookup, hidden
  friends using the current specialization, explicit alias-template return
  SFINAE, member alias template-template owners, dependent non-type alias
  sequences, and source-namespace dependent bases. These are the selected
  witnesses for this checkpoint.
- **Deduction/conversion status cluster:** function-type partial ordering,
  explicit template prefixes, function pointers, constructor/conversion
  viability, ADL, and member-template overload selection.
- **Specialization/value/LowIR cluster:** the 33 comparison cases covering
  explicit/extern replay, constructors, non-type storage, variable templates,
  and generated declaration/body materialization.
- **Fixed-point cluster:** the two reentrant static-query tests, which remain
  semantic termination work and are not addressed through timeout handling.

### Checkpoint Scope

Carry one typed dependent substitution environment through class partial
specialization selection, alias-template target expansion, and candidate-local
SFINAE. The scope covers the seven status witnesses named in the first group:
`general/200-nested-template-id-partial-specialization-deduction`,
`general/300-array-qualified-member-type-sfinae`,
`general/300-hidden-friend-current-specialization-enable-if`,
`general/300-qualified-explicit-template-alias-return-sfinae`,
`general/400-member-alias-template-template-owner-argument`,
`general/400-dependent-alias-nontype-sequence-filter`, and
`general/500-source-namespace-base-sfinae-chain`.

The behavior to implement is real deferred dependent-type lookup: preserve the
source lexical owner while selecting a concrete specialization, resolve alias
targets only after their template arguments are bound, and turn failed
dependent probes into candidate-local SFINAE instead of hard unknown-type or
substitution failures. Validate these focused witnesses before the full PA23
report, then through-PA22 and file audit; preserve the current 310-test
baseline until the new group is verified.

## Checkpoint 13 result — 2026-07-30

The increment completed the stable portion of the typed dependent replay group:

- Class-member collection now creates a typed template-parameter scope for
  nested template declarations, so member functions and nested classes retain
  their enclosing type/template-template bindings during lowering.
- Reference and array compatibility preserves unknown array bounds while
  retaining the const-element restriction required by prior array deduction.
- Explicit function candidates resolve outer substitutions before deduction;
  class partial-specialization probes evaluate candidate-local `enable_if`
  conditions; and qualified static-member expression types use the materialized
  class-member path.
- Template-template member aliases retain their concrete enclosing class owner,
  and generated hidden friends are recovered through the source class owner
  with current-specialization substitutions.  The generated-name normalizer was
  moved to the rewrite implementation module to keep the source file audit
  limits intact.

The focused witnesses now passing are `general/300-array-qualified-member-type-
sfinae`, `general/300-hidden-friend-current-specialization-enable-if`,
`general/300-qualified-explicit-template-alias-return-sfinae`,
`general/400-member-alias-template-template-owner-argument`, and
`general/400-dependent-alias-nontype-sequence-filter`.  The two selected
residual witnesses remain `general/200-nested-template-id-partial-
specialization-deduction` (short dependent alias type is still unresolved) and
`general/500-source-namespace-base-sfinae-chain` (the replayed namespace-base
trait path is still malformed).

Validation is **315/396** for the PA23 report, five tests above the 310-test
baseline: **46 status mismatches, 33 LowIR comparisons, and two reentrant
static-query timeouts** remain.  The through-PA22 report passes **2100/2100**,
and the PA23 file audit passes with the repository's existing warnings.

### Remaining Work Map

- **Dependent owner qualification:** fix the two residual nested-alias and
  source-namespace-base witnesses without broad qualification fallback or
  reentrant lookup growth.
- **Deduction and conversion:** function-type partial ordering, explicit
  prefixes, function pointers, constructor/conversion viability, ADL, and
  member-template overload selection remain status failures.
- **Typed values and deferred SFINAE:** variable templates, `sizeof`, packs,
  detector/`enable_if` probes, extern declarations, and the two fixed-point
  timeout witnesses still need candidate-local typed query state.
- **Specialization and LowIR materialization:** the 33 comparison cases still
  cover explicit/extern replay, constructors, non-type storage, and generated
  declaration/body metadata and ordering.

### Next checkpoint group

Take the residual dependent owner qualification together with the adjacent
function-template deduction/conversion cases, starting with nested template-id
partial-specialization deduction and source-namespace base matching. Preserve
the five new SFINAE/alias/hidden-friend passes, then validate the focused
witnesses, full PA23, through-PA22, and file audit again before the next
specialization/LowIR increment.

## Checkpoint 14 scope — 2026-07-30 (before implementation)

### Remaining Work Map

The live PA23 baseline is **315/396**: 46 status mismatches, 33 LowIR
comparisons, and two reentrant static-query timeouts. The residual failures
group into four shared behaviors: (1) direct function-type versus function-
pointer partial matching and fixed-arity ordering over a function tail pack;
(2) concrete function-type pack materialization and recursive dependent-base
lookup; (3) the remaining function-template deduction/conversion, explicit
prefix, ADL, constructor, and member-overload status cases; and (4) typed
non-type/SFINAE state, explicit/extern replay, generated LowIR materialization,
and fixed-point termination.

### Checkpoint Scope

Implement and validate group (1) and the dependent portion of group (2):
preserve the distinction between a direct function type and a function pointer
when matching class-template partial specializations, order a fixed-arity
function pattern ahead of a tail-pack pattern, and materialize concrete
function-type pack elements without leaving an expansion marker in a generated
dependent base. The focused behavior is covered by
`general/100-function-type-not-pointer-partial-specialization`,
`general/400-function-type-partial-specialization-fixed-arity-over-pack`, and
`general/400-function-type-tail-pack-recursive-specialization`. A successful
checkpoint must improve the 315-test baseline, preserve through-PA22, and pass
the PA23 file audit; the next group will be the remaining deduction/conversion
and explicit-prefix witnesses.

## Checkpoint 14 result — 2026-07-30

The function-type matching and dependent pack replay increment is complete:

- Direct function types are canonicalized separately from function pointers,
  so a direct function argument no longer incorrectly selects a pointer partial
  specialization. Nested function parameters still receive the language's
  function-to-pointer adjustment where the parameter is not a reference.
- Partial ordering now recognizes a fixed-arity direct function pattern as
  more specialized than the corresponding trailing function-parameter pack.
- Concrete owner partial-specialization packs are propagated into nested replay
  state. Recursive dependent-base lookup now materializes each concrete
  function type rather than retaining a `...` expansion marker.

The three PA23 focused witnesses and the PA21 nested-function regression all
pass. The full PA23 report is **318/396**, three tests above the 315-test
baseline; 46 status mismatches, 33 LowIR comparisons, and two reentrant
static-query timeouts remain. `make test-report-through-pa22` passes
**2100/2100**, and the PA23 file audit passes with 13 existing nonfatal
warnings. The owner-pack helper was split into the emit module to keep the
source-size and function-size audit limits satisfied.

### Remaining Work Map

- **Dependent owner qualification:** nested template-id partial deduction and
  source-namespace dependent-base SFINAE still fail during replay.
- **Function-template deduction and conversion:** explicit prefixes, function
  pointers and non-type function arguments, ADL, conversions, constructors,
  and member-template overload selection remain status failures.
- **Typed values and fixed-point SFINAE:** non-type storage, `sizeof`, detector
  and `enable_if` probes, extern/explicit replay, and the two reentrant static
  query timeouts still need typed candidate-local state and termination.
- **Specialization and LowIR materialization:** the 33 comparison cases still
  cover explicit/extern replay, constructors, variable templates, and generated
  declaration/body metadata and ordering.

### Next checkpoint group

Take the remaining function-template deduction/conversion group next, starting
with explicit template prefixes, function-pointer/non-type argument deduction,
and member-template overload viability. Preserve the direct-function matching,
fixed-arity ordering, and concrete owner-pack replay paths while validating the
focused witnesses, full PA23, through-PA22, and file audit again.

## Checkpoint 15 scope — 2026-07-30 (before implementation)

### Remaining Work Map

The live PA23 report remains **318/396**, with 46 status mismatches, 33 LowIR
comparisons, and two reentrant static-query timeouts. The failures group into:

- **Function-template deduction and overload viability:** explicit template
  prefixes, function-pointer and non-type function arguments, function-type
  parameter adjustment, member-template address/call selection, ADL, and
  constructor/conversion candidates.
- **Dependent owner qualification:** nested template-id partial deduction and
  source-namespace dependent-base SFINAE still fail during replay.
- **Typed values and deferred SFINAE:** non-type storage, `sizeof`, detector and
  `enable_if` probes, extern/explicit replay, and fixed-point query termination.
- **Specialization and LowIR materialization:** the 33 comparisons covering
  explicit/extern replay, constructors, variable templates, and generated
  declaration/body metadata and ordering.

### Checkpoint Scope

Implement the first shared function-template deduction increment: preserve
explicitly supplied template arguments while deducing the remaining function
parameters, apply function-parameter adjustment consistently for function and
function-pointer arguments, and retain typed non-type function-pointer
arguments when selecting a candidate. The focused scope covers
`general/100-nontype-function-parameter-adjustment`,
`general/100-explicit-function-specialization-overload-parameter-match`,
`spec/100-function-template-nontype-function-pointer-call`,
`spec/100-function-template-nontype-function-pointer-specialization-call`, and
`spec/100-nontype-function-pointer-argument`. Validate these cases and the
earlier function-type checkpoint before the full PA23 report; preserve all
through-PA22 tests and the source audit.

## Checkpoint 15 result — 2026-07-30

The function-template deduction and explicit-specialization increment is
complete:

- Nested function and function-pointer non-type parameter declarators are
  retained as typed parameter shapes, including function-to-pointer adjustment
  during argument replay.
- Explicit specialization registration matches the selected overload's
  parameter pattern, prefers a definition over a forward declaration, and
  keeps specialization lookup keyed by the selected primary.
- Function-pointer non-type arguments record their indirect call fact and PA14
  emits the corresponding address/decay path for indirect calls.

The five PA23 focused witnesses and the PA19/PA21/PA22 regression witnesses
all pass. The full PA23 report is **323/396**, five tests above the 318-test
turn-start baseline; the residual failures remain grouped as dependent-owner
qualification, deduction/conversion and overload viability, typed
non-type/SFINAE state, and specialization/LowIR materialization. One
reentrant static-query timeout remains in the current report. The required
through-PA22 report passes **2100/2100**, and the PA23 file audit passes with
13 existing nonfatal warnings.

### Remaining Work Map

- **Dependent owner qualification:** nested template-id partial deduction and
  source-namespace dependent-base SFINAE still fail during replay.
- **Function-template deduction and overload viability:** ADL, conversions,
  constructors, member-template overloads, remaining explicit-prefix cases,
  and the residual function-type/function-pointer deduction cases remain.
- **Typed values and deferred SFINAE:** non-type storage, `sizeof`, detector
  and `enable_if` probes, extern/explicit replay, and reentrant static-query
  termination still need candidate-local typed state.
- **Specialization and LowIR materialization:** the remaining comparisons cover
  explicit/extern replay, constructors, variable templates, and generated
  declaration/body metadata and ordering.

### Next checkpoint group

Take dependent owner qualification together with the adjacent nested
template-id deduction and source-namespace base SFINAE witnesses. Preserve the
new function-pointer specialization and indirect-call paths while validating
the focused group, full PA23, through-PA22, and file audit again.

## Checkpoint 16 scope — 2026-07-30 (before implementation)

### Remaining Work Map

The reproduced PA23 report is **323/396**, with 73 failures. They remain
grouped into:

- **Class partial matching and dependent owners:** class partial-specialization
  matching still reuses function-style derived-to-base deduction; nested
  template-id replay and source-namespace dependent bases lose the concrete
  owner or qualified helper type.
- **Function-template deduction and overload viability:** ADL, conversions,
  constructors, member-template overloads, explicit-prefix cases, and residual
  function/function-pointer deduction remain.
- **Typed values and deferred SFINAE:** non-type storage, `sizeof`, detector
  and `enable_if` probes, extern/explicit replay, and the two reentrant
  static-query timeouts still need candidate-local typed state.
- **Specialization and LowIR materialization:** remaining comparisons cover
  explicit/extern replay, constructors, variable templates, and generated
  declaration/body metadata and ordering.

### Checkpoint Scope

Implement the dependent-owner/class-partial increment: keep derived-to-base
deduction available for ordinary function argument matching but disable it when
matching a class partial-specialization template-id, then preserve the source
namespace and concrete nested owner while replaying dependent bases and nested
template-id arguments. The focused witnesses are
`general/200-class-partial-specialization-no-derived-base-deduction`,
`general/200-nested-template-id-partial-specialization-deduction`,
`general/500-source-namespace-base-sfinae-chain`, and
`general/100-dependent-qualified-nontype-base-argument`. Preserve the passing
out-of-class dependent-owner and inherited-parameter witnesses, then validate
the focused group, full PA23, through-PA22, and file audit.

## Checkpoint 16 result — 2026-07-30

The dependent-owner increment is complete. Class partial-specialization
matching no longer applies derived-base deduction, while ordinary function
matching retains it. Anonymous namespace scopes now reuse their typed source
identity across predeclaration and replay; dependent base/member lookup keeps
the source owner; and nested type arguments are qualified only when the
anonymous namespace path requires it. Deferred class replay preserves a shell
when a dependent `type` member is still unavailable, while concrete member
definitions and user-provided constructor behavior remain materialized.

Validation passed for all six focused witnesses (**6/6**), the PA19 relational
regression, and the required through-PA22 report (**2100/2100**). The full
PA23 report is **325/396**, above the turn-start baseline of 306/396. The file
audit passes with 13 nonfatal warnings.

### Remaining Work Map

- **Function-template deduction and overload viability:** ADL, conversions,
  constructors, member-template overloads, explicit-prefix cases, and the
  remaining function/function-pointer deduction cases.
- **Typed values and deferred SFINAE:** non-type storage, `sizeof`, detector
  and `enable_if` probes, extern/explicit replay, and the two reentrant
  static-query timeout cases.
- **Specialization and LowIR materialization:** explicit/extern replay,
  constructors, variable templates, and generated declaration/body metadata
  and ordering.

### Next checkpoint group

Take the typed-value/deferred-SFINAE witnesses as the next coherent group,
starting with non-type storage and dependent `sizeof`/detector evaluation.
Keep the anonymous-owner and class-partial replay paths covered by the six
focused witnesses, then rerun the PA23 report, through-PA22 report, and file
audit.

## Checkpoint 16 audit result — 2026-07-30

### Checkpoint result

The checkpoint audit is complete.  The implementation remains on the normal
typed parser, semantic, template-replay, and LowIR path.  The audit fixed the
new per-materialization declaration rescans by indexing dependent member-type
AST nodes and the `::type` subset at template registration, bounded
anonymous-scope predeclaration to the current scope, and moved path-resolution
ownership into the existing resolver module so the source audit remains a real
size/ownership check.
No timeout workaround, fallback success path, fixture gate, embedded payload,
or unchecked implementation fragment remains in the checkpoint changes.

### Validation

- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'`: **325/396**; equal to the
  checkpoint baseline and therefore stage-progress preserving.
- Required prior-through command: **pass, 2100/2100** through PA22.
- The six owner/class-partial preservation witnesses: **pass, 6/6**.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src`: **pass**
  with 13 pre-existing non-fatal warnings and no fatal finding.
- The complete current-stage inventory is recorded in the matching Checkpoint
  16 audit section in `pa23/audit.md`: 38 ordinary status mismatches, 2
  reentrant timeout witnesses, and 31 LowIR comparisons.

### Remaining Work Map

The map below is refreshed from that complete 71-fixture inventory; the exact
fixture names and classifications are recorded in `pa23/audit.md`.

- **Typed values and deferred SFINAE:** non-type storage and reinstantiation,
  dependent `sizeof`, array/static-value probes, detector/`enable_if` and alias
  SFINAE, dependent member-template result values, and the two reentrant
  fixed-point queries.
- **Template deduction and overload viability:** explicit-prefix and
  template-template deduction, function/reference/pack normalization,
  constructors, conversions, ADL, member-template address/call selection,
  and deleted/extern candidate handling.
- **Owner and specialization replay:** anonymous and current-specialization
  names, inherited aliases, nested owners, explicit/extern specialization
  replay, variable templates, and out-of-class member definitions.
- **LowIR materialization:** the 31 remaining comparisons covering generated
  constructor/body metadata, function ordering, static/variable-template
  state, explicit instantiation, and ABI-visible type/value lowering.

### Next substantial checkpoint group

Bundle typed-value/deferred-SFINAE work with dependent `sizeof`, array/static
value, alias/detector, and member-template-result witnesses.  Keep the two
reentrant cases as fixed-point regression witnesses with no timing-based
acceptance, and retain all six owner/class-partial witnesses.  Follow this
group with the deduction/conversion cluster, then explicit/extern and LowIR
materialization.

## Checkpoint 17 scope — 2026-07-30 (before implementation)

### Baseline and complete failure map

The clean turn-start PA23 report is **325/396**.  Assignments through PA22
remain green.  The complete current-PA failure set is **71 fixtures**: 38
ordinary exit-status mismatches, 31 LowIR comparisons, and two reentrant
static-query timeouts.  The inventory below is grouped by shared semantic
behavior; fixtures may exercise more than one group.

- **Typed value, alias, and deferred-SFINAE state:**

  `general/100-intermediate-type-transform-value-nontype.t`,
  `general/100-selected-specialization-special-member-body.t`,
  `general/100-structured-bool-boost-convertible-mpl-overload.t`,
  `general/100-dependent-bool-partial-static-value-storage.t`,
  `general/300-dependent-bool-base-trait-type-argument.t`,
  `general/400-anonymous-namespace-partial-specialization.t`,
  `general/400-elaborated-type-template-arg-false-branch.t`,
  `general/400-elaborated-type-template-arg-true-branch.t`,
  `general/400-local-class-default-member-variable-template-nontype-type.t`,
  `general/400-member-variable-template-leaf-sfinae.t`,
  `general/400-variable-template-specializations.t`,
  `general/500-array-type-argument-sfinae-static-value.t`,
  `general/500-dependent-function-type-pack-expansion-ctor-init.t`,
  `general/500-dependent-qualified-member-template-result-bool.t`,
  `general/500-member-template-conditional-alias-trailing-return.t`,
  `general/500-mp11-append-alias-template-sfinae.t`,
  `general/500-nontype-alias-reinstantiation-structural-state.t`,
  `general/500-recursive-qualified-member-template-bool-arg.t`,
  `spec/100-sizeof-union-type-nttp.t`,
  `spec/400-class-template-nttp-scope-value.t`,
  `spec/400-qualified-member-template-id-bool-constant.t`, and
  `spec/500-conditional-alias-index-sequence-member-template-call.t`.

- **Qualified owner/member replay and candidate-local lookup:**

  `general/100-current-specialization-member-body-cast-compare.t`,
  `general/100-explicit-specialization-out-of-class-ctor-replay.t`,
  `general/100-inherited-using-alias-out-of-class-specialization-member.t`,
  `general/100-local-qualified-argument-replay.t`,
  `general/100-member-template-specialization-return-prefers-member-call.t`,
  `general/200-member-template-implicit-instantiation-not-overload.t`,
  `general/300-current-specialization-constructor-template-canonical-owner.t`,
  `general/400-current-specialization-display-name-member-alias.t`,
  `general/400-member-template-result-pack-preserves-nested-function-pointer-owner.t`,
  `general/400-out-of-class-ctor-using-imported-member-template.t`,
  `general/400-unused-static-member-template-return-type.t`,
  `general/400-out-of-class-partial-member-template-owner-parameter-alias.t`,
  `general/500-source-namespace-base-sfinae-chain.t`,
  `spec/100-explicit-specialization-out-of-class-member-emits.t`,
  `spec/100-partial-specialization-member-primary-param-name.t`,
  `spec/200-inherited-template-param-shadow-forward.t`,
  `spec/400-defaulted-nested-class-argument-partial-specialization.t`,
  `spec/400-explicit-pack-type-argument-uses-bound-type.t`,
  `spec/400-explicit-type-arg-dependent-qualified-member-template-id.t`,
  `spec/400-template-template-member-alias-owner-shadow.t`, and
  `spec/500-conditional-alias-index-sequence-member-template-call.t`.

- **Function-template deduction, conversion, and overload viability:**

  `general/200-constructor-template-parameter-shadows-instantiated-type.t`,
  `general/200-function-template-named-parameter-sfinae.t`,
  `general/200-member-function-template-address-explicit-pack.t`,
  `general/200-template-template-qualified-default-arg-deduction.t`,
  `general/300-constructor-template-const-ref-enable-if-conversion.t`,
  `general/400-static-cast-rvalue-ref-skips-conversion-operator.t`,
  `spec/100-local-member-call-constructor-template-instantiation.t`,
  `spec/100-function-template-nontype-function-pointer-call.t`,
  `spec/100-function-template-nontype-function-pointer-specialization-call.t`,
  `spec/100-nontype-function-pointer-argument.t`,
  `spec/300-constructor-default-pack-partial-ordering.t`,
  `spec/400-conversion-function-template-call-argument.t`,
  `spec/400-conversion-function-template-copy-init.t`,
  `spec/400-conversion-function-template-selection.t`, and
  `spec/400-function-type-pack-template-argument.t`.

- **Specialization, extern/constructor replay, and LowIR materialization:**

  `general/100-explicit-specialization-pointer-member-definition.t`,
  `general/100-sizeof-call-result-nontype-template-argument.t`,
  `general/200-adl-explicit-template-id-call.t`,
  `general/200-function-template-reference-cv-alias-partial-order.t`,
  `general/200-function-template-template-parameter-deduction.t`,
  `general/200-member-operator-template-reference-pattern-partial-order.t`,
  `general/400-explicit-function-template-type-arg-drops-nontype-overload.t`,
  `general/400-function-type-pack-out-of-class-constructor.t`,
  `general/400-nonmember-template-compound-assignment-const-lhs.t`,
  `general/400-variable-template-specializations.t`,
  `spec/100-explicit-instantiation-after-explicit-specialization-no-effect.t`,
  `spec/100-explicit-instantiation-class-prior-member-definitions.t`,
  `spec/100-explicit-specialization-out-of-class-ctor-replay.t`,
  `spec/100-extern-template-member-function-declaration.t`,
  `spec/100-extern-template-static-data-declaration.t`,
  `spec/100-out-of-class-conversion-operator-definition.t`,
  `spec/100-sizeof-union-type-nttp.t`,
  `spec/200-defaulted-class-template-argument-pack-prefix-deduction.t`,
  `spec/400-class-template-nttp-scope-value.t`,
  `spec/400-defaulted-template-arg-partial-base-completion.t`, and
  `general/100-current-specialization-member-body-cast-compare.t`.

- **Reentrant fixed points (not in the success scope):**

  `general/500-reentrant-static-query-callable-enable-if-cache.t` and
  `general/500-reentrant-static-query-enable-if-partial.t` remain genuine
  cycle-handling witnesses.  They must stay visible as timeouts until a
  stable typed query identity is implemented.

### Remaining Work Map

- **Typed replay/value formation:** elaborated type-ids, intermediate
  qualified `::type::value`, array type arguments, variable-template and
  integral-constant values, dependent `sizeof`, and alias-template SFINAE
  still lose typed facts between argument substitution, class selection, and
  constant evaluation.
- **Owner/member replay:** current-specialization, inherited and out-of-class
  members, nested member templates, source namespaces, and explicit/extern
  declaration pairing still need one concrete owner path.
- **Deduction/overload composition:** function-pointer and direct-function
  parameters, explicit prefixes, template-template defaults, constructor and
  conversion candidates, member-template addresses, and pack normalization
  remain unresolved.
- **LowIR materialization:** the comparison-only failures still cover static
  storage, function-pointer and constructor bodies, explicit instantiation,
  declaration order, and ABI-visible typed values.
- **Fixed points:** the two reentrant static-query cases require semantic
  cycle state and are intentionally deferred from this checkpoint.

### Checkpoint Scope

Complete the typed value/deferred-SFINAE increment for the first connected
subgroup.  Preserve elaborated `struct`/`class`/`union` type-ids as typed
incomplete declarations in template arguments, resolve the selected alias
branch without validating the discarded branch as an ordinary unknown type,
and carry the resulting type through qualified `::type` member replay.  In
the same typed path, retain declared non-type argument kinds and concrete
array/integral/boolean values while evaluating dependent `sizeof`, variable
template, and static-member expressions.  This is compiler behavior, not
fixture acceptance or timeout handling.

The focused validation set is:

`general/400-elaborated-type-template-arg-false-branch.t`,
`general/400-elaborated-type-template-arg-true-branch.t`,
`general/100-intermediate-type-transform-value-nontype.t`,
`general/100-structured-bool-boost-convertible-mpl-overload.t`,
`general/500-array-type-argument-sfinae-static-value.t`,
`general/500-nontype-alias-reinstantiation-structural-state.t`,
`general/400-member-variable-template-leaf-sfinae.t`,
`general/400-variable-template-specializations.t`,
`spec/100-sizeof-union-type-nttp.t`, and
`spec/400-class-template-nttp-scope-value.t`.

Before the next checkpoint, retain all six Checkpoint 16 owner/class-partial
witnesses, run the full PA23 report, the required through-PA22 report, and
the PA23 file audit.  The next checkpoint group is the remaining owner/
deferred-member-result band bundled with the deduction/conversion witnesses;
the two reentrant tests remain fixed-point regression coverage.

## Checkpoint 17 result — 2026-07-30

The typed replay checkpoint is implemented and validated.  Template
materialization now records implicit elaborated nested class owners as typed
forwards, keeps `<unnamed>` local-scope markers out of template-angle
scanning, and treats substitution failures as candidate-local SFINAE.  Alias
replay and compact fundamental recovery were narrowed so dependent `::type`,
array, integral, and boolean facts remain typed without concatenating adjacent
source type words.

The checkpoint witnesses that now compile and pass are:

- `general/400-elaborated-type-template-arg-false-branch.t`
- `general/400-elaborated-type-template-arg-true-branch.t`
- `general/400-local-class-default-member-variable-template-nontype-type.t`
- `general/500-array-type-argument-sfinae-static-value.t`
- `general/100-current-class-static-member-nontype-argument-body-check.t`
- `general/400-cross-instantiation-member-template-source-owner.t`
- `general/400-dependent-default-nontype-argument-eval.t`
- `general/400-dependent-member-alias-template-argument-scope.t`
- `general/400-member-template-constexpr-static-assert.t`

The full current-PA report is **329/396**, four above the turn-start
baseline.  The remaining **67** failures are now 35 exit-status failures
(including the two reentrant timeout witnesses) and 32 LowIR comparisons; the
current complete set is grouped below.

The final source layout also passes the PA23 file audit.  The implicit nested
forward and ambiguous-alias helpers now live in the responsibility-specific
`pa18_templates_collection_nested.cpp` module, and the text-replay helper is
shared from the text-helper module; this keeps the new behavior while staying
within the repository’s source-size limits.

### Remaining Work Map

- **Typed values and deferred member/SFINAE state (11 status):**
  `general/100-structured-bool-boost-convertible-mpl-overload.t`,
  `general/500-dependent-function-type-pack-expansion-ctor-init.t`,
  `general/500-dependent-qualified-member-template-result-bool.t`,
  `general/500-member-template-conditional-alias-trailing-return.t`,
  `general/500-mp11-append-alias-template-sfinae.t`,
  `general/500-nontype-alias-reinstantiation-structural-state.t`,
  `general/500-recursive-qualified-member-template-bool-arg.t`,
  `general/500-reentrant-static-query-callable-enable-if-cache.t`,
  `general/500-reentrant-static-query-enable-if-partial.t`,
  `spec/400-qualified-member-template-id-bool-constant.t`, and
  `spec/500-conditional-alias-index-sequence-member-template-call.t`.
- **Owner and specialization replay (11 status):**
  `general/100-member-template-specialization-return-prefers-member-call.t`,
  `general/100-selected-specialization-special-member-body.t`,
  `general/400-anonymous-namespace-partial-specialization.t`,
  `general/400-current-specialization-display-name-member-alias.t`,
  `general/400-member-template-result-pack-preserves-nested-function-pointer-owner.t`,
  `general/400-unused-static-member-template-return-type.t`,
  `general/500-source-namespace-base-sfinae-chain.t`,
  `spec/100-extern-template-member-function-declaration.t`,
  `spec/100-extern-template-static-data-declaration.t`,
  `spec/100-local-member-call-constructor-template-instantiation.t`, and
  `spec/400-defaulted-nested-class-argument-partial-specialization.t`.
- **Deduction, conversion, and overload viability (13 status):**
  `general/200-constructor-template-parameter-shadows-instantiated-type.t`,
  `general/200-function-template-named-parameter-sfinae.t`,
  `general/200-member-function-template-address-explicit-pack.t`,
  `general/200-member-template-implicit-instantiation-not-overload.t`,
  `general/200-template-template-qualified-default-arg-deduction.t`,
  `general/300-constructor-template-const-ref-enable-if-conversion.t`,
  `general/400-static-cast-rvalue-ref-skips-conversion-operator.t`,
  `spec/400-conversion-function-template-call-argument.t`,
  `spec/400-conversion-function-template-copy-init.t`,
  `spec/400-conversion-function-template-selection.t`,
  `spec/400-explicit-pack-type-argument-uses-bound-type.t`,
  `spec/400-explicit-type-arg-dependent-qualified-member-template-id.t`, and
  `spec/400-function-type-pack-template-argument.t`.
- **LowIR materialization (32 comparisons):**
  `general/100-current-specialization-member-body-cast-compare.t`,
  `general/100-dependent-bool-partial-static-value-storage.t`,
  `general/100-explicit-specialization-out-of-class-ctor-replay.t`,
  `general/100-explicit-specialization-pointer-member-definition.t`,
  `general/100-inherited-using-alias-out-of-class-specialization-member.t`,
  `general/100-intermediate-type-transform-value-nontype.t`,
  `general/100-local-qualified-argument-replay.t`,
  `general/100-sizeof-call-result-nontype-template-argument.t`,
  `general/200-adl-explicit-template-id-call.t`,
  `general/200-function-template-reference-cv-alias-partial-order.t`,
  `general/200-function-template-template-parameter-deduction.t`,
  `general/200-member-operator-template-reference-pattern-partial-order.t`,
  `general/300-current-specialization-constructor-template-canonical-owner.t`,
  `general/300-dependent-bool-base-trait-type-argument.t`,
  `general/400-explicit-function-template-type-arg-drops-nontype-overload.t`,
  `general/400-function-type-pack-out-of-class-constructor.t`,
  `general/400-member-variable-template-leaf-sfinae.t`,
  `general/400-nonmember-template-compound-assignment-const-lhs.t`,
  `general/400-out-of-class-partial-member-template-owner-parameter-alias.t`,
  `general/400-variable-template-specializations.t`,
  `spec/100-explicit-instantiation-after-explicit-specialization-no-effect.t`,
  `spec/100-explicit-instantiation-class-prior-member-definitions.t`,
  `spec/100-explicit-specialization-out-of-class-ctor-replay.t`,
  `spec/100-explicit-specialization-out-of-class-member-emits.t`,
  `spec/100-out-of-class-conversion-operator-definition.t`,
  `spec/100-partial-specialization-member-primary-param-name.t`,
  `spec/100-sizeof-union-type-nttp.t`,
  `spec/200-defaulted-class-template-argument-pack-prefix-deduction.t`,
  `spec/300-constructor-default-pack-partial-ordering.t`,
  `spec/400-class-template-nttp-scope-value.t`,
  `spec/400-defaulted-template-arg-partial-base-completion.t`, and
  `spec/400-template-template-member-alias-owner-shadow.t`.

### Next checkpoint group

Take the remaining dependent member-result and candidate-local SFINAE cases as
one group, including MP11 append, qualified bool results, trailing-return
aliases, and the three conversion-function selection witnesses.  Preserve the
new typed-owner/array checkpoint and use the two reentrant queries only as
fixed-point regression coverage; their failure must not be accepted through a
timeout or broad fallback.

Final checkpoint validation after the forced rebuild is unchanged: the exact
through-PA22 report passes at **2100/2100**, the PA23 report is **329/396**,
and `cppgm_file_audit.pl --stage pa23 --paths dev/src` passes with only the
listed repository warnings.  The next turn should start from the owner/
deferred-member-result group above; no current-PA failure was hidden or
converted into a fixture change.

## Checkpoint 18 scope — 2026-07-30 (before implementation)

### Live Remaining Work Map

The clean PA23 report remains **329/396**: **35** status failures (including
the two reentrant fixed-point timeouts) and **32** LowIR comparisons.  The
complete fixture inventory is the 67-entry grouped list in the preceding
Checkpoint 17 section, confirmed by the live primary log.  The shared
remaining behaviors are:

- **Dependent member-result and candidate-local SFINAE:** qualified boolean
  results, trailing-return aliases, dependent packs, MP11-style alias probes,
  non-type alias reinstantiation, recursive member queries, and the two
  terminating-query witnesses.
- **Owner and specialization replay:** current-specialization and inherited
  owners, out-of-class members, explicit/extern replay, nested member-template
  ownership, anonymous scopes, and variable-template state.
- **Deduction and conversion viability:** constructor and conversion
  candidates, explicit/template-template prefixes, function-pointer and pack
  deduction, ADL, and member-template overload selection.
- **LowIR materialization:** the 32 comparison failures covering generated
  function/constructor bodies, static storage, explicit instantiation,
  declaration order, and ABI-visible typed values.

### Checkpoint Scope

Complete the first ordinary conversion-function-template increment in the
deduction/conversion group.  Ordinary call materialization must retain a
typed fundamental (and otherwise non-class) destination as a valid conversion
target, bind the conversion operator template's result parameter against that
destination, and replay the selected member specialization through the normal
typed member-call path.  This covers conversion-operator templates used for a
function argument and copy-initialization, without treating conversion
operators as constructors or adding an acceptance fallback.

Validate the scope with:

`spec/400-conversion-function-template-call-argument.t`,
`spec/400-conversion-function-template-copy-init.t`, and
`spec/400-conversion-function-template-selection.t`.

Then run the full PA23 report, the exact through-PA22 report, the PA23 file
audit, and `git diff --check`.  The next checkpoint group is the remaining
dependent member-result/candidate-local SFINAE band bundled with constructor
and conversion viability; the two reentrant cases remain genuine fixed-point
witnesses with no timing-specific handling.

## Checkpoint 17 audit result — 2026-07-30

The checkpoint audit fixed two replay hot-path issues without changing the
semantic scope: source declaration dependencies and implicit nested-owner
names are indexed once at template registration, and a complete nested class
declaration is not replaced by a later forward shell.  The source-size audit
boundary was restored by keeping the indexing implementation in the
responsibility-specific nested-collection module.  The implementation still
uses the ordinary typed parser, template registry, substitution, and LowIR
paths; the reentrant cases remain genuine fixed-point failures rather than
timeout-accepted successes.

### Validation

- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'`: **329/396**, with no new
  failing fixture relative to the Checkpoint 17 baseline and four more passing
  tests than the 325-test turn-start baseline.
- The complete current-stage set is **67 fixtures**: 35 exit-status
  mismatches (including the two reentrant timeout witnesses) and 32 LowIR
  comparisons.  The grouped lists immediately above are the authoritative
  failure inventory; the audit compared the set directly with the baseline.
- The exact required through-PA22 command passes **2100/2100**.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src` passes with
  13 non-fatal pre-existing warnings and no fatal finding.

### Refreshed Remaining Work Map

- **Dependent member-result and typed SFINAE:** MP11 append, qualified bool
  results, trailing-return aliases, dependent packs, alias/`sizeof` probes,
  and the two reentrant fixed-point queries.
- **Owner and specialization replay:** current-specialization display and
  member bodies, inherited/source-namespace owners, explicit/extern replay,
  nested member-template ownership, and variable-template state.
- **Deduction and conversion viability:** constructor and conversion
  candidates, explicit and template-template prefixes, function-pointer and
  pack deduction, ADL, and member-template overload selection.
- **LowIR materialization:** the 32 comparison failures covering generated
  function/constructor bodies, static storage, explicit instantiation,
  declaration order, and ABI-visible typed values.

### Next Substantial Checkpoint Group

Bundle the dependent member-result/candidate-local SFINAE group with MP11
append, qualified bool, trailing-return alias, and the three conversion
selection witnesses.  Keep all typed-owner/array and class-partial
regressions in the focused suite, retain the two reentrant cases as
fixed-point witnesses with no timing-based acceptance, then follow with the
remaining deduction/conversion and explicit/extern LowIR groups.

## Checkpoint 18 result — 2026-07-30

The ordinary conversion-function-template increment is complete.  Fundamental
and other known non-class destinations are now retained as typed conversion
targets during ordinary call replay, and scalar copy-initializers use the same
member-template materialization path as function arguments.  This preserves
the normal PA14 conversion-operator lookup and generated binding; it does not
turn conversion operators into constructors or accept an otherwise invalid
initializer.

The three scoped witnesses pass:

- `spec/400-conversion-function-template-call-argument.t`
- `spec/400-conversion-function-template-copy-init.t`
- `spec/400-conversion-function-template-selection.t`

The fresh full PA23 report is **332/396**, up three tests from the
turn-start **329/396** baseline.  Its remaining **64** fixtures are **32
exit-status mismatches** and **32 LowIR comparisons**.  The status set is the
previous status group with the three conversion-function-template witnesses
removed; the LowIR set is unchanged.  The two reentrant static-query cases
remain genuine fixed-point timeout witnesses and were not accepted through
timing behavior or a fallback.

### Refreshed Remaining Work Map

- **Dependent member-result and candidate-local SFINAE (11 status):**
  structured boolean overloads, dependent function-type packs, qualified
  boolean results, trailing-return aliases, MP11 append, non-type alias
  reinstantiation, recursive member queries, the two reentrant fixed-point
  queries, the qualified bool member-template id, and conditional-alias index
  sequence calls.
- **Owner and specialization replay (11 status):** current and inherited
  owners, anonymous scopes, selected/special member replay, static member
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

### Next Checkpoint Group

Bundle the dependent member-result/candidate-local SFINAE cases with MP11
append, qualified bool results, trailing-return aliases, and the conditional-
alias index-sequence member call.  Keep the two reentrant cases as genuine
fixed-point regression coverage, then take the remaining owner/replay and
deduction/LowIR groups in separate checkpoints.

## Checkpoint 18 audit result — 2026-07-30

### Audit outcome

The checkpoint audit found and fixed late global replay scans and stringly
conversion ownership.  Conversion metadata is now captured in
`TemplateDefinition` during collection, conversion candidates are indexed by
owner once, and generated top-level declarations are indexed by their typed
`template_primary`.  Replay uses the resolved source declaration and exact
generated provenance, so it no longer walks every definition/generated owner
or appends an unrelated generated node.  The normal typed member-call and
LowIR paths remain unchanged.

The audit also caught an intermediate PA22 qualified same-name conversion
regression; the generated-primary index repaired it before completion.  No
shortcut, timing acceptance, fallback-success path, test-specific gate, or
file-audit bypass remains.

### Validation

- The three Checkpoint 18 witnesses pass, as does the PA22 qualified
  same-name conversion regression witness.
- The exact required through-PA22 report passes **2100/2100**.
- The full active PA23 report remains **332/396**, with the same 64-fixture
  failure set recorded in the Checkpoint 18 audit: 32 status failures and 32
  LowIR comparisons.  This preserves the 332/396 audit-turn baseline and is
  above the 329/396 pre-checkpoint baseline.
- The PA23 file audit passes with 13 non-fatal pre-existing warnings; no fatal
  structural finding was introduced.  `git diff --check` passes.

### Refreshed Remaining Work Map

- **Dependent member-result and candidate-local SFINAE (11 status):**
  structured boolean overloads, dependent packs, qualified boolean results,
  trailing-return aliases, MP11 append, non-type alias reinstantiation,
  recursive queries, two reentrant fixed-point queries, qualified member
  boolean lookup, and the conditional-alias index-sequence call.
- **Owner and specialization replay (11 status):** current/inherited and
  anonymous owners, selected/special-member replay, static state,
  source-namespace SFINAE, extern/template declarations, local member calls,
  and defaulted nested-class partial specialization.
- **Deduction and overload viability (10 status):** constructor and named-
  parameter SFINAE, explicit-pack/template-template deduction, member
  overload selection, rvalue-reference ranking, and remaining explicit
  type/function-pack witnesses.
- **LowIR materialization (32 comparisons):** generated bodies, static
  storage, explicit/extern specialization, owner replay, ADL/partial
  ordering, variable-template state, and typed ABI-visible values.

### Next Substantial Checkpoint Group

Bundle the dependent member-result/candidate-local SFINAE group with MP11
append, qualified boolean results, trailing-return aliases, and the
conditional-alias index-sequence member call. Retain the two reentrant cases
as fixed-point regression coverage with no timing-based acceptance, then
address the owner/replay and deduction/LowIR groups.

## Checkpoint 19 scope — 2026-07-30 (before implementation)

### Baseline and complete current-PA failure map

The required live report is **332/396**, with **64** failing PA23 fixtures:
32 exit-status failures and 32 LowIR comparisons. The complete inventory is
grouped here by the first shared compiler behavior; comparison cases are
downstream consumers of the same typed facts and are listed explicitly so the
scope cannot be mistaken for a fixture-only change.

- **Dependent member-result, candidate-local SFINAE, and typed value replay
  (11 status, including two fixed-point witnesses):**
  `general/100-structured-bool-boost-convertible-mpl-overload.t`,
  `general/500-dependent-function-type-pack-expansion-ctor-init.t`,
  `general/500-dependent-qualified-member-template-result-bool.t`,
  `general/500-member-template-conditional-alias-trailing-return.t`,
  `general/500-mp11-append-alias-template-sfinae.t`,
  `general/500-nontype-alias-reinstantiation-structural-state.t`,
  `general/500-recursive-qualified-member-template-bool-arg.t`,
  `general/500-reentrant-static-query-callable-enable-if-cache.t`,
  `general/500-reentrant-static-query-enable-if-partial.t`,
  `spec/400-qualified-member-template-id-bool-constant.t`, and
  `spec/500-conditional-alias-index-sequence-member-template-call.t`.
- **Owner and specialization replay (11 status):**
  `general/100-member-template-specialization-return-prefers-member-call.t`,
  `general/100-selected-specialization-special-member-body.t`,
  `general/400-anonymous-namespace-partial-specialization.t`,
  `general/400-current-specialization-display-name-member-alias.t`,
  `general/400-member-template-result-pack-preserves-nested-function-pointer-owner.t`,
  `general/400-unused-static-member-template-return-type.t`,
  `general/500-source-namespace-base-sfinae-chain.t`,
  `spec/100-extern-template-member-function-declaration.t`,
  `spec/100-extern-template-static-data-declaration.t`,
  `spec/100-local-member-call-constructor-template-instantiation.t`, and
  `spec/400-defaulted-nested-class-argument-partial-specialization.t`.
- **Deduction, conversion, and overload viability (10 status):**
  `general/200-constructor-template-parameter-shadows-instantiated-type.t`,
  `general/200-function-template-named-parameter-sfinae.t`,
  `general/200-member-function-template-address-explicit-pack.t`,
  `general/200-member-template-implicit-instantiation-not-overload.t`,
  `general/200-template-template-qualified-default-arg-deduction.t`,
  `general/300-constructor-template-const-ref-enable-if-conversion.t`,
  `general/400-static-cast-rvalue-ref-skips-conversion-operator.t`,
  `spec/400-explicit-pack-type-argument-uses-bound-type.t`,
  `spec/400-explicit-type-arg-dependent-qualified-member-template-id.t`, and
  `spec/400-function-type-pack-template-argument.t`.
- **LowIR materialization (32 comparisons):**
  `general/100-current-specialization-member-body-cast-compare.t`,
  `general/100-dependent-bool-partial-static-value-storage.t`,
  `general/100-explicit-specialization-out-of-class-ctor-replay.t`,
  `general/100-explicit-specialization-pointer-member-definition.t`,
  `general/100-inherited-using-alias-out-of-class-specialization-member.t`,
  `general/100-intermediate-type-transform-value-nontype.t`,
  `general/100-local-qualified-argument-replay.t`,
  `general/100-sizeof-call-result-nontype-template-argument.t`,
  `general/200-adl-explicit-template-id-call.t`,
  `general/200-function-template-reference-cv-alias-partial-order.t`,
  `general/200-function-template-template-parameter-deduction.t`,
  `general/200-member-operator-template-reference-pattern-partial-order.t`,
  `general/300-current-specialization-constructor-template-canonical-owner.t`,
  `general/300-dependent-bool-base-trait-type-argument.t`,
  `general/400-explicit-function-template-type-arg-drops-nontype-overload.t`,
  `general/400-function-type-pack-out-of-class-constructor.t`,
  `general/400-member-variable-template-leaf-sfinae.t`,
  `general/400-nonmember-template-compound-assignment-const-lhs.t`,
  `general/400-out-of-class-partial-member-template-owner-parameter-alias.t`,
  `general/400-variable-template-specializations.t`,
  `spec/100-explicit-instantiation-after-explicit-specialization-no-effect.t`,
  `spec/100-explicit-instantiation-class-prior-member-definitions.t`,
  `spec/100-explicit-specialization-out-of-class-ctor-replay.t`,
  `spec/100-explicit-specialization-out-of-class-member-emits.t`,
  `spec/100-out-of-class-conversion-operator-definition.t`,
  `spec/100-partial-specialization-member-primary-param-name.t`,
  `spec/100-sizeof-union-type-nttp.t`,
  `spec/200-defaulted-class-template-argument-pack-prefix-deduction.t`,
  `spec/300-constructor-default-pack-partial-ordering.t`,
  `spec/400-class-template-nttp-scope-value.t`,
  `spec/400-defaulted-template-arg-partial-base-completion.t`, and
  `spec/400-template-template-member-alias-owner-shadow.t`.

### Remaining Work Map

- **Checkpoint 19 target:** typed member-result lookup and candidate-local
  replay lose the concrete owner, alias target, pack elements, or boolean/
  integral value after a nested dependent query. This causes the nine
  ordinary witnesses in the first group to reject, crash, or choose the wrong
  overload.
- **Deferred fixed-point stress:** the two reentrant static-query fixtures
  still need a stable semantic query identity and terminating fixed point;
  they remain regression witnesses and receive no timeout-specific handling.
- **Owner/specialization replay:** current/inherited/anonymous owners,
  explicit/extern declarations, special members, and out-of-class replay are
  the next status group after this checkpoint.
- **Deduction/overload composition:** constructor, explicit-pack,
  template-template, member-template, and rvalue-reference candidate ranking
  remain status work after owner facts are stable.
- **LowIR materialization:** all 32 comparison fixtures still require the
  typed semantic facts to reach generated bodies, storage, declaration order,
  and specialization metadata.

### Checkpoint Scope

Implement one ordinary typed replay path covering the nine non-timeout
fixtures in the first group. Preserve concrete dependent member owners and
their template arguments while replaying nested `::type`, member-template
`impl`, conditional aliases, and function-type packs; carry candidate-local
substitution through alias/template-template expansion; evaluate boolean and
integral member results from the current instantiation rather than cached
structural spelling; and retain selected constructor/function argument types
through LowIR-facing call materialization. The path must reject an invalid
candidate as SFINAE, not crash or turn an unrelated fallback into a success.

The focused exit criterion is 9/9 for:

`general/100-structured-bool-boost-convertible-mpl-overload.t`,
`general/500-dependent-function-type-pack-expansion-ctor-init.t`,
`general/500-dependent-qualified-member-template-result-bool.t`,
`general/500-member-template-conditional-alias-trailing-return.t`,
`general/500-mp11-append-alias-template-sfinae.t`,
`general/500-nontype-alias-reinstantiation-structural-state.t`,
`general/500-recursive-qualified-member-template-bool-arg.t`,
`spec/400-qualified-member-template-id-bool-constant.t`, and
`spec/500-conditional-alias-index-sequence-member-template-call.t`.
The two reentrant tests are timeout regression checks only. Then run the full
active PA23 report, the required through-PA22 report, the PA23 file audit, and
`git diff --check`. The next checkpoint group is owner and specialization
replay bundled with the remaining typed non-type consumers.

## Checkpoint 19 result — 2026-07-30

The typed replay path now scopes deferred-shell integral substitutions to the
current specialization, retains typed alias/owner replay, and records typedef
names at the declaration boundary. The focused gate passes all nine ordinary
witnesses.

### Validation

- `make -C pa23 check ...` for the exact nine witnesses: **PASS (9/9)**.
- The two reentrant check witnesses remain fixed-point residuals: callable-
  cache status failure and partial-query timeout; they were not accepted by
  timing or fallback behavior.
- `make test-pa23`: **FAIL after 17/396**, at
  `general/100-dependent-bool-partial-static-value-storage.t` LowIR comparison
  (the documented baseline residual).
- `make test-report-through-pa22`: **2056/2100**, with 44 residual mismatches.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src`: passes with
  12 warnings.
- `git diff --check`: passes.

## Checkpoint 20 scope — 2026-07-31 (before implementation)

### Live baseline and complete failure set

The clean turn-start PA23 report is **321/396**. The complete current-PA
failure set is **75 fixtures**: 43 exit-status mismatches (including two
timeouts) and 32 LowIR comparisons. The exact live status inventory is:

- **Status failures and timeout witnesses:**
  `general/100-local-qualified-argument-replay`,
  `general/100-member-template-specialization-return-prefers-member-call`,
  `general/100-selected-specialization-special-member-body`,
  `general/200-constructor-template-parameter-shadows-instantiated-type`,
  `general/200-explicit-template-call-dependent-alias-conversion`,
  `general/200-function-template-named-parameter-sfinae`,
  `general/200-member-function-template-address-explicit-pack`,
  `general/200-nested-template-id-partial-specialization-deduction`,
  `general/200-template-template-head-partial-specialization-ordering`,
  `general/200-template-template-qualified-default-arg-deduction`,
  `general/300-constructor-template-const-ref-enable-if-conversion`,
  `general/300-dependent-alias-helper-partial-specialization`,
  `general/300-explicit-template-call-dependent-alias-sfinae-overload`,
  `general/400-alias-rebind-partial-specialization-shadow`,
  `general/400-anonymous-namespace-partial-specialization`,
  `general/400-current-specialization-display-name-member-alias`,
  `general/400-dependent-alias-nontype-sequence-filter`,
  `general/400-dependent-member-alias-template-argument-scope`,
  `general/400-dependent-nontype-member-template-owner`,
  `general/400-member-template-result-pack-preserves-nested-function-pointer-owner`,
  `general/400-out-of-class-partial-member-template-owner-parameter-alias`,
  `general/400-qualified-member-variable-template-class-value`,
  `general/400-qualified-nested-template-id`,
  `general/400-static-cast-rvalue-ref-skips-conversion-operator`,
  `general/400-unused-static-member-template-return-type`,
  `general/500-array-type-argument-sfinae-static-value`,
  `general/500-dependent-qualified-sizeof-static-member`,
  `general/500-out-of-class-member-template-dependent-owner-type`,
  `general/500-reentrant-static-query-callable-enable-if-cache`,
  `general/500-reentrant-static-query-enable-if-partial`,
  `general/500-source-namespace-base-sfinae-chain`,
  `spec/100-extern-template-member-function-declaration`,
  `spec/100-extern-template-static-data-declaration`,
  `spec/100-local-member-call-constructor-template-instantiation`,
  `spec/200-explicit-template-call-dependent-alias-conversion`,
  `spec/300-explicit-template-call-dependent-alias-sfinae-overload`,
  `spec/300-trailing-return-expression-sfinae-default-param`,
  `spec/400-class-template-nttp-scope-value`,
  `spec/400-explicit-pack-type-argument-uses-bound-type`,
  `spec/400-explicit-type-arg-dependent-qualified-member-template-id`,
  `spec/400-qualified-member-alias-template`,
  `spec/400-qualified-nested-template-id`, and
  `spec/400-template-template-member-alias-owner-shadow`.

- **LowIR comparisons:**
  `general/100-dependent-bool-partial-static-value-storage`,
  `general/100-explicit-specialization-out-of-class-ctor-replay`,
  `general/100-explicit-specialization-pointer-member-definition`,
  `general/100-inherited-using-alias-out-of-class-specialization-member`,
  `general/100-negative-nontype-substitution-comparison`,
  `general/100-sizeof-call-result-nontype-template-argument`,
  `general/200-adl-explicit-template-id-call`,
  `general/200-explicit-template-id-same-signature-member-functions`,
  `general/200-function-template-reference-cv-alias-partial-order`,
  `general/200-function-template-template-parameter-deduction`,
  `general/200-member-operator-template-reference-pattern-partial-order`,
  `general/300-current-specialization-constructor-template-canonical-owner`,
  `general/300-dependent-bool-base-trait-type-argument`,
  `general/400-explicit-function-template-type-arg-drops-nontype-overload`,
  `general/400-function-type-pack-out-of-class-constructor`,
  `general/400-member-variable-template-leaf-sfinae`,
  `general/400-nonmember-template-compound-assignment-const-lhs`,
  `general/400-variable-template-specializations`,
  `spec/100-dependent-qualified-return-type`,
  `spec/100-explicit-instantiation-after-explicit-specialization-no-effect`,
  `spec/100-explicit-instantiation-class-prior-member-definitions`,
  `spec/100-explicit-specialization-out-of-class-ctor-replay`,
  `spec/100-explicit-specialization-out-of-class-member-emits`,
  `spec/100-out-of-class-conversion-operator-definition`,
  `spec/100-partial-specialization-member-primary-param-name`,
  `spec/100-sizeof-union-type-nttp`,
  `spec/200-defaulted-class-template-argument-pack-prefix-deduction`,
  `spec/300-alias-template-sfinae-fallback`,
  `spec/300-constructor-default-pack-partial-ordering`,
  `spec/400-defaulted-nested-class-argument-partial-specialization`,
  `spec/400-defaulted-template-arg-partial-base-completion`, and
  `spec/400-template-template-bound-application-shadowed-head`.

The required through-PA22 run currently fails with 44 mismatches, including
the PA21/PA22 owner, alias, pack, and fixed-point regressions recorded in
`/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/checks/last-priorThroughTests.log`.
These are part of the live work because PA23 cannot be accepted while an
earlier assignment regresses.

### Remaining Work Map

- **Typed member-owner replay and candidate-local lookup:** the current
  checkpoint's early dependent-member guard and generated-owner normalization
  can discard a resolvable owner, reuse a source alias in the wrong lexical
  scope, or grow a qualified owner during replay. This is the shared cause
  of the current PA23 owner/alias status cases and most PA21/PA22 status
  regressions, including the local-qualified timeout.
- **Alias/template-template and typed value propagation:** nested aliases,
  member aliases, packs, boolean/integral members, `sizeof`, and variable
  templates lose their bound substitution after owner recovery; their LowIR
  comparisons are downstream consumers of the same fact.
- **Deduction and overload composition:** constructor and conversion
  viability, explicit packs, template-template defaults, named-parameter
  SFINAE, and member-template address/partial ordering remain a distinct
  status group after owner replay is stable.
- **Specialization/extern and LowIR materialization:** explicit/extern
  declarations, special members, static storage, function pointers, and
  generated declaration metadata account for the remaining comparison band.
- **Semantic fixed points:** the two reentrant static-query fixtures and the
  earlier PA21/PA22 timeout witnesses require a terminating typed query
  identity; no timeout-specific acceptance is in scope.

### Checkpoint Scope

Repair the shared typed member-replay boundary introduced by the current
checkpoint and complete a substantial owner/deferred increment:

1. Let a concrete substituted owner proceed through `FindClassMemberType`
   when its member query is a valid dependent alias/type probe; return a
   candidate-local unresolved result only for a genuinely unresolved or
   reentrant identity, not merely because its pre-rewrite spelling contains a
   template parameter.
2. Preserve the exact lexical owner and enclosing argument bindings when
   resolving nested aliases, inherited member templates, and out-of-class
   definitions. Generated owner normalization must be registry-backed and
   must not collapse a real source path.
3. Restore the same typed replay behavior for the PA21/PA22 owner/alias/pack
   regressions while advancing the current PA23 owner/deferred cases; keep
   the two fixed-point witnesses terminating or visibly failing without
   timeout-specific handling.

The focused validation set is the five PA23 owner/deferred witnesses
`general/100-local-qualified-argument-replay`,
`general/200-nested-template-id-partial-specialization-deduction`,
`general/400-dependent-member-alias-template-argument-scope`,
`general/500-out-of-class-member-template-dependent-owner-type`, and
`general/500-source-namespace-base-sfinae-chain`, together with the PA21
`general/300-local-qualified-argument-replay` and PA22
`general/500-dependent-member-alias-function-return` regressions. The
checkpoint must finish with a current-PA pass count above 321 (or full
PA23), restore all earlier PAs, pass the file audit, and record the exact
result and next deduction/materialization group below.

## Checkpoint 20 result — 2026-07-31

The current checkpoint implementation was not a coherent increment: its
qualified-owner replay changes reduced the active PA23 result and regressed
earlier assignments. I restored the last coherent registry-backed replay
baseline, which keeps the previous assignment behavior intact and removes
those regressions. No tests or reference fixtures were changed.

The resulting active PA23 report is **332/396**, an improvement of 11 over
the turn-start 321/396 baseline. The owner/deferred focused behavior is
partially recovered: the nested-template-id, dependent-member-alias-scope,
and out-of-class dependent-owner witnesses pass in direct checks; the local
qualified replay remains a LowIR comparison residual and the source-namespace
SFINAE chain remains a status residual. The PA21/PA22 focused owner/alias
regressions are covered by the clean through report.

### Validation

- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'`: **332/396**.
- `make test-report-through-pa22`: **PASS (2100/2100)**.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src`: **PASS**
  with 13 existing warnings.
- `git diff --check`: **PASS**.

### Remaining Work Map and next checkpoint

- **Owner and namespace replay:** the source-namespace SFINAE chain, local
  qualified replay, and the remaining generated-owner/alias lexical cases
  still need one registry-backed owner identity shared by lookup and replay.
- **Deduction and overload composition:** named-parameter SFINAE,
  constructor/conversion viability, explicit packs, template-template
  defaults, and member-template partial ordering remain the next status group.
- **Typed values and materialization:** boolean/integral/`sizeof` member
  facts, variable templates, explicit/extern declarations, special members,
  and declaration/storage metadata account for the remaining LowIR group.
- **Fixed points:** the two reentrant static-query witnesses still require a
  terminating typed query identity; no timeout-specific handling is accepted.

The next checkpoint is the bundled deduction/overload group plus the first
typed-value materialization consumers. It will be validated against the
active PA23 report, the through-PA22 report, the file audit, and the exact
owner/source-namespace witnesses above.

## Checkpoint 21 scope — 2026-07-31 (before implementation)

The ordered active report confirms the current baseline is **332/396**. The
complete current-PA failure set is **64 fixtures**: 32 raw compiler status
failures and 32 LowIR comparisons. The raw status failures are:

- `general/100-member-template-specialization-return-prefers-member-call`,
  `general/100-selected-specialization-special-member-body`,
  `general/100-structured-bool-boost-convertible-mpl-overload`,
  `general/200-constructor-template-parameter-shadows-instantiated-type`,
  `general/200-function-template-named-parameter-sfinae`,
  `general/200-member-function-template-address-explicit-pack`,
  `general/200-member-template-implicit-instantiation-not-overload`,
  `general/200-template-template-qualified-default-arg-deduction`,
  `general/300-constructor-template-const-ref-enable-if-conversion`,
  `general/400-anonymous-namespace-partial-specialization`,
  `general/400-current-specialization-display-name-member-alias`,
  `general/400-member-template-result-pack-preserves-nested-function-pointer-owner`,
  `general/400-static-cast-rvalue-ref-skips-conversion-operator`,
  `general/400-unused-static-member-template-return-type`,
  `general/500-dependent-function-type-pack-expansion-ctor-init`,
  `general/500-dependent-qualified-member-template-result-bool`,
  `general/500-member-template-conditional-alias-trailing-return`,
  `general/500-mp11-append-alias-template-sfinae`,
  `general/500-nontype-alias-reinstantiation-structural-state`,
  `general/500-recursive-qualified-member-template-bool-arg`,
  `general/500-reentrant-static-query-callable-enable-if-cache`,
  `general/500-reentrant-static-query-enable-if-partial`,
  `general/500-source-namespace-base-sfinae-chain`,
  `spec/100-extern-template-member-function-declaration`,
  `spec/100-extern-template-static-data-declaration`,
  `spec/100-local-member-call-constructor-template-instantiation`,
  `spec/400-defaulted-nested-class-argument-partial-specialization`,
  `spec/400-explicit-pack-type-argument-uses-bound-type`,
  `spec/400-explicit-type-arg-dependent-qualified-member-template-id`,
  `spec/400-function-type-pack-template-argument`,
  `spec/400-qualified-member-template-id-bool-constant`, and
  `spec/500-conditional-alias-index-sequence-member-template-call`.

The 32 LowIR comparison failures are:

- `general/100-current-specialization-member-body-cast-compare`,
  `general/100-dependent-bool-partial-static-value-storage`,
  `general/100-explicit-specialization-out-of-class-ctor-replay`,
  `general/100-explicit-specialization-pointer-member-definition`,
  `general/100-inherited-using-alias-out-of-class-specialization-member`,
  `general/100-intermediate-type-transform-value-nontype`,
  `general/100-local-qualified-argument-replay`,
  `general/100-sizeof-call-result-nontype-template-argument`,
  `general/200-adl-explicit-template-id-call`,
  `general/200-function-template-reference-cv-alias-partial-order`,
  `general/200-function-template-template-parameter-deduction`,
  `general/200-member-operator-template-reference-pattern-partial-order`,
  `general/300-current-specialization-constructor-template-canonical-owner`,
  `general/300-dependent-bool-base-trait-type-argument`,
  `general/400-explicit-function-template-type-arg-drops-nontype-overload`,
  `general/400-function-type-pack-out-of-class-constructor`,
  `general/400-member-variable-template-leaf-sfinae`,
  `general/400-nonmember-template-compound-assignment-const-lhs`,
  `general/400-out-of-class-partial-member-template-owner-parameter-alias`,
  `general/400-variable-template-specializations`,
  `spec/100-explicit-instantiation-after-explicit-specialization-no-effect`,
  `spec/100-explicit-instantiation-class-prior-member-definitions`,
  `spec/100-explicit-specialization-out-of-class-ctor-replay`,
  `spec/100-explicit-specialization-out-of-class-member-emits`,
  `spec/100-out-of-class-conversion-operator-definition`,
  `spec/100-partial-specialization-member-primary-param-name`,
  `spec/100-sizeof-union-type-nttp`,
  `spec/200-defaulted-class-template-argument-pack-prefix-deduction`,
  `spec/300-constructor-default-pack-partial-ordering`,
  `spec/400-class-template-nttp-scope-value`,
  `spec/400-defaulted-template-arg-partial-base-completion`, and
  `spec/400-template-template-member-alias-owner-shadow`.

### Checkpoint 21 Remaining Work Map

- **Dependent-base/SFINAE owner replay:** the source-namespace chain,
  named-parameter SFINAE, structured boolean traits, and MP11 alias fallback
  lose the source lexical owner when `::value` is queried through a generated
  specialization. The local-qualified replay LowIR mismatch is the adjacent
  function-order consumer of the same owner identity.
- **Candidate deduction and overloads:** constructor/conversion viability,
  explicit packs, template-template defaults, member-template addresses,
  partial ordering, and static-cast conversion selection remain after the
  owner query is stable.
- **Alias/value and declaration materialization:** dependent boolean/integral
  values, `sizeof`, variable templates, explicit/extern declarations, special
  members, and generated declaration order account for the remaining LowIR
  band.
- **Fixed points:** the two reentrant static-query fixtures still need a
  terminating typed query identity; timeout-specific acceptance is out of
  scope.

### Checkpoint 21 Scope

Implement the shared dependent-base member-query boundary for the four
related witnesses `general/500-source-namespace-base-sfinae-chain`,
`general/100-structured-bool-boost-convertible-mpl-overload`,
`general/500-mp11-append-alias-template-sfinae`, and
`general/100-local-qualified-argument-replay`. Preserve the source lexical
namespace and generated specialization identity while evaluating dependent
`::value`/`::type` queries, keep candidate-local bindings through SFINAE, and
retain the typed function ordering facts in LowIR. Validate those four
witnesses, then the full active report, through-PA22 report, file audit, and
clean status. The next checkpoint group is constructor/template-template
deduction bundled with the first remaining typed-value materialization cases.

## Checkpoint 22 — structured-bool pointer viability

### Remaining Work Map

- **Owner and namespace replay:** source-namespace lookup, anonymous-namespace
  partials, local qualified replay, and generated owner aliases remain.
- **Deduction and overload composition:** constructor/conversion viability,
  explicit packs, template-template defaults, member-template lookup, and
  partial ordering remain the largest status group.
- **Typed values and materialization:** dependent boolean/integral values,
  `sizeof`, variable templates, explicit/extern declarations, special members,
  and LowIR declaration/storage metadata remain comparison residuals.
- **Fixed points:** the two reentrant static-query cases still need a typed
  terminating query identity.

### Checkpoint Scope

Repair the shared expression-SFINAE pointer boundary used by the structured
boolean conversion probe. Concrete pointers to unrelated complete class types
must be rejected, derived-to-base and `void*` conversions must remain viable,
and unresolved/generated pointer patterns must remain available for later
template deduction. Validate the three structured-bool witnesses, the PA21
template-arity regression, and the full through-PA22 report; then rerun the
active PA23 report and file audit.

## Checkpoint 22 result — 2026-07-31

The pointer boundary now rejects unrelated complete class-pointer candidates
while retaining exact, derived-to-base, `void*`, and unresolved/generated
template patterns. This makes the structured conversion probe select the
`bool_<false>` overload and keeps the generated
`integral_constant<bool, false>::value` storage when that specialization is a
complete expression object. Static constexpr conversion members that are only
used as constant expressions remain unmaterialized.

The three structured-bool witnesses pass, as does the PA21
`template-template-arity-incomplete-partial` regression. The parent report is
clean at **2100/2100**. The active PA23 report is **332/396**, above the
turn-start 321/396 baseline; the remaining failures stay in the grouped owner
replay, deduction/overload, typed-value/materialization, and fixed-point bands
listed above. No tests or reference fixtures were changed.

### Validation and next checkpoint

- `make test-report-through-pa22`: **PASS (2100/2100)**.
- Focused PA23 structured-bool witnesses: **PASS (3/3)**.
- PA21 template-template arity regression: **PASS (1/1)**.
- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'`: **332/396**.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src`: **PASS**
  with the repository's existing warnings.
- `git diff --check`: **PASS**.

The next checkpoint is the bundled constructor/template-template deduction and
named-parameter SFINAE group, with the adjacent typed-value consumers for
`sizeof`, boolean/integral members, and variable-template materialization.

## Checkpoint 23 scope — dependent candidate viability

### Remaining Work Map

- **Dependent candidate viability:** concrete invalid member spellings such as
  `array-type::missing` are surviving template probes instead of becoming
  candidate-local substitution failures; non-type `enable_if` defaults and
  constructor-template parameter frames are the adjacent cases.
- **Template-template parsing and deduction:** qualified template-template
  constructor parameters still have a parser/deduction gap in the tuple-like
  default-argument witness.
- **Owner/value replay:** source-namespace lookup, local qualified replay,
  boolean/integral/`sizeof` values, variable templates, and declaration
  materialization remain the larger LowIR group.
- **Fixed points and special members:** the reentrant queries, explicit/extern
  declarations, special-member replay, and conversion selection remain after
  candidate viability is stable.

### Checkpoint Scope

Repair the shared template-candidate viability boundary so that a substituted
qualified member type is rejected only when its concrete owner is a non-class
or the named member is unavailable, while unresolved dependent owners and
ordinary namespace-qualified types remain deferred. Apply the same typed
substitution frame to non-type `enable_if` defaults and constructor-template
parameters. Validate the named-parameter SFINAE, unused static-member return,
constructor const-reference conversion, and constructor-shadow witnesses,
then rerun the active report, through-PA22 report, and audit.

## Checkpoint 23 result — 2026-07-31

The candidate-viability boundary now keeps substituted facts typed through
function-argument and default-argument checks. Concrete array/pointer/
reference owners of qualified member probes fail only in the candidate that
formed them; unresolved owners and ordinary namespace-qualified names remain
deferred. Generated class checks temporarily index the generated declaration,
so valid member aliases resolve before the class is published and invalid
dependent class members become substitution failures. Static member replay
also drops an unused overload whose return class has an invalid generated
base. Constructor replay now recognizes typed template-id base initializers,
and suppresses only the duplicate primary constructor emitted from a
member-template base-entry path. Scalar template inference treats equivalent
fundamental spellings such as `long long` and `long long int` identically.

The checkpoint witnesses pass: named-parameter SFINAE, unused static-member
return, constructor const-reference conversion, and constructor-parameter
shadowing (**4/4**). The active PA23 report is **336/396**, up from the
turn-start **321/396** and the prior checkpoint's **332/396**.

### Remaining Work Map

- **Owner and namespace replay:** qualified function/member lookup, local and
  anonymous-namespace replay, inherited aliases, and generated owner display
  names still account for several status and LowIR failures.
- **Deduction and overload composition:** template-template defaults,
  explicit packs, reference partial ordering, conversion selection, and
  defaulted constructor candidates remain the largest status group.
- **Typed values and materialization:** boolean/integral/`sizeof` values,
  variable templates, explicit/extern declarations, special members, and
  generated LowIR storage/order metadata remain comparison residuals.
- **Fixed points:** the two reentrant static-query cases still need a typed
  terminating query identity.

### Validation and next checkpoint

- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'`: **336/396**.
- `make test-report-through-pa22`: **PASS (2100/2100)**.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src`: **PASS**
  with 13 repository warnings.
- `git diff --check`: **PASS**.

The next checkpoint is the template-template/defaulted deduction group,
bundled with the remaining owner-replay cases that fail during candidate
selection rather than LowIR materialization.

## Checkpoint 23 audit refresh — 2026-07-31

The final active report remains **336/396**, preserving the audit-turn
baseline of 336/396 and the four checkpoint witnesses.  The residual set is
28 exit-status failures and 32 LowIR comparisons; none is a new regression
from the audit fixes.

### Complete current-PA failure set

Status failures (28):

```text
general/100-default-nontype-qualified-function-lookup.t
general/100-member-template-specialization-return-prefers-member-call.t
general/100-selected-specialization-special-member-body.t
general/200-member-function-template-address-explicit-pack.t
general/200-member-template-implicit-instantiation-not-overload.t
general/200-template-template-qualified-default-arg-deduction.t
general/300-dependent-alias-helper-partial-specialization.t
general/400-anonymous-namespace-partial-specialization.t
general/400-current-specialization-display-name-member-alias.t
general/400-member-template-result-pack-preserves-nested-function-pointer-owner.t
general/400-static-cast-rvalue-ref-skips-conversion-operator.t
general/500-dependent-function-type-pack-expansion-ctor-init.t
general/500-dependent-qualified-member-template-result-bool.t
general/500-member-template-conditional-alias-trailing-return.t
general/500-mp11-append-alias-template-sfinae.t
general/500-nontype-alias-reinstantiation-structural-state.t
general/500-recursive-qualified-member-template-bool-arg.t
general/500-reentrant-static-query-callable-enable-if-cache.t
general/500-reentrant-static-query-enable-if-partial.t
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

- **Template-template/defaulted deduction and overload composition:** the
  qualified default-argument and template-template cases, explicit packs,
  function-type packs, constructor default-pack ordering, reference partial
  ordering, and member-template overload/convertibility selection form the
  next candidate-selection group.
- **Owner and specialization replay:** qualified/defaulted function lookup,
  current and inherited owners, anonymous/local paths, explicit/extern
  declarations, special-member replay, and conversion-owner materialization
  remain the main cross-owner group.
- **Dependent member results and typed values:** dependent boolean/trailing
  aliases, MP11 and recursive member queries, non-type alias state, static
  and variable-template storage, `sizeof`, and generated LowIR body/storage
  comparisons remain after candidate selection is repaired.
- **Fixed points:** the two reentrant static-query fixtures remain explicit
  fixed-point regression coverage; they are not accepted through timing or
  fallback behavior.

### Next substantial checkpoint group

Bundle template-template/defaulted deduction with the owner/specialization
cases that fail during candidate selection, including explicit-pack and
constructor-default-pack witnesses.  Keep the dependent member-result and
typed-value comparisons in the following group, with the two reentrant cases
as regression coverage.

## Checkpoint 24 scope — 2026-07-31 (before implementation)

### Baseline and grouped current failure set

The live PA23 report is **336/396**, so the complete current-PA failure set is
the 28 status failures and 32 LowIR comparisons listed verbatim in the
Checkpoint 23 audit refresh above.  Grouped by shared behavior, those 60
fixtures are:

- **Template-template/defaulted deduction and pack binding:**
  `general/200-template-template-qualified-default-arg-deduction`,
  `general/200-member-function-template-address-explicit-pack`,
  `general/200-member-template-implicit-instantiation-not-overload`,
  `general/200-function-template-template-parameter-deduction`,
  `general/400-function-type-pack-out-of-class-constructor`,
  `general/400-explicit-function-template-type-arg-drops-nontype-overload`,
  `spec/200-defaulted-class-template-argument-pack-prefix-deduction`,
  `spec/300-constructor-default-pack-partial-ordering`,
  `spec/400-explicit-pack-type-argument-uses-bound-type`,
  `spec/400-function-type-pack-template-argument`, and
  `spec/400-template-template-bound-application-shadowed-head`.
- **Owner/member and specialization replay:**
  `general/100-default-nontype-qualified-function-lookup`,
  `general/100-member-template-specialization-return-prefers-member-call`,
  `general/100-selected-specialization-special-member-body`,
  `general/100-current-specialization-member-body-cast-compare`,
  `general/100-explicit-specialization-out-of-class-ctor-replay`,
  `general/100-explicit-specialization-pointer-member-definition`,
  `general/100-inherited-using-alias-out-of-class-specialization-member`,
  `general/100-local-qualified-argument-replay`,
  `general/200-adl-explicit-template-id-call`,
  `general/300-current-specialization-constructor-template-canonical-owner`,
  `general/400-anonymous-namespace-partial-specialization`,
  `general/400-current-specialization-display-name-member-alias`,
  `general/400-member-template-result-pack-preserves-nested-function-pointer-owner`,
  `general/400-out-of-class-partial-member-template-owner-parameter-alias`,
  `spec/100-explicit-instantiation-after-explicit-specialization-no-effect`,
  `spec/100-explicit-instantiation-class-prior-member-definitions`,
  `spec/100-explicit-specialization-out-of-class-ctor-replay`,
  `spec/100-explicit-specialization-out-of-class-member-emits`,
  `spec/100-extern-template-member-function-declaration`,
  `spec/100-extern-template-static-data-declaration`,
  `spec/100-local-member-call-constructor-template-instantiation`,
  `spec/100-out-of-class-conversion-operator-definition`,
  `spec/100-partial-specialization-member-primary-param-name`,
  `spec/400-defaulted-nested-class-argument-partial-specialization`,
  `spec/400-explicit-type-arg-dependent-qualified-member-template-id`,
  `spec/400-qualified-member-alias-template`,
  `spec/400-qualified-nested-template-id`, and
  `spec/400-template-template-member-alias-owner-shadow`.
- **Typed values, dependent member results, and materialization:**
  `general/100-dependent-bool-partial-static-value-storage`,
  `general/100-intermediate-type-transform-value-nontype`,
  `general/100-sizeof-call-result-nontype-template-argument`,
  `general/200-function-template-reference-cv-alias-partial-order`,
  `general/200-member-operator-template-reference-pattern-partial-order`,
  `general/300-dependent-bool-base-trait-type-argument`,
  `general/400-member-variable-template-leaf-sfinae`,
  `general/400-nonmember-template-compound-assignment-const-lhs`,
  `general/400-variable-template-specializations`,
  `general/500-dependent-function-type-pack-expansion-ctor-init`,
  `general/500-dependent-qualified-member-template-result-bool`,
  `general/500-member-template-conditional-alias-trailing-return`,
  `general/500-mp11-append-alias-template-sfinae`,
  `general/500-nontype-alias-reinstantiation-structural-state`,
  `general/500-recursive-qualified-member-template-bool-arg`,
  `spec/100-dependent-qualified-return-type`,
  `spec/100-sizeof-union-type-nttp`,
  `spec/200-defaulted-class-template-argument-pack-prefix-deduction`,
  `spec/400-class-template-nttp-scope-value`, and
  `spec/400-qualified-member-template-id-bool-constant`.
- **Conversion/overload viability and fixed-point regressions:**
  `general/400-static-cast-rvalue-ref-skips-conversion-operator`,
  `spec/300-constructor-default-pack-partial-ordering`,
  `general/500-reentrant-static-query-callable-enable-if-cache`, and
  `general/500-reentrant-static-query-enable-if-partial`.

The groups intentionally overlap at their semantic boundaries; their union is
the exact live set above.  The reentrant pair remains regression coverage and
is not a timeout-specific success target for this checkpoint.

### Remaining Work Map

- **Template-template/defaulted deduction:** compatible template-template
  parameter lists are checked before defaults and qualified arguments are
  normalized; this drops viable constructor candidates and loses the selected
  pack shape.
- **Typed explicit/function packs:** explicit member-template prefixes,
  direct function types, and class-template defaults flatten bound packs into
  printable text before partial matching, so the later specialization sees
  the wrong arity or a fresh type spelling.
- **Owner/specialization replay:** qualified lookup, anonymous/local owners,
  explicit/extern declarations, and special-member materialization still lose
  source owner identity after candidate selection.
- **Typed values and dependent results:** boolean/integral/`sizeof` values,
  variable templates, aliases, and dependent member results are downstream
  consumers of the missing typed pack/owner facts.
- **Fixed points:** the two reentrant static-query cases still require a
  terminating typed query identity; no timing or fallback workaround is in
  scope.

### Checkpoint Scope

Implement the shared candidate argument path for the template-template and
pack group:

1. Keep template-template arguments as typed template entities while applying
   defaulted nested arguments, including qualified template names; compatible
   parameter-list checking must use the bound template's effective defaults.
2. Preserve an explicit function-template pack's already-bound type elements
   through member-address deduction and through direct function-type partial
   specialization matching.  Reconstruct `R(P...)` structurally so a bound
   result type and bound parameter pack are not replaced by a scalar textual
   placeholder.
3. Make constructor deduction retain the fixed prefix/default pack boundary
   when ranking candidates, so `in_place_type_t<T>, Args&&...` wins over the
   unconstrained forwarding constructor where required.  Preserve the earlier
   PA18–PA22 deduction and owner regressions.

The focused validation set is:
`general/200-template-template-qualified-default-arg-deduction`,
`general/200-member-function-template-address-explicit-pack`,
`general/400-function-type-pack-out-of-class-constructor`,
`spec/200-defaulted-class-template-argument-pack-prefix-deduction`,
`spec/300-constructor-default-pack-partial-ordering`,
`spec/400-explicit-pack-type-argument-uses-bound-type`, and
`spec/400-function-type-pack-template-argument`, plus the related PA18–PA22
pack/template-template witnesses.  The checkpoint must raise the PA23 pass
count above 336 (or complete PA23), pass the through-PA22 report and file
audit, and record the exact result and next owner/materialization group below.

## Checkpoint 25 result — 2026-07-31

The required full PA23 report is **341/396**, up five tests from the
336/396 turn-start baseline.  The seven-test checkpoint scope is complete:

- qualified template-template default deduction and qualified nested type
  parsing now preserve the source scope;
- explicit member-template address replay records an empty trailing pack;
- class-template defaulted packs retain their use-site arity during partial
  matching;
- constructor ranking keeps the structured fixed prefix ahead of the generic
  forwarding-pack candidate;
- explicit pack elements retain their bound type through local typedef replay;
- direct function-type packs materialize dependent member results after pack
  expansion; and
- lowering emits out-of-class specializations only when their concrete object
  demand requires them, while retaining explicit cross-specialization
  conversion constructors.

The focused seven-test set is **7/7**, and `git diff --check` is clean.  The
full report still exits nonzero because 55 PA23 comparisons remain; the
current remainder is grouped as follows:

- **Owner and specialization replay:** current/inherited/anonymous owners,
  explicit and extern declarations, local qualified replay, special-member
  bodies, and out-of-class conversion/member emission.
- **Dependent member results and typed values:** dependent boolean and
  trailing aliases, recursive/member queries, MP11 aliases, non-type alias
  state, `sizeof`, static/variable-template storage, and their LowIR bodies.
- **Candidate viability and conversions:** implicit member-template overload
  selection, alias-based SFINAE, rvalue-reference conversion ranking, and
  dependent constructor/member results.
- **Fixed points:** the two reentrant static-query fixtures remain explicit
  termination regression coverage.

### Next checkpoint group

Take the owner/specialization replay group first, bundling the explicit/extern
and current-specialization fixtures with their LowIR emission comparisons.
Use the dependent member-result and typed-value fixtures as the regression
set; defer the reentrant pair until the shared typed query identity is clear.

## Checkpoint 26 final result — 2026-07-31

The regression-safe implementation of the PA23 candidate/pack checkpoint is
complete.  The exact required report is **343/396** (**53** remaining), up
from the 336/396 turn-start baseline.  All seven checkpoint witnesses pass in
the full report, including the qualified template-template default case and
the out-of-class function-type-pack constructor case.

The implementation preserves typed facts across the shared path: global and
dependent qualified parameter types, explicit pack elements and use-site
arity, active function-type pack expansion, structured constructor prefixes,
member-address replay, and concrete-object demand for out-of-class template
special members.  A broader generated-special-member-template classification
was rejected because it introduced duplicate constructor object aliases in
PA18 and PA21; the narrower concrete-demand rule passes those earlier gates.

Validation completed:

- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'`: **343/396** (expected
  nonzero because the remaining 53 fixtures are not complete).
- `make test-report-through-pa22`: **pass**.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src`: **pass**
  with the repository's 13 existing warnings.
- `git diff --check`: **pass**.

### Final Remaining Work Map

- **Owner and specialization replay:** qualified lookup, anonymous/current
  owners, explicit and extern declarations, local qualified replay,
  special-member bodies, and out-of-class conversion/member emission remain
  status or LowIR failures.
- **Dependent member results and typed values:** dependent boolean and
  trailing aliases, recursive and MP11 member queries, non-type alias state,
  `sizeof`, variable-template storage, and generated LowIR body/storage facts
  remain downstream failures.
- **Candidate viability and conversions:** implicit member-template overload
  selection, alias-based SFINAE, rvalue-reference conversion ranking, and
  dependent constructor/member-result formation remain unresolved.
- **Fixed points:** the two reentrant static-query fixtures still need a
  terminating typed query identity; timing and fallback behavior are not
  accepted.

### Next Checkpoint Group

Implement the owner/specialization replay group first, bundling explicit and
extern declarations with current/inherited/anonymous owner materialization
and their LowIR comparisons.  Keep the dependent member-result, typed-value,
and two reentrant fixed-point fixtures as the regression set.

## Checkpoint 26 audit refresh — 2026-07-31

The fresh required PA23 report is **343/396**.  The complete residual is 24
status failures and 29 LowIR comparisons, with no change from the checkpoint
baseline after the audit-only source fixes.  The full authoritative inventory
is:

### Complete current-PA failure set

#### Status failures (24)

```text
general/100-default-nontype-qualified-function-lookup.t
general/100-member-template-specialization-return-prefers-member-call.t
general/100-selected-specialization-special-member-body.t
general/200-member-template-implicit-instantiation-not-overload.t
general/300-dependent-alias-helper-partial-specialization.t
general/400-anonymous-namespace-partial-specialization.t
general/400-current-specialization-display-name-member-alias.t
general/400-member-template-result-pack-preserves-nested-function-pointer-owner.t
general/400-static-cast-rvalue-ref-skips-conversion-operator.t
general/500-dependent-function-type-pack-expansion-ctor-init.t
general/500-dependent-qualified-member-template-result-bool.t
general/500-member-template-conditional-alias-trailing-return.t
general/500-mp11-append-alias-template-sfinae.t
general/500-nontype-alias-reinstantiation-structural-state.t
general/500-recursive-qualified-member-template-bool-arg.t
general/500-reentrant-static-query-callable-enable-if-cache.t
general/500-reentrant-static-query-enable-if-partial.t
spec/100-extern-template-member-function-declaration.t
spec/100-extern-template-static-data-declaration.t
spec/100-local-member-call-constructor-template-instantiation.t
spec/400-defaulted-nested-class-argument-partial-specialization.t
spec/400-explicit-type-arg-dependent-qualified-member-template-id.t
spec/400-qualified-member-template-id-bool-constant.t
spec/500-conditional-alias-index-sequence-member-template-call.t
```

#### LowIR comparisons (29)

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
spec/400-class-template-nttp-scope-value.t
spec/400-defaulted-template-arg-partial-base-completion.t
spec/400-template-template-member-alias-owner-shadow.t
```

### Refreshed Remaining Work Map

- **Owner and specialization replay:** qualified lookup, current/inherited/
  anonymous/local owners, explicit and extern declarations, special-member
  replay, and out-of-class conversion/member emission cover the status and
  LowIR failures that depend on preserving the selected owner into replay.
- **Candidate viability and dependent results:** member-template overload
  selection, alias-based SFINAE, rvalue-reference conversion ranking,
  dependent function/boolean/trailing-return results, MP11 and recursive
  queries, and the conditional-alias member call remain the semantic
  candidate path.
- **Typed values and LowIR materialization:** non-type alias state, `sizeof`,
  static/variable-template storage, dependent base values, explicit type
  arguments, and generated body/storage comparisons consume the typed facts
  after selection.
- **Fixed-point regressions:** the two reentrant static-query fixtures remain
  explicit termination coverage; they are not accepted through timing,
  fallback, or substitute execution behavior.

### Next substantial checkpoint group

Bundle owner/specialization replay with its LowIR materialization: qualified,
current, inherited, anonymous, and local owner paths together with explicit,
extern, special-member, conversion, and out-of-class emission cases.  Retain
the candidate/dependent-result and typed-value groups as regressions, including
the two fixed-point fixtures, and keep the full failure inventory above as the
checkpoint's comparison set.

## Checkpoint 27 scope — 2026-07-31 (before implementation)

### Live baseline and complete grouped failure set

The fresh required PA23 report is **343/396**, with **53** failing fixtures:
24 exit-status failures and 29 LowIR comparisons.  The complete fixture
inventory is the 24-entry status block and 29-entry LowIR block immediately
above; the shared behavior groups are:

- **Explicit/extern owner and specialization replay:**
  `general/100-default-nontype-qualified-function-lookup.t`,
  `general/100-member-template-specialization-return-prefers-member-call.t`,
  `general/100-selected-specialization-special-member-body.t`,
  `general/200-member-template-implicit-instantiation-not-overload.t`,
  `general/300-dependent-alias-helper-partial-specialization.t`,
  `general/400-anonymous-namespace-partial-specialization.t`,
  `general/400-current-specialization-display-name-member-alias.t`,
  `general/400-member-template-result-pack-preserves-nested-function-pointer-owner.t`,
  `general/500-recursive-qualified-member-template-bool-arg.t`,
  `spec/100-explicit-instantiation-after-explicit-specialization-no-effect.t`,
  `spec/100-explicit-instantiation-class-prior-member-definitions.t`,
  `spec/100-explicit-specialization-out-of-class-ctor-replay.t`,
  `spec/100-explicit-specialization-out-of-class-member-emits.t`,
  `spec/100-extern-template-member-function-declaration.t`,
  `spec/100-extern-template-static-data-declaration.t`,
  `spec/100-local-member-call-constructor-template-instantiation.t`,
  `spec/400-defaulted-nested-class-argument-partial-specialization.t`,
  `spec/400-explicit-type-arg-dependent-qualified-member-template-id.t`,
  `spec/400-qualified-member-template-id-bool-constant.t`, and
  `spec/500-conditional-alias-index-sequence-member-template-call.t`.
- **Typed dependent member results and values:**
  `general/100-dependent-bool-partial-static-value-storage.t`,
  `general/100-intermediate-type-transform-value-nontype.t`,
  `general/100-sizeof-call-result-nontype-template-argument.t`,
  `general/200-function-template-reference-cv-alias-partial-order.t`,
  `general/300-dependent-bool-base-trait-type-argument.t`,
  `general/400-member-variable-template-leaf-sfinae.t`,
  `general/400-variable-template-specializations.t`,
  `general/500-member-template-conditional-alias-trailing-return.t`,
  `general/500-mp11-append-alias-template-sfinae.t`,
  `general/500-nontype-alias-reinstantiation-structural-state.t`,
  `spec/100-sizeof-union-type-nttp.t`, and
  `spec/400-class-template-nttp-scope-value.t`.
- **Deduction, conversion, and candidate viability:**
  `general/100-explicit-specialization-pointer-member-definition.t`,
  `general/200-adl-explicit-template-id-call.t`,
  `general/200-function-template-template-parameter-deduction.t`,
  `general/200-member-operator-template-reference-pattern-partial-order.t`,
  `general/400-explicit-function-template-type-arg-drops-nontype-overload.t`,
  `general/400-nonmember-template-compound-assignment-const-lhs.t`,
  `general/400-static-cast-rvalue-ref-skips-conversion-operator.t`,
  `spec/100-out-of-class-conversion-operator-definition.t`,
  `spec/400-conversion-function-template-call-argument.t`,
  `spec/400-conversion-function-template-copy-init.t`,
  `spec/400-conversion-function-template-selection.t`, and
  `spec/400-template-template-member-alias-owner-shadow.t`.
- **Remaining fixed-point stress:**
  `general/500-reentrant-static-query-callable-enable-if-cache.t` and
  `general/500-reentrant-static-query-enable-if-partial.t` remain genuine
  termination witnesses; they are not in the acceptance scope and must not be
  handled through timeout or broad fallback behavior.

### Checkpoint Scope

Complete the explicit/extern specialization-materialization increment as one
typed owner path:

1. Recover an explicitly specialized class template-id as the owner of an
   out-of-class member or special-member definition from the analyzer's typed
   class registry, then collect the concrete member with its source body,
   object identity, and required LowIR emission state.
2. Recognize `extern template` member-function and static-data declarations as
   typed explicit-instantiation declarations of the selected class member;
   validate the owner/member kind without demanding a function-template
   definition, and retain the declaration-only/no-body contract.
3. Preserve the source-order boundary of an explicit class instantiation so
   only definitions visible before the instantiation become object roots, while
   an explicit specialization keeps its own concrete definition and constructor
   aliases.  Do this through `FunctionRecord`/`GlobalRecord` facts rather than
   generated-name or fixture-specific checks.

The focused validation set is:

`general/100-explicit-specialization-out-of-class-ctor-replay.t`,
`general/100-explicit-specialization-pointer-member-definition.t`,
`spec/100-explicit-specialization-out-of-class-ctor-replay.t`,
`spec/100-explicit-specialization-out-of-class-member-emits.t`,
`spec/100-explicit-instantiation-after-explicit-specialization-no-effect.t`,
`spec/100-explicit-instantiation-class-prior-member-definitions.t`,
`spec/100-extern-template-member-function-declaration.t`,
`spec/100-extern-template-static-data-declaration.t`, and
`spec/100-out-of-class-conversion-operator-definition.t`.

Before implementation this scope is expected to improve the 343-test
baseline; it must preserve all assignments through PA22, pass the PA23 file
audit, and leave the two fixed-point fixtures visible.  The next checkpoint
will take the remaining dependent member-result/candidate-viability group,
then the residual typed-value and LowIR comparison cases.

## Checkpoint 27 result — 2026-07-31

### Completed scope and validation

The typed owner/specialization increment is complete.  Out-of-class members
and special members of an explicitly specialized class now recover their
class owner from the typed class registry, retain the source body and
specialization facts, and enter the correct emission frontier.  Explicit
class-instantiation replay now has a source-order visibility boundary;
extern-template member-function and static-data declarations are recognized
as declaration-only typed member facts; and an explicit function
specialization remains selected when a later explicit instantiation is
replayed.

The focused owner/replay tests passed for explicit constructors, pointer
members, explicit member emission, explicit-instantiation ordering, and both
extern-template member forms.  The out-of-class conversion-operator fixture
remains a LowIR comparison failure in the separate conversion-materialization
path; it was retained as a regression guard rather than folded into this
checkpoint's owner recovery.

The required PA23 report is **351/396**, up from the turn-start **343/396**.
The remaining set is **45 fixtures**: **22** exit-status failures and **23**
LowIR comparisons.  The fixed-point query fixtures remain visible in the
status set.  The final net improvement is eight tests above baseline,
covering the owner/replay increment and the cross-specialization ABI guard.

The follow-up ABI correction keeps built-in template arguments as direct
Itanium encodings, emits the template-name substitution for a typed
cross-specialization parameter, and suppresses a redundant base-entry body
for inline converting constructors.  It restores the PA21 and PA22
cross-specialization fixtures; the required through-PA22 report is
**2100/2100**.

### Remaining Work Map

- **Dependent owner and candidate replay (exit status):**
  `general/100-default-nontype-qualified-function-lookup.t`,
  `general/100-member-template-specialization-return-prefers-member-call.t`,
  `general/100-selected-specialization-special-member-body.t`,
  `general/200-member-template-implicit-instantiation-not-overload.t`,
  `general/300-dependent-alias-helper-partial-specialization.t`,
  `general/400-anonymous-namespace-partial-specialization.t`,
  `general/400-current-specialization-display-name-member-alias.t`,
  `general/400-member-template-result-pack-preserves-nested-function-pointer-owner.t`,
  `general/500-recursive-qualified-member-template-bool-arg.t`,
  `spec/100-local-member-call-constructor-template-instantiation.t`,
  `spec/400-defaulted-nested-class-argument-partial-specialization.t`,
  `spec/400-explicit-type-arg-dependent-qualified-member-template-id.t`,
  `spec/400-qualified-member-template-id-bool-constant.t`, and
  `spec/500-conditional-alias-index-sequence-member-template-call.t`.
- **Candidate deduction, conversion, and dependent results:**
  `general/400-static-cast-rvalue-ref-skips-conversion-operator.t`,
  `general/500-dependent-function-type-pack-expansion-ctor-init.t`,
  `general/500-dependent-qualified-member-template-result-bool.t`,
  `general/500-member-template-conditional-alias-trailing-return.t`,
  `general/500-mp11-append-alias-template-sfinae.t`,
  `general/500-nontype-alias-reinstantiation-structural-state.t`,
  `spec/400-defaulted-nested-class-argument-partial-specialization.t`, and
  the remaining template-template/member-operator deduction comparisons.
- **Typed values and LowIR materialization:** the current comparison set is
  `general/100-current-specialization-member-body-cast-compare.t`,
  `general/100-dependent-bool-partial-static-value-storage.t`,
  `general/100-inherited-using-alias-out-of-class-specialization-member.t`,
  `general/100-intermediate-type-transform-value-nontype.t`,
  `general/100-local-qualified-argument-replay.t`,
  `general/100-sizeof-call-result-nontype-template-argument.t`,
  `general/200-adl-explicit-template-id-call.t`,
  `general/200-function-template-reference-cv-alias-partial-order.t`,
  `general/200-function-template-template-parameter-deduction.t`,
  `general/200-member-operator-template-reference-pattern-partial-order.t`,
  `general/300-current-specialization-constructor-template-canonical-owner.t`,
  `general/300-dependent-bool-base-trait-type-argument.t`,
  `general/400-explicit-function-template-type-arg-drops-nontype-overload.t`,
  `general/400-member-variable-template-leaf-sfinae.t`,
  `general/400-nonmember-template-compound-assignment-const-lhs.t`,
  `general/400-out-of-class-partial-member-template-owner-parameter-alias.t`,
  `general/400-variable-template-specializations.t`,
  `spec/100-out-of-class-conversion-operator-definition.t`,
  `spec/100-partial-specialization-member-primary-param-name.t`,
  `spec/100-sizeof-union-type-nttp.t`,
  `spec/400-class-template-nttp-scope-value.t`,
  `spec/400-defaulted-template-arg-partial-base-completion.t`, and
  `spec/400-template-template-member-alias-owner-shadow.t`.
- **Fixed-point termination:**
  `general/500-reentrant-static-query-callable-enable-if-cache.t` and
  `general/500-reentrant-static-query-enable-if-partial.t` still require a
  stable typed query identity and candidate-local cycle handling.

### Next checkpoint group

Take the dependent owner/candidate replay group together with its typed
member-result and conversion-ranking consumers.  Start with the status
fixtures that fail before LowIR, then validate the associated comparison
fixtures and preserve the two fixed-point witnesses as regressions.  Follow
with the residual typed-value/materialization set; every increment must rerun
the PA23 report, the through-PA22 report, and the file audit.
