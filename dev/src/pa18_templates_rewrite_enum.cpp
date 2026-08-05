#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;
namespace pa18_templates_internal {

bool PA18TemplateExpander::IsKnownEnumTypeFallback(const string& raw,
	const string& context) const
{
	const size_t separator = raw.rfind("::");
	if(separator != string::npos) {
		const string owner = raw.substr(0, separator);
		const string member = raw.substr(separator + 2);
		map<string, vector<string> >::const_iterator directives =
			using_namespace_directives_.find(owner);
		if(directives != using_namespace_directives_.end())
			for(size_t directive = 0; directive < directives->second.size(); ++directive)
				if(named_type_contexts_.find(JoinPath(directives->second[directive], member)) !=
					named_type_contexts_.end() || named_type_contexts_.find(JoinPath(
					 directives->second[directive], JoinPath(LastComponent(owner), member))) !=
					named_type_contexts_.end()) return true;
	}
	const string raw_class = LastComponent(PrefixComponent(raw));
	const string raw_member = LastComponent(raw);
	const string* unique_enum = 0;
	for(set<string>::const_iterator named = named_type_contexts_.begin();
		named != named_type_contexts_.end(); ++named)
		if(LastComponent(*named) == raw_member &&
			LastComponent(PrefixComponent(*named)) == raw_class) {
			if(unique_enum) return false;
			unique_enum = &*named;
		}
	(void)context;
	return unique_enum != 0;
}
}
