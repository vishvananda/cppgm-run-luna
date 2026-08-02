#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

void PA18TemplateExpander::StripStaleGeneratedArguments(string* text) const
{
	if(!text) return;
	for(size_t at = 0; at < text->size();) {
		if(!IsIdentifierCharacter((*text)[at])) { ++at; continue; }
		const size_t begin = at;
		while(at < text->size() && IsIdentifierCharacter((*text)[at])) ++at;
		const string word = text->substr(begin, at - begin);
		if(specialization_bases_.find(word) == specialization_bases_.end() ||
			specialization_arguments_.find(word) == specialization_arguments_.end()) continue;
		size_t open = at;
		while(open < text->size() && isspace(static_cast<unsigned char>((*text)[open]))) ++open;
		if(open >= text->size() || (*text)[open] != '<') continue;
		string ignored;
		size_t close = string::npos;
		if(!TemplateRange(*text, open, &ignored, &close)) continue;
		text->erase(open, close - open + 1);
		at = begin + word.size();
	}
}

void PA18TemplateExpander::RebindGeneratedOwnerMembers(string* raw,
	const string& context, const map<string, string>& substitutions,
	bool* template_replaced)
{
	// The parser has already removed the dependent `template` keyword by the
	// time the ordinary member pass runs.  A type binding such as `Fun` can
	// therefore remain in `Fun::impl<...>` even though its concrete owner is a
	// typed template-id.  Substitute that owner before the `<...>` walker sees
	// the member template.  Direct function-type bindings contribute their
	// result class (`ListSet<int>`), not the `(left)` call-parameter suffix.
	for(map<string, string>::const_iterator current = substitutions.begin();
		current != substitutions.end(); ++current) {
		if(current->first.empty() || current->second.empty()) continue;
		const string marker = current->first + "::";
		string owner = current->second;
		string function_result;
		if(SplitDirectFunctionType(owner, &function_result, 0, 0)) owner = function_result;
		if(owner.empty() || owner.find('<') == string::npos) continue;
		for(size_t at = raw->find(marker); at != string::npos;
			at = raw->find(marker, at + owner.size())) {
			if(at > 0 && IsIdentifierCharacter((*raw)[at - 1])) continue;
			raw->replace(at, current->first.size(), owner);
			if(template_replaced) *template_replaced = true;
			at += owner.size();
		}
	}
	// A generated class can be known as a typed scope before its specialization
	// metadata is indexed.  Resolve that concrete owner before the member lookup
	// pass; otherwise `Fun::impl<...>` is examined as a source-dependent member
	// and the owner substitution is never reached.
	for(map<string, string>::const_iterator current = substitutions.begin();
		current != substitutions.end(); ++current) {
		if(current->first.empty() || current->second.empty() ||
			current->second.find('<') != string::npos ||
			current->first == current->second) continue;
		const bool generated_scope = specialization_bases_.find(
			LastComponent(current->second)) != specialization_bases_.end() &&
			specialization_arguments_.find(LastComponent(current->second)) !=
			specialization_arguments_.end();
		const bool known_scope = generated_scope ||
			class_contexts_.find(current->second) != class_contexts_.end() ||
			FindClassDeclaration(current->second, context) != CPPGMAstNodePtr();
		if(!known_scope) continue;
		const string marker = current->first + "::";
		for(size_t at = raw->find(marker); at != string::npos;
			at = raw->find(marker, at + current->second.size())) {
			if(at > 0 && IsIdentifierCharacter((*raw)[at - 1])) continue;
			if(generated_scope) {
				size_t nested_begin = at + current->first.size() + 2;
				while(nested_begin < raw->size() && isspace(
					static_cast<unsigned char>((*raw)[nested_begin]))) ++nested_begin;
				size_t nested_end = nested_begin;
				while(nested_end < raw->size() && IsIdentifierCharacter((*raw)[nested_end]))
					++nested_end;
				const string nested_name = raw->substr(nested_begin, nested_end - nested_begin);
				map<string, string>::const_iterator generated_base = specialization_bases_.find(
					LastComponent(current->second));
				map<string, vector<string> >::const_iterator generated_arguments =
					specialization_arguments_.find(LastComponent(current->second));
				if(!nested_name.empty() && generated_base != specialization_bases_.end() &&
					generated_arguments != specialization_arguments_.end()) {
					const TemplateDefinition* definition = FindDefinition(generated_base->second,
						context);
					bool nested_class = false;
					if(definition && definition->declaration) for(size_t child = 0;
						child < definition->declaration->children.size(); ++child) {
						const CPPGMAstNodePtr candidate = definition->declaration->children[child];
						if(candidate && (candidate->kind == "class-specifier" ||
							candidate->kind == "class-forward-declaration") &&
							LastComponent(candidate->value) == nested_name) {
							nested_class = true;
							break;
						}
					}
					if(nested_class) {
						requested_nested_classes_[definition->qualified_name].insert(nested_name);
						requested_nested_classes_[LastComponent(definition->qualified_name)].insert(nested_name);
						InstantiateNestedClass(*definition, generated_arguments->second,
							current->second, nested_name, context);
					}
				}
			}
			raw->replace(at, current->first.size(), current->second);
			if(template_replaced) *template_replaced = true;
			at += current->second.size();
		}
	}
}

bool PA18TemplateExpander::RewriteOwnerBoundNestedSpecialization(string* raw,
	size_t begin, size_t close, const string& base,
	const vector<string>& current_arguments, const string& context,
	const map<string, string>& substitutions, bool* template_replaced, size_t* search)
{
	// A concrete owner qualifies a nested class template, but the generated
		// local name is not globally unique: unrelated owners can both materialize
		// `impl_expr_1__int__int_`.  Consult the typed owner-bound identity before
		// the legacy short-name index so a replay can reuse the specialization that
		// belongs to this enclosing class.
		if(base.find("::") != string::npos) {
			const size_t owner_separator = base.rfind("::");
			const string qualified_owner = base.substr(0, owner_separator);
			const string member_name = base.substr(owner_separator + 2);
			map<string, vector<string> >::const_iterator owner_names =
				specialization_names_by_base_.find(member_name);
			if(owner_names != specialization_names_by_base_.end())
			for(size_t candidate_index = 0; candidate_index < owner_names->second.size();
				++candidate_index) {
				const string& candidate = owner_names->second[candidate_index];
				const string owner_key = JoinPath(qualified_owner, candidate);
				map<string, string>::const_iterator owner_base =
					specialization_bases_by_owner_.find(owner_key);
				map<string, vector<string> >::const_iterator owner_arguments =
					specialization_arguments_by_owner_.find(owner_key);
				if(owner_base == specialization_bases_by_owner_.end() ||
					owner_arguments == specialization_arguments_by_owner_.end() ||
					owner_arguments->second.size() != current_arguments.size()) continue;
				bool same_arguments = true;
				for(size_t argument = 0; argument < current_arguments.size(); ++argument) {
					const string actual = NormalizeTypeArgument(RestoreSpecializationSpelling(
						ReplaceIdentifiers(CanonicalSpelling(current_arguments[argument]),
							substitutions)));
					const string expected = NormalizeTypeArgument(RestoreSpecializationSpelling(
						CanonicalSpelling(owner_arguments->second[argument])));
					if(actual != expected) {
						same_arguments = false;
						break;
					}
				}
				if(!same_arguments) continue;
				raw->replace(begin, close - begin + 1, owner_key);
				if(template_replaced) *template_replaced = true;
				*search = begin + owner_key.size();
				return true;
			}
		}
	return false;
}

bool PA18TemplateExpander::RewriteUnqualifiedGeneratedSpecialization(string* raw,
	size_t begin, size_t close, const string& base,
	const vector<string>& current_arguments, const string& context,
	const map<string, string>& substitutions, bool* template_replaced, size_t* search)
{
	const string generated_owner = PrefixComponent(base);
	const bool materialized = !generated_owner.empty() &&
		specialization_bases_.find(LastComponent(generated_owner)) != specialization_bases_.end() &&
		specialization_arguments_.find(LastComponent(generated_owner)) != specialization_arguments_.end();
	map<string, vector<string> >::const_iterator generated_names =
		(materialized || base.find("::") != string::npos) ? specialization_names_by_base_.end() :
		specialization_names_by_base_.find(LastComponent(base));
	if(generated_names == specialization_names_by_base_.end()) return false;
	for(size_t index = 0; index < generated_names->second.size(); ++index) {
		const string& generated_name = generated_names->second[index];
		map<string, string>::const_iterator generated_base = specialization_bases_.find(generated_name);
		map<string, vector<string> >::const_iterator generated =
			specialization_arguments_.find(generated_name);
		const bool known_context = class_contexts_.find(generated_name) != class_contexts_.end() ||
			(current_arguments.empty() && class_contexts_.find(JoinPath(context, generated_name)) !=
				class_contexts_.end());
		if(generated_base == specialization_bases_.end() || !known_context ||
			generated == specialization_arguments_.end() ||
			generated->second.size() != current_arguments.size()) continue;
		bool same_arguments = true;
		for(size_t argument = 0; argument < current_arguments.size(); ++argument)
			if(NormalizeTypeArgument(ReplaceIdentifiers(CanonicalSpelling(current_arguments[argument]),
				substitutions)) != NormalizeTypeArgument(CanonicalSpelling(generated->second[argument]))) {
				same_arguments = false;
				break;
			}
		const bool qualified_member = close + 2 < raw->size() && raw->compare(close + 1, 2, "::") == 0;
		if(same_arguments && !qualified_member) {
			raw->replace(begin, close - begin + 1, generated_name);
			if(template_replaced) *template_replaced = true;
			*search = begin + raw->size();
			return true;
		}
	}
	return false;
}

bool PA18TemplateExpander::MatchQualifiedGeneratedOwner(const string& nested_owner_source,
	const vector<string>& owner_arguments, const string& selected_owner_name,
	const string& owner_primary_source, const string& context) const
{
	bool same_owner = LastComponent(nested_owner_source) == LastComponent(selected_owner_name);
	map<string, TemplateDefinition>::const_iterator found = definitions_.find(nested_owner_source);
	const TemplateDefinition* candidate = found == definitions_.end() ?
		FindDefinition(nested_owner_source, context) : &found->second;
	if(!candidate) {
		string primary_name = LastComponent(nested_owner_source);
		const size_t angle = primary_name.find('<');
		if(angle != string::npos) primary_name.erase(angle);
		const TemplateDefinition* primary = FindDefinition(primary_name, context);
		if(primary) {
			map<string, vector<TemplateDefinition> >::const_iterator partials =
				class_specializations_.find(primary->qualified_name);
			string text;
			size_t close = string::npos;
			vector<string> actual;
			const size_t owner_angle = nested_owner_source.find('<');
			if(owner_angle != string::npos && TemplateRange(nested_owner_source, owner_angle,
				&text, &close)) actual = SplitTemplateArguments(text);
			if(partials != class_specializations_.end())
				for(size_t index = 0; index < partials->second.size(); ++index) {
					const TemplateDefinition& partial = partials->second[index];
					if(partial.specialization_pattern.size() != actual.size()) continue;
					bool matches = true;
					for(size_t argument = 0; argument < actual.size(); ++argument)
						if(NormalizeTypeArgument(CanonicalSpelling(partial.specialization_pattern[argument])) !=
							NormalizeTypeArgument(CanonicalSpelling(actual[argument]))) {
							matches = false;
							break;
						}
					if(matches) {
						candidate = &partial;
						break;
					}
				}
		}
	}
	if(candidate && candidate->class_template) {
		if(candidate->partial_specialization) {
			map<string, string> bindings;
			try {
				same_owner = MatchClassSpecializationPattern(*candidate, owner_arguments,
					&bindings, context);
			} catch(const PA18SubstitutionFailure&) {
				same_owner = false;
			}
		} else {
			string text;
			size_t close = string::npos;
			vector<string> actual;
			const size_t angle = nested_owner_source.find('<');
			if(angle != string::npos && TemplateRange(nested_owner_source, angle, &text, &close))
				actual = SplitTemplateArguments(text);
			if(actual.size() != owner_arguments.size()) same_owner = false;
			else for(size_t argument = 0; argument < actual.size(); ++argument)
				if(NormalizeTypeArgument(CanonicalSpelling(actual[argument])) !=
					NormalizeTypeArgument(CanonicalSpelling(owner_arguments[argument]))) {
					same_owner = false;
					break;
				}
		}
	}
	if(!same_owner && candidate) {
		string primary = owner_primary_source;
		const size_t primary_angle = primary.find('<');
		if(primary_angle != string::npos) primary.erase(primary_angle);
		string candidate_primary = nested_owner_source;
		const size_t candidate_angle = candidate_primary.find('<');
		string text;
		size_t close = string::npos;
		vector<string> actual;
		if(candidate_angle != string::npos && TemplateRange(candidate_primary, candidate_angle,
			&text, &close)) {
			actual = SplitTemplateArguments(text);
			candidate_primary.erase(candidate_angle);
		}
		if(LastComponent(candidate_primary) == LastComponent(primary) &&
			candidate->specialization_pattern.size() == actual.size() &&
			actual.size() == owner_arguments.size()) {
			bool same_shape = true;
			for(size_t argument = 0; argument < actual.size(); ++argument) {
				const string pattern = CanonicalSpelling(
					candidate->specialization_pattern[argument]);
				const string value = CanonicalSpelling(owner_arguments[argument]);
				const size_t pattern_angle = pattern.find('<');
				const size_t value_angle = value.find('<');
				const bool same_template_shape = pattern_angle != string::npos &&
					value_angle != string::npos && pattern.substr(0, pattern_angle) ==
					value.substr(0, value_angle);
				const bool same_function_shape = pattern.find('(') != string::npos &&
					value.find('(') != string::npos;
				if(pattern != value && !same_function_shape &&
					!(same_template_shape && value.find('(') != string::npos)) {
					same_shape = false;
					break;
				}
			}
			if(same_shape) same_owner = true;
		}
	}
	return same_owner;
}

bool PA18TemplateExpander::RewriteQualifiedGeneratedNestedSpecialization(string* raw,
	size_t begin, size_t close, const string& base,
	const vector<string>& current_arguments, const string& context,
	const map<string, string>& substitutions, bool* template_replaced, size_t* search)
{
	const string generated_owner = PrefixComponent(base);
	const bool materialized_generated_owner = !generated_owner.empty() &&
		specialization_bases_.find(LastComponent(generated_owner)) != specialization_bases_.end() &&
		specialization_arguments_.find(LastComponent(generated_owner)) != specialization_arguments_.end();
	// A qualified generated owner has already supplied the enclosing class
		// arguments.  Replaying `owner::member<Args>` through ordinary lookup can
		// re-enter the same member template while its concrete specialization is
		// being formed.  Reuse the typed specialization index, but only after
		// checking both the selected enclosing class and the member arguments; the
		// short member name is shared by unrelated nested templates.
		if(materialized_generated_owner && base.find("::") != string::npos) {
			const size_t owner_separator = base.rfind("::");
			const string qualified_generated_owner = base.substr(0, owner_separator);
			map<string, vector<string> >::const_iterator owner_generated_names =
				specialization_names_by_base_.find(LastComponent(base));
			map<string, string>::const_iterator owner_base = specialization_bases_.find(
				LastComponent(qualified_generated_owner));
			map<string, vector<string> >::const_iterator owner_arguments =
				specialization_arguments_.find(LastComponent(qualified_generated_owner));
			const TemplateDefinition* owner_definition = owner_base ==
				specialization_bases_.end() ? 0 : FindDefinition(owner_base->second, context);
			if(owner_definition && owner_definition->class_template && owner_arguments !=
				specialization_arguments_.end()) {
				const TemplateDefinition* selected_owner = SelectClassTemplateDefinition(
					owner_definition, owner_arguments->second, context);
				bool qualified_replaced = false;
				if(selected_owner && owner_generated_names != specialization_names_by_base_.end())
				for(size_t generated_index = 0;
					generated_index < owner_generated_names->second.size(); ++generated_index) {
					const string& generated_name = owner_generated_names->second[generated_index];
					map<string, string>::const_iterator generated_base =
						specialization_bases_.find(generated_name);
					map<string, vector<string> >::const_iterator generated_arguments =
						specialization_arguments_.find(generated_name);
					if(generated_base == specialization_bases_.end() ||
						generated_arguments == specialization_arguments_.end() ||
						generated_arguments->second.size() != current_arguments.size()) continue;
					const size_t nested_separator = generated_base->second.rfind("::");
					if(nested_separator == string::npos) continue;
					const string nested_owner_source = generated_base->second.substr(0,
						nested_separator);
					if(!MatchQualifiedGeneratedOwner(nested_owner_source,
						owner_arguments->second, selected_owner->qualified_name,
						owner_base->second, context)) continue;
					bool same_arguments = true;
					for(size_t argument = 0; argument < current_arguments.size(); ++argument)
						if(NormalizeTypeArgument(ReplaceIdentifiers(
							CanonicalSpelling(current_arguments[argument]), substitutions)) !=
							NormalizeTypeArgument(CanonicalSpelling(
								generated_arguments->second[argument]))) {
							same_arguments = false;
							break;
						}
					if(!same_arguments) continue;
					const string concrete_member = JoinPath(qualified_generated_owner,
						generated_name);
					raw->replace(begin, close - begin + 1, concrete_member);
					if(template_replaced) *template_replaced = true;
					*search = begin + concrete_member.size();
					qualified_replaced = true;
					break;
				}
				if(qualified_replaced) return true;
			}
		}
	return false;
}

} // namespace pa18_templates_internal
