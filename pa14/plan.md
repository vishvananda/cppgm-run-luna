# PA14 implementation plan

## Baseline

The turn-start report was `0 / 72` PA14 tests.  Every case reached the
`--emit-lowir` driver, but the driver returned `EXIT_NOT_IMPLEMENTED`; the
through-PA13 report was green and the source file audit was green.

## Remaining Work Map

The complete current-PA failure set is grouped by the compiler behavior that
must replace that common stub:

1. **Program collection, symbols, and basic procedural bodies** —
   `100-ret0`, `100-simple-call`, `100-local-arith`, `100-global-variable`,
   `100-namespace-call`, `100-if-else`, `100-for-loop`, `100-while-break`,
   `100-bad-switch`, `100-using-directive-imported-value-function-body`,
   `100-unnamed-parameter-storage`, `200-five-arg-call`,
   `200-local-int-slot-width`, `200-shadowed-local-slot-names`, and
   `200-included-namespace-global-definition`.
2. **Scalar expressions and value/control lowering** —
   `100-literal-canonicalization`, `100-unary-logical-conditional`,
   `100-string-hex-escape-code-unit`, `200-direct-short-circuit-condition-branch`,
   `200-floating-logical-branch`, `200-floating-return-integral-conversion`,
   `200-floating-compound-assign-integral-rhs`,
   `200-immediate-widening-canonicalization`, and
   `300-return-empty-braces-scalar`.
3. **Lvalues, references, casts, and side effects** —
   `100-condition-declaration-variable-rvalue`,
   `100-const-integral-lvalue-overload-category`,
   `200-address-of-local-const-integral-uses-storage`,
   `200-comma-expression-lvalue-address`, `200-const-cast-pointer-const-drop`,
   `200-const-cast-reference-array-subscript`,
   `200-const-cast-reference-similar-pointer`,
   `200-const-ref-converted-float-argument`,
   `200-function-reference-static-cast-call`,
   `200-local-lvalue-reference-alias-init`, `200-lvalue-conditional-address`,
   `200-prefix-incdec-lvalue-address`,
   `200-prefix-pointer-decrement-reference-argument`,
   `200-reference-parameter-temp-name-collision`,
   `200-return-void-call-expression`, `200-scalar-assignment-address-lvalue`,
   and `200-scalar-reference-static-cast-return`.
4. **Arrays, pointer arithmetic, and function-pointer calls** —
   `100-function-pointer-ref-call`, `100-global-function-pointer-argument-call`,
   `100-subscript-sizeof`, `100-unary-plus-array-decay`,
   `200-conditional-array-decay-subscript`,
   `200-global-array-decay-compare`, `200-global-array-one-past-end-pointer`,
   `200-global-pointer-array-subscript-load`,
   `200-pointer-compound-assignment-scale`, `200-pointer-deref-byte-load`,
   and `200-pointer-operator-array-decay`.
5. **Constant globals and structured data** —
   `200-global-array-bitwise-or-enum-init`,
   `200-global-array-scalar-cast-init`,
   `200-global-array-static-const-byte-init`,
   `200-global-pointer-array-null-fill`, and
   `200-global-pointer-array-nullptr-init`.
6. **Enums, overload resolution, conversions, and variadic boundaries** —
   `100-enum-default-argument-constant-fold`,
   `100-scoped-enum-no-implicit-int-bad`,
   `100-scoped-enum-previous-enumerator-bitwise-or`,
   `200-enum-class-scalar-lowering`, `200-qualified-namespace-overload-definition-symbol`,
   `200-scoped-enum-underlying-type`, `200-signed-enum-compare-lowering`,
   `200-unscoped-enum-promotion-overload`, and
   `200-variadic-float-argument-promotes-to-double`.
7. **Structured control-flow edge cases** —
   `200-for-init-assignment-expression`,
   `200-for-iteration-discards-void-comma-rhs`,
   `200-goto-case-block-entry-label`, `200-goto-case-block-label-after-statement`,
   and `200-switch-case-nested-inside-if`.

## Checkpoint Scope

This checkpoint covers the complete PA14 procedural boundary in one typed
lowering path: combine the parsed translation units, reuse the PA11/PA12
scope/type facts, emit valid PA13 LowIR for namespace functions and supported
globals, lower scalar expressions and lvalues, structured control flow,
references, arrays/pointers, enum and floating conversions, direct/indirect
calls, string/aggregate constant data, and the required global initializer
actions.  It also includes deterministic LowIR names and entry metadata so
the result can be validated by the existing normalized PA14 oracle.

Validation for the checkpoint is the PA14 local report, the through-PA13
report, and the PA14 source file audit.  If any family remains after the first
implementation pass, the next checkpoint will be the smallest still-failing
family from the map, with its exact test list recorded here.

## Checkpoint Result

The full checkpoint scope is complete.  The new `--emit-lowir` path now
combines translation units, runs the existing PA11/PA12 analyzer, and emits
the typed procedural PA13 LowIR boundary for every mapped family.  The
implementation also closes the parser's postfix-call chain after a
function-reference `static_cast`, preserves function-reference decay and
scalar-reference loads, reserves temporary names that collide with parameter
names, and handles omitted aggregate zero initialization and enum/integer
conversion boundaries.

Validation completed locally: `make test-pa14 CPPGM_SKIP_DEV_REBUILD=1`
passed `72 / 72` tests.  The normal required report
(`make test-report ACTIVE_TEST_REPORT_PAS='pa14'`) passed `72 / 72`, the
through-PA13 report passed `747 / 747`, and
`perl scripts/cppgm_file_audit.pl --stage pa14 --paths dev/src` passed with
only three non-fatal substantial-header warnings.  The lowering implementation
is split across five audited translation units so each source file remains
within the repository size limit.

## Post-checkpoint Remaining Work

There are no remaining PA14-local behavior groups after this checkpoint.
The next checkpoint is complete; no PA14 behavior group remains.  Later
assignment work should extend the same typed lowering boundary rather than
reopen these procedural groups.

## Architecture Review

The integrated PA14 stage keeps the source-to-LowIR boundary explicit across
the driver and the five registered lowering units:

1. `run_emit_lowir_mode` in `dev/cppgm++.cpp` owns the PA14 invocation.  It
   parses the `--emit-lowir`/`-O0` output contract, preprocesses and validates
   every input, parses each PA10 translation unit, preserves command-line
   translation-unit order, and hands the owned AST vector to
   `EmitPA14LowIR`.  Exceptions become `EXIT_FAILURE` in the existing driver
   boundary; no reference binary, host compiler, subprocess, or fixture file
   is consulted.
2. `PA14Lowerer::Lower` combines the parsed units into one compiler-owned
   translation-unit node, runs the existing PA11/PA12 `Analyzer`, collects
   namespace functions and supported globals, assigns deterministic LowIR
   symbols, and emits declarations, globals, functions, and generated
   initialization in PA13 LowIR form.  The source-set manifest registers
   `pa14_lowering.cpp`, `pa14_lowering_planning.cpp`,
   `pa14_lowering_globals.cpp`, `pa14_lowering_expressions.cpp`, and
   `pa14_lowering_control.cpp` as one frontend implementation.
3. The collection and planning layers preserve PA12 scope/type facts while
   creating explicit function/global records and per-function variable plans.
   `FunctionState` owns environments, local slots, special reference/condition
   slots, blocks, labels, and temporary-name reservations.  Function and
   global records, and local plans referenced by maps, use pointer-stable
   deques so correctness does not depend on an arbitrary source-size reserve
   limit.
4. The global and expression layers centralize LowIR type spelling,
   conversions, constant folding, static addresses, array decay, lvalue
   addressing, references, calls, pointer scaling, scalar operations, and
   string/aggregate data.  String symbols are interned in a direct lookup map;
   all literals are collected before top-level emission so generated globals
   cannot appear after functions.  Function declarations are emitted before
   global and function definitions in the PA13 presentation order.
5. The control layer lowers conditions, short-circuit branches, selection and
   iteration, switch dispatch, labels, jumps, returns, and statement
   discards into terminated LowIR blocks.  It deliberately stops at the PA14
   procedural boundary: class lifetime, templates, exceptions, and native
   ABI/runtime work remain later-stage responsibilities.

The model owns AST references, types, strings, vectors/maps, LowIR text, and
temporary/block state by value or smart pointer.  The output is deterministic
for the same input sequence, keeps order-sensitive instructions and aggregate
items in source order, and remains consumable by the PA13 LowIR contract and
later backend stages.

## Final Architecture Review

The final review covered checkpoint commit `7462cfb`, its PA13/PA12 handoff,
the PA14 README and LowIR contract, all 72 checked-in PA14 general tests, the
complete changed source set, and the integrated through-PA14 implementation.
The checkpoint behavior was green, but the audit found and closed three
integration risks that were not visible in the fixture-only result:

- function/global binding maps and local-plan maps held pointers into vectors
  whose fixed `reserve()` calls could be exceeded by a larger valid input;
  pointer-stable deques now own those records and plans;
- duplicate string interning searched the complete insertion-order vector for
  every repeated literal; a symbol map now makes lookup logarithmic while
  preserving first-use ordering; and
- string literals discovered while lowering function bodies could append a
  global after function definitions.  A pre-emission AST walk interns all
  literals, and the emitter now orders declarations, globals, functions, and
  initialization as one stable top-level sequence.

The review found no runtime dependency on tests, references, host tools, or
external processes; no test/ref/grammar/harness file was edited.  The PA14
header warning reported by the file audit is a non-fatal heuristic for the
private lowering model and declarations shared by the five implementation
units.  The other two warnings are inherited substantial analyzer/parser
headers from earlier stages.  No fatal file-audit issue remains, and moving
those cross-stage models would be a separate ownership rewrite rather than a
PA14 correctness fix.
