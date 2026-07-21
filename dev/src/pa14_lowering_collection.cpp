#include "pa14_lowering.h"

using namespace std;

namespace cppgm_pa14_lowering {

namespace {

string SpecialMemberName(const string& raw_name)
{
    const size_t operator_pos = raw_name.rfind("operator");
    if(operator_pos != string::npos) {
      string suffix = raw_name.substr(operator_pos + 8);
      while(!suffix.empty() && suffix[0] == ' ') suffix.erase(0, 1);
      return "operator" + suffix;
    }
    return LastComponent(raw_name);
  }

} // namespace

bool PA14Lowerer::HasInline(const CPPGMAstNodePtr& node) const
{
    if(!node) return false;
    if(node->value == "KW_INLINE:inline" || node->value == "inline") return true;
    for(size_t i = 0; i < node->children.size(); ++i)
      if(HasInline(node->children[i])) return true;
    return false;
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
    // Analyzer bindings are created from the `auto` spelling before PA14
    // resolves a trailing return type.  Keep the binding's typed function in
    // sync with the record key so direct calls can find the collected body.
    if(trailing && !trailing->children.empty()) {
      const string binding_name = LastComponent(raw_name);
      const vector<Binding*> bindings = DirectBindings(scope, binding_name);
      for(size_t i = 0; i < bindings.size(); ++i) {
        TypePtr existing = function_target_type(bindings[i]->type);
        if(bindings[i]->kind != BIND_FUNCTION || !existing ||
           existing->parameters.size() != function->parameters.size() ||
           existing->variadic != function->variadic ||
           existing->function_const != function->function_const ||
           existing->function_volatile != function->function_volatile) continue;
        bool same_parameters = true;
        for(size_t p = 0; p < function->parameters.size(); ++p)
          if(!PA12SameType(existing->parameters[p], function->parameters[p], false)) {
            same_parameters = false;
            break;
          }
        if(same_parameters) bindings[i]->type = function;
      }
    }
    // Some nested out-of-class declarators arrive from the parser with only
    // the enclosing namespace in their qualified spelling (for example,
    // object::table::allocate is represented as json::allocate).  The
    // return type still identifies the nested class, so recover the owner
    // from its existing member declaration before forming the function key.
    if(!member_owner && raw_name.find("::") != string::npos) {
      TypePtr result_owner = type_value(function->child);
      if(result_owner && result_owner->kind == TYPE_POINTER)
        result_owner = type_value(result_owner->child);
      if(result_owner && result_owner->kind == TYPE_CLASS && result_owner->owned_scope) {
        const string member_name = LastComponent(raw_name);
        const vector<Binding*> owner_bindings =
          DirectBindings(result_owner->owned_scope, member_name);
        for(size_t i = 0; i < owner_bindings.size(); ++i) {
          TypePtr existing = function_target_type(owner_bindings[i]->type);
          if(owner_bindings[i]->kind == BIND_FUNCTION && existing) {
            member_owner = result_owner;
            break;
          }
        }
      }
    }
    const bool hidden_friend = facts.is_friend && static_cast<bool>(member_owner);
    const bool is_member = static_cast<bool>(member_owner) && !hidden_friend;
    bool is_static = facts.is_static;
    if(is_member && member_owner->owned_scope) {
      const vector<Binding*> owner_bindings =
        DirectBindings(member_owner->owned_scope, LastComponent(raw_name));
      bool matched_owner = false;
      for(size_t i = 0; i < owner_bindings.size(); ++i) {
        TypePtr existing = function_target_type(owner_bindings[i]->type);
        if(owner_bindings[i]->kind != BIND_FUNCTION || !existing ||
           existing->parameters.size() != function->parameters.size() ||
           existing->variadic != function->variadic) continue;
        bool same_signature = true;
        for(size_t p = 0; p < function->parameters.size(); ++p)
          if(!PA12SameType(existing->parameters[p], function->parameters[p], false)) {
            same_signature = false;
            break;
          }
        if(same_signature) {
          is_static = owner_bindings[i]->is_static;
          matched_owner = true;
          break;
        }
      }
      if(!matched_owner && owner_bindings.size() == 1 &&
         owner_bindings[0]->kind == BIND_FUNCTION)
        is_static = owner_bindings[0]->is_static;
    }
    if(is_member && member_owner->owned_scope) {
      const string member_name = LastComponent(raw_name);
      vector<Binding*> bindings = DirectBindings(member_owner->owned_scope, member_name);
      for(size_t i = 0; i < bindings.size(); ++i) {
        TypePtr existing = function_target_type(bindings[i]->type);
        if(bindings[i]->kind != BIND_FUNCTION || !bindings[i]->is_member || !existing ||
           existing->variadic != function->variadic ||
           existing->function_const != function->function_const ||
           existing->function_volatile != function->function_volatile ||
           existing->function_lvalue_ref_qualified != function->function_lvalue_ref_qualified ||
           existing->function_rvalue_ref_qualified != function->function_rvalue_ref_qualified ||
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
    Scope* function_scope_owner = scope;
    if(hidden_friend) {
      function_scope_owner = member_owner->owned_scope;
      while(function_scope_owner && function_scope_owner->kind != SCOPE_NAMESPACE)
        function_scope_owner = function_scope_owner->parent;
      if(!function_scope_owner) function_scope_owner = scope;
    }
    string qname;
    if(hidden_friend && raw_name.find("::") == string::npos)
      qname = qualified_name(function_scope_owner, raw_name);
    else if(is_member)
      qname = TypeQualifiedName(member_owner) + "::" + LastComponent(raw_name);
    else qname = qualified_name(scope, raw_name);
    if(hidden_friend && member_owner->owned_scope) {
      vector<Binding*> hidden = DirectBindings(member_owner->owned_scope, LastComponent(raw_name));
      bool matched = false;
      for(size_t i = 0; i < hidden.size(); ++i) {
        TypePtr existing = function_target_type(hidden[i]->type);
        if(hidden[i]->kind != BIND_FUNCTION || !existing ||
           !PA12SameType(existing, function, false)) continue;
        hidden[i]->type = function;
        hidden[i]->qualified_name = qname;
        hidden[i]->hidden_friend = true;
        hidden[i]->friend_owner = member_owner;
        hidden[i]->is_member = false;
        hidden[i]->is_static = false;
        hidden[i]->member_owner.reset();
        hidden[i]->access.clear();
        matched = true;
      }
      if(!matched) {
        Binding binding(BIND_FUNCTION, LastComponent(raw_name), function);
        binding.qualified_name = qname;
        binding.hidden_friend = true;
        binding.friend_owner = member_owner;
        member_owner->owned_scope->add(binding);
      }
    }
    const string key = function_key(qname, function);
    map<string, FunctionRecord*>::const_iterator found = function_by_key_.find(key);
    FunctionRecord* record = 0;
    if(found == function_by_key_.end()) {
      functions_.push_back(FunctionRecord());
      record = &functions_.back();
      function_by_key_[key] = record;
    } else record = found->second;
    record->scope = hidden_friend ? function_scope_owner : scope;
    record->source_type = function;
    record->member_owner = member_owner;
    record->member = is_member;
    record->hidden_friend = hidden_friend;
    record->static_member = is_static;
	record->template_instantiation = node->template_instantiation ||
		(member_owner && member_owner->template_specialization);
	record->object_root = record->object_root || node->explicit_instantiation;
	if(node->explicit_instantiation) record->needed = true;
	record->weak_binding = record->template_instantiation;
	record->inline_definition = record->inline_definition || HasInline(node);
	if(node->template_instantiation) {
		record->template_primary = node->template_primary;
		record->template_arguments = node->template_arguments;
	} else if(member_owner && member_owner->template_specialization) {
		record->template_primary = member_owner->template_primary;
		record->template_arguments = member_owner->template_arguments;
	}
    if(is_member && !is_static) {
      TypePtr this_type = CloneWithCv(member_owner, function->function_const,
        function->function_volatile);
      vector<TypePtr> parameters;
      parameters.push_back(PointerTo(this_type));
      parameters.insert(parameters.end(), function->parameters.begin(), function->parameters.end());
      record->type = FunctionOf(parameters, function->variadic, function->child, false,
        false, function->function_lvalue_ref_qualified,
        function->function_rvalue_ref_qualified);
    } else record->type = function;
    record->qualified_name = qname;
    record->definition = record->definition || definition;
    if(definition) record->node = node;
    record->variadic = function->variadic;
    if(definition) record->unwind_no = record->unwind_no || HasNoexcept(node->children[1]);
    RememberDefaults(record, node->children[1]);
    CPPGMAstNodePtr special_initializer = ChildOfKind(node->children[1], "special-initializer");
    if(!special_initializer) {
      CPPGMAstNodePtr initializer = ChildOfKind(node, "initializer");
      special_initializer = ChildOfKind(initializer, "special-initializer");
    }
    if(special_initializer) {
      record->special_initializer = special_initializer;
      record->defaulted = special_initializer->value == "default";
      record->deleted = special_initializer->value == "delete";
    }
    ClassifySpecialMember(record);
  }

void PA14Lowerer::ClassifySpecialMember(FunctionRecord* record)
{
    if(!record || !record->member || record->static_member ||
       !record->member_owner || !record->source_type) return;
    TypePtr owner = type_value(record->member_owner);
    TypePtr function = function_target_type(record->source_type);
    if(!owner || owner->kind != TYPE_CLASS || !function) return;
    const string name = LastComponent(record->qualified_name);
    const bool constructor = record->constructor || name == LastComponent(owner->name);
    const bool assignment = name == "operator=";
    if(!constructor && !assignment) return;
    if(function->parameters.empty()) return;
    for(size_t i = 1; i < function->parameters.size(); ++i)
      if(i >= record->default_arguments.size() || !record->default_arguments[i]) return;
    TypePtr parameter = function->parameters[0];
    if(!type_is_reference(parameter) ||
       !PA12SameType(type_value(parameter), owner, true)) return;
    record->value_special_member = true;
    if(parameter->kind == TYPE_RVALUE_REFERENCE) {
      if(constructor) record->move_constructor = true;
      else record->move_assignment = true;
    } else if(constructor) {
      record->copy_constructor = true;
    } else {
      record->copy_assignment = true;
    }
    if(record->defaulted) MarkValueMemberDeleted(record);
  }

void PA14Lowerer::CollectSpecialMember(const CPPGMAstNodePtr& node, Scope* scope,
                                       bool definition)
{
    if(!node || !scope || scope->kind != SCOPE_CLASS) return;
    CPPGMAstNodePtr declarator = ChildOfKind(node, "declarator");
    if(!declarator) throw logic_error("special member has no declarator");
    Analyzer::SpecFacts facts;
    TypePtr function;
    const string declared_name = SpecialMemberName(declarator_name(declarator));
    const vector<Binding*> declared_bindings = DirectBindings(scope, declared_name);
    for(size_t i = 0; i < declared_bindings.size(); ++i) {
      if(declared_bindings[i]->kind != BIND_FUNCTION ||
         declared_bindings[i]->declaration.get() != node.get()) continue;
      function = function_target_type(declared_bindings[i]->type);
      if(function) break;
    }
    if(!function)
      function = analyzer_.BuildDeclarator(declarator, Fundamental("void"), scope);
    function = PA12AdjustedType(function);
    function = function_type(function);
    if(!function) throw logic_error("special member has no function type");
    TypePtr owner = scope->owner_type;
    if(!owner || owner->kind != TYPE_CLASS) throw logic_error("special member has no class owner");
    const string raw_name = declarator_name(declarator);
    const string name = SpecialMemberName(raw_name);
    const string qname = TypeQualifiedName(owner) + "::" +
      special_member_symbol_name(owner, name);
    for(size_t i = 0; i < declared_bindings.size(); ++i)
      if(declared_bindings[i] && declared_bindings[i]->kind == BIND_FUNCTION &&
         declared_bindings[i]->declaration.get() == node.get())
        declared_bindings[i]->qualified_name = qname;
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
      binding.qualified_name = qname;
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
	record->template_instantiation = node->template_instantiation || owner->template_specialization ||
		(owner->direct_base && type_value(owner->direct_base) &&
		 type_value(owner->direct_base)->template_specialization);
	record->weak_binding = record->template_instantiation;
	if(node->template_instantiation) {
		record->template_primary = node->template_primary;
		record->template_arguments = node->template_arguments;
	} else if(owner->template_specialization) {
		record->template_primary = owner->template_primary;
		record->template_arguments = owner->template_arguments;
	}
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
    record->node = node;
    record->variadic = function->variadic;
    if(definition) record->unwind_no = record->unwind_no || HasNoexcept(declarator);
    RememberDefaults(record, declarator);
    ClassifySpecialMember(record);
	if(record->defaulted && record->value_special_member)
      record->unwind_no = record->unwind_no || IsTrivialValueStorage(owner);
    const bool out_of_class_definition = definition && node->value.find("::") != string::npos;
    if(out_of_class_definition && (record->constructor || record->destructor)) {
      record->needed = true;
    }
    const bool constructor_record = record->constructor;
    if(constructor_record &&
       (record->defaulted || (out_of_class_definition &&
                              !record->template_instantiation))) {
      EnsureConstructorBaseEntry(record);
      if(definition && node->value.find("::") != string::npos) {
        FunctionRecord* base_entry = BaseEntryFor(record);
        if(base_entry) base_entry->needed = true;
      }
    }
    const TypePtr destructor_owner = type_value(record->member_owner);
    const bool global_destructor_owner = destructor_owner &&
      destructor_owner->name.find("::") == string::npos;
    if(!constructor_record && out_of_class_definition && record->destructor &&
       (record->unwind_no || global_destructor_owner) &&
       !BaseEntryFor(record)) {
      FunctionRecord base_entry;
      base_entry.node = record->node;
      base_entry.scope = record->scope;
      base_entry.type = record->type;
      base_entry.source_type = record->source_type;
      base_entry.member_owner = record->member_owner;
      base_entry.qualified_name = record->qualified_name + "__base_entry";
      base_entry.definition = true;
      base_entry.member = record->member;
      base_entry.static_member = record->static_member;
      base_entry.destructor = true;
      base_entry.unwind_no = record->unwind_no;
      base_entry.needed = true;
      base_entry.base_entry = true;
      base_entry.base_entry_for = record->qualified_name;
      base_entry.special_initializer = record->special_initializer;
      base_entry.default_arguments = record->default_arguments;
      base_entry.template_instantiation = record->template_instantiation;
      base_entry.weak_binding = record->weak_binding;
      base_entry.template_primary = record->template_primary;
      base_entry.template_arguments = record->template_arguments;
      functions_.push_back(base_entry);
    }
    (void)facts;
  }

void PA14Lowerer::CollectInheritedConstructors(const TypePtr& raw_owner, Scope* scope)
{
    TypePtr owner = type_value(raw_owner);
    TypePtr base = owner ? type_value(owner->direct_base) : TypePtr();
    if(!owner || owner->kind != TYPE_CLASS || !base || !scope) return;
    const string owner_name = LastComponent(owner->name);
    const string base_name = LastComponent(base->name);
    const vector<Binding*> inherited = MemberBindings(base, base_name);
    for(size_t i = 0; i < inherited.size(); ++i) {
      Binding* source_binding = inherited[i];
      if(!source_binding || source_binding->kind != BIND_FUNCTION) continue;
      TypePtr source_function = function_target_type(source_binding->type);
      if(!source_function) continue;
      FunctionRecord* source_record = RecordForBinding(source_binding);
      if(source_record && !source_record->constructor) continue;
      vector<string> source_names;
      vector<CPPGMAstNodePtr> default_arguments;
      bool explicit_constructor = false;
      bool unwind_no = false;
      if(source_record) {
        source_names = ParameterNames(*source_record);
        default_arguments = source_record->default_arguments;
        explicit_constructor = source_record->explicit_constructor;
        unwind_no = source_record->unwind_no;
        if(source_record->source_type) source_function = source_record->source_type;
      }
      const string qname = TypeQualifiedName(owner) + "::" +
        special_member_symbol_name(owner, owner_name);
      const string key = function_key(qname, source_function);
      if(function_by_key_.find(key) != function_by_key_.end()) continue;

      CPPGMAstNodePtr special(new CPPGMAstNode("special-member-definition", owner_name));
      CPPGMAstNodePtr declarator(new CPPGMAstNode("declarator"));
      declarator->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", owner_name)));
      CPPGMAstNodePtr clause(new CPPGMAstNode("parameter-clause"));
      for(size_t p = 0; p < source_function->parameters.size(); ++p) {
        CPPGMAstNodePtr parameter(new CPPGMAstNode("parameter-declaration"));
        parameter->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("type-specifier")));
        CPPGMAstNodePtr parameter_declarator(new CPPGMAstNode("declarator"));
        const size_t source_name_index = p + 1;
        const string parameter_name = source_name_index < source_names.size() &&
          !source_names[source_name_index].empty() ? source_names[source_name_index] :
          "__param" + integer_text(static_cast<long long>(source_name_index));
        parameter_declarator->children.push_back(CPPGMAstNodePtr(
          new CPPGMAstNode("identifier", parameter_name)));
        parameter->children.push_back(parameter_declarator);
        clause->children.push_back(parameter);
      }
      declarator->children.push_back(clause);
      special->children.push_back(declarator);

      CPPGMAstNodePtr ctor_initializer(new CPPGMAstNode("ctor-initializer"));
      CPPGMAstNodePtr mem_initializer(new CPPGMAstNode("mem-initializer"));
      mem_initializer->children.push_back(CPPGMAstNodePtr(
        new CPPGMAstNode("mem-initializer-id", base_name)));
      CPPGMAstNodePtr arguments(new CPPGMAstNode("paren-argument-list"));
      for(size_t p = 0; p < source_function->parameters.size(); ++p) {
        const size_t source_name_index = p + 1;
        const string parameter_name = source_name_index < source_names.size() &&
          !source_names[source_name_index].empty() ? source_names[source_name_index] :
          "__param" + integer_text(static_cast<long long>(source_name_index));
        arguments->children.push_back(CPPGMAstNodePtr(
          new CPPGMAstNode("id-expression", parameter_name)));
      }
      mem_initializer->children.push_back(arguments);
      ctor_initializer->children.push_back(mem_initializer);
      special->children.push_back(ctor_initializer);
      special->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("compound-statement")));

      vector<Binding*> existing = DirectBindings(scope, owner_name);
      Binding* binding = 0;
      for(size_t b = 0; b < existing.size(); ++b) {
        TypePtr candidate = function_target_type(existing[b]->type);
        if(existing[b]->kind == BIND_FUNCTION && candidate &&
           PA12SameType(candidate, source_function, false)) {
          binding = existing[b];
          break;
        }
      }
      if(!binding) {
        Binding imported(BIND_FUNCTION, owner_name, source_function);
        imported.is_member = true;
        imported.is_static = false;
        imported.member_owner = owner;
        imported.access = "public";
        imported.declaration = special;
        binding = scope->add(imported);
      } else {
        binding->type = source_function;
        binding->qualified_name = qname;
        binding->is_member = true;
        binding->is_static = false;
        binding->member_owner = owner;
        binding->declaration = special;
      }

      functions_.push_back(FunctionRecord());
      FunctionRecord* record = &functions_.back();
      function_by_key_[key] = record;
      vector<TypePtr> parameters;
      parameters.push_back(PointerTo(owner));
      parameters.insert(parameters.end(), source_function->parameters.begin(),
        source_function->parameters.end());
      record->node = special;
      record->scope = scope;
      record->source_type = source_function;
      record->type = FunctionOf(parameters, source_function->variadic,
        source_function->child, false, source_function->function_volatile);
      record->member_owner = owner;
      record->qualified_name = qname;
      record->member = true;
      record->static_member = false;
      record->constructor = true;
      record->explicit_constructor = explicit_constructor;
      record->definition = true;
      record->unwind_no = unwind_no;
      record->special_initializer = ctor_initializer;
      record->default_arguments = default_arguments;
      record->template_instantiation = owner->template_specialization ||
        (source_record && source_record->template_instantiation);
      record->weak_binding = record->template_instantiation;
      if(owner->template_specialization) {
        record->template_primary = owner->template_primary;
        record->template_arguments = owner->template_arguments;
      } else if(source_record) {
        record->template_primary = source_record->template_primary;
        record->template_arguments = source_record->template_arguments;
      }
      EnsureConstructorBaseEntry(record);
    }
  }

} // namespace cppgm_pa14_lowering
