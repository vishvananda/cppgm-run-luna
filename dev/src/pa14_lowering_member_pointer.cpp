#include "pa14_lowering.h"

using namespace std;
namespace cppgm_pa14_lowering {

string PA14Lowerer::EmitMemberPointerAddress(const CPPGMAstNodePtr& node, Scope* scope)
{
	ExprInfo left_info = Infer(node->children[0], scope);
	ExprInfo right_info = Infer(node->children[1], scope);
	TypePtr object_type = expression_value_type(left_info);
	const TypePtr member_pointer = expression_value_type(right_info);
	if(PA12Operator(node->value) == "->*") {
		if(!object_type || object_type->kind != TYPE_POINTER)
			throw logic_error("arrow-star requires a pointer to class");
		object_type = type_value(object_type->child);
	}
	if(!member_pointer || member_pointer->kind != TYPE_MEMBER_POINTER ||
		!member_pointer->member_owner || !object_type || object_type->kind != TYPE_CLASS ||
		(!PA12SameType(object_type, member_pointer->member_owner, true) &&
		 !IsDerivedFrom(object_type, member_pointer->member_owner)))
		throw logic_error("member pointer does not apply to object");
	if(member_pointer->child && member_pointer->child->kind == TYPE_FUNCTION)
		throw logic_error("member function expression is not an addressable data member");
	string object_address = PA12Operator(node->value) == "->*" ?
		EmitValue(node->children[0], scope).operand : EmitAddress(node->children[0], scope);
	if(!PA12SameType(object_type, member_pointer->member_owner, true))
		object_address = AdjustBaseAddress(object_address, object_type,
			member_pointer->member_owner);
	Value pointer = EmitValue(node->children[1], scope);
	const string offset = new_temp();
	AddInstruction(offset + " = binary sub i64 " + pointer.operand + ", 1");
	const string address = new_temp();
	AddInstruction(address + " = index i8 [projection=field] " + object_address +
		", " + offset);
	return address;
}

} // namespace cppgm_pa14_lowering
