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
    vector<Binding*> candidates;
    bool direct = false;
    bool explicit_member_object = false;
    bool ambiguous_best = false;
    ExprInfo object_info;
    CPPGMAstNodePtr lookup_callee = callee_node;
    while(lookup_callee && lookup_callee->kind == "parenthesized-expression" &&
          !lookup_callee->children.empty())
      lookup_callee = lookup_callee->children[0];
    if(lookup_callee && lookup_callee->kind == "member-expression" &&
       lookup_callee->children.size() >= 2) {
      explicit_member_object = true;
      object_info = Infer(lookup_callee->children[0], scope);
      TypePtr object = expression_value_type(object_info);
      if(PA12Operator(lookup_callee->value) == "->") {
        if(!object || object->kind != TYPE_POINTER) throw logic_error("arrow requires a pointer to class");
        object = type_value(object->child);
      }
      if(lookup_callee->children[0] &&
         lookup_callee->children[0]->kind == "call-expression" &&
         object && object->kind == TYPE_CLASS)
        CollectImplicitConstructor(object, object->owned_scope, true);
      candidates = MemberBindings(object, lookup_callee->children[1]->value);
      if(candidates.empty() && lookup_callee->children[1] &&
         lookup_callee->children[1]->value.compare(0, 8, "operator") == 0) {
        Binding* conversion = FindNamedConversionOperator(object,
          lookup_callee->children[1]->value, scope);
        if(conversion) candidates.push_back(conversion);
      }
      direct = true;
      bool has_direct_function = false;
      for(size_t i = 0; i < candidates.size(); ++i)
        if(candidates[i]->kind == BIND_FUNCTION && function_target_type(candidates[i]->type)) {
          has_direct_function = true;
          break;
        }
      if(!has_direct_function) {
        ExprInfo field = Infer(callee_node, scope);
        TypePtr callable = expression_value_type(field);
        if(callable && callable->kind == TYPE_CLASS) {
          return ChooseCall(MakeMemberCall(callee_node, "operator()", argument_nodes), scope);
        }
        direct = false;
      }
    } else if(lookup_callee && lookup_callee->kind == "id-expression") {
      candidates = Lookup(lookup_callee->value, scope);
      // Ordinary unqualified calls participate in the same associated
      // namespace and hidden-friend lookup as overloaded operators.
      bool suppress_adl = false;
      for(size_t i = 0; i < candidates.size(); ++i)
        if(candidates[i]->is_member) { suppress_adl = true; break; }
      if(lookup_callee->value.find("::") == string::npos && !suppress_adl) {
        set<const Type*> visited_types;
        set<Scope*> visited_scopes;
        for(size_t i = 0; i < arguments.size(); ++i)
          AppendAssociatedOperatorBindings(expression_value_type(arguments[i]),
            lookup_callee->value, candidates, visited_types, visited_scopes);
      }
      // A qualified member-id names the member set of the qualified class
      // directly.  Do not let unqualified lookup in the current member
      // context rebind e.g. Base::operator= to Derived::operator=.
      const size_t separator = lookup_callee->value.rfind("::");
      if(separator != string::npos) {
        const string owner_name = lookup_callee->value.substr(0, separator);
        const string member_name = lookup_callee->value.substr(separator + 2);
        Analyzer::PathTarget owner_target = analyzer_.ResolvePath(scope, owner_name);
        TypePtr owner = owner_target.binding ? type_value(owner_target.binding->type) :
          TypePtr();
        if(owner && owner->kind == TYPE_CLASS) {
          if(member_name == "operator=" &&
             MemberBindings(owner, member_name).empty())
            EnsureImplicitAssignment(owner, false);
          candidates = MemberBindings(owner, member_name);
        }
      }
      if(candidates.empty() && state_ && state_->record && state_->record->member_owner)
        candidates = MemberBindings(state_->record->member_owner, lookup_callee->value);
      for(size_t i = 0; i < candidates.size(); ++i)
        if(candidates[i]->kind == BIND_FUNCTION && function_target_type(candidates[i]->type)) {
          direct = true;
          break;
        }
    }
    if(direct) {
      for(size_t i = 0; i < candidates.size(); ++i) {
        Binding* binding = candidates[i];
        TypePtr function = function_target_type(binding->type);
        if(!function) continue;
        const bool named_destructor = binding->name.size() > 1 &&
          binding->name[0] == '~';
        if(!named_destructor && !IsAccessible(binding, scope)) continue;
        if(named_destructor && arguments.empty() && function->parameters.empty()) {
          best.binding = binding;
          best.function = function;
          best.object = explicit_member_object ? lookup_callee->children[0] :
            CPPGMAstNodePtr(new CPPGMAstNode("keyword-literal", "KW_THIS:this"));
          best.direct = true;
          best.member = binding->is_member && !binding->is_static;
          best.static_member = binding->is_static;
          best.user_defined = 0;
          best.worst = 0;
          best.total = 0;
          break;
        }
        const bool member = binding->is_member && binding->member_owner &&
          binding->kind == BIND_FUNCTION;
        const bool static_member = member && binding->is_static;
        if(member && !static_member && !explicit_member_object) {
          bool implicit_object = callee_node->kind == "id-expression" &&
            callee_node->value.find("::") == string::npos && state_ &&
            state_->record && state_->record->member &&
            !state_->record->static_member;
          if(!implicit_object && state_ && state_->record && state_->record->member_owner) {
            TypePtr current = state_->record->member_owner;
            while(current) {
              if(current == binding->member_owner) { implicit_object = true; break; }
              current = current->direct_base;
            }
          }
          if(!implicit_object) continue;
        }
        if(arguments.size() > function->parameters.size() && !function->variadic) continue;
        if(arguments.size() < function->parameters.size()) {
          bool defaults = true;
          for(size_t p = arguments.size(); p < function->parameters.size(); ++p)
            if(!HasDefaultArgument(binding, p)) { defaults = false; break; }
          if(!defaults) continue;
        }
        int worst = 0;
        int total = 0;
        int user_defined = 0;
        bool viable = true;
        if(member && !static_member) {
          TypePtr object = expression_value_type(object_info);
          if(!explicit_member_object) {
            CPPGMAstNodePtr this_node(new CPPGMAstNode("keyword-literal", "KW_THIS:this"));
            object_info = Infer(this_node, scope);
            object = expression_value_type(object_info);
          }
          if(object && object->kind == TYPE_POINTER) object = type_value(object->child);
          FunctionRecord* member_record = RecordForBinding(binding);
          const bool destructor = (member_record && member_record->destructor) ||
            (binding->name.size() > 1 && binding->name[0] == '~');
          if(!destructor && function->function_lvalue_ref_qualified &&
             object_info.category != "lvalue" && !function->function_const)
            viable = false;
          if(!destructor && function->function_rvalue_ref_qualified &&
             object_info.category == "lvalue")
            viable = false;
          if(!destructor && object && object->is_const && !function->function_const)
            viable = false;
          if(!destructor && object && object->is_volatile && !function->function_volatile)
            viable = false;
          const int object_rank =
            destructor ? 0 :
            ((object && object->is_const && function->function_const ? 0 :
              (function->function_const ? 1 : 0)) +
             (object && object->is_volatile && function->function_volatile ? 0 :
              (function->function_volatile ? 1 : 0)));
          worst = max(worst, object_rank);
          total += object_rank;
        }
        for(size_t a = 0; a < arguments.size(); ++a) {
          const int rank = a < function->parameters.size() ?
            ConversionRank(arguments[a], function->parameters[a]) : 2;
          if(rank < 0) { viable = false; break; }
          if(rank >= 3) ++user_defined;
          worst = max(worst, rank);
          total += rank;
        }
        if(!viable) continue;
        if(!best.binding || user_defined < best.user_defined ||
           (user_defined == best.user_defined &&
            (worst < best.worst || (worst == best.worst && total < best.total)))) {
          best.binding = binding;
          best.function = function;
          best.object = explicit_member_object ? lookup_callee->children[0] :
            CPPGMAstNodePtr(new CPPGMAstNode("keyword-literal", "KW_THIS:this"));
          best.direct = true;
          best.member = member;
          best.static_member = static_member;
          best.user_defined = user_defined;
          best.worst = worst;
          best.total = total;
          ambiguous_best = false;
        } else if(user_defined == best.user_defined && worst == best.worst &&
                  total == best.total &&
                  !PA12SameType(best.function, function, false)) {
          ambiguous_best = true;
        }
      }
      if(ambiguous_best) throw logic_error("ambiguous overload");
      if(!best.binding) {
        string detail = "no viable function";
        for(size_t i = 0; i < candidates.size(); ++i)
          detail += " [" + candidates[i]->qualified_name + "]";
        throw logic_error(detail);
      }
      FunctionRecord* selected = RecordForBinding(best.binding);
      const bool qualified_member_id = lookup_callee &&
        lookup_callee->kind == "id-expression" &&
        lookup_callee->value.find("::") != string::npos;
      const bool destructor_call = selected && selected->destructor;
      if(best.member && !best.static_member && !qualified_member_id &&
         !destructor_call) {
        TypePtr dispatch_object = expression_value_type(Infer(best.object, scope));
        if(dispatch_object && dispatch_object->kind == TYPE_POINTER)
          dispatch_object = type_value(dispatch_object->child);
        size_t virtual_slot = 0;
        if(VirtualSlotForCall(dispatch_object, best.binding, &virtual_slot)) {
          best.direct = false;
          best.virtual_dispatch = true;
          best.virtual_slot = virtual_slot;
          best.virtual_owner = dispatch_object;
        }
      }
      bool pure_virtual_dispatch = best.binding && best.binding->is_pure;
      if(best.virtual_dispatch && best.virtual_owner) {
        size_t semantic_slot = 0;
        size_t expanded_slot = 0;
        if(VirtualSlotForCall(best.virtual_owner, best.binding, &expanded_slot,
                              &semantic_slot) &&
           semantic_slot < best.virtual_owner->virtual_methods.size())
          pure_virtual_dispatch = best.virtual_owner->virtual_methods[semantic_slot].pure;
      }
      if(selected && !pure_virtual_dispatch &&
         !(best.binding && best.binding->is_pure)) {
        selected->needed = true;
        FunctionRecord* base_entry = BaseEntryFor(selected);
        if(base_entry) base_entry->needed = true;
      }
      return best;
    }

    ExprInfo callee = Infer(callee_node, scope);
    best.function = function_target_type(callee.type);
    if(!best.function) {
      TypePtr callable = expression_value_type(callee);
      if(callable && callable->kind == TYPE_CLASS)
        return ChooseCall(MakeMemberCall(callee_node, "operator()", argument_nodes), scope);
      }
    if(!best.function) throw logic_error("expression is not callable");
    best.direct = false;
    return best;
  }

PA14Lowerer::CallChoice PA14Lowerer::ChooseOperatorCall(
    const string& name, const vector<CPPGMAstNodePtr>& argument_nodes, Scope* scope)
{
    CallChoice best;
    vector<ExprInfo> arguments;
    for(size_t i = 0; i < argument_nodes.size(); ++i)
      arguments.push_back(Infer(argument_nodes[i], scope));
    vector<Binding*> candidates = OperatorCandidates(name, arguments, scope);
    if(!arguments.empty()) {
      TypePtr object = expression_value_type(arguments[0]);
      if(object && object->kind == TYPE_CLASS) {
        const vector<Binding*> members = MemberBindings(object, name);
        for(size_t i = 0; i < members.size(); ++i)
          if(find(candidates.begin(), candidates.end(), members[i]) == candidates.end())
            candidates.push_back(members[i]);
      }
    }
    for(size_t i = 0; i < candidates.size(); ++i) {
      Binding* binding = candidates[i];
      if(!binding || binding->kind != BIND_FUNCTION)
        continue;
      TypePtr function = function_target_type(binding->type);
      if(!function || !IsAccessible(binding, scope)) continue;
      const bool member = binding->is_member && !binding->is_static &&
        binding->member_owner;
      const size_t argument_offset = member ? 1 : 0;
      if(member && arguments.empty()) continue;
      if(member) {
        TypePtr object = expression_value_type(arguments[0]);
        if(object && object->kind == TYPE_POINTER) object = type_value(object->child);
        if(!object || object->kind != TYPE_CLASS) continue;
        if(!PA12SameType(object, binding->member_owner, true) &&
           BaseDistance(object, binding->member_owner) < 1) continue;
        if(function->function_lvalue_ref_qualified &&
           arguments[0].category != "lvalue" && !function->function_const) continue;
        if(function->function_rvalue_ref_qualified &&
           arguments[0].category == "lvalue") continue;
        if(object->is_const && !function->function_const) continue;
        if(object->is_volatile && !function->function_volatile) continue;
      }
      if(arguments.size() < argument_offset) continue;
      const size_t explicit_count = arguments.size() - argument_offset;
      if(explicit_count > function->parameters.size() && !function->variadic) continue;
      if(explicit_count < function->parameters.size()) {
        bool defaults = true;
        for(size_t p = explicit_count; p < function->parameters.size(); ++p)
          if(!HasDefaultArgument(binding, p)) { defaults = false; break; }
        if(!defaults) continue;
      }
      int worst = 0;
      int total = 0;
      if(member) {
        TypePtr object = expression_value_type(arguments[0]);
        if(object && object->kind == TYPE_POINTER) object = type_value(object->child);
        const int distance = BaseDistance(object, binding->member_owner);
        const int object_rank = distance >= 1 ? distance : 0;
        worst = max(worst, object_rank);
        total += object_rank;
        if(object && object->is_const && function->function_const) {
          worst = max(worst, 1);
          total += 1;
        }
        if(object && object->is_volatile && function->function_volatile) {
          worst = max(worst, 1);
          total += 1;
        }
      }
      bool viable = true;
      for(size_t a = argument_offset; a < arguments.size(); ++a) {
        const size_t parameter = a - argument_offset;
        const int rank = parameter < function->parameters.size() ?
          ConversionRank(arguments[a], function->parameters[parameter]) : 2;
        if(rank < 0) { viable = false; break; }
        worst = max(worst, rank);
        total += rank;
      }
      if(!viable) continue;
      if(!best.binding || worst < best.worst ||
         (worst == best.worst && total < best.total)) {
        best.binding = binding;
        best.function = function;
        best.object = member ? argument_nodes[0] : CPPGMAstNodePtr();
        best.direct = true;
        best.member = member;
        best.static_member = binding->is_static;
        best.worst = worst;
        best.total = total;
      } else if(worst == best.worst && total == best.total &&
                !PA12SameType(best.function, function, false)) {
        throw logic_error("ambiguous operator overload");
      }
    }
    if(best.binding) {
      FunctionRecord* record = RecordForBinding(best.binding);
      if(record) {
        record->needed = true;
        FunctionRecord* base_entry = BaseEntryFor(record);
        if(base_entry) base_entry->needed = true;
      }
    }
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
    state_->slot_order.push_back(FunctionState::SlotEntry(true, result.str()));
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
    if(function.indirect_result) result.push_back("ret");
    if(function.member && !function.static_member) result.push_back("this");
    CPPGMAstNodePtr declarator;
    if(function.node) declarator = function.constructor || function.destructor ||
      function.value_special_member ?
      ChildOfKind(function.node, "declarator") :
      (function.node->children.size() > 1 ? function.node->children[1] : CPPGMAstNodePtr());
    CPPGMAstNodePtr clause = declarator ? DescendantOfKind(declarator, "parameter-clause") :
      CPPGMAstNodePtr();
    size_t index = function.member && !function.static_member ? 1 : 0;
    if(clause) {
      for(size_t i = 0; i < clause->children.size(); ++i) {
        CPPGMAstNodePtr parameter = clause->children[i];
        if(!parameter || parameter->kind != "parameter-declaration") continue;
        CPPGMAstNodePtr declarator = parameter->children.size() > 1 ? parameter->children[1] : CPPGMAstNodePtr();
        const size_t parameter_index = index++;
        string name = parameter_name(declarator, parameter_index);
        if(function.builtin && name.find("__param") == 0)
          name = "arg" + integer_text(static_cast<long long>(parameter_index));
        result.push_back(name);
      }
    }
    while(result.size() < function.type->parameters.size()) {
      const size_t parameter_index = index++;
      result.push_back((function.builtin ? "arg" : "__param") +
        integer_text(static_cast<long long>(parameter_index)));
    }
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
    plan.global = 0;
    plan.parameter = false;
    plan.parameter_address = false;
    plan.slot_declared = false;
    plan.parameter_operand.clear();
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
    state.environments.push_back(map<string, VariablePlan*>());
    const vector<string> names = ParameterNames(*state.record);
    for(size_t i = 0; i < names.size(); ++i)
      state.reserved_value_names.insert(names[i]);
    for(size_t i = 0; i < state.record->type->parameters.size(); ++i) {
      if(state.record->indirect_result && i == 0) continue;
      const string name = names[i];
      TypePtr source_type = LowParameterSourceType(*state.record, i);
      VariablePlan* parameter = AddVariablePlan(name,
        LowParameterIsByAddress(*state.record, i) ? source_type :
          state.record->type->parameters[i], CPPGMAstNodePtr(), CPPGMAstNodePtr());
      if(parameter) {
        parameter->parameter = true;
        parameter->parameter_address = LowParameterIsByAddress(*state.record, i);
        parameter->parameter_operand = "%" + name;
      }
    }
    if(!state.record->node) return;
    Scope* scope = analyzer_.function_scopes_[state.record->node.get()];
    if(!scope) scope = state.record->scope;
    CPPGMAstNodePtr body = ChildOfKind(state.record->node, "compound-statement");
    if(!body && state.record->node->children.size() > 2)
      body = state.record->node->children[2];
    if(body) PlanStatement(body, scope);
    if(state.record->indirect_result && body) {
      unsigned int return_count = 0;
      CPPGMAstNodePtr expression = FindDirectReturnExpression(body, return_count);
      TypePtr return_type = type_value(SourceReturnType(*state.record));
      if(return_count == 1 && expression && expression->kind == "id-expression" &&
         return_type && return_type->kind == TYPE_CLASS) {
        for(size_t i = state.variables.size(); i > 0; --i) {
          VariablePlan& variable = state.variables[i - 1];
          if(variable.parameter || variable.source_name != expression->value ||
             !PA12SameType(type_value(variable.type), return_type, true) ||
             DestructorHasEffects(variable.type)) continue;
          state.return_slot_plan = &variable;
          variable.initialization_address = "%" + names[0];
          break;
        }
      }
    }
  }

CPPGMAstNodePtr PA14Lowerer::FindDirectReturnExpression(
  const CPPGMAstNodePtr& node, unsigned int& count) const
{
    if(!node) return CPPGMAstNodePtr();
    if(node->kind == "return-statement") {
      ++count;
      return node->children.empty() ? CPPGMAstNodePtr() : node->children[0];
    }
    CPPGMAstNodePtr result;
    for(size_t i = 0; i < node->children.size(); ++i) {
      CPPGMAstNodePtr candidate = FindDirectReturnExpression(node->children[i], count);
      if(!result && candidate) result = candidate;
    }
    return result;
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
    if(variable.global) return "@" + variable.global->symbol;
    return "$" + variable.slot_name;
  }

} // namespace cppgm_pa14_lowering
