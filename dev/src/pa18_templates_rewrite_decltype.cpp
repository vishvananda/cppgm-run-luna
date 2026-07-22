#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

string CollapseRepeatedQualifier(string raw)
{
	for(size_t position = 0; position < raw.size();) {
		if(!IsIdentifierCharacter(raw[position]) ||
			(position > 0 && IsIdentifierCharacter(raw[position - 1]))) {
			++position;
			continue;
		}
		size_t first_end = position;
		while(first_end < raw.size() && IsIdentifierCharacter(raw[first_end])) ++first_end;
		if(first_end + 2 > raw.size() || raw.compare(first_end, 2, "::") != 0) {
			position = first_end;
			continue;
		}
		const string component = raw.substr(position, first_end - position);
		const size_t second_begin = first_end + 2;
		if(raw.compare(second_begin, component.size(), component) != 0 ||
			(second_begin + component.size()) + 2 > raw.size() ||
			raw.compare(second_begin + component.size(), 2, "::") != 0) {
			position = first_end + 2;
			continue;
		}
		raw.erase(second_begin, component.size() + 2);
		position = position + component.size() + 2;
	}
	return raw;
}

size_t TopLevelScopeSeparator(const string& raw)
{
	int angle = 0;
	int parentheses = 0;
	int brackets = 0;
	size_t result = string::npos;
	for(size_t i = 0; i < raw.size(); ++i) {
		const char ch = raw[i];
		if(ch == '<' && IsTemplateAngleOpen(raw, i)) ++angle;
		else if(ch == '>' && angle > 0) --angle;
		else if(ch == '(') ++parentheses;
		else if(ch == ')' && parentheses > 0) --parentheses;
		else if(ch == '[') ++brackets;
		else if(ch == ']' && brackets > 0) --brackets;
		else if(ch == ':' && i + 1 < raw.size() && raw[i + 1] == ':' &&
			angle == 0 && parentheses == 0 && brackets == 0) {
			result = i;
			++i;
		}
	}
	return result;
}

string PA18TemplateExpander::FinishTemplateMemberType(const string& active_key,
	const map<string, vector<string> >& previous_packs, const string& value)
{
	active_pack_substitutions_ = previous_packs;
	active_template_member_types_.erase(active_key);
	string result = NormalizeTypeArgument(CollapseRepeatedQualifier(value));
	string cv_prefix;
	while(result.compare(0, 6, "const ") == 0) {
		cv_prefix += "const ";
		result = NormalizeTypeArgument(result.substr(6));
	}
	while(result.compare(0, 9, "volatile ") == 0) {
		cv_prefix += "volatile ";
		result = NormalizeTypeArgument(result.substr(9));
	}
	string suffix;
	while(!result.empty() && (result[result.size() - 1] == '&' ||
		result[result.size() - 1] == '*')) {
		suffix = result[result.size() - 1] + suffix;
		result.erase(result.size() - 1);
	}
	result = NormalizeTypeArgument(result);
	map<string, string>::const_iterator generated = specialization_bases_.find(
		LastComponent(result));
	if(result.find("::") == string::npos && generated != specialization_bases_.end()) {
		const string generated_owner = PrefixComponent(generated->second);
		if(!generated_owner.empty()) result = generated_owner + "::" + result;
	}
	return NormalizeTypeArgument(cv_prefix + result + suffix);
}

void PA18TemplateExpander::PrepareTemplateMemberSubstitutions(
	const TemplateDefinition& definition, const vector<string>& arguments,
	const string& context, map<string, string>* local)
{
	if(!local) return;
	for(size_t i = 0; i < definition.parameters.size() && i < arguments.size(); ++i)
		(*local)[definition.parameters[i].name] = arguments[i];
	// `arguments` is flattened, so a primary parameter pack cannot be
	// represented by the scalar substitution above.  Template member lookup
	// rewrites the base specialization before TransformInstantiatedNode has a
	// chance to install its pack map; install the same typed collection here so
	// `Base<Args...>::member` preserves every argument.
	size_t argument_index = 0;
	for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter) {
		const TemplateParameter& detail = definition.parameters[parameter];
		if(detail.pack) {
			size_t trailing_fixed = 0;
			for(size_t later = parameter + 1; later < definition.parameters.size(); ++later)
				if(!definition.parameters[later].pack) ++trailing_fixed;
			const size_t available = arguments.size() > argument_index ?
				arguments.size() - argument_index : 0;
			const size_t count = available > trailing_fixed ? available - trailing_fixed : 0;
			vector<string> values;
			for(size_t value = 0; value < count; ++value)
				values.push_back(arguments[argument_index + value]);
			active_pack_substitutions_[detail.name] = values;
			if(values.empty()) local->erase(detail.name);
			else (*local)[detail.name] = values[0];
			argument_index += count;
		} else {
			if(argument_index < arguments.size())
				(*local)[detail.name] = arguments[argument_index];
			++argument_index;
		}
	}
	if(definition.partial_specialization) {
		map<string, string> specialized;
		if(MatchClassSpecializationPattern(definition, arguments, &specialized, context)) {
			for(map<string, string>::const_iterator binding = specialized.begin();
				binding != specialized.end(); ++binding) (*local)[binding->first] = binding->second;
			for(size_t pack = 0; pack < definition.specialization_pack_names.size(); ++pack) {
				const string& name = definition.specialization_pack_names[pack];
				map<string, string>::const_iterator binding = specialized.find(name);
				vector<string> values;
				if(binding != specialized.end() && !binding->second.empty())
					values = SplitTemplateArguments(binding->second);
				active_pack_substitutions_[name] = values;
				if(values.empty()) local->erase(name);
				else (*local)[name] = values[0];
			}
		}
	}
	map<string, vector<string> >::const_iterator generated_names =
		specialization_names_by_base_.find(LastComponent(definition.qualified_name));
	if(generated_names == specialization_names_by_base_.end()) return;
	for(size_t generated_index = 0; generated_index < generated_names->second.size();
		++generated_index) {
		const string& generated_name = generated_names->second[generated_index];
		map<string, string>::const_iterator generated_base = specialization_bases_.find(generated_name);
		map<string, vector<string> >::const_iterator generated =
			specialization_arguments_.find(generated_name);
		if(generated_base == specialization_bases_.end() || generated == specialization_arguments_.end() ||
			generated->second.size() != arguments.size()) continue;
		bool same_arguments = true;
		for(size_t argument = 0; argument < arguments.size(); ++argument)
			if(NormalizeTypeArgument(CanonicalSpelling(generated->second[argument])) !=
				NormalizeTypeArgument(CanonicalSpelling(arguments[argument]))) {
				same_arguments = false;
				break;
			}
		if(same_arguments) {
			(*local)[definition.name] = generated_name;
			break;
		}
	}
}

string PA18TemplateExpander::RewriteTemplateMemberSpelling(
	const TemplateDefinition& definition, const vector<string>& arguments, string spelling,
	const string& context, const map<string, string>& local)
{
	map<string, string> pack_counts;
	size_t argument_index = 0;
	for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter) {
		if(!definition.parameters[parameter].pack) {
			if(argument_index < arguments.size()) ++argument_index;
			continue;
		}
		size_t trailing_fixed = 0;
		for(size_t later = parameter + 1; later < definition.parameters.size(); ++later)
			if(!definition.parameters[later].pack) ++trailing_fixed;
		const size_t available = arguments.size() > argument_index ?
			arguments.size() - argument_index : 0;
		const size_t count_value = available > trailing_fixed ? available - trailing_fixed : 0;
		ostringstream count_stream;
		count_stream << count_value;
		pack_counts[definition.parameters[parameter].name] = count_stream.str();
		argument_index += count_value;
	}
	for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter) {
		if(!definition.parameters[parameter].pack) continue;
		const string token = "sizeof...(" + definition.parameters[parameter].name + ")";
		const string count = pack_counts[definition.parameters[parameter].name];
		for(size_t position = spelling.find(token); position != string::npos;
			position = spelling.find(token, position + count.size()))
			spelling.replace(position, token.size(), count);
	}
	spelling = RewriteText(spelling, context, local, 0);
	for(size_t open = spelling.find('['); open != string::npos;) {
		const size_t close = spelling.find(']', open + 1);
		if(close == string::npos) break;
		const string bound = CanonicalSpelling(spelling.substr(open + 1, close - open - 1));
		PA19IntegralValue bound_value;
		if(!bound.empty() && EvaluateIntegralText(bound, context, local, &bound_value)) {
			const string replacement = IntegralValueSpelling(bound_value);
			spelling.replace(open + 1, close - open - 1, replacement);
			open += replacement.size() + 2;
		} else open = spelling.find('[', close + 1);
	}
	return NormalizeTypeArgument(ResolveAlias(ReplaceIdentifiers(spelling, local), context));
}

bool PA18TemplateExpander::FindDirectTemplateMemberType(
	const TemplateDefinition& definition, const vector<string>& arguments, const string& member,
	const string& context, map<string, string>* local, string* result)
{
	if(!local || !result || !definition.declaration) return false;
	bool has_direct_member = false;
	for(size_t i = 0; i < definition.declaration->children.size(); ++i) {
		const CPPGMAstNodePtr child = definition.declaration->children[i];
		if(!child) continue;
		if(child->kind == "alias-declaration" && child->value == member) {
			has_direct_member = true;
			break;
		}
		if(child->kind != "simple-declaration" || child->children.empty() ||
			SpellNode(child->children[0]).find("typedef") == string::npos) continue;
		const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
		if(!list) continue;
		for(size_t j = 0; j < list->children.size(); ++j) {
			const CPPGMAstNodePtr item = list->children[j];
			if(item && !item->children.empty() &&
				LastComponent(FirstIdentifierLocal(item->children[0])) == member) {
				has_direct_member = true;
				break;
			}
		}
		if(has_direct_member) break;
	}
	if(!has_direct_member) return false;
	for(size_t i = 0; i < definition.declaration->children.size(); ++i) {
		const CPPGMAstNodePtr child = definition.declaration->children[i];
		if(!child) continue;
		if(child->kind == "alias-declaration" && !child->children.empty()) {
			const string spelling = RewriteTemplateMemberSpelling(definition, arguments,
				TypeIdSpelling(child->children[0]), context, *local);
			(*local)[child->value] = spelling;
			if(child->value == member) {
				*result = spelling;
				return true;
			}
			continue;
		}
		if(child->kind != "simple-declaration" || child->children.empty() ||
			SpellNode(child->children[0]).find("typedef") == string::npos) continue;
		const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
		if(!list) continue;
		for(size_t j = 0; j < list->children.size(); ++j) {
			const CPPGMAstNodePtr item = list->children[j];
			if(!item || item->children.empty() ||
				LastComponent(FirstIdentifierLocal(item->children[0])).empty()) continue;
			const string name = LastComponent(FirstIdentifierLocal(item->children[0]));
			const string spelling = RewriteTemplateMemberSpelling(definition, arguments,
				NodeTypeSpelling(child->children[0]) + DeclaratorSuffix(item->children[0]) +
				DeclaratorArraySuffix(item->children[0]), context, *local);
			(*local)[name] = spelling;
			if(name == member) {
				*result = spelling;
				return true;
			}
		}
	}
	return false;
}

bool PA18TemplateExpander::FindInheritedTemplateMemberType(
	const TemplateDefinition& definition, const string& member, const string& context,
	const map<string, string>& local, string* result)
{
	if(!result || !definition.declaration) return false;
	for(size_t child_index = 0; child_index < definition.declaration->children.size(); ++child_index) {
		const CPPGMAstNodePtr clause = definition.declaration->children[child_index];
		if(!clause || clause->kind != "base-clause") continue;
		for(size_t base_index = 0; base_index < clause->children.size(); ++base_index) {
			const CPPGMAstNodePtr base_name = ChildOfKindLocal(clause->children[base_index], "base-name");
			if(!base_name) continue;
			// Keep a pack marker intact until the base argument list has been
			// split.  Replacing `B` in `Base<B...>` first turns it into
			// `Base<first...>`, losing the collection needed by inherited member
			// lookup.
			const string source_base = CanonicalSpelling(base_name->value);
			const size_t source_open = source_base.find('<');
			string base_spelling = source_open == string::npos ?
				CanonicalSpelling(ReplaceIdentifiers(source_base, local)) : source_base;
			const size_t open = base_spelling.find('<');
			if(open == string::npos) continue;
			string argument_text;
			size_t close = string::npos;
			if(!TemplateRange(base_spelling, open, &argument_text, &close)) continue;
			const string base_name_spelling = CanonicalSpelling(ReplaceIdentifiers(
				base_spelling.substr(0, open), local));
			const TemplateDefinition* base_definition = FindDefinition(base_name_spelling, definition.owner);
			if(!base_definition || !base_definition->class_template) continue;
			const vector<string> raw_base_arguments = SplitTemplateArguments(argument_text);
			vector<string> base_arguments;
			for(size_t raw_argument = 0; raw_argument < raw_base_arguments.size(); ++raw_argument) {
				const string source_argument = CanonicalSpelling(raw_base_arguments[raw_argument]);
				if(source_argument.size() > 3 &&
					source_argument.compare(source_argument.size() - 3, 3, "...") == 0) {
					const string prefix = source_argument.substr(0, source_argument.size() - 3);
					string pack_name;
					for(size_t character = 0; character < prefix.size();) {
						if(!IsIdentifierCharacter(prefix[character])) { ++character; continue; }
						const size_t begin = character;
						while(character < prefix.size() && IsIdentifierCharacter(prefix[character])) ++character;
						const string word = prefix.substr(begin, character - begin);
						if(active_pack_substitutions_.find(word) != active_pack_substitutions_.end()) {
							pack_name = word;
							break;
						}
					}
					map<string, vector<string> >::const_iterator pack =
						active_pack_substitutions_.find(pack_name);
					if(pack != active_pack_substitutions_.end()) {
						for(size_t element = 0; element < pack->second.size(); ++element) {
							map<string, string> one = local;
							one[pack_name] = pack->second[element];
							base_arguments.push_back(CollapseReferenceSpelling(
								ReplaceIdentifiers(prefix, one)));
						}
						continue;
					}
				}
				base_arguments.push_back(raw_base_arguments[raw_argument]);
			}
			for(size_t argument = 0; argument < base_arguments.size(); ++argument) {
				base_arguments[argument] = NormalizeTypeArgument(RewriteText(
					base_arguments[argument], context, local, 0, false, false));
				base_arguments[argument] = NormalizeTypeArgument(ReplaceIdentifiers(
					base_arguments[argument], local));
				base_arguments[argument] = ResolveAlias(base_arguments[argument], context);
				if(argument < base_definition->parameters.size() && base_definition->parameters[argument].type)
					base_arguments[argument] = QualifyTypeArgument(base_arguments[argument],
						context, base_definition->owner);
			}
			const TemplateDefinition* selected = SelectClassTemplateDefinition(
				base_definition, base_arguments, context);
			if(!selected) continue;
			const string generated = Instantiate(*selected, base_arguments, context);
			const string generated_path = JoinPath(selected->owner, generated);
			set<string> active;
			if(FindClassMemberType(generated_path, member, map<string, string>(), context,
				result, &active, true)) return true;
		}
	}
	return false;
}

bool PA18TemplateExpander::RewriteConcreteNestedMember(
	string* raw, size_t begin, size_t close, const string& base, const string& context,
	const map<string, string>& substitutions, bool* template_replaced, size_t* search)
{
	if(!raw || close + 2 >= raw->size() || raw->compare(close + 1, 2, "::") != 0)
		return false;
	size_t concrete_member_end = close + 3;
	while(concrete_member_end < raw->size() &&
		IsIdentifierCharacter((*raw)[concrete_member_end])) ++concrete_member_end;
	const string nested_member = raw->substr(close + 3,
		concrete_member_end - (close + 3));
	string owner_spelling = base;
	string nested_name;
	const size_t owner_separator = base.rfind("::");
	if(owner_separator != string::npos && base.find('<') == string::npos) {
		nested_name = base.substr(owner_separator + 2);
		owner_spelling = ResolveAlias(base.substr(0, owner_separator), context);
	}
	const size_t owner_open = owner_spelling.find('<');
	string owner_arguments_text;
	size_t owner_close = string::npos;
	if(owner_open == string::npos || !TemplateRange(owner_spelling, owner_open,
		&owner_arguments_text, &owner_close)) return false;
	const string owner_base = owner_spelling.substr(0, owner_open);
	if(nested_name.empty() && owner_close + 2 < owner_spelling.size() &&
		owner_spelling.compare(owner_close + 1, 2, "::") == 0)
		nested_name = owner_spelling.substr(owner_close + 3);
	const TemplateDefinition* owner_definition = FindDefinition(owner_base, context);
	if(!owner_definition || !owner_definition->class_template || nested_name.empty() ||
		nested_member.empty()) return false;
	vector<string> owner_arguments = SplitTemplateArguments(owner_arguments_text);
	for(size_t owner_argument = 0; owner_argument < owner_arguments.size(); ++owner_argument) {
		owner_arguments[owner_argument] = NormalizeTypeArgument(RewriteText(
			owner_arguments[owner_argument], context, substitutions, 0, false, false));
		owner_arguments[owner_argument] = NormalizeTypeArgument(ReplaceIdentifiers(
			owner_arguments[owner_argument], substitutions));
		owner_arguments[owner_argument] = ResolveAlias(owner_arguments[owner_argument], context);
	}
	const TemplateDefinition* selected_owner = SelectClassTemplateDefinition(
		owner_definition, owner_arguments, context);
	if(!selected_owner) return false;
	const string owner_local_name = Instantiate(*selected_owner, owner_arguments, context);
	InstantiateNestedClass(*selected_owner, owner_arguments, owner_local_name, nested_name, context);
	const string concrete_nested = JoinPath(owner_local_name, nested_name);
	map<string, string> concrete_substitutions = substitutions;
	map<string, string> owner_specialized;
	if(selected_owner->partial_specialization &&
		MatchClassSpecializationPattern(*selected_owner, owner_arguments, &owner_specialized, context))
		for(map<string, string>::const_iterator binding = owner_specialized.begin();
			binding != owner_specialized.end(); ++binding)
			concrete_substitutions[binding->first] = binding->second;
	string concrete_member;
	set<string> concrete_active;
	const bool concrete_found = FindClassMemberType(concrete_nested, nested_member,
		concrete_substitutions, context, &concrete_member, &concrete_active, true) &&
		!concrete_member.empty();
	if(!concrete_found) return false;
	concrete_member = NormalizeTypeArgument(RewriteText(concrete_member, context,
		concrete_substitutions, 0));
	raw->replace(begin, concrete_member_end - begin, concrete_member);
	if(template_replaced) *template_replaced = true;
	if(search) *search = begin + concrete_member.size();
	return true;
}

string PA18TemplateExpander::TemplateMemberType(const TemplateDefinition& definition,
	const vector<string>& arguments, const string& member, const string& context)
{
	if(!definition.declaration) return string();
	ostringstream key_stream;
	key_stream << definition.qualified_name;
	for(size_t argument = 0; argument < arguments.size(); ++argument)
		key_stream << "|" << CanonicalSpelling(arguments[argument]);
	key_stream << "|" << member;
	const string active_key = key_stream.str();
	if(!active_template_member_types_.insert(active_key).second) return string();
	const map<string, vector<string> > previous_packs = active_pack_substitutions_;
	map<string, string> local;
	PrepareTemplateMemberSubstitutions(definition, arguments, context, &local);
	string result;
	if(!FindDirectTemplateMemberType(definition, arguments, member, context, &local, &result)) {
		map<string, string>::const_iterator found = local.find(member);
		if(found != local.end()) result = found->second;
		else FindInheritedTemplateMemberType(definition, member, context, local, &result);
	}
	return FinishTemplateMemberType(active_key, previous_packs, result);
}

} // namespace pa18_templates_internal
