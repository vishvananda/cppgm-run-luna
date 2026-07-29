#include "pa14_lowering.h"

#include <sstream>
#include <string>

using namespace std;

namespace cppgm_pa14_lowering {

string PA14Lowerer::EmitFunction(FunctionRecord& function)
{
    FunctionState state(this, &function);
    state_ = &state;
    infer_cache_.clear();
    PlanFunction(state);
    // Planning uses short-lived synthetic `this` nodes for implicit member
    // accesses.  Do not let their pointer-keyed inference entries leak into
    // the emission pass when the allocator reuses those addresses.
    infer_cache_.clear();
    state.environments.clear();
    state.environments.push_back(map<string, VariablePlan*>());
    const vector<string> names = ParameterNames(function);
    for(size_t i = 0; i < state.variables.size(); ++i)
      if(state.variables[i].parameter) {
        state.environments.back()[state.variables[i].source_name] = &state.variables[i];
        state.variables[i].slot_declared = true;
        state.slot_order.push_back(FunctionState::SlotEntry(
          false, state.variables[i].slot_name, &state.variables[i]));
      }
    const bool entry = function.qualified_name == "main";
    ostringstream header;
    header << "function @" << function.symbol << "(";
    for(size_t i = 0; i < function.type->parameters.size(); ++i) {
      if(i != 0) header << ", ";
      header << "%" << names[i] << " : " << low_type(function.type->parameters[i]);
      if(type_is_reference(function.type->parameters[i])) header << " [pass=reference]";
      else if(function.indirect_result && i == 0)
        header << " [pass=indirect_result]";
      else if(LowParameterIsByAddress(function, i))
        header << " [pass=by_address]";
    }
    header << ") -> " << low_type(function.type->child);
    vector<string> metadata;
    if(entry) {
      metadata.push_back("role=entry");
      metadata.push_back("binding=strong");
      metadata.push_back("keep_alias=yes");
    } else {
      if(function.variadic) metadata.push_back("arity=variadic");
      if(function.effects.empty() == false) metadata.push_back("effects=" + function.effects);
      if(function.unwind_no) metadata.push_back("unwind=no");
      if(function.noreturn) metadata.push_back("return=noreturn");
      metadata.push_back(function.weak_binding ? "binding=weak" : "binding=strong");
      const string object = function.object_name.empty() ? function.symbol : function.object_name;
      if(!object.empty()) metadata.push_back("object=" + object);
      if(function.object_root) metadata.push_back("object_root=yes");
      if(((function.value_special_member && function.copy_constructor &&
           function.defaulted) ||
          (function.constructor && function.implicit_constructor)) &&
         IsTrivialValueStorage(function.member_owner))
        metadata.push_back("trivial_lifecycle=yes");
    }
    if(!metadata.empty()) {
      header << " [";
      for(size_t i = 0; i < metadata.size(); ++i) {
        if(i != 0) header << ", ";
        header << metadata[i];
      }
      header << "]";
    }
    header << " {";

    AddBlock("entry");
    for(size_t i = 0; i < function.type->parameters.size(); ++i) {
      if(function.indirect_result && i == 0) continue;
      VariablePlan* parameter_plan = LocalForName(names[i]);
      if(!parameter_plan || parameter_plan->parameter_address) continue;
      const bool this_parameter = function.member && !function.static_member &&
        ((!function.indirect_result && i == 0) ||
         (function.indirect_result && i == 1));
      TypePtr source_parameter = LowParameterSourceType(function, i);
      if(!this_parameter && source_parameter && type_value(source_parameter) &&
         type_value(source_parameter)->kind == TYPE_CLASS &&
         !type_is_reference(source_parameter)) {
        TypePtr source_class = type_value(source_parameter);
        const bool empty_class = IsEmptyBaseStorage(source_class);
        if(empty_class) continue;
        const string address = local_address(parameter_plan);
        AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(source_parameter))) +
          "x" + integer_text(static_cast<long long>(type_alignment(source_parameter))) +
          " %" + names[i] + ", " + address);
      } else {
        emit_store(function.type->parameters[i], "%" + names[i],
          StorageForVariable(*parameter_plan));
      }
    }
    Scope* scope = FunctionScope();
    if(function.value_special_member && (function.defaulted || function.implicit_constructor))
      EmitValueSpecialMemberBody(function, scope);
    else if(function.constructor && !function.aggregate_constructor)
      EmitConstructorInitializers(function, scope);
    if(function.aggregate_constructor) EmitAggregateConstructorBody(function, scope);
    CPPGMAstNodePtr body = ChildOfKind(function.node, "compound-statement");
    if(!body && function.node && function.node->children.size() > 2)
      body = function.node->children[2];
    if(body && !(function.value_special_member &&
                 (function.defaulted || function.implicit_constructor))) EmitStatement(body, scope);
    if(function.destructor) EmitDestructorBody(function, scope);
    if(!state.current->terminated) {
      if(low_type(function.type->child) == "void") Terminate("return void");
      else Terminate("return " + low_type(function.type->child) + " 0");
    }

    for(size_t i = 0; i < state.variables.size(); ++i) {
      if(state.variables[i].slot_declared) continue;
      state.variables[i].slot_declared = true;
      if(state.return_slot_plan != &state.variables[i])
        state.slot_order.push_back(FunctionState::SlotEntry(
          false, state.variables[i].slot_name, &state.variables[i]));
    }
    ostringstream out;
    out << header.str() << "\n";
    bool emitted_slot = false;
    for(size_t i = 0; i < state.slot_order.size(); ++i) {
      const FunctionState::SlotEntry& entry = state.slot_order[i];
      if(entry.special) {
        emitted_slot = true;
        out << "  slot $" << entry.name << " : " <<
          state.special_slot_types[entry.name] << "\n";
      } else if(entry.variable && !entry.variable->global &&
                entry.variable != state.return_slot_plan) {
        emitted_slot = true;
        out << "  slot $" << entry.name << " : " <<
          storage_type(entry.variable->type) << "\n";
      }
    }
    if(emitted_slot) out << "\n";
    for(size_t i = 0; i < state.blocks.size(); ++i) {
      if(i != 0) out << "\n";
      out << "  block ^" << state.blocks[i].label << ":\n";
      for(size_t j = 0; j < state.blocks[i].lines.size(); ++j)
        out << state.blocks[i].lines[j] << "\n";
    }
    out << "}";
    state_ = 0;
    return out.str();
  }

} // namespace cppgm_pa14_lowering
