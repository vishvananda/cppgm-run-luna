#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::MaterializeExplicitInstantiation(
	const CPPGMAstNodePtr& target, const string& context,
	bool extern_instantiation)
{
	if(!target || (target->kind != "simple-declaration" &&
		target->kind != "special-member-declaration" &&
		target->kind != "special-member-definition")) return false;
	const CPPGMAstNodePtr declarator = FunctionDeclarator(target);
	if(!declarator) return false;
	const CPPGMAstNodePtr parameters = DescendantOfKind(declarator,
		"parameter-clause");
	string raw_name = target->kind == "special-member-declaration" ||
		target->kind == "special-member-definition" ?
		CanonicalSpelling(RemoveMarker(target->value)) :
		RemoveMarker(FirstIdentifierLocal(declarator));
	if(raw_name.empty()) return false;

	string owner;
	string member_name = raw_name;
	const size_t scope_separator = raw_name.rfind("::");
	if(scope_separator != string::npos) {
		owner = raw_name.substr(0, scope_separator);
		member_name = raw_name.substr(scope_separator + 2);
	}
	vector<string> explicit_arguments;
	string function_name = member_name;
	const size_t function_open = member_name.find('<');
	if(function_open != string::npos) {
		string argument_text;
		size_t close = string::npos;
		if(member_name.compare(0, 8, "operator") == 0) { int depth = 0;
			for(size_t position = function_open; position < member_name.size(); ++position) {
				if(member_name[position] == '<') ++depth;
				else if(member_name[position] == '>' && --depth == 0) {
					argument_text = member_name.substr(function_open + 1, position - function_open - 1);
					close = position; break;
				}
			}
		}
		if(close == string::npos && !TemplateRange(member_name, function_open, &argument_text, &close)) return false;
		function_name = member_name.substr(0, function_open);
		explicit_arguments = SplitTemplateArguments(argument_text);
	}
	if(function_name.empty()) return false;
	string lookup_owner = owner;
	vector<string> owner_arguments;
	if(!lookup_owner.empty()) {
		const size_t owner_open = lookup_owner.find('<');
		if(owner_open != string::npos) {
			string argument_text;
			size_t close = string::npos;
			if(!TemplateRange(lookup_owner, owner_open, &argument_text, &close))
				return false;
			owner_arguments = SplitTemplateArguments(argument_text);
			lookup_owner.erase(owner_open);
		}
	}
	const string lookup = owner.empty() ? function_name :
		lookup_owner + "::" + function_name;
	const vector<const TemplateDefinition*> candidates = FindFunctionDefinitions(
		lookup, context);
	// A constructor of a class-template specialization is an explicit
	// instantiation of the enclosing class entity, not a function template in
	// the function-definition registry.  Validate the typed owner before
	// accepting an extern declaration; no generated definition is needed for
	// an extern declaration.
	const TemplateDefinition* owner_definition = owner_arguments.empty() ? 0 :
		FindDefinition(lookup_owner, context);
	if(owner_definition && owner_definition->class_template &&
		LastComponent(lookup_owner) == function_name) {
		if(!extern_instantiation)
			Instantiate(*owner_definition, owner_arguments, context, true);
		return true;
	}
	// Member declarations and static data members are owned by the class
	// template rather than by the free-function registry.  Keep extern
	// declarations as typed ownership facts; an explicit instantiation replays
	// the enclosing class so its already-visible member definitions can enter
	// the normal materialization path.
	if(owner_definition && owner_definition->class_template && !owner_arguments.empty()) {
		bool known_member = HasStaticMember(owner_definition, lookup_owner, function_name);
		if(!known_member && parameters) {
			set<string> active_member_lookup;
			string member_type;
			known_member = FindClassMemberType(owner_definition->qualified_name,
				function_name, map<string, string>(), context, &member_type,
				&active_member_lookup, false);
		}
		if(known_member) {
			if(!extern_instantiation)
				Instantiate(*owner_definition, owner_arguments, context, true);
			return true;
		}
	}
	if(!parameters) return false;
	// An explicit instantiation after an explicit function specialization names
	// the specialization that is already the selected entity; it must not create
	// a second primary-template body.  Re-enter the typed specialization record
	// so the explicit-instantiation root fact reaches its cached declaration.
	if(!explicit_arguments.empty()) for(size_t candidate_index = 0;
		candidate_index < candidates.size(); ++candidate_index) {
		const TemplateDefinition* candidate = candidates[candidate_index];
		if(!candidate || candidate->class_template || candidate->parameters.empty()) continue;
		const TemplateDefinition* specialization =
			FindExplicitFunctionSpecialization(candidate, explicit_arguments);
		if(!specialization) continue;
		if(!extern_instantiation)
			Instantiate(*specialization, explicit_arguments, context, true);
		return true;
	}
	// A non-member overloaded operator must have a class or enum operand.  Do
	// this check only after entity lookup: conversion operators and member
	// operators are valid even when their explicit parameters are arithmetic,
	// while the invalid case is a free operator template materialized with only
	// builtin operands.
	const string operator_suffix = function_name.compare(0, 8, "operator") == 0 ?
		function_name.substr(8) : string();
	const bool operator_requires_operand = !operator_suffix.empty() &&
		!IsIdentifierCharacter(operator_suffix[0]) && operator_suffix != "()" &&
		operator_suffix != "[]";
	const bool owner_is_class = !lookup_owner.empty() &&
		FindClassDeclaration(lookup_owner, context);
	if(!operator_suffix.empty() && operator_requires_operand && !owner_is_class) {
		bool user_defined_operand = false;
		for(size_t parameter = 0; parameter < parameters->children.size(); ++parameter) {
			const CPPGMAstNodePtr parameter_node = parameters->children[parameter];
			if(!parameter_node || parameter_node->kind != "parameter-declaration" ||
				parameter_node->children.empty()) continue;
			string operand = CanonicalSpelling(ResolveAlias(
				NodeTypeSpelling(parameter_node->children[0]), context));
			while(operand.compare(0, 6, "const ") == 0)
				operand = CanonicalSpelling(operand.substr(6));
			while(operand.compare(0, 9, "volatile ") == 0)
				operand = CanonicalSpelling(operand.substr(9));
			while(operand.size() > 6 && operand.compare(operand.size() - 6, 6,
				" const") == 0)
				operand = CanonicalSpelling(operand.substr(0, operand.size() - 6));
			while(operand.size() > 9 && operand.compare(operand.size() - 9, 9,
				" volatile") == 0)
				operand = CanonicalSpelling(operand.substr(0, operand.size() - 9));
			if(FindClassDeclaration(operand, context) ||
				named_type_contexts_.find(operand) != named_type_contexts_.end()) {
				user_defined_operand = true;
				break;
			}
		}
		if(!user_defined_operand)
			throw logic_error("explicit instantiation of builtin operator");
	}

	for(size_t candidate_index = 0; candidate_index < candidates.size();
		++candidate_index) {
		const TemplateDefinition& definition = *candidates[candidate_index];
		if(definition.class_template || definition.alias_template ||
			definition.variable_template || definition.parameters.empty() ||
			LastComponent(definition.name) != LastComponent(function_name)) continue;
		if(!owner.empty()) {
			string candidate_owner = definition.owner;
			const size_t candidate_open = candidate_owner.find('<');
			if(candidate_open != string::npos) candidate_owner.erase(candidate_open);
			if(candidate_owner != lookup_owner &&
				LastComponent(candidate_owner) != LastComponent(lookup_owner)) continue;
		}
		CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
		CPPGMAstNodePtr argument_list(new CPPGMAstNode("argument-list"));
		for(size_t parameter = 0; parameter < parameters->children.size(); ++parameter) {
			const CPPGMAstNodePtr source = parameters->children[parameter];
			if(!source || source->kind != "parameter-declaration") continue;
			CPPGMAstNodePtr argument(new CPPGMAstNode("id-expression"));
			argument->inferred_type = ParameterTypeSpelling(source);
			argument_list->children.push_back(argument);
		}
		call->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
			"id-expression", definition.name)));
		call->children.push_back(argument_list);

		if(!owner.empty()) {
			const TemplateDefinition* owner_definition = owner_arguments.empty() ? 0 :
				FindDefinition(lookup_owner, context);
			if(owner_definition && owner_definition->class_template) {
				map<string, string> owner_substitutions;
				if(!MemberOwnerPattern(definition, *owner_definition, owner_arguments,
					&owner_substitutions)) continue;
				try {
					const string owner_local = Instantiate(*owner_definition,
						owner_arguments, context, true);
					if(definition.member_template) {
						CPPGMAstNodePtr object(new CPPGMAstNode("id-expression"));
						object->inferred_type = owner_local;
						CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "."));
						member->children.push_back(object);
						member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
							"identifier", member_name)));
						call->children[0] = member;
						if(!InstantiateMemberCall(call, member, member_name, context,
							map<string, string>(), true)) continue;
					}
					return true;
				} catch(const logic_error&) {
					continue;
				}
			}
		}
		if(!owner.empty()) {
			CPPGMAstNodePtr object(new CPPGMAstNode("id-expression"));
			object->inferred_type = owner;
			CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "."));
			member->children.push_back(object);
			member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
				"identifier", member_name)));
			call->children[0] = member;
			if(InstantiateMemberCall(call, member, member_name, context,
				map<string, string>(), true)) return true;
		}

		vector<string> complete_arguments;
		map<string, FunctionSignature> inferred_function_values;
		vector<string> normalized_explicit = explicit_arguments;
		for(size_t argument = 0; argument < normalized_explicit.size(); ++argument)
			normalized_explicit[argument] = NormalizeTypeArgument(RewriteText(
				normalized_explicit[argument], context, map<string, string>(), 0));
		vector<string> source_explicit = explicit_arguments;
		for(size_t argument = 0; argument < source_explicit.size(); ++argument)
			source_explicit[argument] = NormalizeTypeArgument(ResolveAlias(
			CanonicalSpelling(source_explicit[argument]), context));
		const vector<string>* explicit_prefix = normalized_explicit.empty() ? 0 :
			&normalized_explicit;
		try {
			bool inferred = InferFunctionArguments(definition, call, &complete_arguments,
				map<string, string>(), context, explicit_prefix, 0,
				&inferred_function_values);
			// An explicit extern template-id supplies the complete typed argument
			// list even when ordinary call-shaped deduction cannot replay a
			// reference-qualified template argument.
			bool explicit_target_match = false;
			if(source_explicit.size() == definition.parameters.size() &&
				find_if(definition.parameters.begin(), definition.parameters.end(),
					[](const TemplateParameter& parameter) { return parameter.pack; }) ==
				definition.parameters.end()) {
				map<string, string> explicit_bindings;
				for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter) {
					const TemplateParameter& value = definition.parameters[parameter];
					if(value.name.empty()) continue;
					explicit_bindings[value.name] = source_explicit[parameter];
				}
				const CPPGMAstNodePtr candidate_parameters = DescendantOfKind(
					FunctionDeclarator(definition.declaration), "parameter-clause");
				if(candidate_parameters && candidate_parameters->children.size() ==
					parameters->children.size()) {
					explicit_target_match = true;
					for(size_t parameter = 0; parameter < parameters->children.size(); ++parameter) {
						const CPPGMAstNodePtr candidate = candidate_parameters->children[parameter];
						const CPPGMAstNodePtr target_parameter = parameters->children[parameter];
						const string expected = candidate && !explicit_bindings.empty() ?
							NormalizeTypeArgument(ResolveAlias(ReplaceIdentifiers(
								ParameterTypeSpelling(candidate), explicit_bindings), context)) :
							string();
						const string actual = target_parameter ? NormalizeTypeArgument(
							ResolveAlias(ParameterTypeSpelling(target_parameter), context)) : string();
						if(!candidate || !target_parameter || expected.empty() || expected != actual) {
							explicit_target_match = false;
							break;
						}
					}
				}
			}
			if(!inferred && explicit_target_match) {
				complete_arguments = source_explicit;
				inferred = true;
			}
			if(!inferred) continue;
			if(extern_instantiation) {
				ostringstream request_key;
				request_key << definition.qualified_name << "@" << definition.declaration.get();
				for(size_t argument = 0; argument < complete_arguments.size(); ++argument)
					request_key << "|" << CanonicalSpelling(complete_arguments[argument]);
				extern_instantiation_keys_.insert(request_key.str());
				return true;
			}
			Instantiate(definition, complete_arguments, context, true, 0, 0, 0,
				&inferred_function_values);
			return true;
		} catch(const logic_error&) {
			continue;
		}
	}
	return false;
}

} // namespace pa18_templates_internal
