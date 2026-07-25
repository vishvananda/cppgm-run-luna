#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
using namespace std;
namespace pa18_templates_internal {

bool PA18TemplateExpander::ValidateExplicitFunctionCandidate(
	const TemplateDefinition& definition, const CPPGMAstNodePtr& input,
	const string& context, const map<string, string>& substitutions,
	const vector<string>& raw_explicit_args, vector<string>* arguments)
{
	if(!arguments) return false;
	try {
		if(!InferFunctionArguments(definition, input, arguments, substitutions, context,
			&raw_explicit_args)) return false;
		map<string, string> bindings;
		for(size_t parameter = 0; parameter < definition.parameters.size() &&
			parameter < arguments->size(); ++parameter)
			if(!definition.parameters[parameter].name.empty())
				bindings[definition.parameters[parameter].name] = (*arguments)[parameter];
		for(size_t parameter = raw_explicit_args.size();
			parameter < definition.parameters.size(); ++parameter) {
			const TemplateParameter& detail = definition.parameters[parameter];
			if(detail.default_type.empty() || parameter >= arguments->size()) continue;
			string value;
			const string default_type = CanonicalSpelling(detail.default_type);
			if(default_type.compare(0, 9, "decltype(") == 0 &&
				default_type.size() > 10 && default_type[default_type.size() - 1] == ')') {
				const string expression = default_type.substr(9, default_type.size() - 10);
				if(!EvaluateDecltypeExpression(expression, context, bindings, &value)) return false;
			} else value = RewriteText(detail.default_type, context, bindings, 0);
			if(value.empty()) return false;
			(*arguments)[parameter] = NormalizeTypeArgument(value);
			if(!detail.name.empty()) bindings[detail.name] = (*arguments)[parameter];
		}
		try {
			const string result = FunctionResultType(definition, *arguments, context, &substitutions);
			string probe = result;
			while(!probe.empty() && (probe[probe.size() - 1] == '*' || probe[probe.size() - 1] == '&')) probe.erase(probe.size() - 1);
			while(probe.size() > 6 && probe.compare(probe.size() - 6, 6, " const") == 0) probe.erase(probe.size() - 6);
			if(HasUnavailableGeneratedMemberType(probe, context, substitutions)) return false;
			return !result.empty();
		} catch(const PA18SubstitutionFailure&) { return false; }
	} catch(const PA18SubstitutionFailure&) {
		return false;
	}
}

} // namespace pa18_templates_internal
