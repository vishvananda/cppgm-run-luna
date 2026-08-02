#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;
namespace pa18_templates_internal {

unsigned PA18TemplateExpander::DeferredTemplateFacts(
	const TemplateDefinition* source_definition,
	const vector<string>& current_arguments, bool defer_class_definition) const
{
	unsigned facts = 0;
	for(size_t argument = 0; argument < current_arguments.size() &&
		!(facts & 2u); ++argument) {
		const string source_argument = CanonicalSpelling(current_arguments[argument]);
		string function_result, function_qualifiers;
		vector<string> function_parameters;
		if(SplitDirectFunctionType(source_argument, &function_result,
			&function_parameters, &function_qualifiers)) {
			facts |= 2u;
			break;
		}
		for(size_t nested_open = source_argument.find('<');
			nested_open != string::npos && !(facts & 2u);
			nested_open = source_argument.find('<', nested_open + 1)) {
			string nested_arguments;
			size_t nested_close = string::npos;
			if(!TemplateRange(source_argument, nested_open, &nested_arguments,
				&nested_close)) continue;
			const vector<string> nested = SplitTemplateArguments(nested_arguments);
			for(size_t nested_argument = 0; nested_argument < nested.size(); ++nested_argument)
				if(SplitDirectFunctionType(nested[nested_argument], &function_result,
					&function_parameters, &function_qualifiers)) {
					facts |= 2u;
					break;
				}
		}
	}
	if(!defer_class_definition || !source_definition || !source_definition->class_template)
		return facts;
	for(size_t node = 0; node < source_definition->dependent_member_type_nodes.size() &&
		!(facts & 1u); ++node) {
		const string dependent_spelling = CanonicalSpelling(RemoveMarker(
			source_definition->dependent_member_type_nodes[node]->value));
		for(size_t word_at = 0; word_at < dependent_spelling.size();) {
			if(!IsIdentifierCharacter(dependent_spelling[word_at])) {
				++word_at;
				continue;
			}
			const size_t word_begin = word_at;
			while(word_at < dependent_spelling.size() &&
				IsIdentifierCharacter(dependent_spelling[word_at])) ++word_at;
			if(dependent_spelling.compare(word_begin, word_at - word_begin,
				source_definition->name) == 0 && dependent_spelling.find_first_of(
				"+-*/%()[]&|!") != string::npos) {
				facts |= 1u;
				break;
			}
		}
	}
	for(size_t argument = 0; argument < current_arguments.size() &&
		!(facts & 4u) && argument < source_definition->parameters.size(); ++argument)
		if(!source_definition->parameters[argument].type &&
			!source_definition->parameters[argument].template_template) {
			const string& parameter_name = source_definition->parameters[argument].name;
			const string source_argument = CanonicalSpelling(current_arguments[argument]);
			const size_t parameter_at = parameter_name.empty() ? string::npos :
				source_argument.find(parameter_name);
			if(parameter_at != string::npos && source_argument.find_first_not_of(
				"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != string::npos &&
				(parameter_at == 0 || !IsIdentifierCharacter(source_argument[parameter_at - 1])) &&
				(parameter_at + parameter_name.size() == source_argument.size() ||
					!IsIdentifierCharacter(source_argument[parameter_at + parameter_name.size()])))
				facts |= 4u;
		}
	return facts;
}

string PA18TemplateExpander::BuildDeferredTemplateSpelling(
	const string& lookup_base, const string& base, const vector<string>& args,
	const vector<string>& raw_template_args, const string& context,
	const map<string, string>& substitutions, bool recursive_dependent_source) const
{
	string deferred_spelling = lookup_base.empty() ? base : lookup_base;
	deferred_spelling += "<";
	for(size_t argument = 0; argument < args.size(); ++argument) {
		if(argument) deferred_spelling += ",";
		string deferred_argument = args[argument];
		if(recursive_dependent_source && argument < raw_template_args.size())
			deferred_argument = CanonicalSpelling(ReplaceIdentifiersPreservingPackSizes(
				raw_template_args[argument], substitutions));
		if((base == "call" || base == "next") && argument < raw_template_args.size()) {
			const string& source_argument = raw_template_args[argument];
			map<string, string> deferred_substitutions = substitutions;
			for(size_t token_at = 0; token_at < source_argument.size();) {
				if(!IsIdentifierCharacter(source_argument[token_at])) {
					++token_at;
					continue;
				}
				const size_t token_begin = token_at;
				while(token_at < source_argument.size() &&
					IsIdentifierCharacter(source_argument[token_at])) ++token_at;
				const string token = source_argument.substr(token_begin, token_at - token_begin);
				map<string, string>::const_iterator binding = substitutions.find(token);
				if(binding == substitutions.end() || !binding->second.empty()) continue;
				size_t after_token = token_at;
				while(after_token < source_argument.size() && isspace(
					static_cast<unsigned char>(source_argument[after_token]))) ++after_token;
				const TemplateDefinition* token_definition = after_token < source_argument.size() &&
					source_argument[after_token] == '<' ? FindDefinition(token, context) : 0;
				if(token_definition && token_definition->class_template)
					deferred_substitutions.erase(token);
			}
			deferred_argument = CanonicalSpelling(ReplaceIdentifiersPreservingPackSizes(
				source_argument, deferred_substitutions));
		}
		deferred_spelling += deferred_argument;
	}
	return deferred_spelling + ">";
}

bool PA18TemplateExpander::RewriteDeferredTemplate(
	string* raw, size_t begin, size_t close, const string& base,
	const string& lookup_base, const vector<string>& args,
	const vector<string>& raw_template_args, const string& context,
	const map<string, string>& substitutions, bool class_template,
	unsigned deferred_facts, bool defer_class_definition,
	bool* template_replaced, size_t* search) const
{
	if(!raw || !search || !defer_class_definition || !class_template ||
		!(deferred_facts & 3u) || (close + 2 < raw->size() &&
			raw->compare(close + 1, 2, "::") == 0)) return false;
	const string deferred_spelling = BuildDeferredTemplateSpelling(lookup_base,
		base, args, raw_template_args, context, substitutions,
		(deferred_facts & 1u) != 0);
	raw->replace(begin, close - begin + 1, deferred_spelling);
	if(template_replaced) *template_replaced = true;
	*search = begin + deferred_spelling.size();
	return true;
}

} // namespace pa18_templates_internal
