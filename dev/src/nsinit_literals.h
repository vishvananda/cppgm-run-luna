#pragma once

#include <string>
#include <vector>

#include "nsinit_model.h"

enum LiteralEncoding { LIT_CHAR, LIT_UTF16, LIT_UTF32, LIT_WCHAR };

struct QuotedData
{
	LiteralEncoding encoding;
	std::vector<int> values;
	std::string suffix;

	QuotedData() : encoding(LIT_CHAR), values(), suffix() {}
};

struct EncodedString
{
	Type type;
	std::vector<unsigned char> bytes;
	std::vector<int> values;
};

QuotedData ParseQuoted(const std::string& source, bool character);
EncodedString EncodeString(const QuotedData& quoted);
bool LooksFloating(const std::string& source);
std::string IntegerCore(const std::string& source, std::string* suffix, int* base);
Type IntegerLiteralType(unsigned long long value, const std::string& suffix, int base);
