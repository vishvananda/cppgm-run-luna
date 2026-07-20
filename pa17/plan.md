# PA17 checkpoint plan

## Baseline

The turn-start `make test-report ACTIVE_TEST_REPORT_PAS='pa17'` report is
2/25 tests passing.  The complete 23-test failure set is grouped below by the
typed compiler behavior it shares.

## Remaining Work Map

### Polymorphic class metadata, layout, vtables, and virtual dispatch

The compiler currently treats every class as a PA16 non-polymorphic value.  It
therefore misses the vpointer-bearing object layout, inherited virtual slot
maps, vtable globals/RTTI data, constructor and destructor vpointer writes,
and indirect calls through object, pointer, and reference expressions.  This
accounts for the LowIR mismatches in:

- `general/300-derived-virtual-root.t`
- `general/300-self-subobject-base-path.t`
- `general/300-temporary-derived-to-base-reference-virtual-call.t`
- `general/300-virtual-call-dereferenced-member-pointer.t`
- `general/400-header-out-of-class-virtual-vtable.t`
- `general/400-inline-polymorphic-constructor-vtable.t`
- `general/400-key-function-vtable-without-local-construction.t`
- `general/400-std-rtti-name-substitution.t`
- `general/400-virtual-declaration-order-vtable.t`
- `general/400-virtual-overload-distinct-vtable-slots.t`
- `spec/200-pure-virtual-override-member.t`
- `spec/300-inherited-virtual.t`
- `spec/400-covariant-return-override.t`

The same missing typed polymorphism causes the expected-success failures in
`spec/300-final-virtual.t`, `spec/300-virtual-base-pointer.t`,
`spec/300-virtual-base-reference.t`, `spec/300-virtual-destructor-override.t`,
and `spec/400-virtual-override-dispatch.t`.

### Virtual declaration validation

The parser preserves `override` and `final`, but semantic collection does not
compare the declaration against inherited virtual facts or reject an override
of a final slot.  These expected-failure tests currently succeed:

- `spec/200-override-signature-mismatch-bad.t`
- `spec/300-bad-final-override.t`
- `spec/300-bad-override.t`

### Destructor calls and qualified non-virtual calls

The existing PA16 destructor/lifetime path has no virtual destructor slot
model and does not yet lower explicit destructor calls in a polymorphic class.
The explicit base-qualified call must also retain a direct, non-virtual target.
These expected-success cases currently fail before producing the required
LowIR:

- `general/400-explicit-virtual-destructor-call-nonvirtual.t`
- `spec/400-base-qualified-virtual-call-nonvirtual.t`

## Checkpoint Scope

Implement the first complete PA17 polymorphism slice in the existing typed
PA11/PA14 model:

1. Record virtual, pure, override, final, destructor, and inherited-slot facts
   on class/member metadata; validate override and final constraints during
   semantic collection.
2. Make supported polymorphic single-inheritance classes carry one vpointer at
   offset zero, retain PA16 base/member offsets, and derive deterministic
   virtual slot order including the complete/deleting destructor pair.
3. Emit the required vtable/RTTI globals and constructor/destructor vpointer
   stores, including base-entry construction and destruction transitions.
4. Lower ordinary member calls through polymorphic objects, pointers,
   references, and dereferenced pointer members as indirect LowIR calls, while
   keeping explicit base qualification direct.
5. Preserve the PA16 direct-call and non-polymorphic object output for inputs
   that do not use virtual members.

This scope covers the shared object-model behavior behind all 23 current
failures; validation will use the complete PA17 suite, with focused checks on
root/derived layout, inherited and pure slots, pointer/reference dispatch,
vtable ordering, destructor transitions, and invalid `override`/`final`
declarations.  The next checkpoint group, if needed, is the remaining ABI
polish for unusual out-of-class/header definitions and explicit destructor
call forms after the core polymorphic path is green.

## Checkpoint result

Completed the full PA17 checkpoint scope.  The implementation now carries
typed polymorphic metadata through semantic analysis and LowIR lowering,
including inherited virtual slots, pure/final/override validation, vpointer
layout, vtable/RTTI globals, constructor/destructor entries, virtual member
dispatch, pointer/reference adjustment, dereferenced member-pointer calls,
and direct qualified calls.

Validation result: `make test-report ACTIVE_TEST_REPORT_PAS='pa17'` passes all
25/25 tests.  The PA16-through regression passes all 1183/1183 tests, and the
PA17 file audit passes with only the repository's existing header-division
warnings.  The PA17 remaining-work map is empty; no current-PA checkpoint
remains.  The next checkpoint is PA18.
