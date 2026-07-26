#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"


using namespace std;

namespace pa18_templates_internal {

int PA18TemplateExpander::MatchDirectTypeParameter(const string& pattern,
	const string& actual, const set<string>& parameter_names,
	map<string, string>* inferred, const string& context, bool class_pattern) const
{
	if(parameter_names.find(pattern) == parameter_names.end() ||
		(!class_pattern && pattern.find('<') != string::npos)) return -1;
	map<string, string>::const_iterator prior = inferred->find(pattern);
	if(prior != inferred->end() && CanonicalSpelling(ResolveAlias(
		prior->second, context)) != CanonicalSpelling(ResolveAlias(actual, context))) return 0;
	(*inferred)[pattern] = actual;
	return 1;
}

int PA18TemplateExpander::MatchReferenceArrayPattern(const string& pattern,
	const string& actual, const set<string>& parameter_names,
	map<string, string>* inferred) const
{
	const size_t reference_array = actual.find("(&)");
	const size_t array_open = actual.rfind('[');
	const bool typed_reference_array = reference_array != string::npos &&
		actual.find('[', reference_array + 3) != string::npos;
	const bool lvalue_array = reference_array == string::npos && array_open != string::npos &&
		!actual.empty() && actual[actual.size() - 1] == ']';
	if((!typed_reference_array && !lvalue_array) || pattern.empty() ||
		pattern[pattern.size() - 1] != '&') return -1;
	string parameter = CanonicalSpelling(pattern.substr(0, pattern.size() - 1));
	while(parameter.compare(0, 6, "const ") == 0) parameter = CanonicalSpelling(parameter.substr(6));
	while(parameter.compare(0, 9, "volatile ") == 0) parameter = CanonicalSpelling(parameter.substr(9));
	while(parameter.size() > 6 && parameter.compare(parameter.size() - 6, 6, " const") == 0)
		parameter = CanonicalSpelling(parameter.substr(0, parameter.size() - 6));
	while(parameter.size() > 9 && parameter.compare(parameter.size() - 9, 9, " volatile") == 0)
		parameter = CanonicalSpelling(parameter.substr(0, parameter.size() - 9));
	if(parameter_names.find(parameter) == parameter_names.end()) return -1;
	string array_type = actual;
	if(typed_reference_array) array_type.erase(reference_array, 3);
	else {
		const size_t reference_before_array = array_type.find("&[");
		if(reference_before_array != string::npos) array_type.erase(reference_before_array, 1);
	}
	map<string, string>::const_iterator prior = inferred->find(parameter);
	if(prior != inferred->end() && CanonicalSpelling(prior->second) !=
		CanonicalSpelling(array_type)) return 0;
	(*inferred)[parameter] = CanonicalSpelling(array_type);
	return 1;
}

bool PA18TemplateExpander::MatchOrderingTypePattern(const string& raw_pattern,
	const string& raw_actual, const set<string>& parameter_names,
	map<string, string>* inferred) const
{
	string pattern = CanonicalSpelling(raw_pattern);
	string actual = CanonicalSpelling(raw_actual);
	string pattern_result, actual_result, pattern_qualifiers, actual_qualifiers;
	vector<string> pattern_parameters, actual_parameters;
	const bool pattern_function = SplitDirectFunctionType(pattern, &pattern_result,
		&pattern_parameters, &pattern_qualifiers);
	const bool actual_function = SplitDirectFunctionType(actual, &actual_result,
		&actual_parameters, &actual_qualifiers);
	if(pattern_function) {
		if(!actual_function || pattern_qualifiers != actual_qualifiers ||
			!MatchOrderingTypePattern(pattern_result, actual_result, parameter_names,
				inferred)) return false;
		return MatchOrderingPatternList(pattern_parameters, actual_parameters,
			parameter_names, inferred);
	}
	if(actual_function && parameter_names.find(pattern) == parameter_names.end()) return false;
	const size_t pattern_array = pattern.rfind('[');
	const size_t actual_array = actual.rfind('[');
	if(pattern_array != string::npos || actual_array != string::npos) {
		if(pattern_array == string::npos || actual_array == string::npos ||
			pattern.empty() || actual.empty() || pattern[pattern.size() - 1] != ']' ||
			actual[actual.size() - 1] != ']') return false;
		if(!MatchOrderingTypePattern(pattern.substr(0, pattern_array),
			actual.substr(0, actual_array), parameter_names, inferred)) return false;
		const string bound = CanonicalSpelling(pattern.substr(pattern_array + 1,
			pattern.size() - pattern_array - 2));
		const string actual_bound = CanonicalSpelling(actual.substr(actual_array + 1,
			actual.size() - actual_array - 2));
		if(bound.empty()) return actual_bound.empty();
		if(parameter_names.find(bound) != parameter_names.end()) {
			map<string, string>::const_iterator prior = inferred->find(bound);
			if(prior != inferred->end() && prior->second != actual_bound) return false;
			(*inferred)[bound] = actual_bound;
			return true;
		}
		return ReplaceIdentifiers(bound, *inferred) == actual_bound;
	}
	const bool pattern_rvalue = pattern.size() > 1 &&
		pattern.compare(pattern.size() - 2, 2, "&&") == 0;
	const bool actual_rvalue = actual.size() > 1 &&
		actual.compare(actual.size() - 2, 2, "&&") == 0;
	const bool pattern_lvalue = !pattern.empty() && pattern[pattern.size() - 1] == '&' &&
		!pattern_rvalue;
	const bool actual_lvalue = !actual.empty() && actual[actual.size() - 1] == '&' &&
		!actual_rvalue;
	if(pattern_rvalue || actual_rvalue) {
		if(!pattern_rvalue || !actual_rvalue) return false;
		pattern.erase(pattern.size() - 2);
		actual.erase(actual.size() - 2);
	} else if(pattern_lvalue || actual_lvalue) {
		if(!pattern_lvalue || !actual_lvalue) return false;
		pattern.erase(pattern.size() - 1);
		actual.erase(actual.size() - 1);
	}
	pattern = CanonicalSpelling(pattern);
	actual = CanonicalSpelling(actual);
	const bool pattern_const = pattern.compare(0, 6, "const ") == 0;
	const bool actual_const = actual.compare(0, 6, "const ") == 0;
	const bool pattern_volatile = pattern.compare(0, 9, "volatile ") == 0;
	const bool actual_volatile = actual.compare(0, 9, "volatile ") == 0;
	if(pattern_const != actual_const || pattern_volatile != actual_volatile) return false;
	if(pattern_const) pattern = CanonicalSpelling(pattern.substr(6));
	if(actual_const) actual = CanonicalSpelling(actual.substr(6));
	if(pattern_volatile) pattern = CanonicalSpelling(pattern.substr(9));
	if(actual_volatile) actual = CanonicalSpelling(actual.substr(9));
	while(pattern.size() > 6 && pattern.compare(pattern.size() - 6, 6, " const") == 0) {
		if(actual.size() <= 6 || actual.compare(actual.size() - 6, 6, " const") != 0) return false;
		pattern = CanonicalSpelling(pattern.substr(0, pattern.size() - 6));
		actual = CanonicalSpelling(actual.substr(0, actual.size() - 6));
	}
	while(pattern.size() > 9 && pattern.compare(pattern.size() - 9, 9, " volatile") == 0) {
		if(actual.size() <= 9 || actual.compare(actual.size() - 9, 9, " volatile") != 0) return false;
		pattern = CanonicalSpelling(pattern.substr(0, pattern.size() - 9));
		actual = CanonicalSpelling(actual.substr(0, actual.size() - 9));
	}
	if(parameter_names.find(pattern) != parameter_names.end()) {
		map<string, string>::const_iterator prior = inferred->find(pattern);
		if(prior != inferred->end() && prior->second != actual) return false;
		(*inferred)[pattern] = actual;
		return true;
	}
	const size_t pattern_open = pattern.find('<');
	if(pattern_open != string::npos) {
		string pattern_arguments;
		size_t pattern_close = string::npos;
		if(!TemplateRange(pattern, pattern_open, &pattern_arguments, &pattern_close)) return false;
		const size_t actual_open = actual.find('<');
		if(actual_open == string::npos) return false;
		string actual_arguments;
		size_t actual_close = string::npos;
		if(!TemplateRange(actual, actual_open, &actual_arguments, &actual_close) ||
			pattern.substr(0, pattern_open) != actual.substr(0, actual_open)) return false;
		return MatchOrderingPatternList(SplitTemplateArguments(pattern_arguments),
			SplitTemplateArguments(actual_arguments), parameter_names, inferred);
	}
	for(set<string>::const_iterator parameter = parameter_names.begin();
		parameter != parameter_names.end(); ++parameter) {
		const size_t position = pattern.find(*parameter);
		if(position == string::npos || (position > 0 &&
			IsIdentifierCharacter(pattern[position - 1])) ||
			(position + parameter->size() < pattern.size() &&
				IsIdentifierCharacter(pattern[position + parameter->size()]))) continue;
		const string prefix = pattern.substr(0, position);
		const string suffix = pattern.substr(position + parameter->size());
		if(actual.size() < prefix.size() + suffix.size() ||
			actual.compare(0, prefix.size(), prefix) != 0 ||
			actual.compare(actual.size() - suffix.size(), suffix.size(), suffix) != 0) continue;
		const string value = actual.substr(prefix.size(), actual.size() -
			prefix.size() - suffix.size());
		map<string, string>::const_iterator prior = inferred->find(*parameter);
		if(prior != inferred->end() && prior->second != value) return false;
		(*inferred)[*parameter] = value;
		return true;
	}
	return pattern == actual;
}

bool PA18TemplateExpander::MatchOrderingPatternList(const vector<string>& patterns,
	const vector<string>& actual, const set<string>& parameter_names,
	map<string, string>* inferred) const
{
	size_t actual_index = 0;
	for(size_t pattern_index = 0; pattern_index < patterns.size(); ++pattern_index) {
		const string pattern = CanonicalSpelling(patterns[pattern_index]);
		if(pattern.size() > 3 && pattern.compare(pattern.size() - 3, 3, "...") == 0 &&
			pattern_index + 1 == patterns.size()) {
			const string pack_name = CanonicalSpelling(pattern.substr(0, pattern.size() - 3));
			if(parameter_names.find(pack_name) == parameter_names.end()) return false;
			string combined;
			while(actual_index < actual.size()) {
				if(!combined.empty()) combined += ",";
				combined += CanonicalSpelling(actual[actual_index++]);
			}
			map<string, string>::const_iterator prior = inferred->find(pack_name);
			if(prior != inferred->end() && prior->second != combined) return false;
			(*inferred)[pack_name] = combined;
			return true;
		}
		if(actual_index >= actual.size() || !MatchOrderingTypePattern(pattern,
			actual[actual_index++], parameter_names, inferred)) return false;
	}
	return actual_index == actual.size();
}

string PA18TemplateExpander::ExpandAliasPattern(string pattern, const string& context,
	set<string>* active) const
{
	pattern = CanonicalSpelling(pattern);
	if(!active) return pattern;
	const size_t open = pattern.find('<');
	if(open == string::npos) return pattern;
	string arguments_text;
	size_t close = string::npos;
	if(!TemplateRange(pattern, open, &arguments_text, &close)) return pattern;
	const string base = CanonicalSpelling(pattern.substr(0, open));
	const TemplateDefinition* definition = FindDefinition(base, context);
	if(!definition || !definition->alias_template || !definition->declaration ||
		definition->declaration->children.empty()) return pattern;
	const string active_key = definition->qualified_name + "<" + arguments_text + ">";
	if(!active->insert(active_key).second) return pattern;
	const vector<string> arguments = SplitTemplateArguments(arguments_text);
	map<string, string> substitutions;
	size_t argument = 0;
	for(size_t parameter = 0; parameter < definition->parameters.size(); ++parameter) {
		const TemplateParameter& item = definition->parameters[parameter];
		if(item.name.empty()) {
			if(item.pack) while(argument < arguments.size()) ++argument;
			else if(argument < arguments.size()) ++argument;
			continue;
		}
		if(item.pack) {
			string combined;
			while(argument < arguments.size()) {
				if(!combined.empty()) combined += ',';
				combined += arguments[argument++];
			}
			// The alias body already contains the pack expansion marker.  A
			// dependent alias argument such as `I...` therefore substitutes the
			// element spelling `I`, not `I...`, or the marker would be duplicated.
			if(combined.size() > 3 &&
				combined.compare(combined.size() - 3, 3, "...") == 0)
				combined.erase(combined.size() - 3);
			if(combined != item.name + "...") substitutions[item.name] = combined;
		} else if(argument < arguments.size())
			substitutions[item.name] = arguments[argument++];
	}
	string target = TypeIdSpelling(definition->declaration->children[0]);
	if(target.empty()) {
		active->erase(active_key);
		return pattern;
	}
	target = ReplaceIdentifiers(target, substitutions);
	const string expanded = ExpandAliasPattern(target, context, active);
	active->erase(active_key);
	return expanded.empty() ? target : expanded;
}

bool PA18TemplateExpander::MatchClassSpecializationPattern(
	const TemplateDefinition& definition, const vector<string>& arguments,
	map<string, string>* inferred, const string& context) const
{
	if(!definition.partial_specialization) return false;
	set<string> parameter_names;
	for(size_t i = 0; i < definition.specialization_parameters.size(); ++i)
		if(!definition.specialization_parameters[i].empty())
			parameter_names.insert(definition.specialization_parameters[i]);
	map<string, string> local;
	vector<pair<string, string> > deferred_decltypes;
	size_t pattern_index = 0;
	size_t argument_index = 0;
	for(; pattern_index < definition.specialization_pattern.size(); ++pattern_index) {
		string pattern = CanonicalSpelling(
			definition.specialization_pattern[pattern_index]);
		set<string> active_aliases;
		pattern = ExpandAliasPattern(pattern, context, &active_aliases);
		// A dependent qualified type is a substitution point.  RewriteText has
		// deliberately permissive fallbacks for source declarations, but those
		// fallbacks must not turn `typename T::missing` into a successful `void`
		// pattern while selecting a class partial specialization.  Check the
		// concrete owner before rewriting the enclosing alias (for example
		// `void_t<typename T::iterator_category>`).
		for(set<string>::const_iterator parameter = parameter_names.begin();
			parameter != parameter_names.end(); ++parameter) {
			if(parameter->empty()) continue;
			const string marker = *parameter + "::";
			for(size_t occurrence = pattern.find(marker); occurrence != string::npos;
				occurrence = pattern.find(marker, occurrence + marker.size())) {
				const size_t owner_end = occurrence + parameter->size();
				if(occurrence > 0 && IsIdentifierCharacter(pattern[occurrence - 1])) continue;
				map<string,string>::const_iterator binding = local.find(*parameter);
				if(binding == local.end() || binding->second.empty()) continue;
				size_t member_begin = owner_end + 2;
				while(member_begin < pattern.size() &&
					isspace(static_cast<unsigned char>(pattern[member_begin]))) ++member_begin;
				if(pattern.compare(member_begin, 8, "template") == 0 &&
					(member_begin + 8 == pattern.size() ||
					 !IsIdentifierCharacter(pattern[member_begin + 8]))) {
					member_begin += 8;
					while(member_begin < pattern.size() &&
						isspace(static_cast<unsigned char>(pattern[member_begin]))) ++member_begin;
				}
				const size_t member_end = member_begin;
				while(member_begin < pattern.size() &&
					IsIdentifierCharacter(pattern[member_begin])) ++member_begin;
				if(member_begin == member_end) continue;
				const string member = pattern.substr(member_end, member_begin - member_end);
				string member_type;
				set<string> active_members;
				if(!FindClassMemberType(binding->second, member, local, context,
					&member_type, &active_members)) return false;
			}
		}
		const bool pack = IsTopLevelPackPattern(pattern);
		if(pack && pattern_index + 1 == definition.specialization_pattern.size()) {
			const string pack_pattern = CanonicalSpelling(pattern.substr(0,
				pattern.size() - 3));
			if(!pack_pattern.empty()) {
				while(argument_index < arguments.size()) {
					const string actual = CanonicalSpelling(arguments[argument_index++]);
					map<string, string> one;
					if(!MatchTypePattern(pack_pattern, actual, parameter_names, &one, context, true)) return false;
					for(map<string, string>::const_iterator binding = one.begin();
						binding != one.end(); ++binding) {
						const bool pack_binding = find(definition.specialization_pack_names.begin(),
							definition.specialization_pack_names.end(), binding->first) !=
							definition.specialization_pack_names.end();
						map<string, string>::iterator prior = local.find(binding->first);
						if(pack_binding) {
							if(prior != local.end() && !prior->second.empty()) prior->second += ",";
							local[binding->first] += binding->second;
						} else {
							if(prior != local.end() && prior->second != binding->second) return false;
							local[binding->first] = binding->second;
						}
					}
				}
				if(argument_index == arguments.size())
					for(size_t pack_name = 0; pack_name < definition.specialization_pack_names.size(); ++pack_name)
						if(local.find(definition.specialization_pack_names[pack_name]) == local.end())
							local[definition.specialization_pack_names[pack_name]] = string();
				break;
			}
		}
		if(argument_index >= arguments.size()) return false;
		string actual = CanonicalSpelling(arguments[argument_index++]);
		const size_t actual_parameter = argument_index - 1;
		bool dependent_actual = false;
		for(size_t actual_word = 0; actual_word < actual.size();) {
			if(!IsIdentifierCharacter(actual[actual_word])) {
				++actual_word;
				continue;
			}
			const size_t actual_begin = actual_word;
			while(actual_word < actual.size() && IsIdentifierCharacter(actual[actual_word]))
				++actual_word;
			const string actual_name = actual.substr(actual_begin,
				actual_word - actual_begin);
			const bool qualified_member_name = actual_begin >= 2 &&
				actual.compare(actual_begin - 2, 2, "::") == 0;
			if(!actual_name.empty() && isdigit(static_cast<unsigned char>(actual_name[0])))
				continue;
			const bool known_actual_name = local.find(actual_name) != local.end() ||
				type_aliases_.find(actual_name) != type_aliases_.end() ||
				class_contexts_.find(actual_name) != class_contexts_.end() ||
				class_declarations_.find(actual_name) != class_declarations_.end() ||
				FindClassDeclaration(actual_name, context) != CPPGMAstNodePtr() ||
				definitions_by_name_.find(actual_name) != definitions_by_name_.end() ||
				constant_values_.find(actual_name) != constant_values_.end() ||
				active_integral_substitutions_.find(actual_name) !=
					active_integral_substitutions_.end() ||
				actual_name == "void" || actual_name == "bool" ||
				actual_name == "char" || actual_name == "short" ||
				actual_name == "int" || actual_name == "long" ||
				actual_name == "signed" || actual_name == "unsigned" ||
				actual_name == "typename" || actual_name == "const" ||
				actual_name == "volatile" || actual_name == "true" ||
				actual_name == "false" || PA19Type(actual_name).integral;
			if(!qualified_member_name && (parameter_names.find(actual_name) !=
				parameter_names.end() || !known_actual_name)) {
				dependent_actual = true;
				break;
			}
		}
		if(!dependent_actual && actual_parameter < definition.parameters.size() &&
			definition.parameters[actual_parameter].type &&
			(actual.find("::") != string::npos || actual.find('<') != string::npos)) {
			const string rewritten_actual = NormalizeTypeArgument(
				const_cast<PA18TemplateExpander*>(this)->RewriteText(
					ReplaceIdentifiersPreservingPackSizes(actual, local), context, local, 0));
			if(!rewritten_actual.empty()) actual = rewritten_actual;
		}
		const size_t pattern_open = pattern.find('<');
		const size_t pattern_scope = pattern.rfind("::");
		const bool dependent_member_pattern = pattern_scope != string::npos &&
			(pattern_open == string::npos || pattern_scope > pattern_open);
		if(pattern.find("typename") != string::npos || dependent_member_pattern) {
			try {
				const string rewritten_pattern = NormalizeTypeArgument(
					const_cast<PA18TemplateExpander*>(this)->RewriteText(
						ReplaceIdentifiersPreservingPackSizes(pattern, local), context, local, 0));
				if(!rewritten_pattern.empty()) pattern = rewritten_pattern;
			} catch(const PA18SubstitutionFailure&) {
				return false;
			}
		}
		// A dependent non-type default can be the result of an unevaluated
		// expression rather than a plain type spelling.  Evaluate it after the
		// earlier class-template parameters have been matched so substitution
		// failure rejects this partial specialization locally.
		if(pattern.compare(0, 9, "decltype(") == 0 && pattern.size() > 10 &&
			pattern[pattern.size() - 1] == ')') {
			bool defer_decltype = false;
			for(set<string>::const_iterator parameter = parameter_names.begin();
				parameter != parameter_names.end(); ++parameter) {
				const size_t occurrence = pattern.find(*parameter);
				if(occurrence != string::npos && local.find(*parameter) == local.end()) {
					defer_decltype = true;
					break;
				}
			}
			if(defer_decltype) {
				deferred_decltypes.push_back(make_pair(pattern, actual));
				continue;
			}
			string evaluated;
			if(!const_cast<PA18TemplateExpander*>(this)->EvaluateDecltypeExpression(
				pattern.substr(9, pattern.size() - 10), context, local, &evaluated)) return false;
			pattern = NormalizeTypeArgument(evaluated);
		}
		bool template_parameter = false;
		for(size_t detail = 0; detail < definition.specialization_parameter_details.size(); ++detail)
			if(definition.specialization_parameter_details[detail].name == pattern &&
				definition.specialization_parameter_details[detail].template_template) {
				template_parameter = true;
				break;
			}
		if(argument_index > 0 && argument_index - 1 < definition.parameters.size() &&
			!definition.parameters[argument_index - 1].type) {
			PA19ConstantExpressionParser parser(constant_values_, local,
				constant_type_sizes_, constant_type_alignments_, type_aliases_);
			PA19IntegralValue normalized_value;
			if(parser.Evaluate(pattern, &normalized_value))
				pattern = TemplateIntegralValueSpelling(normalized_value);
			if(parser.Evaluate(actual, &normalized_value))
				actual = TemplateIntegralValueSpelling(normalized_value);
		}
		if(pattern.size() > 2 && pattern.compare(pattern.size() - 2, 2, "&&") == 0 &&
			(actual.size() < 2 || actual.compare(actual.size() - 2, 2, "&&") != 0)) return false;
		const bool lvalue_reference_pattern = pattern.size() > 0 &&
			pattern[pattern.size() - 1] == '&' &&
			!(pattern.size() > 1 && pattern[pattern.size() - 2] == '&');
		if(lvalue_reference_pattern &&
			(actual.empty() || actual[actual.size() - 1] != '&')) return false;
		if(template_parameter) {
			map<string, string>::const_iterator prior = local.find(pattern);
			if(prior != local.end() && CanonicalSpelling(prior->second) != actual) return false;
			local[pattern] = actual;
		} else {
			const bool matched_pattern = pattern == actual ||
				MatchTypePattern(pattern, actual, parameter_names, &local, context, true);
			if(!matched_pattern) return false;
		}
	}
	while(argument_index < arguments.size()) {
		if(argument_index >= definition.parameters.size() ||
			definition.parameters[argument_index].default_type.empty()) return false;
		const string expected = NormalizeTypeArgument(ReplaceIdentifiers(
			definition.parameters[argument_index].default_type, local));
		if(expected != NormalizeTypeArgument(arguments[argument_index])) return false;
		++argument_index;
	}
	for(size_t deferred = 0; deferred < deferred_decltypes.size(); ++deferred) {
		const string& pattern = deferred_decltypes[deferred].first;
		string evaluated;
		if(!const_cast<PA18TemplateExpander*>(this)->EvaluateDecltypeExpression(
			pattern.substr(9, pattern.size() - 10), context, local, &evaluated)) return false;
		if(NormalizeTypeArgument(evaluated) !=
			NormalizeTypeArgument(deferred_decltypes[deferred].second)) return false;
	}
	// Alias expansion in a partial-specialization pattern is itself subject to
	// substitution.  Class and variable template heads are matched by the
	// ordinary pattern machinery; only probe alias heads here, so a failed
	// operation rejects this specialization without trying to rewrite an
	// unresolved template-template pack.
	for(size_t pattern_index = 0; pattern_index < definition.specialization_pattern.size();
		++pattern_index) {
		const string source_pattern = definition.specialization_pattern[pattern_index];
		const size_t open = source_pattern.find('<');
		if(open == string::npos) continue;
		string base;
		size_t begin = 0;
		if(!TemplateBase(source_pattern, open, &begin, &base)) continue;
		const TemplateDefinition* outer = FindDefinition(base, context);
		if(!outer || !outer->alias_template) continue;
		const string substituted_pattern = ReplaceIdentifiersPreservingPackSizes(
			CanonicalSpelling(source_pattern), local);
		try {
			const_cast<PA18TemplateExpander*>(this)->RewriteText(
				substituted_pattern, context, local, 0);
		} catch(const PA18SubstitutionFailure&) {
			return false;
		}
	}
	if(inferred) *inferred = local;
	return true;
}

} // namespace pa18_templates_internal
