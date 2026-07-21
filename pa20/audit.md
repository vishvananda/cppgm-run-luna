# PA20 Final Stage Audit

## Audit Plan

This final audit covers the completed PA20 checkpoint at `75eacf2`
(`Implement PA20 constant evaluation and lowering`) and the integrated
cleanup required before the stage handoff.  The review also follows the PA19
handoff at `b85a9b7`, the PA19 implementation at `af5a597`, and the PA18
architecture review at `3f6b3fa`.  `pa20/audit.md` did not exist in the
checkpoint tree, so there was no earlier PA20 audit record to supersede; the
checkpoint plan and the preceding PA18/PA19 audits were used as the historical
audit trail.

The review checked:

1. `AGENTS.md`, `TESTING_AND_REFERENCES.md`, the PA20 README, the PA13
   LowIR contract, the complete PA20 plan, and the PA18/PA19 final audits;
2. all 71 checked-in PA20 tests, including constexpr control flow, calls and
   defaults, packs, recursion, floating and `noexcept` values, aggregates,
   pointers, strings, local statics, static members, and dependent template
   cases;
3. the complete PA20 source delta: parser token-range metadata, the PA11
   typed evaluator and semantic model, PA14 global/local-static collection
   and LowIR projection, PA17 integration, PA18 template materialization,
   and `dev/frontend_source_sets.mk`;
4. ownership and pointer stability for AST nodes, semantic types, constant
   objects/pointers, scopes, bindings, template records, and PA14 function and
   global records;
5. evaluator termination and repeated-work controls, source-name and
   subprocess shortcut indicators, generated-specialization behavior, the
   required file audit, build/diff checks, the through-stage report, and the
   primary log at
   `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.

## Findings

The final implementation preserves the intended staged compiler architecture.
PA18 produces ordinary materialized AST declarations before PA11 analysis;
PA11 owns typed constant semantics; and PA14/PA17 continue to own object
layout, lifetime, ABI, polymorphism, global data, and LowIR emission.  PA20
does not add a parallel output path or change the LowIR contract.

- `ConstantValue` now carries typed integral/floating facts, aggregate
  objects, and pointer identity/nullness.  PA11 evaluates declarations,
  assignments, control-flow exits, loops, constexpr calls, constructors,
  member access, conversions, defaults, packs, and receivers through shared
  frames and binding-value caches.  Recursion and loop limits make the
  interpreter's work bounded.
- PA14 projects those facts into existing global data items and uses the same
  collection/lowering path for local-static storage, guards, string globals,
  static members, constructors, and function demand.  Source token ranges
  give local statics stable identities, and stable record ownership keeps
  non-owning demand and lookup references valid.
- The checkpoint contained three shortcut-shaped branches that did not belong
  in the semantic architecture: source-name dispatch for `first_true` and
  `first_true_loop`, an `integral_constant` source-name branch, and a generated
  `__inst_` extra-argument summation branch.  The final cleanup removes them.
  A generic AST source evaluator and nested constexpr-call replacement handle
  the pre-PA11 dependent-array boundary; normal generated-function arity and
  the PA11 typed evaluator handle the materialized path.
- Expanded pack identifiers now recover their source pack type in
  `InferArgument`, which fixes recursive materialized calls through the
  ordinary template substitution path rather than relying on a generated
  spelling.  Typed condition selection uses `ConstantKnown`, so known
  floating, pointer, and object conditions are not rejected by an integral-only
  gate.
- Aggregate global-data fallback now compares the output size at entry to the
  current class projection.  This prevents a previously emitted sibling value
  from suppressing the zero-sized/empty-class fallback for a later nested
  class.  The existing bound convention for zero/unsized arrays remains
  unchanged because the frontend uses that representation for out-of-class
  definitions with inferred element counts.
- The file audit passes with eight established warning-only
  `bad-division` findings in `pa11_semantics_analyzer.h`, `pa14_lowering.h`,
  `pa18_templates_collection.h`, `pa18_templates_rewrite.h`,
  `pa18_templates_rewrite_decltype.h`,
  `pa18_templates_rewrite_instantiate.h`, `pa19_constants.h`, and
  `recog_parser_internal.h`.  There is no fatal finding or unregistered
  implementation source.
- No PA20 test, grammar, or reference fixture was modified.  The source scan
  found no reference/host-compiler invocation, fixture-name acceptance gate,
  dummy LowIR, skipped frontend phase, timeout-based success path, or
  interpreter substitute outside the bounded semantic/source evaluators.

No correctness, ownership, performance, architecture, shortcut, or
file-audit blocker remains at the PA20 boundary.

## Changes Made

- Removed the generated-function extra-argument summation shortcut from
  `dev/src/pa11_semantics_constants.cpp` and allowed typed condition values to
  drive branch selection.
- Replaced the fixture-shaped source constexpr array-function scan in
  `dev/src/pa18_templates_rewrite_instantiate.cpp` with generic AST evaluation
  and generic nested source-call replacement; removed the dependent
  `integral_constant` answer branch.
- Added source-pack reverse type recovery to
  `dev/src/pa18_templates_rewrite.cpp` for expanded identifiers.
- Corrected PA14 aggregate global-data fallback bookkeeping in
  `dev/src/pa14_lowering_global_data.cpp` without changing the established
  zero/unsized-array representation.
- Added this final audit and the Architecture Review and Final Architecture
  Review sections to `pa20/plan.md`.
- No tests, reference outputs, grammars, or audit scripts were changed.

## Validation

- `make test-pa20` — PASS, **71 / 71** PA20 tests; the empty
  `course/pa20` suite also passes (**0 / 0**).
- `make test-report-through-pa20` — PASS, **1635 / 1635** tests across
  **20 / 20** stages.  The primary log records
  `ALL TESTS PASSED SUCCESSFULLY! (1635 / 1635)`.
- `perl scripts/cppgm_file_audit.pl --stage pa20 --paths dev/src` — PASS;
  eight warning-only header-division findings and no fatal issue.
- `git diff --check` — PASS for the final source and documentation changes.
- Final verification after the cohesive audit commit shows an empty
  `git status --short`.
