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
- Required through-stage validation: `make test-report-through-pa24` passes
  2600/2600 tests across all 24 stages.
- The PA24 source audit passes with the same 12 non-fatal repository advisories
  recorded by the preceding stage; neither PA24 implementation module adds a
  fatal or new file-shape finding.
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

## Architecture Review — final PA24 stage audit — 2026-08-04

The completed PA24 stage remains a monotonic extension of the existing compiler
pipeline:

- The parser and PA10 AST remain the syntax boundary.  The PA24 parser changes
  preserve structured initializer, lambda, and range-for nodes, including the
  source token span used for stable lambda identity.  No LowIR answer is
  embedded in parsing.
- PA18 remains the template/materialization seam before semantic analysis.
  `PA18TemplateExpander` prepares synthetic lambda classes, records typed
  capture specifications, rewrites captured bodies on cloned working trees,
  and materializes deferred closure classes only when substitutions are
  concrete.  Ordinary programs without lambdas reuse their AST trees without
  the old unconditional clone; lambda-bearing trees retain the clone before
  the mutating preparation pass.
- PA11 owns typed expression and declaration facts used by this stage:
  `decltype` category handling, conversion-function viability/ranking, using
  declarations, and generated-scope predeclaration.  PA24 does not recover
  those facts from emitted LowIR or from fixture names.
- PA14 owns the source-to-LowIR implementation.  `PlannedType` and
  `DeduceAutoType` resolve placeholder variables from typed initializer facts;
  `PlanRangeFor`/`EmitRangeFor` lower arrays, braced ranges, and supported
  begin/end ranges through ordinary variables, conditions, blocks, updates,
  and cleanup; and `EnsureLambdaFunction` plus the closure helpers connect
  callable demand, closure initialization, ABI construction, and body
  emission to the existing function/object paths.
- Existing PA14 demand, constructor, destructor, global initialization, ABI,
  base-adjustment, and LowIR emission paths remain the single backend.  Lambda
  functions are internal demand-driven `FunctionRecord`s, and generated
  closure classes use the same typed class layout and special-member logic as
  other objects.  Order-sensitive instructions and lifecycle actions are
  still emitted by the normal lowering state machine.
- Ownership is explicit at each handoff: shared AST/type pointers own the
  syntax and semantic graphs, scopes own child scopes and bindings, and
  deque-backed PA14 function/global records keep lookup and demand pointers
  stable.  Lambda identity crosses the PA18-to-PA14 AST rebuild by source span
  and closure maps rather than by a stale AST pointer.
- `dev/frontend_source_sets.mk` registers the two new responsibility-named
  implementation units, `pa14_lowering_lambdas.cpp` and
  `pa18_templates_lambdas.cpp`.  Both are below the source-audit limits.

## Final Architecture Review

The final checkpoint that completed PA24 is `2ae8a70` (`Implement PA24 full-stage
semantics`), following the typed initialization/range checkpoint `9015e71` and
the callable lambda checkpoint `9cb2712`.  The integrated handoff also includes
`c8445fa`, which removes unnecessary PA18 AST cloning for translation units that
contain no lambda expression while preserving the cloning path for all lambda
rewrites.

The final review covered the complete PA24 test categories: `auto` variables and
returns, direct braced scalar/array/aggregate initialization, conversion and
cast closure, captureless and supported capturing/nested lambdas, range-for
forms, and the dependent owner/result recovery witnesses.  The changed source
set contains no reference-binary invocation, host-compiler delegation,
test-specific acceptance branch, timeout-only success path, embedded output,
or unchecked source fragment.  The file audit's 12 warnings are the
established shared-header/catch-all/duplicate-block advisories from earlier
stages; there is no new fatal or PA24-specific warning to carry forward.

Final architecture conclusion: PA24 preserves all earlier assignments, keeps
semantic ownership and LowIR lowering on the intended staged path, and is
consolidated for the PA25 handoff.  The exact audit and gate evidence is
recorded in `pa24/audit.md`.
