#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "pptoken_translation.h"

using namespace std;

int HexCharToValue(int c)
{
	switch (c)
	{
	case '0': return 0;
	case '1': return 1;
	case '2': return 2;
	case '3': return 3;
	case '4': return 4;
	case '5': return 5;
	case '6': return 6;
	case '7': return 7;
	case '8': return 8;
	case '9': return 9;
	case 'A': return 10;
	case 'a': return 10;
	case 'B': return 11;
	case 'b': return 11;
	case 'C': return 12;
	case 'c': return 12;
	case 'D': return 13;
	case 'd': return 13;
	case 'E': return 14;
	case 'e': return 14;
	case 'F': return 15;
	case 'f': return 15;
	default: throw logic_error("HexCharToValue of nonhex char");
	}
}

// See C++ standard 2.11 Identifiers and Appendix/Annex E.1
const vector<pair<int, int>> AnnexE1_Allowed_RangesSorted =
{
	{0xA8,0xA8},
	{0xAA,0xAA},
	{0xAD,0xAD},
	{0xAF,0xAF},
	{0xB2,0xB5},
	{0xB7,0xBA},
	{0xBC,0xBE},
	{0xC0,0xD6},
	{0xD8,0xF6},
	{0xF8,0xFF},
	{0x100,0x167F},
	{0x1681,0x180D},
	{0x180F,0x1FFF},
	{0x200B,0x200D},
	{0x202A,0x202E},
	{0x203F,0x2040},
	{0x2054,0x2054},
	{0x2060,0x206F},
	{0x2070,0x218F},
	{0x2460,0x24FF},
	{0x2776,0x2793},
	{0x2C00,0x2DFF},
	{0x2E80,0x2FFF},
	{0x3004,0x3007},
	{0x3021,0x302F},
	{0x3031,0x303F},
	{0x3040,0xD7FF},
	{0xF900,0xFD3D},
	{0xFD40,0xFDCF},
	{0xFDF0,0xFE44},
	{0xFE47,0xFFFD},
	{0x10000,0x1FFFD},
	{0x20000,0x2FFFD},
	{0x30000,0x3FFFD},
	{0x40000,0x4FFFD},
	{0x50000,0x5FFFD},
	{0x60000,0x6FFFD},
	{0x70000,0x7FFFD},
	{0x80000,0x8FFFD},
	{0x90000,0x9FFFD},
	{0xA0000,0xAFFFD},
	{0xB0000,0xBFFFD},
	{0xC0000,0xCFFFD},
	{0xD0000,0xDFFFD},
	{0xE0000,0xEFFFD}
};

// See C++ standard 2.11 Identifiers and Appendix/Annex E.2
const vector<pair<int, int>> AnnexE2_DisallowedInitially_RangesSorted =
{
	{0x300,0x36F},
	{0x1DC0,0x1DFF},
	{0x20D0,0x20FF},
	{0xFE20,0xFE2F}
};

SourceUnit MakeTranslatedUnit(int code_point, bool raw,
	const SourceUnit& first, const SourceUnit& last)
{
	return SourceUnit(code_point, raw, first.origin_begin, last.origin_end);
}

bool IsHexDigit(int c)
{
	return (c >= '0' && c <= '9') ||
		(c >= 'a' && c <= 'f') ||
		(c >= 'A' && c <= 'F');
}

bool IsAsciiDigit(int c)
{
	return c >= '0' && c <= '9';
}

bool IsAsciiOctalDigit(int c)
{
	return c >= '0' && c <= '7';
}

bool IsAsciiIdentifierStart(int c)
{
	return (c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') || c == '_';
}

bool IsAsciiIdentifierBody(int c)
{
	return IsAsciiIdentifierStart(c) || IsAsciiDigit(c);
}

bool IsAnnexRangeMember(int code_point,
	const vector<pair<int, int>>& ranges)
{
	for (size_t i = 0; i < ranges.size(); ++i)
	{
		if (code_point < ranges[i].first)
			return false;
		if (code_point <= ranges[i].second)
			return true;
	}
	return false;
}

bool IsIdentifierNondigit(int code_point)
{
	return IsAsciiIdentifierStart(code_point) ||
		IsAnnexRangeMember(code_point, AnnexE1_Allowed_RangesSorted) ||
		IsAnnexRangeMember(code_point, AnnexE2_DisallowedInitially_RangesSorted);
}

bool IsIdentifierStart(int code_point)
{
	return IsAsciiIdentifierStart(code_point) ||
		(IsAnnexRangeMember(code_point, AnnexE1_Allowed_RangesSorted) &&
		 !IsAnnexRangeMember(code_point,
			AnnexE2_DisallowedInitially_RangesSorted));
}

bool IsIdentifierBody(int code_point)
{
	return IsAsciiIdentifierBody(code_point) ||
		IsAnnexRangeMember(code_point, AnnexE1_Allowed_RangesSorted) ||
		IsAnnexRangeMember(code_point, AnnexE2_DisallowedInitially_RangesSorted);
}

bool IsSourceWhitespace(int code_point)
{
	return code_point == ' ' || code_point == '\t' ||
		code_point == '\v' || code_point == '\f' ||
		code_point == '\r';
}

vector<int> DecodeUTF8(const string& input)
{
	vector<int> result;
	for (size_t i = 0; i < input.size();)
	{
		const unsigned char first =
			static_cast<unsigned char>(input[i]);
		int length = 0;
		int code_point = 0;

		if (first <= 0x7f)
		{
			length = 1;
			code_point = first;
		}
		else if (first >= 0xc2 && first <= 0xdf)
		{
			length = 2;
			code_point = first & 0x1f;
		}
		else if (first >= 0xe0 && first <= 0xef)
		{
			length = 3;
			code_point = first & 0x0f;
		}
		else if (first >= 0xf0 && first <= 0xf4)
		{
			length = 4;
			code_point = first & 0x07;
		}
		else
		{
			throw logic_error("invalid UTF-8 leading byte");
		}

		if (i + static_cast<size_t>(length) > input.size())
			throw logic_error("truncated UTF-8 sequence");

		for (int j = 1; j < length; ++j)
		{
			const unsigned char continuation =
				static_cast<unsigned char>(input[i + j]);
			if ((continuation & 0xc0) != 0x80)
				throw logic_error("invalid UTF-8 continuation byte");

			if (length == 3 && j == 1)
			{
				if ((first == 0xe0 && continuation < 0xa0) ||
					(first == 0xed && continuation >= 0xa0))
					throw logic_error("invalid UTF-8 code point");
			}
			if (length == 4 && j == 1)
			{
				if ((first == 0xf0 && continuation < 0x90) ||
					(first == 0xf4 && continuation > 0x8f))
					throw logic_error("invalid UTF-8 code point");
			}

			code_point = (code_point << 6) | (continuation & 0x3f);
		}

		if (code_point > 0x10ffff ||
			(code_point >= 0xd800 && code_point <= 0xdfff))
			throw logic_error("invalid UTF-8 code point");

		result.push_back(code_point);
		i += static_cast<size_t>(length);
	}
	return result;
}

string EncodeUTF8CodePoint(int code_point)
{
	if (code_point < 0 || code_point > 0x10ffff ||
		(code_point >= 0xd800 && code_point <= 0xdfff))
		throw logic_error("invalid Unicode code point");

	string result;
	if (code_point <= 0x7f)
	{
		result.push_back(static_cast<char>(code_point));
	}
	else if (code_point <= 0x7ff)
	{
		result.push_back(static_cast<char>(0xc0 | (code_point >> 6)));
		result.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
	}
	else if (code_point <= 0xffff)
	{
		result.push_back(static_cast<char>(0xe0 | (code_point >> 12)));
		result.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
		result.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
	}
	else
	{
		result.push_back(static_cast<char>(0xf0 | (code_point >> 18)));
		result.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3f)));
		result.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
		result.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
	}
	return result;
}

string EncodeUnits(const vector<SourceUnit>& source, size_t begin, size_t end)
{
	string result;
	for (size_t i = begin; i < end; ++i)
		result += EncodeUTF8CodePoint(source[i].code_point);
	return result;
}

int RawPrefixLength(const vector<int>& source, size_t position)
{
	if (position >= source.size())
		return -1;
	if (source[position] == 'R' && position + 1 < source.size() &&
		source[position + 1] == '"')
		return 1;
	if (source[position] == 'u' && position + 2 < source.size() &&
		source[position + 1] == 'R' && source[position + 2] == '"')
		return 2;
	if (source[position] == 'U' && position + 2 < source.size() &&
		source[position + 1] == 'R' && source[position + 2] == '"')
		return 2;
	if (source[position] == 'L' && position + 2 < source.size() &&
		source[position + 1] == 'R' && source[position + 2] == '"')
		return 2;
	if (source[position] == 'u' && position + 3 < source.size() &&
		source[position + 1] == '8' && source[position + 2] == 'R' &&
		source[position + 3] == '"')
		return 3;
	return -1;
}

bool IsRawDelimiterCodePoint(int code_point)
{
	return code_point != ' ' && code_point != '(' && code_point != ')' &&
		code_point != '\\' && code_point != '\t' &&
		code_point != '\v' && code_point != '\f' &&
		code_point != '\r' && code_point != '\n';
}

struct RawProbe
{
	bool candidate;
	size_t end;

	RawProbe(bool candidate = false, size_t end = 0)
		: candidate(candidate), end(end)
	{}
};

RawProbe ProbeRawLiteral(const vector<int>& source, size_t position)
{
	const int prefix_length = RawPrefixLength(source, position);
	if (prefix_length < 0)
		return RawProbe();

	const size_t delimiter_begin =
		position + static_cast<size_t>(prefix_length) + 1;
	size_t delimiter_length = 0;
	size_t open_paren = delimiter_begin;
	for (; open_paren < source.size(); ++open_paren)
	{
		if (source[open_paren] == '(')
			break;
		if (source[open_paren] == '\n' ||
			!IsRawDelimiterCodePoint(source[open_paren]))
			return RawProbe();
		++delimiter_length;
		if (delimiter_length > 16)
		{
			for (size_t j = open_paren + 1; j < source.size(); ++j)
			{
				if (source[j] == '\n')
					return RawProbe();
				if (source[j] == '(')
					return RawProbe(true, source.size());
			}
			return RawProbe();
		}
	}

	if (open_paren == source.size())
		return RawProbe();

	for (size_t close = open_paren + 1; close < source.size(); ++close)
	{
		if (source[close] != ')')
			continue;
		const size_t delimiter_end = close + 1 + delimiter_length;
		if (delimiter_end >= source.size())
			continue;

		bool matches = true;
		for (size_t j = 0; j < delimiter_length; ++j)
		{
			if (source[close + 1 + j] !=
				source[delimiter_begin + j])
			{
				matches = false;
				break;
			}
		}
		if (matches && source[delimiter_end] == '"')
			return RawProbe(true, delimiter_end + 1);
	}

	return RawProbe(true, source.size());
}

size_t OrdinaryStringQuote(const vector<int>& source, size_t position)
{
	if (position >= source.size())
		return source.size();
	if (source[position] == '"')
		return position;
	if (source[position] == 'u' && position + 1 < source.size() &&
		source[position + 1] == '"')
		return position + 1;
	if (source[position] == 'U' && position + 1 < source.size() &&
		source[position + 1] == '"')
		return position + 1;
	if (source[position] == 'L' && position + 1 < source.size() &&
		source[position + 1] == '"')
		return position + 1;
	if (source[position] == 'u' && position + 2 < source.size() &&
		source[position + 1] == '8' && source[position + 2] == '"')
		return position + 2;
	return source.size();
}

size_t OrdinaryCharacterQuote(const vector<int>& source, size_t position)
{
	if (position >= source.size())
		return source.size();
	if (source[position] == '\'')
		return position;
	if (source[position] == 'u' && position + 1 < source.size() &&
		source[position + 1] == '\'')
		return position + 1;
	if (source[position] == 'U' && position + 1 < source.size() &&
		source[position + 1] == '\'')
		return position + 1;
	if (source[position] == 'L' && position + 1 < source.size() &&
		source[position + 1] == '\'')
		return position + 1;
	return source.size();
}

size_t ConsumeScannerQuotedLiteral(const vector<int>& source,
	size_t position,
	size_t quote)
{
	size_t i = quote + 1;
	for (; i < source.size(); ++i)
	{
		if (source[i] == '\\')
		{
			if (i + 1 < source.size())
				++i;
			continue;
		}
		if (source[i] == source[quote])
		{
			++i;
			while (i < source.size() && IsIdentifierBody(source[i]))
				++i;
			return i;
		}
		if (source[i] == '\n')
			return source.size();
	}
	return source.size();
}

size_t ConsumeScannerPPNumber(const vector<int>& source, size_t position)
{
	size_t i = position;
	if (source[i] == '.')
		i += 2;
	else
		++i;

	while (i < source.size())
	{
		const int code_point = source[i];
		if (IsAsciiDigit(code_point) || IsIdentifierNondigit(code_point))
		{
			if ((code_point == 'e' || code_point == 'E') &&
				i + 1 < source.size() &&
				(source[i + 1] == '+' || source[i + 1] == '-'))
				i += 2;
			else
				++i;
			continue;
		}
		if (code_point == '.')
		{
			++i;
			continue;
		}
		break;
	}
	return i;
}

vector<bool> MarkRawLiteralSpans(const vector<int>& source)
{
	vector<bool> raw(source.size(), false);
	for (size_t i = 0; i < source.size();)
	{
		// A physical apostrophe can be the third character of ??',
		// which becomes '^' before tokenization and is not a quote.
		if (i >= 2 && source[i] == '\'' &&
			source[i - 1] == '?' && source[i - 2] == '?')
		{
			++i;
			continue;
		}
		if (IsSourceWhitespace(source[i]) || source[i] == '\n')
		{
			++i;
			continue;
		}

		if (source[i] == '/' && i + 1 < source.size() &&
			source[i + 1] == '/')
		{
			i += 2;
			while (i < source.size() && source[i] != '\n')
				++i;
			continue;
		}
		if (source[i] == '/' && i + 1 < source.size() &&
			source[i + 1] == '*')
		{
			i += 2;
			while (i + 1 < source.size() &&
				!(source[i] == '*' && source[i + 1] == '/'))
				++i;
			if (i + 1 < source.size())
				i += 2;
			else
				i = source.size();
			continue;
		}

		const RawProbe probe = ProbeRawLiteral(source, i);
		if (probe.candidate)
		{
			const size_t end = min(probe.end, source.size());
			for (size_t j = i; j < end; ++j)
				raw[j] = true;
			i = end;
			continue;
		}

		const size_t string_quote = OrdinaryStringQuote(source, i);
		if (string_quote < source.size())
		{
			i = ConsumeScannerQuotedLiteral(source, i, string_quote);
			continue;
		}
		const size_t character_quote = OrdinaryCharacterQuote(source, i);
		if (character_quote < source.size())
		{
			i = ConsumeScannerQuotedLiteral(source, i, character_quote);
			continue;
		}

		if (IsIdentifierStart(source[i]))
		{
			++i;
			while (i < source.size() && IsIdentifierBody(source[i]))
				++i;
			continue;
		}
		if (IsAsciiDigit(source[i]) ||
			(source[i] == '.' && i + 1 < source.size() &&
				IsAsciiDigit(source[i + 1])))
		{
			i = ConsumeScannerPPNumber(source, i);
			continue;
		}
		++i;
	}
	return raw;
}

int TrigraphReplacement(int first, int second, int third)
{
	if (first != '?' || second != '?')
		return -1;
	switch (third)
	{
	case '=': return '#';
	case '/': return '\\';
	case '\'': return '^';
	case '(': return '[';
	case ')': return ']';
	case '!': return '|';
	case '<': return '{';
	case '>': return '}';
	case '-': return '~';
	default: return -1;
	}
}

void ApplyTrigraphs(vector<SourceUnit>* source)
{
	vector<SourceUnit>& units = *source;
	size_t read = 0;
	size_t write = 0;
	while (read < units.size())
	{
		if (!units[read].raw && read + 2 < units.size() &&
			!units[read + 1].raw && !units[read + 2].raw)
		{
			const int replacement = TrigraphReplacement(
				units[read].code_point,
				units[read + 1].code_point,
				units[read + 2].code_point);
			if (replacement >= 0)
			{
				const SourceUnit first = units[read];
				const SourceUnit last = units[read + 2];
				units[write++] = MakeTranslatedUnit(
					replacement, false, first, last);
				read += 3;
				continue;
			}
		}
		units[write++] = units[read++];
	}
	units.resize(write);
}

void ApplyLineSplices(vector<SourceUnit>* source)
{
	vector<SourceUnit>& units = *source;
	size_t pending_origin_begin = SourceUnit::NoOrigin;
	size_t read = 0;
	size_t write = 0;
	while (read < units.size())
	{
		if (!units[read].raw && units[read].code_point == '\\' &&
			read + 1 < units.size() && !units[read + 1].raw &&
			units[read + 1].code_point == '\n')
		{
			if (pending_origin_begin == SourceUnit::NoOrigin)
				pending_origin_begin = units[read].origin_begin;
			read += 2;
			continue;
		}

		SourceUnit unit = units[read++];
		if (pending_origin_begin != SourceUnit::NoOrigin &&
			unit.origin_begin != SourceUnit::NoOrigin)
		{
			unit.origin_begin = pending_origin_begin;
		}
		pending_origin_begin = SourceUnit::NoOrigin;
		units[write++] = unit;
	}
	units.resize(write);
}

void ApplyUniversalCharacterNames(vector<SourceUnit>* source)
{
	vector<SourceUnit>& units = *source;
	size_t read = 0;
	size_t write = 0;
	while (read < units.size())
	{
		if (!units[read].raw && units[read].code_point == '\\' &&
			read + 1 < units.size() && !units[read + 1].raw &&
			units[read + 1].code_point == '\\')
		{
			units[write++] = units[read++];
			units[write++] = units[read++];
			continue;
		}

		if (!units[read].raw && units[read].code_point == '\\' &&
			read + 1 < units.size() && !units[read + 1].raw &&
			(units[read + 1].code_point == 'u' ||
			 units[read + 1].code_point == 'U'))
		{
			const size_t digits = units[read + 1].code_point == 'u' ? 4 : 8;
			if (read + 2 + digits <= units.size())
			{
				bool valid = true;
				uint32_t value = 0;
				for (size_t j = 0; j < digits; ++j)
				{
					if (units[read + 2 + j].raw ||
						!IsHexDigit(units[read + 2 + j].code_point))
					{
						valid = false;
						break;
					}
					value = (value << 4) |
						HexCharToValue(units[read + 2 + j].code_point);
				}
				if (valid)
				{
					if (value == 0 || value > 0x10ffff ||
						(value >= 0xd800 && value <= 0xdfff))
						throw logic_error("invalid universal-character-name");
					const SourceUnit first = units[read];
					const SourceUnit last = units[read + 1 + digits];
					units[write++] = MakeTranslatedUnit(
						static_cast<int>(value), false, first, last);
					read += 2 + digits;
					continue;
				}
			}
		}

		units[write++] = units[read++];
	}
	units.resize(write);
}

vector<SourceUnit> TranslateSource(const string& input)
{
	vector<int> decoded = DecodeUTF8(input);
	if (!decoded.empty() && decoded[0] == 0xfeff)
		decoded.erase(decoded.begin());

	vector<bool> raw_spans = MarkRawLiteralSpans(decoded);
	vector<SourceUnit> source;
	for (;;)
	{
		source = BuildSourceUnits(decoded, raw_spans);
		ApplyTrigraphs(&source);
		ApplyLineSplices(&source);
		ApplyUniversalCharacterNames(&source);
		if (!AddTranslatedRawSpans(source, &raw_spans))
			break;
	}

	if (!decoded.empty() && !source.empty() &&
		source.back().code_point != '\n')
		source.push_back(SourceUnit('\n'));
	return source;
}

vector<SourceUnit> BuildSourceUnits(const vector<int>& decoded,
	const vector<bool>& raw_spans)
{
	vector<SourceUnit> source;
	source.reserve(decoded.size());
	for (size_t i = 0; i < decoded.size(); ++i)
		source.push_back(SourceUnit(decoded[i], raw_spans[i], i, i + 1));
	return source;
}

bool AddTranslatedRawSpans(const vector<SourceUnit>& source,
	vector<bool>* raw_spans)
{
	// Hide already-protected raw bodies while looking for raw literals whose
	// prefixes were formed by an earlier translation step.  The sentinel is
	// not a source code point and therefore cannot start or complete a token.
	const int RawSentinel = -2;
	vector<int> probe;
	probe.reserve(source.size());
	for (size_t i = 0; i < source.size(); ++i)
		probe.push_back(source[i].raw ? RawSentinel : source[i].code_point);

	const vector<bool> discovered = MarkRawLiteralSpans(probe);
	bool added = false;
	for (size_t begin = 0; begin < discovered.size(); ++begin)
	{
		if (!discovered[begin] ||
			(begin != 0 && discovered[begin - 1]))
			continue;

		const int prefix_length = RawPrefixLength(probe, begin);
		if (prefix_length < 0)
			continue;
		const size_t quote = begin +
			static_cast<size_t>(prefix_length);
		size_t end = begin;
		while (end < discovered.size() && discovered[end])
			++end;

		const size_t body_begin = quote + 1;
		size_t open_paren = body_begin;
		for (; open_paren < end && probe[open_paren] != '('; ++open_paren)
		{}
		if (open_paren == end)
			continue;

		// A transformed delimiter was not a valid raw delimiter in the
		// original source.  Only the prefix may be formed by translation;
		// otherwise an ordinary token could be reclassified as raw.
		bool original_delimiter = true;
		for (size_t i = body_begin; i < open_paren; ++i)
		{
			if (source[i].origin_begin == SourceUnit::NoOrigin ||
				source[i].origin_end != source[i].origin_begin + 1)
			{
				original_delimiter = false;
				break;
			}
		}
		if (!original_delimiter)
			continue;

		// The prefix and its quote remain ordinary translated source.  The
		// delimiter and body are the portion that must be protected on the
		// next translation pass.
		for (size_t i = body_begin; i < end; ++i)
		{
			const size_t origin_begin = source[i].origin_begin;
			const size_t origin_end = source[i].origin_end;
			if (origin_begin == SourceUnit::NoOrigin ||
				origin_end == SourceUnit::NoOrigin)
				continue;
			for (size_t origin = origin_begin;
				origin < origin_end && origin < raw_spans->size(); ++origin)
			{
				if (!(*raw_spans)[origin])
				{
					(*raw_spans)[origin] = true;
					added = true;
				}
			}
		}
	}
	return added;
}
