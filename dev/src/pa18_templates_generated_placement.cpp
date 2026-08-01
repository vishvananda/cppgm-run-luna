#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::HasKnownAnonymousClassPath(const string& value) const
{
	map<string, vector<string> >::const_iterator indexed = class_paths_by_name_.find(value);
	if(indexed == class_paths_by_name_.end()) return false;
	for(size_t path = 0; path < indexed->second.size(); ++path)
		if(indexed->second[path].find("<unnamed>") != string::npos &&
			(class_contexts_.find(indexed->second[path]) != class_contexts_.end() ||
			 named_type_contexts_.find(indexed->second[path]) != named_type_contexts_.end())) return true;
	return false;
}

void PA18TemplateExpander::RehomeAnonymousGeneratedLayout(
	vector<CPPGMAstNodePtr>* children)
{
	if(!children) return;
	map<string, vector<CPPGMAstNodePtr> >::iterator found =
		generated_by_owner_.find(string());
	if(found == generated_by_owner_.end() || found->second.empty()) return;
	// Keep a generated class that stores an anonymous-namespace type by value in
	// that namespace's lexical insertion stream.  A root-level forward would
	// otherwise bind the use to an incomplete competing shell.
	set<string> anonymous_layout_names;
	const auto collect_anonymous_layout_names =
		[&](const CPPGMAstNodePtr& namespace_node) {
			if(!namespace_node || namespace_node->kind != "namespace-definition" ||
				namespace_node->value != "<unnamed>") return;
			for(size_t child = 0; child < namespace_node->children.size(); ++child) {
				const CPPGMAstNodePtr& dependency = namespace_node->children[child];
				if(!dependency || dependency->kind != "class-specifier" ||
					dependency->children.size() <= 1) continue;
				vector<CPPGMAstNodePtr> dependency_node(1, dependency);
				for(size_t generated = 0; generated < found->second.size(); ++generated) {
					const CPPGMAstNodePtr& candidate = found->second[generated];
					if(candidate && candidate->kind == "class-specifier" &&
						MentionsGeneratedLayoutClass(candidate, dependency_node))
						anonymous_layout_names.insert(candidate->value);
				}
			}
		};
	for(size_t child = 0; child < children->size(); ++child)
		collect_anonymous_layout_names((*children)[child]);
	if(anonymous_layout_names.empty()) return;
	vector<CPPGMAstNodePtr> retained;
	vector<CPPGMAstNodePtr>& anonymous_generated = generated_by_owner_["<unnamed>"];
	for(size_t generated = 0; generated < found->second.size(); ++generated) {
		const CPPGMAstNodePtr& candidate = found->second[generated];
		const bool class_candidate = candidate &&
			(candidate->kind == "class-specifier" ||
				candidate->kind == "class-forward-declaration");
		if(class_candidate && anonymous_layout_names.find(candidate->value) !=
			anonymous_layout_names.end()) anonymous_generated.push_back(candidate);
		else retained.push_back(candidate);
	}
	found->second.swap(retained);
}

} // namespace pa18_templates_internal
