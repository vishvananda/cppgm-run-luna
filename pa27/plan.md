# PA27 stage plan

## Baseline and failure map

Turn-start baseline: `make test-report ACTIVE_TEST_REPORT_PAS='pa27'` produced
`0 / 34` passing tests. Earlier assignments passed. The complete current-PA
failure set is grouped by the shared behavior exposed by the checked-in
fixtures (a test can appear in more than one cross-cutting group):

### A. Virtual-base layout, projection, and hidden argument propagation

These cases currently show ordinary non-virtual layout (zero/too-small
offsets and object sizes), omit the typed virtual-base forwarding arguments,
or fail to carry the adjusted virtual-base address through a reference/call
boundary:

`100-complete-constructor-reference-vbase-parameter`,
`100-constructor-prvalue-virtual-base-forwarding`,
`100-constructor-template-vbase-hidden-argument`,
`100-converting-constructor-vbase-hidden-argument`,
`100-declared-member-reference-virtual-base-argument`,
`100-forwarded-virtual-base-parameter-layout-union`,
`100-hidden-virtual-base-reference-argument-declaration`,
`100-member-pointer-reference-vbase-hidden-argument`,
`100-multiple-reference-virtual-base-parameters`,
`100-null-pointer-vbase-hidden-argument`,
`100-overload-set-reference-vbase-hidden-argument`,
`100-polymorphic-pointer-virtual-base-argument`,
`100-reference-member-hidden-virtual-base-layout`,
`100-reference-pointer-virtual-base-argument`,
`100-virtual-base-constructor-vptr-hidden-target`,
`100-virtual-base-manipulator-hidden-argument`,
`100-virtual-base-parameter-hidden-layout`,
`100-virtual-inheritance-fields`, and
`200-multi-level-virtual-base-diamond-lifecycle`.

### B. Multi-base ABI views, vtable groups, and RTTI facts

These cases expose missing secondary vtable views/slot merges, incorrect
primary/secondary base placement or adjustor metadata, incomplete RTTI graph
classification, or undefined emitted vtable symbols:

`100-diamond-virtual-destructor-slot-merge`,
`100-multibase-class-return-adjustor-thunk`,
`100-multibase-implicit-virtual-destructor-slot-merge`,
`100-nonprimary-direct-base-ctor-vtable-offset`,
`100-nonprimary-virtual-dispatch`,
`100-polymorphic-pointer-virtual-base-argument`,
`100-primary-polymorphic-base-before-nonpoly-static-cast`,
`100-secondary-primary-base-vptr-overwrite`,
`100-sibling-dynamic-cast`,
`100-typeid-nonprimary-view`,
`100-unrelated-virtual-roots-final-overrider`,
`100-using-base-field-primary-base-offset`, and
`200-multi-level-virtual-base-diamond-lifecycle`.

### C. Class-cast and final-overrider semantic acceptance

These cases currently fail before a matching LowIR comparison or lose the
required null/adjustment path:

`100-nonprimary-direct-base-ctor-vtable-offset`,
`100-nonprimary-virtual-dispatch`,
`100-null-static-pointer-downcast-nonprimary-base`,
`100-private-crtp-static-pointer-downcast`,
`100-sibling-dynamic-cast`,
`100-static-pointer-downcast-nonprimary-base`,
`100-static-reference-downcast-nonprimary-base`, and
`100-unrelated-virtual-roots-final-overrider`.

## Remaining Work Map

The turn-start failures were all instances of the three grouped behaviors
above.  After the implementation and final validation, no PA27-local failure
remains: Groups A, B, and C are covered by the completed compiler increment.

## Checkpoint Scope

The checkpoint covered the whole PA27 contract as one typed compiler
increment:

1. retain whether each direct base edge is virtual in semantic `Type` state;
2. compute deterministic complete-object layout for virtual-base closures,
   including one shared virtual-base subobject and stable direct-base paths;
3. make member/base address projection and object-size/class-value lowering
   consume those typed paths; and
4. carry the required virtual-base addresses through reference, constructor,
   conversion, and hidden-argument call lowering without reconstructing them
   from source text;
5. lower nested and multi-base construction/destruction with the required
   vtable, VTT, RTTI, final-overrider, and virtual-dispatch facts; and
6. emit the nested copy-constructor base entries and stable LowIR ABI order
   required by complete-object constructor calls.

Validation for the checkpoint is the complete PA27 local report, the
through-PA26 report, and the source file audit.  The final audit passed after
the ABI, address-projection, constructor-helper, parameter-name, and
polymorphic-global responsibilities were split into dedicated translation
units; only the repository's existing non-fatal style warnings remain.

## Checkpoint result

PA27 local report: `34 / 34` tests passed.  The implementation improved the
turn-start baseline from `0 / 34` to a complete pass, including the final
nested virtual-base constructor-reference case.

The final case required rebuilding hidden `this` views from the complete
constructor object, preserving reverse hidden-view stores for nested virtual
carriers, and demanding the nested carrier's copy base entry from typed
lowering state.  Earlier PA behavior remains covered by the through-PA26
report and the file audit.

## Remaining work after checkpoint

None for PA27.  The next stage checkpoint, if requested, is PA28.
