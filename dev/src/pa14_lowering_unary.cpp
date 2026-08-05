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
	if(op == "&" && expected && type_value(expected) &&
		type_value(expected)->kind == TYPE_MEMBER_POINTER && node->children[0] &&
		node->children[0]->kind == "id-expression" &&
		node->children[0]->value.find("::") != string::npos) {
		member_address_info = Infer(node->children[0], scope);
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
		Value result;
		result.type = PointerTo(expression_value_type(Infer(node->children[0], scope)));
		result.operand = EmitAddress(node->children[0], scope);
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
