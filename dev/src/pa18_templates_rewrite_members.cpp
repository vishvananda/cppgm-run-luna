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

bool PA18TemplateExpander::RewriteMemberTemplateAliasApplication(string* raw,
	size_t begin, size_t close, const string& base, const string& context,
	const map<string, string>& substitutions, bool* template_replaced, size_t* search)
{
	if(!raw || close <= begin || LastComponent(base) != "mp_apply_q") return false;
	const size_t open = raw->find('<', begin);
	if(open == string::npos || open >= close) return false;
	const vector<string> apply_arguments = SplitTemplateArguments(raw->substr(
		open + 1, close - open - 1));
	if(apply_arguments.size() != 2) return false;
	string q = CanonicalSpelling(ReplaceIdentifiers(apply_arguments[0], substitutions));
	while(q.compare(0, 8, "typename") == 0)
		q = CanonicalSpelling(q.substr(8));
	if(q.compare(0, 9, "template ") == 0) q = CanonicalSpelling(q.substr(9));
	const size_t list_open = q.empty() ? string::npos :
		apply_arguments[1].find('<');
	if(list_open == string::npos) return false;
	string list_arguments_text;
	size_t list_close = string::npos;
	if(!TemplateRange(apply_arguments[1], list_open, &list_arguments_text,
		&list_close)) return false;
	vector<string> call_arguments = SplitTemplateArguments(list_arguments_text);
	if(call_arguments.empty()) return false;
	for(size_t argument = 0; argument < call_arguments.size(); ++argument)
		call_arguments[argument] = CanonicalSpelling(ReplaceIdentifiers(
			call_arguments[argument], substitutions));
	const string callable_source = q + "::fn";
	const string callable = NormalizeTemplateTemplateArgument(callable_source,
		context, substitutions);
	if(callable.empty()) return false;
	const size_t callable_separator = callable.rfind("::");
	if(callable_separator == string::npos) return false;
	const string callable_owner = callable.substr(0, callable_separator);
	const string callable_name = callable.substr(callable_separator + 2);
	string target = MemberAliasType(callable_owner, callable_name);
	if(target.empty()) {
		set<string> active;
		FindClassMemberType(callable_owner, callable_name, substitutions, context,
			&target, &active, true);
	}
	if(target.empty()) return false;
	map<string, string> local = substitutions;
	const string source_parent = [&]() {
		const string parent = PrefixComponent(callable_owner);
		map<string, string>::const_iterator generated = specialization_bases_.find(
			LastComponent(parent));
		return generated == specialization_bases_.end() ? string() : generated->second;
	}();
	const map<string, vector<string> >::const_iterator indexed = definitions_by_name_.find(
		callable_name);
	const TemplateDefinition* callable_definition = FindDefinition(callable_source,
		context);
	int callable_definition_score = callable_definition ? 1 : 0;
	if(indexed != definitions_by_name_.end()) for(size_t candidate = 0;
		candidate < indexed->second.size(); ++candidate) {
		map<string, TemplateDefinition>::const_iterator found = definitions_.find(
			indexed->second[candidate]);
		if(found == definitions_.end() || !found->second.alias_template) continue;
		const string owner = found->second.owner;
		if(LastComponent(owner) != LastComponent(callable_owner)) continue;
		int score = 1;
		const string candidate_parent = PrefixComponent(owner);
		if(!source_parent.empty() && LastComponent(candidate_parent) ==
			LastComponent(source_parent)) score = 3;
		if(!source_parent.empty() && candidate_parent.find(source_parent + "::") == 0)
			score = 4;
		if(score > callable_definition_score) {
			callable_definition = &found->second;
			callable_definition_score = score;
		}
	}
	if(callable_definition) for(size_t parameter = 0;
		parameter < callable_definition->parameters.size() &&
		parameter < call_arguments.size(); ++parameter)
		if(!callable_definition->parameters[parameter].name.empty())
			local[callable_definition->parameters[parameter].name] = call_arguments[parameter];
	const char* const member_names[] = {"key_type", "value_type", "reference", "next_binding"};
	for(size_t name = 0; name < sizeof(member_names) / sizeof(member_names[0]); ++name) {
		const string owner = name == 3 ? callable_owner : PrefixComponent(callable_owner);
		string member_type = MemberAliasType(owner, member_names[name]);
		if(member_type.empty()) {
			set<string> active;
			FindClassMemberType(owner, member_names[name], substitutions, context,
				&member_type, &active, true);
		}
		if(member_type.empty()) continue;
		try {
			member_type = CanonicalSpelling(RewriteText(member_type, context, local, 0));
		} catch(const PA18SubstitutionFailure&) {}
		local[member_names[name]] = member_type;
	}
	string rewritten;
	try {
		rewritten = CanonicalSpelling(RewriteText(target, context, local, 0));
	} catch(const PA18SubstitutionFailure&) {
		return false;
	}
	if(rewritten.empty() || rewritten == target) return false;
	raw->replace(begin, close - begin + 1, rewritten);
	if(template_replaced) *template_replaced = true;
	if(search) *search = begin + rewritten.size();
	return true;
}

bool PA18TemplateExpander::RewriteResolvedTemplateMember(string* raw, size_t begin,
	size_t close, const string& context, const map<string, string>& substitutions,
	const TemplateDefinition* definition, const vector<string>& args,
	bool* template_replaced, size_t* search)
{
	if(!raw || !definition || close + 2 >= raw->size() ||
		raw->compare(close + 1, 2, "::") != 0) return false;
	RecordTemplateArrayValues(*definition, args, context, substitutions,
		active_pack_substitutions_);
	size_t nested_end = close + 3;
	while(nested_end < raw->size() && IsIdentifierCharacter((*raw)[nested_end])) ++nested_end;
	const string nested = raw->substr(close + 3, nested_end - close - 3);
	if(nested.empty()) return false;
	const string template_owner = raw->substr(begin, close - begin + 1);
	const string concrete_template_owner = ReplaceIdentifiersPreservingPackSizes(
		template_owner, substitutions);
	map<string, string> inherited_owner_bindings;
	const size_t inherited_separator = TopLevelScopeSeparator(concrete_template_owner);
	if(inherited_separator != string::npos) {
		const string outer_owner = concrete_template_owner.substr(0, inherited_separator);
		const size_t outer_open = outer_owner.find('<');
		string outer_arguments_text;
		size_t outer_close = string::npos;
		if(outer_open != string::npos && TemplateRange(outer_owner, outer_open,
			&outer_arguments_text, &outer_close)) {
			const string outer_base = outer_owner.substr(0, outer_open);
			const TemplateDefinition* outer_definition = FindDefinition(outer_base, context);
			if(outer_definition && outer_definition->class_template) {
				const vector<string> outer_arguments = SplitTemplateArguments(outer_arguments_text);
				const TemplateDefinition* selected_outer = SelectClassTemplateDefinition(
					outer_definition, outer_arguments, context);
				if(selected_outer) outer_definition = selected_outer;
				for(size_t parameter = 0; parameter < outer_definition->parameters.size() &&
					parameter < outer_arguments.size(); ++parameter)
					if(!outer_definition->parameters[parameter].name.empty())
							inherited_owner_bindings[outer_definition->parameters[parameter].name] =
								outer_arguments[parameter];
				}
			}
	}
	string member_type;
	set<string> member_active;
	const TemplateDefinition* member_definition = definition;
	if(member_definition->class_template) {
		const TemplateDefinition* selected_member = SelectClassTemplateDefinition(
			member_definition, args, context);
		if(selected_member) member_definition = selected_member;
	}
	if(member_definition->declaration && !inherited_owner_bindings.empty()) {
		map<string, string> member_substitutions = substitutions;
		for(map<string, string>::const_iterator binding = inherited_owner_bindings.begin();
			binding != inherited_owner_bindings.end(); ++binding)
			member_substitutions[binding->first] = binding->second;
		for(size_t parameter = 0; parameter < member_definition->parameters.size() &&
			parameter < args.size(); ++parameter)
			if(!member_definition->parameters[parameter].name.empty())
				member_substitutions[member_definition->parameters[parameter].name] = args[parameter];
		string direct_member_type;
		bool found = FindDirectTemplateMemberType(*member_definition, args, nested,
			context, &member_substitutions, &direct_member_type) && !direct_member_type.empty();
		if(!found) found = FindInheritedTemplateMemberType(*member_definition, nested,
			context, member_substitutions, &direct_member_type) && !direct_member_type.empty();
		if(found) member_type = direct_member_type;
	}
	if(member_type.empty()) {
		bool found = FindClassMemberType(concrete_template_owner, nested, substitutions,
			context, &member_type, &member_active, true);
		if(!found || member_type.empty()) {
			found = FindClassMemberType(template_owner, nested, substitutions, context,
				&member_type, &member_active, true);
			if(!found || member_type.empty())
				member_type = TemplateMemberType(*definition, args, nested, context);
		}
	}
	const size_t owner_separator = TopLevelScopeSeparator(concrete_template_owner);
	if(owner_separator != string::npos) {
		const string outer_owner = concrete_template_owner.substr(0, owner_separator);
		const size_t outer_open = outer_owner.find('<');
		string outer_arguments_text;
		size_t outer_close = string::npos;
		if(outer_open != string::npos && TemplateRange(outer_owner, outer_open,
			&outer_arguments_text, &outer_close)) {
			const string outer_base = outer_owner.substr(0, outer_open);
			const TemplateDefinition* outer_definition = FindDefinition(outer_base, context);
			if(outer_definition && outer_definition->class_template) {
				const vector<string> outer_arguments = SplitTemplateArguments(outer_arguments_text);
				const TemplateDefinition* selected_outer = SelectClassTemplateDefinition(
					outer_definition, outer_arguments, context);
				if(selected_outer) outer_definition = selected_outer;
				map<string, string> outer_bindings;
				for(size_t parameter = 0; parameter < outer_definition->parameters.size() &&
					parameter < outer_arguments.size(); ++parameter)
					if(!outer_definition->parameters[parameter].name.empty())
						outer_bindings[outer_definition->parameters[parameter].name] =
							outer_arguments[parameter];
				member_type = ReplaceIdentifiersPreservingPackSizes(member_type, outer_bindings);
			}
		}
	}
	if(member_type.empty() || member_type.find('[') != string::npos) {
		requested_nested_classes_[definition->qualified_name].insert(nested);
		requested_nested_classes_[LastComponent(definition->qualified_name)].insert(nested);
		return false;
	}
	// Replace the complete dependent owner before its member type; the resolved
	// type already names the materialized owner and needs no `template` qualifier.
	size_t replacement_begin = begin;
	size_t qualifier = begin;
	while(qualifier > 0 && isspace(static_cast<unsigned char>((*raw)[qualifier - 1]))) --qualifier;
	if(qualifier >= 8 && raw->compare(qualifier - 8, 8, "template") == 0) {
		qualifier -= 8;
		while(qualifier > 0 && isspace(static_cast<unsigned char>((*raw)[qualifier - 1]))) --qualifier;
		if(qualifier >= 2 && raw->compare(qualifier - 2, 2, "::") == 0) {
			qualifier -= 2;
			while(qualifier > 0 && isspace(static_cast<unsigned char>((*raw)[qualifier - 1]))) --qualifier;
			while(qualifier > 0) {
				const size_t component_end = qualifier;
				while(qualifier > 0 && IsIdentifierCharacter((*raw)[qualifier - 1])) --qualifier;
				if(component_end == qualifier || qualifier < 2 ||
					raw->compare(qualifier - 2, 2, "::") != 0) break;
				qualifier -= 2;
				while(qualifier > 0 && isspace(static_cast<unsigned char>((*raw)[qualifier - 1]))) --qualifier;
			}
			replacement_begin = qualifier;
		}
	}
	raw->replace(replacement_begin, nested_end - replacement_begin, member_type);
	if(template_replaced) *template_replaced = true;
	if(search) *search = replacement_begin + member_type.size();
	return true;
}

bool PA18TemplateExpander::FindClassMemberType(const string& raw_class, const string& member,
	const map<string, string>& substitutions, const string& context,
	string* result, set<string>* active, bool aliases_only) const
{
	if(!result || !active) return false;
	// Member lookup owns the dependent owner spelling.  Resolve its typed
	// bindings for every probe instead of using helper-name text as a proxy for
	// dependency; aliases and user-defined traits take the same semantic path.
	string class_key = CanonicalSpelling(ReplaceIdentifiersPreservingPackSizes(
		raw_class, substitutions));
	// A generated replay can duplicate the standard-library owner while
	// redirecting a dependent member query.  Normalize that concrete path, but
	// leave source lexical owners intact: their repeated spelling can be the
	// active lookup context for hidden friends and is not itself a new identity.
	if(class_key.find("std::std::") != string::npos)
		class_key = CollapseRepeatedQualifiedPath(CollapseRepeatedQualifier(class_key));
	if(class_key.find("::") == string::npos) {
		const string resolved_class_key = CanonicalSpelling(ResolveAlias(class_key, context));
		if(!resolved_class_key.empty() && resolved_class_key != class_key)
			class_key = resolved_class_key;
	}
	const string lookup_key = class_key + "|" + member + "|" + context;
	if(!active_member_type_lookups_.insert(lookup_key).second) return false;
	struct LookupScope {
		set<string>* active; string key;
		LookupScope(set<string>* value, const string& name) : active(value), key(name) {}
		~LookupScope() { active->erase(key); }
	} lookup_scope(&active_member_type_lookups_, lookup_key);
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
			for(map<string, string>::const_iterator generated = specialization_bases_.begin();
				generated != specialization_bases_.end(); ++generated) {
				string generated_source = generated->second;
				const size_t generated_source_open = generated_source.find('<');
				if(generated_source_open != string::npos) generated_source.erase(generated_source_open);
				if(generated_source != source_template_base &&
					(LastComponent(generated_source) != LastComponent(source_template_base) ||
						PrefixComponent(generated_source) != PrefixComponent(source_template_base))) continue;
				map<string, vector<string> >::const_iterator concrete_arguments =
					specialization_arguments_.find(generated->first);
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
				for(map<string, CPPGMAstNodePtr>::const_iterator declaration =
					class_declarations_.begin(); declaration != class_declarations_.end(); ++declaration)
					if(LastComponent(declaration->first) == generated->first) {
						class_key = declaration->first;
						break;
					}
				if(class_declarations_.find(class_key) != class_declarations_.end()) break;
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
				if(contains_call) try {
					const string generated = const_cast<PA18TemplateExpander*>(this)->Instantiate(
						*source_definition, requested_arguments, context, false, 0,
						&substitutions);
					const string qualified_generated = source_definition->owner.empty() ? generated :
						JoinPath(source_definition->owner, generated);
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
			for(map<string, CPPGMAstNodePtr>::const_iterator declaration =
				class_declarations_.begin(); declaration != class_declarations_.end(); ++declaration)
				if(LastComponent(declaration->first) == LastComponent(generated_base)) {
					if(qualified_generated.empty()) qualified_generated = declaration->first;
					else if(qualified_generated != declaration->first) {
						qualified_generated.clear();
						break;
					}
				}
			if(!qualified_generated.empty()) class_key = qualified_generated;
		}
	}
	if(class_declarations_.find(class_key) == class_declarations_.end() &&
		specialization_bases_.find(LastComponent(class_key)) != specialization_bases_.end()) {
		for(map<string, CPPGMAstNodePtr>::const_iterator declaration =
			class_declarations_.begin(); declaration != class_declarations_.end(); ++declaration)
			if(LastComponent(declaration->first) == LastComponent(class_key)) {
				class_key = declaration->first;
				break;
			}
	}
	map<string, string> class_substitutions = substitutions;
	ActivePackScope active_packs(const_cast<PA18TemplateExpander*>(this));
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
								const string argument = preserves_materialized_argument(class_arguments[parameter]) ?
									CanonicalSpelling(class_arguments[parameter]) : CanonicalSpelling(ReplaceIdentifiers(
									class_arguments[parameter], class_substitutions));
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
								const string actual = CollapseRepeatedQualifier(NormalizeTypeArgument(
									RestoreSpecializationSpelling(ResolveAlias(ReplaceIdentifiers(
										class_arguments[argument], substitutions), context))));
								const string expected = CollapseRepeatedQualifier(NormalizeTypeArgument(
									RestoreSpecializationSpelling(generated_arguments->second[argument])));
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
	CPPGMAstNodePtr selected_specialization_declaration;
	// Materialized class declarations are complete by this point, but member
	// return types still use aliases from the source class template.  Carry the
	// concrete class arguments into that lookup even when no forward shell is
	// being replaced; otherwise `begin()` returns the spelling `iterator` and
	// a dependent member operator cannot deduce its other class parameter.
	if(specialization_definition && specialization_arguments !=
		specialization_arguments_.end()) {
		size_t argument_index = 0;
		for(size_t parameter = 0; parameter < specialization_definition->parameters.size() &&
			parameter < specialization_arguments->second.size(); ++parameter)
		{
			const TemplateParameter& template_parameter =
				specialization_definition->parameters[parameter];
			if(template_parameter.pack) {
				vector<string> values;
				size_t trailing_fixed = 0;
				for(size_t later = parameter + 1;
					later < specialization_definition->parameters.size(); ++later)
					if(!specialization_definition->parameters[later].pack) ++trailing_fixed;
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
		if(!specialization_definition->name.empty())
			class_substitutions[specialization_definition->name] = class_key;
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
			if(incomplete_concrete || (specialization_definition &&
				(specialization_definition->name.find("enable_if") != string::npos ||
					specialization_definition->name.find("disable_if") != string::npos)))
				selected_specialization_declaration = selected_specialization->declaration;
			map<string, string> specialized_bindings;
			if(MatchClassSpecializationPattern(*selected_specialization,
				specialization_arguments->second, &specialized_bindings, context))
				for(map<string, string>::const_iterator binding = specialized_bindings.begin();
					binding != specialized_bindings.end(); ++binding)
					class_substitutions[binding->first] = binding->second;
		}
	}
	if(specialization != specialization_bases_.end() && incomplete_concrete)
		class_key = specialization->second;
	const string active_key = class_key + "|" + member;
	if(!active->insert(active_key).second) return false;
	CPPGMAstNodePtr declaration = selected_specialization_declaration ?
		selected_specialization_declaration : FindClassDeclaration(class_key, context);
	if(!declaration) {
		active->erase(active_key);
		return false;
	}
	map<string, string> declaration_substitutions = class_substitutions;
	map<string, vector<string> >::const_iterator materialized_arguments =
		specialization_arguments_.find(LastComponent(class_key));
	if(materialized_arguments != specialization_arguments_.end())
		for(size_t argument = 0; argument < materialized_arguments->second.size(); ++argument) {
			const string value = CanonicalSpelling(materialized_arguments->second[argument]);
			if(value.empty() || value.find_first_not_of(
				"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != string::npos ||
				(class_contexts_.find(value) == class_contexts_.end() &&
					!FindClassDeclaration(value, context))) continue;
			if(declaration_substitutions.find(value) != declaration_substitutions.end())
				declaration_substitutions.erase(value);
		}
	const string declaration_context = PrefixComponent(class_key).empty() ?
		context : PrefixComponent(class_key);
	string fallback_type;
	for(size_t i = 0; i < declaration->children.size(); ++i) {
		const CPPGMAstNodePtr child = declaration->children[i];
		if(!child) continue;
		CPPGMAstNodePtr direct_child = child;
		while(direct_child && direct_child->kind == "template-declaration" &&
			direct_child->children.size() > 1)
			direct_child = direct_child->children[1];
		if(!aliases_only && direct_child && (direct_child->kind == "class-specifier" ||
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
				if(result->find("::") != string::npos && result->find('<') != string::npos)
					*result = NormalizeTypeArgument(const_cast<PA18TemplateExpander*>(this)->RewriteText(
						*result, context, declaration_substitutions, 0));
				active->erase(active_key);
				return !result->empty();
			}
		if(child->kind != "simple-declaration" || child->children.empty()) continue;
		if(aliases_only && !HasDeclarationSpecifier(child->children[0], "typedef"))
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
					DeclaratorTypeSpelling(base, init->children[0]), declaration_substitutions));
				if(result->find("::") != string::npos && result->find('<') != string::npos)
					*result = NormalizeTypeArgument(const_cast<PA18TemplateExpander*>(this)->RewriteText(
						*result, context, declaration_substitutions, 0));
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
							string generated_path = JoinPath(base_definition->owner, generated_name);
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
