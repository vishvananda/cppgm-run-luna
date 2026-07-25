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
