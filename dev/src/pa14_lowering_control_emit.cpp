#include "pa14_lowering.h"

using namespace std;

namespace cppgm_pa14_lowering {

void PA14Lowerer::EmitStatement(const CPPGMAstNodePtr& node, Scope* scope)
{
    if(!node) return;
    const bool is_label = node->kind == "labeled-statement" ||
      node->kind == "case-statement" || node->kind == "default-statement";
    if(state_->current && state_->current->terminated && !is_label) return;

    if(node->kind == "compound-statement") {
      EnterEnvironment();
      for(size_t i = 0; i < node->children.size(); ++i) {
        if(state_->current->terminated &&
           node->children[i] && node->children[i]->kind != "labeled-statement")
          continue;
        EmitStatement(node->children[i], scope);
      }
      map<string, VariablePlan*>& environment = state_->environments.back();
      if(!state_->current->terminated) {
          for(size_t i = state_->variables.size(); i > 0; --i) {
            VariablePlan& variable = state_->variables[i - 1];
            if(state_->return_slot_plan == &variable) continue;
            bool bound_here = false;
          for(map<string, VariablePlan*>::const_iterator it = environment.begin();
              it != environment.end(); ++it)
            if(it->second == &variable) { bound_here = true; break; }
          if(!bound_here) continue;
          TypePtr object_type = type_value(variable.type);
          if(!object_type) continue;
          if(object_type->kind == TYPE_CLASS) {
            if(HasDestructor(object_type))
              (void)EmitDestructorAt(object_type, local_address(&variable), scope);
            continue;
          }
          TypePtr element_type = object_type->child ? type_value(object_type->child) : TypePtr();
          if(object_type->kind != TYPE_ARRAY || object_type->bound < 0 ||
             !element_type || element_type->kind != TYPE_CLASS ||
             !HasDestructor(element_type) || !DestructorHasEffects(element_type)) continue;
          for(size_t element_index = 0;
              element_index < static_cast<size_t>(object_type->bound); ++element_index) {
            const string base = local_address(&variable);
            const string decay = new_temp();
            AddInstruction(decay + " = unary decay ptr " + base);
            const string element = new_temp();
            AddInstruction(element + " = index i8 " + decay + ", " +
              integer_text(static_cast<long long>(element_index)));
            (void)EmitDestructorAt(element_type, element, scope);
          }
        }
      }
      LeaveEnvironment();
      return;
    }
    if(node->kind == "simple-declaration" || node->kind == "bit-field-declaration") {
      BindSimpleDeclaration(node);
      CPPGMAstNodePtr list = ChildOfKind(node, "init-declarator-list");
      if(list) {
        for(size_t i = 0; i < list->children.size(); ++i) {
          CPPGMAstNodePtr item = list->children[i];
          if(!item || item->children.empty()) continue;
          map<const CPPGMAstNode*, VariablePlan*>::iterator found =
            state_->plans.find(item->children[0].get());
          if(found != state_->plans.end() && found->second->global) {
            EmitLocalStaticInitialization(found->second, scope);
            continue;
          }
          bool discard_unused_initializer = false;
          if(found != state_->plans.end() &&
             !type_is_reference(found->second->type) &&
             type_value(found->second->type) &&
             (type_value(found->second->type)->kind == TYPE_CLASS ||
              (type_value(found->second->type)->kind == TYPE_ARRAY &&
               type_value(found->second->type)->child &&
               type_value(found->second->type)->child->kind == TYPE_CLASS))) {
            if(node->materialize_object_address && !node->materialize_object_name.empty()) {
              VariablePlan* dependency = LocalForName(node->materialize_object_name);
              if(dependency && dependency != found->second &&
                 dependency->initialization_address.empty())
                dependency->initialization_address = local_address(dependency);
            }
            bool initialized = item->children.size() > 1;
            bool empty_initializer = false;
            if(initialized) {
              CPPGMAstNodePtr declaration_initializer = InitializerExpression(item->children[1]);
              if(declaration_initializer && declaration_initializer->kind == "braced-init-list" &&
                 declaration_initializer->children.empty()) {
                initialized = false;
                empty_initializer = true;
              }
            }
            const bool referenced = state_->record && state_->record->node &&
              state_->record->node->children.size() > 2 &&
              HasNonSizeofReference(state_->record->node->children[2],
                found->second->source_name);
            const bool meaningfully_referenced = state_->record && state_->record->node &&
              state_->record->node->children.size() > 2 &&
              HasNonSizeofReference(state_->record->node->children[2],
                found->second->source_name, false, true);
            TypePtr planned_object = type_value(found->second->type);
            CPPGMAstNodePtr planned_expression = item->children.size() > 1 ?
              InitializerExpression(item->children[1]) : CPPGMAstNodePtr();
            discard_unused_initializer = initialized && !meaningfully_referenced &&
              planned_expression && planned_expression->kind == "binary-expression" &&
              planned_object && planned_object->kind == TYPE_CLASS &&
              IsTrivialValueStorage(planned_object) &&
              !HasDestructor(planned_object) &&
              !HasDefaultConstructionEffects(planned_object);
            const bool array_default_effects = empty_initializer ?
              HasDefaultInitializationEffects(found->second->type) :
              (HasDefaultConstructionEffects(found->second->type) ||
               HasExplicitConstructor(found->second->type));
            const bool reference_address_needed = planned_object &&
              (planned_object->kind != TYPE_ARRAY ?
               (!empty_initializer || HasDefaultInitializationEffects(found->second->type)) :
               array_default_effects);
            if(initialized || (referenced && reference_address_needed))
              found->second->initialization_address = local_address(found->second);
            else if(node->materialize_object_address &&
                    found->second->initialization_address.empty())
              found->second->initialization_address = local_address(found->second);
          }
          if(found != state_->plans.end() && !found->second->slot_declared) {
            found->second->slot_declared = true;
            state_->slot_order.push_back(FunctionState::SlotEntry(
              false, found->second->slot_name, found->second));
          }
          if(discard_unused_initializer) {
            // Still replay the initializer for demand discovery: lowering
            // the discarded expression marks the selected template bodies
            // needed, but its instructions and temporary storage are not
            // observable when the object value is only discarded.
            const size_t line_mark = state_->current->lines.size();
            const unsigned int temp_mark = state_->next_temp;
            const unsigned int label_mark = state_->next_label;
            const unsigned int special_mark = state_->next_special;
            const size_t special_slot_mark = state_->special_slots.size();
            const size_t slot_order_mark = state_->slot_order.size();
            const size_t temporary_object_mark = state_->temporary_objects.size();
            const set<string> reserved_name_mark = state_->reserved_value_names;
            if(found != state_->plans.end() && item->children.size() > 1)
              EmitInitializer(found->second, item->children[1], scope);
            state_->current->lines.resize(line_mark);
            state_->next_temp = temp_mark;
            state_->next_label = label_mark;
            state_->next_special = special_mark;
            state_->reserved_value_names = reserved_name_mark;
            for(size_t special = special_slot_mark;
                special < state_->special_slots.size(); ++special)
              state_->special_slot_types.erase(state_->special_slots[special]);
            state_->special_slots.resize(special_slot_mark);
            state_->slot_order.resize(slot_order_mark);
            state_->temporary_objects.resize(temporary_object_mark);
            found->second->initialization_address.clear();
            continue;
          }
          if(found != state_->plans.end() && item->children.size() > 1)
            EmitInitializer(found->second, item->children[1], scope);
          else if(found != state_->plans.end() && type_value(found->second->type) &&
                  type_value(found->second->type)->kind == TYPE_CLASS) {
            const bool constructed = EmitObjectConstructor(
              found->second, type_value(found->second->type),
              vector<CPPGMAstNodePtr>(), scope);
            if(!constructed && !found->second->initialization_address.empty())
              (void)EmitAddress(CPPGMAstNodePtr(new CPPGMAstNode(
                "id-expression", found->second->source_name)), scope);
            else if(!constructed && HasConstructor(found->second->type) &&
                    !HasDefaultInitializationEffects(found->second->type))
              (void)EmitAddress(CPPGMAstNodePtr(new CPPGMAstNode(
                "id-expression", found->second->source_name)), scope);
            else if(!constructed) {
              TypePtr object_type = type_value(found->second->type);
              const bool needs_address = object_type &&
                ((!object_type->template_specialization &&
                  !HasConstructor(found->second->type) &&
                  DestructorHasEffects(found->second->type)) ||
                 (object_type->template_specialization &&
                  ((!HasConstructor(found->second->type) &&
                    (DestructorHasEffects(found->second->type) ||
                     HasClassArrayMember(found->second->type))) ||
                   HasNonstaticMemberFunction(found->second->type) ||
                   object_type->class_members.empty())));
              if(needs_address)
                (void)EmitAddress(CPPGMAstNodePtr(new CPPGMAstNode(
                  "id-expression", found->second->source_name)), scope);
            }
          }
          else if(found != state_->plans.end() && found->second->type->kind == TYPE_ARRAY &&
                  found->second->type->child &&
                  type_value(found->second->type->child) &&
                  type_value(found->second->type->child)->kind == TYPE_CLASS &&
                  found->second->type->bound >= 0) {
            const TypePtr element_type = type_value(found->second->type->child);
            if(!HasDefaultConstructionEffects(element_type) &&
               !HasExplicitConstructor(element_type)) continue;
            string base;
            if(!found->second->initialization_address.empty()) {
              base = found->second->initialization_address;
              found->second->initialization_address.clear();
            } else base = local_address(found->second);
            for(size_t element_index = 0;
                element_index < static_cast<size_t>(found->second->type->bound);
                ++element_index) {
              if(element_index != 0)
                base = local_address(found->second);
              const string decay = new_temp();
              AddInstruction(decay + " = unary decay ptr " + base);
              const string element = new_temp();
              AddInstruction(element + " = index i8 " + decay + ", " +
                integer_text(static_cast<long long>(element_index)));
              (void)EmitConstructorAt(found->second->type->child, element,
                vector<CPPGMAstNodePtr>(), scope);
            }
          }
          else if(found != state_->plans.end() &&
                  found->second->type->kind != TYPE_ARRAY &&
                  !type_is_reference(found->second->type) &&
                  (!type_value(found->second->type) ||
                   type_value(found->second->type)->kind != TYPE_CLASS)) {
            // Default-initialization of an automatic scalar leaves it
            // indeterminate.  Do not manufacture a zero store: apart from
            // being semantically wrong, it creates a dead store before a
            // subsequent assignment such as `invoker = &F::call`.
          }
        }
      }
      return;
    }
    if(node->kind == "expression-statement") {
      if(!node->children.empty()) EmitDiscard(node->children[0], scope);
      return;
    }
    if(node->kind == "return-statement") { EmitReturn(node, scope); return; }
    if(node->kind == "if-statement") { EmitIf(node, scope); return; }
    if(node->kind == "while-statement") { EmitWhile(node, scope); return; }
    if(node->kind == "do-statement") { EmitDo(node, scope); return; }
    if(node->kind == "for-statement") { EmitFor(node, scope); return; }
    if(node->kind == "switch-statement") { EmitSwitch(node, scope); return; }
    if(node->kind == "for-init-statement") {
      if(!node->children.empty()) EmitStatement(node->children[0], scope);
      return;
    }
    if(node->kind == "break-statement") {
      if(state_->break_targets.empty()) throw logic_error("break outside loop or switch");
      Terminate("jump ^" + state_->break_targets.back());
      return;
    }
    if(node->kind == "continue-statement") {
      if(state_->continue_targets.empty()) throw logic_error("continue outside loop");
      Terminate("jump ^" + state_->continue_targets.back());
      return;
    }
    if(node->kind == "goto-statement") {
      map<string, string>::iterator found = state_->named_labels.find(node->value);
      if(found == state_->named_labels.end())
        found = state_->named_labels.insert(make_pair(node->value, new_label("goto"))).first;
      Terminate("jump ^" + found->second);
      return;
    }
    if(node->kind == "case-statement" || node->kind == "default-statement") {
      EmitCaseLabelAndBody(node, scope);
      return;
    }
    if(node->kind == "labeled-statement") {
      map<string, string>::iterator found = state_->named_labels.find(node->value);
      if(found == state_->named_labels.end())
        found = state_->named_labels.insert(make_pair(node->value, new_label("goto"))).first;
      const string label = found->second;
      if(!HasBlockLabel(label)) {
        if(!state_->current->terminated && state_->current->label != label)
          Terminate("jump ^" + label);
        AddBlock(label);
      }
      if(!node->children.empty()) EmitStatement(node->children[0], scope);
      return;
    }
    if(node->kind == "null-statement") return;
	    // PA11 has already validated static assertions.  They are declarations
	    // with no runtime LowIR effect and must not reach the statement emitter
	    // when they occur in a materialized function body.
	    if(node->kind == "static-assert-declaration") return;
    // PA14 has no class/object lifetime lowering.  Parsed declaration-like
    // nodes which do not contribute procedural code are harmless here.
    if(node->kind == "alias-declaration" || node->kind == "class-specifier" ||
       node->kind == "class-forward-declaration" || node->kind == "using-declaration" ||
       node->kind == "using-directive" ||
       node->kind == "asm-declaration") return;
    throw logic_error("unsupported statement in LowIR lowering: " + node->kind);
  }

} // namespace cppgm_pa14_lowering
