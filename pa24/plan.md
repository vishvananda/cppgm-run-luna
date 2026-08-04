# PA24 checkpoint plan

## Baseline and contract

The turn-start PA24 report had 104 tests with 26 passing and 78 failing.  The
complete failure set was inspected and grouped by shared compiler behavior
before implementation.  The map below is the complete post-checkpoint
failure set after the typed initialization, conversion, range-for, and first
captureless-lambda increments.

## Remaining Work Map

### 1. Aggregate and direct-list LowIR shape (2 tests)

The semantic paths are present, but these cases still have a structural
LowIR mismatch in constructor/default handling:

- `200-aggregate-omitted-class-tail`
- `200-direct-init-single-braced-constructor-argument`

### 2. Lambda closure synthesis and callable/template use (22 tests)

The supported captureless function-pointer path is complete.  Closure
entities, captures, and callable/template deduction are not yet lowered in
these cases:

- `200-captureless-lambda-forwarding-reference-call`
- `200-captureless-lambda-template-parameter-call`
- `200-included-template-member-dual-capturing-lambda-call`
- `200-lambda-argument-overload-resolution`
- `200-lambda-closure-assignment-sfinae`
- `200-lambda-constructor-template-preferred`
- `200-lambda-copy-move-constructible-traits`
- `200-lambda-enumerator-default-capture`
- `200-lambda-local-destructor-sfinae`
- `200-lambda-local-synthesized-special-member-symbol`
- `200-lambda-member-template-assignment`
- `200-member-template-captureless-lambda-inside-if`
- `200-member-template-explicit-captureless-lambda-call`
- `200-nested-lambda-captures-outer-local`
- `200-nested-lambda-helper-capture`
- `200-nested-lambda-implicit-member-call`
- `200-nested-lambda-this-capture`
- `200-unqualified-static-member-template-call-in-lambda`
- `300-capturing-lambda-enclosing-pack-forward-call`
- `300-capturing-lambda-forward-pack-call`
- `300-explicit-template-lambda-closure-return-type`
- `spec/300-local-lambda-function-template-concrete-type`

### 3. Conversion and semantic lookup closure (5 tests)

These smaller semantic edges remain after the general conversion and cast
paths were extended:

- `200-conversion-operator-reference-overload-rank`
- `200-conversion-operator-using-alias`
- `200-decltype-comma-expression`
- `200-defaulted-member-alias-constructor`
- `200-template-body-scoped-enum-functional-cast`

### 4. Dependent template replay and result recovery (2 tests)

Concrete substitutions are still not recovered through the complete dependent
helper scope in these cases:

- `400-dependent-owner-lifecycle-validation`
- `500-dependent-function-result-recovery`

## Checkpoint Scope

This checkpoint covers typed `auto` deduction for scalar, pointer, array,
reference, and class initializers; direct braced scalar/array/aggregate
initialization; string-array initialization; aggregate class tails and
omitted-field construction; condition declarations; ordinary visible-body
`auto` return deduction; and `constexpr auto` static members.  It also covers
the associated integral/enum/pointer casts, class conversion arguments,
class arithmetic, anonymous-union projections, and reference
increment/decrement.  Class-valued reference arguments use a direct
destination for template conversions and a copied temporary for ordinary
conversions, preserving both PA24 argument semantics and earlier template
ABI behavior.

The range-for increment is complete for bounded arrays, braced-init lists,
member `begin`/`end`, namespace-qualified ADL `begin`/`end`, inherited member
ranges, const/reference loop declarations, and the lambda-user-range case.
Its hidden range, iterator, condition, update, and cleanup state is lowered
through the existing typed control-flow path.

## Checkpoint Result

Completed.  `make test-report ACTIVE_TEST_REPORT_PAS='pa24'` reports
`73 / 104` tests passed, improving the turn-start baseline by 47 tests.
`make test-report-through-pa23` is green at `2496 / 2496`, and the PA24 file
audit passes with only the repository's existing warnings.

## Current checkpoint scope

This turn targets the first coherent lambda increment: captureless lambda
expressions with explicit or deduced non-class return types, ordinary named
parameters, local/global function-pointer conversion, and lowering of their
compound bodies through the existing typed function/control-flow machinery.
The validation scope is the basic local/global/implicit-return lambda cases
plus the existing PA24 report and through-PA23 gate.  Captures, generic
lambda/template replay, and class-valued lambda returns remain outside this
increment and stay grouped below until this callable foundation is stable.

## Next checkpoint group

Implement captureless closure-object materialization and template-callable
deduction as one group.  This should cover lambda arguments to forwarding and
ordinary templates, closure-sensitive assignment/trait queries, and the
member-template call paths.  Capturing and nested lambdas then remain the
following group, followed by the two aggregate LowIR mismatches, the five
semantic lookup edges, and dependent template replay.
