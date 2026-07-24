#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

bool ContainsIdentifierToken(const string& raw, const string& name)
{
	for(size_t position = 0; position < raw.size();) {
		if(!IsIdentifierCharacter(raw[position])) {
			++position;
			continue;
		}
		const size_t begin = position++;
		while(position < raw.size() && IsIdentifierCharacter(raw[position])) ++position;
		if(raw.substr(begin, position - begin) == name) return true;
	}
	return false;
}

bool ContainsIdentifierNode(const CPPGMAstNodePtr& node, const string& name)
{
	if(!node || name.empty()) return false;
	if((node->kind == "identifier" || node->kind == "id-expression") &&
		ContainsIdentifierToken(RemoveMarker(node->value), name)) return true;
	for(size_t argument = 0; argument < node->template_arguments.size(); ++argument)
		if(CanonicalSpelling(RemoveMarker(node->template_arguments[argument])) == name)
			return true;
	for(size_t child = 0; child < node->children.size(); ++child)
		if(ContainsIdentifierNode(node->children[child], name)) return true;
	return false;
}

void PA18TemplateExpander::RecordTemplateArrayValues(
	const TemplateDefinition& definition, const vector<string>& arguments,
	const string& context, const map<string, string>& substitutions,
	const map<string, vector<string> >& pack_substitutions)
{
	if(!definition.declaration) return;
	map<string, vector<string> > concrete_packs = pack_substitutions;
	vector<size_t> trailing_fixed(definition.parameters.size() + 1, 0);
	for(size_t parameter = definition.parameters.size(); parameter > 0; --parameter) {
		const size_t index = parameter - 1;
		trailing_fixed[index] = trailing_fixed[index + 1] +
			(definition.parameters[index].pack ? 0 : 1);
	}
	size_t argument_index = 0;
	for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter) {
		const TemplateParameter& template_parameter = definition.parameters[parameter];
		if(template_parameter.pack) {
			const size_t available = arguments.size() > argument_index ?
				arguments.size() - argument_index : 0;
			const size_t count = available > trailing_fixed[parameter + 1] ?
				available - trailing_fixed[parameter + 1] : 0;
			if(!template_parameter.name.empty() && concrete_packs.find(
				template_parameter.name) == concrete_packs.end())
				concrete_packs[template_parameter.name] = vector<string>(
					arguments.begin() + argument_index,
					arguments.begin() + argument_index + count);
			argument_index += count;
		} else if(argument_index < arguments.size()) ++argument_index;
	}
	for(size_t child_index = 0; child_index < definition.declaration->children.size(); ++child_index) {
		const CPPGMAstNodePtr child = definition.declaration->children[child_index];
		if(!child || child->kind != "simple-declaration" || child->children.empty() ||
			!HasDeclarationSpecifier(child->children[0], "constexpr")) continue;
		const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
		if(!list) continue;
		for(size_t item_index = 0; item_index < list->children.size(); ++item_index) {
			const CPPGMAstNodePtr item = list->children[item_index];
			if(!item || item->children.size() < 2 || !item->children[0]) continue;
			if(DeclaratorArraySuffix(item->children[0]).empty()) continue;
			const string name = LastComponent(FirstIdentifierLocal(item->children[0]));
			CPPGMAstNodePtr initializer = item->children[1];
			if(initializer->kind == "initializer" && initializer->children.size() == 1)
				initializer = initializer->children[0];
			if(name.empty() || !initializer || initializer->kind != "braced-init-list") continue;
			vector<PA19IntegralValue> values;
			for(size_t value_index = 0; value_index < initializer->children.size(); ++value_index) {
				const CPPGMAstNodePtr element = initializer->children[value_index];
				CPPGMAstNodePtr expression = element;
				string pack_name;
				if(element && element->kind == "pack-expansion-expression" &&
					!element->children.empty()) {
					pack_name = PackExpansionIdentifier(element->children[0]);

					if(pack_name.empty()) for(size_t parameter = 0;
						parameter < definition.parameters.size(); ++parameter) {
						const TemplateParameter& template_parameter = definition.parameters[parameter];
						if(!template_parameter.pack || template_parameter.name.empty() ||
							!ContainsIdentifierNode(element->children[0], template_parameter.name)) continue;
						pack_name = template_parameter.name;
						break;
					}
				}
				if(!pack_name.empty()) {
					map<string, vector<string> >::const_iterator pack_values =
						concrete_packs.find(pack_name);
					if(pack_values == concrete_packs.end()) {
						values.clear();
						break;
					}
					for(size_t argument = 0; argument < pack_values->second.size(); ++argument) {
						map<string, string> one = substitutions;
						one[pack_name] = pack_values->second[argument];
						string text = ConstantExpressionSpelling(expression->children.empty() ?
							CPPGMAstNodePtr() : expression->children[0]);
						text = RewriteText(text, context, one, 0);
						PA19IntegralValue value;
						if(!EvaluateIntegralText(text, context, one, &value)) {
							values.clear();
							break;
						}
						values.push_back(value);
					}
				} else {
					const string text = ConstantExpressionSpelling(expression);
					PA19IntegralValue value;
					if(!EvaluateIntegralText(text, context, substitutions, &value)) {
						values.clear();
						break;
					}
					values.push_back(value);
				}
			}
			if(values.empty() && !initializer->children.empty()) continue;
			constant_arrays_[name] = values;
			constant_arrays_[JoinPath(context, name)] = values;
		}
	}
}

} // namespace pa18_templates_internal
