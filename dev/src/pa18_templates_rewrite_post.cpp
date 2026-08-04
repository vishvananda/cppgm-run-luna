#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

void PA18TemplateExpander::DeduceAutoInitializerType(
    const CPPGMAstNodePtr& result, const string& context,
    const map<string, string>& substitutions)
{
    if(!result || result->children.empty()) return;
    const CPPGMAstNodePtr specs = result->children[0];
    bool has_auto = false;
    for(size_t i = 0; specs && i < specs->children.size(); ++i)
        if(specs->children[i] && RemoveMarker(specs->children[i]->value) == "auto") {
            has_auto = true;
            break;
        }
    if(!has_auto) return;
    const CPPGMAstNodePtr list = ChildOfKindLocal(result, "init-declarator-list");
    if(!list || list->children.size() != 1 || !list->children[0] ||
       list->children[0]->children.size() <= 1) return;
    CPPGMAstNodePtr initializer = list->children[0]->children[1];
    if(initializer && initializer->kind == "initializer" &&
       initializer->children.size() == 1) initializer = initializer->children[0];
    string inferred;
    if(!InferArgument(initializer, &inferred, substitutions, context) || inferred.empty()) return;
    const CPPGMAstNodePtr declarator = list->children[0]->children[0];
    const bool reference_declarator = declarator &&
        DeclaratorSuffix(declarator).find('&') != string::npos;
    inferred = CanonicalSpelling(inferred);
    while(!inferred.empty() && (inferred[inferred.size() - 1] == '&' ||
        inferred[inferred.size() - 1] == '*'))
        inferred = CanonicalSpelling(inferred.substr(0, inferred.size() - 1));
    if(!reference_declarator) {
        while(inferred.compare(0, 6, "const ") == 0 ||
            inferred.compare(0, 9, "volatile ") == 0)
            inferred = CanonicalSpelling(inferred.substr(inferred.find(' ') + 1));
        while(inferred.size() > 6 && inferred.compare(inferred.size() - 6, 6, " const") == 0)
            inferred = CanonicalSpelling(inferred.substr(0, inferred.size() - 6));
        while(inferred.size() > 9 && inferred.compare(inferred.size() - 9, 9, " volatile") == 0)
            inferred = CanonicalSpelling(inferred.substr(0, inferred.size() - 9));
    }
    if(inferred.empty()) return;
	const string variable_name = list->children[0]->children.empty() ?
		string() : FirstIdentifierLocal(list->children[0]->children[0]);
	if(!variable_name.empty()) {
		// Auto deduction is a typed local-variable fact.  Keep the recovered type
		// in the same scoped table used by later member-template deduction; the
		// rewritten declaration spelling alone is not visible to those probes.
		variable_types_[variable_name] = inferred;
		function_parameter_types_[context][variable_name] = inferred;
		if(!context.empty()) variable_qualified_names_[variable_name] =
			JoinPath(context, variable_name);
	}
    for(size_t i = 0; i < specs->children.size(); ++i)
        if(specs->children[i] && RemoveMarker(specs->children[i]->value) == "auto") {
            const size_t marker = specs->children[i]->value.find(':');
            specs->children[i]->value = (marker == string::npos ? string() :
                specs->children[i]->value.substr(0, marker + 1)) + inferred;
        }
}

static string ConditionalObjectType(string raw)
{
    raw = CanonicalSpelling(raw);
    while(raw.compare(0, 6, "const ") == 0 || raw.compare(0, 9, "volatile ") == 0)
        raw = CanonicalSpelling(raw.substr(raw.find(' ') + 1));
    while(!raw.empty() && (raw[raw.size() - 1] == '&' || raw[raw.size() - 1] == '*'))
        raw = CanonicalSpelling(raw.substr(0, raw.size() - 1));
    while(raw.size() > 6 && raw.compare(raw.size() - 6, 6, " const") == 0)
        raw = CanonicalSpelling(raw.substr(0, raw.size() - 6));
    while(raw.size() > 9 && raw.compare(raw.size() - 9, 9, " volatile") == 0)
        raw = CanonicalSpelling(raw.substr(0, raw.size() - 9));
    return raw;
}

bool PA18TemplateExpander::MaterializeConditionalConversion(
    const string& actual, const string& expected, const string& context,
    const map<string, string>& substitutions)
{
    const string actual_object = ConditionalObjectType(actual);
    const string expected_object = ConditionalObjectType(expected);
    if(actual_object.empty() || expected_object.empty() || actual_object == expected_object ||
       !FindClassDeclaration(actual_object, context) ||
       !FindClassDeclaration(expected_object, context)) return false;
    string actual_base = LastComponent(actual_object);
    map<string, string>::const_iterator specialized_base = specialization_bases_.find(actual_base);
    if(specialized_base != specialization_bases_.end()) actual_base = LastComponent(specialized_base->second);
    for(map<string, TemplateDefinition>::const_iterator candidate = definitions_.begin();
        candidate != definitions_.end(); ++candidate) {
        const TemplateDefinition& definition = candidate->second;
        const string member_name = LastComponent(definition.name);
        if(!definition.member_template || definition.parameters.empty() ||
           member_name.compare(0, 8, "operator") != 0 || member_name.size() <= 8 ||
           (member_name[8] != ' ' && !IsIdentifierCharacter(member_name[8]))) continue;
        string definition_owner = definition.owner;
        const size_t angle = definition_owner.find('<');
        if(angle != string::npos) definition_owner.erase(angle);
        if(LastComponent(definition_owner) != actual_base &&
           LastComponent(definition_owner) != LastComponent(actual_object)) continue;
        string pattern = member_name.substr(8);
        while(!pattern.empty() && pattern[0] == ' ') pattern.erase(0, 1);
        set<string> parameter_names;
        for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter)
            if(!definition.parameters[parameter].name.empty())
                parameter_names.insert(definition.parameters[parameter].name);
        map<string, string> ignored;
        if(!MatchTypePattern(pattern, expected, parameter_names, &ignored, context)) continue;
        CPPGMAstNodePtr object(new CPPGMAstNode("id-expression"));
        object->inferred_type = actual;
        CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "."));
        member->children.push_back(object);
        member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", member_name)));
        CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
        call->inferred_type = expected;
        call->children.push_back(member);
        call->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("argument-list")));
        try {
            if(InstantiateMemberCall(call, member, member_name, context, substitutions)) return true;
        } catch(const PA18SubstitutionFailure&) {}
    }
    return false;
}

void PA18TemplateExpander::MaterializeConditionalConversions(
    const CPPGMAstNodePtr& result, const string& context,
    const map<string, string>& substitutions)
{
    if(!result || result->children.size() < 3) return;
    string left_type, right_type;
    if(!InferArgument(result->children[1], &left_type, substitutions, context) ||
       !InferArgument(result->children[2], &right_type, substitutions, context)) return;
    if(MaterializeConditionalConversion(left_type, right_type, context, substitutions))
        result->inferred_type = right_type;
    else if(MaterializeConditionalConversion(right_type, left_type, context, substitutions))
        result->inferred_type = left_type;
}

} // namespace pa18_templates_internal
