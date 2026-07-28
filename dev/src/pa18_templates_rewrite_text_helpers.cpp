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

} // namespace pa18_templates_internal
