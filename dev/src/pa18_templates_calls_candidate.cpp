#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;
namespace pa18_templates_internal {

bool PA18TemplateExpander::ValidateMaterializedFreeFunctionCandidate(
	const TemplateDefinition& definition, const CPPGMAstNodePtr& result,
	const map<string, string>& substitutions, const string& context)
{
	const string viability_context = definition.owner.empty() ? context : definition.owner;
	const CPPGMAstNodePtr parameter_clause = DescendantOfKind(
		FunctionDeclarator(definition.declaration), "parameter-clause");
	if(!parameter_clause) return false;
	set<string> template_parameter_names;
	for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter)
		if(!definition.parameters[parameter].name.empty())
			template_parameter_names.insert(definition.parameters[parameter].name);
	const auto contains_template_parameter = [&](const string& pattern) {
		for(set<string>::const_iterator name = template_parameter_names.begin();
			name != template_parameter_names.end(); ++name)
			for(size_t at = pattern.find(*name); at != string::npos;
				at = pattern.find(*name, at + name->size())) {
				const bool left = at == 0 || !IsIdentifierCharacter(pattern[at - 1]);
				const size_t end = at + name->size();
				const bool right = end == pattern.size() || !IsIdentifierCharacter(pattern[end]);
				if(left && right) return true;
			}
		return false;
	};
	bool has_concrete_parameter = false;
	for(size_t parameter = 0; parameter < parameter_clause->children.size(); ++parameter) {
		const CPPGMAstNodePtr parameter_node = parameter_clause->children[parameter];
		if(!parameter_node || parameter_node->kind != "parameter-declaration") continue;
		if(!contains_template_parameter(ParameterTypeSpelling(parameter_node)))
			has_concrete_parameter = true;
	}
	if(!has_concrete_parameter) return true;
	vector<string> actual_types;
	const CPPGMAstNodePtr arguments = result->children.size() > 1 &&
		result->children[1] && result->children[1]->kind == "argument-list" ?
		result->children[1] : CPPGMAstNodePtr();
	if(!arguments) return false;
	for(size_t argument = 0; argument < arguments->children.size(); ++argument) {
		string actual;
		if(!InferArgument(arguments->children[argument], &actual, substitutions, context) ||
			actual.empty()) return false;
		actual_types.push_back(actual);
	}
	size_t actual_index = 0;
	for(size_t parameter = 0; parameter < parameter_clause->children.size(); ++parameter) {
		const CPPGMAstNodePtr parameter_node = parameter_clause->children[parameter];
		if(!parameter_node || parameter_node->kind == "ellipsis" ||
			parameter_node->kind != "parameter-declaration") continue;
		const string pattern = ParameterTypeSpelling(parameter_node);
		const bool dependent = contains_template_parameter(pattern);
		size_t visits = 1;
		if(IsFunctionParameterPack(parameter_node)) {
			size_t trailing_fixed = 0;
			for(size_t later = parameter + 1; later < parameter_clause->children.size(); ++later)
				if(parameter_clause->children[later] &&
					parameter_clause->children[later]->kind == "parameter-declaration" &&
					!IsFunctionParameterPack(parameter_clause->children[later])) ++trailing_fixed;
			const size_t available = actual_types.size() > actual_index ?
				actual_types.size() - actual_index : 0;
			visits = available > trailing_fixed ? available - trailing_fixed : 0;
		}
		for(size_t visit = 0; visit < visits && actual_index < actual_types.size(); ++visit) {
			if(!dependent) {
				string expected = RewriteText(pattern, viability_context, substitutions, 0);
				expected = NormalizeTypeArgument(ReplaceIdentifiers(expected, substitutions));
				if(!FunctionArgumentViable(expected, actual_types[actual_index],
					viability_context)) return false;
			}
			++actual_index;
		}
	}
	return actual_index == actual_types.size();
}
}
