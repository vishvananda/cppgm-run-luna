# PA26 checkpoint plan

## Baseline and latest validation

The turn-start PA26 baseline was 41/66.  The current report is 58/66.  The
through-PA25 report passes 2669/2669, and the PA26 source audit passes
with warnings only.

## Remaining Work Map

### Exact LowIR and ABI shape

- `general/300-bind-typed-member-pointer-source-arg.t`
- `general/300-overloaded-arrow-star-callee-call.t`
- `spec/300-using-member-operator-template-hides-inherited-instantiations.t`
- `general/300-qualified-template-member-type-sfinae.t`

These now compile, but their generated LowIR still differs in object-return,
generated-helper, or unused-parameter shape.

### Semantic replay

- `general/300-friend-template-adl-existing-definition.t` still has a
  successful-code exit mismatch while its generated helper definitions are
  present.
- `general/300-using-directive-template-id-member-pointer-owner.t` still has
  a successful-code exit mismatch in using-directive owner replay.
- `general/400-reference-reset-recollects-inclass-template.t` still misses
  the instantiated static table and wrapper helpers.

### Harness-only comparison

- `general/300-member-pointer-pack-decltype-memfn.t` reaches successful
  compilation, but the comparison script aborts on an undefined sidecar.

## Checkpoint Scope

This checkpoint covers typed member-pointer replay and the dependent semantic
edges that can be completed together:

1. Normalize cv-qualified member-pointer owners and validate typed function
   address arguments during template replay.
2. Substitute and reject unavailable dependent function-template defaults in
   both rewrite and candidate-probe paths, preserving SFINAE behavior.
3. Preserve member-pointer truth values as the typed integer representation in
   lowering, including constant structured-bool conditional folding.
4. Detect ambiguous inherited type/value member-name sources during nontype
   argument validation.

Validation covers the five focused fixes, the full PA26 report, the complete
through-PA25 regression report, and the PA26 source audit.  No tests or
reference fixtures are changed.

## Checkpoint Result

PA26 improved from 41/66 to 58/66.  The focused dependent-default/SFINAE,
structured-bool, overloaded-member-pointer deduction, and ambiguous-nontype
tests pass.  Eight current-PA cases remain in the grouped map above; earlier
assignments remain passing at 2669/2669.

## Next Checkpoint Group

Address the four exact LowIR/ABI cases as one lowering-shape group, then take
the friend/ADL and static-table replay cases together.  Keep the comparator
abort isolated unless its sidecar is produced by compiler behavior.
