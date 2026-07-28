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
                                    Scope* scope, bool allow_explicit, bool base_entry,
                                    bool allow_aggregate, bool force_move,
                                    bool value_initialization)
{
    const size_t temporary_mark = state_ ? state_->temporary_objects.size() : 0;
    TypePtr object_type = type_value(raw_object_type);
    if(!object_type || object_type->kind != TYPE_CLASS) return false;
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
        if(member_type->direct_base && has_unavailable_default(member_type->direct_base)) return true;
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
    const string constructor_name = LastComponent(object_type->name);
    // The ordinary semantic pass does not need to materialize an implicit copy
    // constructor merely to type-check a class mem-initializer.  Lowering does
    // need that candidate when a base is initialized from an object value,
    // however; make the implicit special member available before overload
    // selection so `Base(base_value)` follows the C++ copy-initialization path.
    if(raw_arguments.size() == 1 && raw_arguments[0]) {
      TypePtr argument_type = expression_value_type(Infer(raw_arguments[0], scope));
      if(argument_type && argument_type->kind == TYPE_CLASS &&
         (PA12SameType(argument_type, object_type, true) ||
          IsDerivedFrom(argument_type, object_type)))
        (void)EnsureImplicitCopyConstructor(object_type, false);
    }
	if(!raw_arguments.empty()) (void)EnsureAggregateConstructor(object_type);
    vector<Binding*> candidates = MemberBindings(object_type, constructor_name);
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
		  if(parameter_type && parameter_type->kind == TYPE_CLASS) {
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
    if(record && record->copy_constructor && raw_arguments.size() == 1 &&
       raw_arguments[0] && IsTrivialValueStorage(object_type)) {
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
    if(record && base_entry) {
      if(raw_arguments.empty() && IsEmptyBaseStorage(object_type) &&
         !object_type->polymorphic && !record->member_template &&
         HasUserProvidedConstructor(object_type))
        record->needed = true;
      const TypePtr first_parameter = record->source_type && !record->source_type->parameters.empty() ? record->source_type->parameters[0] : TypePtr();
      const bool inherited_constructor_wrapper = state_ && state_->record &&
        state_->record->inherited_constructor_wrapper;
      const bool out_of_class_template_constructor = record->out_of_class_definition;
      if(record->template_instantiation &&
         (!raw_arguments.empty() || out_of_class_template_constructor) &&
         (record->value_special_member || !type_is_reference(first_parameter) ||
          raw_arguments.size() > 1) && !inherited_constructor_wrapper)
        record->needed = true;
      FunctionRecord* entry = BaseEntryFor(record);
      if(!entry) {
        EnsureConstructorBaseEntry(record);
        entry = BaseEntryFor(record);
      }
      if(entry) record = entry;
    }
    if(record) {
      record->needed = true;
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
      else if(record && target && type_value(target) &&
              type_value(target)->kind == TYPE_CLASS &&
              LowParameterIsByAddress(*record,
                (record->indirect_result ? 1 : 0) +
                (record->member && !record->static_member ? 1 : 0) + i)) {
        const string slot = new_special_slot("arg", low_type(type_value(target)));
        const string argument_address = new_temp();
        AddInstruction(argument_address + " = addr $" + slot);
        if(!EmitObjectTransferAt(type_value(target), argument_address, arguments[i], scope, true))
          throw logic_error("no viable value argument transfer");
        operands.push_back(argument_address);
      }
      else {
        Value value = target && type_value(target) &&
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
    ostringstream call;
    call << "call void @" << record->symbol << "(";
    for(size_t i = 0; i < operands.size(); ++i) {
      if(i != 0) call << ", ";
      call << operands[i];
    }
    call << ")";
    AddInstruction(call.str());
    EmitTemporaryDestructors(temporary_mark, scope);
    return true;
  }
bool PA14Lowerer::EmitObjectConstructor(VariablePlan* variable,
                                        const TypePtr& raw_object_type,
                                        const vector<CPPGMAstNodePtr>& raw_arguments,
                                        Scope* scope, bool allow_explicit)
{
    if(!variable) return false;
    TypePtr object_type = type_value(raw_object_type);
    if(!object_type || object_type->kind != TYPE_CLASS) return false;
    if(raw_arguments.empty() && HasDefaultConstructionEffects(object_type))
      CollectImplicitConstructor(object_type, object_type->owned_scope, true);
    const vector<Binding*> candidates = MemberBindings(object_type, LastComponent(object_type->name));
    const bool empty_base_only_default = object_type->direct_base &&
      IsEmptyBaseStorage(object_type->direct_base) &&
      !HasDefaultConstructionEffects(object_type);
    if(empty_base_only_default) {
      const vector<Binding*> base_constructors = MemberBindings(
        type_value(object_type->direct_base),
        LastComponent(type_value(object_type->direct_base)->name));
      for(size_t i = 0; i < base_constructors.size(); ++i) {
        FunctionRecord* base_record = RecordForBinding(base_constructors[i]);
        if(!base_record || !base_record->constructor || base_record->copy_constructor ||
           base_record->move_constructor || base_record->implicit_constructor) continue;
        TypePtr base_function = function_target_type(base_constructors[i]->type);
        if(base_function && base_function->parameters.empty()) {
          base_record->needed = true;
        }
      }
    }
    bool has_constructor = false;
    for(size_t i = 0; i < candidates.size(); ++i)
      if(candidates[i]->is_member && !candidates[i]->is_static &&
         candidates[i]->kind == BIND_FUNCTION && RecordForBinding(candidates[i]) &&
         RecordForBinding(candidates[i])->constructor &&
         !(raw_arguments.empty() && RecordForBinding(candidates[i])->defaulted &&
           (!HasDefaultInitializationEffects(object_type) || empty_base_only_default)) &&
         !(RecordForBinding(candidates[i])->implicit_constructor &&
           (!raw_arguments.empty() ||
            !HasDefaultInitializationEffects(object_type) || empty_base_only_default))) {
        has_constructor = true;
		break;
      }
	if(!has_constructor) {
	  return false;
	}
    string address;
    if(!variable->initialization_address.empty()) {
      address = variable->initialization_address;
      variable->initialization_address.clear();
    } else {
      address = EmitAddress(CPPGMAstNodePtr(new CPPGMAstNode(
        "id-expression", variable->source_name)), scope);
    }
    return EmitConstructorAt(raw_object_type, address, raw_arguments, scope,
      allow_explicit);
}
void PA14Lowerer::EmitInitializer(VariablePlan* variable, const CPPGMAstNodePtr& initializer,
                       Scope* scope)
{
    if(!variable || !initializer) return;
    CPPGMAstNodePtr expression = InitializerExpression(initializer);
    if(type_is_reference(variable->type)) {
    if(!expression) throw logic_error("reference initializer is empty");
      const ExprInfo source_info = Infer(expression, scope);
      const TypePtr source_type = expression_value_type(source_info);
      const string destination = variable->global ?
        global_address(variable->global) : StorageForVariable(*variable);
      string address = EmitAddress(expression, scope);
      const TypePtr target_type = type_value(variable->type);
      if(source_type && target_type && source_type->kind == TYPE_CLASS &&
         target_type->kind == TYPE_CLASS && IsDerivedFrom(source_type, target_type))
        address = AdjustBaseAddress(address, source_type, target_type);
      emit_store(PointerTo(Fundamental("char")), address, destination);
      return;
    }
    const bool multi_argument_parenthesized = !initializer->children.empty() &&
      initializer->children[0] && initializer->children[0]->kind == "paren-initializer" &&
      initializer->children[0]->children.size() > 1;
    if(variable->type->kind == TYPE_ARRAY) {
      if(!expression) return;
      string base = EmitAddress(CPPGMAstNodePtr(new CPPGMAstNode("id-expression", variable->source_name)), scope);
      if(expression->kind == "literal" &&
         expression->value.find('"') != string::npos &&
         !is_user_defined_string_literal(expression->value)) {
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
        Value value;
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
          if(EmitConstructorAt(element_type, storage, arguments, scope,
                               true, false, true)) continue;
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
        value = EmitValue(child, scope, variable->type->child);
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
      } else if(element_type && element_type->kind != TYPE_ARRAY &&
                variable->type->bound >= 0) {
        for(size_t i = expression->children.size();
            i < static_cast<size_t>(variable->type->bound); ++i) {
          string storage = base;
          if(i != 0) {
            const string index = new_temp();
            AddInstruction(index + " = index i8 " + base + ", " +
              integer_text(static_cast<long long>(i * type_size(variable->type->child))));
            storage = index;
          }
          emit_store(element_type, "0", storage);
        }
      }
      return;
    }
    TypePtr aggregate_type = type_value(variable->type);
    if(aggregate_type && aggregate_type->kind == TYPE_CLASS) {
      // A value-initialized materialized class specialization uses its
      // implicitly generated default constructor.  Treating an empty list as
      // an aggregate initializer instead leaks the member-zeroing path and
      // leaves the constructor demand stale, which is observable when the
      // specialization is later used by a template function.
      if(aggregate_type->template_specialization && expression &&
         expression->kind == "braced-init-list" && expression->children.empty() &&
         aggregate_type->owned_scope) {
        CollectImplicitConstructor(aggregate_type, aggregate_type->owned_scope, true);
      }
      if(aggregate_type->template_specialization && expression &&
         expression->kind == "braced-init-list" && expression->children.empty() &&
         HasConstructor(aggregate_type) &&
         (HasDefaultConstructionEffects(aggregate_type) ||
          aggregate_type->direct_base || !aggregate_type->direct_bases.empty())) {
        string address;
        if(!variable->initialization_address.empty()) {
          address = variable->initialization_address;
          variable->initialization_address.clear();
        } else address = local_address(variable);
        if(EmitConstructorAt(aggregate_type, address,
                             vector<CPPGMAstNodePtr>(), scope, true, false,
                             false, false, false)) return;
      }
      if(expression && expression->kind != "braced-init-list") {
        const ExprInfo source_info = Infer(expression, scope);
        const TypePtr source_type = expression_value_type(source_info);
        if(source_type && source_type->kind == TYPE_CLASS && !multi_argument_parenthesized) {
          const TypePtr constructed = expression->kind == "call-expression" &&
            !expression->children.empty() ?
            ConstructorObjectType(expression->children[0], scope) : TypePtr();
          const CPPGMAstNodePtr source_arguments = expression->children.size() > 1 ?
            expression->children[1] : CPPGMAstNodePtr();
          if(constructed && PA12SameType(constructed, aggregate_type, true) &&
             (!source_arguments || source_arguments->children.empty()) &&
             !HasDefaultInitializationEffects(aggregate_type)) {
            variable->initialization_address.clear();
            return;
          }
          string destination;
          if(!variable->initialization_address.empty()) {
            destination = variable->initialization_address;
            variable->initialization_address.clear();
          } else {
            destination = EmitAddress(CPPGMAstNodePtr(new CPPGMAstNode(
              "id-expression", variable->source_name)), scope);
          }
          if(!variable->parameter && expression->kind == "conditional-expression" &&
             PA12SameType(source_type, aggregate_type, true) &&
             FindValueMember(aggregate_type, false, false) &&
             HasDestructor(aggregate_type)) {
            const string slot = new_special_slot("arg", low_type(aggregate_type));
            const string argument_address = new_temp();
            AddInstruction(argument_address + " = addr $" + slot);
            if(!EmitObjectTransferAt(aggregate_type, argument_address, expression, scope, true))
              throw logic_error("no viable conditional object initializer");
            FunctionRecord* copy = FindValueMember(aggregate_type, false, false);
            if(!copy) copy = EnsureImplicitCopyConstructor(aggregate_type, false);
            if(!copy || copy->deleted)
              throw logic_error("no viable conditional object copy initializer");
            copy->needed = true;
            FunctionRecord* base_entry = BaseEntryFor(copy);
            if(base_entry) base_entry->needed = true;
            AddInstruction("call void @" + copy->symbol + "(" + destination + ", " +
              argument_address + ")");
            (void)EmitDestructorAt(aggregate_type, argument_address, scope, true);
            return;
          }
          if(EmitObjectTransferAt(aggregate_type, destination, expression, scope,
                                  initializer->initializer_form != AST_INITIALIZER_COPY)) return;
        }
      }
      bool has_anonymous_union_member = false;
      for(size_t i = 0; i < aggregate_type->class_members.size(); ++i) {
        const ClassMemberInfo& member = aggregate_type->class_members[i];
        TypePtr member_type = type_value(member.type);
        if(member.name.empty() && member_type && member_type->kind == TYPE_CLASS &&
           member_type->is_union) {
          has_anonymous_union_member = true;
          break;
        }
      }
      if(has_anonymous_union_member && expression &&
         expression->kind == "braced-init-list") {
        variable->initialization_address.clear();
        CPPGMAstNodePtr object_node(new CPPGMAstNode("id-expression", variable->source_name));
        EmitAggregateAt(string(), aggregate_type, expression, scope, object_node);
        return;
      }
      FunctionRecord* aggregate_candidate = EnsureAggregateConstructor(aggregate_type);
      vector<CPPGMAstNodePtr> constructor_arguments;
		if(expression && expression->kind == "braced-init-list") {
			const bool parenthesized_braced = !initializer->children.empty() &&
			  initializer->children[0] &&
			  initializer->children[0]->kind == "paren-initializer";
			const CPPGMAstNodePtr parenthesized = parenthesized_braced ?
			  initializer->children[0] : CPPGMAstNodePtr();
			// `X x({a}, b, c)` keeps the braced list as the first argument;
			// it does not make the whole parenthesized argument sequence one
			// argument.  The single-child form `X x({a, b})` remains a
			// braced aggregate argument for a non-aggregate constructor.
			if(parenthesized_braced && !aggregate_candidate && parenthesized &&
				parenthesized->children.size() == 1)
				constructor_arguments.push_back(expression);
			else if(parenthesized_braced && parenthesized)
				constructor_arguments = parenthesized->children;
			else constructor_arguments = expression->children;
		}
      else if(!initializer->children.empty() && initializer->children[0] &&
              initializer->children[0]->kind == "paren-initializer")
        constructor_arguments = initializer->children[0]->children;
      else if(expression && expression->kind == "call-expression" &&
              !expression->children.empty() && expression->children[0] &&
              expression->children[0]->kind == "id-expression" &&
              LastComponent(aggregate_type->name) == expression->children[0]->value) {
        CPPGMAstNodePtr arguments = expression->children.size() > 1 ?
          expression->children[1] : CPPGMAstNodePtr();
        if(arguments) constructor_arguments = arguments->children;
      } else if(expression) constructor_arguments.push_back(expression);
      const bool allow_explicit = !initializer ||
        initializer->initializer_form != AST_INITIALIZER_COPY;
      const bool empty_aggregate = expression &&
        expression->kind == "braced-init-list" && expression->children.empty();
		// An empty class (or a class containing only static members) is an
		// aggregate with no synthesized constructor parameters.  `{}` still
		// performs a valid value-initialization, but there is no constructor
		// record for the ordinary call path to select.
		if(empty_aggregate && !aggregate_candidate) {
			bool has_instance_member = false;
			for(size_t member = 0; member < aggregate_type->class_members.size(); ++member)
				if(!aggregate_type->class_members[member].is_static &&
					aggregate_type->class_members[member].type) {
					has_instance_member = true;
					break;
				}
			if(!has_instance_member) {
				variable->initialization_address.clear();
				return;
			}
		}
      if((!aggregate_candidate || !empty_aggregate) &&
         EmitObjectConstructor(variable, aggregate_type, constructor_arguments, scope,
                               allow_explicit)) return;
      if(aggregate_candidate && expression && expression->kind == "braced-init-list") {
        // The address reserved for a possible constructor call has already
        // represented the object's lifetime.  Aggregate stores deliberately
        // recompute their field base, matching ordinary aggregate lowering.
        variable->initialization_address.clear();
        CPPGMAstNodePtr object_node(new CPPGMAstNode("id-expression", variable->source_name));
        EmitAggregateAt(string(), aggregate_type, expression, scope, object_node);
        return;
      }
      if(expression && expression->kind == "braced-init-list" &&
         !expression->children.empty())
        throw logic_error("class is not an aggregate and has no viable constructor");
      if(expression)
        throw logic_error("class has no viable constructor for initializer");
    }
    TypePtr scalar_target = type_value(variable->type);
    if(expression && expression->kind != "new-expression" && scalar_target &&
       scalar_target->kind != TYPE_CLASS &&
       scalar_target->kind != TYPE_ARRAY && !type_is_reference(variable->type)) {
      ExprInfo source_info = Infer(expression, scope, variable->type);
      const bool direct_initializer = initializer->initializer_form == AST_INITIALIZER_DIRECT_PAREN ||
        initializer->initializer_form == AST_INITIALIZER_DIRECT_LIST;
      const bool explicit_conversion = direct_initializer &&
        expression_value_type(source_info) &&
        expression_value_type(source_info)->kind == TYPE_CLASS &&
        FindConversionOperator(expression_value_type(source_info), variable->type, true);
      if(ConversionRank(source_info, variable->type) < 0 && !explicit_conversion)
        throw logic_error("invalid initializer conversion");
    }
    if(initializer->initializer_form == AST_INITIALIZER_DIRECT_LIST && expression &&
       expression->kind == "braced-init-list") {
      if(expression->children.size() != 1)
        throw logic_error("scalar list-initializer has multiple elements");
      const CPPGMAstNodePtr element = expression->children[0];
      Value source = EmitValue(element, scope, type_value(variable->type));
      TypePtr source_type = type_value(source.type);
      TypePtr target_type = type_value(variable->type);
      if(source_type && target_type && is_floating_type(source_type) &&
         is_integral_type(target_type))
        throw logic_error("narrowing conversion in list-initialization");
      if(source_type && target_type && is_integral_type(source_type) &&
         is_integral_type(target_type) && type_size(source_type) > type_size(target_type)) {
        const unsigned int bits = static_cast<unsigned int>(type_size(target_type) * 8);
        bool fits = source.known_constant;
        if(fits && bits < 64) {
          const long long minimum = is_unsigned_type(target_type) ? 0 :
            -(1LL << (bits - 1));
          const unsigned long long maximum_unsigned = is_unsigned_type(target_type) ?
            ((1ULL << bits) - 1ULL) : static_cast<unsigned long long>((1LL << (bits - 1)) - 1);
          if(is_unsigned_type(source_type))
            fits = source.constant >= 0 &&
              static_cast<unsigned long long>(source.constant) <= maximum_unsigned;
          else
            fits = source.constant >= minimum &&
              static_cast<unsigned long long>(source.constant) <= maximum_unsigned;
        }
        if(!fits) throw logic_error("narrowing conversion in list-initialization");
      }
      expression = element;
    }
    if(!expression) {
      TypePtr object_type = type_value(variable->type);
      if(variable->type->kind != TYPE_FUNCTION &&
         (!object_type || object_type->kind != TYPE_CLASS))
        emit_store(variable->type, "0", StorageForVariable(*variable));
      return;
    }
    Value value;
    const string global_destination = variable->global ?
      global_address(variable->global) : string();
    ExprInfo source_info = Infer(expression, scope, type_value(variable->type));
    const bool direct_initializer = initializer->initializer_form == AST_INITIALIZER_DIRECT_PAREN ||
      initializer->initializer_form == AST_INITIALIZER_DIRECT_LIST;
    if(direct_initializer && expression_value_type(source_info) &&
       expression_value_type(source_info)->kind == TYPE_CLASS &&
       FindConversionOperator(expression_value_type(source_info), variable->type, true)) {
      value = EmitConversionOperator(expression, scope, variable->type, true);
      if(value.lvalue && value.type) {
        value.operand = emit_load(value.operand, value.type);
        value.lvalue = false;
      }
    } else value = EmitValue(expression, scope, type_value(variable->type));
    const bool preserve_sizeof_type = expression->kind == "sizeof-expression" ||
      expression->kind == "sizeof-pack-expression" ||
      expression->kind == "type-trait-expression";
    if(value.known_constant && is_integral_type(value.type) &&
       is_integral_type(variable->type) &&
       !preserve_sizeof_type &&
       (type_size(variable->type) <= type_size(value.type) ||
        (!is_unsigned_type(variable->type) &&
         type_size(variable->type) > type_size(value.type)))) {
      value.type = type_value(variable->type);
      value.operand = integer_text(value.constant);
    } else value = ConvertValue(value, type_value(variable->type), false, true);
    if(variable->global)
      emit_store(type_value(variable->type), value.operand, global_destination);
    else
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
    TypePtr return_type = SourceReturnType(*state_->record);
    if(state_->record->indirect_result) {
      if(!return_type || type_value(return_type)->kind != TYPE_CLASS)
        throw logic_error("indirect result is not a class value");
      if(node->children.empty()) {
        EmitLiveDestructors(scope);
        Terminate("return void");
        return;
      }
      const vector<string> names = ParameterNames(*state_->record);
      if(names.empty()) throw logic_error("indirect result has no destination");
      const CPPGMAstNodePtr expression = node->children[0];
      if(expression->kind == "id-expression" && state_->return_slot_plan &&
         FindLocalPlan(expression->value) == state_->return_slot_plan) {
        EmitLiveDestructors(scope);
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
      if(!EmitObjectTransferAt(type_value(return_type), "%" + names[0],
                               expression, scope, true, implicit_return_move))
        throw logic_error("no viable return value transfer");
      EmitLiveDestructors(scope);
      Terminate("return void");
      return;
    }
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
      EmitLiveDestructors(scope);
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
      if(!EmitObjectTransferAt(return_value_type, destination, expression, scope, true,
                               implicit_return_move))
        throw logic_error("no viable direct class return transfer");
      EmitLiveDestructors(scope);
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
      EmitLiveDestructors(scope);
      Terminate("return " + low_type(return_type) + " " + integer_text(value.constant));
      return;
    }
    value = ConvertValue(value, return_type, true, true);
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
      if(!variable->slot_declared) {
        variable->slot_declared = true;
        state_->slot_order.push_back(FunctionState::SlotEntry(
          false, variable->slot_name, variable));
      }
      EmitInitializer(variable, condition->children[2], scope);
      CPPGMAstNodePtr selector_id(new CPPGMAstNode("id-expression", variable->source_name));
      ExprInfo selector_info = Infer(selector_id, scope);
      TypePtr selector_type = expression_value_type(selector_info);
      if(selector_type && selector_type->kind == TYPE_CLASS &&
         FindContextConversionOperator(selector_type, false, false))
        selector = EmitContextConversion(selector_id, scope, false, false);
      else selector = EmitValue(selector_id, scope);
      if(selector.lvalue && selector.type) {
        selector.operand = emit_load(selector.operand, selector.type);
        selector.lvalue = false;
      }
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
            if(initialized || (referenced &&
                               (!empty_initializer ||
                                HasDefaultInitializationEffects(found->second->type))))
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
