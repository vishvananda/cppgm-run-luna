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

	bool HasMaterializedMemberFunction(const string& callee,
		const string& context) const
	{
		const size_t separator = callee.rfind("::");
		if(separator == string::npos) return false;
		const string owner = callee.substr(0, separator);
		const string member = callee.substr(separator + 2);
		const CPPGMAstNodePtr declaration = FindClassDeclaration(owner, context);
		if(!declaration) return false;
		for(size_t i = 0; i < declaration->children.size(); ++i) {
			const CPPGMAstNodePtr child = declaration->children[i];
			if(!child) continue;
			if((child->kind == "simple-declaration" ||
				child->kind == "function-definition" ||
				child->kind == "special-member-definition") &&
				LastComponent(DeclarationName(child)) == member &&
				DescendantOfKind(child, "parameter-clause")) return true;
		}
		return false;
	}

	bool HasExactOrdinaryMatch(const CPPGMAstNodePtr& call, const string& callee,
		const map<string, string>& substitutions, const string& context)
	{
		const FunctionSignature* signature = FindFunctionSignature(callee, context);
		// A call from a class member must prefer a member declaration over an
		// out-of-class function-template definition with the same source name.
		// The member's parameter spelling can still be dependent in the source
		// class, so type equality alone cannot identify this ordinary match.
		if(callee.find("::") == string::npos) {
			const string owner = PrefixComponent(context);
			const FunctionSignature* member = 0;
			if(!owner.empty()) {
				map<string, FunctionSignature>::const_iterator found = function_signatures_.find(
					JoinPath(owner, callee));
				if(found != function_signatures_.end()) member = &found->second;
				if(!member) {
					found = function_signatures_.find(JoinPath(owner, JoinPath(owner, callee)));
					if(found != function_signatures_.end()) member = &found->second;
				}
			}
			const bool class_scope = class_contexts_.find(owner) != class_contexts_.end() ||
				(!owner.empty() && class_contexts_.find(JoinPath(owner, owner)) != class_contexts_.end());
			if(member && class_scope && (!signature || member == signature))
				return true;
		}
		if(!signature || !signature->parameters || call->children.size() < 2) return false;
		const vector<const TemplateDefinition*> templates = FindFunctionDefinitions(callee, context);
		const CPPGMAstNodePtr arguments = call->children[1] &&
			call->children[1]->kind == "argument-list" ? call->children[1] :
			ChildOfKindLocal(call->children[1], "argument-list");
		if(!arguments) return false;
		size_t parameter_count = 0;
		size_t required_parameters = 0;
		if(!FunctionParameterCounts(signature->parameters, &parameter_count,
			&required_parameters)) return false;
		if(arguments->children.size() < required_parameters ||
			arguments->children.size() > parameter_count) return false;
		size_t argument = 0;
		for(size_t parameter = 0; parameter < signature->parameters->children.size(); ++parameter) {
			const CPPGMAstNodePtr parameter_node = signature->parameters->children[parameter];
			if(!parameter_node || parameter_node->kind != "parameter-declaration") continue;
			if(argument >= arguments->children.size()) break;
			const string pattern = ParameterTypeSpelling(parameter_node);
			for(size_t candidate = 0; candidate < templates.size(); ++candidate) {
				const CPPGMAstNodePtr declarator = FunctionDeclarator(templates[candidate]->declaration);
				const CPPGMAstNodePtr parameters = DescendantOfKind(declarator, "parameter-clause");
				if(!parameters || parameter >= parameters->children.size()) continue;
				const CPPGMAstNodePtr template_parameter = parameters->children[parameter];
				if(template_parameter && template_parameter->kind == "parameter-declaration" &&
					CanonicalSpelling(ParameterTypeSpelling(template_parameter)) == CanonicalSpelling(pattern)) {
					return false;
				}
			}
			for(map<string, string>::const_iterator substitution = substitutions.begin();
				substitution != substitutions.end(); ++substitution)
				if(pattern == substitution->first || pattern.find(substitution->first) != string::npos)
					return false;
			string actual;
			if(!InferArgument(arguments->children[argument], &actual, substitutions, context)) return false;
			const string expected = NormalizeTypeArgument(RewriteText(pattern, context, substitutions, 0));
			if(CanonicalSpelling(actual) != expected) return false;
			++argument;
		}
		return argument == arguments->children.size();
	}

	bool SplitTopLevelComma(const string& raw, string* tail, string* head = 0) const
	{
		int angle = 0, parentheses = 0, brackets = 0;
		for(size_t i = 0; i < raw.size(); ++i) {
			const char ch = raw[i];
			if(ch == '<' && IsTemplateAngleOpen(raw, i)) ++angle;
			else if(ch == '>' && angle > 0 && IsTemplateAngleClose(raw, i)) --angle;
			else if(ch == '(') ++parentheses;
			else if(ch == ')' && parentheses > 0) --parentheses;
			else if(ch == '[') ++brackets;
			else if(ch == ']' && brackets > 0) --brackets;
			else if(ch == ',' && angle == 0 && parentheses == 0 && brackets == 0) {
				if(head) *head = Trim(raw.substr(0, i));
				if(tail) *tail = Trim(raw.substr(i + 1));
				return true;
			}
		}
		return false;
	}

	bool SplitTopLevelConditional(const string& raw, string* condition,
		string* true_expression, string* false_expression) const
	{
		int angle = 0, parentheses = 0, brackets = 0, questions = 0;
		size_t question = string::npos;
		for(size_t i = 0; i < raw.size(); ++i) {
			const char ch = raw[i];
			if(ch == '<' && IsTemplateAngleOpen(raw, i)) ++angle;
			else if(ch == '>' && angle > 0 && IsTemplateAngleClose(raw, i)) --angle;
			else if(ch == '(') ++parentheses;
			else if(ch == ')' && parentheses > 0) --parentheses;
			else if(ch == '[') ++brackets;
			else if(ch == ']' && brackets > 0) --brackets;
			if(angle != 0 || parentheses != 0 || brackets != 0) continue;
			if(ch == '?' && question == string::npos) {
				question = i;
				questions = 1;
				continue;
			}
			if(question == string::npos) continue;
			if(ch == '?') {
				++questions;
				continue;
			}
			if(ch != ':' || (i + 1 < raw.size() && raw[i + 1] == ':') ||
				(i > 0 && raw[i - 1] == ':')) continue;
			if(--questions != 0) continue;
			if(condition) *condition = Trim(raw.substr(0, question));
			if(true_expression) *true_expression = Trim(raw.substr(question + 1,
				i - question - 1));
			if(false_expression) *false_expression = Trim(raw.substr(i + 1));
			return true;
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

	bool SplitStaticCast(string expression, string* type, string* operand) const
	{
		expression = CanonicalSpelling(expression);
		if(expression.compare(0, 12, "static_cast<") != 0 || !type || !operand)
			return false;
		size_t close = string::npos;
		int depth = 0;
		for(size_t position = 11; position < expression.size(); ++position) {
			if(expression[position] == '<') ++depth;
			else if(expression[position] == '>' && --depth == 0) {
				close = position;
				break;
			}
		}
		if(close == string::npos ||
			close + 2 > expression.size() || expression[close + 1] != '(' ||
			expression[expression.size() - 1] != ')') {
			return false;
		}
		*type = expression.substr(12, close - 12);
		*operand = expression.substr(close + 2, expression.size() - close - 3);
		return true;
	}

	bool InferFunctionTypeArguments(const TemplateDefinition& definition,
		const vector<string>& actual_types, vector<string>* result,
		const map<string, string>& substitutions, const string& context,
		const vector<string>* explicit_prefix = 0);

	string FunctionResultType(const TemplateDefinition& definition,
		const vector<string>& arguments, const string& context,
		const map<string, string>* outer_substitutions = 0)
	{
		if(!definition.declaration || definition.declaration->children.empty()) return string();
		ostringstream result_key_stream;
		result_key_stream << definition.qualified_name << "@" << definition.declaration.get()
			<< "|" << context;
		for(size_t argument = 0; argument < arguments.size(); ++argument)
			result_key_stream << "|" << CanonicalSpelling(arguments[argument]);
		const string result_key = result_key_stream.str();
		if(!active_function_results_.insert(result_key).second) return string();
		ActiveFunctionResultScope result_scope(this, result_key);
		map<string, string> local = outer_substitutions ? *outer_substitutions :
			map<string, string>();
		const map<string, vector<string> > previous_packs = active_pack_substitutions_;
		size_t argument_index = 0;
		for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter) {
			const TemplateParameter& detail = definition.parameters[parameter];
			if(detail.pack) {
				size_t trailing_fixed = 0;
				for(size_t later = parameter + 1; later < definition.parameters.size(); ++later)
					if(!definition.parameters[later].pack) ++trailing_fixed;
				const size_t available = arguments.size() > argument_index ?
					arguments.size() - argument_index : 0;
				const size_t count = available > trailing_fixed ? available - trailing_fixed : 0;
				vector<string> values;
				for(size_t value = 0; value < count; ++value)
					values.push_back(arguments[argument_index + value]);
				if(!detail.name.empty()) {
					active_pack_substitutions_[detail.name] = values;
					if(!values.empty()) local[detail.name] = values[0];
					else local.erase(detail.name);
				}
				argument_index += count;
			} else {
				if(argument_index < arguments.size() && !detail.name.empty())
					local[detail.name] = arguments[argument_index];
				if(argument_index < arguments.size()) ++argument_index;
			}
		}
		for(map<string, string>::iterator binding = local.begin(); binding != local.end();
			++binding) {
			string value = CanonicalSpelling(binding->second);
			set<string> seen;
			while(!value.empty() && seen.insert(value).second) {
				map<string, string>::const_iterator next = local.find(value);
				if(next == local.end() || next->first == binding->first) break;
				value = CanonicalSpelling(next->second);
			}
			binding->second = value;
		}
		const CPPGMAstNodePtr declarator = FunctionDeclarator(definition.declaration);
		string result;
		const CPPGMAstNodePtr trailing_return = ChildOfKindLocal(declarator,
			"trailing-return-type");
		if(trailing_return) {
			const CPPGMAstNodePtr type_id = ChildOfKindLocal(trailing_return, "type-id");
			result = TypeIdSpelling(type_id);
		} else {
			result = NodeTypeSpelling(definition.declaration->children[0]);
			result += DeclaratorSuffix(declarator);
		}
		result = RewriteText(result, context, local, 0);
		result = CollapseReferenceSpelling(ReplaceIdentifiers(result, local));
		result = ResolveDecltypeTypeName(result, context, local);
		active_pack_substitutions_ = previous_packs;
		return NormalizeTypeArgument(result);
	}
	string FunctionLookupContext(const string& context) const;
	const TemplateDefinition* FindExplicitFunctionTemplate(const string& base,
		const string& context) const;
	bool ResolveCallableTemporaryCallResult(const string& callee, const string& function_context, const string& context,
		const map<string, string>& substitutions, const vector<string>& actual_types, string* result);
	bool ResolveCallableVariableCallResult(const string& callee, const string& function_context, const string& context, const map<string, string>& substitutions, const vector<string>& actual_types, string* result);
	bool ResolveConstructedCallResult(const string& callee, const string& context,
		const map<string, string>& substitutions, const vector<string>& actual_types, string* result);
	bool FunctionCallResultType(string expression, const string& context,
		const map<string, string>& substitutions, string* result)
	{
		if(!result) return false;
		string callee, argument_text;
		if(!SplitTextCall(expression, &callee, &argument_text)) return false;
		callee = StripTextParentheses(callee);
		if(callee.empty()) return false;
		const string function_context = FunctionLookupContext(context);
		if(callee[callee.size() - 1] == ')') {
			string returned;
			if(FunctionCallResultType(callee, function_context, substitutions, &returned)) {
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
		string explicit_base_name;
		const size_t template_open = callee.find('<');
		const bool callee_is_static_cast = callee.compare(0, 12, "static_cast<") == 0;
		if(template_open != string::npos && !callee_is_static_cast) {
			string base_arguments, base;
			size_t template_close = string::npos, begin = 0;
				if(!TemplateBase(callee, template_open, &begin, &base) ||
					!TemplateRange(callee, template_open, &base_arguments, &template_close)) return false;
				explicit_base_name = base;
				explicit_definition = FindExplicitFunctionTemplate(base, function_context);
				if(!explicit_definition || explicit_definition->class_template) return false;
			explicit_arguments = SplitTemplateArguments(base_arguments);
			for(size_t i = 0; i < explicit_arguments.size(); ++i) {
				explicit_arguments[i] = RewriteText(explicit_arguments[i], function_context,
					substitutions, 0);
				explicit_arguments[i] = NormalizeTypeArgument(ReplaceIdentifiers(
					explicit_arguments[i], substitutions));
				explicit_arguments[i] = ResolveAlias(explicit_arguments[i], function_context);
			}
		}
		vector<string> actual_types;
		const vector<string> actual_expressions = SplitTemplateArguments(argument_text);
		for(size_t i = 0; i < actual_expressions.size(); ++i) {
			if(actual_expressions[i].empty()) continue;
			string actual_expression = actual_expressions[i];
			const bool pack_expansion = actual_expression.size() >= 3 &&
				actual_expression.compare(actual_expression.size() - 3, 3, "...") == 0;
			if(pack_expansion) actual_expression.erase(actual_expression.size() - 3);
			if(pack_expansion) {
				string pack_name;
				const vector<string>* pack_values = 0;
				for(map<string, vector<string> >::const_iterator pack =
					active_pack_substitutions_.begin(); pack != active_pack_substitutions_.end(); ++pack) {
					if(pack->first.empty()) continue;
					for(size_t at = actual_expression.find(pack->first); at != string::npos;
						at = actual_expression.find(pack->first, at + pack->first.size())) {
						const bool left = at == 0 || !IsIdentifierCharacter(actual_expression[at - 1]);
						const size_t end = at + pack->first.size();
						const bool right = end == actual_expression.size() ||
							!IsIdentifierCharacter(actual_expression[end]);
						if(left && right) {
							pack_name = pack->first;
							pack_values = &pack->second;
							break;
						}
					}
					if(pack_values) break;
				}
				if(!pack_values) for(map<string, vector<string> >::const_iterator pack =
					active_function_pack_substitutions_.begin();
					pack != active_function_pack_substitutions_.end(); ++pack) {
					const size_t at = actual_expression.find(pack->first);
					if(at == string::npos || (at != 0 &&
						IsIdentifierCharacter(actual_expression[at - 1])) ||
						(at + pack->first.size() < actual_expression.size() &&
						 IsIdentifierCharacter(actual_expression[at + pack->first.size()]))) continue;
					pack_name = pack->first;
					pack_values = &pack->second;
					break;
				}
				if(pack_values) {
					for(size_t element = 0; element < pack_values->size(); ++element) {
						map<string, string> one = substitutions;
						one[pack_name] = (*pack_values)[element];
						const string actual = ExpressionTypeSpelling(actual_expression,
							function_context, one);
						if(actual.empty()) return false;
						actual_types.push_back(actual);
					}
					continue;
				}
			}
			const string actual = ExpressionTypeSpelling(actual_expression, function_context,
				substitutions);
			if(actual.empty()) return false;
			actual_types.push_back(actual);
		}
		if(callee[callee.size() - 1] == ')' && ResolveCallableTemporaryCallResult(callee, function_context, context, substitutions, actual_types, result)) return true;
		string callable_type, callable_operand;
		if(SplitStaticCast(callee, &callable_type, &callable_operand)) {
			string function_result;
			vector<string> function_parameters;
			if(SplitFunctionPointerType(ReplaceIdentifiers(callable_type, substitutions),
				&function_result, &function_parameters)) {
				if(function_parameters.size() != actual_types.size()) return false;
				for(size_t argument = 0; argument < actual_types.size(); ++argument) {
					const string parameter = RewriteText(function_parameters[argument],
						function_context, substitutions, 0);
					if(!FunctionArgumentViable(parameter, actual_types[argument],
						function_context)) return false;
				}
				*result = NormalizeTypeArgument(ResolveAlias(ReplaceIdentifiers(
					function_result, substitutions), function_context));
				return !result->empty();
			}
			string object_type = NormalizeTypeArgument(ResolveAlias(
				ReplaceIdentifiers(callable_type, substitutions), function_context));
			while(!object_type.empty() && (object_type[object_type.size() - 1] == '&' ||
				object_type[object_type.size() - 1] == '*')) object_type.erase(object_type.size() - 1);
			object_type = NormalizeTypeArgument(object_type);
			const vector<const TemplateDefinition*> call_operators =
				FindFunctionDefinitions("operator()", object_type);
			for(size_t candidate = 0; candidate < call_operators.size(); ++candidate) {
				vector<string> arguments;
				if(!InferFunctionTypeArguments(*call_operators[candidate], actual_types,
					&arguments, substitutions, function_context)) continue;
				*result = FunctionResultType(*call_operators[candidate], arguments,
					function_context, &substitutions);
				if(!result->empty()) return true;
			}
		}
		vector<const TemplateDefinition*> candidates;
		if(explicit_definition) {
			candidates = FindFunctionDefinitions(explicit_base_name, function_context);
			if(candidates.empty()) candidates.push_back(explicit_definition);
		}
		else candidates = FindFunctionDefinitions(callee, function_context);
		string selected_result;
		bool selected_ellipsis = true;
		for(size_t i = 0; i < candidates.size(); ++i) {
			const TemplateDefinition& definition = *candidates[i];
			vector<string> arguments;
			const bool complete = explicit_definition &&
				explicit_arguments.size() == definition.parameters.size();
			if(complete) arguments = explicit_arguments;
			else if(!InferFunctionTypeArguments(definition, actual_types, &arguments,
				substitutions, function_context, explicit_definition ? &explicit_arguments : 0)) {
				continue;
			}
			try {
				if(!FunctionArgumentsViable(definition, arguments, actual_types,
					function_context)) continue;
			} catch(const PA18SubstitutionFailure&) {
				continue;
			}
			const string candidate_result = FunctionResultType(definition, arguments,
				function_context, &substitutions);
			if(candidate_result.empty()) continue;
			bool candidate_ellipsis = false;
			const CPPGMAstNodePtr candidate_clause = DescendantOfKind(
				FunctionDeclarator(definition.declaration), "parameter-clause");
			if(candidate_clause) for(size_t parameter = 0;
				parameter < candidate_clause->children.size(); ++parameter)
				if(candidate_clause->children[parameter] &&
					candidate_clause->children[parameter]->kind == "ellipsis") {
					candidate_ellipsis = true;
					break;
				}
			if(selected_result.empty() || (selected_ellipsis && !candidate_ellipsis)) {
				selected_result = candidate_result;
				selected_ellipsis = candidate_ellipsis;
			}
		}
		if(!selected_result.empty()) {
			*result = selected_result;
			return true;
		}
		if(ResolveConstructedCallResult(callee, context, substitutions, actual_types, result)) return true;
		if(!explicit_definition) {
			for(map<string, vector<FunctionSignature> >::const_iterator overload =
				function_overloads_.begin(); overload != function_overloads_.end(); ++overload) {
				const string suffix = "::" + callee;
				if(overload->first != callee &&
					(overload->first.size() <= suffix.size() ||
						overload->first.compare(overload->first.size() - suffix.size(),
							suffix.size(), suffix) != 0)) continue;
				for(size_t candidate = 0; candidate < overload->second.size(); ++candidate) {
					const FunctionSignature& signature = overload->second[candidate];
					const CPPGMAstNodePtr parameters = signature.parameters;
					bool ellipsis = false;
					if(parameters) for(size_t parameter = 0; parameter < parameters->children.size();
						++parameter) {
						const CPPGMAstNodePtr item = parameters->children[parameter];
						if(item && item->kind == "ellipsis") {
							ellipsis = true;
							break;
						}
					}
					if(ellipsis && signature.result_specifiers)
						return (*result = NodeTypeSpelling(signature.result_specifiers) +
							ReturnDeclaratorSuffix(signature.declarator), true);
				}
			}
			const FunctionSignature* signature = FindFunctionSignature(callee, context);
			if(signature && signature->result_specifiers) {
				*result = NodeTypeSpelling(signature->result_specifiers) +
					ReturnDeclaratorSuffix(signature->declarator);
				return !result->empty();
			}
			if(ResolveCallableVariableCallResult(callee, function_context, context,
				substitutions, actual_types, result)) return true;
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
			if(!definition->parameters[i].name.empty())
				local[definition->parameters[i].name] = arguments[i];
		string result = FunctionResultType(*definition, arguments, context,
			&substitutions) + "(*) (";
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
				string free_operator_result;
				if(InferOperatorResult("<<", left, right, context, &free_operator_result))
					return free_operator_result;
				CPPGMAstNodePtr declaration = FindClassDeclaration(left, context);
				if(!declaration) return IsKnownTypeSpelling(left, context) ? left : string();
			for(size_t child = 0; child < declaration->children.size(); ++child) {
				const CPPGMAstNodePtr member = declaration->children[child];
				if(!member || member->kind != "function-definition" || member->children.size() < 2 ||
					LastComponent(FirstIdentifierLocal(member->children[1])) != "operator<<") continue;
				return RewriteText(NodeTypeSpelling(member->children[0]) +
					DeclaratorSuffix(member->children[1]), context, substitutions, 0);
			}
			return IsKnownTypeSpelling(left, context) ? left : string();
		}
		// Preserve the historical member-shift path above, then handle the
		// other top-level binary operators with the typed operator and builtin
		// rules.  Keeping the shift path separate avoids treating the first `<`
		// of `<<` as a template delimiter in the compact PA10 spelling.
		angle = parentheses = brackets = 0;
		for(size_t i = 0; i < expression.size(); ++i) {
			const char ch = expression[i];
			if(ch == '(') ++parentheses;
			else if(ch == ')' && parentheses > 0) --parentheses;
			else if(ch == '[') ++brackets;
			else if(ch == ']' && brackets > 0) --brackets;
			else if(ch == '<' && IsTemplateAngleOpen(expression, i)) ++angle;
			else if(ch == '>' && angle > 0 && IsTemplateAngleClose(expression, i)) --angle;
			if(angle != 0 || parentheses != 0 || brackets != 0) continue;
			static const char* const operators[] = {"||", "&&", "==", "!=", "<=", ">=",
				">>", "<", ">", "+", "-", "*", "/", "%", "&"};
			string operation;
			for(size_t candidate = 0; candidate < sizeof(operators) / sizeof(*operators); ++candidate)
				if(expression.compare(i, string(operators[candidate]).size(), operators[candidate]) == 0) {
					operation = operators[candidate]; break;
				}
			if(operation.empty() || ((operation == "+" || operation == "-" || operation == "*" ||
				operation == "&") && (i == 0 || string("([{,=!?+-*/%<>&|").find(expression[i - 1]) != string::npos)))
				continue;
			const string left = ExpressionTypeSpelling(expression.substr(0, i), context, substitutions);
			const string right = ExpressionTypeSpelling(expression.substr(i + operation.size()), context, substitutions);
			if(left.empty() || right.empty()) return string();
			string result;
			if((operation == "&&" || operation == "||" || operation == "==" || operation == "!=" ||
				operation == "<" || operation == ">" || operation == "<=" || operation == ">=") &&
				IsBuiltinLogicalType(left) && IsBuiltinLogicalType(right)) return "bool";
			if(InferOperatorResult(operation, left, right, context, &result)) return result;
			if(IsBuiltinArithmeticType(left) && IsBuiltinArithmeticType(right))
				return CommonBuiltinArithmeticType(left, right);
			return IsKnownTypeSpelling(left, context) ? left : string();
		}
		return string();
	}
	bool IsDefaultConstructibleType(string raw, const string& context) const
	{
		raw = NormalizeTypeArgument(ResolveAlias(ReplaceIdentifiers(raw, map<string, string>()),
			context));
		while(raw.compare(0, 6, "const ") == 0)
			raw = NormalizeTypeArgument(raw.substr(6));
		while(raw.compare(0, 9, "volatile ") == 0)
			raw = NormalizeTypeArgument(raw.substr(9));
		if(raw == "void" || raw == "nullptr_t" || IsBuiltinArithmeticType(raw)) return true;
		if(raw.find('*') != string::npos || raw.find('&') != string::npos) return true;
		const CPPGMAstNodePtr declaration = FindClassDeclaration(raw, context);
		if(!declaration) return true;
		const string class_name = LastComponent(raw);
		bool declared_constructor = false;
		for(size_t child = 0; child < declaration->children.size(); ++child) {
			const CPPGMAstNodePtr member = declaration->children[child];
			if(!member || (member->kind != "special-member-declaration" &&
				member->kind != "special-member-definition") ||
				LastComponent(RemoveMarker(member->value)) != class_name) continue;
			declared_constructor = true;
			const CPPGMAstNodePtr deleted = ChildOfKindLocal(member, "special-initializer");
			if(deleted && RemoveMarker(deleted->value) == "delete") continue;
			const CPPGMAstNodePtr clause = DescendantOfKind(
				FunctionDeclarator(member), "parameter-clause");
			if(!clause) continue;
			bool viable = true;
			for(size_t parameter = 0; parameter < clause->children.size(); ++parameter) {
				const CPPGMAstNodePtr node = clause->children[parameter];
				if(!node || node->kind != "parameter-declaration") continue;
				if(!ChildOfKindLocal(node, "default-argument")) {
					viable = false;
					break;
				}
			}
			if(viable) return true;
		}
		return !declared_constructor;
	}
	string ResolveDecltypeTypeName(string raw, const string& context,
		const map<string, string>& substitutions) const
	{
		raw = NormalizeTypeArgument(ResolveAlias(ReplaceIdentifiers(raw, substitutions),
			context));
		if(IsKnownTypeSpelling(raw, context)) return raw;
		// A type-only operand in decltype can name an alias declared in the
		// current class.  IsKnownTypeSpelling intentionally describes global and
		// qualified type names, so consult the typed class-member index for the
		// unqualified spelling before treating it as an unknown dependent name.
		for(string current = context; ; ) {
			if(!current.empty()) {
				string member_type;
				set<string> active;
				const bool found = FindClassMemberType(current, raw, substitutions, context,
					&member_type, &active, true);
				if(found && !member_type.empty())
					return NormalizeTypeArgument(ResolveAlias(ReplaceIdentifiers(
						member_type, substitutions), context));
			}
			if(current.empty()) break;
			const size_t separator = current.rfind("::");
			if(separator == string::npos) current.clear();
			else current.erase(separator);
		}
		return raw;
	}
	string FunctionArgumentObjectType(string raw, const string& context) const
	{
		// A dependent call result can retain the source template-id spelling while
		// a nondependent overload parameter has already been reduced through its
		// typedef alias.  Normalize concrete template-ids through the same typed
		// specialization table before comparing the object types.
		if(raw.find('<') != string::npos) try {
			raw = const_cast<PA18TemplateExpander*>(this)->RewriteText(raw, context,
				map<string, string>(), 0);
		} catch(const PA18SubstitutionFailure&) {
			// The caller will reject an actually unavailable operand below.
		}
		raw = CanonicalSpelling(ResolveAlias(raw, context));
		while(raw.compare(0, 6, "const ") == 0)
			raw = CanonicalSpelling(raw.substr(6));
		while(raw.compare(0, 9, "volatile ") == 0)
			raw = CanonicalSpelling(raw.substr(9));
		while(raw.size() > 6 && raw.compare(raw.size() - 6, 6, " const") == 0)
			raw = CanonicalSpelling(raw.substr(0, raw.size() - 6));
		while(raw.size() > 9 && raw.compare(raw.size() - 9, 9, " volatile") == 0)
			raw = CanonicalSpelling(raw.substr(0, raw.size() - 9));
		while(raw.size() >= 2 && raw.compare(raw.size() - 2, 2, "&&") == 0)
			raw = CanonicalSpelling(raw.substr(0, raw.size() - 2));
		while(!raw.empty() && raw[raw.size() - 1] == '&')
			raw = CanonicalSpelling(raw.substr(0, raw.size() - 1));
		return raw;
	}
	bool FunctionArgumentViable(const string& parameter, const string& actual,
		const string& context) const
	{
		const string expected = FunctionArgumentObjectType(parameter, context);
		const string received = FunctionArgumentObjectType(actual, context);
		if(expected.empty() || received.empty()) return false;
		if(expected == received) return true;
		if(IsBuiltinArithmeticType(expected) && IsBuiltinArithmeticType(received))
			return true;
		if(IsBuiltinArithmeticType(expected) && FindClassDeclaration(received, context))
			return false;
		// Expression-SFINAE needs to reject an attempted conversion between two
		// unrelated complete class types.  The ordinary call rewriter handles
		// constructors and conversion operators; this narrow viability check is
		// the typed fact needed for probes such as test_aux<To>(declval<From>()).
		const bool direct_parameter = expected.find('*') == string::npos &&
			expected.find('&') == string::npos;
		const bool direct_actual = received.find('*') == string::npos &&
			received.find('&') == string::npos;
		if(direct_parameter && direct_actual &&
			FindClassDeclaration(expected, context) &&
			FindClassDeclaration(received, context)) return false;
		return true;
	}
	bool FunctionArgumentsViable(const TemplateDefinition& definition,
		const vector<string>& arguments, const vector<string>& actual_types,
		const string& context)
	{
		if(!definition.declaration) return false;
		const CPPGMAstNodePtr declarator = FunctionDeclarator(definition.declaration);
		const CPPGMAstNodePtr parameters = DescendantOfKind(declarator, "parameter-clause");
		if(!parameters) return false;
		map<string, string> local;
		map<string, vector<string> > pack_arguments;
		size_t template_argument = 0;
		for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter) {
			const TemplateParameter& detail = definition.parameters[parameter];
			if(detail.pack) {
				size_t trailing_fixed = 0;
				for(size_t later = parameter + 1; later < definition.parameters.size(); ++later)
					if(!definition.parameters[later].pack) ++trailing_fixed;
				const size_t available = arguments.size() > template_argument ?
					arguments.size() - template_argument : 0;
				const size_t count = available > trailing_fixed ? available - trailing_fixed : 0;
				for(size_t value = 0; value < count; ++value)
					pack_arguments[detail.name].push_back(arguments[template_argument + value]);
				template_argument += count;
			} else {
				if(template_argument < arguments.size() && !detail.name.empty())
					local[detail.name] = arguments[template_argument];
				if(template_argument < arguments.size()) ++template_argument;
			}
		}
		size_t actual = 0;
		bool has_ellipsis = false;
		for(size_t parameter = 0; parameter < parameters->children.size(); ++parameter) {
			const CPPGMAstNodePtr node = parameters->children[parameter];
			if(!node) continue;
			if(node->kind == "ellipsis") {
				has_ellipsis = true;
				break;
			}
			if(node->kind != "parameter-declaration") continue;
			if(IsFunctionParameterPack(node)) {
				size_t trailing_fixed = 0;
				for(size_t later = parameter + 1; later < parameters->children.size(); ++later)
					if(parameters->children[later] &&
						parameters->children[later]->kind == "parameter-declaration" &&
						!IsFunctionParameterPack(parameters->children[later])) ++trailing_fixed;
				const size_t available = actual_types.size() > actual ?
					actual_types.size() - actual : 0;
				const size_t visits = available > trailing_fixed ? available - trailing_fixed : 0;
				const string pattern = ParameterTypeSpelling(node);
				string pack_name;
				for(size_t candidate = 0; candidate < definition.parameters.size(); ++candidate) {
					const TemplateParameter& detail = definition.parameters[candidate];
					if(!detail.pack || detail.name.empty()) continue;
					const size_t at = pattern.find(detail.name);
					if(at != string::npos &&
						(at == 0 || !IsIdentifierCharacter(pattern[at - 1])) &&
						(at + detail.name.size() == pattern.size() ||
							!IsIdentifierCharacter(pattern[at + detail.name.size()]))) {
						pack_name = detail.name;
						break;
					}
				}
				for(size_t visit = 0; visit < visits; ++visit) {
					map<string, string> one = local;
					if(!pack_name.empty()) {
						const vector<string>& values = pack_arguments[pack_name];
						if(visit >= values.size()) return false;
						one[pack_name] = values[visit];
					}
					string expected = RewriteText(pattern, context, one, 0);
					expected = NormalizeTypeArgument(ReplaceIdentifiers(expected, one));
					if(!FunctionArgumentViable(expected, actual_types[actual + visit], context))
						return false;
				}
				actual += visits;
				continue;
			}
			if(actual >= actual_types.size()) {
				if(ChildOfKindLocal(node, "default-argument")) continue;
				return false;
			}
			string expected = RewriteText(ParameterTypeSpelling(node), context, local, 0);
			expected = NormalizeTypeArgument(ReplaceIdentifiers(expected, local));
			if(!FunctionArgumentViable(expected, actual_types[actual], context)) return false;
			++actual;
		}
		return has_ellipsis || actual == actual_types.size();
	}

	string ExpressionTypeSpelling(string expression, const string& context,
		const map<string, string>& substitutions)
	{
		expression = StripTextParentheses(CanonicalSpelling(expression));
		string cast_type, cast_operand;
		string call_callee, call_arguments;
		if(SplitTextCall(expression, &call_callee, &call_arguments) &&
			SplitStaticCast(call_callee, &cast_type, &cast_operand)) {
			string function_result;
			vector<string> function_parameters;
			if(SplitFunctionPointerType(ReplaceIdentifiers(cast_type, substitutions),
				&function_result, &function_parameters)) {
				const vector<string> actual_expressions = SplitTemplateArguments(call_arguments);
				if(actual_expressions.size() != function_parameters.size()) return string();
				for(size_t argument = 0; argument < actual_expressions.size(); ++argument) {
					const string actual = ExpressionTypeSpelling(actual_expressions[argument],
						context, substitutions);
					const string parameter = RewriteText(function_parameters[argument], context,
						substitutions, 0);
					if(actual.empty() || !FunctionArgumentViable(parameter, actual, context))
						return string();
				}
				return NormalizeTypeArgument(ResolveAlias(ReplaceIdentifiers(
					function_result, substitutions), context));
			}
		}
		if(SplitStaticCast(expression, &cast_type, &cast_operand))
			return NormalizeTypeArgument(ResolveAlias(ReplaceIdentifiers(
				cast_type, substitutions), context));
		string condition, true_expression, false_expression;
		if(SplitTopLevelConditional(expression, &condition, &true_expression,
			&false_expression)) {
			if(condition.empty() || true_expression.empty() || false_expression.empty() ||
				ExpressionTypeSpelling(condition, context, substitutions).empty()) return string();
			const string true_type = ExpressionTypeSpelling(true_expression, context,
				substitutions);
			const string false_type = ExpressionTypeSpelling(false_expression, context,
				substitutions);
			if(true_type.empty() || false_type.empty()) return string();
			const string normalized_true = NormalizeTypeArgument(true_type);
			const string normalized_false = NormalizeTypeArgument(false_type);
			if(normalized_true == normalized_false) return normalized_true;
			if(IsBuiltinArithmeticType(normalized_true) &&
				IsBuiltinArithmeticType(normalized_false))
				return CommonBuiltinArithmeticType(normalized_true, normalized_false);
			return string();
		}
		string comma_head, comma_tail;
		if(SplitTopLevelComma(expression, &comma_tail, &comma_head)) {
			// The left operand of a comma expression is still an unevaluated
			// expression, but it must be well-formed.  Discarding it made every
			// expression-SFINAE probe look viable, including `missing.member` and
			// failed overload calls.
			if(comma_head.empty() || ExpressionTypeSpelling(comma_head, context,
				substitutions).empty()) return string();
			return ExpressionTypeSpelling(comma_tail, context, substitutions);
		}
		// Member access in a decltype operand is an expression type, not a
		// qualified type spelling.  Infer the object first so a dependent or
		// C-style-cast object can use the normal typed member lookup path.
		int angle = 0, parentheses = 0, brackets = 0;
		for(size_t i = 0; i < expression.size(); ++i) {
			const char ch = expression[i];
			if(ch == '(') ++parentheses;
			else if(ch == ')' && parentheses > 0) --parentheses;
			else if(ch == '[') ++brackets;
			else if(ch == ']' && brackets > 0) --brackets;
			else if(ch == '<' && IsTemplateAngleOpen(expression, i)) ++angle;
			else if(ch == '>' && angle > 0 && IsTemplateAngleClose(expression, i)) --angle;
			if(angle != 0 || parentheses != 0 || brackets != 0) continue;
			string operator_text;
			if(expression.compare(i, 2, "->") == 0) operator_text = "->";
			else if(expression[i] == '.') operator_text = ".";
			if(operator_text.empty()) continue;
			const string left = ExpressionTypeSpelling(expression.substr(0, i),
				context, substitutions);
			const string member = Trim(expression.substr(i + operator_text.size()));
			if(left.empty() || member.empty()) continue;
			set<string> active;
			string member_type;
			string member_name = LastComponent(member);
			const size_t member_call = member_name.find('(');
			if(member_call != string::npos) member_name.erase(member_call);
			const size_t member_template = member_name.find('<');
			if(member_template != string::npos) member_name.erase(member_template);
			if(!member_name.empty() && member_name[0] == '~' && IsKnownTypeSpelling(left, context)) return "void";
			if(FindClassMemberType(left, member_name, substitutions, context,
				&member_type, &active)) {
				return member_type;
			}
		}
		if(expression.compare(0, 9, "decltype(") == 0 && expression.size() > 10 &&
			expression[expression.size() - 1] == ')') {
			string nested;
			if(EvaluateDecltypeExpression(expression.substr(9, expression.size() - 10),
				context, substitutions, &nested)) return nested;
		}
		// Classify template-id calls before binary parsing; `<` belongs to the callee.
		string call_type; if(FunctionCallResultType(expression, context, substitutions, &call_type)) return call_type;
		const string binary_type = BinaryExpressionType(expression, context, substitutions);
		if(!binary_type.empty()) {
			return binary_type;
		}
		const string function_type = FunctionTemplateIdType(expression, context, substitutions);
		if(!function_type.empty()) return function_type;
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
		// The PA10 expression tree is intentionally compact and retains a C-style
		// cast as text.  Recover `(T)operand` before falling back to identifier
		// and call lookup; this is needed for `decltype(((A*)0)->member)`.
		if(!expression.empty() && expression[0] == '(') {
			int depth = 0;
			for(size_t close = 0; close < expression.size(); ++close) {
				if(expression[close] == '(') ++depth;
				else if(expression[close] == ')' && --depth == 0 && close + 1 < expression.size()) {
					const string cast = Trim(expression.substr(1, close - 1));
					const string resolved = NormalizeTypeArgument(ResolveAlias(
						ReplaceIdentifiers(cast, substitutions), context));
					if(!resolved.empty() && (FindClassDeclaration(resolved, context) ||
						IsBuiltinArithmeticType(resolved) || resolved.find('*') != string::npos ||
						resolved.find('&') != string::npos)) return resolved;
					break;
				}
			}
		}
		const size_t open = expression.find('(');
		if(open != string::npos && expression[expression.size() - 1] == ')') {
			string cast_type = ResolveDecltypeTypeName(Trim(expression.substr(0, open)),
				context, substitutions);
			if(IsKnownTypeSpelling(cast_type, context))
				return NormalizeTypeArgument(cast_type);
		}
		map<string, string>::const_iterator substituted = substitutions.find(expression);
		if(substituted != substitutions.end()) return substituted->second;
		// A dependent decltype inside a class can name a static data member
		// declared earlier in the same class (`decltype(_v)`).  It is not a
		// local variable, so the ordinary variable table deliberately does not
		// contain it; query the typed class-member view before falling back to
		// free-variable lookup.
		for(string current = context; !current.empty(); ) {
			if(FindClassDeclaration(current, current)) {
				string member_type;
				set<string> active_members;
				if(FindClassMemberType(current, expression, substitutions, context,
					&member_type, &active_members, false) && !member_type.empty())
					return member_type;
			}
			const size_t separator = current.rfind("::");
			if(separator == string::npos) break;
			current.erase(separator);
		}
		string variable_type;
		if(LookupVariableType(expression, context, &variable_type))
			return ReplaceIdentifiers(ResolveAlias(variable_type, context), substitutions);
		if(expression == "true" || expression == "false") return "bool";
		if(expression == "nullptr") return "nullptr_t";
		if(!expression.empty() && (isdigit(static_cast<unsigned char>(expression[0])) ||
			expression[0] == '\'' || expression[0] == '"'))
			return expression[0] == '\'' ? "char" : "int";
		return string();
	}
	bool EvaluateNewExpression(const string& expression, const string& context,
		const map<string, string>& substitutions, string* result);
	bool EvaluateDecltypeExpression(const string& expression, const string& context,
		const map<string, string>& substitutions, string* result)
	{
		if(!result) return false;
		string head, tail;
		if(SplitTopLevelComma(expression, &tail, &head)) {
			if(head.empty() || ExpressionTypeSpelling(head, context, substitutions).empty())
				return false;
			return EvaluateDecltypeExpression(tail, context, substitutions, result);
		}
		string normalized = StripTextParentheses(expression);
		if(normalized.compare(0, 6, "delete") == 0 && normalized.size() > 6 &&
			normalized[6] != ' ')
			normalized.insert(6, " ");
		if(normalized != expression)
			return EvaluateDecltypeExpression(normalized, context, substitutions, result);
		if(normalized.compare(0, 7, "sizeof(") == 0 ||
			normalized.compare(0, 7, "alignof(") == 0) {
			*result = "unsigned long";
			return true;
		}
		if(normalized.compare(0, 7, "delete ") == 0 || normalized == "delete") {
			*result = "void";
			return true;
		}
		if(EvaluateNewExpression(normalized, context, substitutions, result)) return true;
		const size_t direct_open = normalized.find('(');
		if(direct_open != string::npos && direct_open > 0 &&
			normalized.find('.') == string::npos && normalized.find("->") == string::npos &&
			normalized[normalized.size() - 1] == ')') {
			const string constructed = ResolveDecltypeTypeName(
				Trim(normalized.substr(0, direct_open)), context, substitutions);
			if(IsKnownTypeSpelling(constructed, context)) {
				if(!IsDefaultConstructibleType(constructed, context)) return false;
				*result = constructed;
				return true;
			}
		}
		if(FunctionCallResultType(normalized, context, substitutions, result)) return true;
		const size_t open = normalized.find('(');
		if(open != string::npos && open > 0 &&
			normalized.find('.') == string::npos && normalized.find("->") == string::npos &&
			normalized[normalized.size() - 1] == ')') {
			*result = ResolveDecltypeTypeName(Trim(normalized.substr(0, open)),
				context, substitutions);
			if(!IsKnownTypeSpelling(*result, context)) return false;
			if(!IsDefaultConstructibleType(*result, context)) return false;
			return true;
		}
		*result = ExpressionTypeSpelling(normalized, context, substitutions);
		return !result->empty();
	}
	string TemplateMemberType(const TemplateDefinition& definition,
		const vector<string>& arguments, const string& member, const string& context);
	string FinishTemplateMemberType(const string& active_key,
		const map<string, vector<string> >& previous_packs, const string& value);
	void PrepareTemplateMemberSubstitutions(const TemplateDefinition& definition,
		const vector<string>& arguments, const string& context,
		map<string, string>* local);
	string RewriteTemplateMemberSpelling(const TemplateDefinition& definition,
		const vector<string>& arguments, string spelling, const string& context,
		const map<string, string>& local);
	bool FindDirectTemplateMemberType(const TemplateDefinition& definition,
		const vector<string>& arguments, const string& member, const string& context,
		map<string, string>* local, string* result);
	bool FindInheritedTemplateMemberType(const TemplateDefinition& definition,
		const string& member, const string& context,
		const map<string, string>& local, string* result);

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
		if(DescendantOfKind(type_id, "parameter-clause")) return CPPGMAstNodePtr();
		const string raw_type = SpellNode(type_id); const size_t open = raw_type.find('<');
		if(open == string::npos) return CPPGMAstNodePtr();
		string argument_text, base; size_t close = string::npos, begin = 0;
		if(!TemplateBase(raw_type, open, &begin, &base) ||
			!TemplateRange(raw_type, open, &argument_text, &close)) return CPPGMAstNodePtr();
		if(close + 1 != raw_type.size()) return CPPGMAstNodePtr();
		const TemplateDefinition* definition = FindDefinition(base, context);
		if(!definition || definition->class_template) return CPPGMAstNodePtr();
		vector<string> arguments = SplitTemplateArguments(argument_text);
		for(size_t i = 0; i < arguments.size(); ++i) {
			arguments[i] = NormalizeTypeArgument(RewriteText(arguments[i], context,
				substitutions, 0));
			arguments[i] = NormalizeTypeArgument(ReplaceIdentifiers(arguments[i], substitutions));
		}
		if(arguments.size() != definition->parameters.size()) return CPPGMAstNodePtr();
		const string local_name = Instantiate(*definition, arguments, context); string qualifier = PrefixComponent(base);
		if(qualifier.empty() && !definition->owner.empty() && definition->owner.find("<unnamed>") == string::npos && class_contexts_.find(definition->owner) == class_contexts_.end()) qualifier = definition->owner;
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
