#include "pa14_lowering.h"

using namespace std;
namespace cppgm_pa14_lowering {

PA14Lowerer::ExprInfo PA14Lowerer::InferIdentifier(const CPPGMAstNodePtr& node, Scope* scope,
	const TypePtr& expected) const
{
	if(node && !node->value.empty() &&
		(isdigit(static_cast<unsigned char>(node->value[0])) ||
		 ((node->value[0] == '-' || node->value[0] == '+') && node->value.size() > 1 &&
		  isdigit(static_cast<unsigned char>(node->value[1]))))) {
		TypePtr literal_type;
		long long literal_value = 0;
		bool literal_known = false;
		const string literal = canonical_literal(node->value, &literal_type,
			&literal_value, &literal_known);
		if(literal_known) {
			ExprInfo result;
			result.type = literal_type;
			result.operand = literal;
			result.category = "prvalue";
			result.known_constant = true;
			result.constant = literal_value;
			return result;
		}
	}
	ExprInfo result;
	VariablePlan* local = FindLocalPlan(node->value);
	if(local) {
		result.type = type_is_reference(local->type) ? local->type->child : local->type;
		result.category = "lvalue";
		result.binding = 0;
		InferLocalIdentifierConstant(result.type, &result);
		return result;
	}
	Binding* decltype_member = ResolveDecltypeStaticMember(node->value, scope);
	if(decltype_member) {
		result.binding = decltype_member;
		result.type = PA12AdjustedType(decltype_member->type);
		if(type_is_reference(result.type)) result.type = result.type->child;
		result.category = "lvalue";
		const TypePtr constant_type = type_value(result.type);
		const bool integral_constant = is_integral_type(result.type) ||
			(constant_type && constant_type->kind == TYPE_FUNDAMENTAL && constant_type->name == "bool");
		if(decltype_member->is_static && decltype_member->has_value && integral_constant) {
			result.known_constant = true;
			result.constant = decltype_member->value;
			result.operand = integer_text(result.constant);
			result.category = "prvalue";
		}
		return result;
	}
	result.candidates = Lookup(node->value, scope);
	if(result.candidates.empty()) return InferCapturedIdentifier(node, scope, expected);
	if(result.candidates.size() > 1) {
		bool repeated_binding = true;
		for(size_t i = 1; i < result.candidates.size(); ++i)
			if(result.candidates[i] != result.candidates[0]) { repeated_binding = false; break; }
		if(repeated_binding) throw logic_error("ambiguous expression name: " + node->value);
	}
	if(expected && !result.candidates.empty()) {
		TypePtr target = type_value(expected);
		Binding* selected = 0;
		int best = 1000000;
		for(size_t i = 0; i < result.candidates.size(); ++i) {
			TypePtr candidate = function_target_type(result.candidates[i]->type);
			if(!candidate) continue;
			ExprInfo source;
			source.type = candidate;
			source.category = "lvalue";
			const int rank = ConversionRank(source, target);
			if(rank >= 0 && rank < best) { best = rank; selected = result.candidates[i]; }
			else if(rank >= 0 && rank == best) throw logic_error("ambiguous function target");
		}
		if(selected) result.binding = selected;
	}
	if(result.binding) result.candidates.clear();
	if(!result.binding && result.candidates.empty())
		throw logic_error("unknown expression name: " + node->value);
	if(!result.binding && result.candidates.size() == 1) result.binding = result.candidates[0];
	if(result.binding && !IsAccessible(result.binding, scope))
		throw logic_error("inaccessible member");
	if(result.binding && result.binding->kind == BIND_ENUMERATOR) {
		result.type = result.binding->type;
		result.category = "prvalue";
		result.known_constant = result.binding->has_value;
		result.constant = result.binding->value;
		result.operand = integer_text(result.constant);
		return result;
	}
	if(result.binding) {
		result.type = PA12AdjustedType(result.binding->type);
		if(type_is_reference(result.type)) result.type = result.type->child;
		VariablePlan* this_plan = FindLocalPlan("this");
		TypePtr this_type = this_plan ? type_value(this_plan->type) : TypePtr();
		if(this_type && this_type->kind == TYPE_POINTER) this_type = type_value(this_type->child);
		if(result.binding->is_member && !result.binding->is_static &&
			result.binding->kind != BIND_FUNCTION && this_type && this_type->is_const &&
			result.binding->member_owner && result.binding->member_index != static_cast<size_t>(-1) &&
			result.binding->member_index < result.binding->member_owner->class_members.size() &&
			!result.binding->member_owner->class_members[result.binding->member_index].is_mutable)
			result.type = CloneWithCv(result.type, true, result.type->is_volatile);
		result.category = "lvalue";
		const TypePtr constant_type = type_value(result.type);
		const bool integral_constant = is_integral_type(result.type) ||
			(constant_type && constant_type->kind == TYPE_FUNDAMENTAL && constant_type->name == "bool");
		if(result.binding->is_member && result.binding->is_static && result.binding->has_value && integral_constant) {
			result.known_constant = true;
			result.constant = result.binding->value;
			result.operand = integer_text(result.constant);
			result.category = "prvalue";
		}
		return result;
	}
	result.type = function_target_type(result.candidates[0]->type);
	if(!result.type) result.type = result.candidates[0]->type;
	result.category = "lvalue";
	return result;
}

} // namespace cppgm_pa14_lowering
