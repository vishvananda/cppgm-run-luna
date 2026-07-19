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
The source audit passes for PA12.  The final cleanup removes the model
implementation-body warning by moving the shared model into
`pa11_semantics_model.cpp`; two non-fatal internal-header warnings remain for
the checkpoint's analyzer header and the inherited recognizer header.

## Grouped Remaining Work After Checkpoint

No PA12 behavior groups remain: all 127 checked-in PA12 tests pass.  No
earlier-assignment regressions remain: all 530 through-PA11 tests pass.  The
final integrated report passes 657/657 across 12/12 stages.  The final audit,
architecture review, and file review are complete; only the cohesive commit
and clean-worktree handoff remain as mechanical release steps.

## Next Checkpoint Group

Final full-stage handoff: commit the reviewed implementation and leave the
worktree clean.  No PA12 behavior group remains for another checkpoint.

## Architecture Review

The integrated PA12 implementation keeps the compiler pipeline and semantic
layers separate:

1. `dev/cppgm++.cpp` remains the driver boundary.  It validates the supported
   command shape, runs the existing preprocessing/token pipeline, parses the
   PA10 AST, frames each translation unit, and converts semantic exceptions to
   `EXIT_FAILURE`.  `EmitPA12Semantics` in `pa12_semantics.cpp` performs one
   analyzer pass followed by one semantic output pass; it does not invoke a
   reference binary, a host compiler, or a subprocess.
2. `pa11_semantics_model.h` now declares the shared `Type`, `Binding`,
   `Scope`, and `ConstantValue` records, while
   `pa11_semantics_model.cpp` owns their constructors, scope operations, type
   constructors, type rendering, and AST helper functions.  The source-set
   manifest registers that implementation for `cppgm++`, so PA11 and PA12
   use one semantic model and one set of externally owned definitions.
3. `pa11_semantics_analyzer.h` owns declaration/type formation, namespace and
   using lookup, constant evaluation, and the retained PA11 scope facts.
   `pa12_semantics_support.h` adds PA12 expression records and derived-type,
   value-category, and type-comparison helpers.  `PA12Printer` separates
   expression inference, conversion ranking/candidate choice, statement
   scope selection, and deterministic rendering over the already-built AST.
4. Scope lifetime is explicit.  `Scope` owns child scopes and ordered
   bindings; `TypePtr` owns type graphs; raw parent and owned-scope links point
   only into the live analyzer tree.  PA12 creates dedicated child scopes for
   if/while/do/switch conditions and for-loop init/condition/iteration/body
   processing.  A nested compound reuses an analyzer scope only when its
   recorded parent is the scope being printed, avoiding the PA11 prepass's
   parent mismatch and preventing loop or condition declarations from leaking
   into the enclosing function scope.
5. Semantic type records are not rewritten for output.  Anonymous enum and
   union spellings are kept in a printer-local display-name map with
   per-object allocation; `DisplayType` recursively applies those aliases to
   derived pointer, reference, array, function, and member-pointer types.
   Anonymous-union storage names are derived from the allocated display type,
   and the underlying PA11 type graph remains canonical for later passes.
6. The normal work is bounded by the existing AST and scope walks.  Lookup
   uses local maps, parent walks, and visited sets for using-directive graphs;
   semantic records are built once and output is rendered in source/binding
   order.  The PA12 call layer classifies only the README's supported
   standard-conversion subset, preserving clear extension points for later
   class/template assignments.

The file audit still reports two non-fatal body-division warnings:
`pa11_semantics_analyzer.h` contains the checkpoint's inline analyzer
implementation, and `recog_parser_internal.h` is inherited from the earlier
recognizer stages.  The PA12-introduced model warning was resolved in this
stage.  Neither warning is a required-gate failure, and moving the analyzer or
the shared recognizer parser would be a cross-stage ownership rewrite rather
than a PA12 correctness fix.

## Final Architecture Review

The final review of checkpoint `07f3a94` found all PA12 behavior green but
identified three integration risks that are now closed:

- Shared semantic model code was header-local and duplicated across PA11 and
  PA12 translation units.  It is now a single `pa11_semantics_model.cpp`
  implementation registered in `dev/frontend_source_sets.mk`.
- The checkpoint rewrote anonymous type names while printing and used fixed
  storage text.  Display aliases and per-object counters now preserve the
  semantic records, propagate names through derived types, and derive storage
  names from the selected type.
- The analyzer's PA11 prepass records ordinary compounds but not control
  statement scopes.  PA12 now creates the proper control scopes while printing
  and validates the recorded compound parent before reuse.  Pointer-to-void
  conversion also preserves source pointee cv-qualification, and all integer
  zero literals participate in null-pointer-constant classification.

The cleanup does not alter PA1--PA11 fixtures or parser ownership, does not add
test/ref-file edits, and leaves the PA12 implementation dependent only on the
shared compiler pipeline and semantic records.  The stage is ready for the
next assignment after the final required commands and clean-worktree check.
