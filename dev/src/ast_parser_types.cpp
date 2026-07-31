#include "ast_parser.h"

using namespace std;

namespace {

string FirstDeclaratorIdentifier(const CPPGMAstNodePtr& node)
{
	if (!node) return string();
	if (node->kind == "identifier") return node->value;
	for (size_t i = 0; i < node->children.size(); ++i)
	{
		const string result = FirstDeclaratorIdentifier(node->children[i]);
		if (!result.empty()) return result;
	}
	return string();
}

size_t LastTopLevelScopeSeparator(const string& spelling)
{
	int angle_depth = 0;
	size_t result = string::npos;
	for (size_t position = 0; position < spelling.size(); ++position) {
		if (spelling[position] == '<') ++angle_depth;
		else if (spelling[position] == '>' && angle_depth > 0) --angle_depth;
		else if (angle_depth == 0 && spelling.compare(position, 2, "::") == 0) {
			result = position;
			++position;
		}
	}
	return result;
}

} // namespace

namespace cppgm_pa10 {

CPPGMAstNodePtr Parser::ParseDeclSpecifierSeq(bool type_id_context)
{
	Mark mark = Save();
	CPPGMAstNodePtr result = Node("decl-specifier-seq");
	bool any = false;
	bool saw_type = false;
	while (true)
	{
		if (saw_type && (IsNamedTypeStart() || Is("::"))) break;
		Mark item = Save();
		CPPGMAstNodePtr specifier = ParseDeclSpecifier(type_id_context);
		if (!specifier)
		{
			Restore(item);
			break;
		}
		any = true;
		Add(result, specifier);
		const string text = item.position < tokens_.size() ? tokens_[item.position].text : string();
		if (IsFundamental(text) ||
			(!IsStorageOrFunctionSpecifier(text) && !IsCv(text))) saw_type = true;
	}
	if (!any)
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	return result;
}

CPPGMAstNodePtr Parser::ParseDeclSpecifier(bool type_id_context)
{
	(void)type_id_context;
	const string text = Peek().text;
	if (IsStorageOrFunctionSpecifier(text) || IsCv(text))
	{
		++position_;
		return Node("decl-specifier", TokenLabel(text) + ":" + text);
	}
	if (text == "class" || text == "struct" || text == "union")
		return ParseClassSpecifier(false);
	if (text == "enum") return ParseEnumSpecifier(false);
	if (text == "decltype")
	{
		CPPGMAstNodePtr specifier = ParseDecltypeSpecifier();
		if (!specifier) return CPPGMAstNodePtr();
		specifier->kind = "decl-specifier";
		return specifier;
	}
	if (text == "typename")
	{
		Mark mark = Save();
		++position_;
		string name;
		if (!ParseName(&name))
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		CPPGMAstNodePtr result = Node("decl-specifier", name);
		result->explicit_typename = true;
		return result;
	}
	if (text == "::")
	{
		string name;
		if (!ParseName(&name, false)) return CPPGMAstNodePtr();
		return Node("decl-specifier", name);
	}
	if (IsFundamental(text))
	{
		++position_;
		return Node("decl-specifier", TokenLabel(text) + ":" + text);
	}
	if (IsNamedTypeStart())
	{
		string name;
		if (!ParseName(&name, false)) return CPPGMAstNodePtr();
		const bool bare = name.find("::") == string::npos &&
			name.find('<') == string::npos;
		const string value = bare ? "TT_IDENTIFIER:" + name : name;
		return Node("decl-specifier", value);
	}
	return CPPGMAstNodePtr();
}

CPPGMAstNodePtr Parser::ParseTypeSpecifierSeq()
{
	Mark mark = Save();
	CPPGMAstNodePtr result = Node("type-specifier-seq");
	CPPGMAstNodePtr first = ParseTypeSpecifier();
	if (!first)
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	Add(result, first);
	while (true)
	{
		Mark item = Save();
		CPPGMAstNodePtr next = ParseTypeSpecifier();
		if (!next)
		{
			Restore(item);
			break;
		}
		Add(result, next);
	}
	return result;
}

CPPGMAstNodePtr Parser::ParseTypeSpecifier()
{
	const string text = Peek().text;
	if (IsCv(text))
	{
		++position_;
		return Node("cv-qualifier", TokenLabel(text) + ":" + text);
	}
	if (text == "class" || text == "struct" || text == "union")
		return ParseClassSpecifier(false);
	if (text == "enum") return ParseEnumSpecifier(false);
	if (text == "decltype") return ParseDecltypeSpecifier();
	if (text == "typename")
	{
		Mark mark = Save();
		++position_;
		string name;
		if (!ParseName(&name, false))
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		CPPGMAstNodePtr result = Node("type-name", name);
		result->explicit_typename = true;
		return result;
	}
	if (text == "::")
	{
		string name;
		if (!ParseName(&name, false)) return CPPGMAstNodePtr();
		return Node("type-name", name);
	}
	if (IsFundamental(text))
	{
		++position_;
		return Node("type-specifier", TokenLabel(text) + ":" + text);
	}
	if (IsNamedTypeStart())
	{
		string name;
		if (!ParseName(&name, false)) return CPPGMAstNodePtr();
		return Node("type-name", name);
	}
	return CPPGMAstNodePtr();
}

CPPGMAstNodePtr Parser::ParseDecltypeSpecifier()
{
	Mark mark = Save();
	const size_t begin = position_;
	if (!Take("decltype") || !Take("("))
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	++ordinary_depth_;
	CPPGMAstNodePtr expression = ParseExpression();
	if (!expression || !Take(")"))
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	--ordinary_depth_;
	string value;
	for (size_t i = begin; i < position_; ++i)
	{
		if (i != begin && (tokens_[i - 1].text == "const" ||
			tokens_[i - 1].text == "volatile" ||
			tokens_[i - 1].text == "typename" ||
			tokens_[i - 1].text == "template")) value += " ";
		value += tokens_[i].text;
	}
	CPPGMAstNodePtr result = Node("decltype-specifier", value);
	Add(result, expression);
	return result;
}

CPPGMAstNodePtr Parser::ParseTypeId()
{
	Mark mark = Save();
	CPPGMAstNodePtr specs = ParseTypeSpecifierSeq();
	if (!specs && Peek().kind == AST_IDENTIFIER)
	{
		string name;
		if (ParseName(&name, false)) specs = Node("type-specifier-seq");
		if (specs) Add(specs, Node("type-name", name));
	}
	if (!specs)
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	CPPGMAstNodePtr result = Node("type-id");
	Add(result, specs);
	Mark abstract = Save();
	CPPGMAstNodePtr declarator = ParseAbstractDeclarator();
	if (declarator) Add(result, declarator);
	else Restore(abstract);
	return result;
}

CPPGMAstNodePtr Parser::ParsePtrOperator()
{
	Mark mark = Save();
	if (Take("*"))
	{
		CPPGMAstNodePtr result = Node("ptr-operator", TokenLabel("*") + ":*");
		SkipAttributes();
		return result;
	}
	Restore(mark);
	if (Take("&"))
		return Node("ptr-operator", TokenLabel("&") + ":&");
	Restore(mark);
	if (Take("&&"))
		return Node("ptr-operator", TokenLabel("&&") + ":&&");
	Restore(mark);
	// A member pointer keeps its qualified class spelling in the leaf.
	if (Peek().kind == AST_IDENTIFIER || Is("::"))
	{
		string owner;
		Mark owner_mark = Save();
		if (ParseName(&owner, false) && Take("*") && owner.find("::") != string::npos)
		{
			return Node("ptr-operator", owner + "*");
		}
		Restore(owner_mark);
	}
	Restore(mark);
	return CPPGMAstNodePtr();
}

CPPGMAstNodePtr Parser::ParseDeclarator(bool allow_abstract)
{
	return ParseDeclaratorCore(allow_abstract);
}

CPPGMAstNodePtr Parser::ParseDeclaratorCore(bool allow_abstract)
{
	Mark mark = Save();
	CPPGMAstNodePtr result = Node("declarator");
	vector<CPPGMAstNodePtr> pointers;
	while (true)
	{
		Mark pointer_mark = Save();
		CPPGMAstNodePtr pointer = ParsePtrOperator();
		if (!pointer)
		{
			Restore(pointer_mark);
			break;
		}
		pointers.push_back(pointer);
		while (IsCv(Peek().text))
		{
			const string cv = Peek().text;
			++position_;
			pointers.push_back(Node("cv-qualifier", TokenLabel(cv) + ":" + cv));
		}
	}
	const bool leading_pack = Take("...");
	string identifier;
	if (ParseName(&identifier))
	{
		for (size_t i = 0; i < pointers.size(); ++i) Add(result, pointers[i]);
		if (leading_pack) Add(result, Node("parameter-pack", "..."));
		Add(result, Node("identifier", identifier));
	}
	else if (Take("("))
	{
		++ordinary_depth_;
		CPPGMAstNodePtr inner = ParseDeclaratorCore(true);
		if (!inner || !Take(")"))
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		--ordinary_depth_;
		for (size_t i = 0; i < pointers.size(); ++i) Add(result, pointers[i]);
		if (leading_pack) Add(result, Node("parameter-pack", "..."));
		CPPGMAstNodePtr nested = Node("nested-declarator");
		Add(nested, inner);
		Add(result, nested);
	}
	else if (!allow_abstract)
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	else
	{
		for (size_t i = 0; i < pointers.size(); ++i) Add(result, pointers[i]);
		if (leading_pack) Add(result, Node("parameter-pack", "..."));
		if (pointers.empty() && !leading_pack)
		{
			CPPGMAstNodePtr suffix = ParseParametersAndQualifiers();
			if (!suffix)
			{
				Restore(mark);
				return CPPGMAstNodePtr();
			}
			Add(result, suffix);
		}
	}
	while (true)
	{
		SkipAttributes();
		Mark suffix_mark = Save();
		if (Take("["))
		{
			++ordinary_depth_;
			CPPGMAstNodePtr bound;
			if (!Is("]")) bound = ParseExpression();
			if (!Take("]"))
			{
				Restore(mark);
				return CPPGMAstNodePtr();
			}
			--ordinary_depth_;
			CPPGMAstNodePtr array = Node("array-suffix");
			Add(array, bound);
			Add(result, array);
			continue;
		}
		CPPGMAstNodePtr parameters = ParseParametersAndQualifiers();
		if (parameters)
		{
			if (parameters->kind == "__parameter-group")
				for (size_t i = 0; i < parameters->children.size(); ++i)
					Add(result, parameters->children[i]);
			else Add(result, parameters);
			continue;
		}
		Restore(suffix_mark);
		break;
	}
	SkipAttributes();
	if (result->children.empty())
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	return result;
}

CPPGMAstNodePtr Parser::ParseParametersAndQualifiers()
{
	Mark mark = Save();
	CPPGMAstNodePtr parameters = ParseParameterClause();
	if (!parameters)
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	CPPGMAstNodePtr group = Node("__parameter-group");
	Add(group, parameters);
	ParseFunctionSuffixes(group);
	return group;
}

bool Parser::ParseFunctionSuffixes(const CPPGMAstNodePtr& declarator)
{
	bool parsed = false;
	// Keep consuming suffixes until the next token is no longer part of a
	// function declarator.  The C++ grammar permits the virtual specifiers to
	// follow the exception specification (for example `noexcept override`),
	// while the original PA10 parser only accepted one fixed ordering.
	while (true)
	{
		if (IsCv(Peek().text))
		{
			const string text = Peek().text;
			++position_;
			Add(declarator, Node("cv-qualifier", TokenLabel(text) + ":" + text));
			parsed = true;
			continue;
		}
		if (Is("&") || Is("&&"))
		{
			const string text = Peek().text;
			++position_;
			Add(declarator, Node("ref-qualifier", TokenLabel(text) + ":" + text));
			parsed = true;
			continue;
		}
		if (Is("override") || Is("final"))
		{
			const string text = Peek().text;
			++position_;
			Add(declarator, Node("virt-specifier", TokenLabel(text) + ":" + text));
			parsed = true;
			continue;
		}
		if (Take("noexcept"))
		{
			string value = "noexcept";
			if (Take("("))
			{
				CPPGMAstNodePtr expression = ParseExpression();
				if (!expression || !Take(")")) return false;
			}
			Add(declarator, Node("function-qualifier", value));
			parsed = true;
			continue;
		}
		if (Take("throw"))
		{
			if (!Take("(")) return false;
			string value = "throw(";
			if (!Is(")"))
			{
				while (true)
				{
					const size_t begin = position_;
					CPPGMAstNodePtr type = ParseTypeId();
					if (!type) return false;
					const size_t end = position_;
					for (size_t i = begin; i < end; ++i) value += tokens_[i].text;
					if (!Take(",")) break;
					value += ",";
				}
			}
			if (!Take(")")) return false;
			value += ")";
			Add(declarator, Node("function-qualifier", value));
			parsed = true;
			continue;
		}
		if (Take("->"))
		{
			const size_t begin = position_;
			CPPGMAstNodePtr type = ParseTypeId();
			if (!type) return false;
			string value;
			for (size_t i = begin; i < position_; ++i) value += tokens_[i].text;
			CPPGMAstNodePtr trailing = Node("trailing-return-type", value);
			Add(trailing, type);
			Add(declarator, trailing);
			parsed = true;
			continue;
		}
		break;
	}
	SkipAttributes();
	return parsed;
}

CPPGMAstNodePtr Parser::ParseParameterClause()
{
	Mark mark = Save();
	if (!Take("(")) return CPPGMAstNodePtr();
	++ordinary_depth_;
	CPPGMAstNodePtr result = Node("parameter-clause");
	if (Take("..."))
	{
		Add(result, Node("ellipsis", "..."));
	}
	else if (!Is(")"))
	{
		CPPGMAstNodePtr parameter = ParseParameterDeclaration();
		if (!parameter)
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		// In a block, `T object(expr)` is a direct initializer, but the
		// declarator grammar initially has to try the indistinguishable
		// function-declarator form.  A qualified value such as
		// `enum_type::enumerator` is not a type-id; let the declarator backtrack
		// so ParseInitializer can parse the parenthesized expression instead.
		if (ordinary_depth_ > 1 && parameter->children.size() == 1 &&
			parameter->children[0] && parameter->children[0]->kind == "decl-specifier-seq" &&
			parameter->children[0]->children.size() == 1 &&
			parameter->children[0]->children[0]) {
			string spelling = parameter->children[0]->children[0]->value;
			const size_t marker = spelling.find(':');
			if (marker != string::npos && marker + 1 < spelling.size() &&
				spelling[marker + 1] != ':') spelling = spelling.substr(marker + 1);
			const size_t separator = LastTopLevelScopeSeparator(spelling);
			const string last = separator == string::npos ? spelling :
				spelling.substr(separator + 2);
			const size_t last_angle = last.find('<');
			const string last_name = last_angle == string::npos ? last :
				last.substr(0, last_angle);
			if (separator != string::npos &&
				!parameter->children[0]->children[0]->explicit_typename &&
				spelling.find("typename ") != 0 &&
				last_name.find("template ") != 0 &&
				types_.find(last_name) == types_.end() &&
				templates_.find(last_name) == templates_.end()) {
				Restore(mark);
				return CPPGMAstNodePtr();
			}
		}
		Add(result, parameter);
		while (Take(","))
		{
			if (Take("..."))
			{
				Add(result, Node("parameter-pack", "..."));
				break;
			}
			parameter = ParseParameterDeclaration();
			if (!parameter)
			{
				Restore(mark);
				return CPPGMAstNodePtr();
			}
			if (ordinary_depth_ > 1 && parameter->children.size() == 1 &&
				parameter->children[0] && parameter->children[0]->kind == "decl-specifier-seq" &&
				parameter->children[0]->children.size() == 1 &&
				parameter->children[0]->children[0]) {
				string spelling = parameter->children[0]->children[0]->value;
				const size_t marker = spelling.find(':');
				if (marker != string::npos && marker + 1 < spelling.size() &&
					spelling[marker + 1] != ':') spelling = spelling.substr(marker + 1);
				const size_t separator = LastTopLevelScopeSeparator(spelling);
				const string last = separator == string::npos ? spelling :
					spelling.substr(separator + 2);
				const size_t last_angle = last.find('<');
				const string last_name = last_angle == string::npos ? last :
					last.substr(0, last_angle);
				if (separator != string::npos &&
					!parameter->children[0]->children[0]->explicit_typename &&
					spelling.find("typename ") != 0 &&
					last_name.find("template ") != 0 &&
					types_.find(last_name) == types_.end() &&
					templates_.find(last_name) == templates_.end()) {
					Restore(mark);
					return CPPGMAstNodePtr();
				}
			}
			Add(result, parameter);
		}
	}
	if (!Take(")"))
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	--ordinary_depth_;
	return result;
}

CPPGMAstNodePtr Parser::ParseParameterDeclaration()
{
	Mark mark = Save();
	SkipAttributes();
	if (Take("...")) return Node("parameter-pack", "...");
	CPPGMAstNodePtr specs = ParseDeclSpecifierSeq(false);
	if (!specs)
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	CPPGMAstNodePtr declarator;
	Mark declarator_mark = Save();
	if (!Is("=") && !Is(",") && !Is(")"))
	{
		declarator = ParseDeclarator(true);
		if (!declarator) Restore(declarator_mark);
	}
	CPPGMAstNodePtr result = Node("parameter-declaration");
	Add(result, specs);
	Add(result, declarator);
	if (Take("="))
	{
		CPPGMAstNodePtr clause = ParseInitializerClause();
		if (!clause)
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		CPPGMAstNodePtr initializer = Node("initializer");
		Add(initializer, clause);
	CPPGMAstNodePtr default_argument = Node("default-argument");
		Add(default_argument, initializer);
		Add(result, default_argument);
	}
	if (declarator)
	{
		const string name = FirstDeclaratorIdentifier(declarator);
		if (!name.empty()) value_names_.insert(name);
	}
	return result;
}

CPPGMAstNodePtr Parser::ParseAbstractDeclarator()
{
	Mark mark = Save();
	CPPGMAstNodePtr result = Node("abstract-declarator");
	bool any = false;
	while (true)
	{
		Mark pointer_mark = Save();
		CPPGMAstNodePtr pointer = ParsePtrOperator();
		if (!pointer)
		{
			Restore(pointer_mark);
			break;
		}
		Add(result, pointer);
		while (IsCv(Peek().text))
		{
			const string cv = Peek().text;
			++position_;
			Add(result, Node("cv-qualifier", TokenLabel(cv) + ":" + cv));
		}
		any = true;
	}
	Mark nested_mark = Save();
	if (Take("("))
	{
		++ordinary_depth_;
		CPPGMAstNodePtr inner = ParseAbstractDeclarator();
		if (!inner || !Take(")"))
		{
			Restore(nested_mark);
		}
		else
		{
			--ordinary_depth_;
			CPPGMAstNodePtr nested = Node("nested-declarator");
			Add(nested, inner);
			Add(result, nested);
			any = true;
		}
	}
	while (true)
	{
		Mark suffix = Save();
		if (Is("(") )
		{
			CPPGMAstNodePtr parameters = ParseParameterClause();
			if (parameters)
			{
				Add(result, parameters);
				any = true;
				// Function type-ids use an abstract declarator, so their cv/ref
				// qualifiers follow the parameter clause here rather than the
				// named-declarator path handled by ParseDeclaratorCore.
				ParseFunctionSuffixes(result);
				continue;
			}
		}
		Restore(suffix);
		if (Take("["))
		{
			++ordinary_depth_;
			CPPGMAstNodePtr bound;
			if (!Is("]")) bound = ParseExpression();
			if (!Take("]"))
			{
				Restore(mark);
				return CPPGMAstNodePtr();
			}
			--ordinary_depth_;
			CPPGMAstNodePtr array = Node("array-suffix");
			Add(array, bound);
			Add(result, array);
			any = true;
			continue;
		}
		Restore(suffix);
		break;
	}
	if (!any)
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	return result;
}

CPPGMAstNodePtr Parser::ParseInitializer()
{
	Mark mark = Save();
	if (Take("="))
	{
		if (Is("default") || Is("delete"))
		{
			const string value = Peek().text;
			++position_;
			CPPGMAstNodePtr result = Node("initializer");
			Add(result, Node("special-initializer", value));
			return result;
		}
		CPPGMAstNodePtr clause = ParseInitializerClause();
		if (!clause)
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		CPPGMAstNodePtr result = Node("initializer");
		result->initializer_form = AST_INITIALIZER_COPY;
		Add(result, clause);
		return result;
	}
	if (Is("{"))
	{
		CPPGMAstNodePtr list = ParseBracedInitList();
		if (!list) return CPPGMAstNodePtr();
		CPPGMAstNodePtr result = Node("initializer");
		result->initializer_form = AST_INITIALIZER_DIRECT_LIST;
		Add(result, list);
		return result;
	}
	if (Take("("))
	{
		++ordinary_depth_;
		CPPGMAstNodePtr result = Node("initializer");
		result->initializer_form = AST_INITIALIZER_DIRECT_PAREN;
		CPPGMAstNodePtr paren = Node("paren-initializer");
		if (!Is(")"))
		{
			CPPGMAstNodePtr clause = ParseInitializerClause();
			if (!clause)
			{
				Restore(mark);
				return CPPGMAstNodePtr();
			}
			Add(paren, clause);
			while (Take(","))
			{
				clause = ParseInitializerClause();
				if (!clause)
				{
					Restore(mark);
					return CPPGMAstNodePtr();
				}
				Add(paren, clause);
			}
		}
		if (!Take(")"))
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		--ordinary_depth_;
		Add(result, paren);
		return result;
	}
	Restore(mark);
	return CPPGMAstNodePtr();
}

CPPGMAstNodePtr Parser::ParseInitializerClause()
{
	if (Is("{")) return ParseBracedInitList();
	return ParseAssignmentExpression();
}

CPPGMAstNodePtr Parser::ParseBracedInitList()
{
	Mark mark = Save();
	if (!Take("{")) return CPPGMAstNodePtr();
	++ordinary_depth_;
	CPPGMAstNodePtr result = Node("braced-init-list");
	if (!Is("}"))
	{
		CPPGMAstNodePtr clause = ParseInitializerClause();
		if (!clause)
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		Add(result, clause);
		while (Take(","))
		{
			if (Is("}")) break;
			clause = ParseInitializerClause();
			if (!clause)
			{
				Restore(mark);
				return CPPGMAstNodePtr();
			}
			Add(result, clause);
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

} // namespace cppgm_pa10
