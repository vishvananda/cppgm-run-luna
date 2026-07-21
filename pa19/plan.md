# PA19 checkpoint plan

## Turn-start baseline

The turn began at **54/134 PA19 tests passing**, with all earlier assignments
through PA18 passing.  The complete failure set was grouped by shared
behavior before implementation:

- integral constant evaluation, typed non-type arguments, and constant
  bindings;
- dependent lookup, defaults, and stale semantic state;
- parameter packs and expansions; and
- specialization ownership, generated records, LowIR shape, plus one required
  rejection case for an invalid non-type pack argument.

## Remaining Work Map recorded at turn start

The complete turn-start report contained **80 failures**.  They are grouped
below by the shared compiler behavior selected for this checkpoint; every
failure appears exactly once.  The inventory is preserved even though the
checkpoint subsequently completed the whole PA.

### A — typed integral constants and value-dependent semantic state (22)

`general/100-dependent-bool-trait-nontype-argument`,
`general/100-dependent-nontype-functional-cast-body-check`,
`general/100-dependent-nontype-parameter-type-default`,
`general/100-dependent-static-constant-member-comparison`,
`general/100-dependent-typename-template-argument-static-assert`,
`general/100-enum-constant-function-style-init`,
`general/100-inherited-static-member-value`,
`general/100-integral-promotion-shift-bitwise-or-static-assert`,
`general/100-integral-promotion-shift-plus-one-static-assert`,
`general/100-integral-promotion-shift-plus-zero-static-assert`,
`general/100-nontype-expression-template-argument`,
`general/100-nontype-template-argument-static-member-no-storage`,
`general/100-qualified-template-id-nontype-argument-scope`,
`general/100-reference-member-constant-alias`,
`general/100-rooted-template-id-type-argument-static-value`,
`general/100-static-constexpr-member-call-initializer`,
`general/100-template-alignas-nontype-argument`,
`general/100-template-alignas-static-member-value`,
`general/100-template-static-constant-minimum-chain`,
`general/100-type-template-less-than-expression-argument`,
`general/300-qualified-static-array-member-sizeof`, and
`spec/100-dependent-relational-static-cast-template-default`.

### B — dependent lookup, defaults, and stale declarations (17)

`general/100-defaulted-nontype-expression-syntax-rewrite`,
`general/100-defaulted-trailing-variable-partial-specialization`,
`general/100-dependent-typename`,
`general/100-explicit-type-argument-class-layout-completion`,
`general/100-function-template-dependent-alias-parameter-overloads`,
`general/100-local-decltype-callable-type-argument`,
`general/100-local-variable-template-keeps-concrete-class-instantiation`,
`general/300-template-parameter-class-member-object`,
`general/300-variable-template-specialization-empty-template-id-argument`,
`general/400-defaulted-template-member-call-rematerialization`,
`general/400-stale-class-specialization-default-argument-refresh`,
`general/400-stale-function-template-instantiation-lookup`,
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

### D — specialization ownership, generated records, and LowIR shape (17)

`general/100-class-static-constant-member-lookup`,
`general/100-defaulted-nontype-class-alias-rewrite`,
`general/100-dependent-nontype-vtable-redecl-owned-syntax`,
`general/100-duplicate-template-instantiation-signature`,
`general/100-enum-nontype-template-vtable`,
`general/100-nontype-conversion-operator-dependent-template-id`,
`general/100-reference-static-constexpr-member-replay`,
`general/100-structured-nothrow-invocable-cache-default`,
`general/100-variable-template-id-expression`,
`general/300-class-explicit-specialization`,
`general/300-decltype-delete-type-template-arg`,
`general/300-function-explicit-specialization`,
`spec/300-class-explicit-specialization-simple`,
`spec/300-function-explicit-specialization-parameter-name`,
`spec/300-function-explicit-specialization-simple`, and
`spec/300-inline-explicit-function-specialization-weak`.

The inventory was read from the turn-start PA19 report: the expected-failure
case in Group C had to be rejected, while all other listed cases had to
complete successfully or produce the required LowIR shape.

## Checkpoint Scope

This checkpoint covers the whole PA19 contract:

- typed integral constant evaluation, promotions, casts, literals, `sizeof`,
  `alignof`, and `static_assert`;
- integral non-type template arguments, defaults, dependent expressions,
  canonical specialization identity, and rejection of invalid argument types;
- dependent type/value lookup, aliases, overloads, defaults, and pack
  collection/expansion through calls, declarations, bases, aggregates, and
  `sizeof...`;
- static template-member storage and value propagation;
- constructor function-id deduction and nested template emission order; and
- specialization ownership, generated records, object initialization, and
  vtable/destructor LowIR shape.

The implementation keeps these facts in typed compiler state across PA11,
PA14, PA17, and PA18 boundaries.  The final structural increment also moved
large constant, global-collection, control, semantic, and template-rewrite
implementations into their per-tool translation units without changing the
compiler behavior.

## Checkpoint Result

Completed.  The PA19 report improved from **54/134** to **134/134**.  The
through-PA18 report remains **1430/1430**, and the PA19 file audit passes with
8 non-fatal pre-existing warnings.

## Post-checkpoint Remaining Work Map

None for PA19.

## Next Checkpoint Group

PA19 is complete.  The next assignment may begin from the clean committed
state produced by this checkpoint.

## Architecture Review

The completed stage follows the intended monotonic pipeline from PA10 through
PA19:

- The parser and AST remain the syntax boundary.  PA19 adds explicit AST
  metadata for base/template pack expansions and `sizeof...`; it does not
  encode LowIR answers in the parser.
- `ExpandPA18Templates` is the integration seam.  `PA18TemplateExpander`
  collects template declarations, aliases, defaults, dependent names, pack
  facts, and explicit specializations, then rewrites only the needed source
  into ordinary generated declarations.  `Instantiate` uses canonical typed
  arguments, generated-name metadata, specialization caches, and an active
  recursion set.  This keeps PA19 as an extension of PA18 materialization
  rather than a replacement template backend.
- `PA19IntegralType`/`PA19IntegralValue` in `pa19_constants.h` carry the
  supported compile-time value facts.  `PA19ConstantExpressionParser`
  resolves source-level integral expressions and `pa11_semantics_constants.cpp`
  evaluates AST expressions.  `Binding::constant_value` and
  `ConstantValue::integral` are the typed PA11 owners; signed compatibility
  fields are projections for earlier consumers.
- `Analyzer` owns scopes, lookup, types, layouts, constant bindings,
  dependent assertion timing, and declaration validation.  `PA14Lowerer`
  owns demand analysis, ABI names, object/lifetime operations, static storage,
  vtables, and LowIR.  PA19-generated declarations enter these existing
  responsibilities after expansion, so earlier procedural, class, value, and
  polymorphic assignments remain on one backend path.
- The lowerer uses stable semantic ownership: shared AST/type graphs,
  unique-owned child scopes, deque-backed bindings, and deque-backed function
  and global records.  Member bindings retain owner/index identity, and
  template caches and demand fixed points bound repeated work while preserving
  order-sensitive LowIR regions.
- `dev/frontend_source_sets.mk` registers every added implementation unit for
  `cppgm++`.  The file audit passes with only the documented warning-only
  implementation-bearing headers.  No test, fixture, source-name, reference
  binary, host compiler, or timeout shortcut is part of the PA19 path.

## Final Architecture Review

The final completion checkpoint is `af5a597`, with its complete-stage result
recorded in `3b416e6`; the typed-constant checkpoint audit remains available
in `pa19/audit.md`.  The integrated implementation now covers the complete
PA19 boundary: typed integral literals and operators, dependent constant
bindings, integral non-type arguments and defaults, type and non-type packs,
supported pack expansions, explicit class/function specialization, stale
primary refresh, static-member value/storage propagation, and deferred
`static_assert`.

The final review found no architectural regression against PA18.  Programs
that do not need template expansion remain on the ordinary AST-to-PA11-to-
PA14 path; programs that do need PA19 features are materialized into that
same path.  Typed values cross the PA11/PA14 and PA18 materialization seams
without recovering signedness or argument kind from generated text.  The
existing PA14/PA17 ownership, call, lifetime, vtable, and LowIR contracts
therefore remain the single source of truth for generated code.

The stage is complete and ready for the next PA: the current PA suite is
**134/134**, the through-stage report is **1564/1564** across **19/19**
stages, the required file audit passes, and no required source registration,
ownership, performance, correctness, or shortcut concern remains open.
