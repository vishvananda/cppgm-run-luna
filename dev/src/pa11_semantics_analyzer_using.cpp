#include "pa11_semantics_analyzer.h"

using namespace std;

void Analyzer::ProcessUsingDeclaration(const CPPGMAstNodePtr& node, Scope* scope)
{
	CPPGMAstNodePtr target_node = ChildOfKind(node, "target");
	if (!target_node) throw logic_error("invalid using declaration");
	const string target_name = target_node->value;
	const string target_component = LastComponent(target_name);
	const bool operator_name = target_component.compare(0, 8, "operator") == 0 &&
		target_component.size() > 8 && target_component[8] != ' ';
	if (target_name.find('<') != string::npos && !operator_name)
		throw logic_error("using declaration cannot name template-id");
	const size_t target_separator = target_name.rfind("::");
	const string target_owner_name = target_separator == string::npos ? string() :
		LastComponent(target_name.substr(0, target_separator));
	vector<Binding*> targets;
	if (target_separator != string::npos)
	{
		PathTarget owner = ResolvePath(scope, target_name.substr(0, target_separator));
		Scope* owner_scope = owner.scope;
		if (!owner_scope && owner.binding) owner_scope = ScopeForType(owner.binding->type);
		if (owner_scope)
			for (size_t i = 0; i < owner_scope->bindings.size(); ++i)
				if (owner_scope->bindings[i].name == LastComponent(target_name))
					targets.push_back(&owner_scope->bindings[i]);
		if (targets.empty() && owner_scope)
		{
			const string generated_prefix = LastComponent(target_name) + "__inst_";
			for (size_t i = 0; i < owner_scope->bindings.size(); ++i)
				if (owner_scope->bindings[i].kind == BIND_FUNCTION &&
					owner_scope->bindings[i].name.compare(0, generated_prefix.size(), generated_prefix) == 0)
					targets.push_back(&owner_scope->bindings[i]);
		}
		// PA18 renames a materialized class and its constructor together.  The
		// source using-id still names the primary constructor (`Base::Base`),
		// while the concrete owner scope contains `Base_args_::Base_args_`.
		// Treat that typed owner identity as the constructor declaration.
		if (targets.empty() && owner_scope && owner_scope->owner_type) {
			const string concrete_constructor = LastComponent(
				owner_scope->owner_type->name);
			for (size_t i = 0; i < owner_scope->bindings.size(); ++i)
				if (owner_scope->bindings[i].kind == BIND_FUNCTION &&
					owner_scope->bindings[i].name == concrete_constructor) {
					targets.push_back(&owner_scope->bindings[i]);
					break;
				}
		}
		if (owner_scope)
			for (size_t child_index = 0; child_index < owner_scope->children.size(); ++child_index)
			{
				Scope* child = owner_scope->children[child_index].get();
				if (!child || child->kind != SCOPE_TEMPLATE_PARAMETERS) continue;
				for (size_t i = 0; i < child->bindings.size(); ++i)
					if (child->bindings[i].name == LastComponent(target_name) &&
						child->bindings[i].kind == BIND_FUNCTION)
							targets.push_back(&child->bindings[i]);
				}
		// A using-declaration may name a member inherited from the direct base.
		// Generated template classes commonly spell this through the concrete
		// derived owner (`relay_int_::get`) even though the declaration is retained
		// in `base_impl_false__int_`'s typed scope.
		if (targets.empty() && owner_scope && owner_scope->owner_type)
			for (TypePtr base = owner_scope->owner_type->direct_base; base &&
				targets.empty(); base = base->direct_base)
				if (base->owned_scope)
					for (size_t i = 0; i < base->owned_scope->bindings.size(); ++i)
						if (base->owned_scope->bindings[i].name == LastComponent(target_name)) {
							targets.push_back(&base->owned_scope->bindings[i]);
							break;
						}
	}
	if (targets.empty()) {
		Binding* target = ResolveBinding(scope, target_name);
		if (target) targets.push_back(target);
	}
	if (targets.empty())
	{
		if (!processing_pending_using_declarations_)
		{
			pending_using_declarations_.push_back(make_pair(node, scope));
			return;
		}
		throw logic_error("using target is not a declaration");
	}
	for (size_t target_index = 0; target_index < targets.size(); ++target_index)
	{
		Binding imported = *targets[target_index];
		const string generated_prefix = LastComponent(target_name) + "__inst_";
		const bool materialized_target = imported.name.compare(0, generated_prefix.size(), generated_prefix) == 0;
		const bool constructor_target = imported.kind == BIND_FUNCTION &&
			scope && scope->kind == SCOPE_CLASS && scope->owner_type &&
			!target_owner_name.empty() && (LastComponent(target_name) == target_owner_name ||
				imported.name == target_owner_name);
		imported.name = constructor_target ? LastComponent(scope->owner_type->name) :
			materialized_target ? imported.name : LastComponent(target_name);
		// Scope::add preserves an already-qualified binding name.  An imported
		// constructor is a declaration in the derived class for PA11 lookup,
		// so let the destination scope form its qualified identity.  Ordinary
		// using-declarations retain the source identity for overload lowering.
		if (constructor_target) imported.qualified_name.clear();
		scope->add(imported);
	}
}
