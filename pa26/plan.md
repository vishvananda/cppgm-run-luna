# PA26 full-stage plan

## Baseline and latest validation

The turn-start PA26 baseline was 41/66.  The replay checkpoint reached 65/66,
and the completed full-stage implementation now passes **66/66** PA26 tests.
The preserved through-PA25 report is **2669/2669**, and the final required
through-PA26 report passes **2735/2735**.

The review covers the checkpoint commits
`21f80f9`, `e4c1f96`, `0a80a4f`, and `814aafc`, their audit
`d013cf2`, and the integrated PA26 history back to the PA25 audit
`d98f3ca`.  The assignment contract is `pa26/README.md`, especially its
non-virtual multiple-inheritance boundary and its explicit PA27 handoff.

## Review scope

- Verify the PA26 semantic object model against all assignment-boundary
  features: layout, lookup/access, method adjustment, generated special
  members, member pointers, current single-vptr RTTI, and ambiguity rejection.
- Trace the checkpoint's template replay, typed member-pointer, static-data
  replay, and comparator changes through the existing PA11-PA25 pipeline.
- Check ownership, lifetime, deterministic ordering, repeated traversal, and
  failure behavior in the changed source files.
- Verify that every new implementation source is owned by
  `dev/frontend_source_sets.mk`, that no tests or references were changed,
  and that only warning-level file-audit advisories remain.

## Architecture Review

### Semantic model and layout

PA11 remains the owner of source-level types, bindings, scopes, access facts,
constant evaluation, and class layout.  A class stores the canonical ordered
`direct_bases` and `direct_base_offsets`; `direct_base` and
`direct_base_offset` remain compatibility facts for the existing primary-base
and single-vptr paths.  `DirectBaseTypes` centralizes the compatibility
fallback, while `BaseTypeClosure` provides a cycle-safe breadth-first view for
semantic queries that must see every non-virtual base.

`Analyzer::ComputeClassLayout` is the single layout producer.  It assigns
deterministic non-virtual base and member offsets, applies empty-base storage
rules, and leaves the first-base vptr/virtual-layout behavior at the PA27
boundary.  Stable `Binding.member_owner` and `member_index` facts connect
lookup to layout without rediscovering ownership from emitted strings.
Bindings remain in stable scope-owned storage, so stored pointers survive later
scope additions.

Lookup and access use the typed model: PA14 member collection considers all
direct bases, direct declarations hide inherited declarations, using-declarations
supplement lookup, and distinct viable inherited candidates remain ambiguous.
Base-path adjustment walks typed layout edges and reports failure when a unique
path cannot be established.

### Lowering and object lifetime

PA14 remains the owner of typed LowIR lowering.  The same layout facts drive
field addresses, inherited method receiver adjustment, constructor forwarding,
copy construction, copy assignment, and destruction.  Direct bases are
initialized/copied/assigned in declaration order and destroyed in reverse order;
each operation adjusts to the individual base subobject.  Inherited-constructor
synthesis now has a responsibility-named
`pa14_lowering_inherited_constructors.cpp` module, and member-pointer
conversion has a responsibility-named `pa14_lowering_member_pointer.cpp`
module.  Both are listed in the cppgm++ source set.

PA25 object transfer and constructor paths consume the same direct-base helpers,
so empty-base, triviality, default construction, unwind, and aggregate
classification do not silently revert to first-base behavior.  PA17/PA25 RTTI
continues to implement only the existing polymorphic single-inheritance ABI.
Virtual inheritance, polymorphic multiple inheritance, and the remaining RTTI
cases are explicit PA27 work rather than implicit PA26 support.

### Member-pointer ABI

Member-pointer type formation and application remain typed
`TYPE_MEMBER_POINTER` operations.  Data-member pointers retain their existing
encoded offset representation; a base-to-derived conversion adds the unique
non-virtual base offset while preserving null.  Member-function pointers retain
the existing callable low half and use the high half of the existing i128
representation for a non-primary complete-object adjustment.  The conversion
enumerates typed base paths and rejects an ambiguous owner path.

The `.*`/`->*` expression path decodes that adjustment by AST node, and the
call emitter consumes it when forming the receiver's base-subobject address.
Primary/single-inheritance output remains unchanged.  This keeps the new ABI
fact local to member-pointer expressions and avoids globally changing ordinary
member calls.

### Template replay and parser boundary

PA18 replay associates expected destination types with AST node identity
rather than a mutable field-wide string.  Aggregate replay walks non-static
fields in declaration order, and the qualified member-template-address gate is
structural, so ordinary qualified function-template addresses stay on their
normal path.  Scope maps are swapped only for declarations that contribute a
binding and are restored on recursive exit and setup-time failure.

The shared LowIR comparator preserves enclosing declaration captures across
nested parsing and treats comma-bearing metadata as one value.  That fixes the
checkpoint's valid-output comparison failure without weakening semantic
comparison or accepting missing signatures.

## Final Architecture Review

The integrated implementation is a monotonic extension of the PA11-PA25
pipeline: source syntax is parsed once, typed semantic/layout facts are
shared by lookup, constants, and lowering, and LowIR emission consumes those
facts instead of reparsing source spellings.  Multi-base behavior is
centralized at the model/lowering handoff; it is not implemented as
test-specific output or as parallel first-base and all-base object models.

The final cleanup resolved the only file-audit blockers introduced by the
full-stage source growth.  Member-pointer conversion, inherited-constructor
synthesis, and constant-call noexcept analysis now have named translation-unit
ownership; all new translation units are in the cppgm++ source set.  The
remaining twelve file-audit messages are pre-existing warning-level
header-division, catch-all-helper, and nesting advisories.  No fatal size,
function-size, or unchecked-source-path finding remains.

No PA26 behavior is fabricated through a reference binary, host compiler,
interpreter, VM, trampoline, timeout, or source/test-specific gate.  Unsupported
virtual/PA27 cases still follow their existing semantic boundary.  The
architecture is ready for PA27's virtual/RTTI ABI work.

## Validation and handoff

- Focused PA26 validation: 66/66.
- PA15 preservation validation after the final primary-base adjustment fix:
  200/200.
- Build validation: `make -C dev cppgm++`.
- Required final gates: `perl scripts/cppgm_file_audit.pl --stage pa26 --paths dev/src`
  and `make test-report-through-pa26`.
- Handoff: PA27 owns virtual inheritance, polymorphic multiple inheritance,
  and RTTI requiring multiple vtable views; PA26 owns the complete supported
  non-virtual multi-base slice.
