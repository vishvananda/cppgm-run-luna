#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
#include <functional>
using namespace std;
namespace pa18_templates_internal {
void PA18TemplateExpander::ResolveFunctionArguments(const CPPGMAstNodePtr& result,
	const FunctionSignature* signature, const string& context,
	const map<string, string>* substitutions)
{
	if(!signature || !signature->parameters || result->children.size() < 2 ||
		!result->children[1] || result->children[1]->kind != "argument-list") return;
	const CPPGMAstNodePtr result_arguments = result->children[1];
	size_t argument = 0;
	for(size_t parameter = 0; parameter < signature->parameters->children.size() &&
		argument < result_arguments->children.size(); ++parameter) {
		const CPPGMAstNodePtr parameter_node = signature->parameters->children[parameter];
		if(!parameter_node || parameter_node->kind != "parameter-declaration") continue;
		string expected = FunctionTypeSpelling(parameter_node);
		if(substitutions && !expected.empty()) try {
			expected = RewriteText(expected, context, *substitutions, 0);
		} catch(const logic_error&) {
			++argument;
			continue;
		}
		expected = CanonicalSpelling(ResolveAlias(expected, context));
		if(expected.find("(*") != string::npos && !expected.empty() &&
			expected[expected.size() - 1] == '*')
			expected = CanonicalSpelling(expected.substr(0, expected.size() - 1));
		CPPGMAstNodePtr argument_node = result_arguments->children[argument];
		if(argument_node && argument_node->kind == "unary-expression" &&
			RemoveMarker(argument_node->value) == "&" && !argument_node->children.empty())
			argument_node = argument_node->children[0];
		if(argument_node && argument_node->kind == "id-expression") {
			const vector<const TemplateDefinition*> function_candidates =
				FindFunctionDefinitions(argument_node->value, context);
			for(size_t candidate = 0; candidate < function_candidates.size(); ++candidate) {
				vector<string> inferred;
				if(!InferFunctionFromExpected(*function_candidates[candidate], expected,
					&inferred, context)) continue;
				try {
					const string local_name = Instantiate(*function_candidates[candidate],
						inferred, context);
					// Keep an explicit address-of node intact.  The selected
					// specialization replaces the referenced declaration, not the
					// unary operator carrying it.
					argument_node->value = local_name;
					break;
				} catch(const logic_error&) {}
			}
		}
		++argument;
	}
}
CPPGMAstNodePtr PA18TemplateExpander::TransformSubscriptExpression(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions)
{
	if(!input || input->children.size() < 2) return CPPGMAstNodePtr();
	CPPGMAstNodePtr transformed(new CPPGMAstNode(input->kind, input->value));
	transformed->initializer_form = input->initializer_form;
	transformed->template_instantiation = input->template_instantiation;
	transformed->explicit_specialization = input->explicit_specialization;
	transformed->explicit_instantiation = input->explicit_instantiation;
	transformed->extern_instantiation = input->extern_instantiation;
	transformed->dependent_base_lookup = input->dependent_base_lookup;
	transformed->materialize_object_address = input->materialize_object_address;
	transformed->materialize_object_name = input->materialize_object_name;
	transformed->source_token_begin = input->source_token_begin;
	transformed->source_token_end = input->source_token_end;
	for(size_t child = 0; child < input->children.size(); ++child) {
		CPPGMAstNodePtr rewritten = TransformNode(input->children[child], context,
			substitutions);
		if(rewritten) transformed->children.push_back(rewritten);
	}
	if(transformed->children.size() < 2) return transformed;
	CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "."));
	member->children.push_back(transformed->children[0]);
	member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier",
		"operator[]")));
	CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
	call->children.push_back(member);
	CPPGMAstNodePtr arguments(new CPPGMAstNode("argument-list"));
	arguments->children.push_back(transformed->children[1]);
	call->children.push_back(arguments);
	if(InstantiateMemberCall(call, member, "operator[]", context, substitutions))
		return call;
	return transformed;
}
CPPGMAstNodePtr PA18TemplateExpander::TransformAssignmentExpression(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions)
{
	if(!input || input->children.size() < 2 ||
		RemoveMarker(input->value) != "=") return CPPGMAstNodePtr();
	CPPGMAstNodePtr left = TransformNode(input->children[0], context, substitutions);
	CPPGMAstNodePtr right = TransformNode(input->children[1], context, substitutions);
	if(!left || !right) return CPPGMAstNodePtr();
	CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "."));
	member->children.push_back(left);
	member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier",
		"operator=")));
	CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
	call->children.push_back(member);
	CPPGMAstNodePtr arguments(new CPPGMAstNode("argument-list"));
	arguments->children.push_back(right);
	call->children.push_back(arguments);
	const bool instantiated = InstantiateMemberCall(call, member, "operator=", context,
		substitutions);
	if(!instantiated)
		return CPPGMAstNodePtr();
	return call;
}
void PA18TemplateExpander::ResolveMemberFunctionArguments(
	const CPPGMAstNodePtr& result, const string& context,
	const map<string, string>& substitutions)
{
	if(!result || result->kind != "call-expression" || result->children.size() < 2 ||
	   !result->children[0] || result->children[0]->kind != "member-expression" ||
	   result->children[0]->children.size() < 2 || !result->children[0]->children[1] ||
	   !result->children[1] || result->children[1]->kind != "argument-list") return;
	const string member_name = LastComponent(result->children[0]->children[1]->value);
	if(member_name.empty()) return;
	string object_type;
	if(!InferArgument(result->children[0]->children[0], &object_type,
	                  substitutions, context)) return;
	object_type = CanonicalSpelling(ResolveAlias(RewriteText(object_type, context,
		substitutions, 0), context));
	while(object_type.compare(0, 6, "const ") == 0 ||
		object_type.compare(0, 9, "volatile ") == 0)
		object_type = CanonicalSpelling(object_type.substr(object_type.find(' ') + 1));
	while(!object_type.empty() && (object_type[object_type.size() - 1] == '*' ||
		object_type[object_type.size() - 1] == '&'))
		object_type = CanonicalSpelling(object_type.substr(0, object_type.size() - 1));
	const CPPGMAstNodePtr declaration = FindClassDeclaration(object_type, context);
	if(!declaration) return;
	for(size_t member = 0; member < declaration->children.size(); ++member) {
		const CPPGMAstNodePtr candidate = declaration->children[member];
		if(!candidate || (candidate->kind != "function-definition" &&
			candidate->kind != "simple-declaration") || candidate->children.size() < 2 ||
			LastComponent(FirstIdentifierLocal(candidate->children[1])) != member_name) continue;
		const CPPGMAstNodePtr clause = DescendantOfKind(
			candidate->children[1], "parameter-clause");
		if(!clause) continue;
		size_t argument = 0;
		for(size_t parameter = 0; parameter < clause->children.size() &&
			argument < result->children[1]->children.size(); ++parameter) {
			const CPPGMAstNodePtr parameter_node = clause->children[parameter];
			if(!parameter_node || parameter_node->kind != "parameter-declaration") continue;
			const string expected = FunctionTypeSpelling(parameter_node);
			CPPGMAstNodePtr original = result->children[1]->children[argument++];
			CPPGMAstNodePtr function_argument = original;
			if(function_argument && function_argument->kind == "unary-expression" &&
				RemoveMarker(function_argument->value) == "&" &&
				!function_argument->children.empty())
				function_argument = function_argument->children[0];
			if(expected.empty() || !function_argument ||
				function_argument->kind != "id-expression") continue;
			const vector<const TemplateDefinition*> candidates =
				FindFunctionDefinitions(function_argument->value, context);
			for(size_t candidate_index = 0; candidate_index < candidates.size();
				++candidate_index) {
				vector<string> inferred;
				if(!InferFunctionFromExpected(*candidates[candidate_index], expected,
					&inferred, context)) continue;
				const string local_name = Instantiate(*candidates[candidate_index],
					inferred, context);
				if(original && original->kind == "unary-expression" &&
					!original->children.empty())
					original->children[0]->value = local_name;
				else original->value = local_name;
				break;
			}
		}
	}
}
} // namespace
