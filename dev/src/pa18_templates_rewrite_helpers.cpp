#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

namespace pa18_templates_internal {

string PA18TemplateExpander::ArrayPatternElement(const string& raw) const
{
	string result = CanonicalSpelling(raw);
	if(result.size() >= 3 && result.compare(result.size() - 3, 3, "(&)") == 0)
		result = CanonicalSpelling(result.substr(0, result.size() - 3));
	else if(result.size() >= 5 && result.compare(result.size() - 5, 5, "(& )") == 0)
		result = CanonicalSpelling(result.substr(0, result.size() - 5));
	return result;
}

bool PA18TemplateExpander::FunctionOwnerCompatible(const string& pattern,
	const string& actual, bool class_pattern) const
{
	if(pattern == actual || class_pattern) return pattern == actual;
	return (pattern.find("(&") != string::npos && actual.find("(*") != string::npos) ||
		(actual.find("(&") != string::npos && pattern.find("(*") != string::npos);
}

bool PA18TemplateExpander::PreserveEvaluatedDecltype(
	const CPPGMAstNodePtr& input, const map<string, string>& substitutions,
	const CPPGMAstNodePtr& result) const
{
	if(!input || input->kind != "decltype-specifier" || HasReplayContext(substitutions) ||
		result->value.compare(0, 9, "decltype(") == 0) return false;
	result->kind = "type-name";
	result->value = RemoveMarker(result->value);
	return true;
}

int PA18TemplateExpander::MatchObjectCvPattern(const string& pattern,
	const string& actual, const set<string>& parameter_names,
	map<string, string>* inferred, const string& context, bool class_pattern) const
{
	string function_result, function_qualifiers;
	vector<string> function_parameters;
	if(SplitDirectFunctionType(actual, &function_result, &function_parameters,
		&function_qualifiers)) return -1;
	auto object_cv_and_base = [](const string& spelling, int* mask, string* base) {
		*mask = 0;
		size_t outer_pointer = string::npos;
		int angle_depth = 0, parenthesis_depth = 0, bracket_depth = 0;
		for(size_t position = 0; position < spelling.size(); ++position) {
			const char ch = spelling[position];
			if(ch == '<' && IsTemplateAngleOpen(spelling, position)) ++angle_depth;
			else if(ch == '>' && angle_depth > 0 && IsTemplateAngleClose(spelling, position)) --angle_depth;
			else if(ch == '(') ++parenthesis_depth;
			else if(ch == ')' && parenthesis_depth > 0) --parenthesis_depth;
			else if(ch == '[') ++bracket_depth;
			else if(ch == ']' && bracket_depth > 0) --bracket_depth;
			else if(ch == '*' && angle_depth == 0 && parenthesis_depth == 0 && bracket_depth == 0)
				outer_pointer = position;
		}
		string remaining;
		angle_depth = parenthesis_depth = bracket_depth = 0;
		for(size_t position = 0; position < spelling.size();) {
			const char ch = spelling[position];
			const bool outer_cv = outer_pointer == string::npos || position > outer_pointer;
			if(ch == '<' && IsTemplateAngleOpen(spelling, position)) ++angle_depth;
			else if(ch == '>' && angle_depth > 0 && IsTemplateAngleClose(spelling, position)) --angle_depth;
			else if(ch == '(') ++parenthesis_depth;
			else if(ch == ')' && parenthesis_depth > 0) --parenthesis_depth;
			else if(ch == '[') ++bracket_depth;
			else if(ch == ']' && bracket_depth > 0) --bracket_depth;
			if(outer_cv && angle_depth == 0 && parenthesis_depth == 0 && bracket_depth == 0 &&
				(ch == '_' || isalpha(static_cast<unsigned char>(ch)))) {
				size_t end = position + 1;
				while(end < spelling.size() && IsIdentifierCharacter(spelling[end])) ++end;
				const string word = spelling.substr(position, end - position);
				if(word == "const") *mask |= 1;
				else if(word == "volatile") *mask |= 2;
				else remaining += word;
				position = end;
				continue;
			}
			remaining += ch;
			++position;
		}
		*base = CanonicalSpelling(remaining);
	};
	int pattern_mask = 0, actual_mask = 0;
	string pattern_base, actual_base;
	object_cv_and_base(pattern, &pattern_mask, &pattern_base);
	object_cv_and_base(actual, &actual_mask, &actual_base);
	const bool pattern_lvalue_reference = pattern_base.size() > 0 &&
		pattern_base[pattern_base.size() - 1] == '&' &&
		(pattern_base.size() < 2 || pattern_base[pattern_base.size() - 2] != '&');
	const bool actual_lvalue_reference = actual_base.size() > 0 &&
		actual_base[actual_base.size() - 1] == '&' &&
		(actual_base.size() < 2 || actual_base[actual_base.size() - 2] != '&');
	if(pattern_lvalue_reference || actual_lvalue_reference) {
		if(!pattern_lvalue_reference || !actual_lvalue_reference) return -1;
		pattern_base = CanonicalSpelling(pattern_base.substr(0, pattern_base.size() - 1));
		actual_base = CanonicalSpelling(actual_base.substr(0, actual_base.size() - 1));
	}
	if(!pattern_mask || parameter_names.find(pattern_base) == parameter_names.end() ||
		pattern_base.find('*') != string::npos || pattern_base.find('&') != string::npos) return -1;
	// Top-level cv on a reference pattern qualifies the referred-to object;
	// it does not require the argument spelling to repeat that cv.  In
	// particular, `const T&` binds an ordinary lvalue of type `T` and still
	// deduces `T` from the unqualified object type.
	if((pattern_mask & actual_mask) != pattern_mask &&
		(!pattern_lvalue_reference || class_pattern)) return 0;
	const int remaining_cv = actual_mask & ~pattern_mask;
	string bound = actual_base;
	if(remaining_cv & 2) bound = "volatile " + bound;
	if(remaining_cv & 1) bound = "const " + bound;
	bound = CanonicalSpelling(bound);
	map<string, string>::const_iterator prior = inferred->find(pattern_base);
	if(prior != inferred->end() && CanonicalSpelling(ResolveAlias(prior->second, context)) !=
		CanonicalSpelling(ResolveAlias(bound, context))) return 0;
	(*inferred)[pattern_base] = bound;
	return 1;
}

bool PA18TemplateExpander::MatchTrailingTypePack(const vector<string>& pattern_parts,
	const vector<string>& actual_parts, const set<string>& parameter_names,
	map<string, string>* inferred, const string& context, bool class_pattern) const
{
	const string pack_pattern = CanonicalSpelling(pattern_parts.back().substr(
		0, pattern_parts.back().size() - 3));
	const size_t fixed_parts = pattern_parts.size() - 1;
	if(actual_parts.size() < fixed_parts) return false;
	for(size_t part = 0; part < fixed_parts; ++part)
		if(!MatchTypePattern(pattern_parts[part], actual_parts[part], parameter_names,
			inferred, context, class_pattern)) return false;
	map<string, string> expansion_bindings;
	for(size_t part = fixed_parts; part < actual_parts.size(); ++part) {
		map<string, string> one;
		if(!MatchTypePattern(pack_pattern, CanonicalSpelling(actual_parts[part]),
			parameter_names, &one, context, class_pattern)) return false;
		for(map<string, string>::const_iterator binding = one.begin();
			binding != one.end(); ++binding) {
			const bool pack_binding = binding->first == pack_pattern ||
				template_pack_names_.find(binding->first) != template_pack_names_.end();
			map<string, string>::iterator prior = expansion_bindings.find(binding->first);
			if(pack_binding) {
				if(prior != expansion_bindings.end() && !prior->second.empty())
					prior->second += ",";
				expansion_bindings[binding->first] += binding->second;
			} else if(prior != expansion_bindings.end() &&
				prior->second != binding->second) return false;
			else expansion_bindings[binding->first] = binding->second;
		}
	}
	for(map<string, string>::const_iterator binding = expansion_bindings.begin();
		binding != expansion_bindings.end(); ++binding) {
		map<string, string>::const_iterator prior = inferred->find(binding->first);
		if(prior != inferred->end() && CanonicalSpelling(prior->second) !=
			CanonicalSpelling(binding->second)) return false;
		(*inferred)[binding->first] = binding->second;
	}
	if(actual_parts.size() == fixed_parts) {
		const auto contains_identifier = [](const string& text, const string& name) {
			for(size_t position = text.find(name); position != string::npos;
				position = text.find(name, position + name.size())) {
				if((position == 0 || !IsIdentifierCharacter(text[position - 1])) &&
					(position + name.size() == text.size() ||
						!IsIdentifierCharacter(text[position + name.size()]))) return true;
			}
			return false;
		};
		bool found_pack = false;
		if(parameter_names.find(pack_pattern) != parameter_names.end()) {
			(*inferred)[pack_pattern] = string();
			found_pack = true;
		}
		for(set<string>::const_iterator pack = template_pack_names_.begin();
			pack != template_pack_names_.end(); ++pack)
			if(!pack->empty() && contains_identifier(pack_pattern, *pack)) {
				(*inferred)[*pack] = string();
				found_pack = true;
			}
		if(!found_pack && !pack_pattern.empty()) (*inferred)[pack_pattern] = string();
	}
	return true;
}

int PA18TemplateExpander::MatchGeneratedBaseTypePattern(
	const string& pattern, const string& actual, const string& pattern_base,
	const set<string>& parameter_names, map<string, string>* inferred,
	const string& context, bool class_pattern) const
{
	string actual_class = actual;
	string pointer_suffix;
	while(!actual_class.empty() && (actual_class[actual_class.size() - 1] == '*' ||
		actual_class[actual_class.size() - 1] == '&')) {
		pointer_suffix = actual_class[actual_class.size() - 1] + pointer_suffix;
		actual_class.erase(actual_class.size() - 1);
		actual_class = CanonicalSpelling(actual_class);
	}
	string cv_prefix;
	for(;;) {
		if(actual_class.compare(0, 6, "const ") == 0) {
			cv_prefix += "const ";
			actual_class = CanonicalSpelling(actual_class.substr(6));
		} else if(actual_class.compare(0, 9, "volatile ") == 0) {
			cv_prefix += "volatile ";
			actual_class = CanonicalSpelling(actual_class.substr(9));
		} else break;
	}
	map<string, vector<string> >::const_iterator specialization =
		specialization_arguments_.find(LastComponent(actual_class));
	map<string, string>::const_iterator base =
		specialization_bases_.find(LastComponent(actual_class));
	if(specialization == specialization_arguments_.end() || base == specialization_bases_.end())
		return -1;
	if(parameter_names.find(pattern_base) != parameter_names.end() ||
		LastComponent(base->second) == pattern_base) return 0;
	const TemplateDefinition* definition = FindDefinition(base->second, context);
	if(!definition || !definition->class_template || !definition->declaration) return -1;
	map<string, string> substitutions;
	for(size_t parameter = 0; parameter < definition->parameters.size() &&
		parameter < specialization->second.size(); ++parameter)
		if(!definition->parameters[parameter].name.empty())
			substitutions[definition->parameters[parameter].name] =
				specialization->second[parameter];
	for(size_t child = 0; child < definition->declaration->children.size(); ++child) {
		const CPPGMAstNodePtr clause = definition->declaration->children[child];
		if(!clause || clause->kind != "base-clause") continue;
		for(size_t base_index = 0; base_index < clause->children.size(); ++base_index) {
			const CPPGMAstNodePtr base_name = ChildOfKindLocal(
				clause->children[base_index], "base-name");
			if(!base_name) continue;
			const string concrete_base = cv_prefix + CanonicalSpelling(
				ReplaceIdentifiers(base_name->value, substitutions)) + pointer_suffix;
			if(MatchTypePattern(pattern, concrete_base, parameter_names, inferred,
				context, class_pattern)) return 1;
		}
	}
	return -1;
}

} // namespace pa18_templates_internal
