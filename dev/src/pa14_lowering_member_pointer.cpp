#include "pa14_lowering.h"
#include <functional>

using namespace std;
namespace cppgm_pa14_lowering {

bool PA14Lowerer::UniqueBaseOffset(const TypePtr& raw_derived,
                                   const TypePtr& raw_target,
                                   size_t* offset)
{
	TypePtr derived = type_value(raw_derived);
	TypePtr target = type_value(raw_target);
	if(!derived || !target || derived->kind != TYPE_CLASS ||
		target->kind != TYPE_CLASS) return false;
	if(PA12SameType(derived, target, true)) {
		if(offset) *offset = 0;
		return true;
	}
	size_t found_offset = 0;
	size_t match_count = 0;
	set<const Type*> path;
	function<void(const TypePtr&, size_t)> visit =
		[&](const TypePtr& current, size_t current_offset) {
			if(!current || !path.insert(current.get()).second) return;
			if(PA12SameType(current, target, true)) {
				++match_count;
				if(match_count == 1) found_offset = current_offset;
				path.erase(current.get());
				return;
			}
			const vector<TypePtr> bases = DirectBaseTypes(current);
			for(size_t base_index = 0; base_index < bases.size(); ++base_index) {
				TypePtr base = type_value(bases[base_index]);
				if(!base) continue;
				const size_t base_offset = base_index < current->direct_base_offsets.size() ?
					current->direct_base_offsets[base_index] :
					(base_index == 0 ? current->direct_base_offset : 0);
				visit(base, current_offset + base_offset);
			}
			path.erase(current.get());
		};
	visit(derived, 0);
	if(match_count != 1) return false;
	if(offset) *offset = found_offset;
	return true;
}

bool PA14Lowerer::TryConvertMemberPointerValue(Value value,
	const TypePtr& target_value, Value* converted)
{
	const TypePtr source_value = type_value(value.type);
	if(source_value && source_value->kind == TYPE_FUNDAMENTAL &&
		source_value->name == "nullptr_t") {
		Value result = value;
		result.type = target_value;
		// Keep LowIR's typed null spelling when the source is nullptr.  An
		// integral null pointer constant is represented by the integer zero
		// instead; both are semantically null but the distinction is useful
		// to the typed object representation.
		result.operand = value.operand.empty() ? "nullptr" : value.operand;
		result.known_constant = false;
		result.constant = 0;
		if(converted) *converted = result;
		return true;
	}
	if(value.known_constant && value.constant == 0 && is_integral_type(source_value)) {
		Value result = value;
		result.type = target_value;
		result.operand = "0";
		if(converted) *converted = result;
		return true;
	}
	if(!source_value || source_value->kind != TYPE_MEMBER_POINTER) return false;
	Value result = value;
	result.type = target_value;
	const TypePtr source_owner = type_value(source_value->member_owner);
	const TypePtr target_owner = type_value(target_value->member_owner);
	if(source_owner && target_owner &&
		!PA12SameType(source_owner, target_owner, true) &&
		IsDerivedFrom(target_owner, source_owner)) {
		size_t base_offset = 0;
		if(!UniqueBaseOffset(target_owner, source_owner, &base_offset))
			throw logic_error("ambiguous member-pointer base conversion");
		if(base_offset != 0 && source_value->child &&
			source_value->child->kind == TYPE_FUNCTION) {
			// The low i64 is the callable address and the high i64 carries
			// the complete-object-to-member-owner adjustment. Direct
			// member-function pointers have a zero high half.
			const string nonnull = new_temp();
			AddInstruction(nonnull + " = cmp ne i128 " + value.operand + ", 0");
			const string null_label = new_label("member_ptr_null");
			const string adjusted_label = new_label("member_ptr_adjusted");
			const string end_label = new_label("member_ptr_end");
			const string slot = new_special_slot("member_ptr", "i128");
			Terminate("branch " + nonnull + ", ^" + adjusted_label + ", ^" + null_label);
			AddBlock(null_label);
			emit_store(target_value, "0", "$" + slot);
			Terminate("jump ^" + end_label);
			AddBlock(adjusted_label);
			const string high = new_temp();
			AddInstruction(high + " = convert zext i128 i64 " +
				integer_text(static_cast<long long>(base_offset)));
			const string shifted = new_temp();
			AddInstruction(shifted + " = binary shl i128 " + high + ", 64");
			const string encoded = new_temp();
			AddInstruction(encoded + " = binary or i128 " + value.operand + ", " + shifted);
			emit_store(target_value, encoded, "$" + slot);
			Terminate("jump ^" + end_label);
			AddBlock(end_label);
			result.operand = emit_load("$" + slot, target_value);
			result.known_constant = false;
			result.constant = 0;
			if(converted) *converted = result;
			return true;
		}
		if(base_offset != 0) {
			if(value.known_constant) {
				if(value.constant != 0) {
					result.constant = value.constant + static_cast<long long>(base_offset);
					result.operand = integer_text(result.constant);
				}
				if(converted) *converted = result;
				return true;
			}
			const string nonnull = new_temp();
			AddInstruction(nonnull + " = cmp ne i64 " + value.operand + ", 0");
			const string null_label = new_label("member_ptr_null");
			const string adjusted_label = new_label("member_ptr_adjusted");
			const string end_label = new_label("member_ptr_end");
			const string slot = new_special_slot("member_ptr", "i64");
			Terminate("branch " + nonnull + ", ^" + adjusted_label + ", ^" + null_label);
			AddBlock(null_label);
			emit_store(target_value, "0", "$" + slot);
			Terminate("jump ^" + end_label);
			AddBlock(adjusted_label);
			const string adjusted = new_temp();
			AddInstruction(adjusted + " = binary add i64 " + value.operand + ", " +
				integer_text(static_cast<long long>(base_offset)));
			emit_store(target_value, adjusted, "$" + slot);
			Terminate("jump ^" + end_label);
			AddBlock(end_label);
			result.operand = emit_load("$" + slot, target_value);
			result.known_constant = false;
			result.constant = 0;
		}
	}
	if(converted) *converted = result;
	return true;
}

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
