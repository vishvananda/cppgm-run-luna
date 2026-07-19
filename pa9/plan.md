# PA9 implementation plan

## Baseline and Remaining Work Map

The turn-start `make test-report ACTIVE_TEST_REPORT_PAS='pa9'` result is
0/11.  Every test fails before a program is produced with implementation exit
status 86 (`NotImplementedException`), so the complete current-PA failure set
groups into one shared missing pipeline:

1. **Front end and semantic model** — preprocess the concatenated source
   units, parse labels, literal statements, opcodes, registers, immediates,
   and memory operands, then resolve labels and validate opcode arity,
   widths, read/write constraints, and name collisions.
2. **Object/lowering and executable emission** — lay out aligned CY86 data and
   executable instructions, lower the supported typed operations to x86-64,
   resolve code/data addresses, and emit a valid executable ELF image with the
   correct entry point and permissions.
3. **Integer/control/data/syscall runtime behavior** — implement the paths
   exercised by noop, exit, hello-world, I/O helpers, hexdump, binary
   calculator, and integer calculator, including calls/returns, memory,
   widths, zero-extension, shifts, arithmetic, comparisons, and Linux
   syscalls.
4. **Floating runtime behavior** — implement 32/64/80-bit values,
   conversions, arithmetic, and comparisons for the conversion and float
   calculator tests.

## Checkpoint Scope

This checkpoint covers groups 1–3 as one coherent increment: a real CY86
front end with typed operands/instructions and label layout, x86-64 lowering
and relocatable ELF emission, plus the integer/control/data/syscall semantics
needed by the first eight PA9 tests.  Validation is the PA9 report and the
through-PA8 report; the expected measurable result is all non-floating PA9
tests passing while floating tests remain isolated in group 4.

## Checkpoint result

Completed.  The groups 1–3 checkpoint was implemented as a full PA9
pipeline, and group 4 was bundled because it was the only remaining group.
`make test-report ACTIVE_TEST_REPORT_PAS='pa9'` passes all 11/11 PA9 tests,
including the integer and floating calculator workloads.  The through-PA8
report passes 333/333 tests, and the PA9 file audit passes with only the
pre-existing `recog_parser_internal.h` implementation-body warning.

The implementation now preprocesses and parses concatenated CY86 units into
typed labels, literals, operands, and opcode facts; lays out aligned data;
lowers integer, control-flow, syscall, floating, and conversion operations to
x86-64; resolves absolute/relative relocations; and emits an executable ELF
load segment.

## Architecture Review

The integrated implementation is an in-process CY86-to-Linux translation
pipeline with explicit logical phase boundaries inside `dev/cy86.cpp`:

1. The command-line entry point owns PA8-compatible argument handling,
   translation-unit order, shared PA5 `PreprocessSourceFile` calls, output
   writing, and executable permissions. The PA5 preprocessing, post-token
   Unicode, macro, and token-translation dependencies are listed explicitly
   for `cy86` in `dev/frontend_source_sets.mk`.
2. `cy86::Parser` consumes the concatenated post-preprocessor tokens and owns
   the regular CY86 grammar, including phase-6 concatenation of adjacent
   compatible string literals. It stores labels, literals, registers,
   immediate expressions, and memory expressions as value-owned facts.
   `DescribeOpcode`
   maps only the spellings in `pa9/cy86-opcode.desc`, while
   `ValidateStatement` applies arity, register-width, address-width,
   read/write, floating-memory, and data-operand constraints before lowering.
3. `Compiler::LayoutData` and `Compiler::EmitCode` are separate logical
   passes. Static literals and `data8/16/32/64` statements are aligned and
   assigned virtual addresses first; code labels are assigned as x86 bytes are
   emitted. Absolute data/address relocations and rel32 control-flow
   relocations retain copied label/addend facts until `PatchRelocations` runs
   after all final addresses are known.
4. The lowering helpers use the CY86 register contract (`sp`/`bp` backed by
   `rsp`/`rbp`, and `x`/`y`/`z`/`t` backed by `r12`--`r15`) and fixed x86
   temporaries for integer, syscall, x87 floating, and conversion operations.
   Width normalization preserves the x86 zero-extension rule for 32-bit CY86
   writes; the reserved red zone is used only for compiler temporaries.
5. The final image is one valid Linux x86-64 ELF load segment containing the
   header, aligned CY86 data, and generated code. The entry address is the
   `start` label when present and otherwise the first source statement, as
   required by the handout. The writer performs no host compilation, assembly,
   reference lookup, subprocess execution, fixture dispatch, or test-name
   branching.

The single source file is intentionally the PA9-local executable module, but
the parser, semantic validation, layout, lowering, relocation, and ELF work
are kept as separate compiler methods and data contracts. This keeps the CY86
test language isolated from the PA8 namespace/image model, as the handout's
design notes recommend, while reusing the established PA5 front-end boundary.

## Final Architecture Review

The final audit covered checkpoint commit `a6179e3` and its PA8 parent
`064720f`, then rechecked the integrated implementation after cleanup. The
checkpoint passed all checked-in behavior but had several correctness and
ownership risks that were not represented in its plan:

- Opcode recognition accepted arbitrary identifiers with a recognized prefix
  and a width-looking suffix. That admitted names such as `movebogus8`,
  `data80`, `fadd8`, and malformed conversion spellings. Exact opcode-family
  matching now admits only the descriptors in `cy86-opcode.desc`, and syscall
  numbers must be the exact `syscall0` through `syscall6` spellings.
- C++ keywords were accepted as labels, and the parser accepted arithmetic
  offsets on literal memory/immediate primaries even though the grammar allows
  offsets only on labels (or memory base registers). The parser now rejects
  both cases before code generation.
- The checkpoint's quoted-literal parser treated source UTF-8 bytes as
  independent code points, made every character one byte, and ignored UTF-16,
  UTF-32, and wide string encodings. It now decodes PA2 quoted-literal values,
  preserves Unicode and raw-string contents, concatenates adjacent compatible
  strings, emits the correct element width, and aligns static strings by that
  element width. Floating literals are also converted through their PA2 object
  bytes when used by an immediate/data operand instead of being rejected as
  non-integral.
- Internal relocation labels used a process-global serial and the old
  command-line batch branch returned `EXIT_NOT_IMPLEMENTED`. The serial is now
  owned by each `Compiler`; batching remains the responsibility of the shared
  test-runner wrapper and is not a compiler output path. Dead code-index and
  emitter helpers were removed.

The compiler owns its statement, label, expression, image, and relocation
vectors/maps by value. Relocations copy strings and expressions, so no emitted
image refers to parser-owned storage after parsing. The final code path keeps
the two-pass layout/relocation structure and bounded temporary-register
strategy; no test-specific workaround or reference/host-tool shortcut was
introduced. The source audit still reports only the known non-fatal heuristic
warning for the earlier declaration-only `dev/src/recog_parser_internal.h`;
there is no new PA9 source-audit warning.

## Grouped remaining work

None for PA9.  The complete current-PA failure set is covered by the passing
stage report.

## Next checkpoint group

PA10’s next assignment contract, reusing the PA9 preprocessing and x86
lowering boundaries where applicable.
