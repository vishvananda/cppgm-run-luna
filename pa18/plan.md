# PA18 checkpoint plan

## Continuation turn baseline and selected scope

The continuation turn starts from the committed **179/222** PA18 result with
**43** current-PA failures and 1208/1208 tests through PA17.  This turn
selects a Group A declaration/layout slice covering typed array members and
qualified array template arguments, dependent `decltype` reference/array
signatures, and inherited member projection during template construction.
The focused scope is:

`general/100-class-template-alias-array-member`,
`general/100-function-template-parameter-decltype-ref-array`,
`general/100-member-cv-overload-deduction-argument`, and
`general/300-qualified-array-type-template-argument`.

The intended behavior is complete typed layout and initialization for alias
members, stable declaration records for dependent reference/array results,
base-aware member addresses in instantiated bodies, and a defined call target
for a qualified array type template argument.  Validation will run this focus,
the full PA18 report, through PA17, and the file audit.

## Checkpoint 2 baseline and scope

The full report after that focus is **183/222**, leaving **39** failures.  The
remaining set is grouped as follows:

- **A — generated declarations, ownership, layout, and LowIR shape (19):**
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
  `general/300-reentrant-reference-collection-override-param`,
  `general/300-reference-member-same-template-name`,
  `general/300-reused-template-body-qualified-member-type-arg`,
  `spec/100-default-argument-instantiation-independence`,
  `spec/300-explicit-class-instantiation-nonstatic-member-function`,
  `spec/300-explicit-class-instantiation-static-member-function`,
  `spec/300-out-of-class-template-member-nested-enum-param`, and
  `spec/300-qualified-explicit-class-instantiation`.
- **B — overload sets, using, ADL, and dependent-base calls (9):**
  `general/100-inherited-typedef-hidden-friend-overload`,
  `general/100-local-call-prefers-member-over-template-type-declaration`,
  `general/100-local-using-directive-qualified-template-argument`,
  `general/100-qualified-using-directive-function-template-call`,
  `general/100-using-namespace-ambiguous-less-than-or`,
  `general/300-template-body-enum-adl-call`,
  `spec/100-inherited-class-template-conversion-operator`,
  `spec/100-local-constref-converting-iterator`, and
  `spec/300-unqualified-call-skips-dependent-base`.
- **C — dependent lookup, qualified names, and instantiation timing (8):**
  `general/100-qualified-unnamed-namespace-template-arg`,
  `general/100-qualified-value-does-not-shadow-class-template`,
  `general/300-dependent-decltype-comma-expression`,
  `general/300-dependent-default-construction-through-template-subscript`,
  `general/300-dependent-qualified-value-base-member`,
  `general/300-dependent-typename-template-argument-local-init`,
  `general/300-dependent-typename-template-argument-return`, and
  `spec/300-unnamed-namespace-qualified-class-template-id`.
- **D — required negative diagnostics (3):**
  `general/100-local-alias-shadows-template-parameter-bad`,
  `general/100-nondependent-template-member-body-lookup-bad`, and
  `general/300-out-of-class-special-member-noexcept-mismatch-bad`.

This checkpoint covers the six declaration/ownership witnesses consisting of
the three explicit class instantiations, the nested-enum out-of-class member,
the class-template alias constructor, and the nested-template-owner
constructor.  The behavior is explicit specialization materialization with
object-root records, stable generated parameter identities, and insertion of
out-of-class nested member definitions.  Validation is the focused six-test
set, then the full PA18 report, through PA17, and the file audit.

## Checkpoint 2 result and Checkpoint 3 scope

The six-test declaration/ownership focus passes **6/6**.  The implementation
also fixes the adjacent out-of-class member-alias return signature, so the
full current-PA result is **190/222** with **32** failures.  The emitted
records now materialize explicit class instantiations as weak object roots,
retain declaration parameter names for unnamed out-of-class definitions,
insert nested special-member definitions, and distinguish user-provided C1
constructors from defaulted base-entry chains.

The refreshed **Remaining Work Map** is:

- **A — call/value lowering and generated LowIR shape (11):**
  `general/100-inherited-typedef-hidden-friend-overload`,
  `general/100-qualified-function-template-member-overload-argument`,
  `general/100-static-member-function-object-access-pointer`,
  `general/100-template-function-pointer-rvalue-argument-emits-move`,
  `general/300-member-rvalue-subscript-overload-binding`,
  `general/300-reentrant-reference-collection-override-param`,
  `general/300-reference-member-same-template-name`,
  `general/300-reused-template-body-qualified-member-type-arg`,
  `spec/100-inherited-class-template-conversion-operator`,
  `spec/100-local-constref-converting-iterator`, and
  `general/300-out-of-class-nested-member-class-definition`.
- **B — lookup, using, ADL, and dependent-base resolution (8):**
  `general/100-local-call-prefers-member-over-template-type-declaration`,
  `general/100-local-using-directive-qualified-template-argument`,
  `general/100-qualified-using-directive-function-template-call`,
  `general/100-using-namespace-ambiguous-less-than-or`,
  `general/300-template-body-enum-adl-call`,
  `spec/300-unqualified-call-skips-dependent-base`,
  `general/100-qualified-unnamed-namespace-template-arg`, and
  `spec/300-unnamed-namespace-qualified-class-template-id`.
- **C — dependent types, constants, and instantiation timing (10):**
  `general/100-namespace-template-function-before-tls-object`,
  `general/100-nested-class-template-local-class-argument`,
  `general/200-template-alignas-gnu-alignof-instantiation`,
  `general/100-qualified-value-does-not-shadow-class-template`,
  `general/300-dependent-decltype-comma-expression`,
  `general/300-dependent-default-construction-through-template-subscript`,
  `general/300-dependent-qualified-value-base-member`,
  `general/300-dependent-typename-template-argument-local-init`, and
  `general/300-dependent-typename-template-argument-return`, and
  `spec/100-default-argument-instantiation-independence`.
- **D — required negative diagnostics (3):**
  `general/100-local-alias-shadows-template-parameter-bad`,
  `general/100-nondependent-template-member-body-lookup-bad`, and
  `general/300-out-of-class-special-member-noexcept-mismatch-bad`.

For Checkpoint 3, take the six-test call/value slice:
`general/100-qualified-function-template-member-overload-argument`,
`general/100-static-member-function-object-access-pointer`,
`general/100-template-function-pointer-rvalue-argument-emits-move`,
`general/300-member-rvalue-subscript-overload-binding`,
`spec/100-local-constref-converting-iterator`, and
`spec/100-inherited-class-template-conversion-operator`.  It covers typed
function-object decay, by-address versus object argument lowering, temporary
materialization, rvalue member lookup, and conversion-generated initialization.

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

## Checkpoint 3 result

The declaration/layout checkpoint was superseded by a smaller call/value
increment while preserving the same clean baseline.  The six call/value
witnesses passed and the full PA18 report reached 196/222.

## Checkpoint 4 result and Remaining Work Map

The value/lifetime checkpoint passed all seven focused witnesses and raised
the full PA18 report to **202/222**.  The remaining 20 failures are grouped
below by shared behavior:

* Lookup and overload selection: inherited typedef/hidden-friend argument
  lowering; a direct member function hidden by a same-named template type;
  qualified using-directive function-template selection; ambiguous
  less-than lookup through a using-directive; and unqualified calls that must
  skip dependent-base members.
* Template instantiation and records: namespace-template/TLS initialization;
  nested local-class owner arguments; dependent alignas/alignof; out-of-class
  nested member-class definitions; reentrant reference collection; unused
  enum ADL helper emission; and default-argument instantiation independence.
* Dependent names and static storage: dependent qualified value-base members
  (local-init and return variants), unnamed-namespace qualified template
  static storage, and conversion-operator result materialization.
* Required diagnostics: local alias shadowing a template parameter,
  nondependent template member-body lookup, and out-of-class special-member
  noexcept mismatch.

## Next checkpoint scope

Complete the two-phase/member lookup slice first: direct member functions must
win over same-named type declarations, while unqualified calls in a template
must not bind a dependent base member.  Validate both focused witnesses, then
run the full PA18 report and through-PA17 before selecting the next group.

## Checkpoint 5 result and Remaining Work Map

The two-phase/member lookup checkpoint passed both focused witnesses and
raised the full PA18 report to **204/222**.  The remaining 18 failures are:

* Demand-driven emission and static storage: the namespace-template/TLS
  initialization case; three dependent qualified value-base cases with extra
  declarations; an uninstantiated enum ADL helper; default-argument
  instantiation independence; unnamed-namespace qualified template static
  storage; and conversion-operator result address materialization.
* Template records and LowIR shape: inherited typedef/hidden-friend argument
  lowering; nested local-class owner arguments; qualified using-directive
  function-template emission/order; using-directive less-than address reuse;
  dependent alignas/alignof; out-of-class nested member-class conversion;
  reentrant reference collection; and the nested/record cases in the earlier
  Group A map that remain outside this current report's first-failure list.
* Required diagnostics: local alias shadowing a template parameter,
  nondependent template member-body lookup, and out-of-class special-member
  noexcept mismatch.

## Next checkpoint scope

Complete the demand-driven emission/static-storage slice: suppress
uninstantiated template helpers and default-argument-only specializations,
avoid declarations for unevaluated dependent value references, and preserve
the qualified identity of unnamed-namespace template static storage.  Validate
the focused emission/storage witnesses, then run the full PA18 report and
through-PA17 before selecting the next record/layout group.

## Checkpoint 6 result and Remaining Work Map

The demand/emission checkpoint is complete.  Typed thread-local guard records,
per-object guarded initialization, and late ordinary-template demand passes
raise the full PA18 report to **212/222**; the focused TLS, static-storage,
dependent-value, and default-argument witnesses all pass.

The remaining ten failures are grouped by shared behavior:

* Required template diagnostics: a local alias illegally shadows a template
  parameter, a nondependent template member body must reject an unknown name,
  and an out-of-class special-member definition must reject a noexcept
  mismatch (`general/100-local-alias-shadows-template-parameter-bad`,
  `general/100-nondependent-template-member-body-lookup-bad`, and
  `general/300-out-of-class-special-member-noexcept-mismatch-bad`).
* Class-valued template records and lifetime shape: inherited typedef/hidden
  friend overload argument materialization, nested local-class template
  arguments, and out-of-class nested member-class conversion.
* Dependent layout: `alignas(__alignof(T))` must affect the instantiated
  class size/alignment and `alignof` result.
* LowIR expression shape: using-namespace aggregate initialization emits a
  redundant address, conversion-operator result inspection emits a redundant
  address, and reentrant reference collection leaves two unused field loads.

## Next checkpoint scope

Implement the three required template diagnostics as one semantic-validation
slice.  Preserve valid template declarations while rejecting only illegal
local parameter shadowing, nondependent missing-name lookup, and mismatched
out-of-class special-member exception specifications.  Validate all three
negative witnesses, then run the full PA18 report and through-PA17 before
selecting the next LowIR-shape group.

## Checkpoint 7 result and Remaining Work Map

The semantic-validation checkpoint passes all three required negative
witnesses and preserves the condition-scope and builtin-constant template
body witnesses.  The full report is now **215/222**.  The seven remaining
failures are:

* Class-valued template records and lifetime shape: inherited typedef/hidden
  friend overload argument materialization, nested local-class template
  arguments, and out-of-class nested member-class conversion.
* Dependent layout: `alignas(__alignof(T))` does not yet propagate the
  instantiated alignment into class size and `alignof`.
* LowIR expression shape: aggregate initialization through a namespace-scope
  using-directive emits a redundant second address, conversion-operator result
  inspection emits a redundant address, and reentrant reference collection
  leaves two unused field loads.

## Next checkpoint scope

Complete the three small LowIR expression-shape cases as one lowering cleanup:
reuse the existing aggregate base address, avoid re-addressing a conversion
operator's already-known object, and suppress unused reentrant reference
loads.  Validate those three focused witnesses, then run the full PA18 report
and through-PA17 before selecting the remaining class-record/layout group.

## Checkpoint 8 result and Remaining Work Map

The final class-record/layout and LowIR-shape increment is complete.  It
materializes class prvalues bound to references in typed temporary objects,
projects derived bases from those objects, preserves the explicit integral
conversion boundary required by builtin calls, and restarts demand-driven
member emission so dependent helper records appear before their users.  It
also applies contextual class conversion in conditional expressions, reads
unqualified reference members through their stored pointer, emits fresh
global aggregate member projections, and avoids direct-object readdressing
for `sizeof`.  GNU `__alignof` type operands now participate in dependent
alignment and template argument normalization, while no-op default
construction is skipped without removing explicit constructors.

The enum/builtin bitwise witness was rerun after narrowing the same-width
copy rule to fundamental-to-fundamental conversions.  The complete PA18
report now passes **222/222**, with the earlier PA report unchanged.

The Remaining Work Map is empty: no current-PA tests remain failing.

## Next checkpoint scope

No further implementation checkpoint is required for PA18.  Run the exact
current-PA report, through-PA17 report, source file audit, and clean-worktree
check, then commit this cohesive PA18 implementation.

## Checkpoint 9 result and final Remaining Work Map

The final compatibility/lifetime checkpoint is complete.  It covers two
shared behaviors exposed by the refreshed failure set: cleanup of implicit
class objects now materializes the declaration address before the fresh
destructor address, including the template-specialization cases that need a
typed lifetime anchor; and defaulted copy/move construction uses a base-entry
for direct base subobjects while retaining the ordinary member special-member
entry for non-base members.  The changes preserve the PA18 demand-driven
template records and the earlier aggregate, TLS, and dependent-layout paths.

Validation is **222/222** for PA18 and **1208/1208** through PA17.  The source
audit and `git diff --check` are also clean.

The Remaining Work Map is empty for PA18.  No further checkpoint group is
required; the next action is the final required audit, commit, and clean
worktree check.
