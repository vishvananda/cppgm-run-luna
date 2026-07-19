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
	if (value_names_.find(*value) != value_names_.end()) return false;
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
			tokens_[i - 1].text == "template")) raw += " ";
		raw += tokens_[i].text;
	}
	if (ordinary_depth_ != 0 && templates_.find(*value) == templates_.end() &&
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
	else if (Peek().kind == AST_IDENTIFIER)
	{
		string type;
		ParseIdentifierName(&type);
		result += type;
		if (allow_template) ParseTemplateSuffix(&result);
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
			if (allow_template) ParseTemplateSuffix(&result);
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
			if (allow_template) ParseTemplateSuffix(&result);
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
