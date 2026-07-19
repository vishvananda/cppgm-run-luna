# PA11 implementation plan

## Baseline and failure inventory

The turn-start PA11 baseline is 0/50: every current test reaches the
`--emit-types` dispatcher but returns `EXIT_NOT_IMPLEMENTED`.  Earlier
assignments pass.  The complete failure set is grouped below by the shared
compiler behavior it exercises.

### Remaining Work Map

| Group | Tests | Shared behavior |
| --- | --- | --- |
| A | `spec/100-empty`, `spec/100-global`, `spec/100-alias-and-function`, `general/100-function-pointer-void-parameter`, `general/100-variadic-function-declaration`, `general/200-void-parameter-normalization` | PA11 driver, translation-unit/type dump, declarator-derived fundamental, cv, pointer, reference, array, function, variadic, and `void` parameter types; function and block scopes. |
| B | `spec/100-namespace`, `spec/100-namespace-alias`, `spec/100-qualified-type-lookup`, `spec/100-using-declaration`, `spec/100-using-directive`, `general/100-namespace-reopen`, `general/100-namespace-class`, `general/100-class-forward`, `general/100-nested-class`, `general/200-class-qualified-lookup`, `general/200-alias-qualified-class-lookup`, `general/200-inline-namespace-qualified-lookup`, `general/200-namespace-alias-qualified-using-directive-target` | Persistent namespace/class scopes, reopening and aliases, qualified/unqualified lookup, named/forward/nested classes, and type/value using bindings. |
| C | `spec/200-enum-scoped`, `spec/200-enum-unscoped`, `spec/200-opaque-scoped-enum`, `spec/200-namespace-anonymous-union-injected-members`, `general/200-qualified-member-scoped-enum-definition`, `general/200-scoped-enum-qualified-enumerator`, `general/200-scoped-enum-qualified-enumerator-collision`, `general/300-scoped-enum-cast-constant`, `general/200-sizeof-type-like-id`, `general/200-sizeof-qualified-type-idexpr` | Class/union/enum declarations, enum scopes and enumerator injection, opaque/redeclaration rules, simple integral constant evaluation, and size/alignment facts used in bounds/assertions. |
| D | `spec/200-const-int-static-assert`, `spec/200-sizeof-alignof-bounds`, `spec/200-using-directive-values`, `spec/200-decltype`, `general/200-class-constants-and-using`, `general/200-constexpr-variable`, `general/200-qualified-decltype`, `general/200-using-declaration-values` | Constant object facts, `constexpr` as `const`, static assertions, `sizeof`/`alignof`, `decltype` value/category rules, and value-name using lookup. |
| E | `spec/200-template-parameter-scope`, `general/200-template-template-parameter` | Type and template-template parameter scopes and canonical parameter types. |
| F (diagnostics) | `spec/100-bad-unknown-type`, `spec/300-namespace-alias-non-namespace-bad`, `spec/300-opaque-enum-redecl-underlying-bad`, `spec/300-using-declaration-template-id-bad`, `general/100-bad-using-target`, `general/200-bad-opaque-unscoped-enum`, `general/200-bad-pointer-to-reference-alias`, `general/200-bad-sizeof-incomplete-class`, `general/200-bad-static-assert`, `general/200-class-scope-bad-static-assert` | Semantic rejection of unknown names, invalid namespace/using targets, unsupported opaque enums, pointer-to-reference types, incomplete `sizeof`, and false assertions. |

## Checkpoint Scope

This checkpoint implements groups A and B together, then extends the same
typed semantic model through groups C--F so the complete PA11 contract is
coherent in one committed increment.  It covers the PA11 driver,
AST-to-semantic traversal, typed scope/symbol state, fundamental and
declarator-derived types, namespace reopening and aliases, qualified lookup,
named/forward/nested classes, enums and enumerators, constant evaluation,
`sizeof`/`alignof`, `decltype`, using bindings, template-parameter scopes,
semantic diagnostics, and deterministic output.

## Checkpoint result

Complete.  The direct PA11 harness passes 50/50 tests.  The required root
`make test-report ACTIVE_TEST_REPORT_PAS='pa11'` passes 50/50, the required
`make test-report-through-pa10` passes 480/480, and
`perl scripts/cppgm_file_audit.pl --stage pa11 --paths dev/src` passes (with
one pre-existing `recog_parser_internal.h` header-body warning).

All groups A--F in the Remaining Work Map are closed; no PA11-local behavior
remains outstanding. The final audit retained the complete earlier stage,
corrected anonymous-enum formation and scoped-enum completion ownership, and
left the checked-in PA11 output unchanged.

## Architecture Review

The integrated PA11 implementation has four explicit boundaries:

1. `dev/cppgm++.cpp` owns command-line validation, output-file opening,
   translation-unit framing, and the per-input preprocessing/PA10-AST handoff.
   `EmitPA11Types` owns semantic failure through exceptions; the driver does not
   special-case individual diagnostics or tests.
2. The PA5 preprocessing and post-token validation pipeline remains the single
   phase-1-through-7 boundary. PA10 remains the syntax boundary, and the PA11
   source set registers `pa11_semantics.cpp` explicitly alongside the parser and
   preprocessing dependencies.
3. `pa11_semantics.cpp` owns typed `Type`, `Scope`, `Binding`, and constant
   records. Scope nodes own their children and ordered bindings; `TypePtr`
   preserves derived-type graphs and nominal type records; parent and owned
   scope pointers never outlive the analyzer tree. Namespace aliases and using
   directives are lookup state, not output text.
4. Declarator construction, type formation, lookup, constant evaluation, and
   output are separate operations over the PA10 AST and semantic records.
   `TypeText` and `PrintScope` render only after analysis, in source/declaration
   insertion order, so deterministic output does not require reparsing or a
   second translation-unit scan.

Lookup uses local maps plus parent-scope walks and cycle-guarded
using-directive traversal. The normal path performs one preprocessing pass,
one token validation pass, one PA10 parse, one semantic traversal, and one
output walk per source. There is no reference-binary, host-tool, subprocess,
fixture, source-path, or test-name dependency in the production path. The only
file-audit warning remains the inherited PA6 declaration-header heuristic.

## Final Architecture Review

The review of checkpoint `f702b3d` found that the implementation behavior was
complete but the stage record was incomplete. The final cleanup now:

- creates named internal types for anonymous enums instead of inserting an
  empty-name binding, while retaining direct enumerator injection for unscoped
  enums and the checked anonymous-union spelling;
- preserves one canonical enum scope through ordinary opaque-to-definition
  completion and updates the canonical lookup scope for qualified member
  definitions;
- makes namespace and using-directive lookup safe for cyclic directive graphs
  and rejects namespace/alias name collisions at the declaration owner;
- removes unused semantic state without moving ownership into the driver or
  weakening the PA10 AST boundary; and
- records the final audit and validation evidence required before PA12.

The resulting PA11 stage preserves PA1--PA10, satisfies the README's scope,
lookup, declarator, constant, and deterministic-output requirements covered by
the assignment, and hands PA12 an owned semantic tree with retained type and
declaration facts.

## Next checkpoint group

Begin PA12 by extending the retained PA11 declaration/type facts into
expression and call semantics.  Preserve the PA11 dump and the through-PA11
report while adding that next semantic layer.
