#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

static string StripOrdinaryCallConversionType(string raw)
{
	raw = CanonicalSpelling(raw);
	while(raw.compare(0, 6, "const ") == 0 ||
		raw.compare(0, 9, "volatile ") == 0) {
		const size_t space = raw.find(' ');
		if(space == string::npos) break;
		raw = CanonicalSpelling(raw.substr(space + 1));
	}
	while(raw.size() > 6 && raw.compare(raw.size() - 6, 6, " const") == 0)
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 6));
	while(raw.size() > 9 && raw.compare(raw.size() - 9, 9, " volatile") == 0)
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 9));
	while(raw.size() > 5 && raw.compare(raw.size() - 5, 5, "const") == 0 &&
		!IsIdentifierCharacter(raw[raw.size() - 6]))
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 5));
	while(raw.size() > 8 && raw.compare(raw.size() - 8, 8, "volatile") == 0 &&
		!IsIdentifierCharacter(raw[raw.size() - 9]))
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 8));
	while(!raw.empty() && (raw[raw.size() - 1] == '&' ||
		raw[raw.size() - 1] == '*'))
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 1));
	return raw;
}

bool PA18TemplateExpander::PreserveUnresolvedExplicitTemplateCall(
	const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& result,
	const vector<string>& explicit_arguments, const string& context,
	const map<string, string>& explicit_substitutions,
	const map<string, string>& substitutions)
{
	if(!input || input->children.empty() || !result) return false;
	for(size_t i = 0; i < explicit_arguments.size(); ++i)
		if(HasUnresolvedTemplateParameter(explicit_arguments[i], context,
			explicit_substitutions)) {
			result->children.push_back(CloneNode(input->children[0]));
			for(size_t child = 1; child < input->children.size(); ++child) {
				CPPGMAstNodePtr transformed = TransformNode(input->children[child],
					context, substitutions);
				if(transformed) result->children.push_back(transformed);
			}
			return true;
		}
	return false;
}

void PA18TemplateExpander::MaterializeOrdinaryCallConversions(
	const string& callee_name, const CPPGMAstNodePtr& result,
	const vector<const TemplateDefinition*>& definitions, const string& context,
	const map<string, string>& substitutions)
{
	if(!result) return;
	const auto materialize_conversion = [&](const string& raw_parameter,
		const CPPGMAstNodePtr& argument) {
		if(!argument) return;
		string target_type;
		try {
			target_type = StripOrdinaryCallConversionType(RewriteText(raw_parameter,
				context, substitutions, 0));
		} catch(const PA18SubstitutionFailure&) {
			return;
		}
		if(target_type.empty() || !FindClassDeclaration(target_type, context)) return;
		string source_type;
		try {
			if(!InferArgument(argument, &source_type, substitutions, context)) return;
			source_type = StripOrdinaryCallConversionType(ResolveAlias(RewriteText(
				source_type, context, substitutions, 0), context));
			if(!FindClassDeclaration(source_type, context))
				source_type = StripOrdinaryCallConversionType(QualifyTypeArgument(
					source_type, context));
			if(!FindClassDeclaration(source_type, context)) {
				map<string, string>::const_iterator target_base = specialization_bases_.find(
					LastComponent(target_type));
				if(target_base != specialization_bases_.end()) {
					const string prefix = PrefixComponent(target_base->second);
					if(!prefix.empty() && source_type.find("::") == string::npos)
						source_type = prefix + "::" + source_type;
				}
			}
		} catch(const PA18SubstitutionFailure&) {
			return;
		}
		if(source_type.empty() || !FindClassDeclaration(source_type, context) ||
			source_type == target_type) return;
		string constructor_name = LastComponent(target_type);
		map<string, string>::const_iterator base = specialization_bases_.find(
			constructor_name);
		if(base != specialization_bases_.end())
			constructor_name = LastComponent(base->second);
		CPPGMAstNodePtr object(new CPPGMAstNode("id-expression"));
		object->inferred_type = target_type;
		CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "."));
		member->children.push_back(object);
		member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
			"identifier", constructor_name)));
		CPPGMAstNodePtr conversion_call(new CPPGMAstNode("call-expression"));
		conversion_call->children.push_back(member);
		CPPGMAstNodePtr arguments(new CPPGMAstNode("argument-list"));
		arguments->children.push_back(argument);
		conversion_call->children.push_back(arguments);
		try {
			(void)InstantiateMemberCall(conversion_call, member, constructor_name,
				context, substitutions);
		} catch(const PA18SubstitutionFailure&) {}
	};
	if(result->children.size() > 1 && result->children[1] &&
		result->children[1]->kind == "argument-list") {
		const vector<CPPGMAstNodePtr>& call_arguments = result->children[1]->children;
		for(size_t candidate = 0; candidate < definitions.size(); ++candidate) {
			if(!definitions[candidate] || !definitions[candidate]->declaration) continue;
			const CPPGMAstNodePtr clause = DescendantOfKind(
				FunctionDeclarator(definitions[candidate]->declaration), "parameter-clause");
			if(!clause) continue;
			size_t argument = 0;
			for(size_t parameter = 0; parameter < clause->children.size() &&
				argument < call_arguments.size(); ++parameter) {
				const CPPGMAstNodePtr parameter_node = clause->children[parameter];
				if(!parameter_node || parameter_node->kind != "parameter-declaration") continue;
				materialize_conversion(ParameterTypeSpelling(parameter_node),
					call_arguments[argument++]);
			}
		}
	}
	if(definitions.empty() && result->children.size() > 1 && result->children[1]) {
		const FunctionSignature* signature = FindFunctionSignature(callee_name, context);
		if(signature && signature->parameters) {
			size_t argument = 0;
			for(size_t parameter = 0; parameter < signature->parameters->children.size() &&
				argument < result->children[1]->children.size(); ++parameter) {
				const CPPGMAstNodePtr parameter_node = signature->parameters->children[parameter];
				if(!parameter_node || parameter_node->kind != "parameter-declaration") continue;
				materialize_conversion(ParameterTypeSpelling(parameter_node),
					result->children[1]->children[argument++]);
			}
		}
	}
}

}
