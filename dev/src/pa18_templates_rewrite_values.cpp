#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

	bool PA18TemplateExpander::EvaluateMaterializedTemplateValue(const string& raw,
		const string& context, const map<string, string>& substitutions,
		PA19IntegralValue* result)
	{
		// A transformed dependent expression can retain the source template-id
		// (`Trait<Arg>::value`) even though its concrete class has already been
		// materialized under a generated name.  Redirect that spelling before
		// resolving inherited members.
		const size_t template_value_separator = raw.rfind("::");
		if(template_value_separator != string::npos &&
			raw.substr(template_value_separator + 2) == "value" &&
			raw.find("&&") == string::npos && raw.find("||") == string::npos &&
			raw.find("!") == string::npos) {
			const string template_owner = raw.substr(0, template_value_separator);
			const size_t open = template_owner.find('<');
			if(open != string::npos) {
				string argument_text;
				size_t close = string::npos;
				if(TemplateRange(template_owner, open, &argument_text, &close)) {
					const string primary = CanonicalSpelling(template_owner.substr(0, open));
					const vector<string> arguments = SplitTemplateArguments(argument_text);
					for(map<string,vector<string> >::const_iterator candidate =
						specialization_arguments_.begin(); candidate != specialization_arguments_.end();
						++candidate) {
						map<string,string>::const_iterator candidate_base =
							specialization_bases_.find(candidate->first);
						if(candidate_base == specialization_bases_.end() ||
							candidate->second.size() != arguments.size()) continue;
						bool primary_matches = candidate_base->second == primary ||
							LastComponent(candidate_base->second) == LastComponent(primary);
						if(!primary_matches) for(map<string,string>::const_iterator substitution =
							substitutions.begin(); substitution != substitutions.end(); ++substitution)
							if(CanonicalSpelling(substitution->second) == primary &&
								(candidate_base->second == substitution->first ||
								 LastComponent(candidate_base->second) == LastComponent(substitution->first))) {
								primary_matches = true;
								break;
							}
						if(!primary_matches) continue;
						bool same = true;
						for(size_t argument = 0; argument < arguments.size(); ++argument)
							if(NormalizeTypeArgument(CanonicalSpelling(arguments[argument])) !=
								NormalizeTypeArgument(CanonicalSpelling(candidate->second[argument]))) {
								same = false;
								break;
							}
						if(same && candidate->first + "::value" != raw &&
							EvaluateIntegralText(candidate->first + "::value", context,
								substitutions, result)) return true;
					}
				}
			}
		}
	return false;
	}
	bool PA18TemplateExpander::ExpandIntegralValueOperands(const string& raw,
		const string& context, const map<string, string>& substitutions,
		PA19IntegralValue* result)
	{
		// Resolve qualified generated `::value` operands independently before
		// parsing the surrounding boolean/arithmetic expression.  `rfind("::")`
		// cannot identify the owner of a compound expression such as
		// `A::value && B::value`.
		string expanded = raw;
		bool expanded_any = false;
	for(size_t marker = expanded.find("::value"); marker != string::npos; ) {
		size_t begin = marker;
		if(begin > 0 && expanded[begin - 1] == '>') {
			int angle_depth = 0;
			while(begin > 0) {
				const char character = expanded[begin - 1];
				if(character == '>') ++angle_depth;
				else if(character == '<' && angle_depth > 0) {
					--angle_depth;
					--begin;
					if(angle_depth == 0) break;
					continue;
				}
				--begin;
			}
		}
		while(begin > 0 && (IsIdentifierCharacter(expanded[begin - 1]) ||
			expanded[begin - 1] == ':')) --begin;
			const size_t length = marker + 7 - begin;
			const string operand = expanded.substr(begin, length);
			PA19IntegralValue operand_value;
			if(operand.empty() || operand == expanded ||
				!EvaluateIntegralText(operand, context, substitutions, &operand_value)) {
				marker = expanded.find("::value", marker + 7);
				continue;
			}
			expanded.replace(begin, length, IntegralValueSpelling(operand_value));
			expanded_any = true;
			marker = expanded.find("::value", begin);
		}
		if(expanded_any) {
			PA19ConstantExpressionParser expanded_parser(constant_values_, substitutions,
				constant_type_sizes_, constant_type_alignments_, type_aliases_);
			if(expanded_parser.Evaluate(expanded, result)) return true;
		}
	return false;
	}
	bool PA18TemplateExpander::EvaluateInheritedBaseValue(const TemplateDefinition& definition,
		const string& context, const map<string, string>& member_substitutions,
		PA19IntegralValue* result)
	{
					for(size_t child_index = 0;
						child_index < definition.declaration->children.size(); ++child_index) {
						const CPPGMAstNodePtr clause = definition.declaration->children[child_index];
						if(!clause || clause->kind != "base-clause") continue;
		for(size_t base_index = 0; base_index < clause->children.size(); ++base_index) {
			const CPPGMAstNodePtr base_name = ChildOfKindLocal(
				clause->children[base_index], "base-name");
			if(!base_name) continue;
				map<string, string> base_substitutions = member_substitutions;
			for(map<string, string>::const_iterator substitution = member_substitutions.begin();
				substitution != member_substitutions.end(); ++substitution)
				for(size_t at = base_name->value.find(substitution->first); at != string::npos;
					at = base_name->value.find(substitution->first, at + substitution->first.size())) {
					if(at > 0 && IsIdentifierCharacter(base_name->value[at - 1])) continue;
					size_t after = at + substitution->first.size();
					while(after < base_name->value.size() &&
						isspace(static_cast<unsigned char>(base_name->value[after]))) ++after;
					if(after < base_name->value.size() && base_name->value[after] == '<') {
						base_substitutions.erase(substitution->first);
						break;
					}
				}
				string base_spelling = CanonicalSpelling(ReplaceIdentifiers(
					base_name->value, base_substitutions));
				base_spelling = CanonicalSpelling(RemoveMarker(RewriteText(
					base_spelling, context, base_substitutions, 0)));
				base_spelling = ResolveAlias(base_spelling, context);
							string base_primary = base_spelling;
							vector<string> base_arguments;
							const size_t open = base_spelling.find('<');
							if(open != string::npos) {
								string argument_text;
								size_t close = string::npos;
								if(!TemplateRange(base_spelling, open, &argument_text, &close)) continue;
								base_primary = CanonicalSpelling(base_spelling.substr(0, open));
								base_arguments = SplitTemplateArguments(argument_text);
								const TemplateDefinition* base_definition = FindDefinition(
									base_primary, context);
								for(size_t argument = 0; argument < base_arguments.size(); ++argument) {
									base_arguments[argument] = RewriteText(base_arguments[argument], context,
										member_substitutions, 0, false, false);
									base_arguments[argument] = CanonicalSpelling(ReplaceIdentifiers(
										base_arguments[argument], member_substitutions));
									if(base_definition && argument < base_definition->parameters.size() &&
										!base_definition->parameters[argument].type) {
										PA19IntegralValue argument_value;
						const bool evaluated_argument = EvaluateIntegralText(
							base_arguments[argument], context, member_substitutions,
							&argument_value);
										if(evaluated_argument) {
											PA19IntegralValue normalized_value = argument_value;
											map<string,string> expected_substitutions = member_substitutions;
											for(size_t parameter = 0;
												parameter < base_definition->parameters.size() &&
												parameter < base_arguments.size(); ++parameter)
												if(base_definition->parameters[parameter].type &&
													!base_definition->parameters[parameter].name.empty())
													expected_substitutions[base_definition->parameters[parameter].name] =
														base_arguments[parameter];
											string expected_type = ReplaceIdentifiers(
												base_definition->parameters[argument].non_type_type,
												expected_substitutions);
											expected_type = ResolveAlias(expected_type, context);
											const PA19IntegralType typed = PA19Type(expected_type);
											if(typed.integral) normalized_value = PA19Convert(
												normalized_value, typed);
										base_arguments[argument] = TemplateIntegralValueSpelling(normalized_value);
										} else base_arguments[argument] = ResolveAlias(
											base_arguments[argument], context);
									}
								}
							}
							const TemplateDefinition* base_definition = FindDefinition(
								base_primary, context);
			if(base_definition) base_primary = base_definition->qualified_name;
							string materialized_base;
							for(map<string, vector<string> >::const_iterator candidate =
								specialization_arguments_.begin();
								candidate != specialization_arguments_.end(); ++candidate) {
								map<string, string>::const_iterator candidate_base =
									specialization_bases_.find(candidate->first);
								if(candidate_base == specialization_bases_.end() ||
									candidate_base->second != base_primary ||
									candidate->second.size() != base_arguments.size()) continue;
								bool same = true;
								for(size_t argument = 0; argument < base_arguments.size(); ++argument)
									if(CanonicalSpelling(candidate->second[argument]) !=
										CanonicalSpelling(base_arguments[argument])) {
										same = false;
										break;
									}
				if(same) {
					materialized_base = candidate->first;
									break;
								}
							}
							if(materialized_base.empty() && base_definition) {
								bool concrete = base_arguments.size() == base_definition->parameters.size();
								for(size_t argument = 0; concrete && argument < base_arguments.size(); ++argument)
									if(!base_definition->parameters[argument].type) {
										PA19IntegralValue value;
										concrete = EvaluateIntegralText(base_arguments[argument], context,
											member_substitutions, &value);
									}
								if(concrete) {
									const TemplateDefinition* selected = SelectClassTemplateDefinition(
										base_definition, base_arguments, context);
									try {
										if(selected) materialized_base = Instantiate(*selected,
											base_arguments, context);
									} catch(const logic_error&) {
										materialized_base.clear();
									}
								}
							}
			if(materialized_base.empty() &&
				class_contexts_.find(base_spelling) != class_contexts_.end())
				materialized_base = base_spelling;
			if(materialized_base.empty()) {
				// Generated base spellings are often emitted without the lexical
				// namespace (`integral_constant_bool__false_`) even though the
				// typed class tables retain it (`std::integral_constant...`).
				// Reconnect that spelling by its unique generated class component.
				for(set<string>::const_iterator candidate = class_contexts_.begin();
					candidate != class_contexts_.end(); ++candidate) {
					if(LastComponent(*candidate) != LastComponent(base_spelling)) continue;
					if(!materialized_base.empty()) {
						materialized_base.clear();
						break;
					}
					materialized_base = *candidate;
				}
			}
				if(!materialized_base.empty()) {
								const string member_key = materialized_base + "::value";
								map<string,PA19IntegralValue>::const_iterator direct_value =
									constant_values_.find(member_key);
								if(direct_value != constant_values_.end()) {
									*result = direct_value->second;
									return result->known;
								}
								const string suffix = "::" + member_key;
								for(map<string,PA19IntegralValue>::const_iterator value =
									constant_values_.begin(); value != constant_values_.end(); ++value)
									if(value->first.size() >= suffix.size() &&
										value->first.compare(value->first.size() - suffix.size(),
											suffix.size(), suffix) == 0) {
										*result = value->second;
										return result->known;
									}
							}
							if(!materialized_base.empty() && EvaluateIntegralText(
								materialized_base + "::value", context, member_substitutions, result))
								return true;
						}
					}
		return false;
	}
	bool PA18TemplateExpander::EvaluateInheritedIntegralValue(const string& raw,
		const string& context, const map<string, string>& substitutions,
		PA19IntegralValue* result)
	{
		const size_t value_separator = raw.rfind("::");
		if(value_separator == string::npos || raw.substr(value_separator + 2) != "value" ||
			raw.find_first_of("&|+-*/%!<>=?,") != string::npos) return false;
		const string owner = raw.substr(0, value_separator);
		map<string, string>::const_iterator specialized = specialization_bases_.find(
			LastComponent(owner));
		if(specialized == specialization_bases_.end()) return false;
		const TemplateDefinition* definition = FindDefinition(specialized->second, context);
		if(!definition || !definition->declaration) return false;
		map<string,string> member_substitutions = substitutions;
		map<string,vector<string> >::const_iterator member_arguments =
			specialization_arguments_.find(LastComponent(owner));
		if(member_arguments != specialization_arguments_.end()) {
			const TemplateDefinition* selected = SelectClassTemplateDefinition(
				definition, member_arguments->second, context);
			if(selected) definition = selected;
			for(size_t parameter = 0; parameter < definition->parameters.size() &&
				parameter < member_arguments->second.size(); ++parameter)
				if(!definition->parameters[parameter].name.empty())
					member_substitutions[definition->parameters[parameter].name] =
						member_arguments->second[parameter];
			if(definition->partial_specialization) {
				map<string,string> partial_bindings;
				if(MatchClassSpecializationPattern(*definition, member_arguments->second,
					&partial_bindings, context))
					for(map<string,string>::const_iterator binding = partial_bindings.begin();
						binding != partial_bindings.end(); ++binding)
						member_substitutions[binding->first] = binding->second;
			}
		}
		return EvaluateInheritedBaseValue(*definition, context,
			member_substitutions, result);
	}

} // namespace pa18_templates_internal
