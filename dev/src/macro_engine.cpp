#include "macro_engine.h"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "posttoken_lexer.h"
#include "posttoken_semantics.h"
#include "pptoken_translation.h"

using namespace std;

namespace {

enum TokenKind
{
	TOKEN_SPACE,
	TOKEN_NEWLINE,
	TOKEN_IDENTIFIER,
	TOKEN_NUMBER,
	TOKEN_CHARACTER,
	TOKEN_USER_CHARACTER,
	TOKEN_STRING,
	TOKEN_USER_STRING,
	TOKEN_PUNCTUATOR,
	TOKEN_HEADER,
	TOKEN_NON_WHITESPACE,
	TOKEN_PASTE,
	TOKEN_PLACEMARKER
};

struct Token
{
	TokenKind kind;
	string text;
	set<string> blocked;
	bool from_argument;
	bool from_replacement;

	Token(TokenKind kind = TOKEN_NON_WHITESPACE,
		const string& text = string())
		: kind(kind), text(text), from_argument(false), from_replacement(false)
	{}
};

bool IsSpace(const Token& token)
{
	return token.kind == TOKEN_SPACE || token.kind == TOKEN_NEWLINE;
}

bool IsIdentifier(const Token& token)
{
	return token.kind == TOKEN_IDENTIFIER;
}

bool IsPunct(const Token& token, const string& text)
{
	return token.kind == TOKEN_PUNCTUATOR && token.text == text;
}

bool IsHash(const Token& token)
{
	return IsPunct(token, "#") || IsPunct(token, "%:");
}

bool IsPaste(const Token& token)
{
	return token.kind == TOKEN_PASTE || IsPunct(token, "##") ||
		IsPunct(token, "%:%:");
}

size_t SkipSpaces(const vector<Token>& tokens, size_t position, size_t end)
{
	while (position < end && IsSpace(tokens[position]))
		++position;
	return position;
}

size_t PreviousNonSpace(const vector<Token>& tokens, size_t position)
{
	while (position > 0)
	{
		--position;
		if (!IsSpace(tokens[position]))
			return position;
	}
	return tokens.size();
}

size_t NextNonSpace(const vector<Token>& tokens, size_t position,
	size_t end)
{
	return SkipSpaces(tokens, position, end);
}

Token CopyToken(const Token& token)
{
	return token;
}

vector<Token> CollapseSpaces(const vector<Token>& input)
{
	vector<Token> result;
	bool pending_space = false;
	for (size_t i = 0; i < input.size(); ++i)
	{
		if (IsSpace(input[i]))
		{
			pending_space = true;
			continue;
		}
		if (pending_space)
			result.push_back(Token(TOKEN_SPACE, " "));
		pending_space = false;
		result.push_back(input[i]);
	}
	if (pending_space && !result.empty())
		result.push_back(Token(TOKEN_SPACE, " "));
	return result;
}

vector<Token> TrimAndCollapseSpaces(const vector<Token>& input)
{
	vector<Token> result = CollapseSpaces(input);
	while (!result.empty() && IsSpace(result.front()))
		result.erase(result.begin());
	while (!result.empty() && IsSpace(result.back()))
		result.pop_back();
	return result;
}

struct PhaseTokenizer
{
	vector<SourceUnit> source;
	size_t position;
	bool line_start;
	bool expecting_directive_name;
	bool after_include;

	PhaseTokenizer(const string& input)
		: source(TranslateSource(input)), position(0), line_start(true),
		  expecting_directive_name(false), after_include(false)
	{}

	vector<Token> Run()
	{
		vector<Token> result;
		while (position < source.size())
		{
			if (ConsumeWhitespace(&result))
				continue;
			if (source[position].code_point == '\n')
			{
				result.push_back(Token(TOKEN_NEWLINE, "\n"));
				++position;
				line_start = true;
				expecting_directive_name = false;
				after_include = false;
				continue;
			}
			if (after_include && IsHeaderOpener())
			{
				result.push_back(ParseHeader());
				after_include = false;
				line_start = false;
				continue;
			}
			const bool was_line_start = line_start;
			Token token = NextToken();
			UpdateDirectiveState(token, was_line_start);
			result.push_back(token);
		}
		return result;
	}

	bool ConsumeWhitespace(vector<Token>* result)
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
			if (StartsComment("//"))
			{
				found = true;
				position += 2;
				while (position < source.size() &&
					source[position].code_point != '\n')
					++position;
				continue;
			}
			if (StartsComment("/*"))
			{
				found = true;
				position += 2;
				ConsumeBlockComment();
				continue;
			}
			break;
		}
		if (found)
			result->push_back(Token(TOKEN_SPACE, " "));
		return found;
	}

	bool StartsComment(const string& text) const
	{
		if (position + text.size() > source.size())
			return false;
		for (size_t i = 0; i < text.size(); ++i)
		{
			if (source[position + i].raw ||
				source[position + i].code_point !=
				static_cast<unsigned char>(text[i]))
				return false;
		}
		return true;
	}

	void ConsumeBlockComment()
	{
		while (position + 1 < source.size())
		{
			if (!source[position].raw && !source[position + 1].raw &&
				source[position].code_point == '*' &&
				source[position + 1].code_point == '/')
			{
				position += 2;
				return;
			}
			++position;
		}
		throw logic_error("unterminated comment");
	}

	bool IsHeaderOpener() const
	{
		return source[position].code_point == '<' ||
			source[position].code_point == '"';
	}

	Token ParseHeader()
	{
		const int opener = source[position].code_point;
		const int closer = opener == '<' ? '>' : '"';
		const size_t begin = position++;
		while (position < source.size() &&
			source[position].code_point != closer &&
			source[position].code_point != '\n')
			++position;
		if (position >= source.size() ||
			source[position].code_point != closer)
			throw logic_error("unterminated header name");
		++position;
		return Token(TOKEN_HEADER, EncodeUnits(source, begin, position));
	}

	Token NextToken()
	{
		const int raw_prefix = RawPrefixLength();
		if (raw_prefix >= 0)
		{
			const size_t quote = position +
				static_cast<size_t>(raw_prefix);
			if (quote < source.size() && source[quote].code_point == '"' &&
				FindRawOpening(quote) < source.size())
				return ParseRaw(raw_prefix);
		}
		const size_t string_quote = StringQuote();
		if (string_quote < source.size())
			return ParseQuoted(string_quote, false);
		const size_t character_quote = CharacterQuote();
		if (character_quote < source.size())
			return ParseQuoted(character_quote, true);
		if (IsIdentifierStart(source[position].code_point))
			return ParseIdentifier();
		if (IsAsciiDigit(source[position].code_point) ||
			(source[position].code_point == '.' && position + 1 < source.size() &&
			 IsAsciiDigit(source[position + 1].code_point)))
			return ParseNumber();
		if (source[position].code_point == '\'' ||
			source[position].code_point == '"')
			throw logic_error("invalid quoted preprocessing token");
		return ParsePunctuator();
	}

	int RawPrefixLength() const
	{
		if (position >= source.size())
			return -1;
		if (source[position].code_point == 'R' && position + 1 < source.size() &&
			source[position + 1].code_point == '"')
			return 1;
		if ((source[position].code_point == 'u' ||
			source[position].code_point == 'U' ||
			source[position].code_point == 'L') && position + 2 < source.size() &&
			source[position + 1].code_point == 'R' &&
			source[position + 2].code_point == '"')
			return 2;
		if (source[position].code_point == 'u' && position + 3 < source.size() &&
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
			if (++delimiter_length > 16)
				throw logic_error("raw string delimiter too long");
		}
		return source.size();
	}

	size_t StringQuote() const
	{
		if (position >= source.size())
			return source.size();
		if (source[position].code_point == '"')
			return position;
		if ((source[position].code_point == 'u' ||
			source[position].code_point == 'U' ||
			source[position].code_point == 'L') && position + 1 < source.size() &&
			source[position + 1].code_point == '"')
			return position + 1;
		if (source[position].code_point == 'u' && position + 2 < source.size() &&
			source[position + 1].code_point == '8' &&
			source[position + 2].code_point == '"')
			return position + 2;
		return source.size();
	}

	size_t CharacterQuote() const
	{
		if (position >= source.size())
			return source.size();
		if (source[position].code_point == '\'')
			return position;
		if ((source[position].code_point == 'u' ||
			source[position].code_point == 'U' ||
			source[position].code_point == 'L') && position + 1 < source.size() &&
			source[position + 1].code_point == '\'')
			return position + 1;
		return source.size();
	}

	Token ParseIdentifier()
	{
		const size_t begin = position++;
		while (position < source.size() &&
			IsIdentifierBody(source[position].code_point))
			++position;
		const string text = EncodeUnits(source, begin, position);
		static const set<string> operator_names = {
			"new", "delete", "and", "and_eq", "bitand", "bitor", "compl",
			"not", "not_eq", "or", "or_eq", "xor", "xor_eq"
		};
		return Token(operator_names.find(text) == operator_names.end() ?
			TOKEN_IDENTIFIER : TOKEN_PUNCTUATOR, text);
	}

	Token ParseNumber()
	{
		const size_t begin = position;
		if (source[position].code_point == '.')
			position += 2;
		else
			++position;
		while (position < source.size())
		{
			const int c = source[position].code_point;
			if (IsAsciiDigit(c) || IsIdentifierNondigit(c))
			{
				if ((c == 'e' || c == 'E') && position + 1 < source.size() &&
					(source[position + 1].code_point == '+' ||
					 source[position + 1].code_point == '-'))
					position += 2;
				else
					++position;
				continue;
			}
			if (c == '.')
			{
				++position;
				continue;
			}
			break;
		}
		return Token(TOKEN_NUMBER, EncodeUnits(source, begin, position));
	}

	Token ParsePunctuator()
	{
		static const char* const punctuators[] = {
			"%:%:", "->*", "<<=", ">>=", "...", "##", "<:", ":>",
			"<%", "%>", "%:", ".*", "::", "+=", "-=", "*=", "/=", "%=",
			"^=", "&=", "|=", "<<", ">>", "<=", ">=", "&&", "==", "!=",
			"||", "++", "--", "->", "{", "}", "[", "]", "#", "(", ")",
			";", ":", "?", ".", "+", "-", "*", "/", "%", "^", "&", "|",
			"~", "!", "=", "<", ">", ","
		};
		for (size_t i = 0; i < sizeof(punctuators) / sizeof(*punctuators); ++i)
		{
			const string candidate = punctuators[i];
			if (Matches(candidate))
			{
				position += candidate.size();
				return Token(TOKEN_PUNCTUATOR, candidate);
			}
		}
		const string text = EncodeUnits(source, position, position + 1);
		++position;
		return Token(TOKEN_NON_WHITESPACE, text);
	}

	bool Matches(const string& text) const
	{
		if (position + text.size() > source.size())
			return false;
		for (size_t i = 0; i < text.size(); ++i)
			if (source[position + i].code_point !=
				static_cast<unsigned char>(text[i]))
				return false;
		return true;
	}

	Token ParseQuoted(size_t quote, bool character)
	{
		const size_t begin = position;
		size_t end = quote + 1;
		bool has_content = false;
		while (end < source.size())
		{
			const int c = source[end].code_point;
			if (c == '\n')
				throw logic_error("unterminated quoted literal");
			if (c == '\\')
			{
				end = ValidateEscape(end);
				has_content = true;
				continue;
			}
			if (c == (character ? '\'' : '"'))
			{
				if (character && !has_content)
					throw logic_error("empty character literal");
				++end;
				const size_t suffix = ConsumeSuffix(end);
				position = suffix;
				return Token(character ?
					(suffix == end ? TOKEN_CHARACTER : TOKEN_USER_CHARACTER) :
					(suffix == end ? TOKEN_STRING : TOKEN_USER_STRING),
					EncodeUnits(source, begin, suffix));
			}
			++end;
			has_content = true;
		}
		throw logic_error("unterminated quoted literal");
	}

	size_t ValidateEscape(size_t slash) const
	{
		if (slash + 1 >= source.size())
			throw logic_error("unterminated escape sequence");
		const int c = source[slash + 1].code_point;
		if (string("'\"?\\abfnrtv").find(static_cast<char>(c)) != string::npos)
			return slash + 2;
		if (IsAsciiOctalDigit(c))
		{
			size_t end = slash + 2;
			while (end < source.size() && end < slash + 4 &&
				IsAsciiOctalDigit(source[end].code_point))
				++end;
			return end;
		}
		if (c == 'x')
		{
			size_t end = slash + 2;
			if (end >= source.size() || !IsHexDigit(source[end].code_point))
				throw logic_error("invalid hex escape sequence");
			while (end < source.size() && IsHexDigit(source[end].code_point))
				++end;
			return end;
		}
		throw logic_error("invalid escape sequence");
	}

	size_t ConsumeSuffix(size_t begin) const
	{
		size_t end = begin;
		if (end < source.size() && IsIdentifierStart(source[end].code_point))
		{
			++end;
			while (end < source.size() && IsIdentifierBody(source[end].code_point))
				++end;
		}
		return end;
	}

	Token ParseRaw(int prefix_length)
	{
		const size_t begin = position;
		const size_t quote = position + static_cast<size_t>(prefix_length);
		const size_t delimiter_begin = quote + 1;
		size_t open = FindRawOpening(quote);
		if (open >= source.size())
			throw logic_error("unterminated raw string literal");
		const size_t delimiter_length = open - delimiter_begin;
		for (size_t close = open + 1; close < source.size(); ++close)
		{
			if (source[close].code_point != ')')
				continue;
			const size_t end_delimiter = close + 1 + delimiter_length;
			if (end_delimiter >= source.size() ||
				source[end_delimiter].code_point != '"')
				continue;
			bool matches = true;
			for (size_t i = 0; i < delimiter_length; ++i)
				if (source[close + 1 + i].code_point !=
					source[delimiter_begin + i].code_point)
					matches = false;
			if (!matches)
				continue;
			const size_t end = end_delimiter + 1;
			const size_t suffix = ConsumeSuffix(end);
			position = suffix;
			return Token(suffix == end ? TOKEN_STRING : TOKEN_USER_STRING,
				EncodeUnits(source, begin, suffix));
		}
		throw logic_error("unterminated raw string literal");
	}

	void UpdateDirectiveState(const Token& token, bool was_line_start)
	{
		line_start = false;
		if (was_line_start)
		{
			if (IsHash(token))
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
			after_include = IsIdentifier(token) && token.text == "include";
		}
	}
};

vector<Token> Tokenize(const string& source)
{
	return PhaseTokenizer(source).Run();
}

struct Macro
{
	string name;
	bool function_like;
	bool variadic;
	vector<string> parameters;
	vector<Token> replacement;

	Macro() : function_like(false), variadic(false) {}
};

struct Invocation
{
	vector<vector<Token> > arguments;
	bool empty_call;

	Invocation() : empty_call(false) {}
};

bool HasNonSpace(const vector<Token>& tokens)
{
	for (size_t i = 0; i < tokens.size(); ++i)
		if (!IsSpace(tokens[i]))
			return true;
	return false;
}

bool IsParameterName(const Macro& macro, const string& name)
{
	if (macro.variadic && name == "__VA_ARGS__")
		return true;
	return find(macro.parameters.begin(), macro.parameters.end(), name) !=
		macro.parameters.end();
}

bool EqualTokens(const vector<Token>& left, const vector<Token>& right)
{
	if (left.size() != right.size())
		return false;
	for (size_t i = 0; i < left.size(); ++i)
		if (left[i].kind != right[i].kind || left[i].text != right[i].text)
			return false;
	return true;
}

bool EqualMacros(const Macro& left, const Macro& right)
{
	return left.function_like == right.function_like &&
		left.variadic == right.variadic &&
		left.parameters == right.parameters &&
		EqualTokens(left.replacement, right.replacement);
}

void Require(bool condition, const string& message)
{
	if (!condition)
		throw logic_error(message);
}

size_t FindLineEnd(const vector<Token>& tokens, size_t begin)
{
	while (begin < tokens.size() && tokens[begin].kind != TOKEN_NEWLINE)
		++begin;
	return begin;
}

bool FindDirective(const vector<Token>& tokens, size_t begin, size_t end,
	string* name, size_t* hash_position)
{
	size_t hash = SkipSpaces(tokens, begin, end);
	if (hash >= end || !IsHash(tokens[hash]))
		return false;
	size_t directive = SkipSpaces(tokens, hash + 1, end);
	if (directive >= end || !IsIdentifier(tokens[directive]))
		return false;
	if (tokens[directive].text != "define" &&
		tokens[directive].text != "undef")
		return false;
	*name = tokens[directive].text;
	*hash_position = hash;
	return true;
}

void ParseParameters(const vector<Token>& tokens, size_t open, size_t end,
	vector<string>* parameters, bool* variadic, size_t* close)
{
	size_t position = SkipSpaces(tokens, open + 1, end);
	if (position < end && IsPunct(tokens[position], ")"))
	{
		*close = position;
		return;
	}
	if (position < end && IsPunct(tokens[position], "..."))
	{
		*variadic = true;
		position = SkipSpaces(tokens, position + 1, end);
		Require(position < end && IsPunct(tokens[position], ")"),
			"unterminated variadic macro parameter list");
		*close = position;
		return;
	}
	set<string> seen;
	for (;;)
	{
		Require(position < end && IsIdentifier(tokens[position]),
			"invalid macro parameter list");
		const string parameter = tokens[position].text;
		Require(parameter != "__VA_ARGS__", "invalid __VA_ARGS__ parameter");
		Require(seen.insert(parameter).second, "duplicate macro parameter");
		parameters->push_back(parameter);
		position = SkipSpaces(tokens, position + 1, end);
		if (position < end && IsPunct(tokens[position], ")"))
		{
			*close = position;
			return;
		}
		Require(position < end && IsPunct(tokens[position], ","),
			"invalid macro parameter separator");
		position = SkipSpaces(tokens, position + 1, end);
		if (position < end && IsPunct(tokens[position], "..."))
		{
			*variadic = true;
			position = SkipSpaces(tokens, position + 1, end);
			Require(position < end && IsPunct(tokens[position], ")"),
				"unterminated variadic macro parameter list");
			*close = position;
			return;
		}
	}
}

bool IsParameterToken(const Macro& macro, const Token& token)
{
	return IsIdentifier(token) && IsParameterName(macro, token.text);
}

bool ReplacementHashIsStringize(const Macro& macro,
	const vector<Token>& replacement, size_t position)
{
	if (!macro.function_like)
		return false;
	size_t next = NextNonSpace(replacement, position + 1, replacement.size());
	return next < replacement.size() && IsParameterToken(macro, replacement[next]);
}

void ValidateReplacement(const Macro& macro)
{
	for (size_t i = 0; i < macro.replacement.size(); ++i)
	{
		const Token& token = macro.replacement[i];
		if (IsIdentifier(token) && token.text == "__VA_ARGS__")
			Require(macro.function_like && macro.variadic,
				"__VA_ARGS__ outside variadic macro");
		if (IsPaste(token))
		{
			size_t left = PreviousNonSpace(macro.replacement, i);
			size_t right = NextNonSpace(macro.replacement, i + 1,
				macro.replacement.size());
			Require(left < macro.replacement.size() &&
				right < macro.replacement.size(),
				"invalid initial or final ##");
		}
		if (IsHash(token) && !ReplacementHashIsStringize(
			macro, macro.replacement, i))
		{
			size_t left = PreviousNonSpace(macro.replacement, i);
			size_t right = NextNonSpace(macro.replacement, i + 1,
				macro.replacement.size());
			const bool paste_operand =
				(left < macro.replacement.size() &&
				 IsPaste(macro.replacement[left])) ||
				(right < macro.replacement.size() &&
				 IsPaste(macro.replacement[right]));
			Require(!macro.function_like || paste_operand,
				"# must precede a macro parameter");
		}
	}
}

Macro ParseDefine(const vector<Token>& tokens, size_t hash, size_t end)
{
	Macro macro;
	size_t position = SkipSpaces(tokens, hash + 1, end);
	Require(position < end && IsIdentifier(tokens[position]) &&
		tokens[position].text == "define", "invalid define directive");
	position = SkipSpaces(tokens, position + 1, end);
	Require(position < end && IsIdentifier(tokens[position]),
		"macro name is missing");
	macro.name = tokens[position].text;
	Require(macro.name != "__VA_ARGS__", "invalid macro name");
	const size_t name_position = position++;
	macro.function_like = position < end && !IsSpace(tokens[position]) &&
		IsPunct(tokens[position], "(");
	if (macro.function_like)
	{
		size_t close = end;
		ParseParameters(tokens, position, end, &macro.parameters,
			&macro.variadic, &close);
		position = close + 1;
	}
	else
		position = name_position + 1;
	vector<Token> replacement(tokens.begin() + position, tokens.begin() + end);
	macro.replacement = TrimAndCollapseSpaces(replacement);
	for (size_t i = 0; i < macro.replacement.size(); ++i)
		macro.replacement[i].from_replacement = true;
	ValidateReplacement(macro);
	return macro;
}

void ParseUndef(const vector<Token>& tokens, size_t hash, size_t end,
	map<string, Macro>* macros)
{
	size_t position = SkipSpaces(tokens, hash + 1, end);
	Require(position < end && IsIdentifier(tokens[position]) &&
		tokens[position].text == "undef", "invalid undef directive");
	position = SkipSpaces(tokens, position + 1, end);
	Require(position < end && IsIdentifier(tokens[position]),
		"macro name is missing");
	const string name = tokens[position].text;
	Require(name != "__VA_ARGS__", "invalid macro name");
	position = SkipSpaces(tokens, position + 1, end);
	Require(position == end, "extra tokens after undef macro name");
	macros->erase(name);
}

string StringizeText(const vector<Token>& argument)
{
	string result;
	bool pending_space = false;
	bool emitted = false;
	for (size_t i = 0; i < argument.size(); ++i)
	{
		if (IsSpace(argument[i]))
		{
			if (emitted)
				pending_space = true;
			continue;
		}
		if (pending_space)
		{
			result.push_back(' ');
			pending_space = false;
		}
		const bool escape_backslashes =
			argument[i].kind == TOKEN_STRING ||
			argument[i].kind == TOKEN_USER_STRING ||
			argument[i].kind == TOKEN_CHARACTER ||
			argument[i].kind == TOKEN_USER_CHARACTER;
		for (size_t j = 0; j < argument[i].text.size(); ++j)
		{
			const char c = argument[i].text[j];
			if (c == '"' || (c == '\\' && escape_backslashes))
				result.push_back('\\');
			result.push_back(c);
		}
		emitted = true;
	}
	return result;
}

Token Stringize(const vector<Token>& argument, const set<string>& blocked)
{
	Token result(TOKEN_STRING, "\"");
	result.text += StringizeText(argument);
	result.text += "\"";
	result.blocked = blocked;
	return result;
}

vector<Token> NonSpaceTokens(const vector<Token>& input)
{
	vector<Token> result;
	for (size_t i = 0; i < input.size(); ++i)
		if (!IsSpace(input[i]))
			result.push_back(input[i]);
	return result;
}

vector<Token> PasteTokens(const Token& left, const Token& right)
{
	if (left.kind == TOKEN_PLACEMARKER && right.kind == TOKEN_PLACEMARKER)
		return vector<Token>(1, Token(TOKEN_PLACEMARKER));
	if (left.kind == TOKEN_PLACEMARKER)
	{
		Token result = CopyToken(right);
		result.from_argument = false;
		result.from_replacement = true;
		return vector<Token>(1, result);
	}
	if (right.kind == TOKEN_PLACEMARKER && left.text == ",")
		return vector<Token>();
	if (right.kind == TOKEN_PLACEMARKER)
	{
		Token result = CopyToken(left);
		result.from_argument = false;
		result.from_replacement = true;
		return vector<Token>(1, result);
	}
	vector<Token> pasted = NonSpaceTokens(Tokenize(left.text + right.text));
	set<string> blocked = left.blocked;
	blocked.insert(right.blocked.begin(), right.blocked.end());
	const bool from_argument = left.from_argument || right.from_argument;
	for (size_t i = 0; i < pasted.size(); ++i)
	{
		pasted[i].blocked.insert(blocked.begin(), blocked.end());
		pasted[i].from_argument = from_argument;
		pasted[i].from_replacement = true;
	}
	return pasted;
}

vector<Token> ProcessPastes(vector<Token> input)
{
	for (size_t position = 0; position < input.size(); ++position)
	{
		if (input[position].kind != TOKEN_PASTE)
			continue;
		size_t left = PreviousNonSpace(input, position);
		size_t right = NextNonSpace(input, position + 1, input.size());
		Require(left < input.size() && right < input.size(),
			"invalid token paste");
		vector<Token> pasted = PasteTokens(input[left], input[right]);
		input.erase(input.begin() + left, input.begin() + right + 1);
		input.insert(input.begin() + left, pasted.begin(), pasted.end());
		position = left == 0 ? 0 : left - 1;
	}
	for (size_t i = 0; i < input.size();)
	{
		if (input[i].kind == TOKEN_PLACEMARKER)
			input.erase(input.begin() + i);
		else
			++i;
	}
	return input;
}

class MacroProcessor
{
public:
	void Run(const string& input)
	{
		const vector<Token> tokens = Tokenize(input);
		vector<Token> output;
		ProcessLines(tokens, &output);
		vector<PostPPToken> post_tokens;
		for (size_t i = 0; i < output.size(); ++i)
			AppendPostToken(output[i], &post_tokens);
		post_tokens.push_back(PostPPToken(POST_PP_EOF));
		RunPostToken(post_tokens);
	}

private:
	map<string, Macro> macros;

	void ProcessLines(const vector<Token>& tokens, vector<Token>* output)
	{
		size_t text_begin = 0;
		size_t position = 0;
		bool line_start = true;
		while (position < tokens.size())
		{
			const size_t line_end = FindLineEnd(tokens, position);
			string directive;
			size_t hash = tokens.size();
			if (line_start && FindDirective(tokens, position, line_end,
				&directive, &hash))
			{
				ExpandText(vector<Token>(tokens.begin() + text_begin,
					tokens.begin() + position), output);
				if (directive == "define")
				{
					Macro macro = ParseDefine(tokens, hash, line_end);
					DefineMacro(macro);
				}
				else
					ParseUndef(tokens, hash, line_end, &macros);
				position = line_end;
				if (position < tokens.size())
					++position;
				text_begin = position;
				line_start = true;
				continue;
			}
			if (tokens[position].kind == TOKEN_NEWLINE)
				line_start = true;
			else if (!IsSpace(tokens[position]))
				line_start = false;
			++position;
		}
		ExpandText(vector<Token>(tokens.begin() + text_begin, tokens.end()), output);
	}

	void DefineMacro(const Macro& macro)
	{
		map<string, Macro>::iterator found = macros.find(macro.name);
		if (found != macros.end())
			Require(EqualMacros(found->second, macro),
				"incompatible macro redefinition");
		else
			macros[macro.name] = macro;
	}

	void ExpandText(const vector<Token>& input, vector<Token>* output)
	{
		vector<Token> normalized = TrimAndCollapseSpaces(input);
		vector<Token> expanded = ExpandTokens(normalized);
		output->insert(output->end(), expanded.begin(), expanded.end());
	}

	bool TryInvocation(const vector<Token>& tokens, size_t name_position,
		const Macro& macro, Invocation* invocation, size_t* end)
	{
		size_t open = NextNonSpace(tokens, name_position + 1, tokens.size());
		if (open >= tokens.size() || !IsPunct(tokens[open], "("))
			return false;
		vector<Token> current;
		int depth = 0;
		bool saw_comma = false;
		bool saw_content = false;
		for (size_t i = open + 1; i < tokens.size(); ++i)
		{
			if (IsPunct(tokens[i], "(") )
			{
				++depth;
				current.push_back(tokens[i]);
				continue;
			}
			if (IsPunct(tokens[i], ")"))
			{
				if (depth > 0)
				{
					--depth;
					current.push_back(tokens[i]);
					continue;
				}
				if (saw_comma || saw_content)
					invocation->arguments.push_back(current);
				else
					invocation->empty_call = true;
				*end = i + 1;
				return true;
			}
			if (depth == 0 && IsPunct(tokens[i], ","))
			{
				saw_comma = true;
				invocation->arguments.push_back(current);
				current.clear();
				continue;
			}
			if (!IsSpace(tokens[i]))
				saw_content = true;
			current.push_back(tokens[i]);
		}
		throw logic_error("unterminated macro invocation");
	}

	vector<Token> ExpandTokens(vector<Token> result)
	{
		size_t position = 0;
		while (position < result.size())
		{
			if (!IsIdentifier(result[position]))
			{
				++position;
				continue;
			}
			if (result[position].text == "__VA_ARGS__")
				throw logic_error("__VA_ARGS__ outside variadic macro");
			map<string, Macro>::const_iterator found =
				macros.find(result[position].text);
			bool blocked = found != macros.end() &&
				result[position].blocked.find(found->first) !=
				result[position].blocked.end();
			if (blocked && found->second.function_like &&
				!result[position].from_argument)
			{
				const size_t open = NextNonSpace(result, position + 1,
					result.size());
				if (open < result.size() && IsPunct(result[open], "(") &&
					!result[open].from_replacement)
					blocked = false;
			}
			if (found == macros.end() || blocked)
			{
				++position;
				continue;
			}
			Invocation invocation;
			size_t end = position + 1;
			if (found->second.function_like && !TryInvocation(
				result, position, found->second, &invocation, &end))
			{
				++position;
				continue;
			}
			vector<Token> replacement = ExpandInvocation(
				found->second, result[position], invocation);
			result.erase(result.begin() + position, result.begin() + end);
			result.insert(result.begin() + position,
				replacement.begin(), replacement.end());
		}
		return result;
	}

	vector<Token> JoinVariadic(const vector<vector<Token> >& arguments,
		size_t begin) const
	{
		vector<Token> result;
		for (size_t i = begin; i < arguments.size(); ++i)
		{
			if (i != begin)
				result.push_back(Token(TOKEN_PUNCTUATOR, ","));
			result.insert(result.end(), arguments[i].begin(), arguments[i].end());
		}
		return result;
	}

	void AddInheritedPaint(vector<Token>* argument,
		const set<string>& inherited) const
	{
		for (size_t i = 0; i < argument->size(); ++i)
		{
			if (IsSpace((*argument)[i]))
				continue;
			(*argument)[i].blocked.insert(inherited.begin(), inherited.end());
		}
	}

	void PaintCurrentMacro(vector<Token>* argument, const string& current) const
	{
		for (size_t i = 0; i < argument->size(); ++i)
			if (!IsSpace((*argument)[i]))
				(*argument)[i].blocked.insert(current);
	}

	bool NeedsExpandedArgument(const Macro& macro, const string& name) const
	{
		for (size_t i = 0; i < macro.replacement.size(); ++i)
		{
			if (!IsIdentifier(macro.replacement[i]) ||
				macro.replacement[i].text != name)
				continue;
			const size_t previous = PreviousNonSpace(macro.replacement, i);
			const size_t next = NextNonSpace(macro.replacement, i + 1,
				macro.replacement.size());
			if (previous < macro.replacement.size() &&
				IsHash(macro.replacement[previous]))
				continue;
			if ((previous < macro.replacement.size() &&
				IsPaste(macro.replacement[previous])) ||
				(next < macro.replacement.size() &&
				 IsPaste(macro.replacement[next])))
				continue;
			return true;
		}
		return false;
	}

	vector<Token> ExpandInvocation(const Macro& macro, const Token& head,
		const Invocation& invocation)
	{
		vector<vector<Token> > arguments = invocation.arguments;
		const bool no_arguments = invocation.empty_call &&
			macro.parameters.empty();
		if (invocation.empty_call && !no_arguments)
			arguments.push_back(vector<Token>());
		if (!macro.variadic)
			Require(arguments.size() == macro.parameters.size(),
				"wrong number of macro arguments");
		else
			Require(arguments.size() >= macro.parameters.size(),
				"wrong number of macro arguments");

		vector<vector<Token> > raw_arguments;
		vector<vector<Token> > expanded_arguments;
		for (size_t i = 0; i < arguments.size(); ++i)
		{
			vector<Token> raw_argument = CollapseSpaces(arguments[i]);
			AddInheritedPaint(&raw_argument, head.blocked);
			vector<Token> expanded_argument = raw_argument;
			const string parameter_name = i < macro.parameters.size() ?
				macro.parameters[i] : "__VA_ARGS__";
			if (NeedsExpandedArgument(macro, parameter_name))
				expanded_argument = ExpandTokens(raw_argument);
			AddInheritedPaint(&expanded_argument, head.blocked);
			PaintCurrentMacro(&raw_argument, macro.name);
			PaintCurrentMacro(&expanded_argument, macro.name);
			raw_arguments.push_back(raw_argument);
			expanded_arguments.push_back(expanded_argument);
		}
		map<string, vector<Token> > raw;
		map<string, vector<Token> > expanded;
		for (size_t i = 0; i < macro.parameters.size(); ++i)
		{
			raw[macro.parameters[i]] = raw_arguments[i];
			expanded[macro.parameters[i]] = expanded_arguments[i];
		}
		vector<Token> raw_varargs;
		vector<Token> expanded_varargs;
		if (macro.variadic)
		{
			raw_varargs = JoinVariadic(raw_arguments, macro.parameters.size());
			expanded_varargs = JoinVariadic(expanded_arguments,
				macro.parameters.size());
			raw["__VA_ARGS__"] = raw_varargs;
			expanded["__VA_ARGS__"] = expanded_varargs;
		}
		return Substitute(macro, head, raw, expanded);
	}

	vector<Token> Substitute(const Macro& macro, const Token& head,
		const map<string, vector<Token> >& raw,
		const map<string, vector<Token> >& expanded)
	{
		set<string> blocked = head.from_argument ? set<string>() : head.blocked;
		blocked.insert(macro.name);
		vector<Token> result;
		for (size_t i = 0; i < macro.replacement.size(); ++i)
		{
			const Token& token = macro.replacement[i];
			if (IsHash(token) && ReplacementHashIsStringize(
				macro, macro.replacement, i))
			{
				size_t parameter = NextNonSpace(macro.replacement, i + 1,
					macro.replacement.size());
				const string name = macro.replacement[parameter].text;
				result.push_back(Stringize(raw.find(name)->second, blocked));
				i = parameter;
				continue;
			}
			if (IsPaste(token))
			{
				Token paste(TOKEN_PASTE, token.text);
				paste.blocked = blocked;
				result.push_back(paste);
				continue;
			}
			if (IsParameterToken(macro, token))
			{
				const size_t previous = PreviousNonSpace(macro.replacement, i);
				const size_t next = NextNonSpace(macro.replacement, i + 1,
					macro.replacement.size());
				const bool pasted =
					(previous < macro.replacement.size() &&
					 IsPaste(macro.replacement[previous])) ||
					(next < macro.replacement.size() &&
					 IsPaste(macro.replacement[next]));
				const map<string, vector<Token> >& source = pasted ? raw : expanded;
				map<string, vector<Token> >::const_iterator argument =
					source.find(token.text);
				if (argument == source.end())
					throw logic_error("missing macro argument");
				if (pasted && !HasNonSpace(argument->second))
					result.push_back(Token(TOKEN_PLACEMARKER));
				else
				{
					for (size_t j = 0; j < argument->second.size(); ++j)
					{
						Token copy = argument->second[j];
						copy.from_argument = true;
						copy.from_replacement = false;
						result.push_back(copy);
					}
				}
				continue;
			}
			Token copy = token;
			copy.blocked = blocked;
			result.push_back(copy);
		}
		return ProcessPastes(result);
	}

	void AppendPostToken(const Token& token, vector<PostPPToken>* output)
	{
		if (IsSpace(token) || token.kind == TOKEN_PLACEMARKER ||
			token.kind == TOKEN_PASTE)
			return;
		PostPPTokenKind kind = POST_PP_NON_WHITESPACE;
		switch (token.kind)
		{
		case TOKEN_IDENTIFIER: kind = POST_PP_IDENTIFIER; break;
		case TOKEN_NUMBER: kind = POST_PP_NUMBER; break;
		case TOKEN_CHARACTER: kind = POST_PP_CHARACTER; break;
		case TOKEN_USER_CHARACTER: kind = POST_PP_USER_CHARACTER; break;
		case TOKEN_STRING: kind = POST_PP_STRING; break;
		case TOKEN_USER_STRING: kind = POST_PP_USER_STRING; break;
		case TOKEN_PUNCTUATOR: kind = POST_PP_PUNCTUATOR; break;
		case TOKEN_HEADER: kind = POST_PP_HEADER; break;
		case TOKEN_NON_WHITESPACE: kind = POST_PP_NON_WHITESPACE; break;
		default: return;
		}
		output->push_back(PostPPToken(kind, token.text));
	}
};

} // namespace

void RunMacro(const string& input)
{
	MacroProcessor().Run(input);
}
