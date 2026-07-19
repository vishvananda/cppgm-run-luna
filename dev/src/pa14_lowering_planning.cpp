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

PA14Lowerer::CallChoice PA14Lowerer::ChooseCall(const CPPGMAstNodePtr& expression, Scope* scope)
{
    if(!expression || expression->children.empty()) throw logic_error("invalid call expression");
    const CPPGMAstNodePtr callee_node = expression->children[0];
    const CPPGMAstNodePtr arguments_node = expression->children.size() > 1 ?
      expression->children[1] : CPPGMAstNodePtr();
    vector<CPPGMAstNodePtr> argument_nodes;
    if(arguments_node)
      argument_nodes = arguments_node->children;
    vector<ExprInfo> arguments;
    for(size_t i = 0; i < argument_nodes.size(); ++i)
      arguments.push_back(Infer(argument_nodes[i], scope));

    CallChoice best;
    if(DirectFunctionName(callee_node, scope)) {
      const vector<Binding*> candidates = Lookup(callee_node->value, scope);
      for(size_t i = 0; i < candidates.size(); ++i) {
        Binding* binding = candidates[i];
        TypePtr function = function_target_type(binding->type);
        if(!function) continue;
        if(arguments.size() > function->parameters.size() && !function->variadic) continue;
        if(arguments.size() < function->parameters.size()) {
          bool defaults = true;
          for(size_t p = arguments.size(); p < function->parameters.size(); ++p)
            if(!HasDefaultArgument(binding, p)) { defaults = false; break; }
          if(!defaults) continue;
        }
        int worst = 0;
        int total = 0;
        bool viable = true;
        for(size_t a = 0; a < arguments.size(); ++a) {
          const int rank = a < function->parameters.size() ?
            ConversionRank(arguments[a], function->parameters[a]) : 2;
          if(rank < 0) { viable = false; break; }
          worst = max(worst, rank);
          total += rank;
        }
        if(!viable) continue;
        if(!best.binding || worst < best.worst ||
           (worst == best.worst && total < best.total)) {
          best.binding = binding;
          best.function = function;
          best.direct = true;
          best.worst = worst;
          best.total = total;
        } else if(worst == best.worst && total == best.total &&
                  !PA12SameType(best.function, function, false)) {
          throw logic_error("ambiguous overload");
        }
      }
      if(!best.binding) throw logic_error("no viable function");
      return best;
    }

    ExprInfo callee = Infer(callee_node, scope);
    best.function = function_target_type(callee.type);
    if(!best.function) throw logic_error("expression is not callable");
    best.direct = false;
    return best;
  }

string PA14Lowerer::new_temp()
{
    while(true) {
      ostringstream name;
      name << "t" << state_->next_temp++;
      if(state_->reserved_value_names.insert(name.str()).second)
        return "%" + name.str();
    }
  }

string PA14Lowerer::new_label(const string& prefix)
{
    ostringstream result;
    result << prefix << "_" << state_->next_label++;
    return result.str();
  }

string PA14Lowerer::new_special_slot(const string& prefix, const string& type)
{
    ostringstream result;
    result << prefix << "__" << state_->next_special++;
    state_->special_slots.push_back(result.str());
    state_->special_slot_types[result.str()] = type;
    return result.str();
  }

void PA14Lowerer::AddInstruction(const string& text)
{
    if(!state_->current || state_->current->terminated)
      throw logic_error("instruction emitted after LowIR terminator");
    state_->current->lines.push_back("    " + text);
  }

void PA14Lowerer::Terminate(const string& text)
{
    AddInstruction(text);
    state_->current->terminated = true;
  }

PA14Lowerer::Block* PA14Lowerer::AddBlock(const string& label)
{
    state_->blocks.push_back(Block(label));
    state_->current = &state_->blocks.back();
    return state_->current;
  }

bool PA14Lowerer::block_is_terminated(const Block* block)
{
    return block && block->terminated;
  }

string PA14Lowerer::parameter_name(const CPPGMAstNodePtr& declarator, size_t index) const
{
    if(!declarator) return "__param" + integer_text(static_cast<long long>(index));
    const string name = declarator_name(declarator);
    return name.empty() ? "__param" + integer_text(static_cast<long long>(index)) :
      last_component(name);
  }

vector<string> PA14Lowerer::ParameterNames(const FunctionRecord& function) const
{
    vector<string> result;
    CPPGMAstNodePtr clause = function.node ? ChildOfKind(function.node->children[1], "parameter-clause") :
      CPPGMAstNodePtr();
    size_t index = 0;
    if(clause) {
      for(size_t i = 0; i < clause->children.size(); ++i) {
        CPPGMAstNodePtr parameter = clause->children[i];
        if(!parameter || parameter->kind != "parameter-declaration") continue;
        CPPGMAstNodePtr declarator = parameter->children.size() > 1 ? parameter->children[1] : CPPGMAstNodePtr();
        result.push_back(parameter_name(declarator, index++));
      }
    }
    while(result.size() < function.type->parameters.size())
      result.push_back("__param" + integer_text(static_cast<long long>(result.size())));
    return result;
  }

CPPGMAstNodePtr PA14Lowerer::InitializerExpression(const CPPGMAstNodePtr& initializer) const
{
    if(!initializer || initializer->children.empty()) return CPPGMAstNodePtr();
    CPPGMAstNodePtr expression = initializer->children[0];
    if(expression && expression->kind == "paren-initializer")
      return expression->children.empty() ? CPPGMAstNodePtr() : expression->children[0];
    return expression;
  }

long long PA14Lowerer::BracedElementCount(const CPPGMAstNodePtr& initializer) const
{
    CPPGMAstNodePtr expression = InitializerExpression(initializer);
    return expression && expression->kind == "braced-init-list" ?
      static_cast<long long>(expression->children.size()) : -1;
  }

TypePtr PA14Lowerer::PlannedType(const CPPGMAstNodePtr& declaration,
                      const CPPGMAstNodePtr& declarator,
                      Scope* scope, const CPPGMAstNodePtr& initializer)
{
    Analyzer::SpecFacts facts;
    TypePtr base = analyzer_.TypeFromSpecSeq(declaration, scope, &facts);
    TypePtr type = analyzer_.BuildDeclarator(declarator, base, scope);
    if(type->kind == TYPE_ARRAY && type->bound == 0) {
      const long long count = BracedElementCount(initializer);
      if(count >= 0) type = ArrayOf(count, type->child);
    }
    if(facts.is_constexpr && type->kind != TYPE_FUNCTION)
      type = CloneWithCv(type, true, false);
    return type;
  }

PA14Lowerer::VariablePlan* PA14Lowerer::AddVariablePlan(const string& name, const TypePtr& type,
                                const CPPGMAstNodePtr& declarator,
                                const CPPGMAstNodePtr& initializer)
{
    if(name.empty()) return 0;
    unsigned int& count = state_->variable_name_counts[name];
    ++count;
    string slot = name;
    if(count > 1) slot += "__shadow" + integer_text(count);
    state_->variables.push_back(VariablePlan());
    VariablePlan& plan = state_->variables.back();
    plan.source_name = name;
    plan.slot_name = slot;
    plan.type = type;
    plan.declarator = declarator;
    plan.initializer = initializer;
    if(declarator) state_->plans[declarator.get()] = &plan;
    if(state_->environments.empty()) state_->environments.push_back(map<string, VariablePlan*>());
    state_->environments.back()[name] = &plan;
    return &plan;
  }

void PA14Lowerer::PlanSimpleDeclaration(const CPPGMAstNodePtr& node, Scope* scope)
{
    if(!node || node->children.empty()) return;
    Analyzer::SpecFacts facts;
    TypePtr base = analyzer_.TypeFromSpecSeq(node->children[0], scope, &facts);
    CPPGMAstNodePtr list = ChildOfKind(node, "init-declarator-list");
    if(!list || facts.is_typedef) return;
    for(size_t i = 0; i < list->children.size(); ++i) {
      CPPGMAstNodePtr item = list->children[i];
      if(!item || item->children.empty()) continue;
      CPPGMAstNodePtr declarator = item->children[0];
      TypePtr type = PlannedType(node->children[0], declarator, scope,
        item->children.size() > 1 ? item->children[1] : CPPGMAstNodePtr());
      if(type->kind == TYPE_FUNCTION) continue;
      AddVariablePlan(declarator_name(declarator), type, declarator,
        item->children.size() > 1 ? item->children[1] : CPPGMAstNodePtr());
    }
  }

void PA14Lowerer::PlanCondition(const CPPGMAstNodePtr& condition, Scope* scope)
{
    if(!condition || condition->kind != "condition-declaration" || condition->children.size() < 3) return;
    Analyzer::SpecFacts facts;
    TypePtr base = analyzer_.TypeFromSpecSeq(condition->children[0], scope, &facts);
    TypePtr type = analyzer_.BuildDeclarator(condition->children[1], base, scope);
    AddVariablePlan(declarator_name(condition->children[1]), type,
      condition->children[1], condition->children[2]);
  }

CPPGMAstNodePtr PA14Lowerer::ChildNamed(const CPPGMAstNodePtr& node, const string& name) const
{
    return ChildOfKind(node, name);
  }

void PA14Lowerer::PlanStatement(const CPPGMAstNodePtr& node, Scope* scope)
{
    if(!node) return;
    if(node->kind == "compound-statement") {
      state_->environments.push_back(map<string, VariablePlan*>());
      for(size_t i = 0; i < node->children.size(); ++i) PlanStatement(node->children[i], scope);
      state_->environments.pop_back();
      return;
    }
    if(node->kind == "simple-declaration" || node->kind == "bit-field-declaration") {
      PlanSimpleDeclaration(node, scope);
      return;
    }
    if(node->kind == "if-statement") {
      state_->environments.push_back(map<string, VariablePlan*>());
      CPPGMAstNodePtr condition = ChildNamed(node, "condition");
      if(condition && !condition->children.empty()) PlanCondition(condition->children[0], scope);
      CPPGMAstNodePtr then_node = ChildNamed(node, "then");
      if(then_node && !then_node->children.empty()) PlanStatement(then_node->children[0], scope);
      CPPGMAstNodePtr else_node = ChildNamed(node, "else");
      if(else_node && !else_node->children.empty()) PlanStatement(else_node->children[0], scope);
      state_->environments.pop_back();
      return;
    }
    if(node->kind == "while-statement") {
      state_->environments.push_back(map<string, VariablePlan*>());
      if(!node->children.empty() && node->children[0]->kind == "condition" &&
         !node->children[0]->children.empty()) PlanCondition(node->children[0]->children[0], scope);
      if(node->children.size() > 1) PlanStatement(node->children[1], scope);
      state_->environments.pop_back();
      return;
    }
    if(node->kind == "do-statement") {
      state_->environments.push_back(map<string, VariablePlan*>());
      if(!node->children.empty()) PlanStatement(node->children[0], scope);
      if(node->children.size() > 1 && node->children[1] &&
         !node->children[1]->children.empty()) PlanCondition(node->children[1]->children[0], scope);
      state_->environments.pop_back();
      return;
    }
    if(node->kind == "for-statement") {
      state_->environments.push_back(map<string, VariablePlan*>());
      if(!node->children.empty() && node->children[0] && !node->children[0]->children.empty())
        PlanStatement(node->children[0]->children[0], scope);
      size_t index = 1;
      if(index < node->children.size() && node->children[index]->kind == "condition") {
        if(!node->children[index]->children.empty()) PlanCondition(node->children[index]->children[0], scope);
        ++index;
      }
      if(index < node->children.size() && node->children[index]->kind == "iteration") ++index;
      if(index < node->children.size()) PlanStatement(node->children[index], scope);
      state_->environments.pop_back();
      return;
    }
    if(node->kind == "switch-statement") {
      state_->environments.push_back(map<string, VariablePlan*>());
      if(!node->children.empty() && node->children[0]->kind == "condition" &&
         !node->children[0]->children.empty()) PlanCondition(node->children[0]->children[0], scope);
      if(node->children.size() > 1) PlanStatement(node->children[1], scope);
      state_->environments.pop_back();
      return;
    }
    if(node->kind == "case-statement" || node->kind == "default-statement" ||
       node->kind == "labeled-statement") {
      for(size_t i = 0; i < node->children.size(); ++i)
        if(i != 0 || node->kind != "case-statement") PlanStatement(node->children[i], scope);
      return;
    }
  }

void PA14Lowerer::PlanFunction(FunctionState& state)
{
    state.variables.reserve(512);
    state.environments.push_back(map<string, VariablePlan*>());
    const vector<string> names = ParameterNames(*state.record);
    for(size_t i = 0; i < names.size(); ++i)
      state.reserved_value_names.insert(names[i]);
    for(size_t i = 0; i < state.record->type->parameters.size(); ++i)
      AddVariablePlan(names[i], state.record->type->parameters[i], CPPGMAstNodePtr(), CPPGMAstNodePtr());
    if(!state.record->node || state.record->node->children.size() < 3) return;
    Scope* scope = analyzer_.function_scopes_[state.record->node.get()];
    if(!scope) scope = state.record->scope;
    PlanStatement(state.record->node->children[2], scope);
  }

string PA14Lowerer::FunctionSymbolForBinding(Binding* binding, const TypePtr& fallback) const
{
    if(binding) {
      FunctionRecord* record = RecordForBinding(binding);
      if(record) return record->symbol;
      const string base = low_symbol_component(binding->qualified_name);
      for(size_t i = 0; i < functions_.size(); ++i)
        if(functions_[i].qualified_name == binding->qualified_name &&
           (!fallback || PA12SameType(functions_[i].type, function_target_type(fallback), true)))
          return functions_[i].symbol;
      return base;
    }
    FunctionRecord* record = FindFunction(last_component(""), fallback);
    return record ? record->symbol : string();
  }

string PA14Lowerer::GlobalSymbolForBinding(Binding* binding) const
{
    if(!binding) return string();
    GlobalRecord* global = FindGlobal(binding->qualified_name);
    return global ? global->symbol : low_symbol_component(binding->qualified_name);
  }

PA14Lowerer::VariablePlan* PA14Lowerer::LocalForName(const string& name) const
{
    return FindLocalPlan(name);
  }

string PA14Lowerer::StorageForVariable(const VariablePlan& variable) const
{
    return "$" + variable.slot_name;
  }

} // namespace cppgm_pa14_lowering
