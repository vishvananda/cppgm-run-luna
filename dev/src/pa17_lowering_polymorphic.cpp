#include "pa14_lowering.h"

#include <algorithm>
#include <set>
#include <sstream>

using namespace std;

namespace cppgm_pa14_lowering {

namespace {

bool IsDestructorSlot(const VirtualMethodInfo& slot)
{
  return slot.destructor || (slot.name.size() > 1 && slot.name[0] == '~');
}

string TypeInfoPrefix(const TypePtr& type)
{
  return type && !type->tag.empty() ? type->tag : "class";
}

string TypeInfoSymbol(const TypePtr& type)
{
  return "__typeinfo_name__" + TypeInfoPrefix(type) + "_";
}

string RttiSymbol(const TypePtr& type)
{
  return "__rtti_" + TypeInfoPrefix(type) + "_";
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

} // namespace

string PA14Lowerer::TypeMangledName(const TypePtr& type) const
{
  const string name = TypeQualifiedName(type);
  if(type && type->template_specialization && !type->template_primary.empty())
    return template_type_mangled_name(type);
  if (name == "std::ios_base") return "St8ios_base";
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
  if (components.empty()) return "1X";
  if (components.size() == 1)
    return integer_text(static_cast<long long>(components[0].size())) + components[0];
  string result = "N";
  for (size_t i = 0; i < components.size(); ++i)
    result += integer_text(static_cast<long long>(components[i].size())) + components[i];
  return result + "E";
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
    existing.needed = true;
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
  record->needed = true;
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
  complete->needed = true;
  if (deleting) {
    const string qname = complete->qualified_name + "__deleting_entry";
    const string key = function_key(qname, complete->source_type);
    map<string, FunctionRecord*>::const_iterator found = function_by_key_.find(key);
    if (found != function_by_key_.end()) {
      found->second->needed = true;
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
    entry->needed = true;
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
      entry->needed = true;
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
  if (record) record->needed = true;
  return record;
}

void PA14Lowerer::PreparePolymorphicModel()
{
  emitted_vtables_.clear();
  external_vtables_.clear();
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
      if (IsVirtualFunction(function)) { root = true; break; }
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
          record->needed = true;
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
        if (record) record->needed = true;
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
  for (size_t i = 0; i < functions_.size(); ++i)
    if (functions_[i].member && functions_[i].definition && functions_[i].node &&
        ContainsVirtualMemberCall(functions_[i].node, functions_[i]))
      functions_[i].needed = true;
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
  if (emitted_vtables_.empty() && external_vtables_.empty()) return;
  entries.push_back("declare global @__external_rtti_vtable____class_type_info [binding=strong, object=_ZTVN10__cxxabiv117__class_type_infoE]");
  bool has_si = false;
  set<const Type*> rtti_types;
  vector<const Type*> work;
  const vector<const Type*> emitted_types = OrderedTypes(emitted_vtables_);
  const vector<const Type*> external_types = OrderedTypes(external_vtables_);
  work.insert(work.end(), emitted_types.begin(), emitted_types.end());
  work.insert(work.end(), external_types.begin(), external_types.end());
  while (!work.empty()) {
    const Type* type = work.back();
    work.pop_back();
    if (!rtti_types.insert(type).second) continue;
    if (type->direct_base) {
      has_si = true;
      work.push_back(type->direct_base.get());
    }
  }
  if (has_si)
    entries.push_back("declare global @__external_rtti_vtable____si_class_type_info [binding=strong, object=_ZTVN10__cxxabiv120__si_class_type_infoE]");
  for (size_t type_index = 0; type_index < external_types.size(); ++type_index) {
    const TypePtr type = SemanticType(external_types[type_index]);
    entries.push_back("declare global @__external_vtable__" +
      low_symbol_component(TypeQualifiedName(type)) + " [binding=strong, object=_ZTV" +
      TypeMangledName(type) + "]");
  }
  const vector<const Type*> ordered_rtti_types = OrderedTypes(rtti_types);
  for (size_t type_index = 0; type_index < ordered_rtti_types.size(); ++type_index) {
    const TypePtr type = SemanticType(ordered_rtti_types[type_index]);
    const string info_symbol = TypeInfoSymbol(type) + low_symbol_component(TypeQualifiedName(type));
    const string rtti_symbol = RttiSymbol(type) + low_symbol_component(TypeQualifiedName(type));
    const string mangled = TypeMangledName(type);
    ostringstream name;
    name << "global @" << info_symbol << " [storage=readonly, binding=weak, object=_ZTS" << mangled << "] = {\n";
    for (size_t i = 0; i < mangled.size(); ++i)
      name << "  i8 " << static_cast<unsigned int>(static_cast<unsigned char>(mangled[i])) << "\n";
    name << "  i8 0\n}";
    entries.push_back(name.str());
    ostringstream rtti;
    rtti << "global @" << rtti_symbol <<
      " [storage=readonly, binding=weak, object=_ZTI" << mangled << "] = {\n";
    if (type->direct_base && !ShouldUseExternalVtable(type))
      rtti << "  ptr addr @__external_rtti_vtable____si_class_type_info + 16\n";
    else
      rtti << "  ptr addr @__external_rtti_vtable____class_type_info + 16\n";
    rtti << "  ptr addr @" << info_symbol << "\n";
    if (type->direct_base && !ShouldUseExternalVtable(type)) {
      const TypePtr base = type->direct_base;
      rtti << "  ptr addr @" << RttiSymbol(base) <<
        low_symbol_component(TypeQualifiedName(base)) << "\n";
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
    table << "  i64 0\n  ptr addr @" << RttiSymbol(type) <<
      low_symbol_component(TypeQualifiedName(type)) << "\n";
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
        function->needed = true;
        table << "  ptr addr @" << function->symbol << "\n";
      }
    }
    table << "}";
    entries.push_back(table.str());
  }
}

} // namespace cppgm_pa14_lowering
