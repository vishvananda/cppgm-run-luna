#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

namespace {

bool ContainsIdentifierTokenLocal(const string& text, const string& identifier)
{
    if(identifier.empty()) return false;
    for(size_t at = 0; at < text.size();) {
        if(!IsIdentifierCharacter(text[at])) { ++at; continue; }
        const size_t begin = at;
        while(at < text.size() && IsIdentifierCharacter(text[at])) ++at;
        if(at - begin == identifier.size() && text.compare(begin, identifier.size(), identifier) == 0)
            return true;
    }
    return false;
}

}

bool PA18TemplateExpander::IsElaboratedTypeArgumentSpelling(const string& spelling) const
{
	const string normalized = CanonicalSpelling(spelling);
	const char* const keys[] = {"struct", "class", "union"};
	for(size_t key = 0; key < sizeof(keys) / sizeof(keys[0]); ++key) {
		const string keyword = keys[key];
		if(normalized.compare(0, keyword.size(), keyword) != 0) continue;
		return normalized.size() > keyword.size() &&
			(isspace(static_cast<unsigned char>(normalized[keyword.size()])) ||
				IsIdentifierCharacter(normalized[keyword.size()]));
	}
	return false;
}

string PA18TemplateExpander::ExpandPackCallText(string raw,
	const map<string, vector<string> >& packs) const
{
	// Expand dependent template calls such as `declval<Args>()...` before the
	// normal template-id pass.  Function-call operands use the same rule for
	// `static_cast<Args&&>(args)...`.
	for(size_t search = 0; search < raw.size(); ++search) {
		if(raw[search] != '<') continue;
		size_t begin = 0, close = string::npos;
		string base, arguments;
		if(!TemplateBase(raw, search, &begin, &base) ||
			!TemplateRange(raw, search, &arguments, &close)) continue;
		const vector<string> values = SplitTemplateArguments(arguments);
		if(values.size() != 1) continue;
		const string pack_name = CanonicalSpelling(values[0]);
		map<string, vector<string> >::const_iterator pack = packs.find(pack_name);
		if(pack == packs.end() || close + 5 >= raw.size() ||
			raw[close + 1] != '(' || raw[close + 2] != ')' ||
			raw.compare(close + 3, 3, "...") != 0) continue;
		string expansion;
		for(size_t value = 0; value < pack->second.size(); ++value) {
			if(!expansion.empty()) expansion += ',';
			expansion += base + "<" + pack->second[value] + ">()";
		}
		raw.replace(begin, close + 6 - begin, expansion);
		search = begin + expansion.size();
	}
	for(size_t search = 0; search + 2 < raw.size();) {
		const size_t ellipsis = raw.find("...", search);
		if(ellipsis == string::npos) break;
		if(ellipsis + 3 < raw.size() && raw[ellipsis + 3] == '(') {
			search = ellipsis + 3;
			continue;
		}
		int parentheses = 0, brackets = 0, braces = 0, angles = 0;
		size_t begin = 0;
		for(size_t position = ellipsis; position > 0; --position) {
			const char ch = raw[position - 1];
			if(ch == ')') ++parentheses;
			else if(ch == '(') {
				if(parentheses > 0) --parentheses;
				else { begin = position; break; }
			} else if(ch == ']') ++brackets;
			else if(ch == '[' && brackets > 0) --brackets;
			else if(ch == '}') ++braces;
			else if(ch == '{' && braces > 0) --braces;
			else if(ch == '>' && parentheses == 0 && brackets == 0 && braces == 0)
				++angles;
			else if(ch == '<' && parentheses == 0 && brackets == 0 && braces == 0) {
				if(angles > 0) --angles;
				else if(IsTemplateAngleOpen(raw, position - 1)) {
					begin = position;
					break;
				}
			}
			else if(ch == ',' && parentheses == 0 && brackets == 0 &&
				braces == 0 && angles == 0) {
				begin = position;
				break;
			}
		}
		while(begin < ellipsis && isspace(static_cast<unsigned char>(raw[begin]))) ++begin;
		if(begin == ellipsis) {
			search = ellipsis + 3;
			continue;
		}
		const string source = raw.substr(begin, ellipsis - begin);
		vector<string> pack_names;
		for(map<string, vector<string> >::const_iterator pack = packs.begin();
			pack != packs.end(); ++pack) {
			if(pack->first.empty()) continue;
			for(size_t at = source.find(pack->first); at != string::npos;
				at = source.find(pack->first, at + pack->first.size())) {
				const bool left = at == 0 || !IsIdentifierCharacter(source[at - 1]);
				const size_t end = at + pack->first.size();
				const bool right = end == source.size() || !IsIdentifierCharacter(source[end]);
				const bool sizeof_operand = at >= 10 &&
					source.compare(at - 10, 10, "sizeof...(") == 0;
				if(left && right && !sizeof_operand) {
					pack_names.push_back(pack->first);
					break;
				}
			}
		}
		if(pack_names.empty()) {
			search = ellipsis + 3;
			continue;
		}
		const vector<string>& first_pack = packs.find(pack_names[0])->second;
		for(size_t pack = 1; pack < pack_names.size(); ++pack)
			if(packs.find(pack_names[pack])->second.size() != first_pack.size()) {
				throw PA18SubstitutionFailure("pack expansion length mismatch");
			}
		string expansion;
		for(size_t element = 0; element < first_pack.size(); ++element) {
			map<string, string> one;
			for(size_t pack = 0; pack < pack_names.size(); ++pack)
				one[pack_names[pack]] = packs.find(pack_names[pack])->second[element];
			if(!expansion.empty()) expansion += ',';
			expansion += CollapseReferenceSpelling(
				ReplaceIdentifiersPreservingPackSizes(source, one));
		}
		raw.replace(begin, ellipsis + 3 - begin, expansion);
		search = begin + expansion.size();
	}
	return raw;
}

string PA18TemplateExpander::RewriteTextPackSpelling(string raw,
	const map<string, string>& substitutions)
{
	for(map<string, vector<string> >::const_iterator active_pack =
		active_pack_substitutions_.begin(); active_pack != active_pack_substitutions_.end();
		++active_pack) {
		if(active_pack->first.empty()) continue;
		const string token = active_pack->first + "...";
		if(raw == token) continue;
		string expanded;
		for(size_t element = 0; element < active_pack->second.size(); ++element) {
			if(!expanded.empty()) expanded += ',';
			expanded += active_pack->second[element];
		}
		for(size_t at = raw.find(token); at != string::npos;) {
			raw.replace(at, token.size(), expanded);
			if(expanded.empty()) {
				if(at < raw.size() && raw[at] == ',') raw.erase(at, 1);
				else if(at > 0 && raw[at - 1] == ',') raw.erase(--at, 1);
			}
			at = raw.find(token, at + expanded.size());
		}
	}
	// A pack can expand an arbitrary expression rather than a complete
	// template-id, for example `is_same<T, V>::value...` inside a constexpr
	// helper call.  Use the expression-aware rewriter before the structural
	// template-id pass so empty packs become an empty argument list as well.
	raw = ExpandPackCallText(raw, active_pack_substitutions_);
	for(;;) {
		size_t selected_open = string::npos;
		size_t selected_close = string::npos;
		vector<string> pack_names;
		for(size_t open = raw.find('<'); open != string::npos;
			open = raw.find('<', open + 1)) {
			string arguments;
			size_t close = string::npos;
			if(!TemplateRange(raw, open, &arguments, &close) ||
				close + 3 >= raw.size() || raw.compare(close + 1, 3, "...") != 0)
				continue;
			vector<string> found;
			for(map<string, vector<string> >::const_iterator pack =
				active_pack_substitutions_.begin();
				pack != active_pack_substitutions_.end(); ++pack)
				if(ContainsIdentifierTokenLocal(arguments, pack->first)) found.push_back(pack->first);
			if(found.empty()) continue;
			selected_open = open;
			selected_close = close;
			pack_names = found;
			break;
		}
		if(selected_open == string::npos) break;
		size_t base_begin = 0;
		string base;
		if(!TemplateBase(raw, selected_open, &base_begin, &base)) break;
		const vector<string>& first_pack = active_pack_substitutions_.find(
			pack_names[0])->second;
		for(size_t pack = 1; pack < pack_names.size(); ++pack)
			if(active_pack_substitutions_.find(pack_names[pack])->second.size() !=
				first_pack.size())
				throw PA18SubstitutionFailure("pack expansion length mismatch");
		const string template_id = raw.substr(base_begin,
			selected_close - base_begin + 1);
		string expanded;
		for(size_t element = 0; element < first_pack.size(); ++element) {
			map<string, string> one = substitutions;
			for(size_t pack = 0; pack < pack_names.size(); ++pack)
				one[pack_names[pack]] = active_pack_substitutions_.find(
					pack_names[pack])->second[element];
			if(!expanded.empty()) expanded += ',';
			expanded += ReplaceIdentifiersPreservingPackSizes(template_id, one);
		}
		const size_t replacement_size = selected_close + 4 - base_begin;
		raw.replace(base_begin, replacement_size, expanded);
		if(expanded.empty()) {
			if(base_begin < raw.size() && raw[base_begin] == ',') raw.erase(base_begin, 1);
			else if(base_begin > 0 && raw[base_begin - 1] == ',') raw.erase(base_begin - 1, 1);
		}
	}
	// A compact replay can preserve the expanded pack elements inside a
	// sizeof-pack operand even after the typed pack scope has ended, for example
	// `sizeof...(list<>,list<int>,list<double>)`.  Recover that arity before the
	// expression is carried into a materialized type alias.
	for(size_t search = raw.find("sizeof..."); search != string::npos; ) {
		const size_t open = search + 9;
		if(open >= raw.size() || raw[open] != '(') {
			search = raw.find("sizeof...", search + 9);
			continue;
		}
		int depth = 0;
		size_t close = string::npos;
		for(size_t position = open; position < raw.size(); ++position) {
			if(raw[position] == '(') ++depth;
			else if(raw[position] == ')' && --depth == 0) {
				close = position;
				break;
			}
		}
		if(close == string::npos) break;
		const string expression = raw.substr(search, close - search + 1);
		PA19IntegralValue count;
		if(EvaluateActivePackSize(expression, &count)) {
			const string replacement = IntegralValueSpelling(count);
			raw.replace(search, close - search + 1, replacement);
			search += replacement.size();
			continue;
		}
		const string operand = CanonicalSpelling(raw.substr(open + 1,
			close - open - 1));
		const vector<string> flattened = SplitTemplateArguments(operand);
		if(flattened.size() > 1) {
			const string replacement = IntegralValueSpelling(PA19IntegralValue::Unsigned(
				static_cast<unsigned long long>(flattened.size()), "unsigned long", 64));
			raw.replace(search, close - search + 1, replacement);
			search += replacement.size();
		} else search = raw.find("sizeof...", close + 1);
	}
	return CanonicalSpelling(RewriteActivePackSizes(raw));
}

string PA18TemplateExpander::RewriteNestedMemberSubstitutions(string raw,
	const string& context, const map<string, string>& substitutions,
	bool* template_replaced)
{
	for(map<string, string>::const_iterator current = substitutions.begin();
		current != substitutions.end(); ++current) {
		if(current->first.empty() || current->second.find('<') == string::npos) continue;
		const string marker = current->first + "::template";
		for(size_t at = raw.find(marker); at != string::npos;
			at = raw.find(marker, at + current->second.size())) {
			if(at > 0 && IsIdentifierCharacter(raw[at - 1])) continue;
			raw.replace(at, current->first.size(), current->second);
			const size_t owner_open = raw.find('<', at);
			if(owner_open == string::npos) break;
			string owner_base;
			size_t owner_begin = 0, owner_close = string::npos;
			string owner_arguments;
			if(!TemplateBase(raw, owner_open, &owner_begin, &owner_base) ||
				!TemplateRange(raw, owner_open, &owner_arguments, &owner_close)) break;
			if(RewriteConcreteNestedMember(&raw, owner_begin, owner_close, owner_base,
				context, substitutions, template_replaced, 0)) break;
		}
	}
	return raw;
}

string PA18TemplateExpander::RewriteConcreteMemberSubstitutions(string raw,
	const string& context, const map<string, string>& substitutions,
	bool* materialized_member_type)
{
	for(map<string, string>::const_iterator current = substitutions.begin();
		current != substitutions.end(); ++current) {
		if(current->first.empty() || current->second.empty() ||
			current->second.find('<') != string::npos) continue;
		map<string, string>::const_iterator base = specialization_bases_.find(
			LastComponent(current->second));
		map<string, vector<string> >::const_iterator arguments =
			specialization_arguments_.find(LastComponent(current->second));
		if(base == specialization_bases_.end() || arguments == specialization_arguments_.end()) continue;
		const TemplateDefinition* definition = FindDefinition(base->second, context);
		if(!definition || !definition->class_template) continue;
		const string token = current->first + "::";
		for(size_t at = raw.find(token); at != string::npos; at = raw.find(token, at)) {
			size_t end = at + token.size();
			while(end < raw.size() && IsIdentifierCharacter(raw[end])) ++end;
			if(end == at + token.size()) break;
			const string member = raw.substr(at + token.size(), end - at - token.size());
			string member_type;
			set<string> member_active;
			FindClassMemberType(current->second, member, substitutions, context,
				&member_type, &member_active, true);
			if(member_type.empty()) member_type = TemplateMemberType(*definition,
				arguments->second, member, context);
			if(member_type.empty()) {
				bool nested_class = false;
				if(definition->declaration) for(size_t child = 0;
					child < definition->declaration->children.size(); ++child) {
					const CPPGMAstNodePtr candidate = definition->declaration->children[child];
					if(!candidate || (candidate->kind != "class-specifier" &&
						candidate->kind != "class-forward-declaration") ||
						LastComponent(candidate->value) != member) continue;
					nested_class = true;
					break;
				}
				if(nested_class) {
					requested_nested_classes_[definition->qualified_name].insert(member);
					requested_nested_classes_[LastComponent(definition->qualified_name)].insert(member);
					InstantiateNestedClass(*definition, arguments->second,
						current->second, member, context);
				}
				break;
			}
			if(!member_type.empty()) {
				string member_context = context;
				map<string, string>::const_iterator owner = specialization_bases_.find(
					LastComponent(current->second));
				if(owner != specialization_bases_.end() &&
					!PrefixComponent(owner->second).empty())
					member_context = PrefixComponent(owner->second);
				member_type = QualifyTypeArgument(member_type, member_context,
					member_context, true);
			}
			size_t replacement_begin = at;
			size_t word_begin = at;
			while(word_begin > 0 && isspace(static_cast<unsigned char>(raw[word_begin - 1]))) --word_begin;
			if(word_begin >= 8 && raw.compare(word_begin - 8, 8, "typename") == 0 &&
				(word_begin == 8 || !IsIdentifierCharacter(raw[word_begin - 9])))
				replacement_begin = word_begin - 8;
			if(at >= 2 && raw.compare(at - 2, 2, "::") == 0) {
				size_t owner_begin = at - 2;
				int owner_angle = 0;
				while(owner_begin > 0) {
					const char ch = raw[owner_begin - 1];
					if(ch == '>') ++owner_angle;
					else if(ch == '<' && owner_angle > 0) --owner_angle;
					else if(owner_angle == 0 && (isspace(static_cast<unsigned char>(ch)) ||
						ch == ',' || ch == '(')) break;
					--owner_begin;
				}
				const string owner_spelling = raw.substr(owner_begin, at - 2 - owner_begin);
				bool materialized_owner = specialization_bases_.find(
					LastComponent(owner_spelling)) != specialization_bases_.end();
				if(!materialized_owner && owner_spelling.find('<') != string::npos) {
					string owner_base;
					size_t owner_begin_marker = 0;
					const size_t owner_open = owner_spelling.find('<');
					if(TemplateBase(owner_spelling, owner_open, &owner_begin_marker, &owner_base)) {
						const TemplateDefinition* owner_definition = FindDefinition(owner_base, context);
						materialized_owner = owner_definition && owner_definition->class_template;
					}
				}
				if(materialized_owner) {
					replacement_begin = owner_begin;
					while(replacement_begin > 0 &&
						isspace(static_cast<unsigned char>(raw[replacement_begin - 1]))) --replacement_begin;
					const size_t prefix_end = replacement_begin;
					size_t prefix_begin = prefix_end;
					while(prefix_begin > 0 && IsIdentifierCharacter(raw[prefix_begin - 1])) --prefix_begin;
					if(prefix_end > prefix_begin &&
						(raw.substr(prefix_begin, prefix_end - prefix_begin) == "const" ||
						 raw.substr(prefix_begin, prefix_end - prefix_begin) == "volatile"))
						replacement_begin = owner_begin;
					if(replacement_begin >= 8 && raw.compare(replacement_begin - 8, 8,
						"typename") == 0 && (replacement_begin == 8 ||
						!IsIdentifierCharacter(raw[replacement_begin - 9])))
						replacement_begin -= 8;
				}
			}
			raw.replace(replacement_begin, end - replacement_begin, member_type);
			if(materialized_member_type) *materialized_member_type = true;
			at = replacement_begin + member_type.size();
		}
	}
	return raw;
}

const TemplateDefinition* PA18TemplateExpander::SelectFunctionTemplateOverload(
    const string& raw, const string& lookup_base,
    const vector<string>& explicit_arguments, const string& context,
    const map<string, string>& substitutions,
    const vector<const TemplateDefinition*>& overloads)
{
    string call_callee, call_arguments;
    vector<string> actual_types;
    if(!SplitTextCall(raw, &call_callee, &call_arguments)) return 0;
    const vector<string> actual_expressions = SplitCallArguments(call_arguments);
    for(size_t actual = 0; actual < actual_expressions.size(); ++actual) {
        string type = CanonicalSpelling(actual_expressions[actual]);
        if(type.size() >= 2 && type.compare(type.size() - 2, 2, "{}") == 0)
            type = CanonicalSpelling(type.substr(0, type.size() - 2));
        type = NormalizeTypeArgument(ResolveAlias(
            CanonicalSpelling(ReplaceIdentifiers(type, substitutions)), context));
        if(type.compare(0, 8, "typename") == 0)
            type = CanonicalSpelling(type.substr(8));
        if(type.empty() || !IsKnownTypeSpelling(type, context)) return 0;
        actual_types.push_back(type);
    }
    if(actual_types.empty()) return 0;
    const TemplateDefinition* viable = 0;
    for(size_t overload = 0; overload < overloads.size(); ++overload) {
        const TemplateDefinition* candidate = overloads[overload];
        if(!candidate || candidate->class_template || candidate->alias_template ||
            candidate->variable_template) continue;
        bool candidate_has_pack = false;
        for(size_t parameter = 0; parameter < candidate->parameters.size(); ++parameter)
            if(candidate->parameters[parameter].pack) candidate_has_pack = true;
        if(explicit_arguments.size() > candidate->parameters.size() && !candidate_has_pack)
            continue;
        try {
            if(!FunctionArgumentsViable(*candidate, explicit_arguments, actual_types, context))
                continue;
        } catch(const PA18SubstitutionFailure&) { continue; }
        if(!viable || FunctionTemplateMoreSpecialized(*candidate, *viable, context))
            viable = candidate;
    }
    (void)lookup_base;
    return viable;
}

void PA18TemplateExpander::ProtectMaterializedSubstitutions(
    const string& source_spelling, const string& raw, const string& context,
    const map<string, string>& substitutions, bool materialized_member_type,
    map<string, string>* final_substitutions) const
{
    if(!final_substitutions) return;
    for(map<string, string>::const_iterator substitution = substitutions.begin();
        substitution != substitutions.end(); ++substitution) {
		if(specialization_bases_.find(LastComponent(substitution->second)) ==
			specialization_bases_.end() && substitution->second.find('<') == string::npos)
			continue;
        for(size_t at = raw.find(substitution->first); at != string::npos;
            at = raw.find(substitution->first, at + substitution->first.size())) {
            if(at > 0 && IsIdentifierCharacter(raw[at - 1])) continue;
            size_t after = at + substitution->first.size();
            while(after < raw.size() && isspace(static_cast<unsigned char>(raw[after]))) ++after;
            if(after < raw.size() && raw[after] == '<') {
                final_substitutions->erase(substitution->first);
                break;
            }
        }
    }
    for(map<string, string>::const_iterator substitution = substitutions.begin();
        substitution != substitutions.end(); ++substitution) {
        const string& source_name = substitution->first;
        const string& materialized_name = substitution->second;
        if(source_name.empty() || materialized_name.empty() || source_name == materialized_name ||
            !ContainsIdentifierTokenLocal(source_spelling, source_name) ||
            ContainsIdentifierTokenLocal(source_spelling, materialized_name) ||
            materialized_name.find_first_not_of(
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != string::npos)
            continue;
        if((class_contexts_.find(materialized_name) != class_contexts_.end() ||
            FindClassDeclaration(materialized_name, context)) &&
            ContainsIdentifierTokenLocal(raw, materialized_name))
            final_substitutions->erase(materialized_name);
    }
    if(materialized_member_type) for(map<string, string>::const_iterator substitution =
        substitutions.begin(); substitution != substitutions.end(); ++substitution) {
        const string& materialized_name = substitution->second;
        if(materialized_name.empty() || materialized_name.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != string::npos ||
            (class_contexts_.find(materialized_name) == class_contexts_.end() &&
                !FindClassDeclaration(materialized_name, context)) ||
            !ContainsIdentifierTokenLocal(raw, materialized_name)) continue;
        for(map<string, string>::const_iterator introduced = substitutions.begin();
            introduced != substitutions.end(); ++introduced)
            if(introduced->first != materialized_name && introduced->second == materialized_name) {
                final_substitutions->erase(materialized_name);
                break;
            }
    }
    if(!materialized_member_type && !context.empty() && raw.find('<') == string::npos &&
        raw.find("::") == string::npos &&
        (raw.find('*') != string::npos || raw.find('&') != string::npos))
        for(map<string, string>::const_iterator substitution = substitutions.begin();
            substitution != substitutions.end(); ++substitution) {
            const string& materialized_name = substitution->second;
            if(materialized_name.empty() || materialized_name.find_first_not_of(
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != string::npos ||
                !ContainsIdentifierTokenLocal(raw, materialized_name)) continue;
            for(map<string, string>::const_iterator introduced = substitutions.begin();
                introduced != substitutions.end(); ++introduced)
                if(introduced->first != materialized_name && introduced->second == materialized_name) {
                    final_substitutions->erase(materialized_name);
                    break;
                }
        }
}

void PA18TemplateExpander::ProtectMaterializedTemplateBases(
	const string& raw, const string& context,
	const map<string, string>& substitutions,
	map<string, string>* protected_substitutions) const
{
	(void)context;
	if(!protected_substitutions) return;
	vector<string> erase_keys;
	// Scan the source spelling once and consult the typed substitution index by
	// identifier.  The previous per-binding search repeated the whole spelling
	// for every bound name on a hot rewrite path.
	for(size_t at = 0; at < raw.size();) {
		if(!IsIdentifierCharacter(raw[at])) {
			++at;
			continue;
		}
		const size_t begin = at;
		while(at < raw.size() && IsIdentifierCharacter(raw[at])) ++at;
		const string name = raw.substr(begin, at - begin);
		map<string, string>::const_iterator substitution = substitutions.find(name);
		if(substitution == substitutions.end()) continue;
		const string generated = LastComponent(substitution->second);
		// The specialization registry is the typed ownership fact for a
		// materialized template-id.  Do not fall back to FindClassDeclaration
		// here: lookup itself replays partial specializations and can re-enter
		// the member path that is trying to protect its source spelling.
		if(specialization_bases_.find(generated) == specialization_bases_.end() &&
			substitution->second.find('<') == string::npos) continue;
		size_t after = at;
		while(after < raw.size() && isspace(static_cast<unsigned char>(raw[after]))) ++after;
		if(after < raw.size() && raw[after] == '<') erase_keys.push_back(name);
	}
	for(size_t key = 0; key < erase_keys.size(); ++key)
		protected_substitutions->erase(erase_keys[key]);
}

} // namespace pa18_templates_internal
