#pragma once

	string InferLiteralArgumentType(const string& value) const
	{
		const bool floating = value.find('.') != string::npos || value.find('e') != string::npos ||
			value.find('E') != string::npos || value.find('p') != string::npos ||
			value.find('P') != string::npos;
		if(floating) {
			const char suffix = value.empty() ? 0 : value[value.size() - 1];
			return suffix == 'f' || suffix == 'F' ? "float" :
				suffix == 'l' || suffix == 'L' ? "long double" : "double";
		}
		size_t begin = value.size();
		while(begin && string("uUlL").find(value[begin - 1]) != string::npos) --begin;
		const string suffix = value.substr(begin);
		const bool uns = suffix.find('u') != string::npos || suffix.find('U') != string::npos;
		const size_t longs = suffix.find('l') != string::npos || suffix.find('L') != string::npos ?
			(suffix.size() > 1 ? 2U : 1U) : 0U;
		return longs >= 2 ? (uns ? "unsigned long long" : "long long") :
			longs == 1 ? (uns ? "unsigned long" : "long") : uns ? "unsigned int" : "int";
	}

	bool HasExactOrdinaryMatch(const CPPGMAstNodePtr& call, const string& callee,
		const map<string, string>& substitutions, const string& context)
	{
		const FunctionSignature* signature = FindFunctionSignature(callee, context);
		if(!signature || !signature->parameters || call->children.size() < 2) return false;
		const CPPGMAstNodePtr arguments = call->children[1] &&
			call->children[1]->kind == "argument-list" ? call->children[1] :
			ChildOfKindLocal(call->children[1], "argument-list");
		if(!arguments) return false;
		size_t argument = 0;
		for(size_t parameter = 0; parameter < signature->parameters->children.size(); ++parameter) {
			const CPPGMAstNodePtr parameter_node = signature->parameters->children[parameter];
			if(!parameter_node || parameter_node->kind != "parameter-declaration") continue;
			if(argument >= arguments->children.size()) break;
			string actual;
			if(!InferArgument(arguments->children[argument], &actual, substitutions, context)) return false;
			const string expected = NormalizeTypeArgument(RewriteText(
				ParameterTypeSpelling(parameter_node), context, substitutions, 0));
			if(CanonicalSpelling(actual) != expected) return false;
			++argument;
		}
		return argument == arguments->children.size();
	}

	bool SplitTopLevelComma(const string& raw, string* tail) const
	{
		int angle = 0, parentheses = 0, brackets = 0;
		for(size_t i = 0; i < raw.size(); ++i) {
			const char ch = raw[i];
			if(ch == '<') ++angle;
			else if(ch == '>' && angle > 0) --angle;
			else if(ch == '(') ++parentheses;
			else if(ch == ')' && parentheses > 0) --parentheses;
			else if(ch == '[') ++brackets;
			else if(ch == ']' && brackets > 0) --brackets;
			else if(ch == ',' && angle == 0 && parentheses == 0 && brackets == 0) {
				if(tail) *tail = Trim(raw.substr(i + 1));
				return true;
			}
		}
		return false;
	}

	string StripTextParentheses(string raw) const
	{
		raw = Trim(raw);
		for(;;) {
			if(raw.size() < 2 || raw[0] != '(' || raw[raw.size() - 1] != ')') return raw;
			int depth = 0;
			bool encloses = true;
			for(size_t i = 0; i < raw.size(); ++i) {
				if(raw[i] == '(') ++depth;
				else if(raw[i] == ')' && --depth == 0 && i + 1 != raw.size()) {
					encloses = false;
					break;
				}
			}
			if(!encloses || depth != 0) return raw;
			raw = Trim(raw.substr(1, raw.size() - 2));
		}
	}

	bool SplitTextCall(const string& raw, string* callee, string* arguments) const
	{
		const string spelling = Trim(raw);
		if(spelling.empty() || spelling[spelling.size() - 1] != ')') return false;
		int depth = 0;
		size_t open = string::npos;
		for(size_t i = spelling.size(); i > 0; --i) {
			const char ch = spelling[i - 1];
			if(ch == ')') ++depth;
			else if(ch == '(' && --depth == 0) {
				open = i - 1;
				break;
			}
		}
		if(open == string::npos) return false;
		const string prefix = Trim(spelling.substr(0, open));
		if(prefix.empty()) return false;
		if(callee) *callee = StripTextParentheses(prefix);
		if(arguments) *arguments = spelling.substr(open + 1, spelling.size() - open - 2);
		return true;
	}

	string CollapseReferenceSpelling(string raw) const
	{
		raw = CanonicalSpelling(raw);
		for(size_t i = 0; i + 2 < raw.size();) {
			if(raw[i] == '&' && raw[i + 1] == '&' && raw[i + 2] == '&') {
				raw.replace(i, 3, "&");
				continue;
			}
			++i;
		}
		return CanonicalSpelling(raw);
	}

	bool InferFunctionTypeArguments(const TemplateDefinition& definition,
		const vector<string>& actual_types, vector<string>* result,
		const map<string, string>& substitutions, const string& context,
		const vector<string>* explicit_prefix = 0)
	{
		if(!result || definition.class_template) return false;
		const CPPGMAstNodePtr declarator = FunctionDeclarator(definition.declaration);
		const CPPGMAstNodePtr parameters = DescendantOfKind(declarator, "parameter-clause");
		if(!parameters) return false;
		map<string, string> inferred;
		set<string> parameter_names;
		for(size_t i = 0; i < definition.parameters.size(); ++i) {
			parameter_names.insert(definition.parameters[i].name);
			if(explicit_prefix && i < explicit_prefix->size())
				inferred[definition.parameters[i].name] = (*explicit_prefix)[i];
		}
		size_t actual = 0;
		for(size_t i = 0; i < parameters->children.size(); ++i) {
			const CPPGMAstNodePtr parameter = parameters->children[i];
			if(!parameter || parameter->kind != "parameter-declaration") continue;
			if(actual >= actual_types.size()) break;
			const string pattern = ParameterTypeSpelling(parameter);
			bool dependent = false;
			for(size_t p = 0; p < definition.parameters.size() && !dependent; ++p) {
				const string& name = definition.parameters[p].name;
				for(size_t at = 0; at + name.size() <= pattern.size(); ++at)
					if(pattern.compare(at, name.size(), name) == 0 &&
						(at == 0 || !IsIdentifierCharacter(pattern[at - 1])) &&
						(at + name.size() == pattern.size() ||
							!IsIdentifierCharacter(pattern[at + name.size()]))) {
						dependent = true;
						break;
					}
			}
			if(dependent) {
				const string dependent_pattern = CanonicalSpelling(pattern);
				const string actual_type = CollapseReferenceSpelling(actual_types[actual]);
				if(dependent_pattern.size() > 2 &&
					dependent_pattern.compare(dependent_pattern.size() - 2, 2, "&&") == 0 &&
					!actual_type.empty() && actual_type[actual_type.size() - 1] == '&') {
					const string base = CanonicalSpelling(dependent_pattern.substr(
						0, dependent_pattern.size() - 2));
					if(parameter_names.find(base) != parameter_names.end())
						inferred[base] = actual_type;
					else if(!MatchTypePattern(dependent_pattern, actual_type,
						parameter_names, &inferred, context)) return false;
				} else if(!MatchTypePattern(dependent_pattern, actual_type,
					parameter_names, &inferred, context)) return false;
			}
			++actual;
		}
		for(size_t i = 0; i < definition.parameters.size(); ++i) {
			map<string, string>::const_iterator found = inferred.find(definition.parameters[i].name);
			if(found != inferred.end()) result->push_back(found->second);
			else if(!definition.parameters[i].default_type.empty())
				result->push_back(RewriteText(definition.parameters[i].default_type,
					context, inferred, 0));
			else return false;
		}
		(void)substitutions;
		return true;
	}

	string FunctionResultType(const TemplateDefinition& definition,
		const vector<string>& arguments, const string& context)
	{
		if(!definition.declaration || definition.declaration->children.empty()) return string();
		map<string, string> local;
		for(size_t i = 0; i < definition.parameters.size() && i < arguments.size(); ++i)
			local[definition.parameters[i].name] = arguments[i];
		const CPPGMAstNodePtr declarator = FunctionDeclarator(definition.declaration);
		string result = NodeTypeSpelling(definition.declaration->children[0]);
		result += DeclaratorSuffix(declarator);
		result = RewriteText(result, context, local, 0);
		result = CollapseReferenceSpelling(ReplaceIdentifiers(result, local));
		return NormalizeTypeArgument(result);
	}

	bool FunctionCallResultType(string expression, const string& context,
		const map<string, string>& substitutions, string* result)
	{
		if(!result) return false;
		string callee, argument_text;
		if(!SplitTextCall(expression, &callee, &argument_text)) return false;
		callee = StripTextParentheses(callee);
		if(callee.empty()) return false;
		if(callee[callee.size() - 1] == ')') {
			string returned;
			if(FunctionCallResultType(callee, context, substitutions, &returned)) {
				returned = ResolveAlias(CollapseReferenceSpelling(returned), context);
				while(!returned.empty() && returned[returned.size() - 1] == '&') {
					returned.erase(returned.size() - 1);
					returned = CanonicalSpelling(returned);
				}
				string pointed_result;
				if(SplitFunctionPointerType(returned, &pointed_result, 0) &&
					!pointed_result.empty()) {
					*result = pointed_result;
					return true;
				}
			}
		}
		vector<string> explicit_arguments;
		const TemplateDefinition* explicit_definition = 0;
		const size_t template_open = callee.find('<');
		if(template_open != string::npos) {
			string base_arguments, base;
			size_t template_close = string::npos, begin = 0;
			if(!TemplateBase(callee, template_open, &begin, &base) ||
				!TemplateRange(callee, template_open, &base_arguments, &template_close)) return false;
			explicit_definition = FindDefinition(base, context);
			if(!explicit_definition || explicit_definition->class_template) return false;
			explicit_arguments = SplitTemplateArguments(base_arguments);
			for(size_t i = 0; i < explicit_arguments.size(); ++i) {
				explicit_arguments[i] = RewriteText(explicit_arguments[i], context,
					substitutions, 0);
				explicit_arguments[i] = NormalizeTypeArgument(ReplaceIdentifiers(
					explicit_arguments[i], substitutions));
				explicit_arguments[i] = ResolveAlias(explicit_arguments[i], context);
			}
		}
		vector<string> actual_types;
		const vector<string> actual_expressions = SplitTemplateArguments(argument_text);
		for(size_t i = 0; i < actual_expressions.size(); ++i) {
			if(actual_expressions[i].empty()) continue;
			const string actual = ExpressionTypeSpelling(actual_expressions[i], context, substitutions);
			if(actual.empty()) return false;
			actual_types.push_back(actual);
		}
		vector<const TemplateDefinition*> candidates;
		if(explicit_definition) candidates.push_back(explicit_definition);
		else candidates = FindFunctionDefinitions(callee, context);
		for(size_t i = 0; i < candidates.size(); ++i) {
			const TemplateDefinition& definition = *candidates[i];
			vector<string> arguments;
			const bool complete = explicit_definition &&
				explicit_arguments.size() == definition.parameters.size();
			if(complete) arguments = explicit_arguments;
			else if(!InferFunctionTypeArguments(definition, actual_types, &arguments,
				substitutions, context, explicit_definition ? &explicit_arguments : 0)) continue;
			*result = FunctionResultType(definition, arguments, context);
			if(!result->empty()) return true;
		}
		if(!explicit_definition) {
			const FunctionSignature* signature = FindFunctionSignature(callee, context);
			if(signature && signature->result_specifiers) {
				*result = NodeTypeSpelling(signature->result_specifiers);
				return !result->empty();
			}
		}
		return false;
	}

	string FunctionTemplateIdType(string expression, const string& context,
		const map<string, string>& substitutions)
	{
		const size_t open = expression.find('<');
		if(open == string::npos || expression.empty() || expression[expression.size() - 1] != '>')
			return string();
		string arguments_text, base;
		size_t close = string::npos, begin = 0;
		if(!TemplateBase(expression, open, &begin, &base) ||
			!TemplateRange(expression, open, &arguments_text, &close)) return string();
		if(close + 1 != expression.size()) return string();
		const TemplateDefinition* definition = FindDefinition(base, context);
		if(!definition || definition->class_template) return string();
		vector<string> arguments = SplitTemplateArguments(arguments_text);
		for(size_t i = 0; i < arguments.size(); ++i) {
			arguments[i] = NormalizeTypeArgument(RewriteText(arguments[i], context,
				substitutions, 0));
			arguments[i] = NormalizeTypeArgument(ReplaceIdentifiers(arguments[i], substitutions));
			arguments[i] = ResolveAlias(arguments[i], context);
		}
		if(arguments.size() < definition->parameters.size())
			for(size_t i = arguments.size(); i < definition->parameters.size(); ++i)
				if(!definition->parameters[i].default_type.empty())
					arguments.push_back(RewriteText(definition->parameters[i].default_type,
						context, substitutions, 0));
		if(arguments.size() != definition->parameters.size()) return string();
		const CPPGMAstNodePtr clause = DescendantOfKind(FunctionDeclarator(definition->declaration),
			"parameter-clause");
		if(!clause) return string();
		map<string, string> local = substitutions;
		for(size_t i = 0; i < definition->parameters.size(); ++i)
			local[definition->parameters[i].name] = arguments[i];
		string result = FunctionResultType(*definition, arguments, context) + "(*) (";
		for(size_t i = 0; i < clause->children.size(); ++i) {
			const CPPGMAstNodePtr parameter = clause->children[i];
			if(!parameter || parameter->kind != "parameter-declaration") continue;
			if(result[result.size() - 1] != '(') result += ',';
			result += RewriteText(ParameterTypeSpelling(parameter), context, local, 0);
		}
		result += ')';
		return CanonicalSpelling(result);
	}

	string BinaryExpressionType(string expression, const string& context,
		const map<string, string>& substitutions)
	{
		int angle = 0, parentheses = 0, brackets = 0;
		for(size_t i = 0; i + 1 < expression.size(); ++i) {
			const char ch = expression[i];
			if(ch == '(') ++parentheses;
			else if(ch == ')' && parentheses > 0) --parentheses;
			else if(ch == '[') ++brackets;
			else if(ch == ']' && brackets > 0) --brackets;
			else if(ch == '<' && expression[i + 1] != '<') ++angle;
			else if(ch == '>' && angle > 0) --angle;
			if(angle != 0 || parentheses != 0 || brackets != 0 ||
				expression.compare(i, 2, "<<") != 0) continue;
			const string left = ExpressionTypeSpelling(expression.substr(0, i),
				context, substitutions);
			const string right = ExpressionTypeSpelling(expression.substr(i + 2),
				context, substitutions);
			if(left.empty() || right.empty()) return string();
			CPPGMAstNodePtr declaration = FindClassDeclaration(left, context);
			if(!declaration) return left;
			for(size_t child = 0; child < declaration->children.size(); ++child) {
				const CPPGMAstNodePtr member = declaration->children[child];
				if(!member || member->kind != "function-definition" || member->children.size() < 2 ||
					LastComponent(FirstIdentifierLocal(member->children[1])) != "operator<<") continue;
				return RewriteText(NodeTypeSpelling(member->children[0]) +
					DeclaratorSuffix(member->children[1]), context, substitutions, 0);
			}
			return left;
		}
		return string();
	}

	string ExpressionTypeSpelling(string expression, const string& context,
		const map<string, string>& substitutions)
	{
		expression = StripTextParentheses(CanonicalSpelling(expression));
		string comma_tail;
		if(SplitTopLevelComma(expression, &comma_tail))
			return ExpressionTypeSpelling(comma_tail, context, substitutions);
		if(expression.compare(0, 8, "decltype(") == 0 && expression.size() > 9 &&
			expression[expression.size() - 1] == ')') {
			string nested;
			if(EvaluateDecltypeExpression(expression.substr(8, expression.size() - 9),
				context, substitutions, &nested)) return nested;
		}
		string call_type;
		if(FunctionCallResultType(expression, context, substitutions, &call_type)) return call_type;
		const string function_type = FunctionTemplateIdType(expression, context, substitutions);
		if(!function_type.empty()) return function_type;
		const string binary_type = BinaryExpressionType(expression, context, substitutions);
		if(!binary_type.empty()) return binary_type;
		if(expression.size() > 1 && (expression[0] == '*' || expression[0] == '&')) {
			const string inner = ExpressionTypeSpelling(expression.substr(1), context, substitutions);
			if(!inner.empty()) {
				if(expression[0] == '*') {
					if(inner[inner.size() - 1] == '*')
						return CanonicalSpelling(inner.substr(0, inner.size() - 1) + "&");
					return CanonicalSpelling(inner + "&");
				}
				return CanonicalSpelling(inner + "*");
			}
		}
		const size_t open = expression.find('(');
		if(open != string::npos && expression[expression.size() - 1] == ')') {
			string cast_type = ReplaceIdentifiers(Trim(expression.substr(0, open)), substitutions);
			cast_type = ResolveAlias(cast_type, context);
			if(!cast_type.empty()) return NormalizeTypeArgument(cast_type);
		}
		map<string, string>::const_iterator substituted = substitutions.find(expression);
		if(substituted != substitutions.end()) return substituted->second;
		map<string, string>::const_iterator variable = variable_types_.find(LastComponent(expression));
		if(variable != variable_types_.end())
			return ReplaceIdentifiers(ResolveAlias(variable->second, context), substitutions);
		if(expression == "true" || expression == "false") return "bool";
		if(!expression.empty() && (isdigit(static_cast<unsigned char>(expression[0])) ||
			expression[0] == '\'' || expression[0] == '"'))
			return expression[0] == '\'' ? "char" : "int";
		return string();
	}

	bool EvaluateDecltypeExpression(const string& expression, const string& context,
		const map<string, string>& substitutions, string* result)
	{
		if(!result) return false;
		string tail;
		if(SplitTopLevelComma(expression, &tail))
			return EvaluateDecltypeExpression(tail, context, substitutions, result);
		const string normalized = StripTextParentheses(expression);
		if(normalized != expression)
			return EvaluateDecltypeExpression(normalized, context, substitutions, result);
		if(normalized.compare(0, 7, "sizeof(") == 0 ||
			normalized.compare(0, 7, "alignof(") == 0) {
			*result = "unsigned long";
			return true;
		}
		if(FunctionCallResultType(normalized, context, substitutions, result)) return true;
		const size_t open = normalized.find('(');
		if(open != string::npos && normalized[normalized.size() - 1] == ')') {
			*result = NormalizeTypeArgument(ResolveAlias(ReplaceIdentifiers(
				Trim(normalized.substr(0, open)), substitutions), context));
			return !result->empty();
		}
		*result = ExpressionTypeSpelling(normalized, context, substitutions);
		return !result->empty();
	}

	string TemplateMemberType(const TemplateDefinition& definition,
		const vector<string>& arguments, const string& member, const string& context)
	{
		if(!definition.declaration) return string();
		map<string, string> local;
		for(size_t i = 0; i < definition.parameters.size() && i < arguments.size(); ++i)
			local[definition.parameters[i].name] = arguments[i];
		for(size_t i = 0; i < definition.declaration->children.size(); ++i) {
			const CPPGMAstNodePtr child = definition.declaration->children[i];
			if(!child) continue;
			string spelling;
			if(child->kind == "alias-declaration" && child->value == member &&
				!child->children.empty()) spelling = TypeIdSpelling(child->children[0]);
			else if(child->kind == "simple-declaration" && !child->children.empty() &&
				SpellNode(child->children[0]).find("typedef") != string::npos) {
				const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
				if(list) for(size_t j = 0; j < list->children.size(); ++j) {
					const CPPGMAstNodePtr item = list->children[j];
					if(!item || item->children.empty() ||
						LastComponent(FirstIdentifierLocal(item->children[0])) != member) continue;
					if(!DeclaratorArraySuffix(item->children[0]).empty()) continue;
					spelling = NodeTypeSpelling(child->children[0]) +
						DeclaratorSuffix(item->children[0]) +
						DeclaratorArraySuffix(item->children[0]);
					break;
				}
			}
			if(!spelling.empty()) {
				const string result = ResolveAlias(RewriteText(spelling, context, local, 0), context);
				return result;
			}
		}
		return string();
	}

	void ReifyReferenceType(const CPPGMAstNodePtr& result) const
	{
		if(!result || result->kind != "simple-declaration" || result->children.empty()) return;
		const CPPGMAstNodePtr specs = result->children[0];
		if(!specs) return;
		CPPGMAstNodePtr type_specifier;
		for(size_t i = 0; i < specs->children.size(); ++i) {
			const CPPGMAstNodePtr child = specs->children[i];
			if(child && (child->kind == "decl-specifier" || child->kind == "type-name" ||
				child->kind == "type-specifier")) type_specifier = child;
		}
		if(!type_specifier) return;
		const string spelling = RemoveMarker(type_specifier->value);
		string suffix;
		if(spelling.size() >= 2 && spelling.compare(spelling.size() - 2, 2, "&&") == 0)
			suffix = "&&";
		else if(!spelling.empty() && spelling[spelling.size() - 1] == '&') suffix = "&";
		if(suffix.empty() || spelling.find("(*)") != string::npos) return;
		string marker;
		const size_t colon = type_specifier->value.find(':');
		if(colon != string::npos) marker = type_specifier->value.substr(0, colon + 1);
		type_specifier->value = marker + spelling.substr(0, spelling.size() - suffix.size());
		const CPPGMAstNodePtr list = ChildOfKindLocal(result, "init-declarator-list");
		if(!list) return;
		for(size_t i = 0; i < list->children.size(); ++i) {
			const CPPGMAstNodePtr item = list->children[i];
			if(!item || item->children.empty()) continue;
			const CPPGMAstNodePtr declarator = item->children[0];
			if(!declarator || DeclaratorSuffix(declarator).find('&') != string::npos) continue;
			declarator->children.insert(declarator->children.begin(),
				CPPGMAstNodePtr(new CPPGMAstNode("ptr-operator",
					suffix == "&&" ? "OP_LAND:&&" : "OP_AMP:&")));
		}
	}

	void CollectCallArguments(const CPPGMAstNodePtr& node,
		vector<CPPGMAstNodePtr>* arguments) const
	{
		if(!node || !arguments) return;
		if(node->kind == "parenthesized-expression" && node->children.size() == 1) {
			CollectCallArguments(node->children[0], arguments);
			return;
		}
		if(node->kind == "binary-expression" && RemoveMarker(node->value) == "," &&
			node->children.size() >= 2) {
			CollectCallArguments(node->children[0], arguments);
			CollectCallArguments(node->children[1], arguments);
			return;
		}
		arguments->push_back(node);
	}

	CPPGMAstNodePtr RewriteTemplateCastCall(const CPPGMAstNodePtr& input,
		const string& context, const map<string, string>& substitutions)
	{
		if(!input || input->kind != "cast-expression" || input->children.size() < 2)
			return CPPGMAstNodePtr();
		const CPPGMAstNodePtr type_id = ChildOfKindLocal(input, "type-id");
		if(!type_id || input->children[1] == type_id) return CPPGMAstNodePtr();
		const string raw_type = SpellNode(type_id);
		const size_t open = raw_type.find('<');
		if(open == string::npos) return CPPGMAstNodePtr();
		string argument_text, base;
		size_t close = string::npos, begin = 0;
		if(!TemplateBase(raw_type, open, &begin, &base) ||
			!TemplateRange(raw_type, open, &argument_text, &close)) return CPPGMAstNodePtr();
		const TemplateDefinition* definition = FindDefinition(base, context);
		if(!definition || definition->class_template) return CPPGMAstNodePtr();
		vector<string> arguments = SplitTemplateArguments(argument_text);
		for(size_t i = 0; i < arguments.size(); ++i) {
			arguments[i] = NormalizeTypeArgument(RewriteText(arguments[i], context,
				substitutions, 0));
			arguments[i] = NormalizeTypeArgument(ReplaceIdentifiers(arguments[i], substitutions));
		}
		if(arguments.size() != definition->parameters.size()) return CPPGMAstNodePtr();
		const string local_name = Instantiate(*definition, arguments, context);
		const string qualifier = PrefixComponent(base);
		CPPGMAstNodePtr result(new CPPGMAstNode("call-expression"));
		result->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("id-expression",
			qualifier.empty() ? local_name : qualifier + "::" + local_name)));
		CPPGMAstNodePtr call_arguments(new CPPGMAstNode("argument-list"));
		vector<CPPGMAstNodePtr> raw_arguments;
		CollectCallArguments(input->children[1], &raw_arguments);
		for(size_t i = 0; i < raw_arguments.size(); ++i) {
			CPPGMAstNodePtr argument = TransformNode(raw_arguments[i], context, substitutions);
			if(argument) call_arguments->children.push_back(argument);
		}
		result->children.push_back(call_arguments);
		return result;
	}
