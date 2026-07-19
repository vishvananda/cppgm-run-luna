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

## Architecture Review

The integrated PA13 stage keeps the intended adapter boundary explicit in
`dev/lowir2cy86.cpp`:

1. `run_lowir2cy86` owns the PA13 driver contract, reads the command-line
   inputs in order, and converts parser, validation, translation, and output
   failures to `EXIT_FAILURE`. It does not invoke a host compiler, a reference
   binary, a shell command, or a later native/object backend.
2. `LowParser` is the text boundary. It lexes one line at a time, removes
   comments and optional `!dbg` transport metadata, and builds value-owned
   `Program`, `Global`, `Function`, `Block`, `Instruction`, `Type`, operand,
   signature, and metadata records. Multiple input files are concatenated in
   command-line order without losing source order inside a file.
3. `Validator` is a separate semantic/structural pass. It checks top-level
   symbol and alias ownership, declaration types, roles and metadata domains,
   parameter passing rules, TLS wrappers, block/terminator structure, target
   definitions, temporary/slot references, indirect-call signatures, atomic
   order values, conversion widths, object spans, and the required entry role.
   Symbol maps are populated before instruction validation, so forward direct
   calls to either definitions or declarations resolve consistently. Temporary
   visibility is carried through blocks incrementally rather than rescanning
   every earlier block for each block.
4. `FunctionEmitter` lowers the validated function model to deterministic PA9
   CY86 source. It owns frame locations for parameters, slots, temporaries,
   hidden object/f80 return storage, alignment padding, and scratch storage;
   handles direct and indirect calls with explicit overflow-argument slots;
   preserves f80's 16-byte storage with a 10-byte payload and zero padding;
   and emits scalar, object, atomic, control-flow, and exception forms through
   small lowering helpers. Object copies use exact 8/4/2/1-byte tails.
5. `build_cy86` emits the stable `start` wrapper, optional init/entry/fini
   calls, function bodies in source order, runtime EH support when needed, and
   globals in source order. Declarations and object aliases remain semantic
   validation facts and are not emitted as definitions.

The model is compiler-owned by value: strings, vectors, maps, operands, and
metadata do not retain references into lexer lines or temporary parser state.
The PA13 executable is intentionally a PA13-local module rather than a new
shared `dev/src` implementation unit, so no source-set registration is needed;
the required `dev/src` audit remains limited to the inherited header-body
warnings recorded in the final audit.

## Final Architecture Review

The final review covered checkpoint commit `7093941`, its PA12 handoff, the
complete PA13 contract, all active PA13 fixtures, the changed adapter, and the
integrated through-stage implementation. The checkpoint behavior was green,
but the review found correctness and ownership risks outside the narrow
fixture set. The final implementation closes them without changing existing
checked-in CY86 fixtures:

- Bare `readonly` and `thread_local` global spellings are now preserved and
  merged with bracket metadata instead of being dropped or rejected.
- Function declarations are included in the direct-call symbol map. A call to
  a declared function is a direct call; only a pointer/global-value callee
  requires an explicit indirect signature. Call-signature metadata is limited
  to the call-boundary keys defined by LowIR.
- Calls reserve one slot per overflow argument and keep an indirect callee
  pointer in a separate slot. This also accounts for the hidden result
  pointer consumed by direct object/f80 returns, avoiding register-vector
  overrun and stack-argument overwrite for larger valid calls.
- Narrow i1/i8/i16 accesses, atomic operations, unary operations, branch and
  switch selectors, and integer returns use legal CY86 widths. Signed narrow
  values are extended when a 64-bit address/control value is required.
- Object parameter and object-return copies honor non-8-byte tails, frame
  allocation honors direct-object alignment, and f80-returning functions get
  scratch storage when their body actually performs f80 work rather than
  reusing the frame base.
- Atomic memory orders, declared global types, index metadata keys, negative
  structured zero spans, declaration metadata, and the PA13 type family are
  validated at the LowIR boundary. The validation prefix map is incremental,
  avoiding repeated scans and copies of every earlier block.

No tests, reference outputs, grammar, or harness scripts were edited. The
adapter remains a mechanical LowIR-to-CY86 translation boundary; roles,
passing modes, TLS facts, aliases, and call signatures are retained in the
typed model for validation, while native ABI binding and object/debug output
remain outside PA13.

## Next Checkpoint Group

PA14: begin the next assignment's source-to-LowIR-facing increment while
keeping the PA13 adapter and through-PA13 regression suites green. Any new
LowIR boundary fact must remain representable in the typed model and text
contract before a later backend consumes it.
