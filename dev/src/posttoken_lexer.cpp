#include "posttoken_lexer.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include "posttoken_unicode.h"

using namespace std;

namespace {

struct Unit
{
	int code_point;
	bool raw;
	Unit(int code_point = 0, bool raw = false)
		: code_point(code_point), raw(raw) {}
};

bool IsAsciiDigit(int c) { return c >= '0' && c <= '9'; }
bool IsAsciiOctalDigit(int c) { return c >= '0' && c <= '7'; }
bool IsHexDigit(int c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
		(c >= 'A' && c <= 'F');
}
int HexValue(int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return c - 'A' + 10;
}
bool IsIdentifierStart(int c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		c == '_' || c >= 0x80;
}
bool IsIdentifierBody(int c) { return IsIdentifierStart(c) || IsAsciiDigit(c); }
bool IsWhitespace(int c)
{
	return c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r';
}
bool IsNondigit(int c) { return IsIdentifierStart(c); }

int RawPrefixLength(const vector<int>& source, size_t position)
{
	if (position >= source.size()) return -1;
	if (source[position] == 'R' && position + 1 < source.size() && source[position + 1] == '"') return 1;
	if ((source[position] == 'u' || source[position] == 'U' || source[position] == 'L') && position + 2 < source.size() && source[position + 1] == 'R' && source[position + 2] == '"') return 2;
	if (source[position] == 'u' && position + 3 < source.size() && source[position + 1] == '8' && source[position + 2] == 'R' && source[position + 3] == '"') return 3;
	return -1;
}

vector<bool> MarkRaw(const vector<int>& source)
{
	vector<bool> result(source.size(), false);
	for (size_t i = 0; i < source.size(); ++i)
	{
		const int prefix = RawPrefixLength(source, i);
		if (prefix < 0 || (i != 0 && IsIdentifierBody(source[i - 1]))) continue;
		const size_t quote = i + static_cast<size_t>(prefix);
		size_t open = quote + 1;
		while (open < source.size() && source[open] != '(')
		{
			if (source[open] == '\n' || source[open] == ' ' || source[open] == '\t' || source[open] == ')' || source[open] == '\\' || open - quote - 1 >= 16)
				break;
			++open;
		}
		if (open == source.size() || source[open] != '(') continue;
		const size_t delimiter = open - quote - 1;
		for (size_t close = open + 1; close < source.size(); ++close)
		{
			if (source[close] != ')') continue;
			const size_t end = close + delimiter + 1;
			if (end >= source.size() || source[end] != '"') continue;
			bool match = true;
			for (size_t j = 0; j < delimiter; ++j)
				if (source[close + 1 + j] != source[quote + 1 + j]) match = false;
			if (!match) continue;
			for (size_t j = i; j <= end; ++j) result[j] = true;
			i = end;
			break;
		}
	}
	return result;
}

string Encode(const vector<Unit>& units, size_t begin, size_t end)
{
	string result;
	for (size_t i = begin; i < end; ++i)
		result += PostEncodeUTF8(units[i].code_point);
	return result;
}

int Trigraph(int a, int b, int c)
{
	if (a != '?' || b != '?') return -1;
	switch (c)
	{
	case '=': return '#'; case '/': return '\\'; case '\'': return '^';
	case '(': return '['; case ')': return ']'; case '!': return '|';
	case '<': return '{'; case '>': return '}'; case '-': return '~';
	default: return -1;
	}
}

vector<Unit> Translate(const string& input)
{
	vector<int> decoded = PostDecodeUTF8(input);
	if (!decoded.empty() && decoded[0] == 0xfeff) decoded.erase(decoded.begin());
	const vector<bool> raw = MarkRaw(decoded);
	vector<Unit> units;
	for (size_t i = 0; i < decoded.size(); ++i)
		units.push_back(Unit(decoded[i], raw[i]));

	vector<Unit> translated;
	for (size_t i = 0; i < units.size();)
	{
		if (!units[i].raw && i + 2 < units.size() &&
			!units[i + 1].raw && !units[i + 2].raw)
		{
			const int replacement = Trigraph(units[i].code_point,
				units[i + 1].code_point, units[i + 2].code_point);
			if (replacement >= 0)
			{
				translated.push_back(Unit(replacement));
				i += 3;
				continue;
			}
		}
		translated.push_back(units[i++]);
	}
	units.clear();
	for (size_t i = 0; i < translated.size(); ++i)
	{
		if (!translated[i].raw && translated[i].code_point == '\\' &&
			i + 1 < translated.size() && !translated[i + 1].raw &&
			translated[i + 1].code_point == '\n')
		{
			++i;
			continue;
		}
		units.push_back(translated[i]);
	}
	translated.clear();
	for (size_t i = 0; i < units.size();)
	{
		if (!units[i].raw && units[i].code_point == '\\' &&
			i + 1 < units.size() && !units[i + 1].raw &&
			(units[i + 1].code_point == 'u' || units[i + 1].code_point == 'U'))
		{
			const size_t digits = units[i + 1].code_point == 'u' ? 4 : 8;
			if (i + 2 + digits > units.size())
				throw logic_error("truncated universal-character-name");
			int value = 0;
			for (size_t j = 0; j < digits; ++j)
			{
				if (units[i + 2 + j].raw || !IsHexDigit(units[i + 2 + j].code_point))
					throw logic_error("invalid universal-character-name");
				value = (value << 4) | HexValue(units[i + 2 + j].code_point);
			}
			if (value == 0 || !PostIsValidCodePoint(value))
				throw logic_error("invalid universal-character-name");
			translated.push_back(Unit(value));
			i += 2 + digits;
			continue;
		}
		translated.push_back(units[i++]);
	}
	if (!translated.empty() && translated.back().code_point != '\n')
		translated.push_back(Unit('\n'));
	return translated;
}

int RawPrefix(const vector<Unit>& units, size_t position)
{
	if (position >= units.size()) return -1;
	if (units[position].code_point == 'R' && position + 1 < units.size() &&
		units[position + 1].code_point == '"') return 1;
	if ((units[position].code_point == 'u' || units[position].code_point == 'U' ||
		units[position].code_point == 'L') && position + 2 < units.size() &&
		units[position + 1].code_point == 'R' && units[position + 2].code_point == '"') return 2;
	if (units[position].code_point == 'u' && position + 3 < units.size() &&
		units[position + 1].code_point == '8' && units[position + 2].code_point == 'R' &&
		units[position + 3].code_point == '"') return 3;
	return -1;
}

int StringQuote(const vector<Unit>& units, size_t position)
{
	if (position >= units.size()) return -1;
	if (units[position].code_point == '"') return 0;
	if ((units[position].code_point == 'u' || units[position].code_point == 'U' ||
		units[position].code_point == 'L') && position + 1 < units.size() &&
		units[position + 1].code_point == '"') return 1;
	if (units[position].code_point == 'u' && position + 2 < units.size() &&
		units[position + 1].code_point == '8' && units[position + 2].code_point == '"') return 2;
	return -1;
}

int CharacterQuote(const vector<Unit>& units, size_t position)
{
	if (position >= units.size()) return -1;
	if (units[position].code_point == '\'') return 0;
	if ((units[position].code_point == 'u' || units[position].code_point == 'U' ||
		units[position].code_point == 'L') && position + 1 < units.size() &&
		units[position + 1].code_point == '\'') return 1;
	return -1;
}

size_t ValidateEscape(const vector<Unit>& units, size_t slash)
{
	if (slash + 1 >= units.size()) throw logic_error("unterminated escape");
	const int next = units[slash + 1].code_point;
	if (string("'\"?\\abfnrtv").find(static_cast<char>(next)) != string::npos)
		return slash + 2;
	if (IsAsciiOctalDigit(next))
	{
		size_t end = slash + 2;
		while (end < units.size() && end < slash + 4 && IsAsciiOctalDigit(units[end].code_point)) ++end;
		return end;
	}
	if (next == 'x')
	{
		size_t end = slash + 2;
		if (end >= units.size() || !IsHexDigit(units[end].code_point))
			throw logic_error("invalid hex escape");
		while (end < units.size() && IsHexDigit(units[end].code_point)) ++end;
		return end;
	}
	throw logic_error("invalid escape");
}

PostPPToken ParseQuoted(const vector<Unit>& units, size_t position,
	size_t quote, bool character)
{
	size_t end = quote + 1;
	for (;;)
	{
		if (end >= units.size() || units[end].code_point == '\n')
			throw logic_error("unterminated quoted literal");
		if (units[end].code_point == '\\') { end = ValidateEscape(units, end); continue; }
		if (units[end].code_point == (character ? '\'' : '"')) break;
		++end;
	}
	++end;
	const size_t suffix_begin = end;
	while (end < units.size() && IsIdentifierBody(units[end].code_point)) ++end;
	const PostPPTokenKind kind = character ?
		(suffix_begin == end ? POST_PP_CHARACTER : POST_PP_USER_CHARACTER) :
		(suffix_begin == end ? POST_PP_STRING : POST_PP_USER_STRING);
	return PostPPToken(kind, Encode(units, position, end));
}

PostPPToken ParseRaw(const vector<Unit>& units, size_t position, int prefix)
{
	const size_t quote = position + static_cast<size_t>(prefix);
	const size_t delimiter_begin = quote + 1;
	size_t open = delimiter_begin;
	while (open < units.size() && units[open].code_point != '(')
	{
		if (units[open].code_point == '\n' || units[open].code_point == ')' ||
			units[open].code_point == '\\' || units[open].code_point == ' ' ||
			units[open].code_point == '\t')
			throw logic_error("invalid raw string delimiter");
		if (open - delimiter_begin >= 16) throw logic_error("raw delimiter too long");
		++open;
	}
	if (open == units.size()) throw logic_error("unterminated raw string");
	const size_t delimiter_length = open - delimiter_begin;
	for (size_t close = open + 1; close < units.size(); ++close)
	{
		if (units[close].code_point != ')') continue;
		const size_t end_delimiter = close + 1 + delimiter_length;
		if (end_delimiter >= units.size() || units[end_delimiter].code_point != '"') continue;
		bool match = true;
		for (size_t j = 0; j < delimiter_length; ++j)
			if (units[close + 1 + j].code_point != units[delimiter_begin + j].code_point) match = false;
		if (match) {
			const size_t end = end_delimiter + 1;
			size_t suffix = end;
			while (suffix < units.size() && IsIdentifierBody(units[suffix].code_point)) ++suffix;
			return PostPPToken(suffix == end ? POST_PP_STRING : POST_PP_USER_STRING,
				Encode(units, position, suffix));
		}
	}
	throw logic_error("unterminated raw string");
}

PostPPToken ParseNumber(const vector<Unit>& units, size_t position)
{
	size_t end = position + (units[position].code_point == '.' ? 2 : 1);
	while (end < units.size())
	{
		const int c = units[end].code_point;
		if (IsAsciiDigit(c) || IsNondigit(c))
		{
			if ((c == 'e' || c == 'E' || c == 'p' || c == 'P') && end + 1 < units.size() &&
				(units[end + 1].code_point == '+' || units[end + 1].code_point == '-')) end += 2;
			else ++end;
		}
		else if (c == '.') ++end;
		else break;
	}
	return PostPPToken(POST_PP_NUMBER, Encode(units, position, end));
}

PostPPToken ParsePunctuator(const vector<Unit>& units, size_t position)
{
	static const char* const punctuators[] = {
		"%:%:", "->*", "<<=", ">>=", "...", "##", "<:", ":>", "<%", "%>", "%:",
		".*", "::", "+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=", "<<", ">>", "<=", ">=", "&&", "==", "!=", "||", "++", "--", "->",
		"{", "}", "[", "]", "#", "(", ")", ";", ":", "?", ".", "+", "-", "*", "/", "%", "^", "&", "|", "~", "!", "=", "<", ">", ","
	};
	for (size_t p = 0; p < sizeof(punctuators) / sizeof(*punctuators); ++p)
	{
		const string text = punctuators[p];
		bool match = true;
		for (size_t j = 0; j < text.size(); ++j)
			if (position + j >= units.size() || units[position + j].code_point != text[j]) match = false;
		if (match) return PostPPToken(POST_PP_PUNCTUATOR, Encode(units, position, position + text.size()));
	}
	return PostPPToken(POST_PP_NON_WHITESPACE, Encode(units, position, position + 1));
}

class Lexer
{
public:
	Lexer(const string& input) : units(Translate(input)), position(0), line_start(true), after_include(false) {}
	vector<PostPPToken> Run()
	{
		vector<PostPPToken> result;
		while (position < units.size())
		{
			SkipSpace();
			if (position >= units.size()) break;
			if (units[position].code_point == '\n') { ++position; line_start = true; after_include = false; continue; }
			if (after_include && (units[position].code_point == '<' || units[position].code_point == '"'))
			{
				result.push_back(ParseHeader());
				after_include = false;
				continue;
			}
			const bool was_line_start = line_start;
			PostPPToken token = Next();
			line_start = false;
			if (was_line_start && (token.source == "#" || token.source == "%:"))
				after_include = false;
			else if (token.source == "include" && was_line_start == false)
				after_include = true;
			else if (token.kind != POST_PP_PUNCTUATOR)
				after_include = false;
			result.push_back(token);
			if (token.kind == POST_PP_EOF) break;
		}
		result.push_back(PostPPToken(POST_PP_EOF));
		return result;
	}

private:
	vector<Unit> units;
	size_t position;
	bool line_start;
	bool after_include;

	void SkipSpace()
	{
		while (position < units.size())
		{
			if (IsWhitespace(units[position].code_point)) { ++position; continue; }
			if (!units[position].raw && units[position].code_point == '/' && position + 1 < units.size() &&
				units[position + 1].code_point == '/')
			{
				position += 2;
				while (position < units.size() && units[position].code_point != '\n') ++position;
				continue;
			}
			if (!units[position].raw && units[position].code_point == '/' && position + 1 < units.size() &&
				units[position + 1].code_point == '*')
			{
				position += 2;
				while (position + 1 < units.size() && !(units[position].code_point == '*' && units[position + 1].code_point == '/')) ++position;
				if (position + 1 >= units.size()) throw logic_error("unterminated comment");
				position += 2;
				continue;
			}
			break;
		}
	}

	PostPPToken ParseHeader()
	{
		const int opener = units[position].code_point;
		const int closer = opener == '<' ? '>' : '"';
		const size_t begin = position++;
		while (position < units.size() && units[position].code_point != closer && units[position].code_point != '\n') ++position;
		if (position >= units.size() || units[position].code_point != closer) throw logic_error("unterminated header");
		++position;
		return PostPPToken(POST_PP_HEADER, Encode(units, begin, position));
	}

		PostPPToken Next()
		{
			const int raw = RawPrefix(units, position);
			if (raw >= 0) return Advance(ParseRaw(units, position, raw));
			const int quote = StringQuote(units, position);
			if (quote >= 0) return Advance(ParseQuoted(units, position, position + static_cast<size_t>(quote), false));
			const int character = CharacterQuote(units, position);
			if (character >= 0) return Advance(ParseQuoted(units, position, position + static_cast<size_t>(character), true));
			if (IsIdentifierStart(units[position].code_point))
		{
				size_t end = position + 1;
				while (end < units.size() && IsIdentifierBody(units[end].code_point)) ++end;
				return Advance(PostPPToken(POST_PP_IDENTIFIER, Encode(units, position, end)));
			}
			if (IsAsciiDigit(units[position].code_point) || (units[position].code_point == '.' && position + 1 < units.size() && IsAsciiDigit(units[position + 1].code_point)))
				return Advance(ParseNumber(units, position));
			return Advance(ParsePunctuator(units, position));
		}

	PostPPToken Advance(const PostPPToken& token)
	{
		position += PostDecodeUTF8(token.source).size();
		return token;
	}
	};

} // namespace

vector<PostPPToken> LexPostPPSource(const string& input)
{
	return Lexer(input).Run();
}
