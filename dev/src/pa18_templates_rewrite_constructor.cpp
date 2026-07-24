#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

namespace {

bool SameTemplateOwner(const string& definition_owner, const string& source_owner)
{
	if(source_owner.empty()) return true;
	if(definition_owner == source_owner) return true;
	if(PrefixComponent(definition_owner) == source_owner) return true;
	return LastComponent(definition_owner) == LastComponent(source_owner) &&
		PrefixComponent(definition_owner) == PrefixComponent(source_owner);
}

} // namespace

void PA18TemplateExpander::MaterializeInitializerConstructor(
	const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& result,
	const string& context, const map<string, string>& substitutions)
{
	if(!input || !result || input->kind != "simple-declaration") return;
	const CPPGMAstNodePtr original_list = ChildOfKindLocal(input,
		"init-declarator-list");
	const CPPGMAstNodePtr transformed_list = ChildOfKindLocal(result,
		"init-declarator-list");
	if(!original_list || !transformed_list || original_list->children.size() != 1 ||
		transformed_list->children.size() != 1) return;
	const CPPGMAstNodePtr original_item = original_list->children[0];
	const CPPGMAstNodePtr transformed_item = transformed_list->children[0];
	if(!original_item || !transformed_item || original_item->children.size() < 2 ||
		transformed_item->children.size() < 2) return;
	const CPPGMAstNodePtr original_initializer = original_item->children[1];
	const CPPGMAstNodePtr transformed_initializer = transformed_item->children[1];
	if(!original_initializer || !transformed_initializer) return;
	vector<CPPGMAstNodePtr> arguments;
	CPPGMAstNodePtr initializer_expression = transformed_initializer;
	if(initializer_expression->kind == "initializer" &&
		initializer_expression->children.size() == 1)
		initializer_expression = initializer_expression->children[0];
	if(initializer_expression->kind == "paren-initializer" ||
		initializer_expression->kind == "braced-init-list")
		arguments = initializer_expression->children;
	else if(initializer_expression->kind != "initializer")
		arguments.push_back(initializer_expression);
	if(arguments.empty() && original_initializer->kind != "paren-initializer" &&
		original_initializer->kind != "braced-init-list") return;
	if(input->children.empty()) return;
	const CPPGMAstNodePtr declarator = original_item->children[0];
	string target = DeclaratorTypeSpelling(NodeTypeSpelling(input->children[0]),
		declarator);
	target = CanonicalSpelling(ResolveAlias(RewriteText(target, context,
		substitutions, 0), context));
	if(target.empty() || !FindClassDeclaration(target, context)) return;
	const string constructor_name = LastComponent(target);
	string source_constructor_owner;
	string source_constructor_name = constructor_name;
	map<string, string>::const_iterator generated_base =
		specialization_bases_.find(constructor_name);
	if(generated_base != specialization_bases_.end()) {
		source_constructor_owner = generated_base->second;
		source_constructor_name = LastComponent(source_constructor_owner);
	}
	map<string, vector<string> >::const_iterator indexed_constructors =
		definitions_by_name_.find(source_constructor_name);
	bool has_member_template_constructor = false;
	if(indexed_constructors != definitions_by_name_.end())
		for(size_t candidate = 0; candidate < indexed_constructors->second.size(); ++candidate) {
			map<string, TemplateDefinition>::const_iterator found = definitions_.find(
				indexed_constructors->second[candidate]);
			if(found == definitions_.end()) continue;
			const TemplateDefinition& definition = found->second;
			if(!definition.class_template && !definition.alias_template &&
				definition.member_template && LastComponent(definition.name) ==
					source_constructor_name && SameTemplateOwner(definition.owner,
						source_constructor_owner)) {
				has_member_template_constructor = true;
				break;
			}
		}
	if(!has_member_template_constructor) return;
	CPPGMAstNodePtr object(new CPPGMAstNode("id-expression"));
	object->inferred_type = target;
	CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "."));
	member->children.push_back(object);
	member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
		"identifier", source_constructor_name)));
	CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
	call->children.push_back(member);
	CPPGMAstNodePtr argument_list(new CPPGMAstNode("argument-list"));
	argument_list->children = arguments;
	call->children.push_back(argument_list);
	InstantiateMemberCall(call, member, source_constructor_name, context,
		substitutions);
}

} // namespace pa18_templates_internal
