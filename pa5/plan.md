# PA5 implementation plan

## Failure inventory and Remaining Work Map

The turn-start PA5 report has 68 failures, all with the same immediate cause:
`dev/preproc.cpp` exits with `EXIT_NOT_IMPLEMENTED` before processing an input.
The failures group into these shared behaviors:

1. **Driver and token/output pipeline** — `100-*`, `120-invalid`, `150-*`,
   and `100-nodefs`: command-line parsing, per-source `preproc`/`sof`/`eof`
   framing, PA1 translation/tokenization, and PA2 post-token output/error
   handling.
2. **Directive state and conditional inclusion** — `170-*`, `200-if`,
   `1984-defined-misuse`, `210-defined-identifier-like-operator`, and
   `exclude`: logical-line directive recognition, nested `#if` groups,
   `#ifdef`/`#ifndef`, `#elif`/`#else` ordering, inactive-section skipping,
   and PA3 control-expression evaluation after macro replacement.
3. **Macro state and expansion** — `200-fnlike`, `200-onedef`, all
   `250-*`, `300-*`, `500-tricky-join`, `600-recurse`, `650-recurse`,
   `700-*`, `800-placemarker-*`, `850-*`, `900-*`, `910-*`, and
   `900-repeated-argument-cache`: persistent definitions, function-like and
   variadic invocation, stringizing/pasting/placemarkers, rescanning,
   validation, and compatible redefinition.
4. **Files and pragma processing** — `200-include`, `400-bad-include`,
   `400-header-guarded`, `600-pragma-op`, `800-pragma-once`, and the
   corresponding header fixtures: include search relative to `__FILE__`,
   recursive processing, file identity tracking, `#pragma once`, and
   `_Pragma` removal/dispatch.
5. **Presumed source location and predefined state** — `400-predefined-*`,
   `500-predefined-macros`, `600-line-macro`, `610-line-macro`,
   `660-line-directive`, and `300-line-new-line`: `__FILE__`, `__LINE__`,
   `__DATE__`, `__TIME__`, fixed course macros, and `#line` bookkeeping.
6. **Independent source units** — `two-files`: reset per-file conditional,
   pragma-once, and output state while retaining only the intended macro
   isolation semantics.

## Checkpoint Scope

Implement the complete reusable PA5 preprocessing pipeline in the existing
compiler: source translation and logical-line collection; persistent typed
macro state with PA4 expansion; conditional directives and PA3 expressions;
include recursion and `#pragma once`; predefined/source-location macros,
`#line`, `_Pragma`, `#error`, null directives, and non-directives; and final
PA2 post-token emission for every top-level source file. Validate the scope
with the full PA5 local/course report, the through-PA4 report, and the source
file audit. This bundles the groups because they all depend on one streaming
preprocessor state machine and the current implementation has no partial
PA5 behavior to preserve.

## Checkpoint result

Complete. `preproc` now owns a streaming PA5 source walk over the shared
phase 1–3 translation/tokenizer. It maintains typed conditional state,
reuses PA4 macro replacement through a persistent `MacroState`, evaluates
`#if`/`#elif` with the PA3 typed evaluator and real macro-defined facts, and
emits one combined PA2 post-token stream per top-level source. Include search,
recursive headers, `#pragma once` file identities, direct pragmas, `_Pragma`,
`#line`, all required predefined macros, inactive-section handling, errors,
null directives, non-directives, and independent multi-source state are
implemented in the same path.

The macro state now carries source file/line provenance through replacement,
argument expansion, stringizing, and rescanning so `__FILE__` and `__LINE__`
are semantic expansions rather than test-specific substitutions. Macro hide
sets use copy-on-write typed bitsets; this preserves recursive expansion while
making the 8,000-level repeated-argument regression linear enough for the
normal test timeout.

Validation completed:

- `make test-report ACTIVE_TEST_REPORT_PAS='pa5'`: 103/103 active checks.
- `make test-report-through-pa4`: 19/19 checks.
- `perl scripts/cppgm_file_audit.pl --stage pa5 --paths dev/src`: passed,
  22 files checked.
- Direct PA5 run: local 62/62 and course 6/6 tests passed.

The current PA5 baseline was 0/68; the current PA5 suite is fully passing.

## Remaining work after checkpoint

No PA5 behavior remains in the checked-in local or course suites. The six
baseline groups—driver/output, conditional directives, macro state, files and
pragmas, source-location/predefined state, and independent source units—are
covered by the passing report. Future work must preserve the reusable macro
provenance API, PA3 evaluator, and through-PA4 behavior.

## Next checkpoint group

No next PA5 checkpoint group; this stage is complete. The next assignment may
extend the shared preprocessor/compiler boundary only after rerunning the
through-PA5 report.
