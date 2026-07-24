#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::MaterializeExplicitInstantiation(
	const CPPGMAstNodePtr& target, const string& context,
	bool extern_instantiation)
{
	if(!target || target->kind != "simple-declaration") return false;
	const CPPGMAstNodePtr declarator = FunctionDeclarator(target);
	const CPPGMAstNodePtr parameters = DescendantOfKind(declarator,
		"parameter-clause");
	if(!declarator || !parameters) return false;
	string raw_name = RemoveMarker(FirstIdentifierLocal(declarator));
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
	if(function_name.compare(0, 8, "operator") == 0) { bool user_defined_operand = false;
		for(size_t parameter = 0; parameter < parameters->children.size(); ++parameter) {
			const CPPGMAstNodePtr parameter_node = parameters->children[parameter];
			if(!parameter_node || parameter_node->kind != "parameter-declaration") continue;
			string operand = CanonicalSpelling(ParameterTypeSpelling(parameter_node));
			while(operand.compare(0, 6, "const ") == 0 || operand.compare(0, 9, "volatile ") == 0) operand = CanonicalSpelling(operand.substr(operand.find(' ') + 1));
			while(operand.size() > 6 && operand.compare(operand.size() - 6, 6, " const") == 0) operand = CanonicalSpelling(operand.substr(0, operand.size() - 6));
			while(operand.size() > 9 && operand.compare(operand.size() - 9, 9, " volatile") == 0) operand = CanonicalSpelling(operand.substr(0, operand.size() - 9));
			while(!operand.empty() && (operand[operand.size() - 1] == '&' || operand[operand.size() - 1] == '*')) operand = CanonicalSpelling(operand.substr(0, operand.size() - 1));
			if(!IsBuiltinArithmeticType(operand) && (FindClassDeclaration(operand, context) || named_type_contexts_.find(operand) != named_type_contexts_.end())) { user_defined_operand = true; break; }
		}
		if(!user_defined_operand) throw logic_error("explicit instantiation of builtin operator");
	}
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
	if(candidates.empty()) return false;

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
		const vector<string>* explicit_prefix = normalized_explicit.empty() ? 0 :
			&normalized_explicit;
		try {
			if(!InferFunctionArguments(definition, call, &complete_arguments,
				map<string, string>(), context, explicit_prefix, 0,
				&inferred_function_values)) continue;
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
			if(!candidates[i]->parameters[parameter].name.empty()) local[candidates[i]->parameters[parameter].name] = inferred[parameter];
		*result = NormalizeTypeArgument(ResolveAlias(ReplaceIdentifiers(type, local), context));
		return !result->empty();
	}
	return false;
}

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
	if(have_operands && (operation == "+" || operation == "-") &&
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

bool PA18TemplateExpander::InstantiateMemberCall(const CPPGMAstNodePtr& call,
	const CPPGMAstNodePtr& callee, const string& original_member,
	const string& context,
	const map<string, string>& substitutions,
	bool explicit_instantiation)
{
	if(!call || !callee || callee->kind != "member-expression" ||
		callee->children.size() < 2 || !callee->children[1]) return false;
	string member_spelling = original_member.empty() ? callee->children[1]->value :
		original_member;
	member_spelling = RemoveMarker(member_spelling);
	member_spelling = CanonicalSpelling(member_spelling);
	if(member_spelling.empty()) return false;
	string member_name = member_spelling;
	string member_qualifier;
	vector<string> explicit_member_arguments;
	const size_t member_open = member_spelling.find('<');
	// Operator names contain `<` as part of the operator token (`operator<<`),
	// not as a template-id delimiter.  Let ordinary member lookup see those
	// names; explicit operator template-ids are handled by the parsed member
	// spelling when a real range is present.
	const size_t qualified_template_separator = member_open == string::npos ?
		string::npos : member_spelling.find("::", member_open);
	if(member_open != string::npos && qualified_template_separator != string::npos &&
		member_spelling.compare(0, 8, "operator") != 0) {
		// `base<T>::operator=` is a qualified member name, not a member
		// template-id named `base`.  Keep the dependent owner separate so the
		// inherited-member replay can materialize the operator on that base.
		member_qualifier = member_spelling.substr(0, qualified_template_separator);
		member_name = LastComponent(member_spelling.substr(
			qualified_template_separator + 2));
	} else if(member_open != string::npos && member_spelling.compare(0, 8, "operator") != 0) {
		string member_base;
		string member_argument_text;
		size_t member_begin = 0;
		size_t member_close = string::npos;
		if(!TemplateBase(member_spelling, member_open, &member_begin, &member_base) ||
			!TemplateRange(member_spelling, member_open, &member_argument_text,
				&member_close)) return false;
		member_name = LastComponent(member_base);
		const size_t qualifier_separator = member_base.rfind("::");
		if(qualifier_separator != string::npos)
			member_qualifier = member_base.substr(0, qualifier_separator);
		explicit_member_arguments = SplitTemplateArguments(member_argument_text);
	} else {
		member_name = LastComponent(member_name);
		const size_t qualifier_separator = member_spelling.rfind("::");
		if(qualifier_separator != string::npos)
			member_qualifier = member_spelling.substr(0, qualifier_separator);
	}
	if(member_name.empty()) return false;
	string object_type;
	if(callee->children[0] && callee->children[0]->kind == "keyword-literal" &&
		RemoveMarker(callee->children[0]->value) == "this") {
		map<string, string>::const_iterator function_owner = function_owners_.find(context);
		if(function_owner != function_owners_.end()) object_type = function_owner->second;
		for(string current = object_type.empty() ? context : string(); !current.empty(); ) {
			const TemplateDefinition* current_definition = FindDefinition(current, context);
			if(class_contexts_.find(current) != class_contexts_.end() ||
				(current_definition && current_definition->class_template)) {
				object_type = current;
				break;
			}
			if(current.empty()) break;
			const size_t separator = current.rfind("::");
			if(separator == string::npos) current.clear();
			else current.erase(separator);
		}
		if(object_type.empty()) object_type = context;
	} else if(!InferArgument(callee->children[0], &object_type, substitutions, context))
		return false;
	object_type = CanonicalSpelling(RewriteText(object_type, context, substitutions, 0));
	object_type = ResolveAlias(object_type, context);
	while(object_type.compare(0, 6, "const ") == 0 ||
		object_type.compare(0, 9, "volatile ") == 0)
		object_type = CanonicalSpelling(object_type.substr(object_type.find(' ') + 1));
	while(!object_type.empty() && (object_type[object_type.size() - 1] == '*' ||
		object_type[object_type.size() - 1] == '&'))
		object_type = CanonicalSpelling(object_type.substr(0, object_type.size() - 1));
	while(object_type.size() > 6 && object_type.compare(object_type.size() - 6, 6,
		" const") == 0)
		object_type = CanonicalSpelling(object_type.substr(0, object_type.size() - 6));
	while(object_type.size() > 9 && object_type.compare(object_type.size() - 9, 9,
		" volatile") == 0)
		object_type = CanonicalSpelling(object_type.substr(0, object_type.size() - 9));
	if(object_type.empty()) return false;
	const TemplateDefinition* parent = 0;
	vector<string> parent_arguments;
	map<string, string> member_substitutions = substitutions;
	map<string, string>::const_iterator owner_base = specialization_bases_.find(
		LastComponent(object_type));
	map<string, vector<string> >::const_iterator owner_arguments =
		specialization_arguments_.find(LastComponent(object_type));
	if(owner_base != specialization_bases_.end() &&
		owner_arguments != specialization_arguments_.end()) {
		parent = FindDefinition(owner_base->second, context);
		parent_arguments = owner_arguments->second;
		if(parent && parent->class_template) {
			const TemplateDefinition* selected_parent = SelectClassTemplateDefinition(
				parent, parent_arguments, context);
			if(selected_parent) parent = selected_parent;
		}
	} else {
		string source_owner = object_type;
		const size_t source_open = source_owner.find('<');
		if(source_open != string::npos) source_owner.erase(source_open);
		const TemplateDefinition* candidate_parent = FindDefinition(source_owner, context);
		if(candidate_parent && candidate_parent->class_template) parent = candidate_parent;
	}
	if(parent) {
		// A member template-id in the definition of a function template still
		// needs the dependent-name `template` disambiguator.  Do not turn a
		// dependent `Box<Tag>` object into a concrete member specialization merely
		// because its member has explicit arguments; the normal dependent-name
		// validation must reject the missing keyword.
		if(parent_arguments.empty() && object_type.find('<') != string::npos) {
			for(size_t parameter = 0; parameter < parent->parameters.size(); ++parameter) {
				const string& name = parent->parameters[parameter].name;
				if(name.empty()) continue;
				for(size_t position = object_type.find(name); position != string::npos;
					position = object_type.find(name, position + name.size())) {
					const bool left = position == 0 || !IsIdentifierCharacter(object_type[position - 1]);
					const size_t end = position + name.size();
					const bool right = end == object_type.size() ||
						!IsIdentifierCharacter(object_type[end]);
					if(left && right) return false;
				}
			}
		}
		for(size_t parameter = 0; parameter < parent->parameters.size() &&
			parameter < parent_arguments.size(); ++parameter)
			if(!parent->parameters[parameter].name.empty())
				member_substitutions[parent->parameters[parameter].name] =
					parent_arguments[parameter];
		if(!parent->name.empty()) member_substitutions[parent->name] = object_type;
	}
	string qualified_owner;
	if(!member_qualifier.empty()) {
		qualified_owner = CanonicalSpelling(RewriteText(member_qualifier, context,
			member_substitutions, 0));
		qualified_owner = CanonicalSpelling(ReplaceIdentifiers(qualified_owner,
			member_substitutions));
		qualified_owner = CanonicalSpelling(ResolveAlias(qualified_owner, context));
		const size_t owner_open = qualified_owner.find('<');
		if(owner_open != string::npos) qualified_owner.erase(owner_open);
		qualified_owner = LastComponent(qualified_owner);
	}

	vector<const TemplateDefinition*> candidates;
	map<string, vector<string> >::const_iterator indexed_members =
		definitions_by_name_.find(member_name);
	if(indexed_members != definitions_by_name_.end()) for(size_t indexed = 0;
		indexed < indexed_members->second.size(); ++indexed) {
		map<string, TemplateDefinition>::const_iterator it = definitions_.find(
			indexed_members->second[indexed]);
		if(it == definitions_.end()) continue;
		const TemplateDefinition& definition = it->second;
		if(definition.class_template || definition.alias_template ||
			definition.variable_template || definition.parameters.empty() ||
			LastComponent(definition.name) != member_name || !definition.declaration)
			continue;
		const bool declaration_kind = definition.declaration->kind == "function-definition" ||
			definition.declaration->kind == "simple-declaration" ||
			definition.declaration->kind == "special-member-definition";
		if(!declaration_kind) continue;
		bool owner_matches = false;
		if(!qualified_owner.empty()) {
			string source_owner = definition.owner;
			const size_t source_open = source_owner.find('<');
			if(source_open != string::npos) source_owner.erase(source_open);
			owner_matches = source_owner == qualified_owner ||
				LastComponent(source_owner) == qualified_owner;
		} else if(parent) owner_matches = MemberOwnerPattern(definition, *parent,
			parent_arguments, 0);
		else {
			string source_owner = definition.owner;
			const size_t source_open = source_owner.find('<');
			if(source_open != string::npos) source_owner.erase(source_open);
			owner_matches = source_owner == object_type ||
				LastComponent(source_owner) == LastComponent(object_type);
		}
		if(owner_matches) candidates.push_back(&definition);
	}
	if(parent && parent->partial_specialization) {
		const string member_scope = JoinPath(parent->qualified_name, parent->name);
		vector<const TemplateDefinition*> specialized_candidates;
		for(size_t candidate = 0; candidate < candidates.size(); ++candidate) {
			const string owner = candidates[candidate]->owner;
			const size_t angle = owner.find('<');
			if(angle != string::npos && owner.substr(0, angle) == member_scope)
				specialized_candidates.push_back(candidates[candidate]);
		}
		if(!specialized_candidates.empty()) candidates.swap(specialized_candidates);
	}
	const vector<const TemplateDefinition*> direct_member_candidates = candidates;
	vector<const TemplateDefinition*> inherited_candidates;
	set<string> inherited_active;
	map<const TemplateDefinition*, string> inherited_owners;
	CollectInheritedMemberTemplates(object_type, member_name, member_substitutions,
		context, &inherited_candidates, &inherited_active, &inherited_owners);
	for(size_t inherited = 0; inherited < inherited_candidates.size(); ++inherited)
		if(find(candidates.begin(), candidates.end(), inherited_candidates[inherited]) ==
			candidates.end() || inherited_owners.find(inherited_candidates[inherited]) !=
			inherited_owners.end()) candidates.push_back(inherited_candidates[inherited]);
	// Once the enclosing owner is concrete, overloads that differ only by
	// renamed owner parameters can deduce the same call shape.  Prefer the
	// member template with the smaller member parameter contract; this is the
	// common partial-ordering result for `g<T,S>(T,S)` versus `g<T>(T,T)`.
	stable_sort(candidates.begin(), candidates.end(),
		[](const TemplateDefinition* left, const TemplateDefinition* right) {
			if(left->parameters.size() != right->parameters.size())
				return left->parameters.size() < right->parameters.size();
			return false;
		});
	if(candidates.empty()) return false;
	map<const TemplateDefinition*, size_t> candidate_occurrences;
	for(size_t candidate_index = 0; candidate_index < candidates.size();
		++candidate_index) {
		const TemplateDefinition& definition = *candidates[candidate_index];
		const size_t occurrence = candidate_occurrences[&definition]++;
		const bool direct_member = occurrence == 0 &&
			find(direct_member_candidates.begin(), direct_member_candidates.end(),
				&definition) != direct_member_candidates.end();
		map<string, string> candidate_substitutions = member_substitutions;
		map<const TemplateDefinition*, string>::const_iterator candidate_owner =
			inherited_owners.find(&definition);
		const string concrete_candidate_owner = !direct_member &&
			candidate_owner != inherited_owners.end() && !candidate_owner->second.empty() ?
			candidate_owner->second : object_type;
		// An inherited member template is deduced against the specialization that
		// actually declares it, not against the most-derived object.  Recover that
		// declaring specialization's template arguments before expanding aliases
		// such as `key_type` in its parameter list.
		map<string, string>::const_iterator inherited_base = specialization_bases_.find(
			LastComponent(concrete_candidate_owner));
		map<string, vector<string> >::const_iterator inherited_arguments =
			specialization_arguments_.find(LastComponent(concrete_candidate_owner));
		if(!direct_member && candidate_owner != inherited_owners.end() &&
			inherited_base != specialization_bases_.end() &&
			inherited_arguments != specialization_arguments_.end()) {
			const TemplateDefinition* inherited_definition = FindDefinition(
				inherited_base->second, context);
			if(inherited_definition && inherited_definition->class_template) {
				for(size_t parameter = 0; parameter < inherited_definition->parameters.size() &&
					parameter < inherited_arguments->second.size(); ++parameter)
					if(!inherited_definition->parameters[parameter].name.empty())
						candidate_substitutions[inherited_definition->parameters[parameter].name] =
							inherited_arguments->second[parameter];
				if(!inherited_definition->name.empty())
					candidate_substitutions[inherited_definition->name] = concrete_candidate_owner;
			}
		}
		const CPPGMAstNodePtr concrete_candidate_declaration =
			FindClassDeclaration(concrete_candidate_owner, context);
		if(concrete_candidate_declaration) for(size_t member = 0;
			member < concrete_candidate_declaration->children.size(); ++member) {
			const CPPGMAstNodePtr declaration = concrete_candidate_declaration->children[member];
			if(!declaration || declaration->kind != "simple-declaration" ||
				declaration->children.empty() || SpellNode(declaration->children[0]).find(
					"typedef") == string::npos) continue;
			const CPPGMAstNodePtr list = ChildOfKindLocal(declaration,
				"init-declarator-list");
			if(!list) continue;
			for(size_t item = 0; item < list->children.size(); ++item) {
				const CPPGMAstNodePtr entry = list->children[item];
				if(!entry || entry->children.empty()) continue;
				const string alias = FirstIdentifierLocal(entry->children[0]);
				if(alias.empty()) continue;
				string alias_type = NodeTypeSpelling(declaration->children[0]) +
					DeclaratorSuffix(entry->children[0]);
				alias_type = CanonicalSpelling(ResolveAlias(ReplaceIdentifiers(
					RewriteText(alias_type, concrete_candidate_owner,
						candidate_substitutions, 0), candidate_substitutions), context));
				if(!alias_type.empty()) candidate_substitutions[alias] = alias_type;
			}
		}
		if(definition.declaration->kind == "simple-declaration") {
			bool has_definition = false;
			for(size_t other_index = 0; other_index < candidates.size(); ++other_index) {
				const TemplateDefinition& other = *candidates[other_index];
				if(other.declaration->kind == "function-definition" &&
					( MemberSignatureKey(other) == MemberSignatureKey(definition) ||
						(definition.friend_declaration &&
							LastComponent(other.name) == LastComponent(definition.name)))) {
					has_definition = true;
					break;
				}
			}
			if(has_definition) continue;
		}
		vector<string> member_arguments;
		map<string, vector<string> > inferred_pack_values;
		map<string, FunctionSignature> inferred_function_values;
		vector<string> explicit_arguments = explicit_member_arguments;
		for(size_t argument = 0; argument < explicit_arguments.size(); ++argument) {
			explicit_arguments[argument] = NormalizeTypeArgument(RewriteText(
				explicit_arguments[argument], context, candidate_substitutions, 0));
			explicit_arguments[argument] = NormalizeTypeArgument(ReplaceIdentifiers(
				explicit_arguments[argument], candidate_substitutions));
			explicit_arguments[argument] = ResolveAlias(explicit_arguments[argument], context);
			explicit_arguments[argument] = QualifyTypeArgument(
				explicit_arguments[argument], context, definition.owner);
		}
		const vector<string>* explicit_prefix = explicit_arguments.empty() ? 0 :
			&explicit_arguments;
		bool inferred = false;
		map<string, string> deduction_substitutions = candidate_substitutions;
		if(parent && !parent->name.empty())
			// The enclosing class name is useful while replaying the generated
			// member body, but it is not a template parameter.  Leaving it in the
			// deduction map makes textual substitution turn `iter<Buff, T>` into
			// `iter_<concrete-args><Buff, T>` before matching the member pattern.
			deduction_substitutions.erase(parent->name);
		map<string, vector<string> > bound_pack_values;
		if(parent) {
			size_t parent_argument = 0;
			for(size_t parent_parameter_index = 0;
				parent_parameter_index < parent->parameters.size(); ++parent_parameter_index) {
				const TemplateParameter& parent_parameter =
					parent->parameters[parent_parameter_index];
				if(parent_parameter.pack) {
					size_t trailing_fixed = 0;
					for(size_t later = parent_parameter_index + 1;
						later < parent->parameters.size(); ++later)
						if(!parent->parameters[later].pack) ++trailing_fixed;
					const size_t available = parent_arguments.size() > parent_argument ?
						parent_arguments.size() - parent_argument : 0;
					const size_t count = available > trailing_fixed ?
						available - trailing_fixed : 0;
					vector<string>& values = bound_pack_values[parent_parameter.name];
					for(size_t element = 0; element < count; ++element)
						values.push_back(parent_arguments[parent_argument++]);
				} else if(parent_argument < parent_arguments.size()) ++parent_argument;
			}
		}
		try {
			inferred = InferFunctionArguments(definition, call, &member_arguments,
				deduction_substitutions, context, explicit_prefix, &inferred_pack_values,
				&inferred_function_values, &bound_pack_values);
		} catch(const logic_error&) {
			inferred = false;
		}
		// An explicit member-template-id already fixes every template parameter.
		// Function-pointer expressions can still be intentionally deferred by the
		// general deduction path (notably an address of an overloaded function),
		// but that must not prevent the explicit specialization from being
		// materialized under its concrete member owner.
		if(!inferred && !explicit_arguments.empty() &&
			explicit_arguments.size() == definition.parameters.size() &&
			find_if(definition.parameters.begin(), definition.parameters.end(),
				[](const TemplateParameter& parameter) { return parameter.pack; }) ==
				definition.parameters.end()) {
			member_arguments = explicit_arguments;
			inferred = true;
		}
		if(!inferred) continue;
		bool dependent_member_arguments = false;
		for(size_t parameter = 0; parameter < definition.parameters.size() &&
			!dependent_member_arguments; ++parameter) {
			const string& name = definition.parameters[parameter].name;
			if(name.empty()) continue;
			for(size_t argument = 0; argument < member_arguments.size(); ++argument)
				for(size_t at = member_arguments[argument].find(name); at != string::npos;
					at = member_arguments[argument].find(name, at + name.size())) {
					const bool left = at == 0 || !IsIdentifierCharacter(
						member_arguments[argument][at - 1]);
					const size_t end = at + name.size();
					const bool right = end == member_arguments[argument].size() ||
						!IsIdentifierCharacter(member_arguments[argument][end]);
					if(left && right) { dependent_member_arguments = true; break; }
				}
		}
		if(dependent_member_arguments) continue;

		string requested_owner = object_type;
		map<const TemplateDefinition*, string>::const_iterator inherited_owner =
			inherited_owners.find(&definition);
		if(!direct_member && inherited_owner != inherited_owners.end() &&
			!inherited_owner->second.empty())
			requested_owner = inherited_owner->second;
		const bool concrete_owner = specialization_bases_.find(
			LastComponent(requested_owner)) != specialization_bases_.end() &&
			specialization_arguments_.find(LastComponent(requested_owner)) !=
				specialization_arguments_.end();
		const string* requested_owner_pointer = concrete_owner ? &requested_owner : 0;
		map<string, vector<string> > instantiation_pack_hints = inferred_pack_values;
		for(map<string, vector<string> >::const_iterator bound = bound_pack_values.begin();
			bound != bound_pack_values.end(); ++bound)
			instantiation_pack_hints[bound->first].insert(
				instantiation_pack_hints[bound->first].end(), bound->second.begin(),
				bound->second.end());
		string generated_name;
		const ConcreteOwnerContext previous_concrete_owner = active_concrete_owner_;
		if(requested_owner_pointer) SetActiveConcreteOwner(requested_owner, context);
		try {
		generated_name = Instantiate(definition, member_arguments, context,
			explicit_instantiation,
				&instantiation_pack_hints, &candidate_substitutions,
				requested_owner_pointer, &inferred_function_values);
		} catch(const logic_error&) {
			active_concrete_owner_ = previous_concrete_owner;
			continue;
		}
		active_concrete_owner_ = previous_concrete_owner;
		call->template_primary = definition.qualified_name;
		call->template_arguments = member_arguments;
		map<string, string> result_substitutions = candidate_substitutions;
		for(size_t parameter = 0; parameter < definition.parameters.size() &&
			parameter < member_arguments.size(); ++parameter)
			if(!definition.parameters[parameter].name.empty())
				result_substitutions[definition.parameters[parameter].name] =
					member_arguments[parameter];
		map<const TemplateDefinition*, string>::const_iterator result_owner =
			inherited_owners.find(&definition);
		string result_owner_name = !direct_member && result_owner != inherited_owners.end() ?
			result_owner->second : object_type;
		map<string, string>::const_iterator result_base = specialization_bases_.find(
			LastComponent(result_owner_name));
		map<string, vector<string> >::const_iterator result_arguments =
			specialization_arguments_.find(LastComponent(result_owner_name));
		if(result_base != specialization_bases_.end() &&
			result_arguments != specialization_arguments_.end()) {
			const TemplateDefinition* owner_definition = FindDefinition(result_base->second,
				context);
			if(owner_definition) for(size_t parameter = 0;
				parameter < owner_definition->parameters.size() &&
				parameter < result_arguments->second.size(); ++parameter)
				if(!owner_definition->parameters[parameter].name.empty())
					result_substitutions[owner_definition->parameters[parameter].name] =
						result_arguments->second[parameter];
		}
		if(definition.declaration && !definition.declaration->children.empty()) {
			string result_type = NodeTypeSpelling(definition.declaration->children[0]);
			const CPPGMAstNodePtr result_declarator = FunctionDeclarator(definition.declaration);
			result_type += ReturnDeclaratorSuffix(result_declarator);
			result_type = CanonicalSpelling(ResolveAlias(RewriteText(result_type, context,
				result_substitutions, 0), context));
			call->inferred_type = result_type;
		}
	const bool static_member = definition.static_member;
		const bool generated_operator = member_name.compare(0, 8, "operator") == 0;
		const bool ordinary_class_member = !definition.owner.empty() &&
			definition.owner.find('<') == string::npos &&
			FindClassDeclaration(definition.owner, context) != CPPGMAstNodePtr();
		if(static_member && definition.owner.find("::") == string::npos && concrete_owner) {
			call->children[0] = CPPGMAstNodePtr(new CPPGMAstNode("id-expression",
				definition.owner + "::" + member_name));
		} else callee->children[1]->value =
			(ordinary_class_member && !generated_operator) ? member_name :
			(concrete_owner && !generated_operator ? member_name : generated_name);
		if(!member_qualifier.empty() && concrete_owner && !static_member) {
			// Preserve a dependent qualified-base call as a qualified generated
			// function.  Leaving it as `this->operator=...` redispatches through
			// the derived class during PA14 lookup and loses the selected base
			// specialization.
			call->children[0] = CPPGMAstNodePtr(new CPPGMAstNode("id-expression",
				requested_owner + "::" + generated_name));
		}
		return true;
	}
	return false;
}

bool PA18TemplateExpander::IsKnownMemberTemplateId(const string& raw) const
{
	const string spelling = CanonicalSpelling(RemoveMarker(raw));
	const size_t open = spelling.find('<');
	if(open == string::npos) return false;
	string base;
	string arguments;
	size_t begin = 0;
	size_t close = string::npos;
	if(!TemplateBase(spelling, open, &begin, &base) ||
		!TemplateRange(spelling, open, &arguments, &close)) return false;
	const string member_name = LastComponent(base);
	map<string, vector<string> >::const_iterator indexed = definitions_by_name_.find(member_name);
	if(indexed == definitions_by_name_.end()) return false;
	for(size_t i = 0; i < indexed->second.size(); ++i) {
		map<string, TemplateDefinition>::const_iterator found = definitions_.find(indexed->second[i]);
		if(found == definitions_.end()) continue;
		const TemplateDefinition& definition = found->second;
		if(definition.member_template && !definition.class_template &&
			LastComponent(definition.name) == member_name)
			return true;
	}
	return false;
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
	for(size_t child = 0; child < declaration->children.size(); ++child) {
		const CPPGMAstNodePtr clause = declaration->children[child];
		if(!clause || clause->kind != "base-clause") continue;
		for(size_t base_index = 0; base_index < clause->children.size(); ++base_index) {
			const CPPGMAstNodePtr base_specifier = clause->children[base_index];
			const CPPGMAstNodePtr base_name = ChildOfKindLocal(base_specifier, "base-name");
			if(!base_name) continue;
			string base_spelling = CanonicalSpelling(RemoveMarker(RewriteText(
				base_name->value, declaration_context, class_substitutions, 0)));
			base_spelling = CanonicalSpelling(ReplaceIdentifiers(base_spelling,
				class_substitutions));
			base_spelling = CanonicalSpelling(ResolveAlias(base_spelling,
				declaration_context));
			string base_lookup = base_spelling;
			vector<string> base_arguments;
			const TemplateDefinition* base_definition = 0;
			bool base_lookup_generated = false;
			const size_t open = base_spelling.find('<');
			if(open != string::npos) {
				string argument_text;
				size_t close = string::npos;
				if(!TemplateRange(base_spelling, open, &argument_text, &close)) continue;
				base_lookup = CanonicalSpelling(base_spelling.substr(0, open));
				base_definition = FindDefinition(base_lookup, declaration_context);
				base_arguments = SplitTemplateArguments(argument_text);
				if(base_definition) for(size_t argument = 0; argument < base_arguments.size(); ++argument) {
					base_arguments[argument] = NormalizeTypeArgument(RewriteText(
						base_arguments[argument], declaration_context, class_substitutions, 0));
					base_arguments[argument] = NormalizeTypeArgument(ReplaceIdentifiers(
						base_arguments[argument], class_substitutions));
					base_arguments[argument] = ResolveAlias(base_arguments[argument],
						declaration_context);
					base_arguments[argument] = QualifyTypeArgument(base_arguments[argument],
						declaration_context, base_definition->owner);
				}
			}
			if(!base_definition) {
				base_definition = FindDefinition(base_lookup, declaration_context);
				if(base_definition && base_definition->class_template)
					base_lookup = base_definition->qualified_name;
			}
			if(!base_definition) {
				map<string, string>::const_iterator generated = specialization_bases_.find(
					LastComponent(base_lookup));
				map<string, vector<string> >::const_iterator generated_args =
					specialization_arguments_.find(LastComponent(base_lookup));
				if(generated != specialization_bases_.end() &&
					generated_args != specialization_arguments_.end()) {
					base_definition = FindDefinition(generated->second, declaration_context);
					if(base_definition) {
						base_arguments = generated_args->second;
						base_lookup_generated = true;
					}
				}
			}
			map<string, string> base_substitutions = class_substitutions;
			if(base_definition) for(size_t parameter = 0;
				parameter < base_definition->parameters.size() &&
				parameter < base_arguments.size(); ++parameter)
				if(!base_definition->parameters[parameter].name.empty())
					base_substitutions[base_definition->parameters[parameter].name] =
						base_arguments[parameter];
			map<string, vector<string> >::const_iterator indexed_members =
				definitions_by_name_.find(member);
			if(indexed_members != definitions_by_name_.end()) for(size_t indexed = 0;
				indexed < indexed_members->second.size(); ++indexed) {
				map<string, TemplateDefinition>::const_iterator it = definitions_.find(
					indexed_members->second[indexed]);
				if(it == definitions_.end()) continue;
				const TemplateDefinition& candidate = it->second;
				if(candidate.class_template || candidate.alias_template ||
					candidate.variable_template || candidate.parameters.empty() ||
					LastComponent(candidate.name) != member || !candidate.declaration)
					continue;
				const bool declaration_kind = candidate.declaration->kind == "function-definition" ||
					candidate.declaration->kind == "simple-declaration" ||
					candidate.declaration->kind == "special-member-definition";
				if(!declaration_kind) continue;
				string owner = candidate.owner;
				const size_t owner_open = owner.find('<');
				if(owner_open != string::npos) owner.erase(owner_open);
				bool matches = false;
				if(base_definition && base_definition->class_template)
					matches = MemberOwnerPattern(candidate, *base_definition,
						base_arguments, 0);
				else matches = owner == base_lookup ||
					LastComponent(owner) == LastComponent(base_lookup);
				if(!matches) continue;
				if(concrete_owners && base_lookup_generated &&
					concrete_owners->find(&candidate) == concrete_owners->end())
					(*concrete_owners)[&candidate] = base_lookup;
				if(find(result->begin(), result->end(), &candidate) == result->end())
					result->push_back(&candidate);
			}
			string recursive_base = base_lookup;
			if(!base_lookup_generated && !base_arguments.empty() && base_definition &&
				base_definition->class_template) {
				recursive_base += "<";
				for(size_t argument = 0; argument < base_arguments.size(); ++argument) {
					if(argument) recursive_base += ",";
					recursive_base += base_arguments[argument];
				}
				recursive_base += ">";
			}
			CollectInheritedMemberTemplates(recursive_base, member, base_substitutions,
				declaration_context, result, active, concrete_owners);
		}
	}
	active->erase(class_key + "|" + member);
}

CPPGMAstNodePtr PA18TemplateExpander::TransformCallExpression(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions)
{
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
	if(input_callee && input_callee->kind == "id-expression") {
		const string raw_callee = input_callee->value;
		string lookup_callee = raw_callee;
		const size_t qualifier_separator = lookup_callee.find("::");
		if(qualifier_separator != string::npos) {
			const map<string, string>::const_iterator alias = substitutions.find(
				lookup_callee.substr(0, qualifier_separator));
			if(alias != substitutions.end()) lookup_callee = alias->second +
				lookup_callee.substr(qualifier_separator);
		}
		const size_t open = lookup_callee.find('<');
		if(open != string::npos) {
			string base;
			size_t begin = 0;
			string argument_text;
			size_t close = string::npos;
			const TemplateDefinition* explicit_definition = 0;
			if(TemplateBase(lookup_callee, open, &begin, &base) &&
				TemplateRange(lookup_callee, open, &argument_text, &close))
				explicit_definition = FindDefinition(base, context);
			if(explicit_definition && !explicit_definition->class_template) {
				const vector<const TemplateDefinition*> overloads = FindFunctionDefinitions(base, context);
				if(overloads.size() > 1) {
					const vector<string> raw_explicit_args = SplitTemplateArguments(argument_text);
					for(size_t overload = 0; overload < overloads.size(); ++overload) {
						vector<string> trial_arguments;
						try {
							if(InferFunctionArguments(*overloads[overload], input, &trial_arguments,
								substitutions, context, &raw_explicit_args)) {
								explicit_definition = overloads[overload];
								break;
							}
						} catch(const logic_error&) {}
					}
				}
			}
			if(explicit_definition && !explicit_definition->class_template) {
				vector<string> explicit_args = SplitTemplateArguments(argument_text);
				map<string, string> explicit_substitutions = substitutions;
				for(map<string, PA19IntegralValue>::const_iterator integral =
					active_integral_substitutions_.begin();
					integral != active_integral_substitutions_.end(); ++integral)
					if(integral->second.known)
						explicit_substitutions[integral->first] =
							IntegralValueSpelling(integral->second);
				for(size_t i = 0; i < explicit_args.size(); ++i) {
					explicit_args[i] = NormalizeTypeArgument(RewriteText(
						explicit_args[i], context, explicit_substitutions, 0));
					explicit_args[i] = NormalizeTypeArgument(ReplaceIdentifiers(
						explicit_args[i], explicit_substitutions));
					explicit_args[i] = ResolveAlias(explicit_args[i], context);
					explicit_args[i] = NormalizeTypeArgument(RewriteText(
						explicit_args[i], context, explicit_substitutions, 0));
					explicit_args[i] = ResolveAlias(explicit_args[i], context);
					explicit_args[i] = QualifyTypeArgument(explicit_args[i], context,
						explicit_definition->owner);
				}
				const TemplateDefinition* explicit_specialization =
					FindExplicitFunctionSpecialization(base, explicit_args, context);
				if(explicit_specialization) explicit_definition = explicit_specialization;
				vector<string> complete_args;
				map<string, FunctionSignature> inferred_function_values;
				bool has_parameter_pack = false;
				size_t fixed_template_parameters = 0;
				for(size_t parameter = 0; parameter < explicit_definition->parameters.size(); ++parameter)
					if(explicit_definition->parameters[parameter].pack)
						has_parameter_pack = true;
					else ++fixed_template_parameters;
				// Explicit arguments fill a trailing pack only once at least one
				// element beyond the fixed prefix was written.  With just the
				// fixed prefix (`construct<T>(args...)`), the function arguments
				// still deduce the remaining pack.
				const bool explicit_pack_elements = has_parameter_pack &&
					explicit_args.size() > fixed_template_parameters;
				bool complete = explicit_pack_elements ||
					(!has_parameter_pack && explicit_args.size() == explicit_definition->parameters.size());
				if(complete) complete_args = explicit_args;
					else if(explicit_args.size() < explicit_definition->parameters.size())
						complete = InferFunctionArguments(*explicit_definition, input,
							&complete_args, substitutions, context, &explicit_args, 0,
							&inferred_function_values);
				if(complete) {
					const string local_name = Instantiate(*explicit_definition, complete_args, context,
						false, 0, 0, 0, &inferred_function_values);
					result->template_primary = explicit_definition->qualified_name;
					result->template_arguments = complete_args;
					const string qualifier = PrefixComponent(base);
					CPPGMAstNodePtr callee(new CPPGMAstNode("id-expression",
						qualifier.empty() ? local_name : qualifier + "::" + local_name));
					result->children.push_back(callee);
					for(size_t i = 1; i < input->children.size(); ++i) {
						CPPGMAstNodePtr child = TransformNode(input->children[i], context,
							substitutions);
						if(child) result->children.push_back(child);
					}
					return result;
				}
			}
		}
	}
	for(size_t i = 0; i < input->children.size(); ++i) {
		CPPGMAstNodePtr child = TransformNode(input->children[i], context, substitutions);
		if(child) result->children.push_back(child);
	}
	CPPGMAstNodePtr result_callee = result->children.empty() ? CPPGMAstNodePtr() :
		result->children[0];
	if(result_callee && result_callee->kind == "parenthesized-expression" &&
		result_callee->children.size() == 1 && result_callee->children[0] &&
		result_callee->children[0]->kind == "id-expression") {
		result_callee = result_callee->children[0];
		result->children[0] = result_callee;
	}
	// A constructor's function-pointer parameter supplies the expected
	// signature for an otherwise overloaded function template argument.  The
	// class specialization has already been rewritten at this point, so use
	// its concrete constructor declaration before ordinary call deduction.
	ResolveClassConstructorFunctionArguments(result, context);
	// RewriteText may have already collapsed a qualified class-template-id to
	// its generated concrete owner while retaining a member-template call as
	// `Owner_generated::member`.  Recover the typed owner here; otherwise the
	// ordinary free-function lookup sees only the generated spelling and loses
	// the selected member specialization (notably a partial class
	// specialization reached from a recursive member body).
	if(result_callee && result_callee->kind == "id-expression") {
		const string qualified = RemoveMarker(result_callee->value);
		const size_t separator = qualified.rfind("::");
		if(separator != string::npos) {
			const string owner = qualified.substr(0, separator);
			const string member_name = LastComponent(qualified.substr(separator + 2));
			string qualified_owner = owner;
			if(!owner.empty()) qualified_owner = CanonicalSpelling(
				QualifyTypeArgument(owner, context));
			if(!qualified_owner.empty() && !member_name.empty() &&
				(specialization_bases_.find(LastComponent(qualified_owner)) !=
					specialization_bases_.end() || class_contexts_.find(qualified_owner) !=
					class_contexts_.end())) {
				CPPGMAstNodePtr object(new CPPGMAstNode("id-expression"));
				object->inferred_type = qualified_owner;
				CPPGMAstNodePtr synthetic_member(new CPPGMAstNode("member-expression", "."));
				synthetic_member->children.push_back(object);
				synthetic_member->children.push_back(CPPGMAstNodePtr(
					new CPPGMAstNode("identifier", member_name)));
				if(InstantiateMemberCall(result, synthetic_member, member_name,
					context, substitutions)) {
					result->children[0] = CPPGMAstNodePtr(new CPPGMAstNode(
						"id-expression", qualified_owner + "::" +
							synthetic_member->children[1]->value));
					result->template_instantiation = true;
					return result;
				}
			}
		}
	}
	bool constructor_replayed = false;
	string constructor_type;
	if(result_callee && result_callee->kind == "id-expression") {
		constructor_type = result_callee->value;
		// A constructor used as a functional cast is commonly unqualified inside
		// its class definition.  Resolve that spelling to the owning class before
		// asking the member-template index for a constructor specialization.
		if(constructor_type.find("::") == string::npos) {
			for(string current = context; ; ) {
				const string candidate = JoinPath(current, constructor_type);
				if((class_contexts_.find(candidate) != class_contexts_.end() ||
					FindClassDeclaration(candidate, context)) &&
					LastComponent(candidate) == LastComponent(constructor_type)) {
					constructor_type = candidate;
					break;
				}
				if(current.empty()) break;
				const size_t separator = current.rfind("::");
				if(separator == string::npos) current.clear();
				else current.erase(separator);
			}
		}
		// A class specialization can retain a member-template constructor until
		// its argument list supplies the missing template arguments.  Replay that
		// constructor through the same owner-aware member-template path used for
		// `object.member(args...)`; the synthetic object carries only a typed
		// semantic fact and is never emitted into the transformed AST.
			map<string, string>::const_iterator constructor_base =
				specialization_bases_.find(LastComponent(constructor_type));
			const bool constructor_candidate = constructor_base != specialization_bases_.end() ||
				class_contexts_.find(constructor_type) != class_contexts_.end() ||
				FindClassDeclaration(constructor_type, context);
			if(constructor_candidate) {
				CPPGMAstNodePtr synthetic_object(new CPPGMAstNode("id-expression"));
				synthetic_object->inferred_type = constructor_type;
				CPPGMAstNodePtr synthetic_member(new CPPGMAstNode("member-expression", "."));
				synthetic_member->children.push_back(synthetic_object);
				synthetic_member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
					"identifier", constructor_base == specialization_bases_.end() ?
						LastComponent(constructor_type) : LastComponent(constructor_base->second))));
				constructor_replayed = InstantiateMemberCall(result, synthetic_member,
					constructor_base == specialization_bases_.end() ?
						LastComponent(constructor_type) : LastComponent(constructor_base->second),
					context, substitutions);
			}
		CPPGMAstNodePtr operator_member(new CPPGMAstNode("member-expression", "."));
		operator_member->children.push_back(result_callee);
		operator_member->children.push_back(CPPGMAstNodePtr(
			new CPPGMAstNode("identifier", "operator()")));
		if(InstantiateMemberCall(result, operator_member, "operator()", context,
			substitutions)) {
			result->children[0] = operator_member;
			result_callee = operator_member;
		}
	}
	if(result_callee && result_callee->kind == "call-expression") {
		// A braced functional construction such as `identity{}(value)` has a
		// call-expression as its callee.  Give member-template call operators the
		// same owner-aware materialization path as `object.operator()(value)`;
		// ordinary call operators remain available to PA14's callable-object path.
		CPPGMAstNodePtr operator_member(new CPPGMAstNode("member-expression", "."));
		operator_member->children.push_back(result_callee);
		operator_member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
			"identifier", "operator()")));
		if(InstantiateMemberCall(result, operator_member, "operator()", context,
			substitutions)) {
			result->children[0] = operator_member;
			result_callee = operator_member;
		}
	}
	if(input_callee && input_callee->kind == "id-expression" &&
		input_callee->value.compare(0, 7, "super::") == 0 &&
		result_callee && result_callee->kind == "id-expression") {
		string this_type;
		vector<const TemplateDefinition*> inherited;
		set<string> inherited_active;
		map<const TemplateDefinition*, string> inherited_owners;
		const CPPGMAstNodePtr this_expression(new CPPGMAstNode(
			"keyword-literal", "KW_THIS:this"));
		InferArgument(this_expression, &this_type, substitutions, context);
		CollectInheritedMemberTemplates(this_type, LastComponent(result_callee->value),
			substitutions, context, &inherited, &inherited_active, &inherited_owners);
		string base_owner;
		for(size_t inherited_index = 0; inherited_index < inherited.size(); ++inherited_index) {
			map<const TemplateDefinition*, string>::const_iterator owner =
				inherited_owners.find(inherited[inherited_index]);
			if(owner != inherited_owners.end() && !owner->second.empty()) {
				base_owner = owner->second;
				break;
			}
		}
		if(base_owner.empty()) {
			const CPPGMAstNodePtr declaration = FindClassDeclaration(this_type, context);
			if(declaration) for(size_t child = 0; child < declaration->children.size() &&
				base_owner.empty(); ++child) {
				const CPPGMAstNodePtr clause = declaration->children[child];
				if(!clause || clause->kind != "base-clause") continue;
				for(size_t base = 0; base < clause->children.size() && base_owner.empty(); ++base) {
					const CPPGMAstNodePtr base_name = ChildOfKindLocal(
						clause->children[base], "base-name");
					if(!base_name) continue;
					base_owner = CanonicalSpelling(ResolveAlias(ReplaceIdentifiers(
						RewriteText(base_name->value, context, substitutions, 0),
						substitutions), context));
				}
			}
		}
		CPPGMAstNodePtr base_object = this_expression;
		if(!base_owner.empty()) {
			CPPGMAstNodePtr type_id(new CPPGMAstNode("type-id"));
			CPPGMAstNodePtr specifiers(new CPPGMAstNode("type-specifier-seq"));
			specifiers->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
				"type-name", base_owner)));
			type_id->children.push_back(specifiers);
			CPPGMAstNodePtr abstract(new CPPGMAstNode("abstract-declarator"));
			abstract->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
				"ptr-operator", "OP_AMP:&")));
			type_id->children.push_back(abstract);
			CPPGMAstNodePtr cast(new CPPGMAstNode("cast-expression",
				"KW_STATIC_CAST:static_cast"));
			cast->children.push_back(type_id);
			CPPGMAstNodePtr dereference(new CPPGMAstNode("unary-expression", "OP_STAR:*"));
			dereference->children.push_back(this_expression);
			cast->children.push_back(dereference);
			base_object = cast;
		}
		CPPGMAstNodePtr base_member(new CPPGMAstNode("member-expression",
			base_owner.empty() ? "->" : "."));
		base_member->children.push_back(base_object);
		base_member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
			"identifier", LastComponent(result_callee->value))));
		const bool instantiated_base_call = InstantiateMemberCall(result, base_member,
			LastComponent(result_callee->value),
			context, substitutions);
		if(instantiated_base_call || !base_owner.empty()) {
			// A qualified base call is an implicit-this member call.  Preserve the
			// selected concrete owner in the callee so PA14 performs the required
			// base projection and does not redispatch the name against the current
			// derived specialization.
			if(!base_owner.empty()) {
				CPPGMAstNodePtr qualified_base(new CPPGMAstNode("id-expression",
					base_owner + "::" + LastComponent(result_callee->value)));
				result->children[0] = qualified_base;
				result_callee = qualified_base;
			} else {
				result->children[0] = base_member;
				result_callee = base_member;
			}
		}
	}
	if(result_callee && result_callee->kind == "parenthesized-expression" &&
		result_callee->children.size() == 1 && result_callee->children[0] &&
		result_callee->children[0]->kind == "call-expression") {
		CPPGMAstNodePtr operator_member(new CPPGMAstNode("member-expression", "."));
		operator_member->children.push_back(result_callee->children[0]);
		operator_member->children.push_back(CPPGMAstNodePtr(
			new CPPGMAstNode("identifier", "operator()")));
		result->children[0] = operator_member;
		result_callee = operator_member;
		InstantiateMemberCall(result, result_callee, "operator()", context, substitutions);
	}
	bool implicit_member_instantiated = false;
	string original_member;
	if(input_callee && input_callee->kind == "member-expression" &&
		input_callee->children.size() >= 2 && input_callee->children[1])
		original_member = input_callee->children[1]->value;
	ResolveMemberFunctionArguments(result, context, substitutions);
	if(result_callee && result_callee->kind == "member-expression")
		InstantiateMemberCall(result, result_callee, original_member, context, substitutions);
	ResolveMemberFunctionArguments(result, context, substitutions);
	if(result_callee && result_callee->kind == "id-expression" &&
		result_callee->value.find("::") == string::npos) {
		map<string, string>::const_iterator function_owner = function_owners_.find(context);
		bool member_function_context = function_owner != function_owners_.end() &&
			!function_owner->second.empty();
		if(!member_function_context) for(string current = context; !current.empty(); ) {
			const TemplateDefinition* current_definition = FindDefinition(current, context);
			if(class_contexts_.find(current) != class_contexts_.end() ||
				(current_definition && current_definition->class_template)) {
				member_function_context = true;
				break;
			}
			const size_t separator = current.rfind("::");
			if(separator == string::npos) current.clear();
			else current.erase(separator);
		}
		if(member_function_context) {
			CPPGMAstNodePtr synthetic_object(new CPPGMAstNode("keyword-literal",
				"KW_THIS:this"));
			CPPGMAstNodePtr synthetic_member(new CPPGMAstNode("member-expression", "."));
			synthetic_member->children.push_back(synthetic_object);
			synthetic_member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
				"identifier", result_callee->value)));
			if(InstantiateMemberCall(result, synthetic_member, result_callee->value,
				context, substitutions)) {
				result_callee->value = synthetic_member->children[1]->value;
				implicit_member_instantiated = true;
			}
		}
	}
	if(!constructor_replayed && !implicit_member_instantiated && result_callee &&
		result_callee->kind == "id-expression" &&
		result_callee->value.find('<') == string::npos) {
		const string callee_name = result_callee->value;
		vector<const TemplateDefinition*> definitions =
			FindFunctionDefinitions(callee_name, context);
		map<const TemplateDefinition*, string> inherited_owners;
		const string qualified_callee_owner = PrefixComponent(callee_name);
		if(!qualified_callee_owner.empty()) {
			vector<const TemplateDefinition*> inherited;
			set<string> active;
			CollectInheritedMemberTemplates(qualified_callee_owner,
				LastComponent(callee_name), substitutions, context, &inherited,
				&active, &inherited_owners);
			for(size_t inherited_index = 0; inherited_index < inherited.size(); ++inherited_index)
				if(find(definitions.begin(), definitions.end(), inherited[inherited_index]) ==
					definitions.end()) definitions.push_back(inherited[inherited_index]);
		}
		const bool inline_template_candidate = HasInlineTemplateCandidate(definitions, context);
		bool extern_template_candidate = false;
		for(size_t candidate = 0; candidate < definitions.size() && !extern_template_candidate;
			++candidate) {
			vector<string> inferred;
			if(!InferFunctionArguments(*definitions[candidate], result, &inferred,
				substitutions, context, 0)) continue;
			ostringstream request_key;
			request_key << definitions[candidate]->qualified_name << "@" <<
				definitions[candidate]->declaration.get();
			for(size_t argument = 0; argument < inferred.size(); ++argument)
				request_key << "|" << CanonicalSpelling(inferred[argument]);
			if(extern_instantiation_keys_.find(request_key.str()) !=
				extern_instantiation_keys_.end()) extern_template_candidate = true;
		}
		if(!HasMaterializedMemberFunction(callee_name, context) &&
			(!HasExactOrdinaryMatch(result, callee_name, substitutions, context) ||
				inline_template_candidate || extern_template_candidate))
			for(size_t candidate = 0; candidate < definitions.size(); ++candidate) {
				const TemplateDefinition* definition = definitions[candidate];
				if(definition->declaration && definition->declaration->kind == "simple-declaration") {
					bool has_definition = false;
					for(size_t other = 0; other < definitions.size(); ++other) {
						const TemplateDefinition* replacement = definitions[other];
						if(replacement == definition || !replacement->declaration ||
							replacement->declaration->kind != "function-definition") continue;
						if(MemberSignatureKey(*replacement) == MemberSignatureKey(*definition)) {
							has_definition = true;
							break;
						}
					}
					if(has_definition) continue;
				}
				vector<string> inferred;
				map<string, vector<string> > inferred_pack_values;
				map<string, FunctionSignature> inferred_function_values;
				const bool inferred_ok = InferFunctionArguments(*definition, result, &inferred,
					substitutions, context, 0, &inferred_pack_values,
					&inferred_function_values);
				if(!inferred_ok) continue;
				const TemplateDefinition* selected_definition =
					FindExplicitFunctionSpecialization(definition->qualified_name, inferred, context);
				if(!selected_definition) selected_definition = definition;
				string requested_owner_name = qualified_callee_owner;
				map<const TemplateDefinition*, string>::const_iterator inherited_owner =
					inherited_owners.find(selected_definition);
				if(inherited_owner != inherited_owners.end() && !inherited_owner->second.empty())
					requested_owner_name = inherited_owner->second;
				const bool concrete_member_owner = !requested_owner_name.empty() &&
					class_contexts_.find(requested_owner_name) != class_contexts_.end() &&
					specialization_bases_.find(LastComponent(requested_owner_name)) !=
					specialization_bases_.end() && !selected_definition->owner.empty();
				const string* requested_owner = concrete_member_owner ? &requested_owner_name : 0;
				const string local_name = Instantiate(*selected_definition, inferred, context, false,
					&inferred_pack_values, 0, requested_owner, &inferred_function_values);
				result->template_primary = definition->qualified_name;
				result->template_arguments = inferred;
				const string qualifier = concrete_member_owner ? requested_owner_name :
					GeneratedFunctionQualifier(*definition, callee_name, context);
				const string emitted_name = concrete_member_owner ?
					LastComponent(selected_definition->name) : local_name;
				result_callee->value = qualifier.empty() ? emitted_name : qualifier +
					"::" + emitted_name;
				break;
			}
		if(definitions.empty()) {
			const FunctionSignature* signature = FindFunctionSignature(callee_name, context);
			if(signature && callee_name.find("::") == string::npos &&
				class_contexts_.find(context) == class_contexts_.end() &&
				!HasReplayContext(substitutions)) {
				map<string, vector<string> >::const_iterator names =
					function_signatures_by_name_.find(LastComponent(callee_name));
				if(names != function_signatures_by_name_.end())
					for(size_t name = 0; name < names->second.size(); ++name) {
						const string& qualified = names->second[name];
						map<string, FunctionSignature>::const_iterator found =
							function_signatures_.find(qualified);
						if(found != function_signatures_.end() && &found->second == signature &&
							class_contexts_.find(PrefixComponent(qualified)) == class_contexts_.end() &&
							function_contexts_.find(PrefixComponent(qualified)) == function_contexts_.end()) {
							result->children[0]->value = qualified;
							break;
						}
					}
			}
			ResolveFunctionArguments(result, signature, context);
		}
	}
	if(!result->children.empty() && result->children[0] &&
		result->children[0]->kind == "id-expression") {
		string& callee = result->children[0]->value;
		const size_t separator = callee.find("::");
		if(separator != string::npos) {
			const string owner = callee.substr(0, separator);
			if(callee.compare(separator + 2, owner.size() + 2, owner + "::") == 0)
				callee.erase(separator + 2, owner.size() + 2);
		}
	}
	// Recompute a member-call result after replaying its concrete owner.  The
	// source AST may carry a dependent semantic spelling such as `const T&`
	// from the primary class template; retaining it would override the typed
	// return recovered from `node_value<key>::_M_v()` during later deduction.
	if(result_callee && result_callee->kind == "member-expression" &&
		result_callee->children.size() >= 2 && result_callee->children[1]) {
		string member_object_type;
		string member_result_type;
		set<string> member_active;
		if(InferArgument(result_callee->children[0], &member_object_type,
			substitutions, context) && FindClassMemberType(member_object_type,
			LastComponent(result_callee->children[1]->value), substitutions, context,
			&member_result_type, &member_active) && !member_result_type.empty())
			result->inferred_type = member_result_type;
	}
	return result;
}


} // namespace pa18_templates_internal
