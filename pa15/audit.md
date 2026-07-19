# PA15 checkpoint audit

## Class-layout checkpoint (`9ee6fad`, 2026-07-19)

### Scope Reviewed

Reviewed the latest `Checkpoint Scope`, `Checkpoint Result`, and failure map in
`pa15/plan.md`; the PA15 assignment contract; the parent PA14 audit/lowering
checkpoint; commit `9ee6fad` and its changed source set; the focused layout
tests; the complete PA15 report log; and the PA15 source-file audit.

The reviewed implementation boundary is the typed class-layout service and
its PA14 consumers: declaration-order member layout, pointer/reference/array
and completed class sizes, empty classes, self-pointers, a direct base at
offset zero, bit-field allocation units and zero-width separators, explicit
alignment, `sizeof`/`alignof`, structured zero-initialized class globals, and
trivial local class storage.

### Findings

The checkpoint had no test-name gates, embedded payloads, subprocess or
interpreter paths, VM/trampoline substitute, timeout workaround, or skipped
frontend phase. Its layout walk is linear in the class-member sequence, and
the lowerer does not repeat a full-suite or full-program scan for each class.

The audit did find checkpoint-level correctness and ownership problems:

- incomplete or in-progress classes could fall through to a one-byte size in
  both semantic `TypeSize` and PA14 lowering;
- `alignas` was carried as raw spelling and reparsed with string heuristics,
  with failed type resolution silently ignored;
- bit-field allocation compared serialized `TypeText` rather than typed type
  facts; and static/mutable/bit-field layout facts were duplicated between
  bindings and the class-layout record.

No file-audit bypass or hidden implementation fragment was found. The new
analyzer translation unit is in the `cppgm++` source set. The audit reports
only the repository's pre-existing header-division warnings.

### Changes Made

- Replaced the parser's global pending-attribute spelling channel with typed
  `alignas` AST arguments. Standard type-id and constant-expression forms now
  reach semantic evaluation directly; the earlier vendor-dependent
  `__alignof` form remains syntactic-only for the PA10 contract.
- Made incomplete, recursive, and otherwise invalid class layouts fail at the
  semantic boundary, and removed PA14's fallback size/alignment success path.
- Added typed alignment validation and typed bit-field type comparison and
  validation, including rejection of named zero-width fields.
- Made `ClassMemberInfo` the owner of layout/member-kind facts and linked class
  lookup bindings to those records, removing the unused duplicate binding
  fields and string-based static/mutable recovery.
- Refreshed `pa15/plan.md` with the complete current failure set and the next
  substantial member-collection checkpoint.

### Validation

- Focused checkpoint set: **9 / 9 passed**.
- Required prior-through check:
  `n=15; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`
  — **pass**, **819 / 819**.
- Required check `make test-report ACTIVE_TEST_REPORT_PAS='pa15'` — the
  current PA remains **26 / 200** (up from the turn-start **16 / 200**), so
  stage progress is preserved while the planned PA15 gaps remain.
- Required file audit
  `perl scripts/cppgm_file_audit.pl --stage pa15 --paths dev/src` — **pass**
  with three existing header-division warnings.
- Direct audit smoke checks confirmed a constant-expression `alignas` layout
  is accepted and a recursive by-value class is rejected.
- `make build` and `git diff --check` pass; no test or reference fixture was
  changed.

No checkpoint-level shortcut, fallback, ownership, performance, regression,
or file-audit blocker remains. The remaining PA15 failures are captured in
`pa15/plan.md` as the next implementation groups, beginning with member
collection and field/member-function access.
