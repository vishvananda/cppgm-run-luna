#include "pa14_lowering.h"

using namespace std;

namespace cppgm_pa14_lowering
{

void PA14Lowerer::EmitGlobalInitializer(GlobalRecord& global, Scope* scope)
{
    TypePtr type = global.type;
    TypePtr value_type = type_value(type);
    if(!type || !value_type) return;
    CPPGMAstNodePtr expression = InitializerExpression(global.initializer);
    VariablePlan plan;
    plan.source_name = LastComponent(global.qualified_name);
    plan.slot_name = plan.source_name;
    plan.type = type;
    plan.initializer = global.initializer;
    plan.global = &global;

    if(type_is_reference(type)) {
      if(!expression) return;
      emit_store(PointerTo(Fundamental("char")),
        EmitReferenceArgument(expression, scope, type), "@" + global.symbol);
      return;
    }

    if(type->kind == TYPE_ARRAY) {
      TypePtr element_type = type_value(type->child);
      if(!element_type) return;
      vector<CPPGMAstNodePtr> children;
      if(expression && expression->kind == "braced-init-list") children = expression->children;
      const size_t bound = type->bound < 0 ? children.size() : static_cast<size_t>(type->bound);
      for(size_t i = 0; i < bound; ++i) {
        if(i >= children.size() && element_type->kind != TYPE_CLASS) continue;
        const string base = global_address(&global);
        const string decay = new_temp();
        AddInstruction(decay + " = unary decay ptr " + base);
        const string offset = new_temp();
        AddInstruction(offset + " = binary mul i64 " + integer_text(static_cast<long long>(i)) +
          ", " + integer_text(static_cast<long long>(type_size(type->child))));
        const string element = new_temp();
        AddInstruction(element + " = index i8 [projection=array_element] " + decay + ", " + offset);
        CPPGMAstNodePtr child = i < children.size() ? children[i] : CPPGMAstNodePtr();
        if(element_type->kind == TYPE_CLASS) {
          vector<CPPGMAstNodePtr> arguments;
          if(child && child->kind == "braced-init-list") arguments = child->children;
          else if(child && child->kind == "paren-initializer") arguments = child->children;
          else if(child && child->kind == "call-expression" && child->children.size() > 1 &&
                  child->children[0] && child->children[0]->kind == "id-expression" &&
                  LastComponent(element_type->name) == child->children[0]->value)
            arguments = child->children[1] ? child->children[1]->children :
              vector<CPPGMAstNodePtr>();
          if(child && child->kind != "braced-init-list" &&
             child->kind != "paren-initializer" &&
             EmitObjectTransferAt(element_type, element, child, scope, true)) continue;
          if(EmitConstructorAt(element_type, element, arguments, scope,
                               true, false, true)) continue;
          if(child && child->kind == "braced-init-list") {
            EmitAggregateAt(element, element_type, child, scope);
            continue;
          }
          continue;
        }
        if(element_type->kind == TYPE_ARRAY && child &&
           child->kind == "braced-init-list") {
          EmitAggregateAt(element, element_type, child, scope);
          continue;
        }
        if(!child) continue;
        Value value = EmitValue(child, scope, type->child);
        if(value.known_constant && is_integral_type(value.type) &&
           is_integral_type(type->child)) {
          value.type = type->child;
          value.operand = integer_text(value.constant);
        } else value = ConvertValue(value, type->child);
        emit_store(type->child, value.operand, element);
      }
      return;
    }

    if(value_type->kind == TYPE_CLASS) {
      vector<CPPGMAstNodePtr> arguments;
      if(global.initializer && !global.initializer->children.empty() &&
         global.initializer->children[0] &&
         global.initializer->children[0]->kind == "paren-initializer")
        arguments = global.initializer->children[0]->children;
      else if(expression && expression->kind == "braced-init-list")
        arguments = expression->children;
      if(!expression && !HasConstructor(value_type)) return;
      if(!HasDefaultInitializationEffects(value_type) && !HasDestructor(value_type)) return;
      bool declared_constructor = false;
      const vector<Binding*> constructors =
        MemberBindings(value_type, LastComponent(value_type->name));
      for(size_t i = 0; i < constructors.size(); ++i) {
        FunctionRecord* record = RecordForBinding(constructors[i]);
        if(record && record->constructor && !record->implicit_constructor &&
           !record->aggregate_constructor) {
          declared_constructor = true;
          break;
        }
      }
      if(expression && expression->kind == "braced-init-list" && !declared_constructor) {
        // Aggregate members of a global are projected from a fresh global
        // address, just as ordinary global lvalue projections are.  Keeping
        // the source node here lets the aggregate walker recompute that
        // typed base for each member without changing local aggregate
        // lowering.
        CPPGMAstNodePtr object_node(new CPPGMAstNode(
          "id-expression", plan.source_name));
        EmitAggregateAt(string(), value_type, expression, scope, object_node);
        return;
      }
      plan.initialization_address = global_address(&global);
      if(EmitObjectConstructor(&plan, value_type, arguments, scope)) return;
      return;
    }

    if(!expression) return;
    Value value = EmitValue(expression, scope, type);
    value = ConvertValue(value, type);
    emit_store(type, value.operand, "@" + global.symbol);
  }
void PA14Lowerer::EmitGlobalFinalizer(GlobalRecord& global, Scope* scope)
{
    TypePtr type = type_value(global.type);
    if(!type) return;
    if(type->kind == TYPE_ARRAY) {
      TypePtr element_type = type_value(type->child);
      if(!element_type || type->bound < 0) return;
      const string base = global_address(&global);
      const string decay = new_temp();
      AddInstruction(decay + " = unary decay ptr " + base);
      for(size_t i = static_cast<size_t>(type->bound); i > 0; --i) {
        const size_t index_value = i - 1;
        const string offset = new_temp();
        AddInstruction(offset + " = binary mul i64 " +
          integer_text(static_cast<long long>(index_value)) + ", " +
          integer_text(static_cast<long long>(type_size(type->child))));
        const string element = new_temp();
        AddInstruction(element + " = index i8 [projection=array_element] " + decay + ", " + offset);
        (void)EmitDestructorAt(element_type, element, scope);
      }
      return;
    }
    if(type->kind == TYPE_CLASS)
      (void)EmitDestructorAt(type, global_address(&global), scope);
  }

void PA14Lowerer::EmitLocalStaticInitialization(VariablePlan* variable, Scope* scope)
{
    if(!variable || !variable->global || !variable->global->dynamic_initializer) return;
    GlobalRecord* object = variable->global;
    GlobalRecord* guard = FindGlobal(object->qualified_name + "__guard");
    if(!guard) return;
    const string loaded = new_temp();
    AddInstruction(loaded + " = load i64 @" + guard->symbol);
    const string initialized = new_temp();
    AddInstruction(initialized + " = cmp ne i64 " + loaded + ", 0");
    const string ready = new_label("local_static_ready");
    const string init = new_label("local_static_init");
    Terminate("branch " + initialized + ", ^" + ready + ", ^" + init);
    AddBlock(init);
    EmitInitializer(variable, variable->initializer, scope);
    if(!state_->current->terminated) {
      AddInstruction("store i64 1, @" + guard->symbol);
      Terminate("jump ^" + ready);
    }
    AddBlock(ready);
  }

} // namespace cppgm_pa14_lowering
