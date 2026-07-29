#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::HasStaticMember(const TemplateDefinition* definition,
	const string& owner, const string& name) const
{
	if(name.empty()) return false;
	if(definition && definition->static_members.find(name) !=
		definition->static_members.end()) return true;
	set<string> active;
	const function<bool(const string&)> inherited_static =
		[&](const string& raw_owner) {
			string current = CanonicalSpelling(raw_owner);
			while(current.compare(0, 6, "const ") == 0)
				current = CanonicalSpelling(current.substr(6));
			while(current.compare(0, 9, "volatile ") == 0)
				current = CanonicalSpelling(current.substr(9));
			while(!current.empty() && (current[current.size() - 1] == '*' ||
				current[current.size() - 1] == '&')) current.erase(current.size() - 1);
			current = CanonicalSpelling(current);
			// A partially replayed function-type pack can leave an ellipsis on a
			// concrete template argument (`is_same<T, U...>`).  Recover the typed
			// materialized specialization before walking its inherited bases.
			const size_t source_open = current.find('<');
			if(source_open != string::npos) {
				string source_base, source_arguments;
				size_t source_begin = 0, source_close = string::npos;
				if(TemplateBase(current, source_open, &source_begin, &source_base) &&
					TemplateRange(current, source_open, &source_arguments, &source_close)) {
					vector<string> requested = SplitTemplateArguments(source_arguments);
					for(size_t argument = 0; argument < requested.size(); ++argument) {
						requested[argument] = CanonicalSpelling(requested[argument]);
						for(size_t ellipsis = requested[argument].find("...");
							ellipsis != string::npos;
							ellipsis = requested[argument].find("...", ellipsis))
							requested[argument].erase(ellipsis, 3);
						requested[argument] = NormalizeTypeArgument(requested[argument]);
					}
					for(map<string, string>::const_iterator generated = specialization_bases_.begin();
						generated != specialization_bases_.end(); ++generated) {
						string generated_base = generated->second;
						const size_t generated_open = generated_base.find('<');
						if(generated_open != string::npos) generated_base.erase(generated_open);
						if(LastComponent(generated_base) != LastComponent(source_base)) continue;
						map<string, vector<string> >::const_iterator arguments =
							specialization_arguments_.find(generated->first);
						if(arguments == specialization_arguments_.end() ||
							arguments->second.size() != requested.size()) continue;
						bool same = true;
						for(size_t argument = 0; argument < requested.size(); ++argument) {
							string actual = CanonicalSpelling(arguments->second[argument]);
							for(size_t ellipsis = actual.find("..."); ellipsis != string::npos;
								ellipsis = actual.find("...", ellipsis)) actual.erase(ellipsis, 3);
							actual = NormalizeTypeArgument(actual);
							if(actual != requested[argument]) { same = false; break; }
						}
						if(!same) continue;
						if(class_declarations_.find(generated->first) != class_declarations_.end()) {
							current = generated->first;
							break;
						}
						for(map<string, CPPGMAstNodePtr>::const_iterator declaration =
							class_declarations_.begin(); declaration != class_declarations_.end(); ++declaration)
							if(LastComponent(declaration->first) == generated->first) {
								current = declaration->first;
								break;
							}
						break;
					}
					if(current.find("...") != string::npos && !requested.empty()) {
						const TemplateDefinition* source_definition = FindDefinition(source_base, "");
						if(source_definition && source_definition->class_template) {
							const TemplateDefinition* selected = SelectClassTemplateDefinition(
								source_definition, requested, "");
							if(selected) try {
								const string generated = const_cast<PA18TemplateExpander*>(this)->Instantiate(
									*selected, requested, "");
								const string qualified = selected->owner.empty() ? generated :
									JoinPath(selected->owner, generated);
								if(class_declarations_.find(qualified) != class_declarations_.end())
									current = qualified;
								else if(class_declarations_.find(generated) != class_declarations_.end())
									current = generated;
							} catch(const PA18SubstitutionFailure&) {
							} catch(const logic_error&) {
							}
						}
					}
				}
			}
			if(current.empty() || !active.insert(current).second) return false;
			map<string, set<string> >::const_iterator indexed =
				static_members_by_class_.find(current);
			if(indexed != static_members_by_class_.end() &&
				indexed->second.find(name) != indexed->second.end()) return true;
			const string resolved_current = CanonicalSpelling(ResolveAlias(current, ""));
			if(!resolved_current.empty() && resolved_current != current &&
				inherited_static(resolved_current)) return true;
			map<string, CPPGMAstNodePtr>::const_iterator declaration =
				class_declarations_.find(current);
			if(declaration == class_declarations_.end() || !declaration->second) return false;
			for(size_t child = 0; child < declaration->second->children.size(); ++child) {
				const CPPGMAstNodePtr clause = declaration->second->children[child];
				if(!clause || clause->kind != "base-clause") continue;
				for(size_t base = 0; base < clause->children.size(); ++base) {
					const CPPGMAstNodePtr base_name = clause->children[base] &&
						clause->children[base]->kind == "base-specifier" ?
						DescendantOfKind(clause->children[base], "base-name") :
						CPPGMAstNodePtr();
					if(!base_name || base_name->value.empty()) continue;
					if(inherited_static(RemoveMarker(base_name->value))) return true;
				}
			}
			return false;
		};
	return inherited_static(owner);
}

} // namespace pa18_templates_internal
