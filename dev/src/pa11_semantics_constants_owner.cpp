#include "pa11_semantics_constants_helpers.h"
#include <functional>

bool Analyzer::IsNullPointerConstantExpression(
	const CPPGMAstNodePtr& expression, Scope* scope)
{
	if(!expression) return false;
	ConstantValue value;
	try {
		value = Evaluate(expression, scope);
	} catch(const logic_error&) {
		// This is a classification query: unevaluable expressions are not
		// null pointer constants, while normal diagnostics stay on the caller.
		return false;
	}
	if(value.integral.known && value.integral.type.integral)
		return PA19Raw(value.integral) == 0;
	return value.kind == ConstantValue::CONSTANT_POINTER && value.pointer &&
		value.pointer->null_pointer && value.type &&
		value.type->kind == TYPE_FUNDAMENTAL && value.type->name == "nullptr_t";
}

void PA11BeginConstantFunctionReturn(Analyzer* analyzer, Binding* function)
{
	TypePtr function_return = function->type && function->type->kind == TYPE_FUNCTION ?
		function->type->child : TypePtr();
	if (function_return && function->member_owner &&
		function->member_owner->kind == TYPE_CLASS &&
		function_return->name == function->member_owner->name)
		function_return = function->member_owner;
	if (function_return && function_return->kind == TYPE_CLASS)
		function_return = TypePtr(new Type(*function_return));
	analyzer->constant_function_return_types_.push_back(function_return);
}

void PA11EndConstantFunctionReturn(Analyzer* analyzer)
{
	analyzer->constant_function_return_types_.pop_back();
}

Binding* PA11FindGeneratedConstructor(Analyzer*, const TypePtr& type,
	 size_t argument_count, Scope* caller_scope)
{
	if (!type || type->name.empty()) return 0;
	Binding* generated_constructor = 0;
	for (Scope* current = caller_scope; current && !generated_constructor;
		current = current->parent)
		for (size_t binding = current->bindings.size(); binding > 0; --binding) {
			Binding& candidate = current->bindings[binding - 1];
			if (candidate.kind == BIND_FUNCTION && candidate.name == type->name &&
				candidate.declaration && candidate.declaration->kind == "special-member-definition") {
				generated_constructor = &candidate;
				break;
			}
		}
	if (generated_constructor && generated_constructor->kind == BIND_FUNCTION &&
		generated_constructor->declaration &&
		generated_constructor->declaration->kind == "special-member-definition" &&
		generated_constructor->type && generated_constructor->type->kind == TYPE_FUNCTION &&
		generated_constructor->type->parameters.size() == argument_count)
		return generated_constructor;
	return 0;
}

TypePtr PA11FindMemberObjectType(Analyzer* analyzer, const CPPGMAstNodePtr& object,
	 const ConstantValue& receiver, Scope* scope)
{
	TypePtr object_type;
	if (object && object->kind == "id-expression") {
		Binding* named = analyzer->ResolveBinding(scope, object->value);
		if (named && (named->kind == BIND_TYPE || named->kind == BIND_TYPE_ALIAS))
			object_type = named->type;
	}
	return object_type ? object_type : receiver.object ? receiver.object->type : receiver.type;
}

TypePtr PA11FindGeneratedConstructorOwner(Analyzer* analyzer, Binding* function,
	 Scope* scope)
{
	TypePtr constructor_owner = function->member_owner;
	if (constructor_owner && constructor_owner->kind != TYPE_CLASS)
		constructor_owner.reset();
	const size_t function_separator = function->qualified_name.rfind("::");
	const bool generated_constructor = function_separator != string::npos &&
		function->qualified_name.substr(0, function_separator) == function->name;
	if (!constructor_owner && function->is_member && function->declaration &&
		(generated_constructor || function->declaration->kind == "special-member-definition" ||
			function->declaration->kind == "special-member-declaration")) {
		const size_t separator = function_separator;
		if (!analyzer->constant_function_return_types_.empty() &&
			analyzer->constant_function_return_types_.back() &&
			analyzer->constant_function_return_types_.back()->kind == TYPE_CLASS)
			constructor_owner = analyzer->constant_function_return_types_.back();
		if (!constructor_owner && separator != string::npos) try {
			constructor_owner = analyzer->ResolveType(scope,
				function->qualified_name.substr(0, separator));
		} catch (const logic_error&) {
			try {
				constructor_owner = analyzer->ResolveType(analyzer->global_.get(),
					function->qualified_name.substr(0, separator));
			} catch (const logic_error&) {}
		}
		if (constructor_owner && constructor_owner->kind != TYPE_CLASS)
			constructor_owner.reset();
		if (!constructor_owner) {
			const string owner_name = function->qualified_name.substr(0, separator);
			std::function<void(Scope*)> find_owner = [&](Scope* current) {
				if (!current || constructor_owner) return;
				for (size_t binding = 0; binding < current->bindings.size(); ++binding) {
					Binding& candidate = current->bindings[binding];
					if (candidate.kind == BIND_FUNCTION && candidate.member_owner &&
						candidate.member_owner->kind == TYPE_CLASS &&
						candidate.qualified_name.compare(0, owner_name.size(), owner_name) == 0 &&
						candidate.qualified_name.size() > owner_name.size() &&
						candidate.qualified_name.compare(owner_name.size(), 2, "::") == 0) {
						constructor_owner = candidate.member_owner;
						return;
					}
					if ((candidate.kind == BIND_TYPE || candidate.kind == BIND_TYPE_ALIAS) &&
						candidate.name == owner_name && candidate.type &&
						candidate.type->kind == TYPE_CLASS) {
						constructor_owner = candidate.type;
						return;
					}
				}
				for (size_t child = 0; child < current->children.size() && !constructor_owner; ++child)
					find_owner(current->children[child].get());
			};
			find_owner(analyzer->global_.get());
		}
	}
	return constructor_owner;
}
