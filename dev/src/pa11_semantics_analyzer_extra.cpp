#include "pa11_semantics_analyzer.h"

using namespace std;

namespace {

size_t TopLevelScopeSeparator(const string& raw)
{
	int angle_depth = 0;
	for(size_t i = 0; i + 1 < raw.size(); ++i) {
		if(raw[i] == '<') ++angle_depth;
		else if(raw[i] == '>' && angle_depth > 0) --angle_depth;
		else if(raw[i] == ':' && raw[i + 1] == ':' && angle_depth == 0)
			return i;
	}
	return string::npos;
}

} // namespace

TypePtr Analyzer::ProcessForwardClass(const CPPGMAstNodePtr& node, Scope* scope)
{
	const string raw_name = node->value;
	const string name = LastComponent(raw_name);
	if(name.empty()) throw logic_error("anonymous class forward declaration");
	Scope* owner = scope;
	if(TopLevelScopeSeparator(raw_name) != string::npos) {
		const size_t separator = TopLevelScopeSeparator(raw_name);
		PathTarget prefix = ResolvePath(scope, raw_name.substr(0, separator));
		owner = prefix.binding ? ScopeForType(prefix.binding->type) : prefix.scope;
		if(!owner) throw logic_error("unknown forward class owner");
	}
	Binding* existing = owner->local(name);
	if(existing && existing->kind == BIND_TYPE) {
		if(existing->type && existing->type->kind == TYPE_CLASS)
			ApplyClassAttributes(node, existing->type, scope);
		return existing->type;
	}
	existing = ResolveBinding(scope, name);
	if(existing && (existing->kind == BIND_TYPE || existing->kind == BIND_TYPE_ALIAS) &&
		existing->type && existing->type->kind == TYPE_CLASS) {
		ApplyClassAttributes(node, existing->type, scope);
		return existing->type;
	}
	TypePtr type(new Type(TYPE_CLASS, name));
	type->tag = ClassKey(node);
	type->complete = false;
	ApplyClassAttributes(node, type, scope);
	if(!owner->qualified_prefix.empty()) type->name = owner->qualified_prefix + "::" + name;
	AddTypeBinding(owner, name, type);
	return type;
}
