#pragma once

#include <iosfwd>
#include <string>

// Analyze one PA7 translation unit and emit its namespace/entity description.
// The implementation owns the semantic namespace and type model internally so
// callers cannot accidentally bypass name lookup or output ordering.
void EmitNSDeclTranslationUnit(const std::string& source_path, std::ostream& out);
