# PA23 checkpoint audit

## Checkpoint 4 audit — 2026-07-29

### Scope Reviewed

- The latest `Checkpoint Scope` and Checkpoint 3 result in `pa23/plan.md`.
- The PA23 assignment contract in `pa23/README.md`, the repository testing and
  reference rules, and the focused PA21/PA22/PA23 witnesses for this replay
  path.
- Recent commits `b7f7787` (typed SFINAE checkpoint), `bc58f85` (forwarding
  pack checkpoint), `2608080` (reference-category checkpoint), and `af6768a`
  (PA22 architecture audit).
- The changed implementation files under `dev/src`, especially the PA18
  collection, typed replay, member lookup, selection, specialization, and
  materialization helpers.
- The full primary checkpoint log at
  `/home/vishvananda/work/.ralph/luna-gpt-5.6-luna-ultra/last-test.log`, plus
  a clean `b7f7787` comparison checkout.

### Findings

- The checkpoint remains on the real compiler pipeline required by the PA23
  README.  There is no skipped compiler phase, dummy or embedded output,
  interpreter/VM/trampoline substitute, reference-binary invocation, or
  source/test-specific acceptance gate in the audited changes.
- Recursive semantic re-entry is now represented as scoped substitution
  failure.  Class-specialization matching and class-template selection use
  normalized semantic identities and typed registry ownership; an exact
  re-entry does not succeed by silently returning a primary-template fallback.
- No timeout cap, timeout retry, or timeout-only acceptance path was added.
  The clean checkpoint compiler independently reproduces exit status 124 for
  both reentrant static-query inputs, and the final harness still reports only
  those two timeout fixtures.
- Owner and argument facts stay in typed substitutions, AST declaration kinds,
  `TemplateParameter` kinds, and the specialization registry.  The changed
  member path no longer uses `enable_if`/`disable_if` spelling as a dependency
  gate, direct nested-member recovery requires an AST-declared member and a
  matching non-type parameter, and changed typedef probes use AST specifiers
  rather than reparsing emitted text.
- Materialized template bases are protected only when the typed specialization
  registry confirms their ownership.  The protection helper tokenizes each
  source spelling once, eliminating the prior full-spelling scan for every
  substitution binding.  Selection arguments are normalized once and the
  repeated qualified-path normalizer is shared instead of duplicated in a hot
  lookup function.
- No new full-suite walk, dummy success path, unchecked implementation
  fragment, weakened check, or fixture/ref modification was introduced.  The
  helper definitions were moved into existing source modules when the file
  audit exposed the checkpoint line-limit boundary; no new source file or
  unchecked path was used.
- The final current-PA failure set is exactly the clean checkpoint set minus
  `spec/400-function-type-pack-template-argument.t`; there are no current-only
  regressions.  PA23 is 285/396 versus the turn-start/checkpoint baseline of
  284/396, while all earlier assignments pass through PA22.

### Changes Made

- Added shared semantic qualified-path normalization and scoped selection and
  specialization identities in the PA18 collection/selection paths.
- Added registry-backed materialized-base protection to dependent owner and
  specialization replay, with a single token scan on the hot path.
- Replaced changed text-based typedef/dependency probes with AST declaration
  and template-parameter checks, and constrained concrete nested-member replay
  to an actually declared member with matching typed arity.
- Removed the helper-name dependency gate from member-owner lookup and removed
  the helper-name gate from integral specialization matching.
- Kept the changes within the existing PA18 source ownership modules and
  updated this audit and the PA23 remaining-work map.  No tests or `.ref`
  files were changed.

### Validation

- `make -C dev cppgm++` — pass.
- `make test-report ACTIVE_TEST_REPORT_PAS='pa23'` — expected nonzero report
  status with `285 / 396` passing; 76 exit-status failures, 33 LowIR
  mismatches, and 2 inherited timeouts.
- Required prior-through test command — pass:
  `===== ALL TESTS PASSED SUCCESSFULLY! (2100 / 2100) =====`.
- `perl scripts/cppgm_file_audit.pl --stage pa23 --paths dev/src` — pass
  with 13 existing warnings and no fatal size/audit issue.
- Focused PA21/PA22 witnesses and the focused PA23 owner/SFINAE cluster — all
  pass.  The two reentrant timeout witnesses remain unchanged stress inputs;
  they were not made to pass by a timeout workaround.
- The complete current-PA failure set below contains 111 fixtures.  The
  checkpoint therefore preserves earlier assignments and stays above its
  PA23 baseline while leaving the next typed deferred-SFINAE group selected in
  `pa23/plan.md`.
- After committing these cohesive audit fixes, `git status --short` is empty.

### Remaining Work Map

This is a fixture behavior map for the next implementation checkpoint, not
open checkpoint-audit debt.  The next group is the two dependent-typename
fixtures bundled with non-timeout deferred-SFINAE, detector/deleted-candidate,
cached/no-eager, and dependent-member-result cases.  The timeout pair remains
as stress coverage for the same typed query-identity path.

The full current-PA failure set is:

```text
general/100-current-specialization-member-body-cast-compare.t
general/100-default-nontype-qualified-function-lookup.t
general/100-dependent-bool-partial-static-value-storage.t
general/100-dependent-qualified-nontype-base-argument.t
general/100-direct-namespace-wins-over-using-directive.t
general/100-explicit-function-specialization-overload-parameter-match.t
general/100-explicit-specialization-out-of-class-ctor-replay.t
general/100-explicit-specialization-pointer-member-definition.t
general/100-function-type-not-pointer-partial-specialization.t
general/100-inherited-using-alias-out-of-class-specialization-member.t
general/100-intermediate-type-transform-value-nontype.t
general/100-local-qualified-argument-replay.t
general/100-member-template-specialization-return-prefers-member-call.t
general/100-nested-template-static-value-nontype-expression.t
general/100-nontype-function-parameter-adjustment.t
general/100-selected-specialization-special-member-body.t
general/100-sizeof-call-result-nontype-template-argument.t
general/100-structured-bool-boost-convertible-mpl-overload.t
general/100-using-directive-template-member-type-typedef.t
general/200-adl-explicit-template-id-call.t
general/200-class-partial-specialization-no-derived-base-deduction.t
general/200-constructor-template-parameter-shadows-instantiated-type.t
general/200-empty-pack-member-template-owner-key.t
general/200-function-template-named-parameter-sfinae.t
general/200-function-template-reference-cv-alias-partial-order.t
general/200-function-template-template-parameter-deduction.t
general/200-member-function-template-address-explicit-pack.t
general/200-member-operator-template-reference-pattern-partial-order.t
general/200-member-template-implicit-instantiation-not-overload.t
general/200-nested-template-id-partial-specialization-deduction.t
general/200-pack-not-at-end-nondeduced-bad.t
general/200-template-template-qualified-default-arg-deduction.t
general/300-array-qualified-member-type-sfinae.t
general/300-constructor-template-const-ref-enable-if-conversion.t
general/300-current-specialization-constructor-template-canonical-owner.t
general/300-decltype-member-template-callable-pack-result-cache.t
general/300-dependent-bool-base-trait-type-argument.t
general/300-hidden-friend-current-specialization-enable-if.t
general/300-qualified-explicit-template-alias-return-sfinae.t
general/400-current-specialization-display-name-member-alias.t
general/400-dependent-alias-nontype-sequence-filter.t
general/400-dependent-nontype-member-template-owner.t
general/400-elaborated-type-template-arg-false-branch.t
general/400-elaborated-type-template-arg-true-branch.t
general/400-explicit-function-template-type-arg-drops-nontype-overload.t
general/400-function-type-pack-out-of-class-constructor.t
general/400-function-type-partial-specialization-fixed-arity-over-pack.t
general/400-function-type-tail-pack-recursive-specialization.t
general/400-inherited-qualified-member-template-type.t
general/400-local-class-default-member-variable-template-nontype-type.t
general/400-member-alias-template-template-owner-argument.t
general/400-member-template-result-pack-preserves-nested-function-pointer-owner.t
general/400-member-variable-template-leaf-sfinae.t
general/400-nested-member-template-base-param-shadow-value.t
general/400-nonmember-template-compound-assignment-const-lhs.t
general/400-out-of-class-ctor-using-imported-member-template.t
general/400-out-of-class-partial-member-template-owner-parameter-alias.t
general/400-qualified-member-variable-template-class-value.t
general/400-recursive-member-template-concrete-owner.t
general/400-static-cast-rvalue-ref-skips-conversion-operator.t
general/400-unused-static-member-template-return-type.t
general/400-variable-template-specializations.t
general/500-array-type-argument-sfinae-static-value.t
general/500-const-reference-alias-default-sfinae.t
general/500-constructor-sfinae-owner-pack-function-type.t
general/500-dependent-function-type-pack-expansion-ctor-init.t
general/500-dependent-qualified-member-template-result-bool.t
general/500-dependent-qualified-sizeof-static-member.t
general/500-dependent-std-or-enable-if-defers.t
general/500-dependent-typename-enable-if-candidate.t
general/500-dependent-typename-member-enable-if-return.t
general/500-member-template-conditional-alias-trailing-return.t
general/500-mp11-append-alias-template-sfinae.t
general/500-nontype-alias-reinstantiation-structural-state.t
general/500-out-of-class-member-template-dependent-owner-type.t
general/500-recursive-qualified-member-template-bool-arg.t
general/500-reentrant-static-query-callable-enable-if-cache.t
general/500-reentrant-static-query-enable-if-partial.t
general/500-source-namespace-base-sfinae-chain.t
general/500-type-pack-rejects-value-pack-bad.t
spec/100-dependent-template-id-qualified-member-source-owner.t
spec/100-explicit-instantiation-after-explicit-specialization-no-effect.t
spec/100-explicit-instantiation-class-prior-member-definitions.t
spec/100-explicit-specialization-out-of-class-ctor-replay.t
spec/100-explicit-specialization-out-of-class-member-emits.t
spec/100-extern-template-member-function-declaration.t
spec/100-extern-template-static-data-declaration.t
spec/100-function-template-nontype-function-pointer-call.t
spec/100-function-template-nontype-function-pointer-specialization-call.t
spec/100-local-member-call-constructor-template-instantiation.t
spec/100-nontype-function-pointer-argument.t
spec/100-out-of-class-conversion-operator-definition.t
spec/100-partial-specialization-member-primary-param-name.t
spec/100-sizeof-union-type-nttp.t
spec/200-defaulted-class-template-argument-pack-prefix-deduction.t
spec/200-inherited-template-param-shadow-forward.t
spec/300-constructor-default-pack-partial-ordering.t
spec/300-deleted-function-template-callee-detector-sfinae.t
spec/300-deleted-function-template-expression-sfinae.t
spec/400-class-template-nttp-scope-value.t
spec/400-conversion-function-template-call-argument.t
spec/400-conversion-function-template-copy-init.t
spec/400-conversion-function-template-selection.t
spec/400-defaulted-nested-class-argument-partial-specialization.t
spec/400-defaulted-template-arg-partial-base-completion.t
spec/400-dependent-member-template-call.t
spec/400-explicit-pack-type-argument-uses-bound-type.t
spec/400-explicit-type-arg-dependent-qualified-member-template-id.t
spec/400-qualified-member-template-id-bool-constant.t
spec/400-template-template-member-alias-owner-shadow.t
spec/500-conditional-alias-index-sequence-member-template-call.t
```
