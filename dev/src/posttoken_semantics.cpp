#include "posttoken_semantics.h"

#include <climits>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <unordered_map>
#include <vector>

#include "posttoken_unicode.h"

using namespace std;

namespace {

class ValidationBuffer : public streambuf
{
public:
	ValidationBuffer() : invalid_(false), prefix_length_(0) {}

	bool invalid() const { return invalid_; }

protected:
	virtual int_type overflow(int_type character = traits_type::eof())
	{
		if (!traits_type::eq_int_type(character, traits_type::eof()))
			Consume(traits_type::to_char_type(character));
		return character;
	}

	virtual streamsize xsputn(const char* text, streamsize length)
	{
		for (streamsize i = 0; i < length; ++i) Consume(text[i]);
		return length;
	}

	virtual int sync() { return 0; }

private:
	bool invalid_;
	size_t prefix_length_;

	void Consume(char character)
	{
		static const char prefix[] = "invalid ";
		if (prefix_length_ < sizeof(prefix) - 1)
		{
			if (character == prefix[prefix_length_])
				++prefix_length_;
			else
				prefix_length_ = sizeof(prefix);
			if (prefix_length_ == sizeof(prefix) - 1) invalid_ = true;
		}
		if (character == '\n')
		{
			prefix_length_ = 0;
		}
	}
};

enum FundamentalType
{
	FT_SIGNED_CHAR, FT_SHORT_INT, FT_INT, FT_LONG_INT, FT_LONG_LONG_INT,
	FT_UNSIGNED_CHAR, FT_UNSIGNED_SHORT_INT, FT_UNSIGNED_INT,
	FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT, FT_WCHAR_T, FT_CHAR,
	FT_CHAR16_T, FT_CHAR32_T, FT_BOOL, FT_FLOAT, FT_DOUBLE, FT_LONG_DOUBLE,
	FT_VOID, FT_NULLPTR_T
};

const char* FundamentalName(FundamentalType type)
{
	switch (type)
	{
	case FT_SIGNED_CHAR: return "signed char"; case FT_SHORT_INT: return "short int";
	case FT_INT: return "int"; case FT_LONG_INT: return "long int";
	case FT_LONG_LONG_INT: return "long long int"; case FT_UNSIGNED_CHAR: return "unsigned char";
	case FT_UNSIGNED_SHORT_INT: return "unsigned short int"; case FT_UNSIGNED_INT: return "unsigned int";
	case FT_UNSIGNED_LONG_INT: return "unsigned long int";
	case FT_UNSIGNED_LONG_LONG_INT: return "unsigned long long int";
	case FT_WCHAR_T: return "wchar_t"; case FT_CHAR: return "char";
	case FT_CHAR16_T: return "char16_t"; case FT_CHAR32_T: return "char32_t";
	case FT_BOOL: return "bool"; case FT_FLOAT: return "float";
	case FT_DOUBLE: return "double"; case FT_LONG_DOUBLE: return "long double";
	case FT_VOID: return "void"; case FT_NULLPTR_T: return "nullptr_t";
	}
	return "";
}

const unordered_map<string, string>& SimpleTokens()
{
	static const unordered_map<string, string> values = {
		{"alignas", "KW_ALIGNAS"}, {"alignof", "KW_ALIGNOF"}, {"asm", "KW_ASM"},
		{"auto", "KW_AUTO"}, {"bool", "KW_BOOL"}, {"break", "KW_BREAK"},
		{"case", "KW_CASE"}, {"catch", "KW_CATCH"}, {"char", "KW_CHAR"},
		{"char16_t", "KW_CHAR16_T"}, {"char32_t", "KW_CHAR32_T"}, {"class", "KW_CLASS"},
		{"const", "KW_CONST"}, {"constexpr", "KW_CONSTEXPR"}, {"const_cast", "KW_CONST_CAST"},
		{"continue", "KW_CONTINUE"}, {"decltype", "KW_DECLTYPE"}, {"default", "KW_DEFAULT"},
		{"delete", "KW_DELETE"}, {"do", "KW_DO"}, {"double", "KW_DOUBLE"},
		{"dynamic_cast", "KW_DYNAMIC_CAST"}, {"else", "KW_ELSE"}, {"enum", "KW_ENUM"},
		{"explicit", "KW_EXPLICIT"}, {"export", "KW_EXPORT"}, {"extern", "KW_EXTERN"},
		{"false", "KW_FALSE"}, {"float", "KW_FLOAT"}, {"for", "KW_FOR"},
		{"friend", "KW_FRIEND"}, {"goto", "KW_GOTO"}, {"if", "KW_IF"},
		{"inline", "KW_INLINE"}, {"int", "KW_INT"}, {"long", "KW_LONG"},
		{"mutable", "KW_MUTABLE"}, {"namespace", "KW_NAMESPACE"}, {"new", "KW_NEW"},
		{"noexcept", "KW_NOEXCEPT"}, {"nullptr", "KW_NULLPTR"}, {"operator", "KW_OPERATOR"},
		{"private", "KW_PRIVATE"}, {"protected", "KW_PROTECTED"}, {"public", "KW_PUBLIC"},
		{"register", "KW_REGISTER"}, {"reinterpret_cast", "KW_REINTERPET_CAST"},
		{"return", "KW_RETURN"}, {"short", "KW_SHORT"}, {"signed", "KW_SIGNED"},
		{"sizeof", "KW_SIZEOF"}, {"static", "KW_STATIC"}, {"static_assert", "KW_STATIC_ASSERT"},
		{"static_cast", "KW_STATIC_CAST"}, {"struct", "KW_STRUCT"}, {"switch", "KW_SWITCH"},
		{"template", "KW_TEMPLATE"}, {"this", "KW_THIS"}, {"thread_local", "KW_THREAD_LOCAL"},
		{"throw", "KW_THROW"}, {"true", "KW_TRUE"}, {"try", "KW_TRY"},
		{"typedef", "KW_TYPEDEF"}, {"typeid", "KW_TYPEID"}, {"typename", "KW_TYPENAME"},
		{"union", "KW_UNION"}, {"unsigned", "KW_UNSIGNED"}, {"using", "KW_USING"},
		{"virtual", "KW_VIRTUAL"}, {"void", "KW_VOID"}, {"volatile", "KW_VOLATILE"},
		{"wchar_t", "KW_WCHAR_T"}, {"while", "KW_WHILE"},
		{"{", "OP_LBRACE"}, {"<%", "OP_LBRACE"}, {"}", "OP_RBRACE"}, {"%>", "OP_RBRACE"},
		{"[", "OP_LSQUARE"}, {"<:", "OP_LSQUARE"}, {"]", "OP_RSQUARE"}, {":>", "OP_RSQUARE"},
		{"(", "OP_LPAREN"}, {")", "OP_RPAREN"}, {"|", "OP_BOR"}, {"bitor", "OP_BOR"},
		{"^", "OP_XOR"}, {"xor", "OP_XOR"}, {"~", "OP_COMPL"}, {"compl", "OP_COMPL"},
		{"&", "OP_AMP"}, {"bitand", "OP_AMP"}, {"!", "OP_LNOT"}, {"not", "OP_LNOT"},
		{";", "OP_SEMICOLON"}, {":", "OP_COLON"}, {"...", "OP_DOTS"}, {"?", "OP_QMARK"},
		{"::", "OP_COLON2"}, {".", "OP_DOT"}, {".*", "OP_DOTSTAR"}, {"+", "OP_PLUS"},
		{"-", "OP_MINUS"}, {"*", "OP_STAR"}, {"/", "OP_DIV"}, {"%", "OP_MOD"},
		{"=", "OP_ASS"}, {"<", "OP_LT"}, {">", "OP_GT"}, {"+=", "OP_PLUSASS"},
		{"-=", "OP_MINUSASS"}, {"*=", "OP_STARASS"}, {"/=", "OP_DIVASS"}, {"%=", "OP_MODASS"},
		{"^=", "OP_XORASS"}, {"xor_eq", "OP_XORASS"}, {"&=", "OP_BANDASS"}, {"and_eq", "OP_BANDASS"},
		{"|=", "OP_BORASS"}, {"or_eq", "OP_BORASS"}, {"<<", "OP_LSHIFT"}, {">>", "OP_RSHIFT"},
		{">>=", "OP_RSHIFTASS"}, {"<<=", "OP_LSHIFTASS"}, {"==", "OP_EQ"}, {"!=", "OP_NE"},
		{"not_eq", "OP_NE"}, {"<=", "OP_LE"}, {">=", "OP_GE"}, {"&&", "OP_LAND"},
		{"and", "OP_LAND"}, {"||", "OP_LOR"}, {"or", "OP_LOR"}, {"++", "OP_INC"},
		{"--", "OP_DEC"}, {",", "OP_COMMA"}, {"->*", "OP_ARROWSTAR"}, {"->", "OP_ARROW"}
	};
	return values;
}

void EmitInvalid(const string& source) { cout << "invalid " << source << endl; }
void EmitSimple(const string& source, const string& kind)
{
	cout << "simple " << source << " " << kind << endl;
}
void EmitIdentifier(const string& source) { cout << "identifier " << source << endl; }

string HexDump(const vector<unsigned char>& bytes)
{
	static const char* digits = "0123456789ABCDEF";
	string result;
	result.reserve(bytes.size() * 2);
	for (size_t i = 0; i < bytes.size(); ++i)
	{
		result.push_back(digits[bytes[i] >> 4]);
		result.push_back(digits[bytes[i] & 15]);
	}
	return result;
}

void EmitLiteral(const string& source, FundamentalType type,
	const vector<unsigned char>& bytes)
{
	cout << "literal " << source << " " << FundamentalName(type) << " " << HexDump(bytes) << endl;
}
void EmitArray(const string& source, size_t elements, FundamentalType type,
	const vector<unsigned char>& bytes)
{
	cout << "literal " << source << " array of " << elements << " " << FundamentalName(type) << " " << HexDump(bytes) << endl;
}
void EmitUserCharacter(const string& source, const string& suffix,
	FundamentalType type, const vector<unsigned char>& bytes)
{
	cout << "user-defined-literal " << source << " " << suffix << " character " << FundamentalName(type) << " " << HexDump(bytes) << endl;
}
void EmitUserString(const string& source, const string& suffix, size_t elements,
	FundamentalType type, const vector<unsigned char>& bytes)
{
	cout << "user-defined-literal " << source << " " << suffix << " string array of " << elements << " " << FundamentalName(type) << " " << HexDump(bytes) << endl;
}
void EmitUserNumber(const string& source, const string& suffix,
	const string& category, const string& prefix)
{
	cout << "user-defined-literal " << source << " " << suffix << " " << category << " " << prefix << endl;
}

enum Encoding { ENC_ORDINARY, ENC_UTF8, ENC_UTF16, ENC_UTF32, ENC_WCHAR };
struct QuotedFact
{
	Encoding encoding;
	string suffix;
	vector<int> values;
};

bool IsHex(int c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
bool IsOctal(int c) { return c >= '0' && c <= '7'; }
int HexDigit(int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return c - 'A' + 10;
}
bool IsSuffixBody(int c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		(c >= '0' && c <= '9') || c == '_' || c >= 0x80;
}

struct PrefixInfo
{
	size_t quote;
	bool raw;
	Encoding encoding;
};

PrefixInfo FindPrefix(const vector<int>& units, bool character)
{
	PrefixInfo result = {0, false, ENC_ORDINARY};
	if (!character && units.size() >= 2 && units[0] == 'R' && units[1] == '"')
		{ result.quote = 1; result.raw = true; return result; }
	if (!character && units.size() >= 3 && units[0] == 'u' && units[1] == '8')
	{
		result.encoding = ENC_UTF8;
		if (units[2] == 'R' && units.size() >= 4 && units[3] == '"') { result.quote = 3; result.raw = true; }
		else result.quote = 2;
		return result;
	}
	if (units.size() >= 2 && (units[0] == 'u' || units[0] == 'U' || units[0] == 'L'))
	{
		result.encoding = units[0] == 'u' ? ENC_UTF16 : (units[0] == 'U' ? ENC_UTF32 : ENC_WCHAR);
		if (units[1] == 'R' && units.size() >= 3 && units[2] == '"' && !character)
			{ result.quote = 2; result.raw = true; }
		else { result.quote = 1; result.raw = false; }
		return result;
	}
	return result;
}

size_t FindOrdinaryClose(const vector<int>& units, size_t quote, bool character)
{
	const int terminator = character ? '\'' : '"';
	for (size_t i = quote + 1; i < units.size(); ++i)
	{
		if (units[i] == '\\') { if (i + 1 >= units.size()) throw logic_error("unterminated escape"); ++i; continue; }
		if (units[i] == terminator) return i;
	}
	throw logic_error("unterminated quoted literal");
}

size_t FindRawClose(const vector<int>& units, size_t quote, size_t* body_begin)
{
	size_t open = quote + 1;
	while (open < units.size() && units[open] != '(') ++open;
	if (open == units.size() || open - quote - 1 > 16) throw logic_error("invalid raw string delimiter");
	const size_t delimiter_begin = quote + 1;
	const size_t delimiter_length = open - delimiter_begin;
	*body_begin = open + 1;
	for (size_t close = *body_begin; close < units.size(); ++close)
	{
		if (units[close] != ')') continue;
		const size_t end = close + 1 + delimiter_length;
		if (end >= units.size() || units[end] != '"') continue;
		bool match = true;
		for (size_t j = 0; j < delimiter_length; ++j)
			if (units[close + 1 + j] != units[delimiter_begin + j]) match = false;
		if (match) return close;
	}
	throw logic_error("unterminated raw string");
}

int SimpleEscape(int c)
{
	switch (c)
	{
	case '\'': return '\''; case '"': return '"'; case '?': return '?'; case '\\': return '\\';
	case 'a': return '\a'; case 'b': return '\b'; case 'f': return '\f'; case 'n': return '\n';
	case 'r': return '\r'; case 't': return '\t'; case 'v': return '\v'; default: return -1;
	}
}

vector<int> DecodeEscapes(const vector<int>& units, size_t begin, size_t end)
{
	vector<int> result;
	for (size_t i = begin; i < end; ++i)
	{
		if (units[i] != '\\') { result.push_back(units[i]); continue; }
		if (i + 1 >= end) throw logic_error("unterminated escape");
		const int next = units[++i];
		const int simple = SimpleEscape(next);
		if (simple >= 0) { result.push_back(simple); continue; }
		if (IsOctal(next))
		{
			int value = next - '0';
			size_t count = 1;
			while (count < 3 && i + 1 < end && IsOctal(units[i + 1]))
				{ value = value * 8 + (units[++i] - '0'); ++count; }
			result.push_back(value);
			continue;
		}
		if (next == 'x')
		{
			if (i + 1 >= end || !IsHex(units[i + 1])) throw logic_error("invalid hex escape");
			int value = 0;
			while (i + 1 < end && IsHex(units[i + 1])) value = value * 16 + HexDigit(units[++i]);
			result.push_back(value);
			continue;
		}
		throw logic_error("invalid escape");
	}
	for (size_t i = 0; i < result.size(); ++i)
		if (!PostIsValidCodePoint(result[i])) throw logic_error("invalid literal code point");
	return result;
}

QuotedFact ParseQuotedFact(const string& source, bool character)
{
	const vector<int> units = PostDecodeUTF8(source);
	const PrefixInfo prefix = FindPrefix(units, character);
	if (prefix.quote >= units.size() || units[prefix.quote] != (character ? '\'' : '"'))
		throw logic_error("invalid quoted literal");
	size_t body_begin = prefix.quote + 1;
	size_t body_end = 0;
	size_t end = 0;
	if (prefix.raw)
	{
		body_end = FindRawClose(units, prefix.quote, &body_begin);
		const size_t delimiter_length = body_begin - prefix.quote - 2;
		end = body_end + delimiter_length + 2;
	}
	else
	{
		body_end = FindOrdinaryClose(units, prefix.quote, character);
		end = body_end + 1;
	}
	QuotedFact result;
	result.encoding = prefix.encoding;
	if (end < units.size())
		for (size_t i = end; i < units.size(); ++i)
		{
			if (!IsSuffixBody(units[i])) throw logic_error("invalid user-defined suffix");
			result.suffix += PostEncodeUTF8(units[i]);
		}
	result.values = prefix.raw ? vector<int>(units.begin() + body_begin, units.begin() + body_end) : DecodeEscapes(units, body_begin, body_end);
	return result;
}

void AppendLE(vector<unsigned char>* bytes, unsigned long long value, size_t width)
{
	for (size_t i = 0; i < width; ++i) { bytes->push_back(static_cast<unsigned char>(value & 0xff)); value >>= 8; }
}

struct EncodedString
{
	FundamentalType type;
	size_t elements;
	vector<unsigned char> bytes;
};

EncodedString EncodeString(const vector<int>& values, Encoding encoding)
{
	EncodedString result;
	const size_t unit_width = encoding == ENC_UTF8 ? 1 : (encoding == ENC_UTF16 ? 2 : 4);
	result.type = encoding == ENC_UTF8 ? FT_CHAR : (encoding == ENC_UTF16 ? FT_CHAR16_T : (encoding == ENC_UTF32 ? FT_CHAR32_T : FT_WCHAR_T));
	for (size_t i = 0; i < values.size(); ++i)
	{
		const unsigned long long cp = static_cast<unsigned long long>(values[i]);
		if (encoding == ENC_UTF8)
		{
			const string encoded = PostEncodeUTF8(values[i]);
			result.bytes.insert(result.bytes.end(), encoded.begin(), encoded.end());
		}
		else if (encoding == ENC_UTF16)
		{
			if (values[i] <= 0xffff) AppendLE(&result.bytes, cp, 2);
			else
			{
				const unsigned long long n = cp - 0x10000;
				AppendLE(&result.bytes, 0xd800 + (n >> 10), 2);
				AppendLE(&result.bytes, 0xdc00 + (n & 0x3ff), 2);
			}
		}
		else AppendLE(&result.bytes, cp, unit_width);
	}
	AppendLE(&result.bytes, 0, unit_width);
	result.elements = result.bytes.size() / unit_width;
	return result;
}

void EmitQuotedCharacter(const PostPPToken& token)
{
	try
	{
		const QuotedFact fact = ParseQuotedFact(token.source, true);
		if (!fact.suffix.empty() && fact.suffix[0] != '_') throw logic_error("invalid character suffix");
		if (fact.values.size() != 1) throw logic_error("character literal must contain one code point");
		const int value = fact.values[0];
		FundamentalType type = FT_CHAR;
		size_t width = 1;
		if (fact.encoding == ENC_UTF16) { if (value > 0xffff) throw logic_error("character does not fit char16_t"); type = FT_CHAR16_T; width = 2; }
		else if (fact.encoding == ENC_UTF32) { type = FT_CHAR32_T; width = 4; }
		else if (fact.encoding == ENC_WCHAR) { type = FT_WCHAR_T; width = 4; }
		else if (value > 127) { type = FT_INT; width = 4; }
		vector<unsigned char> bytes;
		AppendLE(&bytes, static_cast<unsigned long long>(value), width);
		if (token.kind == POST_PP_USER_CHARACTER) EmitUserCharacter(token.source, fact.suffix, type, bytes);
		else if (fact.suffix.empty()) EmitLiteral(token.source, type, bytes);
		else throw logic_error("invalid character suffix");
	}
	catch (const exception&) { EmitInvalid(token.source); }
}

void EmitStringSequence(const vector<PostPPToken>& sequence)
{
	string source;
	for (size_t i = 0; i < sequence.size(); ++i)
	{
		if (i != 0) source += ' ';
		source += sequence[i].source;
	}
	try
	{
		vector<QuotedFact> facts;
		for (size_t i = 0; i < sequence.size(); ++i)
			facts.push_back(ParseQuotedFact(sequence[i].source, false));
		Encoding encoding = ENC_ORDINARY;
		string suffix;
		for (size_t i = 0; i < facts.size(); ++i)
		{
			if (facts[i].encoding == ENC_ORDINARY) {}
			else if (encoding == ENC_ORDINARY) encoding = facts[i].encoding;
			else if (facts[i].encoding != encoding)
				throw logic_error("incompatible string encodings");
			if (!facts[i].suffix.empty())
			{
				if (facts[i].suffix[0] != '_') throw logic_error("invalid string suffix");
				if (suffix.empty()) suffix = facts[i].suffix;
				else if (suffix != facts[i].suffix) throw logic_error("different string suffixes");
			}
		}
		vector<int> values;
		for (size_t i = 0; i < facts.size(); ++i)
			values.insert(values.end(), facts[i].values.begin(), facts[i].values.end());
		const Encoding effective = encoding == ENC_ORDINARY ? ENC_UTF8 : encoding;
		const EncodedString encoded = EncodeString(values, effective);
		if (suffix.empty()) EmitArray(source, encoded.elements, encoded.type, encoded.bytes);
		else EmitUserString(source, suffix, encoded.elements, encoded.type, encoded.bytes);
	}
	catch (const exception&) { EmitInvalid(source); }
}

struct IntegerSpec
{
	int base;
	string digits;
	bool unsig;
	int long_kind;
};

bool IsUnsignedMark(char c) { return c == 'u' || c == 'U'; }
bool IsLongMark(char c) { return c == 'l' || c == 'L'; }

bool ParseIntegerSpec(const string& source, bool allow_suffix, IntegerSpec* result)
{
	size_t end = 0;
	int base = 10;
	size_t digits_begin = 0;
	if (source.size() >= 2 && source[0] == '0' && (source[1] == 'x' || source[1] == 'X'))
	{
		base = 16; digits_begin = 2; end = digits_begin;
		while (end < source.size() && IsHex(static_cast<unsigned char>(source[end]))) ++end;
		if (end == digits_begin) return false;
	}
	else if (!source.empty() && source[0] == '0')
	{
		base = 8; end = 1;
		while (end < source.size() && IsOctal(static_cast<unsigned char>(source[end]))) ++end;
		if (end < source.size() && source[end] >= '8' && source[end] <= '9') return false;
	}
	else
	{
		if (source.empty() || source[0] < '1' || source[0] > '9') return false;
		end = 1;
		while (end < source.size() && source[end] >= '0' && source[end] <= '9') ++end;
	}
	string suffix = source.substr(end);
	if (!allow_suffix && !suffix.empty()) return false;
	bool unsig = false;
	int long_kind = 0;
	size_t p = 0;
	if (p < suffix.size() && IsUnsignedMark(suffix[p])) { unsig = true; ++p; }
	if (p < suffix.size() && IsLongMark(suffix[p]))
	{
		const char mark = suffix[p++];
		long_kind = 1;
		if (p < suffix.size() && suffix[p] == mark) { ++p; long_kind = 2; }
	}
	if (!unsig && p < suffix.size() && IsUnsignedMark(suffix[p])) { unsig = true; ++p; }
	if (p != suffix.size()) return false;
	if (!allow_suffix && (unsig || long_kind != 0)) return false;
	result->base = base;
	result->digits = source.substr(digits_begin, end - digits_begin);
	result->unsig = unsig;
	result->long_kind = long_kind;
	return true;
}

bool ParseUnsignedDigits(const string& digits, int base, unsigned long long* value)
{
	unsigned long long result = 0;
	for (size_t i = 0; i < digits.size(); ++i)
	{
		const int c = static_cast<unsigned char>(digits[i]);
		const unsigned long long digit = c <= '9' ? c - '0' : (c <= 'F' ? c - 'A' + 10 : c - 'a' + 10);
		if (digit >= static_cast<unsigned long long>(base) || result > (ULLONG_MAX - digit) / static_cast<unsigned long long>(base)) return false;
		result = result * static_cast<unsigned long long>(base) + digit;
	}
	*value = result;
	return true;
}

struct IntegerCandidate
{
	FundamentalType type;
	unsigned long long maximum;
	 size_t width;
};

vector<IntegerCandidate> IntegerCandidates(const IntegerSpec& spec)
{
	vector<IntegerCandidate> result;
	const unsigned long long signed32 = 0x7fffffffULL;
	const unsigned long long signed64 = 0x7fffffffffffffffULL;
	const unsigned long long unsigned32 = 0xffffffffULL;
	const unsigned long long unsigned64 = ULLONG_MAX;
	if (spec.unsig)
	{
		if (spec.long_kind == 0)
		{
			result.push_back({FT_UNSIGNED_INT, unsigned32, 4});
			result.push_back({FT_UNSIGNED_LONG_INT, unsigned64, 8});
			result.push_back({FT_UNSIGNED_LONG_LONG_INT, unsigned64, 8});
		}
		else if (spec.long_kind == 1)
		{
			result.push_back({FT_UNSIGNED_LONG_INT, unsigned64, 8});
			result.push_back({FT_UNSIGNED_LONG_LONG_INT, unsigned64, 8});
		}
		else result.push_back({FT_UNSIGNED_LONG_LONG_INT, unsigned64, 8});
	}
	else if (spec.long_kind == 0)
	{
		result.push_back({FT_INT, signed32, 4});
		if (spec.base != 10) result.push_back({FT_UNSIGNED_INT, unsigned32, 4});
		result.push_back({FT_LONG_INT, signed64, 8});
		if (spec.base != 10) result.push_back({FT_UNSIGNED_LONG_INT, unsigned64, 8});
		result.push_back({FT_LONG_LONG_INT, signed64, 8});
		if (spec.base != 10) result.push_back({FT_UNSIGNED_LONG_LONG_INT, unsigned64, 8});
	}
	else if (spec.long_kind == 1)
	{
		result.push_back({FT_LONG_INT, signed64, 8});
		if (spec.base != 10) result.push_back({FT_UNSIGNED_LONG_INT, unsigned64, 8});
		result.push_back({FT_LONG_LONG_INT, signed64, 8});
		if (spec.base != 10) result.push_back({FT_UNSIGNED_LONG_LONG_INT, unsigned64, 8});
	}
	else
	{
		result.push_back({FT_LONG_LONG_INT, signed64, 8});
		if (spec.base != 10) result.push_back({FT_UNSIGNED_LONG_LONG_INT, unsigned64, 8});
	}
	return result;
}

void EmitIntegerLiteral(const PostPPToken& token, const string& source)
{
	IntegerSpec spec;
	if (!ParseIntegerSpec(source, true, &spec)) { EmitInvalid(token.source); return; }
	unsigned long long value = 0;
	if (!ParseUnsignedDigits(spec.digits, spec.base, &value)) { EmitInvalid(token.source); return; }
	const vector<IntegerCandidate> candidates = IntegerCandidates(spec);
	for (size_t i = 0; i < candidates.size(); ++i)
		if (value <= candidates[i].maximum)
		{
			vector<unsigned char> bytes;
			AppendLE(&bytes, value, candidates[i].width);
			EmitLiteral(token.source, candidates[i].type, bytes);
			return;
		}
	EmitInvalid(token.source);
}

bool ParseFloatingCore(const string& source)
{
	if (source.size() >= 2 && source[0] == '0' && (source[1] == 'x' || source[1] == 'X'))
	{
		size_t i = 2;
		const size_t before = i;
		while (i < source.size() && IsHex(static_cast<unsigned char>(source[i]))) ++i;
		bool has_digits = i != before;
		if (i < source.size() && source[i] == '.')
		{
			++i;
			const size_t after = i;
			while (i < source.size() && IsHex(static_cast<unsigned char>(source[i]))) ++i;
			has_digits = has_digits || i != after;
		}
		if (!has_digits || i >= source.size() || (source[i] != 'p' && source[i] != 'P')) return false;
		++i;
		if (i < source.size() && (source[i] == '+' || source[i] == '-')) ++i;
		const size_t exponent = i;
		while (i < source.size() && source[i] >= '0' && source[i] <= '9') ++i;
		return i != exponent && i == source.size();
	}
	size_t i = 0;
	const size_t before = i;
	while (i < source.size() && source[i] >= '0' && source[i] <= '9') ++i;
	const bool before_digits = i != before;
	bool dot = false;
	bool after_digits = false;
	if (i < source.size() && source[i] == '.')
	{
		dot = true;
		++i;
		const size_t after = i;
		while (i < source.size() && source[i] >= '0' && source[i] <= '9') ++i;
		after_digits = i != after;
	}
	if (!before_digits && !after_digits) return false;
	if (i < source.size() && (source[i] == 'e' || source[i] == 'E'))
	{
		++i;
		if (i < source.size() && (source[i] == '+' || source[i] == '-')) ++i;
		const size_t exponent = i;
		while (i < source.size() && source[i] >= '0' && source[i] <= '9') ++i;
		if (i == exponent) return false;
		return i == source.size();
	}
	return dot && i == source.size();
}

bool ParseFloatingSpec(const string& source, string* core, char* suffix)
{
	*core = source;
	*suffix = 0;
	if (!source.empty() && (source[source.size() - 1] == 'f' || source[source.size() - 1] == 'F' || source[source.size() - 1] == 'l' || source[source.size() - 1] == 'L'))
	{
		*suffix = source[source.size() - 1];
		core->erase(core->size() - 1);
	}
	return ParseFloatingCore(*core);
}

float PA2DecodeFloat(const string& source)
{
	return ::strtof(source.c_str(), 0);
}
double PA2DecodeDouble(const string& source)
{
	return ::strtod(source.c_str(), 0);
}
long double PA2DecodeLongDouble(const string& source)
{
	return ::strtold(source.c_str(), 0);
}

template<typename T>
vector<unsigned char> ObjectBytes(const T& value)
{
	vector<unsigned char> bytes(sizeof(value));
	memcpy(bytes.data(), &value, sizeof(value));
	return bytes;
}

void EmitFloatingLiteral(const PostPPToken& token, const string& source)
{
	string core;
	char suffix = 0;
	if (!ParseFloatingSpec(source, &core, &suffix)) { EmitInvalid(token.source); return; }
	if (suffix == 'f' || suffix == 'F') EmitLiteral(token.source, FT_FLOAT, ObjectBytes(PA2DecodeFloat(core)));
	else if (suffix == 'l' || suffix == 'L') EmitLiteral(token.source, FT_LONG_DOUBLE, ObjectBytes(PA2DecodeLongDouble(core)));
	else EmitLiteral(token.source, FT_DOUBLE, ObjectBytes(PA2DecodeDouble(core)));
}

bool ValidUserSuffix(const string& suffix)
{
	if (suffix.empty() || suffix[0] != '_') return false;
	for (size_t i = 1; i < suffix.size(); ++i)
		if (!IsSuffixBody(static_cast<unsigned char>(suffix[i]))) return false;
	return true;
}

bool SplitUserNumber(const string& source, string* prefix, string* suffix)
{
	const size_t underscore = source.find('_');
	if (underscore == string::npos) return false;
	*prefix = source.substr(0, underscore);
	*suffix = source.substr(underscore);
	return ValidUserSuffix(*suffix);
}

void EmitNumber(const PostPPToken& token)
{
	string prefix;
	string suffix;
	if (SplitUserNumber(token.source, &prefix, &suffix))
	{
		IntegerSpec integer;
		string floating_core;
		char floating_suffix = 0;
		if (ParseIntegerSpec(prefix, false, &integer)) EmitUserNumber(token.source, suffix, "integer", prefix);
		else if (ParseFloatingSpec(prefix, &floating_core, &floating_suffix) && floating_suffix == 0) EmitUserNumber(token.source, suffix, "floating", prefix);
		else EmitInvalid(token.source);
		return;
	}
	string floating_core;
	char floating_suffix = 0;
	if (ParseFloatingSpec(token.source, &floating_core, &floating_suffix)) EmitFloatingLiteral(token, token.source);
	else EmitIntegerLiteral(token, token.source);
}

void EmitSingleToken(const PostPPToken& token)
{
	if (token.kind == POST_PP_IDENTIFIER)
	{
		const unordered_map<string, string>& simple = SimpleTokens();
		const unordered_map<string, string>::const_iterator found = simple.find(token.source);
		if (found == simple.end()) EmitIdentifier(token.source);
		else EmitSimple(token.source, found->second);
	}
	else if (token.kind == POST_PP_PUNCTUATOR)
	{
		if (token.source == "#" || token.source == "##" || token.source == "%:" || token.source == "%:%:") EmitInvalid(token.source);
		else
		{
			const unordered_map<string, string>& simple = SimpleTokens();
			const unordered_map<string, string>::const_iterator found = simple.find(token.source);
			if (found == simple.end()) EmitInvalid(token.source);
			else EmitSimple(token.source, found->second);
		}
	}
	else if (token.kind == POST_PP_NUMBER) EmitNumber(token);
	else if (token.kind == POST_PP_CHARACTER || token.kind == POST_PP_USER_CHARACTER) EmitQuotedCharacter(token);
	else if (token.kind == POST_PP_HEADER || token.kind == POST_PP_NON_WHITESPACE) EmitInvalid(token.source);
}

void RunPostTokenImpl(const vector<PostPPToken>& tokens)
{
	for (size_t i = 0; i < tokens.size();)
	{
		if (tokens[i].kind == POST_PP_STRING || tokens[i].kind == POST_PP_USER_STRING)
		{
			vector<PostPPToken> sequence;
			while (i < tokens.size() && (tokens[i].kind == POST_PP_STRING || tokens[i].kind == POST_PP_USER_STRING))
				sequence.push_back(tokens[i++]);
			EmitStringSequence(sequence);
			continue;
		}
		if (tokens[i].kind == POST_PP_EOF) { cout << "eof" << endl; return; }
		EmitSingleToken(tokens[i++]);
	}
	cout << "eof" << endl;
}

} // namespace

void RunPostToken(const vector<PostPPToken>& tokens)
{
	RunPostTokenImpl(tokens);
}

bool ValidatePostTokens(const vector<PostPPToken>& tokens)
{
	ValidationBuffer discarded;
	streambuf* old = cout.rdbuf(&discarded);
	try
	{
		RunPostToken(tokens);
	}
	catch (...)
	{
		cout.rdbuf(old);
		throw;
	}
	cout.rdbuf(old);
	return !discarded.invalid();
}
