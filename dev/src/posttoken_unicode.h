#pragma once

#include <string>
#include <vector>

std::vector<int> PostDecodeUTF8(const std::string& input);
std::string PostEncodeUTF8(int code_point);
bool PostIsValidCodePoint(int code_point);
