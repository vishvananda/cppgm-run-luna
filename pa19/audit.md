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
