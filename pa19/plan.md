# PA19 checkpoint plan

## Turn-start baseline

The turn began at **54/134 PA19 tests passing**, with all earlier assignments
through PA18 passing.  The complete failure set was grouped by shared
behavior before implementation:

- integral constant evaluation, typed non-type arguments, and constant
  bindings;
- dependent lookup, defaults, and stale semantic state;
- parameter packs and expansions; and
- specialization ownership, generated records, LowIR shape, plus one required
  rejection case for an invalid non-type pack argument.

## Remaining Work Map

No PA19 behavior remains.  The full current-PA set is passing; earlier PAs
remain passing; and the structural file audit has no fatal findings.  The
eight audit warnings are the repository's existing substantial-implementation
header warnings.

## Checkpoint Scope

This checkpoint covers the whole PA19 contract:

- typed integral constant evaluation, promotions, casts, literals, `sizeof`,
  `alignof`, and `static_assert`;
- integral non-type template arguments, defaults, dependent expressions,
  canonical specialization identity, and rejection of invalid argument types;
- dependent type/value lookup, aliases, overloads, defaults, and pack
  collection/expansion through calls, declarations, bases, aggregates, and
  `sizeof...`;
- static template-member storage and value propagation;
- constructor function-id deduction and nested template emission order; and
- specialization ownership, generated records, object initialization, and
  vtable/destructor LowIR shape.

The implementation keeps these facts in typed compiler state across PA11,
PA14, PA17, and PA18 boundaries.  The final structural increment also moved
large constant, global-collection, control, semantic, and template-rewrite
implementations into their per-tool translation units without changing the
compiler behavior.

## Checkpoint Result

Completed.  The PA19 report improved from **54/134** to **134/134**.  The
through-PA18 report remains **1430/1430**, and the PA19 file audit passes with
8 non-fatal pre-existing warnings.

## Post-checkpoint Remaining Work Map

None for PA19.

## Next Checkpoint Group

PA19 is complete.  The next assignment may begin from the clean committed
state produced by this checkpoint.
