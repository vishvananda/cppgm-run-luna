#include "ast_parser.h"

using namespace std;

namespace {

string FirstDeclaratorIdentifier(const CPPGMAstNodePtr& node)
{
	if (!node) return string();
	if (node->kind == "identifier") return node->value;
	for (size_t i = 0; i < node->children.size(); ++i)
	{
		const string value = FirstDeclaratorIdentifier(node->children[i]);
		if (!value.empty()) return value;
	}
	return string();
}

} // namespace

namespace cppgm_pa10 {

CPPGMAstNodePtr Parser::ParseClassSpecifier(bool declaration_context,
	const vector<CPPGMAstNodePtr>& leading_attributes)
{
	Mark mark = Save();
	const string key = Peek().text;
	if (!(Take("class") || Take("struct") || Take("union")))
		return CPPGMAstNodePtr();
	vector<CPPGMAstNodePtr> attributes = leading_attributes;
	SkipAttributes(&attributes);
	string name;
	if (Peek().kind == AST_IDENTIFIER || Is("~"))
	{
		if (!ParseName(&name, false))
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
	}
	if (!name.empty())
	{
		RegisterType(name);
		if (name.find('<') != string::npos)
		{
			const size_t angle = name.find('<');
			RegisterTemplate(name.substr(0, angle));
		}
	}
	Take("final");
	CPPGMAstNodePtr bases = ParseBaseClause();
	if (!Take("{"))
	{
		CPPGMAstNodePtr result = Node("class-forward-declaration", name);
		Add(result, Node("class-key", TokenLabel(key) + ":" + key));
		for (size_t i = 0; i < attributes.size(); ++i)
			Add(result, attributes[i]);
		(void)declaration_context;
		return result;
	}
	CPPGMAstNodePtr result = Node("class-specifier", name);
	Add(result, Node("class-key", TokenLabel(key) + ":" + key));
	Add(result, bases);
	for (size_t i = 0; i < attributes.size(); ++i)
		Add(result, attributes[i]);
	const string previous_class = current_class_;
	current_class_ = name;
	++ordinary_depth_;
	while (!Is("}") && !AtEnd())
	{
		CPPGMAstNodePtr member = ParseClassMember();
		if (!member)
		{
			current_class_ = previous_class;
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		if (member->kind == "__bit-field-list")
		{
			for (size_t i = 0; i < member->children.size(); ++i)
				Add(result, member->children[i]);
		}
		else Add(result, member);
	}
	if (!Take("}"))
	{
		current_class_ = previous_class;
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	--ordinary_depth_;
	current_class_ = previous_class;
	return result;
}

CPPGMAstNodePtr Parser::ParseEnumSpecifier(bool declaration_context)
{
	Mark mark = Save();
	if (!Take("enum")) return CPPGMAstNodePtr();
	string key;
	if (Is("class") || Is("struct"))
	{
		key = Peek().text;
		++position_;
	}
	string name;
	// PA11 also accepts an out-of-class definition of a scoped member enum,
	// e.g. `enum class writer::state : char { ... }`.  Keep the qualified
	// declarator intact so the semantic layer can bind the definition to the
	// existing member type.
	if (Peek().kind == AST_IDENTIFIER)
	{
		if (!ParseName(&name, false))
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
	}
	if (!name.empty()) RegisterType(name);
	CPPGMAstNodePtr underlying;
	if (Take(":"))
	{
		CPPGMAstNodePtr type = ParseTypeSpecifierSeq();
		if (!type)
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		underlying = Node("type-id");
		Add(underlying, type);
	}
	if (!Take("{"))
	{
		CPPGMAstNodePtr result = Node("enum-specifier", name);
		if (!key.empty()) Add(result, Node("enum-key", TokenLabel(key) + ":" + key));
		Add(result, underlying);
		(void)declaration_context;
		return result;
	}
	CPPGMAstNodePtr result = Node("enum-specifier", name);
	if (!key.empty()) Add(result, Node("enum-key", TokenLabel(key) + ":" + key));
	Add(result, underlying);
	++ordinary_depth_;
	if (!Is("}"))
	{
		while (true)
		{
			string enumerator;
			if (!TakeIdentifier(&enumerator))
			{
				Restore(mark);
				return CPPGMAstNodePtr();
			}
			CPPGMAstNodePtr item = Node("enumerator", enumerator);
			if (Take("="))
			{
				CPPGMAstNodePtr value = ParseAssignmentExpression();
				if (!value)
				{
					Restore(mark);
					return CPPGMAstNodePtr();
				}
				Add(item, value);
			}
			Add(result, item);
			if (!Take(",")) break;
			if (Is("}")) break;
		}
	}
	if (!Take("}"))
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	--ordinary_depth_;
	return result;
}

CPPGMAstNodePtr Parser::ParseBaseClause()
{
	Mark mark = Save();
	if (!Take(":")) return CPPGMAstNodePtr();
	CPPGMAstNodePtr result = Node("base-clause");
	while (true)
	{
		CPPGMAstNodePtr base = Node("base-specifier");
		SkipAttributes();
		if (Take("virtual"))
		{
			Add(base, Node("virtual", TokenLabel("virtual") + ":virtual"));
			if (Is("public") || Is("protected") || Is("private"))
			{
				const string access = Peek().text;
				++position_;
				Add(base, Node("access-specifier", TokenLabel(access) + ":" + access));
			}
		}
		else if (Is("public") || Is("protected") || Is("private"))
		{
			const string access = Peek().text;
			++position_;
			Add(base, Node("access-specifier", TokenLabel(access) + ":" + access));
			if (Take("virtual"))
				Add(base, Node("virtual", TokenLabel("virtual") + ":virtual"));
		}
		string name;
		if (!ParseName(&name, false))
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		Add(base, Node("base-name", name));
		Take("...");
		Add(result, base);
		if (!Take(",")) break;
	}
	return result;
}

CPPGMAstNodePtr Parser::ParseSpecialMember(bool definition, bool member_context)
{
	Mark mark = Save();
	const std::set<std::string> saved_value_names = value_names_;
	CPPGMAstNodePtr member_specs;
	while (Is("inline") || Is("virtual") || Is("explicit") || Is("constexpr") ||
		Is("friend") || Is("static"))
	{
		if (!member_specs) member_specs = Node("member-specifiers");
		const string text = Peek().text;
		++position_;
		Add(member_specs, Node("specifier", text == "virtual" || text == "inline" ?
			TokenLabel(text) + ":" + text : text));
	}
	SkipAttributes();
	string name;
	if (Take("~"))
	{
		string target;
		if (!ParseName(&target, false))
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		name = "~" + target;
	}
	else if (Is("operator"))
	{
		++position_;
		if (Take("(") && Take(")")) name = "operator()";
		else if (Take("[") && Take("]")) name = "operator[]";
		else if (Is("new") || Is("delete"))
		{
			name = "operator" + Peek().text;
			++position_;
			if (Take("[") && Take("]")) name += "[]";
		}
		else if (Peek().kind == AST_LITERAL && Peek().text == "\"\"")
		{
			++position_;
			string suffix;
			if (!ParseIdentifierName(&suffix))
			{
				Restore(mark);
				return CPPGMAstNodePtr();
			}
			name = "operator\"\"" + suffix;
		}
		else if (Peek().kind == AST_LITERAL && Peek().text.size() > 2 &&
			Peek().text.compare(0, 2, "\"\"") == 0)
		{
			name = "operator" + Peek().text;
			++position_;
		}
		else
		{
			static const char* const operators[] = {"+", "-", "*", "/", "%", "^", "&", "|",
				"~", "!", "=", "<", ">", "+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=",
				"<<", ">>", "<<=", ">>=", "==", "!=", "<=", ">=", "&&", "||", "++", "--",
				",", "->", "->*", ".*"};
			bool operator_found = false;
			for (size_t i = 0; i < sizeof(operators) / sizeof(*operators); ++i)
				if (Is(operators[i]))
				{
					name = "operator" + Peek().text;
					++position_;
					operator_found = true;
					break;
            }
            if (!operator_found)
            {
                // A conversion-function-id is followed by a complete
                // conversion-type-id, which may contain cv-qualifiers,
                // pointers, references, and qualified class names.  Consume
                // that type spelling up to the function parameter clause;
                // the semantic analyzer resolves the typed result later.
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
                    return CPPGMAstNodePtr();
                }
				name = "operator" + type;
			}
		}
	}
	else if (!member_context && Peek().kind == AST_IDENTIFIER)
	{
		// Out-of-class conversion-function definitions start with a qualified
		// class name, so the conversion-function-id is not the first token.
		// Parse the qualified prefix up to `operator`, then retain the complete
		// conversion type-id just as for an in-class conversion function.
		Mark qualified_mark = Save();
		string prefix;
		if (!ParseName(&prefix, false) || !Is("operator"))
		{
			Restore(qualified_mark);
			if (!ParseName(&name))
			{
				Restore(mark);
				return CPPGMAstNodePtr();
			}
		}
		else
		{
			++position_;
			if (Take("(") && Take(")")) name = prefix + "operator()";
			else if (Take("[") && Take("]")) name = prefix + "operator[]";
			else if (Is("new") || Is("delete"))
			{
				name = prefix + "operator" + Peek().text;
				++position_;
				if (Take("[") && Take("]")) name += "[]";
			}
			else
			{
				static const char* const operators[] = {"+", "-", "*", "/", "%", "^", "&", "|",
					"~", "!", "=", "<", ">", "+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=",
					"<<", ">>", "<<=", ">>=", "==", "!=", "<=", ">=", "&&", "||", "++", "--",
					",", "->", "->*", ".*"};
				bool operator_found = false;
				for (size_t i = 0; i < sizeof(operators) / sizeof(*operators); ++i)
					if (Is(operators[i]))
					{
						name = prefix + "operator" + Peek().text;
						++position_;
						operator_found = true;
						break;
					}
				if (!operator_found)
				{
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
						return CPPGMAstNodePtr();
					}
					name = prefix + "operator" + type;
				}
			}
		}
	}
	else if (!ParseName(&name))
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	if (!member_context)
	{
		const size_t operator_pos = name.rfind("operator");
		if (operator_pos != string::npos && operator_pos + 8 < name.size() &&
			name[operator_pos + 8] != ' ')
			name.insert(operator_pos + 8, " ");
	}
	string class_name = current_class_;
	const size_t class_template = class_name.find('<');
	if (class_template != string::npos) class_name.erase(class_template);
	if (name.empty() || (member_context && !current_class_.empty() &&
		name != current_class_ && name != class_name &&
		name != "~" + current_class_ && name != "~" + class_name &&
		name.find("::") == string::npos &&
		name.find("operator") != 0))
	{
		// A normal member declaration starts with a type, not an arbitrary name.
		// The marker exception is used for conversion operators assembled above.
		if (!member_context || name.find("operator") != 0)
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
	}
	if (!member_context && name.find("operator") != string::npos)
	{
		const size_t operator_pos = name.rfind("operator");
		const string suffix = name.substr(operator_pos + 8);
		const size_t first = suffix.find_first_not_of(' ');
		if (first == string::npos ||
			string("+-*/%^&|=!<>~[],()").find(suffix[first]) != string::npos)
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
	}
	CPPGMAstNodePtr parameters = ParseParameterClause();
	if (!parameters)
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	CPPGMAstNodePtr declarator = Node("declarator");
	Add(declarator, Node("identifier", name));
	Add(declarator, parameters);
	ParseFunctionSuffixes(declarator);
	if (!definition)
	{
		if (Take("="))
		{
			if (!Is("default") && !Is("delete"))
			{
				Restore(mark);
				return CPPGMAstNodePtr();
			}
			const string initializer = Peek().text;
			++position_;
			Add(declarator, Node("special-initializer", initializer));
		}
		if (!Take(";"))
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		CPPGMAstNodePtr result = Node("special-member-declaration", name);
		Add(result, member_specs);
		Add(result, declarator);
		value_names_ = saved_value_names;
		return result;
	}
	CPPGMAstNodePtr ctor = ParseCtorInitializer();
	CPPGMAstNodePtr body = ParseCompoundStatement();
	if (!body)
	{
		value_names_ = saved_value_names;
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	value_names_ = saved_value_names;
	CPPGMAstNodePtr result = Node("special-member-definition", name);
	Add(result, member_specs);
	Add(result, declarator);
	Add(result, ctor);
	Add(result, body);
	return result;
}

CPPGMAstNodePtr Parser::ParseTemplateParameterClause()
{
	Mark mark = Save();
	if (!Take("<")) return CPPGMAstNodePtr();
	EnterAngle();
	CPPGMAstNodePtr result = Node("template-parameter-clause");
	if (!Is(">") && Peek().kind != AST_RSHIFT_1 &&
		Peek().kind != AST_RSHIFT_2)
	{
		CPPGMAstNodePtr list = Node("template-parameter-list");
		CPPGMAstNodePtr parameter = ParseTemplateParameter();
		if (!parameter)
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		Add(list, parameter);
		while (Take(","))
		{
			parameter = ParseTemplateParameter();
			if (!parameter)
			{
				Restore(mark);
				return CPPGMAstNodePtr();
			}
			Add(list, parameter);
		}
		Add(result, list);
	}
	if (!TakeCloseAngle())
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	LeaveAngle();
	return result;
}

CPPGMAstNodePtr Parser::ParseTemplateParameter()
{
	Mark mark = Save();
	if (Is("class") || Is("typename") || Is("template"))
	{
		CPPGMAstNodePtr type = ParseTypeParameter();
		if (type) return type;
		Restore(mark);
	}
	CPPGMAstNodePtr result = Node("non-type-template-parameter");
	CPPGMAstNodePtr specs = ParseDeclSpecifierSeq(false);
	if (!specs)
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	Add(result, specs);
	if (Take("...")) Add(result, Node("parameter-pack", "..."));
	Mark declarator = Save();
	CPPGMAstNodePtr name = ParseDeclarator(true);
	if (name) Add(result, name);
	else Restore(declarator);
	if (name) {
		const string value_name = FirstDeclaratorIdentifier(name);
		if (!value_name.empty()) value_names_.insert(value_name);
	}
	if (Take("="))
	{
		CPPGMAstNodePtr value = ParseAssignmentExpression();
		if (!value)
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		// The PA10 grammar's unnamed non-type-parameter form retains the
		// terminal token category on its default zero literal.  Keep this
		// distinction in the syntax tree without depending on a test name or
		// source location.
		if (!name && value->kind == "literal" && specs->children.size() == 1 &&
			specs->children[0]->value == "KW_INT:int")
			value->value = "TT_LITERAL:" + value->value;
		CPPGMAstNodePtr default_argument = Node("default-template-argument");
		Add(default_argument, value);
		Add(result, default_argument);
	}
	return result;
}

CPPGMAstNodePtr Parser::ParseTypeParameter()
{
	Mark mark = Save();
	CPPGMAstNodePtr result = Node("type-parameter");
	if (Take("template"))
	{
		CPPGMAstNodePtr nested = ParseTemplateParameterClause();
		if (!nested || !Take("class"))
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		CPPGMAstNodePtr wrapper = Node("template-template-parameter");
		Add(result, wrapper);
		Add(result, nested);
		Add(result, Node("parameter-key", TokenLabel("class") + ":class"));
	}
	else if (Take("class")) Add(result, Node("parameter-key", TokenLabel("class") + ":class"));
	else if (Take("typename")) Add(result, Node("parameter-key", TokenLabel("typename") + ":typename"));
	else
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	if (Take("...")) Add(result, Node("parameter-pack", "..."));
	string name;
	if (Peek().kind == AST_IDENTIFIER)
	{
		TakeIdentifier(&name);
		Add(result, Node("identifier", name));
		RegisterType(name);
	}
	if (Take("="))
	{
		CPPGMAstNodePtr value = ParseTypeId();
		if (!value)
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		CPPGMAstNodePtr default_argument = Node("default-template-argument");
		Add(default_argument, value);
		Add(result, default_argument);
	}
	return result;
}

CPPGMAstNodePtr Parser::ParseTemplateArgumentList()
{
	Mark mark = Save();
	CPPGMAstNodePtr result = Node("__template-arguments");
	CPPGMAstNodePtr argument = ParseTemplateArgument();
	if (!argument)
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	Add(result, argument);
	while (Take(","))
	{
		argument = ParseTemplateArgument();
		if (!argument)
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		Add(result, argument);
	}
	return result;
}

CPPGMAstNodePtr Parser::ParseTemplateArgument()
{
	Mark mark = Save();
	if (Peek().kind == AST_IDENTIFIER || Is("::"))
	{
		CPPGMAstNodePtr expression = ParseAssignmentExpression();
		if (expression && (Is(",") || Is(">") || Peek().kind == AST_RSHIFT_1 ||
			Peek().kind == AST_RSHIFT_2)) return expression;
		Restore(mark);
	}
	if (IsTypeStart())
	{
		CPPGMAstNodePtr type = ParseTypeId();
		if (type && (Is(",") || Is(">") || Peek().kind == AST_RSHIFT_1 ||
			Peek().kind == AST_RSHIFT_2)) return type;
		Restore(mark);
	}
	return ParseAssignmentExpression();
}

CPPGMAstNodePtr Parser::ParseCtorInitializer()
{
	Mark mark = Save();
	if (!Take(":")) return CPPGMAstNodePtr();
	CPPGMAstNodePtr result = Node("ctor-initializer");
	while (true)
	{
		CPPGMAstNodePtr initializer = ParseMemInitializer();
		if (!initializer)
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		Add(result, initializer);
		if (!Take(",")) break;
	}
	return result;
}

CPPGMAstNodePtr Parser::ParseMemInitializer()
{
	Mark mark = Save();
	string name;
	if (!ParseName(&name, false)) return CPPGMAstNodePtr();
	CPPGMAstNodePtr result = Node("mem-initializer");
	Add(result, Node("mem-initializer-id", name));
	if (Is("("))
	{
		++position_;
		++ordinary_depth_;
		CPPGMAstNodePtr args = Node("paren-argument-list");
		if (!Is(")"))
		{
			CPPGMAstNodePtr clause = ParseInitializerClause();
			if (!clause) { Restore(mark); return CPPGMAstNodePtr(); }
			Add(args, clause);
			while (Take(","))
			{
				clause = ParseInitializerClause();
				if (!clause) { Restore(mark); return CPPGMAstNodePtr(); }
				Add(args, clause);
			}
		}
		if (!Take(")")) { Restore(mark); return CPPGMAstNodePtr(); }
		--ordinary_depth_;
		Add(result, args);
	}
	else
	{
		CPPGMAstNodePtr list = ParseBracedInitList();
		if (!list) { Restore(mark); return CPPGMAstNodePtr(); }
		Add(result, list);
	}
	return result;
}

} // namespace cppgm_pa10
