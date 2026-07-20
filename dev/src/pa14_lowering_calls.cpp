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

string PA14Lowerer::EmitReferenceArgument(const CPPGMAstNodePtr& node, Scope* scope,
                               const TypePtr& target)
{
    TypePtr referred = target->child;
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
        RegisterTemporaryObject(call_type, address);
        return address;
      }
    }
    if(node && node->kind == "cast-expression" && node->children.size() > 1 &&
       target->kind == TYPE_RVALUE_REFERENCE &&
       ConstructorObjectType(node->children[1]->children.empty() ?
         CPPGMAstNodePtr() : node->children[1]->children[0], scope) &&
       node->children[1]->kind == "call-expression")
      return EmitTemporaryObjectAddress(node->children[1], scope, "refcall");
    if(node && node->kind == "braced-init-list" && referred &&
       type_value(referred)->kind == TYPE_CLASS) {
      TypePtr object_type = type_value(referred);
      const string slot = new_special_slot("arg", low_type(object_type));
      const string address = new_temp();
      AddInstruction(address + " = addr $" + slot);
      const vector<CPPGMAstNodePtr> arguments = node->children;
      if(EmitConstructorAt(object_type, address, arguments, scope, false,
                           false, true)) {
        RegisterTemporaryObject(object_type, address);
        return address;
      }
    }
    if(node && node->kind == "call-expression" && !node->children.empty()) {
      TypePtr constructed = ConstructorObjectType(node->children[0], scope);
      if(constructed && referred && constructed->kind == TYPE_CLASS &&
         type_value(referred)->kind == TYPE_CLASS &&
         !PA12SameType(constructed, type_value(referred), true) &&
         IsDerivedFrom(constructed, type_value(referred)))
        return AdjustBaseAddress(
          EmitTemporaryObjectAddress(node, scope, "tmpobj"),
          constructed, referred);
      if(constructed) return EmitTemporaryObjectAddress(node, scope, "arg");
    }
    if(node && node->kind == "call-expression" && !node->children.empty()) {
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
        (void)EmitChosenCall(choice, node->children[0], arguments, scope, address);
        RegisterTemporaryObject(result_type, address);
        if(!PA12SameType(result_type, type_value(referred), true) &&
           IsDerivedFrom(result_type, type_value(referred)))
          return AdjustBaseAddress(address, result_type, type_value(referred));
        return address;
      }
    }
    ExprInfo source = Infer(node, scope);
    TypePtr source_type = expression_value_type(source);
    const bool direct_address = source.category == "lvalue" &&
      PA12SameType(source_type, referred, true) &&
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
        Value converted = EmitConversionOperator(node, scope, referred, false);
        if(converted.lvalue) return converted.operand;
      }
    }
    if(referred && referred->kind == TYPE_CLASS && source.category == "prvalue" &&
       PA12SameType(source_type, referred, true)) {
      // An indirect class result is already materialized by the call.  Bind
      // the reference to that result instead of recursively invoking a copy
      // constructor on the same call expression.
      Value value = EmitValue(node, scope, referred);
      FunctionRecord* source_record = source.binding ? RecordForBinding(source.binding) : 0;
      if(source_record && source_record->indirect_result) {
        RegisterTemporaryObject(referred, value.operand);
        return value.operand;
      }
      const string slot = new_special_slot("refarg", low_type(referred));
      emit_store(referred, value.operand, "$" + slot);
      const string address = new_temp();
      AddInstruction(address + " = addr $" + slot);
      RegisterTemporaryObject(referred, address);
      return address;
    }
    if(referred && referred->kind == TYPE_CLASS) {
      const string slot = new_special_slot("arg", low_type(referred));
      const string address = new_temp();
      AddInstruction(address + " = addr $" + slot);
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
    const string& indirect_destination)
{
    FunctionRecord* function_record = 0;
    const size_t temporary_mark = state_ ? state_->temporary_objects.size() : 0;
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
    if(function_record && function_record->member) {
      function_record->needed = true;
      FunctionRecord* base_entry = BaseEntryFor(function_record);
      if(base_entry) base_entry->needed = true;
    }
    string indirect_result_address;
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
      string object_operand;
      ExprInfo object_info = Infer(choice.object, scope);
      TypePtr object_type = expression_value_type(object_info);
      if(object_type && object_type->kind == TYPE_POINTER) {
        object_operand = EmitValue(choice.object, scope).operand;
        object_type = type_value(object_type->child);
      } else if(object_info.category == "lvalue" ||
                (choice.object->kind == "keyword-literal" &&
                 PA12Operator(choice.object->value) == "this")) {
        object_operand = EmitAddress(choice.object, scope);
      } else if(choice.object->kind == "call-expression" &&
                !choice.object->children.empty() &&
                ConstructorObjectType(choice.object->children[0], scope)) {
        object_operand = EmitTemporaryObjectAddress(choice.object, scope, "tmpobj");
      } else if(choice.object->kind == "call-expression" && object_type &&
                object_type->kind == TYPE_CLASS) {
        // A class-valued call is a prvalue object, not a scalar to spill into
        // an arbitrary member-call slot.  Materialize it through the common
        // temporary path so the object ABI and lifetime stay consistent.
        const string slot = new_special_slot("tmpobj", low_type(object_type));
        object_operand = new_temp();
        AddInstruction(object_operand + " = addr $" + slot);
        Value object_value = EmitValue(choice.object, scope);
        AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(object_type))) +
          "x" + integer_text(static_cast<long long>(type_alignment(object_type))) + " " +
          object_value.operand + ", " + object_operand);
      } else if(object_type && object_type->kind == TYPE_CLASS &&
                object_info.category != "lvalue") {
        const string slot = new_special_slot("tmpobj", low_type(object_type));
        object_operand = new_temp();
        AddInstruction(object_operand + " = addr $" + slot);
        Value object_value = EmitValue(choice.object, scope);
        AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(object_type))) +
          "x" + integer_text(static_cast<long long>(type_alignment(object_type))) + " " +
          object_value.operand + ", " + object_operand);
      } else if(choice.conversion && object_info.category == "lvalue" &&
                object_type && object_type->kind == TYPE_CLASS) {
        object_operand = EmitAddress(choice.object, scope);
      } else {
        Value object_value = EmitValue(choice.object, scope);
        const string slot = new_special_slot("object", low_type(object_type));
        emit_store(object_type, object_value.operand, "$" + slot);
        object_operand = new_temp();
        AddInstruction(object_operand + " = addr $" + slot);
      }
      object_operand = AdjustBaseAddress(object_operand, object_type,
        choice.binding ? choice.binding->member_owner : TypePtr());
      operands.push_back(object_operand);
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
    for(size_t i = 0; i < all_arguments.size(); ++i) {
      TypePtr target = i < choice.function->parameters.size() ? choice.function->parameters[i] : TypePtr();
      if(target && type_is_reference(target)) {
        operands.push_back(EmitReferenceArgument(all_arguments[i], scope, target));
        continue;
      }
      const size_t low_index = (function_record && function_record->indirect_result ? 1 : 0) +
        (choice.member && !choice.static_member ? 1 : 0) + i;
      if(function_record && target && type_value(target) &&
         type_value(target)->kind == TYPE_CLASS &&
         LowParameterIsByAddress(*function_record, low_index)) {
        const string slot = new_special_slot("arg", low_type(type_value(target)));
        const string address = new_temp();
        AddInstruction(address + " = addr $" + slot);
        if(!EmitObjectTransferAt(type_value(target), address, all_arguments[i], scope, true))
          throw logic_error("no viable value argument transfer");
        operands.push_back(address);
        continue;
      }
      Value value = target && type_value(target) &&
        type_value(target)->kind == TYPE_CLASS ?
        EmitObjectValueArgument(all_arguments[i], scope, target) :
        EmitValue(all_arguments[i], scope, target);
      if(target && value.known_constant && is_integral_type(value.type) &&
         is_integral_type(target) && type_size(target) > type_size(value.type) &&
         !is_unsigned_type(target)) {
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
    }
    ostringstream arguments_text;
    for(size_t i = 0; i < operands.size(); ++i) {
      if(i != 0) arguments_text << ", ";
      arguments_text << operands[i];
    }
    string callee;
    ostringstream signature;
    if(choice.direct) {
      if(!function_record) throw logic_error("missing direct function record");
      callee = "@" + function_record->symbol;
    } else {
      Value callee_value = EmitValue(callee_node, scope);
      callee = callee_value.operand;
      signature << " as (";
      for(size_t i = 0; i < choice.function->parameters.size(); ++i) {
        if(i != 0) signature << ", ";
        signature << "%arg" << i << " : " << low_type(choice.function->parameters[i]);
        if(type_is_reference(choice.function->parameters[i])) signature << " [pass=reference]";
      }
      signature << ") -> " << low_type(choice.function->child);
    }
    TypePtr return_type = choice.function->child;
    const TypePtr low_function = function_record ? function_record->type : choice.function;
    const string return_low = low_type(low_function->child);
    Value result;
    result.type = type_is_reference(return_type) ? return_type->child : return_type;
    result.lvalue = type_is_reference(return_type);
    if(return_low == "void") {
      AddInstruction("call void " + callee + "(" + arguments_text.str() + ")" + signature.str());
      EmitTemporaryDestructors(temporary_mark, scope);
      if(function_record && function_record->indirect_result)
        result.operand = indirect_result_address;
      return result;
    }
    result.operand = new_temp();
    AddInstruction(result.operand + " = call " + return_low + " " + callee + "(" +
      arguments_text.str() + ")" + signature.str());
    EmitTemporaryDestructors(temporary_mark, scope);
    return result;
  }

} // namespace cppgm_pa14_lowering
