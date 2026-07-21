# PA19 checkpoint plan

## Turn-start baseline

The required `make test-report ACTIVE_TEST_REPORT_PAS='pa19'` baseline is
**31/134 passing**, with **103 current-PA failures**.  Earlier assignments are
expected to remain covered by the through-PA18 report.

## Remaining Work Map

The complete current-PA failure set is grouped by the shared compiler behavior
that must change.  A test appears in exactly one group below.

### A — integral constant values, non-type arguments, and constant bindings (44)

These failures need a typed integral-value path from expression evaluation and
ordinary constant lookup through template argument binding, type/value
rewriting, and `static_assert`.

`general/100-anonymous-enum-nontype-template-argument`,
`general/100-cast-nontype-template-argument`,
`general/100-char32-constant-promotion-unsigned`,
`general/100-character-literal-constant-eval`,
`general/100-current-instantiation-qualified-static-value-nontype`,
`general/100-dependent-bool-trait-nontype-argument`,
`general/100-dependent-member-value-comparison-argument`,
`general/100-dependent-nontype-functional-cast-body-check`,
`general/100-dependent-nontype-parameter-type-default`,
`general/100-enum-constant-function-style-init`,
`general/100-inherited-static-member-value`,
`general/100-integral-constant-boundary-value-stress`,
`general/100-integral-constant-static-value`,
`general/100-integral-constant-template-argument`,
`general/100-integral-type-promotion-static-assert-stress`,
`general/100-long-long-nontype-template-argument`,
`general/100-non-type-class-template`,
`general/100-non-type-function-template`,
`general/100-nontype-expression-template-argument`,
`general/100-nontype-qualified-enum-sum`,
`general/100-nontype-template-argument-static-member-no-storage`,
`general/100-qualified-template-head-shadowed-by-nontype`,
`general/100-qualified-template-id-nontype-argument-scope`,
`general/100-reference-member-constant-alias`,
`general/100-reference-static-constexpr-member-replay`,
`general/100-rooted-template-id-type-argument-static-value`,
`general/100-static-constexpr-member-call-initializer`,
`general/100-template-alignas-nontype-argument`,
`general/100-template-alignas-static-member-value`,
`general/100-template-static-assert`,
`general/100-template-static-constant-minimum-chain`,
`general/100-type-template-less-than-expression-argument`,
`general/100-wide-string-literal-static-assert`,
`general/200-user-defined-integer-literal-template`,
`general/300-qualified-static-array-member-sizeof`,
`general/400-defaulted-template-member-call-rematerialization`,
`spec/100-dependent-nontype-template-parameter-type`,
`spec/100-dependent-relational-static-cast-template-default`,
`spec/100-inherited-static-member-value`,
`spec/100-integral-constant-static-value`,
`spec/100-integral-function-nontype-parameter`,
`spec/100-local-const-integral-nontype-argument`,
`spec/100-long-long-nontype-template-argument`, and
`spec/100-template-static-assert`.

### B — dependent lookup, names, defaults, and stale semantic state (23)

These failures need dependent qualified lookup, current-instantiation facts,
default argument rewriting, and refresh of cached template declarations.

`general/100-defaulted-nontype-class-alias-rewrite`,
`general/100-defaulted-nontype-expression-syntax-rewrite`,
`general/100-defaulted-trailing-variable-partial-specialization`,
`general/100-dependent-static-constant-member-comparison`,
`general/100-dependent-typename-template-argument-static-assert`,
`general/100-dependent-typename`,
`general/100-explicit-type-argument-class-layout-completion`,
`general/100-function-template-dependent-alias-parameter-overloads`,
`general/100-local-decltype-callable-type-argument`,
`general/100-local-variable-template-keeps-concrete-class-instantiation`,
`general/100-nontype-conversion-operator-dependent-template-id`,
`general/100-structured-nothrow-invocable-cache-default`,
`general/100-variable-template-id-expression`,
`general/300-template-parameter-class-member-object`,
`general/300-variable-template-specialization-empty-template-id-argument`,
`general/400-stale-class-specialization-default-argument-refresh`,
`general/400-stale-function-template-instantiation-lookup`,
`spec/100-trivial-template-temporary-base-argument-no-dtor`,
`spec/100-qualified-function-template-call`,
`spec/100-qualified-template-member-type-cast-static-assert`,
`spec/100-rvalue-pointer-derived-template-call`,
`spec/100-type-equivalence-default-argument`, and
`spec/100-unused-member-body-static-assert-not-instantiated`.

### C — parameter packs, expansions, and pack-dependent declarations (24)

These failures need typed pack collections, expansion in declarations/calls and
bodies, empty-pack handling, and `sizeof...`.

`general/200-defaulted-decltype-empty-pack-instantiation`,
`general/200-defaulted-decltype-pointer-placement-new`,
`general/200-empty-base-pack-expansion-sizeof`,
`general/200-function-template-leading-fixed-and-pack-call`,
`general/200-function-template-named-pack-call`,
`general/200-function-template-pack-call`,
`general/200-function-template-pack-forward-call`,
`general/200-function-template-pack-ref-return`,
`general/200-inline-dependent-pack-result-type`,
`general/200-instantiated-function-fixed-prefix-pack-tail`,
`general/200-member-init-covarying-type-index-pack`,
`general/200-pack-expanded-base-instantiation`,
`general/200-pack-expanded-dependent-bool-nontype`,
`general/200-pack-expanded-explicit-nontype-expression-call`,
`general/200-pack-expansion-aggregate-brace-init`,
`general/200-pack-expansion-array-unknown-bound`,
`general/200-qualified-function-template-constructor-function-id-arg`,
`general/200-sizeof-function-parameter-pack`,
`general/200-template-parameter-pack-collection`, and
`general/200-user-defined-cooked-integer-literal`,
`spec/200-call-pack-expansion-template-type-pack-only`,
`spec/200-function-template-leading-fixed-and-pack-call`,
`spec/200-function-template-pack-call`, and
`spec/200-function-template-pack-forward-call`.

### D — explicit specialization, generated records, and LowIR shape (11)

These failures need specialization selection/materialization, declaration
ownership, repeatable instantiation identity, and the already-defined LowIR
record/initialization shape.

`general/100-class-static-constant-member-lookup`,
`general/100-duplicate-template-instantiation-signature`,
`general/100-enum-nontype-template-vtable`,
`general/100-dependent-nontype-vtable-redecl-owned-syntax`,
`general/300-class-explicit-specialization`,
`general/300-decltype-delete-type-template-arg`,
`general/300-function-explicit-specialization`,
`spec/300-class-explicit-specialization-simple`,
`spec/300-function-explicit-specialization-parameter-name`,
`spec/300-function-explicit-specialization-simple`, and
`spec/300-inline-explicit-function-specialization-weak`.

### E — required rejection (1)

`general/200-bad-nontype-template-argument-type-pack` must reject a
non-integral non-type argument instead of accepting it.

## Checkpoint Scope

This checkpoint implements Group A's foundational typed constant-value slice:

* evaluate the supported integral constant-expression operators and literals;
* preserve signed/unsigned/character/enum/long-long values and promotions in
  compiler state;
* resolve ordinary, enum, and class static constant bindings for template
  arguments, `static_assert`, `sizeof`, and `alignof`;
* bind integral non-type template parameters, including defaults and dependent
  occurrences, with canonical value identity; and
* defer dependent `static_assert` conditions until instantiation.

The focused validation set is the direct integral and assertion cases in Group
A, followed by the full PA19 report, through-PA18, and the PA19 file audit.
The checkpoint is complete when that behavior is implemented semantically (not
by fixture-specific output) and the PA19 pass count rises above 31 while all
earlier assignments remain passing.

## Checkpoint Result

Completed this turn.  The compiler now carries a typed PA19 integral value
(`known`, a typed integral type, raw value, and source spelling) through
constant evaluation and template materialization.  The increment covers integer,
character, boolean, string-subscript, cast, promotion, comparison, shift, and
bitwise constant expressions; ordinary/enum/constant-member lookup;
`sizeof`/`alignof`; integral non-type parameters and defaults; canonical
non-type specialization identity; and `static_assert` evaluation after
substitution.  PA14 global folding now consumes the analyzer's typed result
for functional integral casts, and its host character signedness model agrees
with the PA19 constant model.

The checkpoint audit also closed the unnamed-parameter dependent-base scan
loop, preserved static `constexpr` member facts across cv-only type rebuilding,
and made the typed value/type record authoritative at the PA11/PA14 and PA18
materialization boundaries.  Legacy signed fields remain projections for the
earlier lowering contracts; PA19 operations do not recover their facts from
those projections or from rewritten text.

Validation for the checkpoint:

* focused direct-value set: **10/10** after the audit fixes;
* required full current-PA report: **54/134 passing**, up from **31/134**;
* required through-PA18 report: **1430/1430 passing**; and
* PA19 file audit: **pass** (8 non-fatal pre-existing header-division warnings).

## Post-checkpoint Remaining Work Map

After the audit repairs, the complete current-PA report remains at **54/134
passing**, with **80 residual failures**.  They are grouped by the next shared
compiler behavior; the complete turn-start inventory above remains the
baseline record, while this section records the exact residual set.

### B — dependent lookup, defaults, and stale semantic state (40)

`general/100-defaulted-nontype-expression-syntax-rewrite`,
`general/100-defaulted-trailing-variable-partial-specialization`,
`general/100-dependent-bool-trait-nontype-argument`,
`general/100-dependent-nontype-functional-cast-body-check`,
`general/100-dependent-nontype-parameter-type-default`,
`general/100-dependent-static-constant-member-comparison`,
`general/100-dependent-typename-template-argument-static-assert`,
`general/100-dependent-typename`,
`general/100-enum-constant-function-style-init`,
`general/100-explicit-type-argument-class-layout-completion`,
`general/100-function-template-dependent-alias-parameter-overloads`,
`general/100-inherited-static-member-value`,
`general/100-integral-promotion-shift-bitwise-or-static-assert`,
`general/100-integral-promotion-shift-plus-one-static-assert`,
`general/100-integral-promotion-shift-plus-zero-static-assert`,
`general/100-local-decltype-callable-type-argument`,
`general/100-local-variable-template-keeps-concrete-class-instantiation`,
`general/100-nontype-expression-template-argument`,
`general/100-nontype-template-argument-static-member-no-storage`,
`general/100-qualified-template-id-nontype-argument-scope`,
`general/100-reference-member-constant-alias`,
`general/100-rooted-template-id-type-argument-static-value`,
`general/100-static-constexpr-member-call-initializer`,
`general/100-structured-nothrow-invocable-cache-default`,
`general/100-template-alignas-nontype-argument`,
`general/100-template-alignas-static-member-value`,
`general/100-template-static-constant-minimum-chain`,
`general/100-type-template-less-than-expression-argument`,
`general/300-qualified-static-array-member-sizeof`,
`general/300-template-parameter-class-member-object`,
`general/300-variable-template-specialization-empty-template-id-argument`,
`general/400-defaulted-template-member-call-rematerialization`,
`general/400-stale-class-specialization-default-argument-refresh`,
`general/400-stale-function-template-instantiation-lookup`,
`spec/100-dependent-relational-static-cast-template-default`,
`spec/100-qualified-function-template-call`,
`spec/100-qualified-template-member-type-cast-static-assert`,
`spec/100-rvalue-pointer-derived-template-call`,
`spec/100-type-equivalence-default-argument`, and
`spec/100-unused-member-body-static-assert-not-instantiated`.

### C — packs, expansions, and user-defined literal paths (25)

`general/200-bad-nontype-template-argument-type-pack`,
`general/200-defaulted-decltype-empty-pack-instantiation`,
`general/200-defaulted-decltype-pointer-placement-new`,
`general/200-empty-base-pack-expansion-sizeof`,
`general/200-function-template-leading-fixed-and-pack-call`,
`general/200-function-template-named-pack-call`,
`general/200-function-template-pack-call`,
`general/200-function-template-pack-forward-call`,
`general/200-function-template-pack-ref-return`,
`general/200-inline-dependent-pack-result-type`,
`general/200-instantiated-function-fixed-prefix-pack-tail`,
`general/200-member-init-covarying-type-index-pack`,
`general/200-pack-expanded-base-instantiation`,
`general/200-pack-expanded-explicit-nontype-expression-call`,
`general/200-pack-expansion-aggregate-brace-init`,
`general/200-pack-expansion-array-unknown-bound`,
`general/200-qualified-function-template-constructor-function-id-arg`,
`general/200-sizeof-function-parameter-pack`,
`general/200-template-parameter-pack-collection`,
`general/200-user-defined-cooked-integer-literal`,
`general/200-user-defined-integer-literal-template`,
`spec/200-call-pack-expansion-template-type-pack-only`,
`spec/200-function-template-leading-fixed-and-pack-call`,
`spec/200-function-template-pack-call`, and
`spec/200-function-template-pack-forward-call`.

The bad non-type argument remains an intentional rejection failure: the
current implementation accepts that non-integral pack case and must tighten
argument validation.

### D — specialization ownership, generated records, and LowIR shape (15)

LowIR mismatches remain in
`general/100-class-static-constant-member-lookup`,
`general/100-defaulted-nontype-class-alias-rewrite`,
`general/100-duplicate-template-instantiation-signature`,
`general/100-nontype-conversion-operator-dependent-template-id`,
`general/100-reference-static-constexpr-member-replay`,
`general/100-variable-template-id-expression`,
`general/300-class-explicit-specialization`,
`general/300-decltype-delete-type-template-arg`,
`general/300-function-explicit-specialization`,
`spec/300-class-explicit-specialization-simple`,
`spec/300-function-explicit-specialization-parameter-name`,
`spec/300-function-explicit-specialization-simple`, and
`spec/300-inline-explicit-function-specialization-weak`.

The two additional current failures,
`general/100-dependent-nontype-vtable-redecl-owned-syntax` and
`general/100-enum-nontype-template-vtable`, are vtable destructor-slot order
sanity failures and belong with this ownership/materialization group.

## Next Checkpoint Group

Start with the smallest coherent part of Group B: dependent qualified
constant lookup and default-argument rewriting, including the dependent
`static_assert` and `decltype` cases.  Re-run the complete PA19 report after
that slice; keep the pack and generated-record groups separate until they
share a concrete semantic fix.  When those remaining groups become small,
bundle the rejection case with the adjacent pack or generated-record work.
