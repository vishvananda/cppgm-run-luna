# PA26 checkpoint plan

## Baseline and latest validation

The turn-start PA26 report passed 14 of 66 tests.  The latest clean report
passes 41 of 66.  The required through-PA25 report passes all 2669 tests.

## Remaining Work Map

### Pack-expanded base collection

- `general/100-pack-expanded-base-template-parameter-lookup.t`

The PA10 parser/collector still loses the namespace boundary after a
pack-expanded base.  This is a source-collection fix, separate from the
member-pointer lowering work.

### Dependent member-pointer replay and lookup

- `general/300-dependent-member-template-nontype-target-overload.t`
- `general/300-forwarded-data-member-pointer-nontype-base.t`
- `general/300-member-function-pointer-nontype-partial-specialization-call.t`
- `general/300-member-function-pointer-nttp-inherited-type-collision-sfinae.t`
- `general/300-member-pointer-pack-decltype-memfn.t`
- `general/300-qualified-template-id-owner-function-call-probe.t`
- `general/300-template-member-function-template-nttp-dedup.t`
- `general/300-template-member-pointer-nttp-execute.t`
- `general/300-template-member-pointer-nttp-inline-method-reference.t`
- `general/300-using-directive-template-id-member-pointer-owner.t`
- `general/400-reference-reset-recollects-inclass-template.t`
- `general/400-top-level-cv-function-pointer-partial-specialization-ordering.t`
- `spec/300-member-pointer-nontype-template-parameter.t`
- `spec/300-member-pointer-parameter-variadic-deduction.t`
- `spec/300-overloaded-member-pointer-function-template-deduction.t`
- `spec/300-using-member-operator-template-hides-inherited-instantiations.t`

These are the remaining shared typed-state edges: inherited member-template
visibility, dependent non-type replay, owner recovery, pack deduction, and
partial-specialization ordering.

### Friend, operator, and construction selection

- `general/300-friend-template-adl-existing-definition.t`
- `spec/300-template-comma-operator-return-construction.t`

Friend/ADL declaration identity and the inherited operator/construction
selection path still diverge after replay.

### LowIR shape canonicalization

- `general/300-bind-typed-member-pointer-source-arg.t`
- `general/300-member-function-pointer-type-arg-qualified-void.t`
- `general/300-overloaded-arrow-star-callee-call.t`
- `general/300-qualified-template-member-type-sfinae.t`
- `general/300-structured-bool-conditional-member-pointer-dead-branch.t`
- `spec/300-ambiguous-member-nontype-arg-sfinae.t`

These compile through semantic lowering but differ in canonical LowIR for
member-pointer calls, `.*`/`->*`, qualified function types, and structured
boolean branches.

## Checkpoint Scope

This turn completes the typed member-pointer/template increment and its
earlier-stage compatibility boundary:

1. Parse and preserve data/function member-pointer types, owners, cv
   qualifiers, references, packs, and member-pointer declarators.
2. Carry typed member-pointer values through address formation, null and
   conversion checks, deduction, partial specialization, generated nested
   owners, and cached replay.
3. Lower built-in `.*` and `->*`, member-pointer calls/data access, base-path
   adjustment, and the associated multi-base/RTTI facts.
4. Restrict the member-pointer `static_cast` decltype probe to actual
   member-pointer types, preserving ordinary function-pointer call results and
   SFINAE behavior in PA22/PA23.

Validation covers the clean PA26 report and the complete through-PA25 report.

## Checkpoint Result

Completed: PA26 improved from 14/66 to 41/66, and through-PA25 is 2669/2669.
The three earlier decltype/trailing-pack regressions were isolated to the
over-broad member-pointer cast probe and now pass.  Temporary diagnostics were
removed.

## Next Checkpoint Group

Fix the pack-expanded-base collection case together with the dependent
member-pointer replay/lookup group, then rerun the PA26 report before taking
on the six LowIR-shape mismatches.
