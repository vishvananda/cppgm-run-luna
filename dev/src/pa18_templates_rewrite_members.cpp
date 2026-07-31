#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
using namespace std;

namespace pa18_templates_internal {

namespace {

string CollapseRepeatedMemberPaths(string value)
{
	bool changed = false;
	do {
		changed = false;
		for(size_t start = 0; !changed && start < value.size(); ++start)
			for(size_t separator = value.find("::", start);
				separator != string::npos;
				separator = value.find("::", separator + 2)) {
				const size_t prefix_size = separator + 2 - start;
				if(separator + 2 + prefix_size > value.size() ||
					value.compare(separator + 2, prefix_size, value, start,
						prefix_size) != 0) continue;
				value.erase(separator + 2, prefix_size);
				changed = true;
				break;
			}
	} while(changed);
	return value;
}

} // namespace



bool PA18TemplateExpander::FindClassMemberType(const string& raw_class, const string& member,
	const map<string, string>& substitutions, const string& context,
	string* result, set<string>* active, bool aliases_only) const
{
	if(!result || !active) return false;
	const bool unresolved_lookup = HasUnresolvedTemplateParameter(raw_class, context, substitutions) ||
		HasUnresolvedTemplateParameter(member, context, substitutions);
	if(unresolved_lookup) {
		return false;
	}
	// ResolveAlias and partial-specialization selection can ask for the same
	// member before the owner has acquired its generated declaration.  Guard the
	// owner/member identity before any alias or class-template probe; the later
	// normalized guard is too late for that re-entry and lets replay contexts
	// manufacture a fresh key at every nesting level.
	ostringstream substitution_key_stream;
	for(map<string, string>::const_iterator binding = substitutions.begin();
		binding != substitutions.end(); ++binding)
		substitution_key_stream << "|" << binding->first << "=" <<
			CanonicalSpelling(binding->second);
	const string substitution_key = substitution_key_stream.str();
	const string raw_lookup_key = "raw|" + CollapseRepeatedQualifiedPath(
		CollapseRepeatedQualifier(CanonicalSpelling(RestoreSpecializationSpelling(raw_class)))) +
		"|" + member + "|" + context + substitution_key;
	if(!active_member_type_lookups_.insert(raw_lookup_key).second) {
		return false;
	}
	struct RawLookupScope {
		set<string>* active; string key;
		RawLookupScope(set<string>* value, const string& name) : active(value), key(name) {}
		~RawLookupScope() { active->erase(key); }
	} raw_lookup_scope(&active_member_type_lookups_, raw_lookup_key);
	// Member lookup owns the dependent owner spelling.  Resolve its typed
	// bindings for every probe instead of using helper-name text as a proxy for
	// dependency; aliases and user-defined traits take the same semantic path.
	// A materialized specialization is represented by both its generated owner
	// name and its source template-id.  Do not replace the source base first:
	// doing so turns `ListSet<int>` into `ListSet_int_<int>`, after which the
	// nested argument parser can no longer recover the specialization identity.
	map<string, string> class_key_substitutions = substitutions;
	ProtectMaterializedTemplateBases(raw_class, context, substitutions,
		&class_key_substitutions);
	string class_key = CanonicalSpelling(ReplaceIdentifiersPreservingPackSizes(
		raw_class, class_key_substitutions));
	// `aliases_only` is used while rewriting a qualified type spelling.  A
	// static value member is intentionally left as an expression for the
	// integral evaluator; selecting every partial specialization merely to ask
	// for its type can re-enter a callable's own `is_applyable<T>::value` query.
	if(aliases_only && member == "value") return false;
	if(aliases_only) {
		map<string, set<string> >::const_iterator indexed_static =
			static_members_by_class_.find(class_key);
		if(indexed_static != static_members_by_class_.end() &&
			indexed_static->second.find(member) != indexed_static->second.end()) return false;
		string static_base = class_key;
		const size_t static_open = static_base.find('<');
		if(static_open != string::npos) static_base.erase(static_open);
		const TemplateDefinition* static_definition = FindDefinition(static_base, context);
		if(static_definition) {
			if(static_definition->static_members.find(member) !=
				static_definition->static_members.end()) return false;
			if(static_definition->declaration) for(size_t child_index = 0;
				child_index < static_definition->declaration->children.size(); ++child_index) {
				CPPGMAstNodePtr child = static_definition->declaration->children[child_index];
				while(child && child->kind == "template-declaration" &&
					child->children.size() > 1) child = child->children[1];
				if(!child || child->kind != "simple-declaration" || child->children.empty() ||
					(!HasDeclarationSpecifier(child->children[0], "const") &&
					 !HasDeclarationSpecifier(child->children[0], "constexpr"))) continue;
				const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
				if(!list) continue;
				for(size_t item = 0; item < list->children.size(); ++item)
					if(list->children[item] && !list->children[item]->children.empty() &&
						LastComponent(FirstIdentifierLocal(
							list->children[item]->children[0])) == member) return false;
			}
		}
	}
	// A generated replay can duplicate a qualified owner while redirecting a
	// dependent member query.  Collapse only when the normalized spelling is a
	// known generated specialization and the original spelling is not a source
	// declaration; a real lexical path such as `a::a::type` remains distinct.
	const string normalized_generated_key = CollapseRepeatedQualifiedPath(
		CollapseRepeatedQualifier(class_key));
	const bool normalized_is_generated =
		normalized_generated_key != class_key &&
		specialization_bases_.find(LastComponent(normalized_generated_key)) !=
			specialization_bases_.end() &&
		specialization_arguments_.find(LastComponent(normalized_generated_key)) !=
			specialization_arguments_.end();
	const bool original_is_known = class_contexts_.find(class_key) !=
		class_contexts_.end() || class_declarations_.find(class_key) !=
		class_declarations_.end();
	if(normalized_is_generated && !original_is_known) class_key = normalized_generated_key;
	if(class_key.find("::") == string::npos) {
		const string resolved_class_key = CanonicalSpelling(ResolveAlias(class_key, context));
		if(!resolved_class_key.empty() && resolved_class_key != class_key)
			class_key = resolved_class_key;
	}
	const string lookup_key = class_key + "|" + member + "|" + context + substitution_key;
	if(!active_member_type_lookups_.insert(lookup_key).second) {
		return false;
	}
	struct LookupScope {
		set<string>* active; string key;
		LookupScope(set<string>* value, const string& name) : active(value), key(name) {}
		~LookupScope() { active->erase(key); }
	} lookup_scope(&active_member_type_lookups_, lookup_key);
	// A member-template specialization is a materialized class in its own
	// right.  During a nested replay the caller can ask for the generated
	// member component (`owner::impl_<args>`) before the source partial's
	// unparameterized `impl` declaration is visited.  Let the typed declaration
	// index answer that request directly; reducing it to the source owner would
	// lose the member's concrete template arguments and make the next
	// `::result_type` lookup start from an unrelated primary.
	const string generated_member_path = JoinPath(class_key, member);
	map<string, CPPGMAstNodePtr>::const_iterator generated_member =
		class_declarations_.find(generated_member_path);
	if(generated_member != class_declarations_.end() && generated_member->second &&
		(generated_member->second->kind == "class-specifier" ||
			 generated_member->second->kind == "class-forward-declaration")) {
		*result = generated_member_path;
		return true;
	}
	for(size_t template_marker = class_key.find("template ");
		template_marker != string::npos;
		template_marker = class_key.find("template ", template_marker))
		class_key.erase(template_marker, 9);
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
	// A member of a concrete template-id may be queried before the generated
	// class has been substituted into the spelling.  Match that source
	// template-id against the typed specialization registry and use the
	// materialized declaration as the lookup owner.
	const size_t source_template_open = class_key.find('<');
	if(source_template_open != string::npos) {
		string source_template_base, source_argument_text;
		size_t source_template_begin = 0, source_template_close = string::npos;
		if(TemplateBase(class_key, source_template_open, &source_template_begin,
			&source_template_base) && TemplateRange(class_key, source_template_open,
			&source_argument_text, &source_template_close)) {
			vector<string> requested_arguments = SplitTemplateArguments(source_argument_text);
			for(size_t argument = 0; argument < requested_arguments.size(); ++argument)
				requested_arguments[argument] = CollapseRepeatedQualifiedPath(CollapseRepeatedQualifier(
					NormalizeTypeArgument(RestoreSpecializationSpelling(requested_arguments[argument]))));
			map<string, set<string> >::const_iterator generated_names =
				specialization_name_sets_by_base_.find(LastComponent(source_template_base));
			if(generated_names != specialization_name_sets_by_base_.end()) {
				for(set<string>::const_iterator candidate = generated_names->second.begin();
					candidate != generated_names->second.end(); ++candidate) {
					const string& generated_name = *candidate;
					map<string, string>::const_iterator generated =
						specialization_bases_.find(generated_name);
					if(generated == specialization_bases_.end()) continue;
					string generated_source = generated->second;
					const size_t generated_source_open = generated_source.find('<');
					if(generated_source_open != string::npos) generated_source.erase(generated_source_open);
					if(generated_source != source_template_base &&
						(LastComponent(generated_source) != LastComponent(source_template_base) ||
							PrefixComponent(generated_source) != PrefixComponent(source_template_base))) continue;
					map<string, vector<string> >::const_iterator concrete_arguments =
						specialization_arguments_.find(generated_name);
					if(concrete_arguments == specialization_arguments_.end() ||
						concrete_arguments->second.size() < requested_arguments.size()) continue;
					bool same_arguments = true;
					for(size_t argument = 0; argument < requested_arguments.size(); ++argument) {
						if(CollapseRepeatedQualifiedPath(CollapseRepeatedQualifier(NormalizeTypeArgument(
							RestoreSpecializationSpelling(concrete_arguments->second[argument])))) !=
							requested_arguments[argument]) {
							same_arguments = false; break;
						}
					}
					if(!same_arguments) continue;
					map<string, vector<string> >::const_iterator indexed_paths =
						class_paths_by_name_.find(generated_name);
					if(indexed_paths != class_paths_by_name_.end()) {
						string selected_path;
						for(size_t path_index = 0; path_index < indexed_paths->second.size(); ++path_index) {
							const string& path = indexed_paths->second[path_index];
							if(class_declarations_.find(path) != class_declarations_.end() &&
								(selected_path.empty() || path < selected_path)) selected_path = path;
						}
						if(!selected_path.empty()) class_key = selected_path;
					}
					if(class_declarations_.find(class_key) != class_declarations_.end()) break;
				}
			}
			if(class_declarations_.find(class_key) == class_declarations_.end()) {
				const string collapsed_source = CollapseRepeatedQualifiedPath(
					CollapseRepeatedQualifier(source_template_base));
				if(collapsed_source == source_template_base) {
					string materialized_owner;
						if(ResolveMaterializedClassOwner(source_template_base, requested_arguments,
							context, &materialized_owner, substitutions)) class_key = materialized_owner;
				}
			}
			// A source class whose requested alias contains a dependent member
			// call needs a concrete owner while that call is evaluated.  Materialize
			// only this expression-bearing case; eagerly materializing every source
			// template-id changes lookup and overload visibility for ordinary aliases.
			const TemplateDefinition* source_definition = FindDefinition(
				source_template_base, context);
				if(source_definition && source_definition->class_template) {
					const TemplateDefinition* selected_source = SelectClassTemplateDefinition(
						source_definition, requested_arguments, context);
					if(selected_source) source_definition = selected_source;
				string source_member_type;
				if(source_definition->declaration) for(size_t child_index = 0;
					child_index < source_definition->declaration->children.size(); ++child_index) {
					CPPGMAstNodePtr child = source_definition->declaration->children[child_index];
					if(!child) continue;
					while(child->kind == "template-declaration" && child->children.size() > 1)
						child = child->children[1];
					if(child->kind == "alias-declaration" &&
						LastComponent(RemoveMarker(child->value)) == member &&
						!child->children.empty()) {
						source_member_type = TypeIdSpelling(child->children[0]);
						break;
					}
					if(child->kind != "simple-declaration" || child->children.empty() ||
						!HasDeclarationSpecifier(child->children[0], "typedef")) continue;
					const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
					if(!list) continue;
					for(size_t item_index = 0; item_index < list->children.size(); ++item_index) {
						const CPPGMAstNodePtr item = list->children[item_index];
						if(!item || item->children.empty() ||
							LastComponent(FirstIdentifierLocal(item->children[0])) != member) continue;
						source_member_type = DeclaratorTypeSpelling(
							NodeTypeSpelling(child->children[0]), item->children[0]);
						break;
					}
					if(!source_member_type.empty()) break;
				}
				bool contains_call = false;
				for(size_t at = source_member_type.find('('); at != string::npos;
					at = source_member_type.find('(', at + 1)) {
					size_t before = at;
					while(before > 0 && isspace(static_cast<unsigned char>(source_member_type[before - 1])))
						--before;
					if(before > 0 && IsIdentifierCharacter(source_member_type[before - 1])) {
						contains_call = true;
						break;
					}
				}
					const bool member_template_request = member.find('<') != string::npos ||
						member.find("template ") != string::npos;
					const bool incomplete_source_owner = class_declarations_.find(class_key) !=
						class_declarations_.end() && class_declarations_.find(class_key)->second &&
						class_declarations_.find(class_key)->second->kind ==
							"class-forward-declaration";
					if((contains_call || (member_template_request && incomplete_source_owner))) try {
						const string generated = const_cast<PA18TemplateExpander*>(this)->Instantiate(
							*source_definition, requested_arguments, context, false, 0,
							&substitutions);
					const string qualified_generated = GeneratedOwner(*source_definition).empty() ? generated :
						JoinPath(GeneratedOwner(*source_definition), generated);
					if(class_declarations_.find(qualified_generated) != class_declarations_.end())
						class_key = qualified_generated;
					else if(class_declarations_.find(generated) != class_declarations_.end())
						class_key = generated;
				} catch(const PA18SubstitutionFailure&) {}
			}
			}
		}
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
				specialization_bases_.find(LastComponent(generated_base)) !=
					specialization_bases_.end()) {
				string qualified_generated;
				if(class_declarations_.find(generated_base) != class_declarations_.end())
					class_key = generated_base;
				map<string, vector<string> >::const_iterator indexed_paths =
					class_paths_by_name_.find(LastComponent(generated_base));
				if(indexed_paths != class_paths_by_name_.end()) for(size_t path = 0;
					path < indexed_paths->second.size(); ++path) {
					const string& candidate = indexed_paths->second[path];
					if(class_declarations_.find(candidate) == class_declarations_.end()) continue;
					if(qualified_generated.empty()) qualified_generated = candidate;
					else if(qualified_generated != candidate) {
						qualified_generated.clear();
						break;
					}
				}
				if(!qualified_generated.empty()) class_key = qualified_generated;
			}
		}
		if(class_declarations_.find(class_key) == class_declarations_.end() &&
			specialization_bases_.find(LastComponent(class_key)) != specialization_bases_.end()) {
			map<string, vector<string> >::const_iterator indexed_paths =
				class_paths_by_name_.find(LastComponent(class_key));
			if(indexed_paths != class_paths_by_name_.end()) {
				string selected_path;
				for(size_t path = 0; path < indexed_paths->second.size(); ++path) {
					const string& candidate = indexed_paths->second[path];
					if(class_declarations_.find(candidate) == class_declarations_.end() ||
						(!selected_path.empty() && selected_path < candidate)) continue;
					selected_path = candidate;
				}
				if(!selected_path.empty()) class_key = selected_path;
			}
		}
		map<string, string> class_substitutions = substitutions;
		ActivePackScope active_packs(const_cast<PA18TemplateExpander*>(this));
	// Nested generated classes are indexed under their enclosing generated
	// specialization.  Restore the enclosing template bindings before
	// replaying an inherited member type from the nested class.
		map<string, string>::const_iterator nested_base = specialization_bases_.find(
			LastComponent(class_key));
		if(nested_base != specialization_bases_.end() &&
			PrefixComponent(nested_base->second).empty()) nested_base = specialization_bases_.end();
		if(nested_base != specialization_bases_.end()) {
			const string source_owner = PrefixComponent(nested_base->second);
			if(!source_owner.empty()) {
				string selected_path;
				map<string, string>::const_iterator selected_owner_base = specialization_bases_.end();
				map<string, vector<string> >::const_iterator selected_owner_arguments =
					specialization_arguments_.end();
				map<string, vector<string> >::const_iterator indexed_paths =
					class_paths_by_name_.find(LastComponent(class_key));
				if(indexed_paths != class_paths_by_name_.end()) for(size_t path = 0;
					path < indexed_paths->second.size(); ++path) {
					const string& candidate_path = indexed_paths->second[path];
					if(class_declarations_.find(candidate_path) == class_declarations_.end()) continue;
					const string generated_owner = PrefixComponent(candidate_path);
					map<string, string>::const_iterator owner_base = specialization_bases_.find(
						LastComponent(generated_owner));
					map<string, vector<string> >::const_iterator owner_arguments =
						specialization_arguments_.find(LastComponent(generated_owner));
					if(owner_base == specialization_bases_.end() ||
						owner_arguments == specialization_arguments_.end() ||
						LastComponent(owner_base->second) != LastComponent(source_owner)) continue;
					if(!selected_path.empty() && selected_path < candidate_path) continue;
					selected_path = candidate_path;
					selected_owner_base = owner_base;
					selected_owner_arguments = owner_arguments;
				}
				if(selected_owner_base != specialization_bases_.end()) {
					const TemplateDefinition* owner_definition = FindDefinition(
						selected_owner_base->second, context);
					if(!owner_definition || !owner_definition->class_template)
						owner_definition = FindDefinition(LastComponent(selected_owner_base->second), context);
					if(owner_definition && owner_definition->class_template) {
						// The recorded owner arguments are the arguments of the class
						// template-id, not the parameter list of a partial
						// specialization.  A callable specialization such as
						// `call<Fun(A0)>` therefore has one actual argument but two
						// specialization parameters.  Positional binding here maps
						// `Fun` to the complete function type and leaves `A0`
						// unresolved, corrupting every nested member replay.
						const TemplateDefinition* owner_primary = owner_definition;
						const size_t owner_open = selected_owner_base->second.find('<');
						if(owner_open != string::npos) {
							const string owner_base = selected_owner_base->second.substr(0, owner_open);
							const TemplateDefinition* source_primary = FindDefinition(owner_base, context);
							if(source_primary && source_primary->class_template)
								owner_primary = source_primary;
						}
						const TemplateDefinition* selected_owner = SelectClassTemplateDefinition(
							owner_primary, selected_owner_arguments->second, context);
						if(selected_owner && selected_owner->partial_specialization) {
							map<string, string> specialized;
							if(MatchClassSpecializationPattern(*selected_owner,
								selected_owner_arguments->second, &specialized, context))
								for(map<string, string>::const_iterator binding = specialized.begin();
									binding != specialized.end(); ++binding)
									class_substitutions[binding->first] = binding->second;
						} else if(selected_owner) {
							for(size_t parameter = 0; parameter < selected_owner->parameters.size() &&
								parameter < selected_owner_arguments->second.size(); ++parameter)
								if(!selected_owner->parameters[parameter].name.empty())
									class_substitutions[selected_owner->parameters[parameter].name] =
										selected_owner_arguments->second[parameter];
						}
					}
				}
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
					size_t class_argument_index = 0;
					for(size_t parameter = 0; parameter < class_definition->parameters.size(); ++parameter) {
						const TemplateParameter& template_parameter = class_definition->parameters[parameter];
						if(template_parameter.pack) {
							vector<string> values;
							size_t trailing_fixed = 0;
							for(size_t later = parameter + 1;
								later < class_definition->parameters.size(); ++later)
								if(!class_definition->parameters[later].pack) ++trailing_fixed;
							const size_t limit = class_arguments.size() > trailing_fixed ?
								class_arguments.size() - trailing_fixed : class_argument_index;
							while(class_argument_index < limit)
								values.push_back(class_arguments[class_argument_index++]);
							active_packs.Set(template_parameter.name, values);
							if(!template_parameter.name.empty() && !values.empty() &&
								class_substitutions.find(template_parameter.name) == class_substitutions.end())
								class_substitutions[template_parameter.name] = values[0];
						} else if(class_argument_index < class_arguments.size()) {
							++class_argument_index;
						}
					}
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
						const auto preserves_materialized_argument = [&](const string& spelling) {
							if(spelling.empty() || spelling.find_first_not_of(
								"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != string::npos)
								return false;
							if(class_contexts_.find(spelling) == class_contexts_.end() &&
								!FindClassDeclaration(spelling, context)) return false;
							for(map<string, string>::const_iterator binding = class_substitutions.begin();
								binding != class_substitutions.end(); ++binding)
								if(binding->first != spelling && binding->second == spelling) return true;
							return false;
						};
						for(size_t parameter = 0; parameter < class_definition->parameters.size() &&
							parameter < class_arguments.size(); ++parameter)
						if(!class_definition->parameters[parameter].name.empty()) {
							map<string, string> argument_substitutions = class_substitutions;
							ProtectMaterializedTemplateBases(class_arguments[parameter], context,
								class_substitutions, &argument_substitutions);
							const string argument = preserves_materialized_argument(class_arguments[parameter]) ?
								CanonicalSpelling(class_arguments[parameter]) : CanonicalSpelling(ReplaceIdentifiers(
								class_arguments[parameter], argument_substitutions));
							class_substitutions[class_definition->parameters[parameter].name] =
								class_definition->parameters[parameter].template_template ? argument :
								CanonicalSpelling(ResolveAlias(argument, context));
						}
					if(!class_definition->name.empty()) class_substitutions[class_definition->name] =
						class_key;
					if(selected_class && selected_class->partial_specialization &&
						!class_arguments.empty()) for(size_t pattern = 0;
						pattern < selected_class->specialization_pattern.size(); ++pattern) {
						string cv_name = CanonicalSpelling(
							selected_class->specialization_pattern[pattern]);
						bool trailing_const = cv_name.size() > 5 &&
							cv_name.compare(cv_name.size() - 5, 5, "const") == 0;
						bool trailing_volatile = cv_name.size() > 8 &&
							cv_name.compare(cv_name.size() - 8, 8, "volatile") == 0;
						if(!trailing_const && !trailing_volatile) continue;
						const size_t qualifier_size = trailing_const ? 5 : 8;
						cv_name.erase(cv_name.size() - qualifier_size);
						while(!cv_name.empty() && isspace(static_cast<unsigned char>(cv_name[cv_name.size() - 1])))
							cv_name.erase(cv_name.size() - 1);
						bool known_parameter = false;
						for(size_t parameter = 0; parameter < selected_class->specialization_parameters.size(); ++parameter)
							if(selected_class->specialization_parameters[parameter] == cv_name)
								known_parameter = true;
						if(!known_parameter) continue;
						string actual = CanonicalSpelling(class_arguments[0]);
						const string qualifier = trailing_const ? " const" : " volatile";
						if(actual.size() <= qualifier.size() || actual.compare(actual.size() - qualifier.size(),
							qualifier.size(), qualifier) != 0) continue;
						class_substitutions[cv_name] = CanonicalSpelling(
							actual.substr(0, actual.size() - qualifier.size()));
					}
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
						map<string, string> argument_substitutions = substitutions;
						ProtectMaterializedTemplateBases(class_arguments[argument], context,
							substitutions, &argument_substitutions);
									const string actual = CollapseRepeatedQualifier(NormalizeTypeArgument(
										RestoreSpecializationSpelling(ResolveAlias(ReplaceIdentifiers(
											class_arguments[argument], argument_substitutions), context))));
								const string expected = CollapseRepeatedQualifier(NormalizeTypeArgument(
									RestoreSpecializationSpelling(generated_arguments->second[argument])));
								if(actual != expected) {
									same_arguments = false;
									break;
								}
							}
							if(!same_arguments) continue;
						const string generated_path = JoinPath(GeneratedOwner(*selected_class), generated_name);
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
	CPPGMAstNodePtr selected_specialization_declaration;
	// Materialized class declarations are complete by this point, but member
	// return types still use aliases from the source class template.  Carry the
	// concrete class arguments into that lookup even when no forward shell is
	// being replaced; otherwise `begin()` returns the spelling `iterator` and
	// a dependent member operator cannot deduce its other class parameter.
	if(specialization_definition && specialization_arguments !=
		specialization_arguments_.end()) {
		// The generated name records the primary template's concrete arguments,
		// but its declaration may have been emitted from a class partial
		// specialization.  Replaying a member alias from the primary bindings
		// would turn `remove_const<T const>::type` back into `T const` and lose
		// the partial's specialized binding (`T`).
		const TemplateDefinition* selected_specialization =
			SelectClassTemplateDefinition(specialization_definition,
				specialization_arguments->second, context);
		if(selected_specialization && selected_specialization->partial_specialization) {
			// A generated specialization may be registered as a forward while its
			// class body is being replayed.  In that window, continue lookup through
			// the selected source partial so inherited members remain visible; the
			// generated declaration will take over once materialization completes.
			if(incomplete_concrete)
				selected_specialization_declaration = selected_specialization->declaration;
			map<string, string> specialized_bindings;
			if(MatchClassSpecializationPattern(*selected_specialization,
				specialization_arguments->second, &specialized_bindings, context))
				for(map<string, string>::const_iterator binding = specialized_bindings.begin();
					binding != specialized_bindings.end(); ++binding)
					class_substitutions[binding->first] = binding->second;
			for(size_t pack = 0; pack < selected_specialization->specialization_pack_names.size();
				++pack) {
				const string& name = selected_specialization->specialization_pack_names[pack];
				map<string, string>::const_iterator binding = specialized_bindings.find(name);
				const vector<string> values = binding == specialized_bindings.end() ||
					binding->second.empty() ? vector<string>() : SplitTemplateArguments(binding->second);
				active_packs.Set(name, values);
				if(!name.empty() && !values.empty()) class_substitutions[name] = values[0];
			}
		} else if(selected_specialization) {
			size_t argument_index = 0;
			for(size_t parameter = 0; parameter < selected_specialization->parameters.size();
				++parameter) {
				const TemplateParameter& template_parameter =
					selected_specialization->parameters[parameter];
				if(template_parameter.pack) {
					vector<string> values;
					size_t trailing_fixed = 0;
					for(size_t later = parameter + 1;
						later < selected_specialization->parameters.size(); ++later)
						if(!selected_specialization->parameters[later].pack) ++trailing_fixed;
					const size_t limit = specialization_arguments->second.size() > trailing_fixed ?
						specialization_arguments->second.size() - trailing_fixed : argument_index;
					while(argument_index < limit)
						values.push_back(specialization_arguments->second[argument_index++]);
					active_packs.Set(template_parameter.name, values);
					if(!template_parameter.name.empty() && !values.empty())
						class_substitutions[template_parameter.name] = values[0];
				} else {
					if(argument_index >= specialization_arguments->second.size()) break;
					if(!template_parameter.name.empty())
						class_substitutions[template_parameter.name] =
							specialization_arguments->second[argument_index];
					++argument_index;
				}
			}
		}
		// A nested class template specialization carries its enclosing source
		// template-id on `owner`, while the generated owner keeps the concrete
		// class arguments in the specialization registry.  Restore those outer
		// bindings before replaying the nested declaration's own parameters.
		if(selected_specialization && !selected_specialization->owner.empty()) {
			const string owner_source = LastComponent(selected_specialization->owner);
			const size_t owner_open = owner_source.find('<');
			string generated_owner = PrefixComponent(raw_class);
			if(generated_owner.empty()) generated_owner = PrefixComponent(class_key);
			map<string, vector<string> >::const_iterator owner_arguments =
				specialization_arguments_.find(LastComponent(generated_owner));
			// This lookup may be replaying the source nested spelling rather than
			// the generated nested name.  Recover the generated path by matching
			// the source nested specialization entry; its prefix is the concrete
			// enclosing owner whose arguments we need.
			if(owner_arguments == specialization_arguments_.end())
				for(map<string, string>::const_iterator generated = specialization_bases_.begin();
					generated != specialization_bases_.end(); ++generated) {
					if(generated->second != specialization->second) continue;
					const string candidate_owner = PrefixComponent(generated->first);
					if(candidate_owner.empty()) continue;
					map<string, vector<string> >::const_iterator candidate_arguments =
						specialization_arguments_.find(LastComponent(candidate_owner));
					if(candidate_arguments == specialization_arguments_.end()) continue;
					generated_owner = candidate_owner;
					owner_arguments = candidate_arguments;
					break;
				}
			if(owner_open != string::npos && owner_arguments != specialization_arguments_.end()) {
				const TemplateDefinition* owner_primary = FindDefinition(
					owner_source.substr(0, owner_open), context);
				if(owner_primary && owner_primary->class_template) {
					const TemplateDefinition* selected_owner = SelectClassTemplateDefinition(
						owner_primary, owner_arguments->second, context);
					if(selected_owner && selected_owner->partial_specialization) {
						map<string, string> owner_bindings;
						if(MatchClassSpecializationPattern(*selected_owner,
							owner_arguments->second, &owner_bindings, context))
							for(map<string, string>::const_iterator binding = owner_bindings.begin();
								binding != owner_bindings.end(); ++binding)
								class_substitutions[binding->first] = binding->second;
					} else if(selected_owner) {
						for(size_t parameter = 0; parameter < selected_owner->parameters.size() &&
							parameter < owner_arguments->second.size(); ++parameter)
							if(!selected_owner->parameters[parameter].name.empty())
								class_substitutions[selected_owner->parameters[parameter].name] =
									owner_arguments->second[parameter];
					}
				}
			}
		}
		if(specialization_definition && specialization_definition->owner.empty() &&
			!specialization_definition->name.empty())
			class_substitutions[specialization_definition->name] = class_key;
	}
	if(specialization != specialization_bases_.end() && incomplete_concrete) {
		// A type-only replay may have installed only a forward shell for the
		// concrete specialization.  A dependent member query is the point at
		// which that body is required, so complete the cached specialization before
		// falling back to the source primary.  The source fallback loses partial
		// bindings and leaves inherited aliases such as `append_impl::type`
		// unresolved.
		bool completed = false;
		if(specialization_arguments != specialization_arguments_.end()) {
			const TemplateDefinition* source_definition = FindDefinition(
				specialization->second, context);
			if(source_definition && source_definition->class_template) {
				const TemplateDefinition* selected_definition =
					SelectClassTemplateDefinition(source_definition,
						specialization_arguments->second, context);
				if(selected_definition) try {
					// If this lookup is occurring while the same specialization is
					// being emitted, Instantiate can only return its active forward
					// name.  Replay the selected source member directly first; this
					// handles an inherited alias without manufacturing another shell.
						const string source_member = const_cast<PA18TemplateExpander*>(this)->
							TemplateMemberType(*selected_definition,
							specialization_arguments->second, member, context,
							class_substitutions);
						if(!source_member.empty()) {
						*result = source_member;
						return true;
					}
					const string generated_name = const_cast<PA18TemplateExpander*>(this)->Instantiate(
						*selected_definition, specialization_arguments->second, context, false,
						0, &class_substitutions);
					string generated_path = JoinPath(GeneratedOwner(*selected_definition),
						generated_name);
					if(class_declarations_.find(generated_path) == class_declarations_.end())
						generated_path = generated_name;
					map<string, CPPGMAstNodePtr>::const_iterator generated_declaration =
						class_declarations_.find(generated_path);
					if(generated_declaration != class_declarations_.end() &&
						generated_declaration->second &&
						generated_declaration->second->children.size() > 1) {
						class_key = generated_path;
						selected_specialization_declaration.reset();
						completed = true;
					}
				} catch(const PA18SubstitutionFailure&) {}
			}
		}
		if(!completed) class_key = specialization->second;
		}
	const string active_key = class_key + "|" + member;
	if(!active->insert(active_key).second) return false;
	CPPGMAstNodePtr declaration = selected_specialization_declaration ?
		selected_specialization_declaration : FindClassDeclaration(class_key, context);
	if(!declaration) {
		active->erase(active_key);
		return false;
	}
	map<string, string> declaration_substitutions = class_substitutions;
	// Keep values of earlier unqualified constant members available while
	// replaying later aliases, but do not add them to the substitutions used for
	// qualified names such as `trait<T>::value`.
	map<string, string> member_declaration_substitutions = declaration_substitutions;
	map<string, vector<string> >::const_iterator materialized_arguments =
		specialization_arguments_.find(LastComponent(class_key));
	if(materialized_arguments != specialization_arguments_.end())
		for(size_t argument = 0; argument < materialized_arguments->second.size(); ++argument) {
			const string value = CanonicalSpelling(materialized_arguments->second[argument]);
			if(value.empty() || value.find_first_not_of(
				"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != string::npos ||
				(class_contexts_.find(value) == class_contexts_.end() &&
					!FindClassDeclaration(value, context))) continue;
			if(member_declaration_substitutions.find(value) !=
				member_declaration_substitutions.end())
				member_declaration_substitutions.erase(value);
		}
	const string declaration_context = PrefixComponent(class_key).empty() ?
		context : PrefixComponent(class_key);
	string fallback_type;
	// A generated class member can refer to aliases declared earlier in that
	// same class body (`S2` in `append_integer_sequence<S2, S3>::type`).  The
	// source declaration's template bindings alone do not provide those local
	// names, so recover the replayed alias table before returning a dependent
	// member type to the enclosing alias substitution.
	for(size_t child_index = 0; child_index < declaration->children.size();
		++child_index) {
		CPPGMAstNodePtr child = declaration->children[child_index];
		while(child && child->kind == "template-declaration" &&
			child->children.size() > 1) child = child->children[1];
		if(!child) continue;
		if(child->kind == "simple-declaration" && !child->children.empty() &&
			(HasDeclarationSpecifier(child->children[0], "const") ||
				HasDeclarationSpecifier(child->children[0], "constexpr"))) {
			const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
			if(list) for(size_t item_index = 0; item_index < list->children.size();
				++item_index) {
				const CPPGMAstNodePtr item = list->children[item_index];
				if(!item || item->children.size() < 2 || !item->children[1] ||
					item->children[1]->children.empty()) continue;
				const string name = LastComponent(FirstIdentifierLocal(item->children[0]));
				if(name.empty()) continue;
				PA19IntegralValue value;
				const string member_target = ConstantExpressionSpelling(
					item->children[1]->children[0]);
				if(member_target.find("::" + name) == string::npos &&
					const_cast<PA18TemplateExpander*>(this)->EvaluateIntegralText(
					member_target, class_key, member_declaration_substitutions, &value) && value.known)
					member_declaration_substitutions[name] =
						const_cast<PA18TemplateExpander*>(this)->IntegralValueSpelling(value);
			}
		}
		if(child->kind == "alias-declaration" && !child->value.empty() &&
			!child->children.empty()) {
			string target = TypeIdSpelling(child->children[0]);
			map<string, string> alias_substitutions = member_declaration_substitutions;
			ProtectMaterializedTemplateBases(target, declaration_context,
				member_declaration_substitutions, &alias_substitutions);
			target = CanonicalSpelling(ReplaceIdentifiersPreservingPackSizes(
				target, alias_substitutions));
			try {
				target = CanonicalSpelling(const_cast<PA18TemplateExpander*>(this)->RewriteText(
					target, declaration_context, member_declaration_substitutions, 0));
			} catch(const PA18SubstitutionFailure&) {}
			if(!target.empty()) member_declaration_substitutions[child->value] = target;
			continue;
		}
		if(child->kind != "simple-declaration" || child->children.empty() ||
			!HasDeclarationSpecifier(child->children[0], "typedef")) continue;
		const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
				if(!list) continue;
		for(size_t item_index = 0; item_index < list->children.size(); ++item_index) {
			const CPPGMAstNodePtr item = list->children[item_index];
			if(!item || item->children.empty()) continue;
			const string alias = FirstIdentifierLocal(item->children[0]);
			if(alias.empty()) continue;
			string target = DeclaratorTypeSpelling(NodeTypeSpelling(child->children[0]),
				item->children[0]);
			map<string, string> alias_substitutions = member_declaration_substitutions;
			ProtectMaterializedTemplateBases(target, declaration_context,
				member_declaration_substitutions, &alias_substitutions);
			target = CanonicalSpelling(ReplaceIdentifiersPreservingPackSizes(
				target, alias_substitutions));
			try {
				target = CanonicalSpelling(const_cast<PA18TemplateExpander*>(this)->RewriteText(
				target, declaration_context, member_declaration_substitutions, 0));
		} catch(const PA18SubstitutionFailure&) {}
			if(!target.empty()) member_declaration_substitutions[alias] = target;
		}
	}
	for(size_t i = 0; i < declaration->children.size(); ++i) {
		const CPPGMAstNodePtr child = declaration->children[i];
		if(!child) continue;
		CPPGMAstNodePtr direct_child = child;
		while(direct_child && direct_child->kind == "template-declaration" &&
			direct_child->children.size() > 1)
			direct_child = direct_child->children[1];
		if(direct_child && direct_child->kind == "alias-declaration" &&
			LastComponent(RemoveMarker(direct_child->value)) == member &&
			!direct_child->children.empty()) {
				map<string, string> alias_substitutions = declaration_substitutions;
				ProtectMaterializedTemplateBases(TypeIdSpelling(direct_child->children[0]),
					context, declaration_substitutions, &alias_substitutions);
				*result = CanonicalSpelling(ReplaceIdentifiersPreservingPackSizes(
					TypeIdSpelling(direct_child->children[0]), alias_substitutions));
				if(result->find('<') != string::npos)
					*result = NormalizeTypeArgument(const_cast<PA18TemplateExpander*>(this)->RewriteText(
						*result, context, declaration_substitutions, 0, true, true, true));
				const string resolved_alias = ResolveAlias(*result, context,
					declaration_substitutions);
				if(!resolved_alias.empty()) *result = resolved_alias;
			active->erase(active_key);
			return !result->empty();
		}
		const bool generated_concrete_owner =
			specialization_bases_.find(LastComponent(class_key)) !=
				specialization_bases_.end() &&
			specialization_arguments_.find(LastComponent(class_key)) !=
				specialization_arguments_.end() && context != class_key;
		if((!aliases_only || generated_concrete_owner) && direct_child &&
			(direct_child->kind == "class-specifier" ||
			direct_child->kind == "class-forward-declaration" ||
			direct_child->kind == "enum-specifier") &&
			LastComponent(direct_child->value) == member) {
			*result = JoinPath(class_key, member);
			active->erase(active_key);
			return true;
		}
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
				*result = CanonicalSpelling(ReplaceIdentifiers(type, declaration_substitutions));
					if(result->find('<') != string::npos)
						*result = NormalizeTypeArgument(const_cast<PA18TemplateExpander*>(this)->RewriteText(
							*result, context, declaration_substitutions, 0, true, true, true));
					active->erase(active_key);
				return !result->empty();
			}
		if(child->kind != "simple-declaration" || child->children.empty()) continue;
		const string base = NodeTypeSpelling(child->children[0]);
		const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
		if(!list) continue;
		for(size_t item = 0; item < list->children.size(); ++item) {
			const CPPGMAstNodePtr init = list->children[item];
			if(!init || init->children.empty()) continue;
			if(aliases_only && DescendantOfKind(init->children[0], "parameter-clause"))
				continue;
			if(LastComponent(FirstIdentifierLocal(init->children[0])) != member) continue;
			const bool typedef_member = HasDeclarationSpecifier(child->children[0], "typedef");
			const bool concrete_static_member = generated_concrete_owner &&
				HasStaticMember(0, class_key, member) &&
				init->children.size() == 1 &&
				!HasDeclarationSpecifier(child->children[0], "constexpr") &&
				DeclaratorArraySuffix(init->children[0]).find_first_not_of("[]") != string::npos;
			if(aliases_only && !typedef_member && !concrete_static_member)
				continue;
				map<string, string> member_substitutions = declaration_substitutions;
				if(typedef_member) {
					const size_t base_open = base.find('<');
					if(base_open != string::npos) {
						const string base_name = LastComponent(base.substr(0, base_open));
						if(base_name == LastComponent(class_key))
							member_substitutions.erase(base_name);
					}
				}
					*result = CanonicalSpelling(ReplaceIdentifiers(
						DeclaratorTypeSpelling(base, init->children[0]), member_substitutions));
				if(typedef_member && result->find('<') != string::npos)
					*result = NormalizeTypeArgument(const_cast<PA18TemplateExpander*>(this)->RewriteText(
						*result, context, declaration_substitutions, 0, true, true, true));
				active->erase(active_key);
				return !result->empty();
			}
	}
	if(!fallback_type.empty()) {
		*result = CanonicalSpelling(ReplaceIdentifiers(fallback_type, declaration_substitutions));
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
				map<string, string> base_name_substitutions = class_substitutions;
				const size_t raw_base_open = base_name->value.find('<');
				if(raw_base_open != string::npos)
					base_name_substitutions.erase(LastComponent(base_name->value.substr(0,
						raw_base_open)));
				// The inherited base can contain a source template-id nested inside
				// another type, for example `call<ListSet<Char>(left)>`.  A replay
				// binding for the already-materialized callable or return class must
				// not rewrite those source template heads; doing so appends the old
				// argument list to the generated nominal name (`call_X<ListSet_X>`).
				ProtectMaterializedTemplateBases(base_name->value, declaration_context,
					class_substitutions, &base_name_substitutions);
				string base_spelling = NormalizeElaboratedSpelling(
				ReplaceIdentifiersPreservingPackSizes(base_name->value, base_name_substitutions),
				declaration_context);
				base_spelling = CanonicalSpelling(base_spelling);
				const size_t open = base_spelling.find('<');
			const TemplateDefinition* base_definition = 0;
			bool base_is_partial_specialization = false;
			vector<string> base_arguments;
			map<string, string> base_substitutions;
				string base_lookup = base_spelling;
				if(open != string::npos) {
				string argument_text;
				size_t close = string::npos;
					if(!TemplateRange(base_spelling, open, &argument_text, &close)) continue;
				// Keep pack operands intact while applying scalar class bindings,
				// then expand them from the active typed pack collection.  Replacing
				// `_Tail` first would manufacture `double...`, which is neither a
				// type nor a valid template argument.
				for(map<string, vector<string> >::const_iterator pack =
					active_pack_substitutions_.begin();
					pack != active_pack_substitutions_.end(); ++pack) {
					if(pack->first.empty()) continue;
					const string token = pack->first + "...";
					string expanded;
					for(size_t value = 0; value < pack->second.size(); ++value) {
						if(!expanded.empty()) expanded += ',';
						expanded += pack->second[value];
					}
					for(size_t at = base_spelling.find(token); at != string::npos;) {
						base_spelling.replace(at, token.size(), expanded);
						if(expanded.empty()) {
							if(at < base_spelling.size() && base_spelling[at] == ',')
								base_spelling.erase(at, 1);
							else if(at > 0 && base_spelling[at - 1] == ',')
								base_spelling.erase(at - 1, 1), --at;
						}
						at = base_spelling.find(token, at + expanded.size());
					}
				}
				if(!TemplateRange(base_spelling, open, &argument_text, &close)) continue;
				base_lookup = base_spelling.substr(0, open);
					base_definition = FindDefinition(base_lookup, declaration_context);
				if(base_definition) {
					base_arguments = SplitTemplateArguments(argument_text);
					for(size_t parameter = 0; parameter < base_arguments.size() &&
						parameter < base_definition->parameters.size(); ++parameter)
						if(!base_definition->parameters[parameter].type) {
							PA19IntegralValue value;
							if(const_cast<PA18TemplateExpander*>(this)->EvaluateIntegralText(
								base_arguments[parameter], declaration_context,
								class_substitutions, &value))
								base_arguments[parameter] =
									const_cast<PA18TemplateExpander*>(this)->TemplateIntegralValueSpelling(value);
						}
					for(size_t parameter = 0; parameter < base_definition->parameters.size() &&
						parameter < base_arguments.size(); ++parameter)
						if(!base_definition->parameters[parameter].name.empty())
						{
							// `base_spelling` has already been formed from the concrete
							// enclosing bindings.  Reapplying those bindings here can
							// reinterpret a nominal class that shadows a parameter name:
							// `VertexProperty -> Vertex` followed by `Vertex -> unsigned
							// long` would corrupt the nested graph argument.  Carry the
							// typed argument forward as-is; only normalize its spelling
							// and scope below.
							base_substitutions[base_definition->parameters[parameter].name] =
								QualifyTypeArgument(NormalizeElaboratedSpelling(
								base_arguments[parameter], declaration_context),
								declaration_context, base_definition->owner);
				}
				}
			}
			if(base_definition && !base_arguments.empty()) {
				// Prefer a concrete replay of an inherited class specialization.
				// Looking through the source primary loses the enclosing bindings of
				// nested members such as `arg_list<Tag>::binding::fn`; the generated
				// declaration carries the typed `key_type`, `next_binding`, and other
				// aliases needed by the member-template body.
				if(base_definition->class_template) {
					map<string, vector<string> >::const_iterator generated_names =
						specialization_names_by_base_.find(LastComponent(
							base_definition->qualified_name));
						if(generated_names != specialization_names_by_base_.end())
					for(size_t generated_index = 0;
							generated_index < generated_names->second.size(); ++generated_index) {
							const string& generated_name = generated_names->second[generated_index];
							map<string, vector<string> >::const_iterator generated_arguments =
								specialization_arguments_.find(generated_name);
							if(generated_arguments == specialization_arguments_.end() ||
								generated_arguments->second.size() < base_arguments.size()) continue;
							bool omitted_defaults = true;
							for(size_t omitted = base_arguments.size(); omitted <
								generated_arguments->second.size(); ++omitted)
								if(omitted >= base_definition->parameters.size() ||
									(!base_definition->parameters[omitted].pack &&
									 base_definition->parameters[omitted].default_type.empty())) {
									omitted_defaults = false;
									break;
								}
							if(!omitted_defaults) continue;
							bool same_arguments = true;
							for(size_t argument = 0; argument < base_arguments.size(); ++argument) {
								const string actual = CollapseRepeatedMemberPaths(CollapseRepeatedQualifier(
									NormalizeTypeArgument(CanonicalSpelling(ReplaceIdentifiers(
										base_arguments[argument], class_substitutions)))));
								const string expected = CollapseRepeatedMemberPaths(CollapseRepeatedQualifier(
									NormalizeTypeArgument(CanonicalSpelling(
										generated_arguments->second[argument]))));
								if(actual != expected && RestoreSpecializationSpelling(actual) !=
									RestoreSpecializationSpelling(expected) &&
									LastComponent(actual) != LastComponent(expected)) {
									same_arguments = false;
									break;
								}
							}
							if(!same_arguments) continue;
							string generated_path = JoinPath(GeneratedOwner(*base_definition), generated_name);
							if(class_declarations_.find(generated_path) == class_declarations_.end())
								generated_path = generated_name;
							bool requested_nested_class = false;
							if(base_definition->declaration) for(size_t child = 0;
								child < base_definition->declaration->children.size(); ++child) {
								const CPPGMAstNodePtr nested = base_definition->declaration->children[child];
								if(nested && (nested->kind == "class-specifier" ||
									nested->kind == "class-forward-declaration") &&
									LastComponent(nested->value) == member) {
									requested_nested_class = true;
									break;
								}
							}
			if(requested_nested_class) try {
					const_cast<PA18TemplateExpander*>(this)->InstantiateNestedClass(
						*base_definition, generated_arguments->second, generated_name,
						member, declaration_context);
			} catch(const PA18SubstitutionFailure&) {}
			if(requested_nested_class && !aliases_only) {
				const string generated_nested = JoinPath(generated_name, member);
				const string qualified_generated_nested = JoinPath(generated_path, member);
				if(class_declarations_.find(generated_nested) != class_declarations_.end()) {
					*result = generated_nested;
					active->erase(active_key);
					return true;
				}
				if(class_declarations_.find(qualified_generated_nested) !=
					class_declarations_.end()) {
					*result = qualified_generated_nested;
					active->erase(active_key);
					return true;
				}
			}
			const bool generated_member_found = FindClassMemberType(generated_path,
								member, class_substitutions, declaration_context, result, active,
								aliases_only);
								if(generated_member_found) {
								active->erase(active_key);
								return true;
							}
						}
				}
				if(base_definition->alias_template) {
					// `mp_defer<F, T...>` is an inherited alias whose selected
					// branch is `mp_defer_impl<F, T...>`.  Its useful member is
					// therefore the result of invoking the template-template
					// argument, not the generated spelling of the alias itself.
					// Evaluate that typed result while following an inherited
					// member so a valid F remains visible to the surrounding
					// substitution probe.  An invalid F naturally fails through
					// the normal alias replay below.
					if(base_definition->name == "mp_defer" && base_arguments.size() > 1) {
						string deferred_callable = const_cast<PA18TemplateExpander*>(this)->
							NormalizeTemplateTemplateArgument(base_arguments[0],
								declaration_context, base_substitutions);
						if(deferred_callable.empty()) deferred_callable = base_arguments[0];
						bool deferred_concrete = true;
						for(size_t argument = 1; argument < base_arguments.size(); ++argument)
							if(base_arguments[argument].find("...") != string::npos ||
								HasUnresolvedTemplateParameter(base_arguments[argument],
									declaration_context, class_substitutions)) {
								deferred_concrete = false;
								break;
							}
						string deferred_member_target;
						map<string, string> deferred_substitutions = base_substitutions;
						const size_t deferred_member_separator = deferred_callable.rfind("::");
						if(deferred_member_separator != string::npos && deferred_concrete) {
							const string deferred_owner = deferred_callable.substr(0,
								deferred_member_separator);
							const string deferred_member = deferred_callable.substr(
								deferred_member_separator + 2);
							deferred_member_target = MemberAliasType(deferred_owner, deferred_member);
							const string deferred_parent = PrefixComponent(deferred_owner);
							const map<string, vector<string> >::const_iterator member_candidates =
								definitions_by_name_.find(deferred_member);
							const TemplateDefinition* deferred_definition = 0;
							int deferred_definition_score = 0;
							string deferred_source_parent;
							map<string, string>::const_iterator deferred_parent_base =
								specialization_bases_.find(LastComponent(deferred_parent));
							if(deferred_parent_base != specialization_bases_.end())
								deferred_source_parent = deferred_parent_base->second;
							if(member_candidates != definitions_by_name_.end())
								for(size_t candidate = 0; candidate < member_candidates->second.size(); ++candidate) {
									map<string, TemplateDefinition>::const_iterator found = definitions_.find(
										member_candidates->second[candidate]);
									if(found == definitions_.end() || !found->second.alias_template) continue;
									if(LastComponent(found->second.owner) != LastComponent(deferred_owner) &&
										LastComponent(found->second.owner) != deferred_member) continue;
									int score = 1;
									const string candidate_parent = PrefixComponent(found->second.owner);
									if(!deferred_source_parent.empty() &&
										(candidate_parent == deferred_source_parent ||
										 LastComponent(candidate_parent) == LastComponent(deferred_source_parent)))
										score = 3;
									if(!deferred_source_parent.empty() &&
										candidate_parent.find(deferred_source_parent + "::") == 0)
										score = 4;
									if(score > deferred_definition_score) {
										deferred_definition = &found->second;
										deferred_definition_score = score;
									}
								}
							if(deferred_definition) for(size_t parameter = 0;
								parameter < deferred_definition->parameters.size() &&
								parameter + 1 < base_arguments.size(); ++parameter)
								if(!deferred_definition->parameters[parameter].name.empty())
									deferred_substitutions[deferred_definition->parameters[parameter].name] =
										base_arguments[parameter + 1];
						const char* deferred_names[] = {"key_type", "value_type", "reference", "next_binding"};
						for(size_t name = 0; name < sizeof(deferred_names) / sizeof(deferred_names[0]); ++name) {
							string alias_type = name == 3 ? MemberAliasType(deferred_owner,
								deferred_names[name]) : MemberAliasType(deferred_parent, deferred_names[name]);
							if(alias_type.empty()) {
								set<string> alias_active;
								FindClassMemberType(name == 3 ? deferred_owner : deferred_parent,
									deferred_names[name], map<string, string>(), declaration_context,
									&alias_type, &alias_active, true);
							}
							if(alias_type.empty()) continue;
							try {
								alias_type = CanonicalSpelling(const_cast<PA18TemplateExpander*>(this)->RewriteText(
									alias_type, declaration_context, deferred_substitutions, 0));
							} catch(const PA18SubstitutionFailure&) {}
							deferred_substitutions[deferred_names[name]] = alias_type;
						}
						if(!deferred_member_target.empty()) {
							try {
								const string replayed_target = CanonicalSpelling(
									const_cast<PA18TemplateExpander*>(this)->RewriteText(
										deferred_member_target, declaration_context,
										deferred_substitutions, 0));
								if(!replayed_target.empty() && replayed_target != deferred_member_target) {
									*result = CanonicalSpelling(ResolveAlias(replayed_target,
										declaration_context));
									if(result->empty()) *result = replayed_target;
										active->erase(active_key);
										return true;
									}
								} catch(const PA18SubstitutionFailure&) {}
							}
						}
						string deferred_call = deferred_callable + "<";
						for(size_t argument = 1; argument < base_arguments.size(); ++argument) {
							if(argument != 1) deferred_call += ',';
							deferred_call += base_arguments[argument];
						}
						deferred_call += '>';
						try {
							string deferred_type = CanonicalSpelling(
								const_cast<PA18TemplateExpander*>(this)->RewriteText(
									deferred_call, declaration_context, base_substitutions, 0));
							if(!deferred_type.empty() && deferred_type != deferred_call) {
								const string resolved_deferred_type = CanonicalSpelling(
									ResolveAlias(deferred_type, declaration_context));
								*result = resolved_deferred_type.empty() ? deferred_type :
									resolved_deferred_type;
								active->erase(active_key);
								return true;
							}
						} catch(const PA18SubstitutionFailure&) {}
					}
					try {
						string expanded_alias = CanonicalSpelling(
							const_cast<PA18TemplateExpander*>(this)->RewriteText(
								base_spelling, declaration_context, base_substitutions, 0));
						if(!expanded_alias.empty() && expanded_alias != base_spelling &&
							FindClassMemberType(expanded_alias, member, base_substitutions,
								declaration_context, result, active, aliases_only)) {
							active->erase(active_key);
							return true;
						}
					} catch(const PA18SubstitutionFailure&) {}
				}
				const TemplateDefinition* selected_base = SelectClassTemplateDefinition(
					base_definition, base_arguments, declaration_context);
				if(selected_base && selected_base != base_definition &&
					selected_base->partial_specialization) {
					base_is_partial_specialization = true;
					string specialized_member = const_cast<PA18TemplateExpander*>(this)->TemplateMemberType(
						*selected_base, base_arguments, member, declaration_context);
					if(!specialized_member.empty()) {
						*result = specialized_member;
						active->erase(active_key);
						return true;
					}
				}
			}
				if(base_definition && base_is_partial_specialization && !base_arguments.empty())
					base_lookup = base_spelling;
				else if(base_definition) base_lookup = base_definition->qualified_name;
				if(!base_definition) {
					// Ordinary typedefs used as dependent bases (for example
					// `mpl::false_`) are not TemplateDefinitions.  Resolve the alias
					// in the source namespace before giving up inherited member lookup;
					// otherwise `is_same<T, U>::type` remains an unqualified dependent
					// chain even though its bool_ base exposes `type`.
					const string qualified_base = QualifyTypeArgument(base_spelling,
						declaration_context, declaration_context, true);
					const string resolved_base = CanonicalSpelling(ResolveAlias(
						qualified_base, declaration_context));
					if(!resolved_base.empty() && resolved_base != qualified_base &&
						resolved_base != base_spelling &&
						FindClassMemberType(resolved_base, member, base_substitutions,
							declaration_context, result, active, aliases_only)) {
						active->erase(active_key);
						return true;
					}
				}
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
