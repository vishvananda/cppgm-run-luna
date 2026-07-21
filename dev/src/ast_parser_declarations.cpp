#include "ast_parser.h"

using namespace std;

namespace {

string FirstIdentifier(const CPPGMAstNodePtr& node)
{
	if (!node) return string();
	if (node->kind == "identifier") return node->value;
	for (size_t i = 0; i < node->children.size(); ++i)
	{
		const string result = FirstIdentifier(node->children[i]);
		if (!result.empty()) return result;
	}
	return string();
}

bool HasChildKind(const CPPGMAstNodePtr& node, const string& kind)
{
	if (!node) return false;
	for (size_t i = 0; i < node->children.size(); ++i)
		if (node->children[i] && node->children[i]->kind == kind) return true;
	return false;
}

} // namespace

namespace cppgm_pa10 {

CPPGMAstNodePtr Parser::ParseDeclaration(bool member_context)
{
	vector<CPPGMAstNodePtr> leading_attributes;
	SkipAttributes(&leading_attributes);
	if (Take(";")) return Node("empty-declaration");
	if (Is("namespace") || (Is("inline") && Peek(1).text == "namespace"))
	{
		Mark mark = Save();
		CPPGMAstNodePtr alias = ParseNamespaceAliasDefinition();
		if (alias) return alias;
		Restore(mark);
		return ParseNamespaceDefinition();
	}
	if (Is("using"))
	{
		Mark mark = Save();
		CPPGMAstNodePtr alias = ParseUsingDeclaration(false);
		if (alias) return alias;
		Restore(mark);
		return ParseUsingDeclaration(true);
	}
	if (Is("static_assert")) return ParseStaticAssertDeclaration();
	if (Is("extern") && Peek(1).kind == AST_LITERAL)
		return ParseLinkageSpecification();
	if (Is("extern") && Peek(1).text == "template")
		return ParseExplicitInstantiation();
	if (Is("template") && Peek(1).text != "<")
	{
		Mark mark = Save();
		CPPGMAstNodePtr explicit_instantiation = ParseExplicitInstantiation();
		if (explicit_instantiation) return explicit_instantiation;
		Restore(mark);
	}
	if (Is("template")) return ParseTemplateDeclaration(member_context);
	if (Is("class") || Is("struct") || Is("union"))
	{
		Mark mark = Save();
		CPPGMAstNodePtr result = ParseClassSpecifier(true, leading_attributes);
		if (result && Take(";")) return result;
		Restore(mark);
	}
	// Attributes before a non-class declaration belong to that declaration,
	// not to a later class parsed after a speculative branch.  The PA15 layout
	// service currently consumes only class alignment attributes.
	if (Is("enum"))
	{
		Mark mark = Save();
		CPPGMAstNodePtr result = ParseEnumSpecifier(true);
		if (result && Take(";")) return result;
		Restore(mark);
	}
	if (member_context || Is("~") || Is("operator") || Is("inline") ||
		Is("virtual") || Is("explicit") || Is("constexpr") || Is("friend") ||
		Is("static") || (Peek().kind == AST_IDENTIFIER &&
			(Peek(1).text == "::" || Peek(1).text == "<")))
	{
		Mark mark = Save();
		CPPGMAstNodePtr special = ParseSpecialMember(false, member_context);
		if (special) return special;
		Restore(mark);
		mark = Save();
		CPPGMAstNodePtr special_definition = ParseSpecialMember(true, member_context);
		if (special_definition) return special_definition;
		Restore(mark);
		if (!member_context && Peek().kind == AST_IDENTIFIER &&
			Peek(1).text == "::" && Peek(2).text == "operator")
			return CPPGMAstNodePtr();
	}
	return ParseSimpleOrFunctionDeclaration(member_context);
}

CPPGMAstNodePtr Parser::ParseNamespaceDefinition()
{
	Mark mark = Save();
	CPPGMAstNodePtr result = Node("namespace-definition");
	const bool is_inline = Take("inline");
	if (!Take("namespace"))
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	string name;
	if (Peek().kind == AST_IDENTIFIER)
	{
		TakeIdentifier(&name);
		result->value = name;
		namespaces_.insert(name);
	}
	else result->value = "<unnamed>";
	if (is_inline) Add(result, Node("inline"));
	if (!Take("{"))
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	++ordinary_depth_;
	while (!Is("}") && !AtEnd())
	{
		CPPGMAstNodePtr declaration = ParseDeclaration(false);
		if (!declaration)
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		Add(result, declaration);
	}
	if (!Take("}"))
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	--ordinary_depth_;
	return result;
}

CPPGMAstNodePtr Parser::ParseNamespaceAliasDefinition()
{
	Mark mark = Save();
	if (!Take("namespace")) return CPPGMAstNodePtr();
	string name;
	if (!TakeIdentifier(&name) || !Take("="))
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	string target;
	if (!ParseName(&target))
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	if (!Take(";"))
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	namespaces_.insert(name);
	CPPGMAstNodePtr result = Node("namespace-alias-definition", name);
	Add(result, Node("target", target));
	return result;
}

CPPGMAstNodePtr Parser::ParseUsingDeclaration(bool directive)
{
	Mark mark = Save();
	if (!Take("using")) return CPPGMAstNodePtr();
	if (directive)
	{
		if (!Take("namespace"))
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		string target;
		if (!ParseName(&target) || !Take(";"))
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		CPPGMAstNodePtr result = Node("using-directive");
		Add(result, Node("target", target));
		return result;
	}
	string name;
	if (TakeIdentifier(&name) && Take("="))
	{
		CPPGMAstNodePtr type = ParseTypeId();
		if (!type || !Take(";"))
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		RegisterType(name);
		CPPGMAstNodePtr result = Node("alias-declaration", name);
		Add(result, type);
		return result;
	}
	Restore(mark);
	if (!Take("using")) return CPPGMAstNodePtr();
	Take("typename");
	string target;
	if (!ParseName(&target) || !Take(";"))
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	CPPGMAstNodePtr result = Node("using-declaration");
	Add(result, Node("target", target));
	return result;
}

CPPGMAstNodePtr Parser::ParseLinkageSpecification()
{
	Mark mark = Save();
	if (!Take("extern") || Peek().kind != AST_LITERAL)
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	string language;
	TakeLiteral(&language);
	if (language.size() >= 2 && language[0] == '"' &&
		language[language.size() - 1] == '"')
		language = language.substr(1, language.size() - 2);
	CPPGMAstNodePtr result = Node("linkage-specification", language);
	if (!Take("{"))
	{
		CPPGMAstNodePtr declaration = ParseDeclaration(false);
		if (!declaration)
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		Add(result, declaration);
		return result;
	}
	++ordinary_depth_;
	while (!Is("}") && !AtEnd())
	{
		CPPGMAstNodePtr declaration = ParseDeclaration(false);
		if (!declaration)
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		Add(result, declaration);
	}
	if (!Take("}"))
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	--ordinary_depth_;
	return result;
}

CPPGMAstNodePtr Parser::ParseTemplateDeclaration(bool member_context)
{
	Mark mark = Save();
	if (!Take("template")) return CPPGMAstNodePtr();
	CPPGMAstNodePtr parameters = ParseTemplateParameterClause();
	if (!parameters)
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	CPPGMAstNodePtr declaration = ParseDeclaration(member_context);
	if (!declaration)
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	// A class template's class-specifier spelling does not carry its
	// parameter list (the parameters belong to the enclosing declaration),
	// so ParseClassSpecifier cannot register the name as a template itself.
	// Register it here for later declaration-versus-expression disambiguation
	// in function bodies.
	if (declaration->kind == "class-specifier" ||
		declaration->kind == "class-forward-declaration") {
		string name = declaration->value;
		const size_t separator = name.rfind("::");
		if (separator != string::npos) name.erase(0, separator + 2);
		RegisterTemplate(name);
	} else if (declaration->kind == "simple-declaration" ||
		declaration->kind == "function-definition") {
		// Qualified variable and function template-ids must remain parseable
		// while they occur inside another template argument list.  Class
		// templates are registered above; register the other template names in
		// the parser's template set as well, without making variable templates
		// type names.
		string name = FirstIdentifier(declaration);
		const size_t open = name.find('<');
		if (open != string::npos) name.erase(open);
		const size_t separator = name.rfind("::");
		if (separator != string::npos) name.erase(0, separator + 2);
		if (!name.empty()) templates_.insert(name);
	}
	CPPGMAstNodePtr result = Node("template-declaration");
	Add(result, parameters);
	Add(result, declaration);
	return result;
}

CPPGMAstNodePtr Parser::ParseExplicitInstantiation()
{
	Mark mark = Save();
	Take("extern");
	if (!Take("template"))
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	CPPGMAstNodePtr target;
	if (Is("class") || Is("struct") || Is("union"))
	{
		target = ParseClassSpecifier(true);
		if (target && !Take(";"))
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
	}
	else target = ParseSimpleOrFunctionDeclaration(false);
	if (!target)
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	CPPGMAstNodePtr result = Node("explicit-instantiation-declaration");
	Add(result, target);
	return result;
}

CPPGMAstNodePtr Parser::ParseStaticAssertDeclaration()
{
	Mark mark = Save();
	if (!Take("static_assert") || !Take("("))
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	++ordinary_depth_;
	CPPGMAstNodePtr condition = ParseAssignmentExpression();
	if (!condition)
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	CPPGMAstNodePtr result = Node("static-assert-declaration");
	Add(result, condition);
	if (Take(","))
	{
		string message;
		if (!TakeLiteral(&message))
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		Add(result, Node("message", message));
	}
	if (!Take(")") || !Take(";"))
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	--ordinary_depth_;
	return result;
}

CPPGMAstNodePtr Parser::ParseSimpleOrFunctionDeclaration(bool member_context)
{
	Mark mark = Save();
	const std::set<std::string> saved_value_names = value_names_;
	CPPGMAstNodePtr specifiers = ParseDeclSpecifierSeq(false);
	if (!specifiers)
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	CPPGMAstNodePtr result;
	vector<CPPGMAstNodePtr> declarators;
	CPPGMAstNodePtr first_declarator;
	if (!Is(";"))
	{
		CPPGMAstNodePtr first = ParseDeclarator(false);
		if (!first)
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		first_declarator = first;
		CPPGMAstNodePtr first_init;
		if (Is("=") || ((Is("{") || Is("(")) &&
			!HasChildKind(first, "parameter-clause"))) first_init = ParseInitializer();
		CPPGMAstNodePtr first_item = Node("init-declarator");
		Add(first_item, first);
		Add(first_item, first_init);
		declarators.push_back(first_item);
		while (Take(","))
		{
			CPPGMAstNodePtr next = ParseDeclarator(false);
			if (!next)
			{
				Restore(mark);
				return CPPGMAstNodePtr();
			}
			CPPGMAstNodePtr next_init;
			if (Is("=") || ((Is("{") || Is("(")) &&
				!HasChildKind(next, "parameter-clause"))) next_init = ParseInitializer();
			CPPGMAstNodePtr next_item = Node("init-declarator");
			Add(next_item, next);
			Add(next_item, next_init);
			declarators.push_back(next_item);
		}
	}
	if (declarators.size() == 1 && Is("{") && first_declarator)
	{
		result = Node("function-definition");
		Add(result, specifiers);
		Add(result, first_declarator);
		CPPGMAstNodePtr body = ParseCompoundStatement();
		if (!body)
		{
			value_names_ = saved_value_names;
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		value_names_ = saved_value_names;
		Add(result, body);
	}
	else
	{
		if (!Take(";"))
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		result = Node("simple-declaration");
		Add(result, specifiers);
		if (!declarators.empty())
		{
			CPPGMAstNodePtr list = Node("init-declarator-list");
			for (size_t i = 0; i < declarators.size(); ++i) Add(list, declarators[i]);
			Add(result, list);
		}
	}
	if (HasChildKind(specifiers, "class-specifier") ||
		HasChildKind(specifiers, "enum-specifier"))
	{
		// Anonymous aggregate names introduced by typedef are usable type names.
		if (!declarators.empty()) RegisterType(FirstIdentifier(declarators[0]));
	}
	if (specifiers->children.size() > 0 &&
		specifiers->children[0]->kind == "decl-specifier" &&
		specifiers->children[0]->value == "KW_TYPEDEF:typedef")
	{
		for (size_t i = 0; i < declarators.size(); ++i)
			RegisterType(FirstIdentifier(declarators[i]));
	}
	value_names_ = saved_value_names;
	(void)member_context;
	return result;
}

CPPGMAstNodePtr Parser::ParseClassMember()
{
	if (Is("public") || Is("private") || Is("protected"))
	{
		const string access = Peek().text;
		++position_;
		if (!Take(":")) return CPPGMAstNodePtr();
		return Node("access-specifier", TokenLabel(access) + ":" + access);
	}
	if (Is("template")) return ParseTemplateDeclaration(true);
	{
		Mark mark = Save();
		CPPGMAstNodePtr bit = ParseBitFieldDeclaration();
		if (bit) return bit;
		Restore(mark);
	}
	{
		Mark mark = Save();
		CPPGMAstNodePtr special = ParseSpecialMember(false, true);
		if (special) return special;
		Restore(mark);
		mark = Save();
		special = ParseSpecialMember(true, true);
		if (special) return special;
		Restore(mark);
	}
	return ParseDeclaration(true);
}

CPPGMAstNodePtr Parser::ParseBitFieldDeclaration()
{
	Mark mark = Save();
	CPPGMAstNodePtr specs = ParseDeclSpecifierSeq(false);
	if (!specs) return CPPGMAstNodePtr();
	vector<CPPGMAstNodePtr> fields;
	while (true)
	{
		CPPGMAstNodePtr declarator;
		Mark field_mark = Save();
		if (!Is(":")) declarator = ParseDeclarator(false);
		if (!Take(":"))
		{
			Restore(field_mark);
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		CPPGMAstNodePtr width = ParseAssignmentExpression();
		if (!width)
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		CPPGMAstNodePtr field = Node("bit-field-declarator");
		Add(field, declarator);
		Add(field, width);
		fields.push_back(field);
		if (!Take(",")) break;
	}
	if (!Take(";"))
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	vector<CPPGMAstNodePtr> result;
	for (size_t i = 0; i < fields.size(); ++i)
	{
		CPPGMAstNodePtr declaration = Node("bit-field-declaration");
		Add(declaration, specs);
		Add(declaration, fields[i]);
		result.push_back(declaration);
	}
	// A member parser can return one node.  Preserve all fields by hanging the
	// additional declarations under a small synthetic list only when needed;
	// ParseClassSpecifier unwraps this list immediately.
	if (result.size() == 1) return result[0];
	CPPGMAstNodePtr list = Node("__bit-field-list");
	for (size_t i = 0; i < result.size(); ++i) Add(list, result[i]);
	return list;
}

} // namespace cppgm_pa10
