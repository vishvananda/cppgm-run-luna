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
	CPPGMAstNodePtr operand = TransformNode(input->children[0], context, substitutions);
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
	if(!instantiated) return CPPGMAstNodePtr();
	return call;
}

} // namespace pa18_templates_internal
