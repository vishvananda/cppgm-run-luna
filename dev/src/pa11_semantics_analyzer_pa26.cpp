#include "pa11_semantics_analyzer.h"

TypePtr Analyzer::ResolveGeneratedSpecializationType(const string& name) const
{
	vector<const Scope*> pending;
	pending.push_back(global_.get());
	while (!pending.empty()) {
		const Scope* candidate_scope = pending.back();
		pending.pop_back();
		if (!candidate_scope) continue;
		for (size_t candidate = 0; candidate < candidate_scope->bindings.size(); ++candidate) {
			const Binding& found = candidate_scope->bindings[candidate];
			if (found.name == name && (found.kind == BIND_TYPE ||
				found.kind == BIND_TYPE_ALIAS) && found.type &&
				found.type->template_specialization &&
				!found.type->template_primary.empty()) return found.type;
		}
		for (size_t child = 0; child < candidate_scope->children.size(); ++child)
			pending.push_back(candidate_scope->children[child].get());
	}
	return TypePtr();
}

TypePtr Analyzer::ResolveQualifiedNestedType(Scope* from, const string& name) const
{
	const size_t separator = name.rfind("::");
	if (separator == string::npos) return TypePtr();
	try {
		TypePtr owner_type = ResolveType(from, name.substr(0, separator));
		Scope* owner_scope = ScopeForType(owner_type);
		const string requested = name.substr(separator + 2);
		Binding* nested = owner_scope ? owner_scope->local(requested) : 0;
		if (nested && (nested->kind == BIND_TYPE || nested->kind == BIND_TYPE_ALIAS))
			return nested->type;
	} catch (const logic_error&) {}
	return TypePtr();
}

bool Analyzer::ShouldRecordQualifiedClassMember(const string& name,
	const TypePtr& type, Scope* class_scope) const
{
	const size_t separator = name.rfind("::");
	if (separator == string::npos) return true;
	TypePtr qualified_type;
	try { qualified_type = ResolveType(class_scope, name.substr(0, separator)); }
	catch (const logic_error&) {}
	return !qualified_type || qualified_type->kind != TYPE_CLASS ||
		qualified_type.get() == type.get();
}

void Analyzer::RebindGeneratedClassMembers(const TypePtr& type, Scope* owner,
	Scope* class_scope)
{
	TypePtr primary;
	try { primary = ResolveType(owner, type->template_primary); }
	catch (const logic_error&) {}
	if (!primary || primary->kind != TYPE_CLASS || !primary->owned_scope) return;
	for (size_t binding = 0; binding < primary->owned_scope->bindings.size(); ++binding) {
		const Binding& source = primary->owned_scope->bindings[binding];
		if ((source.kind != BIND_TYPE && source.kind != BIND_TYPE_ALIAS) ||
			source.name.empty()) continue;
		if (class_scope->local(source.name)) continue;
		Binding rebound = source;
		class_scope->add(rebound);
	}
}

void Analyzer::SeedGeneratedTemplateScope(const CPPGMAstNodePtr& node,
	const TypePtr& generated, Scope* owner, const string& name)
{
	if (!generated) return;
	Scope* generated_scope = ClassScope(generated, owner, name);
	TypePtr primary;
	try { primary = ResolveType(owner, node->template_primary); }
	catch (const logic_error&) {}
	if (!primary || primary->kind != TYPE_CLASS) return;
	if (generated->template_parameter_names.empty())
		generated->template_parameter_names = primary->template_parameter_names;
	if (generated->template_parameter_packs.empty())
		generated->template_parameter_packs = primary->template_parameter_packs;
	if (generated->template_parameter_names.empty() && primary->owned_scope &&
		primary->owned_scope->parent && primary->owned_scope->parent->kind ==
		SCOPE_TEMPLATE_PARAMETERS)
		for (size_t binding = 0; binding < primary->owned_scope->parent->bindings.size(); ++binding)
			if (primary->owned_scope->parent->bindings[binding].kind == BIND_TYPE ||
				primary->owned_scope->parent->bindings[binding].kind == BIND_TYPE_ALIAS)
				generated->template_parameter_names.push_back(
					primary->owned_scope->parent->bindings[binding].name);
	for (size_t parameter = 0; parameter < generated->template_parameter_names.size() &&
		parameter < node->template_arguments.size(); ++parameter) {
		const string& parameter_name = generated->template_parameter_names[parameter];
		if (parameter_name.empty() || generated_scope->local(parameter_name)) continue;
		TypePtr argument;
		try { argument = ResolveType(owner, node->template_arguments[parameter]); }
		catch (const logic_error&) {}
		if (argument) AddTypeBinding(generated_scope, parameter_name, argument);
	}
}

void Analyzer::SeedTemplateParameterBindings(const CPPGMAstNodePtr& node,
	Scope* scope, Scope* parameters)
{
	if (!scope || !node->children[1] ||
		(node->children[1]->kind != "class-specifier" &&
			node->children[1]->kind != "class-forward-declaration")) return;
	const string class_name = LastComponent(node->children[1]->value);
	Binding* class_binding = class_name.empty() ? 0 : scope->local(class_name);
	Scope* class_scope = class_binding && class_binding->type ?
		class_binding->type->owned_scope : 0;
	if (!class_scope) return;
	for (size_t parameter = 0; parameter < parameters->bindings.size(); ++parameter) {
		const Binding& source = parameters->bindings[parameter];
		if (class_scope->local(source.name)) continue;
		Binding inherited = source;
		inherited.suppress_dump = true;
		class_scope->add(inherited);
	}
}
