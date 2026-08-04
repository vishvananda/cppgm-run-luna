#include "pa14_lowering.h"

#include <cstdlib>
#include <functional>
using namespace std;

namespace cppgm_pa14_lowering {

namespace {

string TypeNameValue(const CPPGMAstNodePtr& node)
{
    if(!node) return string();
    if(node->kind == "type-name") return StripTypeMarker(node->value);
    for(size_t i = 0; i < node->children.size(); ++i) {
      const string found = TypeNameValue(node->children[i]);
      if(!found.empty()) return found;
    }
    return string();
  }

string StripTemplateArgumentsForLookup(const string& raw)
{
    string result;
    int depth = 0;
    for(size_t i = 0; i < raw.size(); ++i) {
      if(raw[i] == '<') { ++depth; continue; }
      if(raw[i] == '>') { if(depth > 0) --depth; continue; }
      if(depth == 0) result += raw[i];
    }
    return result;
  }

bool FriendTypeMatches(const TypePtr& pattern, const TypePtr& actual)
{
    TypePtr left = type_value(pattern);
    TypePtr right = type_value(actual);
    if(!left || !right) return true;
    if(left->kind == TYPE_TEMPLATE_PARAMETER ||
       left->kind == TYPE_TEMPLATE_TEMPLATE_PARAMETER) return true;
    if(left->kind != right->kind) return false;
    if(left->kind == TYPE_FUNDAMENTAL) return left->name == right->name;
    if(left->kind == TYPE_CLASS) {
      if(left->template_specialization) {
        if(!right->template_specialization) return false;
        return last_component(left->template_primary) ==
            last_component(right->template_primary);
      }
      if(right->template_specialization)
        return last_component(left->name) ==
            last_component(right->template_primary);
      return last_component(left->name) == last_component(right->name);
    }
    if(left->kind == TYPE_FUNCTION) {
      if(left->parameters.size() != right->parameters.size() ||
         left->variadic != right->variadic ||
         left->function_const != right->function_const ||
         left->function_volatile != right->function_volatile ||
         left->function_lvalue_ref_qualified != right->function_lvalue_ref_qualified ||
         left->function_rvalue_ref_qualified != right->function_rvalue_ref_qualified)
        return false;
      if(!FriendTypeMatches(left->child, right->child)) return false;
      for(size_t i = 0; i < left->parameters.size(); ++i)
        if(!FriendTypeMatches(left->parameters[i], right->parameters[i])) return false;
      return true;
    }
    if(left->kind == TYPE_POINTER || left->kind == TYPE_LVALUE_REFERENCE ||
       left->kind == TYPE_RVALUE_REFERENCE || left->kind == TYPE_MEMBER_POINTER ||
       left->kind == TYPE_ARRAY)
      return FriendTypeMatches(left->child, right->child);
    return true;
  }

bool FriendFunctionMatches(const FriendAccess& access, const string& identity,
    const TypePtr& function_type)
{
    if(access.kind != FriendAccess::FRIEND_FUNCTION || identity.empty() ||
       access.name.empty()) return false;
    if(access.name != identity && last_component(access.name) != last_component(identity))
      return false;
    return access.target && function_type &&
      FriendTypeMatches(access.target, function_type);
  }

} // namespace

CPPGMAstNodePtr PA14Lowerer::MakeMemberCall(const CPPGMAstNodePtr& object,
                                             const string& name,
                                             const vector<CPPGMAstNodePtr>& arguments) const
{
    CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "OP_DOT:."));
    member->children.push_back(object);
    member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", name)));
    CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
    call->children.push_back(member);
    CPPGMAstNodePtr list(new CPPGMAstNode("argument-list"));
    for(size_t i = 0; i < arguments.size(); ++i) list->children.push_back(arguments[i]);
    call->children.push_back(list);
    return call;
  }

void PA14Lowerer::AppendBindings(Scope* scope, const string& name,
                      vector<Binding*>& result, set<Scope*>& visited) const
{
	if(!scope || !visited.insert(scope).second) return;
	for(size_t i = 0; i < scope->bindings.size(); ++i)
	  if(scope->bindings[i].name == name && !scope->bindings[i].hidden_friend)
	    result.push_back(&scope->bindings[i]);
    for(size_t i = 0; i < scope->using_directives.size(); ++i)
      AppendBindings(scope->using_directives[i], name, result, visited);
  }

vector<Binding*> PA14Lowerer::DirectBindings(Scope* scope, const string& name) const
{
    vector<Binding*> result;
    if(!scope) return result;
    for(size_t i = 0; i < scope->bindings.size(); ++i)
      if(scope->bindings[i].name == name) result.push_back(&scope->bindings[i]);
    return result;
  }

vector<Binding*> PA14Lowerer::LookupUnqualifiedAll(Scope* from, const string& name) const
{
	for(Scope* scope = from; scope != 0; scope = scope->parent) {
	  vector<Binding*> direct;
	  const vector<Binding*> all_direct = DirectBindings(scope, name);
	  for(size_t i = 0; i < all_direct.size(); ++i)
	    if(!all_direct[i]->hidden_friend) direct.push_back(all_direct[i]);
      if(!direct.empty()) return direct;
      vector<Binding*> imported;
      // Keep lookup paths from distinct using-directives separate.  The same
      // declaration reached through two directives is still ambiguous; a
      // single visited set here incorrectly erased that fact.
      for(size_t i = 0; i < scope->using_directives.size(); ++i) {
        set<Scope*> visited;
        AppendBindings(scope->using_directives[i], name, imported, visited);
      }
      if(!imported.empty()) return imported;
      if(scope->kind == SCOPE_CLASS && scope->owner_type) {
        vector<Binding*> inherited = MemberBindings(scope->owner_type, name);
        // Members of a dependent base are not visible to an ordinary
        // unqualified call during template definition lookup.  PA18 carries
        // that fact on the materialized class so namespace lookup can still
        // find a non-member declaration with the same spelling.
        if(!inherited.empty() && !scope->owner_type->dependent_base_lookup)
          return inherited;
      }
    }
    return vector<Binding*>();
  }

Scope* PA14Lowerer::ScopeComponent(Scope* current, const string& component,
                        bool first, bool absolute) const
{
    Scope* scope = (first && !absolute) ? analyzer_.FindNamespace(current, component) :
      analyzer_.FindNamespaceDirect(current, component);
    if(scope) return scope;
    vector<Binding*> bindings = (first && !absolute) ?
      LookupUnqualifiedAll(current, component) : DirectBindings(current, component);
    for(size_t i = 0; i < bindings.size(); ++i)
      if(bindings[i]->kind == BIND_TYPE || bindings[i]->kind == BIND_TYPE_ALIAS)
        return analyzer_.ScopeForType(bindings[i]->type);
    Analyzer::PathTarget target = analyzer_.ResolvePath(current, component);
    if(target.binding && (target.binding->kind == BIND_TYPE ||
                          target.binding->kind == BIND_TYPE_ALIAS))
      return analyzer_.ScopeForType(target.binding->type);
    return 0;
  }

vector<Binding*> PA14Lowerer::Lookup(const string& raw, Scope* from) const
{
    bool absolute = false;
    const vector<string> parts = analyzer_.SplitPath(
      StripTemplateArgumentsForLookup(raw), &absolute);
    if(parts.empty()) return vector<Binding*>();
    if(parts.size() == 1 && !absolute) {
      vector<Binding*> result = LookupUnqualifiedAll(from, parts[0]);
      if(result.empty()) {
        Analyzer::PathTarget target = analyzer_.ResolvePath(from, raw);
        if(target.binding && !target.binding->hidden_friend)
          result.push_back(target.binding);
      }
      if(result.empty() && state_ && state_->record &&
         state_->record->member_owner)
        result = MemberBindings(state_->record->member_owner, parts[0]);
      return result;
    }
    Scope* current = absolute ? analyzer_.global_.get() : from;
    for(size_t i = 0; i + 1 < parts.size(); ++i) {
      current = ScopeComponent(current, parts[i], i == 0, absolute);
      if(!current) break;
    }
    if(current && current->kind == SCOPE_CLASS && current->owner_type)
      return MemberBindings(current->owner_type, parts.back());
    vector<Binding*> result;
    set<Scope*> visited;
    AppendBindings(current, parts.back(), result, visited);
    if(result.empty()) {
      Analyzer::PathTarget target = analyzer_.ResolvePath(from, raw);
      if(target.binding) result.push_back(target.binding);
      else if(target.scope) AppendBindings(target.scope, parts.back(), result, visited);
    }
	if(result.empty() && parts.size() > 1) {
		// Generated specializations can retain a source-relative namespace
		// spelling after PA18 materialization.  Recover only a unique typed
		// binding whose semantic qualified path has that suffix; ordinary
		// overload lookup remains governed by the lexical path above.
		string suffix = raw;
		while(suffix.compare(0, 2, "::") == 0) suffix.erase(0, 2);
		Binding* match = 0;
		bool ambiguous = false;
		function<void(Scope*)> find_suffix;
		find_suffix = [&](Scope* scope) {
			if(!scope || ambiguous) return;
			for(size_t binding = 0; binding < scope->bindings.size(); ++binding) {
				Binding* candidate = &scope->bindings[binding];
				if(candidate->hidden_friend || (candidate->kind != BIND_TYPE &&
					candidate->kind != BIND_TYPE_ALIAS) || !candidate->type) continue;
				const string& qualified = candidate->qualified_name;
				if(qualified != suffix && (qualified.size() <= suffix.size() ||
					qualified.compare(qualified.size() - suffix.size(), suffix.size(), suffix) != 0 ||
					qualified[qualified.size() - suffix.size() - 1] != ':')) continue;
				if(match && match != candidate) { ambiguous = true; return; }
				match = candidate;
			}
			for(size_t child = 0; child < scope->children.size(); ++child)
				find_suffix(scope->children[child].get());
		};
		find_suffix(analyzer_.global_.get());
		if(match && !ambiguous) result.push_back(match);
	}
    return result;
}

Scope* PA14Lowerer::FindTypeOwnerScope(Scope* scope, const TypePtr& type) const
{
    if(!scope || !type) return 0;
    for(size_t i = 0; i < scope->bindings.size(); ++i)
      if((scope->bindings[i].kind == BIND_TYPE ||
          scope->bindings[i].kind == BIND_TYPE_ALIAS) &&
         scope->bindings[i].type == type) return scope;
    for(size_t i = 0; i < scope->children.size(); ++i) {
      Scope* found = FindTypeOwnerScope(scope->children[i].get(), type);
      if(found) return found;
    }
    return 0;
  }

void PA14Lowerer::AppendAssociatedOperatorBindings(const TypePtr& raw_type,
                                                    const string& name,
                                                    vector<Binding*>& result,
                                                    set<const Type*>& visited_types,
                                                    set<Scope*>& visited_scopes) const
{
    TypePtr type = raw_type;
    if(!type) return;
    if(type->kind == TYPE_LVALUE_REFERENCE || type->kind == TYPE_RVALUE_REFERENCE ||
       type->kind == TYPE_POINTER || type->kind == TYPE_ARRAY) {
      AppendAssociatedOperatorBindings(type->child, name, result,
        visited_types, visited_scopes);
      return;
    }
    if(type->kind != TYPE_CLASS && type->kind != TYPE_ENUM) return;
    if(!visited_types.insert(type.get()).second) return;

    Scope* owner_scope = type->owned_scope;
    if(!owner_scope) owner_scope = FindTypeOwnerScope(analyzer_.global_.get(), type);
    if(type->kind == TYPE_CLASS && owner_scope) {
      const vector<Binding*> hidden = DirectBindings(owner_scope, last_component(name));
      for(size_t i = 0; i < hidden.size(); ++i)
        if(hidden[i]->kind == BIND_FUNCTION && hidden[i]->hidden_friend &&
           find(result.begin(), result.end(), hidden[i]) == result.end())
          result.push_back(hidden[i]);
      if(!hidden_friend_binding_index_ready_) {
        vector<Scope*> pending;
        if(analyzer_.global_) pending.push_back(analyzer_.global_.get());
        for(size_t next = 0; next < pending.size(); ++next) {
          Scope* current = pending[next];
          for(size_t i = 0; i < current->bindings.size(); ++i) {
            Binding* binding = &current->bindings[i];
            const TypePtr friend_owner = type_value(binding->friend_owner);
            if(binding->kind != BIND_FUNCTION || !binding->hidden_friend ||
               !friend_owner) continue;
            hidden_friend_binding_index_[friend_owner.get()].push_back(binding);
          }
          for(size_t i = 0; i < current->children.size(); ++i)
            pending.push_back(current->children[i].get());
        }
        hidden_friend_binding_index_ready_ = true;
      }
      const string generated_prefix = last_component(name) + "__inst_";
      const TypePtr owner = type_value(type);
      map<const Type*, vector<Binding*> >::const_iterator indexed =
        hidden_friend_binding_index_.find(owner.get());
      if(indexed != hidden_friend_binding_index_.end())
        for(size_t i = 0; i < indexed->second.size(); ++i) {
          Binding* binding = indexed->second[i];
          if(!binding || binding->name.compare(0, generated_prefix.size(),
              generated_prefix) != 0 || find(result.begin(), result.end(), binding) !=
              result.end()) continue;
          result.push_back(binding);
        }
      AppendAssociatedOperatorBindings(type->direct_base, name, result,
        visited_types, visited_scopes);
    }
    Scope* associated = owner_scope;
    while(associated && associated->kind != SCOPE_NAMESPACE)
      associated = associated->parent;
    if(associated && visited_scopes.insert(associated).second) {
      const vector<Binding*> namespace_bindings = DirectBindings(associated, last_component(name));
      const string expected_name = associated->qualified_prefix.empty() ?
        last_component(name) : associated->qualified_prefix + "::" + last_component(name);
      const bool operator_lookup = name.compare(0, 8, "operator") == 0;
      for(size_t i = 0; i < namespace_bindings.size(); ++i)
        if(namespace_bindings[i]->kind == BIND_FUNCTION &&
           !namespace_bindings[i]->is_member && !namespace_bindings[i]->hidden_friend &&
           (namespace_bindings[i]->qualified_name == expected_name ||
            operator_lookup) &&
           find(result.begin(), result.end(), namespace_bindings[i]) == result.end())
          result.push_back(namespace_bindings[i]);
    }
  }

vector<Binding*> PA14Lowerer::OperatorCandidates(const string& name,
                                                  const vector<ExprInfo>& arguments,
                                                  Scope* scope) const
{
    vector<Binding*> result;
    const vector<Binding*> ordinary = Lookup(name, scope);
    for(size_t i = 0; i < ordinary.size(); ++i)
      if(ordinary[i]->kind == BIND_FUNCTION && !ordinary[i]->is_member &&
         !ordinary[i]->hidden_friend &&
         find(result.begin(), result.end(), ordinary[i]) == result.end())
        result.push_back(ordinary[i]);
    set<const Type*> visited_types;
    set<Scope*> visited_scopes;
    for(size_t i = 0; i < arguments.size(); ++i)
      AppendAssociatedOperatorBindings(expression_value_type(arguments[i]), name,
        result, visited_types, visited_scopes);
    return result;
  }

bool PA14Lowerer::IsAccessible(Binding* binding, Scope* scope) const
{
    if(!binding || !binding->is_member || binding->access.empty() ||
       binding->access == "public") return true;
    TypePtr owner = type_value(binding->member_owner);
    if(!owner) return false;
    const FunctionRecord* function_record = state_ && state_->record ?
      state_->record : 0;
    const string function_identity = function_record &&
      !function_record->template_primary.empty() ? function_record->template_primary :
      function_record ? function_record->qualified_name : string();
    const TypePtr function_type = function_record ? function_record->source_type : TypePtr();
    for(size_t i = 0; i < owner->friend_access.size(); ++i) {
      const FriendAccess& access = owner->friend_access[i];
      if(FriendFunctionMatches(access, function_identity, function_type))
        return true;
    }
    TypePtr context;
    if(state_ && state_->record && state_->record->member && state_->record->member_owner)
      context = type_value(state_->record->member_owner);
    if(!context) for(Scope* current = scope; current; current = current->parent)
      if(current->kind == SCOPE_CLASS && current->owner_type) {
        context = current->owner_type;
        break;
      }
    if(!context) return false;
    map<const Type*, vector<TypePtr> >::const_iterator indexed_friends =
      friend_owner_index_.find(owner.get());
    if(indexed_friends != friend_owner_index_.end()) for(size_t owner_index = 0;
      owner_index < indexed_friends->second.size(); ++owner_index) {
      const TypePtr& friend_owner = indexed_friends->second[owner_index];
      bool friend_match = false;
      for(size_t i = 0; i < friend_owner->friend_access.size(); ++i) {
        const FriendAccess& access = friend_owner->friend_access[i];
        const TypePtr friend_type = type_value(access.target);
        const bool friend_class = access.kind == FriendAccess::FRIEND_CLASS &&
          friend_type && FriendTypeMatches(friend_type, context);
        if(friend_class || FriendFunctionMatches(access, function_identity, function_type)) {
          friend_match = true;
          break;
        }
      }
      if(friend_match && (friend_owner == owner || IsDerivedFrom(friend_owner, owner)))
        return true;
    }
    // PA18 materializes each class specialization as a distinct Type object.
    // Its generated declaration may not retain a dependent friend edge even
    // though the primary class's typed state does.  Match that primary class
    // by template identity and reuse the recorded friend relation; this keeps
    // access checking semantic and still requires the friend target to match
    // the current member owner.
    const auto same_template_class = [](const TypePtr& left, const TypePtr& right) {
      if(!left || !right || left->kind != TYPE_CLASS || right->kind != TYPE_CLASS)
        return false;
      const string left_primary = left->template_primary.empty() ? left->name :
        left->template_primary;
      const string right_primary = right->template_primary.empty() ? right->name :
        right->template_primary;
      return last_component(left_primary) == last_component(right_primary);
    };
    for(map<const Type*, vector<TypePtr> >::const_iterator indexed =
        friend_owner_index_.begin(); indexed != friend_owner_index_.end(); ++indexed)
      for(size_t owner_index = 0; owner_index < indexed->second.size(); ++owner_index) {
        const TypePtr& friend_owner = indexed->second[owner_index];
        if(!same_template_class(friend_owner, owner)) continue;
        bool friend_match = false;
        for(size_t i = 0; i < friend_owner->friend_access.size(); ++i) {
          const FriendAccess& access = friend_owner->friend_access[i];
          if(access.kind == FriendAccess::FRIEND_CLASS && access.target &&
             FriendTypeMatches(access.target, context)) {
            friend_match = true;
            break;
          }
        }
        if(friend_match) return true;
      }
    for(TypePtr current = context; current; current = type_value(current->enclosing_type))
      if(current == owner) return true;
    if(binding->access == "private") return false;
    for(TypePtr enclosing = context; enclosing;
        enclosing = type_value(enclosing->enclosing_type))
      for(TypePtr current = enclosing; current;
          current = type_value(current->direct_base))
        if(current == owner) return true;
    return false;
  }

void PA14Lowerer::CheckTypeAccess(const CPPGMAstNodePtr& declaration,
                                  Scope* scope) const
{
    if(!declaration) return;
    for(size_t i = 0; i < declaration->children.size(); ++i) {
      CPPGMAstNodePtr child = declaration->children[i];
      if(!child || child->kind != "decl-specifier") continue;
      const string raw = StripTypeMarker(child->value);
      if(raw.find("::") == string::npos) continue;
      Analyzer::PathTarget target = analyzer_.ResolvePath(scope, raw);
      if(target.binding && !IsAccessible(target.binding, scope))
        throw logic_error("inaccessible type member");
    }
  }

Binding* PA14Lowerer::MemberBinding(const CPPGMAstNodePtr& node, Scope* scope,
                                    ExprInfo* object_info)
{
    if(!node || node->kind != "member-expression" || node->children.size() < 2)
      return 0;
    ExprInfo local_object = Infer(node->children[0], scope);
    if(object_info) *object_info = local_object;
    TypePtr object = expression_value_type(local_object);
    const string op = PA12Operator(node->value);
    if(op == "->") {
      if(!object || object->kind != TYPE_POINTER) return 0;
      object = type_value(object->child);
    }
    vector<Binding*> candidates = MemberBindings(object, node->children[1]->value);
    if(candidates.empty() && node->children[1] &&
       node->children[1]->value.compare(0, 8, "operator") == 0) {
      Binding* conversion = FindNamedConversionOperator(object,
        node->children[1]->value, scope);
      if(conversion) candidates.push_back(conversion);
    }
	if(candidates.empty()) {
		return 0;
    }
    Binding* selected = 0;
    for(size_t i = 0; i < candidates.size(); ++i) {
      if(candidates[i]->kind != BIND_FUNCTION) return candidates[i];
      if(!selected) selected = candidates[i];
    }
    return selected;
  }

TypePtr PA14Lowerer::expression_value_type(const ExprInfo& info) const
{
    return type_value(info.type);
  }

TypePtr PA14Lowerer::function_target_type(const TypePtr& type) const
{
    TypePtr value = type_value(type);
    if(value && value->kind == TYPE_FUNCTION) return value;
    if(value && value->kind == TYPE_POINTER && value->child &&
       value->child->kind == TYPE_FUNCTION) return value->child;
    return TypePtr();
  }

PA14Lowerer::ExprInfo PA14Lowerer::InferLiteral(const CPPGMAstNodePtr& node,
                                                const TypePtr& expected,
                                                Scope* scope)
{
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
      const CallChoice choice = ChooseCall(call, scope);
      if(!choice.binding || !choice.function)
        throw logic_error("no viable user-defined string literal operator");
      ExprInfo result;
      result.type = choice.function->child;
      result.category = type_is_reference(result.type) ? "lvalue" : "prvalue";
      result.binding = choice.binding;
      result.candidates.push_back(choice.binding);
      return result;
    }
    ExprInfo result;
    long long value = 0;
    bool known = false;
    result.operand = canonical_literal(node->value, &result.type, &value, &known);
    result.category = result.type && result.type->kind == TYPE_ARRAY ? "lvalue" : "prvalue";
    result.null_pointer_constant = known && value == 0 && is_integral_type(result.type);
    result.known_constant = known;
    result.constant = value;
    if(expected && result.null_pointer_constant &&
       (type_value(expected)->kind == TYPE_POINTER ||
        (type_value(expected)->kind == TYPE_FUNDAMENTAL &&
         type_value(expected)->name == "nullptr_t"))) {
      result.type = type_value(expected)->kind == TYPE_FUNDAMENTAL &&
        type_value(expected)->name == "nullptr_t" ? Fundamental("nullptr_t") : expected;
    }
    return result;
  }

PA14Lowerer::ExprInfo PA14Lowerer::InferKeyword(const CPPGMAstNodePtr& node) const
{
    ExprInfo result;
    const string op = PA12Operator(node->value);
    if(op == "nullptr") {
      result.type = Fundamental("nullptr_t");
      result.operand = "nullptr";
    } else if(op == "this") {
      VariablePlan* local = FindLocalPlan("this");
      if(!local) {
        ostringstream diagnostic;
        diagnostic << "this used outside a member function: " <<
          (state_ && state_->record ? state_->record->qualified_name : string("<none>"));
        if(state_ && state_->record) diagnostic << " member=" << state_->record->member <<
          " static=" << state_->record->static_member;
        throw logic_error(diagnostic.str());
      }
      result.type = local->type;
      result.category = "prvalue";
    } else {
      result.type = Fundamental("bool");
      result.operand = op == "true" ? "1" : "0";
      result.known_constant = true;
      result.constant = op == "true" ? 1 : 0;
    }
    return result;
}

Binding* PA14Lowerer::ResolveDecltypeStaticMember(const string& spelling,
                                                  Scope* scope) const
{
    const bool decltype_form = spelling.compare(0, 9, "decltype(") == 0;
    const size_t separator = spelling.rfind("::");
    if(separator == string::npos || separator == 0 || separator + 2 >= spelling.size()) return 0;
    const size_t begin = decltype_form ? 9 : 0;
    if(separator <= begin) return 0;
    string type_expression = spelling.substr(begin, separator - begin);
    if(decltype_form) {
      if(!type_expression.empty() && type_expression[type_expression.size() - 1] == ')')
          type_expression.erase(type_expression.size() - 1);
      if(type_expression.size() >= 2 &&
         type_expression.compare(type_expression.size() - 2, 2, "()") == 0)
          type_expression.erase(type_expression.size() - 2);
    }
    Binding* type_binding = analyzer_.ResolveBinding(scope, type_expression);
    if(!type_binding || (type_binding->kind != BIND_TYPE &&
                         type_binding->kind != BIND_TYPE_ALIAS)) return 0;
    const vector<Binding*> members = MemberBindings(type_binding->type,
      spelling.substr(separator + 2));
    for(size_t i = 0; i < members.size(); ++i)
        if(members[i]->kind == BIND_VARIABLE && members[i]->is_static)
            return members[i];
    return 0;
}

PA14Lowerer::VariablePlan* PA14Lowerer::FindLocalPlan(const string& name) const
{
    if(!state_) return 0;
    for(vector<map<string, VariablePlan*> >::const_reverse_iterator env =
          state_->environments.rbegin(); env != state_->environments.rend(); ++env) {
      map<string, VariablePlan*>::const_iterator found = env->find(name);
      if(found != env->end()) return found->second;
    }
    return 0;
  }

PA14Lowerer::ExprInfo PA14Lowerer::InferIdentifier(const CPPGMAstNodePtr& node, Scope* scope,
                           const TypePtr& expected) const
{
    if(node && !node->value.empty() &&
       (isdigit(static_cast<unsigned char>(node->value[0])) ||
        ((node->value[0] == '-' || node->value[0] == '+') && node->value.size() > 1 &&
         isdigit(static_cast<unsigned char>(node->value[1]))))) {
      TypePtr literal_type;
      long long literal_value = 0;
      bool literal_known = false;
      const string literal = canonical_literal(node->value, &literal_type,
        &literal_value, &literal_known);
      if(literal_known) {
        ExprInfo result;
        result.type = literal_type;
        result.operand = literal;
        result.category = "prvalue";
        result.known_constant = true;
        result.constant = literal_value;
        return result;
      }
    }
    ExprInfo result;
    VariablePlan* local = FindLocalPlan(node->value);
    if(local) {
      result.type = type_is_reference(local->type) ? local->type->child : local->type;
      result.category = "lvalue";
      result.binding = 0;
      InferLocalIdentifierConstant(result.type, &result);
      return result;
    }
    Binding* decltype_member = ResolveDecltypeStaticMember(node->value, scope);
    if(decltype_member) {
      result.binding = decltype_member;
      result.type = PA12AdjustedType(decltype_member->type);
      if(type_is_reference(result.type)) result.type = result.type->child;
      result.category = "lvalue";
      const TypePtr constant_type = type_value(result.type);
      const bool integral_constant = is_integral_type(result.type) ||
        (constant_type && constant_type->kind == TYPE_FUNDAMENTAL &&
         constant_type->name == "bool");
      if(decltype_member->is_static && decltype_member->has_value &&
         integral_constant) {
        result.known_constant = true;
        result.constant = decltype_member->value;
        result.operand = integer_text(result.constant);
        result.category = "prvalue";
      }
      return result;
    }
    result.candidates = Lookup(node->value, scope);
    if(result.candidates.size() > 1) {
      bool repeated_binding = true;
      for(size_t i = 1; i < result.candidates.size(); ++i)
        if(result.candidates[i] != result.candidates[0]) {
          repeated_binding = false;
          break;
        }
      if(repeated_binding) throw logic_error("ambiguous expression name: " + node->value);
    }
    if(expected && !result.candidates.empty()) {
      TypePtr target = type_value(expected);
      Binding* selected = 0;
      int best = 1000000;
      for(size_t i = 0; i < result.candidates.size(); ++i) {
        TypePtr candidate = function_target_type(result.candidates[i]->type);
        if(!candidate) continue;
        ExprInfo source;
        source.type = candidate;
        source.category = "lvalue";
        const int rank = ConversionRank(source, target);
        if(rank >= 0 && rank < best) { best = rank; selected = result.candidates[i]; }
        else if(rank >= 0 && rank == best) throw logic_error("ambiguous function target");
      }
      if(selected) result.binding = selected;
    }
    if(result.binding) result.candidates.clear();
	if(!result.binding && result.candidates.empty()) throw logic_error("unknown expression name: " + node->value);
    if(!result.binding && result.candidates.size() == 1)
      result.binding = result.candidates[0];
    if(result.binding && !IsAccessible(result.binding, scope))
      throw logic_error("inaccessible member");
    if(result.binding && result.binding->kind == BIND_ENUMERATOR) {
      result.type = result.binding->type;
      result.category = "prvalue";
      result.known_constant = result.binding->has_value;
      result.constant = result.binding->value;
      result.operand = integer_text(result.constant);
      return result;
    }
    if(result.binding) {
      result.type = PA12AdjustedType(result.binding->type);
      if(type_is_reference(result.type)) result.type = result.type->child;
      VariablePlan* this_plan = FindLocalPlan("this");
      TypePtr this_type = this_plan ? type_value(this_plan->type) : TypePtr();
      if(this_type && this_type->kind == TYPE_POINTER) this_type = type_value(this_type->child);
      if(result.binding->is_member && !result.binding->is_static &&
         result.binding->kind != BIND_FUNCTION && this_type && this_type->is_const &&
         result.binding->member_owner &&
         result.binding->member_index != static_cast<size_t>(-1) &&
         result.binding->member_index < result.binding->member_owner->class_members.size() &&
         !result.binding->member_owner->class_members[result.binding->member_index].is_mutable)
        result.type = CloneWithCv(result.type, true, result.type->is_volatile);
      result.category = result.type && result.type->kind == TYPE_FUNCTION ? "lvalue" : "lvalue";
		const TypePtr constant_type = type_value(result.type);
		const bool integral_constant = is_integral_type(result.type) ||
			(constant_type && constant_type->kind == TYPE_FUNDAMENTAL &&
				constant_type->name == "bool");
		if(result.binding->is_member && result.binding->is_static &&
			 result.binding->has_value && integral_constant) {
        result.known_constant = true;
        result.constant = result.binding->value;
        result.operand = integer_text(result.constant);
        result.category = "prvalue";
      }
      return result;
    }
    result.type = function_target_type(result.candidates[0]->type);
    if(!result.type) result.type = result.candidates[0]->type;
    result.category = "lvalue";
    return result;
  }

PA14Lowerer::ExprInfo PA14Lowerer::InferMember(const CPPGMAstNodePtr& node,
                                                Scope* scope) const
{
    ExprInfo result;
    if(!node || node->children.size() < 2) throw logic_error("invalid member expression");
    ExprInfo object_info = const_cast<PA14Lowerer*>(this)->Infer(node->children[0], scope);
    TypePtr object = expression_value_type(object_info);
    const string op = PA12Operator(node->value);
    if(op == "->") {
      if(!object || object->kind != TYPE_POINTER)
        throw logic_error("arrow requires a pointer to class");
      object = type_value(object->child);
    }
    if(node->children[1] && !node->children[1]->value.empty() &&
       node->children[1]->value[0] == '~' &&
       (!object || object->kind != TYPE_CLASS)) {
      result.type = Fundamental("void");
      result.category = "prvalue";
      return result;
    }
	vector<Binding*> candidates = MemberBindings(object, node->children[1]->value);
	if(candidates.empty()) {
		throw logic_error("unknown member");
    }
    result.candidates = candidates;
    Binding* selected = 0;
    for(size_t i = 0; i < candidates.size(); ++i) {
      if(candidates[i]->kind != BIND_FUNCTION) { selected = candidates[i]; break; }
      if(!selected) selected = candidates[i];
    }
    result.binding = selected;
    const bool reference_member = selected && selected->kind != BIND_FUNCTION &&
      type_is_reference(selected->type);
    if(selected && !IsAccessible(selected, scope))
      throw logic_error("inaccessible member");
    if(selected && selected->kind == BIND_ENUMERATOR) {
      result.type = selected->type;
      result.known_constant = selected->has_value;
      result.constant = selected->value;
      result.operand = integer_text(selected->value);
      result.category = "prvalue";
      return result;
    }
    result.type = selected ? PA12AdjustedType(selected->type) : Fundamental("int");
    if(type_is_reference(result.type)) result.type = result.type->child;
    if(selected && selected->kind != BIND_FUNCTION && !reference_member &&
       object && object->is_const &&
       !selected->is_static && selected->member_owner &&
       selected->member_index != static_cast<size_t>(-1) &&
       selected->member_index < selected->member_owner->class_members.size() &&
       !selected->member_owner->class_members[selected->member_index].is_mutable)
      result.type = CloneWithCv(result.type, true, result.type->is_volatile);
    result.category = "lvalue";
    return result;
  }


TypePtr PA14Lowerer::CommonType(const TypePtr& left, const TypePtr& right,
                    const string& op) const
{
    TypePtr l = type_value(left);
    TypePtr r = type_value(right);
    if(!l || !r) return Fundamental("int");
    if(PA12SameType(l, r, true)) {
      if(l->kind == TYPE_FUNDAMENTAL && r->kind == TYPE_FUNDAMENTAL &&
         is_arithmetic_type(l) && is_arithmetic_type(r) &&
         !is_floating_type(l) && !is_floating_type(r)) {
        TypePtr promoted = IntegralPromotion(l);
        TypePtr right_promoted = IntegralPromotion(r);
        if(promoted && right_promoted && promoted->name == right_promoted->name)
          return promoted;
      }
      if(l->kind == TYPE_POINTER && (l->child->is_const || r->child->is_const)) {
        TypePtr result(new Type(*l));
        result->child = CloneWithCv(l->child, l->child->is_const || r->child->is_const,
          l->child->is_volatile || r->child->is_volatile);
        return result;
      }
      if(l->is_const || l->is_volatile) {
        TypePtr result(new Type(*l));
        result->is_const = false;
        result->is_volatile = false;
        return result;
      }
      return l;
    }
    if(l->kind == TYPE_POINTER && r->kind == TYPE_POINTER) {
      if(PointerCompatible(l, r)) return r;
      if(PointerCompatible(r, l)) return l;
    }
    if(l->kind == TYPE_POINTER && r->kind == TYPE_FUNDAMENTAL && r->name == "nullptr_t") return l;
    if(r->kind == TYPE_POINTER && l->kind == TYPE_FUNDAMENTAL && l->name == "nullptr_t") return r;
    if(is_arithmetic_type(l) && is_arithmetic_type(r)) {
      if(l->name == "long double" || r->name == "long double") return Fundamental("long double");
      if(l->name == "double" || r->name == "double") return Fundamental("double");
      if(l->name == "float" || r->name == "float") return Fundamental("float");
      TypePtr lp = IntegralPromotion(l), rp = IntegralPromotion(r);
      if(lp && rp && lp->name == "unsigned int" && rp->name == "int") return lp;
      if(lp && rp && rp->name == "unsigned int" && lp->name == "int") return rp;
      if(lp && rp && (lp->name == "long int" || rp->name == "long int" ||
          lp->name == "unsigned long int" || rp->name == "unsigned long int"))
        return Fundamental((lp->name.find("unsigned") != string::npos ||
          rp->name.find("unsigned") != string::npos) ? "unsigned long int" : "long int");
      if(lp && rp) {
        if(type_size(lp) > type_size(rp)) return lp;
        if(type_size(rp) > type_size(lp)) return rp;
        if(lp->name.find("unsigned") != string::npos) return lp;
        if(rp->name.find("unsigned") != string::npos) return rp;
      }
      return Fundamental("int");
    }
    (void)op;
    return l;
  }

string PA14Lowerer::OperatorFunctionName(const string& raw) const
{
    const string op = PA12Operator(raw);
    if(op == "bitand") return "operator&";
    if(op == "bitor") return "operator|";
    if(op == "xor") return "operator^";
    if(op == "and") return "operator&&";
    if(op == "or") return "operator||";
    if(op == "not_eq") return "operator!=";
    return "operator" + op;
  }

PA14Lowerer::ExprInfo PA14Lowerer::InferCall(const CPPGMAstNodePtr& node, Scope* scope)
{
    ExprInfo result;
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
        result.type = Fundamental("void");
        result.category = "prvalue";
        return result;
      }
    }
    TypePtr constructor_type = ConstructorObjectType(
      node && !node->children.empty() ? node->children[0] : CPPGMAstNodePtr(), scope);
    if(constructor_type) {
      result.type = constructor_type;
      result.category = "prvalue";
      return result;
    }
    TypePtr builtin_type = BuiltinCastType(
      node && !node->children.empty() ? node->children[0] : CPPGMAstNodePtr(), scope);
    if(builtin_type) {
      result.type = builtin_type;
      result.category = "prvalue";
      return result;
    }
    CallChoice choice = ChooseCall(node, scope);
    result.type = choice.function->child;
    if(result.type && result.type->kind == TYPE_LVALUE_REFERENCE) result.category = "lvalue";
    else if(result.type && result.type->kind == TYPE_RVALUE_REFERENCE) result.category = "xvalue";
    else result.category = "prvalue";
    result.binding = choice.binding;
    return result;
  }

TypePtr PA14Lowerer::ConstructorObjectType(const CPPGMAstNodePtr& callee,
                                            Scope* scope) const
{
    CPPGMAstNodePtr effective = callee;
    while(effective && effective->kind == "parenthesized-expression" &&
          !effective->children.empty()) effective = effective->children[0];
    if(!effective || effective->kind != "id-expression") return TypePtr();
    const vector<Binding*> candidates = Lookup(effective->value, scope);
    for(size_t i = 0; i < candidates.size(); ++i) {
      if(candidates[i]->kind != BIND_TYPE && candidates[i]->kind != BIND_TYPE_ALIAS)
        continue;
      TypePtr type = type_value(candidates[i]->type);
      if(type && (type->kind == TYPE_CLASS || type->kind == TYPE_ARRAY)) {
        const size_t open = effective->value.find('<');
        if(type->kind == TYPE_CLASS && open != string::npos &&
           !effective->value.empty() && effective->value[effective->value.size() - 1] == '>') {
          TypePtr specialization(new Type(*type));
          specialization->name = effective->value;
          specialization->template_specialization = true;
          specialization->template_primary = type->template_primary.empty() ?
            type->name : type->template_primary;
          specialization->template_arguments.clear();
          string argument;
          int depth = 0;
          for(size_t p = open + 1; p + 1 < effective->value.size(); ++p) {
            const char ch = effective->value[p];
            if(ch == '<') ++depth;
            else if(ch == '>' && depth > 0) --depth;
            if(ch == ',' && depth == 0) {
              specialization->template_arguments.push_back(trim_type_name(argument));
              argument.clear();
            } else argument += ch;
          }
          if(!argument.empty())
            specialization->template_arguments.push_back(trim_type_name(argument));
          return specialization;
        }
        return type;
      }
    }
    // A generated specialization may install its constructor binding under
    // the same spelling as the class while the type binding is still
    // represented by the enclosing member owner.  Recover the typed class
    // fact from the active function instead of treating a class-valued
    // construction as an ordinary member call (which would invent `this` in
    // a static member body).
    if(state_ && state_->record) {
      TypePtr owner = type_value(state_->record->member_owner);
      if(owner && owner->kind == TYPE_CLASS &&
         LastComponent(owner->name) == effective->value) return owner;
      TypePtr function = function_target_type(state_->record->source_type);
      if(function && function->child && function->child->kind == TYPE_CLASS &&
         LastComponent(function->child->name) == effective->value)
        return function->child;
    }
    return TypePtr();
}

TypePtr PA14Lowerer::BuiltinCastType(const CPPGMAstNodePtr& callee,
                                      Scope* scope) const
{
    CPPGMAstNodePtr effective = callee;
    while(effective && effective->kind == "parenthesized-expression" &&
          !effective->children.empty()) effective = effective->children[0];
    if(!effective || effective->kind != "id-expression" ||
       effective->value.find("::") != string::npos) return TypePtr();
    const string name = effective->value;
    if(name == "unsigned") return Fundamental("unsigned int");
    if(name == "bool" || name == "char" || name == "signed char" ||
       name == "unsigned char" || name == "short" || name == "short int" ||
       name == "unsigned short" || name == "unsigned short int" ||
       name == "int" || name == "unsigned" || name == "unsigned int" ||
       name == "long" || name == "long int" || name == "unsigned long" ||
       name == "unsigned long int" || name == "long long" ||
       name == "long long int" || name == "unsigned long long" ||
       name == "unsigned long long int" || name == "float" ||
       name == "double" || name == "long double" || name == "void" ||
       name == "nullptr_t") return Fundamental(name);
    string pointer_base = name;
    size_t pointer_depth = 0;
    while(!pointer_base.empty() && pointer_base[pointer_base.size() - 1] == '*') {
      pointer_base.erase(pointer_base.size() - 1);
      ++pointer_depth;
    }
    if(pointer_depth && (pointer_base == "void" || pointer_base == "bool" ||
        pointer_base == "char" || pointer_base == "signed char" ||
        pointer_base == "unsigned char" || pointer_base == "short" ||
        pointer_base == "short int" || pointer_base == "unsigned short" ||
        pointer_base == "unsigned short int" || pointer_base == "int" ||
        pointer_base == "unsigned" || pointer_base == "unsigned int" ||
        pointer_base == "long" || pointer_base == "long int" ||
        pointer_base == "unsigned long" || pointer_base == "unsigned long int" ||
        pointer_base == "long long" || pointer_base == "long long int" ||
        pointer_base == "unsigned long long" ||
        pointer_base == "unsigned long long int" || pointer_base == "float" ||
        pointer_base == "double" || pointer_base == "long double" ||
        pointer_base == "nullptr_t")) {
      TypePtr result = Fundamental(pointer_base);
      while(pointer_depth--) result = PointerTo(result);
      return result;
    }
    Analyzer::PathTarget target = analyzer_.ResolvePath(scope, name);
    if(!target.binding || (target.binding->kind != BIND_TYPE &&
                           target.binding->kind != BIND_TYPE_ALIAS)) return TypePtr();
    TypePtr alias = target.binding->type;
    return alias && type_value(alias) && type_value(alias)->kind != TYPE_CLASS &&
      type_value(alias)->kind != TYPE_ARRAY ? alias : TypePtr();
  }

PA14Lowerer::ExprInfo PA14Lowerer::InferUnary(const CPPGMAstNodePtr& node, Scope* scope)
{
    ExprInfo result;
    const string op = PA12Operator(node->value);
    ExprInfo child = Infer(node->children[0], scope);
    vector<CPPGMAstNodePtr> operator_arguments;
    operator_arguments.push_back(node->children[0]);
    CallChoice overloaded = ChooseOperatorCall(OperatorFunctionName(op),
      operator_arguments, scope);
    if(overloaded.binding) {
      result.type = overloaded.function->child;
      result.category = type_is_reference(result.type) ?
        (result.type->kind == TYPE_LVALUE_REFERENCE ? "lvalue" : "xvalue") : "prvalue";
      result.binding = overloaded.binding;
      return result;
    }
    TypePtr value = expression_value_type(child);
    if(value && value->kind == TYPE_CLASS && op != "&" && op != "*") {
      Binding* conversion = FindContextConversionOperator(value, op == "!", op == "!");
      if(conversion) {
        TypePtr function = function_target_type(conversion->type);
        result.type = function ? function->child : Fundamental("int");
        result.category = "prvalue";
        return result;
      }
    }
    if(op == "&") result.type = PointerTo(value);
    else if(op == "*") {
      if(!value || (value->kind != TYPE_POINTER && value->kind != TYPE_ARRAY))
        throw logic_error("cannot dereference expression");
      result.type = value->child;
      result.category = "lvalue";
    } else if(op == "!") result.type = Fundamental("bool");
    else if(op == "++" || op == "--") result.type = value;
    else if(op == "+" && value && value->kind == TYPE_ARRAY)
      result.type = PointerTo(value->child);
    else result.type = IntegralPromotion(value);
    if(op != "*") result.category = op == "++" || op == "--" ? "lvalue" : "prvalue";
    if(child.known_constant && (op == "+" || op == "-" || op == "~" || op == "!")) {
      result.known_constant = true;
      if(op == "+") result.constant = child.constant;
      else if(op == "-") result.constant = -child.constant;
      else if(op == "~") result.constant = ~child.constant;
      else result.constant = !child.constant;
    }
    return result;
  }

PA14Lowerer::ExprInfo PA14Lowerer::InferBinary(const CPPGMAstNodePtr& node, Scope* scope)
{
    ExprInfo result;
    const string op = PA12Operator(node->value);
    ExprInfo left = Infer(node->children[0], scope);
    ExprInfo right = Infer(node->children[1], scope);
    vector<CPPGMAstNodePtr> operator_arguments;
    operator_arguments.push_back(node->children[0]);
    operator_arguments.push_back(node->children[1]);
    bool mixed_bitwise = false;
    if(op == "&" || op == "bitand" || op == "|" || op == "bitor" ||
       op == "^" || op == "xor") {
      const TypePtr left_type = expression_value_type(left);
      const TypePtr right_type = expression_value_type(right);
      const bool class_operand = (left_type && left_type->kind == TYPE_CLASS) ||
        (right_type && right_type->kind == TYPE_CLASS);
      const bool same_enum_operands = left_type && right_type &&
        left_type->kind == TYPE_ENUM && right_type->kind == TYPE_ENUM &&
        PA12SameType(left_type, right_type, true);
      mixed_bitwise = !class_operand && !same_enum_operands;
    }
    CallChoice overloaded;
    if(!mixed_bitwise)
      overloaded = ChooseOperatorCall(OperatorFunctionName(op),
        operator_arguments, scope);
    bool prefer_builtin = false;
    const bool comparison = op == "==" || op == "!=" || op == "not_eq" ||
      op == "<" || op == ">" || op == "<=" || op == ">=";
    if(overloaded.binding && comparison) {
      TypePtr left_type = expression_value_type(left);
      TypePtr right_type = expression_value_type(right);
      int builtin_user_defined = 0;
      if(left_type && left_type->kind == TYPE_CLASS && right_type &&
         right_type->kind != TYPE_CLASS) {
        Binding* conversion = FindConversionOperator(left_type, right_type, false);
        if(conversion) {
          ++builtin_user_defined;
          TypePtr function = function_target_type(conversion->type);
          left_type = function ? type_value(function->child) : left_type;
        }
      } else if(right_type && right_type->kind == TYPE_CLASS && left_type &&
                left_type->kind != TYPE_CLASS) {
        Binding* conversion = FindConversionOperator(right_type, left_type, false);
        if(conversion) {
          ++builtin_user_defined;
          TypePtr function = function_target_type(conversion->type);
          right_type = function ? type_value(function->child) : right_type;
        }
      } else if(left_type && right_type && left_type->kind == TYPE_CLASS &&
                right_type->kind == TYPE_CLASS) {
        const vector<Binding*> conversions = ConversionBindings(left_type);
        for(size_t i = 0; i < conversions.size(); ++i) {
          TypePtr function = function_target_type(conversions[i]->type);
          TypePtr result_type = function ? type_value(function->child) : TypePtr();
          if(result_type && FindConversionOperator(right_type, result_type, false)) {
            ++builtin_user_defined;
            ++builtin_user_defined;
            left_type = result_type;
            right_type = result_type;
            break;
          }
        }
      }
      TypePtr common = CommonType(left_type, right_type, op);
      const int left_rank = common ? ConversionRank(left, common) : -1;
      const int right_rank = common ? ConversionRank(right, common) : -1;
      const bool builtin_type = common && type_value(common) &&
        type_value(common)->kind != TYPE_CLASS;
      if(builtin_type && left_rank >= 0 && right_rank >= 0 &&
         (builtin_user_defined < overloaded.user_defined ||
          (builtin_user_defined == overloaded.user_defined &&
           max(left_rank, right_rank) < overloaded.worst)))
        prefer_builtin = true;
    }
    if(overloaded.binding && !prefer_builtin) {
      result.type = overloaded.function->child;
      result.category = type_is_reference(result.type) ?
        (result.type->kind == TYPE_LVALUE_REFERENCE ? "lvalue" : "xvalue") : "prvalue";
      result.binding = overloaded.binding;
      return result;
    }
    if(op == ",") {
      result.type = right.type;
      result.category = right.category;
      return result;
    }
    if(op == "&&" || op == "||" || op == "and" || op == "or" ||
       op == "==" || op == "!=" || op == "not_eq" || op == "<" ||
       op == ">" || op == "<=" || op == ">=") result.type = Fundamental("bool");
    else if(op == "-" && expression_value_type(left) && expression_value_type(right) &&
            expression_value_type(left)->kind == TYPE_POINTER &&
            expression_value_type(right)->kind == TYPE_POINTER)
      result.type = Fundamental("long int");
    else if((op == "+" || op == "-") && expression_value_type(left) &&
            expression_value_type(left)->kind == TYPE_ARRAY)
      result.type = PointerTo(expression_value_type(left)->child);
    else if((op == "+" || op == "-") && expression_value_type(left) &&
            expression_value_type(left)->kind == TYPE_POINTER)
      result.type = expression_value_type(left);
    else if(op == "+" && expression_value_type(right) &&
            (expression_value_type(right)->kind == TYPE_POINTER ||
             expression_value_type(right)->kind == TYPE_ARRAY))
      result.type = expression_value_type(right)->kind == TYPE_ARRAY ?
        PointerTo(expression_value_type(right)->child) : expression_value_type(right);
    else {
      result.type = ArithmeticCommonType(expression_value_type(left),
        expression_value_type(right), op);
    }
    result.category = "prvalue";
    return result;
  }

PA14Lowerer::ExprInfo PA14Lowerer::InferSubscript(const CPPGMAstNodePtr& node, Scope* scope)
{
    ExprInfo base = Infer(node->children[0], scope);
    TypePtr value = expression_value_type(base);
    if(value && value->kind == TYPE_CLASS) {
      vector<CPPGMAstNodePtr> arguments;
      arguments.push_back(node->children[1]);
      if(!MemberBindings(value, "operator[]").empty())
        return InferCall(MakeMemberCall(node->children[0], "operator[]", arguments), scope);
      Binding* conversion = FindContextConversionOperator(value, false, false);
      TypePtr function = conversion ? function_target_type(conversion->type) : TypePtr();
      TypePtr pointer = function ? type_value(function->child) : TypePtr();
      if(pointer && pointer->kind == TYPE_POINTER) {
        ExprInfo result;
        result.type = pointer->child;
        result.category = "lvalue";
        return result;
      }
      throw logic_error("subscript base has no pointer conversion");
    }
    if((!value || (value->kind != TYPE_ARRAY && value->kind != TYPE_POINTER)) &&
       node->children.size() > 1) {
      ExprInfo index = Infer(node->children[1], scope);
      value = expression_value_type(index);
    }
    if(!value || (value->kind != TYPE_ARRAY && value->kind != TYPE_POINTER))
      throw logic_error("subscript requires array or pointer");
    ExprInfo result;
    result.type = value->child;
    result.category = "lvalue";
    return result;
  }

PA14Lowerer::ExprInfo PA14Lowerer::Infer(const CPPGMAstNodePtr& node, Scope* scope,
                const TypePtr& expected)
{
    if(!node) throw logic_error("missing expression during LowIR lowering");
    // Cache only context-free inference.  Expected-type inference is used for
    // initialization/conversion planning and must retain its caller's type.
    // The owning node in each entry prevents a short-lived synthetic AST node
    // from being mistaken for a later allocation at the same raw address.
    if(!expected) {
      map<const CPPGMAstNode*, InferCacheEntry>::const_iterator cached =
        infer_cache_.find(node.get());
      if(cached != infer_cache_.end() && cached->second.node == node &&
         cached->second.scope == scope)
        return cached->second.info;
    }
    const ExprInfo result = InferUncached(node, scope, expected);
    if(!expected) infer_cache_[node.get()] = InferCacheEntry(node, scope, result);
    return result;
}

PA14Lowerer::ExprInfo PA14Lowerer::InferSizeofExpression(
    const CPPGMAstNodePtr& node, Scope* scope)
{
      ExprInfo result;
      result.type = Fundamental("unsigned long int");
      result.known_constant = true;
      if(node->kind == "type-trait-expression" &&
         node->value.find("NOEXCEPT") != string::npos) {
        const ConstantValue value = analyzer_.Evaluate(node, scope);
        if(value.integral.known) {
          result.constant = PA19Signed(value.integral);
          return result;
        }
        result.constant = 0;
        return result;
      }
      const CPPGMAstNodePtr child = node->children.empty() ? CPPGMAstNodePtr() : node->children[0];
      TypePtr type;
      if(child && child->kind == "type-id") {
        const string local_name = TypeNameValue(child);
        VariablePlan* local = local_name.empty() ? 0 : FindLocalPlan(local_name);
        type = local ? type_value(local->type) : TypePtr();
        if(!type && !local_name.empty()) {
          const vector<Binding*> expression_bindings = Lookup(local_name, scope);
          for(size_t binding = 0; binding < expression_bindings.size(); ++binding)
            if(expression_bindings[binding] &&
              expression_bindings[binding]->kind != BIND_TYPE &&
              expression_bindings[binding]->kind != BIND_TYPE_ALIAS) {
              type = expression_bindings[binding]->type;
              break;
            }
        }
        if(!type) type = analyzer_.TypeFromTypeId(child, scope);
      }
      else if(child) {
        bool template_operand = false;
        CPPGMAstNodePtr object = child;
        if(object->kind == "member-expression" && !object->children.empty())
          object = object->children[0];
        if(object && object->kind == "id-expression") {
          VariablePlan* local = LocalForName(object->value);
          TypePtr local_type = local ? type_value(local->type) : TypePtr();
          template_operand = local_type && local_type->kind == TYPE_CLASS &&
            local_type->template_specialization;
        }
        const bool unevaluated_operand = template_operand ||
          (child && child->kind != "id-expression");
        const bool prior_unevaluated = state_ && state_->unevaluated_context;
        if(state_ && unevaluated_operand) state_->unevaluated_context = true;
        try {
          type = Infer(child, scope).type;
        } catch(...) {
          if(state_) state_->unevaluated_context = prior_unevaluated;
          throw;
        }
        if(state_ && unevaluated_operand) state_->unevaluated_context = prior_unevaluated;
      }
      result.constant = node->kind == "type-trait-expression" ?
        static_cast<long long>(type_alignment(type)) : static_cast<long long>(type_size(type));
      return result;
}

PA14Lowerer::ExprInfo PA14Lowerer::InferUncached(const CPPGMAstNodePtr& node, Scope* scope,
                const TypePtr& expected)
{
    if(!node) throw logic_error("missing expression during LowIR lowering");
    if(node->kind == "literal") return InferLiteral(node, expected, scope);
    if(node->kind == "keyword-literal") return InferKeyword(node);
    if(node->kind == "id-expression") return InferIdentifier(node, scope, expected);
    if(node->kind == "lambda-expression") {
      TypePtr closure = LambdaClosureType(node);
      if(closure && LambdaNeedsClosure(node, expected)) {
        ExprInfo result;
        result.type = closure;
        result.category = "prvalue";
        return result;
      }
      FunctionRecord* function = EnsureLambdaFunction(node, scope);
      ExprInfo result;
      // The closure's captureless conversion is represented at expression
      // boundaries as a pointer to its internal callable function.  EmitValue
      // retains the function designator so ConvertValue can materialize the
      // observable function-to-pointer decay at the initialization/call site.
      result.type = PointerTo(function->source_type);
      result.category = "prvalue";
      return result;
    }
    if(node->kind == "parenthesized-expression") return node->children.empty() ? ExprInfo() : Infer(node->children[0], scope, expected);
    if(node->kind == "new-expression" || node->kind == "delete-expression")
      return InferAllocation(node, scope);
    if(node->kind == "call-expression") return InferCall(node, scope);
    if(node->kind == "unary-expression") return InferUnary(node, scope);
    if(node->kind == "postfix-expression") {
      vector<CPPGMAstNodePtr> arguments;
      arguments.push_back(node->children[0]);
      arguments.push_back(CPPGMAstNodePtr(new CPPGMAstNode("literal", "0")));
      CallChoice overloaded = ChooseOperatorCall(
        OperatorFunctionName(PA12Operator(node->value)), arguments, scope);
      if(overloaded.binding) {
        ExprInfo result;
        result.type = overloaded.function->child;
        result.category = type_is_reference(result.type) ?
          (result.type->kind == TYPE_LVALUE_REFERENCE ? "lvalue" : "xvalue") : "prvalue";
        result.binding = overloaded.binding;
        return result;
      }
      ExprInfo result;
      ExprInfo child = Infer(node->children[0], scope);
      result.type = expression_value_type(child);
      result.category = "prvalue";
      return result;
    }
    if(node->kind == "binary-expression") return InferBinary(node, scope);
    if(node->kind == "assignment-expression") {
      ExprInfo left = Infer(node->children[0], scope);
      if(left.category != "lvalue") throw logic_error("assignment requires lvalue");
      const string op = PA12Operator(node->value);
      vector<CPPGMAstNodePtr> arguments;
      arguments.push_back(node->children[0]);
      arguments.push_back(node->children[1]);
      CallChoice overloaded = ChooseOperatorCall(OperatorFunctionName(op), arguments, scope);
      if(overloaded.binding) {
        ExprInfo result;
        result.type = overloaded.function->child;
        result.category = type_is_reference(result.type) ?
          (result.type->kind == TYPE_LVALUE_REFERENCE ? "lvalue" : "xvalue") : "prvalue";
        result.binding = overloaded.binding;
        return result;
      }
      ExprInfo result;
      result.type = expression_value_type(left);
      result.category = "lvalue";
      return result;
    }
    if(node->kind == "conditional-expression") {
      return InferConditional(node, scope);
    }
    if(node->kind == "subscript-expression") return InferSubscript(node, scope);
    if(node->kind == "member-expression") return InferMember(node, scope);
    if(node->kind == "cast-expression") {
      ExprInfo result;
      result.type = analyzer_.TypeFromTypeId(node->children[0], scope);
      result.category = type_is_reference(result.type) ?
        result.type->kind == TYPE_LVALUE_REFERENCE ? "lvalue" : "xvalue" : "prvalue";
      return result;
    }
    if(node->kind == "sizeof-pack-expression") {
      ExprInfo result;
      result.type = Fundamental("unsigned long int");
      result.known_constant = true;
      result.constant = node->value.empty() ? 0 : atoll(node->value.c_str());
      return result;
    }
    if(node->kind == "sizeof-expression" || node->kind == "type-trait-expression")
      return InferSizeofExpression(node, scope);
    if(node->kind == "braced-init-list") {
      ExprInfo result;
      result.type = expected ? expected : Fundamental("int");
      result.category = "lvalue";
      return result;
    }
    throw logic_error("unsupported expression in LowIR lowering: " + node->kind);
  }

PA14Lowerer::ExprInfo PA14Lowerer::InferAllocation(const CPPGMAstNodePtr& node,
                                                    Scope* scope)
{
    ExprInfo result;
    if(node->kind == "delete-expression") result.type = Fundamental("void");
    else {
      CPPGMAstNodePtr type_id;
      for(size_t i = 0; i < node->children.size(); ++i)
        if(node->children[i] && node->children[i]->kind == "type-id") type_id = node->children[i];
      if(!type_id) throw logic_error("new-expression has no allocated type");
      TypePtr allocated;
      try {
        allocated = type_value(analyzer_.TypeFromTypeId(type_id, scope));
      } catch(const logic_error&) {
        if(type_id->children.empty()) throw;
        allocated = type_value(analyzer_.TypeFromSpecSeq(type_id->children[0], scope));
      }
      if(allocated && allocated->kind == TYPE_ARRAY) allocated = type_value(allocated->child);
      result.type = PointerTo(allocated);
    }
    result.category = "prvalue";
    return result;
  }

bool PA14Lowerer::HasExplicitConstructor(const TypePtr& raw_type) const
{
    TypePtr type = type_value(raw_type);
    if(type && type->kind == TYPE_ARRAY) return HasExplicitConstructor(type->child);
    if(!type || type->kind != TYPE_CLASS) return false;
    const vector<Binding*> candidates = MemberBindings(type, LastComponent(type->name));
    for(size_t i = 0; i < candidates.size(); ++i) {
      Binding* binding = candidates[i];
      FunctionRecord* record = RecordForBinding(binding);
      if(binding->kind == BIND_FUNCTION && binding->is_member && !binding->is_static &&
         record && record->constructor && !record->implicit_constructor &&
         !record->defaulted) return true;
    }
    return false;
  }

bool PA14Lowerer::HasUserProvidedConstructor(const TypePtr& raw_type) const
{
    TypePtr type = type_value(raw_type);
    if(type && type->kind == TYPE_ARRAY) return HasUserProvidedConstructor(type->child);
    if(!type || type->kind != TYPE_CLASS) return false;
    const vector<Binding*> candidates = MemberBindings(type, LastComponent(type->name));
    for(size_t i = 0; i < candidates.size(); ++i) {
      Binding* binding = candidates[i];
      FunctionRecord* record = RecordForBinding(binding);
      if(binding->kind == BIND_FUNCTION && binding->is_member && !binding->is_static &&
         record && record->constructor && !record->implicit_constructor &&
         !record->defaulted && !record->aggregate_constructor) return true;
    }
    return false;
  }

bool PA14Lowerer::HasClassArrayMember(const TypePtr& raw_type) const
{
    TypePtr type = type_value(raw_type);
    if(!type || type->kind != TYPE_CLASS) return false;
    for(size_t i = 0; i < type->class_members.size(); ++i) {
      const ClassMemberInfo& member = type->class_members[i];
      if(member.is_static || !member.type) continue;
      TypePtr member_type = type_value(member.type);
      bool array_member = false;
      while(member_type && member_type->kind == TYPE_ARRAY) {
        array_member = true;
        member_type = type_value(member_type->child);
      }
      if(array_member && member_type && member_type->kind == TYPE_CLASS)
        return true;
    }
    return false;
  }

bool PA14Lowerer::HasNonstaticMemberFunction(const TypePtr& raw_type) const
{
    TypePtr type = type_value(raw_type);
    if(!type || type->kind != TYPE_CLASS || !type->owned_scope) return false;
    for(size_t i = 0; i < type->owned_scope->bindings.size(); ++i) {
      const Binding& binding = type->owned_scope->bindings[i];
      if(binding.kind == BIND_FUNCTION && binding.is_member && !binding.is_static)
        return true;
    }
  return false;
}

bool PA14Lowerer::HasNonSizeofReference(const CPPGMAstNodePtr& node,
                                        const string& name, bool inside_sizeof,
                                        bool ignore_discarded) const
{
    if(!node) return false;
    bool cast_to_void = false;
    if(node->kind == "cast-expression" && !node->children.empty() && node->children[0]) {
      function<bool(const CPPGMAstNodePtr&)> contains_void =
        [&](const CPPGMAstNodePtr& value) {
          if(!value) return false;
          if((value->kind == "type-specifier" || value->kind == "decl-specifier") &&
             (value->value == "void" || value->value == "KW_VOID:void")) return true;
          for(size_t child = 0; child < value->children.size(); ++child)
            if(contains_void(value->children[child])) return true;
          return false;
        };
      cast_to_void = contains_void(node->children[0]);
    }
    const bool now_inside_sizeof = inside_sizeof || (ignore_discarded && cast_to_void) ||
      node->kind == "sizeof-expression" ||
      node->kind == "sizeof-pack-expression";
    if(node->kind == "id-expression" && node->value == name && !now_inside_sizeof)
      return true;
    for(size_t i = 0; i < node->children.size(); ++i)
      if(HasNonSizeofReference(node->children[i], name, now_inside_sizeof,
                               ignore_discarded)) return true;
    return false;
  }

} // namespace cppgm_pa14_lowering
