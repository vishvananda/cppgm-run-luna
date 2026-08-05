#include "pa14_lowering.h"

using namespace std;
namespace cppgm_pa14_lowering {

string PA14Lowerer::EmitReferenceCastAddress(const CPPGMAstNodePtr& node, Scope* scope)
{
	if(!node || node->kind != "cast-expression" || node->children.size() <= 1) return string();
	TypePtr target = analyzer_.TypeFromTypeId(node->children[0], scope);
	if(PA12Operator(node->value) == "dynamic_cast" && type_is_reference(target))
		return EmitDynamicCast(node, scope, target).operand;
	if(!type_is_reference(target)) return string();
	TypePtr target_value = type_value(target);
	ExprInfo source_info = Infer(node->children[1], scope);
	TypePtr source_value = expression_value_type(source_info);
	if(target_value && target_value->kind == TYPE_CLASS && source_value &&
		source_value->kind == TYPE_CLASS && !PA12SameType(target_value, source_value, true)) {
		if(IsDerivedFrom(source_value, target_value))
			return AdjustBaseAddress(EmitAddress(node->children[1], scope), source_value, target_value);
		if(IsDerivedFrom(target_value, source_value))
			return AdjustDerivedAddress(EmitAddress(node->children[1], scope), target_value, source_value);
		const string slot = new_special_slot("tmpobj", low_type(target_value));
		const string address = new_temp();
		AddInstruction(address + " = addr $" + slot);
		vector<CPPGMAstNodePtr> arguments(1, node->children[1]);
		if(!EmitConstructorAt(target_value, address, arguments, scope, true))
			throw logic_error("class reference cast has no viable constructor");
		RegisterTemporaryObject(target_value, address);
		return address;
	}
	return EmitAddress(node->children[1], scope);
}

} // namespace cppgm_pa14_lowering
