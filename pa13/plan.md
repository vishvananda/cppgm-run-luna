# PA13 implementation plan

## Baseline

The required active report discovers 90 PA13 `tests/spec/*.t` cases (the
broader PA13 tree contains 108 tests because it also contains debuginfo
wrappers for later tools). At the turn-start baseline, all 90 cases failed
with `EXIT_NOT_IMPLEMENTED`: 65 expected success and 25 expected failure.
The failure set was therefore one scaffold failure, but its required
behaviors group as follows:

## Remaining Work Map

| Shared behavior | Baseline cases | Work remaining |
| --- | --- | --- |
| LowIR text reader and typed program model | all 90 | Read the command-line files, preserve typed symbols/types/operands/instructions and deterministic source order. |
| Structural and semantic validation | `100-bad-missing-terminator`; all 25 `200-bad-*` cases | Validate duplicate symbols/aliases/parameters/slots/blocks, terminators and targets, definitions, metadata domains, parameter rules, storage alignment, conversion widths, and indirect-call signatures. |
| Program/data lowering | 100 `global`, `structured`, `zeroinit`, `copyobj`, index, object-boundary, and 300 object-metadata/alias cases; related 200 global/slot cases | Emit exact PA9 CY86 data/function layout, address constants, object copies, zero padding, aliases, declarations, and startup/shutdown hook behavior. |
| Scalar instruction lowering | 100 arithmetic/control cases; 200 copy, unary, binary, compare, conversion, signed/unsigned and float cases | Lower integer, pointer, f32/f64/f80, comparison-to-canonical-i64, conversion, and address/index operations with PA9-compatible temporaries and labels. |
| Calls and control flow | 100 call/loop/branch cases; 200 direct/indirect/signature/arity and call-boundary metadata cases | Lower direct and indirect calls, explicit signatures, returns, jumps, branches, switches, variadic/prototype-relaxed boundaries, and object-result boundaries. |
| Atomic and exception adapter forms | six 200 atomic cases; three 100 EH cases | Accept the single-threaded atomic interpretation and translate EH forms used by the suite into deterministic CY86 control/runtime behavior. |
| Metadata and unused declarations | remaining 200 metadata smoke cases and declarations | Parse and retain valid metadata without changing CY86 output, while rejecting invalid values and preserving symbol-boundary facts during validation. |

## Checkpoint Scope

Implement the complete PA13 `lowir2cy86` adapter in one coherent checkpoint:
typed parsing, validation, and deterministic CY86 source lowering for every
required instruction/type/top-level form in `pa13/README.md` and the 90 active
spec cases above. The checkpoint includes the rejection contract and the
single-threaded atomics/EH subset, so validation and translation can be
tested together rather than leaving a parser-only increment that cannot run
the LowIR programs.

Validation for the checkpoint is `make test-report ACTIVE_TEST_REPORT_PAS='pa13'`,
the through-PA12 report, and the PA13 source audit. If a narrow remainder is
found after implementation, it will be grouped below with its next
checkpoint rather than hidden behind a test-specific exception.

## Checkpoint Result

Completed the full checkpoint in `dev/lowir2cy86.cpp`. The adapter now has a
typed line-oriented LowIR model, structural/metadata validation, deterministic
PA9 CY86 lowering, direct/indirect call handling, scalar and f32/f64/f80
operations and conversions, structured data/object boundaries, single-threaded
atomics, and the PA13 exception forms. Metadata is parsed into compiler state
and only affects lowering where the PA13 boundary requires it (roles, passing
modes, TLS validation, and call signatures); declarations and object aliases
are preserved for validation while remaining non-emitting CY86 facts.

Required validation completed:

- `make test-report ACTIVE_TEST_REPORT_PAS='pa13'`: 90/90 active PA13 spec cases pass.
- Through-previous check: `make test-report-through-pa12`: 657/657 pass.
- `perl scripts/cppgm_file_audit.pl --stage pa13 --paths dev/src`: passed with two pre-existing header-division warnings.
- `git diff --check`: passed.

## Remaining Work Map

No active PA13 groups remain: the complete checked-in PA13 spec suite passes,
including its 25 structural/metadata rejection cases. The broader PA13 tree
still contains 18 debuginfo wrapper tests for later tools; those are outside
the PA13 `lowir2cy86` driver contract and are not remaining adapter work.

## Next Checkpoint Group

PA14: begin the next assignment's source-to-LowIR-facing increment while
keeping the PA13 adapter and through-PA13 regression suites green. Any new
LowIR boundary fact must remain representable in the typed model and text
contract before a later backend consumes it.
