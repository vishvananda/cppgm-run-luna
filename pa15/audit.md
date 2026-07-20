# PA15 checkpoint audit

## Ralph loop 31 class-layout checkpoint (`9ee6fad` / `015b046`, 2026-07-19)

### Scope Reviewed

Reviewed the latest `Checkpoint Scope`, `Checkpoint Result`, and complete
`Remaining Work Map` in `pa15/plan.md`; the PA15 assignment README; the PA14
handoff; commits `9ee6fad` and `015b046`; every source file changed by those
commits; the nine focused layout tests; the full PA15 report; the required
through-PA14 report; and the PA15 source-file audit.

The reviewed boundary is the typed class-layout service and its direct PA14
consumers: declaration-order fields, pointer/reference/array and completed
class sizes, empty classes, self-pointers, a direct base at offset zero,
integral bit-field units and unnamed zero-width separators, explicit
alignment, `sizeof`/`alignof`, structured zero-initialized class globals, and
trivial local class storage. Member/lifetime lowering remains the explicitly
planned next PA15 checkpoint.

### Findings

The class-layout implementation has no test-name acceptance gates, embedded
answers, subprocess/host-compiler calls, interpreter/VM/trampoline substitute,
dummy output, skipped frontend phase, or repeated full-program walk per class.
Layout is a linear member walk; alignment arguments and bit-field comparison
use typed AST/type facts rather than reparsing emitted text.

The audit found and fixed two checkpoint-level issues:

- The required through-PA14 report exposed a real PA3 performance blocker:
  `ctrlexpr` flushed output with `std::endl` for every line of the 492,075-line
  `300-triple.t` workload. This was a timeout-causing hot-path flush pattern,
  not a test-timeout workaround.
- Class lookup bindings pointed into the relocatable `Type::class_members`
  vector. That non-owning pointer could dangle after member-vector growth or
  type copying, even though `ClassMemberInfo` was intended to own the facts.

No file-audit bypass, hidden implementation fragment, weakened check, or
source/test-specific acceptance path was found. The new analyzer source is in
the `cppgm++` source set. The three file-audit header-division warnings are
pre-existing repository warnings, not unchecked code moved to a new path.

### Changes Made

- Kept `alignas` arguments as typed AST nodes, evaluated standard type-id and
  constant-expression forms semantically, and left the unsupported vendor
  `__alignof` extension syntactic-only.
- Made incomplete, recursive, and invalid layouts fail at the semantic
  boundary; removed PA14's fallback size/alignment success path; and added
  typed alignment and bit-field validation.
- Kept `ClassMemberInfo` as the sole owner of layout/member-kind facts and
  replaced the binding's raw member pointer with a stable owner `TypePtr` plus
  member index.
- Replaced per-line `std::endl` in PA3 controlling-expression output with
  buffered newlines, preserving the exact output stream while removing the
  timeout-causing flushes.
- Refreshed `pa15/plan.md` with the current 174-failure set, checkpoint result,
  and next substantial member-collection group.

### Validation

- Focused PA15 layout set: **9 / 9 passed**.
- Required prior-through check:
  `n=15; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`
  — **pass**, **819 / 819**.
- Required PA15 report `make test-report ACTIVE_TEST_REPORT_PAS='pa15'` —
  **26 / 200**, above the turn-start baseline of **16 / 200**; the remaining
  set is 154 status mismatches plus 20 LowIR mismatches.
- Required file audit
  `perl scripts/cppgm_file_audit.pl --stage pa15 --paths dev/src` — **pass**
  with the three existing header-division warnings.
- Isolated PA3 `300-triple.t` after the buffering fix: **1 / 1 passed** in
  about 8.1 seconds under the normal 10-second test limit.
- `make build` and `git diff --check` pass. No test or reference fixture was
  changed.

No checkpoint-level shortcut, fallback, ownership, performance, regression,
or file-audit blocker remains. The planned PA15 failures are grouped in
`pa15/plan.md`, beginning with member collection, `this`, field access, and
member-function declarations.

## Ralph loop 33 final stage audit (`2556b6e`, 2026-07-20)

### Audit Plan

The final audit reviewed the PA15 assignment README and PA13 LowIR contract,
`TESTING_AND_REFERENCES.md`, the complete PA15 plan history, this checkpoint
audit, the PA15 test sources and representative earlier-stage object/lifetime
handoff tests, and the integrated commit sequence:
`9ee6fad`, `015b046`, `24781a4`, `cb36fd1`, `702ffb2`, `74131bc`, and
`2556b6e`. The review covered the source changed by those commits, with
special attention to the final PA15 checkpoint folded into `2556b6e`:
typed aggregate value initialization, hidden-friend dependency emission, and
user-defined string-literal operators.

The audit checked four boundaries:

1. syntax and typed semantic facts crossing from PA10/PA11 into PA14
   lowering;
2. class layout, member lookup, overload resolution, access, and inheritance
   facts consumed without reparsing LowIR text;
3. constructor/destructor, aggregate, local, and namespace-scope lifetime
   actions and their required LowIR ordering; and
4. source ownership, demand-driven helper emission, performance hot paths,
   shortcut indicators, file-audit status, and preservation of PA1-PA14.

### Findings

The integrated implementation follows the intended PA15 architecture. The
parser preserves initializer form, standard `alignas` arguments, qualified
member definitions, destructor syntax, and user-defined literal tokens as AST
facts. The PA11 model owns complete `Type` metadata, `ClassMemberInfo` layout
records, class scopes, access/friend facts, direct bases, and stable member
owner/index bindings. The analyzer computes declaration-order layout,
padding, empty-class storage, base-at-zero layout, bit-field units, and
alignment before LowIR generation.

The PA14 lowerer remains the single backend path. Its typed
`FunctionRecord`, `GlobalRecord`, `VariablePlan`, `ExprInfo`, and `CallChoice`
records carry ownership and type information through collection, overload and
ADL resolution, statement planning, expression lowering, constructor and
destructor actions, global storage, and LowIR emission. Non-static calls add
the hidden object parameter; globals separate static storage from startup and
reverse-order shutdown actions; class arrays and nested subobjects are walked
element-wise; and helper functions are emitted only after semantic demand
reaches them.

The final checkpoint is implemented semantically rather than by fixture
special cases: empty aggregate member initialization emits a typed zero action,
hidden friends are retained through dependency-aware ADL demand, and a
user-defined string literal is decoded/interned once and passed with its core
string address and length. The implementation contains no test-name gates,
reference-binary or host-compiler subprocesses, dummy output, alternate
interpreter/backend, or skipped frontend phase.

One additional ownership issue was found during the integrated review. PA15
can append synthesized bindings after expression facts have cached `Binding*`
values. `Scope::bindings` therefore could invalidate semantic pointers when it
was a `vector`. The container now uses `deque`, retaining indexed lookup while
making binding identity stable across constructor, destructor, aggregate, and
hidden-friend synthesis. The accompanying misleading indentation in the
analyzer's function-definition path was also made explicit with braces. No
test or reference fixture was changed.

The normal source audit has no fatal findings. It reports the three existing
warning-only header-division findings for `pa11_semantics_analyzer.h`,
`pa14_lowering.h`, and `recog_parser_internal.h`; these were already present
in the earlier stage audit and do not bypass the audit gate. The PA15 source
set includes both added lowering units, and no new file-audit warning or
unowned implementation path was introduced.

### Changes Made

- Changed semantic scope binding storage from `vector<Binding>` to
  `deque<Binding>` so cached lookup facts remain valid while PA15 synthesizes
  semantic entities during lowering.
- Added explicit braces in `Analyzer::ProcessFunctionDefinition` to preserve
  the existing control flow and remove a misleading-indentation compiler
  warning.
- Kept the final checkpoint's typed aggregate, ADL/friend, UDL, lifetime,
  layout, and LowIR changes integrated in the PA14 source set; no tests or
  `.ref` files were edited.

### Validation

- `make -C pa15 test` — **200 / 200** PA15 tests passed; the empty
  `course/pa15` suite also passed (**0 / 0**).
- `make test-report-through-pa15` — **1019 / 1019** tests passed across PA1
  through PA15.
- `perl scripts/cppgm_file_audit.pl --stage pa15 --paths dev/src` — **pass**;
  only the three documented warning-only header-division findings remain.
- `make build` — pass.
- `git diff --check` — pass.

The final worktree check and commit are part of the handoff validation below;
the stage is not complete until `git status --short` is empty after the audit
commit.
