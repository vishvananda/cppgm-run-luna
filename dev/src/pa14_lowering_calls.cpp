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
        const string slot = new_special_slot("arg", low_type(result_type));
        const string address = new_temp();
        AddInstruction(address + " = addr $" + slot);
        (void)EmitChosenCall(choice, CPPGMAstNodePtr(), operator_arguments,
          scope, address);
        RegisterTemporaryObject(result_type, address);
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
         IsDerivedFrom(constructed, type_value(referred)))
        return AdjustBaseAddress(
          EmitTemporaryObjectAddress(node, scope, "tmpobj"),
          constructed, referred);
      if(constructed && referred && constructed->kind == TYPE_CLASS &&
         type_value(referred)->kind == TYPE_CLASS &&
         !PA12SameType(constructed, type_value(referred), true) &&
         !IsDerivedFrom(constructed, type_value(referred))) {
        const string slot = new_special_slot("arg", low_type(type_value(referred)));
        const string address = new_temp();
        AddInstruction(address + " = addr $" + slot);
        vector<CPPGMAstNodePtr> constructor_arguments;
        constructor_arguments.push_back(node);
        if(EmitConstructorAt(type_value(referred), address, constructor_arguments,
                             scope, false)) {
          RegisterTemporaryObject(type_value(referred), address);
          return address;
        }
      }
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
        // A class-valued conversion used as a reference argument needs the
        // same temporary category as an ordinary constructor argument.  The
        // generic conversion helper creates a return-object slot, but this
        // path is an argument boundary and must materialize the result in an
        // arg slot before the destination copy/move constructor sees it.
        const TypePtr conversion_function = function_target_type(conversion->type);
        const TypePtr conversion_result = conversion_function ?
          conversion_function->child : TypePtr();
        Value converted;
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
          const string address = new_temp();
          AddInstruction(address + " = addr $" + slot);
          converted = EmitChosenCall(choice, CPPGMAstNodePtr(),
            vector<CPPGMAstNodePtr>(), scope, address);
        }
        if(converted.operand.empty()) return converted.operand;
        if(converted.lvalue) return converted.operand;
        // A class-valued conversion function returns through the ABI's
        // indirect-result slot.  That slot is the temporary bound to the
        // reference; falling through to EmitConstructorAt would retry the
        // same conversion as a converting constructor indefinitely.
        if(converted.type && type_value(converted.type) &&
           type_value(converted.type)->kind == TYPE_CLASS) {
          RegisterTemporaryObject(type_value(converted.type), converted.operand);
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
      const string slot = new_special_slot(template_context || operator_result ? "arg" : "tmpobj",
        low_type(source_type));
      const string address = new_temp();
      AddInstruction(address + " = addr $" + slot);
      Value value = EmitValue(node, scope);
      AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(source_type))) +
        "x" + integer_text(static_cast<long long>(type_alignment(source_type))) +
        " " + value.operand + ", " + address);
      RegisterTemporaryObject(source_type, address);
      return PA12SameType(source_type, referred, true) ? address :
        AdjustBaseAddress(address, source_type, type_value(referred));
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
    if(function_record) {
      // Calls emitted while lowering a materialized generated body can reach
      // a generated free helper after ordinary root-demand collection has
      // finished.  Demand is a property of the emitted call, not of the
      // declaration's template-instantiation bit; mark the selected record
      // here so unused SFINAE/dependent helpers remain un-emitted.
      function_record->needed = true;
      if(function_record->member) {
        FunctionRecord* base_entry = BaseEntryFor(function_record);
        if(base_entry) base_entry->needed = true;
      }
    }
    string indirect_result_address;
    string virtual_object_operand;
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
	      } else if(choice.object->kind == "cast-expression" &&
	                choice.object->children.size() > 1 && object_type &&
	                object_type->kind == TYPE_CLASS &&
	                type_is_reference(analyzer_.TypeFromTypeId(
	                  choice.object->children[0], scope))) {
	        // A reference cast denotes the selected subobject; materializing its
	        // class value here would copy the most-derived object and lose the
	        // typed base projection used by a subsequent member call.
	        object_operand = EmitAddress(choice.object, scope);
	      } else if(choice.object->kind == "call-expression" &&
                !choice.object->children.empty() &&
                ConstructorObjectType(choice.object->children[0], scope)) {
        object_operand = EmitTemporaryObjectAddress(choice.object, scope, "tmpobj");
      } else if(choice.object->kind == "call-expression" && object_type &&
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
        choice.binding ? choice.binding->member_owner : TypePtr(),
        choice.project_base_path);
      operands.push_back(object_operand);
      if(choice.virtual_dispatch) virtual_object_operand = object_operand;
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
      // An untyped variadic argument that is a direct class construction still
      // has an object ABI.  Materialize the temporary and pass its address;
      // loading it as a value loses the constructor call and disagrees with
      // the representation used by the corresponding class parameter path.
      if(!target && all_arguments[i] && all_arguments[i]->kind == "call-expression" &&
         !all_arguments[i]->children.empty()) {
        TypePtr temporary_type = ConstructorObjectType(all_arguments[i]->children[0], scope);
        if(temporary_type && temporary_type->kind == TYPE_CLASS) {
          operands.push_back(EmitTemporaryObjectAddress(all_arguments[i], scope, "arg"));
          continue;
        }
      }
      if(target && type_is_reference(target)) {
        operands.push_back(EmitReferenceArgument(all_arguments[i], scope, target));
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
         (type_size(target) < type_size(value.type) ||
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
      signature << " as (";
      for(size_t i = 0; i < choice.function->parameters.size(); ++i) {
        if(i != 0) signature << ", ";
        const TypePtr parameter = choice.function->parameters[i];
        const bool by_address = parameter && !type_is_reference(parameter) &&
          type_value(parameter) && type_value(parameter)->kind == TYPE_CLASS &&
          !function_record && ClassValueNeedsIndirect(parameter);
        signature << "%arg" << i << " : " <<
          (by_address ? low_type(PointerTo(type_value(parameter))) : low_type(parameter));
        if(type_is_reference(parameter)) signature << " [pass=reference]";
        else if(by_address) signature << " [pass=by_address]";
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
