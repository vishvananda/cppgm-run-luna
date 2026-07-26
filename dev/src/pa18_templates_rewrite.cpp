#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
#include <functional>
using namespace std;
namespace pa18_templates_internal {
void PA18TemplateExpander::ResolveFunctionArguments(const CPPGMAstNodePtr& result,
	const FunctionSignature* signature, const string& context,
	const map<string, string>* substitutions)
{
	if(!signature || !signature->parameters || result->children.size() < 2 ||
		!result->children[1] || result->children[1]->kind != "argument-list") return;
	const CPPGMAstNodePtr result_arguments = result->children[1];
	size_t argument = 0;
	for(size_t parameter = 0; parameter < signature->parameters->children.size() &&
		argument < result_arguments->children.size(); ++parameter) {
		const CPPGMAstNodePtr parameter_node = signature->parameters->children[parameter];
		if(!parameter_node || parameter_node->kind != "parameter-declaration") continue;
		string expected = FunctionTypeSpelling(parameter_node);
		if(substitutions && !expected.empty()) try {
			expected = RewriteText(expected, context, *substitutions, 0);
		} catch(const logic_error&) {
			++argument;
			continue;
		}
		expected = CanonicalSpelling(ResolveAlias(expected, context));
		if(expected.find("(*") != string::npos && !expected.empty() &&
			expected[expected.size() - 1] == '*')
			expected = CanonicalSpelling(expected.substr(0, expected.size() - 1));
		CPPGMAstNodePtr argument_node = result_arguments->children[argument];
		if(argument_node && argument_node->kind == "unary-expression" &&
			RemoveMarker(argument_node->value) == "&" && !argument_node->children.empty())
			argument_node = argument_node->children[0];
		if(argument_node && argument_node->kind == "id-expression") {
			const vector<const TemplateDefinition*> function_candidates =
				FindFunctionDefinitions(argument_node->value, context);
			for(size_t candidate = 0; candidate < function_candidates.size(); ++candidate) {
				vector<string> inferred;
				if(!InferFunctionFromExpected(*function_candidates[candidate], expected,
					&inferred, context)) continue;
				try {
					const string local_name = Instantiate(*function_candidates[candidate],
						inferred, context);
					// Keep an explicit address-of node intact.  The selected
					// specialization replaces the referenced declaration, not the
					// unary operator carrying it.
					argument_node->value = local_name;
					break;
				} catch(const logic_error&) {}
			}
		}
		++argument;
	}
}
CPPGMAstNodePtr PA18TemplateExpander::TransformSubscriptExpression(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions)
{
	if(!input || input->children.size() < 2) return CPPGMAstNodePtr();
	CPPGMAstNodePtr transformed(new CPPGMAstNode(input->kind, input->value));
	transformed->initializer_form = input->initializer_form;
	transformed->template_instantiation = input->template_instantiation;
	transformed->explicit_specialization = input->explicit_specialization;
	transformed->explicit_instantiation = input->explicit_instantiation;
	transformed->extern_instantiation = input->extern_instantiation;
	transformed->dependent_base_lookup = input->dependent_base_lookup;
	transformed->materialize_object_address = input->materialize_object_address;
	transformed->materialize_object_name = input->materialize_object_name;
	transformed->source_token_begin = input->source_token_begin;
	transformed->source_token_end = input->source_token_end;
	for(size_t child = 0; child < input->children.size(); ++child) {
		CPPGMAstNodePtr rewritten = TransformNode(input->children[child], context,
			substitutions);
		if(rewritten) transformed->children.push_back(rewritten);
	}
	if(transformed->children.size() < 2) return transformed;
	CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "."));
	member->children.push_back(transformed->children[0]);
	member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier",
		"operator[]")));
	CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
	call->children.push_back(member);
	CPPGMAstNodePtr arguments(new CPPGMAstNode("argument-list"));
	arguments->children.push_back(transformed->children[1]);
	call->children.push_back(arguments);
	if(InstantiateMemberCall(call, member, "operator[]", context, substitutions))
		return call;
	return transformed;
}
CPPGMAstNodePtr PA18TemplateExpander::TransformAssignmentExpression(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions)
{
	if(!input || input->children.size() < 2 ||
		RemoveMarker(input->value) != "=") return CPPGMAstNodePtr();
	CPPGMAstNodePtr left = TransformNode(input->children[0], context, substitutions);
	CPPGMAstNodePtr right = TransformNode(input->children[1], context, substitutions);
	if(!left || !right) return CPPGMAstNodePtr();
	CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "."));
	member->children.push_back(left);
	member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier",
		"operator=")));
	CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
	call->children.push_back(member);
	CPPGMAstNodePtr arguments(new CPPGMAstNode("argument-list"));
	arguments->children.push_back(right);
	call->children.push_back(arguments);
	const bool instantiated = InstantiateMemberCall(call, member, "operator=", context,
		substitutions);
	if(!instantiated)
		return CPPGMAstNodePtr();
	return call;
}
void PA18TemplateExpander::ResolveMemberFunctionArguments(
	const CPPGMAstNodePtr& result, const string& context,
	const map<string, string>& substitutions)
{
	if(!result || result->kind != "call-expression" || result->children.size() < 2 ||
	   !result->children[0] || result->children[0]->kind != "member-expression" ||
	   result->children[0]->children.size() < 2 || !result->children[0]->children[1] ||
	   !result->children[1] || result->children[1]->kind != "argument-list") return;
	const string member_name = LastComponent(result->children[0]->children[1]->value);
	if(member_name.empty()) return;
	string object_type;
	if(!InferArgument(result->children[0]->children[0], &object_type,
	                  substitutions, context)) return;
	object_type = CanonicalSpelling(ResolveAlias(RewriteText(object_type, context,
		substitutions, 0), context));
	while(object_type.compare(0, 6, "const ") == 0 ||
		object_type.compare(0, 9, "volatile ") == 0)
		object_type = CanonicalSpelling(object_type.substr(object_type.find(' ') + 1));
	while(!object_type.empty() && (object_type[object_type.size() - 1] == '*' ||
		object_type[object_type.size() - 1] == '&'))
		object_type = CanonicalSpelling(object_type.substr(0, object_type.size() - 1));
	const CPPGMAstNodePtr declaration = FindClassDeclaration(object_type, context);
	if(!declaration) return;
	for(size_t member = 0; member < declaration->children.size(); ++member) {
		const CPPGMAstNodePtr candidate = declaration->children[member];
		if(!candidate || (candidate->kind != "function-definition" &&
			candidate->kind != "simple-declaration") || candidate->children.size() < 2 ||
			LastComponent(FirstIdentifierLocal(candidate->children[1])) != member_name) continue;
		const CPPGMAstNodePtr clause = DescendantOfKind(
			candidate->children[1], "parameter-clause");
		if(!clause) continue;
		size_t argument = 0;
		for(size_t parameter = 0; parameter < clause->children.size() &&
			argument < result->children[1]->children.size(); ++parameter) {
			const CPPGMAstNodePtr parameter_node = clause->children[parameter];
			if(!parameter_node || parameter_node->kind != "parameter-declaration") continue;
			const string expected = FunctionTypeSpelling(parameter_node);
			CPPGMAstNodePtr original = result->children[1]->children[argument++];
			CPPGMAstNodePtr function_argument = original;
			if(function_argument && function_argument->kind == "unary-expression" &&
				RemoveMarker(function_argument->value) == "&" &&
				!function_argument->children.empty())
				function_argument = function_argument->children[0];
			if(expected.empty() || !function_argument ||
				function_argument->kind != "id-expression") continue;
			const vector<const TemplateDefinition*> candidates =
				FindFunctionDefinitions(function_argument->value, context);
			for(size_t candidate_index = 0; candidate_index < candidates.size();
				++candidate_index) {
				vector<string> inferred;
				if(!InferFunctionFromExpected(*candidates[candidate_index], expected,
					&inferred, context)) continue;
				const string local_name = Instantiate(*candidates[candidate_index],
					inferred, context);
				if(original && original->kind == "unary-expression" &&
					!original->children.empty())
					original->children[0]->value = local_name;
				else original->value = local_name;
				break;
			}
		}
	}
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
	if(!dependent_pattern) pattern = NormalizeTypeArgument(ResolveAlias(pattern, context));
	actual = NormalizeTypeArgument(ResolveAlias(actual, context));
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
			spelling.insert(at, " ");
			at += 1;
			}
		}
		return CanonicalSpelling(spelling);
	};
	pattern = separate_compact_cv(pattern);
	actual = separate_compact_cv(actual);
	pattern = CanonicalSpelling(pattern);
	// A dependent alias-template spelling cannot be resolved by the ordinary
	// concrete alias table: `mp_size_t<I>` has no concrete key until I is
	// deduced.  Match its typed target instead, retaining the formal argument
	// expressions so the normal class-specialization matcher can infer them.
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
					if(!alias_target.empty() && alias_target != pattern) {
						return MatchTypePattern(alias_target, actual, parameter_names, inferred,
							context, class_pattern);
					}
				}
			}
		}
	}
	const int direct_type_parameter = MatchDirectTypeParameter(pattern, actual,
		parameter_names, inferred, context, class_pattern);
	if(direct_type_parameter >= 0) return direct_type_parameter != 0;
	const int reference_array = MatchReferenceArrayPattern(pattern, actual,
		parameter_names, inferred);
	if(reference_array >= 0) return reference_array != 0;
	const int object_cv_match = MatchObjectCvPattern(pattern, actual,
		parameter_names, inferred, context, class_pattern);
	if(object_cv_match >= 0) return object_cv_match != 0;
	const size_t pattern_array_open = pattern.rfind('[');
	const size_t actual_array_open = actual.rfind('[');
	if(pattern_array_open != string::npos || actual_array_open != string::npos) {
		if(pattern_array_open == string::npos || actual_array_open == string::npos ||
			pattern.empty() || actual.empty() || pattern[pattern.size() - 1] != ']' ||
			actual[actual.size() - 1] != ']') return false;
		const string pattern_element = ArrayPatternElement(pattern.substr(0, pattern_array_open));
		string actual_element = ArrayPatternElement(actual.substr(0, actual_array_open));
		while(!actual_element.empty() && actual_element[actual_element.size() - 1] == '&')
			actual_element.erase(actual_element.size() - 1);
		actual_element = CanonicalSpelling(actual_element);
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
	if(!class_pattern && !direct_pattern_function && pattern.size() > 1 &&
		pattern[pattern.size() - 1] == '*' && pattern.find(")(") == string::npos &&
		MatchNestedFunctionPointerPattern(pattern, actual, parameter_names, inferred,
			context, class_pattern)) return true;
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
	const bool forwarding_reference_parameter = pattern.size() > 2 &&
		pattern.compare(pattern.size() - 2, 2, "&&") == 0 &&
		parameter_names.find(pattern.substr(0, pattern.size() - 2)) != parameter_names.end();
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
	if(pattern_has_pointer && actual_has_pointer && pattern_cv_qualified &&
		pattern_cv_kind != actual_cv_kind) return false;
		string cv_parameter = pattern;
		if(pattern_trailing_cv && !pattern_has_pointer)
			cv_parameter.erase(cv_parameter.size() -
				(pattern_trailing_cv == 1 ? 6 : 9));
		cv_parameter = CanonicalSpelling(cv_parameter);
		const bool preserve_pointee_cv = actual_pointer && !pattern_cv_qualified &&
			(pattern_pointer || (pattern_trailing_cv &&
				parameter_names.find(cv_parameter) != parameter_names.end()));
		while(pattern.compare(0, 6, "const ") == 0) pattern = CanonicalSpelling(pattern.substr(6));
		while(pattern.compare(0, 9, "volatile ") == 0) pattern = CanonicalSpelling(pattern.substr(9));
		if(!preserve_pointee_cv && !direct_parameter && !bare_reference_parameter &&
			!forwarding_reference_parameter) {
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
			pattern, actual, parameter_names, inferred)) return true;
		while(!pattern.empty() && pattern[pattern.size() - 1] == '&')
			pattern.erase(pattern.size() - 1);
		while(!actual.empty() && actual[actual.size() - 1] == '&')
			actual.erase(actual.size() - 1);
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
		if(!direct_parameter && !bare_reference_parameter &&
			!forwarding_reference_parameter && !preserve_pointee_cv) {
			while(actual.size() > 6 && actual.compare(actual.size() - 6, 6, " const") == 0)
				actual = CanonicalSpelling(actual.substr(0, actual.size() - 6));
			while(actual.size() > 9 && actual.compare(actual.size() - 9, 9, " volatile") == 0)
				actual = CanonicalSpelling(actual.substr(0, actual.size() - 9));
		}
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
			if(pattern_owner == string::npos || actual_owner == string::npos) return false;
		if(!FunctionOwnerCompatible(pattern.substr(pattern_owner, pattern_function - pattern_owner + 1),
			actual.substr(actual_owner, actual_function - actual_owner + 1), class_pattern)) return false;
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
			vector<string> pattern_parts = SplitTemplateArguments(pattern_arguments);
			vector<string> actual_parts;
			const auto template_base = [](string raw) {
				raw = CanonicalSpelling(raw);
				while(raw.compare(0, 6, "const ") == 0 || raw.compare(0, 9, "volatile ") == 0) raw = CanonicalSpelling(raw.substr(raw.find(' ') + 1));
				while(!raw.empty() && (raw[raw.size() - 1] == '&' || raw[raw.size() - 1] == '*')) raw.erase(raw.size() - 1);
				return CanonicalSpelling(raw);
			};
			const auto find_class_definition = [&](const string& raw) {
				const TemplateDefinition* direct = FindDefinition(raw, context);
				if(direct && direct->class_template) return direct;
				const string short_name = LastComponent(raw); const TemplateDefinition* found = 0;
				for(map<string, TemplateDefinition>::const_iterator item = definitions_.begin(); item != definitions_.end(); ++item) {
					if(LastComponent(item->first) != short_name || !item->second.class_template) continue;
					if(found && found != &item->second) return static_cast<const TemplateDefinition*>(0); found = &item->second;
				}
				return found;
			};
			const auto expand_defaults = [&](const TemplateDefinition* definition,
				vector<string>* parts) {
				if(!definition || !parts) return;
				map<string, string> bindings; size_t index = 0;
				for(size_t parameter = 0; parameter < definition->parameters.size(); ++parameter) {
					const TemplateParameter& item = definition->parameters[parameter];
					if(item.pack) { while(index < parts->size()) ++index; continue; }
					string value;
					if(index < parts->size()) value = (*parts)[index++];
					else if(!item.default_type.empty()) {
						value = ReplaceIdentifiers(item.default_type, bindings); parts->push_back(CanonicalSpelling(value)); ++index;
					} else break;
					if(!item.name.empty()) bindings[item.name] = value;
				}
			};
				if(actual_open == string::npos) {
					const CPPGMAstNodePtr actual_declaration = FindClassDeclaration(actual, context);
					if(actual_declaration) for(size_t child = 0; child < actual_declaration->children.size(); ++child) {
						const CPPGMAstNodePtr clause = actual_declaration->children[child]; if(!clause || clause->kind != "base-clause") continue;
						for(size_t base_index = 0; base_index < clause->children.size(); ++base_index) {
							const CPPGMAstNodePtr base_specifier = clause->children[base_index];
							const CPPGMAstNodePtr base_name = ChildOfKindLocal(base_specifier, "base-name");
							if(base_name && MatchTypePattern(pattern, CanonicalSpelling(base_name->value), parameter_names, inferred, context, class_pattern)) return true;
						}
					}
					const string pattern_base = LastComponent(pattern.substr(0, pattern_open));
					map<string, vector<string> >::const_iterator specialization = specialization_arguments_.find(LastComponent(actual));
					map<string, string>::const_iterator base = specialization_bases_.find(LastComponent(actual));
					if(specialization == specialization_arguments_.end() ||
						base == specialization_bases_.end()) return false;
					if(parameter_names.find(pattern_base) == parameter_names.end() &&
						LastComponent(base->second) != pattern_base)
						return MatchGeneratedBaseTypePattern(pattern, actual, pattern_base,
							parameter_names, inferred, context, class_pattern) > 0;
				if(parameter_names.find(pattern_base) != parameter_names.end())
					(*inferred)[pattern_base] = base->second;
				if(pattern_parts.empty()) {
					if(specialization->second.empty()) return true;
						const TemplateDefinition* actual_definition = FindDefinition(base->second, context);
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
					vector<string> explicit_actual_parts = SplitTemplateArguments(actual_arguments);
				if(parameter_names.find(pattern_base) != parameter_names.end()) {
					const TemplateDefinition* actual_definition = FindDefinition(
						actual.substr(0, actual_open), context);
						if(actual_definition && actual_definition->class_template) {
							vector<string> expanded_actual_parts; map<string, string> actual_bindings; size_t actual_index = 0;
							for(size_t parameter = 0; parameter < actual_definition->parameters.size(); ++parameter) {
								const TemplateParameter& actual_parameter = actual_definition->parameters[parameter];
								if(actual_parameter.pack) {
									size_t trailing_fixed = 0;
									for(size_t later = parameter + 1; later < actual_definition->parameters.size(); ++later) if(!actual_definition->parameters[later].pack) ++trailing_fixed;
									while(actual_index < explicit_actual_parts.size() && explicit_actual_parts.size() - actual_index > trailing_fixed) expanded_actual_parts.push_back(explicit_actual_parts[actual_index++]);
									continue;
								}
								string value;
								if(actual_index < explicit_actual_parts.size()) value = explicit_actual_parts[actual_index++];
								else if(!actual_parameter.default_type.empty()) value = ReplaceIdentifiers(actual_parameter.default_type, actual_bindings);
								else break;
								expanded_actual_parts.push_back(CanonicalSpelling(value));
								if(!actual_parameter.name.empty()) actual_bindings[actual_parameter.name] = value;
							}
						if(actual_index == explicit_actual_parts.size()) actual_parts.swap(expanded_actual_parts);
						else actual_parts = explicit_actual_parts;
					} else actual_parts = explicit_actual_parts;
				} else actual_parts = explicit_actual_parts;
				if(parameter_names.find(pattern_base) != parameter_names.end()) {
					(*inferred)[pattern_base] = CanonicalSpelling(actual.substr(0, actual_open));
				} else if(pattern_base != actual_base) {
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
					if(MatchGeneratedBaseTypePattern(pattern, actual, pattern_base,
						parameter_names, inferred, context, class_pattern) > 0) return true;
					return false;
				}
				if(parameter_names.find(pattern_base) == parameter_names.end())
					actual_parts = explicit_actual_parts;
			}
				const TemplateDefinition* pattern_definition = find_class_definition(template_base(pattern.substr(0, pattern_open)));
				const TemplateDefinition* actual_definition = actual_open == string::npos ? 0 : find_class_definition(template_base(actual.substr(0, actual_open)));
				expand_defaults(pattern_definition, &pattern_parts);
				expand_defaults(actual_definition, &actual_parts);
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
	for(size_t i = 0; i < input->children.size(); ++i) { const CPPGMAstNodePtr original_child = input->children[i];
		if(TransformPackChild(input, original_child, child_context, substitutions, local_substitutions, result)) continue;
			if(input->kind == "decl-specifier" && input->value.find("decltype(") != string::npos &&
				original_child && (original_child->kind == "call-expression" ||
				original_child->kind == "binary-expression" || original_child->kind == "conditional-expression" ||
				original_child->kind == "member-expression")) continue;
				if(SkipUnusedNestedClass(input, original_child, child_context, substitutions, i)) continue; if(original_child && original_child->kind == "namespace-alias-definition") {
				const CPPGMAstNodePtr target = ChildOfKindLocal(original_child, "target");
				if(target && !target->value.empty() && local_substitutions)
					(*local_substitutions)[original_child->value] = RewriteText(
						target->value, child_context, *local_substitutions, 0, false);
				continue;
			}
			const bool declarator_with_trailing_return = input->kind == "function-definition" && original_child && original_child->kind == "declarator" && DescendantOfKind(original_child, "trailing-return-type");
			const bool function_child_context = input->kind == "function-definition" && original_child && (original_child->kind == "compound-statement" || declarator_with_trailing_return || original_child->kind == "trailing-return-type");
			const string node_context = function_child_context ? function_context : child_context;
			const CPPGMAstNodePtr using_target = original_child && original_child->kind == "using-declaration" ? ChildOfKindLocal(original_child, "target") : CPPGMAstNodePtr();
			const bool drop_function_using = using_target && IsOrdinaryTemplateUsingTarget(using_target->value, node_context) && class_contexts_.find(node_context) == class_contexts_.end() &&
				!IsGeneratedMemberTemplateUsingTarget(using_target->value, node_context, local_substitutions ? *local_substitutions : substitutions); CPPGMAstNodePtr child;
			if(input->kind == "using-declaration" && original_child && original_child->kind == "target") {
				child = CloneNode(original_child);
				const string raw_target = original_child->value;
				const size_t separator = raw_target.rfind("::");
				if(separator != string::npos && raw_target.substr(0, separator) == raw_target.substr(separator + 2)) {
					map<string, string>::const_iterator alias = local_substitutions->find(raw_target.substr(0, separator));
					if(alias != local_substitutions->end() && !alias->second.empty())
						child->value = alias->second + "::" + LastComponent(alias->second);
						else child->value = RewriteText(raw_target, node_context, *local_substitutions, 0, false, false);
				} else child->value = RewriteText(raw_target, node_context, *local_substitutions, 0, false, false);
					} else if(input->kind == "member-expression" && i == 1 && original_child && original_child->kind == "identifier" && IsKnownMemberTemplateId(original_child->value)) child = CloneNode(original_child);
				else child = TransformNode(original_child, node_context, *local_substitutions); if(child && input->kind == "array-suffix" && !child->children.empty() && child->children[0]) {
					PA19IntegralValue bound;
					const string expression = ConstantExpressionSpelling(child->children[0]);
					if(EvaluateIntegralText(expression, node_context, *local_substitutions, &bound))
						child->children[0] = CPPGMAstNodePtr(new CPPGMAstNode(
							"literal", IntegralValueSpelling(bound)));
				}
			if(child && input->kind == "class-specifier" && child->kind == "simple-declaration" && HasReplayContext(substitutions))
				RecordConstantDeclaration(child, child_context, *local_substitutions);
			if(child && input->kind == "class-specifier" && child->kind == "simple-declaration")
				RecordConstantArrayDeclaration(child, child_context, *local_substitutions);
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
				SpellNode(original_child->children[0]).find("typedef") != string::npos &&
				(HasReplayContext(substitutions) || SpellNode(original_child).find('<') != string::npos))
				RecordTypedefSubstitutions(original_child, child_context, local_substitutions);
		}
}
bool PA18TemplateExpander::PreserveDependentStaticDeclarator(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions, const CPPGMAstNodePtr& result,
	string* promoted_name)
{
	if(!input || input->kind != "identifier") return false;
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
	if(!concrete_static_data || open == string::npos ||
		!TemplateRange(raw, open, &argument_text, &close) ||
		close + 2 >= raw.size() || raw.compare(close + 1, 2, "::") != 0) return false;
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
			value == "void" || value == "wchar_t";
		const bool known = class_contexts_.find(value) != class_contexts_.end() ||
			named_type_contexts_.find(value) != named_type_contexts_.end() ||
			type_aliases_.find(value) != type_aliases_.end() ||
			FindDefinition(value, context) != 0;
		if(bare && !builtin && !known) dependent_owner = true;
	}
	if(!dependent_owner) return false;
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
		if(input->kind == "function-definition") {
			const CPPGMAstNodePtr source_declarator = FunctionDeclarator(input);
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
		if(ConsumeMaterializedStaticAssert(input, result, child_context,
			local_substitutions)) return CPPGMAstNodePtr();
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
		MaterializeInitializerConstructor(input, result, context, substitutions);
		if(input->kind == "simple-declaration") ReifyReferenceType(result);
		if(input->kind == "binary-expression" || input->kind == "assignment-expression") {
			InstantiateOperatorTemplate(result, context, substitutions);
			RewriteOperatorFunctionArgument(result, context, substitutions);
		}
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
	const TemplateDefinition* specialization = FindExplicitFunctionSpecialization(
		base, explicit_arguments->second, context);
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
