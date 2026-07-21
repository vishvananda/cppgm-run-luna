# PA20 checkpoint plan

## Turn-start baseline

The turn-start report was **7/71 PA20 tests passing**, with all earlier
assignments through PA19 passing.  The complete current-PA failure set was
read from the required report and grouped below by shared compiler behavior
before implementation.

## Remaining Work Map recorded before implementation

The 64 failing fixtures are inventoried exactly once:

### A — scalar constexpr calls, local state, and control flow (13)

`general/100-constexpr-default-argument-call`,
`general/100-constexpr-do-while`,
`general/100-constexpr-for-condition-declaration`,
`general/100-constexpr-for-loop`,
`general/100-constexpr-global-from-function-call`,
`general/100-constexpr-if-condition-declaration`,
`general/100-constexpr-local-and-if`,
`general/100-constexpr-recursive-call`,
`general/100-constexpr-recursive-depth`,
`general/100-constexpr-static-assert-function-call`,
`general/100-constexpr-while-break-continue`, and
`general/100-constexpr-while-loop`, and
`general/400-constexpr-validation`.

These share the incomplete `Analyzer::Evaluate` path: it only handles
argument-free constexpr calls and searches for a return expression instead
of interpreting declarations, assignments, branches, loops, and flow exits.
The resulting unknown values either reject `static_assert`/initialization or
leave calls to constexpr-only functions in LowIR.

### B — overload/name and template materialization around constexpr calls (6)

`general/100-constexpr-call-over-same-name-type`,
`general/100-constexpr-function-parameter-pack-call`,
`general/100-inline-namespace-aggregate-member-value`,
`general/300-constexpr-function-template-static-assert`,
`general/400-constexpr-local-using-directive-template-call`, and
`spec/300-constexpr-function-template-operator-call`.

These require call-choice identity, parameter-pack/template expansion, or
inline-namespace/qualified lookup to survive materialization before the
constant evaluator is invoked.

### C — floating and `noexcept` constant expressions (8)

`general/200-constexpr-floating-global-from-function-call`,
`general/200-constexpr-floating-local-if`,
`general/200-constexpr-floating-static-assert-function-call`,
`general/200-noexcept-defaulted-constructor-static-assert`,
`general/200-noexcept-static-assert-function-call`,
`general/200-noexcept-user-provided-constructor-static-assert`,
`general/400-constexpr-floating-suffix-literals`, and
`general/400-static-constexpr-floating-global-storage`.

The evaluator and binding model currently own integral values only; this
group also exposes typed floating values, exception-specification facts, and
one branch LowIR constant-shape mismatch.

### D — class construction, object/array values, and member evaluation (15)

`general/300-constexpr-aggregate-array`,
`general/300-constexpr-base-copy-constructor-init`,
`general/300-constexpr-constructor`,
`general/300-constexpr-decltype-qualified-static-member-lookup`,
`general/300-constexpr-defaulted-constructor`,
`general/300-static-constexpr-array-member-shadowed-auto-decay`,
`general/400-constexpr-class-bool-conversion`,
`general/400-constexpr-function-aggregate-member-nttp`,
`general/400-constexpr-implicit-default-init`,
`general/400-constexpr-method`,
`general/400-constexpr-noexcept-decltype-static-assert`,
`general/400-constexpr-return-operator-conversion`,
`general/400-constexpr-temporary-functor-member-call`,
`spec/400-constexpr-function-and-constructor`, and
`spec/400-constexpr-reference-parameter-conversion-constructor`.

These need a typed object representation, constructor/member-initializer
execution, member access, reference parameters, conversions, and validation
of literal `constexpr` declarations rather than an integral-only result.

### E — local static lifetime, pointer/string constants, and storage lowering (10)

`general/300-function-local-static-array-guard`,
`general/300-function-local-static-scalar-first-use`,
`general/300-function-local-static-scalar`,
`general/300-function-noexcept-array-reference-return`,
`general/300-local-static-class-reference-call-init`,
`general/300-static-constexpr-array-member-sizeof`,
`general/400-constexpr-pointer-truthiness`,
`general/400-constexpr-static-array-pointer-loop-increment`,
`general/400-global-string-literal-pointer-init`,
`general/400-local-static-local-class-array-init`,
`general/400-wide-string-literal-pointer-init`.

These failures are in the object model/lowering boundary: local-static
guarded storage, address identity, string literal globals, pointer/reference
truthiness, array decay/indexing, and structured global initializers.

### F — static/template constant lookup and dependent `sizeof`/NTTPs (12)

`general/400-dependent-class-prvalue-default-bool-nttp`,
`general/400-dependent-sizeof-owner-special-member`,
`general/400-noexcept-declval-callable-object`,
`general/400-own-static-constexpr-member-call-nttp`,
`general/400-pack-expanded-static-constexpr-array-member`,
`general/400-template-constexpr-conversion-static-member`,
`general/400-template-qualified-static-member-arithmetic`,
`spec/100-constexpr-signed-widening-comparison`,
`spec/300-constexpr-local-typedef-visible`,
`spec/300-static-constexpr-aggregate-class-array-member`,
`spec/300-static-constexpr-array-template-member-odr-use`, and
`spec/300-static-constexpr-constructor-class-array-member`.

These require PA18/PA19 materialization to preserve typed constant member
values, dependent owner lookup, widened comparisons, pack-expanded storage,
and generated special-member declarations.  The array-template-member case
also has the storage/indexing symptom from Group E; it is tracked here by
its primary template-constant ownership cause.

The inventory above is a behavior map, not a test-specific allowlist.  The
LowIR mismatches are assigned to the semantic group whose missing typed fact
causes them: branch shape is in Group C, local/static storage is in Group E,
and dependent/template storage is in Group F.  Each fixture appears once.

## Current Remaining Work Map after the first checkpoint

The first refreshed report reached **48/71** before the floating follow-up
was applied (the floating group is now covered by the implementation below).
The remaining failures are grouped by shared behavior:

- **Object-valued constexpr lowering:** aggregate/string global rendering,
  base-copy construction, defaulted construction, class-to-bool conversion,
  constexpr conversion/operator results, temporary functor calls, and the
  discarded temporary object in the `noexcept`/dependent-`sizeof` paths.
- **Static member and array storage:** qualified `decltype` static lookup,
  static-array auto decay and `sizeof`, pack-expanded zero-bound storage,
  template conversion static members, and the three spec fixtures for static
  aggregate/constructor array members.
- **Local/reference and pointer storage:** the local-static class-reference
  read shape, object-reference binding, and wide-string pointer global
  initialization.
- **Presentation-only cleanup coupled to real lowering:** unused string
  globals in aggregate initialization and extra generated special-member
  bodies; these are tracked with the owning object-lowering behavior rather
  than patched per fixture.

The floating failures (`200-constexpr-floating-global-from-function-call`,
`400-constexpr-floating-suffix-literals`, and
`400-static-constexpr-floating-global-storage`) are no longer in the map:
global rendering now consumes typed semantic floating values and retains
direct-literal exponent/suffix information.

## Checkpoint Scope

This checkpoint implements a coherent PA20 semantic/lowering increment:

- collect function-local static declarations with stable token-identity
  symbols and lower their guarded dynamic initialization into function bodies;
- preserve typed destination-address ordering for scalar and reference local
  static initialization;
- collect string literals used by local-static initializers, including local
  class constructor arguments;
- lower array-reference return subscripting without a second decay/call;
- evaluate `noexcept` through the PA19 semantic constant engine and emit its
  integral result directly in branch conditions; and
- render floating scalar/array globals from typed semantic constants, including
  constexpr function calls and direct literal suffix/exponent normalization.

The implementation will remain in the existing PA11 semantic constant path,
with typed compiler-owned value state crossing into PA14 lowering.  It will
not encode answers for fixture names or invoke a reference/host compiler.

## Checkpoint Result

Implemented and built successfully.  The PA20 report improved from the
turn-start **7/71** baseline to **49/71** after the floating pass (with one
small local-reference rendering correction pending in the next report).
Earlier-PAs validation remains clean in the report harness; no tests or ref
fixtures were changed.

## Post-checkpoint Remaining Work Map

The current map is the four grouped behaviors above.  The next substantial
group is object-valued constexpr/global storage, bundled with static aggregate
array materialization because both require the same typed `ConstantValue`
object-to-LowIR projection.

## Next Checkpoint Group

Object-valued constexpr/global storage and static aggregate/array members;
validate the aggregate, base-copy, class-constructor, class-bool, and three
static-array spec fixtures together before moving to remaining template and
pointer presentation cases.

## Final Checkpoint Result

The completed increment covers the entire PA20 failure map: typed constexpr
object construction and conversion, class/member and base-subobject lowering,
local-static guards and references, string and wide-string storage, array
decay and aggregate projection, floating and `noexcept` constants, qualified
and template static-member storage, dependent `sizeof`, and demand-driven
special-member/function emission.  The final required PA20 report is **71/71
tests passing**; no tests or reference fixtures were changed.

## Remaining Work Map

None for PA20.  Earlier assignments remain covered by the through-PA19
report; the remaining work is final validation, file audit, and commit of the
cohesive compiler increment.

## Next Checkpoint Group

No further PA20 implementation group remains.  The next checkpoint is the
required repository validation and clean handoff.

## Final Validation Checkpoint

The last two through-PA19 regressions were repaired without changing the PA20
scope: qualified out-of-class `constexpr` static-member functions are now
materialized when their definition is collected, while only automatic
character arrays suppress string interning (aggregate arrays of pointer
members still receive their required string globals).  The required reports
now pass at **1564/1564 through PA19** and **71/71 for PA20**.

## Remaining Work Map

None.  The selected checkpoint covers the complete PA20 behavior map; only
repository audits, commit, and clean-status verification remain.

## Checkpoint Scope

Final validation of all PA20 tests plus all earlier assignments through PA19,
including the repaired static-member demand and local string-storage boundary.

## Architecture Review

The completed PA20 implementation remains a monotonic extension of the
existing compiler pipeline.  The parser and AST remain the syntax boundary:
the AST carries the source token range needed to give function-local statics
stable identities, while declarations, expressions, control flow, and
initializers remain structured nodes rather than being reinterpreted from
LowIR.

`ExpandPA18Templates` still runs before PA11 analysis and PA14 lowering.  It
materializes template declarations, parameter packs, generated members, and
dependent source facts as ordinary AST declarations.  Its pre-semantic source
boundary uses the existing PA19 typed integral parser for source expressions;
the array-function fallback now interprets the matching constexpr function
AST generically, including calls, scalar values, array pointers, branches,
loops, and recursive calls.  This boundary is limited to facts needed while
materializing dependent template source; ordinary constexpr evaluation is
owned by PA11 after materialization.

PA11 owns the compile-time value model.  `ConstantValue` distinguishes typed
integral and floating values from aggregate objects and pointers,
`ConstantObject` owns array/member values, and `ConstantPointer` preserves
array identity, offset, and nullness.  `Evaluate`/`EvaluateTyped` share the
same value facts with constexpr calls, constructors, member access,
conversions, static assertions, and ordinary semantic checks.  Call frames,
pack frames, receivers, binding-value caching, and explicit flow results
support defaults, packs, recursion, declarations, assignments, branches,
loops, and return/break/continue propagation.  Recursion is capped at 512
active calls and loop interpretation at 100,000 iterations.

PA14 consumes those semantic facts instead of reconstructing answers from
source spelling.  Its global-data projection lowers typed scalar, floating,
aggregate, array, pointer, and reference values into the existing LowIR data
items.  The same lowerer owns local-static storage and guards, string globals,
constructor demand, global initialization, and function emission.  PA17
polymorphic/vtable and ABI behavior remains on this shared path; PA20 does not
introduce a second backend or change the output contract.

Ownership follows the established boundaries: AST nodes and semantic types
are shared-owned, scopes own child scopes, and PA14's function/global records
use stable storage for non-owning lookup and demand references.  Constant
objects and pointers own their nested semantic values through shared objects.
Template specialization caches, active-recursion sets, binding-value caches,
and the evaluator limits bound repeated work.  Both new implementation files
are registered in `dev/frontend_source_sets.mk`.

## Final Architecture Review

The final audit reviewed the integrated PA20 checkpoint at `75eacf2`, the
PA18/PA19 handoff audits, all 71 PA20 fixtures, and the complete changed
source set.  It found and repaired three shortcut risks inherited by the
checkpoint.  The source evaluator no longer dispatches by the fixture-shaped
names `first_true` or `first_true_loop`; the dependent-class path no longer
special-cases `integral_constant`; and PA11 no longer treats extra arguments
to generated `__inst_` functions as an instruction to return their sum.
Those cases now use generic AST evaluation, normal generated-function
materialization, and the existing typed semantic conversion path.  Generated
names remain only identity/lookup metadata.

The audit also added reverse lookup for expanded pack identifiers in
`InferArgument`, so recursive materialized calls recover the source pack's
type instead of depending on a generated identifier spelling.  Nested source
constexpr calls are folded through a generic call replacer before the PA19
expression parser handles the surrounding expression.  Typed branch values
now select a statement through `ConstantKnown`, and aggregate global-data
fallback compares the current item range rather than the entire output list;
the existing zero/unsized array-bound convention is intentionally preserved.

The required file audit passes with only the repository's eight established
warning-only header-division findings and no fatal or unregistered-source
finding.  No test, reference fixture, output-format path, reference binary,
host compiler, or unchecked implementation path was added.  PA20 is
architecturally consolidated and ready for the next assignment.
