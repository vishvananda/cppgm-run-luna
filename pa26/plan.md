# PA26 checkpoint plan

## Baseline and latest validation

The turn-start PA26 baseline was 41/66.  The completed checkpoint reports
51/66, while the complete through-PA25 report remains 2669/2669.

## Remaining Work Map

### Semantic replay and lookup

- `general/300-friend-template-adl-existing-definition.t`
- `general/300-member-function-pointer-nttp-inherited-type-collision-sfinae.t`
- `general/300-using-directive-template-id-member-pointer-owner.t`
- `general/400-reference-reset-recollects-inclass-template.t`
- `general/400-top-level-cv-function-pointer-partial-ordering.t`
- `spec/300-template-comma-operator-return-construction.t`
- `spec/300-using-member-operator-template-hides-inherited-instantiations.t`

These are the remaining friend/ADL, inherited-owner, using-directive,
recollection, partial-ordering, and operator/construction selection edges.

### LowIR shape and comparison

- `general/300-bind-typed-member-pointer-source-arg.t`
- `general/300-member-function-pointer-type-arg-qualified-void.t`
- `general/300-overloaded-arrow-star-callee-call.t`
- `general/300-qualified-template-member-type-sfinae.t`
- `general/300-structured-bool-conditional-member-pointer-dead-branch.t`
- `spec/300-ambiguous-member-nontype-arg-sfinae.t`
- `spec/300-overloaded-member-pointer-function-template-deduction.t`
- `general/300-member-pointer-pack-decltype-memfn.t` (the report comparator
  still aborts while reading an undefined comparison sidecar)

These compile or reach the expected semantic path but still need canonical
member-pointer call/data or comparison-surface alignment.

## Checkpoint Scope

This checkpoint completes the pack/base and typed member-pointer replay
increment:

1. Parse class template-ids and pack expansions in base and constructor
   initializer names, preserving namespace and parameter scope.
2. Correlate multiple active packs by element index for dependent bases and
   constructor replay, while retaining the established single-pack deferral
   behavior used by earlier assignments.
3. Carry typed member-pointer facts through qualified address formation,
   direct data/function lowering, `.*`/`->*`, null/conversion checks, template
   argument validation, deduction, and replayed dependent member targets.
4. Match concrete direct-base initializers by type binding, without treating
   member variables as base aliases, and preserve prior polymorphic cleanup.

Validation for the scope includes the PA26 report, focused pack/replay tests,
the affected PA25 constructor/typeid tests, and the complete through-PA25
report.  No tests or reference fixtures were changed.

## Checkpoint Result

Complete for this turn: PA26 improved from 41/66 to 51/66, the affected PA25
tests pass, the through-PA25 gate passes 2669/2669, and the stage file audit
passes.  Fifteen PA26 tests remain in the grouped map above.

## Next Checkpoint Group

Take the seven semantic replay/lookup and operator/construction exit failures
as the next group.  Then address the eight LowIR/comparison-shape failures.
