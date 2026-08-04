# PA25 checkpoint plan

## Baseline

The first PA25-local report at the start of this checkpoint passed 16 of 69
tests.  The complete failure set was inspected from the report and its
canonical LowIR diffs before implementation.

## Remaining Work Map

- **RTTI, `typeid`, and `dynamic_cast` (complete):** all 14 grouped tests now
  pass, including fundamental/pointer/class/template/lambda type-info naming,
  polymorphic expression queries, pointer casts, and `std::type_info` checks.
- **Capturing-lambda closure behavior (remaining):** class-copy capture,
  explicit/default `this`, nested/template replay, callable ownership, and the
  remaining lambda/EH interactions still fail in the current report.
- **`std::initializer_list` interoperation (remaining):** braced arguments,
  assignment and constructor selection are still flattened into ordinary
  arguments; typed begin/size storage, initializer-list overload preference,
  member-template deduction, and range-for materialization remain incomplete.
- **Exception source lowering and temporary cleanup (remaining):** source
  try/catch, rethrow, catch conversion/ellipsis, parenthesized throw,
  class-template exception construction, hidden-temporary cleanup, and unwind
  cache behavior still account for the current exit/mismatch failures and one
  timeout.
- **Prior-stage gate (complete):** the through-PA24 report now passes 2600/2600.

## Checkpoint Scope

Complete the **RTTI/typeid and pointer-form dynamic-cast group** as one typed
semantic/lowering increment.  The scope includes on-demand fundamental,
pointer, class, template-specialization, and lambda RTTI globals; static and
polymorphic `typeid` LowIR address/load/branch lowering; supported
single-inheritance pointer `dynamic_cast` success/failure control flow; and
the PA25 `std::type_info`/reference-form diagnostics exercised by the group.
Validation is the full set of grouped PA25 RTTI/cast tests, the PA25 file
audit, the through-PA24 report, and the current-PA report.

## Checkpoint result

The RTTI/typeid/dynamic-cast group passes 14/14.  The PA25-local report is
30/69, improving the 16/69 baseline, while the through-PA24 report passes
2600/2600.  The PA25 file audit passes with only its pre-existing warnings.
The remaining PA25 failures are grouped under initializer-list lowering,
capturing-lambda replay, and exception/temporary-cleanup behavior.

## Next checkpoint group

After this checkpoint, take the initializer-list materialization/overload
group, then the remaining capturing-lambda replay group and exception/cleanup
group, while keeping the through-PA24 gate green.
