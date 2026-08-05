#include "pa14_lowering.h"

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace cppgm_pa14_lowering {

namespace {

bool HasIntegralTemplateArgument(const TypePtr& raw_type)
{
  const TypePtr type = type_value(raw_type);
  if(!type || !type->template_specialization) return false;
  for(size_t i = 0; i < type->template_arguments.size(); ++i) {
    PA19IntegralValue value;
    if(PA19ParseInteger(type->template_arguments[i], &value) ||
       PA19DecodeCharacter(type->template_arguments[i], &value)) return true;
    const string argument = PA19Compact(type->template_arguments[i]);
    if(argument == "true" || argument == "false") return true;
  }
  return false;
}

} // namespace

bool PA14Lowerer::EmitObjectTransferAt(const TypePtr& raw_target,
                                       const string& destination,
                                       const CPPGMAstNodePtr& source,
                                       Scope* scope, bool allow_explicit,
                                       bool implicit_return_move,
                                       bool full_expression_cleanup)
{
    TypePtr target = type_value(raw_target);
    if(!target || target->kind != TYPE_CLASS || !source) return false;
    bool handled = false;
    if(EmitObjectTransferSpecial(target, destination, source, scope,
                                 allow_explicit, full_expression_cleanup,
                                 &handled)) return true;
    if(handled) return false;
    if(EmitObjectTransferCall(target, destination, source, scope,
                              allow_explicit, full_expression_cleanup,
                              &handled)) return true;
    if(handled) return false;
    CPPGMAstNodePtr lambda_source = source;
    while(lambda_source && lambda_source->kind == "parenthesized-expression" &&
          lambda_source->children.size() == 1 && lambda_source->children[0])
      lambda_source = lambda_source->children[0];
    ExprInfo source_info;
    if(lambda_source && lambda_source->kind == "lambda-expression" &&
       LambdaClosureType(lambda_source))
      source_info = Infer(lambda_source, scope, LambdaClosureType(lambda_source));
    else
      source_info = Infer(source, scope);
    TypePtr source_type = expression_value_type(source_info);
    if(EmitObjectTransferConverted(target, destination, source, scope,
                                   source_info, source_type,
                                   implicit_return_move, full_expression_cleanup,
                                   &handled)) return true;
    if(handled) return false;
    if(EmitObjectTransferDerived(target, destination, source, scope,
                                 source_info, source_type, allow_explicit,
                                 full_expression_cleanup)) return true;
    return EmitObjectTransferSame(target, destination, source, scope,
                                  source_info, source_type, allow_explicit,
                                  implicit_return_move, full_expression_cleanup);
  }

bool PA14Lowerer::EmitObjectTransferSpecial(const TypePtr& target,
                                            const string& destination,
                                            const CPPGMAstNodePtr& source,
                                            Scope* scope, bool allow_explicit,
                                            bool full_expression_cleanup,
                                            bool* handled)
{
    *handled = false;
    CPPGMAstNodePtr lambda_source = source;
    while(lambda_source && lambda_source->kind == "parenthesized-expression" &&
          lambda_source->children.size() == 1 && lambda_source->children[0])
      lambda_source = lambda_source->children[0];
    if(lambda_source && lambda_source->kind == "lambda-expression") {
      const TypePtr closure = LambdaClosureType(lambda_source);
      if(closure && PA12SameType(closure, target, true)) {
        *handled = true;
        InitializeLambdaClosureAt(closure, destination, lambda_source, scope);
        return true;
      }
    }
    if(source->kind == "braced-init-list") {
      *handled = true;
      const vector<CPPGMAstNodePtr> arguments = source->children;
      // Let the constructor path append omitted aggregate members without
      // turning a later empty-list probe into a second default construction.
      const bool allow_aggregate = true;
      return EmitConstructorAt(target, destination, arguments, scope,
        allow_explicit, false, allow_aggregate, false, false,
        full_expression_cleanup);
    }
    if(source->kind == "conditional-expression") {
      ExprInfo conditional_info = Infer(source, scope);
      TypePtr conditional_type = expression_value_type(conditional_info);
      if(conditional_type && conditional_type->kind == TYPE_CLASS &&
         (PA12SameType(conditional_type, target, true) ||
          IsDerivedFrom(conditional_type, target))) {
        *handled = true;
        const string then_label = new_label("condobj_then");
        const string else_label = new_label("condobj_else");
        const string end_label = new_label("condobj_end");
        EmitCondition(source->children[0], scope, then_label, else_label);
        AddBlock(then_label);
        if(!EmitObjectTransferAt(target, destination, source->children[1], scope,
                                 allow_explicit, false, full_expression_cleanup)) return false;
        if(!state_->current->terminated) Terminate("jump ^" + end_label);
        AddBlock(else_label);
        if(!EmitObjectTransferAt(target, destination, source->children[2], scope,
                                 allow_explicit, false, full_expression_cleanup)) return false;
        if(!state_->current->terminated) Terminate("jump ^" + end_label);
        AddBlock(end_label);
        return true;
      }
    }
    if(source->kind == "cast-expression" && source->children.size() > 1) {
      TypePtr cast_type = analyzer_.TypeFromTypeId(source->children[0], scope);
      if(cast_type && type_value(cast_type) &&
         type_value(cast_type)->kind == TYPE_CLASS &&
         PA12SameType(type_value(cast_type), target, true)) {
        *handled = true;
        vector<CPPGMAstNodePtr> arguments;
        const bool move = cast_type->kind == TYPE_RVALUE_REFERENCE;
        if(type_is_reference(cast_type)) {
          FunctionRecord* value_member = EnsureImplicitCopyConstructor(target, move);
          if(value_member && value_member->deleted) return false;
          if(PA12SameType(type_value(cast_type), target, true) &&
             IsTrivialValueStorage(target) &&
             !(move && ClassHasDeclaredMoveMember(target))) {
            const string source_address = EmitAddress(source->children[1], scope);
            AddInstruction("copyobj " +
              integer_text(static_cast<long long>(type_size(target))) + "x" +
              integer_text(static_cast<long long>(type_alignment(target))) + " " +
              source_address + ", " + destination);
            return true;
          }
        }
        arguments.push_back(type_is_reference(cast_type) ? source : source->children[1]);
        return EmitConstructorAt(target, destination, arguments, scope, true,
          false, false, false, false, full_expression_cleanup);
      }
    }
    return false;
}

bool PA14Lowerer::EmitObjectTransferCall(const TypePtr& target,
                                         const string& destination,
                                         const CPPGMAstNodePtr& source,
                                         Scope* scope, bool allow_explicit,
                                         bool full_expression_cleanup,
                                         bool* handled)
{
    *handled = false;
    CPPGMAstNodePtr direct_source = source;
    while(direct_source && direct_source->kind == "parenthesized-expression" &&
          direct_source->children.size() == 1 && direct_source->children[0])
      direct_source = direct_source->children[0];
    TypePtr constructed = direct_source && direct_source->kind == "call-expression" &&
      !direct_source->children.empty() ?
        ConstructorObjectType(direct_source->children[0], scope) : TypePtr();
    if(constructed && PA12SameType(constructed, target, true)) {
      *handled = true;
      CPPGMAstNodePtr argument_list = source->children.size() > 1 ?
        source->children[1] : CPPGMAstNodePtr();
      vector<CPPGMAstNodePtr> arguments = argument_list ? argument_list->children :
        vector<CPPGMAstNodePtr>();
      if(source->value == "braced-construction" && arguments.size() == 1 &&
         arguments[0] && arguments[0]->kind == "braced-init-list")
        arguments = arguments[0]->children;
      if(arguments.empty())
        CollectImplicitConstructor(constructed, constructed->owned_scope, true);
      const bool allow_aggregate = !arguments.empty() &&
        EnsureAggregateConstructor(constructed);
      const bool value_initialization = arguments.empty() && HasConstructor(constructed) &&
        (source->value == "braced-construction" || HasIntegralTemplateArgument(constructed));
      return EmitConstructorAt(target, destination, arguments, scope, allow_explicit,
        false, allow_aggregate, false, value_initialization,
        full_expression_cleanup);
    }
    if(!constructed && direct_source && direct_source->kind == "call-expression") {
      CallChoice choice = ChooseCall(direct_source, scope);
      FunctionRecord* function = choice.binding ? RecordForBinding(choice.binding) : 0;
      TypePtr result_type = expression_value_type(Infer(direct_source, scope));
      if(function && function->indirect_result && result_type &&
         PA12SameType(result_type, target, true)) {
        *handled = true;
        const bool constructor_context = state_ &&
          !state_->constructor_unwind_active &&
          !state_->suppress_constructor_unwind && result_type->polymorphic &&
          HasDestructor(result_type);
        if(constructor_context)
          BeginConstructorUnwind(vector<FunctionState::TemporaryObject>(), false);
        CPPGMAstNodePtr argument_list = direct_source->children.size() > 1 ?
          direct_source->children[1] : CPPGMAstNodePtr();
        vector<CPPGMAstNodePtr> arguments = argument_list ? argument_list->children :
          vector<CPPGMAstNodePtr>();
        EmitChosenCall(choice, direct_source->children[0], arguments, scope, destination,
          full_expression_cleanup);
        if(constructor_context && state_ && state_->constructor_unwind_active)
          FinishConstructorUnwind(scope);
        RegisterTemporaryObject(result_type, destination);
        return true;
      }
    }
    if(direct_source && direct_source->kind == "binary-expression") {
      vector<CPPGMAstNodePtr> operator_arguments;
      operator_arguments.push_back(direct_source->children.empty() ?
        CPPGMAstNodePtr() : direct_source->children[0]);
      if(direct_source->children.size() > 1)
        operator_arguments.push_back(direct_source->children[1]);
      CallChoice choice = ChooseOperatorCall(
        OperatorFunctionName(PA12Operator(direct_source->value)),
        operator_arguments, scope);
      FunctionRecord* function = choice.binding ? RecordForBinding(choice.binding) : 0;
      TypePtr result_type = function && function->source_type ?
        type_value(function->source_type->child) :
        (choice.function ? type_value(choice.function->child) : TypePtr());
      if(function && function->indirect_result && result_type &&
         result_type->polymorphic && PA12SameType(result_type, target, true)) {
        *handled = true;
        const bool constructor_context = state_ &&
          !state_->constructor_unwind_active &&
          !state_->suppress_constructor_unwind && result_type->polymorphic &&
          HasDestructor(result_type);
        if(constructor_context)
          BeginConstructorUnwind(vector<FunctionState::TemporaryObject>(), false);
        EmitChosenCall(choice, CPPGMAstNodePtr(), operator_arguments, scope,
          destination, full_expression_cleanup);
        if(constructor_context && state_ && state_->constructor_unwind_active)
          FinishConstructorUnwind(scope);
        RegisterTemporaryObject(result_type, destination);
        return true;
      }
    }
    return false;
}

bool PA14Lowerer::EmitObjectTransferConverted(const TypePtr& target,
                                              const string& destination,
                                              const CPPGMAstNodePtr& source,
                                              Scope* scope,
                                              const ExprInfo& source_info,
                                              const TypePtr& source_type,
                                              bool implicit_return_move,
                                              bool full_expression_cleanup,
                                              bool* handled)
{
    *handled = false;
    if(!source_type || source_type->kind != TYPE_CLASS) {
      *handled = true;
      vector<CPPGMAstNodePtr> arguments;
      arguments.push_back(source);
      return EmitConstructorAt(target, destination, arguments, scope, true,
        false, false, false, false, full_expression_cleanup);
    }
    // A class-valued conversion is still a constructor boundary in C++11:
    // materialize the conversion result before initializing the destination.
    vector<CPPGMAstNodePtr> constructor_arguments;
    constructor_arguments.push_back(source);
    if(!PA12SameType(source_type, target, true)) {
      bool constructed = false;
      try {
        constructed = EmitConstructorAt(target, destination,
          constructor_arguments, scope, false, false, false, false, false,
          full_expression_cleanup);
      } catch(const PA14NoViableConstructor&) {
        // A failed destination candidate permits the conversion-function
        // candidate.  Ambiguity and other lowering failures remain errors.
      }
      if(constructed) return true;
    }
    Binding* conversion = FindConversionOperator(source_type, target, true);
    if(conversion) {
      *handled = true;
      FunctionRecord* conversion_record = RecordForBinding(conversion);
      CallChoice choice;
      choice.binding = conversion;
      choice.function = function_target_type(conversion->type);
      choice.object = source;
      choice.direct = true;
      choice.member = true;
      choice.static_member = false;
      choice.conversion = true;
      const Value converted = EmitChosenCall(choice, CPPGMAstNodePtr(),
        vector<CPPGMAstNodePtr>(), scope, destination, full_expression_cleanup);
      if(conversion_record && conversion_record->indirect_result) return true;
      if(converted.operand.empty()) return false;
      AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(target))) +
        "x" + integer_text(static_cast<long long>(type_alignment(target))) +
        " " + converted.operand + ", " + destination);
      return true;
    }
    const bool same_type = PA12SameType(source_type, target, true);
    const bool template_context = type_value(target)->template_specialization ||
      (scope && scope->owner_type &&
       type_value(scope->owner_type)->template_specialization) ||
      (state_ && state_->record && state_->record->template_instantiation) ||
      (state_ && state_->record && state_->record->member_owner &&
       state_->record->member_owner->template_specialization);
    if(same_type && source_info.category == "lvalue" && template_context &&
       IsEmptyBaseStorage(target) && IsTrivialValueStorage(target)) {
      *handled = true;
      VariablePlan* source_local = source->kind == "id-expression" ?
        FindLocalPlan(source->value) : 0;
      const bool materialized_empty_template_reference = source_local &&
        source_local->parameter && target->template_specialization &&
        HasConstructor(target) && !TemplatePrimaryHasNonstaticMemberFunction(target) &&
        !HasUserProvidedConstructor(target);
      const bool static_member_reference = source_info.binding &&
        source_info.binding->is_member && source_info.binding->is_static;
      if((source_local && (implicit_return_move ||
           (source_local->parameter && !materialized_empty_template_reference))) ||
         (source->kind == "binary-expression" && PA12Operator(source->value) == ",") ||
         (source->kind == "call-expression") || static_member_reference)
        (void)EmitAddress(source, scope);
      return true;
    }
    return false;
}

bool PA14Lowerer::EmitObjectTransferDerived(const TypePtr& target,
                                            const string& destination,
                                            const CPPGMAstNodePtr& source,
                                            Scope* scope,
                                            const ExprInfo& source_info,
                                            const TypePtr& source_type,
                                            bool allow_explicit,
                                            bool full_expression_cleanup)
{
    if(!source_type || source_type->kind != TYPE_CLASS ||
       !IsDerivedFrom(source_type, target)) return false;
    FunctionRecord* target_copy = FindValueMember(target, false, false);
    bool has_single_argument_conversion = false;
    const vector<Binding*> target_constructors =
      MemberBindings(target, LastComponent(target->name));
    for(size_t i = 0; i < target_constructors.size(); ++i) {
      FunctionRecord* candidate = RecordForBinding(target_constructors[i]);
      TypePtr candidate_type = function_target_type(target_constructors[i]->type);
      if(candidate && candidate->constructor && !candidate->copy_constructor &&
         !candidate->move_constructor && candidate_type &&
         candidate_type->parameters.size() == 1) {
        has_single_argument_conversion = true;
        break;
      }
    }
    if(IsTrivialValueStorage(target) && target_copy && !target_copy->deleted &&
       (target_copy->defaulted || target_copy->implicit_constructor ||
        target_copy->synthesized_value_member) && !has_single_argument_conversion &&
       (source_info.category == "lvalue" || source_info.category == "xvalue")) {
      string source_address = AdjustBaseAddress(EmitAddress(source, scope),
        source_type, target);
      AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(target))) +
        "x" + integer_text(static_cast<long long>(type_alignment(target))) + " " +
        source_address + ", " + destination);
      return true;
    }
    bool empty_target = DirectBaseTypes(target).empty();
    for(size_t member_index = 0; member_index < target->class_members.size();
        ++member_index)
      if(!target->class_members[member_index].is_static &&
         !target->class_members[member_index].name.empty()) {
        empty_target = false;
        break;
      }
    if(empty_target) return true;
    vector<CPPGMAstNodePtr> constructor_arguments;
    constructor_arguments.push_back(source);
    if(EmitConstructorAt(target, destination, constructor_arguments, scope,
                         allow_explicit, false, false, false, false,
                         full_expression_cleanup)) return true;
    string source_address;
    if(source_info.category == "lvalue" || source_info.category == "xvalue")
      source_address = EmitAddress(source, scope);
    else if(source->kind == "call-expression" &&
            ConstructorObjectType(source->children.empty() ?
              CPPGMAstNodePtr() : source->children[0], scope))
      source_address = EmitTemporaryObjectAddress(source, scope, "tmpobj");
    else
      source_address = EmitAddress(source, scope);
    source_address = AdjustBaseAddress(source_address, source_type, target);
    if(IsTrivialValueStorage(target)) {
      AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(target))) +
        "x" + integer_text(static_cast<long long>(type_alignment(target))) + " " +
        source_address + ", " + destination);
      return true;
    }
    return false;
}

bool PA14Lowerer::EmitObjectTransferSame(const TypePtr& target,
                                         const string& destination,
                                         const CPPGMAstNodePtr& source,
                                         Scope* scope,
                                         const ExprInfo& source_info,
                                         const TypePtr& source_type,
                                         bool allow_explicit,
                                         bool implicit_return_move,
                                         bool full_expression_cleanup)
{
    const bool same_type = PA12SameType(source_type, target, true);
    const bool move = source_info.category == "xvalue" || implicit_return_move;
    if(same_type && ValueOperationDeleted(target, move, false)) return false;
    if(same_type && IsTrivialValueStorage(target) &&
       !(move && ClassHasDeclaredMoveMember(target))) {
      FunctionRecord* copy_member = FindValueMember(target, false, false);
      if(copy_member && source->kind == "unary-expression") {
        MarkFunctionNeeded(copy_member);
        FunctionRecord* base_entry = BaseEntryFor(copy_member);
        if(base_entry) MarkFunctionNeeded(base_entry);
      }
      string source_operand;
      if(source_info.category == "lvalue" || source_info.category == "xvalue")
        source_operand = EmitAddress(source, scope);
      else source_operand = EmitValue(source, scope, target).operand;
      AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(target))) +
        "x" + integer_text(static_cast<long long>(type_alignment(target))) + " " +
        source_operand + ", " + destination);
      return true;
    }
    if(same_type) {
      FunctionRecord* value_member = EnsureImplicitCopyConstructor(target, move);
      if(value_member && value_member->deleted) return false;
    }
    vector<CPPGMAstNodePtr> arguments;
    arguments.push_back(source);
    if(!EmitConstructorAt(target, destination, arguments, scope, allow_explicit,
                          false, false, implicit_return_move, false,
                          full_expression_cleanup)) {
      if(same_type && !move) {
        FunctionRecord* value_member = EnsureImplicitCopyConstructor(target, false);
        if(value_member && !value_member->deleted)
          return EmitConstructorAt(target, destination, arguments, scope, allow_explicit,
            false, false, false, false, full_expression_cleanup);
      }
      return false;
    }
    return true;
}


} // namespace cppgm_pa14_lowering
