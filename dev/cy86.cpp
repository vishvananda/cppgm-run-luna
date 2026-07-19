// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

// PA9 is deliberately implemented as a small, self-contained compiler.  The
// front end consumes the same post-preprocessor token stream as the earlier
// assignments; the back end lowers the typed CY86 model directly to a single
// Linux x86-64 load segment.  Keeping the CY86 facts in the structures below
// makes the parsing, layout, relocation, and instruction-selection phases
// independent of token spellings after parsing.

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

#include "exceptions.h"
#include "posttoken_unicode.h"
#include "preprocessor_engine.h"

static const uint64_t CY86_IMAGE_BASE = 0x400000ULL;
static const size_t ELF_HEADER_SIZE = 64;
static const size_t PROGRAM_HEADER_SIZE = 56;
static const size_t ELF_PREFIX_SIZE = ELF_HEADER_SIZE + PROGRAM_HEADER_SIZE;

struct ElfHeader
{
	unsigned char ident[16] =
	{
		0x7f, 'E', 'L', 'F', 2, 1, 1, 0, 0,
		0, 0, 0, 0, 0, 0, 0
	};

	short int type = 2;
	short int machine = 0x3e;
	int version = 1;
	long int entry;
	long int phoff = 64;
	long int shoff = 0;
	int processor_flags = 0;
	short int ehsize = 64;
	short int phentsize = 56;
	short int phnum = 1;
	short int shentsize = 0;
	short int shnum = 0;
	short int shstrndx = 0;
};

struct ProgramSegmentHeader
{
	int type = 1;
	static constexpr int executable = 1 << 0;
	static constexpr int writable = 1 << 1;
	static constexpr int readable = 1 << 2;
	int flags = executable | writable | readable;
	long int offset = 0;
	long int vaddr = static_cast<long int>(CY86_IMAGE_BASE);
	long int paddr = 0;
	long int filesz;
	long int memsz;
	long int align = 0x1000;
};

extern "C" long int syscall(long int n, ...) throw ();

bool PA9SetFileExecutable(const string& path)
{
	return syscall(/* chmod */ 90, path.c_str(), 0755) == 0;
}

namespace cy86 {

static uint64_t MaskForWidth(int width)
{
	if (width >= 64) return numeric_limits<uint64_t>::max();
	return (static_cast<uint64_t>(1) << width) - 1;
}

static uint64_t SignExtend(uint64_t value, int width)
{
	value &= MaskForWidth(width);
	if (width >= 64 || (value & (static_cast<uint64_t>(1) << (width - 1))) == 0)
		return value;
	return value | (~MaskForWidth(width));
}

static bool StartsWith(const string& value, const string& prefix)
{
	return value.size() >= prefix.size() &&
		value.compare(0, prefix.size(), prefix) == 0;
}

static int ExactWidthSuffix(const string& value, const string& prefix,
	bool allow_float80 = false)
{
	if (!StartsWith(value, prefix)) return 0;
	const string suffix = value.substr(prefix.size());
	if (suffix == "8") return 8;
	if (suffix == "16") return 16;
	if (suffix == "32") return 32;
	if (suffix == "64") return 64;
	if (allow_float80 && suffix == "80") return 80;
	return 0;
}

static int ExactFloatWidthSuffix(const string& value, const string& prefix)
{
	if (!StartsWith(value, prefix)) return 0;
	const string suffix = value.substr(prefix.size());
	if (suffix == "32") return 32;
	if (suffix == "64") return 64;
	if (suffix == "80") return 80;
	return 0;
}

enum CY86QuotedEncoding
{
	CY86_QUOTED_CHAR, CY86_QUOTED_UTF16, CY86_QUOTED_UTF32, CY86_QUOTED_WCHAR
};

struct Literal
{
	bool integral;
	bool floating;
	bool string_literal;
	bool signed_value;
	int width;
	uint64_t bits;
	long double real;
	vector<unsigned char> bytes;
	CY86QuotedEncoding string_encoding;
	vector<int> string_values;

	Literal()
		: integral(false), floating(false), string_literal(false),
		  signed_value(false), width(0), bits(0), real(0), bytes(),
		  string_encoding(CY86_QUOTED_CHAR), string_values()
	{}
};

static int HexValue(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

static bool IsOctal(char c) { return c >= '0' && c <= '7'; }

static size_t CY86QuotedPrefix(const vector<int>& units,
	CY86QuotedEncoding* encoding, bool* raw)
{
	*encoding = CY86_QUOTED_CHAR;
	*raw = false;
	if (units.empty()) return 0;
	if (units[0] == 'u' && units.size() > 1 && units[1] == '8')
	{
		if (units.size() > 2 && units[2] == 'R') { *raw = true; return 3; }
		return 2;
	}
	if (units[0] == 'u' || units[0] == 'U' || units[0] == 'L')
	{
		*encoding = units[0] == 'u' ? CY86_QUOTED_UTF16 :
			(units[0] == 'U' ? CY86_QUOTED_UTF32 : CY86_QUOTED_WCHAR);
		if (units.size() > 1 && units[1] == 'R') { *raw = true; return 2; }
		return 1;
	}
	if (units[0] == 'R') { *raw = true; return 1; }
	return 0;
}

static vector<int> CY86DecodeQuotedBody(const vector<int>& units,
	size_t begin, size_t end)
{
	vector<int> result;
	for (size_t i = begin; i < end; ++i)
	{
		if (units[i] != '\\')
		{
			result.push_back(units[i]);
			continue;
		}
		if (++i >= end) throw logic_error("invalid escape");
		const int next = units[i];
		switch (next)
		{
		case '\'': result.push_back('\''); continue;
		case '"': result.push_back('"'); continue;
		case '?': result.push_back('?'); continue;
		case '\\': result.push_back('\\'); continue;
		case 'a': result.push_back('\a'); continue;
		case 'b': result.push_back('\b'); continue;
		case 'f': result.push_back('\f'); continue;
		case 'n': result.push_back('\n'); continue;
		case 'r': result.push_back('\r'); continue;
		case 't': result.push_back('\t'); continue;
		case 'v': result.push_back('\v'); continue;
		case 'u':
		case 'U':
		{
			const size_t digits = next == 'u' ? 4 : 8;
			if (i + digits >= end) throw logic_error("invalid Unicode escape");
			int value = 0;
			for (size_t j = 0; j < digits; ++j)
			{
				const int digit = HexValue(static_cast<char>(units[++i]));
				if (digit < 0) throw logic_error("invalid Unicode escape");
				value = value * 16 + digit;
			}
			result.push_back(value);
			continue;
		}
		case 'x':
		{
			if (i + 1 >= end || HexValue(static_cast<char>(units[i + 1])) < 0)
				throw logic_error("invalid hexadecimal escape");
			int value = 0;
			while (i + 1 < end &&
				HexValue(static_cast<char>(units[i + 1])) >= 0)
				value = value * 16 + HexValue(static_cast<char>(units[++i]));
			result.push_back(value);
			continue;
		}
		default:
			if (!IsOctal(static_cast<char>(next))) throw logic_error("invalid escape");
		{
			int value = next - '0';
			for (int count = 1; count < 3 && i + 1 < end &&
				IsOctal(static_cast<char>(units[i + 1])); ++count)
				value = value * 8 + (units[++i] - '0');
			result.push_back(value);
		}
		}
	}
	for (size_t i = 0; i < result.size(); ++i)
		if (!PostIsValidCodePoint(result[i]))
			throw logic_error("invalid literal code point");
	return result;
}

static void CY86AppendLittleEndian(vector<unsigned char>* bytes,
	unsigned long long value, size_t width)
{
	for (size_t i = 0; i < width; ++i)
	{
		bytes->push_back(static_cast<unsigned char>(value & 0xff));
		value >>= 8;
	}
}

static vector<unsigned char> CY86EncodeStringValues(
	const vector<int>& values, CY86QuotedEncoding encoding)
{
	const size_t width = encoding == CY86_QUOTED_CHAR ? 1 :
		(encoding == CY86_QUOTED_UTF16 ? 2 : 4);
	vector<unsigned char> result;
	for (size_t i = 0; i < values.size(); ++i)
	{
		const unsigned long long value = static_cast<unsigned long long>(values[i]);
		if (encoding == CY86_QUOTED_CHAR)
		{
			const string encoded = PostEncodeUTF8(values[i]);
			result.insert(result.end(), encoded.begin(), encoded.end());
		}
		else if (encoding == CY86_QUOTED_UTF16 && values[i] > 0xffff)
		{
			const unsigned long long adjusted = value - 0x10000;
			CY86AppendLittleEndian(&result, 0xd800 + (adjusted >> 10), 2);
			CY86AppendLittleEndian(&result, 0xdc00 + (adjusted & 0x3ff), 2);
		}
		else CY86AppendLittleEndian(&result, value, width);
	}
	CY86AppendLittleEndian(&result, 0, width);
	return result;
}

static Literal ParseQuotedLiteralPA2(const string& source, bool character)
{
	const vector<int> units = PostDecodeUTF8(source);
	CY86QuotedEncoding encoding;
	bool raw = false;
	const size_t quote = CY86QuotedPrefix(units, &encoding, &raw);
	if (character && quote == 2)
		throw logic_error("UTF-8 character literal is not supported");
	if (quote >= units.size() || units[quote] != (character ? '\'' : '"'))
		throw logic_error("invalid quoted literal");
	vector<int> values;
	if (raw)
	{
		if (character) throw logic_error("raw character literal is not supported");
		size_t open = quote + 1;
		while (open < units.size() && units[open] != '(') ++open;
		if (open >= units.size()) throw logic_error("invalid raw literal");
		const size_t delimiter_begin = quote + 1;
		const size_t delimiter_length = open - delimiter_begin;
		size_t body_end = units.size();
		for (size_t close = open + 1; close < units.size(); ++close)
		{
			if (units[close] != ')') continue;
			if (close + delimiter_length + 1 >= units.size() ||
				units[close + delimiter_length + 1] != '"') continue;
			bool match = true;
			for (size_t i = 0; i < delimiter_length; ++i)
				if (units[close + 1 + i] != units[delimiter_begin + i]) match = false;
			if (match) { body_end = close; break; }
		}
		if (body_end == units.size()) throw logic_error("unterminated raw literal");
		values.assign(units.begin() + open + 1, units.begin() + body_end);
	}
	else
	{
		size_t body_end = units.size();
		for (size_t i = quote + 1; i < units.size(); ++i)
		{
			if (units[i] == '\\') { ++i; continue; }
			if (units[i] == (character ? '\'' : '"'))
			{
				body_end = i;
				break;
			}
		}
		if (body_end == units.size()) throw logic_error("unterminated quoted literal");
		values = CY86DecodeQuotedBody(units, quote + 1, body_end);
	}
	Literal result;
	if (character)
	{
		if (values.size() != 1) throw logic_error("invalid character literal");
		const int value = values[0];
		if (encoding == CY86_QUOTED_CHAR)
		{
			result.width = value <= 127 ? 1 : 4;
			result.signed_value = true;
		}
		else if (encoding == CY86_QUOTED_UTF16)
		{
			if (value > 0xffff) throw logic_error("character does not fit char16_t");
			result.width = 2;
			result.signed_value = false;
		}
		else
		{
			result.width = 4;
			result.signed_value = encoding == CY86_QUOTED_WCHAR;
		}
		result.integral = true;
		result.bits = static_cast<uint64_t>(value);
		return result;
	}
	result.string_literal = true;
	result.string_encoding = encoding;
	result.string_values = values;
	result.width = encoding == CY86_QUOTED_CHAR ? 1 :
		(encoding == CY86_QUOTED_UTF16 ? 2 : 4);
	result.bytes = CY86EncodeStringValues(values, encoding);
	return result;
}

static Literal ConcatenateStringLiterals(const vector<Literal>& parts)
{
	if (parts.empty()) throw logic_error("empty string literal sequence");
	CY86QuotedEncoding encoding = CY86_QUOTED_CHAR;
	for (size_t i = 0; i < parts.size(); ++i)
	{
		if (parts[i].string_encoding == CY86_QUOTED_CHAR) continue;
		if (encoding != CY86_QUOTED_CHAR && encoding != parts[i].string_encoding)
			throw logic_error("incompatible string literal encodings");
		encoding = parts[i].string_encoding;
	}
	vector<int> values;
	for (size_t i = 0; i < parts.size(); ++i)
		values.insert(values.end(), parts[i].string_values.begin(),
			parts[i].string_values.end());
	Literal result;
	result.string_literal = true;
	result.string_encoding = encoding;
	result.string_values = values;
	result.width = encoding == CY86_QUOTED_CHAR ? 1 :
		(encoding == CY86_QUOTED_UTF16 ? 2 : 4);
	result.bytes = CY86EncodeStringValues(values, encoding);
	return result;
}

static string IntegerCore(const string& source, string* suffix, int* base)
{
	*base = 10;
	size_t position = 0;
	if (source.size() >= 2 && source[0] == '0' &&
		(source[1] == 'x' || source[1] == 'X'))
	{
		*base = 16;
		position = 2;
		while (position < source.size() && HexValue(source[position]) >= 0)
			++position;
	}
	else if (!source.empty() && source[0] == '0')
	{
		*base = 8;
		position = 1;
		while (position < source.size() && IsOctal(source[position])) ++position;
	}
	else
	{
		while (position < source.size() && source[position] >= '0' &&
			source[position] <= '9') ++position;
	}
	*suffix = source.substr(position);
	return source.substr(0, position);
}

static Literal ParseNumberLiteral(const string& source)
{
	Literal result;
	const bool hexadecimal = source.size() >= 2 && source[0] == '0' &&
		(source[1] == 'x' || source[1] == 'X');
	const bool looks_float = source.find('.') != string::npos ||
		(hexadecimal ? source.find('p') != string::npos ||
			source.find('P') != string::npos :
			source.find('e') != string::npos || source.find('E') != string::npos ||
			source.find('p') != string::npos || source.find('P') != string::npos);
	if (looks_float)
	{
		string core = source;
		char suffix = 0;
		if (!core.empty() && (core[core.size() - 1] == 'f' ||
			core[core.size() - 1] == 'F' || core[core.size() - 1] == 'l' ||
			core[core.size() - 1] == 'L'))
		{
			suffix = core[core.size() - 1];
			core.erase(core.size() - 1);
		}
		char* end = NULL;
		errno = 0;
		result.real = strtold(core.c_str(), &end);
		if (errno == ERANGE || end == core.c_str() || *end != '\0')
			throw logic_error("invalid floating literal");
		result.floating = true;
		result.width = suffix == 'f' || suffix == 'F' ? 4 :
			suffix == 'l' || suffix == 'L' ? 16 : 8;
		return result;
	}

	string suffix;
	int base = 10;
	const string core = IntegerCore(source, &suffix, &base);
	if (core.empty() || (base == 16 && core == "0x"))
		throw logic_error("invalid integer literal");
	char* end = NULL;
	errno = 0;
	const uint64_t value = strtoull(core.c_str(), &end, base);
	if (errno == ERANGE || end == core.c_str() || *end != '\0')
		throw logic_error("invalid integer literal");
	bool unsig = false;
	int longs = 0;
	size_t suffix_position = 0;
	if (suffix_position < suffix.size() &&
		(suffix[suffix_position] == 'u' || suffix[suffix_position] == 'U'))
	{
		unsig = true;
		++suffix_position;
	}
	if (suffix_position < suffix.size() &&
		(suffix[suffix_position] == 'l' || suffix[suffix_position] == 'L'))
	{
		const char mark = suffix[suffix_position++];
		longs = 1;
		if (suffix_position < suffix.size() && suffix[suffix_position] == mark)
		{
			++suffix_position;
			longs = 2;
		}
	}
	if (!unsig && suffix_position < suffix.size() &&
		(suffix[suffix_position] == 'u' || suffix[suffix_position] == 'U'))
	{
		unsig = true;
		++suffix_position;
	}
	if (suffix_position != suffix.size()) throw logic_error("invalid integer suffix");
	result.integral = true;
	result.bits = value;
	if (unsig && longs == 0 && value <= 0xffffffffULL)
	{
		result.width = 4;
		result.signed_value = false;
	}
	else if (unsig)
	{
		result.width = 8;
		result.signed_value = false;
	}
	else if (longs == 2)
	{
		if (value > 0x7fffffffffffffffULL)
			throw logic_error("integer literal is out of range");
		result.width = 8;
		result.signed_value = true;
	}
	else if (longs == 1)
	{
		if (value <= 0x7fffffffffffffffULL)
		{
			result.width = 8;
			result.signed_value = true;
		}
		else if (base != 10)
		{
			result.width = 8;
			result.signed_value = false;
		}
		else throw logic_error("integer literal is out of range");
	}
	else if (value <= 0x7fffffffULL)
	{
		result.width = 4;
		result.signed_value = true;
	}
	else if (base != 10 && value <= 0xffffffffULL)
	{
		result.width = 4;
		result.signed_value = false;
	}
	else
	{
		if (base == 10) throw logic_error("integer literal is out of range");
		result.width = 8;
		result.signed_value = false;
	}
	result.bits &= MaskForWidth(result.width * 8);
	return result;
}

static Literal ParseLiteral(const PostPPToken& token, bool negative = false)
{
	Literal result;
	if (token.kind == POST_PP_USER_CHARACTER || token.kind == POST_PP_USER_STRING)
		throw logic_error("user-defined literals are not valid in CY86");
	if (token.kind == POST_PP_CHARACTER)
		result = ParseQuotedLiteralPA2(token.source, true);
	else if (token.kind == POST_PP_STRING)
		result = ParseQuotedLiteralPA2(token.source, false);
	else if (token.kind == POST_PP_NUMBER)
		result = ParseNumberLiteral(token.source);
	else throw logic_error("expected CY86 literal");
	if (negative)
	{
		if (!result.integral || result.string_literal)
			throw logic_error("negative non-integral CY86 literal");
		result.bits = (static_cast<uint64_t>(0) - result.bits) &
			MaskForWidth(result.width * 8);
	}
	return result;
}

static vector<unsigned char> EncodeFloating(const Literal& literal)
{
	vector<unsigned char> result(static_cast<size_t>(literal.width), 0);
	if (literal.width == 4)
	{
		const float value = static_cast<float>(literal.real);
		memcpy(&result[0], &value, 4);
	}
	else if (literal.width == 8)
	{
		const double value = static_cast<double>(literal.real);
		memcpy(&result[0], &value, 8);
	}
	else
	{
		const long double value = literal.real;
		memcpy(&result[0], &value, min(sizeof(value), result.size()));
	}
	return result;
}

static vector<unsigned char> EncodeInteger(const Literal& literal)
{
	vector<unsigned char> result(static_cast<size_t>(literal.width), 0);
	uint64_t value = literal.bits;
	for (size_t i = 0; i < result.size(); ++i)
	{
		result[i] = static_cast<unsigned char>(value & 0xff);
		value >>= 8;
	}
	return result;
}

static vector<unsigned char> EncodeStaticLiteral(const Literal& literal)
{
	if (literal.string_literal) return literal.bytes;
	if (literal.floating) return EncodeFloating(literal);
	return EncodeInteger(literal);
}

struct RegisterDesc
{
	bool valid;
	int machine;
	int width;

	RegisterDesc(bool valid = false, int machine = -1, int width = 0)
		: valid(valid), machine(machine), width(width)
	{}
};

static RegisterDesc GetRegister(const string& name)
{
	if (name == "sp") return RegisterDesc(true, 4, 64);
	if (name == "bp") return RegisterDesc(true, 5, 64);
	if (name.size() >= 2 && (name[0] == 'x' || name[0] == 'y' ||
		name[0] == 'z' || name[0] == 't'))
	{
		int width = 0;
		const string suffix = name.substr(1);
		if (suffix == "8") width = 8;
		if (suffix == "16") width = 16;
		if (suffix == "32") width = 32;
		if (suffix == "64") width = 64;
		if (width == 0) return RegisterDesc();
		const int machine = name[0] == 'x' ? 12 : name[0] == 'y' ? 13 :
			name[0] == 'z' ? 14 : 15;
		return RegisterDesc(true, machine, width);
	}
	return RegisterDesc();
}

static bool IsReservedKeyword(const string& name)
{
	static const set<string> keywords = {
		"alignas", "alignof", "asm", "auto", "bool", "break", "case",
		"catch", "char", "char16_t", "char32_t", "class", "const",
		"constexpr", "const_cast", "continue", "decltype", "default",
		"delete", "do", "double", "dynamic_cast", "else", "enum",
		"explicit", "export", "extern", "false", "float", "for",
		"friend", "goto", "if", "inline", "int", "long", "mutable",
		"namespace", "new", "noexcept", "nullptr", "operator", "private",
		"protected", "public", "register", "reinterpret_cast", "return",
		"short", "signed", "sizeof", "static", "static_assert",
		"static_cast", "struct", "switch", "template", "this",
		"thread_local", "throw", "true", "try", "typedef", "typeid",
		"typename", "union", "unsigned", "using", "virtual", "void",
		"volatile", "wchar_t", "while", "and", "and_eq", "bitand",
		"bitor", "compl", "not", "not_eq", "or", "or_eq", "xor",
		"xor_eq"
	};
	return keywords.find(name) != keywords.end();
}

enum ExprKind { EXPR_LITERAL, EXPR_LABEL, EXPR_REGISTER };

struct Expr
{
	ExprKind kind;
	Literal literal;
	string label;
	int reg;
	int reg_width;
	int64_t addend;

	Expr()
		: kind(EXPR_LITERAL), literal(), label(), reg(-1), reg_width(0), addend(0)
	{}
};

enum OperandKind { OPERAND_REGISTER, OPERAND_IMMEDIATE, OPERAND_MEMORY };

struct Operand
{
	OperandKind kind;
	int reg;
	int reg_width;
	Expr expr;

	Operand() : kind(OPERAND_IMMEDIATE), reg(-1), reg_width(0), expr() {}
};

struct LabelInfo
{
	bool defined;
	bool assigned;
	bool code;
	uint64_t address;

	LabelInfo()
		: defined(false), assigned(false), code(false), address(0)
	{}
};

enum Family
{
	F_DATA, F_MOVE, F_JUMP, F_JUMPIF, F_CALL, F_RET, F_NOT, F_LOGIC,
	F_SHIFT, F_TO_FLOAT, F_FROM_FLOAT, F_INT_ARITH, F_FLOAT_ARITH,
	F_INT_COMPARE, F_FLOAT_COMPARE, F_SYSCALL
};

struct OpInfo
{
	Family family;
	int width;
	int source_width;
	int count;
	bool signed_op;
	string condition;

	OpInfo()
		: family(F_DATA), width(0), source_width(0), count(0), signed_op(false),
		  condition()
	{}
};

static OpInfo DescribeOpcode(const string& opcode)
{
	OpInfo result;
	if (opcode == "ret")
	{
		result.family = F_RET;
		return result;
	}
	if (opcode == "jump")
	{
		result.family = F_JUMP; result.width = 64; result.count = 1; return result;
	}
	if (opcode == "jumpif")
	{
		result.family = F_JUMPIF; result.width = 8; result.source_width = 64;
		result.count = 2; return result;
	}
	if (opcode == "call")
	{
		result.family = F_CALL; result.width = 64; result.count = 1; return result;
	}
	const int data_width = ExactWidthSuffix(opcode, "data");
	if (data_width != 0)
	{
		result.family = F_DATA; result.width = data_width;
		result.count = 1; return result;
	}
	const int move_width = ExactWidthSuffix(opcode, "move", true);
	if (move_width != 0)
	{
		result.family = F_MOVE; result.width = move_width;
		result.count = 2; return result;
	}
	const int not_width = ExactWidthSuffix(opcode, "not");
	if (not_width != 0)
	{
		result.family = F_NOT; result.width = not_width;
		result.count = 2; return result;
	}
	const char* logic_names[] = {"and", "or", "xor"};
	for (size_t i = 0; i < sizeof(logic_names) / sizeof(logic_names[0]); ++i)
	{
		const int width = ExactWidthSuffix(opcode, logic_names[i]);
		if (width != 0)
		{
			result.family = F_LOGIC; result.width = width;
			result.count = 3; return result;
		}
	}
	const int lshift_width = ExactWidthSuffix(opcode, "lshift");
	const int srshift_width = ExactWidthSuffix(opcode, "srshift");
	const int urshift_width = ExactWidthSuffix(opcode, "urshift");
	if (lshift_width != 0 || srshift_width != 0 || urshift_width != 0)
	{
		result.family = F_SHIFT;
		result.width = lshift_width != 0 ? lshift_width :
			srshift_width != 0 ? srshift_width : urshift_width;
		result.count = 3; result.signed_op = srshift_width != 0;
		return result;
	}
	const string to_float_suffix = "convf80";
	const size_t to_float = opcode.find(to_float_suffix);
	if (to_float != string::npos && to_float > 0 &&
		to_float + to_float_suffix.size() == opcode.size())
	{
		const string prefix = opcode.substr(0, to_float);
		if (prefix.size() < 2) throw logic_error("invalid conversion opcode");
		const char kind = prefix[0];
		const int source_width = ExactWidthSuffix(prefix, string(1, kind));
		if ((kind != 's' && kind != 'u' && kind != 'f') ||
			source_width == 0 || (kind == 'f' && source_width != 32 &&
				source_width != 64))
			throw logic_error("invalid conversion opcode");
		result.family = F_TO_FLOAT;
		result.source_width = source_width;
		result.width = 80;
		result.count = 2;
		result.signed_op = kind == 's';
		return result;
	}
	if (StartsWith(opcode, "f80conv"))
	{
		const string suffix = opcode.substr(7);
		if (suffix.size() < 2) throw logic_error("invalid conversion opcode");
		const char kind = suffix[0];
		const int destination_width = ExactWidthSuffix(suffix, string(1, kind));
		if ((kind != 's' && kind != 'u' && kind != 'f') ||
			destination_width == 0 || (kind == 'f' && destination_width != 32 &&
				destination_width != 64))
			throw logic_error("invalid conversion opcode");
		result.family = F_FROM_FLOAT;
		result.source_width = 80;
		result.width = destination_width;
		result.count = 2;
		result.signed_op = kind == 's';
		return result;
	}
	const char* int_arith[] = {"iadd", "isub", "smul", "umul", "sdiv",
		"udiv", "smod", "umod"};
	for (size_t i = 0; i < sizeof(int_arith) / sizeof(int_arith[0]); ++i)
	{
		const int width = ExactWidthSuffix(opcode, int_arith[i]);
		if (width != 0)
		{
			result.family = F_INT_ARITH; result.width = width;
			result.count = 3;
			result.signed_op = int_arith[i][0] == 's';
			return result;
		}
	}
	const char* float_arith[] = {"fadd", "fsub", "fmul", "fdiv"};
	for (size_t i = 0; i < sizeof(float_arith) / sizeof(float_arith[0]); ++i)
	{
		const int width = ExactFloatWidthSuffix(opcode, float_arith[i]);
		if (width != 0)
		{
			result.family = F_FLOAT_ARITH; result.width = width;
			result.count = 3; return result;
		}
	}
	const char* int_compare[] = {"ieq", "ine", "slt", "ult", "sgt", "ugt",
		"sle", "ule", "sge", "uge"};
	const char* int_conditions[] = {"e", "ne", "l", "b", "g", "a", "le", "be", "ge", "ae"};
	for (size_t i = 0; i < sizeof(int_compare) / sizeof(int_compare[0]); ++i)
	{
		const int width = ExactWidthSuffix(opcode, int_compare[i]);
		if (width != 0)
		{
			result.family = F_INT_COMPARE; result.width = width;
			result.count = 3; result.condition = int_conditions[i];
			return result;
		}
	}
	const char* float_compare[] = {"feq", "fne", "flt", "fgt", "fle", "fge"};
	const char* float_conditions[] = {"e", "ne", "b", "a", "be", "ae"};
	for (size_t i = 0; i < sizeof(float_compare) / sizeof(float_compare[0]); ++i)
	{
		const int width = ExactFloatWidthSuffix(opcode, float_compare[i]);
		if (width != 0)
		{
			result.family = F_FLOAT_COMPARE; result.width = width;
			result.count = 3; result.condition = float_conditions[i];
			return result;
		}
	}
	if (StartsWith(opcode, "syscall"))
	{
		const string number = opcode.substr(7);
		if (number.size() != 1 || number[0] < '0' || number[0] > '6')
			throw logic_error("invalid syscall opcode");
		result.family = F_SYSCALL; result.width = 64;
		result.count = static_cast<int>(number[0] - '0') + 2;
		return result;
	}
	throw logic_error("unknown CY86 opcode: " + opcode);
}

static bool IsKnownOpcode(const string& opcode)
{
	try { DescribeOpcode(opcode); return true; }
	catch (const exception&) { return false; }
}

struct Statement
{
	vector<string> labels;
	string opcode;
	vector<Operand> operands;
	bool static_literal;
	Literal literal;
	OpInfo info;
	bool is_data;
	size_t data_offset;

	Statement()
		: labels(), opcode(), operands(), static_literal(false), literal(), info(),
		  is_data(false), data_offset(0)
	{}
};

class Parser
{
public:
	Parser(const vector<PostPPToken>& tokens, map<string, LabelInfo>* labels)
		: tokens_(tokens), position_(0), labels_(labels), statements_()
	{}

	vector<Statement> Parse()
	{
		while (position_ < tokens_.size())
		{
			Statement statement;
			while (IsIdentifier(Peek()) && Peek(1).source == ":")
			{
				const string name = Peek().source;
				DefineLabel(name);
				statement.labels.push_back(name);
				position_ += 2;
			}
			if (position_ >= tokens_.size())
				throw logic_error("label without statement");
			if (IsLiteral(Peek()) || (Peek().source == "-" &&
				IsLiteral(Peek(1))))
			{
				bool negative = false;
				if (Peek().source == "-") { negative = true; ++position_; }
				statement.static_literal = true;
				if (!negative && Peek().kind == POST_PP_STRING)
					statement.literal = ParseStringSequence();
				else
				{
					statement.literal = ParseLiteral(Peek(), negative);
					++position_;
				}
			}
			else if (IsIdentifier(Peek()))
			{
				statement.opcode = Peek().source;
				++position_;
				while (position_ < tokens_.size() && Peek().source != ";")
					statement.operands.push_back(ParseOperand());
			}
			else throw logic_error("expected CY86 statement");
			if (position_ >= tokens_.size() || Peek().source != ";")
				throw logic_error("expected semicolon");
			++position_;
			statements_.push_back(statement);
		}
		return statements_;
	}

private:
	const vector<PostPPToken>& tokens_;
	size_t position_;
	map<string, LabelInfo>* labels_;
	vector<Statement> statements_;

	const PostPPToken& Peek(size_t offset = 0) const
	{
		static const PostPPToken eof(POST_PP_EOF);
		return position_ + offset < tokens_.size() ? tokens_[position_ + offset] : eof;
	}

	static bool IsIdentifier(const PostPPToken& token)
	{
		return token.kind == POST_PP_IDENTIFIER;
	}

	static bool IsLiteral(const PostPPToken& token)
	{
		return token.kind == POST_PP_NUMBER || token.kind == POST_PP_CHARACTER ||
			token.kind == POST_PP_STRING || token.kind == POST_PP_USER_CHARACTER ||
			token.kind == POST_PP_USER_STRING;
	}

	Literal ParseStringSequence()
	{
		vector<Literal> parts;
		while (Peek().kind == POST_PP_STRING)
		{
			parts.push_back(ParseLiteral(Peek()));
			++position_;
		}
		return ConcatenateStringLiterals(parts);
	}

	void DefineLabel(const string& name)
	{
		if (IsReservedKeyword(name) || GetRegister(name).valid ||
			IsKnownOpcode(name))
			throw logic_error("label collides with CY86 name: " + name);
		LabelInfo& info = (*labels_)[name];
		if (info.defined) throw logic_error("duplicate CY86 label: " + name);
		info.defined = true;
	}

	Expr ParsePrimary(bool allow_register)
	{
		if (IsLiteral(Peek()))
		{
			Expr result;
			result.kind = EXPR_LITERAL;
			result.literal = ParseLiteral(Peek());
			++position_;
			return result;
		}
		if (!IsIdentifier(Peek())) throw logic_error("expected CY86 operand");
		const string name = Peek().source;
		++position_;
		const RegisterDesc reg = GetRegister(name);
		if (reg.valid && allow_register)
		{
			Expr result;
			result.kind = EXPR_REGISTER;
			result.reg = reg.machine;
			result.reg_width = reg.width;
			return result;
		}
		if (reg.valid) throw logic_error("register is not an immediate");
		Expr result;
		result.kind = EXPR_LABEL;
		result.label = name;
		return result;
	}

	static int64_t LiteralOffset(const Literal& literal)
	{
		if (!literal.integral || literal.string_literal)
			throw logic_error("memory/immediate offset is not integral");
		if (literal.signed_value)
			return static_cast<int64_t>(SignExtend(literal.bits, literal.width * 8));
		return static_cast<int64_t>(literal.bits);
	}

	Expr ParseImmediate()
	{
		if (Peek().source == "-" && IsLiteral(Peek(1)))
		{
			++position_;
			Expr result;
			result.kind = EXPR_LITERAL;
			result.literal = ParseLiteral(Peek(), true);
			++position_;
			return result;
		}
		if (Peek().source == "(")
		{
			++position_;
			Expr result;
			if (Peek().source == "-" && IsLiteral(Peek(1)))
			{
				++position_;
				result.kind = EXPR_LITERAL;
				result.literal = ParseLiteral(Peek(), true);
				++position_;
			}
			else result = ParsePrimary(false);
			if (Peek().source == "+" || Peek().source == "-")
			{
				if (result.kind != EXPR_LABEL)
					throw logic_error("immediate offset requires a label");
				const bool minus = Peek().source == "-";
				++position_;
				if (!IsLiteral(Peek())) throw logic_error("expected immediate offset");
				const int64_t amount = LiteralOffset(ParseLiteral(Peek()));
				result.addend += minus ? -amount : amount;
				++position_;
			}
			if (Peek().source != ")") throw logic_error("expected ')' in immediate");
			++position_;
			return result;
		}
		return ParsePrimary(false);
	}

	Operand ParseOperand()
	{
		Operand result;
		if (Peek().source == "[")
		{
			++position_;
			result.kind = OPERAND_MEMORY;
			result.expr = ParsePrimary(true);
			if (Peek().source == "+" || Peek().source == "-")
			{
				if (result.expr.kind != EXPR_REGISTER &&
					result.expr.kind != EXPR_LABEL)
					throw logic_error("memory offset requires a register or label");
				const bool minus = Peek().source == "-";
				++position_;
				if (!IsLiteral(Peek())) throw logic_error("expected memory offset");
				const int64_t amount = LiteralOffset(ParseLiteral(Peek()));
				result.expr.addend += minus ? -amount : amount;
				++position_;
			}
			if (Peek().source != "]") throw logic_error("expected ']' in memory operand");
			++position_;
			return result;
		}
		if (IsIdentifier(Peek()))
		{
			const RegisterDesc reg = GetRegister(Peek().source);
			if (reg.valid)
			{
				result.kind = OPERAND_REGISTER;
				result.reg = reg.machine;
				result.reg_width = reg.width;
				++position_;
				return result;
			}
		}
		result.kind = OPERAND_IMMEDIATE;
		result.expr = ParseImmediate();
		return result;
	}
};

static void CheckAddressOperand(const Operand& operand)
{
	if (operand.kind == OPERAND_REGISTER)
	{
		if (operand.reg < 0 || operand.reg_width != 64)
			throw logic_error("address register is not 64-bit");
	}
	if (operand.kind == OPERAND_MEMORY && operand.expr.kind == EXPR_REGISTER)
	{
		if (operand.expr.reg_width != 64)
			throw logic_error("memory base register is not 64-bit");
	}
}

static void CheckOperand(const Operand& operand, int width, bool destination,
	bool floating)
{
	if (destination && operand.kind == OPERAND_IMMEDIATE)
		throw logic_error("immediate cannot be written");
	if (operand.kind == OPERAND_REGISTER)
	{
		if (floating || operand.reg_width != width)
			throw logic_error("CY86 register width mismatch");
	}
	if (floating && operand.kind == OPERAND_IMMEDIATE)
		throw logic_error("floating operand must be memory");
	if (width == 80 && operand.kind != OPERAND_MEMORY)
		throw logic_error("80-bit operand must be memory");
	if (operand.kind == OPERAND_MEMORY) CheckAddressOperand(operand);
}

class Compiler
{
public:
	Compiler(const vector<PostPPToken>& tokens)
		: labels_(), statements_(), data_relocations_(), relocations_(),
		  internal_labels_(), internal_label_serial_(0), image_(),
		  first_statement_address_(0),
		  first_statement_seen_(false)
	{
		Parser parser(tokens, &labels_);
		statements_ = parser.Parse();
		for (size_t i = 0; i < statements_.size(); ++i)
		{
			if (statements_[i].static_literal) continue;
			statements_[i].info = DescribeOpcode(statements_[i].opcode);
			try
			{
				ValidateStatement(statements_[i]);
			}
			catch (const exception& exception)
			{
				throw logic_error(statements_[i].opcode + ": " + exception.what());
			}
			statements_[i].is_data = statements_[i].info.family == F_DATA;
		}
	}

	vector<unsigned char> Build()
	{
		LayoutData();
		EmitCode();
		PatchRelocations();
		if (!first_statement_seen_) throw logic_error("empty CY86 program");
		uint64_t entry = first_statement_address_;
		map<string, LabelInfo>::const_iterator start = labels_.find("start");
		if (start != labels_.end())
		{
			if (!start->second.assigned) throw logic_error("unresolved start label");
			entry = start->second.address;
		}
		ElfHeader elf;
		elf.entry = static_cast<long int>(entry);
		ProgramSegmentHeader segment;
		segment.filesz = static_cast<long int>(image_.size());
		segment.memsz = segment.filesz;
		if (image_.size() < ELF_PREFIX_SIZE) throw logic_error("short ELF image");
		memcpy(&image_[0], &elf, sizeof(elf));
		memcpy(&image_[ELF_HEADER_SIZE], &segment, sizeof(segment));
		return image_;
	}

private:
	struct DataRelocation
	{
		size_t offset;
		int width;
		Expr expr;
	};
	struct Relocation
	{
		size_t offset;
		string target;
		int64_t addend;
		bool relative;
		size_t base_after;
	};

	map<string, LabelInfo> labels_;
	vector<Statement> statements_;
	vector<DataRelocation> data_relocations_;
	vector<Relocation> relocations_;
	map<string, size_t> internal_labels_;
	unsigned long internal_label_serial_;
	vector<unsigned char> image_;
	uint64_t first_statement_address_;
	bool first_statement_seen_;

	void ValidateStatement(Statement& statement)
	{
		const OpInfo& info = statement.info;
		if (statement.operands.size() != static_cast<size_t>(info.count))
			throw logic_error("wrong CY86 operand count for " + statement.opcode);
		// Width facts are reconstructed from the token stream in the practical
		// parser below; all memory operands are width-polymorphic and are checked
		// by their opcode descriptor.
		if (info.family == F_RET) return;
		if (info.family == F_DATA)
		{
			if (statement.operands[0].kind != OPERAND_IMMEDIATE)
				throw logic_error("data operand is not immediate");
			return;
		}
		if (info.family == F_JUMP || info.family == F_CALL)
		{
			CheckAddressOperand(statement.operands[0]);
			return;
		}
		if (info.family == F_JUMPIF)
		{
			CheckOperand(statement.operands[0], 8, false, false);
			CheckAddressOperand(statement.operands[1]);
			return;
		}
		if (info.family == F_MOVE && info.width == 80)
		{
			if (statement.operands[0].kind != OPERAND_MEMORY ||
				statement.operands[1].kind != OPERAND_MEMORY)
				throw logic_error("move80 requires memory operands");
			CheckAddressOperand(statement.operands[0]);
			CheckAddressOperand(statement.operands[1]);
			return;
		}
		if (info.family == F_TO_FLOAT)
		{
			if (statement.operands[0].kind != OPERAND_MEMORY)
				throw logic_error("floating result is not memory");
			CheckAddressOperand(statement.operands[0]);
			CheckOperand(statement.operands[1], info.source_width, false,
				statement.opcode[0] == 'f');
			if (statement.opcode[0] == 'f' && statement.operands[1].kind != OPERAND_MEMORY)
				throw logic_error("floating source is not memory");
			return;
		}
		if (info.family == F_FROM_FLOAT)
		{
			if (statement.opcode[7] == 'f')
			{
				if (statement.operands[0].kind != OPERAND_MEMORY)
					throw logic_error("floating result is not memory");
			}
			else CheckOperand(statement.operands[0], info.width, true, false);
			if (statement.operands[1].kind != OPERAND_MEMORY)
				throw logic_error("floating source is not memory");
			CheckAddressOperand(statement.operands[1]);
			return;
		}
		if (info.family == F_FLOAT_ARITH || info.family == F_FLOAT_COMPARE)
		{
			if (info.family == F_FLOAT_COMPARE)
				CheckOperand(statement.operands[0], 8, true, false);
			else if (statement.operands[0].kind != OPERAND_MEMORY)
				throw logic_error("floating result is not memory");
			for (size_t i = 1; i < statement.operands.size(); ++i)
			{
				if (statement.operands[i].kind != OPERAND_MEMORY)
					throw logic_error("floating source is not memory");
				CheckAddressOperand(statement.operands[i]);
			}
			return;
		}
		if (info.family == F_SYSCALL)
		{
			CheckOperand(statement.operands[0], 64, true, false);
			for (size_t i = 1; i < statement.operands.size(); ++i)
				CheckOperand(statement.operands[i], 64, false, false);
			return;
		}
		if (info.family == F_NOT || info.family == F_LOGIC ||
			info.family == F_SHIFT || info.family == F_INT_ARITH ||
			info.family == F_INT_COMPARE)
		{
			const int destination_width = info.family == F_INT_COMPARE ? 8 : info.width;
			CheckOperand(statement.operands[0], destination_width, true, false);
			for (size_t i = 1; i < statement.operands.size(); ++i)
				CheckOperand(statement.operands[i],
					info.family == F_SHIFT && i == 2 ? 8 : info.width,
					false, false);
			return;
		}
		if (info.family == F_MOVE)
		{
			CheckOperand(statement.operands[0], info.width, true, false);
			CheckOperand(statement.operands[1], info.width, false, false);
			return;
		}
	}

	static void AppendZeros(vector<unsigned char>* image, size_t count)
	{
		image->insert(image->end(), count, 0);
	}

	static void Align(vector<unsigned char>* image, size_t alignment)
	{
		if (alignment == 0) throw logic_error("invalid CY86 alignment");
		while (image->size() % alignment != 0) image->push_back(0);
	}

	void AssignDataLabels(Statement& statement, uint64_t address)
	{
		for (size_t i = 0; i < statement.labels.size(); ++i)
		{
			LabelInfo& label = labels_[statement.labels[i]];
			label.assigned = true;
			label.code = false;
			label.address = address;
		}
	}

	void LayoutData()
	{
		image_.assign(ELF_PREFIX_SIZE, 0);
		for (size_t i = 0; i < statements_.size(); ++i)
		{
			Statement& statement = statements_[i];
			if (statement.static_literal || statement.is_data)
			{
				for (size_t j = 0; j < statement.labels.size(); ++j)
					labels_[statement.labels[j]].code = false;
			}
			else
			{
				for (size_t j = 0; j < statement.labels.size(); ++j)
				{
					labels_[statement.labels[j]].code = true;
				}
			}
		}

		for (size_t i = 0; i < statements_.size(); ++i)
		{
			Statement& statement = statements_[i];
			if (!(statement.static_literal || statement.is_data)) continue;
			const size_t width = statement.static_literal ?
				EncodeStaticLiteral(statement.literal).size() :
				static_cast<size_t>(statement.info.width / 8);
			const size_t alignment = statement.static_literal ?
				(statement.literal.string_literal ?
					static_cast<size_t>(statement.literal.width) : width) : width;
			Align(&image_, max<size_t>(1, alignment));
			statement.data_offset = image_.size();
			AssignDataLabels(statement,
				CY86_IMAGE_BASE + static_cast<uint64_t>(image_.size()));
			if (!first_statement_seen_ && i == 0)
			{
				first_statement_seen_ = true;
				first_statement_address_ = CY86_IMAGE_BASE + image_.size();
			}
			if (statement.static_literal)
			{
				const vector<unsigned char> bytes = EncodeStaticLiteral(statement.literal);
				image_.insert(image_.end(), bytes.begin(), bytes.end());
			}
			else
			{
				data_relocations_.push_back(DataRelocation{
					statement.data_offset, statement.info.width, statement.operands[0].expr});
				AppendZeros(&image_, width);
			}
		}
	}

	void AssignCodeLabels(Statement& statement)
	{
		const uint64_t address = CY86_IMAGE_BASE + image_.size();
		for (size_t i = 0; i < statement.labels.size(); ++i)
		{
			LabelInfo& label = labels_[statement.labels[i]];
			label.assigned = true;
			label.code = true;
			label.address = address;
		}
	}

	void Put8(unsigned int value) { image_.push_back(static_cast<unsigned char>(value)); }

	void Put32(uint32_t value)
	{
		for (int i = 0; i < 4; ++i) { Put8(value); value >>= 8; }
	}

	void Put64(uint64_t value)
	{
		for (int i = 0; i < 8; ++i) { Put8(value); value >>= 8; }
	}

	void Rex(bool w, int reg, int index, int rm)
	{
		const unsigned int value = 0x40 | (w ? 8 : 0) |
			((reg & 8) ? 4 : 0) | ((index & 8) ? 2 : 0) | ((rm & 8) ? 1 : 0);
		if (value != 0x40) Put8(value);
	}

	void ModRM(unsigned int mode, int reg, int rm)
	{
		Put8((mode << 6) | ((reg & 7) << 3) | (rm & 7));
	}

	void EmitMovImm64(int reg, uint64_t value)
	{
		Rex(true, 0, 0, reg);
		Put8(0xb8 + (reg & 7));
		Put64(value);
	}

	void EmitMovRegReg(int dst, int src, int width)
	{
		if (width == 16) Put8(0x66);
		Rex(width == 64, src, 0, dst);
		Put8(width == 8 ? 0x88 : 0x89);
		ModRM(3, src, dst);
	}

	void EmitMovMemReg(int base, int src, int width)
	{
		if (width == 16) Put8(0x66);
		Rex(width == 64, src, 0, base);
		Put8(width == 8 ? 0x88 : 0x89);
		ModRM(0, src, base);
		if ((base & 7) == 4) Put8(0x24);
	}

	void EmitMovRegMem(int dst, int base, int width)
	{
		if (width == 16) Put8(0x66);
		Rex(width == 64, dst, 0, base);
		Put8(width == 8 ? 0x8a : 0x8b);
		ModRM(0, dst, base);
		if ((base & 7) == 4) Put8(0x24);
	}

	void EmitAndImm64(int reg, uint32_t value)
	{
		Rex(true, 0, 0, reg);
		Put8(0x81); ModRM(3, 4, reg); Put32(value);
	}

	void EmitAddImm64(int reg, int64_t value)
	{
		if (value >= numeric_limits<int32_t>::min() &&
			value <= numeric_limits<int32_t>::max())
		{
			Rex(true, 0, 0, reg);
			Put8(0x81); ModRM(3, 0, reg);
			Put32(static_cast<uint32_t>(value));
		}
		else
		{
			EmitMovImm64(10, static_cast<uint64_t>(value));
			EmitArithmeticRegReg(0x01, reg, 10, 64);
		}
	}

	void EmitArithmeticRegReg(unsigned int opcode, int dst, int src, int width)
	{
		if (width == 8)
		{
			if (opcode == 0x01) opcode = 0x00;
			else if (opcode == 0x29) opcode = 0x28;
			else if (opcode == 0x21) opcode = 0x20;
			else if (opcode == 0x09) opcode = 0x08;
			else if (opcode == 0x31) opcode = 0x30;
			else if (opcode == 0x39) opcode = 0x38;
		}
		if (width == 16) Put8(0x66);
		Rex(width == 64, src, 0, dst);
		Put8(opcode); ModRM(3, src, dst);
	}

	void EmitImulRegReg(int dst, int src, int width)
	{
		if (width == 16) Put8(0x66);
		Rex(width == 64, dst, 0, src);
		Put8(0x0f); Put8(0xaf); ModRM(3, dst, src);
	}

	void EmitShift(int reg, int count_kind, int width)
	{
		if (width == 16) Put8(0x66);
		Rex(width == 64, count_kind, 0, reg);
		Put8(width == 8 ? 0xd2 : 0xd3); ModRM(3, count_kind, reg);
	}

	void EmitDivide(int divisor, int width, bool signed_divide)
	{
		if (width == 8)
		{
			Rex(false, signed_divide ? 7 : 6, 0, divisor);
			Put8(0xf6); ModRM(3, signed_divide ? 7 : 6, divisor);
			return;
		}
		if (width == 16) Put8(0x66);
		Rex(width == 64, signed_divide ? 7 : 6, 0, divisor);
		Put8(0xf7); ModRM(3, signed_divide ? 7 : 6, divisor);
	}

	void EmitCompare(int left, int right, int width)
	{
		EmitArithmeticRegReg(0x39, left, right, width);
	}

	void EmitSetCondition(const string& condition)
	{
		static const map<string, int> codes = {
			{"e", 0x94}, {"ne", 0x95}, {"b", 0x92}, {"a", 0x97},
			{"be", 0x96}, {"ae", 0x93}, {"l", 0x9c}, {"g", 0x9f},
			{"le", 0x9e}, {"ge", 0x9d}
		};
		map<string, int>::const_iterator found = codes.find(condition);
		if (found == codes.end()) throw logic_error("unknown condition code");
		Put8(0x0f); Put8(found->second); ModRM(3, 0, 0);
	}

	void EmitTestReg(int reg)
	{
		Rex(true, reg, 0, reg);
		Put8(0x85); ModRM(3, reg, reg);
	}

	void EmitTestRegReg(int left, int right)
	{
		Rex(true, right, 0, left);
		Put8(0x85); ModRM(3, right, left);
	}

	void EmitSignExtend(int reg, int width)
	{
		if (width == 64) return;
		if (width == 32)
		{
			Rex(true, reg, 0, reg);
			Put8(0x63); ModRM(3, reg, reg);
		}
		else
		{
			Rex(true, reg, 0, reg);
			Put8(0x0f); Put8(width == 8 ? 0xbe : 0xbf);
			ModRM(3, reg, reg);
		}
	}

	void EmitAddress(const Expr& expr, int destination)
	{
		if (expr.kind == EXPR_REGISTER)
		{
			EmitMovRegReg(destination, expr.reg, 64);
			if (expr.addend != 0) EmitAddImm64(destination, expr.addend);
			return;
		}
		if (expr.kind != EXPR_LABEL)
		{
			if (!expr.literal.integral || expr.literal.string_literal)
				throw logic_error("non-integral memory address");
			EmitMovImm64(destination, LiteralForWidth(expr.literal, 64));
			if (expr.addend != 0) EmitAddImm64(destination, expr.addend);
			return;
		}
		EmitMovAbsoluteLabel(destination, expr.label, expr.addend);
	}

	void EmitMovAbsoluteLabel(int destination, const string& label, int64_t addend)
	{
		Rex(true, 0, 0, destination);
		Put8(0xb8 + (destination & 7));
		const size_t offset = image_.size();
		Put64(0);
		relocations_.push_back(Relocation{offset, label, addend, false, 0});
	}

	void EmitAddressedLoad(int destination, int base, int width)
	{
		EmitMovRegMem(destination, base, width);
		if (width == 8) EmitAndImm64(destination, 0xff);
		else if (width == 16) EmitAndImm64(destination, 0xffff);
	}

	void EmitAddressedStore(int base, int source, int width)
	{
		EmitMovMemReg(base, source, width);
	}

	uint64_t LiteralForWidth(const Literal& literal, int width) const
	{
		if (literal.string_literal)
		{
			uint64_t value = 0;
			const size_t count = min<size_t>(static_cast<size_t>(width / 8),
				literal.bytes.empty() ? 0 : literal.bytes.size() - 1);
			for (size_t i = 0; i < count && i < 8; ++i)
				value |= static_cast<uint64_t>(literal.bytes[i]) << (8 * i);
			return value;
		}
		if (literal.floating)
		{
			const vector<unsigned char> bytes = EncodeFloating(literal);
			uint64_t value = 0;
			const size_t count = min<size_t>(static_cast<size_t>(width / 8),
				min<size_t>(bytes.size(), 8));
			for (size_t i = 0; i < count; ++i)
				value |= static_cast<uint64_t>(bytes[i]) << (8 * i);
			return value;
		}
		if (!literal.integral)
			throw logic_error("non-integral immediate");
		if (width >= literal.width * 8)
			return literal.signed_value ? SignExtend(literal.bits, literal.width * 8) :
				literal.bits;
		return literal.bits & MaskForWidth(width);
	}

	uint64_t ResolveExpr(const Expr& expr, int width) const
	{
		uint64_t value = 0;
		if (expr.kind == EXPR_LITERAL) value = LiteralForWidth(expr.literal, width);
		else if (expr.kind == EXPR_LABEL)
		{
			map<string, LabelInfo>::const_iterator found = labels_.find(expr.label);
			if (found == labels_.end() || !found->second.assigned)
				throw logic_error("unresolved CY86 label: " + expr.label);
			value = found->second.address;
			value += static_cast<uint64_t>(expr.addend);
		}
		else throw logic_error("register is not an immediate");
		return width < 64 ? value & MaskForWidth(width) : value;
	}

	void NormalizeLoaded(int reg, int width)
	{
		if (width == 8) EmitAndImm64(reg, 0xff);
		else if (width == 16) EmitAndImm64(reg, 0xffff);
		else if (width == 32) EmitMovRegReg(reg, reg, 32);
	}

	void LoadOperand(const Operand& operand, int destination, int width)
	{
		if (operand.kind == OPERAND_REGISTER)
		{
			EmitMovRegReg(destination, operand.reg, width == 64 ? 64 : 64);
			NormalizeLoaded(destination, width);
		}
		else if (operand.kind == OPERAND_MEMORY)
		{
			EmitAddress(operand.expr, 11);
			EmitAddressedLoad(destination, 11, width);
		}
		else if (operand.expr.kind == EXPR_LABEL)
		{
			EmitMovAbsoluteLabel(destination, operand.expr.label,
				operand.expr.addend);
			NormalizeLoaded(destination, width);
		}
		else
		{
			EmitMovImm64(destination, ResolveExpr(operand.expr, width));
		}
	}

	void StoreOperand(const Operand& operand, int source, int width)
	{
		if (operand.kind == OPERAND_REGISTER)
		{
			EmitMovRegReg(operand.reg, source, width);
		}
		else if (operand.kind == OPERAND_MEMORY)
		{
			EmitAddress(operand.expr, 11);
			EmitAddressedStore(11, source, width);
		}
		else throw logic_error("cannot store to immediate");
	}

	void EmitRelative(const string& target, unsigned int opcode, bool two_byte)
	{
		if (two_byte) { Put8(0x0f); Put8(opcode); }
		else Put8(opcode);
		const size_t offset = image_.size();
		Put32(0);
		relocations_.push_back(Relocation{offset, target, 0, true, image_.size()});
	}

	bool IsDirectCodeTarget(const Operand& operand) const
	{
		if (operand.kind != OPERAND_IMMEDIATE || operand.expr.kind != EXPR_LABEL)
			return false;
		if (operand.expr.addend != 0) return false;
		map<string, LabelInfo>::const_iterator found = labels_.find(operand.expr.label);
		return found != labels_.end() && found->second.code;
	}

	void EmitJumpLike(const Statement& statement, bool call)
	{
		const Operand& target = statement.operands[0];
		if (IsDirectCodeTarget(target))
		{
			EmitRelative(target.expr.label, call ? 0xe8 : 0xe9, false);
			return;
		}
		LoadOperand(target, 0, 64);
		Rex(true, 0, 0, 0);
		Put8(0xff); ModRM(3, call ? 2 : 4, 0);
	}

	void EmitJumpIf(const Statement& statement)
	{
		LoadOperand(statement.operands[0], 0, 8);
		EmitTestReg(0);
		const Operand& target = statement.operands[1];
		if (IsDirectCodeTarget(target))
		{
			EmitRelative(target.expr.label, 0x85, true);
		}
		else
		{
			const string skip = NewInternalLabel();
			EmitRelative(skip, 0x84, true);
			LoadOperand(target, 1, 64);
			Rex(true, 0, 0, 1); Put8(0xff); ModRM(3, 4, 1);
			MarkInternalLabel(skip);
		}
	}

	void EmitMove80(const Statement& statement)
	{
		EmitAddress(statement.operands[1].expr, 11);
		EmitAddress(statement.operands[0].expr, 10);
		EmitMovRegMem(0, 11, 64);
		EmitMovMemReg(10, 0, 64);
		EmitAddImm64(11, 8); EmitAddImm64(10, 8);
		EmitMovRegMem(0, 11, 16);
		EmitMovMemReg(10, 0, 16);
	}

	void EmitIntegerOperation(const Statement& statement)
	{
		const OpInfo& info = statement.info;
		if (info.family == F_NOT)
		{
			LoadOperand(statement.operands[1], 0, info.width);
			if (info.width == 8)
			{
				Rex(false, 0, 0, 0); Put8(0xf6); ModRM(3, 2, 0);
			}
			else
			{
				if (info.width == 16) Put8(0x66);
				Rex(info.width == 64, 0, 0, 0); Put8(0xf7); ModRM(3, 2, 0);
			}
			StoreOperand(statement.operands[0], 0, info.width);
			return;
		}
		if (info.family == F_LOGIC)
		{
			LoadOperand(statement.operands[1], 0, info.width);
			LoadOperand(statement.operands[2], 1, info.width);
			const string name = statement.opcode;
			const unsigned int opcode = StartsWith(name, "and") ? 0x21 :
				StartsWith(name, "or") ? 0x09 : 0x31;
			EmitArithmeticRegReg(opcode, 0, 1, info.width);
			StoreOperand(statement.operands[0], 0, info.width);
			return;
		}
		if (info.family == F_SHIFT)
		{
			LoadOperand(statement.operands[1], 0, info.width);
			LoadOperand(statement.operands[2], 1, 8);
			const int group = StartsWith(statement.opcode, "lshift") ? 4 :
				StartsWith(statement.opcode, "srshift") ? 7 : 5;
			EmitShift(0, group, info.width);
			StoreOperand(statement.operands[0], 0, info.width);
			return;
		}
		LoadOperand(statement.operands[1], 0, info.width);
		LoadOperand(statement.operands[2], 1, info.width);
		const string name = statement.opcode;
		if (StartsWith(name, "iadd")) EmitArithmeticRegReg(0x01, 0, 1, info.width);
		else if (StartsWith(name, "isub")) EmitArithmeticRegReg(0x29, 0, 1, info.width);
		else if (StartsWith(name, "smul") || StartsWith(name, "umul"))
			EmitImulRegReg(0, 1, info.width == 8 ? 64 : info.width);
		else
		{
			const bool division = StartsWith(name, "sdiv") || StartsWith(name, "udiv");
			const bool signed_division = StartsWith(name, "sdiv") || StartsWith(name, "smod");
			if (info.width == 8)
			{
				if (signed_division) { Rex(false, 0, 0, 0); Put8(0x66); Put8(0x98); }
				else { EmitAndImm64(0, 0xff); }
			}
			else if (info.width == 16)
			{
				if (signed_division) { Put8(0x66); Put8(0x99); }
				else { Rex(false, 0, 0, 2); Put8(0x31); ModRM(3, 2, 2); }
			}
			else if (info.width == 32)
			{
				if (signed_division) Put8(0x99);
				else { Put8(0x31); ModRM(3, 2, 2); }
			}
			else
			{
				if (signed_division) { Put8(0x48); Put8(0x99); }
				else { Put8(0x48); Put8(0x31); ModRM(3, 2, 2); }
			}
			EmitDivide(1, info.width, signed_division);
			if (!division)
			{
				if (info.width == 8)
				{
					// Byte division returns the remainder in AH, not DX.
					Put8(0x88); ModRM(3, 4, 0); // mov al, ah
					EmitAndImm64(0, 0xff);
				}
				else EmitMovRegReg(0, 2, info.width);
			}
		}
		StoreOperand(statement.operands[0], 0, info.width);
	}

	void EmitIntegerCompare(const Statement& statement)
	{
		LoadOperand(statement.operands[1], 0, statement.info.width);
		LoadOperand(statement.operands[2], 1, statement.info.width);
		EmitCompare(0, 1, statement.info.width);
		EmitSetCondition(statement.info.condition);
		StoreOperand(statement.operands[0], 0, 8);
	}

	void EmitSyscall(const Statement& statement)
	{
		LoadOperand(statement.operands[1], 0, 64);
		static const int argument_registers[] = {7, 6, 2, 10, 8, 9};
		for (size_t i = 2; i < statement.operands.size(); ++i)
			LoadOperand(statement.operands[i], argument_registers[i - 2], 64);
		Put8(0x0f); Put8(0x05);
		StoreOperand(statement.operands[0], 0, 64);
	}

	void EmitFLoad(const Operand& operand, int width)
	{
		if (operand.kind != OPERAND_MEMORY) throw logic_error("float source is not memory");
		EmitAddress(operand.expr, 11);
		Rex(false, 0, 0, 11);
		if (width == 32) { Put8(0xd9); ModRM(0, 0, 11); }
		else if (width == 64) { Put8(0xdd); ModRM(0, 0, 11); }
		else { Put8(0xdb); ModRM(0, 5, 11); }
	}

	void EmitFStore(const Operand& operand, int width)
	{
		if (operand.kind != OPERAND_MEMORY) throw logic_error("float result is not memory");
		EmitAddress(operand.expr, 11);
		Rex(false, 0, 0, 11);
		if (width == 32) { Put8(0xd9); ModRM(0, 3, 11); }
		else if (width == 64) { Put8(0xdd); ModRM(0, 3, 11); }
		else { Put8(0xdb); ModRM(0, 7, 11); }
	}

	void EmitFArithmetic(const Statement& statement)
	{
		const int width = statement.info.width;
		EmitFLoad(statement.operands[1], width);
		EmitFLoad(statement.operands[2], width);
		Put8(0xde);
		const string name = statement.opcode;
		const int subopcode = StartsWith(name, "fadd") ? 0xc1 :
			StartsWith(name, "fsub") ? 0xe9 : StartsWith(name, "fmul") ? 0xc9 : 0xf9;
		Put8(subopcode);
		EmitFStore(statement.operands[0], width);
	}

	void EmitFCompare(const Statement& statement)
	{
		// FCOMIP's flags describe ST0 relative to ST(i) on this encoding;
		// load in reverse so CY86's op2 <op> op3 order is preserved.
		EmitFLoad(statement.operands[2], statement.info.width);
		EmitFLoad(statement.operands[1], statement.info.width);
		Put8(0xdf); Put8(0xf1); // fcomip st(1), st(0), and pop
		Put8(0xdd); Put8(0xd8); // discard the remaining operand
		EmitSetCondition(statement.info.condition);
		StoreOperand(statement.operands[0], 0, 8);
	}

	void EmitTempAddress(int destination, int64_t offset)
	{
		EmitMovRegReg(destination, 4, 64);
		EmitAddImm64(destination, offset);
	}

	void EmitFildTemp(int width, bool signed_value)
	{
		(void)signed_value;
		EmitTempAddress(11, -8);
		Rex(false, 0, 0, 11);
		if (width <= 32) { Put8(0xdb); ModRM(0, 0, 11); }
		else { Put8(0xdf); ModRM(0, 5, 11); }
	}

	void EmitFistpTemp(int width)
	{
		EmitTempAddress(11, -8);
		Rex(false, 0, 0, 11);
		if (width <= 16) { Put8(0xdf); ModRM(0, 3, 11); }
		else if (width <= 32) { Put8(0xdb); ModRM(0, 3, 11); }
		else { Put8(0xdf); ModRM(0, 7, 11); }
	}

	void EmitLoadTwoTo63()
	{
		// Build 2^63 on the x87 stack using 1 * 2^63.  The integer exponent
		// lives in the red zone, so no CY86-visible state is disturbed.
		EmitTempAddress(11, -16);
		EmitMovImm64(0, 63);
		EmitMovMemReg(11, 0, 64);
		Put8(0xd9); Put8(0xe8); // fld1
		EmitTempAddress(11, -16);
		Rex(false, 0, 0, 11);
		Put8(0xdf); ModRM(0, 5, 11); // fild qword
		Put8(0xd9); Put8(0xc9); // fxch st(1)
		Put8(0xd9); Put8(0xfd); // fscale
		Put8(0xdd); Put8(0xd9); // fstp st(1)
	}

	void EmitToFloat(const Statement& statement)
	{
		const int source_width = statement.info.source_width;
		if (statement.opcode[0] == 'f')
		{
			EmitFLoad(statement.operands[1], source_width);
			EmitFStore(statement.operands[0], 80);
			return;
		}
		LoadOperand(statement.operands[1], 0, source_width);
		const bool unsigned_value = statement.opcode[0] == 'u';
		if (!unsigned_value || source_width < 64)
		{
			if (!unsigned_value) EmitSignExtend(0, source_width);
			EmitTempAddress(11, -8);
			EmitMovMemReg(11, 0, 64);
			EmitFildTemp(64, !unsigned_value);
		}
		else
		{
			const string low = NewInternalLabel();
			const string done = NewInternalLabel();
			EmitMovImm64(10, 0x8000000000000000ULL);
			EmitTestRegReg(0, 10);
			EmitRelative(low, 0x84, true);
			EmitMovImm64(10, 0x7fffffffffffffffULL);
			EmitArithmeticRegReg(0x21, 0, 10, 64);
			EmitTempAddress(11, -8); EmitMovMemReg(11, 0, 64); EmitFildTemp(64, false);
			EmitLoadTwoTo63(); Put8(0xde); Put8(0xc1); // faddp
			EmitRelative(done, 0xe9, false);
			MarkInternalLabel(low);
			EmitTempAddress(11, -8); EmitMovMemReg(11, 0, 64); EmitFildTemp(64, false);
			MarkInternalLabel(done);
		}
		EmitFStore(statement.operands[0], 80);
	}

	void EmitFromFloat(const Statement& statement)
	{
		const int destination_width = statement.info.width;
		const char kind = statement.opcode[7];
		if (kind == 'f')
		{
			EmitFLoad(statement.operands[1], 80);
			EmitFStore(statement.operands[0], destination_width);
			return;
		}
		EmitFLoad(statement.operands[1], 80);
		if (kind == 'u' && destination_width == 64)
		{
			const string high = NewInternalLabel();
			const string done = NewInternalLabel();
			EmitLoadTwoTo63();
			Put8(0xdf); Put8(0xf1); // compare original (st1) against 2^63 (st0)
			// The x87 compare is encoded with 2^63 in ST0 and the source in
			// ST1, so the flags are for (2^63 <=> source).  JBE selects the
			// source >= 2^63 case, including equality.
			EmitRelative(high, 0x86, true);
			EmitFistpTemp(64);
			EmitTempAddress(11, -8); EmitAddressedLoad(0, 11, 64);
			StoreOperand(statement.operands[0], 0, 64);
			EmitRelative(done, 0xe9, false);
			MarkInternalLabel(high);
			EmitLoadTwoTo63();
			Put8(0xde); Put8(0xe9); // fsubp: original - 2^63
			EmitFistpTemp(64);
			EmitTempAddress(11, -8); EmitAddressedLoad(0, 11, 64);
			EmitMovImm64(10, 0x8000000000000000ULL);
			EmitArithmeticRegReg(0x09, 0, 10, 64);
			StoreOperand(statement.operands[0], 0, 64);
			MarkInternalLabel(done);
			return;
		}
		// FISTP has signed integer destinations.  Unsigned 16/32-bit values
		// must therefore use the 64-bit temporary even when the final CY86
		// destination is narrower.
		const int temp_width = kind == 'u' ? 64 :
			destination_width <= 8 ? 32 : destination_width;
		EmitFistpTemp(temp_width);
		EmitTempAddress(11, -8);
		EmitAddressedLoad(0, 11, temp_width == 32 ? 32 : temp_width);
		StoreOperand(statement.operands[0], 0, destination_width);
	}

	string NewInternalLabel()
	{
		ostringstream stream;
		stream << "@cy86_internal_" << internal_label_serial_++;
		return stream.str();
	}

	void MarkInternalLabel(const string& label)
	{
		internal_labels_[label] = image_.size();
	}

	void EmitStatement(const Statement& statement)
	{
		const Family family = statement.info.family;
		if (family == F_JUMP) EmitJumpLike(statement, false);
		else if (family == F_CALL) EmitJumpLike(statement, true);
		else if (family == F_JUMPIF) EmitJumpIf(statement);
		else if (family == F_RET) Put8(0xc3);
		else if (family == F_MOVE && statement.info.width == 80) EmitMove80(statement);
		else if (family == F_MOVE)
		{
			LoadOperand(statement.operands[1], 0, statement.info.width);
			StoreOperand(statement.operands[0], 0, statement.info.width);
		}
		else if (family == F_NOT || family == F_LOGIC || family == F_SHIFT ||
			family == F_INT_ARITH) EmitIntegerOperation(statement);
		else if (family == F_INT_COMPARE) EmitIntegerCompare(statement);
		else if (family == F_SYSCALL) EmitSyscall(statement);
		else if (family == F_FLOAT_ARITH) EmitFArithmetic(statement);
		else if (family == F_FLOAT_COMPARE) EmitFCompare(statement);
		else if (family == F_TO_FLOAT) EmitToFloat(statement);
		else if (family == F_FROM_FLOAT) EmitFromFloat(statement);
		else throw logic_error("cannot lower CY86 statement");
	}

	void EmitCode()
	{
		for (size_t i = 0; i < statements_.size(); ++i)
		{
			Statement& statement = statements_[i];
			if (statement.static_literal || statement.is_data) continue;
			AssignCodeLabels(statement);
			if (!first_statement_seen_ && i == 0)
			{
				first_statement_seen_ = true;
				first_statement_address_ = CY86_IMAGE_BASE + image_.size();
			}
			try
			{
				EmitStatement(statement);
			}
			catch (const exception& exception)
			{
				throw logic_error(statement.opcode + ": " + exception.what());
			}
		}
	}

	uint64_t RelocationTarget(const string& target) const
	{
		map<string, LabelInfo>::const_iterator label = labels_.find(target);
		if (label != labels_.end())
		{
			if (!label->second.assigned) throw logic_error("unresolved CY86 label: " + target);
			return label->second.address;
		}
		map<string, size_t>::const_iterator internal = internal_labels_.find(target);
		if (internal != internal_labels_.end())
			return CY86_IMAGE_BASE + internal->second;
		throw logic_error("unresolved assembler label: " + target);
	}

	void PatchRelocations()
	{
		for (size_t i = 0; i < data_relocations_.size(); ++i)
		{
			const DataRelocation& relocation = data_relocations_[i];
			const uint64_t value = ResolveExpr(relocation.expr, relocation.width);
			uint64_t copy = value;
			for (int byte = 0; byte < relocation.width / 8; ++byte)
			{
				image_[relocation.offset + byte] = static_cast<unsigned char>(copy & 0xff);
				copy >>= 8;
			}
		}
		for (size_t i = 0; i < relocations_.size(); ++i)
		{
			const Relocation& relocation = relocations_[i];
			const uint64_t target = RelocationTarget(relocation.target) +
				static_cast<uint64_t>(relocation.addend);
			int64_t value;
			if (relocation.relative)
			{
				value = static_cast<int64_t>(target) -
					static_cast<int64_t>(CY86_IMAGE_BASE + relocation.base_after);
				if (value < numeric_limits<int32_t>::min() ||
					value > numeric_limits<int32_t>::max())
					throw logic_error("CY86 branch is out of range");
			}
			else value = static_cast<int64_t>(target);
			uint64_t copy = static_cast<uint64_t>(value);
			for (int byte = 0; byte < 8 && !relocation.relative; ++byte)
			{
				image_[relocation.offset + byte] = static_cast<unsigned char>(copy & 0xff);
				copy >>= 8;
			}
			if (relocation.relative)
				for (int byte = 0; byte < 4; ++byte)
				{
					image_[relocation.offset + byte] = static_cast<unsigned char>(copy & 0xff);
					copy >>= 8;
				}
		}
	}
};

} // namespace cy86

int main(int argc, char** argv)
{
	try
	{
		string outfile;
		vector<string> source_files;
		for (int i = 1; i < argc; ++i)
		{
			const string argument = argv[i];
			if (argument == "--target")
			{
				if (++i >= argc) throw logic_error("missing target after --target");
				continue;
			}
			if (argument == "-o")
			{
				if (++i >= argc) throw logic_error("missing output file after -o");
				outfile = argv[i];
				continue;
			}
			source_files.push_back(argument);
		}
		if (outfile.empty() || source_files.empty()) throw logic_error("invalid usage");

		vector<PostPPToken> tokens;
		for (size_t i = 0; i < source_files.size(); ++i)
		{
			const vector<PostPPToken> unit = PreprocessSourceFile(source_files[i]);
			tokens.insert(tokens.end(), unit.begin(), unit.end());
		}
		cy86::Compiler compiler(tokens);
		const vector<unsigned char> image = compiler.Build();
		ofstream output(outfile.c_str(), ios::binary | ios::trunc);
		if (!output) throw runtime_error("cannot open CY86 output");
		output.write(reinterpret_cast<const char*>(&image[0]), image.size());
		if (!output) throw runtime_error("cannot write CY86 output");
		output.close();
		if (!PA9SetFileExecutable(outfile)) throw runtime_error("cannot chmod CY86 output");
		return EXIT_SUCCESS;
	}
	catch (const NotImplementedException& exception)
	{
		cerr << "ERROR: " << exception.what() << endl;
		return CPPGM_EXIT_NOT_IMPLEMENTED;
	}
	catch (const exception& exception)
	{
		cerr << "ERROR: " << exception.what() << endl;
		return EXIT_FAILURE;
	}
}
