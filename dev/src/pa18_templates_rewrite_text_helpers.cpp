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
			else if(ch == '<' && angles > 0 && parentheses == 0 &&
				brackets == 0 && braces == 0) --angles;
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
				if(left && right) {
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
			if(packs.find(pack_names[pack])->second.size() != first_pack.size())
				throw PA18SubstitutionFailure("pack expansion length mismatch");
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
            specialization_bases_.end()) continue;
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
		if(specialization_bases_.find(generated) == specialization_bases_.end()) continue;
		size_t after = at;
		while(after < raw.size() && isspace(static_cast<unsigned char>(raw[after]))) ++after;
		if(after < raw.size() && raw[after] == '<') erase_keys.push_back(name);
	}
	for(size_t key = 0; key < erase_keys.size(); ++key)
		protected_substitutions->erase(erase_keys[key]);
}

} // namespace pa18_templates_internal
