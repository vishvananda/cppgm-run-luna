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

## Grouped remaining work

None for PA9.  The complete current-PA failure set is covered by the passing
stage report.

## Next checkpoint group

PA10’s next assignment contract, reusing the PA9 preprocessing and x86
lowering boundaries where applicable.
