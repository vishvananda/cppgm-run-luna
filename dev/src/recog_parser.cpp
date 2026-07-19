#include "recog_parser.h"
#include "recog_parser_internal.h"

namespace recog_pa6 {

bool IsLiteralKind(PostPPTokenKind kind)
{
	return kind == POST_PP_NUMBER || kind == POST_PP_CHARACTER ||
		kind == POST_PP_USER_CHARACTER || kind == POST_PP_STRING ||
		kind == POST_PP_USER_STRING;
}

bool IsKeyword(const string& text)
{
	static const set<string> keywords = {
		"alignas", "alignof", "asm", "auto", "bool", "break", "case",
		"catch", "char", "char16_t", "char32_t", "class", "const",
		"constexpr", "const_cast", "continue", "decltype", "default",
		"delete", "do", "double", "dynamic_cast", "else", "enum",
		"explicit", "export", "extern", "false", "float", "for", "friend",
		"goto", "if", "inline", "int", "long", "mutable", "namespace",
		"new", "noexcept", "nullptr", "operator", "private", "protected",
		"public", "register", "reinterpret_cast", "return", "short", "signed",
		"sizeof", "static", "static_assert", "static_cast", "struct", "switch",
		"template", "this", "thread_local", "throw", "true", "try", "typedef",
		"typeid", "typename", "union", "unsigned", "using", "virtual", "void",
		"volatile", "wchar_t", "while"
	};
	return keywords.find(text) != keywords.end();
}

bool IsIdentifierToken(const RecognizerToken& token)
{
	return token.kind == RK_IDENTIFIER;
}

bool IsLiteralToken(const RecognizerToken& token)
{
	return token.kind == RK_LITERAL;
}

bool IsEmptyStringLiteral(const RecognizerToken& token)
{
	return IsLiteralToken(token) &&
		(token.text == "\"\"" || token.text == "u8\"\"" ||
		 token.text == "u\"\"" || token.text == "U\"\"" ||
		 token.text == "L\"\"");
}

bool IsZeroLiteral(const RecognizerToken& token)
{
	return IsLiteralToken(token) && token.text == "0";
}

vector<RecognizerToken> NormalizeTokens(const vector<PostPPToken>& input)
{
	vector<RecognizerToken> result;
	for (size_t i = 0; i < input.size(); ++i)
	{
		const PostPPToken& source = input[i];
		if (source.kind == POST_PP_EOF)
			continue;
		if (source.kind == POST_PP_NON_WHITESPACE ||
			source.kind == POST_PP_HEADER)
			throw logic_error("invalid token in source");
		if (source.kind == POST_PP_PUNCTUATOR && source.source == ">>")
		{
			result.push_back(RecognizerToken(RK_RSHIFT_1, "ST_RSHIFT_1"));
			result.push_back(RecognizerToken(RK_RSHIFT_2, "ST_RSHIFT_2"));
			continue;
		}

		RecognizerToken token;
		token.text = source.source;
		if (source.kind == POST_PP_IDENTIFIER)
		{
			// The lexer has already applied UCN and trigraph translation.  Keep
			// these facts alongside the token so all name productions use the
			// same typed mock lookup rather than reimplementing it ad hoc.
			token.kind = IsKeyword(source.source) ? RK_PUNCTUATOR : RK_IDENTIFIER;
			token.names.class_name = source.source.find('C') != string::npos;
			token.names.template_name = source.source.find('T') != string::npos;
			token.names.typedef_name = source.source.find('Y') != string::npos;
			token.names.enum_name = source.source.find('E') != string::npos;
			token.names.namespace_name = source.source.find('N') != string::npos;
		}
		else if (IsLiteralKind(source.kind))
			token.kind = RK_LITERAL;
		else
			token.kind = RK_PUNCTUATOR;
		result.push_back(token);
	}
	result.push_back(RecognizerToken(RK_EOF, "ST_EOF"));
	return result;
}
bool Parser::IsOperatorAlias(const string& text, const string& spelling) const
{
	if (text == spelling) return true;
	if (spelling == "|") return text == "bitor";
	if (spelling == "^") return text == "xor";
	if (spelling == "&") return text == "bitand";
	if (spelling == "!") return text == "not";
	if (spelling == "&&") return text == "and";
	if (spelling == "||") return text == "or";
	if (spelling == "!=") return text == "not_eq";
	if (spelling == "&=") return text == "and_eq";
	if (spelling == "|=") return text == "or_eq";
	return false;
}

bool Parser::TakeOperator(const string& spelling)
{
	if (spelling == ">>" || spelling == "ST_RSHIFT_1 ST_RSHIFT_2")
		return TakeShiftRight();
	if (IsOperatorAlias(Peek().text, spelling))
	{
		++position_;
		return true;
	}
	return false;
}

bool Parser::TakeAnyOperator(const set<string>& spellings)
{
	for (set<string>::const_iterator it = spellings.begin();
		it != spellings.end(); ++it)
	{
		Mark mark = Save();
		if (TakeOperator(*it)) return true;
		Restore(mark);
	}
	return false;
}

bool Parser::Parse()
{
	return ParseTranslationUnit() && AtEnd();
}

bool Parser::ParseTranslationUnit()
{
	Mark mark = Save();
	while (!AtEnd())
	{
		Mark before = Save();
		if (!ParseDeclaration())
		{
			Restore(mark);
			return false;
		}
		if (position_ == before.position)
		{
			Restore(mark);
			return false;
		}
	}
	return true;
}

} // namespace recog_pa6

bool RecognizePA6(const std::vector<PostPPToken>& tokens)
{
	const std::vector<recog_pa6::RecognizerToken> normalized = recog_pa6::NormalizeTokens(tokens);
	return recog_pa6::Parser(normalized).Parse();
}
