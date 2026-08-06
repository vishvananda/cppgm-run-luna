#include "pa14_lowering.h"

using namespace std;
namespace cppgm_pa14_lowering {

PA14Lowerer::Value PA14Lowerer::EmitUnary(const CPPGMAstNodePtr& node, Scope* scope,
	const TypePtr& expected)
{
	const string op = PA12Operator(node->value);
	if(op == "&" && !node->children.empty() && IsTypeidExpression(node->children[0])) {
		Value type_info = EmitValue(node->children[0], scope);
		type_info.lvalue = false;
		return type_info;
	}
	ExprInfo member_address_info;
	if(op == "&" && node->children[0] &&
		node->children[0]->kind == "id-expression" &&
		node->children[0]->value.find("::") != string::npos) {
		member_address_info = Infer(node->children[0], scope);
		if(expected && type_value(expected) &&
			type_value(expected)->kind == TYPE_MEMBER_POINTER) {
			const TypePtr target = type_value(expected);
			Binding* selected = 0;
			for(size_t candidate = 0; candidate < member_address_info.candidates.size(); ++candidate) {
				Binding* binding = member_address_info.candidates[candidate];
				TypePtr function = binding ? function_target_type(binding->type) : TypePtr();
				if(!binding || !binding->is_member || binding->is_static || !binding->member_owner ||
					!PA12SameType(type_value(binding->member_owner), target->member_owner, true) ||
					(target->child->kind == TYPE_FUNCTION ?
						(!function || !PA12SameType(function, target->child, false)) :
						(!type_value(binding->type) ||
							!PA12SameType(type_value(binding->type), target->child, true)))) continue;
				if(selected && !PA12SameType(type_value(selected->type), type_value(binding->type), true))
					throw logic_error("ambiguous member function address");
				selected = binding;
			}
			if(selected) {
				member_address_info.binding = selected;
				member_address_info.candidates.clear();
				member_address_info.type = selected->type;
			}
		}
	}
	const bool qualified_member_address = op == "&" && node->children[0] &&
		node->children[0]->kind == "id-expression" &&
		node->children[0]->value.find("::") != string::npos &&
		member_address_info.binding && member_address_info.binding->is_member &&
		!member_address_info.binding->is_static && member_address_info.binding->member_owner;
	if(qualified_member_address) {
		Value result;
		const TypePtr owner = type_value(member_address_info.binding->member_owner);
		const TypePtr member_type = expression_value_type(member_address_info);
		result.type = MemberPointerTo(owner, member_type);
		if(member_type && member_type->kind == TYPE_FUNCTION) {
			FunctionRecord* function = RecordForBinding(member_address_info.binding);
			if(!function) throw logic_error("member function pointer has no definition");
			const string address = function_address(function);
			const string raw_address = new_temp();
			AddInstruction(raw_address + " = copy i64 " + address);
			result.operand = new_temp();
			AddInstruction(result.operand + " = convert zext i128 i64 " + raw_address);
		} else {
			if(member_address_info.binding->member_index == static_cast<size_t>(-1) ||
				member_address_info.binding->member_index >= owner->class_members.size())
				throw logic_error("data member pointer has no layout record");
			const long long encoded = static_cast<long long>(
				owner->class_members[member_address_info.binding->member_index].offset) + 1;
			result.known_constant = true;
			result.constant = encoded;
			result.operand = new_temp();
			AddInstruction(result.operand + " = const i64 " + integer_text(encoded));
		}
		return result;
	}
	vector<CPPGMAstNodePtr> operator_arguments;
	operator_arguments.push_back(node->children[0]);
	if(ChooseOperatorCall(OperatorFunctionName(op), operator_arguments, scope).binding)
		return EmitOperatorCall(OperatorFunctionName(op), operator_arguments, scope);
	if(op == "&") {
		if(!node->children.empty() && node->children[0] &&
			node->children[0]->kind == "id-expression") {
			VariablePlan* local = LocalForName(node->children[0]->value);
			TypePtr source_parameter;
			if(local && state_ && state_->record) {
				const vector<string> names = ParameterNames(*state_->record);
				for(size_t parameter = 0; parameter < names.size(); ++parameter)
					if(names[parameter] == node->children[0]->value) {
						source_parameter = LowParameterSourceType(*state_->record, parameter);
						break;
					}
			}
			const TypePtr source_value = source_parameter ?
				type_value(source_parameter) : type_value(local ? local->type : TypePtr());
			const TypePtr expected_pointer = expected && type_value(expected) &&
				type_value(expected)->kind == TYPE_POINTER ? type_value(expected) : TypePtr();
			const TypePtr expected_base = expected_pointer ?
				type_value(expected_pointer->child) : TypePtr();
			if(local && expected_pointer && expected_base && source_value &&
				source_value->kind == TYPE_CLASS && expected_base->kind == TYPE_CLASS && state_) {
				map<string, vector<string> >::const_iterator hidden =
					state_->virtual_base_hidden_by_source.find(node->children[0]->value);
				if(hidden != state_->virtual_base_hidden_by_source.end()) {
					size_t hidden_index = static_cast<size_t>(-1);
					size_t relative = 0;
					for(size_t candidate = 0; candidate < source_value->virtual_base_types.size(); ++candidate) {
						const TypePtr root = candidate < source_value->virtual_base_roots.size() &&
							source_value->virtual_base_roots[candidate] ?
							source_value->virtual_base_roots[candidate] :
							source_value->virtual_base_types[candidate];
						bool root_matches = root && PA12SameType(root, expected_base, true);
						if(!root_matches && root) {
							for(size_t nested = 0; nested < root->virtual_base_types.size(); ++nested)
								if(root->virtual_base_types[nested] &&
									PA12SameType(root->virtual_base_types[nested], expected_base, true)) {
									relative = nested < root->virtual_base_offsets.size() ?
										root->virtual_base_offsets[nested] : 0;
									root_matches = true;
									break;
								}
						}
						if(root_matches) {
							hidden_index = candidate;
							break;
						}
					}
					if(hidden_index != static_cast<size_t>(-1) &&
						hidden_index < hidden->second.size()) {
						string operand = hidden->second[hidden_index];
						if(!operand.empty() && operand[0] == '$')
							operand = emit_load(operand, PointerTo(Fundamental("char")));
						if(relative != 0) {
							const string projected = new_temp();
							AddInstruction(projected + " = index i8 [projection=base_subobject] " +
								operand + ", " + integer_text(static_cast<long long>(relative)));
							operand = projected;
						}
						const string view = new_temp();
						AddInstruction(view + " = index i8 [projection=base_subobject] " +
							operand + ", 0");
						Value result;
						result.type = expected_pointer;
						result.operand = view;
						result.nonnull = true;
						return result;
					}
				}
			}
			const bool reference_parameter = local &&
				(type_is_reference(local->type) || type_is_reference(source_parameter)) &&
				expected_base && expected_base->kind == TYPE_CLASS &&
				!PA12SameType(source_value, expected_base, true);
			if(local && reference_parameter && source_value) {
				Value result;
				result.type = PointerTo(source_value);
				result.operand = emit_load(local_address(local),
					PointerTo(Fundamental("char")));
				result.nonnull = true;
				if(state_) {
					map<string, vector<string> >::const_iterator hidden =
						state_->virtual_base_hidden_by_source.find(node->children[0]->value);
					if(hidden != state_->virtual_base_hidden_by_source.end())
						state_->virtual_base_hidden_by_operand[result.operand] = hidden->second;
				}
				return result;
			}
		}
		Value result;
		result.type = PointerTo(expression_value_type(Infer(node->children[0], scope)));
		result.operand = EmitAddress(node->children[0], scope);
		// The address-of operator cannot produce a null pointer for a valid
		// glvalue.  Preserve that fact for class-pointer conversions so a
		// virtual-base projection is not needlessly wrapped in a null branch.
		result.nonnull = true;
		return result;
	}
	if(op == "*") {
		Value result;
		ExprInfo child_info = Infer(node->children[0], scope);
		TypePtr child_type = expression_value_type(child_info);
		if(!child_type || (child_type->kind != TYPE_POINTER && child_type->kind != TYPE_ARRAY))
			throw logic_error("invalid dereference");
		result.type = child_type->child;
		result.operand = emit_load(EmitAddress(node, scope), result.type);
		return result;
	}
	if(op == "++" || op == "--") return EmitUpdate(node, scope, false);
	const bool boolean_context = op == "!";
	ExprInfo child_info = Infer(node->children[0], scope);
	TypePtr child_type = expression_value_type(child_info);
	Value child;
	if(child_type && child_type->kind == TYPE_CLASS &&
		FindContextConversionOperator(child_type, boolean_context, boolean_context)) {
		child = EmitContextConversion(node->children[0], scope, boolean_context, boolean_context);
		if(child.lvalue && child.type) {
			child.operand = emit_load(child.operand, child.type);
			child.lvalue = false;
		}
	} else child = EmitValue(node->children[0], scope);
	Value result;
	if(op == "+") {
		if(child.type && type_value(child.type)->kind == TYPE_ARRAY) {
			result.type = PointerTo(type_value(child.type)->child);
			result.operand = child.operand;
			return result;
		}
		result.type = IntegralPromotion(child.type);
		return ConvertValue(child, result.type);
	}
	result.type = op == "!" ? Fundamental("bool") : IntegralPromotion(child.type);
	if(op == "!") {
		TypePtr type = type_value(child.type);
		string compare_type = low_type(type);
		string zero = compare_type == "ptr" ? "nullptr" : is_floating_type(type) ?
			(compare_type == "f80" ? "0.0L" : compare_type == "f32" ? "0.0f" : "0.0") : "0";
		string operand = child.operand;
		if(type && type->kind == TYPE_FUNDAMENTAL && type->name == "bool") compare_type = "i64";
		else if(is_integral_type(type) && compare_type != "i64" && compare_type != "u64") compare_type = "i64";
		result.operand = new_temp();
		AddInstruction(result.operand + " = cmp eq " + compare_type + " " + operand + ", " + zero);
		return result;
	}
	result.operand = new_temp();
	const string unary = op == "-" ? "neg" : op == "~" ? "bitnot" : op;
	AddInstruction(result.operand + " = unary " + unary + " " + low_type(result.type) + " " + child.operand);
	return result;
}

} // namespace cppgm_pa14_lowering
