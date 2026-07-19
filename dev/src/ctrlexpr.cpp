#include "ctrlexpr.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "posttoken_lexer.h"
#include "posttoken_unicode.h"
#include "pptoken_translation.h"

using namespace std;

namespace {

class LineError : public exception
{
public:
	const char* what() const noexcept { return "invalid controlling expression"; }
};

typedef LineError EvalError;

bool IsWhitespace(const SourceUnit& unit)
{
	return !unit.raw && IsSourceWhitespace(unit.code_point);
}

string Encode(const vector<SourceUnit>& units, size_t begin, size_t end)
{
	string result;
	for (size_t i = begin; i < end; ++i)
		result += EncodeUTF8CodePoint(units[i].code_point);
	return result;
}

int RawPrefixLength(const vector<SourceUnit>& units, size_t position)
{
	if (position >= units.size())
		return -1;
	if (units[position].code_point == 'R' &&
		position + 1 < units.size() && units[position + 1].code_point == '"')
		return 1;
	if ((units[position].code_point == 'u' ||
		 units[position].code_point == 'U' ||
		 units[position].code_point == 'L') &&
		position + 2 < units.size() && units[position + 1].code_point == 'R' &&
		units[position + 2].code_point == '"')
		return 2;
	if (units[position].code_point == 'u' &&
		position + 3 < units.size() && units[position + 1].code_point == '8' &&
		units[position + 2].code_point == 'R' &&
		units[position + 3].code_point == '"')
		return 3;
	return -1;
}

size_t StringQuote(const vector<SourceUnit>& units, size_t position)
{
	if (position >= units.size())
		return units.size();
	if (units[position].code_point == '"' ||
		units[position].code_point == '\'')
		return position;
	if ((units[position].code_point == 'u' ||
		 units[position].code_point == 'U' ||
		 units[position].code_point == 'L') &&
		position + 1 < units.size() &&
		(units[position + 1].code_point == '"' ||
		 units[position + 1].code_point == '\''))
		return position + 1;
	if (units[position].code_point == 'u' &&
		position + 2 < units.size() && units[position + 1].code_point == '8' &&
		units[position + 2].code_point == '"')
		return position + 2;
	return units.size();
}

bool IsHex(int c)
{
	return IsHexDigit(c);
}

int HexValue(int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return c - 'A' + 10;
}

bool IsOctal(int c)
{
	return IsAsciiOctalDigit(c);
}

size_t ConsumeEscape(const vector<SourceUnit>& units, size_t slash)
{
	if (slash + 1 >= units.size() || units[slash + 1].code_point == '\n')
		throw logic_error("unterminated escape sequence");
	const int escaped = units[slash + 1].code_point;
	if (string("'\"?\\abfnrtv").find(static_cast<char>(escaped)) !=
		string::npos)
		return slash + 2;
	if (IsOctal(escaped))
	{
		size_t end = slash + 2;
		while (end < units.size() && end < slash + 4 &&
			IsOctal(units[end].code_point))
			++end;
		return end;
	}
	if (escaped == 'x')
	{
		size_t end = slash + 2;
		if (end >= units.size() || !IsHex(units[end].code_point))
			throw logic_error("invalid hexadecimal escape sequence");
		while (end < units.size() && IsHex(units[end].code_point))
			++end;
		return end;
	}
	if (escaped == 'u' || escaped == 'U')
	{
		const size_t count = escaped == 'u' ? 4 : 8;
		if (slash + 2 + count > units.size())
			throw logic_error("invalid universal-character-name");
		for (size_t i = 0; i < count; ++i)
			if (!IsHex(units[slash + 2 + i].code_point))
				throw logic_error("invalid universal-character-name");
		return slash + 2 + count;
	}
	throw logic_error("invalid escape sequence");
}

PostPPToken ParseQuoted(const vector<SourceUnit>& units, size_t position,
	size_t quote, bool character)
{
	size_t end = quote + 1;
	bool has_content = false;
	while (end < units.size())
	{
		if (units[end].code_point == '\n')
			throw logic_error("unterminated quoted literal");
		if (units[end].code_point == '\\')
		{
			end = ConsumeEscape(units, end);
			has_content = true;
			continue;
		}
		if (units[end].code_point == (character ? '\'' : '"'))
			break;
		++end;
		has_content = true;
	}
	if (end >= units.size())
		throw logic_error("unterminated quoted literal");
	if (character && !has_content)
		throw logic_error("empty character literal");
	++end;
	const size_t suffix_begin = end;
	while (end < units.size() && IsIdentifierBody(units[end].code_point))
		++end;
	return PostPPToken(character ?
		(suffix_begin == end ? POST_PP_CHARACTER : POST_PP_USER_CHARACTER) :
		(suffix_begin == end ? POST_PP_STRING : POST_PP_USER_STRING),
		Encode(units, position, end));
}

PostPPToken ParseRaw(const vector<SourceUnit>& units, size_t position,
	int prefix)
{
	const size_t quote = position + static_cast<size_t>(prefix);
	const size_t delimiter_begin = quote + 1;
	size_t open = delimiter_begin;
	while (open < units.size() && units[open].code_point != '(')
	{
		const int c = units[open].code_point;
		if (c == '\n' || !IsRawDelimiterCodePoint(c))
			throw logic_error("invalid raw string delimiter");
		if (open - delimiter_begin >= 16)
			throw logic_error("raw string delimiter too long");
		++open;
	}
	if (open == units.size())
		throw logic_error("unterminated raw string literal");
	const size_t delimiter_length = open - delimiter_begin;
	for (size_t close = open + 1; close < units.size(); ++close)
	{
		if (units[close].code_point != ')')
			continue;
		const size_t delimiter_end = close + 1 + delimiter_length;
		if (delimiter_end >= units.size() ||
			units[delimiter_end].code_point != '"')
			continue;
		bool matches = true;
		for (size_t i = 0; i < delimiter_length; ++i)
			if (units[close + 1 + i].code_point !=
				units[delimiter_begin + i].code_point)
				matches = false;
		if (!matches)
			continue;
		size_t end = delimiter_end + 1;
		const size_t suffix_begin = end;
		while (end < units.size() && IsIdentifierBody(units[end].code_point))
			++end;
		return PostPPToken(suffix_begin == end ? POST_PP_STRING : POST_PP_USER_STRING,
			Encode(units, position, end));
	}
	throw logic_error("unterminated raw string literal");
}

PostPPToken ParseNumber(const vector<SourceUnit>& units, size_t position)
{
	size_t end = position + (units[position].code_point == '.' ? 2 : 1);
	while (end < units.size())
	{
		const int c = units[end].code_point;
		if (IsAsciiDigit(c) || IsIdentifierNondigit(c))
		{
			if ((c == 'e' || c == 'E') && end + 1 < units.size() &&
				(units[end + 1].code_point == '+' ||
				 units[end + 1].code_point == '-'))
				end += 2;
			else
				++end;
		}
		else if (c == '.')
			++end;
		else
			break;
	}
	return PostPPToken(POST_PP_NUMBER, Encode(units, position, end));
}

PostPPToken ParsePunctuator(const vector<SourceUnit>& units, size_t position)
{
	if (units[position].code_point == '<' && position + 2 < units.size() &&
		units[position + 1].code_point == ':' &&
		units[position + 2].code_point == ':' &&
		(position + 3 >= units.size() ||
		 (units[position + 3].code_point != ':' &&
		  units[position + 3].code_point != '>')))
		return PostPPToken(POST_PP_PUNCTUATOR, Encode(units, position, position + 1));

	static const char* const punctuators[] = {
		"%:%:", "->*", "<<=", ">>=", "...", "##", "<:", ":>", "<%", "%>", "%:",
		".*", "::", "+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=", "<<", ">>",
		"<=", ">=", "&&", "==", "!=", "||", "++", "--", "->", "{", "}", "[", "]",
		"#", "(", ")", ";", ":", "?", ".", "+", "-", "*", "/", "%", "^", "&", "|",
		"~", "!", "=", "<", ">", ","
	};
	for (size_t i = 0; i < sizeof(punctuators) / sizeof(*punctuators); ++i)
	{
		const string text = punctuators[i];
		bool matches = true;
		for (size_t j = 0; j < text.size(); ++j)
			if (position + j >= units.size() ||
				units[position + j].code_point != text[j])
				matches = false;
		if (matches)
			return PostPPToken(POST_PP_PUNCTUATOR,
				Encode(units, position, position + text.size()));
	}
	return PostPPToken(POST_PP_NON_WHITESPACE,
		Encode(units, position, position + 1));
}

class CtrlLexer
{
public:
	explicit CtrlLexer(const string& input)
		: units(TranslateSource(input)), position(0) {}

	bool NextLine(vector<PostPPToken>* tokens)
	{
		tokens->clear();
		while (position < units.size())
		{
			if (SkipSpaceOrComment())
				continue;
			if (!units[position].raw && units[position].code_point == '\n')
			{
				++position;
				return true;
			}
			tokens->push_back(NextToken());
		}
		return !tokens->empty();
	}

private:
	vector<SourceUnit> units;
	size_t position;

	bool SkipSpaceOrComment()
	{
		if (position >= units.size())
			return false;
		if (IsWhitespace(units[position]))
		{
			++position;
			return true;
		}
		if (units[position].raw || units[position].code_point != '/' ||
			position + 1 >= units.size() || units[position + 1].raw)
			return false;
		if (units[position + 1].code_point == '/')
		{
			position += 2;
			while (position < units.size() && units[position].code_point != '\n')
				++position;
			return true;
		}
		if (units[position + 1].code_point == '*')
		{
			position += 2;
			while (position + 1 < units.size() &&
				!(units[position].code_point == '*' &&
				  units[position + 1].code_point == '/'))
				++position;
			if (position + 1 >= units.size())
				throw logic_error("unterminated comment");
			position += 2;
			return true;
		}
		return false;
	}

	PostPPToken NextToken()
	{
		const int raw = RawPrefixLength(units, position);
		if (raw >= 0)
			return Advance(ParseRaw(units, position, raw));
		const size_t quote = StringQuote(units, position);
		if (quote < units.size())
			return Advance(ParseQuoted(units, position, quote,
				units[quote].code_point == '\''));
		if (IsIdentifierStart(units[position].code_point))
		{
			size_t end = position + 1;
			while (end < units.size() && IsIdentifierBody(units[end].code_point))
				++end;
			return Advance(PostPPToken(POST_PP_IDENTIFIER,
				Encode(units, position, end)));
		}
		if (IsAsciiDigit(units[position].code_point) ||
			(units[position].code_point == '.' && position + 1 < units.size() &&
			 IsAsciiDigit(units[position + 1].code_point)))
			return Advance(ParseNumber(units, position));
		return Advance(ParsePunctuator(units, position));
	}

	PostPPToken Advance(const PostPPToken& token)
	{
		position += PostDecodeUTF8(token.source).size();
		return token;
	}
};

struct IntegerSpec
{
	bool unsig;
	int long_kind;
	int base;
	string digits;

	IntegerSpec() : unsig(false), long_kind(0), base(10) {}
};

bool IsDecimal(int c)
{
	return IsAsciiDigit(c);
}

bool IsDigitForBase(int c, int base)
{
	if (base == 16)
		return IsHex(c);
	return IsDecimal(c);
}

bool ParseIntegerSpec(const string& source, IntegerSpec* spec)
{
	if (source.empty())
		return false;
	size_t end = 0;
	if (source.size() >= 2 && source[0] == '0' &&
		(source[1] == 'x' || source[1] == 'X'))
	{
		spec->base = 16;
		end = 2;
		while (end < source.size() && IsDigitForBase(source[end], 16))
			++end;
		if (end == 2)
			return false;
	}
	else if (source[0] == '0')
	{
		spec->base = 8;
		end = 1;
		while (end < source.size() && IsDecimal(source[end]))
			++end;
	}
	else
	{
		spec->base = 10;
		end = 0;
		while (end < source.size() && IsDecimal(source[end]))
			++end;
		if (end == 0)
			return false;
	}
	spec->digits = source.substr(spec->base == 16 ? 2 : 0,
		end - (spec->base == 16 ? 2 : 0));
	bool saw_u = false;
	bool saw_l = false;
	while (end < source.size())
	{
		const char c = source[end];
		if (c == 'u' || c == 'U')
		{
			if (saw_u)
				return false;
			saw_u = true;
			++end;
			continue;
		}
		if (c == 'l' || c == 'L')
		{
			if (saw_l)
				return false;
			saw_l = true;
			if (end + 1 < source.size() &&
				(source[end + 1] == 'l' || source[end + 1] == 'L'))
			{
				spec->long_kind = 2;
				end += 2;
			}
			else
			{
				spec->long_kind = 1;
				++end;
			}
			continue;
		}
		return false;
	}
	spec->unsig = saw_u;
	return true;
}

bool ParseUnsignedDigits(const string& digits, int base, uint64_t* value)
{
	uint64_t result = 0;
	for (size_t i = 0; i < digits.size(); ++i)
	{
		const int c = digits[i];
		int digit = -1;
		if (c >= '0' && c <= '9') digit = c - '0';
		else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
		if (digit < 0 || digit >= base)
			return false;
		if (result > (numeric_limits<uint64_t>::max() -
			static_cast<uint64_t>(digit)) / static_cast<uint64_t>(base))
			return false;
		result = result * static_cast<uint64_t>(base) +
			static_cast<uint64_t>(digit);
	}
	*value = result;
	return true;
}

struct Value
{
	bool is_unsigned;
	uint64_t bits;

	Value(bool is_unsigned = false, uint64_t bits = 0)
		: is_unsigned(is_unsigned), bits(bits) {}
};

int64_t SignedFromBits(uint64_t bits)
{
	const uint64_t max = static_cast<uint64_t>(numeric_limits<int64_t>::max());
	if (bits <= max)
		return static_cast<int64_t>(bits);
	return -1 - static_cast<int64_t>(~bits);
}

Value SignedValue(int64_t value)
{
	return Value(false, static_cast<uint64_t>(value));
}

Value UnsignedValue(uint64_t value)
{
	return Value(true, value);
}

Value ParseIntegralValue(const string& source)
{
	IntegerSpec spec;
	uint64_t value = 0;
	if (!ParseIntegerSpec(source, &spec) ||
		!ParseUnsignedDigits(spec.digits, spec.base, &value))
		throw LineError();
	if (spec.unsig || value > static_cast<uint64_t>(numeric_limits<int64_t>::max()))
	{
		if (!spec.unsig && spec.base == 10)
			throw LineError();
		return UnsignedValue(value);
	}
	return SignedValue(static_cast<int64_t>(value));
}

int SimpleEscapeValue(int c)
{
	switch (c)
	{
	case '\'': return '\'';
	case '"': return '"';
	case '?': return '?';
	case '\\': return '\\';
	case 'a': return '\a';
	case 'b': return '\b';
	case 'f': return '\f';
	case 'n': return '\n';
	case 'r': return '\r';
	case 't': return '\t';
	case 'v': return '\v';
	default: return -1;
	}
}

bool ParseCharacterValue(const string& source, Value* result)
{
	const vector<int> units = PostDecodeUTF8(source);
	if (units.empty())
		return false;
	size_t quote = 0;
	bool unsigned_type = false;
	if (units[0] == '\'')
	{
		quote = 0;
	}
	else if (units.size() >= 2 &&
		(units[0] == 'u' || units[0] == 'U' || units[0] == 'L') &&
		units[1] == '\'')
	{
		quote = 1;
		unsigned_type = units[0] == 'u' || units[0] == 'U';
	}
	else
		return false;

	size_t end = quote + 1;
	vector<uint64_t> values;
	while (end < units.size() && units[end] != '\'')
	{
		uint64_t value = 0;
		if (units[end] != '\\')
		{
			value = static_cast<uint64_t>(units[end++]);
		}
		else
		{
			if (++end >= units.size())
				return false;
			const int escaped = units[end++];
			const int simple = SimpleEscapeValue(escaped);
			if (simple >= 0)
				value = static_cast<uint64_t>(simple);
			else if (IsOctal(escaped))
			{
				value = static_cast<uint64_t>(escaped - '0');
				for (int count = 1; count < 3 && end < units.size() &&
					IsOctal(units[end]); ++count, ++end)
					value = value * 8 + static_cast<uint64_t>(units[end] - '0');
			}
			else if (escaped == 'x')
			{
				if (end >= units.size() || !IsHex(units[end]))
					return false;
				while (end < units.size() && IsHex(units[end]))
				{
					const uint64_t digit = static_cast<uint64_t>(HexValue(units[end++]));
					if (value > (numeric_limits<uint64_t>::max() - digit) / 16)
						return false;
					value = value * 16 + digit;
				}
			}
			else if (escaped == 'u' || escaped == 'U')
			{
				const size_t count = escaped == 'u' ? 4 : 8;
				if (end + count > units.size())
					return false;
				for (size_t i = 0; i < count; ++i)
				{
					if (!IsHex(units[end])) return false;
					value = value * 16 + static_cast<uint64_t>(HexValue(units[end++]));
				}
			}
			else
				return false;
		}
		values.push_back(value);
	}
	if (end >= units.size() || values.size() != 1)
		return false;
	if (unsigned_type && units[0] == 'u' && values[0] > 0xffff)
		return false;
	if (values[0] > static_cast<uint64_t>(numeric_limits<int64_t>::max()))
		return false;
	*result = unsigned_type ? UnsignedValue(values[0]) :
		SignedValue(static_cast<int64_t>(values[0]));
	return true;
}

struct Type
{
	bool is_unsigned;
	explicit Type(bool is_unsigned = false) : is_unsigned(is_unsigned) {}
};

Type CommonType(Type left, Type right)
{
	return Type(left.is_unsigned || right.is_unsigned);
}

struct Node
{
	enum Kind { LITERAL, UNARY, BINARY, CONDITIONAL };

	Kind kind;
	Type type;
	string op;
	Value value;
	size_t left;
	size_t right;
	size_t third;

	Node(Kind kind, Type type)
		: kind(kind), type(type), op(), value(),
		  left(static_cast<size_t>(-1)), right(static_cast<size_t>(-1)),
		  third(static_cast<size_t>(-1)) {}
};

class Parser
{
public:
	explicit Parser(const vector<PostPPToken>& tokens, vector<Node>* nodes)
		: tokens(tokens), position(0), nodes(nodes) {}

	size_t Parse()
	{
		if (tokens.empty())
			throw LineError();
		const size_t result = ParseConditional();
		if (position != tokens.size())
			throw LineError();
		return result;
	}

private:
	const vector<PostPPToken>& tokens;
	size_t position;
	vector<Node>* nodes;

	size_t MakeNode(Node::Kind kind, Type type)
	{
		nodes->push_back(Node(kind, type));
		return nodes->size() - 1;
	}

	bool Match(const string& text)
	{
		if (position >= tokens.size() || tokens[position].source != text)
			return false;
		++position;
		return true;
	}

	bool MatchAny(const vector<string>& texts, string* matched)
	{
		if (position >= tokens.size())
			return false;
		for (size_t i = 0; i < texts.size(); ++i)
			if (tokens[position].source == texts[i])
			{
				*matched = texts[i];
				++position;
				return true;
			}
		return false;
	}

	size_t ParseConditional()
	{
		size_t condition = ParseBinaryLevel(9);
		if (!Match("?"))
			return condition;
		const size_t when_true = ParseConditional();
		if (!Match(":"))
			throw LineError();
		const size_t when_false = ParseConditional();
		const size_t result = MakeNode(Node::CONDITIONAL,
			CommonType((*nodes)[when_true].type, (*nodes)[when_false].type));
		(*nodes)[result].left = condition;
		(*nodes)[result].right = when_true;
		(*nodes)[result].third = when_false;
		return result;
	}

	size_t ParseBinaryLevel(int level)
	{
		static const vector<string> operators[] = {
			vector<string>{"*", "/", "%"},
			vector<string>{"+", "-"},
			vector<string>{"<<", ">>"},
			vector<string>{"<", ">", "<=", ">="},
			vector<string>{"==", "!="},
			vector<string>{"&", "bitand"},
			vector<string>{"^", "xor"},
			vector<string>{"|", "bitor"},
			vector<string>{"&&", "and"},
			vector<string>{"||", "or"}
		};
		size_t left = level == 0 ? ParseUnary() :
			ParseBinaryLevel(level - 1);
		string op;
		while (MatchAny(operators[level], &op))
		{
			const size_t right = level == 0 ? ParseUnary() :
				ParseBinaryLevel(level - 1);
			Type type;
			if (level == 2)
				type = (*nodes)[left].type;
			else if ((level >= 3 && level <= 4) || level >= 8)
				type = Type(false);
			else
				type = CommonType((*nodes)[left].type, (*nodes)[right].type);
			const size_t result = MakeNode(Node::BINARY, type);
			(*nodes)[result].op = op;
			(*nodes)[result].left = left;
			(*nodes)[result].right = right;
			left = result;
		}
		return left;
	}

	size_t ParseUnary()
	{
		static const vector<string> operators = {"+", "-", "!", "~", "not", "compl"};
		string op;
		if (MatchAny(operators, &op))
		{
			const size_t operand = ParseUnary();
			const Type type = (op == "!" || op == "not") ? Type(false) :
				(*nodes)[operand].type;
			const size_t result = MakeNode(Node::UNARY, type);
			(*nodes)[result].op = op;
			(*nodes)[result].left = operand;
			return result;
		}
		return ParsePrimary();
	}

	size_t ParsePrimary()
	{
		if (Match("("))
		{
			const size_t result = ParseConditional();
			if (!Match(")"))
				throw LineError();
			return result;
		}
		if (position >= tokens.size())
			throw LineError();
		const PostPPToken& token = tokens[position];
		if (token.kind == POST_PP_IDENTIFIER)
		{
			++position;
			if (token.source == "defined")
				return ParseDefined();
			if (token.source == "true")
				return Literal(SignedValue(1));
			if (token.source == "false")
				return Literal(SignedValue(0));
			return Literal(SignedValue(0));
		}
		++position;
		if (token.kind == POST_PP_NUMBER)
			return Literal(ParseIntegralValue(token.source));
		if (token.kind == POST_PP_CHARACTER)
		{
			Value value;
			if (!ParseCharacterValue(token.source, &value))
				throw LineError();
			return Literal(value);
		}
		throw LineError();
	}

	size_t ParseDefined()
	{
		string identifier;
		if (Match("("))
		{
			if (position >= tokens.size() ||
				tokens[position].kind != POST_PP_IDENTIFIER)
				throw LineError();
			identifier = tokens[position++].source;
			if (!Match(")"))
				throw LineError();
		}
		else
		{
			if (position >= tokens.size() ||
				tokens[position].kind != POST_PP_IDENTIFIER)
				throw LineError();
			identifier = tokens[position++].source;
		}
		const unsigned char first = identifier.empty() ? 0 :
			static_cast<unsigned char>(identifier[0]);
		return Literal(SignedValue(first & 1));
	}

	size_t Literal(Value value)
	{
		const size_t result = MakeNode(Node::LITERAL, Type(value.is_unsigned));
		(*nodes)[result].value = value;
		return result;
	}
};

Value Convert(Value value, Type type)
{
	return Value(type.is_unsigned, value.bits);
}

uint64_t ShiftCount(Value value)
{
	if (value.is_unsigned)
	{
		if (value.bits >= 64)
			throw EvalError();
		return value.bits;
	}
	const int64_t count = SignedFromBits(value.bits);
	if (count < 0 || count >= 64)
		throw EvalError();
	return static_cast<uint64_t>(count);
}

Value Evaluate(size_t index, const vector<Node>& nodes)
{
	const Node& node = nodes[index];
	if (node.kind == Node::LITERAL)
		return node.value;
	if (node.kind == Node::UNARY)
	{
		const Value operand = Evaluate(node.left, nodes);
		if (node.op == "!" || node.op == "not")
			return SignedValue(operand.bits == 0 ? 1 : 0);
		if (node.op == "~" || node.op == "compl")
			return Value(node.type.is_unsigned, ~operand.bits);
		if (node.op == "-")
			return Value(node.type.is_unsigned, uint64_t(0) - operand.bits);
		return Convert(operand, node.type);
	}
	if (node.kind == Node::CONDITIONAL)
	{
		const Value condition = Evaluate(node.left, nodes);
		const Value selected = condition.bits != 0 ?
			Evaluate(node.right, nodes) : Evaluate(node.third, nodes);
		return Convert(selected, node.type);
	}

	const Value left = Evaluate(node.left, nodes);
	if (node.op == "&&" || node.op == "and")
	{
		if (left.bits == 0)
			return SignedValue(0);
		return SignedValue(Evaluate(node.right, nodes).bits == 0 ? 0 : 1);
	}
	if (node.op == "||" || node.op == "or")
	{
		if (left.bits != 0)
			return SignedValue(1);
		return SignedValue(Evaluate(node.right, nodes).bits == 0 ? 0 : 1);
	}
	const Value right = Evaluate(node.right, nodes);
	const bool comparison = node.op == "<" || node.op == ">" ||
		node.op == "<=" || node.op == ">=" || node.op == "==" ||
		node.op == "!=";
	const Type operand_type = comparison ?
		CommonType(nodes[node.left].type, nodes[node.right].type) : node.type;
	const Value a = Convert(left, operand_type);
	const Value b = Convert(right, operand_type);
	if (node.op == "+") return Value(node.type.is_unsigned, a.bits + b.bits);
	if (node.op == "-") return Value(node.type.is_unsigned, a.bits - b.bits);
	if (node.op == "*") return Value(node.type.is_unsigned, a.bits * b.bits);
	if (node.op == "/" || node.op == "%")
	{
		if (b.bits == 0)
			throw EvalError();
		if (node.type.is_unsigned)
			return UnsignedValue(node.op == "/" ? a.bits / b.bits : a.bits % b.bits);
		const int64_t av = SignedFromBits(a.bits);
		const int64_t bv = SignedFromBits(b.bits);
		if (av == numeric_limits<int64_t>::min() && bv == -1)
			throw EvalError();
		return SignedValue(node.op == "/" ? av / bv : av % bv);
	}
	if (node.op == "<<" || node.op == ">>")
	{
		const uint64_t count = ShiftCount(right);
		if (node.type.is_unsigned)
			return UnsignedValue(node.op == "<<" ? a.bits << count : a.bits >> count);
		if (node.op == "<<")
			return SignedValue(SignedFromBits(a.bits << count));
		const int64_t av = SignedFromBits(a.bits);
		if (count == 0)
			return SignedValue(av);
		uint64_t bits = a.bits >> count;
		if (av < 0)
			bits |= numeric_limits<uint64_t>::max() << (64 - count);
		return SignedValue(SignedFromBits(bits));
	}
	if (node.op == "&") return Value(node.type.is_unsigned, a.bits & b.bits);
	if (node.op == "bitand") return Value(node.type.is_unsigned, a.bits & b.bits);
	if (node.op == "^") return Value(node.type.is_unsigned, a.bits ^ b.bits);
	if (node.op == "xor") return Value(node.type.is_unsigned, a.bits ^ b.bits);
	if (node.op == "|") return Value(node.type.is_unsigned, a.bits | b.bits);
	if (node.op == "bitor") return Value(node.type.is_unsigned, a.bits | b.bits);
	const bool unsigned_compare = operand_type.is_unsigned;
	if (unsigned_compare)
	{
		if (node.op == "<") return SignedValue(a.bits < b.bits);
		if (node.op == ">") return SignedValue(a.bits > b.bits);
		if (node.op == "<=") return SignedValue(a.bits <= b.bits);
		if (node.op == ">=") return SignedValue(a.bits >= b.bits);
	}
	else
	{
		const int64_t av = SignedFromBits(a.bits);
		const int64_t bv = SignedFromBits(b.bits);
		if (node.op == "<") return SignedValue(av < bv);
		if (node.op == ">") return SignedValue(av > bv);
		if (node.op == "<=") return SignedValue(av <= bv);
		if (node.op == ">=") return SignedValue(av >= bv);
	}
	if (node.op == "==") return SignedValue(a.bits == b.bits);
	if (node.op == "!=") return SignedValue(a.bits != b.bits);
	throw EvalError();
}

void PrintValue(Value value)
{
	if (value.is_unsigned)
		cout << static_cast<uintmax_t>(value.bits) << 'u' << endl;
	else
		cout << static_cast<intmax_t>(SignedFromBits(value.bits)) << endl;
}

void EvaluateLine(const vector<PostPPToken>& tokens, vector<Node>* nodes)
{
	nodes->clear();
	if (tokens.empty())
		return;
	try
	{
		Parser parser(tokens, nodes);
		const size_t root = parser.Parse();
		PrintValue(Evaluate(root, *nodes));
	}
	catch (const LineError&)
	{
		cout << "error" << endl;
	}
}

} // namespace

void RunCtrlExpr(const string& input)
{
	CtrlLexer lexer(input);
	vector<PostPPToken> line;
	vector<Node> nodes;
	while (lexer.NextLine(&line))
		EvaluateLine(line, &nodes);
	cout << "eof" << endl;
}
