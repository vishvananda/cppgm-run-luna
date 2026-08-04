#include "ast_parser.h"

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

bool OrdinaryStringLiteral(const string& text)
{
	const size_t quote = text.find('"');
	return quote != string::npos && !text.empty() && text[text.size() - 1] == '"' &&
		(quote == 0 || (quote == 1 && (text[0] == 'u' || text[0] == 'U' || text[0] == 'L')) ||
		 (quote == 2 && text.compare(0, 2, "u8") == 0));
}

string ConcatenateStringLiterals(const string& left, const string& right)
{
	const size_t left_end = left.rfind('"');
	const size_t right_begin = right.find('"');
	if(left_end == string::npos || right_begin == string::npos) return left;
	return left.substr(0, left_end) + right.substr(right_begin + 1);
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
			// `>=` is a single comparison token and cannot close a template
			// argument list.  Only a standalone `>` is ambiguous here.
			if (Is(">") && CloseAngleBlocked()) break;
			op = Peek().text;
		}
		else if (level == 7 && Is("<<")) op = Peek().text;
		else if (level == 7 && Peek().kind == AST_RSHIFT_1 &&
			Peek(1).kind == AST_RSHIFT_2 && !CloseAngleBlocked())
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
		Is("reinterpret_cast"))
	{
		CPPGMAstNodePtr cast = ParseKeywordCastExpression();
		return cast ? ParsePostfixSuffix(cast) : CPPGMAstNodePtr();
	}
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
		if (Is("{") && result->kind == "id-expression")
		{
			CPPGMAstNodePtr list = ParseBracedInitList();
			if (!list) return CPPGMAstNodePtr();
			CPPGMAstNodePtr arguments = Node("argument-list");
			Add(arguments, list);
			CPPGMAstNodePtr call = Node("call-expression");
			call->value = "braced-construction";
			Add(call, result);
			Add(call, arguments);
			result = call;
			continue;
		}
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

bool Parser::LooksLikeNamedCastType()
{
	if (Peek().kind != AST_IDENTIFIER || value_names_.find(Peek().text) != value_names_.end() ||
		!IsNamedTypeStart()) return false;
	if (LooksLikeTypeName(Peek())) return true;
	Mark name_mark = Save();
	string name;
	bool result = false;
	// A C-style cast may name a qualified class template, such as
	// `(boost::intrusive::algo_types)5`.  The non-template probe used to stop at
	// the first `<`, misclassify the cast as a parenthesized expression, and
	// leave the following typedef at the translation-unit boundary.
	if (ParseName(&name, false, true)) {
		Mark close_mark = Save();
		const bool qualified_cast = Take(")") && !Is("(");
		Restore(close_mark);
		result = name.find('<') != string::npos || types_.find(name) != types_.end() ||
			templates_.find(name) != templates_.end() || qualified_cast ||
			Is("*") || Is("&") ||
			Is("&&") || Is("[") || IsCv(Peek().text);
	}
	Restore(name_mark);
	return result;
}

CPPGMAstNodePtr Parser::ParseDependentTypeConstruction()
{
	if(!Is("typename")) return CPPGMAstNodePtr();
	Mark typename_mark = Save();
	++position_;
	string type_name;
	if (ParseName(&type_name, false)) {
		if (Is("(")) {
			CPPGMAstNodePtr call = ParseCallSuffix(Node("id-expression", type_name), false);
			if(call) return call;
		}
		if (Is("{")) {
			CPPGMAstNodePtr list = ParseBracedInitList();
			if (list) {
				CPPGMAstNodePtr arguments = Node("argument-list");
				Add(arguments, list);
				CPPGMAstNodePtr call = Node("call-expression", "braced-construction");
				Add(call, Node("id-expression", type_name));
				Add(call, arguments);
				return call;
			}
		}
	}
	Restore(typename_mark);
	return CPPGMAstNodePtr();
}

CPPGMAstNodePtr Parser::ParseAliasFunctionalCast()
{
	if (Peek().kind != AST_IDENTIFIER || !IsNamedTypeStart()) return CPPGMAstNodePtr();
	Mark mark = Save();
	string name;
	if (!ParseName(&name) || !Is("(")) {
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	string base = name;
	const size_t open = base.find('<');
	if (open != string::npos) base.erase(open);
	const size_t separator = base.rfind("::");
	if (separator != string::npos) base.erase(0, separator + 2);
	if (alias_templates_.find(base) == alias_templates_.end()) {
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	Restore(mark);
	CPPGMAstNodePtr type = ParseTypeId();
	if (!type || !Take("(")) {
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	++ordinary_depth_;
	CPPGMAstNodePtr argument;
	if (!Is(")")) argument = ParseExpression();
	if (!Take(")")) {
		--ordinary_depth_;
		Restore(mark);
		return CPPGMAstNodePtr();
	}
	--ordinary_depth_;
	CPPGMAstNodePtr result = Node("cast-expression");
	Add(result, type);
	Add(result, argument);
	return result;
}

CPPGMAstNodePtr Parser::ParsePrimaryExpression()
{
	if(Is("typename")) {
		CPPGMAstNodePtr dependent = ParseDependentTypeConstruction();
		if(dependent) return dependent;
	}
	if (Peek().kind == AST_LITERAL)
	{
		string literal;
		TakeLiteral(&literal);
		if (OrdinaryStringLiteral(literal)) {
			while (Peek().kind == AST_LITERAL && OrdinaryStringLiteral(Peek().text)) {
				string adjacent;
				TakeLiteral(&adjacent);
				literal = ConcatenateStringLiterals(literal, adjacent);
			}
		}
		return Node("literal", literal);
	}
	if (Is("true") || Is("false") || Is("nullptr") || Is("this"))
	{
		const string keyword = Peek().text;
		++position_;
		return Node("keyword-literal", TokenLabel(keyword) + ":" + keyword);
	}
	if (Is("decltype"))
	{
		Mark decltype_mark = Save();
		CPPGMAstNodePtr type = ParseDecltypeSpecifier();
		if (type && Take("("))
		{
			++ordinary_depth_;
			CPPGMAstNodePtr argument;
			if (!Is(")")) argument = ParseInitializerClause();
			if (Take(")"))
			{
				--ordinary_depth_;
				CPPGMAstNodePtr arguments = Node("argument-list");
				Add(arguments, argument);
				CPPGMAstNodePtr result = Node("call-expression");
				Add(result, type);
				Add(result, arguments);
				return result;
			}
			--ordinary_depth_;
		}
		Restore(decltype_mark);
	}
	if (IsFundamental(Peek().text))
	{
		// A multi-word fundamental type followed by parentheses is a
		// functional cast.  The ordinary one-word path below deliberately
		// remains a call-shaped node because the semantic printer also uses
		// that form for aliases and single-word fundamental casts.
		Mark functional_mark = Save();
		CPPGMAstNodePtr functional_type = ParseTypeId();
		if (functional_type && !functional_type->children.empty() &&
			functional_type->children[0] &&
			functional_type->children[0]->children.size() > 1 && Take("("))
		{
			++ordinary_depth_;
			CPPGMAstNodePtr argument;
			if (!Is(")")) argument = ParseExpression();
			if (Take(")"))
			{
				--ordinary_depth_;
				CPPGMAstNodePtr result = Node("cast-expression");
				Add(result, functional_type);
				Add(result, argument);
				return result;
			}
			--ordinary_depth_;
		}
		Restore(functional_mark);
		const string keyword = Peek().text;
		++position_;
		return Node("id-expression", keyword);
	}
	CPPGMAstNodePtr alias_cast = ParseAliasFunctionalCast(); if (alias_cast) return alias_cast;
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
			Is("decltype") || Is("typename") || LooksLikeNamedCastType();
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
	if (keyword == "sizeof" && Is("..."))
	{
		++position_;
		if (!Take("(")) return CPPGMAstNodePtr();
		CPPGMAstNodePtr pack = ParseIdExpression();
		if (!pack || !Take(")")) return CPPGMAstNodePtr();
		CPPGMAstNodePtr result = Node("sizeof-pack-expression");
		Add(result, pack);
		return result;
	}
	if (keyword == "sizeof" && !Is("("))
	{
		CPPGMAstNodePtr result = Node("sizeof-expression");
		CPPGMAstNodePtr expression = ParseUnaryExpression();
		if (!expression) return CPPGMAstNodePtr();
		Add(result, expression);
		return result;
	}
	if (!Take("(")) return CPPGMAstNodePtr();
	++ordinary_depth_;
	CPPGMAstNodePtr result;
	if (keyword == "sizeof") result = Node("sizeof-expression");
	else result = Node("type-trait-expression", TokenLabel(keyword) + ":" + keyword);
	Mark type_mark = Save();
	bool use_type = IsTypeStart();
	if (use_type && Peek().kind == AST_IDENTIFIER)
	{
		// A qualified callable such as `detail::helper(...)` is lexically also
		// a named type start.  Probe the complete name before deciding that the
		// operand is a type; the immediate-token check misses the intervening
		// scope separators and makes the enclosing template declaration fail to
		// parse.
		Mark call_mark = Save();
		string candidate;
		if (ParseName(&candidate, false) && Is("(")) use_type = false;
		Restore(call_mark);
	}
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
	if (!expression || !Take(")")) {
		return CPPGMAstNodePtr();
	}
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
		CPPGMAstNodePtr captures = Node("lambda-capture");
		bool has_default = false;
		const bool starts_default = Is("=") ||
			(Is("&") && (Peek(1).text == "]" || Peek(1).text == ","));
		if (starts_default)
		{
			const string text = Peek().text;
			++position_;
			Add(captures, Node("capture-default", TokenLabel(text) + ":" + text));
			has_default = true;
			if (!Take(","))
			{
				if (!Is("]"))
				{
					Restore(mark);
					return CPPGMAstNodePtr();
				}
			}
			else if (Is("]"))
			{
				Restore(mark);
				return CPPGMAstNodePtr();
			}
		}
		if (!has_default || !Is("]"))
		{
			CPPGMAstNodePtr list = Node("capture-list");
			while (true)
			{
				CPPGMAstNodePtr capture;
				if (Take("this"))
					capture = Node("capture", TokenLabel("this") + ":this");
				else
				{
					const bool by_reference = Take("&");
					string name;
					if (!TakeIdentifier(&name))
					{
						Restore(mark);
						return CPPGMAstNodePtr();
					}
					capture = Node("capture", name);
					if (by_reference)
						Add(capture, Node("capture-reference", TokenLabel("&") + ":&"));
				}
				if (Take("...")) Add(capture, Node("parameter-pack", "..."));
				Add(list, capture);
				if (!Take(",")) break;
				if (Is("]"))
				{
					Restore(mark);
					return CPPGMAstNodePtr();
				}
			}
			Add(captures, list);
		}
		Add(introducer, captures);
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

CPPGMAstNodePtr Parser::ParseNewInitializer()
{
	if (Is("("))
	{
		++position_;
		++ordinary_depth_;
		CPPGMAstNodePtr initializer = Node("initializer");
		CPPGMAstNodePtr paren = Node("paren-initializer");
		if (!Is(")"))
		{
			CPPGMAstNodePtr clause = ParseInitializerClause();
			if (!clause) return CPPGMAstNodePtr();
			Add(paren, clause);
			while (Take(","))
			{
				clause = ParseInitializerClause();
				if (!clause) return CPPGMAstNodePtr();
				Add(paren, clause);
			}
		}
		if (!Take(")")) return CPPGMAstNodePtr();
		--ordinary_depth_;
		Add(initializer, paren);
		return initializer;
	}
	if (Is("{")) return ParseBracedInitList();
	return CPPGMAstNodePtr();
}

CPPGMAstNodePtr Parser::ParseNewExpression()
{
	Mark mark = Save();
	CPPGMAstNodePtr result = Node("new-expression");
	if (Take("::")) Add(result, Node("global-scope"));
	if (!Take("new")) { Restore(mark); return CPPGMAstNodePtr(); }
	// The grammar has a second new-expression form whose first parenthesized
	// group is a type-id rather than placement arguments.  Try that form only
	// when the token after the group can be a new-initializer or an enclosing
	// expression delimiter; an identifier-led group followed by a type remains
	// the ordinary placement-new form.
	if (Is("("))
	{
		const Mark type_form_mark = Save();
		++position_;
		++ordinary_depth_;
		CPPGMAstNodePtr parenthesized_type = ParseTypeId();
		if (parenthesized_type && Take(")"))
		{
			const bool delimiter_after_type =
				Is("(") || Is("{") || Is(";") || Is(")") || Is("]") ||
				Is(",") || Is("}") || Is(":") || Is("?") || AtEnd();
			if (delimiter_after_type)
			{
				--ordinary_depth_;
				Add(result, parenthesized_type);
				if (Is("(") || Is("{"))
				{
					CPPGMAstNodePtr initializer = ParseNewInitializer();
					if (!initializer) { Restore(mark); return CPPGMAstNodePtr(); }
					Add(result, initializer);
				}
				return result;
			}
		}
		Restore(type_form_mark);
	}
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
	if (Is("(") || Is("{"))
	{
		CPPGMAstNodePtr initializer = ParseNewInitializer();
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
