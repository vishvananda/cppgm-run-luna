# PA8 Final Stage Audit

## Audit Plan

The final audit covered the checkpoint that completed PA8 (`a524e5d`) and the
integrated stage after cleanup:

1. Re-read `AGENTS.md`, `TESTING_AND_REFERENCES.md`, `pa8/README.md`, the
   complete `pa8/plan.md`, all PA8 local tests and multi-translation-unit
   fixtures, and the recorded primary report at
   `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.
2. Compared the PA8 checkpoint with the preceding PA7 audit/implementation,
   reviewed the complete `dev/nsinit.cpp`, `nsinit_parser.cpp`, literal and
   image modules, semantic model, and `dev/frontend_source_sets.mk`, and
   checked the new model source dependency.
3. Traced preprocessing, post-token validation, declaration parsing, typed
   lookup/linkage, initialization and constant evaluation, entity/temporary/
   string ordering, alignment, relocation, and binary output against the
   handout's three image blocks.
4. Audited ownership and mutation boundaries, source-set completeness,
   repeated lookup work, output behavior for translation units without `main`,
   and possible reference-binary, host-tool, fixture, subprocess, or
   test-specific shortcuts. The no-`main` probe used a checked-in PA8
   translation unit containing a defined variable and verified that no
   implicit function is emitted after cleanup.
5. Ran the PA8 report, the through-PA8 report, the required source file audit,
   whitespace/diff checks, and final worktree/commit checks.

## Findings

- The checkpoint delivered the intended PA8 fixture behavior: 41/41 PA8
  tests and 292/292 through PA7 were recorded green, and the full primary
  report records 333/333 tests across all eight stages. However,
  `pa8/audit.md` was missing and `pa8/plan.md` stopped before the required
  architecture reviews.
- `nsinit_model.h` was a 769-line implementation-heavy header. Its inline
  factories, type predicates, lookup graph, ownership operations, and
  declaration merging made the semantic boundary less explicit and caused a
  new non-fatal `bad-division` warning in the PA8 file audit.
- `nsinit_image.cpp` created a synthetic `__cppgm_entry` function whenever no
  source declared `main`. That is outside the PA8 output contract, which
  emits declared functions and defined variables only. A checked-in
  variable-only translation unit exposed the extra `fun` bytes.
- The image writer assigned offsets through `const_cast<Entity&>`, although
  `Program` owns mutable entities. This was safe for the checkpoint tests but
  weakened the ownership/mutation contract.
- The semantic path otherwise follows the intended staged architecture:
  PA5 preprocessing and PA2 validation are reused, command-line translation
  unit order is retained, typed entities drive linkage and image layout, and
  relocations are applied only after final offsets exist. No production path
  reads expected/reference data, invokes a host compiler or subprocess, or
  branches on test names.
- The remaining file-audit warning is the already documented,
  declaration-heavy `dev/src/recog_parser_internal.h` warning from earlier
  stages. The new PA8 model-header warning is removed.

## Changes Made

- Moved all PA8 model and `Program` implementation bodies into the new
  `dev/src/nsinit_model.cpp`; retained the typed data contract and method
  declarations in `nsinit_model.h`; and added `nsinit_model` to the explicit
  `nsinit` source set.
- Removed the implicit `__cppgm_entry` generation path. The image builder
  now emits exactly the model's eligible defined variables and declared
  functions, followed by temporaries and strings.
- Changed image emission and relocation helpers to use explicit mutable or
  non-mutating entity pointers, eliminating the `const_cast` offset update.
- Added the Architecture Review and Final Architecture Review to
  `pa8/plan.md`, and added this final stage audit. No tests or reference
  fixtures were edited.

## Validation

- `make test-pa8` — PASS, PA8 local/course **41/41**.
- `perl scripts/cppgm_file_audit.pl --stage pa8 --paths dev/src` — PASS with
  one pre-existing non-fatal warning for
  `dev/src/recog_parser_internal.h`; no PA8 model-header warning remains.
- `make test-report-through-pa8` — PASS, **333/333** tests across **8/8**
  stages. During the audit, a transient host-load run exceeded the default
  text-test budget on the 12 MB PA3 triple-expression fixture; the isolated
  fixture and a serialized retry with the existing runner controls also
  passed.
- `git diff --check` — PASS. The final cleanup is committed and
  `git status --short` is empty.
