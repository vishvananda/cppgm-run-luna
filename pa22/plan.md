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
