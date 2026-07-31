#include "pa14_lowering.h"

#include <functional>

#include <cctype>

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

bool IsGeneratedMemberTemplate(const CPPGMAstNodePtr& node,
                               const string& raw_name)
{
    if(!node || !node->template_instantiation || node->template_primary.empty())
      return false;
    const string primary = LastComponent(node->template_primary);
    string raw_base = LastComponent(raw_name);
    // Materialized member templates may carry both an overload discriminator
    // and an instantiation discriminator (`get__ov1__inst_0`).  The lookup
    // identity remains the source member name (`get`), so remove the first
    // generated suffix before comparing it with template_primary.
    const size_t generated_suffix = raw_base.find("__ov") != string::npos ?
        raw_base.find("__ov") : raw_base.find("__inst_");
    if(generated_suffix != string::npos) raw_base.erase(generated_suffix);
    return node->template_primary.find("::") != string::npos &&
      primary == raw_base;
}

string HexEncode(const string& value)
{
    static const char digits[] = "0123456789abcdef";
    string result;
    result.reserve(value.size() * 2);
    for(size_t i = 0; i < value.size(); ++i) {
      const unsigned char byte = static_cast<unsigned char>(value[i]);
      result += digits[byte >> 4];
      result += digits[byte & 15];
    }
    return result;
}

bool PA14ContainsKind(const CPPGMAstNodePtr& node, const string& kind)
{
    if(!node) return false;
    if(node->kind == kind) return true;
    for(size_t child = 0; child < node->children.size(); ++child)
      if(PA14ContainsKind(node->children[child], kind)) return true;
    return false;
}

string PA14NodeValues(const CPPGMAstNodePtr& node)
{
    if(!node) return string();
    string result = node->value;
    for(size_t child = 0; child < node->children.size(); ++child) {
      const string value = PA14NodeValues(node->children[child]);
      if(!value.empty()) {
        if(!result.empty()) result += ' ';
        result += value;
      }
    }
    return result;
}

string PA14TypeUseName(const CPPGMAstNodePtr& node)
{
    if(!node) return string();
    if(node->kind == "type-name") return node->value;
    if(node->kind == "decl-specifier") {
      const size_t marker = node->value.find(':');
      return marker == string::npos ? node->value : node->value.substr(marker + 1);
    }
    for(size_t child = 0; child < node->children.size(); ++child) {
      const string result = PA14TypeUseName(node->children[child]);
      if(!result.empty()) return result;
    }
    return string();
}

string PA14TypeUseKey(string raw)
{
    while(!raw.empty() && (raw[0] == ':' || isspace(static_cast<unsigned char>(raw[0]))))
      raw.erase(0, 1);
    const size_t identifier_marker = raw.find(':');
    if(identifier_marker != string::npos &&
       raw.compare(0, identifier_marker, "TT_IDENTIFIER") == 0)
      raw.erase(0, identifier_marker + 1);
    while(raw.compare(0, 8, "typename ") == 0) raw.erase(0, 8);
    const size_t template_open = raw.find('<');
    if(template_open != string::npos) raw.erase(template_open);
    const size_t scope_marker = raw.find(':');
    if(scope_marker != string::npos && scope_marker + 1 < raw.size() &&
       raw[scope_marker + 1] == ':')
      raw.erase(0, scope_marker + 2);
    return LastComponent(raw);
}

} // namespace

void PA14Lowerer::IndexCompleteTemplateObjectUses(const CPPGMAstNodePtr& node)
{
    if(!node) return;
    const auto mark_complete_type = [&](const TypePtr& raw_type,
      set<const Type*>* parameter_uses) {
      set<const Type*> visited;
      function<void(const TypePtr&)> mark = [&](const TypePtr& raw_current) {
        const TypePtr current = type_value(raw_current);
        if(!current || current->kind != TYPE_CLASS ||
           !visited.insert(current.get()).second) return;
        // The index is also consumed by polymorphic lowering for ordinary
        // classes, not only by template static-member emission.
        complete_template_object_uses_.insert(current.get());
        if(parameter_uses) parameter_uses->insert(current.get());
        for(size_t base = 0; base < current->direct_bases.size(); ++base)
          mark(current->direct_bases[base]);
        if(current->direct_bases.empty()) mark(current->direct_base);
      };
      mark(raw_type);
    };
    // A generated class passed by value is a complete object use even if the
    // only source occurrence is a function parameter.  Parameter clauses
    // used to be skipped wholesale, which incorrectly deferred the static
    // storage of concrete integral template members and their base chain.
    if(node->kind == "parameter-declaration" && !node->children.empty()) {
      const CPPGMAstNodePtr declarator = node->children.size() > 1 ?
        node->children[1] : CPPGMAstNodePtr();
      if(!PA14ContainsKind(declarator, "ptr-operator") &&
         !PA14ContainsKind(declarator, "array-suffix")) {
        const string key = PA14TypeUseKey(PA14TypeUseName(node->children[0]));
        map<string, vector<TypePtr> >::const_iterator parameter_type =
          class_types_by_name_.find(key);
        if(parameter_type != class_types_by_name_.end())
          for(size_t type = 0; type < parameter_type->second.size(); ++type)
            mark_complete_type(parameter_type->second[type],
              &complete_template_parameter_uses_);
      }
    }
    if(node->kind == "new-expression" || node->kind == "call-expression") {
      const CPPGMAstNodePtr type_id = node->kind == "new-expression" ?
        ChildOfKind(node, "type-id") : CPPGMAstNodePtr();
      const string callee = node->kind == "call-expression" && !node->children.empty() &&
        node->children[0] && node->children[0]->kind == "id-expression" ?
        node->children[0]->value : string();
      const string raw_name = node->kind == "new-expression" ?
        PA14TypeUseName(type_id) : callee;
      const string name = PA14TypeUseKey(raw_name);
      map<string, vector<TypePtr> >::const_iterator found =
        class_types_by_name_.find(name);
      if(found != class_types_by_name_.end())
        for(size_t type = 0; type < found->second.size(); ++type)
          mark_complete_type(found->second[type], 0);
    }
    if(node->kind == "simple-declaration" && !node->children.empty() &&
       PA14NodeValues(node->children[0]).find("typedef") == string::npos &&
       !node->template_instantiation) {
      const CPPGMAstNodePtr list = ChildOfKind(node, "init-declarator-list");
      for(size_t item = 0; list && item < list->children.size(); ++item) {
        const CPPGMAstNodePtr entry = list->children[item];
        if(!entry || entry->children.empty()) continue;
        const CPPGMAstNodePtr declarator = entry->children[0];
        if(!declarator || declarator_name(declarator).find("::") != string::npos ||
           PA14ContainsKind(declarator, "ptr-operator") ||
           PA14ContainsKind(declarator, "parameter-clause")) continue;
        const string name = PA14TypeUseKey(PA14TypeUseName(node->children[0]));
        map<string, vector<TypePtr> >::const_iterator found =
          class_types_by_name_.find(name);
        if(found != class_types_by_name_.end())
          for(size_t type = 0; type < found->second.size(); ++type) {
            const TypePtr value = found->second[type];
            if(value->template_specialization && value->template_primary.empty()) continue;
            mark_complete_type(value, 0);
          }
      }
    }
    for(size_t child = 0; child < node->children.size(); ++child)
      IndexCompleteTemplateObjectUses(node->children[child]);
}

bool PA14Lowerer::HasInline(const CPPGMAstNodePtr& node) const
{
    if(!node) return false;
    if(node->value == "KW_INLINE:inline" || node->value == "inline") return true;
    for(size_t i = 0; i < node->children.size(); ++i)
      if(HasInline(node->children[i])) return true;
    return false;
  }

bool PA14Lowerer::TemplatePrimaryHasNonstaticMemberFunction(const TypePtr& raw_type) const
{
    const TypePtr type = type_value(raw_type);
    if(!type || !type->template_specialization || type->template_primary.empty())
      return false;
    map<string, vector<TypePtr> >::const_iterator group = class_types_by_name_.find(
      LastComponent(type->template_primary));
    if(group == class_types_by_name_.end()) return false;
    for(size_t candidate = 0; candidate < group->second.size(); ++candidate) {
      const TypePtr primary = group->second[candidate];
      if(primary && primary.get() != type.get() && !primary->template_specialization &&
         (primary->name == type->template_primary ||
          TypeQualifiedName(primary) == type->template_primary) &&
         HasNonstaticMemberFunction(primary)) return true;
    }
    return false;
}

void PA14Lowerer::CollectFunction(const CPPGMAstNodePtr& node, Scope* scope, bool definition)
{
    if(!node || node->children.size() < 2) throw logic_error("invalid function declaration");
    Analyzer::SpecFacts facts;
    Scope* type_scope = scope;
    if(!type_scope || type_scope->kind != SCOPE_TEMPLATE_PARAMETERS) {
      // The ordinary class path already has the class scope as its lookup
      // context.  Template-parameter scopes are supplied by the caller and
      // must remain the lookup root for dependent return/parameter types.
      type_scope = scope;
    }
    TypePtr base = analyzer_.TypeFromSpecSeq(node->children[0], type_scope, &facts);
    const string raw_name = declarator_name(node->children[1]);
    if(raw_name.empty()) throw logic_error("function has no name");
    TypePtr member_owner;
    // A member template is lowered from a reconstructed template-parameter
    // scope whose parent is the owning class.  Keep the lexical template
    // scope for dependent type lookup, but recover the class owner by walking
    // to the nearest class scope.
    for(Scope* owner_scope = scope; owner_scope; owner_scope = owner_scope->parent)
      if(owner_scope->kind == SCOPE_CLASS) {
        member_owner = owner_scope->owner_type;
        break;
      }
    if(!member_owner && raw_name.find("::") != string::npos) {
      const size_t separator = raw_name.rfind("::");
      Analyzer::PathTarget owner = analyzer_.ResolvePath(scope, raw_name.substr(0, separator));
      if(owner.binding) member_owner = owner.binding->type;
      else if(owner.scope) member_owner = owner.scope->owner_type;
    }
    if(member_owner && member_owner->kind != TYPE_CLASS) member_owner.reset();
    if(type_scope && type_scope->kind != SCOPE_TEMPLATE_PARAMETERS)
      type_scope = member_owner && member_owner->owned_scope ?
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
    else if(is_member) {
      string member_name = LastComponent(raw_name);
      bool source_named_member_template = false;
      if(is_static && IsGeneratedMemberTemplate(node, raw_name) && member_owner &&
         member_owner->owned_scope)
        for(size_t member = 0; member < member_owner->owned_scope->bindings.size(); ++member) {
          const Binding& binding = member_owner->owned_scope->bindings[member];
          if(binding.kind == BIND_VARIABLE && binding.is_static && binding.declaration &&
             binding.declaration->template_instantiation &&
             binding.declaration->template_primary.find("::") != string::npos) {
            source_named_member_template = true;
            break;
          }
        }
      if(source_named_member_template && !node->template_primary.empty())
        member_name = LastComponent(node->template_primary);
      qname = TypeQualifiedName(member_owner) + "::" + member_name;
    }
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
	record->member_template = record->member_template ||
		(is_member && IsGeneratedMemberTemplate(node, raw_name));
	record->template_instantiation = record->template_instantiation ||
		node->template_instantiation ||
		(member_owner && member_owner->template_specialization);
	record->explicit_specialization = record->explicit_specialization ||
		node->explicit_specialization;
	record->extern_template = record->extern_template || node->extern_instantiation;
	record->object_root = record->object_root || node->explicit_instantiation;
	// An explicit function specialization is an ordinary definition with a
	// concrete template identity.  It must remain an emission root even when
	// the current translation unit does not call that overload; treating it as
	// demand-driven drops the specialization from the LowIR surface.
	if((node->explicit_specialization &&
		(node->kind == "function-definition" ||
		 node->kind == "special-member-definition")) ||
		node->explicit_instantiation || node->extern_instantiation) record->needed = true;
	record->weak_binding = record->template_instantiation && !record->extern_template;
	record->inline_definition = record->inline_definition || HasInline(node) || facts.is_constexpr;
	const bool out_of_class_definition = definition && is_member && member_owner &&
		member_owner->owned_scope && scope != member_owner->owned_scope;
	record->out_of_class_definition = record->out_of_class_definition ||
		out_of_class_definition;
	// A concrete out-of-class member definition is an emission root.  Primary
	// template bodies and member-template instantiations remain demand-driven,
	// while ordinary members and members of an already materialized class
	// specialization retain their typed definition.
	if(out_of_class_definition && !record->member_template &&
		!record->template_instantiation && !member_owner->template_specialization)
		record->needed = true;
	if(node->template_instantiation || node->extern_instantiation) {
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
	if(is_member && !record->member_template && member_owner &&
		!member_owner->template_specialization) {
		const string ordinary_member_name = LastComponent(raw_name);
		if(ordinary_member_name.compare(0, 8, "operator") != 0) {
			for(size_t prior = 0; prior < functions_.size(); ++prior) {
				const FunctionRecord& materialized = functions_[prior];
				if(!materialized.member_template || materialized.member_owner != member_owner ||
					LastComponent(materialized.template_primary) != ordinary_member_name) continue;
				record->needed = true;
				break;
			}
		}
	}
	// Materialized member templates retain a generated declarator spelling
	// (`choose__inst_...`, or `operator-__inst_...`) for symbol uniqueness.
	// Overload lookup still needs the source member name, so install a typed
	// alias binding to the same record without changing its emitted symbol.
	if(record->member_template && is_member && member_owner->owned_scope &&
		node->template_primary.find("::") != string::npos) {
		const string source_member = LastComponent(node->template_primary);
		if(!source_member.empty() && source_member != LastComponent(raw_name)) {
			const vector<Binding*> operator_bindings = DirectBindings(
				member_owner->owned_scope, source_member);
			bool has_alias = false;
			for(size_t alias = 0; alias < operator_bindings.size(); ++alias)
				if(operator_bindings[alias] && operator_bindings[alias]->qualified_name == qname)
					has_alias = true;
			if(!has_alias) {
				Binding alias(BIND_FUNCTION, source_member, function);
				alias.qualified_name = qname;
				alias.is_member = true;
				alias.is_static = is_static;
				alias.member_owner = member_owner;
				member_owner->owned_scope->add(alias);
			}
		}
	}
    record->definition = record->definition || definition;
    // A constexpr static member function is available to the semantic
    // constant evaluator without requiring a LowIR body.  Runtime calls and
    // address-takes mark the record through normal demand tracking; keeping
    // this declaration-time bit clear avoids emitting bodies used only by
    // static_assert/constant initialization.
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
    if(definition && facts.is_constexpr && record->member && record->static_member &&
       raw_name.find("::") != string::npos)
      record->needed = true;
    if(definition && !hidden_friend) {
      map<const CPPGMAstNode*, Scope*>::const_iterator function_scope =
        analyzer_.function_scopes_.find(node.get());
      if(function_scope != analyzer_.function_scopes_.end()) {
        string local_static_function_name = qname;
        if(record->member) {
          const string object_name = TemplateFunctionObjectName(*record);
          if(!object_name.empty())
            local_static_function_name = "function_symbol_" + HexEncode(object_name);
        }
        CollectLocalStatics(ChildOfKind(node, "compound-statement"),
          function_scope->second, local_static_function_name);
      }
    }
  }

void PA14Lowerer::CollectLocalStatics(const CPPGMAstNodePtr& node, Scope* scope,
                                      const string& function_name)
{
    if(!node) return;
    if(node->kind == "class-specifier") {
      // Local classes are not reached through CollectTopLevel.  Collect
      // their member records here before a local-static initializer asks for
      // a constructor; otherwise the synthesized aggregate constructor can
      // hide the user-declared constructor body.
      CollectClassMembers(node, scope);
      return;
    }
    if(node->kind == "class-forward-declaration") return;
    if(node->kind == "compound-statement") {
      map<const CPPGMAstNode*, Scope*>::const_iterator found =
        analyzer_.compound_scopes_.find(node.get());
      Scope* child_scope = found == analyzer_.compound_scopes_.end() ? scope : found->second;
      for(size_t i = 0; i < node->children.size(); ++i)
        CollectLocalStatics(node->children[i], child_scope, function_name);
      return;
    }
    if(node->kind == "simple-declaration" && !node->children.empty()) {
      Analyzer::SpecFacts facts;
      TypePtr base = analyzer_.TypeFromSpecSeq(node->children[0], scope, &facts);
      if(facts.is_static && !facts.is_typedef) {
        CPPGMAstNodePtr list = ChildOfKind(node, "init-declarator-list");
        if(list) for(size_t i = 0; i < list->children.size(); ++i) {
          const CPPGMAstNodePtr item = list->children[i];
          if(!item || item->children.empty()) continue;
          const CPPGMAstNodePtr declarator = item->children[0];
          const string name = declarator_name(declarator);
          if(name.empty()) continue;
          const CPPGMAstNodePtr initializer = item->children.size() > 1 ?
            item->children[1] : CPPGMAstNodePtr();
          TypePtr type = PlannedType(node->children[0], declarator, scope, initializer);
          const size_t begin = item->source_token_begin;
          const size_t end = item->source_token_end;
          if(begin == static_cast<size_t>(-1) || end == static_cast<size_t>(-1)) continue;
          const string local_name = "__local_static__" + LastComponent(function_name) +
            "__" + name + "__tokens" + integer_text(static_cast<long long>(begin)) +
            "_" + integer_text(static_cast<long long>(end));
          GlobalRecord record;
          record.node = node;
          record.scope = scope;
          record.type = type;
          record.qualified_name = local_name;
          record.initializer = initializer;
          record.declaration = false;
          record.internal = true;
          record.local_static = true;
          const TypePtr value_type = type_value(type);
          long long constant = 0;
          const bool constant_integral = initializer && value_type &&
            is_integral_type(value_type) &&
            FoldInteger(InitializerExpression(initializer), scope, &constant, 0);
          record.dynamic_initializer = initializer && !constant_integral;
          map<string, GlobalRecord*>::iterator prior = global_by_key_.find(
            global_key(local_name));
          GlobalRecord* stored = 0;
          if(prior == global_by_key_.end()) {
            globals_.push_back(record);
            stored = &globals_.back();
            global_by_key_[global_key(local_name)] = stored;
          } else {
            stored = prior->second;
            if(initializer) stored->initializer = initializer;
            stored->type = type;
            stored->dynamic_initializer = stored->dynamic_initializer || record.dynamic_initializer;
          }
          if(record.dynamic_initializer) {
            GlobalRecord guard;
            guard.node = node;
            guard.scope = scope;
            guard.type = Fundamental("long int");
            guard.qualified_name = local_name + "__guard";
            guard.declaration = false;
            guard.internal = true;
            guard.local_static = true;
            guard.local_static_guard = true;
            if(global_by_key_.find(global_key(guard.qualified_name)) == global_by_key_.end()) {
              globals_.push_back(guard);
              global_by_key_[global_key(guard.qualified_name)] = &globals_.back();
            }
          }
          local_static_plans_[declarator.get()] = stored;
          Binding* binding = scope ? scope->local(name) : 0;
          if(binding) binding->qualified_name = local_name;
        }
      }
      return;
    }
    for(size_t i = 0; i < node->children.size(); ++i)
      CollectLocalStatics(node->children[i], scope, function_name);
  }

void PA14Lowerer::ClassifySpecialMember(FunctionRecord* record)
{
    if(!record || !record->member || record->member_template || record->static_member ||
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
	bool definition, bool out_of_class_member)
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
    const string raw_name = declarator_name(declarator);
    const string conversion_suffix = raw_name.compare(0, 8, "operator") == 0 ?
      raw_name.substr(8) : string();
    const bool conversion_operator = !conversion_suffix.empty() &&
      (std::isalnum(static_cast<unsigned char>(conversion_suffix[0])) ||
       conversion_suffix[0] == '_' || conversion_suffix[0] == ' ');
    if(conversion_operator) {
      string target_name = conversion_suffix;
      while(!target_name.empty() && target_name[0] == ' ') target_name.erase(0, 1);
      Analyzer::PathTarget target = analyzer_.ResolvePath(scope, target_name);
      TypePtr target_type = target.binding ? type_value(target.binding->type) : TypePtr();
      if(target_type && target_type->kind == TYPE_CLASS) {
        TypePtr adjusted(new Type(*function));
        adjusted->child = target_type;
        function = adjusted;
      }
    }
	if(conversion_operator) for(size_t i = 0; i < declared_bindings.size(); ++i)
      if(declared_bindings[i] && declared_bindings[i]->kind == BIND_FUNCTION &&
         declared_bindings[i]->declaration.get() == node.get())
        declared_bindings[i]->type = function;
    TypePtr owner = scope->owner_type;
    if(!owner || owner->kind != TYPE_CLASS) throw logic_error("special member has no class owner");
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
	record->member_template = record->member_template ||
		(conversion_operator && node->template_instantiation &&
		 node->template_primary.find("operator") != string::npos);
	const bool out_of_class_definition = definition && out_of_class_member;
	record->out_of_class_definition = record->out_of_class_definition ||
		out_of_class_definition;
	record->template_instantiation = node->template_instantiation || owner->template_specialization ||
		(owner->direct_base && type_value(owner->direct_base) &&
		 type_value(owner->direct_base)->template_specialization);
	record->weak_binding = record->template_instantiation;
	// A special-member definition written in the class body is inline even
	// when it is not a template.  Preserve that linkage fact so its emitted
	// object uses the ABI constructor identity and receives the C2 alias.
	record->inline_definition = record->inline_definition ||
		(definition && !out_of_class_member);
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
    record->explicit_specialization = record->explicit_specialization ||
      node->explicit_specialization;
    if(definition) record->unwind_no = record->unwind_no || HasNoexcept(declarator);
    RememberDefaults(record, declarator);
	ClassifySpecialMember(record);
	// A materialized class specialization used as a concrete data member has a
	// real constructor definition even when the containing aggregate's own
	// default construction is trivial.  Retain the empty in-class constructor
	// body in the emission frontier; this is also the definition needed by the
	// typed template closure for such a member object.
	if(record->constructor && !record->member_template &&
		record->template_instantiation && owner->template_specialization &&
		record->source_type && record->source_type->parameters.empty()) {
		if(materialized_member_object_uses_.find(owner.get()) !=
			materialized_member_object_uses_.end()) record->needed = true;
	}
	if(record->defaulted && record->value_special_member)
      record->unwind_no = record->unwind_no || IsTrivialValueStorage(owner);
	if(out_of_class_definition && (record->constructor || record->destructor) &&
		(!record->template_instantiation ||
		 (owner->template_specialization && record->source_type &&
		  !record->source_type->parameters.empty()))) {
		record->needed = true;
	}
	const bool constructor_record = record->constructor;
	if(constructor_record &&
	   (record->defaulted || (out_of_class_definition &&
							  !record->template_instantiation))) {
      EnsureConstructorBaseEntry(record);
      if(out_of_class_definition) {
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
		// A materialized template constructor can carry a synthesized function
		// record whose parameter metadata has already fallen back to ABI names.
		// The inherited wrapper is still required to expose the source
		// declarator's names in LowIR, so recover them from the binding AST before
		// manufacturing its forwarding declaration.
		if(source_binding->declaration) {
			CPPGMAstNodePtr source_declarator;
			if(source_binding->declaration->kind == "function-definition" &&
				source_binding->declaration->children.size() > 1)
				source_declarator = source_binding->declaration->children[1];
			else source_declarator = ChildOfKind(source_binding->declaration, "declarator");
			const CPPGMAstNodePtr source_clause = source_declarator ?
				DescendantOfKind(source_declarator, "parameter-clause") :
				CPPGMAstNodePtr();
			if(source_clause) {
				if(source_names.size() < source_function->parameters.size() + 1)
					source_names.resize(source_function->parameters.size() + 1);
				for(size_t p = 0; p < source_function->parameters.size() &&
					p < source_clause->children.size(); ++p) {
					const size_t name_index = p + 1;
					if(!source_names[name_index].empty() &&
						source_names[name_index].compare(0, 7, "__param") != 0) continue;
					const CPPGMAstNodePtr parameter = source_clause->children[p];
					if(!parameter || parameter->children.size() < 2) continue;
					const string declared = declarator_name(parameter->children[1]);
					if(!declared.empty()) source_names[name_index] = declared;
				}
			}
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
        CPPGMAstNodePtr argument(new CPPGMAstNode("id-expression", parameter_name));
		// An inherited forwarding constructor must preserve the reference
		// category when it initializes the concrete base.  Its parameter is an
		// lvalue expression inside the synthesized body, so model the source
		// rvalue-reference type as an explicit cast rather than attempting to
		// bind that lvalue directly to U&&.
		if(source_function->parameters[p] &&
			source_function->parameters[p]->kind == TYPE_RVALUE_REFERENCE &&
			source_binding->declaration) {
			CPPGMAstNodePtr source_declarator = ChildOfKind(
				source_binding->declaration, "declarator");
			CPPGMAstNodePtr source_clause = source_declarator ?
				DescendantOfKind(source_declarator, "parameter-clause") :
				CPPGMAstNodePtr();
			CPPGMAstNodePtr source_parameter = source_clause && p <
				source_clause->children.size() ? source_clause->children[p] :
				CPPGMAstNodePtr();
			if(source_parameter && source_parameter->children.size() >= 2) {
				CPPGMAstNodePtr type_id(new CPPGMAstNode("type-id"));
				CPPGMAstNodePtr specifiers(new CPPGMAstNode("type-specifier-seq"));
				if(source_parameter->children[0]) for(size_t specifier = 0;
					specifier < source_parameter->children[0]->children.size(); ++specifier) {
					const CPPGMAstNodePtr source_specifier =
						source_parameter->children[0]->children[specifier];
					if(!source_specifier) continue;
					string spelling = source_specifier->value;
					const size_t marker = spelling.find(':');
					if(marker != string::npos) spelling = spelling.substr(marker + 1);
					specifiers->children.push_back(CPPGMAstNodePtr(
						new CPPGMAstNode("type-name", spelling)));
				}
				type_id->children.push_back(specifiers);
				type_id->children.push_back(source_parameter->children[1]);
				CPPGMAstNodePtr cast(new CPPGMAstNode("cast-expression",
					"KW_STATIC_CAST:static_cast"));
				cast->children.push_back(type_id);
				cast->children.push_back(argument);
				argument = cast;
			}
		}
		arguments->children.push_back(argument);
      }
      mem_initializer->children.push_back(arguments);
      ctor_initializer->children.push_back(mem_initializer);
      special->children.push_back(ctor_initializer);
      special->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("compound-statement")));
		// This constructor is synthesized after PA11 has analyzed the source
		// tree, so it has no parser-created function scope.  Give its forwarding
		// parameters a typed scope of its own; otherwise the base mem-initializer
		// sees `__param1` as an unbound expression and rejects the inherited
		// constructor even though its semantic signature is viable.
		Scope* function_scope = analyzer_.NewChild(scope, SCOPE_FUNCTION, qname);
		for(size_t p = 0; p < source_function->parameters.size(); ++p) {
			const size_t source_name_index = p + 1;
			const string parameter_name = source_name_index < source_names.size() &&
				!source_names[source_name_index].empty() ? source_names[source_name_index] :
				"__param" + integer_text(static_cast<long long>(source_name_index));
			function_scope->add(Binding(BIND_PARAMETER, parameter_name,
				source_function->parameters[p]));
		}
		// Keep the synthetic node on the same lookup path as a parser-created
		// function.  FunctionScope() consults this map before the record fallback.
		analyzer_.function_scopes_[special.get()] = function_scope;

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
      record->scope = function_scope;
      record->source_type = source_function;
      record->type = FunctionOf(parameters, source_function->variadic,
        source_function->child, false, source_function->function_volatile);
      record->member_owner = owner;
      record->qualified_name = qname;
      record->member = true;
      record->static_member = false;
      record->constructor = true;
      record->inherited_constructor_wrapper = true;
      record->explicit_constructor = explicit_constructor;
      record->definition = true;
      record->unwind_no = unwind_no;
      record->special_initializer = ctor_initializer;
      record->default_arguments = default_arguments;
      record->template_instantiation = owner->template_specialization ||
        (source_record && source_record->template_instantiation);
      record->explicit_specialization = source_record &&
        source_record->explicit_specialization;
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

void PA14Lowerer::CollectSimpleDeclaration(const CPPGMAstNodePtr& node, Scope* scope)
{
    if(!node || node->children.empty()) return;
    CheckTypeAccess(node->children[0], scope);
    Analyzer::SpecFacts facts;
    analyzer_.TypeFromSpecSeq(node->children[0], scope, &facts);
    CPPGMAstNodePtr list = ChildOfKind(node, "init-declarator-list");
    if(!list) return;
    for(size_t i = 0; i < list->children.size(); ++i)
      CollectSimpleDeclarationItem(node, scope, facts, list->children[i]);
  }

void PA14Lowerer::CollectSimpleDeclarationItem(const CPPGMAstNodePtr& node,
                                                Scope* scope,
                                                const Analyzer::SpecFacts& facts,
                                                const CPPGMAstNodePtr& item)
{
    if(!item || item->children.empty()) return;
    CPPGMAstNodePtr declarator = item->children[0];
    CPPGMAstNodePtr initializer = item->children.size() > 1 ? item->children[1] :
      CPPGMAstNodePtr();
    TypePtr type = PlannedType(node->children[0], declarator, scope, initializer);
    if(facts.is_constexpr && type->kind != TYPE_FUNCTION)
      type = CloneWithCv(type, true, false);
    if(facts.is_typedef) return;
    const string name = declarator_name(declarator);
    if(name.empty()) return;
    if(type->kind == TYPE_FUNCTION) {
      if(initializer && !initializer->children.empty() && initializer->children[0] &&
         initializer->children[0]->kind == "special-initializer" &&
         initializer->children[0]->value == "delete") return;
      CPPGMAstNodePtr wrapper(new CPPGMAstNode("function-declaration"));
      wrapper->children.push_back(node->children[0]);
      wrapper->children.push_back(declarator);
      if(item->children.size() > 1) wrapper->children.push_back(item->children[1]);
      wrapper->template_instantiation = node->template_instantiation;
	  wrapper->explicit_specialization = node->explicit_specialization;
      wrapper->explicit_instantiation = node->explicit_instantiation;
      wrapper->extern_instantiation = node->extern_instantiation;
      wrapper->template_primary = node->template_primary;
      wrapper->template_arguments = node->template_arguments;
      CPPGMAstNodePtr special_initializer = ChildOfKind(initializer, "special-initializer");
      const bool definition = special_initializer && special_initializer->value == "default";
      CollectFunction(wrapper, scope, definition);
      return;
    }
    const bool is_extern = HasStorageSpecifier(node, "extern");
    if(is_extern && item->children.size() < 2) return;
    CollectGlobalDeclaration(node, scope, facts, item, initializer, name, type);
  }

bool PA14Lowerer::PrepareGlobalDeclaration(const CPPGMAstNodePtr& node,
                                           Scope* scope,
                                           const Analyzer::SpecFacts& facts,
                                           const CPPGMAstNodePtr& initializer,
                                           const string& name,
                                           const TypePtr& type,
                                           GlobalRecord* record)
{
    record->node = node;
    record->scope = scope;
    record->type = type;
    record->qualified_name = qualified_name(scope, name);
    record->template_instantiation = node->template_instantiation;
    record->explicit_specialization = node->explicit_specialization;
    record->weak_binding = record->template_instantiation;
    if(name.find("::") != string::npos) {
      const size_t separator = name.rfind("::");
      Analyzer::PathTarget owner = analyzer_.ResolvePath(scope, name.substr(0, separator));
      record->template_owner = owner.binding ? type_value(owner.binding->type) :
        (owner.scope ? owner.scope->owner_type : TypePtr());
      if(record->template_owner && record->template_owner->template_specialization)
        record->template_instantiation = record->weak_binding = true;
    }
    record->initializer = initializer;
    Binding* semantic_binding = scope ? scope->local(name) : 0;
    const TypePtr record_value = type_value(record->type);
    const size_t qualified_separator = name.rfind("::");
    const bool template_qualified_name = qualified_separator != string::npos &&
      name.substr(0, qualified_separator).find('<') != string::npos;
    const bool integral_storage = record_value &&
      (is_integral_type(record_value) ||
       (record_value->kind == TYPE_FUNDAMENTAL && record_value->name == "bool"));
    Binding* member_binding = semantic_binding;
    if(record->template_owner && record->template_owner->owned_scope &&
       name.find("::") != string::npos) {
      const string member_name = name.substr(name.rfind("::") + 2);
      vector<Binding*> candidates = DirectBindings(record->template_owner->owned_scope, member_name);
      for(size_t candidate = 0; candidate < candidates.size(); ++candidate)
        if(candidates[candidate]->kind == BIND_VARIABLE &&
           candidates[candidate]->is_member && candidates[candidate]->is_static) {
          member_binding = candidates[candidate];
          break;
        }
    }
    const bool deferred_static_integral_storage =
      (node->template_instantiation || record->template_owner || template_qualified_name) &&
      !initializer && member_binding && member_binding->is_member &&
      member_binding->is_static && integral_storage &&
      (member_binding->has_value || facts.is_const || facts.is_constexpr) &&
      !(record->template_owner && complete_template_parameter_uses_.find(
        record->template_owner.get()) != complete_template_parameter_uses_.end()) &&
      !(record->template_owner && complete_template_object_uses_.find(
        record->template_owner.get()) != complete_template_object_uses_.end() &&
        LastComponent(record->template_owner->template_primary) == "integral_constant");
    const bool deferred_static_integral_definition =
      !node->explicit_specialization && node->template_instantiation &&
      record->template_owner && initializer && member_binding &&
      member_binding->is_member && member_binding->is_static &&
      !member_binding->has_value &&
      (facts.is_const || facts.is_constexpr) && integral_storage;
    const bool generated_integral_constant = !record->template_owner &&
      node->template_instantiation && (facts.is_const || facts.is_constexpr) &&
      record_value && (is_integral_type(record_value) ||
        (record_value->kind == TYPE_FUNDAMENTAL && record_value->name == "bool")) &&
      semantic_binding && semantic_binding->has_value;
    const TypePtr record_value_type = type_value(record->type);
    const bool enum_function_style_initializer = record_value_type &&
      record_value_type->kind == TYPE_ENUM && record->initializer &&
      !record->initializer->children.empty() && record->initializer->children[0] &&
      record->initializer->children[0]->kind == "paren-initializer";
    if(enum_function_style_initializer)
      record->object_name = "_Z" + integer_text(static_cast<long long>(name.size())) + name;
    if(generated_integral_constant || deferred_static_integral_storage ||
       deferred_static_integral_definition) {
      if(deferred_static_integral_storage || deferred_static_integral_definition)
        deferred_static_members_.insert(record->qualified_name);
      // An out-of-class definition is storage when its class specialization
      // has a complete object use.  Pointer and typedef-only uses keep the
      // old deferred behavior and do not materialize this dependent member.
      if(!deferred_static_integral_definition) return true;
      if(!record->template_owner ||
         complete_template_object_uses_.find(record->template_owner.get()) ==
           complete_template_object_uses_.end()) return true;
    }
    return false;
  }

void PA14Lowerer::CollectGlobalDeclaration(const CPPGMAstNodePtr& node,
                                           Scope* scope,
                                           const Analyzer::SpecFacts& facts,
                                           const CPPGMAstNodePtr& item,
                                           const CPPGMAstNodePtr& initializer,
                                           const string& name,
                                           const TypePtr& type)
{
    (void)item;
    GlobalRecord record;
    if(PrepareGlobalDeclaration(node, scope, facts, initializer, name, type, &record))
      return;
    if(!record.initializer && record.template_owner && record.template_owner->owned_scope) {
      const string member_name = name.substr(name.rfind("::") + 2);
      for(size_t member = 0; member < record.template_owner->class_members.size(); ++member)
        if(record.template_owner->class_members[member].name == member_name &&
           record.template_owner->class_members[member].initializer) {
          record.initializer = record.template_owner->class_members[member].initializer;
          break;
        }
    }
    if(facts.is_constexpr && record.initializer)
      DemandConstantObjectConstructors(record.type, record.initializer);
    if(record.template_owner && record.initializer && record.template_owner->owned_scope) {
      const string member_name = name.substr(name.rfind("::") + 2);
      vector<Binding*> members = DirectBindings(record.template_owner->owned_scope, member_name);
      long long constant = 0;
      if(FoldInteger(InitializerExpression(record.initializer), scope, &constant, 0))
        for(size_t member = 0; member < members.size(); ++member)
          if(members[member] && members[member]->kind == BIND_VARIABLE &&
             type_value(members[member]->type) && type_value(members[member]->type)->is_const) {
            if(record.explicit_specialization || !members[member]->has_value) {
              members[member]->constant_value = PA19Convert(
                PA19IntegralValue::Signed(constant, "long long", 64),
                PA19Type(TypeText(members[member]->type, true)));
              members[member]->has_value = true;
              members[member]->value = PA19Signed(members[member]->constant_value);
            }
          }
    }
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
    const bool is_extern = HasStorageSpecifier(node, "extern");
    if(!is_extern && object) {
      const bool static_member_object = static_cast<bool>(record.template_owner);
      record.dynamic_initializer = !static_member_object ||
        HasDefaultConstructionEffects(value_type) ||
        (value_type && value_type->kind == TYPE_CLASS &&
         HasUserProvidedConstructor(value_type)) ||
        (static_member_object && static_cast<bool>(record.initializer) &&
         !facts.is_constexpr);
      // A constexpr object whose only construction is a trivial/defaulted
      // empty construction is already represented by its zero-initialized
      // static storage.  Do not manufacture a runtime constructor call for
      // that case; nontrivial constexpr constructors (and aggregate values
      // with explicit data) still use the ordinary initialization path.
      CPPGMAstNodePtr object_expression = InitializerExpression(record.initializer);
      const bool empty_value_initialization = object_expression &&
        ((object_expression->kind == "call-expression" &&
          object_expression->children.size() > 1 && object_expression->children[1] &&
          object_expression->children[1]->children.empty()) ||
         (object_expression->kind == "braced-init-list" &&
          object_expression->children.empty()));
      const bool constexpr_empty_initialization = facts.is_constexpr &&
         HasConstructor(value_type) && IsTrivialValueStorage(value_type);
      const bool static_empty_initialization = facts.is_const &&
         HasStorageSpecifier(node, "static") &&
         IsEmptyBaseStorage(value_type) && IsTrivialValueStorage(value_type) &&
         !HasUserProvidedConstructor(value_type);
      if((constexpr_empty_initialization || static_empty_initialization) &&
         empty_value_initialization && !static_member_object)
        record.dynamic_initializer = false;
      record.dynamic_finalizer = HasDestructor(value_type) &&
        (!static_member_object || DestructorHasEffects(value_type));
      if(record.dynamic_initializer) needs_init_helper_ = true;
      if(record.dynamic_finalizer) needs_fini_helper_ = true;
    }
    StoreGlobalDeclaration(record, type_value(record.type));
  }

void PA14Lowerer::DemandConstantObjectConstructors(const TypePtr& raw_type,
                                                    const CPPGMAstNodePtr& initializer)
{
    TypePtr type = type_value(raw_type);
    if(!type || !initializer) return;
    CPPGMAstNodePtr expression = initializer;
    if(expression->kind == "initializer" || expression->kind == "paren-initializer" ||
       expression->kind == "default-argument" || expression->kind == "initializer-clause")
      expression = InitializerExpression(expression);
    if(!expression) return;
    if(type->kind == TYPE_ARRAY) {
      if(expression->kind == "braced-init-list")
        for(size_t i = 0; i < expression->children.size(); ++i)
          DemandConstantObjectConstructors(type->child, expression->children[i]);
      return;
    }
    if(type->kind != TYPE_CLASS) return;
    vector<CPPGMAstNodePtr> arguments;
    if(expression->kind == "braced-init-list") arguments = expression->children;
    else if(expression->kind == "call-expression" && expression->children.size() > 1) {
      CPPGMAstNodePtr list = expression->children[1];
      arguments = list ? list->children : vector<CPPGMAstNodePtr>();
      if(expression->value == "braced-construction" && arguments.size() == 1 &&
         arguments[0] && arguments[0]->kind == "braced-init-list")
        arguments = arguments[0]->children;
    } else return;
    const string constructor_name = type->template_specialization &&
      !type->template_primary.empty() ? LastComponent(type->template_primary) :
      LastComponent(type->name);
    vector<Binding*> candidates = MemberBindings(type, LastComponent(type->name));
    if(candidates.empty() && constructor_name != LastComponent(type->name))
      candidates = MemberBindings(type, constructor_name);
    for(size_t i = 0; i < candidates.size(); ++i) {
      Binding* binding = candidates[i];
      FunctionRecord* record = RecordForBinding(binding);
      if(!record || !record->constructor || record->deleted ||
         !record->source_type || record->source_type->parameters.size() != arguments.size())
        continue;
      record->needed = true;
      return;
    }
  }

void PA14Lowerer::StoreGlobalDeclaration(GlobalRecord& record,
                                         const TypePtr& record_value)
{
    const string key = global_key(record.qualified_name);
    map<string, GlobalRecord*>::iterator found = global_by_key_.find(key);
    GlobalRecord* stored = 0;
    if(found == global_by_key_.end()) {
      globals_.push_back(record);
      global_by_key_[key] = &globals_.back();
      stored = &globals_.back();
    } else {
      GlobalRecord* prior = found->second;
      if(record.initializer &&
         (!prior->explicit_specialization || record.explicit_specialization))
        prior->initializer = record.initializer;
      prior->type = record.type;
      if(record.object_name.size()) prior->object_name = record.object_name;
      if(record.template_owner) prior->template_owner = record.template_owner;
      prior->template_instantiation = prior->template_instantiation ||
			record.template_instantiation;
	      prior->explicit_specialization = prior->explicit_specialization ||
			record.explicit_specialization;
      prior->weak_binding = prior->weak_binding || record.weak_binding;
      prior->declaration = false;
      prior->internal = prior->internal || record.internal;
      prior->thread_local_storage = prior->thread_local_storage || record.thread_local_storage;
      prior->dynamic_initializer = prior->dynamic_initializer || record.dynamic_initializer;
      prior->dynamic_finalizer = prior->dynamic_finalizer || record.dynamic_finalizer;
      stored = prior;
    }
    if(stored && stored->thread_local_storage && stored->dynamic_initializer)
      EnsureThreadLocalGuard(stored);
    if(stored && record_value && record_value->kind == TYPE_CLASS)
      DemandTemplateStaticMembers(record_value);
  }

void PA14Lowerer::CollectClassStaticMember(const CPPGMAstNodePtr& child,
                                           const CPPGMAstNodePtr& item,
                                           const TypePtr& owner, Scope* class_scope,
                                           const Analyzer::SpecFacts& facts,
                                           const TypePtr& member_type,
                                           const string& name)
{
    if(!child || !item || !owner || !class_scope || !facts.is_static || name.empty()) return;
    const bool replayed_variable_member = child->template_instantiation &&
      child->template_primary.find("::") != string::npos;
    GlobalRecord record;
    record.node = child;
    record.scope = class_scope;
    record.type = member_type;
    record.qualified_name = TypeQualifiedName(owner) + "::" + name;
    record.template_owner = owner;
    record.template_instantiation = owner->template_specialization ||
      child->template_instantiation;
    record.explicit_specialization = child->explicit_specialization;
    record.weak_binding = record.template_instantiation;
    record.initializer = item->children.size() > 1 ? item->children[1] :
      CPPGMAstNodePtr();
    // A replayed variable-template member is itself a definition.  Its
    // initializer lives in the class specialization, so retaining the
    // declaration-only marker would discard both storage initialization and
    // the addressable definition from LowIR.
    record.declaration = !(replayed_variable_member && record.initializer);
    if(replayed_variable_member && record.initializer) {
      record.dynamic_initializer = true;
      needs_init_helper_ = true;
      if(facts.is_constexpr) {
        TypePtr value_member_type = type_value(member_type);
        if(value_member_type && value_member_type->kind == TYPE_CLASS)
          CollectImplicitConstructor(value_member_type,
            value_member_type->owned_scope, true);
        DemandConstantObjectConstructors(member_type, record.initializer);
      }
      const function<bool(const CPPGMAstNodePtr&, const string&)> mentions =
        [&](const CPPGMAstNodePtr& node, const string& target) {
          if(!node) return false;
          string value = node->value;
          const size_t marker = value.find(':');
          if(marker != string::npos && (marker + 1 >= value.size() ||
             value[marker + 1] != ':')) value.erase(0, marker + 1);
          const size_t scope = value.rfind("::");
          if(value == target || (scope != string::npos &&
             value.substr(scope + 2) == target)) return true;
          for(size_t nested = 0; nested < node->children.size(); ++nested)
            if(mentions(node->children[nested], target)) return true;
          return false;
        };
      for(size_t function = 0; function < functions_.size(); ++function)
        if(functions_[function].member_owner == owner &&
           functions_[function].static_member && functions_[function].template_instantiation) {
          const string source_name = LastComponent(functions_[function].qualified_name);
          const string generated_name = functions_[function].node &&
            functions_[function].node->children.size() > 1 ?
            FirstIdentifier(functions_[function].node->children[1]) : string();
          if(mentions(record.initializer, source_name) ||
             (!generated_name.empty() && mentions(record.initializer, generated_name)))
            functions_[function].needed = true;
        }
    }
    record.internal = false;
    record.thread_local_storage = HasStorageSpecifier(child, "thread_local");
    if(type_is_reference(member_type) && !record.initializer) {
      deferred_static_members_.insert(record.qualified_name);
      return;
    }
    const TypePtr member_value = type_value(member_type);
    const bool integral_constant = is_integral_type(member_value) ||
      (member_value && member_value->kind == TYPE_FUNDAMENTAL &&
       member_value->name == "bool");
    Binding* semantic_binding = class_scope->local(name);
    const bool initializer_calls = record.initializer &&
      DescendantOfKind(record.initializer, "call-expression");
    const bool typed_const = facts.is_const || facts.is_constexpr ||
      (member_type && member_type->is_const) ||
      (member_value && member_value->is_const);
    if(owner->template_specialization && typed_const && integral_constant) return;
    if(typed_const && integral_constant && !record.initializer) return;
    if((facts.is_const || facts.is_constexpr) && record.initializer &&
       integral_constant && ((semantic_binding && semantic_binding->has_value) ||
       !initializer_calls)) return;
    const string key = global_key(record.qualified_name);
    map<string, GlobalRecord*>::iterator global_found = global_by_key_.find(key);
    if(global_found == global_by_key_.end()) {
      globals_.push_back(record);
      global_by_key_[key] = &globals_.back();
      return;
    }
    GlobalRecord* prior = global_found->second;
    prior->type = record.type;
    if(record.template_owner) prior->template_owner = record.template_owner;
    prior->template_instantiation = prior->template_instantiation ||
      record.template_instantiation;
    prior->explicit_specialization = prior->explicit_specialization ||
      record.explicit_specialization;
    prior->weak_binding = prior->weak_binding || record.weak_binding;
    prior->thread_local_storage = prior->thread_local_storage ||
      record.thread_local_storage;
    if(record.initializer) prior->initializer = record.initializer;
  }
} // namespace cppgm_pa14_lowering
