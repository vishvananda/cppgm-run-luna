#pragma once

#include <vector>

#include "ast_parser.h"

// PA18 keeps the earlier semantic and lowering passes deliberately typed.  The
// small supported template subset is therefore made concrete at the AST
// boundary: template declarations are retained in a registry, and each used
// specialization is materialized as an ordinary declaration before PA11 sees
// the translation unit.
std::vector<CPPGMAstNodePtr> ExpandPA18Templates(
	const std::vector<CPPGMAstNodePtr>& translation_units);
