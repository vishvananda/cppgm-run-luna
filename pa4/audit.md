# PA4 Final Stage Audit

## Audit Plan

The final audit covered both the PA4 checkpoint commit `9cc4692` and the
integrated stage after cleanup:

1. Read `AGENTS.md`, `TESTING_AND_REFERENCES.md`, `pa4/README.md`,
   `pa4/plan.md`, all PA4 local/course tests, the earlier PA1--PA3 test
   inputs covered by the through target, and the full checkpoint log at
   `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.
2. Reviewed the recent PA1--PA3 final-stage commits and audits, the PA4
   checkpoint diff, `dev/macro.cpp`, `dev/src/macro_engine.cpp` and its
   header, source-set wiring, shared phase-1--3 translation, and PA2
   posttoken lexer/semantics that consume the final stream.
3. Audited directive recognition and logical-line boundaries, definition and
   redefinition validation, raw/expanded argument ownership, stringizing,
   token pasting and placemarkers, blue-painted recursion, retokenization,
   PA2 token-boundary compatibility, error propagation, ownership,
   performance, and shortcut risks.
4. Probed behavior adjacent to the fixtures, including direct recursive
   function macros followed by source parentheses, the PA4 tail-helper
   recursion, hexadecimal `p/P` preprocessing numbers, `<::`, generated
   directives, and a 50,000-token object-like macro workload.
5. Ran the file audit, PA4 local/course tests, the through-PA4 report, and
   `git diff --check`; then verified the cohesive commit and empty worktree.

## Findings

- The checkpoint had completed PA4 behavior at 38/38 local and 31/31 course
  tests, but the stage had no `pa4/audit.md` and `pa4/plan.md` stopped at the
  checkpoint result without the required architecture reviews.
- The checkpoint rescanned by erasing and inserting into the middle of one
  `vector<Token>` for every expansion. A 50,000-token probe exceeded 23
  seconds before interruption and reached about 45 MB RSS. Its line processor
  also recomputed the end of the same logical line from every token, creating
  a second quadratic path on long text lines.
- The checkpoint's blue-paint exception could re-invoke a directly recursive
  function macro when a source `(` followed its replacement token. For
  example, `#define f(x) f` followed by `f(1)()` incorrectly consumed the
  source call instead of preserving `f()`.
- The PA4 lexer did not match the established PA2 pp-number `p/P` exponent
  rule or the `<::` longest-match exception. The shared raw-span scanner had
  the same `p/P` omission, so a raw-looking sequence after a pp-number could
  be discovered at the wrong boundary.
- The integrated implementation otherwise matches the assignment contract:
  phase-1--3 translation is shared, only source-level `#define`/`#undef`
  lines change macro state, expansion is rescanned in process, and the final
  tokens use PA2 semantics. The tail-helper fixture demonstrates that nested
  replacement provenance must remain distinct from direct self-recursion.
- No test-name branches, fixture or `.ref` reads, reference-binary calls,
  subprocesses, host compiler calls, generated answers, or test edits were
  found in the production path. The file audit passed before and after the
  cleanup.

## Changes Made

- Added the final `pa4/audit.md` record and grounded `Architecture Review` and
  `Final Architecture Review` sections to `pa4/plan.md`.
- Replaced the whole-text vector rescan with a reverse-loaded `std::deque`
  worklist. Replacement tokens are pushed in reverse order, so rescanning
  remains source-order correct while ordinary expansion uses deque-end
  operations. Reworked `ProcessLines` to advance one logical line per loop,
  removing the repeated line-end search.
- Kept blue-paint state on individual tokens and added non-owning replacement
  provenance. Direct self-recursion stays unavailable, while a nested
  replacement can form the checked PA4 tail-helper invocation with source
  tokens. The owner pointer is used only during the current expansion, while
  the `std::map` macro entries remain stable and value-owned.
- Aligned PA4 preprocessing-token scanning with PA2 for hexadecimal and
  `p/P` exponent spellings and the `<::` exception. Updated the shared raw
  literal scanner to use the same pp-number boundary rule.
- Preserved the existing shared source-set architecture and made no changes
  to tests or reference fixtures.

## Validation

- `make -C pa4 test` — PASS, PA4 local 38/38 and course 31/31.
- `make test-pa4` — PASS.
- `perl scripts/cppgm_file_audit.pl --stage pa4 --paths dev/src` — PASS,
  22 files checked.
- `make test-report-through-pa4` — PASS, 156/156 tests and 4/4 stages; all
  tracked earlier stages remain green.
- `git diff --check` — PASS.
- Supplemental probes passed for direct recursive-tail preservation, the
  `FILLER_0`/`FILLER_1` tail-helper chain, `0x1p+2` macro output, `<::`
  tokenization, and generated `#` text not becoming a directive. The final
  50,000-token object-like macro probe completed in about 0.25 seconds with
  linear worklist/line processing.
- The supplied primary log records the clean 156/156 through-PA4 report, and
  the independent final rerun reproduced it. The cohesive cleanup is
  committed and `git status --short` is empty.
