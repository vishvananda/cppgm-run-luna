#pragma once
#include "pa18_templates.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

using namespace std;

namespace {

bool IsIdentifierCharacter(char ch)
{
	return isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

string Trim(const string& raw)
{
	size_t begin = 0;
	while(begin < raw.size() && isspace(static_cast<unsigned char>(raw[begin]))) ++begin;
	size_t end = raw.size();
	while(end > begin && isspace(static_cast<unsigned char>(raw[end - 1]))) --end;
	return raw.substr(begin, end - begin);
}

string RemoveMarker(const string& raw)
{
	const size_t colon = raw.find(':');
	if(colon == string::npos) return raw;
	const string prefix = raw.substr(0, colon);
	if(prefix == "TT_IDENTIFIER" || prefix.compare(0, 3, "KW_") == 0 ||
		prefix.compare(0, 3, "OP_") == 0)
		return raw.substr(colon + 1);
	return raw;
}

string CanonicalSpelling(string raw)
{
	raw = Trim(raw);
	while(raw.compare(0, 7, "typename") == 0 &&
		(raw.size() == 7 || isspace(static_cast<unsigned char>(raw[7]))))
		raw = Trim(raw.substr(7));
	while(raw.compare(0, 8, "template") == 0 &&
		(raw.size() == 8 || isspace(static_cast<unsigned char>(raw[8]))))
		raw = Trim(raw.substr(8));
	string compact;
	bool previous_space = false;
	for(size_t i = 0; i < raw.size(); ++i) {
		const char ch = raw[i];
		if(isspace(static_cast<unsigned char>(ch))) {
			previous_space = true;
			continue;
		}
		if(previous_space && !compact.empty()) {
			const char prior = compact[compact.size() - 1];
			if(IsIdentifierCharacter(prior) && IsIdentifierCharacter(ch)) compact += ' ';
		}
		previous_space = false;
		compact += ch;
	}
	string result;
	for(size_t i = 0; i < compact.size(); ++i) {
		const char ch = compact[i];
		if((ch == '<' || ch == '>' || ch == ',' || ch == '*' || ch == '&') &&
			!result.empty() && result[result.size() - 1] == ' ')
			result.erase(result.size() - 1);
		result += ch;
		if((ch == '<' || ch == ',' || ch == '>' || ch == '*' || ch == '&') &&
			i + 1 < compact.size() && compact[i + 1] == ' ')
			++i;
	}
	return result;
}

string NormalizeTypeArgument(string raw)
{
	raw = CanonicalSpelling(raw);
	// The PA10 parser preserves some adjacent fundamental tokens as one AST
	// spelling (for example `unsigned long` can arrive as `unsignedlong`).
	// Keep the spelling typed as a fundamental type before it is used as a
	// substitution or specialization key; treating it as an identifier would
	// make an otherwise valid specialization unresolvable later.
	static const char* const compact_fundamentals[][2] = {
		{"unsignedlong", "unsigned long"},
		{"unsignedint", "unsigned int"},
		{"unsignedshort", "unsigned short"},
		{"unsignedchar", "unsigned char"},
		{"signedlong", "signed long"},
		{"signedint", "signed int"},
		{"signedshort", "signed short"},
		{"signedchar", "signed char"},
		{"longlong", "long long"},
		{"longdouble", "long double"}
	};
	for(size_t i = 0; i < sizeof(compact_fundamentals) / sizeof(compact_fundamentals[0]); ++i)
		if(raw == compact_fundamentals[i][0]) {
			raw = compact_fundamentals[i][1];
			break;
		}
	const size_t duplicate_const = raw.find("const const ");
	if(duplicate_const != string::npos) {
		raw.erase(duplicate_const + 6, 6);
		const size_t pointer = raw.rfind('*');
		if(pointer != string::npos && raw.rfind("const") < pointer)
			raw += " const";
	}
	const size_t duplicate_volatile = raw.find("volatile volatile ");
	if(duplicate_volatile != string::npos) {
		raw.erase(duplicate_volatile + 9, 9);
		const size_t pointer = raw.rfind('*');
		if(pointer != string::npos && raw.rfind("volatile") < pointer)
			raw += " volatile";
	}
	// The PA10 name parser intentionally keeps a compact spelling for a
	// qualified template argument in a few contexts (`T const` becomes
	// `Tconst`).  Recover the cv suffix before it reaches semantic type
	// substitution.  This is limited to the two cv words and therefore does
	// not affect ordinary identifiers.
	if(raw.size() > 5 && raw.compare(raw.size() - 5, 5, "const") == 0 &&
		raw.find(' ') == string::npos && raw.find('_') == string::npos)
		raw = "const " + raw.substr(0, raw.size() - 5);
	else if(raw.size() > 8 && raw.compare(raw.size() - 8, 8, "volatile") == 0 &&
		raw.find(' ') == string::npos && raw.find('_') == string::npos)
		raw = "volatile " + raw.substr(0, raw.size() - 8);
	return CanonicalSpelling(raw);
}

string JoinPath(const string& prefix, const string& name)
{
	if(prefix.empty()) return name;
	if(name.empty()) return prefix;
	return prefix + "::" + name;
}

string LastComponent(const string& raw)
{
	const size_t separator = raw.rfind("::");
	return separator == string::npos ? raw : raw.substr(separator + 2);
}

string PrefixComponent(const string& raw)
{
	const size_t separator = raw.rfind("::");
	return separator == string::npos ? string() : raw.substr(0, separator);
}

bool IsInlineNamespace(const CPPGMAstNodePtr& node)
{
	if(!node || node->kind != "namespace-definition") return false;
	for(size_t i = 0; i < node->children.size(); ++i)
		if(node->children[i] && node->children[i]->kind == "inline") return true;
	return false;
}

string FirstIdentifierLocal(const CPPGMAstNodePtr& node)
{
	if(!node) return string();
	if(node->kind == "identifier") return node->value;
	for(size_t i = 0; i < node->children.size(); ++i) {
		const string result = FirstIdentifierLocal(node->children[i]);
		if(!result.empty()) return result;
	}
	return string();
}

CPPGMAstNodePtr ChildOfKindLocal(const CPPGMAstNodePtr& node, const string& kind)
{
	if(!node) return CPPGMAstNodePtr();
	for(size_t i = 0; i < node->children.size(); ++i)
		if(node->children[i] && node->children[i]->kind == kind) return node->children[i];
	return CPPGMAstNodePtr();
}

CPPGMAstNodePtr DescendantOfKind(const CPPGMAstNodePtr& node, const string& kind)
{
	if(!node) return CPPGMAstNodePtr();
	if(node->kind == kind) return node;
	for(size_t i = 0; i < node->children.size(); ++i) {
		CPPGMAstNodePtr result = DescendantOfKind(node->children[i], kind);
		if(result) return result;
	}
	return CPPGMAstNodePtr();
}

CPPGMAstNodePtr CloneNode(const CPPGMAstNodePtr& node)
{
	if(!node) return CPPGMAstNodePtr();
	CPPGMAstNodePtr result(new CPPGMAstNode(node->kind, node->value));
	result->initializer_form = node->initializer_form;
	for(size_t i = 0; i < node->children.size(); ++i)
		result->children.push_back(CloneNode(node->children[i]));
	return result;
}

string SpellNode(const CPPGMAstNodePtr& node)
{
	if(!node) return string();
	if(node->children.empty()) return RemoveMarker(node->value);
	if(node->kind == "type-name" || node->kind == "decl-specifier" ||
		node->kind == "type-specifier" || node->kind == "type-parameter")
		return RemoveMarker(node->value);
	string result;
	for(size_t i = 0; i < node->children.size(); ++i) {
		const string child = SpellNode(node->children[i]);
		if(child.empty()) continue;
		if(!result.empty()) result += ' ';
		result += child;
	}
	return CanonicalSpelling(result);
}

string DefaultTypeSpelling(const CPPGMAstNodePtr& parameter)
{
	const CPPGMAstNodePtr argument = ChildOfKindLocal(parameter, "default-template-argument");
	if(!argument || argument->children.empty()) return string();
	return CanonicalSpelling(SpellNode(argument->children[0]));
}

string ReplaceIdentifiers(const string& raw, const map<string, string>& substitutions)
{
	string result;
	for(size_t i = 0; i < raw.size();) {
		if(IsIdentifierCharacter(raw[i])) {
			size_t end = i + 1;
			while(end < raw.size() && IsIdentifierCharacter(raw[end])) ++end;
			const string word = raw.substr(i, end - i);
			map<string, string>::const_iterator found = substitutions.find(word);
			const bool already_qualified = i >= 2 && result.size() >= 2 &&
				result.compare(result.size() - 2, 2, "::") == 0;
			result += found == substitutions.end() || already_qualified ? word : found->second;
			i = end;
		} else {
			result += raw[i++];
		}
	}
	return result;
}

string TypeSuffix(string raw)
{
	raw = CanonicalSpelling(raw);
	string result;
	for(size_t i = 0; i < raw.size(); ++i) {
		const char ch = raw[i];
		if(IsIdentifierCharacter(ch)) {
			if(!result.empty() && result[result.size() - 1] != '_' && i > 0 &&
				(raw[i - 1] == '*' || raw[i - 1] == '&')) result += '_';
			result += ch;
		}
		else if(ch == ':' && i + 1 < raw.size() && raw[i + 1] == ':') {
			result += "__";
			++i;
		} else if(ch == '*') result += "_ptr";
		else if(ch == '&') result += "_ref";
		else result += '_';
	}
	while(result.size() > 1 && result[result.size() - 1] == '_') result.erase(result.size() - 1);
	return result.empty() ? "arg" : result;
}

vector<string> SplitTemplateArguments(const string& raw)
{
	vector<string> result;
	string current;
	int angle = 0;
	int parentheses = 0;
	int brackets = 0;
	for(size_t i = 0; i < raw.size(); ++i) {
		const char ch = raw[i];
		if(ch == '<') ++angle;
		else if(ch == '>' && angle > 0) --angle;
		else if(ch == '(') ++parentheses;
		else if(ch == ')' && parentheses > 0) --parentheses;
		else if(ch == '[') ++brackets;
		else if(ch == ']' && brackets > 0) --brackets;
		if(ch == ',' && angle == 0 && parentheses == 0 && brackets == 0) {
			result.push_back(CanonicalSpelling(current));
			current.clear();
		} else current += ch;
	}
	if(!current.empty() || !result.empty()) result.push_back(CanonicalSpelling(current));
	if(result.size() == 1 && result[0].empty()) result.clear();
	return result;
}

struct TemplateParameter
{
	string name;
	string default_type;
	bool type;

	TemplateParameter() : name(), default_type(), type(false) {}
};

struct TemplateDefinition
{
	string qualified_name;
	string name;
	string owner;
	CPPGMAstNodePtr declaration;
	vector<TemplateParameter> parameters;
	bool class_template;
	bool alias_template;

	TemplateDefinition() : qualified_name(), name(), owner(), declaration(), parameters(),
		class_template(false), alias_template(false) {}
};

struct FunctionSignature
{
	CPPGMAstNodePtr result_specifiers;
	CPPGMAstNodePtr parameters;
};

class PA18TemplateExpander
{
public:
	vector<CPPGMAstNodePtr> Run(const vector<CPPGMAstNodePtr>& input)
	{
		for(size_t i = 0; i < input.size(); ++i) Collect(input[i], string());
		vector<CPPGMAstNodePtr> result;
		for(size_t i = 0; i < input.size(); ++i) {
			CPPGMAstNodePtr tree = TransformTranslationUnit(input[i]);
			if(tree) result.push_back(tree);
		}
		return result;
	}

private:
	map<string, TemplateDefinition> definitions_;
	map<string, vector<string> > definitions_by_name_;
	map<string, string> specializations_;
	set<string> active_specializations_;
	map<string, vector<CPPGMAstNodePtr> > generated_by_owner_;
	map<string, vector<CPPGMAstNodePtr> > generated_before_class_;
	map<string, vector<CPPGMAstNodePtr> > generated_namespace_forwards_;
	set<string> generated_forward_classes_;
	set<string> class_contexts_;
	set<string> function_contexts_;
	map<string, string> function_owners_;
	map<string, string> local_class_names_;
	map<string, CPPGMAstNodePtr> class_declarations_;
	map<string, string> variable_types_;
	map<string, string> type_aliases_;
	map<string, vector<string> > type_aliases_by_name_;
	map<string, FunctionSignature> function_signatures_;
	map<string, string> specialization_bases_;
	map<string, vector<string> > specialization_arguments_;
	map<string, set<string> > requested_nested_classes_;
	mutable map<string, string> function_markers_;
	mutable map<string, string> function_marker_names_;

	string NodeTypeSpelling(const CPPGMAstNodePtr& sequence) const
	{
		if(!sequence) return string();
		string result;
		for(size_t i = 0; i < sequence->children.size(); ++i) {
			const CPPGMAstNodePtr child = sequence->children[i];
			if(!child || (child->kind == "decl-specifier" &&
				(child->value == "KW_TYPEDEF:typedef" || child->value == "KW_STATIC:static"))) continue;
			if(child->kind != "decl-specifier" && child->kind != "type-name" &&
				child->kind != "type-specifier" && child->kind != "cv-qualifier") continue;
			const string spelling = RemoveMarker(child->value);
			if(spelling.empty()) continue;
			if(!result.empty()) result += ' ';
			result += spelling;
		}
		return CanonicalSpelling(result);
	}

	string QualifyTypeArgument(string spelling, const string& context) const
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
		size_t suffix_begin = spelling.find_first_of("*&");
		string suffix;
		if(suffix_begin != string::npos) {
			suffix = spelling.substr(suffix_begin);
			spelling = CanonicalSpelling(spelling.substr(0, suffix_begin));
		}
		map<string, string>::const_iterator local = local_class_names_.find(
			JoinPath(context, spelling));
		if(local != local_class_names_.end()) spelling = local->second;
		// A cv-qualified pointee can arrive as `Localconst*` after the PA10
		// compact spelling pass.  Treat that suffix as part of the prefix while
		// locating the actual class name.
		if(spelling.size() > 5 && spelling.compare(spelling.size() - 5, 5, "const") == 0 &&
			spelling.find("::") == string::npos) {
			spelling = CanonicalSpelling(spelling.substr(0, spelling.size() - 5));
			prefix = "const ";
		}
		if(spelling.find("::") == string::npos && spelling.find('<') == string::npos) {
			string current = context;
			for(;;) {
				const string candidate = JoinPath(current, spelling);
				if(class_contexts_.find(candidate) != class_contexts_.end()) {
					spelling = candidate;
					break;
				}
				if(current.empty()) break;
				const size_t separator = current.rfind("::");
				if(separator == string::npos) current.clear();
				else current.erase(separator);
			}
			if(class_contexts_.find(spelling) != class_contexts_.end()) {
				// Already a globally visible class name; retain its spelling.
			}
		}
		return CanonicalSpelling(prefix + spelling + suffix);
	}

	string DeclaratorSuffix(const CPPGMAstNodePtr& declarator) const
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

	string ParameterTypeSpelling(const CPPGMAstNodePtr& parameter) const
	{
		if(!parameter || parameter->children.empty()) return string();
		string result = NodeTypeSpelling(parameter->children[0]);
		if(parameter->children.size() > 1) result += DeclaratorSuffix(parameter->children[1]);
		return CanonicalSpelling(result);
	}

	string FunctionTypeSpelling(const CPPGMAstNodePtr& parameter) const
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

	string DeclaratorTypeSpelling(const string& base,
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

	CPPGMAstNodePtr FunctionDeclarator(const CPPGMAstNodePtr& declaration) const
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

	string ResolveAlias(string spelling, const string& context) const
	{
		spelling = CanonicalSpelling(spelling);
		string suffix;
		while(!spelling.empty() && (spelling[spelling.size() - 1] == '*' ||
			spelling[spelling.size() - 1] == '&')) {
			suffix = spelling[spelling.size() - 1] + suffix;
			spelling.erase(spelling.size() - 1);
			spelling = CanonicalSpelling(spelling);
		}
		set<string> seen;
		for(size_t depth = 0; depth < 16; ++depth) {
			if(!seen.insert(spelling).second) break;
			map<string, string>::const_iterator direct = type_aliases_.find(spelling);
			if(direct == type_aliases_.end()) {
				for(string current = context; direct == type_aliases_.end(); ) {
					const string candidate = JoinPath(current, spelling);
					direct = type_aliases_.find(candidate);
					const size_t separator = current.rfind("::");
					if(separator == string::npos) break;
					current.erase(separator);
				}
			}
			if(direct == type_aliases_.end()) {
				const string short_name = LastComponent(spelling);
				map<string, vector<string> >::const_iterator candidates = type_aliases_by_name_.find(short_name);
				if(candidates != type_aliases_by_name_.end() && candidates->second.size() == 1)
					direct = type_aliases_.find(candidates->second[0]);
			}
			if(direct == type_aliases_.end()) break;
			spelling = CanonicalSpelling(direct->second);
		}
		return CanonicalSpelling(spelling + suffix);
	}

	bool ContainsName(const CPPGMAstNodePtr& node, const string& name) const
	{
		if(!node || name.empty()) return false;
		const string value = RemoveMarker(node->value);
		if(value == name || LastComponent(value) == name) return true;
		for(size_t i = 0; i < node->children.size(); ++i)
			if(ContainsName(node->children[i], name)) return true;
		return false;
	}

	string DeclarationName(const CPPGMAstNodePtr& declaration) const
	{
		if(!declaration) return string();
		if(declaration->kind == "class-specifier" ||
			declaration->kind == "class-forward-declaration") return LastComponent(declaration->value);
		if(declaration->kind == "function-definition")
			return LastComponent(FirstIdentifierLocal(declaration->children.size() > 1 ?
				declaration->children[1] : CPPGMAstNodePtr()));
		if(declaration->kind == "simple-declaration") {
			const CPPGMAstNodePtr list = ChildOfKindLocal(declaration, "init-declarator-list");
			if(list && !list->children.empty() && list->children[0] &&
				!list->children[0]->children.empty())
				return LastComponent(FirstIdentifierLocal(list->children[0]->children[0]));
		}
		if(declaration->kind == "alias-declaration") return LastComponent(declaration->value);
		return string();
	}

	void RecordFunctionSignature(const CPPGMAstNodePtr& declaration,
		const string& context)
	{
		if(!declaration) return;
		CPPGMAstNodePtr declarator;
		CPPGMAstNodePtr result_specs;
		if(declaration->kind == "function-definition" && declaration->children.size() > 1) {
			result_specs = declaration->children[0];
			declarator = declaration->children[1];
		} else if(declaration->kind == "simple-declaration" && !declaration->children.empty()) {
			result_specs = declaration->children[0];
			const CPPGMAstNodePtr list = ChildOfKindLocal(declaration, "init-declarator-list");
			if(list && !list->children.empty() && list->children[0] &&
				!list->children[0]->children.empty()) declarator = list->children[0]->children[0];
		}
		if(!declarator || !DescendantOfKind(declarator, "parameter-clause")) return;
		const string name = LastComponent(FirstIdentifierLocal(declarator));
		if(name.empty()) return;
		FunctionSignature signature;
		signature.result_specifiers = CloneNode(result_specs);
		signature.parameters = CloneNode(DescendantOfKind(declarator, "parameter-clause"));
		const string qualified = JoinPath(context, name);
		function_signatures_[qualified] = signature;
	}

	const FunctionSignature* FindFunctionSignature(const string& raw_name,
		const string& context) const
	{
		const string name = LastComponent(raw_name);
		for(string current = context; ; ) {
			map<string, FunctionSignature>::const_iterator found = function_signatures_.find(
				JoinPath(current, raw_name));
			if(found != function_signatures_.end()) return &found->second;
			if(current.empty()) break;
			const size_t separator = current.rfind("::");
			if(separator == string::npos) current.clear();
			else current.erase(separator);
		}
		map<string, FunctionSignature>::const_iterator direct = function_signatures_.find(raw_name);
		if(direct != function_signatures_.end()) return &direct->second;
		const FunctionSignature* result = 0;
		for(map<string, FunctionSignature>::const_iterator it = function_signatures_.begin();
			it != function_signatures_.end(); ++it)
			if(LastComponent(it->first) == name) {
				if(result) return 0;
				result = &it->second;
			}
		return result;
	}

	string FunctionMarker(const string& raw_name, const string& context) const
	{
		const FunctionSignature* signature = FindFunctionSignature(raw_name, context);
		if(!signature) return string();
		const string qualified = JoinPath(context, raw_name);
		map<string, string>::const_iterator existing = function_markers_.find(qualified);
		if(existing != function_markers_.end()) return existing->second;
		string marker = "__PA18_FUNCTION_TYPE_" + LastComponent(raw_name);
		unsigned int suffix = 0;
		for(;; ++suffix) {
			bool collision = false;
			for(map<string, string>::const_iterator it = function_markers_.begin();
				it != function_markers_.end(); ++it)
				if(it->second == marker) collision = true;
			if(!collision) break;
			ostringstream candidate;
			candidate << "__PA18_FUNCTION_TYPE_" << LastComponent(raw_name) << "_" << suffix;
			marker = candidate.str();
		}
		function_markers_[qualified] = marker;
		function_marker_names_[marker] = qualified;
		return marker;
	}

	CPPGMAstNodePtr FunctionParameter(const CPPGMAstNodePtr& original,
		const FunctionSignature& signature, const string& marker) const
	{
		if(!original || original->children.empty() || !signature.result_specifiers ||
			!signature.parameters) return CPPGMAstNodePtr();
		string parameter_name;
		bool reference = false;
		bool rvalue_reference = false;
		if(original->children.size() > 1 && original->children[1]) {
			parameter_name = FirstIdentifierLocal(original->children[1]);
			for(size_t i = 0; i < original->children[1]->children.size(); ++i) {
				const CPPGMAstNodePtr child = original->children[1]->children[i];
				if(!child || child->kind != "ptr-operator") continue;
				if(child->value.find("&") != string::npos) {
					reference = true;
					rvalue_reference = child->value.find("&&") != string::npos;
				}
			}
		}
		if(parameter_name.empty()) parameter_name = "function";
		CPPGMAstNodePtr result(new CPPGMAstNode("parameter-declaration"));
		result->children.push_back(CloneNode(signature.result_specifiers));
		CPPGMAstNodePtr declarator(new CPPGMAstNode("declarator"));
		CPPGMAstNodePtr nested(new CPPGMAstNode("nested-declarator"));
		CPPGMAstNodePtr inner(new CPPGMAstNode("declarator"));
		inner->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("ptr-operator",
			reference ? (rvalue_reference ? "OP_LAND:&&" : "OP_AMP:&") : "OP_STAR:*")));
		inner->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", parameter_name)));
		nested->children.push_back(inner);
		declarator->children.push_back(nested);
		declarator->children.push_back(CloneNode(signature.parameters));
		result->children.push_back(declarator);
		(void)marker;
		return result;
	}

	CPPGMAstNodePtr MakeForwardClass(const string& name) const
	{
		CPPGMAstNodePtr result(new CPPGMAstNode("class-forward-declaration", name));
		result->children.push_back(CPPGMAstNodePtr(
			new CPPGMAstNode("class-key", "KW_STRUCT:struct")));
		return result;
	}

	CPPGMAstNodePtr MakeClassShell(const string& name) const
	{
		CPPGMAstNodePtr result(new CPPGMAstNode("class-specifier", name));
		result->children.push_back(CPPGMAstNodePtr(
			new CPPGMAstNode("class-key", "KW_STRUCT:struct")));
		return result;
	}

	CPPGMAstNodePtr MakeNamespaceForward(const string& owner,
		const vector<CPPGMAstNodePtr>& forwards) const
	{
		if(owner.empty()) return CPPGMAstNodePtr();
		vector<string> parts;
		size_t begin = 0;
		while(begin <= owner.size()) {
			const size_t separator = owner.find("::", begin);
			parts.push_back(owner.substr(begin, separator == string::npos ?
				string::npos : separator - begin));
			if(separator == string::npos) break;
			begin = separator + 2;
		}
		if(parts.empty() || parts[0].empty()) return CPPGMAstNodePtr();
		CPPGMAstNodePtr root(new CPPGMAstNode("namespace-definition", parts[0]));
		CPPGMAstNodePtr current = root;
		for(size_t i = 1; i < parts.size(); ++i) {
			CPPGMAstNodePtr nested(new CPPGMAstNode("namespace-definition", parts[i]));
			current->children.push_back(nested);
			current = nested;
		}
		current->children.insert(current->children.end(), forwards.begin(), forwards.end());
		return root;
	}

	void EnsureForwardClass(const string& spelling, const string& context,
		const string& owner)
	{
		string type = CanonicalSpelling(spelling);
		while(type.compare(0, 6, "const ") == 0)
			type = CanonicalSpelling(type.substr(6));
		while(type.compare(0, 9, "volatile ") == 0)
			type = CanonicalSpelling(type.substr(9));
		while(!type.empty() && (type[type.size() - 1] == '*' ||
			type[type.size() - 1] == '&'))
			type = CanonicalSpelling(type.substr(0, type.size() - 1));
		const size_t separator = type.find("::");
		if(class_contexts_.find(type) == class_contexts_.end())
			return;
		if(separator == string::npos) {
			vector<CPPGMAstNodePtr>& forwards = generated_namespace_forwards_[string()];
			for(size_t i = 0; i < forwards.size(); ++i)
				if(LastComponent(forwards[i]->value) == LastComponent(type)) return;
			forwards.push_back(MakeForwardClass(LastComponent(type)));
			return;
		}
		const string top = type.substr(0, separator);
		if(class_contexts_.find(top) == class_contexts_.end()) {
			const string owner_name = PrefixComponent(type);
			vector<CPPGMAstNodePtr>& forwards = generated_namespace_forwards_[owner_name];
			for(size_t i = 0; i < forwards.size(); ++i)
				if(LastComponent(forwards[i]->value) == LastComponent(type)) return;
			forwards.push_back(MakeForwardClass(LastComponent(type)));
			return;
		}
		if(!generated_forward_classes_.insert(top).second) return;
		CPPGMAstNodePtr forward = MakeClassShell(top);
		string remainder = type.substr(separator + 2);
		CPPGMAstNodePtr current = forward;
		while(!remainder.empty()) {
			const size_t next = remainder.find("::");
			const string part = remainder.substr(0, next);
			if(part.empty()) break;
			CPPGMAstNodePtr nested = next == string::npos ? MakeForwardClass(part) :
				CPPGMAstNodePtr(new CPPGMAstNode("class-specifier", part));
			if(next != string::npos)
				nested->children.push_back(CPPGMAstNodePtr(
					new CPPGMAstNode("class-key", "KW_STRUCT:struct")));
			current->children.push_back(nested);
			current = nested;
			if(next == string::npos) break;
			remainder.erase(0, next + 2);
		}
		if(class_contexts_.find(context) != class_contexts_.end() && context != owner)
			generated_before_class_[context].push_back(forward);
		else
			generated_by_owner_[owner].push_back(forward);
	}

	void EnsureTypeDependency(const string& spelling, const string& context,
		const string& owner)
	{
		const string qualified = QualifyTypeArgument(spelling, context);
		if(!qualified.empty()) EnsureForwardClass(qualified, context, owner);
	}

	void EnsureDeclarationDependencies(const CPPGMAstNodePtr& node,
		const string& context, const string& owner)
	{
		if(!node) return;
		string child_context = context;
		if(node->kind == "class-specifier" || node->kind == "class-forward-declaration")
			child_context = JoinPath(context, LastComponent(node->value));
		if(node->kind == "function-definition" && !node->children.empty()) {
			EnsureTypeDependency(NodeTypeSpelling(node->children[0]), context, owner);
			if(node->children.size() > 1) {
				const CPPGMAstNodePtr clause = DescendantOfKind(node->children[1], "parameter-clause");
				if(clause) for(size_t i = 0; i < clause->children.size(); ++i)
					if(clause->children[i] && clause->children[i]->kind == "parameter-declaration")
						EnsureTypeDependency(ParameterTypeSpelling(clause->children[i]), context, owner);
			}
		}
		if(node->kind == "simple-declaration" && !node->children.empty())
			EnsureTypeDependency(NodeTypeSpelling(node->children[0]), context, owner);
		if(node->kind == "base-name") EnsureTypeDependency(node->value, context, owner);
		for(size_t i = 0; i < node->children.size(); ++i)
			EnsureDeclarationDependencies(node->children[i], child_context, owner);
	}

	vector<TemplateParameter> Parameters(const CPPGMAstNodePtr& clause) const
	{
		vector<TemplateParameter> result;
		if(!clause) return result;
		const CPPGMAstNodePtr list = ChildOfKindLocal(clause, "template-parameter-list");
		if(!list) return result;
		for(size_t i = 0; i < list->children.size(); ++i) {
			const CPPGMAstNodePtr parameter = list->children[i];
			if(!parameter) continue;
			TemplateParameter item;
			item.name = FirstIdentifierLocal(parameter);
			item.default_type = DefaultTypeSpelling(parameter);
			item.type = parameter->kind == "type-parameter";
			result.push_back(item);
		}
		return result;
	}

	void RegisterTemplate(const CPPGMAstNodePtr& node, const string& context)
	{
		if(!node || node->kind != "template-declaration" || node->children.size() < 2) return;
		const CPPGMAstNodePtr declaration = node->children[1];
		const string name = DeclarationName(declaration);
		if(name.empty()) return;
		TemplateDefinition item;
		item.name = name;
		string declared_prefix;
		if(declaration->kind == "class-specifier" ||
			declaration->kind == "class-forward-declaration")
			declared_prefix = PrefixComponent(declaration->value);
		else if(declaration->kind == "function-definition" && declaration->children.size() > 1)
			declared_prefix = PrefixComponent(FirstIdentifierLocal(declaration->children[1]));
		item.owner = JoinPath(context, declared_prefix);
		item.qualified_name = JoinPath(item.owner, name);
		item.declaration = declaration;
		item.parameters = Parameters(node->children[0]);
		item.class_template = declaration->kind == "class-specifier" ||
			declaration->kind == "class-forward-declaration";
		item.alias_template = declaration->kind == "alias-declaration";
		map<string, TemplateDefinition>::iterator prior = definitions_.find(item.qualified_name);
		if(prior != definitions_.end() && !item.class_template && !prior->second.class_template) {
			ostringstream overload_key;
			overload_key << item.qualified_name << "#overload" << definitions_by_name_[item.name].size();
			definitions_[overload_key.str()] = item;
			definitions_by_name_[item.name].push_back(overload_key.str());
			Collect(declaration, item.owner);
			return;
		}
		if(prior != definitions_.end()) {
			for(size_t i = 0; i < item.parameters.size() && i < prior->second.parameters.size(); ++i)
				if(item.parameters[i].default_type.empty())
					item.parameters[i].default_type = prior->second.parameters[i].default_type;
			const bool prior_is_shell = prior->second.declaration &&
				(prior->second.declaration->kind == "class-forward-declaration" ||
				 (prior->second.declaration->kind == "class-specifier" &&
				  prior->second.declaration->children.size() <= 1));
			const bool item_is_definition = declaration->kind == "class-specifier" &&
				declaration->children.size() > 1;
			if(prior_is_shell && item_is_definition) item.declaration = declaration;
			else if(!item_is_definition && prior->second.declaration) item.declaration = prior->second.declaration;
			prior->second.parameters = item.parameters;
			prior->second.declaration = item.declaration;
			prior->second.class_template = item.class_template || prior->second.class_template;
			prior->second.alias_template = item.alias_template || prior->second.alias_template;
		} else {
			definitions_[item.qualified_name] = item;
			definitions_by_name_[item.name].push_back(item.qualified_name);
		}
		// A nested template is looked up in the concrete class scope later.  It
		// is still useful to register its lexical spelling now.
		Collect(declaration, item.class_template ? JoinPath(item.owner, name) : item.owner);
	}

	void Collect(const CPPGMAstNodePtr& node, const string& context)
	{
		if(!node) return;
		if(node->kind == "translation-unit") {
			for(size_t i = 0; i < node->children.size(); ++i) Collect(node->children[i], context);
			return;
		}
		if(node->kind == "namespace-definition") {
			const string child_context = IsInlineNamespace(node) || node->value.empty() ?
				context : JoinPath(context, node->value);
			for(size_t i = 0; i < node->children.size(); ++i)
				if(node->children[i] && node->children[i]->kind != "inline")
					Collect(node->children[i], child_context);
			return;
		}
		if(node->kind == "template-declaration") {
			RegisterTemplate(node, context);
			return;
		}
		if(node->kind == "function-definition") {
			RecordFunctionSignature(node, context);
			const string function_name = DeclarationName(node);
			string function_context = JoinPath(context, function_name);
			if(!function_name.empty() && LastComponent(context) == function_name)
				function_context = context;
			if(!function_name.empty()) {
				function_contexts_.insert(function_context);
				function_owners_[function_context] = context;
			}
			for(size_t i = 0; i < node->children.size(); ++i) {
				const string child_context = node->children[i] &&
					node->children[i]->kind == "compound-statement" ? function_context : context;
				Collect(node->children[i], child_context);
			}
			return;
		}
		if(node->kind == "simple-declaration")
			RecordFunctionSignature(node, context);
		if(node->kind == "alias-declaration" && !node->value.empty() && !node->children.empty()) {
			const string alias = JoinPath(context, node->value);
			const string target = SpellNode(node->children[0]);
			if(!target.empty()) {
				type_aliases_[alias] = target;
				type_aliases_by_name_[node->value].push_back(alias);
			}
		}
		if(node->kind == "simple-declaration" && !node->children.empty() &&
			NodeTypeSpelling(node->children[0]).find("typedef") == string::npos) {
			const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
			if(list) for(size_t i = 0; i < list->children.size(); ++i) {
				const CPPGMAstNodePtr item = list->children[i];
				if(!item || item->children.empty()) continue;
				const CPPGMAstNodePtr declarator = item->children[0];
				const string name = FirstIdentifierLocal(declarator);
				const string base = NodeTypeSpelling(node->children[0]);
				if(name.empty() || base.empty()) continue;
				if(node->children[0]->kind == "decl-specifier-seq" &&
					SpellNode(node->children[0]).find("typedef") != string::npos) {
					const string alias = JoinPath(context, name);
					type_aliases_[alias] = DeclaratorTypeSpelling(base, declarator);
					type_aliases_by_name_[name].push_back(alias);
				}
			}
		}
		string next_context = context;
		if(node->kind == "class-specifier" || node->kind == "class-forward-declaration") {
			const string class_name = LastComponent(node->value);
			next_context = JoinPath(context, class_name);
			class_declarations_[next_context] = node;
			if(LastComponent(context) == class_name) class_declarations_[context] = node;
			if(function_contexts_.find(context) != function_contexts_.end()) {
				const map<string, string>::const_iterator owner = function_owners_.find(context);
				const string function_owner = owner == function_owners_.end() ? PrefixComponent(context) : owner->second;
				const string promoted = JoinPath(function_owner,
					LastComponent(context) + "__" + class_name);
				local_class_names_[next_context] = promoted;
				class_contexts_.insert(promoted);
			} else class_contexts_.insert(next_context);
		}
		for(size_t i = 0; i < node->children.size(); ++i) Collect(node->children[i], next_context);
	}

	void CollectVariables(const CPPGMAstNodePtr& node)
	{
		if(!node) return;
		if(node->kind == "function-definition" && node->children.size() > 1) {
			const CPPGMAstNodePtr parameters = ChildOfKindLocal(node->children[1], "parameter-clause");
			if(parameters) for(size_t i = 0; i < parameters->children.size(); ++i) {
				const CPPGMAstNodePtr parameter = parameters->children[i];
				if(!parameter || parameter->kind != "parameter-declaration" || parameter->children.size() < 2)
					continue;
				const string name = FirstIdentifierLocal(parameter->children[1]);
				if(!name.empty()) variable_types_[name] = ParameterTypeSpelling(parameter);
			}
		}
		if(node->kind == "simple-declaration" && !node->children.empty()) {
			string type;
			const CPPGMAstNodePtr specs = node->children[0];
			for(size_t i = 0; i < specs->children.size(); ++i) {
				const CPPGMAstNodePtr child = specs->children[i];
				if(!child) continue;
				if(child->kind == "decl-specifier" || child->kind == "type-name" ||
					child->kind == "type-specifier") {
					if(!type.empty()) type += ' ';
					type += RemoveMarker(child->value);
				}
			}
			const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
			if(list) for(size_t i = 0; i < list->children.size(); ++i) {
				const CPPGMAstNodePtr item = list->children[i];
				if(!item || item->children.empty()) continue;
				const string name = FirstIdentifierLocal(item->children[0]);
				if(!name.empty() && !type.empty()) variable_types_[name] = CanonicalSpelling(type);
			}
		}
		for(size_t i = 0; i < node->children.size(); ++i) CollectVariables(node->children[i]);
	}

	CPPGMAstNodePtr TransformTranslationUnit(const CPPGMAstNodePtr& input)
	{
		if(!input || input->kind != "translation-unit") return CPPGMAstNodePtr();
		CollectVariables(input);
		CPPGMAstNodePtr result(new CPPGMAstNode("translation-unit"));
		for(size_t i = 0; i < input->children.size(); ++i) {
			CPPGMAstNodePtr child = TransformNode(input->children[i], string(), map<string, string>());
			if(child) result->children.push_back(child);
		}
		InjectGenerated(result, string());
		return result;
	}

	bool TypeOnlyNode(const CPPGMAstNodePtr& node) const
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
		if(node->kind == "simple-declaration") {
			const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
			return list && !list->children.empty() && !list->children[0]->children.empty() &&
				ChildOfKindLocal(list->children[0]->children[0], "parameter-clause");
		}
		return false;
	}

	void InsertGenerated(vector<CPPGMAstNodePtr>* children, const string& owner)
	{
		map<string, vector<CPPGMAstNodePtr> >::iterator found = generated_by_owner_.find(owner);
		if(found == generated_by_owner_.end() || found->second.empty()) return;
		vector<CPPGMAstNodePtr> generated_classes;
		vector<CPPGMAstNodePtr> generated_functions;
		for(size_t i = 0; i < found->second.size(); ++i) {
			if(found->second[i]->kind == "class-specifier" ||
				found->second[i]->kind == "class-forward-declaration" ||
				found->second[i]->kind == "alias-declaration") generated_classes.push_back(found->second[i]);
			else generated_functions.push_back(found->second[i]);
		}
		// A specialization generated in a namespace can be visited before a
		// sibling specialization in the enclosing scope.  Publish lightweight
		// class forwards first, then place the complete generated declarations
		// at the normal type position.  This preserves source declaration order
		// for dependencies such as a generated class containing an earlier
		// ordinary class, while making cross-namespace generated types visible.
		vector<CPPGMAstNodePtr> generated_forwards;
		for(size_t i = 0; i < generated_classes.size(); ++i)
			if(generated_classes[i]->kind == "class-specifier" ||
				generated_classes[i]->kind == "class-forward-declaration")
				generated_forwards.push_back(MakeForwardClass(generated_classes[i]->value));
		if(!generated_forwards.empty())
			children->insert(children->begin(), generated_forwards.begin(), generated_forwards.end());
		size_t type_position = children->size();
		for(size_t i = 0; i < children->size(); ++i) {
			if(!TypeOnlyNode((*children)[i])) {
				type_position = i;
				break;
			}
		}
		if(!generated_classes.empty())
			children->insert(children->begin() + type_position,
				generated_classes.begin(), generated_classes.end());
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

	void InjectGenerated(const CPPGMAstNodePtr& node, const string& context)
	{
		if(!node) return;
		if(node->kind == "translation-unit") {
			vector<CPPGMAstNodePtr> namespace_forwards;
			for(map<string, vector<CPPGMAstNodePtr> >::const_iterator it =
				generated_namespace_forwards_.begin(); it != generated_namespace_forwards_.end(); ++it) {
				CPPGMAstNodePtr wrapper = MakeNamespaceForward(it->first, it->second);
				if(wrapper) namespace_forwards.push_back(wrapper);
				else node->children.insert(node->children.begin(), it->second.begin(), it->second.end());
			}
			if(!namespace_forwards.empty())
				node->children.insert(node->children.begin(), namespace_forwards.begin(), namespace_forwards.end());
			InsertGenerated(&node->children, context);
			for(size_t i = 0; i < node->children.size(); ++i) {
				if(node->children[i] && (node->children[i]->kind == "class-specifier" ||
					node->children[i]->kind == "class-forward-declaration")) {
					const string class_path = JoinPath(context, LastComponent(node->children[i]->value));
					map<string, vector<CPPGMAstNodePtr> >::iterator before =
						generated_before_class_.find(class_path);
					if(before != generated_before_class_.end())
						node->children.insert(node->children.begin() + i,
							before->second.begin(), before->second.end()), i += before->second.size();
				}
				InjectGenerated(node->children[i], context);
			}
			return;
		}
		if(node->kind == "namespace-definition") {
			const string child_context = IsInlineNamespace(node) || node->value.empty() ?
				context : JoinPath(context, node->value);
			// Inline and unnamed namespaces share their semantic lookup context
			// with the enclosing namespace.  The generated declarations therefore
			// belong to the enclosing node; injecting them again into the inline
			// child creates duplicate scopes and corrupts member lookup.
			if(child_context != context) InsertGenerated(&node->children, child_context);
			for(size_t i = 0; i < node->children.size(); ++i) {
				if(node->children[i] && (node->children[i]->kind == "class-specifier" ||
					node->children[i]->kind == "class-forward-declaration")) {
					const string class_path = JoinPath(child_context, LastComponent(node->children[i]->value));
					map<string, vector<CPPGMAstNodePtr> >::iterator before =
						generated_before_class_.find(class_path);
					if(before != generated_before_class_.end())
						node->children.insert(node->children.begin() + i,
							before->second.begin(), before->second.end()), i += before->second.size();
				}
				InjectGenerated(node->children[i], child_context);
			}
			return;
		}
		if(node->kind == "class-specifier" || node->kind == "class-forward-declaration") {
			const string class_context = JoinPath(context, LastComponent(node->value));
			for(size_t i = 0; i < node->children.size(); ++i)
				InjectGenerated(node->children[i], class_context);
			map<string, vector<CPPGMAstNodePtr> >::iterator found = generated_by_owner_.find(class_context);
			if(found != generated_by_owner_.end() && !found->second.empty()) {
				size_t position = node->children.size();
				for(size_t i = 0; i < node->children.size(); ++i)
					if(node->children[i] && node->children[i]->kind == "class-key") {
						position = i + 1;
						break;
					}
				node->children.insert(node->children.begin() + position,
					found->second.begin(), found->second.end());
			}
			return;
		}
		for(size_t i = 0; i < node->children.size(); ++i)
			InjectGenerated(node->children[i], context);
	}
