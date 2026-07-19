#pragma once

#include <vector>

#include "posttoken_lexer.h"

void RunPostToken(const std::vector<PostPPToken>& tokens);

// Validate the post-token stream without exposing the presentation format of
// the posttoken tool to a downstream consumer.
bool ValidatePostTokens(const std::vector<PostPPToken>& tokens);
