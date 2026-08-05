#include "pa14_lowering.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <iomanip>
#include <functional>
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
string NormalizeFloatingLiteral(const string& raw, const TypePtr& target)
{
    string spelling = raw;
    const size_t marker = spelling.find(':');
    if(marker != string::npos && spelling.substr(0, marker) == "TT_LITERAL")
      spelling.erase(0, marker + 1);
    if(spelling.empty()) return spelling;
    const char last = spelling[spelling.size() - 1];
    if(last == 'f' || last == 'F' || last == 'l' || last == 'L')
      spelling.erase(spelling.size() - 1);
    const size_t exponent = spelling.find_first_of("eE");
    const size_t mantissa_end = exponent == string::npos ? spelling.size() : exponent;
    const size_t dot = spelling.find('.', 0);
    if(dot != string::npos && dot < mantissa_end) {
      size_t end = mantissa_end;
      while(end > dot + 1 && spelling[end - 1] == '0') --end;
      if(end == dot + 1) --end;
      spelling.erase(end, mantissa_end - end);
    }
    TypePtr value_type = type_value(target);
    if(value_type && value_type->kind == TYPE_FUNDAMENTAL) {
      if(value_type->name == "float") spelling += "f";
      else if(value_type->name == "long double") spelling += "L";
    }
    return spelling;
  }
bool FloatingConstantText(Analyzer& analyzer, const CPPGMAstNodePtr& expression,
                          Scope* scope, const TypePtr& target, string* text)
{
    if(!expression || !target || !is_floating_type(target)) return false;
    if(expression->kind == "literal" &&
       (expression->value.find('.') != string::npos ||
        expression->value.find_first_of("eEpP") != string::npos)) {
      if(text) *text = NormalizeFloatingLiteral(expression->value, target);
      return true;
    }
    ConstantValue value;
    try {
      value = analyzer.Evaluate(expression, scope);
    } catch(...) {
      return false;
    }
    if(!value.floating_known) return false;
    ostringstream out;
    out << setprecision(18) << value.floating;
    TypePtr value_type = type_value(target);
    if(value_type && value_type->kind == TYPE_FUNDAMENTAL) {
      if(value_type->name == "float") out << "f";
      else if(value_type->name == "long double") out << "L";
    }
    if(text) *text = out.str();
    return true;
  }
bool IntegralConstantValue(Analyzer& analyzer, const CPPGMAstNodePtr& expression,
                           Scope* scope, const TypePtr& target, long long* value)
{
    TypePtr target_value = type_value(target);
    if(!expression || !target || !(is_integral_type(target) ||
       (target_value && target_value->kind == TYPE_FUNDAMENTAL &&
        target_value->name == "bool"))) return false;
    ConstantValue constant;
    try {
      constant = analyzer.EvaluateTyped(expression, scope, target);
    } catch(...) {
      return false;
    }
    if(!constant.integral.known) return false;
    if(value) *value = PA19Signed(constant.integral);
    return true;
  }
} // namespace
PA14Lowerer::Value PA14Lowerer::ConvertValue(Value value, const TypePtr& target,
                     bool immediate_return, bool adjust_derived_pointer)
{
    if(!target) return value;
    TypePtr target_value = type_value(target);
    if(type_is_reference(target)) return value;
    if(!value.type || !target_value) return value;
    if(target_value->kind == TYPE_FUNDAMENTAL &&
       target_value->name == "nullptr_t" &&
       ((value.known_constant && value.constant == 0) ||
        (value.type->kind == TYPE_FUNDAMENTAL && value.type->name == "nullptr_t"))) {
      value.type = target_value;
      return value;
    }
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
    if(target_value->kind == TYPE_POINTER && value.type->kind == TYPE_POINTER &&
       value.type->child && target_value->child &&
       adjust_derived_pointer && IsDerivedFrom(value.type->child, target_value->child)) {
      Value result = value;
      result.type = target_value;
      result.operand = AdjustBaseAddress(value.operand,
        value.type->child, target_value->child);
      return result;
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
       source_low == target_low && target_low == "i64") {
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
      if(!(value.type->kind == TYPE_FUNDAMENTAL && value.type->name == "bool")) {
        const string copied = new_temp();
        AddInstruction(copied + " = copy u8 " + result.operand);
        result.operand = copied;
      }
      return result;
    }
    if(source_low == target_low) {
      Value result = value;
      result.type = target_value;
      if(type_value(value.type)->kind == TYPE_FUNDAMENTAL &&
         target_value->kind == TYPE_FUNDAMENTAL &&
         is_integral_type(value.type) && is_integral_type(target_value) &&
         !is_unsigned_type(value.type) && is_unsigned_type(target_value) &&
         !PA12SameType(type_value(value.type), target_value, false) &&
         immediate_return &&
         !value.known_constant) {
        result.operand = new_temp();
        AddInstruction(result.operand + " = copy " + target_low + " " + value.operand);
      } else if(value.type->kind == TYPE_ENUM && target_value->kind == TYPE_FUNDAMENTAL &&
         target_low == "i64") {
        result.operand = new_temp();
        AddInstruction(result.operand + " = copy " + target_low + " " + value.operand);
      }
      return result;
    }
    if(source_low == "ptr" && is_integral_type(target_value)) {
      Value result = value;
      result.type = target_value;
      result.operand = new_temp();
      AddInstruction(result.operand + " = copy " + target_low + " " + value.operand);
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
    string zero = low == "ptr" ? "0" :
      (is_floating_type(type) ? (low == "f32" ? "0.0f" : low == "f80" ? "0.0L" : "0.0") : "0");
    string compare_type = low;
    if(is_integral_type(type) && low != "i32" && low != "i64" && low != "u64")
      compare_type = "i64";
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
    const string core = string_literal_core(raw);
    map<string, string>::const_iterator found = string_symbols_.find(core);
    if(found != string_symbols_.end()) return found->second;
    const string symbol = "__strlit__" + integer_text(
      static_cast<long long>(string_order_.size() + 1));
    string_data_[core] = decode_string_literal(core);
    string_order_.push_back(core);
    string_symbols_[core] = symbol;
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
      const bool decltype_form = node->value.compare(0, 9, "decltype(") == 0;
      Binding* qualified_member = ResolveDecltypeStaticMember(node->value, scope);
      vector<Binding*> candidates = qualified_member ? vector<Binding*>(1, qualified_member) : Lookup(node->value, scope);
      if(qualified_member && decltype_form) return false;
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
    if(node->kind == "sizeof-pack-expression") {
      if(type) *type = Fundamental("unsigned long int");
      if(result) *result = node->value.empty() ? 0 : atoll(node->value.c_str());
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
	// The semantic analyzer owns the typed constant representation.  Use it
	// for expression forms that the legacy LowIR folder does not spell out
	// (notably functional integral casts such as short(42)).
	const ConstantValue semantic = analyzer_.Evaluate(node, scope);
	if(!semantic.integral.known) return false;
	if(result) *result = semantic.value;
	if(type) {
		Analyzer::SpecFacts facts;
		*type = analyzer_.ResolveSpelledType(semantic.integral.type.name, scope, facts);
	}
	return true;
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
    if(node->kind == "literal" && !node->value.empty() &&
       node->value.find('"') != string::npos) {
      result.valid = true;
      result.symbol = InternString(node->value);
      return result;
    }
    if(node->kind == "keyword-literal" && PA12Operator(node->value) == "nullptr") return result;
    if(node->kind == "lambda-expression") {
      FunctionRecord* function = EnsureLambdaFunction(node, scope);
      MarkFunctionNeeded(function);
      result.valid = true;
      result.function = true;
      result.symbol = function->symbol;
      return result;
    }
    if(node->kind == "unary-expression" && PA12Operator(node->value) == "&" &&
       !node->children.empty()) return StaticAddress(node->children[0], scope);
    if(node->kind == "id-expression") {
      const bool decltype_form = node->value.compare(0, 9, "decltype(") == 0;
      Binding* qualified_member = ResolveDecltypeStaticMember(node->value, scope);
      vector<Binding*> candidates = qualified_member ?
        vector<Binding*>(1, qualified_member) : Lookup(node->value, scope);
      if(candidates.size() != 1) return result;
      Binding* binding = candidates[0];
      if(binding->kind == BIND_FUNCTION) {
        FunctionRecord* function = RecordForBinding(binding);
        if(function) MarkFunctionNeeded(function);
        result.valid = true;
        result.function = true;
        result.symbol = function ? function->symbol : low_symbol_component(binding->qualified_name);
        return result;
      }
      GlobalRecord* global = FindGlobal(binding->qualified_name);
      if(!global && binding->is_member && binding->is_static)
        global = EnsureStaticMemberStorage(binding,
          decltype_form);
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
string PA14Lowerer::GlobalMetadata(const GlobalRecord& global) const
{
    const string binding = global.weak_binding ? "weak" :
      (global.internal ? "internal" : "strong");
    const string object = global.object_name.empty() ? global.symbol : global.object_name;
    if(global.tls_guard)
      return " [storage=thread_local, binding=" + binding + "]";
    if(global.local_static)
      return " [binding=" + binding + "]";
    if(global.thread_local_storage)
      return " [storage=thread_local, binding=" +
        binding + ", object=" + object + "]";
    return " [binding=" + binding + ", object=" + object + "]";
  }
string PA14Lowerer::RenderStringGlobal(const string& symbol, const string& raw,
                                       const vector<unsigned char>& bytes) const
{
    ostringstream out;
    out << "global @" << symbol << " [binding=internal] = {\n";
    string element_type = "i8";
    if(raw.compare(0, 2, "u8") == 0) element_type = "i8";
    else if(!raw.empty() && raw[0] == 'u') element_type = "i16";
    else if(!raw.empty() && (raw[0] == 'U' || raw[0] == 'L')) element_type = "i32";
    for(size_t i = 0; i < bytes.size(); ++i)
      out << "  " << element_type << " " << static_cast<unsigned int>(bytes[i]) << "\n";
    out << "}";
    return out.str();
  }
string PA14Lowerer::RenderGlobal(GlobalRecord& global)
{
    TypePtr type = global.type;
    CPPGMAstNodePtr expression = InitializerExpression(global.initializer);
    ostringstream out;
    TypePtr value_type = type_value(type);
    const bool enum_function_style_initializer = value_type &&
      value_type->kind == TYPE_ENUM && global.initializer &&
      !global.initializer->children.empty() && global.initializer->children[0] &&
      global.initializer->children[0]->kind == "paren-initializer";
    if(enum_function_style_initializer) {
      out << "global @" << global.symbol << GlobalMetadata(global) << " = {\n";
      out << "  zero " << integer_text(static_cast<long long>(type_size(type))) << "\n";
      out << "}";
      return out.str();
    }
    if(!type_is_reference(type) && value_type && value_type->kind == TYPE_CLASS) {
      out << "global @" << global.symbol << GlobalMetadata(global) << " = {\n";
      out << "  zero " << integer_text(static_cast<long long>(type_size(type))) << "\n";
      out << "}";
      return out.str();
    }
    if(type->kind == TYPE_ARRAY) {
      out << "global @" << global.symbol << GlobalMetadata(global) << " = {\n";
      TypePtr element = type->child;
      TypePtr element_value = type_value(element);
      if(!global.local_static && global.internal && element_value &&
         (element_value->kind == TYPE_CLASS || element_value->kind == TYPE_ARRAY) &&
         expression &&
         (Analyzer::HasNodeValue(global.node, "decl-specifier", "constexpr") ||
          Analyzer::HasNodeValue(global.node, "specifier", "constexpr"))) {
        vector<GlobalDataItem> constant_items;
        ConstantValue constant;
        try {
          constant = analyzer_.EvaluateTyped(expression, global.scope, type);
        } catch(...) {
          constant = ConstantValue();
        }
        if(constant.object && AppendConstantGlobalData(type, constant, constant_items)) {
          for(size_t i = 0; i < constant_items.size(); ++i)
            out << "  " << constant_items[i].text << "\n";
          out << "}";
          return out.str();
        }
      }
      if(element_value && (element_value->kind == TYPE_CLASS ||
                           element_value->kind == TYPE_ARRAY)) {
        out << "  zero " << integer_text(static_cast<long long>(type_size(type))) << "\n";
        out << "}";
        return out.str();
      }
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
            if(address.valid) {
              string text = "ptr addr @" + address.symbol;
              if(address.addend) text += " + " + integer_text(address.addend);
              items.push_back(GlobalDataItem(text));
            } else if(item && item->kind == "keyword-literal" && PA12Operator(item->value) == "nullptr")
              items.push_back(GlobalDataItem("zero 8"));
            else if(item && item->kind == "literal" && item->value == "0")
              items.push_back(GlobalDataItem("zero 8"));
            else items.push_back(GlobalDataItem("zero 8"));
          } else {
            TypePtr element_value = type_value(element);
            string floating;
            if(element_value && is_floating_type(element_value) &&
               FloatingConstantText(analyzer_, item, global.scope, element_value, &floating))
              items.push_back(GlobalDataItem(low_type(element) + " " + floating));
            else {
              long long value = 0;
              TypePtr source;
              if(FoldInteger(item, global.scope, &value, &source))
                items.push_back(GlobalDataItem(low_type(element) + " " + integer_text(value)));
              else items.push_back(GlobalDataItem(low_type(element) + " 0"));
            }
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
    out << "global @" << global.symbol << " : " << low << GlobalMetadata(global) << " = ";
    AddressInit address = StaticAddress(expression, global.scope);
    const bool lambda_function_address = address.function && expression &&
      expression->kind == "lambda-expression";
    if(type_value(type)->kind == TYPE_POINTER && address.valid &&
       (!address.function || lambda_function_address)) {
      out << "addr @" << address.symbol;
      if(address.addend) out << " + " << address.addend;
    } else {
      long long value = 0;
      TypePtr source;
      const bool folded = expression && FoldInteger(expression, global.scope, &value, &source);
      long long semantic_value = 0;
      const bool folded_semantic = value_type &&
        IntegralConstantValue(analyzer_, expression, global.scope, value_type, &semantic_value);
      string floating;
      const bool folded_floating = value_type && is_floating_type(value_type) &&
        FloatingConstantText(analyzer_, expression, global.scope, value_type, &floating);
      if(folded_floating) out << floating;
      else if(folded_semantic) out << integer_text(semantic_value);
      else if(address.function || (expression && !folded && !folded_semantic &&
         !(type_value(type)->kind == TYPE_POINTER && address.valid))) {
        global.dynamic_initializer = true;
        needs_init_helper_ = true;
        out << "zero";
      }
      else if(folded) out << integer_text(value);
      else if(expression && expression->kind == "literal" && !expression->value.empty() &&
              expression->value[0] != '"') out << canonical_literal(expression->value);
      else out << "zero";
    }
    return out.str();
  }
void PA14Lowerer::EmitGlobals(vector<string>& entries, size_t begin, bool include_strings)
{
    // Declarations are emitted after function lowering has discovered which
    // declaration-only globals occur in evaluated expressions.  The initial
    // pass still renders definitions and string data, but its declarations
    // must wait; otherwise an unevaluated template probe would be
    // indistinguishable from a runtime reference at this point.
    const size_t declaration_begin = include_strings ? globals_.size() : 0;
    for(size_t i = declaration_begin; i < globals_.size(); ++i) {
      GlobalRecord& global = globals_[i];
      if(!global.declaration || !global.referenced) continue;
      if(global.type && global.type->kind == TYPE_ARRAY && global.type->bound <= 0)
        continue;
      ostringstream declaration;
      declaration << "declare global @" << global.symbol;
      TypePtr value_type = type_value(global.type);
      if(!value_type || value_type->kind != TYPE_CLASS)
        declaration << " : " << low_type(global.type);
      declaration << " [";
      if(global.thread_local_storage) declaration << "storage=thread_local, ";
      declaration << "binding=" << (global.weak_binding ? "weak" :
        (global.internal ? "internal" : "strong")) <<
        ", object=" << (global.object_name.empty() ? global.symbol : global.object_name) << "]";
      entries.push_back(declaration.str());
      if(global.thread_local_storage) {
        const string wrapper = global.symbol + "__tls_wrapper";
        ostringstream wrapper_declaration;
        wrapper_declaration << "declare function @" << wrapper <<
          "() -> ptr [binding=" << (global.tls_guard ? "internal" : "strong");
        if(!global.tls_guard) wrapper_declaration << ", object=" << wrapper;
        wrapper_declaration << ", tls_for=@" << global.symbol << "]";
        entries.push_back(wrapper_declaration.str());
      }
    }
    // A thread-local definition also has a TLS address wrapper.  Definitions
    // are rendered below, but the wrapper is a declaration and must precede
    // the rendered global just like the declaration-only case above.
    for(size_t i = declaration_begin; i < globals_.size(); ++i) {
      GlobalRecord& global = globals_[i];
      if(global.declaration || !global.thread_local_storage) continue;
      const string wrapper = global.symbol + "__tls_wrapper";
      ostringstream wrapper_declaration;
        wrapper_declaration << "declare function @" << wrapper <<
        "() -> ptr [binding=" << (global.tls_guard ? "internal" : "strong");
      if(!global.tls_guard) wrapper_declaration << ", object=" << wrapper;
      wrapper_declaration << ", tls_for=@" << global.symbol << "]";
      entries.push_back(wrapper_declaration.str());
    }
    vector<string> rendered;
    for(size_t i = begin; i < globals_.size(); ++i)
      if(!globals_[i].declaration) rendered.push_back(RenderGlobal(globals_[i]));
	if(begin != 0 || !include_strings) {
      for(size_t i = 0; i < rendered.size(); ++i) entries.push_back(rendered[i]);
      return;
    }
    for(size_t i = 0; i < string_order_.size(); ++i) {
      const string symbol = "__strlit__" + integer_text(static_cast<long long>(i + 1));
      entries.push_back(RenderStringGlobal(symbol, string_order_[i],
        string_data_[string_order_[i]]));
    }
    for(size_t i = 0; i < rendered.size(); ++i) entries.push_back(rendered[i]);
  }
void PA14Lowerer::EmitDeclarations(vector<string>& entries)
{
    for(size_t i = 0; i < functions_.size(); ++i) {
      FunctionRecord& function = functions_[i];
      if(function.definition || !function.needed) continue;
      if(function.member && function.member_owner && function.source_type) {
        const TypePtr owner = type_value(function.member_owner);
        const TypePtr source = function_target_type(function.source_type);
        bool pure_declaration = false;
        if(owner && source) for(size_t slot = 0; slot < owner->virtual_methods.size(); ++slot) {
          const VirtualMethodInfo& method = owner->virtual_methods[slot];
          if(!method.pure || method.name != LastComponent(function.qualified_name) ||
             !method.function || method.function->parameters.size() != source->parameters.size())
            continue;
          bool same_parameters = true;
          for(size_t parameter = 0; parameter < source->parameters.size(); ++parameter)
            if(TypeText(method.function->parameters[parameter], true) !=
               TypeText(source->parameters[parameter], true)) {
              same_parameters = false;
              break;
            }
          if(same_parameters) { pure_declaration = true; break; }
        }
        if(pure_declaration) continue;
      }
      ostringstream out;
      out << "declare function @" << function.symbol << "(";
      for(size_t p = 0; p < function.type->parameters.size(); ++p) {
        if(p != 0) out << ", ";
        const size_t source_parameter = function.indirect_result && p > 0 ? p - 1 : p;
        out << "%" << (function.indirect_result && p == 0 ? "ret" :
          string("arg") + integer_text(static_cast<long long>(source_parameter))) << " : " <<
          low_type(function.type->parameters[p]);
        if(type_is_reference(function.type->parameters[p])) out << " [pass=reference]";
        else if(function.indirect_result && p == 0) out << " [pass=indirect_result]";
        if(p < function.parameter_metadata.size() &&
           !function.parameter_metadata[p].empty())
          out << " [" << function.parameter_metadata[p] << "]";
      }
      out << ") -> " << low_type(function.type->child);
      vector<string> metadata;
      if(function.variadic) metadata.push_back("arity=variadic");
      if(!function.effects.empty()) metadata.push_back("effects=" + function.effects);
      if(function.unwind_no) metadata.push_back("unwind=no");
      if(function.qualified_name == "__external_runtime____cxa_bad_typeid")
        metadata.push_back("unwind=may");
      if(function.qualified_name == "__external_runtime____cxa_bad_cast")
        metadata.push_back("unwind=may");
      if(function.qualified_name == "__external_runtime___Unwind_Resume")
        metadata.push_back("role=eh_resume");
      if(function.qualified_name == "__external_runtime____cxa_allocate_exception")
        metadata.push_back("role=eh_allocate_exception");
      if(function.qualified_name == "__external_runtime____cxa_begin_catch")
        metadata.push_back("role=eh_begin_catch");
      if(function.qualified_name == "__external_runtime____cxa_end_catch")
        metadata.push_back("role=eh_end_catch");
      if(function.qualified_name == "__external_runtime____cxa_rethrow")
        metadata.push_back("role=eh_rethrow");
      if(function.qualified_name == "__external_runtime____cxa_throw")
        metadata.push_back("role=eh_throw");
      if(function.qualified_name == "__external_runtime____gxx_personality_v0")
        metadata.push_back("role=eh_personality");
      if(function.noreturn) metadata.push_back("return=noreturn");
      if(function.qualified_name == "__external_runtime____dynamic_cast" ||
         function.qualified_name == "__external_runtime____cxa_bad_typeid" ||
         function.qualified_name == "__external_runtime____cxa_bad_cast" ||
         function.qualified_name == "__external_runtime___Unwind_Resume" ||
         function.qualified_name == "__external_runtime____cxa_allocate_exception" ||
         function.qualified_name == "__external_runtime____cxa_begin_catch" ||
         function.qualified_name == "__external_runtime____cxa_end_catch" ||
         function.qualified_name == "__external_runtime____cxa_rethrow" ||
         function.qualified_name == "__external_runtime____cxa_throw" ||
         function.qualified_name == "__external_runtime____gxx_personality_v0")
        metadata.push_back("linkage=c");
      metadata.push_back(function.weak_binding ? "binding=weak" : "binding=strong");
      const string object = function.qualified_name == "__cxa_pure_virtual" ?
        string() : (function.object_name.empty() ? function.symbol : function.object_name);
      if(!object.empty()) metadata.push_back("object=" + object);
      if(!metadata.empty()) {
        out << " [";
        for(size_t m = 0; m < metadata.size(); ++m) {
          if(m != 0) out << ", ";
          out << metadata[m];
        }
        out << "]";
      }
      entries.push_back(out.str());
    }
  }
Scope* PA14Lowerer::FunctionScope() const
{
    map<const CPPGMAstNode*, Scope*>::const_iterator found =
      analyzer_.function_scopes_.find(state_->record->node.get());
    Scope* function_scope = found == analyzer_.function_scopes_.end() ?
      state_->record->scope : found->second;
    if(state_->record->node) {
      for(size_t i = 0; i < state_->record->node->children.size(); ++i) {
        CPPGMAstNodePtr child = state_->record->node->children[i];
        if(!child || child->kind != "compound-statement") continue;
        map<const CPPGMAstNode*, Scope*>::const_iterator block =
          analyzer_.compound_scopes_.find(child.get());
        if(block != analyzer_.compound_scopes_.end()) return block->second;
      }
    }
    return function_scope;
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
    VariablePlan* variable = BindPlan(condition->children[1]);
    // The condition declaration is entered before its condition value is
    // emitted.  Reserve its ordinary slot at that point so any temporary
    // created while evaluating the initializer is listed afterwards.
    if(variable && !variable->slot_declared) {
      variable->slot_declared = true;
      state_->slot_order.push_back(FunctionState::SlotEntry(
        false, variable->slot_name, variable));
    }
    return variable;
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
    if(variable->global) {
      if(variable->global->local_static && type_is_reference(variable->type))
        return emit_load("@" + variable->global->symbol,
          PointerTo(Fundamental("char")));
      const string address = global_address(variable->global);
      return address;
    }
    if(variable->parameter_address) return variable->parameter_operand;
    if(state_ && state_->return_slot_plan == variable) {
      const vector<string> names = ParameterNames(*state_->record);
      if(names.empty()) throw logic_error("indirect result has no destination");
      return "%" + names[0];
    }
    if(type_is_reference(variable->type)) return emit_load(StorageForVariable(*variable), PointerTo(Fundamental("char")));
    const string temp = new_temp();
    AddInstruction(temp + " = addr " + StorageForVariable(*variable));
    return temp;
  }
string PA14Lowerer::global_address(GlobalRecord* global)
{
    if(!global) throw logic_error("missing global variable");
    global->referenced = true;
    const string temp = new_temp();
    AddInstruction(temp + " = addr @" + global->symbol);
    return temp;
  }
void PA14Lowerer::EnsureThreadLocalGuard(GlobalRecord* object)
{
    if(!object || !object->thread_local_storage || !object->dynamic_initializer)
      return;
    const string name = object->qualified_name + "__tls_guard";
    if(global_by_key_.find(global_key(name)) != global_by_key_.end()) return;
    GlobalRecord guard;
    guard.node = object->node;
    guard.scope = object->scope;
    guard.type = Fundamental("long int");
    guard.qualified_name = name;
    guard.declaration = false;
    guard.internal = true;
    guard.thread_local_storage = true;
    guard.tls_guard = true;
    globals_.push_back(guard);
    global_by_key_[global_key(name)] = &globals_.back();
  }
string PA14Lowerer::function_address(FunctionRecord* function)
{
    if(!function) throw logic_error("missing function symbol");
    MarkFunctionNeeded(function);
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
    // A call returning an array reference is represented in LowIR by the
    // pointer returned by that call.  The semantic expression still has
    // array type, but applying a second decay would add an observable,
    // redundant `unary decay ptr` before a subscript.
    if(type && type->kind == TYPE_ARRAY && node && node->kind == "call-expression") {
      Value value = EmitCall(node, scope);
      if(value.lvalue && value.type && type_value(value.type) &&
         type_value(value.type)->kind == TYPE_ARRAY)
        return value.operand;
    }
    // A subscript of an array of arrays already produces the address of the
    // selected inner array.  Decaying that address a second time only adds a
    // redundant LowIR unary operation before the next subscript.
    if(type && type->kind == TYPE_ARRAY && node &&
       node->kind == "subscript-expression")
      return EmitAddress(node, scope);
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
    if(!base_type || (base_type->kind != TYPE_ARRAY && base_type->kind != TYPE_POINTER &&
                      base_type->kind != TYPE_CLASS)) {
      base_node = node->children[1];
      index_node = node->children[0];
      base_type = expression_value_type(second);
    }
    if(base_type && base_type->kind == TYPE_CLASS) {
      vector<CPPGMAstNodePtr> arguments;
      arguments.push_back(index_node);
      if(!MemberBindings(base_type, "operator[]").empty()) {
        Value result = EmitCall(MakeMemberCall(base_node, "operator[]", arguments), scope);
        if(!result.lvalue) throw logic_error("subscript result is not addressable");
        return result.operand;
      }
      Binding* conversion = FindContextConversionOperator(base_type, false, false);
      TypePtr function = conversion ? function_target_type(conversion->type) : TypePtr();
      TypePtr pointer = function ? type_value(function->child) : TypePtr();
      if(!pointer || pointer->kind != TYPE_POINTER)
        throw logic_error("subscript base has no pointer conversion");
      Value converted = EmitContextConversion(base_node, scope, false, false);
      if(converted.lvalue && converted.type) {
        converted.operand = emit_load(converted.operand, converted.type);
        converted.lvalue = false;
      }
      Value index = EmitValue(index_node, scope, Fundamental("long int"));
      const string offset = new_temp();
      AddInstruction(offset + " = binary mul i64 " + index.operand + ", " +
        integer_text(static_cast<long long>(type_size(pointer->child))));
      const string address = new_temp();
      AddInstruction(address + " = index i8 " + converted.operand + ", " + offset);
      return address;
    }
    if(!base_type || (base_type->kind != TYPE_ARRAY && base_type->kind != TYPE_POINTER))
      throw logic_error("subscript base is not an array or pointer");
    TypePtr element = base_type->child;
    string base = base_type->kind == TYPE_ARRAY ? EmitArrayDecay(base_node, scope) :
      EmitValue(base_node, scope).operand;
    Value index = EmitValue(index_node, scope, Fundamental("long int"));
    TypePtr element_value = type_value(element);
    if(base_type->kind == TYPE_ARRAY && element_value &&
       (element_value->kind == TYPE_CLASS || element_value->kind == TYPE_ARRAY)) {
      const string offset = new_temp();
      AddInstruction(offset + " = binary mul i64 " + index.operand + ", " +
        integer_text(static_cast<long long>(type_size(element))));
      const string text = new_temp();
      AddInstruction(text + " = index i8 [projection=array_element] " + base + ", " + offset);
      return text;
    } else if(base_type->kind == TYPE_POINTER && element_value &&
              (element_value->kind == TYPE_CLASS || element_value->kind == TYPE_ARRAY)) {
      const string offset = new_temp();
      AddInstruction(offset + " = binary mul i64 " + index.operand + ", " +
        integer_text(static_cast<long long>(type_size(element))));
      const string text = new_temp();
      AddInstruction(text + " = index i8 " + base + ", " + offset);
      return text;
    } else {
      const string text = new_temp();
      AddInstruction(text + " = index " + low_type(element) +
        " [projection=array_element] " + base + ", " + index.operand);
      return text;
    }
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
      Value offset = EmitValue(offset_node, scope, Fundamental("long int"));
      const size_t element_size = type_size(pointer_type->child);
      string scaled;
      if(element_size == 1 && !subtract && offset.known_constant)
        scaled = offset.operand;
      else {
        const string scale = new_temp();
        if(element_size == 1)
          AddInstruction(scale + " = copy i64 " + offset.operand);
        else
          AddInstruction(scale + " = binary mul i64 " + offset.operand + ", " +
            integer_text(static_cast<long long>(element_size)));
        scaled = scale;
      }
      if(subtract) {
        const string neg = new_temp();
        AddInstruction(neg + " = binary sub i64 0, " + scaled);
        scaled = neg;
      }
      const string result = new_temp();
      AddInstruction(result + " = index i8 " + base + ", " + scaled);
      return result;
    }
    const ExprInfo offset_info = pointer_node == node->children[0] ? right_info : left_info;
    const TypePtr offset_value_type = expression_value_type(offset_info);
    const bool class_offset = offset_value_type && type_value(offset_value_type) &&
      type_value(offset_value_type)->kind == TYPE_CLASS;
    Binding* offset_conversion = class_offset ? FindConversionOperator(
      offset_value_type, Fundamental("long int"), false) : 0;
    Value offset;
    string base;
    if(pointer_node == node->children[0]) {
      base = EmitValue(pointer_node, scope).operand;
      offset = offset_conversion ?
        EmitConversionOperator(offset_node, scope, Fundamental("long int"), false) :
        EmitValue(offset_node, scope, Fundamental("long int"));
    } else {
      offset = offset_conversion ?
        EmitConversionOperator(offset_node, scope, Fundamental("long int"), false) :
        EmitValue(offset_node, scope, Fundamental("long int"));
      base = EmitValue(pointer_node, scope).operand;
    }
    if(offset_conversion)
      offset = ConvertValue(offset, Fundamental("long int"), false, true);
    else if(state_ && state_->record && state_->record->explicit_specialization) {
      // An explicit specialization has already resolved the dependent
      // parameter type.  Preserve that typed conversion at the byte-pointer
      // boundary instead of relying on the old same-width copy shortcut.
      const TypePtr offset_value_type = type_value(offset.type);
      const TypePtr index_type = Fundamental("long int");
      if(offset_value_type && is_integral_type(offset_value_type) &&
         !offset.known_constant &&
         !PA12SameType(offset_value_type, index_type, true))
        offset = ConvertValue(offset, index_type, false, true);
    }
    // Pointer arithmetic is performed in signed ptrdiff_t-sized units.  Keep
    // the typed conversion visible when an unsigned size/count expression
    // arrives with the same LowIR width; otherwise the later multiplication
    // consumes the unsigned load directly and loses the conversion boundary.
    const TypePtr offset_type = type_value(offset.type);
    const TypePtr signed_offset_type = Fundamental("long int");
    const long long size = pointer_type && pointer_type->kind == TYPE_POINTER ?
      static_cast<long long>(type_size(pointer_type->child)) : 1;
    // The existing byte-pointer path already materializes its signed index
    // copy while scaling.  For wider element pointers, preserve the explicit
    // unsigned-to-ptrdiff boundary before multiplication.
    if(offset_type && is_integral_type(offset_type) &&
       is_unsigned_type(offset_type) && !offset.known_constant && size > 1 &&
       type_size(offset_type) == type_size(signed_offset_type)) {
      const string converted = new_temp();
      AddInstruction(converted + " = copy i64 " + offset.operand);
      offset.operand = converted;
      offset.type = signed_offset_type;
    }
    string scaled;
    if(size == 1 && !subtract &&
       ((!pointer_node || pointer_node->kind != "binary-expression") ||
        (state_ && state_->record && state_->record->explicit_specialization &&
         low_type(offset.type) == "i64"))) scaled = offset.operand;
    else {
      const string scale = new_temp();
      if(size == 1)
        AddInstruction(scale + " = copy i64 " + offset.operand);
      else
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
string PA14Lowerer::EmitLiteralAddress(const CPPGMAstNodePtr& node)
{
    const string symbol = InternString(node->value);
    const string temp = new_temp();
    AddInstruction(temp + " = addr @" + symbol);
    return temp;
}
string PA14Lowerer::EmitAddress(const CPPGMAstNodePtr& node, Scope* scope)
{
    if(!node) throw logic_error("missing lvalue"); string initializer_list_address;
    if(EmitInitializerListAddress(node, scope, &initializer_list_address)) return initializer_list_address;
    if(node->kind == "parenthesized-expression") return node->children.empty() ? string() : EmitAddress(node->children[0], scope);
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
      const bool decltype_form = node->value.compare(0, 9, "decltype(") == 0;
      Binding* qualified_member = ResolveDecltypeStaticMember(node->value, scope);
      vector<Binding*> candidates = qualified_member ?
        vector<Binding*>(1, qualified_member) : Lookup(node->value, scope);
      if(candidates.empty()) return EmitCapturedAddress(node, scope);
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
      if(!global && binding->is_member && binding->is_static)
        global = EnsureStaticMemberStorage(binding,
          decltype_form);
      if(global) {
        global->referenced = true;
        if(type_is_reference(global->type))
          return emit_load("@" + global->symbol,
            PointerTo(Fundamental("char")));
        const string address = global_address(global);
        return address;
      }
      if(binding->is_member && !binding->is_static) {
        CPPGMAstNodePtr this_node(new CPPGMAstNode("keyword-literal", "KW_THIS:this"));
        CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "OP_ARROW:->"));
        member->children.push_back(this_node);
        member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", binding->name)));
        const string address = EmitMemberAddress(member, scope, true);
		if(type_is_reference(binding->type)) return emit_load(address, PointerTo(Fundamental("char")));
        return address;
      }
      throw logic_error("cannot take address of expression");
    }
    if(node->kind == "member-expression") {
      const string address = EmitMemberAddress(node, scope, true);
      Binding* member = MemberBinding(node, scope);
      if(member && type_is_reference(member->type))
        return emit_load(address, PointerTo(Fundamental("char")));
      return address;
    }
    if(node->kind == "literal" && !node->value.empty() && node->value[0] == '"')
      return EmitLiteralAddress(node);
    if(node->kind == "unary-expression" && PA12Operator(node->value) == "&") {
      vector<CPPGMAstNodePtr> arguments;
      if(!node->children.empty()) arguments.push_back(node->children[0]);
      CallChoice overloaded = ChooseOperatorCall("operator&", arguments, scope);
      if(overloaded.binding) return EmitOperatorCall("operator&", arguments, scope).operand;
      return EmitAddress(node->children.empty() ? CPPGMAstNodePtr() :
        node->children[0], scope);
    }
    if(node->kind == "unary-expression" || node->kind == "postfix-expression" ||
       node->kind == "binary-expression" || node->kind == "assignment-expression") {
      const string operator_address = EmitOperatorAddress(node, scope);
      if(!operator_address.empty()) return operator_address;
    }
    if(node->kind == "unary-expression") {
      const string op = PA12Operator(node->value);
      if(op == "*") return EmitValue(node->children[0], scope).operand;
      if(op == "++" || op == "--") {
        EmitUpdate(node, scope, true);
        return EmitAddress(node->children[0], scope);
      }
    }
    if(node->kind == "subscript-expression") return EmitSubscriptAddress(node, scope); if(node->kind == "conditional-expression") return EmitConditionalAddress(node, scope);
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
      if(PA12Operator(node->value) == "dynamic_cast" && type_is_reference(target)) return EmitDynamicCast(node, scope, target).operand;
      if(type_is_reference(target)) {
        TypePtr target_value = type_value(target);
        ExprInfo source_info = Infer(node->children[1], scope); TypePtr source_value = expression_value_type(source_info);
        if(target_value && target_value->kind == TYPE_CLASS && source_value && source_value->kind == TYPE_CLASS && !PA12SameType(target_value, source_value, true)) {
          if(IsDerivedFrom(source_value, target_value))
            return AdjustBaseAddress(EmitAddress(node->children[1], scope), source_value, target_value);
          if(IsDerivedFrom(target_value, source_value))
            return AdjustDerivedAddress(EmitAddress(node->children[1], scope), target_value,
                                        source_value);
          const string slot = new_special_slot("tmpobj", low_type(target_value));
          const string address = new_temp();
          AddInstruction(address + " = addr $" + slot);
          vector<CPPGMAstNodePtr> arguments(1, node->children[1]);
          if(!EmitConstructorAt(target_value, address, arguments, scope, true))
            throw logic_error("class reference cast has no viable constructor");
          RegisterTemporaryObject(target_value, address);
          return address;
        }
        return EmitAddress(node->children[1], scope);
      }
    }
    if(node->kind == "call-expression") return EmitCallAddress(node, scope);
    throw logic_error("expression is not addressable");
  }
string PA14Lowerer::EmitOperatorAddress(const CPPGMAstNodePtr& node, Scope* scope)
{
    string name;
    vector<CPPGMAstNodePtr> arguments;
    if(node->kind == "unary-expression") {
      name = OperatorFunctionName(PA12Operator(node->value));
      if(!node->children.empty()) arguments.push_back(node->children[0]);
    } else if(node->kind == "postfix-expression") {
      name = OperatorFunctionName(PA12Operator(node->value));
      if(!node->children.empty()) arguments.push_back(node->children[0]);
      arguments.push_back(CPPGMAstNodePtr(new CPPGMAstNode("literal", "0")));
    } else if(node->children.size() >= 2) {
      name = OperatorFunctionName(PA12Operator(node->value));
      arguments.push_back(node->children[0]);
      arguments.push_back(node->children[1]);
    }
    if(name.empty()) return string();
    CallChoice choice = ChooseOperatorCall(name, arguments, scope);
    if(!choice.binding) return string();
    ExprInfo expression_info = Infer(node, scope);
    TypePtr value_type = expression_value_type(expression_info);
    FunctionRecord* function = expression_info.binding ?
      RecordForBinding(expression_info.binding) : 0;
    string address;
    if(value_type && value_type->kind == TYPE_CLASS && expression_info.type &&
       !type_is_reference(expression_info.type) &&
       !(function && function->indirect_result)) {
      const string slot = new_special_slot("tmpobj", low_type(value_type));
      address = new_temp();
      AddInstruction(address + " = addr $" + slot);
    }
    Value result = EmitOperatorCall(name, arguments, scope);
    if(result.lvalue) return result.operand;
    if(value_type && value_type->kind == TYPE_CLASS) {
      if(function && function->indirect_result) {
        RegisterTemporaryObject(value_type, result.operand);
        return result.operand;
      }
      AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(value_type))) +
        "x" + integer_text(static_cast<long long>(type_alignment(value_type))) + " " +
        result.operand + ", " + address);
      RegisterTemporaryObject(value_type, address);
      return address;
    }
    throw logic_error("operator result is not addressable");
}
string PA14Lowerer::EmitCallAddress(const CPPGMAstNodePtr& node, Scope* scope)
{
    TypePtr constructor_type = node->children.empty() ? TypePtr() :
      ConstructorObjectType(node->children[0], scope);
    if(constructor_type)
      return EmitTemporaryObjectAddress(node, scope, "tmpobj",
        HasDestructor(constructor_type));
    ExprInfo info = Infer(node, scope);
    if(type_is_reference(info.type)) return EmitCall(node, scope).operand;
    TypePtr value_type = expression_value_type(info);
    if(value_type && value_type->kind == TYPE_CLASS) {
      FunctionRecord* function = info.binding ? RecordForBinding(info.binding) : 0;
      const string slot = new_special_slot("tmpobj", low_type(value_type));
      const string address = new_temp();
      AddInstruction(address + " = addr $" + slot);
      Value value = EmitCall(node, scope);
      if(function && function->indirect_result) {
        RegisterTemporaryObject(value_type, value.operand);
        return value.operand;
      }
      AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(value_type))) +
        "x" + integer_text(static_cast<long long>(type_alignment(value_type))) + " " +
        value.operand + ", " + address);
      RegisterTemporaryObject(value_type, address);
      return address;
    }
    throw logic_error("expression is not addressable");
  }
string PA14Lowerer::EmitMemberAddress(const CPPGMAstNodePtr& node, Scope* scope,
                                      bool reference_projection)
{
    ExprInfo object_info;
	Binding* member = MemberBinding(node, scope, &object_info);
	if(!member) throw logic_error("unknown member");
    if(member->kind == BIND_FUNCTION) {
      if(member->is_static) {
        FunctionRecord* function = RecordForBinding(member);
        if(function) return function_address(function);
      }
      throw logic_error("member function is not an lvalue");
    }
    if(member->is_static) {
      GlobalRecord* global = EnsureStaticMemberStorage(member);
      if(!global) throw logic_error("static member has no storage");
      return global_address(global);
    }
    if(member->member_index == static_cast<size_t>(-1) || !member->member_owner ||
       member->member_index >= member->member_owner->class_members.size())
      throw logic_error("member has no layout record");
    const ClassMemberInfo& fact = member->member_owner->class_members[member->member_index];
    const string op = PA12Operator(node->value);
    TypePtr field_type = type_value(fact.type);
    const string stable_key = StableMemberAddressKey(node, member, field_type);
    if(!stable_key.empty() && state_) {
      map<string, string>::const_iterator cached =
        state_->stable_member_addresses.find(stable_key);
      if(cached != state_->stable_member_addresses.end()) return cached->second;
    }
    string base;
    if(op == "->") {
      TypePtr object = expression_value_type(object_info);
      if(!object || object->kind != TYPE_POINTER) throw logic_error("arrow requires a pointer to class");
      object = type_value(object->child);
      base = EmitValue(node->children[0], scope).operand;
    } else {
      TypePtr object = expression_value_type(object_info);
      const size_t object_temporary_mark = state_ ?
        state_->temporary_objects.size() : 0;
      base = EmitAddress(node->children[0], scope);
      object = type_value(object);
      if(object && object->kind == TYPE_POINTER) object = type_value(object->child);
      // A class prvalue used as a member object creates a temporary before
      // the member projection itself.  If this projection is an argument to
      // an enclosing call, its cleanup region must already cover the field
      // address/load; leave the typed call context open for EmitChosenCall
      // to close around that enclosing call.
      if(state_ && !state_->constructor_unwind_active &&
         !state_->suppress_constructor_unwind &&
         !state_->defer_temporary_cleanup &&
         state_->temporary_objects.size() > object_temporary_mark &&
         object_info.category == "prvalue") {
        const vector<FunctionState::TemporaryObject> cleanup =
          CaptureLiveCleanupObjects();
        if(!cleanup.empty()) {
          BeginConstructorUnwind(cleanup, true);
          state_->pending_call_argument_context = true;
        }
      }
    }
    TypePtr object = expression_value_type(object_info);
    if(op == "->") object = object && object->kind == TYPE_POINTER ?
      type_value(object->child) : TypePtr();
    else if(object && object->kind == TYPE_POINTER) object = type_value(object->child);
    bool projected_injected_storage = false;
    if(member->injected_member && member->injected_owner &&
       (!object || !PA12SameType(object, member->injected_owner, true))) {
      bool found_injected_storage = false;
      if(object && object->kind == TYPE_CLASS) {
        for(size_t i = 0; i < object->class_members.size(); ++i) {
          const ClassMemberInfo& outer = object->class_members[i];
          if(!outer.name.empty() && outer.type) continue;
          if(outer.type && PA12SameType(type_value(outer.type),
                                        member->injected_owner, true)) {
            found_injected_storage = true;
            if(outer.offset != 0) {
              const string adjusted = new_temp();
              AddInstruction(adjusted + " = index i8 [projection=field] " + base + ", " +
                integer_text(outer.offset));
              base = adjusted;
              projected_injected_storage = true;
            }
            break;
          }
        }
      }
      if(!found_injected_storage)
        throw logic_error("anonymous member has no storage record");
    } else base = AdjustBaseAddress(base, object, member->member_owner);
    ApplyCapturedThisProjection(node, op, &base);
    // An injected member of an anonymous union uses the union storage itself
    // when its layout offset is zero.  The injected binding carries the
    // outer member's offset in the projection above; applying a second
    // zero-offset field projection changes the canonical LowIR shape and,
    // more importantly, obscures that this is the union object address.
    const TypePtr injected_owner = type_value(member->injected_owner);
    if(member->injected_member && injected_owner && injected_owner->kind == TYPE_CLASS &&
       injected_owner->is_union && fact.offset == 0 &&
       projected_injected_storage) {
      if(!stable_key.empty() && state_) state_->stable_member_addresses[stable_key] = base;
      return base;
    }
    const string result = new_temp();
    const bool raw_bit_field = IsBitField(member) && op == ".";
    const bool reference_field = reference_projection && type_is_reference(fact.type);
    AddInstruction(result + " = index i8 " +
      (raw_bit_field ? string() :
       (reference_field ? "[projection=reference_field] " : "[projection=field] ")) +
      base + ", " +
      integer_text(fact.offset));
    if(!stable_key.empty() && state_) state_->stable_member_addresses[stable_key] = result;
    return result;
}
string PA14Lowerer::AdjustBaseAddress(const string& base, const TypePtr& raw_derived,
                                      const TypePtr& target,
                                      bool project_base_path)
{
    TypePtr derived = type_value(raw_derived);
    TypePtr wanted = type_value(target);
    if(!derived || !wanted || PA12SameType(derived, wanted, true)) return base;
    if(derived->kind != TYPE_CLASS || wanted->kind != TYPE_CLASS)
      throw logic_error("member owner is not a base class");
    if(!IsDerivedFrom(derived, wanted))
      throw logic_error("member owner is not a base class");
    vector<size_t> path;
    set<const Type*> visited;
    function<bool(const TypePtr&)> find_base =
      [&](const TypePtr& current) {
        if(!current || !visited.insert(current.get()).second) return false;
        if(PA12SameType(current, wanted, true)) return true;
        if(!current->direct_bases.empty()) {
          for(size_t i = 0; i < current->direct_bases.size(); ++i) {
            const size_t base_offset = i < current->direct_base_offsets.size() ?
              current->direct_base_offsets[i] : (i == 0 ? current->direct_base_offset : 0);
            path.push_back(base_offset);
            if(find_base(type_value(current->direct_bases[i]))) return true;
            path.pop_back();
          }
        } else if(current->direct_base) {
          path.push_back(current->direct_base_offset);
          if(find_base(type_value(current->direct_base))) return true;
          path.pop_back();
        }
        return false;
      };
    if(!find_base(derived)) throw logic_error("member owner is not a base class");
    if(!project_base_path) {
      size_t offset = 0;
      for(size_t i = 0; i < path.size(); ++i) offset += path[i];
      const string adjusted = new_temp();
      AddInstruction(adjusted + " = index i8 [projection=base_subobject] " + base + ", " +
        integer_text(static_cast<long long>(offset)));
      return adjusted;
    }
    string adjusted = base;
    for(size_t i = 0; i < path.size(); ++i) {
      const string projected = new_temp();
      AddInstruction(projected + " = index i8 [projection=base_subobject] " + adjusted + ", " +
        integer_text(static_cast<long long>(path[i])));
      adjusted = projected;
    }
    return adjusted;
  }
} // namespace cppgm_pa14_lowering
