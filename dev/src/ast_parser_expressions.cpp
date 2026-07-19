#include "ast_parser.h"

#include <set>
using namespace std;

namespace {

string OperatorValue(const string& text)
{
	static const char* const values[] = {"=", "+=", "-=", "*=", "/=", "%=", "^=",
		"&=", "|=", "<<=", ">>=", "?"};
	for (size_t i = 0; i < sizeof(values) / sizeof(*values); ++i)
		if (text == values[i]) return text;
	return text;
}

} // namespace

namespace cppgm_pa10 {

CPPGMAstNodePtr Parser::ParseExpression()
{
	CPPGMAstNodePtr result = ParseAssignmentExpression();
	if (!result) return CPPGMAstNodePtr();
	while (Take(","))
	{
		CPPGMAstNodePtr right = ParseAssignmentExpression();
		if (!right) return CPPGMAstNodePtr();
		CPPGMAstNodePtr binary = Node("binary-expression", TokenLabel(",") + ":,");
		Add(binary, result);
		Add(binary, right);
		result = binary;
	}
	return result;
}

CPPGMAstNodePtr Parser::ParseAssignmentExpression()
{
	CPPGMAstNodePtr left = ParseConditionalExpression();
	if (!left) return CPPGMAstNodePtr();
	static const char* const operators[] = {"=", "+=", "-=", "*=", "/=", "%=", "^=",
		"&=", "|=", "<<=", ">>="};
	for (size_t i = 0; i < sizeof(operators) / sizeof(*operators); ++i)
	{
		if (!Is(operators[i])) continue;
		const string op = operators[i];
		++position_;
		CPPGMAstNodePtr right = ParseAssignmentExpression();
		if (!right) return CPPGMAstNodePtr();
		CPPGMAstNodePtr result = Node("assignment-expression",
			TokenLabel(op) + ":" + OperatorValue(op));
		Add(result, left);
		Add(result, right);
		return result;
	}
	return left;
}

CPPGMAstNodePtr Parser::ParseConditionalExpression()
{
	CPPGMAstNodePtr condition = ParseBinaryExpression(0);
	if (!condition) return CPPGMAstNodePtr();
	if (!Take("?")) return condition;
	CPPGMAstNodePtr when_true = ParseExpression();
	if (!when_true || !Take(":")) return CPPGMAstNodePtr();
	CPPGMAstNodePtr when_false = ParseAssignmentExpression();
	if (!when_false) return CPPGMAstNodePtr();
	CPPGMAstNodePtr result = Node("conditional-expression");
	Add(result, condition);
	Add(result, when_true);
	Add(result, when_false);
	return result;
}

CPPGMAstNodePtr Parser::ParseBinaryExpression(int level)
{
	if (level > 10) return ParseUnaryExpression();
	CPPGMAstNodePtr left = level == 10 ? ParseUnaryExpression() :
		ParseBinaryExpression(level + 1);
	if (!left) return CPPGMAstNodePtr();
	while (true)
	{
		string op;
		if (level == 0 && (Is("||") || Is("or"))) op = Peek().text;
		else if (level == 1 && (Is("&&") || Is("and"))) op = Peek().text;
		else if (level == 2 && (Is("|") || Is("bitor"))) op = Peek().text;
		else if (level == 3 && (Is("^") || Is("xor"))) op = Peek().text;
		else if (level == 4 && (Is("&") || Is("bitand"))) op = Peek().text;
		else if (level == 5 && (Is("==") || Is("!=") || Is("not_eq"))) op = Peek().text;
		else if (level == 6 && (Is("<") || Is(">") || Is("<=") || Is(">=")))
		{
			if ((Is(">") || Is(">=")) && CloseAngleBlocked()) break;
			op = Peek().text;
		}
		else if (level == 7 && Is("<<")) op = Peek().text;
		else if (level == 7 && Peek().kind == AST_RSHIFT_1 && Peek(1).kind == AST_RSHIFT_2)
			op = ">>";
		else if (level == 8 && (Is("+") || Is("-"))) op = Peek().text;
		else if (level == 9 && (Is("*") || Is("/") || Is("%"))) op = Peek().text;
		else if (level == 10 && (Is(".*") || Is("->*"))) op = Peek().text;
		else break;
		if (op == ">>") TakeShiftRight();
		else ++position_;
		CPPGMAstNodePtr right = level == 10 ? ParseUnaryExpression() :
			ParseBinaryExpression(level + 1);
		if (!right) return CPPGMAstNodePtr();
		CPPGMAstNodePtr result = Node("binary-expression", TokenLabel(op) + ":" + op);
		Add(result, left);
		Add(result, right);
		left = result;
	}
	return left;
}

CPPGMAstNodePtr Parser::ParseUnaryExpression()
{
	static const char* const operators[] = {"++", "--", "*", "&", "+", "-", "!", "~"};
	for (size_t i = 0; i < sizeof(operators) / sizeof(*operators); ++i)
	{
		if (!Is(operators[i])) continue;
		const string op = operators[i];
		++position_;
		CPPGMAstNodePtr child = ParseUnaryExpression();
		if (!child) return CPPGMAstNodePtr();
		CPPGMAstNodePtr result = Node("unary-expression", TokenLabel(op) + ":" + op);
		Add(result, child);
		return result;
	}
	if (Is("sizeof") || Is("alignof") || Is("typeid") || Is("noexcept"))
	{
		CPPGMAstNodePtr result = ParseTypeTraitExpression();
		return result ? ParsePostfixSuffix(result) : CPPGMAstNodePtr();
	}
	if (Is("static_cast") || Is("dynamic_cast") || Is("const_cast") ||
		Is("reinterpret_cast")) return ParseKeywordCastExpression();
	if (Is("new") || (Is("::") && Peek(1).text == "new")) return ParseNewExpression();
	if (Is("delete") || (Is("::") && Peek(1).text == "delete"))
		return ParseDeleteExpression();
	if (Is("throw"))
	{
		++position_;
		CPPGMAstNodePtr child;
		if (!Is(";") && !Is(")")) child = ParseAssignmentExpression();
		CPPGMAstNodePtr result = Node("throw-expression");
		Add(result, child);
		return result;
	}
	return ParsePostfixExpression();
}

CPPGMAstNodePtr Parser::ParsePostfixExpression()
{
	CPPGMAstNodePtr result = ParsePrimaryExpression();
	if (!result) return CPPGMAstNodePtr();
	return ParsePostfixSuffix(result);
}

CPPGMAstNodePtr Parser::ParsePostfixSuffix(const CPPGMAstNodePtr& expression)
{
	CPPGMAstNodePtr result = expression;
	while (true)
	{
		if (Is("("))
		{
			const bool builtin = result->kind == "id-expression" &&
				(IsFundamental(result->value) || result->value == "bool");
			CPPGMAstNodePtr call = ParseCallSuffix(result, builtin);
			if (!call) return CPPGMAstNodePtr();
			result = call;
			continue;
		}
		if (Take("["))
		{
			CPPGMAstNodePtr index = ParseExpression();
			if (!index || !Take("]")) return CPPGMAstNodePtr();
			CPPGMAstNodePtr subscript = Node("subscript-expression");
			Add(subscript, result);
			Add(subscript, index);
			result = subscript;
			continue;
		}
		if (Is(".") || Is("->"))
		{
			const string op = Peek().text;
			++position_;
			string member;
			if (!ParseName(&member)) return CPPGMAstNodePtr();
			CPPGMAstNodePtr access = Node("member-expression", TokenLabel(op) + ":" + op);
			Add(access, result);
			Add(access, Node("identifier", member));
			result = access;
			continue;
		}
		if (Is("++") || Is("--"))
		{
			const string op = Peek().text;
			++position_;
			CPPGMAstNodePtr postfix = Node("postfix-expression", TokenLabel(op) + ":" + op);
			Add(postfix, result);
			result = postfix;
			continue;
		}
		if (Take("..."))
		{
			CPPGMAstNodePtr expansion = Node("pack-expansion-expression");
			Add(expansion, result);
			result = expansion;
			continue;
		}
		break;
	}
	return result;
}

CPPGMAstNodePtr Parser::ParseCallSuffix(const CPPGMAstNodePtr& callee,
	bool builtin_style)
{
	if (!Take("(")) return CPPGMAstNodePtr();
	++ordinary_depth_;
	CPPGMAstNodePtr args = Node(builtin_style ? "paren-argument-list" : "argument-list");
	if (!Is(")"))
	{
		CPPGMAstNodePtr argument = ParseInitializerClause();
		if (!argument) return CPPGMAstNodePtr();
		Add(args, argument);
		while (Take(","))
		{
			argument = ParseInitializerClause();
			if (!argument) return CPPGMAstNodePtr();
			Add(args, argument);
		}
	}
	if (!Take(")")) return CPPGMAstNodePtr();
	--ordinary_depth_;
	CPPGMAstNodePtr result = Node("call-expression");
	Add(result, callee);
	Add(result, args);
	return result;
}

CPPGMAstNodePtr Parser::ParsePrimaryExpression()
{
	if (Peek().kind == AST_LITERAL)
	{
		string literal;
		TakeLiteral(&literal);
		return Node("literal", literal);
	}
	if (Is("true") || Is("false") || Is("nullptr") || Is("this"))
	{
		const string keyword = Peek().text;
		++position_;
		return Node("keyword-literal", TokenLabel(keyword) + ":" + keyword);
	}
	if (IsFundamental(Peek().text))
	{
		const string keyword = Peek().text;
		++position_;
		return Node("id-expression", keyword);
	}
	if (Is("["))
	{
		CPPGMAstNodePtr lambda = ParseLambdaExpression();
		if (lambda) return lambda;
	}
	if (Is("{")) return ParseBracedInitList();
	if (Is("("))
	{
		Mark cast_mark = Save();
		++position_;
		++ordinary_depth_;
		const bool cast_type_start = IsFundamental(Peek().text) || IsCv(Peek().text) ||
			Is("class") || Is("struct") || Is("union") || Is("enum") ||
			Is("decltype") || Is("typename") ||
			(Peek().kind == AST_IDENTIFIER && LooksLikeTypeName(Peek()));
		if (cast_type_start)
		{
			CPPGMAstNodePtr type = ParseTypeId();
			if (type && Take(")"))
			{
				CPPGMAstNodePtr child = ParseUnaryExpression();
				if (child)
				{
					--ordinary_depth_;
					CPPGMAstNodePtr result = Node("cast-expression", "OP_LPAREN:");
					Add(result, type);
					Add(result, child);
					return result;
				}
			}
		}
		Restore(cast_mark);
		if (!Take("(")) return CPPGMAstNodePtr();
		++ordinary_depth_;
		CPPGMAstNodePtr expression = ParseExpression();
		if (!expression || !Take(")")) return CPPGMAstNodePtr();
		--ordinary_depth_;
		CPPGMAstNodePtr result = Node("parenthesized-expression");
		Add(result, expression);
		return result;
	}
	return ParseIdExpression();
}

CPPGMAstNodePtr Parser::ParseTypeTraitExpression()
{
	const string keyword = Peek().text;
	++position_;
	if (!Take("(")) return CPPGMAstNodePtr();
	++ordinary_depth_;
	CPPGMAstNodePtr result;
	if (keyword == "sizeof") result = Node("sizeof-expression");
	else result = Node("type-trait-expression", TokenLabel(keyword) + ":" + keyword);
	Mark type_mark = Save();
	bool use_type = IsTypeStart();
	if (use_type && Peek().kind == AST_IDENTIFIER && Peek(1).text == "(")
		use_type = false;
	if (use_type)
	{
		CPPGMAstNodePtr type = ParseTypeId();
		if (type && Take(")"))
		{
			--ordinary_depth_;
			Add(result, type);
			return result;
		}
	}
	Restore(type_mark);
	CPPGMAstNodePtr expression = ParseExpression();
	if (!expression || !Take(")")) return CPPGMAstNodePtr();
	--ordinary_depth_;
	Add(result, expression);
	return result;
}

CPPGMAstNodePtr Parser::ParseKeywordCastExpression()
{
	const string keyword = Peek().text;
	++position_;
	if (!Take("<")) return CPPGMAstNodePtr();
	EnterAngle();
	CPPGMAstNodePtr type = ParseTypeId();
	if (!type || !TakeCloseAngle() || !Take("(")) return CPPGMAstNodePtr();
	++ordinary_depth_;
	CPPGMAstNodePtr expression = ParseExpression();
	if (!expression || !Take(")")) return CPPGMAstNodePtr();
	--ordinary_depth_;
	LeaveAngle();
	CPPGMAstNodePtr result = Node("cast-expression", TokenLabel(keyword) + ":" + keyword);
	Add(result, type);
	Add(result, expression);
	return result;
}

CPPGMAstNodePtr Parser::ParseLambdaExpression()
{
	Mark mark = Save();
	if (!Take("[")) return CPPGMAstNodePtr();
	++ordinary_depth_;
	CPPGMAstNodePtr introducer = Node("lambda-introducer", "[]");
	if (!Is("]"))
	{
		if (Is("&") || Is("=")) ++position_;
		while (true)
		{
			if (Is("&")) ++position_;
			if (!TakeIdentifier())
			{
				Restore(mark);
				return CPPGMAstNodePtr();
			}
			Take("...");
			if (!Take(",")) break;
		}
	}
	if (!Take("]")) { Restore(mark); return CPPGMAstNodePtr(); }
	--ordinary_depth_;
	CPPGMAstNodePtr result = Node("lambda-expression");
	Add(result, introducer);
	if (Is("("))
	{
		CPPGMAstNodePtr declarator = Node("lambda-declarator");
		CPPGMAstNodePtr parameters = ParseParameterClause();
		if (!parameters) { Restore(mark); return CPPGMAstNodePtr(); }
		Add(declarator, parameters);
		if (Take("mutable")) Add(declarator, Node("lambda-specifier", TokenLabel("mutable") + ":mutable"));
		if (Take("noexcept"))
		{
			CPPGMAstNodePtr spec = Node("noexcept-specification");
			if (Take("("))
			{
				CPPGMAstNodePtr expression = ParseExpression();
				if (!expression || !Take(")")) { Restore(mark); return CPPGMAstNodePtr(); }
				Add(spec, expression);
			}
			Add(declarator, spec);
		}
		if (Take("->"))
		{
			CPPGMAstNodePtr type = ParseTypeId();
			if (!type) { Restore(mark); return CPPGMAstNodePtr(); }
			CPPGMAstNodePtr trailing = Node("trailing-return-type");
			Add(trailing, type);
			Add(declarator, trailing);
		}
		Add(result, declarator);
	}
	CPPGMAstNodePtr body = ParseCompoundStatement();
	if (!body) { Restore(mark); return CPPGMAstNodePtr(); }
	Add(result, body);
	return result;
}

CPPGMAstNodePtr Parser::ParseNewExpression()
{
	Mark mark = Save();
	CPPGMAstNodePtr result = Node("new-expression");
	if (Take("::")) Add(result, Node("global-scope"));
	if (!Take("new")) { Restore(mark); return CPPGMAstNodePtr(); }
	if (Is("("))
	{
		const size_t begin = position_;
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
		string raw;
		for (size_t i = begin; i < position_; ++i) raw += tokens_[i].text;
		CPPGMAstNodePtr placement = Node("placement", raw);
		Add(placement, args);
		Add(result, placement);
	}
	Mark type_mark = Save();
	CPPGMAstNodePtr type_specs = ParseTypeSpecifierSeq();
	if (!type_specs)
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	CPPGMAstNodePtr type = Node("type-id");
	Add(type, type_specs);
	if (Is("*") || Is("&") || Is("&&") || Is("["))
	{
		CPPGMAstNodePtr abstract = ParseAbstractDeclarator();
		if (!abstract)
		{
			Restore(type_mark);
			return CPPGMAstNodePtr();
		}
		Add(type, abstract);
	}
	Add(result, type);
	if (Is("("))
	{
		++position_;
		++ordinary_depth_;
		CPPGMAstNodePtr initializer = Node("initializer");
		CPPGMAstNodePtr paren = Node("paren-initializer");
		if (!Is(")"))
		{
			CPPGMAstNodePtr clause = ParseInitializerClause();
			if (!clause) { Restore(mark); return CPPGMAstNodePtr(); }
			Add(paren, clause);
			while (Take(","))
			{
				clause = ParseInitializerClause();
				if (!clause) { Restore(mark); return CPPGMAstNodePtr(); }
				Add(paren, clause);
			}
		}
		if (!Take(")")) { Restore(mark); return CPPGMAstNodePtr(); }
		--ordinary_depth_;
		Add(initializer, paren);
		Add(result, initializer);
	}
	else if (Is("{"))
	{
		CPPGMAstNodePtr initializer = ParseBracedInitList();
		if (!initializer) { Restore(mark); return CPPGMAstNodePtr(); }
		Add(result, initializer);
	}
	return result;
}

CPPGMAstNodePtr Parser::ParseDeleteExpression()
{
	Mark mark = Save();
	CPPGMAstNodePtr result = Node("delete-expression");
	if (Take("::")) Add(result, Node("global-scope"));
	if (!Take("delete")) { Restore(mark); return CPPGMAstNodePtr(); }
	if (Take("["))
	{
		if (!Take("]")) { Restore(mark); return CPPGMAstNodePtr(); }
		Add(result, Node("array-delete"));
	}
	CPPGMAstNodePtr expression = ParseUnaryExpression();
	if (!expression) { Restore(mark); return CPPGMAstNodePtr(); }
	Add(result, expression);
	return result;
}

} // namespace cppgm_pa10
