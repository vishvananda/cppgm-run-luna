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
      for(size_t i = 0; i < expression->children.size(); ++i) {
        Value value = EmitValue(expression->children[i], scope, variable->type->child);
        if(value.known_constant && is_integral_type(value.type) &&
           is_integral_type(variable->type->child)) {
          value.type = variable->type->child;
          value.operand = integer_text(value.constant);
        } else value = ConvertValue(value, variable->type->child);
        string storage = base;
        if(i != 0) {
          const string index = new_temp();
          AddInstruction(index + " = index i8 " + base + ", " +
            integer_text(static_cast<long long>(i * type_size(variable->type->child))));
          storage = index;
        }
        emit_store(variable->type->child, value.operand, storage);
      }
      return;
    }
    if(!expression) {
      if(variable->type->kind != TYPE_FUNCTION)
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
      Terminate("return void");
      return;
    }
    if(node->children.empty()) { Terminate("return " + low_type(return_type) + " 0"); return; }
    CPPGMAstNodePtr expression = node->children[0];
    if(type_is_reference(return_type)) {
      const string address = EmitAddress(expression, scope);
      Terminate("return ptr " + address);
      return;
    }
    Value value = EmitValue(expression, scope, return_type);
    if(value.known_constant && is_integral_type(value.type) && is_integral_type(return_type) &&
       type_size(return_type) > type_size(value.type) && !is_unsigned_type(return_type)) {
      // PA14 permits canonical widened integral immediates.  Signed long
      // return literals are emitted directly; unsigned aliases retain the
      // explicit conversion boundary used by the reference LowIR.
      Terminate("return " + low_type(return_type) + " " + integer_text(value.constant));
      return;
    }
    value = ConvertValue(value, return_type);
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
          if(found != state_->plans.end() && item->children.size() > 1)
            EmitInitializer(found->second, item->children[1], scope);
          else if(found != state_->plans.end() &&
                  found->second->type->kind != TYPE_ARRAY &&
                  !type_is_reference(found->second->type))
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
    if(function.node && function.node->children.size() > 2)
      EmitStatement(function.node->children[2], scope);
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

void PA14Lowerer::EmitDynamicInitializers(vector<string>& entries)
{
    vector<GlobalRecord*> dynamic;
    for(size_t i = 0; i < globals_.size(); ++i)
      if(globals_[i].dynamic_initializer) dynamic.push_back(&globals_[i]);
    if(dynamic.empty()) return;
    ostringstream out;
    out << "function @__cppgm_init() -> void [role=init] {\n";
    out << "  block ^entry:\n";
    unsigned int temp = 1;
    for(size_t i = 0; i < dynamic.size(); ++i) {
      AddressInit address = StaticAddress(
        InitializerExpression(dynamic[i]->initializer), dynamic[i]->scope);
      if(!address.valid || !address.function) continue;
      out << "    %t" << temp << " = addr @" << address.symbol << "\n";
      out << "    store ptr %t" << temp++ << ", @" << dynamic[i]->symbol << "\n";
    }
    out << "    return void\n";
    out << "}";
    entries.push_back(out.str());
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
