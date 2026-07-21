# PA18 Final Stage Audit

## Audit Plan

The final audit reviewed the PA18 completion checkpoint at `4790179`
(`Complete PA18 template semantics and lowering`) and the integrated history
from the PA17 handoff through the template checkpoints: `28b92cd`, `2f110cb`,
`4945c13`, `9c1c459`, `fa85b18`, `49b6d03`, `2b50c64`, and `4790179`.  The
earlier checkpoint audit in this file was treated as historical evidence; its
174/222 result is superseded by the completed stage recorded in
`pa18/plan.md`.

The review covered:

1. `AGENTS.md`, `TESTING_AND_REFERENCES.md`, the PA18 assignment README, the
   PA13 LowIR contract, the complete PA18 plan, the PA17 final audit, the
   PA18 test sources and representative final-checkpoint witnesses, and the
   primary stage log at
   `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.
2. The cumulative PA18 source change set from `7feea1d` through `4790179`,
   including parser/AST metadata, PA11 semantic model and analyzer, PA12
   operator naming, PA14 collection/ABI/lifetime/value/lowering units, the
   PA18 template expander and rewrite units, and
   `dev/frontend_source_sets.mk`.
3. The complete checked-in PA18 `general` and `spec` suites, with focused
   review of type/default argument resolution, dependent lookup, explicit and
   implicit instantiation, nested/out-of-class definitions, template-backed
   operators, TLS/static storage, class prvalues, alignment, and the final
   defaulted-special-member/lifetime cases.
4. The complete path from PA10 syntax through PA11/PA12 semantic facts,
   PA18 AST materialization, PA14 collection and demand analysis, and PA13
   LowIR emission.  The audit specifically checked that template expansion
   produces ordinary declarations for the existing PA14-PA17 path rather than
   a second backend.
5. Correctness and diagnostics, ownership and pointer stability, deterministic
   order, demand-driven performance, source registration, shortcut indicators,
   the required file audit, the current-PA report, the through-stage report,
   build/diff checks, and final worktree state.

## Findings

The integrated PA18 implementation follows the intended monotonic compiler
architecture.  The parser remains the syntax boundary; template collection
and substitution produce ordinary AST declarations; PA11 owns scopes, types,
layout, dependent-name facts, and declaration validation; PA12/PA14 retain
overload, ABI, object, value, lifetime, and LowIR responsibilities; and PA17
polymorphic metadata continues to be consumed by the shared lowerer.

- `PA18TemplateExpander::Run` first validates the supported template
  diagnostics, collects lexical/template/function/alias facts, and transforms
  only the input that needs PA18 expansion.  `TemplateDefinition` records keep
  template declarations separate from generated declarations.  Canonical
  type spellings, default arguments, alias resolution, function-argument
  pattern matching, direct-call deduction, `decltype`, and operator-result
  inference are resolved before a specialization is materialized.
- `Instantiate` caches each specialization by declaration identity and
  canonical argument list, uses an active-specialization set for recursive
  dependencies, marks generated nodes with their primary and arguments, and
  records nested classes/member definitions separately.  Generated classes
  are inserted with dependency-aware ordering and generated functions are
  placed in the ordinary declaration/function regions.  Explicit class
  instantiation is represented by the same generated records with explicit
  instantiation metadata.
- `EmitPA14LowIR` passes the expanded AST to `Analyzer` and `PA14Lowerer`; it
  does not lower template source directly to text.  The lowerer rebuilds
  ordinary `Scope`, `Type`, `Binding`, `FunctionRecord`, and `GlobalRecord`
  facts, then uses the existing PA14-PA17 call, class, polymorphism, ABI,
  constructor/destructor, value, global, and LowIR paths.  An input with no
  PA18 trigger is returned without template expansion, preserving the
  PA17-only path.
- The final checkpoint at `4790179` closes two shared lifetime/ABI seams.  For
  implicit class-object cleanup, the lowering path materializes the typed
  declaration address before the fresh destructor address when the object or
  template specialization needs a lifetime anchor.  For defaulted copy/move
  construction, a direct base subobject uses the base constructor entry while
  ordinary non-base members retain their normal special-member entry.  This
  keeps complete-object and base-subobject calls distinct without adding a
  template-only lifetime backend.
- The PA14 driver closes demand frontiers in fixed-point passes: ordinary
  roots, demanded free template functions, hidden friends, base/member
  functions, global initialization/finalization, and any newly demanded
  helpers are emitted in the required order.  Restarting the typed member
  order after a newly emitted dependency prevents a dependent constructor,
  destructor, or nested helper from being emitted after an unrelated user.
- Ownership is explicit and stable.  `TypePtr` owns semantic type graphs;
  scopes own child scopes and use deque-backed bindings; the lowerer uses
  deque-backed function/global records and per-function variable plans; AST
  nodes are shared through `CPPGMAstNodePtr`.  Non-owning pointers retained by
  lookup, demand, and generated ABI records therefore refer to stable owners.
  Specialization, alias, active-recursion, nested-definition, and visited
  sets bound repeated work and prevent duplicate materialization.
- The source scan found no compiler-side reference-binary or host-compiler
  invocation, fixture/source-name acceptance branch, embedded answer, dummy
  LowIR, skipped frontend phase, interpreter/VM/trampoline substitute, or
  timeout-based implementation path.  The process/timeout code found in the
  repository test harness is not used by `cppgm++` to produce compiler output.
  No PA18 test or `.ref` fixture was changed.
- The required file audit passes.  It reports seven warning-only
  `bad-division` findings for the established implementation-bearing headers
  `pa11_semantics_analyzer.h`, `pa14_lowering.h`,
  `pa18_templates_collection.h`, `pa18_templates_rewrite.h`,
  `pa18_templates_rewrite_decltype.h`,
  `pa18_templates_rewrite_instantiate.h`, and
  `recog_parser_internal.h`; it reports no fatal finding or unregistered
  source path.  The PA18 implementation headers are included by the single
  owning template translation unit, while the earlier PA11/PA14 and parser
  header structure is inherited repository organization.  The warning status
  is documented rather than hidden or bypassed.

No additional correctness, ownership, performance, architecture, shortcut,
or file-audit blocker remains for the supported PA18 boundary.  The only
documentation defect found during this final review was that this audit file
still described the earlier `fa85b18` checkpoint instead of the completed
stage; this final audit corrects that evidence.

## Changes Made

- Retained the completed PA18 implementation in `4790179`, including
  specialization collection/materialization, default and deduced type
  arguments, dependent lookup, explicit instantiation, nested definitions,
  template-backed operators, typed alignment, demand-driven emission, TLS,
  class-prvalue lifetime, and final special-member ABI/lifetime cleanup.
- Added this final-stage audit with the checkpoint review, architecture and
  ownership findings, shortcut/file-audit assessment, and final validation
  evidence.
- Updated `pa18/plan.md` with Architecture Review and Final Architecture
  Review sections grounded in the current parser, template, semantic, and
  lowering implementation.
- No tests, grammar files, reference fixtures, or audit gates were modified.

## Validation

- `make test-pa18` — PASS, **222 / 222** PA18 tests; the empty
  `course/pa18` suite also passes (**0 / 0**).
- `make test-report-through-pa18` — PASS, **1430 / 1430** tests across
  **18 / 18** stages.  The primary stage log records
  `ALL TESTS PASSED SUCCESSFULLY! (1430 / 1430)`.
- `perl scripts/cppgm_file_audit.pl --stage pa18 --paths dev/src` — PASS;
  only the seven documented warning-only header-division findings remain.
- `make build` is covered by the through-stage target and completed
  successfully; `git diff --check` over the integrated PA18 change set passes.
- Final verification after the audit commit must show an empty
  `git status --short`.
