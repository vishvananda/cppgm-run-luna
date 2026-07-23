#pragma once

	bool MatchOrderingTypePattern(const string& raw_pattern, const string& raw_actual, const set<string>& parameter_names, map<string, string>* inferred) const
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
	bool MatchOrderingPatternList(const vector<string>& patterns,
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
	bool ClassPartialMoreSpecialized(const TemplateDefinition& lhs,
		const TemplateDefinition& rhs, const string& context) const;
	string ExpandAliasPattern(string pattern, const string& context,
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
				substitutions[item.name] = combined;
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
	bool MatchClassSpecializationPattern(const TemplateDefinition& definition,
		const vector<string>& arguments, map<string, string>* inferred,
		const string& context) const
	{
		if(!definition.partial_specialization) return false;
		set<string> parameter_names;
		for(size_t i = 0; i < definition.specialization_parameters.size(); ++i)
			if(!definition.specialization_parameters[i].empty())
				parameter_names.insert(definition.specialization_parameters[i]);
		map<string, string> local;
		size_t pattern_index = 0;
		size_t argument_index = 0;
		for(; pattern_index < definition.specialization_pattern.size(); ++pattern_index) {
			string pattern = CanonicalSpelling(
				definition.specialization_pattern[pattern_index]);
			set<string> active_aliases;
			pattern = ExpandAliasPattern(pattern, context, &active_aliases);
			const bool pack = IsTopLevelPackPattern(pattern);
			if(pack && pattern_index + 1 == definition.specialization_pattern.size()) {
				const string pack_pattern = CanonicalSpelling(pattern.substr(0,
					pattern.size() - 3));
				if(!pack_pattern.empty()) {
					while(argument_index < arguments.size()) {
						const string actual = CanonicalSpelling(arguments[argument_index++]);
						map<string, string> one;
						if(!MatchTypePattern(pack_pattern, actual, parameter_names, &one, context, true)) return false;
						for(map<string, string>::const_iterator binding = one.begin(); binding != one.end(); ++binding) {
							const bool pack_binding = find(definition.specialization_pack_names.begin(),
								definition.specialization_pack_names.end(), binding->first) !=
								definition.specialization_pack_names.end();
							map<string, string>::iterator prior = local.find(binding->first);
							if(pack_binding) { if(prior != local.end() && !prior->second.empty()) prior->second += ","; local[binding->first] += binding->second; }
							else { if(prior != local.end() && prior->second != binding->second) return false; local[binding->first] = binding->second; }
						}
					}
					if(argument_index == arguments.size())
						for(size_t pack_name = 0; pack_name < definition.specialization_pack_names.size(); ++pack_name)
							if(local.find(definition.specialization_pack_names[pack_name]) == local.end()) local[definition.specialization_pack_names[pack_name]] = string();
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
				const string rewritten_pattern = NormalizeTypeArgument(const_cast<PA18TemplateExpander*>(this)->RewriteText(
					ReplaceIdentifiersPreservingPackSizes(pattern, local), context, local, 0));
				if(!rewritten_pattern.empty()) pattern = rewritten_pattern;
			}
			bool template_parameter = false;
			for(size_t detail = 0; detail < definition.specialization_parameter_details.size(); ++detail)
				if(definition.specialization_parameter_details[detail].name == pattern &&
					definition.specialization_parameter_details[detail].template_template) { template_parameter = true; break; }
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
		if(inferred) *inferred = local;
		return true;
	}
