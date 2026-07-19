# PA4 checkpoint plan

## Baseline

At the start of this checkpoint the required PA4 report was `0/69` tests
passing. The tool returned `EXIT_NOT_IMPLEMENTED` for every case; the
earlier PA1--PA3 report was clean.

## Remaining Work Map

The complete 69-test failure set was grouped by shared behavior as follows.
The groups are disjoint and cover all 38 local tests plus all 31 course tests.

1. Directive parsing, phase-3 boundaries, and basic macro state (19):
   local `100-empty`, `100-nodefs`, `150-max`, `200-fnlike`, `200-onedef`;
   course `097-unterminated-define2`, `097-unterminated-define3`,
   `097-unterminated-define4`, `097-unterminated-define5`,
   `099-empty-function`, `099-extra-paren`, `099-invalid-is-valid`,
   `099-low-param`, `099-object-vs-function`, `099-redef-extra-end-space`,
   `100-undef-simple`, `400-fun-macro-define`, `410-trigraph-in-raw-string`,
   `500-hello-world`.

2. Definition validation, redefinition, and required diagnostics (21):
   local `250-badvargs1`, `250-badvargs2`, `250-badvargs3`, `250-badvargs4`,
   `300-badhash1-1`, `300-badhash1-2`, `300-badhash1-3`,
   `300-badhash2-1`, `300-badhash2-2`, `300-badhash2-3`, `300-badhash2-4`,
   `300-identifier-missing`, `300-undef-extra`, `700-redef2`,
   `700-redeferr1`, `700-redeferr2`, `700-redeferr3`, `700-redeferr4`;
   course `250-badvaargs-param`, `250-badvaargs-undef`, `300-redef`.

3. Rescanning, parameter substitution, and recursive expansion (13):
   local `600-recurse`, `650-recurse`, `700-redef-a`, `700-redef-q`,
   `900-recurse`, `910-recurse2`; course `150-hash-outside`,
   `185-multiple-pp-tokents`, `364-define-inside-macro`,
   `600-hash-from-macro`, `600-parameter-selected-macro-rescans`,
   `600-pasted-helper-macro-rescans`, `600-tail-helper-macro-rescans`.

4. Stringizing, token pasting, placemarkers, varargs, and posttoken output
   (16): local `250-join`, `500-tricky-join`, `700-strlit-a`,
   `700-strlit-a2`, `700-strlit-q`, `800-placemarker-a`,
   `800-placemarker-q`, `850-varargs-a`, `850-varargs-q`; course
   `200-whitespaces`, `250-goodvaargs`, `300-double-hash`,
   `301-gnu-vaargs-comma-paste`, `552-make-raw-string`, `stringized-macro`,
   `stringized-macro-bad`.

## Checkpoint Scope

Complete the whole PA4 implementation in this checkpoint. The scope covers
phase-1--3 preprocessing-token capture using the existing translation layer,
`#define`/`#undef` parsing and redefinition rules, object-like and
function-like macros (including variadic forms), argument collection,
stringizing, token pasting and placemarkers, blue-painted rescanning, and
conversion of the resulting typed tokens through the existing PA2 posttoken
semantics.

## Checkpoint Result

Implemented the scope in `dev/src/macro_engine.cpp`, with a thin `macro`
entry point and the new source listed in `dev/frontend_source_sets.mk`.
Validation is complete: PA4 local tests pass `38/38`, PA4 course tests pass
`31/31`, the active PA4 report passes `69/69`, the through-PA3 report passes
`87/87`, and `perl scripts/cppgm_file_audit.pl --stage pa4 --paths dev/src`
passes.

## Remaining Work

No PA4 groups remain. The next checkpoint group is PA5: read its contract and
tests, establish the PA5 baseline, and group its failures before editing.
