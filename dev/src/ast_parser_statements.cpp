#include "ast_parser.h"

using namespace std;

namespace cppgm_pa10 {

CPPGMAstNodePtr Parser::ParseCompoundStatement()
{
	Mark mark = Save();
	if (!Take("{")) return CPPGMAstNodePtr();
	CPPGMAstNodePtr result = Node("compound-statement");
	++ordinary_depth_;
	while (!Is("}") && !AtEnd())
	{
		CPPGMAstNodePtr statement = ParseStatement();
		if (!statement)
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		Add(result, statement);
	}
	if (!Take("}"))
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	--ordinary_depth_;
	return result;
}

CPPGMAstNodePtr Parser::ParseStatement()
{
	Mark mark = Save();
	CPPGMAstNodePtr result = ParseLabeledStatement();
	if (result) return result;
	Restore(mark);
	result = ParseSelectionStatement();
	if (result) return result;
	Restore(mark);
	result = ParseIterationStatement();
	if (result) return result;
	Restore(mark);
	result = ParseJumpStatement();
	if (result) return result;
	Restore(mark);
	result = ParseTryBlock();
	if (result) return result;
	Restore(mark);
	result = ParseCompoundStatement();
	if (result) return result;
	Restore(mark);
	bool declaration_start = IsTypeStart();
	// A template-name can also name a member function.  When it is followed
	// directly by a call parenthesis, keep the expression form available so
	// semantic lookup can prefer that function over the type declaration.
	if(declaration_start && Peek().kind == AST_IDENTIFIER &&
	   (Peek().names.template_name || templates_.find(Peek().text) != templates_.end()) &&
	   Peek(1).text == "(")
		declaration_start = false;
	if (Peek().kind == AST_IDENTIFIER &&
		(Peek(1).text == "::" || Peek(1).text == "<"))
	{
		Mark qualified = Save();
		string name;
		if (ParseName(&name) && Is("(")) declaration_start = false;
		Restore(qualified);
	}
	if ((declaration_start && !(Peek().kind == AST_IDENTIFIER && Peek(1).text == "(" &&
		Peek(2).text == "&")) || Is("static_assert") || Is("using") || Is("class") ||
		Is("struct") || Is("union") || Is("enum") || Is("typedef") || Is("namespace"))
	{
		result = ParseDeclaration(false);
		if (result) return result;
		Restore(mark);
	}
	result = ParseExpressionStatement();
	if (result) return result;
	Restore(mark);
	return CPPGMAstNodePtr();
}

CPPGMAstNodePtr Parser::ParseLabeledStatement()
{
	Mark mark = Save();
	if (Peek().kind == AST_IDENTIFIER && Peek(1).text == ":")
	{
		string name;
		TakeIdentifier(&name);
		Take(":");
		CPPGMAstNodePtr statement = ParseStatement();
		if (!statement)
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		CPPGMAstNodePtr result = Node("labeled-statement", name);
		Add(result, statement);
		return result;
	}
	if (Take("case"))
	{
		CPPGMAstNodePtr value = ParseExpression();
		if (!value || !Take(":")) { Restore(mark); return CPPGMAstNodePtr(); }
		CPPGMAstNodePtr statement = ParseStatement();
		if (!statement) { Restore(mark); return CPPGMAstNodePtr(); }
		CPPGMAstNodePtr result = Node("case-statement");
		Add(result, value);
		Add(result, statement);
		return result;
	}
	Restore(mark);
	if (Take("default"))
	{
		if (!Take(":")) { Restore(mark); return CPPGMAstNodePtr(); }
		CPPGMAstNodePtr statement = ParseStatement();
		if (!statement) { Restore(mark); return CPPGMAstNodePtr(); }
		CPPGMAstNodePtr result = Node("default-statement");
		Add(result, statement);
		return result;
	}
	Restore(mark);
	return CPPGMAstNodePtr();
}

CPPGMAstNodePtr Parser::ParseExpressionStatement()
{
	Mark mark = Save();
	CPPGMAstNodePtr result = Node("expression-statement");
	if (!Is(";"))
	{
		CPPGMAstNodePtr expression = ParseExpression();
		if (!expression)
		{
			Restore(mark);
			return CPPGMAstNodePtr();
		}
		Add(result, expression);
	}
	if (!Take(";"))
	{
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	return result;
}

CPPGMAstNodePtr Parser::ParseSelectionStatement()
{
	Mark mark = Save();
	if (Take("if"))
	{
		if (!Take("(")) { Restore(mark); return CPPGMAstNodePtr(); }
		++ordinary_depth_;
		CPPGMAstNodePtr condition = ParseCondition();
		if (!condition || !Take(")")) { Restore(mark); return CPPGMAstNodePtr(); }
		--ordinary_depth_;
		CPPGMAstNodePtr then_statement = ParseStatement();
		if (!then_statement) { Restore(mark); return CPPGMAstNodePtr(); }
		CPPGMAstNodePtr result = Node("if-statement");
		Add(result, Node("condition"));
		result->children.back()->children.push_back(condition);
		CPPGMAstNodePtr then_node = Node("then");
		Add(then_node, then_statement);
		Add(result, then_node);
		if (Take("else"))
		{
			CPPGMAstNodePtr otherwise = ParseStatement();
			if (!otherwise) { Restore(mark); return CPPGMAstNodePtr(); }
			CPPGMAstNodePtr else_node = Node("else");
			Add(else_node, otherwise);
			Add(result, else_node);
		}
		return result;
	}
	Restore(mark);
	if (Take("switch"))
	{
		if (!Take("(")) { Restore(mark); return CPPGMAstNodePtr(); }
		++ordinary_depth_;
		CPPGMAstNodePtr condition = ParseCondition();
		if (!condition || !Take(")")) { Restore(mark); return CPPGMAstNodePtr(); }
		--ordinary_depth_;
		CPPGMAstNodePtr statement = ParseStatement();
		if (!statement) { Restore(mark); return CPPGMAstNodePtr(); }
		CPPGMAstNodePtr result = Node("switch-statement");
		CPPGMAstNodePtr condition_node = Node("condition");
		Add(condition_node, condition);
		Add(result, condition_node);
		Add(result, statement);
		return result;
	}
	Restore(mark);
	return CPPGMAstNodePtr();
}

CPPGMAstNodePtr Parser::ParseCondition()
{
	Mark mark = Save();
	if (IsTypeStart())
	{
		CPPGMAstNodePtr specs = ParseDeclSpecifierSeq(false);
		if (specs)
		{
			CPPGMAstNodePtr declarator = ParseDeclarator(false);
			if (declarator)
			{
				CPPGMAstNodePtr initializer = ParseInitializer();
				if (initializer)
				{
					CPPGMAstNodePtr declaration = Node("condition-declaration");
					Add(declaration, specs);
					Add(declaration, declarator);
					Add(declaration, initializer);
					return declaration;
				}
			}
		}
	}
	Restore(mark);
	return ParseExpression();
}

CPPGMAstNodePtr Parser::ParseIterationStatement()
{
	Mark mark = Save();
	if (Take("while"))
	{
		if (!Take("(")) { Restore(mark); return CPPGMAstNodePtr(); }
		++ordinary_depth_;
		CPPGMAstNodePtr condition = ParseCondition();
		if (!condition || !Take(")")) { Restore(mark); return CPPGMAstNodePtr(); }
		--ordinary_depth_;
		CPPGMAstNodePtr body = ParseStatement();
		if (!body) { Restore(mark); return CPPGMAstNodePtr(); }
		CPPGMAstNodePtr result = Node("while-statement");
		CPPGMAstNodePtr condition_node = Node("condition");
		Add(condition_node, condition);
		Add(result, condition_node);
		Add(result, body);
		return result;
	}
	Restore(mark);
	if (Take("do"))
	{
		CPPGMAstNodePtr body = ParseStatement();
		if (!body || !Take("while") || !Take("(")) { Restore(mark); return CPPGMAstNodePtr(); }
		++ordinary_depth_;
		CPPGMAstNodePtr condition = ParseExpression();
		if (!condition || !Take(")") || !Take(";")) { Restore(mark); return CPPGMAstNodePtr(); }
		--ordinary_depth_;
		CPPGMAstNodePtr result = Node("do-statement");
		Add(result, body);
		CPPGMAstNodePtr condition_node = Node("condition");
		Add(condition_node, condition);
		Add(result, condition_node);
		return result;
	}
	Restore(mark);
	if (Take("for"))
	{
		if (!Take("(")) { Restore(mark); return CPPGMAstNodePtr(); }
		++ordinary_depth_;
		CPPGMAstNodePtr for_init = Node("for-init-statement");
		if (IsTypeStart())
		{
			CPPGMAstNodePtr declaration = ParseSimpleOrFunctionDeclaration(false);
			if (!declaration) { Restore(mark); return CPPGMAstNodePtr(); }
			Add(for_init, declaration);
		}
		else if (!Is(";"))
		{
			CPPGMAstNodePtr expression = ParseExpression();
			if (!expression || !Take(";")) { Restore(mark); return CPPGMAstNodePtr(); }
			CPPGMAstNodePtr statement = Node("expression-statement");
			Add(statement, expression);
			Add(for_init, statement);
		}
		else Take(";");
		CPPGMAstNodePtr result = Node("for-statement");
		Add(result, for_init);
		if (!Is(";"))
		{
			CPPGMAstNodePtr condition = ParseCondition();
			if (!condition) { Restore(mark); return CPPGMAstNodePtr(); }
			CPPGMAstNodePtr condition_node = Node("condition");
			Add(condition_node, condition);
			Add(result, condition_node);
		}
		if (!Take(";")) { Restore(mark); return CPPGMAstNodePtr(); }
		if (!Is(")"))
		{
			CPPGMAstNodePtr iteration = ParseExpression();
			if (!iteration) { Restore(mark); return CPPGMAstNodePtr(); }
			Add(result, Node("iteration"));
			result->children.back()->children.push_back(iteration);
		}
		if (!Take(")")) { Restore(mark); return CPPGMAstNodePtr(); }
		--ordinary_depth_;
		CPPGMAstNodePtr body = ParseStatement();
		if (!body) { Restore(mark); return CPPGMAstNodePtr(); }
		Add(result, body);
		return result;
	}
	Restore(mark);
	return CPPGMAstNodePtr();
}

CPPGMAstNodePtr Parser::ParseJumpStatement()
{
	Mark mark = Save();
	if (Take("break"))
	{
		if (!Take(";")) { Restore(mark); return CPPGMAstNodePtr(); }
		return Node("break-statement");
	}
	Restore(mark);
	if (Take("continue"))
	{
		if (!Take(";")) { Restore(mark); return CPPGMAstNodePtr(); }
		return Node("continue-statement");
	}
	Restore(mark);
	if (Take("goto"))
	{
		string name;
		if (!TakeIdentifier(&name) || !Take(";")) { Restore(mark); return CPPGMAstNodePtr(); }
		return Node("goto-statement", name);
	}
	Restore(mark);
	if (Take("return"))
	{
		CPPGMAstNodePtr result = Node("return-statement");
		if (!Is(";"))
		{
			CPPGMAstNodePtr expression = ParseExpression();
			if (!expression) { Restore(mark); return CPPGMAstNodePtr(); }
			Add(result, expression);
		}
		if (!Take(";")) { Restore(mark); return CPPGMAstNodePtr(); }
		return result;
	}
	Restore(mark);
	if (Take("throw"))
	{
		CPPGMAstNodePtr result = Node("throw-statement");
		if (!Is(";"))
		{
			CPPGMAstNodePtr expression = ParseAssignmentExpression();
			if (!expression) { Restore(mark); return CPPGMAstNodePtr(); }
			Add(result, expression);
		}
		if (!Take(";")) { Restore(mark); return CPPGMAstNodePtr(); }
		return result;
	}
	Restore(mark);
	return CPPGMAstNodePtr();
}

CPPGMAstNodePtr Parser::ParseTryBlock()
{
	Mark mark = Save();
	if (!Take("try")) return CPPGMAstNodePtr();
	CPPGMAstNodePtr body = ParseCompoundStatement();
	if (!body) { Restore(mark); return CPPGMAstNodePtr(); }
	CPPGMAstNodePtr result = Node("try-block");
	Add(result, body);
	CPPGMAstNodePtr handler = ParseHandler();
	if (!handler) { Restore(mark); return CPPGMAstNodePtr(); }
	Add(result, handler);
	while (true)
	{
		Mark next = Save();
		handler = ParseHandler();
		if (!handler) { Restore(next); break; }
		Add(result, handler);
	}
	return result;
}

CPPGMAstNodePtr Parser::ParseHandler()
{
	Mark mark = Save();
	if (!Take("catch") || !Take("(")) { Restore(mark); return CPPGMAstNodePtr(); }
	++ordinary_depth_;
	CPPGMAstNodePtr exception = ParseExceptionDeclaration();
	if (!exception || !Take(")")) { Restore(mark); return CPPGMAstNodePtr(); }
	--ordinary_depth_;
	CPPGMAstNodePtr body = ParseCompoundStatement();
	if (!body) { Restore(mark); return CPPGMAstNodePtr(); }
	CPPGMAstNodePtr result = Node("handler");
	Add(result, exception);
	Add(result, body);
	return result;
}

CPPGMAstNodePtr Parser::ParseExceptionDeclaration()
{
	if (Take("..."))
	{
		CPPGMAstNodePtr result = Node("exception-declaration");
		Add(result, Node("ellipsis", "..."));
		return result;
	}
	Mark mark = Save();
	CPPGMAstNodePtr specs = ParseDeclSpecifierSeq(false);
	if (!specs) { Restore(mark); return CPPGMAstNodePtr(); }
	CPPGMAstNodePtr declarator;
	Mark declarator_mark = Save();
	if (!Is(")"))
	{
		declarator = ParseDeclarator(true);
		if (!declarator) Restore(declarator_mark);
	}
	CPPGMAstNodePtr result = Node("exception-declaration");
	Add(result, specs);
	Add(result, declarator);
	return result;
}

} // namespace cppgm_pa10
