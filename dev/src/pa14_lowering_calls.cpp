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

namespace {

bool PA14ArrayCvCompatible(const TypePtr& source, const TypePtr& target)
{
    if(!source || !target || source->kind != target->kind) return false;
    if(source->is_const && !target->is_const) return false;
    if(source->is_volatile && !target->is_volatile) return false;
    if(source->kind == TYPE_ARRAY)
        return (source->bound == target->bound || source->bound < 0 || target->bound < 0) &&
            PA14ArrayCvCompatible(source->child, target->child);
    return true;
}

} // namespace

string PA14Lowerer::EmitReferenceArgument(const CPPGMAstNodePtr& node, Scope* scope,
                               const TypePtr& target)
{
    TypePtr referred = target->child;
    // A direct prvalue bound to a const/lvalue reference remains a managed
    // full-expression temporary in the PA25 cleanup model.  A cast around a
    // returned object is the forwarding-reference materialization boundary
    // used by earlier assignments; its empty destructor is intentionally
    // elided there.
    bool force_empty_temporary = target &&
      target->kind == TYPE_LVALUE_REFERENCE && node &&
      node->kind != "cast-expression";
    if(force_empty_temporary && referred) {
      const TypePtr referred_value = type_value(referred);
      if(referred_value && referred_value->kind == TYPE_CLASS) {
        bool user_destructor = false;
        const vector<Binding*> destructors = MemberBindings(referred_value,
          "~" + LastComponent(referred_value->name));
        for(size_t destructor = 0; destructor < destructors.size(); ++destructor) {
          FunctionRecord* record = RecordForBinding(destructors[destructor]);
          if(record && record->destructor && record->node &&
             (record->node->kind != "special-member-definition" ||
              record->node->source_token_end != record->node->source_token_begin)) {
            user_destructor = true;
            break;
          }
        }
        force_empty_temporary = user_destructor;
      }
    }
    CPPGMAstNodePtr lambda_source = node;
    while(lambda_source && lambda_source->kind == "parenthesized-expression" &&
          lambda_source->children.size() == 1 && lambda_source->children[0])
      lambda_source = lambda_source->children[0];
    if(lambda_source && lambda_source->kind == "lambda-expression" && referred) {
      const TypePtr closure = LambdaClosureType(lambda_source);
      if(closure && PA12SameType(closure, type_value(referred), true)) {
        const string slot = new_special_slot("arg", low_type(closure));
        const string address = new_temp();
        AddInstruction(address + " = addr $" + slot);
        InitializeLambdaClosureAt(closure, address, lambda_source, scope);
        return address;
      }
    }
    if(node && node->kind == "cast-expression" && node->children.size() > 1 &&
       type_is_reference(target) && node->children[1] &&
       node->children[1]->kind != "call-expression") {
      const TypePtr cast_type = analyzer_.TypeFromTypeId(node->children[0], scope);
      const TypePtr cast_value = cast_type ? type_value(cast_type) : TypePtr();
      const TypePtr referred_value = referred ? type_value(referred) : TypePtr();
      if(cast_type && type_is_reference(cast_type) && cast_value && referred_value &&
         cast_value->kind == TYPE_CLASS && referred_value->kind == TYPE_CLASS &&
         (PA12SameType(cast_value, referred_value, true) ||
          IsDerivedFrom(cast_value, referred_value))) {
        const string source_address = EmitAddress(node->children[1], scope);
        const ExprInfo source_info = Infer(node->children[1], scope);
        const TypePtr source_value = expression_value_type(source_info);
        if(source_value && source_value->kind == TYPE_CLASS &&
           IsDerivedFrom(source_value, cast_value))
          return AdjustBaseAddress(source_address, source_value, cast_value);
        return PA12SameType(cast_value, referred_value, true) ? source_address :
          AdjustBaseAddress(source_address, cast_value, referred_value);
      }
    }
    if(node && node->kind == "cast-expression" && node->children.size() > 1 &&
       type_is_reference(target) && node->children[1] &&
       node->children[1]->kind == "call-expression") {
      const CPPGMAstNodePtr call = node->children[1];
      TypePtr cast_type = analyzer_.TypeFromTypeId(node->children[0], scope);
      ExprInfo call_info = Infer(call, scope);
      TypePtr call_type = expression_value_type(call_info);
      const TypePtr constructed_call = !call->children.empty() ?
        ConstructorObjectType(call->children[0], scope) : TypePtr();
      CallChoice choice;
      FunctionRecord* record = 0;
      if(!constructed_call) {
        choice = ChooseCall(call, scope);
        record = choice.binding ? RecordForBinding(choice.binding) : 0;
      }
      if(cast_type && cast_type->kind == TYPE_RVALUE_REFERENCE && referred &&
         type_value(referred)->kind == TYPE_CLASS && call_type &&
         call_type->kind == TYPE_CLASS && PA12SameType(call_type, type_value(referred), true) &&
         record && record->indirect_result) {
        const string slot = new_special_slot("refcall", low_type(call_type));
        const string address = new_temp();
        AddInstruction(address + " = addr $" + slot);
        const CPPGMAstNodePtr argument_list = call->children.size() > 1 ?
          call->children[1] : CPPGMAstNodePtr();
        const vector<CPPGMAstNodePtr> arguments = argument_list ?
          argument_list->children : vector<CPPGMAstNodePtr>();
        (void)EmitChosenCall(choice, call->children[0], arguments, scope, address);
        RegisterTemporaryObject(call_type, address, force_empty_temporary);
        return address;
      }
    }
    if(node && node->kind == "cast-expression" && node->children.size() > 1 &&
       target->kind == TYPE_RVALUE_REFERENCE &&
       ConstructorObjectType(node->children[1]->children.empty() ?
         CPPGMAstNodePtr() : node->children[1]->children[0], scope) &&
       node->children[1]->kind == "call-expression")
      {
        StartPendingReferenceUnwind(scope);
        return EmitTemporaryObjectAddress(node->children[1], scope, "refcall",
          force_empty_temporary);
      }
    if(node && node->kind == "braced-init-list" && referred &&
       IsInitializerListType(referred))
      return EmitInitializerListArgument(node, target, scope, "arg");
    if(node && node->kind == "braced-init-list" && referred &&
       type_value(referred)->kind == TYPE_CLASS) {
      TypePtr object_type = type_value(referred);
      const string slot = new_special_slot("arg", low_type(object_type));
      const string address = new_temp();
      AddInstruction(address + " = addr $" + slot);
      StartPendingReferenceUnwind(scope);
      const vector<CPPGMAstNodePtr> arguments = node->children;
      if(EmitConstructorAt(object_type, address, arguments, scope, false,
                           false, true)) {
        RegisterTemporaryObject(object_type, address, force_empty_temporary);
        return address;
        }
    }
    if(node && node->kind == "braced-init-list" && referred &&
       type_value(referred)->kind == TYPE_ARRAY) {
      const TypePtr array_type = type_value(referred);
      const string slot = new_special_slot("argarr", low_type(array_type));
      const string address = new_temp();
      AddInstruction(address + " = addr $" + slot);
      EmitAggregateArrayAt(address, array_type, node, scope,
        CPPGMAstNodePtr(), -1, true);
      RegisterTemporaryObject(array_type, address);
      return address;
    }
    if(node && node->kind == "binary-expression" && referred &&
       type_value(referred)->kind == TYPE_CLASS) {
      vector<CPPGMAstNodePtr> operator_arguments;
      operator_arguments.push_back(node->children.empty() ? CPPGMAstNodePtr() :
        node->children[0]);
      if(node->children.size() > 1) operator_arguments.push_back(node->children[1]);
      CallChoice choice = ChooseOperatorCall(
        OperatorFunctionName(PA12Operator(node->value)), operator_arguments, scope);
      FunctionRecord* function = choice.binding ? RecordForBinding(choice.binding) : 0;
      TypePtr result_type = function && function->source_type ?
        type_value(function->source_type->child) :
        (choice.function ? type_value(choice.function->child) : TypePtr());
      if(function && function->indirect_result && result_type &&
         result_type->kind == TYPE_CLASS) {
        const bool direct_class_operand = node && node->children.size() > 0 &&
          node->children[0] && node->children[0]->kind == "call-expression" &&
          !node->children[0]->children.empty() &&
          ConstructorObjectType(node->children[0]->children[0], scope) != TypePtr();
        const bool result_reference_context = state_ && direct_class_operand &&
          !state_->constructor_unwind_active &&
          !state_->suppress_constructor_unwind &&
          !state_->defer_temporary_cleanup && HasDestructor(result_type);
        if(result_reference_context)
          BeginConstructorUnwind(vector<FunctionState::TemporaryObject>(), false);
        const string slot = new_special_slot("arg", low_type(result_type));
        const string address = new_temp();
        AddInstruction(address + " = addr $" + slot);
        StartPendingReferenceUnwind(scope);
        (void)EmitChosenCall(choice, CPPGMAstNodePtr(), operator_arguments,
          scope, address);
        if(result_reference_context && state_ && state_->constructor_unwind_active)
          FinishConstructorUnwind(scope);
        RegisterTemporaryObject(result_type, address, force_empty_temporary);
        if(!PA12SameType(result_type, type_value(referred), true) &&
           IsDerivedFrom(result_type, type_value(referred)))
          return AdjustBaseAddress(address, result_type, type_value(referred));
        return address;
      }
    }
    if(node && node->kind == "call-expression" && !node->children.empty()) {
      TypePtr constructed = ConstructorObjectType(node->children[0], scope);
      if(constructed && referred && constructed->kind == TYPE_CLASS &&
         type_value(referred)->kind == TYPE_CLASS &&
         !PA12SameType(constructed, type_value(referred), true) &&
         IsDerivedFrom(constructed, type_value(referred))) {
        StartPendingReferenceUnwind(scope);
        const string temporary = EmitTemporaryObjectAddress(node, scope, "tmpobj",
          force_empty_temporary);
        if(state_ && state_->constructor_unwind_active &&
           !state_->constructor_unwind_call)
          FinishConstructorUnwind(scope);
        if(state_ && !state_->constructor_unwind_active &&
           HasDestructor(constructed) &&
           (DestructorHasEffects(constructed) || force_empty_temporary))
          BeginConstructorUnwind(CaptureLiveCleanupObjects(), true);
        return AdjustBaseAddress(temporary, constructed, referred);
      }
      if(constructed && referred && constructed->kind == TYPE_CLASS &&
         type_value(referred)->kind == TYPE_CLASS &&
         !PA12SameType(constructed, type_value(referred), true) &&
         !IsDerivedFrom(constructed, type_value(referred))) {
        const string slot = new_special_slot("arg", low_type(type_value(referred)));
        const string address = new_temp();
        AddInstruction(address + " = addr $" + slot);
        StartPendingReferenceUnwind(scope);
        vector<CPPGMAstNodePtr> constructor_arguments;
        constructor_arguments.push_back(node);
        if(EmitConstructorAt(type_value(referred), address, constructor_arguments,
                             scope, false)) {
          RegisterTemporaryObject(type_value(referred), address,
            force_empty_temporary);
          return address;
        }
      }
      if(constructed) {
        StartPendingReferenceUnwind(scope);
        return EmitTemporaryObjectAddress(node, scope, "arg",
          force_empty_temporary);
      }
    }
	if(node && node->kind == "call-expression" && !node->children.empty() &&
		!BuiltinCastType(node->children[0], scope)) {
		CallChoice choice = ChooseCall(node, scope);
      FunctionRecord* function = choice.binding ? RecordForBinding(choice.binding) : 0;
      TypePtr result_type = choice.function ? type_value(choice.function->child) : TypePtr();
      if(function && function->indirect_result && result_type &&
         result_type->kind == TYPE_CLASS && referred &&
         type_value(referred)->kind == TYPE_CLASS) {
        const string slot = new_special_slot("arg", low_type(result_type));
        const string address = new_temp();
        AddInstruction(address + " = addr $" + slot);
        const CPPGMAstNodePtr argument_list = node->children.size() > 1 ?
          node->children[1] : CPPGMAstNodePtr();
        vector<CPPGMAstNodePtr> arguments = argument_list ?
          argument_list->children : vector<CPPGMAstNodePtr>();
        if(node->value == "braced-construction" && arguments.size() == 1 &&
           arguments[0] && arguments[0]->kind == "braced-init-list")
          arguments = arguments[0]->children;
        StartPendingReferenceUnwind(scope);
        (void)EmitChosenCall(choice, node->children[0], arguments, scope, address);
        RegisterTemporaryObject(result_type, address,
          target && target->kind == TYPE_LVALUE_REFERENCE);
        if(!PA12SameType(result_type, type_value(referred), true) &&
           IsDerivedFrom(result_type, type_value(referred)))
          return AdjustBaseAddress(address, result_type, type_value(referred));
        return address;
      }
    }
    ExprInfo source = Infer(node, scope);
    TypePtr source_type = expression_value_type(source);
    const bool referred_const_array = referred && referred->kind == TYPE_ARRAY &&
      referred->child && referred->child->is_const;
    const bool unknown_bound_array_reference = source_type && referred &&
      (referred->is_const || referred_const_array) &&
      source_type->kind == TYPE_ARRAY && referred->kind == TYPE_ARRAY &&
      (source_type->bound < 0 || referred->bound < 0) &&
      PA14ArrayCvCompatible(source_type, referred);
    const bool same_referred_type = PA12SameType(source_type, referred, true) ||
      unknown_bound_array_reference;
    const bool direct_address = source.category == "lvalue" &&
      same_referred_type &&
      (!referred->is_const || !source_type->is_const || referred->is_const);
    // An xvalue already denotes a usable object address for an rvalue
    // reference.  Trying to construct another object here recursively
    // re-enters this same binding path for move constructors.
    const bool direct_rvalue = source.category == "xvalue" &&
      target->kind == TYPE_RVALUE_REFERENCE &&
      PA12SameType(source_type, referred, true);
    const bool direct_const_lvalue = source.category == "xvalue" &&
      target->kind == TYPE_LVALUE_REFERENCE && referred && referred->is_const &&
      PA12SameType(source_type, referred, true);
    if(direct_address || direct_rvalue || direct_const_lvalue)
      return EmitAddress(node, scope);
    if(source.category == "xvalue" && referred && source_type &&
       source_type->kind == TYPE_CLASS && referred->kind == TYPE_CLASS &&
       IsDerivedFrom(source_type, type_value(referred)))
      return AdjustBaseAddress(EmitAddress(node, scope), source_type,
                               type_value(referred));
    if(source.category == "lvalue" && IsDerivedFrom(source_type, referred))
      return AdjustBaseAddress(EmitAddress(node, scope), source_type, referred);
    if(source_type && source_type->kind == TYPE_CLASS && referred &&
       referred->kind == TYPE_CLASS) {
      Binding* conversion = FindConversionOperator(source_type, referred, false);
      if(conversion) {
        // A class-valued conversion used as a reference argument needs the
        // same temporary category as an ordinary constructor argument.  The
        // generic conversion helper creates a return-object slot, but this
        // path is an argument boundary and must materialize the result in an
        // arg slot before the destination copy/move constructor sees it.
        const TypePtr conversion_function = function_target_type(conversion->type);
        const TypePtr conversion_result = conversion_function ?
          conversion_function->child : TypePtr();
        Value converted;
        string conversion_address;
        if(conversion_result && type_is_reference(conversion_result)) {
          converted = EmitConversionOperator(node, scope, referred, false);
        } else {
          CallChoice choice;
          choice.binding = conversion;
          choice.function = conversion_function;
          choice.object = node;
          choice.direct = true;
          choice.member = true;
          choice.static_member = false;
          choice.conversion = true;
          const string slot = new_special_slot("arg", low_type(referred));
          FunctionRecord* conversion_record = RecordForBinding(conversion);
          const bool direct_template_conversion = conversion_record &&
            (conversion_record->member_template ||
             conversion_record->template_instantiation);
          if(direct_template_conversion) {
            conversion_address = new_temp();
            AddInstruction(conversion_address + " = addr $" + slot);
            converted = EmitChosenCall(choice, CPPGMAstNodePtr(),
              vector<CPPGMAstNodePtr>(), scope, conversion_address);
          } else {
            conversion_address = new_temp();
            AddInstruction(conversion_address + " = addr $" + slot);
            converted = EmitChosenCall(choice, CPPGMAstNodePtr(),
              vector<CPPGMAstNodePtr>(), scope);
          }
        }
        if(converted.operand.empty()) return converted.operand;
        if(converted.lvalue) return converted.operand;
        // A class-valued conversion function returns through the ABI's
        // indirect-result slot.  That slot is the temporary bound to the
        // reference; falling through to EmitConstructorAt would retry the
        // same conversion as a converting constructor indefinitely.
        if(converted.type && type_value(converted.type) &&
           type_value(converted.type)->kind == TYPE_CLASS) {
          const TypePtr converted_type = type_value(converted.type);
          if(!conversion_address.empty() && converted.operand != conversion_address) {
            AddInstruction("copyobj " +
              integer_text(static_cast<long long>(type_size(converted_type))) +
              "x" + integer_text(static_cast<long long>(type_alignment(converted_type))) +
              " " + converted.operand + ", " + conversion_address);
            RegisterTemporaryObject(converted_type, conversion_address,
              force_empty_temporary);
            return conversion_address;
          }
          RegisterTemporaryObject(converted_type, converted.operand,
            force_empty_temporary);
          return converted.operand;
        }
      }
    }
    if(referred && referred->kind == TYPE_CLASS && source.category == "prvalue" &&
       source_type && source_type->kind == TYPE_CLASS &&
       (PA12SameType(source_type, referred, true) ||
        IsDerivedFrom(source_type, type_value(referred)))) {
      // A direct class return is an object value.  A reference to that
      // prvalue needs a real temporary object, and a derived result needs its
      // typed base projection; storing the value into a refarg slot loses
      // both lifetime and base-subobject shape.
      bool template_context = state_ && state_->record &&
        state_->record->template_instantiation;
      if(node && node->kind == "call-expression") {
        const CallChoice choice = ChooseCall(node, scope);
        FunctionRecord* function = choice.binding ? RecordForBinding(choice.binding) : 0;
        template_context = template_context || (function && function->template_instantiation);
      }
      const bool operator_result = node && node->kind == "binary-expression";
      const bool member_call_result = node && node->kind == "call-expression" &&
        node->children.size() > 0 && node->children[0] &&
        node->children[0]->kind == "member-expression";
      const string slot = new_special_slot(template_context || operator_result ||
        member_call_result ? "arg" : "tmpobj", low_type(source_type));
      const string address = new_temp();
      AddInstruction(address + " = addr $" + slot);
      StartPendingReferenceUnwind(scope);
      Value value = EmitValue(node, scope);
      AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(source_type))) +
        "x" + integer_text(static_cast<long long>(type_alignment(source_type))) +
        " " + value.operand + ", " + address);
      RegisterTemporaryObject(source_type, address,
        force_empty_temporary);
      return PA12SameType(source_type, referred, true) ? address :
        AdjustBaseAddress(address, source_type, type_value(referred));
    }
    if(referred && referred->kind == TYPE_CLASS) {
      const string slot = new_special_slot("arg", low_type(referred));
      const string address = new_temp();
      AddInstruction(address + " = addr $" + slot);
      StartPendingReferenceUnwind(scope);
      vector<CPPGMAstNodePtr> constructor_arguments;
      constructor_arguments.push_back(node);
      if(EmitConstructorAt(referred, address, constructor_arguments, scope, false))
        return address;
    }
    const string slot = new_special_slot("refarg", low_type(referred));
    Value value = EmitValue(node, scope, referred);
    value = ConvertValue(value, referred, false, true);
    emit_store(referred, value.operand, "$" + slot);
    const string address = new_temp();
    AddInstruction(address + " = addr $" + slot);
    return address;
  }

PA14Lowerer::Value PA14Lowerer::EmitConversionOperator(
  const CPPGMAstNodePtr& node, Scope* scope, const TypePtr& target,
  bool allow_explicit)
{
    ExprInfo source = Infer(node, scope);
    Binding* binding = FindConversionOperator(expression_value_type(source), target,
      allow_explicit);
    if(!binding) throw logic_error("no viable conversion operator");
    CallChoice choice;
    choice.binding = binding;
    choice.function = function_target_type(binding->type);
    choice.object = node;
    choice.direct = true;
    choice.member = true;
    choice.static_member = false;
    choice.conversion = true;
    vector<CPPGMAstNodePtr> arguments;
    return EmitChosenCall(choice, CPPGMAstNodePtr(), arguments, scope);
  }

PA14Lowerer::Value PA14Lowerer::EmitContextConversion(
  const CPPGMAstNodePtr& node, Scope* scope, bool allow_explicit,
  bool boolean_context)
{
    ExprInfo source = Infer(node, scope);
    Binding* binding = FindContextConversionOperator(expression_value_type(source),
      allow_explicit, boolean_context);
    if(!binding) throw logic_error("no viable contextual conversion operator");
    CallChoice choice;
    choice.binding = binding;
    choice.function = function_target_type(binding->type);
    choice.object = node;
    choice.direct = true;
    choice.member = true;
    choice.static_member = false;
    choice.conversion = true;
    vector<CPPGMAstNodePtr> arguments;
    return EmitChosenCall(choice, CPPGMAstNodePtr(), arguments, scope);
  }

PA14Lowerer::Value PA14Lowerer::EmitChosenCall(
    const CallChoice& choice, const CPPGMAstNodePtr& callee_node,
    const vector<CPPGMAstNodePtr>& arguments, Scope* scope,
    const string& indirect_destination, bool full_expression_cleanup)
{
    FunctionRecord* function_record = 0;
    const size_t temporary_mark = state_ ? state_->temporary_objects.size() : 0;
    const bool previous_full_expression_defer = state_ &&
      state_->defer_temporary_cleanup;
    if(full_expression_cleanup && state_)
      state_->defer_temporary_cleanup = true;
    // Synthetic operator calls include the object in their argument vector;
    // ordinary member calls do not.  Keep the latter's first explicit
    // argument intact.
    const size_t argument_offset = !callee_node && choice.member &&
      !choice.static_member ? 1 : 0;
    vector<CPPGMAstNodePtr> all_arguments;
    for(size_t i = argument_offset; i < arguments.size(); ++i)
      all_arguments.push_back(arguments[i]);
    vector<string> operands;
    function_record = choice.binding ? RecordForBinding(choice.binding) : 0;
    if(function_record && function_record->deleted)
      throw logic_error("call to deleted function " + function_record->qualified_name);
    if(function_record) {
      // Calls emitted while lowering a materialized generated body can reach
      // a generated free helper after ordinary root-demand collection has
      // finished.  Demand is a property of the emitted call, not of the
      // declaration's template-instantiation bit; mark the selected record
      // here so unused SFINAE/dependent helpers remain un-emitted.
      MarkFunctionNeeded(function_record);
      if(function_record->member) {
        FunctionRecord* base_entry = BaseEntryFor(function_record);
        if(base_entry) MarkFunctionNeeded(base_entry);
      }
    }
    string planned_unwind_result_slot;
    const TypePtr early_return_type = choice.function ? choice.function->child : TypePtr();
    const TypePtr early_return_value = type_value(early_return_type);
    const string early_return_low = function_record && function_record->type ?
      low_type(function_record->type->child) : low_type(early_return_type);
    const bool selected_call_no_throw =
      (function_record && function_record->unwind_no) ||
      (choice.binding && (choice.binding->noexcept_specified ||
        HasNoexcept(choice.binding->declaration))) ||
      (state_ && state_->record && state_->record->destructor);
    const bool indirect_result_unwind = state_ && function_record &&
      function_record->indirect_result && early_return_value &&
      early_return_value->kind == TYPE_CLASS && HasDestructor(early_return_value);
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
    const auto has_explicit_destructor = [this](const TypePtr& raw_type) {
      const TypePtr type = type_value(raw_type);
      if(!type || type->kind != TYPE_CLASS) return false;
      const vector<Binding*> destructors = MemberBindings(type,
        "~" + LastComponent(type->name));
      for(size_t destructor = 0; destructor < destructors.size(); ++destructor) {
        FunctionRecord* record = RecordForBinding(destructors[destructor]);
        if(record && record->destructor && record->node &&
           (record->node->kind != "special-member-definition" ||
            record->node->source_token_end != record->node->source_token_begin))
          return true;
      }
      return false;
    };
    contains_managed_temporary = [&](const CPPGMAstNodePtr& expression) -> bool {
      if(!expression) return false;
    if(expression->kind == "call-expression" && !expression->children.empty()) {
        TypePtr constructed = ConstructorObjectType(expression->children[0], scope);
        if(constructed && HasDestructor(constructed) &&
           (DestructorHasEffects(constructed) || has_explicit_destructor(constructed)) &&
           !constructor_is_nothrow(constructed)) return true;
        try {
          const ExprInfo expression_info = Infer(expression, scope);
          TypePtr value = expression_value_type(expression_info);
          if(value && value->kind == TYPE_CLASS &&
             !type_is_reference(expression_info.type) && HasDestructor(value) &&
             (DestructorHasEffects(value) || has_explicit_destructor(value))) return true;
      } catch(const logic_error&) {
      }
    }
    if((expression->kind == "braced-construction" ||
        expression->kind == "braced-init-list") && !expression->children.empty()) {
      TypePtr constructed = ConstructorObjectType(expression->children[0], scope);
      if(constructed && HasDestructor(constructed) &&
         (DestructorHasEffects(constructed) || has_explicit_destructor(constructed))) return true;
    }
      if(expression->kind == "binary-expression") {
        try {
          const TypePtr value = expression_value_type(Infer(expression, scope));
          if(value && value->kind == TYPE_CLASS &&
             HasDestructor(value) &&
             (DestructorHasEffects(value) || has_explicit_destructor(value))) return true;
        } catch(const logic_error&) {
        }
      }
      for(size_t child = 0; child < expression->children.size(); ++child)
        if(contains_managed_temporary(expression->children[child])) return true;
      return false;
    };
    bool polymorphic_choice_object = false;
    if(choice.member && choice.object) {
      try {
        const TypePtr object_value = expression_value_type(Infer(choice.object, scope));
        polymorphic_choice_object = object_value &&
          object_value->kind == TYPE_CLASS && object_value->polymorphic;
      } catch(const logic_error&) {
      }
    }
    const bool deferred_class_receiver = state_ &&
      state_->defer_temporary_cleanup && choice.member && choice.object &&
      choice.object->kind == "call-expression";
    if(early_return_low != "void" && early_return_value &&
       (early_return_value->kind != TYPE_CLASS || type_is_reference(early_return_type)) &&
       ((state_ && state_->condition_cleanup_depth == 2) ||
        (!selected_call_no_throw && polymorphic_choice_object)) &&
       (HasLiveCleanupObjects() || contains_managed_temporary(choice.object) ||
        deferred_class_receiver))
      planned_unwind_result_slot = new_special_slot("call", early_return_low);
    if(early_return_low != "void" && early_return_value &&
       (early_return_value->kind != TYPE_CLASS || type_is_reference(early_return_type)) &&
       planned_unwind_result_slot.empty()) {
    for(size_t argument = 0; argument < all_arguments.size(); ++argument)
        if(contains_managed_temporary(all_arguments[argument])) {
          planned_unwind_result_slot = new_special_slot("call", early_return_low);
          break;
        }
    }
    if(function_record) {
      while(all_arguments.size() < choice.function->parameters.size()) {
        const size_t index = all_arguments.size();
        if(index >= function_record->default_arguments.size() ||
           !function_record->default_arguments[index]) break;
        CPPGMAstNodePtr initializer = function_record->default_arguments[index];
        all_arguments.push_back(InitializerExpression(initializer));
      }
    }
    if(indirect_result_unwind && !state_->constructor_unwind_active &&
       !state_->suppress_constructor_unwind &&
       !CaptureLiveCleanupObjects().empty()) {
      BeginConstructorUnwind(vector<FunctionState::TemporaryObject>(), true);
      state_->pending_call_argument_context = true;
      state_->preserve_call_argument_context = false;
    }
    // A call may throw while evaluating an ordinary scalar argument, not only
    // while materializing a managed argument.  If class objects are already
    // live at the call boundary, keep their typed cleanup region open across
    // the complete argument evaluation.  This also lets later calls reuse
    // the same dispatch block when their live cleanup set is unchanged.
    if(state_ && !state_->constructor_unwind_active &&
       !state_->suppress_constructor_unwind &&
       !type_is_reference(early_return_type) &&
       !selected_call_no_throw &&
       !CaptureLiveCleanupObjects().empty()) {
      BeginConstructorUnwind(CaptureLiveCleanupObjects(), true);
      state_->pending_call_argument_context = true;
      state_->preserve_call_argument_context = false;
    }
    bool deferred_managed_expression = false;
    for(size_t argument = 0; argument < all_arguments.size(); ++argument) {
      CPPGMAstNodePtr expression = all_arguments[argument];
      while(expression && expression->kind == "parenthesized-expression" &&
            expression->children.size() == 1)
        expression = expression->children[0];
      if(expression && expression->kind == "binary-expression" &&
         contains_managed_temporary(expression)) {
        deferred_managed_expression = true;
        break;
      }
    }
    if(state_ && !state_->constructor_unwind_active &&
       !state_->suppress_constructor_unwind &&
       (!state_->defer_temporary_cleanup || deferred_managed_expression)) {
      size_t managed_argument_count = 0;
      size_t managed_class_value_count = 0;
      for(size_t argument = 0; argument < all_arguments.size(); ++argument)
        if(contains_managed_temporary(all_arguments[argument])) {
          ++managed_argument_count;
          const TypePtr parameter = argument < choice.function->parameters.size() ?
            choice.function->parameters[argument] : TypePtr();
          if(parameter && !type_is_reference(parameter) && type_value(parameter) &&
             type_value(parameter)->kind == TYPE_CLASS)
            ++managed_class_value_count;
        }
      for(size_t argument = 0; argument < all_arguments.size(); ++argument)
        if(contains_managed_temporary(all_arguments[argument])) {
          BeginConstructorUnwind(CaptureLiveCleanupObjects(), false);
          state_->pending_call_argument_context = true;
          state_->preserve_call_argument_context = managed_argument_count > 1 &&
            managed_class_value_count == managed_argument_count;
          break;
        }
    }
    string indirect_result_address;
    string virtual_object_operand;
    bool call_argument_context_started = false;
    auto begin_argument_unwind = [&](const CPPGMAstNodePtr& expression,
                                     size_t mark) -> bool {
      if(!state_ || state_->constructor_unwind_active ||
         state_->suppress_constructor_unwind ||
         !contains_managed_temporary(expression)) return false;
      const vector<FunctionState::TemporaryObject> cleanup =
        CaptureLiveCleanupObjects();
      if(cleanup.empty()) return false;
      BeginConstructorUnwind(cleanup, false);
      (void)mark;
      return true;
    };
    auto finish_argument_unwind = [&](size_t mark, size_t index) {
      if(!state_) return;
      if(state_->constructor_unwind_active &&
         !state_->constructor_unwind_call &&
         !state_->preserve_call_argument_context &&
         state_->temporary_objects.size() > mark)
        FinishConstructorUnwind(scope);
      if(index + 1 < all_arguments.size() &&
         !state_->constructor_unwind_active &&
         !state_->suppress_constructor_unwind) {
        const vector<FunctionState::TemporaryObject> next_cleanup =
          CaptureLiveCleanupObjects();
        if(!next_cleanup.empty()) {
          const bool next_managed = contains_managed_temporary(all_arguments[index + 1]);
          BeginConstructorUnwind(next_cleanup, !next_managed);
          if(!next_managed) {
            call_argument_context_started = true;
            state_->pending_call_argument_context = true;
          }
        }
      }
    };
    if(function_record && function_record->indirect_result) {
      const TypePtr result_type = type_value(choice.function->child);
      if(!indirect_destination.empty()) indirect_result_address = indirect_destination;
      else {
        const string slot = new_special_slot("retobj", low_type(result_type));
        indirect_result_address = new_temp();
        AddInstruction(indirect_result_address + " = addr $" + slot);
      }
      operands.push_back(indirect_result_address);
    }
    if(choice.member && !choice.static_member) {
      if(!choice.object) throw logic_error("member call has no object");
      const size_t object_temporary_mark = state_ ?
        state_->temporary_objects.size() : 0;
      const bool object_unwind_started =
        begin_argument_unwind(choice.object, object_temporary_mark);
      string object_operand;
      ExprInfo object_info = Infer(choice.object, scope);
      TypePtr object_type = expression_value_type(object_info);
      const TypePtr receiver_owner = choice.member_pointer_owner ?
        choice.member_pointer_owner :
        (choice.binding ? choice.binding->member_owner : TypePtr());
      bool used_hidden_receiver = false;
      bool hidden_cast_receiver = false;
      string hidden_receiver_source;
      if(choice.object && choice.object->kind == "keyword-literal" &&
         PA12Operator(choice.object->value) == "this")
        hidden_receiver_source = "this";
      else if(choice.object && choice.object->kind == "id-expression")
        hidden_receiver_source = choice.object->value;
      else if(choice.object && choice.object->kind == "cast-expression" &&
              choice.object->children.size() > 1) {
        CPPGMAstNodePtr source = choice.object->children[1];
        while(source && source->kind == "parenthesized-expression" &&
              source->children.size() == 1) source = source->children[0];
        if(source && source->kind == "unary-expression" &&
           PA12Operator(source->value) == "*" && !source->children.empty() &&
           source->children[0] && source->children[0]->kind == "keyword-literal" &&
           PA12Operator(source->children[0]->value) == "this") {
          hidden_receiver_source = "this";
          hidden_cast_receiver = true;
        }
      }
      // A member call on a complete object whose selected owner is a virtual
      // base can use the incoming typed view directly.  Resolve this before
      // materializing the ordinary receiver so the LowIR does not contain a
      // dead load of `this`/the complete object before the hidden view.
      TypePtr hidden_object_type = object_type;
      if(hidden_object_type && hidden_object_type->kind == TYPE_POINTER)
        hidden_object_type = type_value(hidden_object_type->child);
      if(hidden_cast_receiver) {
        const TypePtr complete_owner = state_ && state_->record &&
          state_->record->member_owner ? state_->record->member_owner :
          (function_record ? function_record->member_owner : TypePtr());
        if(complete_owner) hidden_object_type = type_value(complete_owner);
      }
      if(state_ && receiver_owner && hidden_object_type && hidden_object_type->kind == TYPE_CLASS &&
         !hidden_receiver_source.empty()) {
        map<string, vector<string> >::const_iterator hidden_this =
          state_->virtual_base_hidden_by_source.find(hidden_receiver_source);
        if(hidden_this != state_->virtual_base_hidden_by_source.end()) {
          size_t virtual_index = 0;
          size_t relative = 0;
          bool found = false;
          for(; virtual_index < hidden_object_type->virtual_base_types.size(); ++virtual_index)
            if(hidden_object_type->virtual_base_types[virtual_index] &&
               (SameLayoutType(hidden_object_type->virtual_base_types[virtual_index], receiver_owner) ||
                (virtual_index < hidden_object_type->virtual_base_roots.size() &&
                 hidden_object_type->virtual_base_roots[virtual_index] &&
                 FindVirtualBaseOffset(hidden_object_type->virtual_base_roots[virtual_index],
                   receiver_owner, &relative)))) {
              found = true;
              break;
            }
          if(found && virtual_index < hidden_this->second.size()) {
            object_operand = hidden_this->second[virtual_index];
            if(!object_operand.empty() && object_operand[0] == '$')
              object_operand = emit_load(object_operand, PointerTo(Fundamental("char")));
            if(relative != 0) {
              const string adjusted = new_temp();
              AddInstruction(adjusted + " = index i8 " + object_operand + ", " +
                integer_text(static_cast<long long>(relative)));
              object_operand = adjusted;
            }
            if(!hidden_cast_receiver) {
              const string projected = new_temp();
              AddInstruction(projected + " = index i8 " + object_operand + ", 0");
              object_operand = projected;
            }
            object_type = receiver_owner;
            used_hidden_receiver = true;
          }
        }
      }
      if(!used_hidden_receiver && object_type && object_type->kind == TYPE_POINTER) {
        object_operand = EmitValue(choice.object, scope).operand;
        object_type = type_value(object_type->child);
	      } else if(!used_hidden_receiver && (object_info.category == "lvalue" ||
	                (choice.object->kind == "keyword-literal" &&
	                 PA12Operator(choice.object->value) == "this"))) {
        object_operand = EmitAddress(choice.object, scope);
	      } else if(!used_hidden_receiver && choice.object->kind == "cast-expression" &&
	                choice.object->children.size() > 1 && object_type &&
	                object_type->kind == TYPE_CLASS &&
	                type_is_reference(analyzer_.TypeFromTypeId(
	                  choice.object->children[0], scope))) {
	        // A reference cast denotes the selected subobject; materializing its
	        // class value here would copy the most-derived object and lose the
	        // typed base projection used by a subsequent member call.
	        object_operand = EmitAddress(choice.object, scope);
      } else if(!used_hidden_receiver && object_type && object_type->kind == TYPE_CLASS &&
	                object_info.category == "xvalue") {
	        // An xvalue class expression denotes an existing object.  A
	        // forwarding-reference helper can return the callable closure by
	        // reference, so materializing it here would add an extra copy.
	        object_operand = EmitAddress(choice.object, scope);
      } else if(!used_hidden_receiver && choice.object->kind == "call-expression" &&
                !choice.object->children.empty() &&
                ConstructorObjectType(choice.object->children[0], scope)) {
        object_operand = EmitTemporaryObjectAddress(choice.object, scope, "tmpobj");
        // An empty-storage temporary used only as the receiver of a scalar
        // member call has no value boundary to carry beyond that call in the
        // existing object model.  Keep its constructor boundary for unwind,
        // but retire the receiver record before the call's result is lowered.
        // Reference-bound and argument temporaries use their own typed path
        // and are intentionally retained.
        if(state_ && object_type && IsEmptyBaseStorage(object_type) &&
           choice.function && type_value(choice.function->child) &&
           type_value(choice.function->child)->kind != TYPE_CLASS) {
          if(state_->temporary_objects.size() > object_temporary_mark &&
             state_->temporary_objects.back().address == object_operand)
            state_->temporary_objects.pop_back();
        }
      } else if(!used_hidden_receiver && choice.object->kind == "call-expression" && object_type &&
                object_type->kind == TYPE_CLASS) {
        // A class-valued call is a prvalue object, not a scalar to spill into
        // an arbitrary member-call slot.  Give the common object-transfer
        // path the final member-call destination so an indirect class return
        // can construct there without an intermediate return object.
        const string slot = new_special_slot("tmpobj", low_type(object_type));
        object_operand = new_temp();
        AddInstruction(object_operand + " = addr $" + slot);
        if(!EmitObjectTransferAt(object_type, object_operand, choice.object, scope, true)) {
          Value object_value = EmitValue(choice.object, scope);
          AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(object_type))) +
            "x" + integer_text(static_cast<long long>(type_alignment(object_type))) + " " +
            object_value.operand + ", " + object_operand);
        }
      } else if(!used_hidden_receiver && object_type && object_type->kind == TYPE_CLASS &&
                object_info.category != "lvalue") {
        CPPGMAstNodePtr lambda_object = choice.object;
        while(lambda_object && lambda_object->kind == "parenthesized-expression" &&
          lambda_object->children.size() == 1 && lambda_object->children[0])
          lambda_object = lambda_object->children[0];
        const bool operator_object = lambda_object &&
          (lambda_object->kind == "binary-expression" ||
           lambda_object->kind == "call-expression");
        const bool object_constructor_context = state_ &&
          !state_->constructor_unwind_active &&
          !state_->suppress_constructor_unwind && object_type->polymorphic &&
          HasDestructor(object_type) &&
          operator_object;
        if(object_constructor_context)
          BeginConstructorUnwind(vector<FunctionState::TemporaryObject>(), false);
        const string slot = new_special_slot("tmpobj", low_type(object_type));
        object_operand = new_temp();
        AddInstruction(object_operand + " = addr $" + slot);
        if((!lambda_object || (lambda_object->kind != "lambda-expression" &&
                              !operator_object)) ||
           !EmitObjectTransferAt(type_value(object_type), object_operand,
                                 choice.object, scope, true)) {
          Value object_value = EmitValue(choice.object, scope);
          AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(object_type))) +
            "x" + integer_text(static_cast<long long>(type_alignment(object_type))) + " " +
            object_value.operand + ", " + object_operand);
        }
      } else if(!used_hidden_receiver && choice.conversion && object_info.category == "lvalue" &&
                object_type && object_type->kind == TYPE_CLASS) {
        object_operand = EmitAddress(choice.object, scope);
      } else if(!used_hidden_receiver) {
        Value object_value = EmitValue(choice.object, scope);
        const string slot = new_special_slot("object", low_type(object_type));
        emit_store(object_type, object_value.operand, "$" + slot);
        object_operand = new_temp();
        AddInstruction(object_operand + " = addr $" + slot);
      }
      if(choice.project_base_type && object_type &&
         !PA12SameType(object_type, choice.project_base_type, true)) {
        object_operand = AdjustBaseAddress(object_operand, object_type,
          choice.project_base_type, true);
        object_type = choice.project_base_type;
      }
      const bool hidden_from_complete_receiver = function_record &&
        function_record->member && !function_record->static_member &&
        receiver_owner && object_type && object_type->kind == TYPE_CLASS &&
        !PA12SameType(object_type, receiver_owner, true) &&
        IsDerivedFrom(object_type, receiver_owner) &&
        !function_record->hidden_virtual_bases.empty() && state_;
      if(!used_hidden_receiver)
        object_operand = AdjustBaseAddress(object_operand, object_type,
          receiver_owner,
          choice.project_base_path);
      if(hidden_from_complete_receiver) {
        // The ordinary receiver is the selected base view.  Hidden virtual
        // arguments, however, are rooted in the complete lvalue used to form
        // that receiver; retain a separate typed address evaluation so the
        // shared virtual subobject is not reconstructed from the base view.
        const string complete_receiver = EmitAddress(choice.object, scope);
        vector<string> hidden_addresses;
        for(size_t hidden = 0;
            hidden < function_record->hidden_virtual_bases.size(); ++hidden) {
          size_t offset = 0;
          if(!FindVirtualBaseOffset(object_type,
              function_record->hidden_virtual_bases[hidden], &offset)) {
            hidden_addresses.push_back(string());
            continue;
          }
          const string address = new_temp();
          AddInstruction(address + " = index i8 " + complete_receiver + ", " +
            integer_text(static_cast<long long>(offset)));
          hidden_addresses.push_back(address);
        }
        state_->virtual_base_hidden_by_operand[object_operand] = hidden_addresses;
      }
      operands.push_back(object_operand);
      if(choice.virtual_dispatch) virtual_object_operand = object_operand;
      if(object_unwind_started && state_ && state_->constructor_unwind_active &&
         !state_->constructor_unwind_call &&
         state_->temporary_objects.size() > object_temporary_mark)
        FinishConstructorUnwind(scope);
    }
    // A class-valued receiver materialized above is live for every following
    // argument evaluation and for the virtual call itself.  Start the call
    // context before evaluating those arguments so a throwing reference
    // argument is covered by the receiver's destructor cleanup.
    if(state_ && !state_->constructor_unwind_active &&
       !state_->suppress_constructor_unwind &&
       (!function_record || !function_record->unwind_no)) {
      const vector<FunctionState::TemporaryObject> receiver_cleanup =
        CaptureLiveCleanupObjects();
      bool polymorphic_receiver = false;
      for(size_t cleanup = 0; cleanup < receiver_cleanup.size(); ++cleanup) {
        const TypePtr cleanup_type = type_value(receiver_cleanup[cleanup].type);
        if(cleanup_type && cleanup_type->kind == TYPE_CLASS &&
           cleanup_type->polymorphic) {
          polymorphic_receiver = true;
          break;
        }
      }
      if(polymorphic_receiver && !selected_call_no_throw) {
        BeginConstructorUnwind(receiver_cleanup, true);
        state_->pending_call_argument_context = true;
        state_->preserve_call_argument_context = false;
      }
    }
    for(size_t i = 0; i < all_arguments.size(); ++i) {
      // A string literal is a non-throwing address formation.  If the
      // preceding argument opened a call-boundary cleanup region, close that
      // region before materializing the literal so the subsequent overloaded
      // call gets its own protected boundary.
      if(i != 0 && state_ && state_->constructor_unwind_active &&
         state_->constructor_unwind_call &&
         all_arguments[i] && all_arguments[i]->kind == "literal" &&
         !all_arguments[i]->value.empty() && all_arguments[i]->value[0] == '"') {
        FinishConstructorUnwind(scope);
        state_->pending_call_argument_context = false;
        const vector<FunctionState::TemporaryObject> literal_cleanup =
          CaptureLiveCleanupObjects();
        if(!literal_cleanup.empty()) {
          BeginConstructorUnwind(literal_cleanup, true);
          state_->pending_call_argument_context = true;
          state_->preserve_call_argument_context = false;
        }
      }
      const size_t argument_temporary_mark = state_ ?
        state_->temporary_objects.size() : 0;
      begin_argument_unwind(all_arguments[i], argument_temporary_mark);
      TypePtr target = i < choice.function->parameters.size() ? choice.function->parameters[i] : TypePtr();
      // An untyped variadic argument that is a direct class construction still
      // has an object ABI.  Materialize the temporary and pass its address;
      // loading it as a value loses the constructor call and disagrees with
      // the representation used by the corresponding class parameter path.
      if(!target && all_arguments[i] && all_arguments[i]->kind == "call-expression" &&
         !all_arguments[i]->children.empty()) {
        TypePtr temporary_type = ConstructorObjectType(all_arguments[i]->children[0], scope);
        if(temporary_type && temporary_type->kind == TYPE_CLASS) {
          operands.push_back(EmitTemporaryObjectAddress(all_arguments[i], scope, "arg"));
          finish_argument_unwind(argument_temporary_mark, i);
          continue;
        }
      }
      if(target && all_arguments[i] && all_arguments[i]->kind == "braced-init-list" &&
         IsInitializerListType(target)) {
        operands.push_back(EmitInitializerListArgument(all_arguments[i], target, scope,
          type_is_reference(target) ? "arg" : "argobj"));
        finish_argument_unwind(argument_temporary_mark, i);
        continue;
      }
      if(target && type_is_reference(target)) {
        operands.push_back(EmitReferenceArgument(all_arguments[i], scope, target));
        finish_argument_unwind(argument_temporary_mark, i);
        continue;
      }
      const size_t low_index = (function_record && function_record->indirect_result ? 1 : 0) +
        (choice.member && !choice.static_member ? 1 : 0) + i;
      const bool class_value = target && type_value(target) &&
        type_value(target)->kind == TYPE_CLASS;
      const bool indirect_class_value = class_value &&
        ((!function_record && ClassValueNeedsIndirect(target)) ||
         (function_record && LowParameterIsByAddress(*function_record, low_index)));
      if(indirect_class_value) {
        const string slot = new_special_slot("arg", low_type(type_value(target)));
        const string address = new_temp();
        AddInstruction(address + " = addr $" + slot);
        if(!EmitObjectTransferAt(type_value(target), address, all_arguments[i], scope, true))
          throw logic_error("no viable value argument transfer");
        operands.push_back(address);
        finish_argument_unwind(argument_temporary_mark, i);
        continue;
      }
      Value value = target && type_value(target) &&
        type_value(target)->kind == TYPE_CLASS ?
        EmitObjectValueArgument(all_arguments[i], scope, target) :
        EmitValue(all_arguments[i], scope, target);
      const bool preserve_size_expression = all_arguments[i] &&
        (all_arguments[i]->kind == "sizeof-expression" ||
         all_arguments[i]->kind == "type-trait-expression");
      if(target && value.known_constant && !preserve_size_expression &&
         is_integral_type(value.type) &&
         is_integral_type(target) &&
         (type_size(target) == type_size(value.type) ||
          type_size(target) < type_size(value.type) ||
          (!is_unsigned_type(target) && type_size(target) > type_size(value.type)))) {
        value.type = target;
        value.operand = integer_text(value.constant);
      } else if(target) value = ConvertValue(value, target, false, true);
      else if(value.type && type_value(value.type)->kind == TYPE_FUNDAMENTAL &&
              type_value(value.type)->name == "float") {
        value = ConvertValue(value, Fundamental("double"));
      } else if(target && type_value(target)->kind == TYPE_FUNDAMENTAL &&
                type_value(target)->name == "double" && value.type &&
                type_value(value.type)->name == "float") {
        value = ConvertValue(value, target);
      }
      operands.push_back(value.operand);
      finish_argument_unwind(argument_temporary_mark, i);
    }
    if(function_record)
      AppendVirtualBaseCallArguments(*function_record, operands, all_arguments, scope);
    vector<TypePtr> indirect_hidden_bases;
    if(!function_record && choice.member && !choice.static_member && choice.function) {
      // A pointer-to-member call has no collected FunctionRecord at the call
      // site, but its pointed-to member type still carries ordinary reference
      // parameters whose virtual views belong in the indirect-call ABI.
      for(size_t parameter = 0; parameter < choice.function->parameters.size(); ++parameter) {
        const TypePtr source_type = choice.function->parameters[parameter];
        const TypePtr carrier = virtual_base_carrier(source_type);
        if(!carrier || carrier->kind != TYPE_CLASS) continue;
        vector<TypePtr> bases = VirtualBaseTypes(carrier);
        if(source_type->kind == TYPE_POINTER) {
          vector<TypePtr> roots;
          for(size_t base = 0; base < bases.size(); ++base) {
            TypePtr root = base < carrier->virtual_base_roots.size() &&
              carrier->virtual_base_roots[base] ? carrier->virtual_base_roots[base] : bases[base];
            bool seen = false;
            for(size_t prior = 0; prior < roots.size(); ++prior)
              if(PA12SameType(roots[prior], root, true)) { seen = true; break; }
            if(!seen) roots.push_back(root);
          }
          bases = roots;
        }
        const size_t ordinary_operand = 1 + parameter;
        for(size_t base = 0; base < bases.size(); ++base) {
          if(ordinary_operand >= operands.size()) break;
          string complete = operands[ordinary_operand];
          if(parameter < all_arguments.size() && all_arguments[parameter]) {
            ExprInfo argument_info = Infer(all_arguments[parameter], scope);
            TypePtr actual = expression_value_type(argument_info);
            if(actual && actual->kind == TYPE_CLASS &&
               IsDerivedFrom(actual, carrier) &&
               !PA12SameType(actual, carrier, true))
              complete = EmitAddress(all_arguments[parameter], scope);
          }
          size_t offset = 0;
          if(!FindVirtualBaseOffset(carrier, bases[base], &offset)) continue;
          const string hidden = new_temp();
          AddInstruction(hidden + " = index i8 " + complete + ", " +
            integer_text(static_cast<long long>(offset)));
          operands.push_back(hidden);
          indirect_hidden_bases.push_back(bases[base]);
        }
      }
    }
    ostringstream arguments_text;
    for(size_t i = 0; i < operands.size(); ++i) {
      if(i != 0) arguments_text << ", ";
      arguments_text << operands[i];
    }
    string callee;
    ostringstream signature;
    if(choice.virtual_dispatch) {
      if(virtual_object_operand.empty()) throw logic_error("virtual call has no object operand");
      const string vptr = emit_load(virtual_object_operand,
        PointerTo(Fundamental("char")));
      string slot_address = vptr;
      if(choice.virtual_slot != 0) {
        slot_address = new_temp();
        AddInstruction(slot_address + " = index i8 " + vptr + ", " +
          integer_text(static_cast<long long>(choice.virtual_slot * 8)));
      }
      callee = emit_load(slot_address, PointerTo(Fundamental("char")));
      signature << " as (";
      const TypePtr indirect_function = function_record ? function_record->type : choice.function;
      for(size_t i = 0; indirect_function && i < indirect_function->parameters.size(); ++i) {
        if(i != 0) signature << ", ";
        signature << "%arg" << i << " : " << low_type(indirect_function->parameters[i]);
        if(type_is_reference(indirect_function->parameters[i])) signature << " [pass=reference]";
      }
      signature << ") -> " << low_type(indirect_function ? indirect_function->child : choice.function->child);
    } else if(choice.direct) {
      if(!function_record) throw logic_error("missing direct function record");
      callee = "@" + function_record->symbol;
	} else {
	  Value callee_value = EmitValue(callee_node, scope);
	  callee = callee_value.operand;
	  if(choice.member && !choice.static_member && state_ && callee_node) {
	    CPPGMAstNodePtr adjustment_node = callee_node;
	    while(adjustment_node && adjustment_node->kind == "parenthesized-expression" &&
	      adjustment_node->children.size() == 1 && adjustment_node->children[0])
	      adjustment_node = adjustment_node->children[0];
	    map<const CPPGMAstNode*, string>::const_iterator adjustment =
	      state_->member_pointer_adjustments.find(adjustment_node.get());
	    if(adjustment != state_->member_pointer_adjustments.end()) {
	      const size_t object_index = function_record && function_record->indirect_result ? 1 : 0;
	      if(object_index >= operands.size())
	        throw logic_error("member-pointer call has no receiver operand");
	      const string adjusted_object = new_temp();
	      AddInstruction(adjusted_object +
	        " = index i8 [projection=base_subobject] " +
	        operands[object_index] + ", " + adjustment->second);
	      operands[object_index] = adjusted_object;
	      arguments_text.str(string());
	      arguments_text.clear();
	      for(size_t operand = 0; operand < operands.size(); ++operand) {
	        if(operand != 0) arguments_text << ", ";
	        arguments_text << operands[operand];
	      }
	    }
	  }
	  const bool lambda_callee = callee_node &&
	    (callee_node->kind == "lambda-expression" ||
	     (callee_node->kind == "parenthesized-expression" &&
	      callee_node->children.size() == 1 && callee_node->children[0] &&
	      callee_node->children[0]->kind == "lambda-expression"));
	  if(lambda_callee && callee_value.function && callee_value.type &&
	     type_value(callee_value.type)->kind == TYPE_FUNCTION) {
	    const string decay = new_temp();
	    AddInstruction(decay + " = unary decay ptr " + callee);
	    callee = decay;
	  }
	  // A named function used through a non-type function-pointer parameter is
	  // still a function designator at this boundary.  Materialize the standard
	  // function-to-pointer conversion before the indirect call; local function
	  // references already carry their decay in EmitIdentifier and are left
	  // unchanged.
	  if(callee_node && callee_node->kind == "id-expression" &&
		callee_value.function) {
		const vector<Binding*> bindings = Lookup(callee_node->value, scope);
		bool named_function = false;
		for(size_t binding = 0; binding < bindings.size(); ++binding)
			if(bindings[binding] && bindings[binding]->kind == BIND_FUNCTION) {
				named_function = true;
				break;
			}
		if(named_function) {
			const string decay = new_temp();
			AddInstruction(decay + " = unary decay ptr " + callee);
			callee = decay;
		}
	  }
	  signature << " as (";
      const bool synthetic_member_pointer_call = choice.member &&
        !choice.static_member && !function_record;
      const size_t signature_parameters = choice.function->parameters.size() +
        (synthetic_member_pointer_call ? 1 + indirect_hidden_bases.size() : 0);
      for(size_t i = 0; i < signature_parameters; ++i) {
        if(i != 0) signature << ", ";
        const bool hidden_parameter = synthetic_member_pointer_call &&
          i >= 1 + choice.function->parameters.size();
        const TypePtr parameter = hidden_parameter ?
          PointerTo(Fundamental("char")) :
          (synthetic_member_pointer_call && i == 0 ?
            PointerTo(choice.member_pointer_owner) :
            choice.function->parameters[i - (synthetic_member_pointer_call ? 1 : 0)]);
        const bool by_address = parameter && !type_is_reference(parameter) &&
          type_value(parameter) && type_value(parameter)->kind == TYPE_CLASS &&
          !function_record && ClassValueNeedsIndirect(parameter);
        if(hidden_parameter)
          signature << "%__pvbptr" << (i - 1 - choice.function->parameters.size()) << " : ";
        else signature << "%arg" << i << " : ";
        signature <<
          (by_address ? low_type(PointerTo(type_value(parameter))) : low_type(parameter));
        if(type_is_reference(parameter)) signature << " [pass=reference]";
        else if(by_address) signature << " [pass=by_address]";
      }
      signature << ") -> " << low_type(choice.function->child);
    }
    bool reused_call_context = false;
    bool leave_reused_call_context_open = false;
    bool suppress_preserved_reference_cleanup = false;
    if(state_ && state_->constructor_unwind_active &&
       state_->constructor_unwind_call &&
       (call_argument_context_started || state_->pending_call_argument_context ||
        (state_->pending_call_unwind && state_->defer_call_unwind_completion))) {
      state_->constructor_unwind_cleanup = CaptureLiveCleanupObjects();
      state_->pending_call_argument_context = false;
      reused_call_context = true;
    } else if(state_ && state_->constructor_unwind_active &&
              !state_->constructor_unwind_call &&
              !indirect_destination.empty()) {
      // An indirect-result call materializing a class argument is itself part
      // of the enclosing argument evaluation.  Reuse that argument's EH
      // region for the call, then let the caller register the returned object
      // as a live temporary after the call completes.
      state_->constructor_unwind_cleanup = CaptureLiveCleanupObjects();
      leave_reused_call_context_open = state_->preserve_call_argument_context;
      if(!leave_reused_call_context_open)
        state_->pending_call_argument_context = false;
      reused_call_context = true;
    } else if(state_ && state_->constructor_unwind_active &&
              !state_->constructor_unwind_call && indirect_destination.empty() &&
              state_->preserve_call_argument_context &&
              state_->pending_call_argument_context) {
      // The enclosing multi-argument call is now at its actual call
      // boundary.  Reuse the region for that call and allow its normal
      // completion path below to close it after the call instruction.
      state_->constructor_unwind_cleanup = CaptureLiveCleanupObjects();
      state_->pending_call_argument_context = false;
      state_->preserve_call_argument_context = false;
      size_t managed_class_value_arguments = 0;
      size_t managed_arguments = 0;
      for(size_t argument = 0; argument < all_arguments.size(); ++argument) {
        if(!contains_managed_temporary(all_arguments[argument])) continue;
        ++managed_arguments;
        const TypePtr parameter = argument < choice.function->parameters.size() ?
          choice.function->parameters[argument] : TypePtr();
        if(parameter && !type_is_reference(parameter) && type_value(parameter) &&
           type_value(parameter)->kind == TYPE_CLASS)
          ++managed_class_value_arguments;
      }
      suppress_preserved_reference_cleanup = managed_arguments > 1 &&
        managed_class_value_arguments == managed_arguments;
      if(suppress_preserved_reference_cleanup)
        state_->constructor_unwind_cleanup.clear();
      reused_call_context = true;
    } else if(state_ && state_->constructor_unwind_active) {
      FinishConstructorUnwind(scope);
    }
    // EmitCompare/EmitConditionalValue deliberately defer completion so a
    // whole value expression remains inside one protected region.  If the
    // first operand already opened that region, a later call operand must
    // reuse its dispatch instead of nesting a second region inside it.
    const bool reuse_pending_call_unwind = state_ &&
      state_->pending_call_unwind && state_->defer_call_unwind_completion;
    if(full_expression_cleanup && state_)
      state_->defer_temporary_cleanup = previous_full_expression_defer;
    TypePtr return_type = choice.function->child;
    const TypePtr low_function = function_record ? function_record->type : choice.function;
    const string return_low = low_type(low_function->child);
    const vector<FunctionState::TemporaryObject> unwind_cleanup =
      CaptureLiveCleanupObjects();
    bool cleanup_construction_no_throw = true;
    for(size_t cleanup = 0; cleanup < unwind_cleanup.size(); ++cleanup)
      if(!unwind_cleanup[cleanup].variable &&
         !unwind_cleanup[cleanup].construction_unwind_no) {
        cleanup_construction_no_throw = false;
        break;
      }
    const bool no_throw_cleanup_boundary = selected_call_no_throw &&
      (state_ && state_->record && state_->record->destructor ?
        true : cleanup_construction_no_throw);
    bool managed_call_argument = false;
    for(size_t argument = 0; argument < all_arguments.size(); ++argument)
      if(contains_managed_temporary(all_arguments[argument])) {
        managed_call_argument = true;
        break;
      }
    const bool wrap_unwind = reused_call_context ||
      (!unwind_cleanup.empty() && !no_throw_cleanup_boundary) ||
      (!planned_unwind_result_slot.empty() && managed_call_argument);
    const string unwind_dispatch = reused_call_context ?
      state_->constructor_unwind_dispatch :
      (reuse_pending_call_unwind ? state_->pending_call_unwind_dispatch :
       (wrap_unwind ? new_label("call_unwind_dispatch") : string()));
    const string unwind_end = reused_call_context ?
      state_->constructor_unwind_end :
      (reuse_pending_call_unwind ? state_->pending_call_unwind_end :
       (wrap_unwind ? new_label("call_unwind_end") : string()));
    Value result;
    result.type = type_is_reference(return_type) ? return_type->child : return_type;
    result.lvalue = type_is_reference(return_type);
    if(return_low == "void") {
      if(wrap_unwind && !reused_call_context && !reuse_pending_call_unwind) {
        AddInstruction("eh_try ^" + unwind_dispatch);
      }
      AddInstruction("call void " + callee + "(" + arguments_text.str() + ")" + signature.str());
      const bool defer_unwind_completion = state_ &&
        state_->defer_call_unwind_completion && wrap_unwind;
      if(defer_unwind_completion) {
        if(!reuse_pending_call_unwind &&
           (!reused_call_context || !unwind_cleanup.empty())) {
          state_->pending_call_unwind = true;
          state_->pending_call_cleanup = unwind_cleanup;
          state_->pending_call_unwind_dispatch = unwind_dispatch;
          state_->pending_call_unwind_end = unwind_end;
        }
        return result;
      }
      if(!state_ || (!state_->defer_temporary_cleanup &&
                     !suppress_preserved_reference_cleanup))
        EmitTemporaryDestructors(temporary_mark, scope);
      if(reused_call_context && !leave_reused_call_context_open) {
        FinishConstructorUnwind(scope);
      } else if(wrap_unwind && !reused_call_context) {
        AddInstruction("eh_end");
        Terminate("jump ^" + unwind_end);
        AddBlock(unwind_dispatch);
        EmitCleanupObjects(unwind_cleanup, scope);
        Terminate("resume");
        AddBlock(unwind_end);
      }
      if(function_record && function_record->indirect_result)
        result.operand = indirect_result_address;
      if(suppress_preserved_reference_cleanup && state_)
        state_->temporary_objects.resize(temporary_mark);
      return result;
    }
    result.operand = new_temp();
    const bool spill_unwind_result = (wrap_unwind ||
      !planned_unwind_result_slot.empty()) &&
      (type_is_reference(return_type) || !type_value(result.type) ||
       type_value(result.type)->kind != TYPE_CLASS);
    const string unwind_result_slot = spill_unwind_result ?
      (planned_unwind_result_slot.empty() ?
       new_special_slot("call", return_low) : planned_unwind_result_slot) : string();
    if(wrap_unwind && !reused_call_context && !reuse_pending_call_unwind) {
      AddInstruction("eh_try ^" + unwind_dispatch);
    }
    AddInstruction(result.operand + " = call " + return_low + " " + callee + "(" +
      arguments_text.str() + ")" + signature.str());
    if(spill_unwind_result) {
      const TypePtr spill_type = type_is_reference(return_type) ?
        PointerTo(Fundamental("char")) : result.type;
      emit_store(spill_type, result.operand, "$" + unwind_result_slot);
      result.operand = emit_load("$" + unwind_result_slot, spill_type);
    }
    const bool defer_unwind_completion = state_ &&
      state_->defer_call_unwind_completion && wrap_unwind;
    if(defer_unwind_completion) {
      if(!reuse_pending_call_unwind &&
         (!reused_call_context || !unwind_cleanup.empty())) {
        state_->pending_call_unwind = true;
        state_->pending_call_cleanup = unwind_cleanup;
        state_->pending_call_unwind_dispatch = unwind_dispatch;
        state_->pending_call_unwind_end = unwind_end;
      }
      return result;
    }
    if(!state_ || (!state_->defer_temporary_cleanup &&
                   !suppress_preserved_reference_cleanup))
      EmitTemporaryDestructors(temporary_mark, scope);
    if(reused_call_context && !leave_reused_call_context_open) {
      FinishConstructorUnwind(scope);
    } else if(wrap_unwind && !reused_call_context) {
      AddInstruction("eh_end");
      Terminate("jump ^" + unwind_end);
      AddBlock(unwind_dispatch);
      EmitCleanupObjects(unwind_cleanup, scope);
      Terminate("resume");
      AddBlock(unwind_end);
    }
    if(suppress_preserved_reference_cleanup && state_)
      state_->temporary_objects.resize(temporary_mark);
    return result;
  }

void PA14Lowerer::FinishPendingCallUnwind(Scope* scope)
{
    if(!state_ || !state_->pending_call_unwind) return;
    // A deferred call can reuse the active typed constructor context.  In
    // that case the context owns the shared dispatch (and may intentionally
    // have no end label); finish it through the same state machine instead
    // of emitting a second, label-less pending region.
    if(state_->constructor_unwind_active) {
      state_->pending_call_unwind = false;
      state_->pending_call_cleanup.clear();
      state_->pending_call_unwind_dispatch.clear();
      state_->pending_call_unwind_end.clear();
      FinishConstructorUnwind(scope);
      return;
    }
    const vector<FunctionState::TemporaryObject> cleanup =
      state_->pending_call_cleanup;
    const string unwind_dispatch = state_->pending_call_unwind_dispatch;
    const string unwind_end = state_->pending_call_unwind_end;
    state_->pending_call_unwind = false;
    state_->pending_call_cleanup.clear();
    state_->pending_call_unwind_dispatch.clear();
    state_->pending_call_unwind_end.clear();
    AddInstruction("eh_end");
    Terminate("jump ^" + unwind_end);
    AddBlock(unwind_dispatch);
    EmitCleanupObjects(cleanup, scope);
    Terminate("resume");
    AddBlock(unwind_end);
  }

} // namespace cppgm_pa14_lowering
