# PA17 Final Stage Audit

## Audit Plan

The final audit reviewed the PA17 completion checkpoint at `5603c19`
(`Implement PA17 polymorphism`) and the integrated cleanup that follows it.
At the start of this audit the checkout had no prior `pa17/audit.md`; the
PA16 final audit supplied the preceding ownership, lifetime, and source-set
handoff.  The review covered:

1. `TESTING_AND_REFERENCES.md`, the PA17 assignment README and its PA13 LowIR
   boundary, the complete PA17 plan, the PA16 final audit, and the primary
   stage log at `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.
2. The recent implementation history: `1ed0112`, `54bf1ed`, `61dfa9e`, and
   `5603c19`, plus every source file changed by the PA17 checkpoint and final
   audit cleanup.
3. The complete checked-in PA17 `general` and `spec` suites, with focused
   review of non-polymorphic-base layout, inherited and overloaded slots,
   pure/final/override validation, pointer/reference dispatch, qualified
   calls, vtable ordering, and virtual destructor transitions.
4. The full typed path from PA10 syntax through PA11 semantic class facts,
   PA12 call resolution, PA14 collection/planning/lowering, and PA13 LowIR
   emission, including the shared PA16 value and lifetime consumers.
5. Correctness, subobject ownership, pointer stability, deterministic output,
   demand/emission performance, shortcut indicators, source registration, the
   required file audit, the local PA17 suite, the through-stage report, build,
   diff check, and final worktree state.

## Findings

The completed PA17 implementation follows the intended staged compiler
architecture.  Syntax remains an AST boundary; semantic analysis owns
effective virtual slots and class layout; PA14 owns overload/call planning,
ABI, lifetime, and LowIR lowering; and the new polymorphic unit contributes
typed vtable/RTTI and virtual-dispatch behavior to that shared backend.

- `VirtualMethodInfo` records the effective inherited declaration, owner,
  binding, destructor/pure/final state, and function shape.  The analyzer
  validates override and final rules and covariant pointer/reference returns
  before any LowIR is emitted.
- Root polymorphic classes and first-polymorphic derived classes reserve the
  vpointer at complete-object offset zero.  `direct_base_offset` records the
  non-polymorphic base adjustment when necessary, and `AdjustBaseAddress`
  applies the accumulated typed adjustment to member access, base calls,
  construction/destruction, conversions, and value-special-member copies.
- The vtable model emits declaration-ordered ordinary slots and expands a
  virtual destructor into adjacent complete/deleting entries.  Both ordinary
  calls and delete use the same expanded-slot accounting; explicit base
  qualification remains a direct call.
- Constructor and destructor vpointer writes, base-entry transitions, pure
  virtual entries, RTTI globals, pointer/reference/dereferenced dispatch, and
  PA16 class value/lifetime behavior remain in their existing typed lowering
  responsibilities rather than being duplicated in a fixture-oriented path.
- Long-lived `FunctionRecord` and `GlobalRecord` objects use stable deque
  ownership; semantic scopes use stable binding storage; `TypePtr` owns class
  metadata; and virtual records retain pointers to those stable semantic
  entities.  Ordered emission converts pointer sets to semantic-name order,
  so output and generated helper order do not depend on heap addresses.

The audit found and fixed four checkpoint-level seams:

- A first vpointer could overlap a non-polymorphic direct base because the
  pointer was placed after the base while base access still used zero.
- Expanded vtable indices were wrong for calls after a destructor declared in
  the middle of the semantic slot list, and delete used a hard-coded
  destructor position.
- Direct iteration of raw `Type*` sets made vtable/RTTI/helper order depend on
  allocation addresses.
- The demand pass matched virtual member calls by source name alone, so an
  unrelated same-name member could mark a definition as needed; typed receiver
  and slot resolution now supplies the dependency, including `this->` calls.

No test-name branch, fixture-path answer, reference-binary or host-compiler
subprocess, alternate interpreter/backend, dummy output, skipped frontend
phase, or repeated full-program walk was found.  The file audit has no fatal
finding.  It retains the three warning-only inherited header-division findings
for `pa11_semantics_analyzer.h`, `pa14_lowering.h`, and
`recog_parser_internal.h`, consistent with the PA15 and PA16 audits.

## Changes Made

- Added the semantic `Type::direct_base_offset` fact and used it to keep the
  first vpointer at offset zero while preserving correct base/member addresses.
- Made base adjustment cumulative and shared it with value-special-member base
  copies.
- Added expanded virtual-slot accounting and a common virtual deleting-
  destructor slot lookup for calls and delete lowering.
- Replaced raw-pointer iteration in polymorphic preparation and global emission
  with deterministic semantic ordering.
- Replaced name-only virtual-call demand discovery with typed receiver and
  virtual-slot resolution, including safe `this->` handling.
- Added this final audit and the architecture review sections to `pa17/plan.md`.
  No tests or `.ref` fixtures were changed, and the PA1-PA16 implementation
  remains reused and covered by the through-stage gate.

## Validation

- `make -j1 build` — PASS; all shared compiler tools and `cppgm++` linked.
- `make -C pa17 test` — PASS, **25 / 25** PA17 tests.
- `make test-report-through-pa17` — PASS, **1208 / 1208** tests across
  **17 / 17** stages.
- `perl scripts/cppgm_file_audit.pl --stage pa17 --paths dev/src` — PASS;
  only the three documented warning-only header-division findings remain.
- `git diff --check` — PASS.
- Focused generated-LowIR checks confirmed that a non-polymorphic base is
  adjusted after a first vpointer, a virtual call after an ordinary slot and
  destructor pair uses the expanded entry index, and repeated compilation of
  the same polymorphic input is byte-identical.  These checks did not add
  tests or references to the repository.
- Final handoff requires the cohesive audit commit and an empty
  `git status --short`.
