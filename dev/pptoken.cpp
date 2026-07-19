#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;

#include "IPPTokenStream.h"
#include "DebugPPTokenStream.h"
#include "exceptions.h"

// Translation features you need to implement:
// - utf8 decoder
// - utf8 encoder
// - universal-character-name decoder
// - trigraphs
// - line splicing
// - newline at eof
// - comment striping (can be part of whitespace-sequence)

// EndOfFile: synthetic "character" to represent the end of source file
constexpr int EndOfFile = -1;

// given hex digit character c, return its value
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

// See C++ standard 2.13 Operators and punctuators
const unordered_set<string> Digraph_IdentifierLike_Operators =
{
	"new", "delete", "and", "and_eq", "bitand",
	"bitor", "compl", "not", "not_eq", "or",
	"or_eq", "xor", "xor_eq"
};

// See `simple-escape-sequence` grammar
const unordered_set<int> SimpleEscapeSequence_CodePoints =
{
	'\'', '"', '?', '\\', 'a', 'b', 'f', 'n', 'r', 't', 'v'
};

struct SourceUnit
{
	int code_point;
	bool raw;

	SourceUnit(int code_point = 0, bool raw = false)
		: code_point(code_point), raw(raw)
	{}
};

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
			while (i < source.size() && IsAsciiIdentifierBody(source[i]))
				++i;
			return i;
		}
		if (source[i] == '\n')
			return source.size();
	}
	return source.size();
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

		if (IsAsciiIdentifierStart(source[i]))
		{
			++i;
			while (i < source.size() && IsAsciiIdentifierBody(source[i]))
				++i;
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

vector<SourceUnit> ApplyTrigraphs(const vector<SourceUnit>& source)
{
	vector<SourceUnit> result;
	for (size_t i = 0; i < source.size();)
	{
		if (!source[i].raw && i + 2 < source.size() &&
			!source[i + 1].raw && !source[i + 2].raw)
		{
			const int replacement = TrigraphReplacement(
				source[i].code_point,
				source[i + 1].code_point,
				source[i + 2].code_point);
			if (replacement >= 0)
			{
				result.push_back(SourceUnit(replacement));
				i += 3;
				continue;
			}
		}
		result.push_back(source[i]);
		++i;
	}
	return result;
}

vector<SourceUnit> ApplyLineSplices(const vector<SourceUnit>& source)
{
	vector<SourceUnit> result;
	for (size_t i = 0; i < source.size(); ++i)
	{
		if (!source[i].raw && source[i].code_point == '\\' &&
			i + 1 < source.size() && !source[i + 1].raw &&
			source[i + 1].code_point == '\n')
		{
			++i;
			continue;
		}
		result.push_back(source[i]);
	}
	return result;
}

vector<SourceUnit> ApplyUniversalCharacterNames(
	const vector<SourceUnit>& source)
{
	vector<SourceUnit> result;
	for (size_t i = 0; i < source.size();)
	{
		if (!source[i].raw && source[i].code_point == '\\' &&
			i + 1 < source.size() && !source[i + 1].raw &&
			source[i + 1].code_point == '\\')
		{
			result.push_back(source[i]);
			result.push_back(source[i + 1]);
			i += 2;
			continue;
		}

		if (!source[i].raw && source[i].code_point == '\\' &&
			i + 1 < source.size() && !source[i + 1].raw &&
			(source[i + 1].code_point == 'u' ||
			 source[i + 1].code_point == 'U'))
		{
			const size_t digits = source[i + 1].code_point == 'u' ? 4 : 8;
			if (i + 2 + digits <= source.size())
			{
				bool valid = true;
				int value = 0;
				for (size_t j = 0; j < digits; ++j)
				{
					if (source[i + 2 + j].raw ||
						!IsHexDigit(source[i + 2 + j].code_point))
					{
						valid = false;
						break;
					}
					value = (value << 4) |
						HexCharToValue(source[i + 2 + j].code_point);
				}
				if (valid)
				{
					if (value == 0 || value > 0x10ffff ||
						(value >= 0xd800 && value <= 0xdfff))
						throw logic_error("invalid universal-character-name");
					result.push_back(SourceUnit(value));
					i += 2 + digits;
					continue;
				}
			}
		}

		result.push_back(source[i]);
		++i;
	}
	return result;
}

vector<SourceUnit> TranslateSource(const string& input)
{
	vector<int> decoded = DecodeUTF8(input);
	if (!decoded.empty() && decoded[0] == 0xfeff)
		decoded.erase(decoded.begin());

	const vector<bool> raw_spans = MarkRawLiteralSpans(decoded);
	vector<SourceUnit> source;
	for (size_t i = 0; i < decoded.size(); ++i)
		source.push_back(SourceUnit(decoded[i], raw_spans[i]));

	source = ApplyTrigraphs(source);
	source = ApplyLineSplices(source);
	source = ApplyUniversalCharacterNames(source);

	if (!decoded.empty() && !source.empty() &&
		source.back().code_point != '\n')
		source.push_back(SourceUnit('\n'));
	return source;
}

enum EPPTokenKind
{
	PP_IDENTIFIER,
	PP_NUMBER,
	PP_CHARACTER_LITERAL,
	PP_USER_DEFINED_CHARACTER_LITERAL,
	PP_STRING_LITERAL,
	PP_USER_DEFINED_STRING_LITERAL,
	PP_PREPROCESSING_OP_OR_PUNC,
	PP_NON_WHITESPACE_CHARACTER
};

struct LexedToken
{
	EPPTokenKind kind;
	size_t end;
	string data;

	LexedToken(EPPTokenKind kind, size_t end, const string& data)
		: kind(kind), end(end), data(data)
	{}
};

struct PPTokenizer
{
	IPPTokenStream& output;
	string input;
	bool finished;
	vector<SourceUnit> source;
	size_t position;
	bool at_line_start;
	bool expecting_directive_name;
	bool after_include;

	PPTokenizer(IPPTokenStream& output)
		: output(output), finished(false), position(0),
		  at_line_start(true), expecting_directive_name(false),
		  after_include(false)
	{}

	void process(int c)
	{
		if (finished)
			return;
		if (c != EndOfFile)
		{
			if (c < 0 || c > 255)
				throw logic_error("invalid input byte");
			input.push_back(static_cast<char>(c));
			return;
		}

		finished = true;
		source = TranslateSource(input);
		Tokenize();
	}

	bool Matches(size_t begin, const string& text) const
	{
		if (begin + text.size() > source.size())
			return false;
		for (size_t i = 0; i < text.size(); ++i)
		{
			if (source[begin + i].code_point !=
				static_cast<unsigned char>(text[i]))
				return false;
		}
		return true;
	}

	LexedToken MakeToken(EPPTokenKind kind, size_t end) const
	{
		return LexedToken(kind, end, EncodeUnits(source, position, end));
	}

	size_t ConsumeIdentifierSuffix(size_t begin) const
	{
		if (begin >= source.size() ||
			!IsIdentifierStart(source[begin].code_point))
			return begin;
		size_t i = begin + 1;
		while (i < source.size() && IsIdentifierBody(source[i].code_point))
			++i;
		return i;
	}

	size_t ValidateEscape(size_t slash) const
	{
		if (slash + 1 >= source.size())
			throw logic_error("unterminated escape sequence");
		const int escaped = source[slash + 1].code_point;
		if (SimpleEscapeSequence_CodePoints.find(escaped) !=
			SimpleEscapeSequence_CodePoints.end())
			return slash + 2;
		if (IsAsciiOctalDigit(escaped))
		{
			size_t i = slash + 2;
			for (int digits = 1; digits < 3 && i < source.size() &&
				IsAsciiOctalDigit(source[i].code_point); ++digits, ++i)
			{}
			return i;
		}
		if (escaped == 'x')
		{
			size_t i = slash + 2;
			if (i >= source.size() || !IsHexDigit(source[i].code_point))
				throw logic_error("invalid hex escape sequence");
			while (i < source.size() && IsHexDigit(source[i].code_point))
				++i;
			return i;
		}
		throw logic_error("invalid escape sequence");
	}

	LexedToken ParseQuotedLiteral(size_t quote, bool character)
	{
		size_t i = quote + 1;
		bool has_content = false;
		for (; i < source.size();)
		{
			const int code_point = source[i].code_point;
			if (code_point == '\n')
				throw logic_error(character ?
					"unterminated character literal" :
					"unterminated string literal");
			if (code_point == '\\')
			{
				i = ValidateEscape(i);
				has_content = true;
				continue;
			}
			if (code_point == (character ? '\'' : '"'))
			{
				if (character && !has_content)
					throw logic_error("empty character literal");
				++i;
				const size_t suffix_end = ConsumeIdentifierSuffix(i);
				if (suffix_end != i)
				{
					return LexedToken(
						character ? PP_USER_DEFINED_CHARACTER_LITERAL :
						PP_USER_DEFINED_STRING_LITERAL,
						suffix_end,
						EncodeUnits(source, position, suffix_end));
				}
				return MakeToken(character ? PP_CHARACTER_LITERAL :
					PP_STRING_LITERAL, i);
			}
			++i;
			has_content = true;
		}
		throw logic_error(character ? "unterminated character literal" :
			"unterminated string literal");
	}

	LexedToken ParseRawLiteral(int prefix_length)
	{
		const size_t quote = position + static_cast<size_t>(prefix_length);
		const size_t delimiter_begin = quote + 1;
		size_t delimiter_length = 0;
		size_t open_paren = delimiter_begin;
		for (; open_paren < source.size(); ++open_paren)
		{
			if (source[open_paren].code_point == '(')
				break;
			if (source[open_paren].code_point == '\n' ||
				!IsRawDelimiterCodePoint(source[open_paren].code_point))
				return LexedToken(PP_NON_WHITESPACE_CHARACTER,
					position + 1, EncodeUnits(source, position, position + 1));
			++delimiter_length;
			if (delimiter_length > 16)
				throw logic_error("raw string delimiter too long");
		}
		if (open_paren == source.size())
			return LexedToken(PP_NON_WHITESPACE_CHARACTER,
				position + 1, EncodeUnits(source, position, position + 1));

		for (size_t close = open_paren + 1; close < source.size(); ++close)
		{
			if (source[close].code_point != ')')
				continue;
			const size_t delimiter_end = close + 1 + delimiter_length;
			if (delimiter_end >= source.size())
				continue;
			bool matches = true;
			for (size_t j = 0; j < delimiter_length; ++j)
			{
				if (source[close + 1 + j].code_point !=
					source[delimiter_begin + j].code_point)
				{
					matches = false;
					break;
				}
			}
			if (!matches || source[delimiter_end].code_point != '"')
				continue;

			size_t end = delimiter_end + 1;
			const size_t suffix_end = ConsumeIdentifierSuffix(end);
			if (suffix_end != end)
				return LexedToken(PP_USER_DEFINED_STRING_LITERAL,
					suffix_end, EncodeUnits(source, position, suffix_end));
			return MakeToken(PP_STRING_LITERAL, end);
		}
		throw logic_error("unterminated raw string literal");
	}

	LexedToken ParseIdentifier()
	{
		size_t i = position + 1;
		while (i < source.size() && IsIdentifierBody(source[i].code_point))
			++i;
		LexedToken result = MakeToken(PP_IDENTIFIER, i);
		if (Digraph_IdentifierLike_Operators.find(result.data) !=
			Digraph_IdentifierLike_Operators.end())
			result.kind = PP_PREPROCESSING_OP_OR_PUNC;
		return result;
	}

	LexedToken ParsePPNumber()
	{
		size_t i = position;
		if (source[i].code_point == '.')
			i += 2;
		else
			++i;

		while (i < source.size())
		{
			const int code_point = source[i].code_point;
			if (IsAsciiDigit(code_point) || IsIdentifierNondigit(code_point))
			{
				if ((code_point == 'e' || code_point == 'E') &&
					i + 1 < source.size() &&
					(source[i + 1].code_point == '+' ||
					 source[i + 1].code_point == '-'))
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
		return MakeToken(PP_NUMBER, i);
	}

	LexedToken ParsePunctuator()
	{
		if (source[position].code_point == '<' &&
			position + 2 < source.size() &&
			source[position + 1].code_point == ':' &&
			source[position + 2].code_point == ':')
		{
			if (position + 3 >= source.size() ||
				(source[position + 3].code_point != ':' &&
				 source[position + 3].code_point != '>'))
				return MakeToken(PP_PREPROCESSING_OP_OR_PUNC, position + 1);
		}

		static const char* const punctuators[] =
		{
			"%:%:", "->*", "<<=", ">>=", "...",
			"##", "<:", ":>", "<%", "%>", "%:",
			".*", "::", "+=", "-=", "*=", "/=", "%=", "^=",
			"&=", "|=", "<<", ">>", "<=", ">=", "&&", "==",
			"!=", "||", "++", "--", "->",
			"{", "}", "[", "]", "#", "(", ")", ";", ":", "?",
			".", "+", "-", "*", "/", "%", "^", "&", "|", "~",
			"!", "=", "<", ">", ","
		};
		for (size_t i = 0; i < sizeof(punctuators) / sizeof(punctuators[0]); ++i)
		{
			const string punctuator = punctuators[i];
			if (Matches(position, punctuator))
				return MakeToken(PP_PREPROCESSING_OP_OR_PUNC,
					position + punctuator.size());
		}
		return MakeToken(PP_NON_WHITESPACE_CHARACTER, position + 1);
	}

	LexedToken ParseHeaderName()
	{
		const int opener = source[position].code_point;
		const int closer = opener == '<' ? '>' : '"';
		size_t i = position + 1;
		for (; i < source.size(); ++i)
		{
			if (source[i].code_point == '\n')
				throw logic_error("unterminated header name");
			if (source[i].code_point == closer)
				return LexedToken(PP_PREPROCESSING_OP_OR_PUNC,
					i + 1, EncodeUnits(source, position, i + 1));
		}
		throw logic_error("unterminated header name");
	}

	void EmitToken(const LexedToken& token)
	{
		switch (token.kind)
		{
		case PP_IDENTIFIER: output.emit_identifier(token.data); break;
		case PP_NUMBER: output.emit_pp_number(token.data); break;
		case PP_CHARACTER_LITERAL: output.emit_character_literal(token.data); break;
		case PP_USER_DEFINED_CHARACTER_LITERAL:
			output.emit_user_defined_character_literal(token.data);
			break;
		case PP_STRING_LITERAL: output.emit_string_literal(token.data); break;
		case PP_USER_DEFINED_STRING_LITERAL:
			output.emit_user_defined_string_literal(token.data);
			break;
		case PP_PREPROCESSING_OP_OR_PUNC:
			output.emit_preprocessing_op_or_punc(token.data);
			break;
		case PP_NON_WHITESPACE_CHARACTER:
			output.emit_non_whitespace_char(token.data);
			break;
		}
	}

	void EmitWhitespace()
	{
		output.emit_whitespace_sequence();
	}

	void ConsumeWhitespaceOrComments()
	{
		bool found = false;
		while (position < source.size())
		{
			if (source[position].raw)
				break;
			if (IsSourceWhitespace(source[position].code_point))
			{
				found = true;
				++position;
				continue;
			}
			if (source[position].code_point == '/' &&
				position + 1 < source.size() &&
				!source[position + 1].raw &&
				source[position + 1].code_point == '/')
			{
				found = true;
				position += 2;
				while (position < source.size() &&
					source[position].code_point != '\n')
					++position;
				continue;
			}
			if (source[position].code_point == '/' &&
				position + 1 < source.size() &&
				!source[position + 1].raw &&
				source[position + 1].code_point == '*')
			{
				found = true;
				position += 2;
				bool closed = false;
				while (position + 1 < source.size())
				{
					if (source[position].code_point == '*' &&
						source[position + 1].code_point == '/')
					{
						position += 2;
						closed = true;
						break;
					}
					++position;
				}
				if (!closed)
					throw logic_error("unterminated comment");
				continue;
			}
			break;
		}
		if (found)
			EmitWhitespace();
	}

	void Tokenize()
	{
		while (position < source.size())
		{
			if (!source[position].raw &&
				(IsSourceWhitespace(source[position].code_point) ||
				 source[position].code_point == '/' ||
				 source[position].code_point == '*'))
			{
				const size_t before = position;
				ConsumeWhitespaceOrComments();
				if (position != before)
					continue;
			}

			if (!source[position].raw && source[position].code_point == '\n')
			{
				output.emit_new_line();
				++position;
				at_line_start = true;
				expecting_directive_name = false;
				after_include = false;
				continue;
			}

			if (after_include &&
				(source[position].code_point == '<' ||
				 source[position].code_point == '"'))
			{
				LexedToken header = ParseHeaderName();
				EmitTokenAsHeader(header);
				position = header.end;
				after_include = false;
				at_line_start = false;
				continue;
			}

			LexedToken token = ParseNextToken();
			const bool was_line_start = at_line_start;
			at_line_start = false;
			if (was_line_start)
			{
				if (token.kind == PP_PREPROCESSING_OP_OR_PUNC &&
					(token.data == "#" || token.data == "%:"))
				{
					expecting_directive_name = true;
					after_include = false;
				}
				else
				{
					expecting_directive_name = false;
					after_include = false;
				}
			}
			else if (expecting_directive_name)
			{
				expecting_directive_name = false;
				after_include = token.kind == PP_IDENTIFIER &&
					token.data == "include";
			}
			else if (after_include)
			{
				after_include = false;
			}

			EmitToken(token);
			position = token.end;
		}
		output.emit_eof();
	}

	LexedToken ParseNextToken()
	{
		const int raw_prefix = RawPrefixLengthUnits(position);
		if (raw_prefix >= 0)
		{
			const size_t quote = position + static_cast<size_t>(raw_prefix);
			if (quote < source.size() && source[quote].code_point == '"')
			{
				const size_t open = FindRawOpening(quote);
				if (open != source.size())
					return ParseRawLiteral(raw_prefix);
			}
		}

		const size_t string_quote = OrdinaryStringQuoteUnits(position);
		if (string_quote < source.size())
			return ParseQuotedLiteral(string_quote, false);
		const size_t character_quote = OrdinaryCharacterQuoteUnits(position);
		if (character_quote < source.size())
			return ParseQuotedLiteral(character_quote, true);

		if (IsIdentifierStart(source[position].code_point))
			return ParseIdentifier();
		if (IsAsciiDigit(source[position].code_point) ||
			(source[position].code_point == '.' &&
			 position + 1 < source.size() &&
			 IsAsciiDigit(source[position + 1].code_point)))
			return ParsePPNumber();
	if (source[position].code_point == '\'' ||
			source[position].code_point == '"')
			throw logic_error("invalid quoted preprocessing token");
		return ParsePunctuator();
	}

	int RawPrefixLengthUnits(size_t position) const
	{
		if (position >= source.size())
			return -1;
		if (source[position].code_point == 'R' &&
			position + 1 < source.size() &&
			source[position + 1].code_point == '"')
			return 1;
		if (source[position].code_point == 'u' &&
			position + 2 < source.size() &&
			source[position + 1].code_point == 'R' &&
			source[position + 2].code_point == '"')
			return 2;
		if (source[position].code_point == 'U' &&
			position + 2 < source.size() &&
			source[position + 1].code_point == 'R' &&
			source[position + 2].code_point == '"')
			return 2;
		if (source[position].code_point == 'L' &&
			position + 2 < source.size() &&
			source[position + 1].code_point == 'R' &&
			source[position + 2].code_point == '"')
			return 2;
		if (source[position].code_point == 'u' &&
			position + 3 < source.size() &&
			source[position + 1].code_point == '8' &&
			source[position + 2].code_point == 'R' &&
			source[position + 3].code_point == '"')
			return 3;
		return -1;
	}

	size_t FindRawOpening(size_t quote) const
	{
		size_t delimiter_length = 0;
		for (size_t i = quote + 1; i < source.size(); ++i)
		{
			if (source[i].code_point == '(')
				return i;
			if (source[i].code_point == '\n' ||
				!IsRawDelimiterCodePoint(source[i].code_point))
				return source.size();
			++delimiter_length;
			if (delimiter_length > 16)
				throw logic_error("raw string delimiter too long");
		}
		return source.size();
	}

	size_t OrdinaryStringQuoteUnits(size_t position) const
	{
		if (position >= source.size())
			return source.size();
		if (source[position].code_point == '"')
			return position;
		if (source[position].code_point == 'u' &&
			position + 1 < source.size() &&
			source[position + 1].code_point == '"')
			return position + 1;
		if (source[position].code_point == 'U' &&
			position + 1 < source.size() &&
			source[position + 1].code_point == '"')
			return position + 1;
		if (source[position].code_point == 'L' &&
			position + 1 < source.size() &&
			source[position + 1].code_point == '"')
			return position + 1;
		if (source[position].code_point == 'u' &&
			position + 2 < source.size() &&
			source[position + 1].code_point == '8' &&
			source[position + 2].code_point == '"')
			return position + 2;
		return source.size();
	}

	size_t OrdinaryCharacterQuoteUnits(size_t position) const
	{
		if (position >= source.size())
			return source.size();
		if (source[position].code_point == '\'')
			return position;
		if ((source[position].code_point == 'u' ||
			source[position].code_point == 'U' ||
			source[position].code_point == 'L') &&
			position + 1 < source.size() &&
			source[position + 1].code_point == '\'')
			return position + 1;
		return source.size();
	}

	void EmitTokenAsHeader(const LexedToken& token)
	{
		output.emit_header_name(token.data);
	}
};

bool HasBatchStdinArg(int argc, char** argv)
{
	for (int i = 1; i < argc; i++)
	{
		if (string(argv[i]) == "--batch-stdin")
			return true;
	}
	return false;
}

int RunNotImplementedBatchMode()
{
	string line;
	while (getline(cin, line))
	{
		(void)line;
		cout << "EXIT_NOT_IMPLEMENTED" << endl;
	}
	return EXIT_SUCCESS;
}

int main(int argc, char** argv)
{
	try
	{
		if (HasBatchStdinArg(argc, argv))
			return RunNotImplementedBatchMode();

		ostringstream oss;
		oss << cin.rdbuf();

		string input = oss.str();

		DebugPPTokenStream output;

		PPTokenizer tokenizer(output);

		for (char c : input)
		{
			unsigned char code_unit = c;
			tokenizer.process(code_unit);
		}

		tokenizer.process(EndOfFile);

		return EXIT_SUCCESS;
	}
	catch (const NotImplementedException& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return CPPGM_EXIT_NOT_IMPLEMENTED;
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
