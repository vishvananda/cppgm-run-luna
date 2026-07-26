#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

string PA18TemplateExpander::FunctionPointerAliasSpelling(const string& spelling,
	const string& context) const
{
	string result;
	const string canonical = CanonicalSpelling(spelling);
	if(canonical.empty() || canonical[canonical.size() - 1] != '*') return result;
	const string pointee = CanonicalSpelling(canonical.substr(0, canonical.size() - 1));
	const string resolved_pointee = CanonicalSpelling(ResolveAlias(pointee, context));
	string direct_result;
	vector<string> direct_parameters;
	string direct_qualifiers;
	if(SplitDirectFunctionType(resolved_pointee, &direct_result, &direct_parameters,
		&direct_qualifiers) || SplitFunctionPointerType(resolved_pointee, &direct_result,
		&direct_parameters)) result = pointee + "*";
	if(result.empty()) return result;
	string matching_alias;
	for(map<string, string>::const_iterator alias = type_aliases_.begin();
		alias != type_aliases_.end(); ++alias) {
		const string name = LastComponent(alias->first);
		if(name.empty() || CanonicalSpelling(ResolveAlias(name, context)) != resolved_pointee)
			continue;
		if(!matching_alias.empty() && matching_alias != name) return result;
		matching_alias = name;
	}
	return matching_alias.empty() ? result : matching_alias + "*";
}

bool PA18TemplateExpander::IsValidFunctionAddressTemplateArgument(
	const string& raw, const string& expected, const string& context,
	const map<string, string>& substitutions) const
{
	string address = CanonicalSpelling(ReplaceIdentifiers(raw, substitutions));
	if(address.size() < 4 || address[0] != '&') return false;
	const size_t separator = address.rfind("::");
	if(separator == string::npos || separator + 2 >= address.size()) return false;
	string owner = CanonicalSpelling(ResolveAlias(address.substr(1, separator - 1), context));
	const string member = LastComponent(address.substr(separator + 2));
	map<string, string>::const_iterator owner_base = specialization_bases_.find(LastComponent(owner));
	map<string, vector<string> >::const_iterator owner_arguments =
		specialization_arguments_.find(LastComponent(owner));
	string expected_result;
	vector<string> expected_parameters;
	if(!SplitFunctionPointerType(expected, &expected_result, &expected_parameters)) return false;
	const vector<const TemplateDefinition*> candidates = FindFunctionDefinitions(member, owner);
	for(size_t candidate = 0; candidate < candidates.size(); ++candidate) {
		if(!candidates[candidate] || candidates[candidate]->class_template) continue;
		vector<string> arguments;
		string source_expected = RestoreSpecializationSpelling(expected);
		const CPPGMAstNodePtr candidate_declarator = FunctionDeclarator(
			candidates[candidate]->declaration);
		if(owner_base != specialization_bases_.end() &&
			owner_arguments != specialization_arguments_.end()) {
			string source_owner = owner_base->second + "<";
			for(size_t argument = 0; argument < owner_arguments->second.size(); ++argument) {
				if(argument) source_owner += ", ";
				source_owner += RestoreSpecializationSpelling(owner_arguments->second[argument]);
			}
			source_owner += ">";
			string candidate_result;
			vector<string> candidate_parameters;
			if(candidate_declarator && !candidates[candidate]->declaration->children.empty()) {
				candidate_result = NodeTypeSpelling(candidates[candidate]->declaration->children[0]) +
					DeclaratorSuffix(candidate_declarator);
				const CPPGMAstNodePtr clause = DescendantOfKind(candidate_declarator,
					"parameter-clause");
				if(clause) for(size_t parameter = 0; parameter < clause->children.size(); ++parameter)
					if(clause->children[parameter] && clause->children[parameter]->kind ==
						"parameter-declaration") candidate_parameters.push_back(
						ParameterTypeSpelling(clause->children[parameter]));
			}
			if(CanonicalSpelling(candidate_result) == LastComponent(owner_base->second))
				candidate_result = source_owner;
			set<string> parameter_names;
			for(size_t parameter = 0; parameter < candidates[candidate]->parameters.size(); ++parameter)
				if(!candidates[candidate]->parameters[parameter].name.empty())
					parameter_names.insert(candidates[candidate]->parameters[parameter].name);
			map<string, string> inferred;
			string source_result;
			vector<string> source_parameters;
			if(SplitFunctionPointerType(source_expected, &source_result, &source_parameters) &&
				candidate_parameters.size() == source_parameters.size() &&
				MatchTypePattern(candidate_result, source_result, parameter_names, &inferred, context)) {
				bool matched = true;
				for(size_t parameter = 0; parameter < candidate_parameters.size(); ++parameter)
					if(!MatchTypePattern(candidate_parameters[parameter], source_parameters[parameter],
						parameter_names, &inferred, context)) {
						matched = false;
						break;
					}
				if(matched) for(size_t parameter = 0; parameter < candidates[candidate]->parameters.size(); ++parameter) {
					const TemplateParameter& formal = candidates[candidate]->parameters[parameter];
					if(formal.name.empty() || inferred.find(formal.name) != inferred.end() ||
						!formal.default_type.empty()) continue;
					matched = false;
					break;
				}
				if(matched) return true;
			}
		}
		if((InferFunctionFromExpected(*candidates[candidate], expected, &arguments, context) ||
			InferFunctionFromExpected(*candidates[candidate], source_expected, &arguments, context))) {
			return true;
		}
	}
	return false;
}

void PA18TemplateExpander::ResolveTemplateArguments(const TemplateDefinition& definition,
	const vector<string>& raw_args, const string& context,
	vector<string>* args, vector<string>* metadata_args,
	map<string, string>* substitutions,
	map<string, PA19IntegralValue>* integral_substitutions,
	map<string, vector<string> >* pack_substitutions,
	const map<string, vector<string> >* pack_hints)
{
	size_t raw_index = 0;
	for(size_t i = 0; i < definition.parameters.size(); ++i) {
		const TemplateParameter& parameter = definition.parameters[i];
		if(parameter.pack) {
			vector<string> values;
			size_t trailing_fixed = 0;
			for(size_t later = i + 1; later < definition.parameters.size(); ++later)
				if(!definition.parameters[later].pack) ++trailing_fixed;
			const size_t available = raw_args.size() > raw_index ?
				raw_args.size() - raw_index : 0;
			size_t count = available > trailing_fixed ? available - trailing_fixed : 0;
			if(pack_hints && !parameter.name.empty()) {
				map<string, vector<string> >::const_iterator hint =
					pack_hints->find(parameter.name);
				if(hint != pack_hints->end()) count = hint->second.size();
			}
			for(size_t element = 0; element < count; ++element) {
				string argument = raw_index < raw_args.size() ? raw_args[raw_index++] : string();
				if(argument.empty() && pack_hints && !parameter.name.empty()) {
					map<string, vector<string> >::const_iterator hint = pack_hints->find(parameter.name);
					if(hint != pack_hints->end() && element < hint->second.size()) argument = hint->second[element];
				}
				if(argument.empty()) throw logic_error("missing template pack argument");
				PA19IntegralValue integral_value;
				if(parameter.template_template) {
					string normalized;
					if(!CompatibleTemplateTemplateArgument(parameter, argument, context,
						*substitutions, &normalized))
						throw logic_error("template-template argument does not match");
					argument = normalized;
				} else if(parameter.type) {
					argument = RewriteText(argument, context, *substitutions, 0);
					argument = NormalizeTypeArgument(argument);
					argument = NormalizeTypeArgument(ReplaceIdentifiers(argument, *substitutions));
					const string function_pointer_alias = FunctionPointerAliasSpelling(argument, context);
					argument = ResolveAlias(argument, context);
					argument = RewriteText(argument, context, *substitutions, 0);
					argument = NormalizeTypeArgument(argument);
					if(!function_pointer_alias.empty()) argument = function_pointer_alias;
					argument = QualifyTypeArgument(argument, context, definition.owner);
					if(!function_pointer_alias.empty()) argument = function_pointer_alias;
				} else {
					try {
						argument = ResolveIntegralArgument(parameter, argument, context,
							*substitutions, &integral_value);
						} catch(const PA18SubstitutionFailure& error) { throw PA18SubstitutionFailure("definition=" + definition.qualified_name + " " + error.what());
						} catch(const logic_error& error) { throw logic_error("definition=" + definition.qualified_name + " " + error.what()); }
					if(!parameter.name.empty()) (*integral_substitutions)[parameter.name] = integral_value;
				}
				if(argument.empty()) throw logic_error("missing template argument");
				values.push_back(argument);
				args->push_back(argument);
				metadata_args->push_back(TemplateArgumentMetadata(parameter, argument,
					integral_value, context, *substitutions));
			}
			if(!parameter.name.empty()) {
				if(pack_substitutions) (*pack_substitutions)[parameter.name] = values;
				if(!values.empty()) (*substitutions)[parameter.name] = values[0];
				else substitutions->erase(parameter.name);
			}
			continue;
		}
		string argument, source_type_argument;
		PA19IntegralValue integral_value; bool from_default = false;
		if(raw_index < raw_args.size() && !raw_args[raw_index].empty()) source_type_argument = argument = raw_args[raw_index++];
		else if(!parameter.name.empty()) {
			map<string, string>::const_iterator substituted = substitutions->find(parameter.name);
			if(substituted != substitutions->end()) argument = substituted->second;
			map<string, PA19IntegralValue>::const_iterator integral = integral_substitutions->find(parameter.name);
			if(argument.empty() && integral != integral_substitutions->end()) argument = TemplateIntegralValueSpelling(integral->second);
		}
		if(argument.empty()) { argument = parameter.default_type; from_default = !argument.empty(); }
		if(!parameter.default_type.empty() && argument == parameter.default_type)
			from_default = true;
		const string argument_context = from_default && !definition.owner.empty() ? definition.owner : context;
		if(parameter.template_template) {
			string normalized;
			if(!CompatibleTemplateTemplateArgument(parameter, argument, context,
				*substitutions, &normalized))
				throw logic_error("template-template argument does not match");
			argument = normalized;
		} else if(parameter.type) {
			argument = ExpandPackCallText(argument, *pack_substitutions);
			argument = RewriteText(argument, context, *substitutions, 0);
			argument = NormalizeTypeArgument(argument);
			argument = NormalizeTypeArgument(ReplaceIdentifiers(argument, *substitutions));
			string function_pointer_alias = FunctionPointerAliasSpelling(source_type_argument, context);
			if(function_pointer_alias.empty()) function_pointer_alias =
				FunctionPointerAliasSpelling(argument, context);
			argument = ResolveAlias(argument, context);
			argument = RewriteText(argument, context, *substitutions, 0);
			argument = NormalizeTypeArgument(argument);
			if(!function_pointer_alias.empty()) argument = function_pointer_alias;
			argument = QualifyTypeArgument(argument, context, definition.owner);
			if(!function_pointer_alias.empty()) argument = function_pointer_alias;
		} else {
			try {
				try {
					argument = ResolveIntegralArgument(parameter, argument, argument_context,
						*substitutions, &integral_value);
				} catch(const PA18SubstitutionFailure&) {
					if(definition.owner.empty() || definition.owner == context) throw;
					argument = ResolveIntegralArgument(parameter, argument, definition.owner,
						*substitutions, &integral_value);
				}
			} catch(const PA18SubstitutionFailure& error) { throw PA18SubstitutionFailure("definition=" + definition.qualified_name + " " + error.what());
			} catch(const logic_error& error) { throw logic_error("definition=" + definition.qualified_name + " " + error.what()); }
			if(!parameter.name.empty()) (*integral_substitutions)[parameter.name] = integral_value;
		}
		if(definition.alias_template && parameter.type && !source_type_argument.empty() &&
			!ResolveAlias(source_type_argument, context).empty() &&
			ResolveAlias(source_type_argument, context).back() == '&') argument = source_type_argument;
		if(argument.empty()) throw logic_error("missing template argument");
		args->push_back(argument);
		metadata_args->push_back(TemplateArgumentMetadata(parameter, argument,
			integral_value, context, *substitutions));
		if(!parameter.name.empty()) (*substitutions)[parameter.name] = argument; }
	if(raw_index != raw_args.size()) throw logic_error("too many template arguments");
}

} // namespace pa18_templates_internal
