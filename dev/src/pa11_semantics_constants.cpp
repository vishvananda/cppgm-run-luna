#include "pa11_semantics_analyzer.h"
#include <cstdlib>

namespace {

CPPGMAstNodePtr FindReturnStatement(const CPPGMAstNodePtr& node)
{
	if (!node) return CPPGMAstNodePtr();
	if (node->kind == "return-statement") return node;
	for (size_t i = 0; i < node->children.size(); ++i) {
		CPPGMAstNodePtr found = FindReturnStatement(node->children[i]);
		if (found) return found;
	}
	return CPPGMAstNodePtr();
}

}

ConstantValue Analyzer::Evaluate(const CPPGMAstNodePtr& expression, Scope* scope)
{
	if (!expression) return ConstantValue();
	if (expression->kind == "literal") return FromIntegralValue(ParseLiteralValue(expression->value));
	if (expression->kind == "keyword-literal")
		return FromIntegralValue(PA19IntegralValue::Signed(
			OperatorFromNode(expression->value) == "true" ? 1 : 0, "bool", 1));
	if (expression->kind == "id-expression")
	{
		Binding* binding = ResolveBinding(scope, expression->value);
		return !binding || !binding->constant_value.known ? ConstantValue() :
			FromIntegralValue(binding->constant_value);
	}
	if (expression->kind == "parenthesized-expression")
		return expression->children.empty() ? ConstantValue() : Evaluate(expression->children[0], scope);
	if (expression->kind == "initializer" || expression->kind == "paren-initializer" ||
		expression->kind == "initializer-clause")
		return expression->children.empty() ? ConstantValue() : Evaluate(expression->children[0], scope);
	if (expression->kind == "braced-init-list" && expression->children.size() == 1)
		return Evaluate(expression->children[0], scope);
	if (expression->kind == "subscript-expression" && expression->children.size() >= 2 &&
		expression->children[0] && expression->children[0]->kind == "literal") {
		ConstantValue index = Evaluate(expression->children[1], scope);
		if (!index.integral.known) return ConstantValue();
		ostringstream spelling;
		spelling << expression->children[0]->value << "[" << PA19Raw(ToIntegralValue(index)) << "]";
		map<string, PA19IntegralValue> constants;
		map<string, string> substitutions;
		PA19ConstantExpressionParser parser(constants, substitutions);
		PA19IntegralValue value;
		return parser.Evaluate(spelling.str(), &value) ? FromIntegralValue(value) : ConstantValue();
	}
	if (expression->kind == "sizeof-pack-expression")
	{
		const long long count = expression->value.empty() ? 0 : atoll(expression->value.c_str());
		return FromIntegralValue(PA19IntegralValue::Unsigned(
			static_cast<unsigned long long>(count), "unsigned long", 64));
	}
	if (expression->kind == "sizeof-expression" || expression->kind == "type-trait-expression")
	{
		if (expression->children.empty()) return ConstantValue();
		const CPPGMAstNodePtr child = expression->children[0];
		TypePtr type;
		if (child->kind == "type-id") {
			// `sizeof(alias::member)` is parsed as a type-id because the
			// qualified spelling is type-shaped.  If its terminal binding is a
			// variable/function rather than a type, use that expression's type.
			string spelling;
			if (!child->children.empty() && child->children[0])
				for (size_t specifier = 0;
					specifier < child->children[0]->children.size(); ++specifier) {
					const CPPGMAstNodePtr node = child->children[0]->children[specifier];
					if (node && (node->kind == "type-name" ||
						node->kind == "decl-specifier" || node->kind == "type-specifier")) {
						spelling = StripTypeMarker(node->value);
						break;
					}
				}
			Binding* expression_binding = spelling.empty() ? 0 :
				ResolveBinding(scope, spelling);
			if (expression_binding && expression_binding->kind != BIND_TYPE &&
				expression_binding->kind != BIND_TYPE_ALIAS)
				type = expression_binding->type;
			else type = TypeFromTypeId(child, scope);
		} else type = ExpressionType(child, scope);
		const bool align = expression->kind == "type-trait-expression";
		return FromIntegralValue(PA19IntegralValue::Unsigned(
			static_cast<unsigned long long>(align ? TypeAlignment(type) : TypeSize(type)),
			"unsigned long", 64));
	}
	if (expression->kind == "cast-expression")
	{
		if (expression->children.size() < 2) return ConstantValue();
		const ConstantValue operand = Evaluate(expression->children[1], scope);
		if (!operand.integral.known) return ConstantValue();
		const TypePtr target = TypeFromTypeId(expression->children[0], scope);
		return FromIntegralValue(PA19Convert(ToIntegralValue(operand), PA19Type(TypeText(target, true))));
	}
	if (expression->kind == "call-expression" && expression->children.size() >= 2 &&
		expression->children[0] && expression->children[0]->kind == "id-expression" &&
		expression->children[1] &&
		(expression->children[1]->kind == "paren-argument-list" ||
			expression->children[1]->kind == "argument-list")) {
		const string& callee_name = expression->children[0]->value;
		const PA19IntegralType target = PA19Type(callee_name);
		if (target.integral && !expression->children[1]->children.empty()) {
			const ConstantValue operand = Evaluate(expression->children[1]->children[0], scope);
			return operand.integral.known ? FromIntegralValue(PA19Convert(ToIntegralValue(operand), target)) : ConstantValue();
		}
		if (!target.integral && expression->children[1]->children.empty()) {
			Binding* function = ResolveBinding(scope, callee_name);
			if (!function || function->kind != BIND_FUNCTION || !function->declaration ||
				!HasNodeValue(function->declaration, "decl-specifier", "constexpr"))
				return ConstantValue();
			const CPPGMAstNodePtr body = ChildOfKind(function->declaration, "compound-statement");
			if (!body || !constant_function_stack_.insert(function->declaration.get()).second)
				return ConstantValue();
			ConstantValue result;
			map<const CPPGMAstNode*, Scope*>::const_iterator block = compound_scopes_.find(body.get());
			Scope* body_scope = block == compound_scopes_.end() ? scope : block->second;
			CPPGMAstNodePtr statement = FindReturnStatement(body);
			if (statement && !statement->children.empty()) result = Evaluate(statement->children[0], body_scope);
			constant_function_stack_.erase(function->declaration.get());
			return result;
		}
	}
	if (expression->kind == "unary-expression")
	{
		if (expression->children.empty()) return ConstantValue();
		ConstantValue child = Evaluate(expression->children[0], scope);
		if (!child.integral.known) return child;
		PA19IntegralValue value = ToIntegralValue(child);
		const string op = OperatorFromNode(expression->value);
		if (op == "+") return FromIntegralValue(PA19Promote(value));
		if (op == "-") {
			value = PA19Promote(value);
			const PA19IntegralType type = value.type;
			const unsigned long long raw = (0ULL - PA19Raw(value)) & PA19Mask(type.bits);
			return FromIntegralValue(type.is_unsigned ? PA19IntegralValue::Unsigned(raw, type.name, type.bits) :
				PA19IntegralValue::Signed(static_cast<long long>(raw), type.name, type.bits));
		}
		if (op == "!") return FromIntegralValue(PA19IntegralValue::Signed(!PA19Raw(value), "int", 32));
		if (op == "~") {
			value = PA19Promote(value);
			const PA19IntegralType type = value.type;
			const unsigned long long raw = (~PA19Raw(value)) & PA19Mask(type.bits);
			return FromIntegralValue(type.is_unsigned ? PA19IntegralValue::Unsigned(raw, type.name, type.bits) :
				PA19IntegralValue::Signed(static_cast<long long>(raw), type.name, type.bits));
		}
		return ConstantValue();
	}
	if (expression->kind == "conditional-expression" && expression->children.size() == 3)
	{
		ConstantValue condition = Evaluate(expression->children[0], scope);
		return !condition.integral.known ? ConstantValue() :
			Evaluate(expression->children[PA19Raw(ToIntegralValue(condition)) ? 1 : 2], scope);
	}
	if (expression->kind == "binary-expression" || expression->kind == "assignment-expression")
	{
		if (expression->children.size() < 2) return ConstantValue();
		ConstantValue left = Evaluate(expression->children[0], scope);
		ConstantValue right = Evaluate(expression->children[1], scope);
		if (!left.integral.known || !right.integral.known) return ConstantValue();
		return FromIntegralValue(PA19Binary(OperatorFromNode(expression->value),
			ToIntegralValue(left), ToIntegralValue(right)));
	}
	return ConstantValue();
}
