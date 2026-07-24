# PA21 checkpoint plan

## Checkpoint 66 result — 2026-07-24 (185/215)

### Checkpoint Scope

Completed the CRTP dependent-base construction slice of owner/member replay.
Unqualified functional construction now resolves an exact lexical class owner
instead of accepting `FindClassDeclaration`'s unrelated short-name fallback.
The selected member constructor template is consequently replayed on the
concrete owner.  Function-template deduction also treats cv-qualification on
an lvalue reference parameter as a binding rule, while class partial-
specialization matching retains strict cv identity.

### Validation

The current PA report is **185/215**, above the checkpoint-65 baseline of
**184/215** and the turn-start baseline of **182/215**.  Both the CRTP
constructor fixture and the cv/ref partial-ordering regression fixture pass.
The through-PA20 report and PA21 file audit remain required gates for the
checkpoint commit.

### Remaining Work Map

The current report has 30 failures, grouped by shared behavior:

- **Owner/member replay, lookup, and generated-call lowering (11):**
  `general/300-dependent-hidden-friend-static-member-definition`,
  `general/300-explicit-type-arg-decltype-member-access`,
  `general/300-friend-existing-template-private-ctor-access`,
  `general/300-function-pack-template-id-deduction-decltype`,
  `general/300-local-qualified-argument-replay`,
  `general/300-member-class-explicit-specialization-owner-lookup`,
  `general/300-namespace-function-template-hides-outer-callable-object`,
  `general/300-nested-class-template-current-owner-lookup`,
  `general/300-out-of-class-ctor-using-imported-member-template`,
  `general/300-parenthesized-qualified-template-functional-call`, and
  `general/300-qualified-friend-member-template-access`.
- **Specialization, alias, cv, and pack identity (14):**
  `general/100-top-cv-pointer-does-not-match-unqualified-pointer-partial`,
  `general/200-reference-alias-top-cv-return-binding`,
  `general/300-function-signature-partial-specialization-functor-assignment`,
  `general/300-variable-template-forwarding-partial-top-cv`,
  `general/400-alias-pack-nontype-expression-fast-path`,
  `general/400-forward-primary-partial-switch-value`,
  `general/400-inline-namespace-template-template-argument`,
  `general/400-member-template-nontype-shadowed-global-replay`,
  `general/400-partial-specialization-conversion-operator-pointer-binding`,
  `general/400-partial-specialization-redecl-member-template-empty-pack`,
  `general/400-reference-member-depth-pack-sum`,
  `general/400-reference-member-lookup-in-progress-base-typedef`,
  `spec/100-inline-namespace-qualified-template-id-pack-expansion`, and
  `spec/300-defaulted-type-arg-specialization-nontype-value`.
- **Dependent initialization, references, and address lowering (5):**
  `general/100-rvalue-reference-binds-converted-temporary`,
  `general/300-array-functional-cast-pack-call`,
  `general/300-constexpr-static-fn-template-address-pack`,
  `general/300-single-pack-cast-target`, and
  `general/300-static-constexpr-function-template-pointer-array`.

### Next Checkpoint Group

Continue with the remaining owner/member cases, starting with the shared
qualified/local functional-call replay path.  Keep the 185/215 current report,
through-PA20 report, and file audit as regression gates.

## Checkpoint 65 result — 2026-07-24 (184/215)

### Checkpoint Scope

Implemented owner-aware lowering for an unqualified explicit member-template
id inside a member function.  The call is replayed through typed `this`
lookup instead of free-function lookup, materialized non-operator member
templates retain unique generated declarators, and their typed source-name
aliases remain available to member overload lookup.  A non-template sibling
is retained as a reachable out-of-class definition when that member-template
overload set is materialized; operator-template emission is unchanged.

### Validation

The current PA report is **184/215**, above the turn-start **182/215**.  The
focused `general/300-explicit-member-template-id-shares-ordinary-overload`
fixture passes, as does the existing
`spec/300-member-operator-template-in-class-template` regression fixture.
The through-PA20 report remains green at **1635/1635**, and the PA21 file
audit passes with the nine pre-existing header-division warnings.

### Remaining Work Map

The current report has 31 failures, grouped by shared behavior:

- **Owner/member replay, lookup, and generated-call lowering (12):**
  `general/300-crtp-static-cast-reference-before-constructor-template`,
  `general/300-dependent-hidden-friend-static-member-definition`,
  `general/300-explicit-type-arg-decltype-member-access`,
  `general/300-friend-existing-template-private-ctor-access`,
  `general/300-function-pack-template-id-deduction-decltype`,
  `general/300-local-qualified-argument-replay`,
  `general/300-member-class-explicit-specialization-owner-lookup`,
  `general/300-namespace-function-template-hides-outer-callable-object`,
  `general/300-nested-class-template-current-owner-lookup`,
  `general/300-out-of-class-ctor-using-imported-member-template`,
  `general/300-parenthesized-qualified-template-functional-call`, and
  `general/300-qualified-friend-member-template-access`.
- **Specialization, alias, cv, and pack identity (14):**
  `general/100-top-cv-pointer-does-not-match-unqualified-pointer-partial`,
  `general/200-reference-alias-top-cv-return-binding`,
  `general/300-function-signature-partial-specialization-functor-assignment`,
  `general/300-variable-template-forwarding-partial-top-cv`,
  `general/400-alias-pack-nontype-expression-fast-path`,
  `general/400-forward-primary-partial-switch-value`,
  `general/400-inline-namespace-template-template-argument`,
  `general/400-member-template-nontype-shadowed-global-replay`,
  `general/400-partial-specialization-conversion-operator-pointer-binding`,
  `general/400-partial-specialization-redecl-member-template-empty-pack`,
  `general/400-reference-member-depth-pack-sum`,
  `general/400-reference-member-lookup-in-progress-base-typedef`,
  `spec/100-inline-namespace-qualified-template-id-pack-expansion`, and
  `spec/300-defaulted-type-arg-specialization-nontype-value`.
- **Dependent initialization, references, and address lowering (5):**
  `general/100-rvalue-reference-binds-converted-temporary`,
  `general/300-array-functional-cast-pack-call`,
  `general/300-constexpr-static-fn-template-address-pack`,
  `general/300-single-pack-cast-target`, and
  `general/300-static-constexpr-function-template-pointer-array`.

### Next Checkpoint Group

Continue with the remaining 12-case owner/member group, beginning with CRTP
dependent-base construction and the qualified/local owner replay cases.  Keep
the 184/215 PA report, through-PA20 report, and file audit as regression gates.

## Checkpoint 64 result — 2026-07-24 (184/215)

### Checkpoint Scope

Implemented the member-operator owner/replay increment: member operator
templates are materialized through concrete-owner lookup, generated operator
declarations retain a typed source-name alias, overloaded binary lowering keeps
class operands on the selected user-defined operator, unary free operator
templates and dependent-base deduction are replayed, and specialization
identity reuse is limited to the generated primary namespace so associated
namespace lookup remains intact.

### Validation

The current PA report is **184/215**, up from the turn-start **182/215**;
both member-operator fixtures now pass.  The through-PA20 report is green at
**1635/1635**, and the PA21 file audit passes with the nine pre-existing
header-division warnings.

### Remaining Work Map

The current report has 31 failures, grouped by shared behavior:

- **Owner/member replay, lookup, and generated-call lowering (12):**
  `general/300-crtp-static-cast-reference-before-constructor-template`,
  `general/300-dependent-hidden-friend-static-member-definition`,
  `general/300-explicit-member-template-id-shares-ordinary-overload`,
  `general/300-explicit-type-arg-decltype-member-access`,
  `general/300-friend-existing-template-private-ctor-access`,
  `general/300-function-pack-template-id-deduction-decltype`,
  `general/300-local-qualified-argument-replay`,
  `general/300-member-class-explicit-specialization-owner-lookup`,
  `general/300-namespace-function-template-hides-outer-callable-object`,
  `general/300-nested-class-template-current-owner-lookup`,
  `general/300-parenthesized-qualified-template-functional-call`, and
  `general/300-qualified-friend-member-template-access`.
- **Specialization, alias, cv, and pack identity (14):**
  `general/100-top-cv-pointer-does-not-match-unqualified-pointer-partial`,
  `general/200-reference-alias-top-cv-return-binding`,
  `general/300-function-signature-partial-specialization-functor-assignment`,
  `general/300-variable-template-forwarding-partial-top-cv`,
  `general/400-alias-pack-nontype-expression-fast-path`,
  `general/400-forward-primary-partial-switch-value`,
  `general/400-inline-namespace-template-template-argument`,
  `general/400-member-template-nontype-shadowed-global-replay`,
  `general/400-partial-specialization-conversion-operator-pointer-binding`,
  `general/400-partial-specialization-redecl-member-template-empty-pack`,
  `general/400-reference-member-depth-pack-sum`,
  `general/400-reference-member-lookup-in-progress-base-typedef`,
  `spec/100-inline-namespace-qualified-template-id-pack-expansion`, and
  `spec/300-defaulted-type-arg-specialization-nontype-value`.
- **Dependent initialization, references, and address lowering (5):**
  `general/100-rvalue-reference-binds-converted-temporary`,
  `general/300-array-functional-cast-pack-call`,
  `general/300-constexpr-static-fn-template-address-pack`,
  `general/300-single-pack-cast-target`, and
  `general/300-static-constexpr-function-template-pointer-array`.

### Next Checkpoint Group

Continue with the remaining 12-case owner/member group, starting with CRTP
dependent-base downcasts, explicit member-template overload sets, and nested
owner replay.  Keep the 184/215 current report, 1635/1635 through-PA20 gate,
and file audit as regression checks before bundling the 14 identity cases with
the five initialization/address cases.

## Checkpoint 63 audit refresh — 2026-07-24 (current baseline 182/215)

### Remaining Work Map

The fresh `make test-report ACTIVE_TEST_REPORT_PAS='pa21'` result has 33
failures.  This is the complete current-PA set after the three scoped
specialization/instantiation diagnostics passed:

- **Owner/member replay, lookup, and generated-call lowering (14):**
  `general/300-crtp-static-cast-reference-before-constructor-template`,
  `general/300-dependent-hidden-friend-static-member-definition`,
  `general/300-explicit-member-template-id-shares-ordinary-overload`,
  `general/300-explicit-type-arg-decltype-member-access`,
  `general/300-friend-existing-template-private-ctor-access`,
  `general/300-function-pack-template-id-deduction-decltype`,
  `general/300-local-qualified-argument-replay`,
  `general/300-member-class-explicit-specialization-owner-lookup`,
  `general/300-namespace-function-template-hides-outer-callable-object`,
  `general/300-nested-class-template-current-owner-lookup`,
  `general/300-parenthesized-qualified-template-functional-call`,
  `general/300-qualified-friend-member-template-access`,
  `spec/300-member-operator-template-active-owner`, and
  `spec/300-member-operator-template-in-class-template`.
- **Specialization, alias, cv, and pack identity (14):**
  `general/100-top-cv-pointer-does-not-match-unqualified-pointer-partial`
  (comparator internal undefined canonical value),
  `general/200-reference-alias-top-cv-return-binding`,
  `general/300-function-signature-partial-specialization-functor-assignment`,
  `general/300-variable-template-forwarding-partial-top-cv`,
  `general/400-alias-pack-nontype-expression-fast-path`,
  `general/400-forward-primary-partial-switch-value`,
  `general/400-inline-namespace-template-template-argument`,
  `general/400-member-template-nontype-shadowed-global-replay`,
  `general/400-partial-specialization-conversion-operator-pointer-binding`,
  `general/400-partial-specialization-redecl-member-template-empty-pack`,
  `general/400-reference-member-depth-pack-sum`,
  `general/400-reference-member-lookup-in-progress-base-typedef`,
  `spec/100-inline-namespace-qualified-template-id-pack-expansion`, and
  `spec/300-defaulted-type-arg-specialization-nontype-value`.
- **Dependent initialization, references, and address lowering (5):**
  `general/100-rvalue-reference-binds-converted-temporary`,
  `general/300-array-functional-cast-pack-call`,
  `general/300-constexpr-static-fn-template-address-pack`,
  `general/300-single-pack-cast-target`, and
  `general/300-static-constexpr-function-template-pointer-array`.

### Next Substantial Checkpoint Group

Take the 14-case owner/member replay group next, starting with the two
member-operator fixtures, explicit member-template overload lookup, and
concrete nested-owner replay.  After that substantial group, bundle the
five dependent-initialization/address cases with the 14-case specialization,
alias, cv, and pack-identity group rather than leaving the small dependent
tail as a standalone checkpoint.

## Implementation checkpoint 63 scope (2026-07-24, baseline 179/215)

### Remaining Work Map

The complete current-PA failure set from `make test-report
ACTIVE_TEST_REPORT_PAS='pa21'` is grouped by shared compiler behavior:

- **Owner/member replay, lookup, and generated-call lowering (14):**
  `general/300-crtp-static-cast-reference-before-constructor-template`,
  `general/300-dependent-hidden-friend-static-member-definition`,
  `general/300-explicit-member-template-id-shares-ordinary-overload`,
  `general/300-explicit-type-arg-decltype-member-access`,
  `general/300-friend-existing-template-private-ctor-access`,
  `general/300-function-pack-template-id-deduction-decltype`,
  `general/300-local-qualified-argument-replay`,
  `general/300-member-class-explicit-specialization-owner-lookup`,
  `general/300-namespace-function-template-hides-outer-callable-object`,
  `general/300-nested-class-template-current-owner-lookup`,
  `general/300-parenthesized-qualified-template-functional-call`,
  `general/300-qualified-friend-member-template-access`,
  `spec/300-member-operator-template-active-owner`, and
  `spec/300-member-operator-template-in-class-template`.
- **Specialization, alias, cv, and pack identity (14):**
  `general/100-top-cv-pointer-does-not-match-unqualified-pointer-partial`
  (the comparator reports an undefined canonical value),
  `general/200-reference-alias-top-cv-return-binding`,
  `general/300-function-signature-partial-specialization-functor-assignment`,
  `general/300-variable-template-forwarding-partial-top-cv`,
  `general/400-alias-pack-nontype-expression-fast-path`,
  `general/400-forward-primary-partial-switch-value`,
  `general/400-inline-namespace-template-template-argument`,
  `general/400-member-template-nontype-shadowed-global-replay`,
  `general/400-partial-specialization-conversion-operator-pointer-binding`,
  `general/400-partial-specialization-redecl-member-template-empty-pack`,
  `general/400-reference-member-depth-pack-sum`,
  `general/400-reference-member-lookup-in-progress-base-typedef`,
  `spec/100-inline-namespace-qualified-template-id-pack-expansion`, and
  `spec/300-defaulted-type-arg-specialization-nontype-value`.
- **Explicit specialization/instantiation ordering and diagnostics (3):**
  `general/300-extern-template-builtin-operator-function-declaration-bad`,
  `general/300-template-instantiation-use-location-explicit-specialization`,
  and `spec/300-explicit-specialized-ctor-template-header-bad`.
- **Dependent initialization, references, and address lowering (5):**
  `general/100-rvalue-reference-binds-converted-temporary`,
  `general/300-array-functional-cast-pack-call`,
  `general/300-constexpr-static-fn-template-address-pack`,
  `general/300-single-pack-cast-target`, and
  `general/300-static-constexpr-function-template-pointer-array`.

No failure is omitted from this map; the comparator internal failure is counted
in the identity group rather than treated as a compiler success.

### Checkpoint Scope

Implement the explicit-specialization/instantiation ordering slice as real
typed semantic state: reject explicit specialization syntax that cannot denote
a specialization, reject explicit instantiation declarations that target a
builtin operator without a valid template entity, and preserve the source/use
ordering facts needed to diagnose a specialization after an instantiated use.
The focused validation set is the three diagnostic fixtures named above, plus
the through-PA20 report and the PA21 file audit.  This is a coherent
checkpoint because all three failures arise while validating declarations
against the template entity and its materialization history, before ordinary
LowIR lowering.

## Checkpoint 63 result (2026-07-24, 182/215)

The three scoped fixtures now pass.  The implementation keeps explicit
specialization and instantiation facts in the template registry, validates
builtin operator instantiation targets, replays synthesized functional
initializers through typed member-template deduction, carries promoted local
class identity into constructor and array deduction, and reuses planned array
addresses during construction/destruction lowering.  The current-PA report
rose from **179/215 to 182/215**; the through-PA20 report passes at
**1635/1635**, and the PA21 file audit passes with only the nine pre-existing
header-division warnings.

### Remaining Work Map

The fresh current-PA report has 33 failures, grouped by shared behavior:

- **Owner/member replay, lookup, and generated-call lowering (14):**
  `general/300-crtp-static-cast-reference-before-constructor-template`,
  `general/300-dependent-hidden-friend-static-member-definition`,
  `general/300-explicit-member-template-id-shares-ordinary-overload`,
  `general/300-explicit-type-arg-decltype-member-access`,
  `general/300-friend-existing-template-private-ctor-access`,
  `general/300-function-pack-template-id-deduction-decltype`,
  `general/300-local-qualified-argument-replay`,
  `general/300-member-class-explicit-specialization-owner-lookup`,
  `general/300-namespace-function-template-hides-outer-callable-object`,
  `general/300-nested-class-template-current-owner-lookup`,
  `general/300-parenthesized-qualified-template-functional-call`,
  `general/300-qualified-friend-member-template-access`,
  `spec/300-member-operator-template-active-owner`, and
  `spec/300-member-operator-template-in-class-template`.
- **Specialization, alias, cv, and pack identity (14):**
  `general/100-top-cv-pointer-does-not-match-unqualified-pointer-partial`,
  `general/200-reference-alias-top-cv-return-binding`,
  `general/300-function-signature-partial-specialization-functor-assignment`,
  `general/300-variable-template-forwarding-partial-top-cv`,
  `general/400-alias-pack-nontype-expression-fast-path`,
  `general/400-forward-primary-partial-switch-value`,
  `general/400-inline-namespace-template-template-argument`,
  `general/400-member-template-nontype-shadowed-global-replay`,
  `general/400-partial-specialization-conversion-operator-pointer-binding`,
  `general/400-partial-specialization-redecl-member-template-empty-pack`,
  `general/400-reference-member-depth-pack-sum`,
  `general/400-reference-member-lookup-in-progress-base-typedef`,
  `spec/100-inline-namespace-qualified-template-id-pack-expansion`, and
  `spec/300-defaulted-type-arg-specialization-nontype-value`.
  The top-cv fixture remains a comparator-internal failure.
- **Dependent initialization, references, and address lowering (5):**
  `general/100-rvalue-reference-binds-converted-temporary`,
  `general/300-array-functional-cast-pack-call`,
  `general/300-constexpr-static-fn-template-address-pack`,
  `general/300-single-pack-cast-target`, and
  `general/300-static-constexpr-function-template-pointer-array`.

The next checkpoint group is the owner/member replay slice, beginning with
the two member-operator fixtures, explicit member-template overload lookup,
and concrete nested-owner replay; validate it against the owner group above,
then rerun the full current-PA report and the through-PA20/file-audit gates.

## Implementation checkpoint 61 scope (2026-07-24, baseline 176/215)

### Remaining Work Map

The complete baseline failure set is grouped below by shared behavior:

- **Owner-context replay and member lookup (16):** nested/current-owner
  replay, out-of-class member ownership, friend/member lookup, and generated
  call lowering (`general/300-crtp-static-cast-reference-before-constructor-template`,
  `general/300-dependent-hidden-friend-static-member-definition`,
  `general/300-explicit-member-template-id-shares-ordinary-overload`,
  `general/300-explicit-type-arg-decltype-member-access`,
  `general/300-friend-existing-template-private-ctor-access`,
  `general/300-function-pack-template-id-deduction-decltype`,
  `general/300-local-qualified-argument-replay`,
  `general/300-member-class-explicit-specialization-owner-lookup`,
  `general/300-namespace-function-template-hides-outer-callable-object`,
  `general/300-nested-class-template-current-owner-lookup`,
  `general/300-nested-class-template-reference-reset`,
  `general/300-out-of-class-ctor-using-imported-member-template`,
  `general/300-parenthesized-qualified-template-functional-call`,
  `general/300-qualified-friend-member-template-access`,
  `spec/300-member-operator-template-active-owner`, and
  `spec/300-member-operator-template-in-class-template`).
- **Partial-specialization/alias/cv/pack identity (15):** canonical matching,
  partial ordering, and dependent value identity.
- **Explicit specialization/instantiation diagnostics (3):** declaration
  ordering and invalid explicit-specialization forms.
- **Dependent initialization/address lowering (5):** reference binding,
  functional casts, and template-function address materialization.

The report also exposes one comparator internal failure for
`general/100-top-cv-pointer-does-not-match-unqualified-pointer-partial`; it
is included in the identity group rather than treated as compiler success.

### Checkpoint Scope

Implement the owner-context replay slice as typed compiler behavior: preserve a
materialized class specialization's owning scope while replaying nested class
and member-template declarations, restore that owner across nested replay, and
make member-template/operator lookup use the active concrete owner.  Validate
the six focused fixtures named above, then run the full PA21 report and the
through-PA20 gate.  The checkpoint is substantial because it covers both the
replay registry and the ordinary semantic/lowering member candidate path.

## Checkpoint audit result (2026-07-24, 176/215)

The latest checkpoint audit preserved the current-PA result at **176/215**,
the audit turn-start baseline, while keeping **1635/1635 through PA20**.
The audit found and fixed four checkpoint-level issues: implementation packed
`pa18_templates_rewrite.cpp` to the 1500-line ceiling; nested static-member
identity was recovered by repeated AST serialization; generated hidden-friend
ADL scanned an entire owner binding deque without using typed ownership; and
constructor replay selected source definitions by bare name without checking
their qualified owner.  The fixes move constructor replay into its own source
unit, index static members in `TemplateDefinition`, build a typed hidden-friend
binding index once, and enforce qualified constructor ownership.

### Remaining Work Map

- **Owner replay, member/friend lookup, and generated-call lowering (16):**
  `general/300-crtp-static-cast-reference-before-constructor-template`,
  `general/300-dependent-hidden-friend-static-member-definition`,
  `general/300-explicit-member-template-id-shares-ordinary-overload`,
  `general/300-explicit-type-arg-decltype-member-access`,
  `general/300-friend-existing-template-private-ctor-access`,
  `general/300-function-pack-template-id-deduction-decltype`,
  `general/300-local-qualified-argument-replay`,
  `general/300-member-class-explicit-specialization-owner-lookup`,
  `general/300-namespace-function-template-hides-outer-callable-object`,
  `general/300-nested-class-template-current-owner-lookup`,
  `general/300-nested-class-template-reference-reset`,
  `general/300-out-of-class-ctor-using-imported-member-template`,
  `general/300-parenthesized-qualified-template-functional-call`,
  `general/300-qualified-friend-member-template-access`,
  `spec/300-member-operator-template-active-owner`, and
  `spec/300-member-operator-template-in-class-template`.
  The generated-call cases are LowIR-only diffs; the other cases still fail
  during semantic/template replay.

- **Partial-specialization, alias, cv, and pack identity (15):**
  `general/100-top-cv-pointer-does-not-match-unqualified-pointer-partial`
  (the comparator reports an undefined canonical value),
  `general/200-reference-alias-top-cv-return-binding`,
  `general/300-function-signature-partial-specialization-functor-assignment`,
  `general/300-nested-member-partial-specialization-apply-scope`,
  `general/300-variable-template-forwarding-partial-top-cv`,
  `general/400-alias-pack-nontype-expression-fast-path`,
  `general/400-forward-primary-partial-switch-value`,
  `general/400-inline-namespace-template-template-argument`,
  `general/400-member-template-nontype-shadowed-global-replay`,
  `general/400-partial-specialization-conversion-operator-pointer-binding`,
  `general/400-partial-specialization-redecl-member-template-empty-pack`,
  `general/400-reference-member-depth-pack-sum`,
  `general/400-reference-member-lookup-in-progress-base-typedef`,
  `spec/100-inline-namespace-qualified-template-id-pack-expansion`, and
  `spec/300-defaulted-type-arg-specialization-nontype-value`.

- **Explicit specialization/instantiation diagnostics (3):**
  `general/300-extern-template-builtin-operator-function-declaration-bad`,
  `general/300-template-instantiation-use-location-explicit-specialization`,
  and `spec/300-explicit-specialized-ctor-template-header-bad`.

- **Dependent initialization, references, and address lowering (5):**
  `general/100-rvalue-reference-binds-converted-temporary`,
  `general/300-array-functional-cast-pack-call`,
  `general/300-constexpr-static-fn-template-address-pack`,
  `general/300-single-pack-cast-target`, and
  `general/300-static-constexpr-function-template-pointer-array`.

These are the complete **38 status/LowIR fixture failures plus one comparator
internal failure** reported by the current-PA run; no failure is omitted from
the map.

### Checkpoint Scope and Audit Result

The audited scope remains the generated-entity replay slice: hidden-friend
operator association, generated-to-source constructor ownership, unnamed
aggregate storage, enclosing substitutions, plain `Owner<T>::Nested<U>`
replay, concrete nested-owner registration, and static-member expression
preservation.  The focused hidden-friend ADL, anonymous-union constructor, and
out-of-class nested-member tests pass.  No shortcut, timeout workaround,
acceptance bypass, or unresolved checkpoint-level architecture/performance
blocker remains.

The required PA21 report is **176/215**, the through report is **1635/1635**,
and the file audit passes with warnings only.  The next substantial checkpoint
group is the remaining PA18 owner-context replay slice:
`general/300-nested-class-template-current-owner-lookup`,
`general/300-nested-class-template-reference-reset`,
`general/300-nested-member-partial-specialization-apply-scope`,
`general/300-out-of-class-ctor-using-imported-member-template`,
`spec/300-member-operator-template-active-owner`, and
`spec/300-member-operator-template-in-class-template`.

## Superseded checkpoint result (2026-07-23, 176/215)

The fresh required report, `make test-report ACTIVE_TEST_REPORT_PAS='pa21'`,
is **176/215 passing** with **39 residual fixture failures plus one comparator
internal failure**, up from the turn-start baseline of **153/215**.  The
prior-PA gate remains clean at **1635/1635 through PA20**.  The PA21 file audit
passes with warnings only.

### Remaining Work Map

- **Owner replay, member/friend lookup, and generated-call lowering:**
  `general/300-crtp-static-cast-reference-before-constructor-template`,
  `general/300-dependent-hidden-friend-static-member-definition`,
  `general/300-explicit-member-template-id-shares-ordinary-overload`,
  `general/300-explicit-type-arg-decltype-member-access`,
  `general/300-friend-existing-template-private-ctor-access`,
  `general/300-function-pack-template-id-deduction-decltype`,
  `general/300-local-qualified-argument-replay`,
  `general/300-member-class-explicit-specialization-owner-lookup`,
  `general/300-namespace-function-template-hides-outer-callable-object`,
  `general/300-nested-class-template-current-owner-lookup`,
  `general/300-nested-class-template-reference-reset`,
  `general/300-out-of-class-ctor-using-imported-member-template`,
  `general/300-parenthesized-qualified-template-functional-call`,
  `general/300-qualified-friend-member-template-access`,
  `spec/300-member-operator-template-active-owner`, and
  `spec/300-member-operator-template-in-class-template`.
  The generated-call cases in this group are LowIR-only diffs; the others
  still fail during semantic/template replay.

- **Partial-specialization, alias, cv, and pack identity:**
  `general/100-top-cv-pointer-does-not-match-unqualified-pointer-partial`
  (the comparator reports an undefined canonical value),
  `general/200-reference-alias-top-cv-return-binding`,
  `general/300-function-signature-partial-specialization-functor-assignment`,
  `general/300-nested-member-partial-specialization-apply-scope`,
  `general/300-variable-template-forwarding-partial-top-cv`,
  `general/400-alias-pack-nontype-expression-fast-path`,
  `general/400-forward-primary-partial-switch-value`,
  `general/400-inline-namespace-template-template-argument`,
  `general/400-member-template-nontype-shadowed-global-replay`,
  `general/400-partial-specialization-conversion-operator-pointer-binding`,
  `general/400-partial-specialization-redecl-member-template-empty-pack`,
  `general/400-reference-member-depth-pack-sum`,
  `general/400-reference-member-lookup-in-progress-base-typedef`,
  `spec/100-inline-namespace-qualified-template-id-pack-expansion`, and
  `spec/300-defaulted-type-arg-specialization-nontype-value`.

- **Explicit specialization/instantiation diagnostics:**
  `general/300-extern-template-builtin-operator-function-declaration-bad`,
  `general/300-template-instantiation-use-location-explicit-specialization`,
  and `spec/300-explicit-specialized-ctor-template-header-bad`.

- **Dependent initialization, references, and address lowering:**
  `general/100-rvalue-reference-binds-converted-temporary`,
  `general/300-array-functional-cast-pack-call`,
  `general/300-constexpr-static-fn-template-address-pack`,
  `general/300-single-pack-cast-target`, and
  `general/300-static-constexpr-function-template-pointer-array`.

### Checkpoint Scope and Result

This checkpoint completes one coherent generated-entity replay slice.  Hidden
friend operator bindings whose materialized names carry the `__inst_` suffix
are now reachable through associated-namespace lookup.  Initializer
constructor materialization maps generated class names back to source
constructor definitions, so member-template constructors participate in
overload selection.  Unnamed aggregate children are retained during template
replay, preserving anonymous-union storage.  Enclosing substitutions, plain
`Owner<T>::Nested<U>` syntax, concrete owner registration, and static-member
expression preservation now cover out-of-class nested class templates.

The focused hidden-friend ADL, anonymous-union constructor, and nested
member-class checks pass.  The required report is **176/215**, the through
report is **1635/1635**, and the file audit passes with warnings only.

The next checkpoint group is the remaining PA18 owner-context replay slice:
`general/300-nested-class-template-current-owner-lookup`,
`general/300-nested-class-template-reference-reset`,
`general/300-nested-member-partial-specialization-apply-scope`,
`general/300-out-of-class-ctor-using-imported-member-template`,
`spec/300-member-operator-template-active-owner`, and
`spec/300-member-operator-template-in-class-template`.

## Superseded checkpoint result (2026-07-23, 173/215)

The fresh required report, `make test-report ACTIVE_TEST_REPORT_PAS='pa21'`,
is **173/215 passing** with **42 failures**, up from the turn-start baseline
of **153/215**.  The prior-PA gate remains clean at **1635/1635 through PA20**.
The PA21 file audit passes with warnings only.

### Remaining Work Map

- **Owner replay, member/friend lookup, and generated-call lowering (18):**
  `general/300-crtp-static-cast-reference-before-constructor-template`,
  `general/300-dependent-hidden-friend-static-member-definition`,
  `general/300-explicit-member-template-id-shares-ordinary-overload`,
  `general/300-explicit-type-arg-decltype-member-access`,
  `general/300-friend-existing-template-private-ctor-access`,
  `general/300-function-pack-template-id-deduction-decltype`,
  `general/300-local-qualified-argument-replay`,
  `general/300-member-class-explicit-specialization-owner-lookup`,
  `general/300-namespace-function-template-hides-outer-callable-object`,
  `general/300-nested-class-template-current-owner-lookup`,
  `general/300-nested-class-template-reference-reset`,
  `general/300-out-of-class-ctor-using-imported-member-template`,
  `general/300-parenthesized-qualified-template-functional-call`,
  `general/300-qualified-friend-member-template-access`,
  `spec/300-hidden-friend-template-operator-adl`,
  `spec/300-member-class-template-out-of-class`,
  `spec/300-member-operator-template-active-owner`, and
  `spec/300-member-operator-template-in-class-template`.

- **Partial-specialization, alias, cv, and pack identity (15):**
  `general/100-top-cv-pointer-does-not-match-unqualified-pointer-partial`
  (the comparison path currently reports an undefined canonical value),
  `general/200-reference-alias-top-cv-return-binding`,
  `general/300-function-signature-partial-specialization-functor-assignment`,
  `general/300-nested-member-partial-specialization-apply-scope`,
  `general/300-variable-template-forwarding-partial-top-cv`,
  `general/400-alias-pack-nontype-expression-fast-path`,
  `general/400-forward-primary-partial-switch-value`,
  `general/400-inline-namespace-template-template-argument`,
  `general/400-member-template-nontype-shadowed-global-replay`,
  `general/400-partial-specialization-conversion-operator-pointer-binding`,
  `general/400-partial-specialization-redecl-member-template-empty-pack`,
  `general/400-reference-member-depth-pack-sum`,
  `general/400-reference-member-lookup-in-progress-base-typedef`,
  `spec/100-inline-namespace-qualified-template-id-pack-expansion`, and
  `spec/300-defaulted-type-arg-specialization-nontype-value`.

- **Explicit specialization/instantiation diagnostics (3):**
  `general/300-extern-template-builtin-operator-function-declaration-bad`,
  `general/300-template-instantiation-use-location-explicit-specialization`,
  and `spec/300-explicit-specialized-ctor-template-header-bad`.

- **Dependent initialization, references, and address lowering (6):**
  `general/100-rvalue-reference-binds-converted-temporary`,
  `general/300-anonymous-union-storage-constructor-noop`,
  `general/300-array-functional-cast-pack-call`,
  `general/300-constexpr-static-fn-template-address-pack`,
  `general/300-single-pack-cast-target`, and
  `general/300-static-constexpr-function-template-pointer-array`.

### Checkpoint Scope and Result

This checkpoint completes the typed specialization/source-order slice: builtin
type spellings no longer become false template entities; out-of-class explicit
member specializations retain their enclosing class specialization while the
primary member body remains separate; concrete class materialization is tracked
as typed state so a later explicit specialization is rejected without rejecting
typedef-only uses; and dependent out-of-class integral static definitions emit
storage only when a complete object use demands it, preserving pointer/typedef
behavior from earlier PAs.

Validation for the specialization slice is
`make -C pa21 check TEST='tests/spec/300-explicit-specialization-*.t'`,
which passes **7/7**, plus the PA19 regression test, which passes **1/1**.
The hidden-friend static case now contains the expected weak global and only
retains a separate LowIR mismatch in trivial-object operator/copy lowering;
that residual is the next group rather than an unresolved storage-routing rule.
The required report is **173/215**, the through report is **1635/1635**, and
the file audit passes with warnings only.

The next checkpoint group is friend/member call and trivial-object lowering:
`general/300-dependent-hidden-friend-static-member-definition`,
`general/300-explicit-member-template-id-shares-ordinary-overload`,
`general/300-friend-existing-template-private-ctor-access`,
`general/300-qualified-friend-member-template-access`,
`spec/300-hidden-friend-template-operator-adl`, and
`general/300-anonymous-union-storage-constructor-noop`.

## Superseded checkpoint result (2026-07-23)

The clean required report, `make test-report ACTIVE_TEST_REPORT_PAS='pa21'`,
is **170/215 passing** with **45 failures**, up from the turn-start baseline
of **153/215**.  The clean rebuild also passes the through-PA20 suite at
**1635/1635**.  The six-test Group C checkpoint is **5/6**: member-template
assignment fallback, local-static specialization identity, explicit
instantiation of a static member function, explicit static-data specialization,
and rooted static-data ownership pass.  The remaining focused mismatch is
`general/300-dependent-hidden-friend-static-member-definition`, whose lowered
output still lacks the expected global storage and emits an extra empty-object
operator call/copy sequence compared with the checked-in LowIR.

### Remaining Work Map

- **Owner replay, member/friend lookup, and generated-call lowering (19):**
  `general/300-crtp-static-cast-reference-before-constructor-template`,
  `general/300-dependent-hidden-friend-static-member-definition`,
  `general/300-explicit-member-template-id-shares-ordinary-overload`,
  `general/300-explicit-type-arg-decltype-member-access`,
  `general/300-friend-existing-template-private-ctor-access`,
  `general/300-function-pack-template-id-deduction-decltype`,
  `general/300-local-qualified-argument-replay`,
  `general/300-member-class-explicit-specialization-owner-lookup`,
  `general/300-member-template-nontype-shadowed-global-replay`,
  `general/300-namespace-function-template-hides-outer-callable-object`,
  `general/300-nested-class-template-current-owner-lookup`,
  `general/300-nested-class-template-reference-reset`,
  `general/300-out-of-class-ctor-using-imported-member-template`,
  `general/300-parenthesized-qualified-template-functional-call`,
  `general/300-qualified-friend-member-template-access`,
  `spec/300-hidden-friend-template-operator-adl`,
  `spec/300-member-class-template-out-of-class`,
  `spec/300-member-operator-template-active-owner`, and
  `spec/300-member-operator-template-in-class-template`.

- **Partial-specialization, alias, and cv/pack identity (15):**
  `general/100-top-cv-pointer-does-not-match-unqualified-pointer-partial`,
  `general/200-reference-alias-top-cv-return-binding`,
  `general/300-function-signature-partial-specialization-functor-assignment`,
  `general/300-nested-member-partial-specialization-apply-scope`,
  `general/300-variable-template-forwarding-partial-top-cv`,
  `general/400-alias-pack-nontype-expression-fast-path`,
  `general/400-forward-primary-partial-switch-value`,
  `general/400-inline-namespace-template-template-argument`,
  `general/400-partial-specialization-conversion-operator-pointer-binding`,
  `general/400-partial-specialization-redecl-member-template-empty-pack`,
  `general/400-reference-member-depth-pack-sum`,
  `general/400-reference-member-lookup-in-progress-base-typedef`,
  `spec/100-inline-namespace-qualified-template-id-pack-expansion`,
  `spec/300-defaulted-type-arg-specialization-nontype-value`, and
  `spec/300-explicit-specialization-out-of-class-member-overrides-primary`.

- **Explicit specialization/instantiation ordering and diagnostics (5):**
  `general/300-extern-template-builtin-operator-function-declaration-bad`,
  `general/300-template-instantiation-use-location-explicit-specialization`,
  `spec/300-explicit-specialization-after-instantiation`,
  `spec/300-explicit-specialization-refreshes-stale-primary-instantiation`,
  and `spec/300-explicit-specialized-ctor-template-header-bad`.

- **Dependent initialization, address, and ordinary lowering (6):**
  `general/100-rvalue-reference-binds-converted-temporary`,
  `general/300-anonymous-union-storage-constructor-noop`,
  `general/300-array-functional-cast-pack-call`,
  `general/300-constexpr-static-fn-template-address-pack`,
  `general/300-single-pack-cast-target`, and
  `general/300-static-constexpr-function-template-pointer-array`.

### Checkpoint Scope and Result

This turn implements the typed generated-entity slice needed by PA21:
generated class layout is finalized after replayed dependent definitions are
available; generated member templates remain distinct from copy/move special
members; implicit copy/move assignment is inserted during replayed member-call
selection and wins equal-rank overload ties over a member template; local
static identities use the final template function object; explicit
specialization facts survive AST/template replay into global records; and
primary-template values remain protected from stale specialization data, while
concrete explicit-specialization storage is folded from its own initializer.
It also fixes nested owner rewriting for generated function definitions and the
ABI name of nested member-template local statics.

Validation for this scope is the focused **5/6** result above, the full
PA21-local report at **170/215**, the through-PA20 report at **1635/1635**,
and the PA21 file audit (pass, with warnings only).  The next checkpoint group
is the four-case explicit-specialization/storage merge slice:

- `general/300-dependent-hidden-friend-static-member-definition`
- `spec/300-explicit-specialization-out-of-class-member-overrides-primary`
- `spec/300-explicit-specialization-refreshes-stale-primary-instantiation`
- `spec/300-explicit-specialization-after-instantiation`

## Checkpoint result: 153/215 (2026-07-23)

The required report, `make test-report ACTIVE_TEST_REPORT_PAS='pa21'`, is
**153/215 passing** with **62 failures**, improving the turn-start baseline
of **146/215** by seven tests.  The required through report remains
**1635/1635 through PA20**.  The map below is refreshed from the complete
post-checkpoint report; every failure appears exactly once and is grouped by
shared compiler behavior.

### Remaining Work Map

- **Member/friend owner replay, lookup, ADL, and generated members (25):**
	`general/300-basic-template-operator-overloads`,
	`general/300-crtp-static-cast-reference-before-constructor-template`,
	`general/300-dependent-base-qualified-rvalue-assignment`,
	`general/300-dependent-hidden-friend-static-member-definition`,
	`general/300-explicit-member-template-id-shares-ordinary-overload`,
	`general/300-explicit-type-arg-decltype-member-access`,
	`general/300-friend-existing-template-private-ctor-access`,
	`general/300-function-pack-template-id-deduction-decltype`,
	`general/300-inherited-member-template-subscript-action`,
	`general/300-local-qualified-argument-replay`,
	`general/300-member-template-assignment-operator-value`,
	`general/300-member-template-local-using-does-not-suppress-adl`,
	`general/300-nested-class-template-current-owner-lookup`,
	`general/300-nested-class-template-reference-reset`,
	`general/300-out-of-class-ctor-using-imported-member-template`,
	`general/300-parenthesized-qualified-template-functional-call`,
	`general/300-qualified-friend-member-template-access`,
	`general/300-unary-member-operator-template-default`,
	`spec/300-member-call-template-hides-inherited-instantiation`,
	`spec/300-member-class-template-out-of-class`,
	`spec/300-member-operator-template-active-owner`,
	`spec/300-member-operator-template-in-class-template`,
	`spec/300-rooted-qualified-static-data-member-template-definition`,
	`spec/300-using-imported-member-template-active-owner`, and
	`spec/300-using-inherited-alias-operator-template`.

- **Alias, template-template, and dependent non-type/type replay (14):**
  `general/200-reference-alias-top-cv-return-binding`,
  `general/400-alias-nontype-expression-declaration-scope`,
  `general/400-alias-nontype-pack-partial-specialization-pattern`,
  `general/400-alias-pack-nontype-expression-fast-path`,
  `general/400-alias-template-nontype-shadowed-by-inner-value`,
  `general/400-alias-template-pointer-cv-cache-distinction`,
  `general/400-dependent-pack-typename-nontype-expression`,
  `general/400-function-parameter-pack-alias-expansion`,
  `general/400-inline-namespace-template-template-argument`,
  `general/400-member-alias-template-template-dependent-replay`,
  `general/400-member-template-nontype-shadowed-global-replay`,
  `general/400-reference-member-depth-pack-sum`,
  `general/400-reference-member-lookup-in-progress-base-typedef`, and
  `spec/100-inline-namespace-qualified-template-id-pack-expansion`.

- **Partial-specialization and current-specialization identity (7):**
  `general/100-reference-shell-out-of-class-current-specialization-iterator`,
  `general/300-function-signature-partial-specialization-functor-assignment`,
  `general/300-nested-member-partial-specialization-apply-scope`,
  `general/400-forward-primary-partial-switch-value`,
  `general/400-partial-specialization-conversion-operator-pointer-binding`,
  `general/400-partial-specialization-redecl-member-template-empty-pack`, and
  `spec/300-defaulted-type-arg-specialization-nontype-value`.

- **Explicit specialization/instantiation ownership and diagnostics (8):**
  `general/300-extern-template-builtin-operator-function-declaration-bad`,
  `general/300-member-class-explicit-specialization-owner-lookup`,
  `general/300-template-instantiation-use-location-explicit-specialization`,
  `spec/300-explicit-instantiation-static-member-function`,
  `spec/300-explicit-specialization-after-instantiation`,
  `spec/300-explicit-specialization-refreshes-stale-primary-instantiation`,
  `spec/300-explicit-specialization-static-data-member`, and
  `spec/300-explicit-specialized-ctor-template-header-bad`.

- **Dependent lowering, address/initialization, and ordinary expression replay (8):**
  `general/100-rvalue-reference-binds-converted-temporary`,
  `general/300-anonymous-union-storage-constructor-noop`,
  `general/300-array-functional-cast-pack-call`,
  `general/300-constexpr-static-fn-template-address-pack`,
  `general/300-function-template-local-static-per-specialization`,
  `general/300-namespace-function-template-hides-outer-callable-object`,
  `general/300-single-pack-cast-target`, and
  `general/300-static-constexpr-function-template-pointer-array`.

### Checkpoint Scope

This checkpoint completes the typed friend-owner/access slice of the current
member/friend group.  Replayed friend function and friend class declarations
must retain the declaring class specialization, namespace-level function or
class identity, and the access relationship used while lowering the generated
body.  It covers ordinary and qualified friend function templates, nested
class friend replay, existing-template friend definitions, template friend
class access, and private/protected members reached through a friend class.

The focused validation set is:

`general/300-friend-function-template-access`,
`general/300-friend-existing-template-private-ctor-access`,
`general/300-nested-class-friend-template-namespace-scope`,
`general/300-qualified-friend-member-template-access`,
`general/300-template-friend-class-constructor-access`,
`spec/300-friend-class-template-nested-private-typedef-access`,
`spec/300-friend-class-template-protected-base-access`,
`spec/300-qualified-friend-function-template-member-access`, and
`spec/300-template-friend-inside-class-template`.

The selected scope is the shared typed ownership/access behavior across these
nine tests, not test-specific acceptance.  Earlier assignments must remain at
1635/1635, and the full PA21 report must improve beyond the 146/215 baseline.

### Checkpoint Result and Next Scope

Implemented the typed friend-owner/access slice across PA18 replay, PA11
friend metadata, PA14 access checks, dependent `typename` construction, and
generated friend-class inheritance ordering.  The audit removed the synthetic
empty friend declaration and its PA11/PA14 bypass, replaced untyped friend-name
recovery with typed access edges, narrowed layout reordering to structured
friend declarations, and indexed derived friend owners for access checks.  The
nine-test focused run had **7/9** reference-comparison passes: all nine
compile/status checks pass, with two relaxed-LowIR presentation mismatches in
the existing-template friend operator and qualified friend member cases.  The
full report remains **153/215** (the checkpoint baseline), the through-PA20
report is **1635/1635**, and file audit passes with warnings only.

The next checkpoint group is inherited/using member-template owner replay:
preserve the active declaring class through using-imported and dependent-base
lookup, then validate operator/member-call selection and qualified member
definitions.  Focus next on
`general/300-inherited-member-template-subscript-action`,
`general/300-member-template-local-using-does-not-suppress-adl`,
`spec/300-member-call-template-hides-inherited-instantiation`,
`spec/300-using-imported-member-template-active-owner`, and
`spec/300-using-inherited-alias-operator-template`.

## Current turn checkpoint scope (before implementation)

The turn-start report was 153/215 with 62 failures; the grouped path
inventory above is the complete source set for this checkpoint.  No failure
is being moved between groups for this checkpoint.

This checkpoint selects the five-test inherited/using member-template owner
group named by the previous handoff.  It covers one typed behavior: when a
member template is reached through a dependent base or a class
using-declaration, replay must retain the class specialization that declared
the member, select the derived direct member over an inherited fallback, and
lower overloaded `operator[]`, member calls, ADL-sensitive calls, and
`operator=` through the ordinary selected-function path.  The focused
validation set is:

`general/300-inherited-member-template-subscript-action`,
`general/300-member-template-local-using-does-not-suppress-adl`,
`spec/300-member-call-template-hides-inherited-instantiation`,
`spec/300-using-imported-member-template-active-owner`, and
`spec/300-using-inherited-alias-operator-template`.

The checkpoint is substantial because it spans PA18 typed owner replay and
the PA14 call/assignment/subscript consumers; it is not an acceptance rule
for any individual fixture.  Success requires these five focused tests to
pass, the full PA21 count to increase above 153/215 (or finish PA21), and the
through-PA20 report to remain 1635/1635.

## Archived 151/215 checkpoint

### Remaining Work Map

The required report, `make test-report ACTIVE_TEST_REPORT_PAS='pa21'`, is
**151/215 passing** with **64 failures**.  This is +32 over the 119/215
turn-start baseline and +3 over the previous 148/215 checkpoint.  The complete
current failure set is grouped by shared compiler behavior below; every current
failure appears once.

- **Member/friend owner replay, lookup, ADL, and generated members (28):**
  `general/300-basic-template-operator-overloads`,
  `general/300-crtp-static-cast-reference-before-constructor-template`,
  `general/300-dependent-base-qualified-rvalue-assignment`,
  `general/300-dependent-hidden-friend-static-member-definition`,
  `general/300-explicit-member-template-id-shares-ordinary-overload`,
  `general/300-explicit-type-arg-decltype-member-access`,
  `general/300-friend-existing-template-private-ctor-access`,
  `general/300-friend-function-template-access`,
  `general/300-function-pack-template-id-deduction-decltype`,
  `general/300-hidden-friend-template-call-adl`,
  `general/300-local-qualified-argument-replay`,
  `general/300-member-template-assignment-not-special-member`,
  `general/300-member-template-assignment-operator-value`,
  `general/300-member-template-local-using-does-not-suppress-adl`,
  `general/300-nested-class-friend-template-namespace-scope`,
  `general/300-nested-class-template-current-owner-lookup`,
  `general/300-nested-class-template-reference-reset`,
  `general/300-parenthesized-qualified-template-functional-call`,
  `general/300-qualified-friend-member-template-access`,
  `general/300-template-friend-class-constructor-access`,
  `general/300-unary-member-operator-template-default`,
  `general/400-reference-member-lookup-in-progress-base-typedef`,
  `spec/300-friend-class-template-nested-private-typedef-access`,
  `spec/300-hidden-friend-template-call-adl`,
  `spec/300-hidden-friend-template-operator-adl`,
  `spec/300-member-operator-template-active-owner`,
  `spec/300-qualified-friend-function-template-member-access`, and
  `spec/300-template-friend-inside-class-template`.

- **Alias, template-template, and dependent non-type replay (13):**
  `general/200-reference-alias-top-cv-return-binding`,
  `general/400-alias-nontype-expression-declaration-scope`,
  `general/400-alias-nontype-pack-partial-specialization-pattern`,
  `general/400-alias-pack-nontype-expression-fast-path`,
  `general/400-alias-template-nontype-shadowed-by-inner-value`,
  `general/400-alias-template-pointer-cv-cache-distinction`,
  `general/400-function-parameter-pack-alias-expansion`,
  `general/400-inline-namespace-template-template-argument`,
  `general/400-member-template-nontype-shadowed-global-replay`,
  `general/400-out-of-namespace-class-template-member-result`,
  `general/400-reference-member-depth-pack-sum`,
  `spec/100-inline-namespace-qualified-template-id-pack-expansion`, and
  `spec/100-nontype-static-outdef-value-member-preserves-type`.

- **Partial-specialization and current-specialization identity (8):**
  `general/100-reference-shell-out-of-class-current-specialization-iterator`,
  `general/300-function-signature-partial-specialization-functor-assignment`,
  `general/300-nested-member-partial-specialization-apply-scope`,
  `general/400-forward-primary-partial-switch-value`,
  `general/400-partial-specialization-concrete-namespace-argument-order`,
  `general/400-partial-specialization-conversion-operator-pointer-binding`,
  `general/400-partial-specialization-redecl-member-template-empty-pack`, and
  `spec/300-defaulted-type-arg-specialization-nontype-value`.

- **Explicit specialization/instantiation ordering and diagnostics (6):**
  `general/300-extern-template-builtin-operator-function-declaration-bad`,
  `general/300-member-class-explicit-specialization-owner-lookup`,
  `general/300-template-instantiation-use-location-explicit-specialization`,
  `spec/300-explicit-specialization-after-instantiation`,
  `spec/300-explicit-specialization-static-data-member`, and
  `spec/300-explicit-specialized-ctor-template-header-bad`.

- **Dependent values and ordinary lowering/initialization (9):**
  `general/100-rvalue-reference-binds-converted-temporary`,
  `general/200-lazy-header-constexpr-static-assert`,
  `general/300-anonymous-union-storage-constructor-noop`,
  `general/300-array-functional-cast-pack-call`,
  `general/300-constexpr-static-fn-template-address-pack`,
  `general/300-function-template-local-static-per-specialization`,
  `general/300-namespace-function-template-hides-outer-callable-object`,
  `general/300-single-pack-cast-target`, and
  `general/300-static-constexpr-function-template-pointer-array`.

### Checkpoint Scope

This completed owner-replay increment covers qualified out-of-class member
definitions: class collection no longer duplicates an entered class scope;
owner matching binds renamed parameters positionally; nested owner arguments
are replayed separately from nested arguments; generated member calls retain
the concrete owner, overload identity, declaration parameter names, and
static-member facts; and rooted static data definitions merge with their
class-member storage.  It also covers the corresponding LowIR address,
constructor, empty-derived-object, and reference-initialization behavior.

Validation for this scope is the six qualified owner fixtures at 6/6, the
explicit/extern and owner regression set at 13/13, and the fresh full PA21
report at 151/215.  The selected scope is complete; the next checkpoint is
the 28-test member/friend owner and ADL group, beginning with hidden-friend
lookup and dependent-base member selection.

### Checkpoint Result and Next Scope

The current implementation is +32 tests over the turn-start baseline and
preserves the prior explicit-instantiation and owner regressions.  The next
group should extend the same typed owner state through hidden friends, ADL,
dependent bases, using-declarations, and member-template assignment/operator
selection; validate that group against the owner and explicit regression sets,
then rerun the full PA21 report.

## Previous 148/215 checkpoint

### Remaining Work Map

The fresh required PA21-local report,
`make test-report ACTIVE_TEST_REPORT_PAS='pa21'`, is **148/215 passing** with
**67 failures**.  The turn-start baseline was 119/215, so this increment is
already +29 tests.  The complete current failure set is grouped below by
shared compiler behavior; each path appears exactly once.

- **Member/friend owner replay, lookup, and generated members (33):**
  `general/300-basic-template-operator-overloads`,
  `general/300-crtp-static-cast-reference-before-constructor-template`,
  `general/300-dependent-base-qualified-rvalue-assignment`,
  `general/300-dependent-hidden-friend-static-member-definition`,
  `general/300-explicit-member-template-id-shares-ordinary-overload`,
  `general/300-explicit-type-arg-decltype-member-access`,
  `general/300-friend-existing-template-private-ctor-access`,
  `general/300-friend-function-template-access`,
  `general/300-function-pack-template-id-deduction-decltype`,
  `general/300-hidden-friend-template-call-adl`,
  `general/300-local-qualified-argument-replay`,
  `general/300-member-template-assignment-not-special-member`,
  `general/300-member-template-assignment-operator-value`,
  `general/300-member-template-local-using-does-not-suppress-adl`,
  `general/300-nested-class-friend-template-namespace-scope`,
  `general/300-nested-class-template-current-owner-lookup`,
  `general/300-nested-class-template-reference-reset`,
  `general/300-parenthesized-qualified-template-functional-call`,
  `general/300-qualified-friend-member-template-access`,
  `general/300-template-friend-class-constructor-access`,
  `general/300-unary-member-operator-template-default`,
  `general/400-reference-member-lookup-in-progress-base-typedef`,
  `spec/300-friend-class-template-nested-private-typedef-access`,
  `spec/300-hidden-friend-template-call-adl`,
  `spec/300-hidden-friend-template-operator-adl`,
  `spec/300-member-class-template-out-of-class`,
  `spec/300-out-of-class-member-template-owner-param-rename`,
  `spec/300-out-of-class-member-template-tag-dispatch-definition`,
  `spec/300-out-of-class-overloaded-member-template-definition`,
  `spec/300-qualified-friend-function-template-member-access`,
  `spec/300-rooted-qualified-static-data-member-template-definition`,
  `spec/300-static-member-function-template-out-of-class`, and
  `spec/300-template-friend-inside-class-template`.

- **Alias/template-template and dependent non-type replay (13):**
  `general/200-reference-alias-top-cv-return-binding`,
  `general/400-alias-nontype-expression-declaration-scope`,
  `general/400-alias-nontype-pack-partial-specialization-pattern`,
  `general/400-alias-pack-nontype-expression-fast-path`,
  `general/400-alias-template-nontype-shadowed-by-inner-value`,
  `general/400-alias-template-pointer-cv-cache-distinction`,
  `general/400-dependent-pack-typename-nontype-expression`,
  `general/400-function-parameter-pack-alias-expansion`,
  `general/400-inline-namespace-template-template-argument`,
  `general/400-member-alias-template-template-dependent-replay`,
  `general/400-member-template-nontype-shadowed-global-replay`,
  `spec/100-inline-namespace-qualified-template-id-pack-expansion`, and
  `general/400-forward-primary-partial-switch-value`.

- **Partial-specialization and current-specialization identity (6):**
  `general/100-reference-shell-out-of-class-current-specialization-iterator`,
  `general/300-function-signature-partial-specialization-functor-assignment`,
  `general/300-nested-member-partial-specialization-apply-scope`,
  `general/400-partial-specialization-conversion-operator-pointer-binding`,
  `general/400-partial-specialization-redecl-member-template-empty-pack`, and
  `spec/300-defaulted-type-arg-specialization-nontype-value`.

- **Explicit specialization/instantiation ordering (6):**
  `general/300-extern-template-builtin-operator-function-declaration-bad`,
  `general/300-member-class-explicit-specialization-owner-lookup`,
  `general/300-template-instantiation-use-location-explicit-specialization`,
  `spec/300-explicit-specialization-after-instantiation`,
  `spec/300-explicit-specialization-static-data-member`,
  `spec/300-explicit-specialized-ctor-template-header-bad`.

- **Dependent values and ordinary lowering/layout (9):**
  `general/100-rvalue-reference-binds-converted-temporary`,
  `general/300-anonymous-union-storage-constructor-noop`,
  `general/300-array-functional-cast-pack-call`,
  `general/300-constexpr-static-fn-template-address-pack`,
  `general/300-function-template-local-static-per-specialization`,
  `general/300-namespace-function-template-hides-outer-callable-object`,
  `general/300-single-pack-cast-target`,
  `general/300-static-constexpr-function-template-pointer-array`, and
  `general/400-reference-member-depth-pack-sum`.

The exact failure modes are in `/tmp/pa21-full-report-4.log`; the map above is
the implementation view used for the next checkpoint.

### Checkpoint Scope

This completed increment covers template entity identity and materialization:
declaration-identity keys distinguish overloaded extern requests; positive
explicit instantiation promotes an extern declaration back to a definition;
extern function, constructor, operator, class, and deduced free-function
requests emit declaration-only roots; and template metadata remains monotonic
when generated declarations merge with source declarations.  The call-rewrite
ordinary-overload guard still performs deduction for an exact registered extern
specialization.

Validation is 148/215 on the full local report.  The positive explicit and
extern-template focused set is 10/10, including overload identity and deduced
free-function cases.  File audit is still pending for the final handoff.

### Checkpoint Result and Next Scope

The implementation is +29 tests over the 119/215 turn-start baseline and +6
over the previous clean 142/215 checkpoint.  The next substantial group is
qualified out-of-class member-template owner replay:
`spec/300-member-class-template-out-of-class`,
`spec/300-out-of-class-member-template-owner-param-rename`,
`spec/300-out-of-class-member-template-tag-dispatch-definition`,
`spec/300-out-of-class-overloaded-member-template-definition`,
`spec/300-rooted-qualified-static-data-member-template-definition`, and
`spec/300-static-member-function-template-out-of-class`.

Implement one owner-aware declaration/replay path for these qualified
definitions, preserving generated owner and overload identity.  Validate this
focused group plus the existing owner/pack and explicit-instantiation
regressions, then rerun the full PA21 report and regroup the remaining map.

## Prior 122/215 checkpoint archive

### Remaining Work Map (after checkpoint)

The required PA21-local report was rerun after this checkpoint with
`make test-report ACTIVE_TEST_REPORT_PAS='pa21'`.  It is **122/215 passing**
with **93 failures**: 82 exit-status mismatches and 11 relaxed-LowIR
mismatches.  There are no timeout failures.  The complete remaining failure
set is grouped below by shared compiler behavior; each reported path appears
exactly once.

- **Member/friend owner replay and lookup (56):**
  `general/300-basic-template-operator-overloads`,
  `general/300-class-template-member-overload-braced-call-operator`,
  `general/300-crtp-static-cast-reference-before-constructor-template`,
  `general/300-dependent-base-qualified-rvalue-assignment`,
  `general/300-dependent-hidden-friend-static-member-definition`,
  `general/300-explicit-member-call-function-template-id`,
  `general/300-explicit-member-template-id-shares-ordinary-overload`,
  `general/300-explicit-type-arg-decltype-member-access`,
  `general/300-friend-existing-template-private-ctor-access`,
  `general/300-friend-function-template-access`,
  `general/300-function-pack-template-id-deduction-decltype`,
  `general/300-hidden-friend-template-call-adl`,
  `general/300-inherited-member-template-subscript-action`,
  `general/300-local-qualified-argument-replay`,
  `general/300-member-class-explicit-specialization-owner-lookup`,
  `general/300-member-template-assignment-operator-value`,
  `general/300-member-template-class-pack-forward-before-token`,
  `general/300-member-template-local-using-does-not-suppress-adl`,
  `general/300-namespace-function-template-hides-outer-callable-object`,
  `general/300-nested-class-friend-template-namespace-scope`,
  `general/300-nested-class-template-current-owner-lookup`,
  `general/300-nested-class-template-reference-reset`,
  `general/300-nested-member-partial-specialization-apply-scope`,
  `general/300-nested-member-partial-specialization-survives-reference-reset`,
  `general/300-out-of-class-ctor-using-imported-member-template`,
  `general/300-parenthesized-qualified-template-functional-call`,
  `general/300-qualified-friend-member-template-access`,
  `general/300-qualified-preselected-member-template-call`,
  `general/300-single-pack-cast-target`,
  `general/300-template-friend-class-constructor-access`,
  `general/300-templated-constructor-special-member-collection`,
  `general/300-unary-member-operator-template-default`,
  `general/300-using-base-same-signature-derived-template-preferred`,
  `general/400-reference-member-lookup-in-progress-base-typedef`,
  `spec/300-const-member-function-template-overload`,
  `spec/300-dependent-super-member-template-chain`,
  `spec/300-friend-class-template-nested-private-typedef-access`,
  `spec/300-friend-class-template-protected-base-access`,
  `spec/300-hidden-friend-template-call-adl`,
  `spec/300-hidden-friend-template-operator-adl`,
  `spec/300-member-call-template-hides-inherited-instantiation`,
  `spec/300-member-class-template-out-of-class`,
  `spec/300-member-operator-template-active-owner`,
  `spec/300-member-operator-template-in-class-template`,
  `spec/300-member-template-cache-hit-concrete-scope`,
  `spec/300-noexcept-member-template-call-operator`,
  `spec/300-out-of-class-member-template-owner-param-rename`,
  `spec/300-out-of-class-member-template-tag-dispatch-definition`,
  `spec/300-out-of-class-overloaded-member-template-definition`,
  `spec/300-qualified-friend-function-template-member-access`,
  `spec/300-rooted-qualified-static-data-member-template-definition`,
  `spec/300-static-member-function-template-out-of-class`,
  `spec/300-template-friend-inside-class-template`,
  `spec/300-using-imported-member-template-active-owner`,
  `spec/300-using-inherited-alias-operator-template`, and
  `spec/300-qualified-member-template-hides-base-function`.

- **Alias/template-template and dependent non-type replay (13):**
  `general/200-reference-alias-top-cv-return-binding`,
  `general/400-alias-nontype-expression-declaration-scope`,
  `general/400-alias-nontype-pack-partial-specialization-pattern`,
  `general/400-alias-pack-nontype-expression-fast-path`,
  `general/400-alias-template-nontype-shadowed-by-inner-value`,
  `general/400-alias-template-pointer-cv-cache-distinction`,
  `general/400-dependent-pack-typename-nontype-expression`,
  `general/400-function-parameter-pack-alias-expansion`,
  `general/400-inline-namespace-template-template-argument`,
  `general/400-member-alias-template-owner-rebind-cache`,
  `general/400-member-alias-template-template-dependent-replay`,
  `spec/100-inline-namespace-qualified-template-id-pack-expansion`, and
  `general/400-member-template-nontype-shadowed-global-replay`.

- **Class partial-selection and current-specialization identity (5):**
  `general/100-reference-shell-out-of-class-current-specialization-iterator`,
  `general/400-partial-specialization-conversion-operator-pointer-binding`,
  `general/400-partial-specialization-redecl-member-template-empty-pack`,
  `spec/300-defaulted-type-arg-specialization-nontype-value`, and
  `general/400-forward-primary-partial-switch-value`.

- **Explicit specialization/instantiation ownership (12):**
  `general/300-template-instantiation-use-location-explicit-specialization`,
  `spec/300-explicit-instantiation-class`,
  `spec/300-explicit-instantiation-deduced-member-function-template`,
  `spec/300-explicit-instantiation-free-function-emits-definition`,
  `spec/300-explicit-instantiation-function`,
  `spec/300-explicit-instantiation-static-member-function`,
  `spec/300-explicit-specialization-after-instantiation`,
  `spec/300-explicit-specialization-static-data-member`,
  `spec/300-explicit-specialized-ctor-template-header-bad`,
  `spec/300-extern-template-constructor-declaration`,
  `spec/300-extern-template-function-call-suppresses-materialization`,
  `spec/300-extern-template-operator-function-declaration`.

- **Dependent values and ordinary lowering/layout (7):**
  `general/300-anonymous-union-storage-constructor-noop`,
  `general/300-array-functional-cast-pack-call`,
  `general/300-constexpr-static-fn-template-address-pack`,
  `general/300-function-signature-partial-specialization-functor-assignment`,
  `general/300-function-template-local-static-per-specialization`,
  `general/300-static-constexpr-function-template-pointer-array`, and
  `general/400-reference-member-depth-pack-sum`.

The report's count is authoritative for the stage gate.  The path lists above
are the current report inventory; the broad historical group descriptions below
are retained only as provenance.

### Checkpoint Result

This checkpoint completed the first owner-reconstruction slice of the
member/friend group.  It adds a typed member-template fact so enclosing class
parameters are not confused with member parameters, replays out-of-class
member definitions with owner-pattern substitutions, preserves namespace-level
static-data ownership, rejects a missing dependent `template` disambiguator,
and selects explicit member specializations before primary definitions.

Focused validation passed for:

- `general/300-explicit-member-function-template-call`
- `general/300-nondependent-member-template-id-call`
- `general/300-out-of-class-member-function-template-definition`
- `general/300-out-of-class-member-template-namespace-typedef`
- `general/100-partial-specialization-member-typedef-outdef`
- `general/100-partial-specialization-nested-template-id-member-outdef`
- `general/300-partial-owner-out-of-class-member-binding-args`
- `general/300-dependent-member-template-missing-keyword-bad`
- `general/400-bool-or-dependent-member-type-conditional-base`
- `spec/300-explicit-specialization-member-function`
- `spec/300-explicit-specialization-out-of-class-member-overrides-primary`

The full report improved from the 119-test turn-start baseline to 122/215.
The selected parser, inherited lookup, and member-template overload cases that
remain failures are listed in the remaining-work map above; no fixture-specific
branching was added.

### Next Checkpoint Group

Handle qualified member-template lookup through aliases and dependent bases:
route qualified-id calls such as `super::insert_` through the same owner-aware
member-call path, then extend it to inherited/using and friend-member lookup.
Validate `spec/300-qualified-member-template-hides-base-function` first, then
the remaining qualified/preselected and inherited member-template cases from
the member/friend group.  Keep the alias/template-template and explicit
instantiation groups unchanged until this lookup slice is stable.

### Turn-start audit

The audit-turn stage baseline was **118/215 PA21 tests**, which is the result
of the landed checkpoint under review.  Its complete current-PA failure set
was inspected before the audit and grouped by shared behavior: dependent
value/member replay, generated pack and base layout, alias/template-template
entities, member/friend owner lookup and overload/ADL, explicit instantiation
ownership, and ordinary lowering/layout.

### Checkpoint Scope

The landed checkpoint completes the dependent value and generated-layout
increment:

- preserve typed non-type values while recursively replaying variadic static
  members and global typedefs;
- retain reference/array declarator shells through alias and nested-member
  lookup;
- handle empty pack completion and concrete global template-member replay;
- carry all direct template bases into semantic layout and lower base-owner
  address adjustments; and
- materialize local class addresses for deferred `sizeof` layout queries.

Validation covered the recursive 64-element depth sum, nested outer-type pack,
reference-array alias, template-template arity, deferred incomplete-layout,
conditional/dependent owner, variadic-base, empty-pack, explicit-specialization
and generated-base probes.  The complete report was rerun after cleanup.

### Checkpoint Result

The focused depth-sum fixture passes with generated value **64** (previously
**1**), and the empty-pack/generated-base and alias declarator probes remain
passing.  The landed checkpoint report was **118/215**, an improvement of
**22 tests** over the implementation-turn baseline and **2 tests** over the
prior checkpoint.

The two through-PA20 regressions found during validation were repaired as
well: qualified generated-owner member replay now preserves the resolved
member type, and dependent static initializers remain deferred until concrete
substitutions exist.  The audit also repaired one invalid-LowIR result caused
by using-directive function names entering scalar substitutions, promoted the
specialization matcher into an audited `.cpp` source unit, and replaced the
reserved owner-substitution marker with scoped typed owner state.  The
post-audit report is **119/215**, so current-PA progress is preserved and
improved by one test; through PA20 remains **1635/1635**.

### Remaining Work Map

The post-audit report has **96** failures.  The complete report set is covered
by the following groups; the one removed entry was the invalid-LowIR local
using-directive case:

- **Member/friend owner replay and lookup (52):** out-of-class and nested
  member-template definitions, using imports, hidden friends/ADL, inherited
  members, explicit member calls, operators, and overload selection.
- **Alias/template-template and dependent non-type replay (17):** alias
  partials, template-template arity/entity binding, pointer/cv distinctions,
  pack expressions, declaration-scope values, and inline-namespace aliases.
- **Explicit specialization/instantiation ownership (18):** explicit and
  extern instantiation materialization, specialization ordering, constructors,
  static data/functions, and use-location replay.
- **Partial-selection and ordinary lowering/layout (9):** remaining
  reference/cv/pack selection, incomplete-class checks, anonymous storage,
  static-constexpr array arguments, and LowIR/ABI presentation.

### Next Checkpoint Group

Take the member/friend owner replay group, starting with explicit and
qualified member-template calls plus out-of-class owner reconstruction; then
validate the related using/inherited/ADL fixtures together.  Keep the alias
and explicit-instantiation groups separate unless a shared owner-state fix
proves to cover them without changing the completed value/layout behavior.


## Historical 62/215 failure map and prior checkpoint scope

The superseded pre-entity report was **62/215 passing** with assignments
through PA20 passing.  Its complete **153-test** failure set was inspected
before implementation: 138 exit-status mismatches, 14 relaxed-LowIR
mismatches, and one invalid-LowIR result.  There were no timeout failures.  The
paths below are retained as historical provenance for that earlier checkpoint,
not as the current PA21 result.

### A — remaining class partial-specialization matching and ordering (34)

`general/100-partial-specialization-pack-expansion-value-pattern`,
`general/100-reference-shell-out-of-class-current-specialization-iterator`,
`general/100-relative-qualified-partial-specialization`,
`general/100-rvalue-reference-binds-converted-temporary`,
`general/200-partial-specialization-nested-template-id-pack-expansion`,
`general/400-bool-or-dependent-member-type-conditional-base`,
`general/400-cv-qualified-template-id-wrapper-class-partial-specialization`,
`general/400-defaulted-nested-cv-template-template-partial-specialization`,
`general/400-dependent-owner-member-class-template-partial-specialization`,
`general/400-forward-function-type-partial-specialization-pack-arity`,
`general/400-forward-primary-partial-switch-value`,
`general/400-function-type-cv-partial-specialization`,
`general/400-function-type-partial-specialization-preference`,
`general/400-function-type-ref-qualified-partial-specialization`,
`general/400-nested-function-type-argument-partial-specialization`,
`general/400-nontype-pack-fixed-tail-partial-specialization-ordering`,
`general/400-partial-specialization-concrete-namespace-argument-order`,
`general/400-partial-specialization-conversion-operator-pointer-binding`,
`general/400-partial-specialization-cv-ref-ordering`,
`general/400-partial-specialization-fixed-nontype-cv-ref`,
`general/400-partial-specialization-member-template-id-cv-mismatch`,
`general/400-partial-specialization-nontype-pattern-order`,
`general/400-repeated-pack-partial-specialization-ordering`,
`general/400-trailing-pack-partial-specialization-common-type-member`,
`general/400-void-head-pack-partial-specialization-ordering`,
`spec/100-function-type-pack-partial-specialization-replay`,
`spec/100-function-type-top-level-cv-partial-specialization`,
`spec/100-inline-namespace-qualified-template-id-pack-expansion`,
`spec/100-nontype-static-outdef-value-member-preserves-type`,
`spec/100-partial-specialization-cv-pointer-selection`,
`spec/100-qualified-function-type-partial-specialization`,
`spec/100-variadic-base-pack-expansion`,
`spec/300-defaulted-type-arg-specialization-nontype-value`, and
`spec/300-variable-template-defaulted-trailing-specialization`.

### B — alias/variable-template entities and template-template binding (34)

`general/200-adl-template-template-argument-namespace`,
`general/200-deferred-incomplete-member-layout-alias-value-type`,
`general/200-dependent-remove-cv-transform-alias-substitution`,
`general/200-qualified-template-template-variable-partial`,
`general/300-member-template-as-template-template-argument`,
`general/300-template-template-trailing-pack-rebind-function-pointer`,
`general/300-variable-template-forwarding-partial-top-cv`,
`general/300-variable-template-run-specialization-selection`,
`general/400-alias-nontype-expression-declaration-scope`,
`general/400-alias-nontype-pack-partial-specialization-pattern`,
`general/400-alias-pack-nontype-expression-fast-path`,
`general/400-alias-rebind-partial-specialization-shadow`,
`general/400-alias-template-decltype-greater-type-argument`,
`general/400-alias-template-decltype-member-type-argument`,
`general/400-alias-template-decltype-shift-type-argument`,
`general/400-alias-template-nontype-shadowed-by-inner-value`,
`general/400-alias-template-pack-id-preserves-syntax`,
`general/400-alias-template-pointer-cv-cache-distinction`,
`general/400-alias-value-expression-type-argument`,
`general/400-concrete-template-head-beats-template-template-pack`,
`general/400-dependent-alias-helper-partial-specialization`,
`general/400-dependent-alias-member-template-id-defer`,
`general/400-dependent-pack-typename-nontype-expression`,
`general/400-function-parameter-pack-alias-expansion`,
`general/400-member-alias-template-owner-rebind-cache`,
`general/400-member-alias-template-template-dependent-replay`,
`general/400-member-template-nontype-shadowed-global-replay`,
`general/400-nontype-pack-comma-expression-syntax`,
`general/400-nttp-pack-void-comma-expression`,
`general/400-qualified-base-type-alias-from-nontype-pack`,
`general/400-qualified-template-id-current-scope-alias-shadow`,
`general/400-template-template-arity-incomplete-partial`,
`general/400-template-template-fixed-prefix-pack-order`, and
`spec/200-template-template-parameter-arity-mismatch`.

### C — member/friend-template owner and lookup replay (60)

The 59 current paths in the historical C group remain failures, plus
`spec/300-friend-class-template-nested-private-typedef-access`; they cover
member/friend declarations, inherited lookup, ADL/overload selection, nested
owners, and out-of-class definitions.

### D — explicit specialization/instantiation ownership (12)

The 12 current paths in the historical D group remain failures; they cover
explicit specialization ordering, explicit instantiation, and extern-template
materialization.

### E — dependent constants, layout, and ordinary lowering (13)

The 13 current paths in the historical E group remain failures; they cover
dependent constant replay, object layout, and LowIR ownership.

### Checkpoint Scope

This turn takes Group B as one substantial coherent increment: represent
template-template parameters and arguments as typed template entities with
arity/pack/default matching; make alias templates resolve through the same
argument-normalization path as class templates; select and materialize
variable-template partial specializations; and preserve typed non-type values
and dependent `decltype`/pack expressions while replaying aliases.  Validation
will cover the Group B fixtures, then the full PA21 report, through-PA20
report, and file audit.

## Turn-start baseline

The turn-start PA21 report was **47/215 tests passing**, with assignments
through PA20 passing.  The complete current-PA failure set was read from the
required report before implementation: **168 failures** consisting of 141
exit-status mismatches, 24 relaxed-LowIR mismatches, one invalid LowIR, and
two timeouts.  The paths below are grouped by the shared compiler behavior
they expose; each failing path appears once.

## Remaining Work Map recorded before implementation

### A — class partial-specialization identity, matching, and ordering

These fixtures exercise the class-template declaration graph, typed
specialization keys, current-specialization rewriting, pack/reference/cv
pattern matching, and selection of the owning partial-specialization body:

`general/100-nested-pack-expansion-outer-type-pack`,
`general/100-partial-specialization-member-typedef-outdef`,
`general/100-partial-specialization-nested-template-id-member-outdef`,
`general/100-partial-specialization-pack-expansion-value-pattern`,
`general/100-reference-shell-out-of-class-current-specialization-iterator`,
`general/100-relative-qualified-partial-specialization`,
`general/100-rvalue-reference-binds-converted-temporary`,
`general/100-top-cv-pointer-does-not-match-unqualified-pointer-partial`,
`general/200-partial-specialization-nested-template-id-pack-expansion`,
`general/400-bool-or-dependent-member-type-conditional-base`,
`general/400-cv-qualified-template-id-wrapper-class-partial-specialization`,
`general/400-defaulted-nested-cv-template-template-partial-specialization`,
`general/400-dependent-owner-member-class-template-partial-specialization`,
`general/400-forward-function-type-partial-specialization-pack-arity`,
`general/400-forward-primary-partial-switch-value`,
`general/400-function-type-cv-partial-specialization`,
`general/400-function-type-partial-specialization-preference`,
`general/400-function-type-ref-qualified-partial-specialization`,
`general/400-nested-function-type-argument-partial-specialization`,
`general/400-nontype-pack-fixed-tail-partial-specialization-ordering`,
`general/400-partial-specialization-concrete-namespace-argument-order`,
`general/400-partial-specialization-conversion-operator-pointer-binding`,
`general/400-partial-specialization-cv-ref-ordering`,
`general/400-partial-specialization-fixed-nontype-cv-ref`,
`general/400-partial-specialization-member-template-id-cv-mismatch`,
`general/400-partial-specialization-nontype-pattern-order`,
`general/400-partial-specialization-redecl-member-template-empty-pack`,
`general/400-repeated-argument-partial-specialization-ordering`,
`general/400-repeated-pack-partial-specialization-ordering`,
`general/400-variadic-template-pack-under-lvalue-reference-partial`,
`general/400-void-head-pack-partial-specialization-ordering`,
`spec/100-class-partial-specialization-array-size`,
`spec/100-class-partial-specialization-unbounded-array-mismatch`,
`spec/100-function-type-pack-partial-specialization-replay`,
`spec/100-function-type-top-level-cv-partial-specialization`,
`spec/100-inline-namespace-qualified-template-id-pack-expansion`,
`spec/100-nontype-static-outdef-value-member-preserves-type`,
`spec/100-partial-specialization-concrete-type-shadows-parameter-name`,
`spec/100-partial-specialization-cv-pointer-selection`,
`spec/100-qualified-function-type-partial-specialization`,
`spec/100-variadic-base-pack-expansion`,
`spec/200-template-template-parameter-nested-partial-specialization`,
`spec/300-defaulted-type-arg-specialization-nontype-value`, and
`spec/300-variable-template-defaulted-trailing-specialization`.

### B — alias/variable-template entities and template-template arguments

These expose missing canonical template entities, template-template arity and
pack binding, alias expansion, dependent non-type expression scope, and
variable-template specialization selection:

`general/200-adl-template-template-argument-namespace`,
`general/200-deferred-incomplete-member-layout-alias-value-type`,
`general/200-dependent-remove-cv-transform-alias-substitution`,
`general/200-qualified-template-template-variable-partial`,
`general/300-member-template-as-template-template-argument`,
`general/300-template-template-trailing-pack-rebind-function-pointer`,
`general/300-variable-template-forwarding-partial-top-cv`,
`general/300-variable-template-run-specialization-selection`,
`general/400-alias-nontype-expression-declaration-scope`,
`general/400-alias-nontype-pack-partial-specialization-pattern`,
`general/400-alias-pack-expansion-through-alias`,
`general/400-alias-pack-nontype-expression-fast-path`,
`general/400-alias-rebind-partial-specialization-shadow`,
`general/400-alias-template-decltype-greater-type-argument`,
`general/400-alias-template-decltype-member-type-argument`,
`general/400-alias-template-decltype-shift-type-argument`,
`general/400-alias-template-nontype-shadowed-by-inner-value`,
`general/400-alias-template-pack-id-preserves-syntax`,
`general/400-alias-template-pointer-cv-cache-distinction`,
`general/400-alias-value-expression-type-argument`,
`general/400-concrete-template-head-beats-template-template-pack`,
`general/400-dependent-alias-helper-partial-specialization`,
`general/400-dependent-alias-member-template-id-defer`,
`general/400-dependent-pack-typename-nontype-expression`,
`general/400-function-parameter-pack-alias-expansion`,
`general/400-member-alias-template-owner-rebind-cache`,
`general/400-member-alias-template-template-dependent-replay`,
`general/400-member-alias-template-template-empty-template-id-argument`,
`general/400-member-template-nontype-shadowed-global-replay`,
`general/400-nontype-pack-comma-expression-syntax`,
`general/400-nttp-pack-void-comma-expression`,
`general/400-qualified-base-type-alias-from-nontype-pack`,
`general/400-qualified-template-id-current-scope-alias-shadow`,
`general/400-template-template-arity-incomplete-partial`,
`general/400-template-template-fixed-prefix-pack-order`,
`spec/200-template-template-parameter-arity-mismatch`.

### C — member-template, friend-template, and owner/lookup replay

These require member declarations and namespace-scope friends to retain their
active class specialization, lexical scope, overload identity, and generated
owner through out-of-class definitions and calls:

`general/300-basic-template-operator-overloads`,
`general/300-class-template-hidden-friend-body-lexical-scope`,
`general/300-class-template-member-overload-braced-call-operator`,
`general/300-dependent-base-qualified-rvalue-assignment`,
`general/300-dependent-hidden-friend-static-member-definition`,
`general/300-explicit-member-call-function-template-id`,
`general/300-explicit-member-function-template-call`,
`general/300-explicit-member-template-id-shares-ordinary-overload`,
`general/300-explicit-type-arg-decltype-member-access`,
`general/300-friend-existing-template-private-ctor-access`,
`general/300-friend-function-template-access`,
`general/300-function-pack-template-id-deduction-decltype`,
`general/300-inherited-member-template-subscript-action`,
`general/300-local-qualified-argument-replay`,
`general/300-member-class-explicit-specialization-owner-lookup`,
`general/300-member-template-assignment-operator-value`,
`general/300-member-template-class-pack-forward-before-token`,
`general/300-member-template-local-using-does-not-suppress-adl`,
`general/300-namespace-function-template-hides-outer-callable-object`,
`general/300-nested-class-friend-template-namespace-scope`,
`general/300-nested-class-template-current-owner-lookup`,
`general/300-nested-class-template-reference-reset`,
`general/300-nested-member-partial-specialization-apply-scope`,
`general/300-nested-member-partial-specialization-survives-reference-reset`,
`general/300-nondependent-member-template-id-call`,
`general/300-out-of-class-ctor-using-imported-member-template`,
`general/300-out-of-class-member-function-template-definition`,
`general/300-out-of-class-member-template-namespace-typedef`,
`general/300-parenthesized-qualified-template-functional-call`,
`general/300-qualified-friend-member-template-access`,
`general/300-qualified-member-class-explicit-specialization`,
`general/300-qualified-preselected-member-template-call`,
`general/300-reference-member-class-template-visible`,
`general/300-reference-shell-nested-class-template-reuse`,
`general/300-sibling-namespace-dependent-member-template-id-owner`,
`general/300-single-pack-cast-target`,
`general/300-template-friend-class-constructor-access`,
`general/300-templated-constructor-special-member-collection`,
`general/300-unary-member-operator-template-default`,
`general/300-using-base-same-signature-derived-template-preferred`,
`spec/300-const-member-function-template-overload`,
`spec/300-friend-class-template-protected-base-access`,
`spec/300-hidden-friend-template-call-adl`,
`spec/300-hidden-friend-template-operator-adl`,
`spec/300-member-call-template-hides-inherited-instantiation`,
`spec/300-member-class-template-out-of-class`,
`spec/300-member-operator-template-active-owner`,
`spec/300-member-operator-template-in-class-template`,
`spec/300-member-template-cache-hit-concrete-scope`,
`spec/300-noexcept-member-template-call-operator`,
`spec/300-out-of-class-member-template-owner-param-rename`,
`spec/300-out-of-class-member-template-tag-dispatch-definition`,
`spec/300-out-of-class-overloaded-member-template-definition`,
`spec/300-qualified-friend-function-template-member-access`,
`spec/300-qualified-member-template-hides-base-function`,
`spec/300-rooted-qualified-static-data-member-template-definition`,
`spec/300-static-member-function-template-out-of-class`,
`spec/300-template-friend-inside-class-template`,
`spec/300-using-imported-member-template-active-owner`, and
`spec/300-using-inherited-alias-operator-template`.

### D — explicit specialization, explicit instantiation, and extern ownership

These require declaration-state ownership and ordering to distinguish primary,
explicit-specialization, explicit-instantiation, and extern-template forms:

`general/300-template-instantiation-use-location-explicit-specialization`,
`spec/300-explicit-instantiation-class`,
`spec/300-explicit-instantiation-deduced-member-function-template`,
`spec/300-explicit-instantiation-free-function-emits-definition`,
`spec/300-explicit-instantiation-function`,
`spec/300-explicit-instantiation-static-member-function`,
`spec/300-explicit-specialization-after-instantiation`,
`spec/300-explicit-specialization-member-function`,
`spec/300-explicit-specialization-out-of-class-member-overrides-primary`,
`spec/300-explicit-specialization-static-data-member`,
`spec/300-explicit-specialized-ctor-template-header-bad`,
`spec/300-extern-template-constructor-declaration`,
`spec/300-extern-template-function-call-suppresses-materialization`, and
`spec/300-extern-template-operator-function-declaration`.

### E — dependent expression, constant/value replay, and ordinary lowering

These are the remaining cross-cutting cases where the template graph reaches
constant evaluation, object layout, overload deduction, or LowIR ownership:

`general/200-lazy-header-constexpr-static-assert`,
`general/300-anonymous-union-storage-constructor-noop`,
`general/300-array-functional-cast-pack-call`,
`general/300-constexpr-static-fn-template-address-pack`,
`general/300-crtp-static-cast-reference-before-constructor-template`,
`general/300-function-signature-partial-specialization-functor-assignment`,
`general/300-function-template-local-static-per-specialization`,
`general/300-hidden-friend-template-call-adl`,
`general/300-static-constexpr-function-template-pointer-array`,
`general/400-local-value-shadows-template-relational`,
`general/400-out-of-namespace-class-template-member-result`,
`general/400-reference-member-depth-pack-sum`,
`general/400-reference-member-lookup-in-progress-base-typedef`, and
`spec/300-dependent-super-member-template-chain`.

The two timeout entries are retained in their owning groups above rather than
hidden: one is an owner/replay cycle and one is a specialization-ordering
cycle.  The invalid-LowIR entry is also retained in Group E.

## Checkpoint Scope

This checkpoint implements Group A's first substantial, coherent increment:

- canonicalize and retain typed class-partial-specialization patterns and
  their primary-template parameter contract;
- match nested template-ids, cv/ref/pointer forms, fixed and variadic packs,
  and non-type values without treating a concrete type as a deduced parameter;
- rank all matching partial-specialization candidates by specificity instead
  of selecting the first registered candidate;
- use the selected specialization as the active owner when materializing its
  class body and out-of-class members; and
- preserve current-specialization identity when rewriting qualified members,
  aliases, and nested declarations.

Validation scope is the Group A 100/200/spec probes plus the two repeated
argument/pack ordering fixtures listed above, followed by the full PA21 report
and the through-PA20 report.  The implementation stays in
the existing typed template/entity graph and ordinary LowIR path; it does not
special-case fixture names or invoke another compiler.

## Checkpoint Result

Completed the first Group A increment.  The matcher now retains typed array
and bound structure, distinguishes pointer pointee cv from top-level cv, and
supports repeated nested packs, template-template base bindings, and
defaulted trailing primary arguments.  Candidate partials are ranked by
structural specificity; the selected class declaration and its out-of-class
member owner are then used for materialization, with current-specialization
identity preserved during qualified rewrites, aliases, and nested declarations.

Validation is green for the six targeted Group A probes (6/6), the exact
through-PA20 report (1635/1635), and the PA21 file audit.  The required full
PA21 report is now 62/215, above the checkpoint baseline of 57/215.  Its 153
failures are 135 expected-success exit mismatches, three expected-failure
exit mismatches, 14 relaxed-LowIR mismatches, and one invalid-LowIR result.
There are no timeout failures.  The audit refactor also preserves the two
additional semantic passes already present after the checkpoint:
general/300-dependent-friend-alias-private-constructor-access and
general/300-dependent-friend-self-private-constructor-access.

## Refreshed Remaining Work Map

The complete current-PA21 report has 153 failing paths, classified exhaustively
by the next semantic owner:

- Group A — 34: remaining partial-specialization matching and ordering,
  function/reference/cv patterns, pack/value tails, and nested current-owner
  replay.
- Group B — 34: alias and variable-template entities, template-template
  binding/arity, dependent non-type expressions, and argument normalization.
- Group C — 60: member/friend templates, inherited lookup, overload/ADL
  selection, nested owners, and out-of-class definition replay.  This includes
  the two friend/private-typedef paths that were absent from the historical
  inventory.
- Group D — 12: explicit specialization, explicit instantiation, and
  extern-template ownership/order.
- Group E — 13: dependent constant/expression replay, layout, and ordinary
  lowering/LowIR ownership.

The failure counts include every path in the required report; no timeout or
architecture/audit blocker is being carried forward as assignment work.

## Next Checkpoint Group

The next substantial checkpoint is Group B: alias/variable-template entities
and template-template-parameter binding, with the shared typed argument
normalization and dependent non-type expression paths.

## Current turn checkpoint result (2026-07-23)

The selected inherited/using owner-replay slice is complete.  Member templates
reached through dependent bases and using-declarations now retain the concrete
declaring specialization while operator subscripting, assignment, unary
operators, member calls, ADL-sensitive calls, and member-template constructors
use the ordinary typed call-materialization path.  The implementation was
split so AST rewrite helpers live with the rewrite module; no fixture-specific
logic or external compiler path was added.

Validation is green for all five focused PA21 probes and the PA19
`general/200-member-init-covarying-type-index-pack` regression.  The full PA21
report is **156/215**, improving the turn-start **153/215** baseline.  The
through-PA20 report remains **1635/1635**, and the PA21 file audit passes with
warnings only.

### Remaining Work Map

The current report has 59 failures.  Groups A, B, D, and E from the exact
turn-start map above remain unchanged at 7, 14, 8, and 8 paths respectively:
partial-specialization identity; alias/template-template and dependent
non-type replay; explicit ownership/order; and dependent lowering/layout.
Group C is reduced to these 22 remaining member/friend owner-replay paths:

`general/300-basic-template-operator-overloads`,
`general/300-crtp-static-cast-reference-before-constructor-template`,
`general/300-dependent-base-qualified-rvalue-assignment`,
`general/300-dependent-hidden-friend-static-member-definition`,
`general/300-explicit-member-template-id-shares-ordinary-overload`,
`general/300-explicit-type-arg-decltype-member-access`,
`general/300-friend-existing-template-private-ctor-access`,
`general/300-function-pack-template-id-deduction-decltype`,
`general/300-local-qualified-argument-replay`,
`general/300-member-template-assignment-not-special-member`,
`general/300-member-template-assignment-operator-value`,
`general/300-nested-class-template-current-owner-lookup`,
`general/300-nested-class-template-reference-reset`,
`general/300-out-of-class-ctor-using-imported-member-template`,
`general/300-parenthesized-qualified-template-functional-call`,
`general/300-qualified-friend-member-template-access`,
`general/300-templated-constructor-special-member-collection`,
`general/300-unary-member-operator-template-default`,
`spec/300-member-class-template-out-of-class`,
`spec/300-member-operator-template-active-owner`,
`spec/300-member-operator-template-in-class-template`, and
`spec/300-rooted-qualified-static-data-member-template-definition`.

## Next Checkpoint Group

Continue Group C with generated-member and constructor/assignment owner replay:
resolve the remaining special-member-vs-member-template distinction, then
carry the concrete owner through explicit member-template IDs, qualified
friend/member definitions, and nested/out-of-class definitions.  Keep the
existing five-test owner slice and the through-PA20 report as regression gates.

## Continuation turn checkpoint scope (before implementation, 2026-07-23)

The current required report is **156/215** with the complete **59-path**
failure set recorded immediately above.  This checkpoint selects the shared
generated-member materialization slice of Group C:

`general/300-member-template-assignment-not-special-member`,
`general/300-member-template-assignment-operator-value`,
`general/300-templated-constructor-special-member-collection`,
`general/300-basic-template-operator-overloads`,
`general/300-unary-member-operator-template-default`, and
`general/300-dependent-base-qualified-rvalue-assignment`.

The scope is typed replay of member-template operators and constructors:
deduce operator template arguments without colliding with implicit special
members, materialize a templated constructor as a class-value producer,
retain overloaded unary result types, and carry dependent-base ownership
through qualified rvalue assignment.  It covers the PA18 owner/call graph and
the PA14 assignment, dereference, return-transfer, and LowIR consumers through
their ordinary semantic paths.  Success requires the six focused tests to
pass, the full PA21 count to exceed 156/215, through-PA20 to remain 1635/1635,
and the file audit to pass.

## Continuation checkpoint result (2026-07-23)

The selected generated-member slice is implemented.  Function-local value-name
tracking is now explicitly scoped to function bodies and excludes typedef and
anonymous-aggregate declarations, while namespace aliases remain usable in
qualified type names.  Template argument defaults are replayed into typed
parameter bindings; variable-type lookup is context-aware; reference-pattern
matching accepts generated rvalue-reference spellings; and generated member
operators, templated constructors, and dependent-base calls retain their
concrete declaring owner.  The variable collection body was moved out of the
large collection header so the stage file audit remains within its size rule.

All six focused tests pass their compiler/run checks.  The required full report
is **158/215** (up from the task-state baseline of **153/215**), with 57
remaining failures: 44 semantic/parse/lookup exit-status mismatches and 13
relaxed LowIR owner/body/emission mismatches.  The selected assignment
member-template case is in the latter set: it executes successfully but its
generated LowIR still differs from the checked reference.  The through-PA20
report is green at **1635/1635**, and the PA21 file audit passes with ten
pre-existing warnings.

### Remaining Work Map

The complete final report failure set is grouped by the shared observed
behavior below.  The first group still needs semantic/parser/template-state
work across partial specialization, aliases, member/friend lookup, explicit
ownership, and dependent lowering:

`general/100-reference-shell-out-of-class-current-specialization-iterator`,
`general/100-rvalue-reference-binds-converted-temporary`,
`general/200-reference-alias-top-cv-return-binding`,
`general/300-anonymous-union-storage-constructor-noop`,
`general/300-array-functional-cast-pack-call`,
`general/300-constexpr-static-fn-template-address-pack`,
`general/300-crtp-static-cast-reference-before-constructor-template`,
`general/300-extern-template-builtin-operator-function-declaration-bad`,
`general/300-function-pack-template-id-deduction-decltype`,
`general/300-function-signature-partial-specialization-functor-assignment`,
`general/300-local-qualified-argument-replay`,
`general/300-member-class-explicit-specialization-owner-lookup`,
`general/300-nested-class-template-current-owner-lookup`,
`general/300-nested-class-template-reference-reset`,
`general/300-nested-member-partial-specialization-apply-scope`,
`general/300-out-of-class-ctor-using-imported-member-template`,
`general/300-out-of-class-member-template-namespace-typedef`,
`general/300-parenthesized-qualified-template-functional-call`,
`general/300-single-pack-cast-target`,
`general/300-static-constexpr-function-template-pointer-array`,
`general/300-template-instantiation-use-location-explicit-specialization`,
`general/400-alias-nontype-expression-declaration-scope`,
`general/400-alias-nontype-pack-partial-specialization-pattern`,
`general/400-alias-rebind-partial-specialization-shadow`,
`general/400-alias-template-nontype-shadowed-by-inner-value`,
`general/400-alias-template-pointer-cv-cache-distinction`,
`general/400-dependent-pack-typename-nontype-expression`,
`general/400-forward-primary-partial-switch-value`,
`general/400-function-parameter-pack-alias-expansion`,
`general/400-inline-namespace-template-template-argument`,
`general/400-member-alias-template-template-dependent-replay`,
`general/400-member-template-nontype-shadowed-global-replay`,
`general/400-partial-specialization-conversion-operator-pointer-binding`,
`general/400-partial-specialization-redecl-member-template-empty-pack`,
`general/400-reference-member-lookup-in-progress-base-typedef`,
`spec/100-inline-namespace-qualified-template-id-pack-expansion`,
`spec/300-defaulted-type-arg-specialization-nontype-value`,
`spec/300-explicit-specialization-after-instantiation`,
`spec/300-explicit-specialization-refreshes-stale-primary-instantiation`,
`spec/300-explicit-specialized-ctor-template-header-bad`,
`spec/300-hidden-friend-template-operator-adl`,
`spec/300-member-class-template-out-of-class`,
`spec/300-member-operator-template-active-owner`, and
`spec/300-member-operator-template-in-class-template`.

The remaining emission/owner-body comparison group is:

`general/300-dependent-hidden-friend-static-member-definition`,
`general/300-explicit-member-template-id-shares-ordinary-overload`,
`general/300-explicit-type-arg-decltype-member-access`,
`general/300-friend-existing-template-private-ctor-access`,
`general/300-function-template-local-static-per-specialization`,
`general/300-member-template-assignment-not-special-member`,
`general/300-namespace-function-template-hides-outer-callable-object`,
`general/300-qualified-friend-member-template-access`,
`general/400-alias-pack-nontype-expression-fast-path`,
`general/400-reference-member-depth-pack-sum`,
`spec/300-explicit-instantiation-static-member-function`,
`spec/300-explicit-specialization-static-data-member`, and
`spec/300-rooted-qualified-static-data-member-template-definition`.

## Next Checkpoint Group

Continue with the ten remaining alias/non-type replay paths as one coherent
Group B checkpoint: declaration-scope and shadowed-value handling,
alias-template rebinding, pointer/cv cache identity, dependent pack
expressions, member-alias template replay, and the nontype expression fast
path.  Keep the six generated-member run probes, the through-PA20 report, and
the file audit as regression gates.

## Group B continuation scope (before implementation, 2026-07-23)

The current authoritative PA21 report remains **158/215** with the complete
57-path map above.  This checkpoint selects the ten remaining alias/non-type
paths that share argument normalization and template-entity replay:

`general/400-alias-nontype-expression-declaration-scope`,
`general/400-alias-nontype-pack-partial-specialization-pattern`,
`general/400-alias-pack-nontype-expression-fast-path`,
`general/400-alias-rebind-partial-specialization-shadow`,
`general/400-alias-template-nontype-shadowed-by-inner-value`,
`general/400-alias-template-pointer-cv-cache-distinction`,
`general/400-dependent-pack-typename-nontype-expression`,
`general/400-function-parameter-pack-alias-expansion`,
`general/400-member-alias-template-template-dependent-replay`, and
`general/400-member-template-nontype-shadowed-global-replay`.

The behavior scope is to preserve typed non-type expressions and their
declaration scope while expanding aliases; retain template-template
parameter arity, packs, and member-alias ownership through dependent replay;
and keep cv/ref distinctions in normalized alias/cache keys.  Validation will
run all ten focused fixtures, the full PA21 report, through-PA20, and the
stage file audit.  The checkpoint must improve the 158/215 baseline or finish
the PA, without regressing the six generated-member probes.

## Group B continuation result (2026-07-23)

The selected alias/non-type replay scope is complete.  All ten checkpoint
fixtures pass their compiler/run checks, and the six generated-member probes
remain green (16/16 focused compiler/run checks).  The full PA21 report is
**165/215**, improving the pre-checkpoint **158/215** result by seven tests and
the turn-start **153/215** baseline by twelve.  Through-PA20 remains green at
**1635/1635**, and the stage file audit passes with the ten pre-existing
warnings.

The implementation now preserves template-template entities through inherited
and nested member replay, resolves repeated in-class member-template owners
for dependent `decltype` calls, evaluates compact `sizeof...(Pack)` values
from active typed packs before dependent-scope rejection, and keeps cv words
inside identifiers such as `is_volatile` intact.  The relational-angle
classifier and generated owner context fixes keep non-type expression parsing
and cache/alias identity stable across these paths.

### Remaining Work Map

The current report has 50 failures, grouped by the shared downstream
behavior:

- **Dependent owner, lookup, partial-specialization, and diagnostic behavior
  (35 exit-status failures):**
  `general/100-reference-shell-out-of-class-current-specialization-iterator`,
  `general/100-rvalue-reference-binds-converted-temporary`,
  `general/100-top-cv-pointer-does-not-match-unqualified-pointer-partial`,
  `general/200-reference-alias-top-cv-return-binding`,
  `general/300-anonymous-union-storage-constructor-noop`,
  `general/300-array-functional-cast-pack-call`,
  `general/300-constexpr-static-fn-template-address-pack`,
  `general/300-crtp-static-cast-reference-before-constructor-template`,
  `general/300-extern-template-builtin-operator-function-declaration-bad`,
  `general/300-function-pack-template-id-deduction-decltype`,
  `general/300-function-signature-partial-specialization-functor-assignment`,
  `general/300-local-qualified-argument-replay`,
  `general/300-member-class-explicit-specialization-owner-lookup`,
  `general/300-nested-class-template-current-owner-lookup`,
  `general/300-nested-class-template-reference-reset`,
  `general/300-nested-member-partial-specialization-apply-scope`,
  `general/300-out-of-class-ctor-using-imported-member-template`,
  `general/300-single-pack-cast-target`,
  `general/300-static-constexpr-function-template-pointer-array`,
  `general/300-template-instantiation-use-location-explicit-specialization`,
  `general/300-variable-template-forwarding-partial-top-cv`,
  `general/400-forward-primary-partial-switch-value`,
  `general/400-inline-namespace-template-template-argument`,
  `general/400-partial-specialization-conversion-operator-pointer-binding`,
  `general/400-partial-specialization-redecl-member-template-empty-pack`,
  `general/400-reference-member-lookup-in-progress-base-typedef`,
  `spec/100-inline-namespace-qualified-template-id-pack-expansion`,
  `spec/300-defaulted-type-arg-specialization-nontype-value`,
  `spec/300-explicit-specialization-after-instantiation`,
  `spec/300-explicit-specialization-refreshes-stale-primary-instantiation`,
  `spec/300-explicit-specialized-ctor-template-header-bad`,
  `spec/300-hidden-friend-template-operator-adl`,
  `spec/300-member-class-template-out-of-class`,
  `spec/300-member-operator-template-active-owner`, and
  `spec/300-member-operator-template-in-class-template`.

- **Generated owner/body and LowIR emission mismatches (15):**
  `general/300-dependent-hidden-friend-static-member-definition`,
  `general/300-explicit-member-template-id-shares-ordinary-overload`,
  `general/300-explicit-type-arg-decltype-member-access`,
  `general/300-friend-existing-template-private-ctor-access`,
  `general/300-function-template-local-static-per-specialization`,
  `general/300-member-template-assignment-not-special-member`,
  `general/300-namespace-function-template-hides-outer-callable-object`,
  `general/300-parenthesized-qualified-template-functional-call`,
  `general/300-qualified-friend-member-template-access`,
  `general/400-alias-pack-nontype-expression-fast-path`,
  `general/400-member-template-nontype-shadowed-global-replay`,
  `general/400-reference-member-depth-pack-sum`,
  `spec/300-explicit-instantiation-static-member-function`,
  `spec/300-explicit-specialization-static-data-member`, and
  `spec/300-rooted-qualified-static-data-member-template-definition`.

## Next Checkpoint Group

Take the 15-case generated owner/body group as the next coherent checkpoint:
compare the typed generated declaration owner, member-template/special-member
classification, static/local storage emission, and final LowIR body ordering.
Retain the 16/16 focused replay probes, through-PA20, and the file audit as
regression gates while addressing the remaining 35 semantic/diagnostic paths
as subsequent grouped work.

## Group C checkpoint scope (2026-07-23, before implementation)

The current PA21 report revalidated at 165/215 with the complete 50-case map
above: 35 exit-status failures and 15 relaxed LowIR mismatches. This checkpoint
covers the shared generated-member/static-storage emission behavior in six of
the LowIR cases: dependent hidden-friend static-member definition, function
template local statics, explicit-instantiation static member function,
explicit-specialization static data member, rooted qualified static-data
member-template definition, and the generated member-template assignment
classification. The behavior under test is typed owner selection followed by
correct generated declaration/definition registration, static versus automatic
storage, guarded local-static initialization, special-member classification,
and final LowIR emission/order. Validation is the six focused fixtures plus
the 16/16 replay probes, full PA21, through-PA20, and the file audit.

## Checkpoint 62 — owner-aware replay, qualified calls, and bound packs

### Checkpoint Scope

This increment covers the complete owner/replay group: deferred using-declaration
resolution; direct versus inherited member-template provenance; nested partial
specialization replay; concrete-owner propagation through qualified member calls
and member-template addresses; hidden-friend and static-member generated names;
typed parameter scopes in generated bodies; and enclosing class packs used by
function parameter packs and aliases. It also keeps template-pack hints safe
when raw arguments have already been consumed.

### Result

The PA21 report is **179/215**, preserving the audit turn-start baseline of
179/215.  The nine focused owner/pack/operator probes are **1/9**; their eight
remaining failures are represented in the complete map below.  Through-PA20
is green at **1635/1635**, and the PA21 file audit passes with nine warnings
and no fatal finding.  The qualified-member and semantic-using implementations
are registered as cohesive source units rather than being packed into files at
the source-size ceiling.

### Remaining Work Map

The complete current-PA failure set is **36 cases**, grouped by the next
semantic owner:

- **Generated owner/member replay and LowIR body ownership (13):**
  `general/300-crtp-static-cast-reference-before-constructor-template`,
  `general/300-dependent-hidden-friend-static-member-definition`,
  `general/300-explicit-member-template-id-shares-ordinary-overload`,
  `general/300-explicit-type-arg-decltype-member-access`,
  `general/300-friend-existing-template-private-ctor-access`,
  `general/300-local-qualified-argument-replay`,
  `general/300-member-class-explicit-specialization-owner-lookup`,
  `general/300-namespace-function-template-hides-outer-callable-object`,
  `general/300-nested-class-template-current-owner-lookup`,
  `general/300-parenthesized-qualified-template-functional-call`,
  `general/300-qualified-friend-member-template-access`,
  `spec/300-member-operator-template-active-owner`, and
  `spec/300-member-operator-template-in-class-template`.
- **Specialization identity, alias, conversion, and pack deduction (15):**
  `general/100-top-cv-pointer-does-not-match-unqualified-pointer-partial`,
  `general/200-reference-alias-top-cv-return-binding`,
  `general/300-function-pack-template-id-deduction-decltype`,
  `general/300-function-signature-partial-specialization-functor-assignment`,
  `general/300-variable-template-forwarding-partial-top-cv`,
  `general/400-alias-pack-nontype-expression-fast-path`,
  `general/400-forward-primary-partial-switch-value`,
  `general/400-inline-namespace-template-template-argument`,
  `general/400-member-template-nontype-shadowed-global-replay`,
  `general/400-partial-specialization-conversion-operator-pointer-binding`,
  `general/400-partial-specialization-redecl-member-template-empty-pack`,
  `general/400-reference-member-depth-pack-sum`,
  `general/400-reference-member-lookup-in-progress-base-typedef`,
  `spec/100-inline-namespace-qualified-template-id-pack-expansion`, and
  `spec/300-defaulted-type-arg-specialization-nontype-value`.
  The first case also exposes the known comparator internal failure.
- **Dependent initialization, casts, and function-address lowering (5):**
  `general/100-rvalue-reference-binds-converted-temporary`,
  `general/300-array-functional-cast-pack-call`,
  `general/300-constexpr-static-fn-template-address-pack`,
  `general/300-single-pack-cast-target`, and
  `general/300-static-constexpr-function-template-pointer-array`.
- **Explicit-specialization/instantiation diagnostics (3):**
  `general/300-extern-template-builtin-operator-function-declaration-bad`,
  `general/300-template-instantiation-use-location-explicit-specialization`,
  and `spec/300-explicit-specialized-ctor-template-header-bad`.

### Next Checkpoint

Address the 13-case generated owner/member replay and LowIR group first,
beginning with the nested-owner sanity failure and typed generated declaration
body ownership.  Retain the nine focused probes, the **1635/1635** through-PA20
gate, the **179/215** full PA21 baseline, and the file audit as regression gates;
the 15-case identity/pack group, five initialization/address cases, and three
diagnostics can then be bundled into the following smaller checkpoints.
