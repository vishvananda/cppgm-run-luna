#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

static string StripOrdinaryCallConversionType(string raw)
{
	raw = CanonicalSpelling(raw);
	while(raw.compare(0, 6, "const ") == 0 ||
		raw.compare(0, 9, "volatile ") == 0) {
		const size_t space = raw.find(' ');
		if(space == string::npos) break;
		raw = CanonicalSpelling(raw.substr(space + 1));
	}
	while(raw.size() > 6 && raw.compare(raw.size() - 6, 6, " const") == 0)
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 6));
	while(raw.size() > 9 && raw.compare(raw.size() - 9, 9, " volatile") == 0)
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 9));
	while(raw.size() > 5 && raw.compare(raw.size() - 5, 5, "const") == 0 &&
		!IsIdentifierCharacter(raw[raw.size() - 6]))
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 5));
	while(raw.size() > 8 && raw.compare(raw.size() - 8, 8, "volatile") == 0 &&
		!IsIdentifierCharacter(raw[raw.size() - 9]))
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 8));
	while(!raw.empty() && (raw[raw.size() - 1] == '&' ||
		raw[raw.size() - 1] == '*'))
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 1));
	return raw;
}

static string OrdinaryConversionOperatorPattern(const TemplateDefinition& definition)
{
	return definition.conversion_operator ? definition.conversion_target : string();
}

static string OrdinaryConversionOwnerBase(const TemplateDefinition& definition)
{
	string owner = definition.owner;
	const size_t operator_position = owner.find("operator");
	if(operator_position != string::npos) owner.erase(operator_position);
	const size_t angle = owner.find('<');
	if(angle != string::npos) owner.erase(angle);
	while(owner.size() >= 2 && owner.compare(owner.size() - 2, 2, "::") == 0)
		owner.erase(owner.size() - 2);
	for(;;) {
		const string prefix = PrefixComponent(owner);
		if(prefix.empty() || LastComponent(prefix) != LastComponent(owner)) break;
		owner = prefix;
	}
	return owner;
}

void PA18TemplateExpander::IndexOrdinaryConversionDefinitions()
{
	conversion_operator_definitions_by_owner_.clear();
	for(map<string, TemplateDefinition>::const_iterator definition = definitions_.begin();
		definition != definitions_.end(); ++definition) {
		const TemplateDefinition& item = definition->second;
		if(!item.conversion_operator || !item.member_template || item.parameters.empty())
			continue;
		const string owner = OrdinaryConversionOwnerBase(item);
		if(!owner.empty()) conversion_operator_definitions_by_owner_[LastComponent(owner)].push_back(
			&item);
	}
}

bool PA18TemplateExpander::PreserveUnresolvedExplicitTemplateCall(
	const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& result,
	const vector<string>& explicit_arguments, const string& context,
	const map<string, string>& explicit_substitutions,
	const map<string, string>& substitutions)
{
	if(!input || input->children.empty() || !result) return false;
	for(size_t i = 0; i < explicit_arguments.size(); ++i)
		if(HasUnresolvedTemplateParameter(explicit_arguments[i], context,
			explicit_substitutions)) {
			result->children.push_back(CloneNode(input->children[0]));
			for(size_t child = 1; child < input->children.size(); ++child) {
				CPPGMAstNodePtr transformed = TransformNode(input->children[child],
					context, substitutions);
				if(transformed) result->children.push_back(transformed);
			}
			return true;
		}
	return false;
}

void PA18TemplateExpander::MaterializeOrdinaryCallConversions(
	const string& callee_name, const CPPGMAstNodePtr& result,
	const string& context, const map<string, string>& substitutions)
{
	if(!result || result->children.size() < 2 || !result->children[1]) return;
	const FunctionSignature* signature = FindFunctionSignature(callee_name, context);
	if(!signature || !signature->parameters) return;
	size_t argument = 0;
	for(size_t parameter = 0; parameter < signature->parameters->children.size() &&
		argument < result->children[1]->children.size(); ++parameter) {
		const CPPGMAstNodePtr parameter_node = signature->parameters->children[parameter];
		if(!parameter_node || parameter_node->kind != "parameter-declaration") continue;
		MaterializeOrdinaryConversion(ParameterTypeSpelling(parameter_node),
			result->children[1]->children[argument++], context, substitutions);
	}
}

void PA18TemplateExpander::MaterializeMemberCallConversions(
	const CPPGMAstNodePtr& result, const CPPGMAstNodePtr& callee,
	const string& context, const map<string, string>& substitutions)
{
	if(!result || result->children.size() < 2 || !callee ||
		callee->kind != "member-expression" || callee->children.size() < 2 ||
		!callee->children[0] || !callee->children[1] || !result->children[1] ||
		result->children[1]->kind != "argument-list") return;
	string object_type;
	if(!InferArgument(callee->children[0], &object_type, substitutions, context)) return;
	try {
		object_type = CanonicalSpelling(ResolveAlias(RewriteText(
			object_type, context, substitutions, 0), context));
	} catch(const PA18SubstitutionFailure&) {
		return;
	}
	while(object_type.compare(0, 6, "const ") == 0 ||
		object_type.compare(0, 9, "volatile ") == 0)
		object_type = CanonicalSpelling(object_type.substr(object_type.find(' ') + 1));
	if(callee->value == "->" && !object_type.empty() && object_type.back() == '*')
		object_type = CanonicalSpelling(object_type.substr(0, object_type.size() - 1));
while(!object_type.empty() && (object_type.back() == '&' || object_type.back() == '*'))
		object_type = CanonicalSpelling(object_type.substr(0, object_type.size() - 1));
	const CPPGMAstNodePtr declaration = FindClassDeclaration(object_type, context);
	if(!declaration) return;
	const string member_name = LastComponent(callee->children[1]->value);
	const vector<CPPGMAstNodePtr>& arguments = result->children[1]->children;
	for(size_t child = 0; child < declaration->children.size(); ++child) {
		CPPGMAstNodePtr candidate = declaration->children[child];
		while(candidate && candidate->kind == "template-declaration" &&
			candidate->children.size() > 1) candidate = candidate->children[1];
		if(!candidate || (candidate->kind != "function-definition" &&
			candidate->kind != "special-member-definition" &&
			candidate->kind != "simple-declaration")) continue;
		CPPGMAstNodePtr declarator = FunctionDeclarator(candidate);
		if(!declarator || LastComponent(FirstIdentifierLocal(declarator)) != member_name)
			continue;
		const CPPGMAstNodePtr parameters = DescendantOfKind(declarator,
			"parameter-clause");
		if(!parameters) continue;
		size_t argument = 0;
		for(size_t parameter = 0; parameter < parameters->children.size() &&
			argument < arguments.size(); ++parameter) {
			const CPPGMAstNodePtr& parameter_node = parameters->children[parameter];
			if(!parameter_node || parameter_node->kind != "parameter-declaration") continue;
			MaterializeOrdinaryConversion(ParameterTypeSpelling(parameter_node),
				arguments[argument++], context, substitutions);
		}
	}
}

void PA18TemplateExpander::MaterializeOrdinaryInitializerConversions(
	const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& result,
	const string& context, const map<string, string>& substitutions)
{
	if(!input || !result || input->kind != "simple-declaration") return;
	const CPPGMAstNodePtr original_list = ChildOfKindLocal(input,
		"init-declarator-list");
	const CPPGMAstNodePtr transformed_list = ChildOfKindLocal(result,
		"init-declarator-list");
	if(!original_list || !transformed_list || input->children.empty()) return;
	const string declaration_type = NodeTypeSpelling(input->children[0]);
	const size_t count = min(original_list->children.size(),
		transformed_list->children.size());
	for(size_t item = 0; item < count; ++item) {
		const CPPGMAstNodePtr original_item = original_list->children[item];
		const CPPGMAstNodePtr transformed_item = transformed_list->children[item];
		if(!original_item || !transformed_item || original_item->children.empty() ||
			transformed_item->children.size() < 2) continue;
		const CPPGMAstNodePtr original_declarator = original_item->children[0];
		CPPGMAstNodePtr initializer = transformed_item->children[1];
		if(!original_declarator || !initializer) continue;
		if(initializer->kind == "initializer") {
			if(initializer->children.size() != 1) continue;
			initializer = initializer->children[0];
		} else if(initializer->kind == "paren-initializer" ||
			initializer->kind == "braced-init-list") {
			if(initializer->children.size() != 1) continue;
			initializer = initializer->children[0];
		}
		if(!initializer || initializer->kind == "braced-init-list") continue;
		string target;
		try {
			target = CanonicalSpelling(ResolveAlias(RewriteText(
				DeclaratorTypeSpelling(declaration_type, original_declarator),
				context, substitutions, 0), context));
		} catch(const PA18SubstitutionFailure&) {
			continue;
		}
		// Constructor and class-initializer replay has its own path.  This
		// path supplies the missing ordinary copy-initialization fact for
		// destinations such as `int y = box_value`.
		if(target.empty() || FindClassDeclaration(target, context)) continue;
		MaterializeOrdinaryConversion(target, initializer, context, substitutions);
	}
}

bool PA18TemplateExpander::ResolveOrdinaryConversionTypes(
	const string& raw_parameter, const CPPGMAstNodePtr& argument,
	const string& context, const map<string, string>& substitutions,
	string* target_type, string* source_type, CPPGMAstNodePtr* source_declaration)
{
	if(!argument || !target_type || !source_type || !source_declaration) return false;
	try {
		*target_type = StripOrdinaryCallConversionType(RewriteText(raw_parameter,
			context, substitutions, 0));
	} catch(const PA18SubstitutionFailure&) { return false; }
	// A conversion-function template can target a fundamental or pointer type
	// just as it can target a class.  The destination is still a typed semantic
	// fact; requiring a class declaration here prevented `operator T()` from
	// being materialized for ordinary calls such as `f(box)` where `f` takes an
	// int.
	if(target_type->empty() || (!FindClassDeclaration(*target_type, context) &&
		!IsKnownTypeSpelling(*target_type, context))) return false;
	try {
		if(!InferArgument(argument, source_type, substitutions, context)) return false;
		*source_type = StripOrdinaryCallConversionType(ResolveAlias(RewriteText(
			*source_type, context, substitutions, 0), context));
		*source_declaration = FindClassDeclaration(*source_type, context);
		if(!*source_declaration)
			*source_type = StripOrdinaryCallConversionType(QualifyTypeArgument(*source_type, context));
		if(!*source_declaration)
			*source_declaration = FindClassDeclaration(*source_type, context);
		if(!*source_declaration) {
			map<string, string>::const_iterator base = specialization_bases_.find(
				LastComponent(*target_type));
			if(base != specialization_bases_.end()) {
				const string prefix = PrefixComponent(base->second);
				if(!prefix.empty() && source_type->find("::") == string::npos)
					*source_type = prefix + "::" + *source_type;
			}
			*source_declaration = FindClassDeclaration(*source_type, context);
		}
	} catch(const PA18SubstitutionFailure&) { return false; }
	return !source_type->empty() && *source_declaration && *source_type != *target_type;
}

bool PA18TemplateExpander::TryOrdinaryConversionDefinition(
	const TemplateDefinition& definition, const string& source_type,
	const string& target_type, const string& expected_pattern,
	const CPPGMAstNodePtr& source_declaration, const string& context,
	const map<string, string>& substitutions)
{
	const string member_name = LastComponent(definition.name);
	const string conversion_pattern = OrdinaryConversionOperatorPattern(definition);
	if(definition.class_template || definition.alias_template || definition.variable_template ||
		!definition.member_template || conversion_pattern.empty() || definition.parameters.empty()) return false;
	string owner = OrdinaryConversionOwnerBase(definition);
	string source_base = LastComponent(source_type);
	if(source_declaration && !source_declaration->template_primary.empty())
		source_base = LastComponent(source_declaration->template_primary);
	else {
		map<string, string>::const_iterator source_specialization =
			specialization_bases_.find(source_base);
		if(source_specialization != specialization_bases_.end())
			source_base = LastComponent(source_specialization->second);
	}
	if(LastComponent(owner) != source_base) return false;
	set<string> parameter_names;
	for(size_t i = 0; i < definition.parameters.size(); ++i)
		if(!definition.parameters[i].name.empty()) parameter_names.insert(definition.parameters[i].name);
	map<string, string> ignored;
	if(!MatchTypePattern(conversion_pattern, expected_pattern, parameter_names, &ignored, context)) return false;
	CPPGMAstNodePtr object(new CPPGMAstNode("id-expression"));
	object->inferred_type = source_type;
	CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "."));
	member->children.push_back(object);
	member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", member_name)));
	CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
	call->inferred_type = target_type;
	call->children.push_back(member);
	call->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("argument-list")));
	try {
		if(!InstantiateMemberCall(call, member, member_name, context, substitutions)) return false;
	} catch(const PA18SubstitutionFailure&) { return false; }
	if(!source_declaration) return true;
	map<string, vector<CPPGMAstNodePtr> >::const_iterator generated =
		generated_by_primary_.find(definition.qualified_name);
	if(generated == generated_by_primary_.end()) return true;
	CPPGMAstNodePtr selected;
	for(vector<CPPGMAstNodePtr>::const_reverse_iterator node = generated->second.rbegin();
		node != generated->second.rend(); ++node) {
		if(!*node) continue;
		selected = *node;
		break;
	}
	if(selected && find(source_declaration->children.begin(), source_declaration->children.end(),
		selected) == source_declaration->children.end()) source_declaration->children.push_back(selected);
	return true;
}

bool PA18TemplateExpander::ReplayOrdinaryConversion(
	const string& source_type, const string& target_type,
	const CPPGMAstNodePtr& source_declaration, const string& context,
	const map<string, string>& substitutions)
{
	if(!source_declaration) return false;
	string source_owner = source_type;
	if(!source_declaration->template_primary.empty()) source_owner =
		source_declaration->template_primary;
	else {
		map<string, string>::const_iterator source_specialization =
			specialization_bases_.find(LastComponent(source_type));
		if(source_specialization != specialization_bases_.end() &&
			(LastComponent(source_specialization->second) == LastComponent(source_type) ||
				PrefixComponent(source_specialization->second) == PrefixComponent(source_type)))
			source_owner = source_specialization->second;
	}
	const string source_base = LastComponent(source_owner);
	string expected_pattern = target_type;
	map<string, CPPGMAstNodePtr>::const_iterator target_class = class_declarations_.find(target_type);
	string target_owner;
	vector<string> target_arguments;
	if(target_class != class_declarations_.end() && target_class->second &&
		!target_class->second->template_primary.empty()) {
		target_owner = target_class->second->template_primary;
		target_arguments = target_class->second->template_arguments;
	} else {
		map<string, string>::const_iterator target_specialization =
			specialization_bases_.find(LastComponent(target_type));
		if(target_specialization != specialization_bases_.end() &&
			(LastComponent(target_specialization->second) == LastComponent(target_type) ||
				PrefixComponent(target_specialization->second) == PrefixComponent(target_type))) {
			target_owner = target_specialization->second;
			map<string, vector<string> >::const_iterator args = specialization_arguments_.find(
				target_specialization->first);
			if(args != specialization_arguments_.end()) target_arguments = args->second;
		}
	}
	if(!target_owner.empty() && !target_arguments.empty()) {
		expected_pattern = target_owner + "<";
		for(size_t i = 0; i < target_arguments.size(); ++i) {
			if(i) expected_pattern += ",";
			expected_pattern += target_arguments[i];
		}
		expected_pattern += ">";
	}
	map<string, vector<const TemplateDefinition*> >::const_iterator indexed =
		conversion_operator_definitions_by_owner_.find(source_base);
	if(indexed == conversion_operator_definitions_by_owner_.end()) return false;
	for(size_t candidate = 0; candidate < indexed->second.size(); ++candidate)
		if(indexed->second[candidate] && TryOrdinaryConversionDefinition(
			*indexed->second[candidate], source_type, target_type, expected_pattern,
			source_declaration, context, substitutions)) return true;
	return false;
}

void PA18TemplateExpander::MaterializeOrdinaryConversion(
	const string& raw_parameter, const CPPGMAstNodePtr& argument, const string& context,
	const map<string, string>& substitutions)
{
	string target_type, source_type;
	CPPGMAstNodePtr source_declaration;
	if(!ResolveOrdinaryConversionTypes(raw_parameter, argument, context, substitutions,
		&target_type, &source_type, &source_declaration)) return;
	if(ReplayOrdinaryConversion(source_type, target_type, source_declaration, context, substitutions)) return;
	string constructor_name = LastComponent(target_type);
	map<string, string>::const_iterator base = specialization_bases_.find(constructor_name);
	if(base != specialization_bases_.end()) constructor_name = LastComponent(base->second);
	CPPGMAstNodePtr object(new CPPGMAstNode("id-expression"));
	object->inferred_type = target_type;
	CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "."));
	member->children.push_back(object);
	member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", constructor_name)));
	CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
	call->children.push_back(member);
	CPPGMAstNodePtr arguments(new CPPGMAstNode("argument-list"));
	arguments->children.push_back(argument);
	call->children.push_back(arguments);
	try { (void)InstantiateMemberCall(call, member, constructor_name, context, substitutions); }
	catch(const PA18SubstitutionFailure&) {}
}

void PA18TemplateExpander::MaterializeReturnConversions(
	const CPPGMAstNodePtr& function, const CPPGMAstNodePtr& result,
	const string& context, const string& function_context,
	const map<string, string>& substitutions)
{
	if(!function || function->kind != "function-definition" || !result) return;
	const CPPGMAstNodePtr declarator = FunctionDeclarator(function);
	if(!declarator || function->children.empty()) return;
	string target_type;
	const CPPGMAstNodePtr trailing_return = ChildOfKindLocal(declarator,
		"trailing-return-type");
	if(trailing_return) target_type = TypeIdSpelling(
		ChildOfKindLocal(trailing_return, "type-id"));
	else target_type = NodeTypeSpelling(function->children[0]) +
		ReturnDeclaratorSuffix(declarator);
	try {
		target_type = CanonicalSpelling(ResolveAlias(RewriteText(target_type,
			context, substitutions, 0), context));
	} catch(const PA18SubstitutionFailure&) { return; }
	if(target_type.empty() || target_type == "void") return;
	const CPPGMAstNodePtr body = ChildOfKindLocal(result, "compound-statement");
	if(!body) return;
	std::function<void(const CPPGMAstNodePtr&)> visit = [&](const CPPGMAstNodePtr& node) {
		if(!node) return;
		if(node->kind == "return-statement" && !node->children.empty()) {
			const CPPGMAstNodePtr expression = node->children[0];
			string source_type;
			try {
				if(InferArgument(expression, &source_type, substitutions,
					function_context)) {
					source_type = CanonicalSpelling(ResolveAlias(RewriteText(
						source_type, function_context, substitutions, 0),
						function_context));
					while(source_type.compare(0, 6, "const ") == 0 ||
						source_type.compare(0, 9, "volatile ") == 0)
						source_type = CanonicalSpelling(source_type.substr(
							source_type.find(' ') + 1));
					while(!source_type.empty() && (source_type[source_type.size() - 1] == '&' ||
						source_type[source_type.size() - 1] == '*'))
						source_type = CanonicalSpelling(source_type.substr(0,
							source_type.size() - 1));
					const CPPGMAstNodePtr source_declaration = FindClassDeclaration(
						source_type, function_context);
					if(source_declaration && source_type != target_type)
						ReplayOrdinaryConversion(source_type, target_type,
							source_declaration, function_context, substitutions);
				}
			} catch(const PA18SubstitutionFailure&) {}
		}
		for(size_t child = 0; child < node->children.size(); ++child)
			visit(node->children[child]);
	};
	visit(body);
}

}
