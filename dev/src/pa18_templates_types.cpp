#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

string PA18TemplateExpander::MemberAliasType(const string& class_key, const string& member,
	bool allow_nested_class) const
{
	const string owner_key = CanonicalSpelling(RemoveMarker(class_key));
	const string member_name = CanonicalSpelling(RemoveMarker(member));
	if(owner_key.empty() || member_name.empty()) return string();
	for(size_t i = 0; i < member_name.size(); ++i)
		if(!IsIdentifierCharacter(member_name[i])) return string();
	map<string, CPPGMAstNodePtr>::const_iterator declaration_it =
		class_declarations_.find(owner_key);
	if(declaration_it == class_declarations_.end() || !declaration_it->second)
		return string();
	const CPPGMAstNodePtr& declaration = declaration_it->second;
	if(declaration) {
		for(size_t i = 0; i < declaration->children.size(); ++i) {
			CPPGMAstNodePtr child = declaration->children[i];
			if(!child) continue;
			// A member alias template is represented by a wrapper node around
			// the alias declaration.  Materialized nested classes use that same
			// AST shape, so inspect the wrapped declaration before falling through
			// to inherited-member lookup.
			while(child->kind == "template-declaration" && child->children.size() > 1)
				child = child->children[1];
			if(child->kind == "alias-declaration" &&
				LastComponent(RemoveMarker(child->value)) == member_name &&
				!child->children.empty()) {
				const string value = QualifyNestedMembers(TypeIdSpelling(child->children[0]),
					owner_key, declaration);
				return value;
			}
			if(child->kind != "simple-declaration" || child->children.empty() ||
				SpellNode(child->children[0]).find("typedef") == string::npos) continue;
			const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
			if(!list) continue;
			for(size_t j = 0; j < list->children.size(); ++j) {
				const CPPGMAstNodePtr item = list->children[j];
				if(!item || item->children.empty() ||
					LastComponent(RemoveMarker(FirstIdentifierLocal(item->children[0]))) != member_name)
					continue;
				const string value = QualifyNestedMembers(DeclaratorTypeSpelling(
					NodeTypeSpelling(child->children[0]), item->children[0]),
					owner_key, declaration);
				return value;
			}
		}
	}
	// The aliases-only fallback below also sees generated nested class members.
	// A nested class and a type alias cannot share a member name, so an indexed
	// concrete declaration is authoritative evidence that this lookup is a type
	// member, not an alias suitable for replacing a bare identifier.
	if(!allow_nested_class && class_declarations_.find(JoinPath(owner_key, member_name)) !=
		class_declarations_.end()) return string();
	map<string, string> substitutions;
	set<string> active;
	string inherited;
	if(FindClassMemberType(owner_key, member_name, substitutions, PrefixComponent(owner_key),
		&inherited, &active, true)) {
		if(!allow_nested_class && CanonicalSpelling(inherited) == CanonicalSpelling(JoinPath(owner_key,
			member_name))) return string();
		return QualifyNestedMembers(inherited, owner_key, declaration);
	}
	return string();
}
string PA18TemplateExpander::QualifyNestedMembers(string spelling, const string& class_key,
	const CPPGMAstNodePtr& declaration) const
{
	if(!declaration || class_key.empty()) return spelling;
	for(size_t i = 0; i < declaration->children.size(); ++i) {
		const CPPGMAstNodePtr child = declaration->children[i];
		if(!child || (child->kind != "class-specifier" &&
			child->kind != "class-forward-declaration")) continue;
		const string name = LastComponent(child->value);
		for(size_t at = spelling.find(name); at != string::npos;
			at = spelling.find(name, at + name.size())) {
			const bool left = at == 0 || !IsIdentifierCharacter(spelling[at - 1]);
			const size_t end = at + name.size();
			const bool right = end == spelling.size() || !IsIdentifierCharacter(spelling[end]);
			const bool qualified = at >= 2 && spelling.compare(at - 2, 2, "::") == 0;
			if(left && right && !qualified) {
				spelling.replace(at, name.size(), class_key + "::" + name);
				at += class_key.size() + 2;
			}
		}
	}
	return spelling;
}
string PA18TemplateExpander::ParameterTypeSpelling(const CPPGMAstNodePtr& parameter) const
{
	if(!parameter || parameter->children.empty()) return string();
	// ParameterTypeSpelling is consumed by deduction as well as by variable
	// collection.  Preserve array suffixes and nested references so
	// `T (&)[N]` contributes both its element and bound to deduction.
	return DeclaratorTypeSpelling(NodeTypeSpelling(parameter->children[0]),
		parameter->children.size() > 1 ? parameter->children[1] : CPPGMAstNodePtr());
}
string PA18TemplateExpander::FunctionTypeSpelling(const CPPGMAstNodePtr& parameter) const
{
	if(!parameter || parameter->children.size() < 2 || !parameter->children[1])
		return ParameterTypeSpelling(parameter);
	const CPPGMAstNodePtr declarator = parameter->children[1];
	const string base = NodeTypeSpelling(parameter->children[0]);
	const auto append_parameters = [this](const CPPGMAstNodePtr& clause,
		string* result) {
		if(!clause || !result) return;
		for(size_t i = 0; i < clause->children.size(); ++i) {
			const CPPGMAstNodePtr item = clause->children[i];
			if(!item || item->kind != "parameter-declaration") continue;
			const string pack_name = IsFunctionParameterPack(item) ?
				PackExpansionIdentifier(item) : string();
			map<string, vector<string> >::const_iterator pack =
				active_pack_substitutions_.find(pack_name);
			vector<string> values;
			if(!pack_name.empty() && pack != active_pack_substitutions_.end())
				values = pack->second;
			if(values.empty() && (pack_name.empty() ||
				pack == active_pack_substitutions_.end()))
				values.push_back(ParameterTypeSpelling(item));
			for(size_t value = 0; value < values.size(); ++value) {
				string spelling = values[value];
				if(!pack_name.empty()) {
					map<string, string> one;
					one[pack_name] = spelling;
					spelling = ReplaceIdentifiersPreservingPackSizes(
						ParameterTypeSpelling(item), one);
					// Each value is one element of the parameter pack.  The
					// declarator spelling still carries the source expansion marker
					// (`T...`), but retaining it after substituting one element turns
					// a function type such as `R(T...)` into `R(U...)` instead of
					// `R(U)`.  The marker has already been consumed by this loop.
					if(spelling.size() >= 3 &&
						spelling.compare(spelling.size() - 3, 3, "...") == 0)
						spelling.erase(spelling.size() - 3);
				}
				if(result->size() > 0 && result->at(result->size() - 1) != '(')
					*result += ',';
				*result += spelling;
			}
		}
	};
	const CPPGMAstNodePtr nested = ChildOfKindLocal(declarator, "nested-declarator");
	const CPPGMAstNodePtr clause = ChildOfKindLocal(declarator, "parameter-clause");
	if(!nested || !clause) return ParameterTypeSpelling(parameter);
	const CPPGMAstNodePtr inner = nested->children.empty() ? CPPGMAstNodePtr() : nested->children[0];
	string result = base + DeclaratorSuffix(declarator);
	result += inner && DeclaratorSuffix(inner).find('&') != string::npos ? "(&)(" : "(*)(";
	append_parameters(clause, &result);
	result += ')';
	return CanonicalSpelling(result);
}
string PA18TemplateExpander::DeclaratorTypeSpelling(const string& base,
	const CPPGMAstNodePtr& declarator) const
{
	if(!declarator) return base;
	const auto append_parameters = [this](const CPPGMAstNodePtr& clause,
		string* result) {
		if(!clause || !result) return;
		for(size_t i = 0; i < clause->children.size(); ++i) {
			const CPPGMAstNodePtr item = clause->children[i];
			if(!item || item->kind != "parameter-declaration") continue;
			const string pack_name = IsFunctionParameterPack(item) ?
				PackExpansionIdentifier(item) : string();
			map<string, vector<string> >::const_iterator pack =
				active_pack_substitutions_.find(pack_name);
			vector<string> values;
			if(!pack_name.empty() && pack != active_pack_substitutions_.end())
				values = pack->second;
			if(values.empty() && (pack_name.empty() ||
				pack == active_pack_substitutions_.end()))
				values.push_back(ParameterTypeSpelling(item));
			for(size_t value = 0; value < values.size(); ++value) {
				string spelling = values[value];
				if(!pack_name.empty()) {
					map<string, string> one;
					one[pack_name] = spelling;
					spelling = ReplaceIdentifiersPreservingPackSizes(
						ParameterTypeSpelling(item), one);
					// The active pack supplies a single declarator here; do not
					// carry the source pack-expansion marker into that element.
					if(spelling.size() >= 3 &&
						spelling.compare(spelling.size() - 3, 3, "...") == 0)
						spelling.erase(spelling.size() - 3);
				}
				if(result->size() > 0 && result->at(result->size() - 1) != '(')
					*result += ',';
				*result += spelling;
			}
		}
	};
	const CPPGMAstNodePtr nested = ChildOfKindLocal(declarator, "nested-declarator");
	const CPPGMAstNodePtr clause = ChildOfKindLocal(declarator, "parameter-clause");
	if(nested && !clause) {
		// In `char (&name)[N]`, the reference operator belongs to the nested
		// declarator while the array suffix belongs to the outer declarator.
		// Keeping only the outer suffix silently turns the alias into `char[N]`
		// (and, after alias lookup, often into just `char`).
		const CPPGMAstNodePtr inner = nested->children.empty() ? CPPGMAstNodePtr() :
			nested->children[0];
		const string inner_suffix = DeclaratorSuffix(inner);
		string result = base + DeclaratorSuffix(declarator);
		if(!inner_suffix.empty()) result += "(" + inner_suffix + ")";
		result += DeclaratorArraySuffix(declarator);
		return CanonicalSpelling(result);
	}
	if(!nested && clause) {
		// A typedef of a function (`typedef R name(A);`) has a direct
		// parameter-clause rather than the nested declarator used by a
		// function pointer.  Retain its complete function type so a later
		// pointer-shaped partial specialization can bind the typedef as a
		// type, instead of reducing it to only R.
		string result = base + "(";
		append_parameters(clause, &result);
		result += ')';
		return CanonicalSpelling(result);
	}
	if(!nested || !clause) return CanonicalSpelling(base + DeclaratorSuffix(declarator) +
		DeclaratorArraySuffix(declarator));
	const CPPGMAstNodePtr inner = nested->children.empty() ? CPPGMAstNodePtr() : nested->children[0];
	string result = base + DeclaratorSuffix(declarator);
	result += inner && DeclaratorSuffix(inner).find('&') != string::npos ? "(&)(" : "(*)(";
	append_parameters(clause, &result);
	result += ')';
	return CanonicalSpelling(result);
}

} // namespace pa18_templates_internal
