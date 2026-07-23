#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;
namespace pa18_templates_internal {

namespace {

bool HasStaticDataMemberLocal(const CPPGMAstNodePtr& node, const string& name)
{
	if(!node || name.empty()) return false;
	if(node->kind == "simple-declaration" && !node->children.empty() &&
		SpellNode(node->children[0]).find("static") != string::npos) {
		const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
		if(list) for(size_t item = 0; item < list->children.size(); ++item) {
			const CPPGMAstNodePtr entry = list->children[item];
			if(!entry || entry->children.empty()) continue;
			if(LastComponent(FirstIdentifierLocal(entry->children[0])) == name)
				return true;
		}
	}
	for(size_t child = 0; child < node->children.size(); ++child)
		if(HasStaticDataMemberLocal(node->children[child], name)) return true;
	return false;
}

} // namespace

CPPGMAstNodePtr PA18TemplateExpander::TransformSubscriptExpression(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions)
{
	if(!input || input->children.size() < 2) return CPPGMAstNodePtr();
	CPPGMAstNodePtr transformed(new CPPGMAstNode(input->kind, input->value));
	transformed->initializer_form = input->initializer_form;
	transformed->template_instantiation = input->template_instantiation;
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
	if(!InstantiateMemberCall(call, member, "operator=", context, substitutions))
		return CPPGMAstNodePtr();
	return call;
}

CPPGMAstNodePtr PA18TemplateExpander::TransformUnaryExpression(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions)
{
	if(!input || input->children.empty()) return CPPGMAstNodePtr();
	const string operation = RemoveMarker(input->value);
	if(operation.empty()) return CPPGMAstNodePtr();
	CPPGMAstNodePtr operand = TransformNode(input->children[0], context, substitutions);
	if(!operand) return CPPGMAstNodePtr();
	CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "."));
	member->children.push_back(operand);
	member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier",
		"operator" + operation)));
	CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
	call->children.push_back(member);
	call->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("argument-list")));
	if(!InstantiateMemberCall(call, member, "operator" + operation,
		context, substitutions)) {
		return CPPGMAstNodePtr();
	}
	return call;
}

void PA18TemplateExpander::MaterializeInitializerConstructor(
	const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& result,
	const string& context, const map<string, string>& substitutions)
{
	if(!input || !result || input->kind != "simple-declaration") return;
	const CPPGMAstNodePtr original_list = ChildOfKindLocal(input,
		"init-declarator-list");
	const CPPGMAstNodePtr transformed_list = ChildOfKindLocal(result,
		"init-declarator-list");
	if(!original_list || !transformed_list || original_list->children.size() != 1 ||
		transformed_list->children.size() != 1) return;
	const CPPGMAstNodePtr original_item = original_list->children[0];
	const CPPGMAstNodePtr transformed_item = transformed_list->children[0];
	if(!original_item || !transformed_item || original_item->children.size() < 2 ||
		transformed_item->children.size() < 2) return;
	CPPGMAstNodePtr original_initializer = original_item->children[1];
	CPPGMAstNodePtr transformed_initializer = transformed_item->children[1];
	if(!original_initializer || !transformed_initializer) return;
	vector<CPPGMAstNodePtr> arguments;
	CPPGMAstNodePtr initializer_expression = transformed_initializer;
	if(initializer_expression->kind == "initializer" &&
		initializer_expression->children.size() == 1)
		initializer_expression = initializer_expression->children[0];
	if(initializer_expression->kind == "paren-initializer" ||
		initializer_expression->kind == "braced-init-list")
		arguments = initializer_expression->children;
	else if(initializer_expression->kind != "initializer")
		arguments.push_back(initializer_expression);
	if(arguments.empty() && original_initializer->kind != "paren-initializer" &&
		original_initializer->kind != "braced-init-list") return;
	if(input->children.empty()) return;
	const CPPGMAstNodePtr declarator = original_item->children[0];
	string target = DeclaratorTypeSpelling(NodeTypeSpelling(input->children[0]),
		declarator);
	target = CanonicalSpelling(ResolveAlias(RewriteText(target, context,
		substitutions, 0), context));
	if(target.empty() || !FindClassDeclaration(target, context)) return;
	const string constructor_name = LastComponent(target);
	map<string, vector<string> >::const_iterator indexed_constructors =
		definitions_by_name_.find(constructor_name);
	bool has_member_template_constructor = false;
	if(indexed_constructors != definitions_by_name_.end())
		for(size_t candidate = 0; candidate < indexed_constructors->second.size(); ++candidate) {
			map<string, TemplateDefinition>::const_iterator found = definitions_.find(
				indexed_constructors->second[candidate]);
			if(found == definitions_.end()) continue;
			const TemplateDefinition& definition = found->second;
			if(!definition.class_template && !definition.alias_template &&
				definition.member_template && LastComponent(definition.name) == constructor_name) {
				has_member_template_constructor = true;
				break;
			}
		}
	if(!has_member_template_constructor) return;
	CPPGMAstNodePtr object(new CPPGMAstNode("id-expression"));
	object->inferred_type = target;
	CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "."));
	member->children.push_back(object);
	member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier",
		LastComponent(target))));
	CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
	call->children.push_back(member);
	CPPGMAstNodePtr argument_list(new CPPGMAstNode("argument-list"));
	argument_list->children = arguments;
	call->children.push_back(argument_list);
	InstantiateMemberCall(call, member, LastComponent(target), context, substitutions);
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
	pattern = NormalizeTypeArgument(ResolveAlias(pattern, context));
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
	const bool forwarding_reference_parameter = pattern.size() > 2 &&
		pattern.compare(pattern.size() - 2, 2, "&&") == 0 &&
		parameter_names.find(pattern.substr(0, pattern.size() - 2)) != parameter_names.end();
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
		// A forwarding-reference pattern deduces the underlying parameter as
		// an lvalue reference when the argument is an lvalue.  Handle the
		// two-character `&&` suffix before ordinary reference stripping;
		// otherwise `Args&&` becomes `Args&` and is no longer recognized as the
		// template parameter name.
		if(forwarding_reference_parameter) {
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
		if(!direct_parameter && !bare_reference_parameter &&
			!forwarding_reference_parameter) {
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
					const string pattern_base = LastComponent(pattern.substr(0, pattern_open));
					map<string, vector<string> >::const_iterator specialization =
						specialization_arguments_.find(LastComponent(actual));
					map<string, string>::const_iterator base =
						specialization_bases_.find(LastComponent(actual));
					if(specialization == specialization_arguments_.end() ||
						base == specialization_bases_.end()) return false;
					if(parameter_names.find(pattern_base) == parameter_names.end() &&
						LastComponent(base->second) != pattern_base)
						return MatchGeneratedBaseTypePattern(pattern, actual, pattern_base,
							parameter_names, inferred, context, class_pattern) > 0;
					if(parameter_names.find(pattern_base) != parameter_names.end())
						(*inferred)[pattern_base] = base->second;
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
	for(size_t i = 0; i < input->children.size(); ++i) { const CPPGMAstNodePtr original_child = input->children[i];
		if(TransformPackChild(input, original_child, child_context, substitutions, local_substitutions, result)) continue;
					if(input->kind == "decl-specifier" && input->value.find("decltype(") != string::npos && original_child && (original_child->kind == "call-expression" || original_child->kind == "binary-expression")) continue;
				if(SkipUnusedNestedClass(input, original_child, child_context, substitutions, i)) continue; if(original_child && original_child->kind == "namespace-alias-definition") {
				const CPPGMAstNodePtr target = ChildOfKindLocal(original_child, "target");
				if(target && !target->value.empty() && local_substitutions)
					(*local_substitutions)[original_child->value] = RewriteText(
						target->value, child_context, *local_substitutions, 0, false);
				continue;
			}
			const string node_context = input->kind == "function-definition" && original_child && original_child->kind == "compound-statement" ? function_context : child_context; const CPPGMAstNodePtr using_target = original_child && original_child->kind == "using-declaration" ? ChildOfKindLocal(original_child, "target") : CPPGMAstNodePtr();
			const bool drop_function_using = using_target && IsOrdinaryTemplateUsingTarget(
				using_target->value, node_context) && class_contexts_.find(node_context) == class_contexts_.end() &&
				!IsGeneratedMemberTemplateUsingTarget(using_target->value, node_context,
					local_substitutions ? *local_substitutions : substitutions); CPPGMAstNodePtr child;
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
					} else if(input->kind == "member-expression" && i == 1 && original_child &&
						original_child->kind == "identifier" &&
						IsKnownMemberTemplateId(original_child->value)) child = CloneNode(original_child);
					else child = TransformNode(original_child, node_context, *local_substitutions); if(child && input->kind == "array-suffix" && !child->children.empty() && child->children[0]) {
					PA19IntegralValue bound;
					const string expression = ConstantExpressionSpelling(child->children[0]);
					if(EvaluateIntegralText(expression, node_context, *local_substitutions, &bound))
						child->children[0] = CPPGMAstNodePtr(new CPPGMAstNode(
							"literal", IntegralValueSpelling(bound)));
				}
			if(child && input->kind == "class-specifier" &&
				child->kind == "simple-declaration" && HasReplayContext(substitutions))
				RecordConstantDeclaration(child, child_context, *local_substitutions);
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
				if(child && !drop_function_using && !(input->kind == "compound-statement" && HasReplayContext(substitutions) &&
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
	const bool concrete_static_data = concrete_replay_owner &&
		HasStaticDataMemberLocal(FindClassDeclaration(concrete_owner->second, context),
			LastComponent(raw));
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

CPPGMAstNodePtr PA18TemplateExpander::RewriteRegularNodeValue(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions,
	const CPPGMAstNodePtr& result, string* promoted_name)
{
		bool template_replaced = false;
	if(PreserveDependentStaticDeclarator(input, context, substitutions, result,
		promoted_name)) return CPPGMAstNodePtr();
	const bool type_spelling = input->kind == "decl-specifier" ||
		input->kind == "type-name" || input->kind == "type-specifier";
	result->value = RewriteText(input->value, context, substitutions,
			&template_replaced, !type_spelling, true);
	if((input->kind == "special-member-definition" ||
		input->kind == "special-member-declaration") &&
		input->value.find("::") != string::npos && result->value.find("::") == string::npos)
		result->value += "::" + LastComponent(input->value);
	if(type_spelling && (result->value.find('[') != string::npos ||
		result->value.find("(&") != string::npos ||
		result->value.find("(*") != string::npos)) {
		// A typedef such as `char (&type)[N]` is a valid return type, but its
		// expanded spelling cannot be installed as a declaration-specifier
		// (`char(&)[N]` would make the following function declarator invalid).
		// Keep the concrete generated alias as the AST type and reserve the
		// expanded spelling for typed expression queries such as sizeof.
		bool preserved_template = false;
		const string preserved = RewriteText(input->value, context, substitutions,
			&preserved_template, false, false);
		if(!preserved.empty()) result->value = preserved;
	}
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
			if(!HasReplayContext(substitutions) && input->value.find('<') == string::npos)
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
		result->explicit_instantiation = input->explicit_instantiation;
		result->extern_instantiation = input->extern_instantiation;
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
