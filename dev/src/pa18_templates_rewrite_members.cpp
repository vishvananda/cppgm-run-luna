#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
using namespace std;

namespace pa18_templates_internal {
bool PA18TemplateExpander::FindClassMemberType(const string& raw_class, const string& member,
	const map<string, string>& substitutions, const string& context,
	string* result, set<string>* active, bool aliases_only) const
{
	if(!result || !active) return false;
	string class_key = CanonicalSpelling(raw_class);
	// A few dependent replay paths pass the complete qualified member
	// spelling as `raw_class` while also supplying `member` separately.
	// Normalize that accidental duplicate before consulting specialization
	// identity; otherwise `case_<Tag>::proto_grammar` is keyed as if the
	// member itself were the class name.
	const string member_suffix = "::" + member;
	if(class_key.size() > member_suffix.size() &&
		class_key.compare(class_key.size() - member_suffix.size(),
			member_suffix.size(), member_suffix) == 0)
		class_key.erase(class_key.size() - member_suffix.size());
	bool object_const = false;
	while(class_key.compare(0, 6, "const ") == 0) {
		object_const = true;
		class_key = CanonicalSpelling(class_key.substr(6));
	}
	while(class_key.compare(0, 9, "volatile ") == 0)
		class_key = CanonicalSpelling(class_key.substr(9));
	while(!class_key.empty() && (class_key[class_key.size() - 1] == '&' ||
		class_key[class_key.size() - 1] == '*')) class_key.erase(class_key.size() - 1);
	class_key = CanonicalSpelling(class_key);
	while(class_key.size() > 6 && class_key.compare(class_key.size() - 6, 6,
		" const") == 0) {
		object_const = true;
		class_key = CanonicalSpelling(class_key.substr(0, class_key.size() - 6));
	}
	while(class_key.size() > 9 && class_key.compare(class_key.size() - 9, 9,
		" volatile") == 0)
		class_key = CanonicalSpelling(class_key.substr(0, class_key.size() - 9));
	// A member replay can leave the source argument list attached to an
	// already materialized generated name (`expr_<...><Tag,Args>`).  The
	// generated name is the nominal class key; discard only that redundant
	// argument suffix when its declaration is present in typed state.
	const size_t generated_open = class_key.find('<');
	if(generated_open != string::npos) {
		string generated_base;
		size_t generated_begin = 0;
		string generated_arguments;
		size_t generated_close = string::npos;
		if(TemplateBase(class_key, generated_open, &generated_begin, &generated_base) &&
			TemplateRange(class_key, generated_open, &generated_arguments, &generated_close) &&
			class_declarations_.find(generated_base) != class_declarations_.end() &&
			specialization_bases_.find(LastComponent(generated_base)) !=
			specialization_bases_.end())
			class_key = generated_base;
	}
	map<string, string> class_substitutions = substitutions;
	// Nested generated classes are indexed under their enclosing generated
	// specialization.  Restore the enclosing template bindings before
	// replaying an inherited member type from the nested class.
	map<string, string>::const_iterator nested_base = specialization_bases_.end();
	for(map<string, string>::const_iterator candidate = specialization_bases_.begin();
		candidate != specialization_bases_.end(); ++candidate)
		if(LastComponent(candidate->first) == LastComponent(class_key) &&
			!PrefixComponent(candidate->second).empty()) {
			nested_base = candidate;
			break;
		}
	if(nested_base != specialization_bases_.end()) {
		const string source_owner = PrefixComponent(nested_base->second);
		if(!source_owner.empty()) for(map<string, CPPGMAstNodePtr>::const_iterator declaration =
			class_declarations_.begin(); declaration != class_declarations_.end(); ++declaration) {
			if(LastComponent(declaration->first) != LastComponent(class_key)) continue;
			const string generated_owner = PrefixComponent(declaration->first);
			map<string, string>::const_iterator owner_base = specialization_bases_.find(
				LastComponent(generated_owner));
			map<string, vector<string> >::const_iterator owner_arguments =
				specialization_arguments_.find(LastComponent(generated_owner));
			if(owner_base == specialization_bases_.end() ||
				owner_arguments == specialization_arguments_.end() ||
				LastComponent(owner_base->second) != LastComponent(source_owner)) continue;
			const TemplateDefinition* owner_definition = FindDefinition(owner_base->second, context);
			if(!owner_definition || !owner_definition->class_template)
				owner_definition = FindDefinition(LastComponent(owner_base->second), context);
			if(!owner_definition || !owner_definition->class_template) continue;
			for(size_t parameter = 0; parameter < owner_definition->parameters.size() &&
				parameter < owner_arguments->second.size(); ++parameter)
				if(!owner_definition->parameters[parameter].name.empty())
					class_substitutions[owner_definition->parameters[parameter].name] =
						owner_arguments->second[parameter];
			break;
		}
	}
	// A direct template-id such as `node_value<key>` is looked up against the
	// primary declaration below, so its class-template arguments must also be
	// installed before replaying member return types.  Without this mapping
	// `node_value<T>::_M_v()` leaks its own `T` into the enclosing call
	// deduction instead of producing `const key&`.
	const size_t class_open = class_key.find('<');
	if(class_open != string::npos) {
		string class_argument_text;
		size_t class_close = string::npos;
		string class_base;
		size_t class_begin = 0;
		if(TemplateBase(class_key, class_open, &class_begin, &class_base) &&
			TemplateRange(class_key, class_open, &class_argument_text, &class_close)) {
			const TemplateDefinition* class_definition = FindDefinition(class_base, context);
			if(class_definition && class_definition->class_template) {
				const vector<string> class_arguments = SplitTemplateArguments(
					class_argument_text);
				const TemplateDefinition* selected_class = SelectClassTemplateDefinition(
					class_definition, class_arguments, context);
				if(selected_class) {
					class_definition = selected_class;
					// A nested class template is declared in the scope of an
					// enclosing specialization (`cases::case_<Tag>`).  Its own
					// parameter list does not carry the enclosing `Gram` binding,
					// so recover that binding from the concrete owner visible in
					// the current substitution map before following inherited
					// aliases such as `proto_grammar`.
					const size_t owner_open = selected_class->owner.find('<');
					if(owner_open != string::npos) {
						string owner_arguments_text;
						size_t owner_close = string::npos;
						if(TemplateRange(selected_class->owner, owner_open,
							&owner_arguments_text, &owner_close)) {
							const string owner_base = selected_class->owner.substr(0, owner_open);
							vector<string> owner_arguments = SplitTemplateArguments(
								owner_arguments_text);
							for(size_t owner_argument = 0; owner_argument < owner_arguments.size();
								++owner_argument) {
								owner_arguments[owner_argument] = CanonicalSpelling(
									ReplaceIdentifiers(owner_arguments[owner_argument], substitutions));
								owner_arguments[owner_argument] = ResolveAlias(
									owner_arguments[owner_argument], context);
							}
							// The outer class is often available as a single dependent
							// substitution (`Cases -> cases<char_type, grammar<...>>`),
							// rather than as separate `Char`/`Gram` entries.
							for(map<string, string>::const_iterator substitution = substitutions.begin();
								substitution != substitutions.end(); ++substitution) {
								const string value = CanonicalSpelling(substitution->second);
								const size_t value_open = value.find('<');
								string value_base, value_arguments_text;
								size_t value_begin = 0, value_close = string::npos;
								if(value_open == string::npos || !TemplateBase(value, value_open,
									&value_begin, &value_base) || !TemplateRange(value, value_open,
									&value_arguments_text, &value_close) ||
									LastComponent(value_base) != LastComponent(owner_base)) continue;
								owner_arguments = SplitTemplateArguments(value_arguments_text);
								break;
							}
							const TemplateDefinition* owner_definition = FindDefinition(owner_base,
								context);
							if(!owner_definition)
								owner_definition = FindDefinition(LastComponent(owner_base), context);
							if(owner_definition && owner_definition->class_template) {
								const TemplateDefinition* selected_owner = SelectClassTemplateDefinition(
									owner_definition, owner_arguments, context);
								if(selected_owner) {
									for(size_t parameter = 0; parameter < selected_owner->parameters.size() &&
										parameter < owner_arguments.size(); ++parameter)
										if(!selected_owner->parameters[parameter].name.empty())
											class_substitutions[selected_owner->parameters[parameter].name] =
												owner_arguments[parameter];
									map<string, string> owner_specialized;
									if(selected_owner->partial_specialization &&
										MatchClassSpecializationPattern(*selected_owner, owner_arguments,
											&owner_specialized, context))
										for(map<string, string>::const_iterator binding = owner_specialized.begin();
											binding != owner_specialized.end(); ++binding)
												class_substitutions[binding->first] = binding->second;
						}
				}
							}
					} else if(!selected_class->owner.empty()) {
						// Some parser scopes retain the nested owner as the
						// unparameterized spelling (`cases::cases`).  The concrete
						// arguments are still recoverable from a substitution whose
						// value is the enclosing class template-id.
						const string owner_base = selected_class->owner;
						vector<string> owner_arguments;
						for(map<string, string>::const_iterator substitution = substitutions.begin();
							substitution != substitutions.end() && owner_arguments.empty(); ++substitution) {
							const string value = CanonicalSpelling(substitution->second);
							const size_t value_open = value.find('<');
							string value_base, value_arguments_text;
							size_t value_begin = 0, value_close = string::npos;
							if(value_open == string::npos || !TemplateBase(value, value_open,
								&value_begin, &value_base) || !TemplateRange(value, value_open,
								&value_arguments_text, &value_close) ||
								LastComponent(value_base) != LastComponent(owner_base)) continue;
							owner_arguments = SplitTemplateArguments(value_arguments_text);
						}
						const TemplateDefinition* owner_definition = FindDefinition(owner_base, context);
						if(!owner_definition)
							owner_definition = FindDefinition(LastComponent(owner_base), context);
						if(owner_definition && owner_definition->class_template) {
							const TemplateDefinition* selected_owner = SelectClassTemplateDefinition(
								owner_definition, owner_arguments, context);
							if(selected_owner) {
								for(size_t parameter = 0; parameter < selected_owner->parameters.size() &&
									parameter < owner_arguments.size(); ++parameter)
									if(!selected_owner->parameters[parameter].name.empty())
										class_substitutions[selected_owner->parameters[parameter].name] =
											owner_arguments[parameter];
							}
						}
					}
					}
					for(size_t parameter = 0; parameter < class_definition->parameters.size() &&
						parameter < class_arguments.size(); ++parameter)
						if(!class_definition->parameters[parameter].name.empty()) {
							const string argument = CanonicalSpelling(ReplaceIdentifiers(
								class_arguments[parameter], class_substitutions));
							class_substitutions[class_definition->parameters[parameter].name] =
								class_definition->parameters[parameter].template_template ? argument :
								CanonicalSpelling(ResolveAlias(argument, context));
						}
				if(!class_definition->name.empty()) class_substitutions[class_definition->name] =
					class_key;
					// Prefer the already replayed declaration for a concrete
					// template-id.  Falling back to the source partial here loses
					// the complete binding of a parameter pack and turns
					// `plus<T...>::type` into the first element's result.
					map<string, vector<string> >::const_iterator generated_names =
						specialization_names_by_base_.find(LastComponent(
							selected_class->qualified_name));
					if(generated_names != specialization_names_by_base_.end())
						for(size_t generated_index = 0;
							generated_index < generated_names->second.size(); ++generated_index) {
							const string& generated_name = generated_names->second[generated_index];
							map<string, string>::const_iterator generated_base =
								specialization_bases_.find(generated_name);
							map<string, vector<string> >::const_iterator generated_arguments =
								specialization_arguments_.find(generated_name);
							if(generated_base == specialization_bases_.end() ||
								generated_arguments == specialization_arguments_.end() ||
								generated_arguments->second.size() != class_arguments.size() ||
								LastComponent(generated_base->second) !=
								LastComponent(selected_class->qualified_name)) continue;
							bool same_arguments = true;
							for(size_t argument = 0; argument < class_arguments.size(); ++argument) {
								const string actual = NormalizeTypeArgument(ResolveAlias(
									ReplaceIdentifiers(class_arguments[argument], substitutions), context));
								const string expected = NormalizeTypeArgument(CanonicalSpelling(
									generated_arguments->second[argument]));
								if(actual != expected) {
									same_arguments = false;
									break;
								}
							}
							if(!same_arguments) continue;
							const string generated_path = JoinPath(selected_class->owner, generated_name);
							map<string, CPPGMAstNodePtr>::const_iterator generated_declaration =
								class_declarations_.find(generated_path);
							if(generated_declaration == class_declarations_.end() ||
								!generated_declaration->second ||
								generated_declaration->second->children.size() <= 1) continue;
							if(FindClassMemberType(generated_path, member, substitutions, context,
								result, active, aliases_only)) {
								return true;
							}
						}
					}
			}
		}
	map<string, string>::const_iterator specialization = specialization_bases_.find(
		LastComponent(class_key));
	map<string, CPPGMAstNodePtr>::const_iterator concrete_declaration =
		class_declarations_.find(class_key);
	const bool incomplete_concrete = concrete_declaration == class_declarations_.end() ||
		(concrete_declaration->second &&
			(concrete_declaration->second->kind == "class-forward-declaration" ||
				concrete_declaration->second->children.size() <= 1));
	map<string, vector<string> >::const_iterator specialization_arguments =
		specialization_arguments_.find(LastComponent(class_key));
	const TemplateDefinition* specialization_definition =
		specialization == specialization_bases_.end() ? 0 :
		FindDefinition(specialization->second, context);
	// Materialized class declarations are complete by this point, but member
	// return types still use aliases from the source class template.  Carry the
	// concrete class arguments into that lookup even when no forward shell is
	// being replaced; otherwise `begin()` returns the spelling `iterator` and
	// a dependent member operator cannot deduce its other class parameter.
	if(specialization_definition && specialization_arguments !=
		specialization_arguments_.end()) {
		for(size_t parameter = 0; parameter < specialization_definition->parameters.size() &&
			parameter < specialization_arguments->second.size(); ++parameter)
			if(!specialization_definition->parameters[parameter].name.empty())
				class_substitutions[specialization_definition->parameters[parameter].name] =
					specialization_arguments->second[parameter];
		if(!specialization_definition->name.empty())
			class_substitutions[specialization_definition->name] = class_key;
	}
	if(specialization != specialization_bases_.end() && incomplete_concrete)
		class_key = specialization->second;
	const string active_key = class_key + "|" + member;
	if(!active->insert(active_key).second) return false;
	CPPGMAstNodePtr declaration = FindClassDeclaration(class_key, context);
	if(!declaration) {
		active->erase(active_key);
		return false;
	}
	const string declaration_context = PrefixComponent(class_key).empty() ?
		context : PrefixComponent(class_key);
	string fallback_type;
	for(size_t i = 0; i < declaration->children.size(); ++i) {
		const CPPGMAstNodePtr child = declaration->children[i];
		if(!child) continue;
		if(!aliases_only && child->kind == "function-definition" &&
			child->children.size() > 1 &&
			LastComponent(FirstIdentifierLocal(child->children[1])) == member) {
			string type = NodeTypeSpelling(child->children[0]) +
				ReturnDeclaratorSuffix(child->children[1]);
			const bool function_const = DeclaratorSuffix(child->children[1]).find("const") != string::npos;
			if(function_const != object_const) {
				if(!object_const && fallback_type.empty())
					fallback_type = type;
				continue;
			}
				*result = CanonicalSpelling(ReplaceIdentifiers(type, class_substitutions));
				active->erase(active_key);
			return !result->empty();
		}
		if(child->kind != "simple-declaration" || child->children.empty()) continue;
		if(aliases_only && SpellNode(child->children[0]).find("typedef") == string::npos)
			continue;
		const string base = NodeTypeSpelling(child->children[0]);
		const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
		if(!list) continue;
		for(size_t item = 0; item < list->children.size(); ++item) {
			const CPPGMAstNodePtr init = list->children[item];
			if(!init || init->children.empty()) continue;
			if(aliases_only && DescendantOfKind(init->children[0], "parameter-clause"))
				continue;
			if(LastComponent(FirstIdentifierLocal(init->children[0])) != member) continue;
				*result = CanonicalSpelling(ReplaceIdentifiers(
					DeclaratorTypeSpelling(base, init->children[0]), class_substitutions));
				active->erase(active_key);
			return !result->empty();
		}
	}
	if(!fallback_type.empty()) {
		*result = CanonicalSpelling(ReplaceIdentifiers(fallback_type, class_substitutions));
		active->erase(active_key);
		return !result->empty();
	}
	for(size_t i = 0; i < declaration->children.size(); ++i) {
		const CPPGMAstNodePtr child = declaration->children[i];
		if(!child || child->kind != "base-clause") continue;
		for(size_t base_index = 0; base_index < child->children.size(); ++base_index) {
			const CPPGMAstNodePtr base_specifier = child->children[base_index];
			const CPPGMAstNodePtr base_name = ChildOfKindLocal(base_specifier, "base-name");
			if(!base_name) continue;
			string base_spelling = NormalizeElaboratedSpelling(
				ReplaceIdentifiers(base_name->value, class_substitutions), declaration_context);
			base_spelling = CanonicalSpelling(base_spelling);
			const size_t open = base_spelling.find('<');
			const TemplateDefinition* base_definition = 0;
			map<string, string> base_substitutions;
			string base_lookup = base_spelling;
			if(open != string::npos) {
				string argument_text;
				size_t close = string::npos;
				if(!TemplateRange(base_spelling, open, &argument_text, &close)) continue;
				base_lookup = base_spelling.substr(0, open);
				base_definition = FindDefinition(base_lookup, declaration_context);
				if(base_definition) {
					const vector<string> arguments = SplitTemplateArguments(argument_text);
					for(size_t parameter = 0; parameter < base_definition->parameters.size() &&
						parameter < arguments.size(); ++parameter)
						if(!base_definition->parameters[parameter].name.empty())
							base_substitutions[base_definition->parameters[parameter].name] =
								QualifyTypeArgument(NormalizeElaboratedSpelling(
								ReplaceIdentifiers(arguments[parameter], class_substitutions), declaration_context),
									declaration_context, base_definition->owner);
				}
			}
			if(base_definition) base_lookup = base_definition->qualified_name;
			if(FindClassMemberType(base_lookup, member, base_substitutions,
				declaration_context, result, active, aliases_only)) {
				active->erase(active_key);
				return true;
			}
		}
	}
	active->erase(active_key);
	return false;
}


} // namespace pa18_templates_internal
