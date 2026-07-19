# PA1 checkpoint plan

## Baseline

At the start of this checkpoint the required PA1 report was **0/49 tests
passing**. Every failure returned `EXIT_NOT_IMPLEMENTED` from the existing
`pptoken` scaffold, so the failure set was grouped by the compiler behavior
that must replace that shared stub rather than by individual expected-output
files.

## Remaining Work Map

- **Translation pipeline and source validation** — local `100-empty`,
  `100-utf8`, `200-charname-allowed`, `300-ucn-trigraph-ordering`,
  `300-utf8-ff`, `500-isspace-code-point-wrong`; course
  `010-utf8-bom`, `020-line-splice-comment`,
  `020-trigraph-line-splice-comment-include`, `200-e2`,
  `200-escaped-universal-name-prefix-string`, plus every token test because
  all tokens consume the translated code-point stream. Covers strict UTF-8
  decoding/encoding, BOM handling, universal-character-name conversion,
  trigraph replacement, line splicing, final newline insertion, and malformed
  input failure.
- **Whitespace, comments, and ordinary tokenization** — local `100-a`,
  `100-comments`, `100-example`, `100-floating`, `100-line-splice`,
  `100-preprocessing-op-or-punc`, `200-trigraphs`, `250-dot2-ppnum`,
  `400-angle-colon-madness`, `900-real-world`; course `010-header-name`,
  `020-not-raw-string`, `020-template-style`, `100-extra-comments`,
  `100-more-floating`, `200-trigraphs-and-comments`, `210-include2`.
  Covers maximal whitespace sequences, newlines, comments outside literals,
  identifiers, pp-numbers, longest preprocessing operators/punctuators,
  digraphs, and the `<::` exception.
- **Character and string literal lexing** — local `100-character-literals`,
  `100-raw-string-literal`, `100-string-literals`,
  `150-ud-character-literals`, `150-ud-string-literals`,
  `200-escape-sequence`, `300-quote-strange`; course
  `010-raw-string-delimiter`, `010-raw-string-literal`,
  `010-ud-literals`, `010-ud-raw-string-literal`, `020-not-raw-string`,
  `030-escape-string`, `030-escape-string-bad-hex`,
  `030-escape-string-bad-oct`.
  Covers encoding prefixes, ordinary and raw strings, character literals,
  user-defined suffixes, literal escape validation, raw delimiters, and
  preserving literal spelling after phase 3.
- **Context-sensitive header names** — local `200-header-name`; course
  `010-header-name` and `210-include2`. Covers `#`/`%:` include recognition
  after a logical line start while ignoring whitespace/comments, and leaving
  look-alike uses as ordinary tokens.
- **Malformed literal/raw-delimiter diagnostics** — local
  `100-partial-comment`, `100-partial-string-literal`; course
  `010-raw-string-delimiter-bad-trigraph`,
  `010-raw-string-delimiter-bad`. These must terminate with failure without
  corrupting already emitted token data.

## Checkpoint Scope

Implement the complete PA1 `pptoken` behavior in the existing compiler: a
typed code-point translation pipeline followed by a longest-match tokenizer
and `DebugPPTokenStream` emission. The scope includes all five groups above,
including validation and context-sensitive header recognition, so it covers
the full current 49-test stage rather than only the first scaffold-removal
increment. Validation will use the PA1 local and course suites, the required
PA1 report, the through-PA1 report, and the PA1 source-file audit.

## Checkpoint result

Completed the full checkpoint scope. `pptoken` now performs strict UTF-8
decoding/encoding, initial BOM handling, trigraph replacement, line splicing,
universal-character-name conversion, final-newline handling, comment
coalescing, longest-match preprocessing-token recognition, raw and ordinary
literal handling, user-defined literal suffixes, and context-sensitive
`#include`/`%:include` header names. It emits through the existing typed
`IPPTokenStream` interface and reports malformed source as failure.

Validation completed:

- `make -C pa1 test`: local 28/28 and course 21/21.
- `make test-report ACTIVE_TEST_REPORT_PAS='pa1'`: 49/49.
- `make test-report-through-pa1`: 49/49.
- `perl scripts/cppgm_file_audit.pl --stage pa1 --paths dev/src`: passed.

The stage moved from the 0/49 baseline to 49/49, with no current-PA failures
remaining.

## Remaining work after checkpoint

No PA1 work remains in the checked-in stage suites. Future compiler work is
outside this checkpoint and must preserve the PA1 translation/tokenization
behavior.

## Architecture Review

The checkpoint implementation keeps the assignment boundary intact. `main`
buffers standard input and feeds byte values to `PPTokenizer::process`; the EOF
call runs the translation pipeline once and emits through the existing
`IPPTokenStream`/`DebugPPTokenStream` interface. The normal PA1 path is entirely
in-process and does not inspect fixtures, reference outputs, or invoke another
tool.

Translation and tokenization have separate responsibilities:

- `DecodeUTF8`, `EncodeUTF8CodePoint`, and the Annex E range predicates operate
  on Unicode code points rather than signed input bytes.
- `SourceUnit` carries the translated code point, raw-string protection, and
  source-origin bounds. The type and raw-span provenance helpers live in
  `dev/src/pptoken_translation.h/.cpp`; the per-tool source set wires that
  module only into `pptoken`.
- `TranslateSource` applies trigraph replacement, line splicing, and
  universal-character-name conversion while retaining origins. A second raw
  discovery pass protects a raw body when its prefix becomes recognizable only
  after translation, without protecting an ordinary string that merely follows
  an identifier or pp-number.
- `PPTokenizer` performs longest-match recognition for identifiers,
  pp-numbers, literals, punctuators, and fallback characters. Its line-start
  state recognizes `#include` and `%:include` header names while comments and
  whitespace are ignored for context. Literal parsing validates escapes but
  preserves literal spelling after phase 3.

The final review found three correctness gaps in the checkpoint code: the raw
pre-scan did not skip all preceding identifier/pp-number tokens, raw prefixes
formed by line splicing or UCN conversion could have translated bodies, and a
comment inside a header name was not replaced with one whitespace character.
The review also brought the tool source back under the repository's ownership
limit by moving raw-span provenance into a named translation module. None of
these changes alter the public token-stream interface or the existing PA1 test
fixtures.

## Final Architecture Review

The integrated PA1 stage now has a stable source-to-token flow:

    bytes -> strict UTF-8 code points -> protected translation passes
          -> comments/whitespace and contextual header handling
          -> longest-match preprocessing tokens -> IPPTokenStream

Raw protection is represented as source metadata rather than as a special test
case. Translation replacements retain origin ranges, so a later raw discovery
can protect the original delimiter/body spelling on the next pass while still
translating the prefix. Header parsing independently normalizes block comments
to one space and rejects a line comment or unterminated comment before the
header terminator. These choices preserve the intended phase ordering and keep
ownership local to the translation module or tokenizer state that needs it.

The final source layout is cohesive: `dev/pptoken.cpp` owns the PA1 driver and
tokenizer, `dev/src/pptoken_translation.*` owns raw-span provenance, and
`dev/frontend_source_sets.mk` explicitly links the helper only for `pptoken`.
The normal path uses bounded range scans and linear source passes; it has no
fixture-name branches, reference-binary dependency, subprocess, or generated
script shortcut. Existing interfaces, build targets, and earlier-stage behavior
are preserved, as demonstrated by the through-PA1 report.

The final audit record, including the exact validation evidence and the
checkpoint comparison, is in `pa1/audit.md`.

## Next checkpoint group

PA2: build on the PA1 preprocessing-token stream with the assignment's next
semantic/token-processing behavior, while keeping the PA1 through-report
green.
