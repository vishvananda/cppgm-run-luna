#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
using namespace std;
namespace pa18_templates_internal {

namespace {

int PointerObjectCv(const string& raw)
{
	const string spelling = CanonicalSpelling(raw);
	size_t marker = spelling.find("::*");
	if(marker != string::npos) {
		const size_t close = spelling.find(')', marker + 3);
		if(close != string::npos) {
			const string qualifiers = spelling.substr(marker + 3, close - marker - 3);
			int result = 0;
			if(qualifiers.find("const") != string::npos) result |= 1;
			if(qualifiers.find("volatile") != string::npos) result |= 2;
			if(result) return result;
		}
	}
	marker = spelling.find("(*");
	if(marker != string::npos) {
		const size_t close = spelling.find(')', marker + 2);
		if(close != string::npos && spelling.find(")(", close) == close) {
			const string qualifiers = spelling.substr(marker + 2, close - marker - 2);
			int result = 0;
			if(qualifiers.find("const") != string::npos) result |= 1;
			if(qualifiers.find("volatile") != string::npos) result |= 2;
			if(result) return result;
		}
	}
	if(spelling.find('*') == string::npos) return 0;
	const size_t function_close = spelling.rfind(')');
	if(function_close != string::npos) {
		const string suffix = Trim(spelling.substr(function_close + 1));
		if(suffix == "const") return 1;
		if(suffix == "volatile") return 2;
		if(suffix == "const volatile" || suffix == "volatile const" ||
			suffix == "constvolatile" || suffix == "volatileconst") return 3;
	}
	const size_t star = spelling.rfind('*');
	if(star == string::npos) return 0;
	const string suffix = Trim(spelling.substr(star + 1));
	if(suffix == "const") return 1;
	if(suffix == "volatile") return 2;
	if(suffix == "const volatile" || suffix == "volatile const" ||
		suffix == "constvolatile" || suffix == "volatileconst") return 3;
	return 0;
}

string WithoutPointerObjectCv(const string& raw)
{
	string spelling = CanonicalSpelling(raw);
	size_t marker = spelling.find("::*");
	if(marker != string::npos) {
		const size_t close = spelling.find(')', marker + 3);
		if(close != string::npos) {
			const string qualifiers = spelling.substr(marker + 3, close - marker - 3);
			if(!Trim(qualifiers).empty()) {
				spelling.erase(marker + 3, close - marker - 3);
				return CanonicalSpelling(spelling);
			}
		}
	}
	marker = spelling.find("(*");
	if(marker != string::npos) {
		const size_t close = spelling.find(')', marker + 2);
		if(close != string::npos && spelling.find(")(", close) == close) {
			const string qualifiers = spelling.substr(marker + 2, close - marker - 2);
			if(!Trim(qualifiers).empty()) {
				spelling.erase(marker + 2, close - marker - 2);
				return CanonicalSpelling(spelling);
			}
		}
	}
	const size_t function_close = spelling.rfind(')');
	if(function_close != string::npos) {
		const string suffix = Trim(spelling.substr(function_close + 1));
		if(suffix == "const" || suffix == "volatile" ||
			suffix == "const volatile" || suffix == "volatile const" ||
			suffix == "constvolatile" || suffix == "volatileconst") {
			spelling.erase(function_close + 1);
			return CanonicalSpelling(spelling);
		}
	}
	const size_t star = spelling.rfind('*');
	if(star != string::npos) {
		string suffix = Trim(spelling.substr(star + 1));
		if(suffix == "const" || suffix == "volatile" ||
			suffix == "const volatile" || suffix == "volatile const" ||
			suffix == "constvolatile" || suffix == "volatileconst")
			spelling.erase(star + 1);
	}
	return CanonicalSpelling(spelling);
}

bool CvWrappedParameter(const string& raw, const set<string>& parameter_names,
	int* cv, string* parameter)
{
	if(cv) *cv = 0;
	if(parameter) parameter->clear();
	string spelling = CanonicalSpelling(raw);
	int mask = 0;
	for(;;) {
		if(spelling.compare(0, 14, "constvolatile ") == 0) {
			mask |= 3; spelling = CanonicalSpelling(spelling.substr(14)); continue;
		}
		if(spelling.compare(0, 14, "volatileconst ") == 0) {
			mask |= 3; spelling = CanonicalSpelling(spelling.substr(14)); continue;
		}
		if(spelling.compare(0, 6, "const ") == 0) {
			mask |= 1; spelling = CanonicalSpelling(spelling.substr(6)); continue;
		}
		if(spelling.compare(0, 9, "volatile ") == 0) {
			mask |= 2; spelling = CanonicalSpelling(spelling.substr(9)); continue;
		}
		if(spelling.size() > 6 && spelling.compare(spelling.size() - 6, 6,
			" const") == 0) {
			mask |= 1; spelling = CanonicalSpelling(spelling.substr(0, spelling.size() - 6)); continue;
		}
		if(spelling.size() > 14 && spelling.compare(spelling.size() - 14, 14,
			" constvolatile") == 0) {
			mask |= 3; spelling = CanonicalSpelling(spelling.substr(0, spelling.size() - 14)); continue;
		}
		if(spelling.size() > 14 && spelling.compare(spelling.size() - 14, 14,
			" volatileconst") == 0) {
			mask |= 3; spelling = CanonicalSpelling(spelling.substr(0, spelling.size() - 14)); continue;
		}
		if(spelling.size() > 9 && spelling.compare(spelling.size() - 9, 9,
			" volatile") == 0) {
			mask |= 2; spelling = CanonicalSpelling(spelling.substr(0, spelling.size() - 9)); continue;
		}
		break;
	}
	if(!mask || parameter_names.find(spelling) == parameter_names.end()) return false;
	if(cv) *cv = mask;
	if(parameter) *parameter = spelling;
	return true;
}

} // namespace

bool PA18TemplateExpander::SplitMemberPointerType(string raw, string* result_type,
	string* owner_type, vector<string>* parameters, string* qualifiers,
	bool* function_type) const
{
	raw = CanonicalSpelling(raw);
	const size_t marker = raw.find("::*");
	if(marker == string::npos) return false;
	if(result_type) result_type->clear();
	if(owner_type) owner_type->clear();
	if(parameters) parameters->clear();
	if(qualifiers) qualifiers->clear();
	if(function_type) *function_type = false;
	const size_t open = raw.rfind('(', marker);
	if(open != string::npos) {
		const string owner = Trim(raw.substr(open + 1, marker - open - 1));
		const string result = Trim(raw.substr(0, open));
		size_t suffix_begin = marker + 3;
		const size_t owner_close = raw.find(')', suffix_begin);
		if(owner_close == string::npos) return false;
		for(size_t position = suffix_begin; position < owner_close; ++position) {
			if(isspace(static_cast<unsigned char>(raw[position]))) continue;
			if(raw.compare(position, 5, "const") == 0) { position += 4; continue; }
			if(raw.compare(position, 7, "volatile") == 0) { position += 6; continue; }
			return false;
		}
		suffix_begin = owner_close + 1;
		const string suffix = raw.substr(suffix_begin);
		if(owner.empty() || result.empty() || suffix.empty() || suffix[0] != '(')
			return false;
		int depth = 0;
		size_t close = string::npos;
		for(size_t position = 0; position < suffix.size(); ++position) {
			if(suffix[position] == '(') ++depth;
			else if(suffix[position] == ')' && --depth == 0) {
				close = position;
				break;
			}
		}
		if(close == string::npos) return false;
		if(result_type) *result_type = CanonicalSpelling(result);
		if(owner_type) *owner_type = CanonicalSpelling(owner);
		if(parameters) *parameters = SplitTemplateArguments(suffix.substr(1, close - 1));
		if(qualifiers) *qualifiers = CanonicalSpelling(suffix.substr(close + 1));
		if(function_type) *function_type = true;
		return true;
	}
	const size_t separator = raw.rfind(' ', marker);
	const size_t owner_begin = separator == string::npos ? 0 : separator + 1;
	const string owner = Trim(raw.substr(owner_begin, marker - owner_begin));
	const string result = Trim(raw.substr(0, owner_begin));
	if(owner.empty() || result.empty()) return false;
	if(result_type) *result_type = CanonicalSpelling(result);
	if(owner_type) *owner_type = CanonicalSpelling(owner);
	return true;
}

namespace {
size_t MatchPatternPointerDepth(const string& spelling)
{
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
}

string SeparatePatternCv(string spelling)
{
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
			spelling.insert(at, " ");
			at += 1;
		}
	}
	return CanonicalSpelling(spelling);
}

int PatternTrailingCv(const string& spelling)
{
	if(spelling.size() >= 9 && spelling.compare(spelling.size() - 8, 8,
		"volatile") == 0 && spelling[spelling.size() - 9] == '*') return 2;
	if(spelling.size() >= 6 && spelling.compare(spelling.size() - 5, 5,
		"const") == 0 && spelling[spelling.size() - 6] == '*') return 1;
	if(spelling.size() >= 9 && spelling.compare(spelling.size() - 9, 9,
		" volatile") == 0) return 2;
	if(spelling.size() >= 6 && spelling.compare(spelling.size() - 6, 6,
		" const") == 0) return 1;
	return 0;
}

bool PatternHasTopLevelPointer(const string& spelling)
{
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
}

string MatchPatternTemplateBase(string raw)
{
	raw = CanonicalSpelling(raw);
	while(raw.compare(0, 6, "const ") == 0 ||
		raw.compare(0, 9, "volatile ") == 0)
		raw = CanonicalSpelling(raw.substr(raw.find(' ') + 1));
	while(!raw.empty() && (raw[raw.size() - 1] == '&' ||
		raw[raw.size() - 1] == '*')) raw.erase(raw.size() - 1);
	return CanonicalSpelling(raw);
}

bool IsBuiltinScalarWord(const string& word)
{
	return word == "bool" || word == "char" || word == "double" ||
		word == "float" || word == "int" || word == "long" ||
		word == "short" || word == "signed" || word == "unsigned" ||
		word == "void" || word == "wchar_t" || word == "char16_t" ||
		word == "char32_t" || word == "nullptr_t";
}

}

string CanonicalBuiltinScalarSpelling(string raw)
{
	raw = NormalizeTypeArgument(CanonicalSpelling(raw));
	vector<string> words;
	string word;
	for(size_t i = 0; i <= raw.size(); ++i) {
		const char ch = i < raw.size() ? raw[i] : ' ';
		if(isspace(static_cast<unsigned char>(ch))) {
			if(!word.empty()) {
				words.push_back(word);
				word.clear();
			}
		} else word += ch;
	}
	if(words.empty()) return raw;
	bool add_const = false, add_volatile = false, is_signed = false,
		is_unsigned = false, is_short = false, has_int = false;
	int long_count = 0;
	string base;
	for(size_t i = 0; i < words.size(); ++i) {
		const string& current = words[i];
		if(current == "const") add_const = true;
		else if(current == "volatile") add_volatile = true;
		else if(current == "signed") is_signed = true;
		else if(current == "unsigned") is_unsigned = true;
		else if(current == "short") is_short = true;
		else if(current == "long") ++long_count;
		else if(current == "int") has_int = true;
		else if(IsBuiltinScalarWord(current) && base.empty()) base = current;
		else return raw;
	}
	if((is_signed && is_unsigned) || (is_short && long_count != 0) ||
		long_count > 2)
		return raw;
	if(base.empty()) base = "int";
	string normalized;
	if(base == "int") {
		if(is_short) normalized = "short";
		else if(long_count == 1) normalized = "long";
		else if(long_count == 2) normalized = "long long";
		else normalized = "int";
		if(is_unsigned) normalized = "unsigned" +
			(normalized == "int" ? string() : " " + normalized);
		else if(is_signed && normalized == "int") normalized = "signed";
	} else if(base == "double" && long_count <= 1 && !is_short &&
		!is_signed && !is_unsigned && !has_int)
		normalized = long_count == 1 ? "long double" : "double";
	else if((base == "char" || base == "bool" || base == "float" ||
		base == "void" || base == "wchar_t" || base == "char16_t" ||
		base == "char32_t" || base == "nullptr_t") && long_count == 0 &&
		!is_short && !has_int && (base == "char" ||
			(!is_signed && !is_unsigned))) {
		normalized = base;
		if(base == "char" && is_unsigned) normalized = "unsigned char";
		else if(base == "char" && is_signed) normalized = "signed char";
	} else return raw;
	if(add_const) normalized = "const " + normalized;
	if(add_volatile) normalized = "volatile " + normalized;
	return CanonicalSpelling(normalized);
}

bool PA18TemplateExpander::MatchTypePattern(string pattern, string actual,
	const set<string>& parameter_names, map<string, string>* inferred,
	const string& context, bool class_pattern) const
{
	pattern = NormalizeTypeArgument(pattern);
	if(parameter_names.find(pattern) != parameter_names.end())
		return MatchDirectTypeParameter(pattern, actual, parameter_names, inferred,
			context, class_pattern);
	// A leading global qualifier is lookup syntax, not part of a type's
	// identity.  Recursive matching reaches qualified template arguments as
	// standalone spellings, so normalize it here as well as at the outer type.
	while(pattern.compare(0, 2, "::") == 0) pattern.erase(0, 2);
	while(actual.compare(0, 2, "::") == 0) actual.erase(0, 2);
	if(pattern.find('<') != string::npos) {
		set<string> active_aliases;
		pattern = ExpandAliasPattern(pattern, context, &active_aliases);
	}
	bool dependent_pattern = false;
	for(size_t position = 0; position < pattern.size();) {
		if(!IsIdentifierCharacter(pattern[position])) {
			++position;
			continue;
		}
		const size_t begin = position;
		while(position < pattern.size() && IsIdentifierCharacter(pattern[position])) ++position;
		if(parameter_names.find(pattern.substr(begin, position - begin)) != parameter_names.end()) {
			dependent_pattern = true;
			break;
		}
	}
	if(!dependent_pattern) pattern = NormalizeTypeArgument(CollapseRepeatedQualifier(
		ResolveAlias(pattern, context)));
	const auto has_dependent_identifier = [this, &parameter_names](const string& raw) {
		for(size_t position = 0; position < raw.size();) {
			if(!IsIdentifierCharacter(raw[position])) {
				++position;
				continue;
			}
			const size_t begin = position;
			while(position < raw.size() && IsIdentifierCharacter(raw[position])) ++position;
			const string word = raw.substr(begin, position - begin);
			if(parameter_names.find(word) != parameter_names.end() ||
				template_parameter_names_.find(word) != template_parameter_names_.end()) return true;
			size_t next = position;
			while(next < raw.size() && isspace(static_cast<unsigned char>(raw[next]))) ++next;
			if(next + 1 < raw.size() && raw.compare(next, 2, "::") == 0) continue;
			const bool known = word == "typename" || word == "const" || word == "volatile" ||
				word == "true" || word == "false" || word == "void" || word == "bool" ||
				word == "char" || word == "short" || word == "int" || word == "long" ||
				word == "signed" || word == "unsigned" ||
				class_contexts_.find(word) != class_contexts_.end() ||
				class_declarations_.find(word) != class_declarations_.end() ||
				definitions_by_name_.find(word) != definitions_by_name_.end() ||
				type_aliases_.find(word) != type_aliases_.end() ||
				constant_values_.find(word) != constant_values_.end();
			if(!known) return true;
		}
		return false;
	};
	// A materialized specialization is already the canonical nominal spelling
	// for matching.  Expanding it back through ResolveAlias can replay its source
	// template while a member lookup is selecting that same specialization (the
	// recursive `is_applyable<next<...>>` path); keep the typed generated identity
	// and let MatchClassTemplateBasePattern inspect its registered base instead.
	const bool generated_actual = specialization_bases_.find(LastComponent(actual)) !=
		specialization_bases_.end() && specialization_arguments_.find(
		LastComponent(actual)) != specialization_arguments_.end();
	const bool dependent_actual = generated_actual || has_dependent_identifier(actual);
	actual = NormalizeTypeArgument(CollapseRepeatedQualifier(dependent_actual ? actual :
		ResolveAlias(actual, context)));
	return MatchTypePatternNormalized(pattern, actual, parameter_names, inferred,
		context, class_pattern) != 0;
}

int PA18TemplateExpander::MatchTypePatternMemberPointerCases(
	const string& pattern, const string& actual,
	const set<string>& parameter_names, map<string, string>* inferred,
	const string& context, bool class_pattern) const
{
	string pattern_result, pattern_owner, pattern_qualifiers;
	string actual_result, actual_owner, actual_qualifiers;
	vector<string> pattern_parameters, actual_parameters;
	bool pattern_function = false, actual_function = false;
	const bool pattern_member = SplitMemberPointerType(pattern, &pattern_result,
		&pattern_owner, &pattern_parameters, &pattern_qualifiers, &pattern_function);
	const bool actual_member = SplitMemberPointerType(actual, &actual_result,
		&actual_owner, &actual_parameters, &actual_qualifiers, &actual_function);
	if(!pattern_member && !actual_member) return -1;
	if(!pattern_member || !actual_member || pattern_function != actual_function)
		return 0;
	if(!MatchTypePattern(pattern_owner, actual_owner, parameter_names, inferred,
		context, class_pattern) ||
		!MatchTypePattern(pattern_result, actual_result, parameter_names, inferred,
			context, class_pattern)) return 0;
	if(!pattern_function) return 1;
	if(CanonicalSpelling(pattern_qualifiers) != CanonicalSpelling(actual_qualifiers))
		return 0;
	const auto void_parameter_list = [](vector<string>* parameters) {
		if(parameters && parameters->size() == 1 &&
			CanonicalSpelling((*parameters)[0]) == "void") parameters->clear();
	};
	void_parameter_list(&pattern_parameters);
	void_parameter_list(&actual_parameters);
	const bool parameter_pack = !pattern_parameters.empty() &&
		pattern_parameters.back().size() > 3 &&
		pattern_parameters.back().compare(pattern_parameters.back().size() - 3, 3, "...") == 0;
	if(parameter_pack) {
		const size_t fixed = pattern_parameters.size() - 1;
		if(actual_parameters.size() < fixed) return 0;
		for(size_t parameter = 0; parameter < fixed; ++parameter)
			if(!MatchTypePattern(pattern_parameters[parameter], actual_parameters[parameter],
				parameter_names, inferred, context, class_pattern)) return 0;
		const string pack_name = CanonicalSpelling(pattern_parameters.back().substr(
			0, pattern_parameters.back().size() - 3));
		if(parameter_names.find(pack_name) == parameter_names.end()) return 0;
		string combined;
		for(size_t parameter = fixed; parameter < actual_parameters.size(); ++parameter) {
			map<string, string> one;
			if(!MatchTypePattern(pack_name, actual_parameters[parameter], parameter_names,
				&one, context, class_pattern)) return 0;
			map<string, string>::const_iterator value = one.find(pack_name);
			if(value == one.end()) return 0;
			if(!combined.empty()) combined += ",";
			combined += CanonicalSpelling(value->second);
		}
		map<string, string>::const_iterator prior = inferred->find(pack_name);
		if(prior != inferred->end() && CanonicalSpelling(prior->second) !=
			CanonicalSpelling(combined)) return 0;
		(*inferred)[pack_name] = combined;
		return 1;
	}
	if(pattern_parameters.size() != actual_parameters.size()) return 0;
	for(size_t parameter = 0; parameter < pattern_parameters.size(); ++parameter)
		if(!MatchTypePattern(pattern_parameters[parameter], actual_parameters[parameter],
			parameter_names, inferred, context, class_pattern)) return 0;
	return 1;
}

int PA18TemplateExpander::MatchTypePatternNormalized(string pattern, string actual,
	const set<string>& parameter_names, map<string, string>* inferred,
	const string& context, bool class_pattern) const
{
	const size_t pattern_pointer_depth = MatchPatternPointerDepth(pattern);
	const size_t actual_pointer_depth = MatchPatternPointerDepth(actual);
	if(pattern_pointer_depth > 1 && actual_pointer_depth > 1) {
		if(pattern_pointer_depth != actual_pointer_depth) return 0;
		pattern.erase(pattern.size() - pattern_pointer_depth);
		actual.erase(actual.size() - actual_pointer_depth);
		return MatchTypePattern(pattern, actual, parameter_names, inferred,
			context, class_pattern) ? 1 : 0;
	}
	pattern = SeparatePatternCv(pattern);
	actual = SeparatePatternCv(actual);
	if(CanonicalBuiltinScalarSpelling(pattern) ==
		CanonicalBuiltinScalarSpelling(actual)) {
		return 1;
	}
	string member_pattern_result, member_pattern_owner, member_pattern_cv;
	string member_actual_result, member_actual_owner, member_actual_cv;
	vector<string> member_pattern_parameters, member_actual_parameters;
	bool member_pattern_function = false, member_actual_function = false;
	const bool has_member_pattern = SplitMemberPointerType(pattern,
		&member_pattern_result, &member_pattern_owner, &member_pattern_parameters,
		&member_pattern_cv, &member_pattern_function);
	const bool has_member_actual = SplitMemberPointerType(actual,
		&member_actual_result, &member_actual_owner, &member_actual_parameters,
		&member_actual_cv, &member_actual_function);
	// The compact type spelling can move cv from `Object::* const` to the
	// function suffix (`Object::*)() const`).  A structured member-pointer
	// pattern must compare that suffix as part of the member-function type;
	// only a cv wrapper such as `T const` should enter the generic pointer-cv
	// branch below.
	if(has_member_pattern && has_member_actual)
	{
		return MatchTypePatternMemberPointerCases(pattern, actual, parameter_names,
			inferred, context, class_pattern);
	}
	const int actual_pointer_cv = PointerObjectCv(actual);

	if(actual_pointer_cv) {
		int wrapper_cv = 0;
		string wrapper_parameter;
		const bool wrapped = CvWrappedParameter(pattern, parameter_names, &wrapper_cv,
			&wrapper_parameter);
		if(wrapped) {
			if(wrapper_cv != actual_pointer_cv) return 0;
			string unqualified = WithoutPointerObjectCv(actual);
			const int remaining = actual_pointer_cv & ~wrapper_cv;
			if(remaining & 2) unqualified = "volatile " + unqualified;
			if(remaining & 1) unqualified = "const " + unqualified;
			(*inferred)[wrapper_parameter] = CanonicalSpelling(unqualified);
			return 1;
		}
		const int pattern_pointer_cv = PointerObjectCv(pattern);
		if(!pattern_pointer_cv || pattern_pointer_cv != actual_pointer_cv) return 0;
		string unqualified_pattern = WithoutPointerObjectCv(pattern);
		string unqualified_actual = WithoutPointerObjectCv(actual);
		string actual_result;
		vector<string> actual_parameters;
		if(SplitFunctionPointerType(unqualified_actual, &actual_result,
			&actual_parameters)) {
			string actual_function = actual_result + "(";
			for(size_t parameter = 0; parameter < actual_parameters.size(); ++parameter) {
				if(parameter) actual_function += ',';
				actual_function += actual_parameters[parameter];
			}
			actual_function += ')';
			if(unqualified_pattern.size() > 1 &&
				unqualified_pattern[unqualified_pattern.size() - 1] == '*')
				unqualified_pattern.erase(unqualified_pattern.size() - 1);
			return MatchTypePattern(unqualified_pattern, actual_function,
				parameter_names, inferred, context, class_pattern) ? 1 : 0;
		}
		if(!unqualified_pattern.empty() && unqualified_pattern[unqualified_pattern.size() - 1] == '*' &&
			!unqualified_actual.empty() && unqualified_actual[unqualified_actual.size() - 1] == '*') {
			unqualified_pattern.erase(unqualified_pattern.size() - 1);
			unqualified_actual.erase(unqualified_actual.size() - 1);
			return MatchTypePattern(unqualified_pattern, unqualified_actual,
				parameter_names, inferred, context, class_pattern) ? 1 : 0;
		}
		return 0;
	}
	pattern = CanonicalSpelling(pattern);
	int result = MatchTypePatternSimpleCases(pattern, actual, parameter_names,
		inferred, context, class_pattern);
	if(result >= 0) return result;
	result = MatchTypePatternMemberPointerCases(pattern, actual, parameter_names,
		inferred, context, class_pattern);
	if(result >= 0) return result;
	result = MatchTypePatternFunctionCases(pattern, &actual, parameter_names,
		inferred, context, class_pattern);
	if(result >= 0) return result;
	const int cv_result = NormalizePatternCv(&pattern, &actual, parameter_names,
		inferred, class_pattern);
	if(cv_result < 0) return 0;
	if(cv_result > 0) return cv_result;
	result = MatchTypePatternCompound(pattern, actual, parameter_names, inferred,
		context, class_pattern);
	return result;
}

int PA18TemplateExpander::MatchTypePatternSimpleCases(const string& pattern,
	const string& actual, const set<string>& parameter_names,
	map<string, string>* inferred, const string& context, bool class_pattern) const
{
	const size_t alias_open = pattern.find('<');
	if(alias_open != string::npos) {
		string alias_arguments;
		size_t alias_close = string::npos;
		if(TemplateRange(pattern, alias_open, &alias_arguments, &alias_close)) {
			const string alias_base = CanonicalSpelling(pattern.substr(0, alias_open));
			const TemplateDefinition* alias_definition = FindDefinition(alias_base, context);
			if(alias_definition && alias_definition->alias_template &&
				alias_definition->declaration &&
				!alias_definition->declaration->children.empty()) {
				const vector<string> alias_parts = SplitTemplateArguments(alias_arguments);
				map<string, string> alias_substitutions;
				for(size_t parameter = 0; parameter < alias_definition->parameters.size() &&
					parameter < alias_parts.size(); ++parameter)
					if(!alias_definition->parameters[parameter].name.empty())
						alias_substitutions[alias_definition->parameters[parameter].name] =
							alias_parts[parameter];
				string alias_target = TypeIdSpelling(alias_definition->declaration->children[0]);
				alias_target = ReplaceIdentifiersPreservingPackSizes(alias_target,
					alias_substitutions);
				alias_target = CanonicalSpelling(alias_target + pattern.substr(alias_close + 1));
				if(!alias_target.empty() && alias_target != pattern)
					return MatchTypePattern(alias_target, actual, parameter_names,
						inferred, context, class_pattern) ? 1 : 0;
			}
		}
	}
	const int direct_type_parameter = MatchDirectTypeParameter(pattern, actual,
		parameter_names, inferred, context, class_pattern);
	if(direct_type_parameter >= 0) return direct_type_parameter;
	const int reference_array = MatchReferenceArrayPattern(pattern, actual,
		parameter_names, inferred);
	if(reference_array >= 0) return reference_array;
	const int object_cv_match = MatchObjectCvPattern(pattern, actual,
		parameter_names, inferred, context, class_pattern);
	if(object_cv_match >= 0) return object_cv_match;
	const size_t pattern_array_open = pattern.rfind('[');
	const size_t actual_array_open = actual.rfind('[');
	if(pattern_array_open == string::npos && actual_array_open == string::npos) return -1;
	// An lvalue array passed to a forwarding reference is represented by the
	// typed call fact as `U(&)[N]`.  Handle that dependent reference before the
	// ordinary array matcher, which otherwise rejects the scalar `T&&` pattern
	// merely because only the actual spelling contains brackets.
	if(pattern_array_open == string::npos && actual_array_open != string::npos &&
		pattern.size() > 2 && pattern.compare(pattern.size() - 2, 2, "&&") == 0 &&
		parameter_names.find(pattern.substr(0, pattern.size() - 2)) != parameter_names.end())
		return MatchForwardingReferencePattern(pattern, actual, parameter_names, inferred) ? 1 : 0;
	if(pattern_array_open == string::npos || actual_array_open == string::npos ||
		pattern.empty() || actual.empty() || pattern[pattern.size() - 1] != ']' ||
		actual[actual.size() - 1] != ']') return 0;
	const string pattern_element = ArrayPatternElement(pattern.substr(0, pattern_array_open));
	string actual_element = ArrayPatternElement(actual.substr(0, actual_array_open));
	while(!actual_element.empty() && actual_element[actual_element.size() - 1] == '&')
		actual_element.erase(actual_element.size() - 1);
	actual_element = CanonicalSpelling(actual_element);
	if(!MatchTypePattern(pattern_element, actual_element, parameter_names,
		inferred, context, class_pattern)) return 0;
	const string pattern_bound = CanonicalSpelling(pattern.substr(
		pattern_array_open + 1, pattern.size() - pattern_array_open - 2));
	const string actual_bound = CanonicalSpelling(actual.substr(
		actual_array_open + 1, actual.size() - actual_array_open - 2));
	if(pattern_bound.empty() || actual_bound.empty())
		return pattern_bound.empty() && actual_bound.empty() ? 1 : 0;
	if(parameter_names.find(pattern_bound) != parameter_names.end()) {
		map<string, string>::const_iterator prior = inferred->find(pattern_bound);
		if(prior != inferred->end() && CanonicalSpelling(prior->second) != actual_bound)
			return 0;
		(*inferred)[pattern_bound] = actual_bound;
		return 1;
	}
	return CanonicalSpelling(ReplaceIdentifiers(pattern_bound, *inferred)) ==
		actual_bound ? 1 : 0;
}

int PA18TemplateExpander::MatchTypePatternFunctionCases(const string& raw_pattern,
	string* actual_value, const set<string>& parameter_names,
	map<string, string>* inferred, const string& context, bool class_pattern) const
{
	if(!actual_value) return 0;
	string pattern = raw_pattern;
	string actual = *actual_value;
	const auto normalize_function_pointer_alias = [this](string value) {
		if(value.empty() || value[value.size() - 1] != '*') return value;
		string result, qualifiers;
		vector<string> parameters;
		if(!SplitDirectFunctionType(value.substr(0, value.size() - 1), &result,
			&parameters, &qualifiers)) return value;
		string normalized = result + "(*) (";
		for(size_t parameter = 0; parameter < parameters.size(); ++parameter) {
			if(parameter) normalized += ',';
			normalized += parameters[parameter];
		}
		normalized += ")" + qualifiers;
		return CanonicalSpelling(normalized);
	};
	pattern = normalize_function_pointer_alias(pattern);
	actual = normalize_function_pointer_alias(actual);
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
				context, class_pattern)) return 0;
		if(!pattern_parameters.empty() && pattern_parameters.back().size() > 3 &&
			pattern_parameters.back().compare(pattern_parameters.back().size() - 3, 3, "...") == 0) {
			const size_t fixed = pattern_parameters.size() - 1;
			if(actual_parameters.size() < fixed) return 0;
			for(size_t parameter = 0; parameter < fixed; ++parameter)
				if(!MatchTypePattern(pattern_parameters[parameter], actual_parameters[parameter],
					parameter_names, inferred, context, class_pattern)) return 0;
			const string pack_pattern = CanonicalSpelling(pattern_parameters.back().substr(
				0, pattern_parameters.back().size() - 3));
			if(parameter_names.find(pack_pattern) == parameter_names.end()) return 0;
			string combined;
			for(size_t parameter = fixed; parameter < actual_parameters.size(); ++parameter) {
				map<string, string> one;
				if(!MatchTypePattern(pack_pattern, actual_parameters[parameter], parameter_names,
					&one, context, class_pattern)) return 0;
				map<string, string>::const_iterator value = one.find(pack_pattern);
				if(value == one.end()) return 0;
				if(!combined.empty()) combined += ",";
				combined += CanonicalSpelling(value->second);
			}
			map<string, string>::const_iterator prior = inferred->find(pack_pattern);
			if(prior != inferred->end() && CanonicalSpelling(prior->second) !=
				CanonicalSpelling(combined)) return 0;
			(*inferred)[pack_pattern] = combined;
			return 1;
		}
		if(pattern_parameters.size() != actual_parameters.size()) return 0;
		const auto adjust_function_parameter = [this](const string& value) {
			string result, qualifiers;
			vector<string> parameters;
			if(!SplitDirectFunctionType(value, &result, &parameters, &qualifiers)) return value;
			string adjusted = result + "(*) (";
			for(size_t parameter = 0; parameter < parameters.size(); ++parameter) {
				if(parameter) adjusted += ',';
				adjusted += parameters[parameter];
			}
			adjusted += ")" + qualifiers;
			return CanonicalSpelling(adjusted);
		};
		for(size_t parameter = 0; parameter < pattern_parameters.size(); ++parameter)
			if(!MatchTypePattern(pattern_parameters[parameter],
				(pattern_parameters[parameter].size() > 0 &&
				 (pattern_parameters[parameter][pattern_parameters[parameter].size() - 1] == '&' ||
				  (pattern_parameters[parameter].size() > 1 &&
				   pattern_parameters[parameter].compare(
					pattern_parameters[parameter].size() - 2, 2, "&&") == 0))) ?
					actual_parameters[parameter] : adjust_function_parameter(actual_parameters[parameter]),
				parameter_names, inferred, context, class_pattern)) return 0;
		return 1;
	}
	if(!class_pattern && direct_actual_function && pattern.find(")(") != string::npos &&
		actual_qualifiers.empty()) {
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
			CanonicalSpelling(ResolveAlias(actual, context))) return 0;
		(*inferred)[pattern] = actual;
		return 1;
	}
	if(!class_pattern && !direct_pattern_function && pattern.size() > 1 &&
		pattern[pattern.size() - 1] == '*' && pattern.find(")(") == string::npos &&
		MatchNestedFunctionPointerPattern(pattern, actual, parameter_names, inferred,
			context, class_pattern)) return 1;
	string direct_function_reference_name;
	if(pattern.size() > 1 && pattern[pattern.size() - 1] == '&') {
		direct_function_reference_name = CanonicalSpelling(pattern.substr(0, pattern.size() - 1));
		while(direct_function_reference_name.compare(0, 6, "const ") == 0)
			direct_function_reference_name = CanonicalSpelling(
				direct_function_reference_name.substr(6));
		while(direct_function_reference_name.compare(0, 9, "volatile ") == 0)
			direct_function_reference_name = CanonicalSpelling(
				direct_function_reference_name.substr(9));
		while(direct_function_reference_name.size() > 6 &&
			direct_function_reference_name.compare(
				direct_function_reference_name.size() - 6, 6, " const") == 0)
			direct_function_reference_name = CanonicalSpelling(
				direct_function_reference_name.substr(0,
					direct_function_reference_name.size() - 6));
		while(direct_function_reference_name.size() > 9 &&
			direct_function_reference_name.compare(
				direct_function_reference_name.size() - 9, 9, " volatile") == 0)
			direct_function_reference_name = CanonicalSpelling(
				direct_function_reference_name.substr(0,
					direct_function_reference_name.size() - 9));
	}
	const bool direct_function_reference_parameter = !direct_function_reference_name.empty() &&
		parameter_names.find(direct_function_reference_name) != parameter_names.end();
	const bool forwarding_function_reference_parameter = pattern.size() > 2 &&
		pattern.compare(pattern.size() - 2, 2, "&&") == 0 &&
		parameter_names.find(pattern.substr(0, pattern.size() - 2)) != parameter_names.end();
	if(direct_actual_function && !actual_function_converted &&
		!direct_function_reference_parameter && !forwarding_function_reference_parameter) return 0;
	*actual_value = actual;
	return -1;
}

int PA18TemplateExpander::NormalizePatternCv(string* pattern_value, string* actual_value,
	const set<string>& parameter_names, map<string, string>* inferred,
	bool class_pattern) const
{
	if(!pattern_value || !actual_value) return -1;
	string& pattern = *pattern_value;
	string& actual = *actual_value;
	const int pattern_trailing_cv = PatternTrailingCv(pattern);
	const int actual_trailing_cv = PatternTrailingCv(actual);
	const bool pattern_has_pointer = PatternHasTopLevelPointer(pattern);
	const bool actual_has_pointer = PatternHasTopLevelPointer(actual);
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
	const bool forwarding_reference_parameter = pattern.size() > 2 &&
		pattern.compare(pattern.size() - 2, 2, "&&") == 0 &&
		parameter_names.find(pattern.substr(0, pattern.size() - 2)) != parameter_names.end();
	if((class_pattern || pattern_has_pointer || actual_has_pointer) &&
		pattern_effective_cv && pattern_effective_cv != actual_effective_cv &&
		!reference_pattern) return -1;
	if((class_pattern || pattern_has_pointer || actual_has_pointer) &&
		actual_effective_cv && !pattern_effective_cv && !direct_parameter &&
		!reference_pattern) return -1;
	if(pattern_has_pointer && pattern_trailing_cv &&
		pattern_trailing_cv != actual_trailing_cv) return -1;
	const bool pattern_pointer = !pattern.empty() && pattern[pattern.size() - 1] == '*';
	const bool actual_pointer = !actual.empty() && actual[actual.size() - 1] == '*';
	const bool pattern_cv_qualified =
		pattern.compare(0, 6, "const ") == 0 ||
		pattern.compare(0, 9, "volatile ") == 0;
	const int pattern_cv_kind = pattern.compare(0, 6, "const ") == 0 ? 1 :
		(pattern.compare(0, 9, "volatile ") == 0 ? 2 : 0);
	const int actual_cv_kind = actual.compare(0, 6, "const ") == 0 ? 1 :
		(actual.compare(0, 9, "volatile ") == 0 ? 2 : 0);
	if(pattern_has_pointer && actual_has_pointer && pattern_cv_qualified &&
		pattern_cv_kind != actual_cv_kind) return -1;
	string cv_parameter = pattern;
	if(pattern_trailing_cv && !pattern_has_pointer)
		cv_parameter.erase(cv_parameter.size() -
			(pattern_trailing_cv == 1 ? 6 : 9));
	cv_parameter = CanonicalSpelling(cv_parameter);
	const bool preserve_pointee_cv = actual_pointer && !pattern_cv_qualified &&
		(pattern_pointer || (pattern_trailing_cv &&
			parameter_names.find(cv_parameter) != parameter_names.end()));
	while(pattern.compare(0, 6, "const ") == 0)
		pattern = CanonicalSpelling(pattern.substr(6));
	while(pattern.compare(0, 9, "volatile ") == 0)
		pattern = CanonicalSpelling(pattern.substr(9));
	if(!preserve_pointee_cv && !direct_parameter && !bare_reference_parameter &&
		!forwarding_reference_parameter) {
		while(actual.compare(0, 6, "const ") == 0)
			actual = CanonicalSpelling(actual.substr(6));
		while(actual.compare(0, 9, "volatile ") == 0)
			actual = CanonicalSpelling(actual.substr(9));
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
	if(!direct_parameter && !bare_reference_parameter &&
		!forwarding_reference_parameter) for(;;) {
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
	if(forwarding_reference_parameter && MatchForwardingReferencePattern(
		pattern, actual, parameter_names, inferred)) {
		return 1;
	}
	while(!pattern.empty() && pattern[pattern.size() - 1] == '&')
		pattern.erase(pattern.size() - 1);
	while(!actual.empty() && actual[actual.size() - 1] == '&')
		actual.erase(actual.size() - 1);
	if(!pattern.empty() && pattern[pattern.size() - 1] == '*') {
		if(actual.empty() || actual[actual.size() - 1] != '*') return -1;
		pattern.erase(pattern.size() - 1);
		actual.erase(actual.size() - 1);
	}
	pattern = CanonicalSpelling(pattern);
	actual = CanonicalSpelling(actual);
	while(pattern.size() > 6 && pattern.compare(pattern.size() - 6, 6, " const") == 0)
		pattern = CanonicalSpelling(pattern.substr(0, pattern.size() - 6));
	while(pattern.size() > 9 && pattern.compare(pattern.size() - 9, 9, " volatile") == 0)
		pattern = CanonicalSpelling(pattern.substr(0, pattern.size() - 9));
	if(!direct_parameter && !bare_reference_parameter &&
		!forwarding_reference_parameter && !preserve_pointee_cv) {
		while(actual.size() > 6 && actual.compare(actual.size() - 6, 6, " const") == 0)
			actual = CanonicalSpelling(actual.substr(0, actual.size() - 6));
		while(actual.size() > 9 && actual.compare(actual.size() - 9, 9, " volatile") == 0)
			actual = CanonicalSpelling(actual.substr(0, actual.size() - 9));
	}
	return 0;
}

int PA18TemplateExpander::MatchTypePatternCompound(const string& pattern,
	const string& actual, const set<string>& parameter_names,
	map<string, string>* inferred, const string& context, bool class_pattern) const
{
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
		if(pattern_owner == string::npos || actual_owner == string::npos) return 0;
		if(!FunctionOwnerCompatible(pattern.substr(pattern_owner,
			pattern_function - pattern_owner + 1), actual.substr(actual_owner,
			actual_function - actual_owner + 1), class_pattern)) return 0;
		const string pattern_result = pattern.substr(0, pattern_owner);
		const string actual_result = actual.substr(0, actual_owner);
		const vector<string> pattern_parameters = SplitTemplateArguments(pattern.substr(
			pattern_function + 2, pattern.size() - pattern_function - 3));
		const vector<string> actual_parameters = SplitTemplateArguments(actual.substr(
			actual_function + 2, actual.size() - actual_function - 3));
		if(pattern_parameters.size() != actual_parameters.size() ||
			!MatchTypePattern(pattern_result, actual_result, parameter_names, inferred,
				context, class_pattern)) return 0;
		for(size_t parameter = 0; parameter < pattern_parameters.size(); ++parameter)
			if(!MatchTypePattern(pattern_parameters[parameter], actual_parameters[parameter],
				parameter_names, inferred, context, class_pattern)) return 0;
		return 1;
	}
	if(parameter_names.find(pattern) != parameter_names.end() &&
		pattern.find('<') == string::npos) {
		map<string, string>::const_iterator prior = inferred->find(pattern);
		if(prior != inferred->end() &&
			CanonicalSpelling(ResolveAlias(prior->second, context)) !=
			CanonicalSpelling(ResolveAlias(actual, context))) return 0;
		(*inferred)[pattern] = actual;
		return 1;
	}
	const int class_result = MatchTypePatternClassCases(pattern, actual,
		parameter_names, inferred, context, class_pattern);
	if(class_result >= 0) return class_result;
	if(pattern == actual) return 1;
	if(pattern.find("::") == string::npos && actual.find("::") != string::npos) {
		const TemplateDefinition* actual_definition = FindDefinition(actual, context);
		if(actual_definition && actual_definition->class_template &&
			LastComponent(actual_definition->qualified_name) == pattern) return 1;
	}
	if((pattern.find("::") == string::npos) != (actual.find("::") == string::npos)) {
		string qualified_pattern = CanonicalSpelling(QualifyTypeArgument(pattern, context));
		string qualified_actual = CanonicalSpelling(QualifyTypeArgument(actual, context));
		if(pattern.find("::") != string::npos && actual.find("::") == string::npos) {
			const string expected_scope = PrefixComponent(pattern);
			if(!expected_scope.empty()) qualified_actual = CanonicalSpelling(
				QualifyTypeArgument(actual, expected_scope, expected_scope, true));
		}
		while(qualified_pattern.compare(0, 2, "::") == 0) qualified_pattern.erase(0, 2);
		while(qualified_actual.compare(0, 2, "::") == 0) qualified_actual.erase(0, 2);
		if(!qualified_pattern.empty() && qualified_pattern == qualified_actual) return 1;
	}
	return 0;
}

int PA18TemplateExpander::MatchTypePatternClassCases(const string& pattern,
	const string& actual, const set<string>& parameter_names,
	map<string, string>* inferred, const string& context, bool class_pattern) const
{
	const size_t pattern_open = pattern.find('<');
	if(pattern_open == string::npos) return -1;
	string pattern_arguments;
	size_t pattern_close = string::npos;
	if(!TemplateRange(pattern, pattern_open, &pattern_arguments, &pattern_close))
		return 0;
	const size_t actual_open = actual.find('<');
	vector<string> pattern_parts = SplitTemplateArguments(pattern_arguments);
	vector<string> actual_parts;
	int base_result = -1;
	if(actual_open == string::npos)
		base_result = MatchClassTemplateBasePattern(pattern, actual, pattern_parts,
			parameter_names, inferred, context, class_pattern, &actual_parts);
	else {
		const string pattern_base = LastComponent(pattern.substr(0, pattern_open));
		base_result = MatchActualTemplateBasePattern(pattern, actual, actual_open,
			pattern_base, parameter_names, inferred, context, class_pattern,
			&actual_parts);
	}
	if(base_result >= 0) return base_result;
	const bool outer_match = MatchClassTemplateArgumentLists(pattern, actual, pattern_open, actual_open,
		pattern_parts, actual_parts, parameter_names, inferred, context, class_pattern);
	const string pattern_tail = pattern.substr(pattern_close + 1);
	if(pattern_tail.empty()) return outer_match ? 1 : 0;
	if(pattern_tail.compare(0, 2, "::") != 0 || pattern_tail.find('<') == string::npos ||
		actual_open == string::npos)
		return outer_match ? 1 : 0;
	size_t actual_close = string::npos;
	string actual_arguments;
	if(!TemplateRange(actual, actual_open, &actual_arguments, &actual_close))
		return outer_match ? 1 : 0;
	const string actual_tail = actual_close == string::npos ? string() :
		actual.substr(actual_close + 1);
	if(!outer_match) return 0;
	// Object cv/reference suffixes and generated class spellings are normalized
	// independently by the ordinary matcher.  Only descend when both sides
	// actually carry another qualified template-id; that is the dependent
	// nested-owner form whose arguments must participate in deduction.
	if(actual_tail.empty() || actual_tail.compare(0, 2, "::") != 0 ||
		actual_tail.find('<') == string::npos)
		return 1;
	return MatchTypePattern(pattern_tail, actual_tail, parameter_names, inferred,
		context, class_pattern) ? 1 : 0;
}

int PA18TemplateExpander::MatchClassTemplateBasePattern(const string& pattern,
	const string& actual, const vector<string>& pattern_parts,
	const set<string>& parameter_names, map<string, string>* inferred,
	const string& context, bool class_pattern, vector<string>* actual_parts) const
{
	const CPPGMAstNodePtr actual_declaration = FindClassDeclaration(actual, context);
	if(!class_pattern && actual_declaration) for(size_t child = 0;
		child < actual_declaration->children.size(); ++child) {
		const CPPGMAstNodePtr clause = actual_declaration->children[child];
		if(!clause || clause->kind != "base-clause") continue;
		for(size_t base_index = 0; base_index < clause->children.size(); ++base_index) {
			const CPPGMAstNodePtr base_specifier = clause->children[base_index];
			const CPPGMAstNodePtr base_name = ChildOfKindLocal(base_specifier, "base-name");
			if(base_name && MatchTypePattern(pattern, CanonicalSpelling(base_name->value),
				parameter_names, inferred, context, class_pattern)) return 1;
		}
	}
	map<string, vector<string> >::const_iterator specialization =
		specialization_arguments_.find(LastComponent(actual));
	map<string, string>::const_iterator base =
		specialization_bases_.find(LastComponent(actual));
	if(specialization == specialization_arguments_.end() ||
		base == specialization_bases_.end()) return 0;
	const string pattern_base = LastComponent(pattern.substr(0, pattern.find('<')));
	if(parameter_names.find(pattern_base) == parameter_names.end() &&
		LastComponent(base->second) != pattern_base) {
		if(!class_pattern && MatchGeneratedBaseTypePattern(pattern, actual, pattern_base,
			parameter_names, inferred, context, class_pattern) > 0) return 1;
		return 0;
	}
	if(parameter_names.find(pattern_base) != parameter_names.end())
		(*inferred)[pattern_base] = base->second;
	if(pattern_parts.empty()) {
		if(specialization->second.empty()) return 1;
		const TemplateDefinition* actual_definition = FindDefinition(base->second, context);
		if(!actual_definition || actual_definition->parameters.size() !=
			specialization->second.size()) return 0;
		map<string, string> default_bindings;
		for(size_t parameter = 0; parameter < actual_definition->parameters.size(); ++parameter) {
			const TemplateParameter& actual_parameter = actual_definition->parameters[parameter];
			if(actual_parameter.default_type.empty()) return 0;
			const string expected = NormalizeTypeArgument(ReplaceIdentifiers(
				actual_parameter.default_type, default_bindings));
			const string concrete = NormalizeTypeArgument(specialization->second[parameter]);
			if(expected != concrete) return 0;
			if(!actual_parameter.name.empty()) default_bindings[actual_parameter.name] = concrete;
		}
		return 1;
	}
	if(actual_parts) {
		*actual_parts = specialization->second;
		if(!pattern_parts.empty() && pattern_parts.back().size() > 3 &&
			pattern_parts.back().compare(pattern_parts.back().size() - 3, 3, "...") == 0) {
			map<string, size_t>::const_iterator explicit_count =
				specialization_explicit_argument_counts_.find(LastComponent(actual));
			if(explicit_count != specialization_explicit_argument_counts_.end() &&
				explicit_count->second < actual_parts->size())
				actual_parts->resize(explicit_count->second);
		}
	}
	return -1;
}

int PA18TemplateExpander::MatchActualTemplateBasePattern(const string& pattern,
	const string& actual, size_t actual_open, const string& pattern_base,
	const set<string>& parameter_names, map<string, string>* inferred,
	const string& context, bool class_pattern, vector<string>* actual_parts) const
{
	string actual_arguments;
	size_t actual_close = string::npos;
	if(!TemplateRange(actual, actual_open, &actual_arguments, &actual_close)) return 0;
	const string actual_base = LastComponent(actual.substr(0, actual_open));
	vector<string> explicit_actual_parts = SplitTemplateArguments(actual_arguments);
	if(parameter_names.find(pattern_base) != parameter_names.end()) {
		const TemplateDefinition* actual_definition = FindDefinition(
			actual.substr(0, actual_open), context);
		if(actual_definition && actual_definition->class_template) {
			vector<string> expanded_actual_parts;
			map<string, string> actual_bindings;
			size_t actual_index = 0;
			for(size_t parameter = 0; parameter < actual_definition->parameters.size(); ++parameter) {
				const TemplateParameter& actual_parameter = actual_definition->parameters[parameter];
				if(actual_parameter.pack) {
					size_t trailing_fixed = 0;
					for(size_t later = parameter + 1; later < actual_definition->parameters.size(); ++later)
						if(!actual_definition->parameters[later].pack) ++trailing_fixed;
					while(actual_index < explicit_actual_parts.size() &&
						explicit_actual_parts.size() - actual_index > trailing_fixed)
						expanded_actual_parts.push_back(explicit_actual_parts[actual_index++]);
					continue;
				}
				string value;
				if(actual_index < explicit_actual_parts.size())
					value = explicit_actual_parts[actual_index++];
				else if(!actual_parameter.default_type.empty())
					value = ReplaceIdentifiers(actual_parameter.default_type, actual_bindings);
				else break;
				expanded_actual_parts.push_back(CanonicalSpelling(value));
				if(!actual_parameter.name.empty()) actual_bindings[actual_parameter.name] = value;
			}
			if(actual_index == explicit_actual_parts.size()) actual_parts->swap(expanded_actual_parts);
			else *actual_parts = explicit_actual_parts;
		} else *actual_parts = explicit_actual_parts;
		(*inferred)[pattern_base] = CanonicalSpelling(actual.substr(0, actual_open));
		return -1;
	}
	if(pattern_base != actual_base) {
		const TemplateDefinition* actual_definition = FindDefinition(
			actual.substr(0, actual_open), context);
		if(!class_pattern && actual_definition && actual_definition->class_template &&
			actual_definition->declaration) {
			const vector<string> concrete_parts = SplitTemplateArguments(actual_arguments);
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
						inferred, context, class_pattern)) return 1;
				}
			}
		}
		if(!class_pattern && MatchGeneratedBaseTypePattern(pattern, actual, pattern_base,
			parameter_names, inferred, context, class_pattern) > 0) return 1;
		return 0;
	}
	if(actual_parts) *actual_parts = explicit_actual_parts;
	return -1;
}

const TemplateDefinition* PA18TemplateExpander::FindClassPatternDefinition(
	const string& raw, const string& context) const
{
	const TemplateDefinition* direct = FindDefinition(raw, context);
	if(direct && direct->class_template) return direct;
	const string short_name = LastComponent(raw);
	const TemplateDefinition* found = 0;
	for(map<string, TemplateDefinition>::const_iterator item = definitions_.begin();
		item != definitions_.end(); ++item) {
		if(LastComponent(item->first) != short_name || !item->second.class_template) continue;
		if(found && found != &item->second) return 0;
		found = &item->second;
	}
	return found;
}

void PA18TemplateExpander::ExpandClassPatternDefaults(
	const TemplateDefinition* definition, vector<string>* parts) const
{
	if(!definition || !parts) return;
	map<string, string> bindings;
	size_t index = 0;
	for(size_t parameter = 0; parameter < definition->parameters.size(); ++parameter) {
		const TemplateParameter& item = definition->parameters[parameter];
		if(item.pack) {
			while(index < parts->size()) ++index;
			continue;
		}
		string value;
		if(index < parts->size()) value = (*parts)[index++];
		else if(!item.default_type.empty()) {
			value = ReplaceIdentifiers(item.default_type, bindings);
			parts->push_back(CanonicalSpelling(value));
			++index;
		} else break;
		if(!item.name.empty()) bindings[item.name] = value;
	}
}

bool PA18TemplateExpander::MatchClassTemplateArgumentLists(
	const string& pattern, const string& actual, size_t pattern_open,
	size_t actual_open, vector<string> pattern_parts, vector<string> actual_parts,
	const set<string>& parameter_names, map<string, string>* inferred,
	const string& context, bool class_pattern) const
{
	for(size_t actual_part = 0; actual_part < actual_parts.size(); ++actual_part)
		actual_parts[actual_part] = NormalizeTypeArgument(
			CollapseRepeatedQualifier(actual_parts[actual_part]));
	const TemplateDefinition* pattern_definition = FindClassPatternDefinition(
		MatchPatternTemplateBase(pattern.substr(0, pattern_open)), context);
	const TemplateDefinition* actual_definition = actual_open == string::npos ? 0 :
		FindClassPatternDefinition(MatchPatternTemplateBase(actual.substr(0, actual_open)),
			context);
	bool pattern_omits_defaults = pattern_definition &&
		pattern_parts.size() < pattern_definition->parameters.size();
	if(pattern_omits_defaults)
		for(size_t parameter = pattern_parts.size();
			parameter < pattern_definition->parameters.size(); ++parameter) {
			const TemplateParameter& omitted = pattern_definition->parameters[parameter];
			if(!omitted.pack && omitted.default_type.empty()) {
				pattern_omits_defaults = false;
				break;
			}
		}
	if(!pattern_omits_defaults)
		ExpandClassPatternDefaults(pattern_definition, &pattern_parts);
	const bool pattern_trailing_pack = !pattern_parts.empty() &&
		pattern_parts.back().size() > 3 &&
		pattern_parts.back().compare(pattern_parts.back().size() - 3, 3, "...") == 0;
	// A trailing pack in the deduction pattern consumes only the arguments
	// written at the use site.  Expanding the actual class's default arguments
	// first would make `tuple<T0, Ts...>` bind Ts to every defaulted null_type,
	// even though those defaults were not part of the caller's argument list.
	if(!pattern_trailing_pack) ExpandClassPatternDefaults(actual_definition, &actual_parts);
	if(pattern_trailing_pack && actual_definition) {
		map<string, string> actual_bindings;
		for(size_t parameter = 0; parameter < actual_definition->parameters.size() &&
			parameter < actual_parts.size(); ++parameter)
			if(!actual_definition->parameters[parameter].name.empty())
				actual_bindings[actual_definition->parameters[parameter].name] =
					actual_parts[parameter];
		while(!actual_parts.empty() && actual_parts.size() <=
			actual_definition->parameters.size()) {
			const size_t parameter = actual_parts.size() - 1;
			const TemplateParameter& detail = actual_definition->parameters[parameter];
			if(detail.pack || detail.default_type.empty()) break;
			const string expected = NormalizeTypeArgument(ResolveAlias(
				ReplaceIdentifiers(detail.default_type, actual_bindings), context));
			if(expected != NormalizeTypeArgument(actual_parts.back())) break;
			actual_parts.pop_back();
		}
	}
	if(pattern_trailing_pack) {
		const string pack_pattern = CanonicalSpelling(pattern_parts.back().substr(
			0, pattern_parts.back().size() - 3));
		if(parameter_names.find(pack_pattern) != parameter_names.end() ||
			pack_pattern.find('<') != string::npos)
			return MatchTrailingTypePack(pattern_parts, actual_parts, parameter_names,
				inferred, context, class_pattern);
	}
	if(pattern_omits_defaults) {
		if(actual_parts.size() < pattern_parts.size()) return false;
		for(size_t i = 0; i < pattern_parts.size(); ++i)
			if(!MatchTypePattern(pattern_parts[i], actual_parts[i], parameter_names,
				inferred, context, class_pattern)) return false;
		map<string, string> default_bindings = *inferred;
		for(size_t parameter = 0; parameter < pattern_parts.size() &&
			parameter < pattern_definition->parameters.size() &&
			parameter < actual_parts.size(); ++parameter) {
			const string& name = pattern_definition->parameters[parameter].name;
			if(!name.empty()) default_bindings[name] = actual_parts[parameter];
		}
		size_t actual_index = pattern_parts.size();
		for(size_t parameter = pattern_parts.size();
			parameter < pattern_definition->parameters.size(); ++parameter) {
			const TemplateParameter& detail = pattern_definition->parameters[parameter];
			if(detail.pack) return actual_index == actual_parts.size();
			if(detail.default_type.empty() || actual_index >= actual_parts.size()) return false;
			string expected = NormalizeTypeArgument(ResolveAlias(
				ReplaceIdentifiers(detail.default_type, default_bindings), context));
			map<string, string> ignored;
			if(!MatchTypePattern(expected, actual_parts[actual_index], set<string>(),
				&ignored, context, class_pattern)) return false;
			if(!detail.name.empty()) default_bindings[detail.name] = actual_parts[actual_index];
			++actual_index;
		}
		return actual_index == actual_parts.size();
	}
	if(pattern_parts.size() != actual_parts.size()) return false;
	for(size_t i = 0; i < pattern_parts.size(); ++i)
		if(!MatchTypePattern(pattern_parts[i], actual_parts[i], parameter_names,
			inferred, context, class_pattern)) return false;
	return true;
}
} // namespace pa18_templates_internal
