# PA25 checkpoint plan

## Baseline

The turn-start PA25-local report passed 16 of 69 tests.  The complete
current-PA failure set was inspected and grouped by shared compiler behavior
before implementation.

## Remaining Work Map (refreshed)

- **RTTI, `typeid`, and `dynamic_cast` (complete):** all 14 grouped tests
  pass, including fundamental/pointer/class/template/lambda type-info
  naming, polymorphic expression queries, pointer casts, and
  `std::type_info` checks.
- **Initializer-list interoperation (complete):** all 11 scoped tests pass.
  This includes `auto` list deduction and packs, list object
  begin/size/element storage, initializer-list overload and constructor
  preference, assignment, nested construction, member-template deduction,
  reference/value arguments, and list range-for lowering.
- **Capturing-lambda replay (1 remaining):** distinct closure types produced
  by separate member-template instantiations still have a LowIR mismatch;
  direct/class/explicit-`this` captures and the trivial-dtor forwarding-call
  case now pass.
- **Indirect class-parameter ABI (1 remaining):** the parameter-prologue copy
  case still differs in LowIR.
- **Source exception lowering (10 remaining):** source try/catch and rethrow,
  catch conversion/ellipsis, parenthesized throw, class-template exception
  construction, a lambda/`decltype` EH fallback, hidden call unwind, outer
  catch cleanup, and the polymorphic array-reference cleanup timeout still
  fail exit status or lowering.
- **Hidden EH/temporary cleanup (11 remaining):** cross-function unwind cache,
  condition/conditional temporaries, const-reference and nested/default
  argument temporaries, protected-base construction, reference-prvalue
  cleanup, non-LIFO shared dispatch, and short-circuit RHS cleanup still
  differ in LowIR.
- **Prior-stage gate:** must be rerun after this increment; no tests or
  reference fixtures were changed.

## Completed checkpoint scope: RTTI

The first checkpoint covered typed RTTI/typeid and pointer-form dynamic-cast
lowering: on-demand fundamental, pointer, class, template-specialization,
and lambda RTTI globals; static and polymorphic `typeid` queries; supported
single-inheritance pointer casts; and the PA25 `std::type_info` diagnostics.

## Checkpoint result: RTTI

The RTTI/typeid/dynamic-cast group passed 14/14.  That increment raised the
PA25-local report from 16/69 to 30/69 and passed the through-PA24 report and
file audit at that point.

## Completed checkpoint scope: initializer lists and closure boundary

This turn completed the initializer-list materialization/interoperation
scope as one typed semantic/lowering increment.  It also completed the
closely related captured-call boundary needed by the scoped template case:
class captures use object transfer, explicit/default `this` uses the typed
captured pointer, and an xvalue closure returned through a forwarding
reference is called in place without an extra copy.  Empty local destructors
are elided when they have no body/member/base effects.

Validation: the 11 initializer-list tests and the four focused closure tests
pass exactly; non-empty braced arguments now deduce `initializer_list<U>`,
`new holder<T>(...)` replays its constructor template, initializer-list
layout is completed before size queries, and call-argument string literals are
pre-interned without changing aggregate-array behavior.  The refreshed full
PA25 report is 46/69.

## Next checkpoint group

Start with the hidden EH/temporary-cleanup group.  Unify temporary lifetime
marks and unwind-cache closure across conditionals, short-circuit branches,
default arguments, reference-bound prvalues, and shared dispatch, then
validate its 11 LowIR cases before regrouping the remaining source exception
status failures, lambda replay mismatch, and indirect-parameter ABI case.

## Prior-stage gate result

The required through-PA24 retry reached 2596/2600.  Its four failures were
timeouts only: PA3 `300-triple`, PA4 `410-trigraph-in-raw-string` and
`600-parameter-selected-macro-rescans`, and PA22
`400-enum-nttp-cstyle-cast-default-rebind`; no semantic mismatch was reported.
The preceding run reached 2598/2600 with two different timeout-only failures
(PA2 hard string concatenation and PA3 triple).  No tests or reference files
were changed; the gate remains the external validation item to rerun when
resource conditions are stable.

## Turn 135 failure audit and checkpoint scope

The fresh PA25-local report is 46/69, with the complete 23-test failure set
grouped as follows:

- **Source exception front-end/lowering status (8):** source rethrow and
  try/catch, class-template move-constructor throw, lambda/`decltype` EH
  fallback, parenthesized throw, base-reference catch, catch ellipsis, and
  a missed inner catch that must clean an outer scope.
- **Hidden EH and temporary cleanup (13):** cross-function unwind-cache
  reset; condition-call, conditional-expression, const-reference-bound,
  protected-base, nested/default-argument, managed-argument,
  reference-prvalue, shared-dispatch, short-circuit, shared-call hidden
  temporary, and polymorphic array-reference cleanup cases.
- **Template lambda replay (1):** distinct closure types from separate
  member-template instantiations have a LowIR helper-number/shape mismatch.
- **Indirect parameter ABI (1):** aggregate indirect-parameter prologue copy
  differs from the required LowIR.

The checkpoint scope is the hidden EH/temporary group: make temporary-lifetime
marks and unwind-cache closure explicit in the typed lowering state, preserve
cleanup actions on normal and exceptional exits (including short-circuit and
shared dispatch paths), and validate the 13 grouped cases plus the through-PA24
gate.  The next group after this checkpoint is source exception parsing/status,
followed by the remaining lambda replay and indirect-parameter ABI cases.

## Current turn checkpoint

The turn-start baseline for this implementation was 46/69 PA25 tests.  The
complete failure set was regrouped after the hidden cleanup work and the
source-exception increment.

### Remaining Work Map

- **Protected-destructor negative semantic check (1):** throwing a class with
  an inaccessible destructor must be rejected.
- **Lambda replay (2):** distinct member-template closure types and the
  function-template `lambda`/`decltype` EH fallback still differ in LowIR.
- **Indirect class-parameter ABI (1):** the prologue copy fixture still has a
  LowIR mismatch.
- **Template-array polymorphic cleanup (1):** the fixture still times out.

### Completed Checkpoint Scope

This increment covers typed source exception lowering for `try`/`catch`,
rethrow, ellipsis and base-reference matching, parenthesized throw
expressions, class-template exception-object construction, exception RTTI and
ABI globals, and nested outer-scope cleanup.  It also preserves the complete
hidden EH/temporary-cleanup group, including default arguments, conditions,
short-circuit paths, reference-bound prvalues, and shared unwind dispatch.

Validation: the current PA25 report is 64/69, up from 46/69; all direct source
exception fixtures and all focused hidden-cleanup fixtures pass.  The next
checkpoint is the protected-destructor diagnostic plus the two remaining
lambda/EH replay cases, then the indirect ABI and timeout investigation.

## Final checkpoint: PA25 full-stage implementation

Turn-start baseline: 46/69 PA25 tests.  The remaining work map at the start
of this checkpoint was grouped into source exception status, hidden temporary
and EH cleanup, template lambda replay, indirect class-parameter ABI, and the
polymorphic template-array cleanup case.

### Checkpoint Scope

This checkpoint completes the typed PA25 source-exception, RTTI, temporary
lifetime, constructor-unwind, reference-bound-prvalue, conditional and
short-circuit cleanup, shared-dispatch, initializer-list, lambda-closure,
object-transfer, template replay, and indirect-result lowering behavior.  It
also splits object transfer into responsibility-specific methods so the
implementation remains auditable without changing its semantic paths.

### Result

- Exact `make test-report ACTIVE_TEST_REPORT_PAS='pa25'`: **69/69 passed**.
- File audit: passed with repository warnings; no fatal findings.
- The exact through-PA24 retry reached **2599/2600**; its sole failure was the
  pre-existing PA22 `400-enum-nttp-cstyle-cast-default-rebind` timeout.  The
  clean starting revision reproduces that timeout, so no PA25 change is
  implicated and no test or reference fixture was changed.
- The complete PA25 failure set is now empty.  The next checkpoint is PA26;
  the earlier PA22 timeout remains an external prior-stage follow-up.
