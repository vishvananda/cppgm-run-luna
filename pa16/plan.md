# PA16 checkpoint plan

## Baseline

The required `make test-report ACTIVE_TEST_REPORT_PAS='pa16'` baseline is
24/164 tests passing.  The failures below are grouped by the compiler behavior
they share; every failure in the baseline report is represented in exactly one
group (apart from the two intentionally separate directory-qualified cases
with the same stem).

## Remaining Work Map

### Value transfers, special members, and temporary materialization

`general/100-derived-converting-ctor-beats-base-copy.t`,
`general/100-xvalue-pass-by-value-uses-move-ctor.t`,
`general/100-xvalue-ref-call-binds-directly.t`,
`general/100-xvalue-static-cast-return-uses-move-ctor.t`,
`general/200-aggregate-appertainment-direct-copy-reference-members.t`,
`general/200-class-reference-return-forward-to-const-ref.t`,
`general/200-class-reference-return-forward-to-rvalue-ref.t`,
`general/200-conditional-hidden-copy-ctor-support.t`,
`general/200-constructor-argument-temporary-dtor.t`,
`general/200-cstyle-class-cast-direct-initializes.t`,
`general/200-default-member-class-call-init-target.t`,
`general/200-derived-to-base-by-value-overload.t`,
`general/200-enum-class-pass-by-value.t`,
`general/200-implicit-copy-assignment.t`,
`general/200-implicit-copy-constructor.t`,
`general/200-nested-subobject-pass-return-by-value.t`,
`general/200-pass-by-value-lvalue.t`,
`general/200-pass-return-forwarding.t`,
`general/200-prvalue-field-access-function-return.t`,
`general/200-prvalue-field-access-temporary.t`,
`general/200-prvalue-method-call-temporary.t`,
`general/200-return-by-value-init.t`,
`general/200-return-nullptr-class-braced-default.t`,
`general/200-rvalue-reference-call-pass-by-value.t`,
`general/200-static-cast-explicit-constructor.t`,
`general/300-base-rvalue-reference-assignment.t`,
`general/300-class-conditional-prvalue-return.t`,
`general/300-conditional-local-prvalue-init-elides-copy.t`,
`general/300-defaulted-copy-constructor-base-copy-init.t`,
`general/300-defaulted-move-ctor-not-copy-fallback.t`,
`general/300-direct-object-parameter-passthrough-base-copy.t`,
`general/300-discarded-class-call.t`,
`general/300-discarded-large-class-call-void-cast.t`,
`general/300-empty-base-defaulted-assignment-user-copy-ctor.t`,
`general/300-empty-base-subobject-assignment-no-storage.t`,
`general/300-explicit-default-copy-assignment-with-move-ctor.t`,
`general/300-extra-required-rvalue-ctor-not-move-ctor.t`,
`general/300-function-pointer-class-return-call.t`,
`general/300-function-style-braced-prvalue-init-elides-copy.t`,
`general/300-generated-move-constructor-nontrivial-member.t`,
`general/300-implicit-assign-symbol-collides-operator-deref.t`,
`general/300-indirect-call-result-rvalue-reference-materialization.t`,
`general/300-leading-trivial-prefix-storage-copy-enum.t`,
`general/300-leading-trivial-prefix-storage-copy.t`,
`general/300-local-default-ctor-return-copy.t`,
`general/300-local-prvalue-init-elides-move.t`,
`general/300-local-return-slot-reuse.t`,
`general/300-move-only-aggregate-brace-member.t`,
`general/300-qualified-base-implicit-copy-assignment.t`,
`general/300-refmember-copy-constructor-binds-storage.t`,
`general/300-return-const-ref-parameter-copy-not-move.t`,
`general/300-return-member-copy-ctor.t`,
`general/300-rvalue-ref-brace-temp-direct-materialization.t`,
`general/300-rvalue-reference-cast-value-init-temporary.t`,
`general/300-shadowed-local-cleanup-rebind-on-early-return.t`,
`general/300-trivial-copy-value-transfer-storage-copy.t`,
`general/300-xvalue-call-init-uses-move-ctor.t`,
`general/400-by-value-param-converting-ctor-overload.t`,
`general/400-nonclass-by-value-converting-ctor-argument.t`,
`general/400-reference-converting-ctor-default-argument.t`,
`general/400-reference-param-converting-ctor-temporary.t`,
`general/400-shadowed-local-cleanup-rebind-on-return.t`,
`general/400-static-cast-class-reference-temporary.t`,
`spec/100-copy-suppresses-implicit-move.t`,
`spec/100-defaulted-move-nontrivial-subobject.t`,
`spec/100-implicit-copy-assignment-deleted-base-bad.t`,
`spec/100-implicit-move-assignment-moveonly-member.t`,
`spec/200-moveonly-defaulted-move-assignment.t`,
`spec/200-moveonly-defaulted-move.t`,
`spec/200-out-of-class-defaulted-special-members.t`,
`spec/300-implicit-move-constructor-moveonly-member.t`.

### Ref-qualified members and qualified member/operator lookup

`general/200-out-of-class-ref-qualified-member-definition.t`,
`general/200-qualified-operator-function-id.t`,
`general/200-ref-qualified-call-operator.t`,
`general/200-ref-qualified-member-call-conditional-shadowed-setter.t`,
`general/200-ref-qualified-member-function-lvalue.t`,
`general/200-ref-qualified-member-function-prvalue.t`,
`general/300-ref-qualified-member-call-implicit-object-rank.t`,
`spec/200-const-ref-qualified-temporary-call.t`.

### ADL, using directives, and nonmember operator candidate sets

`general/200-adl-ignores-using-declaration-candidate.t`,
`general/200-associated-namespace-adl-function-call.t`,
`general/200-hidden-friend-adl-function-call.t`,
`general/200-hidden-friend-adl.t`,
`general/200-member-call-braced-default-argument.t`,
`general/200-namespaced-nonmember-operator-greater-adl.t`,
`general/200-namespaced-nonmember-operator-minus-adl.t`,
`general/200-nonmember-operator-minus.t`,
`general/200-parenthesized-braced-aggregate-constructor-arg.t`,
`general/200-parenthesized-function-name-suppresses-adl.t`,
`general/200-pointer-predecrement-reference-argument.t`,
`general/200-reverse-friend-plus-operator.t`,
`general/300-qualified-friend-private-constructor-return.t`,
`general/300-using-directive-imported-value-ambiguous.t`.

### Delegating/out-of-class special members and object lifetime

`general/200-out-of-class-special-members.t`,
`general/300-delegating-constructor-basic.t`,
`general/300-delegating-constructor.t`,
`general/300-delegating-external-ctor-overload-nonfirst-argument.t`,
`general/300-delete-class-pointer-lifetime.t`,
`general/300-delete-selects-usual-deallocation.t`,
`general/300-destructor-alias-member-call.t`,
`general/300-for-init-class-lifetime-extends-through-loop.t`,
`general/300-shadowed-local-cleanup-rebind-on-early-return.t`,
`general/400-shadowed-local-cleanup-rebind-on-return.t`.

### New/delete and array object lifetime

`general/300-class-new-expression-default-constructor.t`,
`general/300-class-specific-placement-new.t`,
`general/300-global-operator-new-delete-call.t`,
`general/300-nothrow-new-null-skips-constructor.t`,
`general/300-placement-new-scalar-init-converts-source.t`,
`general/300-string-literal-rvalue-ref-pointer-argument.t`,
`general/300-void-cast-placement-new-preserves-result-type.t`,
`general/400-array-new-default-ctor-default-arg.t`,
`general/400-array-new-empty-paren-value-init.t`,
`general/400-delete-array-expression.t`,
`general/400-global-array-delete.t`,
`general/400-global-class-array-named-implicit-copy.t`,
`general/400-global-operator-new-array-delete-array-call.t`,
`general/400-nontrivial-class-array-new-delete.t`,
`general/400-trivial-class-array-new.t`.

### Union, anonymous storage, bit-fields, and aggregate initialization

`general/200-aggregate-appertainment-direct-copy-reference-members.t`,
`general/300-anonymous-union-member-id-expression.t`,
`general/300-bit-field-copy-semantics.t`,
`general/300-class-array-missing-value-init.t`,
`general/300-local-anonymous-union-sizeof.t`,
`general/300-named-unnamed-union-member-layout.t` is already passing and is
therefore not part of the failure set,
`general/300-union-special-member-storage-copy.t`,
`general/300-union-trivial-subobject-dtor-omission.t`,
`general/300-union-user-default-ctor-inactive-class-member.t`.

### Conversion operators and built-in/operator overload ranking

`general/300-pointer-bool-local-storage-lowering.t`,
`general/300-proxy-subscript-assignment.t`,
`general/400-built-in-unary-class-conversion.t`,
`general/400-builtin-comparison-after-rejected-operator-overload.t`,
`general/400-builtin-comparison-prefers-fewer-user-conversions.t`,
`general/400-c-style-cast-explicit-bool-conversion.t`,
`general/400-class-pointer-conversion-builtin-eq.t`,
`general/400-class-pointer-conversion-builtin-subscript.t`,
`general/400-compound-assignment-rhs-conversion.t`,
`general/400-condition-declaration-user-bool-conversion.t`,
`general/400-cv-conversion-operator-builtin-comparison.t`,
`general/400-cv-conversion-operator-overloads.t` is already passing,
`general/400-direct-init-explicit-bool-conversion.t`,
`general/400-explicit-conversion-operator-alias-target-call.t`,
`general/400-logical-not-after-rejected-operator-overload.t`,
`general/400-out-of-class-qualified-conversion-operator-result.t`,
`general/400-pointer-arithmetic-class-conversion.t`,
`general/400-switch-condition-declaration-conversion-operator.t`,
`general/400-user-defined-conversion-path-tiebreak.t`,
`general/400-user-defined-conversion-second-rank.t`,
`spec/400-inherited-conversion-operator-parameter-binding.t`.

## Checkpoint Scope

Implement the first value-semantics group as a typed extension of the existing
PA15 object model:

1. classify complete class parameters and results as indirect LowIR storage,
2. synthesize demand-driven copy/move constructor and assignment records for
   ordinary field-wise/base-wise classes, including reference and nested-class
   members where the existing lifetime machinery can lower them,
3. select copy versus move construction from the source value category and
   lower class initialization, pass-by-value arguments, and return-by-value
   transfers through the selected helper or a direct trivial storage copy,
4. preserve derived-to-base address adjustment and ensure helper calls receive
   their complete object ABI arguments, and
5. materialize/destroy the common temporary objects created by those paths.

Validation for this checkpoint is the baseline's core value-transfer subset:
the `general/100` xvalue cases, the `general/200` copy/value/return cases,
the `general/300` copy/move/base/return-slot cases, and the PA16 copy/move
spec cases.  The next checkpoint group is ref-qualified member viability and
qualified lookup; after that come delegating/out-of-class lifetime, allocation,
union/aggregate, and conversion/ADL groups.

## Checkpoint result

The first checkpoint is implemented.  The focused copy/value set passes 7/7,
and the full PA16 report increased from 24/164 to 38/164.  It now covers
typed indirect class parameters/results, direct xvalue reference binding,
implicit and defaulted copy/move construction, implicit assignment, trivial
storage transfers, and the basic pass/return forwarding cases.

Checkpoint 2 is implemented as well: its selected class-result/address
fixtures pass, and the full report is now 54/164.  It added conditional
destination construction, direct indirect-result forwarding, derived-to-base
constructor selection, and full-expression temporary reference
materialization/destruction.

## Remaining Work Map after checkpoint 2

The complete post-checkpoint-2 report has 110 failures.  The remaining value
group is concentrated in class conversion/cast initialization, default
arguments and enum/object ABI details, derived/base storage-copy shapes, and
deleted/nontrivial special-member viability.  The other groups remain
ref-qualified member viability and
qualified lookup; ADL/using/nonmember operators; delegating and out-of-class
member/lifetime cleanup; new/delete and arrays; union/anonymous/bit-field
storage; and conversion/operator ranking.  Several tests in these latter
groups are still front-end or independent lowering gaps and are intentionally
kept separate from the value-transfer fixes.

## Checkpoint Scope 2

Implement class-prvalue materialization at reference and member-access sites,
lower class conditional expressions directly into their destination object,
and repair constructor base-entry calls so the complete `this` plus argument
ABI is forwarded.  Validate this group with
`100-xvalue-static-cast-return-uses-move-ctor`,
`200-rvalue-reference-call-pass-by-value`,
`200-constructor-argument-temporary-dtor`,
`300-class-conditional-prvalue-return`,
`200-prvalue-field-access-function-return`,
`100-derived-converting-ctor-beats-base-copy`,
`200-derived-to-base-by-value-overload`,
`300-defaulted-copy-constructor-base-copy-init`, and the direct-object/base
parameter and rvalue-reference materialization fixtures.  This scope covers
the remaining common class-result/address shape rather than the independent
lookup, allocation, union, and conversion groups.

## Checkpoint Scope 3

Close the remaining class-value conversion and viability seam: lower scalar
and explicit class casts into an initialized class destination, preserve
conversion constructors through by-value argument selection, and reject
deleted copy/move operations in implicit initialization and assignment.
Validate with the current `200-cstyle-class-cast-direct-initializes`,
`200-static-cast-explicit-constructor`, by-value converting-constructor and
enum fixtures, `100-copy-constructor-default-parameter`, and the deleted
copy/move spec fixtures.  This leaves ref-qualified lookup and the independent
ADL, allocation, union, and conversion-operator groups separate.

## Current report and Remaining Work Map

The latest required PA16 report is 64/164, up from the 24/164 turn-start
baseline.  The complete remaining set is represented by the checked-in
failure artifacts and is grouped as follows: ref-qualified member/operator
viability and out-of-class matching (`200-out-of-class-ref-qualified-member-`
`definition`, the four failing `200-ref-qualified-*`/`300-ref-qualified-*`
LowIR cases, and
`300-ref-qualified-member-call-implicit-object-rank`); class transfers and
special-member/lifetime cleanup (the remaining prvalue, conditional,
defaulted/deleted, base/assignment, discarded-call, and local-slot cases);
ADL/qualified lookup and aggregate argument handling; new/delete and array
object lifetime; union/anonymous storage and bit-fields; and conversion
operator/built-in overload ranking.  Status-only failures in these groups are
lowering or semantic viability gaps, while LowIR-only failures are shape or
selection gaps.

## Checkpoint Scope 4

Implement ref-qualified member viability as typed function-state facts and
carry the parser's existing `&`/`&&` qualifier nodes through function types,
overload identity, out-of-class member matching, and implicit-object call
selection.  Validate the lvalue/rvalue member and `operator()` fixtures, the
conditional shadowed setter, the implicit-object ranking case, and the
out-of-class definitions.  This scope covers the complete currently failing
ref-qualified group; the next group is deleted/nontrivial special-member
viability and remaining class-transfer shapes.

## Checkpoint 4 result

The ref-qualified checkpoint passes its focused member/operator, conditional,
implicit-object ranking, temporary, and out-of-class-definition fixtures.
Function types now retain both ref-qualifier facts through overload identity,
and class-valued member-call operands use the common temporary materialization
path.  The focused ref-qualified set was clean; the final full PA16 report is
64/164.  The next
checkpoint is the deleted/nontrivial special-member viability group, followed
by the remaining class-transfer, lifetime, allocation, union, ADL, and
conversion groups.

## Checkpoint 4 validation and current handoff

The required full report after restoring PA15-safe comparison and enum
conversion behavior is 64/164; the earlier-PA gate remains clean at 1019/1019.
The audit also passes after moving value ABI/conversion helpers and call-address
materialization into dedicated implementation units.  The remaining work map
is unchanged in substance: deleted/nontrivial special-member viability and
class transfer shapes are the next coherent group, followed by ADL/qualified
lookup, lifetime/allocation/arrays, unions/aggregates/bit-fields, and
conversion-operator ranking.  The next checkpoint should target deleted and
nontrivial copy/move selection, including base/member viability and the
corresponding class return/argument transfers.

## Checkpoint Scope 5

Implement the aggregate/braced-initialization and reference-temporary ABI seam
in the existing typed value-lowering state.  This scope covers:

- direct versus indirect class-result storage based on complete object size and
  nontrivial special members, including reference-member objects;
- aggregate appertainment, scalar evaluation order, nested aggregate transfer,
  and parenthesized braced aggregate constructor arguments;
- parser and lowering support for function-style braced construction,
  braced/default class arguments, and class assignment from a braced value;
- rvalue-reference conversion ranking, array-to-pointer decay, converting
  constructor materialization, class-reference casts, and returned class
  storage; and
- member lookup for default member class initializers and direct copy/move
  storage transfers at those sites.

The focused checkpoint validation is clean for
`general/200-aggregate-appertainment-direct-copy-reference-members`,
`general/200-parenthesized-braced-aggregate-constructor-arg`,
`general/200-member-call-braced-default-argument`,
`general/200-default-member-class-call-init-target`,
`general/300-function-style-braced-prvalue-init-elides-copy`,
`general/300-rvalue-ref-brace-temp-direct-materialization`,
`general/300-rvalue-reference-cast-value-init-temporary`,
`general/400-reference-converting-ctor-default-argument`,
`general/400-reference-param-converting-ctor-temporary`,
`general/400-static-cast-class-reference-temporary`,
`general/300-refmember-copy-constructor-binds-storage`,
`general/100-xvalue-ref-call-binds-directly`,
`general/300-xvalue-call-init-uses-move-ctor`, and
`general/300-string-literal-rvalue-ref-pointer-argument`.

## Remaining Work Map after checkpoint 5

The latest complete PA16 report is 89/164, up from the 24/164 turn-start
baseline.  The complete remaining failure set is grouped below; LowIR
failures are shape/selection mismatches, while exit-status failures still need
semantic or lowering behavior.

### ADL, lookup, and nonmember operators

`general/200-associated-namespace-adl-function-call`,
`general/200-hidden-friend-adl-function-call`,
`general/200-hidden-friend-adl`,
`general/200-reverse-friend-plus-operator`, and
`general/300-using-directive-imported-value-ambiguous`.

### Class ABI, transfers, and special-member/lifetime shapes

`general/200-derived-to-base-by-value-overload`,
`general/200-enum-class-pass-by-value`,
`general/200-out-of-class-special-members`,
`general/200-prvalue-field-access-temporary`,
`general/200-return-nullptr-class-braced-default`,
`general/300-base-rvalue-reference-assignment`,
`general/300-bit-field-copy-semantics`,
`general/300-class-array-missing-value-init`,
`general/300-conditional-local-prvalue-init-elides-copy`,
`general/300-direct-object-parameter-passthrough-base-copy`,
`general/300-discarded-class-call`,
`general/300-discarded-large-class-call-void-cast`,
`general/300-empty-base-defaulted-assignment-user-copy-ctor`,
`general/300-empty-base-subobject-assignment-no-storage`,
`general/300-explicit-default-copy-assignment-with-move-ctor`,
`general/300-indirect-call-result-rvalue-reference-materialization`,
`general/300-leading-trivial-prefix-storage-copy-enum`,
`general/300-leading-trivial-prefix-storage-copy`,
`general/300-move-only-aggregate-brace-member`,
`general/300-qualified-base-implicit-copy-assignment`,
`general/300-return-const-ref-parameter-copy-not-move`, and
`general/300-trivial-copy-value-transfer-storage-copy`.

### Allocation, construction, destruction, and arrays

`general/300-class-new-expression-default-constructor`,
`general/300-class-specific-placement-new`,
`general/300-delegating-constructor-basic`,
`general/300-delegating-constructor`,
`general/300-delegating-external-ctor-overload-nonfirst-argument`,
`general/300-delete-class-pointer-lifetime`,
`general/300-delete-selects-usual-deallocation`,
`general/300-destructor-alias-member-call`,
`general/300-global-operator-new-delete-call`,
`general/300-nothrow-new-null-skips-constructor`,
`general/300-placement-new-scalar-init-converts-source`,
`general/300-void-cast-placement-new-preserves-result-type`,
`general/400-array-new-default-ctor-default-arg`,
`general/400-array-new-empty-paren-value-init`,
`general/400-delete-array-expression`,
`general/400-for-init-class-lifetime-extends-through-loop`,
`general/400-global-array-delete`,
`general/400-global-class-array-named-implicit-copy`,
`general/400-global-operator-new-array-delete-array-call`,
`general/400-nontrivial-class-array-new-delete`, and
`general/400-trivial-class-array-new`.

### Unions, anonymous storage, and aggregate members

`general/300-anonymous-union-member-id-expression`,
`general/300-local-anonymous-union-sizeof`,
`general/300-union-special-member-storage-copy`,
`general/300-union-trivial-subobject-dtor-omission`, and
`general/300-union-user-default-ctor-inactive-class-member`.

### Conversion operators and built-in/operator ranking

`general/300-pointer-bool-local-storage-lowering`,
`general/300-proxy-subscript-assignment`,
`general/400-built-in-unary-class-conversion`,
`general/400-builtin-comparison-after-rejected-operator-overload`,
`general/400-builtin-comparison-prefers-fewer-user-conversions`,
`general/400-by-value-param-converting-ctor-overload`,
`general/400-c-style-cast-explicit-bool-conversion`,
`general/400-class-pointer-conversion-builtin-eq`,
`general/400-class-pointer-conversion-builtin-subscript`,
`general/400-compound-assignment-rhs-conversion`,
`general/400-condition-declaration-user-bool-conversion`,
`general/400-cv-conversion-operator-builtin-comparison`,
`general/400-direct-init-explicit-bool-conversion`,
`general/400-explicit-conversion-operator-alias-target-call`,
`general/400-logical-not-after-rejected-operator-overload`,
`general/400-out-of-class-qualified-conversion-operator-result`,
`general/400-pointer-arithmetic-class-conversion`,
`general/400-shadowed-local-cleanup-rebind-on-return`,
`general/400-switch-condition-declaration-conversion-operator`,
`general/400-user-defined-conversion-path-tiebreak`,
`general/400-user-defined-conversion-second-rank`, and
`spec/400-inherited-conversion-operator-parameter-binding`.

The next checkpoint should target allocation/deallocation and delegating
constructor lifetime, then return to the remaining class-transfer shapes;
ADL/lookup, union storage, and conversion ranking remain separate groups.

## Checkpoint 5 result

The aggregate/braced/reference checkpoint is implemented.  Its focused tests
pass, the full PA16 report is 89/164, the through-PA15 gate is 1019/1019,
and the PA16 source audit passes.  The object-transfer implementation was
kept in the dedicated object-lowering unit so the control unit remains within
the stage file-size limit.

## Checkpoint Scope 6

Implement typed scalar allocation/deallocation and delegating-constructor
lifetime over the existing constructor, call, and object state.  This scope
covers:

- builtin and user-provided `operator new`/`delete` bindings, stable runtime
  symbols, placement arguments, class-specific allocation lookup, scalar
  initialization conversion, and nothrow null-result guarding;
- scalar `delete` overload selection, class destructor-before-deallocation
  ordering, null-pointer guards, and destructor-name alias lookup; and
- delegating mem-initializers, including external constructor overloads,
  without separately initializing the delegated object's bases or members.

The focused allocation/delegation set passed 11/12 after the PA15-safe
comparison repair; the one remaining fixture is a LowIR-only shape mismatch
inside the class-specific placement-new body.  The ordinary scalar/class
allocation, delete, nothrow, placement, destructor-alias, and all three
delegation fixtures pass.  The full PA16 report is now 100/164, up from
89/164 at the start of this checkpoint.

## Remaining Work Map after checkpoint 6

The latest complete PA16 report has 64 failures.  They are grouped below;
the class-specific placement fixture remains in the allocation group because
its runtime behavior is implemented but its canonical LowIR shape is not.

### ADL, lookup, and nonmember operators

`general/200-associated-namespace-adl-function-call`,
`general/200-hidden-friend-adl-function-call`,
`general/200-hidden-friend-adl`,
`general/200-reverse-friend-plus-operator`, and
`general/300-using-directive-imported-value-ambiguous`.

### Class ABI, transfers, and special-member/lifetime shapes

`general/200-derived-to-base-by-value-overload`,
`general/200-enum-class-pass-by-value`,
`general/200-out-of-class-special-members`,
`general/200-prvalue-field-access-temporary`,
`general/200-return-nullptr-class-braced-default`,
`general/300-base-rvalue-reference-assignment`,
`general/300-bit-field-copy-semantics`,
`general/300-class-array-missing-value-init`,
`general/300-conditional-local-prvalue-init-elides-copy`,
`general/300-direct-object-parameter-passthrough-base-copy`,
`general/300-discarded-class-call`,
`general/300-discarded-large-class-call-void-cast`,
`general/300-empty-base-defaulted-assignment-user-copy-ctor`,
`general/300-empty-base-subobject-assignment-no-storage`,
`general/300-explicit-default-copy-assignment-with-move-ctor`,
`general/300-indirect-call-result-rvalue-reference-materialization`,
`general/300-leading-trivial-prefix-storage-copy-enum`,
`general/300-leading-trivial-prefix-storage-copy`,
`general/300-move-only-aggregate-brace-member`,
`general/300-qualified-base-implicit-copy-assignment`,
`general/300-return-const-ref-parameter-copy-not-move`, and
`general/300-trivial-copy-value-transfer-storage-copy`.

### Allocation, construction, destruction, and arrays

`general/300-class-specific-placement-new`,
`general/400-array-new-default-ctor-default-arg`,
`general/400-array-new-empty-paren-value-init`,
`general/400-delete-array-expression`,
`general/400-for-init-class-lifetime-extends-through-loop`,
`general/400-global-array-delete`,
`general/400-global-class-array-named-implicit-copy`,
`general/400-global-operator-new-array-delete-array-call`,
`general/400-nontrivial-class-array-new-delete`,
`general/400-shadowed-local-cleanup-rebind-on-return`, and
`general/400-trivial-class-array-new`.

### Unions, anonymous storage, and aggregate members

`general/300-anonymous-union-member-id-expression`,
`general/300-local-anonymous-union-sizeof`,
`general/300-union-special-member-storage-copy`,
`general/300-union-trivial-subobject-dtor-omission`, and
`general/300-union-user-default-ctor-inactive-class-member`.

### Conversion operators and built-in/operator ranking

`general/300-pointer-bool-local-storage-lowering`,
`general/300-proxy-subscript-assignment`,
`general/400-built-in-unary-class-conversion`,
`general/400-builtin-comparison-after-rejected-operator-overload`,
`general/400-builtin-comparison-prefers-fewer-user-conversions`,
`general/400-by-value-param-converting-ctor-overload`,
`general/400-c-style-cast-explicit-bool-conversion`,
`general/400-class-pointer-conversion-builtin-eq`,
`general/400-class-pointer-conversion-builtin-subscript`,
`general/400-compound-assignment-rhs-conversion`,
`general/400-condition-declaration-user-bool-conversion`,
`general/400-cv-conversion-operator-builtin-comparison`,
`general/400-direct-init-explicit-bool-conversion`,
`general/400-explicit-conversion-operator-alias-target-call`,
`general/400-logical-not-after-rejected-operator-overload`,
`general/400-out-of-class-qualified-conversion-operator-result`,
`general/400-pointer-arithmetic-class-conversion`,
`general/400-switch-condition-declaration-conversion-operator`,
`general/400-user-defined-conversion-path-tiebreak`,
`general/400-user-defined-conversion-second-rank`, and
`spec/400-inherited-conversion-operator-parameter-binding`.

The next checkpoint should implement array-new/delete cookies and element
construction/destruction, including trivial and nontrivial class arrays; the
remaining class-transfer shapes, ADL/lookup, union storage, and conversion
ranking stay separate follow-on groups.

## Checkpoint Scope 7

Implement the array-new/delete lifetime path over typed element and allocation
facts.  The scope is the current array cluster:

`general/400-array-new-default-ctor-default-arg`,
`general/400-array-new-empty-paren-value-init`,
`general/400-delete-array-expression`,
`general/400-global-array-delete`,
`general/400-global-class-array-named-implicit-copy`,
`general/400-global-operator-new-array-delete-array-call`,
`general/400-nontrivial-class-array-new-delete`, and
`general/400-trivial-class-array-new`, with the class-specific placement
fixture’s shared LowIR allocation shape kept in the same validation group.

The implementation must derive dynamic bounds from the new-type-id AST,
allocate the typed byte count through `operator new[]`, reserve and maintain
the count cookie for class arrays, construct forward, destroy backward, and
pass the cookie address to `operator delete[]`; scalar arrays remain
cookie-free and use value-initialization loops when an empty initializer is
present.

## Checkpoint 7 result

The array allocation/lifetime checkpoint is implemented.  The focused array
new/delete fixtures now pass, including default-argument constructors,
dynamic-bound value-initialization, scalar arrays, trivial class arrays,
nontrivial class cookies and reverse destruction, direct `operator new[]` /
`operator delete[]` calls, and the global class-array copy initializer.  The
class-specific placement-new LowIR shape also passes.  The full PA16 report is
109/164, up from 100/164 at the start of this checkpoint.  The PA1--PA15 gate
is 1019/1019 and the PA16 source audit passes.

The implementation keeps the bound expression in typed lowering state, saves
dynamic byte sizes in special slots, emits an eight-byte count cookie only for
class arrays, and uses the existing constructor/destructor and aggregate
transfer machinery for element lifetime.  It also preserves direct scalar
constant stores and the canonical nested pointer-offset forms used by the
allocation fixtures.

## Remaining Work Map after checkpoint 7

The current report has 55 failures, grouped by shared behavior:

### ADL, lookup, and nonmember operators

`general/200-associated-namespace-adl-function-call`,
`general/200-hidden-friend-adl-function-call`,
`general/200-hidden-friend-adl`,
`general/200-reverse-friend-plus-operator`, and
`general/300-using-directive-imported-value-ambiguous`.

### Class ABI, value transfers, and special-member storage

`general/200-derived-to-base-by-value-overload`,
`general/200-enum-class-pass-by-value`,
`general/200-out-of-class-special-members`,
`general/200-prvalue-field-access-temporary`,
`general/200-return-nullptr-class-braced-default`,
`general/300-base-rvalue-reference-assignment`,
`general/300-bit-field-copy-semantics`,
`general/300-class-array-missing-value-init`,
`general/300-conditional-local-prvalue-init-elides-copy`,
`general/300-direct-object-parameter-passthrough-base-copy`,
`general/300-discarded-class-call`,
`general/300-discarded-large-class-call-void-cast`,
`general/300-empty-base-defaulted-assignment-user-copy-ctor`,
`general/300-empty-base-subobject-assignment-no-storage`,
`general/300-explicit-default-copy-assignment-with-move-ctor`,
`general/300-indirect-call-result-rvalue-reference-materialization`,
`general/300-leading-trivial-prefix-storage-copy-enum`,
`general/300-leading-trivial-prefix-storage-copy`,
`general/300-move-only-aggregate-brace-member`,
`general/300-qualified-base-implicit-copy-assignment`,
`general/300-return-const-ref-parameter-copy-not-move`, and
`general/300-trivial-copy-value-transfer-storage-copy`.

### Union and anonymous storage

`general/300-anonymous-union-member-id-expression`,
`general/300-local-anonymous-union-sizeof`,
`general/300-union-special-member-storage-copy`,
`general/300-union-trivial-subobject-dtor-omission`, and
`general/300-union-user-default-ctor-inactive-class-member`.

### Conversion operators and built-in/operator ranking

`general/300-pointer-bool-local-storage-lowering`,
`general/300-proxy-subscript-assignment`,
`general/400-built-in-unary-class-conversion`,
`general/400-builtin-comparison-after-rejected-operator-overload`,
`general/400-builtin-comparison-prefers-fewer-user-conversions`,
`general/400-by-value-param-converting-ctor-overload`,
`general/400-c-style-cast-explicit-bool-conversion`,
`general/400-class-pointer-conversion-builtin-eq`,
`general/400-class-pointer-conversion-builtin-subscript`,
`general/400-compound-assignment-rhs-conversion`,
`general/400-condition-declaration-user-bool-conversion`,
`general/400-cv-conversion-operator-builtin-comparison`,
`general/400-direct-init-explicit-bool-conversion`,
`general/400-explicit-conversion-operator-alias-target-call`,
`general/400-logical-not-after-rejected-operator-overload`,
`general/400-out-of-class-qualified-conversion-operator-result`,
`general/400-pointer-arithmetic-class-conversion`,
`general/400-switch-condition-declaration-conversion-operator`,
`general/400-user-defined-conversion-path-tiebreak`,
`general/400-user-defined-conversion-second-rank`, and
`spec/400-inherited-conversion-operator-parameter-binding`.

### Remaining lifetime cleanup

`general/400-for-init-class-lifetime-extends-through-loop` and
`general/400-shadowed-local-cleanup-rebind-on-return` remain as cleanup-state
issues adjacent to, but separate from, the completed dynamic array lifetime
path.

## Next Checkpoint Scope

Target the remaining class ABI/storage group first: unify class result and
parameter materialization with the typed copy/move state, repair empty-base and
leading-trivial-prefix storage copies, and preserve bit-field and enum
representation facts.  Validate that group with the listed class ABI fixtures
while keeping the ADL, union, conversion-ranking, and cleanup groups isolated.

## Checkpoint Scope 8

Complete the class ABI/storage seam in one typed lowering increment:

- use the same materialized-object contract for discarded and member-access
  class results, direct-object parameters, and indirect result destinations;
- treat empty base subobjects as layout-only storage, so aggregate/value
  transfers do not invent bytes or constructors for them;
- lower defaulted value operations field-wise when bit-fields or a trivial
  prefix precedes a nontrivial member, preserving the field representation and
  the required base/member order; and
- preserve enum and narrow-integral value types through pass-by-value and
  return expressions without introducing unrelated conversion/copy nodes.

Validation covers `200-derived-to-base-by-value-overload`,
`200-enum-class-pass-by-value`, `200-prvalue-field-access-temporary`,
`300-direct-object-parameter-passthrough-base-copy`, the discarded-class-call
fixtures, the empty-base assignment fixtures, `300-bit-field-copy-semantics`,
the leading-trivial-prefix copy fixtures, and
`300-trivial-copy-value-transfer-storage-copy`.  ADL, unions, conversion
ranking, and cleanup remain outside this checkpoint.

## Checkpoint Scope 9

Close the union and anonymous-storage aggregate seam using the existing typed
class-member facts:

- record anonymous union class-specifiers as real, unnamed storage-bearing
  members while keeping their injected names available for lookup;
- resolve local `sizeof` operands through the typed local object plan;
- initialize only the active first union member for an empty aggregate and
  avoid constructing inactive class members; and
- preserve the union's indirect result ABI and direct integral stores through
  union return, assignment, and copy paths.

Validate `300-anonymous-union-member-id-expression`,
`300-local-anonymous-union-sizeof`, `300-union-special-member-storage-copy`,
`300-union-trivial-subobject-dtor-omission`, and
`300-union-user-default-ctor-inactive-class-member`; the remaining ADL,
conversion-ranking, class-transfer, and cleanup groups stay separate.

## Checkpoint Scope 10

Repair the remaining class-transfer and special-member ABI shapes that share
the value-category/address machinery:

- allow derived xvalues to bind to base rvalue-reference parameters;
- avoid materializing discarded reference-return calls and keep const-reference
  returns on the copy-constructor path;
- emit required out-of-class constructor/destructor base-entry definitions;
- omit no-op union subobject construction from enclosing default constructors;
- apply the normal class ABI classifier to synthesized aggregate constructors;
  and
- recognize constructor calls inside explicit rvalue-reference casts without
  treating them as callable functor expressions.

Validate `200-derived-to-base-by-value-overload`,
`200-out-of-class-special-members`, `300-base-rvalue-reference-assignment`,
`300-const-return-copy-assignment-suppresses-implicit`,
`300-leading-trivial-prefix-storage-copy`,
`300-leading-trivial-prefix-storage-copy-enum`,
`300-move-only-aggregate-brace-member`,
`300-qualified-base-implicit-copy-assignment`,
`300-return-const-ref-parameter-copy-not-move`, and
`300-rvalue-reference-cast-value-init-temporary`.

### Checkpoint Scope 10 result

Complete.  The class-transfer increment raised the PA16 result from 122/164
to 130/164 before the qualified-base repair, and then to 132/164; all nine
listed fixtures now pass, including the move-only aggregate and qualified
base-assignment cases.  The synthesized aggregate constructor now follows
the typed ABI classifier, derived xvalues reach base rvalue references, and
out-of-class special-member emission has the required entry points.

## Remaining Work Map

The current PA16 run is 138/164, with 26 failures grouped as follows:

- Conversion selection and built-in lowering: the class-to-scalar and
  scalar-to-class conversion paths, explicit conversion operators, pointer
  conversions, condition conversions, and user-defined overload ranking in
  `300-pointer-bool-local-storage-lowering`, `300-proxy-subscript-assignment`,
  the `400-*conversion*`/`400-*comparison*`/`400-*operator*` fixtures, and
  `spec/400-inherited-conversion-operator-parameter-binding`.
- Class temporaries and cleanup: conditional local prvalues, nullptr/default
  construction, proxy result materialization, shadowed-local cleanup, loop
  lifetime extension, and the remaining nontrivial defaulted move fixture.
  These are the seven `200-return-nullptr-*`, `200-reverse-friend-*`,
  `300-conditional-*`, `300-shadowed-*`, `400-for-init-*`,
  `400-shadowed-*`, and `spec/100-defaulted-move-*` cases.

Ordinary ADL, hidden-friend ADL, using-directive ambiguity, and the qualified
base assignment lookup seam are now closed; later failures in
`200-reverse-friend-plus-operator` are output-shape/lifetime failures rather
than unresolved lookup.

## Checkpoint Scope 11

Complete the ordinary-call lookup seam with typed associated-name facts:

- add associated namespaces and hidden friends to ordinary unqualified calls;
- retain ambiguity when distinct using-directive paths reach the same binding;
- do not treat using-declaration imports as declarations from the associated
  namespace during ADL; and
- keep qualified base-member lookup and operator ADL behavior unchanged.

Validate `200-associated-namespace-adl-function-call`,
`200-adl-ignores-using-declaration-candidate`,
`200-hidden-friend-adl-function-call`, `200-hidden-friend-adl`,
`200-reverse-friend-plus-operator`, and
`300-using-directive-imported-value-ambiguous` for exit status and LowIR.

### Checkpoint Scope 11 result

Complete.  The ordinary ADL group and the using-directive ambiguity fixture
now pass; the PA16 result is 138/164.  The remaining reverse-friend failure
is a class temporary/output-shape issue and stays in the next map.

## Checkpoint Scope 12

Repair the shared conversion-ranking and conversion-operator lowering path:

- collect eligible conversion operators through aliases, inherited members,
  and out-of-class definitions;
- distinguish explicit-context conversions from implicit overload viability;
- rank built-in candidates against rejected or user-defined operator paths by
  standard conversion sequence quality; and
- preserve the typed pointer, boolean, cv, and class-value representation
  while emitting the selected conversion.

Validate the complete remaining conversion group listed in the map, including
the pointer/bool, explicit-bool, built-in comparison/unary, pointer
arithmetic/subscript, compound-assignment, condition/switch, and
user-defined-conversion ranking fixtures.  Class cleanup and temporary
materialization remain outside this checkpoint.

## Final checkpoint result

Complete.  The final PA16 increment closed the remaining six failures and
raised the current-PA result from 158/164 to 164/164.  It now covers
nontrivial conditional local-prvalue materialization with copy/destructor
lifetime, class-valued proxy member-call temporaries, constructor-success
cleanup accounting, constructor base-entry ABI generation, source-order
pointer arithmetic with class conversion operands, and deterministic
const-qualified conversion-operator emission order.

## Remaining Work Map

No PA16-local work remains: `make test-report ACTIVE_TEST_REPORT_PAS='pa16'`
passes all 164 tests.  The earlier groups are
retained above as the implementation history; they are all covered by the
passing PA16 report.

## Checkpoint Scope

The completed scope was the whole PA16 stage.  Validation included the full
PA16 local suite, the required root PA16 report, the through-PA15 regression
report, and the PA16 source file audit.  The next checkpoint group is PA17
according to the staged assignment sequence.

## Audit-structure checkpoint result

Complete.  The green PA16 implementation was reorganized into responsibility-
named lowering modules for driver orchestration, call lowering, and function
emission; class-member declaration recording and subscript inference were also
given focused helpers.  The source set was updated accordingly, all 164 PA16
tests and all 1,019 through-PA15 tests still pass, and the file audit passes
with only its existing nonfatal header-division warnings.

## Remaining Work Map

No PA16-local implementation or audit work remains.  Continue with PA17 in the
staged assignment sequence.

## Next Checkpoint Group

PA17 implementation and its assignment-specific tests, preserving the
through-PA16 green baseline.
