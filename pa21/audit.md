# PA21 checkpoint audit

## Scope Reviewed

Reviewed the latest Checkpoint Scope and complete failure map in
`pa21/plan.md`, the PA21 contract in `pa21/README.md`,
`TESTING_AND_REFERENCES.md`, the changed source files and source-set entries,
recent commits `297acef`, `a74b4e5`, `a916cd1`, `64aa678`, `6b2c6fc`, and
`ff9d4f3`, and the supplied full primary log at
`/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`.

The landed checkpoint is the hidden-friend namespace/ADL increment: replayed
friend function templates retain an unqualified generated declarator so PA14
can assign the surrounding namespace symbol, and normalized `friend`
specifiers remain semantic facts rather than becoming types.  The audit also
covers the supporting PA18 replay changes required by that checkpoint:
structured function-signature propagation, typed lookup indexes, bounded
dependent replay, integral-evaluation phase separation, and using-imported
member-template declarations.

## Findings

- The full compiler pipeline remains active: preprocessing, tokenization,
  parsing, PA11 semantic analysis, PA14 lowering, PA18 replay, and ordinary
  LowIR emission all run.  There is no skipped phase, dummy or embedded
  output, reference/host compiler invocation, interpreter/VM/trampoline
  substitute, source/test-specific acceptance gate, timeout workaround, or
  fallback success path.

- The source prototype retained for a using-imported member template is a
  real dependent declaration: it preserves the collected result specifier,
  declarator, template parameters, and semantic identity, while omitting only
  the source body at the declaration boundary.  PA11 processes that
  declaration, PA14 processes the resulting declaration node, and PA18
  materializes the concrete definition through `Instantiate`; it is not dummy
  output or a skipped semantic/lowering path.

- Function types are carried as `FunctionSignature` values through deduction,
  instantiation, and parameter replay.  The former reserved string markers and
  fake function payloads are gone.  Call, cast, binary, and `decltype`
  fallbacks now accept only known typed spellings, so arbitrary source text
  cannot manufacture a successful semantic result.

- Dependent replay defers only identifiers registered as template parameters;
  concrete local classes and known aliases materialize normally.  Lookup facts
  are retained in `definitions_by_name_`, namespace-export indexes, and
  a class-scoped collection-time using-member index.  Function-local and
  namespace-level using declarations do not retain unrelated source template
  bodies.  Member-template lookup no longer performs registry-wide scans on
  its normal indexed paths, and no emitted LowIR or generated text is reparsed
  to recover semantic facts.

- Function-argument inference and integral evaluation are split into focused
  preparation, matching, completion, normalization, special-form, known-value,
  and fallback phases.  This removes the oversized hot routines while keeping
  pack arity, defaults, reference adjustment, overload deferral, and typed
  constant values explicit.

- No file-audit bypass, hidden implementation fragment, weakened check, or
  code moved to an unchecked path remains.  The specialization matcher is an
  audited `.cpp` translation unit listed in `dev/frontend_source_sets.mk`.
  Earlier assignments remain clean, and the current PA remains at the landed
  checkpoint baseline.

## Changes Made

- Removed fake function markers and the temporary template-entity-only path;
  propagated typed function signatures through concrete replay.
- Added strict known-type checks to semantic fallbacks and corrected dependent
  replay to distinguish unresolved template parameters from concrete classes.
- Replaced normal full-registry member/template lookup walks with collection-
  time indexes, scoped the using-member index to class contexts, and retained
  a real dependent prototype for using-imported member templates.
- Split function inference and integral evaluation into auditable helpers,
  kept the specialization implementation in the checked source set, and
  refreshed the current failure map and next substantial checkpoint in
  `pa21/plan.md`.

## Validation

- `make -j2`: passed.
- Required prior-through check (`make test-report-through-pa20`):
  **1635/1635 passed**.
- `perl scripts/cppgm_file_audit.pl --stage pa21 --paths dev/src`: passed with
  warning-only findings and no fatal file, size, or source-set finding.
- `make test-report ACTIVE_TEST_REPORT_PAS='pa21'`: **146/215 passed** with
  **69 failures**, matching the complete map in `pa21/plan.md`.  This equals
  the checkpoint baseline and remains above the turn-start 119/215 baseline;
  earlier PAs still pass.
- Focused hidden-friend ADL/operator, using-imported member-template, and PA18
  inference regressions passed.
- `git diff --check`: passed.  The implementation is ready for the next
  member/friend owner-replay checkpoint group.
