# PA26 checkpoint plan

## Baseline and latest validation

The turn-start PA26 baseline was 41/66.  The current report is 65/66.  The
through-PA25 report passes 2669/2669, and the PA26 source audit passes with
warnings only.

## Remaining Work Map

### Harness-only comparison

- `general/300-member-pointer-pack-decltype-memfn.t` reaches successful
  compilation, but the comparison script aborts on an undefined sidecar.

No semantic PA26 test remains in the compiler failure set.  The remaining
failure is confined to comparison-harness sidecar handling.

## Checkpoint Scope

This checkpoint covers the remaining static-data replay group and the two
already-isolated semantic replay fixes:

1. Resolve aliases before comparing friend-ADL template candidates so a
   pre-existing friend definition wins over a sibling specialization.
2. Compute `sizeof` for an overloaded dependent expression from its rewritten
   expression type.
3. Queue replayed nested out-of-class static-data definitions at namespace
   scope, preserving anonymous-namespace identity for relative declarators;
   propagate the first aggregate field's typed function-pointer destination
   into member-template address selection; and avoid evaluating replayed
   pointer-cast sources twice.

Validation covers these three focused cases, the full PA26 report, the
complete through-PA25 regression report, and the PA26 source audit.  No tests
or reference fixtures are changed.

## Checkpoint Result

PA26 improved from 41/66 to 65/66.  Friend ADL retains the existing
definition, dependent `sizeof` uses the overloaded expression's type, and the
static table replay now materializes namespace-scope storage with the
two-argument field-compatible wrapper ABI.  The focused PA26 cases and the
PA18 unnamed-namespace regression pass; through-PA25 remains 2669/2669.

## Next Checkpoint Group

The next checkpoint group is only the harness-side comparison case:

- `general/300-member-pointer-pack-decltype-memfn.t`

The compiler reaches successful compilation, but the comparison script still
aborts on its undefined exit-status sidecar.  Keep this isolated as a
harness-side issue; no tests or reference fixtures are changed.
