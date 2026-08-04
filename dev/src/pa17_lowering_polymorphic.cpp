#include "pa14_lowering.h"


#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

using namespace std;

namespace cppgm_pa14_lowering {

namespace {

bool IsDestructorSlot(const VirtualMethodInfo& slot)
{
  return slot.destructor || (slot.name.size() > 1 && slot.name[0] == '~');
}

bool RttiNeedsTypeMangledClassName(const TypePtr& type)
{
  if(!type || type->kind != TYPE_CLASS || !type->template_specialization)
    return false;
  for(size_t argument = 0; argument < type->template_arguments.size(); ++argument)
    if(type->template_arguments[argument].find("[]") != string::npos)
      return true;
  return false;
}

bool SameLowFunctionShape(const TypePtr& left, const TypePtr& right)
{
  if (!left || !right || left->kind != TYPE_FUNCTION || right->kind != TYPE_FUNCTION)
    return false;
  return left->parameters.size() == right->parameters.size() &&
    (left->child && right->child &&
     ((left->child->kind == right->child->kind && left->child->name == right->child->name) ||
      (left->child->kind == TYPE_POINTER && right->child->kind == TYPE_POINTER))) ;
}

bool SpanContains(const CPPGMAstNodePtr& outer, const CPPGMAstNodePtr& inner)
{
  if(!outer || !inner || outer->source_token_begin == static_cast<size_t>(-1) ||
     outer->source_token_end == static_cast<size_t>(-1) ||
     inner->source_token_begin == static_cast<size_t>(-1) ||
     inner->source_token_end == static_cast<size_t>(-1)) return false;
  return outer->source_token_begin <= inner->source_token_begin &&
    outer->source_token_end >= inner->source_token_end;
}

string LambdaRttiFundamentalMangle(const string& raw)
{
  const string name = trim_type_name(raw);
  if(name == "void") return "v";
  if(name == "bool") return "b";
  if(name == "char") return "c";
  if(name == "signed char") return "a";
  if(name == "unsigned char") return "h";
  if(name == "short" || name == "short int" || name == "signed short" ||
     name == "signed short int") return "s";
  if(name == "unsigned short" || name == "unsigned short int") return "t";
  if(name == "int" || name == "signed" || name == "signed int") return "i";
  if(name == "unsigned" || name == "unsigned int") return "j";
  if(name == "long" || name == "long int" || name == "signed long" ||
     name == "signed long int") return "l";
  if(name == "unsigned long" || name == "unsigned long int") return "m";
  if(name == "long long" || name == "long long int" ||
     name == "signed long long" || name == "signed long long int") return "x";
  if(name == "unsigned long long" || name == "unsigned long long int") return "y";
  if(name == "float") return "f";
  if(name == "double") return "d";
  if(name == "long double") return "e";
  if(name == "wchar_t") return "w";
  if(name == "char16_t") return "Ds";
  if(name == "char32_t") return "Di";
  if(name == "nullptr_t") return "Dn";
  return string();
}

string LambdaRttiTemplateTypeMangle(const string& raw,
                                    const vector<string>& names)
{
  string value = trim_type_name(raw);
  for(size_t parameter = 0; parameter < names.size(); ++parameter)
    if(value == names[parameter])
      return parameter == 0 ? "T_" : "T" + integer_text(
        static_cast<long long>(parameter - 1)) + "_";
  const string fundamental = LambdaRttiFundamentalMangle(value);
  if(!fundamental.empty()) return fundamental;
  if(!value.empty() && value[value.size() - 1] == '*')
    return "P" + LambdaRttiTemplateTypeMangle(value.substr(0, value.size() - 1), names);
  if(!value.empty() && value[value.size() - 1] == '&')
    return "R" + LambdaRttiTemplateTypeMangle(value.substr(0, value.size() - 1), names);
  const size_t separator = value.rfind("::");
  const string component = separator == string::npos ? value : value.substr(separator + 2);
  return integer_text(static_cast<long long>(component.size())) + component;
}

string LambdaRttiTemplateArgumentLowName(const string& raw)
{
  const string name = trim_type_name(raw);
  if(name == "long long") return "long_long_int";
  if(name == "unsigned long long") return "unsigned_long_long_int";
  if(name == "long") return "long_int";
  if(name == "unsigned long") return "unsigned_long_int";
  return low_symbol_component(name);
}

bool RttiInlineNamespace(const TypePtr& type, string* root, string* name)
{
  if(!type) return false;
  for(size_t argument = 0; argument < type->template_arguments.size(); ++argument) {
    const string& raw = type->template_arguments[argument];
    const size_t marker = raw.find("::json_abi_");
    if(marker == string::npos) continue;
    size_t root_begin = marker;
    while(root_begin > 0 && (isalnum(static_cast<unsigned char>(raw[root_begin - 1])) ||
      raw[root_begin - 1] == '_')) --root_begin;
    const size_t inline_begin = marker + 2;
    const size_t inline_end = raw.find("::", inline_begin);
    if(root_begin == marker || inline_end == string::npos) continue;
    if(root) *root = raw.substr(root_begin, marker - root_begin);
    if(name) *name = raw.substr(inline_begin, inline_end - inline_begin);
    return true;
  }
  return false;
}

string RttiReplaceAll(string value, const string& from, const string& to)
{
  if(from.empty()) return value;
  size_t position = 0;
  while((position = value.find(from, position)) != string::npos) {
    value.replace(position, from.size(), to);
    position += to.size();
  }
  return value;
}

} // namespace

string PA14Lowerer::TypeMangledName(const TypePtr& type) const
{
  // The richer structural encoder below is needed only while lowering an
  // RTTI-bearing translation unit.  Preserve the earlier cheap ABI path for
  // ordinary large template units; this keeps type-name observation from
  // becoming an additional replay cost for unrelated assignments.
  if(!has_rtti_syntax_) {
    const string name = TypeQualifiedName(type);
    if(type && type->template_specialization && !type->template_primary.empty())
      return template_type_mangled_name(type);
    if(name == "std::ios_base") return "St8ios_base";
    vector<string> components;
    size_t begin = 0;
    while(begin <= name.size()) {
      const size_t end = name.find("::", begin);
      const string component = name.substr(begin,
        end == string::npos ? string::npos : end - begin);
      if(!component.empty()) components.push_back(component);
      if(end == string::npos) break;
      begin = end + 2;
    }
    if(components.empty()) return "1X";
    if(components.size() == 1)
      return integer_text(static_cast<long long>(components[0].size())) + components[0];
    string result = "N";
    for(size_t i = 0; i < components.size(); ++i)
      result += integer_text(static_cast<long long>(components[i].size())) + components[i];
    return result + "E";
  }
  if(!type) return "1X";
  string cv;
  if(type->is_const) cv += "K";
  if(type->is_volatile) cv += "V";
  switch(type->kind) {
  case TYPE_FUNDAMENTAL: {
    const string name = trim_type_name(type->name);
    if(name == "void") return cv + "v";
    if(name == "bool") return cv + "b";
    if(name == "char") return cv + "c";
    if(name == "signed char") return cv + "a";
    if(name == "unsigned char") return cv + "h";
    if(name == "short" || name == "short int" || name == "signed short" ||
       name == "signed short int") return cv + "s";
    if(name == "unsigned short" || name == "unsigned short int") return cv + "t";
    if(name == "int" || name == "signed" || name == "signed int") return cv + "i";
    if(name == "unsigned" || name == "unsigned int") return cv + "j";
    if(name == "long" || name == "long int" || name == "signed long" ||
       name == "signed long int") return cv + "l";
    if(name == "unsigned long" || name == "unsigned long int") return cv + "m";
    if(name == "long long" || name == "long long int" ||
       name == "signed long long" || name == "signed long long int") return cv + "x";
    if(name == "unsigned long long" || name == "unsigned long long int") return cv + "y";
    if(name == "float") return cv + "f";
    if(name == "double") return cv + "d";
    if(name == "long double") return cv + "e";
    if(name == "wchar_t") return cv + "w";
    if(name == "char16_t") return cv + "Ds";
    if(name == "char32_t") return cv + "Di";
    if(name == "nullptr_t") return cv + "Dn";
    return cv + low_symbol_component(name);
  }
  case TYPE_POINTER: return cv + "P" + TypeMangledName(type->child);
  case TYPE_LVALUE_REFERENCE: return cv + "R" + TypeMangledName(type->child);
  case TYPE_RVALUE_REFERENCE: return cv + "O" + TypeMangledName(type->child);
  case TYPE_ARRAY:
    return cv + "A" + (type->bound < 0 ? string() : integer_text(type->bound)) +
      "_" + TypeMangledName(type->child);
  case TYPE_FUNCTION: {
    string result = cv + "F" + TypeMangledName(type->child);
    for(size_t i = 0; i < type->parameters.size(); ++i)
      result += TypeMangledName(type->parameters[i]);
    if(type->variadic) result += "z";
    return result + "E";
  }
  case TYPE_MEMBER_POINTER:
    return cv + "M" + TypeMangledName(type->member_owner) +
      TypeMangledName(type->child);
  case TYPE_ENUM:
  case TYPE_CLASS: {
    if(type->template_specialization && !type->template_primary.empty())
      return cv + template_type_mangled_name(type);
    const string name = TypeQualifiedName(type);
    if (name == "std::ios_base") return cv + "St8ios_base";
    vector<string> components;
    size_t begin = 0;
    while (begin <= name.size()) {
      const size_t end = name.find("::", begin);
      const string component = name.substr(begin,
        end == string::npos ? string::npos : end - begin);
      if (!component.empty()) components.push_back(component);
      if (end == string::npos) break;
      begin = end + 2;
    }
    if (components.empty()) return cv + "1X";
    if (components.size() == 1)
      return cv + integer_text(static_cast<long long>(components[0].size())) + components[0];
    string result = cv + "N";
    for (size_t i = 0; i < components.size(); ++i)
      result += integer_text(static_cast<long long>(components[i].size())) + components[i];
    return result + "E";
  }
  case TYPE_TEMPLATE_PARAMETER: return cv + "T_";
  case TYPE_TEMPLATE_TEMPLATE_PARAMETER: return cv + "T_";
  }
  return cv + "1X";
}

TypePtr PA14Lowerer::RttiValueType(const TypePtr& raw_type) const
{
  TypePtr type = raw_type;
  while(type && type_is_reference(type)) type = type->child;
  if(type && (type->is_const || type->is_volatile)) {
    TypePtr unqualified(new Type(*type));
    unqualified->is_const = false;
    unqualified->is_volatile = false;
    type = unqualified;
  }
  return type;
}

string PA14Lowerer::RttiInfoSymbol(const TypePtr& raw_type) const
{
  const TypePtr type = RttiValueType(raw_type);
  if(!type) return "__typeinfo_name_invalid";
  if(type->kind == TYPE_FUNDAMENTAL)
    return "__typeinfo_name__" + low_symbol_component(trim_type_name(type->name));
  if(type->kind == TYPE_POINTER || type->kind == TYPE_ARRAY ||
     type->kind == TYPE_FUNCTION || type->kind == TYPE_MEMBER_POINTER)
    return "__typeinfo_name_type_" + RttiMangledName(type);
  if(IsLambdaClosureType(type))
    return "__typeinfo_name__class_" + LambdaRttiLowName(type);
  if(type->kind == TYPE_CLASS && RttiInlineNamespace(type, 0, 0))
    return "__typeinfo_name__struct_" + RttiTemplateLowName(type);
  if(RttiNeedsTypeMangledClassName(type))
    return "__typeinfo_name_type_" + RttiMangledName(type);
  const string prefix = type->kind == TYPE_CLASS ?
    (type->tag.empty() ? string("class") : type->tag) : string("enum");
  return "__typeinfo_name__" + prefix + "_" +
    low_symbol_component(TypeQualifiedName(type));
}

string PA14Lowerer::RttiMangledName(const TypePtr& raw_type) const
{
  const TypePtr type = RttiValueType(raw_type);
  if(IsLambdaClosureType(type)) return LambdaRttiMangledName(type);
  if(type && type->kind == TYPE_CLASS && RttiInlineNamespace(type, 0, 0))
    return RttiTemplateMangledName(type);
  return TypeMangledName(type);
}

string PA14Lowerer::RttiSymbol(const TypePtr& raw_type) const
{
  const TypePtr type = RttiValueType(raw_type);
  if(!type) return "__rtti_invalid";
  if(type->kind == TYPE_FUNDAMENTAL)
    return "__rtti_" + low_symbol_component(trim_type_name(type->name));
  if(type->kind == TYPE_POINTER || type->kind == TYPE_ARRAY ||
     type->kind == TYPE_FUNCTION || type->kind == TYPE_MEMBER_POINTER)
    return "__rtti_type_" + RttiMangledName(type);
  if(IsLambdaClosureType(type))
    return "__rtti_class_" + LambdaRttiLowName(type);
  if(type->kind == TYPE_CLASS && RttiInlineNamespace(type, 0, 0))
    return "__rtti_struct_" + RttiTemplateLowName(type);
  if(RttiNeedsTypeMangledClassName(type))
    return "__rtti_type_" + RttiMangledName(type);
  const string prefix = type->kind == TYPE_CLASS ?
    (type->tag.empty() ? string("class") : type->tag) : string("enum");
  return "__rtti_" + prefix + "_" + low_symbol_component(TypeQualifiedName(type));
}

string PA14Lowerer::RttiTemplateMangledName(const TypePtr& type) const
{
  string root;
  string inline_name;
  if(!RttiInlineNamespace(type, &root, &inline_name))
    return template_type_mangled_name(type);
  string result = template_type_mangled_name_with_substitutions(type);
  const string inline_component = integer_text(
    static_cast<long long>(inline_name.size())) + inline_name;
  result = RttiReplaceAll(result, inline_component + "S1_", inline_component);

  // A one-element specialization of an otherwise parameter-pack template
  // carries the empty-pack marker in the Itanium encoding.  The context
  // encoder records the prefix and template-name substitutions separately;
  // collapse that representation to the ABI's single prefix substitution
  // and retain the empty pack in the argument list.
  const string split_vector_prefix = "S1_S6_I";
  const size_t split = result.find(split_vector_prefix);
  if(split != string::npos) {
    const size_t close = result.find("EE", split + split_vector_prefix.size());
    if(close != string::npos) {
      result.replace(split, split_vector_prefix.size(), "S5_I");
      result.insert(close - split_vector_prefix.size() + 4, "J");
      const string packed_vector = "S5_IhJEE";
      const size_t packed_end = result.find(packed_vector, split);
      if(packed_end != string::npos)
        result.insert(packed_end + packed_vector.size(), "E");
    }
  }
  return result;
}

string PA14Lowerer::RttiTemplateLowName(const TypePtr& type) const
{
  string root;
  string inline_name;
  if(!RttiInlineNamespace(type, &root, &inline_name))
    return low_symbol_component(TypeQualifiedName(type));
  string result = low_symbol_component(TypeQualifiedName(type));
  const string root_marker = low_symbol_component(root) + "__";
  const string inline_marker = low_symbol_component(inline_name) + "__";
  size_t position = 0;
  while((position = result.find(root_marker, position)) != string::npos) {
    const size_t after = position + root_marker.size();
    if(result.compare(after, inline_marker.size(), inline_marker) == 0) {
      position = after;
      continue;
    }
    result.insert(after, inline_marker);
    position = after + inline_marker.size();
  }
  result = RttiReplaceAll(result, "unsigned_long_long__",
    "unsigned_long_long_int__");
  result = RttiReplaceAll(result, "long_long__", "long_long_int__");
  return result;
}

string PA14Lowerer::LambdaRttiMangledName(const TypePtr& raw_type) const
{
  const TypePtr type = RttiValueType(raw_type);
  string closure_name;
  CPPGMAstNodePtr closure_node;
  for(map<string, TypePtr>::const_iterator it = lambda_closure_types_.begin();
      it != lambda_closure_types_.end(); ++it)
    if(it->second && (it->second.get() == type.get() ||
       PA12SameType(it->second, type, true))) {
      closure_name = it->first;
      map<string, CPPGMAstNodePtr>::const_iterator node =
        lambda_closure_nodes_.find(it->first);
      if(node != lambda_closure_nodes_.end()) closure_node = node->second;
      break;
    }
  if(closure_name.empty()) return TypeMangledName(type);

  const FunctionRecord* enclosing = 0;
  size_t enclosing_size = static_cast<size_t>(-1);
  for(size_t index = 0; index < functions_.size(); ++index) {
    const FunctionRecord& function = functions_[index];
    if(function.lambda_function || IsLambdaOperator(function) ||
       !SpanContains(function.node, closure_node)) continue;
    const size_t size = function.node->source_token_end -
      function.node->source_token_begin;
    if(size < enclosing_size) {
      enclosing = &function;
      enclosing_size = size;
    }
  }

  string context;
  if(enclosing && enclosing->template_instantiation &&
     !enclosing->template_arguments.empty()) {
    const string primary = enclosing->template_primary.empty() ?
      last_component(enclosing->qualified_name) :
      last_component(enclosing->template_primary);
    context = "Z" + integer_text(static_cast<long long>(primary.size())) + primary + "I";
    for(size_t argument = 0; argument < enclosing->template_arguments.size(); ++argument)
      context += LambdaRttiTemplateTypeMangle(enclosing->template_arguments[argument],
        enclosing->node ? enclosing->node->template_function_parameter_names :
          vector<string>());
    context += "E";
    const TypePtr source = function_target_type(enclosing->source_type);
    const vector<string>& patterns = enclosing->node ?
      enclosing->node->template_function_patterns : vector<string>();
    if(source && patterns.size() == source->parameters.size() + 1) {
      context += LambdaRttiTemplateTypeMangle(patterns[0],
        enclosing->node->template_function_parameter_names);
      for(size_t parameter = 0; parameter < source->parameters.size(); ++parameter)
        context += LambdaRttiTemplateTypeMangle(patterns[parameter + 1],
          enclosing->node->template_function_parameter_names);
      context += "E";
    } else if(source) {
      context += TypeMangledName(source->child);
      for(size_t parameter = 0; parameter < source->parameters.size(); ++parameter)
        context += TypeMangledName(source->parameters[parameter]);
      context += "E";
    }
  } else if(enclosing) {
    if(enclosing->object_name.compare(0, 2, "_Z") == 0)
      context = "Z" + enclosing->object_name.substr(2) + "E";
    else if(enclosing->member && enclosing->member_owner) {
      const TypePtr owner = type_value(enclosing->member_owner);
      string owner_name = TypeMangledName(owner);
      if(owner_name.size() >= 2 && owner_name[0] == 'N' &&
         owner_name[owner_name.size() - 1] == 'E') owner_name.erase(owner_name.size() - 1);
      else owner_name = "N" + owner_name;
      const TypePtr source = function_target_type(enclosing->source_type);
      context = "Z" + owner_name + integer_text(static_cast<long long>(
        last_component(enclosing->qualified_name).size())) +
        last_component(enclosing->qualified_name);
      context += "E";
      if(source) for(size_t parameter = 0; parameter < source->parameters.size(); ++parameter)
        context += TypeMangledName(source->parameters[parameter]);
      context += "E";
    }
  }

  TypePtr lambda_function_type;
  for(size_t index = 0; index < functions_.size(); ++index) {
    const FunctionRecord& function = functions_[index];
    if((!function.lambda_function && !IsLambdaOperator(function)) ||
       function.qualified_name.compare(0, closure_name.size() + 2,
         closure_name + "::") != 0 || last_component(function.qualified_name) !=
         "operator()") continue;
    lambda_function_type = function_target_type(function.source_type);
    break;
  }
  context += "Ul";
  if(!lambda_function_type || lambda_function_type->parameters.empty()) context += "v";
  else for(size_t parameter = 0; parameter < lambda_function_type->parameters.size(); ++parameter)
    context += TypeMangledName(lambda_function_type->parameters[parameter]);
  return context + "E_";
}

string PA14Lowerer::LambdaRttiLowName(const TypePtr& raw_type) const
{
  const TypePtr type = RttiValueType(raw_type);
  string closure_name;
  CPPGMAstNodePtr closure_node;
  for(map<string, TypePtr>::const_iterator it = lambda_closure_types_.begin();
      it != lambda_closure_types_.end(); ++it)
    if(it->second && (it->second.get() == type.get() ||
       PA12SameType(it->second, type, true))) {
      closure_name = it->first;
      map<string, CPPGMAstNodePtr>::const_iterator node =
        lambda_closure_nodes_.find(it->first);
      if(node != lambda_closure_nodes_.end()) closure_node = node->second;
      break;
    }
  if(closure_name.empty()) return low_symbol_component(TypeQualifiedName(type));

  const FunctionRecord* enclosing = 0;
  size_t enclosing_size = static_cast<size_t>(-1);
  for(size_t index = 0; index < functions_.size(); ++index) {
    const FunctionRecord& function = functions_[index];
    if(function.lambda_function || IsLambdaOperator(function) ||
       !SpanContains(function.node, closure_node)) continue;
    const size_t size = function.node->source_token_end -
      function.node->source_token_begin;
    if(size < enclosing_size) {
      enclosing = &function;
      enclosing_size = size;
    }
  }

  string name = closure_name;
  if(enclosing && enclosing->member && enclosing->member_owner)
    name = TypeQualifiedName(type_value(enclosing->member_owner)) + "::" + name;
  else if(enclosing && !enclosing->template_arguments.empty()) {
    const size_t token = name.find("_t");
    string arguments;
    for(size_t argument = 0; argument < enclosing->template_arguments.size(); ++argument)
      arguments += "__" + LambdaRttiTemplateArgumentLowName(
        enclosing->template_arguments[argument]);
    if(token == string::npos) name += arguments;
    else name.insert(token, arguments);
  }
  return low_symbol_component(name);
}

void PA14Lowerer::EnsureRttiType(const TypePtr& raw_type)
{
  const TypePtr type = RttiValueType(raw_type);
  if(!type || type->kind == TYPE_TEMPLATE_PARAMETER ||
     type->kind == TYPE_TEMPLATE_TEMPLATE_PARAMETER || type->kind == TYPE_FUNCTION)
    return;
  const string key = RttiMangledName(type);
  if(!demanded_rtti_types_.insert(make_pair(key, type)).second) return;
  if(type->kind == TYPE_POINTER || type->kind == TYPE_ARRAY)
    EnsureRttiType(type->child);
  else if(type->kind == TYPE_CLASS && type->direct_base)
    EnsureRttiType(type->direct_base);
}

TypePtr PA14Lowerer::TypeInfoType(Scope* scope) const
{
  Analyzer::PathTarget target = analyzer_.ResolvePath(
    analyzer_.global_.get(), "std::type_info");
  if(!target.binding || (target.binding->kind != BIND_TYPE &&
                         target.binding->kind != BIND_TYPE_ALIAS))
    throw logic_error("typeid requires std::type_info");
  TypePtr type = type_value(target.binding->type);
  if(!type || type->kind != TYPE_CLASS)
    throw logic_error("std::type_info is not a class type");
  (void)scope;
  return type;
}

bool PA14Lowerer::IsTypeidExpression(const CPPGMAstNodePtr& node) const
{
  return node && node->kind == "type-trait-expression" &&
    node->value.find("typeid") != string::npos;
}

void PA14Lowerer::IndexRttiUses(const CPPGMAstNodePtr& node, Scope* scope)
{
  if(!node) return;
  Scope* current = scope;
  map<const CPPGMAstNode*, Scope*>::const_iterator function_scope =
    analyzer_.function_scopes_.find(node.get());
  if(function_scope != analyzer_.function_scopes_.end()) current = function_scope->second;
  map<const CPPGMAstNode*, Scope*>::const_iterator compound_scope =
    analyzer_.compound_scopes_.find(node.get());
  if(compound_scope != analyzer_.compound_scopes_.end()) current = compound_scope->second;
  if(IsTypeidExpression(node)) {
    (void)InferTypeidExpression(node, current);
  } else if(node->kind == "cast-expression" &&
            PA12Operator(node->value) == "dynamic_cast" && node->children.size() >= 2) {
    const TypePtr target = analyzer_.TypeFromTypeId(node->children[0], current);
    const bool reference_target = type_is_reference(target);
    const TypePtr target_value = reference_target ? type_value(target->child) :
      type_value(target);
    if(!target_value || (target_value->kind != TYPE_POINTER &&
       target_value->kind != TYPE_CLASS) ||
       (target_value->kind == TYPE_POINTER && (!target_value->child ||
         type_value(target_value->child)->kind != TYPE_CLASS)))
      throw logic_error("unsupported dynamic_cast target");
    const TypePtr target_class = target_value->kind == TYPE_POINTER ?
      type_value(target_value->child) : target_value;
    EnsureRttiType(target_class);
    if(target_class && target_class->kind == TYPE_CLASS && target_class->polymorphic) {
      if(ShouldUseExternalVtable(target_class)) external_vtables_.insert(target_class.get());
      else emitted_vtables_.insert(target_class.get());
    }
    const TypePtr source = expression_value_type(Infer(node->children[1], current));
    const bool source_pointer = source && source->kind == TYPE_POINTER;
    const TypePtr source_class = source_pointer ?
      (source->child ? type_value(source->child) : TypePtr()) : type_value(source);
    if(!source_class || source_class->kind != TYPE_CLASS ||
       !source_class->polymorphic || (!source_pointer && !reference_target))
      throw logic_error("dynamic_cast source is not polymorphic");
    EnsureRttiType(source_class);
  }
  for(size_t child = 0; child < node->children.size(); ++child)
    IndexRttiUses(node->children[child], current);
}

PA14Lowerer::ExprInfo PA14Lowerer::InferTypeidExpression(
  const CPPGMAstNodePtr& node, Scope* scope)
{
  if(!node || node->children.empty()) throw logic_error("typeid has no operand");
  const TypePtr info_type = TypeInfoType(scope);
  const CPPGMAstNodePtr operand = node->children[0];
  TypePtr queried;
  if(operand && operand->kind == "type-id") queried =
    analyzer_.TypeFromTypeId(operand, scope);
  else queried = Infer(operand, scope).type;
  queried = RttiValueType(queried);
  if(!queried) throw logic_error("typeid operand has no type");
  EnsureRttiType(queried);
  ExprInfo result;
  // Keep the LowIR-facing representation as the address of the type_info
  // object.  The source-language type_info member comparison is validated in
  // InferBinary, while this pointer form also handles `&typeid(...)` without
  // inventing a second object layer.
  result.type = PointerTo(info_type);
  result.category = "prvalue";
  return result;
}

string PA14Lowerer::VTableSymbol(const TypePtr& type) const
{
  return low_symbol_component(TypeQualifiedName(type)) + "__vtable";
}

string PA14Lowerer::VTableAddressSymbol(const TypePtr& type) const
{
  if (external_vtables_.find(type.get()) != external_vtables_.end())
    return "__external_vtable__" + low_symbol_component(TypeQualifiedName(type));
  return VTableSymbol(type);
}

TypePtr PA14Lowerer::SemanticType(const Type* raw_type) const
{
  if (!raw_type) return TypePtr();
  for (map<const CPPGMAstNode*, TypePtr>::const_iterator it = analyzer_.class_types_.begin();
       it != analyzer_.class_types_.end(); ++it)
    if (it->second.get() == raw_type) return it->second;
  return TypePtr();
}

vector<const Type*> PA14Lowerer::OrderedTypes(const set<const Type*>& types) const
{
  vector<const Type*> result(types.begin(), types.end());
  map<const Type*, TypePtr> semantic_types;
  for (map<const CPPGMAstNode*, TypePtr>::const_iterator it = analyzer_.class_types_.begin();
       it != analyzer_.class_types_.end(); ++it)
    if (it->second) semantic_types[it->second.get()] = it->second;
  sort(result.begin(), result.end(), [this, &semantic_types](const Type* left, const Type* right) {
    map<const Type*, TypePtr>::const_iterator left_found = semantic_types.find(left);
    map<const Type*, TypePtr>::const_iterator right_found = semantic_types.find(right);
    const TypePtr left_type = left_found == semantic_types.end() ? TypePtr() : left_found->second;
    const TypePtr right_type = right_found == semantic_types.end() ? TypePtr() : right_found->second;
    const string left_name = TypeQualifiedName(left_type);
    const string right_name = TypeQualifiedName(right_type);
    if (left_name != right_name) return left_name < right_name;
    const string left_tag = left_type ? left_type->tag : string();
    const string right_tag = right_type ? right_type->tag : string();
    if (left_tag != right_tag) return left_tag < right_tag;
    const size_t left_size = left_type ? left_type->object_size : 0;
    const size_t right_size = right_type ? right_type->object_size : 0;
    return left_size < right_size;
  });
  return result;
}

bool PA14Lowerer::IsVirtualFunction(const FunctionRecord& function) const
{
  const TypePtr owner = type_value(function.member_owner);
  if (!owner || owner->kind != TYPE_CLASS || !function.member || function.static_member)
    return false;
  if (function.destructor)
    for (size_t i = 0; i < owner->virtual_methods.size(); ++i)
      if (owner->virtual_methods[i].destructor) return true;
  const string name = LastComponent(function.qualified_name);
  for (size_t i = 0; i < owner->virtual_methods.size(); ++i) {
    const VirtualMethodInfo& slot = owner->virtual_methods[i];
    if (slot.destructor || slot.name != name || !slot.binding) continue;
    FunctionRecord* source = RecordForBinding(slot.binding);
    if (source == &function) return true;
    if (function.source_type && slot.function &&
        SameLowFunctionShape(function.source_type, slot.function)) return true;
  }
  return false;
}

bool PA14Lowerer::ShouldUseExternalVtable(const TypePtr& raw_type) const
{
  const TypePtr type = type_value(raw_type);
  if (!type || !type->polymorphic) return false;
  bool pure_override = false;
  bool direct_definition = false;
  for (size_t i = 0; i < type->virtual_methods.size(); ++i) {
    const VirtualMethodInfo& slot = type->virtual_methods[i];
    if (slot.binding && slot.binding->is_override && slot.pure)
      pure_override = true;
  }
  for (size_t i = 0; i < functions_.size(); ++i) {
    const FunctionRecord& function = functions_[i];
    if (!function.definition || type_value(function.member_owner) != type ||
        !function.member || function.static_member) continue;
    if (IsVirtualFunction(function)) {
      direct_definition = true;
      break;
    }
  }
  return pure_override && !direct_definition;
}

bool PA14Lowerer::VirtualSlotForCall(const TypePtr& raw_object, Binding* binding,
                                     size_t* slot, size_t* semantic_slot) const
{
  TypePtr object = type_value(raw_object);
  if (!object || object->kind != TYPE_CLASS || !object->polymorphic || !binding)
    return false;
  const TypePtr function = function_target_type(binding->type);
  if (!function) return false;
  size_t expanded_slot = 0;
  for (size_t i = 0; i < object->virtual_methods.size(); ++i) {
    const VirtualMethodInfo& candidate = object->virtual_methods[i];
    const size_t candidate_slot = expanded_slot;
    expanded_slot += IsDestructorSlot(candidate) ? 2 : 1;
    if (IsDestructorSlot(candidate) || candidate.name != binding->name ||
        !candidate.function || candidate.function->parameters.size() != function->parameters.size() ||
        candidate.function->variadic != function->variadic ||
        candidate.function->function_const != function->function_const ||
        candidate.function->function_volatile != function->function_volatile ||
        candidate.function->function_lvalue_ref_qualified != function->function_lvalue_ref_qualified ||
        candidate.function->function_rvalue_ref_qualified != function->function_rvalue_ref_qualified)
      continue;
    bool same = true;
    for (size_t p = 0; p < function->parameters.size(); ++p)
      if (TypeText(candidate.function->parameters[p], true) !=
          TypeText(function->parameters[p], true)) { same = false; break; }
    if (!same) continue;
    // A destructor slot expands to complete and deleting entries in the
    // emitted table.  Calls after that declaration therefore use the
    // expanded entry number rather than the compact semantic-slot number.
    if (slot) *slot = candidate_slot;
    if (semantic_slot) *semantic_slot = i;
    return true;
  }
  return false;
}

bool PA14Lowerer::VirtualDestructorDeletingSlot(const TypePtr& raw_object,
                                                size_t* slot) const
{
  const TypePtr object = type_value(raw_object);
  if (!object || object->kind != TYPE_CLASS || !object->polymorphic) return false;
  size_t expanded_slot = 0;
  for (size_t i = 0; i < object->virtual_methods.size(); ++i) {
    const VirtualMethodInfo& candidate = object->virtual_methods[i];
    if (IsDestructorSlot(candidate)) {
      if (slot) *slot = expanded_slot + 1;
      return true;
    }
    ++expanded_slot;
  }
  return false;
}

bool PA14Lowerer::ContainsVirtualMemberCall(const CPPGMAstNodePtr& node,
                                            const FunctionRecord& function)
{
  if (!node) return false;
  if (node->kind == "call-expression" && !node->children.empty() &&
      node->children[0] && node->children[0]->kind == "member-expression" &&
      node->children[0]->children.size() > 1) {
    const CPPGMAstNodePtr member = node->children[0];
    const CPPGMAstNodePtr object_node = member->children[0];
    if (!member->children[1]) return false;
    const string member_name = member->children[1]->value;
    const bool object_is_this = object_node && object_node->kind == "keyword-literal" &&
      PA12Operator(object_node->value) == "this";
    TypePtr object;
    Scope* scope = function.scope;
    map<const CPPGMAstNode*, Scope*>::const_iterator scope_found =
      analyzer_.function_scopes_.find(function.node.get());
    if (scope_found != analyzer_.function_scopes_.end()) scope = scope_found->second;
    try {
      if (object_is_this) {
        object = type_value(function.member_owner);
      } else {
        ExprInfo object_info = Infer(object_node, scope);
        object = expression_value_type(object_info);
      }
      if (member->value.find("->") != string::npos) {
        if (!object_is_this) {
          if (!object || object->kind != TYPE_POINTER) object.reset();
          else object = type_value(object->child);
        }
      }
      if (object && object->kind == TYPE_CLASS) {
        const vector<Binding*> candidates = MemberBindings(object, member_name);
        for (size_t i = 0; i < candidates.size(); ++i) {
          size_t ignored_slot = 0;
          if (VirtualSlotForCall(object, candidates[i], &ignored_slot)) return true;
        }
      }
    } catch (const logic_error&) {
      // Demand discovery is conservative.  The normal function lowering
      // remains responsible for reporting an unsupported expression.
    }
  }
  for (size_t i = 0; i < node->children.size(); ++i)
    if (ContainsVirtualMemberCall(node->children[i], function)) return true;
  return false;
}

PA14Lowerer::FunctionRecord* PA14Lowerer::EnsurePureVirtual(const VirtualMethodInfo& slot)
{
  if (!slot.function || !slot.owner) return 0;
  vector<TypePtr> parameters;
  parameters.push_back(PointerTo(slot.owner));
  parameters.insert(parameters.end(), slot.function->parameters.begin(),
    slot.function->parameters.end());
  const TypePtr low_function = FunctionOf(parameters, slot.function->variadic,
    slot.function->child, false);
  for (size_t i = 0; i < functions_.size(); ++i) {
    FunctionRecord& existing = functions_[i];
    if (!existing.builtin || existing.qualified_name != "__cxa_pure_virtual") continue;
    MarkFunctionNeeded(&existing);
    return &existing;
  }
  functions_.push_back(FunctionRecord());
  FunctionRecord* record = &functions_.back();
  function_by_key_[function_key("__cxa_pure_virtual", low_function)] = record;
  record->scope = analyzer_.global_.get();
  record->source_type = slot.function;
  record->type = low_function;
  record->qualified_name = "__cxa_pure_virtual";
  record->symbol = "__cxa_pure_virtual";
  record->builtin = true;
  MarkFunctionNeeded(record);
  record->effects = "readnone";
  record->unwind_no = true;
  record->noreturn = true;
  return record;
}

PA14Lowerer::FunctionRecord* PA14Lowerer::EnsureVirtualDestructor(const TypePtr& raw_owner,
                                                     const VirtualMethodInfo& slot,
                                                     bool deleting)
{
  const TypePtr owner = type_value(raw_owner);
  if (!owner || owner->kind != TYPE_CLASS) return 0;
  const string name = "~" + LastComponent(owner->name);
  FunctionRecord* complete = 0;
  if (owner->owned_scope) {
    const vector<Binding*> candidates = DirectBindings(owner->owned_scope, name);
    for (size_t i = 0; i < candidates.size(); ++i) {
      FunctionRecord* candidate = RecordForBinding(candidates[i]);
      if (candidate && candidate->destructor && !candidate->base_entry &&
          !candidate->deleting_entry) {
        complete = candidate;
        break;
      }
    }
  }
  if (!complete) {
    const TypePtr source = FunctionOf(vector<TypePtr>(), false, Fundamental("void"), false);
    const string qname = TypeQualifiedName(owner) + "::" + name;
    const string key = function_key(qname, source);
    map<string, FunctionRecord*>::const_iterator found = function_by_key_.find(key);
    if (found != function_by_key_.end()) complete = found->second;
    if (!complete) {
      Binding binding(BIND_FUNCTION, name, source);
      binding.is_member = true;
      binding.is_static = false;
      binding.is_virtual = true;
      binding.member_owner = owner;
      binding.declaration = slot.binding ? slot.binding->declaration : CPPGMAstNodePtr();
      owner->owned_scope->add(binding);
      functions_.push_back(FunctionRecord());
      complete = &functions_.back();
      function_by_key_[key] = complete;
      complete->scope = owner->owned_scope;
      complete->source_type = source;
      complete->type = FunctionOf(vector<TypePtr>(1, PointerTo(owner)), false,
        Fundamental("void"), false);
      complete->member_owner = owner;
      complete->qualified_name = qname;
      complete->member = true;
      complete->destructor = true;
      complete->definition = true;
      CPPGMAstNodePtr special(new CPPGMAstNode("special-member-definition", name));
      CPPGMAstNodePtr declarator(new CPPGMAstNode("declarator"));
      declarator->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", name)));
      declarator->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("parameter-clause")));
      special->children.push_back(declarator);
      special->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("compound-statement")));
      complete->node = special;
    }
  }
  MarkFunctionNeeded(complete);
  if (deleting) {
    const string qname = complete->qualified_name + "__deleting_entry";
    const string key = function_key(qname, complete->source_type);
    map<string, FunctionRecord*>::const_iterator found = function_by_key_.find(key);
    if (found != function_by_key_.end()) {
      MarkFunctionNeeded(found->second);
      return found->second;
    }
    functions_.push_back(FunctionRecord());
    FunctionRecord* entry = &functions_.back();
    function_by_key_[key] = entry;
    *entry = FunctionRecord();
    entry->node = complete->node;
    entry->scope = complete->scope;
    entry->type = complete->type;
    entry->source_type = complete->source_type;
    entry->member_owner = complete->member_owner;
    entry->qualified_name = qname;
    entry->definition = true;
    entry->member = true;
    entry->destructor = true;
    entry->deleting_entry = true;
    MarkFunctionNeeded(entry);
    entry->unwind_no = complete->unwind_no;
    entry->special_initializer = complete->special_initializer;
    entry->template_instantiation = complete->template_instantiation;
    entry->inline_definition = complete->inline_definition;
    entry->weak_binding = complete->weak_binding;
    entry->template_primary = complete->template_primary;
    entry->template_arguments = complete->template_arguments;
    return entry;
  }
  // The materialized template-vtable ABI used by PA19 has no standalone D2
  // body for a class without a base; its D2 alias points at the complete
  // destructor.  Keep the PA17 base-entry model for ordinary classes and
  // for template classes that actually have a base subobject.
  if (!owner->direct_base && !owner->template_specialization && !BaseEntryFor(complete)) {
    const string qname = complete->qualified_name + "__base_entry";
    const string key = function_key(qname, complete->source_type);
    if (function_by_key_.find(key) == function_by_key_.end()) {
      functions_.push_back(FunctionRecord());
      FunctionRecord* entry = &functions_.back();
      function_by_key_[key] = entry;
      entry->node = complete->node;
      entry->scope = complete->scope;
      entry->type = complete->type;
      entry->source_type = complete->source_type;
      entry->member_owner = complete->member_owner;
      entry->qualified_name = qname;
      entry->definition = true;
      entry->member = true;
      entry->destructor = true;
      MarkFunctionNeeded(entry);
      entry->unwind_no = complete->unwind_no;
      entry->base_entry = true;
      entry->base_entry_for = complete->qualified_name;
      entry->special_initializer = complete->special_initializer;
    }
  }
  return complete;
}

PA14Lowerer::FunctionRecord* PA14Lowerer::VirtualFunctionRecord(const TypePtr& raw_owner,
                                                   const VirtualMethodInfo& slot)
{
  const TypePtr owner = type_value(raw_owner);
  if (IsDestructorSlot(slot)) return EnsureVirtualDestructor(owner, slot, false);
  if (slot.pure) return EnsurePureVirtual(slot);
  FunctionRecord* record = slot.binding ? RecordForBinding(slot.binding) : 0;
  if (!record && slot.owner)
    record = FindFunction(TypeQualifiedName(slot.owner) + "::" + slot.name, slot.function);
  if (record) MarkFunctionNeeded(record);
  return record;
}

void PA14Lowerer::PreparePolymorphicModel()
{
  emitted_rtti_.clear();
  // A class with a real virtual definition is a vtable root.  Constructors
  // and base-subobject construction then pull the required inherited tables
  // into the same model below.
  for (map<const CPPGMAstNode*, TypePtr>::const_iterator it = analyzer_.class_types_.begin();
       it != analyzer_.class_types_.end(); ++it) {
    const TypePtr type = it->second;
    if (!type || !type->polymorphic) continue;
    bool root = false;
    for (size_t i = 0; i < functions_.size(); ++i) {
      const FunctionRecord& function = functions_[i];
      if (!function.definition || type_value(function.member_owner) != type ||
          !function.member || function.static_member) continue;
      if (!IsVirtualFunction(function)) continue;
      const string declared_name = function.node && function.node->children.size() > 1 ?
        declarator_name(function.node->children[1]) : string();
      const bool key_function = declared_name.find("::") != string::npos;
      if(function.needed || key_function ||
         complete_template_object_uses_.find(type.get()) !=
           complete_template_object_uses_.end()) { root = true; break; }
    }
    if (root) emitted_vtables_.insert(type.get());
  }
  bool changed = true;
  while (changed) {
    changed = false;
    vector<const Type*> current;
    current.insert(current.end(), emitted_vtables_.begin(), emitted_vtables_.end());
    current.insert(current.end(), external_vtables_.begin(), external_vtables_.end());
    for (size_t i = 0; i < current.size(); ++i) {
      const Type* raw_base = current[i]->direct_base.get();
      if (!raw_base || !raw_base->polymorphic) continue;
      const TypePtr base = SemanticType(raw_base);
      if (ShouldUseExternalVtable(base)) {
        if (external_vtables_.insert(raw_base).second) changed = true;
      } else if (emitted_vtables_.insert(raw_base).second) changed = true;
    }
  }

  // A polymorphic object always needs a constructor action even when PA16's
  // value-semantics demand pass would otherwise omit an empty implicit
  // constructor.  External abstract tables still need their base-entry
  // constructor to establish the correct intermediate vptr.
  set<const Type*> constructor_type_set = emitted_vtables_;
  constructor_type_set.insert(external_vtables_.begin(), external_vtables_.end());
  const vector<const Type*> constructor_types = OrderedTypes(constructor_type_set);
  for (size_t type_index = 0; type_index < constructor_types.size(); ++type_index) {
    const TypePtr type = SemanticType(constructor_types[type_index]);
    if (!type || !type->owned_scope) continue;
    CollectImplicitConstructor(type, type->owned_scope, true);
    const vector<Binding*> constructors = DirectBindings(type->owned_scope,
      LastComponent(type->name));
    for (size_t i = 0; i < constructors.size(); ++i) {
      FunctionRecord* record = RecordForBinding(constructors[i]);
      if (record && record->constructor && !record->static_member) {
        if (!record->implicit_constructor && constructors[i]->access == "protected")
          MarkFunctionNeeded(record);
        EnsureConstructorBaseEntry(record);
        break;
      }
    }
  }

  const vector<const Type*> emitted_types = OrderedTypes(emitted_vtables_);
  for (size_t type_index = 0; type_index < emitted_types.size(); ++type_index) {
    const TypePtr type = SemanticType(emitted_types[type_index]);
    for (size_t slot = 0; slot < type->virtual_methods.size(); ++slot) {
      const VirtualMethodInfo& method = type->virtual_methods[slot];
      if (method.pure) EnsurePureVirtual(method);
      else if (IsDestructorSlot(method)) {
        EnsureVirtualDestructor(type, method, false);
        EnsureVirtualDestructor(type, method, true);
      } else {
        FunctionRecord* record = VirtualFunctionRecord(type, method);
        if (record) MarkFunctionNeeded(record);
      }
    }
  }

  // A pure declaration supplies only the slot type; the table points at the
  // shared runtime pure-virtual entry.  Calls through that slot must not
  // resurrect the source declaration as an undefined LowIR function.
  for (size_t i = 0; i < functions_.size(); ++i) {
    FunctionRecord& function = functions_[i];
    if (!function.member || !function.member_owner) continue;
    const TypePtr owner = type_value(function.member_owner);
    if (!owner) continue;
    const string name = LastComponent(function.qualified_name);
    for (size_t slot = 0; slot < owner->virtual_methods.size(); ++slot) {
      const VirtualMethodInfo& method = owner->virtual_methods[slot];
      if (!method.pure || method.name != name || !method.function) continue;
      if (!function.definition || SameLowFunctionShape(function.source_type, method.function)) {
        function.needed = false;
        break;
      }
    }
  }

  // A member body containing a call through a class member can be the only
  // observable use of a virtual slot (for example, a callback helper that is
  // not itself called by main).  Discover that dependency through the typed
  // receiver and virtual-slot map so name collisions cannot manufacture an
  // unrelated demand edge.
  for (size_t i = 0; i < functions_.size(); ++i) {
    if (!functions_[i].member || !functions_[i].definition || !functions_[i].node)
      continue;
    vector<bool> needed_before;
    needed_before.reserve(functions_.size());
    for (size_t j = 0; j < functions_.size(); ++j)
      needed_before.push_back(functions_[j].needed);
    const bool has_virtual_call = ContainsVirtualMemberCall(functions_[i].node,
      functions_[i]);
    for (size_t j = 0; j < needed_before.size() && j < functions_.size(); ++j)
      functions_[j].needed = needed_before[j];
    if (has_virtual_call) MarkFunctionNeeded(&functions_[i]);
  }
}

void PA14Lowerer::EmitVPointerStore(const TypePtr& owner, const string& address)
{
  if (!owner || !owner->polymorphic || address.empty()) return;
  const string vtable = VTableAddressSymbol(owner);
  const string table_address = new_temp();
  AddInstruction(table_address + " = addr @" + vtable);
  const string vptr = new_temp();
  AddInstruction(vptr + " = index i8 " + table_address + ", 16");
  emit_store(PointerTo(Fundamental("char")), vptr, address);
}

void PA14Lowerer::EmitPolymorphicGlobals(vector<string>& entries)
{
  for(set<const Type*>::const_iterator it = emitted_vtables_.begin(); it != emitted_vtables_.end(); ++it) EnsureRttiType(SemanticType(*it));
  for(set<const Type*>::const_iterator it = external_vtables_.begin(); it != external_vtables_.end(); ++it) EnsureRttiType(SemanticType(*it));
  if (emitted_vtables_.empty() && external_vtables_.empty() && demanded_rtti_types_.empty()) return;
  bool has_class = false, has_fundamental = false, has_pointer = false, has_si = false;
  for(map<string, TypePtr>::const_iterator it = demanded_rtti_types_.begin(); it != demanded_rtti_types_.end(); ++it) {
    const TypePtr type = RttiValueType(it->second);
    if(!type) continue;
    if(type->kind == TYPE_FUNDAMENTAL) has_fundamental = true;
    else if(type->kind == TYPE_POINTER || type->kind == TYPE_ARRAY) has_pointer = true;
    else if(type->kind == TYPE_CLASS || type->kind == TYPE_ENUM) { has_class = true; if(type->kind == TYPE_CLASS && type->direct_base) has_si = true; }
  }
  if(has_fundamental)
    entries.push_back("declare global @__external_rtti_vtable____fundamental_type_info [binding=strong, object=_ZTVN10__cxxabiv123__fundamental_type_infoE]");
  if(has_pointer)
    entries.push_back("declare global @__external_rtti_vtable____pointer_type_info [binding=strong, object=_ZTVN10__cxxabiv119__pointer_type_infoE]");
  if(has_class)
    entries.push_back("declare global @__external_rtti_vtable____class_type_info [binding=strong, object=_ZTVN10__cxxabiv117__class_type_infoE]");
  if(has_si)
    entries.push_back("declare global @__external_rtti_vtable____si_class_type_info [binding=strong, object=_ZTVN10__cxxabiv120__si_class_type_infoE]");

  const vector<const Type*> emitted_types = OrderedTypes(emitted_vtables_);
  const vector<const Type*> external_types = OrderedTypes(external_vtables_);
  for (size_t type_index = 0; type_index < external_types.size(); ++type_index) {
    const TypePtr type = SemanticType(external_types[type_index]);
    entries.push_back("declare global @__external_vtable__" +
      low_symbol_component(TypeQualifiedName(type)) + " [binding=strong, object=_ZTV" +
      TypeMangledName(type) + "]");
  }

  vector<TypePtr> ordered_rtti_types;
  for(map<string, TypePtr>::const_iterator it = demanded_rtti_types_.begin();
      it != demanded_rtti_types_.end(); ++it)
    ordered_rtti_types.push_back(RttiValueType(it->second));
  sort(ordered_rtti_types.begin(), ordered_rtti_types.end(),
    [this](const TypePtr& left, const TypePtr& right) {
      if(RttiMangledName(left) != RttiMangledName(right))
        return RttiMangledName(left) < RttiMangledName(right);
      return RttiInfoSymbol(left) < RttiInfoSymbol(right);
    });
  for (size_t type_index = 0; type_index < ordered_rtti_types.size(); ++type_index) {
    const TypePtr type = ordered_rtti_types[type_index];
    if(!type) continue;
    const string info_symbol = RttiInfoSymbol(type);
    const string rtti_symbol = RttiSymbol(type);
    const string mangled = RttiMangledName(type);
    ostringstream name;
    name << "global @" << info_symbol << " [storage=readonly, binding=weak, object=_ZTS" << mangled << "] = {\n";
    for (size_t i = 0; i < mangled.size(); ++i)
      name << "  i8 " << static_cast<unsigned int>(static_cast<unsigned char>(mangled[i])) << "\n";
    name << "  i8 0\n}";
    entries.push_back(name.str());
    ostringstream rtti;
    rtti << "global @" << rtti_symbol <<
      " [storage=readonly, binding=weak, object=_ZTI" << mangled << "] = {\n";
    if(type->kind == TYPE_FUNDAMENTAL)
      rtti << "  ptr addr @__external_rtti_vtable____fundamental_type_info + 16\n";
    else if(type->kind == TYPE_POINTER || type->kind == TYPE_ARRAY ||
            type->kind == TYPE_FUNCTION || type->kind == TYPE_MEMBER_POINTER)
      rtti << "  ptr addr @__external_rtti_vtable____pointer_type_info + 16\n";
    else if (type->direct_base && !ShouldUseExternalVtable(type))
      rtti << "  ptr addr @__external_rtti_vtable____si_class_type_info + 16\n";
    else
      rtti << "  ptr addr @__external_rtti_vtable____class_type_info + 16\n";
    rtti << "  ptr addr @" << info_symbol << "\n";
    if(type->kind == TYPE_POINTER || type->kind == TYPE_ARRAY ||
       type->kind == TYPE_FUNCTION || type->kind == TYPE_MEMBER_POINTER) {
      const bool incomplete_pointee = type->child &&
        RttiNeedsTypeMangledClassName(RttiValueType(type->child));
      rtti << "  i32 " << (incomplete_pointee ? 8 : 0) << "\n";
      rtti << "  ptr addr @" << RttiSymbol(type->child) << "\n";
    } else if (type->direct_base && !ShouldUseExternalVtable(type)) {
      const TypePtr base = type->direct_base;
      rtti << "  ptr addr @" << RttiSymbol(base) << "\n";
    }
    rtti << "}";
    entries.push_back(rtti.str());
  }
  for (size_t type_index = 0; type_index < emitted_types.size(); ++type_index) {
    const TypePtr type = SemanticType(emitted_types[type_index]);
    const string mangled = TypeMangledName(type);
    bool strong = false;
    for (size_t i = 0; i < functions_.size(); ++i) {
      const FunctionRecord& function = functions_[i];
      if (!function.definition || type_value(function.member_owner) != type ||
          !function.member || !IsVirtualFunction(function)) continue;
      if (function.node && FirstIdentifier(function.node).find("::") != string::npos)
        strong = true;
    }
    ostringstream table;
    table << "global @" << VTableSymbol(type) <<
      " [storage=readonly, binding=" << (strong ? "strong" : "weak") <<
      ", object=_ZTV" << mangled << "] = {\n";
    table << "  i64 0\n  ptr addr @" << RttiSymbol(type) << "\n";
    for (size_t i = 0; i < type->virtual_methods.size(); ++i) {
      const VirtualMethodInfo& slot = type->virtual_methods[i];
      if (slot.pure) {
        FunctionRecord* pure = EnsurePureVirtual(slot);
        table << "  ptr addr @" << (pure ? pure->symbol : "__cxa_pure_virtual") << "\n";
        continue;
      }
      if (IsDestructorSlot(slot)) {
        FunctionRecord* complete = EnsureVirtualDestructor(type, slot, false);
        FunctionRecord* deleting = EnsureVirtualDestructor(type, slot, true);
        table << "  ptr addr @" << (complete ? complete->symbol : "__missing_destructor") << "\n";
        table << "  ptr addr @" << (deleting ? deleting->symbol : "__missing_deleting_destructor") << "\n";
        continue;
      }
      FunctionRecord* function = VirtualFunctionRecord(type, slot);
      if (!function) {
        table << "  ptr addr @__cxa_pure_virtual\n";
      } else {
        MarkFunctionNeeded(function);
        table << "  ptr addr @" << function->symbol << "\n";
      }
    }
    table << "}";
    entries.push_back(table.str());
  }
}

} // namespace cppgm_pa14_lowering
