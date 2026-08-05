#include "pa14_lowering.h"

#include <algorithm>
#include <sstream>

using namespace std;

namespace cppgm_pa14_lowering
{

namespace {

CPPGMAstNodePtr range_runtime_identifier(const string& name)
{
  return CPPGMAstNodePtr(new CPPGMAstNode("id-expression", name));
}

CPPGMAstNodePtr range_runtime_unary(const string& op, const string& name)
{
  CPPGMAstNodePtr result(new CPPGMAstNode("unary-expression", "range:" + op));
  result->children.push_back(range_runtime_identifier(name));
  return result;
}

CPPGMAstNodePtr range_runtime_binary(const string& op, const string& left,
                                     const string& right)
{
  CPPGMAstNodePtr result(new CPPGMAstNode("binary-expression", "range:" + op));
  result->children.push_back(range_runtime_identifier(left));
  result->children.push_back(range_runtime_identifier(right));
  return result;
}

CPPGMAstNodePtr range_identifier(const string& name)
{
  return CPPGMAstNodePtr(new CPPGMAstNode("id-expression", name));
}

CPPGMAstNodePtr range_unary(const string& op, const CPPGMAstNodePtr& child)
{
  CPPGMAstNodePtr result(new CPPGMAstNode("unary-expression", "range:" + op));
  result->children.push_back(child);
  return result;
}

CPPGMAstNodePtr range_call(const CPPGMAstNodePtr& object, const string& name,
                           bool member)
{
  CPPGMAstNodePtr callee;
  if(member) {
    callee.reset(new CPPGMAstNode("member-expression", "OP_DOT:."));
    callee->children.push_back(object);
    callee->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", name)));
  } else callee = range_identifier(name);
  CPPGMAstNodePtr result(new CPPGMAstNode("call-expression"));
  result->children.push_back(callee);
  CPPGMAstNodePtr arguments(new CPPGMAstNode("argument-list"));
  if(!member) arguments->children.push_back(object);
  result->children.push_back(arguments);
  return result;
}

CPPGMAstNodePtr range_initializer(const CPPGMAstNodePtr& expression)
{
  CPPGMAstNodePtr result(new CPPGMAstNode("initializer"));
  result->initializer_form = AST_INITIALIZER_COPY;
  result->children.push_back(expression);
  return result;
}

CPPGMAstNodePtr range_subscript(const string& object, const string& index)
{
  CPPGMAstNodePtr result(new CPPGMAstNode("subscript-expression"));
  result->children.push_back(range_identifier(object));
  result->children.push_back(range_identifier(index));
  return result;
}

} // namespace

void PA14Lowerer::EmitReturn(const CPPGMAstNodePtr& node, Scope* scope)
{
    TypePtr return_type = SourceReturnType(*state_->record);
    const auto finish_return_boundary = [&]() {
      // A return from a handler closes the active catch before the ordinary
      // live-object walk.  A return from a try body closes its source EH
      // region after that walk, so destructors remain outside the handler.
      FinishExceptionHandlerForReturn();
      EmitLiveDestructors(scope);
      FinishExceptionTryForReturn();
    };
    if(state_->record->indirect_result) {
      if(!return_type || type_value(return_type)->kind != TYPE_CLASS)
        throw logic_error("indirect result is not a class value");
      if(node->children.empty()) {
        finish_return_boundary();
        Terminate("return void");
        return;
      }
      const vector<string> names = ParameterNames(*state_->record);
      if(names.empty()) throw logic_error("indirect result has no destination");
      const CPPGMAstNodePtr expression = node->children[0];
      if(expression->kind == "id-expression" && state_->return_slot_plan &&
         FindLocalPlan(expression->value) == state_->return_slot_plan) {
        // A named local returned through the indirect result slot is elided,
        // but its class type still has an implicit copy frontier.  Demand the
        // typed member operations after the local has been lowered so this
        // late metadata does not change the constructor path being emitted.
        TypePtr returned_type = type_value(return_type);
        if(returned_type && returned_type->kind == TYPE_CLASS) {
          for(size_t member = 0; member < returned_type->class_members.size(); ++member) {
            const ClassMemberInfo& field = returned_type->class_members[member];
            if(field.is_static || !field.type) continue;
            TypePtr field_type = type_value(field.type);
            while(field_type && field_type->kind == TYPE_ARRAY)
              field_type = type_value(field_type->child);
            if(!field_type || field_type->kind != TYPE_CLASS ||
               !HasDestructor(field_type)) continue;
            FunctionRecord* copy = EnsureImplicitCopyConstructor(field_type, false);
            if(copy && !copy->deleted) MarkFunctionNeeded(copy);
          }
        }
        finish_return_boundary();
        Terminate("return void");
        return;
      }
      const ExprInfo expression_info = Infer(expression, scope);
      TypePtr move_source_type = type_value(expression_info.type);
      if(expression_info.type && type_is_reference(expression_info.type))
        move_source_type = type_value(expression_info.type->child);
      const bool implicit_return_move = expression->kind == "id-expression" &&
        FindLocalPlan(expression->value) && move_source_type &&
        !move_source_type->is_const;
      const bool suppress_return_move_unwind = expression &&
        expression->kind == "id-expression";
      const bool previous_return_unwind_suppression = state_ &&
        state_->suppress_constructor_unwind;
      if(state_) state_->suppress_constructor_unwind =
        previous_return_unwind_suppression || suppress_return_move_unwind;
      bool transferred = false;
      try {
        transferred = EmitObjectTransferAt(type_value(return_type), "%" + names[0],
          expression, scope, true, implicit_return_move);
      } catch(...) {
        if(state_) state_->suppress_constructor_unwind =
          previous_return_unwind_suppression;
        throw;
      }
      if(state_) state_->suppress_constructor_unwind =
        previous_return_unwind_suppression;
      if(!transferred) throw logic_error("no viable return value transfer");
      finish_return_boundary();
      Terminate("return void");
      return;
    }
    if(!return_type || low_type(return_type) == "void") {
      if(!node->children.empty()) EmitDiscard(node->children[0], scope);
      finish_return_boundary();
      Terminate("return void");
      return;
    }
    if(node->children.empty()) {
      finish_return_boundary();
      Terminate("return " + low_type(return_type) + " 0");
      return;
    }
    CPPGMAstNodePtr expression = node->children[0];
    if(type_is_reference(return_type)) {
      const ExprInfo expression_info = Infer(expression, scope);
      const TypePtr source_type = expression_value_type(expression_info);
      const TypePtr target_type = type_value(return_type);
      string address;
      if(expression->kind == "literal" && target_type &&
         target_type->kind == TYPE_FUNDAMENTAL) {
        const string slot = new_special_slot("retref", low_type(target_type));
        Value value = EmitValue(expression, scope, target_type);
        value = ConvertValue(value, target_type, false, true);
        emit_store(target_type, value.operand, "$" + slot);
        address = new_temp();
        AddInstruction(address + " = addr $" + slot);
      } else {
        address = EmitAddress(expression, scope);
      }
      if(source_type && target_type && source_type->kind == TYPE_CLASS &&
         target_type->kind == TYPE_CLASS &&
         IsDerivedFrom(source_type, target_type))
        address = AdjustBaseAddress(address, source_type, target_type);
      finish_return_boundary();
      Terminate("return ptr " + address);
      return;
    }
    TypePtr return_value_type = type_value(return_type);
    if(return_value_type && return_value_type->kind == TYPE_CLASS) {
      if(state_->return_object_slot.empty())
        state_->return_object_slot = new_special_slot("retobj", low_type(return_value_type));
      const string slot = state_->return_object_slot;
      const string destination = new_temp();
      AddInstruction(destination + " = addr $" + slot);
      const ExprInfo expression_info = Infer(expression, scope);
      TypePtr move_source_type = type_value(expression_info.type);
      if(expression_info.type && type_is_reference(expression_info.type))
        move_source_type = type_value(expression_info.type->child);
      const bool implicit_return_move = expression->kind == "id-expression" &&
        FindLocalPlan(expression->value) && move_source_type &&
        !move_source_type->is_const;
      const bool suppress_return_move_unwind = expression &&
        expression->kind == "id-expression";
      const bool previous_return_unwind_suppression = state_ &&
        state_->suppress_constructor_unwind;
      if(state_) state_->suppress_constructor_unwind =
        previous_return_unwind_suppression || suppress_return_move_unwind;
      bool transferred = false;
      try {
        transferred = EmitObjectTransferAt(return_value_type, destination, expression,
          scope, true, implicit_return_move);
      } catch(...) {
        if(state_) state_->suppress_constructor_unwind =
          previous_return_unwind_suppression;
        throw;
      }
      if(state_) state_->suppress_constructor_unwind =
        previous_return_unwind_suppression;
      if(!transferred) throw logic_error("no viable direct class return transfer");
      finish_return_boundary();
      Terminate("return " + low_type(return_type) + " $" + slot);
      return;
    }
    Value value = EmitValue(expression, scope, return_type);
    const bool preserve_sizeof_type = expression->kind == "sizeof-expression" ||
      expression->kind == "sizeof-pack-expression" ||
      expression->kind == "type-trait-expression";
    if(value.known_constant && is_integral_type(value.type) && is_integral_type(return_type) &&
       !preserve_sizeof_type &&
       (type_size(return_type) <= type_size(value.type) ||
        (!is_unsigned_type(return_type) && type_size(return_type) > type_size(value.type)))) {
      // A known integral return value is already folded by the semantic
      // evaluator; retain it as an immediate across the ordinary integral
      // conversion boundary.
      finish_return_boundary();
      Terminate("return " + low_type(return_type) + " " + integer_text(value.constant));
      return;
    }
    value = ConvertValue(value, return_type, true, true);
    const bool enum_functional_cast = return_value_type &&
      return_value_type->kind == TYPE_ENUM && expression &&
      expression->kind == "call-expression" && expression->children.size() > 0 &&
      expression->children[0] && expression->children[0]->kind == "id-expression" &&
      expression->children[0]->value == LastComponent(return_value_type->name) &&
      value.type && low_type(value.type) == low_type(return_type);
    if(enum_functional_cast) {
      const string copied = new_temp();
      AddInstruction(copied + " = copy " + low_type(return_type) + " " + value.operand);
      value.operand = copied;
    }
    finish_return_boundary();
    Terminate("return " + low_type(return_type) + " " + value.operand);
  }

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
      else if(expression && expression->kind == "call-expression" &&
              expression->children.size() > 1 && expression->children[1])
        arguments = expression->children[1]->children;
      if(!expression && !HasConstructor(value_type)) return;
      if((!HasDefaultInitializationEffects(value_type) ||
          HasElidedTemplateInitialization(value_type)) && !HasDestructor(value_type)) {
        // Zero-storage objects with a user-declared constructor still
        // participate in the initialization frontier through their address,
        // even when no constructor action is needed.  Plain aggregate empty
        // objects have no such runtime frontier of their own.
        const bool generated_member_template = global.node &&
          global.node->template_instantiation &&
          global.node->template_primary.find("::") != string::npos &&
          global.template_owner &&
          global.node->template_arguments.size() >
            global.template_owner->template_arguments.size();
        if(HasConstructor(value_type) || generated_member_template)
          (void)global_address(&global);
        return;
      }
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
void PA14Lowerer::PlanRangeFor(const CPPGMAstNodePtr& node, Scope* scope)
{
    if(!node || node->children.size() < 3) throw logic_error("invalid range-for statement");
    const CPPGMAstNodePtr declaration = node->children[0];
    const CPPGMAstNodePtr initializer = node->children[1];
    const CPPGMAstNodePtr expression = initializer && !initializer->children.empty() ?
      initializer->children[0] : CPPGMAstNodePtr();
    if(!declaration || declaration->children.size() < 2 || !expression)
      throw logic_error("invalid range-for declaration");
    const string loop_name = FirstIdentifier(declaration->children[1]);
    if(loop_name.empty()) throw logic_error("range-for declaration has no name");
    const bool braced = expression->kind == "braced-init-list";
    string range_kind;
    string first_hidden;
    string second_hidden;
    unsigned int hidden = ++state_->variable_name_counts["\001range-hidden"];
    TypePtr element_type;
    CPPGMAstNodePtr element_expression;
    if(braced) {
      if(expression->children.empty()) throw logic_error("empty range initializer");
      element_type = expression_value_type(Infer(expression->children[0], scope));
      if(!element_type) throw logic_error("range initializer has no element type");
      first_hidden = "__range" + integer_text(static_cast<long long>(hidden));
      second_hidden = "__idx" + integer_text(static_cast<long long>(hidden + 1));
      range_kind = "braced";
      AddVariablePlan(first_hidden, ArrayOf(
        static_cast<long long>(expression->children.size()), type_value(element_type)),
        CPPGMAstNodePtr(), range_initializer(expression));
      AddVariablePlan(second_hidden, Fundamental("int"), CPPGMAstNodePtr(),
        range_initializer(CPPGMAstNodePtr(new CPPGMAstNode("literal", "0"))));
      element_expression = range_subscript(first_hidden, second_hidden);
    } else {
      ExprInfo source_info = Infer(expression, scope);
      TypePtr source_type = expression_value_type(source_info);
      if(!source_type) throw logic_error("range expression has no type");
      if(source_type->kind == TYPE_ARRAY) {
        if(source_type->bound < 0) throw logic_error("range array has unknown bound");
        first_hidden = "__idx" + integer_text(static_cast<long long>(hidden));
        range_kind = "array";
        element_type = type_value(source_type->child);
        AddVariablePlan(first_hidden, Fundamental("int"), CPPGMAstNodePtr(),
          range_initializer(CPPGMAstNodePtr(new CPPGMAstNode("literal", "0"))));
        element_expression = range_subscript(expression->value, first_hidden);
      } else if(IsInitializerListType(source_type)) {
        first_hidden = expression->kind == "id-expression" ? expression->value :
          "__range" + integer_text(static_cast<long long>(hidden));
        second_hidden = "__idx" + integer_text(static_cast<long long>(hidden));
        range_kind = "initializer-list";
        element_type = InitializerListElementType(source_type, scope);
        if(!element_type) throw logic_error("initializer_list has no element type");
        if(first_hidden.compare(0, 7, "__range") == 0)
          AddVariablePlan(first_hidden, source_type, CPPGMAstNodePtr(),
            range_initializer(expression));
        AddVariablePlan(second_hidden, Fundamental("int"), CPPGMAstNodePtr(),
          range_initializer(CPPGMAstNodePtr(new CPPGMAstNode("literal", "0"))));
        element_expression.reset(new CPPGMAstNode("initializer-list-element"));
        element_expression->children.push_back(expression);
        element_expression->children.push_back(range_identifier(second_hidden));
      } else if(source_type->kind == TYPE_CLASS) {
        const bool member_begin = !MemberBindings(source_type, "begin").empty();
        const bool member_end = !MemberBindings(source_type, "end").empty();
        if(member_begin != member_end) throw logic_error("range begin/end mismatch");
        first_hidden = "__begin" + integer_text(static_cast<long long>(hidden));
        second_hidden = "__end" + integer_text(static_cast<long long>(hidden + 1));
        range_kind = member_begin ? "member" : "adl";
        const size_t namespace_separator = source_type->name.rfind("::");
        const string associated_namespace = !member_begin &&
          namespace_separator != string::npos ?
          source_type->name.substr(0, namespace_separator) : string();
        const string begin_name = associated_namespace.empty() ? "begin" :
          associated_namespace + "::begin";
        const string end_name = associated_namespace.empty() ? "end" :
          associated_namespace + "::end";
        const CPPGMAstNodePtr begin = range_call(expression, begin_name, member_begin);
        const CPPGMAstNodePtr end = range_call(expression, end_name, member_end);
        TypePtr begin_type = expression_value_type(Infer(begin, scope));
        TypePtr end_type = expression_value_type(Infer(end, scope));
        if(!begin_type || !end_type) throw logic_error("range begin/end has no type");
        AddVariablePlan(first_hidden, begin_type, CPPGMAstNodePtr(), range_initializer(begin));
        AddVariablePlan(second_hidden, end_type, CPPGMAstNodePtr(), range_initializer(end));
        element_expression = range_unary("*", range_identifier(first_hidden));
        element_type = expression_value_type(Infer(element_expression, scope));
      } else throw logic_error("unsupported range expression");
    }
    if(!element_type) throw logic_error("range has no element type");
    Analyzer::SpecFacts facts;
    TypePtr declared = analyzer_.TypeFromSpecSeq(declaration->children[0], scope, &facts);
    declared = analyzer_.BuildDeclarator(declaration->children[1], declared, scope);
    TypePtr loop_type = declared;
    if(ContainsAutoType(declared))
      loop_type = DeduceAutoType(declared, range_initializer(element_expression), scope);
    AddVariablePlan(loop_name, loop_type, declaration->children[1],
      range_initializer(element_expression));
    state_->environments.push_back(map<string, VariablePlan*>());
    for(size_t i = 0; i < state_->variables.size(); ++i) {
      VariablePlan& variable = state_->variables[i];
      if(variable.source_name == first_hidden || variable.source_name == second_hidden ||
         variable.source_name == loop_name)
        state_->environments.back()[variable.source_name] = &variable;
    }
    PlanStatement(node->children[2], scope);
    state_->environments.pop_back();
    node->value = range_kind + "|" + first_hidden + "|" + second_hidden;
  }

void PA14Lowerer::EmitRangeFor(const CPPGMAstNodePtr& node, Scope* scope)
{
    if(!node || node->children.size() < 3) throw logic_error("invalid range-for statement");
    const size_t first_separator = node->value.find('|');
    const size_t second_separator = first_separator == string::npos ? string::npos :
      node->value.find('|', first_separator + 1);
    if(first_separator == string::npos || second_separator == string::npos)
      throw logic_error("range-for plan is missing hidden names");
    const string kind = node->value.substr(0, first_separator);
    const string first_name = node->value.substr(first_separator + 1,
      second_separator - first_separator - 1);
    const string second_name = node->value.substr(second_separator + 1);
    const string loop_name = FirstIdentifier(node->children[0]->children[1]);
    state_->environments.push_back(map<string, VariablePlan*>());
    VariablePlan* first = 0;
    VariablePlan* second = 0;
    VariablePlan* loop = 0;
    for(size_t i = state_->variables.size(); i > 0; --i) {
      VariablePlan& variable = state_->variables[i - 1];
      if(variable.source_name == first_name && !first) first = &variable;
      if(variable.source_name == second_name && !second) second = &variable;
      if(variable.source_name == loop_name && !loop) loop = &variable;
    }
    if(!first || !loop || (kind != "array" && (!second || second_name.empty())))
      throw logic_error("range-for plan has no hidden variables");
    state_->environments.back()[first_name] = first;
    if(second) state_->environments.back()[second_name] = second;
    state_->environments.back()[loop_name] = loop;
    const auto declare_slot = [&](VariablePlan* variable) {
      if(variable && !variable->slot_declared) {
        variable->slot_declared = true;
        state_->slot_order.push_back(FunctionState::SlotEntry(
          false, variable->slot_name, variable));
      }
    };
    declare_slot(first);
    if(kind != "initializer-list") EmitInitializer(first, first->initializer, scope);
    if(second) {
      declare_slot(second);
      EmitInitializer(second, second->initializer, scope);
    }
    const string condition_label = new_label("for_cond");
    const string body_label = new_label("for_body");
    const string iteration_label = new_label("for_iter");
    const string end_label = new_label("for_end");
    if(!state_->current->terminated) Terminate("jump ^" + condition_label);
    AddBlock(condition_label);
    const string index_name = kind == "braced" || kind == "initializer-list" ? second_name : first_name;
    CPPGMAstNodePtr condition;
    if(kind == "array" || kind == "braced") {
      const CPPGMAstNodePtr expression = node->children[1]->children.empty() ?
        CPPGMAstNodePtr() : node->children[1]->children[0];
      long long bound = 0;
      if(kind == "braced") bound = expression && expression->kind == "braced-init-list" ?
        static_cast<long long>(expression->children.size()) : 0;
      else {
        TypePtr source_type = expression_value_type(Infer(expression, scope));
        bound = source_type ? source_type->bound : 0;
      }
      condition = range_runtime_binary("<", index_name, integer_text(bound));
      // The right operand is a literal spelling, not an identifier.
      condition->children[1].reset(new CPPGMAstNode("literal", integer_text(bound)));
    } else if(kind == "initializer-list") {
      const CPPGMAstNodePtr expression = node->children[1]->children.empty() ?
        CPPGMAstNodePtr() : node->children[1]->children[0];
      condition = range_runtime_binary("<", index_name, string());
      condition->children[1].reset(new CPPGMAstNode("initializer-list-size"));
      condition->children[1]->children.push_back(expression);
    } else condition = range_runtime_binary("!=", first_name, second_name);
    EmitCondition(condition, scope, body_label, end_label);
    AddBlock(body_label);
    state_->break_targets.push_back(end_label);
    state_->continue_targets.push_back(iteration_label);
    declare_slot(loop);
    if(type_is_reference(loop->type)) {
      const CPPGMAstNodePtr element = InitializerExpression(loop->initializer);
      const ExprInfo source_info = Infer(element, scope);
      const TypePtr source_type = expression_value_type(source_info);
      const TypePtr target_type = type_value(loop->type);
      string address;
      if(source_info.category == "prvalue" && target_type &&
         target_type->kind != TYPE_CLASS) {
        const string temporary = new_special_slot("tmpref", low_type(target_type));
        Value value = EmitValue(element, scope, target_type);
        value = ConvertValue(value, target_type, false, true);
        emit_store(target_type, value.operand, "$" + temporary);
        address = new_temp();
        AddInstruction(address + " = addr $" + temporary);
      } else if(source_info.category == "prvalue" && target_type &&
                target_type->kind == TYPE_CLASS)
        address = EmitReferenceArgument(element, scope, loop->type);
      else address = EmitAddress(element, scope);
      if(source_type && target_type && source_type->kind == TYPE_CLASS &&
         target_type->kind == TYPE_CLASS && IsDerivedFrom(source_type, target_type))
        address = AdjustBaseAddress(address, source_type, target_type);
      emit_store(PointerTo(Fundamental("char")), address, StorageForVariable(*loop));
    }
    else {
      const CPPGMAstNodePtr element = InitializerExpression(loop->initializer);
      Value value = EmitValue(element, scope, type_value(loop->type));
      if(value.lvalue && value.type) {
        value.operand = emit_load(value.operand, value.type);
        value.lvalue = false;
      } else if(value.type && type_is_reference(value.type)) {
        value.operand = emit_load(value.operand, value.type->child);
        value.type = value.type->child;
      } else if(value.type && value.type->kind == TYPE_POINTER && element &&
                element->kind == "unary-expression" &&
                PA12Operator(element->value) == "*") {
        value.operand = emit_load(value.operand, value.type->child);
        value.type = value.type->child;
      }
      value = ConvertValue(value, type_value(loop->type), false, true);
      StoreLValue(range_runtime_identifier(loop_name), scope,
        type_value(loop->type), value.operand);
    }
    EmitStatement(node->children[2], scope);
    state_->continue_targets.pop_back();
    state_->break_targets.pop_back();
    if(!state_->current->terminated) Terminate("jump ^" + iteration_label);
    AddBlock(iteration_label);
    EmitDiscard(range_runtime_unary("++", index_name), scope);
    if(!state_->current->terminated) Terminate("jump ^" + condition_label);
    AddBlock(end_label);
    map<string, VariablePlan*>& environment = state_->environments.back();
    for(size_t i = state_->variables.size(); i > 0; --i) {
      VariablePlan& variable = state_->variables[i - 1];
      bool bound_here = false;
      for(map<string, VariablePlan*>::const_iterator it = environment.begin();
          it != environment.end(); ++it)
        if(it->second == &variable) { bound_here = true; break; }
      if(!bound_here || type_is_reference(variable.type)) continue;
      TypePtr object_type = type_value(variable.type);
      if(object_type && object_type->kind == TYPE_CLASS &&
         DestructorHasEffects(object_type))
        (void)EmitDestructorAt(object_type, local_address(&variable), scope);
    }
    LeaveEnvironment();
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
    if(variable->initializer) EmitInitializer(variable, variable->initializer, scope);
    else {
      TypePtr type = type_value(object->type);
      if(type && type->kind == TYPE_CLASS)
        (void)EmitObjectConstructor(variable, type, vector<CPPGMAstNodePtr>(), scope);
    }
    if(!state_->current->terminated) {
      AddInstruction("store i64 1, @" + guard->symbol);
      Terminate("jump ^" + ready);
    }
    AddBlock(ready);
  }

void PA14Lowerer::EmitDynamicInitializers(vector<string>& entries)
{
    vector<GlobalRecord*> initializers;
    vector<GlobalRecord*> tls_initializers;
    vector<GlobalRecord*> finalizers;
    for(size_t i = 0; i < globals_.size(); ++i) {
      if(globals_[i].dynamic_initializer && !globals_[i].local_static) {
        if(globals_[i].thread_local_storage) tls_initializers.push_back(&globals_[i]);
        else initializers.push_back(&globals_[i]);
      }
      if(globals_[i].dynamic_finalizer && !globals_[i].local_static)
        finalizers.push_back(&globals_[i]);
    }
    if(initializers.empty() && tls_initializers.empty() && finalizers.empty()) return;
    stable_partition(initializers.begin(), initializers.end(),
      [](GlobalRecord* object) { return object && type_is_reference(object->type); });
    const auto render = [](FunctionState& state, const string& name,
                           const string& metadata) -> string {
      ostringstream out;
      out << "function @" << name << "() -> void";
      if(!metadata.empty()) out << " [" << metadata << "]";
      out << " {\n";
      for(size_t i = 0; i < state.special_slots.size(); ++i)
        out << "  slot $" << state.special_slots[i] << " : " <<
          state.special_slot_types[state.special_slots[i]] << "\n";
      if(!state.special_slots.empty()) out << "\n";
      for(size_t i = 0; i < state.blocks.size(); ++i) {
        if(i != 0) out << "\n";
        out << "  block ^" << state.blocks[i].label << ":\n";
        for(size_t j = 0; j < state.blocks[i].lines.size(); ++j)
          out << state.blocks[i].lines[j] << "\n";
      }
      out << "}";
      return out.str();
    };
    if(!initializers.empty()) {
      FunctionRecord helper;
      helper.scope = analyzer_.global_.get();
      helper.type = FunctionOf(vector<TypePtr>(), false, Fundamental("void"), false);
      helper.qualified_name = "__cppgm_init";
      helper.symbol = "__cppgm_init";
      helper.definition = true;
      FunctionState state(this, &helper);
      state_ = &state;
      state.environments.push_back(map<string, VariablePlan*>());
      AddBlock("entry");
      for(size_t i = 0; i < initializers.size() && !state.current->terminated; ++i)
        EmitGlobalInitializer(*initializers[i], initializers[i]->scope);
      if(!state.current->terminated) Terminate("return void");
      entries.push_back(render(state, "__cppgm_init", "role=init, binding=internal"));
      state_ = 0;
    }
    for(size_t i = 0; i < tls_initializers.size(); ++i) {
      GlobalRecord* object = tls_initializers[i];
      GlobalRecord* guard = FindGlobal(object->qualified_name + "__tls_guard");
      if(!guard) {
        if(initializers.empty()) initializers.push_back(object);
        continue;
      }
      FunctionRecord helper;
      helper.scope = analyzer_.global_.get();
      helper.type = FunctionOf(vector<TypePtr>(), false, Fundamental("void"), false);
      helper.qualified_name = object->qualified_name + "__tls_init";
      helper.symbol = object->symbol + "__tls_init";
      helper.definition = true;
      FunctionState state(this, &helper);
      state_ = &state;
      state.environments.push_back(map<string, VariablePlan*>());
      AddBlock("entry");
      const string loaded = new_temp();
      AddInstruction(loaded + " = load i64 @" + guard->symbol);
      const string initialized = new_temp();
      AddInstruction(initialized + " = cmp ne i64 " + loaded + ", 0");
      AddInstruction("branch " + initialized + ", ^local_static_ctor_done_2, ^local_static_ctor_run_1");
      AddBlock("local_static_ctor_run_1");
      EmitGlobalInitializer(*object, object->scope);
      if(!state.current->terminated) {
        AddInstruction("store i64 1, @" + guard->symbol);
        Terminate("jump ^local_static_ctor_done_2");
      }
      AddBlock("local_static_ctor_done_2");
      if(!state.current->terminated) Terminate("return void");
      entries.push_back(render(state, helper.symbol, "binding=internal"));
      state_ = 0;
    }
    if(!finalizers.empty()) {
      FunctionRecord helper;
      helper.scope = analyzer_.global_.get();
      helper.type = FunctionOf(vector<TypePtr>(), false, Fundamental("void"), false);
      helper.qualified_name = "__cppgm_fini";
      helper.symbol = "__cppgm_fini";
      helper.definition = true;
      FunctionState state(this, &helper);
      state_ = &state;
      state.environments.push_back(map<string, VariablePlan*>());
      AddBlock("entry");
      for(size_t i = finalizers.size(); i > 0 && !state.current->terminated; --i)
        EmitGlobalFinalizer(*finalizers[i - 1], finalizers[i - 1]->scope);
      if(!state.current->terminated) Terminate("return void");
      entries.push_back(render(state, "__cppgm_fini", "role=fini"));
      state_ = 0;
    }
  }

} // namespace cppgm_pa14_lowering
