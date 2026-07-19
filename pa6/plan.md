# PA6 checkpoint plan

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

This checkpoint covers groups 1--3: a real PA5-compatible token stream and a
recursive-descent recognizer for translation units' expressions, statements,
attributes, and local declarations, including the PA6 mock-name and
close-angle rules.  It is sized to move the current stage above its 0/43
baseline while providing the foundation for group 4.  Validation is the full
PA6 local report plus the through-PA5 report and file audit; group 4 is the
next checkpoint if declaration-heavy cases remain.

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
- `perl scripts/cppgm_file_audit.pl --stage pa6 --paths dev/src`: passed
  (one non-fatal header-body warning).

## Remaining Work Map

There is no remaining checked-in PA6 failure group.  The next general compiler
group is PA7's semantic/name-state work; the PA6 recognizer's mock facts remain
deliberately local to its typed token state.  A future hardening checkpoint can
also expose the existing PA5 preprocessor engine as a reusable library entry
point for `recog`, so PA6 clients with macro/include inputs use the same
preprocessing path without duplicating it.

## Next Checkpoint Group

Begin PA7 by replacing mock lookup with persistent symbol/type facts while
keeping the PA6 grammar regression suite green.  If PA6 preprocessing reuse is
required before that assignment, first extract the `Preprocessor::Process`
engine from `dev/preproc.cpp` into a shared `dev/src` module and add a focused
macro/include recognizer regression test.
