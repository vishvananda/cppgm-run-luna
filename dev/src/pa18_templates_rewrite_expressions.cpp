#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

CPPGMAstNodePtr PA18TemplateExpander::TransformUnaryExpression(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions)
{
	if(!input || input->children.empty()) return CPPGMAstNodePtr();
	const string operation = RemoveMarker(input->value);
	if(operation.empty()) return CPPGMAstNodePtr();
	// A non-type pointer parameter is represented in the typed substitution map
	// by its address constant (for example `&ns::flag`).  It is an ordinary
	// built-in dereference in the instantiated body, not a call to a user
	// supplied `operator*`.  Build the small expression tree explicitly so the
	// LowIR semantic pass sees an address operand rather than an identifier whose
	// spelling happens to contain `&`.
	if(operation == "*" && input->children[0] &&
		input->children[0]->kind == "id-expression") {
		map<string, string>::const_iterator pointer = substitutions.find(
			RemoveMarker(input->children[0]->value));
		if(pointer != substitutions.end() && pointer->second.size() > 1 &&
			pointer->second[0] == '&' && pointer->second[1] != '&') {
			CPPGMAstNodePtr address(new CPPGMAstNode("unary-expression", "OP_AMP:&"));
			address->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
				"id-expression", pointer->second.substr(1))));
			CPPGMAstNodePtr result(new CPPGMAstNode("unary-expression", input->value));
			result->children.push_back(address);
			return result;
		}
	}
	const bool preserve_qualified_template_address = operation == "&" &&
		input->children[0] && input->children[0]->kind == "id-expression" &&
		input->children[0]->value.find("::") != string::npos &&
		input->children[0]->value.find('<') != string::npos;
	CPPGMAstNodePtr operand = preserve_qualified_template_address ?
		CloneNode(input->children[0]) : TransformNode(input->children[0], context, substitutions);
	if(!operand) return CPPGMAstNodePtr();
	// Materialize qualified member-template addresses through owner-aware lookup.
	if(operation == "&" && operand->kind == "id-expression") {
		const string raw = RemoveMarker(operand->value);
		const size_t separator = raw.rfind("::");
		if(separator != string::npos) {
			size_t member_begin = separator + 2;
			while(member_begin < raw.size() && isspace(
				static_cast<unsigned char>(raw[member_begin]))) ++member_begin;
			if(raw.compare(member_begin, 8, "template") == 0) {
				member_begin += 8;
				while(member_begin < raw.size() && isspace(
					static_cast<unsigned char>(raw[member_begin]))) ++member_begin;
			}
			const size_t member_open = raw.find('<', member_begin);
			if(member_open != string::npos) {
				string owner = CanonicalSpelling(RewriteText(raw.substr(0, separator),
					context, substitutions, 0));
				owner = CanonicalSpelling(ResolveAlias(owner, context));
				const string member_spelling = raw.substr(member_begin);
				CPPGMAstNodePtr object(new CPPGMAstNode("id-expression"));
				object->value = owner;
				object->inferred_type = owner;
				CPPGMAstNodePtr synthetic_member(new CPPGMAstNode("member-expression", "."));
				synthetic_member->children.push_back(object);
				synthetic_member->children.push_back(CPPGMAstNodePtr(
					new CPPGMAstNode("identifier", member_spelling)));
				CPPGMAstNodePtr synthetic_call(new CPPGMAstNode("call-expression"));
				synthetic_call->children.push_back(synthetic_member);
				synthetic_call->children.push_back(CPPGMAstNodePtr(
					new CPPGMAstNode("argument-list")));
				if(InstantiateMemberCall(synthetic_call, synthetic_member, member_spelling,
					context, substitutions)) {
					CPPGMAstNodePtr address(new CPPGMAstNode(input->kind, input->value));
					address->initializer_form = input->initializer_form;
					address->template_instantiation = input->template_instantiation;
					address->explicit_specialization = input->explicit_specialization;
					address->explicit_instantiation = input->explicit_instantiation;
					address->extern_instantiation = input->extern_instantiation;
					address->dependent_base_lookup = input->dependent_base_lookup;
					address->materialize_object_address = input->materialize_object_address;
					address->materialize_object_name = input->materialize_object_name;
					address->source_token_begin = input->source_token_begin;
					address->source_token_end = input->source_token_end;
					operand->value = owner + "::" + LastComponent(
						member_spelling.substr(0, member_open - member_begin));
					operand->template_instantiation = true;
					operand->template_primary = synthetic_call->template_primary;
					operand->template_arguments = synthetic_call->template_arguments;
					address->children.push_back(operand);
					return address;
				}
			}
		}
	}
	CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "."));
	member->children.push_back(operand);
	member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier",
		"operator" + operation)));
	CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
	call->children.push_back(member);
	call->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("argument-list")));
	const bool instantiated = InstantiateMemberCall(call, member, "operator" + operation,
		context, substitutions);
	if(!instantiated) {
		// Unary operators are commonly declared as free function templates (for
		// example iterator::operator* in a CRTP base).  Materialize that
		// overload as well; PA14 can then perform the ordinary operator lookup on
		// the preserved unary-expression node.
		const vector<const TemplateDefinition*> candidates = FindFunctionDefinitions(
			"operator" + operation, context);
		for(size_t candidate = 0; candidate < candidates.size(); ++candidate) {
			CPPGMAstNodePtr free_call(new CPPGMAstNode("call-expression"));
			free_call->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
				"id-expression", "operator" + operation)));
			CPPGMAstNodePtr arguments(new CPPGMAstNode("argument-list"));
			arguments->children.push_back(operand);
			free_call->children.push_back(arguments);
			vector<string> inferred;
			map<string, vector<string> > inferred_pack_values;
			map<string, FunctionSignature> inferred_function_values;
			if(!InferFunctionArguments(*candidates[candidate], free_call, &inferred,
				substitutions, context, 0, &inferred_pack_values,
				&inferred_function_values)) {
				continue;
			}
			Instantiate(*candidates[candidate], inferred, context, false,
				&inferred_pack_values, 0, 0, &inferred_function_values);
			CPPGMAstNodePtr result(new CPPGMAstNode(input->kind, input->value));
			result->initializer_form = input->initializer_form;
			result->template_instantiation = input->template_instantiation;
			result->explicit_specialization = input->explicit_specialization;
			result->explicit_instantiation = input->explicit_instantiation;
			result->extern_instantiation = input->extern_instantiation;
			result->dependent_base_lookup = input->dependent_base_lookup;
			result->materialize_object_address = input->materialize_object_address;
			result->materialize_object_name = input->materialize_object_name;
			result->source_token_begin = input->source_token_begin;
			result->source_token_end = input->source_token_end;
			result->children.push_back(operand);
			return result;
		}
		return CPPGMAstNodePtr();
	}
	return call;
}

} // namespace pa18_templates_internal
