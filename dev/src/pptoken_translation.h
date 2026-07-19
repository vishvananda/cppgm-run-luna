#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct SourceUnit
{
	static const std::size_t NoOrigin = static_cast<std::size_t>(-1);

	int code_point;
	bool raw;
	std::size_t origin_begin;
	std::size_t origin_end;

	SourceUnit(int code_point = 0, bool raw = false,
		std::size_t origin_begin = NoOrigin,
		std::size_t origin_end = NoOrigin)
		: code_point(code_point), raw(raw), origin_begin(origin_begin),
		  origin_end(origin_end)
	{}
};

bool IsAsciiDigit(int code_point);
bool IsAsciiOctalDigit(int code_point);
bool IsHexDigit(int code_point);
bool IsIdentifierNondigit(int code_point);
bool IsIdentifierStart(int code_point);
bool IsIdentifierBody(int code_point);
bool IsSourceWhitespace(int code_point);
bool IsRawDelimiterCodePoint(int code_point);

std::string EncodeUTF8CodePoint(int code_point);
std::string EncodeUnits(const std::vector<SourceUnit>& source,
	std::size_t begin, std::size_t end);
std::vector<SourceUnit> BuildSourceUnits(
	const std::vector<int>& decoded,
	const std::vector<bool>& raw_spans);
bool AddTranslatedRawSpans(const std::vector<SourceUnit>& source,
	std::vector<bool>* raw_spans);
std::vector<SourceUnit> TranslateSource(const std::string& input);
