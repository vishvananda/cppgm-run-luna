#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
using namespace std;
namespace pa18_templates_internal {

bool PA18TemplateExpander::ContainsSubstitutionIdentifier(
	const string& text, const map<string, string>& substitutions) const
{
	for(map<string, string>::const_iterator substitution = substitutions.begin();
		substitution != substitutions.end(); ++substitution) {
		const string& name = substitution->first;
		for(size_t position = text.find(name); position != string::npos;
			position = text.find(name, position + name.size())) {
			const bool left = position == 0 ||
				!IsIdentifierCharacter(text[position - 1]);
			const size_t end = position + name.size();
			if(left && (end == text.size() || !IsIdentifierCharacter(text[end]))) return true;
		}
	}
	return false;
}

bool PA18TemplateExpander::FunctionTemplateCvPointerTie(
	const TemplateDefinition& lhs, const TemplateDefinition& rhs) const
{
	const auto parameter_pattern = [this](const TemplateDefinition& definition) {
		vector<string> result;
		if(!definition.declaration) return result;
		const CPPGMAstNodePtr parameters = DescendantOfKind(
			FunctionDeclarator(definition.declaration), "parameter-clause");
		if(!parameters || parameters->children.size() != 1 || !parameters->children[0])
			return result;
		result.push_back(CanonicalSpelling(ParameterTypeSpelling(parameters->children[0])));
		return result;
	};
	const vector<string> left = parameter_pattern(lhs);
	const vector<string> right = parameter_pattern(rhs);
	if(left.size() != 1 || right.size() != 1 ||
		left[0].empty() || right[0].empty() ||
		left[0][left[0].size() - 1] != '*' || right[0][right[0].size() - 1] != '*')
		return false;
	const auto without_cv = [](string pattern) {
		const string tokens[] = {"const", "volatile"};
		for(size_t token = 0; token < 2; ++token)
			for(size_t position = pattern.find(tokens[token]); position != string::npos; ) {
				const bool left_boundary = position == 0 ||
					!IsIdentifierCharacter(pattern[position - 1]);
				const size_t end = position + tokens[token].size();
				const bool right_boundary = end == pattern.size() ||
					!IsIdentifierCharacter(pattern[end]);
				if(left_boundary && right_boundary) pattern.erase(position, tokens[token].size());
				else position = end;
				position = pattern.find(tokens[token], position);
			}
		return CanonicalSpelling(pattern);
	};
	const bool left_const = left[0].find("const") != string::npos;
	const bool left_volatile = left[0].find("volatile") != string::npos;
	const bool right_const = right[0].find("const") != string::npos;
	const bool right_volatile = right[0].find("volatile") != string::npos;
	if(left_const == right_const || left_volatile == right_volatile ||
		left_const == left_volatile || right_const == right_volatile)
		return false;
	return without_cv(left[0]) == without_cv(right[0]);
}

bool PA18TemplateExpander::FunctionTemplateMoreSpecialized(
	const TemplateDefinition& lhs, const TemplateDefinition& rhs,
	const string& context) const
{
	// `const T*` and `volatile T*` are symmetric partial-ordering patterns.
	// Neither can be selected over the other for a cv-qualified pointer, so
	// leave the tie visible to call ranking instead of treating cv as a
	// deduction-insensitive match.
	if(FunctionTemplateCvPointerTie(lhs, rhs)) return false;
	const auto parameter_patterns = [this](const TemplateDefinition& definition,
		vector<string>* result, set<string>* names) {
		if(!result || !names || !definition.declaration) return false;
		const CPPGMAstNodePtr parameters = DescendantOfKind(
			FunctionDeclarator(definition.declaration), "parameter-clause");
		if(!parameters) return false;
		for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter)
			if(!definition.parameters[parameter].name.empty())
				names->insert(definition.parameters[parameter].name);
		for(size_t parameter = 0; parameter < parameters->children.size(); ++parameter) {
			const CPPGMAstNodePtr item = parameters->children[parameter];
			if(!item || item->kind != "parameter-declaration") continue;
			string pattern = ParameterTypeSpelling(item);
			if(IsFunctionParameterPack(item) && pattern.size() >= 3 &&
				pattern.compare(pattern.size() - 3, 3, "...") == 0)
				pattern.erase(pattern.size() - 3);
			result->push_back(CanonicalSpelling(pattern));
		}
		return true;
	};
	const auto can_deduce = [this, &context, &parameter_patterns](
		const TemplateDefinition& pattern_definition,
		const TemplateDefinition& argument_definition) {
		vector<string> patterns, arguments;
		set<string> names;
		if(!parameter_patterns(pattern_definition, &patterns, &names) ||
			!parameter_patterns(argument_definition, &arguments, &names)) return false;
		map<string, string> placeholders;
		size_t placeholder = 0;
		for(size_t parameter = 0; parameter < argument_definition.parameters.size(); ++parameter) {
			const string& name = argument_definition.parameters[parameter].name;
			if(name.empty()) continue;
			ostringstream value;
			value << "__pa22_partial_" << placeholder++;
			placeholders[name] = value.str();
		}
		for(size_t argument = 0; argument < arguments.size(); ++argument)
			arguments[argument] = CanonicalSpelling(ReplaceIdentifiers(arguments[argument], placeholders));
		for(size_t parameter = 0; parameter < patterns.size(); ++parameter) {
			if(parameter >= arguments.size()) return false;
			map<string, string> ignored;
			if(!MatchTypePattern(patterns[parameter], arguments[parameter], names,
				&ignored, context)) return false;
		}
		return patterns.size() == arguments.size();
	};
	try {
		const bool rhs_from_lhs = can_deduce(rhs, lhs);
		const bool lhs_from_rhs = can_deduce(lhs, rhs);
		return rhs_from_lhs && !lhs_from_rhs;
	} catch(const PA18SubstitutionFailure&) {
		return false;
	}
}

bool PA18TemplateExpander::PreserveFunctionLookupOrder(
	const vector<const TemplateDefinition*>& definitions, const string& context,
	const map<string, string>& substitutions) const
{
	if(HasReplayContext(substitutions)) return false;
	map<string, CPPGMAstNodePtr>::const_iterator enclosing =
		function_definitions_.find(context);
	if(enclosing == function_definitions_.end() || !enclosing->second ||
		enclosing->second->source_token_begin == static_cast<size_t>(-1)) return false;
	const size_t visibility = enclosing->second->source_token_begin;
	for(size_t candidate = 0; candidate < definitions.size(); ++candidate) {
		const TemplateDefinition* definition = definitions[candidate];
		if(definition && definition->declaration &&
			definition->declaration->source_token_begin != static_cast<size_t>(-1) &&
			definition->declaration->source_token_begin > visibility) return true;
	}
	return false;
}

void PA18TemplateExpander::SortFunctionTemplateCandidates(
	vector<const TemplateDefinition*>* candidates, const string& context) const
{
	if(!candidates) return;
	stable_sort(candidates->begin(), candidates->end(),
		[this, &context](const TemplateDefinition* lhs, const TemplateDefinition* rhs) {
			if(!lhs || !rhs || lhs->parameters.empty() || rhs->parameters.empty()) return false;
			if(FunctionTemplateMoreSpecialized(*lhs, *rhs, context)) return true;
			if(FunctionTemplateMoreSpecialized(*rhs, *lhs, context)) return false;
			return false;
		});
}

void PA18TemplateExpander::RankFunctionTemplateCandidatesForCall(
	vector<const TemplateDefinition*>* candidates, const CPPGMAstNodePtr& call,
	const string& context, const map<string, string>& substitutions) const
{
	if(!candidates || !call) return;
	// Partial ordering does not carry the call's value category and top-level cv
	// through this AST boundary.  Rank otherwise-equivalent viable templates
	// with those typed facts before materialization.
	map<const TemplateDefinition*, bool> call_viable;
	map<const TemplateDefinition*, int> call_score;
	map<const TemplateDefinition*, int> call_non_dependent;
	map<const TemplateDefinition*, int> call_fixedness;
	const CPPGMAstNodePtr call_arguments = call->children.size() > 1 &&
		call->children[1] && call->children[1]->kind == "argument-list" ?
		call->children[1] : CPPGMAstNodePtr();
	for(size_t candidate = 0; candidate < candidates->size(); ++candidate) {
		const TemplateDefinition* definition = (*candidates)[candidate];
		vector<string> inferred;
		bool viable = false;
		try { viable = InferFunctionArguments(*definition, call, &inferred,
			substitutions, context, 0); }
		catch(const PA18SubstitutionFailure&) { viable = false; }
		call_viable[definition] = viable;
		if(!viable) { call_score[definition] = 1000000; continue; }
		int score = 0, non_dependent = 0, fixedness = 0;
		for(size_t parameter = 0; parameter < definition->parameters.size(); ++parameter)
			if(!definition->parameters[parameter].pack) ++fixedness;
		const CPPGMAstNodePtr parameter_clause = DescendantOfKind(
			FunctionDeclarator(definition->declaration), "parameter-clause");
		size_t argument_index = 0;
		if(parameter_clause && call_arguments) for(size_t parameter = 0;
			parameter < parameter_clause->children.size(); ++parameter) {
			const CPPGMAstNodePtr parameter_node = parameter_clause->children[parameter];
			if(!parameter_node || parameter_node->kind != "parameter-declaration") continue;
			const string pattern = ParameterTypeSpelling(parameter_node);
			bool dependent = false;
			for(size_t template_parameter = 0;
				template_parameter < definition->parameters.size(); ++template_parameter) {
				const string& name = definition->parameters[template_parameter].name;
				for(size_t position = pattern.find(name); !name.empty() &&
					position != string::npos; position = pattern.find(name,
					position + name.size())) {
					const bool left = position == 0 || !IsIdentifierCharacter(pattern[position - 1]);
					const size_t end = position + name.size();
					const bool right = end == pattern.size() || !IsIdentifierCharacter(pattern[end]);
					if(left && right) { dependent = true; break; }
				}
				if(dependent) break;
			}
			if(!dependent) ++non_dependent;
			const bool pack = IsFunctionParameterPack(parameter_node);
			size_t remaining = argument_index < call_arguments->children.size() ?
				call_arguments->children.size() - argument_index : 0;
			size_t visits = pack ? remaining : (remaining ? 1 : 0);
			if(pack) {
				size_t trailing_fixed = 0;
				for(size_t later = parameter + 1; later < parameter_clause->children.size(); ++later)
					if(parameter_clause->children[later] &&
						parameter_clause->children[later]->kind == "parameter-declaration" &&
						!IsFunctionParameterPack(parameter_clause->children[later])) ++trailing_fixed;
				visits = remaining < trailing_fixed ? 0 : remaining - trailing_fixed;
			}
			for(size_t visit = 0; visit < visits && argument_index < call_arguments->children.size(); ++visit) {
				string actual;
				if(!InferArgument(call_arguments->children[argument_index], &actual,
					substitutions, context)) { score += 100; ++argument_index; continue; }
				// A derived tag object is viable for a base-tag parameter, but the
				// exact tag overload wins ordinary overload resolution.  Preserve that
				// conversion rank before template partial-ordering compares otherwise
				// similar parameter packs.
				try {
					const string expected = const_cast<PA18TemplateExpander*>(this)->RewriteText(
						pattern, context, substitutions, 0);
					const string expected_object = FunctionArgumentObjectType(expected, context);
					const string actual_object = FunctionArgumentObjectType(actual, context);
					if(!expected_object.empty() && !actual_object.empty() &&
						expected_object != actual_object)
						score += HasClassConversion(expected_object, actual_object, context) ? 1 : 4;
				} catch(const PA18SubstitutionFailure&) {}
				const bool actual_const = CanonicalSpelling(actual).compare(0, 6, "const ") == 0;
				const bool reference = !pattern.empty() && pattern[pattern.size() - 1] == '&' &&
					(pattern.size() < 2 || pattern[pattern.size() - 2] != '&');
				const bool const_reference = reference && (pattern.find("const ") != string::npos ||
					pattern.find(" const") != string::npos);
				if(reference && const_reference != actual_const) ++score;
				++argument_index;
			}
		}
		call_score[definition] = score;
		call_non_dependent[definition] = non_dependent;
		call_fixedness[definition] = fixedness;
	}
	for(size_t left = 0; left < candidates->size(); ++left) {
		const TemplateDefinition* lhs = (*candidates)[left];
		if(!lhs || !call_viable[lhs]) continue;
		for(size_t right = left + 1; right < candidates->size(); ++right) {
			const TemplateDefinition* rhs = (*candidates)[right];
			if(!rhs || !call_viable[rhs] || !FunctionTemplateCvPointerTie(*lhs, *rhs)) continue;
			throw logic_error("ambiguous function template overload");
		}
	}
	stable_sort(candidates->begin(), candidates->end(),
		[this, &context, &call_viable, &call_score, &call_non_dependent, &call_fixedness](
			const TemplateDefinition* lhs, const TemplateDefinition* rhs) {
			if(call_viable[lhs] != call_viable[rhs]) return call_viable[lhs];
			if(call_score[lhs] != call_score[rhs]) return call_score[lhs] < call_score[rhs];
			const bool lhs_more = FunctionTemplateMoreSpecialized(*lhs, *rhs, context);
			const bool rhs_more = FunctionTemplateMoreSpecialized(*rhs, *lhs, context);
			if(lhs_more != rhs_more) return lhs_more;
			if(call_non_dependent[lhs] != call_non_dependent[rhs])
				return call_non_dependent[lhs] > call_non_dependent[rhs];
			if(call_fixedness[lhs] != call_fixedness[rhs])
				return call_fixedness[lhs] > call_fixedness[rhs];
			return false;
		});
}

int PA18TemplateExpander::MemberTemplatePatternScore(
	const TemplateDefinition* candidate) const
{
	if(!candidate || !candidate->declaration) return 0;
	const CPPGMAstNodePtr parameters = DescendantOfKind(
		FunctionDeclarator(candidate->declaration), "parameter-clause");
	if(!parameters) return 0;
	int score = 0;
	for(size_t parameter = 0; parameter < parameters->children.size(); ++parameter) {
		const CPPGMAstNodePtr item = parameters->children[parameter];
		if(!item || item->kind != "parameter-declaration") continue;
		const string pattern = CanonicalSpelling(ParameterTypeSpelling(item));
		if(pattern.find('<') != string::npos) score += 32;
		if(pattern.find('*') != string::npos) score += 8;
		if(pattern.find('&') != string::npos) score += 4;
		if(pattern.compare(0, 6, "const ") == 0 ||
			pattern.compare(0, 9, "volatile ") == 0) score += 2;
		for(size_t template_parameter = 0;
			template_parameter < candidate->parameters.size(); ++template_parameter)
			if(!candidate->parameters[template_parameter].name.empty() &&
				pattern == candidate->parameters[template_parameter].name) {
				score -= 16;
				break;
			}
	}
	return score;
}

void PA18TemplateExpander::RestoreMemberTemplateDefaults(
	const string& member_name, const TemplateDefinition& definition,
	TemplateDefinition* result) const
{
	if(!result) return;
	map<string, vector<string> >::const_iterator indexed =
		definitions_by_name_.find(member_name);
	if(indexed == definitions_by_name_.end()) return;
	const string definition_signature = MemberSignatureKey(definition);
	const string owner_qualifier = definition.owner + "::";
	for(size_t index = 0; index < indexed->second.size(); ++index) {
		map<string, TemplateDefinition>::const_iterator found = definitions_.find(
			indexed->second[index]);
		if(found == definitions_.end() || !found->second.declaration) continue;
		const TemplateDefinition& declaration = found->second;
		if(declaration.declaration->kind != "simple-declaration" &&
			declaration.declaration->kind != "special-member-declaration") continue;
		if(LastComponent(StripTemplateArgumentsForValidation(declaration.owner)) !=
			LastComponent(StripTemplateArgumentsForValidation(definition.owner))) continue;
		string declaration_signature = MemberSignatureKey(declaration);
		for(size_t qualifier = declaration_signature.find(owner_qualifier);
			qualifier != string::npos;
			qualifier = declaration_signature.find(owner_qualifier, qualifier + 1))
			declaration_signature.erase(qualifier, owner_qualifier.size());
		if(declaration_signature != definition_signature) continue;
		for(size_t parameter = 0; parameter < result->parameters.size() &&
			parameter < declaration.parameters.size(); ++parameter)
			if(result->parameters[parameter].default_type.empty())
				result->parameters[parameter].default_type =
					declaration.parameters[parameter].default_type;
		return;
	}
}

CPPGMAstNodePtr PA18TemplateExpander::FunctionParameterDefaultNode(
	const TemplateDefinition& definition, size_t parameter) const
{
	const CPPGMAstNodePtr own_declarator = FunctionDeclarator(definition.declaration);
	const CPPGMAstNodePtr own_clause = DescendantOfKind(own_declarator,
		"parameter-clause");
	if(own_clause && parameter < own_clause->children.size() &&
		ChildOfKindLocal(own_clause->children[parameter], "default-argument"))
		return ChildOfKindLocal(own_clause->children[parameter], "default-argument");
	// A free-function definition cannot inherit a default from an unrelated
	// declaration found by the name index.  The cross-declaration lookup is
	// only needed for out-of-class member definitions, where the declaration
	// and definition have the same owner and signature.
	if(definition.owner.empty()) return CPPGMAstNodePtr();
	const string member_name = LastComponent(definition.name);
	map<string, vector<string> >::const_iterator indexed = definitions_by_name_.find(member_name);
	if(indexed == definitions_by_name_.end()) return CPPGMAstNodePtr();
	const string definition_owner = LastComponent(StripTemplateArgumentsForValidation(
		definition.owner));
	const string definition_signature = MemberSignatureKey(definition);
	for(size_t index = 0; index < indexed->second.size(); ++index) {
		map<string, TemplateDefinition>::const_iterator found = definitions_.find(
			indexed->second[index]);
		if(found == definitions_.end() || !found->second.declaration ||
			&found->second == &definition) continue;
		const TemplateDefinition& candidate = found->second;
		if(candidate.declaration->kind != "simple-declaration" &&
			candidate.declaration->kind != "special-member-declaration") continue;
		const string candidate_owner = LastComponent(StripTemplateArgumentsForValidation(
			candidate.owner));
		if(candidate_owner != definition_owner ||
			MemberSignatureKey(candidate) != definition_signature) continue;
		const CPPGMAstNodePtr candidate_declarator = FunctionDeclarator(
			candidate.declaration);
		const CPPGMAstNodePtr candidate_clause = DescendantOfKind(candidate_declarator,
			"parameter-clause");
		if(candidate_clause && parameter < candidate_clause->children.size() &&
			ChildOfKindLocal(candidate_clause->children[parameter], "default-argument"))
			return ChildOfKindLocal(candidate_clause->children[parameter], "default-argument");
	}
	return CPPGMAstNodePtr();
}

bool PA18TemplateExpander::FunctionParameterHasDefault(
	const TemplateDefinition& definition, size_t parameter) const
{
	return static_cast<bool>(FunctionParameterDefaultNode(definition, parameter));
}

bool PA18TemplateExpander::RestoreFunctionParameterDefaults(
	const TemplateDefinition& definition, TemplateDefinition* result) const
{
	if(!result || !result->declaration) return false;
	const CPPGMAstNodePtr source_clause = DescendantOfKind(
		FunctionDeclarator(result->declaration), "parameter-clause");
	if(!source_clause) return false;
	vector<CPPGMAstNodePtr> defaults(source_clause->children.size());
	bool restored = false;
	for(size_t parameter = 0; parameter < source_clause->children.size(); ++parameter) {
		const CPPGMAstNodePtr target = source_clause->children[parameter];
		if(!target || target->kind != "parameter-declaration" ||
			ChildOfKindLocal(target, "default-argument")) continue;
		const CPPGMAstNodePtr source = FunctionParameterDefaultNode(definition, parameter);
		if(source) { defaults[parameter] = source; restored = true; }
	}
	if(!restored) return false;
	result->declaration = CloneNode(result->declaration);
	const CPPGMAstNodePtr target_clause = DescendantOfKind(
		FunctionDeclarator(result->declaration), "parameter-clause");
	if(!target_clause) return false;
	for(size_t parameter = 0; parameter < defaults.size() &&
		parameter < target_clause->children.size(); ++parameter)
		if(defaults[parameter])
			target_clause->children[parameter]->children.push_back(CloneNode(defaults[parameter]));
	return true;
}

bool PA18TemplateExpander::IsAbstractClassType(const string& raw,
	const string& context, set<string>* active) const
{
	string name = CanonicalSpelling(raw);
	while(name.compare(0, 6, "const ") == 0 ||
		name.compare(0, 9, "volatile ") == 0)
		name = CanonicalSpelling(name.substr(name.find(' ') + 1));
	while(name.size() > 6 && name.compare(name.size() - 6, 6, " const") == 0)
		name = CanonicalSpelling(name.substr(0, name.size() - 6));
	while(name.size() > 9 && name.compare(name.size() - 9, 9, " volatile") == 0)
		name = CanonicalSpelling(name.substr(0, name.size() - 9));
	while(!name.empty() && (name[name.size() - 1] == '&' ||
		name[name.size() - 1] == '*')) name.erase(name.size() - 1);
	name = CanonicalSpelling(name);
	while(name.compare(0, 6, "const ") == 0 ||
		name.compare(0, 9, "volatile ") == 0)
		name = CanonicalSpelling(name.substr(name.find(' ') + 1));
	if(name.compare(0, 7, "struct ") == 0)
		name = CanonicalSpelling(name.substr(7));
	else if(name.compare(0, 6, "class ") == 0)
		name = CanonicalSpelling(name.substr(6));
	else if(name.compare(0, 6, "union ") == 0)
		name = CanonicalSpelling(name.substr(6));
	if(name.empty() || name.find('<') != string::npos || !active ||
		!active->insert(name).second) return false;
	const CPPGMAstNodePtr declaration = FindClassDeclaration(name, context);
	if(!declaration) {
		active->erase(name);
		return false;
	}
	for(size_t child = 0; child < declaration->children.size(); ++child) {
		const CPPGMAstNodePtr member = declaration->children[child];
		if(!member || member->kind != "simple-declaration" ||
			member->children.empty() ||
			!HasDeclarationSpecifier(member->children[0], "virtual")) continue;
		const CPPGMAstNodePtr initializer = DescendantOfKind(member, "initializer");
		if(initializer && !initializer->children.empty() && initializer->children[0] &&
			Trim(RemoveMarker(initializer->children[0]->value)) == "0") {
			active->erase(name);
			return true;
		}
	}
	for(size_t child = 0; child < declaration->children.size(); ++child) {
		const CPPGMAstNodePtr base_clause = declaration->children[child];
		if(!base_clause || base_clause->kind != "base-clause") continue;
		for(size_t base = 0; base < base_clause->children.size(); ++base) {
			const CPPGMAstNodePtr base_name = ChildOfKindLocal(
				base_clause->children[base], "base-name");
			if(base_name && IsAbstractClassType(base_name->value, context, active)) {
				active->erase(name);
				return true;
			}
		}
	}
	active->erase(name);
	return false;
}

bool PA18TemplateExpander::HasAbstractFunctionParameter(
	const TemplateDefinition& definition, const vector<string>& arguments,
	const string& context, const map<string, string>& substitutions)
{
	map<string, string> bindings = substitutions;
	for(size_t parameter = 0; parameter < definition.parameters.size() &&
		parameter < arguments.size(); ++parameter)
		if(!definition.parameters[parameter].name.empty())
			bindings[definition.parameters[parameter].name] = arguments[parameter];
	const CPPGMAstNodePtr parameter_clause = DescendantOfKind(
		FunctionDeclarator(definition.declaration), "parameter-clause");
	if(!parameter_clause) return false;
	for(size_t parameter = 0; parameter < parameter_clause->children.size(); ++parameter) {
		const CPPGMAstNodePtr parameter_node = parameter_clause->children[parameter];
		if(!parameter_node || parameter_node->kind != "parameter-declaration") continue;
		string resolved;
		try {
			resolved = NormalizeTypeArgument(ReplaceIdentifiers(
				ParameterTypeSpelling(parameter_node), bindings));
			resolved = NormalizeTypeArgument(RewriteText(resolved, context,
				bindings, 0));
		} catch(const PA18SubstitutionFailure&) {
			return true;
		}
		if(IsAbstractObjectSpelling(resolved, context)) return true;
	}
	return false;
}

bool PA18TemplateExpander::IsAbstractObjectSpelling(
	const string& raw, const string& context) const
{
	string spelling = CanonicalSpelling(raw);
	if(spelling.find('<') != string::npos) return false;
	const bool object_array = spelling.find('[') != string::npos;
	const bool indirection = spelling.find('*') != string::npos ||
		spelling.find('&') != string::npos;
	while(!spelling.empty() && spelling[spelling.size() - 1] == ']') {
		const size_t open = spelling.rfind('[');
		if(open == string::npos) break;
		spelling.erase(open);
	}
	// An array parameter written as `T (*)[N]` leaves the abstract pointer
	// declarator parenthesized after its bound is removed.  Strip that syntax
	// before the typed class lookup; it is not part of the object type.
	const size_t abstract_pointer = spelling.find("(");
	if(abstract_pointer != string::npos)
		spelling = CanonicalSpelling(spelling.substr(0, abstract_pointer));
	set<string> active;
	return (object_array || !indirection) &&
		IsAbstractClassType(spelling, context, &active);
}

bool PA18TemplateExpander::ValidateExplicitFunctionCandidate(
	const TemplateDefinition& definition, const CPPGMAstNodePtr& input,
	const string& context, const map<string, string>& substitutions,
	const vector<string>& raw_explicit_args, vector<string>* arguments)
{
	if(!arguments) return false;
	try {
		if(!InferFunctionArguments(definition, input, arguments, substitutions, context,
			&raw_explicit_args)) return false;
		map<string, string> bindings;
		for(size_t parameter = 0; parameter < definition.parameters.size() &&
			parameter < arguments->size(); ++parameter)
			if(!definition.parameters[parameter].name.empty())
				bindings[definition.parameters[parameter].name] = (*arguments)[parameter];
		for(size_t parameter = raw_explicit_args.size();
			parameter < definition.parameters.size(); ++parameter) {
			const TemplateParameter& detail = definition.parameters[parameter];
			if(detail.default_type.empty() || parameter >= arguments->size()) continue;
			string value;
			const string default_type = CanonicalSpelling(detail.default_type);
			if(default_type.compare(0, 9, "decltype(") == 0 &&
				default_type.size() > 10 && default_type[default_type.size() - 1] == ')') {
				const string expression = default_type.substr(9, default_type.size() - 10);
				if(!EvaluateDecltypeExpression(expression, context, bindings, &value)) return false;
			} else value = RewriteText(detail.default_type, context, bindings, 0);
			if(value.empty()) return false;
			(*arguments)[parameter] = NormalizeTypeArgument(value);
			if(!detail.name.empty()) bindings[detail.name] = (*arguments)[parameter];
		}
		if(!raw_explicit_args.empty() &&
			!ValidateTemplateDefaults(definition, *arguments, context, substitutions)) return false;
		try {
			const string result = FunctionResultType(definition, *arguments, context, &substitutions);
			string probe = result;
			while(!probe.empty() && (probe[probe.size() - 1] == '*' || probe[probe.size() - 1] == '&')) probe.erase(probe.size() - 1);
			while(probe.size() > 6 && probe.compare(probe.size() - 6, 6, " const") == 0) probe.erase(probe.size() - 6);
			if(HasUnavailableGeneratedMemberType(probe, context, substitutions)) return false;
			return !result.empty();
		} catch(const PA18SubstitutionFailure&) { return false; }
	} catch(const PA18SubstitutionFailure&) {
		return false;
	}
}

} // namespace pa18_templates_internal
