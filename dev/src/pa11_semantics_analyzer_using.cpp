#include "pa11_semantics_analyzer.h"

#include <cctype>

using namespace std;

void Analyzer::ProcessUsingDeclaration(const CPPGMAstNodePtr& node, Scope* scope)
{
	CPPGMAstNodePtr target_node = ChildOfKind(node, "target");
	if (!target_node) throw logic_error("invalid using declaration");
	const string target_name = target_node->value;
	const string target_component = LastComponent(target_name);
	const bool operator_name = target_component.compare(0, 8, "operator") == 0 &&
		target_component.size() > 8;
	const size_t target_separator = target_name.rfind("::");
	string target_owner_component = target_separator == string::npos ? string() :
		LastComponent(target_name.substr(0, target_separator));
	const size_t target_owner_open = target_owner_component.find('<');
	if(target_owner_open != string::npos) target_owner_component.erase(target_owner_open);
	const bool constructor_spelling = target_separator != string::npos &&
		target_owner_component == target_component;
	if (target_name.find('<') != string::npos && !operator_name && !constructor_spelling)
		throw logic_error("using declaration cannot name template-id");
	const string target_owner_name = target_separator == string::npos ? string() :
		LastComponent(target_name.substr(0, target_separator));
	vector<Binding*> targets;
	Scope* operator_owner_scope = 0;
	if (target_separator != string::npos)
	{
		PathTarget owner = ResolvePath(scope, target_name.substr(0, target_separator));
		Scope* owner_scope = owner.scope;
		if (!owner_scope && owner.binding) owner_scope = ScopeForType(owner.binding->type);
		TypePtr requested_owner;
		if (!owner_scope) {
			try {
				requested_owner = ResolveType(scope, target_name.substr(0, target_separator));
			} catch (const logic_error&) {
				// Qualified using-declarations may name a namespace.  The typed owner
				// recovery below is only for a class-template specialization; a
				// namespace owner is already completely described by ResolvePath.
			}
		}
		if (!owner_scope) owner_scope = ScopeForType(requested_owner);
		// A generated specialization may intentionally be emitted as a class
		// shell while its primary still owns the dependent member-template
		// declarations.  Use that typed primary scope for a using-id lookup;
		// the concrete call is still replayed against the generated owner later.
		if (owner_scope && owner_scope->bindings.empty() && owner_scope->owner_type &&
			!owner_scope->owner_type->template_primary.empty()) {
			TypePtr primary;
			try { primary = ResolveType(scope, owner_scope->owner_type->template_primary); }
			catch (const logic_error&) {}
			Scope* primary_scope = ScopeForType(primary);
			if (primary_scope && !primary_scope->bindings.empty()) owner_scope = primary_scope;
		}
		if (owner_scope && owner_scope->bindings.empty()) owner_scope = 0;
		// A dependent constructor using-declaration can retain the source
		// template-id while PA18 has already materialized the concrete base under
		// its generated type name.  ResolveType still describes the source
		// specialization, whose template scope is intentionally empty; recover
		// the typed generated specialization from the enclosing concrete class.
		if ((!owner_scope || owner_scope->bindings.empty()) && requested_owner &&
			scope && scope->owner_type) {
			const string requested_primary = requested_owner->template_primary.empty() ?
				target_owner_component : LastComponent(requested_owner->template_primary);
			const vector<string>& concrete_arguments = scope->owner_type->template_arguments;
			const size_t requested_arguments = requested_owner->template_arguments.size();
			vector<string> expected_arguments;
			for (size_t argument = 0; argument < requested_arguments; ++argument) {
				TypePtr resolved;
				try { resolved = ResolveType(scope, requested_owner->template_arguments[argument]); }
				catch (const logic_error&) {}
				expected_arguments.push_back(resolved ? resolved->name :
					requested_owner->template_arguments[argument]);
			}
			const auto same_generated_argument = [](const string& left, const string& right) {
				if (left == right) return true;
				if (left.size() != right.size()) return false;
				for (size_t character = 0; character < left.size(); ++character)
					if (tolower(static_cast<unsigned char>(left[character])) !=
						tolower(static_cast<unsigned char>(right[character]))) return false;
				return true;
			};
			for (Scope* visible = scope; visible && !owner_scope; visible = visible->parent)
				for (size_t binding = 0; binding < visible->bindings.size() && !owner_scope; ++binding) {
					TypePtr candidate = visible->bindings[binding].type;
					if (visible->bindings[binding].kind != BIND_TYPE || !candidate ||
						candidate->kind != TYPE_CLASS || candidate->template_primary.empty() ||
						LastComponent(candidate->template_primary) != requested_primary ||
						candidate->template_arguments.size() != requested_arguments ||
						concrete_arguments.size() < requested_arguments) continue;
					bool same_arguments = true;
					for (size_t argument = 0; argument < requested_arguments; ++argument) {
						const string& expected = expected_arguments.empty() ? concrete_arguments[argument] :
							expected_arguments[argument];
						if (!same_generated_argument(candidate->template_arguments[argument], expected)) {
							same_arguments = false;
							break;
						}
					}
					if (same_arguments) owner_scope = ScopeForType(candidate);
				}
		}
		operator_owner_scope = owner_scope;
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
		// A generated class shell can contain replay bookkeeping bindings (for
		// example the synthetic `Derived` type) while its source member-template
		// declarations remain on the primary class scope.  Recover that typed
		// declaration scope for a using-id such as `operator,`.
		if (targets.empty() && owner_scope && owner_scope->owner_type &&
			!owner_scope->owner_type->template_primary.empty()) {
			TypePtr primary_type;
			try { primary_type = ResolveType(scope,
				owner_scope->owner_type->template_primary); }
			catch (const logic_error&) {}
			Scope* primary_scope = ScopeForType(primary_type);
			if (primary_scope) {
				for (size_t i = 0; i < primary_scope->bindings.size(); ++i)
					if (primary_scope->bindings[i].name == LastComponent(target_name))
						targets.push_back(&primary_scope->bindings[i]);
				if (!targets.empty()) operator_owner_scope = primary_scope;
			}
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
	// A conversion using-id can name the target through a derived alias:
	// `using base_type::operator integral_type;`.  The source base binding is
	// spelled with its own alias (`operator int_type`), so exact identifier
	// lookup is insufficient even though both conversion targets are the same
	// concrete type in the specialization.
	if (targets.empty() && operator_name && operator_owner_scope) {
		string requested = target_component.substr(8);
		while (!requested.empty() && isspace(static_cast<unsigned char>(requested[0])))
			requested.erase(0, 1);
		while (!requested.empty() && isspace(static_cast<unsigned char>(requested[requested.size() - 1])))
			requested.erase(requested.size() - 1);
		SpecFacts requested_facts;
		TypePtr requested_type;
		try { requested_type = ResolveSpelledType(requested, scope, requested_facts); }
		catch (const logic_error&) {}
		if (requested_type) for (size_t i = 0; i < operator_owner_scope->bindings.size(); ++i) {
			Binding& candidate = operator_owner_scope->bindings[i];
			if (candidate.kind != BIND_FUNCTION || candidate.name.compare(0, 8, "operator") != 0)
				continue;
			string available = candidate.name.substr(8);
			while (!available.empty() && isspace(static_cast<unsigned char>(available[0])))
				available.erase(0, 1);
			SpecFacts available_facts;
			TypePtr available_type;
			try { available_type = ResolveSpelledType(available, operator_owner_scope, available_facts); }
			catch (const logic_error&) {}
			if (available_type && SameTypeIgnoringTopCv(available_type, requested_type)) {
				targets.push_back(&candidate);
				break;
			}
		}
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
