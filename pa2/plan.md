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

## Next checkpoint group

The next assignment checkpoint is PA3 preprocessing behavior, beginning with
the existing PA2 token-stream boundary and adding the PA3 macro/preprocessor
rules without regressing PA1 or PA2.
