#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::RewriteGeneratedOwnerNestedMember(string* raw, size_t begin,
	size_t close, const string& base, const string& context,
	const map<string, string>& substitutions, bool* template_replaced, size_t* search)
{
	// The owner may already be a generated specialization by the time a
	// second replay sees its nested template-id.  Resolve that typed owner
	// directly instead of letting the short `impl` name inherit the active
	// caller's nested specialization.
	const size_t generated_owner_separator = base.rfind("::");
	if(generated_owner_separator != string::npos && base.find('<') == string::npos) {
		const string generated_owner = base.substr(0, generated_owner_separator);
		const string generated_nested_name = base.substr(generated_owner_separator + 2);
		map<string, string>::const_iterator generated_base = specialization_bases_.find(
			LastComponent(generated_owner));
		map<string, vector<string> >::const_iterator generated_arguments =
			specialization_arguments_.find(LastComponent(generated_owner));
		if(generated_base != specialization_bases_.end() &&
			generated_arguments != specialization_arguments_.end() &&
			(class_contexts_.find(generated_owner) != class_contexts_.end() ||
			 class_declarations_.find(generated_owner) != class_declarations_.end())) {
			string owner_source = generated_base->second;
			const size_t owner_source_open = owner_source.find('<');
			if(owner_source_open != string::npos) owner_source.erase(owner_source_open);
			const TemplateDefinition* owner_definition = FindDefinition(owner_source, context);
			if(!owner_definition) owner_definition = FindDefinition(LastComponent(owner_source), context);
			if(owner_definition && owner_definition->class_template) {
				const TemplateDefinition* selected_owner = SelectClassTemplateDefinition(
					owner_definition, generated_arguments->second, context);
				const TemplateDefinition* nested_definition = selected_owner ?
					FindNestedDefinition(*selected_owner, generated_nested_name) : 0;
				const size_t nested_open = raw->find('<', begin);
				string nested_arguments_text;
				size_t nested_close = string::npos;
				if(selected_owner && nested_definition && nested_open != string::npos &&
					TemplateRange(*raw, nested_open, &nested_arguments_text, &nested_close)) {
					map<string, string> nested_substitutions = substitutions;
					AddConcreteOwnerSubstitutions(generated_owner, context,
						&nested_substitutions);
					for(size_t parameter = 0; parameter < selected_owner->parameters.size() &&
						parameter < generated_arguments->second.size(); ++parameter)
						if(!selected_owner->parameters[parameter].name.empty())
							nested_substitutions[selected_owner->parameters[parameter].name] =
								generated_arguments->second[parameter];
					nested_substitutions[selected_owner->name] = generated_owner;
					vector<string> nested_arguments = SplitTemplateArguments(
						nested_arguments_text);
					for(size_t argument = 0; argument < nested_arguments.size(); ++argument) {
						nested_arguments[argument] = NormalizeTypeArgument(RewriteText(
							nested_arguments[argument], context, nested_substitutions, 0,
							false, false));
						nested_arguments[argument] = NormalizeTypeArgument(ReplaceIdentifiers(
							nested_arguments[argument], nested_substitutions));
					}
					bool unresolved = false;
					for(size_t argument = 0; argument < nested_arguments.size(); ++argument)
						if(HasUnresolvedTemplateParameter(nested_arguments[argument], context,
							nested_substitutions)) unresolved = true;
					if(!unresolved) {
						const TemplateDefinition* selected_nested = SelectClassTemplateDefinition(
							nested_definition, nested_arguments, context);
						if(selected_nested) {
							const string nested_local_name = Instantiate(*selected_nested,
								nested_arguments, context, false, 0, &nested_substitutions,
								&generated_owner);
							if(!nested_local_name.empty()) {
								const string concrete_nested = JoinPath(generated_owner,
									nested_local_name);
								raw->replace(begin, nested_close - begin + 1,
									concrete_nested);
								if(template_replaced) *template_replaced = true;
								if(search) *search = begin + concrete_nested.size();
								return true;
							}
						}
					}
				}
			}
		}
	}
	return false;
}

} // namespace pa18_templates_internal
