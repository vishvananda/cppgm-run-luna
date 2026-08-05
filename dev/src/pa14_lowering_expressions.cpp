#include "pa14_lowering.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
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

void PA14Lowerer::InferLocalIdentifierConstant(const TypePtr& type,
                                                ExprInfo* result) const
{
  if(!result) return;
  TypePtr local_type = type_value(type);
  if(!local_type || local_type->kind != TYPE_CLASS || !local_type->owned_scope ||
     !FindContextConversionOperator(local_type, false, true)) return;
  const vector<Binding*> value_members = MemberBindings(local_type, "value");
  for(size_t member = 0; member < value_members.size(); ++member) {
    Binding* value = value_members[member];
    const TypePtr value_type = value ? type_value(value->type) : TypePtr();
    const bool integral_value = value_type &&
      (is_integral_type(value_type) ||
       (value_type->kind == TYPE_FUNDAMENTAL && value_type->name == "bool"));
    if(!value || value->kind != BIND_VARIABLE || !value->is_static ||
       !value->has_value || !integral_value) continue;
    result->known_constant = true;
    result->constant = value->constant_value.known ?
      PA19Signed(value->constant_value) : value->value;
    return;
  }
}

PA14Lowerer::Value PA14Lowerer::EmitBitFieldLoad(Binding* binding, const string& address,
                                      const TypePtr& type, bool copy_result)
{
    long long bit_offset = 0;
    long long bit_width = 0;
    if(!IsBitField(binding, &bit_offset, &bit_width)) {
      Value result;
      result.type = type;
      result.operand = emit_load(address, type);
      return result;
    }
    const string low = low_type(type);
    const unsigned int bits = static_cast<unsigned int>(max<size_t>(1, type_size(type)) * 8);
    const unsigned long long mask = bit_width >= 64 ? ~0ULL :
      ((1ULL << static_cast<unsigned int>(bit_width)) - 1ULL);
    const string loaded = emit_load(address, type);
    const string masked = new_temp();
    AddInstruction(masked + " = binary and " + low + " " + loaded + ", " +
      integer_text(static_cast<long long>(mask)));
    string operand = masked;
    if(bit_offset != 0) {
      const string shifted = new_temp();
      AddInstruction(shifted + " = binary shr " + low + " " + masked + ", " +
        integer_text(bit_offset));
      operand = shifted;
    }
    Value result;
    result.type = type;
    result.operand = operand;
    if(copy_result) {
      const string copied = new_temp();
      AddInstruction(copied + " = copy " + low + " " + operand);
      result.operand = copied;
    }
    (void)bits;
    return result;
  }

string PA14Lowerer::PrepareBitFieldValue(Binding* binding, const TypePtr& type,
                                          const string& value)
{
    long long bit_offset = 0;
    long long bit_width = 0;
    if(!IsBitField(binding, &bit_offset, &bit_width)) return value;
    const string low = low_type(type);
    const unsigned long long field_mask = bit_width >= 64 ? ~0ULL :
      ((1ULL << static_cast<unsigned int>(bit_width)) - 1ULL);
    const string masked = new_temp();
    AddInstruction(masked + " = binary and " + low + " " +
      integer_text(static_cast<long long>(field_mask)) + ", " + value);
    if(bit_offset == 0) return masked;
    const string shifted = new_temp();
    AddInstruction(shifted + " = binary shl " + low + " " + masked + ", " +
      integer_text(bit_offset));
    return shifted;
  }

string PA14Lowerer::MergeBitFieldValue(Binding* binding, const string& address,
                                        const TypePtr& type, const string& value,
                                        bool preserve)
{
    long long bit_offset = 0;
    long long bit_width = 0;
    if(!IsBitField(binding, &bit_offset, &bit_width)) return value;
    if(!preserve) return value;
    const string low = low_type(type);
    const unsigned int bits = static_cast<unsigned int>(max<size_t>(1, type_size(type)) * 8);
    const unsigned long long field_mask = bit_width >= 64 ? ~0ULL :
      ((1ULL << static_cast<unsigned int>(bit_width)) - 1ULL);
    const unsigned long long shifted_mask = bit_offset >= 64 ? 0ULL :
      (field_mask << static_cast<unsigned int>(bit_offset));
    const string old = emit_load(address, type);
    const unsigned long long full_mask = bits >= 64 ? ~0ULL : ((1ULL << bits) - 1ULL);
    const string retained = new_temp();
    AddInstruction(retained + " = binary and " + low + " " + old + ", " +
      integer_text(static_cast<long long>(full_mask ^ shifted_mask)));
    const string prepared = PrepareBitFieldValue(binding, type, value);
    const string combined = new_temp();
    AddInstruction(combined + " = binary or " + low + " " + retained + ", " + prepared);
    return combined;
  }

void PA14Lowerer::StoreBitField(Binding* binding, const string& address,
                                const TypePtr& type, const string& value,
                                bool initializing)
{
    if(!IsBitField(binding)) {
      emit_store(type, value, address);
      return;
    }
    const bool preserve = !initializing;
    const string merged = MergeBitFieldValue(binding, address, type, value, preserve);
    const string stored = preserve ? merged : PrepareBitFieldValue(binding, type, value);
    emit_store(type, stored, address);
  }

void PA14Lowerer::StoreLValue(const CPPGMAstNodePtr& node, Scope* scope,
                   const TypePtr& type, const string& value)
{
    if(node && node->kind == "parenthesized-expression" && !node->children.empty()) {
      StoreLValue(node->children[0], scope, type, value);
      return;
    }
    if(node && node->kind == "member-expression") {
      Binding* binding = MemberBinding(node, scope);
      if(binding && IsBitField(binding)) {
        StoreBitField(binding, EmitMemberAddress(node, scope), type, value);
        return;
      }
    }
    if(node && node->kind == "id-expression") {
      VariablePlan* local = LocalForName(node->value);
      if(local && !type_is_reference(local->type)) {
        emit_store(type, value, StorageForVariable(*local));
        return;
      }
      if(!local) {
        vector<Binding*> candidates = Lookup(node->value, scope);
        if(candidates.size() == 1 && candidates[0]->is_member &&
           !candidates[0]->is_static && IsBitField(candidates[0])) {
          CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "OP_ARROW:->"));
          member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
            "keyword-literal", "KW_THIS:this")));
          member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
            "identifier", candidates[0]->name)));
          StoreBitField(candidates[0], EmitMemberAddress(member, scope), type, value);
          return;
        }
        if(candidates.size() == 1 && candidates[0]->kind == BIND_VARIABLE) {
          Binding* global_binding = candidates[0];
          GlobalRecord* global = FindGlobal(global_binding->qualified_name);
          if(global) {
            global->referenced = true;
            if(type_is_reference(global->type)) {
              const string address = emit_load("@" + global->symbol,
                PointerTo(Fundamental("char")));
              emit_store(type, value, address);
            } else emit_store(type, value, "@" + global->symbol);
            return;
          }
        }
        if(candidates.size() > 1) {
          bool duplicate_declarations = true;
          for(size_t i = 1; i < candidates.size(); ++i)
            if(candidates[i]->kind != BIND_VARIABLE ||
               candidates[i]->qualified_name != candidates[0]->qualified_name ||
               !PA12SameType(candidates[i]->type, candidates[0]->type, false)) {
              duplicate_declarations = false;
              break;
            }
          if(duplicate_declarations) {
            GlobalRecord* global = FindGlobal(candidates[0]->qualified_name);
            if(global) {
              global->referenced = true;
              if(type_is_reference(global->type)) {
                const string address = emit_load("@" + global->symbol,
                  PointerTo(Fundamental("char")));
                emit_store(type, value, address);
              } else emit_store(type, value, "@" + global->symbol);
              return;
            }
          }
        }
      }
    }
    emit_store(type, value, EmitAddress(node, scope));
  }
PA14Lowerer::Value PA14Lowerer::EmitAssignment(const CPPGMAstNodePtr& node, Scope* scope)
{
    if(!node || node->children.size() < 2) throw logic_error("invalid assignment");
    const string op = PA12Operator(node->value);
    vector<CPPGMAstNodePtr> operator_arguments;
    operator_arguments.push_back(node->children[0]);
    operator_arguments.push_back(node->children[1]);
    if(op == "=" && node->children[1] &&
       node->children[1]->kind == "braced-init-list") {
      Value direct_result;
      if(EmitInitializerListAssignment(operator_arguments, scope, &direct_result)) return direct_result;
      ExprInfo left_info = Infer(node->children[0], scope);
      TypePtr left_type = expression_value_type(left_info);
      if(left_type && left_type->kind == TYPE_CLASS) {
        const size_t temporary_mark = state_ ? state_->temporary_objects.size() : 0;
        FunctionRecord* assignment = EnsureImplicitAssignment(left_type, true);
        if(assignment && !assignment->deleted) {
          const string destination = EmitAddress(node->children[0], scope);
          const string slot = new_special_slot("arg", low_type(left_type));
          const string temporary = new_temp();
          AddInstruction(temporary + " = addr $" + slot);
          if(!EmitConstructorAt(left_type, temporary, node->children[1]->children,
                                scope, true, false, true))
            throw logic_error("braced assignment temporary has no constructor");
          MarkFunctionNeeded(assignment);
          FunctionRecord* base_entry = BaseEntryFor(assignment);
          if(base_entry) MarkFunctionNeeded(base_entry);
          const string result = new_temp();
          const string return_type = low_type(assignment->type->child);
          AddInstruction(result + " = call " + return_type + " @" +
            assignment->symbol + "(" + destination + ", " + temporary + ")");
          RegisterTemporaryObject(left_type, temporary);
          EmitTemporaryDestructors(temporary_mark, scope);
          Value assigned;
          assigned.type = left_type;
          assigned.operand = result;
          assigned.lvalue = true;
          return assigned;
        }
      }
    }
    if(op == "=") {
      ExprInfo left_probe = Infer(node->children[0], scope);
      TypePtr left_probe_type = expression_value_type(left_probe);
      if(left_probe_type && left_probe_type->kind == TYPE_CLASS) {
        const vector<Binding*> direct_assignments = DirectBindings(
          left_probe_type->owned_scope, "operator=");
        bool has_direct_assignment = false;
        for(size_t i = 0; i < direct_assignments.size(); ++i)
          if(direct_assignments[i]->kind == BIND_FUNCTION &&
             (!RecordForBinding(direct_assignments[i]) ||
              !RecordForBinding(direct_assignments[i])->member_template)) {
            has_direct_assignment = true;
        }
        // An inherited operator= does not suppress the derived class's
        // implicit assignment operator.  Synthesize that direct candidate
        // before overload lookup so its complete object/base storage is used.
        ExprInfo right_probe = Infer(node->children[1], scope);
        bool has_direct_move_assignment = false;
        for(size_t i = 0; i < direct_assignments.size(); ++i) {
          FunctionRecord* direct_record = RecordForBinding(direct_assignments[i]);
          if(direct_record && !direct_record->member_template &&
             direct_record->move_assignment) {
            has_direct_move_assignment = true;
            break;
          }
        }
        if(!has_direct_assignment)
          (void)EnsureImplicitAssignment(left_probe_type, false);
        if(right_probe.category != "lvalue" && !has_direct_move_assignment)
          (void)EnsureImplicitAssignment(left_probe_type, true);
      }
    }
    if(ChooseOperatorCall(OperatorFunctionName(op), operator_arguments, scope).binding)
      return EmitOperatorCall(OperatorFunctionName(op), operator_arguments, scope);
    ExprInfo left_info = Infer(node->children[0], scope);
    TypePtr left_type = expression_value_type(left_info);
    if(!left_type) throw logic_error("assignment has no target type");
    ExprInfo right_order_info;
    if(op == "=") right_order_info = Infer(node->children[1], scope, left_type);
    Binding* target_binding = node->children[0] &&
      node->children[0]->kind == "member-expression" ?
      MemberBinding(node->children[0], scope) : left_info.binding;
    FunctionRecord* target_function = target_binding &&
      target_binding->kind == BIND_FUNCTION ? RecordForBinding(target_binding) : 0;
    const bool template_assignment =
      (target_binding && target_binding->member_owner &&
       target_binding->member_owner->template_specialization) ||
      (target_function && target_function->template_instantiation);
    const bool union_member = target_binding && target_binding->member_owner &&
      target_binding->member_owner->is_union;
    const bool byte_array_element = node->children[0] &&
      node->children[0]->kind == "subscript-expression";
    const bool unqualified_member_rhs = op == "=" && node->children[0] &&
      node->children[0]->kind == "member-expression" && node->children[1] &&
      node->children[1]->kind == "id-expression" && right_order_info.binding &&
      right_order_info.binding->is_member && !right_order_info.binding->is_static &&
      right_order_info.binding->member_owner &&
      right_order_info.binding->member_owner->template_specialization;
    if(unqualified_member_rhs) {
      // A dependent unqualified member on the right has a typed implicit
      // object.  Materialize the target base before that object is evaluated;
      // StoreLValue still emits the final member projection after the value.
      if(PA12Operator(node->children[0]->value) == ".")
        (void)EmitAddress(node->children[0]->children[0], scope);
      else
        (void)EmitValue(node->children[0]->children[0], scope);
    }
    Value right;
    const bool defer_assignment_call_unwind = op == "=" && state_ &&
      node->children[1] && node->children[1]->kind == "call-expression";
    const bool previous_assignment_call_defer = state_ &&
      state_->defer_call_unwind_completion;
    if(defer_assignment_call_unwind)
      state_->defer_call_unwind_completion = true;
    if(op == "=") {
      const ExprInfo& right_info = right_order_info;
		if(ConversionRank(right_info, left_type) < 0)
			throw logic_error("invalid assignment conversion");
      try {
        right = EmitValue(node->children[1], scope, left_type);
      } catch(...) {
        if(defer_assignment_call_unwind)
          state_->defer_call_unwind_completion = previous_assignment_call_defer;
        throw;
      }
      if(defer_assignment_call_unwind)
        state_->defer_call_unwind_completion = previous_assignment_call_defer;
      // A constant scalar assignment already has its final value; retaining
      // the literal avoids manufacturing a widening/truncation instruction
      // just to store a byte-sized array element.
      const TypePtr right_type = type_value(right.type);
      const bool right_integral = right_type &&
        (is_integral_type(right_type) ||
         (right_type->kind == TYPE_FUNDAMENTAL && right_type->name == "bool"));
      const TypePtr target_type = type_value(left_type);
      const bool target_integral = target_type &&
        (is_integral_type(target_type) ||
         (target_type->kind == TYPE_FUNDAMENTAL && target_type->name == "bool"));
      const bool preserve_size_expression = node->children[1] &&
        (node->children[1]->kind == "sizeof-expression" ||
         node->children[1]->kind == "sizeof-pack-expression");
      if((template_assignment || union_member || byte_array_element) &&
         !preserve_size_expression &&
         right.known_constant && right_integral && target_integral) {
        right.type = left_type;
        right.operand = integer_text(right.constant);
      }
    }
    else {
      Value left = EmitValue(node->children[0], scope);
      ExprInfo raw_right_info = Infer(node->children[1], scope);
      Value raw_right;
      if(expression_value_type(raw_right_info) &&
         expression_value_type(raw_right_info)->kind == TYPE_CLASS &&
         FindConversionOperator(expression_value_type(raw_right_info), left_type, false)) {
        raw_right = EmitConversionOperator(node->children[1], scope, left_type, false);
        if(raw_right.lvalue && raw_right.type) {
          raw_right.operand = emit_load(raw_right.operand, raw_right.type);
          raw_right.lvalue = false;
        }
      } else raw_right = EmitValue(node->children[1], scope);
      TypePtr common = CommonType(left.type, raw_right.type, op);
      if(!common || common->kind == TYPE_POINTER) common = left_type;
      left = ConvertValue(left, common);
      right = ConvertValue(raw_right, common);
      string binary;
      if(op == "+=") binary = "add";
      else if(op == "-=") binary = "sub";
      else if(op == "*=") binary = "mul";
      else if(op == "/=") binary = "div";
      else if(op == "%=") binary = "mod";
      else if(op == "&=") binary = "and";
      else if(op == "|=") binary = "or";
      else if(op == "^=") binary = "xor";
      else if(op == "<<=") binary = "shl";
      else if(op == ">>=") binary = "shr";
      else throw logic_error("unsupported compound assignment");
      if(left_type->kind == TYPE_POINTER && (op == "+=" || op == "-=")) {
        // Pointer compound assignment uses the same element-scaled index
        // operation as ordinary pointer arithmetic.
        const long long size = static_cast<long long>(type_size(left_type->child));
        const string scale = new_temp();
        AddInstruction(scale + " = binary mul i64 " + raw_right.operand + ", " + integer_text(size));
        string offset = scale;
        if(op == "-=") {
          const string neg = new_temp();
          AddInstruction(neg + " = binary sub i64 0, " + scale);
          offset = neg;
        }
        right.type = left_type;
        right.operand = new_temp();
        AddInstruction(right.operand + " = index i8 " + left.operand + ", " + offset);
      } else {
        const string right_operand = right.operand;
        right.type = common;
        right.operand = new_temp();
        AddInstruction(right.operand + " = binary " + binary + " " + low_type(common) +
          " " + left.operand + ", " + right_operand);
      }
    }
    right = ConvertValue(right, left_type, false, true);
    StoreLValue(node->children[0], scope, left_type, right.operand);
    if(defer_assignment_call_unwind && state_ && state_->pending_call_unwind)
      FinishPendingCallUnwind(scope);
    right.lvalue = true;
    right.type = left_type;
    return right;
  }
PA14Lowerer::Value PA14Lowerer::EmitUpdate(const CPPGMAstNodePtr& node, Scope* scope, bool address_only)
{
    const CPPGMAstNodePtr child_node = node->children[0];
    vector<CPPGMAstNodePtr> operator_arguments;
    operator_arguments.push_back(child_node);
    if(node->kind == "postfix-expression")
      operator_arguments.push_back(CPPGMAstNodePtr(new CPPGMAstNode("literal", "0")));
    if(ChooseOperatorCall(OperatorFunctionName(PA12Operator(node->value)),
                          operator_arguments, scope).binding)
      return EmitOperatorCall(OperatorFunctionName(PA12Operator(node->value)),
                              operator_arguments, scope);
    ExprInfo child_info = Infer(child_node, scope);
    TypePtr type = expression_value_type(child_info);
    Value converted_result;
    if(EmitReferenceConversionUpdate(node, scope, &converted_result))
      return converted_result;
    string cached_address;
    Binding* cached_member = 0;
    if(child_node && (child_node->kind == "member-expression" ||
       (child_info.binding && child_info.binding->is_member &&
       !child_info.binding->is_static) ||
       (child_node->kind == "unary-expression" &&
        PA12Operator(child_node->value) == "*"))) {
      cached_member = child_node->kind == "member-expression" ?
        MemberBinding(child_node, scope) : child_info.binding;
      cached_address = child_node->kind == "member-expression" ?
        EmitMemberAddress(child_node, scope) : EmitAddress(child_node, scope);
    }
    Value old;
    old.type = type;
    old.operand = cached_address.empty() ? EmitValue(child_node, scope).operand :
      emit_load(cached_address, type);
    Value result;
    result.type = type;
    result.lvalue = true;
    if(type && type->kind == TYPE_POINTER) {
      const long long size = static_cast<long long>(type_size(type->child));
      string offset;
      if(size == 1 && PA12Operator(node->value) == "++") offset = "1";
      else {
        const string scale = new_temp();
        AddInstruction(scale + " = binary mul i64 1, " + integer_text(size));
        offset = scale;
        if(PA12Operator(node->value) == "--") {
          const string neg = new_temp();
          AddInstruction(neg + " = binary sub i64 0, " + scale);
          offset = neg;
        }
      }
      result.operand = new_temp();
      AddInstruction(result.operand + " = index i8 " + old.operand + ", " + offset);
    } else {
      result.operand = new_temp();
      AddInstruction(result.operand + " = binary " +
        (PA12Operator(node->value) == "++" ? "add" : "sub") + " " + low_type(type) +
        " " + old.operand + ", 1");
    }
    if(!cached_address.empty()) {
      if(cached_member && IsBitField(cached_member))
        StoreBitField(cached_member, cached_address, type, result.operand);
      else emit_store(type, result.operand, cached_address);
    } else StoreLValue(child_node, scope, type, result.operand);
    (void)address_only;
    return node->kind == "postfix-expression" ? old : result;
  }
PA14Lowerer::Value PA14Lowerer::EmitCompare(const CPPGMAstNodePtr& node, Scope* scope)
{
    vector<CPPGMAstNodePtr> operator_arguments;
    operator_arguments.push_back(node->children[0]);
    operator_arguments.push_back(node->children[1]);
    ExprInfo binary_info = Infer(node, scope);
    if(binary_info.binding)
      return EmitOperatorCall(OperatorFunctionName(PA12Operator(node->value)),
                              operator_arguments, scope);
    ExprInfo left_info = Infer(node->children[0], scope);
    ExprInfo right_info = Infer(node->children[1], scope);
    TypePtr left_type = expression_value_type(left_info);
    TypePtr right_type = expression_value_type(right_info);
    TypePtr common;
    if(left_type && left_type->kind == TYPE_MEMBER_POINTER)
      common = left_type;
    else if(right_type && right_type->kind == TYPE_MEMBER_POINTER)
      common = right_type;
    else common = ArithmeticCommonType(left_type, right_type,
      PA12Operator(node->value));
    if(left_type && right_type && left_type->kind == TYPE_POINTER && right_type->kind == TYPE_POINTER)
      common = left_type;
    const bool left_bit_field = node->children[0] &&
      node->children[0]->kind == "member-expression" &&
      IsBitField(MemberBinding(node->children[0], scope));
    const bool right_bit_field = node->children[1] &&
      node->children[1]->kind == "member-expression" &&
      IsBitField(MemberBinding(node->children[1], scope));
    const bool left_class_operand = left_info.type &&
      left_info.type->kind == TYPE_CLASS;
    const bool right_class_operand = right_info.type &&
      right_info.type->kind == TYPE_CLASS;
    const bool left_template_class = left_class_operand &&
      left_info.type->template_specialization;
    const bool right_template_class = right_class_operand &&
      right_info.type->template_specialization;
    const TypePtr left_expected = left_class_operand && !left_template_class ?
      common : TypePtr();
    const TypePtr right_expected = right_class_operand && !right_template_class ?
      common : TypePtr();
    Value left = left_bit_field ? EmitValue(node->children[0], scope) :
      EmitValue(node->children[0], scope, left_expected);
    Value right = right_bit_field ? EmitValue(node->children[1], scope) :
      EmitValue(node->children[1], scope, right_expected);
	if(left.known_constant &&
	   (is_integral_type(left.type) ||
	    (type_value(left.type) && type_value(left.type)->kind == TYPE_FUNDAMENTAL &&
	     type_value(left.type)->name == "bool")) &&
	   is_integral_type(common) &&
       type_size(common) > type_size(left.type) && !is_unsigned_type(common)) {
      left.type = common;
      left.operand = integer_text(left.constant);
    } else if((right_info.known_constant && right_info.constant == 0) ||
              (node->children[1] && node->children[1]->kind == "literal" &&
               node->children[1]->value == "0")) {
      if(common && low_type(common) == "i64" && left_type &&
         low_type(left_type) == "i32") {
        // LowIR's comparison form accepts the narrow loaded operand directly
        // in this canonical zero-comparison case.
        left.type = common;
      } else left = ConvertValue(left, common);
    } else left = ConvertValue(left, common);
    const bool right_scalar_integral = right.type &&
      (is_integral_type(right.type) ||
       (type_value(right.type)->kind == TYPE_FUNDAMENTAL &&
        type_value(right.type)->name == "bool"));
	if(right.known_constant &&
       right_scalar_integral &&
       is_integral_type(common) &&
       type_size(common) > type_size(right.type) && !is_unsigned_type(common)) {
      right.type = common;
      right.operand = integer_text(right.constant);
    } else if(right.known_constant && is_integral_type(right.type) && right.constant == 0 &&
       common && type_value(common)->kind == TYPE_FUNDAMENTAL &&
       is_unsigned_type(common) && low_type(common) == "u32") {
      right.type = common;
      right.operand = "0";
    } else if(right.known_constant && right.type && type_value(right.type)->kind == TYPE_FUNDAMENTAL &&
       (type_value(right.type)->name == "char" ||
        type_value(right.type)->name == "signed char" ||
        type_value(right.type)->name == "unsigned char")) {
      right.type = common;
      right.operand = integer_text(right.constant);
    } else right = ConvertValue(right, common);
    const string op = PA12Operator(node->value);
    string predicate;
    const bool unsigned_compare = common && is_unsigned_type(common) &&
      !is_floating_type(common);
    if(op == "==") predicate = "eq";
    else if(op == "!=" || op == "not_eq") predicate = "ne";
    else if(op == "<") predicate = unsigned_compare ? "ult" : "lt";
    else if(op == ">") predicate = unsigned_compare ? "ugt" : "gt";
    else if(op == "<=") predicate = unsigned_compare ? "ule" : "le";
    else if(op == ">=") predicate = unsigned_compare ? "uge" : "ge";
    else throw logic_error("unsupported comparison");
    Value result;
    result.type = Fundamental("bool");
    result.operand = new_temp();
    AddInstruction(result.operand + " = cmp " + predicate + " " + low_type(common) +
      " " + left.operand + ", " + right.operand);
    return result;
  }
PA14Lowerer::Value PA14Lowerer::EmitBinary(const CPPGMAstNodePtr& node, Scope* scope)
{
    const string op = PA12Operator(node->value);
    vector<CPPGMAstNodePtr> operator_arguments;
    operator_arguments.push_back(node->children[0]);
    operator_arguments.push_back(node->children[1]);
    if(op == ".*" || op == "->*") {
      CallChoice overloaded = ChooseOperatorCall(OperatorFunctionName(op),
        operator_arguments, scope);
      if(overloaded.binding)
        return EmitOperatorCall(OperatorFunctionName(op), operator_arguments, scope);
      ExprInfo left_info = Infer(node->children[0], scope);
      ExprInfo right_info = Infer(node->children[1], scope);
      TypePtr object_type = expression_value_type(left_info);
      const TypePtr member_pointer = expression_value_type(right_info);
      if(op == "->*") {
        if(!object_type || object_type->kind != TYPE_POINTER)
          throw logic_error("arrow-star requires a pointer to class");
        object_type = type_value(object_type->child);
      }
      if(!member_pointer || member_pointer->kind != TYPE_MEMBER_POINTER ||
         !member_pointer->member_owner || !object_type || object_type->kind != TYPE_CLASS ||
         (!PA12SameType(object_type, member_pointer->member_owner, true) &&
          !IsDerivedFrom(object_type, member_pointer->member_owner)))
        throw logic_error("member pointer does not apply to object");
		if(member_pointer->child && member_pointer->child->kind == TYPE_FUNCTION) {
			Value pointer;
			const bool qualified_member_address = node->children[1] &&
				((node->children[1]->kind == "unary-expression" &&
				  PA12Operator(node->children[1]->value) == "&") ||
				 (node->children[1]->kind == "id-expression" &&
				  !node->children[1]->value.empty() && node->children[1]->value[0] == '&'));
			if(qualified_member_address && right_info.binding &&
			   right_info.binding->kind == BIND_FUNCTION &&
			   right_info.binding->member_owner) {
          FunctionRecord* function = RecordForBinding(right_info.binding);
          if(!function) throw logic_error("member function pointer has no definition");
          pointer.type = member_pointer;
          pointer.operand = function_address(function);
          pointer.function = true;
        } else pointer = EmitValue(node->children[1], scope);
        if(pointer.function) {
          Value result;
          result.type = member_pointer->child;
          result.operand = pointer.operand;
          result.function = true;
          return result;
        }
        const string raw_address = new_temp();
        AddInstruction(raw_address + " = convert trunc i64 i128 " + pointer.operand);
        Value result;
        result.type = member_pointer->child;
        result.operand = new_temp();
        AddInstruction(result.operand + " = copy ptr " + raw_address);
        result.function = true;
        return result;
      }
      string object_address;
      if(op == "->*") object_address = EmitValue(node->children[0], scope).operand;
      else object_address = EmitAddress(node->children[0], scope);
      if(!PA12SameType(object_type, member_pointer->member_owner, true))
        object_address = AdjustBaseAddress(object_address, object_type,
          member_pointer->member_owner);
		Value pointer;
		const bool qualified_data_address = node->children[1] &&
			((node->children[1]->kind == "unary-expression" &&
			  PA12Operator(node->children[1]->value) == "&") ||
			 (node->children[1]->kind == "id-expression" &&
			  !node->children[1]->value.empty() && node->children[1]->value[0] == '&'));
		if(qualified_data_address && right_info.binding &&
		   right_info.binding->is_member && !right_info.binding->is_static &&
         right_info.binding->member_owner && member_pointer->child &&
         member_pointer->child->kind != TYPE_FUNCTION) {
        const TypePtr owner = type_value(right_info.binding->member_owner);
        if(right_info.binding->member_index == static_cast<size_t>(-1) ||
           right_info.binding->member_index >= owner->class_members.size())
          throw logic_error("data member pointer has no layout record");
        pointer.type = member_pointer;
        pointer.known_constant = true;
        pointer.constant = static_cast<long long>(owner->class_members[
          right_info.binding->member_index].offset) + 1;
        pointer.operand = integer_text(pointer.constant);
      } else pointer = EmitValue(node->children[1], scope, member_pointer);
	      const string pointer_operand = pointer.known_constant ?
	        integer_text(pointer.constant) : pointer.operand;
	      const string offset = new_temp();
	      AddInstruction(offset + " = binary sub i64 " + pointer_operand + ", 1");
      const string address = new_temp();
      AddInstruction(address + " = index i8 [projection=field] " + object_address +
        ", " + offset);
      Value result;
      result.type = member_pointer->child;
      result.operand = emit_load(address, result.type);
      result.lvalue = false;
      return result;
    }
    if(op == "==" || op == "!=" || op == "not_eq" || op == "<" ||
       op == ">" || op == "<=" || op == ">=") return EmitCompare(node, scope);
    const TypePtr left_operator_type = expression_value_type(Infer(node->children[0], scope));
    const TypePtr right_operator_type = expression_value_type(Infer(node->children[1], scope));
    const bool class_operand = (left_operator_type && left_operator_type->kind == TYPE_CLASS) ||
      (right_operator_type && right_operator_type->kind == TYPE_CLASS);
    bool mixed_bitwise = false;
    if(op == "&" || op == "bitand" || op == "|" || op == "bitor" ||
       op == "^" || op == "xor") {
      const bool same_enum_operands = left_operator_type && right_operator_type &&
        left_operator_type->kind == TYPE_ENUM && right_operator_type->kind == TYPE_ENUM &&
        PA12SameType(left_operator_type, right_operator_type, true);
      mixed_bitwise = !class_operand && !same_enum_operands;
    }
    CallChoice operator_choice;
    if(!mixed_bitwise)
      operator_choice = ChooseOperatorCall(OperatorFunctionName(op),
        operator_arguments, scope);
    if(operator_choice.binding && !mixed_bitwise &&
       (operator_choice.user_defined == 0 || class_operand))
      return EmitOperatorCall(OperatorFunctionName(op), operator_arguments, scope);
    if(op == "&&" || op == "||" || op == "and" || op == "or") return EmitLogicalValue(node, scope);
    if(op == ",") {
      EmitDiscard(node->children[0], scope);
      return EmitValue(node->children[1], scope);
    }
    ExprInfo info = Infer(node, scope);
    TypePtr result_type = expression_value_type(info);
    ExprInfo left_info = Infer(node->children[0], scope);
    ExprInfo right_info = Infer(node->children[1], scope);
    TypePtr left_type = expression_value_type(left_info);
    TypePtr right_type = expression_value_type(right_info);
    if(left_type && left_type->kind == TYPE_POINTER &&
       node->children[0] && node->children[0]->kind == "binary-expression" &&
       PA12Operator(node->children[0]->value) == "-" &&
       node->children[0]->children.size() >= 2) {
      const TypePtr nested_left = expression_value_type(
        Infer(node->children[0]->children[0], scope));
      const TypePtr nested_right = expression_value_type(
        Infer(node->children[0]->children[1], scope));
      if(nested_left && nested_left->kind == TYPE_POINTER && nested_right &&
         (nested_right->kind == TYPE_POINTER || nested_right->kind == TYPE_ARRAY)) {
        left_type = Fundamental("long int");
        result_type = Fundamental("long int");
      }
    }
    if((op == "+" || op == "-") &&
       ((left_type && (left_type->kind == TYPE_POINTER || left_type->kind == TYPE_ARRAY)) ||
        (right_type && (right_type->kind == TYPE_POINTER || right_type->kind == TYPE_ARRAY)))) {
      if(op == "-" && left_type && right_type && left_type->kind == TYPE_POINTER &&
         (right_type->kind == TYPE_POINTER || right_type->kind == TYPE_ARRAY)) {
        Value left = EmitValue(node->children[0], scope);
        Value right = EmitValue(node->children[1], scope);
        const string difference = new_temp();
        AddInstruction(difference + " = binary sub ptr " + left.operand + ", " + right.operand);
        string result = difference;
        const size_t element_size = type_size(left_type->child);
        if(element_size != 1) {
          result = new_temp();
          AddInstruction(result + " = binary div i64 " + difference + ", " +
            integer_text(static_cast<long long>(element_size)));
        }
        Value value;
        // Pointer subtraction yields ptrdiff_t even when the analyzer has
        // retained the pointer-shaped expression type for a dependent AST.
        value.type = Fundamental("long int");
        value.operand = result;
        return value;
      }
      Value value;
      value.type = result_type;
      value.operand = EmitPointerOffset(node, scope);
      return value;
    }
	TypePtr common = ArithmeticCommonType(left_type, right_type, op);
	const auto is_conditional_operand = [](CPPGMAstNodePtr value) {
		while(value && value->kind == "parenthesized-expression" &&
			!value->children.empty()) value = value->children[0];
		return value && value->kind == "conditional-expression";
	};
	// The conditional expression computes its own arm/common type before the
	// surrounding arithmetic conversion.  Passing the binary common type into
	// it would widen its temporary slot (for example `true ? 1 : 0` to i64)
	// instead of converting the completed i32 value at this binary boundary.
	Value left_raw = EmitValue(node->children[0], scope,
		is_conditional_operand(node->children[0]) ? TypePtr() : common);
	Value right_raw = EmitValue(node->children[1], scope,
		is_conditional_operand(node->children[1]) ? TypePtr() : common);
    Value left = left_raw;
    Value right = right_raw;
    if(left_raw.known_constant && is_integral_type(left_raw.type) &&
       is_integral_type(common) && type_size(common) > type_size(left_raw.type) &&
       !is_unsigned_type(common)) {
      left.type = common;
      left.operand = integer_text(left_raw.constant);
    } else left = ConvertValue(left_raw, common);
    if(right_raw.known_constant && is_integral_type(right_raw.type) &&
       !(node->children[0] && node->children[0]->kind == "sizeof-expression") &&
       is_integral_type(common) && type_size(common) > type_size(right_raw.type) &&
       !is_unsigned_type(common)) {
      right.type = common;
      right.operand = integer_text(right_raw.constant);
    } else right = ConvertValue(right_raw, common);
    if(op == "*" && right.known_constant && right.constant == 1) {
      Value result;
      result.type = result_type;
      result.operand = new_temp();
      AddInstruction(result.operand + " = copy " + low_type(common) + " " + left.operand);
      return result;
    }
    if(op == "*" && left.known_constant && left.constant == 1) {
      Value result;
      result.type = result_type;
      result.operand = new_temp();
      AddInstruction(result.operand + " = copy " + low_type(common) + " " + right.operand);
      return result;
    }
    string binary;
    if(op == "+") binary = "add";
    else if(op == "-") binary = "sub";
    else if(op == "*") binary = "mul";
    else if(op == "/") binary = common && is_unsigned_type(common) ? "udiv" : "div";
    else if(op == "%") binary = common && is_unsigned_type(common) ? "umod" : "mod";
    else if(op == "&" || op == "bitand") binary = "and";
    else if(op == "|") binary = "or";
    else if(op == "^") binary = "xor";
    else if(op == "<<") binary = "shl";
    else if(op == ">>") binary = common && is_unsigned_type(common) ? "ushr" : "shr";
    else throw logic_error("unsupported binary operator");
    Value result;
    result.type = result_type;
    result.operand = new_temp();
    AddInstruction(result.operand + " = binary " + binary + " " + low_type(common) +
      " " + left.operand + ", " + right.operand);
    return result;
  }

PA14Lowerer::Value PA14Lowerer::EmitCall(const CPPGMAstNodePtr& node, Scope* scope)
{
    if(node && !node->children.empty() && node->children[0] &&
       node->children[0]->kind == "member-expression" &&
       node->children[0]->children.size() >= 2 &&
       !node->children[0]->children[1]->value.empty() &&
       node->children[0]->children[1]->value[0] == '~') {
      ExprInfo object = Infer(node->children[0]->children[0], scope);
      TypePtr object_type = expression_value_type(object);
      if(object_type && object_type->kind == TYPE_POINTER)
        object_type = type_value(object_type->child);
      if(!object_type || object_type->kind != TYPE_CLASS) {
        (void)EmitValue(node->children[0]->children[0], scope);
        Value result;
        result.type = Fundamental("void");
        return result;
      }
    }
    CallChoice choice = ChooseCall(node, scope);
    CPPGMAstNodePtr argument_list = node->children.size() > 1 ? node->children[1] : CPPGMAstNodePtr();
    vector<CPPGMAstNodePtr> arguments = argument_list ? argument_list->children : vector<CPPGMAstNodePtr>();
    return EmitChosenCall(choice, node->children[0], arguments, scope);
  }

PA14Lowerer::Value PA14Lowerer::EmitOperatorCall(
    const string& name, const vector<CPPGMAstNodePtr>& arguments, Scope* scope)
{
    CallChoice choice = ChooseOperatorCall(name, arguments, scope);
    if(!choice.binding) throw logic_error("no viable operator overload");
    return EmitChosenCall(choice, CPPGMAstNodePtr(), arguments, scope);
  }

PA14Lowerer::Value PA14Lowerer::EmitConditionalValue(const CPPGMAstNodePtr& node, Scope* scope,
                              const TypePtr& expected)
{
    ExprInfo info = Infer(node, scope, expected);
    // The conditional expression has its own common type.  `expected` is the
    // conversion target at the surrounding initialization/call boundary; it
    // must not change the type of the temporary that represents `?:` itself.
    // In particular, `value_type ? 1 : 0` remains int when assigned to an
    // unsigned value_type, and the later conversion is emitted at the store.
    TypePtr type = expression_value_type(info);
    const TypePtr expected_value = type_value(expected);
    // The semantic bool result is represented as a bool in the surrounding
    // branch/value boundary.  Keep that established normalization, while
    // leaving integral conditional arms at their own common type.
    if(expected_value && expected_value->kind == TYPE_FUNDAMENTAL &&
       expected_value->name == "bool")
      type = expected_value;
    // An array conditional decays to a pointer when it is passed to a
    // pointer parameter.  Keep the value-form lowering for that case; the
    // equal-bound array case is handled separately by EmitConditionalAddress
    // at the call boundary, where the reference-like address is required.
    if(expected_value && expected_value->kind == TYPE_POINTER &&
       node->children.size() > 2) {
      TypePtr true_type = expression_value_type(
        Infer(node->children[1], scope));
      TypePtr false_type = expression_value_type(
        Infer(node->children[2], scope));
      if(true_type && false_type && true_type->kind == TYPE_ARRAY &&
         false_type->kind == TYPE_ARRAY && true_type->bound != false_type->bound)
        type = expected_value;
    }
    // The condition of ?: is a value context.  Logical operators therefore
    // materialize their short-circuit result before selecting the arm; direct
    // statement conditions use EmitCondition and keep the branch-only form.
    ExprInfo condition_info = Infer(node->children[0], scope);
    // Keep the established branch shape for qualified static trait members;
    // their value is useful to semantic deduction but PA14 still materializes
    // the conditional expression.  A local integral-constant object is the
    // direct value-context case that must be folded (notably an aliased
    // bool_constant result).
	const bool local_constant = condition_info.known_constant && node->children[0] &&
		(node->children[0]->kind == "call-expression" ||
		 (node->children[0]->kind == "id-expression" &&
			node->children[0]->value.find("::") == string::npos));
    if(local_constant) {
      const CPPGMAstNodePtr selected = condition_info.constant != 0 ?
        node->children[1] : node->children[2];
      Value result = EmitValue(selected, scope, type);
      return ConvertValue(result, type);
    }
    const string slot = new_special_slot("cond", low_type(type));
    const string then_label = new_label("cond_then");
    const string else_label = new_label("cond_else");
    const string end_label = new_label("cond_end");
    const size_t condition_temporary_mark = state_ ?
      state_->temporary_objects.size() : 0;
    const bool previous_condition_defer = state_ &&
      state_->defer_temporary_cleanup;
    const unsigned int previous_condition_cleanup_depth = state_ ?
      state_->condition_cleanup_depth : 0;
    if(state_) state_->defer_temporary_cleanup = true;
    TypePtr condition_type = expression_value_type(condition_info);
    Value condition = condition_type && condition_type->kind == TYPE_CLASS &&
      FindContextConversionOperator(condition_type, true, true) ?
      EmitContextConversion(node->children[0], scope, true, true) :
      EmitValue(node->children[0], scope);
    if(state_ && state_->pending_call_unwind)
      FinishPendingCallUnwind(scope);
    if(condition.lvalue && condition.type) {
      condition.operand = emit_load(condition.operand, condition.type);
      condition.lvalue = false;
    }
    condition_type = type_value(condition.type);
    if(is_floating_type(condition_type))
      condition.operand = EmitTruthValue(condition);
    const bool defer_condition_branch = state_ &&
      state_->constructor_unwind_active &&
      !state_->constructor_unwind_call &&
      state_->constructor_unwind_end.empty();
    if(defer_condition_branch) {
      // A conditional pointer argument owns the EH dispatch while its
      // condition is evaluated.  Close that dispatch after the condition
      // value has been emitted so the condition blocks receive the same
      // labels as the ordinary conditional lowering.
      state_->constructor_unwind_end = new_label("call_unwind_end");
      FinishConstructorUnwind(scope);
    }
    Terminate("branch " + condition.operand + ", ^" + then_label + ", ^" + else_label);
    if(state_) state_->condition_cleanup_depth = 2;
    AddBlock(then_label);
    Value when_true = EmitValue(node->children[1], scope, type);
    when_true = ConvertValue(when_true, type);
    emit_store(type, when_true.operand, "$" + slot);
    if(!state_->current->terminated) Terminate("jump ^" + end_label);
    AddBlock(else_label);
    Value when_false = EmitValue(node->children[2], scope, type);
    when_false = ConvertValue(when_false, type);
    emit_store(type, when_false.operand, "$" + slot);
    if(!state_->current->terminated) Terminate("jump ^" + end_label);
    AddBlock(end_label);
    Value result;
    result.type = type;
    result.operand = emit_load("$" + slot, type);
    if(state_ && state_->temporary_objects.size() > condition_temporary_mark)
      EmitTemporaryDestructors(condition_temporary_mark, scope);
    if(state_) {
      state_->defer_temporary_cleanup = previous_condition_defer;
      state_->condition_cleanup_depth = previous_condition_cleanup_depth;
    }
    return result;
  }

string PA14Lowerer::EmitConditionalAddress(const CPPGMAstNodePtr& node, Scope* scope)
{
    ExprInfo info = Infer(node, scope);
    TypePtr type = expression_value_type(info);
    const string slot = new_special_slot("condaddr", "ptr");
    const string then_label = new_label("condaddr_then");
    const string else_label = new_label("condaddr_else");
    const string end_label = new_label("condaddr_end");
    ExprInfo condition_info = Infer(node->children[0], scope);
    TypePtr condition_type = expression_value_type(condition_info);
    const bool defer_condition_branch = state_ &&
      state_->constructor_unwind_active &&
      !state_->constructor_unwind_call &&
      state_->constructor_unwind_end.empty();
    if(defer_condition_branch) {
      Value condition = condition_type && condition_type->kind == TYPE_CLASS &&
        FindContextConversionOperator(condition_type, true, true) ?
        EmitContextConversion(node->children[0], scope, true, true) :
        EmitValue(node->children[0], scope);
      if(condition.lvalue && condition.type) {
        condition.operand = emit_load(condition.operand, condition.type);
        condition.lvalue = false;
      }
      condition_type = type_value(condition.type);
      if(is_floating_type(condition_type))
        condition.operand = EmitTruthValue(condition);
      state_->constructor_unwind_end = new_label("call_unwind_end");
      FinishConstructorUnwind(scope);
      Terminate("branch " + condition.operand + ", ^" + then_label + ", ^" + else_label);
    } else if(condition_type && condition_type->kind == TYPE_POINTER) {
      Value condition = EmitValue(node->children[0], scope);
      Terminate("branch " + condition.operand + ", ^" + then_label + ", ^" + else_label);
    } else EmitCondition(node->children[0], scope, then_label, else_label);
    TypePtr target_type = expression_value_type(info);
    AddBlock(then_label);
    const ExprInfo true_info = Infer(node->children[1], scope);
    const TypePtr true_value_type = expression_value_type(true_info);
    string true_address_raw;
    if(true_value_type && target_type && true_value_type->kind == TYPE_CLASS &&
       target_type->kind == TYPE_CLASS && !PA12SameType(true_value_type, target_type, false) &&
       FindConversionOperator(true_value_type, target_type, false))
      true_address_raw = EmitConversionOperator(node->children[1], scope, target_type, false).operand;
    else true_address_raw = EmitAddress(node->children[1], scope);
    TypePtr true_type = expression_value_type(Infer(node->children[1], scope));
    const string true_address = true_type && target_type &&
      true_type->kind == TYPE_CLASS && target_type->kind == TYPE_CLASS &&
      IsDerivedFrom(true_type, target_type) ?
      AdjustBaseAddress(true_address_raw, true_type, target_type) : true_address_raw;
    emit_store(PointerTo(Fundamental("char")), true_address, "$" + slot);
    Terminate("jump ^" + end_label);
    AddBlock(else_label);
    const ExprInfo false_info = Infer(node->children[2], scope);
    const TypePtr false_value_type = expression_value_type(false_info);
    string false_address_raw;
    if(false_value_type && target_type && false_value_type->kind == TYPE_CLASS &&
       target_type->kind == TYPE_CLASS && !PA12SameType(false_value_type, target_type, false) &&
       FindConversionOperator(false_value_type, target_type, false))
      false_address_raw = EmitConversionOperator(node->children[2], scope, target_type, false).operand;
    else false_address_raw = EmitAddress(node->children[2], scope);
    TypePtr false_type = expression_value_type(Infer(node->children[2], scope));
    const string false_address = false_type && target_type &&
      false_type->kind == TYPE_CLASS && target_type->kind == TYPE_CLASS &&
      IsDerivedFrom(false_type, target_type) ?
      AdjustBaseAddress(false_address_raw, false_type, target_type) : false_address_raw;
    emit_store(PointerTo(Fundamental("char")), false_address, "$" + slot);
    Terminate("jump ^" + end_label);
    AddBlock(end_label);
    (void)type;
    return emit_load("$" + slot, PointerTo(Fundamental("char")));
  }
string PA14Lowerer::EmitLogicalRightTruth(const CPPGMAstNodePtr& node, Scope* scope)
{
    CPPGMAstNodePtr value_node = node;
    while(value_node && value_node->kind == "parenthesized-expression" &&
          !value_node->children.empty()) value_node = value_node->children[0];
    Value value = EmitValue(value_node, scope);
    if(value_node && (value_node->kind == "binary-expression" &&
       (PA12Operator(value_node->value) == "&&" || PA12Operator(value_node->value) == "||" ||
        PA12Operator(value_node->value) == "and" || PA12Operator(value_node->value) == "or"))) {
      const string temp = new_temp();
      AddInstruction(temp + " = cmp ne i64 " + value.operand + ", 0");
      return temp;
    }
    if(value_node && (value_node->kind == "binary-expression" &&
       (PA12Operator(value_node->value) == "==" || PA12Operator(value_node->value) == "!=" ||
        PA12Operator(value_node->value) == "<" || PA12Operator(value_node->value) == ">" ||
        PA12Operator(value_node->value) == "<=" || PA12Operator(value_node->value) == ">="))) {
      const string temp = new_temp();
      AddInstruction(temp + " = cmp ne i64 " + value.operand + ", 0");
      return temp;
    }
    if(value.type && (is_integral_type(value.type) ||
                      (type_value(value.type)->kind == TYPE_FUNDAMENTAL &&
                       type_value(value.type)->name == "bool"))) {
      const string temp = new_temp();
      AddInstruction(temp + " = cmp ne i64 " + value.operand + ", 0");
      return temp;
    }
    return EmitTruthValue(value);
  }
PA14Lowerer::Value PA14Lowerer::EmitValue(const CPPGMAstNodePtr& node, Scope* scope,
                  const TypePtr& expected)
{
    if(!node) throw logic_error("missing value during LowIR lowering");
    Value initializer_list_value;
    if(EmitInitializerListValue(node, scope, expected, &initializer_list_value)) return initializer_list_value;
    if(node->kind == "throw-expression") {
      EmitThrow(node->children.empty() ? CPPGMAstNodePtr() : node->children[0], scope);
      return Value();
    }
    if(IsTypeidExpression(node)) return EmitTypeidExpression(node, scope);
    if(node->kind == "lambda-expression") {
      Value closure_result;
      if(EmitLambdaClosureValue(node, scope, expected, &closure_result))
        return closure_result;
      FunctionRecord* function = EnsureLambdaFunction(node, scope);
      Value result;
      result.type = function->source_type;
      result.operand = function_address(function);
      result.function = true;
      return result;
    }
    if(node->kind == "literal") {
      if(is_user_defined_string_literal(node->value)) {
        const string operator_name = "operator\"\"" +
          string_literal_suffix(node->value);
        CPPGMAstNodePtr callee(new CPPGMAstNode("id-expression", operator_name));
        CPPGMAstNodePtr arguments(new CPPGMAstNode("argument-list"));
        arguments->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
          "literal", string_literal_core(node->value))));
        arguments->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
          "literal", integer_text(static_cast<long long>(
            decode_string_literal(node->value).size() - 1)))));
        CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
        call->children.push_back(callee);
        call->children.push_back(arguments);
        return EmitCall(call, scope);
      }
      Value result;
      if(!node->value.empty() && node->value[0] == '"') {
        result.type = ArrayOf(-1, Fundamental("char"));
        result.array = true;
        result.operand = EmitAddress(node, scope);
        return result;
      }
      bool known = false;
      long long constant = 0;
      result.operand = canonical_literal(node->value, &result.type, &constant, &known);
      result.known_constant = known;
      result.constant = constant;
      return result;
    }
    if(node->kind == "keyword-literal") {
      const string op = PA12Operator(node->value);
      if(op == "this") {
        CPPGMAstNodePtr this_id(new CPPGMAstNode("id-expression", "this"));
        return EmitIdentifier(this_id, scope, expected);
      }
      return InferKeyword(node).type->name == "nullptr_t" ?
        ValueWithNullptr() : ValueFromInfo(InferKeyword(node));
    }
    if(node->kind == "id-expression") {
      if(expected && type_value(expected) && type_value(expected)->kind != TYPE_CLASS) {
        ExprInfo source = Infer(node, scope);
        if(expression_value_type(source) && expression_value_type(source)->kind == TYPE_CLASS &&
           FindConversionOperator(expression_value_type(source), expected, false)) {
          Value converted = EmitConversionOperator(node, scope, expected, false);
          if(converted.lvalue && converted.type) {
            converted.operand = emit_load(converted.operand, converted.type);
            converted.lvalue = false;
          }
          return ConvertValue(converted, expected, false, true);
        }
      }
      return EmitIdentifier(node, scope, expected);
    }
    if(node->kind == "parenthesized-expression")
      return node->children.empty() ? Value() : EmitValue(node->children[0], scope, expected);
    if(node->kind == "new-expression") return EmitNewExpression(node, scope, expected);
    if(node->kind == "delete-expression") return EmitDeleteExpression(node, scope);
    if(node->kind == "call-expression") {
      TypePtr builtin_type = node->children.empty() ? TypePtr() :
        BuiltinCastType(node->children[0], scope);
      if(builtin_type) {
        if(node->children.size() >= 2 && node->children[1] &&
           node->children[1]->children.empty()) {
          // Functional notation with no initializer value is value
          // initialization for a scalar (`T()`).  Template substitution
          // exposes this form frequently when a return type becomes a
          // fundamental type; it is not a one-argument cast.
          Value result;
          result.type = builtin_type;
          result.operand = builtin_type->kind == TYPE_POINTER ? "nullptr" :
            is_floating_type(builtin_type) ?
              (low_type(builtin_type) == "f32" ? "0.0f" :
                low_type(builtin_type) == "f80" ? "0.0L" : "0.0") : "0";
          result.known_constant = true;
          result.constant = 0;
          return result;
        }
        if(node->children.size() < 2 || !node->children[1] ||
           node->children[1]->children.size() != 1)
          throw logic_error("built-in cast requires one argument");
        CPPGMAstNodePtr argument = node->children[1]->children[0];
        if(argument && argument->kind == "braced-init-list") {
          if(argument->children.size() != 1)
            throw logic_error("built-in cast list-initializer has multiple elements");
          argument = argument->children[0];
        }
        ExprInfo argument_info = Infer(argument, scope);
        Value value;
        if(expression_value_type(argument_info) &&
           expression_value_type(argument_info)->kind == TYPE_CLASS &&
           FindConversionOperator(expression_value_type(argument_info), builtin_type, true)) {
          value = EmitConversionOperator(argument, scope, builtin_type, true);
          if(value.lvalue && value.type) {
            value.operand = emit_load(value.operand, value.type);
            value.lvalue = false;
          }
        } else value = EmitValue(argument, scope, builtin_type);
        if(value.known_constant && is_integral_type(value.type) &&
           is_integral_type(builtin_type) &&
           !(type_value(builtin_type)->kind == TYPE_FUNDAMENTAL &&
             type_value(builtin_type)->name == "bool")) {
          value.type = builtin_type;
          value.operand = integer_text(value.constant);
          return value;
        }
        return ConvertValue(value, builtin_type, true);
      }
      TypePtr constructor_type = node->children.empty() ? TypePtr() :
        ConstructorObjectType(node->children[0], scope);
      if(constructor_type) {
        const string slot = new_special_slot("arg", low_type(constructor_type));
        const string address = new_temp();
        AddInstruction(address + " = addr $" + slot);
        const CPPGMAstNodePtr argument_list = node->children.size() > 1 ?
          node->children[1] : CPPGMAstNodePtr();
        vector<CPPGMAstNodePtr> arguments = argument_list ?
          argument_list->children : vector<CPPGMAstNodePtr>();
        if(node->value == "braced-construction" && arguments.size() == 1 &&
           arguments[0] && arguments[0]->kind == "braced-init-list")
          arguments = arguments[0]->children;
        if(constructor_type->kind == TYPE_ARRAY) {
          CPPGMAstNodePtr aggregate(new CPPGMAstNode("braced-init-list"));
          aggregate->children = arguments;
          EmitAggregateAt(address, constructor_type, aggregate, scope);
          Value result;
          result.type = constructor_type;
          result.operand = address;
          result.array = true;
          result.lvalue = true;
          return result;
        }
        if(!EmitConstructorAt(constructor_type, address, arguments, scope))
          throw logic_error("no viable functional construction");
        Value result;
        result.type = constructor_type;
        result.operand = emit_load(address, constructor_type);
        return result;
      }
      if(expected && type_value(expected) && type_value(expected)->kind != TYPE_CLASS) {
        ExprInfo source = Infer(node, scope);
        if(expression_value_type(source) && expression_value_type(source)->kind == TYPE_CLASS &&
           FindConversionOperator(expression_value_type(source), expected, false)) {
          Value converted = EmitConversionOperator(node, scope, expected, false);
          if(converted.lvalue && converted.type) {
            converted.operand = emit_load(converted.operand, converted.type);
            converted.lvalue = false;
          }
          return ConvertValue(converted, expected, false, true);
        }
      }
      Value result = EmitCall(node, scope);
      if(result.lvalue && result.type) {
        result.operand = emit_load(result.operand, result.type);
        result.lvalue = false;
      }
      return expected ? ConvertValue(result, type_value(expected), false, true) : result;
    }
    if(node->kind == "unary-expression") return EmitUnary(node, scope, expected);
    if(node->kind == "postfix-expression") {
      return EmitUpdate(node, scope, false);
    }
    if(node->kind == "binary-expression") return EmitBinary(node, scope);
    if(node->kind == "assignment-expression") return EmitAssignment(node, scope);
    if(node->kind == "conditional-expression") {
      ExprInfo info = Infer(node, scope, expected);
      if(info.category == "lvalue" && expected && type_is_reference(expected)) {
        Value result;
        result.type = info.type;
        result.operand = EmitConditionalAddress(node, scope);
        result.lvalue = true;
        return result;
      }
      return EmitConditionalValue(node, scope, expected);
    }
    if(node->kind == "subscript-expression") {
      ExprInfo info = Infer(node, scope);
      TypePtr base_type = expression_value_type(Infer(node->children[0], scope));
      if(base_type && base_type->kind == TYPE_CLASS) {
        vector<CPPGMAstNodePtr> arguments;
        arguments.push_back(node->children[1]);
        Value result = EmitCall(MakeMemberCall(node->children[0], "operator[]", arguments), scope);
        if(result.lvalue) {
          result.operand = emit_load(result.operand, result.type);
          result.lvalue = false;
        }
        return result;
      }
      Value result;
      result.type = info.type;
      result.operand = emit_load(EmitSubscriptAddress(node, scope), info.type);
      return result;
    }
    if(node->kind == "member-expression") {
      ExprInfo info = InferMember(node, scope);
      if(expected && type_value(expected) && type_value(expected)->kind != TYPE_CLASS &&
         expression_value_type(info) && expression_value_type(info)->kind == TYPE_CLASS &&
         FindConversionOperator(expression_value_type(info), expected, false)) {
        Value converted = EmitConversionOperator(node, scope, expected, false);
        if(converted.lvalue && converted.type) {
          converted.operand = emit_load(converted.operand, converted.type);
          converted.lvalue = false;
        }
        return ConvertValue(converted, expected, false, true);
      }
      Value result;
      result.type = info.type;
      if(info.binding && info.binding->kind == BIND_ENUMERATOR && info.binding->has_value) {
        result.operand = integer_text(info.binding->value);
        result.known_constant = true;
        result.constant = info.binding->value;
        return result;
      }
      if(info.binding && info.binding->is_member && info.binding->is_static &&
         info.binding->has_value) {
        EnsureStaticMemberStorage(info.binding);
        result.operand = integer_text(info.binding->value);
        result.known_constant = true;
        result.constant = info.binding->value;
        return result;
      }
      if(info.binding && info.binding->kind == BIND_FUNCTION &&
         info.binding->is_static) {
        result.operand = EmitMemberAddress(node, scope);
        result.function = true;
        const string decay = new_temp();
        AddInstruction(decay + " = unary decay ptr " + result.operand);
        result.operand = decay;
        return result;
      }
      if(info.type && info.type->kind == TYPE_ARRAY) {
        result.array = true;
        result.operand = EmitArrayDecay(node, scope);
      } else {
        const string address = EmitMemberAddress(node, scope, true);
        if(info.binding && IsBitField(info.binding)) {
          TypePtr read_type = expected ? type_value(expected) : info.type;
          result = EmitBitFieldLoad(info.binding, address, read_type,
            static_cast<bool>(expected));
        }
        else if(info.binding && type_is_reference(info.binding->type)) {
          const string referred = emit_load(address, PointerTo(Fundamental("char")));
          result.operand = emit_load(referred, info.type);
        } else result.operand = emit_load(address, info.type);
      }
      return result;
    }
    if(node->kind == "cast-expression") {
      TypePtr target = analyzer_.TypeFromTypeId(node->children[0], scope);
      Value cast_value;
      bool cast_value_emitted = false;
      if(target && PA12Operator(node->value) == "dynamic_cast")
        return EmitDynamicCast(node, scope, target);
      if(target && type_value(target) && type_value(target)->kind == TYPE_POINTER &&
         PA12Operator(node->value) == "reinterpret_cast") {
        cast_value = EmitValue(node->children[1], scope);
        cast_value_emitted = true;
        Value& source = cast_value;
        if(source.type && is_integral_type(source.type)) {
          Value result;
          result.type = target;
          result.operand = new_temp();
          AddInstruction(result.operand + " = copy ptr " + source.operand);
          return result;
        }
      }
      if(target && !type_is_reference(target) && node->children.size() > 1) {
        ExprInfo source = Infer(node->children[1], scope);
        if(expression_value_type(source) && expression_value_type(source)->kind == TYPE_CLASS &&
           FindConversionOperator(expression_value_type(source), target, true)) {
          Value converted = EmitConversionOperator(node->children[1], scope, target, true);
          if(converted.lvalue && converted.type) {
            converted.operand = emit_load(converted.operand, converted.type);
            converted.lvalue = false;
          }
          return ConvertValue(converted, target, false, true);
        }
      }
      if(type_is_reference(target)) {
        Value result;
        result.type = target->child;
        result.operand = EmitAddress(node->children[1], scope);
        result.lvalue = true;
        if(result.type && result.type->kind == TYPE_FUNCTION) {
          const string decay = new_temp();
          AddInstruction(decay + " = unary decay ptr " + result.operand);
          result.operand = decay;
          result.lvalue = false;
        } else if(result.type) {
          result.operand = emit_load(result.operand, result.type);
          result.lvalue = false;
        }
        return result;
      }
		// Keep the operand's source type visible to the explicit conversion.  An
		// expected target passed into EmitValue can normalize a same-width signed
		// operand before ConvertValue sees the cast boundary, losing the required
		// signed-to-unsigned value copy.
		Value value = cast_value_emitted ? cast_value :
			EmitValue(node->children[1], scope);
		if(state_ && state_->record && state_->record->constructor &&
		   value.known_constant && is_integral_type(value.type) &&
		   is_integral_type(target) &&
		   !(type_value(target) && type_value(target)->kind == TYPE_FUNDAMENTAL &&
		     type_value(target)->name == "bool")) {
			value.type = target;
			value.operand = integer_text(value.constant);
			return value;
		}
		// An explicit scalar cast is a value-producing conversion boundary.  In
		// particular, a dependent alias can resolve from signed to unsigned
		// types with the same LowIR width; retain the conversion copy instead of
		// letting the later return path see only the already-normalized width.
		const TypePtr source_type = type_value(value.type);
		const TypePtr target_type = type_value(target);
		const bool preserve_signedness_boundary = !value.known_constant &&
			source_type && target_type &&
			is_integral_type(source_type) && is_integral_type(target_type) &&
			type_size(source_type) == type_size(target_type) &&
			is_unsigned_type(source_type) != is_unsigned_type(target_type);
		return ConvertValue(value, target, preserve_signedness_boundary);
    }
    if(node->kind == "sizeof-pack-expression" || node->kind == "sizeof-expression" ||
       node->kind == "type-trait-expression") {
      ExprInfo info = Infer(node, scope);
      const CPPGMAstNodePtr operand = node->children.empty() ? CPPGMAstNodePtr() : node->children[0];
      if(node->kind == "sizeof-expression" && operand &&
         operand->kind == "id-expression") {
        VariablePlan* local = LocalForName(operand->value);
        if(local && type_value(local->type) &&
           type_value(local->type)->kind == TYPE_CLASS) {
          bool has_array_member = false;
          for(size_t member = 0; member < type_value(local->type)->class_members.size(); ++member) {
            TypePtr member_type = type_value(
              type_value(local->type)->class_members[member].type);
            if(!type_value(local->type)->class_members[member].is_static && member_type &&
               member_type->kind == TYPE_ARRAY) {
              has_array_member = true;
              break;
            }
          }
          if(!has_array_member &&
             (!type_value(local->type)->template_specialization ||
              type_value(local->type)->materialize_sizeof_address))
            (void)EmitAddress(operand, scope);
        }
      } else if(node->kind == "sizeof-expression" && operand &&
         operand->kind == "member-expression" && !operand->children.empty()) {
        // A member sizeof-expression needs the base object address so the
        // member projection remains visible in LowIR.  A direct sizeof(x)
        // does not: its object lifetime lowering has already emitted any
        // address that is semantically required.
        CPPGMAstNodePtr address_operand = operand->children[0];
        if(address_operand && address_operand->kind == "id-expression") {
          VariablePlan* local = LocalForName(address_operand->value);
          if(local && type_value(local->type) &&
             type_value(local->type)->kind == TYPE_CLASS)
            (void)EmitAddress(address_operand, scope);
        }
      }
      Value result;
      result.type = Fundamental("unsigned long int");
      if(node->kind == "type-trait-expression" &&
         node->value.find("NOEXCEPT") != string::npos && info.known_constant) {
        result.operand = integer_text(info.constant);
        result.known_constant = true;
        result.constant = info.constant;
        return result;
      }
      result.operand = new_temp();
      AddInstruction(result.operand + " = const i64 " + integer_text(info.constant));
      result.known_constant = true;
      result.constant = info.constant;
      return result;
    }
    if(node->kind == "braced-init-list") {
      Value result;
      result.type = expected ? type_value(expected) : Fundamental("int");
      result.operand = "0";
      result.known_constant = true;
      result.constant = 0;
      return result;
    }
    throw logic_error("unsupported value node in LowIR lowering: " + node->kind);
  }

} // namespace cppgm_pa14_lowering
