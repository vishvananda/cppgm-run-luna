#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

set<string> PA18TemplateExpander::RegisterGeneratedAliasOwners(
	const TemplateDefinition& definition, const CPPGMAstNodePtr& generated,
	const string& generated_owner, const string& local_name,
	const string& concrete_owner, const map<string, string>& substitutions,
	const vector<string>& args, const string& context)
{
	bool reference_argument = false;
	for(size_t argument = 0; argument < args.size() &&
		argument < definition.parameters.size(); ++argument)
		if(definition.parameters[argument].type) {
			const string resolved = ResolveAlias(args[argument], context);
			if(!resolved.empty() && resolved[resolved.size() - 1] == '&') {
				reference_argument = true;
				break;
			}
		}
	if(reference_argument && definition.reference_alias_cv_parameter) {
		reference_alias_specializations_[local_name] = true;
		reference_alias_specializations_[JoinPath(definition.owner, local_name)] = true;
		reference_alias_specializations_[JoinPath(
		definition.lexical_owner.empty() ? definition.owner : definition.lexical_owner,
		local_name)] = true;
	}
	RegisterGeneratedTypeAlias(generated, generated_owner);
	set<string> concrete_owners;
	if(!concrete_owner.empty()) concrete_owners.insert(concrete_owner);
	for(map<string, string>::const_iterator substitution = substitutions.begin();
		substitution != substitutions.end(); ++substitution) {
		const string value = CanonicalSpelling(substitution->second);
		if(specialization_bases_.find(LastComponent(value)) != specialization_bases_.end() &&
			specialization_arguments_.find(LastComponent(value)) != specialization_arguments_.end())
			concrete_owners.insert(value);
		const size_t separator = substitution->second.rfind("::");
		if(separator == string::npos) continue;
		const string owner = substitution->second.substr(0, separator);
		if(specialization_bases_.find(LastComponent(owner)) != specialization_bases_.end() &&
			specialization_arguments_.find(LastComponent(owner)) != specialization_arguments_.end())
			concrete_owners.insert(owner);
	}
	for(set<string>::const_iterator owner = concrete_owners.begin();
		owner != concrete_owners.end(); ++owner)
		RegisterGeneratedTypeAlias(generated, *owner);
	return concrete_owners;
}

string PA18TemplateExpander::ExpandGeneratedAliasSourceTarget(string source_target,
	const map<string, vector<string> >& pack_substitutions,
	const map<string, string>& substitutions) const
{
	for(map<string, vector<string> >::const_iterator pack = pack_substitutions.begin();
		pack != pack_substitutions.end(); ++pack) {
		if(pack->first.empty()) continue;
		const string token = pack->first + "...";
		string expanded;
		for(size_t element = 0; element < pack->second.size(); ++element) {
			if(!expanded.empty()) expanded += ',';
			expanded += pack->second[element];
		}
		for(size_t at = source_target.find(token); at != string::npos;) {
			source_target.replace(at, token.size(), expanded);
			if(expanded.empty()) {
				if(at < source_target.size() && source_target[at] == ',')
					source_target.erase(at, 1);
				else if(at > 0 && source_target[at - 1] == ',')
					source_target.erase(at - 1, 1), --at;
			}
			at = source_target.find(token, at + expanded.size());
		}
	}
	return CanonicalSpelling(ReplaceIdentifiersPreservingPackSizes(
		source_target, substitutions));
}

string PA18TemplateExpander::FindGeneratedAliasMemberTarget(
	const TemplateDefinition& definition, const string& source_target,
	const map<string, string>& substitutions, const string& context)
{
	string direct_target = source_target;
	while(direct_target.compare(0, 8, "typename") == 0 &&
		(direct_target.size() == 8 ||
			isspace(static_cast<unsigned char>(direct_target[8]))))
		direct_target = CanonicalSpelling(direct_target.substr(8));
	const size_t direct_separator = TopLevelScopeSeparator(direct_target);
	if(direct_separator == string::npos || definition.name != "same_actual") return string();
	const string direct_owner = direct_target.substr(0, direct_separator);
	const string direct_member = direct_target.substr(direct_separator + 2);
	bool owner_replaced = false;
	string materialized_owner = direct_owner;
	const size_t owner_open = direct_owner.find('<');
	string owner_arguments;
	size_t owner_close = string::npos;
	string owner_base;
	size_t owner_begin = 0;
	if(owner_open != string::npos && TemplateBase(direct_owner, owner_open,
		&owner_begin, &owner_base) && TemplateRange(direct_owner, owner_open,
		&owner_arguments, &owner_close)) {
		const vector<string> raw_owner_arguments = SplitTemplateArguments(owner_arguments);
		map<string, vector<string> >::const_iterator names =
			specialization_names_by_base_.find(LastComponent(owner_base));
		if(names != specialization_names_by_base_.end())
			for(size_t name = 0; name < names->second.size() && !owner_replaced; ++name) {
				const string& candidate = names->second[name];
				map<string, string>::const_iterator candidate_base =
					specialization_bases_.find(candidate);
				map<string, vector<string> >::const_iterator candidate_arguments =
					specialization_arguments_.find(candidate);
				if(candidate_base == specialization_bases_.end() ||
					candidate_arguments == specialization_arguments_.end() ||
					LastComponent(candidate_base->second) != LastComponent(owner_base) ||
					candidate_arguments->second.size() != raw_owner_arguments.size()) continue;
				bool same_owner = true;
				for(size_t argument = 0; argument < raw_owner_arguments.size(); ++argument) {
					const string actual = NormalizeTypeArgument(ResolveAlias(
						ReplaceIdentifiersPreservingPackSizes(raw_owner_arguments[argument],
							substitutions), context));
					const string expected = NormalizeTypeArgument(candidate_arguments->second[argument]);
					if(actual != expected && NormalizeTypeArgument(RestoreSpecializationSpelling(actual)) !=
						NormalizeTypeArgument(RestoreSpecializationSpelling(expected))) {
						same_owner = false;
						break;
					}
				}
				if(!same_owner) continue;
				const string qualified_candidate = PrefixComponent(direct_owner).empty() ?
					candidate : JoinPath(PrefixComponent(direct_owner), candidate);
				if(class_declarations_.find(qualified_candidate) == class_declarations_.end() &&
					class_declarations_.find(candidate) == class_declarations_.end()) continue;
				materialized_owner = class_declarations_.find(qualified_candidate) !=
					class_declarations_.end() ? qualified_candidate : candidate;
				owner_replaced = true;
			}
	}
	if(!owner_replaced)
		materialized_owner = RewriteText(direct_owner, context, substitutions,
			&owner_replaced, false, false);
	string member_target = MemberAliasType(materialized_owner, direct_member);
	set<string> member_active;
	bool direct_found = !member_target.empty();
	if(!direct_found && (owner_replaced || materialized_owner != direct_owner))
		direct_found = FindClassMemberType(materialized_owner, direct_member,
			substitutions, context, &member_target, &member_active, true);
	return direct_found ? NormalizeTypeArgument(member_target) : string();
}

string PA18TemplateExpander::ComputeGeneratedAliasTarget(
	const TemplateDefinition& definition, const map<string, string>& substitutions,
	const map<string, vector<string> >& pack_substitutions, const string& context)
{
	if(!definition.declaration || definition.declaration->children.empty()) return string();
	const map<string, vector<string> > previous_packs = active_pack_substitutions_;
	for(map<string, vector<string> >::const_iterator pack = pack_substitutions.begin();
		pack != pack_substitutions.end(); ++pack)
		if(!pack->first.empty()) active_pack_substitutions_[pack->first] = pack->second;
	string concrete_target;
	try {
		const string source_target = ExpandGeneratedAliasSourceTarget(
			TypeIdSpelling(definition.declaration->children[0]), pack_substitutions,
			substitutions);
		concrete_target = FindGeneratedAliasMemberTarget(definition, source_target,
			substitutions, context);
		if(concrete_target.empty()) concrete_target = NormalizeTypeArgument(
			RewriteText(source_target, context, substitutions, 0, true, true));
		concrete_target = NormalizeTypeArgument(ResolveAlias(concrete_target,
			context, substitutions));
	} catch(const PA18SubstitutionFailure&) {
		concrete_target.clear();
	}
	active_pack_substitutions_ = previous_packs;
	return concrete_target;
}

void PA18TemplateExpander::RegisterGeneratedAliasEntity(
	const TemplateDefinition& definition, const CPPGMAstNodePtr& generated,
	const string& generated_owner, const string& local_name,
	const string& concrete_owner, const map<string, string>& substitutions,
	const vector<string>& args,
	const map<string, vector<string> >& pack_substitutions, const string& context)
{
	if(!definition.alias_template) return;
	const set<string> concrete_owners = RegisterGeneratedAliasOwners(definition,
		generated, generated_owner, local_name, concrete_owner, substitutions, args,
		context);
	const string concrete_target = ComputeGeneratedAliasTarget(definition,
		substitutions, pack_substitutions, context);
	if(concrete_target.empty()) return;
	const auto update_alias = [this, &local_name, &concrete_target](const string& owner) {
		const string alias = JoinPath(owner, local_name);
		if(!alias.empty()) type_aliases_[alias] = concrete_target;
	};
	update_alias(generated_owner);
	for(set<string>::const_iterator owner = concrete_owners.begin();
		owner != concrete_owners.end(); ++owner) update_alias(*owner);
}

void PA18TemplateExpander::InstallOuterOwnerSubstitutions(
	const map<string, string>* outer_substitutions, const string& context,
	map<string, string>* substitutions, bool bind_enclosing_owner)
{
	if(outer_substitutions) for(map<string, string>::const_iterator outer =
		outer_substitutions->begin(); outer != outer_substitutions->end(); ++outer) {
		const size_t separator = outer->second.rfind("::");
		if(separator == string::npos) continue;
		AddConcreteOwnerSubstitutions(outer->second.substr(0, separator), context,
			substitutions, bind_enclosing_owner);
	}
}

void PA18TemplateExpander::ApplyForwardingPackHints(
	const map<string, vector<string> >* hints,
	map<string, vector<string> >* pack_substitutions)
{
	if(!hints || !pack_substitutions) return;
	for(map<string, vector<string> >::const_iterator hint = hints->begin();
		hint != hints->end(); ++hint) {
		if(hint->first.empty()) continue;
		map<string, vector<string> >::iterator current =
			pack_substitutions->find(hint->first);
		if(current != pack_substitutions->end() &&
			current->second.size() == hint->second.size()) current->second = hint->second;
	}
}

} // namespace pa18_templates_internal
