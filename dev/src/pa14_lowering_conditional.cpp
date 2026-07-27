#include "pa14_lowering.h"

using namespace std;

namespace cppgm_pa14_lowering {

PA14Lowerer::ExprInfo PA14Lowerer::InferConditional(
    const CPPGMAstNodePtr& node, Scope* scope)
{
    ExprInfo result;
    ExprInfo when_true = Infer(node->children[1], scope);
    ExprInfo when_false = Infer(node->children[2], scope);
    if(when_true.null_pointer_constant && expression_value_type(when_false) &&
       expression_value_type(when_false)->kind == TYPE_POINTER)
      result.type = expression_value_type(when_false);
    else if(when_false.null_pointer_constant && expression_value_type(when_true) &&
            expression_value_type(when_true)->kind == TYPE_POINTER)
      result.type = expression_value_type(when_true);
    else {
      const TypePtr true_value = expression_value_type(when_true);
      const TypePtr false_value = expression_value_type(when_false);
      Binding* true_conversion = 0;
      Binding* false_conversion = 0;
      if(true_value && false_value && true_value->kind == TYPE_CLASS &&
         false_value->kind == TYPE_CLASS &&
         !PA12SameType(true_value, false_value, false)) {
        true_conversion = FindConversionOperator(true_value, false_value, false);
        false_conversion = FindConversionOperator(false_value, true_value, false);
      }
      if(true_conversion && !false_conversion) {
        TypePtr function = function_target_type(true_conversion->type);
        result.type = function ? function->child : CommonType(when_true.type,
          when_false.type);
      } else if(false_conversion && !true_conversion) {
        TypePtr function = function_target_type(false_conversion->type);
        result.type = function ? function->child : CommonType(when_true.type,
          when_false.type);
      } else result.type = CommonType(when_true.type, when_false.type);
    }
    const bool same_lvalue_type = PA12SameType(when_true.type, when_false.type, false) &&
      when_true.category == "lvalue" && when_false.category == "lvalue";
    const bool converted_lvalue = type_is_reference(result.type) &&
      result.type->kind == TYPE_LVALUE_REFERENCE &&
      ((when_true.category == "lvalue" && when_false.category == "lvalue") ||
       (when_true.category == "prvalue" && when_false.category == "lvalue") ||
       (when_true.category == "lvalue" && when_false.category == "prvalue"));
    result.category = (same_lvalue_type || converted_lvalue) ? "lvalue" : "prvalue";
    return result;
}

} // namespace cppgm_pa14_lowering
