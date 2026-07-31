#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::RewriteMemberTemplateAliasApplication(string* raw,
	size_t begin, size_t close, const string& base, const string& context,
	const map<string, string>& substitutions, bool* template_replaced, size_t* search)
{
	if(!raw || close <= begin || LastComponent(base) != "mp_apply_q") return false;
	const size_t open = raw->find('<', begin);
	if(open == string::npos || open >= close) return false;
	const vector<string> apply_arguments = SplitTemplateArguments(raw->substr(
		open + 1, close - open - 1));
	if(apply_arguments.size() != 2) return false;
	string q = CanonicalSpelling(ReplaceIdentifiers(apply_arguments[0], substitutions));
	while(q.compare(0, 8, "typename") == 0)
		q = CanonicalSpelling(q.substr(8));
	if(q.compare(0, 9, "template ") == 0) q = CanonicalSpelling(q.substr(9));
	const size_t list_open = q.empty() ? string::npos :
		apply_arguments[1].find('<');
	if(list_open == string::npos) return false;
	string list_arguments_text;
	size_t list_close = string::npos;
	if(!TemplateRange(apply_arguments[1], list_open, &list_arguments_text,
		&list_close)) return false;
	vector<string> call_arguments = SplitTemplateArguments(list_arguments_text);
	if(call_arguments.empty()) return false;
	for(size_t argument = 0; argument < call_arguments.size(); ++argument)
		call_arguments[argument] = CanonicalSpelling(ReplaceIdentifiers(
			call_arguments[argument], substitutions));
	const string callable_source = q + "::fn";
	const string callable = NormalizeTemplateTemplateArgument(callable_source,
		context, substitutions);
	if(callable.empty()) return false;
	const size_t callable_separator = callable.rfind("::");
	if(callable_separator == string::npos) return false;
	const string callable_owner = callable.substr(0, callable_separator);
	const string callable_name = callable.substr(callable_separator + 2);
	string target = MemberAliasType(callable_owner, callable_name);
	if(target.empty()) {
		set<string> active;
		FindClassMemberType(callable_owner, callable_name, substitutions, context,
			&target, &active, true);
	}
	if(target.empty()) return false;
	map<string, string> local = substitutions;
	const string source_parent = [&]() {
		const string parent = PrefixComponent(callable_owner);
		map<string, string>::const_iterator generated = specialization_bases_.find(
			LastComponent(parent));
		return generated == specialization_bases_.end() ? string() : generated->second;
	}();
	const map<string, vector<string> >::const_iterator indexed = definitions_by_name_.find(
		callable_name);
	const TemplateDefinition* callable_definition = FindDefinition(callable_source,
		context);
	int callable_definition_score = callable_definition ? 1 : 0;
	if(indexed != definitions_by_name_.end()) for(size_t candidate = 0;
		candidate < indexed->second.size(); ++candidate) {
		map<string, TemplateDefinition>::const_iterator found = definitions_.find(
			indexed->second[candidate]);
		if(found == definitions_.end() || !found->second.alias_template) continue;
		const string owner = found->second.owner;
		if(LastComponent(owner) != LastComponent(callable_owner)) continue;
		int score = 1;
		const string candidate_parent = PrefixComponent(owner);
		if(!source_parent.empty() && LastComponent(candidate_parent) ==
			LastComponent(source_parent)) score = 3;
		if(!source_parent.empty() && candidate_parent.find(source_parent + "::") == 0)
			score = 4;
		if(score > callable_definition_score) {
			callable_definition = &found->second;
			callable_definition_score = score;
		}
	}
	if(callable_definition) for(size_t parameter = 0;
		parameter < callable_definition->parameters.size() &&
		parameter < call_arguments.size(); ++parameter)
		if(!callable_definition->parameters[parameter].name.empty())
			local[callable_definition->parameters[parameter].name] = call_arguments[parameter];
	const char* const member_names[] = {"key_type", "value_type", "reference", "next_binding"};
	for(size_t name = 0; name < sizeof(member_names) / sizeof(member_names[0]); ++name) {
		const string owner = name == 3 ? callable_owner : PrefixComponent(callable_owner);
		string member_type = MemberAliasType(owner, member_names[name]);
		if(member_type.empty()) {
			set<string> active;
			FindClassMemberType(owner, member_names[name], substitutions, context,
				&member_type, &active, true);
		}
		if(member_type.empty()) continue;
		try {
			member_type = CanonicalSpelling(RewriteText(member_type, context, local, 0));
		} catch(const PA18SubstitutionFailure&) {}
		local[member_names[name]] = member_type;
	}
	string rewritten;
	try {
		rewritten = CanonicalSpelling(RewriteText(target, context, local, 0));
	} catch(const PA18SubstitutionFailure&) {
		return false;
	}
	if(rewritten.empty() || rewritten == target) return false;
	raw->replace(begin, close - begin + 1, rewritten);
	if(template_replaced) *template_replaced = true;
	if(search) *search = begin + rewritten.size();
	return true;
}

bool PA18TemplateExpander::RewriteResolvedTemplateMember(string* raw, size_t begin,
	size_t close, const string& context, const map<string, string>& substitutions,
	const TemplateDefinition* definition, const vector<string>& args,
	bool* template_replaced, size_t* search)
{
	if(!raw || !definition || close + 2 >= raw->size() ||
		raw->compare(close + 1, 2, "::") != 0) return false;
	RecordTemplateArrayValues(*definition, args, context, substitutions,
		active_pack_substitutions_);
	size_t nested_end = close + 3;
	while(nested_end < raw->size() && IsIdentifierCharacter((*raw)[nested_end])) ++nested_end;
	const string nested = raw->substr(close + 3, nested_end - close - 3);
	if(nested.empty()) return false;
	const string template_owner = raw->substr(begin, close - begin + 1);
	const string concrete_template_owner = ReplaceIdentifiersPreservingPackSizes(
		template_owner, substitutions);
	map<string, string> inherited_owner_bindings;
	const size_t inherited_separator = TopLevelScopeSeparator(concrete_template_owner);
	if(inherited_separator != string::npos) {
		const string outer_owner = concrete_template_owner.substr(0, inherited_separator);
		const size_t outer_open = outer_owner.find('<');
		string outer_arguments_text;
		size_t outer_close = string::npos;
		if(outer_open != string::npos && TemplateRange(outer_owner, outer_open,
			&outer_arguments_text, &outer_close)) {
			const string outer_base = outer_owner.substr(0, outer_open);
			const TemplateDefinition* outer_definition = FindDefinition(outer_base, context);
			if(outer_definition && outer_definition->class_template) {
				const vector<string> outer_arguments = SplitTemplateArguments(outer_arguments_text);
				const TemplateDefinition* selected_outer = SelectClassTemplateDefinition(
					outer_definition, outer_arguments, context);
				if(selected_outer) outer_definition = selected_outer;
				for(size_t parameter = 0; parameter < outer_definition->parameters.size() &&
					parameter < outer_arguments.size(); ++parameter)
					if(!outer_definition->parameters[parameter].name.empty())
							inherited_owner_bindings[outer_definition->parameters[parameter].name] =
								outer_arguments[parameter];
				}
			}
	}
	string member_type;
	set<string> member_active;
	const TemplateDefinition* member_definition = definition;
	if(member_definition->class_template) {
		const TemplateDefinition* selected_member = SelectClassTemplateDefinition(
			member_definition, args, context);
		if(selected_member) member_definition = selected_member;
	}
	if(member_definition->declaration) {
		map<string, string> member_substitutions = substitutions;
		for(map<string, string>::const_iterator binding = inherited_owner_bindings.begin();
			binding != inherited_owner_bindings.end(); ++binding)
			member_substitutions[binding->first] = binding->second;
		for(size_t parameter = 0; parameter < member_definition->parameters.size() &&
			parameter < args.size(); ++parameter)
			if(!member_definition->parameters[parameter].name.empty())
				member_substitutions[member_definition->parameters[parameter].name] = args[parameter];
		string direct_member_type;
		bool found = false;
		try {
			found = FindDirectTemplateMemberType(*member_definition, args, nested,
				context, &member_substitutions, &direct_member_type) && !direct_member_type.empty();
			if(!found) found = FindInheritedTemplateMemberType(*member_definition, nested,
				context, member_substitutions, &direct_member_type) && !direct_member_type.empty();
		} catch(const PA18SubstitutionFailure&) {
			throw;
		}
		if(found) member_type = direct_member_type;
	}
	if(member_type.empty()) {
		bool found = FindClassMemberType(concrete_template_owner, nested, substitutions,
			context, &member_type, &member_active, true);
		if(!found || member_type.empty()) {
			found = FindClassMemberType(template_owner, nested, substitutions, context,
				&member_type, &member_active, true);
			if(!found || member_type.empty())
				member_type = TemplateMemberType(*definition, args, nested, context);
		}
	}
	const bool static_template_member = HasStaticMember(0, concrete_template_owner, nested) ||
		HasStaticMember(0, template_owner, nested);
	if(static_template_member)
		return false;
	const size_t owner_separator = TopLevelScopeSeparator(concrete_template_owner);
	if(owner_separator != string::npos) {
		const string outer_owner = concrete_template_owner.substr(0, owner_separator);
		const size_t outer_open = outer_owner.find('<');
		string outer_arguments_text;
		size_t outer_close = string::npos;
		if(outer_open != string::npos && TemplateRange(outer_owner, outer_open,
			&outer_arguments_text, &outer_close)) {
			const string outer_base = outer_owner.substr(0, outer_open);
			const TemplateDefinition* outer_definition = FindDefinition(outer_base, context);
			if(outer_definition && outer_definition->class_template) {
				const vector<string> outer_arguments = SplitTemplateArguments(outer_arguments_text);
				const TemplateDefinition* selected_outer = SelectClassTemplateDefinition(
					outer_definition, outer_arguments, context);
				if(selected_outer) outer_definition = selected_outer;
				map<string, string> outer_bindings;
				for(size_t parameter = 0; parameter < outer_definition->parameters.size() &&
					parameter < outer_arguments.size(); ++parameter)
					if(!outer_definition->parameters[parameter].name.empty())
						outer_bindings[outer_definition->parameters[parameter].name] =
							outer_arguments[parameter];
				member_type = ReplaceIdentifiersPreservingPackSizes(member_type, outer_bindings);
			}
		}
	}
	if(member_type.empty() || member_type.find('[') != string::npos) {
		requested_nested_classes_[definition->qualified_name].insert(nested);
		requested_nested_classes_[LastComponent(definition->qualified_name)].insert(nested);
		return false;
	}
	// Replace the complete dependent owner before its member type; the resolved
	// type already names the materialized owner and needs no `template` qualifier.
	size_t replacement_begin = begin;
	size_t qualifier = begin;
	while(qualifier > 0 && isspace(static_cast<unsigned char>((*raw)[qualifier - 1]))) --qualifier;
	if(qualifier >= 8 && raw->compare(qualifier - 8, 8, "template") == 0) {
		qualifier -= 8;
		while(qualifier > 0 && isspace(static_cast<unsigned char>((*raw)[qualifier - 1]))) --qualifier;
		if(qualifier >= 2 && raw->compare(qualifier - 2, 2, "::") == 0) {
			qualifier -= 2;
			while(qualifier > 0 && isspace(static_cast<unsigned char>((*raw)[qualifier - 1]))) --qualifier;
			while(qualifier > 0) {
				const size_t component_end = qualifier;
				while(qualifier > 0 && IsIdentifierCharacter((*raw)[qualifier - 1])) --qualifier;
				if(component_end == qualifier || qualifier < 2 ||
					raw->compare(qualifier - 2, 2, "::") != 0) break;
				qualifier -= 2;
				while(qualifier > 0 && isspace(static_cast<unsigned char>((*raw)[qualifier - 1]))) --qualifier;
			}
			replacement_begin = qualifier;
		}
	}
	raw->replace(replacement_begin, nested_end - replacement_begin, member_type);
	if(template_replaced) *template_replaced = true;
	if(search) *search = replacement_begin + member_type.size();
	return true;
}

} // namespace pa18_templates_internal
