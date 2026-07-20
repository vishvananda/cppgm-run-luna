# PA18 checkpoint plan

## Baseline and checkpoint result

The turn-start PA18 baseline was 16/222.  The current full report is
169/222.  This checkpoint extends template specialization identity, generated
class/member ownership, ordinary-member lookup, operator-template naming,
namespace using/ADL lookup, hidden-friend demand emission, dependent signature
guards, and nested-enum qualification.

Validation for this checkpoint:

```text
full PA18 report: 169/222
focused operator/signature witnesses: 5/8 pass; 3 LowIR compare-only diffs
through-PA17: 1206/1208; only the two existing PA15 shift-stress timeouts
file audit: pass with 7 pre-existing header-division warnings
```

## Remaining Work Map

The exact current-PA failure set is 53 tests, grouped by shared behavior.

### A — generated records, signatures, layout, and LowIR value/lifetime shape

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
`spec/300-explicit-class-instantiation-nonstatic-member-function`,
`spec/300-explicit-class-instantiation-static-member-function`,
`spec/300-out-of-class-template-member-nested-enum-param`,
`spec/300-qualified-explicit-class-instantiation`.

### B — overload candidates, using, operators, and ADL

`general/100-function-template-pair-vs-range-predicate`,
`general/100-functional-cast-argument-nested-type-hides-function`,
`general/100-inherited-typedef-hidden-friend-overload`,
`general/100-local-call-prefers-member-over-template-type-declaration`,
`general/100-local-using-directive-qualified-template-argument`,
`general/100-member-cv-overload-deduction-argument`,
`general/100-qualified-using-directive-function-template-call`,
`general/100-using-base-cv-overload-deduces-mutable-ref`,
`general/100-using-namespace-ambiguous-less-than-or`,
`general/300-dependent-functional-template-id-hides-outer-function`,
`general/300-local-using-inline-namespace-function-template`,
`general/300-template-body-enum-adl-call`,
`spec/100-inherited-class-template-conversion-operator`,
`spec/100-local-constref-converting-iterator`,
`spec/300-unqualified-call-skips-dependent-base`.

### C — dependent lookup, qualified names, and instantiation timing

`general/100-qualified-unnamed-namespace-template-arg`,
`general/100-qualified-value-does-not-shadow-class-template`,
`general/300-dependent-decltype-comma-expression`,
`general/300-dependent-default-construction-through-template-subscript`,
`general/300-dependent-qualified-value-base-member`,
`general/300-dependent-typename-template-argument-local-init`,
`general/300-dependent-typename-template-argument-return`,
`spec/300-unnamed-namespace-qualified-class-template-id`.

### D — required diagnostics

`general/100-local-alias-shadows-template-parameter-bad`,
`general/100-nondependent-template-member-body-lookup-bad`, and
`general/300-out-of-class-special-member-noexcept-mismatch-bad` still return
success instead of the required failure status.

## Completed checkpoint scope

The selected scope was the shared operator/ADL and dependent-signature group.
Concrete function-template operator specializations retain their source
operator names, namespace using-declarations participate in associated lookup,
hidden friends are emitted after the demand fixed point, and class-member calls
do not get redirected to an out-of-class template merely because the source
signature is dependent.  Raw dependent signatures are guarded from premature
ordinary matching, and generated out-of-class member signatures can name a
specialized nested enum.  The focused witnesses and the full report above
validate the scope; remaining failures in this group are recorded as exact
LowIR-shape or candidate-lookup work.

## Next checkpoint scope

The next coherent group is function-template candidate viability and using/cv
deduction: pair-vs-range selection, functional-cast calls, member cv overloads,
base using-reference deduction, and inline-namespace using lookup.  Validate it
with those focused witnesses, the full PA18 report, the through-PA17 report,
and the file audit.
