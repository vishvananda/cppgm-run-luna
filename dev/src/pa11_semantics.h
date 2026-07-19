#pragma once

#include <iosfwd>

#include "ast_parser.h"

// Analyze the PA10 syntax tree and emit the deterministic PA11 scope/type
// description.  Semantic failures are reported as exceptions so the driver
// can preserve the assignment's EXIT_FAILURE contract.
void EmitPA11Types(const CPPGMAstNodePtr& translation_unit, std::ostream& out);
