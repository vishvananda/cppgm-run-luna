#include "pa14_lowering.h"
#include <functional>

using namespace std;

namespace cppgm_pa14_lowering {

bool PA14Lowerer::EmitDestructorAt(const TypePtr& raw_object_type, const string& address,
                                   Scope* scope, bool force_empty)
{
    TypePtr object_type = type_value(raw_object_type);
    if(!object_type || object_type->kind != TYPE_CLASS) return false;
    CPPGMAstNodePtr this_node(new CPPGMAstNode("keyword-literal", "KW_THIS:this"));
    const string name = "~" + LastComponent(object_type->name);
    vector<Binding*> candidates = MemberBindings(object_type, name);
    if(candidates.empty() && object_type->owned_scope) {
      CollectImplicitDestructor(object_type, object_type->owned_scope);
      candidates = MemberBindings(object_type, name);
    }
    for(size_t i = 0; i < candidates.size(); ++i) {
      Binding* binding = candidates[i];
      if(binding->kind != BIND_FUNCTION || !binding->is_member || binding->is_static) continue;
      FunctionRecord* record = RecordForBinding(binding);
      if(!record || !record->destructor) continue;
      if(!HasDestructor(object_type)) continue;
      if(!force_empty && !DestructorHasEffects(object_type)) continue;
      // A destructor call made while tearing down a base subobject must use
      // that class's base entry.  Demand it at the call site so ordinary
      // complete-object destruction does not manufacture unused entries.
      // This also keeps the semantic owner attached to the record selected
      // below instead of relying on a speculative prepass.
      if(object_type->polymorphic && force_empty) {
        for(size_t slot_index = 0; slot_index < object_type->virtual_methods.size();
            ++slot_index)
          if(object_type->virtual_methods[slot_index].destructor) {
            EnsureVirtualDestructor(object_type,
              object_type->virtual_methods[slot_index], false, true);
            break;
          }
      }
      // EnsureVirtualDestructor may have materialized the entry after the
      // original binding record was collected.  Re-resolve the typed record
      // before selecting the call target.
      record = RecordForBinding(binding);
      if(!record || !record->destructor) continue;
      MarkFunctionNeeded(record);
      FunctionRecord* base_entry = BaseEntryFor(record);
      FunctionRecord* call_record = object_type->polymorphic && force_empty && base_entry ?
        base_entry : record;
      if(base_entry && object_type->polymorphic && force_empty)
        MarkFunctionNeeded(base_entry);
      vector<string> operands;
      operands.push_back(address);
      if(call_record->vtt_parameter) {
        const bool inherited_vtt = state_ && state_->record &&
          state_->record->base_entry && state_->record->vtt_parameter;
        string vtt_address;
        if(inherited_vtt) vtt_address = "%" + ParameterNames(*state_->record)[
          (state_->record->indirect_result ? 1 : 0) +
          (state_->record->member && !state_->record->static_member ? 1 : 0)];
        else {
          vtt_address = new_temp();
          const TypePtr construction_owner = state_ && state_->record &&
            state_->record->member_owner ? type_value(state_->record->member_owner) : object_type;
          AddInstruction(vtt_address + " = addr @" + VttSymbol(construction_owner));
        }
        const TypePtr construction_owner = state_ && state_->record &&
          state_->record->member_owner ? type_value(state_->record->member_owner) : object_type;
        const string vtt_slot = new_temp();
        AddInstruction(vtt_slot + " = index i8 " + vtt_address + ", " +
          integer_text(static_cast<long long>(ConstructionVttIndex(
            construction_owner, object_type) * 8)));
        operands.push_back(vtt_slot);
      }
      const vector<string> current_names = state_ && state_->record ?
        ParameterNames(*state_->record) : vector<string>();
      const size_t current_hidden_count = state_ && state_->record ?
        state_->record->hidden_virtual_bases.size() : 0;
      const size_t current_hidden_begin = state_ && state_->record &&
        state_->record->type && state_->record->type->parameters.size() >= current_hidden_count ?
        state_->record->type->parameters.size() - current_hidden_count : 0;
      for(size_t hidden = 0; hidden < call_record->hidden_virtual_bases.size(); ++hidden) {
        string hidden_operand;
        if(state_ && state_->record && state_->record->base_entry) {
          for(size_t current = 0; current < current_hidden_count; ++current) {
            if(current_hidden_begin + current >= current_names.size() ||
               !state_->record->hidden_virtual_bases[current] ||
               !call_record->hidden_virtual_bases[hidden] ||
               !PA12SameType(state_->record->hidden_virtual_bases[current],
                             call_record->hidden_virtual_bases[hidden], true)) continue;
            hidden_operand = "%" + current_names[current_hidden_begin + current];
            break;
          }
        }
        if(hidden_operand.empty()) {
          const TypePtr construction_owner = state_ && state_->record &&
            state_->record->member_owner ? type_value(state_->record->member_owner) : object_type;
          size_t virtual_offset = 0;
          if(!FindVirtualBaseOffset(construction_owner,
                                    call_record->hidden_virtual_bases[hidden], &virtual_offset))
            virtual_offset = 0;
          const string this_address = EmitValue(this_node, scope).operand;
          hidden_operand = new_temp();
          AddInstruction(hidden_operand + " = index i8 " + this_address + ", " +
            integer_text(static_cast<long long>(virtual_offset)));
        }
        operands.push_back(hidden_operand);
      }
      ostringstream call;
      call << "call void @" << call_record->symbol << "(";
      for(size_t operand = 0; operand < operands.size(); ++operand) {
        if(operand != 0) call << ", ";
        call << operands[operand];
      }
      call << ")";
      AddInstruction(call.str());
      return true;
    }
    (void)scope;
    return false;
  }

void PA14Lowerer::DemandConstantObjectConstructors(const TypePtr& raw_type,
                                                    const CPPGMAstNodePtr& initializer,
                                                    Scope* scope)
{
  TypePtr type = type_value(raw_type);
  if(!type || !initializer) return;
  CPPGMAstNodePtr expression = initializer;
  if(expression->kind == "initializer" || expression->kind == "paren-initializer" ||
     expression->kind == "default-argument" || expression->kind == "initializer-clause")
    expression = InitializerExpression(expression);
  if(!expression) return;
  if(type->kind == TYPE_ARRAY) {
    if(expression->kind == "braced-init-list")
      for(size_t i = 0; i < expression->children.size(); ++i)
        DemandConstantObjectConstructors(type->child, expression->children[i], scope);
    return;
  }
  if(type->kind != TYPE_CLASS) return;
  vector<CPPGMAstNodePtr> arguments;
  if(expression->kind == "braced-init-list" || expression->kind == "argument-list")
    arguments = expression->children;
  else if(expression->kind == "call-expression" && expression->children.size() > 1) {
    CPPGMAstNodePtr list = expression->children[1];
    arguments = list ? list->children : vector<CPPGMAstNodePtr>();
    if(expression->value == "braced-construction" && arguments.size() == 1 &&
       arguments[0] && arguments[0]->kind == "braced-init-list")
      arguments = arguments[0]->children;
  } else return;
  const string constructor_name = type->template_specialization &&
    !type->template_primary.empty() ? LastComponent(type->template_primary) :
    LastComponent(type->name);
  vector<Binding*> candidates = MemberBindings(type, LastComponent(type->name));
  if(candidates.empty() && constructor_name != LastComponent(type->name))
    candidates = MemberBindings(type, constructor_name);
  if(candidates.empty() && arguments.empty() && scope && type->owned_scope) {
    // Empty class expressions use the implicitly declared default constructor,
    // which is not represented by a binding until a concrete object use
    // demands it.  Materialize that typed member before matching the call.
    CollectImplicitConstructor(type, type->owned_scope, true);
    candidates = MemberBindings(type, LastComponent(type->name));
    if(candidates.empty() && constructor_name != LastComponent(type->name))
      candidates = MemberBindings(type, constructor_name);
  }
  for(size_t i = 0; i < candidates.size(); ++i) {
    Binding* binding = candidates[i];
    FunctionRecord* record = RecordForBinding(binding);
    if(!record || !record->constructor || record->deleted ||
       !record->source_type || record->source_type->parameters.size() != arguments.size())
      continue;
    MarkFunctionNeeded(record);
    break;
  }

  // A zero-storage global may still have a typed constructor closure even
  // when its runtime initialization is elided.  Walk nested construction
  // expressions so the closure remains available in LowIR without emitting
  // the elided constructor actions themselves.
  if(scope) {
    function<void(const CPPGMAstNodePtr&)> demand_nested;
    demand_nested = [&](const CPPGMAstNodePtr& node) {
      if(!node) return;
      if(node->kind == "call-expression" && !node->children.empty()) {
        TypePtr constructed;
        try {
          constructed = ConstructorObjectType(node->children[0], scope);
        } catch(const logic_error&) {
          constructed.reset();
        }
        if(!constructed && !node->inferred_type.empty()) {
          try {
            constructed = type_value(analyzer_.ResolveType(scope,
              node->inferred_type));
          } catch(const logic_error&) {
            constructed.reset();
          }
        }
        if(constructed) {
          const CPPGMAstNodePtr nested_initializer = node->children.size() > 1 &&
            node->children[1] ? node->children[1] : node;
          DemandConstantObjectConstructors(constructed, nested_initializer, scope);
          return;
        }
      }
      for(size_t child = 0; child < node->children.size(); ++child)
        demand_nested(node->children[child]);
    };
    demand_nested(expression);
  }
}

} // namespace cppgm_pa14_lowering
