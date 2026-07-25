#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::InferFunctionTypeArguments(const TemplateDefinition& definition,
	const vector<string>& actual_types, vector<string>* result,
	const map<string, string>& substitutions, const string& context,
	const vector<string>* explicit_prefix)
{
	if(!result || definition.class_template) return false;
	const CPPGMAstNodePtr declarator = FunctionDeclarator(definition.declaration);
	const CPPGMAstNodePtr parameters = DescendantOfKind(declarator, "parameter-clause");
	if(!parameters) return false;
	size_t parameter_count = 0, required_parameters = 0;
	bool has_parameter_pack = false;
	for(size_t i = 0; i < parameters->children.size(); ++i) {
		const CPPGMAstNodePtr parameter = parameters->children[i];
		if(!parameter || parameter->kind != "parameter-declaration") continue;
		++parameter_count;
		if(IsFunctionParameterPack(parameter)) has_parameter_pack = true;
		else if(!ChildOfKindLocal(parameter, "default-argument")) ++required_parameters;
	}
	if(actual_types.size() < required_parameters ||
		(!has_parameter_pack && actual_types.size() > parameter_count)) return false;
	map<string, string> inferred;
	set<string> parameter_names, pack_parameter_names;
	for(size_t i = 0; i < definition.parameters.size(); ++i) {
		if(definition.parameters[i].name.empty()) continue;
		parameter_names.insert(definition.parameters[i].name);
		if(definition.parameters[i].pack) pack_parameter_names.insert(definition.parameters[i].name);
	}
	map<string, vector<string> > inferred_packs;
	if(explicit_prefix) {
		size_t explicit_index = 0;
		for(size_t i = 0; i < definition.parameters.size(); ++i) {
			const TemplateParameter& parameter = definition.parameters[i];
			if(parameter.name.empty()) continue;
			if(parameter.pack) {
				size_t trailing_fixed = 0;
				for(size_t later = i + 1; later < definition.parameters.size(); ++later)
					if(!definition.parameters[later].pack) ++trailing_fixed;
				const size_t remaining = explicit_prefix->size() > explicit_index ?
					explicit_prefix->size() - explicit_index : 0;
				const size_t count = remaining > trailing_fixed ? remaining - trailing_fixed : 0;
				for(size_t value = 0; value < count; ++value)
					inferred_packs[parameter.name].push_back((*explicit_prefix)[explicit_index++]);
			} else if(explicit_index < explicit_prefix->size())
				inferred[parameter.name] = (*explicit_prefix)[explicit_index++];
		}
	}
	size_t actual = 0;
	for(size_t i = 0; i < parameters->children.size(); ++i) {
		const CPPGMAstNodePtr parameter = parameters->children[i];
		if(!parameter || parameter->kind != "parameter-declaration") continue;
		const string pattern = ParameterTypeSpelling(parameter);
		if(IsFunctionParameterPack(parameter)) {
			size_t trailing_fixed = 0;
			for(size_t later = i + 1; later < parameters->children.size(); ++later)
				if(parameters->children[later] &&
					parameters->children[later]->kind == "parameter-declaration" &&
					!IsFunctionParameterPack(parameters->children[later])) ++trailing_fixed;
			const size_t available = actual_types.size() > actual ? actual_types.size() - actual : 0;
			const size_t visits = available > trailing_fixed ? available - trailing_fixed : 0;
			string pack_pattern = pattern;
			if(pack_pattern.size() >= 3 &&
				pack_pattern.compare(pack_pattern.size() - 3, 3, "...") == 0)
				pack_pattern.erase(pack_pattern.size() - 3);
			for(size_t visit = 0; visit < visits; ++visit) {
				map<string, string> one;
				if(!MatchTypePattern(pack_pattern, actual_types[actual + visit],
					parameter_names, &one, context)) return false;
				for(map<string, string>::const_iterator binding = one.begin();
					binding != one.end(); ++binding) {
					if(pack_parameter_names.find(binding->first) != pack_parameter_names.end())
						inferred_packs[binding->first].push_back(binding->second);
					else {
						map<string, string>::const_iterator prior = inferred.find(binding->first);
						if(prior != inferred.end() && CanonicalSpelling(ResolveAlias(
							prior->second, context)) != CanonicalSpelling(ResolveAlias(
							binding->second, context))) return false;
						inferred[binding->first] = binding->second;
					}
				}
			}
			actual += visits;
			continue;
		}
		if(actual >= actual_types.size()) break;
		bool dependent = false;
		for(size_t p = 0; p < definition.parameters.size() && !dependent; ++p) {
			const string& name = definition.parameters[p].name;
			for(size_t at = 0; at + name.size() <= pattern.size(); ++at)
				if(pattern.compare(at, name.size(), name) == 0 &&
					(at == 0 || !IsIdentifierCharacter(pattern[at - 1])) &&
					(at + name.size() == pattern.size() ||
						!IsIdentifierCharacter(pattern[at + name.size()]))) {
					dependent = true;
					break;
				}
		}
		if(dependent) {
			const string dependent_pattern = CanonicalSpelling(pattern);
			const string actual_type = CollapseReferenceSpelling(actual_types[actual]);
			if(dependent_pattern.size() > 2 &&
				dependent_pattern.compare(dependent_pattern.size() - 2, 2, "&&") == 0 &&
				!actual_type.empty() && actual_type[actual_type.size() - 1] == '&') {
				const string base = CanonicalSpelling(dependent_pattern.substr(
					0, dependent_pattern.size() - 2));
				if(parameter_names.find(base) != parameter_names.end()) inferred[base] = actual_type;
				else if(!MatchTypePattern(dependent_pattern, actual_type,
					parameter_names, &inferred, context)) return false;
			} else if(!MatchTypePattern(dependent_pattern, actual_type,
				parameter_names, &inferred, context)) return false;
		}
		++actual;
	}
	for(size_t i = 0; i < definition.parameters.size(); ++i) {
		if(definition.parameters[i].pack) {
			map<string, vector<string> >::const_iterator found = inferred_packs.find(
				definition.parameters[i].name);
			if(found != inferred_packs.end())
				result->insert(result->end(), found->second.begin(), found->second.end());
			continue;
		}
		map<string, string>::const_iterator found = inferred.find(definition.parameters[i].name);
		if(found != inferred.end()) result->push_back(found->second);
		else if(!definition.parameters[i].default_type.empty())
			result->push_back(RewriteText(definition.parameters[i].default_type,
				context, inferred, 0));
		else return false;
	}
	(void)substitutions;
	return true;
}

} // namespace pa18_templates_internal
