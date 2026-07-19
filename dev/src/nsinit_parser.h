#pragma once

#include <string>
#include <vector>

// Translate the supplied PA8 translation units and build the PA8 mock image.
// The function throws std::exception on a diagnostic-required ill-formed
// program or an input/output error.
void BuildNSInitImage(const std::vector<std::string>& source_paths,
	std::vector<unsigned char>* image);
