# PA26 checkpoint plan

## Baseline and latest validation

The turn-start PA26 baseline was 41/66.  The checkpoint before this audit was
65/66; the completed audit fixes leave PA26 at **66/66**.  The required
through-PA25 report passes **2669/2669**, and the complete through-PA26 report
passes **2735/2735**.

## Remaining Work Map

### Current PA failure set

There are no current PA26 failures.  The previously reported
`general/300-member-pointer-pack-decltype-memfn.t` case compiled successfully
and had all three exit-status sidecars; its failure was in the shared LowIR
comparison parser.  The parser now preserves enclosing declaration captures
across nested parsers and accepts comma-bearing `object=` metadata, so the
case passes without changing its source or reference fixture.

No semantic PA26 failure remains to group or defer.

## Checkpoint Scope

This checkpoint covers the final member-pointer/template replay group landed
by `21f80f9`, `e4c1f96`, `0a80a4f`, and `814aafc`:

1. Resolve aliases before comparing friend-ADL template candidates so a
   pre-existing friend definition wins over a sibling specialization.
2. Compute `sizeof` for an overloaded dependent expression from its rewritten
   expression type.
3. Queue replayed nested out-of-class static-data definitions at namespace
   scope while preserving anonymous-namespace identity and the generated
   owner mapping.
4. Carry the destination type for each member-template address in an
   aggregate initializer by AST node and declaration-order field, and avoid
   reevaluating replayed pointer-cast sources.

The audit also corrected the shared comparison parser exposed by the final
checkpoint case.  Validation covers the focused replay cases, the complete
PA26 suite, both through-stage reports, and the source file audit.  No tests
or reference fixtures were changed.

## Checkpoint Result

The implementation preserves all earlier assignments and raises the current
PA result from the checkpoint's 65/66 to 66/66.  Friend ADL retains the
existing definition, dependent `sizeof` uses the overloaded expression type,
static-table replay materializes namespace-scope storage with the expected
wrapper ABI, and aggregate member-template addresses use the corresponding
field type.  The comparator now validates the resulting LowIR instead of
aborting on valid metadata.

The PA26 file audit passes with warning-level findings only; the compact
collection header remains at its 1200-line limit and no new source file or
unchecked source path was introduced.

## Next Checkpoint Group

PA26 has no remaining failure group.  The next substantial checkpoint is the
planned PA27 virtual/RTTI ABI group:

- shared virtual-base layout and access;
- multiple active vtable views for polymorphic multiple inheritance;
- pointer-form sibling `dynamic_cast` and RTTI through non-primary views.

Those are the PA27 handoff boundary in `pa26/README.md`, not deferred PA26
behavior.
