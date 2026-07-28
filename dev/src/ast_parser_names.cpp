#include "ast_parser.h"

using namespace std;

namespace cppgm_pa10 {

bool Parser::ParseIdentifierName(string* value)
{
	if (Peek().kind != AST_IDENTIFIER) return false;
	*value = Peek().text;
	++position_;
	return true;
}

bool Parser::ParseTemplateSuffix(string* value)
{
	if (!Is("<")) return false;
	if (value_names_.find(*value) != value_names_.end()) {
		return false;
	}
	if (CloseAngleBlocked() && value->find("::") != string::npos)
	{
		const size_t separator = value->rfind("::");
		const string component = value->substr(separator + 2);
		if (component.find("template ") != 0 &&
			templates_.find(component) == templates_.end() &&
			types_.find(component) == types_.end()) return false;
	}
	const Mark mark = Save();
	const size_t begin = position_;
	++position_;
	EnterAngle();
	if (!((Is(">") || Peek().kind == AST_RSHIFT_1 || Peek().kind == AST_RSHIFT_2)))
	{
		if (!ParseTemplateArgumentList())
		{
			Restore(mark);
			return false;
		}
	}
	if (!TakeCloseAngle())
	{
		Restore(mark);
		return false;
	}
	LeaveAngle();
	const size_t raw_begin = begin;
	string raw;
	for (size_t i = raw_begin; i < position_; ++i)
	{
		if (i != raw_begin && (tokens_[i - 1].text == "const" ||
			tokens_[i - 1].text == "volatile" ||
			tokens_[i - 1].text == "typename" ||
			tokens_[i - 1].text == "template" ||
			tokens_[i - 1].text == "class" ||
			tokens_[i - 1].text == "struct" ||
			tokens_[i - 1].text == "union" ||
			tokens_[i - 1].text == "enum")) raw += " ";
		raw += tokens_[i].text;
	}
	// A class or namespace member declaration can legitimately use a boolean
	// expression in a template argument (`enable_if_t<A || B, T>`).  The
	// ambiguity guard is only needed once an ordinary expression is nested
	// inside a function/body construct; at class scope treating the complete
	// template-id as a name is the declaration grammar's unambiguous choice.
	if ((function_body_depth_ != 0 || ordinary_depth_ > 1) &&
		value->find("::") == string::npos && raw.find("||") != string::npos &&
		Peek().text != "{" && Peek().text != ";" && Peek().text != "," &&
		Peek().text != ">" && Peek().text != ")" && Peek().text != "::")
	{
		Restore(mark);
		return false;
	}
	*value += raw;
	return true;
}

bool Parser::ParseOperatorName(string* value, bool allow_template)
{
	const Mark mark = Save();
	if (!Take("operator")) return false;
	string result = "operator";
	if (Take("(") && Take(")")) result += "()";
	else if (Take("[") && Take("]")) result += "[]";
	else if (Is("new") || Is("delete"))
	{
		result += Peek().text;
		++position_;
		if (Take("[") && Take("]")) result += "[]";
	}
	else if (Peek().kind == AST_LITERAL && Peek().text == "\"\"")
	{
		++position_;
		string suffix;
		if (!ParseIdentifierName(&suffix))
		{
			Restore(mark);
			return false;
		}
		result += "\"\"" + suffix;
	}
	else if (Peek().kind == AST_LITERAL && Peek().text.size() > 2 &&
		Peek().text.compare(0, 2, "\"\"") == 0)
	{
		// The post-token lexer keeps a user-defined string literal as one
		// typed token.  In an operator declaration the spelling is still the
		// empty literal followed by its identifier suffix, so retain the
		// canonical operator name while consuming that single token.
		result += Peek().text;
		++position_;
	}
	else if (Peek().kind == AST_IDENTIFIER)
	{
		// Conversion-function-ids can name a qualified class type and may carry
		// cv/ref spelling before the call parentheses.  Keep that complete
		// type-id together so an explicit conversion call remains one name.
		string type;
		while (!AtEnd() && !Is("("))
		{
			if (!type.empty() &&
			    (isalnum(static_cast<unsigned char>(type[type.size() - 1])) ||
			     type[type.size() - 1] == '_') &&
			    (isalnum(static_cast<unsigned char>(Peek().text[0])) ||
			     Peek().text[0] == '_')) type += " ";
			type += Peek().text;
			++position_;
		}
		if (type.empty())
		{
			Restore(mark);
			return false;
		}
		result += type;
	}
	else if (IsFundamental(Peek().text))
	{
		result += " " + Peek().text;
		++position_;
	}
	else if (Is("typename"))
	{
		++position_;
		string type;
		if (!ParseName(&type, false))
		{
			Restore(mark);
			return false;
		}
		result += " typename " + type;
	}
	else
	{
		// Normalize() splits a shift-right token while a template argument
		// list is open.  An operator-function-id still needs the two token
		// spelling to be recognized as one `operator>>` name.
		if (Peek().kind == AST_RSHIFT_1 && Peek(1).kind == AST_RSHIFT_2)
		{
			result += ">>";
			TakeShiftRight();
		}
		else
		{
		const string op = Peek().text;
		static const char* const operators[] = {"+", "-", "*", "/", "%", "^", "&", "|",
			"~", "!", "=", "<", ">", "+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=",
			"<<", ">>", "<<=", ">>=", "==", "!=", "<=", ">=", "&&", "||", "++", "--",
			",", "->", "->*", ".*"};
		bool found = false;
		for (size_t i = 0; i < sizeof(operators) / sizeof(*operators); ++i)
			if (op == operators[i]) found = true;
		if (!found)
		{
			Restore(mark);
			return false;
		}
		result += op;
		++position_;
		}
	}
	if (allow_template) ParseTemplateSuffix(&result);
	*value = result;
	return true;
}

bool Parser::ParseName(string* value, bool allow_operator, bool allow_template)
{
	const Mark mark = Save();
	string result;
	if (Take("::")) result = "::";
	bool any = false;
	while (true)
	{
		if (Is("decltype"))
		{
			const size_t begin = position_;
			++position_;
			if (!Take("("))
			{
				Restore(mark);
				return false;
			}
			++ordinary_depth_;
			CPPGMAstNodePtr expression = ParseExpression();
			if (!expression || !Take(")"))
			{
				Restore(mark);
				return false;
			}
			--ordinary_depth_;
			for (size_t i = begin; i < position_; ++i) result += tokens_[i].text;
			any = true;
		}
		else if (allow_operator && Is("operator"))
		{
			string operator_name;
			if (!ParseOperatorName(&operator_name, allow_template))
			{
				Restore(mark);
				return false;
			}
			result += operator_name;
			any = true;
		}
		else if (Take("template"))
		{
			string identifier;
			if (!ParseIdentifierName(&identifier))
			{
				Restore(mark);
				return false;
			}
			result += "template " + identifier;
			if (allow_template && Is("<")) {
				ParseTemplateSuffix(&result);
			}
			any = true;
		}
		else if (Take("~"))
		{
			string target;
			if (!ParseIdentifierName(&target))
			{
				Restore(mark);
				return false;
			}
			result += "~" + target;
			any = true;
		}
		else
		{
			string identifier;
			if (!ParseIdentifierName(&identifier)) break;
			result += identifier;
			if (allow_template && Is("<")) {
				ParseTemplateSuffix(&result);
			}
			any = true;
		}
		if (!Take("::")) break;
		result += "::";
	}
	if (!any)
	{
		Restore(mark);
		return false;
	}
	*value = result;
	return true;
}

CPPGMAstNodePtr Parser::ParseIdExpression()
{
	Mark mark = Save();
	string name;
	if (!ParseName(&name))
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	return Node("id-expression", name);
}

CPPGMAstNodePtr Parser::ParseNamedType(bool type_node)
{
	string name;
	if (!ParseName(&name, false)) return CPPGMAstNodePtr();
	if (type_node) return Node("type-name", name);
	return Node("decl-specifier", name);
}

} // namespace cppgm_pa10
