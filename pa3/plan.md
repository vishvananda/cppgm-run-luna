# PA3 checkpoint plan

## Baseline and complete failure set

At the start of this checkpoint the required PA3 report was **0/19 tests
passing**. Every current-PA test stopped in the `ctrlexpr` scaffold with
`EXIT_NOT_IMPLEMENTED`; the through-PA2 behavior was already passing.

The complete current-PA failure set is grouped by shared compiler behavior:

- **Phase 1–3 input and logical-line integration (all 19 tests):** the
  implementation must reuse the PA2 translation pipeline, split the
  translated stream at newlines, discard whitespace/comments, preserve
  phase 1–3 failures as process failure, and emit one result per logical
  line followed by `eof`. This is exercised across `pa3/tests/100-primary.t`,
  `110-paren.t`, `120-defined.t`, `200-ops-alts.t`, `200-ops.t`,
  `250-eval-order.t`, `260-cond-ret-type.t`, and `300-triple.t`, plus the
  course tests `00-multi.t`, `010-invalid.t`, `011-minus-minus.t`,
  `040-char.t`, `050-comp.t`, `060-defined-defined.t`, `070-double-shift.t`,
  `070-multi-div.t`, `110-allop.t`, `130-defparam.t`, and
  `500-integer-overflow.t`.
- **Post-tokenization, integral literals, identifiers, and `defined`:**
  local `100-primary.t`, `110-paren.t`, and `120-defined.t`, and course
  `010-invalid.t`, `040-char.t`, `060-defined-defined.t`, and `130-defparam.t`
  require PA2-compatible token classification, integral-only literal
  acceptance, boolean/identifier handling, mock `defined` lookup, and
  syntax/semantic rejection.
- **Predictive expression parsing and operator precedence:** local
  `200-ops-alts.t` and `200-ops.t`, and course `00-multi.t`, `011-minus-minus.t`,
  and `110-allop.t` require the complete controlling-expression grammar,
  left-associative binary operators, right-associative conditional operators,
  parentheses, alternate operator spellings, and invalid-token recovery.
- **Typed evaluation, conversions, sequencing, and errors:** local
  `250-eval-order.t`, `260-cond-ret-type.t`, and `300-triple.t`, and course
  `050-comp.t`, `070-double-shift.t`, `070-multi-div.t`, and
  `500-integer-overflow.t` require signed/unsigned `intmax_t`-style state,
  usual arithmetic conversions, sign-preserving shifts, conditional result
  typing, short-circuit evaluation, and safe division/modulo/shift error
  handling.

## Remaining Work Map

Before implementation, all four groups above remain. They are coupled through
one line-oriented compiler pipeline: source translation feeds typed PA2
tokens, the parser builds the grammar's structure, and the evaluator must
retain signedness even when a branch or right operand is not evaluated.

## Checkpoint Scope

Implement the complete PA3 `ctrlexpr` behavior as one coherent extension of
the existing compiler. The scope includes the shared PA1/PA2 phase 1–3
translation and logical-line adapter, PA2-compatible post-tokenization for
integral literals and identifiers, the full hand-written controlling-expression
parser, typed `intmax_t`/`uintmax_t` evaluation with the required conversion
rules, short-circuit sequencing, course-defined error conditions, and the
required result/`eof` output. Validation will cover all 19 PA3 tests, the
through-PA2 report, and the PA3 source audit.

## Checkpoint result

Complete. `ctrlexpr` now consumes the shared phase 1–3 translation stream,
preserves logical-line boundaries, lexes the PA3 preprocessing-token subset,
and evaluates each controlling expression in process. The parser implements
the complete precedence and associativity grammar, including alternate
operator spellings and both forms of `defined`. Integral literals and
character literals retain signedness in typed compiler state and are promoted
to the course-defined 64-bit signed/unsigned domains before operations.
Short-circuit `&&`, `||`, and `?:` evaluation keeps static result types from
unevaluated branches while avoiding their runtime errors; division/modulo and
shift validation is applied only when those operations execute.

Validation completed:

- `make -C pa3 test`: local 8/8 and course 11/11.
- `make test-report ACTIVE_TEST_REPORT_PAS='pa3'`: 19/19.
- `make test-report-through-pa2`: 68/68.
- `perl scripts/cppgm_file_audit.pl --stage pa3 --paths dev/src`: passed,
  20 files checked.

The stage moved from the 0/19 baseline to 19/19, with no current-PA failures
remaining.

## Remaining work after checkpoint

No PA3 behavior remains in the checked-in local or course suites. The four
baseline groups—translation/line integration, token and literal semantics,
grammar parsing, and typed evaluation—are all covered by the passing report.
Future work is outside this stage and must preserve the shared translation
module, PA2 post-tokenizer, and PA3 evaluator boundary.

## Next checkpoint group

PA4 macro replacement and preprocessor state, built on the PA3 token and
expression behavior while preserving the through-PA3 report.
