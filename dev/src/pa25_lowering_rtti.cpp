#include "pa14_lowering.h"

using namespace std;

namespace cppgm_pa14_lowering {

PA14Lowerer::Value PA14Lowerer::EmitTypeidExpression(
  const CPPGMAstNodePtr& node, Scope* scope)
{
  if(!node || node->children.empty()) throw logic_error("typeid has no operand");
  const TypePtr info_type = TypeInfoType(scope);
  const CPPGMAstNodePtr operand = node->children[0];
  TypePtr queried = operand && operand->kind == "type-id" ?
    analyzer_.TypeFromTypeId(operand, scope) : Infer(operand, scope).type;
  queried = RttiValueType(queried);
  if(!queried) throw logic_error("typeid operand has no type");
  EnsureRttiType(queried);

  Value result;
  result.type = PointerTo(info_type);
  if(operand && operand->kind != "type-id" && queried->kind == TYPE_CLASS &&
     queried->polymorphic) {
    const ExprInfo operand_info = Infer(operand, scope);
    const string object = operand_info.category == "lvalue" ?
      EmitAddress(operand, scope) : EmitValue(operand, scope).operand;
    const string fail_label = new_label("typeid_fail");
    const string scan_label = new_label("typeid_scan");
    const string null_object = new_temp();
    AddInstruction(null_object + " = cmp eq ptr " + object + ", 0");
    Terminate("branch " + null_object + ", ^" + fail_label + ", ^" + scan_label);

    AddBlock(fail_label);
    const TypePtr bad_typeid_type = FunctionOf(vector<TypePtr>(), false,
      Fundamental("void"), false);
    FunctionRecord* bad_typeid = FindFunction(
      "__external_runtime____cxa_bad_typeid", bad_typeid_type);
    if(!bad_typeid) throw logic_error("missing __cxa_bad_typeid runtime declaration");
    MarkFunctionNeeded(bad_typeid);
    AddInstruction("call void @" + bad_typeid->symbol + "()");
    const TypePtr return_type = state_ && state_->record && state_->record->type ?
      state_->record->type->child : Fundamental("void");
    Terminate(low_type(return_type) == "void" ? "return void" :
      "return " + low_type(return_type) + " 0");

    AddBlock(scan_label);
    const string vtable = emit_load(object, PointerTo(Fundamental("char")));
    const string rtti_address = new_temp();
    AddInstruction(rtti_address + " = index i8 " + vtable + ", -8");
    result.operand = emit_load(rtti_address, PointerTo(Fundamental("char")));
    return result;
  }
  result.operand = new_temp();
  AddInstruction(result.operand + " = addr @" + RttiSymbol(queried));
  return result;
}

PA14Lowerer::Value PA14Lowerer::EmitDynamicCast(
  const CPPGMAstNodePtr& node, Scope* scope, const TypePtr& target)
{
  if(!node || node->children.size() < 2 || !target)
    throw logic_error("unsupported dynamic_cast target");
  const bool reference_target = type_is_reference(target);
  const TypePtr target_value = reference_target ? type_value(target->child) :
    type_value(target);
  const TypePtr target_class = target_value && target_value->kind == TYPE_POINTER ?
    RttiValueType(target_value->child) : RttiValueType(target_value);
  if(!target_class || target_class->kind != TYPE_CLASS)
    throw logic_error("unsupported dynamic_cast target");
  const ExprInfo source_info = Infer(node->children[1], scope);
  const TypePtr source = expression_value_type(source_info);
  const bool source_pointer = source && source->kind == TYPE_POINTER;
  const TypePtr source_class = source_pointer ?
    (source->child ? RttiValueType(source->child) : TypePtr()) : RttiValueType(source);
  if(!source_class || source_class->kind != TYPE_CLASS ||
     !source_class->polymorphic || (!source_pointer && !reference_target))
    throw logic_error("dynamic_cast source is not polymorphic");
  EnsureRttiType(source_class);
  EnsureRttiType(target_class);

  Value source_value;
  if(!source_pointer) {
    const CPPGMAstNodePtr source_node = node->children[1];
    source_value.type = PointerTo(Fundamental("char"));
    source_value.operand = source_info.category == "lvalue" ?
      EmitAddress(source_node, scope) : EmitValue(source_node, scope).operand;
  } else {
    source_value = EmitValue(node->children[1], scope);
    if(source_value.lvalue && source_value.type) {
      source_value.operand = emit_load(source_value.operand, source_value.type);
      source_value.lvalue = false;
    }
  }
  const string slot = new_special_slot("dyn_cast", "ptr");
  emit_store(PointerTo(Fundamental("char")), "0", "$" + slot);
  const string scan_label = new_label("dyn_cast_scan");
  const string end_label = new_label("dyn_cast_end");
  const string null_source = new_temp();
  AddInstruction(null_source + " = cmp eq ptr " + source_value.operand + ", 0");
  Terminate("branch " + null_source + ", ^" + end_label + ", ^" + scan_label);

  AddBlock(scan_label);
  vector<TypePtr> parameters;
  parameters.push_back(PointerTo(Fundamental("void")));
  parameters.push_back(PointerTo(Fundamental("void")));
  parameters.push_back(PointerTo(Fundamental("void")));
  parameters.push_back(Fundamental("long int"));
  const TypePtr dynamic_cast_type = FunctionOf(parameters, false,
    PointerTo(Fundamental("void")), false);
  FunctionRecord* dynamic_cast_function = FindFunction(
    "__external_runtime____dynamic_cast", dynamic_cast_type);
  if(!dynamic_cast_function)
    throw logic_error("missing __dynamic_cast runtime declaration");
  MarkFunctionNeeded(dynamic_cast_function);
  const string source_rtti = new_temp();
  AddInstruction(source_rtti + " = addr @" + RttiSymbol(source_class));
  const string target_rtti = new_temp();
  AddInstruction(target_rtti + " = addr @" + RttiSymbol(target_class));
  const string converted = new_temp();
  AddInstruction(converted + " = call ptr @" + dynamic_cast_function->symbol +
    "(" + source_value.operand + ", " + source_rtti + ", " + target_rtti + ", 0)");
  emit_store(PointerTo(Fundamental("char")), converted, "$" + slot);
  if(reference_target) {
    const string fail_label = new_label("dyn_cast_fail");
    const string found_label = new_label("dyn_cast_found");
    const string block_label = new_label("block");
    const string null_result = new_temp();
    AddInstruction(null_result + " = cmp eq ptr " + converted + ", 0");
    Terminate("branch " + null_result + ", ^" + fail_label + ", ^" + found_label);
    AddBlock(fail_label);
    const TypePtr bad_cast_type = FunctionOf(vector<TypePtr>(), false,
      Fundamental("void"), false);
    FunctionRecord* bad_cast = FindFunction(
      "__external_runtime____cxa_bad_cast", bad_cast_type);
    if(!bad_cast) throw logic_error("missing __cxa_bad_cast runtime declaration");
    MarkFunctionNeeded(bad_cast);
    AddInstruction("call void @" + bad_cast->symbol + "()");
    const TypePtr return_type = state_ && state_->record && state_->record->type ?
      state_->record->type->child : Fundamental("void");
    Terminate(low_type(return_type) == "void" ? "return void" :
      "return " + low_type(return_type) + " 0");
    AddBlock(found_label);
    Terminate("jump ^" + end_label);
    AddBlock(block_label);
    Terminate("jump ^" + end_label);
  } else Terminate("jump ^" + end_label);

  AddBlock(end_label);
  Value result;
  result.type = reference_target ? target->child : target;
  result.operand = emit_load("$" + slot,
    reference_target ? PointerTo(Fundamental("char")) : result.type);
  result.lvalue = reference_target;
  return result;
}

} // namespace cppgm_pa14_lowering
