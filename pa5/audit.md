# PA5 Final Stage Audit

## Audit Plan

The final audit covered the PA5 checkpoint commit `6a9fe10` and the
integrated stage after the cleanup:

1. Read `AGENTS.md`, `TESTING_AND_REFERENCES.md`, `pa5/README.md`, the full
   `pa5/plan.md`, the PA5 local/course test inventory, the earlier PA1--PA4
   audits, and the primary checkpoint report at
   `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.
2. Compared the recent PA1--PA4 stage commits with the PA5 checkpoint diff,
   then reviewed `dev/preproc.cpp`, `dev/src/macro_engine.cpp/.h`,
   `dev/src/ctrlexpr.cpp/.h`, the PA2 post-token boundary, and
   `dev/frontend_source_sets.mk`.
3. Audited logical-line and inactive-section handling, conditional ordering,
   macro-state isolation, provenance for `__FILE__` and `__LINE__`, include
   search and file identity, `#pragma once`, `_Pragma`, `#line`, predefined
   macros, final PA2 invalid-token handling, ownership, performance, and
   shortcut risks.
4. Probed behavior adjacent to the fixtures, including a user definition that
   collides with the `defined`-operand protection name, multiple such
   collisions, pasted `__FILE__`/`__LINE__` tokens with and without
   placemarkers, and a backward `#line 1` transition. The probes were run
   through the production `preproc` binary using `/dev/fd` input/output; no
   reference implementation was used to produce output.
5. Ran the required file audit and through-stage report after the cleanup,
   ran the direct PA5 local/course suite, checked whitespace and source-set
   wiring, and verified the final cohesive commit and worktree state.

## Findings

- The checkpoint delivered the complete PA5 fixture behavior from a 0/68
  baseline. The implementation covers the driver/output pipeline, conditional
  inclusion, PA4 macro replacement, recursive includes, `#pragma once`,
  predefined/source-location macros, `#line`, `_Pragma`, errors, and isolated
  top-level source units.
- The checkpoint had no `pa5/audit.md` and its plan stopped at the checkpoint
  result without grounded Architecture Review and Final Architecture Review
  sections. The final record and architecture sections are now present.
- The checkpoint protected operands of `defined` by rewriting them to a
  generated identifier. A valid user macro with that generated name expanded
  the placeholder before the PA3 evaluator saw it, making a valid controlling
  expression fail. A deeper probe also showed that a different macro could
  produce the same spelling, so name allocation alone was not a complete
  protection boundary. This was a real state-isolation defect not covered by
  the checked-in fixtures.
- The checkpoint computed the `#line` delta through an unsigned expression.
  Backward line changes relied on implementation-defined conversion after
  unsigned underflow, even though ordinary Linux results happened to work.
- PA4 token pasting retokenized a pasted `__FILE__` or `__LINE__` without the
  macro invocation's provenance, so a valid `CAT(__, LINE__)` expansion used
  line 1. This crossed the PA4/PA5 boundary because PA5 makes those builtins
  observable in source-file output.
- The final code preserves the established staged architecture. PA5 consumes
  the shared phase 1--3 translation, the PA4 macro engine, the PA3 typed
  evaluator, and PA2 post-token semantics. Macro replacement is performed in
  process; includes and pragmas mutate explicit per-source state; and final
  invalid-token detection is delegated to the existing PA2 output layer.
- Ownership is explicit and bounded: macro definitions and replacement tokens
  are value-owned by the `MacroState` map, provenance pointers are non-owning
  and limited to one expansion, recursive files and conditional frames use
  STL containers, and post-token output is collected only for the active
  top-level source. The PA4 deque and typed hide sets remain the rescan
  performance boundary; control expressions carry protected operands directly
  instead of using a spelling-based restoration table.
- No test-name branches, fixture/reference reads, reference-binary calls,
  subprocesses, host-compiler calls, generated answers, or test/reference
  edits were found in the production path. The required source audit reports
  no file-ownership or source-set issue.

## Changes Made

- Changed the PA5 control-expression adapter and `MacroState` to carry an
  explicit token-level protection bit for each `defined` operand. The operand
  is now opaque during macro expansion, so neither a same-named macro nor a
  macro replacement that happens to produce a chosen spelling can alter it.
- Added the token-vector expansion entry point needed to preserve that bit
  while still applying the current presumed file and logical line to
  `__FILE__` and `__LINE__` in the controlling expression.
- Changed `#line` delta computation to signed arithmetic so backward line
  controls do not depend on unsigned-underflow conversion.
- Carried the macro invocation's file and logical line through paste
  retokenization, including placemarker results, so pasted predefined macros
  expand with the same provenance as ordinary replacement tokens.
- Added this final audit and the grounded Architecture Review and Final
  Architecture Review sections to `pa5/plan.md`. Corrected the plan's
  through-PA4 checkpoint count to the actual 156/156, 4/4 result while
  retaining the checkpoint's 103/103 active-check record.
- Preserved all earlier assignment implementations, build source sets, tests,
  and reference fixtures. No tests or `.ref` files were changed.

## Validation

- `make -C pa5 test` — PASS, local 62/62 and course 6/6 (68/68).
- `perl scripts/cppgm_file_audit.pl --stage pa5 --paths dev/src` — PASS,
  22 files checked.
- `make test-report-through-pa5` — PASS, 224/224 tests and 5/5 stages; all
  tracked earlier stages remain green.
- Supplemental production probes — PASS for same-name and
  replacement-spelling `defined` collisions (`#if defined FOO` selects the
  active branch), pasted `CAT(__, LINE__)` and `CAT(__, FILE__)` provenance,
  placemarker provenance, and a backward `#line 1` transition (`__LINE__`
  emits literal 1).
- `git diff --check` — PASS. The final cleanup is committed and
  `git status --short` is empty.
