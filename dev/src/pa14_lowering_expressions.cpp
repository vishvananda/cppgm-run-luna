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

PA14Lowerer::Value PA14Lowerer::EmitIdentifier(const CPPGMAstNodePtr& node, Scope* scope,
                       const TypePtr& expected)
{
    Value result;
    VariablePlan* local = LocalForName(node->value);
    if(local) {
      if(local->type->kind == TYPE_ARRAY) {
        result.type = local->type;
        result.array = true;
        result.operand = EmitArrayDecay(node, scope);
        return result;
      }
      if(type_is_reference(local->type)) {
        TypePtr referred = local->type->child;
        const string address = local_address(local);
        if(referred && referred->kind == TYPE_FUNCTION) {
          result.type = referred;
          result.function = true;
          result.operand = address;
          const string decay = new_temp();
          AddInstruction(decay + " = unary decay ptr " + result.operand);
          result.operand = decay;
          return result;
        }
        result.type = referred;
        result.operand = emit_load(address, referred);
        return result;
      }
      if(local->parameter_address) {
        result.type = local->type;
        result.operand = local->parameter_operand;
        result.lvalue = true;
        return result;
      }
      result.type = local->type;
      result.operand = emit_load(StorageForVariable(*local), local->type);
      return result;
    }
    vector<Binding*> candidates = Lookup(node->value, scope);
    if(candidates.empty()) throw logic_error("unknown identifier during lowering");
    Binding* binding = candidates.size() == 1 ? candidates[0] : 0;
    if(!binding && candidates.size() > 1) {
      bool duplicate_declarations = true;
      for(size_t i = 1; i < candidates.size(); ++i)
        if(candidates[i]->qualified_name != candidates[0]->qualified_name ||
           !PA12SameType(candidates[i]->type, candidates[0]->type, false)) {
          duplicate_declarations = false;
          break;
        }
      if(duplicate_declarations) binding = candidates[0];
    }
    if(expected && !binding) {
      TypePtr target = type_value(expected);
      int best = 1000000;
      for(size_t i = 0; i < candidates.size(); ++i) {
        TypePtr function = function_target_type(candidates[i]->type);
        if(!function) continue;
        ExprInfo source;
        source.type = function;
        source.category = "lvalue";
        const int rank = ConversionRank(source, target);
        if(rank >= 0 && rank < best) { best = rank; binding = candidates[i]; }
      }
    }
    if(!binding && candidates.size() == 1) binding = candidates[0];
    if(!binding) throw logic_error("ambiguous identifier during lowering");
    if(!IsAccessible(binding, scope)) throw logic_error("inaccessible member");
    if(binding->kind == BIND_ENUMERATOR) {
      result.type = binding->type;
      result.operand = integer_text(binding->value);
      result.known_constant = binding->has_value;
      result.constant = binding->value;
      return result;
    }
    if(binding->is_member && binding->member_owner) {
      if(binding->kind == BIND_FUNCTION) {
        FunctionRecord* function = RecordForBinding(binding);
        if(!function) throw logic_error("unknown member function symbol during lowering");
        if(function->member) function->needed = true;
        result.type = function->type;
        result.function = true;
        result.operand = function_address(function);
        return result;
      }
      result.type = binding->type;
      if(binding->is_static) {
        if(binding->has_value) {
          result.known_constant = true;
          result.constant = binding->value;
          result.operand = integer_text(result.constant);
          return result;
        }
        GlobalRecord* global_member = FindGlobal(binding->qualified_name);
        if(!global_member) throw logic_error("unknown static member during lowering");
        result.type = global_member->type;
        result.operand = global_member->type->kind == TYPE_ARRAY ?
          EmitArrayDecay(node, scope) : emit_load("@" + global_member->symbol, global_member->type);
        result.array = global_member->type->kind == TYPE_ARRAY;
        return result;
      }
      CPPGMAstNodePtr this_node(new CPPGMAstNode("keyword-literal", "KW_THIS:this"));
      if(binding->name != node->value && node->value.find("::") != string::npos) {
        Value this_value = EmitValue(this_node, scope);
        TypePtr object = expression_value_type(Infer(this_node, scope));
        if(object && object->kind == TYPE_POINTER) object = type_value(object->child);
        string base = AdjustBaseAddress(this_value.operand, object, binding->member_owner);
        if(binding->member_index == static_cast<size_t>(-1) || !binding->member_owner ||
           binding->member_index >= binding->member_owner->class_members.size())
          throw logic_error("member has no layout record");
        const ClassMemberInfo& fact = binding->member_owner->class_members[binding->member_index];
        const string address = new_temp();
        AddInstruction(address + " = index i8 [projection=field] " + base + ", " +
          integer_text(fact.offset));
        result.type = binding->type;
        if(type_is_reference(result.type)) result.type = result.type->child;
        if(object && object->is_const && !fact.is_mutable)
          result.type = CloneWithCv(result.type, true, object->is_volatile);
        if(IsBitField(binding)) {
          TypePtr read_type = expected ? type_value(expected) : result.type;
          result = EmitBitFieldLoad(binding, address, read_type, static_cast<bool>(expected));
        } else result.operand = emit_load(address, result.type);
        result.lvalue = false;
        return result;
      }
      CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "OP_ARROW:->"));
      member->children.push_back(this_node);
      member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", binding->name)));
      ExprInfo member_info = InferMember(member, scope);
      result.type = member_info.type;
      const string address = EmitMemberAddress(member, scope);
      if(IsBitField(binding)) {
        TypePtr read_type = expected ? type_value(expected) : result.type;
        result = EmitBitFieldLoad(binding, address, read_type,
          static_cast<bool>(expected));
      }
      else
        result.operand = emit_load(address, result.type);
      result.lvalue = false;
      return result;
    }
    if(binding->kind == BIND_FUNCTION) {
      FunctionRecord* function = RecordForBinding(binding);
      if(!function) throw logic_error("unknown function symbol during lowering");
      result.type = function->type;
      result.function = true;
      result.operand = function_address(function);
      return result;
    }
    GlobalRecord* global = FindGlobal(binding->qualified_name);
    if(!global) throw logic_error("unknown global during lowering");
    result.type = global->type;
    if(global->type->kind == TYPE_ARRAY) {
      result.array = true;
      result.operand = EmitArrayDecay(node, scope);
    } else result.operand = emit_load("@" + global->symbol, global->type);
    return result;
  }

PA14Lowerer::Value PA14Lowerer::EmitUnary(const CPPGMAstNodePtr& node, Scope* scope,
                  const TypePtr& expected)
{
    const string op = PA12Operator(node->value);
    vector<CPPGMAstNodePtr> operator_arguments;
    operator_arguments.push_back(node->children[0]);
    if(ChooseOperatorCall(OperatorFunctionName(op), operator_arguments, scope).binding)
      return EmitOperatorCall(OperatorFunctionName(op), operator_arguments, scope);
    if(op == "&") {
      Value result;
      result.type = PointerTo(expression_value_type(Infer(node->children[0], scope)));
      result.operand = EmitAddress(node->children[0], scope);
      return result;
    }
    if(op == "*") {
      Value result;
      ExprInfo child_info = Infer(node->children[0], scope);
      TypePtr child_type = expression_value_type(child_info);
      if(!child_type || (child_type->kind != TYPE_POINTER && child_type->kind != TYPE_ARRAY))
        throw logic_error("invalid dereference");
      result.type = child_type->child;
      result.operand = emit_load(EmitAddress(node, scope), result.type);
      return result;
    }
    if(op == "++" || op == "--") return EmitUpdate(node, scope, false);
    Value child = EmitValue(node->children[0], scope);
    Value result;
    if(op == "+") {
      if(child.type && type_value(child.type)->kind == TYPE_ARRAY) {
        result.type = PointerTo(type_value(child.type)->child);
        result.operand = child.operand;
        return result;
      }
      result.type = IntegralPromotion(child.type);
      return ConvertValue(child, result.type);
    }
    result.type = op == "!" ? Fundamental("bool") : IntegralPromotion(child.type);
    if(op == "!") {
      TypePtr type = type_value(child.type);
      string compare_type = low_type(type);
      string zero = compare_type == "ptr" ? "nullptr" :
        is_floating_type(type) ? (compare_type == "f80" ? "0.0L" : compare_type == "f32" ? "0.0f" : "0.0") : "0";
      string operand = child.operand;
      if(type && type->kind == TYPE_FUNDAMENTAL && type->name == "bool") {
        compare_type = "i64";
      } else if(is_integral_type(type) && compare_type != "i64" &&
                compare_type != "u64") {
        // The canonical LowIR form compares an ordinary narrow integral
        // value directly against the i64 zero literal.
        compare_type = "i64";
      }
      result.operand = new_temp();
      AddInstruction(result.operand + " = cmp eq " + compare_type + " " + operand + ", " + zero);
      return result;
    }
    result.operand = new_temp();
    const string unary = op == "-" ? "neg" : op == "~" ? "bitnot" : op;
    AddInstruction(result.operand + " = unary " + unary + " " + low_type(result.type) + " " + child.operand);
    return result;
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
          assignment->needed = true;
          FunctionRecord* base_entry = BaseEntryFor(assignment);
          if(base_entry) base_entry->needed = true;
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
        const vector<Binding*> assignments =
          MemberBindings(left_probe_type, "operator=");
        if(assignments.empty()) {
          ExprInfo right_probe = Infer(node->children[1], scope);
          (void)EnsureImplicitAssignment(left_probe_type, false);
          if(right_probe.category != "lvalue")
            (void)EnsureImplicitAssignment(left_probe_type, true);
        }
      }
    }
    if(ChooseOperatorCall(OperatorFunctionName(op), operator_arguments, scope).binding)
      return EmitOperatorCall(OperatorFunctionName(op), operator_arguments, scope);
    ExprInfo left_info = Infer(node->children[0], scope);
    TypePtr left_type = expression_value_type(left_info);
    if(!left_type) throw logic_error("assignment has no target type");
    Value right;
    if(op == "=") {
      ExprInfo right_info = Infer(node->children[1], scope, left_type);
      if(ConversionRank(right_info, left_type) < 0)
        throw logic_error("invalid assignment conversion");
      right = EmitValue(node->children[1], scope, left_type);
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
      if(node->children[0] && node->children[0]->kind == "subscript-expression" &&
         right.known_constant && right_integral && target_integral) {
        right.type = left_type;
        right.operand = integer_text(right.constant);
      }
    }
    else {
      Value left = EmitValue(node->children[0], scope);
      Value raw_right = EmitValue(node->children[1], scope);
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
    string cached_address;
    Binding* cached_member = 0;
    if(child_node && (child_node->kind == "member-expression" ||
       (child_info.binding && child_info.binding->is_member &&
        !child_info.binding->is_static))) {
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
    if(ChooseOperatorCall(OperatorFunctionName(PA12Operator(node->value)),
                          operator_arguments, scope).binding)
      return EmitOperatorCall(OperatorFunctionName(PA12Operator(node->value)),
                              operator_arguments, scope);
    ExprInfo left_info = Infer(node->children[0], scope);
    ExprInfo right_info = Infer(node->children[1], scope);
    TypePtr left_type = expression_value_type(left_info);
    TypePtr right_type = expression_value_type(right_info);
    TypePtr common = CommonType(left_type, right_type, PA12Operator(node->value));
    if(left_type && right_type && left_type->kind == TYPE_POINTER && right_type->kind == TYPE_POINTER)
      common = left_type;
    const bool left_bit_field = node->children[0] &&
      node->children[0]->kind == "member-expression" &&
      IsBitField(MemberBinding(node->children[0], scope));
    const bool right_bit_field = node->children[1] &&
      node->children[1]->kind == "member-expression" &&
      IsBitField(MemberBinding(node->children[1], scope));
    Value left = left_bit_field ? EmitValue(node->children[0], scope) :
      EmitValue(node->children[0], scope, common);
    Value right = right_bit_field ? EmitValue(node->children[1], scope) :
      EmitValue(node->children[1], scope, common);
    CPPGMAstNodePtr difference = node->children[0];
    while(difference && difference->kind == "parenthesized-expression" &&
          !difference->children.empty()) difference = difference->children[0];
    const bool pointer_difference = difference && difference->kind == "binary-expression" &&
      PA12Operator(difference->value) == "-" && difference->children.size() >= 2 &&
      expression_value_type(Infer(difference->children[0], scope)) &&
      expression_value_type(Infer(difference->children[0], scope))->kind == TYPE_POINTER &&
      expression_value_type(Infer(difference->children[1], scope)) &&
      expression_value_type(Infer(difference->children[1], scope))->kind == TYPE_POINTER;
    const bool prefer_literal_common = node->children[0] &&
      node->children[0]->kind == "member-expression";
    if((pointer_difference || prefer_literal_common) && left.known_constant &&
       is_integral_type(left.type) &&
       is_integral_type(common) &&
       type_size(common) > type_size(left.type)) {
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
    if((pointer_difference || prefer_literal_common) && right.known_constant &&
       is_integral_type(right.type) &&
       is_integral_type(common) &&
       type_size(common) > type_size(right.type)) {
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
    if(ChooseOperatorCall(OperatorFunctionName(op), operator_arguments, scope).binding)
      return EmitOperatorCall(OperatorFunctionName(op), operator_arguments, scope);
    if(op == "&&" || op == "||" || op == "and" || op == "or") return EmitLogicalValue(node, scope);
    if(op == ",") {
      EmitDiscard(node->children[0], scope);
      return EmitValue(node->children[1], scope);
    }
    if(op == "==" || op == "!=" || op == "not_eq" || op == "<" ||
       op == ">" || op == "<=" || op == ">=") return EmitCompare(node, scope);
    ExprInfo info = Infer(node, scope);
    TypePtr result_type = expression_value_type(info);
    ExprInfo left_info = Infer(node->children[0], scope);
    ExprInfo right_info = Infer(node->children[1], scope);
    TypePtr left_type = expression_value_type(left_info);
    TypePtr right_type = expression_value_type(right_info);
    if((op == "+" || op == "-") &&
       ((left_type && (left_type->kind == TYPE_POINTER || left_type->kind == TYPE_ARRAY)) ||
        (right_type && (right_type->kind == TYPE_POINTER || right_type->kind == TYPE_ARRAY)))) {
      if(op == "-" && left_type && right_type && left_type->kind == TYPE_POINTER &&
         right_type->kind == TYPE_POINTER) {
        Value left = EmitValue(node->children[0], scope);
        Value right = EmitValue(node->children[1], scope);
        const string difference = new_temp();
        AddInstruction(difference + " = binary sub ptr " + left.operand + ", " + right.operand);
        const string result = new_temp();
        AddInstruction(result + " = binary div i64 " + difference + ", " +
          integer_text(static_cast<long long>(type_size(left_type->child))));
        Value value;
        value.type = result_type;
        value.operand = result;
        return value;
      }
      Value value;
      value.type = result_type;
      value.operand = EmitPointerOffset(node, scope);
      return value;
    }
    TypePtr common = CommonType(left_type, right_type, op);
    Value left_raw = EmitValue(node->children[0], scope, common);
    Value right_raw = EmitValue(node->children[1], scope, common);
    Value left = left_raw;
    Value right = right_raw;
    if(left_raw.known_constant && is_integral_type(left_raw.type) &&
       is_integral_type(common) && type_size(common) > type_size(left_raw.type)) {
      left.type = common;
      left.operand = integer_text(left_raw.constant);
    } else left = ConvertValue(left_raw, common);
    if(right_raw.known_constant && is_integral_type(right_raw.type) &&
       !(node->children[0] && node->children[0]->kind == "sizeof-expression") &&
       is_integral_type(common) && type_size(common) > type_size(right_raw.type)) {
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
    else if(op == "/") binary = "div";
    else if(op == "%") binary = "mod";
    else if(op == "&" || op == "bitand") binary = "and";
    else if(op == "|") binary = "or";
    else if(op == "^") binary = "xor";
    else if(op == "<<") binary = "shl";
    else if(op == ">>") binary = "shr";
    else throw logic_error("unsupported binary operator");
    Value result;
    result.type = result_type;
    result.operand = new_temp();
    AddInstruction(result.operand + " = binary " + binary + " " + low_type(common) +
      " " + left.operand + ", " + right.operand);
    return result;
  }

string PA14Lowerer::EmitReferenceArgument(const CPPGMAstNodePtr& node, Scope* scope,
                               const TypePtr& target)
{
    TypePtr referred = target->child;
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
    if(source.category == "lvalue" && IsDerivedFrom(source_type, referred))
      return AdjustBaseAddress(EmitAddress(node, scope), source_type, referred);
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
      if(target) value = ConvertValue(value, target, false, true);
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
    TypePtr type = expression_value_type(info);
    if(expected) type = type_value(expected);
    const string slot = new_special_slot("cond", low_type(type));
    const string then_label = new_label("cond_then");
    const string else_label = new_label("cond_else");
    const string end_label = new_label("cond_end");
    // The condition of ?: is a value context.  Logical operators therefore
    // materialize their short-circuit result before selecting the arm; direct
    // statement conditions use EmitCondition and keep the branch-only form.
    Value condition = EmitValue(node->children[0], scope);
    TypePtr condition_type = type_value(condition.type);
    if(is_floating_type(condition_type))
      condition.operand = EmitTruthValue(condition);
    Terminate("branch " + condition.operand + ", ^" + then_label + ", ^" + else_label);
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
    if(condition_type && condition_type->kind == TYPE_POINTER) {
      Value condition = EmitValue(node->children[0], scope);
      Terminate("branch " + condition.operand + ", ^" + then_label + ", ^" + else_label);
    } else EmitCondition(node->children[0], scope, then_label, else_label);
    TypePtr target_type = expression_value_type(info);
    AddBlock(then_label);
    const string true_address_raw = EmitAddress(node->children[1], scope);
    TypePtr true_type = expression_value_type(Infer(node->children[1], scope));
    const string true_address = true_type && target_type &&
      true_type->kind == TYPE_CLASS && target_type->kind == TYPE_CLASS &&
      IsDerivedFrom(true_type, target_type) ?
      AdjustBaseAddress(true_address_raw, true_type, target_type) : true_address_raw;
    emit_store(PointerTo(Fundamental("char")), true_address, "$" + slot);
    Terminate("jump ^" + end_label);
    AddBlock(else_label);
    const string false_address_raw = EmitAddress(node->children[2], scope);
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
    const string right = EmitLogicalRightTruth(node->children[1], scope);
    emit_store(Fundamental("long int"), right, "$" + slot);
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

void PA14Lowerer::EmitCondition(const CPPGMAstNodePtr& node, Scope* scope,
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
      Value value = EmitValue(node->children[1] && node->children[1]->children.size() ?
        CPPGMAstNodePtr(new CPPGMAstNode("id-expression", variable->source_name)) :
        CPPGMAstNodePtr(new CPPGMAstNode("id-expression", variable->source_name)), scope);
      string operand = value.operand;
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
        EmitCondition(node->children[0], scope, rhs_label, false_label);
        AddBlock(rhs_label);
        EmitCondition(node->children[1], scope, true_label, false_label);
        return;
      }
      if(op == "||" || op == "or") {
        const string rhs_label = new_label("lor_rhs");
        EmitCondition(node->children[0], scope, true_label, rhs_label);
        AddBlock(rhs_label);
        EmitCondition(node->children[1], scope, true_label, false_label);
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
      Value child = EmitValue(node->children[0], scope);
      TypePtr type = type_value(child.type);
      if(type && (is_integral_type(type) ||
                  (type->kind == TYPE_FUNDAMENTAL && type->name == "bool"))) {
        const string temp = new_temp();
        AddInstruction(temp + " = cmp eq i64 " + child.operand + ", 0");
        Terminate("branch " + temp + ", ^" + true_label + ", ^" + false_label);
      } else {
        const string operand = EmitTruthValue(child);
        Terminate("branch " + operand + ", ^" + false_label + ", ^" + true_label);
      }
      return;
    }
    Value value = EmitValue(node, scope);
    string operand = value.operand;
    TypePtr type = type_value(value.type);
    if(is_floating_type(type)) operand = EmitTruthValue(value);
    Terminate("branch " + operand + ", ^" + true_label + ", ^" + false_label);
  }

PA14Lowerer::Value PA14Lowerer::EmitValue(const CPPGMAstNodePtr& node, Scope* scope,
                  const TypePtr& expected)
{
    if(!node) throw logic_error("missing value during LowIR lowering");
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
    if(node->kind == "id-expression") return EmitIdentifier(node, scope, expected);
    if(node->kind == "parenthesized-expression")
      return node->children.empty() ? Value() : EmitValue(node->children[0], scope, expected);
    if(node->kind == "new-expression") return EmitNewExpression(node, scope, expected);
    if(node->kind == "delete-expression") return EmitDeleteExpression(node, scope);
    if(node->kind == "call-expression") {
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
        if(!EmitConstructorAt(constructor_type, address, arguments, scope))
          throw logic_error("no viable functional construction");
        Value result;
        result.type = constructor_type;
        result.operand = emit_load(address, constructor_type);
        return result;
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
        result.operand = integer_text(info.binding->value);
        result.known_constant = true;
        result.constant = info.binding->value;
        return result;
      }
      if(info.type && info.type->kind == TYPE_ARRAY) {
        result.array = true;
        result.operand = EmitArrayDecay(node, scope);
      } else {
        const string address = EmitMemberAddress(node, scope);
        if(info.binding && IsBitField(info.binding)) {
          TypePtr read_type = expected ? type_value(expected) : info.type;
          result = EmitBitFieldLoad(info.binding, address, read_type,
            static_cast<bool>(expected));
        }
        else result.operand = emit_load(address, info.type);
      }
      return result;
    }
    if(node->kind == "cast-expression") {
      TypePtr target = analyzer_.TypeFromTypeId(node->children[0], scope);
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
      Value value = EmitValue(node->children[1], scope, target);
      return ConvertValue(value, target);
    }
    if(node->kind == "sizeof-expression" || node->kind == "type-trait-expression") {
      ExprInfo info = Infer(node, scope);
      const CPPGMAstNodePtr operand = node->children.empty() ? CPPGMAstNodePtr() : node->children[0];
      if(node->kind == "sizeof-expression" && operand && operand->kind != "type-id" &&
         operand->kind == "id-expression") {
        VariablePlan* local = LocalForName(operand->value);
        if(local && type_value(local->type) &&
           type_value(local->type)->kind == TYPE_CLASS)
          (void)EmitAddress(operand, scope);
      }
      Value result;
      result.type = Fundamental("long int");
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
