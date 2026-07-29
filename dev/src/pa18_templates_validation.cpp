#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

namespace {

string ValidationArgumentSpelling(const string& raw)
{
	string spelling = CanonicalSpelling(raw);
	if(spelling.size() >= 3 &&
		spelling.compare(spelling.size() - 3, 3, "...") == 0)
		spelling = CanonicalSpelling(spelling.substr(0, spelling.size() - 3));
	while(spelling.size() >= 2 && spelling[0] == '(' &&
		spelling[spelling.size() - 1] == ')')
		spelling = CanonicalSpelling(spelling.substr(1, spelling.size() - 2));
	return spelling;
}

} // namespace

bool PA18TemplateExpander::ValidationKnownTypeMember(const string& raw_owner,
	const string& member, const string& context) const
{
	if(member.empty()) return false;
	string owner_spelling = ValidationArgumentSpelling(raw_owner);
	const size_t open = owner_spelling.find('<');
	if(open != string::npos) owner_spelling = owner_spelling.substr(0, open);
	owner_spelling = CanonicalSpelling(owner_spelling);
	if(owner_spelling.empty()) return false;
	const string qualified_member = JoinPath(owner_spelling, member);
	if(type_aliases_.find(qualified_member) != type_aliases_.end() ||
		class_declarations_.find(qualified_member) != class_declarations_.end() ||
		named_type_contexts_.find(qualified_member) != named_type_contexts_.end()) return true;
	const TemplateDefinition* definition = FindDefinition(owner_spelling, context);
	if(!definition) return false;
	const string definition_member = JoinPath(definition->qualified_name, member);
	return type_aliases_.find(definition_member) != type_aliases_.end() ||
		class_declarations_.find(definition_member) != class_declarations_.end() ||
		named_type_contexts_.find(definition_member) != named_type_contexts_.end();
}

bool PA18TemplateExpander::ValidationTypeArgument(const string& raw,
	const string& context, const map<string, bool>& parameters) const
{
	string spelling = CanonicalSpelling(RemoveMarker(raw));
	if(spelling.size() >= 3 &&
		spelling.compare(spelling.size() - 3, 3, "...") == 0)
		spelling = CanonicalSpelling(spelling.substr(0, spelling.size() - 3));
	while(spelling.compare(0, 8, "typename") == 0 &&
		(spelling.size() == 8 || isspace(static_cast<unsigned char>(spelling[8]))))
		spelling = CanonicalSpelling(spelling.substr(8));
	map<string, bool>::const_iterator parameter = parameters.find(spelling);
	if(parameter != parameters.end() && parameter->second) return true;
	if(spelling.compare(0, 8, "typename") == 0 &&
		(spelling.size() == 8 || isspace(static_cast<unsigned char>(spelling[8]))))
		return true;
	if(spelling.compare(0, 7, "decltype") == 0 &&
		spelling.size() > 8 && spelling[7] == '(') return true;
	const size_t separator = TopLevelScopeSeparator(spelling);
	if(separator != string::npos && ValidationKnownTypeMember(
		spelling.substr(0, separator), spelling.substr(separator + 2), context)) return true;
	return false;
}

bool PA18TemplateExpander::ValidationValueArgument(const string& raw,
	const string& context, const map<string, bool>& parameters) const
{
	const string spelling = ValidationArgumentSpelling(raw);
	if(spelling.empty() || ValidationTypeArgument(spelling, context, parameters)) return false;
	map<string, bool>::const_iterator parameter = parameters.find(spelling);
	if(parameter != parameters.end()) return !parameter->second;
	if(spelling == "true" || spelling == "false" || spelling == "nullptr") return true;
	if(isdigit(static_cast<unsigned char>(spelling[0])) || spelling[0] == '\'' ||
		spelling[0] == '"') return true;
	if(spelling.compare(0, 7, "sizeof(") == 0 ||
		spelling.compare(0, 8, "alignof(") == 0 ||
		spelling.compare(0, 12, "static_cast<") == 0 ||
		spelling.compare(0, 11, "const_cast<") == 0 ||
		spelling.compare(0, 14, "reinterpret_cast<") == 0 ||
		spelling.compare(0, 13, "dynamic_cast<") == 0) return true;
	const size_t separator = TopLevelScopeSeparator(spelling);
	if(separator != string::npos) {
		const string owner = spelling.substr(0, separator);
		const string member = spelling.substr(separator + 2);
		const size_t owner_open = owner.find('<');
		const string owner_base = owner_open == string::npos ? owner :
			owner.substr(0, owner_open);
		map<string, set<string> >::const_iterator indexed =
			static_members_by_class_.find(CanonicalSpelling(owner));
		if(indexed != static_members_by_class_.end() &&
			indexed->second.find(member) != indexed->second.end()) return true;
		const TemplateDefinition* definition = FindDefinition(owner_base, context);
		if(definition && definition->static_members.find(member) !=
			definition->static_members.end()) return true;
	}
	return false;
}

void PA18TemplateExpander::ValidateTemplateArgumentSpelling(
	const string& raw, const string& context,
	const map<string, bool>& parameters) const
{
	for(size_t position = 0; position < raw.size(); ++position) {
		if(raw[position] != '<') continue;
		string base, argument_text;
		size_t begin = 0, close = string::npos;
		if(!TemplateBase(raw, position, &begin, &base) ||
			!TemplateRange(raw, position, &argument_text, &close)) continue;
		const TemplateDefinition* definition = FindDefinition(base, context);
		if(definition) {
			const vector<string> arguments = SplitTemplateArguments(argument_text);
			size_t required = definition->parameters.size();
			while(required > 0 && (definition->parameters[required - 1].pack ||
				!definition->parameters[required - 1].default_type.empty())) --required;
			size_t parameter = 0;
			for(size_t argument = 0; arguments.size() >= required &&
				argument < arguments.size(); ++argument) {
				while(parameter < definition->parameters.size() &&
					!definition->parameters[parameter].pack && argument > parameter) ++parameter;
				if(parameter >= definition->parameters.size()) break;
				const TemplateParameter& expected = definition->parameters[parameter];
				if(!expected.type && ValidationTypeArgument(arguments[argument], context, parameters))
					throw logic_error("type used as non-type template argument");
				if(expected.type && ValidationValueArgument(arguments[argument], context, parameters))
					throw logic_error("value used as type template argument");
				if(!expected.pack) ++parameter;
			}
			for(size_t argument = 0; argument < arguments.size(); ++argument)
				ValidateTemplateArgumentSpelling(arguments[argument], context, parameters);
		}
		position = close;
	}
}

void PA18TemplateExpander::ValidateTemplateArgumentKinds(
	const CPPGMAstNodePtr& node, const string& inherited_context,
	const map<string, bool>& inherited_parameters) const
{
	if(!node) return;
	string context = inherited_context;
	map<const CPPGMAstNode*, string>::const_iterator lexical = lexical_contexts_.find(node.get());
	if(lexical != lexical_contexts_.end()) context = lexical->second;
	map<string, bool> parameters = inherited_parameters;
	if(node->kind == "template-declaration" && node->children.size() > 1) {
		const vector<TemplateParameter> own = Parameters(node->children[0]);
		for(size_t i = 0; i < own.size(); ++i) parameters[own[i].name] = own[i].type;
		ValidateTemplateArgumentKinds(node->children[1], context, parameters);
		return;
	}
	if(node->kind == "type-name" || node->kind == "decl-specifier" ||
		node->kind == "type-specifier") {
		ValidateTemplateArgumentSpelling(RemoveMarker(node->value), context, parameters);
	}
	for(size_t i = 0; i < node->children.size(); ++i)
		ValidateTemplateArgumentKinds(node->children[i], context, parameters);
}

} // namespace pa18_templates_internal
