# PA21 checkpoint audit

## Scope Reviewed

Reviewed the latest Checkpoint Scope and failure map in `pa21/plan.md`, the
PA21 contract in `pa21/README.md`, `TESTING_AND_REFERENCES.md`, the changed
source and source-set entries, recent commits `ff9d4f3`, `6b2c6fc`, and
`64aa678` (with `07fc383`, `c4e148e`, and `a2afd2e` for preceding ownership
context), and the complete PA21 log supplied at
`/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.

The landed checkpoint is the dependent typed-value and generated-layout
increment: recursive non-type/pack replay, declarator-shell preservation,
empty-pack completion, concrete template-member replay, multi-base layout and
owner adjustments, and deferred local-class layout materialization.  The
audit-turn baseline was 118/215 PA21 tests.

## Findings

- The complete compiler pipeline remains active: preprocessing, tokenization,
  parsing, semantic collection/rewrite, and ordinary LowIR lowering all run.
  There is no skipped phase, dummy or embedded output, reference/host compiler
  invocation, interpreter/VM/trampoline substitute, source/test-specific
  acceptance gate, fallback success path, or timeout workaround.

- One checkpoint-generated LowIR result was structurally invalid.  A
  using-directive was flattening imported function templates into scalar name
  substitutions, so a local object named `next` was rewritten as `std::next`.
  Function entities are now excluded from scalar substitution, while function
  lookup remains available through the typed definition index.  The focused
  regression now passes and the invalid-LowIR entry is gone from the full
  report.

- Concrete enclosing ownership had been encoded as the reserved fake
  identifier `__PA18_CONCRETE_OWNER__` inside the ordinary substitution map.
  That stringly fact has been replaced by a scoped typed owner context,
  explicit owner-binding recovery, owner/definition matching that handles
  templated owner spellings, and concrete alias registration.  Nested replay
  preserves the owner without leaking it into unrelated target classes.

- The specialization-ordering implementation was a class-body `.inc`
  fragment included from a header and therefore outside the file-audit source
  set.  It is now a normal `pa18_templates_rewrite_specialization.cpp`, listed
  in `dev/frontend_source_sets.mk`; the fragment was removed.  No code was
  hidden in an unchecked path and no file-size or audit check was weakened.

- Using-directive replay no longer performs a registry-wide definition walk:
  class contexts use an ordered prefix range, and collection builds a typed
  namespace-export index.  Existing `definitions_by_name_`, pack-name, and
  constant-member indexes remain the owners of their respective facts.  The
  owner fallback examines bounded source-definition metadata on a cache miss;
  it does not reparse emitted text or LowIR.

- `direct_bases` and `direct_base_offsets` are the canonical multi-base
  representations.  `direct_base` and `direct_base_offset` are compatibility
  projections for earlier single-inheritance consumers; their writes remain
  centralized in class processing/layout, so downstream code does not own a
  second independently updated base graph.  This preserves earlier PA
  behavior while making the new multi-base facts available to layout and
  lowering.

## Changes Made

- Added the typed scoped owner context and owner-binding helper, removed the
  reserved owner marker, and retained concrete-owner alias registration.
- Fixed using-directive scalar substitution and replaced its repeated scans
  with collection-time namespace-export indexing and ordered class-context
  lookup.
- Moved specialization matching into an audited `.cpp` translation unit and
  registered it in the frontend source set.
- Updated this audit and refreshed the concise 96-failure map and next
  checkpoint group in `pa21/plan.md`.

## Validation

- `make -C dev cppgm++`: passed.
- Focused owner-replay probes for alias rebinding and dependent member-class
  partial specialization: passed; the local-value/using-directive invalid-
  LowIR regression: passed.
- `make test-report ACTIVE_TEST_REPORT_PAS='pa21'`: **119/215 passed**;
  96 semantic/LowIR failures remain, with no timeout or invalid-LowIR result.
  The command is nonzero because PA21 is not complete, but the result is above
  the 118/215 audit-turn baseline.
- Required prior-through gate (`make test-report-through-pa20`): **1635/1635**.
- `perl scripts/cppgm_file_audit.pl --stage pa21 --paths dev/src`: passed with
  warning-only header-division findings and no fatal finding.
- `git diff --check`: passed.  The cohesive audit fixes and documentation are
  committed together, and the final worktree is clean.
