# PA18 checkpoint audit

## Scope Reviewed

Reviewed the latest checkpoint at `fa85b18` and its PA18 predecessors
`28b92cd`, `2f110cb`, `4945c13`, and `9c1c459`, including the checkpoint scope
and remaining-work map in `pa18/plan.md`.  The review covered the PA18
contract in `pa18/README.md`, the changed template-expansion and lowering
sources, the PA14 inference-cache lifecycle, the complete 222-test PA18
report, the through-PA17 report, and the `dev/src` file audit.

## Findings

The checkpoint preserves the earlier PA1–PA17 implementation boundary and
does not shell out, read fixtures, embed a runtime or payload, emit dummy
output, skip a compiler phase, or use a test/source-name acceptance gate.  The
PA18 path still materializes ordinary AST declarations before PA14 lowering;
it does not replace lowering with an interpreter, VM, trampoline, or timeout
path.  The inference cache is cleared at each function boundary and owns its
temporary node identity only for that bounded lifetime.

The audit did find checkpoint-level semantic and structural issues.  A
tautological specialization branch and an empty lookup condition obscured the
real path.  Function-argument arity was duplicated between call and decltype
lookup.  Binary `-` deduction used an unconditional string fact of `int`,
which could mis-deduce overloaded operators.  Generalizing that inference
briefly exposed the header-size/function-size audit gates; the implementation
was moved to the owning `.cpp` module before validation completed.

The file audit finishes with its seven pre-existing implementation-header
warnings and no fatal issue.  All changed implementation remains in tracked,
included, audited source paths; there is no include-as-code split or weakened
audit check.

## Changes Made

- Removed the tautological specialization branch and dead lookup block.
- Added one arity-counting helper and reused it for direct calls and decltype
  result recovery, including default arguments.
- Added ordinary member/free operator-result matching, template-operator
  result inference, and builtin arithmetic promotion for deduction instead of
  the unconditional `int` fallback.
- Moved call-expression and binary/operator inference bodies into
  `dev/src/pa18_templates.cpp`, keeping declarations in the template-expander
  interface; indexed ordinary function signatures by unqualified name to
  avoid repeated full-map scans.

## Validation

- `make test-report ACTIVE_TEST_REPORT_PAS='pa18'`: checkpoint report remains
  **174/222**, equal to the post-checkpoint baseline and above the turn-start
  baseline of 169/222; the expected 48 current-PA failures remain grouped in
  `pa18/plan.md`.
- `n=18; ... make test-report-through-pa17`: **1208/1208**, pass.
- `perl scripts/cppgm_file_audit.pl --stage pa18 --paths dev/src`: pass with
  seven pre-existing header-division warnings.
- Focused operator/deduction witnesses after the fixes: **3/3 pass**;
  logical-operator and free-operator-result regressions also pass.
