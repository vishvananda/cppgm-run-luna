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

PA14Lowerer::Value PA14Lowerer::ConvertValue(Value value, const TypePtr& target,
                     bool immediate_return)
{
    if(!target) return value;
    TypePtr target_value = type_value(target);
    if(type_is_reference(target)) return value;
    if(!value.type || !target_value) return value;
    if(target_value->kind == TYPE_POINTER &&
       value.type->kind == TYPE_FUNDAMENTAL && value.type->name == "nullptr_t") {
      Value result;
      result.type = target_value;
      result.operand = new_temp();
      AddInstruction(result.operand + " = copy ptr nullptr");
      return result;
    }
    if(target_value->kind == TYPE_POINTER && value.type->kind == TYPE_ARRAY) {
      return value;
    }
    if(target_value->kind == TYPE_POINTER && value.type->kind == TYPE_FUNCTION) {
      Value result = value;
      result.type = target_value;
      result.operand = new_temp();
      AddInstruction(result.operand + " = unary decay ptr " + value.operand);
      return result;
    }
    const string source_low = low_type(value.type);
    const string target_low = low_type(target_value);
    if(value.type->kind == TYPE_ENUM && target_value->kind == TYPE_ENUM &&
       PA12SameType(value.type, target_value, false)) return value;
    if(value.type->kind == TYPE_ENUM && target_value->kind == TYPE_FUNDAMENTAL &&
       source_low == target_low) {
      Value result = value;
      result.type = target_value;
      if(value.known_constant) {
        result.operand = integer_text(value.constant);
        return result;
      }
      result.operand = new_temp();
      AddInstruction(result.operand + " = copy " + target_low + " " + value.operand);
      return result;
    }
    if(source_low == target_low &&
       PA12SameType(type_value(value.type), target_value, true)) {
      Value result = value;
      result.type = target_value;
      return result;
    }
    const bool source_enum = type_value(value.type) &&
      type_value(value.type)->kind == TYPE_ENUM;
    if((is_integral_type(value.type) || source_enum) && is_integral_type(target_value) &&
       type_size(value.type) == type_size(target_value) &&
       source_low != target_low) {
      Value result = value;
      result.type = target_value;
      if(value.known_constant) {
        result.operand = integer_text(value.constant);
        return result;
      }
      result.operand = new_temp();
      AddInstruction(result.operand + " = copy " + target_low + " " + value.operand);
      return result;
    }
    if(target_value->kind == TYPE_FUNDAMENTAL && target_value->name == "bool") {
      Value result;
      result.type = target_value;
      result.operand = EmitTruthValue(value);
      return result;
    }
    if(source_low == target_low) {
      Value result = value;
      result.type = target_value;
      return result;
    }
    if(target_low == "ptr" || source_low == "ptr") return value;
    Value result;
    result.type = target_value;
    result.operand = new_temp();
    string operation;
    if(is_floating_type(value.type) && is_floating_type(target_value)) {
      const size_t source_size = type_size(value.type);
      const size_t target_size = type_size(target_value);
      operation = target_size > source_size ? "fpext" : "fptrunc";
    } else if(is_floating_type(value.type) && is_integral_type(target_value)) {
      operation = is_unsigned_type(target_value) ? "fptoui" : "fptosi";
    } else if(is_integral_type(value.type) && is_floating_type(target_value)) {
      operation = is_unsigned_type(value.type) ? "uitofp" : "sitofp";
    } else {
      const size_t source_size = type_size(value.type);
      const size_t target_size = type_size(target_value);
      operation = target_size > source_size ?
        (is_unsigned_type(value.type) ? "zext" : "sext") : "trunc";
    }
    AddInstruction(result.operand + " = convert " + operation + " " + target_low +
      " " + source_low + " " + value.operand);
    (void)immediate_return;
    return result;
  }

string PA14Lowerer::EmitTruthValue(const Value& value)
{
    if(!value.type) return value.operand;
    TypePtr type = type_value(value.type);
    const string low = low_type(type);
    if(type->kind == TYPE_FUNDAMENTAL && type->name == "bool") return value.operand;
    string zero = low == "ptr" ? "nullptr" :
      (is_floating_type(type) ? (low == "f32" ? "0.0f" : low == "f80" ? "0.0L" : "0.0") : "0");
    string compare_type = low;
    if(is_integral_type(type) && low != "i64" && low != "u64") compare_type = "i64";
    string operand = value.operand;
    if(compare_type != low && low != "ptr" && !is_floating_type(type)) {
      Value widened = ConvertValue(value, Fundamental(is_unsigned_type(type) ? "unsigned long int" : "long int"));
      operand = widened.operand;
      compare_type = "i64";
    }
    const string temp = new_temp();
    AddInstruction(temp + " = cmp ne " + compare_type + " " + operand + ", " + zero);
    return temp;
  }

string PA14Lowerer::InternString(const string& raw)
{
    map<string, string>::const_iterator found = string_symbols_.find(raw);
    if(found != string_symbols_.end()) return found->second;
    const string symbol = "__strlit__" + integer_text(
      static_cast<long long>(string_order_.size() + 1));
    string_data_[raw] = decode_string_literal(raw);
    string_order_.push_back(raw);
    string_symbols_[raw] = symbol;
    return symbol;
}

bool PA14Lowerer::FoldInteger(const CPPGMAstNodePtr& node, Scope* scope,
                  long long* result, TypePtr* type)
{
    if(!node) return false;
    if(node->kind == "literal") {
      bool known = false;
      TypePtr literal_type;
      long long value = 0;
      canonical_literal(node->value, &literal_type, &value, &known);
      if(type) *type = literal_type;
      if(result) *result = value;
      return known;
    }
    if(node->kind == "keyword-literal") {
      const string op = PA12Operator(node->value);
      if(op == "nullptr") return false;
      if(type) *type = Fundamental("bool");
      if(result) *result = op == "true" ? 1 : 0;
      return true;
    }
    if(node->kind == "id-expression") {
      vector<Binding*> candidates = Lookup(node->value, scope);
      if(candidates.size() != 1 || !candidates[0]->has_value) return false;
      if(type) *type = candidates[0]->type;
      if(result) *result = candidates[0]->value;
      return true;
    }
    if(node->kind == "parenthesized-expression")
      return node->children.empty() ? false : FoldInteger(node->children[0], scope, result, type);
    if(node->kind == "cast-expression") {
      if(node->children.size() < 2) return false;
      TypePtr target = analyzer_.TypeFromTypeId(node->children[0], scope);
      long long value = 0;
      if(!FoldInteger(node->children[1], scope, &value, 0)) return false;
      if(type) *type = target;
      if(result) *result = value;
      return true;
    }
    if(node->kind == "sizeof-expression" || node->kind == "type-trait-expression") {
      TypePtr target;
      if(node->children.empty()) return false;
      if(node->children[0]->kind == "type-id") target = analyzer_.TypeFromTypeId(node->children[0], scope);
      else target = analyzer_.ExpressionType(node->children[0], scope);
      if(type) *type = Fundamental("unsigned long int");
      if(result) *result = node->kind == "type-trait-expression" ?
        static_cast<long long>(type_alignment(target)) : static_cast<long long>(type_size(target));
      return true;
    }
    if(node->kind == "unary-expression") {
      long long child = 0;
      TypePtr child_type;
      if(node->children.empty() || !FoldInteger(node->children[0], scope, &child, &child_type)) return false;
      const string op = PA12Operator(node->value);
      if(op == "+") {}
      else if(op == "-") child = -child;
      else if(op == "!") child = !child;
      else if(op == "~") child = ~child;
      else return false;
      if(type) *type = op == "!" ? Fundamental("bool") : child_type;
      if(result) *result = child;
      return true;
    }
    if(node->kind == "conditional-expression" && node->children.size() == 3) {
      long long condition = 0;
      if(!FoldInteger(node->children[0], scope, &condition, 0)) return false;
      return FoldInteger(node->children[condition ? 1 : 2], scope, result, type);
    }
    if((node->kind == "binary-expression" || node->kind == "assignment-expression") &&
       node->children.size() >= 2) {
      long long left = 0, right = 0;
      TypePtr left_type, right_type;
      if(!FoldInteger(node->children[0], scope, &left, &left_type) ||
         !FoldInteger(node->children[1], scope, &right, &right_type)) return false;
      const string op = PA12Operator(node->value);
      if(op == "+") left += right;
      else if(op == "-") left -= right;
      else if(op == "*") left *= right;
      else if(op == "/") { if(right == 0) return false; left /= right; }
      else if(op == "%") { if(right == 0) return false; left %= right; }
      else if(op == "==") left = left == right;
      else if(op == "!=") left = left != right;
      else if(op == "<") left = left < right;
      else if(op == ">") left = left > right;
      else if(op == "<=") left = left <= right;
      else if(op == ">=") left = left >= right;
      else if(op == "&&" || op == "and") left = left && right;
      else if(op == "||" || op == "or") left = left || right;
      else if(op == "&" || op == "bitand") left &= right;
      else if(op == "|") left |= right;
      else if(op == "^") left ^= right;
      else if(op == "<<") left <<= right;
      else if(op == ">>") left >>= right;
      else if(op == ",") left = right;
      else return false;
      if(type) *type = (op == "==" || op == "!=" || op == "<" || op == ">" ||
        op == "<=" || op == ">=" || op == "&&" || op == "||") ?
        Fundamental("bool") : (op == "," ? right_type : left_type);
      if(result) *result = left;
      return true;
    }
    return false;
  }

PA14Lowerer::AddressInit PA14Lowerer::StaticAddress(const CPPGMAstNodePtr& expression, Scope* scope)
{
    AddressInit result;
    if(!expression) return result;
    CPPGMAstNodePtr node = expression;
    if(node->kind == "parenthesized-expression" && !node->children.empty())
      return StaticAddress(node->children[0], scope);
    if(node->kind == "cast-expression" && node->children.size() > 1)
      return StaticAddress(node->children[1], scope);
    if(node->kind == "literal" && !node->value.empty() && node->value[0] == '"') {
      result.valid = true;
      result.symbol = InternString(node->value);
      return result;
    }
    if(node->kind == "keyword-literal" && PA12Operator(node->value) == "nullptr") return result;
    if(node->kind == "unary-expression" && PA12Operator(node->value) == "&" &&
       !node->children.empty()) return StaticAddress(node->children[0], scope);
    if(node->kind == "id-expression") {
      vector<Binding*> candidates = Lookup(node->value, scope);
      if(candidates.size() != 1) return result;
      Binding* binding = candidates[0];
      if(binding->kind == BIND_FUNCTION) {
        FunctionRecord* function = RecordForBinding(binding);
        result.valid = true;
        result.function = true;
        result.symbol = function ? function->symbol : low_symbol_component(binding->qualified_name);
        return result;
      }
      GlobalRecord* global = FindGlobal(binding->qualified_name);
      if(global) {
        result.valid = true;
        result.symbol = global->symbol;
        return result;
      }
      return result;
    }
    if(node->kind == "binary-expression" && node->children.size() >= 2) {
      const string op = PA12Operator(node->value);
      if(op == "+" || op == "-") {
        AddressInit base = StaticAddress(node->children[0], scope);
        long long offset = 0;
        if(base.valid && FoldInteger(node->children[1], scope, &offset, 0)) {
          ExprInfo left = Infer(node->children[0], scope);
          TypePtr element = type_value(left.type);
          if(element && element->kind == TYPE_ARRAY) element = element->child;
          else if(element && element->kind == TYPE_POINTER) element = element->child;
          base.addend += (op == "+" ? offset : -offset) * static_cast<long long>(type_size(element));
          return base;
        }
      }
    }
    return result;
  }

string PA14Lowerer::GlobalMetadata(bool internal) const
{
    return internal ? " [binding=internal]" : " [binding=strong]";
  }

string PA14Lowerer::RenderStringGlobal(const string& symbol, const vector<unsigned char>& bytes) const
{
    ostringstream out;
    out << "global @" << symbol << " [binding=internal] = {\n";
    for(size_t i = 0; i < bytes.size(); ++i) out << "  i8 " << static_cast<unsigned int>(bytes[i]) << "\n";
    out << "}";
    return out.str();
  }

string PA14Lowerer::RenderGlobal(GlobalRecord& global)
{
    TypePtr type = global.type;
    CPPGMAstNodePtr expression = InitializerExpression(global.initializer);
    ostringstream out;
    TypePtr value_type = type_value(type);
    if(value_type && value_type->kind == TYPE_CLASS && !expression) {
      out << "global @" << global.symbol << GlobalMetadata(global.internal) << " = {\n";
      out << "  zero " << integer_text(static_cast<long long>(type_size(type))) << "\n";
      out << "}";
      return out.str();
    }
    if(type->kind == TYPE_ARRAY) {
      out << "global @" << global.symbol << GlobalMetadata(global.internal) << " = {\n";
      TypePtr element = type->child;
      vector<GlobalDataItem> items;
      if(expression && expression->kind == "literal" && !expression->value.empty() && expression->value[0] == '"' &&
         element->kind == TYPE_FUNDAMENTAL && element->name == "char") {
        const vector<unsigned char> bytes = decode_string_literal(expression->value);
        for(size_t i = 0; i < bytes.size(); ++i) items.push_back(GlobalDataItem("i8 " + integer_text(bytes[i])));
      } else if(expression && expression->kind == "braced-init-list") {
        for(size_t i = 0; i < expression->children.size(); ++i) {
          CPPGMAstNodePtr item = expression->children[i];
          if(element->kind == TYPE_POINTER) {
            AddressInit address = StaticAddress(item, global.scope);
            if(address.valid && !address.function) {
              string text = "ptr addr @" + address.symbol;
              if(address.addend) text += " + " + integer_text(address.addend);
              items.push_back(GlobalDataItem(text));
            } else if(item && item->kind == "keyword-literal" && PA12Operator(item->value) == "nullptr")
              items.push_back(GlobalDataItem("zero 8"));
            else if(item && item->kind == "literal" && item->value == "0")
              items.push_back(GlobalDataItem("zero 8"));
            else items.push_back(GlobalDataItem("zero 8"));
          } else {
            long long value = 0;
            TypePtr source;
            if(FoldInteger(item, global.scope, &value, &source))
              items.push_back(GlobalDataItem(low_type(element) + " " + integer_text(value)));
            else items.push_back(GlobalDataItem(low_type(element) + " 0"));
          }
        }
      }
      if(!expression) {
        // An omitted initializer is one zero-initialized aggregate, rather
        // than a presentation-level list of one zero item per element.
        items.push_back(GlobalDataItem("zero " + integer_text(static_cast<long long>(type_size(type)))));
      } else {
        const size_t count = type->bound < 0 ? items.size() : static_cast<size_t>(type->bound);
        while(items.size() < count) {
          const size_t bytes = type_size(element);
          items.push_back(GlobalDataItem(element->kind == TYPE_POINTER || element->kind == TYPE_FUNCTION ?
            "zero " + integer_text(static_cast<long long>(bytes)) : low_type(element) + " 0"));
        }
        if(items.empty() && type_size(type) != 0)
          items.push_back(GlobalDataItem("zero " + integer_text(static_cast<long long>(type_size(type)))));
      }
      for(size_t i = 0; i < items.size(); ++i) out << "  " << items[i].text << "\n";
      out << "}";
      return out.str();
    }

    const string low = storage_type(type);
    out << "global @" << global.symbol << " : " << low << GlobalMetadata(global.internal) << " = ";
    AddressInit address = StaticAddress(expression, global.scope);
    if(type_value(type)->kind == TYPE_POINTER && address.valid && !address.function) {
      out << "addr @" << address.symbol;
      if(address.addend) out << " + " << address.addend;
    } else {
      if(address.function) global.dynamic_initializer = true;
      long long value = 0;
      TypePtr source;
      if(expression && FoldInteger(expression, global.scope, &value, &source)) out << integer_text(value);
      else if(expression && expression->kind == "literal" && !expression->value.empty() &&
              expression->value[0] != '"') out << canonical_literal(expression->value);
      else out << "zero";
    }
    return out.str();
  }

void PA14Lowerer::EmitGlobals(vector<string>& entries)
{
    vector<string> rendered;
    for(size_t i = 0; i < globals_.size(); ++i) rendered.push_back(RenderGlobal(globals_[i]));
    for(size_t i = 0; i < string_order_.size(); ++i) {
      const string symbol = "__strlit__" + integer_text(static_cast<long long>(i + 1));
      entries.push_back(RenderStringGlobal(symbol, string_data_[string_order_[i]]));
    }
    for(size_t i = 0; i < rendered.size(); ++i) entries.push_back(rendered[i]);
  }

void PA14Lowerer::EmitDeclarations(vector<string>& entries)
{
    for(size_t i = 0; i < functions_.size(); ++i) {
      FunctionRecord& function = functions_[i];
      if(function.definition) continue;
      ostringstream out;
      out << "declare function @" << function.symbol << "(";
      for(size_t p = 0; p < function.type->parameters.size(); ++p) {
        if(p != 0) out << ", ";
        out << "%arg" << p << " : " << low_type(function.type->parameters[p]);
        if(type_is_reference(function.type->parameters[p])) out << " [pass=reference]";
      }
      out << ") -> " << low_type(function.type->child);
      if(function.variadic) out << " [arity=variadic]";
      entries.push_back(out.str());
    }
  }

Scope* PA14Lowerer::FunctionScope() const
{
    map<const CPPGMAstNode*, Scope*>::const_iterator found =
      analyzer_.function_scopes_.find(state_->record->node.get());
    return found == analyzer_.function_scopes_.end() ? state_->record->scope : found->second;
  }

PA14Lowerer::VariablePlan* PA14Lowerer::BindPlan(const CPPGMAstNodePtr& declarator)
{
    if(!declarator) return 0;
    map<const CPPGMAstNode*, VariablePlan*>::iterator found = state_->plans.find(declarator.get());
    if(found == state_->plans.end()) return 0;
    state_->environments.back()[found->second->source_name] = found->second;
    return found->second;
  }

void PA14Lowerer::BindSimpleDeclaration(const CPPGMAstNodePtr& node)
{
    CPPGMAstNodePtr list = ChildOfKind(node, "init-declarator-list");
    if(!list) return;
    for(size_t i = 0; i < list->children.size(); ++i)
      if(list->children[i] && !list->children[i]->children.empty())
        BindPlan(list->children[i]->children[0]);
  }

PA14Lowerer::VariablePlan* PA14Lowerer::BindCondition(const CPPGMAstNodePtr& condition)
{
    if(!condition || condition->kind != "condition-declaration" || condition->children.size() < 2)
      return 0;
    return BindPlan(condition->children[1]);
  }

void PA14Lowerer::EnterEnvironment()
{ state_->environments.push_back(map<string, VariablePlan*>()); }

void PA14Lowerer::LeaveEnvironment()
{ state_->environments.pop_back(); }

string PA14Lowerer::emit_load(const string& address, const TypePtr& type)
{
    const string temp = new_temp();
    AddInstruction(temp + " = load " + low_type(type) + " " + address);
    return temp;
  }

void PA14Lowerer::emit_store(const TypePtr& type, const string& value, const string& storage)
{
    AddInstruction("store " + low_type(type) + " " + value + ", " + storage);
  }

string PA14Lowerer::local_address(VariablePlan* variable)
{
    if(!variable) throw logic_error("missing local variable");
    if(type_is_reference(variable->type)) return emit_load(StorageForVariable(*variable), PointerTo(Fundamental("char")));
    const string temp = new_temp();
    AddInstruction(temp + " = addr " + StorageForVariable(*variable));
    return temp;
  }

string PA14Lowerer::global_address(GlobalRecord* global)
{
    if(!global) throw logic_error("missing global variable");
    const string temp = new_temp();
    AddInstruction(temp + " = addr @" + global->symbol);
    return temp;
  }

string PA14Lowerer::function_address(FunctionRecord* function)
{
    if(!function) throw logic_error("missing function symbol");
    const string temp = new_temp();
    AddInstruction(temp + " = addr @" + function->symbol);
    return temp;
  }

string PA14Lowerer::EmitArrayDecay(const CPPGMAstNodePtr& node, Scope* scope)
{
    ExprInfo info = Infer(node, scope);
    TypePtr type = expression_value_type(info);
    if(type && type->kind == TYPE_POINTER) return EmitValue(node, scope).operand;
    if(type && type->kind == TYPE_FUNCTION) return EmitValue(node, scope).operand;
    const string address = EmitAddress(node, scope);
    if(node && node->kind == "literal") return address;
    if(node && node->kind == "conditional-expression") return address;
    if(node && node->kind == "parenthesized-expression" && node->children.size() == 1 &&
       node->children[0] && node->children[0]->kind == "conditional-expression") return address;
    const string temp = new_temp();
    AddInstruction(temp + " = unary decay ptr " + address);
    return temp;
  }

string PA14Lowerer::EmitSubscriptAddress(const CPPGMAstNodePtr& node, Scope* scope)
{
    if(!node || node->children.size() < 2) throw logic_error("invalid subscript");
    ExprInfo first = Infer(node->children[0], scope);
    ExprInfo second = Infer(node->children[1], scope);
    CPPGMAstNodePtr base_node = node->children[0];
    CPPGMAstNodePtr index_node = node->children[1];
    TypePtr base_type = expression_value_type(first);
    if(!base_type || (base_type->kind != TYPE_ARRAY && base_type->kind != TYPE_POINTER)) {
      base_node = node->children[1];
      index_node = node->children[0];
      base_type = expression_value_type(second);
    }
    if(!base_type || (base_type->kind != TYPE_ARRAY && base_type->kind != TYPE_POINTER))
      throw logic_error("subscript base is not an array or pointer");
    TypePtr element = base_type->child;
    string base = base_type->kind == TYPE_ARRAY ? EmitArrayDecay(base_node, scope) :
      EmitValue(base_node, scope).operand;
    Value index = EmitValue(index_node, scope);
    string text = new_temp();
    AddInstruction(text + " = index " + low_type(element) +
      " [projection=array_element] " + base + ", " + index.operand);
    return text;
  }

string PA14Lowerer::EmitPointerOffset(const CPPGMAstNodePtr& node, Scope* scope)
{
    ExprInfo left_info = Infer(node->children[0], scope);
    ExprInfo right_info = Infer(node->children[1], scope);
    CPPGMAstNodePtr pointer_node = node->children[0];
    CPPGMAstNodePtr offset_node = node->children[1];
    TypePtr pointer_type = expression_value_type(left_info);
    bool subtract = PA12Operator(node->value) == "-";
    if(!pointer_type || (pointer_type->kind != TYPE_POINTER && pointer_type->kind != TYPE_ARRAY)) {
      pointer_node = node->children[1];
      offset_node = node->children[0];
      pointer_type = expression_value_type(right_info);
      subtract = false;
    }
    if(pointer_type && pointer_type->kind == TYPE_ARRAY) {
      // The array operand is converted to a pointer before the offset is scaled.
      string base = EmitArrayDecay(pointer_node, scope);
      Value offset = EmitValue(offset_node, scope);
      const string scale = new_temp();
      AddInstruction(scale + " = binary mul i64 " + offset.operand + ", " +
        integer_text(static_cast<long long>(type_size(pointer_type->child))));
      string scaled = scale;
      if(subtract) {
        const string neg = new_temp();
        AddInstruction(neg + " = binary sub i64 0, " + scale);
        scaled = neg;
      }
      const string result = new_temp();
      AddInstruction(result + " = index i8 " + base + ", " + scaled);
      return result;
    }
    string base = EmitValue(pointer_node, scope).operand;
    Value offset = EmitValue(offset_node, scope);
    const long long size = pointer_type && pointer_type->kind == TYPE_POINTER ?
      static_cast<long long>(type_size(pointer_type->child)) : 1;
    string scaled;
    if(size == 1 && !subtract) scaled = offset.operand;
    else {
      const string scale = new_temp();
      AddInstruction(scale + " = binary mul i64 " + offset.operand + ", " + integer_text(size));
      scaled = scale;
      if(subtract) {
        const string neg = new_temp();
        AddInstruction(neg + " = binary sub i64 0, " + scale);
        scaled = neg;
      }
    }
    const string result = new_temp();
    AddInstruction(result + " = index i8 " + base + ", " + scaled);
    return result;
  }

string PA14Lowerer::EmitAddress(const CPPGMAstNodePtr& node, Scope* scope)
{
    if(!node) throw logic_error("missing lvalue");
    if(node->kind == "parenthesized-expression")
      return node->children.empty() ? string() : EmitAddress(node->children[0], scope);
    if(node->kind == "id-expression") {
      VariablePlan* local = LocalForName(node->value);
      if(local) {
        if(!local->initialization_address.empty()) {
          const string address = local->initialization_address;
          local->initialization_address.clear();
          return address;
        }
        return local_address(local);
      }
      vector<Binding*> candidates = Lookup(node->value, scope);
      if(candidates.empty()) throw logic_error("unknown address expression");
      Binding* binding = candidates.size() == 1 ? candidates[0] : candidates[0];
      if(binding->kind == BIND_FUNCTION) {
        FunctionRecord* function = RecordForBinding(binding);
        if(!function) {
          for(size_t i = 0; i < functions_.size(); ++i)
            if(functions_[i].qualified_name == binding->qualified_name) {
              function = const_cast<FunctionRecord*>(&functions_[i]);
              break;
            }
        }
        return function_address(function);
      }
      GlobalRecord* global = FindGlobal(binding->qualified_name);
      if(global) return global_address(global);
      if(binding->is_member && !binding->is_static) {
        CPPGMAstNodePtr this_node(new CPPGMAstNode("keyword-literal", "KW_THIS:this"));
        CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "OP_ARROW:->"));
        member->children.push_back(this_node);
        member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", binding->name)));
        return EmitMemberAddress(member, scope);
      }
      throw logic_error("cannot take address of expression");
    }
    if(node->kind == "member-expression") return EmitMemberAddress(node, scope);
    if(node->kind == "literal" && !node->value.empty() && node->value[0] == '"') {
      const string symbol = InternString(node->value);
      const string temp = new_temp();
      AddInstruction(temp + " = addr @" + symbol);
      return temp;
    }
    if(node->kind == "unary-expression") {
      const string op = PA12Operator(node->value);
      if(op == "*") return EmitValue(node->children[0], scope).operand;
      if(op == "++" || op == "--") {
        EmitUpdate(node, scope, true);
        return EmitAddress(node->children[0], scope);
      }
    }
    if(node->kind == "subscript-expression") return EmitSubscriptAddress(node, scope);
    if(node->kind == "conditional-expression") return EmitConditionalAddress(node, scope);
    if(node->kind == "binary-expression" && PA12Operator(node->value) == ",") {
      EmitDiscard(node->children[0], scope);
      return EmitAddress(node->children[1], scope);
    }
    if(node->kind == "assignment-expression") {
      EmitAssignment(node, scope);
      return EmitAddress(node->children[0], scope);
    }
    if(node->kind == "cast-expression" && node->children.size() > 1) {
      TypePtr target = analyzer_.TypeFromTypeId(node->children[0], scope);
      if(type_is_reference(target)) return EmitAddress(node->children[1], scope);
    }
    if(node->kind == "call-expression") {
      ExprInfo info = Infer(node, scope);
      if(type_is_reference(info.type)) return EmitCall(node, scope).operand;
    }
    throw logic_error("expression is not addressable");
  }

string PA14Lowerer::EmitMemberAddress(const CPPGMAstNodePtr& node, Scope* scope)
{
    ExprInfo object_info;
    Binding* member = MemberBinding(node, scope, &object_info);
    if(!member) throw logic_error("unknown member");
    if(member->kind == BIND_FUNCTION)
      throw logic_error("member function is not an lvalue");
    if(member->is_static) {
      GlobalRecord* global = FindGlobal(member->qualified_name);
      if(!global) throw logic_error("static member has no storage");
      return global_address(global);
    }
    if(member->member_index == static_cast<size_t>(-1) || !member->member_owner ||
       member->member_index >= member->member_owner->class_members.size())
      throw logic_error("member has no layout record");
    const ClassMemberInfo& fact = member->member_owner->class_members[member->member_index];
    string base;
    const string op = PA12Operator(node->value);
    if(op == "->") {
      TypePtr object = expression_value_type(object_info);
      if(!object || object->kind != TYPE_POINTER) throw logic_error("arrow requires a pointer to class");
      object = type_value(object->child);
      base = EmitValue(node->children[0], scope).operand;
    } else {
      TypePtr object = expression_value_type(object_info);
      base = EmitAddress(node->children[0], scope);
      object = type_value(object);
      if(object && object->kind == TYPE_POINTER) object = type_value(object->child);
    }
    TypePtr object = expression_value_type(object_info);
    if(op == "->") object = object && object->kind == TYPE_POINTER ?
      type_value(object->child) : TypePtr();
    else if(object && object->kind == TYPE_POINTER) object = type_value(object->child);
    base = AdjustBaseAddress(base, object, member->member_owner);
    const string result = new_temp();
    AddInstruction(result + " = index i8 [projection=field] " + base + ", " +
      integer_text(fact.offset));
    return result;
  }

string PA14Lowerer::AdjustBaseAddress(const string& base, const TypePtr& raw_derived,
                                      const TypePtr& target)
{
    TypePtr derived = type_value(raw_derived);
    TypePtr wanted = type_value(target);
    if(!derived || !wanted || PA12SameType(derived, wanted, true)) return base;
    if(derived->kind != TYPE_CLASS || wanted->kind != TYPE_CLASS)
      throw logic_error("member owner is not a base class");
    if(!derived->direct_base) throw logic_error("member owner is not a base class");
    const string adjusted = new_temp();
    AddInstruction(adjusted + " = index i8 [projection=base_subobject] " + base + ", 0");
    return AdjustBaseAddress(adjusted, derived->direct_base, wanted);
  }

} // namespace cppgm_pa14_lowering
