# PA8 implementation plan

## Baseline

At the start of this checkpoint the PA8 tool was still the starter stub:
all 41 PA8 tests returned `EXIT_NOT_IMPLEMENTED`, while the through-PA7
report was green.

## Remaining Work Map

The complete current-PA failure set is grouped by shared compiler behavior:

1. **Front end and entity model** — `100-empty-decl`, `110-emptydef`,
   `110-vardef`, `120-linkedvar`, `130-staticvar`, `150-inline-namespace2`,
   `150-thread-local`, `200-function`, `200-fundamental`, `250-nullptr`,
   `300-array`, `300-inline-function`, `400-linked-function`,
   `600-qualified-redeclaration`, `600-qualified-redeclaration2`, and
   `700-reference-to-reference`.  These need PA8 declarations, definitions,
   typed entities, command-line translation-unit order, and the mock image.
2. **Initialization, object layout, and relocation** — `110-vardef`,
   `200-fundamental`, `250-nullptr`, `300-array`, `310-array-str-lit`,
   `350-function-to-pointer`, `450-reference`, `500-static-assert3`,
   `600-qualified-redeclaration2`, and `700-reference-to-reference`.
   This includes zero/constant initialization, fundamental widths and
   alignment, string-array encoding, function/reference/pointer offsets,
   and lifetime-extended temporaries.
3. **Declaration and namespace diagnostics** — `120-const-default`,
   `150-inline-namespace`, `300-bad-ref1`, `300-bad-ref2`, `300-bad-ref3`,
   `300-nonenclosing-qualified-decl`, `300-uninit-ref`, `300-void-ref`,
   `340-array-const`, `400-double-func-def`,
   `400-namespace-alias-misuse`, `400-namespace-alias-to-self`,
   `400-using-decl-to-namespace`, and all six `410-namespace-conflict*`
   cases.  These need PA8 semantic checks while retaining PA7 lookup rules.
4. **Constant expressions** — `340-array-const`, `500-static-assert`,
   `500-static-assert2`, `500-static-assert3`, and the constant bounds and
   initializers used by the other groups.  Literals, id-expressions,
   null-pointer constants, and const/constexpr propagation must be typed
   facts rather than test-specific answers.

## Checkpoint Scope

Implement a reusable PA8 front end and mock-image builder for the first two
groups: preprocess and parse all PA8 declaration forms; maintain typed
fundamental/pointer/reference/array/function entities across translation
units; resolve declarations and definitions; evaluate supported literal and
id-expression initializers; lay out variables, temporaries, functions, and
strings with PA8 alignment/order rules; and apply relocations after final
offsets are known.  Validation covers the successful declaration, linkage,
fundamental, array, string, pointer, function, reference, and multi-unit
cases, with earlier PA tests remaining green.

The diagnostic and constant-expression cases are the next checkpoint group;
the implementation should expose enough semantic facts for that group to be
added without replacing the image builder.

## Checkpoint result

Implemented and validated the full PA8 scope. The parser now preprocesses and
parses the PA8 declaration forms into typed entities across command-line
translation units; the semantic checks cover linkage, namespaces, ODR,
references, arrays, initialization, `static_assert`, and constant bounds; and
the image builder emits PA8 layout, lifetime-extended temporaries, strings,
functions, and relocations. The literal and image phases are separate source
modules and share the typed model.

Validation completed:

- `make test-report ACTIVE_TEST_REPORT_PAS='pa8'`: 41/41 PA8 tests passed.
- `make test-report-through-pa7`: 292/292 earlier tests passed.
- `perl scripts/cppgm_file_audit.pl --stage pa8 --paths dev/src`: passed
  (two pre-existing/non-fatal substantial-header warnings).

There are no remaining checked-in PA8 test failures. The former four failure
groups are all covered by this checkpoint.

## Next checkpoint group

Final handoff only: rebuild after cleanup, rerun the PA8 report, through-PA7
regression, and file audit, then commit the cohesive PA8 implementation. PA9
is the next assignment after this clean checkpoint.
