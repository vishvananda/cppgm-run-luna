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
	// Preserve the typed member-pointer fact while materializing the selected
	// address.  Without the expected type, a qualified `&Owner::member` inside
	// a replayed member-template body is treated as an implicit `this` member
	// access on the enclosing helper class.  The typed unary-address path can
	// instead select the owner binding and encode its layout offset directly.
	Value pointer;
	if(node->children[1] && node->children[1]->kind == "unary-expression" &&
		PA12Operator(node->children[1]->value) == "&" && right_info.binding &&
		right_info.binding->is_member && !right_info.binding->is_static &&
		right_info.binding->member_owner && member_pointer->child &&
		member_pointer->child->kind != TYPE_FUNCTION) {
		const TypePtr owner = type_value(right_info.binding->member_owner);
		if(right_info.binding->member_index == static_cast<size_t>(-1) ||
			right_info.binding->member_index >= owner->class_members.size())
			throw logic_error("data member pointer has no layout record");
		pointer.type = member_pointer;
		pointer.known_constant = true;
		pointer.constant = static_cast<long long>(owner->class_members[
			right_info.binding->member_index].offset) + 1;
		pointer.operand = integer_text(pointer.constant);
	} else pointer = EmitValue(node->children[1], scope, member_pointer);
	const string pointer_operand = pointer.known_constant ?
		integer_text(pointer.constant) : pointer.operand;
	const string offset = new_temp();
	AddInstruction(offset + " = binary sub i64 " + pointer_operand + ", 1");
	const string address = new_temp();
	AddInstruction(address + " = index i8 [projection=field] " + object_address +
		", " + offset);
	return address;
}

} // namespace cppgm_pa14_lowering
