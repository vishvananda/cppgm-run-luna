#include "pa11_semantics_analyzer.h"

void Analyzer::ProcessNamespace(const CPPGMAstNodePtr& node, Scope* scope)
{
	const string name = node->value;
	if (node->synthetic_namespace_forward) {
		Binding* class_binding = scope->local(name);
		if (class_binding && class_binding->kind == BIND_TYPE &&
			class_binding->type && class_binding->type->kind == TYPE_CLASS) {
			Scope* class_scope = ClassScope(class_binding->type, scope, name);
			for (size_t i = 0; i < node->children.size(); ++i)
				Process(node->children[i], class_scope);
			return;
		}
	}
	if (name != "<unnamed>" && (scope->local(name) ||
		scope->namespace_aliases.find(name) != scope->namespace_aliases.end()))
		throw logic_error("namespace conflicts with declaration");
	Scope* namespace_scope = 0;
	bool reused_anonymous_scope = false;
	map<const CPPGMAstNode*, Scope*>::iterator predeclared =
		namespace_scopes_.find(node.get());
	if (predeclared != namespace_scopes_.end()) {
		namespace_scope = predeclared->second;
		reused_anonymous_scope = namespace_scope != 0;
	}
	if (node->synthetic_namespace_forward && name == "<unnamed>")
		for (size_t child = scope->children.size(); child > 0; --child)
			if (scope->children[child - 1] &&
				scope->children[child - 1]->kind == SCOPE_NAMESPACE &&
				scope->children[child - 1]->name == "<unnamed>") {
				namespace_scope = scope->children[child - 1].get();
				reused_anonymous_scope = true;
				break;
			}
	map<string, Scope*>::iterator found = scope->namespace_children.find(name);
	if (!namespace_scope && name != "<unnamed>" && found != scope->namespace_children.end())
		namespace_scope = found->second;
	if (!namespace_scope) {
		namespace_scope = NewChild(scope, SCOPE_NAMESPACE, name);
		if (name != "<unnamed>") scope->namespace_children[name] = namespace_scope;
	}
	if (name == "<unnamed>") {
		// Keep anonymous-namespace identity in the typed scope path.  The
		// enclosing namespace still exposes the scope through its using path,
		// while symbol lowering needs the stable internal namespace component.
		if (!reused_anonymous_scope) {
			size_t occurrence = 0;
			for (size_t child = 0; child < scope->children.size(); ++child)
				if (scope->children[child] &&
					scope->children[child]->kind == SCOPE_NAMESPACE &&
					scope->children[child]->name == "<unnamed>") ++occurrence;
			ostringstream suffix;
			suffix << occurrence;
			const string component = "_GLOBAL__N_" + suffix.str();
			namespace_scope->qualified_prefix = scope->qualified_prefix.empty() ?
				component : scope->qualified_prefix + "::" + component;
		}
	}
	if (name == "<unnamed>") {
		bool already_visible = false;
		for (size_t using_index = 0; using_index < scope->using_directives.size(); ++using_index)
			if (scope->using_directives[using_index] == namespace_scope) already_visible = true;
		if (!already_visible) scope->using_directives.push_back(namespace_scope);
	}
	namespace_scopes_[node.get()] = namespace_scope;
	namespace_scope->inline_namespace = HasKind(node, "inline");
	if (namespace_scope->inline_namespace) {
		bool already_visible = false;
		for (size_t i = 0; i < scope->using_directives.size(); ++i)
			if (scope->using_directives[i] == namespace_scope) already_visible = true;
		if (!already_visible) scope->using_directives.push_back(namespace_scope);
	}
	for (size_t i = 0; i < node->children.size(); ++i)
		if (node->children[i]->kind != "inline") Process(node->children[i], namespace_scope);
}
