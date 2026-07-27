#include "pa11_semantics_analyzer.h"

void Analyzer::ProcessTemplate(const CPPGMAstNodePtr& node, Scope* scope)
{
	if (node->children.size() < 2) throw logic_error("invalid template declaration");
	Scope* parameters = NewChild(scope, SCOPE_TEMPLATE_PARAMETERS, string());
	CPPGMAstNodePtr clause = node->children[0];
	CPPGMAstNodePtr list = ChildOfKind(clause, "template-parameter-list");
	if (list)
	{
		for (size_t i = 0; i < list->children.size(); ++i)
		{
			CPPGMAstNodePtr parameter = list->children[i];
			if (!parameter) continue;
			if (parameter->kind == "type-parameter")
			{
				const string name = FirstIdentifier(parameter);
				if (name.empty()) continue;
				const bool template_template = HasKind(parameter, "template-template-parameter");
				TypePtr type(new Type(template_template ? TYPE_TEMPLATE_TEMPLATE_PARAMETER : TYPE_TEMPLATE_PARAMETER, name));
				AddTypeBinding(parameters, name, type);
			}
		}
	}
	// Keep alias-template declarations as typed lookup anchors.  Their concrete
	// targets are materialized during PA18 replay, while PA11 still needs a
	// dependent type in both the parameter and enclosing scopes.
	if (node->children[1] && node->children[1]->kind == "alias-declaration")
	{
		const string alias_name = node->children[1]->value;
		if (!alias_name.empty())
		{
			string alias_target;
			if (!node->children[1]->children.empty() && node->children[1]->children[0]) {
				const CPPGMAstNodePtr type_id = node->children[1]->children[0];
				if (!type_id->children.empty() && type_id->children[0]) {
					const CPPGMAstNodePtr sequence = type_id->children[0];
					if (!sequence->children.empty() && sequence->children[0])
						alias_target = sequence->children[0]->value;
				}
			}
			TypePtr alias_type;
			const size_t target_open = alias_target.find('<');
			if (target_open != string::npos &&
				alias_target.find("::", target_open) == string::npos &&
				alias_target.compare(0, 8, "typename ") != 0) {
				try { alias_type = ResolveType(parameters, alias_target); }
				catch (const logic_error&) {}
			}
			if (!alias_type) alias_type.reset(new Type(TYPE_TEMPLATE_PARAMETER, alias_name));
			else {
				alias_type->name = alias_name;
				alias_type->template_primary = alias_name;
			}
			AddTypeBinding(parameters, alias_name, alias_type, true);
			if (scope) AddTypeBinding(scope, alias_name, alias_type, true);
			return;
		}
	}
	Process(node->children[1], parameters);
	// Keep a typed primary anchor in the enclosing scope for later concrete
	// member replay.  It is semantic lookup state, not a second source binding,
	// so the PA11 dump suppresses this synthetic anchor.
	if (scope && (node->children[1]->kind == "class-specifier" ||
		node->children[1]->kind == "class-forward-declaration")) {
		const string class_name = LastComponent(node->children[1]->value);
		Binding* template_class = class_name.empty() ? 0 : parameters->local(class_name);
		if (template_class && template_class->kind == BIND_TYPE && template_class->type) {
			AddTypeBinding(scope, class_name, template_class->type);
			Binding* anchor = scope->local(class_name);
			if (anchor) anchor->suppress_dump = true;
		}
	}
	// Materialized classes retain dependent member-template declarations so
	// later lowering can select their concrete replay.  Keep the callable
	// binding in the enclosing class scope without adding it to layout.
	if (scope && scope->kind == SCOPE_CLASS && scope->owner_type &&
		node->children[1] && node->children[1]->kind != "alias-declaration")
		for (size_t binding_index = 0; binding_index < parameters->bindings.size();
			++binding_index) {
			const Binding& source = parameters->bindings[binding_index];
			if (source.kind != BIND_FUNCTION || source.name.empty()) continue;
			Binding member = source;
			member.is_member = true;
			member.member_owner = scope->owner_type;
			member.declaration = node->children[1];
			scope->add(member);
		}
	// A friend function template declared inside a class template is a
	// namespace-level entity whose declaration is nevertheless needed by the
	// owning class for access checks.  Record its typed access fact here.
	if (scope && scope->kind == SCOPE_CLASS && scope->owner_type &&
		node->children[1] && node->children[1]->kind == "simple-declaration" &&
		!node->children[1]->children.empty()) {
		SpecFacts facts;
		TypePtr friend_type = TypeFromSpecSeq(node->children[1]->children[0],
			parameters, &facts);
		const CPPGMAstNodePtr friend_list = ChildOfKind(node->children[1],
			"init-declarator-list");
		const CPPGMAstNodePtr friend_item = friend_list &&
			!friend_list->children.empty() ? friend_list->children[0] :
			CPPGMAstNodePtr();
		const CPPGMAstNodePtr friend_declarator = friend_item &&
			!friend_item->children.empty() ? friend_item->children[0] :
			CPPGMAstNodePtr();
		TypePtr friend_function = friend_declarator ? BuildDeclarator(
			friend_declarator, friend_type, parameters) : TypePtr();
		if (facts.is_friend && friend_function &&
			friend_function->kind == TYPE_FUNCTION) {
			const string friend_name = FirstIdentifier(node->children[1]);
			if (!friend_name.empty()) scope->owner_type->friend_access.push_back(
				FriendAccess(FriendAccess::FRIEND_FUNCTION, friend_name, friend_function));
		}
	}
	if (node->children[1] && node->children[1]->kind == "function-definition" &&
		node->children[1]->children.size() > 1)
	{
		const string function_name = FirstIdentifier(node->children[1]->children[1]);
		Binding* function = parameters->local(function_name);
		if (function && function->kind == BIND_FUNCTION)
			constant_template_functions_[function_name].push_back(function);
	}
}
