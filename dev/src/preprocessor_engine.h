#pragma once

#include <string>
#include <vector>

#include "posttoken_lexer.h"

// Preprocess one source file with the PA5 macro, conditional, and include
// engine.  The returned stream contains post-preprocessor tokens and omits
// the synthetic EOF token; callers own the vector and may add the consumer's
// stage-specific end marker.
std::vector<PostPPToken> PreprocessSourceFile(const std::string& path);
