# PA6 stage plan and architecture review

## Baseline

At the start of this checkpoint, all 43 PA6 tests failed because `recog`
returned `EXIT_NOT_IMPLEMENTED`; the through-PA5 suite was passing.  The
failure set is therefore one shared entry-point failure, not 43 independent
grammar failures.

## Remaining Work Map

Grouped by compiler behavior needed to turn the stub into a recognizer:

1. **Front-end/token facts (all 43 tests):** read each source, perform the
   PA5-compatible post-token translation, reject invalid post tokens, classify
   identifiers/literals/keywords/operators, append EOF, and split `>>` into
   the PA6 close-angle tokens.
2. **Expression and primary parsing (120--201, plus expressions embedded in
   declarations/statements):** names and mock lookup, lambdas, postfix/unary/
   cast expressions, precedence, assignment, conditional, and angle-bracket
   reservation.
3. **Statements and local declarations (100--180, 600--700, and course
   regressions):** compound/selection/iteration/jump/try statements,
   labels, attributes, declaration-vs-expression disambiguation, and balanced
   token handling.
4. **Declarations and type syntax (250--450 and declaration-bearing earlier
   tests):** decl-specifier sequences, declarators/type-ids, classes/enums,
   namespaces/linkage/using/asm, functions, initializers, templates,
   operators, exceptions, and member declarations.
5. **Validation and integration:** preserve PA1--PA5, match per-file `OK` or
   `BAD` output, run the local and through reports, and pass the source-file
   audit.

## Checkpoint Scope

The checkpoint began with groups 1--3, then expanded through group 4 when the
same parser state and token facts supported the declaration-heavy grammar.
Validation is the full PA6 local report, the through-PA6 report, and the
source-file audit.

## Checkpoint Result

The selected scope was completed and expanded through the declaration-heavy
group because the remaining groups were small once the shared parser state was
in place.  `recog` now performs post-token translation, typed literal/operator
classification, mock-name lookup, recursive-descent expression and statement
recognition, declarators/type-ids, classes/enums/namespaces/linkage, templates,
attributes, exceptions, operator ids, and PA6 close-angle handling.  The
parser is split into cohesive compiled modules under `dev/src/`, and the
source set links the same lexer/translation code used by the earlier tools.

Validation completed:

- current PA6 report: **43/43 passed**;
- through PA5 report: **224/224 passed**;
- through PA6 report: **267/267 passed**;
- `perl scripts/cppgm_file_audit.pl --stage pa6 --paths dev/src`: passed
  (one non-fatal header-body warning).

## Architecture Review

The implemented pipeline is intentionally layered:

1. `recog.cpp` owns command-line handling, per-file `OK`/`BAD` isolation, and
   output formatting.
2. `preprocessor_engine.cpp` exposes the PA5 preprocessor as the shared
   `PreprocessSourceFile` entry point.  `preproc` and `recog` therefore use the
   same macro, conditional, include, pragma, and line-directive behavior.
3. `ValidatePostTokens` reuses the PA5 post-token semantic checks before PA6
   normalization.  Its discard-only stream buffer keeps the existing
   presentation-oriented validator out of the recognizer's output while
   avoiding a second full report string.
4. `NormalizeTokens` converts post-preprocessor tokens to typed recognizer
   tokens, records the PA6 mock-name facts, rejects non-whitespace/header
   leftovers, adds `ST_EOF`, and expands `>>` into the two close-angle facts.
5. `Parser` owns only grammar state and is split by responsibility across
   `recog_parser.cpp`, `recog_parser_expressions.cpp`,
   `recog_parser_statements.cpp`, and `recog_parser_declarations.cpp`.

The parser preserves the assignment's deliberate boundaries: mock lookup is
stored on normalized tokens rather than a future symbol table, and angle
depth is tracked separately from ordinary bracket depth so a close-angle is
not confused with a relational or shift operator.  Speculative recursive
descent snapshots and restores all parser state, including angle floors.

The final audit moved parser state/helper definitions out of the internal
header and removed dead PA6 entry-point helpers.  The remaining file-audit
warning names `recog_parser_internal.h` because its declaration-only class
interface contains many grammar method declarations; no executable parser
body remains there.  Splitting that cohesive interface solely to satisfy the
heuristic would make ownership less clear.

## Final Architecture Review

The completed stage has one preprocessing implementation, one semantic token
validation boundary, one typed-token normalization boundary, and one
recursive-descent parser.  There is no test-specific dispatch, reference
binary call, host compiler call, duplicated PA5 preprocessing path, or parser
output dependency.  Invalid opening, preprocessing, token-validation, and
grammar cases are caught at the per-source-file boundary and reported as
`BAD`, while command-line/output failures remain process-level errors as
required by the handout.

The cleanup also covers correctness edges found during review: alternative
operator keywords are not treated as identifiers, empty-string recognition is
limited to the ordinary `""` spelling, lambda default captures reject a
trailing comma without a capture, cast operands leave angle mode before
parsing ordinary expressions, qualified enum/class heads are supported,
trailing-return declarators are disambiguated, anonymous bit-fields are
accepted, and repeated attribute specifiers follow the grammar.  The shared
preprocessor is linked explicitly in both frontend source sets, so ownership
and build dependency tracking remain visible to the build system.

## Remaining Work Map

There is no remaining checked-in PA6 failure group.  The next general compiler
group is PA7's semantic/name-state work; the PA6 recognizer's mock facts remain
deliberately local to its typed token state.  The PA5 preprocessing engine is
already exposed through the shared `dev/src/preprocessor_engine.*` module for
future front ends.

## Next Checkpoint Group

Begin PA7 by replacing mock lookup with persistent symbol/type facts while
keeping the PA6 grammar regression suite green.
