# PA22 checkpoint plan

## Checkpoint 69 scope — 2026-07-25 (before implementation)

### Baseline

The turn-start PA22 report is **78/250**.  Earlier assignments pass.  The
complete current failure set is the 172 fixtures below; the 156 exit-status
failures, 15 LowIR comparison failures, and one timeout are grouped by the
shared semantic behavior they exercise.

### Remaining Work Map

- **Function-call deduction, argument type normalization, and array/function
  parameter matching (27):**
  `general/100-decltype-function-template-deduced-call`,
  `general/100-dependent-remove-reference-transform-forwarding`,
  `general/100-empty-pack-static-assert-trait-expansion`,
  `general/100-explicit-template-id-user-conversion-deduction`,
  `general/100-forwarding-reference-preserves-top-const-function-pointer`,
  `general/100-forwarding-reference-qualified-enumerator`,
  `general/100-function-parameter-empty-middle-pack-alias`,
  `general/100-function-reference-deduction`,
  `general/100-function-template-const-ref-top-cv-deduction`,
  `general/100-function-template-elaborated-top-cv-deduction`,
  `general/100-function-template-fixed-over-trailing-pack-fallback`,
  `general/100-function-template-template-defaulted-argument-deduction`,
  `general/100-function-template-variadic-template-template-deduction`,
  `general/100-local-class-declval-explicit-template-id`,
  `general/100-pointer-qualification-deduction`,
  `general/100-qualified-alias-template-member-deduction`,
  `general/100-template-array-reference-cv-default-arg` (timeout),
  `general/100-template-deduction-rejects-value-base-argument`,
  `general/100-type-pack-element-preserves-concrete-argument`,
  `general/100-type-pack-element-result-selects-copy-ctor`,
  `general/200-constructor-template-rvalue-beats-const-ref`,
  `general/200-inherited-constructor-template-forwarding`,
  `general/200-partial-ordering-ref-vs-const-ref`,
  `general/200-range-array-reference-mutable-begin`,
  `spec/100-function-template-array-bound-braced-empty-argument`,
  `spec/100-function-template-array-bound-deduction`,
  `spec/100-function-template-array-bound-only-deduction`,
  `spec/100-function-template-array-bound-shared-deduction`,
  `spec/100-function-template-array-parameter-string-literal`.
- **Function-template partial ordering and overload ranking (23):**
  `general/200-ambiguous-cv-pointer-partial-ordering-bad`,
  `general/200-class-template-partial-order-placeholder-argument`,
  `general/200-empty-index-sequence-overload-order`,
  `general/200-function-pointer-vs-const-ref-partial-order`,
  `general/200-function-template-partial-order-class-template-cv`,
  `general/200-function-template-partial-order-const-pointer`,
  `general/200-function-template-trailing-pack-partial-order`,
  `general/200-partial-order-synthetic-virtual-member-emission`,
  `general/200-partial-ordering-pointer-vs-value`,
  `spec/100-constructor-template-braced-array-bound-deduction`,
  `spec/200-array-reference-cv-partial-ordering`,
  `spec/200-constructor-template-qualified-nested-id-partial-ordering`,
  `spec/200-defaulted-class-template-argument-prefix-deduction`,
  `spec/200-dependent-specialized-default-arg-deduction`,
  `spec/200-function-template-class-template-param-partial-order`,
  `spec/200-function-template-fixed-parameter-default-tail-partial-order`,
  `spec/200-function-template-partial-order-const-pointer`,
  `spec/200-member-operator-fixed-tag-default-partial-order`,
  `spec/200-nondeduced-qualified-member-type-allows-conversion`,
  `spec/200-overload-set-address-nondeduced-bad`,
  `spec/300-constructor-forwarding-lvalue-beats-const-ref`,
  `spec/500-template-template-piecewise-partial-ordering`.
- **Substitution failure, dependent expressions, and deferred/no-eager
  instantiation (46):**
  `general/300-abstract-array-parameter-sfinae`,
  `general/300-alias-sfinae-function-pointer-deduction-key`,
  `general/300-alias-sfinae-inherited-member-value`,
  `general/300-base-qualified-template-value-arg-syntax`,
  `general/300-boost-enable-if-type-condition-static-keyword-overload`,
  `general/300-class-template-id-argument-no-eager-complete`,
  `general/300-decltype-conditional-no-body-instantiation`,
  `general/300-decltype-default-type-arg-construction-sfinae`,
  `general/300-defaulted-enable-if-overload-drop`,
  `general/300-defaulted-sfinae-ctor-candidate-drop`,
  `general/300-dependent-enable-if-nontype-candidate-drop`,
  `general/300-dependent-enable-if-return-less-equal`,
  `general/300-dependent-enable-if-return-nontype-less-pack`,
  `general/300-destructor-template-id-sfinae`,
  `general/300-explicit-function-template-pack-alias-sfinae`,
  `general/300-explicit-template-call-transitive-base-deduction`,
  `general/300-function-template-nested-alias-explicit-call`,
  `general/300-hidden-friend-dependent-return-specialization-scope`,
  `general/300-hidden-friend-sfinae-use-scope-shadowing`,
  `general/300-internal-remove-cvref-alias-sfinae`,
  `general/300-lazy-nested-member-class-instantiation`,
  `general/300-local-alias-explicit-template-pack-decltype`,
  `general/300-member-template-assignment-sfinae-copy-fallback`,
  `general/300-out-of-class-partial-owner-ctor-using-alias`,
  `general/300-pack-expanded-enable-if-member-value`,
  `general/300-qualified-alias-nontype-pack-function-deduction`,
  `general/300-qualified-alias-sfinae-function-pointer-deduction-key`,
  `general/300-qualified-rebind-detected-type-arg`,
  `general/300-recursive-streamable-sfinae-guard`,
  `general/300-single-element-detector-idiom-sfinae-false`,
  `general/300-structured-enable-if-sizeof-pack-value`,
  `general/300-structured-lazy-and-enable-if-pack-size`,
  `general/300-unevaluated-sizeof-call-surrogates`,
  `general/300-using-declaration-imports-member-template-sfinae-shadow`,
  `general/300-using-directive-overloaded-function-template-arg`,
  `general/300-using-member-template-implicit-object-cv-overload`,
  `general/300-variable-template-detected-idiom-direct-arg`,
  `general/400-pack-expansion-size-mismatch-sfinae`,
  `general/400-qualified-member-alias-sfinae`,
  `general/400-template-template-alias-default-arity-sfinae`,
  `general/500-adl-alias-return-operator-template`,
  `general/500-adl-explicit-function-template-id`,
  `general/500-alias-pack-enable-if-constexpr-constructor`,
  `general/500-alias-rebind-forwarding-nondependent-param`,
  `general/500-alias-template-template-defaulted-sfinae-canonical-args`,
  `general/500-async-initiate-dependent-return-sfinae`,
  `general/500-boost-mp11-conditional-alias-reference-set`,
  `general/500-defaulted-pack-bool-short-circuit-sfinae`,
  `general/500-dependent-result-sizeof-sfinae-base`,
  `general/500-member-template-dependent-owner-defaulted-sfinae`,
  `general/500-member-template-enable-if-redeclaration-overload`,
  `general/500-member-template-retained-dependent-param-candidate-drop`,
  `general/500-pack-alias-functional-bool-trait-sfinae`,
  `general/500-partial-specialization-cv-qualifier-subset`,
  `general/500-short-circuit-alias-member-sfinae`,
  `general/500-sizeof-void-sfinae-fallback`,
  `spec/300-decltype-call-substitution-failure-partial-specialization`,
  `spec/300-defaulted-enable-if-after-array-bound-deduction`,
  `spec/300-detected-or-sfinae-fallback`,
  `spec/300-expression-sfinae-decltype-conversion`,
  `spec/300-expression-sfinae-decltype-member-comma`,
  `spec/300-expression-sfinae-decltype`,
  `spec/300-hidden-friend-template-pointer-sfinae-adl`,
  `spec/300-inline-namespace-using-directive-sfinae-overloads`,
  `spec/300-out-of-class-sfinae-member-template-alias-body`,
  `spec/300-out-of-class-sfinae-member-template-body`,
  `spec/300-qualified-function-template-return-sfinae-overloads`,
  `spec/300-qualified-member-function-value-fallback-sfinae`,
  `spec/300-sfinae-member-typedef-probe-no-body-instantiation`,
  `spec/300-stream-insertion-decltype-overload-sfinae`,
  `spec/300-template-id-direct-parameter-same-name-deduction`,
  `spec/300-typedef-class-template-does-not-instantiate`,
  `spec/300-void-t-detector`,
  `spec/500-hidden-friend-query-free-decltype-noexcept`,
  `spec/500-member-alias-pack-owner-sfinae`.
- **Alias, template-template, pack, constructor/conversion and owner
  propagation (42):**
  `general/300-alias-bool-explicit-pack-call-dependent-tag`,
  `general/300-constructor-template-keeps-ctor-refinement-viable`,
  `general/300-empty-pack-unknown-bound-array-lowir`,
  `general/300-inherited-variable-template-enable-if-return`,
  `general/300-static-member-template-function-pointer-nttp`,
  `general/300-variable-template-detected-idiom-direct-arg`,
  `general/400-alias-template-function-argument-cv`,
  `general/400-constructor-template-pack-before-defaulted-nontype`,
  `general/400-conversion-function-template-prefers-nontemplate`,
  `general/400-function-assignment-invocable-and-helper`,
  `general/400-member-alias-template-template-partial-deduction-owner`,
  `general/400-partial-specialization-inherited-constructor-template`,
  `general/400-template-template-alias-default-arity-sfinae`,
  `general/400-unnamed-nontype-pack-static-enable-if-default`,
  `general/500-constructor-pack-default-rewritten-pointer`,
  `general/500-constructor-sfinae-member-template-value`,
  `general/500-explicit-pack-deduced-pack-member-result`,
  `general/500-index-sequence-alias-constructor-deduction`,
  `general/500-inherited-constructor-template-member-alias-pack`,
  `general/500-weak-ptr-shared-ptr-template-ctor`,
  `spec/100-constructor-template-braced-array-bound-deduction`,
  `spec/100-explicit-specialization-dependent-param-typedef`,
  `spec/100-explicit-template-argument-overload-rejects-short-candidate`,
  `spec/200-member-template-explicit-pack-forward-call`,
  `spec/200-member-template-nontype-param-shadows-inherited-value`,
  `spec/200-member-template-nontype-param-shadows-inherited-value-sum`,
  `spec/300-constructor-template-const-ref-conversion`,
  `spec/300-conversion-function-template-owner-result-copy-init`,
  `spec/300-cross-specialization-converting-ctor-operator-template`,
  `spec/300-current-specialization-constructor-template-canonical-owner`,
  `spec/300-current-specialization-constructor-template-owner`,
  `spec/400-dependent-decltype-member-template-conversion-operator`,
  `spec/500-conversion-function-template-reference-conditional-auto-ref`,
  `spec/500-conversion-function-template-same-name-target`,
  `spec/500-defaulted-rebind-constructor-deduction`,
  `spec/500-function-result-template-id-shadowed-argument`,
  `spec/500-template-template-conversion-operator-reference-target`,
  `spec/500-type-pack-qualified-static-member-expansion`,
  `spec/500-unqualified-member-template-local-alias-deduction`.
- **Typed non-type template arguments and LowIR materialization/comparison
  (34):**
  `general/300-inherited-variable-template-enable-if-return`,
  `general/300-static-member-template-function-pointer-nttp`,
  `general/300-variable-template-detected-idiom-direct-arg`,
  `general/400-defaulted-pointer-nontype-cstyle-null`,
  `general/400-enum-nttp-cstyle-cast-default-rebind`,
  `general/400-object-pointer-nttp-address`,
  `general/400-object-pointer-nttp-rebound-member-template`,
  `general/400-static-data-nttp-pack-sizeof-bound`,
  `general/500-bool-alias-function-template-result-metadata`,
  `general/500-current-specialization-nontype-default-dependent`,
  `general/500-defaulted-nontype-qualified-alias-value`,
  `general/500-owner-enum-nontype-result-sfinae`,
  `spec/400-nontype-reference-argument`,
  and the LowIR comparison portions of
  `general/100-function-parameter-empty-middle-pack-alias`,
  `general/100-function-template-fixed-over-trailing-pack-fallback`,
  `general/200-class-template-partial-order-placeholder-argument`,
  `general/200-empty-index-sequence-overload-order`,
  `general/200-function-template-partial-order-class-template-cv`,
  `general/200-function-template-partial-order-const-pointer`,
  `general/200-partial-order-synthetic-virtual-member-emission`,
  `general/200-partial-ordering-pointer-vs-value`,
  `general/300-defaulted-sfinae-ctor-candidate-drop`,
  `general/300-empty-pack-unknown-bound-array-lowir`,
  `general/300-inherited-variable-template-enable-if-return`,
  `general/300-variable-template-detected-idiom-direct-arg`,
  `general/400-constructor-template-pack-before-defaulted-nontype`,
  `general/400-static-data-nttp-pack-sizeof-bound`,
  `general/400-template-template-alias-default-arity-sfinae`,
  `general/400-unnamed-nontype-pack-static-enable-if-default`,
  `general/500-bool-alias-function-template-result-metadata`,
  `general/500-current-specialization-nontype-default-dependent`,
  `spec/100-constructor-template-braced-array-bound-deduction`,
  `spec/100-explicit-specialization-dependent-param-typedef`,
  `spec/100-explicit-template-argument-overload-rejects-short-candidate`,
  `spec/200-defaulted-class-template-argument-prefix-deduction`,
  `spec/200-dependent-specialized-default-arg-deduction`,
  `spec/300-cross-specialization-converting-ctor-operator-template`,
  `spec/300-qualified-member-function-value-fallback-sfinae`,
  `spec/300-void-t-detector`,
  `spec/400-dependent-decltype-member-template-conversion-operator`,
  `spec/500-type-pack-qualified-static-member-expansion`.

Some fixtures intentionally exercise more than one bucket; the overlapping
list records the observable failure surface, while the first matching bucket
is the primary ownership for checkpoint planning.  No fixture from the
current report is intentionally omitted.

### Checkpoint Scope

Implement the call-deduction slice first: make `InferFunctionArguments` and
its typed helpers handle function references/pointers, forwarding and
top-level cv/reference adjustment, array references and bounds, string/array
decay, braced empty arguments, explicit prefixes, defaults, and trailing or
middle packs.  Preserve function signatures as typed deduction facts so
overload-set arguments are deferred and resolved against the expected
parameter type.  Use those completed bindings in the existing candidate path
without eagerly instantiating discarded candidates.  The selected behavior is
validated by the 27 call/array fixtures above, the three directly affected
partial-ordering fixtures, the full PA22 report, through-PA21, and the PA22
file audit.

The SFINAE/deferred, alias/template-template/owner, typed non-type, conversion
and remaining partial-ordering groups stay as explicit follow-up checkpoints;
the next group after this increment is substitution failure plus deferred
instantiation, bundled with any newly exposed partial-ordering cases.

## Checkpoint 69 result

Implemented and validated.  The current PA22 report improved from **78/250**
to **88/250**: ten baseline fixtures are fixed, including the prior timeout,
and no new PA22 failure names were introduced.  The increment preserves typed
function signatures and deduction facts through function-reference/pointer
matching, forwarding-reference lvalue adjustment, top-level cv normalization,
array-bound deduction/decay, and the affected decltype/SFINAE call path.  It
also carries forwarding-pack lvalue categories separately during replay,
validates missing `typename` without rejecting current-specialization
constructor types, keeps explicit-specialization identity stable for LowIR,
and preserves typed return facts for associated-namespace operator deduction.

Validation completed: the audited build succeeds; PA22 reports 88/250 with no
timeout; the through-PA21 suite passes 1850/1850; the repaired PA19
rematerialization regression passes; and the PA22 file audit passes with the
repository's existing warnings.

### Remaining Work Map after checkpoint

The final active report is **88/250**: **162** failures and **0** timeouts.
The exact current failure set is grouped by harness bucket below; no fixture
is omitted.

- **general/100 — call deduction and argument normalization (14):**
  `general/100-dependent-remove-reference-transform-forwarding.t`,
  `general/100-empty-pack-static-assert-trait-expansion.t`,
  `general/100-explicit-template-id-user-conversion-deduction.t`,
  `general/100-forwarding-reference-preserves-top-const-function-pointer.t`,
  `general/100-forwarding-reference-qualified-enumerator.t`,
  `general/100-function-parameter-empty-middle-pack-alias.t`,
  `general/100-function-template-elaborated-top-cv-deduction.t`,
  `general/100-function-template-fixed-over-trailing-pack-fallback.t`,
  `general/100-function-template-template-defaulted-argument-deduction.t`,
  `general/100-function-template-variadic-template-template-deduction.t`,
  `general/100-local-class-declval-explicit-template-id.t`,
  `general/100-qualified-alias-template-member-deduction.t`,
  `general/100-template-deduction-rejects-value-base-argument.t`,
  `general/100-type-pack-element-result-selects-copy-ctor.t`.

- **general/200 — partial ordering and overload ranking (13):**
  `general/200-ambiguous-cv-pointer-partial-ordering-bad.t`,
  `general/200-class-template-partial-order-placeholder-argument.t`,
  `general/200-constructor-template-rvalue-beats-const-ref.t`,
  `general/200-empty-index-sequence-overload-order.t`,
  `general/200-function-pointer-vs-const-ref-partial-order.t`,
  `general/200-function-template-partial-order-class-template-cv.t`,
  `general/200-function-template-partial-order-const-pointer.t`,
  `general/200-function-template-trailing-pack-partial-order.t`,
  `general/200-inherited-constructor-template-forwarding.t`,
  `general/200-partial-order-synthetic-virtual-member-emission.t`,
  `general/200-partial-ordering-pointer-vs-value.t`,
  `general/200-partial-ordering-ref-vs-const-ref.t`,
  `general/200-range-array-reference-mutable-begin.t`.

- **general/300 — substitution failure and deferred instantiation (42):**
  `general/300-abstract-array-parameter-sfinae.t`,
  `general/300-alias-bool-explicit-pack-call-dependent-tag.t`,
  `general/300-alias-sfinae-function-pointer-deduction-key.t`,
  `general/300-alias-sfinae-inherited-member-value.t`,
  `general/300-base-qualified-template-value-arg-syntax.t`,
  `general/300-boost-enable-if-type-condition-static-keyword-overload.t`,
  `general/300-class-template-id-argument-no-eager-complete.t`,
  `general/300-constructor-template-keeps-ctor-refinement-viable.t`,
  `general/300-decltype-conditional-no-body-instantiation.t`,
  `general/300-decltype-default-type-arg-construction-sfinae.t`,
  `general/300-defaulted-enable-if-overload-drop.t`,
  `general/300-defaulted-sfinae-ctor-candidate-drop.t`,
  `general/300-dependent-enable-if-nontype-candidate-drop.t`,
  `general/300-dependent-enable-if-return-less-equal.t`,
  `general/300-dependent-enable-if-return-nontype-less-pack.t`,
  `general/300-destructor-template-id-sfinae.t`,
  `general/300-empty-pack-unknown-bound-array-lowir.t`,
  `general/300-explicit-function-template-pack-alias-sfinae.t`,
  `general/300-explicit-template-call-transitive-base-deduction.t`,
  `general/300-function-template-nested-alias-explicit-call.t`,
  `general/300-hidden-friend-dependent-return-specialization-scope.t`,
  `general/300-hidden-friend-sfinae-use-scope-shadowing.t`,
  `general/300-inherited-variable-template-enable-if-return.t`,
  `general/300-internal-remove-cvref-alias-sfinae.t`,
  `general/300-lazy-nested-member-class-instantiation.t`,
  `general/300-local-alias-explicit-template-pack-decltype.t`,
  `general/300-member-template-assignment-sfinae-copy-fallback.t`,
  `general/300-out-of-class-partial-owner-ctor-using-alias.t`,
  `general/300-pack-expanded-enable-if-member-value.t`,
  `general/300-qualified-alias-nontype-pack-function-deduction.t`,
  `general/300-qualified-alias-sfinae-function-pointer-deduction-key.t`,
  `general/300-qualified-rebind-detected-type-arg.t`,
  `general/300-recursive-streamable-sfinae-guard.t`,
  `general/300-single-element-detector-idiom-sfinae-false.t`,
  `general/300-static-member-template-function-pointer-nttp.t`,
  `general/300-structured-enable-if-sizeof-pack-value.t`,
  `general/300-structured-lazy-and-enable-if-pack-size.t`,
  `general/300-unevaluated-sizeof-call-surrogates.t`,
  `general/300-using-declaration-imports-member-template-sfinae-shadow.t`,
  `general/300-using-directive-overloaded-function-template-arg.t`,
  `general/300-using-member-template-implicit-object-cv-overload.t`,
  `general/300-variable-template-detected-idiom-direct-arg.t`.

- **general/400 — aliases, constructors, conversions, and typed NTTPs (14):**
  `general/400-alias-template-function-argument-cv.t`,
  `general/400-constructor-template-pack-before-defaulted-nontype.t`,
  `general/400-conversion-function-template-prefers-nontemplate.t`,
  `general/400-defaulted-pointer-nontype-cstyle-null.t`,
  `general/400-enum-nttp-cstyle-cast-default-rebind.t`,
  `general/400-function-assignment-invocable-and-helper.t`,
  `general/400-member-alias-template-template-partial-deduction-owner.t`,
  `general/400-object-pointer-nttp-address.t`,
  `general/400-object-pointer-nttp-rebound-member-template.t`,
  `general/400-pack-expansion-size-mismatch-sfinae.t`,
  `general/400-partial-specialization-inherited-constructor-template.t`,
  `general/400-static-data-nttp-pack-sizeof-bound.t`,
  `general/400-template-template-alias-default-arity-sfinae.t`,
  `general/400-unnamed-nontype-pack-static-enable-if-default.t`.

- **general/500 — ADL, aliases, packs, owners, and NTTPs (26):**
  `general/500-adl-alias-return-operator-template.t`,
  `general/500-adl-explicit-function-template-id.t`,
  `general/500-alias-pack-enable-if-constexpr-constructor.t`,
  `general/500-alias-rebind-forwarding-nondependent-param.t`,
  `general/500-alias-template-template-defaulted-sfinae-canonical-args.t`,
  `general/500-async-initiate-dependent-return-sfinae.t`,
  `general/500-bool-alias-function-template-result-metadata.t`,
  `general/500-boost-mp11-conditional-alias-reference-set.t`,
  `general/500-constructor-pack-default-rewritten-pointer.t`,
  `general/500-constructor-sfinae-member-template-value.t`,
  `general/500-current-specialization-nontype-default-dependent.t`,
  `general/500-defaulted-nontype-qualified-alias-value.t`,
  `general/500-defaulted-pack-bool-short-circuit-sfinae.t`,
  `general/500-dependent-result-sizeof-sfinae-base.t`,
  `general/500-explicit-pack-deduced-pack-member-result.t`,
  `general/500-index-sequence-alias-constructor-deduction.t`,
  `general/500-inherited-constructor-template-member-alias-pack.t`,
  `general/500-member-template-dependent-owner-defaulted-sfinae.t`,
  `general/500-member-template-enable-if-redeclaration-overload.t`,
  `general/500-member-template-retained-dependent-param-candidate-drop.t`,
  `general/500-owner-enum-nontype-result-sfinae.t`,
  `general/500-pack-alias-functional-bool-trait-sfinae.t`,
  `general/500-partial-specialization-cv-qualifier-subset.t`,
  `general/500-short-circuit-alias-member-sfinae.t`,
  `general/500-sizeof-void-sfinae-fallback.t`,
  `general/500-weak-ptr-shared-ptr-template-ctor.t`.

- **spec/100 — basic deduction and array bounds (6):**
  `spec/100-constructor-template-braced-array-bound-deduction.t`,
  `spec/100-explicit-specialization-dependent-param-typedef.t`,
  `spec/100-explicit-template-argument-overload-rejects-short-candidate.t`,
  `spec/100-function-template-array-bound-braced-empty-argument.t`,
  `spec/100-function-template-array-bound-only-deduction.t`,
  `spec/100-function-template-array-parameter-string-literal.t`.

- **spec/200 — partial ordering and nondeduced contexts (13):**
  `spec/200-array-reference-cv-partial-ordering.t`,
  `spec/200-constructor-template-qualified-nested-id-partial-ordering.t`,
  `spec/200-defaulted-class-template-argument-prefix-deduction.t`,
  `spec/200-dependent-specialized-default-arg-deduction.t`,
  `spec/200-function-template-class-template-param-partial-order.t`,
  `spec/200-function-template-fixed-parameter-default-tail-partial-order.t`,
  `spec/200-function-template-partial-order-const-pointer.t`,
  `spec/200-member-operator-fixed-tag-default-partial-order.t`,
  `spec/200-member-template-explicit-pack-forward-call.t`,
  `spec/200-member-template-nontype-param-shadows-inherited-value-sum.t`,
  `spec/200-member-template-nontype-param-shadows-inherited-value.t`,
  `spec/200-nondeduced-qualified-member-type-allows-conversion.t`,
  `spec/200-overload-set-address-nondeduced-bad.t`.

- **spec/300 — SFINAE, owners, and deferred materialization (22):**
  `spec/300-constructor-forwarding-lvalue-beats-const-ref.t`,
  `spec/300-constructor-template-const-ref-conversion.t`,
  `spec/300-conversion-function-template-owner-result-copy-init.t`,
  `spec/300-cross-specialization-converting-ctor-operator-template.t`,
  `spec/300-current-specialization-constructor-template-canonical-owner.t`,
  `spec/300-current-specialization-constructor-template-owner.t`,
  `spec/300-decltype-call-substitution-failure-partial-specialization.t`,
  `spec/300-detected-or-sfinae-fallback.t`,
  `spec/300-expression-sfinae-decltype-conversion.t`,
  `spec/300-expression-sfinae-decltype-member-comma.t`,
  `spec/300-expression-sfinae-decltype.t`,
  `spec/300-hidden-friend-template-pointer-sfinae-adl.t`,
  `spec/300-inline-namespace-using-directive-sfinae-overloads.t`,
  `spec/300-out-of-class-sfinae-member-template-alias-body.t`,
  `spec/300-out-of-class-sfinae-member-template-body.t`,
  `spec/300-qualified-function-template-return-sfinae-overloads.t`,
  `spec/300-qualified-member-function-value-fallback-sfinae.t`,
  `spec/300-sfinae-member-typedef-probe-no-body-instantiation.t`,
  `spec/300-stream-insertion-decltype-overload-sfinae.t`,
  `spec/300-template-id-direct-parameter-same-name-deduction.t`,
  `spec/300-typedef-class-template-does-not-instantiate.t`,
  `spec/300-void-t-detector.t`.

- **spec/400 — dependent conversion and NTTP references (2):**
  `spec/400-dependent-decltype-member-template-conversion-operator.t`,
  `spec/400-nontype-reference-argument.t`.

- **spec/500 — final owner, alias, template-template, and pack cases (10):**
  `spec/500-conversion-function-template-reference-conditional-auto-ref.t`,
  `spec/500-conversion-function-template-same-name-target.t`,
  `spec/500-defaulted-rebind-constructor-deduction.t`,
  `spec/500-function-result-template-id-shadowed-argument.t`,
  `spec/500-hidden-friend-query-free-decltype-noexcept.t`,
  `spec/500-member-alias-pack-owner-sfinae.t`,
  `spec/500-template-template-conversion-operator-reference-target.t`,
  `spec/500-template-template-piecewise-partial-ordering.t`,
  `spec/500-type-pack-qualified-static-member-expansion.t`,
  `spec/500-unqualified-member-template-local-alias-deduction.t`.

### Next Checkpoint Group

Start with the substantial **general/300 + spec/300 substitution-failure and
deferred-instantiation group (64 fixtures)**, including dependent `decltype`,
SFINAE candidate dropping, hidden-friend lookup, and no-eager-body cases.  Then
bundle the smaller **100/200 deduction and ordering bands (46)**, followed by
the **400/500 alias, owner, conversion, pack, and typed-NTTP bands (52)**.

## Checkpoint 71 scope — 2026-07-25 (before implementation)

### Refreshed failure audit

The required `make test-report ACTIVE_TEST_REPORT_PAS='pa22'` report is
**88/250**, with earlier assignments passing and **162** current-PA failures.
The complete names remain recorded in the preceding post-checkpoint map; the
failure set is grouped here by primary shared behavior:

- **Substitution failure and deferred instantiation (64):** 42 `general/300`
  failures plus 22 `spec/300` failures covering dependent `decltype`, alias and
  `enable_if` substitution, candidate dropping, dependent lookup/ADL, and
  no-eager body/class materialization.
- **Call deduction and overload ordering (27):** 14 `general/100` plus 13
  `general/200` failures covering normalized argument types, packs, partial
  ordering, and constructor ranking.
- **Alias, owner, conversion, and typed non-type arguments (40):** 14
  `general/400` plus 26 `general/500` failures covering owner propagation,
  conversion/constructor templates, aliases, packs, ADL, and typed NTTPs.
- **Spec edge cases and LowIR materialization (31):** six `spec/100`, 13
  `spec/200`, two `spec/400`, and ten `spec/500` failures, overlapping the
  behaviors above but retained as a separate validation band.

### Checkpoint Scope

Implement the complete **64-fixture substitution/deferred-instantiation
slice**.  The compiler must carry substitution state as typed semantic facts,
resolve dependent aliases and `decltype` probes only after template arguments
are known, turn invalid dependent forms into discarded candidates during
overload/partial-specialization selection, preserve qualified and hidden-friend
lookup context, and avoid instantiating discarded bodies or merely named class
templates.  Validate with every failing `general/300` and `spec/300` fixture,
the full PA22 report, through-PA21, and the PA22 file audit.  The remaining
98 fixtures are intentionally outside this checkpoint; the next group is the
27-fixture 100/200 deduction-and-ordering band, bundled with any newly exposed
partial-ordering cases.

For this turn, the executable checkpoint within that behavior family is the
candidate-local immediate-context slice: reject a substitution-invalid
function candidate while continuing overload selection, preserve typed
ordinary signatures beside non-type-parameter templates, validate generated
qualified member types from indexed semantic facts, and resolve generated
qualified static `value` members during integral non-type argument binding.
The deeper dependent `decltype`, pack-expression, hidden-friend, and deferred
class-owner cases remain explicit follow-up work in this same family.

## Checkpoint 71 result — 2026-07-25

The implementation completes the candidate-local immediate-context portion of
the recorded substitution/deferred-instantiation scope.  Function-template
candidate inference and replay now treat substitution/instantiation
`logic_error` as a failed candidate and continue overload selection.  Typed
compiler state keeps non-type-parameter function templates out of ordinary
call signatures, validates generated qualified member types against indexed
member/alias/source facts, and resolves a generated qualified static `value`
member through the typed integral-member path when it is used as a non-type
template argument.  Result metadata is committed only after candidate replay
and associated-namespace return-type rewriting succeed.

The original 64-fixture 300-band contains deeper dependent `decltype`,
pack-expression parsing, hidden-friend/ADL, and deferred class/owner cases that
are not yet closed by this increment; they are retained below as the next
subgroups rather than being counted as fixed.  The seven fixtures that left
the previous failure set are `general/300-defaulted-enable-if-overload-drop`,
`general/300-dependent-enable-if-nontype-candidate-drop`,
`general/300-member-template-assignment-sfinae-copy-fallback`,
`general/300-structured-lazy-and-enable-if-pack-size`,
`spec/300-inline-namespace-using-directive-sfinae-overloads`,
`spec/300-qualified-function-template-return-sfinae-overloads`, and
`spec/500-member-alias-pack-owner-sfinae`.  One target-aware constructor
shadowing diagnostic is newly exposed in `general/400` and remains mapped
below.

Validation completed: the compiler build succeeds; the PA22 report is
**94/250** with no timeout (up from the continuation-state 92/250 and the
checkpoint baseline 88/250); `make test-report-through-pa21` passes
**1850/1850**; and `perl scripts/cppgm_file_audit.pl --stage pa22 --paths
dev/src` passes.

### Remaining Work Map

The exact active report has **156** failures.  The complete current set is
grouped below; the first four groups are the next coherent substitution and
lookup work, followed by deduction/order and the alias/owner/NTTP bands.

- **general/100 — call deduction and argument normalization (14):**
  `general/100-dependent-remove-reference-transform-forwarding.t`,
  `general/100-empty-pack-static-assert-trait-expansion.t`,
  `general/100-explicit-template-id-user-conversion-deduction.t`,
  `general/100-forwarding-reference-preserves-top-const-function-pointer.t`,
  `general/100-forwarding-reference-qualified-enumerator.t`,
  `general/100-function-parameter-empty-middle-pack-alias.t`,
  `general/100-function-template-elaborated-top-cv-deduction.t`,
  `general/100-function-template-fixed-over-trailing-pack-fallback.t`,
  `general/100-function-template-template-defaulted-argument-deduction.t`,
  `general/100-function-template-variadic-template-template-deduction.t`,
  `general/100-local-class-declval-explicit-template-id.t`,
  `general/100-qualified-alias-template-member-deduction.t`,
  `general/100-template-deduction-rejects-value-base-argument.t`,
  `general/100-type-pack-element-result-selects-copy-ctor.t`.

- **general/200 — partial ordering and overload ranking (13):**
  `general/200-ambiguous-cv-pointer-partial-ordering-bad.t`,
  `general/200-class-template-partial-order-placeholder-argument.t`,
  `general/200-constructor-template-rvalue-beats-const-ref.t`,
  `general/200-empty-index-sequence-overload-order.t`,
  `general/200-function-pointer-vs-const-ref-partial-order.t`,
  `general/200-function-template-partial-order-class-template-cv.t`,
  `general/200-function-template-partial-order-const-pointer.t`,
  `general/200-function-template-trailing-pack-partial-order.t`,
  `general/200-inherited-constructor-template-forwarding.t`,
  `general/200-partial-order-synthetic-virtual-member-emission.t`,
  `general/200-partial-ordering-pointer-vs-value.t`,
  `general/200-partial-ordering-ref-vs-const-ref.t`,
  `general/200-range-array-reference-mutable-begin.t`.

- **general/300 — dependent substitution and deferred replay (38):**
  `general/300-abstract-array-parameter-sfinae.t`,
  `general/300-alias-bool-explicit-pack-call-dependent-tag.t`,
  `general/300-alias-sfinae-function-pointer-deduction-key.t`,
  `general/300-alias-sfinae-inherited-member-value.t`,
  `general/300-base-qualified-template-value-arg-syntax.t`,
  `general/300-boost-enable-if-type-condition-static-keyword-overload.t`,
  `general/300-class-template-id-argument-no-eager-complete.t`,
  `general/300-constructor-template-keeps-ctor-refinement-viable.t`,
  `general/300-decltype-conditional-no-body-instantiation.t`,
  `general/300-decltype-default-type-arg-construction-sfinae.t`,
  `general/300-defaulted-sfinae-ctor-candidate-drop.t`,
  `general/300-dependent-enable-if-return-less-equal.t`,
  `general/300-dependent-enable-if-return-nontype-less-pack.t`,
  `general/300-destructor-template-id-sfinae.t`,
  `general/300-empty-pack-unknown-bound-array-lowir.t`,
  `general/300-explicit-function-template-pack-alias-sfinae.t`,
  `general/300-explicit-template-call-transitive-base-deduction.t`,
  `general/300-function-template-nested-alias-explicit-call.t`,
  `general/300-hidden-friend-dependent-return-specialization-scope.t`,
  `general/300-hidden-friend-sfinae-use-scope-shadowing.t`,
  `general/300-inherited-variable-template-enable-if-return.t`,
  `general/300-internal-remove-cvref-alias-sfinae.t`,
  `general/300-lazy-nested-member-class-instantiation.t`,
  `general/300-local-alias-explicit-template-pack-decltype.t`,
  `general/300-out-of-class-partial-owner-ctor-using-alias.t`,
  `general/300-pack-expanded-enable-if-member-value.t`,
  `general/300-qualified-alias-nontype-pack-function-deduction.t`,
  `general/300-qualified-alias-sfinae-function-pointer-deduction-key.t`,
  `general/300-qualified-rebind-detected-type-arg.t`,
  `general/300-recursive-streamable-sfinae-guard.t`,
  `general/300-single-element-detector-idiom-sfinae-false.t`,
  `general/300-static-member-template-function-pointer-nttp.t`,
  `general/300-structured-enable-if-sizeof-pack-value.t`,
  `general/300-unevaluated-sizeof-call-surrogates.t`,
  `general/300-using-declaration-imports-member-template-sfinae-shadow.t`,
  `general/300-using-directive-overloaded-function-template-arg.t`,
  `general/300-using-member-template-implicit-object-cv-overload.t`,
  `general/300-variable-template-detected-idiom-direct-arg.t`.

- **general/400 — aliases, constructors, conversions, and typed NTTPs (15):**
  `general/400-alias-template-function-argument-cv.t`,
  `general/400-bad-constructor-template-parameter-shadowing-target-aware.t`,
  `general/400-constructor-template-pack-before-defaulted-nontype.t`,
  `general/400-conversion-function-template-prefers-nontemplate.t`,
  `general/400-defaulted-pointer-nontype-cstyle-null.t`,
  `general/400-enum-nttp-cstyle-cast-default-rebind.t`,
  `general/400-function-assignment-invocable-and-helper.t`,
  `general/400-member-alias-template-template-partial-deduction-owner.t`,
  `general/400-object-pointer-nttp-address.t`,
  `general/400-object-pointer-nttp-rebound-member-template.t`,
  `general/400-pack-expansion-size-mismatch-sfinae.t`,
  `general/400-partial-specialization-inherited-constructor-template.t`,
  `general/400-static-data-nttp-pack-sizeof-bound.t`,
  `general/400-template-template-alias-default-arity-sfinae.t`,
  `general/400-unnamed-nontype-pack-static-enable-if-default.t`.

- **general/500 — ADL, aliases, packs, owners, and NTTPs (26):**
  `general/500-adl-alias-return-operator-template.t`,
  `general/500-adl-explicit-function-template-id.t`,
  `general/500-alias-pack-enable-if-constexpr-constructor.t`,
  `general/500-alias-rebind-forwarding-nondependent-param.t`,
  `general/500-alias-template-template-defaulted-sfinae-canonical-args.t`,
  `general/500-async-initiate-dependent-return-sfinae.t`,
  `general/500-bool-alias-function-template-result-metadata.t`,
  `general/500-boost-mp11-conditional-alias-reference-set.t`,
  `general/500-constructor-pack-default-rewritten-pointer.t`,
  `general/500-constructor-sfinae-member-template-value.t`,
  `general/500-current-specialization-nontype-default-dependent.t`,
  `general/500-defaulted-nontype-qualified-alias-value.t`,
  `general/500-defaulted-pack-bool-short-circuit-sfinae.t`,
  `general/500-dependent-result-sizeof-sfinae-base.t`,
  `general/500-explicit-pack-deduced-pack-member-result.t`,
  `general/500-index-sequence-alias-constructor-deduction.t`,
  `general/500-inherited-constructor-template-member-alias-pack.t`,
  `general/500-member-template-dependent-owner-defaulted-sfinae.t`,
  `general/500-member-template-enable-if-redeclaration-overload.t`,
  `general/500-member-template-retained-dependent-param-candidate-drop.t`,
  `general/500-owner-enum-nontype-result-sfinae.t`,
  `general/500-pack-alias-functional-bool-trait-sfinae.t`,
  `general/500-partial-specialization-cv-qualifier-subset.t`,
  `general/500-short-circuit-alias-member-sfinae.t`,
  `general/500-sizeof-void-sfinae-fallback.t`,
  `general/500-weak-ptr-shared-ptr-template-ctor.t`.

- **spec/100 — basic deduction and array bounds (6):**
  `spec/100-constructor-template-braced-array-bound-deduction.t`,
  `spec/100-explicit-specialization-dependent-param-typedef.t`,
  `spec/100-explicit-template-argument-overload-rejects-short-candidate.t`,
  `spec/100-function-template-array-bound-braced-empty-argument.t`,
  `spec/100-function-template-array-bound-only-deduction.t`,
  `spec/100-function-template-array-parameter-string-literal.t`.

- **spec/200 — partial ordering and nondeduced contexts (13):**
  `spec/200-array-reference-cv-partial-ordering.t`,
  `spec/200-constructor-template-qualified-nested-id-partial-ordering.t`,
  `spec/200-defaulted-class-template-argument-prefix-deduction.t`,
  `spec/200-dependent-specialized-default-arg-deduction.t`,
  `spec/200-function-template-class-template-param-partial-order.t`,
  `spec/200-function-template-fixed-parameter-default-tail-partial-order.t`,
  `spec/200-function-template-partial-order-const-pointer.t`,
  `spec/200-member-operator-fixed-tag-default-partial-order.t`,
  `spec/200-member-template-explicit-pack-forward-call.t`,
  `spec/200-member-template-nontype-param-shadows-inherited-value-sum.t`,
  `spec/200-member-template-nontype-param-shadows-inherited-value.t`,
  `spec/200-nondeduced-qualified-member-type-allows-conversion.t`,
  `spec/200-overload-set-address-nondeduced-bad.t`.

- **spec/300 — SFINAE, owners, and deferred materialization (20):**
  `spec/300-constructor-forwarding-lvalue-beats-const-ref.t`,
  `spec/300-constructor-template-const-ref-conversion.t`,
  `spec/300-conversion-function-template-owner-result-copy-init.t`,
  `spec/300-cross-specialization-converting-ctor-operator-template.t`,
  `spec/300-current-specialization-constructor-template-canonical-owner.t`,
  `spec/300-current-specialization-constructor-template-owner.t`,
  `spec/300-decltype-call-substitution-failure-partial-specialization.t`,
  `spec/300-detected-or-sfinae-fallback.t`,
  `spec/300-expression-sfinae-decltype-conversion.t`,
  `spec/300-expression-sfinae-decltype-member-comma.t`,
  `spec/300-expression-sfinae-decltype.t`,
  `spec/300-hidden-friend-template-pointer-sfinae-adl.t`,
  `spec/300-out-of-class-sfinae-member-template-alias-body.t`,
  `spec/300-out-of-class-sfinae-member-template-body.t`,
  `spec/300-qualified-member-function-value-fallback-sfinae.t`,
  `spec/300-sfinae-member-typedef-probe-no-body-instantiation.t`,
  `spec/300-stream-insertion-decltype-overload-sfinae.t`,
  `spec/300-template-id-direct-parameter-same-name-deduction.t`,
  `spec/300-typedef-class-template-does-not-instantiate.t`,
  `spec/300-void-t-detector.t`.

- **spec/400 — dependent conversion and NTTP references (2):**
  `spec/400-dependent-decltype-member-template-conversion-operator.t`,
  `spec/400-nontype-reference-argument.t`.

- **spec/500 — final owner, alias, template-template, and pack cases (9):**
  `spec/500-conversion-function-template-reference-conditional-auto-ref.t`,
  `spec/500-conversion-function-template-same-name-target.t`,
  `spec/500-defaulted-rebind-constructor-deduction.t`,
  `spec/500-function-result-template-id-shadowed-argument.t`,
  `spec/500-hidden-friend-query-free-decltype-noexcept.t`,
  `spec/500-template-template-conversion-operator-reference-target.t`,
  `spec/500-template-template-piecewise-partial-ordering.t`,
  `spec/500-type-pack-qualified-static-member-expansion.t`,
  `spec/500-unqualified-member-template-local-alias-deduction.t`.

### Next Checkpoint Group

Continue the remaining `general/300` and `spec/300` cases with the shared
dependent-return/`decltype` and explicit-overload candidate path: make
pack-aware integral expressions and dependent aliases resolve in typed state,
then preserve lookup context for hidden friends and no-eager class probes.
Validate that subgroup against its exact fixtures, the full PA22 report,
through-PA21, and file audit before bundling the remaining 100/200 ordering
band.

## Checkpoint 72 audit result — 2026-07-25

The Checkpoint 71 implementation was audited and its candidate-status,
generated-member lookup, and qualified constant-member paths were tightened
without changing the checkpoint baseline.  The complete current-PA failure
set remains 156 fixtures, grouped as follows:

- `general/300` + `spec/300` substitution/deferred lookup: **58**
  (`38 + 20`).
- `general/100` + `general/200` + `spec/100` + `spec/200` deduction and
  ordering: **46** (`14 + 13 + 6 + 13`).
- `general/400` + `general/500` + `spec/400` + `spec/500` aliases, owners,
  NTTPs, and late conversion cases: **52** (`15 + 26 + 2 + 9`).

These groups account for every fixture in the exact failure map above and
sum to 156.  The seven Checkpoint 71 fixtures remain passing.  The next
substantial checkpoint is the 58-fixture `general/300` + `spec/300` group,
with the 46-fixture 100/200 band bundled as the following checkpoint once
the dependent substitution path is complete.

## Checkpoint 73 scope — 2026-07-25

### Remaining Work Map

The live PA22 report still has 94/250 passing and 156 failing fixtures.  The
complete failure set, grouped by shared compiler behavior, is:

- **Basic deduction/defaults (general/100, 14):**
  `dependent-remove-reference-transform-forwarding`,
  `empty-pack-static-assert-trait-expansion`,
  `explicit-template-id-user-conversion-deduction`,
  `forwarding-reference-preserves-top-const-function-pointer`,
  `forwarding-reference-qualified-enumerator`,
  `function-parameter-empty-middle-pack-alias`,
  `function-template-elaborated-top-cv-deduction`,
  `function-template-fixed-over-trailing-pack-fallback`,
  `function-template-template-defaulted-argument-deduction`,
  `function-template-variadic-template-template-deduction`,
  `local-class-declval-explicit-template-id`,
  `qualified-alias-template-member-deduction`,
  `template-deduction-rejects-value-base-argument`,
  `type-pack-element-result-selects-copy-ctor`.

- **Partial ordering/default-tail deduction (general/200, 13):**
  `ambiguous-cv-pointer-partial-ordering-bad`,
  `class-template-partial-order-placeholder-argument`,
  `constructor-template-rvalue-beats-const-ref`,
  `empty-index-sequence-overload-order`,
  `function-pointer-vs-const-ref-partial-order`,
  `function-template-partial-order-class-template-cv`,
  `function-template-partial-order-const-pointer`,
  `function-template-trailing-pack-partial-order`,
  `inherited-constructor-template-forwarding`,
  `partial-order-synthetic-virtual-member-emission`,
  `partial-ordering-pointer-vs-value`,
  `partial-ordering-ref-vs-const-ref`,
  `range-array-reference-mutable-begin`.

- **Dependent substitution/lookup (general/300, 38):**
  `abstract-array-parameter-sfinae`,
  `alias-bool-explicit-pack-call-dependent-tag`,
  `alias-sfinae-function-pointer-deduction-key`,
  `alias-sfinae-inherited-member-value`,
  `base-qualified-template-value-arg-syntax`,
  `boost-enable-if-type-condition-static-keyword-overload`,
  `class-template-id-argument-no-eager-complete`,
  `constructor-template-keeps-ctor-refinement-viable`,
  `decltype-conditional-no-body-instantiation`,
  `decltype-default-type-arg-construction-sfinae`,
  `defaulted-sfinae-ctor-candidate-drop`,
  `dependent-enable-if-return-less-equal`,
  `dependent-enable-if-return-nontype-less-pack`,
  `destructor-template-id-sfinae`,
  `empty-pack-unknown-bound-array-lowir`,
  `explicit-function-template-pack-alias-sfinae`,
  `explicit-template-call-transitive-base-deduction`,
  `function-template-nested-alias-explicit-call`,
  `hidden-friend-dependent-return-specialization-scope`,
  `hidden-friend-sfinae-use-scope-shadowing`,
  `inherited-variable-template-enable-if-return`,
  `internal-remove-cvref-alias-sfinae`,
  `lazy-nested-member-class-instantiation`,
  `local-alias-explicit-template-pack-decltype`,
  `out-of-class-partial-owner-ctor-using-alias`,
  `pack-expanded-enable-if-member-value`,
  `qualified-alias-nontype-pack-function-deduction`,
  `qualified-alias-sfinae-function-pointer-deduction-key`,
  `qualified-rebind-detected-type-arg`,
  `recursive-streamable-sfinae-guard`,
  `single-element-detector-idiom-sfinae-false`,
  `static-member-template-function-pointer-nttp`,
  `structured-enable-if-sizeof-pack-value`,
  `unevaluated-sizeof-call-surrogates`,
  `using-declaration-imports-member-template-sfinae-shadow`,
  `using-directive-overloaded-function-template-arg`,
  `using-member-template-implicit-object-cv-overload`,
  `variable-template-detected-idiom-direct-arg`.

- **Late aliases/owners/NTTPs (general/400, 15):**
  `alias-template-function-argument-cv`,
  `bad-constructor-template-parameter-shadowing-target-aware`,
  `constructor-template-pack-before-defaulted-nontype`,
  `conversion-function-template-prefers-nontemplate`,
  `defaulted-pointer-nontype-cstyle-null`,
  `enum-nttp-cstyle-cast-default-rebind`,
  `function-assignment-invocable-and-helper`,
  `member-alias-template-template-partial-deduction-owner`,
  `object-pointer-nttp-address`,
  `object-pointer-nttp-rebound-member-template`,
  `pack-expansion-size-mismatch-sfinae`,
  `partial-specialization-inherited-constructor-template`,
  `static-data-nttp-pack-sizeof-bound`,
  `template-template-alias-default-arity-sfinae`,
  `unnamed-nontype-pack-static-enable-if-default`.

- **Late aliases/owners/packs (general/500, 26):**
  `adl-alias-return-operator-template`, `adl-explicit-function-template-id`,
  `alias-pack-enable-if-constexpr-constructor`,
  `alias-rebind-forwarding-nondependent-param`,
  `alias-template-template-defaulted-sfinae-canonical-args`,
  `async-initiate-dependent-return-sfinae`,
  `bool-alias-function-template-result-metadata`,
  `boost-mp11-conditional-alias-reference-set`,
  `constructor-pack-default-rewritten-pointer`,
  `constructor-sfinae-member-template-value`,
  `current-specialization-nontype-default-dependent`,
  `defaulted-nontype-qualified-alias-value`,
  `defaulted-pack-bool-short-circuit-sfinae`,
  `dependent-result-sizeof-sfinae-base`,
  `explicit-pack-deduced-pack-member-result`,
  `index-sequence-alias-constructor-deduction`,
  `inherited-constructor-template-member-alias-pack`,
  `member-template-dependent-owner-defaulted-sfinae`,
  `member-template-enable-if-redeclaration-overload`,
  `member-template-retained-dependent-param-candidate-drop`,
  `owner-enum-nontype-result-sfinae`,
  `pack-alias-functional-bool-trait-sfinae`,
  `partial-specialization-cv-qualifier-subset`,
  `short-circuit-alias-member-sfinae`, `sizeof-void-sfinae-fallback`,
  `weak-ptr-shared-ptr-template-ctor`.

- **Array bounds and nondeduced contexts (spec/100, 6):**
  `constructor-template-braced-array-bound-deduction`,
  `explicit-specialization-dependent-param-typedef`,
  `explicit-template-argument-overload-rejects-short-candidate`,
  `function-template-array-bound-braced-empty-argument`,
  `function-template-array-bound-only-deduction`,
  `function-template-array-parameter-string-literal`.

- **Ordering/nondeduced contexts (spec/200, 13):**
  `array-reference-cv-partial-ordering`,
  `constructor-template-qualified-nested-id-partial-ordering`,
  `defaulted-class-template-argument-prefix-deduction`,
  `dependent-specialized-default-arg-deduction`,
  `function-template-class-template-param-partial-order`,
  `function-template-fixed-parameter-default-tail-partial-order`,
  `function-template-partial-order-const-pointer`,
  `member-operator-fixed-tag-default-partial-order`,
  `member-template-explicit-pack-forward-call`,
  `member-template-nontype-param-shadows-inherited-value-sum`,
  `member-template-nontype-param-shadows-inherited-value`,
  `nondeduced-qualified-member-type-allows-conversion`,
  `overload-set-address-nondeduced-bad`.

- **Dependent substitution/lookup (spec/300, 20):**
  `constructor-forwarding-lvalue-beats-const-ref`,
  `constructor-template-const-ref-conversion`,
  `conversion-function-template-owner-result-copy-init`,
  `cross-specialization-converting-ctor-operator-template`,
  `current-specialization-constructor-template-canonical-owner`,
  `current-specialization-constructor-template-owner`,
  `decltype-call-substitution-failure-partial-specialization`,
  `detected-or-sfinae-fallback`, `expression-sfinae-decltype-conversion`,
  `expression-sfinae-decltype-member-comma`, `expression-sfinae-decltype`,
  `hidden-friend-template-pointer-sfinae-adl`,
  `out-of-class-sfinae-member-template-alias-body`,
  `out-of-class-sfinae-member-template-body`,
  `qualified-member-function-value-fallback-sfinae`,
  `sfinae-member-typedef-probe-no-body-instantiation`,
  `stream-insertion-decltype-overload-sfinae`,
  `template-id-direct-parameter-same-name-deduction`,
  `typedef-class-template-does-not-instantiate`, `void-t-detector`.

- **Late dependent conversion/NTTPs (spec/400, 2):**
  `dependent-decltype-member-template-conversion-operator`,
  `nontype-reference-argument`.

- **Late owners/aliases/template-template/pack cases (spec/500, 9):**
  `conversion-function-template-reference-conditional-auto-ref`,
  `conversion-function-template-same-name-target`,
  `defaulted-rebind-constructor-deduction`,
  `function-result-template-id-shadowed-argument`,
  `hidden-friend-query-free-decltype-noexcept`,
  `template-template-conversion-operator-reference-target`,
  `template-template-piecewise-partial-ordering`,
  `type-pack-qualified-static-member-expansion`,
  `unqualified-member-template-local-alias-deduction`.

### Checkpoint Scope

This checkpoint completes the shared dependent-expression substitution core
for the conditional and default-construction `decltype`, expression-SFINAE
member/comma and conversion, detected-or, member-typedef, stream-insertion,
and partial-specialization call probes.  The implementation covers typed
expression-result lookup, substitution-failure propagation, lazy
class-template references, placement-new `decltype` results, and concrete
member-owner routing.  The focused exit-status fixtures pass; the remaining
`spec/300-expression-sfinae-decltype` failure is an isolated LowIR naming and
constructor-emission mismatch, recorded for the next checkpoint.

### Checkpoint 73 / final result

Completed. The current PA22 report is 106/250, up 12 tests from the turn-start
baseline of 94.  The increment covers typed expression-result lookup,
class-conversion rejection, propagation of outer/member bindings,
non-materializing dependent-return SFINAE probes, ellipsis fallback, function
parameter packs, recursive trailing-return guarding, static-data owner
routing, concrete replay of evaluated `decltype`, placement-new result typing,
and active concrete member-owner routing.  The focused PA19/PA21 regression
fixtures and the scoped PA22 exit-status fixtures pass.

Validation completed: `make test-report-through-pa21` passed all 1850 tests;
the current PA22 report is 106/250; and the PA22 file audit passed with only
the repository's existing 10 structural warnings.

### Remaining Work Map (final)

The final report has 144 failures and 106 passes.  The remaining work is
grouped by the shared behavior bands below; the detailed fixture map above
remains the source-level inventory.

- `general/100` (14): forwarding/reference, pack/array, and
  template-template deduction cases.
- `general/200` (13): partial ordering and constructor/template fallback.
- `general/300` (35): remaining alias, owner, NTTP, SFINAE, and deduction
  interactions.
- `general/400` (15): later alias/owner and NTTP cases.
- `general/500` (25): later alias/owner, pack, and dependent lookup cases.
- `spec/100` (6): array bounds and non-deduced-context cases.
- `spec/200` (12): ordering and non-deduced-context cases.
- `spec/300` (13): remaining dependent conversion, lookup, body, and void
  detector cases.
- `spec/400` (2): dependent conversion and NTTP cases.
- `spec/500` (9): conversion, owner, template-template, and pack cases.

### Next Checkpoint Group

Take the remaining `general/300` and `spec/300` dependent lookup, owner,
alias, and expression-result cases, beginning with pure exit-status failures
and the isolated `expression-sfinae-decltype` LowIR mismatch.  Then move to
the general/spec 200 partial-ordering band if more work is needed.

### Checkpoint 73 audit result — 2026-07-25

The timeout-level audit fixes are complete.  The final active report is
**107/250**, with **143** failures, up from the turn-start baseline of 106;
there are no timeout failures.  PA1–PA21 remain **1850/1850**, and the
current file audit passes with the existing ten warnings.

### Remaining Work Map (refreshed)

The following is the complete current-PA failure set from the final report.
Each name is the fixture stem under `pa22/tests/<group>/<band>-<name>.t`.

- `general/100` (14):
  `dependent-remove-reference-transform-forwarding`,
  `empty-pack-static-assert-trait-expansion`,
  `explicit-template-id-user-conversion-deduction`,
  `forwarding-reference-preserves-top-const-function-pointer`,
  `forwarding-reference-qualified-enumerator`,
  `function-parameter-empty-middle-pack-alias`,
  `function-template-elaborated-top-cv-deduction`,
  `function-template-fixed-over-trailing-pack-fallback`,
  `function-template-template-defaulted-argument-deduction`,
  `function-template-variadic-template-template-deduction`,
  `local-class-declval-explicit-template-id`,
  `qualified-alias-template-member-deduction`,
  `template-deduction-rejects-value-base-argument`,
  `type-pack-element-result-selects-copy-ctor`.
- `general/200` (13):
  `ambiguous-cv-pointer-partial-ordering-bad`,
  `class-template-partial-order-placeholder-argument`,
  `constructor-template-rvalue-beats-const-ref`,
  `empty-index-sequence-overload-order`,
  `function-pointer-vs-const-ref-partial-order`,
  `function-template-partial-order-class-template-cv`,
  `function-template-partial-order-const-pointer`,
  `function-template-trailing-pack-partial-order`,
  `inherited-constructor-template-forwarding`,
  `partial-order-synthetic-virtual-member-emission`,
  `partial-ordering-pointer-vs-value`,
  `partial-ordering-ref-vs-const-ref`,
  `range-array-reference-mutable-begin`.
- `general/300` (34):
  `abstract-array-parameter-sfinae`,
  `alias-bool-explicit-pack-call-dependent-tag`,
  `alias-sfinae-function-pointer-deduction-key`,
  `alias-sfinae-inherited-member-value`,
  `base-qualified-template-value-arg-syntax`,
  `boost-enable-if-type-condition-static-keyword-overload`,
  `class-template-id-argument-no-eager-complete`,
  `constructor-template-keeps-ctor-refinement-viable`,
  `defaulted-sfinae-ctor-candidate-drop`,
  `dependent-enable-if-return-less-equal`,
  `dependent-enable-if-return-nontype-less-pack`,
  `destructor-template-id-sfinae`,
  `empty-pack-unknown-bound-array-lowir`,
  `explicit-template-call-transitive-base-deduction`,
  `function-template-nested-alias-explicit-call`,
  `hidden-friend-dependent-return-specialization-scope`,
  `inherited-variable-template-enable-if-return`,
  `internal-remove-cvref-alias-sfinae`,
  `lazy-nested-member-class-instantiation`,
  `local-alias-explicit-template-pack-decltype`,
  `out-of-class-partial-owner-ctor-using-alias`,
  `pack-expanded-enable-if-member-value`,
  `qualified-alias-nontype-pack-function-deduction`,
  `qualified-alias-sfinae-function-pointer-deduction-key`,
  `qualified-rebind-detected-type-arg`,
  `recursive-streamable-sfinae-guard`,
  `single-element-detector-idiom-sfinae-false`,
  `static-member-template-function-pointer-nttp`,
  `structured-enable-if-sizeof-pack-value`,
  `unevaluated-sizeof-call-surrogates`,
  `using-declaration-imports-member-template-sfinae-shadow`,
  `using-directive-overloaded-function-template-arg`,
  `using-member-template-implicit-object-cv-overload`,
  `variable-template-detected-idiom-direct-arg`.
- `general/400` (15):
  `alias-template-function-argument-cv`,
  `bad-constructor-template-parameter-shadowing-target-aware`,
  `constructor-template-pack-before-defaulted-nontype`,
  `conversion-function-template-prefers-nontemplate`,
  `defaulted-pointer-nontype-cstyle-null`,
  `enum-nttp-cstyle-cast-default-rebind`,
  `function-assignment-invocable-and-helper`,
  `member-alias-template-template-partial-deduction-owner`,
  `object-pointer-nttp-address`,
  `object-pointer-nttp-rebound-member-template`,
  `pack-expansion-size-mismatch-sfinae`,
  `partial-specialization-inherited-constructor-template`,
  `static-data-nttp-pack-sizeof-bound`,
  `template-template-alias-default-arity-sfinae`,
  `unnamed-nontype-pack-static-enable-if-default`.
- `general/500` (25):
  `adl-alias-return-operator-template`, `adl-explicit-function-template-id`,
  `alias-pack-enable-if-constexpr-constructor`,
  `alias-rebind-forwarding-nondependent-param`,
  `alias-template-template-defaulted-sfinae-canonical-args`,
  `async-initiate-dependent-return-sfinae`,
  `bool-alias-function-template-result-metadata`,
  `boost-mp11-conditional-alias-reference-set`,
  `constructor-pack-default-rewritten-pointer`,
  `constructor-sfinae-member-template-value`,
  `current-specialization-nontype-default-dependent`,
  `defaulted-nontype-qualified-alias-value`,
  `defaulted-pack-bool-short-circuit-sfinae`,
  `dependent-result-sizeof-sfinae-base`,
  `explicit-pack-deduced-pack-member-result`,
  `index-sequence-alias-constructor-deduction`,
  `inherited-constructor-template-member-alias-pack`,
  `member-template-dependent-owner-defaulted-sfinae`,
  `member-template-enable-if-redeclaration-overload`,
  `member-template-retained-dependent-param-candidate-drop`,
  `owner-enum-nontype-result-sfinae`,
  `partial-specialization-cv-qualifier-subset`,
  `short-circuit-alias-member-sfinae`, `sizeof-void-sfinae-fallback`,
  `weak-ptr-shared-ptr-template-ctor`.
- `spec/100` (6):
  `constructor-template-braced-array-bound-deduction`,
  `explicit-specialization-dependent-param-typedef`,
  `explicit-template-argument-overload-rejects-short-candidate`,
  `function-template-array-bound-braced-empty-argument`,
  `function-template-array-bound-only-deduction`,
  `function-template-array-parameter-string-literal`.
- `spec/200` (12):
  `array-reference-cv-partial-ordering`,
  `constructor-template-qualified-nested-id-partial-ordering`,
  `defaulted-class-template-argument-prefix-deduction`,
  `dependent-specialized-default-arg-deduction`,
  `function-template-class-template-param-partial-order`,
  `function-template-fixed-parameter-default-tail-partial-order`,
  `function-template-partial-order-const-pointer`,
  `member-operator-fixed-tag-default-partial-order`,
  `member-template-explicit-pack-forward-call`,
  `member-template-nontype-param-shadows-inherited-value-sum`,
  `nondeduced-qualified-member-type-allows-conversion`,
  `overload-set-address-nondeduced-bad`.
- `spec/300` (13):
  `constructor-forwarding-lvalue-beats-const-ref`,
  `constructor-template-const-ref-conversion`,
  `conversion-function-template-owner-result-copy-init`,
  `cross-specialization-converting-ctor-operator-template`,
  `current-specialization-constructor-template-canonical-owner`,
  `current-specialization-constructor-template-owner`,
  `expression-sfinae-decltype`,
  `out-of-class-sfinae-member-template-alias-body`,
  `out-of-class-sfinae-member-template-body`,
  `qualified-member-function-value-fallback-sfinae`,
  `template-id-direct-parameter-same-name-deduction`,
  `typedef-class-template-does-not-instantiate`, `void-t-detector`.
- `spec/400` (2):
  `dependent-decltype-member-template-conversion-operator`,
  `nontype-reference-argument`.
- `spec/500` (9):
  `conversion-function-template-reference-conditional-auto-ref`,
  `conversion-function-template-same-name-target`,
  `defaulted-rebind-constructor-deduction`,
  `function-result-template-id-shadowed-argument`,
  `hidden-friend-query-free-decltype-noexcept`,
  `template-template-conversion-operator-reference-target`,
  `template-template-piecewise-partial-ordering`,
  `type-pack-qualified-static-member-expansion`,
  `unqualified-member-template-local-alias-deduction`.

### Next substantial checkpoint

Bundle `general/300` (34), `spec/300` (13), and the small related `spec/400`
(2) group: dependent lookup, owner routing, expression-result substitution,
and conversion/NTTP probes.  Start with the pure exit-status failures and
`spec/300-expression-sfinae-decltype`; then take the general/spec 200 ordering
band.

## Checkpoint 75 scope — 2026-07-25 (before implementation)

### Remaining Work Map

The live PA22 report is **107/250**, with **143** failures and no timeout;
PA1–PA21 pass.  The complete failure set is the refreshed fixture inventory
above, grouped by shared behavior as follows: basic deduction/defaults
(`general/100`, 14), partial ordering and constructor fallback
(`general/200`, 13), dependent substitution/lookup (`general/300`, 34),
alias/owner/NTTP interactions (`general/400`, 15; `general/500`, 25),
array/non-deduced deduction (`spec/100`, 6), ordering and overload
participation (`spec/200`, 12), dependent conversion/lookup/SFINAE
(`spec/300`, 13), and the remaining dependent conversion/NTTP and
template-template cases (`spec/400`, 2; `spec/500`, 9).

### Checkpoint Scope

Complete the shared dependent-expression candidate path for the next
substantial group: preserve SFINAE state while resolving dependent aliases,
qualified member/owner lookup, defaulted `enable_if` and detected-idiom
arguments, and unevaluated `decltype`/conversion probes.  Ensure discarded
candidates do not materialize bodies or constructors, while viable concrete
results retain typed aliases and owner identity for LowIR.  Validate with the
pure exit-status cases in `general/300`, `spec/300`, and `spec/400`, the
isolated `spec/300-expression-sfinae-decltype` LowIR case, all PA22 tests,
through-PA21, and the file audit.  The remaining general/spec 100/200
deduction and ordering bands plus later general/spec 400/500 owner/NTTP cases
are explicit follow-up work if this scope does not finish the PA.

## Checkpoint 75 result — 2026-07-25

The implementation increment raised PA22 from **107/250** to **110/250**.
The authoritative current-PA report has no timeout; PA1–PA21 pass **1850/1850**
and the PA22 file audit passes.

Completed in this increment: dependent alias probes now reject failed member
substitutions without caching invalid generated entities; explicit using imports
can resolve direct class/alias definitions without perturbing function-template
lookup; qualified alias targets retain their typed owner; function-pointer
`decltype` casts are not mistaken for alias casts; macro string spellings are
preserved; variadic direct class temporaries use the address-passing ABI; and
constant-bool comparisons retain the expected LowIR conversion shape.  Focused
dependent-expression checks pass except for the qualified-cv specialization
identity LowIR case.

### Remaining Work Map

The live residual is **140/250 failures**, grouped by shared behavior:

- `general/100` **14** — basic deduction/default and pack cases (12 exit-status,
  2 LowIR).
- `general/200` **13** — partial ordering and constructor fallback (7 exit,
  6 LowIR).
- `general/300` **33** — dependent lookup, alias/member SFINAE, detected idioms,
  and lazy instantiation (28 exit, 5 LowIR).
- `general/400` **15** and `general/500` **25** — owner routing, NTTP,
  redeclaration, ADL, and later alias/constructor interactions.
- `spec/100` **6** and `spec/200` **12** — array/non-deduced deduction and
  ordering/overload participation.
- `spec/300` **11** and `spec/400` **2** — dependent conversion/member probes,
  constructor ownership, and NTTP reference handling.
- `spec/500` **9** — conversion operators, template-template ordering, and
  qualified pack expansion.

The closest residuals to this checkpoint are
`general/300-using-declaration-imports-member-template-sfinae-shadow`, the
qualified-alias-cv specialization identity LowIR case, and the remaining
dependent `enable_if`/member-body probes.

### Next Checkpoint Group

Start with the remaining `general/300` dependent candidate group, specifically
using-declaration member-template SFINAE, defaulted `enable_if`, lazy nested
class/variable-template probes, and qualified rebind cases; then bundle the
`spec/300` dependent constructor/conversion probes.  Keep the current PA1–PA21
gate and file audit as the validation boundary.

## Checkpoint 76 audit result — 2026-07-25

The audit of the Checkpoint 75 implementation is complete.  The checkpoint
preserves the PA1–PA21 result (**1850/1850**), preserves the current PA22
passing count at **110/250**, and passes the PA22 file audit.  The audit fixes
removed broad hard-error-to-candidate fallback catches, made explicit-using
lookup and alias-owner qualification typed, and introduced no timeout or
output shortcut.  The final active report has no timeout failures.

### Remaining Work Map

The complete residual is **140/250** fixtures, grouped directly from the final
`make test-report ACTIVE_TEST_REPORT_PAS='pa22'` report.  Counts in
parentheses are exit-status failures and LowIR comparison failures.

- `general/100` **14** (12 exit, 2 LowIR):
  `dependent-remove-reference-transform-forwarding`,
  `empty-pack-static-assert-trait-expansion`,
  `explicit-template-id-user-conversion-deduction`,
  `forwarding-reference-preserves-top-const-function-pointer`,
  `forwarding-reference-qualified-enumerator`,
  `function-parameter-empty-middle-pack-alias`,
  `function-template-elaborated-top-cv-deduction`,
  `function-template-fixed-over-trailing-pack-fallback`,
  `function-template-template-defaulted-argument-deduction`,
  `function-template-variadic-template-template-deduction`,
  `local-class-declval-explicit-template-id`,
  `qualified-alias-template-member-deduction`,
  `template-deduction-rejects-value-base-argument`,
  `type-pack-element-result-selects-copy-ctor`.
- `general/200` **13** (7 exit, 6 LowIR):
  `ambiguous-cv-pointer-partial-ordering-bad`,
  `class-template-partial-order-placeholder-argument`,
  `constructor-template-rvalue-beats-const-ref`,
  `empty-index-sequence-overload-order`,
  `function-pointer-vs-const-ref-partial-order`,
  `function-template-partial-order-class-template-cv`,
  `function-template-partial-order-const-pointer`,
  `function-template-trailing-pack-partial-order`,
  `inherited-constructor-template-forwarding`,
  `partial-order-synthetic-virtual-member-emission`,
  `partial-ordering-pointer-vs-value`,
  `partial-ordering-ref-vs-const-ref`,
  `range-array-reference-mutable-begin`.
- `general/300` **33** (28 exit, 5 LowIR):
  `abstract-array-parameter-sfinae`,
  `alias-bool-explicit-pack-call-dependent-tag`,
  `alias-sfinae-inherited-member-value`,
  `base-qualified-template-value-arg-syntax`,
  `boost-enable-if-type-condition-static-keyword-overload`,
  `class-template-id-argument-no-eager-complete`,
  `constructor-template-keeps-ctor-refinement-viable`,
  `defaulted-sfinae-ctor-candidate-drop`,
  `dependent-enable-if-return-less-equal`,
  `dependent-enable-if-return-nontype-less-pack`,
  `destructor-template-id-sfinae`,
  `empty-pack-unknown-bound-array-lowir`,
  `explicit-template-call-transitive-base-deduction`,
  `function-template-nested-alias-explicit-call`,
  `hidden-friend-dependent-return-specialization-scope`,
  `inherited-variable-template-enable-if-return`,
  `internal-remove-cvref-alias-sfinae`,
  `lazy-nested-member-class-instantiation`,
  `local-alias-explicit-template-pack-decltype`,
  `out-of-class-partial-owner-ctor-using-alias`,
  `pack-expanded-enable-if-member-value`,
  `qualified-alias-nontype-pack-function-deduction`,
  `qualified-alias-sfinae-function-pointer-deduction-key`,
  `qualified-rebind-detected-type-arg`,
  `recursive-streamable-sfinae-guard`,
  `single-element-detector-idiom-sfinae-false`,
  `static-member-template-function-pointer-nttp`,
  `structured-enable-if-sizeof-pack-value`,
  `unevaluated-sizeof-call-surrogates`,
  `using-declaration-imports-member-template-sfinae-shadow`,
  `using-directive-overloaded-function-template-arg`,
  `using-member-template-implicit-object-cv-overload`,
  `variable-template-detected-idiom-direct-arg`.
- `general/400` **15** (11 exit, 4 LowIR):
  `alias-template-function-argument-cv`,
  `bad-constructor-template-parameter-shadowing-target-aware`,
  `constructor-template-pack-before-defaulted-nontype`,
  `conversion-function-template-prefers-nontemplate`,
  `defaulted-pointer-nontype-cstyle-null`,
  `enum-nttp-cstyle-cast-default-rebind`,
  `function-assignment-invocable-and-helper`,
  `member-alias-template-template-partial-deduction-owner`,
  `object-pointer-nttp-address`,
  `object-pointer-nttp-rebound-member-template`,
  `pack-expansion-size-mismatch-sfinae`,
  `partial-specialization-inherited-constructor-template`,
  `static-data-nttp-pack-sizeof-bound`,
  `template-template-alias-default-arity-sfinae`,
  `unnamed-nontype-pack-static-enable-if-default`.
- `general/500` **25** (23 exit, 2 LowIR):
  `adl-alias-return-operator-template`, `adl-explicit-function-template-id`,
  `alias-pack-enable-if-constexpr-constructor`,
  `alias-rebind-forwarding-nondependent-param`,
  `alias-template-template-defaulted-sfinae-canonical-args`,
  `async-initiate-dependent-return-sfinae`,
  `bool-alias-function-template-result-metadata`,
  `boost-mp11-conditional-alias-reference-set`,
  `constructor-pack-default-rewritten-pointer`,
  `constructor-sfinae-member-template-value`,
  `current-specialization-nontype-default-dependent`,
  `defaulted-nontype-qualified-alias-value`,
  `defaulted-pack-bool-short-circuit-sfinae`,
  `dependent-result-sizeof-sfinae-base`,
  `explicit-pack-deduced-pack-member-result`,
  `index-sequence-alias-constructor-deduction`,
  `inherited-constructor-template-member-alias-pack`,
  `member-template-dependent-owner-defaulted-sfinae`,
  `member-template-enable-if-redeclaration-overload`,
  `member-template-retained-dependent-param-candidate-drop`,
  `owner-enum-nontype-result-sfinae`,
  `partial-specialization-cv-qualifier-subset`,
  `short-circuit-alias-member-sfinae`, `sizeof-void-sfinae-fallback`,
  `weak-ptr-shared-ptr-template-ctor`.
- `spec/100` **6** (2 exit, 4 LowIR):
  `constructor-template-braced-array-bound-deduction`,
  `explicit-specialization-dependent-param-typedef`,
  `explicit-template-argument-overload-rejects-short-candidate`,
  `function-template-array-bound-braced-empty-argument`,
  `function-template-array-bound-only-deduction`,
  `function-template-array-parameter-string-literal`.
- `spec/200` **12** (6 exit, 6 LowIR):
  `array-reference-cv-partial-ordering`,
  `constructor-template-qualified-nested-id-partial-ordering`,
  `defaulted-class-template-argument-prefix-deduction`,
  `dependent-specialized-default-arg-deduction`,
  `function-template-class-template-param-partial-order`,
  `function-template-fixed-parameter-default-tail-partial-order`,
  `function-template-partial-order-const-pointer`,
  `member-operator-fixed-tag-default-partial-order`,
  `member-template-explicit-pack-forward-call`,
  `member-template-nontype-param-shadows-inherited-value-sum`,
  `nondeduced-qualified-member-type-allows-conversion`,
  `overload-set-address-nondeduced-bad`.
- `spec/300` **11** (9 exit, 2 LowIR):
  `constructor-forwarding-lvalue-beats-const-ref`,
  `constructor-template-const-ref-conversion`,
  `conversion-function-template-owner-result-copy-init`,
  `cross-specialization-converting-ctor-operator-template`,
  `current-specialization-constructor-template-canonical-owner`,
  `current-specialization-constructor-template-owner`,
  `out-of-class-sfinae-member-template-alias-body`,
  `out-of-class-sfinae-member-template-body`,
  `qualified-member-function-value-fallback-sfinae`,
  `template-id-direct-parameter-same-name-deduction`,
  `typedef-class-template-does-not-instantiate`.
- `spec/400` **2** (1 exit, 1 LowIR):
  `dependent-decltype-member-template-conversion-operator`,
  `nontype-reference-argument`.
- `spec/500` **9** (8 exit, 1 LowIR):
  `conversion-function-template-reference-conditional-auto-ref`,
  `conversion-function-template-same-name-target`,
  `defaulted-rebind-constructor-deduction`,
  `function-result-template-id-shadowed-argument`,
  `hidden-friend-query-free-decltype-noexcept`,
  `template-template-conversion-operator-reference-target`,
  `template-template-piecewise-partial-ordering`,
  `type-pack-qualified-static-member-expansion`,
  `unqualified-member-template-local-alias-deduction`.

### Next substantial checkpoint group

Bundle the remaining `general/300` (33), `spec/300` (11), and `spec/400` (2)
fixtures into the next dependent-lookup/owner-routing checkpoint (46 total).
Start with the pure exit-status cases, then the two LowIR conversion/NTTP
cases; after that, take the general/spec 200 partial-ordering band.  Preserve
the same PA1–PA21 and file-audit validation boundary.

## Checkpoint 77 scope — 2026-07-25 (before implementation)

### Baseline and complete residual

The required current-PA report is **110/250** with **140** failures and no
timeout.  PA1–PA21 pass.  The complete current failure set, grouped by the
shared compiler behavior that owns the first observable failure, is:

- **Call deduction, arrays, defaults, and packs (14):**
  `general/100-dependent-remove-reference-transform-forwarding`,
  `general/100-empty-pack-static-assert-trait-expansion`,
  `general/100-explicit-template-id-user-conversion-deduction`,
  `general/100-forwarding-reference-preserves-top-const-function-pointer`,
  `general/100-forwarding-reference-qualified-enumerator`,
  `general/100-function-parameter-empty-middle-pack-alias`,
  `general/100-function-template-elaborated-top-cv-deduction`,
  `general/100-function-template-fixed-over-trailing-pack-fallback`,
  `general/100-function-template-template-defaulted-argument-deduction`,
  `general/100-function-template-variadic-template-template-deduction`,
  `general/100-local-class-declval-explicit-template-id`,
  `general/100-qualified-alias-template-member-deduction`,
  `general/100-template-deduction-rejects-value-base-argument`,
  `general/100-type-pack-element-result-selects-copy-ctor`.

- **Function-template partial ordering and overload ranking (13):**
  `general/200-ambiguous-cv-pointer-partial-ordering-bad`,
  `general/200-class-template-partial-order-placeholder-argument`,
  `general/200-constructor-template-rvalue-beats-const-ref`,
  `general/200-empty-index-sequence-overload-order`,
  `general/200-function-pointer-vs-const-ref-partial-order`,
  `general/200-function-template-partial-order-class-template-cv`,
  `general/200-function-template-partial-order-const-pointer`,
  `general/200-function-template-trailing-pack-partial-order`,
  `general/200-inherited-constructor-template-forwarding`,
  `general/200-partial-order-synthetic-virtual-member-emission`,
  `general/200-partial-ordering-pointer-vs-value`,
  `general/200-partial-ordering-ref-vs-const-ref`,
  `general/200-range-array-reference-mutable-begin`.

- **Dependent substitution, SFINAE, and deferred instantiation (46):**
  `general/300-abstract-array-parameter-sfinae`,
  `general/300-alias-bool-explicit-pack-call-dependent-tag`,
  `general/300-alias-sfinae-inherited-member-value`,
  `general/300-base-qualified-template-value-arg-syntax`,
  `general/300-boost-enable-if-type-condition-static-keyword-overload`,
  `general/300-class-template-id-argument-no-eager-complete`,
  `general/300-constructor-template-keeps-ctor-refinement-viable`,
  `general/300-defaulted-sfinae-ctor-candidate-drop`,
  `general/300-dependent-enable-if-return-less-equal`,
  `general/300-dependent-enable-if-return-nontype-less-pack`,
  `general/300-destructor-template-id-sfinae`,
  `general/300-empty-pack-unknown-bound-array-lowir`,
  `general/300-explicit-template-call-transitive-base-deduction`,
  `general/300-function-template-nested-alias-explicit-call`,
  `general/300-hidden-friend-dependent-return-specialization-scope`,
  `general/300-inherited-variable-template-enable-if-return`,
  `general/300-internal-remove-cvref-alias-sfinae`,
  `general/300-lazy-nested-member-class-instantiation`,
  `general/300-local-alias-explicit-template-pack-decltype`,
  `general/300-out-of-class-partial-owner-ctor-using-alias`,
  `general/300-pack-expanded-enable-if-member-value`,
  `general/300-qualified-alias-nontype-pack-function-deduction`,
  `general/300-qualified-alias-sfinae-function-pointer-deduction-key`,
  `general/300-qualified-rebind-detected-type-arg`,
  `general/300-recursive-streamable-sfinae-guard`,
  `general/300-single-element-detector-idiom-sfinae-false`,
  `general/300-static-member-template-function-pointer-nttp`,
  `general/300-structured-enable-if-sizeof-pack-value`,
  `general/300-unevaluated-sizeof-call-surrogates`,
  `general/300-using-declaration-imports-member-template-sfinae-shadow`,
  `general/300-using-directive-overloaded-function-template-arg`,
  `general/300-using-member-template-implicit-object-cv-overload`,
  `general/300-variable-template-detected-idiom-direct-arg`,
  `spec/300-constructor-forwarding-lvalue-beats-const-ref`,
  `spec/300-constructor-template-const-ref-conversion`,
  `spec/300-conversion-function-template-owner-result-copy-init`,
  `spec/300-cross-specialization-converting-ctor-operator-template`,
  `spec/300-current-specialization-constructor-template-canonical-owner`,
  `spec/300-current-specialization-constructor-template-owner`,
  `spec/300-out-of-class-sfinae-member-template-alias-body`,
  `spec/300-out-of-class-sfinae-member-template-body`,
  `spec/300-qualified-member-function-value-fallback-sfinae`,
  `spec/300-template-id-direct-parameter-same-name-deduction`,
  `spec/300-typedef-class-template-does-not-instantiate`,
  `spec/400-dependent-decltype-member-template-conversion-operator`,
  `spec/400-nontype-reference-argument`.

- **Alias/template-template/constructor/conversion/owner and typed NTTP
  propagation (49):**
  `general/400-alias-template-function-argument-cv`,
  `general/400-bad-constructor-template-parameter-shadowing-target-aware`,
  `general/400-constructor-template-pack-before-defaulted-nontype`,
  `general/400-conversion-function-template-prefers-nontemplate`,
  `general/400-defaulted-pointer-nontype-cstyle-null`,
  `general/400-enum-nttp-cstyle-cast-default-rebind`,
  `general/400-function-assignment-invocable-and-helper`,
  `general/400-member-alias-template-template-partial-deduction-owner`,
  `general/400-object-pointer-nttp-address`,
  `general/400-object-pointer-nttp-rebound-member-template`,
  `general/400-pack-expansion-size-mismatch-sfinae`,
  `general/400-partial-specialization-inherited-constructor-template`,
  `general/400-static-data-nttp-pack-sizeof-bound`,
  `general/400-template-template-alias-default-arity-sfinae`,
  `general/400-unnamed-nontype-pack-static-enable-if-default`,
  `general/500-adl-alias-return-operator-template`,
  `general/500-adl-explicit-function-template-id`,
  `general/500-alias-pack-enable-if-constexpr-constructor`,
  `general/500-alias-rebind-forwarding-nondependent-param`,
  `general/500-alias-template-template-defaulted-sfinae-canonical-args`,
  `general/500-async-initiate-dependent-return-sfinae`,
  `general/500-bool-alias-function-template-result-metadata`,
  `general/500-boost-mp11-conditional-alias-reference-set`,
  `general/500-constructor-pack-default-rewritten-pointer`,
  `general/500-constructor-sfinae-member-template-value`,
  `general/500-current-specialization-nontype-default-dependent`,
  `general/500-defaulted-nontype-qualified-alias-value`,
  `general/500-defaulted-pack-bool-short-circuit-sfinae`,
  `general/500-dependent-result-sizeof-sfinae-base`,
  `general/500-explicit-pack-deduced-pack-member-result`,
  `general/500-index-sequence-alias-constructor-deduction`,
  `general/500-inherited-constructor-template-member-alias-pack`,
  `general/500-member-template-dependent-owner-defaulted-sfinae`,
  `general/500-member-template-enable-if-redeclaration-overload`,
  `general/500-member-template-retained-dependent-param-candidate-drop`,
  `general/500-owner-enum-nontype-result-sfinae`,
  `general/500-partial-specialization-cv-qualifier-subset`,
  `general/500-short-circuit-alias-member-sfinae`,
  `general/500-sizeof-void-sfinae-fallback`,
  `general/500-weak-ptr-shared-ptr-template-ctor`,
  `spec/500-conversion-function-template-reference-conditional-auto-ref`,
  `spec/500-conversion-function-template-same-name-target`,
  `spec/500-defaulted-rebind-constructor-deduction`,
  `spec/500-function-result-template-id-shadowed-argument`,
  `spec/500-hidden-friend-query-free-decltype-noexcept`,
  `spec/500-template-template-conversion-operator-reference-target`,
  `spec/500-template-template-piecewise-partial-ordering`,
  `spec/500-type-pack-qualified-static-member-expansion`,
  `spec/500-unqualified-member-template-local-alias-deduction`.

- **Array/non-deduced and defaulted ordering edge cases (18):**
  `spec/100-constructor-template-braced-array-bound-deduction`,
  `spec/100-explicit-specialization-dependent-param-typedef`,
  `spec/100-explicit-template-argument-overload-rejects-short-candidate`,
  `spec/100-function-template-array-bound-braced-empty-argument`,
  `spec/100-function-template-array-bound-only-deduction`,
  `spec/100-function-template-array-parameter-string-literal`,
  `spec/200-array-reference-cv-partial-ordering`,
  `spec/200-constructor-template-qualified-nested-id-partial-ordering`,
  `spec/200-defaulted-class-template-argument-prefix-deduction`,
  `spec/200-dependent-specialized-default-arg-deduction`,
  `spec/200-function-template-class-template-param-partial-order`,
  `spec/200-function-template-fixed-parameter-default-tail-partial-order`,
  `spec/200-function-template-partial-order-const-pointer`,
  `spec/200-member-operator-fixed-tag-default-partial-order`,
  `spec/200-member-template-explicit-pack-forward-call`,
  `spec/200-member-template-nontype-param-shadows-inherited-value-sum`,
  `spec/200-nondeduced-qualified-member-type-allows-conversion`,
  `spec/200-overload-set-address-nondeduced-bad`.

### Checkpoint Scope

Complete the typed dependent-candidate path for **no-eager instantiation and
dependent return/SFINAE probes**: class-template arguments used only as
unevaluated or inherited base facts must remain lazy; dependent aliases,
`enable_if` return/default parameters, `decltype`/destructor probes, and
qualified member-template calls must convert failed substitution into a
discarded candidate; successful replay must retain the concrete owner and
result type.  The focused scope is the no-eager/dependent-return subset of
the 46-fixture dependent group: `general/300-class-template-id-argument-no-
eager-complete`, `general/300-dependent-enable-if-return-less-equal`,
`general/300-dependent-enable-if-return-nontype-less-pack`,
`general/300-destructor-template-id-sfinae`,
`general/300-lazy-nested-member-class-instantiation`,
`general/300-unevaluated-sizeof-call-surrogates`,
`spec/300-decltype-call-substitution-failure-partial-specialization` when
exposed by the same path, `spec/300-typedef-class-template-does-not-
instantiate`, and the related typed LowIR cases.  This is a real semantic
boundary, not a fixture-specific exception: the same candidate state must be
used for ordinary calls and member/constructor replay.

Validation for this checkpoint is the focused group, the full PA22 report,
the through-PA21 report, and the PA22 source audit.  The remaining call and
partial-ordering band, alias/owner/NTTP band, and unrelated conversion cases
remain explicit follow-up groups; the next group is the rest of dependent
SFINAE/lookup, bundled with any newly exposed owner-routing cases.

## Checkpoint 77 final result — 2026-07-25

The safe increment completed at **114/250**, up from the turn-start baseline
of **110/250**; the residual is **136** failures.  The through-PA21 report is
clean (**1850/1850**) and the PA22 source audit passes.  The increment covers
dependent relational `<`/`<=` parsing and constant evaluation, typed rejection
of unavailable explicit function results, temporary-callable and constructed
call result typing, destructor decltype typing, and physical ownership for
inline-namespace template arguments.  The focused probes for dependent
`enable_if` return comparisons, unevaluated call/constructor sizeof, destructor
SFINAE, and decltype substitution now pass; nested no-eager class/typedef
materialization remains follow-up work.

### Remaining Work Map (final)

- **Call deduction/arrays/defaults/packs:** 14 general/100 failures.
- **Partial ordering and overload ranking:** 13 general/200 plus 12 spec/200.
- **Dependent substitution/SFINAE/lookup:** 29 general/300, 11 spec/300,
  and 2 spec/400.
- **Alias/owner/typed NTTP and late conversion behavior:** 15 general/400,
  25 general/500, and 9 spec/500.
- **Array/non-deduced/defaulted edges:** 6 spec/100 failures.

### Next Checkpoint Group (final)

Take the 42 dependent substitution/lookup cases (general/300 plus spec/300
and spec/400), including the deferred nested-class/typedef cases.  Extend the
typed candidate state through qualified lookup, no-eager class materialization,
constructor viability, and out-of-class owner replay, then rerun the focused
group, full PA22 report, through-PA21 report, and source audit.

## Checkpoint 78 scope — 2026-07-25 (before implementation)

The complete current-PA report before this increment was **114/250**, with
**136** failures and no timeout.  The residual map was refreshed before
implementation: general/100 **14**, general/200 **13**, general/300 **33**,
general/400 **15**, general/500 **25**, spec/100 **6**, spec/200 **12**,
spec/300 **11**, spec/400 **2**, and spec/500 **9**.

Selected checkpoint scope: typed lazy class materialization.  A class template
used only as a typedef/type argument must not eagerly instantiate its body;
nested classes referenced only from function bodies must remain lazy; and a
later qualified member use must promote the cached forward state to a complete
class.  This covers the shared behavior behind
`general/300-class-template-id-argument-no-eager-complete`,
`general/300-lazy-nested-member-class-instantiation`, and
`spec/300-typedef-class-template-does-not-instantiate`, with the dependent
type-argument replay cases in the focused substitution group as regression
coverage.

## Checkpoint 78 result — 2026-07-25

The scope is complete at **117/250**, up three tests from the 114/250
checkpoint baseline, with no newly failing PA22 tests.  The three direct fixes
are the no-eager class-template-id argument case, lazy nested member-class
materialization, and typedef-only class-template use.  The implementation
keeps deferred class state and promotion state in typed template-expander
collections, suppresses only function-body references when deciding whether a
nested class is needed, and preserves dependent type arguments until their
concrete owner is known.  The focused validation for this scope passes **8/8**.
The earlier PA1–PA21 report passes **1850/1850**, and the PA22 source audit
passes.  The final required PA22 report remains **117/250** with no new
failures relative to the 114/250 checkpoint baseline.

### Remaining Work Map

- **Call deduction, arrays, defaults, and packs:** 14 general/100 failures.
- **Partial ordering and overload ranking:** 13 general/200 and 12 spec/200.
- **Dependent substitution, SFINAE, lookup, and owner replay:** 30
  general/300, 11 spec/300, and 2 spec/400.
- **Alias, constructor/conversion, owner, and typed NTTP behavior:** 15
  general/400, 25 general/500, and 9 spec/500.
- **Array/non-deduced/defaulted edges:** 6 spec/100.

The next checkpoint group is the remaining out-of-class member-template
SFINAE and concrete-owner replay cases, bundled with the closely related
constructor-owner cases when they share the same qualified lookup path.  The
required validation boundary remains the focused group, full PA22 report,
through-PA21 report, and PA22 source audit.

## Checkpoint 79 scope — 2026-07-25 (before implementation)

The complete current-PA report was refreshed at **117/250**, leaving **133**
failures.  The grouped Remaining Work Map is: call deduction/arrays/defaults/
packs **14** general/100; partial ordering and overload ranking **13**
general/200 plus **12** spec/200; dependent substitution/SFINAE/lookup/owner
replay **30** general/300, **11** spec/300, and **2** spec/400; alias,
constructor/conversion, owner, and typed NTTP behavior **15** general/400,
**25** general/500, and **9** spec/500; and array/non-deduced/defaulted edges
**6** spec/100.

Selected scope: route out-of-class member-template declarations and
constructor definitions through the concrete enclosing specialization while
preserving substitution failure as candidate rejection.  The focused fixtures
are `spec/300-out-of-class-sfinae-member-template-body`,
`spec/300-out-of-class-sfinae-member-template-alias-body`,
`general/300-out-of-class-partial-owner-ctor-using-alias`,
`spec/300-current-specialization-constructor-template-owner`, and
`spec/300-current-specialization-constructor-template-canonical-owner`.
The shared behavior is typed owner identity and member-body replay, including
the correct `this`/nested-alias context; it is not a test-specific diagnostic
change.  Validation for this checkpoint covers these five tests, the full
PA22 report, through-PA21, and the source audit.

## Checkpoint 79 result — 2026-07-25

The selected owner-replay scope is complete.  The final PA22 report is
**123/250**, up six from the 117/250 checkpoint baseline, with no newly
failing PA22 fixture names.  The fixed fixtures are the two out-of-class
SFINAE member-template bodies, the partial-owner constructor using an alias,
both current-specialization constructor-owner cases, and the member-template
enable-if redeclaration overload case.  The focused five-fixture check passes
**5/5**; through-PA21 passes **1850/1850**; and the PA22 source audit passes
with the same 10 pre-existing warnings.

The increment keeps in-class defaults in a typed inference copy for matching
out-of-class definitions, searches declarations across filtered owner
candidate sets, preserves current-specialization owner spelling, and ranks
member candidates by structural parameter specialization before the existing
template-parameter-count tie-breaker.  Static member replay also retains the
absence of an implicit `this`, while generated parameter side tables rewrite
only types containing active substitution identifiers.  These changes cover
the shared owner, overload, and body-replay behavior rather than individual
diagnostics.

### Remaining Work Map

- **Call deduction, arrays, defaults, and packs:** 14 general/100 failures.
- **Partial ordering and overload ranking:** 13 general/200 and 12 spec/200.
- **Dependent substitution, SFINAE, lookup, and owner replay:** 29
  general/300, 7 spec/300, and 2 spec/400.
- **Alias, constructor/conversion, owner, and typed NTTP behavior:** 15
  general/400, 24 general/500, and 9 spec/500.
- **Array/non-deduced/defaulted edges:** 6 spec/100.

The next checkpoint group is the remaining dependent lookup/SFINAE band,
bundled with qualified member calls and defaulted enable-if candidates that
share the same typed substitution state.  After that, take the remaining
function-template partial-ordering cases.  Each checkpoint continues to use
the focused group, full PA22 report, through-PA21 report, and source audit as
its validation boundary.

## Checkpoint 80 scope — 2026-07-25 (before implementation)

The current PA22 report was refreshed at **123/250**, leaving **127**
failures (**94** exit-status failures and **33** LowIR comparisons).  The
grouped Remaining Work Map is: call deduction/arrays/defaults/packs **14**
general/100; function-template partial ordering and overload ranking **13**
general/200 plus **12** spec/200; dependent substitution/SFINAE/lookup/owner
replay **29** general/300, **7** spec/300, and **2** spec/400; alias,
constructor/conversion, owner, and typed NTTP behavior **15** general/400,
**24** general/500, and **9** spec/500; and array/non-deduced/defaulted edges
**6** spec/100.  Some fixtures overlap buckets; the listed bucket is their
primary planning owner.

Selected scope: make free-function template candidate selection preserve the
typed partial-ordering distinction between bare parameters and pointer,
reference, and nested class-template patterns, including defaulted trailing
parameters or packs.  The focused fixtures are
`general/200-function-pointer-vs-const-ref-partial-order`,
`general/200-partial-ordering-pointer-vs-value`,
`general/200-function-template-partial-order-const-pointer`,
`general/200-function-template-partial-order-class-template-cv`,
`general/200-class-template-partial-order-placeholder-argument`,
`general/200-empty-index-sequence-overload-order`,
`spec/200-function-template-partial-order-const-pointer`, and
`spec/200-function-template-class-template-param-partial-order`.
The shared behavior is candidate ranking after successful typed deduction; it
must retain ambiguity and substitution failure rather than hardcoding result
values.  Validation covers this focused group, the full PA22 report,
through-PA21, and the PA22 source audit.

## Checkpoint 80 result — 2026-07-25

The core free-function partial-ordering increment is complete at **129/250**,
up six from the 123/250 checkpoint baseline.  The fixed fixtures are
`general/200-class-template-partial-order-placeholder-argument`,
`general/200-empty-index-sequence-overload-order`,
`general/200-function-template-partial-order-class-template-cv`,
`general/200-function-template-partial-order-const-pointer`,
`general/200-partial-ordering-pointer-vs-value`, and
`spec/200-function-template-partial-order-const-pointer`.  The focused group
is therefore **6/8**: the function-pointer-vs-const-ref case reaches the
correct ranked candidate but still fails PA14 function-pointer type
materialization, while the class-template-parameter case has the correct
selection but a return-object/default-constructor LowIR mismatch.

The implementation compares typed parameter patterns through the existing
substitution matcher, so bare parameters lose to pointer/reference and nested
class-template patterns only when deduction establishes a strict ordering.
It also preserves source-order lookup for an ordinary function body when the
whole-file collection has indexed later templates; this keeps the PA18
nondependent-name-binding behavior intact.  No new PA22 failure names were
introduced: the current residual is **121** fixtures.  Through-PA21 passes
**1850/1850**, and the PA22 source audit passes with the same 10 pre-existing
warnings.

### Remaining Work Map

- **Call deduction, arrays, defaults, and packs:** 14 general/100 failures.
- **Function-template partial ordering and overload ranking:** 8 general/200
  and 11 spec/200 failures, including the two focused residuals above and
  the default-tail/trailing-pack cases.
- **Dependent substitution, SFINAE, lookup, and owner replay:** 26
  general/300, 6 spec/300, and 2 spec/400 failures.
- **Alias, constructor/conversion, owner, and typed NTTP behavior:** 15
  general/400, 24 general/500, and 9 spec/500 failures.
- **Array/non-deduced/defaulted edges:** 6 spec/100 failures.

### Next Checkpoint Group

Complete the remaining function-template ranking band, starting with
defaulted trailing parameters and packs, reference-vs-const-reference
ordering, and the function-pointer/function-reference pattern path.  Bundle
the return-object/default-constructor LowIR repair only where it is the same
selected-candidate lowering path.  Then resume the dependent SFINAE/qualified
lookup band.  The next validation boundary remains the focused ranking group,
the full PA22 report, through-PA21, and the PA22 source audit.

## Checkpoint 81 scope — 2026-07-25 (before implementation)

The refreshed current-PA report remains **129/250**, with **121** failures:
general/100 **14**, general/200 **8**, general/300 **26**, general/400 **15**,
general/500 **24**, spec/100 **6**, spec/200 **11**, spec/300 **6**,
spec/400 **2**, and spec/500 **9**.  The complete names are captured by the
current report; the grouped map above remains the planning partition.

Selected scope: complete the shared function/reference type path at the
boundary between PA18 deduction and PA14 lowering, and preserve typed
construction of aggregate return objects from instantiated function
templates.  The focused fixtures are
`general/200-function-pointer-vs-const-ref-partial-order`,
`general/200-function-template-trailing-pack-partial-order`,
`general/200-partial-ordering-ref-vs-const-ref`,
`spec/200-function-template-class-template-param-partial-order`, and
`spec/200-function-template-fixed-parameter-default-tail-partial-order`.
The first three must retain function/reference structure through deduction,
candidate ranking, and generated function types; the last two exercise the
same indirect-result construction path after the correct candidate is chosen.
The implementation must use typed function signatures, substitution state,
and constructor records, not fixture-specific symbol or result selection.

Validation for this checkpoint covers the focused group, the full PA22
report, the through-PA21 report, and the PA22 source audit.

## Checkpoint 81 result — 2026-07-25

The selected scope is complete.  The final PA22 report is **134/250**, up
five from the refreshed 129/250 checkpoint baseline and above the reported
turn-start baseline; the five focused fixtures pass and no new PA22 failure
names were introduced.  The increment preserves direct and outer reference
layers around function types, gives ABI spelling a typed direct-function path,
keeps function-parameter pack arguments available during unevaluated call
lookup, and ranks viable call candidates using value category, cv, fixedness,
and existing partial ordering.  It also keeps overload identities distinct,
limits function-body context replay to the correct declarator/trailing-return
nodes, and materializes aggregate return construction and sizeof addresses
only for the corresponding typed array/scalar cases.

The six earlier-PA regressions exposed during the checkpoint are repaired:
the two PA15 class-sizeof cases, two PA18 lookup/context cases, and two PA21
function-type/partial-specialization cases.  Through-PA21 passes **1850/1850**.
The PA22 source file audit passes with warnings only.

### Remaining Work Map

- **Call deduction and argument normalization:** 14 general/100 failures.
- **Remaining partial ordering and overload ranking:** 5 general/200 and
  9 spec/200 failures.
- **Dependent substitution, SFINAE, lookup, and owner replay:** 26
  general/300, 6 spec/300, and 2 spec/400 failures.
- **Alias, constructor/conversion, owner, and typed NTTP behavior:** 15
  general/400, 24 general/500, and 9 spec/500 failures.
- **Array/non-deduced/defaulted edges:** 6 spec/100 failures.

The residual report has **116** failures and no newly introduced names when
compared with the pre-checkpoint failure set; the five focused names are the
only names removed.

### Next Checkpoint Group

Take the remaining 14 partial-ordering/overload-ranking cases, starting with
the general/200 call-viability and synthetic-member cases, then the spec/200
defaulted, array-reference, qualified, and member-template ordering cases.
Keep candidate rejection and source-order lookup typed while extending the
same ranking path.  Validate with the focused group, full PA22 report,
through-PA21, and the PA22 source audit.

## Checkpoint 82 scope — 2026-07-25 (before implementation)

The refreshed PA22 report is **134/250**, leaving **116** failures with no
timeout.  The complete failure set is partitioned as follows: general/100
**14**, general/200 **5**, general/300 **26**, general/400 **15**, general/500
**24**, spec/100 **6**, spec/200 **9**, spec/300 **6**, spec/400 **2**, and
spec/500 **9**.  The general/200 and spec/200 residuals are the remaining
partial-ordering band; the other buckets are retained as explicit follow-up
groups rather than omitted from the map.

Selected scope: repair dependent free-function lookup and typed candidate
materialization for the shared call path behind
`general/200-range-array-reference-mutable-begin`,
`spec/200-array-reference-cv-partial-ordering`, and
`spec/200-nondeduced-qualified-member-type-allows-conversion`.  Each currently
fails before overload deduction with an unknown `find_not`, qualified `begin`,
or `next` expression.  The scope covers lookup context/qualification,
function-template visibility, array-reference and nondeduced parameter
deduction, and replay of the selected typed result; it must preserve ordinary
SFINAE rejection and must not hardcode any callee or return value.

Validation for this checkpoint covers the three focused fixtures, the full
PA22 report, through-PA21, and the PA22 source audit.

## Checkpoint 85 result and next scope — 2026-07-26

The typed pack/forwarding checkpoint is complete.  Empty-pack static-assert
expansion, concrete pack-element replay with copy construction, and dependent
`remove_reference_t` forwarding now pass; the same increment also repairs
qualified alias deduction, value-base SFINAE rejection, empty index-sequence
ordering, inherited-constructor forwarding, and symmetric cv-pointer
partial-ordering ambiguity.  The current report is **153/250**, up from the
turn-start **110/250**; through-PA21 is **1850/1850**, and the PA22 source
audit passes.

### Remaining Work Map

- **general/100 — 4:** three LowIR parity differences and the
  array-reference/default-argument replay timeout.
- **general/200 — 1:** synthetic virtual-member LowIR parity.
- **general/300 — 25:** qualified lookup, dependent alias/SFINAE, constructor,
  and owner replay.
- **general/400 — 15:** non-type, template-template, alias, and constructor
  replay.
- **general/500 — 23:** owner, conversion, alias, and dependent-result
  behavior.
- **spec/100 — 5; spec/200 — 7; spec/300 — 6; spec/400 — 2; spec/500 — 9:**
  remaining array/specialization, ordering/default, conversion/SFINAE,
  decltype/reference, and template-template/non-type edges.

The remaining **97** failures are concentrated in typed replay and expected
LowIR ordering/metadata parity; no earlier-assignment regressions remain.

Next checkpoint scope: finish the residual general/100+200 parity band by
isolating the array-reference replay recursion and normalizing constant-index,
temporary numbering, and synthetic virtual-constructor emission.  Validate
that five-fixture group with the full PA22 report, through-PA21, and the PA22
source audit.

## Checkpoint 85 result and next scope — 2026-07-26

The selected pack/forwarding slice is implemented.  Empty type-pack static
assertions now disappear after a known-true materialization, type-pack
element results retain their concrete copy-constructor type, and forwarding
references deduce the underlying rvalue type or the lvalue reference type as
appropriate.  Dependent `remove_reference_t` return types are rewritten from
typed substitutions, function-template expressions in synthetic direct
initializers are replayed through call deduction, and constructor
mem-initializers materialize selected member-template constructors.  The
materialized static-assert check is owned by the rewrite-helper module so the
source-size audit remains within its limit.  Friend declarations are excluded
from synthetic-initializer rewriting, preserving the PA21 friend-access path.

The required PA22 report is **145/250**, up from the turn-start **110/250**;
the residual set is **105** fixtures, including one timeout.  Its grouped
remaining map is: general/100 **8**, general/200 **4**, general/300 **26**,
general/400 **15**, general/500 **23**, spec/100 **5**, spec/200 **7**,
spec/300 **6**, spec/400 **2**, and spec/500 **9**.  The remaining behaviors
are primarily explicit/defaulted deduction and LowIR parity in general/100,
partial ordering and constructor ranking in general/200, dependent
substitution/SFINAE and deferred lookup in general/300, and alias, owner,
conversion, pack, and typed-NTTP replay in the 400/500 and spec bands.

Validation completed: the three focused PA22 fixtures and the PA21 friend
fixture compile successfully; `make test-report ACTIVE_TEST_REPORT_PAS='pa22'`
reports 145/250; `make test-report-through-pa21` passes **1850/1850**; and
`perl scripts/cppgm_file_audit.pl --stage pa22 --paths dev/src` passes with
the repository's existing warnings.

Next checkpoint scope: bundle the remaining **general/100 + general/200
(12-fixture)** deduction and overload-ordering group.  Preserve typed
top-level cv/array/default and pack deduction while making partial ordering,
ambiguity, and constructor ranking use the same candidate facts; validate the
focused group, the full PA22 report, through-PA21, and the source audit.

## Checkpoint 86 scope — 2026-07-26 (before implementation)

The current required report was refreshed from commit `e01518f`: **145/250**,
with **105** residual fixtures and one timeout.  The complete failure set is
grouped by primary shared behavior as follows: general/100 **8** (explicit
conversion/LowIR parity, top-level cv and array/default deduction, local
`decltype`, qualified aliases, and a value-base rejection); general/200 **4**
(cv-pointer ambiguity, empty-pack overload order, inherited constructor
visibility, and synthetic virtual-member emission); general/300 **26**
(dependent lookup, SFINAE, aliases, and deferred materialization);
general/400 **15** (typed NTTPs, aliases, conversions, and constructor replay);
general/500 **23** (owners, ADL, aliases, packs, conversions, and dependent
results); spec/100 **5**, spec/200 **7**, spec/300 **6**, spec/400 **2**, and
spec/500 **9** (the remaining array, ordering, conversion, decltype,
template-template, and reference-NTTP edges).  The exact current fixture
names are the 105 `ERROR` entries in the refreshed PA22 report; no fixture is
omitted from these buckets.

### Remaining Work Map

- **general/100 (8):**
  `explicit-template-id-user-conversion-deduction`,
  `function-parameter-empty-middle-pack-alias`,
  `function-template-elaborated-top-cv-deduction`,
  `function-template-fixed-over-trailing-pack-fallback`,
  `local-class-declval-explicit-template-id`,
  `qualified-alias-template-member-deduction`,
  `template-array-reference-cv-default-arg`, and
  `template-deduction-rejects-value-base-argument`.
- **general/200 (4):** `ambiguous-cv-pointer-partial-ordering-bad`,
  `empty-index-sequence-overload-order`,
  `inherited-constructor-template-forwarding`, and
  `partial-order-synthetic-virtual-member-emission`.
- **general/300 (26), general/400 (15), general/500 (23), and
  spec/100/200/300/400/500 (29):** retain the current report’s complete
  dependent-substitution, alias/owner/conversion, typed-NTTP, and
  specification-edge residuals as the follow-up groups.

### Checkpoint Scope

Implement the complete 12-fixture general/100+200 band.  The compiler must
preserve typed top-level cv/array/default facts during deduction, defer
function-template and qualified-alias materialization until call context is
known, keep local-class and `decltype` expression types tied to typed callable
facts, reject value-base template deductions through SFINAE, and select empty
pack/constructor/partial-order candidates using the same candidate state.
The scope includes LowIR parity for the two semantically compiling fixtures
and must preserve the already passing pack/forwarding checkpoint.  Validate
focused fixtures, the full PA22 report, through-PA21, and the PA22 source
audit.

## Checkpoint 82 result and next scope — 2026-07-25

The checkpoint implementation passed all three focused fixtures and raised
the full PA22 report from **134/250** to **138/250**.  It also repaired the
related `spec/100-function-template-array-bound-only-deduction` fixture.  The
array-reference group now covers top-level array binding, reference-array
substitution, by-value array-to-pointer adjustment, nondeduced qualified
traits, and referenced-array decay in LowIR.

The refreshed remaining map is **112** failures: general/100 **14**,
general/200 **4**, general/300 **26**, general/400 **15**, general/500 **24**,
spec/100 **5**, spec/200 **7**, spec/300 **6**, spec/400 **2**, and spec/500
**9**.  The detailed names remain in the report-derived map for this
checkpoint; the dominant behaviors are forwarding and template-template/pack
deduction, dependent alias/SFINAE replay, constructor/member candidate
materialization, and expected LowIR metadata or overload emission.

Next checkpoint scope: repair the shared forwarding-reference and function
template argument replay path for the basic general/100 group, beginning with
`general/100-forwarding-reference-qualified-enumerator`,
`general/100-forwarding-reference-preserves-top-const-function-pointer`, and
`general/100-dependent-remove-reference-transform-forwarding`.  This scope
covers lvalue/rvalue reference typed facts, top-level const and function
pointer substitution, dependent alias removal, and ordinary SFINAE rejection;
it will be validated against the focused fixtures, the full PA22 report,
through-PA21, and the source audit.

## Checkpoint 83 scope — 2026-07-25 (before implementation)

The complete failure set was refreshed before this increment from the active
report at **140/250** and grouped as: general/100 **12**, general/200 **4**,
general/300 **26**, general/400 **15**, general/500 **24**, spec/100 **5**,
spec/200 **7**, spec/300 **6**, spec/400 **2**, and spec/500 **9**.  The
remaining behaviors are call-argument normalization and packs; partial
ordering; dependent lookup/SFINAE; constructor, alias, conversion, and owner
replay; and typed non-type argument lowering.

Selected scope: finish the explicit-template call path for
`general/100-explicit-template-id-user-conversion-deduction`,
`general/100-function-template-template-defaulted-argument-deduction`,
`general/100-function-template-variadic-template-template-deduction`,
`general/500-adl-explicit-function-template-id`, and
`general/300-explicit-template-call-transitive-base-deduction`.  This covers
explicit prefix deduction through a user conversion, defaulted and variadic
template-template parameters, using-directive/ADL visibility, and recursive
typed matching through instantiated transitive bases.  The same checkpoint
also covers the shared special-member body lookup and discarded scalar
reference-return lowering exposed while validating those calls.  It must
preserve typed substitution and candidate state rather than select fixture
specific symbols.

Validation for this checkpoint covers all five focused fixtures with their
full LowIR comparisons, the required PA22 report, through-PA21, and the PA22
source audit.

## Checkpoint 83 result and next scope — 2026-07-25

The five focused fixtures now pass their complete comparisons.  Explicit
template ids retain conversion targets through rewriting, defaulted and
variadic template-template arguments resolve in the owning definition
context, top-level using directives make qualified types visible to the later
call body, and recursive base matching expands class-template defaults and
pack-derived bases.  Special-member bodies now use their actual compound
statement for local-use analysis, and discarded reference-return calls
materialize the scalar value through the ordinary lowering path.

The last exact required report is **138/250**, or **112** residual failures,
which is **28** above the turn-start **110/250** baseline.  Its grouped map is:
general/100 **13** (forwarding, cv/array/default, and pack normalization),
general/200 **4** (ranking and synthetic-member emission), general/300 **26**
(qualified lookup, alias/SFINAE, and constructor candidates), general/400
**15** (non-type, template-template, alias, and constructor replay),
general/500 **24** (owner, conversion, alias, and dependent-result behavior),
spec/100 **5** (array and specialization edges), spec/200 **7** (defaults and
partial ordering), spec/300 **6** (constructor/conversion/SFINAE), spec/400
**2** (decltype and reference non-type arguments), and spec/500 **10**
(conversion, template-template, alias, and dependent non-type cases).

Next checkpoint scope: take the remaining general/100 forwarding and pack
normalization band, beginning with dependent remove-reference forwarding,
top-level-cv function-pointer deduction, empty-pack/static-assert expansion,
and concrete type-pack element replay.  Validate that group with its full
comparisons, the PA22 report, through-PA21, and the source audit.

## Checkpoint 84 scope — 2026-07-25 (before implementation)

The complete current-PA failure set was refreshed before this increment from
the active report at **138/250**.  The selected group targets explicit function
template calls: incomplete explicit prefixes must remain available for
call-site deduction, explicitly fixed parameters must remain fixed while
matching class conversions and transitive bases, and fixed alias-defined
reference-to-array parameters must retain their reference shape during
deduction.  The focused semantic cases are
`general/100-explicit-template-id-user-conversion-deduction`,
`general/300-explicit-template-call-transitive-base-deduction`, and the PA21
conversion-operator binding fixture.  The scope also includes the source
lookup/replay path needed to defer incomplete function template-ids without
materializing an invalid specialization.

## Checkpoint 84 result and next scope — 2026-07-25

The increment preserves the PA21 conversion-operator selection and raises the
PA22 report to **142/250**, above the turn-start **110/250** baseline.  The two
focused PA22 programs now compile through explicit-prefix deduction, user
conversion viability, and recursive transitive-base matching; their remaining
report entries are LowIR fixture differences only (the generated code is
semantically valid).  The PA21 focused fixture passes, and through-PA21 is
**1850/1850**.  Member class-exactness ranking was moved to a dedicated source
module so the PA22 file audit remains passing.

### Remaining Work Map

- **general/100 — 11:** forwarding/reference, cv/array/default, and pack
  normalization, plus the two explicit-call LowIR parity cases.
- **general/200 — 4:** partial ordering, ambiguity, and synthetic-member
  emission.
- **general/300 — 26:** qualified lookup, alias/SFINAE, constructor, and
  dependent owner replay.
- **general/400 — 15:** non-type, template-template, alias, and constructor
  replay.
- **general/500 — 23:** owner, conversion, alias, and dependent-result
  behavior.
- **spec/100 — 5; spec/200 — 7; spec/300 — 6; spec/400 — 2; spec/500 — 9:**
  remaining array/specialization, ordering/default, conversion/SFINAE,
  decltype/reference, and template-template/non-type edges.

The report has **108** residual failures.  Six names were removed from the
138/250 checkpoint set; the two explicit-call names remain as LowIR-only
parity residuals, with no earlier-PA regressions.

Next checkpoint scope: take the remaining general/100 forwarding and pack
normalization group, beginning with dependent remove-reference forwarding,
top-level-cv function-pointer deduction, empty-pack/static-assert expansion,
and concrete type-pack element replay.  Validate focused comparisons, the
full PA22 report, through-PA21, and the source audit.

## Checkpoint 85 scope — 2026-07-26 (before implementation)

The current report was refreshed from the clean checkpoint and remains
**142/250**, with **108** failures: general/100 **11**, general/200 **4**,
general/300 **26**, general/400 **15**, general/500 **23**, spec/100 **5**,
spec/200 **7**, spec/300 **6**, spec/400 **2**, and spec/500 **9**.  The
complete names are in the report-derived residual set; the general/100 names
are the immediate shared call-deduction band.

Selected scope: repair typed pack and forwarding replay for
`general/100-empty-pack-static-assert-trait-expansion`,
`general/100-type-pack-element-result-selects-copy-ctor`, and
`general/100-dependent-remove-reference-transform-forwarding`.  These cases
share empty type-pack bindings, pack-element specialization replay, and
forwarding-reference substitution through `remove_reference_t`; the scope
covers deduction, dependent return/type rewriting, static-assert expansion,
and aggregate/copy construction without hardcoding any fixture symbols.

Validation for this checkpoint covers the three focused fixtures, the full
PA22 report, through-PA21, and the PA22 source audit.

## Checkpoint 87 completion and checkpoint-audit result — 2026-07-26

The implementation entered this audit at **153/250** PA22 tests.  The audit
fixes raise the authoritative result to **154/250**: the array-reference /
default-argument replay timeout is gone, with no timeout failures remaining.
The completed checkpoint scope was typed pack and forwarding replay: empty
pack static-assert expansion, concrete pack-element copy construction,
dependent `remove_reference_t` forwarding, qualified alias deduction,
value-base SFINAE rejection, empty index-sequence ordering,
inherited-constructor forwarding, and symmetric cv-pointer ambiguity.

### Remaining Work Map

The complete current-PA failure set is **96** fixtures, grouped as follows:

- **general/100 — 3:** `explicit-template-id-user-conversion-deduction`,
  `function-parameter-empty-middle-pack-alias`, and
  `function-template-fixed-over-trailing-pack-fallback` (LowIR parity).
- **general/200 — 1:** `partial-order-synthetic-virtual-member-emission`
  (LowIR parity).
- **general/300 — 25:** qualified lookup, dependent alias/SFINAE,
  constructor, and owner replay.
- **general/400 — 15:** non-type, template-template, alias, and constructor
  replay.
- **general/500 — 23:** owner, conversion, alias, and dependent-result
  behavior.
- **spec/100 — 5; spec/200 — 7; spec/300 — 6; spec/400 — 2; spec/500 — 9:**
  remaining array/specialization, ordering/default, conversion/SFINAE,
  decltype/reference, and template-template/non-type edges.

The next substantial checkpoint bundles the four residual general/100+200
LowIR parity fixtures.  It will normalize the already-semantic output’s
constant-index, temporary-numbering, and synthetic virtual-constructor
presentation without reopening the completed deduction or timeout paths.

## Checkpoint 88 scope — 2026-07-26 (before implementation)

### Current failure audit

The required `make test-report ACTIVE_TEST_REPORT_PAS='pa22'` report is
**154/250**, with **96** failures and no timeouts.  Through-PA21 is passing and
the worktree is clean.  The complete current-PA failure set is grouped below
by primary shared compiler behavior; the four general/100+200 entries in the
first group are the selected checkpoint.

- **Residual LowIR replay/materialization parity (4):**
  `general/100-explicit-template-id-user-conversion-deduction`,
  `general/100-function-parameter-empty-middle-pack-alias`,
  `general/100-function-template-fixed-over-trailing-pack-fallback`,
  `general/200-partial-order-synthetic-virtual-member-emission`.
- **Dependent substitution, lookup, and deferred instantiation (25):**
  `general/300-abstract-array-parameter-sfinae`,
  `general/300-alias-bool-explicit-pack-call-dependent-tag`,
  `general/300-alias-sfinae-inherited-member-value`,
  `general/300-base-qualified-template-value-arg-syntax`,
  `general/300-boost-enable-if-type-condition-static-keyword-overload`,
  `general/300-constructor-template-keeps-ctor-refinement-viable`,
  `general/300-defaulted-sfinae-ctor-candidate-drop`,
  `general/300-empty-pack-unknown-bound-array-lowir`,
  `general/300-explicit-template-call-transitive-base-deduction`,
  `general/300-function-template-nested-alias-explicit-call`,
  `general/300-hidden-friend-dependent-return-specialization-scope`,
  `general/300-inherited-variable-template-enable-if-return`,
  `general/300-internal-remove-cvref-alias-sfinae`,
  `general/300-local-alias-explicit-template-pack-decltype`,
  `general/300-pack-expanded-enable-if-member-value`,
  `general/300-qualified-alias-sfinae-function-pointer-deduction-key`,
  `general/300-qualified-rebind-detected-type-arg`,
  `general/300-recursive-streamable-sfinae-guard`,
  `general/300-single-element-detector-idiom-sfinae-false`,
  `general/300-static-member-template-function-pointer-nttp`,
  `general/300-structured-enable-if-sizeof-pack-value`,
  `general/300-using-declaration-imports-member-template-sfinae-shadow`,
  `general/300-using-directive-overloaded-function-template-arg`,
  `general/300-using-member-template-implicit-object-cv-overload`,
  `general/300-variable-template-detected-idiom-direct-arg`.
- **Typed aliases, constructors, conversions, and non-type arguments (15):**
  `general/400-alias-template-function-argument-cv`,
  `general/400-bad-constructor-template-parameter-shadowing-target-aware`,
  `general/400-constructor-template-pack-before-defaulted-nontype`,
  `general/400-conversion-function-template-prefers-nontemplate`,
  `general/400-defaulted-pointer-nontype-cstyle-null`,
  `general/400-enum-nttp-cstyle-cast-default-rebind`,
  `general/400-function-assignment-invocable-and-helper`,
  `general/400-member-alias-template-template-partial-deduction-owner`,
  `general/400-object-pointer-nttp-address`,
  `general/400-object-pointer-nttp-rebound-member-template`,
  `general/400-pack-expansion-size-mismatch-sfinae`,
  `general/400-partial-specialization-inherited-constructor-template`,
  `general/400-static-data-nttp-pack-sizeof-bound`,
  `general/400-template-template-alias-default-arity-sfinae`,
  `general/400-unnamed-nontype-pack-static-enable-if-default`.
- **Owner, ADL, alias, pack, conversion, and dependent-result replay (23):**
  `general/500-adl-alias-return-operator-template`,
  `general/500-alias-pack-enable-if-constexpr-constructor`,
  `general/500-alias-rebind-forwarding-nondependent-param`,
  `general/500-alias-template-template-defaulted-sfinae-canonical-args`,
  `general/500-async-initiate-dependent-return-sfinae`,
  `general/500-bool-alias-function-template-result-metadata`,
  `general/500-boost-mp11-conditional-alias-reference-set`,
  `general/500-constructor-pack-default-rewritten-pointer`,
  `general/500-constructor-sfinae-member-template-value`,
  `general/500-current-specialization-nontype-default-dependent`,
  `general/500-defaulted-nontype-qualified-alias-value`,
  `general/500-defaulted-pack-bool-short-circuit-sfinae`,
  `general/500-dependent-result-sizeof-sfinae-base`,
  `general/500-explicit-pack-deduced-pack-member-result`,
  `general/500-index-sequence-alias-constructor-deduction`,
  `general/500-inherited-constructor-template-member-alias-pack`,
  `general/500-member-template-dependent-owner-defaulted-sfinae`,
  `general/500-member-template-retained-dependent-param-candidate-drop`,
  `general/500-owner-enum-nontype-result-sfinae`,
  `general/500-partial-specialization-cv-qualifier-subset`,
  `general/500-short-circuit-alias-member-sfinae`,
  `general/500-sizeof-void-sfinae-fallback`,
  `general/500-weak-ptr-shared-ptr-template-ctor`.
- **Specification-band array and specialization edges (5):**
  `spec/100-constructor-template-braced-array-bound-deduction`,
  `spec/100-explicit-specialization-dependent-param-typedef`,
  `spec/100-explicit-template-argument-overload-rejects-short-candidate`,
  `spec/100-function-template-array-bound-braced-empty-argument`,
  `spec/100-function-template-array-parameter-string-literal`.
- **Specification-band defaults, ordering, and member packs (7):**
  `spec/200-constructor-template-qualified-nested-id-partial-ordering`,
  `spec/200-defaulted-class-template-argument-prefix-deduction`,
  `spec/200-dependent-specialized-default-arg-deduction`,
  `spec/200-member-operator-fixed-tag-default-partial-order`,
  `spec/200-member-template-explicit-pack-forward-call`,
  `spec/200-member-template-nontype-param-shadows-inherited-value-sum`,
  `spec/200-overload-set-address-nondeduced-bad`.
- **Specification-band substitution, conversion, and decltype (6):**
  `spec/300-constructor-forwarding-lvalue-beats-const-ref`,
  `spec/300-constructor-template-const-ref-conversion`,
  `spec/300-conversion-function-template-owner-result-copy-init`,
  `spec/300-cross-specialization-converting-ctor-operator-template`,
  `spec/300-qualified-member-function-value-fallback-sfinae`,
  `spec/300-template-id-direct-parameter-same-name-deduction`.
- **Specification-band dependent decltype and reference NTTPs (2):**
  `spec/400-dependent-decltype-member-template-conversion-operator`,
  `spec/400-nontype-reference-argument`.
- **Specification-band final owner/template-template/pack cases (9):**
  `spec/500-conversion-function-template-reference-conditional-auto-ref`,
  `spec/500-conversion-function-template-same-name-target`,
  `spec/500-defaulted-rebind-constructor-deduction`,
  `spec/500-function-result-template-id-shadowed-argument`,
  `spec/500-hidden-friend-query-free-decltype-noexcept`,
  `spec/500-template-template-conversion-operator-reference-target`,
  `spec/500-template-template-piecewise-partial-ordering`,
  `spec/500-type-pack-qualified-static-member-expansion`,
  `spec/500-unqualified-member-template-local-alias-deduction`.

### Checkpoint Scope

Complete the four-fixture residual general/100+200 parity band using typed
compiler behavior.  Preserve the already selected template candidates and
their semantic facts while making replay emit the expected fixed/default pack
specializations, constant array indices, and synthetic constructor/vtable
dependencies.  The scope covers explicit-template conversion helper
materialization, empty-middle-pack function replay, fixed-over-trailing-pack
array indexing, and partial-ordering-driven virtual member construction.  It
must not edit fixtures or compare against an external compiler.  Validation is
the four focused fixtures, the full PA22 report, through-PA21, and the PA22
source audit; the remaining 92 fixtures stay grouped above for follow-up
substitution and typed-owner checkpoints.

## Checkpoint 88 result — 2026-07-26

The selected four-fixture parity scope is complete.  The compiler now reuses
typed constant offsets for byte-sized pointer arithmetic, materializes the
member base at the required assignment evaluation boundary while retaining
constant-expression facts, maps partial-class member replay through its full
specialization parameter details (including empty enclosing packs), preserves
forwarding-reference lvalue deduction, canonicalizes top-level cv-qualified
class specialization identities, and collects implicit constructors from
replayed default-construction effects before lifecycle lowering.

All four focused fixtures pass:

- `general/100-explicit-template-id-user-conversion-deduction`
- `general/100-function-parameter-empty-middle-pack-alias`
- `general/100-function-template-fixed-over-trailing-pack-fallback`
- `general/200-partial-order-synthetic-virtual-member-emission`

The authoritative PA22 report improved from **154/250** to **158/250** with
no timeout failures, and the constructor-template regression found during
the checkpoint was removed by retaining typed unary constant facts for
assignment ordering.  The remaining **92** fixtures are grouped below.

The required through-PA21 report passes **1850/1850**, and the PA22 source
audit passes (with repository-structure warnings only).

- **general/300 — 25:** dependent substitution, qualified lookup, alias and
  SFINAE, constructor viability, and deferred owner replay (19 status, 6
  LowIR parity).
- **general/400 — 15:** typed aliases, constructors, conversions, template-
  template arguments, and non-type arguments (12 status, 3 LowIR parity).
- **general/500 — 23:** owner/ADL, aliases, packs, conversions, and
  dependent-result replay (20 status, 3 LowIR parity).
- **spec/100 — 5; spec/200 — 7; spec/300 — 6; spec/400 — 2; spec/500 — 9:**
  remaining array and specialization, default/ordering, conversion/SFINAE,
  dependent-decltype/reference, and final template-template/non-type edges.

Next checkpoint group: the 25 general/300 fixtures, starting with shared
dependent lookup/substitution and SFINAE candidate viability.  Validation for
this checkpoint remains the four focused tests, the full PA22 report,
through-PA21, and the PA22 source audit.

## Checkpoint 89 scope — 2026-07-26 (before implementation)

The current baseline is **158/250**, with **92** failures and no timeout
failures.  The complete residual set remains grouped as **general/300: 25**
(19 status, 6 LowIR), **general/400: 15** (12 status, 3 LowIR),
**general/500: 23** (20 status, 3 LowIR), **spec/100: 5** (2 status,
3 LowIR), **spec/200: 7** (3 status, 4 LowIR), **spec/300: 6** (4 status,
2 LowIR), **spec/400: 2** (1 status, 1 LowIR), and **spec/500: 9** (8
status, 1 LowIR).  The detailed fixture names are listed in the remaining
work map above.

Selected scope: the shared general/300 parser and typed-replay behavior for
non-type template-parameter scope, alias-template registration, alias-template
functional casts, and alias-template result typing during function-template
deduction.  The focused behavior is represented by
`general/300-function-template-nested-alias-explicit-call`, with adjacent
parser/SFINAE checks `general/300-alias-bool-explicit-pack-call-dependent-tag`,
`general/300-hidden-friend-dependent-return-specialization-scope`,
`general/300-internal-remove-cvref-alias-sfinae`,
`general/300-recursive-streamable-sfinae-guard`, and
`general/300-structured-enable-if-sizeof-pack-value`.  Validation covers this
parser cluster, the complete PA22 report, through-PA21, and the source audit;
the other 86 fixtures remain grouped for the next typed-substitution and
candidate-viability checkpoints.

## Checkpoint 89 result — 2026-07-26

The selected parser and typed-replay scope is complete.  Alias templates are
registered in parser type state and template-parameter scopes, alias
functional casts retain their typed result during deduction, explicit packs
consume trailing arguments correctly, and dependent `typename T(...)` and
`sizeof...(Pack)` forms replay through the same typed substitution path.  The
checkpoint also preserves callable/noexcept result facts, avoids spurious
empty-base `copyobj` lowering, and handles normalized `operator>>`/`>=`
template-angle ambiguity.

The six focused fixtures all pass in both exit status and LowIR comparison:

- `general/300-function-template-nested-alias-explicit-call`
- `general/300-alias-bool-explicit-pack-call-dependent-tag`
- `general/300-hidden-friend-dependent-return-specialization-scope`
- `general/300-internal-remove-cvref-alias-sfinae`
- `general/300-recursive-streamable-sfinae-guard`
- `general/300-structured-enable-if-sizeof-pack-value`

The generated dependency namespace wrapper is now an explicit AST fact.  PA11
merges such a wrapper into an existing class scope when its owner is a class,
predeclares source classes in named namespaces, and keeps same-spelled source
nested classes distinct from generated shells.  This closes the three
through-PA21 regressions found during the checkpoint while retaining the
local-class early-layout rule.

The authoritative PA22 report improved from **158/250** at checkpoint start
to **163/250** (**87** residual fixtures, no timeout).  The required
through-PA21 report passes **1850/1850**, and the PA22 source audit passes with
repository-structure warnings only.

### Remaining Work Map

- **General/100–200 deduction and LowIR parity (2):**
  `general/100-template-array-reference-cv-default-arg` (status) and
  `general/200-inherited-constructor-template-forwarding` (LowIR).
- **General/300 dependent substitution and SFINAE (18; 12 status, 6
  LowIR):** abstract-array and inherited-value SFINAE; qualified/base value
  arguments; enable-if and constructor viability; empty packs; transitive
  base deduction; local/qualified aliases; variable/static member templates;
  using-declaration/directive lookup; and the remaining detector idioms.
  The exact residual fixtures are the 18 `general/300-*` entries in the
  current report, beginning with `abstract-array-parameter-sfinae` and ending
  with `variable-template-detected-idiom-direct-arg`.
- **General/400 typed aliases, constructors, conversions, and NTTPs (14;
  12 status, 2 LowIR):** alias argument cv, constructor parameter shadowing
  and pack/default handling, conversion ranking, template-template deduction,
  pointer/enum NTTPs, pack mismatch, inherited constructors, and static data.
- **General/500 owner/ADL, alias, pack, conversion, and dependent-result
  replay (24; 20 status, 4 LowIR):** owner rebinding, alias-pack and
  template-template SFINAE, conversion/constructor replay, dependent member
  results, enum/pack NTTPs, and short-circuit/sizeof viability.
- **Specification bands (29):** spec/100 has 5 array/specialization cases
  (2 status, 3 LowIR), spec/200 has 7 default/ordering/member-pack cases
  (3 status, 4 LowIR), spec/300 has 6 constructor/conversion/decltype cases
  (4 status, 2 LowIR), spec/400 has 2 dependent-decltype/reference cases
  (1 status, 1 LowIR), and spec/500 has 9 owner/template-template/pack cases
  (8 status, 1 LowIR).

Next checkpoint group: the remaining 18 general/300 fixtures bundled with
the two small general/100–200 cases, focusing on shared dependent lookup,
substitution-failure candidate viability, and deferred constructor replay.
Validation remains the focused cluster, the full PA22 report, through-PA21,
and the PA22 source audit.

## Checkpoint 90 scope — 2026-07-26 (before implementation)

The fresh baseline remains **163/250**, with **87** residual fixtures and no
timeouts.  The complete current failure set is grouped as general/100 (1),
general/200 (1), general/300 (18), general/400 (14), general/500 (24), and
spec/100/200/300/400/500 (5/7/6/2/9); the report classifies **63** as status
failures and **24** as LowIR parity failures.  The prior checkpoint's six
general/300 parser/replay fixtures remain green.

Selected scope: preserve typed constant and declaration facts for variable
and static member templates when they are used as non-type template arguments,
dependent detector specializations, or inherited enable-if conditions.  The
focused fixtures are:

- `general/300-inherited-variable-template-enable-if-return`
- `general/300-variable-template-detected-idiom-direct-arg`
- `general/300-static-member-template-function-pointer-nttp`
- `general/300-qualified-rebind-detected-type-arg`

This scope covers materializing a variable-template specialization's boolean
value, selecting dependent partial specializations from a qualified alias,
and retaining the typed address/signature of a static member function template
for NTTP deduction.  Validation is the four focused fixtures in exit-status
and LowIR modes, the complete PA22 report, through-PA21, and the source audit.
The remaining 83 fixtures stay grouped for later deduction/ordering,
constructor/conversion, and specification-band checkpoints.

## Checkpoint 90 result — 2026-07-26

The selected variable/static member replay scope is complete.  Non-type
function-address arguments now retain and validate their typed member-function
signatures, explicit null-pointer calls can complete dependent pointer
deduction, variable-template specializations are evaluated through the typed
constant table, and dependent static constants are deferred until concrete
replay.  Generated-owner call-result lookup and partial-specialization
`decltype` matching use the same typed replay state.  Omitted template
arguments now use the referenced template's own defaults, preventing an
outer same-spelled parameter from leaking into an alias default such as
`enable_if_t<B>`.

All four focused fixtures pass in exit-status and LowIR modes:

- `general/300-inherited-variable-template-enable-if-return`
- `general/300-variable-template-detected-idiom-direct-arg`
- `general/300-static-member-template-function-pointer-nttp`
- `general/300-qualified-rebind-detected-type-arg`

The authoritative PA22 report improved from **163/250** to **169/250**;
**81** fixtures remain, with no timeout failures.  Through-PA21 passes
**1850/1850**, and the PA22 source audit passes with the repository's existing
11 structural warnings.  During validation, an over-broad generated-owner
lookup was narrowed back to source-owner spelling; the two PA21 regressions
then cleared while the checkpoint fixtures remained green.

### Remaining Work Map

- **General/100–200 (2):** one array-reference/default-deduction status
  failure and one inherited-constructor LowIR parity case.
- **General/300 (14):** dependent array and alias SFINAE, qualified/base
  non-type lookup, constructor viability, empty-pack replay, transitive-base
  deduction, using-declaration/directive lookup, and remaining detector idioms
  (9 status, 5 LowIR parity).
- **General/400 (14):** typed aliases, constructor parameter/pack/default
  handling, conversion ranking, template-template deduction, pointer/enum
  NTTPs, pack mismatch, inherited constructors, and static data (12 status,
  2 LowIR parity).
- **General/500 (23):** owner/ADL rebinding, alias packs and
  template-template SFINAE, constructor/conversion replay, dependent member
  results, enum/pack NTTPs, and short-circuit/sizeof viability (19 status,
  4 LowIR parity).
- **Specification bands (28):** spec/100 has 5 array/specialization cases
  (2 status, 3 LowIR), spec/200 has 7 default/ordering/member-pack cases
  (3 status, 4 LowIR), spec/300 has 6 conversion/decltype cases (4 status,
  2 LowIR), spec/400 has 2 dependent-decltype/reference cases (1 status,
  1 LowIR), and spec/500 has 8 owner/template-template/pack cases (7 status,
  1 LowIR).

Next checkpoint group: the 14 remaining general/300 fixtures, beginning with
the shared dependent lookup and substitution-failure candidate-viability
cases, while preserving the now-green variable/static member replay paths.

## Checkpoint 91 scope — 2026-07-26 (before implementation)

The refreshed authoritative report remains **169/250**, with **81** residual
fixtures and no timeout failures.  The complete residual set is the grouped
map immediately above: general/100/200 has 2, general/300 has 14,
general/400 has 14, general/500 has 23, and the specification bands have
28.  Through-PA21 remains 1850/1850 and the worktree is clean.

Selected scope: repair typed lookup and candidate viability for using-imported
members and namespace-directed overloaded function-template arguments.  The
focused fixtures are:

- `general/300-using-declaration-imports-member-template-sfinae-shadow`
- `general/300-using-directive-overloaded-function-template-arg`
- `general/300-using-member-template-implicit-object-cv-overload`

This scope covers inherited member-template visibility beside a using-imported
non-template, dependent enable-if substitution while forming the imported
candidate set, namespace using-directive lookup for an overloaded function
address, and implicit-object cv ranking.  Validation is the three focused
fixtures in exit-status and LowIR modes, the full PA22 report,
through-PA21, and the PA22 source audit.

## Checkpoint 91 result — 2026-07-26

The selected using-lookup and candidate-viability scope is complete.  Typed
using-directive exports now include namespace function templates, dependent
using-imported member templates are omitted from concrete replay while their
typed definitions remain available for lookup, deferred member-typedef
function-pointer deduction is replayed after enclosing arguments are known,
and ordinary imported-member viability accounts for the implicit object's cv
qualification.  Selected function-template arguments are materialized with
the selected substitution map before lowering.

All three focused fixtures pass in exit-status and LowIR modes.  The
authoritative PA22 report improved from **169/250** to **172/250**; **78**
fixtures remain, with no timeout failures.  Through-PA21 passes **1850/1850**,
and the PA22 source audit passes with the repository's existing 11 structural
warnings.

### Remaining Work Map

- **General/100–200 (2):** array-reference/default deduction and inherited
  constructor forwarding.
- **General/300 (11):** dependent array and alias SFINAE, qualified/base
  non-type lookup, constructor viability, empty-pack replay, transitive-base
  deduction, and remaining detector/alias cases.
- **General/400 (14):** typed aliases, constructor parameter/pack/default
  handling, conversion ranking, template-template deduction, pointer/enum
  NTTPs, pack mismatch, inherited constructors, and static data.
- **General/500 (23):** owner/ADL rebinding, alias packs and
  template-template SFINAE, constructor/conversion replay, dependent member
  results, enum/pack NTTPs, and short-circuit/sizeof viability.
- **Specification bands (28):** spec/100 has 5, spec/200 has 7, spec/300
  has 6, spec/400 has 2, and spec/500 has 8 remaining deduction, ordering,
  constructor, conversion, decltype, reference, and pack cases.

Next checkpoint group: the remaining general/300 dependent lookup and
substitution-failure cases, starting with the alias/qualified-value and
detector fixtures, while preserving the now-green using-lookup paths.

## Checkpoint 91 audit result — 2026-07-26

The checkpoint audit found and fixed four implementation issues before handoff:
using-directive exports now retain non-owning typed declaration pointers;
using-imported member-template promotion is scope-indexed by typed imported
definitions; selected function-template arguments replace only the referenced
identifier beneath an explicit address-of node; and deferred expected-signature
matches no longer recompute function-expression candidates.  Dependent base
rewrites in the ordinary-member viability probe now reject that candidate on
substitution failure instead of escaping as a hard lookup failure.  No
checkpoint shortcut, timeout workaround, output bypass, ownership violation,
or audit bypass remains.

The exact authoritative current-PA residual is **78/250** failures, unchanged
from the turn-start **172/250** passing baseline, with no timeout.  The
required through-PA21 report is **1850/1850** and the source audit passes with
11 pre-existing structural warnings.

### Remaining Work Map (refreshed from the complete report)

- **General/100–200 (2):** status
  `general/100-template-array-reference-cv-default-arg`; LowIR parity
  `general/200-inherited-constructor-template-forwarding`.
- **General/300 (11; 7 status, 4 LowIR):** status
  `general/300-abstract-array-parameter-sfinae`,
  `general/300-alias-sfinae-inherited-member-value`,
  `general/300-base-qualified-template-value-arg-syntax`,
  `general/300-boost-enable-if-type-condition-static-keyword-overload`,
  `general/300-constructor-template-keeps-ctor-refinement-viable`,
  `general/300-local-alias-explicit-template-pack-decltype`, and
  `general/300-single-element-detector-idiom-sfinae-false`; LowIR parity
  `general/300-defaulted-sfinae-ctor-candidate-drop`,
  `general/300-empty-pack-unknown-bound-array-lowir`,
  `general/300-explicit-template-call-transitive-base-deduction`, and
  `general/300-qualified-alias-sfinae-function-pointer-deduction-key`.
- **General/400 (14; 12 status, 2 LowIR):** status
  `general/400-alias-template-function-argument-cv`,
  `general/400-bad-constructor-template-parameter-shadowing-target-aware`,
  `general/400-conversion-function-template-prefers-nontemplate`,
  `general/400-defaulted-pointer-nontype-cstyle-null`,
  `general/400-enum-nttp-cstyle-cast-default-rebind`,
  `general/400-function-assignment-invocable-and-helper`,
  `general/400-member-alias-template-template-partial-deduction-owner`,
  `general/400-object-pointer-nttp-address`,
  `general/400-object-pointer-nttp-rebound-member-template`,
  `general/400-pack-expansion-size-mismatch-sfinae`,
  `general/400-partial-specialization-inherited-constructor-template`, and
  `general/400-template-template-alias-default-arity-sfinae`; LowIR parity
  `general/400-constructor-template-pack-before-defaulted-nontype` and
  `general/400-static-data-nttp-pack-sizeof-bound`.
- **General/500 (23; 19 status, 4 LowIR):** status
  `general/500-adl-alias-return-operator-template`,
  `general/500-alias-pack-enable-if-constexpr-constructor`,
  `general/500-alias-template-template-defaulted-sfinae-canonical-args`,
  `general/500-async-initiate-dependent-return-sfinae`,
  `general/500-boost-mp11-conditional-alias-reference-set`,
  `general/500-constructor-pack-default-rewritten-pointer`,
  `general/500-constructor-sfinae-member-template-value`,
  `general/500-defaulted-nontype-qualified-alias-value`,
  `general/500-defaulted-pack-bool-short-circuit-sfinae`,
  `general/500-dependent-result-sizeof-sfinae-base`,
  `general/500-explicit-pack-deduced-pack-member-result`,
  `general/500-index-sequence-alias-constructor-deduction`,
  `general/500-member-template-dependent-owner-defaulted-sfinae`,
  `general/500-member-template-retained-dependent-param-candidate-drop`,
  `general/500-owner-enum-nontype-result-sfinae`,
  `general/500-partial-specialization-cv-qualifier-subset`,
  `general/500-short-circuit-alias-member-sfinae`,
  `general/500-sizeof-void-sfinae-fallback`, and
  `general/500-weak-ptr-shared-ptr-template-ctor`; LowIR parity
  `general/500-bool-alias-function-template-result-metadata`,
  `general/500-current-specialization-nontype-default-dependent`,
  `general/500-dependent-member-alias-function-return`, and
  `general/500-inherited-constructor-template-member-alias-pack`.
- **Specification bands (28):** spec/100 (5: status
  `spec/100-constructor-template-braced-array-bound-deduction`,
  `spec/100-function-template-array-bound-braced-empty-argument`; LowIR
  `spec/100-explicit-specialization-dependent-param-typedef`,
  `spec/100-explicit-template-argument-overload-rejects-short-candidate`,
  `spec/100-function-template-array-parameter-string-literal`); spec/200
  (7: status `spec/200-member-operator-fixed-tag-default-partial-order`,
  `spec/200-member-template-explicit-pack-forward-call`,
  `spec/200-overload-set-address-nondeduced-bad`; LowIR
  `spec/200-constructor-template-qualified-nested-id-partial-ordering`,
  `spec/200-defaulted-class-template-argument-prefix-deduction`,
  `spec/200-dependent-specialized-default-arg-deduction`,
  `spec/200-member-template-nontype-param-shadows-inherited-value-sum`);
  spec/300 (6: status `spec/300-constructor-forwarding-lvalue-beats-const-ref`,
  `spec/300-constructor-template-const-ref-conversion`,
  `spec/300-conversion-function-template-owner-result-copy-init`,
  `spec/300-template-id-direct-parameter-same-name-deduction`; LowIR
  `spec/300-cross-specialization-converting-ctor-operator-template`,
  `spec/300-qualified-member-function-value-fallback-sfinae`); spec/400
  (2: status `spec/400-nontype-reference-argument`; LowIR
  `spec/400-dependent-decltype-member-template-conversion-operator`); and
  spec/500 (8: status
  `spec/500-conversion-function-template-reference-conditional-auto-ref`,
  `spec/500-conversion-function-template-same-name-target`,
  `spec/500-defaulted-rebind-constructor-deduction`,
  `spec/500-function-result-template-id-shadowed-argument`,
  `spec/500-hidden-friend-query-free-decltype-noexcept`,
  `spec/500-template-template-conversion-operator-reference-target`,
  `spec/500-template-template-piecewise-partial-ordering`; LowIR
  `spec/500-type-pack-qualified-static-member-expansion`).

Next substantial checkpoint group: bundle the two general/100–200 cases with
the 11 general/300 cases (13 total), beginning with the shared qualified/base
value and detector/SFINAE behavior.  Preserve the now-typed using-lookup
tables and the three green Checkpoint 91 fixtures while repairing that group.

## Checkpoint 92 scope — 2026-07-26 (before implementation)

### Baseline and Remaining Work Map

The refreshed required report is **172/250** passing: **78** PA22 fixtures
remain, with **56** exit-status failures and **22** LowIR-parity failures and
no timeout.  The complete residual set, grouped by primary shared behavior,
is the following:

- **General/100–200 deduction and replay (2):** status
  `general/100-template-array-reference-cv-default-arg`; LowIR parity
  `general/200-inherited-constructor-template-forwarding`.
- **General/300 dependent substitution and SFINAE (11):** status
  `general/300-abstract-array-parameter-sfinae`,
  `general/300-alias-sfinae-inherited-member-value`,
  `general/300-base-qualified-template-value-arg-syntax`,
  `general/300-boost-enable-if-type-condition-static-keyword-overload`,
  `general/300-constructor-template-keeps-ctor-refinement-viable`,
  `general/300-local-alias-explicit-template-pack-decltype`, and
  `general/300-single-element-detector-idiom-sfinae-false`; LowIR parity
  `general/300-defaulted-sfinae-ctor-candidate-drop`,
  `general/300-empty-pack-unknown-bound-array-lowir`,
  `general/300-explicit-template-call-transitive-base-deduction`, and
  `general/300-qualified-alias-sfinae-function-pointer-deduction-key`.
- **General/400 typed aliases, constructors, conversions, and NTTPs (14):**
  `general/400-alias-template-function-argument-cv`,
  `general/400-bad-constructor-template-parameter-shadowing-target-aware`,
  `general/400-constructor-template-pack-before-defaulted-nontype`,
  `general/400-conversion-function-template-prefers-nontemplate`,
  `general/400-defaulted-pointer-nontype-cstyle-null`,
  `general/400-enum-nttp-cstyle-cast-default-rebind`,
  `general/400-function-assignment-invocable-and-helper`,
  `general/400-member-alias-template-template-partial-deduction-owner`,
  `general/400-object-pointer-nttp-address`,
  `general/400-object-pointer-nttp-rebound-member-template`,
  `general/400-pack-expansion-size-mismatch-sfinae`,
  `general/400-partial-specialization-inherited-constructor-template`,
  `general/400-static-data-nttp-pack-sizeof-bound`, and
  `general/400-template-template-alias-default-arity-sfinae`.
- **General/500 owner, ADL, alias, pack, conversion, and result replay (23):**
  `general/500-adl-alias-return-operator-template`,
  `general/500-alias-pack-enable-if-constexpr-constructor`,
  `general/500-alias-template-template-defaulted-sfinae-canonical-args`,
  `general/500-async-initiate-dependent-return-sfinae`,
  `general/500-bool-alias-function-template-result-metadata`,
  `general/500-boost-mp11-conditional-alias-reference-set`,
  `general/500-constructor-pack-default-rewritten-pointer`,
  `general/500-constructor-sfinae-member-template-value`,
  `general/500-current-specialization-nontype-default-dependent`,
  `general/500-defaulted-nontype-qualified-alias-value`,
  `general/500-defaulted-pack-bool-short-circuit-sfinae`,
  `general/500-dependent-member-alias-function-return`,
  `general/500-dependent-result-sizeof-sfinae-base`,
  `general/500-explicit-pack-deduced-pack-member-result`,
  `general/500-index-sequence-alias-constructor-deduction`,
  `general/500-inherited-constructor-template-member-alias-pack`,
  `general/500-member-template-dependent-owner-defaulted-sfinae`,
  `general/500-member-template-retained-dependent-param-candidate-drop`,
  `general/500-owner-enum-nontype-result-sfinae`,
  `general/500-partial-specialization-cv-qualifier-subset`,
  `general/500-short-circuit-alias-member-sfinae`,
  `general/500-sizeof-void-sfinae-fallback`, and
  `general/500-weak-ptr-shared-ptr-template-ctor`.
- **Specification-band deduction, ordering, conversion, and dependent replay
  (28):** spec/100
  `constructor-template-braced-array-bound-deduction`,
  `explicit-specialization-dependent-param-typedef`,
  `explicit-template-argument-overload-rejects-short-candidate`,
  `function-template-array-bound-braced-empty-argument`,
  `function-template-array-parameter-string-literal`; spec/200
  `constructor-template-qualified-nested-id-partial-ordering`,
  `defaulted-class-template-argument-prefix-deduction`,
  `dependent-specialized-default-arg-deduction`,
  `member-operator-fixed-tag-default-partial-order`,
  `member-template-explicit-pack-forward-call`,
  `member-template-nontype-param-shadows-inherited-value-sum`,
  `overload-set-address-nondeduced-bad`; spec/300
  `constructor-forwarding-lvalue-beats-const-ref`,
  `constructor-template-const-ref-conversion`,
  `conversion-function-template-owner-result-copy-init`,
  `cross-specialization-converting-ctor-operator-template`,
  `qualified-member-function-value-fallback-sfinae`,
  `template-id-direct-parameter-same-name-deduction`; spec/400
  `dependent-decltype-member-template-conversion-operator`,
  `nontype-reference-argument`; and spec/500
  `conversion-function-template-reference-conditional-auto-ref`,
  `conversion-function-template-same-name-target`,
  `defaulted-rebind-constructor-deduction`,
  `function-result-template-id-shadowed-argument`,
  `hidden-friend-query-free-decltype-noexcept`,
  `template-template-conversion-operator-reference-target`,
  `template-template-piecewise-partial-ordering`, and
  `type-pack-qualified-static-member-expansion`.

### Checkpoint Scope

Complete the 13-fixture general/100–300 group as one typed substitution and
candidate-viability increment.  Repair the shared behavior behind dependent
array/reference deduction, qualified/base value lookup, alias and detector
SFINAE, constructor refinement and defaulted-SFINAE candidate dropping,
transitive-base explicit calls, empty-pack array replay, and dependent
function-pointer alias matching.  Preserve substitution failures as
candidate-local state and carry typed pack, owner, function-signature, and
constant facts through replay so the two general/100–200 fixtures and all 11
general/300 fixtures can be validated in both exit-status and LowIR modes.
The next group after this checkpoint is general/400 typed aliases,
constructors, conversions, and non-type arguments; the remaining general/500
and specification bands stay explicitly listed above.

## Checkpoint 92 result — 2026-07-26

- Completed scope: the 13-fixture general/100–300 typed substitution and
  candidate-viability group.  The increment now carries typed pack expansion,
  owner/alias identity, function-signature lvalue facts, dependent decltype
  call results, abstract-object SFINAE, partial-specialization selection,
  constructor/base-entry demand, static-object initialization, and lazy
  polymorphic vtable demand through lowering.  Responsibility-sized source
  files were added for discard lowering, type qualification, and decltype-call
  replay; each is registered in `dev/frontend_source_sets.mk`.
- Checkpoint validation: the selected 13-fixture `pa22 check` passed **13/13**.
  `make test-report-through-pa21` passed **1850/1850**.  The required current
  report reached **178/250**, above the turn-start baseline of **172/250**;
  the remaining 72 failures are 53 exit-status cases and 19 LowIR-parity
  cases, with no timeout.
- File audit: `perl scripts/cppgm_file_audit.pl --stage pa22 --paths dev/src`
  passed, with the repository's existing 11 non-fatal warnings.

### Remaining Work Map

- **General/100–300 (6):** status
  `general/100-defaulted-nested-class-template-deduction`,
  `general/100-fixed-over-empty-trailing-pack-index-sequence`,
  `general/100-template-deduction-rejects-value-base-argument`, and
  `general/300-hidden-friend-sfinae-use-scope-shadowing`; LowIR parity
  `general/200-function-template-partial-order-class-template-cv` and
  `general/200-partial-order-synthetic-virtual-member-emission`.
- **General/400 (14):** status
  `general/400-alias-template-function-argument-cv`,
  `general/400-bad-constructor-template-parameter-shadowing-target-aware`,
  `general/400-conversion-function-template-prefers-nontemplate`,
  `general/400-defaulted-pointer-nontype-cstyle-null`,
  `general/400-enum-nttp-cstyle-cast-default-rebind`,
  `general/400-function-assignment-invocable-and-helper`,
  `general/400-member-alias-template-template-partial-deduction-owner`,
  `general/400-object-pointer-nttp-address`,
  `general/400-object-pointer-nttp-rebound-member-template`,
  `general/400-pack-expansion-size-mismatch-sfinae`,
  `general/400-partial-specialization-inherited-constructor-template`, and
  `general/400-template-template-alias-default-arity-sfinae`; LowIR parity
  `general/400-constructor-template-pack-before-defaulted-nontype` and
  `general/400-static-data-nttp-pack-sizeof-bound`.
- **General/500 (23):** status
  `general/500-adl-alias-return-operator-template`,
  `general/500-alias-pack-enable-if-constexpr-constructor`,
  `general/500-alias-template-template-defaulted-sfinae-canonical-args`,
  `general/500-async-initiate-dependent-return-sfinae`,
  `general/500-boost-mp11-conditional-alias-reference-set`,
  `general/500-constructor-pack-default-rewritten-pointer`,
  `general/500-constructor-sfinae-member-template-value`,
  `general/500-defaulted-nontype-qualified-alias-value`,
  `general/500-defaulted-pack-bool-short-circuit-sfinae`,
  `general/500-dependent-result-sizeof-sfinae-base`,
  `general/500-explicit-pack-deduced-pack-member-result`,
  `general/500-index-sequence-alias-constructor-deduction`,
  `general/500-member-template-dependent-owner-defaulted-sfinae`,
  `general/500-member-template-retained-dependent-param-candidate-drop`,
  `general/500-owner-enum-nontype-result-sfinae`,
  `general/500-partial-specialization-cv-qualifier-subset`,
  `general/500-short-circuit-alias-member-sfinae`,
  `general/500-sizeof-void-sfinae-fallback`, and
  `general/500-weak-ptr-shared-ptr-template-ctor`; LowIR parity
  `general/500-bool-alias-function-template-result-metadata`,
  `general/500-current-specialization-nontype-default-dependent`,
  `general/500-dependent-member-alias-function-return`, and
  `general/500-inherited-constructor-template-member-alias-pack`.
- **Specification bands (29):** spec/100 status
  `spec/100-constructor-template-braced-array-bound-deduction`,
  `spec/100-defaulted-nested-class-template-deduction`, and
  `spec/100-function-template-array-bound-braced-empty-argument`; LowIR
  `spec/100-explicit-specialization-dependent-param-typedef`,
  `spec/100-explicit-template-argument-overload-rejects-short-candidate`, and
  `spec/100-function-template-array-parameter-string-literal`.  Spec/200
  status `spec/200-function-template-class-template-param-partial-order`,
  `spec/200-member-operator-fixed-tag-default-partial-order`, and
  `spec/200-overload-set-address-nondeduced-bad`; LowIR
  `spec/200-constructor-template-qualified-nested-id-partial-ordering`,
  `spec/200-defaulted-class-template-argument-prefix-deduction`,
  `spec/200-dependent-specialized-default-arg-deduction`, and
  `spec/200-member-template-nontype-param-shadows-inherited-value-sum`.
  Spec/300 status
  `spec/300-constructor-forwarding-lvalue-beats-const-ref`,
  `spec/300-constructor-template-const-ref-conversion`,
  `spec/300-conversion-function-template-owner-result-copy-init`, and
  `spec/300-template-id-direct-parameter-same-name-deduction`; LowIR
  `spec/300-cross-specialization-converting-ctor-operator-template` and
  `spec/300-qualified-member-function-value-fallback-sfinae`.  Spec/400
  status `spec/400-nontype-reference-argument`; LowIR
  `spec/400-dependent-decltype-member-template-conversion-operator`.  Spec/500
  status `spec/500-conversion-function-template-reference-conditional-auto-ref`,
  `spec/500-conversion-function-template-same-name-target`,
  `spec/500-defaulted-rebind-constructor-deduction`,
  `spec/500-function-result-template-id-shadowed-argument`,
  `spec/500-hidden-friend-query-free-decltype-noexcept`,
  `spec/500-template-template-conversion-operator-reference-target`, and
  `spec/500-template-template-piecewise-partial-ordering`; LowIR
  `spec/500-type-pack-qualified-static-member-expansion`.

### Next Checkpoint Scope

Take the general/400 group as the next coherent increment, beginning with the
shared alias-template and constructor/NTTP deduction paths.  Preserve the
green 13-fixture group and the through-PA21 gate while validating both exit
status and LowIR behavior for that group.

## Checkpoint 93 scope — 2026-07-26 (before implementation)

### Current failure audit

The required current-PA report is **178/250**: **72** failures, with no
timeout, while the through-PA21 report remains green.  The complete failure set
from the authoritative report is grouped by primary shared behavior below;
every failing fixture is listed exactly once in its primary group.

- **Earlier deduction, ordering, and dependent lookup (6):**
  `general/100-defaulted-nested-class-template-deduction`,
  `general/100-fixed-over-empty-trailing-pack-index-sequence`,
  `general/100-template-deduction-rejects-value-base-argument`,
  `general/200-function-template-partial-order-class-template-cv`,
  `general/200-partial-order-synthetic-virtual-member-emission`, and
  `general/300-hidden-friend-sfinae-use-scope-shadowing`.
- **General/400 alias, constructor, conversion, and typed-NTTP behavior
  (14):** status failures
  `general/400-alias-template-function-argument-cv`,
  `general/400-bad-constructor-template-parameter-shadowing-target-aware`,
  `general/400-conversion-function-template-prefers-nontemplate`,
  `general/400-defaulted-pointer-nontype-cstyle-null`,
  `general/400-enum-nttp-cstyle-cast-default-rebind`,
  `general/400-function-assignment-invocable-and-helper`,
  `general/400-member-alias-template-template-partial-deduction-owner`,
  `general/400-object-pointer-nttp-address`,
  `general/400-object-pointer-nttp-rebound-member-template`,
  `general/400-pack-expansion-size-mismatch-sfinae`,
  `general/400-partial-specialization-inherited-constructor-template`, and
  `general/400-template-template-alias-default-arity-sfinae`; LowIR parity
  failures `general/400-constructor-template-pack-before-defaulted-nontype`
  and `general/400-static-data-nttp-pack-sizeof-bound`.
- **General/500 owner, alias, pack, ADL, conversion, and result replay
  (23):** status failures
  `general/500-adl-alias-return-operator-template`,
  `general/500-alias-pack-enable-if-constexpr-constructor`,
  `general/500-alias-template-template-defaulted-sfinae-canonical-args`,
  `general/500-async-initiate-dependent-return-sfinae`,
  `general/500-boost-mp11-conditional-alias-reference-set`,
  `general/500-constructor-pack-default-rewritten-pointer`,
  `general/500-constructor-sfinae-member-template-value`,
  `general/500-defaulted-nontype-qualified-alias-value`,
  `general/500-defaulted-pack-bool-short-circuit-sfinae`,
  `general/500-dependent-result-sizeof-sfinae-base`,
  `general/500-explicit-pack-deduced-pack-member-result`,
  `general/500-index-sequence-alias-constructor-deduction`,
  `general/500-member-template-dependent-owner-defaulted-sfinae`,
  `general/500-member-template-retained-dependent-param-candidate-drop`,
  `general/500-owner-enum-nontype-result-sfinae`,
  `general/500-partial-specialization-cv-qualifier-subset`,
  `general/500-short-circuit-alias-member-sfinae`,
  `general/500-sizeof-void-sfinae-fallback`, and
  `general/500-weak-ptr-shared-ptr-template-ctor`; LowIR parity failures
  `general/500-bool-alias-function-template-result-metadata`,
  `general/500-current-specialization-nontype-default-dependent`,
  `general/500-dependent-member-alias-function-return`, and
  `general/500-inherited-constructor-template-member-alias-pack`.
- **Specification-band deduction, ordering, conversion, and dependent replay
  (29):** spec/100
  `constructor-template-braced-array-bound-deduction`,
  `defaulted-nested-class-template-deduction`,
  `explicit-specialization-dependent-param-typedef`,
  `explicit-template-argument-overload-rejects-short-candidate`,
  `function-template-array-bound-braced-empty-argument`, and
  `function-template-array-parameter-string-literal`; spec/200
  `constructor-template-qualified-nested-id-partial-ordering`,
  `defaulted-class-template-argument-prefix-deduction`,
  `dependent-specialized-default-arg-deduction`,
  `function-template-class-template-param-partial-order`,
  `member-operator-fixed-tag-default-partial-order`,
  `member-template-nontype-param-shadows-inherited-value-sum`, and
  `overload-set-address-nondeduced-bad`; spec/300
  `constructor-forwarding-lvalue-beats-const-ref`,
  `constructor-template-const-ref-conversion`,
  `conversion-function-template-owner-result-copy-init`,
  `cross-specialization-converting-ctor-operator-template`,
  `qualified-member-function-value-fallback-sfinae`, and
  `template-id-direct-parameter-same-name-deduction`; spec/400
  `dependent-decltype-member-template-conversion-operator` and
  `nontype-reference-argument`; spec/500
  `conversion-function-template-reference-conditional-auto-ref`,
  `conversion-function-template-same-name-target`,
  `defaulted-rebind-constructor-deduction`,
  `function-result-template-id-shadowed-argument`,
  `hidden-friend-query-free-decltype-noexcept`,
  `template-template-conversion-operator-reference-target`,
  `template-template-piecewise-partial-ordering`, and
  `type-pack-qualified-static-member-expansion`.

### Checkpoint Scope

Complete the **14-fixture general/400 alias, constructor, conversion, and
typed non-type argument slice**.  The compiler will preserve typed template
parameter kinds while resolving defaulted pointer/enum/object NTTPs, expand
multiple bound packs as candidate-local substitution, enforce target-aware
constructor-template parameter rules, retain inherited and alias-owned
constructor candidates, and rank an exact non-template conversion before a
conversion-function template.  It will also carry owner and template-template
arity facts through alias lookup and materialize static data/function members
through the existing LowIR path.  Validation covers all 14 listed fixtures in
both status and LowIR modes, the full PA22 report, through-PA21, and the PA22
file audit.  The six earlier-band cases and the 52 general/500/spec cases
remain explicit follow-up groups; the next checkpoint will take general/500
owner/alias/pack replay after this increment.

## Checkpoint 93 result — authoritative rerun

The compiler rebuild succeeded and the required PA22 report currently records
**176/250**.  The focused invocation was corrected to pass the runner's
`--emit-lowir -O0` arguments; under that authoritative path, the general/400
checkpoint has 7 passing fixtures and 7 remaining failures.  The remaining
general/400 behavior is grouped as:

- **Alias/member-template owner replay (2 status):**
  `alias-template-function-argument-cv` (constructor viability) and
  `member-alias-template-template-partial-deduction-owner` (template-template
  compatibility).
- **Conversion/call result replay (2 status):**
  `conversion-function-template-prefers-nontemplate` and
  `function-assignment-invocable-and-helper`.
- **Constructor replay (1 status):**
  `partial-specialization-inherited-constructor-template`.
- **Presentation/LowIR normalization (3):**
  `pack-expansion-size-mismatch-sfinae`,
  `static-data-nttp-pack-sizeof-bound`, and
  `template-template-alias-default-arity-sfinae`.

The complete remaining report map is 49 status mismatches and 25 LowIR
mismatches.  The status groups are earlier deduction/lookup (6), the five
general/400 status cases above, general/500 owner/alias/pack/conversion replay
(19), and specification-band deduction/conversion/replay (19); the LowIR
groups are earlier partial-ordering (4), the three general/400 cases above,
general/500 result/alias replay (7), and specification-band normalization
(11).  The exact fixture lists remain in the preceding audit and the report
artifacts under `tests/**.my.*` are the current evidence.

## Next Checkpoint Scope — general/400 remainder

Complete the seven remaining general/400 fixtures as one coherent owner-replay
increment: alias-template and template-template member lookup, conversion
function result selection, callable assignment result typing, inherited
constructor visibility, and the three corresponding LowIR canonicalization
paths.  Validate the seven fixtures with the real runner arguments, then rerun
the full PA22 report, through-PA21, and file audit.  General/500 and spec-band
replay remain the next groups if this scope completes.

## Checkpoint 94 result — 2026-07-26

The turn-start baseline was **178/250**.  The completed increment reaches
**186/250** (+8).  It completed typed pointer/reference and enum non-type
argument normalization, C-style null-pointer handling, typed variable lookup
for bare reference arguments, function-pointer overload-set validation,
constructor materialization/default replay, delegating/inherited constructor
replay, alias-pack/index-sequence deduction, and lvalue-reference constructor
ranking.  Focused checks passed for the object-pointer address, C-style null,
non-type reference, static-member function-pointer, overload-set ambiguity,
index-sequence alias, defaulted constructor-SFINAE, and forwarding-lvalue
fixtures.

Required validation is green through PA21 (**1850/1850**), and the PA22 file
audit passes with warnings only.  The current PA22 report is authoritative for
the remaining map below; the 64 failures are grouped by shared behavior.

### Remaining Work Map

- **Early deduction/ordering and pack replay (10):** status
  `general/100-defaulted-nested-class-template-deduction`,
  `general/100-fixed-over-empty-trailing-pack-index-sequence`,
  `general/100-template-deduction-rejects-value-base-argument`,
  `general/200-constructor-template-rvalue-beats-const-ref`,
  `general/300-alias-bool-explicit-pack-call-dependent-tag`, and
  `general/300-hidden-friend-sfinae-use-scope-shadowing`; LowIR
  `general/100-dependent-remove-reference-transform-forwarding`,
  `general/100-function-parameter-empty-middle-pack-alias`,
  `general/200-function-template-partial-order-class-template-cv`, and
  `general/200-partial-order-synthetic-virtual-member-emission`.
- **General/400 alias, conversion, constructor, and LowIR replay (8):** status
  `general/400-alias-template-function-argument-cv`,
  `general/400-conversion-function-template-prefers-nontemplate`,
  `general/400-member-alias-template-template-partial-deduction-owner`,
  `general/400-pack-expansion-size-mismatch-sfinae`, and
  `general/400-partial-specialization-inherited-constructor-template`; LowIR
  `general/400-function-assignment-invocable-and-helper`,
  `general/400-static-data-nttp-pack-sizeof-bound`, and
  `general/400-template-template-alias-default-arity-sfinae`.
- **General/500 owner/alias/pack/result replay (19):** status
  `general/500-adl-alias-return-operator-template`,
  `general/500-alias-pack-enable-if-constexpr-constructor`,
  `general/500-async-initiate-dependent-return-sfinae`,
  `general/500-boost-mp11-conditional-alias-reference-set`,
  `general/500-constructor-sfinae-member-template-value`,
  `general/500-defaulted-nontype-qualified-alias-value`,
  `general/500-dependent-result-sizeof-sfinae-base`,
  `general/500-explicit-pack-deduced-pack-member-result`,
  `general/500-inherited-constructor-template-member-alias-pack` (timeout),
  `general/500-member-template-dependent-owner-defaulted-sfinae`,
  `general/500-member-template-retained-dependent-param-candidate-drop`,
  `general/500-owner-enum-nontype-result-sfinae`,
  `general/500-partial-specialization-cv-qualifier-subset`,
  `general/500-short-circuit-alias-member-sfinae`,
  `general/500-sizeof-void-sfinae-fallback`, and
  `general/500-weak-ptr-shared-ptr-template-ctor`; LowIR
  `general/500-bool-alias-function-template-result-metadata`,
  `general/500-current-specialization-nontype-default-dependent`, and
  `general/500-dependent-member-alias-function-return`.
- **Specification-band deduction/conversion/replay (27):** spec/100 status
  `constructor-template-braced-array-bound-deduction`,
  `defaulted-nested-class-template-deduction`, and
  `function-template-array-bound-braced-empty-argument`; spec/100 LowIR
  `explicit-specialization-dependent-param-typedef`,
  `explicit-template-argument-overload-rejects-short-candidate`, and
  `function-template-array-parameter-string-literal`.  Spec/200 status
  `function-template-class-template-param-partial-order` and
  `member-operator-fixed-tag-default-partial-order`; spec/200 LowIR
  `constructor-template-qualified-nested-id-partial-ordering`,
  `defaulted-class-template-argument-prefix-deduction`,
  `dependent-specialized-default-arg-deduction`, and
  `member-template-nontype-param-shadows-inherited-value-sum`.  Spec/300
  status `constructor-template-const-ref-conversion`,
  `conversion-function-template-owner-result-copy-init`,
  `defaulted-nttp-alias-pack-sfinae-bad`, and
  `template-id-direct-parameter-same-name-deduction`; spec/300 LowIR
  `cross-specialization-converting-ctor-operator-template` and
  `qualified-member-function-value-fallback-sfinae`.  Spec/400 LowIR
  `dependent-decltype-member-template-conversion-operator`.  Spec/500 status
  `conversion-function-template-reference-conditional-auto-ref`,
  `conversion-function-template-same-name-target`,
  `defaulted-rebind-constructor-deduction`,
  `function-result-template-id-shadowed-argument`,
  `hidden-friend-query-free-decltype-noexcept`,
  `template-template-conversion-operator-reference-target`, and
  `template-template-piecewise-partial-ordering`; spec/500 LowIR
  `type-pack-qualified-static-member-expansion`.

### Next Checkpoint Scope

Take the general/500 owner/alias/pack/result group as the next coherent
increment, prioritizing dependent default-SFINAE boundaries, concrete-owner
replay, and alias-pack expansion.  Validate its status and LowIR fixtures,
then rerun the full PA22 report, through-PA21, and file audit.

## Checkpoint 95 scope — before implementation — 2026-07-27

- The current-PA report was revalidated at **186/250**; the complete 64-fixture
  failure set remains grouped above.
- **Remaining Work Map for this checkpoint:** select eight related general/500
  failures: `boost-mp11-conditional-alias-reference-set`,
  `alias-pack-enable-if-constexpr-constructor`,
  `defaulted-nontype-qualified-alias-value`,
  `dependent-result-sizeof-sfinae-base`,
  `explicit-pack-deduced-pack-member-result`,
  `owner-enum-nontype-result-sfinae`,
  `short-circuit-alias-member-sfinae`, and
  `sizeof-void-sfinae-fallback`. The remaining 56 failures stay in the
  earlier deduction/ordering, general/400, remaining general/500, and
  specification-band groups already listed above.
- **Checkpoint Scope:** preserve dependent alias/member structure through
  template substitution, evaluate dependent `sizeof`/enum/non-type defaults
  only after owner arguments are concrete, and discard invalid alias/SFINAE
  candidates without leaking dependent identifiers into ordinary lookup.
- **Validation scope:** the selected fixtures, the full PA22 report,
  through-PA21 regression, and the PA22 source file audit. The next checkpoint
  group is the remaining general/500 owner/constructor/ADL replay.

## Checkpoint 95 result — 2026-07-27

The authoritative current-PA report is **187/250**, an improvement over the
turn-start **178/250**.  The focused checkpoint fixtures that now pass are
`general/500-dependent-result-sizeof-sfinae-base`,
`general/500-short-circuit-alias-member-sfinae`, and
`general/500-sizeof-void-sfinae-fallback`.

The completed behavior is grouped as follows:

- **Candidate-local default validation:** explicit and deduced function
  candidates now validate defaulted logical `enable_if` non-type arguments
  against their local type and pack bindings, dropping false or invalid
  candidates without leaking those bindings into ordinary lookup.
- **Dependent integral replay:** logical non-type arguments short-circuit
  before evaluating an invalid right operand; dependent `sizeof` expressions
  are expanded after substitution, while `void` and incomplete-class operands
  remain substitution failures.  Generated class layout is available to the
  typed size evaluator, and a legitimate overloaded call probe is not treated
  as an invalid unresolved `sizeof`.
- **Generated result lookup:** generated function result probes retain their
  owner context, match the generated template primary, and use a recursion
  guard for nested call-result deduction.

The complete remaining **63** failures are grouped by the next shared
behaviors: early deduction/pack/using and partial-order replay (11),
general/400 alias/conversion/constructor and LowIR replay (9), general/500
owner/alias/pack/conversion replay (16), and specification-band
deduction/conversion/LowIR replay (27).  The five carried general/500 cases
from this checkpoint are `boost-mp11-conditional-alias-reference-set`,
`alias-pack-enable-if-constexpr-constructor`,
`defaulted-nontype-qualified-alias-value`,
`explicit-pack-deduced-pack-member-result`, and
`owner-enum-nontype-result-sfinae`; the inherited-constructor timeout remains
in the same group.

Required validation passed: the exact through-PA21 report is **1850/1850**,
and `cppgm_file_audit.pl --stage pa22 --paths dev/src` passed with warnings
only.  The next checkpoint group is the remaining general/500 owner,
constructor, alias-pack, and ADL replay, starting with the five carried cases.

## Checkpoint 96 scope — before implementation — 2026-07-27

- The current-PA report was revalidated at **187/250** with 63 failures; the
  complete current set is the four grouped bands above.
- **Remaining Work Map for this checkpoint:** the five carried general/500
  cases are `boost-mp11-conditional-alias-reference-set`,
  `alias-pack-enable-if-constexpr-constructor`,
  `defaulted-nontype-qualified-alias-value`,
  `explicit-pack-deduced-pack-member-result`, and
  `owner-enum-nontype-result-sfinae`.  The other 58 failures remain in early
  deduction/ordering, general/400 replay, the remaining general/500 group,
  and specification-band replay.
- **Checkpoint Scope:** keep alias and pack bindings typed through concrete
  owner replay; evaluate defaulted logical and enum non-type expressions only
  after their owner and pack arguments are bound; validate explicit-pack
  function results before emitting a generated declaration; and preserve
  constexpr constructor candidates whose defaults are valid after alias
  substitution.
- **Validation scope:** the five selected fixtures, the authoritative full
  PA22 report, through-PA21, and the PA22 file audit.  The next checkpoint
  group is the remaining general/500 constructor/ADL and owner replay.

## Checkpoint 96 result — 2026-07-27

The authoritative current-PA report is now **194/250**, up from the
turn-start **178/250**.  All five selected fixtures pass in the full report:
`boost-mp11-conditional-alias-reference-set`,
`alias-pack-enable-if-constexpr-constructor`,
`defaulted-nontype-qualified-alias-value`,
`explicit-pack-deduced-pack-member-result`, and
`owner-enum-nontype-result-sfinae`.

The completed increment preserves typed bindings through concrete owner
replay, registers concrete generated nested-class layouts for dependent
`sizeof`, keeps repeated class-owner member templates in the correct lookup
set, replays generated free-function results in their lexical owner context,
and synchronizes multiple packs during one expansion.  Expanded function
parameter identifiers now recover the corresponding typed pack element, so
materialized constructors receive the actual per-element argument types.
Generated constructor candidates and ordinary typedefs are also available
during the constant-evaluation and early generated-scope passes.
The constant-evaluation owner helpers are factored into a registered source
module so the PA22 file-size audit remains within its fatal limits.

### Remaining Work Map

- **Early deduction, pack, using, and partial-order replay (11):** status
  `general/100-defaulted-nested-class-template-deduction`,
  `general/100-template-deduction-rejects-value-base-argument`,
  `general/200-constructor-template-rvalue-beats-const-ref`,
  `general/300-alias-bool-explicit-pack-call-dependent-tag`,
  `general/300-hidden-friend-sfinae-use-scope-shadowing`, and
  `general/300-using-member-template-implicit-object-cv-overload`; LowIR
  `general/100-dependent-remove-reference-transform-forwarding`,
  `general/100-fixed-over-empty-trailing-pack-index-sequence`,
  `general/100-function-parameter-empty-middle-pack-alias`,
  `general/200-function-template-partial-order-class-template-cv`, and
  `general/200-partial-order-synthetic-virtual-member-emission`.
- **General/400 alias, conversion, constructor, and LowIR replay (8):**
  status `general/400-alias-template-function-argument-cv`,
  `general/400-conversion-function-template-prefers-nontemplate`,
  `general/400-member-alias-template-template-partial-deduction-owner`,
  `general/400-pack-expansion-size-mismatch-sfinae`, and
  `general/400-partial-specialization-inherited-constructor-template`; LowIR
  `general/400-function-assignment-invocable-and-helper`,
  `general/400-static-data-nttp-pack-sizeof-bound`, and
  `general/400-template-template-alias-default-arity-sfinae`.
- **Remaining general/500 owner, alias, constructor, ADL, and result replay
  (11):** status `general/500-adl-alias-return-operator-template`,
  `general/500-async-initiate-dependent-return-sfinae`,
  `general/500-constructor-sfinae-member-template-value`,
  `general/500-inherited-constructor-template-member-alias-pack` (timeout),
  `general/500-member-template-dependent-owner-defaulted-sfinae`,
  `general/500-member-template-retained-dependent-param-candidate-drop`,
  `general/500-partial-specialization-cv-qualifier-subset`, and
  `general/500-weak-ptr-shared-ptr-template-ctor`; LowIR
  `general/500-bool-alias-function-template-result-metadata`,
  `general/500-current-specialization-nontype-default-dependent`, and
  `general/500-dependent-member-alias-function-return`.
- **Specification-band deduction, conversion, and LowIR replay (26):**
  status `spec/100-constructor-template-braced-array-bound-deduction`,
  `spec/100-defaulted-nested-class-template-deduction`,
  `spec/100-function-template-array-bound-braced-empty-argument`,
  `spec/200-function-template-class-template-param-partial-order`,
  `spec/200-member-operator-fixed-tag-default-partial-order`,
  `spec/300-constructor-template-const-ref-conversion`,
  `spec/300-conversion-function-template-owner-result-copy-init`,
  `spec/300-template-id-direct-parameter-same-name-deduction`,
  `spec/500-conversion-function-template-reference-conditional-auto-ref`,
  `spec/500-conversion-function-template-same-name-target`,
  `spec/500-defaulted-rebind-constructor-deduction`,
  `spec/500-function-result-template-id-shadowed-argument`,
  `spec/500-hidden-friend-query-free-decltype-noexcept`,
  `spec/500-template-template-conversion-operator-reference-target`, and
  `spec/500-template-template-piecewise-partial-ordering`; LowIR
  `spec/100-explicit-specialization-dependent-param-typedef`,
  `spec/100-explicit-template-argument-overload-rejects-short-candidate`,
  `spec/100-function-template-array-parameter-string-literal`,
  `spec/200-constructor-template-qualified-nested-id-partial-ordering`,
  `spec/200-defaulted-class-template-argument-prefix-deduction`,
  `spec/200-dependent-specialized-default-arg-deduction`,
  `spec/200-member-template-nontype-param-shadows-inherited-value-sum`,
  `spec/300-cross-specialization-converting-ctor-operator-template`,
  `spec/300-qualified-member-function-value-fallback-sfinae`,
  `spec/400-dependent-decltype-member-template-conversion-operator`, and
  `spec/500-type-pack-qualified-static-member-expansion`.

### Next Checkpoint Scope

Take the remaining general/500 group as the next coherent increment:
inherited/member constructor replay, dependent owner defaults, ADL alias
return lookup, and the remaining partial-specialization/conversion result
selection.  Include its three LowIR result-materialization cases, then rerun
the full PA22 report, through-PA21, and the PA22 file audit.

## Checkpoint 97 scope — before implementation — 2026-07-27

- The current-PA report was revalidated at **194/250** before this increment;
  the complete failure set was grouped below by the shared early-deduction,
  general/400, general/500, and specification-band behaviors.
- **Remaining Work Map:** the selected general/500 owner-replay group was
  `adl-alias-return-operator-template`,
  `async-initiate-dependent-return-sfinae`,
  `member-template-dependent-owner-defaulted-sfinae`,
  `member-template-retained-dependent-param-candidate-drop`, and
  `partial-specialization-cv-qualifier-subset`; the other current failures
  remain in the four grouped bands listed in the result below.
- **Checkpoint Scope:** preserve typed enclosing-owner bindings while replaying
  member-template calls; resolve qualified dependent `decltype` calls with
  nested call arguments; recover default function parameters from the matching
  in-class declaration when materializing an out-of-class definition; and
  retain operator template-id using declarations without weakening ordinary
  template-id rejection.
- **Validation scope:** the selected general/500 behavior, the exact PA22
  report, through-PA21 regression, and the PA22 source file audit.

## Checkpoint 97 result — 2026-07-27

The authoritative current-PA report is **197/250**, up from the turn-start
**178/250** and the checkpoint baseline **194/250**.  The increment now
preserves typed owner facts through out-of-class member definitions, parses
qualified dependent member calls and nested call arguments during `decltype`
replay, restores matching declaration defaults before materialization, and
keeps operator template-id using declarations available to lookup.  The
general/500 ADL alias-return, async dependent-return, and cv-qualified partial
specialization cases no longer fail the exact report; the two retained/default
member replay cases now reach materialization but still have LowIR-shape work.

Required validation passed: the exact through-PA21 report is **1850/1850**, and
`cppgm_file_audit.pl --stage pa22 --paths dev/src` passed with warnings only.

### Remaining Work Map

- **Early deduction, pack, using, and partial-order replay (11):** status
  `general/100-defaulted-nested-class-template-deduction`,
  `general/100-template-deduction-rejects-value-base-argument`,
  `general/200-constructor-template-rvalue-beats-const-ref`,
  `general/300-alias-bool-explicit-pack-call-dependent-tag`,
  `general/300-hidden-friend-sfinae-use-scope-shadowing`, and
  `general/300-using-member-template-implicit-object-cv-overload`; LowIR
  `general/100-dependent-remove-reference-transform-forwarding`,
  `general/100-fixed-over-empty-trailing-pack-index-sequence`,
  `general/100-function-parameter-empty-middle-pack-alias`,
  `general/200-function-template-partial-order-class-template-cv`, and
  `general/200-partial-order-synthetic-virtual-member-emission`.
- **General/400 alias, conversion, constructor, and LowIR replay (8):** status
  `general/400-alias-template-function-argument-cv`,
  `general/400-conversion-function-template-prefers-nontemplate`,
  `general/400-member-alias-template-template-partial-deduction-owner`,
  `general/400-pack-expansion-size-mismatch-sfinae`, and
  `general/400-partial-specialization-inherited-constructor-template`; LowIR
  `general/400-function-assignment-invocable-and-helper`,
  `general/400-static-data-nttp-pack-sizeof-bound`, and
  `general/400-template-template-alias-default-arity-sfinae`.
- **Remaining general/500 owner, constructor, and result replay (8):** status
  `general/500-constructor-sfinae-member-template-value`,
  `general/500-inherited-constructor-template-member-alias-pack` (timeout),
  and `general/500-weak-ptr-shared-ptr-template-ctor`; LowIR
  `general/500-bool-alias-function-template-result-metadata`,
  `general/500-current-specialization-nontype-default-dependent`,
  `general/500-dependent-member-alias-function-return`,
  `general/500-member-template-dependent-owner-defaulted-sfinae`, and
  `general/500-member-template-retained-dependent-param-candidate-drop`.
- **Specification-band deduction, conversion, and LowIR replay (26):** the
  remaining `spec/100`, `spec/200`, `spec/300`, `spec/400`, and `spec/500`
  cases are unchanged from Checkpoint 96: 15 status failures covering
  constructor, conversion, partial-order, template-id, and hidden-friend
  lookup, plus 11 LowIR-shape failures covering explicit specialization,
  array/string arguments, nested/defaulted deduction, cross-specialization
  conversion, qualified-member fallback, and qualified static-member pack
  expansion.

### Next Checkpoint Scope

Finish the remaining general/500 constructor and result-materialization group,
starting with the two member replay LowIR cases and the inherited-constructor
timeout; then continue with the specification-band deduction/conversion group.

## Checkpoint 98 result — 2026-07-27

The authoritative current-PA report is **200/250**, above the Checkpoint 97
baseline and audit turn-start baseline of **197/250**.  Earlier assignments
remain **1850/1850**, and the PA22 file audit passes.  The recursive defaulted
nested-class failure is gone, constructor replay no longer re-enters an active
special-member declaration, and the inherited-constructor fixture completes
without a timeout.  The complete residual set is **26 exit-status failures**
and **24 LowIR-parity failures**, with **0 timeout failures**.

### Remaining Work Map

- **General exit-status deduction and replay (12):**
  `general/100-template-deduction-rejects-value-base-argument`,
  `general/200-constructor-template-rvalue-beats-const-ref`,
  `general/300-alias-bool-explicit-pack-call-dependent-tag`,
  `general/300-hidden-friend-sfinae-use-scope-shadowing`,
  `general/300-using-member-template-implicit-object-cv-overload`,
  `general/400-alias-template-function-argument-cv`,
  `general/400-conversion-function-template-prefers-nontemplate`,
  `general/400-member-alias-template-template-partial-deduction-owner`,
  `general/400-pack-expansion-size-mismatch-sfinae`,
  `general/400-partial-specialization-inherited-constructor-template`,
  `general/500-constructor-sfinae-member-template-value`, and
  `general/500-weak-ptr-shared-ptr-template-ctor`.
- **General LowIR replay (13):**
  `general/100-fixed-over-empty-trailing-pack-index-sequence`,
  `general/100-function-parameter-empty-middle-pack-alias`,
  `general/200-function-template-partial-order-class-template-cv`,
  `general/200-partial-order-synthetic-virtual-member-emission`,
  `general/400-function-assignment-invocable-and-helper`,
  `general/400-static-data-nttp-pack-sizeof-bound`,
  `general/400-template-template-alias-default-arity-sfinae`,
  `general/500-bool-alias-function-template-result-metadata`,
  `general/500-current-specialization-nontype-default-dependent`,
  `general/500-dependent-member-alias-function-return`,
  `general/500-inherited-constructor-template-member-alias-pack`,
  `general/500-member-template-dependent-owner-defaulted-sfinae`, and
  `general/500-member-template-retained-dependent-param-candidate-drop`.
- **Specification exit-status deduction and conversion (14):**
  `spec/100-constructor-template-braced-array-bound-deduction`,
  `spec/100-function-template-array-bound-braced-empty-argument`,
  `spec/200-function-template-class-template-param-partial-order`,
  `spec/200-member-operator-fixed-tag-default-partial-order`,
  `spec/300-constructor-template-const-ref-conversion`,
  `spec/300-conversion-function-template-owner-result-copy-init`,
  `spec/300-template-id-direct-parameter-same-name-deduction`,
  `spec/500-conversion-function-template-reference-conditional-auto-ref`,
  `spec/500-conversion-function-template-same-name-target`,
  `spec/500-defaulted-rebind-constructor-deduction`,
  `spec/500-function-result-template-id-shadowed-argument`,
  `spec/500-hidden-friend-query-free-decltype-noexcept`,
  `spec/500-template-template-conversion-operator-reference-target`, and
  `spec/500-template-template-piecewise-partial-ordering`.
- **Specification LowIR replay (11):**
  `spec/100-explicit-specialization-dependent-param-typedef`,
  `spec/100-explicit-template-argument-overload-rejects-short-candidate`,
  `spec/100-function-template-array-parameter-string-literal`,
  `spec/200-constructor-template-qualified-nested-id-partial-ordering`,
  `spec/200-defaulted-class-template-argument-prefix-deduction`,
  `spec/200-dependent-specialized-default-arg-deduction`,
  `spec/200-member-template-nontype-param-shadows-inherited-value-sum`,
  `spec/300-cross-specialization-converting-ctor-operator-template`,
  `spec/300-qualified-member-function-value-fallback-sfinae`,
  `spec/400-dependent-decltype-member-template-conversion-operator`, and
  `spec/500-type-pack-qualified-static-member-expansion`.

### Next Checkpoint Scope

Take the remaining general/500 constructor, owner, and result-materialization
group as the next substantial increment: the two constructor-SFINAE/status
cases, the weak/shared-pointer constructor, and the six general/500 LowIR
cases.  The inherited-constructor case is now a bounded LowIR selection
problem rather than a timeout.  After that group, bundle the smaller
general/400 and specification conversion/partial-order bands before the
remaining specification LowIR cases.

## Checkpoint 99 scope — before implementation — 2026-07-27

### Current failure audit

The required `make test-report ACTIVE_TEST_REPORT_PAS='pa22'` report was
refreshed immediately before this checkpoint: **200/250** pass, earlier
assignments are green, and there are **50** PA22 failures with no timeout.
The complete current failure set is grouped by primary shared behavior below;
the LowIR groups are parity failures after the harness's relaxed canonical
comparison, while the other entries are exit-status failures.

### Remaining Work Map

- **General exit-status deduction and replay (12):**
  `general/100-template-deduction-rejects-value-base-argument`,
  `general/200-constructor-template-rvalue-beats-const-ref`,
  `general/300-alias-bool-explicit-pack-call-dependent-tag`,
  `general/300-hidden-friend-sfinae-use-scope-shadowing`,
  `general/300-using-member-template-implicit-object-cv-overload`,
  `general/400-alias-template-function-argument-cv`,
  `general/400-conversion-function-template-prefers-nontemplate`,
  `general/400-member-alias-template-template-partial-deduction-owner`,
  `general/400-pack-expansion-size-mismatch-sfinae`,
  `general/400-partial-specialization-inherited-constructor-template`,
  `general/500-constructor-sfinae-member-template-value`, and
  `general/500-weak-ptr-shared-ptr-template-ctor`.
- **General LowIR replay (13):**
  `general/100-fixed-over-empty-trailing-pack-index-sequence`,
  `general/100-function-parameter-empty-middle-pack-alias`,
  `general/200-function-template-partial-order-class-template-cv`,
  `general/200-partial-order-synthetic-virtual-member-emission`,
  `general/400-function-assignment-invocable-and-helper`,
  `general/400-static-data-nttp-pack-sizeof-bound`,
  `general/400-template-template-alias-default-arity-sfinae`,
  `general/500-bool-alias-function-template-result-metadata`,
  `general/500-current-specialization-nontype-default-dependent`,
  `general/500-dependent-member-alias-function-return`,
  `general/500-inherited-constructor-template-member-alias-pack`,
  `general/500-member-template-dependent-owner-defaulted-sfinae`, and
  `general/500-member-template-retained-dependent-param-candidate-drop`.
- **Specification exit-status deduction and conversion (14):**
  `spec/100-constructor-template-braced-array-bound-deduction`,
  `spec/100-function-template-array-bound-braced-empty-argument`,
  `spec/200-function-template-class-template-param-partial-order`,
  `spec/200-member-operator-fixed-tag-default-partial-order`,
  `spec/300-constructor-template-const-ref-conversion`,
  `spec/300-conversion-function-template-owner-result-copy-init`,
  `spec/300-template-id-direct-parameter-same-name-deduction`,
  `spec/500-conversion-function-template-reference-conditional-auto-ref`,
  `spec/500-conversion-function-template-same-name-target`,
  `spec/500-defaulted-rebind-constructor-deduction`,
  `spec/500-function-result-template-id-shadowed-argument`,
  `spec/500-hidden-friend-query-free-decltype-noexcept`,
  `spec/500-template-template-conversion-operator-reference-target`, and
  `spec/500-template-template-piecewise-partial-ordering`.
- **Specification LowIR replay (11):**
  `spec/100-explicit-specialization-dependent-param-typedef`,
  `spec/100-explicit-template-argument-overload-rejects-short-candidate`,
  `spec/100-function-template-array-parameter-string-literal`,
  `spec/200-constructor-template-qualified-nested-id-partial-ordering`,
  `spec/200-defaulted-class-template-argument-prefix-deduction`,
  `spec/200-dependent-specialized-default-arg-deduction`,
  `spec/200-member-template-nontype-param-shadows-inherited-value-sum`,
  `spec/300-cross-specialization-converting-ctor-operator-template`,
  `spec/300-qualified-member-function-value-fallback-sfinae`,
  `spec/400-dependent-decltype-member-template-conversion-operator`, and
  `spec/500-type-pack-qualified-static-member-expansion`.

### Checkpoint Scope

Complete the eight-fixture **general/500 owner, constructor, and result
materialization** slice: preserve typed enclosing-owner and member-template
bindings while replaying constructor candidates; apply immediate-context
SFINAE to constructor member values and weak/shared-pointer converting
constructors; and make the resulting selected declarations, defaulted
non-type values, dependent member aliases, and inherited constructor packs
lower to the same LowIR as the reference.  The three status/replay cases are
`general/500-constructor-sfinae-member-template-value`,
`general/500-weak-ptr-shared-ptr-template-ctor`, and
`general/500-inherited-constructor-template-member-alias-pack`; the five
additional LowIR cases are
`general/500-bool-alias-function-template-result-metadata`,
`general/500-current-specialization-nontype-default-dependent`,
`general/500-dependent-member-alias-function-return`,
`general/500-member-template-dependent-owner-defaulted-sfinae`, and
`general/500-member-template-retained-dependent-param-candidate-drop`.
Validation is the eight focused fixtures, the full PA22 report, through-PA21,
and the PA22 file audit.  The remaining 45 failures stay mapped above; the
next checkpoint bundles general/400 conversion/alias replay with the smaller
general/specification partial-order status group.

## Checkpoint 99 result — 2026-07-27

The selected owner/SFINAE slice is complete.  The clean current-PA report is
**205/250**, up from the turn-start baseline of **200/250** (and the
post-checkpoint baseline of 203/250); earlier assignments remain green.  The
two newly fixed fixtures are
`general/500-member-template-dependent-owner-defaulted-sfinae` and
`general/500-member-template-retained-dependent-param-candidate-drop`.
The previously repaired constructor-SFINAE, weak/shared-pointer constructor,
and bool-alias result cases remain passing.  The PA22 file audit also passes.

The implementation now carries declared alias-template defaults while matching
class partial specializations, evaluates dependent boolean arguments for the
`enable_if`/`disable_if` SFINAE boundary, and preserves the selected SFINAE
partial declaration during member-type lookup.  Dependent member probes receive
the concrete function-template substitutions before class selection, while
ordinary pack/type lookup remains unchanged.

### Remaining Work Map

- **General exit-status deduction/replay (10):**
  `general/100-template-deduction-rejects-value-base-argument`,
  `general/200-constructor-template-rvalue-beats-const-ref`,
  `general/300-alias-bool-explicit-pack-call-dependent-tag`,
  `general/300-hidden-friend-sfinae-use-scope-shadowing`,
  `general/300-using-member-template-implicit-object-cv-overload`,
  `general/400-alias-template-function-argument-cv`,
  `general/400-conversion-function-template-prefers-nontemplate`,
  `general/400-member-alias-template-template-partial-deduction-owner`,
  `general/400-pack-expansion-size-mismatch-sfinae`, and
  `general/400-partial-specialization-inherited-constructor-template`.
- **General LowIR replay/materialization (10):**
  `general/100-fixed-over-empty-trailing-pack-index-sequence`,
  `general/100-function-parameter-empty-middle-pack-alias`,
  `general/200-function-template-partial-order-class-template-cv`,
  `general/200-partial-order-synthetic-virtual-member-emission`,
  `general/400-function-assignment-invocable-and-helper`,
  `general/400-static-data-nttp-pack-sizeof-bound`,
  `general/400-template-template-alias-default-arity-sfinae`,
  `general/500-current-specialization-nontype-default-dependent`,
  `general/500-dependent-member-alias-function-return`, and
  `general/500-inherited-constructor-template-member-alias-pack`.
- **Specification exit-status deduction/conversion (14):** the existing
  `spec/100`, `spec/200`, `spec/300`, and `spec/500` constructor, conversion,
  partial-order, hidden-friend, and template-template cases remain grouped in
  the prior map.
- **Specification LowIR replay (11):** the existing explicit-specialization,
  array/string argument, nested/defaulted deduction, cross-specialization,
  qualified-member fallback, and qualified static-member pack cases remain
  grouped in the prior map.

### Next Checkpoint Scope

Take the remaining general/400 alias, conversion, and partial-order status
group as one coherent increment, bundling the smaller specification
partial-order/conversion status cases.  Validate the focused group, the full
PA22 report, through-PA21, and the PA22 file audit.

## Checkpoint 100 result — 2026-07-27

The recorded eight-fixture owner/constructor/result scope is now complete:
the focused check passes **8/8**, the current PA22 report is **208/250**
(42 failures, up from the turn-start baseline of 200), through-PA21 passes
**1850/1850**, and the PA22 file audit passes.  The final scope fixes retain
the typed inherited-constructor wrapper boundary, keep a required out-of-class
template constructor entry while suppressing uncalled inherited duplicates,
and preserve the signedness conversion copy at a dependent alias return.

### Remaining Work Map

- **General exit-status deduction/replay (10):**
  `general/100-template-deduction-rejects-value-base-argument`,
  `general/200-constructor-template-rvalue-beats-const-ref`,
  `general/300-alias-bool-explicit-pack-call-dependent-tag`,
  `general/300-hidden-friend-sfinae-use-scope-shadowing`,
  `general/300-using-member-template-implicit-object-cv-overload`,
  `general/400-alias-template-function-argument-cv`,
  `general/400-conversion-function-template-prefers-nontemplate`,
  `general/400-member-alias-template-template-partial-deduction-owner`,
  `general/400-pack-expansion-size-mismatch-sfinae`, and
  `general/400-partial-specialization-inherited-constructor-template`.
- **General LowIR replay/materialization (7):**
  `general/100-fixed-over-empty-trailing-pack-index-sequence`,
  `general/100-function-parameter-empty-middle-pack-alias`,
  `general/200-function-template-partial-order-class-template-cv`,
  `general/200-partial-order-synthetic-virtual-member-emission`,
  `general/400-function-assignment-invocable-and-helper`,
  `general/400-static-data-nttp-pack-sizeof-bound`, and
  `general/400-template-template-alias-default-arity-sfinae`.
- **Specification exit-status deduction/conversion (14):**
  `spec/100-constructor-template-braced-array-bound-deduction`,
  `spec/100-function-template-array-bound-braced-empty-argument`,
  `spec/200-function-template-class-template-param-partial-order`,
  `spec/200-member-operator-fixed-tag-default-partial-order`,
  `spec/300-constructor-template-const-ref-conversion`,
  `spec/300-conversion-function-template-owner-result-copy-init`,
  `spec/300-template-id-direct-parameter-same-name-deduction`,
  `spec/500-conversion-function-template-reference-conditional-auto-ref`,
  `spec/500-conversion-function-template-same-name-target`,
  `spec/500-defaulted-rebind-constructor-deduction`,
  `spec/500-function-result-template-id-shadowed-argument`,
  `spec/500-hidden-friend-query-free-decltype-noexcept`,
  `spec/500-template-template-conversion-operator-reference-target`, and
  `spec/500-template-template-piecewise-partial-ordering`.
- **Specification LowIR replay (11):**
  `spec/100-explicit-specialization-dependent-param-typedef`,
  `spec/100-explicit-template-argument-overload-rejects-short-candidate`,
  `spec/100-function-template-array-parameter-string-literal`,
  `spec/200-constructor-template-qualified-nested-id-partial-ordering`,
  `spec/200-defaulted-class-template-argument-prefix-deduction`,
  `spec/200-dependent-specialized-default-arg-deduction`,
  `spec/200-member-template-nontype-param-shadows-inherited-value-sum`,
  `spec/300-cross-specialization-converting-ctor-operator-template`,
  `spec/300-qualified-member-function-value-fallback-sfinae`,
  `spec/400-dependent-decltype-member-template-conversion-operator`, and
  `spec/500-type-pack-qualified-static-member-expansion`.

### Next Checkpoint Scope

Take the remaining general/400 alias, conversion, and inherited-constructor
status group together with its three general LowIR materialization cases,
then bundle the smaller specification partial-order/conversion status group.

## Checkpoint 100 audit result — 2026-07-27

The checkpoint audit found and fixed a stringly out-of-class-definition test
in constructor lowering and an avoidable duplicate base-entry scan.  The
semantic fact now travels from typed collection records through base entries;
the inherited-constructor wrapper boundary and dependent return conversion
remain typed.  The eight-fixture focus is **8/8**, through-PA21 is
**1850/1850**, and the PA22 file audit passes with the unchanged 12 warnings.

The fresh active report remains **208/250**, above the turn-start baseline of
**200/250**, with no timeout and no new failure name.  The Remaining Work Map
above is refreshed against the complete 42-fixture report: 10 general
exit-status, 7 general LowIR, 14 specification exit-status, and 11
specification LowIR cases.  The next substantial checkpoint is the grouped
general/400 alias, conversion, and inherited-constructor status/materialization
slice, followed by the smaller specification partial-order/conversion group.

## Checkpoint 101 scope — 2026-07-27 (before implementation)

### Current failure audit

The required live report is **208/250**, with earlier assignments passing and
42 PA22 failures.  The complete failure set is grouped by primary behavior:

- **Call deduction and partial ordering (10):**
  `general/100-fixed-over-empty-trailing-pack-index-sequence`,
  `general/100-function-parameter-empty-middle-pack-alias`,
  `general/200-function-template-partial-order-class-template-cv`,
  `general/200-partial-order-synthetic-virtual-member-emission`,
  `spec/100-constructor-template-braced-array-bound-deduction`,
  `spec/100-function-template-array-bound-braced-empty-argument`,
  `spec/200-constructor-template-qualified-nested-id-partial-ordering`,
  `spec/200-defaulted-class-template-argument-prefix-deduction`,
  `spec/200-dependent-specialized-default-arg-deduction`, and
  `spec/200-function-template-class-template-param-partial-order`.
- **Substitution, lookup, and pack SFINAE (7):**
  `general/100-template-deduction-rejects-value-base-argument`,
  `general/300-alias-bool-explicit-pack-call-dependent-tag`,
  `general/300-hidden-friend-sfinae-use-scope-shadowing`,
  `general/300-using-member-template-implicit-object-cv-overload`,
  `general/400-pack-expansion-size-mismatch-sfinae`,
  `spec/200-member-operator-fixed-tag-default-partial-order`, and
  `spec/500-hidden-friend-query-free-decltype-noexcept`.
- **Constructor and conversion candidate selection (11):**
  `general/200-constructor-template-rvalue-beats-const-ref`,
  `general/400-conversion-function-template-prefers-nontemplate`,
  `general/400-partial-specialization-inherited-constructor-template`,
  `spec/300-constructor-template-const-ref-conversion`,
  `spec/300-conversion-function-template-owner-result-copy-init`,
  `spec/300-template-id-direct-parameter-same-name-deduction`,
  `spec/500-conversion-function-template-reference-conditional-auto-ref`,
  `spec/500-conversion-function-template-same-name-target`,
  `spec/500-defaulted-rebind-constructor-deduction`,
  `spec/500-function-result-template-id-shadowed-argument`, and
  `spec/500-template-template-conversion-operator-reference-target`.
- **Alias, owner, template-template, and typed LowIR replay (14):**
  `general/400-alias-template-function-argument-cv`,
  `general/400-function-assignment-invocable-and-helper`,
  `general/400-member-alias-template-template-partial-deduction-owner`,
  `general/400-static-data-nttp-pack-sizeof-bound`,
  `general/400-template-template-alias-default-arity-sfinae`,
  `spec/100-explicit-specialization-dependent-param-typedef`,
  `spec/100-explicit-template-argument-overload-rejects-short-candidate`,
  `spec/100-function-template-array-parameter-string-literal`,
  `spec/200-member-template-nontype-param-shadows-inherited-value-sum`,
  `spec/300-cross-specialization-converting-ctor-operator-template`,
  `spec/300-qualified-member-function-value-fallback-sfinae`,
  `spec/400-dependent-decltype-member-template-conversion-operator`,
  `spec/500-template-template-piecewise-partial-ordering`, and
  `spec/500-type-pack-qualified-static-member-expansion`.

The buckets overlap at the semantic boundaries, but every name in the live
report appears exactly once in this primary map.

### Checkpoint Scope

Complete the **braced/array-bound deduction and constructor-candidate** slice:
carry empty braced-init arguments as typed non-deduced candidates, preserve
reference-to-array element and bound facts while matching a later deduced
parameter, and replay the selected constructor with forwarding- and
const-reference conversion ranking.  The focused fixtures are
`spec/100-constructor-template-braced-array-bound-deduction`,
`spec/100-function-template-array-bound-braced-empty-argument`,
`general/200-constructor-template-rvalue-beats-const-ref`,
`spec/300-constructor-template-const-ref-conversion`,
`spec/300-template-id-direct-parameter-same-name-deduction`, and
`spec/500-defaulted-rebind-constructor-deduction`.  The implementation must
keep candidate-local deduction facts in typed state and leave invalid
substitutions to candidate selection rather than turning them into global
diagnostics.  Validation is the focused group, the full PA22 report,
through-PA21, and the PA22 file audit; the remaining 36 failures stay mapped
above, with the next group being conversion/template-template owner replay.

## Checkpoint 101 result — 2026-07-27

The selected six-fixture scope is complete: all six focused tests pass their
checked LowIR comparisons.  The clean current-PA report improved from
**208/250** to **214/250** with no timeout and no new failure name.  The
increment preserves empty braced-list deduction facts, materializes bounded
array-reference temporaries, keeps dependent forwarding calls deferred until
member-template parameters are bound, and materializes converting constructor
specializations when the target class appears only in an ordinary function
parameter.  Empty-class value transfers now still run cross-specialization
constructors.

The final checkpoint validation passes through-PA21 at **1850/1850** and the
PA22 file audit passes with the repository's existing warnings.  The lowering
increment keeps derived-to-base value transfers on their prior temporary path
and limits replay-only constant folding to constructor bodies, preserving the
earlier LowIR contract while retaining the six-fixture gain.

### Checkpoint Audit Result

The audit removed the conversion-replay path's pre-selection scan over every
visible template definition.  Early replay now requires the unique ordinary
function signature, reuses indexed class declarations, and leaves template
candidate ranking to the normal candidate path.  PA14 now performs the extra
source-type inference only for empty-storage objects, where the new
cross-specialization constructor behavior needs it.  The fresh report remains
**214/250**, with the same complete 36-fixture residual and no timeout; the
map below is therefore refreshed and unchanged.  The next substantial group
remains conversion/template-template owner replay plus the smaller
substitution/lookup/pack-SFINAE group.

### Remaining Work Map

The live report has **36** failures, grouped by primary behavior:

- **Call deduction, partial ordering, and pack boundaries (11):**
  `general/100-fixed-over-empty-trailing-pack-index-sequence`,
  `general/100-function-parameter-empty-middle-pack-alias`,
  `general/100-template-deduction-rejects-value-base-argument`,
  `general/200-function-template-partial-order-class-template-cv`,
  `general/200-partial-order-synthetic-virtual-member-emission`,
  `spec/200-constructor-template-qualified-nested-id-partial-ordering`,
  `spec/200-defaulted-class-template-argument-prefix-deduction`,
  `spec/200-dependent-specialized-default-arg-deduction`,
  `spec/200-function-template-class-template-param-partial-order`,
  `spec/200-member-operator-fixed-tag-default-partial-order`, and
  `spec/500-template-template-piecewise-partial-ordering`.
- **Substitution, lookup, and pack SFINAE (5):**
  `general/300-alias-bool-explicit-pack-call-dependent-tag`,
  `general/300-hidden-friend-sfinae-use-scope-shadowing`,
  `general/300-using-member-template-implicit-object-cv-overload`,
  `general/400-pack-expansion-size-mismatch-sfinae`, and
  `spec/500-hidden-friend-query-free-decltype-noexcept`.
- **Constructor and conversion candidate selection (8):**
  `general/400-conversion-function-template-prefers-nontemplate`,
  `general/400-partial-specialization-inherited-constructor-template`,
  `spec/300-conversion-function-template-owner-result-copy-init`,
  `spec/300-cross-specialization-converting-ctor-operator-template`,
  `spec/500-conversion-function-template-reference-conditional-auto-ref`,
  `spec/500-conversion-function-template-same-name-target`,
  `spec/500-function-result-template-id-shadowed-argument`, and
  `spec/500-template-template-conversion-operator-reference-target`.
- **Alias, owner, template-template, and typed LowIR replay (12):**
  `general/400-alias-template-function-argument-cv`,
  `general/400-function-assignment-invocable-and-helper`,
  `general/400-member-alias-template-template-partial-deduction-owner`,
  `general/400-static-data-nttp-pack-sizeof-bound`,
  `general/400-template-template-alias-default-arity-sfinae`,
  `spec/100-explicit-specialization-dependent-param-typedef`,
  `spec/100-explicit-template-argument-overload-rejects-short-candidate`,
  `spec/100-function-template-array-parameter-string-literal`,
  `spec/200-member-template-nontype-param-shadows-inherited-value-sum`,
  `spec/300-qualified-member-function-value-fallback-sfinae`,
  `spec/400-dependent-decltype-member-template-conversion-operator`, and
  `spec/500-type-pack-qualified-static-member-expansion`.

### Next Checkpoint Scope

Take the remaining conversion/candidate group together with the smaller
substitution/lookup/pack-SFINAE group.  Preserve typed owner and immediate-
context facts while resolving conversion-function templates, inherited
constructors, hidden friends, and defaulted/template-template candidates;
validate the focused group, full PA22, through-PA21, and the PA22 file audit.

## Checkpoint 102 scope — 2026-07-27 (before implementation)

### Remaining Work Map

The live `make test-report ACTIVE_TEST_REPORT_PAS='pa22'` baseline is
**214/250**, with **36** failures and no timeout.  The complete set is grouped
by primary behavior:

- **Call deduction, ordering, and pack boundaries (11):**
  `general/100-fixed-over-empty-trailing-pack-index-sequence`,
  `general/100-function-parameter-empty-middle-pack-alias`,
  `general/100-template-deduction-rejects-value-base-argument`,
  `general/200-function-template-partial-order-class-template-cv`,
  `general/200-partial-order-synthetic-virtual-member-emission`,
  `spec/200-constructor-template-qualified-nested-id-partial-ordering`,
  `spec/200-defaulted-class-template-argument-prefix-deduction`,
  `spec/200-dependent-specialized-default-arg-deduction`,
  `spec/200-function-template-class-template-param-partial-order`,
  `spec/200-member-operator-fixed-tag-default-partial-order`, and
  `spec/500-template-template-piecewise-partial-ordering`.
- **Substitution, lookup, and pack SFINAE (5):**
  `general/300-alias-bool-explicit-pack-call-dependent-tag`,
  `general/300-hidden-friend-sfinae-use-scope-shadowing`,
  `general/300-using-member-template-implicit-object-cv-overload`,
  `general/400-pack-expansion-size-mismatch-sfinae`, and
  `spec/500-hidden-friend-query-free-decltype-noexcept`.
- **Conversion and constructor candidate selection (8):**
  `general/400-conversion-function-template-prefers-nontemplate`,
  `general/400-partial-specialization-inherited-constructor-template`,
  `spec/300-conversion-function-template-owner-result-copy-init`,
  `spec/300-cross-specialization-converting-ctor-operator-template`,
  `spec/500-conversion-function-template-reference-conditional-auto-ref`,
  `spec/500-conversion-function-template-same-name-target`,
  `spec/500-function-result-template-id-shadowed-argument`, and
  `spec/500-template-template-conversion-operator-reference-target`.
- **Alias, owner, template-template, and typed LowIR replay (12):**
  `general/400-alias-template-function-argument-cv`,
  `general/400-function-assignment-invocable-and-helper`,
  `general/400-member-alias-template-template-partial-deduction-owner`,
  `general/400-static-data-nttp-pack-sizeof-bound`,
  `general/400-template-template-alias-default-arity-sfinae`,
  `spec/100-explicit-specialization-dependent-param-typedef`,
  `spec/100-explicit-template-argument-overload-rejects-short-candidate`,
  `spec/100-function-template-array-parameter-string-literal`,
  `spec/200-member-template-nontype-param-shadows-inherited-value-sum`,
  `spec/300-qualified-member-function-value-fallback-sfinae`,
  `spec/400-dependent-decltype-member-template-conversion-operator`, and
  `spec/500-type-pack-qualified-static-member-expansion`.

The status failures are the executable part of the first three groups; the
remaining 19 failures are LowIR comparisons.  No fixture from the live report
is omitted.  Several tests intentionally overlap (for example, conversion
templates also exercise owner replay), but the grouping identifies the shared
compiler path to change.

### Checkpoint Scope

Complete the **17-fixture candidate-status slice**: preserve typed owner and
template-template arguments through conversion-function and inherited-
constructor replay; prefer a viable non-template conversion over an equally
ranked conversion template; keep hidden-friend and using-imported lookup in
the immediate SFINAE context; reject mismatched multi-pack expansions as a
discarded candidate; and select the correct fixed/variadic member-template
candidate after defaults.  This scope covers the 17 status fixtures in the
first three groups, including the dependent conversion, function-result, and
template-template conversion cases.  The 19 LowIR-only cases remain explicit
follow-up work.

Validation for this checkpoint is the focused status group, the authoritative
full PA22 report, the through-PA21 report, and the PA22 file audit.  The next
checkpoint will address the remaining LowIR materialization/ordering group.

## Checkpoint 102 result — 2026-07-27

Implemented the conversion/constructor and typed LowIR boundary slice.  PA14
now ranks equally viable non-template conversions ahead of conversion
templates, preserves declared class-template assignments, materializes
class-valued conversions in the correct C++11 temporary boundary for
reference, by-address, and by-value arguments, and keeps cv-qualified class
views tied to canonical size/alignment facts.  PA18 now carries expected
conversion results through member-template deduction, replays
template-template conversion targets, preserves generated conversion return
types, and inserts only root helpers referenced by generated bodies.  The
lowering demand sweep closes over free helpers created while emitting those
bodies without retaining unused SFINAE candidates.

The live report is **218/250**, up four tests from the 214/250 baseline, with
no newly failing fixture names.  The repaired fixtures are
`general/400-conversion-function-template-prefers-nontemplate`,
`spec/500-conversion-function-template-reference-conditional-auto-ref`,
`spec/500-conversion-function-template-same-name-target`, and
`spec/500-template-template-conversion-operator-reference-target`; the
explicit user-conversion call path also remains passing after the temporary
boundary fix.

### Remaining Work Map

The exact live failure set has 32 fixtures:

- **Call deduction, partial ordering, and pack boundaries (12):**
  `general/100-fixed-over-empty-trailing-pack-index-sequence`,
  `general/100-function-parameter-empty-middle-pack-alias`,
  `general/100-template-deduction-rejects-value-base-argument`,
  `general/200-function-template-partial-order-class-template-cv`,
  `general/200-partial-order-synthetic-virtual-member-emission`,
  `spec/200-constructor-template-qualified-nested-id-partial-ordering`,
  `spec/200-defaulted-class-template-argument-prefix-deduction`,
  `spec/200-dependent-specialized-default-arg-deduction`,
  `spec/200-function-template-class-template-param-partial-order`,
  `spec/200-member-operator-fixed-tag-default-partial-order`,
  `spec/500-template-template-piecewise-partial-ordering`, and
  `spec/100-function-template-array-parameter-string-literal`.
- **Substitution, lookup, and pack SFINAE (5):**
  `general/300-alias-bool-explicit-pack-call-dependent-tag`,
  `general/300-hidden-friend-sfinae-use-scope-shadowing`,
  `general/300-using-member-template-implicit-object-cv-overload`,
  `general/400-pack-expansion-size-mismatch-sfinae`, and
  `spec/500-hidden-friend-query-free-decltype-noexcept`.
- **Conversion, constructor, and owner replay (4):**
  `general/400-partial-specialization-inherited-constructor-template`,
  `spec/300-conversion-function-template-owner-result-copy-init`,
  `spec/300-cross-specialization-converting-ctor-operator-template`, and
  `spec/500-function-result-template-id-shadowed-argument`.
- **Alias, template-template, and typed LowIR materialization (11):**
  `general/400-alias-template-function-argument-cv`,
  `general/400-function-assignment-invocable-and-helper`,
  `general/400-member-alias-template-template-partial-deduction-owner`,
  `general/400-static-data-nttp-pack-sizeof-bound`,
  `general/400-template-template-alias-default-arity-sfinae`,
  `spec/100-explicit-specialization-dependent-param-typedef`,
  `spec/100-explicit-template-argument-overload-rejects-short-candidate`,
  `spec/200-member-template-nontype-param-shadows-inherited-value-sum`,
  `spec/300-qualified-member-function-value-fallback-sfinae`,
  `spec/400-dependent-decltype-member-template-conversion-operator`, and
  `spec/500-type-pack-qualified-static-member-expansion`.

### Next Checkpoint Group

Take the 12-fixture call/partial-ordering group first, bundled with the
five-fixture substitution/pack-SFINAE group.  The following checkpoint will
then close the remaining alias/owner/template-template and typed LowIR cases.

## Checkpoint 102 validation — 2026-07-27

The complete current-PA report confirms **218/250**, four above the
214/250 turn-start baseline, with the exact 32-fixture remaining map above.
The focused conversion fixtures all pass, including the same-name target,
reference/conditional-auto-ref, template-template reference-target, and
non-template-preference cases.  `make test-report-through-pa21` passes all
**1850/1850** earlier tests.  The PA22 file audit passes with 12 existing
warnings; no fatal audit finding remains.  Temporary diagnostic tracing was
removed before this checkpoint was closed.

The next checkpoint remains the bundled call/partial-ordering plus
substitution/pack-SFINAE group, followed by the alias/owner/template-template
and typed-LowIR group listed in the remaining map.

## Checkpoint 103 scope — 2026-07-27 (before implementation)

### Remaining Work Map

The complete current-PA failure set is **26 fixtures** (the report baseline
for this checkpoint was **214/250**):

- **Candidate viability, lookup, and pack-SFINAE (11):**
  `general/100-template-deduction-rejects-value-base-argument`,
  `general/300-alias-bool-explicit-pack-call-dependent-tag`,
  `general/300-hidden-friend-sfinae-use-scope-shadowing`,
  `general/300-using-member-template-implicit-object-cv-overload`,
  `general/400-member-alias-template-template-partial-deduction-owner`,
  `general/400-pack-expansion-size-mismatch-sfinae`,
  `spec/200-member-operator-fixed-tag-default-partial-order`,
  `spec/300-conversion-function-template-owner-result-copy-init`,
  `spec/500-function-result-template-id-shadowed-argument`,
  `spec/500-hidden-friend-query-free-decltype-noexcept`, and
  `general/400-partial-specialization-inherited-constructor-template`.
- **Typed LowIR, alias, owner, and template-template replay (15):**
  `general/100-function-parameter-empty-middle-pack-alias`,
  `general/400-alias-template-function-argument-cv`,
  `general/400-function-assignment-invocable-and-helper`,
  `general/400-static-data-nttp-pack-sizeof-bound`,
  `general/400-template-template-alias-default-arity-sfinae`,
  `spec/100-explicit-specialization-dependent-param-typedef`,
  `spec/100-explicit-template-argument-overload-rejects-short-candidate`,
  `spec/100-function-template-array-parameter-string-literal`,
  `spec/200-defaulted-class-template-argument-prefix-deduction`,
  `spec/200-dependent-specialized-default-arg-deduction`,
  `spec/200-member-template-nontype-param-shadows-inherited-value-sum`,
  `spec/300-cross-specialization-converting-ctor-operator-template`,
  `spec/300-qualified-member-function-value-fallback-sfinae`,
  `spec/400-dependent-decltype-member-template-conversion-operator`, and
  `spec/500-type-pack-qualified-static-member-expansion`.

### Checkpoint Scope

Complete the coherent call-ordering and value-transfer increment: keep local
declaration types scoped in typed compiler state; order fixed and trailing
function packs; compare cv-qualified pointers, references, and nested class
template patterns during function partial ordering; apply the same ordering
to explicit and member-template candidates; avoid synthesizing a constructor
for an empty aggregate specialization; preserve pointer zero-initialization;
and propagate the selected free-function specialization's return type into
class-valued initialization.  The scope covers the repaired ordering,
constructor, aggregate, and dijkstra class-template-parameter fixtures plus
the restored const-pointer/reference cases.  Validation is the focused group,
the full PA22 report, through-PA21, and the PA22 file audit.

## Checkpoint 103 result — 2026-07-27

The scope is complete.  The current PA22 report is **224/250**, ten above the
214/250 turn-start baseline, with no timeout.  The increment fixes the fixed
versus trailing-pack and longer-prefix ordering cases, nested class-template
partial ordering, member constructor ordering, aggregate specialization
value-initialization, pointer aggregate zeroing, and the selected free
function's class-valued result fact.  It also retains the correct reference
conversion tie-break when two candidates have the same reference shape while
allowing a structurally more specialized class-template pattern to win.

Focused validation passes for the repaired fixed/trailing-pack, class-template
cv, synthetic virtual, constructor nested-id, template-template, const-pointer,
reference, and dijkstra fixtures.  The exact 26-fixture residual is the map
above.  The final through-PA21 report passes **1850/1850**; the PA19 stale
function-template LowIR regression exposed during validation was corrected by
retaining empty-list constructor emission for aggregate specializations with
inherited bases.  The PA22 file audit passes with the repository's 12 existing
warnings.  The next checkpoint is the candidate-status/SFINAE group, bundled
with the remaining typed LowIR replay cases where the status failures are
small.

## Checkpoint 104 scope — 2026-07-27 (before implementation)

### Remaining Work Map

The authoritative report is still **224/250**, with the complete 26-fixture
residual listed in Checkpoint 103.  The next shared behavior slice is the
member-candidate subset:

- imported base members must remain viable beside a derived member template,
  including implicit-object cv ranking;
- defaulted member-operator templates must prefer the fixed keyword/tag
  overload over the generic fallback; and
- constructor templates inherited through a class-template partial
  specialization must be collected, deduced, and materialized under the
  declaring owner.

These three status fixtures are:
`general/300-using-member-template-implicit-object-cv-overload`,
`spec/200-member-operator-fixed-tag-default-partial-order`, and
`general/400-partial-specialization-inherited-constructor-template`.
The other 23 failures remain grouped by the complete map in Checkpoint 103:
template-template/alias and pack SFINAE, hidden-friend lookup, conversion
owner replay, and typed LowIR materialization.

### Checkpoint Scope

Repair the shared member lookup and candidate-ranking path so it preserves
typed implicit-object cv facts, imported/inherited owner identity, defaulted
member-template arguments, and partial-specialization constructor ownership.
The scope is complete only when all three focused status fixtures pass without
weakening ordinary overload resolution.  Validate the focused group, the full
PA22 report, through-PA21, and the PA22 file audit.

## Checkpoint 104 result — 2026-07-27

The member increment is complete.  PA18 now identifies the concrete enclosing
class for implicit `this` during member-template deduction, carries its typed
substitutions into inherited callable members, and follows a partial base
specialization without replacing its primary template name prematurely.  PA11
accepts constructor-shaped template-id using-declarations, recovers the typed
generated owner when the source specialization has no populated scope, and
leaves namespace/operator using-declarations on the ordinary path.  PA14
materializes callable data-member operator targets, preserves empty-class
constructor base-entry behavior, and uses the argument temporary convention
for binary operator results passed by reference.  Constant-call type probing
was moved out of the analyzer header to keep the source audit within its
limits.

The three focused fixtures pass with authoritative LowIR comparison:
`general/300-using-member-template-implicit-object-cv-overload`,
`spec/200-member-operator-fixed-tag-default-partial-order`, and
`general/400-partial-specialization-inherited-constructor-template`.
The full PA22 report is **227/250**, up three from the 224/250 checkpoint
baseline and thirteen from the 214/250 turn-start baseline.  The through-PA21
report passes **1850/1850**, and the PA22 file audit passes with the normal 12
warnings.

### Remaining Work Map

The authoritative residual is 23 fixtures, with no timeout:

- **Candidate viability, deduction, and SFINAE (8):**
  `general/100-template-deduction-rejects-value-base-argument`,
  `general/300-alias-bool-explicit-pack-call-dependent-tag`,
  `general/300-hidden-friend-sfinae-use-scope-shadowing`,
  `general/400-member-alias-template-template-partial-deduction-owner`,
  `general/400-pack-expansion-size-mismatch-sfinae`,
  `spec/300-conversion-function-template-owner-result-copy-init`,
  `spec/500-function-result-template-id-shadowed-argument`, and
  `spec/500-hidden-friend-query-free-decltype-noexcept`.
- **Typed LowIR and alias/template replay (15):**
  `general/100-function-parameter-empty-middle-pack-alias`,
  `general/400-alias-template-function-argument-cv`,
  `general/400-function-assignment-invocable-and-helper`,
  `general/400-static-data-nttp-pack-sizeof-bound`,
  `general/400-template-template-alias-default-arity-sfinae`,
  `spec/100-explicit-specialization-dependent-param-typedef`,
  `spec/100-explicit-template-argument-overload-rejects-short-candidate`,
  `spec/100-function-template-array-parameter-string-literal`,
  `spec/200-defaulted-class-template-argument-prefix-deduction`,
  `spec/200-dependent-specialized-default-arg-deduction`,
  `spec/200-member-template-nontype-param-shadows-inherited-value-sum`,
  `spec/300-cross-specialization-converting-ctor-operator-template`,
  `spec/300-qualified-member-function-value-fallback-sfinae`,
  `spec/400-dependent-decltype-member-template-conversion-operator`, and
  `spec/500-type-pack-qualified-static-member-expansion`.

### Next Checkpoint Group

Take the eight-fixture candidate-viability/SFINAE group first, concentrating
on hidden-friend scope lookup, template-template owner matching, pack-size
discarding, and class-valued conversion candidate selection.  Then address
the 15 typed LowIR alias/template replay cases as a separate materialization
group.
