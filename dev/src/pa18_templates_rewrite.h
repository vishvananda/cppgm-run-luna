#pragma once
#include "pa18_templates_rewrite_lookup.h"
#include "pa18_templates_rewrite_decltype.h"
#include "pa18_templates_rewrite_instantiate.h"
bool MatchOrderingTypePattern(const string& raw_pattern, const string& raw_actual,
	const set<string>& parameter_names, map<string, string>* inferred) const;
bool MatchOrderingPatternList(const vector<string>& patterns,
	const vector<string>& actual, const set<string>& parameter_names,
	map<string, string>* inferred) const;
bool ClassPartialMoreSpecialized(const TemplateDefinition& lhs,
	const TemplateDefinition& rhs, const string& context) const;
string ExpandAliasPattern(string pattern, const string& context,
	set<string>* active) const;
bool MatchClassSpecializationPattern(const TemplateDefinition& definition,
	const vector<string>& arguments, map<string, string>* inferred,
	const string& context) const;
bool MatchTypePattern(string pattern, string actual,
		const set<string>& parameter_names, map<string, string>* inferred, const string& context, bool class_pattern = false) const;
	int MatchGeneratedBaseTypePattern(const string& pattern, const string& actual,
		const string& pattern_base, const set<string>& parameter_names,
		map<string, string>* inferred, const string& context, bool class_pattern) const;
	bool SplitDirectFunctionType(const string& raw, string* result, vector<string>* parameters, string* qualifiers) const;
	int MatchObjectCvPattern(const string& pattern, const string& actual, const set<string>& parameter_names,
				map<string, string>* inferred, const string& context,
				bool class_pattern = false) const;
	bool MatchTrailingTypePack(const vector<string>& pattern_parts, const vector<string>& actual_parts,
		const set<string>& parameter_names, map<string, string>* inferred, const string& context, bool class_pattern) const;
	bool IsTemplatePackName(const TemplateDefinition& definition, const string& name) const;
		const TemplateDefinition* SelectClassTemplateDefinition(
		const TemplateDefinition* primary, const vector<string>& arguments,
		const string& context) const
	{
		if(!primary) return primary;
		if(!primary->class_template && !primary->alias_template &&
			!primary->variable_template) return primary;
		vector<const TemplateDefinition*> candidates;
		map<string, vector<TemplateDefinition> >::const_iterator direct_candidates =
			class_specializations_.find(primary->qualified_name);
		if(direct_candidates != class_specializations_.end())
			for(size_t i = 0; i < direct_candidates->second.size(); ++i)
				candidates.push_back(&direct_candidates->second[i]);
		else {
			const auto strip_template_arguments = [](const string& raw) {
				string result;
				int depth = 0;
				for(size_t i = 0; i < raw.size(); ++i) {
					if(raw[i] == '<') { ++depth; continue; }
					if(raw[i] == '>') { if(depth > 0) --depth; continue; }
					if(depth == 0) result += raw[i];
				}
				return result;
			};
			const auto collapse_repeated_owner = [](string raw) {
				const size_t separator = raw.rfind("::");
				if(separator != string::npos &&
					LastComponent(raw.substr(0, separator)) == raw.substr(separator + 2))
					raw.erase(separator);
				return raw;
			};
			const string primary_owner = collapse_repeated_owner(
				strip_template_arguments(primary->owner));
			for(map<string, vector<TemplateDefinition> >::const_iterator group =
				class_specializations_.begin(); group != class_specializations_.end(); ++group)
				for(size_t i = 0; i < group->second.size(); ++i) {
					const TemplateDefinition& candidate = group->second[i];
					if(candidate.name != primary->name) continue;
					const string candidate_owner = collapse_repeated_owner(
						strip_template_arguments(candidate.owner));
					if(candidate_owner == primary_owner) candidates.push_back(&candidate);
				}
		}
		if(candidates.empty()) return primary;
		vector<string> matching_arguments = arguments;
		map<string, string> default_substitutions;
		for(size_t parameter = 0; parameter < primary->parameters.size() && parameter < arguments.size(); ++parameter)
			if(!primary->parameters[parameter].name.empty()) default_substitutions[primary->parameters[parameter].name] = arguments[parameter];
		for(size_t parameter = arguments.size(); parameter < primary->parameters.size(); ++parameter) {
			if(primary->parameters[parameter].default_type.empty()) break;
			const string default_argument = NormalizeTypeArgument(ReplaceIdentifiers(primary->parameters[parameter].default_type, default_substitutions));
			matching_arguments.push_back(default_argument);
			if(!primary->parameters[parameter].name.empty()) default_substitutions[primary->parameters[parameter].name] = default_argument;
		}
		vector<const TemplateDefinition*> matched;
		for(size_t i = 0; i < candidates.size(); ++i) {
			const bool matches = MatchClassSpecializationPattern(*candidates[i],
				matching_arguments, 0, context);
			if(matches)
				matched.push_back(candidates[i]);
		}
		if(matched.empty()) return primary;
		for(size_t i = 0; i < matched.size(); ++i) {
			bool dominated = false;
			for(size_t j = 0; j < matched.size(); ++j) {
				if(i == j) continue;
				if(ClassPartialMoreSpecialized(*matched[j], *matched[i], context)) {
					dominated = true;
					break;
				}
			}
			if(!dominated) return matched[i];
		}
		return matched[0];
	}
	CPPGMAstNodePtr FindClassDeclaration(string raw_class, const string& context) const
	{
		raw_class = CanonicalSpelling(raw_class);
		while(!raw_class.empty() && (raw_class[raw_class.size() - 1] == '&' ||
			raw_class[raw_class.size() - 1] == '*')) raw_class.erase(raw_class.size() - 1);
		raw_class = CanonicalSpelling(raw_class);
		map<string, CPPGMAstNodePtr>::const_iterator concrete = class_declarations_.find(raw_class);
		if(concrete != class_declarations_.end()) return concrete->second;
		const size_t template_open = raw_class.find('<');
		if(template_open != string::npos) {
			const TemplateDefinition* template_definition = FindDefinition(
				raw_class.substr(0, template_open), context);
			if(template_definition && template_definition->class_template) {
				const string argument_text = raw_class.substr(template_open + 1,
					raw_class.size() - template_open - 2);
				const vector<string> arguments = SplitTemplateArguments(argument_text);
				const TemplateDefinition* selected = SelectClassTemplateDefinition(
					template_definition, arguments, context);
				return selected ? selected->declaration : template_definition->declaration;
			}
		}
		map<string, string>::const_iterator specialization = specialization_bases_.find(
			LastComponent(raw_class));
		if(specialization != specialization_bases_.end()) raw_class = specialization->second;
		map<string, CPPGMAstNodePtr>::const_iterator direct = class_declarations_.find(raw_class);
		if(direct != class_declarations_.end()) return direct->second;
		for(string current = context; ; ) {
			const string candidate = JoinPath(current, raw_class);
			map<string, CPPGMAstNodePtr>::const_iterator found = class_declarations_.find(candidate);
			if(found != class_declarations_.end()) return found->second;
			if(current.empty()) break;
			const size_t separator = current.rfind("::");
			if(separator == string::npos) current.clear();
			else current.erase(separator);
		}
		CPPGMAstNodePtr result;
		for(map<string, CPPGMAstNodePtr>::const_iterator it = class_declarations_.begin();
			it != class_declarations_.end(); ++it) {
			if(LastComponent(it->first) != LastComponent(raw_class)) continue;
			if(result) return CPPGMAstNodePtr();
			result = it->second;
		}
		if(result) return result;
		const TemplateDefinition* definition = FindDefinition(raw_class, context);
		return definition && definition->class_template ? definition->declaration :
			CPPGMAstNodePtr();
	}
	bool FindClassMemberType(const string& raw_class, const string& member,
		const map<string, string>& substitutions, const string& context,
		string* result, set<string>* active, bool aliases_only = false) const
	{
		if(!result || !active) return false;
		string class_key = CanonicalSpelling(raw_class);
		bool object_const = false;
		while(class_key.compare(0, 6, "const ") == 0) {
			object_const = true;
			class_key = CanonicalSpelling(class_key.substr(6));
		}
		while(class_key.compare(0, 9, "volatile ") == 0)
			class_key = CanonicalSpelling(class_key.substr(9));
		while(!class_key.empty() && (class_key[class_key.size() - 1] == '&' ||
			class_key[class_key.size() - 1] == '*')) class_key.erase(class_key.size() - 1);
		class_key = CanonicalSpelling(class_key);
		while(class_key.size() > 6 && class_key.compare(class_key.size() - 6, 6,
			" const") == 0) {
			object_const = true;
			class_key = CanonicalSpelling(class_key.substr(0, class_key.size() - 6));
		}
		while(class_key.size() > 9 && class_key.compare(class_key.size() - 9, 9,
			" volatile") == 0)
			class_key = CanonicalSpelling(class_key.substr(0, class_key.size() - 9));
		map<string, string> class_substitutions = substitutions;
		// A direct template-id such as `node_value<key>` is looked up against the
		// primary declaration below, so its class-template arguments must also be
		// installed before replaying member return types.  Without this mapping
		// `node_value<T>::_M_v()` leaks its own `T` into the enclosing call
		// deduction instead of producing `const key&`.
		const size_t class_open = class_key.find('<');
		if(class_open != string::npos) {
			string class_argument_text;
			size_t class_close = string::npos;
			string class_base;
			size_t class_begin = 0;
			if(TemplateBase(class_key, class_open, &class_begin, &class_base) &&
				TemplateRange(class_key, class_open, &class_argument_text, &class_close)) {
				const TemplateDefinition* class_definition = FindDefinition(class_base, context);
				if(class_definition && class_definition->class_template) {
					const vector<string> class_arguments = SplitTemplateArguments(
						class_argument_text);
					const TemplateDefinition* selected_class = SelectClassTemplateDefinition(
						class_definition, class_arguments, context);
					if(selected_class) class_definition = selected_class;
						for(size_t parameter = 0; parameter < class_definition->parameters.size() &&
							parameter < class_arguments.size(); ++parameter)
							if(!class_definition->parameters[parameter].name.empty()) {
								const string argument = CanonicalSpelling(ReplaceIdentifiers(
									class_arguments[parameter], class_substitutions));
								class_substitutions[class_definition->parameters[parameter].name] =
									class_definition->parameters[parameter].template_template ? argument :
									CanonicalSpelling(ResolveAlias(argument, context));
							}
					if(!class_definition->name.empty()) class_substitutions[class_definition->name] =
						class_key;
				}
			}
		}
		map<string, string>::const_iterator specialization = specialization_bases_.find(
			LastComponent(class_key));
		map<string, CPPGMAstNodePtr>::const_iterator concrete_declaration =
			class_declarations_.find(class_key);
		const bool incomplete_concrete = concrete_declaration == class_declarations_.end() ||
			(concrete_declaration->second &&
				(concrete_declaration->second->kind == "class-forward-declaration" ||
					concrete_declaration->second->children.size() <= 1));
		map<string, vector<string> >::const_iterator specialization_arguments =
			specialization_arguments_.find(LastComponent(class_key));
		const TemplateDefinition* specialization_definition =
			specialization == specialization_bases_.end() ? 0 :
			FindDefinition(specialization->second, context);
		// Materialized class declarations are complete by this point, but member
		// return types still use aliases from the source class template.  Carry the
		// concrete class arguments into that lookup even when no forward shell is
		// being replaced; otherwise `begin()` returns the spelling `iterator` and
		// a dependent member operator cannot deduce its other class parameter.
		if(specialization_definition && specialization_arguments !=
			specialization_arguments_.end()) {
			for(size_t parameter = 0; parameter < specialization_definition->parameters.size() &&
				parameter < specialization_arguments->second.size(); ++parameter)
				if(!specialization_definition->parameters[parameter].name.empty())
					class_substitutions[specialization_definition->parameters[parameter].name] =
						specialization_arguments->second[parameter];
			if(!specialization_definition->name.empty())
				class_substitutions[specialization_definition->name] = class_key;
		}
		if(specialization != specialization_bases_.end() && incomplete_concrete)
			class_key = specialization->second;
		const string active_key = class_key + "|" + member;
		if(!active->insert(active_key).second) return false;
		CPPGMAstNodePtr declaration = FindClassDeclaration(class_key, context);
		if(!declaration) {
			active->erase(active_key);
			return false;
		}
		const string declaration_context = PrefixComponent(class_key).empty() ?
			context : PrefixComponent(class_key);
		string fallback_type;
		for(size_t i = 0; i < declaration->children.size(); ++i) {
			const CPPGMAstNodePtr child = declaration->children[i];
			if(!child) continue;
			if(!aliases_only && child->kind == "function-definition" &&
				child->children.size() > 1 &&
				LastComponent(FirstIdentifierLocal(child->children[1])) == member) {
				string type = NodeTypeSpelling(child->children[0]) +
					ReturnDeclaratorSuffix(child->children[1]);
				const bool function_const = DeclaratorSuffix(child->children[1]).find("const") != string::npos;
				if(function_const != object_const) {
					if(!object_const && fallback_type.empty())
						fallback_type = type;
					continue;
				}
					*result = CanonicalSpelling(ReplaceIdentifiers(type, class_substitutions));
				active->erase(active_key);
				return !result->empty();
			}
			if(child->kind != "simple-declaration" || child->children.empty()) continue;
			if(aliases_only && SpellNode(child->children[0]).find("typedef") == string::npos)
				continue;
			const string base = NodeTypeSpelling(child->children[0]);
			const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
			if(!list) continue;
			for(size_t item = 0; item < list->children.size(); ++item) {
				const CPPGMAstNodePtr init = list->children[item];
				if(!init || init->children.empty()) continue;
				if(aliases_only && DescendantOfKind(init->children[0], "parameter-clause"))
					continue;
				if(LastComponent(FirstIdentifierLocal(init->children[0])) != member) continue;
					*result = CanonicalSpelling(ReplaceIdentifiers(
						DeclaratorTypeSpelling(base, init->children[0]), class_substitutions));
				active->erase(active_key);
				return !result->empty();
			}
		}
		if(!fallback_type.empty()) {
			*result = CanonicalSpelling(ReplaceIdentifiers(fallback_type, class_substitutions));
			active->erase(active_key);
			return !result->empty();
		}
		for(size_t i = 0; i < declaration->children.size(); ++i) {
			const CPPGMAstNodePtr child = declaration->children[i];
			if(!child || child->kind != "base-clause") continue;
			for(size_t base_index = 0; base_index < child->children.size(); ++base_index) {
				const CPPGMAstNodePtr base_specifier = child->children[base_index];
				const CPPGMAstNodePtr base_name = ChildOfKindLocal(base_specifier, "base-name");
				if(!base_name) continue;
				string base_spelling = NormalizeElaboratedSpelling(
					ReplaceIdentifiers(base_name->value, class_substitutions), declaration_context);
				base_spelling = CanonicalSpelling(base_spelling);
				const size_t open = base_spelling.find('<');
				const TemplateDefinition* base_definition = 0;
				map<string, string> base_substitutions;
				string base_lookup = base_spelling;
				if(open != string::npos) {
					string argument_text;
					size_t close = string::npos;
					if(!TemplateRange(base_spelling, open, &argument_text, &close)) continue;
					base_lookup = base_spelling.substr(0, open);
					base_definition = FindDefinition(base_lookup, declaration_context);
					if(base_definition) {
						const vector<string> arguments = SplitTemplateArguments(argument_text);
						for(size_t parameter = 0; parameter < base_definition->parameters.size() &&
							parameter < arguments.size(); ++parameter)
							if(!base_definition->parameters[parameter].name.empty())
								base_substitutions[base_definition->parameters[parameter].name] =
									QualifyTypeArgument(NormalizeElaboratedSpelling(
									ReplaceIdentifiers(arguments[parameter], class_substitutions), declaration_context),
										declaration_context, base_definition->owner);
					}
				}
				if(base_definition) base_lookup = base_definition->qualified_name;
				if(FindClassMemberType(base_lookup, member, base_substitutions,
					declaration_context, result, active, aliases_only)) {
					active->erase(active_key);
					return true;
				}
			}
		}
		active->erase(active_key);
		return false;
	}
	bool InferArgument(const CPPGMAstNodePtr& expression, string* result,
		const map<string, string>& substitutions, const string& context,
		FunctionSignature* function_signature = 0) const;
	bool IsFunctionParameterPack(const CPPGMAstNodePtr& parameter) const
	{
		return parameter && DescendantOfKind(parameter, "parameter-pack");
	}
	string FunctionSignatureType(const FunctionSignature& signature) const
	{
		if(!signature.result_specifiers || !signature.parameters) return string();
		string result = NodeTypeSpelling(signature.result_specifiers) + "(*) (";
		for(size_t i = 0; i < signature.parameters->children.size(); ++i) {
			const CPPGMAstNodePtr parameter = signature.parameters->children[i];
			if(!parameter || parameter->kind != "parameter-declaration") continue;
			if(result[result.size() - 1] != '(') result += ',';
			const bool function_parameter = parameter->children.size() > 1 &&
				parameter->children[1] &&
				ChildOfKindLocal(parameter->children[1], "nested-declarator") &&
				ChildOfKindLocal(parameter->children[1], "parameter-clause");
			result += function_parameter ? FunctionTypeSpelling(parameter) :
				ParameterTypeSpelling(parameter);
		}
		result += ')';
		return CanonicalSpelling(result);
	}
	vector<string> FunctionExpressionTypes(const CPPGMAstNodePtr& expression,
		const string& context) const
	{
		vector<string> result;
		CPPGMAstNodePtr function = expression;
		if(function && function->kind == "unary-expression" &&
			RemoveMarker(function->value) == "&" && !function->children.empty())
			function = function->children[0];
		if(!function || function->kind != "id-expression") return result;
		const string name = LastComponent(function->value);
		map<string, vector<string> >::const_iterator names =
			function_signatures_by_name_.find(name);
		if(names != function_signatures_by_name_.end()) {
			for(size_t i = 0; i < names->second.size(); ++i) {
				map<string, vector<FunctionSignature> >::const_iterator overloads =
					function_overloads_.find(names->second[i]);
				if(overloads != function_overloads_.end())
					for(size_t overload = 0; overload < overloads->second.size(); ++overload) {
						const string type = FunctionSignatureType(overloads->second[overload]);
						if(!type.empty() && find(result.begin(), result.end(), type) == result.end())
							result.push_back(type);
					}
				else {
					map<string, FunctionSignature>::const_iterator signature =
						function_signatures_.find(names->second[i]);
					if(signature == function_signatures_.end()) continue;
					const string type = FunctionSignatureType(signature->second);
					if(!type.empty() && find(result.begin(), result.end(), type) == result.end())
						result.push_back(type);
				}
			}
		}
		if(result.empty()) {
			const FunctionSignature* signature = FindFunctionSignature(function->value, context);
			if(signature) {
				const string type = FunctionSignatureType(*signature);
				if(!type.empty()) result.push_back(type);
			}
		}
		return result;
	}
	bool InferFunctionArguments(const TemplateDefinition& definition,
		const CPPGMAstNodePtr& call, vector<string>* result,
		const map<string, string>& substitutions, const string& context,
		const vector<string>* explicit_prefix = 0,
		map<string, vector<string> >* inferred_pack_values = 0,
		map<string, FunctionSignature>* inferred_function_values = 0,
		const map<string, vector<string> >* bound_pack_values = 0) const;
	bool MergeInferredFunctionArgument(const TemplateDefinition& definition,
		const string& pattern, const string& type, const FunctionSignature& signature,
		const map<string, string>& substitutions, const string& context,
		const set<string>& parameter_names, map<string, string>* inferred,
		map<string, vector<string> >* inferred_packs,
		map<string, FunctionSignature>* inferred_functions,
		const map<string, vector<string> >* bound_pack_values = 0) const;
	bool InferFunctionParameter(const TemplateDefinition& definition,
		const CPPGMAstNodePtr& parameter, const CPPGMAstNodePtr& parameter_list,
		size_t parameter_position, const CPPGMAstNodePtr& arguments,
		size_t* argument_index, const map<string, string>& substitutions,
		const string& context, const set<string>& parameter_names,
		map<string, string>* inferred, map<string, vector<string> >* inferred_packs,
		vector<string>* deferred_patterns,
		vector<CPPGMAstNodePtr>* deferred_arguments,
		map<string, FunctionSignature>* inferred_functions,
		const map<string, vector<string> >* bound_pack_values = 0) const;
	bool CompleteFunctionArguments(const TemplateDefinition& definition,
		const vector<string>& deferred_patterns,
		const vector<CPPGMAstNodePtr>& deferred_arguments,
		const set<string>& parameter_names, map<string, string>* inferred,
		const map<string, vector<string> >& inferred_packs, vector<string>* result,
		map<string, vector<string> >* inferred_pack_values,
		const string& context) const;
	bool SplitFunctionPointerType(string raw, string* result_type,
		vector<string>* parameters) const
	{
		raw = CanonicalSpelling(raw);
		const size_t separator = raw.find(")(");
		if(separator == string::npos || raw.empty() || raw[raw.size() - 1] != ')') return false;
		const size_t pointer = raw.rfind("(*", separator);
		const size_t reference = raw.rfind("(&", separator);
		const size_t owner = pointer != string::npos ? pointer : reference;
		if(owner == string::npos) return false;
		if(result_type) *result_type = CanonicalSpelling(raw.substr(0, owner));
		if(parameters) *parameters = SplitTemplateArguments(raw.substr(separator + 2,
			raw.size() - separator - 3));
		return result_type == 0 || !result_type->empty();
	}
	bool InferFunctionFromExpected(const TemplateDefinition& definition,
		string expected, vector<string>* result, const string& context) const
	{
		if(!result || definition.class_template) return false;
		string expected_result;
		vector<string> expected_parameters;
		expected = ResolveAlias(expected, context);
		if(!SplitFunctionPointerType(expected, &expected_result, &expected_parameters)) return false;
		const CPPGMAstNodePtr declarator = FunctionDeclarator(definition.declaration);
		if(!declarator || !definition.declaration || definition.declaration->children.empty()) return false;
		const CPPGMAstNodePtr parameter_clause = DescendantOfKind(declarator, "parameter-clause");
		if(!parameter_clause || parameter_clause->children.size() != expected_parameters.size()) return false;
		set<string> parameter_names;
		for(size_t i = 0; i < definition.parameters.size(); ++i)
			if(!definition.parameters[i].name.empty()) parameter_names.insert(
				definition.parameters[i].name);
		map<string, string> inferred;
		const string result_pattern = NodeTypeSpelling(definition.declaration->children[0]) +
			DeclaratorSuffix(declarator);
		if(!MatchTypePattern(result_pattern, expected_result, parameter_names, &inferred, context)) return false;
		size_t expected_index = 0;
		for(size_t i = 0; i < parameter_clause->children.size(); ++i) {
			const CPPGMAstNodePtr parameter = parameter_clause->children[i];
			if(!parameter || parameter->kind != "parameter-declaration") continue;
			if(!MatchTypePattern(ParameterTypeSpelling(parameter), expected_parameters[expected_index++],
				parameter_names, &inferred, context)) return false;
		}
		for(size_t i = 0; i < definition.parameters.size(); ++i) {
			map<string, string>::const_iterator found = inferred.find(definition.parameters[i].name);
			if(found != inferred.end()) result->push_back(found->second);
			else if(!definition.parameters[i].default_type.empty()) result->push_back(definition.parameters[i].default_type);
			else return false;
		}
		return true;
	}
	void RewriteOperatorFunctionArgument(const CPPGMAstNodePtr& expression,
		const string& context, const map<string, string>& substitutions)
	{
		if(!expression || expression->children.size() < 2 ||
			RemoveMarker(expression->value) != "<<") return;
		CPPGMAstNodePtr argument = expression->children[1];
		if(argument && argument->kind == "unary-expression" &&
			RemoveMarker(argument->value) == "&" && !argument->children.empty())
			argument = argument->children[0];
		if(!argument || argument->kind != "id-expression") return;
		string object_type;
		if(!InferArgument(expression->children[0], &object_type, substitutions, context)) return;
		CPPGMAstNodePtr declaration = FindClassDeclaration(object_type, context);
		if(!declaration) return;
		string expected;
		for(size_t i = 0; i < declaration->children.size(); ++i) {
			const CPPGMAstNodePtr child = declaration->children[i];
			if(!child || child->kind != "function-definition" || child->children.size() < 2 ||
				LastComponent(FirstIdentifierLocal(child->children[1])) != "operator<<") continue;
			const CPPGMAstNodePtr clause = DescendantOfKind(child->children[1], "parameter-clause");
			if(!clause || clause->children.empty()) continue;
			const CPPGMAstNodePtr parameter = clause->children[0];
			expected = FunctionTypeSpelling(parameter);
			break;
		}
		if(expected.empty()) return;
		expected = RewriteText(expected, context, substitutions, 0);
		const vector<const TemplateDefinition*> candidates =
			FindFunctionDefinitions(argument->value, context);
		for(size_t i = 0; i < candidates.size(); ++i) {
			vector<string> inferred;
			if(!InferFunctionFromExpected(*candidates[i], expected, &inferred, context)) continue;
			argument->value = Instantiate(*candidates[i], inferred, context);
			return;
		}
	}
	void ResolveMemberFunctionArguments(const CPPGMAstNodePtr& result,
		const string& context, const map<string, string>& substitutions);
	bool IsBuiltinLogicalType(string raw) const
	{
		raw = CanonicalSpelling(raw);
		while(raw.compare(0, 6, "const ") == 0)
			raw = CanonicalSpelling(raw.substr(6));
		while(raw.compare(0, 9, "volatile ") == 0)
			raw = CanonicalSpelling(raw.substr(9));
		if(raw.find('*') != string::npos) return true;
		return raw == "bool" || raw == "char" || raw == "signed char" ||
			raw == "unsigned char" || raw == "short" || raw == "short int" ||
			raw == "unsigned short" || raw == "unsigned short int" ||
			raw == "int" || raw == "unsigned" || raw == "unsigned int" ||
			raw == "long" || raw == "long int" || raw == "unsigned long" ||
			raw == "unsigned long int" || raw == "long long" ||
			raw == "long long int" || raw == "unsigned long long" ||
			raw == "unsigned long long int" || raw == "float" ||
			raw == "double" || raw == "long double" || raw == "nullptr_t";
	}
	void InstantiateOperatorTemplate(const CPPGMAstNodePtr& expression,
		const string& context, const map<string, string>& substitutions)
	{
		if(!expression || expression->children.size() < 2) return;
		const string operation = RemoveMarker(expression->value);
		if(operation.empty() || operation == ",") return;
		if((operation == "&&" || operation == "||") && expression->children.size() >= 2) {
			string left, right;
			if(InferArgument(expression->children[0], &left, substitutions, context) &&
				InferArgument(expression->children[1], &right, substitutions, context) &&
				IsBuiltinLogicalType(left) && IsBuiltinLogicalType(right)) return;
		}
		// A binary operator may be a member template.  Feed the left operand
		// through the normal member-call path so lookup uses the concrete owner
		// and the right operand is deduced as the member's sole argument.
		CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "."));
		member->children.push_back(expression->children[0]);
		member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier",
			"operator" + operation)));
		CPPGMAstNodePtr member_call(new CPPGMAstNode("call-expression"));
		member_call->children.push_back(member);
		CPPGMAstNodePtr member_arguments(new CPPGMAstNode("argument-list"));
		member_arguments->children.push_back(expression->children[1]);
		member_call->children.push_back(member_arguments);
		if(InstantiateMemberCall(member_call, member, "operator" + operation,
			context, substitutions)) return;
		const vector<const TemplateDefinition*> candidates = FindFunctionDefinitions(
			"operator" + operation, context);
		if(candidates.empty()) return;
		CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
		call->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
			"id-expression", "operator" + operation)));
		CPPGMAstNodePtr arguments(new CPPGMAstNode("argument-list"));
		arguments->children.push_back(expression->children[0]);
		arguments->children.push_back(expression->children[1]);
		call->children.push_back(arguments);
		for(size_t i = 0; i < candidates.size(); ++i) {
			// A forward declaration and its later definition are both indexed as
			// function templates.  Replay the definition when the signatures match;
			// otherwise operator discovery can materialize only the declaration and
			// lose the body (including accesses made by a friend definition).
			if(candidates[i]->declaration && candidates[i]->declaration->kind ==
				"simple-declaration") {
				bool has_definition = false;
				for(size_t other = 0; other < candidates.size(); ++other) {
					const TemplateDefinition* replacement = candidates[other];
					if(replacement == candidates[i] || !replacement->declaration ||
						replacement->declaration->kind != "function-definition") continue;
					if(MemberSignatureKey(*replacement) ==
						MemberSignatureKey(*candidates[i])) {
						has_definition = true;
						break;
					}
				}
				if(has_definition) continue;
			}
			vector<string> inferred;
			map<string, vector<string> > inferred_pack_values;
			map<string, FunctionSignature> inferred_function_values;
			if(!InferFunctionArguments(*candidates[i], call, &inferred,
				substitutions, context, 0, &inferred_pack_values,
				&inferred_function_values)) continue;
			Instantiate(*candidates[i], inferred, context, false, &inferred_pack_values,
				0, 0, &inferred_function_values);
			return;
		}
	}
	string RewriteDecltypeText(string raw, const string& context,
		const map<string, string>& substitutions, bool* template_replaced)
	{
		for(size_t search = 0; search + 9 <= raw.size(); ++search) {
			const size_t decltype_start = raw.find("decltype(", search);
			if(decltype_start == string::npos) break;
			search = decltype_start;
			int depth = 0;
			size_t close = string::npos;
			for(size_t i = search + 8; i < raw.size(); ++i) {
				if(raw[i] == '(') ++depth;
				else if(raw[i] == ')' && --depth == 0) {
					close = i;
					break;
				}
			}
			if(close == string::npos) continue;
			const string expression = CanonicalSpelling(raw.substr(search + 9,
				close - search - 9));
			string type;
			if(!EvaluateDecltypeExpression(expression, context, substitutions, &type) ||
				type.empty()) {
				search = close;
				continue;
			}
			raw.replace(search, close - search + 1, type);
			if(template_replaced) *template_replaced = true;
			search += type.size();
		}
		return raw;
	}
	string ExpandPackCallText(string raw,
		const map<string, vector<string> >& packs) const
	{
		// Textual unevaluated operands such as `declval<Args>()...` are
		// encountered while resolving a default template argument, before the
		// declaration AST is transformed.  Expand that complete call here so
		// the ordinary template-id pass never tries to instantiate a dependent
		// `declval<Args>` specialization.
		for(size_t search = 0; search < raw.size(); ++search) {
			if(raw[search] != '<') continue;
			size_t begin = 0, close = string::npos;
			string base, arguments;
			if(!TemplateBase(raw, search, &begin, &base) ||
				!TemplateRange(raw, search, &arguments, &close)) continue;
			const vector<string> values = SplitTemplateArguments(arguments);
			if(values.size() != 1) continue;
			const string pack_name = CanonicalSpelling(values[0]);
			map<string, vector<string> >::const_iterator pack = packs.find(pack_name);
			if(pack == packs.end() || close + 5 >= raw.size() ||
				raw[close + 1] != '(' || raw[close + 2] != ')' ||
				raw.compare(close + 3, 3, "...") != 0) continue;
			string expansion;
			for(size_t value = 0; value < pack->second.size(); ++value) {
				if(!expansion.empty()) expansion += ',';
				expansion += base + "<" + pack->second[value] + ">()";
			}
			raw.replace(begin, close + 6 - begin, expansion);
			search = begin + expansion.size();
		}
		return raw;
	}
	string RewriteText(string raw, const string& context,
		const map<string, string>& substitutions, bool* template_replaced,
		bool resolve_alias = true, bool resolve_member = true);
	bool RewriteConcreteNestedMember(string* raw, size_t begin, size_t close,
		const string& base, const string& context,
		const map<string, string>& substitutions, bool* template_replaced,
		size_t* search);
	void ResolveFunctionArguments(const CPPGMAstNodePtr& result,
		const FunctionSignature* signature, const string& context)
	{
		if(!signature || !signature->parameters || result->children.size() < 2 ||
			!result->children[1] || result->children[1]->kind != "argument-list") return;
		const CPPGMAstNodePtr result_arguments = result->children[1];
		size_t argument = 0;
		for(size_t parameter = 0; parameter < signature->parameters->children.size() &&
			argument < result_arguments->children.size(); ++parameter) {
			const CPPGMAstNodePtr parameter_node = signature->parameters->children[parameter];
			if(!parameter_node || parameter_node->kind != "parameter-declaration") continue;
			const string expected = FunctionTypeSpelling(parameter_node);
			CPPGMAstNodePtr argument_node = result_arguments->children[argument];
			if(argument_node && argument_node->kind == "unary-expression" &&
				RemoveMarker(argument_node->value) == "&" && !argument_node->children.empty())
				argument_node = argument_node->children[0];
			if(argument_node && argument_node->kind == "id-expression") {
				const vector<const TemplateDefinition*> function_candidates =
					FindFunctionDefinitions(argument_node->value, context);
				for(size_t candidate = 0; candidate < function_candidates.size(); ++candidate) {
					vector<string> inferred;
					if(InferFunctionFromExpected(*function_candidates[candidate],
						expected, &inferred, context)) {
						const string local_name = Instantiate(*function_candidates[candidate], inferred, context);
						result_arguments->children[argument]->value = local_name;
						break;
					}
				}
			}
			++argument;
		}
	}
	void ResolveClassConstructorFunctionArguments(const CPPGMAstNodePtr& result,
		const string& context)
	{
		if(!result || result->children.size() < 2 || !result->children[0] ||
			result->children[0]->kind != "id-expression" || !result->children[1] ||
			result->children[1]->kind != "argument-list") return;
		const CPPGMAstNodePtr declaration = FindClassDeclaration(
			result->children[0]->value, context);
		if(!declaration) return;
		const CPPGMAstNodePtr arguments = result->children[1];
		for(size_t member = 0; member < declaration->children.size(); ++member) {
			const CPPGMAstNodePtr constructor = declaration->children[member];
			if(!constructor || (constructor->kind != "special-member-definition" &&
				constructor->kind != "special-member-declaration")) continue;
			const CPPGMAstNodePtr clause = DescendantOfKind(
				FunctionDeclarator(constructor), "parameter-clause");
			if(!clause) continue;
			size_t argument = 0;
			for(size_t parameter = 0; parameter < clause->children.size() &&
				argument < arguments->children.size(); ++parameter) {
				const CPPGMAstNodePtr parameter_node = clause->children[parameter];
				if(!parameter_node || parameter_node->kind != "parameter-declaration") continue;
				const string expected = FunctionTypeSpelling(parameter_node);
				CPPGMAstNodePtr original_argument = arguments->children[argument];
				CPPGMAstNodePtr function_argument = original_argument;
				if(function_argument && function_argument->kind == "unary-expression" &&
					RemoveMarker(function_argument->value) == "&" &&
					!function_argument->children.empty())
					function_argument = function_argument->children[0];
				if(function_argument && function_argument->kind == "id-expression" &&
					!expected.empty()) {
					const vector<const TemplateDefinition*> candidates =
						FindFunctionDefinitions(function_argument->value, context);
					for(size_t candidate = 0; candidate < candidates.size(); ++candidate) {
						vector<string> inferred;
						if(!InferFunctionFromExpected(*candidates[candidate], expected,
							&inferred, context)) continue;
						const string local_name = Instantiate(*candidates[candidate], inferred, context);
						const string qualified_name = PrefixComponent(
							candidates[candidate]->qualified_name).empty() ? local_name :
							PrefixComponent(candidates[candidate]->qualified_name) + "::" + local_name;
						if(original_argument && original_argument->kind == "unary-expression" &&
							!original_argument->children.empty())
							original_argument->children[0]->value = qualified_name;
						else if(original_argument) original_argument->value = qualified_name;
						break;
					}
				}
				++argument;
			}
			return;
		}
	}
	bool SkipUnusedNestedClass(const CPPGMAstNodePtr& input,
		const CPPGMAstNodePtr& original_child, const string& child_context,
		const map<string, string>& substitutions, size_t index) const
	{
		if(!input || !original_child ||
			(input->kind != "class-specifier" && input->kind != "class-forward-declaration") ||
			(PrefixComponent(input->value).find('<') == string::npos &&
				!HasReplayContext(substitutions)) ||
			(original_child->kind != "class-specifier" &&
				original_child->kind != "class-forward-declaration")) return false;
		// An unnamed aggregate is a data member of the enclosing class, not an
		// optional nested type.  Keep it in a specialization so its storage and
		// injected member bindings survive template replay.
		if(original_child->value.empty() || original_child->value == "<unnamed>") return false;
		const string nested_name = LastComponent(original_child->value);
		const string class_key = JoinPath(child_context, LastComponent(input->value));
		map<string, set<string> >::const_iterator requested = requested_nested_classes_.find(class_key);
		if(requested == requested_nested_classes_.end())
			requested = requested_nested_classes_.find(LastComponent(input->value));
		if(requested != requested_nested_classes_.end() &&
			requested->second.find(nested_name) != requested->second.end()) return false;
		for(size_t sibling = 0; sibling < input->children.size(); ++sibling)
			if(sibling != index && input->children[sibling] &&
				input->children[sibling]->kind != "class-key" &&
				ContainsName(input->children[sibling], nested_name)) return false;
		bool has_sibling = false;
		for(size_t sibling = 0; sibling < input->children.size(); ++sibling)
			if(sibling != index && input->children[sibling] &&
				input->children[sibling]->kind != "class-key") has_sibling = true;
		return has_sibling;
	}
	void RemoveParameterPackMarkers(const CPPGMAstNodePtr& node) const
	{
		if(!node) return;
		for(size_t i = 0; i < node->children.size();) {
			if(node->children[i] && (node->children[i]->kind == "parameter-pack" ||
				node->children[i]->kind == "pack-expansion"))
				node->children.erase(node->children.begin() + i);
			else {
				RemoveParameterPackMarkers(node->children[i]);
				++i;
			}
		}
	}
	void CollapseForwardingReference(const CPPGMAstNodePtr& node) const
	{
		if(!node) return;
		const string declarator_kind = node->kind == "parameter-declaration" ?
			"declarator" : "abstract-declarator";
		const CPPGMAstNodePtr declarator = ChildOfKindLocal(node, declarator_kind);
		if(!declarator) return;
		for(size_t i = 0; i < declarator->children.size(); ++i) {
			const CPPGMAstNodePtr child = declarator->children[i];
			if(child && child->kind == "ptr-operator" && child->value.find("&&") != string::npos) {
				declarator->children.erase(declarator->children.begin() + i);
				return;
			}
		}
	}
	bool RenameParameterIdentifier(const CPPGMAstNodePtr& node,
		const string& name) const
	{
		if(!node) return false;
		if(node->kind == "identifier") {
			node->value = name;
			return true;
		}
		for(size_t i = 0; i < node->children.size(); ++i)
			if(RenameParameterIdentifier(node->children[i], name)) return true;
		return false;
	}
	string ParameterIdentifier(const CPPGMAstNodePtr& parameter) const
	{
		if(!parameter) return string();
		for(size_t i = 0; i < parameter->children.size(); ++i) {
			const CPPGMAstNodePtr child = parameter->children[i];
			if(!child || child->kind != "declarator") continue;
			return FirstIdentifierLocal(child);
		}
		return string();
	}
	string PackExpansionIdentifier(const CPPGMAstNodePtr& node) const
	{
		if(!node) return string();
		if(node->kind == "id-expression") {
			const string value = RemoveMarker(node->value);
			for(size_t i = 0; i < value.size();) {
				if(!IsIdentifierCharacter(value[i])) { ++i; continue; }
				const size_t begin = i;
				while(i < value.size() && IsIdentifierCharacter(value[i])) ++i;
				const string word = value.substr(begin, i - begin);
				if(active_pack_substitutions_.find(word) != active_pack_substitutions_.end() ||
					active_pack_identifier_substitutions_.find(word) !=
						active_pack_identifier_substitutions_.end()) return word;
			}
			return LastComponent(value);
		}
		if(node->kind == "type-name" || node->kind == "decl-specifier" ||
			node->kind == "type-specifier") {
			const string value = RemoveMarker(node->value);
			for(size_t i = 0; i < value.size();) {
				if(!IsIdentifierCharacter(value[i])) { ++i; continue; }
				const size_t begin = i;
				while(i < value.size() && IsIdentifierCharacter(value[i])) ++i;
				const string word = value.substr(begin, i - begin);
				if(active_pack_substitutions_.find(word) != active_pack_substitutions_.end() ||
					active_pack_identifier_substitutions_.find(word) !=
						active_pack_identifier_substitutions_.end()) return word;
			}
			return LastComponent(value);
		}
		for(size_t i = 0; i < node->children.size(); ++i) {
			const string found = PackExpansionIdentifier(node->children[i]);
			if(!found.empty()) return found;
		}
		return string();
	}
	string UserDefinedIntegerSuffix(const string& raw) const
	{
		if(raw.empty() || !isdigit(static_cast<unsigned char>(raw[0]))) return string();
		const size_t underscore = raw.find('_');
		if(underscore == string::npos || underscore + 1 >= raw.size()) return string();
		for(size_t i = underscore + 1; i < raw.size(); ++i)
			if(!IsIdentifierCharacter(raw[i])) return string();
		return raw.substr(underscore);
	}
	CPPGMAstNodePtr RewriteUserDefinedIntegerLiteral(
		const CPPGMAstNodePtr& input, const string& context,
		const map<string, string>& substitutions)
	{
		if(!input || input->kind != "literal") return CPPGMAstNodePtr();
		const string suffix = UserDefinedIntegerSuffix(input->value);
		if(suffix.empty()) return CPPGMAstNodePtr();
		const string operator_name = "operator\"\"" + suffix;
		const vector<const TemplateDefinition*> candidates =
			FindFunctionDefinitions(operator_name, context);
		const string core = input->value.substr(0, input->value.size() - suffix.size());
		const TemplateDefinition* character_pack = 0;
		for(size_t i = 0; i < candidates.size(); ++i) {
			const TemplateDefinition* candidate = candidates[i];
			if(candidate->parameters.size() != 1 || !candidate->parameters[0].pack ||
				candidate->parameters[0].type ||
				CanonicalSpelling(candidate->parameters[0].non_type_type) != "char") continue;
			character_pack = candidate;
			break;
		}
		string callee = operator_name;
		if(character_pack) {
			callee += "<";
			for(size_t i = 0; i < core.size(); ++i) {
				if(i) callee += ",";
				callee += "'";
				if(core[i] == '\\' || core[i] == '\'') callee += '\\';
				callee += core[i];
				callee += "'";
			}
			callee += ">";
		}
		CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
		call->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
			"id-expression", callee)));
		CPPGMAstNodePtr arguments(new CPPGMAstNode("argument-list"));
		if(!character_pack)
			arguments->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
				"literal", core)));
		call->children.push_back(arguments);
		return TransformNode(call, context, substitutions);
	}
	void ExpandPackChild(const CPPGMAstNodePtr& input,
		const CPPGMAstNodePtr& original_child, const string& child_context,
		const map<string, string>& substitutions,
		map<string, string>* local_substitutions,
		const CPPGMAstNodePtr& result)
	{
		if(!original_child || original_child->kind != "pack-expansion-expression" ||
			original_child->children.empty()) return;
		const string name = PackExpansionIdentifier(original_child->children[0]);
		map<string, vector<string> >::const_iterator typed_pack =
			active_pack_substitutions_.find(name);
		map<string, vector<string> >::const_iterator named_pack =
			active_pack_identifier_substitutions_.find(name);
		const vector<string>* values = typed_pack != active_pack_substitutions_.end() ?
			&typed_pack->second : named_pack != active_pack_identifier_substitutions_.end() ?
			&named_pack->second : 0;
		if(!values) return;
		if(named_pack != active_pack_identifier_substitutions_.end() &&
			active_pack_substitutions_.find(name) == active_pack_substitutions_.end())
			values = &named_pack->second;
		const map<string, PA19IntegralValue> saved_integrals =
			active_integral_substitutions_;
		for(size_t i = 0; i < values->size(); ++i) {
			map<string, string> one = substitutions;
			one[name] = (*values)[i];
			// A function parameter pack has two identities in the AST: its
			// declared type pack (`Args`) and its parameter identifier (`args`).
			// The parameter-clause replay records the concrete identifier for each
			// element; carry the same element index into the expanded expression so
			// `static_cast<Args&&>(args)...` selects `args__pack2` on its second
			// expansion instead of reusing the first parameter.
			for(map<string, vector<string> >::const_iterator identifier =
				active_pack_identifier_substitutions_.begin();
				identifier != active_pack_identifier_substitutions_.end(); ++identifier)
				if(i < identifier->second.size())
					one[identifier->first] = identifier->second[i];
			// A single expansion can depend on more than one pack, for
			// example `identity<Args>(get<I>(value))...`.  Select the same
			// element for every active integral pack before transforming the
			// child, even when the syntactic expansion name is the type pack.
			active_integral_substitutions_ = saved_integrals;
			for(map<string, PA19IntegralValue>::const_iterator integral =
				saved_integrals.begin(); integral != saved_integrals.end(); ++integral) {
				map<string, vector<string> >::const_iterator integral_pack =
					active_pack_substitutions_.find(integral->first);
				if(integral_pack == active_pack_substitutions_.end() ||
					i >= integral_pack->second.size()) continue;
				PA19IntegralValue element;
				if(PA19ParseInteger(integral_pack->second[i], &element))
					active_integral_substitutions_[integral->first] =
						PA19Convert(element, integral->second.type);
			}
			CPPGMAstNodePtr child = TransformNode(original_child->children[0],
				child_context, one);
			if(child) result->children.push_back(child);
		}
		active_integral_substitutions_ = saved_integrals;
		(void)input;
		(void)local_substitutions;
	}
void TransformRegularChildren(const CPPGMAstNodePtr& input,
		const string& child_context, const string& function_context,
		const map<string, string>& substitutions,
		map<string, string>* local_substitutions,
		const CPPGMAstNodePtr& result);
	bool TransformPackChild(const CPPGMAstNodePtr& input,
		const CPPGMAstNodePtr& original_child, const string& child_context,
		const map<string, string>& substitutions,
		map<string, string>* local_substitutions,
		const CPPGMAstNodePtr& result);
	void RecordUsingDirective(const CPPGMAstNodePtr& original_child,
		map<string, string>* local_substitutions);
	void RecordTypedefSubstitutions(const CPPGMAstNodePtr& original_child,
		const string& child_context, map<string, string>* local_substitutions)
	{
		const CPPGMAstNodePtr list = ChildOfKindLocal(original_child, "init-declarator-list");
		if(!list) return;
		for(size_t item_index = 0; item_index < list->children.size(); ++item_index) {
			const CPPGMAstNodePtr item = list->children[item_index];
			if(!item || item->children.empty()) continue;
			const string alias_name = FirstIdentifierLocal(item->children[0]);
			if(!alias_name.empty()) {
				const string raw_type = DeclaratorTypeSpelling(
					NodeTypeSpelling(original_child->children[0]), item->children[0]);
				const string rewritten_type = RewriteText(
					raw_type, child_context, *local_substitutions, 0);
				if(raw_type.empty() || rewritten_type.empty()) continue;
				(*local_substitutions)[alias_name] = rewritten_type;
				const string qualified_alias = JoinPath(child_context, alias_name);
				type_aliases_[qualified_alias] = (*local_substitutions)[alias_name];
				vector<string>& aliases = type_aliases_by_name_[alias_name];
				if(find(aliases.begin(), aliases.end(), qualified_alias) == aliases.end())
					aliases.push_back(qualified_alias);
			}
		}
	}
	void RewriteTemplateInitializer(const CPPGMAstNodePtr& input,
		const string& context, const map<string, string>& substitutions,
		const CPPGMAstNodePtr& result)
	{
		if(!input || !result || input->kind != "simple-declaration") return;
		const CPPGMAstNodePtr original_list = ChildOfKindLocal(input, "init-declarator-list");
		const CPPGMAstNodePtr transformed_list = ChildOfKindLocal(result, "init-declarator-list");
		if(!original_list || !transformed_list || original_list->children.size() != 1 ||
			transformed_list->children.size() != 1) return;
		const CPPGMAstNodePtr original_item = original_list->children[0];
		const CPPGMAstNodePtr transformed_item = transformed_list->children[0];
		if(!original_item || !transformed_item || original_item->children.empty() ||
			transformed_item->children.empty()) return;
		const CPPGMAstNodePtr original_declarator = original_item->children[0];
		const CPPGMAstNodePtr transformed_declarator = transformed_item->children[0];
		const CPPGMAstNodePtr parameter_clause = ChildOfKindLocal(
			original_declarator, "parameter-clause");
		if(!parameter_clause || parameter_clause->children.size() != 1 ||
			!parameter_clause->children[0]) return;
		const CPPGMAstNodePtr parameter = parameter_clause->children[0];
		const CPPGMAstNodePtr parameter_specs = parameter->children.empty() ?
			CPPGMAstNodePtr() : parameter->children[0];
		if(!parameter_specs || parameter_specs->children.empty()) return;
		const string raw_type = RemoveMarker(parameter_specs->children[0]->value);
		const size_t open = raw_type.find('<');
		if(open == string::npos) return;
		string arguments;
		size_t close = string::npos;
		string base;
		size_t begin = 0;
		if(!TemplateBase(raw_type, open, &begin, &base) ||
			!TemplateRange(raw_type, open, &arguments, &close) ||
			!FindDefinition(base, context)) return;
		const string callee = RewriteText(raw_type, context, substitutions, 0);
		if(callee.empty() || callee == raw_type) return;
		string argument_name = FirstIdentifierLocal(parameter->children.size() > 1 ?
			parameter->children[1] : CPPGMAstNodePtr());
		if(argument_name.empty()) return;
		CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
		call->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
			"id-expression", callee)));
		CPPGMAstNodePtr call_arguments(new CPPGMAstNode("argument-list"));
		CPPGMAstNodePtr argument(new CPPGMAstNode("id-expression", argument_name));
		const CPPGMAstNodePtr nested_pointer = DescendantOfKind(parameter->children.size() > 1 ? parameter->children[1] : CPPGMAstNodePtr(), "ptr-operator");
		if(nested_pointer && RemoveMarker(nested_pointer->value) == "*") { CPPGMAstNodePtr dereference(new CPPGMAstNode("unary-expression", "OP_STAR:*")); dereference->children.push_back(argument); argument = dereference; }
		call_arguments->children.push_back(argument); call->children.push_back(call_arguments);
		CPPGMAstNodePtr transformed_call = TransformNode(call, context, substitutions);
		if(transformed_call) call = transformed_call;
		for(size_t i = 0; i < transformed_declarator->children.size();) {
			if(transformed_declarator->children[i] &&
				transformed_declarator->children[i]->kind == "parameter-clause")
				transformed_declarator->children.erase(
					transformed_declarator->children.begin() + i);
			else ++i;
		}
		transformed_item->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("initializer")));
		transformed_item->children.back()->children.push_back(call);
	}
	CPPGMAstNodePtr TransformRegularNode(const CPPGMAstNodePtr& input,
		const string& context, const map<string, string>& substitutions);
	CPPGMAstNodePtr RewriteRegularNodeValue(const CPPGMAstNodePtr& input,
		const string& context, const map<string, string>& substitutions,
		const CPPGMAstNodePtr& result, string* promoted_name);
	bool PreserveDependentStaticDeclarator(const CPPGMAstNodePtr& input,
		const string& context, const map<string, string>& substitutions,
		const CPPGMAstNodePtr& result, string* promoted_name);
	CPPGMAstNodePtr FinishRegularNode(const CPPGMAstNodePtr& input,
		const string& context, const map<string, string>& substitutions,
		const CPPGMAstNodePtr& result, const string& promoted_local_class);
	bool TransformExplicitSpecialization(const CPPGMAstNodePtr& input,
		const string& context, const map<string, string>& substitutions);
	void CheckExplicitSpecializationOrder(const CPPGMAstNodePtr& input,
		const string& context);
	CPPGMAstNodePtr TransformNode(const CPPGMAstNodePtr& input, const string& context,
		const map<string, string>& substitutions)
	{
		if(!input) return CPPGMAstNodePtr();
		if(input->kind == "explicit-instantiation-declaration" &&
			!input->children.empty() && input->children[0]) {
			const CPPGMAstNodePtr target = input->children[0];
			target->explicit_instantiation = input->explicit_instantiation;
			target->extern_instantiation = input->extern_instantiation;
			if(target->kind == "class-forward-declaration" ||
				target->kind == "class-specifier") {
				const string raw = RemoveMarker(target->value);
				const size_t open = raw.find('<');
				string base, arguments;
				size_t begin = 0, close = string::npos;
				if(open == string::npos || !TemplateBase(raw, open, &begin, &base) ||
					!TemplateRange(raw, open, &arguments, &close))
					throw logic_error("explicit instantiation is not a template-id");
				const TemplateDefinition* definition = FindDefinition(base, context);
				if(!definition || !definition->class_template)
					throw logic_error("explicit instantiation names no class template");
				if(input->explicit_instantiation)
					Instantiate(*definition, SplitTemplateArguments(arguments), context, true);
				// A valid extern class instantiation suppresses an emitted definition;
				// it is still accepted only after the typed class entity was found.
				return CPPGMAstNodePtr();
			}
			if(!input->explicit_instantiation) {
				if(!MaterializeExplicitInstantiation(target, context, true))
					throw logic_error("extern template names no function template");
				return CPPGMAstNodePtr();
			}
			if(!MaterializeExplicitInstantiation(target, context, false))
				throw logic_error("explicit instantiation names no function template");
			return CPPGMAstNodePtr();
		}
		if(input->kind == "template-declaration") {
			CheckExplicitSpecializationOrder(input, context);
			if(TransformExplicitSpecialization(input, context, substitutions))
				return CPPGMAstNodePtr();
			// Preserve a friend template declaration with its complete typed
			// declarator.  The semantic pass records the friend edge from the
			// normal template-parameter scope; no synthetic empty signature is
			// needed and PA14 naturally ignores declarations without bodies.
			if(input->children.size() > 1 && input->children[1] &&
				input->children[1]->kind == "simple-declaration" &&
				!input->children[1]->children.empty() &&
				HasFriendSpecifier(input->children[1]->children[0]) &&
				DescendantOfKind(input->children[1], "parameter-clause"))
				return TransformRegularNode(input, context, substitutions);
			if(input->children.size() > 1 && input->children[1] &&
				(input->children[1]->kind == "class-specifier" ||
				 input->children[1]->kind == "class-forward-declaration"))
				return PrefixComponent(input->children[1]->value).find('<') == string::npos ?
					MakeClassShell(LastComponent(input->children[1]->value).substr(0,
						LastComponent(input->children[1]->value).find('<'))) :
					CPPGMAstNodePtr();
			if(input->children.size() > 1 && input->children[1] &&
				input->children[1]->kind == "function-definition" && !context.empty()) {
				const string member = LastComponent(DeclarationName(input->children[1]));
				if(using_member_template_names_.find(member) != using_member_template_names_.end()) {
					// PA11 needs the template entity for using-declaration lookup. Keep the real dependent signature, but expose it as a declaration so
					// PA14 never lowers the source body; concrete calls use Instantiate().
					CPPGMAstNodePtr declaration = CloneNode(input);
					CPPGMAstNodePtr function = declaration->children[1];
					if(function->children.size() > 1) {
						CPPGMAstNodePtr simple(new CPPGMAstNode("simple-declaration"));
						simple->children.push_back(CloneNode(function->children[0]));
						CPPGMAstNodePtr list(new CPPGMAstNode("init-declarator-list"));
						CPPGMAstNodePtr item(new CPPGMAstNode("init-declarator"));
						item->children.push_back(CloneNode(function->children[1]));
						list->children.push_back(item);
						simple->children.push_back(list);
						declaration->children[1] = simple;
					}
					return declaration;
				}
			}
			return CPPGMAstNodePtr();
		}
		if(input->kind == "using-declaration") {
			const CPPGMAstNodePtr target = ChildOfKindLocal(input, "target");
			if(target && IsOrdinaryTemplateUsingTarget(target->value, context) &&
				class_contexts_.find(context) != class_contexts_.end() &&
				!IsGeneratedMemberTemplateUsingTarget(target->value, context, substitutions))
				return CPPGMAstNodePtr();
		}
		if(input->kind == "parameter-declaration" && !input->children.empty() &&
			input->children[0] && input->children[0]->kind == "decl-specifier-seq") {
			for(size_t i = 0; i < input->children[0]->children.size(); ++i) {
				const CPPGMAstNodePtr specifier = input->children[0]->children[i];
				if(!specifier) continue;
				const string name = RemoveMarker(specifier->value);
				map<string, string>::const_iterator substitution = substitutions.find(name);
				if(substitution == substitutions.end()) continue;
				map<string, FunctionSignature>::const_iterator signature =
					active_function_substitutions_.find(name);
				if(signature != active_function_substitutions_.end())
					return FunctionParameter(input, signature->second);
			}
		}
		if(input->kind == "namespace-definition") return TransformNamespace(input, context, substitutions);
		if(input->kind == "call-expression") return TransformCallExpression(input, context, substitutions);
		return TransformRegularNode(input, context, substitutions);
	}
};
} // namespace pa18_templates_internal
