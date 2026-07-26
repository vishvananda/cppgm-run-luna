#include "pa11_semantics_analyzer.h"

using namespace std;

namespace {

string SourceAliasPath(const string& scope, const string& name)
{
	return scope.empty() ? name : (name.empty() ? scope : scope + "::" + name);
}

bool HasTypedefSpecifier(const CPPGMAstNodePtr& node)
{
	if(!node) return false;
	for(size_t child = 0; child < node->children.size(); ++child)
		if(node->children[child] && node->children[child]->kind == "decl-specifier" &&
			node->children[child]->value == "KW_TYPEDEF:typedef") return true;
	return false;
}

} // namespace

void CollectSourceTypeAliasPaths(const CPPGMAstNodePtr& node, const string& scope,
	set<string>* paths)
{
	if(!node || !paths) return;
	if(node->kind == "alias-declaration") {
		const string name = LastComponent(node->value);
		if(!name.empty()) paths->insert(SourceAliasPath(scope, name));
		return;
	}
	if(node->kind == "simple-declaration" && !node->children.empty() &&
		HasTypedefSpecifier(node->children[0])) {
		const CPPGMAstNodePtr list = ChildOfKind(node, "init-declarator-list");
		if(list) for(size_t item = 0; item < list->children.size(); ++item) {
			const CPPGMAstNodePtr entry = list->children[item];
			if(!entry || entry->children.empty()) continue;
			const string name = LastComponent(FirstIdentifier(entry->children[0]));
			if(!name.empty()) paths->insert(SourceAliasPath(scope, name));
		}
	}
	string child_scope = scope;
	if(node->kind == "namespace-definition" && node->value != "<unnamed>")
		child_scope = SourceAliasPath(scope, LastComponent(node->value));
	else if((node->kind == "class-specifier" ||
		node->kind == "class-forward-declaration") && !node->value.empty())
		child_scope = SourceAliasPath(scope, LastComponent(node->value));
	for(size_t child = 0; child < node->children.size(); ++child)
		CollectSourceTypeAliasPaths(node->children[child], child_scope, paths);
}

bool ContainsSourceTypeAliasBase(const CPPGMAstNodePtr& node,
	const set<string>& paths)
{
	if(!node) return false;
	if(node->kind == "base-name") {
		string base = StripTypeMarker(node->value);
		while(base.compare(0, 2, "::") == 0) base.erase(0, 2);
		if(base.find('<') == string::npos && paths.find(base) != paths.end()) return true;
		if(base.find("::") == string::npos && paths.find(LastComponent(base)) != paths.end())
			return true;
	}
	for(size_t child = 0; child < node->children.size(); ++child)
		if(ContainsSourceTypeAliasBase(node->children[child], paths)) return true;
	return false;
}
