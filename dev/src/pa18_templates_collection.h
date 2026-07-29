#pragma once
#include "pa18_templates.h"
#include "pa19_constants.h"
#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
using namespace std; namespace pa18_templates_internal {
class PA18SubstitutionFailure : public logic_error { public: explicit PA18SubstitutionFailure(const string& message) : logic_error(message) {} };
inline bool IsIdentifierCharacter(char ch) {
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
			const bool cv_after_pointer = (prior == '*' || prior == '&') &&
				((raw.compare(i, 5, "const") == 0 &&
					(i + 5 == raw.size() || !IsIdentifierCharacter(raw[i + 5]))) ||
				 (raw.compare(i, 8, "volatile") == 0 &&
					(i + 8 == raw.size() || !IsIdentifierCharacter(raw[i + 8]))));
			if((IsIdentifierCharacter(prior) && IsIdentifierCharacter(ch)) || cv_after_pointer)
				compact += ' ';
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
			i + 1 < compact.size() && compact[i + 1] == ' ') {
			const size_t next = i + 2;
			const bool cv_after_pointer = (ch == '*' || ch == '&') &&
				((compact.compare(next, 5, "const") == 0 &&
					(next + 5 == compact.size() || !IsIdentifierCharacter(compact[next + 5]))) ||
				 (compact.compare(next, 8, "volatile") == 0 &&
					(next + 8 == compact.size() || !IsIdentifierCharacter(compact[next + 8]))));
			if(!cv_after_pointer) ++i;
		}
	}
	return result;
}
string CollapseRepeatedQualifier(string raw);
string CollapseRepeatedQualifiedPath(string value);
string NormalizeTypeArgument(string raw);
string PA18ExplicitSpecializationKey(const string& qualified_name,
	const vector<string>& arguments);
inline string JoinPath(const string& prefix, const string& name)
{
	if(prefix.empty()) return name;
	if(name.empty()) return prefix;
	return prefix + "::" + name;
}
inline bool IsTemplateAngleOpen(const string& raw, size_t position);
inline bool IsTemplateAngleClose(const string& raw, size_t position);
bool LooksLikeRelationalLessThan(const string& raw, size_t position);
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
bool HasDeclarationSpecifier(const CPPGMAstNodePtr& node, const string& wanted);
inline CPPGMAstNodePtr CloneNode(const CPPGMAstNodePtr& node)
{
	if(!node) return CPPGMAstNodePtr();
	CPPGMAstNodePtr result(new CPPGMAstNode(node->kind, node->value));
	result->initializer_form = node->initializer_form;
	result->template_instantiation = node->template_instantiation;
	result->explicit_specialization = node->explicit_specialization;
	result->explicit_instantiation = node->explicit_instantiation; result->extern_instantiation = node->extern_instantiation;
	result->synthetic_namespace_forward = node->synthetic_namespace_forward; result->dependent_base_lookup = node->dependent_base_lookup;
	result->materialize_object_address = node->materialize_object_address; result->has_deferred_constructor = node->has_deferred_constructor;
	result->materialize_object_name = node->materialize_object_name;
	result->inferred_type = node->inferred_type;
	result->explicit_typename = node->explicit_typename;
	result->source_token_begin = node->source_token_begin; result->source_token_end = node->source_token_end;
	result->template_primary = node->template_primary;
	result->template_arguments = node->template_arguments;
	result->template_empty_pack = node->template_empty_pack;
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
	if(node->kind == "sizeof-pack-expression" && !node->children.empty()) return "sizeof...(" + TemplateArgumentSpelling(node->children[0]) + ")";
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
					if(word.compare(0, 8, "typename") == 0 && word.size() > 8 && word.substr(8) == it->first) { result += "typename " + it->second; compact_substitution = true; break; }
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
inline string TypeSuffix(string raw, bool preserve_trailing_underscores = false)
{
	raw = CanonicalSpelling(raw);
	// Keep rvalue references as one generated-name component.  Replaying the
	// older `_ref_ref` spelling loses the reference category and can turn the
	// second `_ref` into a nominal type during nested constructor replay.
	string collapsed;
	for(size_t i = 0; i < raw.size(); ++i) {
		if(i + 1 < raw.size() && raw[i] == '&' && raw[i + 1] == '&') {
			collapsed += "&&";
			++i;
		} else collapsed += raw[i];
	}
	raw.swap(collapsed);
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
		else if(ch == '&') {
			if(i + 1 < raw.size() && raw[i + 1] == '&') {
				result += "_rref";
				++i;
			} else result += "_ref";
		}
		else result += '_';
	}
	while(!array_type && !preserve_trailing_underscores && result.size() > 1 &&
		result[result.size() - 1] == '_')
		result.erase(result.size() - 1);
	return result.empty() ? "arg" : result;
}
inline bool IsTemplateAngleOpen(const string& raw, size_t position)
{
	if(position >= raw.size() || raw[position] != '<') return false;
	if(position + 1 < raw.size() && raw[position + 1] == '<') return false;
	if(position + 1 < raw.size() && raw[position + 1] == '=') return false;
	size_t previous = position;
	while(previous > 0 && isspace(static_cast<unsigned char>(raw[previous - 1]))) --previous;
	if(previous == 0) return true;
	const char prior = raw[previous - 1];
	// A less-than following a literal, a closed expression, or another less
	// than is an operator (including the first half of <<), not a nested
	// template delimiter.
	if(isalpha(static_cast<unsigned char>(prior)) || prior == '_' ||
		prior == ':' || prior == '>') {
		if(LooksLikeRelationalLessThan(raw, position)) return false;
		return true;
	}
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
	bool explicit_specialization;
	vector<string> specialization_parameters;
	vector<TemplateParameter> specialization_parameter_details;
	vector<string> specialization_pack_names;
	vector<string> specialization_pattern;
	bool class_template;
	bool alias_template;
	bool variable_template;
	bool member_template;
	bool friend_declaration;
	bool static_member; bool deleted, immediate_return_constraint; string immediate_return_condition; bool reference_alias_cv_parameter;
	set<string> static_members;
	TemplateDefinition() : qualified_name(), name(), owner(), lexical_owner(), declaration(), parameters(), partial_specialization(false), explicit_specialization(false), specialization_parameters(), specialization_parameter_details(), specialization_pack_names(), specialization_pattern(), class_template(false), alias_template(false), variable_template(false), member_template(false), friend_declaration(false), static_member(false), deleted(false), immediate_return_constraint(false), immediate_return_condition(), reference_alias_cv_parameter(false), static_members() {}
};
// A materialized class specialization is identified by the canonical template
// entity and its ordered arguments.  Keep the entity pointer separate from
// presentation spellings so source-order checks do not reconstruct identity
// from a delimiter-packed generated-name string.
struct ClassSpecializationIdentity
{
	const TemplateDefinition* primary;
	vector<string> arguments;
	ClassSpecializationIdentity(const TemplateDefinition* primary_definition = 0,
		const vector<string>& specialization_arguments = vector<string>())
		: primary(primary_definition), arguments(specialization_arguments) {}
	bool operator<(const ClassSpecializationIdentity& other) const
	{
		if(primary != other.primary)
			return less<const TemplateDefinition*>()(primary, other.primary);
		return arguments < other.arguments;
	}
};
struct ConcreteOwnerContext
{
	string name;
	const TemplateDefinition* definition;
	vector<string> arguments;
	ConcreteOwnerContext() : name(), definition(0), arguments() {}
};
struct FunctionSignature
{
	CPPGMAstNodePtr result_specifiers;
	CPPGMAstNodePtr declarator;
	CPPGMAstNodePtr parameters; bool lvalue_argument; bool deleted; FunctionSignature() : result_specifiers(), declarator(), parameters(), lvalue_argument(false), deleted(false) {}
}; bool IsDeletedFunctionDeclaration(const CPPGMAstNodePtr& declaration);
struct ExplicitCallSelection; struct MemberCallState; struct MemberCallCandidateState; class PA18TemplateExpander
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
	map<string, TemplateDefinition> definitions_; map<const CPPGMAstNode*, TemplateDefinition> template_definitions_by_declaration_;
	map<string, vector<string> > definitions_by_name_, pending_using_declarations_, using_namespace_directives_; map<string, vector<const TemplateDefinition*> > using_declaration_targets_, using_directive_exports_;
	set<string> template_pack_names_, template_parameter_names_;
	map<string, vector<TemplateDefinition> > class_specializations_; map<string, set<string> > class_specialization_groups_by_name_;
	map<const CPPGMAstNode*, string> lexical_contexts_; set<string> lexical_namespace_paths_;
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
	map<string, string> local_class_names_; map<string, CPPGMAstNodePtr> class_declarations_; map<string, set<string> > static_members_by_class_; map<string, vector<const TemplateDefinition*> > using_member_template_targets_;
	map<string, vector<string> > constant_member_owners_;
	set<string> named_type_contexts_; map<string, string> enumerator_types_; map<string, string> variable_types_; map<string, string> variable_qualified_names_;
	map<string, vector<string> > class_paths_by_name_;
	map<string, map<string, string> > function_parameter_types_;
	map<string, PA19IntegralValue> constant_values_;
	map<string, vector<PA19IntegralValue> > constant_arrays_; map<string, size_t> constant_type_sizes_, constant_type_alignments_;
	map<string, PA19IntegralValue> active_integral_substitutions_;
	// During replay, the source class context remains dependent while its
	// materialized class is being transformed.  Keep the concrete owner in
	// typed state so an unqualified static member such as `_v` resolves to the
	// current specialization rather than a previous specialization's fallback.
	string active_instantiation_name_; bool active_static_member_;
	// Propagate the concrete enclosing specialization through nested RewriteText
	// calls without smuggling it through the ordinary identifier substitutions.
	ConcreteOwnerContext active_concrete_owner_;
	// A template parameter pack is a collection of typed substitutions.  Keep
	// the collection separate from the scalar substitution map so an expanded
	// declaration or call can consume every element without losing the first
	// one to ordinary identifier replacement.
	map<string, vector<string> > active_pack_substitutions_; map<string, vector<string> > active_pack_identifier_substitutions_;
	map<string, vector<string> > active_function_pack_substitutions_;
	struct ActivePackScope
	{
		PA18TemplateExpander* owner;
		map<string, vector<string> > saved;
		explicit ActivePackScope(PA18TemplateExpander* value)
			: owner(value), saved(value->active_pack_substitutions_) {}
		void Set(const string& name, const vector<string>& values)
		{
			if(!name.empty()) owner->active_pack_substitutions_[name] = values;
		}
		~ActivePackScope() { owner->active_pack_substitutions_ = saved; }
	};
	map<string, string> type_aliases_;
	// Alias-template specializations retain whether a type argument was a
	// reference alias.  This lets cv applied by the alias body follow the
	// language rule for top-level cv on a reference typedef.
	map<string, bool> reference_alias_specializations_;
	map<string, vector<string> > type_aliases_by_name_;
	map<string, CPPGMAstNodePtr> function_definitions_;
	map<string, FunctionSignature> function_signatures_;
	map<string, vector<string> > function_signatures_by_name_;
	map<string, vector<FunctionSignature> > function_overloads_; set<const CPPGMAstNode*> template_function_signatures_; map<string, string> specialization_bases_;
	map<string, vector<string> > specialization_arguments_;
	map<string, vector<string> > specialization_names_by_base_;
	set<ClassSpecializationIdentity> instantiated_class_specializations_;
	map<string, TemplateDefinition> explicit_function_specializations_;
	map<const CPPGMAstNode*, vector<string> > explicit_function_arguments_;
	set<string> extern_instantiation_keys_;
	map<string, CPPGMAstNodePtr> extern_instantiation_declarations_;
	map<string, set<string> > requested_nested_classes_;
	set<string> materialized_nested_classes_, materialized_member_definitions_, deferred_class_instantiations_; size_t defer_type_only_class_definitions_ = 0; size_t active_template_declaration_depth_ = 0; set<string> active_template_member_types_;
	mutable set<string> active_member_type_lookups_,
		active_function_results_,
		active_class_template_selections_, active_class_template_selection_families_,
		active_class_specialization_matches_;
	struct ActiveFunctionResultScope { PA18TemplateExpander* owner; string key; ActiveFunctionResultScope(PA18TemplateExpander* value, const string& name) : owner(value), key(name) {} ~ActiveFunctionResultScope() { owner->active_function_results_.erase(key); } };
	map<string, FunctionSignature> active_function_substitutions_;
	size_t EstimateTypeSize(string raw, const string& context) const;
	void RecordClassTypeSize(const CPPGMAstNodePtr& node, const string& context,
		const string& class_path);
	string InheritedTypeName(const string& scope, const string& name,
		set<string>* active) const;
	string QualifyTypeArgument(string spelling, const string& context,
		const string& template_owner = string(),
		bool preserve_nested_namespace = false) const; string PromotedLocalClass(const string& name, const string& context) const;
	ClassSpecializationIdentity MakeClassSpecializationIdentity(
		const TemplateDefinition& definition, const vector<string>& arguments,
		const string& context) const;
	void SetActiveConcreteOwner(const string& owner, const string& context);
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
	bool CollectImmediateReturnConstraint(const CPPGMAstNodePtr& declaration, string* condition) const; bool IsDirectCvQualifiedAliasTarget(const CPPGMAstNodePtr& declaration, const vector<TemplateParameter>& parameters) const;
	// Keep generated declaration ownership in one typed helper so forwards,
	// class shells, and materialized definitions cannot diverge.
	string GeneratedOwner(const TemplateDefinition& definition) const; string QualifyAliasTarget(const string& target, const string& alias) const; void ResolveUsingDeclarationTargets(); bool HasUsingMemberTemplate(const string& context, const string& member) const;
	bool HasReplayContext(const map<string, string>& substitutions) const
	{
		return !substitutions.empty() || !active_concrete_owner_.name.empty();
	}
	bool PreserveInlineGeneratedOrder(const vector<CPPGMAstNodePtr>& generated_classes,
		const string& owner) const;
	bool HasInlineTemplateCandidate(const vector<const TemplateDefinition*>& definitions,
		const string& context) const;
	bool IsTopLevelPackPattern(const string& value) const;
	CPPGMAstNodePtr FunctionDeclarator(const CPPGMAstNodePtr& declaration) const; CPPGMAstNodePtr FunctionParameterDefaultNode(const TemplateDefinition& definition, size_t parameter) const; bool FunctionParameterHasDefault(const TemplateDefinition& definition, size_t parameter) const; bool RestoreFunctionParameterDefaults(const TemplateDefinition& definition, TemplateDefinition* result) const;
	bool IsBuiltinArithmeticType(string raw) const; bool IsKnownTypeSpelling(string raw, const string& context) const; bool HasUnavailableGeneratedMemberType(string raw, const string& context, const map<string, string>& substitutions) const; bool GeneratedNodeHasUnavailableMemberType(const CPPGMAstNodePtr& node, const string& context, const map<string, string>& substitutions) const;
	bool HasDeferredDependentClassMember(const TemplateDefinition& definition, const string& context, const map<string, string>& substitutions) const;
	bool HasUnresolvedTemplateParameter(string raw, const string& context, const map<string, string>& substitutions) const;
	string CommonBuiltinArithmeticType(const string& left, const string& right) const;
	bool InferOperatorResult(const string& operation, const string& left, const string& right, const string& context, string* result) const;
	bool InferTemplateOperatorResult(const string& operation,
		const CPPGMAstNodePtr& left_expression, const CPPGMAstNodePtr& right_expression,
		const map<string, string>& substitutions, const string& context,
		string* result) const;
	bool InferBinaryArgument(const CPPGMAstNodePtr& expression, string* result, const map<string, string>& substitutions, const string& context) const;
	bool IsKnownMemberTemplateId(const string& raw) const;
	void CollectInheritedMemberTemplates(const string& raw_class, const string& member,
		const map<string, string>& substitutions, const string& context,
	vector<const TemplateDefinition*>* result, set<string>* active,
		map<const TemplateDefinition*, string>* concrete_owners); void CollectInheritedMemberBases(const CPPGMAstNodePtr& declaration, const string& member, const string& declaration_context, const map<string, string>& class_substitutions, vector<const TemplateDefinition*>* result, set<string>* active, map<const TemplateDefinition*, string>* concrete_owners); void AppendInheritedMemberCandidates(const string& member, const string& declaration_context, const string& base_lookup, const TemplateDefinition* base_definition, const vector<string>& base_arguments, bool base_lookup_generated, const map<string, string>& base_substitutions, vector<const TemplateDefinition*>* result, set<string>* active, map<const TemplateDefinition*, string>* concrete_owners);
	bool HasViableOrdinaryCallableMember(const CPPGMAstNodePtr& call,
		const string& object_type, const string& member_name,
		const string& context, const map<string, string>& substitutions, bool object_const = false, bool object_volatile = false, bool include_special_members = false);
	bool InstantiateMemberCall(const CPPGMAstNodePtr& call,
		const CPPGMAstNodePtr& callee, const string& original_member,
		const string& context,
		const map<string, string>& substitutions,
		bool explicit_instantiation = false, bool constructor_replay = false);
	int MemberTemplatePatternScore(const TemplateDefinition* candidate) const; void RestoreMemberTemplateDefaults(const string& member_name, const TemplateDefinition& definition, TemplateDefinition* result) const; bool ContainsSubstitutionIdentifier(const string& text, const map<string, string>& substitutions) const; bool FunctionTemplateCvPointerTie(const TemplateDefinition& lhs, const TemplateDefinition& rhs) const; bool FunctionTemplateMoreSpecialized(const TemplateDefinition& lhs, const TemplateDefinition& rhs, const string& context) const; bool PreserveFunctionLookupOrder(const vector<const TemplateDefinition*>& definitions, const string& context, const map<string, string>& substitutions) const; void SortFunctionTemplateCandidates(vector<const TemplateDefinition*>* candidates, const string& context) const; void RankFunctionTemplateCandidatesForCall(vector<const TemplateDefinition*>* candidates, const CPPGMAstNodePtr& call, const string& context, const map<string, string>& substitutions) const;
	void RankMemberCandidatesByClassExactness(vector<const TemplateDefinition*>* candidates, const CPPGMAstNodePtr& call, const map<string, string>& substitutions, const string& context); bool ValidateTemplateDefaults(const TemplateDefinition& definition, const vector<string>& arguments, const string& context, const map<string, string>& substitutions); bool TransformQualifiedMemberTemplateCall(const CPPGMAstNodePtr& input,
		const CPPGMAstNodePtr& input_callee, const string& context,
		const map<string, string>& substitutions,
		const CPPGMAstNodePtr& result);
	CPPGMAstNodePtr TransformSubscriptExpression(const CPPGMAstNodePtr& input, const string& context, const map<string, string>& substitutions);
	CPPGMAstNodePtr TransformAssignmentExpression(const CPPGMAstNodePtr& input, const string& context, const map<string, string>& substitutions);
	CPPGMAstNodePtr TransformUnaryExpression(const CPPGMAstNodePtr& input, const string& context, const map<string, string>& substitutions);
	void MaterializeInitializerConstructor(const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& result, const string& context, const map<string, string>& substitutions);
	bool MaterializeExplicitInstantiation(const CPPGMAstNodePtr& target, const string& context, bool extern_instantiation = false);
	CPPGMAstNodePtr TransformCallExpression(const CPPGMAstNodePtr& input, const string& context, const map<string, string>& substitutions); CPPGMAstNodePtr BuildExplicitDeductionInput(const CPPGMAstNodePtr& input, const string& context, const map<string, string>& substitutions); ExplicitCallSelection SelectExplicitCallDefinition(const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& input_callee, const string& context, const map<string, string>& substitutions); bool TransformExplicitFunctionCall(const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& input_callee, const string& context, const map<string, string>& substitutions, const CPPGMAstNodePtr& result); bool TransformUnqualifiedMemberTemplateCall(const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& input_callee, const string& context, const map<string, string>& substitutions, const CPPGMAstNodePtr& result); void TransformCallChildren(const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& result, const string& context, const map<string, string>& substitutions); CPPGMAstNodePtr MaterializeStaticCastCall(const CPPGMAstNodePtr& result, CPPGMAstNodePtr result_callee, const string& context, const map<string, string>& substitutions); bool MaterializeNamedCallTarget(const CPPGMAstNodePtr& result, CPPGMAstNodePtr* result_callee, const string& context, const map<string, string>& substitutions, bool* constructor_replayed); CPPGMAstNodePtr MaterializeOperatorCallTargets(const CPPGMAstNodePtr& result, const CPPGMAstNodePtr& input_callee, CPPGMAstNodePtr result_callee, const string& context, const map<string, string>& substitutions); bool MaterializeImplicitMemberCall(const CPPGMAstNodePtr& result, const CPPGMAstNodePtr& result_callee, const CPPGMAstNodePtr& input_callee, const string& context, const map<string, string>& substitutions); bool MaterializeFreeFunctionCandidates(const vector<const TemplateDefinition*>& definitions, const CPPGMAstNodePtr& result, const CPPGMAstNodePtr& result_callee, const string& callee_name, const string& qualified_callee_owner, const string& context, const map<string, string>& substitutions, const map<const TemplateDefinition*, string>& inherited_owners); void MaterializeFreeFunctionCall(const CPPGMAstNodePtr& result, const CPPGMAstNodePtr& result_callee, bool constructor_replayed, bool implicit_member_instantiated, const string& context, const map<string, string>& substitutions); void FinalizeCallResult(const CPPGMAstNodePtr& result, const CPPGMAstNodePtr& result_callee, const string& context, const map<string, string>& substitutions);
	map<string, vector<string> > BuildOwnerPackValues(const string& owner, const string& context) const; bool MaterializeFreeFunctionCandidate(const TemplateDefinition* definition, const CPPGMAstNodePtr& result, const CPPGMAstNodePtr& result_callee, const string& callee_name, const string& qualified_owner, const string& context, const map<string, string>& substitutions, const map<const TemplateDefinition*, string>& inherited_owners, const map<string, vector<string> >& owner_pack_values);
	string MaterializedFunctionResultType(const TemplateDefinition& definition, const vector<string>& inferred, const string& context, const map<string, string>& substitutions, const map<string, vector<string> >& inferred_pack_values); void ResolveSelectedFunctionArguments(const TemplateDefinition& definition, const CPPGMAstNodePtr& result, const vector<string>& inferred, const string& context, const map<string, string>& substitutions);
	void RefinePartialSpecializationCallDefinitions(vector<const TemplateDefinition*>* definitions, const string& qualified_owner, const string& callee_name, const string& context);
	bool PreserveUnresolvedExplicitTemplateCall(const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& result, const vector<string>& explicit_arguments, const string& context, const map<string, string>& explicit_substitutions, const map<string, string>& substitutions);
	void MaterializeOrdinaryCallConversions(const string& callee_name, const CPPGMAstNodePtr& result, const string& context, const map<string, string>& substitutions);
	void MaterializeOrdinaryConversion(const string& raw_parameter, const CPPGMAstNodePtr& argument, const string& context, const map<string, string>& substitutions); bool ResolveOrdinaryConversionTypes(const string& raw_parameter, const CPPGMAstNodePtr& argument, const string& context, const map<string, string>& substitutions, string* target_type, string* source_type, CPPGMAstNodePtr* source_declaration); bool ReplayOrdinaryConversion(const string& source_type, const string& target_type, const CPPGMAstNodePtr& source_declaration, const string& context, const map<string, string>& substitutions); bool TryOrdinaryConversionDefinition(const TemplateDefinition& definition, const string& source_type, const string& target_type, const string& expected_pattern, const string& context, const map<string, string>& substitutions);
	void MaterializeReturnConversions(const CPPGMAstNodePtr& function, const CPPGMAstNodePtr& result, const string& context, const string& function_context, const map<string, string>& substitutions);
	bool ValidateExplicitFunctionCandidate(const TemplateDefinition& definition, const CPPGMAstNodePtr& input, const string& context, const map<string, string>& substitutions, const vector<string>& raw_explicit_args, vector<string>* arguments); bool HasAbstractFunctionParameter(const TemplateDefinition& definition, const vector<string>& arguments, const string& context, const map<string, string>& substitutions);
	bool IsAbstractClassType(const string& raw, const string& context, set<string>* active) const; bool IsAbstractObjectSpelling(const string& raw, const string& context) const; string ResolveAlias(string spelling, const string& context) const; bool FindLogicalNamespaceAlias(const string& spelling, string* alias_key) const; bool IsArrayTypeAlias(const string& alias_name, const string& context) const; bool HasPackBeforeFixed(const TemplateDefinition& definition) const; bool ResolveGeneratedMemberAlias(const string& class_key, const string& member, const string& context, string* member_type) const; bool ResolveContextMemberAlias(const string& class_key, const string& member, const string& context, string* member_type) const;
	bool LookupVariableType(const string& name, const string& context,
		string* result) const;
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
		if(template_function_signatures_.find(declaration.get()) != template_function_signatures_.end()) return;
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
		signature.parameters = CloneNode(DescendantOfKind(declarator, "parameter-clause")); signature.deleted = IsDeletedFunctionDeclaration(declaration);
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
		root->synthetic_namespace_forward = true;
		CPPGMAstNodePtr current = root;
		for(size_t i = 1; i < parts.size(); ++i) {
			CPPGMAstNodePtr nested(new CPPGMAstNode("namespace-definition", parts[i])); nested->synthetic_namespace_forward = true;
			current->children.push_back(nested);
			current = nested;
		}
		current->children.insert(current->children.end(), forwards.begin(), forwards.end());
		return root;
	}
	void EnsureForwardClass(const string& spelling, const string& context,
		const string& owner);
	void EnsureTypeDependency(const string& spelling, const string& context,
		const string& owner);
	void EnsureDeclarationDependencies(const CPPGMAstNodePtr& node,
		const string& context, const string& owner);
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
		string declaration_spelling = name;
		if(declaration->kind == "function-definition" && declaration->children.size() > 1)
			declaration_spelling = CanonicalSpelling(FirstIdentifierLocal(
				declaration->children[1]));
		else if(declaration->kind == "simple-declaration") {
			const CPPGMAstNodePtr list = ChildOfKindLocal(declaration,
				"init-declarator-list");
			if(list && !list->children.empty() && list->children[0] &&
				!list->children[0]->children.empty())
				declaration_spelling = CanonicalSpelling(FirstIdentifierLocal(
					list->children[0]->children[0]));
		}
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
	item.static_member = declaration && !declaration->children.empty() &&
		HasDeclarationSpecifier(declaration->children[0], "static");
		item.deleted = IsDeletedFunctionDeclaration(declaration); item.immediate_return_constraint = CollectImmediateReturnConstraint(declaration, &item.immediate_return_condition); item.parameters = Parameters(node->children[0]);
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
		if(item.class_template) IndexStaticMembers(declaration, item.static_members);
			item.alias_template = declaration->kind == "alias-declaration"; item.reference_alias_cv_parameter = item.alias_template && IsDirectCvQualifiedAliasTarget(declaration, item.parameters);
		item.variable_template = declaration->kind == "simple-declaration" &&
			DescendantOfKind(declaration, "parameter-clause") == CPPGMAstNodePtr();
		item.member_template = nested_member_template ||
			(!item.class_template && class_declarations_.find(context) !=
				class_declarations_.end());
		template_definitions_by_declaration_[declaration.get()] = item;
		bool has_non_type_parameter = false; for(size_t parameter = 0; parameter < item.parameters.size(); ++parameter) if(!item.parameters[parameter].type && !item.parameters[parameter].template_template) has_non_type_parameter = true;
		if(!item.class_template && !item.alias_template && !item.variable_template && has_non_type_parameter && declaration && (declaration->kind == "function-definition" || declaration->kind == "simple-declaration" || declaration->kind == "special-member-definition" || declaration->kind == "special-member-declaration")) template_function_signatures_.insert(declaration.get());
		// `template<>` function declarations are explicit specializations, not
		// overloads of the primary template.  Keep their concrete body in typed
		// state so a later call can select it after normal template deduction.
		if(item.parameters.empty() && !item.class_template &&
			(declaration->kind == "function-definition" ||
			 declaration->kind == "simple-declaration")) {
			string specialization_base;
			vector<string> specialization_arguments;
			const size_t open = declaration_spelling.find('<');
			string argument_text;
			size_t begin = 0, close = string::npos;
			if(open != string::npos && TemplateBase(declaration_spelling, open, &begin,
					&specialization_base) && TemplateRange(declaration_spelling, open,
					&argument_text, &close)) {
				const string suffix = declaration_spelling.substr(close + 1);
				if(suffix.compare(0, 2, "::") == 0)
					specialization_base += suffix;
				specialization_arguments = SplitTemplateArguments(argument_text);
			}
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
					item.explicit_specialization = true;
					item.name = primary->name;
					item.owner = primary->owner;
					item.lexical_owner = primary->lexical_owner;
					item.qualified_name = primary->qualified_name;
					item.parameters = primary->parameters; item.declaration = CloneNode(declaration); item.deleted = IsDeletedFunctionDeclaration(item.declaration); item.immediate_return_constraint = CollectImmediateReturnConstraint(item.declaration, &item.immediate_return_condition);
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
			if(prior != definitions_.end()) item.parameters = prior->second.parameters;
			class_specializations_[item.qualified_name].push_back(item);
			class_specialization_groups_by_name_[item.name].insert(item.qualified_name);
			Collect(declaration, JoinPath(item.owner, item.name));
			return;
		}
		if(prior != definitions_.end() && !item.class_template && !prior->second.class_template) {
			ostringstream overload_key;
			overload_key << item.qualified_name << "#overload" << definitions_by_name_[item.name].size();
			definitions_[overload_key.str()] = item;
			definitions_by_name_[item.name].push_back(overload_key.str());
			IndexUsingDirectiveDefinition(definitions_[overload_key.str()]);
			Collect(declaration, item.owner);
			return;
		}
		if(prior != definitions_.end()) {
			for(size_t i = 0; i < item.parameters.size() && i < prior->second.parameters.size(); ++i)
				if(item.parameters[i].default_type.empty())
					item.parameters[i].default_type = prior->second.parameters[i].default_type;
			// A class-specifier with only its class-key is still a complete empty
			// class (`struct tuple { };`).  Only the grammar's explicit forward
			// declaration is an incomplete template declaration here; otherwise a
			// later empty definition would leave the earlier shell selected during
			// specialization replay.
			const bool prior_is_shell = prior->second.declaration &&
				prior->second.declaration->kind == "class-forward-declaration";
			const bool item_is_definition = declaration->kind == "class-specifier";
			if(prior_is_shell && item_is_definition) item.declaration = declaration;
			else if(!item_is_definition && prior->second.declaration) item.declaration = prior->second.declaration;
			prior->second.parameters = item.parameters;
			prior->second.declaration = item.declaration;
			prior->second.lexical_owner = item.lexical_owner;
			prior->second.class_template = item.class_template || prior->second.class_template;
			prior->second.alias_template = item.alias_template || prior->second.alias_template; prior->second.deleted = item.deleted; prior->second.immediate_return_constraint = item.immediate_return_constraint; prior->second.immediate_return_condition = item.immediate_return_condition; prior->second.reference_alias_cv_parameter = item.reference_alias_cv_parameter;
		} else {
			definitions_[item.qualified_name] = item;
			definitions_by_name_[item.name].push_back(item.qualified_name);
			IndexUsingDirectiveDefinition(definitions_[item.qualified_name]);
		}
		// A nested template is looked up in the concrete class scope later.  It
		// is still useful to register its lexical spelling now.
		Collect(declaration, item.class_template ? JoinPath(item.owner, name) : item.owner);
	}
	void IndexConstantMembers(const CPPGMAstNodePtr& node, const string& owner); void IndexStaticMembers(const CPPGMAstNodePtr& node, set<string>& members) const; void IndexUsingDirectiveDefinition(const TemplateDefinition& definition);
	void RememberClassPath(const string& path);
	bool HasStaticMember(const TemplateDefinition* definition, const string& owner, const string& name) const;
	void Collect(const CPPGMAstNodePtr& node, const string& context, bool type_reference = false)
	{
		if(!node) return;
		const bool elaborated_type_reference = type_reference && node->kind == "class-forward-declaration";
		if(node->kind == "using-directive") { const CPPGMAstNodePtr target = ChildOfKindLocal(node, "target"); if(target) { string target_name = CanonicalSpelling(RemoveMarker(target->value)); while(!target_name.empty() && target_name[0] == ':') target_name.erase(0, 1); if(!target_name.empty()) { vector<string>& directives = using_namespace_directives_[context]; if(find(directives.begin(), directives.end(), target_name) == directives.end()) directives.push_back(target_name); } } } if(node->kind == "using-declaration") { const CPPGMAstNodePtr target = ChildOfKindLocal(node, "target"); if(target) { const string target_name = RemoveMarker(target->value); if(!target_name.empty() && target_name.find("::") != string::npos) pending_using_declarations_[context].push_back(target_name); } }
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
		if(node->kind == "simple-declaration" && !node->children.empty())
			EnsureTypeDependency(NodeTypeSpelling(node->children[0]), context, context);
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
		if(!elaborated_type_reference && (node->kind == "class-specifier" || node->kind == "class-forward-declaration")) {
			const string class_name = LastComponent(node->value);
			next_context = JoinPath(context, class_name);
			class_declarations_[next_context] = node;
			IndexStaticMembers(node, static_members_by_class_[next_context]);
			IndexConstantMembers(node, next_context);
			if(LastComponent(context) == class_name) {
				class_declarations_[context] = node;
				IndexStaticMembers(node, static_members_by_class_[context]);
				IndexConstantMembers(node, context);
			}
			if(function_contexts_.find(context) != function_contexts_.end()) {
				const map<string, string>::const_iterator owner = function_owners_.find(context);
				const string function_owner = owner == function_owners_.end() ? PrefixComponent(context) : owner->second;
				const string promoted = JoinPath(function_owner,
					LastComponent(context) + "__" + class_name);
				local_class_names_[next_context] = promoted;
				RememberClassPath(promoted);
			} else RememberClassPath(next_context);
		}
		if(node->kind == "enum-specifier" && !node->value.empty())
			named_type_contexts_.insert(JoinPath(context, LastComponent(node->value)));
		for(size_t i = 0; i < node->children.size(); ++i) { const bool child_type_reference = node->kind == "decl-specifier-seq" && node->children[i] && node->children[i]->kind == "class-forward-declaration"; Collect(node->children[i], next_context, child_type_reference); }
		if(!elaborated_type_reference && (node->kind == "class-specifier" || node->kind == "class-forward-declaration"))
			RecordClassTypeSize(node, context, JoinPath(context, LastComponent(node->value)));
	}
	void CollectVariables(const CPPGMAstNodePtr& node, const string& context); void CountNamespaceOccurrences(const CPPGMAstNodePtr& node, const string& context);
	CPPGMAstNodePtr TransformTranslationUnit(const CPPGMAstNodePtr& input);
	bool TypeOnlyNode(const CPPGMAstNodePtr& node) const;
	void InsertGenerated(vector<CPPGMAstNodePtr>* children, const string& owner);
	void InjectGenerated(const CPPGMAstNodePtr& node, const string& context,
		const string& lexical_context); void InjectLateRootGenerated(const CPPGMAstNodePtr& node);
	bool HasExternalCompleteDependency(const CPPGMAstNodePtr& node,
		const string& owner, set<string>* dependencies) const;
	bool DeclaresSourceType(const CPPGMAstNodePtr& node, const set<string>& names) const;
	void InsertDeferredGenerated(const CPPGMAstNodePtr& node);
	bool MentionsGeneratedType(const CPPGMAstNodePtr& node, const string& type_name) const;
	bool MentionsGeneratedTypeOutsideFunctionBodies(const CPPGMAstNodePtr& node, const string& type_name) const;
	bool MentionsGeneratedClass(const CPPGMAstNodePtr& node,
		const vector<CPPGMAstNodePtr>& generated) const;
	bool MentionsGeneratedLayoutClass(const CPPGMAstNodePtr& node,
		const vector<CPPGMAstNodePtr>& generated) const;
	bool MentionsTemplateId(const CPPGMAstNodePtr& node) const;
	vector<CPPGMAstNodePtr> OrderGeneratedClasses(const vector<CPPGMAstNodePtr>& input) const;
