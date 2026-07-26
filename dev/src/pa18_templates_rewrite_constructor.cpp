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
	if(!input || !result) return;
	if(input->kind == "special-member-definition") {
		// A constructor mem-initializer is the same semantic event as a direct
		// initializer, but the parser keeps it under `ctor-initializer` instead
		// of a simple-declaration.  Materialize member-template constructors here
		// so the lowering pass can emit the selected forwarding constructor body.
		const CPPGMAstNodePtr original_ctor = ChildOfKindLocal(input,
			"ctor-initializer");
		const CPPGMAstNodePtr transformed_ctor = ChildOfKindLocal(result,
			"ctor-initializer");
		if(!original_ctor || !transformed_ctor) return;
		for(size_t initializer = 0; initializer < original_ctor->children.size();
			++initializer) {
			const CPPGMAstNodePtr original_member = original_ctor->children[initializer];
			const CPPGMAstNodePtr transformed_member = initializer <
				transformed_ctor->children.size() ? transformed_ctor->children[initializer] :
				CPPGMAstNodePtr();
			if(!original_member || !transformed_member ||
				original_member->kind != "mem-initializer" ||
				transformed_member->kind != "mem-initializer") continue;
			const CPPGMAstNodePtr member_id = ChildOfKindLocal(original_member,
				"mem-initializer-id");
			if(!member_id || member_id->value.empty()) continue;
			const CPPGMAstNodePtr transformed_arguments = ChildOfKindLocal(
				transformed_member, "paren-argument-list");
			if(!transformed_arguments) continue;
			string member_type;
			set<string> active;
			if(!FindClassMemberType(context, LastComponent(member_id->value),
				substitutions, context, &member_type, &active)) continue;
			member_type = CanonicalSpelling(ResolveAlias(RewriteText(member_type,
				context, substitutions, 0), context));
			if(member_type.empty() || !FindClassDeclaration(member_type, context)) continue;
			CPPGMAstNodePtr object(new CPPGMAstNode("id-expression"));
			object->inferred_type = member_type;
			CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "."));
			member->children.push_back(object);
			member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
				"identifier", LastComponent(member_type))));
			CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
			call->children.push_back(member);
			CPPGMAstNodePtr arguments(new CPPGMAstNode("argument-list"));
			arguments->children = transformed_arguments->children;
			call->children.push_back(arguments);
			InstantiateMemberCall(call, member, LastComponent(member_type), context,
				substitutions);
		}
		return;
	}
	if(input->kind != "simple-declaration") return;
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
	// An inherited constructor is selected through the derived class's
	// initializer, but its member-template declaration belongs to the concrete
	// base named by the derived class.  The specialization map for the derived
	// owner itself points to `key`, not to the inherited `tuple_like` owner;
	// follow the typed base edge before giving up on constructor replay.
	if(!has_member_template_constructor) {
		const CPPGMAstNodePtr declaration = FindClassDeclaration(target, context);
		if(declaration) for(size_t child = 0; child < declaration->children.size() &&
			!has_member_template_constructor; ++child) {
			const CPPGMAstNodePtr clause = declaration->children[child];
			if(!clause || clause->kind != "base-clause") continue;
			for(size_t base = 0; base < clause->children.size() &&
				!has_member_template_constructor; ++base) {
				const CPPGMAstNodePtr base_name = ChildOfKindLocal(
					clause->children[base], "base-name");
				if(!base_name) continue;
				const string concrete_base = CanonicalSpelling(ResolveAlias(
					RewriteText(base_name->value, context, substitutions, 0), context));
				map<string, string>::const_iterator base_primary =
					specialization_bases_.find(LastComponent(concrete_base));
				if(base_primary != specialization_bases_.end()) {
					source_constructor_owner = base_primary->second;
					source_constructor_name = LastComponent(source_constructor_owner);
				} else {
					source_constructor_owner = concrete_base;
					source_constructor_name = LastComponent(concrete_base);
				}
				indexed_constructors = definitions_by_name_.find(source_constructor_name);
				if(indexed_constructors == definitions_by_name_.end()) continue;
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
			}
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
