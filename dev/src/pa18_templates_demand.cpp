#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {
namespace {

bool MentionsGeneratedFunction(const CPPGMAstNodePtr& node, const string& name)
{
	if(!node || name.empty()) return false;
	if(node->template_instantiation &&
		(node->kind == "id-expression" || node->kind == "identifier" ||
		 node->kind == "function-name" || node->kind == "declarator")) {
		string spelling = LastComponent(RemoveMarker(node->value));
		const size_t angle = spelling.find('<');
		if(angle != string::npos) spelling.erase(angle);
		if(spelling == name) return true;
	}
	for(size_t i = 0; i < node->children.size(); ++i)
		if(MentionsGeneratedFunction(node->children[i], name)) return true;
	return false;
}

}

void PA18TemplateExpander::InjectLateRootGenerated(const CPPGMAstNodePtr& node)
{
	map<string, vector<CPPGMAstNodePtr> >::iterator late_root =
		generated_by_owner_.find(string());
	if(late_root == generated_by_owner_.end()) return;
	map<string, CPPGMAstNodePtr> late_functions;
	for(size_t generated = 0; generated < late_root->second.size(); ++generated) {
		const CPPGMAstNodePtr& candidate = late_root->second[generated];
		if(!candidate || (candidate->kind != "function-definition" &&
			candidate->kind != "special-member-definition" &&
			candidate->kind != "special-member-declaration" &&
			candidate->kind != "simple-declaration")) continue;
		const string name = LastComponent(FirstIdentifierLocal(
			FunctionDeclarator(candidate)));
		if(!name.empty()) late_functions[name] = candidate;
	}
	set<string> selected_names;
	vector<CPPGMAstNodePtr> selected_functions;
	for(map<string, CPPGMAstNodePtr>::const_iterator candidate = late_functions.begin();
		candidate != late_functions.end(); ++candidate) {
		const string primary = LastComponent(candidate->second->template_primary);
		if(MentionsGeneratedFunction(node, candidate->first) ||
			(!primary.empty() && MentionsGeneratedFunction(node, primary))) {
			selected_names.insert(candidate->first);
			selected_functions.push_back(candidate->second);
		}
	}
	for(size_t selected = 0; selected < selected_functions.size(); ++selected)
		for(map<string, CPPGMAstNodePtr>::const_iterator candidate = late_functions.begin();
			candidate != late_functions.end(); ++candidate)
			if(selected_names.find(candidate->first) == selected_names.end() &&
				MentionsGeneratedFunction(selected_functions[selected], candidate->first)) {
				selected_names.insert(candidate->first);
				selected_functions.push_back(candidate->second);
			}
	if(selected_functions.empty()) return;
	set<const CPPGMAstNode*> direct_children;
	for(size_t child = 0; child < node->children.size(); ++child)
		if(node->children[child]) direct_children.insert(node->children[child].get());
	vector<CPPGMAstNodePtr> to_insert;
	for(size_t selected = 0; selected < selected_functions.size(); ++selected) {
		const CPPGMAstNodePtr& candidate = selected_functions[selected];
		if(!candidate || direct_children.find(candidate.get()) != direct_children.end()) continue;
		to_insert.push_back(candidate);
		direct_children.insert(candidate.get());
	}
	if(to_insert.empty()) return;
	size_t position = node->children.size();
	for(size_t child = 0; child < node->children.size(); ++child) {
		const string& kind = node->children[child]->kind;
		if(kind == "static-assert-declaration" || kind == "function-definition" ||
			kind == "special-member-definition") {
			position = child;
			break;
		}
	}
	node->children.insert(node->children.begin() + position, to_insert.begin(), to_insert.end());
}

} // namespace pa18_templates_internal
