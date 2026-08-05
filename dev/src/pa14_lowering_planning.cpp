#include "pa14_lowering.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <functional>
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
	const bool force_indirect = expression->indirect_function_call;
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
      bool assignment_call = lookup_callee->children[1] &&
        lookup_callee->children[1]->value == "operator=";
      if(!assignment_call)
        for(size_t i = 0; i < candidates.size(); ++i) {
          FunctionRecord* candidate = RecordForBinding(candidates[i]);
          if(candidate && candidate->member_template &&
             LastComponent(candidate->qualified_name).find("operator=") == 0) {
            assignment_call = true;
            break;
          }
        }
      if(direct && assignment_call) {
        TypePtr assignment_owner = type_value(object_info.type);
        if(assignment_owner && assignment_owner->kind == TYPE_CLASS) {
          bool has_direct_assignment = false;
          bool has_direct_move_assignment = false;
          for(size_t candidate = 0; candidate < candidates.size(); ++candidate) {
            FunctionRecord* record = RecordForBinding(candidates[candidate]);
            if(!record || record->member_template) continue;
            has_direct_assignment = true;
            if(record->move_assignment) has_direct_move_assignment = true;
          }
          if(!has_direct_assignment)
            (void)EnsureImplicitAssignment(assignment_owner, false);
          if(!arguments.empty() && arguments[0].category != "lvalue" &&
             !has_direct_move_assignment)
            (void)EnsureImplicitAssignment(assignment_owner, true);
          const vector<Binding*> ordinary = MemberBindings(assignment_owner, "operator=");
          for(size_t i = 0; i < ordinary.size(); ++i)
            if(find(candidates.begin(), candidates.end(), ordinary[i]) == candidates.end())
              candidates.push_back(ordinary[i]);
        }
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
	if(direct && !force_indirect) {
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
				implicit_object = current == binding->member_owner ||
					IsDerivedFrom(current, binding->member_owner);
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
			int rank = a < function->parameters.size() ?
				ConversionRank(arguments[a], function->parameters[a]) : 2;
			// Empty braced-init-lists have no standalone expression type.  Their
			// target is the parameter being considered: class parameters use the
			// aggregate-construction path, and an array reference can bind the
			// value-initialized temporary materialized by EmitReferenceArgument.
			if(rank < 0 && a < function->parameters.size() &&
				argument_nodes[a] && argument_nodes[a]->kind == "braced-init-list" &&
				argument_nodes[a]->children.empty()) {
				const TypePtr parameter_type = type_value(function->parameters[a]);
				if(parameter_type && parameter_type->kind == TYPE_ARRAY &&
					function->parameters[a]->kind == TYPE_LVALUE_REFERENCE)
					rank = 2;
			}
			if(a < function->parameters.size() && argument_nodes[a] &&
				argument_nodes[a]->kind == "braced-init-list" &&
				IsInitializerListType(function->parameters[a]) &&
				InitializerListArgumentViable(argument_nodes[a], function->parameters[a], scope))
				rank = 0;
			if(rank < 0 && a < function->parameters.size() &&
				argument_nodes[a] && argument_nodes[a]->kind == "braced-init-list" &&
				!argument_nodes[a]->children.empty() &&
				function->parameters[a] && type_is_reference(function->parameters[a])) {
				const TypePtr parameter_type = type_value(function->parameters[a]);
				if(parameter_type && parameter_type->kind == TYPE_ARRAY &&
					(parameter_type->bound < 0 ||
					 parameter_type->bound == static_cast<long long>(argument_nodes[a]->children.size())) &&
					(function->parameters[a]->kind == TYPE_RVALUE_REFERENCE ||
					 (function->parameters[a]->kind == TYPE_LVALUE_REFERENCE &&
					  parameter_type->child && parameter_type->child->is_const))) {
					bool elements_viable = true;
					for(size_t element = 0; element < argument_nodes[a]->children.size(); ++element) {
						ExprInfo element_info = Infer(argument_nodes[a]->children[element], scope);
						if(ConversionRank(element_info, parameter_type->child) < 0) {
							elements_viable = false;
							break;
						}
					}
					if(elements_viable)
						rank = function->parameters[a]->kind == TYPE_RVALUE_REFERENCE ? 1 : 2;
				}
			}
			if(rank < 0) { viable = false; break; }
          if(rank >= 3) ++user_defined;
          worst = max(worst, rank);
          total += rank;
        }
        if(!viable) continue;
        FunctionRecord* candidate_record = RecordForBinding(binding);
        FunctionRecord* best_record = RecordForBinding(best.binding);
        const bool prefer_non_template = best_record && candidate_record &&
          best_record->member_template && !candidate_record->member_template;
        // A non-template and a function-template overload with identical
        // conversion ranks are ordered by the non-template candidate.  Keep
        // that ordering symmetric: the old replacement-only check preferred
        // the non-template when it was visited second, but still diagnosed an
        // ambiguity when the template was visited second.
        const bool same_rank = best.binding && user_defined == best.user_defined &&
          worst == best.worst && total == best.total;
        const bool prefer_existing_non_template = same_rank && best_record &&
          candidate_record && !best_record->member_template &&
          candidate_record->member_template;
        if(!best.binding || prefer_non_template || user_defined < best.user_defined ||
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
        } else if(prefer_existing_non_template) {
          continue;
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
      best.project_base_path = qualified_member_id;
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
         (!state_ || !state_->unevaluated_context) &&
         !(best.binding && best.binding->is_pure)) {
        MarkFunctionNeeded(selected);
        FunctionRecord* base_entry = BaseEntryFor(selected);
        if(base_entry) MarkFunctionNeeded(base_entry);
      }
		return best;
	}
	if(force_indirect) {
		ExprInfo callee = Infer(callee_node, scope);
		best.function = function_target_type(callee.type);
		if(!best.function) {
			TypePtr callable = expression_value_type(callee);
			if(callable && callable->kind == TYPE_CLASS) {
				return ChooseCall(MakeMemberCall(callee_node, "operator()", argument_nodes), scope);
			}
		}
		if(!best.function) throw logic_error("expression is not callable");
		best.direct = false;
		return best;
	}
	ExprInfo callee = Infer(callee_node, scope);
    best.function = function_target_type(callee.type);
    if(!best.function) {
      TypePtr callable = expression_value_type(callee);
      if(callable && callable->kind == TYPE_CLASS) {
        return ChooseCall(MakeMemberCall(callee_node, "operator()", argument_nodes), scope);
      }
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
      int user_defined = 0;
      for(size_t a = argument_offset; a < arguments.size(); ++a) {
        const size_t parameter = a - argument_offset;
        int rank = parameter < function->parameters.size() ?
          ConversionRank(arguments[a], function->parameters[parameter]) : 2;
        if(parameter < function->parameters.size() && a < argument_nodes.size() &&
           argument_nodes[a] && argument_nodes[a]->kind == "braced-init-list" &&
           IsInitializerListType(function->parameters[parameter]) &&
           InitializerListArgumentViable(argument_nodes[a],
             function->parameters[parameter], scope)) rank = 0;
        if(rank < 0) { viable = false; break; }
        if(rank >= 3) ++user_defined;
        worst = max(worst, rank);
        total += rank;
      }
      if(!viable) continue;
      FunctionRecord* candidate_record = RecordForBinding(binding);
      FunctionRecord* best_record = RecordForBinding(best.binding);
      const bool same_rank = best.binding && user_defined == best.user_defined &&
        worst == best.worst && total == best.total;
      const bool prefer_candidate_non_template = best_record && candidate_record &&
        best_record->member_template && !candidate_record->member_template;
      const bool prefer_existing_non_template = same_rank && best_record &&
        candidate_record && !best_record->member_template &&
        candidate_record->member_template;
      if(!best.binding || prefer_candidate_non_template || user_defined < best.user_defined ||
         (user_defined == best.user_defined &&
          (worst < best.worst || (worst == best.worst && total < best.total)))) {
        best.binding = binding;
        best.function = function;
        best.object = member ? argument_nodes[0] : CPPGMAstNodePtr();
        best.direct = true;
        best.member = member;
        best.static_member = binding->is_static;
        best.user_defined = user_defined;
        best.worst = worst;
        best.total = total;
      } else if(prefer_existing_non_template) {
        continue;
      } else if(worst == best.worst && total == best.total &&
                !PA12SameType(best.function, function, false)) {
        throw logic_error("ambiguous operator overload");
      }
    }
    if(best.binding && (!state_ || !state_->unevaluated_context)) {
      FunctionRecord* record = RecordForBinding(best.binding);
      if(record) {
        MarkFunctionNeeded(record);
        FunctionRecord* base_entry = BaseEntryFor(record);
        if(base_entry) MarkFunctionNeeded(base_entry);
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
    CPPGMAstNodePtr declaration_clause;
    if(function.member && function.member_owner && function.source_type) {
      const TypePtr source = function_target_type(function.source_type);
      const vector<Binding*> candidates = MemberBindings(function.member_owner,
        LastComponent(function.qualified_name));
      for(size_t candidate = 0; candidate < candidates.size() && !declaration_clause;
          ++candidate) {
        Binding* binding = candidates[candidate];
        TypePtr binding_type = binding ? function_target_type(binding->type) : TypePtr();
        if(!binding || binding->kind != BIND_FUNCTION || !binding->declaration ||
           !binding_type || !PA12SameType(binding_type, source, false)) continue;
        CPPGMAstNodePtr binding_declarator;
        if(binding->declaration->kind == "function-definition" &&
           binding->declaration->children.size() > 1)
          binding_declarator = binding->declaration->children[1];
        else if(binding->declaration->kind == "simple-declaration") {
          const CPPGMAstNodePtr list = ChildOfKind(binding->declaration,
            "init-declarator-list");
          if(list) for(size_t item = 0; item < list->children.size(); ++item)
            if(list->children[item] && !list->children[item]->children.empty() &&
               LastComponent(declarator_name(list->children[item]->children[0])) ==
                 LastComponent(function.qualified_name)) {
              binding_declarator = list->children[item]->children[0];
              break;
            }
        }
        declaration_clause = binding_declarator ?
          DescendantOfKind(binding_declarator, "parameter-clause") :
          CPPGMAstNodePtr();
      }
    }
    size_t index = function.member && !function.static_member ? 1 : 0;
    if(clause) {
      for(size_t i = 0; i < clause->children.size(); ++i) {
        CPPGMAstNodePtr parameter = clause->children[i];
        if(!parameter || parameter->kind != "parameter-declaration") continue;
        CPPGMAstNodePtr declarator = parameter->children.size() > 1 ? parameter->children[1] : CPPGMAstNodePtr();
        const size_t parameter_index = index++;
        string name = parameter_name(declarator, parameter_index);
        if(name.find("__param") == 0 && declaration_clause &&
           i < declaration_clause->children.size()) {
          const CPPGMAstNodePtr declaration_parameter = declaration_clause->children[i];
          const CPPGMAstNodePtr declaration_declarator = declaration_parameter &&
            declaration_parameter->children.size() > 1 ?
            declaration_parameter->children[1] : CPPGMAstNodePtr();
          const string declared_name = parameter_name(declaration_declarator,
            parameter_index);
          if(declared_name.find("__param") != 0) name = declared_name;
        }
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
    if(expression && expression->kind == "braced-init-list") {
      if(expression->children.size() == 1 && expression->children[0] &&
         expression->children[0]->kind == "literal" &&
         expression->children[0]->value.find('"') != string::npos)
        return static_cast<long long>(decode_string_literal(
          expression->children[0]->value).size());
      return static_cast<long long>(expression->children.size());
    }
    if(expression && expression->kind == "literal" &&
       expression->value.find('"') != string::npos)
      return static_cast<long long>(decode_string_literal(expression->value).size());
    return -1;
  }
PA14Lowerer::FunctionRecord* PA14Lowerer::EnsureAggregateConstructor(const TypePtr& raw_type)
{
    return EnsureAggregateConstructorForArguments(raw_type,
      static_cast<size_t>(-1));
  }
PA14Lowerer::FunctionRecord* PA14Lowerer::EnsureAggregateConstructorForArguments(
    const TypePtr& raw_type, size_t argument_count)
{
    TypePtr owner = type_value(raw_type);
    if(!owner || owner->kind != TYPE_CLASS || !owner->owned_scope) return 0;
    vector<Binding*> existing_constructors = MemberBindings(owner, LastComponent(owner->name));
    for(size_t i = 0; i < existing_constructors.size(); ++i) {
      FunctionRecord* existing = RecordForBinding(existing_constructors[i]);
      if(existing && existing->constructor && !existing->implicit_constructor &&
         !existing->defaulted && !existing->deleted && !existing->aggregate_constructor)
        return 0;
    }
    vector<TypePtr> member_parameters;
    vector<string> member_names;
    for(size_t i = 0; i < owner->class_members.size(); ++i) {
      const ClassMemberInfo& member = owner->class_members[i];
      if(member.is_static || member.name.empty() || !member.type) continue;
      if(argument_count != static_cast<size_t>(-1) &&
         member_parameters.size() >= argument_count) break;
      const vector<Binding*> field_bindings = DirectBindings(owner->owned_scope, member.name);
      for(size_t j = 0; j < field_bindings.size(); ++j)
        if(field_bindings[j]->kind == BIND_VARIABLE && field_bindings[j]->is_member &&
           !field_bindings[j]->is_static && field_bindings[j]->access != "public")
          return 0;
      if(member.initializer) return 0;
      member_parameters.push_back(member.type);
      member_names.push_back(member.name);
    }
    if(member_parameters.empty()) return 0;
    const string name = LastComponent(owner->name);
    const string qname = TypeQualifiedName(owner) + "::" +
      special_member_symbol_name(owner, name);
    TypePtr source = FunctionOf(member_parameters, false, Fundamental("void"), false);
    const string key = function_key(qname, source);
    map<string, FunctionRecord*>::const_iterator found = function_by_key_.find(key);
    if(found != function_by_key_.end()) return found->second;
    CPPGMAstNodePtr special(new CPPGMAstNode("special-member-definition", name));
    CPPGMAstNodePtr declarator(new CPPGMAstNode("declarator"));
    declarator->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", name)));
    CPPGMAstNodePtr clause(new CPPGMAstNode("parameter-clause"));
    for(size_t i = 0; i < member_names.size(); ++i) {
      CPPGMAstNodePtr parameter(new CPPGMAstNode("parameter-declaration"));
      parameter->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("type-specifier")));
      CPPGMAstNodePtr parameter_declarator(new CPPGMAstNode("declarator"));
      parameter_declarator->children.push_back(CPPGMAstNodePtr(
        new CPPGMAstNode("identifier", member_names[i])));
      parameter->children.push_back(parameter_declarator);
      clause->children.push_back(parameter);
    }
    declarator->children.push_back(clause);
    special->children.push_back(declarator);
    special->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("compound-statement")));
    Binding binding(BIND_FUNCTION, name, source);
    binding.qualified_name = qname;
    binding.is_member = true;
    binding.is_static = false;
    binding.member_owner = owner;
    binding.access = "public";
    binding.declaration = special;
    owner->owned_scope->add(binding);
    functions_.push_back(FunctionRecord());
    FunctionRecord* record = &functions_.back();
    function_by_key_[key] = record;
    record->node = special;
    record->scope = owner->owned_scope;
    record->source_type = source;
    vector<TypePtr> parameters;
    parameters.push_back(PointerTo(owner));
    parameters.insert(parameters.end(), member_parameters.begin(), member_parameters.end());
    record->type = FunctionOf(parameters, false, Fundamental("void"), false);
    record->member_owner = owner;
    record->qualified_name = qname;
    record->member = true;
    record->static_member = false;
    record->constructor = true;
    record->aggregate_constructor = true;
    record->definition = true;
    record->template_instantiation = owner->template_specialization;
    record->weak_binding = record->template_instantiation;
    if(owner->template_specialization) {
      record->template_primary = owner->template_primary;
      record->template_arguments = owner->template_arguments;
    }
    BuildFunctionABI(*record);
    const string base = low_symbol_component(qname);
    record->symbol = base + "__ov2";
    unsigned int suffix = 2;
    while(true) {
      bool collision = false;
      for(size_t i = 0; i + 1 < functions_.size(); ++i)
        if(functions_[i].symbol == record->symbol) { collision = true; break; }
      if(!collision) break;
      record->symbol = base + "__ov" + integer_text(static_cast<long long>(++suffix));
    }
    return record;
  }
TypePtr PA14Lowerer::ArithmeticCommonType(const TypePtr& left, const TypePtr& right,
                                          const string& op) const
{
    TypePtr common_left = left;
    TypePtr common_right = right;
    if(common_left && common_left->kind == TYPE_CLASS && common_right &&
       common_right->kind != TYPE_CLASS) {
      Binding* conversion = FindConversionOperator(common_left, common_right, false);
      if(conversion) {
        TypePtr function = function_target_type(conversion->type);
        common_left = function ? type_value(function->child) : common_left;
      }
    } else if(common_right && common_right->kind == TYPE_CLASS && common_left &&
              common_left->kind != TYPE_CLASS) {
      Binding* conversion = FindConversionOperator(common_right, common_left, false);
      if(conversion) {
        TypePtr function = function_target_type(conversion->type);
        common_right = function ? type_value(function->child) : common_right;
      }
    } else if(common_left && common_right && common_left->kind == TYPE_CLASS &&
              common_right->kind == TYPE_CLASS) {
      const vector<Binding*> conversions = ConversionBindings(common_left);
      for(size_t conversion = 0; conversion < conversions.size(); ++conversion) {
        TypePtr function = function_target_type(conversions[conversion]->type);
        TypePtr result_type = function ? type_value(function->child) : TypePtr();
        if(result_type && FindConversionOperator(common_right, result_type, false)) {
          common_left = result_type;
          common_right = result_type;
          break;
        }
      }
    }
    return CommonType(common_left, common_right, op);
  }
bool PA14Lowerer::AppendAggregateConstructorDefaults(
    const TypePtr& raw_object_type, const vector<CPPGMAstNodePtr>& input,
    vector<CPPGMAstNodePtr>* arguments)
{
    TypePtr object_type = type_value(raw_object_type);
    if(!object_type || object_type->kind != TYPE_CLASS || !arguments)
      return false;
    if(HasUserProvidedConstructor(object_type)) return false;
    const auto has_out_of_line_default = [this](const TypePtr& raw_type) {
      const TypePtr type = type_value(raw_type);
      if(!type || type->kind != TYPE_CLASS || !type->owned_scope) return false;
      const vector<Binding*> constructors = MemberBindings(type,
        LastComponent(type->name));
      for(size_t i = 0; i < constructors.size(); ++i) {
        FunctionRecord* record = RecordForBinding(constructors[i]);
        if(!record || !record->constructor || record->deleted ||
           record->implicit_constructor || record->aggregate_constructor ||
           record->copy_constructor || record->move_constructor ||
           record->defaulted) continue;
        TypePtr signature = function_target_type(constructors[i]->type);
        if(!signature) continue;
        bool defaultable = true;
        for(size_t parameter = 0; parameter < signature->parameters.size(); ++parameter)
          if(!HasDefaultArgument(constructors[i], parameter)) {
            defaultable = false;
            break;
          }
        if(defaultable && !ChildOfKind(record->node, "compound-statement"))
          return true;
      }
      return false;
    };
    size_t initialized_members = 0;
    bool aggregate_class_tail = false;
    for(size_t member = 0; member < object_type->class_members.size(); ++member) {
      const ClassMemberInfo& field = object_type->class_members[member];
      if(field.is_static || field.name.empty() || !field.type) continue;
      if(initialized_members++ < arguments->size()) continue;
      const TypePtr field_type = type_value(field.type);
      if(field_type && (field_type->kind == TYPE_CLASS ||
                        field_type->kind == TYPE_ARRAY)) {
        // Keep an out-of-line/default-only member in the generated aggregate
        // body.  Its declaration has no inline body to replay here, while an
        // inline default constructor can be materialized as the synthetic
        // aggregate argument used by the earlier object-lowering contract.
        if(field_type->kind == TYPE_CLASS && has_out_of_line_default(field_type))
          return false;
        aggregate_class_tail = true;
        break;
      }
    }
    if(!aggregate_class_tail) return false;
    initialized_members = 0;
    for(size_t member = 0; member < object_type->class_members.size(); ++member) {
      const ClassMemberInfo& field = object_type->class_members[member];
      if(field.is_static || field.name.empty() || !field.type) continue;
      if(initialized_members++ < input.size()) continue;
      const TypePtr field_type = type_value(field.type);
      if(field_type && field_type->kind == TYPE_CLASS) {
        CPPGMAstNodePtr default_call(new CPPGMAstNode("call-expression"));
        default_call->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
          "id-expression", LastComponent(field_type->name))));
        default_call->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
          "argument-list")));
        arguments->push_back(default_call);
      } else if(field_type && field_type->kind == TYPE_ARRAY)
        arguments->push_back(CPPGMAstNodePtr(new CPPGMAstNode("braced-init-list")));
      else arguments->push_back(CPPGMAstNodePtr(new CPPGMAstNode("literal", "0")));
    }
    return true;
  }
bool PA14Lowerer::EmitStringArrayInitializer(VariablePlan* variable,
    const CPPGMAstNodePtr& expression, const string& base, Scope* scope)
{
    (void)scope;
    if(!variable || !variable->type || variable->type->kind != TYPE_ARRAY ||
       !expression || expression->kind != "braced-init-list" ||
       expression->children.size() != 1 || !expression->children[0] ||
       expression->children[0]->kind != "literal" ||
       expression->children[0]->value.find('"') == string::npos ||
       is_user_defined_string_literal(expression->children[0]->value)) return false;
    const vector<unsigned char> bytes = decode_string_literal(expression->children[0]->value);
    const long long bound = variable->type->bound >= 0 ? variable->type->bound :
      static_cast<long long>(bytes.size());
    for(long long byte = 0; byte < bound && byte < static_cast<long long>(bytes.size()); ++byte) {
      string storage = base;
      if(byte != 0) {
        const string index = new_temp();
        AddInstruction(index + " = index i8 " + base + ", " +
          integer_text(byte * static_cast<long long>(type_size(variable->type->child))));
        storage = index;
      }
      emit_store(variable->type->child, integer_text(bytes[byte]), storage);
    }
    return true;
  }
bool PA14Lowerer::EmitAggregateOmittedField(const ClassMemberInfo& member,
    const CPPGMAstNodePtr& this_node, Scope* scope)
{
    const TypePtr field_type = type_value(member.type);
    if(!field_type) return false;
    const string this_address = EmitValue(this_node, scope).operand;
    const string address = new_temp();
    AddInstruction(address + " = index i8 " + this_address + ", " +
      integer_text(member.offset));
    if(field_type->kind == TYPE_CLASS) {
      if(EmitConstructorAt(field_type, address, vector<CPPGMAstNodePtr>(), scope, true))
        return true;
      CPPGMAstNodePtr empty(new CPPGMAstNode("braced-init-list"));
      EmitAggregateAt(address, field_type, empty, scope);
    } else if(field_type->kind == TYPE_ARRAY) {
      CPPGMAstNodePtr empty(new CPPGMAstNode("braced-init-list"));
      EmitAggregateAt(address, field_type, empty, scope);
    } else emit_store(member.type, "0", address);
    return true;
  }
bool PA14Lowerer::EmitAggregateClassParameter(FunctionRecord& function,
    const ClassMemberInfo& member, const string& address,
    const vector<string>& names, size_t* parameter)
{
    if(!parameter || *parameter >= names.size()) return false;
    const TypePtr field_type = type_value(member.type);
    const bool class_value_parameter = field_type && field_type->kind == TYPE_CLASS &&
      !type_is_reference(member.type);
    if(!class_value_parameter) return false;
    const size_t index = *parameter;
    const bool by_address = LowParameterIsByAddress(function, index);
    if(!by_address) {
      const string source_address = new_temp();
      AddInstruction(source_address + " = addr $" + names[index]);
      AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(member.type))) +
        "x" + integer_text(static_cast<long long>(type_alignment(member.type))) +
        " " + source_address + ", " + address);
      ++*parameter;
      return true;
    }
    FunctionRecord* value_member = EnsureImplicitCopyConstructor(member.type, true);
    if(!value_member || value_member->deleted)
      value_member = EnsureImplicitCopyConstructor(member.type, false);
    if(!value_member || value_member->deleted) return false;
    MarkFunctionNeeded(value_member);
    FunctionRecord* call_member = value_member;
    const bool need_base_entry = !BaseEntryFor(value_member) &&
      !value_member->template_instantiation && !function.template_instantiation &&
      !function.synthesized_value_member && !function.aggregate_constructor &&
      (!function.member_owner || !function.member_owner->template_specialization);
    if(need_base_entry) EnsureConstructorBaseEntry(value_member);
    FunctionRecord* base_member = BaseEntryFor(value_member);
    if(base_member) {
      MarkFunctionNeeded(base_member);
      call_member = base_member;
    }
    AddInstruction("call void @" + call_member->symbol + "(" + address + ", %" +
      names[index] + ")");
    ++*parameter;
    return true;
  }
bool PA14Lowerer::EmitReferenceConversionUpdate(const CPPGMAstNodePtr& node,
    Scope* scope, Value* result)
{
    if(!node || node->children.empty() || !result) return false;
    const CPPGMAstNodePtr child_node = node->children[0];
    ExprInfo child_info = Infer(child_node, scope);
    TypePtr type = expression_value_type(child_info);
    Binding* conversion = FindContextConversionOperator(type, false, false);
    TypePtr function = conversion ? function_target_type(conversion->type) : TypePtr();
    if(!conversion || !function || !type_is_reference(function->child) ||
       function->child->kind != TYPE_LVALUE_REFERENCE) return false;
    Value converted = EmitContextConversion(child_node, scope, false, false);
    if(!converted.lvalue || converted.operand.empty() || !converted.type) return false;
    Value old;
    old.type = converted.type;
    old.operand = emit_load(converted.operand, converted.type);
    Value updated;
    updated.type = converted.type;
    updated.lvalue = true;
    updated.operand = new_temp();
    AddInstruction(updated.operand + " = binary " +
      (PA12Operator(node->value) == "++" ? "add" : "sub") + " " +
      low_type(converted.type) + " " + old.operand + ", 1");
    string store_address = converted.operand;
    if(node->kind == "postfix-expression") {
      Value store_conversion = EmitContextConversion(child_node, scope, false, false);
      if(store_conversion.lvalue && !store_conversion.operand.empty())
        store_address = store_conversion.operand;
    }
    emit_store(converted.type, updated.operand, store_address);
    *result = node->kind == "postfix-expression" ? old : updated;
    return true;
  }
bool PA14Lowerer::ContainsAutoType(const TypePtr& raw) const
{
    if(!raw) return false;
    if(raw->kind == TYPE_FUNDAMENTAL && raw->name == "auto") return true;
    if(raw->child && ContainsAutoType(raw->child)) return true;
    if(raw->kind == TYPE_FUNCTION) {
      for(size_t i = 0; i < raw->parameters.size(); ++i)
        if(ContainsAutoType(raw->parameters[i])) return true;
    }
    return false;
  }
PA14Lowerer::ExprInfo PA14Lowerer::InferAutoInitializer(
    const CPPGMAstNodePtr& initializer, Scope* scope)
{
    CPPGMAstNodePtr expression = InitializerExpression(initializer);
    if(!expression) throw logic_error("auto declaration requires an initializer");
    if(expression->kind == "braced-init-list") {
      return InferInitializerListAuto(expression, scope);
    }
    return Infer(expression, scope);
  }
TypePtr PA14Lowerer::DeduceAutoType(const TypePtr& declared,
                                    const CPPGMAstNodePtr& initializer,
                                    Scope* scope)
{
    if(!declared || !ContainsAutoType(declared)) return declared;
    const ExprInfo source_info = InferAutoInitializer(initializer, scope);
    TypePtr source = source_info.type;
    if(!source) throw logic_error("auto initializer has no type");
    TypePtr source_value = type_value(source);
    // A by-value placeholder follows the ordinary array/function adjustment
    // before the placeholder is substituted.  References retain the source
    // value and category, which is the distinction that makes auto&& useful.
    const auto without_top_cv = [](const TypePtr& original) -> TypePtr {
      if(!original) return original;
      TypePtr result(new Type(*original));
      result->is_const = false;
      result->is_volatile = false;
      return result;
    };
    if(declared->kind != TYPE_LVALUE_REFERENCE &&
       declared->kind != TYPE_RVALUE_REFERENCE) {
      if(source_value && source_value->kind == TYPE_ARRAY)
        source_value = PointerTo(source_value->child);
      else source_value = without_top_cv(source_value);
    }
    function<TypePtr(const TypePtr&, const TypePtr&)> substitute;
    substitute = [&](const TypePtr& pattern, const TypePtr& value) -> TypePtr {
      if(!pattern) return value;
      if(pattern->kind == TYPE_FUNDAMENTAL && pattern->name == "auto") {
        if(!value) throw logic_error("auto initializer has no deduced type");
        return CloneWithCv(value, pattern->is_const, pattern->is_volatile);
      }
      if(pattern->kind == TYPE_POINTER) {
        if(!value || value->kind != TYPE_POINTER)
          throw logic_error("auto pointer initializer has incompatible type");
        return PointerTo(substitute(pattern->child, value->child));
      }
      if(pattern->kind == TYPE_ARRAY) {
        if(!value || value->kind != TYPE_ARRAY)
          throw logic_error("auto array initializer has incompatible type");
        const long long bound = pattern->bound >= 0 ? pattern->bound : value->bound;
        return ArrayOf(bound, substitute(pattern->child, value->child));
      }
      if(ContainsAutoType(pattern))
        throw logic_error("unsupported auto declarator shape");
      return pattern;
    };
    if(declared->kind == TYPE_LVALUE_REFERENCE ||
       declared->kind == TYPE_RVALUE_REFERENCE) {
      const TypeKind reference_kind = declared->kind == TYPE_LVALUE_REFERENCE ||
        source_info.category == "lvalue" ||
        (source && source->kind == TYPE_LVALUE_REFERENCE) ?
        TYPE_LVALUE_REFERENCE : TYPE_RVALUE_REFERENCE;
      TypePtr referred = substitute(declared->child, source_value);
      return ReferenceTo(reference_kind, referred);
    }
    return substitute(declared, source_value);
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
    if(type->kind != TYPE_FUNCTION && ContainsAutoType(type))
      type = DeduceAutoType(type, initializer, scope);
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
		VariablePlan* plan = AddVariablePlan(declarator_name(declarator), type, declarator,
        item->children.size() > 1 ? item->children[1] : CPPGMAstNodePtr());
		map<const CPPGMAstNode*, GlobalRecord*>::const_iterator local_static =
			local_static_plans_.find(declarator.get());
		if(plan && local_static != local_static_plans_.end())
      plan->global = local_static->second;
    // Replayed template bodies can carry a fresh declarator node while the
    // semantic scope retains the binding whose qualified name was assigned
    // by CollectLocalStatics.  Recover that typed global identity by binding
    // name so a local-static guard is not silently dropped from the replay.
    if(plan && !plan->global) {
      const string name = LastComponent(declarator_name(declarator));
      for(Scope* current = scope; current && !plan->global;
          current = current->parent) {
        Binding* binding = current->local(name);
        if(!binding || binding->qualified_name.find("__local_static__") != 0)
          continue;
        map<string, GlobalRecord*>::const_iterator found =
          global_by_key_.find(global_key(binding->qualified_name));
        if(found != global_by_key_.end()) plan->global = found->second;
      }
    }
    }
  }
void PA14Lowerer::PlanCondition(const CPPGMAstNodePtr& condition, Scope* scope)
{
    if(!condition || condition->kind != "condition-declaration" || condition->children.size() < 3) return;
    Analyzer::SpecFacts facts;
    TypePtr type = PlannedType(condition->children[0], condition->children[1],
      scope, condition->children[2]);
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
    if(node->kind == "range-for-statement") {
      PlanRangeFor(node, scope);
      return;
    }
    if(node->kind == "compound-statement") {
      state_->environments.push_back(map<string, VariablePlan*>());
      for(size_t i = 0; i < node->children.size(); ++i) PlanStatement(node->children[i], scope);
      state_->environments.pop_back();
      return;
    }
    if(node->kind == "try-block") {
      if(!node->children.empty()) PlanStatement(node->children[0], scope);
      for(size_t i = 1; i < node->children.size(); ++i) {
        const CPPGMAstNodePtr handler = node->children[i];
        if(handler && handler->kind == "handler" && handler->children.size() > 1)
          PlanStatement(handler->children[1], scope);
      }
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
		if(body) {
			map<const CPPGMAstNode*, Scope*>::const_iterator compound =
				analyzer_.compound_scopes_.find(body.get());
			if(compound != analyzer_.compound_scopes_.end()) scope = compound->second;
		}
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
             !PA12SameType(type_value(variable.type), return_type, true)) continue;
          state.return_slot_plan = &variable;
          variable.initialization_address = "%" + names[0];
          break;
        }
      }
    }
  }
void PA14Lowerer::ResolveAutoFunctionReturns()
{
    for(size_t function_index = 0; function_index < functions_.size();
        ++function_index) {
      ResolveAutoFunctionReturn(functions_[function_index]);
    }
  }
void PA14Lowerer::ResolveAutoFunctionReturn(FunctionRecord& record)
{
    TypePtr source_function = function_target_type(record.source_type);
    if(!source_function || !ContainsAutoType(source_function->child) ||
       !record.definition || !record.node) return;
    CPPGMAstNodePtr body = ChildOfKind(record.node, "compound-statement");
    if(!body && record.node->children.size() > 2) body = record.node->children[2];
    if(!body) throw logic_error("auto function has no definition body");
    vector<CPPGMAstNodePtr> returns;
    function<void(const CPPGMAstNodePtr&)> collect_returns;
    collect_returns = [&](const CPPGMAstNodePtr& node) {
      if(!node || node->kind == "lambda-expression") return;
      if(node->kind == "return-statement") {
        if(!node->children.empty() && node->children[0]) returns.push_back(node->children[0]);
        return;
      }
      for(size_t child = 0; child < node->children.size(); ++child)
        collect_returns(node->children[child]);
    };
    collect_returns(body);
    if(returns.empty()) {
      if(!record.lambda_function && !IsLambdaOperator(record))
        throw logic_error("cannot deduce auto return type");
      const TypePtr old_source = record.source_type;
      const TypePtr result_type = Fundamental("void");
      ApplyAutoFunctionReturn(record, old_source, source_function, result_type);
      return;
    }
    TypePtr result_type = DeduceAutoFunctionReturn(record, source_function, body, returns);
    if(ContainsAutoType(result_type))
      throw logic_error("could not deduce auto return type");
    const TypePtr old_source = record.source_type;
    TypePtr adjusted_source(new Type(*source_function));
    adjusted_source->child = result_type;
    ApplyAutoFunctionReturn(record, old_source, source_function, result_type);
    (void)adjusted_source;
  }
TypePtr PA14Lowerer::DeduceAutoFunctionReturn(FunctionRecord& record,
    const TypePtr& source_function, const CPPGMAstNodePtr& body,
    const vector<CPPGMAstNodePtr>& returns)
{
    FunctionState* saved_state = state_;
    FunctionState scratch(this, &record);
    state_ = &scratch;
    scratch.environments.push_back(map<string, VariablePlan*>());
    if(record.member && !record.static_member && record.member_owner) {
      TypePtr this_type = CloneWithCv(type_value(record.member_owner),
        source_function->function_const, source_function->function_volatile);
      VariablePlan* this_plan = AddVariablePlan("this", PointerTo(this_type),
        CPPGMAstNodePtr(), CPPGMAstNodePtr());
      if(this_plan) scratch.environments.back()["this"] = this_plan;
    }
    if(record.lambda_function || IsLambdaOperator(record)) {
      // The normal auto-return pass runs before a local lambda is emitted, so
      // its body has not gone through PlanFunction yet.  Plan it here to make
      // local declarations available to return-expression inference; the
      // final emission pass replans the body in its ordinary state.
      PlanFunction(scratch);
      map<string, VariablePlan*> planned;
      for(size_t variable = 0; variable < scratch.variables.size(); ++variable)
        planned[scratch.variables[variable].source_name] = &scratch.variables[variable];
      scratch.environments.push_back(planned);
    }
    Scope* expression_scope = record.scope;
    map<const CPPGMAstNode*, Scope*>::const_iterator function_scope =
      analyzer_.function_scopes_.find(record.node.get());
    if(function_scope != analyzer_.function_scopes_.end()) expression_scope = function_scope->second;
    map<const CPPGMAstNode*, Scope*>::const_iterator body_scope =
      analyzer_.compound_scopes_.find(body.get());
    if(body_scope != analyzer_.compound_scopes_.end()) expression_scope = body_scope->second;
    TypePtr deduced;
    string category;
    for(size_t result_index = 0; result_index < returns.size(); ++result_index) {
      ExprInfo info = Infer(returns[result_index], expression_scope);
      TypePtr value = expression_value_type(info);
      if(!value) throw logic_error("auto return expression has no type");
      if(!deduced) {
        deduced = value;
        category = info.category;
        if(info.type && info.type->kind == TYPE_LVALUE_REFERENCE) category = "lvalue";
        else if(info.type && info.type->kind == TYPE_RVALUE_REFERENCE) category = "xvalue";
      } else if(!PA12SameType(deduced, value, false) || category != info.category) {
        if(!PA12SameType(deduced, value, false) ||
           (source_function->child->kind == TYPE_LVALUE_REFERENCE ||
            source_function->child->kind == TYPE_RVALUE_REFERENCE) &&
           category != info.category)
          throw logic_error("inconsistent auto return deduction");
      }
    }
    const auto without_top_cv = [](const TypePtr& original) -> TypePtr {
      if(!original) return original;
      TypePtr result(new Type(*original));
      result->is_const = false;
      result->is_volatile = false;
      return result;
    };
    function<TypePtr(const TypePtr&, const TypePtr&)> substitute;
    substitute = [&](const TypePtr& pattern, const TypePtr& value) -> TypePtr {
      if(!pattern) return value;
      if(pattern->kind == TYPE_FUNDAMENTAL && pattern->name == "auto")
        return CloneWithCv(value, pattern->is_const, pattern->is_volatile);
      if(pattern->kind == TYPE_POINTER) {
        if(!value || value->kind != TYPE_POINTER)
          throw logic_error("auto return pointer has incompatible type");
        return PointerTo(substitute(pattern->child, value->child));
      }
      if(pattern->kind == TYPE_ARRAY) {
        if(!value || value->kind != TYPE_ARRAY)
          throw logic_error("auto return array has incompatible type");
        return ArrayOf(pattern->bound, substitute(pattern->child, value->child));
      }
      return pattern;
    };
    TypePtr result_type;
    if(source_function->child->kind == TYPE_LVALUE_REFERENCE ||
       source_function->child->kind == TYPE_RVALUE_REFERENCE) {
      const TypeKind kind = source_function->child->kind == TYPE_LVALUE_REFERENCE ||
        category == "lvalue" ? TYPE_LVALUE_REFERENCE : TYPE_RVALUE_REFERENCE;
      result_type = ReferenceTo(kind, substitute(source_function->child->child, deduced));
    } else result_type = substitute(source_function->child, without_top_cv(deduced));
    state_ = saved_state;
    return result_type;
  }
void PA14Lowerer::ApplyAutoFunctionReturn(FunctionRecord& record,
    const TypePtr& old_source, const TypePtr& source_function,
    const TypePtr& result_type)
{
    TypePtr adjusted_source(new Type(*source_function));
    adjusted_source->child = result_type;
    record.source_type = adjusted_source;
    if(record.type) {
      TypePtr adjusted_type(new Type(*record.type));
      adjusted_type->child = result_type;
      record.type = adjusted_type;
    }
    function<void(Scope*)> update_bindings;
    update_bindings = [&](Scope* scope) {
      if(!scope) return;
      for(size_t binding_index = 0; binding_index < scope->bindings.size(); ++binding_index) {
        Binding& binding = scope->bindings[binding_index];
        if(binding.kind != BIND_FUNCTION) continue;
        const bool same_declaration = binding.declaration && record.node &&
          binding.declaration.get() == record.node.get();
        const bool same_name = !record.qualified_name.empty() &&
          binding.qualified_name == record.qualified_name;
        if(!same_declaration && !same_name) continue;
        TypePtr existing = function_target_type(binding.type);
        if(existing && (ContainsAutoType(existing) ||
           PA12SameType(existing, function_target_type(old_source), false)))
          binding.type = adjusted_source;
      }
      for(size_t child = 0; child < scope->children.size(); ++child)
        update_bindings(scope->children[child].get());
    };
    update_bindings(analyzer_.global_.get());
    function_by_key_[function_key(record.qualified_name, source_function)] = &record;
    function_by_key_[function_key(record.qualified_name, adjusted_source)] = &record;
    infer_cache_.clear();
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
