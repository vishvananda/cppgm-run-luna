# PA15 implementation plan

## Baseline

The turn-start PA15 report was **16 / 200** tests.  The landed class-layout
checkpoint raised it to **26 / 200** while earlier assignments and the source
file audit remained clean.  The remaining PA15 failures are real object-model
gaps rather than driver stubs.

## Remaining Work Map

The complete current-PA failure set is grouped below by shared compiler
behavior.  The 20 LowIR mismatches are listed separately from the 154
exit-status failures so every one of the 174 failures is accounted for.

1. **Class layout, object storage, and aggregate LowIR shape** — output
   mismatches `100-global-reference-incomplete-referent`,
   `200-comma-class-lvalue-reference-init`, `200-derived-pointer-member-init`,
   `200-external-ctor-overload-nonfirst-argument`, `200-global-constructor`,
   `200-global-scalar-dynamic-init`, `200-local-default-class-array-lifecycle`,
   `200-return-preserves-value`, `200-static-thread-local-member`,
   `400-bit-field-constructor-member-init`, and
   `400-bit-field-sparse-member-init`.  The same family has status
   failures `100-default-member-initializer-aggregate-member`,
   `100-default-member-initializer-class-member`,
   `100-default-member-initializer-scalar-brace`,
   `100-default-member-initializer-scalar`,
   `100-default-member-initializer-user-ctor`,
   `100-defaulted-constructor-default-member-initializer`,
   `200-aggregate-array-member-brace-elision`,
   `200-aggregate-class-member-subobject-init-target`,
   `200-aggregate-reference-member-binds-storage`,
   `200-global-class-array-enum-trivial-dtor`, `200-global-class-array-init`,
   `200-in-class-member-initializer`, `200-local-struct-array-init`,
   `200-member-initializer-aggregate-member`,
   `200-member-initializer-overrides-default-member-initializer`,
   `200-pointer-member-zero-brace-init`, `200-pointer-member-zero-paren-init`,
   `200-reference-member-class-init`, `300-array-member-empty-paren-value-init`,
   `300-value-init-aggregate-with-nontrivial-member`,
   `400-bit-field-member-access-bad`, `400-bitfield-aggregate-init`, and
   spec `200-aggregate-brace-elision`.

2. **Member collection, `this`, member lookup, and non-static/static method
   lowering** — output mismatches `100-qualified-typedef-cstyle-cast-same-name-operand`,
   `100-this-arrow-member-binds-lvalue-ref`, `300-member-operator-bang-out-of-class`,
   and `300-friend-function-definition-skip`; status failures
   `100-member-methods`, `100-object-member-enumerator-constant`,
   `100-out-of-class-methods`, `100-qualified-const-method-definition`,
   `100-qualified-typedef-const-method-definition`,
   `100-static-member-object-access`,
   `100-static-member-overload-skips-nonstatic-this`,
   `100-static-member-qualified-call`, `100-using-directive-imported-value-method-body`,
   `200-const-member-call-prefers-const-object-overload`,
   `200-const-subobject-member-call`, `200-derived-method-hides-base-field-call`,
   `200-function-reference-return-expression-type`,
   `200-implicit-member-call-suppresses-adl`, `200-member-call-implicit-object-cv-overload`,
   `200-member-call-implicit-this-cv-overload`,
   `200-member-call-return-type-overload-arity`,
   `200-member-function-default-arguments`, `200-member-pointer-const-typedef-return`,
   `200-method-cv-overload-preference`, `200-mutable-member-const-method`,
   `200-nested-class-member-object-access`, `200-out-of-class-getter-only`,
   `200-out-of-class-member-default-argument`,
   `200-out-of-line-member-inherited-typedef-body`,
   `200-parenthesized-member-call`, `200-pointer-subscript-class-reference-return`,
   `200-protected-base-method`, `200-simple-class-member-object-access`,
   `200-static-nonstatic-same-pointer-signature`,
   `200-static-thread-local-member-object-call`,
   `300-const-method-array-member-binds-const-reference`,
   `300-member-callable-field-call`, `300-member-function-pointer-field-call`,
   and `300-static-const-member-address`.

3. **Constructors, destructors, recursive lifetime, and initialization
   actions** — output mismatches `200-friend-noexcept-redeclaration` and
   `200-member-object-lifetime`; status failures
   `200-aliased-base-mem-initializer-match`,
   `200-bad-implicit-default-ctor-with-nondefault-member`,
   `200-base-default-argument-constructor-action`, `200-constructor-member-init`,
   `200-constructor-overload-default-arg-nonfirst-argument`,
   `200-defaulted-constructor-still-aggregate`, `200-deleted-constructor-still-aggregate`,
   `200-derived-base-constructor-member-init`, `200-empty-class-member-declaration`,
   `200-global-function-style-constructor`,
   `200-local-class-direct-init-free-function`,
   `200-local-class-direct-init-inherited-member-call`,
   `200-local-class-direct-init-member-function`,
   `200-local-class-direct-init-parameter-hides-type`,
   `200-nested-out-of-class-constructor-enclosing-type`,
   `200-placement-new-expression-constructor-call`,
   `300-const-pointer-explicit-destructor-call`,
   `300-explicit-destructor-call-enclosing-namespace-type`,
   `300-header-static-class-init`, `300-scalar-pseudo-destructor-call`,
   `300-static-class-member-object-definition`,
   `300-synthesized-array-member-lifecycle`, `500-inheriting-constructors`,
   `500-inheriting-external-transitive-constructor`, and spec
   `200-direct-list-init-explicit-ctor`.

4. **Inheritance, access control, nested-name lookup, references, and
   conversions** — status failures
   `200-base-field-access`, `200-const-cast-pointer-reference-alias`,
   `200-derived-pointer-overload-prefers-base-over-void`,
   `200-elaborated-member-forward-type`,
   `200-friend-derived-access-inherited-protected-field`,
   `200-friend-function-member-access`,
   `200-friend-intermediate-derived-protected-base-method`,
   `200-inherited-base-typedefs-in-derived-members`,
   `200-inherited-injected-class-name-qualified-type`,
   `200-inherited-member-overload-set`, `200-inherited-static-member-qualified-call`,
   `200-multilevel-qualification-conversion-bad`,
   `200-nested-injected-class-name-hides-base-name`,
   `200-protected-member-typedef-access-bad`,
   `200-qualified-friend-function-member-access`,
   `200-qualified-inherited-member-typedef`,
   `200-reference-member-conditional-lvalue`, `200-single-inheritance`,
   `300-class-using-declaration-reexposes-protected-field`,
   `300-lazy-nested-class-enclosing-alias-lookup`,
   `300-private-base-using-method-call`,
   `300-prvalue-derived-base-friend-operator`,
   `300-qualified-friend-function-access`,
   `300-reference-member-same-name-as-class`,
   `300-using-base-same-signature-derived-preferred`,
   `300-using-declaration-public-private-base-member`, and spec failures
   `200-conditional-derived-base-lvalue-reference`,
   `200-const-reference-binds-derived-pointer-prvalue`,
   `200-derived-base-reference-overload-rank`,
   `200-nested-class-enclosing-access`.

5. **Operator overload resolution, ADL, and callable/member expression
   forms** — output mismatches `300-enum-class-nonmember-operator-bitand`,
   `300-enum-operator-adl-selects-matching-overload`, and
   `spec/300-operator-lookup-ordinary-adl-union`; status failures
   `300-adl-associated-namespace-does-not-climb-parents`,
   `300-basic-operator-overloads`, `300-chained-member-subscript-operator-call`,
   `300-compound-assignment-adl-nonmember-after-member-reject`,
   `300-derived-shift-prefers-free-char-pointer`,
   `300-discarded-comma-reference-result-no-copy`,
   `300-hidden-friend-definition-adl-call`,
   `300-hidden-friend-operator-nullptr-compare`,
   `300-late-member-subscript-shadows-type`, `300-member-binary-operator-eq`,
   `300-member-binary-operator-ne-wrapper`, `300-member-deref-after-prefix-decrement`,
   `300-member-postfix-increment-operator`, `300-member-prefix-decrement`,
   `300-member-subscript-operator-call`, `300-mixed-member-free-shift-stress-chain`,
   `300-operator-nullptr-t-from-zero`, `300-operator-shift-stress-chain`,
   `300-operator-token-result-typing`, `300-out-of-class-member-trailing-return`,
   `300-overloaded-arrow-star-operator`, `300-overloaded-deref-user-assignment`,
   `300-overloaded-unary-deref-base-ref-return`,
   `300-postfix-ref-return-deref-member-call`, `300-stream-shift-selection-chain`,
   `300-subobject-member-deref-after-prefix-decrement`, `300-temporary-functor-call`,
   `300-unary-address-of-builtin-fallback`, `300-user-defined-string-literal-operator`,
   and spec failures `300-logical-operator-overload` and
   `300-overloaded-comma-nonviable-falls-back-builtin`.

6. **Remaining parser/diagnostic and LowIR metadata gaps** — status failures
   `100-function-pointer-nested-param-name-shadow`,
   `200-function-boundary-metadata-emission`, `200-parameter-access-metadata-emission`,
   `200-parameter-alias-metadata-emission`, and
   `300-alignas-out-of-class-nested-type`, `300-member-function-trailing-return`;
   plus the rejection/acceptance cases
   `200-copy-init-explicit-ctor-bad`, `200-copy-init-explicit-ctor-overload-refinement`,
   `200-copy-list-init-explicit-ctor-bad`, `200-list-init-narrowing-bad`,
   `300-compound-assignment-adl-nonmember-after-member-reject`, and
   `300-user-defined-string-literal-operator`.

## Checkpoint Scope

This checkpoint implements the typed class-layout service and its direct
LowIR consumers.  It covers ordinary non-static fields in declaration order,
pointer/reference/array and previously completed class-member sizes, empty
classes, self-referential pointer members, the direct single base at offset
zero, integral bit-field allocation units including unnamed zero-width
separators, explicit class alignment, and the resulting `sizeof`/`alignof`
constant evaluation.  Alignment arguments are retained as typed AST
expressions, so constant-expression and type-id forms reach semantic
evaluation without reparsing source spelling.  Class-member bindings link to
their canonical typed member records, and the lowerer rejects incomplete or
in-progress class layouts instead of manufacturing fallback sizes.  It also
changes zero-initialized class globals to the structured `global { zero <size> }`
form and prevents PA14's scalar zero store from pretending to initialize a
trivial class object.  The implementation preserves all earlier procedural
behavior and keeps member/lifetime lowering as the next checkpoint rather than
baking source-test answers into the emitter.

Validation for this checkpoint is the focused layout/global subset, the full
PA15 report, the through-PA14 report, and the PA15 source-file audit.  The
next checkpoint group is member collection and field/member-function access;
after that, constructor/destructor actions and then inheritance/access and
operator/ADL resolution remain as grouped above.

## Checkpoint Result

Completed.  The semantic model now retains typed class-member and layout facts;
the analyzer computes declaration-order offsets, base and bit-field storage,
alignment, and final object size; and the PA14 lowerer consumes those facts for
object slots, `sizeof`/`alignof`, structured zero-initialized class globals, and
trivial local class declarations.  The parser preserves standard `alignas`
arguments as typed AST facts while leaving the earlier vendor-dependent
`__alignof` syntax syntactic-only, and invalid incomplete/recursive layouts
now fail at the semantic boundary.

The focused checkpoint set passed **9 / 9**:

- `100-class-local-sizeof`
- `100-global-class-zero`
- `100-empty-class-sizeof`
- `100-large-class-local`
- `100-large-global-class-zero`
- `100-self-pointer-layout`
- `300-alignas-class-layout`
- `300-bit-field-layout-sizeof`
- `300-zero-width-bit-field-layout`

The full required PA15 report improved from **16 / 200** to **26 / 200**.
No test or reference fixture was changed.  The required through-PA14 report
is clean at **819 / 819**.  The PA15 source audit passes with only the
repository's existing header-division warnings.

The Ralph loop 31 checkpoint audit found two blockers and fixed both before
handoff: PA3's per-line `std::endl` flushes could time out the required
`300-triple.t` workload, and class bindings used raw pointers into the
relocatable member vector.  Output now uses buffered newlines, and bindings
refer to the canonical member facts through a stable owner/index pair.  The
post-fix PA15 report remains **26 / 200**, above its **16 / 200** turn-start
baseline, and the next substantial checkpoint is unchanged.

## Post-checkpoint Remaining Work

The current report has **174 failures**: 154 exit-status mismatches and 20
LowIR mismatches.  The remaining LowIR set is:

- `100-global-reference-incomplete-referent`,
  `100-qualified-typedef-cstyle-cast-same-name-operand`,
  `100-this-arrow-member-binds-lvalue-ref`;
- `200-comma-class-lvalue-reference-init`,
  `200-derived-pointer-member-init`,
  `200-external-ctor-overload-nonfirst-argument`,
  `200-friend-noexcept-redeclaration`, `200-global-constructor`,
  `200-global-scalar-dynamic-init`,
  `200-local-default-class-array-lifecycle`,
  `200-member-object-lifetime`, `200-return-preserves-value`,
  `200-static-thread-local-member`;
- `300-enum-class-nonmember-operator-bitand`,
  `300-enum-operator-adl-selects-matching-overload`,
  `300-friend-function-definition-skip`,
  `300-member-operator-bang-out-of-class`;
- `400-bit-field-constructor-member-init`,
  `400-bit-field-sparse-member-init`, and
  `spec/300-operator-lookup-ordinary-adl-union`.

The status failures remain grouped by the shared behaviors in the turn-start
map above: aggregate/default-member initialization and lifetime; member
collection and access; constructors/destructors; inheritance and conversions;
operator/ADL lookup; and parser/diagnostic metadata.  The next checkpoint is
the member-collection slice: bind ordinary and static members with their
typed offsets/access facts, lower `this` plus `.`/`->` field access, and start
emitting member-function declarations while preserving the completed layout
behavior.

## Turn 32 Checkpoint Scope

The complete failure map above remains the authoritative grouping for this
checkpoint.  This increment targets the member-collection group: extend the
typed semantic bindings so class fields, static fields, and ordinary member
functions retain their owning class, access, storage kind, and declaration;
collect in-class and qualified out-of-class member definitions; lower `this`,
direct `.` and `->` field lvalues using the completed layout offsets; and lower
ordinary non-static member calls with an explicit hidden object pointer.  It
also covers static member lookup/calls and cv-qualified overload selection
based on the object expression.  The validation target is the direct-member
and method subset of `tests/general`, plus the full PA15 local report, the
through-PA14 report, and the source-file audit.  Constructors, recursive
lifetime, inherited lookup, access diagnostics, and operator/ADL resolution
remain grouped for subsequent checkpoints unless this implementation exposes
them as required shared dependencies.

## Turn 32 Checkpoint Result

Completed.  The typed member facts are now carried from the PA11 semantic
model into lowering: ordinary and static fields and member functions retain
their owner, access, storage kind, declaration, and layout index; in-class
and qualified out-of-class definitions are collected; `this`, implicit
member lookup, `.`/`->` field lvalues, static access, base-subobject address
adjustment, and hidden-object member calls are lowered.  Member overload
selection accounts for the object's cv-qualification, and demand-driven
member-function emission avoids emitting unused wrappers.

The member collection work exposed the lifetime dependencies needed by this
slice, so this checkpoint also includes typed constructor/destructor records,
implicit special-member synthesis for supported subobjects, constructor
mem-initializers/default member initializers, local object initialization,
reverse-order local finalization, and recursive base/member destruction.

The focused validation set passed, including:

- `100-member-methods`, `100-out-of-class-methods`,
  `100-qualified-const-method-definition`, and
  `100-qualified-typedef-const-method-definition`;
- `100-static-member-object-access`, `200-simple-class-member-object-access`,
  `200-base-field-access`, and `200-protected-base-method`;
- `200-member-call-implicit-object-cv-overload`,
  `200-const-member-call-prefers-const-object-overload`, and
  `200-const-subobject-member-call`;
- `200-constructor-member-init`,
  `200-constructor-overload-default-arg-nonfirst-argument`,
  `100-default-member-initializer-class-member`,
  `100-default-member-initializer-user-ctor`,
  `200-local-class-direct-init-member-function`, and
  `200-member-object-lifetime`.

The required current-PA report is **46 / 200**, above the turn-start
baseline of **26 / 200**.  The through-PA14 report remains clean at
**819 / 819**, `git diff --check` is clean, and the PA15 source audit passes
with only the repository's three existing header-division warnings.  No
tests or reference fixtures were changed.

## Turn 32 Remaining Work Map and Next Checkpoint

The current report has **154 failures**.  They group into these shared
behaviors:

- aggregate, reference, array, bit-field, and default-member initialization,
  including ordering and nontrivial subobject cases;
- namespace-scope and static/thread-local object lifetime, dynamic startup,
  and global constructor/destructor helper emission;
- constructor policy details still missing for deleted/explicit/inherited
  constructors and several derived/base conversion paths;
- access, friend, nested-class, using-declaration, inherited lookup, and
  typedef resolution diagnostics;
- ordinary operator overload, ADL, callable-field, pointer/reference
  conversion, and pseudo-destructor lowering;
- trailing-return, `noexcept`, declaration metadata, and other parser/type
  boundary cases.

The next checkpoint covers the first two groups: complete aggregate and
default-member initializer lowering for arrays, references, bit-fields, and
nontrivial members, then connect the same typed plans to namespace-scope
dynamic initialization and finalization.  Its validation is the matching
`general/100` through `general/400` lifetime/initializer subset, the full
PA15 report, the through-PA14 report, and the source audit.  Access/inherited
lookup and operator/ADL groups remain queued after that checkpoint.

## Turn 33 Checkpoint Scope

This increment targets the initializer/lifetime group above.  It will make
global class and scalar initializers first-class typed plans, render their
zero/static storage separately from dynamic actions, and emit the required
namespace-scope startup and shutdown calls in declaration order with reverse
finalization order.  It will also extend the existing local aggregate path to
construct class-array elements and recursively apply aggregate/default-member
initialization for nested nontrivial subobjects.  The scope is validated with
the affected `general/200` and `general/300` global/lifetime cases, class-array
and aggregate cases in `general/200` and `general/400`, the full PA15 report,
the through-PA14 report, and the source audit.

## Turn 33 Checkpoint Result

Completed.  Global class objects now have typed zero/static storage and an
explicit initialization phase; dynamic scalar, class, and class-array
initializers lower into the startup helper, with finalizers emitted in reverse
global order.  Defaulted constructors and empty scalar/pointer member
initializers are collected and lowered.  Local class arrays now construct and
finalize each element, including early-return cleanup.  Aggregate lowering
handles nested class/array subobjects, brace elision, reference-member
binding/address semantics, empty array value-initialization, value-initialized
nontrivial aggregates, and bit-field mask/merge storage with stable LowIR
ordering.  Aggregate/lifetime lowering was split into
`pa14_lowering_objects.cpp` so the source audit remains clean.

Focused validation passed for the Turn 33 scope, including global zero and
class-array initialization, local class-array lifecycle, aggregate array
brace elision, aggregate reference members, empty array value-initialization,
value-initialized nontrivial aggregates, and all four PA15 bit-field cases.
The full required PA15 report is **86 / 200**, up from the checkpoint baseline
of **46 / 200**; through-PA14 remains **819 / 819**, and the file audit passes
with only the three existing header-division warnings.  No tests or reference
fixtures were changed.

## Turn 33 Remaining Work Map and Next Checkpoint

The remaining failures stay grouped by shared behavior: default-member and
aggregate edge cases still involving class-member target selection; ordinary
and static member collection/cv/access behavior; constructor policy for
deleted, explicit, inherited, and derived/base cases; references and
inheritance conversions; operator/ADL and callable expressions; and parser,
friend, `noexcept`, declaration-metadata, and diagnostic boundaries.

The next checkpoint should take the member-collection group: improve typed
member binding and lookup for ordinary/static methods and fields, then cover
`this`/`.`/`->` calls, access checks, and cv-qualified overload selection.
Validate with the direct-member and method subset, the full PA15 report,
through-PA14, and the source audit.
