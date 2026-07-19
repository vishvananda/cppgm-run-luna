#include "nsinit_literals.h"

#include <cerrno>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

#include "posttoken_unicode.h"
#include "pptoken_translation.h"

using namespace std;

int HexValue(int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

bool IsHexValue(int c)
{
	return HexValue(c) >= 0;
}

bool IsOctalValue(int c)
{
	return c >= '0' && c <= '7';
}

size_t QuotedPrefix(const vector<int>& units, LiteralEncoding* encoding,
	bool* raw)
{
	*encoding = LIT_CHAR;
	*raw = false;
	if (units.empty()) return 0;
	if (units[0] == 'u' && units.size() > 1 && units[1] == '8')
	{
		if (units.size() > 2 && units[2] == 'R') { *raw = true; return 3; }
		return 2;
	}
	if (units[0] == 'u' || units[0] == 'U' || units[0] == 'L')
	{
		*encoding = units[0] == 'u' ? LIT_UTF16 :
			(units[0] == 'U' ? LIT_UTF32 : LIT_WCHAR);
		if (units.size() > 1 && units[1] == 'R') { *raw = true; return 2; }
		return 1;
	}
	if (units[0] == 'R') { *raw = true; return 1; }
	return 0;
}

vector<int> DecodeQuotedBody(const vector<int>& units, size_t begin, size_t end)
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
			if (i + digits >= end) throw logic_error("invalid unicode escape");
			int value = 0;
			for (size_t j = 0; j < digits; ++j)
			{
				const int digit = HexValue(units[++i]);
				if (digit < 0) throw logic_error("invalid unicode escape");
				value = value * 16 + digit;
			}
			result.push_back(value);
			continue;
		}
		case 'x':
		{
			if (i + 1 >= end || !IsHexValue(units[i + 1]))
				throw logic_error("invalid hex escape");
			int value = 0;
			while (i + 1 < end && IsHexValue(units[i + 1]))
				value = value * 16 + HexValue(units[++i]);
			result.push_back(value);
			continue;
		}
		default:
			if (!IsOctalValue(next)) throw logic_error("invalid escape");
		{
				int value = next - '0';
				for (int count = 1; count < 3 && i + 1 < end &&
					IsOctalValue(units[i + 1]); ++count)
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

QuotedData ParseQuoted(const string& source, bool character)
{
	const vector<int> units = PostDecodeUTF8(source);
	QuotedData result;

	bool raw = false;
	const size_t quote = QuotedPrefix(units, &result.encoding, &raw);
	if (quote >= units.size() || units[quote] != (character ? '\'' : '"'))
		throw logic_error("invalid quoted literal");
	size_t body_begin = quote + 1;
	size_t body_end = 0;
	size_t end = 0;
	if (raw)
	{
		size_t open = body_begin;
		while (open < units.size() && units[open] != '(') ++open;
		if (open >= units.size()) throw logic_error("unterminated raw string");
		const size_t delimiter_begin = body_begin;
		const size_t delimiter_length = open - delimiter_begin;
		for (size_t close = open + 1; close < units.size(); ++close)
		{
			if (units[close] != ')') continue;
			if (close + delimiter_length + 1 >= units.size()) continue;
			bool match = units[close + delimiter_length + 1] == '"';
			for (size_t i = 0; match && i < delimiter_length; ++i)
				match = units[close + 1 + i] == units[delimiter_begin + i];
			if (match)
			{
				body_begin = open + 1;
				body_end = close;
				end = close + delimiter_length + 2;
				break;
			}
		}
		if (end == 0) throw logic_error("unterminated raw string");
		result.values.assign(units.begin() + body_begin, units.begin() + body_end);
	}
	else
	{
		for (size_t close = body_begin; close < units.size(); ++close)
		{
			if (units[close] == '\\') { ++close; continue; }
			if (units[close] == (character ? '\'' : '"'))
			{
				body_end = close;
				end = close + 1;
				break;
			}
		}
		if (end == 0) throw logic_error("unterminated quoted literal");
		result.values = DecodeQuotedBody(units, body_begin, body_end);
	}
	for (size_t i = end; i < units.size(); ++i)
	{
		if (!IsIdentifierNondigit(units[i]))
			throw logic_error("invalid literal suffix");
		result.suffix += PostEncodeUTF8(units[i]);
	}
	return result;
}

void AppendLE(vector<unsigned char>* bytes, unsigned long long value, size_t width)
{
	for (size_t i = 0; i < width; ++i)
	{
		bytes->push_back(static_cast<unsigned char>(value & 0xff));
		value >>= 8;
	}
}

EncodedString EncodeString(const QuotedData& quoted)
{
	EncodedString result;
	const size_t width = quoted.encoding == LIT_CHAR ? 1 :
		(quoted.encoding == LIT_UTF16 ? 2 : 4);
	const string element = quoted.encoding == LIT_CHAR ? "char" :
		(quoted.encoding == LIT_UTF16 ? "char16_t" :
		 quoted.encoding == LIT_UTF32 ? "char32_t" : "wchar_t");
	result.type = Type::Array(0, Type::Fundamental(element));
	result.values = quoted.values;
	for (size_t i = 0; i < quoted.values.size(); ++i)
	{
		const unsigned long long value =
			static_cast<unsigned long long>(quoted.values[i]);
		if (quoted.encoding == LIT_CHAR)
		{
			const string utf8 = PostEncodeUTF8(quoted.values[i]);
			result.bytes.insert(result.bytes.end(), utf8.begin(), utf8.end());
		}
		else if (quoted.encoding == LIT_UTF16 && quoted.values[i] > 0xffff)
		{
			const unsigned long long adjusted = value - 0x10000;
			AppendLE(&result.bytes, 0xd800 + (adjusted >> 10), 2);
			AppendLE(&result.bytes, 0xdc00 + (adjusted & 0x3ff), 2);
		}
		else AppendLE(&result.bytes, value, width);
	}
	AppendLE(&result.bytes, 0, width);
	result.type.bound = static_cast<long long>(result.bytes.size() / width);
	return result;
}

bool LooksFloating(const string& source)
{
	return source.find('.') != string::npos || source.find('e') != string::npos ||
		source.find('E') != string::npos || source.find('p') != string::npos ||
		source.find('P') != string::npos;
}

string IntegerCore(const string& source, string* suffix, int* base)
{
	size_t end = 0;
	*base = 10;
	if (source.size() >= 2 && source[0] == '0' &&
		(source[1] == 'x' || source[1] == 'X'))
	{
		*base = 16;
		end = 2;
		while (end < source.size() && IsHexValue(source[end])) ++end;
	}
	else if (!source.empty() && source[0] == '0')
	{
		*base = 8;
		end = 1;
		while (end < source.size() && IsOctalValue(source[end])) ++end;
	}
	else
	{
		end = 0;
		while (end < source.size() && source[end] >= '0' && source[end] <= '9') ++end;
	}
	*suffix = source.substr(end);
	return source.substr(0, end);
}

Type IntegerLiteralType(unsigned long long value, const string& suffix, int base)
{
	bool unsig = false;
	int longs = 0;
	for (size_t i = 0; i < suffix.size(); ++i)
	{
		if (suffix[i] == 'u' || suffix[i] == 'U') unsig = true;
		else if (suffix[i] == 'l' || suffix[i] == 'L') ++longs;
		else throw logic_error("invalid integer suffix");
	}
	if (longs > 2) throw logic_error("invalid integer suffix");
	if (unsig)
	{
		if (longs == 0 && value <= 0xffffffffULL) return Type::Fundamental("unsigned int");
		if (longs <= 1) return Type::Fundamental("unsigned long int");
		return Type::Fundamental("unsigned long long int");
	}
	if (longs == 2) return Type::Fundamental("long long int");
	if (longs == 1) return Type::Fundamental("long int");
	if (value <= 0x7fffffffULL) return Type::Fundamental("int");
	if (base != 10 && value <= 0xffffffffULL) return Type::Fundamental("unsigned int");
	if (value <= 0x7fffffffffffffffULL) return Type::Fundamental("long int");
	return Type::Fundamental("long long int");
}
