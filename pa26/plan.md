# PA26 checkpoint plan

## Baseline and latest validation

The turn-start PA26 baseline was 41/66.  The current report is 62/66.  The
through-PA25 report passes 2669/2669, and the PA26 source audit passes with
warnings only.

## Remaining Work Map

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

This checkpoint covers the four executable member-pointer expression and
lowering cases that share typed result propagation and replayed dependent
operators:

1. Classify member-pointer-containing trivial aggregates for direct object
   returns and by-value arguments.
2. Elide only typed empty/trivial transfers with no user-provided constructor,
   preserving generated helper ABI and earlier constructor effects.
3. Defer dependent operator using-declarations until their generated owner is
   materialized, then replay the inherited operator lookup.
4. Bind dependent function-template defaults left-to-right and reject an
   unavailable qualified member type at the candidate's SFINAE boundary.

Validation covers the four focused cases, the full PA26 report, the complete
through-PA25 regression report, and the PA26 source audit.  No tests or
reference fixtures are changed.

## Checkpoint Result

PA26 improved from 41/66 to 62/66.  The four scoped tests now pass: member
pointer object-return/argument ABI is direct for trivial payloads, dependent
operator using-declarations replay without premature lookup failure, empty
trivial object transfers do not emit unused parameter loads, and dependent
function-template defaults bind left-to-right so unavailable member types are
rejected by SFINAE.  Three earlier-PA regressions caused by over-broad empty
object elision were narrowed to types without user-provided constructors; the
focused PA18, PA21, and PA22 regressions pass again.  Four current-PA cases
remain in the grouped map above.

## Next Checkpoint Group

The next checkpoint group is the three executable semantic-replay cases:

- `general/300-friend-template-adl-existing-definition.t`
- `general/300-using-directive-template-id-member-pointer-owner.t`
- `general/400-reference-reset-recollects-inclass-template.t`

The shared behavior is replay of already-defined friend/using declarations and
static in-class template state after member-pointer owner substitution.  Keep
`general/300-member-pointer-pack-decltype-memfn.t` isolated as a harness-side
undefined-sidecar failure unless compiler output supplies the missing sidecar.
