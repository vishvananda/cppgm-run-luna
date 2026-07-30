#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
using namespace std;

namespace pa18_templates_internal {

void PA18TemplateExpander::InstallImplicitNestedForwards(
	const TemplateDefinition& definition, const CPPGMAstNodePtr& generated,
	const string& generated_owner, const string& local_name)
{
	if(!definition.class_template || !definition.declaration) return;
	const string source_class = LastComponent(definition.declaration->value);
	if(source_class.empty()) return;
	set<string> explicit_nested;
	for(size_t child = 0; child < definition.declaration->children.size(); ++child) {
		const CPPGMAstNodePtr member = definition.declaration->children[child];
		if(member && (member->kind == "class-specifier" ||
			member->kind == "class-forward-declaration"))
			explicit_nested.insert(LastComponent(member->value));
	}
	set<string> implicit_nested;
	const auto collect_names = [&implicit_nested](const string& raw) {
		const char* const keys[] = {"struct", "class", "union"};
		for(size_t position = 0; position < raw.size();) {
			if(position > 0 && IsIdentifierCharacter(raw[position - 1])) {
				++position;
				continue;
			}
			bool found = false;
			for(size_t key = 0; key < sizeof(keys) / sizeof(keys[0]); ++key) {
				const string keyword = keys[key];
				if(raw.compare(position, keyword.size(), keyword) != 0) continue;
				size_t begin = position + keyword.size();
				if(begin < raw.size() && IsIdentifierCharacter(raw[begin]) &&
					IsIdentifierCharacter(raw[position + keyword.size() - 1])) {
					// `structName` is accepted by the parser's compact spelling
					// recovery; the following identifier scan handles both forms.
				} else {
					while(begin < raw.size() && isspace(static_cast<unsigned char>(raw[begin]))) ++begin;
				}
				if(begin >= raw.size() || !IsIdentifierCharacter(raw[begin])) continue;
				size_t end = begin;
				while(end < raw.size() && (IsIdentifierCharacter(raw[end]) || raw[end] == ':')) ++end;
				const string candidate = CanonicalSpelling(raw.substr(begin, end - begin));
				if(!candidate.empty() && candidate.find("::") == string::npos)
					implicit_nested.insert(candidate);
				position = end;
				found = true;
				break;
			}
			if(!found) ++position;
		}
	};
	const auto scan = [&](const CPPGMAstNodePtr& root) {
		function<void(const CPPGMAstNodePtr&, bool)> visit;
		visit = [&](const CPPGMAstNodePtr& node, bool is_root) {
			if(!node) return;
			if(!is_root && (node->kind == "class-specifier" ||
				node->kind == "class-forward-declaration")) return;
			if(node->kind == "function-definition" ||
				node->kind == "special-member-definition" ||
				node->kind == "special-member-declaration") {
				if(!node->children.empty()) collect_names(NodeTypeSpelling(node->children[0]));
				if(node->children.size() > 1) {
					const CPPGMAstNodePtr clause = DescendantOfKind(node->children[1],
						"parameter-clause");
					if(clause) for(size_t parameter = 0; parameter < clause->children.size(); ++parameter)
						if(clause->children[parameter] && clause->children[parameter]->kind ==
							"parameter-declaration")
							collect_names(ParameterTypeSpelling(clause->children[parameter]));
				}
				return;
			}
			if(node->kind == "simple-declaration" && !node->children.empty())
				collect_names(NodeTypeSpelling(node->children[0]));
			else if(node->kind == "alias-declaration" && !node->children.empty())
				collect_names(TypeIdSpelling(node->children[0]));
			else if(node->kind == "base-name") collect_names(node->value);
			for(size_t child = 0; child < node->children.size(); ++child)
				visit(node->children[child], false);
		};
		visit(root, true);
	};
	scan(definition.declaration);
	for(set<string>::const_iterator name = implicit_nested.begin();
		name != implicit_nested.end(); ++name) {
		if(explicit_nested.find(*name) != explicit_nested.end()) continue;
		CPPGMAstNodePtr forward;
		if(generated && generated->kind == "class-specifier") {
			bool already_generated = false;
			for(size_t child = 0; child < generated->children.size(); ++child) {
				const CPPGMAstNodePtr member = generated->children[child];
				if(member && (member->kind == "class-specifier" ||
					member->kind == "class-forward-declaration") &&
					LastComponent(member->value) == *name) {
					forward = member;
					already_generated = true;
					break;
				}
			}
			if(!already_generated) {
				forward = MakeForwardClass(*name);
				size_t insert = 0;
				while(insert < generated->children.size() && generated->children[insert] &&
					(generated->children[insert]->kind == "class-key" ||
					 generated->children[insert]->kind == "base-clause")) ++insert;
				generated->children.insert(generated->children.begin() + insert, forward);
			}
		}
		if(!forward) forward = MakeForwardClass(*name);
		const string generated_path = JoinPath(generated_owner, local_name);
		const string nested_path = JoinPath(generated_path, *name);
		class_declarations_[nested_path] = forward;
		RememberClassPath(nested_path);
	}
}

bool PA18TemplateExpander::FindUnqualifiedGeneratedAliasPath(
	const string& spelling, const string& context, string* alias_path) const
{
	if(!alias_path || spelling.find("::") != string::npos) return false;
	const map<string, vector<string> >::const_iterator candidates =
		type_aliases_by_name_.find(LastComponent(spelling));
	if(candidates == type_aliases_by_name_.end() || candidates->second.empty()) return false;
	if(candidates->second.size() == 1) {
		*alias_path = candidates->second[0];
		return true;
	}
	for(size_t candidate = candidates->second.size(); candidate > 0; --candidate) {
		const string& path = candidates->second[candidate - 1];
		const string owner = PrefixComponent(path);
		bool generated_owner = false;
		for(size_t begin = 0; begin <= owner.size();) {
			const size_t end = owner.find("::", begin);
			const string component = owner.substr(begin,
				end == string::npos ? string::npos : end - begin);
			if(specialization_bases_.find(component) != specialization_bases_.end()) {
				generated_owner = true;
				break;
			}
			if(end == string::npos) break;
			begin = end + 2;
		}
		if(!generated_owner) continue;
		map<string, string>::const_iterator value = type_aliases_.find(path);
		if(value != type_aliases_.end() && !value->second.empty() &&
			!HasUnresolvedTemplateParameter(value->second, context, map<string, string>())) {
			*alias_path = path;
			return true;
		}
	}
	return false;
}

}
