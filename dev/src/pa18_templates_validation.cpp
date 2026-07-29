#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::ValidationValueArgument(const string& raw,
	const map<string, bool>& parameters) const
{
	string spelling = CanonicalSpelling(raw);
	if(spelling.size() >= 3 &&
		spelling.compare(spelling.size() - 3, 3, "...") == 0)
		spelling = CanonicalSpelling(spelling.substr(0, spelling.size() - 3));
	while(spelling.size() >= 2 && spelling[0] == '(' &&
		spelling[spelling.size() - 1] == ')')
		spelling = CanonicalSpelling(spelling.substr(1, spelling.size() - 2));
	if(spelling.empty() || ValidationTypeArgument(spelling, parameters)) return false;
	map<string, bool>::const_iterator parameter = parameters.find(spelling);
	if(parameter != parameters.end()) return !parameter->second;
	if(spelling == "true" || spelling == "false" || spelling == "nullptr") return true;
	if(isdigit(static_cast<unsigned char>(spelling[0])) || spelling[0] == '\'' ||
		spelling[0] == '"') return true;
	if(spelling.compare(0, 7, "sizeof(") == 0 ||
		spelling.compare(0, 8, "alignof(") == 0 ||
		spelling.compare(0, 12, "static_cast<") == 0) return true;
	// A qualified member named `value` (and the conventional size/length
	// constants) denotes an expression in a template argument.  The suffix
	// check deliberately leaves `T::type` and other dependent type-ids alone.
	const size_t separator = spelling.rfind("::");
	if(separator != string::npos) {
		const string member = spelling.substr(separator + 2);
		if(member == "value" || member == "value_v" || member == "size" ||
			member == "length" || member == "count") return true;
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
				if(!expected.type && ValidationTypeArgument(arguments[argument], parameters))
					throw logic_error("type used as non-type template argument");
				if(expected.type && ValidationValueArgument(arguments[argument], parameters))
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
		const string raw = RemoveMarker(node->value);
		const size_t open = raw.find('<');
		if(open != string::npos) {
			string base, argument_text;
			size_t begin = 0, close = string::npos;
			if(TemplateBase(raw, open, &begin, &base) &&
				TemplateRange(raw, open, &argument_text, &close)) {
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
						if(!expected.type && ValidationTypeArgument(arguments[argument], parameters))
							throw logic_error("type used as non-type template argument");
						if(expected.type && ValidationValueArgument(arguments[argument], parameters))
							throw logic_error("value used as type template argument");
						if(!expected.pack) ++parameter;
					}
					ValidateTemplateArgumentSpelling(raw, context, parameters);
				}
			}
		}
	}
	for(size_t i = 0; i < node->children.size(); ++i)
		ValidateTemplateArgumentKinds(node->children[i], context, parameters);
}

} // namespace pa18_templates_internal
