#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

void PA18TemplateExpander::CollectInheritedMemberBases(
    const CPPGMAstNodePtr& declaration, const string& member,
    const string& declaration_context, const map<string, string>& class_substitutions,
    vector<const TemplateDefinition*>* result, set<string>* active,
    map<const TemplateDefinition*, string>* concrete_owners)
{
	if(!declaration) return;
	// A using-declaration names the selected base member and hides same-named
	// members inherited through the other direct bases.  Keep that overload-set
	// rule in the replay lookup as well as in PA11's typed scope; otherwise a
	// left-associated operator chain can keep rediscovering the first base's
	// specialization after the concrete using target has been materialized.
	set<string> imported_owners;
	for(size_t child = 0; child < declaration->children.size(); ++child) {
		const CPPGMAstNodePtr using_declaration = declaration->children[child];
		if(!using_declaration || using_declaration->kind != "using-declaration") continue;
		const CPPGMAstNodePtr target = ChildOfKindLocal(using_declaration, "target");
		if(!target || LastComponent(target->value) != member) continue;
		string owner = PrefixComponent(target->value);
		try {
			owner = CanonicalSpelling(ResolveAlias(RewriteText(owner,
				declaration_context, class_substitutions, 0), declaration_context));
		} catch(const logic_error&) {
			owner = CanonicalSpelling(ReplaceIdentifiers(owner, class_substitutions));
		}
		if(!owner.empty()) imported_owners.insert(owner);
	}
	for(size_t child = 0; child < declaration->children.size(); ++child) {
        const CPPGMAstNodePtr clause = declaration->children[child];
        if(!clause || clause->kind != "base-clause") continue;
        for(size_t base_index = 0; base_index < clause->children.size(); ++base_index) {
            const CPPGMAstNodePtr base_name = ChildOfKindLocal(
                clause->children[base_index], "base-name");
            if(!base_name) continue;
            string base_spelling = CanonicalSpelling(RemoveMarker(RewriteText(
                base_name->value, declaration_context, class_substitutions, 0)));
            base_spelling = CanonicalSpelling(ReplaceIdentifiers(base_spelling,
                class_substitutions));
			base_spelling = CanonicalSpelling(ResolveAlias(base_spelling, declaration_context));
			string base_lookup = base_spelling;
			if(!imported_owners.empty()) {
				bool imported_base = imported_owners.find(base_spelling) != imported_owners.end();
				if(!imported_base) {
					string base_identity = base_spelling;
					const size_t base_open = base_identity.find('<');
					if(base_open != string::npos) base_identity.erase(base_open);
					for(set<string>::const_iterator imported = imported_owners.begin();
						imported != imported_owners.end() && !imported_base; ++imported) {
						string imported_identity = *imported;
						const size_t imported_open = imported_identity.find('<');
						if(imported_open != string::npos) imported_identity.erase(imported_open);
						imported_base = LastComponent(imported_identity) ==
							LastComponent(base_identity) && imported_identity == base_identity;
					}
				}
				if(!imported_base) continue;
			}
            vector<string> base_arguments;
            const TemplateDefinition* base_definition = 0;
            bool base_lookup_generated = false;
            const size_t open = base_spelling.find('<');
            if(open != string::npos) {
                string argument_text;
                size_t close = string::npos;
                if(!TemplateRange(base_spelling, open, &argument_text, &close)) continue;
                base_lookup = CanonicalSpelling(base_spelling.substr(0, open));
                base_definition = FindDefinition(base_lookup, declaration_context);
                base_arguments = SplitTemplateArguments(argument_text);
                if(base_definition) for(size_t argument = 0;
                    argument < base_arguments.size(); ++argument) {
                    base_arguments[argument] = NormalizeTypeArgument(RewriteText(
                        base_arguments[argument], declaration_context, class_substitutions, 0));
                    base_arguments[argument] = NormalizeTypeArgument(ReplaceIdentifiers(
                        base_arguments[argument], class_substitutions));
                    base_arguments[argument] = ResolveAlias(base_arguments[argument], declaration_context);
                    base_arguments[argument] = QualifyTypeArgument(base_arguments[argument],
                        declaration_context, base_definition->owner);
                }
            }
            if(!base_definition) {
                base_definition = FindDefinition(base_lookup, declaration_context);
                if(base_definition && base_definition->class_template)
                    base_lookup = base_definition->qualified_name;
            }
            if(!base_definition) {
                map<string, string>::const_iterator generated = specialization_bases_.find(
                    LastComponent(base_lookup));
                map<string, vector<string> >::const_iterator generated_args =
                    specialization_arguments_.find(LastComponent(base_lookup));
                if(generated != specialization_bases_.end() &&
                    generated_args != specialization_arguments_.end()) {
                    base_definition = FindDefinition(generated->second, declaration_context);
                    if(base_definition) {
                        base_arguments = generated_args->second;
                        base_lookup_generated = true;
                    }
                }
            }
            map<string, string> base_substitutions = class_substitutions;
            if(base_definition) for(size_t parameter = 0;
                parameter < base_definition->parameters.size() &&
                parameter < base_arguments.size(); ++parameter)
                if(!base_definition->parameters[parameter].name.empty())
                    base_substitutions[base_definition->parameters[parameter].name] =
                        base_arguments[parameter];
            AppendInheritedMemberCandidates(member, declaration_context, base_lookup,
                base_definition, base_arguments, base_lookup_generated, base_substitutions,
                result, active, concrete_owners);
        }
    }
}

void PA18TemplateExpander::AppendInheritedMemberCandidates(
    const string& member, const string& declaration_context, const string& base_lookup,
    const TemplateDefinition* base_definition, const vector<string>& base_arguments,
    bool base_lookup_generated, const map<string, string>& base_substitutions,
    vector<const TemplateDefinition*>* result, set<string>* active,
    map<const TemplateDefinition*, string>* concrete_owners)
{
    map<string, vector<string> >::const_iterator indexed_members = definitions_by_name_.find(member);
    if(indexed_members != definitions_by_name_.end()) for(size_t indexed = 0;
        indexed < indexed_members->second.size(); ++indexed) {
        map<string, TemplateDefinition>::const_iterator it = definitions_.find(
            indexed_members->second[indexed]);
        if(it == definitions_.end()) continue;
        const TemplateDefinition& candidate = it->second;
        if(candidate.class_template || candidate.alias_template || candidate.variable_template ||
            candidate.parameters.empty() || LastComponent(candidate.name) != member ||
            !candidate.declaration) continue;
        const bool declaration_kind = candidate.declaration->kind == "function-definition" ||
            candidate.declaration->kind == "simple-declaration" ||
            candidate.declaration->kind == "special-member-definition";
        if(!declaration_kind) continue;
        string owner = candidate.owner;
        const size_t owner_open = owner.find('<');
        if(owner_open != string::npos) owner.erase(owner_open);
        const bool matches = base_definition && base_definition->class_template ?
            MemberOwnerPattern(candidate, *base_definition, base_arguments, 0) :
            owner == base_lookup || LastComponent(owner) == LastComponent(base_lookup);
        if(!matches) continue;
        if(concrete_owners && base_lookup_generated &&
            concrete_owners->find(&candidate) == concrete_owners->end())
            (*concrete_owners)[&candidate] = base_lookup;
        if(find(result->begin(), result->end(), &candidate) == result->end())
            result->push_back(&candidate);
    }
    string recursive_base = base_lookup;
    if(!base_lookup_generated && !base_arguments.empty() && base_definition &&
        base_definition->class_template) {
        recursive_base += "<";
        for(size_t argument = 0; argument < base_arguments.size(); ++argument) {
            if(argument) recursive_base += ",";
            recursive_base += base_arguments[argument];
        }
        recursive_base += ">";
    }
    CollectInheritedMemberTemplates(recursive_base, member, base_substitutions,
        declaration_context, result, active, concrete_owners);
}

} // namespace pa18_templates_internal
