#include "recog_parser_internal.h"

namespace recog_pa6 {

bool Parser::ParseExpression()
{
	Mark mark = Save();
	if (!ParseAssignmentExpression())
	{
		Restore(mark);
		return false;
	}
	while (Take(","))
	{
		if (!ParseAssignmentExpression())
		{
			Restore(mark);
			return false;
		}
	}
	return true;
}

bool Parser::ParseExpressionList()
{
	return ParseInitializerList();
}

bool Parser::ParseAssignmentExpression()
{
	Mark mark = Save();
	if (ParseLogicalOrExpression())
	{
		static const set<string> operators = {
			"=", "*=", "/=", "%=", "+=", "-=", ">>=", "<<=", "&=", "^=", "|="
		};
		Mark after_left = Save();
		if (TakeAnyOperator(operators) && ParseInitializerClause())
			return true;
		Restore(after_left);
	}
	Restore(mark);
	if (ParseThrowExpression()) return true;
	Restore(mark);
	return ParseConditionalExpression();
}

bool Parser::ParseConditionalExpression()
{
	Mark mark = Save();
	if (!ParseLogicalOrExpression())
	{
		Restore(mark);
		return false;
	}
	if (Take("?"))
	{
		if (!ParseExpression() || !Take(":") || !ParseAssignmentExpression())
		{
			Restore(mark);
			return false;
		}
	}
	return true;
}

bool Parser::ParseLogicalOrExpression()
{
	Mark mark = Save();
	if (!ParseLogicalAndExpression())
	{
		Restore(mark);
		return false;
	}
	while (TakeOperator("||"))
	{
		if (!ParseLogicalAndExpression())
		{
			Restore(mark);
			return false;
		}
	}
	return true;
}

bool Parser::ParseLogicalAndExpression()
{
	Mark mark = Save();
	if (!ParseInclusiveOrExpression())
	{
		Restore(mark);
		return false;
	}
	while (TakeOperator("&&"))
	{
		if (!ParseInclusiveOrExpression())
		{
			Restore(mark);
			return false;
		}
	}
	return true;
}

bool Parser::ParseInclusiveOrExpression()
{
	Mark mark = Save();
	if (!ParseExclusiveOrExpression())
	{
		Restore(mark);
		return false;
	}
	while (TakeOperator("|"))
	{
		if (!ParseExclusiveOrExpression())
		{
			Restore(mark);
			return false;
		}
	}
	return true;
}

bool Parser::ParseExclusiveOrExpression()
{
	Mark mark = Save();
	if (!ParseAndExpression())
	{
		Restore(mark);
		return false;
	}
	while (TakeOperator("^"))
	{
		if (!ParseAndExpression())
		{
			Restore(mark);
			return false;
		}
	}
	return true;
}

bool Parser::ParseAndExpression()
{
	Mark mark = Save();
	if (!ParseEqualityExpression())
	{
		Restore(mark);
		return false;
	}
	while (TakeOperator("&"))
	{
		if (!ParseEqualityExpression())
		{
			Restore(mark);
			return false;
		}
	}
	return true;
}

bool Parser::ParseEqualityExpression()
{
	Mark mark = Save();
	if (!ParseRelationalExpression())
	{
		Restore(mark);
		return false;
	}
	while (TakeOperator("==") || TakeOperator("!="))
	{
		if (!ParseRelationalExpression())
		{
			Restore(mark);
			return false;
		}
	}
	return true;
}

bool Parser::ParseRelationalExpression()
{
	Mark mark = Save();
	if (!ParseShiftExpression())
	{
		Restore(mark);
		return false;
	}
	for (;;)
	{
		Mark operator_mark = Save();
		bool found = false;
		if (TakeOperator("<") || TakeOperator("<=") || TakeOperator(">="))
			found = true;
		else if (!CloseAngleBlocked() && TakeOperator(">"))
			found = true;
		if (!found)
		{
			Restore(operator_mark);
			break;
		}
		if (!ParseShiftExpression())
		{
			Restore(mark);
			return false;
		}
	}
	return true;
}

bool Parser::ParseShiftExpression()
{
	Mark mark = Save();
	if (!ParseAdditiveExpression())
	{
		Restore(mark);
		return false;
	}
	for (;;)
	{
		Mark operator_mark = Save();
		bool found = TakeOperator("<<");
		if (!found) found = TakeShiftRight();
		if (!found)
		{
			Restore(operator_mark);
			break;
		}
		if (!ParseAdditiveExpression())
		{
			Restore(mark);
			return false;
		}
	}
	return true;
}

bool Parser::ParseAdditiveExpression()
{
	Mark mark = Save();
	if (!ParseMultiplicativeExpression())
	{
		Restore(mark);
		return false;
	}
	while (TakeOperator("+") || TakeOperator("-"))
	{
		if (!ParseMultiplicativeExpression())
		{
			Restore(mark);
			return false;
		}
	}
	return true;
}

bool Parser::ParseMultiplicativeExpression()
{
	Mark mark = Save();
	if (!ParsePMExpression())
	{
		Restore(mark);
		return false;
	}
	while (TakeOperator("*") || TakeOperator("/") || TakeOperator("%"))
	{
		if (!ParsePMExpression())
		{
			Restore(mark);
			return false;
		}
	}
	return true;
}

bool Parser::ParsePMExpression()
{
	Mark mark = Save();
	if (!ParseCastExpression())
	{
		Restore(mark);
		return false;
	}
	while (TakeOperator(".*") || TakeOperator("->*"))
	{
		if (!ParseCastExpression())
		{
			Restore(mark);
			return false;
		}
	}
	return true;
}

bool Parser::ParseCastExpression()
{
	Mark mark = Save();
	if (Take("(") && ParseTypeId() && Take(")") && ParseCastExpression())
		return true;
	Restore(mark);
	if (ParseUnaryExpression()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseUnaryExpression()
{
	Mark mark = Save();
	static const set<string> unary_operators = {
		"++", "--", "*", "&", "+", "-", "!", "~"
	};
	if (TakeAnyOperator(unary_operators) && ParseCastExpression()) return true;
	Restore(mark);

	if (Take("sizeof"))
	{
		if (Take("..."))
		{
			if (Take("(") && TakeIdentifier() && Take(")")) return true;
		}
		Restore(mark);
		if (Take("sizeof") && Take("(") && ParseTypeId() && Take(")"))
			return true;
		Restore(mark);
		if (Take("sizeof") && ParseUnaryExpression()) return true;
	}
	Restore(mark);

	if (Take("alignof") && Take("(") && ParseTypeId() && Take(")"))
		return true;
	Restore(mark);
	if (ParseNoexceptExpression()) return true;
	Restore(mark);
	if (ParseNewExpression()) return true;
	Restore(mark);
	if (ParseDeleteExpression()) return true;
	Restore(mark);
	return ParsePostfixExpression();
}

bool Parser::ParseNoexceptExpression()
{
	Mark mark = Save();
	if (Take("noexcept") && Take("("))
	{
		++ordinary_depth_;
		if (ParseExpression() && Take(")"))
		{
			--ordinary_depth_;
			return true;
		}
	}
	Restore(mark);
	return false;
}

bool Parser::ParseNewExpression()
{
	Mark mark = Save();
	Take("::");
	if (!Take("new"))
	{
		Restore(mark);
		return false;
	}

	// The parenthesized type-id form must be tested before placement.  A
	// placement such as (3, 2) cannot parse as a type-id and naturally falls
	// through to the ordinary form.
	Mark parenthesized_type = Save();
	if (Take("("))
	{
		++ordinary_depth_;
		if (ParseTypeId() && Take(")"))
		{
			--ordinary_depth_;
			ParseNewInitializer();
			return true;
		}
	}
	Restore(parenthesized_type);

	Mark placement = Save();
	if (Take("("))
	{
		++ordinary_depth_;
		if (ParseExpressionList() && Take(")"))
		{
			--ordinary_depth_;
		}
		else
			Restore(placement);
	}
	if (!ParseTypeSpecifierSeq())
	{
		Restore(mark);
		return false;
	}
	Mark declarator = Save();
	if (!ParseNewDeclarator()) Restore(declarator);
	ParseNewInitializer();
	return true;
}

bool Parser::ParseNewInitializer()
{
	Mark mark = Save();
	if (Take("("))
	{
		++ordinary_depth_;
		if ((Is(")") || ParseExpressionList()) && Take(")"))
		{
			--ordinary_depth_;
			return true;
		}
		Restore(mark);
	}
	Restore(mark);
	if (ParseBracedInitList()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseDeleteExpression()
{
	Mark mark = Save();
	Take("::");
	if (!Take("delete"))
	{
		Restore(mark);
		return false;
	}
	Mark array = Save();
	if (Take("[") && Take("]")) {}
	else Restore(array);
	if (ParseCastExpression()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseThrowExpression()
{
	Mark mark = Save();
	if (!Take("throw")) return false;
	if (Is(";") || Is(")") || Is(",") || Is("}")) return true;
	if (ParseAssignmentExpression()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParsePostfixExpression()
{
	Mark mark = Save();
	if (!ParsePostfixRoot())
	{
		Restore(mark);
		return false;
	}
	while (true)
	{
		Mark suffix_mark = Save();
		if (!ParsePostfixSuffix())
		{
			Restore(suffix_mark);
			break;
		}
	}
	return true;
}

bool Parser::ParsePostfixRoot()
{
	Mark mark = Save();
	if (ParsePrimaryExpression()) return true;
	Restore(mark);

	if (ParseSimpleTypeSpecifier())
	{
		if (Take("(") )
		{
			++ordinary_depth_;
			if (Is(")") || ParseExpressionList())
			{
				if (Take(")"))
				{
					--ordinary_depth_;
					return true;
				}
			}
			--ordinary_depth_;
		}
		if (ParseBracedInitList()) return true;
	}
	Restore(mark);
	if (ParseTypenameSpecifier())
	{
		if (Take("("))
		{
			++ordinary_depth_;
			if (Is(")") || ParseExpressionList())
			{
				if (Take(")"))
				{
					--ordinary_depth_;
					return true;
				}
			}
			--ordinary_depth_;
		}
		if (ParseBracedInitList()) return true;
	}
	Restore(mark);

	static const set<string> casts = {
		"dynamic_cast", "static_cast", "reinterpret_cast", "const_cast"
	};
	for (set<string>::const_iterator it = casts.begin(); it != casts.end(); ++it)
	{
		Mark cast_mark = Save();
		if (Take(*it) && Take("<") )
		{
			EnterAngle();
			if (ParseTypeId() && ParseCloseAngleBracket())
			{
				LeaveAngle();
				if (Take("("))
				{
					++ordinary_depth_;
					if (ParseExpression() && Take(")"))
					{
						--ordinary_depth_;
						return true;
					}
				}
			}
		}
		Restore(cast_mark);
	}
	Restore(mark);

	if (Take("typeid") && Take("("))
	{
		++ordinary_depth_;
		Mark expression_mark = Save();
		if (ParseExpression() && Take(")"))
		{
			--ordinary_depth_;
			return true;
		}
		Restore(expression_mark);
		if (ParseTypeId() && Take(")"))
		{
			--ordinary_depth_;
			return true;
		}
		--ordinary_depth_;
	}
	Restore(mark);
	return false;
}

bool Parser::ParsePostfixSuffix()
{
	Mark mark = Save();
	if (Take("["))
	{
		++ordinary_depth_;
		if (ParseExpression() && Take("]"))
		{
			--ordinary_depth_;
			return true;
		}
		Restore(mark);
	}
	Restore(mark);
	if (Take("[") && ParseBracedInitList() && Take("]")) return true;
	Restore(mark);

	if (Take("("))
	{
		++ordinary_depth_;
		if ((Is(")") || ParseExpressionList()) && Take(")"))
		{
			--ordinary_depth_;
			return true;
		}
		--ordinary_depth_;
	}
	Restore(mark);

	if (Take(".") || Take("->"))
	{
		Mark target = Save();
		if (ParsePseudoDestructorName() ||
			(Take("template") && ParseIdExpression()) || ParseIdExpression())
			return true;
		Restore(target);
	}
	Restore(mark);
	if (Take("++") || Take("--")) return true;
	Restore(mark);
	return false;
}

bool Parser::ParsePrimaryExpression()
{
	Mark mark = Save();
	if (Is("true") || Is("false") || Is("nullptr") || Is("this"))
	{
		++position_;
		return true;
	}
	if (TakeLiteral()) return true;
	Restore(mark);
	if (Take("("))
	{
		++ordinary_depth_;
		if (ParseExpression() && Take(")"))
		{
			--ordinary_depth_;
			return true;
		}
		--ordinary_depth_;
	}
	Restore(mark);
	if (ParseLambdaExpression()) return true;
	Restore(mark);
	return ParseIdExpression();
}

bool Parser::ParseLambdaExpression()
{
	Mark mark = Save();
	if (ParseLambdaIntroducer())
	{
		ParseLambdaDeclarator();
		if (ParseCompoundStatement()) return true;
	}
	Restore(mark);
	return false;
}

bool Parser::ParseLambdaIntroducer()
{
	Mark mark = Save();
	if (!Take("[")) return false;
	++ordinary_depth_;
	if (!Is("]") && !ParseLambdaCapture())
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
	return true;
}

bool Parser::ParseLambdaCapture()
{
	Mark mark = Save();
	if (ParseCaptureDefault())
	{
		if (Take(","))
		{
			if (ParseCaptureList()) return true;
			Restore(mark);
			return false;
		}
		if (Is("]")) return true;
	}
	Restore(mark);
	if (ParseCaptureList()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseCaptureDefault()
{
	return Take("&") || Take("=");
}

bool Parser::ParseCaptureList()
{
	Mark mark = Save();
	if (!ParseCapture())
	{
		Restore(mark);
		return false;
	}
	Take("...");
	while (Take(","))
	{
		if (!ParseCapture())
		{
			Restore(mark);
			return false;
		}
		Take("...");
	}
	return true;
}

bool Parser::ParseCapture()
{
	Mark mark = Save();
	if (Take("this")) return true;
	Restore(mark);
	if (Take("&") && TakeIdentifier()) return true;
	Restore(mark);
	return TakeIdentifier();
}

bool Parser::ParseLambdaDeclarator()
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
	Take("mutable");
	ParseExceptionSpecification();
	ParseRepeatedAttributes();
	ParseTrailingReturnType();
	return true;
}

bool Parser::ParseIdExpression()
{
	Mark mark = Save();
	if (ParseQualifiedId()) return true;
	Restore(mark);
	if (ParseUnqualifiedId()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseQualifiedId()
{
	Mark mark = Save();
	if (!ParseNestedNameSpecifier())
	{
		Restore(mark);
		return false;
	}
	Take("template");
	if (!ParseUnqualifiedId())
	{
		Restore(mark);
		return false;
	}
	return true;
}

bool Parser::ParseUnqualifiedId()
{
	Mark mark = Save();
	if (ParseOperatorFunctionId()) return true;
	Restore(mark);
	if (ParseConversionFunctionId()) return true;
	Restore(mark);
	if (ParseLiteralOperatorId()) return true;
	Restore(mark);
	if (Take("~") && (ParseClassName() || ParseDecltypeSpecifier())) return true;
	Restore(mark);
	if (ParseTemplateId()) return true;
	Restore(mark);
	return TakeIdentifier();
}

bool Parser::ParseTemplateName()
{
	if (!IsIdentifierToken(Peek()) || !Peek().names.template_name) return false;
	++position_;
	return true;
}

bool Parser::ParseSimpleTemplateId()
{
	Mark mark = Save();
	if (!ParseTemplateName() || !Take("<"))
	{
		Restore(mark);
		return false;
	}
	EnterAngle();
	if (!ParseCloseAngleBracket())
	{
		if (!ParseTemplateArgumentList() || !ParseCloseAngleBracket())
		{
			Restore(mark);
			return false;
		}
	}
	LeaveAngle();
	return true;
}

bool Parser::ParseTemplateId()
{
	Mark mark = Save();
	if (ParseSimpleTemplateId()) return true;
	Restore(mark);
	if (ParseOperatorFunctionId() && Take("<"))
	{
		EnterAngle();
		if ((ParseCloseAngleBracket() ||
			(ParseTemplateArgumentList() && ParseCloseAngleBracket())))
		{
			LeaveAngle();
			return true;
		}
	}
	Restore(mark);
	if (ParseLiteralOperatorId() && Take("<"))
	{
		EnterAngle();
		if ((ParseCloseAngleBracket() ||
			(ParseTemplateArgumentList() && ParseCloseAngleBracket())))
		{
			LeaveAngle();
			return true;
		}
	}
	Restore(mark);
	return false;
}

bool Parser::ParseClassName()
{
	Mark mark = Save();
	if (ParseSimpleTemplateId()) return true;
	Restore(mark);
	if (IsIdentifierToken(Peek()) && Peek().names.class_name)
	{
		++position_;
		return true;
	}
	Restore(mark);
	return false;
}

bool Parser::ParseEnumName()
{
	if (IsIdentifierToken(Peek()) && Peek().names.enum_name)
	{
		++position_;
		return true;
	}
	return false;
}

bool Parser::ParseTypedefName()
{
	if (IsIdentifierToken(Peek()) && Peek().names.typedef_name)
	{
		++position_;
		return true;
	}
	return false;
}

bool Parser::ParseNamespaceName()
{
	if (IsIdentifierToken(Peek()) && Peek().names.namespace_name)
	{
		++position_;
		return true;
	}
	return false;
}

bool Parser::ParseTypeName()
{
	Mark mark = Save();
	if (ParseClassName()) return true;
	Restore(mark);
	if (ParseEnumName()) return true;
	Restore(mark);
	if (ParseTypedefName()) return true;
	Restore(mark);
	return ParseSimpleTemplateId();
}

bool Parser::ParseNestedNameSpecifier()
{
	Mark mark = Save();
	if (Take("::"))
	{
		// Root-only global scope is a complete nested-name-specifier.
	}
	else
	{
		Mark root = Save();
		if (!(ParseTypeName() || ParseNamespaceName() ||
			(ParseDecltypeSpecifier())))
		{
			Restore(mark);
			return false;
		}
		if (!Take("::"))
		{
			Restore(mark);
			return false;
		}
		(void)root;
	}
	while (true)
	{
		Mark suffix = Save();
		if (Take("template"))
		{
			if (ParseSimpleTemplateId() && Take("::")) continue;
			Restore(suffix);
		}
		if (ParseSimpleTemplateId() && Take("::")) continue;
		Restore(suffix);
		if (TakeIdentifier() && Take("::")) continue;
		Restore(suffix);
		break;
	}
	return true;
}

bool Parser::ParseDecltypeSpecifier()
{
	Mark mark = Save();
	if (Take("decltype") && Take("("))
	{
		++ordinary_depth_;
		if (ParseExpression() && Take(")"))
		{
			--ordinary_depth_;
			return true;
		}
	}
	Restore(mark);
	return false;
}

bool Parser::ParseTypenameSpecifier()
{
	Mark mark = Save();
	if (!Take("typename") || !ParseNestedNameSpecifier())
	{
		Restore(mark);
		return false;
	}
	if (Take("template") && ParseSimpleTemplateId()) return true;
	Restore(mark);
	if (Take("typename") && ParseNestedNameSpecifier() && TakeIdentifier()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParsePseudoDestructorName()
{
	Mark mark = Save();
	ParseNestedNameSpecifier();
	if (Take("~") && ParseTypeName()) return true;
	Restore(mark);
	if (Take("~") && ParseDecltypeSpecifier()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseOperatorFunctionId()
{
	Mark mark = Save();
	if (!Take("operator")) return false;
	if (Take("new"))
	{
		Mark array = Save();
		if (Take("[") && Take("]")) return true;
		Restore(array);
		return true;
	}
	if (Take("delete"))
	{
		if (Take("[") && Take("]")) return true;
		return true;
	}
	static const set<string> operators = {
		"+", "-", "*", "/", "%", "^", "&", "|", "~", "!", "=", "<", ">",
		"+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=", "<<", ">>", ">>=", "<<=",
		"==", "!=", "<=", ">=", "&&", "||", "++", "--", ",", ".*", "->*", "->"
	};
	if (TakeAnyOperator(operators)) return true;
	if (Take("(") && Take(")")) return true;
	if (Take("[") && Take("]")) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseConversionFunctionId()
{
	Mark mark = Save();
	if (Take("operator") && ParseConversionTypeId()) return true;
	Restore(mark);
	return false;
}

bool Parser::ParseConversionTypeId()
{
	Mark mark = Save();
	if (!ParseTypeSpecifierSeq())
	{
		Restore(mark);
		return false;
	}
	while (ParsePtrOperator()) {}
	return true;
}

bool Parser::ParseLiteralOperatorId()
{
	Mark mark = Save();
	if (Take("operator") && IsEmptyStringLiteral(Peek()))
	{
		++position_;
		if (TakeIdentifier()) return true;
	}
	Restore(mark);
	return false;
}

} // namespace recog_pa6
