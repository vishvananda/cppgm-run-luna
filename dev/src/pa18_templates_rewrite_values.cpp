#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
using namespace std;

namespace pa18_templates_internal {

	const TemplateDefinition* PA18TemplateExpander::FindNestedDefinition(
		const TemplateDefinition& parent, const string& nested_name) const
{
	// Nested member templates in distinct class partial specializations share
	// one qualified name in the ordinary definition index.  The selected outer
	// definition still owns the source declaration that must be replayed, so
	// consult the declaration-identity index before the name-based fallback.
	if(parent.partial_specialization && parent.declaration) for(size_t child = 0;
		child < parent.declaration->children.size(); ++child) {
		CPPGMAstNodePtr declaration = parent.declaration->children[child];
		if(!declaration || declaration->kind != "template-declaration") continue;
		while(declaration && declaration->kind == "template-declaration" &&
			declaration->children.size() >= 2)
			declaration = declaration->children[1];
		const bool constructor_declaration = declaration &&
			(declaration->kind == "special-member-definition" ||
			 declaration->kind == "special-member-declaration") &&
			(nested_name == LastComponent(parent.name) ||
			 nested_name == LastComponent(parent.qualified_name));
		if(constructor_declaration) continue;
		if(!declaration || LastComponent(DeclarationName(declaration)) != nested_name) continue;
		map<const CPPGMAstNode*, TemplateDefinition>::const_iterator found =
			template_definitions_by_declaration_.find(declaration.get());
		if(found != template_definitions_by_declaration_.end()) return &found->second;
	}
	set<string> active;
	function<const TemplateDefinition*(const TemplateDefinition&)> search;
	search = [&](const TemplateDefinition& current) -> const TemplateDefinition* {
		if(!active.insert(current.qualified_name).second) return 0;
		const TemplateDefinition* fallback = 0;
		for(map<string, TemplateDefinition>::const_iterator it = definitions_.begin();
			it != definitions_.end(); ++it) {
			const TemplateDefinition& candidate = it->second;
				if(!candidate.class_template && !candidate.alias_template &&
					!candidate.variable_template) continue;
				if(candidate.name != nested_name) continue;
			const size_t angle = candidate.owner.find('<');
			const string owner_prefix = angle == string::npos ? candidate.owner :
				candidate.owner.substr(0, angle);
			// Some parser paths retain the enclosing class name twice when a
			// nested member template is indexed (`Owner::Owner::member`).  Match
			// that typed owner spelling without duplicating the whole qualified
			// namespace path.
			if(owner_prefix != current.qualified_name && owner_prefix !=
				JoinPath(current.qualified_name, LastComponent(current.qualified_name))) continue;
			if(!fallback) fallback = &candidate;
			if(candidate.class_template && candidate.declaration &&
				candidate.declaration->kind == "class-specifier" &&
				candidate.declaration->children.size() > 1) return &candidate;
		}
		if(fallback) return fallback;
		if(!current.declaration) return static_cast<const TemplateDefinition*>(0);
		for(size_t child = 0; child < current.declaration->children.size(); ++child) {
			const CPPGMAstNodePtr clause = current.declaration->children[child];
			if(!clause || clause->kind != "base-clause") continue;
			for(size_t base_index = 0; base_index < clause->children.size(); ++base_index) {
				const CPPGMAstNodePtr base_name = ChildOfKindLocal(
					clause->children[base_index], "base-name");
				if(!base_name) continue;
				string spelling = CanonicalSpelling(RemoveMarker(base_name->value));
				spelling = CanonicalSpelling(ReplaceIdentifiers(spelling,
					map<string, string>()));
				const size_t open = spelling.find('<');
				if(open != string::npos) spelling.erase(open);
				const TemplateDefinition* base = FindDefinition(spelling, current.owner);
				if(!base) base = FindDefinition(LastComponent(spelling), current.owner);
				if(!base || !base->class_template) continue;
				const TemplateDefinition* inherited = search(*base);
				if(inherited) return inherited;
			}
		}
		return static_cast<const TemplateDefinition*>(0);
	};
	return search(parent);
}

bool PA18TemplateExpander::EvaluateVariableTemplateValue(
	const string& raw, const string& context,
	const map<string, string>& substitutions, PA19IntegralValue* result)
{
	if(!result) return false;
	const size_t open = raw.find('<');
	if(open == string::npos || raw.empty() || raw[raw.size() - 1] != '>') return false;
	string base;
	size_t begin = 0, close = string::npos;
	string argument_text;
	if(!TemplateBase(raw, open, &begin, &base) ||
		!TemplateRange(raw, open, &argument_text, &close) || close + 1 != raw.size())
		return false;
	const TemplateDefinition* primary = FindDefinition(base, context);
	if(!primary || !primary->variable_template) return false;
	const vector<string> source_arguments = SplitTemplateArguments(argument_text);
	vector<string> arguments;
	for(size_t source = 0; source < source_arguments.size(); ++source) {
		string argument = CanonicalSpelling(source_arguments[source]);
		if(argument.size() >= 3 && argument.compare(argument.size() - 3, 3, "...") == 0) {
			const string pack_name = CanonicalSpelling(argument.substr(0, argument.size() - 3));
			vector<string> values;
			map<string, vector<string> >::const_iterator active_pack =
				active_pack_substitutions_.find(pack_name);
			if(active_pack != active_pack_substitutions_.end()) values = active_pack->second;
			if(values.empty()) {
				map<string, string>::const_iterator scalar = substitutions.find(pack_name);
				if(scalar != substitutions.end() && scalar->second.find("...") == string::npos)
					values.push_back(scalar->second);
			}
			if(values.empty() && !pack_name.empty()) return false;
			for(size_t value = 0; value < values.size(); ++value)
				arguments.push_back(CanonicalSpelling(ReplaceIdentifiers(values[value], substitutions)));
			continue;
		}
		argument = CanonicalSpelling(RemoveMarker(RewriteText(argument, context,
			substitutions, 0)));
		argument = CanonicalSpelling(ReplaceIdentifiers(argument, substitutions));
		arguments.push_back(argument);
	}
	if(arguments.empty() && !primary->parameters.empty()) return false;
	const TemplateDefinition* selected = SelectClassTemplateDefinition(primary, arguments, context);
	if(!selected) selected = primary;
	try {
		const string local_name = Instantiate(*selected, arguments, context, false,
			0, &substitutions);
		map<string, PA19IntegralValue>::const_iterator value = constant_values_.find(local_name);
		if(value != constant_values_.end() && value->second.known) {
			*result = value->second;
			return true;
		}
		const string qualified = JoinPath(selected->owner, local_name);
		value = constant_values_.find(qualified);
		if(value != constant_values_.end() && value->second.known) {
			*result = value->second;
			return true;
		}
	} catch(const PA18SubstitutionFailure&) {
		return false;
	} catch(const logic_error&) {
		return false;
	}
	return false;
}

bool PA18TemplateExpander::EvaluateLogicalIntegralText(const string& raw,
	const string& context, const map<string, string>& substitutions,
	PA19IntegralValue* result)
{
	const auto split_logical = [](const string& expression, const string& operation,
		string* left, string* right) {
		int angle = 0, parentheses = 0, brackets = 0;
		for(size_t position = 0; position + 1 < expression.size(); ++position) {
			const char ch = expression[position];
			if(ch == '<' && IsTemplateAngleOpen(expression, position)) ++angle;
			else if(ch == '>' && angle > 0 && IsTemplateAngleClose(expression, position)) --angle;
			else if(ch == '(') ++parentheses;
			else if(ch == ')' && parentheses > 0) --parentheses;
			else if(ch == '[') ++brackets;
			else if(ch == ']' && brackets > 0) --brackets;
			if(angle == 0 && parentheses == 0 && brackets == 0 &&
				expression.compare(position, operation.size(), operation) == 0) {
				if(left) *left = Trim(expression.substr(0, position));
				if(right) *right = Trim(expression.substr(position + operation.size()));
				return !left || !left->empty();
			}
		}
		return false;
	};
	string left, right;
	if(split_logical(raw, "||", &left, &right)) {
		PA19IntegralValue value;
		if(!EvaluateIntegralText(left, context, substitutions, &value) || !value.known)
			return true;
		if(PA19Raw(value) != 0) *result = PA19IntegralValue::Signed(1, "int", 32);
		else EvaluateIntegralText(right, context, substitutions, result);
		return true;
	}
	if(split_logical(raw, "&&", &left, &right)) {
		PA19IntegralValue value;
		const bool left_evaluated = EvaluateIntegralText(left, context, substitutions, &value);
		if(!left_evaluated || !value.known)
			return true;
		if(PA19Raw(value) == 0) *result = PA19IntegralValue::Signed(0, "int", 32);
		else EvaluateIntegralText(right, context, substitutions, result);
		return true;
	}
	return false;
}

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
					string base_spelling = CanonicalSpelling(ReplaceIdentifiersPreservingPackSizes(
						base_name->value, base_substitutions));
					// The first pass above substitutes source parameters
					// simultaneously.  If one binding introduces the spelling of
					// another binding (`VertexProperty -> Vertex`, `Vertex ->
					// unsigned long`), do not let the follow-up template rewrite
					// reinterpret that already-materialized nominal type.
					map<string, string> base_rewrite_substitutions = base_substitutions;
					for(map<string, string>::const_iterator substitution = base_substitutions.begin();
						substitution != base_substitutions.end(); ++substitution) {
						if(substitution->first.empty() || substitution->first == substitution->second)
							continue;
						map<string, string>::const_iterator introduced =
							base_substitutions.find(substitution->second);
						if(introduced == base_substitutions.end() ||
							introduced->second == introduced->first)
							continue;
						for(size_t at = base_name->value.find(substitution->first);
							at != string::npos;
							at = base_name->value.find(substitution->first,
								at + substitution->first.size())) {
							if(at > 0 && IsIdentifierCharacter(base_name->value[at - 1])) continue;
							const size_t after = at + substitution->first.size();
							if(after < base_name->value.size() &&
								IsIdentifierCharacter(base_name->value[after])) continue;
							base_rewrite_substitutions.erase(substitution->second);
							break;
						}
					}
					base_spelling = CanonicalSpelling(RemoveMarker(RewriteText(
						base_spelling, context, base_rewrite_substitutions, 0)));
					base_spelling = ResolveAlias(base_spelling, context);
					// A replayed tail pack can leave a concrete base argument with its
					// expansion marker attached (`is_same<T, T...>`).  Once the active
					// substitutions have supplied the typed value, normalize that one
					// concrete argument before looking up the materialized base.  Keep
					// genuine dependent/function-type packs intact for later deduction.
					if(base_spelling.find("...") != string::npos) {
						const size_t base_open = base_spelling.find('<');
						string base_arguments_text;
						string normalized_base_primary;
						size_t base_begin = 0, base_close = string::npos;
						if(base_open != string::npos && TemplateBase(base_spelling, base_open,
							&base_begin, &normalized_base_primary) && TemplateRange(base_spelling, base_open,
							&base_arguments_text, &base_close)) {
							vector<string> normalized_arguments = SplitTemplateArguments(
								base_arguments_text);
							bool normalized_pack = false;
							for(size_t argument = 0; argument < normalized_arguments.size(); ++argument) {
								string candidate = Trim(normalized_arguments[argument]);
								if(candidate.size() < 3 || candidate.compare(candidate.size() - 3, 3, "...") != 0)
									continue;
								candidate.erase(candidate.size() - 3);
								if(candidate.empty() || HasUnresolvedTemplateParameter(candidate,
									context, member_substitutions)) continue;
								normalized_arguments[argument] = candidate;
								normalized_pack = true;
							}
							if(normalized_pack) {
								base_spelling = normalized_base_primary + "<";
								for(size_t argument = 0; argument < normalized_arguments.size(); ++argument) {
									if(argument) base_spelling += ",";
									base_spelling += normalized_arguments[argument];
								}
								base_spelling += ">";
							}
						}
					}
				// A still-dependent base from a partial specialization is only a
				// declaration-time relationship.  Do not try to materialize its
				// nested pack expression as a concrete template argument while the
				// enclosing specialization is being replayed; the concrete base is
				// revisited after the selected pack bindings are known.
				if(base_spelling.find("...") != string::npos) continue;
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
							if(selected) {
								materialized_base = Instantiate(*selected, base_arguments, context);
							}
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
			{
				return true;
			}
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
		// Recover the enclosing specialization's bindings before replaying a
		// nested class value.  A local alias parameter named `T` must not shadow
		// the enclosing `tuple<T...>` pack while evaluating `Enable<U...>::value`.
		const map<string, vector<string> > previous_packs = active_pack_substitutions_;
		const string nested_suffix = "::" + owner;
		for(map<string, CPPGMAstNodePtr>::const_iterator declaration =
			class_declarations_.begin(); declaration != class_declarations_.end(); ++declaration) {
			if(declaration->first.size() <= nested_suffix.size() ||
				declaration->first.compare(declaration->first.size() - nested_suffix.size(),
					nested_suffix.size(), nested_suffix) != 0) continue;
			const string concrete_parent = declaration->first.substr(0,
				declaration->first.size() - nested_suffix.size());
			map<string, string>::const_iterator parent_base = specialization_bases_.find(
				LastComponent(concrete_parent));
			map<string, vector<string> >::const_iterator parent_arguments =
				specialization_arguments_.find(LastComponent(concrete_parent));
			if(parent_base == specialization_bases_.end() ||
				parent_arguments == specialization_arguments_.end()) continue;
			const string source_parent = LastComponent(parent_base->second);
			const string source_owner_prefix = PrefixComponent(definition->owner);
			if(source_owner_prefix.empty() ||
				LastComponent(source_owner_prefix) != source_parent) continue;
			const TemplateDefinition* parent_definition = FindDefinition(
				parent_base->second, context);
			if(!parent_definition || !parent_definition->class_template) continue;
			const TemplateDefinition* selected_parent = SelectClassTemplateDefinition(
				parent_definition, parent_arguments->second, context);
			if(selected_parent) parent_definition = selected_parent;
			AddConcreteOwnerSubstitutions(concrete_parent, context,
				&member_substitutions);
			size_t parent_argument = 0;
			for(size_t parameter = 0; parameter < parent_definition->parameters.size();
				++parameter) {
				const TemplateParameter& item = parent_definition->parameters[parameter];
				if(item.pack) {
					size_t trailing_fixed = 0;
					for(size_t later = parameter + 1;
						later < parent_definition->parameters.size(); ++later)
						if(!parent_definition->parameters[later].pack) ++trailing_fixed;
					const size_t available = parent_arguments->second.size() > parent_argument ?
						parent_arguments->second.size() - parent_argument : 0;
					const size_t count = available > trailing_fixed ? available - trailing_fixed : 0;
					vector<string> values;
					for(size_t value = 0; value < count; ++value)
						values.push_back(parent_arguments->second[parent_argument++]);
					if(!item.name.empty())
						active_pack_substitutions_[item.name] = values;
				} else if(parent_argument < parent_arguments->second.size()) ++parent_argument;
			}
			break;
		}
		map<string,vector<string> >::const_iterator member_arguments =
			specialization_arguments_.find(LastComponent(owner));
		if(member_arguments != specialization_arguments_.end()) {
			const TemplateDefinition* selected = SelectClassTemplateDefinition(
				definition, member_arguments->second, context);
			if(selected) definition = selected;
			// A selected partial specialization keeps the primary parameter list for
			// materialization, but its base clause is written in the partial's own
			// parameter names.  Use the pattern matcher as the source of those
			// bindings: a fixed pattern argument (such as the `true` in
			// `__or_step<true,B1,Bn...>`) is not represented in the partial clause's
			// parameter list, so positional replay against that list would shift the
			// pack by one argument.
			if(definition->partial_specialization) {
				map<string,string> partial_bindings;
				if(MatchClassSpecializationPattern(*definition, member_arguments->second,
					&partial_bindings, context)) {
					for(size_t detail_index = 0;
						detail_index < definition->specialization_parameter_details.size();
						++detail_index) {
						const TemplateParameter& detail =
							definition->specialization_parameter_details[detail_index];
						if(detail.name.empty()) continue;
						map<string,string>::const_iterator binding = partial_bindings.find(detail.name);
						if(binding == partial_bindings.end()) continue;
						if(detail.pack) {
							const vector<string> values = SplitTemplateArguments(binding->second);
							active_pack_substitutions_[detail.name] = values;
							if(!values.empty()) member_substitutions[detail.name] = values[0];
							else member_substitutions.erase(detail.name);
						} else member_substitutions[detail.name] = binding->second;
					}
				}
			} else {
				size_t argument = 0;
				for(size_t parameter = 0; parameter < definition->parameters.size(); ++parameter) {
					const TemplateParameter& detail = definition->parameters[parameter];
					if(detail.pack) {
						const size_t available = member_arguments->second.size() > argument ?
							member_arguments->second.size() - argument : 0;
						vector<string> values;
						for(size_t value = 0; value < available; ++value)
							values.push_back(member_arguments->second[argument++]);
						if(!detail.name.empty()) active_pack_substitutions_[detail.name] = values;
					} else if(argument < member_arguments->second.size()) {
						if(!detail.name.empty()) member_substitutions[detail.name] =
							member_arguments->second[argument];
						++argument;
					}
				}
			}
		}
		const bool evaluated = EvaluateInheritedBaseValue(*definition, context,
			member_substitutions, result);
		active_pack_substitutions_ = previous_packs;
		return evaluated;
	}

} // namespace pa18_templates_internal
