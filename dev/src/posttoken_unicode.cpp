#include "posttoken_unicode.h"

#include <stdexcept>

using namespace std;

bool PostIsValidCodePoint(int code_point)
{
	return code_point >= 0 && code_point <= 0x10ffff &&
		!(code_point >= 0xd800 && code_point <= 0xdfff);
}

vector<int> PostDecodeUTF8(const string& input)
{
	vector<int> result;
	for (size_t i = 0; i < input.size();)
	{
		const unsigned char first = static_cast<unsigned char>(input[i]);
		int length = 0;
		int value = 0;
		if (first <= 0x7f) { length = 1; value = first; }
		else if (first >= 0xc2 && first <= 0xdf) { length = 2; value = first & 0x1f; }
		else if (first >= 0xe0 && first <= 0xef) { length = 3; value = first & 0x0f; }
		else if (first >= 0xf0 && first <= 0xf4) { length = 4; value = first & 0x07; }
		else throw logic_error("invalid UTF-8 leading byte");
		if (i + static_cast<size_t>(length) > input.size())
			throw logic_error("truncated UTF-8 sequence");
		for (int j = 1; j < length; ++j)
		{
			const unsigned char continuation =
				static_cast<unsigned char>(input[i + j]);
			if ((continuation & 0xc0) != 0x80)
				throw logic_error("invalid UTF-8 continuation byte");
			if (length == 3 && j == 1 &&
				((first == 0xe0 && continuation < 0xa0) ||
				 (first == 0xed && continuation >= 0xa0)))
				throw logic_error("invalid UTF-8 code point");
			if (length == 4 && j == 1 &&
				((first == 0xf0 && continuation < 0x90) ||
				 (first == 0xf4 && continuation > 0x8f)))
				throw logic_error("invalid UTF-8 code point");
			value = (value << 6) | (continuation & 0x3f);
		}
		if (!PostIsValidCodePoint(value))
			throw logic_error("invalid UTF-8 code point");
		result.push_back(value);
		i += static_cast<size_t>(length);
	}
	return result;
}

string PostEncodeUTF8(int code_point)
{
	if (!PostIsValidCodePoint(code_point))
		throw logic_error("invalid Unicode code point");
	string result;
	if (code_point <= 0x7f)
		result.push_back(static_cast<char>(code_point));
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
