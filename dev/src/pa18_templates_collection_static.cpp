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
	const string semantic_owner = CollapseRepeatedQualifiedPath(
		CollapseRepeatedQualifier(CanonicalSpelling(RestoreSpecializationSpelling(owner))));
	const string static_lookup_key = (definition ? definition->qualified_name : string()) +
		"|" + semantic_owner + "|" + name;
	if(!active_static_member_lookups_.insert(static_lookup_key).second) return false;
	struct StaticLookupScope {
		set<string>* active;
		string key;
		StaticLookupScope(set<string>* value, const string& name)
			: active(value), key(name) {}
		~StaticLookupScope() { active->erase(key); }
	} static_lookup_scope(&active_static_member_lookups_, static_lookup_key);
	if(definition && definition->static_members.find(name) !=
		definition->static_members.end()) return true;
	set<string> active;
	return HasInheritedStaticMember(owner, name, &active);
}

} // namespace pa18_templates_internal
