#include "pa14_lowering.h"

#include <algorithm>
#include <cctype>
#include <sstream>

using namespace std;

namespace cppgm_pa14_lowering {

namespace {

string trim_initializer_text(string text)
{
  while(!text.empty() && isspace(static_cast<unsigned char>(text[0]))) text.erase(0, 1);
  while(!text.empty() && isspace(static_cast<unsigned char>(text[text.size() - 1])))
    text.erase(text.size() - 1);
  return text;
}

string initializer_type_spelling(const TypePtr& raw)
{
  if(!raw) return string();
  TypePtr type = raw;
  string cv;
  if(type->is_const) cv += "const ";
  if(type->is_volatile) cv += "volatile ";
  if(type->kind == TYPE_POINTER) return cv + initializer_type_spelling(type->child) + "*";
  if(type->kind == TYPE_LVALUE_REFERENCE)
    return cv + initializer_type_spelling(type->child) + "&";
  if(type->kind == TYPE_RVALUE_REFERENCE)
    return cv + initializer_type_spelling(type->child) + "&&";
  if(type->kind == TYPE_ARRAY) {
    ostringstream result;
    result << cv << initializer_type_spelling(type->child) << "[" << type->bound << "]";
    return result.str();
  }
  if(type->kind == TYPE_CLASS || type->kind == TYPE_ENUM)
    return cv + type->name;
  return cv + type->name;
}

TypePtr strip_initializer_cv(const TypePtr& raw)
{
  TypePtr type = raw;
  while(type && type_is_reference(type)) type = type->child;
  return type;
}

CPPGMAstNodePtr initializer_list_child(const CPPGMAstNodePtr& node, size_t index)
{
  return node && index < node->children.size() ? node->children[index] : CPPGMAstNodePtr();
}

bool initializer_list_node_kind(const CPPGMAstNodePtr& node, const string& kind)
{
  return node && node->kind == kind && !node->children.empty();
}

} // namespace

bool PA14Lowerer::IsInitializerListType(const TypePtr& raw) const
{
  TypePtr type = strip_initializer_cv(raw);
  if(!type || type->kind != TYPE_CLASS) return false;
  const string primary = type->template_primary.empty() ? type->name : type->template_primary;
  return last_component(primary) == "initializer_list";
}

void PA14Lowerer::EnsureInitializerListType(const TypePtr& raw) const
{
  TypePtr type = strip_initializer_cv(raw);
  if(!IsInitializerListType(type)) return;
  if(type->complete && type->layout_complete && type->object_size == 16 &&
     type->object_alignment == 8) return;
  type->complete = true;
  type->layout_complete = true;
  type->object_size = 16;
  type->object_alignment = 8;
}

TypePtr PA14Lowerer::InitializerListElementType(const TypePtr& raw, Scope* scope) const
{
  TypePtr type = strip_initializer_cv(raw);
  if(!IsInitializerListType(type)) return TypePtr();
  string spelling;
  if(!type->template_arguments.empty()) spelling = type->template_arguments[0];
  else {
    const size_t open = type->name.find('<');
    const size_t close = type->name.rfind('>');
    if(open != string::npos && close > open) spelling = type->name.substr(open + 1, close - open - 1);
  }
  spelling = trim_initializer_text(spelling);
  if(spelling.empty()) return TypePtr();
  while(spelling.size() && spelling[spelling.size() - 1] == '*') {
    spelling = trim_initializer_text(spelling.substr(0, spelling.size() - 1));
    TypePtr pointee = InitializerListElementType(
      TypePtr(new Type(TYPE_CLASS, spelling)), scope);
    if(!pointee) {
      try { pointee = analyzer_.ResolveType(scope, spelling); }
      catch(const logic_error&) { return TypePtr(); }
    }
    return PointerTo(pointee);
  }
  try { return analyzer_.ResolveType(scope, spelling); }
  catch(const logic_error&) {
    const string const_prefix = "const ";
    const string volatile_prefix = "volatile ";
    bool add_const = false, add_volatile = false;
    if(spelling.compare(0, const_prefix.size(), const_prefix) == 0) {
      add_const = true; spelling = trim_initializer_text(spelling.substr(const_prefix.size()));
    }
    if(spelling.compare(0, volatile_prefix.size(), volatile_prefix) == 0) {
      add_volatile = true; spelling = trim_initializer_text(spelling.substr(volatile_prefix.size()));
    }
    try { return CloneWithCv(analyzer_.ResolveType(scope, spelling), add_const, add_volatile); }
    catch(const logic_error&) {
      if(spelling == "bool" || spelling == "char" || spelling == "char16_t" ||
         spelling == "char32_t" || spelling == "double" || spelling == "float" ||
         spelling == "int" || spelling == "long" || spelling == "long int" ||
         spelling == "long long" || spelling == "long long int" ||
         spelling == "short" || spelling == "short int" || spelling == "unsigned" ||
         spelling == "unsigned int" || spelling == "unsigned long" ||
         spelling == "unsigned long int" || spelling == "unsigned long long" ||
         spelling == "unsigned long long int" || spelling == "unsigned short" ||
         spelling == "unsigned short int" || spelling == "void" ||
         spelling == "wchar_t" || spelling == "nullptr_t")
        return CloneWithCv(Fundamental(spelling), add_const, add_volatile);
      return TypePtr();
    }
  }
}

TypePtr PA14Lowerer::MakeInitializerListType(const TypePtr& element, Scope* scope) const
{
  if(!element) return TypePtr();
  TypePtr primary;
  try { primary = analyzer_.ResolveType(scope, "std::initializer_list"); }
  catch(const logic_error&) {
    try { primary = analyzer_.ResolveType(scope, "initializer_list"); }
    catch(const logic_error&) { return TypePtr(); }
  }
  if(!primary || primary->kind != TYPE_CLASS) return TypePtr();
  const string spelling = initializer_type_spelling(element);
  TypePtr result(new Type(*primary));
  result->name = primary->name + "<" + spelling + ">";
  result->template_specialization = true;
  result->template_primary = primary->name;
  result->template_arguments.clear();
  result->template_arguments.push_back(spelling);
  EnsureInitializerListType(result);
  return result;
}

bool PA14Lowerer::HasInitializerListConstructor(const TypePtr& raw) const
{
  TypePtr type = type_value(raw);
  if(!type || type->kind != TYPE_CLASS || IsInitializerListType(type)) return false;
  vector<Binding*> candidates = MemberBindings(type, LastComponent(type->name));
  if(candidates.empty() && !type->template_primary.empty())
    candidates = MemberBindings(type, LastComponent(type->template_primary));
  for(size_t i = 0; i < candidates.size(); ++i) {
    FunctionRecord* record = RecordForBinding(candidates[i]);
    TypePtr function = function_target_type(candidates[i]->type);
    if(!record || !record->constructor || record->deleted || !function ||
       function->parameters.size() != 1) continue;
    if(IsInitializerListType(function->parameters[0])) return true;
  }
  return false;
}

bool PA14Lowerer::InitializerListArgumentViable(const CPPGMAstNodePtr& node,
                                                const TypePtr& raw_parameter,
                                                Scope* scope)
{
  if(!node || node->kind != "braced-init-list") return false;
  TypePtr element = InitializerListElementType(raw_parameter, scope);
  if(!element) return false;
  for(size_t i = 0; i < node->children.size(); ++i) {
    const CPPGMAstNodePtr child = node->children[i];
    TypePtr element_value = type_value(element);
    if(element_value && element_value->kind == TYPE_CLASS && child &&
       child->kind == "braced-init-list") {
      if(!HasConstructor(element_value) && element_value->class_members.empty()) return false;
      continue;
    }
    ExprInfo info;
    try { info = Infer(child, scope); }
    catch(const logic_error&) { return false; }
    if(ConversionRank(info, element) < 0) {
      TypePtr constructed = child && child->kind == "call-expression" &&
        !child->children.empty() ? ConstructorObjectType(child->children[0], scope) : TypePtr();
      if(!constructed || !PA12SameType(constructed, element_value, true)) return false;
    }
  }
  return true;
}

PA14Lowerer::ExprInfo PA14Lowerer::InferInitializerListAuto(
  const CPPGMAstNodePtr& expression, Scope* scope)
{
  if(!expression || expression->kind != "braced-init-list" || expression->children.empty())
    throw logic_error("cannot deduce auto from an empty initializer");
  TypePtr common;
  for(size_t i = 0; i < expression->children.size(); ++i) {
    ExprInfo info = Infer(expression->children[i], scope);
    TypePtr current = expression_value_type(info);
    if(!current) throw logic_error("initializer-list element has no type");
    if(!common) common = current;
    else if(PA12SameType(common, current, true)) {}
    else common = CommonType(common, current);
    if(!common || ConversionRank(info, common) < 0)
      throw logic_error("initializer-list elements have incompatible types");
  }
  TypePtr list = MakeInitializerListType(common, scope);
  if(!list) throw logic_error("initializer_list type is unavailable");
  ExprInfo result;
  result.type = list;
  result.category = "prvalue";
  return result;
}

bool PA14Lowerer::EmitInitializerListAt(const string& destination,
                                        const CPPGMAstNodePtr& expression,
                                        const TypePtr& raw_list_type, Scope* scope)
{
  if(!destination.empty() && expression && expression->kind == "braced-init-list") {
    TypePtr list_type = type_value(raw_list_type);
    TypePtr element_type = InitializerListElementType(list_type, scope);
    if(!IsInitializerListType(list_type) || !element_type) return false;
    EnsureInitializerListType(list_type);
    const size_t count = expression->children.size();
    const string data_slot = new_special_slot("initlist",
      low_type(ArrayOf(static_cast<long long>(count), element_type)));
    const string data_address = new_temp();
    AddInstruction(data_address + " = addr $" + data_slot);
    for(size_t i = 0; i < count; ++i) {
      const CPPGMAstNodePtr child = expression->children[i];
      TypePtr value_type = type_value(element_type);
      Value scalar_value;
      const bool class_element = value_type && value_type->kind == TYPE_CLASS;
      if(!class_element) scalar_value = EmitValue(child, scope, element_type);
      string element_address = data_address;
      if(i != 0) {
        const string offset = integer_text(static_cast<long long>(i * type_size(element_type)));
        element_address = new_temp();
        AddInstruction(element_address + " = index i8 " +
          data_address + ", " + offset);
      }
      if(class_element) {
        if(IsEmptyBaseStorage(value_type) && IsTrivialValueStorage(value_type) && child &&
           (child->kind == "id-expression" || child->kind == "member-expression" ||
            child->kind == "subscript-expression")) {
          (void)EmitAddress(child, scope);
          continue;
        }
        vector<CPPGMAstNodePtr> arguments;
        if(child && child->kind == "braced-init-list") arguments = child->children;
        else if(child && child->kind == "paren-initializer") arguments = child->children;
        else if(child && child->kind == "call-expression" && child->children.size() > 1 &&
                child->children[0] && child->children[0]->kind == "id-expression")
          arguments = child->children[1] ? child->children[1]->children : vector<CPPGMAstNodePtr>();
        else if(child) arguments.push_back(child);
        if(arguments.size() == 1 && arguments[0] &&
           arguments[0]->kind == "braced-init-list")
          arguments = arguments[0]->children;
        if(EmitConstructorAt(value_type, element_address, arguments, scope, true, false, true))
          continue;
        if(child && child->kind == "braced-init-list") {
          EmitAggregateAt(element_address, value_type, child, scope);
          continue;
        }
        throw logic_error("no viable initializer-list element constructor");
      }
      Value value = scalar_value;
      if(value.lvalue && value.type) {
        value.operand = emit_load(value.operand, value.type);
        value.lvalue = false;
      }
      if(value.known_constant && is_integral_type(value.type) && is_integral_type(element_type)) {
        value.type = element_type;
        value.operand = integer_text(value.constant);
      } else value = ConvertValue(value, element_type, false, true);
      emit_store(element_type, value.operand, element_address);
    }
    emit_store(PointerTo(Fundamental("char")), data_address, destination);
    const string size_field = new_temp();
    AddInstruction(size_field + " = index i8 " + destination + ", 8");
    emit_store(Fundamental("unsigned long int"), integer_text(static_cast<long long>(count)), size_field);
    return true;
  }
  return false;
}

string PA14Lowerer::EmitInitializerListArgument(const CPPGMAstNodePtr& expression,
                                                const TypePtr& target, Scope* scope,
                                                const string& prefix)
{
  TypePtr list_type = type_value(target);
  if(!IsInitializerListType(list_type)) throw logic_error("initializer-list argument has wrong target");
  const string slot = new_special_slot(prefix, low_type(list_type));
  const string address = new_temp();
  AddInstruction(address + " = addr $" + slot);
  if(!EmitInitializerListAt(address, expression, list_type, scope))
    throw logic_error("invalid initializer-list argument");
  return type_is_reference(target) ? address : "$" + slot;
}

bool PA14Lowerer::EmitInitializerListAssignment(
  const vector<CPPGMAstNodePtr>& arguments, Scope* scope, Value* result)
{
  CallChoice choice = ChooseOperatorCall("operator=", arguments, scope);
  if(!choice.binding || !choice.function || choice.function->parameters.size() != 1 ||
     !IsInitializerListType(choice.function->parameters[0])) return false;
  *result = EmitChosenCall(choice, CPPGMAstNodePtr(), arguments, scope);
  return true;
}

bool PA14Lowerer::InferInitializerListNode(const CPPGMAstNodePtr& node, Scope* scope,
                                           const TypePtr&, ExprInfo* result)
{
  if(!result || (!initializer_list_node_kind(node, "initializer-list-size") &&
                 !initializer_list_node_kind(node, "initializer-list-element"))) return false;
  ExprInfo source = Infer(initializer_list_child(node, 0), scope);
  TypePtr list = expression_value_type(source);
  if(!IsInitializerListType(list)) throw logic_error("range source is not an initializer_list");
  if(node->kind == "initializer-list-size") {
    result->type = Fundamental("long int");
    result->category = "prvalue";
  } else {
    result->type = InitializerListElementType(list, scope);
    if(!result->type) throw logic_error("initializer-list element has no type");
    result->category = "lvalue";
  }
  return true;
}

bool PA14Lowerer::EmitInitializerListAddress(const CPPGMAstNodePtr& node, Scope* scope,
                                             string* address)
{
  if(!initializer_list_node_kind(node, "initializer-list-element")) return false;
  TypePtr element = InitializerListElementType(Infer(initializer_list_child(node, 0), scope).type, scope);
  if(!element) throw logic_error("initializer-list element has no type");
  const string base = EmitAddress(initializer_list_child(node, 0), scope);
  const string field = new_temp();
  AddInstruction(field + " = index i8 [projection=field] " + base + ", 0");
  const string data = emit_load(field, PointerTo(Fundamental("char")));
  Value index = EmitValue(initializer_list_child(node, 1), scope, Fundamental("long int"));
  TypePtr value_type = type_value(element);
  if(value_type && (value_type->kind == TYPE_CLASS || value_type->kind == TYPE_ARRAY)) {
    const string offset = new_temp();
    AddInstruction(offset + " = binary mul i64 " + index.operand + ", " +
      integer_text(static_cast<long long>(type_size(element))));
    *address = new_temp();
    AddInstruction(*address + " = index i8 [projection=array_element] " + data + ", " + offset);
  } else {
    *address = new_temp();
    AddInstruction(*address + " = index " + low_type(element) +
      " [projection=array_element] " + data + ", " + index.operand);
  }
  return true;
}

bool PA14Lowerer::EmitInitializerListValue(const CPPGMAstNodePtr& node, Scope* scope,
                                           const TypePtr&, Value* result)
{
  if(initializer_list_node_kind(node, "initializer-list-size")) {
    TypePtr list = Infer(initializer_list_child(node, 0), scope).type;
    const string base = EmitAddress(initializer_list_child(node, 0), scope);
    const string field = new_temp();
    AddInstruction(field + " = index i8 [projection=field] " + base + ", 8");
    result->type = Fundamental("long int");
    result->operand = emit_load(field, result->type);
    return true;
  }
  if(initializer_list_node_kind(node, "initializer-list-element")) {
    TypePtr list = Infer(initializer_list_child(node, 0), scope).type;
    TypePtr element = InitializerListElementType(list, scope);
    string address;
    EmitInitializerListAddress(node, scope, &address);
    result->type = element;
    result->operand = emit_load(address, element);
    return true;
  }
  return false;
}

} // namespace cppgm_pa14_lowering
