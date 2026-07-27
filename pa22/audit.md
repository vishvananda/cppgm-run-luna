# PA22 checkpoint audit

## Checkpoint 69 Audit

### Scope Reviewed

Reviewed the Checkpoint 69 scope and result in [plan.md](plan.md), the PA22
assignment contract in [README.md](README.md), the latest checkpoint commit
`a0f870a` and its PA21 audit parent `97e8cf3`, the complete current-PA report
at `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`, and
the changed implementation in `dev/src/pa14_lowering_values.cpp`,
`dev/src/pa18_templates_calls.cpp`, `dev/src/pa18_templates_collection.cpp`,
`dev/src/pa18_templates_rewrite.cpp`, `dev/src/pa18_templates_rewrite.h`,
`dev/src/pa18_templates_rewrite_infer.cpp`, and
`dev/src/pa18_templates_rewrite_specialization.cpp`.

The review covered semantic completeness, replay ownership, candidate lookup,
timeout behavior, hot-path work, file-audit limits, and preservation of all
earlier assignments.

### Findings

- The checkpoint’s broad call-callee validation exception accepted arbitrary
  identifiers in function position.  That was a source-specific acceptance
  gate, so it was narrowed to the actual builtin value-initialization form.
- Callable data-member return types were not preserved before overload
  deduction.  That allowed an unknown operand to select an unrelated lazy
  operator template and caused the reported timeout.  The fix recovers the
  declared `operator()` type and uses an active-set recursion check for
  inherited callable classes; it does not add a timeout cap or success path.
- Array-reference deduction lost the literal’s typed array bound and could
  normalize `T const&` into `T const&[N]`.  Typed literal array references,
  declarator normalization, direct complete-type binding, and PA14 array cv
  ranking now preserve the semantic type through deduction and lowering.
- Operator-template replay did not reliably include the operand’s associated
  namespace.  Lookup now appends only deduplicated, typed associated-owner
  candidates, while ordinary global lookup remains unchanged.  Return types
  are propagated only for owner-qualified templates, preserving the earlier
  PA19 rematerialization contract.
- The new matcher logic initially crossed the `pa18_templates_rewrite.cpp`
  file-size limit and enlarged inference functions past their audit limits.
  The direct matcher is owned by the specialization helper, and call,
  identifier, member, and callable inference are separated into bounded
  helpers.

No compiler phase was skipped.  The checkpoint contains no fallback-success
path, dummy output, embedded payload, interpreter/VM/trampoline substitute,
test-specific acceptance gate, emitted-text execution, weakened test, or
unchecked implementation fragment.  Associated lookup scans only the
 indexed operator candidates for the typed operands and deduplicates them;
 it does not walk the test suite or reparse emitted LowIR.  Ownership and
 return facts remain on definitions and replay AST nodes rather than being
 reconstructed downstream from generated names.

### Changes Made

- Added typed callable-object return recovery and split `InferArgument` into
  focused member, identifier, and call helpers.
- Added literal array-reference typing, reference-preserving normalization,
  direct complete-type matching, and array cv-compatible conversion ranking.
- Added typed associated-owner operator-template discovery and scoped return
  propagation, while narrowing template validation to the valid builtin form.
- Centralized direct type-parameter matching in
  `pa18_templates_rewrite_specialization.cpp` and brought all changed files
  under the PA22 file-audit limits.
- Refreshed the plan result and replaced its prose-only work map with the
  exact 162-fixture current-PA failure set and the next checkpoint grouping.

### Validation

- Required prior-through check: `make test-report-through-pa21` — **1850/1850
  passed**.
- Required file audit: `perl scripts/cppgm_file_audit.pl --stage pa22 --paths
  dev/src` — **passed** with the repository’s 10 pre-existing warnings.
- Required active report: `make test-report ACTIVE_TEST_REPORT_PAS='pa22'` —
  **88/250 passed**, up from the checkpoint start baseline of 86/250, with
  **0 timeout failures** and 162 remaining fixtures recorded in `plan.md`.
- The repaired array-reference fixture emits valid LowIR in 0.10 seconds,
  including the typed callable return and associated `operator<<` calls.
- `make -C pa19 check TEST=tests/general/400-defaulted-template-member-call-rematerialization.t`
  passes after the scoped return-propagation fix.
- `make build` and `git diff --check` pass; the final commit leaves
  `git status --short` empty.

## Checkpoint 71 Audit

### Scope Reviewed

Reviewed the latest Checkpoint 71 scope and result in [plan.md](plan.md), the
PA22 contract in [README.md](README.md), commits `fede37f`, `a19082c`, and
`a0f870a`, the complete active report at
`/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`, and the
changed implementation in `pa18_templates_calls.cpp`,
`pa18_templates_collection.h`, `pa18_templates_rewrite_arguments.cpp`,
`pa18_templates_rewrite_emit.cpp`, `pa18_templates_rewrite_infer.cpp`,
`pa18_templates_rewrite_instantiate.cpp`,
`pa18_templates_rewrite_instantiate.h`, `pa18_templates_rewrite_members.cpp`,
and `pa18_templates_rewrite_text.cpp`.

The review covered typed substitution ownership, candidate viability and
lookup, deferred replay, nested type and constant facts, hot-path work,
file-audit limits, timeout behavior, and preservation of PA1–PA21.

### Findings

- The checkpoint’s new candidate loops caught `logic_error` broadly and used
  every semantic failure as a candidate drop.  That was an unsafe fallback:
  hard errors could be hidden and SFINAE status was not represented at the
  boundary.  Candidate failures now use `PA18SubstitutionFailure`; deduction
  preserves that status through its wrappers, while hard failures are not
  accepted as successful candidates.
- Generated-member viability had a source-declaration/name scan fallback,
  including `ContainsName`, after emission.  That recovered semantic facts
  downstream from generated text.  The fallback was removed; nested class,
  forward-class, and enum entities are recognized by the typed member lookup,
  and generated nested entities are checked through the indexed class maps.
- Qualified non-type arguments had a literal `::value` special case.  It is
  now a generic qualified-member path that delegates to the indexed constant
  owner/declaration evaluator, so no member name or payload is embedded in
  acceptance logic.
- The non-type template function index is a non-owning declaration index and
  remains separate from ordinary function signatures; AST ownership is not
  duplicated.  The implementation was also kept within all PA22 size and
  function-audit limits.
- No compiler phase is skipped.  There is no dummy output, embedded payload,
  interpreter/VM/trampoline substitute, test-specific acceptance gate,
  timeout workaround, emitted-text execution, weakened check, or unchecked
  implementation fragment.  The new validation performs a bounded generated
  type walk over indexed semantic facts; it does not walk the suite or reparse
  emitted LowIR.

### Changes Made

- Added the typed substitution-failure status and narrowed the Checkpoint 71
  candidate catches to that status.
- Preserved the typed status through integral-argument replay and made
  `InferFunctionArguments` rethrow deduction-time semantic failures as the
  typed candidate status.
- Replaced the generated-source member fallback with indexed nested-entity and
  `FindClassMemberType` lookup.
- Replaced the hardcoded qualified `::value` path with
  `EvaluateQualifiedConstantMember` backed by the existing constant-member
  tables.
- Reworked the affected formatting and compacted the changed implementation
  only as needed to restore the PA22 file-audit limits; no checked tests or
  reference fixtures were changed.
- Refreshed the plan with the complete 156-fixture failure map, grouped into
  58 substitution/deferred cases, 46 deduction/ordering cases, and 52
  aliases/owners/NTTP cases; the next substantial checkpoint is the 58-case
  `general/300` + `spec/300` group.

### Validation

- Required prior-through check: `make test-report-through-pa21` — **1850/1850
  passed**.
- Required file audit: `perl scripts/cppgm_file_audit.pl --stage pa22 --paths
  dev/src` — **passed** with only the 10 pre-existing warnings.
- Required active report: `make test-report ACTIVE_TEST_REPORT_PAS='pa22'` —
  **94/250 passed**, exactly preserving the checkpoint baseline, with 156
  remaining fixtures and no timeout failures.
- Checkpoint regression set: `make -C pa22 check TEST=...` for the seven
  Checkpoint 71 fixtures — **7/7 passed**.
- Earlier regression probes for the nested-class and dependent type cases —
  **PA18 2/2** and **PA21 1/1** passed; `git diff --check` passed.

## Checkpoint 73 Audit

### Scope Reviewed

Reviewed the latest `Checkpoint Scope` and result in [plan.md](plan.md), the
PA22 contract in [README.md](README.md), commits `0223a90` and `2739cc9`
(including the preceding call-deduction checkpoint), the complete active
report at `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`,
and the changed implementation in the PA18 call-probe, collection,
decltype, substitution, specialization, inference, text-replay, and emit
files.  The audit also covered the new checked source-set entry and the
current `dev/src` file-audit output.

The reviewed scope is the dependent-expression substitution core: conditional
and default-construction `decltype`, expression-SFINAE member/comma and
conversion probes, detected-or and member-typedef probes, stream insertion,
partial-specialization call probes, typed expression-result lookup, lazy
class references, placement-new results, and concrete member-owner routing.

### Findings

- The checkpoint had a real timeout path in hidden-friend SFINAE.  Expression
  type inference attempted binary parsing before function-call parsing, so the
  `<` in a template-id call such as `declval<T>()` could be treated as a
  relational operator.  The malformed operand then re-entered member lookup
  and specialization probing.  The integral evaluator also evaluated both
  operands of top-level `||` and `&&`, recursively materializing a hidden
  friend that C++ short-circuit semantics do not require.
- Those were algorithmic issues, not a test timeout budget problem.  Calls are
  now recognized before binary expressions, and logical integral expressions
  are split with template/parenthesis/bracket depth tracking and evaluated
  left-to-right with semantic short-circuiting.  The bounded scan carries no
  test names, success payload, or fallback-success branch.
- A nonmatching dependent class partial specialization was still reported as
  an internal `logic_error`; it now crosses the candidate boundary as the
  existing typed `PA18SubstitutionFailure` state.  Direct `new void` was also
  accepted by the decltype helper; it is rejected after qualifier
  normalization, while `void()` remains valid for unevaluated
  default-construction probes.
- No compiler phase is skipped.  There is no dummy output, embedded payload,
  interpreter/VM/trampoline substitute, source- or test-specific acceptance
  gate, timeout workaround, weakened check, or unchecked implementation
  fragment.  The fixes use the existing typed declaration/result lookup and
  substitution state; they do not duplicate AST ownership or recover facts
  by reparsing emitted LowIR.  No avoidable full-suite walk or repeated
  emitted-text scan was introduced.
- The two touched large implementation files were kept at their PA22 limits
  (1200 and 1500 lines).  The file audit reports only the same ten pre-existing
  structural warnings; no new warning or unchecked path was used to evade it.

### Changes Made

- Added call-before-binary ordering in `ExpressionTypeSpelling` so template-id
  calls retain their typed result instead of entering relational fallback.
- Added `EvaluateLogicalIntegralText` in the existing checked-in value
  implementation, with a declaration in the expander interface, to preserve
  C++ logical short-circuit evaluation without eager recursive probing.
- Converted partial-specialization pattern mismatch to
  `PA18SubstitutionFailure` at the substitution boundary.
- Rejected `new void` in `EvaluateNewExpression` while preserving the valid
  `void()` unevaluated expression case, and kept all changes within the file
  audit limits.

### Validation

- Required prior-through check — **passed**: `make test-report-through-pa21`
  reported **1850/1850**.
- Required active report — **107/250 passed**, up from the turn-start
  baseline of **106/250**, with **143** exact remaining failures recorded in
  the new plan map and **0 timeout failures**.
- Checkpoint timeout regression — **passed**:
  `make -C pa22 check TEST=tests/general/300-hidden-friend-sfinae-use-scope-shadowing.t`.
  The two regression probes for `decltype(T())` and hidden-friend pointer ADL
  also pass.
- Required file audit — **passed**:
  `perl scripts/cppgm_file_audit.pl --stage pa22 --paths dev/src`, with only
  the ten pre-existing warnings.  `git diff --check` also passes.

## Checkpoint 76 Audit — Checkpoint 75 implementation

### Scope Reviewed

Reviewed the latest Checkpoint 75 scope and result in [plan.md](plan.md), the
PA22 contract in [README.md](README.md), commits `ce27caa`, `ac7e9cc`,
`0223a90`, `2739cc9`, `fede37f`, `a19082c`, and `a0f870`, the complete active
report at `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`,
and the changed PA18/PA14 implementation.  The review covered the new
dependent-alias and using-declaration lookup, alias-owner qualification,
variadic class-temporary lowering, partial-specialization replay, candidate
failure ownership, hot-path lookup work, file limits, and PA1–PA21
preservation.

### Findings

- The landed checkpoint added broad `logic_error` catches around partial
  specialization and nested-member matching.  Those catches converted hard
  semantic failures into candidate rejection and could create fallback success.
  They were removed from the checkpoint paths.  Genuine substitution mismatch
  at template-template argument formation now uses
  `PA18SubstitutionFailure`; unrelated `logic_error` failures propagate.
- Explicit-using lookup stored qualified target spellings and repeatedly
  reconstructed declaration identity from strings.  The source spelling is
  now only a collection-time input: after all declarations are collected, it
  resolves once into a non-owning index of `TemplateDefinition` pointers.
  `definitions_` remains the sole owner, and lookup compares typed declaration
  names without duplicating AST or declaration ownership.
- Alias qualification used a short-name/class-context fallback that could
  qualify an unrelated declaration.  It now qualifies only when the exact
  owner-qualified declaration is present in the typed definition or class
  declaration indexes.  The PA19 static-constexpr regression exposed by the
  first version was fixed before validation was accepted.
- The typed import index is built once per collection and the hot lookup path
  walks only enclosing scopes and their bounded imported candidates.  No
  full-suite walk, repeated emitted-LowIR parse, avoidable timeout cap, or
  quadratic fallback was introduced.
- No compiler phase is skipped.  There is no dummy output, embedded payload,
  interpreter/VM/trampoline substitute, source- or test-specific acceptance
  gate, timeout workaround, weakened check, or unchecked implementation
  fragment.  The final file audit remains within the PA22 limits; its ten
  warnings are the same pre-existing header-division/complexity warnings.

### Changes Made

- Removed all six checkpoint-added broad `logic_error` candidate catches and
  retained only the typed substitution-failure boundary.
- Classified template-template argument mismatch as
  `PA18SubstitutionFailure` so candidate dropping is explicit and hard errors
  are not hidden.
- Replaced `using_declaration_exports_` string recovery with the post-collection
  `using_declaration_targets_` typed, non-owning declaration index.
- Made alias-owner qualification use exact typed declaration indexes, avoiding
  short-name recovery while preserving the existing namespace-relative alias
  rule.
- Kept all changes in the checked source set; no tests or reference fixtures
  were changed.

### Validation

- Required prior-through check — **passed**:
  `n=22; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`
  reported **1850/1850** through PA21.
- Required active report — **passed for preservation**:
  `make test-report ACTIVE_TEST_REPORT_PAS='pa22'` remained at
  **110/250**, matching the checkpoint baseline after the landed increment,
  with no timeout.  The complete 140-fixture residual is recorded in the
  refreshed plan map.
- Required file audit — **passed**:
  `perl scripts/cppgm_file_audit.pl --stage pa22 --paths dev/src`, with only
  the ten pre-existing warnings.
- Regression check — **passed**:
  `make -C pa19 check TEST=tests/general/100-static-constexpr-member-call-initializer.t`.
- `make build` and `git diff --check` pass.  The final commit will be checked
  for an empty `git status --short`.

## Checkpoint 87 Audit — 2026-07-26

### Scope Reviewed

Reviewed the latest `Checkpoint Scope` and result in [plan.md](plan.md), the
PA22 contract in [README.md](README.md), commits `d5d69a5`, `e01518f`,
`1bbbcb4`, and `10fa1e9`, the current report at
`/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`, and
the changed deduction/replay implementation in
`pa18_templates_rewrite_infer.cpp`, `pa18_templates_rewrite_decltype.h`, and
`pa18_templates_rewrite_decltype.cpp`.  The review also covered the checked
frontend source set, the PA22 general/100 and general/200 regression band,
the full PA22 report, the PA22 README boundary, and the complete PA1–PA21
through-report.

The audit focused on the typed pack/forwarding replay checkpoint and its
candidate-admission behavior, including the reported
`template-array-reference-cv-default-arg` timeout.

### Findings

- The checkpoint had one real semantic and performance blocker.  Its new
  fixed-parameter fallback treated every pair of known class types as a
  viable candidate after `MatchTypePattern` failed.  In the array-reference
  fixture this admitted the unrelated global `operator<<` for a stream
  operand, then recursively instantiated another lazy wrapper around the
  generated helper type until the test timed out.  This was an algorithmic
  candidate-admission error, not a timeout-budget problem.
- The shared object-type normalizer also removed the reference transport
  before removing a trailing cv qualifier, so `const T&` could become `T
  const` and evade typed class lookup.  That made the broad fallback harder
  to reject and also affected qualified same-class arguments and legitimate
  conversion-operator calls.
- No skipped compiler phase, dummy output, embedded payload,
  interpreter/VM/trampoline substitute, source- or test-specific acceptance
  gate, emitted-text execution, weakened check, or unchecked implementation
  fragment was found in the reviewed checkpoint.  The fix keeps candidate
  viability in the typed deduction path; it does not add a success fallback
  for the affected fixture.
- The compatibility work uses `TemplateDefinition`/AST declaration indexes
  and the existing typed class-conversion members.  It does not duplicate AST
  ownership, reconstruct semantic facts from emitted LowIR, or walk the test
  suite.  The conversion scan is limited to the one mismatched class
  declaration, and the timeout-causing unbounded re-entry is removed before
  instantiation.
- The first corrected implementation enlarged
  `pa18_templates_rewrite_decltype.h` past the PA22 file-audit limit.  That
  ownership issue was fixed by moving the non-template compatibility helpers
  into the responsibility-named `.cpp` module.  The final audit reports only
  the repository’s existing 11 non-fatal structural warnings.

### Changes Made

- Replaced the unconditional fixed-known-class success path in
  `MergeInferredFunctionArgument` with `FunctionArgumentViable`, so unrelated
  fixed class operands are rejected before replay while arithmetic conversions
  remain viable.
- Corrected object-type normalization after reference removal, qualified
  same-class identity checks through the typed resolver/declaration index, and
  class conversion-operator matching for legitimate user-defined conversions.
- Moved `FunctionArgumentObjectType`, `HasClassConversion`, and
  `FunctionArgumentViable` implementations from the header to
  `pa18_templates_rewrite_decltype.cpp`, restoring file-audit ownership and
  size health without changing the frontend source set.
- Did not modify tests or reference fixtures.

### Validation

- Required active report — **154/250 passed**, up from the audit-start
  baseline of **153/250**; the complete residual set is **96** fixtures,
  consisting of the three general/100 parity cases, one general/200 parity
  case, and the previously reported downstream groups.  There are **0
  timeout failures**.
- The timeout regression fixture
  `general/100-template-array-reference-cv-default-arg.t` passes its full
  LowIR comparison, and the forwarding-reference top-const function-pointer
  regression passes its full comparison.  The explicit user-conversion case
  remains a semantic success; its current residual is LowIR parity only.
- Required prior-through check — **passed**:
  `n=22; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`
  reported **1850/1850** through PA21.
- Required file audit — **passed**:
  `perl scripts/cppgm_file_audit.pl --stage pa22 --paths dev/src`, with only
  the 11 pre-existing warnings.
- `git diff --check` passed, and the cohesive audit fixes plus documentation
  are committed with an empty `git status --short`.

## Checkpoint 91 Audit — 2026-07-26

### Scope Reviewed

Reviewed the latest `Checkpoint Scope` and result in [plan.md](plan.md), the
PA22 contract in [README.md](README.md), commits `b727079`, `5af6c7b`,
`a966998`, and `4b20268`, the complete report at
`/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`, and
the changed lookup, deduction, replay, and candidate-viability sources:
`pa18_templates_collection.{h,cpp}`, `pa18_templates_calls_qualified.cpp`,
`pa18_templates_rewrite.{h,cpp}`, and `pa18_templates_rewrite_infer.cpp`.
The review included the three Checkpoint 91 fixtures, the complete PA22
failure set, the PA22 file audit, and the PA1–PA21 through-report.

The audit specifically traced using-directive export ownership, using-imported
member visibility, deferred member-typedef function-pointer deduction,
selected function-template argument materialization, implicit-object cv
viability, and the checkpoint's hot-path candidate probes.

### Findings

- The checkpoint's namespace using-directive index stored declaration identity
  as `(visible-name, map-key-string)` pairs and recovered the declaration from
  `definitions_` during replay.  That duplicated ownership information and
  contradicted the checkpoint's typed-lookup design.  The member-template
  promotion gate was also a process-wide name set, so an unrelated class with
  the same member spelling could be promoted as an imported template.
- `ResolveFunctionArguments` correctly recognized an explicit `&foo` operand,
  but wrote the selected specialization through the outer unary node.  That
  could erase the address-of operator while materializing a selected function
  template argument.
- `CompleteFunctionArguments` performed `FunctionExpressionTypes` even after
  a typed expected-signature match had already succeeded.  This was avoidable
  replay work on a deduction hot path and could force an unnecessary second
  overload walk.
- The ordinary-member probe recursively rewrites dependent base names before
  deciding viability.  A substitution or semantic failure in that probe must
  reject only that ordinary candidate; allowing it to escape could turn a
  lookup probe into a hard failure.
- No skipped compiler phase, dummy output, embedded payload,
  interpreter/VM/trampoline substitute, source- or test-specific acceptance
  gate, timeout workaround, emitted-text execution, weakened check, or
  unchecked implementation fragment was found.  The checkpoint still uses
  the normal typed deduction, substitution, overload, and LowIR paths.  The
  residual fixtures are semantic parity work outside this checkpoint, not
  deferred checkpoint blockers.

### Changes Made

- Changed using-directive exports to non-owning `TemplateDefinition*` entries
  owned solely by `definitions_`; indexing now receives the inserted typed
  definition directly, and replay consumes its qualified name without key
  recovery.
- Replaced the global using-member name set with a scope-indexed typed import
  table resolved after collection, matching imported member templates by
  owner and retaining declaration identity through replay.
- Materialized a selected function template into the referenced identifier
  child, preserving an enclosing unary address-of node.
- Avoided `FunctionExpressionTypes` when typed expected-signature matching has
  already established the deferred deduction, and made dependent base probing
  convert substitution/semantic failures into ordinary-candidate rejection.
- Kept all implementation changes in the checked `dev/src` source set; no
  tests or reference fixtures were changed.

### Validation

- Required active report — **172/250 passed**, with **78** residual fixtures
  and **0** timeout failures.  The report command exits nonzero because those
  residual PA22 fixtures are expected at this checkpoint; the count is equal
  to the turn-start baseline, so stage-progress preservation passes.
- All three Checkpoint 91 fixtures pass in exit-status and LowIR modes:
  `general/300-using-declaration-imports-member-template-sfinae-shadow`,
  `general/300-using-directive-overloaded-function-template-arg`, and
  `general/300-using-member-template-implicit-object-cv-overload`.
- Required prior-through check — **passed**:
  `n=22; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`
  reported **1850/1850** through PA21.
- Required file audit — **passed**:
  `perl scripts/cppgm_file_audit.pl --stage pa22 --paths dev/src`, with only
  the repository's existing 11 structural warnings.
- `make build` and `git diff --check` pass.  The final commit will verify an
  empty `git status --short`.

## Checkpoint 92 Audit — 2026-07-26

### Scope Reviewed

Reviewed the latest Checkpoint 92 scope and result in `pa22/plan.md`, the
assignment contract in `pa22/README.md`, the complete current-PA log at
`/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`, and the
recent implementation history from `2581989` through the preceding PA22
checkpoint and audit commits.  The changed implementation surface reviewed
was `dev/frontend_source_sets.mk`, the PA11 alias-layout pass, PA14 lowering
collection/driver state and emission pipeline, and the PA18 abstract-parameter,
type-qualification, class-path, constructor-candidate, decltype-candidate,
instantiation, and generated-type emission sources.  The review covered all
13 Checkpoint 92 fixtures, the complete PA22 failure set, the through-PA21
suite, and the PA22 file audit.

### Findings

- The checkpoint's abstract-parameter work duplicated recursive class
  declaration traversal in two hot paths and searched every stored declaration
  by raw spelling.  That duplicated semantic ownership and made abstract-array
  checks scale with the entire collected class map.  It was replaced with one
  typed abstract-class predicate and one normalized object-spelling caller.
- Generated-type lookup still had avoidable full-map work: PA14 rescanned all
  class types for each new/call/declaration use, and PA18 rescanned all
  specializations while reconciling generated identities.  The checkpoint also
  needed a generic namespace recovery path after a dependent replay lost its
  prefix.  These are now declaration indexes with lexical lookup first,
  qualified suffix recovery only when the indexed path is unambiguous, and
  malformed repeated-owner paths are rejected rather than rebound.
- Candidate viability had broad exception fallbacks.  Constructor materializing
  and decltype candidate probes could treat unrelated semantic failures as
  candidate-local recovery.  They now continue only for the typed
  `PA18SubstitutionFailure` path, and constructor materialization returns only
  after a candidate actually succeeds.
- The PA14 lowering driver concentrated the complete lowering pipeline in one
  oversized function.  Its phase order is now represented by bounded helpers
  for preparation, root frontiers, member fixed points, and final emission;
  the refactor preserves the earlier demand order and exposes no skipped
  phase.
- The early PA11 source-alias decision used a process-wide short-name set and
  raw substring detection of `typedef`.  It now uses an AST-kind-checked,
  scope-qualified alias-path pass in a responsibility-sized source module.
- No skipped compiler phase, fallback-success path, dummy output, embedded
  payload, interpreter/VM/trampoline substitute, source/test-specific
  acceptance gate, timeout workaround, emitted-text reparsing, weakened check,
  hidden implementation fragment, or unchecked source path was found.  No
  checkpoint-level architecture or performance blocker remains.

### Changes Made

- Added `pa11_semantics_alias_gate.cpp` and registered it in the checked C++
  frontend source set; narrowed early layout gating to scoped alias facts.
- Added typed class-use indexing to PA14 and split its lowering driver into
  responsibility-sized phase helpers without changing the established phase
  order.
- Added `RememberClassPath` and indexed generated/nested declaration paths in
  PA18; replaced the literal namespace/type recovery cases with generic typed
  lexical and indexed lookup, including safeguards for repeated generated
  owners.  Replaced the specialization identity scan with the existing
  specialization-name index.
- Centralized abstract-class/object detection, removed broad candidate catches,
  and kept constructor/decltype substitution failures candidate-local.
- Kept all fixes in `dev/` and `dev/src/`; no tests or reference fixtures were
  modified.

### Validation

- Required current-PA report — **178/250 passed**, equal to the audit-start
  baseline of **178/250** and above the Checkpoint 92 landing baseline of
  172/250.  The complete residual is 53 exit-status and 19 LowIR-parity
  failures, with no timeout; the report exits nonzero only because those
  residual fixtures remain.
- All 13 Checkpoint 92 fixtures pass: **13/13**.
- Required prior-through command passed: **1850/1850** through PA21.
- Required file audit passed:
  `perl scripts/cppgm_file_audit.pl --stage pa22 --paths dev/src`, with only
  the repository's existing 11 warnings.
- `make build` and `git diff --check` pass.  The refreshed exact failure map
  and the next substantial group, general/400 typed aliases, constructors,
  conversions, and NTTPs, are recorded in `pa22/plan.md`.

## Checkpoint 98 Audit — Checkpoint 97 implementation

### Scope Reviewed

Reviewed the latest Checkpoint 97 scope in [plan.md](plan.md), the PA22
contract in [README.md](README.md), the primary checkpoint log at
`/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`, and
recent commits `dbc9614`, `aa42352`, `94b49df`, `309be2e`, and `dc21ac9`.
The changed implementation reviewed was
`dev/src/pa18_templates_calls.cpp`, `dev/src/pa18_templates_calls_qualified.cpp`,
`dev/src/pa18_templates_collection.h`, `dev/src/pa18_templates_rewrite.cpp`,
`dev/src/pa18_templates_rewrite.h`,
`dev/src/pa18_templates_rewrite_constructor.cpp`,
`dev/src/pa18_templates_rewrite_specialization.cpp`, and
`dev/src/pa18_templates_rewrite_text.cpp`.

The audit traced constructor/member-template replay, dependent default
substitution, inherited-constructor materialization, nested-type deduction,
ordinary-member candidate viability, hot-path lookup work, file-audit health,
and preservation of PA1–PA21.  It also reviewed the complete current-PA
failure set rather than only the checkpoint fixtures.

### Findings

- A dependent replacement whose template-id still contained the substituted
  parameter could recursively re-enter `RewriteText` without reaching a
  semantic fixed point.  The resulting failure was a genuine algorithmic
  recursion bug, not a timeout-budget problem.  The rewrite boundary now
  protects only the self-containing template-id binding while retaining the
  typed substitution map for all other arguments.
- Special-member constructor replay used the mem-initializer spelling as the
  lookup name even when that spelling was a data member.  That dropped the
  typed member class and could omit a nested member-template constructor.
  Replay now derives the constructor owner from the resolved member type and
  its specialization base identity.
- Inherited constructor template declaration replay could materialize its own
  initializer while the concrete special-member declaration was still being
  transformed.  This caused recursive re-entry and the reported timeout.  A
  scoped declaration-depth fact defers only that reentrant materialization;
  it does not cap execution time or convert failure into success.
- Ordinary member lookup and constructor replay now have an explicit semantic
  boundary: special-member definitions are considered by the ordinary-call
  viability probe only when the caller marks constructor replay.  The normal
  overload path is not made to recover constructor facts from generated text.
- A concrete nested type that was already represented by a qualified owner
  could be compared against a bare nested spelling during deduction.  The
  matcher now resolves that identity through the owner declaration index.
  The attempted concrete-member lookup mode that regressed broad SFINAE was
  rejected and reverted; the retained path preserves the earlier lookup
  contract.
- No skipped compiler phase, fallback-success path, dummy output, embedded
  payload, interpreter/VM/trampoline substitute, source- or test-specific
  acceptance gate, emitted-text execution, weakened check, hidden
  implementation fragment, or file-audit bypass was found.  The recursion
  guards are semantic cycle guards, not timeout workarounds.  No full-suite
  walk, emitted-LowIR reparse, duplicated AST ownership, or hot-path
  unbounded scan was introduced.

### Changes Made

- Added a bounded self-containing-template-id substitution guard at the text
  rewrite boundary.
- Added typed member-class constructor-name recovery and threaded an explicit
  `constructor_replay` flag through member-call viability.
- Added scoped concrete special-member declaration replay depth so initializer
  constructor materialization cannot recursively re-enter its declaration.
- Added generated-owner/nested-class identity normalization through the typed
  class declaration index.
- Kept all implementation changes under `dev/src`; no tests or reference
  fixtures were changed.  The rejected concrete nested lookup mode was
  removed after its full-report regression was confirmed.

### Validation

- Required active report — **passed for stage progress**:
  `make test-report ACTIVE_TEST_REPORT_PAS='pa22'` reported **200/250**,
  above the checkpoint baseline of **197/250**, with **50** residual
  current-PA fixtures and **0 timeout failures**.
- Required prior-through check — **passed**:
  `n=22; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`
  reported **1850/1850** through PA21.
- Required file audit — **passed**:
  `perl scripts/cppgm_file_audit.pl --stage pa22 --paths dev/src`.
- The formerly recursive/defaulted nested fixture, dependent forwarding
  fixture, and forwarding-lvalue overload fixture complete without timeout;
  the inherited-constructor fixture completes in **0.02s** rather than
  timing out.  Its remaining result is a LowIR parity failure recorded in the
  refreshed work map, not an execution or architecture shortcut.
- `git diff --check` passes; the final commit will leave `git status --short`
  empty.
