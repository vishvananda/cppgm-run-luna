#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
using namespace std;
namespace pa18_templates_internal {

string PA18TemplateExpander::NormalizeTemplateTemplateArgument(
	string raw, const string& context,
	const map<string, string>& substitutions) const
{
	raw = CanonicalSpelling(ReplaceIdentifiers(raw, substitutions));
	if(raw.compare(0, 9, "template ") == 0) raw = CanonicalSpelling(raw.substr(9));
	for(size_t position = raw.find("::template "); position != string::npos;
		position = raw.find("::template ", position + 2)) raw.erase(position + 2, 9);
	const TemplateDefinition* definition = FindDefinition(raw, context);
	string concrete_member_argument;
	const size_t member_separator = raw.rfind("::");
	if(member_separator != string::npos) {
		const string owner = raw.substr(0, member_separator);
		const size_t owner_open = owner.find('<');
		string owner_arguments_text;
		size_t owner_close = string::npos;
		if(owner_open != string::npos && TemplateRange(owner, owner_open,
			&owner_arguments_text, &owner_close)) {
			const string owner_base = owner.substr(0, owner_open);
			const TemplateDefinition* owner_definition = FindDefinition(owner_base, context);
			if(owner_definition && owner_definition->class_template) {
				const vector<string> owner_arguments = SplitTemplateArguments(owner_arguments_text);
				string concrete_owner;
				ResolveMaterializedClassOwner(owner_base, owner_arguments, context,
					&concrete_owner);
				if(concrete_owner.empty()) try {
					const string local_owner = const_cast<PA18TemplateExpander*>(this)->Instantiate(
						*owner_definition, owner_arguments, context, false);
					concrete_owner = JoinPath(owner_definition->owner, local_owner);
				} catch(const PA18SubstitutionFailure&) {}
				if(!concrete_owner.empty() && FindClassDeclaration(concrete_owner, context))
					concrete_member_argument = concrete_owner + "::" + raw.substr(member_separator + 2);
			}
		}
	}
	if(!definition) {
		const size_t nested_separator = raw.rfind("::");
		if(nested_separator != string::npos) {
			const string member = raw.substr(nested_separator + 2);
			const string nested_owner = raw.substr(0, nested_separator);
			const size_t owner_separator = nested_owner.rfind("::");
			if(owner_separator != string::npos) {
				const string parent = nested_owner.substr(0, owner_separator);
				const string nested = nested_owner.substr(owner_separator + 2);
				string resolved_nested;
				set<string> active_nested;
				const bool materialized_nested_owner = class_declarations_.find(
					nested_owner) != class_declarations_.end();
				if(materialized_nested_owner || FindClassMemberType(parent, nested,
					map<string, string>(), context, &resolved_nested, &active_nested,
					false)) {
					if(materialized_nested_owner) resolved_nested = nested_owner;
					resolved_nested = CanonicalSpelling(resolved_nested);
					const string resolved_prefix = PrefixComponent(resolved_nested);
					const string repeated_owner = resolved_prefix.empty() ? resolved_nested :
						JoinPath(resolved_prefix, JoinPath(LastComponent(resolved_prefix),
						LastComponent(resolved_nested)));
					map<string, vector<string> >::const_iterator indexed =
						definitions_by_name_.find(LastComponent(member));
					if(indexed != definitions_by_name_.end()) for(size_t candidate_index = 0;
						candidate_index < indexed->second.size(); ++candidate_index) {
						map<string, TemplateDefinition>::const_iterator candidate = definitions_.find(
							indexed->second[candidate_index]);
						if(candidate == definitions_.end() ||
							(!candidate->second.alias_template && !candidate->second.class_template &&
								!candidate->second.variable_template)) continue;
						string candidate_owner = candidate->second.owner;
						const size_t candidate_open = candidate_owner.find('<');
						if(candidate_open != string::npos) candidate_owner.erase(candidate_open);
						bool generated_owner_match = false;
						const string generated_parent = PrefixComponent(resolved_nested);
						map<string, string>::const_iterator generated_source =
							specialization_bases_.find(LastComponent(generated_parent));
						if(generated_source != specialization_bases_.end()) {
							const string source_parent = generated_source->second;
							generated_owner_match = candidate_owner == JoinPath(source_parent,
								LastComponent(resolved_nested)) || candidate_owner ==
								JoinPath(source_parent, JoinPath(LastComponent(source_parent),
									LastComponent(resolved_nested)));
						}
						if(candidate_owner == resolved_nested || candidate_owner == repeated_owner ||
							generated_owner_match) {
							definition = &candidate->second;
							if(specialization_bases_.find(LastComponent(
								PrefixComponent(resolved_nested))) != specialization_bases_.end())
								concrete_member_argument = resolved_nested + "::" + member;
							break;
						}
					}
				}
			}
		}
	}
	if(!definition) return string();
	if(!concrete_member_argument.empty()) return concrete_member_argument;
	const size_t owner_separator = raw.rfind("::");
	if(owner_separator != string::npos) {
		const string owner = raw.substr(0, owner_separator);
		if(specialization_bases_.find(LastComponent(owner)) != specialization_bases_.end() &&
			specialization_arguments_.find(LastComponent(owner)) != specialization_arguments_.end())
			return raw;
	}
	return definition->qualified_name;
}

bool PA18TemplateExpander::SplitDirectFunctionType(const string& raw,
	string* result, vector<string>* parameters, string* qualifiers) const
{
	const string spelling = NormalizeTypeArgument(raw);
	int angle_depth = 0;
	int bracket_depth = 0;
	for(size_t open = 0; open < spelling.size(); ++open) {
		const char ch = spelling[open];
		if(ch == '<' && IsTemplateAngleOpen(spelling, open)) {
			++angle_depth;
			continue;
		}
		if(ch == '>' && angle_depth > 0 && IsTemplateAngleClose(spelling, open)) {
			--angle_depth;
			continue;
		}
		if(ch == '[') {
			++bracket_depth;
			continue;
		}
		if(ch == ']' && bracket_depth > 0) {
			--bracket_depth;
			continue;
		}
		if(ch != '(' || angle_depth != 0 || bracket_depth != 0) continue;
		const string prefix = CanonicalSpelling(spelling.substr(0, open));
		if(prefix.empty()) return false;
	if(prefix == "decltype" || prefix == "sizeof" || prefix == "alignof" ||
		prefix == "new") return false;
		int parentheses = 1;
		size_t close = string::npos;
		for(size_t position = open + 1; position < spelling.size(); ++position) {
			if(spelling[position] == '(') ++parentheses;
			else if(spelling[position] == ')' && --parentheses == 0) {
				close = position;
				break;
			}
		}
		if(close == string::npos) return false;
	const string suffix = CanonicalSpelling(spelling.substr(close + 1));
		bool valid_suffix = true;
		for(size_t position = 0; position < suffix.size();) {
			if(suffix[position] == '&') {
				++position;
				if(position < suffix.size() && suffix[position] == '&') ++position;
				continue;
			}
			if(!IsIdentifierCharacter(suffix[position])) {
				valid_suffix = false;
				break;
			}
			size_t end = position + 1;
			while(end < suffix.size() && IsIdentifierCharacter(suffix[end])) ++end;
			const string word = suffix.substr(position, end - position);
			if(word != "const" && word != "volatile" && word != "noexcept") {
				valid_suffix = false;
				break;
			}
			position = end;
		}
		if(!valid_suffix || suffix.find('(') != string::npos ||
			prefix.find('(') != string::npos || prefix.find(')') != string::npos) return false;
		if(result) *result = prefix;
		if(parameters) *parameters = SplitTemplateArguments(spelling.substr(open + 1,
			close - open - 1));
		if(qualifiers) *qualifiers = suffix;
		return true;
	}
	return false;
}
	bool PA18TemplateExpander::ClassPartialMoreSpecialized(const TemplateDefinition& lhs,
		const TemplateDefinition& rhs, const string& context) const
	{
		(void)context;
		if(!lhs.partial_specialization || !rhs.partial_specialization) return false;
		const auto repeated_pack_shape = [](const TemplateDefinition& definition) {
			return definition.specialization_pattern.size() == 2 &&
				definition.specialization_pattern[0].find("<") != string::npos &&
				definition.specialization_pattern[0].find("...") != string::npos &&
				CanonicalSpelling(definition.specialization_pattern[0]) ==
				CanonicalSpelling(definition.specialization_pattern[1]);
		};
		const bool lhs_repeated_pack = repeated_pack_shape(lhs);
		const bool rhs_repeated_pack = repeated_pack_shape(rhs);
		if(lhs_repeated_pack != rhs_repeated_pack) return lhs_repeated_pack;
		const bool lhs_const_pointer = !lhs.specialization_pattern.empty() && CanonicalSpelling(lhs.specialization_pattern[0]).find("const ") == 0;
		const bool rhs_const_pointer = !rhs.specialization_pattern.empty() && CanonicalSpelling(rhs.specialization_pattern[0]).find("const ") == 0;
		if(lhs_const_pointer != rhs_const_pointer) return lhs_const_pointer;
		const auto reference_cv = [](const TemplateDefinition& definition) {
			int mask = 0;
			for(size_t i = 0; i < definition.specialization_pattern.size(); ++i) {
				const string pattern = CanonicalSpelling(definition.specialization_pattern[i]);
				if(pattern.empty() || pattern[pattern.size() - 1] != '&' ||
					(pattern.size() > 1 && pattern[pattern.size() - 2] == '&')) continue;
				if(pattern.find("const") != string::npos) mask |= 1;
				if(pattern.find("volatile") != string::npos) mask |= 2;
			}
			return mask;
		};
		const int lhs_reference_cv = reference_cv(lhs);
		const int rhs_reference_cv = reference_cv(rhs);
		if(lhs_reference_cv != rhs_reference_cv) {
			if(lhs_reference_cv && (lhs_reference_cv | rhs_reference_cv) == lhs_reference_cv) return true;
			if(rhs_reference_cv && (lhs_reference_cv | rhs_reference_cv) == rhs_reference_cv) return false;
		}
		const auto template_head = [](const TemplateDefinition& definition) {
			if(definition.specialization_pattern.empty()) return false;
			const string pattern = CanonicalSpelling(definition.specialization_pattern[0]);
			const size_t open = pattern.find('<');
			const string name = open == string::npos ? pattern : pattern.substr(0, open);
			for(size_t parameter = 0; parameter < definition.specialization_parameters.size() &&
				parameter < definition.specialization_parameter_details.size(); ++parameter)
				if(definition.specialization_parameters[parameter] == name &&
					definition.specialization_parameter_details[parameter].template_template)
					return true;
			return false;
		};
		const bool lhs_template_head = template_head(lhs);
		const bool rhs_template_head = template_head(rhs);
		if(lhs_template_head != rhs_template_head) return !lhs_template_head;
	const auto template_head_arity = [&](const TemplateDefinition& definition,
			size_t* fixed, bool* trailing_pack) {
			for(size_t pattern_index = 0; pattern_index < definition.specialization_pattern.size();
				++pattern_index) {
				const string pattern = CanonicalSpelling(
					definition.specialization_pattern[pattern_index]);
				const size_t open = pattern.find('<');
				if(open == string::npos) continue;
				const string name = CanonicalSpelling(pattern.substr(0, open));
				bool template_parameter = false;
				for(size_t parameter = 0; parameter < definition.specialization_parameters.size() &&
					parameter < definition.specialization_parameter_details.size(); ++parameter)
					if(definition.specialization_parameters[parameter] == name &&
						definition.specialization_parameter_details[parameter].template_template) {
						template_parameter = true;
						break;
					}
				if(!template_parameter) continue;
				string arguments;
				size_t close = string::npos;
				if(!TemplateRange(pattern, open, &arguments, &close)) return false;
				const vector<string> parts = SplitTemplateArguments(arguments);
				*trailing_pack = !parts.empty() && parts.back().size() > 3 &&
					parts.back().compare(parts.back().size() - 3, 3, "...") == 0;
				*fixed = parts.size() - (*trailing_pack ? 1 : 0);
				return true;
			}
			return false;
		};
		size_t lhs_fixed = 0, rhs_fixed = 0;
		bool lhs_trailing_pack = false, rhs_trailing_pack = false;
		if(template_head_arity(lhs, &lhs_fixed, &lhs_trailing_pack) &&
			template_head_arity(rhs, &rhs_fixed, &rhs_trailing_pack)) {
			if(lhs_trailing_pack != rhs_trailing_pack)
				return !lhs_trailing_pack;
			if(lhs_fixed != rhs_fixed)
				return lhs_fixed > rhs_fixed;
		}
	const auto direct_parameter = [](const TemplateDefinition& definition,
			const string& raw) {
			string name = CanonicalSpelling(raw);
			if(name.size() > 3 && name.compare(name.size() - 3, 3, "...") == 0)
				name.erase(name.size() - 3);
			for(size_t parameter = 0; parameter < definition.specialization_parameters.size();
				++parameter)
				if(definition.specialization_parameters[parameter] == name) return true;
			return false;
		};
		const size_t comparable = min(lhs.specialization_pattern.size(),
			rhs.specialization_pattern.size());
		for(size_t pattern = 0; pattern < comparable; ++pattern) {
			if(!direct_parameter(lhs, lhs.specialization_pattern[pattern]) &&
				direct_parameter(rhs, rhs.specialization_pattern[pattern])) return true;
			if(direct_parameter(lhs, lhs.specialization_pattern[pattern]) &&
				!direct_parameter(rhs, rhs.specialization_pattern[pattern])) return false;
		}
		const auto renamed_definition = [](const TemplateDefinition& definition,
			const string& side) {
			map<string, string> renames;
			for(size_t i = 0; i < definition.specialization_parameters.size(); ++i) {
				if(definition.specialization_parameters[i].empty()) continue;
				ostringstream fresh_name;
				fresh_name << "__pa21_order_" << side << "_" << i;
				renames[definition.specialization_parameters[i]] = fresh_name.str();
			}
			TemplateDefinition result = definition;
			for(size_t i = 0; i < result.specialization_parameters.size(); ++i) {
				map<string, string>::const_iterator rename = renames.find(
					result.specialization_parameters[i]);
				if(rename != renames.end()) result.specialization_parameters[i] = rename->second;
			}
			for(size_t i = 0; i < definition.specialization_pattern.size(); ++i)
				result.specialization_pattern[i] = ReplaceIdentifiers(
					definition.specialization_pattern[i], renames);
			return result;
		};
		const TemplateDefinition lhs_ordered = renamed_definition(lhs, "lhs");
		const TemplateDefinition rhs_ordered = renamed_definition(rhs, "rhs");
		set<string> rhs_names;
		set<string> lhs_names;
		for(size_t i = 0; i < rhs_ordered.specialization_parameters.size(); ++i)
			if(!rhs_ordered.specialization_parameters[i].empty()) rhs_names.insert(
				rhs_ordered.specialization_parameters[i]);
		for(size_t i = 0; i < lhs_ordered.specialization_parameters.size(); ++i)
			if(!lhs_ordered.specialization_parameters[i].empty()) lhs_names.insert(
				lhs_ordered.specialization_parameters[i]);
		const auto function_shape = [this](const TemplateDefinition& definition,
			string* result, vector<string>* parameters, string* qualifiers,
			bool* trailing_pack) {
			for(size_t pattern = 0; pattern < definition.specialization_pattern.size(); ++pattern) {
				vector<string> parts;
				if(!SplitDirectFunctionType(CanonicalSpelling(
					definition.specialization_pattern[pattern]), result, &parts, qualifiers)) continue;
				*trailing_pack = !parts.empty() && IsTopLevelPackPattern(parts.back());
				*parameters = parts;
				return true;
			}
			return false;
		};
		string lhs_function_result, rhs_function_result, lhs_function_qualifiers,
			rhs_function_qualifiers;
		vector<string> lhs_function_parameters, rhs_function_parameters;
		bool lhs_function_pack = false, rhs_function_pack = false;
		if(function_shape(lhs_ordered, &lhs_function_result, &lhs_function_parameters,
			&lhs_function_qualifiers, &lhs_function_pack) &&
			function_shape(rhs_ordered, &rhs_function_result, &rhs_function_parameters,
			&rhs_function_qualifiers, &rhs_function_pack) &&
			lhs_function_qualifiers == rhs_function_qualifiers) {
			const auto accepts_function_shape = [this](const string& pattern_result,
				const vector<string>& pattern_parameters, bool pattern_pack,
				const set<string>& pattern_names, const string& actual_result,
				const vector<string>& actual_parameters) {
				map<string, string> inferred;
				if(!MatchOrderingTypePattern(pattern_result, actual_result, pattern_names,
					&inferred)) return false;
				const size_t fixed = pattern_parameters.size() - (pattern_pack ? 1 : 0);
				if(actual_parameters.size() < fixed ||
					(!pattern_pack && actual_parameters.size() != fixed)) return false;
				for(size_t parameter = 0; parameter < fixed; ++parameter)
					if(!MatchOrderingTypePattern(pattern_parameters[parameter],
						actual_parameters[parameter], pattern_names, &inferred)) return false;
				if(pattern_pack) {
					const string pack = CanonicalSpelling(pattern_parameters.back().substr(
						0, pattern_parameters.back().size() - 3));
					if(pattern_names.find(pack) == pattern_names.end()) return false;
					for(size_t parameter = fixed; parameter < actual_parameters.size(); ++parameter)
						if(!MatchOrderingTypePattern(pack, actual_parameters[parameter],
							pattern_names, &inferred)) return false;
				}
				return true;
			};
			if(!lhs_function_pack && rhs_function_pack && accepts_function_shape(
				rhs_function_result, rhs_function_parameters, true, rhs_names,
				lhs_function_result, lhs_function_parameters)) return true;
			if(lhs_function_pack && !rhs_function_pack && accepts_function_shape(
				lhs_function_result, lhs_function_parameters, true, lhs_names,
				rhs_function_result, rhs_function_parameters)) return false;
		}
		const auto template_shape = [this](const TemplateDefinition& definition,
			string* base, vector<string>* parts, bool* trailing_pack) {
			for(size_t pattern = 0; pattern < definition.specialization_pattern.size(); ++pattern) {
				const string raw = CanonicalSpelling(definition.specialization_pattern[pattern]);
				const size_t open = raw.find('<');
				if(open == string::npos) continue;
				string arguments;
				size_t close = string::npos;
				if(!TemplateRange(raw, open, &arguments, &close)) continue;
				*base = CanonicalSpelling(raw.substr(0, open));
				*parts = SplitTemplateArguments(arguments);
				*trailing_pack = !parts->empty() &&
					IsTopLevelPackPattern(parts->back());
				return true;
			}
			return false;
		};
		string lhs_base, rhs_base;
		vector<string> lhs_parts, rhs_parts;
		bool lhs_shape_pack = false, rhs_shape_pack = false;
		if(template_shape(lhs_ordered, &lhs_base, &lhs_parts, &lhs_shape_pack) &&
			template_shape(rhs_ordered, &rhs_base, &rhs_parts, &rhs_shape_pack) &&
			lhs_base == rhs_base) {
			const auto matches_ordering_pattern = [this](const string& pattern,
				const string& actual, const set<string>& names) {
				map<string, string> inferred;
				return MatchOrderingTypePattern(pattern, actual, names, &inferred);
			};
			const auto accepts_fixed_shape = [&](const vector<string>& pattern_parts,
				bool pattern_pack, const set<string>& pattern_names,
				const string& pattern_pack_part, const vector<string>& actual_parts) {
				const size_t fixed = pattern_parts.size() - (pattern_pack ? 1 : 0);
				if(actual_parts.size() < fixed) return false;
				for(size_t part = 0; part < fixed; ++part)
					if(!matches_ordering_pattern(pattern_parts[part], actual_parts[part],
						pattern_names)) return false;
				if(!pattern_pack && actual_parts.size() != fixed) return false;
				if(pattern_pack) for(size_t part = fixed; part < actual_parts.size(); ++part)
					if(!matches_ordering_pattern(pattern_pack_part, actual_parts[part],
						pattern_names)) return false;
				return true;
			};
			const string lhs_pack_part = lhs_shape_pack ? CanonicalSpelling(
				lhs_parts.back().substr(0, lhs_parts.back().size() - 3)) : string();
			const string rhs_pack_part = rhs_shape_pack ? CanonicalSpelling(
				rhs_parts.back().substr(0, rhs_parts.back().size() - 3)) : string();
			if(!lhs_shape_pack && rhs_shape_pack && accepts_fixed_shape(rhs_parts,
				true, rhs_names, rhs_pack_part, lhs_parts)) return true;
			if(lhs_shape_pack && !rhs_shape_pack && accepts_fixed_shape(lhs_parts,
				true, lhs_names, lhs_pack_part, rhs_parts)) return false;
			if(lhs_shape_pack && rhs_shape_pack &&
				lhs_parts.size() == rhs_parts.size()) {
				vector<string> lhs_fixed(lhs_parts.begin(), lhs_parts.end() - 1);
				vector<string> rhs_fixed(rhs_parts.begin(), rhs_parts.end() - 1);
				const bool rhs_accepts_lhs = accepts_fixed_shape(rhs_parts, true,
					rhs_names, rhs_pack_part, lhs_fixed) &&
					matches_ordering_pattern(rhs_pack_part, lhs_pack_part, rhs_names);
				const bool lhs_accepts_rhs = accepts_fixed_shape(lhs_parts, true,
					lhs_names, lhs_pack_part, rhs_fixed) &&
					matches_ordering_pattern(lhs_pack_part, rhs_pack_part, lhs_names);
				if(rhs_accepts_lhs != lhs_accepts_rhs) return rhs_accepts_lhs;
			}
		}
		map<string, string> rhs_inferred;
		map<string, string> lhs_inferred;
		return MatchOrderingPatternList(rhs_ordered.specialization_pattern,
			lhs_ordered.specialization_pattern, rhs_names, &rhs_inferred) &&
			!MatchOrderingPatternList(lhs_ordered.specialization_pattern,
				rhs_ordered.specialization_pattern, lhs_names, &lhs_inferred);
	}
bool PA18TemplateExpander::IsTemplatePackName(const TemplateDefinition& definition,
	const string& name) const
{
	for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter)
		if(definition.parameters[parameter].pack &&
			definition.parameters[parameter].name == name) return true;
	return template_pack_names_.find(name) != template_pack_names_.end();
}
bool PA18TemplateExpander::IsTopLevelPackPattern(const string& value) const
{
	if(value.size() <= 3 || value.compare(value.size() - 3, 3, "...") != 0) return false;
	int angle = 0, paren = 0, bracket = 0;
	for(size_t i = 0; i < value.size(); ++i) {
		if(value[i] == '<' && IsTemplateAngleOpen(value, i)) ++angle;
		else if(value[i] == '>' && angle > 0 && IsTemplateAngleClose(value, i)) --angle;
		else if(value[i] == '(') ++paren;
		else if(value[i] == ')' && paren > 0) --paren;
		else if(value[i] == '[') ++bracket;
		else if(value[i] == ']' && bracket > 0) --bracket;
	}
	return angle == 0 && paren == 0 && bracket == 0;
}
bool PA18TemplateExpander::TransformPackChild(
	const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& original_child,
	const string& child_context,
	const map<string, string>& substitutions,
	map<string, string>* local_substitutions,
	const CPPGMAstNodePtr& result)
{
	if(original_child && original_child->kind == "unary-expression" && original_child->children.size() == 1 && original_child->children[0] && original_child->children[0]->kind == "pack-expansion-expression") {
		const CPPGMAstNodePtr expansion = original_child->children[0];
		const string pack_name = PackExpansionIdentifier(expansion->children.empty() ? CPPGMAstNodePtr() : expansion->children[0]);
		map<string, vector<string> >::const_iterator pack = active_pack_substitutions_.find(pack_name);
		if(pack != active_pack_substitutions_.end()) {
			const vector<string> pack_values = pack->second;
			for(size_t element = 0; element < pack_values.size(); ++element) {
				CPPGMAstNodePtr expanded = CloneNode(original_child);
				CPPGMAstNodePtr body = expansion->children.empty() ? CPPGMAstNodePtr() : CloneNode(expansion->children[0]);
				if(!body) continue;
				RemoveParameterPackMarkers(body); expanded->children[0] = body;
				map<string, string> one = substitutions; one[pack_name] = pack_values[element];
				if(CPPGMAstNodePtr child = TransformNode(expanded, child_context, one))
					result->children.push_back(child);
			}
			return true;
		}
	}
	if(input->kind == "base-clause" && original_child &&
		original_child->kind == "base-specifier" &&
		ChildOfKindLocal(original_child, "pack-expansion")) {
		const CPPGMAstNodePtr original_base = ChildOfKindLocal(original_child, "base-name");
		const string pack_name = PackExpansionIdentifier(original_base);
		map<string, vector<string> >::const_iterator pack =
			active_pack_substitutions_.find(pack_name);
		if(pack != active_pack_substitutions_.end()) {
			const vector<string> pack_values = pack->second;
			for(size_t element = 0; element < pack_values.size(); ++element) {
				CPPGMAstNodePtr expanded = CloneNode(original_child);
				RemoveParameterPackMarkers(expanded);
				const CPPGMAstNodePtr base = ChildOfKindLocal(expanded, "base-name");
				if(base && original_base) {
					map<string, string> one = substitutions;
					one[pack_name] = pack_values[element];
					base->value = ReplaceIdentifiers(original_base->value, one);
				}
				CPPGMAstNodePtr child = TransformNode(expanded, child_context,
					substitutions);
				if(child) result->children.push_back(child);
			}
		}
		return true;
	}
	if(original_child && original_child->kind == "pack-expansion-expression") {
		ExpandPackChild(input, original_child, child_context, substitutions,
			local_substitutions, result);
		return true;
	}
	if(input->kind == "parameter-clause" && original_child &&
		original_child->kind == "parameter-declaration" &&
		IsFunctionParameterPack(original_child)) {
		const string pack_name = PackExpansionIdentifier(original_child);
		map<string, vector<string> >::const_iterator values =
			active_pack_substitutions_.find(pack_name);
		if(values != active_pack_substitutions_.end()) {
			const vector<string> pack_values = values->second;
			const string identifier = ParameterIdentifier(original_child);
			vector<string>& expanded_identifiers =
				active_pack_identifier_substitutions_[identifier];
			for(size_t element = 0; element < pack_values.size(); ++element) {
				ostringstream pack_suffix;
				pack_suffix << element + 1;
				const string expanded_name = identifier.empty() || element == 0 ? identifier :
					identifier + "__pack" + pack_suffix.str();
				if(!identifier.empty()) expanded_identifiers.push_back(expanded_name);
				map<string, string> one = substitutions;
				one[pack_name] = pack_values[element];
					if(!identifier.empty()) one[identifier] = expanded_name;
					CPPGMAstNodePtr copy = CloneNode(original_child);
					RemoveParameterPackMarkers(copy);
					CPPGMAstNodePtr child = TransformNode(copy, child_context, one);
					if(child) result->children.push_back(child);
			}
			return true;
		}
	}
	return false;
}
void PA18TemplateExpander::RecordUsingDirective(const CPPGMAstNodePtr& original_child,
	map<string, string>* local_substitutions)
{
	const CPPGMAstNodePtr target = ChildOfKindLocal(original_child, "target");
	if(!target || target->value.empty()) return;
	const string prefix = target->value + "::";
	for(set<string>::const_iterator type = class_contexts_.lower_bound(prefix);
		type != class_contexts_.end() &&
			type->compare(0, prefix.size(), prefix) == 0; ++type) {
		const string relative = type->substr(prefix.size());
		const size_t separator = relative.find("::");
		const string visible = relative.substr(0, separator);
		if(variable_types_.find(visible) != variable_types_.end()) continue;
		if(!visible.empty() && local_substitutions->find(visible) == local_substitutions->end())
			(*local_substitutions)[visible] = target->value + "::" + visible;
	}
	map<string, vector<const TemplateDefinition*> >::const_iterator indexed =
		using_directive_exports_.find(target->value);
	if(indexed == using_directive_exports_.end()) return;
	for(size_t index = 0; index < indexed->second.size(); ++index) {
		const TemplateDefinition* definition = indexed->second[index];
		if(!definition) continue;
		const string qualified_prefix = target->value + "::";
		if(definition->qualified_name.compare(0, qualified_prefix.size(),
			qualified_prefix) != 0) continue;
		const string visible = definition->qualified_name.substr(qualified_prefix.size());
		if(visible.empty() || visible.find("::") != string::npos) continue;
		if(variable_types_.find(visible) != variable_types_.end()) continue;
		if(local_substitutions->find(visible) == local_substitutions->end())
			(*local_substitutions)[visible] = definition->qualified_name;
	}
}
void PA18TemplateExpander::TransformRegularChildren(const CPPGMAstNodePtr& input,
	const string& child_context, const string& function_context,
	const map<string, string>& substitutions,
	map<string, string>* local_substitutions,
	const CPPGMAstNodePtr& result)
{
	const CPPGMAstNodePtr source_declarator = FunctionDeclarator(input); const bool function_declaration = input && input->kind == "function-definition"; for(size_t i = 0; i < input->children.size(); ++i) { const CPPGMAstNodePtr original_child = input->children[i];
		const bool transformed_pack_child = TransformPackChild(input, original_child, child_context, substitutions, local_substitutions, result); if(transformed_pack_child) continue;
			if(input->kind == "decl-specifier" && input->value.find("decltype(") != string::npos &&
				original_child && (original_child->kind == "call-expression" ||
				original_child->kind == "binary-expression" || original_child->kind == "conditional-expression" ||
				original_child->kind == "member-expression")) {
				result->children.push_back(CloneNode(original_child)); continue;
			}
				if(SkipUnusedNestedClass(input, original_child, child_context, substitutions, i)) continue; if(original_child && original_child->kind == "namespace-alias-definition") {
				const CPPGMAstNodePtr target = ChildOfKindLocal(original_child, "target");
				if(target && !target->value.empty() && local_substitutions)
					(*local_substitutions)[original_child->value] = RewriteText(
						target->value, child_context, *local_substitutions, 0, false);
				continue;
			}
			const bool declarator_with_trailing_return = function_declaration && original_child && original_child->kind == "declarator" && DescendantOfKind(original_child, "trailing-return-type");
			const bool function_child_context = function_declaration && original_child && (original_child->kind == "compound-statement" || declarator_with_trailing_return || original_child->kind == "trailing-return-type");
			const string node_context = function_child_context ? function_context : child_context;
			const CPPGMAstNodePtr using_target = original_child && original_child->kind == "using-declaration" ? ChildOfKindLocal(original_child, "target") : CPPGMAstNodePtr();
			const size_t using_separator = using_target ? using_target->value.rfind("::") : string::npos;
			string using_owner = using_separator == string::npos ? string() : using_target->value.substr(0, using_separator);
			const size_t using_owner_open = using_owner.find('<');
			if(using_owner_open != string::npos) using_owner.erase(using_owner_open);
			const bool constructor_using = using_target && using_separator != string::npos && LastComponent(using_owner) == LastComponent(using_target->value);
			const bool drop_function_using = using_target && IsOrdinaryTemplateUsingTarget(using_target->value, node_context) && class_contexts_.find(node_context) == class_contexts_.end() &&
				!IsGeneratedMemberTemplateUsingTarget(using_target->value, node_context, local_substitutions ? *local_substitutions : substitutions) && !constructor_using; CPPGMAstNodePtr child;
				if(input->kind == "using-declaration" && original_child && original_child->kind == "target") {
					child = CloneNode(original_child);
					const string raw_target = original_child->value;
				const size_t separator = raw_target.rfind("::"); if(separator != string::npos && raw_target.substr(0, separator) == raw_target.substr(separator + 2)) {
					map<string, string>::const_iterator alias = local_substitutions->find(raw_target.substr(0, separator));
					if(alias != local_substitutions->end() && !alias->second.empty())
						child->value = alias->second + "::" + LastComponent(alias->second);
						else child->value = RewriteText(raw_target, node_context, *local_substitutions, 0, false, false);
					} else child->value = RewriteText(raw_target, node_context, *local_substitutions, 0, false, false);
					} else if(input->kind == "member-expression" && i == 1 && original_child && original_child->kind == "identifier" && IsKnownMemberTemplateId(original_child->value)) child = CloneNode(original_child);
				else if(input->kind == "class-specifier" && HasReplayContext(substitutions) && original_child && original_child->kind == "function-definition" && !original_child->children.empty() && HasDeclarationSpecifier(original_child->children[0], "static")) try {
					child = TransformNode(original_child, node_context, *local_substitutions);
				}
				catch(const PA18SubstitutionFailure&) { continue; } else child = TransformNode(original_child, node_context, *local_substitutions);
				if(child && input->kind == "array-suffix" && !child->children.empty() && child->children[0]) {
					PA19IntegralValue bound; const string expression = ConstantExpressionSpelling(child->children[0]);
					if(EvaluateIntegralText(expression, node_context, *local_substitutions, &bound)) child->children[0] = CPPGMAstNodePtr(new CPPGMAstNode("literal", IntegralValueSpelling(bound)));
				}
			if(child && input->kind == "class-specifier" && child->kind == "simple-declaration" && HasReplayContext(substitutions)) RecordConstantDeclaration(child, active_instantiation_name_.empty() ? child_context : active_instantiation_name_, *local_substitutions);
			if(child && input->kind == "class-specifier" && child->kind == "simple-declaration") RecordConstantArrayDeclaration(child, active_instantiation_name_.empty() ? child_context : active_instantiation_name_, *local_substitutions);
				if(!child && input->kind == "decl-specifier-seq" && original_child &&
					(original_child->kind == "class-specifier" ||
						original_child->kind == "class-forward-declaration")) {
					map<string, string>::const_iterator promoted = local_class_names_.find(
						JoinPath(child_context, LastComponent(original_child->value)));
					if(promoted != local_class_names_.end())
						result->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
							"decl-specifier", "TT_IDENTIFIER:" + LastComponent(promoted->second))));
				}
				if(child && !drop_function_using && !(input->kind == "compound-statement" && HasReplayContext(substitutions) &&
					(original_child->kind == "simple-declaration" &&
					SpellNode(original_child->children.empty() ? CPPGMAstNodePtr() : original_child->children[0]).find("typedef") != string::npos))) result->children.push_back(child);
				if(original_child && original_child->kind == "alias-declaration" && !original_child->value.empty() &&
					!original_child->children.empty())
				{
					(*local_substitutions)[original_child->value] = RewriteText(
						TypeIdSpelling(original_child->children[0]), child_context,
						*local_substitutions, 0);
				}
			if(original_child && original_child->kind == "using-declaration") {
				const CPPGMAstNodePtr target = ChildOfKindLocal(original_child, "target");
				if(target && !target->value.empty()) {
					const string target_name = LastComponent(target->value);
					const string owner = PrefixComponent(target->value);
					const CPPGMAstNodePtr rewritten = child ?
						ChildOfKindLocal(child, "target") : CPPGMAstNodePtr();
					bool function_target = false;
					if(!owner.empty()) {
						const string target_suffix = "::" + target->value;
						for(map<string, TemplateDefinition>::const_iterator candidate = definitions_.begin();
							candidate != definitions_.end(); ++candidate)
							if(!candidate->second.class_template &&
								(candidate->second.qualified_name == target->value ||
									(candidate->second.qualified_name.size() > target_suffix.size() &&
									 candidate->second.qualified_name.compare(
										candidate->second.qualified_name.size() - target_suffix.size(),
										target_suffix.size(), target_suffix) == 0))) {
								function_target = true;
								break;
							}
						const string signature_suffix = "::" + target->value;
						for(map<string, FunctionSignature>::const_iterator candidate = function_signatures_.begin();
							candidate != function_signatures_.end(); ++candidate)
							if(candidate->first == target->value ||
								(candidate->first.size() > signature_suffix.size() &&
								 candidate->first.compare(candidate->first.size() - signature_suffix.size(),
									signature_suffix.size(), signature_suffix) == 0)) {
								function_target = true;
								break;
							}
						const string rewritten_owner = rewritten ? PrefixComponent(rewritten->value) : owner;
						const CPPGMAstNodePtr owner_declaration = FindClassDeclaration(rewritten_owner.empty() ? owner : rewritten_owner, child_context);
						if(owner_declaration) for(size_t member = 0; member < owner_declaration->children.size(); ++member) {
								const CPPGMAstNodePtr declaration = owner_declaration->children[member];
								if(declaration && declaration->kind == "function-definition" &&
									declaration->children.size() > 1 &&
									LastComponent(FirstIdentifierLocal(declaration->children[1])) == target_name) {
									function_target = true;
									break;
								}
							}
					}
					if(!owner.empty() && !function_target)
						(*local_substitutions)[target_name] = rewritten &&
							!rewritten->value.empty() ? rewritten->value : target->value;
				}
				} if(original_child && original_child->kind == "using-directive") RecordUsingDirective(original_child, local_substitutions);
			if(original_child && original_child->kind == "simple-declaration" && !original_child->children.empty() &&
				HasDeclarationSpecifier(original_child->children[0], "typedef") &&
				(HasReplayContext(substitutions) || SpellNode(original_child).find('<') != string::npos))
				RecordTypedefSubstitutions(original_child, child_context, local_substitutions);
		}
}
bool PA18TemplateExpander::PreserveDependentStaticDeclarator(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions, const CPPGMAstNodePtr& result,
	string* promoted_name)
{
	if(!input || (input->kind != "identifier" && input->kind != "id-expression")) return false;
	const string raw = CanonicalSpelling(RemoveMarker(input->value));
	const size_t open = raw.find('<');
	string argument_text;
	size_t close = string::npos;
	string owner_base;
	size_t owner_begin = 0;
	const bool has_owner_base = open != string::npos &&
		TemplateBase(raw, open, &owner_begin, &owner_base);
	map<string, string>::const_iterator concrete_owner = substitutions.find(owner_base);
	const bool concrete_replay_owner = has_owner_base && concrete_owner != substitutions.end() &&
		class_contexts_.find(concrete_owner->second) != class_contexts_.end() &&
		specialization_bases_.find(LastComponent(concrete_owner->second)) !=
			specialization_bases_.end();
	const bool concrete_static_data = concrete_replay_owner && HasStaticMember(
		0, concrete_owner->second, LastComponent(raw));
	bool source_static_data = false;
	string source_generated_owner;
	if(input->kind == "id-expression")
		source_static_data = FindSourceStaticArrayOwner(raw, context,
			&source_generated_owner);
	if((!concrete_static_data && !source_static_data) || open == string::npos ||
		!TemplateRange(raw, open, &argument_text, &close) ||
		close + 2 >= raw.size() || raw.compare(close + 1, 2, "::") != 0) {
		return false;
	}
	const vector<string> arguments = SplitTemplateArguments(argument_text);
	bool dependent_owner = false;
	for(size_t argument = 0; argument < arguments.size() && !dependent_owner; ++argument) {
		string value = CanonicalSpelling(NormalizeElaboratedSpelling(arguments[argument], context));
		const char* const keys[] = {"struct ", "class ", "union ", "enum "};
		for(size_t key = 0; key < sizeof(keys) / sizeof(keys[0]); ++key) {
			const string keyword = keys[key];
			if(value.compare(0, keyword.size(), keyword) == 0) {
				value = CanonicalSpelling(value.substr(keyword.size()));
				break;
			}
		}
		bool bare = !value.empty() &&
			(isalpha(static_cast<unsigned char>(value[0])) || value[0] == '_');
		for(size_t character = 1; bare && character < value.size(); ++character)
			if(!IsIdentifierCharacter(value[character])) bare = false;
		const bool builtin = value == "bool" || value == "char" || value == "double" ||
			value == "float" || value == "int" || value == "long" ||
			value == "short" || value == "signed" || value == "unsigned" ||
			value == "void" || value == "wchar_t" || value == "true" ||
			value == "false" || value == "nullptr";
		const bool known = class_contexts_.find(value) != class_contexts_.end() ||
			named_type_contexts_.find(value) != named_type_contexts_.end() ||
			type_aliases_.find(value) != type_aliases_.end() ||
			FindDefinition(value, context) != 0;
		if(bare && !builtin && !known) dependent_owner = true;
	}
	if(!dependent_owner) {
		// This is a declarator name, not a type-use expression.  A static member
		// lookup may nevertheless resolve the qualified spelling to its declared
		// array type; retain the concrete owner/member identity instead of letting
		// that type spelling replace the identifier.
		result->value = concrete_static_data ? concrete_owner->second + "::" +
			LastComponent(raw) : !source_generated_owner.empty() ?
			source_generated_owner + "::" + LastComponent(raw) : input->value;
		*promoted_name = string();
		return concrete_static_data || source_static_data;
	}
	result->value = input->value;
	*promoted_name = string();
	return true;
}
CPPGMAstNodePtr PA18TemplateExpander::FinishRegularNode(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions,
	const CPPGMAstNodePtr& result, const string& promoted_local_class,
	bool defer_type_only_classes)
{
		string child_context = context;
		if(input->kind == "class-specifier" || input->kind == "class-forward-declaration")
			child_context = JoinPath(context, LastComponent(input->value));
		const CPPGMAstNodePtr source_declarator = FunctionDeclarator(input); const bool function_declaration = input->kind == "function-definition";
		string function_context = context;
		if(function_declaration) {
			const string function_name = DeclarationName(input);
			string function_owner;
			if(source_declarator) {
				const string qualified_name = RewriteText(
					FirstIdentifierLocal(source_declarator), context, substitutions, 0,
					false, false);
				function_owner = PrefixComponent(qualified_name);
				if(!function_owner.empty()) function_owner = ResolveGeneratedFunctionOwner(
					function_owner, context, &child_context);
			}
			function_context = JoinPath(function_owner.empty() ? context : function_owner,
				function_name);
			if(!function_name.empty() && LastComponent(context) == function_name)
				function_context = context;
		}
		if(function_declaration) {
			const CPPGMAstNodePtr source_parameters = DescendantOfKind(
				source_declarator, "parameter-clause");
			if(source_parameters) for(size_t parameter = 0;
				parameter < source_parameters->children.size(); ++parameter) {
				const CPPGMAstNodePtr parameter_node = source_parameters->children[parameter];
				if(!parameter_node || parameter_node->kind != "parameter-declaration") continue;
				const string name = ParameterIdentifier(parameter_node);
				if(name.empty()) continue;
				string type = ParameterTypeSpelling(parameter_node); const string rewritten_type = ContainsSubstitutionIdentifier(type, substitutions) ? RewriteText(type, context, substitutions, 0, true, true) : string();
				if(!rewritten_type.empty()) type = rewritten_type; else type = CanonicalSpelling(ReplaceIdentifiers(type, substitutions));
				function_parameter_types_[function_context][name] = type;
			}
		}
		map<string, string> local_substitutions = substitutions;
		struct TypeOnlyScope { size_t& depth; const size_t saved; TypeOnlyScope(size_t& d, bool active) : depth(d), saved(d) { if(active) ++depth; } ~TypeOnlyScope() { depth = saved; } } type_only_scope(defer_type_only_class_definitions_, defer_type_only_classes);
		TransformRegularChildren(input, child_context, function_context, substitutions, &local_substitutions, result);
		RecoverDependentSizeofArrayType(input, result);
		if(input->kind == "function-definition") MaterializeReturnConversions(input, result, context, function_context, local_substitutions);
		if(input->kind == "class-specifier" || input->kind == "class-forward-declaration") {
			string class_name = LastComponent(input->value);
			const size_t class_angle = class_name.find('<');
			if(class_angle != string::npos) class_name.erase(class_angle);
			for(size_t child = 0; child < input->children.size(); ++child) {
				CPPGMAstNodePtr declaration = input->children[child];
				while(declaration && declaration->kind == "template-declaration" &&
					declaration->children.size() > 1)
					declaration = declaration->children[1];
				if(declaration && (declaration->kind == "special-member-definition" ||
					declaration->kind == "special-member-declaration") &&
					LastComponent(declaration->value) == class_name) {
					result->has_deferred_constructor = true;
					break;
				}
			}
		}
		if(ConsumeMaterializedStaticAssert(input, result, child_context, local_substitutions)) return CPPGMAstNodePtr();
		if(input->kind == "array-suffix" && !result->children.empty() && result->children[0]) { PA19IntegralValue bound;
			if(EvaluateIntegralText(ConstantExpressionSpelling(result->children[0]), child_context, local_substitutions, &bound))
				result->children[0] = CPPGMAstNodePtr(new CPPGMAstNode("literal", IntegralValueSpelling(bound)));
		}
		if((input->kind == "parameter-declaration" || input->kind == "type-id") &&
			HasReplayContext(substitutions))
			for(map<string, string>::const_iterator substitution = substitutions.begin();
				substitution != substitutions.end(); ++substitution)
				if(!substitution->second.empty() && substitution->second[substitution->second.size() - 1] == '&' &&
					ContainsName(input, substitution->first) &&
					ContainsName(input, "&&"))
					CollapseForwardingReference(result);
		RewriteTemplateInitializer(input, context, substitutions, result);
		if(active_template_declaration_depth_ == 0) {
			MaterializeInitializerConstructor(input, result, context, substitutions);
			MaterializeOrdinaryInitializerConversions(input, result, context, substitutions);
		}
		if(input->kind == "simple-declaration")
			DeduceAutoInitializerType(result, context, substitutions);
		if(input->kind == "simple-declaration") ReifyReferenceType(result);
		if(input->kind == "conditional-expression")
			MaterializeConditionalConversions(result, context, substitutions);
		if(input->kind == "binary-expression" || input->kind == "assignment-expression") { InstantiateOperatorTemplate(result, context, substitutions); RewriteOperatorFunctionArgument(result, context, substitutions); }
		if(!promoted_local_class.empty()) {
			map<string, string> promoted_substitution;
			promoted_substitution[LastComponent(input->value)] = promoted_local_class;
			function<void(const CPPGMAstNodePtr&)> rewrite_promoted_types =
				[&](const CPPGMAstNodePtr& node) {
					if(!node) return;
					if(node->kind == "decl-specifier" || node->kind == "type-name" ||
						node->kind == "type-specifier") {
						const size_t marker = node->value.find(':');
						const string prefix = marker != string::npos ?
							node->value.substr(0, marker + 1) : string();
						node->value = prefix + ReplaceIdentifiers(
							RemoveMarker(node->value), promoted_substitution);
					}
					for(size_t child = 0; child < node->children.size(); ++child)
						rewrite_promoted_types(node->children[child]);
				};
			for(size_t child = 0; child < result->children.size(); ++child) {
				const CPPGMAstNodePtr member = result->children[child];
				if(!member || (member->kind != "special-member-definition" &&
					member->kind != "special-member-declaration")) continue;
				member->value = promoted_local_class;
				const CPPGMAstNodePtr declarator = FunctionDeclarator(member);
				RenameParameterIdentifier(declarator, promoted_local_class);
				rewrite_promoted_types(member);
			}
			const string promoted_local_name = PromotedLocalClass(promoted_local_class, context);
			string generated_owner = promoted_local_name.empty() ? string() :
				PrefixComponent(promoted_local_name);
			if(promoted_local_name.empty()) {
				const map<string, string>::const_iterator owner = function_owners_.find(context);
				generated_owner = owner == function_owners_.end() ? PrefixComponent(context) : owner->second;
			}
			generated_by_owner_[generated_owner].push_back(result);
		return CPPGMAstNodePtr();
	}
	return result;
}
bool PA18TemplateExpander::TransformExplicitSpecialization(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>&)
{
	if(!input || input->kind != "template-declaration" || input->children.size() <= 1 ||
		!input->children[0] || !input->children[1] ||
		!Parameters(input->children[0]).empty()) return false;
	map<const CPPGMAstNode*, vector<string> >::const_iterator explicit_arguments =
		explicit_function_arguments_.find(input->children[1].get());
	if(explicit_arguments == explicit_function_arguments_.end()) return false;
	string raw_name = DeclarationName(input->children[1]);
	if(input->children[1]->kind == "simple-declaration") {
		const CPPGMAstNodePtr list = ChildOfKindLocal(input->children[1], "init-declarator-list");
		if(list && !list->children.empty() && list->children[0] &&
			!list->children[0]->children.empty())
			raw_name = FirstIdentifierLocal(list->children[0]->children[0]);
	}
	const size_t open = raw_name.find('<');
	string base = open == string::npos ? raw_name : raw_name.substr(0, open);
	if(open != string::npos) {
		string ignored_arguments;
		size_t close = string::npos;
		if(TemplateRange(raw_name, open, &ignored_arguments, &close) &&
			close + 1 < raw_name.size() && raw_name.compare(close + 1, 2, "::") == 0)
			base += raw_name.substr(close + 1);
	}
	const TemplateDefinition* primary = 0;
	map<const CPPGMAstNode*, const TemplateDefinition*>::const_iterator primary_found =
		explicit_function_primary_definitions_.find(input->children[1].get());
	if(primary_found != explicit_function_primary_definitions_.end()) primary = primary_found->second;
	const TemplateDefinition* specialization = primary ?
		FindExplicitFunctionSpecialization(primary, explicit_arguments->second) :
		FindExplicitFunctionSpecialization(base, explicit_arguments->second, context);
	if(!specialization) return false;
	string owner_local;
	string owner_base = specialization->owner;
	const size_t owner_angle = owner_base.find('<');
	if(owner_angle != string::npos) owner_base.erase(owner_angle);
	const TemplateDefinition* owner_definition = owner_base.empty() ? 0 :
		FindDefinition(owner_base, context);
	if(owner_definition && owner_definition->class_template)
		owner_local = Instantiate(*owner_definition, explicit_arguments->second, context);
	if(owner_local.empty()) Instantiate(*specialization, explicit_arguments->second, context);
	else Instantiate(*specialization, explicit_arguments->second, context, false, 0, 0,
		&owner_local);
	return true;
}
CPPGMAstNodePtr PA18TemplateExpander::TransformRegularNode(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions)
{
	if(input->kind == "subscript-expression") {
		CPPGMAstNodePtr transformed = TransformSubscriptExpression(input, context,
			substitutions);
		if(transformed) return transformed;
	}
	if(input->kind == "assignment-expression") {
		CPPGMAstNodePtr transformed = TransformAssignmentExpression(input, context,
			substitutions);
		if(transformed) return transformed;
	}
	if(input->kind == "unary-expression") {
		CPPGMAstNodePtr transformed = TransformUnaryExpression(input, context,
			substitutions);
		if(transformed) return transformed;
	}
		CPPGMAstNodePtr user_defined_literal = RewriteUserDefinedIntegerLiteral(
			input, context, substitutions);
		if(user_defined_literal) return user_defined_literal;
		if(input->kind == "sizeof-pack-expression") {
			const string name = PackExpansionIdentifier(
				input->children.empty() ? CPPGMAstNodePtr() : input->children[0]);
			map<string, vector<string> >::const_iterator typed =
				active_pack_substitutions_.find(name);
			map<string, vector<string> >::const_iterator named =
				active_pack_identifier_substitutions_.find(name);
			const vector<string>* values = typed != active_pack_substitutions_.end() ?
				&typed->second : named != active_pack_identifier_substitutions_.end() ?
				&named->second : 0;
			if(values) {
				ostringstream count;
				count << values->size();
				CPPGMAstNodePtr result(new CPPGMAstNode("sizeof-pack-expression",
					count.str()));
	return result;
	}
		}
		CPPGMAstNodePtr template_call = RewriteTemplateCastCall(input, context, substitutions);
		if(template_call) return template_call;
		CPPGMAstNodePtr result(new CPPGMAstNode(input->kind, input->value));
		result->initializer_form = input->initializer_form;
		result->template_instantiation = input->template_instantiation;
		result->explicit_specialization = input->explicit_specialization;
		result->explicit_instantiation = input->explicit_instantiation;
		result->extern_instantiation = input->extern_instantiation;
		result->dependent_base_lookup = input->dependent_base_lookup;
		result->has_deferred_constructor = input->has_deferred_constructor;
		result->materialize_object_address = input->materialize_object_address;
		result->source_token_begin = input->source_token_begin;
		result->source_token_end = input->source_token_end;
		if(input->kind == "simple-declaration" &&
			SpellNode(input).find("decltype(") != string::npos) {
			result->materialize_object_address = true;
			const string spelling = SpellNode(input);
			const size_t open = spelling.find("decltype(");
			const size_t close = open == string::npos ? string::npos :
				spelling.find(')', open + 9);
			if(close != string::npos) {
				const string expression = spelling.substr(open + 9, close - open - 9);
				const size_t call = expression.find('(');
				if(call != string::npos)
					result->materialize_object_name = expression.substr(0, call);
			}
		}
	string promoted_local_class;
	CPPGMAstNodePtr rewritten = RewriteRegularNodeValue(
		input, context, substitutions, result, &promoted_local_class);
	if(rewritten) return rewritten;
	const bool defer_type_only_classes = input->kind == "alias-declaration" || (input->kind == "simple-declaration" && !input->children.empty() && SpellNode(input->children[0]).find("typedef") != string::npos);
	return FinishRegularNode(input, context, substitutions, result, promoted_local_class, defer_type_only_classes); } } // namespace pa18_templates_internal
