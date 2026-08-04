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
