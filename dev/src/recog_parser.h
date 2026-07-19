#pragma once

#include <vector>

#include "posttoken_lexer.h"

// Recognize one PA6 translation unit.  The parser consumes the already
// translated post-token stream and returns false for either a lexical error
// token or a sequence that does not reduce to translation-unit.
bool RecognizePA6(const std::vector<PostPPToken>& tokens);
