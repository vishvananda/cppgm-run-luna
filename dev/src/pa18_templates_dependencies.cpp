#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;
using namespace pa18_templates_internal;

bool PA18TemplateExpander::HasExternalCompleteDependency(
	const CPPGMAstNodePtr& node, const string& owner, set<string>* dependencies) const
{
	if(!node || !dependencies) return false;
	for(map<string, CPPGMAstNodePtr>::const_iterator declaration =
		class_declarations_.begin(); declaration != class_declarations_.end(); ++declaration) {
		const CPPGMAstNodePtr& source = declaration->second;
		if(!source || source->kind != "class-specifier" || source->children.size() <= 1)
			continue;
		const string qualified = declaration->first;
		map<string, TemplateDefinition>::const_iterator source_template =
			definitions_.find(qualified);
		if(source_template != definitions_.end() && source_template->second.class_template)
			continue;
		if(!PrefixComponent(qualified).empty() &&
			PrefixComponent(qualified) == owner) continue;
		if(specialization_bases_.find(LastComponent(qualified)) != specialization_bases_.end()) continue;
		const string name = LastComponent(qualified);
		if(!name.empty() && ContainsName(node, name)) dependencies->insert(name);
	}
	// A generated specialization can acquire a promoted function-local class
	// spelling only while its rewritten declaration is being assembled.  The
	// source class is still recorded under its lexical function scope, so the
	// ordinary declaration-name scan above cannot see the enclosing owner.  Tie
	// that typed local-class fact back to its complete owning class and defer
	// the generated specialization until that owner has been analyzed.
	for(map<string, string>::const_iterator local = local_class_names_.begin();
		local != local_class_names_.end(); ++local) {
		const string lexical_name = LastComponent(local->first);
		const string promoted_name = LastComponent(local->second);
		if((lexical_name.empty() || !ContainsName(node, lexical_name)) &&
			(promoted_name.empty() || !ContainsName(node, promoted_name))) continue;
		const string promoted_owner = PrefixComponent(local->second);
		if(!promoted_owner.empty()) dependencies->insert(LastComponent(promoted_owner));
	}
	const map<string, vector<string> >::const_iterator generated_arguments =
		specialization_arguments_.find(LastComponent(node->value));
	if(generated_arguments != specialization_arguments_.end())
		for(size_t argument = 0; argument < generated_arguments->second.size(); ++argument)
			for(map<string, string>::const_iterator local = local_class_names_.begin();
				local != local_class_names_.end(); ++local) {
				const string lexical_name = LastComponent(local->first);
				const string promoted_name = LastComponent(local->second);
				const string value = CanonicalSpelling(generated_arguments->second[argument]);
				if((!lexical_name.empty() && (value == lexical_name ||
					value.find(lexical_name + "::") != string::npos)) ||
					(!promoted_name.empty() && (value == promoted_name ||
					value.find(promoted_name + "::") != string::npos))) {
					const string promoted_owner = PrefixComponent(local->second);
					if(!promoted_owner.empty()) dependencies->insert(LastComponent(promoted_owner));
				}
			}
	const map<string, vector<CPPGMAstNodePtr> >::const_iterator nested_generated =
		generated_by_owner_.find(LastComponent(node->value));
	if(nested_generated != generated_by_owner_.end())
		for(size_t generated = 0; generated < nested_generated->second.size(); ++generated)
			for(map<string, string>::const_iterator local = local_class_names_.begin();
				local != local_class_names_.end(); ++local) {
				const string lexical_name = LastComponent(local->first);
				const string promoted_name = LastComponent(local->second);
				if((lexical_name.empty() || !ContainsName(nested_generated->second[generated], lexical_name)) &&
					(promoted_name.empty() || !ContainsName(nested_generated->second[generated], promoted_name))) continue;
				const string promoted_owner = PrefixComponent(local->second);
				if(!promoted_owner.empty()) dependencies->insert(LastComponent(promoted_owner));
			}
	return !dependencies->empty();
}
