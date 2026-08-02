#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;
namespace pa18_templates_internal {

string PA18TemplateExpander::RewriteTextMemberSuffix(
	string raw, const string& source_spelling, const string& context,
	const map<string, string>& substitutions, bool* template_replaced,
	bool materialized_member_type, bool preserved_static_member,
	bool resolve_alias, bool resolve_member)
{
	map<string, string> final_substitutions = substitutions;
	ProtectMaterializedSubstitutions(source_spelling, raw, context, substitutions,
		materialized_member_type, &final_substitutions);
	raw = ReplaceIdentifiersPreservingPackSizes(raw, final_substitutions);
	// A concrete generated owner can appear without its source template-id after
	// an earlier member substitution (`list2<...>::child0` -> `expr_X::member`).
	// Resolve those typed member aliases in a second pass so a chain such as
	// `Args::child0::proto_grammar` is reduced from the inside out.
	const auto next_scope_separator = [this](const string& text, size_t start) {
		int angle = 0;
		for(size_t position = 0; position + 1 < text.size(); ++position) {
			if(text[position] == '<' && IsTemplateAngleOpen(text, position)) ++angle;
			else if(text[position] == '>' && angle > 0 && IsTemplateAngleClose(text, position)) --angle;
			if(position >= start && angle == 0 && text.compare(position, 2, "::") == 0)
				return position;
		}
		return string::npos;
	};
	if(resolve_member) for(size_t separator = next_scope_separator(raw, 0);
		separator != string::npos; ) {
		size_t member_begin = separator + 2;
		while(member_begin < raw.size() && isspace(static_cast<unsigned char>(raw[member_begin])))
			++member_begin;
		if(member_begin >= raw.size() || !IsIdentifierCharacter(raw[member_begin])) {
			separator = next_scope_separator(raw, separator + 2);
			continue;
		}
		size_t member_end = member_begin + 1;
		while(member_end < raw.size() && IsIdentifierCharacter(raw[member_end])) ++member_end;
		size_t owner_end = separator;
		while(owner_end > 0 && isspace(static_cast<unsigned char>(raw[owner_end - 1]))) --owner_end;
		while(owner_end > 0 && (raw[owner_end - 1] == '&' || raw[owner_end - 1] == '*')) --owner_end;
		while(owner_end > 0 && isspace(static_cast<unsigned char>(raw[owner_end - 1]))) --owner_end;
		size_t owner_begin = owner_end;
		if(owner_begin > 0 && raw[owner_begin - 1] == '>') {
			int nested_angle = 0;
			while(owner_begin > 0) {
				const char ch = raw[owner_begin - 1];
				if(ch == '>') ++nested_angle;
				else if(ch == '<' && nested_angle > 0) {
					--nested_angle;
					if(nested_angle == 0) {
						--owner_begin;
						break;
					}
				}
				--owner_begin;
			}
			while(owner_begin > 0 && IsIdentifierCharacter(raw[owner_begin - 1])) --owner_begin;
		} else while(owner_begin > 0 && IsIdentifierCharacter(raw[owner_begin - 1])) --owner_begin;
		if(owner_begin == owner_end) {
			separator = next_scope_separator(raw, member_end);
			continue;
		}
		while(owner_begin >= 2 && raw.compare(owner_begin - 2, 2, "::") == 0) {
			size_t component_begin = owner_begin - 2;
			while(component_begin > 0 &&
				IsIdentifierCharacter(raw[component_begin - 1])) --component_begin;
			owner_begin = component_begin;
		}
		const string owner = raw.substr(owner_begin, separator - owner_begin);
		string owner_key = CanonicalSpelling(owner);
		while(!owner_key.empty() && (owner_key[owner_key.size() - 1] == '&' ||
			owner_key[owner_key.size() - 1] == '*')) owner_key.erase(owner_key.size() - 1);
		while(owner_key.compare(0, 6, "const ") == 0)
			owner_key = CanonicalSpelling(owner_key.substr(6));
		while(owner_key.compare(0, 9, "volatile ") == 0)
			owner_key = CanonicalSpelling(owner_key.substr(9));
		const bool known_owner = specialization_bases_.find(LastComponent(owner_key)) !=
			specialization_bases_.end() || owner.find('<') != string::npos;
		if(!known_owner) {
			separator = next_scope_separator(raw, member_end);
			continue;
		}
		string member_type;
		set<string> member_active;
		const string member_name = raw.substr(member_begin, member_end - member_begin);
		string lookup_owner = owner;
		// The generated nested class records its concrete typedef under the full
		// materialized owner (`tuples::detail::drop_front_3_::apply_...`).  The
		// rewritten source can legitimately use the namespace-relative spelling
		// (`drop_front_3_::apply_...`), so recover that exact typed alias by suffix
		// before falling back to the source family, whose target still contains the
		// local dependent name `next`.
		bool indexed_owner_alias = false;
		const string indexed_alias_key = owner + "::" + member_name;
		const string indexed_alias_suffix = "::" + indexed_alias_key;
		for(map<string, string>::const_iterator indexed = type_aliases_.begin();
			indexed != type_aliases_.end(); ++indexed) {
			const bool exact = indexed->first == indexed_alias_key;
			const bool suffix = indexed->first.size() > indexed_alias_suffix.size() &&
				indexed->first.compare(indexed->first.size() - indexed_alias_suffix.size(),
					indexed_alias_suffix.size(), indexed_alias_suffix) == 0;
			if(!exact && !suffix) continue;
			member_type = CanonicalSpelling(ReplaceIdentifiers(indexed->second, substitutions));
			indexed_owner_alias = !member_type.empty();
			if(indexed_owner_alias) break;
		}
		// The generated owner can have been formed by a prior scalar pass over a
		// nested dependent type (`vector<Property>` becoming `vector<unsigned
		// long>`).  Keep the concrete lookup first: it carries the already
		// materialized pack spelling.  Recover the source owner only when that
		// lookup cannot resolve the member, so a source `Args...` does not get
		// reintroduced after the generated owner has been typed.
		string source_lookup_owner;
		const size_t source_separator = next_scope_separator(source_spelling, 0);
		if(source_separator != string::npos) {
			size_t source_member_begin = source_separator + 2;
			while(source_member_begin < source_spelling.size() &&
				isspace(static_cast<unsigned char>(source_spelling[source_member_begin]))) ++source_member_begin;
			size_t source_member_end = source_member_begin;
			while(source_member_end < source_spelling.size() &&
				IsIdentifierCharacter(source_spelling[source_member_end])) ++source_member_end;
			if(source_spelling.substr(source_member_begin, source_member_end - source_member_begin) ==
				member_name) {
				const string source_owner = source_spelling.substr(0, source_separator);
				if(source_owner.find('<') != string::npos) source_lookup_owner = source_owner;
			}
		}
		bool found_member = indexed_owner_alias || FindClassMemberType(lookup_owner, member_name,
			substitutions, context, &member_type, &member_active, true);
		if((!found_member || member_type.empty()) && !source_lookup_owner.empty() &&
			source_lookup_owner != lookup_owner) {
			member_type.clear();
			member_active.clear();
				lookup_owner = source_lookup_owner;
				found_member = FindClassMemberType(lookup_owner, member_name,
					substitutions, context, &member_type, &member_active, true);
		}
		if(!found_member || member_type.empty()) {
			separator = next_scope_separator(raw, member_end);
			continue;
		}
		map<string, string>::const_iterator generated_owner_base = specialization_bases_.find(
			LastComponent(owner_key));
		if(generated_owner_base != specialization_bases_.end()) {
			string source_owner = generated_owner_base->second;
			const size_t source_angle = source_owner.find('<');
			if(source_angle != string::npos) source_owner.erase(source_angle);
			if(member_type.compare(0, source_owner.size(), source_owner) == 0 &&
				member_type.size() > source_owner.size() &&
				member_type[source_owner.size()] == ':')
				member_type = owner_key + member_type.substr(source_owner.size());
		}
		size_t replacement_member_end = member_end;
		if(member_end < raw.size() && raw[member_end] == '<') {
			string trailing_arguments;
			size_t trailing_close = string::npos;
			const size_t member_open = member_type.find('<');
			string existing_arguments;
			size_t existing_close = string::npos;
			if(member_open != string::npos &&
				TemplateRange(member_type, member_open, &existing_arguments, &existing_close) &&
				existing_close + 1 == member_type.size() &&
				TemplateRange(raw, member_end, &trailing_arguments, &trailing_close)) {
				member_type = member_type.substr(0, existing_close) +
					(existing_arguments.empty() ? string() : string(",")) +
					trailing_arguments + ">";
				replacement_member_end = trailing_close + 1;
			}
		}
		// FindClassMemberType may already return the complete dependent alias
		// chain for the direct member (`apply<Tuple>::type::tail_type`).  If the
		// source spelling also carries that same suffix, replacing only the first
		// component would duplicate it on every replay pass and grow an invalid
		// `::tail_type::tail_type...` chain.  Consume the suffix that is already
		// present in the typed member result.
		if(replacement_member_end == member_end && member_end < raw.size()) {
			const string trailing = raw.substr(member_end);
			if(trailing.size() < member_type.size() &&
				member_type.compare(member_type.size() - trailing.size(),
					trailing.size(), trailing) == 0)
				replacement_member_end = raw.size();
		}
		const bool static_member_expression = HasStaticMember(0, owner_key, member_name) ||
			HasStaticMember(0, owner, member_name) ||
			HasStaticMember(0, LastComponent(owner_key), member_name);
		if(static_member_expression) {
			preserved_static_member = true;
			separator = next_scope_separator(raw, member_end);
			continue;
		}
		materialized_member_type = true;
		if(member_type.find("::") == string::npos) {
			const string resolved_member_type = ResolveAlias(member_type, context);
			if(!resolved_member_type.empty()) member_type = resolved_member_type;
		}
		size_t replacement_begin = owner_begin;
		while(replacement_begin > 0 && isspace(static_cast<unsigned char>(raw[replacement_begin - 1])))
			--replacement_begin;
		const size_t prefix_end = replacement_begin;
		size_t prefix_begin = prefix_end;
		while(prefix_begin > 0 && IsIdentifierCharacter(raw[prefix_begin - 1])) --prefix_begin;
		if(prefix_end > prefix_begin &&
			(raw.substr(prefix_begin, prefix_end - prefix_begin) == "const" ||
				raw.substr(prefix_begin, prefix_end - prefix_begin) == "volatile"))
			replacement_begin = owner_begin;
		if(replacement_begin >= 8 && raw.compare(replacement_begin - 8, 8, "typename") == 0 &&
			(replacement_begin == 8 || !IsIdentifierCharacter(raw[replacement_begin - 9])))
			replacement_begin -= 8;
		raw.replace(replacement_begin, replacement_member_end - replacement_begin,
			NormalizeTypeArgument(member_type));
		if(template_replaced) *template_replaced = true;
		separator = next_scope_separator(raw, replacement_begin + member_type.size());
	}
	raw = CollapseReferenceSpelling(raw);
	if(preserved_static_member) return raw;
	if(!resolve_alias || raw.find("::") == string::npos) return raw;
	if(constant_values_.find(raw) != constant_values_.end()) return raw;
	const string resolved = ResolveAlias(raw, context);
	const string raw_owner = PrefixComponent(raw);
	map<string, string>::const_iterator generated_base = specialization_bases_.find(
		LastComponent(raw_owner));
	if(generated_base != specialization_bases_.end() && !raw_owner.empty()) {
		string source_owner = generated_base->second;
		const size_t source_angle = source_owner.find('<');
		if(source_angle != string::npos) source_owner.erase(source_angle);
		if(resolved.compare(0, source_owner.size(), source_owner) == 0 &&
			resolved.size() > source_owner.size() &&
			resolved[source_owner.size()] == ':')
			return raw_owner + resolved.substr(source_owner.size());
	}
	return resolved;
}

} // namespace pa18_templates_internal
