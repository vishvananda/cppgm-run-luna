#include <iostream>

#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

namespace {
string OrderingDeclaredName(const CPPGMAstNodePtr& node)
{
	if(!node) return string();
	if(node->kind == "class-specifier" || node->kind == "class-forward-declaration" ||
		node->kind == "enum-specifier" || node->kind == "alias-declaration")
		return LastComponent(node->value);
	if(node->kind != "simple-declaration") return string();
	const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
	if(!list || list->children.empty() || !list->children[0] ||
		list->children[0]->children.empty()) return string();
	return LastComponent(FirstIdentifierLocal(list->children[0]->children[0]));
}

bool OrderingTypeDeclaration(const CPPGMAstNodePtr& node)
{
	if(!node) return false;
	if(node->kind == "class-specifier" || node->kind == "class-forward-declaration" ||
		node->kind == "enum-specifier" || node->kind == "alias-declaration") return true;
	return node->kind == "simple-declaration" && !node->children.empty() &&
		SpellNode(node->children[0]).find("typedef") != string::npos;
}

bool MentionsQualifiedGeneratedType(const CPPGMAstNodePtr& node,
	const string& type_name)
{
	if(!node || type_name.empty()) return false;
	const string spelling = CanonicalSpelling(RemoveMarker(node->value));
	if(spelling.find(type_name + "::") != string::npos) return true;
	for(size_t i = 0; i < node->children.size(); ++i)
		if(MentionsQualifiedGeneratedType(node->children[i], type_name)) return true;
	return false;
}

bool NeedsPA18Expansion(const CPPGMAstNodePtr& node)
{
	if(!node) return false;
	if(node->kind == "template-declaration" ||
		node->kind == "explicit-instantiation-declaration") return true;
	if((node->kind == "id-expression" || node->kind == "decl-specifier" ||
		node->kind == "type-name" || node->kind == "type-specifier" ||
		node->kind == "class-specifier" || node->kind == "class-forward-declaration" ||
		node->kind == "alias-declaration" || node->kind == "target") &&
		node->value.find('<') != string::npos) return true;
	for(size_t i = 0; i < node->children.size(); ++i)
		if(NeedsPA18Expansion(node->children[i])) return true;
	return false;
}

}

// Type spelling and alias helpers live out of line to keep the collection
// declaration compact while retaining access to its typed semantic state.
string PA18TemplateExpander::InheritedTypeName(const string& scope, const string& name,
	set<string>* active) const
{
	if(!active || scope.empty() || !active->insert(scope).second) return string();
	map<string, CPPGMAstNodePtr>::const_iterator found = class_declarations_.find(scope);
	if(found == class_declarations_.end() || !found->second) {
		active->erase(scope);
		return string();
	}
	const CPPGMAstNodePtr& declaration = found->second;
	for(size_t i = 0; i < declaration->children.size(); ++i) {
		const CPPGMAstNodePtr clause = declaration->children[i];
		if(!clause || clause->kind != "base-clause") continue;
		for(size_t j = 0; j < clause->children.size(); ++j) {
			const CPPGMAstNodePtr base = ChildOfKindLocal(clause->children[j], "base-name");
			if(!base) continue;
			const string base_name = CanonicalSpelling(base->value);
			if(named_type_contexts_.find(JoinPath(base_name, name)) !=
				named_type_contexts_.end() || type_aliases_.find(JoinPath(base_name, name)) !=
				type_aliases_.end()) {
				active->erase(scope);
				return JoinPath(base_name, name);
			}
			const string inherited = InheritedTypeName(base_name, name, active);
			if(!inherited.empty()) {
				active->erase(scope);
				return inherited;
			}
		}
	}
	active->erase(scope);
	return string();
}
string PA18TemplateExpander::QualifyTypeArgument(string spelling, const string& context,
	const string& template_owner) const
{
	spelling = CanonicalSpelling(spelling);
	while(spelling.compare(0, 8, "typename") == 0 &&
		(spelling.size() == 8 || isspace(static_cast<unsigned char>(spelling[8]))))
		spelling = CanonicalSpelling(spelling.substr(8));
	const char* const elaborated_keys[] = {"struct", "class", "union"};
	for(size_t key = 0; key < sizeof(elaborated_keys) / sizeof(elaborated_keys[0]); ++key) {
		const string prefix_key = elaborated_keys[key];
		if(spelling.compare(0, prefix_key.size(), prefix_key) == 0 &&
			spelling.size() > prefix_key.size()) {
			const string candidate = spelling.substr(prefix_key.size());
			if(class_contexts_.find(candidate) != class_contexts_.end()) {
				spelling = candidate;
				break;
			}
		}
	}
	while(spelling.compare(0, 7, "struct ") == 0 ||
		spelling.compare(0, 6, "class ") == 0 ||
		spelling.compare(0, 6, "union ") == 0) {
		const size_t space = spelling.find(' ');
		spelling = CanonicalSpelling(spelling.substr(space + 1));
	}
	string prefix;
	if(spelling.compare(0, 6, "const ") == 0) {
		prefix = "const ";
		spelling = CanonicalSpelling(spelling.substr(6));
	} else if(spelling.compare(0, 9, "volatile ") == 0) {
		prefix = "volatile ";
		spelling = CanonicalSpelling(spelling.substr(9));
	}
	if(named_type_contexts_.find(spelling) != named_type_contexts_.end()) {
		const string enum_owner = PrefixComponent(spelling);
		const bool visible_from_context = context == enum_owner ||
			(context.size() > enum_owner.size() &&
				context.compare(0, enum_owner.size(), enum_owner) == 0 &&
				context[enum_owner.size()] == ':');
		if(visible_from_context) spelling = LastComponent(spelling);
	}
	size_t suffix_begin = spelling.find_first_of("*&");
	string suffix;
	if(suffix_begin != string::npos) {
		suffix = spelling.substr(suffix_begin);
		spelling = CanonicalSpelling(spelling.substr(0, suffix_begin));
	}
	map<string, string>::const_iterator local = local_class_names_.find(
		JoinPath(context, spelling));
	if(local != local_class_names_.end()) spelling = local->second;
	if(spelling.size() > 5 && spelling.compare(spelling.size() - 5, 5, "const") == 0 &&
		spelling.find("::") == string::npos) {
	spelling = CanonicalSpelling(spelling.substr(0, spelling.size() - 5));
	prefix = "const ";
	}
	if(spelling.find("::") != string::npos && spelling[0] != ':') {
	const size_t separator = spelling.find("::");
	const string first = spelling.substr(0, separator);
	const string remainder = spelling.substr(separator);
	for(string current = context; ; ) {
		const string candidate = JoinPath(current, first);
		if(class_contexts_.find(candidate) != class_contexts_.end()) {
			spelling = candidate + remainder;
			break;
		}
		if(current.empty()) break;
		const size_t parent = current.rfind("::");
		if(parent == string::npos) current.clear();
		else current.erase(parent);
	}
	}
	if(!template_owner.empty()) {
	const string owner_prefix = template_owner + "::";
	if(spelling.compare(0, owner_prefix.size(), owner_prefix) == 0)
		spelling.erase(0, owner_prefix.size());
	}
	if(spelling.find("::") == string::npos && spelling.find('<') == string::npos) {
	string current = context;
	for(;;) {
		const string candidate = JoinPath(current, spelling);
		if(class_contexts_.find(candidate) != class_contexts_.end() ||
			named_type_contexts_.find(candidate) != named_type_contexts_.end()) {
			const bool function_scope = function_contexts_.find(context) !=
				function_contexts_.end();
			const bool same_template_owner = !template_owner.empty() &&
				PrefixComponent(candidate) == template_owner;
			spelling = (function_scope && !same_template_owner) ||
				(!template_owner.empty() && !same_template_owner) ? candidate :
				LastComponent(candidate);
			break;
		}
		if(current.empty()) break;
		const size_t separator = current.rfind("::");
		if(separator == string::npos) current.clear();
		else current.erase(separator);
	}
	if(class_contexts_.find(spelling) != class_contexts_.end()) {
	}
	if(spelling.find("::") == string::npos) {
		set<string> active;
		const string inherited = InheritedTypeName(context, spelling, &active);
		if(!inherited.empty()) spelling = inherited;
	}
	}
	return CanonicalSpelling(prefix + spelling + suffix);
}
string PA18TemplateExpander::DeclaratorSuffix(const CPPGMAstNodePtr& declarator) const
{
	if(!declarator) return string();
	string result;
	for(size_t i = 0; i < declarator->children.size(); ++i) {
		const CPPGMAstNodePtr child = declarator->children[i];
		if(!child) continue;
		if(child->kind == "ptr-operator") {
			if(child->value.find("&&") != string::npos) result += "&&";
			else if(child->value.find('&') != string::npos) result += '&';
			else result += '*';
		} else if(child->kind == "cv-qualifier") result += RemoveMarker(child->value);
	}
	return result;
}
string PA18TemplateExpander::DeclaratorArraySuffix(const CPPGMAstNodePtr& declarator) const
{
	if(!declarator) return string();
	string result;
	for(size_t i = 0; i < declarator->children.size(); ++i) {
		const CPPGMAstNodePtr child = declarator->children[i];
		if(!child || child->kind != "array-suffix") continue;
		result += '[';
		if(!child->children.empty()) result += SpellNode(child->children[0]);
		result += ']';
	}
	return result;
}
string PA18TemplateExpander::MemberAliasType(const string& class_key, const string& member) const
{
	map<string, CPPGMAstNodePtr>::const_iterator found = class_declarations_.find(class_key);
	if(found == class_declarations_.end() || !found->second) return string();
	const CPPGMAstNodePtr& declaration = found->second;
	for(size_t i = 0; i < declaration->children.size(); ++i) {
		const CPPGMAstNodePtr child = declaration->children[i];
		if(!child) continue;
		if(child->kind == "alias-declaration" && child->value == member &&
			!child->children.empty())
			return QualifyNestedMembers(TypeIdSpelling(child->children[0]),
				class_key, declaration);
		if(child->kind != "simple-declaration" || child->children.empty() ||
			SpellNode(child->children[0]).find("typedef") == string::npos) continue;
		const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
		if(!list) continue;
		for(size_t j = 0; j < list->children.size(); ++j) {
			const CPPGMAstNodePtr item = list->children[j];
			if(!item || item->children.empty() ||
				LastComponent(FirstIdentifierLocal(item->children[0])) != member) continue;
			if(!DeclaratorArraySuffix(item->children[0]).empty()) return string();
			return QualifyNestedMembers(NodeTypeSpelling(child->children[0]) +
				DeclaratorSuffix(item->children[0]) +
				DeclaratorArraySuffix(item->children[0]), class_key, declaration);
		}
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
	string result = NodeTypeSpelling(parameter->children[0]);
	if(parameter->children.size() > 1) result += DeclaratorSuffix(parameter->children[1]);
	return CanonicalSpelling(result);
}
string PA18TemplateExpander::FunctionTypeSpelling(const CPPGMAstNodePtr& parameter) const
{
	if(!parameter || parameter->children.size() < 2 || !parameter->children[1])
		return ParameterTypeSpelling(parameter);
	const CPPGMAstNodePtr declarator = parameter->children[1];
	const string base = NodeTypeSpelling(parameter->children[0]);
	const CPPGMAstNodePtr nested = ChildOfKindLocal(declarator, "nested-declarator");
	const CPPGMAstNodePtr clause = ChildOfKindLocal(declarator, "parameter-clause");
	if(!nested || !clause) return ParameterTypeSpelling(parameter);
	const CPPGMAstNodePtr inner = nested->children.empty() ? CPPGMAstNodePtr() : nested->children[0];
	string result = base + DeclaratorSuffix(declarator);
	result += inner && DeclaratorSuffix(inner).find('&') != string::npos ? "(&)(" : "(*)(";
	for(size_t i = 0; i < clause->children.size(); ++i) {
		const CPPGMAstNodePtr item = clause->children[i];
		if(!item || item->kind != "parameter-declaration") continue;
		if(result[result.size() - 1] != '(') result += ',';
		result += ParameterTypeSpelling(item);
	}
	result += ')';
	return CanonicalSpelling(result);
}
string PA18TemplateExpander::DeclaratorTypeSpelling(const string& base,
	const CPPGMAstNodePtr& declarator) const
{
	if(!declarator) return base;
	const CPPGMAstNodePtr nested = ChildOfKindLocal(declarator, "nested-declarator");
	const CPPGMAstNodePtr clause = ChildOfKindLocal(declarator, "parameter-clause");
	if(!nested || !clause) return CanonicalSpelling(base + DeclaratorSuffix(declarator));
	const CPPGMAstNodePtr inner = nested->children.empty() ? CPPGMAstNodePtr() : nested->children[0];
	string result = base + DeclaratorSuffix(declarator);
	result += inner && DeclaratorSuffix(inner).find('&') != string::npos ? "(&)(" : "(*)(";
	for(size_t i = 0; i < clause->children.size(); ++i) {
		const CPPGMAstNodePtr item = clause->children[i];
		if(!item || item->kind != "parameter-declaration") continue;
		if(result[result.size() - 1] != '(') result += ',';
		result += ParameterTypeSpelling(item);
	}
	result += ')';
	return CanonicalSpelling(result);
}
string PA18TemplateExpander::TypeIdSpelling(const CPPGMAstNodePtr& type_id) const
{
	if(!type_id) return string();
	const CPPGMAstNodePtr specs = ChildOfKindLocal(type_id, "type-specifier-seq");
	const string base = NodeTypeSpelling(specs);
	const CPPGMAstNodePtr abstract = ChildOfKindLocal(type_id, "abstract-declarator");
	if(!abstract) return base;
	const CPPGMAstNodePtr nested = ChildOfKindLocal(abstract, "nested-declarator");
	const CPPGMAstNodePtr clause = nested ? ChildOfKindLocal(nested, "parameter-clause") :
		ChildOfKindLocal(abstract, "parameter-clause");
	if(nested && clause) {
		const CPPGMAstNodePtr inner = nested->children.empty() ? CPPGMAstNodePtr() :
			ChildOfKindLocal(nested, "abstract-declarator");
		string result = base;
		result += inner && DeclaratorSuffix(inner).find('&') != string::npos ? "(&)(" : "(*)(";
		for(size_t i = 0; i < clause->children.size(); ++i) {
			const CPPGMAstNodePtr parameter = clause->children[i];
			if(!parameter || parameter->kind != "parameter-declaration") continue;
			if(result[result.size() - 1] != '(') result += ',';
			result += ParameterTypeSpelling(parameter);
		}
		result += ')';
		return CanonicalSpelling(result);
	}
	return DeclaratorTypeSpelling(base, abstract);
}
CPPGMAstNodePtr PA18TemplateExpander::FunctionDeclarator(const CPPGMAstNodePtr& declaration) const
{
	if(!declaration) return CPPGMAstNodePtr();
	if(declaration->kind == "function-definition" && declaration->children.size() > 1)
		return declaration->children[1];
	if(declaration->kind == "simple-declaration") {
		const CPPGMAstNodePtr list = ChildOfKindLocal(declaration, "init-declarator-list");
		if(list && !list->children.empty() && list->children[0] &&
			!list->children[0]->children.empty()) return list->children[0]->children[0];
	}
	return CPPGMAstNodePtr();
}

bool PA18TemplateExpander::TypeOnlyNode(const CPPGMAstNodePtr& node) const
{
	if(!node || node->kind == "class-specifier" ||
		node->kind == "class-forward-declaration" || node->kind == "enum-specifier" ||
		node->kind == "template-declaration" || node->kind == "empty-declaration") return true;
	if(node->kind == "namespace-definition") {
		for(size_t i = 0; i < node->children.size(); ++i)
			if(node->children[i] && node->children[i]->kind != "inline" &&
				(node->children[i]->kind == "simple-declaration" ||
				 node->children[i]->kind == "function-definition" ||
				 node->children[i]->kind == "alias-declaration" ||
				 !TypeOnlyNode(node->children[i]))) return false;
		return true;
	}
	// Function declarations and ordinary object declarations can mention a
	// generated complete type.  They must therefore remain after generated
	// class materializations rather than being treated as type-only nodes.
	if(node->kind == "simple-declaration") {
		const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
		return list && !list->children.empty() && !list->children[0]->children.empty() &&
			ChildOfKindLocal(list->children[0]->children[0], "parameter-clause");
	}
	return false;
}

bool PA18TemplateExpander::MentionsGeneratedType(const CPPGMAstNodePtr& node,
	const string& type_name) const
{
	if(!node || type_name.empty()) return false;
	const string wanted = LastComponent(CanonicalSpelling(RemoveMarker(type_name)));
	string spelling = CanonicalSpelling(RemoveMarker(node->value));
	if(!spelling.empty()) {
		const size_t angle = spelling.find('<');
		const string bare = angle == string::npos ? spelling : spelling.substr(0, angle);
		if(bare == type_name || bare == wanted ||
			bare.compare(0, wanted.size() + 2, wanted + "::") == 0 ||
			bare.find("::" + wanted + "::") != string::npos ||
			(bare.size() > wanted.size() + 2 &&
			 bare.compare(bare.size() - wanted.size() - 2, wanted.size() + 2,
				"::" + wanted) == 0)) return true;
		for(size_t position = spelling.find(wanted); position != string::npos;
			position = spelling.find(wanted, position + wanted.size())) {
			const bool left_boundary = position == 0 ||
				!IsIdentifierCharacter(spelling[position - 1]);
			const size_t end = position + wanted.size();
			const bool right_boundary = end == spelling.size() ||
				!IsIdentifierCharacter(spelling[end]);
			if(left_boundary && right_boundary) return true;
		}
	}
	for(size_t i = 0; i < node->children.size(); ++i)
		if(MentionsGeneratedType(node->children[i], type_name)) return true;
	return false;
}

bool PA18TemplateExpander::MentionsGeneratedClass(const CPPGMAstNodePtr& node,
	const vector<CPPGMAstNodePtr>& generated) const
{
	if(!node) return false;
	for(size_t i = 0; i < generated.size(); ++i) {
		if(!generated[i]) continue;
		const string type_name = LastComponent(generated[i]->value);
		if(type_name.empty()) continue;
		if(MentionsGeneratedType(node, type_name)) {
			return true;
		}
		const string spelling = CanonicalSpelling(RemoveMarker(node->value));
		if(!spelling.empty()) {
			const string resolved = ResolveAlias(spelling, string());
			if(resolved != spelling && MentionsGeneratedType(
				CPPGMAstNodePtr(new CPPGMAstNode("type-name", resolved)), type_name))
				return true;
		}
	}
	for(size_t i = 0; i < node->children.size(); ++i)
		if(MentionsGeneratedClass(node->children[i], generated)) return true;
	return false;
}

bool PA18TemplateExpander::MentionsTemplateId(const CPPGMAstNodePtr& node) const
{
	if(!node) return false;
	if(node->kind == "template-id") return true;
	const string spelling = RemoveMarker(node->value);
	if(spelling.find('<') != string::npos && spelling.find('>') != string::npos)
		return true;
	for(size_t i = 0; i < node->children.size(); ++i)
		if(MentionsTemplateId(node->children[i])) return true;
	return false;
}

vector<CPPGMAstNodePtr> PA18TemplateExpander::OrderGeneratedClasses(
	const vector<CPPGMAstNodePtr>& input) const
{
	vector<CPPGMAstNodePtr> result;
	vector<bool> placed(input.size(), false);
	for(size_t count = 0; count < input.size(); ++count) {
		int selected = -1;
		for(size_t i = 0; i < input.size(); ++i) {
			if(placed[i] || !input[i]) continue;
			bool ready = true;
			for(size_t j = 0; j < input.size(); ++j) {
				if(i == j || placed[j] || !input[j]) continue;
				if(LastComponent(input[i]->value) == LastComponent(input[j]->value)) continue;
				vector<CPPGMAstNodePtr> dependency(1, input[j]);
				if(MentionsGeneratedClass(input[i], dependency)) {
					ready = false;
					break;
				}
			}
			if(ready) {
				selected = static_cast<int>(i);
				break;
			}
		}
		if(selected < 0) {
			size_t best_qualified_dependencies = static_cast<size_t>(-1);
			for(size_t i = 0; i < input.size(); ++i) {
				if(placed[i] || !input[i]) continue;
				size_t qualified_dependencies = 0;
				for(size_t j = 0; j < input.size(); ++j) {
					if(i == j || placed[j] || !input[j]) continue;
					if(MentionsQualifiedGeneratedType(input[i],
						LastComponent(input[j]->value))) ++qualified_dependencies;
				}
				if(selected < 0 || qualified_dependencies < best_qualified_dependencies) {
					selected = static_cast<int>(i);
					best_qualified_dependencies = qualified_dependencies;
				}
			}
		}
		if(selected < 0) break;
		placed[static_cast<size_t>(selected)] = true;
		result.push_back(input[static_cast<size_t>(selected)]);
	}
	return result;
}

void PA18TemplateExpander::InsertGenerated(vector<CPPGMAstNodePtr>* children,
	const string& owner)
{
	if(!children) return;
	map<string, vector<CPPGMAstNodePtr> >::iterator found = generated_by_owner_.find(owner);
	if(found == generated_by_owner_.end() || found->second.empty()) return;
	vector<CPPGMAstNodePtr> generated_classes;
	vector<CPPGMAstNodePtr> generated_functions;
	for(size_t i = 0; i < found->second.size(); ++i) {
		const CPPGMAstNodePtr& generated = found->second[i];
		if(!generated) continue;
		if(generated->kind == "class-specifier" ||
			generated->kind == "class-forward-declaration" ||
			generated->kind == "alias-declaration") generated_classes.push_back(generated);
		else generated_functions.push_back(generated);
	}
	generated_classes = OrderGeneratedClasses(generated_classes);
	vector<CPPGMAstNodePtr> generated_forwards;
	for(size_t i = 0; i < generated_classes.size(); ++i)
		if(generated_classes[i]->kind == "class-specifier" ||
			generated_classes[i]->kind == "class-forward-declaration")
			generated_forwards.push_back(MakeForwardClass(generated_classes[i]->value));
	if(!generated_forwards.empty())
		children->insert(children->begin(), generated_forwards.begin(), generated_forwards.end());

	size_t default_position = 0;
	while(default_position < children->size()) {
		const CPPGMAstNodePtr& child = (*children)[default_position];
		if(child && (child->kind == "function-definition" ||
			child->kind == "special-member-definition")) break;
		if(!TypeOnlyNode(child)) break;
		++default_position;
	}
	if(default_position < children->size() && MentionsTemplateId((*children)[default_position]))
		default_position = children->size();
	set<string> generated_names;
	for(size_t i = 0; i < generated_classes.size(); ++i)
		if(generated_classes[i]) generated_names.insert(generated_classes[i]->value);
	vector<size_t> positions(generated_classes.size(), default_position);
	for(size_t i = 0; i < generated_classes.size(); ++i) {
		const CPPGMAstNodePtr& generated = generated_classes[i];
		if(!generated) continue;
		for(size_t child = 0; child < children->size(); ++child) {
			const CPPGMAstNodePtr& source = (*children)[child];
			if(!source) continue;
			const bool generated_forward =
				source->kind == "class-forward-declaration" &&
				generated_names.find(source->value) != generated_names.end();
			if(generated_forward) continue;
			const string declared = OrderingDeclaredName(source);
			if(OrderingTypeDeclaration(source) && !declared.empty() &&
				generated_names.find(declared) == generated_names.end() &&
				MentionsGeneratedType(generated, declared))
				positions[i] = max(positions[i], child + 1);
			if(MentionsGeneratedType(source, LastComponent(generated->value)))
				positions[i] = min(positions[i], child);
		}
	}
	if(owner.empty()) {
		for(size_t i = 0; i < generated_classes.size(); ++i) {
			for(map<string, vector<CPPGMAstNodePtr> >::const_iterator other =
				generated_by_owner_.begin(); other != generated_by_owner_.end(); ++other) {
				if(other->first.empty()) continue;
				for(size_t dependency = 0; dependency < other->second.size(); ++dependency) {
					if(!other->second[dependency]) continue;
					vector<CPPGMAstNodePtr> dependency_node(1, other->second[dependency]);
					if(!MentionsGeneratedClass(generated_classes[i], dependency_node)) continue;
					for(size_t child = 0; child < children->size(); ++child)
						if((*children)[child] && (*children)[child]->kind == "namespace-definition" &&
							(*children)[child]->value == other->first)
							positions[i] = max(positions[i], child + 1);
				}
			}
		}
	}
	// Keep generated dependencies in the same source slot when their source
	// dependencies would otherwise place the dependent before its materialized
	// prerequisite.  OrderGeneratedClasses already makes the vector topological.
	for(size_t i = 0; i < generated_classes.size(); ++i) {
		for(size_t prior = 0; prior < i; ++prior)
			if(generated_classes[i] && generated_classes[prior] &&
				LastComponent(generated_classes[i]->value) !=
				LastComponent(generated_classes[prior]->value) &&
				MentionsGeneratedType(generated_classes[i],
					LastComponent(generated_classes[prior]->value)))
				positions[prior] = min(positions[prior], positions[i]);
	}
	vector<vector<CPPGMAstNodePtr> > insertions(children->size() + 1);
	for(size_t i = 0; i < generated_classes.size(); ++i) {
		const size_t position = min(positions[i], children->size());
		insertions[position].push_back(generated_classes[i]);
	}
	vector<CPPGMAstNodePtr> reordered;
	reordered.reserve(children->size() + generated_classes.size());
	for(size_t i = 0; i <= children->size(); ++i) {
		reordered.insert(reordered.end(), insertions[i].begin(), insertions[i].end());
		if(i < children->size()) reordered.push_back((*children)[i]);
	}
	children->swap(reordered);

	size_t function_position = children->size();
	for(size_t i = 0; i < children->size(); ++i) {
		const string& kind = (*children)[i]->kind;
		if(kind == "function-definition" || kind == "special-member-definition") {
			function_position = i;
			break;
		}
	}
	if(!generated_functions.empty())
		children->insert(children->begin() + function_position,
			generated_functions.begin(), generated_functions.end());
}

void PA18TemplateExpander::InjectGenerated(const CPPGMAstNodePtr& node,
	const string& context, const string& lexical_context)
{
	if(!node) return;
	if(node->kind == "translation-unit") {
		vector<CPPGMAstNodePtr> namespace_forwards;
		set<string> consumed_forwards;
		for(map<string, vector<CPPGMAstNodePtr> >::const_iterator it =
			generated_namespace_forwards_.begin(); it != generated_namespace_forwards_.end(); ++it) {
			// When the source contains the owner namespace, defer insertion to
			// that namespace's first lexical occurrence.  This matters for an
			// inline namespace: the logical owner `s::c` is lexically reached
			// through `s::i::c`, and a sibling root wrapper would not make a
			// forward visible inside that actual scope.
			if(!it->first.empty() && early_namespace_forwards_.find(it->first) ==
				early_namespace_forwards_.end() && (namespace_occurrences_.find(it->first) !=
				namespace_occurrences_.end() || lexical_namespace_paths_.find(it->first) !=
				lexical_namespace_paths_.end())) continue;
			CPPGMAstNodePtr wrapper = MakeNamespaceForward(it->first, it->second);
			if(wrapper) {
				namespace_forwards.push_back(wrapper);
				synthetic_namespace_forwards_.insert(wrapper.get());
			}
			else node->children.insert(node->children.begin(), it->second.begin(), it->second.end());
			consumed_forwards.insert(it->first);
		}
		for(set<string>::const_iterator it = consumed_forwards.begin();
			it != consumed_forwards.end(); ++it) generated_namespace_forwards_.erase(*it);
		if(!namespace_forwards.empty())
			node->children.insert(node->children.begin(), namespace_forwards.begin(), namespace_forwards.end());
		InsertGenerated(&node->children, context);
		for(size_t i = 0; i < node->children.size(); ++i) {
			if(node->children[i] && node->children[i]->kind == "class-specifier" &&
				node->children[i]->children.size() > 1) {
				const string class_path = JoinPath(context, LastComponent(node->children[i]->value));
				map<string, vector<CPPGMAstNodePtr> >::iterator before =
					generated_before_class_.find(class_path);
				if(before != generated_before_class_.end()) {
					const size_t inserted = before->second.size();
					node->children.insert(node->children.begin() + i,
						before->second.begin(), before->second.end());
					generated_before_class_.erase(before);
					i += inserted;
				}
			}
			InjectGenerated(node->children[i], context, context);
		}
		return;
	}
	if(node->kind == "namespace-definition") {
		if(synthetic_namespace_forwards_.find(node.get()) !=
			synthetic_namespace_forwards_.end()) return;
		const string child_context = IsInlineNamespace(node) || node->value.empty() ?
			context : JoinPath(context, node->value);
		const string child_lexical_context = node->value.empty() ? lexical_context :
			JoinPath(lexical_context, node->value);
		map<string, vector<CPPGMAstNodePtr> >::iterator forwards =
			generated_namespace_forwards_.find(child_context);
		if(forwards == generated_namespace_forwards_.end())
			forwards = generated_namespace_forwards_.find(child_lexical_context);
		if(forwards != generated_namespace_forwards_.end()) {
			node->children.insert(node->children.begin(), forwards->second.begin(),
				forwards->second.end());
			generated_namespace_forwards_.erase(forwards);
		}
		bool last_namespace = true;
		map<string, size_t>::iterator occurrence = namespace_occurrences_.find(child_lexical_context);
		if(occurrence != namespace_occurrences_.end() && occurrence->second > 0)
			last_namespace = --occurrence->second == 0;
		if(last_namespace) InsertGenerated(&node->children, child_lexical_context);
		for(size_t i = 0; i < node->children.size(); ++i) {
			if(node->children[i] && node->children[i]->kind == "class-specifier" &&
				node->children[i]->children.size() > 1) {
				const string class_path = JoinPath(child_context, LastComponent(node->children[i]->value));
				map<string, vector<CPPGMAstNodePtr> >::iterator before =
					generated_before_class_.find(class_path);
				if(before != generated_before_class_.end()) {
					const size_t inserted = before->second.size();
					node->children.insert(node->children.begin() + i,
						before->second.begin(), before->second.end());
					generated_before_class_.erase(before);
					i += inserted;
				}
			}
			InjectGenerated(node->children[i], child_context, child_lexical_context);
		}
		return;
	}
	if(node->kind == "class-specifier" || node->kind == "class-forward-declaration") {
		const string class_context = JoinPath(context, LastComponent(node->value));
		map<string, vector<CPPGMAstNodePtr> >::iterator found =
			generated_by_owner_.find(class_context);
		if(node->kind == "class-specifier" && node->children.size() > 1 &&
			found != generated_by_owner_.end() && !found->second.empty()) {
			vector<CPPGMAstNodePtr> generated_classes;
			vector<CPPGMAstNodePtr> generated_functions;
			for(size_t i = 0; i < found->second.size(); ++i) {
				const CPPGMAstNodePtr& generated = found->second[i];
				if(!generated) continue;
				if(generated->kind == "class-specifier" ||
					generated->kind == "class-forward-declaration" ||
					generated->kind == "alias-declaration") generated_classes.push_back(generated);
				else generated_functions.push_back(generated);
			}
			generated_classes = OrderGeneratedClasses(generated_classes);
			vector<CPPGMAstNodePtr> generated;
			generated.insert(generated.end(), generated_classes.begin(), generated_classes.end());
			generated.insert(generated.end(), generated_functions.begin(), generated_functions.end());
			set<string> complete_names;
			for(size_t i = 0; i < generated_classes.size(); ++i)
				if(generated_classes[i]->kind == "class-specifier" &&
					generated_classes[i]->children.size() > 1)
					complete_names.insert(generated_classes[i]->value);
			if(!complete_names.empty()) {
				for(size_t i = 0; i < node->children.size();) {
					const CPPGMAstNodePtr& child = node->children[i];
					if(child && child->kind == "class-forward-declaration" &&
						complete_names.find(child->value) != complete_names.end())
						node->children.erase(node->children.begin() + i);
					else ++i;
				}
			}
			size_t position = node->children.size();
			for(size_t i = 0; i < node->children.size(); ++i)
				if(node->children[i] && node->children[i]->kind == "class-key") {
					position = i + 1;
					break;
				}
			if(!generated.empty()) node->children.insert(node->children.begin() + position,
				generated.begin(), generated.end());
		}
		for(size_t i = 0; i < node->children.size(); ++i) {
			if(node->children[i] && node->children[i]->kind == "class-specifier" &&
				node->children[i]->children.size() > 1) {
				const string nested_path = JoinPath(class_context,
					LastComponent(node->children[i]->value));
				map<string, vector<CPPGMAstNodePtr> >::iterator before =
					generated_before_class_.find(nested_path);
				if(before != generated_before_class_.end()) {
					const size_t inserted = before->second.size();
					node->children.insert(node->children.begin() + i,
						before->second.begin(), before->second.end());
					generated_before_class_.erase(before);
					i += inserted;
				}
			}
			InjectGenerated(node->children[i], class_context,
				JoinPath(lexical_context, LastComponent(node->value)));
		}
		return;
	}
	for(size_t i = 0; i < node->children.size(); ++i)
		InjectGenerated(node->children[i], context, lexical_context);
}

vector<CPPGMAstNodePtr> ExpandPA18Templates(const vector<CPPGMAstNodePtr>& translation_units)
{
	for(size_t i = 0; i < translation_units.size(); ++i)
		if(NeedsPA18Expansion(translation_units[i])) {
			PA18TemplateExpander expander;
			return expander.Run(translation_units);
		}
	return translation_units;
}
