# PA13 Final Stage Audit

## Audit Plan

The final audit covered checkpoint commit `7093941` and the integrated PA13
stage after cleanup:

1. Re-read `TESTING_AND_REFERENCES.md`, the PA13 README, `pa13.gram`,
   `lowir.md`, the complete PA13 plan, the PA12 architecture handoff, and the
   primary report at
   `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.
2. Compared the checkpoint commit, its changed source, `pa13/Makefile`, the
   source-set manifest, recent PA11/PA12/PA13 history, all 90 checked-in
   `tests/spec` inputs and references, and the current integrated worktree.
3. Traced the complete LowIR path: line parsing and typed ownership,
   declaration/definition and alias handling, metadata and structural
   validation, temporary visibility, frame layout, calls and hidden object
   returns, scalar/floating/atomic/control lowering, exception scaffolding,
   startup hooks, and deterministic CY86/data emission.
4. Audited correctness, ownership, alignment, exact-width/tail handling,
   call-stack layout, f80 scratch lifetime, validation complexity, source-set
   ownership, and possible reference-binary, host-tool, subprocess,
   hardcoded-fixture, or test-name shortcuts.
5. Applied the focused cleanup, ran supplemental direct-call, overflow-call,
   narrow-value, and runtime probes, then ran the required PA13 suite, the
   through-PA13 report, the source file audit, diff checks, and final
   worktree/commit checks.

## Findings

- Checkpoint `7093941` completed the PA13 behavior: the active checked-in
  `tests/spec` suite has 90/90 passing cases, including its 25 rejection
  cases. The required integrated report now passes **747/747** tests across
  **13/13** stages. The broader PA13 tree contains 18 later-tool debuginfo
  wrappers; they are outside the PA13 `lowir2cy86` driver contract and are not
  part of `make test-report-through-pa13`.
- The checkpoint plan had no `pa13/audit.md` and stopped before the required
  Architecture Review and Final Architecture Review. Both records are now
  present and describe the actual parser, validator, emitter, ownership, and
  handoff boundaries.
- The checkpoint had several real gaps outside the narrow fixture set:
  bare `thread_local` globals were rejected and bare storage metadata could
  be lost; declared functions were mistaken for indirect callees; overflow
  arguments could overwrite one another or an indirect callee pointer; narrow
  memory/atomic/control operations could select invalid CY86 widths; object
  copies over-read non-8-byte tails; object frame alignment was not honored;
  and f80-returning functions could lack body scratch storage. These risks
  are closed in the integrated adapter.
- Metadata validation now distinguishes top-level symbol metadata from
  call-signature metadata, permits the documented declaration boundary, checks
  atomic order domains and declared global types, rejects unknown index
  metadata and negative raw zero spans, and accepts only the PA13 type family.
  Function symbols are indexed before validation, and temporary definitions
  are carried through one incremental prefix map rather than rescanning or
  copying every earlier block.
- The parser/emitter model owns strings, vectors, maps, types, operands,
  instructions, metadata, and output text by value. No generated output keeps
  a pointer into lexer lines or parser temporaries. Direct object/f80 returns
  use explicit hidden result storage; f80 values retain 16-byte storage with a
  10-byte payload and six zero-padding bytes; bulk copies use exact 8/4/2/1
  byte tails.
- PA13 remains a text adapter over the PA9 CY86 source contract. It does not
  lower C++, emit native objects, link, invoke a host compiler, call a
  reference binary, read `.ref` files, shell out to another solution, branch
  on test names or paths, or edit fixtures. Declarations, aliases, symbol
  metadata, TLS facts, and call-boundary facts remain representable in the
  parsed LowIR model even when they do not produce CY86 definitions.
- The required file audit passes. Its only findings are the two inherited,
  non-fatal header-division warnings for
  `dev/src/pa11_semantics_analyzer.h` and
  `dev/src/recog_parser_internal.h`; no PA13 `dev/src` implementation warning
  was introduced.

## Changes Made

- Hardened `dev/lowir2cy86.cpp` global parsing for the documented bare
  `readonly`/`thread_local` forms and preserved explicit metadata without
  silent overwrite.
- Included declarations in direct-function resolution, separated indirect
  function-pointer storage from overflow arguments, and accounted for hidden
  object/f80 result pointers in register and stack capacity.
- Added legal narrow-width lowering for loads, stores, atomics, unary ops,
  branch/switch selectors, integer returns, and conversions; added signed
  extension where a 64-bit value is required.
- Honored direct-object alignment, copied object parameters and returns with
  exact tails, allocated f80 scratch for f80 body operations, and tightened
  LowIR metadata/type/order/span validation.
- Added the Architecture Review and Final Architecture Review sections to
  `pa13/plan.md` and added this final stage audit. No tests, references,
  grammar, harness, or earlier-assignment source was edited.

## Validation

- `make -C pa13 test` — PASS, PA13 `tests/spec` **90/90** and course
  extension **0/0**.
- `make test-report-through-pa13` — PASS, **747/747** tests across **13/13**
  stages; the complete report is recorded in
  `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.
- `perl scripts/cppgm_file_audit.pl --stage pa13 --paths dev/src` — PASS with
  the two documented inherited warnings.
- Supplemental probes — PASS: a direct call to a declared function and bare
  TLS storage parse successfully; a five-argument indirect call places the
  callee pointer separately and executes with exit value 5; a narrow i8 load
  and sign extension produces valid CY86 and executes with exit value 1.
- `git diff --check` — PASS before the final commit.
- The cohesive final commit contains the implementation and audit artifacts;
  final `git status --short` is empty.
