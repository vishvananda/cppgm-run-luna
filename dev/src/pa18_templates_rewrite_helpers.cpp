#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
namespace pa18_templates_internal {

bool PA18TemplateExpander::MatchForwardingReferencePattern(const string& pattern,
	const string& actual, const set<string>& parameter_names,
	map<string, string>* inferred) const
{
	const string base = CanonicalSpelling(pattern.substr(0, pattern.size() - 2));
	if(parameter_names.find(base) == parameter_names.end()) return false;
	string deduced = actual;
	if(deduced.size() > 1 && deduced.compare(deduced.size() - 2, 2, "&&") == 0)
		deduced = CanonicalSpelling(deduced.substr(0, deduced.size() - 2));
	(*inferred)[base] = deduced;
	return true;
}

bool PA18TemplateExpander::ConsumeMaterializedStaticAssert(
	const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& result,
	const string& context, const map<string, string>& substitutions)
{
	if(!input || input->kind != "static-assert-declaration" || !result ||
		result->children.empty()) return false;
	PA19IntegralValue value;
	const string expression = ConstantExpressionSpelling(result->children[0]);
	if(active_instantiation_name_.empty() && expression.find("::") == string::npos) return false;
	const bool evaluated = EvaluateIntegralText(expression, context, substitutions, &value);
	if(!evaluated || !value.known) return false;
	if(PA19Raw(value) == 0 && !active_instantiation_name_.empty()) {
		throw logic_error("static assertion failed");
	}
	return PA19Raw(value) != 0;
}

string PA18TemplateExpander::PromotedLocalClass(const string& name,
	const string& context) const
{
	map<string, string>::const_iterator local = local_class_names_.find(
		JoinPath(context, name));
	if(local != local_class_names_.end()) return local->second;
	const string function_component = LastComponent(context);
	map<string, string>::const_iterator candidate = local_class_names_.end();
	for(map<string, string>::const_iterator entry = local_class_names_.begin();
		entry != local_class_names_.end(); ++entry) {
		if(LastComponent(entry->first) != name && entry->second != name ||
			LastComponent(PrefixComponent(entry->first)) != function_component) continue;
		if(candidate != local_class_names_.end()) return string();
		candidate = entry;
	}
	return candidate == local_class_names_.end() ? string() : candidate->second;
}

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

bool PA18TemplateExpander::MatchNestedFunctionPointerPattern(
	const string& pattern, const string& actual, const set<string>& parameter_names,
	map<string, string>* inferred, const string& context, bool class_pattern) const
{
	// A parameter can spell a pointer to a dependent member typedef, such as
	// `typename equality<T>::type*`.  The concrete alias table cannot resolve
	// that member before T is known, but its class-template declaration carries
	// the typed alias body needed for deduction.  Expand that body while keeping
	// the formal owner arguments intact; the normal function-pointer matcher can
	// then deduce T from the selected function overload.
	const size_t member_separator = pattern.rfind("::");
	if(member_separator != string::npos && member_separator + 2 < pattern.size()) {
		string owner = CanonicalSpelling(pattern.substr(0, member_separator));
		const string member = pattern.substr(member_separator + 2,
			pattern.find_first_of("*&", member_separator + 2) - member_separator - 2);
		const size_t owner_open = owner.find('<');
		string owner_arguments;
		size_t owner_close = string::npos;
		string owner_base;
		size_t owner_begin = 0;
		if(owner_open != string::npos && TemplateBase(owner, owner_open, &owner_begin,
			&owner_base) && TemplateRange(owner, owner_open, &owner_arguments, &owner_close)) {
			const TemplateDefinition* owner_definition = FindDefinition(owner_base, context);
			if(owner_definition && owner_definition->class_template &&
				owner_definition->declaration) {
				const vector<string> owner_parts = SplitTemplateArguments(owner_arguments);
				map<string, string> owner_substitutions;
				for(size_t parameter = 0; parameter < owner_definition->parameters.size() &&
					parameter < owner_parts.size(); ++parameter)
					if(!owner_definition->parameters[parameter].name.empty())
						owner_substitutions[owner_definition->parameters[parameter].name] =
							owner_parts[parameter];
				for(size_t child = 0; child < owner_definition->declaration->children.size(); ++child) {
					const CPPGMAstNodePtr declaration = owner_definition->declaration->children[child];
					if(!declaration || declaration->kind != "simple-declaration" ||
						declaration->children.empty() ||
						SpellNode(declaration->children[0]).find("typedef") == string::npos) continue;
					const CPPGMAstNodePtr list = ChildOfKindLocal(declaration,
						"init-declarator-list");
					if(!list) continue;
					for(size_t item = 0; item < list->children.size(); ++item) {
						const CPPGMAstNodePtr entry = list->children[item];
						if(!entry || entry->children.empty() ||
							LastComponent(FirstIdentifierLocal(entry->children[0])) != member) continue;
						string expanded = DeclaratorTypeSpelling(
							NodeTypeSpelling(declaration->children[0]), entry->children[0]);
						expanded = CanonicalSpelling(ReplaceIdentifiersPreservingPackSizes(
							expanded, owner_substitutions));
						const size_t alias_end = member_separator + 2 + member.size();
						const string suffix = pattern.substr(alias_end);
						if(expanded.find("(*") == string::npos || suffix != "*")
							expanded = CanonicalSpelling(expanded + suffix);
						if(!expanded.empty() && expanded != pattern)
							return MatchTypePattern(expanded, actual, parameter_names,
								inferred, context, class_pattern);
					}
				}
			}
		}
	}
	string result;
	vector<string> parameters;
	if(!SplitFunctionPointerType(actual, &result, &parameters)) return false;
	string direct = result + "(";
	for(size_t parameter = 0; parameter < parameters.size(); ++parameter) {
		if(parameter) direct += ',';
		direct += parameters[parameter];
	}
	direct += ')';
	return MatchTypePattern(pattern.substr(0, pattern.size() - 1),
		CanonicalSpelling(direct), parameter_names, inferred, context, class_pattern);
}

bool PA18TemplateExpander::PreserveEvaluatedDecltype(
	const CPPGMAstNodePtr& input, const map<string, string>& substitutions,
	const CPPGMAstNodePtr& result) const
{
	if(!input || input->kind != "decltype-specifier" ||
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
	const string& context, bool class_pattern, set<string>* active) const
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
	set<string> local_active;
	if(!active) active = &local_active;
	const string active_key = pattern + "|" + actual_class;
	if(!active->insert(active_key).second) return -1;
	const size_t source_open = actual_class.find('<');
	if(source_open != string::npos) {
		string source_arguments;
		size_t source_close = string::npos;
		if(!TemplateRange(actual_class, source_open, &source_arguments, &source_close)) return -1;
		const string source_base = actual_class.substr(0, source_open);
		const TemplateDefinition* primary = FindDefinition(source_base, context);
		if(!primary || !primary->class_template || !primary->declaration) return -1;
		vector<string> arguments = SplitTemplateArguments(source_arguments);
		map<string, string> defaults;
		for(size_t parameter = 0; parameter < primary->parameters.size(); ++parameter) {
			if(parameter < arguments.size()) {
				if(!primary->parameters[parameter].name.empty())
					defaults[primary->parameters[parameter].name] = arguments[parameter];
				continue;
			}
			if(primary->parameters[parameter].pack) continue;
			if(primary->parameters[parameter].default_type.empty()) break;
			arguments.push_back(CanonicalSpelling(ReplaceIdentifiers(
				primary->parameters[parameter].default_type, defaults)));
			if(!primary->parameters[parameter].name.empty())
				defaults[primary->parameters[parameter].name] = arguments.back();
		}
		const TemplateDefinition* selected = SelectClassTemplateDefinition(
			primary, arguments, context);
		if(!selected || !selected->declaration) return -1;
		map<string, string> substitutions;
		map<string, vector<string> > packs;
		if(selected->partial_specialization) {
			if(!MatchClassSpecializationPattern(*selected, arguments, &substitutions, context))
				return -1;
			for(size_t pack = 0; pack < selected->specialization_pack_names.size(); ++pack) {
				const string& name = selected->specialization_pack_names[pack];
				map<string, string>::const_iterator value = substitutions.find(name);
				packs[name] = value == substitutions.end() || value->second.empty() ?
					vector<string>() : SplitTemplateArguments(value->second);
			}
		} else {
			size_t argument = 0;
			for(size_t parameter = 0; parameter < selected->parameters.size(); ++parameter) {
				const TemplateParameter& item = selected->parameters[parameter];
				if(item.pack) {
					size_t trailing_fixed = 0;
					for(size_t later = parameter + 1; later < selected->parameters.size(); ++later)
						if(!selected->parameters[later].pack) ++trailing_fixed;
					const size_t available = arguments.size() > argument ?
						arguments.size() - argument : 0;
					const size_t count = available > trailing_fixed ? available - trailing_fixed : 0;
					if(!item.name.empty()) packs[item.name] = vector<string>(
						arguments.begin() + argument, arguments.begin() + argument + count);
					argument += count;
					continue;
				}
				if(argument < arguments.size()) {
					if(!item.name.empty()) substitutions[item.name] = arguments[argument];
					++argument;
				}
			}
		}
		const auto expand_base = [&](string raw) {
			for(map<string, vector<string> >::const_iterator pack = packs.begin();
				pack != packs.end(); ++pack) {
				const string token = pack->first + "...";
				string expansion;
				for(size_t value = 0; value < pack->second.size(); ++value) {
					if(!expansion.empty()) expansion += ',';
					expansion += pack->second[value];
				}
				for(size_t at = raw.find(token); at != string::npos; ) {
					if(expansion.empty()) {
						if(at > 0 && raw[at - 1] == ',') {
							raw.erase(at - 1, token.size() + 1);
							at = at == 0 ? 0 : at - 1;
						} else if(at + token.size() < raw.size() && raw[at + token.size()] == ',')
							raw.erase(at, token.size() + 1);
						else raw.erase(at, token.size());
					} else {
						raw.replace(at, token.size(), expansion);
						at += expansion.size();
					}
					at = raw.find(token, at);
				}
			}
			return CanonicalSpelling(ReplaceIdentifiers(raw, substitutions));
		};
		const TemplateDefinition* base_definition = FindDefinition(source_base, context);
		for(size_t child = 0; child < selected->declaration->children.size(); ++child) {
			const CPPGMAstNodePtr clause = selected->declaration->children[child];
			if(!clause || clause->kind != "base-clause") continue;
			for(size_t base_index = 0; base_index < clause->children.size(); ++base_index) {
				const CPPGMAstNodePtr base_name = ChildOfKindLocal(
					clause->children[base_index], "base-name");
				if(!base_name) continue;
				string concrete_base = expand_base(base_name->value);
				const size_t base_open = concrete_base.find('<');
				if(base_definition && base_open != string::npos) {
					string base_arguments;
					size_t base_close = string::npos;
					const string base_name_text = concrete_base.substr(0, base_open);
					const TemplateDefinition* definition = FindDefinition(base_name_text, context);
					if(definition && TemplateRange(concrete_base, base_open, &base_arguments, &base_close)) {
						vector<string> parts = SplitTemplateArguments(base_arguments);
						for(size_t part = 0; part < parts.size() && part < definition->parameters.size(); ++part)
							if(!definition->parameters[part].type) {
								PA19IntegralValue value;
								if(const_cast<PA18TemplateExpander*>(this)->EvaluateIntegralText(
									parts[part], context, substitutions, &value))
									parts[part] = IntegralValueSpelling(value);
							}
						concrete_base = base_name_text + "<";
						for(size_t part = 0; part < parts.size(); ++part) {
							if(part) concrete_base += ',';
							concrete_base += parts[part];
						}
						concrete_base += '>';
					}
				}
				concrete_base = cv_prefix + concrete_base + pointer_suffix;
				if(MatchTypePattern(pattern, concrete_base, parameter_names, inferred,
					context, class_pattern)) return 1;
				if(MatchGeneratedBaseTypePattern(pattern, concrete_base, pattern_base,
					parameter_names, inferred, context, class_pattern, active) > 0) return 1;
			}
		}
		return -1;
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
			if(MatchGeneratedBaseTypePattern(pattern, concrete_base, pattern_base,
				parameter_names, inferred, context, class_pattern, active) > 0) return 1;
		}
	}
	return -1;
}

void PA18TemplateExpander::TransformRegularChildrenWithInitializerType(
	const CPPGMAstNodePtr& input, const string& child_context,
	const string& function_context, const string& context,
	const map<string, string>& substitutions,
	map<string, string>* local_substitutions, const CPPGMAstNodePtr& result)
{
	string expected;
	if(input->kind == "simple-declaration" && !input->children.empty() &&
		input->children[0] && input->children[0]->kind == "decl-specifier-seq") {
		const CPPGMAstNodePtr declarators = ChildOfKindLocal(input,
			"init-declarator-list");
		if(declarators) for(size_t item = 0; item < declarators->children.size(); ++item) {
			const CPPGMAstNodePtr entry = declarators->children[item];
			if(!entry || entry->children.size() < 2 || !entry->children[0] ||
				!entry->children[1]) continue;
			const string spelling = SpellNode(entry->children[1]);
			if(spelling.find('&') == string::npos || spelling.find("::") == string::npos ||
				spelling.find('<') == string::npos) continue;
			expected = DeclaratorTypeSpelling(NodeTypeSpelling(input->children[0]),
				entry->children[0]);
			expected = RewriteText(expected, context, substitutions, 0, true, true,
				defer_type_only_class_definitions_ != 0);
			break;
		}
	}
	// A qualified member-template address in an aggregate initializer is
	// selected by the type of the destination field, not by the aggregate's
	// own class type.  Preserve that typed fact while replaying the initializer
	// so an overloaded `wrap` template can be deduced from the field's function
	// pointer signature.
	if(!expected.empty() && input->kind == "simple-declaration") {
		const CPPGMAstNodePtr declarators = ChildOfKindLocal(input,
			"init-declarator-list");
		if(declarators) for(size_t item = 0; item < declarators->children.size(); ++item) {
			const CPPGMAstNodePtr entry = declarators->children[item];
			if(!entry || entry->children.size() < 2 || !entry->children[1]) continue;
			CPPGMAstNodePtr initializer = entry->children[1];
			if(initializer->kind == "initializer" && initializer->children.size() == 1)
				initializer = initializer->children[0];
			if(!initializer || initializer->kind != "braced-init-list" ||
				initializer->children.empty()) continue;
			string aggregate = CanonicalSpelling(expected);
			while(aggregate.compare(0, 6, "const ") == 0 ||
				aggregate.compare(0, 9, "volatile ") == 0)
				aggregate = CanonicalSpelling(aggregate.substr(aggregate.find(' ') + 1));
			while(aggregate.size() > 6 && aggregate.compare(aggregate.size() - 6,
				6, " const") == 0)
				aggregate = CanonicalSpelling(aggregate.substr(0, aggregate.size() - 6));
			while(aggregate.size() > 9 && aggregate.compare(aggregate.size() - 9,
				9, " volatile") == 0)
				aggregate = CanonicalSpelling(aggregate.substr(0, aggregate.size() - 9));
			aggregate = CanonicalSpelling(ResolveAlias(aggregate, context));
			const CPPGMAstNodePtr declaration = FindClassDeclaration(aggregate, context);
			if(!declaration) continue;
			bool aggregate_field_type = false;
			for(size_t member_index = 0; member_index < declaration->children.size();
				++member_index) {
				const CPPGMAstNodePtr member = declaration->children[member_index];
				if(!member || member->kind != "simple-declaration" ||
					member->children.empty() || HasDeclarationSpecifier(
					member->children[0], "typedef") || HasDeclarationSpecifier(
					member->children[0], "static")) continue;
				const CPPGMAstNodePtr member_declarators = ChildOfKindLocal(member,
					"init-declarator-list");
				if(!member_declarators) continue;
				for(size_t member_item = 0; member_item < member_declarators->children.size();
					++member_item) {
					const CPPGMAstNodePtr member_entry = member_declarators->children[member_item];
					if(!member_entry || member_entry->children.empty() ||
						DescendantOfKind(member_entry->children[0], "parameter-clause")) continue;
					const string member_name = LastComponent(FirstIdentifierLocal(
						member_entry->children[0]));
					if(member_name.empty()) continue;
					string member_type;
					set<string> active;
					if(!FindClassMemberType(aggregate, member_name, substitutions, context,
						&member_type, &active, false))
						member_type = DeclaratorTypeSpelling(NodeTypeSpelling(member->children[0]),
							member_entry->children[0]);
					if(member_type.empty()) continue;
					member_type = RewriteText(member_type, context, substitutions, 0, true,
						true, defer_type_only_class_definitions_ != 0);
					if(!member_type.empty()) {
						expected = CanonicalSpelling(ResolveAlias(member_type, context));
						aggregate_field_type = !expected.empty();
					}
					break;
				}
				if(aggregate_field_type) break;
			}
		}
	}
	const string saved = active_initializer_expected_type_;
	if(input->kind == "simple-declaration") active_initializer_expected_type_ = expected;
	try {
		TransformRegularChildren(input, child_context, function_context, substitutions,
			local_substitutions, result);
	} catch(...) {
		active_initializer_expected_type_ = saved;
		throw;
	}
	active_initializer_expected_type_ = saved;
}

bool PA18TemplateExpander::TransformCorrelatedPackChild(
	const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& original_child,
	const string& child_context, const map<string, string>& substitutions,
	const CPPGMAstNodePtr& result)
{
	const bool base_pack = input && input->kind == "base-clause" &&
		original_child && original_child->kind == "base-specifier" &&
		ChildOfKindLocal(original_child, "pack-expansion");
	const bool mem_pack = input && input->kind == "ctor-initializer" &&
		original_child && original_child->kind == "mem-initializer" &&
		ChildOfKindLocal(original_child, "pack-expansion");
	if(!base_pack && !mem_pack) return false;
	const string spelling = SpellNode(original_child);
	vector<string> pack_names;
	const auto collect = [&](const map<string, vector<string> >& packs) {
		for(map<string, vector<string> >::const_iterator pack = packs.begin();
			pack != packs.end(); ++pack) {
			if(pack->first.empty()) continue;
			for(size_t at = spelling.find(pack->first); at != string::npos;
				at = spelling.find(pack->first, at + pack->first.size())) {
				const bool left = at == 0 || !IsIdentifierCharacter(spelling[at - 1]);
				const size_t end = at + pack->first.size();
				const bool right = end == spelling.size() ||
					!IsIdentifierCharacter(spelling[end]);
				if(left && right) { pack_names.push_back(pack->first); break; }
			}
		}
	};
	collect(active_pack_substitutions_);
	collect(active_pack_identifier_substitutions_);
	if(pack_names.empty()) return true;
	if(base_pack && pack_names.size() == 1) {
		const CPPGMAstNodePtr original_base = ChildOfKindLocal(
			original_child, "base-name");
		const string pack_name = PackExpansionIdentifier(original_base);
		map<string, vector<string> >::const_iterator pack =
			active_pack_substitutions_.find(pack_name);
		if(pack != active_pack_substitutions_.end()) {
			for(size_t element = 0; element < pack->second.size(); ++element) {
				CPPGMAstNodePtr expanded = CloneNode(original_child);
				RemoveParameterPackMarkers(expanded);
				const CPPGMAstNodePtr base = ChildOfKindLocal(expanded, "base-name");
				map<string, string> one = substitutions;
				one[pack_name] = pack->second[element];
				if(base && original_base)
					base->value = ReplaceIdentifiers(original_base->value, one);
				CPPGMAstNodePtr child = TransformNode(expanded, child_context,
					substitutions);
				if(child) result->children.push_back(child);
			}
		}
		return true;
	}
	const map<string, vector<string> >::const_iterator first_typed =
		active_pack_substitutions_.find(pack_names[0]);
	const vector<string>& first_values = first_typed !=
		active_pack_substitutions_.end() ? first_typed->second :
		active_pack_identifier_substitutions_.find(pack_names[0])->second;
	for(size_t pack = 1; pack < pack_names.size(); ++pack) {
		const map<string, vector<string> >::const_iterator typed =
			active_pack_substitutions_.find(pack_names[pack]);
		const map<string, vector<string> >::const_iterator named =
			active_pack_identifier_substitutions_.find(pack_names[pack]);
		const vector<string>& values = typed != active_pack_substitutions_.end() ?
			typed->second : named->second;
		if(values.size() != first_values.size())
			throw PA18SubstitutionFailure("pack expansion length mismatch");
	}
	for(size_t element = 0; element < first_values.size(); ++element) {
		map<string, string> one = substitutions;
		for(size_t pack = 0; pack < pack_names.size(); ++pack) {
			const map<string, vector<string> >::const_iterator typed =
				active_pack_substitutions_.find(pack_names[pack]);
			const map<string, vector<string> >::const_iterator named =
				active_pack_identifier_substitutions_.find(pack_names[pack]);
			const vector<string>& values = typed != active_pack_substitutions_.end() ?
				typed->second : named->second;
			one[pack_names[pack]] = values[element];
		}
		CPPGMAstNodePtr expanded = CloneNode(original_child);
		RemoveParameterPackMarkers(expanded);
		if(base_pack) {
			const CPPGMAstNodePtr base = ChildOfKindLocal(expanded, "base-name");
			const CPPGMAstNodePtr original_base = ChildOfKindLocal(
				original_child, "base-name");
			if(base && original_base) base->value = ReplaceIdentifiersPreservingPackSizes(
				original_base->value, one);
		}
		CPPGMAstNodePtr child = TransformNode(expanded, child_context, one);
		if(child) result->children.push_back(child);
	}
	return true;
}

} // namespace pa18_templates_internal
