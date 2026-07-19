# PA12 Running Plan

## Baseline

At the start of this stage, the required command
`make test-report ACTIVE_TEST_REPORT_PAS='pa12'` reports `0 / 127` PA12
tests passing.  Every case reaches the PA12 dispatcher but receives
`EXIT_NOT_IMPLEMENTED`, so the complete current-PA failure set is the 127
checked-in `.t` files under `pa12/tests/` (the same set is reproduced by the
stage report in the Ralph log).

## Remaining Work Map

The failures are grouped by compiler behavior rather than by the current
stub's common exit status:

1. **Semantic driver and deterministic dump framing (all 127 cases).**
   Replace the PA12 `EXIT_NOT_IMPLEMENTED` path with preprocessing, PA10
   parsing, PA12 analysis, and the required translation-unit wrappers.

2. **Declarations, namespaces, aliases, and scopes.**  This covers
   `spec/100-empty.t`, `spec/100-function-decls.t`,
   `spec/300-block-scope-namespace-alias-qualified-call.t`, the declaration
   and namespace cases in `general/100-*` (`100-namespace-alias-call.t`,
   `100-nested-namespace-call.t`, `100-using-declaration-call.t`,
   `100-using-directive-call.t`), the local declaration/scope cases in
   `general/200-*`, and the namespace, unnamed-namespace, local-extern,
   parameter-renaming, enum, class, and pointer-type declarations in
   `general/300-*`.

3. **Expression semantic nodes and value categories.**  This covers
   `spec/100-local-arith.t`, `spec/200-unary-logical-conditional.t`,
   `spec/200-subscript-expression.t`, `spec/200-sizeof-typeid.t`, and the
   assignment, comma/bitwise/shift, pointer arithmetic/comparison,
   subscript, increment, cast, functional-cast, enum, floating-literal, and
   `__builtin_constant_p` cases in `general/100-*` through `general/300-*`.

4. **Calls, conversion classification, and overload resolution.**  This
   covers `spec/100-simple-call.t`, `spec/100-qualified-namespace-call.t`,
   `spec/100-reference-binding.t`, `spec/100-overload-exact.t`,
   `spec/100-overload-ranking.t`, `spec/200-array-decay-call.t`,
   `spec/200-function-pointer-call.t`, and all `spec/300-*` call/conversion
   cases, plus the call, reference, pointer, `nullptr`, variadic, using, and
   function-pointer cases in `general/100-*` through `general/300-*`.

5. **Statement scopes and control flow.**  This covers
   `spec/200-if-control-flow.t`, `spec/200-do-statement.t`,
   `spec/200-for-loop.t`, `spec/200-switch-statement.t`,
   `spec/300-condition-declaration-scope.t`, and the while/loop-jump,
   condition, switch-case/default, and local-scope cases in `general/200-*`
   and `general/300-*`.

6. **Expected semantic failures.**  Preserve `EXIT_FAILURE` for
   `spec/100-bad-no-match.t`, `spec/300-bad-ambiguous-overload.t`,
   `spec/300-compound-assignment-lvalue-bad.t`,
   `spec/300-nullptr-t-vs-long-overload-bad.t`, the bad reference-binding
   cases in `general/100-*`, `general/200-bad-noncallable-variable.t`, and
   `general/300-most-vexing-local-function-member-call-bad.t`.

## Checkpoint Scope

The first implementation checkpoint covers groups 1, 2, the procedural core
of groups 3 and 4, and group 5: a shared PA12 semantic model will collect
namespace/function/variable/type bindings, form function and block scopes,
resolve literals, identifiers, declarations, calls, basic conversions and
operators, analyze the supported statement forms, and print the exact
indented PA12 tree.  It also establishes the extension points for pointer,
reference, array, enum, `nullptr`, and overload conversion ranking so the
remaining edge cases can be completed without reparsing or test-specific
output.

Validation for this checkpoint is the PA12 local report, the through-PA11
report, and the source audit.  The checkpoint is complete only when the
current-PA pass count is above the 0/127 baseline (or PA12 is fully green)
and earlier assignments remain green.

## Checkpoint Result

The checkpoint is complete.  The PA12 semantic driver, shared typed semantic
model, parser extensions, expression/value-category analysis, declaration and
scope handling, overload/conversion ranking, statement/control-flow analysis,
and deterministic failure output are implemented.  The required local report
passes `127 / 127`, and `make test-report-through-pa11` passes `530 / 530`.
The source audit passes for PA12 with only the existing implementation-body
warnings in internal headers.

## Grouped Remaining Work After Checkpoint

No PA12 behavior groups remain: all 127 checked-in PA12 tests pass.  No
earlier-assignment regressions remain: all 530 through-PA11 tests pass.  The
remaining work is final required-report verification, plan/file-audit review,
temporary build-artifact cleanup, and the cohesive commit.

## Next Checkpoint Group

Final full-stage handoff: rerun the exact required PA12 report and file audit,
verify the plan and diff, commit the implementation, and leave the worktree
clean.
