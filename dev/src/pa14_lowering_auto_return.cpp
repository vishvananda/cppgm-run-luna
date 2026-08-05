#include <functional>
#include "pa14_lowering.h"

using namespace std;
namespace cppgm_pa14_lowering {

TypePtr PA14Lowerer::DeduceAutoFunctionReturn(FunctionRecord& record,
	const TypePtr& source_function, const CPPGMAstNodePtr& body,
	const vector<CPPGMAstNodePtr>& returns)
{
	FunctionState* saved_state = state_;
	FunctionState scratch(this, &record);
	state_ = &scratch;
	scratch.environments.push_back(map<string, VariablePlan*>());
	if(record.member && !record.static_member && record.member_owner) {
		TypePtr this_type = CloneWithCv(type_value(record.member_owner),
			source_function->function_const, source_function->function_volatile);
		VariablePlan* this_plan = AddVariablePlan("this", PointerTo(this_type),
			CPPGMAstNodePtr(), CPPGMAstNodePtr());
		if(this_plan) scratch.environments.back()["this"] = this_plan;
	}
	if(record.lambda_function || IsLambdaOperator(record)) {
		PlanFunction(scratch);
		map<string, VariablePlan*> planned;
		for(size_t variable = 0; variable < scratch.variables.size(); ++variable)
			planned[scratch.variables[variable].source_name] = &scratch.variables[variable];
		scratch.environments.push_back(planned);
	}
	Scope* expression_scope = record.scope;
	map<const CPPGMAstNode*, Scope*>::const_iterator function_scope =
		analyzer_.function_scopes_.find(record.node.get());
	if(function_scope != analyzer_.function_scopes_.end()) expression_scope = function_scope->second;
	map<const CPPGMAstNode*, Scope*>::const_iterator body_scope =
		analyzer_.compound_scopes_.find(body.get());
	if(body_scope != analyzer_.compound_scopes_.end()) expression_scope = body_scope->second;
	TypePtr deduced;
	string category;
	for(size_t result_index = 0; result_index < returns.size(); ++result_index) {
		ExprInfo info = Infer(returns[result_index], expression_scope);
		TypePtr value = expression_value_type(info);
		if(!value) throw logic_error("auto return expression has no type");
		if(!deduced) {
			deduced = value;
			category = info.category;
			if(info.type && info.type->kind == TYPE_LVALUE_REFERENCE) category = "lvalue";
			else if(info.type && info.type->kind == TYPE_RVALUE_REFERENCE) category = "xvalue";
		} else if(!PA12SameType(deduced, value, false) || category != info.category) {
			if(!PA12SameType(deduced, value, false) ||
				(source_function->child->kind == TYPE_LVALUE_REFERENCE ||
				 source_function->child->kind == TYPE_RVALUE_REFERENCE) &&
				category != info.category)
				throw logic_error("inconsistent auto return deduction");
		}
	}
	const auto without_top_cv = [](const TypePtr& original) -> TypePtr {
		if(!original) return original;
		TypePtr result(new Type(*original));
		result->is_const = false;
		result->is_volatile = false;
		return result;
	};
	function<TypePtr(const TypePtr&, const TypePtr&)> substitute;
	substitute = [&](const TypePtr& pattern, const TypePtr& value) -> TypePtr {
		if(!pattern) return value;
		if(pattern->kind == TYPE_FUNDAMENTAL && pattern->name == "auto")
			return CloneWithCv(value, pattern->is_const, pattern->is_volatile);
		if(pattern->kind == TYPE_POINTER) {
			if(!value || value->kind != TYPE_POINTER)
				throw logic_error("auto return pointer has incompatible type");
			return PointerTo(substitute(pattern->child, value->child));
		}
		if(pattern->kind == TYPE_ARRAY) {
			if(!value || value->kind != TYPE_ARRAY)
				throw logic_error("auto return array has incompatible type");
			return ArrayOf(pattern->bound, substitute(pattern->child, value->child));
		}
		return pattern;
	};
	TypePtr result_type;
	if(source_function->child->kind == TYPE_LVALUE_REFERENCE ||
		source_function->child->kind == TYPE_RVALUE_REFERENCE) {
		const TypeKind kind = source_function->child->kind == TYPE_LVALUE_REFERENCE ||
			category == "lvalue" ? TYPE_LVALUE_REFERENCE : TYPE_RVALUE_REFERENCE;
		result_type = ReferenceTo(kind, substitute(source_function->child->child, deduced));
	} else result_type = substitute(source_function->child, without_top_cv(deduced));
	state_ = saved_state;
	return result_type;
}

} // namespace cppgm_pa14_lowering
