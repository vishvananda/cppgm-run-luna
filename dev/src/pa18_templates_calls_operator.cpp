#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::InferBinaryArgument(const CPPGMAstNodePtr& expression,
	string* result, const map<string, string>& substitutions, const string& context) const
{
	if(!expression || expression->children.size() < 2 || !result) return false;
	const string operation = RemoveMarker(expression->value);
	string left;
	string right;
	const bool have_operands = InferArgument(expression->children[0], &left,
		substitutions, context) && InferArgument(expression->children[1], &right,
		substitutions, context);
	if (have_operands && operation == ",") {
		// The built-in comma operator yields the right operand.  Treating it as
		// the left operand hides overload viability in expressions such as
		// `(probe(), 0)`, causing a class-specific overload to preempt the
		// function-template fallback selected by the actual call.
		*result = right;
		return true;
	}
	if(have_operands && InferOperatorResult(operation, left, right, context, result)) return true;
	if(have_operands && InferTemplateOperatorResult(operation, expression->children[0],
		expression->children[1], substitutions, context, result)) return true;
	if(have_operands && (operation == "&&" || operation == "||" || operation == "==" ||
		operation == "!=" || operation == "<" || operation == ">" ||
		operation == "<=" || operation == ">=") && IsBuiltinLogicalType(left) &&
		IsBuiltinLogicalType(right)) {
		*result = "bool";
		return true;
	}
	if(have_operands && (operation == "+" || operation == "-" ||
		operation == "*" || operation == "/" || operation == "%") &&
		IsBuiltinArithmeticType(left) && IsBuiltinArithmeticType(right)) {
		*result = CommonBuiltinArithmeticType(left, right);
		return true;
	}
	string fallback;
	if(!InferArgument(expression->children[0], &fallback, substitutions, context) ||
		!IsKnownTypeSpelling(fallback, context)) return false;
	*result = fallback;
	return true;
}

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
	// A member operator template is stored in the template index separately
	// from the materialized class declaration.  Recover the enclosing class
	// arguments from the typed left operand, then use the ordinary function
	// deduction path for the operator's explicit argument.  This keeps a
	// binary operator's result type typed for later member-template deduction;
	// falling back to the left operand would make `key | default` look like a
	// keyword rather than a `default_<key, default_type>` object.
	string left_base = left;
	vector<string> left_arguments;
	const size_t left_open = left_base.find('<');
	if(left_open != string::npos) {
		string argument_text;
		size_t left_close = string::npos;
		if(TemplateRange(left_base, left_open, &argument_text, &left_close)) {
			left_arguments = SplitTemplateArguments(argument_text);
			left_base = CanonicalSpelling(left_base.substr(0, left_open));
		}
	}
	const TemplateDefinition* left_definition = FindDefinition(left_base, context);
	map<string, string> enclosing_substitutions;
	if(left_definition && left_definition->class_template)
		for(size_t parameter = 0; parameter < left_definition->parameters.size() &&
			parameter < left_arguments.size(); ++parameter)
			if(!left_definition->parameters[parameter].name.empty())
				enclosing_substitutions[left_definition->parameters[parameter].name] =
					left_arguments[parameter];
	if(left_definition && left_definition->class_template) {
		const string operator_name = "operator" + operation;
		for(map<string, TemplateDefinition>::const_iterator candidate = definitions_.begin();
			candidate != definitions_.end(); ++candidate) {
			const TemplateDefinition& definition = candidate->second;
			if(!definition.member_template || definition.name.empty() ||
				LastComponent(definition.name) != operator_name ||
				LastComponent(definition.owner) != LastComponent(left_base) ||
				!definition.declaration || definition.declaration->children.empty()) continue;
			const CPPGMAstNodePtr parameters = DescendantOfKind(
				FunctionDeclarator(definition.declaration), "parameter-clause");
			if(!parameters || parameters->children.size() != 1 ||
				!parameters->children[0] || parameters->children[0]->kind !=
				"parameter-declaration") continue;
			set<string> parameter_names;
			for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter)
				if(!definition.parameters[parameter].name.empty())
					parameter_names.insert(definition.parameters[parameter].name);
			map<string, string> inferred;
			string pattern = ParameterTypeSpelling(parameters->children[0]);
			pattern = NormalizeTypeArgument(ReplaceIdentifiers(pattern,
				enclosing_substitutions));
			if(!MatchTypePattern(pattern, right, parameter_names, &inferred, context)) {
				// The argument type used for a reference parameter is the referred
				// object type during ordinary call deduction.
				if(pattern.empty() || pattern[pattern.size() - 1] != '&') continue;
				pattern = NormalizeTypeArgument(pattern.substr(0, pattern.size() - 1));
				if(!MatchTypePattern(pattern, right, parameter_names, &inferred,
					context)) continue;
			}
			string return_type = NodeTypeSpelling(definition.declaration->children[0]);
			return_type += DeclaratorSuffix(FunctionDeclarator(definition.declaration));
		for(map<string, string>::const_iterator binding = inferred.begin();
			binding != inferred.end(); ++binding)
				enclosing_substitutions[binding->first] = binding->second;
			return_type = NormalizeTypeArgument(ResolveAlias(ReplaceIdentifiers(
				return_type, enclosing_substitutions), context));
			if(!return_type.empty()) {
				*result = return_type;
				return true;
			}
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
		const bool inferred_ok = InferFunctionArguments(*candidates[i], call, &inferred,
			substitutions, context);
		if(!inferred_ok) continue;
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
