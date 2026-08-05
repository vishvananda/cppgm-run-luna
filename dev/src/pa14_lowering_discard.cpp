#include "pa14_lowering.h"

using namespace std;

namespace cppgm_pa14_lowering {

void PA14Lowerer::EmitDiscard(const CPPGMAstNodePtr& node, Scope* scope)
{
    if(!node) return;
    if(node->kind == "parenthesized-expression") {
      if(!node->children.empty()) EmitDiscard(node->children[0], scope);
      return;
    }
    if(node->kind == "throw-expression") {
      EmitThrow(node->children.empty() ? CPPGMAstNodePtr() : node->children[0], scope);
      return;
    }
    if(node->kind == "postfix-expression") {
      EmitUpdate(node, scope, false);
      return;
    }
    if(node->kind == "assignment-expression") {
      EmitAssignment(node, scope);
      return;
    }
    if(node->kind == "call-expression") {
      ExprInfo info = Infer(node, scope);
      TypePtr value_type = expression_value_type(info);
      TypePtr constructed = !node->children.empty() ?
        ConstructorObjectType(node->children[0], scope) : TypePtr();
      if(constructed) {
        if(constructed->kind == TYPE_ARRAY) {
          const string slot = new_special_slot("discardarr", low_type(constructed));
          const string address = new_temp();
          AddInstruction(address + " = addr $" + slot);
          const CPPGMAstNodePtr argument_list = node->children.size() > 1 ?
            node->children[1] : CPPGMAstNodePtr();
          vector<CPPGMAstNodePtr> arguments = argument_list ?
            argument_list->children : vector<CPPGMAstNodePtr>();
          if(node->value == "braced-construction" && arguments.size() == 1 &&
             arguments[0] && arguments[0]->kind == "braced-init-list")
            arguments = arguments[0]->children;
          CPPGMAstNodePtr aggregate(new CPPGMAstNode("braced-init-list"));
          aggregate->children = arguments;
          EmitAggregateArrayAt(address, constructed, aggregate, scope,
            CPPGMAstNodePtr(), -1, true);
          return;
        }
        (void)EmitValue(node, scope);
        return;
      }
      if(value_type && value_type->kind == TYPE_CLASS &&
         !type_is_reference(info.type)) {
        const string slot = new_special_slot("discard", low_type(value_type));
        const string destination = new_temp();
        AddInstruction(destination + " = addr $" + slot);
        CallChoice choice = ChooseCall(node, scope);
        const CPPGMAstNodePtr argument_list = node->children.size() > 1 ?
          node->children[1] : CPPGMAstNodePtr();
        const vector<CPPGMAstNodePtr> arguments = argument_list ?
          argument_list->children : vector<CPPGMAstNodePtr>();
        FunctionRecord* record = choice.binding ? RecordForBinding(choice.binding) : 0;
        Value result = EmitChosenCall(choice, node->children[0], arguments, scope,
                                      destination);
        if(!record || !record->indirect_result)
          AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(value_type))) +
            "x" + integer_text(static_cast<long long>(type_alignment(value_type))) +
            " " + result.operand + ", " + destination);
        RegisterTemporaryObject(value_type, destination);
      } else {
        EmitCall(node, scope);
      }
      return;
    }
    if(node->kind == "binary-expression" && PA12Operator(node->value) == ",") {
      if(node->children.size() > 0) EmitDiscard(node->children[0], scope);
      if(node->children.size() > 1) EmitDiscard(node->children[1], scope);
      return;
    }
    if(node->kind == "cast-expression" && node->children.size() > 1) {
		TypePtr target = analyzer_.TypeFromTypeId(node->children[0], scope);
		if(low_type(target) == "void") {
			const CPPGMAstNodePtr discarded = node->children[1];
			if(discarded && discarded->kind == "call-expression") {
				const ExprInfo discarded_info = Infer(discarded, scope);
				const TypePtr discarded_value = expression_value_type(discarded_info);
				if(type_is_reference(discarded_info.type) && discarded_value &&
					discarded_value->kind != TYPE_CLASS &&
					discarded_value->kind != TYPE_ARRAY &&
					discarded_value->kind != TYPE_FUNCTION) {
					(void)EmitValue(discarded, scope);
					return;
				}
			}
			EmitDiscard(discarded, scope);
			return;
		}
    }
    ExprInfo info = Infer(node, scope);
    TypePtr value_type = expression_value_type(info);
    if(info.category == "lvalue" && value_type &&
       (value_type->kind == TYPE_CLASS || value_type->kind == TYPE_ARRAY)) {
      (void)EmitAddress(node, scope);
      return;
    }
    if(value_type && value_type->kind == TYPE_CLASS &&
       !type_is_reference(info.type)) {
      const string slot = new_special_slot("discard", low_type(value_type));
      const string destination = new_temp();
      AddInstruction(destination + " = addr $" + slot);
      const Value result = EmitValue(node, scope);
      if(!result.operand.empty())
        AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(value_type))) +
          "x" + integer_text(static_cast<long long>(type_alignment(value_type))) +
          " " + result.operand + ", " + destination);
      RegisterTemporaryObject(value_type, destination);
      return;
    }
    EmitValue(node, scope);
  }


} // namespace cppgm_pa14_lowering
