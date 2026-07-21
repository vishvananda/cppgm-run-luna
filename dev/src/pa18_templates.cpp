#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace pa18_templates_internal;

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
	if(node->kind == "literal" && !node->value.empty() &&
		isdigit(static_cast<unsigned char>(node->value[0])) &&
		node->value.find('_') != string::npos) return true;
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

namespace pa18_templates_internal {

size_t PA18TemplateExpander::EstimateTypeSize(string raw, const string& context) const
{
	raw = CanonicalSpelling(raw);
	while(raw.compare(0, 8, "typename") == 0 &&
		(raw.size() == 8 || isspace(static_cast<unsigned char>(raw[8]))))
		raw = CanonicalSpelling(raw.substr(8));
	while(raw.compare(0, 6, "const ") == 0 ||
		raw.compare(0, 9, "volatile ") == 0) {
		raw = CanonicalSpelling(raw.substr(raw.find(' ') + 1));
	}
	const size_t array_open = raw.find('[');
	if(array_open != string::npos) {
		const size_t array_close = raw.find(']', array_open);
		if(array_close != string::npos) {
			const long count = strtol(raw.substr(array_open + 1,
				array_close - array_open - 1).c_str(), 0, 10);
			if(count >= 0) return EstimateTypeSize(raw.substr(0, array_open), context) *
				static_cast<size_t>(count);
		}
	}
	if(!raw.empty() && (raw[raw.size() - 1] == '*' || raw[raw.size() - 1] == '&')) return 8;
	const PA19IntegralType fundamental = PA19Type(raw);
	if(fundamental.integral) return fundamental.bits <= 8 ? 1 :
		fundamental.bits <= 16 ? 2 : fundamental.bits <= 32 ? 4 : 8;
	map<string, string>::const_iterator alias = type_aliases_.find(raw);
	if(alias != type_aliases_.end() && alias->second != raw)
		return EstimateTypeSize(alias->second, context);
	map<string, size_t>::const_iterator direct = constant_type_sizes_.find(raw);
	if(direct != constant_type_sizes_.end()) return direct->second;
	for(string current = context; ; ) {
		const string candidate = JoinPath(current, raw);
		map<string, size_t>::const_iterator found = constant_type_sizes_.find(candidate);
		if(found != constant_type_sizes_.end()) return found->second;
		if(current.empty()) break;
		const size_t separator = current.rfind("::");
		if(separator == string::npos) current.clear();
		else current.erase(separator);
	}
	return 0;
}

void PA18TemplateExpander::RecordClassTypeSize(const CPPGMAstNodePtr& node,
	const string& context, const string& class_path)
{
	if(!node || (node->kind != "class-specifier" &&
		node->kind != "class-forward-declaration")) return;
	size_t offset = 0;
	size_t alignment = 1;
	for(size_t i = 0; i < node->children.size(); ++i) {
		const CPPGMAstNodePtr child = node->children[i];
		if(!child || child->kind != "simple-declaration" || child->children.empty()) continue;
		const string specifiers = SpellNode(child->children[0]);
		if(specifiers.find("typedef") != string::npos ||
			specifiers.find("static") != string::npos) continue;
		const string base = NodeTypeSpelling(child->children[0]);
		const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
		if(!list) continue;
		for(size_t j = 0; j < list->children.size(); ++j) {
			const CPPGMAstNodePtr item = list->children[j];
			if(!item || item->children.empty()) continue;
			const CPPGMAstNodePtr declarator = item->children[0];
			if(DescendantOfKind(declarator, "parameter-clause")) continue;
			const string spelling = base + DeclaratorSuffix(declarator) +
				DeclaratorArraySuffix(declarator);
			const size_t size = EstimateTypeSize(spelling, class_path);
			if(!size) continue;
			const size_t member_alignment = size > 8 ? 8 : size;
			alignment = max(alignment, member_alignment);
			offset = (offset + member_alignment - 1) / member_alignment * member_alignment;
			offset += size;
		}
	}
	if(!offset) offset = 1;
	offset = (offset + alignment - 1) / alignment * alignment;
	constant_type_sizes_[class_path] = offset;
	constant_type_alignments_[class_path] = alignment;
	const string short_name = LastComponent(class_path);
	if(constant_type_sizes_.find(short_name) == constant_type_sizes_.end()) {
		constant_type_sizes_[short_name] = offset;
		constant_type_alignments_[short_name] = alignment;
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
		map<string, CPPGMAstNodePtr>::const_iterator class_declaration =
			class_declarations_.find(current);
		if(class_declaration != class_declarations_.end() &&
			specialization_bases_.find(LastComponent(current)) != specialization_bases_.end()) {
			const CPPGMAstNodePtr& declaration = class_declaration->second;
			const string member_alias = MemberAliasType(current, spelling);
			if(!member_alias.empty()) {
				spelling = member_alias;
				break;
			}
			for(size_t i = 0; i < declaration->children.size(); ++i)
				if(declaration->children[i] && declaration->children[i]->kind == "enum-specifier" &&
					LastComponent(declaration->children[i]->value) == spelling) {
					spelling = current + "::" + spelling;
					return CanonicalSpelling(prefix + spelling + suffix);
				}
		}
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
		if(!child->children.empty()) result += ConstantExpressionSpelling(child->children[0]);
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
	if(!nested || !clause) return CanonicalSpelling(base + DeclaratorSuffix(declarator) +
		DeclaratorArraySuffix(declarator));
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
	if(declaration->kind == "special-member-definition" ||
		declaration->kind == "special-member-declaration")
		return ChildOfKindLocal(declaration, "declarator");
	if(declaration->kind == "simple-declaration") {
		const CPPGMAstNodePtr list = ChildOfKindLocal(declaration, "init-declarator-list");
		if(list && !list->children.empty() && list->children[0] &&
			!list->children[0]->children.empty()) return list->children[0]->children[0];
	}
	return CPPGMAstNodePtr();
}

bool PA18TemplateExpander::IsBuiltinArithmeticType(string raw) const
{
	raw = CanonicalSpelling(raw);
	while(raw.compare(0, 6, "const ") == 0)
		raw = CanonicalSpelling(raw.substr(6));
	while(raw.compare(0, 9, "volatile ") == 0)
		raw = CanonicalSpelling(raw.substr(9));
	return raw == "bool" || raw == "char" || raw == "signed char" ||
		raw == "unsigned char" || raw == "short" || raw == "short int" ||
		raw == "unsigned short" || raw == "unsigned short int" ||
		raw == "int" || raw == "unsigned" || raw == "unsigned int" ||
	raw == "long" || raw == "long int" || raw == "unsigned long" ||
	raw == "unsigned long int" || raw == "long long" ||
	raw == "long long int" || raw == "unsigned long long" ||
	raw == "unsigned long long int" || raw == "float" ||
	raw == "double" || raw == "long double";
}

string PA18TemplateExpander::CommonBuiltinArithmeticType(const string& left,
	const string& right) const
{
	const string a = CanonicalSpelling(left);
	const string b = CanonicalSpelling(right);
	if(a == b) return a;
	if(a == "long double" || b == "long double") return "long double";
	if(a == "double" || b == "double") return "double";
	if(a == "float" || b == "float") return "float";
	if(a.find("long long") != string::npos || b.find("long long") != string::npos)
		return a.find("unsigned") != string::npos || b.find("unsigned") != string::npos ?
			"unsigned long long int" : "long long int";
	if(a.find("long") != string::npos || b.find("long") != string::npos)
		return a.find("unsigned") != string::npos || b.find("unsigned") != string::npos ?
			"unsigned long int" : "long int";
	return a.find("unsigned") != string::npos || b.find("unsigned") != string::npos ?
		"unsigned int" : "int";
}

bool PA18TemplateExpander::InferOperatorResult(const string& operation,
	const string& left, const string& right, const string& context, string* result) const
{
	if(operation.empty() || !result) return false;
	const string name = "operator" + operation;
	const set<string> no_template_parameters;
	CPPGMAstNodePtr left_declaration = FindClassDeclaration(left, context);
	if(left_declaration) {
		for(size_t i = 0; i < left_declaration->children.size(); ++i) {
			const CPPGMAstNodePtr declaration = left_declaration->children[i];
			if(!declaration || declaration->kind != "function-definition" ||
				declaration->children.size() < 2 ||
				LastComponent(FirstIdentifierLocal(declaration->children[1])) != name) continue;
			const CPPGMAstNodePtr parameters = DescendantOfKind(declaration->children[1],
				"parameter-clause");
			size_t total = 0;
			size_t required = 0;
			if(!FunctionParameterCounts(parameters, &total, &required) || total != 1)
				continue;
			CPPGMAstNodePtr parameter;
			for(size_t p = 0; p < parameters->children.size(); ++p)
				if(parameters->children[p] && parameters->children[p]->kind ==
					"parameter-declaration") {
					parameter = parameters->children[p];
					break;
				}
			if(!parameter) continue;
			map<string, string> inferred;
			if(!MatchTypePattern(ParameterTypeSpelling(parameter), right,
				no_template_parameters, &inferred, context)) continue;
			*result = NormalizeTypeArgument(NodeTypeSpelling(declaration->children[0]) +
				DeclaratorSuffix(declaration->children[1]));
			return !result->empty();
		}
	}
	map<string, vector<string> >::const_iterator names = function_signatures_by_name_.find(name);
	if(names == function_signatures_by_name_.end()) return false;
	for(size_t name_index = 0; name_index < names->second.size(); ++name_index) {
		map<string, FunctionSignature>::const_iterator it = function_signatures_.find(
			names->second[name_index]);
		if(it == function_signatures_.end()) continue;
		const CPPGMAstNodePtr parameters = it->second.parameters;
		size_t total = 0;
		size_t required = 0;
		if(!FunctionParameterCounts(parameters, &total, &required) || total != 2)
			continue;
		CPPGMAstNodePtr first;
		CPPGMAstNodePtr second;
		for(size_t p = 0; p < parameters->children.size(); ++p) {
			const CPPGMAstNodePtr parameter = parameters->children[p];
			if(!parameter || parameter->kind != "parameter-declaration") continue;
			if(!first) first = parameter;
			else {
				second = parameter;
				break;
			}
		}
		if(!first || !second) continue;
		map<string, string> inferred;
		if(!MatchTypePattern(ParameterTypeSpelling(first), left,
			no_template_parameters, &inferred, context) ||
			!MatchTypePattern(ParameterTypeSpelling(second), right,
				no_template_parameters, &inferred, context)) continue;
		*result = NormalizeTypeArgument(NodeTypeSpelling(it->second.result_specifiers));
		return !result->empty();
	}
	return false;
}

bool PA18TemplateExpander::InferTemplateOperatorResult(const string& operation,
	const CPPGMAstNodePtr& left_expression, const CPPGMAstNodePtr& right_expression,
	const map<string, string>& substitutions, const string& context, string* result) const
{
	if(operation.empty() || !left_expression || !right_expression || !result) return false;
	const vector<const TemplateDefinition*> candidates = FindFunctionDefinitions(
		"operator" + operation, context);
	if(candidates.empty()) return false;
	CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
	call->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("id-expression",
		"operator" + operation)));
	CPPGMAstNodePtr arguments(new CPPGMAstNode("argument-list"));
	arguments->children.push_back(left_expression);
	arguments->children.push_back(right_expression);
	call->children.push_back(arguments);
	for(size_t i = 0; i < candidates.size(); ++i) {
		vector<string> inferred;
		if(!InferFunctionArguments(*candidates[i], call, &inferred,
			substitutions, context)) continue;
		if(!candidates[i]->declaration || candidates[i]->declaration->children.empty()) continue;
		string type = NodeTypeSpelling(candidates[i]->declaration->children[0]);
		const CPPGMAstNodePtr declarator = FunctionDeclarator(candidates[i]->declaration);
		type += DeclaratorSuffix(declarator);
		map<string, string> local = substitutions;
		for(size_t parameter = 0; parameter < candidates[i]->parameters.size() &&
			parameter < inferred.size(); ++parameter)
			local[candidates[i]->parameters[parameter].name] = inferred[parameter];
		*result = NormalizeTypeArgument(ResolveAlias(ReplaceIdentifiers(type, local), context));
		return !result->empty();
	}
	return false;
}

bool PA18TemplateExpander::InferBinaryArgument(const CPPGMAstNodePtr& expression,
	string* result, const map<string, string>& substitutions, const string& context) const
{
	if(!expression || expression->children.size() < 2 || !result) return false;
	const string operation = RemoveMarker(expression->value);
	string left;
	string right;
	const bool have_operands = InferArgument(expression->children[0], &left,
		substitutions, context) && InferArgument(expression->children[1], &right,
		substitutions, context);
	if(have_operands && InferOperatorResult(operation, left, right, context, result)) return true;
	if(have_operands && InferTemplateOperatorResult(operation, expression->children[0],
		expression->children[1], substitutions, context, result)) return true;
	if(have_operands && (operation == "&&" || operation == "||" || operation == "==" ||
		operation == "!=" || operation == "<" || operation == ">" ||
		operation == "<=" || operation == ">=") && IsBuiltinLogicalType(left) &&
		IsBuiltinLogicalType(right)) {
		*result = "bool";
		return true;
	}
	if(have_operands && (operation == "+" || operation == "-") &&
		IsBuiltinArithmeticType(left) && IsBuiltinArithmeticType(right)) {
		*result = CommonBuiltinArithmeticType(left, right);
		return true;
	}
	return InferArgument(expression->children[0], result, substitutions, context);
}

CPPGMAstNodePtr PA18TemplateExpander::TransformCallExpression(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions)
{
	CPPGMAstNodePtr result(new CPPGMAstNode(input->kind, input->value));
	result->initializer_form = input->initializer_form;
	result->template_instantiation = input->template_instantiation;
	result->explicit_instantiation = input->explicit_instantiation;
	result->dependent_base_lookup = input->dependent_base_lookup;
	result->template_primary = input->template_primary;
	result->template_arguments = input->template_arguments;
	CPPGMAstNodePtr input_callee = input->children.empty() ? CPPGMAstNodePtr() :
		input->children[0];
	if(input_callee && input_callee->kind == "parenthesized-expression" &&
		input_callee->children.size() == 1 && input_callee->children[0] &&
		input_callee->children[0]->kind == "id-expression")
		input_callee = input_callee->children[0];
	if(input_callee && input_callee->kind == "id-expression") {
		const string raw_callee = input_callee->value;
		string lookup_callee = raw_callee;
		const size_t qualifier_separator = lookup_callee.find("::");
		if(qualifier_separator != string::npos) {
			const map<string, string>::const_iterator alias = substitutions.find(
				lookup_callee.substr(0, qualifier_separator));
			if(alias != substitutions.end()) lookup_callee = alias->second +
				lookup_callee.substr(qualifier_separator);
		}
		const size_t open = lookup_callee.find('<');
		if(open != string::npos) {
			string base;
			size_t begin = 0;
			string argument_text;
			size_t close = string::npos;
			const TemplateDefinition* explicit_definition = 0;
			if(TemplateBase(lookup_callee, open, &begin, &base) &&
				TemplateRange(lookup_callee, open, &argument_text, &close))
				explicit_definition = FindDefinition(base, context);
			if(explicit_definition && !explicit_definition->class_template) {
				vector<string> explicit_args = SplitTemplateArguments(argument_text);
				map<string, string> explicit_substitutions = substitutions;
				for(map<string, PA19IntegralValue>::const_iterator integral =
					active_integral_substitutions_.begin();
					integral != active_integral_substitutions_.end(); ++integral)
					if(integral->second.known)
						explicit_substitutions[integral->first] =
							IntegralValueSpelling(integral->second);
				for(size_t i = 0; i < explicit_args.size(); ++i) {
					explicit_args[i] = NormalizeTypeArgument(RewriteText(
						explicit_args[i], context, explicit_substitutions, 0));
					explicit_args[i] = NormalizeTypeArgument(ReplaceIdentifiers(
						explicit_args[i], explicit_substitutions));
					explicit_args[i] = ResolveAlias(explicit_args[i], context);
					explicit_args[i] = NormalizeTypeArgument(RewriteText(
						explicit_args[i], context, explicit_substitutions, 0));
					explicit_args[i] = ResolveAlias(explicit_args[i], context);
					explicit_args[i] = QualifyTypeArgument(explicit_args[i], context,
						explicit_definition->owner);
				}
				const TemplateDefinition* explicit_specialization =
					FindExplicitFunctionSpecialization(base, explicit_args, context);
				if(explicit_specialization) explicit_definition = explicit_specialization;
				vector<string> complete_args;
				bool has_parameter_pack = false;
				size_t fixed_template_parameters = 0;
				for(size_t parameter = 0; parameter < explicit_definition->parameters.size(); ++parameter)
					if(explicit_definition->parameters[parameter].pack)
						has_parameter_pack = true;
					else ++fixed_template_parameters;
				// Explicit arguments fill a trailing pack only once at least one
				// element beyond the fixed prefix was written.  With just the
				// fixed prefix (`construct<T>(args...)`), the function arguments
				// still deduce the remaining pack.
				const bool explicit_pack_elements = has_parameter_pack &&
					explicit_args.size() > fixed_template_parameters;
				bool complete = explicit_pack_elements ||
					(!has_parameter_pack && explicit_args.size() == explicit_definition->parameters.size());
				if(complete) complete_args = explicit_args;
				else if(explicit_args.size() < explicit_definition->parameters.size())
					complete = InferFunctionArguments(*explicit_definition, input,
						&complete_args, substitutions, context, &explicit_args);
				if(complete) {
					const string local_name = Instantiate(*explicit_definition, complete_args, context);
					result->template_primary = explicit_definition->qualified_name;
					result->template_arguments = complete_args;
					const string qualifier = PrefixComponent(base);
					CPPGMAstNodePtr callee(new CPPGMAstNode("id-expression",
						qualifier.empty() ? local_name : qualifier + "::" + local_name));
					result->children.push_back(callee);
					for(size_t i = 1; i < input->children.size(); ++i) {
						CPPGMAstNodePtr child = TransformNode(input->children[i], context,
							substitutions);
						if(child) result->children.push_back(child);
					}
					return result;
				}
			}
		}
	}
	for(size_t i = 0; i < input->children.size(); ++i) {
		CPPGMAstNodePtr child = TransformNode(input->children[i], context, substitutions);
		if(child) result->children.push_back(child);
	}
	CPPGMAstNodePtr result_callee = result->children.empty() ? CPPGMAstNodePtr() :
		result->children[0];
	if(result_callee && result_callee->kind == "parenthesized-expression" &&
		result_callee->children.size() == 1 && result_callee->children[0] &&
		result_callee->children[0]->kind == "id-expression") {
		result_callee = result_callee->children[0];
		result->children[0] = result_callee;
	}
	// A constructor's function-pointer parameter supplies the expected
	// signature for an otherwise overloaded function template argument.  The
	// class specialization has already been rewritten at this point, so use
	// its concrete constructor declaration before ordinary call deduction.
	ResolveClassConstructorFunctionArguments(result, context);
	if(result_callee && result_callee->kind == "id-expression" &&
		result_callee->value.find('<') == string::npos) {
		const string callee_name = result_callee->value;
		const vector<const TemplateDefinition*> definitions =
			FindFunctionDefinitions(callee_name, context);
		if(!HasMaterializedMemberFunction(callee_name, context) &&
			!HasExactOrdinaryMatch(result, callee_name, substitutions, context))
			for(size_t candidate = 0; candidate < definitions.size(); ++candidate) {
				const TemplateDefinition* definition = definitions[candidate];
				vector<string> inferred;
				map<string, vector<string> > inferred_pack_values;
				const bool inferred_ok = InferFunctionArguments(*definition, result, &inferred,
					substitutions, context, 0, &inferred_pack_values);
				if(!inferred_ok) continue;
				const TemplateDefinition* selected_definition =
					FindExplicitFunctionSpecialization(definition->qualified_name, inferred, context);
				if(!selected_definition) selected_definition = definition;
				const string local_name = Instantiate(*selected_definition, inferred, context, false,
					&inferred_pack_values);
				result->template_primary = definition->qualified_name;
				result->template_arguments = inferred;
				const string qualifier = GeneratedFunctionQualifier(*definition,
					callee_name, context);
				result_callee->value = qualifier.empty() ? local_name : qualifier +
					"::" + local_name;
				break;
			}
		if(definitions.empty()) {
			const FunctionSignature* signature = FindFunctionSignature(callee_name, context);
			if(signature && callee_name.find("::") == string::npos &&
				class_contexts_.find(context) == class_contexts_.end() && substitutions.empty()) {
				map<string, vector<string> >::const_iterator names =
					function_signatures_by_name_.find(LastComponent(callee_name));
				if(names != function_signatures_by_name_.end())
					for(size_t name = 0; name < names->second.size(); ++name) {
						const string& qualified = names->second[name];
						map<string, FunctionSignature>::const_iterator found =
							function_signatures_.find(qualified);
						if(found != function_signatures_.end() && &found->second == signature &&
							class_contexts_.find(PrefixComponent(qualified)) == class_contexts_.end() &&
							function_contexts_.find(PrefixComponent(qualified)) == function_contexts_.end()) {
							result->children[0]->value = qualified;
							break;
						}
					}
			}
			ResolveFunctionArguments(result, signature, context);
		}
	}
	if(!result->children.empty() && result->children[0] &&
		result->children[0]->kind == "id-expression") {
		string& callee = result->children[0]->value;
		const size_t separator = callee.find("::");
		if(separator != string::npos) {
			const string owner = callee.substr(0, separator);
			if(callee.compare(separator + 2, owner.size() + 2, owner + "::") == 0)
				callee.erase(separator + 2, owner.size() + 2);
		}
	}
	return result;
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
	vector<CPPGMAstNodePtr> generated_variables;
	vector<CPPGMAstNodePtr> generated_functions;
	for(size_t i = 0; i < found->second.size(); ++i) {
		const CPPGMAstNodePtr& generated = found->second[i];
		if(!generated) continue;
		if(generated->kind == "class-specifier" ||
			generated->kind == "class-forward-declaration" ||
			generated->kind == "alias-declaration") generated_classes.push_back(generated);
		else if(generated->kind == "simple-declaration") generated_variables.push_back(generated);
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
		if(kind == "static-assert-declaration" || kind == "function-definition" ||
			kind == "special-member-definition") {
			function_position = i;
			break;
		}
	}
	if(!generated_functions.empty())
		children->insert(children->begin() + function_position,
			generated_functions.begin(), generated_functions.end());
	if(!generated_variables.empty()) {
		size_t variable_position = children->size();
		for(size_t i = 0; i < children->size(); ++i) {
			const string& kind = (*children)[i]->kind;
			if(kind == "static-assert-declaration" || kind == "function-definition" ||
				kind == "special-member-definition") {
				variable_position = i;
				break;
			}
		}
		children->insert(children->begin() + variable_position,
			generated_variables.begin(), generated_variables.end());
	}
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

} // namespace pa18_templates_internal

vector<CPPGMAstNodePtr> ExpandPA18Templates(const vector<CPPGMAstNodePtr>& translation_units)
{
	for(size_t i = 0; i < translation_units.size(); ++i)
		if(NeedsPA18Expansion(translation_units[i])) {
			PA18TemplateExpander expander;
			const vector<CPPGMAstNodePtr> result = expander.Run(translation_units);
			return result;
		}
	return translation_units;
}
