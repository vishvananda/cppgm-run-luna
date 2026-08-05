#include "pa14_lowering.h"

#include <algorithm>
#include <functional>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace cppgm_pa14_lowering {

bool PA14Lowerer::EmitConstructorAt(const TypePtr& raw_object_type, const string& address,
                                    const vector<CPPGMAstNodePtr>& input_arguments,
                                    Scope* scope, bool allow_explicit, bool base_entry,
                                    bool allow_aggregate, bool force_move,
                                    bool value_initialization,
                                    bool full_expression_cleanup)
{
	const size_t temporary_mark = state_ ? state_->temporary_objects.size() : 0;
	TypePtr object_type = type_value(raw_object_type);
	if(!object_type || object_type->kind != TYPE_CLASS) return false;
    vector<CPPGMAstNodePtr> raw_arguments = input_arguments;
    // Default construction is normally collected while walking a class body,
    // but a replayed specialization can first become observable through a
    // local whose only source use is unevaluated.  Materialize the implicit
    // lifecycle entry from the typed effect fact before constructor lookup.
    if(raw_arguments.empty() && HasDefaultConstructionEffects(object_type))
      CollectImplicitConstructor(object_type, object_type->owned_scope, true);
    // An aggregate's implicit default constructor has no lowered action when
    // the aggregate is a concrete (non-template) class with no construction
    // effects.  Keep materialized template specializations on the normal path:
    // their generated lifecycle entry is part of the replayed typed state.
    bool has_nonstatic_member = false;
    for(size_t member = 0; member < object_type->class_members.size(); ++member)
      if(!object_type->class_members[member].is_static &&
         !object_type->class_members[member].name.empty()) {
        has_nonstatic_member = true;
        break;
      }
    function<bool(const TypePtr&)> has_unavailable_default =
      [&](const TypePtr& raw_member) {
        TypePtr member_type = type_value(raw_member);
        if(!member_type) return false;
        if(member_type->kind == TYPE_ARRAY) return has_unavailable_default(member_type->child);
        if(member_type->kind != TYPE_CLASS) return false;
        const vector<TypePtr> direct_bases = DirectBaseTypes(member_type);
        for(size_t base = 0; base < direct_bases.size(); ++base)
          if(has_unavailable_default(direct_bases[base])) return true;
        for(size_t field = 0; field < member_type->class_members.size(); ++field)
          if(!member_type->class_members[field].is_static &&
             has_unavailable_default(member_type->class_members[field].type)) return true;
        bool has_user_constructor = false;
        bool has_default_constructor = false;
        const vector<Binding*> constructors = MemberBindings(member_type,
          LastComponent(member_type->name));
        for(size_t candidate = 0; candidate < constructors.size(); ++candidate) {
          FunctionRecord* record = RecordForBinding(constructors[candidate]);
          if(!record || !record->constructor || record->implicit_constructor ||
             record->aggregate_constructor || record->copy_constructor ||
             record->move_constructor) continue;
          has_user_constructor = true;
          TypePtr signature = function_target_type(constructors[candidate]->type);
          bool defaultable = signature && signature->parameters.size() == 0;
          if(signature) for(size_t parameter = 0; parameter < signature->parameters.size(); ++parameter)
            if(!HasDefaultArgument(constructors[candidate], parameter)) defaultable = false;
          if(defaultable && !record->deleted) has_default_constructor = true;
        }
        return has_user_constructor && !has_default_constructor;
      };
    bool has_array_member = false;
    for(size_t member = 0; member < object_type->class_members.size(); ++member) {
      if(object_type->class_members[member].is_static) continue;
      TypePtr member_type = type_value(object_type->class_members[member].type);
      if(member_type && member_type->kind == TYPE_ARRAY) {
        has_array_member = true;
        break;
      }
    }
    const bool template_context = state_ && state_->record &&
      state_->record->template_instantiation;
    if(raw_arguments.empty() && !object_type->template_specialization &&
       !(template_context && has_array_member) &&
       has_nonstatic_member && !HasDefaultConstructionEffects(object_type) &&
       !has_unavailable_default(object_type) && !HasExplicitConstructor(object_type)) return true;
    const string constructor_name = object_type->template_specialization &&
      !object_type->template_primary.empty() ?
      LastComponent(object_type->template_primary) : LastComponent(object_type->name);
    if(raw_arguments.size() == 1 && raw_arguments[0]) {
      TypePtr argument_type = expression_value_type(Infer(raw_arguments[0], scope));
      if(argument_type && argument_type->kind == TYPE_CLASS &&
         (PA12SameType(argument_type, object_type, true) ||
          IsDerivedFrom(argument_type, object_type)))
        (void)EnsureImplicitCopyConstructor(object_type, false);
    }
    bool aggregate_class_tail = false;
    if(allow_aggregate) {
      aggregate_class_tail = AppendAggregateConstructorDefaults(object_type,
        input_arguments, &raw_arguments);
      if(!raw_arguments.empty()) {
        if(!aggregate_class_tail)
          (void)EnsureAggregateConstructorForArguments(object_type, raw_arguments.size());
        else (void)EnsureAggregateConstructor(object_type);
      }
    } else if(!raw_arguments.empty()) (void)EnsureAggregateConstructor(object_type);
    vector<Binding*> candidates = MemberBindings(object_type, LastComponent(object_type->name));
    if(candidates.empty() && constructor_name != LastComponent(object_type->name))
      candidates = MemberBindings(object_type, constructor_name);
    vector<ExprInfo> argument_infos;
    for(size_t i = 0; i < raw_arguments.size(); ++i) {
      ExprInfo info = Infer(raw_arguments[i], scope);
      if(force_move && i == 0 && info.category == "lvalue") info.category = "xvalue";
      argument_infos.push_back(info);
    }
    Binding* best_binding = 0;
    TypePtr best_function;
    int best_worst = 1000000;
    int best_total = 1000000;
    const auto better_lvalue_reference_binding = [](const TypePtr& candidate,
                                                    const TypePtr& current,
                                                    const vector<ExprInfo>& infos) {
      if(!candidate || !current) return false;
      const size_t count = min(infos.size(), min(candidate->parameters.size(),
        current->parameters.size()));
      for(size_t argument = 0; argument < count; ++argument) {
        if(infos[argument].category != "lvalue" ||
           candidate->parameters[argument]->kind != TYPE_LVALUE_REFERENCE ||
           current->parameters[argument]->kind != TYPE_LVALUE_REFERENCE) continue;
        const TypePtr candidate_referred = candidate->parameters[argument]->child;
        const TypePtr current_referred = current->parameters[argument]->child;
        if(candidate_referred && current_referred &&
           candidate_referred->is_const != current_referred->is_const)
          return !candidate_referred->is_const && current_referred->is_const;
      }
      return false;
    };
    for(size_t i = 0; i < candidates.size(); ++i) {
      Binding* binding = candidates[i];
      if(!binding->is_member || binding->is_static || binding->kind != BIND_FUNCTION)
        continue;
      FunctionRecord* record = RecordForBinding(binding);
      if(!record || !record->constructor) continue;
      if(record->aggregate_constructor && !allow_aggregate) continue;
      if(record->deleted) continue;
      if(!allow_explicit && record->explicit_constructor) continue;
      if(record->implicit_constructor && !record->copy_constructor &&
         !record->move_constructor && !raw_arguments.empty()) continue;
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
        int rank = 2;
        bool braced_class_handled = false;
		if(a < function->parameters.size() && raw_arguments[a] &&
		   raw_arguments[a]->kind == "braced-init-list") {
		  const TypePtr parameter_type = type_value(function->parameters[a]);
		  if(IsInitializerListType(parameter_type) &&
		     InitializerListArgumentViable(raw_arguments[a], function->parameters[a], scope)) {
		    rank = 0;
		    braced_class_handled = true;
		  }
		  if(!braced_class_handled && parameter_type && parameter_type->kind == TYPE_CLASS) {
		    FunctionRecord* aggregate = EnsureAggregateConstructor(parameter_type);
		    if(aggregate && !aggregate->deleted) {
		      rank = function->parameters[a]->kind == TYPE_RVALUE_REFERENCE ? 2 : 3;
		      braced_class_handled = true;
		    }
			    if(!braced_class_handled && raw_arguments[a]->children.empty()) {
			      const vector<Binding*> defaults = MemberBindings(parameter_type,
			        LastComponent(parameter_type->name));
			      for(size_t candidate = 0; candidate < defaults.size(); ++candidate) {
			        FunctionRecord* default_record = RecordForBinding(defaults[candidate]);
			        TypePtr default_function = function_target_type(defaults[candidate]->type);
			        if(!default_record || !default_record->constructor ||
			           default_record->deleted || !default_function) continue;
			        bool defaultable = default_function->parameters.empty();
			        for(size_t parameter = 0; defaultable &&
			            parameter < default_function->parameters.size(); ++parameter)
			          if(!HasDefaultArgument(defaults[candidate], parameter)) defaultable = false;
			        if(defaultable) {
			          rank = function->parameters[a]->kind == TYPE_RVALUE_REFERENCE ? 2 : 3;
			          braced_class_handled = true;
			          break;
			        }
			      }
			    }
			    if(!braced_class_handled) {
			      // A braced argument can initialize a class through a user-declared
			      // constructor, not only through the synthesized aggregate path.
			      // The actual nested construction is emitted by EmitReferenceArgument
			      // after this outer overload has been selected; keep that constructor
			      // viable during the outer probe as well.
			      const vector<Binding*> nested = MemberBindings(parameter_type,
			        LastComponent(parameter_type->name));
			      for(size_t nested_candidate = 0; nested_candidate < nested.size();
			        ++nested_candidate) {
			        FunctionRecord* nested_record = RecordForBinding(nested[nested_candidate]);
			        if(!nested_record || !nested_record->constructor || nested_record->deleted ||
			           (nested_record->implicit_constructor && !nested_record->copy_constructor &&
			            !nested_record->move_constructor)) continue;
			        rank = function->parameters[a]->kind == TYPE_RVALUE_REFERENCE ? 2 : 3;
			        braced_class_handled = true;
			        break;
			      }
			    }
		  }
		}
		if(!braced_class_handled)
			rank = a < function->parameters.size() ?
				ConversionRank(argument_infos[a], function->parameters[a]) : 2;
		if(rank < 0) { viable = false; break; }
        worst = max(worst, rank);
        total += rank;
      }
      if(!viable) continue;
      const bool better_reference = worst == best_worst && total == best_total &&
        better_lvalue_reference_binding(function, best_function, argument_infos);
      if(!best_binding || worst < best_worst ||
         (worst == best_worst && (total < best_total || better_reference))) {
        best_binding = binding;
        best_function = function;
        best_worst = worst;
        best_total = total;
      } else if(worst == best_worst && total == best_total &&
                !PA12SameType(best_function, function, false) &&
                !better_lvalue_reference_binding(best_function, function, argument_infos)) {
		throw logic_error("ambiguous constructor overload");
		}
    }
    if(!best_binding) {
	  bool has_nonstatic_data = false;
      for(size_t i = 0; i < object_type->class_members.size(); ++i)
        if(!object_type->class_members[i].is_static &&
           !object_type->class_members[i].name.empty()) {
          has_nonstatic_data = true;
          break;
        }
      // An empty class with no user-declared constructor is default
      // constructible without an emitted action.  This matters for a
      // temporary functor such as F()(arg): its storage only needs an
      // address for the hidden operator() object.
      if(raw_arguments.empty() && !has_nonstatic_data && candidates.empty())
        return true;
      bool aggregate_candidate = false;
      for(size_t i = 0; i < candidates.size(); ++i)
        if(candidates[i]->is_member && !candidates[i]->is_static &&
           candidates[i]->kind == BIND_FUNCTION && RecordForBinding(candidates[i]) &&
           RecordForBinding(candidates[i])->constructor &&
           !(RecordForBinding(candidates[i])->implicit_constructor &&
             !RecordForBinding(candidates[i])->copy_constructor &&
             !RecordForBinding(candidates[i])->move_constructor && !raw_arguments.empty()) &&
           !RecordForBinding(candidates[i])->deleted) {
          if(RecordForBinding(candidates[i])->defaulted) continue;
          if(!allow_explicit && RecordForBinding(candidates[i])->explicit_constructor) continue;
          if(RecordForBinding(candidates[i])->aggregate_constructor) aggregate_candidate = true;
          else throw PA14NoViableConstructor("no viable constructor");
        }
      if(aggregate_candidate) return false;
      return false;
    }
    FunctionRecord* record = RecordForBinding(best_binding);
    const bool replayed_template_context = state_ && state_->record &&
      state_->record->template_instantiation;
    const bool rtti_template_copy = replayed_template_context &&
      !demanded_rtti_types_.empty() &&
      demanded_rtti_types_.find(RttiMangledName(object_type)) != demanded_rtti_types_.end();
    if(record && record->copy_constructor && raw_arguments.size() == 1 &&
       raw_arguments[0] && IsTrivialValueStorage(object_type) &&
       !rtti_template_copy) {
      if(IsEmptyBaseStorage(object_type)) {
        EmitTemporaryDestructors(temporary_mark, scope);
        return true;
      }
      const ExprInfo source_info = Infer(raw_arguments[0], scope);
      const TypePtr source_type = expression_value_type(source_info);
      string source = EmitAddress(raw_arguments[0], scope);
      if(source_type && IsDerivedFrom(source_type, object_type) &&
         !PA12SameType(source_type, object_type, true))
        source = AdjustBaseAddress(source, source_type, object_type);
      AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(object_type))) +
        "x" + integer_text(static_cast<long long>(type_alignment(object_type))) +
        " " + source + ", " + address);
      EmitTemporaryDestructors(temporary_mark, scope);
      return true;
    }
    FunctionRecord* complete_record = record;
    bool demand_complete_record = false;
    if(record && base_entry) {
      // A template specialization with a direct base can be entered solely
      // through that base ABI entry.  Keep the complete-object entry demand
      // for ordinary/defaultable bases, but do not create it while replaying
      // an implicit direct-base chain.
      if((!base_entry || object_type->template_specialization) &&
         raw_arguments.empty() && IsEmptyBaseStorage(object_type) &&
         !object_type->polymorphic && !record->member_template &&
         HasUserProvidedConstructor(object_type) &&
         (!record->template_instantiation || DirectBaseTypes(object_type).empty()))
        demand_complete_record = true;
      const TypePtr first_parameter = record->source_type && !record->source_type->parameters.empty() ? record->source_type->parameters[0] : TypePtr();
      const bool inherited_constructor_wrapper = state_ && state_->record &&
        state_->record->inherited_constructor_wrapper;
      const bool replayed_member_template = state_ && state_->record &&
        state_->record->member_template_frame;
      const bool out_of_class_template_constructor = record->out_of_class_definition;
      if(record->template_instantiation &&
         (!raw_arguments.empty() || out_of_class_template_constructor) &&
         (record->value_special_member || !type_is_reference(first_parameter) ||
          raw_arguments.size() > 1) && !inherited_constructor_wrapper &&
         // An explicit constructor invoked only as a base does not require a
         // separate complete-object replay entry.
         !record->explicit_constructor &&
          // A member-template replay already has a typed base-entry call;
          // retaining the primary C1 entry here creates an unused duplicate.
          !replayed_member_template)
        demand_complete_record = true;
      FunctionRecord* entry = BaseEntryFor(record);
      if(!entry) {
        EnsureConstructorBaseEntry(record);
        entry = BaseEntryFor(record);
      }
      if(entry) record = entry;
    }
	bool has_instance_member = false;
	for(size_t member = 0; member < object_type->class_members.size(); ++member)
		if(!object_type->class_members[member].is_static &&
			!object_type->class_members[member].name.empty()) {
			has_instance_member = true;
			break;
		}
	if(value_initialization && raw_arguments.empty() && has_instance_member && record &&
		(record->aggregate_constructor || record->implicit_constructor || record->defaulted)) {
		TypePtr zero_type;
		switch(type_size(object_type)) {
		case 1: zero_type = Fundamental("char"); break;
		case 2: zero_type = Fundamental("short int"); break;
		case 4: zero_type = Fundamental("int"); break;
		case 8: zero_type = Fundamental("long int"); break;
		default: break;
		}
		if(zero_type) emit_store(zero_type, "0", address);
		else AddInstruction("store " + low_type(object_type) + " 0, " + address);
	}
    const bool previous_full_expression_defer = state_ &&
      state_->defer_temporary_cleanup;
    if(full_expression_cleanup && state_)
      state_->defer_temporary_cleanup = true;
    vector<CPPGMAstNodePtr> arguments = raw_arguments;
    if(record) {
      while(arguments.size() < best_function->parameters.size()) {
        const size_t index = arguments.size();
        if(index >= record->default_arguments.size() || !record->default_arguments[index]) break;
        arguments.push_back(InitializerExpression(record->default_arguments[index]));
      }
    }
    function<bool(const CPPGMAstNodePtr&)> contains_managed_temporary;
    const auto constructor_is_nothrow = [this](const TypePtr& raw_type) {
      const TypePtr type = type_value(raw_type);
      if(!type || type->kind != TYPE_CLASS) return false;
      const vector<Binding*> constructors = MemberBindings(type,
        LastComponent(type->name));
      bool found = false;
      for(size_t i = 0; i < constructors.size(); ++i) {
        FunctionRecord* candidate = RecordForBinding(constructors[i]);
        if(!candidate || !candidate->constructor || candidate->deleted) continue;
        found = true;
        if(!candidate->unwind_no) return false;
      }
      return found;
    };
    contains_managed_temporary = [&](const CPPGMAstNodePtr& expression) -> bool {
      if(!expression) return false;
      if(expression->kind == "call-expression" && !expression->children.empty()) {
        TypePtr constructed = ConstructorObjectType(expression->children[0], scope);
        if(constructed && HasDestructor(constructed) &&
           DestructorHasEffects(constructed) &&
           !constructor_is_nothrow(constructed)) return true;
        try {
          const ExprInfo expression_info = Infer(expression, scope);
          TypePtr value = expression_value_type(expression_info);
          if(value && value->kind == TYPE_CLASS &&
             !type_is_reference(expression_info.type) && HasDestructor(value) &&
             DestructorHasEffects(value) &&
             (!constructed || !constructor_is_nothrow(constructed))) return true;
        } catch(const logic_error&) {
        }
      }
      for(size_t child = 0; child < expression->children.size(); ++child)
        if(contains_managed_temporary(expression->children[child])) return true;
      return false;
    };
    const auto is_reference_temporary = [this, scope](
      const CPPGMAstNodePtr& expression, const TypePtr& parameter) {
      if(!expression || !parameter || !type_is_reference(parameter) ||
         !type_value(parameter->child) ||
         type_value(parameter->child)->kind != TYPE_CLASS) return false;
      CPPGMAstNodePtr value = expression;
      while(value && value->kind == "parenthesized-expression" &&
            value->children.size() == 1 && value->children[0])
        value = value->children[0];
      if(value && value->kind == "call-expression" &&
         !value->children.empty()) {
        TypePtr constructed = ConstructorObjectType(value->children[0], scope);
        if(constructed && HasDestructor(constructed)) return true;
        try {
          const ExprInfo info = Infer(value, scope);
          const TypePtr result = expression_value_type(info);
          if(result && result->kind == TYPE_CLASS &&
             !type_is_reference(info.type) && HasDestructor(result)) return true;
        } catch(const logic_error&) {
        }
      }
      if(value && (value->kind == "braced-construction" ||
                   value->kind == "braced-init-list"))
        return HasDestructor(type_value(parameter->child));
      return false;
    };
    const vector<FunctionState::TemporaryObject> cleanup_before_arguments =
      CaptureLiveCleanupObjects();
    bool managed_argument = false;
    for(size_t argument = 0; argument < arguments.size() && !managed_argument;
        ++argument) {
      const TypePtr parameter = argument < best_function->parameters.size() ?
        best_function->parameters[argument] : TypePtr();
      managed_argument = contains_managed_temporary(arguments[argument]) ||
        is_reference_temporary(arguments[argument], parameter);
    }
    bool reference_temporary_argument = false;
    for(size_t argument = 0; argument < arguments.size() &&
        !reference_temporary_argument; ++argument) {
      if(argument >= best_function->parameters.size()) break;
      TypePtr parameter = best_function->parameters[argument];
      CPPGMAstNodePtr expression = arguments[argument];
      while(expression && expression->kind == "parenthesized-expression" &&
            expression->children.size() == 1)
        expression = expression->children[0];
      if(state_ && state_->condition_cleanup_depth != 0 &&
         parameter && type_is_reference(parameter) &&
         type_value(parameter->child) &&
         type_value(parameter->child)->kind == TYPE_CLASS && expression &&
         expression->kind == "call-expression" &&
         expression->value == "braced-construction")
        reference_temporary_argument = true;
    }
    bool conditional_pointer_argument = false;
    bool conditional_address_argument = false;
    for(size_t argument = 0; argument < arguments.size() &&
        !conditional_address_argument; ++argument) {
      if(argument >= best_function->parameters.size()) break;
      TypePtr parameter = best_function->parameters[argument];
      CPPGMAstNodePtr expression = arguments[argument];
      while(expression && expression->kind == "parenthesized-expression" &&
            expression->children.size() == 1)
        expression = expression->children[0];
      if(parameter && parameter->kind == TYPE_POINTER && expression &&
         expression->kind == "conditional-expression")
        conditional_pointer_argument = true;
      if(parameter && parameter->kind == TYPE_POINTER && expression &&
         expression->kind == "conditional-expression" &&
         expression->children.size() > 2) {
        TypePtr true_type = expression_value_type(
          Infer(expression->children[1], scope));
        TypePtr false_type = expression_value_type(
          Infer(expression->children[2], scope));
        if(true_type && false_type && true_type->kind == TYPE_ARRAY &&
           false_type->kind == TYPE_ARRAY &&
           true_type->bound == false_type->bound)
          conditional_address_argument = true;
      }
    }
    // A conditional pointer argument is evaluated in its own protected
    // region, but a managed default argument that follows it belongs to the
    // enclosing constructor call's region.  Keep that later cleanup pending
    // until the default temporary has been formed; otherwise the temporary
    // helper opens a second region around the default constructor itself.
    if(state_ && conditional_pointer_argument &&
       !state_->constructor_unwind_active &&
       !state_->pending_constructor_unwind_start) {
      for(size_t argument = raw_arguments.size(); argument < arguments.size();
          ++argument) {
        const TypePtr parameter = argument < best_function->parameters.size() ?
          best_function->parameters[argument] : TypePtr();
        if(!contains_managed_temporary(arguments[argument]) &&
           !is_reference_temporary(arguments[argument], parameter)) continue;
        state_->pending_constructor_unwind_start = true;
        state_->pending_constructor_unwind_suppress_temporary = true;
        break;
      }
    }
    const bool constructor_unwind_already_active = state_ &&
      state_->constructor_unwind_active;
    const bool selected_constructor_no_throw = record &&
      (record->unwind_no || (best_binding &&
        (best_binding->noexcept_specified || HasNoexcept(best_binding->declaration))));
    const bool temporary_constructor_context = state_ &&
      state_->temporary_construction && HasDestructor(object_type) &&
      !selected_constructor_no_throw;
    const bool noexcept_constructor_argument_context = record &&
      record->constructor && (record->unwind_no ||
        (best_binding && (best_binding->noexcept_specified ||
          HasNoexcept(best_binding->declaration)))) &&
      state_ && state_->condition_cleanup_depth == 0;
    const bool wrap_before_arguments = state_ &&
      !state_->suppress_constructor_unwind &&
      !constructor_unwind_already_active &&
      (temporary_constructor_context ||
       (managed_argument && !noexcept_constructor_argument_context) ||
       (conditional_pointer_argument && !cleanup_before_arguments.empty()) ||
       reference_temporary_argument);
    size_t unwind_argument_index = arguments.size();
    if(wrap_before_arguments) {
      if(temporary_constructor_context || conditional_pointer_argument ||
          reference_temporary_argument)
        unwind_argument_index = 0;
      else {
        for(size_t argument = 0; argument < arguments.size(); ++argument)
          if(contains_managed_temporary(arguments[argument]) ||
             is_reference_temporary(arguments[argument],
               argument < best_function->parameters.size() ?
                 best_function->parameters[argument] : TypePtr())) {
            unwind_argument_index = argument;
            break;
          }
      }
    }
    const bool defer_unwind_start = wrap_before_arguments &&
      !state_->temporary_construction && !conditional_pointer_argument &&
      unwind_argument_index < arguments.size() &&
      unwind_argument_index >= raw_arguments.size();
    string pending_dispatch = defer_unwind_start ?
      new_label("call_unwind_dispatch") : string();
    string pending_end = defer_unwind_start ?
      new_label("call_unwind_end") : string();
    bool argument_context_started_here = false;
    if(temporary_constructor_context && arguments.empty() &&
       (!state_ || !state_->suppress_constructor_unwind)) {
      BeginConstructorUnwind(cleanup_before_arguments, false);
      argument_context_started_here = true;
    }
    vector<string> operands;
    operands.push_back(address);
    for(size_t i = 0; i < arguments.size(); ++i) {
      TypePtr target = i < best_function->parameters.size() ? best_function->parameters[i] : TypePtr();
      const size_t target_low_index = (record && record->indirect_result ? 1 : 0) +
        (record && record->member && !record->static_member ? 1 : 0) + i;
      const bool delayed_reference_context = wrap_before_arguments &&
        !defer_unwind_start && i == unwind_argument_index && target &&
        type_is_reference(target) && type_value(target->child) &&
        type_value(target->child)->kind == TYPE_CLASS;
      const bool delayed_argument_context = wrap_before_arguments &&
        !defer_unwind_start && i == unwind_argument_index && record && target &&
        type_value(target) && type_value(target)->kind == TYPE_CLASS &&
        LowParameterIsByAddress(*record, target_low_index);
      if(wrap_before_arguments && i == unwind_argument_index &&
         !delayed_argument_context && !delayed_reference_context) {
        if(defer_unwind_start) {
          state_->pending_constructor_unwind_start = true;
          state_->pending_constructor_unwind_suppress_temporary =
            i >= raw_arguments.size();
          state_->pending_constructor_unwind_dispatch = pending_dispatch;
          state_->pending_constructor_unwind_end = pending_end;
        } else if(conditional_pointer_argument && i == 0) {
          state_->constructor_unwind_cleanup = cleanup_before_arguments;
          state_->constructor_unwind_call = false;
          state_->constructor_unwind_dispatch =
            new_label("call_unwind_dispatch");
          state_->constructor_unwind_end.clear();
          AddInstruction("eh_try ^" + state_->constructor_unwind_dispatch);
          state_->constructor_unwind_active = true;
          argument_context_started_here = true;
        } else {
          BeginConstructorUnwind(cleanup_before_arguments, false);
          argument_context_started_here = true;
        }
      }
      if(target && arguments[i] && arguments[i]->kind == "braced-init-list" &&
         IsInitializerListType(target))
        operands.push_back(EmitInitializerListArgument(arguments[i], target, scope, "argobj"));
      else if(target && type_is_reference(target)) {
        operands.push_back(EmitReferenceArgument(arguments[i], scope, target));
      } else if(record && target && type_value(target) &&
              type_value(target)->kind == TYPE_CLASS &&
              LowParameterIsByAddress(*record,
                (record->indirect_result ? 1 : 0) +
                (record->member && !record->static_member ? 1 : 0) + i)) {
        const string slot = new_special_slot("arg", low_type(type_value(target)));
        const string argument_address = new_temp();
        AddInstruction(argument_address + " = addr $" + slot);
        if(delayed_argument_context && state_ &&
           !state_->constructor_unwind_active)
          BeginConstructorUnwind(cleanup_before_arguments, false);
        if(!EmitObjectTransferAt(type_value(target), argument_address, arguments[i], scope, true))
          throw logic_error("no viable value argument transfer");
        operands.push_back(argument_address);
      }
      else {
        CPPGMAstNodePtr conditional = arguments[i];
        while(conditional && conditional->kind == "parenthesized-expression" &&
              conditional->children.size() == 1)
          conditional = conditional->children[0];
        Value value;
        if(target && target->kind == TYPE_POINTER && conditional &&
           conditional_address_argument &&
           conditional->kind == "conditional-expression") {
          value.type = target;
          value.operand = EmitConditionalAddress(conditional, scope);
        } else value = target && type_value(target) &&
          type_value(target)->kind == TYPE_CLASS ?
          EmitObjectValueArgument(arguments[i], scope, target) :
          EmitValue(arguments[i], scope, target);
        if(target && value.known_constant && is_integral_type(value.type) &&
           is_integral_type(target) &&
           (type_size(target) < type_size(value.type) ||
            (!is_unsigned_type(target) && type_size(target) > type_size(value.type)))) {
          value.type = target;
          value.operand = integer_text(value.constant);
        } else if(target) value = ConvertValue(value, target, false, true);
        operands.push_back(value.operand);
      }
    }
    if(defer_unwind_start && state_ &&
       state_->pending_constructor_unwind_start &&
       !state_->constructor_unwind_active) {
      BeginConstructorUnwind(cleanup_before_arguments, true,
        pending_dispatch, pending_end);
      state_->pending_constructor_unwind_start = false;
      state_->pending_constructor_unwind_suppress_temporary = false;
      state_->pending_constructor_unwind_dispatch.clear();
      state_->pending_constructor_unwind_end.clear();
      argument_context_started_here = true;
    }
    const bool new_temporary = state_ &&
      state_->temporary_objects.size() > temporary_mark;
    if(state_ && state_->constructor_unwind_active &&
       !state_->defer_constructor_unwind_finish &&
       !state_->constructor_unwind_call && new_temporary)
      FinishConstructorUnwind(scope);
    if(demand_complete_record) MarkFunctionNeeded(complete_record);
    if(record) MarkFunctionNeeded(record);
    if(full_expression_cleanup && state_)
      state_->defer_temporary_cleanup = previous_full_expression_defer;
    const vector<FunctionState::TemporaryObject> unwind_cleanup =
      CaptureLiveCleanupObjects();
    const bool constructor_call_no_throw = (record && record->unwind_no) ||
      (state_ && state_->record && state_->record->destructor);
    bool has_temporary_cleanup = false;
    for(size_t cleanup = 0; cleanup < unwind_cleanup.size(); ++cleanup)
      if(!unwind_cleanup[cleanup].variable) {
        has_temporary_cleanup = true;
        break;
      }
    if(state_ && !state_->constructor_unwind_active &&
       !state_->suppress_constructor_unwind &&
       !constructor_call_no_throw &&
       (has_temporary_cleanup ||
        (full_expression_cleanup && !unwind_cleanup.empty())) &&
       !unwind_cleanup.empty())
      BeginConstructorUnwind(unwind_cleanup, true);
    ostringstream call;
    call << "call void @" << record->symbol << "(";
    for(size_t i = 0; i < operands.size(); ++i) {
      if(i != 0) call << ", ";
      call << operands[i];
    }
    call << ")";
    AddInstruction(call.str());
    if((!state_ || !state_->defer_temporary_cleanup) &&
       (!state_ || !state_->suppress_constructor_unwind))
      EmitTemporaryDestructors(temporary_mark, scope);
    if(state_ && state_->constructor_unwind_active &&
       !state_->defer_constructor_unwind_finish &&
       state_->constructor_unwind_call)
      FinishConstructorUnwind(scope);
    else if(state_ && state_->constructor_unwind_active &&
            argument_context_started_here)
      FinishConstructorUnwind(scope);
    return true;
  }

} // namespace cppgm_pa14_lowering
