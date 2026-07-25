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
					} catch(const logic_error& error) {
						throw logic_error("definition=" + definition.qualified_name +
							" " + error.what());
					}
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
		string argument;
		string source_type_argument;
		PA19IntegralValue integral_value;
		if(raw_index < raw_args.size() && !raw_args[raw_index].empty())
			source_type_argument = argument = raw_args[raw_index++];
		else if(!parameter.name.empty()) {
			map<string, string>::const_iterator substituted = substitutions->find(parameter.name);
			if(substituted != substitutions->end()) argument = substituted->second;
			map<string, PA19IntegralValue>::const_iterator integral =
				integral_substitutions->find(parameter.name);
			if(argument.empty() && integral != integral_substitutions->end())
				argument = TemplateIntegralValueSpelling(integral->second);
		}
		if(argument.empty()) argument = parameter.default_type;
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
				argument = ResolveIntegralArgument(parameter, argument, context, *substitutions,
					&integral_value);
			} catch(const logic_error& error) {
				throw logic_error("definition=" + definition.qualified_name +
					" " + error.what());
			}
			if(!parameter.name.empty()) (*integral_substitutions)[parameter.name] = integral_value;
		}
		if(definition.alias_template && parameter.type && !source_type_argument.empty() &&
			!ResolveAlias(source_type_argument, context).empty() &&
			ResolveAlias(source_type_argument, context).back() == '&') argument = source_type_argument;
		if(argument.empty()) throw logic_error("missing template argument");
		args->push_back(argument);
		metadata_args->push_back(TemplateArgumentMetadata(parameter, argument,
			integral_value, context, *substitutions));
		if(!parameter.name.empty()) (*substitutions)[parameter.name] = argument;
	}
	if(raw_index != raw_args.size()) throw logic_error("too many template arguments");
}

} // namespace pa18_templates_internal
