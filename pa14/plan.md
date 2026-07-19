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
