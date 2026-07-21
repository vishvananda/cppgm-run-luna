#pragma once

	string NodeTypeSpelling(const CPPGMAstNodePtr& sequence) const
	{
		if(!sequence) return string();
		string result;
		for(size_t i = 0; i < sequence->children.size(); ++i) {
			const CPPGMAstNodePtr child = sequence->children[i];
			if(!child || (child->kind == "decl-specifier" &&
				(child->value == "KW_TYPEDEF:typedef" || child->value == "KW_STATIC:static" ||
					 child->value == "KW_INLINE:inline" ||
					 child->value == "KW_CONSTEXPR:constexpr"))) continue;
			if(child->kind != "decl-specifier" && child->kind != "type-name" &&
				child->kind != "type-specifier" && child->kind != "decltype-specifier" &&
				child->kind != "cv-qualifier") continue;
			const string spelling = RemoveMarker(child->value);
			if(spelling.empty()) continue;
			if(!result.empty()) result += ' ';
			result += spelling;
		}
		return CanonicalSpelling(result);
	}
	string IntegralValueSpelling(const PA19IntegralValue& value) const
	{
		if(!value.known) return string();
		const PA19IntegralType type = value.type;
		if(type.name == "bool") return PA19Raw(value) ? "true" : "false";
		ostringstream result;
		if(type.is_unsigned) result << PA19Raw(value);
		else result << PA19Signed(value);
		if(type.is_unsigned) {
			if(type.bits > 32) result << "ULL";
			else result << "u";
		} else if(type.bits > 32) result << "LL";
		return result.str();
	}
	string TemplateIntegralValueSpelling(const PA19IntegralValue& value) const
	{
		if(!value.known) return string();
		if(value.type.name == "bool") return PA19Raw(value) ? "true" : "false";
		ostringstream result;
		if(value.type.is_unsigned) result << PA19Raw(value);
		else result << PA19Signed(value);
		return result.str();
	}
	string ConstantExpressionSpelling(const CPPGMAstNodePtr& node) const
	{
		if(!node) return string();
		if(node->kind == "literal" || node->kind == "keyword-literal" ||
			node->kind == "id-expression" || node->kind == "template-id")
			return RemoveMarker(node->value);
		if(node->kind == "parenthesized-expression" && !node->children.empty())
			return "(" + ConstantExpressionSpelling(node->children[0]) + ")";
		if(node->kind == "unary-expression" && !node->children.empty())
			return RemoveMarker(node->value) + ConstantExpressionSpelling(node->children[0]);
		if((node->kind == "binary-expression" || node->kind == "assignment-expression") &&
			node->children.size() >= 2)
			return "(" + ConstantExpressionSpelling(node->children[0]) + " " +
				RemoveMarker(node->value) + " " +
				ConstantExpressionSpelling(node->children[1]) + ")";
		if(node->kind == "conditional-expression" && node->children.size() >= 3)
			return "(" + ConstantExpressionSpelling(node->children[0]) + " ? " +
				ConstantExpressionSpelling(node->children[1]) + " : " +
				ConstantExpressionSpelling(node->children[2]) + ")";
		if((node->kind == "sizeof-expression" || node->kind == "type-trait-expression") &&
			!node->children.empty())
			return (node->kind == "sizeof-expression" ? "sizeof(" : "alignof(") +
				SpellNode(node->children[0]) + ")";
		if(node->kind == "sizeof-pack-expression" && !node->children.empty())
			return "sizeof...(" + ConstantExpressionSpelling(node->children[0]) + ")";
		if(node->kind == "cast-expression" && node->children.size() >= 2)
			return "static_cast<" + SpellNode(node->children[0]) + ">("
				+ ConstantExpressionSpelling(node->children[1]) + ")";
		if(node->kind == "subscript-expression" && node->children.size() >= 2)
			return ConstantExpressionSpelling(node->children[0]) + "[" +
				ConstantExpressionSpelling(node->children[1]) + "]";
		if(node->kind == "call-expression" && !node->children.empty()) {
			string result = ConstantExpressionSpelling(node->children[0]) + "(";
			if(node->children.size() > 1 && node->children[1]) {
				const CPPGMAstNodePtr arguments = node->children[1];
				for(size_t i = 0; i < arguments->children.size(); ++i) {
					if(i) result += ", ";
					result += ConstantExpressionSpelling(arguments->children[i]);
				}
			}
			return result + ")";
		}
		if(node->kind == "member-expression" && node->children.size() >= 2)
			return ConstantExpressionSpelling(node->children[0]) +
				RemoveMarker(node->value) + ConstantExpressionSpelling(node->children[1]);
		return SpellNode(node);
	}
	void RecordConstantArrayDeclaration(const CPPGMAstNodePtr& node,
		const string& context, const map<string, string>& substitutions);
	const vector<PA19IntegralValue>* FindConstantArray(const string& raw,
		const string& context) const;
	bool EvaluateSourceArrayFunction(string raw, const string& context,
		const map<string, string>& substitutions, PA19IntegralValue* result);
	bool EvaluateMaterializedTemplateValue(const string& raw,
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
							(candidate_base->second != primary &&
							 LastComponent(candidate_base->second) != LastComponent(primary)) ||
							candidate->second.size() != arguments.size()) continue;
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
	bool ExpandIntegralValueOperands(const string& raw,
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
	bool EvaluateInheritedBaseValue(const TemplateDefinition& definition,
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
							string base_spelling = CanonicalSpelling(ReplaceIdentifiers(
								base_name->value, member_substitutions));
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
												if(base_definition->parameters[parameter].type)
													expected_substitutions[base_definition->parameters[parameter].name] =
														base_arguments[parameter];
											string expected_type = ReplaceIdentifiers(
												base_definition->parameters[argument].non_type_type,
												expected_substitutions);
											expected_type = ResolveAlias(expected_type, context);
											const PA19IntegralType typed = PA19Type(expected_type);
											if(typed.integral) normalized_value = PA19Convert(
												normalized_value, typed);
											base_arguments[argument] = IntegralValueSpelling(normalized_value);
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
							if(materialized_base.empty() &&
								class_contexts_.find(base_spelling) != class_contexts_.end())
								materialized_base = base_spelling;
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
	bool EvaluateInheritedIntegralValue(const string& raw,
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
		if(member_arguments != specialization_arguments_.end())
			for(size_t parameter = 0; parameter < definition->parameters.size() &&
				parameter < member_arguments->second.size(); ++parameter)
				if(!definition->parameters[parameter].name.empty())
					member_substitutions[definition->parameters[parameter].name] =
						member_arguments->second[parameter];
		return EvaluateInheritedBaseValue(*definition, context,
			member_substitutions, result);
	}
	CPPGMAstNodePtr FindSourceConstantFunction(string raw, const string& context) const;
	CPPGMAstNodePtr SourceReturnExpression(const CPPGMAstNodePtr& function) const;
	bool EvaluateSourceFunctionReturn(const CPPGMAstNodePtr& function,
		const string& context, const map<string, string>& substitutions,
		PA19IntegralValue* result);
	bool EvaluateSourceObjectMember(const string& raw, const string& context,
		const map<string, string>& substitutions, PA19IntegralValue* result);
	bool EvaluateSourceClassTruth(string raw, const string& context,
		const map<string, string>& substitutions, PA19IntegralValue* result);
	bool EvaluateSourceIntegralExpression(string raw, const string& context,
		const map<string, string>& substitutions, PA19IntegralValue* result);
		bool EvaluateIntegralText(string raw, const string& context,
		const map<string, string>& substitutions, PA19IntegralValue* result)
	{
		raw = CanonicalSpelling(ReplaceIdentifiers(raw, substitutions));
		PA19ConstantExpressionParser parser(constant_values_, substitutions,
			constant_type_sizes_, constant_type_alignments_, type_aliases_);
		const bool qualified_value_expression = raw.find("::value") != string::npos;
		if((!qualified_value_expression || constant_values_.find(raw) != constant_values_.end()) &&
			parser.Evaluate(raw, result)) return true;
		if(EvaluateSourceIntegralExpression(raw, context, substitutions, result)) return true;
		if(EvaluateMaterializedTemplateValue(raw, context, substitutions, result)) return true;
		if(ExpandIntegralValueOperands(raw, context, substitutions, result)) return true;
		if(EvaluateInheritedIntegralValue(raw, context, substitutions, result)) return true;
		// Some transformed dependent defaults retain the generated class spelling
		// but lose the explicit `::value` member suffix while the alias is being
		// replayed.  Recover that integral-constant object only for a materialized
		// class whose typed value is already registered.
		if(class_contexts_.find(raw) != class_contexts_.end()) {
			map<string, PA19IntegralValue>::const_iterator object_value =
				constant_values_.find(raw + "::value");
			if(object_value != constant_values_.end()) {
				*result = object_value->second;
				return result->known;
			}
		}
		if(raw.find("::") == string::npos && !context.empty()) {
			for(string current = context; ; ) {
				const string qualified = JoinPath(current, raw);
				if(parser.Evaluate(qualified, result)) return true;
				const size_t separator = current.rfind("::");
				if(separator == string::npos) break;
				current.erase(separator);
			}
		}
		// A member constant of the class currently being materialized may be
		// referenced by a later member type before the generated class has been
		// registered in constant_values_.  Evaluate its source initializer with
		// the active template substitutions so aliases such as `ratio1<num>` can
		// be resolved during the same transformation pass.
		string owner = context;
		string member = raw;
		const size_t separator = raw.rfind("::");
		if(separator != string::npos) {
			owner = raw.substr(0, separator);
			member = raw.substr(separator + 2);
		}
		CPPGMAstNodePtr declaration = FindClassDeclaration(owner, context);
		if(declaration) {
			for(size_t i = 0; i < declaration->children.size(); ++i) {
				const CPPGMAstNodePtr child = declaration->children[i];
				if(!child || child->kind != "simple-declaration" || child->children.empty()) continue;
				const string specifiers = SpellNode(child->children[0]);
				if(specifiers.find("const") == string::npos &&
					specifiers.find("constexpr") == string::npos) continue;
				const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
				if(!list) continue;
				for(size_t j = 0; j < list->children.size(); ++j) {
					const CPPGMAstNodePtr item = list->children[j];
					if(!item || item->children.size() < 2) continue;
					if(LastComponent(FirstIdentifierLocal(item->children[0])) != LastComponent(member)) continue;
					const CPPGMAstNodePtr initializer = item->children[1];
					if(!initializer || initializer->children.empty()) continue;
					PA19IntegralValue value;
					const string member_expression = ConstantExpressionSpelling(initializer->children[0]);
					if(!EvaluateIntegralText(member_expression, owner, substitutions, &value))
						continue;
					const PA19IntegralType type = PA19Type(ResolveAlias(
						NodeTypeSpelling(child->children[0]), owner));
					if(type.integral) value = PA19Convert(value, type);
					*result = value;
					return result->known;
				}
			}
		}
		return false;
	}
	void RecordTemplateArrayValues(const TemplateDefinition& definition,
		const vector<string>& arguments, const string& context,
		const map<string, string>& substitutions);
	void RecordConstantDeclaration(const CPPGMAstNodePtr& node, const string& context)
	{
		if(!node || node->kind != "simple-declaration" || node->children.empty()) return;
		const string specifiers = SpellNode(node->children[0]);
		if(specifiers.find("const") == string::npos && specifiers.find("constexpr") == string::npos) return;
		const string base_type = NodeTypeSpelling(node->children[0]);
		const string resolved_base_type = ResolveAlias(base_type, context);
		if(!PA19Type(resolved_base_type).integral) return;
		const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
		if(!list) return;
		for(size_t i = 0; i < list->children.size(); ++i) {
			const CPPGMAstNodePtr item = list->children[i];
			if(!item || item->children.size() < 2) continue;
			const string name = FirstIdentifierLocal(item->children[0]);
			const CPPGMAstNodePtr initializer = item->children[1];
			if(name.empty() || !initializer || initializer->children.empty()) continue;
			PA19IntegralValue value;
			const string expression_text = ConstantExpressionSpelling(initializer->children[0]);
			if(!EvaluateIntegralText(expression_text, context,
				map<string,string>(), &value)) continue;
			const string qualified = JoinPath(context, name);
			constant_values_[qualified] = value;
			if(constant_values_.find(name) == constant_values_.end()) constant_values_[name] = value;
			const PA19IntegralType type = PA19Type(resolved_base_type);
			if(type.integral) {
				constant_type_sizes_[qualified] = type.bits <= 8 ? 1 : type.bits <= 16 ? 2 : type.bits <= 32 ? 4 : 8;
				constant_type_alignments_[qualified] = constant_type_sizes_[qualified];
			}
		}
	}
	void RecordEnumConstants(const CPPGMAstNodePtr& node, const string& context)
	{
		if(!node || node->kind != "enum-specifier") return;
		long long next = 0;
		const string enum_name = LastComponent(node->value);
		for(size_t i = 0; i < node->children.size(); ++i) {
			const CPPGMAstNodePtr enumerator = node->children[i];
			if(!enumerator || enumerator->kind != "enumerator") continue;
			PA19IntegralValue value = PA19IntegralValue::Signed(next, "int", 32);
			if(!enumerator->children.empty())
				EvaluateIntegralText(ConstantExpressionSpelling(enumerator->children[0]), context,
					map<string,string>(), &value);
			const string unqualified = JoinPath(context, enumerator->value);
			constant_values_[unqualified] = value;
			if(constant_values_.find(enumerator->value) == constant_values_.end())
				constant_values_[enumerator->value] = value;
			if(!enum_name.empty()) constant_values_[JoinPath(JoinPath(context, enum_name), enumerator->value)] = value;
			next = PA19Signed(value) + 1;
		}
	}
	bool FunctionParameterCounts(const CPPGMAstNodePtr& parameters,
		size_t* total, size_t* required) const
	{
		if(!parameters || !total || !required) return false;
		*total = 0;
		*required = 0;
		for(size_t i = 0; i < parameters->children.size(); ++i) {
			const CPPGMAstNodePtr parameter = parameters->children[i];
			if(!parameter || parameter->kind != "parameter-declaration") continue;
			++*total;
			if(!ChildOfKindLocal(parameter, "default-argument")) ++*required;
		}
		return true;
	}
	string ResolveGeneratedFunctionOwner(const string& owner, const string& context,
		string* child_context) const
	{
		if(class_contexts_.find(owner) != class_contexts_.end()) {
			if(child_context) *child_context = owner;
			return owner;
		}
		for(string current = context; ; ) {
			const string candidate = JoinPath(current, owner);
			if(class_contexts_.find(candidate) != class_contexts_.end()) {
				if(child_context) *child_context = candidate;
				return candidate;
			}
			if(current.empty()) break;
			const size_t separator = current.rfind("::");
			if(separator == string::npos) current.clear();
			else current.erase(separator);
		}
		return owner;
	}
	bool DefinitionHasDependentBase(const TemplateDefinition& definition) const
	{
		if(!definition.declaration) return false;
		for(size_t child = 0; child < definition.declaration->children.size(); ++child) {
			const CPPGMAstNodePtr clause = definition.declaration->children[child];
			if(!clause || clause->kind != "base-clause") continue;
			for(size_t base = 0; base < clause->children.size(); ++base) {
				const CPPGMAstNodePtr name = ChildOfKindLocal(clause->children[base], "base-name");
				if(!name) continue;
				const string spelling = RemoveMarker(name->value);
				for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter) {
					const string& wanted = definition.parameters[parameter].name;
					// An unnamed template parameter carries no dependent-name
					// information.  In particular, find("") never advances and
					// would turn this scan into an unbounded loop when PA19
					// materializes a trait with an unnamed parameter.
					if(wanted.empty()) continue;
					for(size_t position = spelling.find(wanted); position != string::npos;
						position = spelling.find(wanted, position + wanted.size())) {
						const bool left_boundary = position == 0 ||
							!IsIdentifierCharacter(spelling[position - 1]);
						const size_t end = position + wanted.size();
						const bool right_boundary = end == spelling.size() ||
							!IsIdentifierCharacter(spelling[end]);
						if(left_boundary && right_boundary) return true;
					}
				}
			}
		}
		return false;
	}

	void MarkGeneratedNode(const CPPGMAstNodePtr& node, const string& primary,
		const vector<string>& arguments, bool explicit_instantiation = false)
	{
		if(!node) return;
		node->template_instantiation = true;
		node->explicit_instantiation = node->explicit_instantiation || explicit_instantiation;
		node->template_primary = primary;
		node->template_arguments = arguments;
		for(size_t i = 0; i < node->children.size(); ++i)
			MarkGeneratedNode(node->children[i], primary, arguments,
				explicit_instantiation);
	}
	void RenameGeneratedFunction(const CPPGMAstNodePtr& declaration,
		const string& name)
	{
		if(!declaration || name.empty()) return;
		const CPPGMAstNodePtr declarator = FunctionDeclarator(declaration);
		if(!declarator) return;
		RenameGeneratedIdentifier(declarator, name);
	}
	bool RenameGeneratedIdentifier(const CPPGMAstNodePtr& node,
		const string& name)
	{
		if(!node) return false;
		if(node->kind == "identifier") {
			node->value = name;
			return true;
		}
		for(size_t i = 0; i < node->children.size(); ++i)
			if(RenameGeneratedIdentifier(node->children[i], name)) return true;
		return false;
	}
	string RestoreSpecializationSpelling(const string& raw) const
	{
		string result;
		for(size_t i = 0; i < raw.size();) {
			if(!IsIdentifierCharacter(raw[i])) {
				result += raw[i++];
				continue;
			}
			size_t end = i + 1;
			while(end < raw.size() && IsIdentifierCharacter(raw[end])) ++end;
			const string word = raw.substr(i, end - i);
			map<string, string>::const_iterator base = specialization_bases_.find(word);
			map<string, vector<string> >::const_iterator arguments =
				specialization_arguments_.find(word);
			if(base != specialization_bases_.end() && arguments != specialization_arguments_.end()) {
				result += base->second + "<";
				for(size_t argument = 0; argument < arguments->second.size(); ++argument) {
					if(argument != 0) result += ", ";
					result += RestoreSpecializationSpelling(arguments->second[argument]);
				}
				result += ">";
			} else result += word;
			i = end;
		}
		return result;
	}
	const TemplateDefinition* FindNestedDefinition(const TemplateDefinition& parent,
		const string& nested_name) const
	{
		for(map<string, TemplateDefinition>::const_iterator it = definitions_.begin();
			it != definitions_.end(); ++it) {
			const TemplateDefinition& candidate = it->second;
			if(!candidate.class_template || candidate.name != nested_name) continue;
			const size_t angle = candidate.owner.find('<');
			if(angle == string::npos || candidate.owner.substr(0, angle) !=
				parent.qualified_name) continue;
			return &candidate;
		}
		return 0;
	}
	vector<const TemplateDefinition*> NestedDefinitions(
		const TemplateDefinition& parent) const
	{
		vector<const TemplateDefinition*> result;
		for(map<string, TemplateDefinition>::const_iterator it = definitions_.begin();
			it != definitions_.end(); ++it) {
			const TemplateDefinition& candidate = it->second;
			if(!candidate.class_template) continue;
			const size_t angle = candidate.owner.find('<');
			if(angle != string::npos && candidate.owner.substr(0, angle) ==
				parent.qualified_name)
				result.push_back(&candidate);
		}
		return result;
	}
	vector<const TemplateDefinition*> MemberDefinitions(
		const TemplateDefinition& parent) const
	{
		vector<const TemplateDefinition*> result;
		for(map<string, TemplateDefinition>::const_iterator it = definitions_.begin();
			it != definitions_.end(); ++it) {
			const TemplateDefinition& candidate = it->second;
			if(candidate.class_template ||
				(candidate.declaration->kind != "simple-declaration" &&
					candidate.declaration->kind != "function-definition" &&
					candidate.declaration->kind != "special-member-definition")) continue;
			// Anonymous namespace components use the ABI spelling
			// `<unnamed>`.  Searching from the beginning would mistake that
			// namespace delimiter for the template argument list.
			const size_t angle = candidate.owner.rfind('<');
			const size_t close = angle == string::npos ? string::npos :
				candidate.owner.find('>', angle);
			if(angle != string::npos && close != string::npos &&
				candidate.owner.substr(0, angle) == parent.qualified_name)
				result.push_back(&candidate);
		}
		return result;
	}
	void InstantiateMemberDefinitions(const TemplateDefinition& parent,
		const vector<string>& parent_args, const string& parent_local_name,
		bool explicit_instantiation = false)
	{
		const vector<const TemplateDefinition*> members = MemberDefinitions(parent);
		for(size_t i = 0; i < members.size(); ++i) {
			const TemplateDefinition& member = *members[i];
			const string key = member.qualified_name + "@" + parent_local_name;
			if(!materialized_member_definitions_.insert(key).second) continue;
			map<string, string> substitutions;
			map<string, vector<string> > pack_substitutions;
			map<string, PA19IntegralValue> integral_substitutions;
			size_t parent_argument = 0;
			for(size_t parameter = 0; parameter < member.parameters.size(); ++parameter) {
				const TemplateParameter& member_parameter = member.parameters[parameter];
				if(member_parameter.pack) {
					size_t trailing_fixed = 0;
					for(size_t later = parameter + 1; later < member.parameters.size(); ++later)
						if(!member.parameters[later].pack) ++trailing_fixed;
					const size_t available = parent_args.size() > parent_argument ?
						parent_args.size() - parent_argument : 0;
					const size_t count = available > trailing_fixed ? available - trailing_fixed : 0;
					vector<string>& values = pack_substitutions[member_parameter.name];
					for(size_t value = 0; value < count; ++value)
						values.push_back(parent_args[parent_argument++]);
					if(!values.empty()) substitutions[member_parameter.name] = values[0];
					else substitutions.erase(member_parameter.name);
					continue;
				}
				if(parent_argument < parent_args.size())
					substitutions[member_parameter.name] = parent_args[parent_argument++];
			}
			substitutions[parent.name] = parent_local_name;
			const string generated_context = JoinPath(
				parent.lexical_owner.empty() ? parent.owner : parent.lexical_owner,
				parent_local_name);
			map<string, CPPGMAstNodePtr>::const_iterator concrete = class_declarations_.find(
				JoinPath(parent.owner, parent_local_name));
			if(concrete != class_declarations_.end() && concrete->second) {
				for(size_t child = 0; child < concrete->second->children.size(); ++child) {
					const CPPGMAstNodePtr declaration = concrete->second->children[child];
					if(!declaration) continue;
					if(declaration->kind == "alias-declaration" &&
						!declaration->children.empty())
						substitutions[declaration->value] = RewriteText(
							TypeIdSpelling(declaration->children[0]), generated_context,
							substitutions, 0);
					else if(declaration->kind == "simple-declaration" &&
						!declaration->children.empty() &&
						SpellNode(declaration->children[0]).find("typedef") != string::npos) {
						const CPPGMAstNodePtr list = ChildOfKindLocal(declaration,
							"init-declarator-list");
						if(!list) continue;
						for(size_t item = 0; item < list->children.size(); ++item) {
							const CPPGMAstNodePtr entry = list->children[item];
							if(!entry || entry->children.empty()) continue;
							const string alias = FirstIdentifierLocal(entry->children[0]);
							if(alias.empty()) continue;
							substitutions[alias] = RewriteText(
								NodeTypeSpelling(declaration->children[0]) +
								DeclaratorSuffix(entry->children[0]), generated_context,
								substitutions, 0);
						}
					}
				}
			}
			CPPGMAstNodePtr generated = TransformInstantiatedNode(member,
				generated_context, substitutions, integral_substitutions,
				pack_substitutions);
			if(!generated) continue;
			if(member.declaration->kind == "special-member-definition" &&
				LastComponent(member.declaration->value) == parent.name) {
				const CPPGMAstNodePtr declarator = FunctionDeclarator(generated);
				if(declarator) for(size_t child = 0; child < declarator->children.size(); ++child) {
					const CPPGMAstNodePtr identifier = declarator->children[child];
					if(!identifier || identifier->kind != "identifier") continue;
					const string owner = PrefixComponent(identifier->value);
					identifier->value = (owner.empty() ? string() : owner + "::") +
						parent_local_name;
					break;
				}
			}
			MarkGeneratedNode(generated, parent.qualified_name, parent_args,
				explicit_instantiation);
			generated_by_owner_[parent.lexical_owner.empty() ? parent.owner :
				parent.lexical_owner].push_back(generated);
		}
	}
	void InstantiateNestedClass(const TemplateDefinition& parent,
		const vector<string>& parent_args, const string& parent_local_name,
		const string& nested_name, const string& context)
	{
		const TemplateDefinition* nested = FindNestedDefinition(parent, nested_name);
		if(!nested) return;
		class_contexts_.insert(parent_local_name);
		class_contexts_.insert(parent_local_name + "::" + nested_name);
		const string key = parent_local_name + "::" + nested_name;
		if(!materialized_nested_classes_.insert(key).second) return;
		map<string, string> substitutions;
		for(size_t i = 0; i < nested->parameters.size() && i < parent_args.size(); ++i)
			if(nested->parameters[i].type || !nested->parameters[i].non_type_type.empty())
				substitutions[nested->parameters[i].name] = parent_args[i];
		substitutions[parent.name] = parent_local_name;
		const vector<const TemplateDefinition*> candidates = NestedDefinitions(parent);
		for(size_t i = 0; i < candidates.size(); ++i) {
			if(candidates[i]->name == nested_name ||
				!MentionsGeneratedType(nested->declaration, candidates[i]->name)) continue;
			InstantiateNestedClass(parent, parent_args, parent_local_name,
				candidates[i]->name, context);
		}
		const string generated_context = JoinPath(
			parent.lexical_owner.empty() ? parent.owner : parent.lexical_owner,
			parent_local_name);
		CPPGMAstNodePtr generated = TransformNode(nested->declaration,
			generated_context, substitutions);
		if(!generated) return;
		MarkGeneratedNode(generated, parent.qualified_name, parent_args);
		generated->value = nested_name;
		class_declarations_[parent_local_name + "::" + nested_name] = generated;
		EnsureDeclarationDependencies(generated, parent_local_name, parent_local_name);
		generated_by_owner_[parent_local_name].push_back(generated);
	}
	void InstantiateRequestedNestedClasses(const TemplateDefinition& parent,
		const vector<string>& parent_args, const string& parent_local_name,
		const string& context)
	{
		set<string> requested;
		map<string, set<string> >::const_iterator full =
			requested_nested_classes_.find(parent.qualified_name);
		if(full != requested_nested_classes_.end())
			requested.insert(full->second.begin(), full->second.end());
		map<string, set<string> >::const_iterator short_name =
			requested_nested_classes_.find(LastComponent(parent.qualified_name));
		if(short_name != requested_nested_classes_.end())
			requested.insert(short_name->second.begin(), short_name->second.end());
		const vector<const TemplateDefinition*> candidates = NestedDefinitions(parent);
		for(size_t i = 0; i < candidates.size(); ++i)
			class_contexts_.insert(parent_local_name + "::" + candidates[i]->name);
		for(set<string>::const_iterator it = requested.begin(); it != requested.end(); ++it)
			InstantiateNestedClass(parent, parent_args, parent_local_name, *it, context);
	}
	string ResolveIntegralArgument(const TemplateParameter& parameter,
		string raw, const string& context, const map<string, string>& substitutions,
		PA19IntegralValue* typed_result = 0)
	{
		raw = RemoveMarker(RewriteText(raw, context, substitutions, 0));
		raw = ReplaceIdentifiers(raw, substitutions);
		PA19IntegralValue value;
		if(!EvaluateIntegralText(raw, context, substitutions, &value)) {
			throw logic_error("non-type template argument is not an integral constant");
		}
		string expected = RewriteText(parameter.non_type_type, context, substitutions, 0);
		expected = ResolveAlias(ReplaceIdentifiers(expected, substitutions), context);
		const PA19IntegralType expected_type = PA19Type(expected);
		if(expected_type.integral) value = PA19Convert(value, expected_type);
		if(typed_result) *typed_result = value;
		return TemplateIntegralValueSpelling(value);
	}
	bool IsKnownEnumType(string raw, const string& context) const
	{
		raw = CanonicalSpelling(RemoveMarker(raw));
		if(raw.compare(0, 5, "enum ") == 0) raw = Trim(raw.substr(5));
		if(named_type_contexts_.find(raw) != named_type_contexts_.end()) return true;
		string current = CanonicalSpelling(context);
		for(;;) {
			if(named_type_contexts_.find(JoinPath(current, raw)) !=
				named_type_contexts_.end()) return true;
			if(current.empty()) break;
			const size_t separator = current.rfind("::");
			if(separator == string::npos) current.clear();
			else current.erase(separator);
		}
		return false;
	}
	string TemplateArgumentMetadata(const TemplateParameter& parameter,
		const string& argument, const PA19IntegralValue& value,
		const string& context, const map<string, string>& substitutions)
	{
		if(parameter.type || !value.known) return RestoreSpecializationSpelling(argument);
		string declared = RewriteText(parameter.non_type_type, context, substitutions, 0);
		declared = ResolveAlias(ReplaceIdentifiers(declared, substitutions), context);
		if(IsKnownEnumType(declared, context))
			return CanonicalSpelling(declared + " " + IntegralValueSpelling(value));
		return RestoreSpecializationSpelling(argument);
	}
	void RegisterGeneratedTypeAlias(const CPPGMAstNodePtr& generated,
		const string& generated_path)
	{
		if(!generated) return;
		if(generated->kind == "alias-declaration" && !generated->value.empty() &&
			!generated->children.empty()) {
			const string alias = JoinPath(generated_path, generated->value);
			const string target = TypeIdSpelling(generated->children[0]);
			if(!target.empty()) {
				type_aliases_[alias] = CanonicalSpelling(target);
				vector<string>& aliases = type_aliases_by_name_[generated->value];
				if(find(aliases.begin(), aliases.end(), alias) == aliases.end())
					aliases.push_back(alias);
			}
			return;
		}
		if(generated->kind != "simple-declaration" ||
			generated->children.empty() ||
			SpellNode(generated->children[0]).find("typedef") == string::npos)
			return;
		const CPPGMAstNodePtr list = ChildOfKindLocal(generated,
			"init-declarator-list");
		if(!list) return;
		for(size_t i = 0; i < list->children.size(); ++i) {
			const CPPGMAstNodePtr item = list->children[i];
			if(!item || item->children.empty()) continue;
			const string name = LastComponent(FirstIdentifierLocal(item->children[0]));
			if(name.empty()) continue;
			const string alias = JoinPath(generated_path, name);
			const string target = DeclaratorTypeSpelling(
				NodeTypeSpelling(generated->children[0]), item->children[0]);
			if(target.empty()) continue;
			type_aliases_[alias] = CanonicalSpelling(target);
			vector<string>& aliases = type_aliases_by_name_[name];
			if(find(aliases.begin(), aliases.end(), alias) == aliases.end())
				aliases.push_back(alias);
		}
	}
	void RegisterGeneratedConstants(
		const CPPGMAstNodePtr& generated, const string& generated_path)
	{
		if(!generated) return;
		RegisterGeneratedTypeAlias(generated, generated_path);
		if(generated->kind == "class-specifier" ||
			generated->kind == "class-forward-declaration")
			for(size_t i = 0; i < generated->children.size(); ++i)
				RegisterGeneratedTypeAlias(generated->children[i], generated_path);
		if(generated->kind == "simple-declaration")
			RecordConstantDeclaration(generated, generated_path);
		if(generated->kind == "enum-specifier")
			RecordEnumConstants(generated, generated_path);
		for(size_t i = 0; i < generated->children.size(); ++i)
			if(generated->children[i] && generated->children[i]->kind != "compound-statement") {
				// `generated_path` already names the materialized class.  The old
				// recursion appended the current class name again, so a member
				// such as `Box_int::value` was recorded as
				// `Box_int::Box_int::value` and could not be found by a later
				// dependent non-type argument.  Extend the path only when the
				// child itself opens a nested class scope.
				string child_path = generated_path;
				if(generated->children[i]->kind == "class-specifier" ||
					generated->children[i]->kind == "class-forward-declaration")
					child_path = JoinPath(generated_path,
						LastComponent(generated->children[i]->value));
				RegisterGeneratedConstants(generated->children[i], child_path);
			}
	}
	CPPGMAstNodePtr TransformInstantiatedNode(const TemplateDefinition& definition,
		const string& context, const map<string, string>& substitutions,
		const map<string, PA19IntegralValue>& integral_substitutions,
		const map<string, vector<string> >& pack_substitutions)
	{
		const map<string, PA19IntegralValue> previous = active_integral_substitutions_;
		const map<string, vector<string> > previous_packs = active_pack_substitutions_;
		const map<string, vector<string> > previous_pack_identifiers =
			active_pack_identifier_substitutions_;
		active_integral_substitutions_ = integral_substitutions;
		active_pack_substitutions_ = pack_substitutions;
		active_pack_identifier_substitutions_.clear();
		try {
			CPPGMAstNodePtr result = TransformNode(definition.declaration, context, substitutions);
			active_integral_substitutions_ = previous;
			active_pack_substitutions_ = previous_packs;
			active_pack_identifier_substitutions_ = previous_pack_identifiers;
			return result;
		} catch(...) {
			active_integral_substitutions_ = previous;
			active_pack_substitutions_ = previous_packs;
			active_pack_identifier_substitutions_ = previous_pack_identifiers;
			throw;
		}
	}
	void ResolveTemplateArguments(const TemplateDefinition& definition,
		const vector<string>& raw_args, const string& context,
		vector<string>* args, vector<string>* metadata_args,
		map<string, string>* substitutions,
		map<string, PA19IntegralValue>* integral_substitutions,
		map<string, vector<string> >* pack_substitutions,
		const map<string, vector<string> >* pack_hints = 0)
	{
		size_t raw_index = 0;
		for(size_t i = 0; i < definition.parameters.size(); ++i) {
			const TemplateParameter& parameter = definition.parameters[i];
			if(parameter.pack) {
				vector<string> values;
				// A function parameter pack is normally trailing.  Reserve source
				// arguments for any fixed template parameters that follow it so
				// the same collector also handles a pack in a partial declaration.
				size_t trailing_fixed = 0;
				for(size_t later = i + 1; later < definition.parameters.size(); ++later)
					if(!definition.parameters[later].pack) ++trailing_fixed;
				const size_t available = raw_args.size() > raw_index ?
					raw_args.size() - raw_index : 0;
				size_t count = available > trailing_fixed ? available - trailing_fixed : 0;
				if(pack_hints) {
					map<string, vector<string> >::const_iterator hint =
						pack_hints->find(parameter.name);
					if(hint != pack_hints->end()) count = hint->second.size();
				}
					for(size_t element = 0; element < count; ++element) {
						string argument = raw_args[raw_index++];
						PA19IntegralValue integral_value;
						if(parameter.type) {
						argument = RewriteText(argument, context, *substitutions, 0);
						argument = NormalizeTypeArgument(argument);
						argument = NormalizeTypeArgument(ReplaceIdentifiers(argument, *substitutions));
						argument = ResolveAlias(argument, context);
						argument = RewriteText(argument, context, *substitutions, 0);
						argument = NormalizeTypeArgument(argument);
						argument = QualifyTypeArgument(argument, context, definition.owner);
						} else {
							argument = ResolveIntegralArgument(parameter, argument, context,
							*substitutions, &integral_value);
						(*integral_substitutions)[parameter.name] = integral_value;
					}
				if(argument.empty()) throw logic_error("missing template argument");
						values.push_back(argument);
						args->push_back(argument);
						metadata_args->push_back(TemplateArgumentMetadata(parameter, argument,
							integral_value, context, *substitutions));
				}
				if(pack_substitutions) (*pack_substitutions)[parameter.name] = values;
				if(!values.empty()) (*substitutions)[parameter.name] = values[0];
				else substitutions->erase(parameter.name);
				continue;
			}
			string argument;
			PA19IntegralValue integral_value;
			if(raw_index < raw_args.size() && !raw_args[raw_index].empty())
				argument = raw_args[raw_index++];
			else {
				map<string, string>::const_iterator substituted = substitutions->find(parameter.name);
				if(substituted != substitutions->end()) argument = substituted->second;
				map<string, PA19IntegralValue>::const_iterator integral =
					integral_substitutions->find(parameter.name);
				if(argument.empty() && integral != integral_substitutions->end())
					argument = TemplateIntegralValueSpelling(integral->second);
			}
			if(argument.empty()) argument = parameter.default_type;
			if(parameter.type) {
				argument = ExpandPackCallText(argument, *pack_substitutions);
				argument = RewriteText(argument, context, *substitutions, 0);
				argument = NormalizeTypeArgument(argument);
				argument = NormalizeTypeArgument(ReplaceIdentifiers(argument, *substitutions));
				argument = ResolveAlias(argument, context);
				argument = RewriteText(argument, context, *substitutions, 0);
				argument = NormalizeTypeArgument(argument);
				argument = QualifyTypeArgument(argument, context, definition.owner);
			} else {
				argument = ResolveIntegralArgument(parameter, argument, context, *substitutions,
					&integral_value);
				(*integral_substitutions)[parameter.name] = integral_value;
			}
			if(argument.empty()) throw logic_error("missing template argument");
			args->push_back(argument);
			metadata_args->push_back(TemplateArgumentMetadata(parameter, argument,
				integral_value, context, *substitutions));
			(*substitutions)[parameter.name] = argument;
		}
		if(raw_index != raw_args.size())
			throw logic_error("too many template arguments");
	}
	string GeneratedSpecializationName(const TemplateDefinition& definition,
		const vector<string>& args, const vector<string>& metadata_args,
		const map<string, string>& substitutions, const string& context)
	{
		string local_name = definition.name;
		if(definition.class_template || definition.alias_template) {
			for(size_t i = 0; i < args.size(); ++i) {
				local_name += i == 0 ? "_" : "__";
				if(i < definition.parameters.size() && !definition.parameters[i].type) {
					string declared = RewriteText(definition.parameters[i].non_type_type,
						context, substitutions, 0);
					declared = ResolveAlias(ReplaceIdentifiers(declared, substitutions), context);
					if(IsKnownEnumType(declared, context)) local_name += "_";
				}
				const bool enum_argument = i < definition.parameters.size() &&
					!definition.parameters[i].type && IsKnownEnumType(
						ResolveAlias(ReplaceIdentifiers(definition.parameters[i].non_type_type,
							substitutions), context), context);
				local_name += TypeSuffix(enum_argument ? metadata_args[i] : args[i]);
				if(i + 1 == args.size()) {
					const bool trailing_pack = !definition.parameters.empty() &&
						definition.parameters.back().pack;
					const bool nested_specialization =
						specialization_bases_.find(LastComponent(args[i])) !=
						specialization_bases_.end();
					local_name += trailing_pack ? "_" :
						(i == 0 && nested_specialization ? "__" : "_");
				}
			}
		} else if(definition.name.compare(0, 8, "operator") != 0) {
			// Keep each concrete function specialization distinct in its generated AST.
			for(size_t i = 0; i < args.size(); ++i) {
				local_name += i == 0 ? "__inst_" : "__";
				local_name += TypeSuffix(args[i]);
			}
		}
		return local_name;
	}
	string Instantiate(const TemplateDefinition& definition, const vector<string>& raw_args,
		const string& context, bool explicit_instantiation = false,
		const map<string, vector<string> >* pack_hints = 0)
	{
		if(definition.parameters.empty()) throw logic_error("template has no type parameters");
		vector<string> args, metadata_args;
		map<string, string> substitutions;
		map<string, PA19IntegralValue> integral_substitutions;
		map<string, vector<string> > pack_substitutions;
		ResolveTemplateArguments(definition, raw_args, context, &args, &metadata_args,
			&substitutions, &integral_substitutions, &pack_substitutions, pack_hints);
		if(definition.partial_specialization) {
			map<string, string> specialized;
			if(!MatchClassSpecializationPattern(definition, args, &specialized,
				context)) throw logic_error("class partial specialization does not match");
			for(map<string, string>::const_iterator it = specialized.begin();
				it != specialized.end(); ++it)
				substitutions[it->first] = it->second;
		}
		ostringstream definition_key;
		definition_key << definition.qualified_name << "@" << definition.declaration.get();
		string key = definition_key.str();
		for(size_t i = 0; i < args.size(); ++i) key += "|" + CanonicalSpelling(args[i]);
		map<string, string>::const_iterator cached = specializations_.find(key);
		if(cached != specializations_.end()) {
			if(definition.class_template) {
				InstantiateRequestedNestedClasses(definition, args, cached->second, context);
				InstantiateMemberDefinitions(definition, args, cached->second,
					explicit_instantiation);
			}
			return cached->second;
		}
		string local_name = GeneratedSpecializationName(definition, args,
			metadata_args, substitutions, context);
	const string generated_owner = definition.lexical_owner.empty() ?
		definition.owner : definition.lexical_owner;
	if(definition.class_template) {
		specialization_bases_[local_name] = definition.qualified_name;
			specialization_arguments_[local_name] = metadata_args;
		const string forward_owner = generated_owner;
			if(class_contexts_.find(forward_owner) == class_contexts_.end()) {
				vector<CPPGMAstNodePtr>& forwards = generated_namespace_forwards_[forward_owner];
				bool already_forwarded = false;
				for(size_t i = 0; i < forwards.size(); ++i)
					if(forwards[i] && LastComponent(forwards[i]->value) == local_name)
						already_forwarded = true;
				if(!already_forwarded) forwards.push_back(MakeForwardClass(local_name));
			}
		}
		if(definition.class_template) substitutions[definition.name] = local_name;
		specializations_[key] = local_name;
		if(!active_specializations_.insert(key).second) return local_name;
		if(definition.class_template) {
			class_contexts_.insert(JoinPath(definition.owner, local_name));
			class_contexts_.insert(JoinPath(
				definition.lexical_owner.empty() ? definition.owner : definition.lexical_owner,
				local_name));
		}
		CPPGMAstNodePtr generated = TransformInstantiatedNode(definition,
			definition.owner, substitutions, integral_substitutions,
			pack_substitutions);
		if(!generated) throw logic_error("unable to instantiate template");
		MarkGeneratedNode(generated, definition.qualified_name, metadata_args,
			explicit_instantiation);
		if(generated->kind == "simple-declaration") {
			const CPPGMAstNodePtr identifier = DescendantOfKind(generated, "identifier");
			if(identifier) identifier->value = local_name;
		}
		if(generated->kind == "simple-declaration")
			RecordConstantDeclaration(generated, generated_owner);
		if(definition.class_template)
			generated->dependent_base_lookup = DefinitionHasDependentBase(definition);
		if(!definition.class_template && !definition.alias_template)
			RenameGeneratedFunction(generated, local_name);
		if(definition.class_template || definition.alias_template) generated->value = local_name;
		if(definition.class_template) {
			const string generated_path = JoinPath(definition.owner, local_name);
			class_declarations_[generated_path] = generated;
			const string lexical_path = JoinPath(
				definition.lexical_owner.empty() ? definition.owner : definition.lexical_owner,
				local_name);
			class_declarations_[lexical_path] = generated;
			class_contexts_.insert(generated_path);
			RegisterGeneratedConstants(generated, generated_path);
			InstantiateRequestedNestedClasses(definition, args, local_name, context);
			InstantiateMemberDefinitions(definition, args, local_name,
				explicit_instantiation);
		}
		EnsureDeclarationDependencies(generated, definition.owner, generated_owner);
		for(size_t i = 0; i < args.size(); ++i)
			EnsureForwardClass(args[i], context, generated_owner);
		bool recursive_context_argument = false;
		for(size_t i = 0; i < args.size(); ++i)
			if(LastComponent(args[i]) == LastComponent(context) && !context.empty())
				recursive_context_argument = true;
		if(class_contexts_.find(context) != class_contexts_.end() && context != definition.owner &&
			!recursive_context_argument)
			generated_before_class_[context].push_back(generated);
		else if(recursive_context_argument && definition.owner.empty() &&
			!PrefixComponent(context).empty())
			generated_before_class_[PrefixComponent(context)].push_back(generated);
		else generated_by_owner_[generated_owner].push_back(generated);
		active_specializations_.erase(key);
		(void)context;
		return local_name;
	}
