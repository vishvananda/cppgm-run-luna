# PA18 checkpoint plan

## Baseline and checkpoint result

The turn-start PA18 baseline was **169/222**.  This checkpoint raises the
current report to **174/222** and keeps all earlier assignments green.

The implementation scope was typed function-template candidate viability and
lookup: reject candidates whose function-argument arity is impossible; do not
mistake a template signature for an ordinary member match; infer constructor
arguments through enclosing class scopes; use inherited member/cv facts while
deducing references; and make function-scope `using` declarations contribute a
visible template target without leaving an unsupported `using-declaration` in
the PA14 AST.  The same lookup path now handles dependent functional
template-ids and inline-namespace function templates.  A shared PA14 fix adds
an owning inference cache entry so nested overloaded-operator expressions are
memoized without synthetic-node pointer aliasing.

Validation for the checkpoint:

```text
focused PA18 candidate/using witnesses: 5/6 exact; the member-cv witness has
  the expected successful semantic path but remains a LowIR-only constructor
  layout diff
focused PA15 shift stress pair: 2/2 pass
full PA18 report: 174/222
through-PA17: 1208/1208 pass
file audit: pass with 7 pre-existing header-division warnings
```

## Remaining Work Map

The complete current-PA failure set is **48 tests**, grouped by shared
compiler behavior.

### A — generated records, signatures, initialization, layout, and LowIR shape

`general/100-class-template-alias-array-member`,
`general/100-class-template-member-plus-calls-later-plus-assign`,
`general/100-defaulted-copy-constructor-reference-member`,
`general/100-defaulted-move-constructor-reference-member`,
`general/100-function-template-parameter-decltype-ref-array`,
`general/100-member-cv-overload-deduction-argument`,
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

### B — remaining overload sets, using, ADL, and required diagnostics

`general/100-inherited-typedef-hidden-friend-overload`,
`general/100-local-call-prefers-member-over-template-type-declaration`,
`general/100-local-using-directive-qualified-template-argument`,
`general/100-qualified-using-directive-function-template-call`,
`general/100-using-namespace-ambiguous-less-than-or`,
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

### D — required negative diagnostics

`general/100-local-alias-shadows-template-parameter-bad`,
`general/100-nondependent-template-member-body-lookup-bad`, and
`general/300-out-of-class-special-member-noexcept-mismatch-bad` still return
success instead of the required failure status.

## Completed checkpoint scope

The selected candidate/using group is complete at the semantic/status level:
the pair-vs-range, nested-type functional-cast, base-cv reference, inline
namespace using, and dependent functional-template-id witnesses pass; the
member-cv witness reaches the correct instantiated `join_alloc` call and only
differs in an existing constructor-layout presentation.  Arity filtering and
direct member-signature lookup preserve ordinary overloads while allowing
template deduction, and the PA15 operator-chain regression is fixed by typed
owning memoization rather than a test-specific timeout change.

## Next checkpoint scope

Take group A's generated declaration and LowIR shape work: preserve typed
specialization identity through class/member initialization, nested/out-of-
class signatures, reference/array layout, and explicit instantiation.  Start
with the member-cv constructor-layout witness and the defaulted
copy/move/reference-member pair, then bundle adjacent signature/layout cases
that share the same generated-record behavior.  Validate with the focused
group, the full PA18 report, through-PA17, and the file audit.
