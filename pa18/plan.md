# PA18 checkpoint plan

## Current turn baseline and selected scope

The refreshed current-PA baseline for this turn is **174/222**, with the
complete failure set at **48 tests**.  It matches the failure inventory below
and preserves the prior checkpoint's 174/222 result as the starting point for
this increment.

This turn selects the following substantial Group A slice: carry typed
reference-member facts through instantiated class layouts and defaulted
copy/move constructor lowering, retain the generated special-member records
needed by those calls, and preserve the same instantiated-member identity when
an enclosing template class invokes an out-of-class or defaulted member.  The
scope covers the defaulted copy/move reference-member pair, the template class
member-plus-copy call path, and the adjacent defaulted base-constructor chain
cases that share this generated-record and member-layout behavior.  Validation
will use those focused cases, the complete PA18 report, through-PA17, and the
file audit.

## Baseline and checkpoint result

The turn-start PA18 baseline was **174/222**.  This checkpoint raises the
current report to **179/222** and preserves the earlier PA results.

The selected increment now carries typed reference-member facts through
instantiated layouts and defaulted copy/move bodies, distinguishes reference
storage projection from reference-value reads, and handles derived-to-empty
base transfers without inventing source projections.  It also retains the
special-member records needed by instantiated calls, gives inherited
constructors their template identity, emits overload-stable base entries and
constructor aliases, and encodes member-template ABI substitutions.  The
shared lowering fixes cover pointer subtraction over dependent arrays,
class-valued operator temporaries, and operator `+=`/`-=` names.

Validation for the checkpoint:

```text
selected PA18 reference-member/base-template witnesses: 5/5 pass
regression sweep for the ten previously passing affected tests: 10/10 pass
full PA18 report: 179/222
through-PA17: 1208/1208 pass
file audit: pass with 7 pre-existing header-division warnings
```

The checkpoint audit found no shortcut, timeout workaround, fixture-dependent
acceptance path, embedded payload, or unchecked source fragment.  The focused
changes are semantic lowering and typed compiler-state fixes; the remaining
failures below are the pre-existing PA18 inventory after this increment.

## Remaining Work Map

The complete current-PA failure set, refreshed after the checkpoint, is
**43 tests**, grouped by shared compiler behavior.  The five selected
reference/base/layout witnesses are no longer failing, and the ten
checkpoint-induced regressions were restored.

### A — generated records, signatures, initialization, layout, and LowIR shape

`general/100-class-template-alias-array-member`,
`general/100-function-template-parameter-decltype-ref-array`,
`general/100-member-cv-overload-deduction-argument`,
`general/100-namespace-template-function-before-tls-object`,
`general/100-nested-class-template-local-class-argument`,
`general/100-qualified-function-template-member-overload-argument`,
`general/100-static-member-function-object-access-pointer`,
`general/100-template-function-pointer-rvalue-argument-emits-move`,
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

The selected reference/base/layout group is complete: both defaulted
reference-member copy/move witnesses, the class-template member-plus-copy
call, and both defaulted-template base-initializer/chain witnesses pass.  The
same typed path also restored the ten affected operator, inherited-constructor,
move-only, static-member, dependent-cv, and ADL/member-call regressions.

## Next checkpoint scope

Take the remaining Group A declaration/layout slice: preserve typed
specialization identity through reference/array parameter layout, class
aliases, nested and out-of-class member signatures, and explicit class
instantiation.  Start with the function-template ref/array and member-cv
witnesses, then bundle the adjacent generated-record cases that share their
signature and LowIR-shape behavior.  Validate with that focused group, the
full PA18 report, through-PA17, and the file audit.
