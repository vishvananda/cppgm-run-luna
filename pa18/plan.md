# PA18 checkpoint plan

## Baseline and checkpoint result

The turn-start PA18 baseline was 16/222.  After the specialization and
static-member work, the full report reached 164/222.  The completed checkpoint
preserves template identity through the AST and typed semantic model, carries
generated class/member ownership into PA14 records, materializes nested and
out-of-class members, reuses in-class static initializers, folds const static
members only, and emits weak/template object identities.  Function
specializations now also receive distinct typed generated names, numeric
literal deduction preserves `long`/`unsigned`/floating suffixes, and ordinary
non-template exact matches win over implicit template candidates.

Focused validation completed during this turn:

```text
static-member checkpoint: 7/7
enum builtin fallback: 1/1
function specialization/overload checkpoint: 3/3
file audit: pass (7 pre-existing warnings)
PA16 operator-new/delete regression witness: 1/1
through-PA17: 1206/1208; only the two existing PA15 shift-stress timeouts remain
```

## Remaining Work Map

The latest full current-PA report has 58 failures.  The complete set is grouped
by shared behavior below; each fixture appears once.

### A — generated member records, signatures, and value/lifetime lowering

`general/100-class-template-alias-array-member`,
`general/100-class-template-member-plus-calls-later-plus-assign`,
`general/100-defaulted-copy-constructor-reference-member`,
`general/100-defaulted-move-constructor-reference-member`,
`general/100-function-template-parameter-decltype-ref-array`,
`general/100-namespace-template-function-before-tls-object`,
`general/100-nested-class-template-local-class-argument`,
`general/100-qualified-function-template-member-overload-argument`,
`general/100-static-member-function-object-access-pointer`,
`general/100-template-function-pointer-rvalue-argument-emits-move`,
`general/200-defaulted-template-arg-base-initializer-match`,
`general/200-defaulted-template-arg-base-reference-chain`,
`general/200-template-alignas-gnu-alignof-instantiation`,
`general/300-class-template-alias-out-of-class-ctor`,
`general/300-member-rvalue-subscript-overload-binding`,
`general/300-out-of-class-member-alias-return-signature`,
`general/300-out-of-class-nested-member-class-definition`,
`general/300-out-of-class-nested-template-owner-constructor`,
`general/300-qualified-array-type-template-argument`,
`general/300-reentrant-reference-collection-override-param`,
`general/300-reference-member-same-template-name`,
`general/300-reused-template-body-qualified-member-type-arg`,
`spec/100-default-argument-instantiation-independence`,
`spec/100-function-template-member-array-reference-return`,
`spec/300-explicit-class-instantiation-nonstatic-member-function`,
`spec/300-explicit-class-instantiation-static-member-function`,
`spec/300-out-of-class-template-member-nested-enum-param`, and
`spec/300-qualified-explicit-class-instantiation`.

### B — overload resolution, operators, using, and ADL

`general/100-inherited-typedef-hidden-friend-overload`,
`general/100-local-call-prefers-member-over-template-type-declaration`,
`general/100-local-type-cross-namespace-operator-template`,
`general/100-local-using-directive-qualified-template-argument`,
`general/100-member-cv-overload-deduction-argument`,
`general/100-qualified-using-directive-function-template-call`,
`general/100-template-operator-shift-stress-chain`,
`general/100-template-operator-shift-two-step`,
`general/100-using-namespace-ambiguous-less-than-or`,
`general/300-dependent-base-using-overload`,
`general/300-lazy-header-parenthesized-qualified-function-template-call`,
`general/300-local-using-inline-namespace-function-template`,
`general/300-template-body-enum-adl-call`,
`spec/100-inherited-class-template-conversion-operator`,
`spec/100-local-constref-converting-iterator`,
`spec/100-local-using-template-specialization-does-not-suppress-adl`,
`spec/100-template-logical-operator-call-argument`,
`spec/100-using-declaration-operator-template-adl`, and
`spec/300-unqualified-call-skips-dependent-base`.

### C — dependent lookup, qualified names, and instantiation timing

`general/100-qualified-unnamed-namespace-template-arg`,
`general/100-qualified-value-does-not-shadow-class-template`,
`general/300-dependent-decltype-comma-expression`,
`general/300-dependent-default-construction-through-template-subscript`,
`general/300-dependent-qualified-value-base-member`,
`general/300-dependent-typename-template-argument-local-init`,
`general/300-dependent-typename-template-argument-return`, and
`spec/300-unnamed-namespace-qualified-class-template-id`.

### D — required diagnostics

These three expected-negative fixtures still return success:

`general/100-local-alias-shadows-template-parameter-bad`,
`general/100-nondependent-template-member-body-lookup-bad`, and
`general/300-out-of-class-special-member-noexcept-mismatch-bad`.

The hidden-friend item in group B is also a LowIR sanity failure: `ns__Vec__size`
still calls an unresolved `ns__operator` target.

## Completed checkpoint scope

The static-member scope is complete and validated by the seven focused tests
listed above.  The function-identity scope is also complete for the focused
three-test set: distinct concrete argument lists now map to distinct generated
bindings, exact ordinary overloads are not rewritten to templates, and
integer literal suffixes participate in deduction.

The symbol/ABI helpers were moved into `pa14_lowering_symbols.cpp` and the
rewriter support was split across its existing helper headers so the PA18 file
audit remains clean without changing generated behavior.  The broad PA18
checkpoint remains 164/222; the 58 remaining fixtures are still grouped above
and no generated comparison artifacts are part of the implementation.

## Next checkpoint scope

The next coherent group is the remaining function/operator/ADL surface in
group B, starting with dependent member-vs-template lookup and using/hidden-
friend operator calls.  Validation should include the group-B witnesses, the
full PA18 report, the through-PA17 report, and the file audit.
