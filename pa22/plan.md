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
to **86/250**: eight baseline fixtures are fixed and no new PA22 failure names
were introduced.  The increment preserves typed function signatures and
deduction facts through function-reference/pointer matching, forwarding
reference lvalue adjustment, top-level cv normalization, array-bound
deduction/decay, and the affected decltype/SFINAE call path.  It also carries
forwarding-pack lvalue categories separately during replay, validates missing
`typename` without rejecting current-specialization constructor types, and
keeps explicit-specialization identity stable for LowIR.

Validation completed: the audited build succeeds; PA22 reports 86/250; the
through-PA21 suite has a recorded 1850/1850 pass (the later default-timeout
retries were blocked only by the known PA3 `300-triple` timeout, while a
15-second diagnostic timeout also passed 1850/1850); and the PA22 file audit
passes with the repository's existing warnings.

### Remaining Work Map after checkpoint

The remaining 164 PA22 failures are the original baseline set minus the eight
resolved fixtures above, with no newly exposed group:

- Function-call deduction still needs the remaining defaults, middle/trailing
  packs, conversion cases, and array/reference edge cases.
- Partial ordering and overload ranking still need pointer/cv/function,
  constructor, template-template, and trailing-pack ordering.
- Substitution failure and deferred/no-eager instantiation remain the largest
  group, including dependent decltype, alias probes, ADL, and short-circuit
  enable-if behavior.
- Alias/template-template propagation, owner-aware constructor/conversion
  replay, and typed non-type argument materialization remain incomplete.
- The remaining LowIR mismatches are downstream manifestations of those
  deduction, ownership, pack, and typed non-type gaps.

### Next Checkpoint Group

Substitution failure and deferred/no-eager instantiation, bundled with the
remaining dependent-decltype and SFINAE fixtures; then continue with
alias/template-template and owner-aware constructor/conversion propagation.
