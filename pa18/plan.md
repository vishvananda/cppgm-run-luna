# PA18 checkpoint plan

## Baseline and remaining-work map

The turn-start report contained 222 PA18 tests: 16 passed and 206 failed.
There were 204 exit-status failures and two successful-process LowIR mismatches:
`general/200-variable-template-defaulted-partial` and
`general/300-template-body-enum-adl-call`.  The failure set is grouped by the
shared compiler behavior it needs, rather than by the test directory.

### Group A — template-id materialization and basic substitution (8 direct
status failures, plus the simple class/function cases in Groups B and C)

The compiler has parser support and temporary template-parameter scopes, but
does not turn a supported type template argument into a typed class/function
specialization.  This group includes the forward-only/normalization and
trailing-return edges:

`100-forward-template-alias-no-output-shell`,
`100-forward-template-typedef-no-output-shell`,
`100-functional-cast-argument-nested-type-hides-function`,
`100-template-auto-trailing-return`,
`100-template-forward-definition-parameter-rename` (general and spec),
`100-template-move-only-byvalue-argument-does-not-copy`, and
`100-typedef-template-argument-witness-normalization`.

### Group B — ordinary class-template specialization and object lowering (63
status failures)

These require a concrete substituted class type to flow through PA11 layout,
PA14 constructors/value operations/member lookup, and LowIR demand collection:

`100-alias-base-member-type-forward-upgrade`,
`100-alias-qualified-member-type-forward-upgrade`,
`100-class-template-alias-array-member`, `100-class-template-field`,
`100-class-template-member-plus-calls-later-plus-assign`,
`100-class-template-method`, `100-const-reference-member-forward-xvalue-init`,
`100-cv-qualified-qualified-type-template-arg`,
`100-declared-byvalue-class-parameter-abi`,
`100-decltype-qualified-type-template-argument`,
`100-defaulted-copy-constructor-reference-member`,
`100-defaulted-move-constructor-reference-member`,
`100-elaborated-template-argument-enclosing-scope`,
`100-forwarding-static-cast`, `100-inherited-constructor-using-alias-template`,
`100-local-alias-shadows-template-parameter-bad`,
`100-local-call-prefers-member-over-template-type-declaration`,
`100-local-type-alias-noop`, `100-member-access-completes-returned-template`,
`100-nested-class-specialization-symbols`,
`100-nested-class-template-local-class-argument`,
`100-new-expression-completes-template-layout`,
`100-nonprimary-embedded-typedef-reprepare`,
`100-qualified-template-id-pointer-argument`,
`100-qualified-value-does-not-shadow-class-template`,
`100-reentrant-full-collection-abort-layout`,
`100-required-template-special-member-emission`,
`100-returned-class-template-prvalue-field-access`,
`100-returned-class-template-prvalue-method-call`,
`100-rvalue-reference-member-forward-init`,
`100-rvalue-reference-return-slot-init-move`,
`100-sizeof-class-with-deleted-default-ctor-member`,
`100-skip-placeholder-specialization-output`,
`100-specialization-signed-builtin-types`,
`100-static-member-function-object-access-pointer`,
`100-template-specialization-member-object-access`,
`100-unused-nested-class-instantiation`,
`100-using-directive-inline-namespace-class-template`,
`200-default-template-arg-const-pointer-alias` (general and spec),
`200-defaulted-template-arg-base-initializer-match`,
`200-defaulted-template-arg-base-reference-chain`,
`200-forward-declared-class-template-default-merge`,
`200-template-alignas-gnu-alignof-instantiation`,
`300-class-template-alias-out-of-class-ctor`,
`300-class-template-default-arg-preserves-template-scope`,
`300-class-template-default-cache-isolation`,
`300-class-template-static-member-assignment-lvalue`,
`300-class-template-static-member-class-lifetime`,
`300-class-template-static-member-out-of-class-definition`,
`300-current-class-template-id-using-directive-template-arg`,
`300-default-template-argument-merge`,
`300-nested-class-template-special-member-definition`,
`300-out-of-class-member-alias-return-signature`,
`300-out-of-class-member-nested-trailing-return`,
`300-out-of-class-nested-member-class-definition`,
`300-out-of-class-nested-template-owner-constructor`,
`300-qualified-array-type-template-argument`,
`300-qualified-nested-class-template-definition`,
`300-reference-member-alias-visible`,
`300-reference-member-same-template-name`,
`300-reference-member-static-constant-visible`,
`300-reference-shell-nested-class-reuse`, and
`300-using-alias-forward-template-member-layout`.

### Group C — function-template instantiation, deduction, overloads, and
template-backed operators (50 status failures)

These share function-template specialization records, type-parameter
substitution in function signatures/bodies, default function arguments,
ordinary argument deduction, and participation in the existing PA12 overload
selection/ADL machinery:

`100-class-template-member-plus-calls-later-plus-assign`,
`100-conversion-completes-template`,
`100-conversion-operator-qualified-template-result-call`,
`100-enum-builtin-bitand-beats-class-wrapper-operator`,
`100-enum-operator-template-fallback-to-builtin`,
`100-forward-only-member-operator-overload`,
`100-function-reference-template-parameter`,
`100-function-template-argument-uses-free-operator-result`,
`100-function-template-const-by-value-parameter-call`,
`100-function-template-explicit-specialization-address`,
`100-function-template-explicit`,
`100-function-template-pair-vs-range-predicate`,
`100-function-template-parameter-decltype-ref-array`,
`100-function-template-result-member-trailing-return`,
`100-function-template-returns-aggregate-class-template`,
`100-function-template-returns-constructed-class-template`,
`100-inherited-typedef-hidden-friend-overload`,
`100-local-type-cross-namespace-operator-template`,
`100-member-cv-overload-deduction-argument`,
`100-member-operator-target-deduced-function-template`,
`100-nonconst-subscript-deduces-mutable-pointer`,
`100-partial-explicit-function-template-id-call`,
`100-qualified-function-template-member-overload-argument`,
`100-qualified-using-directive-function-template-call`,
`100-target-function-template-id-two-type-params`,
`100-template-argument-adl-call`,
`100-template-function-pointer-rvalue-argument-emits-move`,
`100-template-operator-shift-stress-chain`,
`100-template-operator-shift-two-step`,
`100-template-overload-fallback`,
`100-using-base-cv-overload-deduces-mutable-ref`,
`100-using-namespace-ambiguous-less-than-or`,
`200-function-template-default-parameter-instantiation`,
`200-function-template-empty-template-id-reference-param`,
`300-member-rvalue-subscript-overload-binding`,
`100-conversion-operator-qualified-template-result` (spec),
`100-function-template-cv-reference-extra-qualifier`,
`100-function-template-member-array-reference-return`,
`100-inherited-class-template-conversion-operator`,
`100-member-call-ignores-enclosing-template-distractor`,
`100-overload-set-explicit-function-template-id`,
`100-overloaded-function-address-context`,
`100-repeated-implicit-function-template-call`,
`100-target-function-template-signature-match`,
`100-template-logical-operator-call-argument`,
`100-template-vs-nontemplate-overload`, and
`100-using-declaration-operator-template-adl`.

### Group D — dependent/current-instantiation lookup and scope timing (79
status failures)

These need a preserved template environment and deferred lookup/substitution
for qualified names, `typename`/`template`, using declarations/directives,
current instantiation, namespace reopening, and point-of-instantiation ADL:

`100-inline-namespace-function-template-parameter`,
`100-lazy-header-qualified-function-template-text-lookup`,
`100-lazy-header-qualified-template-id-shadowed-by-local`,
`100-local-using-declaration-hides-using-directive-template`,
`100-local-using-directive-qualified-template-argument`,
`100-namespace-qualified-class-template-definition`,
`100-namespace-template-class`,
`100-namespace-template-forward-definition-parameter-rename`,
`100-namespace-template-function-before-tls-object`,
`100-namespace-template-function`,
`100-nondependent-template-member-body-lookup-bad`,
`100-qualified-unnamed-namespace-template-arg`,
`100-reopened-namespace-function-template-parameter-scope`,
`100-template-local-alias-postfix-cv-declaration`,
`100-template-local-anonymous-union-nested-struct`,
`100-qualified-alias-function-result-recovery`,
`100-qualified-function-template-id-parenthesized-initializer`,
`100-qualified-template-id-ignores-local-value-shadow`,
`100-local-constref-converting-iterator`,
`100-local-using-template-specialization-does-not-suppress-adl`,
`100-nondependent-name-binding`,
`100-namespace-template-forward-definition-parameter-rename` (spec),
`300-current-instantiation-qualified-type-declaration`,
`300-current-member-alias-defers-later-typedef`,
`300-current-specialization-nested-constructor-param-alias`,
`300-current-specialization-nested-member-base-alias`,
`300-dependent-adl-class-member-parameter`,
`300-dependent-adl-member-call-argument`,
`300-dependent-adl-point-of-instantiation`,
`300-dependent-base-member-this`,
`300-dependent-base-using-member-function`,
`300-dependent-base-using-overload`,
`300-dependent-base-using-typename-type-and-value`,
`300-dependent-base-using-typename-type`,
`300-dependent-bit-field-allocator-traits-placeholder-instantiation`,
`300-dependent-bit-field-base-type-placeholder-instantiation`,
`300-dependent-class-template-shape`,
`300-dependent-decltype-comma-expression`,
`300-dependent-decltype-function-pointer-reference-call`,
`300-dependent-default-construction-through-template-subscript`,
`300-dependent-enum-sizeof-nested-class`,
`300-dependent-functional-template-id-hides-outer-function`,
`300-dependent-member-access-assignment`,
`300-dependent-qualified-return-same-name-function`,
`300-dependent-qualified-value-base-member`,
`300-dependent-template-keyword-value-name-collision`,
`300-dependent-typename-template-argument-local-init`,
`300-dependent-typename-template-argument-return`,
`300-explicit-type-arg-dependent-template-cv-suffix`,
`300-inline-namespace-qualified-decltype-lookup`,
`300-later-redeclaration-default-template-argument`,
`300-lazy-header-parenthesized-qualified-function-template-call`,
`300-lazy-header-parenthesized-qualified-function-template-id-call`,
`300-local-using-alias-template-member-body`,
`300-local-using-directive-template-member-enumerator`,
`300-local-using-inline-namespace-function-template`,
`300-member-typedef-qualifier-hides-outer-namespace`,
`300-out-of-class-template-member-inherited-typedef-param`,
`300-qualified-dependent-base-implicit-assignment`,
`300-qualified-explicit-class-instantiation`,
`300-qualified-template-member-type-class-scope-argument`,
`300-unnamed-namespace-qualified-class-template-id`,
`300-unqualified-call-skips-dependent-base`,
`300-reused-template-body-qualified-member-type-arg`,
`300-template-body-inherited-enumerator`,
`300-template-body-typedef-enum-member-lookup`,
`300-forward-owner-default-member-type`,
`300-out-of-class-template-member-nested-enum-param`,
`300-reference-member-same-template-name`,
`300-using-alias-forward-template-member-layout` (also Group B),
`300-template-static-object-member-definition`, and
`300-template-static-member-storage-definition-preserves-inclass-initializer`.

The deliberately overlapping names above are shared-behavior witnesses; the
union is the complete current failure set.  The remaining expected-failure
checks are `general/100-local-alias-shadows-template-parameter-bad`,
`general/100-nondependent-template-member-body-lookup-bad`, and
`general/300-out-of-class-special-member-noexcept-mismatch-bad`.  The two
LowIR-only failures are recorded at the top and remain part of the final
normalization/ADL cleanup group.

## Checkpoint scope

Implement the first substantial PA18 increment: a typed, demand-driven
materialization layer for supported type-only template arguments.  The
checkpoint will:

1. Preserve template definitions and parameter/default metadata in compiler
   state, including namespace/class ownership and declaration order.
2. Resolve explicit class/function template-ids with supported type
   arguments, apply defaults (including defaults referring to earlier type
   parameters), and reject unsupported/non-type forms without corrupting the
   surrounding scope.
3. Clone and substitute the supported template declaration/body AST into an
   ordinary instantiated declaration, while keeping dependent type/value
   facts typed through the existing PA11 `Type`, `Scope`, and `Binding` model.
4. Feed instantiated class templates through existing PA15-PA17 layout,
   constructor, member, static/global, and polymorphic lowering, and feed
   instantiated function templates through existing PA12 call resolution and
   PA14 LowIR emission.
5. Support direct explicit function-template calls and the first direct
   deduction case where an ordinary argument type matches a type parameter;
   retain non-template overloads and ordinary PA17 behavior unchanged.

Validation for this checkpoint is the focused explicit class/function/default
subset plus the full PA18 report, prior-through-PA17 report, and file audit.
The next checkpoint group is the remaining function-template deduction and
operator/conversion overload participation, followed by deferred dependent
lookup/current-instantiation and the more complex out-of-class/static/nested
member cases.  The three negative semantic tests and the two LowIR-only
fixtures remain explicit cleanup targets.

## Checkpoint result

The completed checkpoint raises PA18 from 16/222 to 160/222. It preserves lexical
and logical namespace ownership, materializes qualified/reopened/inline namespace
specializations, resolves aliases/defaults/qualified arrays, and orders generated
nested/current-specialization classes before their users. It also preserves
function-type aliases and parameter names, handles explicit function-template
arguments and signatures, keeps local declaration scopes available during lowering,
and lowers derived-to-base class reference casts without recursively constructing a
new base object.

### Remaining Work Map

The final PA18 report has 62 failures: 3 exit-status mismatches, 58 LowIR
canonicalization mismatches, and 1 LowIR sanity failure.

- Exit-status cleanup: the three expected-negative fixtures for local alias
  shadowing, nondependent template-member lookup, and out-of-class special-member
  `noexcept` mismatch still return success.
- LowIR normalization and demand collection: 58 fixtures still differ in
  generated-member/static storage and constructor emission, function/operator/
  conversion overload selection, dependent `decltype`/array/reference lowering,
  namespace-qualified ordering, and ADL/using-declaration lowering.
- LowIR sanity: the inherited-typedef hidden-friend operator fixture still emits
  an unresolved `ns__operator` call target.

The remaining cases are grouped by shared compiler behavior rather than fixture
order; process-status validation and LowIR canonicalization are tracked separately.

### Checkpoint Scope

This turn completed the namespace/alias/current-instantiation and function-signature
increment: typed owner paths are collected before rewriting, source namespace
forwards are emitted before generated dependent classes, nested dependencies are
materialized in order, alias/`decltype`/function-type spellings are canonicalized
without losing declarator structure, ordinary local aliases are retained for
lowering, and function bodies use their compound-statement scope. Validation covers
the focused semantic witnesses, the full PA18 report (160/222), the PA18 file audit,
and the through-PA17 report (1206/1208; the two PA15 shift-stress cases still time
out at the repository's 10-second baseline). The next checkpoint group is the
operator/ADL and generated-member LowIR groups, followed by deferred dependent
lookup/current-instantiation and the three negative semantic diagnostics.
