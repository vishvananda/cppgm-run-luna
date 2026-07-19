#include "posttoken_lexer.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include "posttoken_unicode.h"
#include "pptoken_translation.h"

using namespace std;

namespace {

typedef SourceUnit Unit;

string Encode(const vector<Unit>& units, size_t begin, size_t end)
{
	return EncodeUnits(units, begin, end);
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
	if (end < units.size() && IsIdentifierStart(units[end].code_point))
	{
		++end;
		while (end < units.size() && IsIdentifierBody(units[end].code_point)) ++end;
	}
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
		if (!IsRawDelimiterCodePoint(units[open].code_point))
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
			if (suffix < units.size() && IsIdentifierStart(units[suffix].code_point))
			{
				++suffix;
				while (suffix < units.size() && IsIdentifierBody(units[suffix].code_point)) ++suffix;
			}
			return PostPPToken(suffix == end ? POST_PP_STRING : POST_PP_USER_STRING,
				Encode(units, position, suffix));
		}
	}
	throw logic_error("unterminated raw string");
}

bool HasRawOpening(const vector<Unit>& units, size_t quote)
{
	size_t delimiter_length = 0;
	for (size_t open = quote + 1; open < units.size(); ++open)
	{
		if (units[open].code_point == '(') return true;
		if (!IsRawDelimiterCodePoint(units[open].code_point)) return false;
		if (++delimiter_length > 16) throw logic_error("raw delimiter too long");
	}
	return false;
}

PostPPToken ParseNumber(const vector<Unit>& units, size_t position)
{
	size_t end = position + (units[position].code_point == '.' ? 2 : 1);
	while (end < units.size())
	{
		const int c = units[end].code_point;
		if (IsAsciiDigit(c) || IsIdentifierNondigit(c))
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
	if (units[position].code_point == '<' &&
		position + 2 < units.size() &&
		units[position + 1].code_point == ':' &&
		units[position + 2].code_point == ':' &&
		(position + 3 >= units.size() ||
			(units[position + 3].code_point != ':' &&
			 units[position + 3].code_point != '>')))
		return PostPPToken(POST_PP_PUNCTUATOR,
			Encode(units, position, position + 1));

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
	Lexer(const string& input) : units(TranslateSource(input)), position(0),
		line_start(true), expecting_directive_name(false), after_include(false) {}
	vector<PostPPToken> Run()
	{
		vector<PostPPToken> result;
		while (position < units.size())
		{
			SkipSpace();
			if (position >= units.size()) break;
			if (units[position].code_point == '\n')
			{
				++position;
				line_start = true;
				expecting_directive_name = false;
				after_include = false;
				continue;
			}
			if (after_include && (units[position].code_point == '<' || units[position].code_point == '"'))
			{
				result.push_back(ParseHeader());
				after_include = false;
				continue;
			}
			const bool was_line_start = line_start;
			PostPPToken token = Next();
			line_start = false;
			if (was_line_start)
			{
				if (token.kind == POST_PP_PUNCTUATOR &&
					(token.source == "#" || token.source == "%:"))
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
				after_include = token.kind == POST_PP_IDENTIFIER &&
					token.source == "include";
			}
			else if (after_include)
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
	bool expecting_directive_name;
	bool after_include;

	void SkipSpace()
	{
		while (position < units.size())
		{
			if (IsSourceWhitespace(units[position].code_point)) { ++position; continue; }
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
			if (raw >= 0 && HasRawOpening(units,
				position + static_cast<size_t>(raw)))
				return Advance(ParseRaw(units, position, raw));
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
