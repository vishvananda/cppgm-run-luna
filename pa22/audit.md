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
