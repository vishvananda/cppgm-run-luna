#include "recog_parser_internal.h"

namespace recog_pa6 {

bool Parser::ParseDeclSpecifierSeq()
{
	Mark mark = Save();
	bool any = false;
	bool previous_type_specifier = false;
	while (true)
	{
		if (previous_type_specifier && IsIdentifierToken(Peek()) &&
			(Peek().names.class_name || Peek().names.template_name ||
			 Peek().names.typedef_name || Peek().names.enum_name))
			break;
		Mark item = Save();
		if (!ParseDeclSpecifier())
		{
			Restore(item);
			break;
		}
		any = true;
		const string first = tokens_[item.position].text;
		const bool cv_only = first == "const" || first == "volatile";
		const bool non_type = first == "register" || first == "static" ||
			first == "thread_local" || first == "extern" || first == "mutable" ||
			first == "inline" || first == "virtual" || first == "explicit" ||
			first == "friend" || first == "typedef" || first == "constexpr";
		if (!cv_only && !non_type) previous_type_specifier = true;
	}
	if (!any)
	{
		Restore(mark);
		return false;
	}
	ParseRepeatedAttributes();
	return true;
}

bool Parser::ParseDeclSpecifier()
{
	Mark mark = Save();
	static const set<string> storage = {
		"register", "static", "thread_local", "extern", "mutable"
	};
	if (storage.find(Peek().text) != storage.end())
	{
		++position_;
		return true;
	}
	static const set<string> functions = {"inline", "virtual", "explicit"};
	if (functions.find(Peek().text) != functions.end())
	{
		++position_;
		return true;
	}
	if (Take("friend") || Take("typedef") || Take("constexpr")) return true;
	Restore(mark);
	return ParseTypeSpecifier();
}

bool Parser::ParseTypeSpecifierSeq()
{
	Mark mark = Save();
	if (!ParseTypeSpecifier())
	{
		Restore(mark);
		return false;
	}
	while (true)
	{
		Mark item = Save();
		if (!ParseTypeSpecifier())
		{
			Restore(item);
			break;
		}
	}
	ParseRepeatedAttributes();
	return true;
}

bool Parser::ParseTrailingTypeSpecifierSeq()
{
	Mark mark = Save();
	if (!ParseTrailingTypeSpecifier())
	{
		Restore(mark);
		return false;
	}
	while (ParseTrailingTypeSpecifier()) {}
	ParseRepeatedAttributes();
	return true;
}

bool Parser::ParseTypeSpecifier()
{
	Mark mark = Save();
	if (ParseClassSpecifier()) return true;
	Restore(mark);
	if (ParseEnumSpecifier()) return true;
	Restore(mark);
	if (ParseTrailingTypeSpecifier()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseTrailingTypeSpecifier()
{
	Mark mark = Save();
	if (ParseSimpleTypeSpecifier()) return true;
	Restore(mark);
	if (ParseElaboratedTypeSpecifier()) return true;
	Restore(mark);
	if (ParseTypenameSpecifier()) return true;
	Restore(mark);
	return Take("const") || Take("volatile");
}

bool Parser::ParseSimpleTypeSpecifier()
{
	static const set<string> fundamental = {
		"char", "char16_t", "char32_t", "wchar_t", "bool", "short", "int",
		"long", "signed", "unsigned", "float", "double", "void", "auto"
	};
	if (fundamental.find(Peek().text) != fundamental.end())
	{
		++position_;
		return true;
	}
	Mark mark = Save();
	if (ParseNestedNameSpecifier())
	{
		if (Take("template") && ParseSimpleTemplateId()) return true;
		if (ParseTypeName()) return true;
	}
	Restore(mark);
	if (ParseDecltypeSpecifier()) return true;
	Restore(mark);
	return ParseTypeName();
}

bool Parser::ParseElaboratedTypeSpecifier()
{
	Mark mark = Save();
	if (Peek().text == "class" || Peek().text == "struct" ||
		Peek().text == "union")
	{
		++position_;
		ParseRepeatedAttributes();
		Mark qualified = Save();
		ParseNestedNameSpecifier();
		if (TakeIdentifier()) return true;
		Restore(qualified);
		if (Take("template") && ParseSimpleTemplateId()) return true;
		if (ParseSimpleTemplateId()) return true;
	}
	Restore(mark);
	if (Take("enum"))
	{
		ParseNestedNameSpecifier();
		if (TakeIdentifier()) return true;
	}
	Restore(mark);
	return false;
}

bool Parser::ParseEnumKey()
{
	if (!Take("enum")) return false;
	Take("class") || Take("struct");
	return true;
}

bool Parser::ParseEnumHead()
{
	if (!ParseEnumKey()) return false;
	ParseRepeatedAttributes();
	Mark name = Save();
	if (TakeIdentifier())
	{
		if (!Is("::"))
		{
			ParseEnumBase();
			return true;
		}
	}
	Restore(name);
	if (ParseNestedNameSpecifier() && TakeIdentifier())
	{
		ParseEnumBase();
		return true;
	}
	Restore(name);
	// The grammar permits an unnamed enum, with or without an underlying type.
	ParseEnumBase();
	return true;
}

bool Parser::ParseEnumSpecifier()
{
	Mark mark = Save();
	if (!ParseEnumHead() || !Take("{"))
	{
		Restore(mark);
		return false;
	}
	++ordinary_depth_;
	if (!Is("}") && !ParseEnumeratorList())
	{
		Restore(mark);
		return false;
	}
	Take(",");
	if (!Take("}"))
	{
		Restore(mark);
		return false;
	}
	--ordinary_depth_;
	return true;
}

bool Parser::ParseEnumBase()
{
	Mark mark = Save();
	if (!Take(":")) return false;
	if (!ParseTypeSpecifierSeq())
	{
		Restore(mark);
		return false;
	}
	return true;
}

bool Parser::ParseEnumeratorList()
{
	Mark mark = Save();
	if (!TakeIdentifier())
	{
		Restore(mark);
		return false;
	}
	if (Take("="))
	{
		if (!ParseConditionalExpression())
		{
			Restore(mark);
			return false;
		}
	}
	while (Take(","))
	{
		if (Is("}")) break;
		if (!TakeIdentifier())
		{
			Restore(mark);
			return false;
		}
		if (Take("=") && !ParseConditionalExpression())
		{
			Restore(mark);
			return false;
		}
	}
	return true;
}

bool Parser::ParseClassSpecifier()
{
	Mark mark = Save();
	if (!ParseClassHead() || !Take("{"))
	{
		Restore(mark);
		return false;
	}
	++ordinary_depth_;
	while (!Is("}") && !AtEnd())
	{
		Mark member = Save();
		if (!ParseMemberSpecification())
		{
			Restore(mark);
			return false;
		}
		if (position_ == member.position)
		{
			Restore(mark);
			return false;
		}
	}
	if (!Take("}"))
	{
		Restore(mark);
		return false;
	}
	--ordinary_depth_;
	return true;
}

bool Parser::ParseClassHead()
{
	if (!(Take("class") || Take("struct") || Take("union"))) return false;
	ParseRepeatedAttributes();
	Mark name = Save();
	ParseNestedNameSpecifier();
	if (ParseClassName())
	{
		Take("final");
		ParseBaseClause();
		return true;
	}
	Restore(name);
	if (ParseBaseClause()) return true;
	return true;
}

bool Parser::ParseBaseClause()
{
	Mark mark = Save();
	if (!Take(":")) return false;
	if (!ParseBaseSpecifierList())
	{
		Restore(mark);
		return false;
	}
	return true;
}

bool Parser::ParseBaseSpecifierList()
{
	Mark mark = Save();
	if (!ParseBaseSpecifier())
	{
		Restore(mark);
		return false;
	}
	Mark dots = Save();
	if (Take("...")) {}
	else Restore(dots);
	while (Take(","))
	{
		if (!ParseBaseSpecifier())
		{
			Restore(mark);
			return false;
		}
		Mark tail = Save();
		if (Take("...")) {}
		else Restore(tail);
	}
	return true;
}

bool Parser::ParseBaseSpecifier()
{
	Mark mark = Save();
	ParseRepeatedAttributes();
	if (Take("virtual")) ParseAccessSpecifier();
	else if (ParseAccessSpecifier()) Take("virtual");
	if (ParseClassOrDecltype()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseClassOrDecltype()
{
	Mark mark = Save();
	Mark qualified = Save();
	ParseNestedNameSpecifier();
	if (ParseClassName()) return true;
	Restore(qualified);
	if (ParseDecltypeSpecifier()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseAccessSpecifier()
{
	if (Is("private") || Is("protected") || Is("public"))
	{
		++position_;
		return true;
	}
	return false;
}

bool Parser::ParseDeclarator()
{
	Mark mark = Save();
	if (ParsePtrDeclarator())
	{
		if (!Is("->")) return true;
		Restore(mark);
	}
	Restore(mark);
	if (ParseNoptrDeclarator() && ParseTrailingReturnType()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParsePtrDeclarator()
{
	while (ParsePtrOperator()) {}
	return ParseNoptrDeclarator();
}

bool Parser::ParseNoptrDeclarator()
{
	Mark mark = Save();
	if (!ParseNoptrDeclaratorRoot())
	{
		Restore(mark);
		return false;
	}
	while (true)
	{
		Mark suffix = Save();
		if (!ParseNoptrDeclaratorSuffix())
		{
			Restore(suffix);
			break;
		}
	}
	return true;
}

bool Parser::ParseNoptrDeclaratorRoot()
{
	Mark mark = Save();
	if (ParseDeclaratorId())
	{
		ParseRepeatedAttributes();
		return true;
	}
	Restore(mark);
	if (Take("("))
	{
		++ordinary_depth_;
		if (ParsePtrDeclarator() && Take(")"))
		{
			--ordinary_depth_;
			return true;
		}
	}
	Restore(mark);
	return false;
}

bool Parser::ParseNoptrDeclaratorSuffix()
{
	Mark mark = Save();
	if (ParseParametersAndQualifiers()) return true;
	Restore(mark);
	if (Take("["))
	{
		++ordinary_depth_;
		if (!Is("]") && !ParseConditionalExpression())
		{
			Restore(mark);
			return false;
		}
		if (!Take("]"))
		{
			Restore(mark);
			return false;
		}
		--ordinary_depth_;
		ParseRepeatedAttributes();
		return true;
	}
	Restore(mark);
	return false;
}

bool Parser::ParseParametersAndQualifiers()
{
	Mark mark = Save();
	if (!Take("(")) return false;
	++ordinary_depth_;
	if (!ParseParameterDeclarationClause() || !Take(")"))
	{
		Restore(mark);
		return false;
	}
	--ordinary_depth_;
	while (Is("const") || Is("volatile")) ++position_;
	ParseRefQualifier();
	ParseExceptionSpecification();
	ParseRepeatedAttributes();
	return true;
}

bool Parser::ParsePtrOperator()
{
	Mark mark = Save();
	if (Take("*"))
	{
		ParseRepeatedAttributes();
		while (Is("const") || Is("volatile")) ++position_;
		return true;
	}
	Restore(mark);
	if (Take("&"))
	{
		ParseRepeatedAttributes();
		return true;
	}
	Restore(mark);
	if (Take("&&"))
	{
		ParseRepeatedAttributes();
		return true;
	}
	Restore(mark);
	if (ParseNestedNameSpecifier() && Take("*"))
	{
		ParseRepeatedAttributes();
		while (Is("const") || Is("volatile")) ++position_;
		return true;
	}
	Restore(mark);
	return false;
}

bool Parser::ParseDeclaratorId()
{
	Mark mark = Save();
	Take("...");
	if (ParseIdExpression()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseTypeId()
{
	Mark mark = Save();
	if (!ParseTypeSpecifierSeq())
	{
		Restore(mark);
		return false;
	}
	Mark abstract = Save();
	if (!ParseAbstractDeclarator()) Restore(abstract);
	return true;
}

bool Parser::ParseAbstractDeclarator()
{
	Mark mark = Save();
	if (ParsePtrAbstractDeclarator()) return true;
	Restore(mark);
	if (ParseNoptrAbstractDeclarator()) return true;
	Restore(mark);
	return ParseAbstractPackDeclarator();
}

bool Parser::ParsePtrAbstractDeclarator()
{
	Mark mark = Save();
	if (!ParsePtrOperator())
	{
		Restore(mark);
		return false;
	}
	while (ParsePtrOperator()) {}
	Mark nested = Save();
	if (!ParseNoptrAbstractDeclarator()) Restore(nested);
	return true;
}

bool Parser::ParseNoptrAbstractDeclarator()
{
	Mark mark = Save();
	if (ParseNoptrAbstractDeclaratorRoot())
	{
		while (ParseNoptrDeclaratorSuffix()) {}
		return true;
	}
	Restore(mark);
	if (!ParseNoptrDeclaratorSuffix())
	{
		Restore(mark);
		return false;
	}
	while (ParseNoptrDeclaratorSuffix()) {}
	return true;
}

bool Parser::ParseNoptrAbstractDeclaratorRoot()
{
	Mark mark = Save();
	if (ParseNoptrDeclaratorSuffix()) return true;
	Restore(mark);
	if (Take("("))
	{
		++ordinary_depth_;
		if (ParsePtrAbstractDeclarator() && Take(")"))
		{
			--ordinary_depth_;
			return true;
		}
	}
	Restore(mark);
	return false;
}

bool Parser::ParseAbstractPackDeclarator()
{
	Mark mark = Save();
	while (ParsePtrOperator()) {}
	if (!Take("..."))
	{
		Restore(mark);
		return false;
	}
	while (ParseNoptrDeclaratorSuffix()) {}
	return true;
}

bool Parser::ParseParameterDeclarationClause()
{
	Mark mark = Save();
	if (Take("...")) return true;
	if (Is(")")) return true;
	if (!ParseParameterDeclarationList())
	{
		Restore(mark);
		return false;
	}
	if (Take(","))
	{
		if (!Take("..."))
		{
			Restore(mark);
			return false;
		}
	}
	else
	{
		Mark dots = Save();
		if (Take("...")) {}
		else Restore(dots);
	}
	return true;
}

bool Parser::ParseParameterDeclarationList()
{
	Mark mark = Save();
	if (!ParseParameterDeclaration())
	{
		Restore(mark);
		return false;
	}
	while (Take(","))
	{
		Mark next = Save();
		if (!ParseParameterDeclaration())
		{
			Restore(next);
			// The caller handles a comma followed by ellipsis.
			break;
		}
	}
	return true;
}

bool Parser::ParseParameterDeclaration()
{
	Mark mark = Save();
	ParseRepeatedAttributes();
	if (!ParseDeclSpecifierSeq())
	{
		Restore(mark);
		return false;
	}
	Mark declarator = Save();
	if (!ParseDeclarator()) Restore(declarator);
	if (Take("=") && !ParseInitializerClause())
	{
		Restore(mark);
		return false;
	}
	return true;
}

bool Parser::ParseTrailingReturnType()
{
	Mark mark = Save();
	if (!Take("->")) return false;
	if (!ParseTrailingTypeSpecifierSeq())
	{
		Restore(mark);
		return false;
	}
	Mark abstract = Save();
	if (!ParseAbstractDeclarator()) Restore(abstract);
	return true;
}

bool Parser::ParseVirtSpecifier()
{
	if (Is("override") || Is("final"))
	{
		++position_;
		return true;
	}
	return false;
}

bool Parser::ParseRefQualifier()
{
	return Take("&") || Take("&&");
}

bool Parser::ParseInitializer()
{
	Mark mark = Save();
	if (ParseBraceOrEqualInitializer()) return true;
	Restore(mark);
	if (Take("("))
	{
		++ordinary_depth_;
		if (ParseExpressionList() && Take(")"))
		{
			--ordinary_depth_;
			return true;
		}
	}
	Restore(mark);
	return false;
}

bool Parser::ParseBraceOrEqualInitializer()
{
	Mark mark = Save();
	if (Take("=") && ParseInitializerClause()) return true;
	Restore(mark);
	return ParseBracedInitList();
}

bool Parser::ParseInitializerClause()
{
	Mark mark = Save();
	if (ParseAssignmentExpression()) return true;
	Restore(mark);
	return ParseBracedInitList();
}

bool Parser::ParseInitializerList()
{
	Mark mark = Save();
	if (!ParseInitializerClauseDots())
	{
		Restore(mark);
		return false;
	}
	while (Take(","))
	{
		if (!ParseInitializerClauseDots())
		{
			Restore(mark);
			return false;
		}
	}
	return true;
}

bool Parser::ParseInitializerClauseDots()
{
	Mark mark = Save();
	if (!ParseInitializerClause())
	{
		Restore(mark);
		return false;
	}
	Mark dots = Save();
	if (Take("...")) return true;
	Restore(dots);
	return true;
}

bool Parser::ParseBracedInitList()
{
	Mark mark = Save();
	if (!Take("{")) return false;
	++ordinary_depth_;
	if (!Is("}") && !ParseInitializerList())
	{
		Restore(mark);
		return false;
	}
	Take(",");
	if (!Take("}"))
	{
		Restore(mark);
		return false;
	}
	--ordinary_depth_;
	return true;
}

bool Parser::ParseFunctionBody()
{
	Mark mark = Save();
	if (ParseFunctionTryBlock()) return true;
	Restore(mark);
	if (Take("=") && (Take("default") || Take("delete")) && Take(";")) return true;
	Restore(mark);
	ParseCtorInitializer();
	if (ParseCompoundStatement()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseCtorInitializer()
{
	Mark mark = Save();
	if (!Take(":")) return false;
	if (!ParseMemInitializerList())
	{
		Restore(mark);
		return false;
	}
	return true;
}

bool Parser::ParseMemInitializerList()
{
	Mark mark = Save();
	if (!ParseMemInitializer())
	{
		Restore(mark);
		return false;
	}
	Mark dots = Save();
	if (Take("...")) {}
	else Restore(dots);
	while (Take(","))
	{
		if (!ParseMemInitializer())
		{
			Restore(mark);
			return false;
		}
		Mark tail = Save();
		if (Take("...")) {}
		else Restore(tail);
	}
	return true;
}

bool Parser::ParseMemInitializer()
{
	Mark mark = Save();
	if (!ParseMemInitializerId())
	{
		Restore(mark);
		return false;
	}
	if (Take("("))
	{
		++ordinary_depth_;
		if ((Is(")") || ParseExpressionList()) && Take(")"))
		{
			--ordinary_depth_;
			return true;
		}
		Restore(mark);
		return false;
	}
	if (ParseBracedInitList()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseMemInitializerId()
{
	Mark mark = Save();
	if (ParseClassOrDecltype()) return true;
	Restore(mark);
	return TakeIdentifier();
}

bool Parser::ParseMemberSpecification()
{
	Mark mark = Save();
	if (ParseAccessSpecifier() && Take(":")) return true;
	Restore(mark);
	return ParseMemberDeclaration();
}

bool Parser::ParseMemberDeclaration()
{
	Mark mark = Save();
	if (ParseFunctionDefinition())
	{
		Take(";");
		return true;
	}
	Restore(mark);
	if (ParseUsingDeclaration() || ParseAliasDeclaration() ||
		ParseStaticAssertDeclaration() || ParseTemplateDeclaration())
		return true;
	Restore(mark);

	ParseRepeatedAttributes();
	if (!ParseDeclSpecifierSeq())
	{
		Restore(mark);
		return false;
	}
	Mark list = Save();
	if (!ParseMemberDeclaratorList()) Restore(list);
	if (!Take(";"))
	{
		Restore(mark);
		return false;
	}
	return true;
}

bool Parser::ParseMemberDeclaratorList()
{
	Mark mark = Save();
	if (!ParseMemberDeclarator())
	{
		Restore(mark);
		return false;
	}
	while (Take(","))
	{
		if (!ParseMemberDeclarator())
		{
			Restore(mark);
			return false;
		}
	}
	return true;
}

bool Parser::ParseMemberDeclarator()
{
	Mark mark = Save();
	if (!ParseDeclarator())
	{
		Restore(mark);
		ParseRepeatedAttributes();
		if (Take(":") && ParseConditionalExpression()) return true;
		Restore(mark);
		return false;
	}
	while (ParseVirtSpecifier()) {}
	Mark pure = Save();
	if (ParsePureSpecifier()) return true;
	Restore(pure);
	Mark bitfield = Save();
	if (Take(":"))
	{
		if (ParseConditionalExpression()) return true;
		Restore(mark);
		return false;
	}
	Restore(bitfield);
	Mark initializer = Save();
	if (ParseBraceOrEqualInitializer()) return true;
	Restore(initializer);
	return true;
}

bool Parser::ParsePureSpecifier()
{
	Mark mark = Save();
	if (Take("=") && IsZeroLiteral(Peek()))
	{
		++position_;
		return true;
	}
	Restore(mark);
	return false;
}

bool Parser::ParseNewDeclarator()
{
	Mark mark = Save();
	while (ParsePtrOperator()) {}
	Mark array = Save();
	if (!Take("["))
	{
		Restore(array);
		return position_ != mark.position;
	}
	++ordinary_depth_;
	if (!ParseExpression() || !Take("]"))
	{
		Restore(mark);
		return false;
	}
	--ordinary_depth_;
	ParseRepeatedAttributes();
	while (Take("["))
	{
		++ordinary_depth_;
		if (!ParseConditionalExpression() || !Take("]"))
		{
			Restore(mark);
			return false;
		}
		--ordinary_depth_;
		ParseRepeatedAttributes();
	}
	return true;
}

bool Parser::ParseExceptionSpecification()
{
	Mark mark = Save();
	if (Take("throw"))
	{
		if (Take("("))
		{
			++ordinary_depth_;
			if (!Is(")"))
			{
				if (!ParseTypeId())
				{
					Restore(mark);
					return false;
				}
				Take("...");
				while (Take(","))
				{
					if (!ParseTypeId())
					{
						Restore(mark);
						return false;
					}
					Take("...");
				}
			}
			if (!Take(")"))
			{
				Restore(mark);
				return false;
			}
			--ordinary_depth_;
			return true;
		}
		Restore(mark);
		return false;
	}
	Restore(mark);
	if (Take("noexcept"))
	{
		if (Take("("))
		{
			++ordinary_depth_;
			if (!ParseExpression() || !Take(")"))
			{
				Restore(mark);
				return false;
			}
			--ordinary_depth_;
		}
		return true;
	}
	Restore(mark);
	return false;
}

bool Parser::ParseDeclaration()
{
	Mark mark = Save();
	if (ParseExplicitSpecialization()) return true;
	Restore(mark);
	if (ParseExplicitInstantiation()) return true;
	Restore(mark);
	if (ParseTemplateDeclaration()) return true;
	Restore(mark);
	if (ParseLinkageSpecification() || ParseNamespaceDefinition()) return true;
	Restore(mark);
	if (ParseFunctionDefinition()) return true;
	Restore(mark);
	return ParseBlockDeclaration();
}

bool Parser::ParseBlockDeclaration()
{
	Mark mark = Save();
	if (ParseEmptyDeclaration() || ParseAttributeDeclaration() ||
		ParseNamespaceAliasDefinition() || ParseUsingDeclaration() ||
		ParseUsingDirective() || ParseStaticAssertDeclaration() ||
		ParseAliasDeclaration() || ParseAsmDefinition() ||
		ParseOpaqueEnumDeclaration())
		return true;
	Restore(mark);
	return ParseSimpleDeclaration();
}

bool Parser::ParseSimpleDeclaration()
{
	Mark mark = Save();
	ParseRepeatedAttributes();
	if (!ParseDeclSpecifierSeq())
	{
		Restore(mark);
		return false;
	}
	Mark declarators = Save();
	if (ParseDeclarator())
	{
		while (Take(","))
		{
			if (!ParseDeclarator())
			{
				Restore(mark);
				return false;
			}
			ParseOptionalInitializer();
		}
		ParseOptionalInitializer();
	}
	else
		Restore(declarators);
	if (!Take(";"))
	{
		Restore(mark);
		return false;
	}
	return true;
}

bool Parser::ParseEmptyDeclaration()
{
	return Take(";");
}

bool Parser::ParseAttributeDeclaration()
{
	Mark mark = Save();
	if (!ParseAttributeSpecifier())
	{
		Restore(mark);
		return false;
	}
	while (ParseAttributeSpecifier()) {}
	if (Take(";")) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseStaticAssertDeclaration()
{
	Mark mark = Save();
	if (Take("static_assert") && Take("("))
	{
		++ordinary_depth_;
		if (ParseConditionalExpression() && Take(",") && TakeLiteral() && Take(")"))
		{
			--ordinary_depth_;
			return Take(";");
		}
	}
	Restore(mark);
	return false;
}

bool Parser::ParseAliasDeclaration()
{
	Mark mark = Save();
	if (Take("using") && TakeIdentifier())
	{
		ParseRepeatedAttributes();
		if (Take("=") && ParseTypeId() && Take(";")) return true;
	}
	Restore(mark);
	return false;
}

bool Parser::ParseAsmDefinition()
{
	Mark mark = Save();
	if (Take("asm") && Take("(") && TakeLiteral() && Take(")") && Take(";"))
		return true;
	Restore(mark);
	return false;
}

bool Parser::ParseNamespaceDefinition()
{
	Mark mark = Save();
	Take("inline");
	if (!Take("namespace"))
	{
		Restore(mark);
		return false;
	}
	TakeIdentifier();
	if (!Take("{"))
	{
		Restore(mark);
		return false;
	}
	++ordinary_depth_;
	while (!Is("}") && !AtEnd())
	{
		if (!ParseDeclaration())
		{
			Restore(mark);
			return false;
		}
	}
	if (!Take("}"))
	{
		Restore(mark);
		return false;
	}
	--ordinary_depth_;
	return true;
}

bool Parser::ParseNamespaceAliasDefinition()
{
	Mark mark = Save();
	if (Take("namespace") && TakeIdentifier() && Take("=") &&
		ParseQualifiedNamespaceSpecifier() && Take(";")) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseUsingDeclaration()
{
	Mark mark = Save();
	if (!Take("using")) return false;
	Take("typename");
	if (Take("::"))
	{
		if (ParseUnqualifiedId() && Take(";")) return true;
	}
	else if (ParseNestedNameSpecifier() && ParseUnqualifiedId() && Take(";"))
		return true;
	Restore(mark);
	return false;
}

bool Parser::ParseUsingDirective()
{
	Mark mark = Save();
	ParseRepeatedAttributes();
	if (Take("using") && Take("namespace"))
	{
		Mark nns = Save();
		ParseNestedNameSpecifier();
		if (ParseNamespaceName() && Take(";")) return true;
		Restore(nns);
		if (ParseNamespaceName() && Take(";")) return true;
	}
	Restore(mark);
	return false;
}

bool Parser::ParseLinkageSpecification()
{
	Mark mark = Save();
	if (!Take("extern") || !TakeLiteral())
	{
		Restore(mark);
		return false;
	}
	if (Take("{"))
	{
		++ordinary_depth_;
		while (!Is("}") && !AtEnd())
		{
			if (!ParseDeclaration())
			{
				Restore(mark);
				return false;
			}
		}
		if (!Take("}"))
		{
			Restore(mark);
			return false;
		}
		--ordinary_depth_;
		return true;
	}
	if (ParseDeclaration()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseOpaqueEnumDeclaration()
{
	Mark mark = Save();
	if (!ParseEnumKey()) return false;
	ParseRepeatedAttributes();
	if (TakeIdentifier())
	{
		ParseEnumBase();
		if (Take(";")) return true;
	}
	Restore(mark);
	return false;
}

bool Parser::ParseFunctionDefinition()
{
	Mark mark = Save();
	ParseRepeatedAttributes();
	if (!ParseDeclSpecifierSeq() || !ParseDeclarator())
	{
		Restore(mark);
		return false;
	}
	while (ParseVirtSpecifier()) {}
	if (ParseFunctionBody()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseExplicitInstantiation()
{
	Mark mark = Save();
	Take("extern");
	if (Take("template") && !Is("<") && ParseDeclaration()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseExplicitSpecialization()
{
	Mark mark = Save();
	if (Take("template") && Take("<"))
	{
		EnterAngle();
		if (ParseCloseAngleBracket() && (LeaveAngle(), ParseDeclaration())) return true;
	}
	Restore(mark);
	return false;
}

bool Parser::ParseTemplateDeclaration()
{
	Mark mark = Save();
	if (!Take("template") || !Take("<"))
	{
		Restore(mark);
		return false;
	}
	EnterAngle();
	if (!ParseTemplateParameterList() || !ParseCloseAngleBracket())
	{
		Restore(mark);
		return false;
	}
	LeaveAngle();
	if (ParseDeclaration()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseOptionalInitializer()
{
	Mark mark = Save();
	if (ParseInitializer()) return true;
	Restore(mark);
	return false;
}

} // namespace recog_pa6
