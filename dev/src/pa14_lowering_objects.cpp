#include "pa14_lowering.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace std;

namespace cppgm_pa14_lowering {

void PA14Lowerer::EmitAggregateArrayAt(const string& base, const TypePtr& raw_type,
                                        const CPPGMAstNodePtr& expression, Scope* scope)
{
    TypePtr type = type_value(raw_type);
    if(!type || type->kind != TYPE_ARRAY || !expression ||
       expression->kind != "braced-init-list") return;
    for(size_t i = 0; i < expression->children.size(); ++i) {
      string element = base;
      if(i != 0) {
        const string index = new_temp();
        AddInstruction(index + " = index i8 " + base + ", " +
          integer_text(static_cast<long long>(i * type_size(type->child))));
        element = index;
      }
      const CPPGMAstNodePtr child = expression->children[i];
      TypePtr child_type = type_value(type->child);
      if(child_type && child_type->kind == TYPE_CLASS) {
        vector<CPPGMAstNodePtr> arguments = child && child->kind == "braced-init-list" ?
          child->children : vector<CPPGMAstNodePtr>();
        if(arguments.empty() && child && child->kind != "braced-init-list")
          arguments.push_back(child);
        if(EmitConstructorAt(child_type, element, arguments, scope)) continue;
        if(child && child->kind == "braced-init-list") {
          EmitAggregateAt(element, child_type, child, scope);
          continue;
        }
      }
      if(child_type && child_type->kind == TYPE_ARRAY &&
         child && child->kind == "braced-init-list") {
        EmitAggregateAt(element, child_type, child, scope);
        continue;
      }
      if(type_is_reference(type->child)) {
        emit_store(PointerTo(Fundamental("char")),
          EmitReferenceArgument(child, scope, type->child), element);
        continue;
      }
      Value value = EmitValue(child, scope, child_type);
      if(value.known_constant && is_integral_type(value.type) &&
         is_integral_type(child_type)) {
        value.type = child_type;
        value.operand = integer_text(value.constant);
      } else value = ConvertValue(value, child_type);
      emit_store(child_type, value.operand, element);
    }
  }

void PA14Lowerer::EmitAggregateClassFields(const string& base, const TypePtr& raw_type,
                                           const CPPGMAstNodePtr& expression, Scope* scope,
                                           const CPPGMAstNodePtr& refresh_node,
                                           size_t* child_index)
{
    TypePtr type = type_value(raw_type);
    if(!type || type->kind != TYPE_CLASS || !expression ||
       expression->kind != "braced-init-list" || !child_index) return;
    bool bitfield_storage_initialized = false;
    for(size_t i = 0; i < type->class_members.size() &&
        *child_index < expression->children.size(); ++i) {
      const ClassMemberInfo& member = type->class_members[i];
      if(member.is_static || member.name.empty() || !member.type) continue;
      const CPPGMAstNodePtr child = expression->children[(*child_index)++];
      TypePtr child_type = type_value(member.type);
      Binding* member_binding = 0;
      vector<Binding*> member_bindings = DirectBindings(type->owned_scope, member.name);
      for(size_t j = 0; j < member_bindings.size(); ++j)
        if(member_bindings[j]->kind == BIND_VARIABLE && member_bindings[j]->is_member &&
           !member_bindings[j]->is_static) { member_binding = member_bindings[j]; break; }
      if(member_binding && IsBitField(member_binding)) {
        Value value = EmitValue(child, scope, child_type);
        if(value.known_constant && is_integral_type(value.type) &&
           is_integral_type(child_type)) {
          value.type = child_type;
          value.operand = integer_text(value.constant);
        } else value = ConvertValue(value, child_type);
        string stored;
        if(bitfield_storage_initialized) {
          const string read_base = refresh_node ? EmitAddress(refresh_node, scope) : base;
          const string read_address = new_temp();
          AddInstruction(read_address + " = index i8 " + read_base + ", " +
            integer_text(member.offset));
          stored = MergeBitFieldValue(member_binding, read_address, member.type,
            value.operand, true);
        } else stored = PrepareBitFieldValue(member_binding, member.type, value.operand);
        const string store_base = refresh_node ? EmitAddress(refresh_node, scope) : base;
        const string store_address = new_temp();
        AddInstruction(store_address + " = index i8 " + store_base + ", " +
          integer_text(member.offset));
        emit_store(member.type, stored, store_address);
        bitfield_storage_initialized = true;
        continue;
      }
      if(type_is_reference(member.type)) {
        const string reference = EmitReferenceArgument(child, scope, member.type);
        const string field_base = refresh_node ? EmitAddress(refresh_node, scope) : base;
        const string field = new_temp();
        AddInstruction(field + " = index i8 [projection=field] " + field_base + ", " +
          integer_text(member.offset));
        emit_store(PointerTo(Fundamental("char")), reference, field);
        continue;
      }
      if(child_type && child_type->kind == TYPE_ARRAY &&
         child && child->kind != "braced-init-list" && child_type->bound >= 0) {
        const bool string_child = child->kind == "literal" && !child->value.empty() &&
          child->value[0] == '"';
        const vector<unsigned char> bytes = string_child ? decode_string_literal(child->value) :
          vector<unsigned char>();
        Value scalar_value;
        if(!string_child) scalar_value = EmitValue(child, scope, child_type->child);
        for(size_t element_index = 0;
            element_index < static_cast<size_t>(child_type->bound); ++element_index) {
          string field;
          const string field_base = refresh_node ? EmitAddress(refresh_node, scope) : base;
          field = new_temp();
          AddInstruction(field + " = index i8 [projection=field] " + field_base + ", " +
            integer_text(member.offset));
          const string decay = new_temp();
          AddInstruction(decay + " = unary decay ptr " + field);
          const string element = new_temp();
          AddInstruction(element + " = index " + low_type(child_type->child) + " " +
            decay + ", " + integer_text(static_cast<long long>(element_index)));
          if(string_child) {
            const unsigned char byte = element_index < bytes.size() ? bytes[element_index] : 0;
            emit_store(child_type->child, integer_text(byte), element);
          } else if(element_index == 0) {
            if(scalar_value.known_constant && is_integral_type(scalar_value.type) &&
               is_integral_type(child_type->child)) {
              scalar_value.type = child_type->child;
              scalar_value.operand = integer_text(scalar_value.constant);
            } else scalar_value = ConvertValue(scalar_value, child_type->child);
            emit_store(child_type->child, scalar_value.operand, element);
          } else emit_store(child_type->child, "0", element);
        }
        continue;
      }
      const string field_base = refresh_node ? EmitAddress(refresh_node, scope) : base;
      const string field = new_temp();
      AddInstruction(field + " = index i8 [projection=field] " + field_base + ", " +
        integer_text(member.offset));
      if(child_type && child_type->kind == TYPE_CLASS) {
        vector<CPPGMAstNodePtr> arguments = child && child->kind == "braced-init-list" ?
          child->children : vector<CPPGMAstNodePtr>();
        if(arguments.empty() && child && child->kind != "braced-init-list")
          arguments.push_back(child);
        if(EmitConstructorAt(child_type, field, arguments, scope)) continue;
        if(child && child->kind == "braced-init-list") {
          EmitAggregateAt(field, child_type, child, scope);
          continue;
        }
      }
      if(child_type && child_type->kind == TYPE_ARRAY &&
         child && child->kind == "braced-init-list") {
        EmitAggregateAt(field, child_type, child, scope);
        continue;
      }
      Value value = EmitValue(child, scope, child_type);
      if(value.known_constant && is_integral_type(value.type) &&
         is_integral_type(child_type)) {
        value.type = child_type;
        value.operand = integer_text(value.constant);
      } else value = ConvertValue(value, child_type);
      emit_store(child_type, value.operand, field);
    }
  }

void PA14Lowerer::EmitAggregateClassDefaults(const string& base, const TypePtr& raw_type,
                                             const CPPGMAstNodePtr& expression, Scope* scope,
                                             const CPPGMAstNodePtr& refresh_node,
                                             size_t child_index)
{
    TypePtr type = type_value(raw_type);
    if(!type || type->kind != TYPE_CLASS || !expression ||
       expression->kind != "braced-init-list") return;
    size_t consumed = 0;
    for(size_t i = 0; i < type->class_members.size(); ++i) {
      const ClassMemberInfo& member = type->class_members[i];
      if(member.is_static || member.name.empty() || !member.type) continue;
      if(consumed++ < child_index) continue;
      const string field_base = refresh_node ? EmitAddress(refresh_node, scope) : base;
      const string field = new_temp();
      AddInstruction(field + " = index i8 [projection=field] " + field_base + ", " +
        integer_text(member.offset));
      TypePtr member_type = type_value(member.type);
      CPPGMAstNodePtr member_expression = member.initializer ?
        InitializerExpression(member.initializer) : CPPGMAstNodePtr();
      vector<CPPGMAstNodePtr> arguments;
      if(member.initializer && !member.initializer->children.empty() &&
         member.initializer->children[0] &&
         member.initializer->children[0]->kind == "paren-initializer")
        arguments = member.initializer->children[0]->children;
      else if(member_expression && member_expression->kind == "braced-init-list")
        arguments = member_expression->children;
      else if(member_expression) arguments.push_back(member_expression);
      if(member_type && member_type->kind == TYPE_CLASS) {
        if(EmitConstructorAt(member_type, field, arguments, scope)) continue;
        if(member_expression && member_expression->kind == "braced-init-list") {
          EmitAggregateAt(field, member_type, member_expression, scope);
          continue;
        }
      }
      if(type_is_reference(member.type)) {
        if(member_expression) emit_store(PointerTo(Fundamental("char")),
          EmitReferenceArgument(member_expression, scope, member.type), field);
        continue;
      }
      if(member_expression) {
        Value value = EmitValue(member_expression, scope, member.type);
        value = ConvertValue(value, member.type);
        emit_store(member.type, value.operand, field);
      } else if(!member_type || member_type->kind != TYPE_CLASS) {
        emit_store(member.type, "0", field);
      }
    }
  }

void PA14Lowerer::EmitAggregateAt(const string& base, const TypePtr& raw_type,
                                  const CPPGMAstNodePtr& expression, Scope* scope,
                                  const CPPGMAstNodePtr& refresh_node)
{
    if(!expression) return;
    TypePtr type = type_value(raw_type);
    if(!type) return;
    if(expression->kind == "parenthesized-expression" && !expression->children.empty()) {
      EmitAggregateAt(base, type, expression->children[0], scope, refresh_node);
      return;
    }
    if(refresh_node && base.empty()) (void)EmitAddress(refresh_node, scope);
    if(type->kind == TYPE_ARRAY && expression->kind == "braced-init-list") {
      EmitAggregateArrayAt(base, type, expression, scope);
      return;
    }
    if(type->kind != TYPE_CLASS || expression->kind != "braced-init-list") return;
    size_t child_index = 0;
    EmitAggregateClassFields(base, type, expression, scope, refresh_node, &child_index);
    EmitAggregateClassDefaults(base, type, expression, scope, refresh_node, child_index);
  }

void PA14Lowerer::EmitAggregateConstructorBody(FunctionRecord& function, Scope* scope)
{
    TypePtr owner = type_value(function.member_owner);
    if(!owner) return;
    const vector<string> names = ParameterNames(function);
    CPPGMAstNodePtr this_node(new CPPGMAstNode("keyword-literal", "KW_THIS:this"));
    size_t parameter = 1;
    for(size_t i = 0; i < owner->class_members.size(); ++i) {
      const ClassMemberInfo& member = owner->class_members[i];
      if(member.is_static || member.name.empty() || !member.type) continue;
      if(parameter >= names.size()) break;
      const string value = emit_load("$" + names[parameter], member.type);
      const string this_address = EmitValue(this_node, scope).operand;
      const string address = new_temp();
      AddInstruction(address + " = index i8 " + this_address + ", " +
        integer_text(member.offset));
      Binding* binding = 0;
      vector<Binding*> candidates = DirectBindings(owner->owned_scope, member.name);
      for(size_t j = 0; j < candidates.size(); ++j)
        if(candidates[j]->kind == BIND_VARIABLE && candidates[j]->is_member &&
           !candidates[j]->is_static) { binding = candidates[j]; break; }
      if(binding && IsBitField(binding)) StoreBitField(binding, address, member.type, value, true);
      else emit_store(member.type, value, address);
      ++parameter;
    }
  }

void PA14Lowerer::EmitDestructorBody(FunctionRecord& function, Scope* scope)
{
    TypePtr owner = type_value(function.member_owner);
    if(!owner) return;
    CPPGMAstNodePtr this_node(new CPPGMAstNode("keyword-literal", "KW_THIS:this"));
    for(size_t i = owner->class_members.size(); i > 0; --i) {
      const ClassMemberInfo& member = owner->class_members[i - 1];
      TypePtr member_type = type_value(member.type);
      if(member.is_static || member.name.empty() || !member_type ||
         member_type->kind != TYPE_CLASS) continue;
      CPPGMAstNodePtr expression(new CPPGMAstNode("member-expression", "OP_ARROW:->"));
      expression->children.push_back(this_node);
      expression->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", member.name)));
      const string address = EmitMemberAddress(expression, scope);
      (void)EmitDestructorAt(member_type, address, scope);
    }
    TypePtr base = type_value(owner->direct_base);
    if(base) {
      const string this_address = EmitValue(this_node, scope).operand;
      const string base_address = AdjustBaseAddress(this_address, owner, base);
      (void)EmitDestructorAt(base, base_address, scope);
    }
  }

void PA14Lowerer::EmitLiveDestructors(Scope* scope)
{
    for(size_t i = state_->variables.size(); i > 0; --i) {
      VariablePlan& variable = state_->variables[i - 1];
      bool live = false;
      for(size_t environment_index = 0;
          environment_index < state_->environments.size() && !live;
          ++environment_index) {
        const map<string, VariablePlan*>& environment =
          state_->environments[environment_index];
        for(map<string, VariablePlan*>::const_iterator it = environment.begin();
            it != environment.end(); ++it)
          if(it->second == &variable) { live = true; break; }
      }
      if(!live) continue;
      TypePtr object_type = type_value(variable.type);
      if(!object_type) continue;
      if(object_type->kind == TYPE_CLASS) {
        if(!HasDestructor(object_type)) continue;
        (void)EmitDestructorAt(object_type, local_address(&variable), scope);
        continue;
      }
      TypePtr element_type = object_type->child ? type_value(object_type->child) : TypePtr();
      if(object_type->kind != TYPE_ARRAY || object_type->bound < 0 ||
         !element_type || element_type->kind != TYPE_CLASS || !HasDestructor(element_type)) continue;
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

} // namespace cppgm_pa14_lowering
