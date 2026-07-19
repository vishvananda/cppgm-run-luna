#pragma once

#include <cstddef>
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

int RawPrefixLength(const std::vector<int>& source, std::size_t position);
std::vector<bool> MarkRawLiteralSpans(const std::vector<int>& source);

std::vector<SourceUnit> BuildSourceUnits(
	const std::vector<int>& decoded,
	const std::vector<bool>& raw_spans);
bool AddTranslatedRawSpans(const std::vector<SourceUnit>& source,
	std::vector<bool>* raw_spans);
