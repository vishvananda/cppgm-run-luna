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
			(node->template_instantiation || node->explicit_specialization ||
			 LastComponent(node->value).compare(0, 9, "__lambda_") == 0))
			generated_spellings.insert(node->value);
		for (size_t i = 0; i < node->children.size(); ++i)
			collect_generated(node->children[i]);
	};
	collect_generated(tree);
	const bool has_generated = !generated_spellings.empty();
	set<Scope*> synthetic_anonymous_scopes;
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
			if (name == "<unnamed>" && !has_generated) return;
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
			map<const CPPGMAstNode*, Scope*>::iterator predeclared =
				namespace_scopes_.find(node.get());
			if (predeclared != namespace_scopes_.end()) namespace_scope = predeclared->second;
			if (!namespace_scope) {
				map<string, Scope*>::iterator found = scope->namespace_children.find(name);
				if (found != scope->namespace_children.end()) namespace_scope = found->second;
			}
			if (!namespace_scope) {
				namespace_scope = NewChild(scope, SCOPE_NAMESPACE, name);
				if (name == "<unnamed>") {
					size_t occurrence = 0;
					for (size_t child = 0; child < scope->children.size(); ++child)
						if (scope->children[child] &&
							scope->children[child]->kind == SCOPE_NAMESPACE &&
							scope->children[child]->name == "<unnamed>") ++occurrence;
					bool synthetic_anonymous = false;
					for (size_t child = 0; child < scope->children.size(); ++child)
						if (scope->children[child] && synthetic_anonymous_scopes.find(
							scope->children[child].get()) != synthetic_anonymous_scopes.end()) {
							synthetic_anonymous = true;
							break;
						}
					ostringstream suffix;
					suffix << (synthetic_anonymous && occurrence > 0 ? occurrence - 1 : occurrence);
					const string component = "_GLOBAL__N_" + suffix.str();
					namespace_scope->qualified_prefix = scope->qualified_prefix.empty() ?
						component : scope->qualified_prefix + "::" + component;
				} else namespace_scope->qualified_prefix = scope->qualified_prefix.empty() ?
					name : scope->qualified_prefix + "::" + name;
				if (name != "<unnamed>") scope->namespace_children[name] = namespace_scope;
			}
			if (name == "<unnamed>" && node->synthetic_namespace_forward)
				synthetic_anonymous_scopes.insert(namespace_scope);
			namespace_scopes_[node.get()] = namespace_scope;
			if (name == "<unnamed>") {
				bool visible = false;
				for (size_t using_index = 0; using_index < scope->using_directives.size(); ++using_index)
					if (scope->using_directives[using_index] == namespace_scope) visible = true;
				if (!visible) scope->using_directives.push_back(namespace_scope);
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
		if (node->template_instantiation) {
			type->template_specialization = true;
			type->template_primary = node->template_primary;
			type->template_arguments = node->template_arguments;
			type->template_parameter_names = node->template_parameter_names;
			type->template_parameter_packs = node->template_parameter_packs;
			type->template_empty_pack = node->template_empty_pack;
		}
		Scope* class_scope = ClassScope(type, scope, name);
		for (size_t i = 0; i < node->children.size(); ++i) {
			const CPPGMAstNodePtr child = node->children[i];
			if (!child || child->kind == "class-key" || child->kind == "base-clause") continue;
			predeclare(child, class_scope, generated_parent || generated);
		}
	};
	predeclare(tree, global_.get(), false);
	// PA18 may place a materialized alias or class specialization before the
	// source declaration that caused the demand.  Its generated scope still
	// belongs to the source translation unit, so make source class identities
	// visible before the early alias pass without processing their members or
	// computing layout out of source order.  The ordinary Process pass below
	// reuses these bindings and remains responsible for the complete definition.
	if (has_generated) {
		function<void(const CPPGMAstNodePtr&, Scope*)> predeclare_source_classes;
		predeclare_source_classes = [&](const CPPGMAstNodePtr& node, Scope* scope) {
			if (!node || !scope) return;
			if (node->kind == "translation-unit") {
				for (size_t i = 0; i < node->children.size(); ++i)
					predeclare_source_classes(node->children[i], scope);
				return;
			}
			if (node->kind == "namespace-definition") {
				Scope* namespace_scope = 0;
				map<const CPPGMAstNode*, Scope*>::iterator mapped =
					namespace_scopes_.find(node.get());
				if (mapped != namespace_scopes_.end()) namespace_scope = mapped->second;
				if (!namespace_scope) {
					map<string, Scope*>::iterator found = scope->namespace_children.find(node->value);
					if (found != scope->namespace_children.end()) namespace_scope = found->second;
				}
				if (!namespace_scope) return;
				for (size_t i = 0; i < node->children.size(); ++i)
					predeclare_source_classes(node->children[i], namespace_scope);
				return;
			}
			if (node->kind == "template-declaration") {
				for (size_t i = 0; i < node->children.size(); ++i)
					predeclare_source_classes(node->children[i], scope);
				return;
			}
			if (node->kind == "enum-specifier") {
				const string name = LastComponent(node->value);
				if (name.empty() || scope->local(name)) return;
				TypePtr type(new Type(TYPE_ENUM, name));
				type->complete = false;
				type->underlying = Fundamental("int");
				if (!scope->qualified_prefix.empty())
					type->name = scope->qualified_prefix + "::" + name;
				AddTypeBinding(scope, name, type);
				return;
			}
			if (node->kind != "class-specifier" &&
				node->kind != "class-forward-declaration") return;
			if (node->template_instantiation || node->explicit_specialization) return;
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
			for (size_t i = 0; i < node->children.size(); ++i)
				predeclare_source_classes(node->children[i], class_scope);
		};
		predeclare_source_classes(tree, global_.get());
	}
	// Generated nested specializations can be processed before the source
	// declaration that names a global typedef.  Make ordinary typedef names
	// available during that early class replay, while leaving alias templates
	// to PA18's typed template state.
	function<bool(const CPPGMAstNodePtr&)> contains_typedef;
	contains_typedef = [&](const CPPGMAstNodePtr& node) {
		if (!node) return false;
		if (node->value.find("TYPEDEF") != string::npos ||
			node->value.find("typedef") != string::npos) return true;
		for (size_t i = 0; i < node->children.size(); ++i)
			if (contains_typedef(node->children[i])) return true;
		return false;
	};
	function<void(const CPPGMAstNodePtr&, Scope*)> predeclare_aliases;
	predeclare_aliases = [&](const CPPGMAstNodePtr& node, Scope* scope) {
		if (!node || !scope) return;
		if (node->kind == "translation-unit") {
			for (size_t i = 0; i < node->children.size(); ++i)
				predeclare_aliases(node->children[i], scope);
			return;
		}
		if (node->kind == "namespace-definition") {
			if (node->value == "<unnamed>") return;
			Scope* namespace_scope = 0;
			map<string, Scope*>::iterator found = scope->namespace_children.find(node->value);
			if (found != scope->namespace_children.end()) namespace_scope = found->second;
			if (!namespace_scope) return;
			for (size_t i = 0; i < node->children.size(); ++i)
				predeclare_aliases(node->children[i], namespace_scope);
			return;
		}
		if (node->kind == "template-declaration") {
			for (size_t i = 0; i < node->children.size(); ++i) {
				const CPPGMAstNodePtr child = node->children[i];
				if (!child || (child->kind != "class-specifier" &&
					child->kind != "class-forward-declaration")) continue;
				predeclare_aliases(child, scope);
			}
			return;
		}
		if (node->kind == "class-specifier" ||
			node->kind == "class-forward-declaration") {
			const string name = LastComponent(node->value);
			Binding* binding = name.empty() ? 0 : scope->local(name);
			Scope* class_scope = binding && binding->type &&
				binding->type->kind == TYPE_CLASS ? ClassScope(binding->type, scope, name) : 0;
			if (!class_scope) return;
			for (size_t i = 0; i < node->children.size(); ++i)
				predeclare_aliases(node->children[i], class_scope);
			return;
		}
		if (node->kind == "simple-declaration" && !node->children.empty() &&
			contains_typedef(node->children[0])) {
			try {
				SpecFacts facts;
				const TypePtr base = TypeFromSpecSeq(node->children[0], scope, &facts);
				const CPPGMAstNodePtr list = ChildOfKind(node, "init-declarator-list");
				if (!list) return;
				for (size_t i = 0; i < list->children.size(); ++i) {
					const CPPGMAstNodePtr item = list->children[i];
					if (!item || item->children.empty()) continue;
					const string name = FirstIdentifier(item->children[0]);
					if (name.empty() || scope->local(name)) continue;
					AddTypeBinding(scope, name, BuildDeclarator(item->children[0], base, scope), true);
				}
			} catch (const logic_error&) {}
			return;
		}
		if (node->kind == "alias-declaration" && !node->children.empty() &&
			!scope->local(node->value)) {
			try { AddTypeBinding(scope, node->value,
				TypeFromTypeId(node->children[0], scope), true); }
			catch (const logic_error&) {}
			return;
		}
	};
	predeclare_aliases(tree, global_.get());
}
