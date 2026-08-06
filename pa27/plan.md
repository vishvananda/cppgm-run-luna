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
lowering state.  The final regression repair also made function-record lookup
respect the binding's static/member status, and limited reference-parameter
address fallback to class-pointer conversions; this preserves PA15 overload
selection while preventing an extra load for ordinary `void*` address-taking.

The exact prior-through report passed with `2735 / 2735` tests.  The required
PA27 report passed with `34 / 34` tests.  The required source file audit passed
with the repository's existing 14 non-fatal warnings.

## Remaining work after checkpoint

None for PA27 or the through-PA26 regression set.  The next checkpoint group
is the PA28 full-stage implementation and its native-validation contract.

## Architecture Review

The completed implementation follows the intended staged compiler boundary.
PA11 owns the semantic facts: `Type` records the ordered direct-base graph,
per-edge virtual/access properties, the primary-base choice, non-virtual and
complete object sizes, virtual-base closure/offsets, and the polymorphic
`VirtualTableView` set.  `pa11_semantics_layout.cpp` computes those facts from
typed base and member declarations.  Virtual-base closure is a typed,
visited graph walk, so a diamond contributes one shared virtual subobject and
does not depend on a source-level base spelling or a first-base shortcut.

PA14 consumes those facts for LowIR.  `pa14_lowering_addresses.cpp` projects
through typed direct-base paths and virtual-base offsets; `pa14_lowering_abi.cpp`
and `pa14_lowering_parameter_names.cpp` keep hidden virtual-base carriers
aligned with the function ABI; and the constructor, destructor, object,
function, call, and constructor-entry modules use the same typed path and
ownership rules.  Complete constructors own each virtual base, while base
entries forward the existing hidden view and skip duplicate construction.
VTT and secondary-view handling is kept with construction and polymorphic
lowering rather than encoded in individual tests.

PA17 and PA25 complete the ABI-facing side.  The polymorphic lowering renders
primary and secondary views, adapts final overriders and adjustor thunks, and
emits vtables, VTTs, RTTI, and view globals in deterministic typed order.
`pa25_lowering_rtti.cpp` delegates the general runtime cast cases while the
lowering supplies the semantic RTTI objects and the complete-object fast path
needed for the PA27 sibling-cast and non-primary-view cases.  This preserves
the earlier PA15--PA26 overload, call, lifetime, and RTTI contracts.

The ownership review found no unstable raw-pointer container ownership in the
new path: function and global records use stable deques, and semantic derived
facts are rebuilt from canonical type data.  As a small rematerialization
cleanup, `ProcessClass` now clears `virtual_base_roots` and
`virtual_table_views` together with the other derived layout/polymorphism
state before rebuilding a class.  The graph walks use visited sets and the
polymorphic model is demand/fixpoint driven, avoiding repeated unbounded
replay.  The file audit's 14 warnings are existing non-fatal style/complexity
warnings in the staged codebase; no new source file or test/reference
workaround was introduced.

## Final Architecture Review

PA27 is consolidated as a monotonic extension of the PA26 architecture.  The
semantic layer is the single owner of shared virtual-base identity, layout,
access, and final-overrider facts; LowIR lowering reads those facts through
typed helpers and explicit hidden ABI state; and the PA17/PA25 emitters are
the only owners of vtable/RTTI materialization.  Construction, destruction,
member access, virtual dispatch, `dynamic_cast`, and `typeid` therefore agree
on the same complete-object and subobject model.

The checkpoint repairs are architectural rather than fixture-specific: they
cover hidden carrier projection, nested constructor/base-entry demand,
function-record selection, EH cleanup paths, and address-taking without
changing ordinary pointer shapes.  The implementation does not invoke
reference binaries, a host compiler, or an interpreter to manufacture output,
and the unsupported PA27 runtime/error boundaries remain explicit in the
handout contract.  Earlier assignments remain covered by the through-stage
report, so the handoff to PA28 is the existing typed model plus its tested
LowerIR/ABI boundary, not a parallel PA27 implementation.
