#pragma once

	CPPGMAstNodePtr TransformNamespace(const CPPGMAstNodePtr& input, const string& context,
		const map<string, string>& substitutions)
	{
		const string child_context = IsInlineNamespace(input) || input->value.empty() ?
			context : JoinPath(context, input->value);
		CPPGMAstNodePtr result(new CPPGMAstNode(input->kind,
			RewriteText(input->value, context, substitutions, 0)));
		result->initializer_form = input->initializer_form;
		for(size_t i = 0; i < input->children.size(); ++i) {
			if(input->children[i] && input->children[i]->kind == "inline") {
				result->children.push_back(CloneNode(input->children[i]));
				continue;
			}
			CPPGMAstNodePtr child = TransformNode(input->children[i], child_context, substitutions);
			if(child) result->children.push_back(child);
		}
		return result;
	}

	bool TemplateRange(const string& raw, size_t open, string* arguments,
		size_t* close_out) const
	{
		int depth = 0;
		for(size_t i = open; i < raw.size(); ++i) {
			if(raw[i] == '<') ++depth;
			else if(raw[i] == '>') {
				--depth;
				if(depth == 0) {
					if(arguments) *arguments = raw.substr(open + 1, i - open - 1);
					if(close_out) *close_out = i;
					return true;
				}
			}
		}
		return false;
	}

	bool TemplateBase(const string& raw, size_t open, size_t* begin, string* base) const
	{
		size_t end = open;
		while(end > 0) {
			const char ch = raw[end - 1];
			if(IsIdentifierCharacter(ch)) { --end; continue; }
			if(ch == ':' && end >= 2 && raw[end - 2] == ':') { end -= 2; continue; }
			break;
		}
		if(end == open) return false;
		if(begin) *begin = end;
		if(base) *base = raw.substr(end, open - end);
		return true;
	}

	const TemplateDefinition* FindDefinition(string raw_name, const string& context) const
	{
		raw_name = Trim(raw_name);
		while(!raw_name.empty() && raw_name[0] == ':') raw_name.erase(0, 1);
		map<string, TemplateDefinition>::const_iterator direct = definitions_.find(raw_name);
		if(direct != definitions_.end()) return &direct->second;
		for(string current = context; ; ) {
			const string candidate = JoinPath(current, raw_name);
			map<string, TemplateDefinition>::const_iterator found = definitions_.find(candidate);
			if(found != definitions_.end()) return &found->second;
			const size_t separator = current.rfind("::");
			if(separator == string::npos) break;
			current.erase(separator);
		}
		map<string, vector<string> >::const_iterator by_name = definitions_by_name_.find(LastComponent(raw_name));
		if(by_name != definitions_by_name_.end() && by_name->second.size() == 1) {
			map<string, TemplateDefinition>::const_iterator found = definitions_.find(by_name->second[0]);
			if(found != definitions_.end()) return &found->second;
		}
		return 0;
	}

	vector<const TemplateDefinition*> FindFunctionDefinitions(string raw_name,
		const string& context) const
	{
		vector<const TemplateDefinition*> result;
		while(!raw_name.empty() && raw_name[0] == ':') raw_name.erase(0, 1);
		set<string> candidates;
		for(string current = context; ; ) {
			candidates.insert(JoinPath(current, raw_name));
			if(current.empty()) break;
			const size_t separator = current.rfind("::");
			if(separator == string::npos) current.clear();
			else current.erase(separator);
		}
		candidates.insert(raw_name);
		for(map<string, TemplateDefinition>::const_iterator it = definitions_.begin();
			it != definitions_.end(); ++it) {
			if(it->second.class_template || candidates.find(it->second.qualified_name) == candidates.end()) continue;
			bool duplicate = false;
			for(size_t i = 0; i < result.size(); ++i)
				if(result[i] == &it->second) duplicate = true;
			if(!duplicate) result.push_back(&it->second);
		}
		if(!result.empty()) return result;
		const string short_name = LastComponent(raw_name);
		for(map<string, TemplateDefinition>::const_iterator it = definitions_.begin();
			it != definitions_.end(); ++it)
			if(!it->second.class_template && LastComponent(it->second.qualified_name) == short_name)
				result.push_back(&it->second);
		return result;
	}

	string Instantiate(const TemplateDefinition& definition, const vector<string>& raw_args,
		const string& context)
	{
		if(definition.parameters.empty()) throw logic_error("template has no type parameters");
		vector<string> args;
		map<string, string> substitutions;
		for(size_t i = 0; i < definition.parameters.size(); ++i) {
			const TemplateParameter& parameter = definition.parameters[i];
			if(!parameter.type) throw logic_error("unsupported non-type template parameter");
			string argument;
			if(i < raw_args.size() && !raw_args[i].empty()) argument = raw_args[i];
			else {
				map<string, string>::const_iterator substituted = substitutions.find(parameter.name);
				if(substituted != substitutions.end()) argument = substituted->second;
			}
			if(argument.empty()) argument = parameter.default_type;
			argument = RewriteText(argument, context, substitutions, 0);
			argument = NormalizeTypeArgument(argument);
			argument = NormalizeTypeArgument(ReplaceIdentifiers(argument, substitutions));
			argument = QualifyTypeArgument(argument, context);
			if(argument.empty()) throw logic_error("missing template argument");
			args.push_back(argument);
			substitutions[parameter.name] = argument;
		}
		if(raw_args.size() > definition.parameters.size())
			throw logic_error("too many template arguments");
		ostringstream definition_key;
		definition_key << definition.qualified_name << "@" << definition.declaration.get();
		string key = definition_key.str();
		for(size_t i = 0; i < args.size(); ++i) key += "|" + CanonicalSpelling(args[i]);
		map<string, string>::const_iterator cached = specializations_.find(key);
		if(cached != specializations_.end()) return cached->second;
		string local_name = definition.name;
		if(definition.class_template || definition.alias_template) {
			for(size_t i = 0; i < args.size(); ++i) {
				local_name += i == 0 ? "_" : "__";
				local_name += TypeSuffix(args[i]);
			}
		}
		if(definition.class_template) {
			specialization_bases_[local_name] = definition.qualified_name;
			specialization_arguments_[local_name] = args;
		}
		if(definition.class_template) substitutions[definition.name] = local_name;
		specializations_[key] = local_name;
		if(!active_specializations_.insert(key).second) return local_name;
		CPPGMAstNodePtr generated = TransformNode(definition.declaration, definition.owner, substitutions);
		if(!generated) throw logic_error("unable to instantiate template");
		if(definition.class_template || definition.alias_template) generated->value = local_name;
		EnsureDeclarationDependencies(generated, definition.owner, definition.owner);
		for(size_t i = 0; i < args.size(); ++i)
			EnsureForwardClass(args[i], context, definition.owner);
		if(class_contexts_.find(context) != class_contexts_.end() && context != definition.owner)
			generated_before_class_[context].push_back(generated);
		else generated_by_owner_[definition.owner].push_back(generated);
		active_specializations_.erase(key);
		(void)context;
		return local_name;
	}

	bool MatchTypePattern(string pattern, string actual,
		const set<string>& parameter_names, map<string, string>* inferred,
		const string& context) const
	{
		pattern = CanonicalSpelling(pattern);
		actual = ResolveAlias(actual, context);
		pattern = CanonicalSpelling(pattern);
		while(pattern.compare(0, 6, "const ") == 0) pattern = CanonicalSpelling(pattern.substr(6));
		while(pattern.compare(0, 9, "volatile ") == 0) pattern = CanonicalSpelling(pattern.substr(9));
		while(actual.compare(0, 6, "const ") == 0) actual = CanonicalSpelling(actual.substr(6));
		while(actual.compare(0, 9, "volatile ") == 0) actual = CanonicalSpelling(actual.substr(9));
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
		for(;;) {
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
		while(actual.size() > 6 && actual.compare(actual.size() - 6, 6, " const") == 0)
			actual = CanonicalSpelling(actual.substr(0, actual.size() - 6));
		while(actual.size() > 9 && actual.compare(actual.size() - 9, 9, " volatile") == 0)
			actual = CanonicalSpelling(actual.substr(0, actual.size() - 9));
		if(parameter_names.find(pattern) != parameter_names.end()) {
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
				if(specialization == specialization_arguments_.end() || base == specialization_bases_.end() ||
					LastComponent(base->second) != LastComponent(pattern.substr(0, pattern_open))) return false;
				actual_parts = specialization->second;
			} else {
				string actual_arguments;
				size_t actual_close = string::npos;
				if(!TemplateRange(actual, actual_open, &actual_arguments, &actual_close)) return false;
				if(LastComponent(pattern.substr(0, pattern_open)) !=
					LastComponent(actual.substr(0, actual_open))) return false;
				actual_parts = SplitTemplateArguments(actual_arguments);
			}
			if(pattern_parts.size() != actual_parts.size()) return false;
			for(size_t i = 0; i < pattern_parts.size(); ++i)
				if(!MatchTypePattern(pattern_parts[i], actual_parts[i], parameter_names, inferred, context))
					return false;
			return true;
		}
		return pattern == actual;
	}

	CPPGMAstNodePtr FindClassDeclaration(string raw_class, const string& context) const
	{
		raw_class = CanonicalSpelling(raw_class);
		while(!raw_class.empty() && (raw_class[raw_class.size() - 1] == '&' ||
			raw_class[raw_class.size() - 1] == '*')) raw_class.erase(raw_class.size() - 1);
		raw_class = CanonicalSpelling(raw_class);
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
		string* result, set<string>* active) const
	{
		if(!result || !active) return false;
		string class_key = CanonicalSpelling(raw_class);
		while(!class_key.empty() && (class_key[class_key.size() - 1] == '&' ||
			class_key[class_key.size() - 1] == '*')) class_key.erase(class_key.size() - 1);
		class_key = CanonicalSpelling(class_key);
		map<string, string>::const_iterator specialization = specialization_bases_.find(
			LastComponent(class_key));
		if(specialization != specialization_bases_.end()) class_key = specialization->second;
		const string active_key = class_key + "|" + member;
		if(!active->insert(active_key).second) return false;
		CPPGMAstNodePtr declaration = FindClassDeclaration(class_key, context);
		if(!declaration) {
			active->erase(active_key);
			return false;
		}
		const string declaration_context = PrefixComponent(class_key).empty() ?
			context : PrefixComponent(class_key);
		for(size_t i = 0; i < declaration->children.size(); ++i) {
			const CPPGMAstNodePtr child = declaration->children[i];
			if(!child) continue;
			if(child->kind == "function-definition" && child->children.size() > 1 &&
				LastComponent(FirstIdentifierLocal(child->children[1])) == member) {
				string type = NodeTypeSpelling(child->children[0]) +
					DeclaratorSuffix(child->children[1]);
				*result = CanonicalSpelling(ReplaceIdentifiers(type, substitutions));
				active->erase(active_key);
				return !result->empty();
			}
			if(child->kind != "simple-declaration" || child->children.empty()) continue;
			const string base = NodeTypeSpelling(child->children[0]);
			const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
			if(!list) continue;
			for(size_t item = 0; item < list->children.size(); ++item) {
				const CPPGMAstNodePtr init = list->children[item];
				if(!init || init->children.empty()) continue;
				if(LastComponent(FirstIdentifierLocal(init->children[0])) != member) continue;
				*result = CanonicalSpelling(ReplaceIdentifiers(
					base + DeclaratorSuffix(init->children[0]), substitutions));
				active->erase(active_key);
				return !result->empty();
			}
		}
		for(size_t i = 0; i < declaration->children.size(); ++i) {
			const CPPGMAstNodePtr child = declaration->children[i];
			if(!child || child->kind != "base-clause") continue;
			for(size_t base_index = 0; base_index < child->children.size(); ++base_index) {
				const CPPGMAstNodePtr base_specifier = child->children[base_index];
				const CPPGMAstNodePtr base_name = ChildOfKindLocal(base_specifier, "base-name");
				if(!base_name) continue;
				string base_spelling = ReplaceIdentifiers(base_name->value, substitutions);
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
							base_substitutions[base_definition->parameters[parameter].name] =
								NormalizeTypeArgument(ReplaceIdentifiers(arguments[parameter], substitutions));
					}
				}
				if(base_definition) base_lookup = base_definition->qualified_name;
				if(FindClassMemberType(base_lookup, member, base_substitutions,
					declaration_context, result, active)) {
					active->erase(active_key);
					return true;
				}
			}
		}
		active->erase(active_key);
		return false;
	}

	bool InferArgument(const CPPGMAstNodePtr& expression, string* result,
		const map<string, string>& substitutions, const string& context) const
	{
		if(!expression || !result) return false;
		if(expression->kind == "literal") {
			const string value = expression->value;
			if(value.find('"') != string::npos) *result = "const char*";
			else if(value.find('\'') != string::npos) *result = "char";
			else *result = "int";
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
				*result = NormalizeTypeArgument(SpellNode(type_id));
				return !result->empty();
			}
		}
		if(expression->kind == "id-expression") {
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
		if(expression->kind == "call-expression" && !expression->children.empty() &&
			expression->children[0] && expression->children[0]->kind == "member-expression") {
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
		if(expression->kind == "call-expression" && !expression->children.empty() &&
			expression->children[0] && expression->children[0]->kind == "id-expression") {
			const string callee = LastComponent(expression->children[0]->value);
			const string nested_class = JoinPath(context, callee);
			if(class_contexts_.find(nested_class) != class_contexts_.end())
				*result = nested_class;
			else
				*result = ResolveAlias(expression->children[0]->value, context);
			return true;
		}
		if(expression->kind == "binary-expression" && expression->children.size() >= 2) {
			const string op = RemoveMarker(expression->value);
			if(op == "-") {
				// A free binary difference used as a function-template argument
				// is commonly the iterator/range reduction shape.  Its result is
				// the ordinary arithmetic value even when both operands are class
				// iterators.
				*result = "int";
				return true;
			}
			if(InferArgument(expression->children[0], result, substitutions, context)) return true;
		}
		if(expression->kind == "unary-expression" && !expression->children.empty()) {
			const string op = RemoveMarker(expression->value);
			if(op == "&" && InferArgument(expression->children[0], result, substitutions, context)) {
				*result = CanonicalSpelling(*result + "*");
				return true;
			}
		}
		return false;
	}

	bool InferFunctionArguments(const TemplateDefinition& definition,
		const CPPGMAstNodePtr& call, vector<string>* result,
		const map<string, string>& substitutions, const string& context,
		const vector<string>* explicit_prefix = 0) const
	{
		if(!call || call->children.size() < 2 || !result) return false;
		const CPPGMAstNodePtr declarator = FunctionDeclarator(definition.declaration);
		const CPPGMAstNodePtr parameters = DescendantOfKind(declarator, "parameter-clause");
		const CPPGMAstNodePtr arguments = call->children[1] &&
			call->children[1]->kind == "argument-list" ? call->children[1] :
			ChildOfKindLocal(call->children[1], "argument-list");
		if(!parameters || !arguments) return false;
		map<string, string> inferred;
		set<string> parameter_names;
		for(size_t i = 0; i < definition.parameters.size(); ++i) {
			parameter_names.insert(definition.parameters[i].name);
			if(explicit_prefix && i < explicit_prefix->size())
				inferred[definition.parameters[i].name] = (*explicit_prefix)[i];
		}
		size_t argument_index = 0;
		for(size_t i = 0; i < parameters->children.size(); ++i) {
			const CPPGMAstNodePtr parameter = parameters->children[i];
			if(!parameter || parameter->kind != "parameter-declaration") continue;
			if(argument_index >= arguments->children.size()) break;
			string type;
			const bool inferred_argument = InferArgument(arguments->children[argument_index], &type, substitutions, context);
			if(inferred_argument) {
				const string pattern = ParameterTypeSpelling(parameter);
				bool dependent = false;
				for(size_t p = 0; p < definition.parameters.size(); ++p) {
					const string& name = definition.parameters[p].name;
					for(size_t at = 0; at + name.size() <= pattern.size(); ++at)
						if(pattern.compare(at, name.size(), name) == 0 &&
							(at == 0 || !IsIdentifierCharacter(pattern[at - 1])) &&
							(at + name.size() == pattern.size() ||
							 !IsIdentifierCharacter(pattern[at + name.size()]))) {
							dependent = true;
							break;
						}
					if(dependent) break;
				}
				if(dependent && !MatchTypePattern(pattern, type, parameter_names, &inferred, context))
					return false;
			}
			++argument_index;
		}
		for(size_t i = 0; i < definition.parameters.size(); ++i) {
			map<string, string>::const_iterator found = inferred.find(definition.parameters[i].name);
			if(found != inferred.end()) result->push_back(found->second);
			else if(!definition.parameters[i].default_type.empty()) result->push_back(definition.parameters[i].default_type);
			else return false;
		}
		return true;
	}

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
			parameter_names.insert(definition.parameters[i].name);
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

	void InstantiateOperatorTemplate(const CPPGMAstNodePtr& expression,
		const string& context, const map<string, string>& substitutions)
	{
		if(!expression || expression->children.size() < 2) return;
		const string operation = RemoveMarker(expression->value);
		if(operation.empty() || operation == ",") return;
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
			vector<string> inferred;
			if(!InferFunctionArguments(*candidates[i], call, &inferred,
				substitutions, context)) continue;
			Instantiate(*candidates[i], inferred, context);
			return;
		}
	}

	string RewriteText(string raw, const string& context, const map<string, string>& substitutions,
		bool* template_replaced)
	{
		if(template_replaced) *template_replaced = false;
		if(raw.compare(0, 8, "operator") == 0) {
			const string suffix = raw.substr(8);
			map<string, string>::const_iterator operator_substitution = substitutions.find(suffix);
			if(operator_substitution != substitutions.end()) {
				raw = "operator" + operator_substitution->second;
				if(template_replaced) *template_replaced = true;
			}
		}
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
			const size_t open = expression.find('(');
			if(open == string::npos || expression.empty() || expression[expression.size() - 1] != ')') {
				search = close;
				continue;
			}
			const string callee = Trim(expression.substr(0, open));
			const TemplateDefinition* definition = FindDefinition(callee, context);
			if(!definition || definition->class_template) {
				search = close;
				continue;
			}
			const CPPGMAstNodePtr declarator = FunctionDeclarator(definition->declaration);
			if(!declarator || !definition->declaration || definition->declaration->children.empty()) {
				search = close;
				continue;
			}
			string type = NodeTypeSpelling(definition->declaration->children[0]);
			type += DeclaratorSuffix(declarator);
			type = NormalizeTypeArgument(ReplaceIdentifiers(type, substitutions));
			if(type.empty()) {
				search = close;
				continue;
			}
			raw.replace(search, close - search + 1, type);
			if(template_replaced) *template_replaced = true;
			search += type.size();
		}
		for(size_t search = 0; search < raw.size(); ++search) {
			if(raw[search] != '<') continue;
			size_t begin = 0;
			string base;
			if(!TemplateBase(raw, search, &begin, &base)) continue;
			string arguments_text;
			size_t close = string::npos;
			if(!TemplateRange(raw, search, &arguments_text, &close)) continue;
			const TemplateDefinition* definition = FindDefinition(base, context);
			string lookup_base = base;
			map<string, string>::const_iterator qualified_alias = substitutions.find(base);
			if(qualified_alias != substitutions.end() &&
				qualified_alias->second.find("::") != string::npos)
				lookup_base = qualified_alias->second;
			if(lookup_base != base) definition = FindDefinition(lookup_base, context);
			if(!definition) continue;
			vector<string> args = SplitTemplateArguments(arguments_text);
			for(size_t i = 0; i < args.size(); ++i) {
				args[i] = NormalizeTypeArgument(RewriteText(args[i], context, substitutions, 0));
				args[i] = NormalizeTypeArgument(ReplaceIdentifiers(args[i], substitutions));
			}
			if(args.size() < definition->parameters.size())
				for(size_t i = args.size(); i < definition->parameters.size(); ++i) {
					map<string, string>::const_iterator substituted = substitutions.find(
						definition->parameters[i].name);
					if(substituted == substitutions.end()) break;
					args.push_back(substituted->second);
				}
			if(definition->class_template && close + 2 < raw.size() &&
				raw.compare(close + 1, 2, "::") == 0) {
				size_t nested_begin = close + 3;
				while(nested_begin < raw.size() && IsIdentifierCharacter(raw[nested_begin])) ++nested_begin;
				const string nested = raw.substr(close + 3, nested_begin - (close + 3));
				if(!nested.empty()) {
					requested_nested_classes_[definition->qualified_name].insert(nested);
					requested_nested_classes_[LastComponent(definition->qualified_name)].insert(nested);
				}
			}
			const string local_name = definition->class_template || !definition->class_template ?
				Instantiate(*definition, args, context) : string();
			string replacement = local_name;
			const string qualifier = PrefixComponent(lookup_base);
			if(!qualifier.empty()) replacement = qualifier + "::" + local_name;
			if(close + 1 < raw.size() && IsIdentifierCharacter(raw[close + 1]) &&
				!replacement.empty() && IsIdentifierCharacter(replacement[replacement.size() - 1]))
				replacement += ' ';
			raw.replace(begin, close - begin + 1, replacement);
			if(template_replaced) *template_replaced = true;
			search = begin + replacement.size();
		}
		return ReplaceIdentifiers(raw, substitutions);
	}

	CPPGMAstNodePtr TransformCallExpression(const CPPGMAstNodePtr& input,
		const string& context, const map<string, string>& substitutions)
	{
		CPPGMAstNodePtr result(new CPPGMAstNode(input->kind, input->value));
		result->initializer_form = input->initializer_form;
		if(!input->children.empty() && input->children[0] &&
			input->children[0]->kind == "id-expression") {
			const string raw_callee = input->children[0]->value;
			const size_t open = raw_callee.find('<');
			if(open != string::npos) {
				string base;
				size_t begin = 0;
				string argument_text;
				size_t close = string::npos;
				const TemplateDefinition* explicit_definition = 0;
				if(TemplateBase(raw_callee, open, &begin, &base) &&
					TemplateRange(raw_callee, open, &argument_text, &close))
					explicit_definition = FindDefinition(base, context);
				if(explicit_definition && !explicit_definition->class_template) {
					vector<string> explicit_args = SplitTemplateArguments(argument_text);
					for(size_t i = 0; i < explicit_args.size(); ++i)
						explicit_args[i] = NormalizeTypeArgument(RewriteText(
							explicit_args[i], context, substitutions, 0));
					vector<string> complete_args;
					bool complete = explicit_args.size() == explicit_definition->parameters.size();
					if(complete) complete_args = explicit_args;
					else if(explicit_args.size() < explicit_definition->parameters.size())
						complete = InferFunctionArguments(*explicit_definition, input,
							&complete_args, substitutions, context, &explicit_args);
					if(complete) {
						const string local_name = Instantiate(*explicit_definition, complete_args, context);
						const string qualifier = PrefixComponent(base);
						CPPGMAstNodePtr callee(new CPPGMAstNode("id-expression",
							qualifier.empty() ? local_name : qualifier + "::" + local_name));
						result->children.push_back(callee);
						for(size_t i = 1; i < input->children.size(); ++i) {
							CPPGMAstNodePtr child = TransformNode(input->children[i], context, substitutions);
							if(child) result->children.push_back(child);
						}
						return result;
					}
				}
			}
		}
		for(size_t i = 0; i < input->children.size(); ++i) {
			CPPGMAstNodePtr child = TransformNode(input->children[i], context, substitutions);
			if(child) result->children.push_back(child);
		}
		if(!result->children.empty() && result->children[0] &&
			result->children[0]->kind == "id-expression" &&
			result->children[0]->value.find('<') == string::npos) {
			const string callee_name = result->children[0]->value;
			const vector<const TemplateDefinition*> definitions =
				FindFunctionDefinitions(callee_name, context);
			for(size_t candidate = 0; candidate < definitions.size(); ++candidate) {
				const TemplateDefinition* definition = definitions[candidate];
				vector<string> inferred;
				if(InferFunctionArguments(*definition, result, &inferred,
					substitutions, context)) {
					const string local_name = Instantiate(*definition, inferred, context);
					const string qualifier = PrefixComponent(callee_name);
					result->children[0]->value = qualifier.empty() ? local_name : qualifier + "::" + local_name;
					break;
				}
			}
			if(definitions.empty()) {
				const FunctionSignature* signature = FindFunctionSignature(callee_name, context);
				if(signature && callee_name.find("::") == string::npos) {
					for(map<string, FunctionSignature>::const_iterator found = function_signatures_.begin();
						found != function_signatures_.end(); ++found)
						if(&found->second == signature) {
							result->children[0]->value = found->first;
							break;
						}
				}
				ResolveFunctionArguments(result, signature, context);
			}
		}
		return result;
	}

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

	void TransformRegularChildren(const CPPGMAstNodePtr& input,
		const string& child_context, const string& function_context,
		const map<string, string>& substitutions,
		map<string, string>* local_substitutions,
		const CPPGMAstNodePtr& result)
	{
		for(size_t i = 0; i < input->children.size(); ++i) {
			const CPPGMAstNodePtr original_child = input->children[i];
			if((input->kind == "class-specifier" || input->kind == "class-forward-declaration") &&
				original_child && (original_child->kind == "class-specifier" ||
					original_child->kind == "class-forward-declaration")) {
				const string nested_name = LastComponent(original_child->value);
				bool used = false;
				const string class_key = JoinPath(child_context, LastComponent(input->value));
				map<string, set<string> >::const_iterator requested =
					requested_nested_classes_.find(class_key);
				if(requested == requested_nested_classes_.end())
					requested = requested_nested_classes_.find(LastComponent(input->value));
				if(requested != requested_nested_classes_.end() &&
					requested->second.find(nested_name) != requested->second.end()) used = true;
				bool has_sibling = false;
				for(size_t sibling = 0; sibling < input->children.size(); ++sibling)
					if(sibling != i && input->children[sibling] &&
						input->children[sibling]->kind != "class-key") {
						has_sibling = true;
						if(ContainsName(input->children[sibling], nested_name)) {
							used = true;
							break;
						}
					}
				if(has_sibling && !used) continue;
			}
			const string node_context = input->kind == "function-definition" &&
				original_child && original_child->kind == "compound-statement" ?
				function_context : child_context;
			CPPGMAstNodePtr child = TransformNode(original_child, node_context, *local_substitutions);
			if(child && !(input->kind == "compound-statement" &&
				(original_child->kind == "alias-declaration" ||
					(original_child->kind == "simple-declaration" &&
					 SpellNode(original_child->children.empty() ? CPPGMAstNodePtr() :
						original_child->children[0]).find("typedef") != string::npos))))
				result->children.push_back(child);
			if(original_child && original_child->kind == "alias-declaration" &&
				!original_child->value.empty() && !original_child->children.empty())
				(*local_substitutions)[original_child->value] = RewriteText(
					SpellNode(original_child->children[0]), child_context, *local_substitutions, 0);
			if(original_child && original_child->kind == "using-declaration") {
				const CPPGMAstNodePtr target = ChildOfKindLocal(original_child, "target");
				if(target && !target->value.empty()) {
					const string target_name = LastComponent(target->value);
					const string owner = PrefixComponent(target->value);
					if(!owner.empty()) (*local_substitutions)[target_name] = target->value;
				}
			}
			if(original_child && original_child->kind == "using-directive")
				RecordUsingDirective(original_child, local_substitutions);
			if(original_child && original_child->kind == "simple-declaration" &&
				!original_child->children.empty() &&
				SpellNode(original_child->children[0]).find("typedef") != string::npos)
				RecordTypedefSubstitutions(original_child, child_context, local_substitutions);
		}
	}

	void RecordUsingDirective(const CPPGMAstNodePtr& original_child,
		map<string, string>* local_substitutions)
	{
		const CPPGMAstNodePtr target = ChildOfKindLocal(original_child, "target");
		if(!target || target->value.empty()) return;
		const string prefix = target->value + "::";
		for(set<string>::const_iterator type = class_contexts_.begin();
			type != class_contexts_.end(); ++type) {
			if(type->compare(0, prefix.size(), prefix) != 0) continue;
			const string relative = type->substr(prefix.size());
			const size_t separator = relative.find("::");
			const string visible = relative.substr(0, separator);
			if(!visible.empty() && local_substitutions->find(visible) == local_substitutions->end())
				(*local_substitutions)[visible] = target->value + "::" + visible;
		}
		for(map<string, TemplateDefinition>::const_iterator definition = definitions_.begin();
			definition != definitions_.end(); ++definition) {
			if(definition->second.qualified_name.compare(0, prefix.size(), prefix) != 0) continue;
			const string relative = definition->second.qualified_name.substr(prefix.size());
			const size_t separator = relative.find("::");
			const string visible = relative.substr(0, separator);
			if(!visible.empty() && local_substitutions->find(visible) == local_substitutions->end())
				(*local_substitutions)[visible] = target->value + "::" + visible;
		}
	}

	void RecordTypedefSubstitutions(const CPPGMAstNodePtr& original_child,
		const string& child_context, map<string, string>* local_substitutions)
	{
		const CPPGMAstNodePtr list = ChildOfKindLocal(original_child, "init-declarator-list");
		if(!list) return;
		for(size_t item_index = 0; item_index < list->children.size(); ++item_index) {
			const CPPGMAstNodePtr item = list->children[item_index];
			if(!item || item->children.empty()) continue;
			const string alias_name = FirstIdentifierLocal(item->children[0]);
			if(!alias_name.empty()) (*local_substitutions)[alias_name] = RewriteText(
				NodeTypeSpelling(original_child->children[0]) +
				DeclaratorSuffix(item->children[0]), child_context, *local_substitutions, 0);
		}
	}

	CPPGMAstNodePtr TransformRegularNode(const CPPGMAstNodePtr& input,
		const string& context, const map<string, string>& substitutions)
	{
		CPPGMAstNodePtr result(new CPPGMAstNode(input->kind, input->value));
		result->initializer_form = input->initializer_form;
		bool template_replaced = false;
		result->value = RewriteText(input->value, context, substitutions, &template_replaced);
		if(input->kind == "decl-specifier" || input->kind == "type-name" ||
			input->kind == "type-specifier") {
			const size_t marker_colon = result->value.find(':');
			const string marker = marker_colon == string::npos ? string() :
				result->value.substr(0, marker_colon + 1);
			const string spelling = RemoveMarker(result->value);
			const string qualified = QualifyTypeArgument(spelling, context);
			if(qualified != spelling) result->value = marker + qualified;
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
		string child_context = context;
		if(input->kind == "class-specifier" || input->kind == "class-forward-declaration")
			child_context = JoinPath(context, LastComponent(input->value));
		string function_context = context;
		if(input->kind == "function-definition") {
			const string function_name = DeclarationName(input);
			function_context = JoinPath(context, function_name);
			if(!function_name.empty() && LastComponent(context) == function_name)
				function_context = context;
		}
		map<string, string> local_substitutions = substitutions;
		TransformRegularChildren(input, child_context, function_context, substitutions,
			&local_substitutions, result);
		if(input->kind == "binary-expression") {
			InstantiateOperatorTemplate(result, context, substitutions);
			RewriteOperatorFunctionArgument(result, context, substitutions);
		}
		if(!promoted_local_class.empty()) {
			const map<string, string>::const_iterator owner = function_owners_.find(context);
			generated_by_owner_[owner == function_owners_.end() ? PrefixComponent(context) :
				owner->second].push_back(result);
			return CPPGMAstNodePtr();
		}
		return result;
	}

	CPPGMAstNodePtr TransformNode(const CPPGMAstNodePtr& input, const string& context,
		const map<string, string>& substitutions)
	{
		if(!input) return CPPGMAstNodePtr();
		if(input->kind == "template-declaration") {
			if(input->children.size() > 1 && input->children[1] &&
				(input->children[1]->kind == "class-specifier" ||
					input->children[1]->kind == "class-forward-declaration"))
				return MakeClassShell(LastComponent(input->children[1]->value));
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
				map<string, string>::const_iterator marker = function_marker_names_.find(substitution->second);
				if(marker == function_marker_names_.end()) continue;
				map<string, FunctionSignature>::const_iterator signature = function_signatures_.find(marker->second);
				if(signature != function_signatures_.end())
					return FunctionParameter(input, signature->second, substitution->second);
				const FunctionSignature* recovered = FindFunctionSignature(marker->second, context);
				if(recovered) return FunctionParameter(input, *recovered, substitution->second);
			}
		}
		if(input->kind == "namespace-definition") return TransformNamespace(input, context, substitutions);
		if(input->kind == "call-expression") return TransformCallExpression(input, context, substitutions);
		return TransformRegularNode(input, context, substitutions);
	}
};

} // namespace
