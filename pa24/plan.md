# PA24 checkpoint plan

## Baseline and contract

The turn-start PA24 report had 104 tests with 26 passing and 78 failing.  The
complete failure set was inspected and grouped by shared compiler behavior
before implementation.  This checkpoint completes the typed initialization,
conversion, and supported range-for increment; the map below is the complete
post-checkpoint failure set.

## Remaining Work Map

### 1. Aggregate and direct-list LowIR shape (2 tests)

The semantic paths are present, but these cases still have a structural
LowIR mismatch in constructor/default handling:

- `200-aggregate-omitted-class-tail`
- `200-direct-init-single-braced-constructor-argument`

### 2. Lambda closure synthesis and callable/template use (36 tests)

Lambda ASTs parse, but closure entities, captures, implicit returns, and
callable/template deduction are not yet lowered:

- `200-captureless-lambda-forward-pack-array-ref-call`
- `200-captureless-lambda-forward-pack-call`
- `200-captureless-lambda-forwarding-reference-call`
- `200-captureless-lambda-implicit-return-parameter-member`
- `200-captureless-lambda-no-declarator-template-reference`
- `200-captureless-lambda-private-member-object-call`
- `200-captureless-lambda-sizeof-parameter-pack-call`
- `200-captureless-lambda-sizeof-parameter-pack`
- `200-captureless-lambda-template-parameter-call`
- `200-captureless-lambda-trailing-return-decltype-parameter`
- `200-captureless-lambda-wrapper-conversion`
- `200-class-template-lambda-static-member-template-call`
- `200-included-template-member-dual-capturing-lambda-call`
- `200-lambda-argument-overload-resolution`
- `200-lambda-closure-assignment-sfinae`
- `200-lambda-constructor-template-preferred`
- `200-lambda-copy-move-constructible-traits`
- `200-lambda-enumerator-default-capture`
- `200-lambda-global`
- `200-lambda-local-destructor-sfinae`
- `200-lambda-local-synthesized-special-member-symbol`
- `200-lambda-local`
- `200-lambda-member-template-assignment`
- `200-lambda-multistmt-implicit-return`
- `200-lambda-user-range-for-implicit-return`
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
ranges, and const/reference loop declarations.  Its hidden range, iterator,
condition, update, and cleanup state is lowered through the existing typed
control-flow path.  The lambda-user-range case remains in the lambda group
because its range body is inside an unsupported lambda entity.

## Checkpoint Result

Completed.  `make test-report ACTIVE_TEST_REPORT_PAS='pa24'` reports
`59 / 104` tests passed, improving the turn-start baseline by 33 tests.
`make test-report-through-pa23` is green at `2496 / 2496`, and the PA24 file
audit passes with only the repository's existing warnings.

## Next checkpoint group

Implement captureless lambda entity synthesis first, including function-body
scope and implicit return deduction, then extend it to supported by-reference
and `this` captures.  Recheck the two aggregate/direct-list LowIR cases and
the five semantic lookup edges after the lambda boundary is stable; finish
with dependent template replay and lambda/template combinations.
