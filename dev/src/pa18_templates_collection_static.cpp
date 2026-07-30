#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::ResolveMaterializedClassOwner(
	const string& source_base, const vector<string>& requested_arguments,
	const string& context, string* resolved_owner) const
{
	if(!resolved_owner) return false;
	resolved_owner->clear();
	string base = CanonicalSpelling(source_base);
	while(!base.empty() && base[0] == ':') base.erase(base.begin());
	if(base.empty()) return false;
	string generated_name;
	vector<string> arguments = requested_arguments;
	map<string, string>::const_iterator generated = specialization_bases_.find(
		LastComponent(base));
	map<string, vector<string> >::const_iterator generated_arguments =
		specialization_arguments_.find(LastComponent(base));
	if(base.find('<') == string::npos && generated != specialization_bases_.end() &&
		generated_arguments != specialization_arguments_.end()) {
		generated_name = LastComponent(base);
		arguments = generated_arguments->second;
		base = generated->second;
	}
	const size_t open = base.find('<');
	if(open != string::npos) base.erase(open);
	const TemplateDefinition* primary = FindDefinition(base, context);
	if(!primary) primary = FindDefinition(LastComponent(base), context);
	if(!primary || !primary->class_template) return false;
	vector<const TemplateDefinition*> definitions;
	definitions.push_back(primary);
	if(generated_name.empty() && !arguments.empty()) {
		const TemplateDefinition* selected = SelectClassTemplateDefinition(
			primary, arguments, context);
		if(selected && find(definitions.begin(), definitions.end(), selected) ==
			definitions.end()) definitions.push_back(selected);
	}
	const auto normalize_argument = [this, &context](const string& raw,
		const TemplateParameter* parameter) {
		string normalized = raw;
		if(parameter && !parameter->type && !parameter->template_template) {
			PA19IntegralValue value;
			map<string, string> substitutions;
			try {
				const string resolved = const_cast<PA18TemplateExpander*>(this)->
					ResolveIntegralArgument(*parameter, raw, context, substitutions, &value);
				if(!resolved.empty()) normalized = resolved;
			} catch(...) {
				if(const_cast<PA18TemplateExpander*>(this)->EvaluateIntegralText(
					raw, context, substitutions, &value) && value.known)
					normalized = TemplateIntegralValueSpelling(value);
			}
		}
		return CollapseRepeatedQualifiedPath(CollapseRepeatedQualifier(
			NormalizeTypeArgument(CanonicalSpelling(RestoreSpecializationSpelling(normalized)))));
	};
	const auto arguments_match = [&normalize_argument](const vector<string>& expected,
		const vector<string>& actual, const TemplateDefinition& definition) {
		if(expected.size() > actual.size()) return false;
		for(size_t argument = 0; argument < expected.size(); ++argument) {
			const TemplateParameter* parameter = argument < definition.parameters.size() ?
				&definition.parameters[argument] : 0;
			if(normalize_argument(expected[argument], parameter) !=
				normalize_argument(actual[argument], parameter))
				return false;
		}
		for(size_t argument = expected.size(); argument < actual.size(); ++argument) {
			if(argument >= definition.parameters.size()) return false;
			const TemplateParameter& parameter = definition.parameters[argument];
			if(!parameter.pack && parameter.default_type.empty()) return false;
		}
		return true;
	};
	const auto owner_path = [this, &context](const string& raw_owner) {
		string owner = CanonicalSpelling(raw_owner);
		const size_t owner_open = owner.find('<');
		if(owner_open == string::npos) return owner;
		string owner_arguments_text;
		size_t owner_close = string::npos;
		if(!TemplateRange(owner, owner_open, &owner_arguments_text, &owner_close))
			return owner;
		string owner_path_result;
		if(ResolveMaterializedClassOwner(owner.substr(0, owner_open),
			SplitTemplateArguments(owner_arguments_text), context, &owner_path_result))
			return owner_path_result;
		return owner;
	};
	for(size_t definition_index = 0; definition_index < definitions.size();
		++definition_index) {
		const TemplateDefinition& definition = *definitions[definition_index];
		map<string, vector<string> >::const_iterator names =
			specialization_names_by_base_.find(LastComponent(definition.qualified_name));
		if(names == specialization_names_by_base_.end()) continue;
		for(size_t name_index = 0; name_index < names->second.size(); ++name_index) {
			const string& candidate_name = names->second[name_index];
			map<string, string>::const_iterator candidate_base =
				specialization_bases_.find(candidate_name);
			map<string, vector<string> >::const_iterator candidate_arguments =
				specialization_arguments_.find(candidate_name);
			if(candidate_base == specialization_bases_.end() ||
				candidate_arguments == specialization_arguments_.end()) continue;
			string candidate_source = candidate_base->second;
			const size_t candidate_open = candidate_source.find('<');
			if(candidate_open != string::npos) candidate_source.erase(candidate_open);
			if(candidate_source != definition.qualified_name) continue;
			if(generated_name.empty() && !arguments_match(arguments,
				candidate_arguments->second, definition)) continue;
			if(!generated_name.empty() && candidate_name != generated_name) continue;
			const string owners[] = {definition.owner, definition.lexical_owner};
			for(size_t owner_index = 0; owner_index < sizeof(owners) / sizeof(owners[0]);
				++owner_index) {
				const string path = JoinPath(owner_path(owners[owner_index]), candidate_name);
				map<string, CPPGMAstNodePtr>::const_iterator declaration =
					class_declarations_.find(path);
				if(declaration == class_declarations_.end()) continue;
				if(resolved_owner->empty()) {
					*resolved_owner = path;
					continue;
				}
				if(*resolved_owner != path) {
					map<string, CPPGMAstNodePtr>::const_iterator prior =
						class_declarations_.find(*resolved_owner);
					if(prior == class_declarations_.end() || prior->second != declaration->second)
						return false;
				}
			}
			if(!resolved_owner->empty()) return true;
		}
	}
	return false;
}

bool PA18TemplateExpander::HasStaticMember(const TemplateDefinition* definition,
	const string& owner, const string& name) const
{
	if(name.empty()) return false;
	if(definition && definition->static_members.find(name) !=
		definition->static_members.end()) return true;
	set<string> active;
	const function<bool(const string&)> inherited_static =
		[&](const string& raw_owner) {
			string current = CanonicalSpelling(raw_owner);
			while(current.compare(0, 6, "const ") == 0)
				current = CanonicalSpelling(current.substr(6));
			while(current.compare(0, 9, "volatile ") == 0)
				current = CanonicalSpelling(current.substr(9));
			while(!current.empty() && (current[current.size() - 1] == '*' ||
				current[current.size() - 1] == '&')) current.erase(current.size() - 1);
			current = CanonicalSpelling(current);
			if(current.empty()) return false;
			const string active_key = current + "|" + name;
			if(!active.insert(active_key).second) return false;
			// A partially replayed function-type pack can leave an ellipsis on a
			// concrete template argument (`is_same<T, U...>`).  Recover the typed
			// materialized specialization before walking its inherited bases.
			const size_t source_open = current.find('<');
			if(source_open != string::npos) {
				string source_base, source_arguments;
				size_t source_begin = 0, source_close = string::npos;
				if(TemplateBase(current, source_open, &source_begin, &source_base) &&
					TemplateRange(current, source_open, &source_arguments, &source_close)) {
					vector<string> requested = SplitTemplateArguments(source_arguments);
					for(size_t argument = 0; argument < requested.size(); ++argument) {
						requested[argument] = CanonicalSpelling(requested[argument]);
						for(size_t ellipsis = requested[argument].find("...");
							ellipsis != string::npos;
							ellipsis = requested[argument].find("...", ellipsis))
							requested[argument].erase(ellipsis, 3);
						requested[argument] = NormalizeTypeArgument(requested[argument]);
					}
					map<string, vector<string> >::const_iterator generated_names =
						specialization_names_by_base_.find(LastComponent(source_base));
					if(generated_names != specialization_names_by_base_.end())
					for(size_t generated_index = 0;
						generated_index < generated_names->second.size(); ++generated_index) {
						const string& generated_name = generated_names->second[generated_index];
						map<string, string>::const_iterator generated =
							specialization_bases_.find(generated_name);
						if(generated == specialization_bases_.end()) continue;
						string generated_base = generated->second;
						const size_t generated_open = generated_base.find('<');
						if(generated_open != string::npos) generated_base.erase(generated_open);
						if(LastComponent(generated_base) != LastComponent(source_base)) continue;
						map<string, vector<string> >::const_iterator arguments =
							specialization_arguments_.find(generated->first);
						if(arguments == specialization_arguments_.end() ||
							arguments->second.size() != requested.size()) continue;
						bool same = true;
						for(size_t argument = 0; argument < requested.size(); ++argument) {
							string actual = CanonicalSpelling(arguments->second[argument]);
							for(size_t ellipsis = actual.find("..."); ellipsis != string::npos;
								ellipsis = actual.find("...", ellipsis)) actual.erase(ellipsis, 3);
							actual = NormalizeTypeArgument(actual);
							if(actual != requested[argument]) { same = false; break; }
						}
						if(!same) continue;
						const TemplateDefinition* generated_definition =
							FindDefinition(generated->second, "");
						if(generated_definition) {
							const string generated_path = JoinPath(generated_definition->owner,
								generated_name);
							if(class_declarations_.find(generated_path) != class_declarations_.end())
								current = generated_path;
							else if(!generated_definition->lexical_owner.empty()) {
								const string lexical_path = JoinPath(
									generated_definition->lexical_owner, generated_name);
								if(class_declarations_.find(lexical_path) != class_declarations_.end())
									current = lexical_path;
							}
						}
						break;
					}
					if(current.find("...") != string::npos && !requested.empty()) {
						const TemplateDefinition* source_definition = FindDefinition(source_base, "");
						if(source_definition && source_definition->class_template) {
							const TemplateDefinition* selected = SelectClassTemplateDefinition(
								source_definition, requested, "");
							if(selected) try {
								const string generated = const_cast<PA18TemplateExpander*>(this)->Instantiate(
									*selected, requested, "");
								const string qualified = selected->owner.empty() ? generated :
									JoinPath(selected->owner, generated);
								if(class_declarations_.find(qualified) != class_declarations_.end())
									current = qualified;
								else if(class_declarations_.find(generated) != class_declarations_.end())
									current = generated;
							} catch(const PA18SubstitutionFailure&) {
							} catch(const logic_error&) {
							}
						}
					}
				}
			}
			const string resolved_key = current + "|" + name;
			if(resolved_key != active_key && !active.insert(resolved_key).second) return false;
			map<string, set<string> >::const_iterator indexed =
				static_members_by_class_.find(current);
			if(indexed != static_members_by_class_.end() &&
				indexed->second.find(name) != indexed->second.end()) return true;
			const string resolved_current = CanonicalSpelling(ResolveAlias(current, ""));
			if(!resolved_current.empty() && resolved_current != current &&
				inherited_static(resolved_current)) return true;
			map<string, CPPGMAstNodePtr>::const_iterator declaration =
				class_declarations_.find(current);
			if(declaration == class_declarations_.end() || !declaration->second) return false;
			for(size_t child = 0; child < declaration->second->children.size(); ++child) {
				const CPPGMAstNodePtr clause = declaration->second->children[child];
				if(!clause || clause->kind != "base-clause") continue;
				for(size_t base = 0; base < clause->children.size(); ++base) {
					const CPPGMAstNodePtr base_name = clause->children[base] &&
						clause->children[base]->kind == "base-specifier" ?
						DescendantOfKind(clause->children[base], "base-name") :
						CPPGMAstNodePtr();
					if(!base_name || base_name->value.empty()) continue;
					if(inherited_static(RemoveMarker(base_name->value))) return true;
				}
			}
			return false;
		};
	return inherited_static(owner);
}

} // namespace pa18_templates_internal
