#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

namespace {

bool SameTemplateOwner(const string& definition_owner, const string& source_owner)
{
	if(source_owner.empty()) return true;
	auto strip_template_arguments = [](string value) {
		int depth = 0;
		for(size_t i = 0; i < value.size(); ++i) {
			if(value[i] == '<') {
				if(depth == 0) value.erase(i);
				else ++depth;
				if(depth == 0) break;
			} else if(value[i] == '>' && depth > 0) --depth;
		}
		return value;
	};
	const string definition_base = strip_template_arguments(definition_owner);
	const string source_base = strip_template_arguments(source_owner);
	if(definition_base == source_base) return true;
	if(PrefixComponent(definition_base) == source_base) return true;
	return LastComponent(definition_base) == LastComponent(source_base) &&
		PrefixComponent(definition_base) == PrefixComponent(source_base);
}

} // namespace

void PA18TemplateExpander::MaterializeInitializerConstructor(
	const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& result,
	const string& context, const map<string, string>& substitutions)
{
	if(!input || !result) return;
	CPPGMAstNodePtr constructor_input = input;
	CPPGMAstNodePtr constructor_result = result;
	while(constructor_input && constructor_input->kind == "template-declaration" &&
		constructor_input->children.size() > 1 && constructor_result &&
		constructor_result->kind == "template-declaration" &&
		constructor_result->children.size() > 1) {
		constructor_input = constructor_input->children[1];
		constructor_result = constructor_result->children[1];
	}
	if(constructor_input->kind == "special-member-definition") {
		// A constructor mem-initializer is the same semantic event as a direct
		// initializer, but the parser keeps it under `ctor-initializer` instead
		// of a simple-declaration.  Materialize member-template constructors here
		// so the lowering pass can emit the selected forwarding constructor body.
		const CPPGMAstNodePtr original_ctor = ChildOfKindLocal(constructor_input,
			"ctor-initializer");
		const CPPGMAstNodePtr transformed_ctor = ChildOfKindLocal(constructor_result,
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
			string constructor_member_name = LastComponent(member_id->value);
		const string current_constructor = CanonicalSpelling(RemoveMarker(
			result->value));
		const size_t current_separator = current_constructor.rfind("::");
		string current_owner = current_separator == string::npos ?
			current_constructor : current_constructor.substr(0, current_separator);
		if(current_separator == string::npos) {
			map<string, string>::const_iterator generated = specialization_bases_.find(
				LastComponent(current_constructor));
			if(generated != specialization_bases_.end()) current_owner = LastComponent(
				current_constructor);
			else if(class_contexts_.find(context) != class_contexts_.end()) current_owner = context;
		}
		map<string, string>::const_iterator current_base = specialization_bases_.find(
			LastComponent(current_owner));
		const string current_source_owner = current_base == specialization_bases_.end() ?
			LastComponent(current_owner) : LastComponent(current_base->second);
		const bool delegating = !current_owner.empty() &&
			(current_source_owner == constructor_member_name ||
			 LastComponent(current_owner) == constructor_member_name);
			if(delegating) {
				member_type = current_owner;
				map<string, string>::const_iterator base = specialization_bases_.find(
					LastComponent(member_type));
				constructor_member_name = base == specialization_bases_.end() ?
					LastComponent(member_type) : LastComponent(base->second);
			} else if(!FindClassMemberType(context, LastComponent(member_id->value),
					substitutions, context, &member_type, &active)) continue;
			member_type = CanonicalSpelling(ResolveAlias(RewriteText(member_type,
				context, substitutions, 0), context));
			if(member_type.empty() || (!delegating && !FindClassDeclaration(member_type, context))) continue;
			if(!delegating) {
				// The mem-initializer id names a field, not its constructor.  Use the
				// resolved field type and its source specialization as the typed lookup
				// owner instead of rediscovering that fact from generated spelling.
				map<string, string>::const_iterator base = specialization_bases_.find(
					LastComponent(member_type));
				constructor_member_name = base == specialization_bases_.end() ?
					LastComponent(member_type) : LastComponent(base->second);
			}
			CPPGMAstNodePtr object(new CPPGMAstNode("id-expression"));
			object->inferred_type = member_type;
			CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "."));
			member->children.push_back(object);
			member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
				"identifier", constructor_member_name)));
			CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
			call->children.push_back(member);
			CPPGMAstNodePtr arguments(new CPPGMAstNode("argument-list"));
			arguments->children = transformed_arguments->children;
			call->children.push_back(arguments);
			InstantiateMemberCall(call, member, constructor_member_name, context,
				substitutions, false, delegating);
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
	if(!original_item || !transformed_item || original_item->children.empty() ||
		transformed_item->children.empty()) return;
	const CPPGMAstNodePtr original_initializer = original_item->children.size() > 1 ?
		original_item->children[1] : CPPGMAstNodePtr();
	const CPPGMAstNodePtr transformed_initializer = transformed_item->children.size() > 1 ?
		transformed_item->children[1] : CPPGMAstNodePtr();
	vector<CPPGMAstNodePtr> arguments;
	if(transformed_initializer) {
		CPPGMAstNodePtr initializer_expression = transformed_initializer;
		if(initializer_expression->kind == "initializer" &&
			initializer_expression->children.size() == 1)
			initializer_expression = initializer_expression->children[0];
		if(initializer_expression->kind == "paren-initializer" ||
			initializer_expression->kind == "braced-init-list")
			arguments = initializer_expression->children;
		else if(initializer_expression->kind != "initializer")
			arguments.push_back(initializer_expression);
		if(arguments.empty() && original_initializer &&
			original_initializer->kind != "paren-initializer" &&
			original_initializer->kind != "braced-init-list") return;
	}
	if(original_initializer && !transformed_initializer) return;
	if(input->children.empty()) return;
	const CPPGMAstNodePtr declarator = original_item->children[0];
	string target = DeclaratorTypeSpelling(NodeTypeSpelling(input->children[0]),
		declarator);
	target = CanonicalSpelling(ResolveAlias(RewriteText(target, context,
		substitutions, 0), context));
	if(target.empty() || !FindClassDeclaration(target, context)) return;
	// A direct initializer can select a non-template constructor of the target
	// through a user-defined conversion.  Materialize the conversion object's
	// constructor before PA11 builds constructor bindings; otherwise a template
	// such as String(T) is invisible because no source expression names String
	// directly.
	const CPPGMAstNodePtr target_declaration = FindClassDeclaration(target, context);
	const auto materialize_conversion_constructor = [&](const string& raw_parameter,
		const CPPGMAstNodePtr& argument) {
		if(!argument) return;
		string parameter_type = CanonicalSpelling(ResolveAlias(RewriteText(
			raw_parameter, context, substitutions, 0), context));
		while(parameter_type.compare(0, 6, "const ") == 0 ||
			parameter_type.compare(0, 9, "volatile ") == 0) {
			const size_t space = parameter_type.find(' ');
			if(space == string::npos) break;
			parameter_type = CanonicalSpelling(parameter_type.substr(space + 1));
		}
		while(!parameter_type.empty() && (parameter_type[parameter_type.size() - 1] == '&' ||
			parameter_type[parameter_type.size() - 1] == '*'))
			parameter_type = CanonicalSpelling(parameter_type.substr(0, parameter_type.size() - 1));
		if(parameter_type.empty() || !FindClassDeclaration(parameter_type, context)) return;
		string source_owner = parameter_type;
		string source_name = LastComponent(parameter_type);
		map<string, string>::const_iterator base = specialization_bases_.find(source_name);
		if(base != specialization_bases_.end()) {
			source_owner = base->second;
			source_name = LastComponent(source_owner);
		}
		map<string, vector<string> >::const_iterator indexed = definitions_by_name_.find(source_name);
		if(indexed == definitions_by_name_.end()) return;
		for(size_t candidate = 0; candidate < indexed->second.size(); ++candidate) {
			map<string, TemplateDefinition>::const_iterator found = definitions_.find(
				indexed->second[candidate]);
			if(found == definitions_.end()) continue;
			const TemplateDefinition& definition = found->second;
			if(definition.class_template || definition.alias_template || !definition.member_template ||
				LastComponent(definition.name) != source_name ||
				!SameTemplateOwner(definition.owner, source_owner)) continue;
			CPPGMAstNodePtr object(new CPPGMAstNode("id-expression"));
			object->inferred_type = parameter_type;
			CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "."));
			member->children.push_back(object);
			member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
				"identifier", source_name)));
			CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
			call->children.push_back(member);
			CPPGMAstNodePtr call_arguments(new CPPGMAstNode("argument-list"));
			call_arguments->children.push_back(argument);
			call->children.push_back(call_arguments);
			try {
				if(InstantiateMemberCall(call, member, source_name, context, substitutions))
					return;
			} catch(const PA18SubstitutionFailure&) {
				// This candidate is unavailable after substitution; another
				// constructor template may still be viable.
			}
		}
	};
	if(target_declaration) for(size_t child = 0; child < target_declaration->children.size(); ++child) {
		const CPPGMAstNodePtr constructor = target_declaration->children[child];
		if(!constructor || (constructor->kind != "special-member-definition" &&
			constructor->kind != "special-member-declaration")) continue;
		const CPPGMAstNodePtr clause = DescendantOfKind(FunctionDeclarator(constructor),
			"parameter-clause");
		if(!clause) continue;
		for(size_t parameter = 0; parameter < clause->children.size() &&
			parameter < arguments.size(); ++parameter) {
			const CPPGMAstNodePtr item = clause->children[parameter];
			if(item && item->kind == "parameter-declaration")
				materialize_conversion_constructor(ParameterTypeSpelling(item), arguments[parameter]);
		}
	}
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
	InstantiateMemberCall(call, member, source_constructor_name, context, substitutions);
}

} // namespace pa18_templates_internal
