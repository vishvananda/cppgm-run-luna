#include "recog_parser_internal.h"

namespace recog_pa6 {

bool Parser::ParseStatement()
{
	Mark mark = Save();
	if (ParseLabeledStatement()) return true;
	Restore(mark);
	if (ParseSelectionStatement()) return true;
	Restore(mark);
	if (ParseIterationStatement()) return true;
	Restore(mark);
	if (ParseJumpStatement()) return true;
	Restore(mark);
	if (ParseTryBlock()) return true;
	Restore(mark);
	if (ParseCompoundStatement()) return true;
	Restore(mark);

	// In block scope a declaration is preferred when its first name has one of
	// the PA6 mock type facts.  If the declaration cannot consume its complete
	// form, restoring here gives the expression grammar the C++ 6.8 cases.
	if (StartsDeclaration() && ParseBlockDeclaration()) return true;
	Restore(mark);
	if (ParseExpressionStatement()) return true;
	Restore(mark);
	if (ParseBlockDeclaration()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseLabeledStatement()
{
	Mark mark = Save();
	ParseRepeatedAttributes();
	if (TakeIdentifier() && Take(":") && ParseStatement()) return true;
	Restore(mark);
	ParseRepeatedAttributes();
	if (Take("case") && ParseConditionalExpression() && Take(":") &&
		ParseStatement()) return true;
	Restore(mark);
	ParseRepeatedAttributes();
	if (Take("default") && Take(":") && ParseStatement()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseExpressionStatement()
{
	Mark mark = Save();
	ParseRepeatedAttributes();
	if (!Is(";") && !ParseExpression())
	{
		Restore(mark);
		return false;
	}
	if (Take(";")) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseCompoundStatement()
{
	Mark mark = Save();
	ParseRepeatedAttributes();
	if (!Take("{")) return false;
	++ordinary_depth_;
	while (!Is("}") && !AtEnd())
	{
		if (!ParseStatement())
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

bool Parser::ParseSelectionStatement()
{
	Mark mark = Save();
	ParseRepeatedAttributes();
	if (Take("if"))
	{
		if (Take("(") )
		{
			++ordinary_depth_;
			if (ParseCondition() && Take(")"))
			{
				--ordinary_depth_;
				if (ParseStatement())
				{
					Mark otherwise = Save();
					if (Take("else") && ParseStatement()) return true;
					Restore(otherwise);
					return true;
				}
			}
		}
	}
	Restore(mark);
	ParseRepeatedAttributes();
	if (Take("switch") && Take("("))
	{
		++ordinary_depth_;
		if (ParseCondition() && Take(")"))
		{
			--ordinary_depth_;
			if (ParseStatement()) return true;
		}
	}
	Restore(mark);
	return false;
}

bool Parser::ParseCondition()
{
	Mark mark = Save();
	if (ParseConditionDeclaration()) return true;
	Restore(mark);
	return ParseExpression();
}

bool Parser::ParseConditionDeclaration()
{
	Mark mark = Save();
	ParseRepeatedAttributes();
	if (!ParseDeclSpecifierSeq() || !ParseDeclarator())
	{
		Restore(mark);
		return false;
	}
	if (Take("=") && ParseInitializerClause()) return true;
	Restore(mark);
	if (ParseDeclSpecifierSeq() && ParseDeclarator() && ParseBracedInitList()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseIterationStatement()
{
	Mark mark = Save();
	ParseRepeatedAttributes();
	if (Take("while") && Take("("))
	{
		++ordinary_depth_;
		if (ParseCondition() && Take(")"))
		{
			--ordinary_depth_;
			if (ParseStatement()) return true;
		}
	}
	Restore(mark);
	ParseRepeatedAttributes();
	if (Take("do") && ParseStatement() && Take("while") && Take("("))
	{
		++ordinary_depth_;
		if (ParseExpression() && Take(")") && Take(";"))
		{
			--ordinary_depth_;
			return true;
		}
	}
	Restore(mark);
	ParseRepeatedAttributes();
	if (Take("for") && Take("("))
	{
		++ordinary_depth_;
		Mark range = Save();
		if (ParseForRangeDeclaration() && Take(":") && ParseForRangeInitializer() &&
			Take(")"))
		{
			--ordinary_depth_;
			if (ParseStatement()) return true;
		}
		Restore(range);
	}
	Restore(mark);

	ParseRepeatedAttributes();
	if (Take("for") && Take("("))
	{
		++ordinary_depth_;
		Mark init = Save();
		if (ParseSimpleDeclaration())
		{
			Mark condition = Save();
			if (!Is(";") && !ParseCondition()) Restore(condition);
			if (Take(";"))
			{
				Mark increment = Save();
				if (!Is(")") && !ParseExpression()) Restore(increment);
				if (Take(")"))
				{
					--ordinary_depth_;
					if (ParseStatement()) return true;
				}
			}
		}
		Restore(init);
		if (ParseExpressionStatement())
		{
			Mark condition = Save();
			if (!Is(";") && !ParseCondition()) Restore(condition);
			if (Take(";"))
			{
				Mark increment = Save();
				if (!Is(")") && !ParseExpression()) Restore(increment);
				if (Take(")"))
				{
					--ordinary_depth_;
					if (ParseStatement()) return true;
				}
			}
		}
	}
	Restore(mark);
	return false;
}

bool Parser::ParseForRangeDeclaration()
{
	Mark mark = Save();
	ParseRepeatedAttributes();
	if (ParseDeclSpecifierSeq() && ParseDeclarator()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseForRangeInitializer()
{
	Mark mark = Save();
	if (ParseExpression()) return true;
	Restore(mark);
	return ParseBracedInitList();
}

bool Parser::ParseJumpStatement()
{
	Mark mark = Save();
	ParseRepeatedAttributes();
	if (Take("break") || Take("continue")) return Take(";");
	Restore(mark);
	ParseRepeatedAttributes();
	if (Take("return"))
	{
		if (Take("{"))
		{
			Restore(mark);
			if (Take("return") && ParseBracedInitList() && Take(";")) return true;
			Restore(mark);
			return false;
		}
		if (!Is(";") && !ParseExpression())
		{
			Restore(mark);
			return false;
		}
		return Take(";");
	}
	Restore(mark);
	ParseRepeatedAttributes();
	if (Take("goto") && TakeIdentifier() && Take(";")) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseTryBlock()
{
	Mark mark = Save();
	ParseRepeatedAttributes();
	if (Take("try") && ParseCompoundStatement() && ParseHandler())
	{
		while (ParseHandler()) {}
		return true;
	}
	Restore(mark);
	return false;
}

bool Parser::ParseFunctionTryBlock()
{
	Mark mark = Save();
	if (!Take("try")) return false;
	ParseCtorInitializer();
	if (!ParseCompoundStatement() || !ParseHandler())
	{
		Restore(mark);
		return false;
	}
	while (ParseHandler()) {}
	return true;
}

bool Parser::ParseHandler()
{
	Mark mark = Save();
	if (Take("catch") && Take("("))
	{
		++ordinary_depth_;
		if (ParseExceptionDeclaration() && Take(")"))
		{
			--ordinary_depth_;
			if (ParseCompoundStatement()) return true;
		}
	}
	Restore(mark);
	return false;
}

bool Parser::ParseExceptionDeclaration()
{
	Mark mark = Save();
	if (Take("...")) return true;
	ParseRepeatedAttributes();
	if (ParseDeclSpecifierSeq())
	{
		ParseRepeatedAttributes();
		Mark declarator = Save();
		if (!ParseDeclarator()) Restore(declarator);
		return true;
	}
	Restore(mark);
	return false;
}

bool Parser::ParseQualifiedNamespaceSpecifier()
{
	Mark mark = Save();
	ParseNestedNameSpecifier();
	if (ParseNamespaceName()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseTemplateParameterList()
{
	Mark mark = Save();
	if (!ParseTemplateParameter())
	{
		Restore(mark);
		return false;
	}
	while (Take(","))
	{
		if (!ParseTemplateParameter())
		{
			Restore(mark);
			return false;
		}
	}
	return true;
}

bool Parser::ParseTemplateParameter()
{
	Mark mark = Save();
	if (ParseTypeParameter()) return true;
	Restore(mark);
	if (ParseParameterDeclaration()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseTypeParameter()
{
	Mark mark = Save();
	if (Take("template") && Take("<"))
	{
		EnterAngle();
		if (ParseTemplateParameterList() && ParseCloseAngleBracket())
		{
			LeaveAngle();
			if (Take("class"))
			{
				Take("...");
				TakeIdentifier();
				if (Take("=") && !ParseIdExpression())
				{
					Restore(mark);
					return false;
				}
				return true;
			}
		}
	}
	Restore(mark);

	if (Take("class") || Take("typename"))
	{
		Take("...");
		TakeIdentifier();
		if (Take("=") && !ParseTypeId())
		{
			Restore(mark);
			return false;
		}
		return true;
	}
	Restore(mark);
	return false;
}

bool Parser::ParseTemplateArgumentList()
{
	Mark mark = Save();
	if (!ParseTemplateArgumentDots())
	{
		Restore(mark);
		return false;
	}
	while (Take(","))
	{
		if (!ParseTemplateArgumentDots())
		{
			Restore(mark);
			return false;
		}
	}
	return true;
}

bool Parser::ParseTemplateArgumentDots()
{
	Mark mark = Save();
	if (!ParseTemplateArgument())
	{
		Restore(mark);
		return false;
	}
	Mark dots = Save();
	if (Take("...")) return true;
	Restore(dots);
	return true;
}

bool Parser::ParseTemplateArgument()
{
	Mark mark = Save();
	if (ParseConditionalExpression()) return true;
	Restore(mark);
	if (ParseTypeId()) return true;
	Restore(mark);
	return ParseIdExpression();
}

bool Parser::ParseCloseAngleBracket()
{
	return TakeCloseAngle();
}

bool Parser::ParseAttributeSpecifier()
{
	Mark mark = Save();
	if (Take("[") && Take("["))
	{
		++ordinary_depth_;
		if (ParseAttributeList() && Take("]") && Take("]"))
		{
			--ordinary_depth_;
			return true;
		}
	}
	Restore(mark);
	return ParseAlignmentSpecifier();
}

bool Parser::ParseAlignmentSpecifier()
{
	Mark mark = Save();
	if (!Take("alignas") || !Take("("))
	{
		Restore(mark);
		return false;
	}
	++ordinary_depth_;
	Mark type = Save();
	if (!ParseTypeId())
	{
		Restore(type);
		if (!ParseAssignmentExpression())
		{
			Restore(mark);
			return false;
		}
	}
	Take("...");
	if (!Take(")"))
	{
		Restore(mark);
		return false;
	}
	--ordinary_depth_;
	return true;
}

bool Parser::ParseAttributeList()
{
	Mark mark = Save();
	if (!ParseAttributePart())
	{
		Restore(mark);
		return false;
	}
	while (Take(","))
	{
		if (!ParseAttributePart())
		{
			Restore(mark);
			return false;
		}
	}
	return true;
}

bool Parser::ParseAttributePart()
{
	Mark mark = Save();
	if (ParseAttribute())
	{
		Mark dots = Save();
		if (Take("...")) return true;
		Restore(dots);
		return true;
	}
	Restore(mark);
	return true; // The grammar permits an empty attribute-part.
}

bool Parser::ParseAttribute()
{
	Mark mark = Save();
	if (!ParseAttributeToken())
	{
		Restore(mark);
		return false;
	}
	Mark argument = Save();
	if (ParseAttributeArgumentClause()) return true;
	Restore(argument);
	return true;
}

bool Parser::ParseAttributeToken()
{
	Mark mark = Save();
	if (ParseAttributeScopedToken()) return true;
	Restore(mark);
	return TakeIdentifier();
}

bool Parser::ParseAttributeScopedToken()
{
	Mark mark = Save();
	if (TakeIdentifier() && Take("::") && TakeIdentifier()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseAttributeArgumentClause()
{
	Mark mark = Save();
	if (!Take("(")) return false;
	++ordinary_depth_;
	while (!Is(")") && !AtEnd())
	{
		if (!ParseBalancedToken())
		{
			Restore(mark);
			return false;
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

bool Parser::ParseBalancedToken()
{
	Mark mark = Save();
	const string opener = Peek().text;
	const string closer = opener == "(" ? ")" :
		opener == "[" ? "]" : opener == "{" ? "}" : string();
	if (!closer.empty())
	{
		++position_;
		++ordinary_depth_;
		while (!Is(closer) && !AtEnd())
		{
			if (!ParseBalancedToken())
			{
				Restore(mark);
				return false;
			}
		}
		if (!Take(closer))
		{
			Restore(mark);
			return false;
		}
		--ordinary_depth_;
		return true;
	}
	if (AtEnd() || opener == ")" || opener == "]" || opener == "}")
	{
		Restore(mark);
		return false;
	}
	++position_;
	return true;
}

bool Parser::ParseRepeatedAttributes()
{
	while (ParseAttributeSpecifier()) {}
	return true;
}

bool Parser::StartsTypeSpecifier() const
{
	static const set<string> starts = {
		"char", "char16_t", "char32_t", "wchar_t", "bool", "short", "int",
		"long", "signed", "unsigned", "float", "double", "void", "auto",
		"const", "volatile", "class", "struct", "union", "enum", "typename",
		"decltype", "typedef", "static", "extern", "virtual", "inline", "friend",
		"constexpr", "register", "thread_local", "mutable"
	};
	if (starts.find(Peek().text) != starts.end()) return true;
	return IsIdentifierToken(Peek()) && (Peek().names.class_name ||
		Peek().names.template_name || Peek().names.typedef_name ||
		Peek().names.enum_name);
}

bool Parser::StartsDeclaration() const
{
	return StartsTypeSpecifier() || Is(";") || Is("using") || Is("namespace") ||
		Is("asm") || Is("static_assert") || Is("enum") || Is("class") ||
		Is("struct") || Is("union") || Is("template") || Is("extern");
}

} // namespace recog_pa6
