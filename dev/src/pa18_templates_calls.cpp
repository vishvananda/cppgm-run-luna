#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;
namespace pa18_templates_internal {

bool PA18TemplateExpander::InstantiateMemberCall(
	const CPPGMAstNodePtr& call, const CPPGMAstNodePtr& callee,
	const string& original_member, const string& context,
	const map<string, string>& substitutions,
	bool explicit_instantiation, bool constructor_replay)
{
	return ReplayMemberCall(call, callee, original_member, context, substitutions,
		explicit_instantiation, constructor_replay);
}

void PA18TemplateExpander::CollectInheritedMemberTemplates(const string& raw_class,
	const string& member, const map<string, string>& substitutions,
	const string& context, vector<const TemplateDefinition*>* result,
	set<string>* active, map<const TemplateDefinition*, string>* concrete_owners)
{
	if(raw_class.empty() || member.empty() || !result || !active) return;
	string class_key = CanonicalSpelling(ReplaceIdentifiers(raw_class, substitutions));
	class_key = CanonicalSpelling(ResolveAlias(class_key, context));
	while(class_key.compare(0, 6, "const ") == 0 ||
		class_key.compare(0, 9, "volatile ") == 0)
		class_key = CanonicalSpelling(class_key.substr(class_key.find(' ') + 1));
	while(!class_key.empty() && (class_key[class_key.size() - 1] == '&' ||
		class_key[class_key.size() - 1] == '*'))
		class_key = CanonicalSpelling(class_key.substr(0, class_key.size() - 1));
	if(class_key.empty() || !active->insert(class_key + "|" + member).second) return;

	map<string, string> class_substitutions = substitutions;
	map<string, string>::const_iterator generated_base = specialization_bases_.find(
		LastComponent(class_key));
	map<string, vector<string> >::const_iterator generated_arguments =
		specialization_arguments_.find(LastComponent(class_key));
	if(generated_base != specialization_bases_.end() &&
		generated_arguments != specialization_arguments_.end()) {
		const TemplateDefinition* source = FindDefinition(generated_base->second, context);
		if(source && source->class_template) {
			for(size_t parameter = 0; parameter < source->parameters.size() &&
				parameter < generated_arguments->second.size(); ++parameter)
				if(!source->parameters[parameter].name.empty())
					class_substitutions[source->parameters[parameter].name] =
						generated_arguments->second[parameter];
			if(!source->name.empty()) class_substitutions[source->name] = class_key;
		}
	}
	CPPGMAstNodePtr declaration = FindClassDeclaration(class_key, context);
	if(!declaration) {
		active->erase(class_key + "|" + member);
		return;
	}
	const string declaration_context = PrefixComponent(class_key).empty() ?
		context : PrefixComponent(class_key);
	CollectInheritedMemberBases(declaration, member, declaration_context,
		class_substitutions, result, active, concrete_owners);
	active->erase(class_key + "|" + member);
}

CPPGMAstNodePtr PA18TemplateExpander::TransformCallExpression(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions)
{
	if(!input) return CPPGMAstNodePtr();
	CPPGMAstNodePtr result(new CPPGMAstNode(input->kind, input->value));
	result->initializer_form = input->initializer_form;
	result->template_instantiation = input->template_instantiation;
	result->explicit_specialization = input->explicit_specialization;
	result->explicit_instantiation = input->explicit_instantiation;
	result->extern_instantiation = input->extern_instantiation;
	result->dependent_base_lookup = input->dependent_base_lookup;
	result->materialize_object_address = input->materialize_object_address;
	result->materialize_object_name = input->materialize_object_name;
	result->inferred_type = input->inferred_type;
	result->indirect_function_call = input->indirect_function_call;
	result->source_token_begin = input->source_token_begin;
	result->source_token_end = input->source_token_end;
	result->template_primary = input->template_primary;
	result->template_arguments = input->template_arguments;
	CPPGMAstNodePtr input_callee = input->children.empty() ? CPPGMAstNodePtr() :
		input->children[0];
	if(input_callee && input_callee->kind == "parenthesized-expression" &&
		input_callee->children.size() == 1 && input_callee->children[0] &&
		input_callee->children[0]->kind == "id-expression")
		input_callee = input_callee->children[0];
	if(input_callee && input_callee->kind == "id-expression" &&
		TransformQualifiedMemberTemplateCall(input, input_callee, context,
			substitutions, result))
		return result;
	if(TransformUnqualifiedMemberTemplateCall(input, input_callee, context,
		substitutions, result))
		return result;
	const bool explicit_call = TransformExplicitFunctionCall(input, input_callee, context,
		substitutions, result);
	if(explicit_call) return result;
	if(input_callee && input_callee->kind == "id-expression" &&
		active_function_pointer_substitutions_.find(
			RemoveMarker(input_callee->value)) !=
			active_function_pointer_substitutions_.end())
		result->indirect_function_call = true;
	TransformCallChildren(input, result, context, substitutions);
	CPPGMAstNodePtr result_callee = result->children.empty() ? CPPGMAstNodePtr() :
		result->children[0];
	if(result_callee && result_callee->kind == "parenthesized-expression" &&
		result_callee->children.size() == 1 && result_callee->children[0] &&
		result_callee->children[0]->kind == "id-expression") {
		result_callee = result_callee->children[0];
		result->children[0] = result_callee;
	}
	result_callee = MaterializeStaticCastCall(result, result_callee, context, substitutions);
	bool constructor_replayed = false;
	if(MaterializeNamedCallTarget(result, &result_callee, context, substitutions,
		&constructor_replayed)) {
		return result;
	}
	result_callee = MaterializeOperatorCallTargets(result, input_callee, result_callee,
		context, substitutions);
	const bool implicit_member_instantiated = MaterializeImplicitMemberCall(result,
		result_callee, input_callee, context, substitutions);
	MaterializeFreeFunctionCall(result, result_callee, constructor_replayed,
		implicit_member_instantiated, context, substitutions);
	FinalizeCallResult(result, result_callee, context, substitutions);
	return result;
}

} // namespace pa18_templates_internal
