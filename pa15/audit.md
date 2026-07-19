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
