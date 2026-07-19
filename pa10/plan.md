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

## Architecture Review

The integrated PA10 path has four explicit boundaries:

1. `dev/cppgm++.cpp` owns the PA10 driver contract.  It accepts the
   `--emit-ast -o <outfile> <source...>` shape, preserves command-line source
   order, writes the translation-unit framing, and turns preprocessing,
   token-validation, or parse failures into process-level `EXIT_FAILURE`.
   It does not route PA10 through the later type, semantics, or low-IR modes.
2. `PreprocessSourceFile` and `ValidatePostTokens` reuse the established PA5
   phase-1-through-7 boundary.  The PA10 source set lists the preprocessor,
   macro engine, post-token lexer/unicode/semantics, token translation, and
   controlling-expression dependencies explicitly in
   `dev/frontend_source_sets.mk`; there is no second PA10-only source reader
   or preprocessing implementation.
3. `Parser::Normalize` copies post-preprocessor spelling into PA10-owned
   typed tokens, rejects presentation-only leftovers, preserves the PA6
   mock-name facts, and splits `>>` into close-angle pieces.  `Parser` keeps
   angle depth separate from ordinary bracket depth and snapshots all
   speculative state, including angle floors, before ambiguous declaration,
   type-id, template, and expression alternatives.
4. The recursive-descent parser is divided by responsibility across
   declarations, types/declarators, classes/templates/names,
   expressions, and statements.  It builds `CPPGMAstNode` trees during the
   parse; it does not preserve unsupported syntax as an opaque source span.
   `shared_ptr` nodes own their children, normalized tokens own copied text,
   and parser symbol/name sets are local to one translation unit.  No AST node
   points back into the input token vector, so the tree remains valid after
   preprocessing and normalization temporaries are released.

The printer is a single deterministic preorder walk.  It emits node kind,
optional leaf value, and fixed two-space indentation, leaving all ordering to
the parser's source-order child vectors.  Expression precedence is handled by
the bounded ten-level binary-expression descent, while template and
declarator ambiguity is resolved only with the PA10/PA6 syntax facts available;
name lookup and type semantics remain deferred as required by the assignment.

The normal work per source is one preprocessing pass, one validation pass, one
normalization pass, one parse, and one print.  Speculative marks copy only the
small angle-floor state in addition to the token position and bracket depths;
there is no reference-binary invocation, host compiler call, subprocess,
fixture read, or test-name dispatch in the production path.  The source audit
reports one inherited, non-fatal heuristic warning for the declaration-heavy
PA6 interface `dev/src/recog_parser_internal.h`; the PA10 modules themselves
stay below the audit's source/function-shape limits.

## Final Architecture Review

The final audit of checkpoint commit `4b7d688` and its PA9 parent found that
the checkpoint behavior was complete but the stage record was not: PA10 had
no `audit.md`, and the plan stopped before the required architecture review
sections.  The integrated review also exercised valid syntax adjacent to the
fixtures and found several parser-boundary gaps.  The cleanup keeps those
fixes in the syntax layer rather than adding semantic lookup or fixture
special cases:

- global-qualified type-ids and `using ::qualified::name;` now use the same
  qualified-name parser as expressions and aliases;
- lambda captures preserve default captures, reference captures, `this`, and
  pack expansions in `lambda-capture`/`capture-list` nodes;
- the alternate parenthesized-type-id `new` form and unary `sizeof` form are
  parsed without weakening the existing placement-new and `sizeof(type)`
  paths;
- `override`/`final` are emitted as `virt-specifier` children, literal
  operator names are parsed as names, and class explicit instantiations must
  consume their required semicolon;
- unused parser helpers were removed, and the new parse helpers were split so
  the required file-audit limits remain satisfied.  Unnamed non-type template
  defaults retain their terminal literal category, independent of test names
  or source locations.

The final stage therefore hands PA11 a stable, owned syntax tree for all
checked-in PA10 forms, while preserving the earlier preprocessing and token
contracts.  It does not claim name binding, type checking, overload
resolution, template deduction, or lowering; those remain clean downstream
responsibilities.  The only remaining audit warning belongs to the earlier
PA6 declaration interface and is documented consistently across prior stage
audits rather than weakening that cohesive parser boundary to silence a
heuristic.

## Remaining Work Map after checkpoint

No PA10 behavior remains in the checked-in current-PA test set.  No next
checkpoint is required for PA10; future work should preserve this parser and
extend it through PA11 using the existing typed AST and preprocessing
pipeline.
