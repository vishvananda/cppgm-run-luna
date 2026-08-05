#include "pa14_lowering.h"

using namespace std;
namespace cppgm_pa14_lowering {

PA14Lowerer::ExprInfo PA14Lowerer::InferBinary(const CPPGMAstNodePtr& node, Scope* scope)
{
	ExprInfo result;
	const string op = PA12Operator(node->value);
	ExprInfo left = Infer(node->children[0], scope);
	ExprInfo right = Infer(node->children[1], scope);
	if(op == ".*" || op == "->*") {
		vector<CPPGMAstNodePtr> operator_arguments;
		operator_arguments.push_back(node->children[0]);
		operator_arguments.push_back(node->children[1]);
		CallChoice overloaded = ChooseOperatorCall(OperatorFunctionName(op), operator_arguments, scope);
		if(overloaded.binding) {
			result.type = overloaded.function->child;
			result.category = type_is_reference(result.type) ?
				(result.type->kind == TYPE_LVALUE_REFERENCE ? "lvalue" : "xvalue") : "prvalue";
			result.binding = overloaded.binding;
			return result;
		}
		const TypePtr member_pointer = expression_value_type(right);
		TypePtr object = expression_value_type(left);
		if(op == "->*") {
			if(!object || object->kind != TYPE_POINTER)
				throw logic_error("arrow-star requires a pointer to class");
			object = type_value(object->child);
		}
		if(!member_pointer || member_pointer->kind != TYPE_MEMBER_POINTER ||
			!member_pointer->member_owner || !object || object->kind != TYPE_CLASS ||
			(!PA12SameType(object, member_pointer->member_owner, true) &&
			 !IsDerivedFrom(object, member_pointer->member_owner)))
			throw logic_error("member pointer does not apply to object");
		result.type = member_pointer->child;
		result.category = "lvalue";
		return result;
	}
	const bool typeid_comparison = op == "==" || op == "!=" || op == "not_eq";
	if(typeid_comparison && IsTypeidExpression(node->children[0]) &&
		IsTypeidExpression(node->children[1])) {
		const TypePtr type_info = TypeInfoType(scope);
		const string member_name = op == "==" ? "operator==" : "operator!=";
		if(MemberBindings(type_info, member_name).empty())
			throw logic_error("type_info comparison operator is unavailable");
		result.type = Fundamental("bool");
		result.category = "prvalue";
		return result;
	}
	vector<CPPGMAstNodePtr> operator_arguments;
	operator_arguments.push_back(node->children[0]);
	operator_arguments.push_back(node->children[1]);
	bool mixed_bitwise = false;
	if(op == "&" || op == "bitand" || op == "|" || op == "bitor" || op == "^" || op == "xor") {
		const TypePtr left_type = expression_value_type(left);
		const TypePtr right_type = expression_value_type(right);
		const bool class_operand = (left_type && left_type->kind == TYPE_CLASS) ||
			(right_type && right_type->kind == TYPE_CLASS);
		const bool same_enum_operands = left_type && right_type &&
			left_type->kind == TYPE_ENUM && right_type->kind == TYPE_ENUM &&
			PA12SameType(left_type, right_type, true);
		mixed_bitwise = !class_operand && !same_enum_operands;
	}
	CallChoice overloaded;
	if(!mixed_bitwise) overloaded = ChooseOperatorCall(OperatorFunctionName(op), operator_arguments, scope);
	bool prefer_builtin = false;
	const bool comparison = op == "==" || op == "!=" || op == "not_eq" ||
		op == "<" || op == ">" || op == "<=" || op == ">=";
	if(overloaded.binding && comparison) {
		TypePtr left_type = expression_value_type(left);
		TypePtr right_type = expression_value_type(right);
		int builtin_user_defined = 0;
		if(left_type && left_type->kind == TYPE_CLASS && right_type && right_type->kind != TYPE_CLASS) {
			Binding* conversion = FindConversionOperator(left_type, right_type, false);
			if(conversion) {
				++builtin_user_defined;
				TypePtr function = function_target_type(conversion->type);
				left_type = function ? type_value(function->child) : left_type;
			}
		} else if(right_type && right_type->kind == TYPE_CLASS && left_type && left_type->kind != TYPE_CLASS) {
			Binding* conversion = FindConversionOperator(right_type, left_type, false);
			if(conversion) {
				++builtin_user_defined;
				TypePtr function = function_target_type(conversion->type);
				right_type = function ? type_value(function->child) : right_type;
			}
		} else if(left_type && right_type && left_type->kind == TYPE_CLASS && right_type->kind == TYPE_CLASS) {
			const vector<Binding*> conversions = ConversionBindings(left_type);
			for(size_t i = 0; i < conversions.size(); ++i) {
				TypePtr function = function_target_type(conversions[i]->type);
				TypePtr result_type = function ? type_value(function->child) : TypePtr();
				if(result_type && FindConversionOperator(right_type, result_type, false)) {
					++builtin_user_defined;
					left_type = result_type;
					right_type = result_type;
					break;
				}
			}
		}
		TypePtr common = CommonType(left_type, right_type, op);
		const int left_rank = common ? ConversionRank(left, common) : -1;
		const int right_rank = common ? ConversionRank(right, common) : -1;
		const bool builtin_type = common && type_value(common) && type_value(common)->kind != TYPE_CLASS;
		if(builtin_type && left_rank >= 0 && right_rank >= 0 &&
			(builtin_user_defined < overloaded.user_defined ||
			 (builtin_user_defined == overloaded.user_defined && max(left_rank, right_rank) < overloaded.worst)))
			prefer_builtin = true;
	}
	if(overloaded.binding && !prefer_builtin) {
		result.type = overloaded.function->child;
		result.category = type_is_reference(result.type) ?
			(result.type->kind == TYPE_LVALUE_REFERENCE ? "lvalue" : "xvalue") : "prvalue";
		result.binding = overloaded.binding;
		return result;
	}
	if(op == ",") {
		result.type = right.type;
		result.category = right.category;
		return result;
	}
	if(op == "&&" || op == "||" || op == "and" || op == "or" || op == "==" || op == "!=" ||
		op == "not_eq" || op == "<" || op == ">" || op == "<=" || op == ">=")
		result.type = Fundamental("bool");
	else if(op == "-" && expression_value_type(left) && expression_value_type(right) &&
		expression_value_type(left)->kind == TYPE_POINTER && expression_value_type(right)->kind == TYPE_POINTER)
		result.type = Fundamental("long int");
	else if((op == "+" || op == "-") && expression_value_type(left) &&
		expression_value_type(left)->kind == TYPE_ARRAY)
		result.type = PointerTo(expression_value_type(left)->child);
	else if((op == "+" || op == "-") && expression_value_type(left) &&
		expression_value_type(left)->kind == TYPE_POINTER)
		result.type = expression_value_type(left);
	else if(op == "+" && expression_value_type(right) &&
		(expression_value_type(right)->kind == TYPE_POINTER || expression_value_type(right)->kind == TYPE_ARRAY))
		result.type = expression_value_type(right)->kind == TYPE_ARRAY ?
			PointerTo(expression_value_type(right)->child) : expression_value_type(right);
	else result.type = ArithmeticCommonType(expression_value_type(left), expression_value_type(right), op);
	result.category = "prvalue";
	return result;
}

} // namespace cppgm_pa14_lowering
