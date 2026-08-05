#include "pa14_lowering.h"

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace std;

namespace cppgm_pa14_lowering {

PA14Lowerer::Value PA14Lowerer::EmitLogicalValue(const CPPGMAstNodePtr& node, Scope* scope)
{
    const string op = PA12Operator(node->value);
    const string slot = new_special_slot(op == "||" || op == "or" ? "lor" : "land", "i64");
    const string rhs_label = new_label(op == "||" || op == "or" ? "lor_rhs" : "land_rhs");
    const string short_label = new_label(op == "||" || op == "or" ? "lor_short" : "land_short");
    const string end_label = new_label(op == "||" || op == "or" ? "lor_end" : "land_end");
    const CPPGMAstNodePtr left_node = node->children[0];
    const bool left_is_logical = left_node && left_node->kind == "binary-expression" &&
      (PA12Operator(left_node->value) == "&&" || PA12Operator(left_node->value) == "||" ||
       PA12Operator(left_node->value) == "and" || PA12Operator(left_node->value) == "or");
    if(left_is_logical) {
      Value left = EmitValue(left_node, scope);
      Terminate("branch " + left.operand + ", ^" +
        (op == "||" || op == "or" ? short_label : rhs_label) + ", ^" +
        (op == "||" || op == "or" ? rhs_label : short_label));
    } else if(op == "||" || op == "or") {
      EmitCondition(left_node, scope, short_label, rhs_label);
    } else EmitCondition(left_node, scope, rhs_label, short_label);
    AddBlock(rhs_label);
    const size_t right_temporary_mark = state_ ? state_->temporary_objects.size() : 0;
    const bool previous_defer = state_ && state_->defer_temporary_cleanup;
    const bool previous_defer_unwind = state_ && state_->defer_call_unwind_completion;
    if(state_) state_->defer_temporary_cleanup = true;
    if(state_) state_->defer_call_unwind_completion = true;
    string right;
    try {
      right = EmitLogicalRightTruth(node->children[1], scope);
    } catch(...) {
      if(state_) state_->defer_temporary_cleanup = previous_defer;
      if(state_) state_->defer_call_unwind_completion = previous_defer_unwind;
      throw;
    }
    if(state_) state_->defer_temporary_cleanup = previous_defer;
    if(state_) state_->defer_call_unwind_completion = previous_defer_unwind;
    emit_store(Fundamental("long int"), right, "$" + slot);
    if(state_ && state_->temporary_objects.size() > right_temporary_mark)
      EmitTemporaryDestructors(right_temporary_mark, scope);
    if(state_ && state_->pending_call_unwind)
      FinishPendingCallUnwind(scope);
    Terminate("jump ^" + end_label);
    AddBlock(short_label);
    emit_store(Fundamental("long int"), op == "||" || op == "or" ? "1" : "0", "$" + slot);
    Terminate("jump ^" + end_label);
    AddBlock(end_label);
    Value result;
    result.type = Fundamental("bool");
    result.operand = emit_load("$" + slot, Fundamental("long int"));
    return result;
  }

void PA14Lowerer::EmitConditionCore(const CPPGMAstNodePtr& node, Scope* scope,
                     const string& true_label, const string& false_label)
{
    if(!node) { Terminate("branch 0, ^" + false_label + ", ^" + false_label); return; }
    if(node->kind == "parenthesized-expression" && !node->children.empty()) {
      EmitCondition(node->children[0], scope, true_label, false_label);
      return;
    }
    if(node->kind == "condition-declaration") {
      VariablePlan* variable = BindCondition(node);
      if(!variable || node->children.size() < 3) throw logic_error("invalid condition declaration");
      EmitInitializer(variable, node->children[2], scope);
      CPPGMAstNodePtr condition_id(new CPPGMAstNode("id-expression", variable->source_name));
      ExprInfo condition_info = Infer(condition_id, scope);
      TypePtr condition_type = expression_value_type(condition_info);
      Value value = condition_type && condition_type->kind == TYPE_CLASS &&
        FindContextConversionOperator(condition_type, true, true) ?
        EmitContextConversion(condition_id, scope, true, true) :
        EmitValue(condition_id, scope);
      string operand = value.operand;
      if(value.type && type_value(value.type) && type_value(value.type)->kind == TYPE_CLASS &&
         FindContextConversionOperator(type_value(value.type), true, true)) {
        value = EmitContextConversion(CPPGMAstNodePtr(
          new CPPGMAstNode("id-expression", variable->source_name)), scope, true, true);
        operand = value.operand;
      }
      if(is_floating_type(value.type)) operand = EmitTruthValue(value);
      Terminate("branch " + operand + ", ^" + true_label + ", ^" + false_label);
      return;
    }
    if(node->kind == "binary-expression") {
      const string op = PA12Operator(node->value);
      if(op == "&&" || op == "||" || op == "and" || op == "or") {
        vector<CPPGMAstNodePtr> operator_arguments;
        operator_arguments.push_back(node->children[0]);
        operator_arguments.push_back(node->children[1]);
        if(ChooseOperatorCall(OperatorFunctionName(op), operator_arguments, scope).binding) {
          Value value = EmitOperatorCall(OperatorFunctionName(op), operator_arguments, scope);
          value.operand = EmitTruthValue(value);
          Terminate("branch " + value.operand + ", ^" + true_label + ", ^" + false_label);
          return;
        }
      }
      if(op == "&&" || op == "and") {
        const string rhs_label = new_label("land_rhs");
        EmitConditionCore(node->children[0], scope, rhs_label, false_label);
        AddBlock(rhs_label);
        EmitConditionCore(node->children[1], scope, true_label, false_label);
        return;
      }
      if(op == "||" || op == "or") {
        const string rhs_label = new_label("lor_rhs");
        EmitConditionCore(node->children[0], scope, true_label, rhs_label);
        AddBlock(rhs_label);
        EmitConditionCore(node->children[1], scope, true_label, false_label);
        return;
      }
      if(op == "==" || op == "!=" || op == "not_eq" || op == "<" ||
         op == ">" || op == "<=" || op == ">=") {
        Value value = EmitCompare(node, scope);
        Terminate("branch " + value.operand + ", ^" + true_label + ", ^" + false_label);
        return;
      }
    }
    if(node->kind == "unary-expression" && PA12Operator(node->value) == "!") {
      ExprInfo child_info = Infer(node->children[0], scope);
      TypePtr child_type = expression_value_type(child_info);
      Value child = child_type && child_type->kind == TYPE_CLASS &&
        FindContextConversionOperator(child_type, true, true) ?
        EmitContextConversion(node->children[0], scope, true, true) :
        EmitValue(node->children[0], scope);
      if(child.lvalue && child.type) {
        child.operand = emit_load(child.operand, child.type);
        child.lvalue = false;
      }
      TypePtr type = type_value(child.type);
      if(type && (is_integral_type(type) ||
                  (type->kind == TYPE_FUNDAMENTAL && type->name == "bool"))) {
        const string temp = new_temp();
        AddInstruction(temp + " = cmp eq i64 " + child.operand + ", 0");
        Terminate("branch " + temp + ", ^" + true_label + ", ^" + false_label);
      } else if(type && type->kind == TYPE_POINTER) {
        const string temp = new_temp();
        AddInstruction(temp + " = cmp eq " + low_type(type) + " " +
          child.operand + ", 0");
        Terminate("branch " + temp + ", ^" + true_label + ", ^" + false_label);
      } else {
        const string operand = EmitTruthValue(child);
        Terminate("branch " + operand + ", ^" + false_label + ", ^" + true_label);
      }
      return;
    }
    ExprInfo value_info = Infer(node, scope);
    TypePtr type = expression_value_type(value_info);
    Value value = type && type->kind == TYPE_CLASS &&
      FindContextConversionOperator(type, true, true) ?
      EmitContextConversion(node, scope, true, true) : EmitValue(node, scope);
    string operand = value.operand;
    if(type && type->kind == TYPE_CLASS && value.type &&
       type_value(value.type)->kind != TYPE_CLASS) {
      if(value.lvalue && value.type) {
        value.operand = emit_load(value.operand, value.type);
        value.lvalue = false;
      }
      operand = EmitTruthValue(value);
      type = type_value(value.type);
    } else if(value.type && type_value(value.type)->kind != TYPE_CLASS) {
      operand = is_floating_type(value.type) ? EmitTruthValue(value) : value.operand;
    }
    Terminate("branch " + operand + ", ^" + true_label + ", ^" + false_label);
  }

void PA14Lowerer::EmitCondition(const CPPGMAstNodePtr& node, Scope* scope,
                                const string& true_label,
                                const string& false_label)
{
    if(!state_ || state_->condition_cleanup_depth != 0) {
      EmitConditionCore(node, scope, true_label, false_label);
      return;
    }
    const size_t temporary_mark = state_->temporary_objects.size();
    const size_t block_mark = state_->blocks.size();
    const size_t line_mark = state_->current ? state_->current->lines.size() : 0;
    const bool previous_defer = state_->defer_temporary_cleanup;
    state_->condition_cleanup_depth = 1;
    state_->defer_temporary_cleanup = true;
    try {
      EmitConditionCore(node, scope, true_label, false_label);
    } catch(...) {
      state_->condition_cleanup_depth = 0;
      state_->defer_temporary_cleanup = previous_defer;
      throw;
    }
    state_->condition_cleanup_depth = 0;
    state_->defer_temporary_cleanup = previous_defer;
    if(state_->temporary_objects.size() == temporary_mark) return;

    const string cleanup_true = new_label("cond_true_cleanup");
    const string cleanup_false = new_label("cond_false_cleanup");
    const string true_token = "^" + true_label;
    const string false_token = "^" + false_label;
    for(size_t block = block_mark; block < state_->blocks.size(); ++block) {
      const size_t first_line = block == block_mark ? line_mark : 0;
      for(size_t line = first_line; line < state_->blocks[block].lines.size(); ++line) {
        string& text = state_->blocks[block].lines[line];
        size_t position = text.find(true_token);
        while(position != string::npos) {
          text.replace(position, true_token.size(), "^" + cleanup_true);
          position = text.find(true_token, position + cleanup_true.size() + 1);
        }
        position = text.find(false_token);
        while(position != string::npos) {
          text.replace(position, false_token.size(), "^" + cleanup_false);
          position = text.find(false_token, position + cleanup_false.size() + 1);
        }
      }
    }
    vector<FunctionState::TemporaryObject> cleanup;
    for(size_t i = state_->temporary_objects.size(); i > temporary_mark; --i)
      cleanup.push_back(state_->temporary_objects[i - 1]);
    AddBlock(cleanup_true);
    EmitCleanupObjects(cleanup, scope);
    Terminate("jump ^" + true_label);
    AddBlock(cleanup_false);
    EmitCleanupObjects(cleanup, scope);
    Terminate("jump ^" + false_label);
    state_->temporary_objects.resize(temporary_mark);
  }


} // namespace cppgm_pa14_lowering
