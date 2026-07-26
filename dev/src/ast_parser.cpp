#include "ast_parser.h"

#include <algorithm>
#include <map>
#include <iostream>
#include <stdexcept>

using namespace std;

namespace cppgm_pa10 { bool IsKeywordToken(const string& text); }

namespace {

bool Contains(const char* const* begin, const char* const* end,
	const string& value)
{
	return find(begin, end, value) != end;
}

} // namespace

namespace cppgm_pa10 {

Parser::Parser(const vector<Token>& tokens)
	: tokens_(tokens), position_(0), angle_depth_(0), ordinary_depth_(0),
	  angle_floors_(), types_(), templates_(), alias_templates_(), namespaces_(), value_names_(),
	  in_template_declaration_(false), function_body_depth_(0)
{
	const char* const fundamentals[] = {
		"bool", "char", "char16_t", "char32_t", "double", "float", "int",
		"long", "short", "signed", "unsigned", "void", "wchar_t", "auto", "nullptr_t"
	};
	for (size_t i = 0; i < sizeof(fundamentals) / sizeof(*fundamentals); ++i)
		types_.insert(fundamentals[i]);
}

Parser::Mark Parser::Save() const
{
	Mark result = {position_, angle_depth_, ordinary_depth_, angle_floors_};
	return result;
}

void Parser::Restore(const Mark& mark)
{
	position_ = mark.position;
	angle_depth_ = mark.angle_depth;
	ordinary_depth_ = mark.ordinary_depth;
	angle_floors_ = mark.angle_floors;
}

const Token& Parser::Peek(size_t offset) const
{
	const size_t index = position_ + offset;
	return index < tokens_.size() ? tokens_[index] : tokens_.back();
}

bool Parser::AtEnd() const { return Peek().kind == AST_EOF; }
bool Parser::Is(const string& text) const { return Peek().text == text; }

bool Parser::Take(const string& text)
{
	if (!Is(text)) return false;
	++position_;
	return true;
}

bool Parser::TakeIdentifier(string* text)
{
	if (Peek().kind != AST_IDENTIFIER) return false;
	if (text != 0) *text = Peek().text;
	++position_;
	return true;
}

bool Parser::TakeLiteral(string* text)
{
	if (Peek().kind != AST_LITERAL) return false;
	if (text != 0) *text = Peek().text;
	++position_;
	return true;
}

void Parser::EnterAngle()
{
	angle_floors_.push_back(ordinary_depth_);
	++angle_depth_;
}

void Parser::LeaveAngle()
{
	if (!angle_floors_.empty()) angle_floors_.pop_back();
	if (angle_depth_ > 0) --angle_depth_;
}

bool Parser::TakeCloseAngle()
{
	if (Is(">") || Peek().kind == AST_RSHIFT_1 || Peek().kind == AST_RSHIFT_2)
	{
		++position_;
		return true;
	}
	return false;
}

bool Parser::TakeShiftRight()
{
	if (Peek().kind != AST_RSHIFT_1 || Peek(1).kind != AST_RSHIFT_2)
		return false;
	position_ += 2;
	return true;
}

bool Parser::CloseAngleBlocked() const
{
	return angle_depth_ != 0 && !angle_floors_.empty() &&
		ordinary_depth_ == angle_floors_.back();
}

CPPGMAstNodePtr Parser::Node(const string& kind, const string& value) const
{
	return CPPGMAstNodePtr(new CPPGMAstNode(kind, value));
}

void Parser::Add(const CPPGMAstNodePtr& parent,
	const CPPGMAstNodePtr& child) const
{
	if (child != 0) parent->children.push_back(child);
}

void Parser::RegisterType(const string& name)
{
	if (!name.empty()) types_.insert(name);
}

void Parser::RegisterTemplate(const string& name)
{
	if (!name.empty()) {
		templates_.insert(name);
		types_.insert(name);
	}
}

bool Parser::IsFundamental(const string& text) const
{
	static const char* const values[] = {
		"bool", "char", "char16_t", "char32_t", "double", "float", "int",
		"long", "short", "signed", "unsigned", "void", "wchar_t", "auto", "nullptr_t"
	};
	return Contains(values, values + sizeof(values) / sizeof(*values), text);
}

bool Parser::IsStorageOrFunctionSpecifier(const string& text) const
{
	static const char* const values[] = {
		"typedef", "extern", "static", "inline", "virtual", "constexpr",
		"thread_local", "register", "mutable", "friend", "explicit"
	};
	return Contains(values, values + sizeof(values) / sizeof(*values), text);
}

bool Parser::IsCv(const string& text) const
{
	return text == "const" || text == "volatile";
}

string Parser::TokenLabel(const string& text) const
{
	static const map<string, string> labels = {
		{"alignas", "KW_ALIGNAS"}, {"alignof", "KW_ALIGNOF"}, {"asm", "KW_ASM"},
		{"auto", "KW_AUTO"}, {"bool", "KW_BOOL"}, {"break", "KW_BREAK"},
		{"case", "KW_CASE"}, {"catch", "KW_CATCH"}, {"char", "KW_CHAR"},
		{"char16_t", "KW_CHAR16_T"}, {"char32_t", "KW_CHAR32_T"},
		{"class", "KW_CLASS"}, {"const", "KW_CONST"}, {"constexpr", "KW_CONSTEXPR"},
		{"const_cast", "KW_CONST_CAST"}, {"continue", "KW_CONTINUE"},
		{"decltype", "KW_DECLTYPE"}, {"default", "KW_DEFAULT"}, {"delete", "KW_DELETE"},
		{"do", "KW_DO"}, {"double", "KW_DOUBLE"}, {"dynamic_cast", "KW_DYNAMIC_CAST"},
		{"else", "KW_ELSE"}, {"enum", "KW_ENUM"}, {"explicit", "KW_EXPLICIT"},
		{"extern", "KW_EXTERN"}, {"false", "KW_FALSE"}, {"float", "KW_FLOAT"},
		{"for", "KW_FOR"}, {"friend", "KW_FRIEND"}, {"goto", "KW_GOTO"},
		{"if", "KW_IF"}, {"inline", "KW_INLINE"}, {"int", "KW_INT"},
		{"long", "KW_LONG"}, {"mutable", "KW_MUTABLE"}, {"namespace", "KW_NAMESPACE"},
		{"new", "KW_NEW"}, {"noexcept", "KW_NOEXCEPT"}, {"nullptr", "KW_NULLPTR"},
		{"operator", "KW_OPERATOR"}, {"private", "KW_PRIVATE"},
		{"protected", "KW_PROTECTED"}, {"public", "KW_PUBLIC"}, {"register", "KW_REGISTER"},
		{"reinterpret_cast", "KW_REINTERPET_CAST"}, {"return", "KW_RETURN"},
		{"short", "KW_SHORT"}, {"signed", "KW_SIGNED"}, {"sizeof", "KW_SIZEOF"},
		{"static", "KW_STATIC"}, {"static_assert", "KW_STATIC_ASSERT"},
		{"static_cast", "KW_STATIC_CAST"}, {"struct", "KW_STRUCT"},
		{"switch", "KW_SWITCH"}, {"template", "KW_TEMPLATE"}, {"this", "KW_THIS"},
		{"thread_local", "KW_THREAD_LOCAL"}, {"throw", "KW_THROW"},
		{"true", "KW_TRUE"}, {"try", "KW_TRY"}, {"typedef", "KW_TYPEDEF"},
		{"typeid", "KW_TYPEID"}, {"typename", "KW_TYPENAME"}, {"union", "KW_UNION"},
		{"unsigned", "KW_UNSIGNED"}, {"using", "KW_USING"}, {"virtual", "KW_VIRTUAL"},
		{"void", "KW_VOID"}, {"volatile", "KW_VOLATILE"}, {"wchar_t", "KW_WCHAR_T"},
		{"while", "KW_WHILE"}, {"final", "ST_FINAL"}, {"override", "ST_OVERRIDE"},
		{"{", "OP_LBRACE"}, {"}", "OP_RBRACE"}, {"[", "OP_LSQUARE"},
		{"]", "OP_RSQUARE"}, {"(", "OP_LPAREN"}, {")", "OP_RPAREN"},
		{"|", "OP_BOR"}, {"bitor", "OP_BOR"}, {"^", "OP_XOR"},
		{"xor", "OP_XOR"}, {"~", "OP_COMPL"}, {"compl", "OP_COMPL"},
		{"&", "OP_AMP"}, {"bitand", "OP_AMP"}, {"!", "OP_LNOT"},
		{"not", "OP_LNOT"}, {";", "OP_SEMICOLON"}, {":", "OP_COLON"},
		{"...", "OP_DOTS"}, {"?", "OP_QMARK"}, {"::", "OP_COLON2"},
		{".", "OP_DOT"}, {".*", "OP_DOTSTAR"}, {"+", "OP_PLUS"},
		{"-", "OP_MINUS"}, {"*", "OP_STAR"}, {"/", "OP_DIV"},
		{"%", "OP_MOD"}, {"=", "OP_ASS"}, {"<", "OP_LT"}, {">", "OP_GT"},
		{"+=", "OP_PLUSASS"}, {"-=", "OP_MINUSASS"}, {"*=", "OP_STARASS"},
		{"/=", "OP_DIVASS"}, {"%=", "OP_MODASS"}, {"^=", "OP_XORASS"},
		{"xor_eq", "OP_XORASS"}, {"&=", "OP_BANDASS"}, {"and_eq", "OP_BANDASS"},
		{"|=", "OP_BORASS"}, {"or_eq", "OP_BORASS"}, {"<<", "OP_LSHIFT"},
		{">>", "OP_RSHIFT"}, {">>=" , "OP_RSHIFTASS"}, {"<<=", "OP_LSHIFTASS"},
		{"==", "OP_EQ"}, {"!=", "OP_NE"}, {"not_eq", "OP_NE"}, {"<=", "OP_LE"},
		{">=", "OP_GE"}, {"&&", "OP_LAND"}, {"and", "OP_LAND"},
		{"||", "OP_LOR"}, {"or", "OP_LOR"}, {"++", "OP_INC"}, {"--", "OP_DEC"},
		{",", "OP_COMMA"}, {"->*", "OP_ARROWSTAR"}, {"->", "OP_ARROW"}
	};
	map<string, string>::const_iterator found = labels.find(text);
	if (found != labels.end()) return found->second;
	return text;
}

bool Parser::LooksLikeTypeName(const Token& token) const
{
	return token.kind == AST_IDENTIFIER &&
		(token.names.class_name || token.names.template_name ||
		 token.names.typedef_name || token.names.enum_name ||
		 types_.find(token.text) != types_.end());
}

bool Parser::IsNamedTypeStart() const
{
	if (Peek().kind != AST_IDENTIFIER) return false;
	if (value_names_.find(Peek().text) != value_names_.end() &&
		!(Peek(1).text == "::" &&
			namespaces_.find(Peek().text) != namespaces_.end())) return false;
	if (LooksLikeTypeName(Peek())) return true;
	return Peek(1).text == "::" || Peek(1).text == "<";
}

bool Parser::IsTypeStart() const
{
	if (IsFundamental(Peek().text) || IsCv(Peek().text) ||
		IsStorageOrFunctionSpecifier(Peek().text) || Peek().text == "class" ||
		Peek().text == "struct" || Peek().text == "union" || Peek().text == "enum" ||
		Peek().text == "decltype" || Peek().text == "typename") return true;
	return IsNamedTypeStart() || Peek().text == "::";
}

void Parser::SkipAttributes(vector<CPPGMAstNodePtr>* captured)
{
	while (true)
	{
		Mark mark = Save();
		if (Take("["))
		{
			if (Take("[") )
			{
				int depth = 1;
				while (depth != 0 && !AtEnd())
				{
					if (Take("[")) ++depth;
					else if (Take("]")) --depth;
					else ++position_;
				}
				if (depth == 0 && Take("]")) continue;
			}
		}
		Restore(mark);
		if (Take("__attribute__") && Take("("))
		{
			int depth = 1;
			while (depth != 0 && !AtEnd())
			{
				if (Take("(")) ++depth;
				else if (Take(")")) --depth;
				else ++position_;
			}
			if (depth == 0) continue;
		}
		Restore(mark);
		if (Take("alignas") && Take("("))
		{
			++ordinary_depth_;
			CPPGMAstNodePtr argument;
			// GNU's __alignof(T) is lexically an identifier, so a normal
			// type-id parse would misclassify the whole call as a type named
			// __alignof with a function declarator.  Preserve it as a typed
			// alignment operand before trying the ordinary alignas forms.
			Mark gnu_mark = Save();
			if (Take("__alignof") && Take("("))
			{
				CPPGMAstNodePtr operand = ParseTypeId();
				if (operand && Take(")"))
				{
					argument.reset(new CPPGMAstNode("gnu-alignof-expression", "__alignof"));
					Add(argument, operand);
				}
				else Restore(gnu_mark);
			}
			if (!argument)
			{
				Mark type_mark = Save();
				CPPGMAstNodePtr type = ParseTypeId();
				if (type && Is(")")) argument = type;
				else
				{
				// ParseTypeId may accept a prefix of an expression in an
				// ambiguous named-type position.  The closing parenthesis is
				// the disambiguating boundary for an alignment argument.
					Restore(type_mark);
					argument = ParseAssignmentExpression();
				}
			}
			if (argument && Take(")"))
			{
				--ordinary_depth_;
				if (captured)
				{
					CPPGMAstNodePtr attribute = Node("attribute");
					Add(attribute, argument);
					captured->push_back(attribute);
				}
				continue;
			}
			--ordinary_depth_;
			Restore(mark);
			return;
		}
		Restore(mark);
		return;
	}
}

CPPGMAstNodePtr Parser::Parse()
{
	CPPGMAstNodePtr root = Node("translation-unit");
	while (!AtEnd())
	{
		const Mark before = Save();
		CPPGMAstNodePtr declaration = ParseDeclaration(false);
		if (declaration == 0 || position_ == before.position)
		{
			std::cerr << "PA10 parse stopped at token " << position_ << " "
				<< Peek().text << "\n";
			return CPPGMAstNodePtr();
		}
		Add(root, declaration);
	}
	return root;
}

vector<Token> Parser::Normalize(const vector<PostPPToken>& input)
{
	vector<Token> result;
	for (size_t i = 0; i < input.size(); ++i)
	{
		const PostPPToken& source = input[i];
		if (source.kind == POST_PP_EOF) continue;
		if (source.kind == POST_PP_NON_WHITESPACE || source.kind == POST_PP_HEADER)
			throw logic_error("invalid token in source");
		if (source.kind == POST_PP_PUNCTUATOR && source.source == ">>")
		{
			result.push_back(Token(AST_RSHIFT_1, ">"));
			result.push_back(Token(AST_RSHIFT_2, ">"));
			continue;
		}
		Token token;
		token.text = source.source;
		if (source.kind == POST_PP_IDENTIFIER)
		{
			token.kind = IsKeywordToken(source.source) ? AST_PUNCTUATOR : AST_IDENTIFIER;
			token.names.class_name = source.source.find('C') != string::npos;
			token.names.template_name = source.source.find('T') != string::npos;
			token.names.typedef_name = source.source.find('Y') != string::npos;
			token.names.enum_name = source.source.find('E') != string::npos;
			token.names.namespace_name = source.source.find('N') != string::npos;
		}
		else if (source.kind == POST_PP_NUMBER || source.kind == POST_PP_CHARACTER ||
			source.kind == POST_PP_USER_CHARACTER || source.kind == POST_PP_STRING ||
			source.kind == POST_PP_USER_STRING)
			token.kind = AST_LITERAL;
		else token.kind = AST_PUNCTUATOR;
		result.push_back(token);
	}
	result.push_back(Token(AST_EOF, "ST_EOF"));
	return result;
}

bool IsKeywordToken(const string& text)
{
	static const char* const values[] = {
		"alignas", "alignof", "asm", "auto", "bool", "break", "case", "catch",
		"char", "char16_t", "char32_t", "class", "const", "constexpr", "const_cast",
		"continue", "decltype", "default", "delete", "do", "double", "dynamic_cast",
		"else", "enum", "explicit", "extern", "false", "float", "for", "friend", "goto",
		"if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept", "nullptr",
		"operator", "private", "protected", "public", "register", "reinterpret_cast", "return",
		"short", "signed", "sizeof", "static", "static_assert", "static_cast", "struct", "switch",
		"template", "this", "thread_local", "throw", "true", "try", "typedef", "typeid", "typename",
		"union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while", "and",
		"and_eq", "bitand", "bitor", "compl", "not", "not_eq", "or", "or_eq", "xor", "xor_eq",
		"final", "override", "export", "__attribute__"
	};
	return Contains(values, values + sizeof(values) / sizeof(*values), text);
}

} // namespace cppgm_pa10

CPPGMAstNodePtr ParsePA10TranslationUnit(const vector<PostPPToken>& tokens)
{
	const vector<cppgm_pa10::Token> normalized =
		cppgm_pa10::Parser::Normalize(tokens);
	return cppgm_pa10::Parser(normalized).Parse();
}

void PrintPA10Ast(const CPPGMAstNodePtr& node, ostream& output,
	unsigned int indentation)
{
	if (!node) return;
	// Attributes are syntax accepted and retained for later semantic stages,
	// but PA10's public AST format intentionally omits them.
	if (node->kind == "attribute") return;
	for (unsigned int i = 0; i < indentation; ++i) output << "  ";
	output << node->kind;
	if (!node->value.empty()) output << " " << node->value;
	output << '\n';
	for (size_t i = 0; i < node->children.size(); ++i)
		PrintPA10Ast(node->children[i], output, indentation + 1);
}
