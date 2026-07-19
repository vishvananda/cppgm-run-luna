#pragma once

#include <set>
#include <string>
#include <vector>

#include "posttoken_lexer.h"

void RunCtrlExpr(const std::string& input);

bool EvaluateControlExpression(const std::vector<PostPPToken>& tokens,
	const std::set<std::string>& defined_names, bool* result);
