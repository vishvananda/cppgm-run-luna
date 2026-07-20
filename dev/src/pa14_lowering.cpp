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

string trim_type_name(const string& name)
{
  if(name == "signed char") return "char";
  if(name == "signed int") return "int";
  if(name == "signed short int") return "short int";
  if(name == "signed long int") return "long int";
  if(name == "signed long long int") return "long long int";
  return name;
}

bool type_is_reference(const TypePtr& type)
{
  return type && (type->kind == TYPE_LVALUE_REFERENCE ||
                  type->kind == TYPE_RVALUE_REFERENCE);
}

TypePtr type_value(const TypePtr& type)
{
  return type_is_reference(type) ? type->child : type;
}

string last_component(const string& name)
{
  const size_t p = name.rfind("::");
  return p == string::npos ? name : name.substr(p + 2);
}

string low_symbol_component(const string& name)
{
  string result;
  for(size_t i = 0; i < name.size(); ++i) {
    const unsigned char ch = static_cast<unsigned char>(name[i]);
    if(isalnum(ch) || name[i] == '_') result += name[i];
    else if(name[i] == '~') result += "_";
    else if(name[i] == ':' && i + 1 < name.size() && name[i + 1] == ':') {
      result += "__";
      ++i;
    }
  }
  if(result.empty()) result = "anonymous";
  return result;
}

bool is_integral_type(const TypePtr& type)
{
  TypePtr value = type_value(type);
  if(!value) return false;
  if(value->kind == TYPE_ENUM) return !value->scoped_enum;
  return value->kind == TYPE_FUNDAMENTAL &&
    value->name != "bool" && value->name != "float" &&
    value->name != "double" && value->name != "long double" &&
    value->name != "void" && value->name != "nullptr_t";
}

bool is_arithmetic_type(const TypePtr& type)
{
  TypePtr value = type_value(type);
  return is_integral_type(value) ||
    (value && value->kind == TYPE_FUNDAMENTAL &&
      (value->name == "bool" || value->name == "float" ||
       value->name == "double" || value->name == "long double"));
}

bool is_floating_type(const TypePtr& type)
{
  TypePtr value = type_value(type);
  return value && value->kind == TYPE_FUNDAMENTAL &&
    (value->name == "float" || value->name == "double" ||
     value->name == "long double");
}

bool is_unsigned_type(const TypePtr& type)
{
  TypePtr value = type_value(type);
  if(!value) return false;
  if(value->kind == TYPE_ENUM)
    return value->underlying && is_unsigned_type(value->underlying);
  return value->kind == TYPE_FUNDAMENTAL &&
    (value->name == "bool" || value->name.find("unsigned") != string::npos);
}

bool is_pointer_like(const TypePtr& type)
{
  TypePtr value = type_value(type);
  return value && (value->kind == TYPE_POINTER || value->kind == TYPE_FUNCTION ||
                   value->kind == TYPE_ARRAY);
}

string integer_text(long long value)
{
  ostringstream out;
  out << value;
  return out.str();
}

string strip_literal_suffix(string value)
{
  while(!value.empty()) {
    const char ch = value[value.size() - 1];
    if(ch == 'u' || ch == 'U' || ch == 'l' || ch == 'L') value.erase(value.size() - 1);
    else break;
  }
  return value;
}

long long parse_integer_literal(const string& raw, bool* okay)
{
  string value = strip_literal_suffix(raw);
  if(value.size() >= 2 && value[0] == '\'' && value[value.size() - 1] == '\'') {
    if(okay) *okay = true;
    string body = value.substr(1, value.size() - 2);
    if(body.empty()) { if(okay) *okay = false; return 0; }
    if(body[0] != '\\') return static_cast<unsigned char>(body[0]);
    if(body.size() == 2) {
      switch(body[1]) {
      case 'a': return 7;
      case 'b': return 8;
      case 'f': return 12;
      case 'n': return 10;
      case 'r': return 13;
      case 't': return 9;
      case 'v': return 11;
      case '\\': return 92;
      case '\'': return 39;
      case '"': return 34;
      case '0': return 0;
      default: return static_cast<unsigned char>(body[1]);
      }
    }
    if(body.size() > 2 && (body[1] == 'x' || body[1] == 'X')) {
      char* end = 0;
      errno = 0;
      const long long result = strtoll(body.c_str() + 2, &end, 16);
      if(okay) *okay = errno != ERANGE && end != body.c_str() + 2 && *end == '\0';
      return result;
    }
    char* end = 0;
    errno = 0;
    const long long result = strtoll(body.c_str() + 1, &end, 8);
    if(okay) *okay = errno != ERANGE && end != body.c_str() + 1 && *end == '\0';
    return result;
  }
  if(value.empty()) { if(okay) *okay = false; return 0; }
  char* end = 0;
  errno = 0;
  const long long result = strtoll(value.c_str(), &end, 0);
  const bool valid = errno != ERANGE && end != value.c_str() && *end == '\0';
  if(okay) *okay = valid;
  return valid ? result : 0;
}

vector<unsigned char> decode_string_literal(const string& raw)
{
  size_t begin = 0;
  if(raw.size() > 0 && (raw[0] == 'u' || raw[0] == 'U' || raw[0] == 'L')) begin = 1;
  if(begin >= raw.size() || raw[begin] != '"') return vector<unsigned char>();
  vector<unsigned char> result;
  for(size_t i = begin + 1; i + 1 < raw.size(); ++i) {
    unsigned char value = static_cast<unsigned char>(raw[i]);
    if(raw[i] != '\\') {
      result.push_back(value);
      continue;
    }
    if(++i + 1 > raw.size()) break;
    const char escaped = raw[i];
    switch(escaped) {
    case 'a': result.push_back(7); break;
    case 'b': result.push_back(8); break;
    case 'f': result.push_back(12); break;
    case 'n': result.push_back(10); break;
    case 'r': result.push_back(13); break;
    case 't': result.push_back(9); break;
    case 'v': result.push_back(11); break;
    case '\\': result.push_back('\\'); break;
    case '\'': result.push_back('\''); break;
    case '"': result.push_back('"'); break;
    case '?': result.push_back('?'); break;
    case 'x': case 'X': {
      size_t j = i + 1;
      string digits;
      while(j < raw.size() - 1 && isxdigit(static_cast<unsigned char>(raw[j]))) {
        digits += raw[j++];
      }
      if(digits.empty()) result.push_back(static_cast<unsigned char>(escaped));
      else {
        char* end = 0;
        result.push_back(static_cast<unsigned char>(strtoul(digits.c_str(), &end, 16)));
        i = j - 1;
      }
      break;
    }
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': {
      string digits(1, escaped);
      size_t j = i + 1;
      while(j < raw.size() - 1 && digits.size() < 3 && raw[j] >= '0' && raw[j] <= '7')
        digits += raw[j++];
      char* end = 0;
      result.push_back(static_cast<unsigned char>(strtoul(digits.c_str(), &end, 8)));
      i = j - 1;
      break;
    }
    default: result.push_back(static_cast<unsigned char>(escaped)); break;
    }
  }
  result.push_back(0);
  return result;
}

string canonical_literal(const string& raw, TypePtr* type_out,
                         long long* constant, bool* known)
{
  if(type_out) *type_out = TypePtr();
  if(constant) *constant = 0;
  if(known) *known = false;
  if(raw.empty()) return "0";
  const size_t prefix = (raw.size() > 1 && (raw[0] == 'u' || raw[0] == 'U' || raw[0] == 'L')) ? 1 : 0;
  if(raw[prefix] == '"') {
    if(type_out) *type_out = ArrayOf(
      static_cast<long long>(decode_string_literal(raw).size()), Fundamental("char"));
    return raw;
  }
  if(raw[prefix] == '\'') {
    bool okay = false;
    const long long value = parse_integer_literal(raw.substr(prefix), &okay);
    if(type_out) *type_out = Fundamental(prefix == 1 && raw[0] == 'u' ? "char16_t" :
      prefix == 1 && raw[0] == 'U' ? "char32_t" :
      prefix == 1 && raw[0] == 'L' ? "wchar_t" : "char");
    if(constant) *constant = value;
    if(known) *known = okay;
    return integer_text(value);
  }
  const bool hexadecimal = raw.size() > 2 && raw[0] == '0' &&
    (raw[1] == 'x' || raw[1] == 'X');
  const bool floating = raw.find('.') != string::npos ||
    (!hexadecimal && (raw.find('e') != string::npos || raw.find('E') != string::npos)) ||
    (hexadecimal && (raw.find('p') != string::npos || raw.find('P') != string::npos));
  if(floating) {
    const char suffix = raw[raw.size() - 1];
    if(type_out) *type_out = suffix == 'f' || suffix == 'F' ? Fundamental("float") :
      suffix == 'l' || suffix == 'L' ? Fundamental("long double") : Fundamental("double");
    return raw;
  }
  bool okay = false;
  const long long value = parse_integer_literal(raw, &okay);
  bool uns = false;
  unsigned int longs = 0;
  for(size_t i = 0; i < raw.size(); ++i) {
    if(raw[i] == 'u' || raw[i] == 'U') uns = true;
    if(raw[i] == 'l' || raw[i] == 'L') ++longs;
  }
  if(type_out) *type_out = longs >= 2 ? Fundamental(uns ? "unsigned long long int" : "long long int") :
    longs == 1 ? Fundamental(uns ? "unsigned long int" : "long int") :
    Fundamental(uns ? "unsigned int" : "int");
  if(constant) *constant = value;
  if(known) *known = okay;
  return integer_text(value);
}

} // namespace

namespace cppgm_pa14_lowering {

PA14Lowerer::PA14Lowerer(const vector<CPPGMAstNodePtr>& trees)
    : trees_(trees), program_(new CPPGMAstNode("translation-unit")), analyzer_(),
      functions_(), globals_(), function_by_key_(), global_by_key_(),
      string_data_(), string_symbols_(), string_order_(), needs_init_helper_(false),
      needs_fini_helper_(false), state_()
{}

void PA14Lowerer::Lower(ostream& out)
{
    for(size_t i = 0; i < trees_.size(); ++i) {
      if(!trees_[i] || trees_[i]->kind != "translation-unit")
        throw logic_error("invalid translation unit for LowIR");
      for(size_t j = 0; j < trees_[i]->children.size(); ++j)
        program_->children.push_back(trees_[i]->children[j]);
    }
    analyzer_.Analyze(program_);
    InstallBuiltins();
    CollectTopLevel(program_, analyzer_.global_.get());
    FinalizeSymbols();
    CollectStringLiterals(program_);

    vector<string> entries;
    EmitGlobals(entries);
    // Emit ordinary functions first so their calls establish the roots of
    // the demand-driven member-function set.  Member bodies can in turn
    // reach other member bodies, so walk that set to a fixed point.
    for(size_t i = 0; i < functions_.size(); ++i) {
      if(!functions_[i].definition || functions_[i].member) continue;
      entries.push_back(EmitFunction(functions_[i]));
      functions_[i].emitted = true;
    }
    bool added_member = true;
    while(added_member) {
      added_member = false;
      for(size_t i = 0; i < functions_.size(); ++i) {
        FunctionRecord& function = functions_[i];
        if(!function.definition || !function.member || !function.needed || function.emitted)
          continue;
        entries.push_back(EmitFunction(function));
        function.emitted = true;
        added_member = true;
      }
    }
    EmitDynamicInitializers(entries);
    added_member = true;
    while(added_member) {
      added_member = false;
      for(size_t i = 0; i < functions_.size(); ++i) {
        FunctionRecord& function = functions_[i];
        if(!function.definition || !function.member || !function.needed || function.emitted)
          continue;
        entries.push_back(EmitFunction(function));
        function.emitted = true;
        added_member = true;
      }
    }

    vector<string> declarations;
    EmitDeclarations(declarations);
    entries.insert(entries.begin(), declarations.begin(), declarations.end());

    for(size_t i = 0; i < entries.size(); ++i) {
      if(i != 0) out << "\n";
      out << entries[i];
      if(entries[i].empty() || entries[i][entries[i].size() - 1] != '\n') out << "\n";
    }
  }

string PA14Lowerer::function_key(const string& name, const TypePtr& type)
{
    return name + "|" + TypeText(type, true);
  }

string PA14Lowerer::global_key(const string& name)
{
    return name;
  }

string PA14Lowerer::low_type(const TypePtr& raw) const
{
    if(type_is_reference(raw)) return "ptr";
    TypePtr type = type_value(raw);
    if(!type) throw logic_error("missing type during LowIR lowering");
    if(type->kind == TYPE_POINTER || type->kind == TYPE_FUNCTION ||
       type->kind == TYPE_MEMBER_POINTER) return "ptr";
    if(type->kind == TYPE_ARRAY) {
      ostringstream result;
      result << "obj<" << type_size(type) << "x" << type_alignment(type) << ">";
      return result.str();
    }
    if(type->kind == TYPE_CLASS) {
      ostringstream result;
      result << "obj<" << type_size(type) << "x" << type_alignment(type) << ">";
      return result.str();
    }
    if(type->kind == TYPE_ENUM) {
      const string underlying = low_type(type->underlying ? type->underlying : Fundamental("int"));
      if(underlying == "u8") return "i8";
      if(underlying == "u16") return "i16";
      if(underlying == "u32") return "i32";
      if(underlying == "u64") return "i64";
      return underlying;
    }
    if(type->kind != TYPE_FUNDAMENTAL) return "ptr";
    const string name = trim_type_name(type->name);
    if(name == "void") return "void";
    if(name == "bool") return "u8";
    if(name == "unsigned char") return "u8";
    if(name == "signed char") return "i8";
    if(name == "char") return "i8";
    if(name == "char16_t" || name == "short int" || name == "unsigned short int")
      return name == "unsigned short int" ? "u16" : "i16";
    if(name == "char32_t" || name == "wchar_t") return "u32";
    if(name == "int") return "i32";
    if(name == "unsigned int") return "u32";
    if(name == "long int" || name == "long long int" ||
       name == "unsigned long int" || name == "unsigned long long int") return "i64";
    if(name == "float") return "f32";
    if(name == "double") return "f64";
    if(name == "long double") return "f80";
    if(name == "nullptr_t") return "ptr";
    return "i32";
  }

size_t PA14Lowerer::type_size(const TypePtr& type) const
{
    if(!type) throw logic_error("missing type during size computation");
    return analyzer_.TypeSize(type_value(type));
  }

size_t PA14Lowerer::type_alignment(const TypePtr& type) const
{
    if(!type) throw logic_error("missing type during alignment computation");
    return max<size_t>(1, analyzer_.TypeAlignment(type_value(type)));
  }

string PA14Lowerer::storage_type(const TypePtr& type) const
{
    if(type_is_reference(type)) return "ptr";
    return low_type(type);
  }

string PA14Lowerer::qualified_name(Scope* scope, const string& raw) const
{
    if(raw.find("::") != string::npos) return raw;
    if(scope && !scope->qualified_prefix.empty()) return scope->qualified_prefix + "::" + raw;
    return raw;
  }

string PA14Lowerer::TypeQualifiedName(const TypePtr& type) const
{
    if(type && type->owned_scope && !type->owned_scope->qualified_prefix.empty())
      return type->owned_scope->qualified_prefix;
    return type ? type->name : string();
  }

void PA14Lowerer::InstallBuiltins()
{
    const TypePtr character = Fundamental("char");
    const TypePtr const_character = CloneWithCv(character, true, false);
    const TypePtr size_type = Fundamental("unsigned long int");

    struct BuiltinSpec
    {
      const char* name;
      TypePtr type;
      const char* effects;
      const char* object_name;
      bool noreturn;
      const char* first_parameter;
      const char* second_parameter;
    };

    vector<TypePtr> strlen_parameters;
    strlen_parameters.push_back(PointerTo(const_character));
    vector<TypePtr> byte_copy_parameters;
    byte_copy_parameters.push_back(PointerTo(character));
    byte_copy_parameters.push_back(PointerTo(const_character));
    byte_copy_parameters.push_back(size_type);
    vector<TypePtr> no_parameters;

    vector<BuiltinSpec> specs;
    specs.push_back(BuiltinSpec{"__builtin_strlen",
      FunctionOf(strlen_parameters, false, size_type, false), "readonly",
      "cppgm_builtin_strlen", false,
      "capture=nocapture, access=read", 0});
    specs.push_back(BuiltinSpec{"__builtin_unreachable",
      FunctionOf(no_parameters, false, Fundamental("void"), false), "readnone",
      "cppgm_builtin_unreachable", true, 0, 0});
    specs.push_back(BuiltinSpec{"__builtin_memcpy",
      FunctionOf(byte_copy_parameters, false, PointerTo(Fundamental("void")), false),
      "readwrite", "cppgm_builtin_memcpy", false,
      "capture=nocapture, access=write, alias=noalias",
      "capture=nocapture, access=read, alias=noalias"});
    specs.push_back(BuiltinSpec{"__builtin_memmove",
      FunctionOf(byte_copy_parameters, false, PointerTo(Fundamental("void")), false),
      "readwrite", "cppgm_builtin_memmove", false,
      "capture=nocapture, access=readwrite", "capture=nocapture, access=read"});

    for(size_t i = 0; i < specs.size(); ++i) {
      const BuiltinSpec& spec = specs[i];
      Binding binding(BIND_FUNCTION, spec.name, spec.type);
      binding.qualified_name = spec.name;
      binding.declaration = CPPGMAstNodePtr();
      analyzer_.global_->add(binding);

      functions_.push_back(FunctionRecord());
      FunctionRecord* record = &functions_.back();
      function_by_key_[function_key(spec.name, spec.type)] = record;
      record->scope = analyzer_.global_.get();
      record->source_type = spec.type;
      record->type = spec.type;
      record->qualified_name = spec.name;
      record->symbol = spec.name;
      record->builtin = true;
      record->unwind_no = true;
      record->noreturn = spec.noreturn;
      record->effects = spec.effects;
      record->object_name = spec.object_name;
      if(spec.first_parameter) record->parameter_metadata.push_back(spec.first_parameter);
      if(spec.second_parameter) record->parameter_metadata.push_back(spec.second_parameter);
      if(spec.first_parameter && spec.type->parameters.size() > 2)
        record->parameter_metadata.push_back(string());
    }
  }

bool PA14Lowerer::HasNoexcept(const CPPGMAstNodePtr& node) const
{
    if(!node) return false;
    if(node->kind == "function-qualifier" && node->value == "noexcept") return true;
    for(size_t i = 0; i < node->children.size(); ++i)
      if(HasNoexcept(node->children[i])) return true;
    return false;
  }

string PA14Lowerer::declarator_name(const CPPGMAstNodePtr& node) const
{
    return FirstIdentifier(node);
  }

TypePtr PA14Lowerer::declared_type(const CPPGMAstNodePtr& node, Scope* scope,
                       Analyzer::SpecFacts* facts)
{
    if(!node || node->children.empty()) throw logic_error("invalid declaration during LowIR lowering");
    Analyzer::SpecFacts local;
    Analyzer::SpecFacts& info = facts ? *facts : local;
    TypePtr type = analyzer_.TypeFromSpecSeq(node->children[0], scope, &info);
    if(node->children.size() > 1 && node->children[1] &&
       (node->children[1]->kind == "declarator" ||
        node->children[1]->kind == "abstract-declarator"))
      type = analyzer_.BuildDeclarator(node->children[1], type, scope);
    return type;
  }

TypePtr PA14Lowerer::function_type(const TypePtr& raw) const
{
    TypePtr type = type_value(raw);
    if(type && type->kind == TYPE_FUNCTION) return type;
    if(type && type->kind == TYPE_POINTER && type->child &&
       type->child->kind == TYPE_FUNCTION) return type->child;
    return TypePtr();
  }

void PA14Lowerer::CollectTopLevel(const CPPGMAstNodePtr& node, Scope* scope)
{
    if(!node) return;
    if(node->kind == "translation-unit") {
      for(size_t i = 0; i < node->children.size(); ++i)
        CollectTopLevel(node->children[i], scope);
      return;
    }
    if(node->kind == "namespace-definition") {
      map<const CPPGMAstNode*, Scope*>::iterator found = analyzer_.namespace_scopes_.find(node.get());
      Scope* child = found == analyzer_.namespace_scopes_.end() ? scope : found->second;
      for(size_t i = 0; i < node->children.size(); ++i)
        if(node->children[i] && node->children[i]->kind != "inline")
          CollectTopLevel(node->children[i], child);
      return;
    }
    if(node->kind == "linkage-specification" ||
       node->kind == "explicit-instantiation-declaration") {
      for(size_t i = 0; i < node->children.size(); ++i)
        CollectTopLevel(node->children[i], scope);
      return;
    }
    if(node->kind == "template-declaration") {
      if(node->children.size() > 1) CollectTopLevel(node->children[1], scope);
      return;
    }
    if(node->kind == "class-specifier") {
      CollectClassMembers(node, scope);
      return;
    }
    if(node->kind == "function-definition") {
      CollectFunction(node, scope, true);
      return;
    }
    if(node->kind == "special-member-definition") {
      const size_t separator = node->value.rfind("::");
      Scope* owner_scope = scope;
      if(separator != string::npos) {
        Analyzer::PathTarget owner = analyzer_.ResolvePath(scope, node->value.substr(0, separator));
        TypePtr owner_type = owner.binding ? owner.binding->type :
          (owner.scope ? owner.scope->owner_type : TypePtr());
        if(owner_type && owner_type->kind == TYPE_CLASS && owner_type->owned_scope)
          owner_scope = owner_type->owned_scope;
      }
      CollectSpecialMember(node, owner_scope, true);
      return;
    }
    if(node->kind == "simple-declaration" || node->kind == "bit-field-declaration") {
      CollectSimpleDeclaration(node, scope);
      return;
    }
  }

void PA14Lowerer::CollectClassMembers(const CPPGMAstNodePtr& node, Scope* scope)
{
    if(!node) return;
    map<const CPPGMAstNode*, TypePtr>::const_iterator type_found =
      analyzer_.class_types_.find(node.get());
    if(type_found == analyzer_.class_types_.end() || !type_found->second ||
       !type_found->second->owned_scope) return;
    Scope* class_scope = type_found->second->owned_scope;
    for(size_t i = 0; i < node->children.size(); ++i) {
      const CPPGMAstNodePtr child = node->children[i];
      if(!child) continue;
      if(child->kind == "class-specifier") {
        CollectClassMembers(child, class_scope);
        continue;
      }
      if(child->kind == "function-definition") {
        CollectFunction(child, class_scope, true);
        continue;
      }
      if(child->kind == "special-member-definition" ||
         child->kind == "special-member-declaration") {
        const CPPGMAstNodePtr special_declarator = ChildOfKind(child, "declarator");
        const CPPGMAstNodePtr special_initializer = ChildOfKind(
          special_declarator, "special-initializer");
        const bool defaulted = special_initializer && special_initializer->value == "default";
        CollectSpecialMember(child, class_scope,
          child->kind == "special-member-definition" || defaulted);
        continue;
      }
      if(child->kind != "simple-declaration") continue;
      if(child->children.empty()) continue;
      Analyzer::SpecFacts facts;
      TypePtr base = analyzer_.TypeFromSpecSeq(child->children[0], class_scope, &facts);
      if(facts.is_friend) continue;
      CPPGMAstNodePtr list = ChildOfKind(child, "init-declarator-list");
      if(!list) continue;
      for(size_t j = 0; j < list->children.size(); ++j) {
        const CPPGMAstNodePtr item = list->children[j];
        if(!item || item->children.empty()) continue;
        const CPPGMAstNodePtr declarator = item->children[0];
        TypePtr member_type = analyzer_.BuildDeclarator(declarator, base, class_scope);
        if(!function_type(member_type)) {
          const string name = declarator_name(declarator);
          if(!facts.is_static || name.empty()) continue;
          GlobalRecord record;
          record.node = child;
          record.scope = class_scope;
          record.type = member_type;
          record.qualified_name = TypeQualifiedName(type_found->second) + "::" + name;
          record.initializer = item->children.size() > 1 ? item->children[1] :
            CPPGMAstNodePtr();
          record.declaration = true;
          record.internal = false;
          record.thread_local_storage = HasStorageSpecifier(child, "thread_local");
          const string key = global_key(record.qualified_name);
          map<string, GlobalRecord*>::iterator global_found = global_by_key_.find(key);
          if(global_found == global_by_key_.end()) {
            globals_.push_back(record);
            global_by_key_[key] = &globals_.back();
          } else {
            GlobalRecord* prior = global_found->second;
            prior->type = record.type;
            prior->thread_local_storage = prior->thread_local_storage ||
              record.thread_local_storage;
            if(record.initializer) prior->initializer = record.initializer;
          }
          continue;
        }
        CPPGMAstNodePtr wrapper(new CPPGMAstNode("function-declaration"));
        wrapper->children.push_back(child->children[0]);
        wrapper->children.push_back(declarator);
        CollectFunction(wrapper, class_scope, false);
      }
    }
    CollectImplicitConstructor(type_found->second, class_scope);
    CollectImplicitDestructor(type_found->second, class_scope);
    (void)scope;
  }

void PA14Lowerer::CollectStringLiterals(const CPPGMAstNodePtr& node,
                                        unsigned int braced_depth)
{
    if(!node) return;
    if(node->kind == "literal" && braced_depth != 1 && !node->value.empty() &&
       node->value[0] == '"')
      InternString(node->value);
    const unsigned int child_depth = braced_depth +
      (node->kind == "braced-init-list" ? 1U : 0U);
    for(size_t i = 0; i < node->children.size(); ++i)
      CollectStringLiterals(node->children[i], child_depth);
  }

void PA14Lowerer::CollectFunction(const CPPGMAstNodePtr& node, Scope* scope, bool definition)
{
    if(!node || node->children.size() < 2) throw logic_error("invalid function declaration");
    Analyzer::SpecFacts facts;
    TypePtr base = analyzer_.TypeFromSpecSeq(node->children[0], scope, &facts);
    const string raw_name = declarator_name(node->children[1]);
    if(raw_name.empty()) throw logic_error("function has no name");
    TypePtr member_owner;
    if(scope && scope->kind == SCOPE_CLASS) member_owner = scope->owner_type;
    if(!member_owner && raw_name.find("::") != string::npos) {
      const size_t separator = raw_name.rfind("::");
      Analyzer::PathTarget owner = analyzer_.ResolvePath(scope, raw_name.substr(0, separator));
      if(owner.binding) member_owner = owner.binding->type;
      else if(owner.scope) member_owner = owner.scope->owner_type;
    }
    if(member_owner && member_owner->kind != TYPE_CLASS) member_owner.reset();
    Scope* type_scope = member_owner && member_owner->owned_scope ?
      member_owner->owned_scope : scope;
    TypePtr type = analyzer_.BuildDeclarator(node->children[1], base, type_scope);
    type = PA12AdjustedType(type);
    TypePtr function = function_type(type);
    if(!function) throw logic_error("LowIR function declaration has no function type");
    CPPGMAstNodePtr trailing = ChildOfKind(node->children[1], "trailing-return-type");
    if(trailing && !trailing->children.empty()) {
      TypePtr trailing_type = analyzer_.TypeFromTypeId(trailing->children[0], type_scope);
      TypePtr adjusted(new Type(*function));
      adjusted->child = trailing_type;
      function = adjusted;
    }
    const bool is_member = static_cast<bool>(member_owner);
    const bool is_static = facts.is_static;
    if(is_member && member_owner->owned_scope) {
      const string member_name = LastComponent(raw_name);
      vector<Binding*> bindings = DirectBindings(member_owner->owned_scope, member_name);
      for(size_t i = 0; i < bindings.size(); ++i) {
        TypePtr existing = function_target_type(bindings[i]->type);
        if(bindings[i]->kind != BIND_FUNCTION || !bindings[i]->is_member || !existing ||
           existing->variadic != function->variadic ||
           existing->function_const != function->function_const ||
           existing->function_volatile != function->function_volatile ||
           existing->parameters.size() != function->parameters.size()) continue;
        bool same_signature = true;
        for(size_t parameter = 0; parameter < function->parameters.size(); ++parameter)
          if(!PA12SameType(existing->parameters[parameter], function->parameters[parameter], false)) {
            same_signature = false;
            break;
          }
        if(same_signature) bindings[i]->type = function;
      }
    }
    string qname;
    if(is_member && raw_name.find("::") == string::npos)
      qname = TypeQualifiedName(member_owner) + "::" + raw_name;
    else qname = qualified_name(scope, raw_name);
    const string key = function_key(qname, function);
    map<string, FunctionRecord*>::const_iterator found = function_by_key_.find(key);
    FunctionRecord* record = 0;
    if(found == function_by_key_.end()) {
      functions_.push_back(FunctionRecord());
      record = &functions_.back();
      function_by_key_[key] = record;
    } else record = found->second;
    record->scope = scope;
    record->source_type = function;
    record->member_owner = member_owner;
    record->member = is_member;
    record->static_member = is_static;
    if(is_member && !is_static) {
      TypePtr this_type = CloneWithCv(member_owner, function->function_const,
        function->function_volatile);
      vector<TypePtr> parameters;
      parameters.push_back(PointerTo(this_type));
      parameters.insert(parameters.end(), function->parameters.begin(), function->parameters.end());
      record->type = FunctionOf(parameters, function->variadic, function->child, false);
    } else record->type = function;
    record->qualified_name = qname;
    record->definition = record->definition || definition;
    if(definition) record->node = node;
    record->variadic = function->variadic;
    if(definition) record->unwind_no = record->unwind_no || HasNoexcept(node->children[1]);
    RememberDefaults(record, node->children[1]);
  }

void PA14Lowerer::CollectSpecialMember(const CPPGMAstNodePtr& node, Scope* scope,
                                       bool definition)
{
    if(!node || !scope || scope->kind != SCOPE_CLASS) return;
    CPPGMAstNodePtr declarator = ChildOfKind(node, "declarator");
    if(!declarator) throw logic_error("special member has no declarator");
    Analyzer::SpecFacts facts;
    TypePtr function = analyzer_.BuildDeclarator(declarator, Fundamental("void"), scope);
    function = PA12AdjustedType(function);
    function = function_type(function);
    if(!function) throw logic_error("special member has no function type");
    TypePtr owner = scope->owner_type;
    if(!owner || owner->kind != TYPE_CLASS) throw logic_error("special member has no class owner");
    const string raw_name = declarator_name(declarator);
    const string name = LastComponent(raw_name);
    const string qname = TypeQualifiedName(owner) + "::" + name;
    const string key = function_key(qname, function);
    vector<Binding*> existing_bindings = DirectBindings(scope, name);
    bool has_binding = false;
    for(size_t i = 0; i < existing_bindings.size(); ++i)
      if(existing_bindings[i]->kind == BIND_FUNCTION &&
         function_target_type(existing_bindings[i]->type) &&
         PA12SameType(function_target_type(existing_bindings[i]->type), function, false)) {
        has_binding = true;
        break;
      }
    if(!has_binding) {
      Binding binding(BIND_FUNCTION, name, function);
      binding.is_member = true;
      binding.is_static = false;
      binding.member_owner = owner;
      binding.declaration = node;
      scope->add(binding);
    }
    map<string, FunctionRecord*>::const_iterator found = function_by_key_.find(key);
    FunctionRecord* record = 0;
    if(found == function_by_key_.end()) {
      functions_.push_back(FunctionRecord());
      record = &functions_.back();
      function_by_key_[key] = record;
    } else record = found->second;
    vector<TypePtr> parameters;
    parameters.push_back(PointerTo(owner));
    parameters.insert(parameters.end(), function->parameters.begin(), function->parameters.end());
    record->scope = scope;
    record->source_type = function;
    record->type = FunctionOf(parameters, function->variadic, function->child, false);
    record->member_owner = owner;
    record->qualified_name = qname;
    record->member = true;
    record->static_member = false;
    record->constructor = name == LastComponent(owner->name);
    record->destructor = name.size() > 1 && name[0] == '~';
    CPPGMAstNodePtr member_specs = ChildOfKind(node, "member-specifiers");
    if(member_specs) for(size_t i = 0; i < member_specs->children.size(); ++i)
      if(member_specs->children[i] && member_specs->children[i]->value == "explicit")
        record->explicit_constructor = true;
    record->special_initializer = ChildOfKind(node, "ctor-initializer");
    if(!record->special_initializer)
      record->special_initializer = ChildOfKind(declarator, "special-initializer");
    record->defaulted = record->special_initializer &&
      record->special_initializer->value == "default";
    record->deleted = record->special_initializer &&
      record->special_initializer->value == "delete";
    record->definition = record->definition || definition;
    if(definition) record->node = node;
    record->variadic = function->variadic;
    if(definition) record->unwind_no = record->unwind_no || HasNoexcept(declarator);
    RememberDefaults(record, declarator);
    (void)facts;
  }

void PA14Lowerer::CollectImplicitConstructor(const TypePtr& owner, Scope* scope)
{
    if(!owner || !scope || owner->kind != TYPE_CLASS) return;
    const string name = LastComponent(owner->name);
    vector<Binding*> constructors = DirectBindings(scope, name);
    bool has_constructor = false;
    for(size_t i = 0; i < constructors.size(); ++i)
      if(constructors[i]->kind == BIND_FUNCTION) { has_constructor = true; break; }
    if(has_constructor) return;
    bool needed = false;
    if(owner->direct_base) {
      TypePtr base = type_value(owner->direct_base);
      vector<Binding*> base_constructors = MemberBindings(base, LastComponent(base->name));
      for(size_t i = 0; i < base_constructors.size(); ++i)
        if(base_constructors[i]->kind == BIND_FUNCTION) { needed = true; break; }
    }
    for(size_t i = 0; i < owner->class_members.size(); ++i) {
      const ClassMemberInfo& member = owner->class_members[i];
      if(member.is_static || !member.type) continue;
      if(member.initializer) { needed = true; break; }
      TypePtr member_type = type_value(member.type);
      if(member_type && member_type->kind == TYPE_CLASS) {
        vector<Binding*> member_constructors = MemberBindings(member_type,
          LastComponent(member_type->name));
        for(size_t j = 0; j < member_constructors.size(); ++j)
          if(member_constructors[j]->kind == BIND_FUNCTION) { needed = true; break; }
      }
      if(needed) break;
    }
    if(!needed) return;
    CPPGMAstNodePtr special(new CPPGMAstNode("special-member-definition", name));
    CPPGMAstNodePtr declarator(new CPPGMAstNode("declarator"));
    declarator->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", name)));
    declarator->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("parameter-clause")));
    special->children.push_back(declarator);
    special->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("compound-statement")));
    TypePtr source = FunctionOf(vector<TypePtr>(), false, Fundamental("void"), false);
    const string qname = TypeQualifiedName(owner) + "::" + name;
    const string key = function_key(qname, source);
    map<string, FunctionRecord*>::const_iterator found = function_by_key_.find(key);
    if(found != function_by_key_.end()) return;
    Binding binding(BIND_FUNCTION, name, source);
    binding.is_member = true;
    binding.is_static = false;
    binding.member_owner = owner;
    binding.declaration = special;
    Binding* stored = scope->add(binding);
    (void)stored;
    functions_.push_back(FunctionRecord());
    FunctionRecord* record = &functions_.back();
    function_by_key_[key] = record;
    record->node = special;
    record->scope = scope;
    record->source_type = source;
    vector<TypePtr> parameters;
    parameters.push_back(PointerTo(owner));
    record->type = FunctionOf(parameters, false, Fundamental("void"), false);
    record->member_owner = owner;
    record->qualified_name = qname;
    record->member = true;
    record->static_member = false;
    record->constructor = true;
    record->implicit_constructor = true;
    record->definition = true;
  }

void PA14Lowerer::CollectImplicitDestructor(const TypePtr& owner, Scope* scope)
{
    if(!owner || !scope || owner->kind != TYPE_CLASS) return;
    const string name = "~" + LastComponent(owner->name);
    vector<Binding*> destructors = DirectBindings(scope, name);
    bool has_destructor = false;
    for(size_t i = 0; i < destructors.size(); ++i)
      if(destructors[i]->kind == BIND_FUNCTION) { has_destructor = true; break; }
    if(has_destructor) return;
    bool needed = false;
    TypePtr base = type_value(owner->direct_base);
    if(base) {
      vector<Binding*> base_destructors = MemberBindings(base, "~" + LastComponent(base->name));
      for(size_t i = 0; i < base_destructors.size(); ++i)
        if(base_destructors[i]->kind == BIND_FUNCTION) { needed = true; break; }
    }
    for(size_t i = 0; i < owner->class_members.size() && !needed; ++i) {
      const ClassMemberInfo& member = owner->class_members[i];
      TypePtr member_type = type_value(member.type);
      if(member.is_static || !member_type || member_type->kind != TYPE_CLASS) continue;
      vector<Binding*> member_destructors = MemberBindings(member_type,
        "~" + LastComponent(member_type->name));
      for(size_t j = 0; j < member_destructors.size(); ++j)
        if(member_destructors[j]->kind == BIND_FUNCTION) { needed = true; break; }
    }
    if(!needed) return;
    CPPGMAstNodePtr special(new CPPGMAstNode("special-member-definition", name));
    CPPGMAstNodePtr declarator(new CPPGMAstNode("declarator"));
    declarator->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", name)));
    declarator->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("parameter-clause")));
    special->children.push_back(declarator);
    special->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("compound-statement")));
    TypePtr source = FunctionOf(vector<TypePtr>(), false, Fundamental("void"), false);
    const string qname = TypeQualifiedName(owner) + "::" + name;
    const string key = function_key(qname, source);
    if(function_by_key_.find(key) != function_by_key_.end()) return;
    Binding binding(BIND_FUNCTION, name, source);
    binding.is_member = true;
    binding.is_static = false;
    binding.member_owner = owner;
    binding.declaration = special;
    scope->add(binding);
    functions_.push_back(FunctionRecord());
    FunctionRecord* record = &functions_.back();
    function_by_key_[key] = record;
    record->node = special;
    record->scope = scope;
    record->source_type = source;
    vector<TypePtr> parameters(1, PointerTo(owner));
    record->type = FunctionOf(parameters, false, Fundamental("void"), false);
    record->member_owner = owner;
    record->qualified_name = qname;
    record->member = true;
    record->destructor = true;
    record->definition = true;
  }

void PA14Lowerer::RememberDefaults(FunctionRecord* record, const CPPGMAstNodePtr& declarator)
{
    CPPGMAstNodePtr clause = ChildOfKind(declarator, "parameter-clause");
    if(!clause) return;
    if(record->default_arguments.size() < clause->children.size())
      record->default_arguments.resize(clause->children.size());
    size_t parameter = 0;
    for(size_t i = 0; i < clause->children.size(); ++i) {
      CPPGMAstNodePtr item = clause->children[i];
      if(!item || item->kind != "parameter-declaration") continue;
      for(size_t j = 1; j < item->children.size(); ++j) {
        CPPGMAstNodePtr default_node = item->children[j];
        if(!default_node || default_node->kind != "default-argument") continue;
        if(!default_node->children.empty()) default_node = default_node->children[0];
        record->default_arguments[parameter] = default_node;
        break;
      }
      ++parameter;
    }
  }

void PA14Lowerer::CollectSimpleDeclaration(const CPPGMAstNodePtr& node, Scope* scope)
{
    if(!node || node->children.empty()) return;
    CheckTypeAccess(node->children[0], scope);
    Analyzer::SpecFacts facts;
    analyzer_.TypeFromSpecSeq(node->children[0], scope, &facts);
    CPPGMAstNodePtr list = ChildOfKind(node, "init-declarator-list");
    if(!list) return;
    for(size_t i = 0; i < list->children.size(); ++i) {
      CPPGMAstNodePtr item = list->children[i];
      if(!item || item->children.empty()) continue;
      CPPGMAstNodePtr declarator = item->children[0];
      CPPGMAstNodePtr initializer = item->children.size() > 1 ? item->children[1] :
        CPPGMAstNodePtr();
      TypePtr type = PlannedType(node->children[0], declarator, scope, initializer);
      if(facts.is_constexpr && type->kind != TYPE_FUNCTION)
        type = CloneWithCv(type, true, false);
      if(facts.is_typedef) continue;
      const string name = declarator_name(declarator);
      if(name.empty()) continue;
      if(type->kind == TYPE_FUNCTION) {
        if(initializer && !initializer->children.empty() && initializer->children[0] &&
           initializer->children[0]->kind == "special-initializer" &&
           initializer->children[0]->value == "delete") continue;
        // A declaration and a definition are merged by the same semantic
        // signature.  This keeps forward declarations available to calls.
        CPPGMAstNodePtr wrapper(new CPPGMAstNode("function-declaration"));
        wrapper->children.push_back(node->children[0]);
        wrapper->children.push_back(declarator);
        CollectFunction(wrapper, scope, false);
        continue;
      }
      const bool is_extern = HasStorageSpecifier(node, "extern");
      if(is_extern && item->children.size() < 2) continue;
      GlobalRecord record;
      record.node = node;
      record.scope = scope;
      record.type = type;
      record.qualified_name = qualified_name(scope, name);
      record.initializer = initializer;
      record.declaration = false;
      record.internal = facts.is_const || facts.is_constexpr || HasStorageSpecifier(node, "static");
      record.thread_local_storage = HasStorageSpecifier(node, "thread_local");
      record.dynamic_initializer = false;
      record.dynamic_finalizer = false;
      TypePtr value_type = type_value(type);
      const bool object = value_type &&
        (value_type->kind == TYPE_CLASS ||
         (value_type->kind == TYPE_ARRAY && value_type->child &&
          type_value(value_type->child) && type_value(value_type->child)->kind == TYPE_CLASS));
      if(!is_extern && object) {
        // Every namespace-scope class object has a construction phase, even
        // when its implicit constructor has no executable body.  Keeping the
        // phase explicit also gives later lifetime lowering a stable place to
        // attach member initialization actions.
        record.dynamic_initializer = true;
        record.dynamic_finalizer = HasDestructor(value_type);
        if(record.dynamic_initializer) needs_init_helper_ = true;
        if(record.dynamic_finalizer) needs_fini_helper_ = true;
      }
      const string key = global_key(record.qualified_name);
      map<string, GlobalRecord*>::iterator found = global_by_key_.find(key);
      if(found == global_by_key_.end()) {
        globals_.push_back(record);
        global_by_key_[key] = &globals_.back();
      } else {
        GlobalRecord* prior = found->second;
        if(record.initializer) prior->initializer = record.initializer;
        prior->type = record.type;
        prior->declaration = false;
        prior->internal = prior->internal || record.internal;
        prior->thread_local_storage = prior->thread_local_storage || record.thread_local_storage;
        prior->dynamic_initializer = prior->dynamic_initializer || record.dynamic_initializer;
        prior->dynamic_finalizer = prior->dynamic_finalizer || record.dynamic_finalizer;
      }
    }
  }

bool PA14Lowerer::HasStorageSpecifier(const CPPGMAstNodePtr& node, const string& word) const
{
    if(!node) return false;
    CPPGMAstNodePtr seq = node->children.empty() ? CPPGMAstNodePtr() : node->children[0];
    if(!seq) return false;
    for(size_t i = 0; i < seq->children.size(); ++i)
      if(seq->children[i] && seq->children[i]->value.find(":" + word) != string::npos)
        return true;
    return false;
  }

void PA14Lowerer::FinalizeSymbols()
{
    map<string, unsigned int> overloads;
    for(size_t i = 0; i < functions_.size(); ++i) {
      FunctionRecord& function = functions_[i];
      string base = low_symbol_component(function.qualified_name);
      unsigned int& count = overloads[base];
      ++count;
      function.symbol = count == 1 ? base : base + "__ov" + integer_text(count);
    }
    for(size_t i = 0; i < globals_.size(); ++i) {
      globals_[i].symbol = low_symbol_component(globals_[i].qualified_name);
    }
  }

PA14Lowerer::FunctionRecord* PA14Lowerer::FindFunction(const string& qname, const TypePtr& type) const
{
    const string key = function_key(qname, function_type(type));
    map<string, FunctionRecord*>::const_iterator found = function_by_key_.find(key);
    return found == function_by_key_.end() ? 0 : found->second;
  }

PA14Lowerer::GlobalRecord* PA14Lowerer::FindGlobal(const string& qname) const
{
    map<string, GlobalRecord*>::const_iterator found = global_by_key_.find(qname);
    return found == global_by_key_.end() ? 0 : found->second;
  }

TypePtr PA14Lowerer::IntegralPromotion(const TypePtr& raw) const
{
    TypePtr type = type_value(raw);
    if(type && type->kind == TYPE_ENUM && !type->scoped_enum) return Fundamental("int");
    if(!type || type->kind != TYPE_FUNDAMENTAL) return type;
    if(type->name == "bool" || type->name == "char" || type->name == "signed char" ||
       type->name == "unsigned char" || type->name == "short int" ||
       type->name == "unsigned short int") return Fundamental("int");
    return type;
  }

bool PA14Lowerer::PointerCompatible(const TypePtr& source, const TypePtr& target) const
{
    if(!source || !target || source->kind != TYPE_POINTER || target->kind != TYPE_POINTER)
      return false;
    return PA12SameType(source->child, target->child, true) ||
      (target->child && target->child->kind == TYPE_FUNDAMENTAL &&
      target->child->name == "void");
  }

bool PA14Lowerer::IsDerivedFrom(const TypePtr& raw_derived, const TypePtr& raw_base) const
{
    return BaseDistance(raw_derived, raw_base) >= 1;
  }

int PA14Lowerer::BaseDistance(const TypePtr& raw_derived, const TypePtr& raw_base) const
{
    TypePtr derived = type_value(raw_derived);
    TypePtr base = type_value(raw_base);
    if(!derived || !base || derived->kind != TYPE_CLASS || base->kind != TYPE_CLASS)
      return -1;
    int distance = 1;
    for(TypePtr current = type_value(derived->direct_base); current;
        current = type_value(current->direct_base), ++distance)
      if(PA12SameType(current, base, true)) return distance;
    return -1;
  }

int PA14Lowerer::ConversionRank(const ExprInfo& source, const TypePtr& target) const
{
    if(!target || !source.type) return -1;
    TypePtr source_value = type_value(source.type);
    TypePtr target_value = type_value(target);
    if(!source_value || !target_value) return -1;
    if(target->kind == TYPE_LVALUE_REFERENCE || target->kind == TYPE_RVALUE_REFERENCE) {
      if(target->kind == TYPE_LVALUE_REFERENCE) {
        if(source.category == "lvalue") {
          if(!target_value->is_const && source_value->is_const) return -1;
          if(PA12SameType(source_value, target_value, true)) return
            PA12SameType(source_value, target_value, false) ? 0 : 1;
          if(IsDerivedFrom(source_value, target_value))
            return BaseDistance(source_value, target_value);
          if(is_arithmetic_type(source_value) && is_arithmetic_type(target_value) &&
             target_value->is_const) return 2;
          return -1;
        }
        if(target_value->is_const &&
           (PA12SameType(source_value, target_value, true) ||
            (is_arithmetic_type(source_value) && is_arithmetic_type(target_value)) ||
            (source_value->kind == TYPE_POINTER && target_value->kind == TYPE_POINTER &&
             IsDerivedFrom(source_value->child, target_value->child)))) return 2;
        return -1;
      }
      if(source.category == "lvalue") return -1;
      return PA12SameType(source_value, target_value, true) ||
        (is_arithmetic_type(source_value) && is_arithmetic_type(target_value)) ? 1 : -1;
    }
    if(target_value->kind == TYPE_POINTER) {
      if(source.null_pointer_constant ||
         (source_value->kind == TYPE_FUNDAMENTAL && source_value->name == "nullptr_t")) return 2;
      if(source_value->kind == TYPE_ARRAY &&
         PA12SameType(source_value->child, target_value->child, true)) return 0;
      if(source_value->kind == TYPE_FUNCTION && target_value->child &&
         target_value->child->kind == TYPE_FUNCTION &&
         PA12SameType(source_value, target_value->child, true)) return 0;
      if(source_value->kind == TYPE_POINTER) {
        if(PA12SameType(source_value, target_value, false)) return 0;
        if(source_value->child && target_value->child &&
           source_value->child->kind == TYPE_POINTER &&
           target_value->child->kind == TYPE_POINTER &&
           !PA12SameType(source_value->child, target_value->child, false) &&
           PA12SameType(source_value->child, target_value->child, true)) return -1;
        if(PA12SameType(source_value, target_value, true)) return 1;
        if(source_value->child && target_value->child &&
           IsDerivedFrom(source_value->child, target_value->child))
          return BaseDistance(source_value->child, target_value->child);
        if(target_value->child && target_value->child->kind == TYPE_FUNDAMENTAL &&
           target_value->child->name == "void") return 2;
      }
      return -1;
    }
    if(target_value->kind == TYPE_FUNDAMENTAL && target_value->name == "nullptr_t")
      return source.null_pointer_constant || source_value->name == "nullptr_t" ? 1 : -1;
    if(target_value->kind == TYPE_FUNDAMENTAL && target_value->name == "bool" &&
       source_value->kind == TYPE_POINTER) return 3;
    if(PA12SameType(source_value, target_value, false)) return 0;
    if(PA12SameType(source_value, target_value, true)) return 1;
    if(is_arithmetic_type(source_value) && is_arithmetic_type(target_value)) {
      if(source_value->kind == TYPE_ENUM && !source_value->scoped_enum &&
         target_value->kind == TYPE_FUNDAMENTAL && target_value->name == "int") return 1;
      return 2;
    }
    if(source_value->kind == TYPE_FUNCTION && target_value->kind == TYPE_FUNCTION &&
       PA12SameType(source_value, target_value, true)) return 0;
    return -1;
  }

bool PA14Lowerer::DirectFunctionName(const CPPGMAstNodePtr& callee, Scope* scope) const
{
    if(!callee || callee->kind != "id-expression") return false;
    const vector<Binding*> candidates = Lookup(callee->value, scope);
    for(size_t i = 0; i < candidates.size(); ++i)
      if(candidates[i]->kind == BIND_FUNCTION && function_target_type(candidates[i]->type)) return true;
    return false;
  }

PA14Lowerer::FunctionRecord* PA14Lowerer::RecordForBinding(Binding* binding) const
{
    if(!binding) return 0;
    const string key = function_key(binding->qualified_name,
      function_target_type(binding->type));
    map<string, FunctionRecord*>::const_iterator found = function_by_key_.find(key);
    return found == function_by_key_.end() ? 0 : found->second;
  }

PA14Lowerer::FunctionRecord* PA14Lowerer::EnsureAggregateConstructor(const TypePtr& raw_type)
{
    TypePtr owner = type_value(raw_type);
    if(!owner || owner->kind != TYPE_CLASS || !owner->owned_scope) return 0;
    vector<Binding*> existing_constructors = MemberBindings(owner, LastComponent(owner->name));
    for(size_t i = 0; i < existing_constructors.size(); ++i) {
      FunctionRecord* existing = RecordForBinding(existing_constructors[i]);
      if(existing && existing->constructor && !existing->implicit_constructor &&
         !existing->defaulted && !existing->deleted) return 0;
    }
    vector<TypePtr> member_parameters;
    vector<string> member_names;
    for(size_t i = 0; i < owner->class_members.size(); ++i) {
      const ClassMemberInfo& member = owner->class_members[i];
      if(member.is_static || member.name.empty() || !member.type) continue;
      if(member.initializer) return 0;
      member_parameters.push_back(member.type);
      member_names.push_back(member.name);
    }
    if(member_parameters.empty()) return 0;
    const string name = LastComponent(owner->name);
    const string qname = TypeQualifiedName(owner) + "::" + name;
    TypePtr source = FunctionOf(member_parameters, false, Fundamental("void"), false);
    const string key = function_key(qname, source);
    map<string, FunctionRecord*>::const_iterator found = function_by_key_.find(key);
    if(found != function_by_key_.end()) return found->second;

    CPPGMAstNodePtr special(new CPPGMAstNode("special-member-definition", name));
    CPPGMAstNodePtr declarator(new CPPGMAstNode("declarator"));
    declarator->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", name)));
    CPPGMAstNodePtr clause(new CPPGMAstNode("parameter-clause"));
    for(size_t i = 0; i < member_names.size(); ++i) {
      CPPGMAstNodePtr parameter(new CPPGMAstNode("parameter-declaration"));
      parameter->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("type-specifier")));
      CPPGMAstNodePtr parameter_declarator(new CPPGMAstNode("declarator"));
      parameter_declarator->children.push_back(CPPGMAstNodePtr(
        new CPPGMAstNode("identifier", member_names[i])));
      parameter->children.push_back(parameter_declarator);
      clause->children.push_back(parameter);
    }
    declarator->children.push_back(clause);
    special->children.push_back(declarator);
    special->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("compound-statement")));

    Binding binding(BIND_FUNCTION, name, source);
    binding.qualified_name = qname;
    binding.is_member = true;
    binding.is_static = false;
    binding.member_owner = owner;
    binding.access = "public";
    binding.declaration = special;
    owner->owned_scope->add(binding);

    functions_.push_back(FunctionRecord());
    FunctionRecord* record = &functions_.back();
    function_by_key_[key] = record;
    record->node = special;
    record->scope = owner->owned_scope;
    record->source_type = source;
    vector<TypePtr> parameters;
    parameters.push_back(PointerTo(owner));
    parameters.insert(parameters.end(), member_parameters.begin(), member_parameters.end());
    record->type = FunctionOf(parameters, false, Fundamental("void"), false);
    record->member_owner = owner;
    record->qualified_name = qname;
    record->member = true;
    record->static_member = false;
    record->constructor = true;
    record->aggregate_constructor = true;
    record->definition = true;
    const string base = low_symbol_component(qname);
    record->symbol = base + "__ov2";
    unsigned int suffix = 2;
    while(true) {
      bool collision = false;
      for(size_t i = 0; i + 1 < functions_.size(); ++i)
        if(functions_[i].symbol == record->symbol) { collision = true; break; }
      if(!collision) break;
      record->symbol = base + "__ov" + integer_text(static_cast<long long>(++suffix));
    }
    return record;
  }

bool PA14Lowerer::HasDefaultArgument(Binding* binding, size_t index) const
{
    FunctionRecord* record = RecordForBinding(binding);
    return record && index < record->default_arguments.size() &&
      static_cast<bool>(record->default_arguments[index]);
  }

bool PA14Lowerer::HasConstructor(const TypePtr& raw_type) const
{
    TypePtr type = type_value(raw_type);
    if(type && type->kind == TYPE_ARRAY) return HasConstructor(type->child);
    if(!type || type->kind != TYPE_CLASS) return false;
    const vector<Binding*> candidates = MemberBindings(type, LastComponent(type->name));
    for(size_t i = 0; i < candidates.size(); ++i) {
      Binding* binding = candidates[i];
      FunctionRecord* record = RecordForBinding(binding);
      if(binding->kind == BIND_FUNCTION && binding->is_member && !binding->is_static &&
         record && record->constructor) return true;
    }
    return false;
  }

bool PA14Lowerer::HasDestructor(const TypePtr& raw_type) const
{
    TypePtr type = type_value(raw_type);
    if(type && type->kind == TYPE_ARRAY) return HasDestructor(type->child);
    if(!type || type->kind != TYPE_CLASS) return false;
    const vector<Binding*> candidates = MemberBindings(type, "~" + LastComponent(type->name));
    for(size_t i = 0; i < candidates.size(); ++i) {
      Binding* binding = candidates[i];
      FunctionRecord* record = RecordForBinding(binding);
      if(binding->kind == BIND_FUNCTION && binding->is_member && !binding->is_static &&
         record && record->destructor) return true;
    }
    return false;
  }

bool PA14Lowerer::IsBitField(Binding* binding, long long* bit_offset,
                             long long* bit_width) const
{
    if(!binding || !binding->member_owner || binding->member_index == static_cast<size_t>(-1) ||
       binding->member_index >= binding->member_owner->class_members.size()) return false;
    const ClassMemberInfo& member = binding->member_owner->class_members[binding->member_index];
    if(!member.bit_field || member.bit_width <= 0) return false;
    if(bit_offset) *bit_offset = member.bit_offset;
    if(bit_width) *bit_width = member.bit_width;
    return true;
  }

} // namespace cppgm_pa14_lowering

void EmitPA14LowIR(const vector<CPPGMAstNodePtr>& translation_units,
                   ostream& out)
{
  cppgm_pa14_lowering::PA14Lowerer lowerer(translation_units);
  lowerer.Lower(out);
}
