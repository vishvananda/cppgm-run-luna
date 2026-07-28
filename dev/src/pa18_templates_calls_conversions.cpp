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
	if(!definition.declaration) return string();
	string raw = definition.declaration->value;
	if(raw.empty()) {
		const CPPGMAstNodePtr identifier = DescendantOfKind(definition.declaration,
			"identifier");
		if(identifier) raw = identifier->value;
	}
	const size_t operator_position = raw.find("operator");
	if(operator_position == string::npos) return string();
	string suffix = raw.substr(operator_position + 8);
	while(!suffix.empty() && suffix[0] == ' ') suffix.erase(0, 1);
	if(suffix.empty() || string("+-*/%^&|=!<>~[],()").find(suffix[0]) != string::npos)
		return string();
	return CanonicalSpelling(suffix);
}

static string OrdinaryConversionOwnerBase(const TemplateDefinition& definition)
{
	string owner = definition.owner;
	const size_t operator_position = owner.find("operator");
	if(operator_position != string::npos) owner.erase(operator_position);
	while(owner.size() >= 2 && owner.compare(owner.size() - 2, 2, "::") == 0)
		owner.erase(owner.size() - 2);
	for(;;) {
		const string prefix = PrefixComponent(owner);
		if(prefix.empty() || LastComponent(prefix) != LastComponent(owner)) break;
		owner = prefix;
	}
	return owner;
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
	if(target_type->empty() || !FindClassDeclaration(*target_type, context)) return false;
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
	const string& target_type, const string& expected_pattern, const string& context,
	const map<string, string>& substitutions)
{
	const string member_name = LastComponent(definition.name);
	const string conversion_pattern = OrdinaryConversionOperatorPattern(definition);
	if(definition.class_template || definition.alias_template || definition.variable_template ||
		!definition.member_template || conversion_pattern.empty() || definition.parameters.empty()) return false;
	string owner = OrdinaryConversionOwnerBase(definition);
	const size_t angle = owner.find('<');
	if(angle != string::npos) owner.erase(angle);
	string source_base = LastComponent(source_type);
	map<string, CPPGMAstNodePtr>::const_iterator source_class =
		class_declarations_.find(source_type);
	if(source_class != class_declarations_.end() && source_class->second &&
		!source_class->second->template_primary.empty())
		source_base = LastComponent(source_class->second->template_primary);
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
	map<string, CPPGMAstNodePtr>::iterator declaration = class_declarations_.find(source_type);
	if(declaration == class_declarations_.end())
		for(map<string, CPPGMAstNodePtr>::iterator it = class_declarations_.begin();
			it != class_declarations_.end(); ++it)
			if(LastComponent(it->first) == LastComponent(source_type)) { declaration = it; break; }
	map<string, vector<CPPGMAstNodePtr> >::iterator generated = generated_by_owner_.find(source_type);
	if(generated == generated_by_owner_.end()) generated = generated_by_owner_.find(LastComponent(source_type));
	if(declaration != class_declarations_.end() && generated != generated_by_owner_.end() &&
		!generated->second.empty()) {
		const CPPGMAstNodePtr node = generated->second.back();
		if(node && find(declaration->second->children.begin(), declaration->second->children.end(), node) ==
			declaration->second->children.end()) declaration->second->children.push_back(node);
	}
	if(declaration != class_declarations_.end())
		for(map<string, vector<CPPGMAstNodePtr> >::const_iterator owner_it = generated_by_owner_.begin();
			owner_it != generated_by_owner_.end(); ++owner_it)
			for(size_t i = 0; i < owner_it->second.size(); ++i) {
				const CPPGMAstNodePtr& node = owner_it->second[i];
				if(!node || (node->kind != "special-member-definition" &&
					node->kind != "special-member-declaration") ||
					node->value.find("operator") == string::npos ||
					find(declaration->second->children.begin(), declaration->second->children.end(), node) !=
					declaration->second->children.end()) continue;
				declaration->second->children.push_back(node);
				generated_by_owner_[source_type].push_back(node);
			}
	return true;
}

bool PA18TemplateExpander::ReplayOrdinaryConversion(
	const string& source_type, const string& target_type,
	const CPPGMAstNodePtr& source_declaration, const string& context,
	const map<string, string>& substitutions)
{
	if(!source_declaration) return false;
	string source_owner = source_type;
	map<string, CPPGMAstNodePtr>::const_iterator source_class = class_declarations_.find(source_type);
	if(source_class != class_declarations_.end() && source_class->second &&
		!source_class->second->template_primary.empty()) source_owner = source_class->second->template_primary;
	else for(map<string, string>::const_iterator it = specialization_bases_.begin();
		it != specialization_bases_.end(); ++it)
		if(it->first == LastComponent(source_type) &&
			(LastComponent(it->second) == LastComponent(source_type) ||
			 PrefixComponent(it->second) == PrefixComponent(source_type))) {
			source_owner = it->second;
			break;
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
	} else for(map<string, string>::const_iterator it = specialization_bases_.begin();
		it != specialization_bases_.end(); ++it)
		if(it->first == LastComponent(target_type) &&
			(LastComponent(it->second) == LastComponent(target_type) ||
			 PrefixComponent(it->second) == PrefixComponent(target_type))) {
			target_owner = it->second;
			map<string, vector<string> >::const_iterator args = specialization_arguments_.find(it->first);
			if(args != specialization_arguments_.end()) target_arguments = args->second;
			break;
		}
	if(!target_owner.empty() && !target_arguments.empty()) {
		expected_pattern = target_owner + "<";
		for(size_t i = 0; i < target_arguments.size(); ++i) {
			if(i) expected_pattern += ",";
			expected_pattern += target_arguments[i];
		}
		expected_pattern += ">";
	}
	for(map<string, TemplateDefinition>::const_iterator it = definitions_.begin();
		it != definitions_.end(); ++it) {
		const string owner = OrdinaryConversionOwnerBase(it->second);
		if(LastComponent(owner) == source_base &&
			TryOrdinaryConversionDefinition(it->second, source_type, target_type,
				expected_pattern, context, substitutions)) return true;
	}
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
