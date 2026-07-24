#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::IsBuiltinArithmeticType(string raw) const
{
	raw = CanonicalSpelling(raw);
	while(raw.compare(0, 6, "const ") == 0)
		raw = CanonicalSpelling(raw.substr(6));
	while(raw.compare(0, 9, "volatile ") == 0)
		raw = CanonicalSpelling(raw.substr(9));
	return raw == "bool" || raw == "char" || raw == "signed char" ||
		raw == "unsigned char" || raw == "short" || raw == "short int" ||
		raw == "unsigned short" || raw == "unsigned short int" ||
		raw == "int" || raw == "unsigned" || raw == "unsigned int" ||
	raw == "long" || raw == "long int" || raw == "unsigned long" ||
	raw == "unsigned long int" || raw == "long long" ||
	raw == "long long int" || raw == "unsigned long long" ||
	raw == "unsigned long long int" || raw == "float" ||
	raw == "double" || raw == "long double";
}

string PA18TemplateExpander::CommonBuiltinArithmeticType(const string& left,
	const string& right) const
{
	const string a = CanonicalSpelling(left);
	const string b = CanonicalSpelling(right);
	if(a == b) return a;
	if(a == "long double" || b == "long double") return "long double";
	if(a == "double" || b == "double") return "double";
	if(a == "float" || b == "float") return "float";
	if(a.find("long long") != string::npos || b.find("long long") != string::npos)
		return a.find("unsigned") != string::npos || b.find("unsigned") != string::npos ?
			"unsigned long long int" : "long long int";
	if(a.find("long") != string::npos || b.find("long") != string::npos)
		return a.find("unsigned") != string::npos || b.find("unsigned") != string::npos ?
			"unsigned long int" : "long int";
	return a.find("unsigned") != string::npos || b.find("unsigned") != string::npos ?
		"unsigned int" : "int";
}

bool PA18TemplateExpander::InferOperatorResult(const string& operation,
	const string& left, const string& right, const string& context, string* result) const
{
	if(operation.empty() || !result) return false;
	const string name = "operator" + operation;
	const set<string> no_template_parameters;
	CPPGMAstNodePtr left_declaration = FindClassDeclaration(left, context);
	if(left_declaration) {
		for(size_t i = 0; i < left_declaration->children.size(); ++i) {
			const CPPGMAstNodePtr declaration = left_declaration->children[i];
			if(!declaration || declaration->kind != "function-definition" ||
				declaration->children.size() < 2 ||
				LastComponent(FirstIdentifierLocal(declaration->children[1])) != name) continue;
			const CPPGMAstNodePtr parameters = DescendantOfKind(declaration->children[1],
				"parameter-clause");
			size_t total = 0;
			size_t required = 0;
			if(!FunctionParameterCounts(parameters, &total, &required) || total != 1)
				continue;
			CPPGMAstNodePtr parameter;
			for(size_t p = 0; p < parameters->children.size(); ++p)
				if(parameters->children[p] && parameters->children[p]->kind ==
					"parameter-declaration") {
					parameter = parameters->children[p];
					break;
				}
			if(!parameter) continue;
			map<string, string> inferred;
			if(!MatchTypePattern(ParameterTypeSpelling(parameter), right,
				no_template_parameters, &inferred, context)) continue;
			*result = NormalizeTypeArgument(NodeTypeSpelling(declaration->children[0]) +
				DeclaratorSuffix(declaration->children[1]));
			return !result->empty();
		}
	}
	map<string, vector<string> >::const_iterator names = function_signatures_by_name_.find(name);
	if(names == function_signatures_by_name_.end()) return false;
	for(size_t name_index = 0; name_index < names->second.size(); ++name_index) {
		map<string, FunctionSignature>::const_iterator it = function_signatures_.find(
			names->second[name_index]);
		if(it == function_signatures_.end()) continue;
		const CPPGMAstNodePtr parameters = it->second.parameters;
		size_t total = 0;
		size_t required = 0;
		if(!FunctionParameterCounts(parameters, &total, &required) || total != 2)
			continue;
		CPPGMAstNodePtr first;
		CPPGMAstNodePtr second;
		for(size_t p = 0; p < parameters->children.size(); ++p) {
			const CPPGMAstNodePtr parameter = parameters->children[p];
			if(!parameter || parameter->kind != "parameter-declaration") continue;
			if(!first) first = parameter;
			else {
				second = parameter;
				break;
			}
		}
		if(!first || !second) continue;
		map<string, string> inferred;
		if(!MatchTypePattern(ParameterTypeSpelling(first), left,
			no_template_parameters, &inferred, context) ||
			!MatchTypePattern(ParameterTypeSpelling(second), right,
				no_template_parameters, &inferred, context)) continue;
		*result = NormalizeTypeArgument(NodeTypeSpelling(it->second.result_specifiers));
		return !result->empty();
	}
	return false;
}

bool PA18TemplateExpander::InferTemplateOperatorResult(const string& operation,
	const CPPGMAstNodePtr& left_expression, const CPPGMAstNodePtr& right_expression,
	const map<string, string>& substitutions, const string& context, string* result) const
{
	if(operation.empty() || !left_expression || !right_expression || !result) return false;
	const vector<const TemplateDefinition*> candidates = FindFunctionDefinitions(
		"operator" + operation, context);
	if(candidates.empty()) return false;
	CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
	call->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("id-expression",
		"operator" + operation)));
	CPPGMAstNodePtr arguments(new CPPGMAstNode("argument-list"));
	arguments->children.push_back(left_expression);
	arguments->children.push_back(right_expression);
	call->children.push_back(arguments);
	for(size_t i = 0; i < candidates.size(); ++i) {
		vector<string> inferred;
		if(!InferFunctionArguments(*candidates[i], call, &inferred,
			substitutions, context)) continue;
		if(!candidates[i]->declaration || candidates[i]->declaration->children.empty()) continue;
		string type = NodeTypeSpelling(candidates[i]->declaration->children[0]);
		const CPPGMAstNodePtr declarator = FunctionDeclarator(candidates[i]->declaration);
		type += DeclaratorSuffix(declarator);
		map<string, string> local = substitutions;
		for(size_t parameter = 0; parameter < candidates[i]->parameters.size() &&
			parameter < inferred.size(); ++parameter)
			if(!candidates[i]->parameters[parameter].name.empty())
				local[candidates[i]->parameters[parameter].name] = inferred[parameter];
		*result = NormalizeTypeArgument(ResolveAlias(ReplaceIdentifiers(type, local), context));
		return !result->empty();
	}
	return false;
}

} // namespace pa18_templates_internal
