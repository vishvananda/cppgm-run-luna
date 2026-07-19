# PA9 Final Stage Audit

## Audit Plan

The final audit covered checkpoint commit `a6179e3` and the integrated PA9
stage after cleanup:

1. Re-read `AGENTS.md`, `TESTING_AND_REFERENCES.md`, the complete PA9 README,
   `pa9/cy86-opcode.desc`, `pa9/pa9.gram`, the complete `pa9/plan.md`, all
   checked-in PA9 programs and headers, and the primary report at
   `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.
2. Compared the checkpoint with the PA8 audit/architecture handoff and
   reviewed `dev/cy86.cpp`, `dev/frontend_source_sets.mk`, the shared PA5
   preprocessing/token modules, the changed source history, and the PA9
   generated-program harness.
3. Traced preprocessing and translation-unit concatenation, CY86 parsing,
   exact opcode description, label collision and forward-reference handling,
   PA2 literal decoding, operand validation, data/code layout, x86 lowering,
   x87 conversions, syscall ABI mapping, ELF construction, and absolute and
   relative relocation patching.
4. Audited value ownership and relocation lifetime, compiler-temporary
   register/red-zone use, pass structure and repeated work, source-set
   completeness, executable permissions, and possible reference-binary,
   host-tool, subprocess, fixture, test-name, or fake batch-mode shortcuts.
5. Probed adjacent production inputs for malformed opcode families, reserved
   keyword labels, illegal literal offsets, Unicode/PA2 literals, and floating
   data immediates. Ran the required PA9 file audit, PA9 tests, through-PA9
   report, diff checks, and final worktree/commit checks.

## Findings

- The checkpoint delivered the complete checked-in PA9 behavior: the primary
  report records **344/344** tests across **9/9** stages, including **11/11**
  PA9 tests, and the current PA9 suite has no remaining fixture failure.
- The checkpoint plan had no `pa9/audit.md` and stopped before the required
  Architecture Review and Final Architecture Review. Those records are now
  present and describe the actual parser, lowering, layout, relocation, and
  ELF implementation.
- The checkpoint had real semantic gaps outside the narrow fixtures: broad
  prefix-based opcode recognition, keyword labels, invalid literal offsets,
  incomplete PA2 quoted-literal handling, rejection of floating literals in
  data/immediate operands, and rejection of adjacent ordinary string
  literals. These could accept ill-formed CY86 or reject valid PA2 literals,
  so they were corrected in the integrated compiler.
- The implementation reuses the PA5 preprocessing boundary and post-token
  Unicode/token infrastructure, retains command-line source order, and keeps
  CY86 state separate from the PA8 namespace/image model. Parser facts,
  labels, relocations, and emitted bytes are owned by the compiler rather than
  borrowed from temporary token storage.
- The final lowering covers integer widths and aliases, x86 zero-extension,
  control transfer, memory, syscalls, x87 32/64/80-bit operations and
  conversions, and label-based code/data addresses. Data and code are laid
  out in stable passes, then patched only after final addresses exist.
- No production path reads reference output, invokes a host compiler or
  assembler, shells out to a previous solution, dispatches on a fixture/test
  name, or uses the reference binary to produce output. The batch protocol is
  handled outside the real compiler by `test_runner`; the removed PA9-local
  fake batch branch no longer emits `EXIT_NOT_IMPLEMENTED`.
- The required source file audit passes. Its only warning is the known,
  non-fatal heuristic warning for the earlier declaration-only
  `dev/src/recog_parser_internal.h`; no PA9 implementation warning was added.

## Changes Made

- Hardened `DescribeOpcode` with exact family/width matching, exact floating
  conversion spellings, and exact `syscall0`--`syscall6` validation.
- Rejected C++ keyword labels and offset expressions whose primary is not a
  label or memory base register.
- Replaced the incomplete quoted-literal path with PA2-compatible UTF-8,
  escape, raw-string, UTF-16, UTF-32, wide-character, and character-width
  handling, including phase-6 concatenation of adjacent ordinary and
  compatible prefixed strings. Added floating object-byte conversion for
  immediate/data width conversion and correct static-string alignment.
- Made internal-label serial state compiler-owned, removed unused code-index
  and emitter state, and removed the PA9-local fake `--batch-stdin` path.
- Added the Architecture Review and Final Architecture Review to
  `pa9/plan.md`. No tests or `.ref` files were edited.

## Validation

- `make -C dev cy86` — PASS.
- `make test-pa9` — PASS, PA9 local **11/11** and course **0/0**.
- `perl scripts/cppgm_file_audit.pl --stage pa9 --paths dev/src` — PASS with
  the one documented pre-existing `recog_parser_internal.h` warning.
- `make test-report-through-pa9` — PASS, **344/344** tests across **9/9**
  stages; all tracked earlier assignments remain green.
- Supplemental production probes — PASS: malformed opcode-family names,
  malformed conversions/syscalls, C++ keyword labels, and illegal literal
  offsets fail; valid label offsets, Unicode/PA2 literals, adjacent strings,
  and `data32 1.0f` are accepted and encoded.
- `git diff --check` — PASS. The cleanup is committed and `git status --short`
  is empty.
