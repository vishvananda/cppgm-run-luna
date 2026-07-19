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

## Architecture Review

The checkpoint implementation is an in-process translation and image-building
pipeline with explicit ownership between its phases:

1. `dev/nsinit.cpp` owns command-line validation, output-file handling, and
   binary emission. `BuildNSInitImage` processes source paths in the exact
   command-line order, runs the shared PA5 `PreprocessSourceFile` pipeline,
   validates the resulting PA2 post-token stream, and then parses each
   translation unit into one program model.
2. `dev/src/nsinit_parser.cpp` owns PA8 grammar actions and semantic
   annotation. It lowers declarations into recursive `Type` values, resolves
   names through namespaces, aliases, using declarations/directives, inline
   and unnamed namespaces, and merges linked declarations while retaining
   first-declaration order. Initializers are represented as zero bytes,
   constant bytes, or position-independent `Address` facts rather than as
   presentation strings.
3. `dev/src/nsinit_model.h/.cpp` is the shared semantic-model boundary.
   `Program` owns namespaces, entities, lifetime-extended temporaries, and
   string literals with `unique_ptr`; binding maps, parent links, declaration
   order, and relocation addresses are non-owning views into those owners.
   Lookup traversal is guarded by visited sets so using-directive and alias
   cycles do not recurse indefinitely. The implementation is compiled once
   through the explicit `nsinit` source set in
   `dev/frontend_source_sets.mk`.
4. `dev/src/nsinit_literals.cpp` decodes PA2 literal spellings and emits the
   PA8 fundamental/string representations. `dev/src/nsinit_image.cpp` is a
   separate layout/link phase: it writes the PA8 magic, aligns and emits
   defined variables and declared functions in block 1, lifetime-extended
   temporaries in block 2, strings in block 3, and applies little-endian
   relocations after all offsets are known.

The pipeline performs no reference-output lookup, host-compiler invocation,
subprocess execution, fixture dispatch, or test-name branching. Layout and
relocation are driven by the typed entity/address graph. Entity and string
order vectors are append-only, so multi-translation-unit order and first-use
order remain stable while linked declarations reuse their existing owners.

## Final Architecture Review

The final audit covered checkpoint commit `a524e5d` and the integrated PA8
stage after cleanup. The checkpoint had the complete checked-in behavior, but
the final review found two architectural issues that were not represented in
the checkpoint record:

- The new `nsinit_model.h` contained the complete type utilities, lookup
  implementation, and ownership operations inline. That obscured the parser
  and image-writer boundary and triggered a new substantial-header warning.
  The implementation now lives in `dev/src/nsinit_model.cpp`; the header
  contains the shared data contract and declarations, and the new source is
  wired only to `nsinit`.
- The image builder synthesized a `__cppgm_entry` function when no input
  declared `main`. PA8 requires output for declared functions and defined
  variables, not an implicit entry entity. The synthetic path was removed, so
  an input containing only a defined variable emits only that variable after
  the magic bytes.

The image writer also no longer mutates an entity through `const_cast`; it
receives the owned mutable entity pointer when assigning its final offset.
These changes preserve all checked-in PA8 output while making ownership,
source-set dependencies, and output eligibility explicit. The final source
audit has no new PA8 warning; the remaining warning is the pre-existing
declaration-heavy `dev/src/recog_parser_internal.h` heuristic from PA6.

No PA8 failure group remains in the checked-in suite. The final cleanup keeps
the PA1--PA7 preprocessing, token, expression, recognizer, and namespace
stages on their existing source boundaries, and the through-PA8 validation
below confirms that the integrated build still exercises all 333 tests.

## Remaining Work Map

There is no remaining checked-in PA8 work. PA9 may extend the model, but must
retain the PA8 parser/image phases, explicit source-set ownership, and the
through-PA8 regression result.

## Next checkpoint group

The final PA8 handoff is complete after the cleanup and validation recorded
below. PA9 is the next assignment; future work must preserve the through-PA8
report and the typed model/image boundary established here.
