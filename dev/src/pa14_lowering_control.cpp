#include "pa14_lowering.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

using namespace std;

namespace cppgm_pa14_lowering {

PA14Lowerer::Value PA14Lowerer::ValueFromInfo(const ExprInfo& info) const
{
    Value result;
    result.type = info.type;
    result.operand = info.operand;
    result.known_constant = info.known_constant;
    result.constant = info.constant;
    return result;
  }

PA14Lowerer::Value PA14Lowerer::ValueWithNullptr() const
{
    Value result;
    result.type = Fundamental("nullptr_t");
    result.operand = "nullptr";
    return result;
  }

bool PA14Lowerer::EmitConstructorAt(const TypePtr& raw_object_type, const string& address,
                                    const vector<CPPGMAstNodePtr>& raw_arguments,
                                    Scope* scope)
{
    TypePtr object_type = type_value(raw_object_type);
    if(!object_type || object_type->kind != TYPE_CLASS) return false;
    const string constructor_name = LastComponent(object_type->name);
    if(!raw_arguments.empty()) (void)EnsureAggregateConstructor(object_type);
    vector<Binding*> candidates = MemberBindings(object_type, constructor_name);
    vector<ExprInfo> argument_infos;
    for(size_t i = 0; i < raw_arguments.size(); ++i)
      argument_infos.push_back(Infer(raw_arguments[i], scope));
    Binding* best_binding = 0;
    TypePtr best_function;
    int best_worst = 1000000;
    int best_total = 1000000;
    for(size_t i = 0; i < candidates.size(); ++i) {
      Binding* binding = candidates[i];
      if(!binding->is_member || binding->is_static || binding->kind != BIND_FUNCTION)
        continue;
      FunctionRecord* record = RecordForBinding(binding);
      if(!record || !record->constructor) continue;
      if(record->implicit_constructor && !raw_arguments.empty()) continue;
      TypePtr function = function_target_type(binding->type);
      if(!function) continue;
      if(argument_infos.size() > function->parameters.size() && !function->variadic) continue;
      if(argument_infos.size() < function->parameters.size()) {
        bool defaults = true;
        for(size_t p = argument_infos.size(); p < function->parameters.size(); ++p)
          if(!HasDefaultArgument(binding, p)) { defaults = false; break; }
        if(!defaults) continue;
      }
      int worst = 0;
      int total = 0;
      bool viable = true;
      for(size_t a = 0; a < argument_infos.size(); ++a) {
        const int rank = a < function->parameters.size() ?
          ConversionRank(argument_infos[a], function->parameters[a]) : 2;
        if(rank < 0) { viable = false; break; }
        worst = max(worst, rank);
        total += rank;
      }
      if(!viable) continue;
      if(!best_binding || worst < best_worst ||
         (worst == best_worst && total < best_total)) {
        best_binding = binding;
        best_function = function;
        best_worst = worst;
        best_total = total;
      } else if(worst == best_worst && total == best_total &&
                !PA12SameType(best_function, function, false)) {
        throw logic_error("ambiguous constructor overload");
      }
    }
    if(!best_binding) {
      for(size_t i = 0; i < candidates.size(); ++i)
        if(candidates[i]->is_member && !candidates[i]->is_static &&
           candidates[i]->kind == BIND_FUNCTION && RecordForBinding(candidates[i]) &&
           RecordForBinding(candidates[i])->constructor &&
           !(RecordForBinding(candidates[i])->implicit_constructor && !raw_arguments.empty()))
          throw logic_error("no viable constructor");
      return false;
    }
    FunctionRecord* record = RecordForBinding(best_binding);
    if(record) record->needed = true;
    vector<CPPGMAstNodePtr> arguments = raw_arguments;
    if(record) {
      while(arguments.size() < best_function->parameters.size()) {
        const size_t index = arguments.size();
        if(index >= record->default_arguments.size() || !record->default_arguments[index]) break;
        arguments.push_back(InitializerExpression(record->default_arguments[index]));
      }
    }
    vector<string> operands;
    operands.push_back(address);
    for(size_t i = 0; i < arguments.size(); ++i) {
      TypePtr target = i < best_function->parameters.size() ? best_function->parameters[i] : TypePtr();
      if(target && type_is_reference(target))
        operands.push_back(EmitReferenceArgument(arguments[i], scope, target));
      else {
        Value value = EmitValue(arguments[i], scope, target);
        if(target) value = ConvertValue(value, target);
        operands.push_back(value.operand);
      }
    }
    ostringstream call;
    call << "call void @" << record->symbol << "(";
    for(size_t i = 0; i < operands.size(); ++i) {
      if(i != 0) call << ", ";
      call << operands[i];
    }
    call << ")";
    AddInstruction(call.str());
    return true;
  }

bool PA14Lowerer::EmitDestructorAt(const TypePtr& raw_object_type, const string& address,
                                   Scope* scope)
{
    TypePtr object_type = type_value(raw_object_type);
    if(!object_type || object_type->kind != TYPE_CLASS) return false;
    const string name = "~" + LastComponent(object_type->name);
    vector<Binding*> candidates = MemberBindings(object_type, name);
    for(size_t i = 0; i < candidates.size(); ++i) {
      Binding* binding = candidates[i];
      if(binding->kind != BIND_FUNCTION || !binding->is_member || binding->is_static) continue;
      FunctionRecord* record = RecordForBinding(binding);
      if(!record || !record->destructor) continue;
      record->needed = true;
      AddInstruction("call void @" + record->symbol + "(" + address + ")");
      return true;
    }
    (void)scope;
    return false;
  }

bool PA14Lowerer::EmitObjectConstructor(VariablePlan* variable,
                                        const TypePtr& raw_object_type,
                                        const vector<CPPGMAstNodePtr>& raw_arguments,
                                        Scope* scope)
{
    if(!variable) return false;
    TypePtr object_type = type_value(raw_object_type);
    if(!object_type || object_type->kind != TYPE_CLASS) return false;
    const vector<Binding*> candidates = MemberBindings(object_type, LastComponent(object_type->name));
    bool has_constructor = false;
    for(size_t i = 0; i < candidates.size(); ++i)
      if(candidates[i]->is_member && !candidates[i]->is_static &&
         candidates[i]->kind == BIND_FUNCTION && RecordForBinding(candidates[i]) &&
         RecordForBinding(candidates[i])->constructor &&
         !(RecordForBinding(candidates[i])->implicit_constructor && !raw_arguments.empty())) {
        has_constructor = true;
        break;
      }
    if(!has_constructor) return false;
    string address;
    if(!variable->initialization_address.empty()) {
      address = variable->initialization_address;
      variable->initialization_address.clear();
    } else {
      address = EmitAddress(CPPGMAstNodePtr(new CPPGMAstNode(
        "id-expression", variable->source_name)), scope);
    }
    return EmitConstructorAt(raw_object_type, address, raw_arguments, scope);
  }

void PA14Lowerer::EmitConstructorInitializers(FunctionRecord& function, Scope* scope)
{
    TypePtr owner = type_value(function.member_owner);
    if(!owner) return;
    CPPGMAstNodePtr this_node(new CPPGMAstNode("keyword-literal", "KW_THIS:this"));
    set<string> initialized_members;
    TypePtr base = type_value(owner->direct_base);
    if(base) {
      const string this_address = EmitValue(this_node, scope).operand;
      const string base_address = AdjustBaseAddress(this_address, owner, base);
      (void)EmitConstructorAt(base, base_address, vector<CPPGMAstNodePtr>(), scope);
    }
    if(function.special_initializer) for(size_t i = 0; i < function.special_initializer->children.size(); ++i) {
      CPPGMAstNodePtr initializer = function.special_initializer->children[i];
      if(!initializer || initializer->kind != "mem-initializer") continue;
      CPPGMAstNodePtr name_node = ChildOfKind(initializer, "mem-initializer-id");
      if(!name_node) continue;
      const string name = LastComponent(name_node->value);
      CPPGMAstNodePtr argument_node = ChildOfKind(initializer, "paren-argument-list");
      if(!argument_node) argument_node = ChildOfKind(initializer, "braced-init-list");
      vector<CPPGMAstNodePtr> arguments = argument_node ? argument_node->children :
        vector<CPPGMAstNodePtr>();
      TypePtr named_base;
      if(base && (name == LastComponent(base->name) || name == base->name)) named_base = base;
      if(named_base) {
        const string this_address = EmitValue(this_node, scope).operand;
        const string base_address = AdjustBaseAddress(this_address, owner, named_base);
        if(!EmitConstructorAt(named_base, base_address, arguments, scope))
          throw logic_error("base mem-initializer has no constructor");
        continue;
      }
      vector<Binding*> fields = DirectBindings(owner->owned_scope, name);
      Binding* field = 0;
      for(size_t j = 0; j < fields.size(); ++j)
        if(fields[j]->kind == BIND_VARIABLE && fields[j]->is_member && !fields[j]->is_static) {
          field = fields[j];
          break;
        }
      if(!field) throw logic_error("unknown mem-initializer");
      initialized_members.insert(name);
      TypePtr field_type = type_value(field->type);
      Value value;
      string reference_source;
      if(!(field_type && field_type->kind == TYPE_CLASS && !arguments.empty()) &&
         !arguments.empty()) {
        if(arguments.size() != 1) throw logic_error("member mem-initializer has too many arguments");
        if(type_is_reference(field->type))
          reference_source = EmitReferenceArgument(arguments[0], scope, field->type);
        else {
          value = EmitValue(arguments[0], scope, field->type);
          value = ConvertValue(value, field->type);
        }
      }
      CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "OP_ARROW:->"));
      member->children.push_back(this_node);
      member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", name)));
      if(IsBitField(field) && !arguments.empty()) {
        bool preserve = false;
        if(field->member_owner && field->member_index != static_cast<size_t>(-1) &&
           field->member_index < field->member_owner->class_members.size()) {
          const long long offset = field->member_owner->class_members[field->member_index].offset;
          for(set<string>::const_iterator it = initialized_members.begin();
              it != initialized_members.end() && !preserve; ++it) {
            if(*it == name) continue;
            vector<Binding*> prior = DirectBindings(owner->owned_scope, *it);
            for(size_t j = 0; j < prior.size(); ++j)
              if(IsBitField(prior[j]) && prior[j]->member_owner &&
                 prior[j]->member_index != static_cast<size_t>(-1) &&
                 prior[j]->member_index < prior[j]->member_owner->class_members.size() &&
                 prior[j]->member_owner->class_members[prior[j]->member_index].offset == offset) {
                preserve = true;
                break;
              }
          }
        }
        string stored;
        if(preserve) {
          const string read_address = EmitMemberAddress(member, scope);
          stored = MergeBitFieldValue(field, read_address, field->type, value.operand, true);
        } else stored = PrepareBitFieldValue(field, field->type, value.operand);
        const string store_address = EmitMemberAddress(member, scope);
        emit_store(field->type, stored, store_address);
        continue;
      }
      const string address = EmitMemberAddress(member, scope);
      if(field_type && field_type->kind == TYPE_ARRAY && arguments.empty() &&
         field_type->bound >= 0 && field_type->child &&
         type_value(field_type->child) && type_value(field_type->child)->kind != TYPE_CLASS) {
        for(size_t element_index = 0;
            element_index < static_cast<size_t>(field_type->bound); ++element_index) {
          const string member_address = element_index == 0 ? address :
            EmitMemberAddress(member, scope);
          const string decay = new_temp();
          AddInstruction(decay + " = unary decay ptr " + member_address);
          const string element = new_temp();
          AddInstruction(element + " = index " + low_type(field_type->child) + " " +
            decay + ", " + integer_text(static_cast<long long>(element_index)));
          emit_store(field_type->child, "0", element);
        }
        continue;
      }
      if(field_type && field_type->kind == TYPE_CLASS && arguments.empty())
        emit_store(Fundamental("long int"), "0", address);
      if(field_type && field_type->kind == TYPE_CLASS &&
         EmitConstructorAt(field_type, address, arguments, scope)) continue;
      if(arguments.empty()) {
        if(!field_type || field_type->kind != TYPE_CLASS)
          emit_store(field->type, "0", address);
        continue;
      }
      if(type_is_reference(field->type)) {
        emit_store(PointerTo(Fundamental("char")), reference_source, address);
      } else if(IsBitField(field)) {
        StoreBitField(field, address, field->type, value.operand, true);
      } else {
        emit_store(field->type, value.operand, address);
      }
    }
    for(size_t i = 0; i < owner->class_members.size(); ++i) {
      const ClassMemberInfo& member_fact = owner->class_members[i];
      if(member_fact.is_static || member_fact.name.empty() || !member_fact.type ||
         initialized_members.find(member_fact.name) != initialized_members.end()) continue;
      vector<Binding*> fields = DirectBindings(owner->owned_scope, member_fact.name);
      Binding* field = 0;
      for(size_t j = 0; j < fields.size(); ++j)
        if(fields[j]->kind == BIND_VARIABLE && fields[j]->is_member && !fields[j]->is_static) {
          field = fields[j];
          break;
        }
      if(!field) continue;
      TypePtr field_type = type_value(field->type);
      CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "OP_ARROW:->"));
      member->children.push_back(this_node);
      member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
        "identifier", member_fact.name)));
      vector<CPPGMAstNodePtr> arguments;
      CPPGMAstNodePtr expression;
      if(member_fact.initializer) {
        if(!member_fact.initializer->children.empty() && member_fact.initializer->children[0] &&
           member_fact.initializer->children[0]->kind == "paren-initializer") {
          arguments = member_fact.initializer->children[0]->children;
          if(arguments.size() == 1) expression = arguments[0];
        } else {
          expression = InitializerExpression(member_fact.initializer);
          if(expression && expression->kind == "braced-init-list")
            arguments = expression->children;
          else if(expression) arguments.push_back(expression);
        }
        if(expression && expression->kind == "call-expression" &&
           !expression->children.empty() && expression->children[0] &&
           expression->children[0]->kind == "id-expression" && field_type &&
           field_type->kind == TYPE_CLASS &&
           LastComponent(field_type->name) == expression->children[0]->value) {
          CPPGMAstNodePtr argument_list = expression->children.size() > 1 ?
            expression->children[1] : CPPGMAstNodePtr();
          arguments = argument_list ? argument_list->children : vector<CPPGMAstNodePtr>();
          expression.reset();
        }
      }
      const bool empty_value_initializer = expression &&
        ((expression->kind == "braced-init-list" && expression->children.empty()) ||
         (expression->kind == "paren-initializer" && expression->children.empty()));
      if(arguments.empty() && (!field_type || field_type->kind != TYPE_CLASS) &&
         !empty_value_initializer) continue;
      if(IsBitField(field)) {
        if(arguments.size() != 1) throw logic_error("default member initializer has too many arguments");
        Value value = EmitValue(arguments[0], scope, field->type);
        value = ConvertValue(value, field->type);
        bool preserve = false;
        if(field->member_owner && field->member_index != static_cast<size_t>(-1) &&
           field->member_index < field->member_owner->class_members.size()) {
          const long long offset = field->member_owner->class_members[field->member_index].offset;
          for(set<string>::const_iterator it = initialized_members.begin();
              it != initialized_members.end() && !preserve; ++it) {
            if(*it == member_fact.name) continue;
            vector<Binding*> prior = DirectBindings(owner->owned_scope, *it);
            for(size_t j = 0; j < prior.size(); ++j)
              if(IsBitField(prior[j]) && prior[j]->member_owner &&
                 prior[j]->member_index != static_cast<size_t>(-1) &&
                 prior[j]->member_index < prior[j]->member_owner->class_members.size() &&
                 prior[j]->member_owner->class_members[prior[j]->member_index].offset == offset) {
                preserve = true;
                break;
              }
          }
        }
        string stored;
        if(preserve) {
          const string read_address = EmitMemberAddress(member, scope);
          stored = MergeBitFieldValue(field, read_address, field->type, value.operand, true);
        } else stored = PrepareBitFieldValue(field, field->type, value.operand);
        const string store_address = EmitMemberAddress(member, scope);
        emit_store(field->type, stored, store_address);
        initialized_members.insert(member_fact.name);
        continue;
      }
      const string address = EmitMemberAddress(member, scope);
      if(field_type && field_type->kind == TYPE_CLASS &&
         EmitConstructorAt(field_type, address, arguments, scope)) continue;
      if(expression && expression->kind == "braced-init-list" &&
         field_type && (field_type->kind == TYPE_CLASS || field_type->kind == TYPE_ARRAY)) {
        EmitAggregateAt(address, field_type, expression, scope);
        continue;
      }
      if(arguments.empty()) {
        if(empty_value_initializer && (!field_type || field_type->kind != TYPE_CLASS))
          emit_store(field->type,
            field_type && field_type->kind == TYPE_POINTER ? "nullptr" : "0", address);
        continue;
      }
      if(arguments.size() != 1) throw logic_error("default member initializer has too many arguments");
      Value value = type_is_reference(field->type) ? Value() :
        EmitValue(arguments[0], scope, field->type);
      if(type_is_reference(field->type)) {
        emit_store(PointerTo(Fundamental("char")),
          EmitReferenceArgument(arguments[0], scope, field->type), address);
      } else if(IsBitField(field)) {
        StoreBitField(field, address, field->type, value.operand, true);
      } else {
        value = ConvertValue(value, field->type);
        emit_store(field->type, value.operand, address);
      }
    }
  }

void PA14Lowerer::EmitInitializer(VariablePlan* variable, const CPPGMAstNodePtr& initializer,
                       Scope* scope)
{
    if(!variable || !initializer) return;
    CPPGMAstNodePtr expression = InitializerExpression(initializer);
    if(type_is_reference(variable->type)) {
      if(!expression) throw logic_error("reference initializer is empty");
      const string address = EmitAddress(expression, scope);
      emit_store(PointerTo(Fundamental("char")), address, StorageForVariable(*variable));
      return;
    }
    if(variable->type->kind == TYPE_ARRAY) {
      if(!expression) return;
      string base = EmitAddress(CPPGMAstNodePtr(new CPPGMAstNode("id-expression", variable->source_name)), scope);
      if(expression->kind == "literal" && !expression->value.empty() && expression->value[0] == '"') {
        const vector<unsigned char> bytes = decode_string_literal(expression->value);
        for(size_t i = 0; i < bytes.size() && i < static_cast<size_t>(max(0LL, variable->type->bound)); ++i) {
          string storage = base;
          if(i != 0) {
            const string index = new_temp();
            AddInstruction(index + " = index i8 " + base + ", " +
              integer_text(static_cast<long long>(i * type_size(variable->type->child))));
            storage = index;
          }
          emit_store(variable->type->child, integer_text(bytes[i]), storage);
        }
        return;
      }
      if(expression->kind != "braced-init-list") return;
      TypePtr element_type = type_value(variable->type->child);
      for(size_t i = 0; i < expression->children.size(); ++i) {
        Value value = EmitValue(expression->children[i], scope, variable->type->child);
        string storage = base;
        if(i != 0) {
          const string index = new_temp();
          AddInstruction(index + " = index i8 " + base + ", " +
            integer_text(static_cast<long long>(i * type_size(variable->type->child))));
          storage = index;
        }
        const CPPGMAstNodePtr child = expression->children[i];
        if(element_type && element_type->kind == TYPE_CLASS) {
          vector<CPPGMAstNodePtr> arguments;
          if(child && child->kind == "braced-init-list") arguments = child->children;
          else if(child && child->kind == "paren-initializer") arguments = child->children;
          else if(child && child->kind == "call-expression" && child->children.size() > 1 &&
                  child->children[0] && child->children[0]->kind == "id-expression" &&
                  LastComponent(element_type->name) == child->children[0]->value) {
            arguments = child->children[1] ? child->children[1]->children :
              vector<CPPGMAstNodePtr>();
          } else if(child) arguments.push_back(child);
          if(EmitConstructorAt(element_type, storage, arguments, scope)) continue;
          if(child && child->kind == "braced-init-list") {
            EmitAggregateAt(storage, element_type, child, scope);
            continue;
          }
        }
        if(element_type && element_type->kind == TYPE_ARRAY &&
           child && child->kind == "braced-init-list") {
          EmitAggregateAt(storage, element_type, child, scope);
          continue;
        }
        if(type_is_reference(variable->type->child)) {
          emit_store(PointerTo(Fundamental("char")),
            EmitReferenceArgument(child, scope, variable->type->child), storage);
          continue;
        }
        if(value.known_constant && is_integral_type(value.type) &&
           is_integral_type(variable->type->child)) {
          value.type = variable->type->child;
          value.operand = integer_text(value.constant);
        } else value = ConvertValue(value, variable->type->child);
        emit_store(variable->type->child, value.operand, storage);
      }
      if(element_type && element_type->kind == TYPE_CLASS && variable->type->bound >= 0) {
        for(size_t i = expression->children.size();
            i < static_cast<size_t>(variable->type->bound); ++i) {
          string storage = base;
          if(i != 0) {
            const string index = new_temp();
            AddInstruction(index + " = index i8 " + base + ", " +
              integer_text(static_cast<long long>(i * type_size(variable->type->child))));
            storage = index;
          }
          if(!EmitConstructorAt(element_type, storage,
                                vector<CPPGMAstNodePtr>(), scope) &&
             !HasConstructor(element_type)) break;
        }
      }
      return;
    }
    TypePtr aggregate_type = type_value(variable->type);
    if(aggregate_type && aggregate_type->kind == TYPE_CLASS) {
      vector<CPPGMAstNodePtr> constructor_arguments;
      if(expression && expression->kind == "braced-init-list")
        constructor_arguments = expression->children;
      else if(!initializer->children.empty() && initializer->children[0] &&
              initializer->children[0]->kind == "paren-initializer")
        constructor_arguments = initializer->children[0]->children;
      if(EmitObjectConstructor(variable, aggregate_type, constructor_arguments, scope)) return;
      if(expression && expression->kind == "braced-init-list") {
        if(expression->children.empty()) return;
        CPPGMAstNodePtr object_node(new CPPGMAstNode("id-expression", variable->source_name));
        EmitAggregateAt(string(), aggregate_type, expression, scope, object_node);
        return;
      }
    }
    if(!expression) {
      TypePtr object_type = type_value(variable->type);
      if(variable->type->kind != TYPE_FUNCTION &&
         (!object_type || object_type->kind != TYPE_CLASS))
        emit_store(variable->type, "0", StorageForVariable(*variable));
      return;
    }
    Value value = EmitValue(expression, scope, type_value(variable->type));
    if(value.known_constant && is_integral_type(value.type) &&
       is_integral_type(variable->type)) {
      value.type = type_value(variable->type);
      value.operand = integer_text(value.constant);
    } else value = ConvertValue(value, type_value(variable->type));
    StoreLValue(CPPGMAstNodePtr(new CPPGMAstNode("id-expression", variable->source_name)),
      scope, type_value(variable->type), value.operand);
  }

bool PA14Lowerer::HasNonSizeofReference(const CPPGMAstNodePtr& node,
                                        const string& name, bool inside_sizeof) const
{
    if(!node) return false;
    const bool now_inside_sizeof = inside_sizeof || node->kind == "sizeof-expression";
    if(node->kind == "id-expression" && node->value == name && !now_inside_sizeof)
      return true;
    for(size_t i = 0; i < node->children.size(); ++i)
      if(HasNonSizeofReference(node->children[i], name, now_inside_sizeof)) return true;
    return false;
  }

bool PA14Lowerer::StatementTerminates(const CPPGMAstNodePtr& node) const
{
    if(!node) return false;
    if(node->kind == "return-statement" || node->kind == "break-statement" ||
       node->kind == "continue-statement" || node->kind == "goto-statement") return true;
    if(node->kind == "compound-statement")
      return !node->children.empty() && StatementTerminates(node->children.back());
    if(node->kind == "if-statement") {
      CPPGMAstNodePtr then_node = ChildOfKind(node, "then");
      CPPGMAstNodePtr else_node = ChildOfKind(node, "else");
      return then_node && else_node && !then_node->children.empty() &&
        !else_node->children.empty() && StatementTerminates(then_node->children[0]) &&
        StatementTerminates(else_node->children[0]);
    }
    return false;
  }

void PA14Lowerer::EmitReturn(const CPPGMAstNodePtr& node, Scope* scope)
{
    TypePtr return_type = state_->record->type->child;
    if(!return_type || low_type(return_type) == "void") {
      if(!node->children.empty()) EmitDiscard(node->children[0], scope);
      EmitLiveDestructors(scope);
      Terminate("return void");
      return;
    }
    if(node->children.empty()) {
      EmitLiveDestructors(scope);
      Terminate("return " + low_type(return_type) + " 0");
      return;
    }
    CPPGMAstNodePtr expression = node->children[0];
    if(type_is_reference(return_type)) {
      const string address = EmitAddress(expression, scope);
      EmitLiveDestructors(scope);
      Terminate("return ptr " + address);
      return;
    }
    Value value = EmitValue(expression, scope, return_type);
    if(value.known_constant && is_integral_type(value.type) && is_integral_type(return_type) &&
       type_size(return_type) > type_size(value.type) && !is_unsigned_type(return_type)) {
      // PA14 permits canonical widened integral immediates.  Signed long
      // return literals are emitted directly; unsigned aliases retain the
      // explicit conversion boundary used by the reference LowIR.
      EmitLiveDestructors(scope);
      Terminate("return " + low_type(return_type) + " " + integer_text(value.constant));
      return;
    }
    value = ConvertValue(value, return_type);
    EmitLiveDestructors(scope);
    Terminate("return " + low_type(return_type) + " " + value.operand);
  }

void PA14Lowerer::EmitIf(const CPPGMAstNodePtr& node, Scope* scope)
{
    EnterEnvironment();
    CPPGMAstNodePtr condition_wrapper = ChildOfKind(node, "condition");
    CPPGMAstNodePtr condition = condition_wrapper && !condition_wrapper->children.empty() ?
      condition_wrapper->children[0] : CPPGMAstNodePtr();
    if(condition && condition->kind == "condition-declaration") BindCondition(condition);
    CPPGMAstNodePtr then_wrapper = ChildOfKind(node, "then");
    CPPGMAstNodePtr else_wrapper = ChildOfKind(node, "else");
    const string then_label = new_label("if_then");
    const string else_label = new_label("if_else");
    bool has_else = else_wrapper && !else_wrapper->children.empty();
    bool needs_end = !has_else || !StatementTerminates(then_wrapper->children[0]) ||
      !StatementTerminates(else_wrapper->children[0]);
    string end_label;
    if(needs_end) end_label = new_label("if_end");
    EmitCondition(condition, scope, then_label, else_label);
    AddBlock(then_label);
    if(then_wrapper && !then_wrapper->children.empty()) EmitStatement(then_wrapper->children[0], scope);
    if(!state_->current->terminated) {
      if(needs_end) Terminate("jump ^" + end_label);
      else Terminate("jump ^" + else_label);
    }
    AddBlock(else_label);
    if(has_else) EmitStatement(else_wrapper->children[0], scope);
    if(!state_->current->terminated && needs_end) Terminate("jump ^" + end_label);
    if(needs_end) AddBlock(end_label);
    LeaveEnvironment();
  }

void PA14Lowerer::EmitWhile(const CPPGMAstNodePtr& node, Scope* scope)
{
    EnterEnvironment();
    if(!node->children.empty() && node->children[0]->kind == "condition" &&
       !node->children[0]->children.empty() &&
       node->children[0]->children[0]->kind == "condition-declaration")
      BindCondition(node->children[0]->children[0]);
    const string condition_label = new_label("while_cond");
    const string body_label = new_label("while_body");
    const string end_label = new_label("while_end");
    Terminate("jump ^" + condition_label);
    AddBlock(condition_label);
    CPPGMAstNodePtr condition = node->children.empty() ? CPPGMAstNodePtr() : node->children[0];
    if(condition && condition->kind == "condition" && !condition->children.empty()) condition = condition->children[0];
    EmitCondition(condition, scope, body_label, end_label);
    AddBlock(body_label);
    state_->break_targets.push_back(end_label);
    state_->continue_targets.push_back(condition_label);
    if(node->children.size() > 1) EmitStatement(node->children[1], scope);
    state_->continue_targets.pop_back();
    state_->break_targets.pop_back();
    if(!state_->current->terminated) Terminate("jump ^" + condition_label);
    AddBlock(end_label);
    LeaveEnvironment();
  }

void PA14Lowerer::EmitDo(const CPPGMAstNodePtr& node, Scope* scope)
{
    EnterEnvironment();
    const string body_label = new_label("do_body");
    const string condition_label = new_label("do_cond");
    const string end_label = new_label("do_end");
    Terminate("jump ^" + body_label);
    AddBlock(body_label);
    state_->break_targets.push_back(end_label);
    state_->continue_targets.push_back(condition_label);
    if(!node->children.empty()) EmitStatement(node->children[0], scope);
    state_->continue_targets.pop_back();
    state_->break_targets.pop_back();
    if(!state_->current->terminated) Terminate("jump ^" + condition_label);
    AddBlock(condition_label);
    CPPGMAstNodePtr condition = node->children.size() > 1 ? node->children[1] : CPPGMAstNodePtr();
    if(condition && !condition->children.empty()) condition = condition->children[0];
    EmitCondition(condition, scope, body_label, end_label);
    AddBlock(end_label);
    LeaveEnvironment();
  }

void PA14Lowerer::EmitFor(const CPPGMAstNodePtr& node, Scope* scope)
{
    EnterEnvironment();
    if(!node->children.empty() && node->children[0] && !node->children[0]->children.empty())
      EmitStatement(node->children[0]->children[0], scope);
    const string condition_label = new_label("for_cond");
    const string body_label = new_label("for_body");
    const string iteration_label = new_label("for_iter");
    const string end_label = new_label("for_end");
    if(!state_->current->terminated) Terminate("jump ^" + condition_label);
    AddBlock(condition_label);
    size_t index = 1;
    CPPGMAstNodePtr condition;
    if(index < node->children.size() && node->children[index]->kind == "condition") {
      condition = node->children[index];
      if(!condition->children.empty()) condition = condition->children[0];
      ++index;
    }
    if(condition) EmitCondition(condition, scope, body_label, end_label);
    else Terminate("jump ^" + body_label);
    AddBlock(body_label);
    state_->break_targets.push_back(end_label);
    state_->continue_targets.push_back(iteration_label);
    if(index < node->children.size() && node->children[index]->kind == "iteration") ++index;
    if(index < node->children.size()) EmitStatement(node->children[index], scope);
    state_->continue_targets.pop_back();
    state_->break_targets.pop_back();
    if(!state_->current->terminated) Terminate("jump ^" + iteration_label);
    AddBlock(iteration_label);
    size_t iteration_index = 1;
    if(iteration_index < node->children.size() && node->children[iteration_index]->kind == "condition") ++iteration_index;
    if(iteration_index < node->children.size() && node->children[iteration_index]->kind == "iteration") {
      if(!node->children[iteration_index]->children.empty()) EmitDiscard(node->children[iteration_index]->children[0], scope);
    }
    if(!state_->current->terminated) Terminate("jump ^" + condition_label);
    AddBlock(end_label);
    LeaveEnvironment();
  }

void PA14Lowerer::CollectCaseNodes(const CPPGMAstNodePtr& node,
                        vector<CPPGMAstNodePtr>& cases) const
{
    if(!node) return;
    if(node->kind == "case-statement" || node->kind == "default-statement") {
      cases.push_back(node);
      const size_t first_body = node->kind == "case-statement" ? 1 : 0;
      for(size_t i = first_body; i < node->children.size(); ++i)
        CollectCaseNodes(node->children[i], cases);
      return;
    }
    for(size_t i = 0; i < node->children.size(); ++i)
      CollectCaseNodes(node->children[i], cases);
  }

void PA14Lowerer::CollectNamedLabels(const CPPGMAstNodePtr& node,
                          vector<string>& labels) const
{
    if(!node) return;
    if(node->kind == "labeled-statement") labels.push_back(node->value);
    for(size_t i = 0; i < node->children.size(); ++i)
      CollectNamedLabels(node->children[i], labels);
  }

bool PA14Lowerer::HasBlockLabel(const string& label) const
{
    if(!state_) return false;
    for(size_t i = 0; i < state_->blocks.size(); ++i)
      if(state_->blocks[i].label == label) return true;
    return false;
  }

void PA14Lowerer::EmitCaseLabelAndBody(const CPPGMAstNodePtr& node, Scope* scope)
{
    if(!node) return;
    map<const CPPGMAstNode*, string>::const_iterator found =
      state_->case_labels.find(node.get());
    if(found == state_->case_labels.end()) throw logic_error("unknown switch label");
    const string label = found->second;
    if(state_->emitted_cases.find(node.get()) != state_->emitted_cases.end()) {
      if(!state_->current->terminated && state_->current->label != label)
        Terminate("jump ^" + label);
      return;
    }
    if(state_->current->label != label) {
      if(!state_->current->terminated) Terminate("jump ^" + label);
      AddBlock(label);
    }
    state_->emitted_cases.insert(node.get());
    const size_t first_body = node->kind == "case-statement" ? 1 : 0;
    for(size_t i = first_body; i < node->children.size(); ++i)
      EmitStatement(node->children[i], scope);
  }

void PA14Lowerer::EmitSwitchBody(const CPPGMAstNodePtr& node, Scope* scope)
{
    if(!node) return;
    if(node->kind == "compound-statement") {
      for(size_t i = 0; i < node->children.size(); ++i) {
        const CPPGMAstNodePtr child = node->children[i];
        if(child && (child->kind == "case-statement" ||
                     child->kind == "default-statement"))
          EmitCaseLabelAndBody(child, scope);
        else if(!state_->current->terminated ||
                (child && (child->kind == "labeled-statement" ||
                           child->kind == "case-statement" ||
                           child->kind == "default-statement")))
          EmitStatement(child, scope);
      }
      return;
    }
    if(node->kind == "case-statement" || node->kind == "default-statement")
      EmitCaseLabelAndBody(node, scope);
    else EmitStatement(node, scope);
  }

void PA14Lowerer::EmitSwitch(const CPPGMAstNodePtr& node, Scope* scope)
{
    EnterEnvironment();
    CPPGMAstNodePtr condition = node && !node->children.empty() ? node->children[0] : CPPGMAstNodePtr();
    if(condition && condition->kind == "condition" && !condition->children.empty())
      condition = condition->children[0];
    Value selector;
    if(condition && condition->kind == "condition-declaration") {
      VariablePlan* variable = BindCondition(condition);
      if(!variable || condition->children.size() < 3)
        throw logic_error("invalid switch condition declaration");
      EmitInitializer(variable, condition->children[2], scope);
      selector = EmitValue(CPPGMAstNodePtr(new CPPGMAstNode(
        "id-expression", variable->source_name)), scope);
    } else {
      selector = EmitValue(condition, scope);
    }

    const string dispatch_label = new_label("switch_dispatch");
    const string end_label = new_label("switch_end");
    vector<CPPGMAstNodePtr> cases;
    if(node && node->children.size() > 1) CollectCaseNodes(node->children[1], cases);
    CPPGMAstNodePtr default_node;
    for(size_t i = 0; i < cases.size(); ++i) {
      if(cases[i]->kind == "default-statement") {
        default_node = cases[i];
        continue;
      }
      state_->case_labels[cases[i].get()] = new_label("switch_case");
    }
    if(default_node) state_->case_labels[default_node.get()] = new_label("switch_default");
    vector<string> labels;
    if(node && node->children.size() > 1) CollectNamedLabels(node->children[1], labels);
    for(size_t i = 0; i < labels.size(); ++i) {
      if(state_->named_labels.find(labels[i]) == state_->named_labels.end())
        state_->named_labels[labels[i]] = new_label("goto");
    }

    if(!state_->current->terminated) Terminate("jump ^" + dispatch_label);
    AddBlock(dispatch_label);
    ostringstream dispatch;
    dispatch << "switch " << selector.operand << ", ^" <<
      (default_node ? state_->case_labels[default_node.get()] : end_label);
    for(size_t i = 0; i < cases.size(); ++i) {
      if(cases[i]->kind != "case-statement") continue;
      long long value = 0;
      if(cases[i]->children.empty() ||
         !FoldInteger(cases[i]->children[0], scope, &value, 0))
        throw logic_error("nonconstant switch case");
      dispatch << ", " << value << ":^" << state_->case_labels[cases[i].get()];
    }
    AddInstruction(dispatch.str());
    state_->current->terminated = true;

    state_->break_targets.push_back(end_label);
    state_->switch_end_targets.push_back(end_label);
    if(node && node->children.size() > 1) EmitSwitchBody(node->children[1], scope);
    state_->switch_end_targets.pop_back();
    state_->break_targets.pop_back();
    if(!state_->current->terminated) Terminate("jump ^" + end_label);
    AddBlock(end_label);
    LeaveEnvironment();
  }

void PA14Lowerer::EmitDiscard(const CPPGMAstNodePtr& node, Scope* scope)
{
    if(!node) return;
    if(node->kind == "parenthesized-expression") {
      if(!node->children.empty()) EmitDiscard(node->children[0], scope);
      return;
    }
    if(node->kind == "postfix-expression") {
      EmitUpdate(node, scope, false);
      return;
    }
    if(node->kind == "assignment-expression") {
      EmitAssignment(node, scope);
      return;
    }
    if(node->kind == "call-expression") {
      EmitCall(node, scope);
      return;
    }
    if(node->kind == "binary-expression" && PA12Operator(node->value) == ",") {
      if(node->children.size() > 0) EmitDiscard(node->children[0], scope);
      if(node->children.size() > 1) EmitDiscard(node->children[1], scope);
      return;
    }
    if(node->kind == "cast-expression" && node->children.size() > 1) {
      TypePtr target = analyzer_.TypeFromTypeId(node->children[0], scope);
      if(low_type(target) == "void") {
        EmitDiscard(node->children[1], scope);
        return;
      }
    }
    EmitValue(node, scope);
  }

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
          bool bound_here = false;
          for(map<string, VariablePlan*>::const_iterator it = environment.begin();
              it != environment.end(); ++it)
            if(it->second == &variable) { bound_here = true; break; }
          if(!bound_here) continue;
          TypePtr object_type = type_value(variable.type);
          if(!object_type) continue;
          if(object_type->kind == TYPE_CLASS) {
            (void)EmitDestructorAt(object_type, local_address(&variable), scope);
            continue;
          }
          TypePtr element_type = object_type->child ? type_value(object_type->child) : TypePtr();
          if(object_type->kind != TYPE_ARRAY || object_type->bound < 0 ||
             !element_type || element_type->kind != TYPE_CLASS) continue;
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
          if(found != state_->plans.end() &&
             type_value(found->second->type) &&
             (type_value(found->second->type)->kind == TYPE_CLASS ||
              (type_value(found->second->type)->kind == TYPE_ARRAY &&
               type_value(found->second->type)->child &&
               type_value(found->second->type)->child->kind == TYPE_CLASS))) {
            bool initialized = item->children.size() > 1;
            if(initialized) {
              CPPGMAstNodePtr declaration_initializer = InitializerExpression(item->children[1]);
              if(declaration_initializer && declaration_initializer->kind == "braced-init-list" &&
                 declaration_initializer->children.empty()) initialized = false;
            }
            const bool referenced = state_->record && state_->record->node &&
              state_->record->node->children.size() > 2 &&
              HasNonSizeofReference(state_->record->node->children[2],
                found->second->source_name);
            if(initialized || referenced)
              found->second->initialization_address = local_address(found->second);
          }
          if(found != state_->plans.end() && item->children.size() > 1)
            EmitInitializer(found->second, item->children[1], scope);
          else if(found != state_->plans.end() && type_value(found->second->type) &&
                  type_value(found->second->type)->kind == TYPE_CLASS) {
            (void)EmitObjectConstructor(found->second, type_value(found->second->type),
              vector<CPPGMAstNodePtr>(), scope);
            if(!found->second->initialization_address.empty())
              (void)EmitAddress(CPPGMAstNodePtr(new CPPGMAstNode(
                "id-expression", found->second->source_name)), scope);
          }
          else if(found != state_->plans.end() && found->second->type->kind == TYPE_ARRAY &&
                  found->second->type->child &&
                  type_value(found->second->type->child) &&
                  type_value(found->second->type->child)->kind == TYPE_CLASS &&
                  found->second->type->bound >= 0) {
            for(size_t element_index = 0;
                element_index < static_cast<size_t>(found->second->type->bound);
                ++element_index) {
              const string base = local_address(found->second);
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
                   type_value(found->second->type)->kind != TYPE_CLASS))
            emit_store(found->second->type, "0", StorageForVariable(*found->second));
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
    // PA14 has no class/object lifetime lowering.  Parsed declaration-like
    // nodes which do not contribute procedural code are harmless here.
    if(node->kind == "using-declaration" || node->kind == "using-directive" ||
       node->kind == "asm-declaration") return;
    throw logic_error("unsupported statement in LowIR lowering: " + node->kind);
  }

string PA14Lowerer::EmitFunction(FunctionRecord& function)
{
    FunctionState state(this, &function);
    state_ = &state;
    PlanFunction(state);
    state.environments.clear();
    state.environments.push_back(map<string, VariablePlan*>());
    const vector<string> names = ParameterNames(function);
    for(size_t i = 0; i < function.type->parameters.size(); ++i) {
      if(i < state.variables.size())
        state.environments.back()[names[i]] = &state.variables[i];
    }

    const bool entry = function.qualified_name == "main";
    ostringstream header;
    header << "function @" << function.symbol << "(";
    for(size_t i = 0; i < function.type->parameters.size(); ++i) {
      if(i != 0) header << ", ";
      header << "%" << names[i] << " : " << low_type(function.type->parameters[i]);
      if(type_is_reference(function.type->parameters[i])) header << " [pass=reference]";
    }
    header << ") -> " << low_type(function.type->child);
    if(entry) header << " [role=entry]";
    else if(function.variadic) header << " [arity=variadic]";
    header << " {";

    AddBlock("entry");
    for(size_t i = 0; i < function.type->parameters.size(); ++i) {
      if(i >= state.variables.size()) break;
      emit_store(function.type->parameters[i], "%" + names[i],
        StorageForVariable(state.variables[i]));
    }
    Scope* scope = FunctionScope();
    if(function.constructor && !function.aggregate_constructor)
      EmitConstructorInitializers(function, scope);
    if(function.aggregate_constructor) EmitAggregateConstructorBody(function, scope);
    CPPGMAstNodePtr body = function.constructor || function.destructor ?
      ChildOfKind(function.node, "compound-statement") :
      (function.node && function.node->children.size() > 2 ? function.node->children[2] :
       CPPGMAstNodePtr());
    if(body) EmitStatement(body, scope);
    if(function.destructor) EmitDestructorBody(function, scope);
    if(!state.current->terminated) {
      if(low_type(function.type->child) == "void") Terminate("return void");
      else Terminate("return " + low_type(function.type->child) + " 0");
    }

    ostringstream out;
    out << header.str() << "\n";
    for(size_t i = 0; i < state.variables.size(); ++i)
      out << "  slot $" << state.variables[i].slot_name << " : " <<
        storage_type(state.variables[i].type) << "\n";
    for(size_t i = 0; i < state.special_slots.size(); ++i)
      out << "  slot $" << state.special_slots[i] << " : " <<
        state.special_slot_types[state.special_slots[i]] << "\n";
    if(!state.variables.empty() || !state.special_slots.empty()) out << "\n";
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
          if(EmitConstructorAt(element_type, element, arguments, scope)) continue;
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
      plan.initialization_address = global_address(&global);
      if(EmitObjectConstructor(&plan, value_type, arguments, scope)) return;
      if(expression && expression->kind == "braced-init-list" &&
         !expression->children.empty())
        EmitAggregateAt(plan.initialization_address, value_type, expression, scope);
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

void PA14Lowerer::EmitDynamicInitializers(vector<string>& entries)
{
    vector<GlobalRecord*> initializers;
    vector<GlobalRecord*> finalizers;
    for(size_t i = 0; i < globals_.size(); ++i) {
      if(globals_[i].dynamic_initializer) initializers.push_back(&globals_[i]);
      if(globals_[i].dynamic_finalizer) finalizers.push_back(&globals_[i]);
    }
    if(initializers.empty() && finalizers.empty()) return;

    const auto render = [](FunctionState& state, const string& name,
                           const string& role) -> string {
      ostringstream out;
      out << "function @" << name << "() -> void [role=" << role << "] {\n";
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
      entries.push_back(render(state, "__cppgm_init", "init"));
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
      entries.push_back(render(state, "__cppgm_fini", "fini"));
      state_ = 0;
    }
  }

PA14Lowerer::ExprInfo PA14Lowerer::InferCall(const CPPGMAstNodePtr& node, Scope* scope)
{
    ExprInfo result;
    CallChoice choice = ChooseCall(node, scope);
    result.type = choice.function->child;
    if(result.type && result.type->kind == TYPE_LVALUE_REFERENCE) result.category = "lvalue";
    else if(result.type && result.type->kind == TYPE_RVALUE_REFERENCE) result.category = "xvalue";
    else result.category = "prvalue";
    result.binding = choice.binding;
    return result;
  }

PA14Lowerer::ExprInfo PA14Lowerer::InferUnary(const CPPGMAstNodePtr& node, Scope* scope)
{
    ExprInfo result;
    const string op = PA12Operator(node->value);
    ExprInfo child = Infer(node->children[0], scope);
    TypePtr value = expression_value_type(child);
    if(op == "&") result.type = PointerTo(value);
    else if(op == "*") {
      if(!value || (value->kind != TYPE_POINTER && value->kind != TYPE_ARRAY))
        throw logic_error("cannot dereference expression");
      result.type = value->child;
      result.category = "lvalue";
    } else if(op == "!") result.type = Fundamental("bool");
    else if(op == "++" || op == "--") result.type = value;
    else if(op == "+" && value && value->kind == TYPE_ARRAY)
      result.type = PointerTo(value->child);
    else result.type = IntegralPromotion(value);
    if(op != "*") result.category = op == "++" || op == "--" ? "lvalue" : "prvalue";
    return result;
  }

PA14Lowerer::ExprInfo PA14Lowerer::InferBinary(const CPPGMAstNodePtr& node, Scope* scope)
{
    ExprInfo result;
    const string op = PA12Operator(node->value);
    ExprInfo left = Infer(node->children[0], scope);
    ExprInfo right = Infer(node->children[1], scope);
    if(op == ",") {
      result.type = right.type;
      result.category = right.category;
      return result;
    }
    if(op == "&&" || op == "||" || op == "and" || op == "or" ||
       op == "==" || op == "!=" || op == "not_eq" || op == "<" ||
       op == ">" || op == "<=" || op == ">=") result.type = Fundamental("bool");
    else if(op == "-" && expression_value_type(left) && expression_value_type(right) &&
            expression_value_type(left)->kind == TYPE_POINTER &&
            expression_value_type(right)->kind == TYPE_POINTER)
      result.type = Fundamental("long int");
    else if((op == "+" || op == "-") && expression_value_type(left) &&
            expression_value_type(left)->kind == TYPE_ARRAY)
      result.type = PointerTo(expression_value_type(left)->child);
    else if((op == "+" || op == "-") && expression_value_type(left) &&
            expression_value_type(left)->kind == TYPE_POINTER)
      result.type = expression_value_type(left);
    else if(op == "+" && expression_value_type(right) &&
            (expression_value_type(right)->kind == TYPE_POINTER ||
             expression_value_type(right)->kind == TYPE_ARRAY))
      result.type = expression_value_type(right)->kind == TYPE_ARRAY ?
        PointerTo(expression_value_type(right)->child) : expression_value_type(right);
    else result.type = CommonType(left.type, right.type, op);
    result.category = "prvalue";
    return result;
  }

PA14Lowerer::ExprInfo PA14Lowerer::Infer(const CPPGMAstNodePtr& node, Scope* scope,
                const TypePtr& expected)
{
    if(!node) throw logic_error("missing expression during LowIR lowering");
    if(node->kind == "literal") return InferLiteral(node, expected);
    if(node->kind == "keyword-literal") return InferKeyword(node);
    if(node->kind == "id-expression") return InferIdentifier(node, scope, expected);
    if(node->kind == "parenthesized-expression")
      return node->children.empty() ? ExprInfo() : Infer(node->children[0], scope, expected);
    if(node->kind == "call-expression") return InferCall(node, scope);
    if(node->kind == "unary-expression") return InferUnary(node, scope);
    if(node->kind == "postfix-expression") {
      ExprInfo result;
      ExprInfo child = Infer(node->children[0], scope);
      result.type = expression_value_type(child);
      result.category = "prvalue";
      return result;
    }
    if(node->kind == "binary-expression") return InferBinary(node, scope);
    if(node->kind == "assignment-expression") {
      ExprInfo left = Infer(node->children[0], scope);
      if(left.category != "lvalue") throw logic_error("assignment requires lvalue");
      ExprInfo result;
      result.type = expression_value_type(left);
      result.category = "lvalue";
      return result;
    }
    if(node->kind == "conditional-expression") {
      ExprInfo result;
      ExprInfo when_true = Infer(node->children[1], scope);
      ExprInfo when_false = Infer(node->children[2], scope);
      if(when_true.null_pointer_constant && expression_value_type(when_false) &&
         expression_value_type(when_false)->kind == TYPE_POINTER) result.type = expression_value_type(when_false);
      else if(when_false.null_pointer_constant && expression_value_type(when_true) &&
              expression_value_type(when_true)->kind == TYPE_POINTER) result.type = expression_value_type(when_true);
      else result.type = CommonType(when_true.type, when_false.type);
      result.category = PA12SameType(when_true.type, when_false.type, false) &&
        when_true.category == "lvalue" && when_false.category == "lvalue" ? "lvalue" : "prvalue";
      return result;
    }
    if(node->kind == "subscript-expression") {
      ExprInfo base = Infer(node->children[0], scope);
      TypePtr value = expression_value_type(base);
      if((!value || (value->kind != TYPE_ARRAY && value->kind != TYPE_POINTER)) &&
         node->children.size() > 1) {
        ExprInfo index = Infer(node->children[1], scope);
        value = expression_value_type(index);
      }
      if(!value || (value->kind != TYPE_ARRAY && value->kind != TYPE_POINTER))
        throw logic_error("subscript requires array or pointer");
      ExprInfo result;
      result.type = value->child;
      result.category = "lvalue";
      return result;
    }
    if(node->kind == "member-expression") return InferMember(node, scope);
    if(node->kind == "cast-expression") {
      ExprInfo result;
      result.type = analyzer_.TypeFromTypeId(node->children[0], scope);
      result.category = type_is_reference(result.type) ?
        result.type->kind == TYPE_LVALUE_REFERENCE ? "lvalue" : "xvalue" : "prvalue";
      return result;
    }
    if(node->kind == "sizeof-expression" || node->kind == "type-trait-expression") {
      ExprInfo result;
      result.type = Fundamental("long int");
      result.known_constant = true;
      const CPPGMAstNodePtr child = node->children.empty() ? CPPGMAstNodePtr() : node->children[0];
      TypePtr type;
      if(child && child->kind == "type-id") type = analyzer_.TypeFromTypeId(child, scope);
      else if(child) type = Infer(child, scope).type;
      result.constant = node->kind == "type-trait-expression" ?
        static_cast<long long>(type_alignment(type)) : static_cast<long long>(type_size(type));
      return result;
    }
    if(node->kind == "braced-init-list") {
      ExprInfo result;
      result.type = expected ? expected : Fundamental("int");
      result.category = "lvalue";
      return result;
    }
    throw logic_error("unsupported expression in LowIR lowering: " + node->kind);
  }

} // namespace cppgm_pa14_lowering
