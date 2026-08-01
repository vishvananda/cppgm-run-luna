#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

#include <cctype>
#include <functional>
#include <sstream>


using namespace std;

namespace pa18_templates_internal {

namespace {

string StringLiteralArrayReferenceType(const string& value)
{
	const size_t quote = value.find('"');
	if(quote == string::npos) return string();
	string element = "char";
	if(quote > 0) {
		const string prefix = value.substr(0, quote);
		if(prefix == "L") element = "wchar_t";
		else if(prefix == "u") element = "char16_t";
		else if(prefix == "U") element = "char32_t";
	}
	const size_t close = value.rfind('"');
	if(close == string::npos || close <= quote) return string();
	// Count decoded characters rather than source bytes so ordinary escapes do
	// not produce a false array bound.  The null terminator is part of the
	// language-defined array type.  The full literal decoder runs later in the
	// lowering pipeline; deduction only needs the element count here.
	size_t elements = 1;
	for(size_t i = quote + 1; i < close; ++i) {
		if(value[i] != '\\') {
			++elements;
			continue;
		}
		if(++i >= close) break;
		if(value[i] == 'u' || value[i] == 'U') {
			const size_t digits = value[i] == 'u' ? 4 : 8;
			i = min(close, i + digits);
			++elements;
		} else if(value[i] == 'x') {
			while(i + 1 < close && isxdigit(static_cast<unsigned char>(value[i + 1]))) ++i;
			++elements;
		} else if(value[i] >= '0' && value[i] <= '7') {
			for(size_t digit = 1; digit < 3 && i + 1 < close &&
				value[i + 1] >= '0' && value[i + 1] <= '7'; ++digit) ++i;
			++elements;
		} else ++elements;
	}
	ostringstream bound;
	bound << elements;
	return "const " + element + "(&)[" + bound.str() + "]";
}

bool ReferenceParameterPattern(const string& pattern)
{
	return (!pattern.empty() && pattern[pattern.size() - 1] == '&') ||
		pattern.find("(&") != string::npos || pattern.find("(& ") != string::npos;
}

bool ConstReferenceParameterPattern(const string& pattern)
{
	for(size_t position = pattern.find("const"); position != string::npos;
		position = pattern.find("const", position + 5)) {
		const bool left = position == 0 || !IsIdentifierCharacter(pattern[position - 1]);
		const size_t end = position + 5;
		const bool right = end == pattern.size() || !IsIdentifierCharacter(pattern[end]);
		if(left && right) return true;
	}
	return false;
}

} // namespace

bool IsLvalueTemplateArgument(const CPPGMAstNodePtr& expression)
{
	if(!expression) return false;
	if(expression->kind == "id-expression" || expression->kind == "member-expression" ||
		expression->kind == "subscript-expression" || expression->kind == "keyword-literal")
		return true;
	if(expression->kind == "parenthesized-expression" && !expression->children.empty())
		return IsLvalueTemplateArgument(expression->children[0]);
	if(expression->kind == "unary-expression" && !expression->children.empty())
		return RemoveMarker(expression->value) == "*" ||
			RemoveMarker(expression->value) == "++" ||
			RemoveMarker(expression->value) == "--";
	if(expression->kind == "binary-expression" && expression->children.size() >= 2 &&
		RemoveMarker(expression->value) == ",")
		return IsLvalueTemplateArgument(expression->children[1]);
	if(expression->kind == "conditional-expression" && expression->children.size() >= 3)
		return IsLvalueTemplateArgument(expression->children[1]) &&
			IsLvalueTemplateArgument(expression->children[2]);
	return false;
}

bool PA18TemplateExpander::MergeInferredFunctionArgument(
	const TemplateDefinition& definition, const string& pattern, const string& type,
	const FunctionSignature& signature, const map<string, string>& substitutions,
	const string& context, const set<string>& parameter_names,
	map<string, string>* inferred, map<string, vector<string> >* inferred_packs,
	map<string, FunctionSignature>* inferred_functions,
	const map<string, vector<string> >* bound_pack_values,
	const set<string>* fixed_template_parameters) const
{
	map<string, string> one;
	string match_pattern = pattern;
	map<string, string> pattern_substitutions;
	bool alias_substitution = false;
	bool dependent_substitution = false;
	for(map<string, string>::const_iterator substitution = substitutions.begin();
		substitution != substitutions.end(); ++substitution)
		if(parameter_names.find(substitution->first) == parameter_names.end()) {
			pattern_substitutions[substitution->first] = substitution->second;
			for(size_t position = pattern.find(substitution->first);
				position != string::npos;
				position = pattern.find(substitution->first,
					position + substitution->first.size())) {
				const size_t end = position + substitution->first.size();
				const bool left = position == 0 ||
					!IsIdentifierCharacter(pattern[position - 1]);
				const bool right = end == pattern.size() ||
					!IsIdentifierCharacter(pattern[end]);
				if(!left || !right) continue;
				if(!FindDefinition(substitution->first, context) &&
					class_contexts_.find(substitution->first) == class_contexts_.end())
					dependent_substitution = true;
				if(type_aliases_by_name_.find(substitution->first) !=
					type_aliases_by_name_.end()) alias_substitution = true;
				break;
			}
		}
	for(map<string, string>::const_iterator value = inferred->begin();
		value != inferred->end(); ++value)
		pattern_substitutions[value->first] = value->second;
	bool rewrite_inferred = !inferred->empty();
	const auto template_shape = [this](string raw) -> pair<string, size_t> {
		raw = CanonicalSpelling(raw);
		while(raw.compare(0, 6, "const ") == 0 || raw.compare(0, 9, "volatile ") == 0)
			raw = CanonicalSpelling(raw.substr(raw.find(' ') + 1));
		while(!raw.empty() && (raw[raw.size() - 1] == '&' || raw[raw.size() - 1] == '*'))
			raw.erase(raw.size() - 1);
		const size_t open = raw.find('<');
		if(open == string::npos || raw.rfind('>') == string::npos)
			return make_pair(string(), static_cast<size_t>(0));
		return make_pair(LastComponent(raw.substr(0, open)),
			SplitTemplateArguments(raw.substr(open + 1, raw.rfind('>') - open - 1)).size());
	};
	if(rewrite_inferred && pattern.find('<') != string::npos && type.find('<') != string::npos) {
		const pair<string, size_t> pattern_shape = template_shape(pattern);
		const pair<string, size_t> actual_shape = template_shape(type);
		if(!pattern_shape.first.empty() && pattern_shape == actual_shape &&
			pattern.find("...") == string::npos) rewrite_inferred = false;
	}
	bool bound_pack_in_pattern = false;
	if(bound_pack_values) for(map<string, vector<string> >::const_iterator bound =
		bound_pack_values->begin(); bound != bound_pack_values->end() && !bound_pack_in_pattern;
		++bound) {
		if(bound->first.empty()) continue;
		for(size_t position = pattern.find(bound->first); position != string::npos;
			position = pattern.find(bound->first, position + bound->first.size())) {
			const bool left = position == 0 || !IsIdentifierCharacter(pattern[position - 1]);
			const size_t end = position + bound->first.size();
			const bool right = end == pattern.size() || !IsIdentifierCharacter(pattern[end]);
			if(left && right) { bound_pack_in_pattern = true; break; }
		}
	}
	const bool rewrite_pattern = !pattern_substitutions.empty() &&
		(alias_substitution || (rewrite_inferred && pattern.find("::") != string::npos) ||
		bound_pack_in_pattern || dependent_substitution);
	if(rewrite_pattern) {
		match_pattern = const_cast<PA18TemplateExpander*>(this)->RewriteText(
			pattern, context, pattern_substitutions, 0);
		match_pattern = NormalizeTypeArgument(ReplaceIdentifiers(match_pattern,
			pattern_substitutions));
		match_pattern = ResolveAlias(match_pattern, context);
		// RewriteText may materialize a dependent class pattern while resolving
		// enclosing bindings. Restore that generated spelling before deduction so
		// a shadowing member parameter (for example `Y` in `shared_ptr<Y,A,D>`)
		// remains a deducible formal rather than a concrete class name.
		match_pattern = NormalizeTypeArgument(RestoreSpecializationSpelling(match_pattern));
		// A fixed enclosing-class binding can itself be an lvalue reference.  When
		// that binding fills a forwarding-reference parameter, textual substitution
		// produces `T& &&`; collapse the two reference layers before matching the
		// already typed call argument.
		bool fixed_lvalue_reference = false;
		for(map<string, string>::const_iterator substitution = pattern_substitutions.begin();
			substitution != pattern_substitutions.end(); ++substitution)
			if(!substitution->second.empty() &&
				substitution->second[substitution->second.size() - 1] == '&') {
				fixed_lvalue_reference = true;
				break;
			}
		if(fixed_lvalue_reference && match_pattern.size() >= 2 &&
			match_pattern.compare(match_pattern.size() - 2, 2, "&&") == 0)
				match_pattern.erase(match_pattern.size() - 1);
	}
	// A named function passed to a function-reference parameter does not decay
	// during template deduction: `F&` deduces `F` as the function type, while
	// the same expression passed by value deduces a function pointer.  The
	// identifier lookup fact is intentionally pointer-shaped for ordinary calls,
	// so recover the direct function spelling only at this reference boundary.
	string deduction_type = type;
	if(ReferenceParameterPattern(pattern) && signature.result_specifiers &&
		signature.parameters) {
		deduction_type = NodeTypeSpelling(signature.result_specifiers) + "(";
		for(size_t parameter = 0; parameter < signature.parameters->children.size(); ++parameter) {
			const CPPGMAstNodePtr item = signature.parameters->children[parameter];
			if(!item || item->kind != "parameter-declaration") continue;
			if(deduction_type[deduction_type.size() - 1] != '(') deduction_type += ',';
			const bool function_parameter = item->children.size() > 1 && item->children[1] &&
				ChildOfKindLocal(item->children[1], "nested-declarator") &&
				ChildOfKindLocal(item->children[1], "parameter-clause");
			deduction_type += function_parameter ? FunctionTypeSpelling(item) :
				ParameterTypeSpelling(item);
		}
		deduction_type += ')';
		deduction_type = CanonicalSpelling(deduction_type);
	}
	set<string> matching_parameter_names = parameter_names;
	string matching_pattern = match_pattern;
	for(map<string, string>::const_iterator binding = inferred->begin();
		binding != inferred->end(); ++binding) {
		if(!fixed_template_parameters || fixed_template_parameters->find(binding->first) ==
			fixed_template_parameters->end()) continue;
		bool present = false;
		for(size_t at = matching_pattern.find(binding->first); at != string::npos;
			at = matching_pattern.find(binding->first, at + binding->first.size())) {
			const bool left = at == 0 || !IsIdentifierCharacter(matching_pattern[at - 1]);
			const size_t end = at + binding->first.size();
			const bool right = end == matching_pattern.size() ||
				!IsIdentifierCharacter(matching_pattern[end]);
			if(left && right) { present = true; break; }
		}
		if(present) {
			map<string, string> fixed_binding;
			fixed_binding[binding->first] = binding->second;
			matching_pattern = ReplaceIdentifiers(matching_pattern, fixed_binding);
			matching_parameter_names.erase(binding->first);
		}
	}
	// Function-template deduction sees the complete specialization represented
	// by an object expression.  The compact type fact may omit class-template
	// defaults (`queue<int>`), while a parameter pattern can mention the
	// defaulted tail (`queue<T, Container>`).  Complete that typed fact at this
	// deduction boundary so prefix deduction remains structural.
	const auto complete_class_template_type = [this, &context](string spelling) {
		string suffix;
		while(!spelling.empty() && (spelling[spelling.size() - 1] == '&' ||
			spelling[spelling.size() - 1] == '*')) {
			suffix = spelling[spelling.size() - 1] + suffix;
			spelling.erase(spelling.size() - 1);
		}
		const size_t open = spelling.find('<');
		if(open == string::npos) return spelling + suffix;
		string argument_text;
		size_t close = string::npos;
		if(!TemplateRange(spelling, open, &argument_text, &close)) return spelling + suffix;
		const string base = CanonicalSpelling(spelling.substr(0, open));
		const TemplateDefinition* class_definition = FindDefinition(base, context);
		if(!class_definition || !class_definition->class_template) return spelling + suffix;
		vector<string> arguments = SplitTemplateArguments(argument_text);
		if(arguments.size() >= class_definition->parameters.size()) return spelling + suffix;
		map<string, string> bindings;
		for(size_t parameter = 0; parameter < arguments.size() &&
			parameter < class_definition->parameters.size(); ++parameter)
			if(!class_definition->parameters[parameter].name.empty())
				bindings[class_definition->parameters[parameter].name] = arguments[parameter];
		for(size_t parameter = arguments.size(); parameter < class_definition->parameters.size(); ++parameter) {
			const TemplateParameter& detail = class_definition->parameters[parameter];
			if(detail.pack || detail.default_type.empty()) return spelling + suffix;
			// This is only completion of the typed class specialization used for
			// deduction.  Re-running the full source rewriter here can materialize
			// a generated class name (`Alloc_Info_ptr_`) instead of preserving the
			// default's structural template spelling, which makes the subsequent
			// class-pattern comparison fail.  Identifier substitution plus alias
			// resolution is sufficient at this boundary.
			string value = NormalizeTypeArgument(ResolveAlias(
				ReplaceIdentifiers(detail.default_type, bindings), context));
			if(value.empty()) return spelling + suffix;
			arguments.push_back(value);
			if(!detail.name.empty()) bindings[detail.name] = value;
		}
		string result = base + "<";
		for(size_t argument = 0; argument < arguments.size(); ++argument)
			result += (argument ? "," : string()) + arguments[argument];
		return result + ">" + suffix;
	};
	if(pattern.find("...") != string::npos) {
		deduction_type = NormalizeTypeArgument(
			RestoreSpecializationSpelling(deduction_type));
	}
	// A member alias template can hide the actual deducible class pattern. For
	// example, `matcher<B>` may alias `matcher_impl<T,B>` while the call only
	// exposes the concrete owner specialization. Expand that alias with the
	// enclosing bindings before matching its member template parameters.
	if(definition.member_template && matching_pattern.find('<') != string::npos) {
		const size_t open = matching_pattern.find('<');
		string alias_arguments;
		size_t close = string::npos;
		if(TemplateRange(matching_pattern, open, &alias_arguments, &close)) {
			const string alias_base = CanonicalSpelling(matching_pattern.substr(0, open));
			const string member_owner = StripTemplateArgumentsForValidation(definition.owner);
			const auto owner_matches = [this, &member_owner](const string& raw_owner) {
				const string owner = StripTemplateArgumentsForValidation(raw_owner);
				if(owner.empty() || member_owner.empty()) return false;
				if(owner == member_owner) return true;
				return owner == member_owner + "::" + LastComponent(member_owner) ||
					member_owner == owner + "::" + LastComponent(owner);
			};
			const TemplateDefinition* alias_definition = FindDefinition(alias_base, context);
			// A typed non-alias result (for example the namespace class template
			// `detail::matcher`) is already the intended pattern.  Only recover a
			// member alias when normal lookup found no entity, or found an alias
			// whose owner is provably unrelated.
			if(alias_definition && alias_definition->alias_template &&
				!alias_definition->owner.empty() && !owner_matches(alias_definition->owner))
				alias_definition = 0;
			if(!alias_definition) {
				map<string, vector<string> >::const_iterator indexed =
					definitions_by_name_.find(LastComponent(alias_base));
				if(indexed != definitions_by_name_.end() && !member_owner.empty())
					for(size_t candidate = 0; candidate < indexed->second.size(); ++candidate) {
						map<string, TemplateDefinition>::const_iterator found =
							definitions_.find(indexed->second[candidate]);
						if(found == definitions_.end() || !found->second.alias_template ||
							!owner_matches(found->second.owner)) continue;
						if(alias_definition && alias_definition != &found->second) {
							alias_definition = 0;
							break;
						}
						if(!alias_definition) {
							alias_definition = &found->second;
						}
					}
			}
			if(alias_definition && alias_definition->alias_template &&
				alias_definition->declaration && !alias_definition->declaration->children.empty()) {
				map<string, string> alias_substitutions = substitutions;
				const vector<string> alias_parts = SplitTemplateArguments(alias_arguments);
				for(size_t parameter = 0;
					parameter < alias_definition->parameters.size() &&
					parameter < alias_parts.size(); ++parameter)
					if(!alias_definition->parameters[parameter].name.empty())
						alias_substitutions[alias_definition->parameters[parameter].name] =
							alias_parts[parameter];
				string alias_target = TypeIdSpelling(
					alias_definition->declaration->children[0]);
				alias_target = ReplaceIdentifiersPreservingPackSizes(
					alias_target, alias_substitutions);
				alias_target += matching_pattern.substr(close + 1);
				matching_pattern = NormalizeTypeArgument(alias_target);
			}
		}
	}
	if(definition.member_template) try {
		matching_pattern = NormalizeTypeArgument(
			ResolveAlias(matching_pattern, context));
		const string size_type = ResolveAlias("size_t", context);
		if(!size_type.empty() && size_type != "size_t")
			matching_pattern = ReplaceIdentifiers(matching_pattern,
				map<string, string>{{"size_t", size_type}});
	} catch(const PA18SubstitutionFailure&) {}
	const string completed_type = complete_class_template_type(deduction_type);
	const bool matched = MatchTypePattern(matching_pattern, completed_type,
		matching_parameter_names, &one, context);
	if(!matched) {
		const bool lvalue = match_pattern.size() > 0 && match_pattern[match_pattern.size() - 1] == '&' &&
			(match_pattern.size() < 2 || match_pattern[match_pattern.size() - 2] != '&');
		const bool rvalue = match_pattern.size() > 1 &&
			match_pattern.compare(match_pattern.size() - 2, 2, "&&") == 0;
		if(lvalue && match_pattern.find("const") == string::npos) return false;
		if(rvalue) {
			const string target = CanonicalSpelling(match_pattern.substr(0, match_pattern.size() - 2));
			string actual_type = CanonicalSpelling(type);
			while(!actual_type.empty() && actual_type[actual_type.size() - 1] == '&')
				actual_type = CanonicalSpelling(actual_type.substr(0, actual_type.size() - 1));
			if(!IsBuiltinArithmeticType(actual_type) ||
				!IsBuiltinArithmeticType(ReplaceIdentifiers(target, *inferred))) return false;
		}
		for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter)
			if(definition.parameters[parameter].name == pattern) return false;
		string fixed_pattern_source = match_pattern;
		map<string, string> fixed_substitutions;
		if(fixed_template_parameters) for(map<string, string>::const_iterator binding =
			inferred->begin(); binding != inferred->end(); ++binding)
			if(fixed_template_parameters->find(binding->first) != fixed_template_parameters->end())
				fixed_substitutions[binding->first] = binding->second;
		if(!fixed_substitutions.empty()) {
			const string substituted_pattern = NormalizeTypeArgument(ReplaceIdentifiers(
				match_pattern, fixed_substitutions));
			if(!HasUnresolvedTemplateParameter(substituted_pattern, context, fixed_substitutions))
				fixed_pattern_source = substituted_pattern;
		}
		string fixed_pattern = CanonicalSpelling(fixed_pattern_source);
		string fixed_actual = CanonicalSpelling(type);
		while(fixed_pattern.compare(0, 6, "const ") == 0)
			fixed_pattern = CanonicalSpelling(fixed_pattern.substr(6));
		while(fixed_actual.compare(0, 6, "const ") == 0)
			fixed_actual = CanonicalSpelling(fixed_actual.substr(6));
		while(fixed_pattern.size() > 6 && fixed_pattern.compare(fixed_pattern.size() - 6, 6, " const") == 0)
			fixed_pattern = CanonicalSpelling(fixed_pattern.substr(0, fixed_pattern.size() - 6));
		while(fixed_actual.size() > 6 && fixed_actual.compare(fixed_actual.size() - 6, 6, " const") == 0)
			fixed_actual = CanonicalSpelling(fixed_actual.substr(0, fixed_actual.size() - 6));
		if((fixed_pattern.find('*') != string::npos) != (fixed_actual.find('*') != string::npos)) return false;
		while(!fixed_pattern.empty() && (fixed_pattern[fixed_pattern.size() - 1] == '&' ||
			fixed_pattern[fixed_pattern.size() - 1] == '*')) fixed_pattern.erase(fixed_pattern.size() - 1);
		while(!fixed_actual.empty() && (fixed_actual[fixed_actual.size() - 1] == '&' ||
			fixed_actual[fixed_actual.size() - 1] == '*')) fixed_actual.erase(fixed_actual.size() - 1);
		while(fixed_pattern.size() > 6 && fixed_pattern.compare(fixed_pattern.size() - 6, 6, " const") == 0)
			fixed_pattern = CanonicalSpelling(fixed_pattern.substr(0, fixed_pattern.size() - 6));
		while(fixed_actual.size() > 6 && fixed_actual.compare(fixed_actual.size() - 6, 6, " const") == 0)
			fixed_actual = CanonicalSpelling(fixed_actual.substr(0, fixed_actual.size() - 6));
		// A nondependent parameter still participates in candidate viability.  Do
		// not admit every pair of known class types here: that turns an unrelated
		// overload into a successful deduction and can recursively re-enter the
		// same template through the generated call expression.  Reuse the typed
		// argument-compatibility check, which retains arithmetic conversions while
		// rejecting unrelated complete class objects.
		if(!HasUnresolvedTemplateParameter(fixed_pattern_source, context, fixed_substitutions))
			return FunctionArgumentViable(fixed_pattern_source, type, context);
		return !(FindClassDeclaration(fixed_pattern, context) &&
			FindClassDeclaration(fixed_actual, context) &&
			LastComponent(fixed_pattern) != LastComponent(fixed_actual));
	}
	if(inferred_functions && signature.result_specifiers && signature.parameters)
		for(map<string, string>::const_iterator binding = one.begin(); binding != one.end(); ++binding)
				if(binding->second == deduction_type || binding->first == pattern)
				(*inferred_functions)[binding->first] = signature;
	for(map<string, string>::const_iterator it = one.begin(); it != one.end(); ++it) {
		const size_t template_index = find_if(definition.parameters.begin(), definition.parameters.end(),
			[&](const TemplateParameter& candidate) { return candidate.name == it->first; }) -
			definition.parameters.begin();
		if(template_index < definition.parameters.size() && definition.parameters[template_index].pack) {
			string packed = it->second;
			if(packed.size() > 3 && packed.compare(packed.size() - 3, 3, "...") == 0) {
				const string prefix = CanonicalSpelling(packed.substr(0, packed.size() - 3));
				const bool dependent_pack = !prefix.empty() &&
					prefix.find_first_not_of(
						"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") == string::npos &&
					template_pack_names_.find(prefix) != template_pack_names_.end();
				if(!dependent_pack) packed = prefix;
			}
			const vector<string> values = SplitTemplateArguments(packed);
			if(!packed.empty())
				(*inferred_packs)[it->first].insert((*inferred_packs)[it->first].end(),
					values.begin(), values.end());
		} else {
			map<string, string>::const_iterator prior = inferred->find(it->first);
			if(prior != inferred->end() && CanonicalSpelling(ResolveAlias(prior->second, context)) !=
				CanonicalSpelling(ResolveAlias(it->second, context))) return false;
			(*inferred)[it->first] = it->second;
		}
	}
	return true;
}

bool PA18TemplateExpander::InferFunctionParameter(
	const TemplateDefinition& definition, const CPPGMAstNodePtr& parameter,
	const CPPGMAstNodePtr& parameter_list, size_t parameter_position,
	const CPPGMAstNodePtr& arguments, size_t* argument_index,
	const map<string, string>& substitutions, const string& context,
	const set<string>& parameter_names, map<string, string>* inferred,
	map<string, vector<string> >* inferred_packs, vector<string>* deferred_patterns,
	vector<CPPGMAstNodePtr>* deferred_arguments,
	map<string, FunctionSignature>* inferred_functions,
	const map<string, vector<string> >* bound_pack_values,
	map<string, vector<string> >* forwarding_pack_values,
	const set<string>* fixed_template_parameters) const
{
	if(!parameter || parameter->kind != "parameter-declaration" || !argument_index) return true;
	const string pattern = parameter->children.size() > 1 && parameter->children[1] &&
		ChildOfKindLocal(parameter->children[1], "nested-declarator") &&
		ChildOfKindLocal(parameter->children[1], "parameter-clause") ?
		FunctionTypeSpelling(parameter) : ParameterTypeSpelling(parameter);
	string deduction_pattern = pattern;
	bool dependent_parameter_pattern = false;
	for(size_t template_parameter = 0; template_parameter < definition.parameters.size();
		++template_parameter) {
		const string& name = definition.parameters[template_parameter].name;
		if(name.empty()) continue;
		for(size_t at = deduction_pattern.find(name); at != string::npos;
			at = deduction_pattern.find(name, at + name.size())) {
			const bool left = at == 0 || !IsIdentifierCharacter(deduction_pattern[at - 1]);
			const size_t end = at + name.size();
			const bool right = end == deduction_pattern.size() ||
				!IsIdentifierCharacter(deduction_pattern[end]);
			if(left && right) { dependent_parameter_pattern = true; break; }
		}
		if(dependent_parameter_pattern) break;
	}
	if(!dependent_parameter_pattern) {
		const string resolved_parameter_pattern = NormalizeTypeArgument(ResolveAlias(
			deduction_pattern, context));
		if(!resolved_parameter_pattern.empty()) deduction_pattern = resolved_parameter_pattern;
	}
	const bool pack_parameter = IsFunctionParameterPack(parameter);
	const vector<string>* explicit_pack_values = 0;
	string explicit_pack_name;
	if(pack_parameter && inferred_packs) for(size_t template_parameter = 0;
		template_parameter < definition.parameters.size(); ++template_parameter) {
		const TemplateParameter& candidate = definition.parameters[template_parameter];
		if(!candidate.pack || candidate.name.empty()) continue;
		for(size_t position = pattern.find(candidate.name); position != string::npos;
			position = pattern.find(candidate.name, position + candidate.name.size())) {
			const bool left = position == 0 || !IsIdentifierCharacter(pattern[position - 1]);
			const size_t end = position + candidate.name.size();
			const bool right = end == pattern.size() || !IsIdentifierCharacter(pattern[end]);
			if(left && right) {
				map<string, vector<string> >::const_iterator values = inferred_packs->find(candidate.name);
				if(values != inferred_packs->end()) {
					explicit_pack_name = candidate.name;
					explicit_pack_values = &values->second;
				}
				break;
			}
		}
		if(explicit_pack_values) break;
	}
	vector<string> bound_pack_names;
	if(pack_parameter && bound_pack_values) for(map<string, vector<string> >::const_iterator
		bound = bound_pack_values->begin(); bound != bound_pack_values->end(); ++bound) {
		if(bound->first.empty()) continue;
		for(size_t position = pattern.find(bound->first); position != string::npos;
			position = pattern.find(bound->first, position + bound->first.size())) {
			const bool left = position == 0 || !IsIdentifierCharacter(pattern[position - 1]);
			const size_t end = position + bound->first.size();
			const bool right = end == pattern.size() || !IsIdentifierCharacter(pattern[end]);
			if(left && right) {
				bound_pack_names.push_back(bound->first);
				break;
			}
		}
	}
	size_t trailing_fixed = 0;
	if(pack_parameter) for(size_t later = parameter_position + 1;
		later < parameter_list->children.size(); ++later)
		if(parameter_list->children[later] && parameter_list->children[later]->kind ==
			"parameter-declaration" && !IsFunctionParameterPack(parameter_list->children[later])) ++trailing_fixed;
	const size_t pack_count = pack_parameter && arguments->children.size() >=
		*argument_index + trailing_fixed ? arguments->children.size() - *argument_index - trailing_fixed : 0;
	size_t bound_pack_count = pack_count;
	if(pack_parameter && !bound_pack_names.empty() && bound_pack_values) {
		map<string, vector<string> >::const_iterator bound = bound_pack_values->find(
			bound_pack_names[0]);
		if(bound != bound_pack_values->end()) bound_pack_count = bound->second.size();
	}
	// A function parameter pack followed by a fixed parameter is a
	// non-deduced context.  Without an explicit binding (or an enclosing typed
	// pack binding), deduction must leave it empty; consuming the leading call
	// arguments here incorrectly makes `f(1, 2, 3)` viable for
	// `f(Types..., T1)`.
	const bool nondeduced_nonterminal_pack = pack_parameter && trailing_fixed > 0 &&
		!explicit_pack_values && bound_pack_names.empty();
	const size_t visits = nondeduced_nonterminal_pack ? 0 : (pack_parameter ? bound_pack_count :
		(*argument_index < arguments->children.size() ? 1 : 0));
	if(explicit_pack_values && visits != explicit_pack_values->size()) return false;
	for(size_t visit = 0; visit < visits; ++visit) {
		if(*argument_index >= arguments->children.size()) return false;
		map<string, string> parameter_substitutions = substitutions;
		for(size_t bound = 0; bound < bound_pack_names.size(); ++bound) {
			const vector<string>& values = bound_pack_values->find(
				bound_pack_names[bound])->second;
			if(visit >= values.size()) return false;
			parameter_substitutions[bound_pack_names[bound]] = values[visit];
		}
		string type;
		const CPPGMAstNodePtr argument = arguments->children[*argument_index];
		FunctionSignature signature;
		bool inferred_argument = InferArgument(argument, &type, parameter_substitutions,
				context, &signature);
		if(inferred_argument && !type.empty()) {
			string resolved_type = ReplaceIdentifiersPreservingPackSizes(type,
				parameter_substitutions);
			try {
				resolved_type = const_cast<PA18TemplateExpander*>(this)->RewriteText(
					resolved_type, context, parameter_substitutions, 0);
			} catch(const PA18SubstitutionFailure&) {}
			resolved_type = CanonicalSpelling(ResolveAlias(resolved_type, context));
				if(!resolved_type.empty()) type = resolved_type;
			}
		// An empty braced-init-list is a non-deduced argument, but it still
		// participates in viability once the other template arguments have
		// supplied every dependent part of this parameter.  Carry the resolved
		// parameter spelling as the candidate-local type fact instead of making
		// a non-dependent parameter reject the candidate outright.
		if(!inferred_argument && argument && argument->kind == "braced-init-list" &&
			argument->children.empty()) {
			map<string, string> braced_substitutions = parameter_substitutions;
			for(map<string, string>::const_iterator binding = inferred->begin();
				binding != inferred->end(); ++binding)
				braced_substitutions[binding->first] = binding->second;
			string resolved_braced = ReplaceIdentifiersPreservingPackSizes(
				pattern, braced_substitutions);
			try {
				resolved_braced = const_cast<PA18TemplateExpander*>(this)->RewriteText(
					resolved_braced, context, braced_substitutions, 0);
			} catch(const PA18SubstitutionFailure&) {
				resolved_braced.clear();
			}
			resolved_braced = NormalizeTypeArgument(ReplaceIdentifiers(
				resolved_braced, braced_substitutions));
			if(!resolved_braced.empty() && !HasUnresolvedTemplateParameter(
				resolved_braced, context, braced_substitutions)) {
				type = resolved_braced;
				inferred_argument = true;
			}
		}
		if(inferred_argument && signature.result_specifiers && signature.parameters &&
			pattern.size() > 2 && pattern.compare(pattern.size() - 2, 2, "&&") == 0 &&
			IsLvalueTemplateArgument(argument))
			signature.lvalue_argument = true;
		const bool enumerator_prvalue = argument && argument->kind == "id-expression" &&
			enumerator_types_.find(RemoveMarker(argument->value)) != enumerator_types_.end();
		if(!pack_parameter && !pattern.empty() && pattern[pattern.size() - 1] == '&' &&
			(pattern.size() < 2 || pattern[pattern.size() - 2] != '&') &&
			!ConstReferenceParameterPattern(pattern) &&
			(!IsLvalueTemplateArgument(argument) || enumerator_prvalue) &&
			(!inferred_argument || type.empty() || type[type.size() - 1] != '&')) return false;
		// String literals are arrays, not pointers, when the parameter preserves
		// the reference.  Keep that typed fact for deduction; the ordinary
		// non-reference path retains the pointer spelling returned by
		// InferArgument and therefore still performs array-to-pointer decay.
		if(inferred_argument && argument && argument->kind == "literal" &&
			argument->value.find('"') != string::npos &&
			ReferenceParameterPattern(pattern)) {
			const string literal_array_reference = StringLiteralArrayReferenceType(argument->value);
			if(!literal_array_reference.empty()) type = literal_array_reference;
		}
		if(inferred_argument && (deduction_pattern.empty() ||
			deduction_pattern[deduction_pattern.size() - 1] != '&'))
			while(!type.empty() && type[type.size() - 1] == '&')
				type = CanonicalSpelling(type.substr(0, type.size() - 1));
		// Array expressions undergo the standard function-parameter decay before
		// template deduction.  Keep the promoted local-class element type while
		// changing only the array transport to a pointer (`T a[N]` -> `T*`).
		if(inferred_argument && (deduction_pattern.find('*') != string::npos ||
			!ReferenceParameterPattern(deduction_pattern))) {
			const size_t array = type.rfind('[');
			if(array != string::npos && !type.empty() && type[type.size() - 1] == ']')
				type = CanonicalSpelling(type.substr(0, array) + "*");
		}
		// A parameter declared as `T const s[]` is adjusted to `T const*`
		// before function-template deduction.  A reference-to-array parameter is
		// deliberately excluded: its bound and element type are the deduction
		// pattern, not a decay opportunity.
		const size_t array_pattern = deduction_pattern.rfind('[');
		const bool reference_array = deduction_pattern.find("(&)") != string::npos ||
			deduction_pattern.find("(& ") != string::npos;
		if(array_pattern != string::npos && !reference_array) {
			deduction_pattern = CanonicalSpelling(deduction_pattern.substr(0,
				array_pattern) + "*");
			const size_t array = type.find('[');
			if(array != string::npos)
				 type = CanonicalSpelling(type.substr(0, array) + "*");
		}
		// Keep source partial-ordering patterns strict, but apply the ordinary
		// call-deduction spelling rules here.  The semantic type model commonly
		// writes promoted unsigned/long values with an explicit `int`, and a
		// pointer-to-const parameter may accept an unqualified pointer argument.
		const auto normalize_builtin = [](string spelling) {
			string prefix;
			if(spelling.compare(0, 6, "const ") == 0) {
				prefix = "const "; spelling = CanonicalSpelling(spelling.substr(6));
			} else if(spelling.compare(0, 9, "volatile ") == 0) {
				prefix = "volatile "; spelling = CanonicalSpelling(spelling.substr(9));
			}
			if(spelling == "unsigned") spelling = "unsigned int";
			else if(spelling == "short") spelling = "short int";
			else if(spelling == "unsigned short") spelling = "unsigned short int";
			else if(spelling == "long") spelling = "long int";
			else if(spelling == "unsigned long") spelling = "unsigned long int";
			else if(spelling == "long long") spelling = "long long int";
			else if(spelling == "unsigned long long") spelling = "unsigned long long int";
			return CanonicalSpelling(prefix + spelling);
		};
		const auto normalize_pointer_pointee_cv = [](string spelling) {
			if(spelling.size() > 1 && spelling[spelling.size() - 1] == '*') {
				const string before = spelling.substr(0, spelling.size() - 1);
				if(before.size() > 6 && before.compare(before.size() - 6, 6, " const") == 0)
					return CanonicalSpelling("const " + before.substr(0, before.size() - 6) + "*");
				if(before.size() > 9 && before.compare(before.size() - 9, 9, " volatile") == 0)
					return CanonicalSpelling("volatile " + before.substr(0, before.size() - 9) + "*");
			}
			return spelling;
		};
		deduction_pattern = normalize_pointer_pointee_cv(
			normalize_builtin(deduction_pattern));
		string deduction_type = normalize_pointer_pointee_cv(normalize_builtin(type));
		// The compact semantic type fact for a named object is its referred-to
		// type; the reference is a property of the expression category.  Preserve
		// that category at the deduction boundary for a non-const lvalue
		// reference parameter.  Without it, a pattern such as `Vec<P>&` is matched
		// against `Vec<Info*>` as though the call supplied a prvalue, so the class
		// specialization cannot contribute its template argument.
		const bool nonconst_lvalue_reference = !pack_parameter &&
			!deduction_pattern.empty() &&
			deduction_pattern[deduction_pattern.size() - 1] == '&' &&
			(deduction_pattern.size() < 2 ||
			 deduction_pattern[deduction_pattern.size() - 2] != '&') &&
			!ConstReferenceParameterPattern(deduction_pattern) &&
			IsLvalueTemplateArgument(argument) && !enumerator_prvalue;
		if(inferred_argument && nonconst_lvalue_reference &&
			type.find('[') == string::npos &&
			(deduction_type.empty() || deduction_type[deduction_type.size() - 1] != '&')) {
			type = CanonicalSpelling(type + "&");
			deduction_type = normalize_pointer_pointee_cv(normalize_builtin(type));
		}
		if(deduction_pattern.compare(0, 6, "const ") == 0 &&
			deduction_pattern.size() > 6 && deduction_pattern[deduction_pattern.size() - 1] == '*' &&
			deduction_type.compare(0, 6, "const ") != 0)
			deduction_pattern.erase(0, 6);
		else if(deduction_pattern.compare(0, 9, "volatile ") == 0 &&
			deduction_pattern.size() > 9 && deduction_pattern[deduction_pattern.size() - 1] == '*' &&
			deduction_type.compare(0, 9, "volatile ") != 0)
			deduction_pattern.erase(0, 9);
		const vector<string> function_types = deduction_pattern.find(")(") != string::npos ?
			FunctionExpressionTypes(argument, context) : vector<string>();
		if(!function_types.empty()) {
			const bool overloaded = function_types.size() > 1 && argument &&
				(argument->kind == "id-expression" || (argument->kind == "unary-expression" &&
				 RemoveMarker(argument->value) == "&"));
			if(overloaded) {
				deferred_patterns->push_back(deduction_pattern);
				deferred_arguments->push_back(argument);
				inferred_argument = false;
			} else {
				type = function_types[0];
				inferred_argument = true;
			}
		}
		// A pointer to a dependent member typedef is an expected function
		// signature, not an ordinary object type.  Its overload-set argument
		// cannot be checked until later parameters have deduced the owner of
		// that typedef (for example `equality<TypeA>::type*` before `TypeA&`).
		if(inferred_argument && signature.result_specifiers && signature.parameters &&
			pattern.find("::type") != string::npos && pattern.find('*') != string::npos) {
			deferred_patterns->push_back(deduction_pattern);
			deferred_arguments->push_back(argument);
			inferred_argument = false;
		}
		if(inferred_argument && pattern.size() > 2 && pattern.compare(pattern.size() - 2, 2, "&&") == 0 &&
			argument && (argument->kind == "id-expression" || argument->kind == "member-expression" ||
				argument->kind == "subscript-expression") && !enumerator_prvalue) {
			if(signature.result_specifiers && signature.parameters) {
				// A function lvalue passed to a forwarding reference deduces the
				// function type itself (`T = R(Args...)&`), not the decayed
				// function-pointer fact used by ordinary value parameters.
				string direct_function_type = NodeTypeSpelling(signature.result_specifiers) + "(";
				for(size_t parameter = 0; parameter < signature.parameters->children.size();
					++parameter) {
					const CPPGMAstNodePtr item = signature.parameters->children[parameter];
					if(!item || item->kind != "parameter-declaration") continue;
					if(direct_function_type[direct_function_type.size() - 1] != '(')
						direct_function_type += ',';
					const bool function_parameter = item->children.size() > 1 && item->children[1] &&
						ChildOfKindLocal(item->children[1], "nested-declarator") &&
						ChildOfKindLocal(item->children[1], "parameter-clause");
					direct_function_type += function_parameter ? FunctionTypeSpelling(item) :
						ParameterTypeSpelling(item);
				}
				direct_function_type += ")&";
				type = CanonicalSpelling(direct_function_type);
				} else {
					const bool lvalue_reference_spelling = type.find('&') != string::npos &&
						type.find("&&") == string::npos;
					if(!lvalue_reference_spelling) {
						while(!type.empty() && type[type.size() - 1] == '&')
							type.erase(type.size() - 1);
						type = CanonicalSpelling(type + "&");
					}
				}
			// Forwarding-reference deduction changes the typed argument used for
			// merging, not just the later replay fact.  Recompute the normalized
			// spelling after adding the lvalue reference so `T&&` with an lvalue
			// deduces `T = U&`, rather than materializing a stale `T = U` call.
				deduction_type = normalize_pointer_pointee_cv(normalize_builtin(type));
		}
		if(inferred_argument && forwarding_pack_values && pack_parameter &&
			pattern.size() > 2 && pattern.compare(pattern.size() - 2, 2, "&&") == 0) {
			string forwarding_name;
			for(size_t candidate = 0; candidate < definition.parameters.size(); ++candidate) {
				const TemplateParameter& template_parameter = definition.parameters[candidate];
				if(!template_parameter.pack || !template_parameter.type ||
					template_parameter.name.empty()) continue;
				const size_t position = pattern.find(template_parameter.name);
				if(position == string::npos ||
					(position > 0 && IsIdentifierCharacter(pattern[position - 1])) ||
					(position + template_parameter.name.size() < pattern.size() &&
						IsIdentifierCharacter(pattern[position + template_parameter.name.size()]))) continue;
				forwarding_name = template_parameter.name;
				break;
			}
			if(!forwarding_name.empty())
				(*forwarding_pack_values)[forwarding_name].push_back(type);
		}
		bool null_pointer_conversion = false;
		const bool null_pointer_constant = inferred_argument && argument &&
			((argument->kind == "literal" &&
				Trim(RemoveMarker(argument->value)) == "0") ||
			(argument->kind == "keyword-literal" &&
				Trim(RemoveMarker(argument->value)) == "nullptr"));
		if(null_pointer_constant && deduction_pattern.find('*') != string::npos) {
			map<string, string> null_pointer_substitutions = parameter_substitutions;
			for(map<string, string>::const_iterator binding = inferred->begin();
				binding != inferred->end(); ++binding)
				null_pointer_substitutions[binding->first] = binding->second;
			string null_pointer_pattern = NormalizeTypeArgument(ReplaceIdentifiers(
				deduction_pattern, null_pointer_substitutions));
			null_pointer_pattern = const_cast<PA18TemplateExpander*>(this)->RewriteText(
				null_pointer_pattern, context, null_pointer_substitutions, 0);
			null_pointer_pattern = NormalizeTypeArgument(ReplaceIdentifiers(
				null_pointer_pattern, null_pointer_substitutions));
			null_pointer_conversion = null_pointer_pattern.find('*') != string::npos &&
				!HasUnresolvedTemplateParameter(null_pointer_pattern, context,
					null_pointer_substitutions);
		}
		if(inferred_argument && explicit_pack_values) {
			// Explicit template arguments own this pack.  Ordinary deduction must
			// not append the same element a second time, but it still has to prove
			// that the call argument matches the explicitly selected element.
			map<string, string> explicit_substitutions = parameter_substitutions;
			explicit_substitutions[explicit_pack_name] = (*explicit_pack_values)[visit];
			const string explicit_pattern = NormalizeTypeArgument(ReplaceIdentifiers(
				pattern, explicit_substitutions));
			set<string> explicit_parameter_names = parameter_names;
			explicit_parameter_names.erase(explicit_pack_name);
			map<string, string> ignored;
			if(!MatchTypePattern(explicit_pattern, type, explicit_parameter_names,
				&ignored, context)) return false;
		} else if(inferred_argument && !null_pointer_conversion) {
				const bool merged = MergeInferredFunctionArgument(definition, deduction_pattern,
					deduction_type, signature, parameter_substitutions, context, parameter_names,
					inferred, inferred_packs, inferred_functions, bound_pack_values,
					fixed_template_parameters);
				if(!merged) return false;
		}
		++*argument_index;
	}
	return true;
}

bool PA18TemplateExpander::CompleteFunctionArguments(
	const TemplateDefinition& definition, const vector<string>& deferred_patterns,
	const vector<CPPGMAstNodePtr>& deferred_arguments, const set<string>& parameter_names,
	map<string, string>* inferred, const map<string, vector<string> >& inferred_packs,
	vector<string>* result, map<string, vector<string> >* inferred_pack_values,
	const string& context,
	const map<string, vector<string> >* forwarding_pack_values) const
{
	for(size_t deferred = 0; deferred < deferred_patterns.size(); ++deferred) {
		bool matched = false;
		map<string, string> expected_substitutions = *inferred;
		string expected_pattern = ReplaceIdentifiersPreservingPackSizes(
			deferred_patterns[deferred], expected_substitutions);
		try {
			expected_pattern = CanonicalSpelling(ResolveAlias(
				const_cast<PA18TemplateExpander*>(this)->RewriteText(
					expected_pattern, context, expected_substitutions, 0), context));
		} catch(const PA18SubstitutionFailure&) {
			expected_pattern.clear();
		}
		if(expected_pattern.find("(*") != string::npos &&
			!expected_pattern.empty() && expected_pattern[expected_pattern.size() - 1] == '*')
			expected_pattern = CanonicalSpelling(expected_pattern.substr(0,
			expected_pattern.size() - 1));
		CPPGMAstNodePtr function_argument = deferred_arguments[deferred];
		if(function_argument && function_argument->kind == "unary-expression" &&
			RemoveMarker(function_argument->value) == "&" &&
			!function_argument->children.empty())
			function_argument = function_argument->children[0];
		if(!expected_pattern.empty() && function_argument &&
			function_argument->kind == "id-expression") {
			const vector<const TemplateDefinition*> candidates =
				FindFunctionDefinitions(function_argument->value, context);
			for(size_t candidate = 0; candidate < candidates.size() && !matched; ++candidate) {
				vector<string> ignored;
				const bool viable = InferFunctionFromExpected(*candidates[candidate], expected_pattern,
					&ignored, context);
				if(viable) matched = true;
			}
		}
		if(!matched) {
			const vector<string> function_types = FunctionExpressionTypes(
				deferred_arguments[deferred], context);
			map<string, string> selected_inferred;
			for(size_t candidate = 0; candidate < function_types.size(); ++candidate) {
				map<string, string> one = *inferred;
				if(!MatchTypePattern(deferred_patterns[deferred], function_types[candidate],
					parameter_names, &one, context)) continue;
				if(!matched) {
					selected_inferred = one;
					matched = true;
				} else if(one != selected_inferred) {
					// An overload set used as a function-template argument is
					// viable only when deduction identifies one unambiguous
					// template argument set.  Accepting the first overload turns
					// a nondeduced call into an arbitrary specialization.
					return false;
				}
			}
			if(matched) *inferred = selected_inferred;
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
			} else if(inferred_pack_values) (*inferred_pack_values)[parameter.name] = vector<string>();
			continue;
		}
		map<string, string>::const_iterator found = inferred->find(parameter.name);
		if(found != inferred->end()) result->push_back(found->second);
		else if(!parameter.default_type.empty()) {
			// Defaults are ordered: a later default may depend on a parameter
			// supplied by an earlier default (`Result = Ref`).  Keep each
			// completed default in the typed deduction map so both the result
			// vector and subsequent defaults see the same concrete argument.
			const string value = NormalizeTypeArgument(ReplaceIdentifiers(
				parameter.default_type, *inferred));
			(*inferred)[parameter.name] = value;
			result->push_back(value);
		}
		else return false;
	}
	return true;
}

bool PA18TemplateExpander::InferFunctionArguments(const TemplateDefinition& definition,
	const CPPGMAstNodePtr& call, vector<string>* result,
	const map<string, string>& substitutions, const string& context,
		const vector<string>* explicit_prefix,
		map<string, vector<string> >* inferred_pack_values,
		map<string, FunctionSignature>* inferred_function_values,
		const map<string, vector<string> >* bound_pack_values,
		map<string, vector<string> >* forwarding_pack_values) const
{
	try {
	if(inferred_function_values) inferred_function_values->clear();
	if(!call || call->children.size() < 2 || !result) return false;
	const CPPGMAstNodePtr declarator = FunctionDeclarator(definition.declaration);
	const CPPGMAstNodePtr parameters = DescendantOfKind(declarator, "parameter-clause");
	const CPPGMAstNodePtr arguments = call->children[1] && call->children[1]->kind == "argument-list" ?
		call->children[1] : ChildOfKindLocal(call->children[1], "argument-list");
	if(!parameters || !arguments) return false;
	bool has_pack = false;
	size_t required_parameters = 0;
	for(size_t i = 0; i < parameters->children.size(); ++i) {
		const CPPGMAstNodePtr parameter = parameters->children[i];
		if(!parameter || parameter->kind != "parameter-declaration") continue;
		if(IsFunctionParameterPack(parameter)) has_pack = true;
		else if(!FunctionParameterHasDefault(definition, i)) ++required_parameters;
	}
	if(arguments->children.size() < required_parameters ||
		(!has_pack && arguments->children.size() > parameters->children.size())) return false;
	// An explicit template-id cannot bind arguments past a fixed template
	// parameter list.  The call-parameter pass below only consumes the prefix
	// positionally, so without this check a candidate such as `pick<A>` would
	// remain viable for `pick<A,B,C>` and steal the overload from the
	// three-parameter template.
	if(explicit_prefix) {
		bool template_pack = false;
		for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter)
			if(definition.parameters[parameter].pack) {
				template_pack = true;
				break;
			}
		if(!template_pack && explicit_prefix->size() > definition.parameters.size())
			return false;
	}
	bool only_ellipsis = !parameters->children.empty();
	for(size_t parameter = 0; parameter < parameters->children.size(); ++parameter)
		if(!parameters->children[parameter] ||
			parameters->children[parameter]->kind != "ellipsis") {
			only_ellipsis = false;
			break;
		}
	map<string, string> inferred;
	map<string, vector<string> > inferred_packs;
	set<string> fixed_template_parameters;
	set<string> parameter_names;
	vector<string> deferred_patterns;
	vector<CPPGMAstNodePtr> deferred_arguments;
	for(size_t i = 0; i < definition.parameters.size(); ++i) {
		if(!definition.parameters[i].name.empty()) parameter_names.insert(definition.parameters[i].name);
	}
	if(bound_pack_values) for(map<string, vector<string> >::const_iterator bound =
		bound_pack_values->begin(); bound != bound_pack_values->end(); ++bound)
		parameter_names.erase(bound->first);
	size_t explicit_index = 0;
	bool explicit_pack_consumed = false;
	if(explicit_prefix) for(size_t parameter = 0;
		parameter < definition.parameters.size(); ++parameter) {
		const TemplateParameter& template_parameter = definition.parameters[parameter];
		if(template_parameter.pack) {
			bool pack_precedes_fixed = false;
			for(size_t later = parameter + 1; later < definition.parameters.size(); ++later) {
				if(!definition.parameters[later].pack) {
					pack_precedes_fixed = true;
					break;
				}
			}
			if(pack_precedes_fixed || explicit_index < explicit_prefix->size()) {
				vector<string>& values = inferred_packs[template_parameter.name];
				while(explicit_index < explicit_prefix->size())
					values.push_back((*explicit_prefix)[explicit_index++]);
				if(!template_parameter.name.empty())
					fixed_template_parameters.insert(template_parameter.name);
				if(pack_precedes_fixed) explicit_pack_consumed = true;
			}
		} else if(!explicit_pack_consumed && explicit_index < explicit_prefix->size() &&
				!template_parameter.name.empty()) {
				inferred[template_parameter.name] = (*explicit_prefix)[explicit_index++];
				fixed_template_parameters.insert(template_parameter.name);
			}
	}
	if(only_ellipsis && explicit_prefix) {
		// Explicit arguments for a trailing template pack are flattened in the
		// completed argument vector.  The old parameter-count check rejected a
		// valid fallback such as `template<class, class...> f(...)` as soon as
		// more than two explicit arguments were supplied, leaving an invalid
		// overload selected when a preceding candidate's default SFINAE failed.
		size_t explicit_index = 0;
		bool explicit_pack_consumed = false;
		for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter) {
			const TemplateParameter& detail = definition.parameters[parameter];
			if(detail.pack) {
				while(explicit_index < explicit_prefix->size())
					result->push_back((*explicit_prefix)[explicit_index++]);
				explicit_pack_consumed = true;
				continue;
			}
			if(!explicit_pack_consumed && explicit_index < explicit_prefix->size())
				result->push_back((*explicit_prefix)[explicit_index++]);
			else if(!detail.default_type.empty()) result->push_back(detail.default_type);
			else return false;
		}
		return explicit_index == explicit_prefix->size();
	}
	if(only_ellipsis)
		return CompleteFunctionArguments(definition, deferred_patterns, deferred_arguments,
			parameter_names, &inferred, inferred_packs, result, inferred_pack_values,
			context, forwarding_pack_values);
	size_t argument_index = 0;
	for(size_t i = 0; i < parameters->children.size(); ++i) {
		const bool parameter_ok = InferFunctionParameter(definition, parameters->children[i], parameters, i, arguments, &argument_index,
			substitutions, context, parameter_names, &inferred, &inferred_packs,
			&deferred_patterns, &deferred_arguments, inferred_function_values,
			bound_pack_values, forwarding_pack_values,
			fixed_template_parameters.empty() ? 0 : &fixed_template_parameters);
		if(!parameter_ok) return false;
	}
	if(argument_index != arguments->children.size()) {
		return false;
	}
	const bool complete = CompleteFunctionArguments(definition, deferred_patterns, deferred_arguments,
			parameter_names, &inferred, inferred_packs, result, inferred_pack_values,
			context, forwarding_pack_values);
	return complete;
	} catch(const PA18SubstitutionFailure&) {
		throw;
	} catch(const logic_error& error) {
		throw PA18SubstitutionFailure(error.what());
	}
}

} // namespace pa18_templates_internal
