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

## Architecture Review

The integrated PA3 flow has an explicit application boundary and four owned
layers:

    dev/ctrlexpr.cpp
        UTF-8 stdin -> RunCtrlExpr -> process exit/error boundary

    dev/src/ctrlexpr.cpp
        shared translated source -> line-preserving PA3 lexer
        -> typed recursive-descent parse tree -> lazy evaluation -> output

    dev/src/pptoken_translation.cpp/.h
        phase 1-3 decoding, raw-span protection, trigraphs, line splices,
        universal-character names, and source encoding

    dev/src/posttoken_unicode.cpp and posttoken_lexer.h
        UTF-8 semantic decoding and the shared PA2 preprocessing-token record
        kinds used by the PA3 line lexer

`dev/frontend_source_sets.mk` links the translation and Unicode modules into
`ctrlexpr`; the same translation module is linked by `pptoken` and
`posttoken`. This preserves the PA1/PA2 translation contract while allowing
PA3 to retain new-line boundaries instead of routing through PA2's whole-stream
consumer. `CtrlLexer` discards whitespace and comments, returns one token
sequence per translated logical line, and lets phase 1-3 exceptions escape to
the application boundary. Parser/evaluation errors are contained per line and
produce `error`, followed by the final `eof` record.

The parser is a hand-written predictive parser matching the PA3 grammar. Its
ten binary precedence levels use iteration for left associativity, while the
conditional parser recurses on both arms for the grammar's right associativity.
Each node records static signedness. `Value` stores a 64-bit bit pattern and
signedness, so the evaluator can apply the course-defined `intmax_t` and
`uintmax_t` domains, usual arithmetic conversions, sign-preserving right
shifts, and conditional result conversion without evaluating an unselected
branch. `&&`, `||`, and `?:` retain this static type information while keeping
their runtime evaluation lazy.

The checkpoint implementation was behaviorally complete, but its eight-digit
UCN accumulator used signed `int`, and its translation passes temporarily
allocated replacement vectors for the full input. The final cleanup moves UCN
accumulation to `uint32_t`, compacts the three shrinking translation passes in
place while retaining `SourceUnit` provenance, and stores parse nodes in a
reusable vector arena rather than allocating a `unique_ptr` object for every
AST node. These changes keep ownership local and bounded for the generated
large-expression case without changing the grammar or evaluator semantics.

## Final Architecture Review

The final stage boundary is:

    UTF-8 bytes
      -> shared phase 1-3 SourceUnit stream with raw/origin metadata
      -> PA3 line lexer and PA2-compatible token kinds
      -> precedence parser with a reusable indexed node arena
      -> lazy typed evaluator using signed/unsigned 64-bit state
      -> decimal result, error, and eof output

Raw literal bodies remain protected by translation metadata, comments are
removed only at the line-lexer boundary, and translated new-lines continue to
define the required logical-line output behavior. Invalid phase input still
escapes as process failure, while invalid tokens, grammar, literal, division,
modulo, and shift cases are converted to a line-level `error`. No reference
fixture, test name, subprocess, host compiler, or generated answer is used by
the production path.

The final ownership model is cohesive across the completed stages:
`pptoken_translation.*` owns reusable source translation; PA1 and PA2 consume
that module through their source sets; PA3 owns only its line adapter, parser,
typed evaluator, and output; and the node arena is cleared and reused for each
line. Translation compaction avoids simultaneous full-size phase buffers, and
the large checked-in PA3 stress input completes below the normal per-test
timeout. The through-PA3 report and the final source audit verify that the
cleanup preserves all earlier assignments and the intended staged compiler
architecture.

## Next checkpoint group

PA4 macro replacement and preprocessor state, built on the PA3 token and
expression behavior while preserving the through-PA3 report.
