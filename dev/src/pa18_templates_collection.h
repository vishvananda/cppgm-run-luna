#pragma once
#include "pa18_templates.h"
#include "pa19_constants.h"
#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
using namespace std;
namespace pa18_templates_internal {
inline bool IsIdentifierCharacter(char ch)
{
	return isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}
inline string Trim(const string& raw)
{
	size_t begin = 0;
	while(begin < raw.size() && isspace(static_cast<unsigned char>(raw[begin]))) ++begin;
	size_t end = raw.size();
	while(end > begin && isspace(static_cast<unsigned char>(raw[end - 1]))) --end;
	return raw.substr(begin, end - begin);
}
inline string RemoveMarker(const string& raw)
{
	const size_t colon = raw.find(':');
	if(colon == string::npos) return raw;
	const string prefix = raw.substr(0, colon);
	if(prefix == "TT_IDENTIFIER" || prefix == "TT_LITERAL" ||
		prefix.compare(0, 3, "KW_") == 0 ||
		prefix.compare(0, 3, "OP_") == 0)
		return raw.substr(colon + 1);
	return raw;
}
inline string CanonicalSpelling(string raw)
{
	raw = Trim(raw);
	while(raw.compare(0, 8, "typename") == 0 &&
		(raw.size() == 8 || isspace(static_cast<unsigned char>(raw[8]))))
		raw = Trim(raw.substr(8));
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
string CollapseRepeatedQualifier(string raw);
string NormalizeTypeArgument(string raw);
inline string PA18ExplicitSpecializationKey(const string& qualified_name,
	const vector<string>& arguments)
{
	string result = qualified_name;
	for(size_t i = 0; i < arguments.size(); ++i)
		result += "|" + NormalizeTypeArgument(arguments[i]);
	return result;
}
inline string JoinPath(const string& prefix, const string& name)
{
	if(prefix.empty()) return name;
	if(name.empty()) return prefix;
	return prefix + "::" + name;
}
inline bool IsTemplateAngleOpen(const string& raw, size_t position);
size_t TopLevelScopeSeparator(const string& raw);
inline string LastComponent(const string& raw)
{
	const size_t separator = TopLevelScopeSeparator(raw);
	return separator == string::npos ? raw : raw.substr(separator + 2);
}
inline string PrefixComponent(const string& raw)
{
	const size_t separator = TopLevelScopeSeparator(raw);
	return separator == string::npos ? string() : raw.substr(0, separator);
}
inline bool IsInlineNamespace(const CPPGMAstNodePtr& node)
{
	if(!node || node->kind != "namespace-definition") return false;
	for(size_t i = 0; i < node->children.size(); ++i)
		if(node->children[i] && node->children[i]->kind == "inline") return true;
	return false;
}
inline string FirstIdentifierLocal(const CPPGMAstNodePtr& node)
{
	if(!node) return string();
	if(node->kind == "identifier") return node->value;
	for(size_t i = 0; i < node->children.size(); ++i) {
		const string result = FirstIdentifierLocal(node->children[i]);
		if(!result.empty()) return result;
	}
	return string();
}
inline CPPGMAstNodePtr ChildOfKindLocal(const CPPGMAstNodePtr& node, const string& kind)
{
	if(!node) return CPPGMAstNodePtr();
	for(size_t i = 0; i < node->children.size(); ++i)
		if(node->children[i] && node->children[i]->kind == kind) return node->children[i];
	return CPPGMAstNodePtr();
}
inline CPPGMAstNodePtr DescendantOfKind(const CPPGMAstNodePtr& node, const string& kind)
{
	if(!node) return CPPGMAstNodePtr();
	if(node->kind == kind) return node;
	for(size_t i = 0; i < node->children.size(); ++i) {
		CPPGMAstNodePtr result = DescendantOfKind(node->children[i], kind);
		if(result) return result;
	}
	return CPPGMAstNodePtr();
}
bool HasFriendSpecifier(const CPPGMAstNodePtr& node);
inline CPPGMAstNodePtr CloneNode(const CPPGMAstNodePtr& node)
{
	if(!node) return CPPGMAstNodePtr();
	CPPGMAstNodePtr result(new CPPGMAstNode(node->kind, node->value));
	result->initializer_form = node->initializer_form;
	result->template_instantiation = node->template_instantiation;
	result->explicit_instantiation = node->explicit_instantiation;
	result->extern_instantiation = node->extern_instantiation;
	result->dependent_base_lookup = node->dependent_base_lookup;
	result->materialize_object_address = node->materialize_object_address;
	result->materialize_object_name = node->materialize_object_name;
	result->inferred_type = node->inferred_type;
	result->source_token_begin = node->source_token_begin; result->source_token_end = node->source_token_end;
	result->template_primary = node->template_primary;
	result->template_arguments = node->template_arguments;
	for(size_t i = 0; i < node->children.size(); ++i)
		result->children.push_back(CloneNode(node->children[i]));
	return result;
}
inline void ReplaceAstIdentifiers(const CPPGMAstNodePtr& node,
	const map<string, string>& replacements)
{
	if(!node) return;
	if(node->kind == "identifier" || node->kind == "id-expression") {
		map<string, string>::const_iterator replacement = replacements.find(
			RemoveMarker(node->value));
		if(replacement != replacements.end()) node->value = replacement->second;
	}
	for(size_t i = 0; i < node->children.size(); ++i)
		ReplaceAstIdentifiers(node->children[i], replacements);
}
inline string SpellNode(const CPPGMAstNodePtr& node)
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
inline string TemplateArgumentSpelling(const CPPGMAstNodePtr& node)
{
	if(!node) return string();
	if(node->kind == "literal" || node->kind == "keyword-literal" ||
		node->kind == "id-expression" || node->kind == "template-id")
		return RemoveMarker(node->value);
	if(node->kind == "parenthesized-expression" && !node->children.empty())
		return "(" + TemplateArgumentSpelling(node->children[0]) + ")";
	if(node->kind == "unary-expression" && !node->children.empty())
		return RemoveMarker(node->value) + TemplateArgumentSpelling(node->children[0]);
	if((node->kind == "binary-expression" || node->kind == "assignment-expression") &&
		node->children.size() >= 2)
		return "(" + TemplateArgumentSpelling(node->children[0]) + " " +
			RemoveMarker(node->value) + " " +
			TemplateArgumentSpelling(node->children[1]) + ")";
	if(node->kind == "conditional-expression" && node->children.size() >= 3)
		return "(" + TemplateArgumentSpelling(node->children[0]) + " ? " +
			TemplateArgumentSpelling(node->children[1]) + " : " +
			TemplateArgumentSpelling(node->children[2]) + ")";
	if((node->kind == "sizeof-expression" || node->kind == "type-trait-expression") &&
		!node->children.empty())
		return (node->kind == "sizeof-expression" ? "sizeof(" : "alignof(") +
			SpellNode(node->children[0]) + ")";
	if(node->kind == "delete-expression" && !node->children.empty())
		return "delete " + TemplateArgumentSpelling(node->children[0]);
	if(node->kind == "cast-expression" && node->children.size() >= 2)
		return "static_cast<" + SpellNode(node->children[0]) + ">(" +
			TemplateArgumentSpelling(node->children[1]) + ")";
	if(node->kind == "subscript-expression" && node->children.size() >= 2)
		return TemplateArgumentSpelling(node->children[0]) + "[" +
			TemplateArgumentSpelling(node->children[1]) + "]";
	if(node->kind == "call-expression" && !node->children.empty()) {
		string result = TemplateArgumentSpelling(node->children[0]) + "(";
		if(node->children.size() > 1 && node->children[1]) {
			const CPPGMAstNodePtr arguments = node->children[1];
			for(size_t i = 0; i < arguments->children.size(); ++i) {
				if(i) result += ", ";
				result += TemplateArgumentSpelling(arguments->children[i]);
			}
		}
		return result + ")";
	}
	if(node->kind == "member-expression" && node->children.size() >= 2)
		return TemplateArgumentSpelling(node->children[0]) +
			RemoveMarker(node->value) + TemplateArgumentSpelling(node->children[1]);
	return SpellNode(node);
}
inline string DefaultTypeSpelling(const CPPGMAstNodePtr& parameter)
{
	const CPPGMAstNodePtr argument = ChildOfKindLocal(parameter, "default-template-argument");
	if(!argument || argument->children.empty()) return string();
	// SpellNode intentionally flattens AST children with spaces, which loses
	// the punctuation of a dependent `decltype(new T(declval<Args>()...))`.
	// Keep the parser's complete decltype spelling so the later pack-aware
	// rewriter can expand the operand before resolving nested template-ids.
	const CPPGMAstNodePtr decltype_spec = DescendantOfKind(
		argument->children[0], "decltype-specifier");
	if(decltype_spec && !decltype_spec->value.empty())
		return CanonicalSpelling(decltype_spec->value);
	return CanonicalSpelling(TemplateArgumentSpelling(argument->children[0]));
}
inline string ReplaceIdentifiers(const string& raw, const map<string, string>& substitutions)
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
			if(found != substitutions.end() && !already_qualified) result += found->second;
			else if(found != substitutions.end()) result += word;
			else {
				bool compact_substitution = false;
				for(map<string,string>::const_iterator it = substitutions.begin();
					it != substitutions.end(); ++it) {
					if(it->first.empty() || word.size() <= it->first.size()) continue;
					if(word.compare(0, it->first.size(), it->first) == 0) {
						const string suffix = word.substr(it->first.size());
						if(suffix == "const" || suffix == "volatile") {
							result += it->second + " " + suffix;
							compact_substitution = true;
							break;
						}
					}
					if(word.compare(word.size() - it->first.size(), it->first.size(),
						it->first) == 0) {
						const string prefix = word.substr(0, word.size() - it->first.size());
						if(prefix == "const" || prefix == "volatile") {
							result += prefix + " " + it->second;
							compact_substitution = true;
							break;
						}
					}
				}
				if(!compact_substitution) result += word;
			}
			i = end;
		} else {
			result += raw[i++];
		}
	}
	return result;
}
string ReplaceIdentifiersPreservingPackSizes(const string& raw,
	const map<string, string>& substitutions);
inline string TypeSuffix(string raw)
{
	raw = CanonicalSpelling(raw);
	const bool array_type = raw.find('[') != string::npos;
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
	while(!array_type && result.size() > 1 && result[result.size() - 1] == '_')
		result.erase(result.size() - 1);
	return result.empty() ? "arg" : result;
}
inline bool IsTemplateAngleOpen(const string& raw, size_t position)
{
	if(position >= raw.size() || raw[position] != '<') return false;
	if(position + 1 < raw.size() && raw[position + 1] == '<') return false;
	size_t previous = position;
	while(previous > 0 && isspace(static_cast<unsigned char>(raw[previous - 1]))) --previous;
	if(previous == 0) return true;
	const char prior = raw[previous - 1];
	// A less-than following a literal, a closed expression, or another less
	// than is an operator (including the first half of <<), not a nested
	// template delimiter.
	if(isalpha(static_cast<unsigned char>(prior)) || prior == '_' ||
		prior == ':' || prior == '>') return true;
	if(isdigit(static_cast<unsigned char>(prior))) {
		size_t begin = previous - 1;
		while(begin > 0 && IsIdentifierCharacter(raw[begin - 1])) --begin;
		for(size_t i = begin; i < previous; ++i)
			if(isalpha(static_cast<unsigned char>(raw[i])) || raw[i] == '_') return true;
	}
	return false;
}
inline bool IsTemplateAngleClose(const string& raw, size_t position)
{
	if(position >= raw.size() || raw[position] != '>') return false;
	size_t previous = position;
	while(previous > 0 && isspace(static_cast<unsigned char>(raw[previous - 1]))) --previous;
	// `<>` is an empty template argument list.  It must close at this `>`;
	// otherwise an adjacent outer `>>` is mistaken for the inner close and the
	// first default argument is rewritten as the literal `>` token.
	if(previous > 0 && raw[previous - 1] == '<') return true;
	if(position + 1 < raw.size() && raw[position + 1] == '=') return false;
	if(position + 1 < raw.size() && raw[position + 1] == '>') {
		size_t after = position + 2;
		while(after < raw.size() && isspace(static_cast<unsigned char>(raw[after]))) ++after;
		if(after < raw.size() && (isalnum(static_cast<unsigned char>(raw[after])) ||
			raw[after] == '_' || raw[after] == '(' || raw[after] == '\'')) return false;
	}
	if(position > 0 && raw[previous - 1] == '>') {
		size_t after = position + 1;
		while(after < raw.size() && isspace(static_cast<unsigned char>(raw[after]))) ++after;
		if(after < raw.size() && (isalnum(static_cast<unsigned char>(raw[after])) ||
			raw[after] == '_' || raw[after] == '(' || raw[after] == '\'')) return false;
	}
	return true;
}
vector<string> SplitTemplateArguments(const string& raw);
struct TemplateParameter
{
	string name;
	string default_type;
	string non_type_type;
	bool type;
	bool pack;
	bool template_template;
	vector<TemplateParameter> template_parameters;
	TemplateParameter() : name(), default_type(), non_type_type(), type(false), pack(false),
		template_template(false), template_parameters() {}
};
struct TemplateDefinition
{
	string qualified_name;
	string name;
	string owner;
	string lexical_owner;
	CPPGMAstNodePtr declaration;
	vector<TemplateParameter> parameters;
	bool partial_specialization;
	vector<string> specialization_parameters;
	vector<TemplateParameter> specialization_parameter_details;
	vector<string> specialization_pack_names;
	vector<string> specialization_pattern;
	bool class_template;
	bool alias_template;
	bool variable_template;
	bool member_template;
	bool friend_declaration;
	TemplateDefinition() : qualified_name(), name(), owner(), lexical_owner(), declaration(), parameters(), partial_specialization(false), specialization_parameters(), specialization_parameter_details(), specialization_pack_names(), specialization_pattern(), class_template(false), alias_template(false), variable_template(false), member_template(false), friend_declaration(false) {}
};
struct FunctionSignature
{
	CPPGMAstNodePtr result_specifiers;
	CPPGMAstNodePtr declarator;
	CPPGMAstNodePtr parameters;
};
class PA18TemplateExpander
{
public:
	vector<CPPGMAstNodePtr> Run(const vector<CPPGMAstNodePtr>& input);
private:
	string StripTemplateArgumentsForValidation(const string& raw) const;
	bool ValidationHasNoexcept(const CPPGMAstNodePtr& node) const;
	void CollectValidationNames(const CPPGMAstNodePtr& node, set<string>& names) const;
	bool ValidationDependentName(const string& raw,
		const set<string>& parameters) const;
	void ValidateTemplateNode(const CPPGMAstNodePtr& node,
		const set<string>& parameters, const set<string>& known_names,
		const string& current_class, bool in_function,
		map<string, bool>& special_members,
		const CPPGMAstNodePtr& parent = CPPGMAstNodePtr(),
		size_t child_index = static_cast<size_t>(-1)) const;
	void ValidateDependentMemberTemplateNode(const CPPGMAstNodePtr& node,
		const set<string>& parameters,
		const map<string, string>& variables) const;
	void ValidateTemplateDiagnostics(const vector<CPPGMAstNodePtr>& input) const;
	void ValidateTemplateDiagnosticsNode(const CPPGMAstNodePtr& node,
		const set<string>& known_names, map<string, bool>& special_members) const;
	bool ValidationTypeArgument(const string& raw,
		const map<string, bool>& parameters) const;
	void ValidateTemplateArgumentKinds(const CPPGMAstNodePtr& node,
		const string& inherited_context,
		const map<string, bool>& inherited_parameters) const;
	map<string, TemplateDefinition> definitions_;
	map<string, vector<string> > definitions_by_name_;
	map<string, vector<pair<string, string> > > using_directive_exports_;
	set<string> template_pack_names_, template_parameter_names_;
	map<string, vector<TemplateDefinition> > class_specializations_;
	map<const CPPGMAstNode*, string> lexical_contexts_;
	set<string> lexical_namespace_paths_;
	map<string, string> lexical_namespace_logical_;
	map<string, string> specializations_;
	set<string> active_specializations_;
	map<string, vector<CPPGMAstNodePtr> > generated_by_owner_;
	map<string, vector<CPPGMAstNodePtr> > deferred_generated_by_owner_;
	map<string, set<string> > deferred_generated_dependencies_;
	map<string, vector<CPPGMAstNodePtr> > generated_before_class_;
	map<string, vector<CPPGMAstNodePtr> > generated_namespace_forwards_;
	set<string> early_namespace_forwards_;
	set<const CPPGMAstNode*> synthetic_namespace_forwards_;
	map<string, size_t> namespace_occurrences_;
	set<string> generated_forward_classes_;
	set<string> class_contexts_;
	set<string> function_contexts_;
	map<string, string> function_owners_;
	map<string, string> local_class_names_; map<string, CPPGMAstNodePtr> class_declarations_; set<string> using_member_template_names_;
	map<string, vector<string> > constant_member_owners_;
	set<string> named_type_contexts_;
	map<string, string> variable_types_;
	map<string, map<string, string> > function_parameter_types_;
	map<string, PA19IntegralValue> constant_values_;
	map<string, vector<PA19IntegralValue> > constant_arrays_; map<string, size_t> constant_type_sizes_, constant_type_alignments_;
	map<string, PA19IntegralValue> active_integral_substitutions_;
	// During replay, the source class context remains dependent while its
	// materialized class is being transformed.  Keep the concrete owner in
	// typed state so an unqualified static member such as `_v` resolves to the
	// current specialization rather than a previous specialization's fallback.
	string active_instantiation_name_;
	// Propagate the concrete enclosing specialization through nested RewriteText
	// calls without smuggling it through the ordinary identifier substitutions.
	string active_concrete_owner_;
	// A template parameter pack is a collection of typed substitutions.  Keep
	// the collection separate from the scalar substitution map so an expanded
	// declaration or call can consume every element without losing the first
	// one to ordinary identifier replacement.
	map<string, vector<string> > active_pack_substitutions_;
	map<string, vector<string> > active_pack_identifier_substitutions_;
	map<string, string> type_aliases_;
	map<string, vector<string> > type_aliases_by_name_;
	map<string, CPPGMAstNodePtr> function_definitions_;
	map<string, FunctionSignature> function_signatures_;
	map<string, vector<string> > function_signatures_by_name_;
	map<string, vector<FunctionSignature> > function_overloads_;
	map<string, string> specialization_bases_;
	map<string, vector<string> > specialization_arguments_;
	map<string, vector<string> > specialization_names_by_base_;
	map<string, TemplateDefinition> explicit_function_specializations_;
	map<const CPPGMAstNode*, vector<string> > explicit_function_arguments_;
	set<string> extern_instantiation_keys_;
	map<string, CPPGMAstNodePtr> extern_instantiation_declarations_;
	map<string, set<string> > requested_nested_classes_;
	set<string> materialized_nested_classes_, materialized_member_definitions_;
	set<string> active_template_member_types_; map<string, FunctionSignature> active_function_substitutions_;
	size_t EstimateTypeSize(string raw, const string& context) const;
	void RecordClassTypeSize(const CPPGMAstNodePtr& node, const string& context,
		const string& class_path);
	string InheritedTypeName(const string& scope, const string& name,
		set<string>* active) const;
	string QualifyTypeArgument(string spelling, const string& context,
		const string& template_owner = string()) const;
	string NormalizeElaboratedSpelling(string raw, const string& context) const;
	string DeclaratorSuffix(const CPPGMAstNodePtr& declarator) const;
	string ReturnDeclaratorSuffix(const CPPGMAstNodePtr& declarator) const;
	string DeclaratorArraySuffix(const CPPGMAstNodePtr& declarator) const;
	string MemberAliasType(const string& class_key, const string& member) const;
	string QualifyNestedMembers(string spelling, const string& class_key,
		const CPPGMAstNodePtr& declaration) const;
	string ParameterTypeSpelling(const CPPGMAstNodePtr& parameter) const;
	string FunctionTypeSpelling(const CPPGMAstNodePtr& parameter) const;
	string DeclaratorTypeSpelling(const string& base,
		const CPPGMAstNodePtr& declarator) const;
	string TypeIdSpelling(const CPPGMAstNodePtr& type_id) const;
	// Keep generated declaration ownership in one typed helper so forwards,
	// class shells, and materialized definitions cannot diverge.
	string GeneratedOwner(const TemplateDefinition& definition) const;
	bool HasReplayContext(const map<string, string>& substitutions) const
	{
		return !substitutions.empty() || !active_concrete_owner_.empty();
	}
	bool PreserveInlineGeneratedOrder(const vector<CPPGMAstNodePtr>& generated_classes,
		const string& owner) const;
	bool HasInlineTemplateCandidate(const vector<const TemplateDefinition*>& definitions,
		const string& context) const;
	bool IsTopLevelPackPattern(const string& value) const;
	CPPGMAstNodePtr FunctionDeclarator(const CPPGMAstNodePtr& declaration) const;
	bool IsBuiltinArithmeticType(string raw) const; bool IsKnownTypeSpelling(string raw, const string& context) const;
	bool HasUnresolvedTemplateParameter(string raw, const string& context,
		const map<string, string>& substitutions) const;
	string CommonBuiltinArithmeticType(const string& left, const string& right) const;
	bool InferOperatorResult(const string& operation, const string& left,
		const string& right, const string& context, string* result) const;
	bool InferTemplateOperatorResult(const string& operation,
		const CPPGMAstNodePtr& left_expression, const CPPGMAstNodePtr& right_expression,
		const map<string, string>& substitutions, const string& context,
		string* result) const;
	bool InferBinaryArgument(const CPPGMAstNodePtr& expression, string* result,
		const map<string, string>& substitutions, const string& context) const;
	bool IsKnownMemberTemplateId(const string& raw) const;
	void CollectInheritedMemberTemplates(const string& raw_class, const string& member,
		const map<string, string>& substitutions, const string& context,
		vector<const TemplateDefinition*>* result, set<string>* active,
		map<const TemplateDefinition*, string>* concrete_owners);
	bool InstantiateMemberCall(const CPPGMAstNodePtr& call,
		const CPPGMAstNodePtr& callee, const string& original_member,
		const string& context,
		const map<string, string>& substitutions,
		bool explicit_instantiation = false);
	bool MaterializeExplicitInstantiation(const CPPGMAstNodePtr& target,
		const string& context, bool extern_instantiation = false);
	CPPGMAstNodePtr TransformCallExpression(const CPPGMAstNodePtr& input,
		const string& context, const map<string, string>& substitutions);
	string ResolveAlias(string spelling, const string& context) const;
	bool ContainsName(const CPPGMAstNodePtr& node, const string& name) const;
	string DeclarationName(const CPPGMAstNodePtr& declaration) const
	{
		if(!declaration) return string();
		if(declaration->kind == "class-specifier" ||
			declaration->kind == "class-forward-declaration") return LastComponent(declaration->value);
		if(declaration->kind == "function-definition")
			return LastComponent(FirstIdentifierLocal(declaration->children.size() > 1 ?
				declaration->children[1] : CPPGMAstNodePtr()));
		if(declaration->kind == "special-member-definition" ||
			declaration->kind == "special-member-declaration")
			return LastComponent(declaration->value);
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
		signature.declarator = CloneNode(declarator);
		signature.parameters = CloneNode(DescendantOfKind(declarator, "parameter-clause"));
		const string qualified = JoinPath(context, name);
		function_overloads_[qualified].push_back(signature);
		if(function_signatures_.find(qualified) == function_signatures_.end())
			function_signatures_by_name_[name].push_back(qualified);
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
		map<string, vector<string> >::const_iterator names =
			function_signatures_by_name_.find(name);
		if(names == function_signatures_by_name_.end() || names->second.size() != 1) return 0;
		map<string, FunctionSignature>::const_iterator found = function_signatures_.find(
			names->second[0]);
		return found == function_signatures_.end() ? 0 : &found->second;
	}
	CPPGMAstNodePtr FunctionParameter(const CPPGMAstNodePtr& original, const FunctionSignature& signature) const;
	CPPGMAstNodePtr MakeForwardClass(const string& name) const;
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
		if(separator != string::npos && class_declarations_.find(type) !=
			class_declarations_.end() && class_contexts_.find(type.substr(0, separator)) !=
			class_contexts_.end()) return;
	if(class_declarations_.find(type) != class_declarations_.end() &&
		class_contexts_.find(context) == class_contexts_.end()) {
			string logical_owner = owner;
			map<string, string>::const_iterator logical = lexical_namespace_logical_.find(owner);
			if(logical != lexical_namespace_logical_.end()) logical_owner = logical->second;
		if(logical_owner == PrefixComponent(type) || owner == PrefixComponent(type)) return;
	}
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
			if(owner != owner_name) early_namespace_forwards_.insert(owner_name);
			for(size_t i = 0; i < forwards.size(); ++i)
				if(LastComponent(forwards[i]->value) == LastComponent(type)) return;
			forwards.push_back(MakeForwardClass(LastComponent(type)));
			return;
		}
		// A dependency which names a nested class of the specialization being
		// materialized is already supplied by that specialization's class shell.
		// Building another shell for the top component here would create a
		// duplicate member class (for example `ptree_int`) beside the real one.
		if(!context.empty() && top == PrefixComponent(context)) return;
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
		const string qualified = QualifyTypeArgument(
			NormalizeElaboratedSpelling(spelling, context), context);
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
			if(parameter->kind == "type-parameter") {
				for(size_t child = parameter->children.size(); child > 0; --child)
					if(parameter->children[child - 1] &&
						parameter->children[child - 1]->kind == "identifier") {
						item.name = parameter->children[child - 1]->value;
						break;
					}
				if(item.name.empty()) item.name = FirstIdentifierLocal(parameter);
			}
			else {
				for(size_t child = 0; child < parameter->children.size(); ++child)
					if(parameter->children[child] &&
						(parameter->children[child]->kind == "declarator" ||
						 parameter->children[child]->kind == "abstract-declarator")) {
						item.name = FirstIdentifierLocal(parameter->children[child]);
						if(!item.name.empty()) break;
					}
				if(item.name.empty()) item.name = FirstIdentifierLocal(parameter);
			}
			item.default_type = DefaultTypeSpelling(parameter);
			item.type = parameter->kind == "type-parameter";
			item.pack = ChildOfKindLocal(parameter, "parameter-pack") != CPPGMAstNodePtr();
			const CPPGMAstNodePtr nested_clause = ChildOfKindLocal(parameter,
				"template-parameter-clause");
			item.template_template = nested_clause != CPPGMAstNodePtr();
			if(item.template_template) item.template_parameters = Parameters(nested_clause);
			if(!item.type && !parameter->children.empty()) {
				item.non_type_type = NodeTypeSpelling(parameter->children[0]);
				for(size_t child = 0; child < parameter->children.size(); ++child)
					if(parameter->children[child] &&
						(parameter->children[child]->kind == "declarator" ||
						 parameter->children[child]->kind == "abstract-declarator"))
						item.non_type_type += DeclaratorSuffix(parameter->children[child]);
				item.non_type_type = CanonicalSpelling(item.non_type_type);
			}
			result.push_back(item);
		}
		return result;
	}
	void CollectLexical(const CPPGMAstNodePtr& node, const string& context,
		const string& logical_context)
	{
		if(!node) return;
		lexical_contexts_[node.get()] = context;
		string child_context = context;
		string child_logical_context = logical_context;
		if(node->kind == "namespace-definition") {
			child_context = node->value.empty() ? context : JoinPath(context, node->value);
			child_logical_context = IsInlineNamespace(node) || node->value.empty() ?
				logical_context : JoinPath(logical_context, node->value);
			lexical_namespace_paths_.insert(child_context);
			lexical_namespace_logical_[child_context] = child_logical_context;
			for(size_t i = 0; i < node->children.size(); ++i)
				if(node->children[i] && node->children[i]->kind != "inline")
					CollectLexical(node->children[i], child_context, child_logical_context);
			return;
		}
		if(node->kind == "class-specifier" || node->kind == "class-forward-declaration")
			child_context = JoinPath(context, LastComponent(node->value));
		for(size_t i = 0; i < node->children.size(); ++i)
			CollectLexical(node->children[i], child_context, child_logical_context);
	}
	void RegisterTemplate(const CPPGMAstNodePtr& node, const string& context,
		bool nested_member_template = false)
	{
		if(!node || node->kind != "template-declaration" || node->children.size() < 2) return;
		const CPPGMAstNodePtr declaration = node->children[1];
		if(declaration && declaration->kind == "template-declaration") {
			RegisterTemplate(declaration, context, true);
			return;
		}
		const string raw_declaration_name = DeclarationName(declaration);
		string name = CanonicalSpelling(raw_declaration_name);
		if(name.empty()) return;
		TemplateDefinition item;
		item.name = name;
		item.partial_specialization = false;
		if(declaration->kind == "class-specifier" ||
			declaration->kind == "class-forward-declaration" ||
			declaration->kind == "simple-declaration") {
			const size_t open = name.find('<');
			if(open != string::npos && name[name.size() - 1] == '>') {
				item.partial_specialization = true;
				const string pattern_text = name.substr(open + 1,
					name.size() - open - 2);
				name = name.substr(0, open);
				item.name = name;
				item.specialization_pattern = SplitTemplateArguments(pattern_text);
				const vector<TemplateParameter> clause_parameters =
					Parameters(node->children[0]);
				item.specialization_parameter_details = clause_parameters;
				for(size_t i = 0; i < clause_parameters.size(); ++i) {
					item.specialization_parameters.push_back(clause_parameters[i].name);
					if(clause_parameters[i].pack)
						item.specialization_pack_names.push_back(clause_parameters[i].name);
				}
			}
		}
		// A qualified out-of-class declarator carries its owner in the
		// declarator name (for example `box<T>::cast`).  The owner is indexed
		// separately below; retaining the full spelling in `item.name` would
		// duplicate it in the registry key and make the definition look like a
		// different member from its in-class declaration.
		name = LastComponent(name);
		string declared_prefix;
		if(declaration->kind == "class-specifier" ||
			declaration->kind == "class-forward-declaration")
			declared_prefix = PrefixComponent(declaration->value);
		else if(declaration->kind == "function-definition" && declaration->children.size() > 1)
			declared_prefix = PrefixComponent(FirstIdentifierLocal(declaration->children[1]));
		else if(declaration->kind == "special-member-definition" ||
			declaration->kind == "special-member-declaration")
			declared_prefix = PrefixComponent(declaration->value);
		else if(declaration->kind == "simple-declaration") {
			const CPPGMAstNodePtr list = ChildOfKindLocal(declaration, "init-declarator-list");
			if(list && !list->children.empty() && list->children[0] &&
				!list->children[0]->children.empty())
				declared_prefix = PrefixComponent(FirstIdentifierLocal(
					list->children[0]->children[0]));
		}
	const bool rooted_prefix = declared_prefix.compare(0, 2, "::") == 0;
	const string normalized_prefix = rooted_prefix ? declared_prefix.substr(2) : declared_prefix;
	item.owner = rooted_prefix ? normalized_prefix : JoinPath(context, normalized_prefix);
	map<const CPPGMAstNode*, string>::const_iterator lexical = lexical_contexts_.find(node.get());
	item.lexical_owner = lexical == lexical_contexts_.end() || rooted_prefix ? item.owner :
		JoinPath(lexical->second, normalized_prefix);
	item.qualified_name = JoinPath(item.owner, name);
	item.declaration = declaration;
	item.friend_declaration = declaration && !declaration->children.empty() && HasFriendSpecifier(declaration->children[0]);
		item.parameters = Parameters(node->children[0]);
		for(size_t parameter = 0; parameter < item.parameters.size(); ++parameter)
			if(!item.parameters[parameter].name.empty()) {
				template_parameter_names_.insert(item.parameters[parameter].name);
				if(item.parameters[parameter].pack)
					template_pack_names_.insert(item.parameters[parameter].name);
			}
		for(size_t parameter = 0; parameter < item.specialization_pack_names.size(); ++parameter)
			template_pack_names_.insert(item.specialization_pack_names[parameter]);
		item.class_template = declaration->kind == "class-specifier" ||
			declaration->kind == "class-forward-declaration";
		item.alias_template = declaration->kind == "alias-declaration";
		item.variable_template = declaration->kind == "simple-declaration" &&
			DescendantOfKind(declaration, "parameter-clause") == CPPGMAstNodePtr();
		item.member_template = nested_member_template ||
			(!item.class_template && class_declarations_.find(context) !=
				class_declarations_.end());
		// `template<>` function declarations are explicit specializations, not
		// overloads of the primary template.  Keep their concrete body in typed
		// state so a later call can select it after normal template deduction.
		if(item.parameters.empty() && !item.class_template &&
			(declaration->kind == "function-definition" ||
			 declaration->kind == "simple-declaration")) {
			string specialization_base;
			vector<string> specialization_arguments;
			const size_t open = name.find('<');
			string argument_text;
			size_t begin = 0, close = string::npos;
			if(open != string::npos && TemplateBase(name, open, &begin,
				&specialization_base) && TemplateRange(name, open, &argument_text, &close))
				specialization_arguments = SplitTemplateArguments(argument_text);
			else specialization_base = name;
			const TemplateDefinition* primary = FindDefinition(specialization_base, context);
			if(primary && !primary->class_template && !primary->parameters.empty()) {
				if(specialization_arguments.empty()) {
					const CPPGMAstNodePtr primary_declarator = FunctionDeclarator(primary->declaration);
					const CPPGMAstNodePtr primary_parameters =
						DescendantOfKind(primary_declarator, "parameter-clause");
					const CPPGMAstNodePtr specialized_declarator = FunctionDeclarator(declaration);
					const CPPGMAstNodePtr specialized_parameters =
						DescendantOfKind(specialized_declarator, "parameter-clause");
					set<string> parameter_names;
					for(size_t parameter = 0; parameter < primary->parameters.size(); ++parameter)
						if(!primary->parameters[parameter].name.empty()) parameter_names.insert(
							primary->parameters[parameter].name);
					map<string, string> inferred;
					if(primary_parameters && specialized_parameters &&
						primary_parameters->children.size() == specialized_parameters->children.size())
						for(size_t parameter = 0; parameter < primary_parameters->children.size(); ++parameter) {
							const CPPGMAstNodePtr primary_parameter = primary_parameters->children[parameter];
							const CPPGMAstNodePtr specialized_parameter = specialized_parameters->children[parameter];
							if(!primary_parameter || !specialized_parameter ||
								!MatchTypePattern(ParameterTypeSpelling(primary_parameter),
									ParameterTypeSpelling(specialized_parameter), parameter_names,
									&inferred, context)) {
								inferred.clear();
								break;
							}
						}
					if(!inferred.empty()) for(size_t parameter = 0;
						parameter < primary->parameters.size(); ++parameter) {
							map<string, string>::const_iterator found = inferred.find(
								primary->parameters[parameter].name);
							if(found == inferred.end()) { inferred.clear(); break; }
							specialization_arguments.push_back(found->second);
						}
				}
				if(specialization_arguments.size() == primary->parameters.size()) {
					item.name = primary->name;
					item.owner = primary->owner;
					item.lexical_owner = primary->lexical_owner;
					item.qualified_name = primary->qualified_name;
					item.parameters = primary->parameters;
					item.declaration = CloneNode(declaration);
					const CPPGMAstNodePtr primary_declarator = FunctionDeclarator(primary->declaration);
					const CPPGMAstNodePtr specialized_declarator = FunctionDeclarator(declaration);
					const CPPGMAstNodePtr primary_parameters =
						DescendantOfKind(primary_declarator, "parameter-clause");
					const CPPGMAstNodePtr specialized_parameters =
						DescendantOfKind(specialized_declarator, "parameter-clause");
					map<string, string> parameter_renames;
					if(primary_parameters && specialized_parameters)
						for(size_t parameter = 0; parameter < primary_parameters->children.size() &&
							parameter < specialized_parameters->children.size(); ++parameter) {
							const string primary_name = FirstIdentifierLocal(
								primary_parameters->children[parameter]);
							const string specialized_name = FirstIdentifierLocal(
								specialized_parameters->children[parameter]);
							if(!primary_name.empty() && !specialized_name.empty() &&
								primary_name != specialized_name)
								parameter_renames[specialized_name] = primary_name;
						}
					ReplaceAstIdentifiers(item.declaration, parameter_renames);
					explicit_function_arguments_[declaration.get()] = specialization_arguments;
					explicit_function_specializations_[PA18ExplicitSpecializationKey(
						primary->qualified_name, specialization_arguments)] = item;
					return;
				}
			}
		}
		map<string, TemplateDefinition>::iterator prior = definitions_.find(item.qualified_name);
		if(item.partial_specialization) {
			// A partial specialization is selected from the primary only after
			// its concrete arguments are known.  Keep the primary's parameter
			// contract for materialization, while retaining the specialization's
			// own names/pattern for matching and body substitution.
			if(prior != definitions_.end())
				item.parameters = prior->second.parameters;
			class_specializations_[item.qualified_name].push_back(item);
			Collect(declaration, JoinPath(item.owner, item.name));
			return;
		}
		if(prior != definitions_.end() && !item.class_template && !prior->second.class_template) {
			ostringstream overload_key;
			overload_key << item.qualified_name << "#overload" << definitions_by_name_[item.name].size();
			definitions_[overload_key.str()] = item;
			definitions_by_name_[item.name].push_back(overload_key.str());
			IndexUsingDirectiveDefinition(item, overload_key.str());
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
			prior->second.lexical_owner = item.lexical_owner;
			prior->second.class_template = item.class_template || prior->second.class_template;
			prior->second.alias_template = item.alias_template || prior->second.alias_template;
		} else {
			definitions_[item.qualified_name] = item;
			definitions_by_name_[item.name].push_back(item.qualified_name);
			IndexUsingDirectiveDefinition(item, item.qualified_name);
		}
		// A nested template is looked up in the concrete class scope later.  It
		// is still useful to register its lexical spelling now.
		Collect(declaration, item.class_template ? JoinPath(item.owner, name) : item.owner);
	}
	void IndexConstantMembers(const CPPGMAstNodePtr& node, const string& owner); void IndexUsingDirectiveDefinition(const TemplateDefinition& definition, const string& key);
	void Collect(const CPPGMAstNodePtr& node, const string& context)
	{
		if(!node) return;
		if(node->kind == "using-declaration" && class_contexts_.find(context) != class_contexts_.end()) { const CPPGMAstNodePtr target = ChildOfKindLocal(node, "target"); if(target) using_member_template_names_.insert(LastComponent(target->value)); }
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
			if(!function_name.empty()) {
				const string qualified_name = JoinPath(context, function_name);
				// Keep the first ordinary overload available to the constexpr source
				// evaluator.  A templated overload is collected through its template
				// declaration afterwards and must not hide a zero-argument/base-case
				// function under the same spelling.
				if(function_definitions_.find(qualified_name) == function_definitions_.end())
					function_definitions_[qualified_name] = node;
			}
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
		if(node->kind == "simple-declaration")
			RecordConstantDeclaration(node, context);
		if(node->kind == "enum-specifier")
			RecordEnumConstants(node, context);
		if(node->kind == "alias-declaration" && !node->value.empty() && !node->children.empty()) {
			const string alias = JoinPath(context, node->value);
			const string target = TypeIdSpelling(node->children[0]);
			if(!target.empty()) {
				type_aliases_[alias] = target;
				type_aliases_by_name_[node->value].push_back(alias);
			}
		}
		if(node->kind == "simple-declaration" && !node->children.empty() &&
			SpellNode(node->children[0]).find("typedef") != string::npos) {
			const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
			if(list) for(size_t i = 0; i < list->children.size(); ++i) {
				const CPPGMAstNodePtr item = list->children[i];
				if(!item || item->children.empty()) continue;
				const CPPGMAstNodePtr declarator = item->children[0];
				const string name = FirstIdentifierLocal(declarator);
				const string base = NodeTypeSpelling(node->children[0]);
				if(name.empty() || base.empty()) continue;
				const string alias = JoinPath(context, name);
				type_aliases_[alias] = DeclaratorTypeSpelling(base, declarator);
				type_aliases_by_name_[name].push_back(alias);
			}
		}
		string next_context = context;
		if(node->kind == "class-specifier" || node->kind == "class-forward-declaration") {
			const string class_name = LastComponent(node->value);
			next_context = JoinPath(context, class_name);
			class_declarations_[next_context] = node;
			IndexConstantMembers(node, next_context);
			if(LastComponent(context) == class_name) {
				class_declarations_[context] = node;
				IndexConstantMembers(node, context);
			}
			if(function_contexts_.find(context) != function_contexts_.end()) {
				const map<string, string>::const_iterator owner = function_owners_.find(context);
				const string function_owner = owner == function_owners_.end() ? PrefixComponent(context) : owner->second;
				const string promoted = JoinPath(function_owner,
					LastComponent(context) + "__" + class_name);
				local_class_names_[next_context] = promoted;
				class_contexts_.insert(promoted);
			} else class_contexts_.insert(next_context);
		}
		if(node->kind == "enum-specifier" && !node->value.empty())
			named_type_contexts_.insert(JoinPath(context, LastComponent(node->value)));
		for(size_t i = 0; i < node->children.size(); ++i) Collect(node->children[i], next_context);
		if(node->kind == "class-specifier" || node->kind == "class-forward-declaration")
			RecordClassTypeSize(node, context, JoinPath(context, LastComponent(node->value)));
	}
	void CollectVariables(const CPPGMAstNodePtr& node)
	{
		if(!node) return;
		if((node->kind == "function-definition" ||
			node->kind == "special-member-definition") && node->children.size() > 1) {
			const CPPGMAstNodePtr declarator = node->kind == "function-definition" ?
				node->children[1] : ChildOfKindLocal(node, "declarator");
			const CPPGMAstNodePtr parameters = DescendantOfKind(declarator,
				"parameter-clause");
			if(parameters) for(size_t i = 0; i < parameters->children.size(); ++i) {
				const CPPGMAstNodePtr parameter = parameters->children[i];
				if(!parameter || parameter->kind != "parameter-declaration" || parameter->children.size() < 2)
					continue;
				const string name = FirstIdentifierLocal(parameter->children[1]);
				if(!name.empty()) variable_types_[name] = ParameterTypeSpelling(parameter);
			}
		}
		if(node->kind == "simple-declaration") {
			const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
			if(list) for(size_t i = 0; i < list->children.size(); ++i) {
				const CPPGMAstNodePtr item = list->children[i];
				if(!item || item->children.empty()) continue;
				const CPPGMAstNodePtr clause = DescendantOfKind(item->children[0],
					"parameter-clause");
				if(!clause) continue;
				for(size_t j = 0; j < clause->children.size(); ++j) {
					const CPPGMAstNodePtr parameter = clause->children[j];
					if(!parameter || parameter->kind != "parameter-declaration" ||
						parameter->children.size() < 2) continue;
					const string name = FirstIdentifierLocal(parameter->children[1]);
					if(!name.empty()) variable_types_[name] = ParameterTypeSpelling(parameter);
				}
			}
		}
		if(node->kind == "simple-declaration" && !node->children.empty()) {
			const CPPGMAstNodePtr specs = node->children[0];
			const string type = NodeTypeSpelling(specs);
			const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
			if(list) for(size_t i = 0; i < list->children.size(); ++i) {
				const CPPGMAstNodePtr item = list->children[i];
				if(!item || item->children.empty()) continue;
				const string name = FirstIdentifierLocal(item->children[0]);
				if(!name.empty() && !type.empty())
					variable_types_[name] = DeclaratorTypeSpelling(type, item->children[0]);
			}
		}
		for(size_t i = 0; i < node->children.size(); ++i) CollectVariables(node->children[i]);
	}
	void CountNamespaceOccurrences(const CPPGMAstNodePtr& node, const string& context);
	CPPGMAstNodePtr TransformTranslationUnit(const CPPGMAstNodePtr& input);
	bool TypeOnlyNode(const CPPGMAstNodePtr& node) const;
	void InsertGenerated(vector<CPPGMAstNodePtr>* children, const string& owner);
	void InjectGenerated(const CPPGMAstNodePtr& node, const string& context,
		const string& lexical_context);
	bool HasExternalCompleteDependency(const CPPGMAstNodePtr& node,
		const string& owner, set<string>* dependencies) const;
	bool DeclaresSourceType(const CPPGMAstNodePtr& node, const set<string>& names) const;
	void InsertDeferredGenerated(const CPPGMAstNodePtr& node);
	bool MentionsGeneratedType(const CPPGMAstNodePtr& node,
		const string& type_name) const;
	bool MentionsGeneratedClass(const CPPGMAstNodePtr& node,
		const vector<CPPGMAstNodePtr>& generated) const;
	bool MentionsGeneratedLayoutClass(const CPPGMAstNodePtr& node,
		const vector<CPPGMAstNodePtr>& generated) const;
	bool MentionsTemplateId(const CPPGMAstNodePtr& node) const;
	vector<CPPGMAstNodePtr> OrderGeneratedClasses(const vector<CPPGMAstNodePtr>& input) const;
