# PA2 checkpoint plan

## Baseline and complete failure set

At the start of this checkpoint the required PA2 report had 0/19 tests
passing.  Every test stopped in the `posttoken` stub with
`EXIT_NOT_IMPLEMENTED`; PA1-through tests passed.

## Remaining Work Map

The current-PA failures group by shared compiler behavior:

1. **Token-stream integration (all 19 tests):** connect the existing PA1
   preprocessing-token stream to a PA2 consumer, ignore whitespace/newlines,
   continue after conversion errors, and emit `eof`.
2. **Simple and invalid preprocessing tokens (1 test):**
   `pa2/tests/100-simple.t` checks keyword/operator/digraph mapping,
   identifiers, and invalid preprocessing operators.
3. **Numeric post-tokenization (11 tests):** integer and floating grammar
   classification, user-defined numeric suffix splitting, ABI-sized integer
   type selection/range checks, and bit-compatible floating conversion.  The
   group is `100-integer-zero`, `200-basic-floating`,
   `200-basic-integer-suffix`, `200-octal-limits`, `300-floating-suffix`,
   `300-hex-limits`, `300-integer-limits`, `500-plus-ud-suffix`,
   `050-two-to-two`, `060-illegal-float-literals`, and
   `200-long-ud-literals`.
4. **Character literals (2 tests):** decode ordinary, escaped, universal,
   UTF-prefixed, and user-defined character literals with the PA2 Unicode
   validity and ABI type rules (`200-character-literal` and shared `040-char`).
5. **String literals (5 tests):** decode ordinary and raw strings into UTF-8,
   UTF-16, or UTF-32 ABI storage, classify user-defined string literals, and
   process maximal mixed literal sequences with encoding/suffix validation.
   The group is `250-string-literal`, `250-ud-strchar`, `400-raw-string`,
   `450-string-literal-concat`, and `700-hard-string-concat`.

## Checkpoint Scope

Implement the complete PA2 `posttoken` behavior as a coherent extension of
the existing PA1 tokenizer, split into the `posttoken_lexer`,
`posttoken_unicode`, and `posttoken_semantics` modules: typed
preprocessing-token records, simple/identifier conversion, integer and
floating literal analysis, Unicode escape decoding and ABI hexdumps,
raw/non-raw string conversion, user-defined literals, maximal string
concatenation, invalid-token recovery, and final EOF emission.  Validate it
against all 19 current-PA tests and the required PA1-through report, then run
the PA2 source file audit.

## Checkpoint result

Complete.  The implementation converts the full PA1-style translated token
stream in process and keeps conversion errors as PA2 `invalid` tokens.  The
PA2 local/course suites pass 19/19; the PA2-only root report passes 19/19;
the through-PA1 report passes 49/49; and the PA2 source audit passes with 18
files checked.

## Remaining work after checkpoint

No PA2 fixture behavior remains.  Any future PA2 defect found outside the
checked-in suite should be grouped by translation, literal classification,
Unicode encoding, or string-sequence semantics rather than handled as a
fixture-specific exception.

## Architecture Review

The integrated PA2 path has three explicit layers. `dev/posttoken.cpp` owns
the small application boundary: it buffers UTF-8 standard input, invokes
`LexPostPPSource`, and passes the resulting preprocessing-token records to
`RunPostToken`. `dev/src/posttoken_lexer.cpp` owns token-stream assembly,
whitespace/comment skipping, raw and quoted preprocessing-token recognition,
directive/header context, and longest-match punctuator handling.

Phase 1 through phase 3 behavior is shared with PA1 instead of being copied
into the PA2 driver. `dev/src/pptoken_translation.cpp/.h` owns strict UTF-8
decode/encode, Annex-E identifier predicates, `SourceUnit` origin metadata,
raw-span discovery, trigraph replacement, line splicing, UCN conversion, and
the final synthetic newline. The source-set entry in
`dev/frontend_source_sets.mk` links this module to both `pptoken` and
`posttoken`, so the through-stage boundary uses one implementation.

`dev/src/posttoken_semantics.cpp` is responsible only for converting typed
preprocessing tokens to PA2 output: simple-token mapping, integer candidate
selection, decimal/hexadecimal floating scanning, character escape decoding,
ABI byte dumps, and maximal string-literal concatenation. The separate
`posttoken_unicode` module supplies the semantic UTF-8 decode/encode helpers.
Conversion failures are caught at this layer and emitted as `invalid` while
phase 1–3 failures still escape to `main` and return failure, matching the
assignment contract.

The checkpoint review found the duplicated translation path, the hexadecimal
float scanner issue, Unicode/raw translation gaps, and token-boundary/context
edges recorded in `pa2/audit.md`. The cleanup keeps the public PA1 token
stream unchanged while making PA2 consume the same translated source model.
The normal path has no fixture-name branches, reference-binary dependency,
subprocess, or host-toolchain shortcut.

## Final Architecture Review

The final stage flow is:

    UTF-8 bytes -> shared phase 1-3 SourceUnit stream
                 -> PA2 preprocessing-token lexer
                 -> typed literal/simple/identifier semantics
                 -> PA2 output and eof

Raw protection is source metadata with origin bounds, not a PA2 test special
case. The shared translation pass protects raw bodies before trigraph,
splice, and UCN transformations, then performs a bounded follow-up discovery
for prefixes formed by those transformations. The lexer consumes the result
without reinterpreting protected body text. Identifier and pp-number scans
use the same Unicode classification as PA1, and the `<::` exception remains
at the tokenization boundary where it belongs.

Ownership is now cohesive: `pptoken_translation.*` owns reusable source
translation, `pptoken.cpp` owns the PA1 stream consumer, `posttoken_lexer.cpp`
owns PA2 token records, and `posttoken_semantics.cpp` owns PA2 conversion and
output. The implementation uses bounded delimiter checks, linear source
passes for ordinary inputs, maximal-run string processing, and in-process
numeric/Unicode conversion. It does not inspect tests or references and does
not invoke another compiler or process.

The final stage preserves PA1, as shown by the through-PA2 report and the
independent PA1 report, while the PA2 local/course fixtures and source audit
are green. The complete final audit, including checkpoint comparison and
validation evidence, is recorded in `pa2/audit.md`.

## Next checkpoint group

The next assignment checkpoint is PA3 preprocessing behavior, beginning with
the existing PA2 token-stream boundary and adding the PA3 macro/preprocessor
rules without regressing PA1 or PA2.
