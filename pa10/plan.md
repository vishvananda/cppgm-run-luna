# PA10 implementation plan

## Baseline and failure grouping

The turn-start required PA10 report was `0 / 136` tests passing.  Every
failure had the same immediate status mismatch: the scaffold returned
`EXIT_NOT_IMPLEMENTED` from `--emit-ast`, while the fixture expected either
`EXIT_SUCCESS` or (for malformed inputs) `EXIT_FAILURE`.

The complete current-PA failure set is grouped by the shared compiler
behavior it needs:

1. **Driver and source pipeline (all 136 tests).**  Implement the PA10
   `--emit-ast -o <file> <sources...>` path, preprocess and post-tokenize each
   source in command-line order, normalize split right-shift tokens, and emit
   deterministic translation-unit framing and AST text.
2. **Core declarations, types, and declarators.**  This covers the `spec/100`
   declaration/enum/namespace/template/declarator cases and the `general/100`
   declaration, typedef, alias, static-assert, and structured type-id cases.
   The shared behavior is typed declaration-specifier, type-id, declarator,
   initializer, and name nodes with exact leaf-token spelling.
3. **Statements and expressions.**  This covers `spec/100-switch-try`,
   `spec/300-*`, and the `general/100-*` and `general/200-*` control-flow,
   cast, operator, call, lambda, `new`/`delete`, and condition cases.  The
   shared behavior is precedence-aware expression parsing and structured
   statement/condition nodes, including declaration-vs-expression choice.
4. **Classes, namespaces, templates, and member syntax.**  This covers
   `spec/200-*` plus the `general/200-*` and `general/300-*` qualified names,
   class bases/members, constructor initializers, template parameters and
   arguments, partial/specialization syntax, attributes, exception
   specifications, and anonymous aggregates.  These need preservation of
   unresolved syntax rather than semantic lookup.
5. **Failure behavior.**  Malformed tests such as `general/100-bad.t`, the
   bad condition/parameter/template/operator cases, and unsupported
   `co_return` must fail during preprocessing/tokenization/parsing while
   successful syntax remains a structured tree.

## Checkpoint scope

The first substantial checkpoint implements the reusable typed AST node and
printer, the PA5 preprocessing/post-token pipeline, translation-unit framing,
and the core declaration/declarator/type-id, statement, and precedence-aware
expression parser.  It will validate against the complete `spec/100` and
`general/100` groups, plus their shared syntax needed by later complex cases;
the remaining class/template/attribute edge cases will be the next checkpoint
group.  The implementation must use compiler-owned typed state and real parse
behavior, not test-specific output synthesis.

## Checkpoint result

Completed in this turn.  The PA10 `--emit-ast` path now preprocesses and
validates all source units, normalizes split right-shift tokens, builds a
typed AST, and prints the required translation-unit framing.  The parser
covers the core declarations/types/declarators, class and namespace forms,
templates and dependent names, statement/expression precedence, type-id and
new/delete handling, member/special-member syntax, attributes, linkage and
exception specifications, and the required malformed-input failures.

Validation: `make test-report ACTIVE_TEST_REPORT_PAS='pa10'` passed all 136
tests.  The implementation also remains confined to `dev/` plus this
running plan, with the new parser translation units registered in
`dev/frontend_source_sets.mk`.

## Remaining Work Map after checkpoint

No PA10 behavior remains in the checked-in current-PA test set.  No next
checkpoint is required for PA10; future work should preserve this parser and
extend it through PA11 using the existing typed AST and preprocessing
pipeline.
