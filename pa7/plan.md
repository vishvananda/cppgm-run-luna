# PA7 stage plan

## Baseline

The initial PA7 report found one shared stub failure across all 25 tests:
`nsdecl` returned `EXIT_NOT_IMPLEMENTED`, so the current count was 0/25.
The checked-in PA7 cases cover the complete handout surface in four related
groups rather than 25 independent behaviors.

## Remaining Work Map

1. **Translation-unit model and namespace emission (100--120):** replace the
   stub with the PA5 preprocessing boundary, parse declarations, represent the
   global/unnamed namespace, named namespace reopening, declaration order, and
   inline namespaces, and emit the required namespace tree.
2. **Typed declarations and declarator lowering (130--140):** keep canonical
   fundamental types in typed compiler state; parse cv-qualifiers, pointers,
   arrays, function declarators, and typedef declarations; lower each declared
   entity to the recursive PA7 type description.
3. **Persistent lookup and namespace imports (150, 190--290):** resolve
   typedefs and qualified names through enclosing namespaces, namespace aliases,
   using declarations/directives, unnamed namespaces, inline namespaces,
   reopened namespaces, and qualified out-of-namespace definitions while
   retaining entity order and suppressing duplicate imports.
4. **Function/array/reference semantics (300--370):** parse `void` and
   variadic parameter clauses, apply function/array parameter adjustment,
   collapse references, preserve top-level cv semantics where required, and
   print function entities with their complete function types.

## Checkpoint Scope

Implement the whole current PA7 in one coherent checkpoint.  The groups share
one parser and symbol/type model: namespace declarations create scopes,
declarations consume lookup results to build typed entities, and final output
walks those same scopes in first-declaration order.  Validation covers all 25
PA7 fixtures, the through-PA6 regression suite, the PA7 source audit, and a
clean committed worktree.

## Checkpoint Result

The whole selected scope is complete.  `nsdecl` now preprocesses each source
file through the shared PA5 front end, constructs a typed namespace/entity
model while parsing the PA7 declaration grammar, resolves qualified and
unqualified typedef/namespace names, applies using directives/declarations and
namespace aliases, merges reopened declarations, and emits canonical PA7
types in the required variable/function/namespace order.  The model also
handles inline and unnamed namespaces, array bounds, function and variadic
parameters, parameter adjustment, and reference collapsing.

Validation completed during this checkpoint:

- current PA7 report: **25/25 passed**;
- through-PA6 report: **267/267 passed**;
- `perl scripts/cppgm_file_audit.pl --stage pa7 --paths dev/src`: passed
  (one pre-existing non-fatal warning for `recog_parser_internal.h`).

The implementation is split between the small command-line/output entry
point in `dev/nsdecl.cpp` and the compiled semantic parser in
`dev/src/nsdecl_parser.cpp`, with the PA5 preprocessing dependencies explicit
in `dev/frontend_source_sets.mk`.

## Remaining Work Map

There is no remaining checked-in PA7 failure group.  Future work belongs to
later assignments: preserve this namespace/type model, then add only the next
PA's linkage or semantic facts while keeping the through-PA7 report green.

## Next Checkpoint Group

Begin PA8 by reusing the PA7 typed declaration and lookup state as the base for
the next assignment contract; no PA7 behavior remains unvalidated.
