#include "pa11_semantics_analyzer.h"
#include <functional>

using namespace std;

void Analyzer::PredeclareGeneratedScopes(const CPPGMAstNodePtr& tree)
{
	set<string> generated_spellings;
	function<void(const CPPGMAstNodePtr&)> collect_generated;
	collect_generated = [&](const CPPGMAstNodePtr& node) {
		if (!node) return;
		if ((node->kind == "class-specifier" ||
			node->kind == "class-forward-declaration") &&
			(node->template_instantiation || node->explicit_specialization))
			generated_spellings.insert(node->value);
		for (size_t i = 0; i < node->children.size(); ++i)
			collect_generated(node->children[i]);
	};
	collect_generated(tree);
	const bool has_generated = !generated_spellings.empty();
	function<void(const CPPGMAstNodePtr&, Scope*, bool)> predeclare;
	predeclare = [&](const CPPGMAstNodePtr& node, Scope* scope, bool generated_parent) {
		if (!node || !scope) return;
		if (node->kind == "translation-unit") {
			for (size_t i = 0; i < node->children.size(); ++i)
				predeclare(node->children[i], scope, false);
			return;
		}
		if (node->kind == "namespace-definition") {
			const string name = node->value;
			if (name == "<unnamed>") return;
			if (node->synthetic_namespace_forward) {
				Binding* class_binding = scope->local(name);
				if (class_binding && class_binding->kind == BIND_TYPE &&
					class_binding->type && class_binding->type->kind == TYPE_CLASS) {
					Scope* class_scope = ClassScope(class_binding->type, scope, name);
					for (size_t i = 0; i < node->children.size(); ++i)
						predeclare(node->children[i], class_scope, true);
					return;
				}
			}
			Scope* namespace_scope = 0;
			map<string, Scope*>::iterator found = scope->namespace_children.find(name);
			if (found != scope->namespace_children.end()) namespace_scope = found->second;
			else {
				namespace_scope = NewChild(scope, SCOPE_NAMESPACE, name);
				namespace_scope->qualified_prefix = scope->qualified_prefix.empty() ?
					name : scope->qualified_prefix + "::" + name;
				scope->namespace_children[name] = namespace_scope;
			}
			for (size_t i = 0; i < node->children.size(); ++i)
				if (node->children[i]->kind != "inline")
					predeclare(node->children[i], namespace_scope, false);
			return;
		}
		if (node->kind == "template-declaration") {
			for (size_t i = 0; i < node->children.size(); ++i)
				predeclare(node->children[i], scope, generated_parent);
			return;
		}
		if (node->kind != "class-specifier" &&
			node->kind != "class-forward-declaration") return;
		const bool generated = node->template_instantiation || node->explicit_specialization;
		if (!generated && generated_parent && generated_spellings.find(node->value) !=
			generated_spellings.end()) return;
		if (!generated && !has_generated)
			return;
		const string name = LastComponent(node->value);
		if (name.empty()) return;
		TypePtr type;
		Binding* existing = scope->local(name);
		if (existing && existing->kind == BIND_TYPE && existing->type &&
			existing->type->kind == TYPE_CLASS) type = existing->type;
		else {
			type.reset(new Type(TYPE_CLASS, name));
			type->tag = ClassKey(node);
			if (!scope->qualified_prefix.empty())
				type->name = scope->qualified_prefix + "::" + name;
			AddTypeBinding(scope, name, type);
		}
		type->tag = ClassKey(node);
		Scope* class_scope = ClassScope(type, scope, name);
		for (size_t i = 0; i < node->children.size(); ++i) {
			const CPPGMAstNodePtr child = node->children[i];
			if (!child || child->kind == "class-key" || child->kind == "base-clause") continue;
			predeclare(child, class_scope, generated_parent || generated);
		}
	};
	predeclare(tree, global_.get(), false);
}
