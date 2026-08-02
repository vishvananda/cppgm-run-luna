#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

void PA18TemplateExpander::RebindGeneratedOwnerTypes(const TemplateDefinition& definition,
	const string& concrete_owner, const CPPGMAstNodePtr& generated) const
{
		if(!concrete_owner.empty() && !definition.owner.empty()) {
			string source_owner = definition.owner;
			const size_t source_angle = source_owner.find('<');
			if(source_angle != string::npos) source_owner.erase(source_angle);
			const string collapsed_source_owner = CollapseRepeatedQualifiedPath(
				CollapseRepeatedQualifier(source_owner));
			if(source_owner != concrete_owner && !source_owner.empty()) {
				const function<void(const CPPGMAstNodePtr&)> rebind_owner_types =
					[&](const CPPGMAstNodePtr& node) {
					if(!node) return;
					if(node->kind == "decl-specifier" || node->kind == "type-name" ||
						node->kind == "type-specifier" || node->kind == "base-name") {
						const size_t marker = node->value.find(':');
						string prefix;
						if(marker != string::npos) {
							const string marker_name = node->value.substr(0, marker);
							if(marker_name == "TT_IDENTIFIER" || marker_name.compare(0, 3,
								"KW_") == 0 || marker_name.compare(0, 3, "OP_") == 0)
								prefix = node->value.substr(0, marker + 1);
						}
						string spelling = prefix.empty() ? node->value :
							node->value.substr(prefix.size());
						string owner_prefix;
						if(spelling.compare(0, source_owner.size(), source_owner) == 0 &&
							spelling.size() > source_owner.size() &&
							spelling[source_owner.size()] == ':') owner_prefix = source_owner;
						else if(!collapsed_source_owner.empty() &&
							spelling.compare(0, collapsed_source_owner.size(), collapsed_source_owner) == 0 &&
							spelling.size() > collapsed_source_owner.size() &&
							spelling[collapsed_source_owner.size()] == ':')
							owner_prefix = collapsed_source_owner;
						else {
							const string short_source_owner = LastComponent(source_owner);
							if(!short_source_owner.empty() && spelling.compare(0,
								short_source_owner.size(), short_source_owner) == 0 &&
								spelling.size() > short_source_owner.size() &&
								spelling[short_source_owner.size()] == ':')
								owner_prefix = short_source_owner;
						}
						if(!owner_prefix.empty())
							node->value = prefix + concrete_owner +
								spelling.substr(owner_prefix.size());
					}
					for(size_t child = 0; child < node->children.size(); ++child)
						rebind_owner_types(node->children[child]);
				};
				rebind_owner_types(generated);
			}
		}
}

string PA18TemplateExpander::ReuseMaterializedClassInstantiation(
	const TemplateDefinition& definition, const vector<string>& metadata_args,
	const string& key)
{
	// A primary request can arrive again after class-template selection already
	// materialized its matching partial specialization.  Both definitions share
	// the nominal generated name, but replaying the primary would transform the
	// dependent body a second time with the primary pack bindings.  Reuse the
	// complete typed entity when its source base and arguments match exactly.
	if(definition.class_template) {
		map<string, vector<string> >::const_iterator indexed =
			specialization_names_by_base_.find(LastComponent(definition.qualified_name));
		if(indexed != specialization_names_by_base_.end())
			for(size_t candidate_index = 0; candidate_index < indexed->second.size();
				++candidate_index) {
				const string& candidate = indexed->second[candidate_index];
				map<string, string>::const_iterator candidate_base =
					specialization_bases_.find(candidate);
				map<string, vector<string> >::const_iterator candidate_arguments =
					specialization_arguments_.find(candidate);
				if(candidate_base == specialization_bases_.end() ||
					candidate_arguments == specialization_arguments_.end() ||
					candidate_base->second != definition.qualified_name ||
					candidate_arguments->second.size() != metadata_args.size()) continue;
				bool same_arguments = true;
				for(size_t argument = 0; argument < metadata_args.size(); ++argument)
					if(NormalizeTypeArgument(RestoreSpecializationSpelling(
						candidate_arguments->second[argument])) !=
						NormalizeTypeArgument(RestoreSpecializationSpelling(metadata_args[argument]))) {
						same_arguments = false;
						break;
					}
				if(!same_arguments) continue;
				map<string, CPPGMAstNodePtr>::const_iterator declaration =
					class_declarations_.find(candidate);
				if(declaration == class_declarations_.end() || !declaration->second ||
					declaration->second->kind != "class-specifier" ||
					declaration->second->children.size() <= 1) continue;
				specializations_[key] = candidate;
				return candidate;
			}
	}	return string();
}

} // namespace pa18_templates_internal
