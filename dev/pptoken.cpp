#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <unordered_set>
#include <vector>

using namespace std;

#include "IPPTokenStream.h"
#include "DebugPPTokenStream.h"
#include "exceptions.h"
#include "pptoken_translation.h"

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
		string data = EncodeUTF8CodePoint(opener);
		size_t i = position + 1;
		for (; i < source.size(); ++i)
		{
			if (!source[i].raw && source[i].code_point == '/' &&
				i + 1 < source.size() && !source[i + 1].raw &&
				source[i + 1].code_point == '*')
			{
				data += " ";
				i += 2;
				bool closed = false;
				while (i + 1 < source.size())
				{
					if (source[i].code_point == '*' &&
						source[i + 1].code_point == '/')
					{
						i += 2;
						closed = true;
						break;
					}
					++i;
				}
				if (!closed)
					throw logic_error("unterminated header-name comment");
				--i;
				continue;
			}
			if (!source[i].raw && source[i].code_point == '/' &&
				i + 1 < source.size() && !source[i + 1].raw &&
				source[i + 1].code_point == '/')
			{
				while (i < source.size() && source[i].code_point != '\n')
					++i;
				throw logic_error("unterminated header-name line");
			}
			if (source[i].code_point == '\n')
				throw logic_error("unterminated header name");
			if (source[i].code_point == closer)
			{
				data += EncodeUTF8CodePoint(closer);
				return LexedToken(PP_PREPROCESSING_OP_OR_PUNC, i + 1, data);
			}
			data += EncodeUTF8CodePoint(source[i].code_point);
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
