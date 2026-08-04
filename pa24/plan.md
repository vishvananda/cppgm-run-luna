# PA24 checkpoint plan

## Baseline and contract

The turn-start PA24 report was 26/104.  The complete failure set was grouped
before implementation into four shared behaviors: scoped-enum functional-cast
facts, capturing-lambda pack forwarding, dependent owner/lifecycle replay, and
dependent function-result recovery.  Earlier PAs were kept in scope for every
checkpoint.

## Remaining Work Map

No PA24 behavior remains failing: the current PA24 report is 104/104.

## Checkpoint Scope

The completed scope is the full PA24 contract: typed initialization and
aggregate tails, casts/conversions and `decltype`, captureless and capturing
lambda closure materialization, enclosing-pack forwarding, dependent owner and
default replay, concrete dependent-result recovery, and the PA14 lowering and
ABI facts consumed by those paths.  Validation covers PA24, through-PA23, and
the PA24 source audit.

## Checkpoint Result

- PA24 current report: 104/104.
- Through-PA23 semantic validation: 2496/2496 with the 120-second report build
  timeout.  The default 60-second parallel gate has one host-load-sensitive
  PA22 timeout (2495/2496); the isolated batch fixture completes in 49.27s and
  no semantic failure is observed.
- PA18 avoids cloning translation units that contain no lambdas, preserving the
  closure path while removing needless earlier-stage AST work.
- The PA23 nested-class member-template regression was fixed without regressing
  the PA18 nested-class or PA21 current-owner cases.

## Regression protections included

- Keep nested classes qualified while resolving bare dependent aliases, while
  qualifying nested-class type arguments for explicit member-template replay.
- Defer dependent member-body call materialization until concrete owner facts
  are available.
- Classify data-bearing pair-like results as indirect and emit ordinary roots in
  typed demand order.

## Next checkpoint group

PA24 is complete; the next assignment is PA25.  No additional PA24 checkpoint
group is required.
