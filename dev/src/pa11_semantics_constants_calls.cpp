#include "pa11_semantics_constants_helpers.h"

namespace {
bool NoexceptMemberCall(Analyzer& analyzer, const CPPGMAstNodePtr& call,
	Scope* scope, const CPPGMAstNodePtr& callee)
{
	TypePtr object = analyzer.ExpressionType(callee->children[0], scope);
	while (object && (object->kind == TYPE_LVALUE_REFERENCE ||
		object->kind == TYPE_RVALUE_REFERENCE || object->kind == TYPE_POINTER))
		object = object->child;
	const string member_name = callee->children[1]->value;
	const vector<TypePtr> object_types = BaseTypeClosure(object);
	for (size_t current_index = 0; current_index < object_types.size(); ++current_index) {
		TypePtr current = object_types[current_index];
		Scope* owner = analyzer.ScopeForType(current);
		if (!owner) continue;
		for (size_t i = 0; i < owner->bindings.size(); ++i) {
			Binding& candidate = owner->bindings[i];
			if (candidate.kind != BIND_FUNCTION || candidate.name != member_name ||
				!candidate.type || candidate.type->kind != TYPE_FUNCTION) continue;
			if (candidate.type->parameters.size() != (call->children.size() > 1 ?
				call->children[1]->children.size() : 0)) continue;
			bool matches = true;
			if (call->children.size() > 1)
				for (size_t argument = 0; argument < call->children[1]->children.size(); ++argument) {
					const CPPGMAstNodePtr actual = call->children[1]->children[argument];
					TypePtr actual_type;
					if (actual && actual->kind == "call-expression" && !actual->children.empty() &&
						actual->children[0] && actual->children[0]->kind == "id-expression") {
						Binding* named_type = analyzer.ResolveBinding(scope,
							actual->children[0]->value);
						if (named_type && (named_type->kind == BIND_TYPE ||
							named_type->kind == BIND_TYPE_ALIAS) ) actual_type = named_type->type;
					}
					if (!actual_type) actual_type = analyzer.ExpressionType(actual, scope);
					TypePtr formal = candidate.type->parameters[argument];
					while (formal && (formal->kind == TYPE_LVALUE_REFERENCE ||
						formal->kind == TYPE_RVALUE_REFERENCE)) formal = formal->child;
					if (actual_type && formal && actual_type->kind == TYPE_CLASS &&
						formal->kind == TYPE_CLASS && !SameTypeIgnoringTopCv(actual_type, formal))
						matches = false;
				}
			if (matches) return candidate.noexcept_specified || Analyzer::HasNodeValue(
				candidate.declaration, "function-qualifier", "noexcept");
		}
	}
	return false;
}
}
bool PA11NoexceptCall(Analyzer& analyzer, const CPPGMAstNodePtr& call, Scope* scope)
{
	if (!call || call->kind != "call-expression" || call->children.empty()) return false;
	CPPGMAstNodePtr callee = call->children[0];
	if (!callee) return false;
	if (callee->kind == "member-expression" && callee->children.size() >= 2)
		return NoexceptMemberCall(analyzer, call, scope, callee);
	if (callee->kind == "call-expression" && !callee->children.empty() &&
		callee->children[0] && callee->children[0]->kind == "id-expression")
	{
		// A declval<T>() call is an unevaluated source of a T object.  The
		// parser retains the template-id in the id spelling, so recover the
		// class argument directly.  PA18 may instead replace that spelling with
		// a materialized function name; in that case its call result supplies the
		// same class type.
		const string raw = callee->children[0]->value;
		const size_t open = raw.find('<');
		const size_t close = raw.rfind('>');
		TypePtr object;
		if (raw.compare(0, open, "declval") == 0 && open != string::npos &&
			close != string::npos && close > open)
		{
			string type_name = raw.substr(open + 1, close - open - 1);
			while (!type_name.empty() && isspace(static_cast<unsigned char>(type_name[0])))
				type_name.erase(type_name.begin());
			while (!type_name.empty() && (type_name[type_name.size() - 1] == '&' ||
				type_name[type_name.size() - 1] == '*' ||
				isspace(static_cast<unsigned char>(type_name[type_name.size() - 1]))))
				type_name.erase(type_name.size() - 1);
			if (type_name.compare(0, 6, "const ") == 0) type_name.erase(0, 6);
			if (type_name.compare(0, 8, "volatile ") == 0) type_name.erase(0, 8);
			Binding* type_binding = analyzer.ResolveBinding(scope, type_name);
			object = type_binding && (type_binding->kind == BIND_TYPE ||
				type_binding->kind == BIND_TYPE_ALIAS) ? type_binding->type : TypePtr();
		}
		if (!object)
		{
			Binding* generated = analyzer.ResolveBinding(scope, raw);
			if (generated && generated->kind == BIND_FUNCTION && generated->type &&
				generated->type->kind == TYPE_FUNCTION)
				object = generated->type->child;
		}
		if (!object && raw.compare(0, 14, "declval__inst_") == 0)
		{
			string encoded = raw.substr(14);
			if (encoded.compare(0, 6, "const_") == 0) encoded.erase(0, 6);
			if (encoded.size() > 5 && encoded.compare(encoded.size() - 5, 5, "_rref") == 0)
				encoded.erase(encoded.size() - 5);
			else if (encoded.size() > 4 && encoded.compare(encoded.size() - 4, 4, "_ref") == 0)
				encoded.erase(encoded.size() - 4);
			else if (encoded.size() > 4 && encoded.compare(encoded.size() - 4, 4, "_ptr") == 0)
				encoded.erase(encoded.size() - 4);
			Binding* type_binding = analyzer.ResolveBinding(scope, encoded);
			if (type_binding && (type_binding->kind == BIND_TYPE ||
				type_binding->kind == BIND_TYPE_ALIAS)) object = type_binding->type;
		}
		while (object && (object->kind == TYPE_LVALUE_REFERENCE ||
			object->kind == TYPE_RVALUE_REFERENCE || object->kind == TYPE_POINTER))
			object = object->child;
		if (object && object->kind == TYPE_CLASS)
		{
			Scope* owner = analyzer.ScopeForType(object);
			const size_t arity = call->children.size() > 1 ?
				call->children[1]->children.size() : 0;
			if (owner)
				for (size_t i = 0; i < owner->bindings.size(); ++i)
				{
					Binding& candidate = owner->bindings[i];
					if (candidate.kind == BIND_FUNCTION && candidate.name == "operator()" &&
						candidate.type && candidate.type->kind == TYPE_FUNCTION &&
						candidate.type->parameters.size() == arity)
						return candidate.noexcept_specified || Analyzer::HasNodeValue(
							candidate.declaration, "function-qualifier", "noexcept");
				}
		}
		return false;
	}
	if (callee->kind != "id-expression") return false;
	Binding* binding = analyzer.ResolveBinding(scope, callee->value);
	if (!binding) return false;
	if (binding->kind == BIND_FUNCTION)
		return binding->noexcept_specified ||
			Analyzer::HasNodeValue(binding->declaration, "function-qualifier", "noexcept");
	if (binding->kind != BIND_TYPE && binding->kind != BIND_TYPE_ALIAS) return false;
	Scope* class_scope = analyzer.ScopeForType(binding->type);
	if (!class_scope) return false;
	const string constructor_name = LastComponent(binding->type->name);
	for (size_t i = 0; i < class_scope->bindings.size(); ++i)
	{
		Binding& candidate = class_scope->bindings[i];
		if (candidate.kind != BIND_FUNCTION || candidate.name != constructor_name) continue;
		if (candidate.noexcept_specified || Analyzer::HasNodeValue(candidate.declaration,
			"function-qualifier", "noexcept")) return true;
		if (Analyzer::HasNodeValue(candidate.declaration, "special-initializer", "default"))
			return true;
	}
	return false;
}
