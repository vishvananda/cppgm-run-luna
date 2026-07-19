# PA6 final stage audit

## Audit Plan

- Re-read `TESTING_AND_REFERENCES.md`, `pa6/README.md`, `pa6/pa6.gram`, and
  the existing `pa6/plan.md` against the implementation contract.
- Review the PA6 checkpoint commit (`c35defc`), the preceding PA5 stage
  commit, all changed recognizer and frontend source files, and the recorded
  checkpoint results.
- Trace the complete `recog` pipeline for source opening, preprocessing,
  post-token validation, normalization, mock-name facts, angle splitting,
  recursive-descent parsing, and per-file error handling.
- Check ownership, source-set wiring, speculative-parser state restoration,
  repeated parsing work, reference/host-tool shortcuts, and the file-audit
  warning before running the required gates.

## Findings

1. The checkpoint recognizer read a source file directly and called the
   post-token lexer, while PA6 requires the PA5 preprocessing behavior.  Macro,
   include, conditional, pragma, and line-directive inputs could therefore
   diverge from `preproc`.
2. The checkpoint did not run the PA5 post-token semantic validation before
   parsing.  Invalid literal spellings and invalid post-token kinds could
   reach the grammar layer.
3. Parser state helpers and the constructor were executable inline bodies in
   `recog_parser_internal.h`.  That made ownership less clear and caused the
   source audit's header-body warning.  After moving the bodies to
   `recog_parser.cpp`, the remaining warning is a heuristic false positive on
   the declaration-heavy class interface: the header contains declarations,
   not implementation bodies.
4. Grammar review found several correctness edges outside the original
   checked-in fixtures: alternative operator keywords, ordinary versus
   prefixed empty strings, cast operands after a close-angle, lambda default
   captures, qualified enum/class heads, trailing-return declarators,
   anonymous bit-fields, exception type-id ellipses, and repeated attribute
   specifiers.  The old speculative ordinary-`for` branch was also dead and
   misleading.
5. The output stream was not checked when opened.  A failed output file is a
   process-level error, while source-file failures must remain isolated as
   `BAD` results.

No reference binary, host compiler, test-specific answer, or external process
is used by the implementation.  The recognizer keeps PA6 mock lookup local to
typed token facts, as required; it does not introduce a premature symbol-table
dependency.

## Changes Made

- Extracted the PA5 `Preprocessor` implementation into the shared
  `dev/src/preprocessor_engine.cpp` / `.h` module and linked it into both
  `preproc` and `recog` in `dev/frontend_source_sets.mk`.
- Added `ValidatePostTokens` to the post-token semantic API.  `recog` now
  preprocesses, validates, normalizes, and parses each source through one
  explicit pipeline.  Validation uses a discard-only stream buffer rather than
  materializing a duplicate presentation report.
- Hardened `recog.cpp` output-file handling and removed unused PA6 helper
  functions.
- Moved parser state/helper definitions from the internal header into the
  parser implementation and kept the parser split into expression,
  statement, and declaration modules.
- Corrected the reviewed grammar cases listed above, including alternative
  operators, angle-depth transitions, lambda captures, qualified heads,
  declarator disambiguation, anonymous bit-fields, exception ellipses, and
  repeated attributes.
- Updated `pa6/plan.md` with the actual architecture and final architecture
  review.

## Validation

- `make -C dev recog preproc`: passed.
- `make -C pa6 test`: **43/43 passed** (32 local and 11 course tests).
- `perl scripts/cppgm_file_audit.pl --stage pa6 --paths dev/src`: **passed**;
  it reports one non-fatal declaration-density warning for
  `recog_parser_internal.h`, documented above.
- `make test-report-through-pa6`: **267/267 passed** across all six stages.
- Focused smoke checks covered macro expansion, invalid post-token rejection,
  repeated attributes, alternative operators, and per-file `BAD` handling.
