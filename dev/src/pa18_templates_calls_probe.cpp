#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
using namespace std;
namespace pa18_templates_internal {

bool PA18TemplateExpander::ContainsSubstitutionIdentifier(
	const string& text, const map<string, string>& substitutions) const
{
	for(map<string, string>::const_iterator substitution = substitutions.begin();
		substitution != substitutions.end(); ++substitution) {
		const string& name = substitution->first;
		for(size_t position = text.find(name); position != string::npos;
			position = text.find(name, position + name.size())) {
			const bool left = position == 0 ||
				!IsIdentifierCharacter(text[position - 1]);
			const size_t end = position + name.size();
			if(left && (end == text.size() || !IsIdentifierCharacter(text[end]))) return true;
		}
	}
	return false;
}

bool PA18TemplateExpander::FunctionTemplateMoreSpecialized(
	const TemplateDefinition& lhs, const TemplateDefinition& rhs,
	const string& context) const
{
	const auto parameter_patterns = [this](const TemplateDefinition& definition,
		vector<string>* result, set<string>* names) {
		if(!result || !names || !definition.declaration) return false;
		const CPPGMAstNodePtr parameters = DescendantOfKind(
			FunctionDeclarator(definition.declaration), "parameter-clause");
		if(!parameters) return false;
		for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter)
			if(!definition.parameters[parameter].name.empty())
				names->insert(definition.parameters[parameter].name);
		for(size_t parameter = 0; parameter < parameters->children.size(); ++parameter) {
			const CPPGMAstNodePtr item = parameters->children[parameter];
			if(!item || item->kind != "parameter-declaration") continue;
			string pattern = ParameterTypeSpelling(item);
			if(IsFunctionParameterPack(item) && pattern.size() >= 3 &&
				pattern.compare(pattern.size() - 3, 3, "...") == 0)
				pattern.erase(pattern.size() - 3);
			result->push_back(CanonicalSpelling(pattern));
		}
		return true;
	};
	const auto can_deduce = [this, &context, &parameter_patterns](
		const TemplateDefinition& pattern_definition,
		const TemplateDefinition& argument_definition) {
		vector<string> patterns, arguments;
		set<string> names;
		if(!parameter_patterns(pattern_definition, &patterns, &names) ||
			!parameter_patterns(argument_definition, &arguments, &names)) return false;
		map<string, string> placeholders;
		size_t placeholder = 0;
		for(size_t parameter = 0; parameter < argument_definition.parameters.size(); ++parameter) {
			const string& name = argument_definition.parameters[parameter].name;
			if(name.empty()) continue;
			ostringstream value;
			value << "__pa22_partial_" << placeholder++;
			placeholders[name] = value.str();
		}
		for(size_t argument = 0; argument < arguments.size(); ++argument)
			arguments[argument] = CanonicalSpelling(ReplaceIdentifiers(arguments[argument], placeholders));
		for(size_t parameter = 0; parameter < patterns.size(); ++parameter) {
			if(parameter >= arguments.size()) return false;
			map<string, string> ignored;
			if(!MatchTypePattern(patterns[parameter], arguments[parameter], names,
				&ignored, context)) return false;
		}
		return patterns.size() == arguments.size();
	};
	try {
		const bool rhs_from_lhs = can_deduce(rhs, lhs);
		const bool lhs_from_rhs = can_deduce(lhs, rhs);
		return rhs_from_lhs && !lhs_from_rhs;
	} catch(const PA18SubstitutionFailure&) {
		return false;
	}
}

bool PA18TemplateExpander::PreserveFunctionLookupOrder(
	const vector<const TemplateDefinition*>& definitions, const string& context,
	const map<string, string>& substitutions) const
{
	if(HasReplayContext(substitutions)) return false;
	map<string, CPPGMAstNodePtr>::const_iterator enclosing =
		function_definitions_.find(context);
	if(enclosing == function_definitions_.end() || !enclosing->second ||
		enclosing->second->source_token_begin == static_cast<size_t>(-1)) return false;
	const size_t visibility = enclosing->second->source_token_begin;
	for(size_t candidate = 0; candidate < definitions.size(); ++candidate) {
		const TemplateDefinition* definition = definitions[candidate];
		if(definition && definition->declaration &&
			definition->declaration->source_token_begin != static_cast<size_t>(-1) &&
			definition->declaration->source_token_begin > visibility) return true;
	}
	return false;
}

void PA18TemplateExpander::SortFunctionTemplateCandidates(
	vector<const TemplateDefinition*>* candidates, const string& context) const
{
	if(!candidates) return;
	stable_sort(candidates->begin(), candidates->end(),
		[this, &context](const TemplateDefinition* lhs, const TemplateDefinition* rhs) {
			if(!lhs || !rhs || lhs->parameters.empty() || rhs->parameters.empty()) return false;
			if(FunctionTemplateMoreSpecialized(*lhs, *rhs, context)) return true;
			if(FunctionTemplateMoreSpecialized(*rhs, *lhs, context)) return false;
			return false;
		});
}

int PA18TemplateExpander::MemberTemplatePatternScore(
	const TemplateDefinition* candidate) const
{
	if(!candidate || !candidate->declaration) return 0;
	const CPPGMAstNodePtr parameters = DescendantOfKind(
		FunctionDeclarator(candidate->declaration), "parameter-clause");
	if(!parameters) return 0;
	int score = 0;
	for(size_t parameter = 0; parameter < parameters->children.size(); ++parameter) {
		const CPPGMAstNodePtr item = parameters->children[parameter];
		if(!item || item->kind != "parameter-declaration") continue;
		const string pattern = CanonicalSpelling(ParameterTypeSpelling(item));
		if(pattern.find('<') != string::npos) score += 32;
		if(pattern.find('*') != string::npos) score += 8;
		if(pattern.find('&') != string::npos) score += 4;
		if(pattern.compare(0, 6, "const ") == 0 ||
			pattern.compare(0, 9, "volatile ") == 0) score += 2;
		for(size_t template_parameter = 0;
			template_parameter < candidate->parameters.size(); ++template_parameter)
			if(!candidate->parameters[template_parameter].name.empty() &&
				pattern == candidate->parameters[template_parameter].name) {
				score -= 16;
				break;
			}
	}
	return score;
}

void PA18TemplateExpander::RestoreMemberTemplateDefaults(
	const string& member_name, const TemplateDefinition& definition,
	TemplateDefinition* result) const
{
	if(!result) return;
	map<string, vector<string> >::const_iterator indexed =
		definitions_by_name_.find(member_name);
	if(indexed == definitions_by_name_.end()) return;
	const string definition_signature = MemberSignatureKey(definition);
	const string owner_qualifier = definition.owner + "::";
	for(size_t index = 0; index < indexed->second.size(); ++index) {
		map<string, TemplateDefinition>::const_iterator found = definitions_.find(
			indexed->second[index]);
		if(found == definitions_.end() || !found->second.declaration) continue;
		const TemplateDefinition& declaration = found->second;
		if(declaration.declaration->kind != "simple-declaration" &&
			declaration.declaration->kind != "special-member-declaration") continue;
		if(LastComponent(StripTemplateArgumentsForValidation(declaration.owner)) !=
			LastComponent(StripTemplateArgumentsForValidation(definition.owner))) continue;
		string declaration_signature = MemberSignatureKey(declaration);
		for(size_t qualifier = declaration_signature.find(owner_qualifier);
			qualifier != string::npos;
			qualifier = declaration_signature.find(owner_qualifier, qualifier + 1))
			declaration_signature.erase(qualifier, owner_qualifier.size());
		if(declaration_signature != definition_signature) continue;
		for(size_t parameter = 0; parameter < result->parameters.size() &&
			parameter < declaration.parameters.size(); ++parameter)
			if(result->parameters[parameter].default_type.empty())
				result->parameters[parameter].default_type =
					declaration.parameters[parameter].default_type;
		return;
	}
}

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
