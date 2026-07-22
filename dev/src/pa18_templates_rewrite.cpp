#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
using namespace std;
namespace pa18_templates_internal {

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
		// Expression wrappers such as `decltype(...)` use the same parenthesis
		// shape as a direct function type, but they are not function types.  Do
		// not route dependent placement-new/default-argument expressions through
		// the function-type matcher.
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
		// A function-pointer spelling has a second declarator parenthesis after
		// the first pair (`R(*)(A)`).  It is handled by the existing pointer
		// matcher, so only accept a direct function type here.
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
		// When the outer specialization pattern is headed by a
		// template-template parameter, the inner parameter list participates in
		// partial ordering too.  A fixed arity head such as `Ptr<A>` is more
		// specialized than the same head with a trailing `An...` pack.  The
		// generic spelling matcher intentionally treats both as viable, so make
		// this distinction before the identifier-renaming comparison below.
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
		// A fixed non-type pattern outranks a corresponding unconstrained
		// parameter.  The ordering matcher below deliberately treats template
		// parameters as metavariables, which otherwise makes `T<0, ...>` and
		// `T<I, ...>` appear equivalent and can select the recursive partial for
		// the zero case.
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

bool PA18TemplateExpander::MatchTypePattern(string pattern, string actual,
	const set<string>& parameter_names, map<string, string>* inferred,
	const string& context, bool class_pattern) const
{
	pattern = NormalizeTypeArgument(pattern);
	actual = NormalizeTypeArgument(ResolveAlias(actual, context));
	// Peel matching pointer depth before the single-level pointer matcher below
	// inspects a nested template-id.  Without this, `F<T>**` leaves one `*` on
	// each spelling and the generated specialization identity cannot be
	// recovered from `specialization_bases_`.
	const auto pointer_depth = [](const string& spelling) {
		size_t depth = 0;
		int angle_depth = 0, parenthesis_depth = 0, bracket_depth = 0;
		for(size_t position = 0; position < spelling.size(); ++position) {
			const char ch = spelling[position];
			if(ch == '<' && IsTemplateAngleOpen(spelling, position)) ++angle_depth;
			else if(ch == '>' && angle_depth > 0 && IsTemplateAngleClose(spelling, position))
				--angle_depth;
			else if(ch == '(') ++parenthesis_depth;
			else if(ch == ')' && parenthesis_depth > 0) --parenthesis_depth;
			else if(ch == '[') ++bracket_depth;
			else if(ch == ']' && bracket_depth > 0) --bracket_depth;
			else if(ch == '*' && angle_depth == 0 && parenthesis_depth == 0 &&
				bracket_depth == 0) ++depth;
		}
		return depth;
	};
	const size_t pattern_pointer_depth = pointer_depth(pattern);
	const size_t actual_pointer_depth = pointer_depth(actual);
	if(pattern_pointer_depth > 1 && actual_pointer_depth > 1) {
		if(pattern_pointer_depth != actual_pointer_depth) return false;
		pattern.erase(pattern.size() - pattern_pointer_depth);
		actual.erase(actual.size() - actual_pointer_depth);
		return MatchTypePattern(pattern, actual, parameter_names, inferred,
			context, class_pattern);
	}
	if(class_pattern && actual_pointer_depth > 1 && pattern_pointer_depth == 1)
		return false;
	const auto separate_compact_cv = [](string spelling) {
		static const char* const qualifiers[] = {"const", "volatile"};
		for(size_t qualifier = 0; qualifier < 2; ++qualifier) {
			const string word = qualifiers[qualifier];
			for(size_t at = spelling.find(word); at != string::npos;
				at = spelling.find(word, at + word.size() + 1)) {
				if(at == 0 || !IsIdentifierCharacter(spelling[at - 1])) continue;
				const size_t end = at + word.size();
				if(end < spelling.size() && IsIdentifierCharacter(spelling[end])) continue;
				size_t next = end;
				while(next < spelling.size() && isspace(static_cast<unsigned char>(spelling[next]))) ++next;
				if(next == spelling.size() || spelling[next] == '&' || spelling[next] == '*') {
					spelling.insert(at, " ");
					at += 1;
				}
			}
		}
		return CanonicalSpelling(spelling);
	};
	pattern = separate_compact_cv(pattern);
	actual = separate_compact_cv(actual);
	pattern = CanonicalSpelling(pattern);
	const int object_cv_match = MatchObjectCvPattern(pattern, actual,
		parameter_names, inferred, context);
	if(object_cv_match >= 0) return object_cv_match != 0;
	// Array types are part of a specialization key, not an expression suffix
	// to be discarded.  Match the element type and the bound independently so
	// `T[N]` can bind both a type and a typed non-type parameter.  This also
	// keeps `T[]` distinct from a bounded array, which is required when a
	// bounded partial specialization is not viable.
	const size_t pattern_array_open = pattern.rfind('[');
	const size_t actual_array_open = actual.rfind('[');
	if(pattern_array_open != string::npos || actual_array_open != string::npos) {
		if(pattern_array_open == string::npos || actual_array_open == string::npos ||
			pattern.empty() || actual.empty() || pattern[pattern.size() - 1] != ']' ||
			actual[actual.size() - 1] != ']') return false;
		const string pattern_element = CanonicalSpelling(pattern.substr(0,
			pattern_array_open));
		const string actual_element = CanonicalSpelling(actual.substr(0,
			actual_array_open));
		if(!MatchTypePattern(pattern_element, actual_element, parameter_names,
			inferred, context, class_pattern)) {
			return false;
		}
		const string pattern_bound = CanonicalSpelling(pattern.substr(
			pattern_array_open + 1, pattern.size() - pattern_array_open - 2));
		const string actual_bound = CanonicalSpelling(actual.substr(
			actual_array_open + 1, actual.size() - actual_array_open - 2));
		if(pattern_bound.empty() || actual_bound.empty()) return pattern_bound.empty() &&
			actual_bound.empty();
		if(parameter_names.find(pattern_bound) != parameter_names.end()) {
			map<string, string>::const_iterator prior = inferred->find(pattern_bound);
			if(prior != inferred->end() && CanonicalSpelling(prior->second) != actual_bound)
				return false;
			(*inferred)[pattern_bound] = actual_bound;
			return true;
		}
		return CanonicalSpelling(ReplaceIdentifiers(pattern_bound, *inferred)) ==
			actual_bound;
	}
	// Direct function types have a different top-level grammar from function
	// pointers.  Keep their parameter list and cv/ref qualifiers intact before
	// the ordinary reference and object-cv normalization below, then bind a
	// trailing type pack as one typed comma-separated substitution.
	string pattern_result, actual_result, pattern_qualifiers, actual_qualifiers;
	vector<string> pattern_parameters, actual_parameters;
	const bool direct_pattern_function = SplitDirectFunctionType(pattern, &pattern_result,
		&pattern_parameters, &pattern_qualifiers);
	const bool direct_actual_function = SplitDirectFunctionType(actual, &actual_result,
		&actual_parameters, &actual_qualifiers);
	bool actual_function_converted = false;
	if(direct_pattern_function) {
		if(!direct_actual_function || pattern_qualifiers != actual_qualifiers ||
			!MatchTypePattern(pattern_result, actual_result, parameter_names, inferred,
				context, class_pattern)) return false;
		if(!pattern_parameters.empty() && pattern_parameters.back().size() > 3 &&
			pattern_parameters.back().compare(pattern_parameters.back().size() - 3, 3, "...") == 0) {
			const size_t fixed = pattern_parameters.size() - 1;
			if(actual_parameters.size() < fixed) return false;
			for(size_t parameter = 0; parameter < fixed; ++parameter)
				if(!MatchTypePattern(pattern_parameters[parameter], actual_parameters[parameter],
					parameter_names, inferred, context, class_pattern)) return false;
			const string pack_pattern = CanonicalSpelling(pattern_parameters.back().substr(
				0, pattern_parameters.back().size() - 3));
			if(parameter_names.find(pack_pattern) == parameter_names.end()) return false;
			string combined;
			for(size_t parameter = fixed; parameter < actual_parameters.size(); ++parameter) {
				map<string, string> one;
				if(!MatchTypePattern(pack_pattern, actual_parameters[parameter], parameter_names,
					&one, context, class_pattern)) return false;
				map<string, string>::const_iterator value = one.find(pack_pattern);
				if(value == one.end()) return false;
				if(!combined.empty()) combined += ",";
				combined += CanonicalSpelling(value->second);
			}
			map<string, string>::const_iterator prior = inferred->find(pack_pattern);
			if(prior != inferred->end() && CanonicalSpelling(prior->second) !=
				CanonicalSpelling(combined)) return false;
			(*inferred)[pack_pattern] = combined;
			return true;
		}
		if(pattern_parameters.size() != actual_parameters.size()) return false;
		for(size_t parameter = 0; parameter < pattern_parameters.size(); ++parameter)
			if(!MatchTypePattern(pattern_parameters[parameter], actual_parameters[parameter],
				parameter_names, inferred, context, class_pattern)) return false;
		return true;
	}
	// A function type used as a nested type argument can be supplied to a
	// pointer-shaped partial specialization after the language's function-to-
	// pointer adjustment.  Preserve the typed result and parameter list while
	// presenting the spelling expected by the existing pointer matcher.
	if(direct_actual_function && !direct_pattern_function &&
		pattern.find(")(") != string::npos && actual_qualifiers.empty()) {
		string converted = actual_result + "(*)(";
		for(size_t parameter = 0; parameter < actual_parameters.size(); ++parameter) {
			if(parameter) converted += ',';
			converted += actual_parameters[parameter];
		}
		converted += ')';
		actual = CanonicalSpelling(converted);
		actual_function_converted = true;
	}
	if(direct_actual_function && parameter_names.find(pattern) != parameter_names.end()) {
		map<string, string>::const_iterator prior = inferred->find(pattern);
		if(prior != inferred->end() && CanonicalSpelling(ResolveAlias(prior->second, context)) !=
			CanonicalSpelling(ResolveAlias(actual, context))) return false;
		(*inferred)[pattern] = actual;
		return true;
	}
	if(direct_actual_function && !actual_function_converted) return false;
	const auto trailing_cv_kind = [](const string& spelling) {
		if(spelling.size() >= 9 && spelling.compare(spelling.size() - 8, 8,
			"volatile") == 0 && spelling[spelling.size() - 9] == '*') return 2;
		if(spelling.size() >= 6 && spelling.compare(spelling.size() - 5, 5,
			"const") == 0 && spelling[spelling.size() - 6] == '*') return 1;
		if(spelling.size() >= 9 && spelling.compare(spelling.size() - 9, 9,
			" volatile") == 0) return 2;
		if(spelling.size() >= 6 && spelling.compare(spelling.size() - 6, 6,
			" const") == 0) return 1;
		return 0;
	};
	const int pattern_trailing_cv = trailing_cv_kind(pattern);
	const int actual_trailing_cv = trailing_cv_kind(actual);
	const auto has_top_level_pointer = [](const string& spelling) {
		int angle_depth = 0;
		int parenthesis_depth = 0;
		for(size_t i = 0; i < spelling.size(); ++i) {
			if(spelling[i] == '<' && IsTemplateAngleOpen(spelling, i)) ++angle_depth;
			else if(spelling[i] == '>' && angle_depth > 0 &&
				IsTemplateAngleClose(spelling, i)) --angle_depth;
			else if(spelling[i] == '(') ++parenthesis_depth;
			else if(spelling[i] == ')' && parenthesis_depth > 0) --parenthesis_depth;
			else if(spelling[i] == '*' && angle_depth == 0 && parenthesis_depth == 0)
				return true;
		}
		return false;
	};
	const bool pattern_has_pointer = has_top_level_pointer(pattern);
	const bool actual_has_pointer = has_top_level_pointer(actual);
	const bool reference_pattern = !pattern.empty() && pattern[pattern.size() - 1] == '&';
	const int pattern_object_cv = !pattern_has_pointer ?
		(pattern.compare(0, 6, "const ") == 0 ? 1 :
		 (pattern.compare(0, 9, "volatile ") == 0 ? 2 : pattern_trailing_cv)) : 0;
	const int actual_object_cv = !actual_has_pointer ?
		(actual.compare(0, 6, "const ") == 0 ? 1 :
		 (actual.compare(0, 9, "volatile ") == 0 ? 2 : actual_trailing_cv)) : 0;
	if(pattern_trailing_cv && pattern_has_pointer) {
		pattern.erase(pattern.size() - (pattern_trailing_cv == 1 ? 5 : 8));
		pattern = CanonicalSpelling(pattern);
	}
	if(actual_trailing_cv && actual_has_pointer) {
		actual.erase(actual.size() - (actual_trailing_cv == 1 ? 5 : 8));
		actual = CanonicalSpelling(actual);
	}
	const int pattern_effective_cv = pattern_has_pointer ? pattern_trailing_cv :
		pattern_object_cv;
	const int actual_effective_cv = actual_has_pointer ? actual_trailing_cv :
		actual_object_cv;
	const bool direct_parameter = parameter_names.find(pattern) != parameter_names.end() &&
		pattern.find('<') == string::npos;
	const bool bare_reference_parameter = reference_pattern && pattern.size() > 1 &&
		parameter_names.find(pattern.substr(0, pattern.size() - 1)) != parameter_names.end();
	// A cv qualifier on a reference parameter is part of the binding rule, not
	// a requirement that the argument spelling carry the same top-level cv.
	// Keep the stricter comparison for pointer pointee patterns, where
	// `const T*` and `T*` are distinct specialization keys.
	if((class_pattern || pattern_has_pointer || actual_has_pointer) &&
		pattern_effective_cv && pattern_effective_cv != actual_effective_cv &&
		!reference_pattern) return false;
	if((class_pattern || pattern_has_pointer || actual_has_pointer) &&
		actual_effective_cv && !pattern_effective_cv && !direct_parameter &&
		!reference_pattern) return false;
	if(pattern_has_pointer && pattern_trailing_cv &&
		pattern_trailing_cv != actual_trailing_cv) return false;
	const bool pattern_pointer = !pattern.empty() && pattern[pattern.size() - 1] == '*';
	const bool actual_pointer = !actual.empty() && actual[actual.size() - 1] == '*';
	const bool pattern_cv_qualified =
		pattern.compare(0, 6, "const ") == 0 ||
		pattern.compare(0, 9, "volatile ") == 0;
	const int pattern_cv_kind = pattern.compare(0, 6, "const ") == 0 ? 1 :
		(pattern.compare(0, 9, "volatile ") == 0 ? 2 : 0);
	const int actual_cv_kind = actual.compare(0, 6, "const ") == 0 ? 1 :
		(actual.compare(0, 9, "volatile ") == 0 ? 2 : 0);
	// A cv-qualified pattern constrains the corresponding pointee/object
	// type.  `const T*` may deduce T from `const int*`, but it must not also
	// claim `int*`; the unqualified `T*` candidate is the viable one there.
	if(pattern_has_pointer && actual_has_pointer && pattern_cv_qualified &&
		pattern_cv_kind != actual_cv_kind) return false;
		string cv_parameter = pattern;
		if(pattern_trailing_cv && !pattern_has_pointer)
			cv_parameter.erase(cv_parameter.size() -
				(pattern_trailing_cv == 1 ? 6 : 9));
		cv_parameter = CanonicalSpelling(cv_parameter);
		// For T*, cv-qualification before the pointed-to type belongs to T.
		// It must survive deduction; stripping it here previously made a
		// candidate deduced from T* and const T& appear consistent when it was
		// not.  A pattern that explicitly spells const T* still consumes that
		// qualification as part of the pattern.
		const bool preserve_pointee_cv = actual_pointer && !pattern_cv_qualified &&
			(pattern_pointer || (pattern_trailing_cv &&
				parameter_names.find(cv_parameter) != parameter_names.end()));
		while(pattern.compare(0, 6, "const ") == 0) pattern = CanonicalSpelling(pattern.substr(6));
		while(pattern.compare(0, 9, "volatile ") == 0) pattern = CanonicalSpelling(pattern.substr(9));
			if(!preserve_pointee_cv && !direct_parameter && !bare_reference_parameter) {
			while(actual.compare(0, 6, "const ") == 0) actual = CanonicalSpelling(actual.substr(6));
			while(actual.compare(0, 9, "volatile ") == 0) actual = CanonicalSpelling(actual.substr(9));
		}
		for(;;) {
			bool removed = false;
			if(pattern.size() > 6 && pattern.compare(pattern.size() - 6, 6, " const") == 0) {
				pattern = CanonicalSpelling(pattern.substr(0, pattern.size() - 6));
				removed = true;
			} else if(pattern.size() > 9 &&
				pattern.compare(pattern.size() - 9, 9, " volatile") == 0) {
				pattern = CanonicalSpelling(pattern.substr(0, pattern.size() - 9));
				removed = true;
			}
			if(!removed) break;
		}
		if(!direct_parameter && !bare_reference_parameter) for(;;) {
			bool removed = false;
			if(actual.size() > 6 && actual.compare(actual.size() - 6, 6, " const") == 0) {
				actual = CanonicalSpelling(actual.substr(0, actual.size() - 6));
				removed = true;
			} else if(actual.size() > 9 &&
				actual.compare(actual.size() - 9, 9, " volatile") == 0) {
				actual = CanonicalSpelling(actual.substr(0, actual.size() - 9));
				removed = true;
			}
			if(!removed) break;
		}
		// A forwarding-reference pattern deduces the underlying parameter as
		// an lvalue reference when the argument is an lvalue.  Handle the
		// two-character `&&` suffix before ordinary reference stripping;
		// otherwise `Args&&` becomes `Args&` and is no longer recognized as the
		// template parameter name.
		if(pattern.size() > 2 && pattern.compare(pattern.size() - 2, 2, "&&") == 0) {
			const string base = CanonicalSpelling(pattern.substr(0, pattern.size() - 2));
			if(parameter_names.find(base) != parameter_names.end()) {
				(*inferred)[base] = actual;
				return true;
			}
		}
		if(!pattern.empty() && pattern[pattern.size() - 1] == '&') {
			pattern.erase(pattern.size() - 1);
			if(!actual.empty() && actual[actual.size() - 1] == '&') actual.erase(actual.size() - 1);
		}
		if(!pattern.empty() && pattern[pattern.size() - 1] == '*') {
			if(actual.empty() || actual[actual.size() - 1] != '*') return false;
			pattern.erase(pattern.size() - 1);
			actual.erase(actual.size() - 1);
		}
		pattern = CanonicalSpelling(pattern);
		actual = CanonicalSpelling(actual);
		while(pattern.size() > 6 && pattern.compare(pattern.size() - 6, 6, " const") == 0)
			pattern = CanonicalSpelling(pattern.substr(0, pattern.size() - 6));
		while(pattern.size() > 9 && pattern.compare(pattern.size() - 9, 9, " volatile") == 0)
			pattern = CanonicalSpelling(pattern.substr(0, pattern.size() - 9));
		if(!direct_parameter && !bare_reference_parameter) {
			while(actual.size() > 6 && actual.compare(actual.size() - 6, 6, " const") == 0)
				actual = CanonicalSpelling(actual.substr(0, actual.size() - 6));
			while(actual.size() > 9 && actual.compare(actual.size() - 9, 9, " volatile") == 0)
				actual = CanonicalSpelling(actual.substr(0, actual.size() - 9));
		}
		// Function-pointer parameters carry their nested declarator in the
		// spelling (`R(*)(A...)`).  Match the return type and each parameter
		// recursively so a dependent parameter such as `T` can be deduced from
		// the function pointer supplied at the call site.
		const size_t pattern_function = pattern.find(")(");
		const size_t actual_function = actual.find(")(");
		if(pattern_function != string::npos && actual_function != string::npos &&
			pattern[pattern.size() - 1] == ')' && actual[actual.size() - 1] == ')') {
			const size_t pattern_pointer = pattern.rfind("(*", pattern_function);
			const size_t actual_pointer = actual.rfind("(*", actual_function);
			const size_t pattern_reference = pattern.rfind("(&", pattern_function);
			const size_t actual_reference = actual.rfind("(&", actual_function);
			const size_t pattern_owner = pattern_pointer != string::npos ? pattern_pointer : pattern_reference;
			const size_t actual_owner = actual_pointer != string::npos ? actual_pointer : actual_reference;
			if(pattern_owner == string::npos || actual_owner == string::npos ||
				pattern.substr(pattern_owner, pattern_function - pattern_owner + 1) !=
				actual.substr(actual_owner, actual_function - actual_owner + 1)) return false;
			const string pattern_result = pattern.substr(0, pattern_owner);
			const string actual_result = actual.substr(0, actual_owner);
			const vector<string> pattern_parameters = SplitTemplateArguments(pattern.substr(
				pattern_function + 2, pattern.size() - pattern_function - 3));
			const vector<string> actual_parameters = SplitTemplateArguments(actual.substr(
				actual_function + 2, actual.size() - actual_function - 3));
			if(pattern_parameters.size() != actual_parameters.size() ||
				!MatchTypePattern(pattern_result, actual_result, parameter_names, inferred,
					context, class_pattern)) return false;
			for(size_t parameter = 0; parameter < pattern_parameters.size(); ++parameter)
				if(!MatchTypePattern(pattern_parameters[parameter], actual_parameters[parameter],
					parameter_names, inferred, context, class_pattern)) return false;
			return true;
		}
		if(parameter_names.find(pattern) != parameter_names.end() &&
			pattern.find('<') == string::npos) {
			map<string, string>::const_iterator prior = inferred->find(pattern);
			if(prior != inferred->end() &&
				CanonicalSpelling(ResolveAlias(prior->second, context)) !=
				CanonicalSpelling(ResolveAlias(actual, context))) return false;
			(*inferred)[pattern] = actual;
			return true;
		}
		const size_t pattern_open = pattern.find('<');
		if(pattern_open != string::npos) {
			string pattern_arguments;
			size_t pattern_close = string::npos;
			if(!TemplateRange(pattern, pattern_open, &pattern_arguments, &pattern_close)) return false;
			const size_t actual_open = actual.find('<');
			const vector<string> pattern_parts = SplitTemplateArguments(pattern_arguments);
			vector<string> actual_parts;
			if(actual_open == string::npos) {
				map<string, vector<string> >::const_iterator specialization =
					specialization_arguments_.find(LastComponent(actual));
				map<string, string>::const_iterator base = specialization_bases_.find(LastComponent(actual));
				const string pattern_base = LastComponent(pattern.substr(0, pattern_open));
				if(specialization == specialization_arguments_.end() || base == specialization_bases_.end() ||
					(parameter_names.find(pattern_base) == parameter_names.end() &&
					LastComponent(base->second) != pattern_base)) return false;
				if(parameter_names.find(pattern_base) != parameter_names.end())
					(*inferred)[pattern_base] = base->second;
				if(pattern_parts.empty()) {
					if(specialization->second.empty()) return true;
					const TemplateDefinition* actual_definition = FindDefinition(
						base->second, context);
					if(!actual_definition || actual_definition->parameters.size() !=
						specialization->second.size()) return false;
					map<string, string> default_bindings;
					for(size_t parameter = 0; parameter < actual_definition->parameters.size(); ++parameter) {
						const TemplateParameter& actual_parameter =
							actual_definition->parameters[parameter];
						if(actual_parameter.default_type.empty()) return false;
						const string expected = NormalizeTypeArgument(ReplaceIdentifiers(
							actual_parameter.default_type, default_bindings));
						const string concrete = NormalizeTypeArgument(
							specialization->second[parameter]);
						if(expected != concrete) return false;
						if(!actual_parameter.name.empty()) default_bindings[actual_parameter.name] = concrete;
					}
					return true;
				}
				actual_parts = specialization->second;
			} else {
				string actual_arguments;
				size_t actual_close = string::npos;
				if(!TemplateRange(actual, actual_open, &actual_arguments, &actual_close)) return false;
				const string pattern_base = LastComponent(pattern.substr(0, pattern_open));
				const string actual_base = LastComponent(actual.substr(0, actual_open));
				if(parameter_names.find(pattern_base) != parameter_names.end()) {
					(*inferred)[pattern_base] = CanonicalSpelling(actual.substr(0, actual_open));
				} else if(pattern_base != actual_base) {
					// A pointer to a derived class can bind to a dependent base
					// pointer.  Recover the concrete base spelling from the actual
					// class template before matching its arguments (for example,
					// vector<int>* against store<N,U>*).
					const TemplateDefinition* actual_definition = FindDefinition(
						actual.substr(0, actual_open), context);
					if(actual_definition && actual_definition->class_template &&
						actual_definition->declaration) {
						const vector<string> concrete_parts =
							SplitTemplateArguments(actual_arguments);
						map<string, string> class_substitutions;
						for(size_t parameter = 0; parameter < actual_definition->parameters.size() &&
							parameter < concrete_parts.size(); ++parameter)
							class_substitutions[actual_definition->parameters[parameter].name] =
								concrete_parts[parameter];
						for(size_t child = 0; child < actual_definition->declaration->children.size(); ++child) {
							const CPPGMAstNodePtr clause = actual_definition->declaration->children[child];
							if(!clause || clause->kind != "base-clause") continue;
							for(size_t base_index = 0; base_index < clause->children.size(); ++base_index) {
								const CPPGMAstNodePtr specifier = clause->children[base_index];
								const CPPGMAstNodePtr base_name = ChildOfKindLocal(specifier, "base-name");
								if(!base_name) continue;
								const string concrete_base = CanonicalSpelling(ReplaceIdentifiers(
									base_name->value, class_substitutions));
								if(MatchTypePattern(pattern, concrete_base, parameter_names,
									inferred, context, class_pattern)) return true;
							}
						}
					}
					return false;
				}
				actual_parts = SplitTemplateArguments(actual_arguments);
			}
			if(!pattern_parts.empty() && pattern_parts.back().size() > 3 &&
				pattern_parts.back().compare(pattern_parts.back().size() - 3, 3, "...") == 0) {
				const string pack_pattern = CanonicalSpelling(pattern_parts.back().substr(
					0, pattern_parts.back().size() - 3));
				if(parameter_names.find(pack_pattern) != parameter_names.end() ||
					pack_pattern.find('<') != string::npos) {
					const bool matched_pack = MatchTrailingTypePack(pattern_parts, actual_parts,
						parameter_names, inferred, context, class_pattern);
					return matched_pack;
				}
			}
			if(pattern_parts.size() != actual_parts.size()) return false;
			for(size_t i = 0; i < pattern_parts.size(); ++i)
				if(!MatchTypePattern(pattern_parts[i], actual_parts[i], parameter_names,
					inferred, context, class_pattern))
					return false;
			return true;
		}
		return pattern == actual;
	}
bool PA18TemplateExpander::InferArgument(const CPPGMAstNodePtr& expression,
	string* result, const map<string, string>& substitutions,
	const string& context) const
{
		if(!expression || !result) return false;
		if(expression->kind == "literal") {
			const string value = expression->value;
			if(value.find('"') != string::npos) *result = "const char*";
			else if(value.find('\'') != string::npos) *result = "char";
			else *result = InferLiteralArgumentType(value);
			return true;
		}
		if(expression->kind == "keyword-literal") {
			*result = "bool";
			return true;
		}
		if(expression->kind == "member-expression" && expression->children.size() >= 2) {
			string object_type;
			if(expression->children[0] && expression->children[0]->kind == "keyword-literal" &&
				RemoveMarker(expression->children[0]->value) == "this") object_type = context;
			else InferArgument(expression->children[0], &object_type, substitutions, context);
			const string member = expression->children[1] ?
				LastComponent(expression->children[1]->value) : string();
			set<string> active;
			if(!object_type.empty() && !member.empty() && FindClassMemberType(
				object_type, member, substitutions, context, result, &active)) return true;
		}
		if(expression->kind == "cast-expression" && !expression->children.empty()) {
			const CPPGMAstNodePtr type_id = expression->children[0];
			if(type_id && type_id->kind == "type-id") {
				*result = NormalizeTypeArgument(TypeIdSpelling(type_id));
				return !result->empty();
			}
		}
		if(expression->kind == "id-expression") {
			// The same source identifier can be reused by a later local
			// declaration.  Prefer the parameter belonging to the current class
			// constructor over the collector's translation-unit fallback map.
			const CPPGMAstNodePtr current_class = FindClassDeclaration(context, context);
			if(current_class) for(size_t member = 0; member < current_class->children.size(); ++member) {
				const CPPGMAstNodePtr declaration = current_class->children[member];
				if(!declaration || declaration->kind != "special-member-definition") continue;
				const CPPGMAstNodePtr clause = DescendantOfKind(declaration, "parameter-clause");
				if(!clause) continue;
				for(size_t parameter = 0; parameter < clause->children.size(); ++parameter) {
					const CPPGMAstNodePtr candidate = clause->children[parameter];
					if(!candidate || candidate->kind != "parameter-declaration" ||
						candidate->children.size() < 2 ||
						FirstIdentifierLocal(candidate->children[1]) != expression->value) continue;
					*result = CanonicalSpelling(ReplaceIdentifiers(
						ParameterTypeSpelling(candidate), substitutions));
					return !result->empty();
				}
			}
			for(map<string, vector<string> >::const_iterator pack =
				active_pack_identifier_substitutions_.begin();
				pack != active_pack_identifier_substitutions_.end(); ++pack) {
				if(find(pack->second.begin(), pack->second.end(), expression->value) ==
					pack->second.end()) continue;
				map<string, string>::const_iterator source = variable_types_.find(pack->first);
				if(source == variable_types_.end()) continue;
				*result = ReplaceIdentifiers(ResolveAlias(source->second, context), substitutions);
				if(!result->empty()) return true;
			}
			map<string, string>::const_iterator found = variable_types_.find(LastComponent(expression->value));
			if(found != variable_types_.end()) {
				*result = ReplaceIdentifiers(ResolveAlias(found->second, context), substitutions);
				return true;
			}
			const string marker = FunctionMarker(expression->value, context);
			if(!marker.empty()) {
				*result = marker;
				return true;
			}
		}
		if(expression->kind == "call-expression" && !expression->children.empty() && expression->children[0] && expression->children[0]->kind == "member-expression") {
			const CPPGMAstNodePtr callee = expression->children[0];
			string object_type;
			if(callee->children.size() >= 2) {
				if(callee->children[0] && callee->children[0]->kind == "keyword-literal" &&
					RemoveMarker(callee->children[0]->value) == "this") object_type = context;
				else InferArgument(callee->children[0], &object_type, substitutions, context);
			}
			const string member = callee->children.size() > 1 && callee->children[1] ?
				LastComponent(callee->children[1]->value) : string();
			set<string> active;
			if(!object_type.empty() && !member.empty() && FindClassMemberType(
				object_type, member, substitutions, context, result, &active)) return true;
		}
		if(expression->kind == "call-expression" && !expression->children.empty() && expression->children[0] && expression->children[0]->kind == "id-expression") {
			if(!expression->template_primary.empty() && !expression->template_arguments.empty()) {
				const vector<const TemplateDefinition*> materialized =
					FindFunctionDefinitions(expression->template_primary, context);
				for(size_t candidate = 0; candidate < materialized.size(); ++candidate) {
					const TemplateDefinition* definition = materialized[candidate];
					if(!definition || definition->parameters.size() !=
						expression->template_arguments.size() || !definition->declaration ||
						definition->declaration->children.empty()) continue;
					map<string, string> function_substitutions;
					for(size_t parameter = 0; parameter < definition->parameters.size(); ++parameter)
						if(!definition->parameters[parameter].name.empty())
							function_substitutions[definition->parameters[parameter].name] =
								expression->template_arguments[parameter];
					const CPPGMAstNodePtr declarator = FunctionDeclarator(definition->declaration);
					string return_type = NodeTypeSpelling(definition->declaration->children[0]) +
						DeclaratorSuffix(declarator);
					*result = CollapseReferenceSpelling(ReplaceIdentifiers(
						return_type, function_substitutions));
					if(!result->empty()) return true;
				}
			}
			const string member = LastComponent(expression->children[0]->value);
			string owner; for(string current = context; ; ) {
				if(class_contexts_.find(current) != class_contexts_.end()) { owner = current; break; }
				if(current.empty()) break;
				const size_t separator = current.rfind("::");
				if(separator == string::npos) current.clear(); else current.erase(separator);
			}
			set<string> active;
			if(!owner.empty() && !member.empty() && FindClassMemberType(
				owner, member, substitutions, context, result, &active)) return true;
		}
		if(expression->kind == "call-expression" && !expression->children.empty() &&
			expression->children[0] && expression->children[0]->kind == "id-expression") {
			const string callee = LastComponent(expression->children[0]->value);
			string nested_class;
			const string qualified_callee = expression->children[0]->value;
			if(class_contexts_.find(qualified_callee) != class_contexts_.end() ||
				class_declarations_.find(qualified_callee) != class_declarations_.end())
				nested_class = qualified_callee;
			for(string current = context; nested_class.empty(); ) {
				const string candidate = JoinPath(current, callee);
				if(class_contexts_.find(candidate) != class_contexts_.end() || class_declarations_.find(candidate) != class_declarations_.end()) { nested_class = candidate; break; }
				if(current.empty()) break;
				const size_t separator = current.rfind("::");
				if(separator == string::npos) current.clear(); else current.erase(separator);
			}
			if(!nested_class.empty()) {
				*result = nested_class;
				return true;
			}
			const FunctionSignature* signature = FindFunctionSignature(
				expression->children[0]->value, context);
			if(signature && signature->result_specifiers) {
				*result = NodeTypeSpelling(signature->result_specifiers);
				if(!result->empty()) return true;
			}
			// A type functional-cast such as T() or a local typedef cast is
			// represented as a call-expression by the parser.  Preserve the
			// original spelling fallback after class-constructor and ordinary
			// function-result lookup so deduction can resolve the type alias.
			*result = ResolveAlias(expression->children[0]->value, context);
			return !result->empty();
		}
		if(expression->kind == "binary-expression" &&
			InferBinaryArgument(expression, result, substitutions, context)) return true;
		if(expression->kind == "unary-expression" && !expression->children.empty()) {
			const string op = RemoveMarker(expression->value);
			if(op == "&" && InferArgument(expression->children[0], result, substitutions, context)) {
				*result = CanonicalSpelling(*result + "*");
				return true;
			}
			if(op == "*" && InferArgument(expression->children[0], result, substitutions, context)) {
				if(!result->empty() && result->at(result->size() - 1) == '*')
					result->erase(result->size() - 1);
				*result = CanonicalSpelling(*result + "&");
				return true;
			}
		}
		return false;
	}
bool PA18TemplateExpander::InferFunctionArguments(const TemplateDefinition& definition,
	const CPPGMAstNodePtr& call, vector<string>* result,
	const map<string, string>& substitutions, const string& context,
	const vector<string>* explicit_prefix,
	map<string, vector<string> >* inferred_pack_values) const
{
		if(!call || call->children.size() < 2 || !result) return false;
		const CPPGMAstNodePtr declarator = FunctionDeclarator(definition.declaration);
		const CPPGMAstNodePtr parameters = DescendantOfKind(declarator, "parameter-clause");
		const CPPGMAstNodePtr arguments = call->children[1] &&
			call->children[1]->kind == "argument-list" ? call->children[1] :
			ChildOfKindLocal(call->children[1], "argument-list");
		if(!parameters || !arguments) return false;
		bool has_pack = false;
		size_t required_parameters = 0;
		for(size_t i = 0; i < parameters->children.size(); ++i) {
			const CPPGMAstNodePtr parameter = parameters->children[i];
			if(!parameter || parameter->kind != "parameter-declaration") continue;
			if(IsFunctionParameterPack(parameter)) has_pack = true;
			else if(!ChildOfKindLocal(parameter, "default-argument")) ++required_parameters;
		}
		if(arguments->children.size() < required_parameters ||
			(!has_pack && arguments->children.size() > parameters->children.size())) return false;
		map<string, string> inferred;
		map<string, vector<string> > inferred_packs;
		set<string> parameter_names;
		vector<string> deferred_function_patterns;
		vector<CPPGMAstNodePtr> deferred_function_arguments;
		for(size_t i = 0; i < definition.parameters.size(); ++i) {
			if(!definition.parameters[i].name.empty()) parameter_names.insert(
				definition.parameters[i].name);
			if(explicit_prefix && i < explicit_prefix->size() &&
				!definition.parameters[i].pack && !definition.parameters[i].name.empty())
				inferred[definition.parameters[i].name] = (*explicit_prefix)[i];
		}
		size_t argument_index = 0;
		for(size_t i = 0; i < parameters->children.size(); ++i) {
			const CPPGMAstNodePtr parameter = parameters->children[i];
			if(!parameter || parameter->kind != "parameter-declaration") continue;
			const string pattern = parameter->children.size() > 1 && parameter->children[1] &&
				ChildOfKindLocal(parameter->children[1], "nested-declarator") &&
				ChildOfKindLocal(parameter->children[1], "parameter-clause") ?
				FunctionTypeSpelling(parameter) : ParameterTypeSpelling(parameter);
			const bool pack_parameter = IsFunctionParameterPack(parameter);
			size_t trailing_fixed = 0;
			if(pack_parameter) for(size_t later = i + 1; later < parameters->children.size(); ++later)
				if(parameters->children[later] && parameters->children[later]->kind ==
					"parameter-declaration" && !IsFunctionParameterPack(parameters->children[later]))
					++trailing_fixed;
			const size_t pack_count = pack_parameter && arguments->children.size() >= argument_index + trailing_fixed ?
				arguments->children.size() - argument_index - trailing_fixed : 0;
			const size_t visits = pack_parameter ? pack_count :
				(argument_index < arguments->children.size() ? 1 : 0);
			for(size_t visit = 0; visit < visits; ++visit) {
				string type;
				const CPPGMAstNodePtr argument_expression = arguments->children[argument_index];
				bool inferred_argument = InferArgument(
					argument_expression, &type, substitutions, context);
				const vector<string> function_types = pattern.find(")(") != string::npos ?
					FunctionExpressionTypes(argument_expression, context) : vector<string>();
				if(!function_types.empty()) {
					// An overloaded function name has no single expression type.  Defer
					// its deduction until the other call arguments establish the shared
					// template parameter (for example apply(square, 4)).
					if(function_types.size() > 1 && argument_expression &&
						(argument_expression->kind == "id-expression" ||
						 (argument_expression->kind == "unary-expression" &&
						  RemoveMarker(argument_expression->value) == "&"))) {
						deferred_function_patterns.push_back(pattern);
						deferred_function_arguments.push_back(argument_expression);
						inferred_argument = false;
					} else {
						type = function_types[0];
						inferred_argument = true;
					}
				}
				if(inferred_argument && pack_parameter && pattern.size() > 2 &&
					pattern.compare(pattern.size() - 2, 2, "&&") == 0 &&
					argument_expression &&
					(argument_expression->kind == "id-expression" ||
					 argument_expression->kind == "member-expression" ||
					 argument_expression->kind == "subscript-expression"))
					type = CanonicalSpelling(type + "&");
				if(inferred_argument) {
					map<string, string> one;
					string match_pattern = pattern;
					if(!inferred.empty() && pattern.find('<') != string::npos &&
						pattern.find("::") != string::npos) {
						map<string, string> pattern_substitutions = substitutions;
						for(map<string, string>::const_iterator inferred_value = inferred.begin();
							inferred_value != inferred.end(); ++inferred_value)
							pattern_substitutions[inferred_value->first] = inferred_value->second;
						// Resolving a dependent member alias may materialize its class
						// template, so use the expander's normal stateful rewriter while
						// deducing this candidate.
						match_pattern = const_cast<PA18TemplateExpander*>(this)->RewriteText(
							pattern, context, pattern_substitutions, 0);
						match_pattern = NormalizeTypeArgument(ReplaceIdentifiers(
							match_pattern, pattern_substitutions));
						match_pattern = ResolveAlias(match_pattern, context);
					}
					const bool matched = MatchTypePattern(match_pattern, type, parameter_names, &one, context);
					if(!matched) {
						bool dependent = false;
						for(size_t p = 0; p < definition.parameters.size(); ++p)
							if(definition.parameters[p].name == pattern) dependent = true;
						if(dependent) return false;
						string fixed_pattern = CanonicalSpelling(match_pattern);
						string fixed_actual = CanonicalSpelling(type);
						while(fixed_pattern.compare(0, 6, "const ") == 0)
							fixed_pattern = CanonicalSpelling(fixed_pattern.substr(6));
						while(fixed_actual.compare(0, 6, "const ") == 0)
							fixed_actual = CanonicalSpelling(fixed_actual.substr(6));
						while(fixed_pattern.size() > 6 &&
							fixed_pattern.compare(fixed_pattern.size() - 6, 6, " const") == 0)
							fixed_pattern = CanonicalSpelling(fixed_pattern.substr(0, fixed_pattern.size() - 6));
						while(fixed_actual.size() > 6 &&
							fixed_actual.compare(fixed_actual.size() - 6, 6, " const") == 0)
							fixed_actual = CanonicalSpelling(fixed_actual.substr(0, fixed_actual.size() - 6));
						if((fixed_pattern.find('*') != string::npos) !=
							(fixed_actual.find('*') != string::npos)) return false;
						while(!fixed_pattern.empty() && (fixed_pattern[fixed_pattern.size() - 1] == '&' ||
							fixed_pattern[fixed_pattern.size() - 1] == '*'))
							fixed_pattern.erase(fixed_pattern.size() - 1);
						while(!fixed_actual.empty() && (fixed_actual[fixed_actual.size() - 1] == '&' ||
							fixed_actual[fixed_actual.size() - 1] == '*'))
							fixed_actual.erase(fixed_actual.size() - 1);
						while(fixed_pattern.size() > 6 &&
							fixed_pattern.compare(fixed_pattern.size() - 6, 6, " const") == 0)
							fixed_pattern = CanonicalSpelling(fixed_pattern.substr(0, fixed_pattern.size() - 6));
						while(fixed_actual.size() > 6 &&
							fixed_actual.compare(fixed_actual.size() - 6, 6, " const") == 0)
							fixed_actual = CanonicalSpelling(fixed_actual.substr(0, fixed_actual.size() - 6));
						if(FindClassDeclaration(fixed_pattern, context) &&
							FindClassDeclaration(fixed_actual, context) &&
							LastComponent(fixed_pattern) != LastComponent(fixed_actual)) return false;
					} else {
						for(map<string, string>::const_iterator it = one.begin(); it != one.end(); ++it) {
							const size_t template_index = find_if(definition.parameters.begin(),
								definition.parameters.end(), [&](const TemplateParameter& candidate) {
									return candidate.name == it->first;
								}) - definition.parameters.begin();
							if(template_index < definition.parameters.size() &&
								definition.parameters[template_index].pack) {
								const vector<string> values = SplitTemplateArguments(it->second);
								if(values.empty()) inferred_packs[it->first].push_back(it->second);
								else inferred_packs[it->first].insert(
									inferred_packs[it->first].end(), values.begin(), values.end());
							}
						else {
							map<string, string>::const_iterator prior = inferred.find(it->first);
							if(prior != inferred.end() &&
								CanonicalSpelling(ResolveAlias(prior->second, context)) !=
								CanonicalSpelling(ResolveAlias(it->second, context))) return false;
							inferred[it->first] = it->second;
						}
						}
					}
					}
				++argument_index;
			}
		}
		if(argument_index != arguments->children.size()) return false;
		for(size_t deferred = 0; deferred < deferred_function_patterns.size(); ++deferred) {
			const vector<string> function_types = FunctionExpressionTypes(
				deferred_function_arguments[deferred], context);
			bool matched = false;
			for(size_t candidate = 0; candidate < function_types.size(); ++candidate) {
				map<string, string> one = inferred;
				if(!MatchTypePattern(deferred_function_patterns[deferred], function_types[candidate],
					parameter_names, &one, context)) continue;
				inferred = one;
				matched = true;
				break;
			}
			if(!matched) return false;
		}
		for(size_t i = 0; i < definition.parameters.size(); ++i) {
			const TemplateParameter& parameter = definition.parameters[i];
			if(parameter.pack) {
				map<string, vector<string> >::const_iterator found = inferred_packs.find(parameter.name);
				if(found != inferred_packs.end()) {
					result->insert(result->end(), found->second.begin(), found->second.end());
					if(inferred_pack_values) (*inferred_pack_values)[parameter.name] = found->second;
				} else if(inferred_pack_values)
					(*inferred_pack_values)[parameter.name] = vector<string>();
				continue;
			}
			map<string, string>::const_iterator found = inferred.find(parameter.name);
			if(found != inferred.end()) result->push_back(found->second);
			else if(!parameter.default_type.empty()) result->push_back(
				ReplaceIdentifiers(parameter.default_type, inferred));
			else return false;
		}
		return true;
	}
bool PA18TemplateExpander::TransformPackChild(
	const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& original_child,
	const string& child_context,
	const map<string, string>& substitutions,
	map<string, string>* local_substitutions,
	const CPPGMAstNodePtr& result)
{
	if(input->kind == "base-clause" && original_child &&
		original_child->kind == "base-specifier" &&
		ChildOfKindLocal(original_child, "pack-expansion")) {
		const CPPGMAstNodePtr original_base = ChildOfKindLocal(original_child, "base-name");
		const string pack_name = PackExpansionIdentifier(original_base);
		map<string, vector<string> >::const_iterator pack =
			active_pack_substitutions_.find(pack_name);
		if(pack != active_pack_substitutions_.end()) {
			for(size_t element = 0; element < pack->second.size(); ++element) {
				CPPGMAstNodePtr expanded = CloneNode(original_child);
				RemoveParameterPackMarkers(expanded);
				const CPPGMAstNodePtr base = ChildOfKindLocal(expanded, "base-name");
				if(base && original_base) {
					map<string, string> one = substitutions;
					one[pack_name] = pack->second[element];
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
			const string identifier = ParameterIdentifier(original_child);
			vector<string>& expanded_identifiers =
				active_pack_identifier_substitutions_[identifier];
			for(size_t element = 0; element < values->second.size(); ++element) {
				ostringstream pack_suffix;
				pack_suffix << element + 1;
				const string expanded_name = identifier.empty() || element == 0 ? identifier :
					identifier + "__pack" + pack_suffix.str();
				if(!identifier.empty()) expanded_identifiers.push_back(expanded_name);
				map<string, string> one = substitutions;
				one[pack_name] = values->second[element];
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
void PA18TemplateExpander::TransformRegularChildren(const CPPGMAstNodePtr& input,
	const string& child_context, const string& function_context,
	const map<string, string>& substitutions,
	map<string, string>* local_substitutions,
	const CPPGMAstNodePtr& result)
{
	for(size_t i = 0; i < input->children.size(); ++i) {
		const CPPGMAstNodePtr original_child = input->children[i];
		if(TransformPackChild(input, original_child, child_context, substitutions,
			local_substitutions, result)) continue;
					if(input->kind == "decl-specifier" && input->value.find("decltype(") != string::npos &&
						original_child && (original_child->kind == "call-expression" || original_child->kind == "binary-expression")) continue;
				if(SkipUnusedNestedClass(input, original_child, child_context, substitutions, i)) continue;
			if(original_child && original_child->kind == "namespace-alias-definition") {
				const CPPGMAstNodePtr target = ChildOfKindLocal(original_child, "target");
				if(target && !target->value.empty() && local_substitutions)
					(*local_substitutions)[original_child->value] = RewriteText(
						target->value, child_context, *local_substitutions, 0, false);
				continue;
			}
			const string node_context = input->kind == "function-definition" && original_child && original_child->kind == "compound-statement" ? function_context : child_context;
			const CPPGMAstNodePtr using_target = original_child && original_child->kind == "using-declaration" ? ChildOfKindLocal(original_child, "target") : CPPGMAstNodePtr();
			const bool drop_function_using = using_target && IsOrdinaryTemplateUsingTarget(
				using_target->value, node_context) && class_contexts_.find(node_context) == class_contexts_.end(); CPPGMAstNodePtr child;
			if(input->kind == "using-declaration" && original_child && original_child->kind == "target") {
				child = CloneNode(original_child);
				const string raw_target = original_child->value;
				const size_t separator = raw_target.rfind("::");
				if(separator != string::npos &&
					raw_target.substr(0, separator) == raw_target.substr(separator + 2)) {
					map<string, string>::const_iterator alias = local_substitutions->find(
						raw_target.substr(0, separator));
					if(alias != local_substitutions->end() && !alias->second.empty())
						child->value = alias->second + "::" + LastComponent(alias->second);
					else child->value = RewriteText(raw_target, node_context,
						*local_substitutions, 0, false, false);
				} else child->value = RewriteText(raw_target, node_context,
					*local_substitutions, 0, false, false);
				} else child = TransformNode(original_child, node_context, *local_substitutions);
				if(child && input->kind == "array-suffix" && !child->children.empty() &&
					child->children[0]) {
					PA19IntegralValue bound;
					const string expression = ConstantExpressionSpelling(child->children[0]);
					if(EvaluateIntegralText(expression, node_context, *local_substitutions, &bound))
						child->children[0] = CPPGMAstNodePtr(new CPPGMAstNode(
							"literal", IntegralValueSpelling(bound)));
				}
				if(child && input->kind == "class-specifier" &&
					child->kind == "simple-declaration")
					RecordConstantArrayDeclaration(child, child_context,
						*local_substitutions);
				if(!child && input->kind == "decl-specifier-seq" && original_child &&
					(original_child->kind == "class-specifier" ||
						original_child->kind == "class-forward-declaration")) {
					map<string, string>::const_iterator promoted = local_class_names_.find(
						JoinPath(child_context, LastComponent(original_child->value)));
					if(promoted != local_class_names_.end())
						result->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
							"decl-specifier", "TT_IDENTIFIER:" + LastComponent(promoted->second))));
				}
				if(child && !drop_function_using && !(input->kind == "compound-statement" && !substitutions.empty() &&
					(original_child->kind == "alias-declaration" || (original_child->kind == "simple-declaration" &&
					SpellNode(original_child->children.empty() ? CPPGMAstNodePtr() : original_child->children[0]).find("typedef") != string::npos)))) result->children.push_back(child);
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
				}
			if(original_child && original_child->kind == "using-directive") RecordUsingDirective(original_child, local_substitutions);
			if(original_child && original_child->kind == "simple-declaration" && !original_child->children.empty() &&
				SpellNode(original_child->children[0]).find("typedef") != string::npos &&
				(!substitutions.empty() || input->value.find('<') != string::npos))
				RecordTypedefSubstitutions(original_child, child_context, local_substitutions);
		}
}

CPPGMAstNodePtr PA18TemplateExpander::RewriteRegularNodeValue(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions,
	const CPPGMAstNodePtr& result, string* promoted_name)
{
		bool template_replaced = false;
		const bool type_spelling = input->kind == "decl-specifier" ||
			input->kind == "type-name" || input->kind == "type-specifier";
	result->value = RewriteText(input->value, context, substitutions,
			&template_replaced, !type_spelling, true);
		// Non-type template substitutions are semantic values, not names.  Keep
		// them as literal AST nodes so PA11/lowering resolve the same typed fact
		// that selected the specialization.
		if(input->kind == "id-expression") {
			map<string, PA19IntegralValue>::const_iterator typed =
				active_integral_substitutions_.find(RemoveMarker(input->value));
			if(typed != active_integral_substitutions_.end() && typed->second.known) {
				const string spelling = IntegralValueSpelling(typed->second);
				if(typed->second.type.name == "bool")
					return CPPGMAstNodePtr(new CPPGMAstNode("keyword-literal",
						PA19Raw(typed->second) ? "KW_TRUE:true" : "KW_FALSE:false"));
				CPPGMAstNodePtr literal(new CPPGMAstNode("literal", spelling));
				literal->initializer_form = input->initializer_form;
				return literal;
			}
		}
		if((type_spelling || input->kind == "id-expression") &&
			result->value.find('<') != string::npos)
			result->value = RewriteText(result->value, context, substitutions, &template_replaced);
		if(input->kind == "target") {
			const string raw_target = RemoveMarker(input->value);
			const size_t separator = raw_target.rfind("::");
			if(separator != string::npos && raw_target.substr(0, separator) ==
				raw_target.substr(separator + 2)) {
				map<string, string>::const_iterator alias = substitutions.find(
					raw_target.substr(0, separator));
				if(alias != substitutions.end() && !alias->second.empty())
					result->value = alias->second + "::" + LastComponent(alias->second);
			}
		}
		if(input->kind == "decl-specifier" || input->kind == "type-name" ||
			input->kind == "type-specifier") {
			const size_t marker_colon = result->value.find(':');
			string marker;
			if(marker_colon != string::npos) {
				const string prefix = result->value.substr(0, marker_colon);
				if(prefix == "TT_IDENTIFIER" || prefix.compare(0, 3, "KW_") == 0 ||
					prefix.compare(0, 3, "OP_") == 0)
					marker = result->value.substr(0, marker_colon + 1);
			}
			const string spelling = RemoveMarker(result->value);
			string qualified = QualifyTypeArgument(spelling, context);
			string resolved = ResolveAlias(qualified, context);
			if(substitutions.empty() && input->value.find('<') == string::npos)
				resolved = qualified;
			if(resolved.find('<') != string::npos)
				resolved = RewriteText(resolved, context, substitutions, 0);
			if(resolved.find('(') != string::npos && resolved.find(')') != string::npos)
				resolved = qualified;
			if(resolved != qualified) qualified = resolved;
			if(qualified != spelling) result->value = marker + qualified;
			if(input->kind == "decl-specifier" && marker.empty() &&
				qualified != spelling && result->value.find(':') == string::npos)
				result->value = "TT_IDENTIFIER:" + qualified;
			if(input->kind == "decl-specifier" && marker.empty() &&
				(qualified.find(' ') != string::npos || qualified.find('*') != string::npos ||
					qualified.find('&') != string::npos || qualified.find('[') != string::npos))
				result->value = "TT_IDENTIFIER:" + qualified;
		}
		string promoted_local_class;
		if((input->kind == "class-specifier" || input->kind == "class-forward-declaration") &&
			function_contexts_.find(context) != function_contexts_.end()) {
			map<string, string>::const_iterator local = local_class_names_.find(
				JoinPath(context, LastComponent(input->value)));
			if(local != local_class_names_.end()) {
				promoted_local_class = LastComponent(local->second);
				result->value = promoted_local_class;
			}
		}
	if(input->kind == "decl-specifier" && template_replaced &&
			result->value.find(':') == string::npos)
			result->value = "TT_IDENTIFIER:" + result->value;
	*promoted_name = promoted_local_class;
	return CPPGMAstNodePtr();
}
CPPGMAstNodePtr PA18TemplateExpander::FinishRegularNode(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions,
	const CPPGMAstNodePtr& result, const string& promoted_local_class)
{
		string child_context = context;
		if(input->kind == "class-specifier" || input->kind == "class-forward-declaration")
			child_context = JoinPath(context, LastComponent(input->value));
		string function_context = context;
		if(input->kind == "function-definition") {
			const string function_name = DeclarationName(input);
			string function_owner;
			if(input->children.size() > 1 && input->children[1]) {
				const string qualified_name = RewriteText(
					FirstIdentifierLocal(input->children[1]), context, substitutions, 0,
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
		map<string, string> local_substitutions = substitutions;
		TransformRegularChildren(input, child_context, function_context, substitutions,
			&local_substitutions, result);
		if((input->kind == "parameter-declaration" || input->kind == "type-id") &&
			!substitutions.empty())
			for(map<string, string>::const_iterator substitution = substitutions.begin();
				substitution != substitutions.end(); ++substitution)
				if(!substitution->second.empty() && substitution->second[substitution->second.size() - 1] == '&' &&
					ContainsName(input, substitution->first) &&
					ContainsName(input, "&&"))
					CollapseForwardingReference(result);
		RewriteTemplateInitializer(input, context, substitutions, result);
		if(input->kind == "simple-declaration") ReifyReferenceType(result);
		if(input->kind == "binary-expression") {
			InstantiateOperatorTemplate(result, context, substitutions);
			RewriteOperatorFunctionArgument(result, context, substitutions);
		}
		if(!promoted_local_class.empty()) {
			// A local class is promoted into a translation-unit class so the
			// later semantic pass can name it.  Its constructor declaration must
			// follow that promoted identity as well; retaining the source-local
			// `pair_like` name makes the generated class appear to have no viable
			// constructor when it is initialized.
			for(size_t child = 0; child < result->children.size(); ++child) {
				const CPPGMAstNodePtr member = result->children[child];
				if(!member || (member->kind != "special-member-definition" &&
					member->kind != "special-member-declaration")) continue;
				member->value = promoted_local_class;
				const CPPGMAstNodePtr declarator = FunctionDeclarator(member);
				RenameParameterIdentifier(declarator, promoted_local_class);
			}
			const map<string, string>::const_iterator owner = function_owners_.find(context);
			generated_by_owner_[owner == function_owners_.end() ? PrefixComponent(context) :
				owner->second].push_back(result);
			return CPPGMAstNodePtr();
		}
	return result;
}
CPPGMAstNodePtr PA18TemplateExpander::TransformRegularNode(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions)
{
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
		result->dependent_base_lookup = input->dependent_base_lookup;
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
	return FinishRegularNode(input, context, substitutions, result,
		promoted_local_class);
}

} // namespace pa18_templates_internal
