# PA19 Checkpoint Audit

## Scope Reviewed

This audit covers the typed integral-constant checkpoint recorded in
`pa19/plan.md` and landed in `cab0747` (`Implement PA19 typed integral
constants checkpoint`).  It reviews the PA19 README and tests, the complete
checkpoint scope and residual failure map, the PA18 handoff through the latest
commit, and the primary log at
`/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.

The source review covers `pa19_constants.h`, the PA11 semantic analyzer and
model, PA14 global folding/lowering, and the PA18 collection and rewrite
materialization path.  The audit follows the pipeline from parsed AST, through
template expansion and typed PA11 facts, into the existing PA14/PA17 LowIR
backend.  It also checks ownership, caching, source registration, file-audit
limits, test/ref immutability, and the complete current-PA failure set.

## Findings

- The checkpoint preserves the staged compiler pipeline.  There is no skipped
  compiler phase, dummy LowIR, embedded answer, reference/host-compiler
  invocation, interpreter/VM/trampoline substitute, source/test-name gate, or
  fallback success path.  The PA14 fallback now consumes the analyzer's typed
  semantic result and still fails when that result is unknown.
- The one timeout in the checkpoint log was an actual non-terminating scan:
  unnamed template parameters produced `find("")`, which never advanced.  A
  guard in both dependent-name scans removes the zero-length search; this is an
  algorithmic termination fix, not a timeout allowance.  The affected test now
  completes normally.
- The typed value path had two checkpoint-level ownership problems.  Integral
  type facts were repeatedly reconstructed from a string, and `Binding` and
  `ConstantValue` duplicated the typed fields beside their legacy signed
  projections.  `PA19IntegralValue` now owns a typed `PA19IntegralType`, while
  `ConstantValue.integral` and `Binding.constant_value` own the PA19 facts.
  Earlier lowering fields remain only as compatibility projections.
- Rewritten non-type identifiers no longer get reparsed to recover whether a
  substitution is an integer or boolean.  The active specialization carries
  the typed value into AST materialization.  The remaining expression parser is
  used at the source-expression boundary for template arguments and constant
  initializers, not on emitted LowIR or already-materialized semantic facts.
- A cv-only type difference in class-member rebuilding could drop the static
  `constexpr` member fact; broadening the match by name would regress a
  reference member whose spelling intentionally resolves differently.  The
  final fix compares variable member types while ignoring only top-level cv and
  retains the original type when the underlying type differs.
- No avoidable full-suite walk or unbounded hot-path scan was added.  Existing
  specialization caching and active-recursion guards remain in force; generated
  constant registration visits generated trees only.  The helper extraction
  keeps the implementation within the repository's file and function limits.
- The file audit passes with the same eight warning-only header-division
  findings.  No implementation was hidden, moved to an unchecked path, or
  used to weaken an audit gate.  No tests, grammars, or reference fixtures were
  modified.

No checkpoint-level shortcut, timeout, ownership, performance, regression, or
file-audit blocker remains.  The 80 residual PA19 failures are the complete
behavioral set recorded in `pa19/plan.md` for the next dependent-lookup,
pack, and generated-record implementation groups; they are not being used as
an audit bypass.

## Changes Made

- Added the unnamed-parameter termination guards and the cv-safe member-binding
  match, while retaining earlier member/type behavior.
- Consolidated PA19 constant ownership around `PA19IntegralType` and
  `PA19IntegralValue`, with typed PA11/PA14 handoff and typed specialization
  substitutions.
- Extracted template-argument resolution and instantiation materialization
  helpers so the source remains within file-audit size and function limits.
- Added this checkpoint audit and refreshed `pa19/plan.md` with the post-audit
  result, exact residual failure inventory, and the next Group B checkpoint.

## Validation

- Ten focused Group A direct-value tests: **10/10 PASS**.
- `make test-report ACTIVE_TEST_REPORT_PAS='pa19'`: complete report, **54/134
  passing**; this is at the turn-start checkpoint baseline, with the residual
  80 failures listed in `pa19/plan.md`.  The former timeout completes normally.
- Required prior-through command (`n=19 ... make test-report-through-pa18`):
  **PASS, 1430/1430**.
- `perl scripts/cppgm_file_audit.pl --stage pa19 --paths dev/src`: **PASS**;
  eight warning-only header-division findings.
- `make -C dev cppgm++` and `git diff --check`: **PASS**.
- Final handoff check: `git status --short` is empty after the cohesive audit
  commit.

## Final Stage Audit

### Audit Plan

This final audit covers the PA19 completion checkpoint implemented by
`af5a597` (`Implement PA19 template constants and lowering`) and recorded by
`3b416e6` (`Record complete PA19 checkpoint inventory`).  The earlier typed
constant checkpoint at `cab0747`, together with its audit repair commit
`a93bae4`, is retained as historical evidence rather than treated as the
stage result.  The integrated review starts at the PA18 handoff
`3f6b3fa` and covers the complete PA19 source change set through the current
tree.

The review checked:

1. `AGENTS.md`, `TESTING_AND_REFERENCES.md`, the PA19 README, the PA13 LowIR
   contract, the complete PA19 plan, and the PA18 final audit;
2. all 134 checked-in PA19 tests under `tests/general` and `tests/spec`, with
   representative witnesses for typed non-type arguments, deferred
   `static_assert`, packs, explicit specialization, stale-primary refresh,
   dependent defaults, and weak inline specialization;
3. the parser/AST changes, PA19 constant model, PA11 analyzer and semantic
   model, PA18 collection/rewrite/materialization path, PA14 collection and
   fixed-point lowering path, and `dev/frontend_source_sets.mk`;
4. ownership and pointer stability for AST nodes, semantic types, bindings,
   class-member records, function/global records, and template caches;
5. deterministic declaration and LowIR order, recursion and specialization
   caching, demand-driven emission, shortcut indicators, the required file
   audit, build/diff checks, the through-stage report, and the primary stage
   log at
   `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.

### Findings

The completed PA19 implementation preserves the staged compiler architecture.
The PA10 parser remains the syntax boundary: pack expansions and `sizeof...`
are represented as AST nodes, and template declarations are retained as
source metadata.  `EmitPA14LowIR` invokes `ExpandPA18Templates` before
constructing `PA14Lowerer`; it does not create a PA19-specific output path.

- `PA18TemplateExpander` collects template parameters, defaults, explicit
  specializations, aliases, dependent names, and pack facts.  Its rewrite
  path materializes ordinary declarations and definitions, including
  generated class/member records, before PA11 analysis.  Integral arguments
  are normalized through `ResolveIntegralArgument` and typed substitution
  maps; an already-materialized non-type identifier becomes a literal AST
  value instead of being reparsed from rewritten LowIR text.
- `PA19IntegralType` and `PA19IntegralValue` provide the compile-time value
  model for signedness, width, rank, promotions, conversions, literals,
  characters, arithmetic, comparisons, shifts, bitwise/logical operators,
  conditional expressions, `sizeof`, `alignof`, and `sizeof...`.  The
  `PA19ConstantExpressionParser` is used at the source-expression boundary
  for template arguments and dependent defaults.  `Binding::constant_value`
  and `ConstantValue::integral` own the typed facts; their legacy signed
  fields are compatibility projections for earlier lowering code.
- PA11 evaluates ordinary and generated constant bindings, defers dependent
  assertions until materialization, and preserves static member and array
  definition facts.  PA14 consumes those semantic results for value
  inference, global folding, static-member storage, ABI naming, object
  lifetime, and LowIR emission.  Existing PA14/PA17 call, class, vtable,
  constructor, destructor, and initialization machinery remains the backend
  for PA19-generated declarations.
- Explicit class/function specialization tables, canonical integral
  argument spellings, generated-specialization caches, active-recursion
  guards, and late-specialization refresh logic keep one semantic identity
  per supported specialization.  Pack expansion is handled in declarations,
  calls, bases, initializers, and instantiated bodies; it does not require a
  second evaluator or backend.
- Ownership is stable across the handoff.  `CPPGMAstNodePtr` owns source and
  generated trees, `TypePtr` owns semantic type graphs, scopes own child
  scopes and deque-backed bindings, and PA14 stores function/global records
  in deques.  Class bindings retain an owner plus member index rather than a
  pointer into a relocatable member vector.  Lookup and demand records are
  therefore non-owning views of stable owners, while specialization and
  active-recursion sets bound repeated materialization.
- The lowerer closes its ordinary, hidden-friend, member, constructor,
  destructor, global-initializer, and generated-function demand frontiers in
  fixed-point passes.  The source scan found no new unbounded repository walk,
  reference-binary or host-compiler invocation, timeout-based success path,
  fixture/source-name gate, embedded answer, dummy LowIR, skipped frontend
  phase, or interpreter substitute in the PA19 path.
- Source registration is complete for the structural split: the added
  `pa11_semantics_constants.cpp`, `pa14_lowering_control_globals.cpp`,
  `pa18_templates_collection.cpp`, and `pa18_templates_rewrite.cpp` are all
  listed for `cppgm++` in `dev/frontend_source_sets.mk`.  The file audit
  passes with eight warning-only header-division findings (the established
  PA11/PA14/PA18/parser implementation headers plus `pa19_constants.h`),
  and reports no fatal finding or unregistered implementation source.

No correctness, ownership, performance, architecture, shortcut, or fatal
file-audit blocker remains for the PA19 assignment boundary.  No PA19 tests,
grammars, or reference fixtures were changed.  The residual warning-only
header findings are documented because the repository audit treats them as a
pass and the headers are still part of the established shared implementation
layout.

### Changes Made

- Retained the completed implementation checkpoint at `af5a597`, including
  typed integral constants, dependent/default argument resolution, packs and
  expansions, explicit specialization, generated static-member state, and
  ordinary PA14/PA17 LowIR lowering.
- Consolidated the checkpoint evidence into this final-stage audit, covering
  the completed checkpoint and the integrated PA18-to-PA19 implementation.
- Added architecture and final architecture reviews to `pa19/plan.md`,
  grounded in the actual parser, semantic, template, ownership, and lowering
  units.
- No compiler source, test, grammar, or reference fixture changes were
  needed after the final implementation review.

### Validation

- `make test-pa19` — PASS, **134 / 134** PA19 tests.
- `make test-report-through-pa19` — PASS, **1564 / 1564** tests across
  **19 / 19** stages; the primary log records
  `ALL TESTS PASSED SUCCESSFULLY! (1564 / 1564)`.
- `perl scripts/cppgm_file_audit.pl --stage pa19 --paths dev/src` — PASS;
  eight warning-only header-division findings and no fatal issue.
- `make build` and `git diff --check` — PASS.
- Final verification after the audit commit shows an empty `git status
  --short`.
