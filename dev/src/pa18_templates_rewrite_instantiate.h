#pragma once

	string NodeTypeSpelling(const CPPGMAstNodePtr& sequence) const
	{
		if(!sequence) return string();
		string result;
		for(size_t i = 0; i < sequence->children.size(); ++i) {
		const CPPGMAstNodePtr child = sequence->children[i];
		if(!child) continue;
		const string spelling = RemoveMarker(child->value);
		if(child->kind == "decl-specifier" &&
			(spelling == "typedef" || spelling == "static" || spelling == "inline" ||
			 spelling == "constexpr" || spelling == "extern" ||
			 spelling == "thread_local" || spelling == "register" || spelling == "mutable"))
			continue;
		if(child->kind != "decl-specifier" && child->kind != "type-name" &&
			child->kind != "type-specifier" && child->kind != "decltype-specifier" &&
			child->kind != "cv-qualifier") continue;
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
		PA19IntegralValue* result);
	bool ExpandIntegralValueOperands(const string& raw,
		const string& context, const map<string, string>& substitutions,
		PA19IntegralValue* result);
	bool EvaluateInheritedBaseValue(const TemplateDefinition& definition,
		const string& context, const map<string, string>& member_substitutions,
		PA19IntegralValue* result);
	bool EvaluateInheritedIntegralValue(const string& raw,
		const string& context, const map<string, string>& substitutions,
		PA19IntegralValue* result);
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
	bool ExpandIntegralPackExpression(const string& raw, const string& context,
		const map<string, string>& substitutions, string* expanded);
	bool EvaluateUnqualifiedConstantMember(const string& raw,
		const string& context, const map<string, string>& substitutions,
		PA19IntegralValue* result);
	bool ExpandNamedIntegralOperands(const string& raw, const string& context,
		const map<string, string>& substitutions, PA19IntegralValue* result);
	string NormalizeIntegralExpression(string raw) const;
	bool EvaluateActivePackSize(string raw, PA19IntegralValue* result) const;
	string RewriteActivePackSizes(string raw) const;
	bool EvaluateIntegralText(string raw, const string& context,
		const map<string, string>& substitutions, PA19IntegralValue* result);
	void RecordTemplateArrayValues(const TemplateDefinition& definition,
		const vector<string>& arguments, const string& context,
		const map<string, string>& substitutions);
	void RecordConstantDeclaration(const CPPGMAstNodePtr& node, const string& context,
		const map<string, string>& substitutions = map<string, string>())
	{
		if(!node || node->kind != "simple-declaration" || node->children.empty()) return;
		const string specifiers = SpellNode(node->children[0]);
		if(specifiers.find("const") == string::npos && specifiers.find("constexpr") == string::npos) return;
		const string base_type = NodeTypeSpelling(node->children[0]);
		const string resolved_base_type = ResolveAlias(
			ReplaceIdentifiers(base_type, substitutions), context);
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
			// A primary template's dependent static initializer is indexed before a
			// concrete specialization has a substitution scope.  Leaving it
			// deferred avoids treating the member itself as an unqualified constant
			// operand (`value`) and recursively evaluating its own initializer.
			if(!HasReplayContext(substitutions) &&
				expression_text.find("decltype(") != string::npos)
				continue;
			if(!EvaluateIntegralText(expression_text, context,
				substitutions, &value)) continue;
			const string qualified = JoinPath(
				active_instantiation_name_.empty() ? context : active_instantiation_name_, name);
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
			if(angle == string::npos) continue;
			const string owner_prefix = candidate.owner.substr(0, angle);
			if(owner_prefix != parent.qualified_name && owner_prefix !=
				JoinPath(parent.qualified_name, parent.qualified_name)) continue;
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
			if(angle != string::npos && (candidate.owner.substr(0, angle) ==
				parent.qualified_name || candidate.owner.substr(0, angle) ==
				JoinPath(parent.qualified_name, parent.qualified_name)))
				result.push_back(&candidate);
		}
		return result;
	}
	bool MemberOwnerPattern(const TemplateDefinition& candidate,
		const TemplateDefinition& parent, const vector<string>& parent_args,
		map<string, string>* inferred) const;
	string MemberSignatureKey(const TemplateDefinition& candidate) const;
	vector<const TemplateDefinition*> MemberDefinitions(
		const TemplateDefinition& parent, const vector<string>& parent_args) const;
	void InstantiateMemberDefinitions(const TemplateDefinition& parent,
		const vector<string>& parent_args, const string& parent_local_name,
		bool explicit_instantiation = false)
	{
		const vector<const TemplateDefinition*> members = MemberDefinitions(parent, parent_args);
		for(size_t i = 0; i < members.size(); ++i) {
			const TemplateDefinition& member = *members[i];
			const string key = member.qualified_name + "@" + parent_local_name;
			if(!materialized_member_definitions_.insert(key).second) continue;
			map<string, string> substitutions;
			map<string, vector<string> > pack_substitutions;
			map<string, PA19IntegralValue> integral_substitutions;
			for(size_t parameter = 0; parameter < parent.parameters.size() &&
				parameter < parent_args.size(); ++parameter)
				if(!parent.parameters[parameter].name.empty())
					substitutions[parent.parameters[parameter].name] = parent_args[parameter];
			map<string, string> owner_substitutions;
			MemberOwnerPattern(member, parent, parent_args, &owner_substitutions);
			size_t parent_argument = 0;
			for(size_t parameter = 0; parameter < member.parameters.size(); ++parameter) {
				const TemplateParameter& member_parameter = member.parameters[parameter];
				map<string, string>::const_iterator owner_value = owner_substitutions.find(
					member_parameter.name);
				if(member_parameter.pack) {
					vector<string> values;
					if(owner_value != owner_substitutions.end())
						values = SplitTemplateArguments(owner_value->second);
					else {
						size_t trailing_fixed = 0;
						for(size_t later = parameter + 1; later < member.parameters.size(); ++later)
							if(!member.parameters[later].pack) ++trailing_fixed;
						const size_t available = parent_args.size() > parent_argument ?
							parent_args.size() - parent_argument : 0;
						const size_t count = available > trailing_fixed ? available - trailing_fixed : 0;
						for(size_t value = 0; value < count; ++value)
							values.push_back(parent_args[parent_argument++]);
					}
					if(!member_parameter.name.empty()) {
						pack_substitutions[member_parameter.name] = values;
						if(!values.empty()) substitutions[member_parameter.name] = values[0];
						else substitutions.erase(member_parameter.name);
					}
					continue;
				}
				if(owner_value != owner_substitutions.end())
				{
					string value = owner_value->second;
					if(member_parameter.type && value.size() > 1 &&
						(value.compare(value.size() - 2, 2, "&&") == 0 ||
						 value[value.size() - 1] == '&'))
						value.erase(value.size() - (value.compare(value.size() - 2, 2, "&&") == 0 ? 2 : 1));
					if(!member_parameter.name.empty())
						substitutions[member_parameter.name] = CanonicalSpelling(value);
				}
				else if(parent_argument < parent_args.size())
				{
					string value = parent_args[parent_argument++];
					if(member_parameter.type && value.size() > 1 &&
						(value.compare(value.size() - 2, 2, "&&") == 0 ||
						 value[value.size() - 1] == '&'))
						value.erase(value.size() - (value.compare(value.size() - 2, 2, "&&") == 0 ? 2 : 1));
					if(!member_parameter.name.empty())
						substitutions[member_parameter.name] = CanonicalSpelling(value);
				}
			}
			substitutions[parent.name] = parent_local_name;
			const string generated_context = JoinPath(GeneratedOwner(parent),
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
			const string generated_owner = GeneratedOwner(parent);
			generated_by_owner_[generated_owner].push_back(generated);
		}
	}
	void InstantiateNestedClass(const TemplateDefinition& parent,
		const vector<string>& parent_args, const string& parent_local_name,
		const string& nested_name, const string& context)
	{
		const TemplateDefinition* nested = FindNestedDefinition(parent, nested_name);
		CPPGMAstNodePtr nested_declaration = nested ? nested->declaration : CPPGMAstNodePtr();
		vector<TemplateParameter> nested_parameters;
		if(nested) nested_parameters = nested->parameters;
		if(!nested) {
			for(size_t child = 0; parent.declaration && child < parent.declaration->children.size(); ++child) {
				const CPPGMAstNodePtr candidate = parent.declaration->children[child];
				if(!candidate || (candidate->kind != "class-specifier" &&
					candidate->kind != "class-forward-declaration") ||
					LastComponent(candidate->value) != nested_name) continue;
				nested_declaration = candidate;
				break;
			}
		}
		if(!nested_declaration) return;
		class_contexts_.insert(parent_local_name);
		class_contexts_.insert(parent_local_name + "::" + nested_name);
		const string key = parent_local_name + "::" + nested_name;
		if(!materialized_nested_classes_.insert(key).second) return;
		map<string, string> substitutions;
		for(size_t i = 0; i < nested_parameters.size() && i < parent_args.size(); ++i)
			if(!nested_parameters[i].name.empty() &&
				(nested_parameters[i].type || !nested_parameters[i].non_type_type.empty()))
				substitutions[nested_parameters[i].name] = parent_args[i];
		if(!nested)
			for(size_t i = 0; i < parent.parameters.size() && i < parent_args.size(); ++i)
				if(!parent.parameters[i].name.empty())
					substitutions[parent.parameters[i].name] = parent_args[i];
		substitutions[parent.name] = parent_local_name;
		const vector<const TemplateDefinition*> candidates = NestedDefinitions(parent);
		for(size_t i = 0; i < candidates.size(); ++i) {
			if(candidates[i]->name == nested_name || !nested ||
				!MentionsGeneratedType(nested_declaration, candidates[i]->name)) continue;
			InstantiateNestedClass(parent, parent_args, parent_local_name,
				candidates[i]->name, context);
		}
		const string generated_context = JoinPath(GeneratedOwner(parent),
			parent_local_name);
		const map<string, vector<string> > previous_packs = active_pack_substitutions_;
		map<string, vector<string> > parent_packs;
		if(parent.partial_specialization) {
			map<string, string> specialized;
			if(MatchClassSpecializationPattern(parent, parent_args, &specialized, context)) {
				for(map<string, string>::const_iterator binding = specialized.begin();
					binding != specialized.end(); ++binding)
					substitutions[binding->first] = binding->second;
				for(size_t pack = 0; pack < parent.specialization_pack_names.size(); ++pack) {
					const string& name = parent.specialization_pack_names[pack];
					map<string, string>::const_iterator binding = specialized.find(name);
					if(!name.empty()) {
						parent_packs[name] = binding == specialized.end() || binding->second.empty() ?
							vector<string>() : SplitTemplateArguments(binding->second);
						if(parent_packs[name].empty()) substitutions.erase(name);
						else substitutions[name] = parent_packs[name][0];
					}
				}
			}
		} else {
			size_t argument = 0;
			for(size_t parameter = 0; parameter < parent.parameters.size(); ++parameter) {
				if(parent.parameters[parameter].pack) {
					if(!parent.parameters[parameter].name.empty()) {
						vector<string>& values = parent_packs[parent.parameters[parameter].name];
						while(argument < parent_args.size()) values.push_back(parent_args[argument++]);
					} else while(argument < parent_args.size()) ++argument;
				} else if(argument < parent_args.size()) ++argument;
			}
		}
		active_pack_substitutions_ = previous_packs;
		for(map<string, vector<string> >::const_iterator pack = parent_packs.begin();
			pack != parent_packs.end(); ++pack)
			if(!pack->first.empty()) active_pack_substitutions_[pack->first] = pack->second;
		CPPGMAstNodePtr generated;
		try {
			generated = TransformNode(nested_declaration, generated_context, substitutions);
		} catch(...) {
			active_pack_substitutions_ = previous_packs;
			throw;
		}
		active_pack_substitutions_ = previous_packs;
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
		const string source_raw = raw;
		string expanded_pack;
		if(ExpandIntegralPackExpression(source_raw, context, substitutions,
			&expanded_pack)) raw = expanded_pack;
		else {
			map<string, string> expression_substitutions = substitutions;
			// Preserve the base of a template-id until RewriteText can match or
			// materialize the complete argument list.  Substituting a generated
			// class name into the base alone creates `Generated_<Args>`.
			for(map<string, string>::const_iterator substitution = substitutions.begin();
				substitution != substitutions.end(); ++substitution)
				for(size_t at = raw.find(substitution->first); at != string::npos;
					at = raw.find(substitution->first, at + substitution->first.size())) {
					if(at > 0 && IsIdentifierCharacter(raw[at - 1])) continue;
					size_t after = at + substitution->first.size();
					while(after < raw.size() && isspace(static_cast<unsigned char>(raw[after]))) ++after;
					if(after < raw.size() && raw[after] == '<') {
						expression_substitutions.erase(substitution->first);
						break;
					}
				}
			raw = ReplaceIdentifiersPreservingPackSizes(raw, expression_substitutions);
		}
		raw = RemoveMarker(RewriteText(raw, context, substitutions, 0));
		raw = ReplaceIdentifiersPreservingPackSizes(raw, substitutions);
		// When a non-type expression contains a nested template-id followed by
		// a parenthesized comparison, the compact PA10 spelling can retain the
		// enclosing template delimiter (`(expr)>`).  The delimiter is not part
		// of the integral expression once this argument is isolated.
		while(raw.size() >= 2 && raw[raw.size() - 1] == '>' &&
			raw[raw.size() - 2] == ')') raw.erase(raw.size() - 1);
		PA19IntegralValue value;
		const bool evaluated = EvaluateIntegralText(raw, context, substitutions, &value);
		if(!evaluated) {
			string details = "non-type template argument is not an integral constant: " +
				parameter.name + "=" + raw + " context=" + context + " substitutions=";
			for(map<string, string>::const_iterator it = substitutions.begin();
				it != substitutions.end(); ++it) details += it->first + "->" + it->second + ";";
			map<string, vector<string> >::const_iterator owner_names = constant_member_owners_.find(raw);
			if(owner_names != constant_member_owners_.end()) {
				details += " owners=";
				for(size_t owner = 0; owner < owner_names->second.size(); ++owner) {
					details += owner_names->second[owner] + "[" +
						(class_declarations_.find(owner_names->second[owner]) == class_declarations_.end() ?
							"no-decl" : "decl") + "];";
				}
			}
			throw logic_error(details);
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
		if((generated->kind == "class-specifier" ||
			generated->kind == "class-forward-declaration") && !generated_path.empty())
			IndexConstantMembers(generated, generated_path);
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
		active_pack_substitutions_ = previous_packs;
		// An unnamed parameter pack is only an arity constraint.  It has no
		// source spelling that can be substituted, so an empty map key must never
		// reach text replay (where it would make every `...` look expandable).
		active_pack_substitutions_.erase("");
		for(map<string, vector<string> >::const_iterator pack = pack_substitutions.begin();
			pack != pack_substitutions.end(); ++pack)
			if(!pack->first.empty()) active_pack_substitutions_[pack->first] = pack->second;
		for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter)
			if(definition.parameters[parameter].pack &&
				!definition.parameters[parameter].name.empty() &&
				active_pack_substitutions_.find(definition.parameters[parameter].name) ==
				active_pack_substitutions_.end())
				active_pack_substitutions_[definition.parameters[parameter].name] = vector<string>();
		for(size_t pack = 0; pack < definition.specialization_pack_names.size(); ++pack)
			if(!definition.specialization_pack_names[pack].empty() &&
				active_pack_substitutions_.find(definition.specialization_pack_names[pack]) ==
				active_pack_substitutions_.end())
				active_pack_substitutions_[definition.specialization_pack_names[pack]] = vector<string>();
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
	bool DeferIncompleteAliasClass(const TemplateDefinition& definition,
		const vector<string>& arguments, const string& context) const
	{
		if(definition.partial_specialization) return false;
		if(!definition.class_template || !definition.declaration ||
			definition.declaration->kind != "class-specifier" ||
			definition.declaration->children.size() <= 1) return false;
		bool has_alias = false;
		for(size_t child = 0; child < definition.declaration->children.size(); ++child) {
			const CPPGMAstNodePtr member = definition.declaration->children[child];
			if(!member || member->kind == "class-key" ||
				member->kind == "access-specifier" || member->kind == "empty-declaration") continue;
			if(member->kind == "alias-declaration") {
				has_alias = true;
				continue;
			}
			if(member->kind == "simple-declaration" && !member->children.empty() &&
				SpellNode(member->children[0]).find("typedef") != string::npos) {
				has_alias = true;
				continue;
			}
			return false;
		}
		if(!has_alias) return false;
		for(size_t argument = 0; argument < arguments.size(); ++argument) {
			const string spelling = NormalizeTypeArgument(arguments[argument]);
			const CPPGMAstNodePtr declaration = FindClassDeclaration(spelling, context);
			if(declaration && declaration->kind == "class-forward-declaration") {
				map<string, string>::const_iterator generated = specialization_bases_.find(
					LastComponent(spelling));
				const TemplateDefinition* primary = generated == specialization_bases_.end() ?
					0 : FindDefinition(generated->second, context);
				const bool complete_primary = primary && primary->declaration &&
					primary->declaration->kind == "class-specifier" &&
					primary->declaration->children.size() > 1;
				if(!complete_primary) return true;
			}
		}
		return false;
	}
	string NormalizeTemplateTemplateArgument(string raw, const string& context,
		const map<string, string>& substitutions) const
	{
		raw = CanonicalSpelling(ReplaceIdentifiers(raw, substitutions));
		if(raw.compare(0, 9, "template ") == 0)
			raw = CanonicalSpelling(raw.substr(9));
		for(size_t position = raw.find("::template "); position != string::npos;
			position = raw.find("::template ", position + 2))
			raw.erase(position + 2, 9);
		const TemplateDefinition* definition = FindDefinition(raw, context);
		if(!definition) return string();
		// Keep the concrete owner on a member template entity.  The registry
		// definition is keyed by its source owner (`quote::quote::fn`), but a
		// template-template argument such as `quote<X>::fn` also carries the
		// specialization that binds the outer `F` parameter.  Canonicalizing it
		// to the source member alone would make that binding unrecoverable when
		// the alias is instantiated later.
		const size_t owner_separator = raw.rfind("::");
		if(owner_separator != string::npos) {
			const string owner = raw.substr(0, owner_separator);
			if(specialization_bases_.find(LastComponent(owner)) !=
				specialization_bases_.end() &&
				specialization_arguments_.find(LastComponent(owner)) !=
				specialization_arguments_.end()) return raw;
		}
		return definition->qualified_name;
	}
	bool CompatibleTemplateParameter(const TemplateParameter& expected,
		const TemplateParameter& actual) const
	{
		if(expected.type != actual.type) return false;
		if(expected.template_template != actual.template_template) return false;
		if(!expected.type) {
			const string left = NormalizeTypeArgument(expected.non_type_type);
			const string right = NormalizeTypeArgument(actual.non_type_type);
			if(!left.empty() && !right.empty() && left != right) return false;
		}
		if(expected.template_template && !CompatibleTemplateParameterList(
			expected.template_parameters, actual.template_parameters)) return false;
		return true;
	}
	bool CompatibleTemplateParameterList(const vector<TemplateParameter>& expected,
		const vector<TemplateParameter>& actual) const
	{
		size_t expected_index = 0;
		size_t actual_index = 0;
		while(expected_index < expected.size()) {
			const TemplateParameter& wanted = expected[expected_index];
			if(wanted.pack) {
				const TemplateParameter* element = wanted.template_parameters.empty() ?
					&wanted : &wanted.template_parameters[0];
				while(actual_index < actual.size()) {
					if(!CompatibleTemplateParameter(*element, actual[actual_index])) return false;
					++actual_index;
				}
				return true;
			}
			if(actual_index >= actual.size()) return false;
			if(actual[actual_index].pack) {
				// A candidate pack can supply every remaining fixed parameter when
				// the parameter kinds agree.
				for(size_t remaining = expected_index; remaining < expected.size(); ++remaining)
					if(!CompatibleTemplateParameter(expected[remaining], actual[actual_index]))
						return false;
				return true;
			}
			if(!CompatibleTemplateParameter(wanted, actual[actual_index])) return false;
			++expected_index;
			++actual_index;
		}
		for(; actual_index < actual.size(); ++actual_index)
			if(!actual[actual_index].pack && actual[actual_index].default_type.empty()) return false;
		return true;
	}
	bool CompatibleTemplateTemplateArgument(const TemplateParameter& parameter,
		const string& raw, const string& context,
		const map<string, string>& substitutions, string* normalized) const
	{
		const string argument = NormalizeTemplateTemplateArgument(raw, context, substitutions);
		if(argument.empty()) return false;
		const TemplateDefinition* definition = FindDefinition(argument, context);
		if(!definition || (!definition->class_template && !definition->alias_template &&
			!definition->variable_template)) return false;
		if(!CompatibleTemplateParameterList(parameter.template_parameters,
			definition->parameters)) return false;
		if(normalized) *normalized = argument;
		return true;
	}
	bool HasDependentVariableTemplate(const string& raw, const string& context,
		const map<string, string>& substitutions) const
	{
		const string resolved = CanonicalSpelling(
			ReplaceIdentifiersPreservingPackSizes(raw, substitutions));
		for(size_t search = 0; search < resolved.size(); ++search) {
			if(resolved[search] != '<') continue;
			size_t begin = 0, close = string::npos;
			string base, arguments;
			if(!TemplateBase(resolved, search, &begin, &base) ||
				!TemplateRange(resolved, search, &arguments, &close)) continue;
			const TemplateDefinition* definition = FindDefinition(base, context);
			if(definition && definition->variable_template) return true;
			search = close;
		}
		return false;
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
				if(pack_hints && !parameter.name.empty()) {
					map<string, vector<string> >::const_iterator hint =
						pack_hints->find(parameter.name);
					if(hint != pack_hints->end()) count = hint->second.size();
				}
				for(size_t element = 0; element < count; ++element) {
					string argument = raw_args[raw_index++];
					PA19IntegralValue integral_value;
					if(parameter.template_template) {
						string normalized;
						if(!CompatibleTemplateTemplateArgument(parameter, argument, context,
							*substitutions, &normalized))
							throw logic_error("template-template argument does not match");
						argument = normalized;
					} else if(parameter.type) {
						argument = RewriteText(argument, context, *substitutions, 0);
						argument = NormalizeTypeArgument(argument);
						argument = NormalizeTypeArgument(ReplaceIdentifiers(argument, *substitutions));
						argument = ResolveAlias(argument, context);
						argument = RewriteText(argument, context, *substitutions, 0);
						argument = NormalizeTypeArgument(argument);
						argument = QualifyTypeArgument(argument, context, definition.owner);
					} else {
						try {
							argument = ResolveIntegralArgument(parameter, argument, context,
								*substitutions, &integral_value);
						} catch(const logic_error& error) {
							throw logic_error("definition=" + definition.qualified_name +
								" " + error.what());
						}
						if(!parameter.name.empty())
							(*integral_substitutions)[parameter.name] = integral_value;
					}
				if(argument.empty()) throw logic_error("missing template argument");
					values.push_back(argument);
					args->push_back(argument);
					metadata_args->push_back(TemplateArgumentMetadata(parameter, argument,
						integral_value, context, *substitutions));
				}
				if(!parameter.name.empty()) {
					if(pack_substitutions) (*pack_substitutions)[parameter.name] = values;
					if(!values.empty()) (*substitutions)[parameter.name] = values[0];
					else substitutions->erase(parameter.name);
				}
				continue;
			}
			string argument;
			PA19IntegralValue integral_value;
			if(raw_index < raw_args.size() && !raw_args[raw_index].empty())
				argument = raw_args[raw_index++];
			else {
				if(!parameter.name.empty()) {
					map<string, string>::const_iterator substituted = substitutions->find(parameter.name);
					if(substituted != substitutions->end()) argument = substituted->second;
					map<string, PA19IntegralValue>::const_iterator integral =
						integral_substitutions->find(parameter.name);
					if(argument.empty() && integral != integral_substitutions->end())
						argument = TemplateIntegralValueSpelling(integral->second);
				}
			}
			if(argument.empty()) argument = parameter.default_type;
			if(parameter.template_template) {
				string normalized;
				if(!CompatibleTemplateTemplateArgument(parameter, argument, context,
					*substitutions, &normalized))
					throw logic_error("template-template argument does not match");
				argument = normalized;
			} else if(parameter.type) {
				argument = ExpandPackCallText(argument, *pack_substitutions);
				argument = RewriteText(argument, context, *substitutions, 0);
				argument = NormalizeTypeArgument(argument);
				argument = NormalizeTypeArgument(ReplaceIdentifiers(argument, *substitutions));
				argument = ResolveAlias(argument, context);
				argument = RewriteText(argument, context, *substitutions, 0);
				argument = NormalizeTypeArgument(argument);
				argument = QualifyTypeArgument(argument, context, definition.owner);
			} else {
				try {
					argument = ResolveIntegralArgument(parameter, argument, context, *substitutions,
						&integral_value);
				} catch(const logic_error& error) {
					throw logic_error("definition=" + definition.qualified_name +
						" " + error.what());
				}
				if(!parameter.name.empty())
					(*integral_substitutions)[parameter.name] = integral_value;
			}
			if(argument.empty()) throw logic_error("missing template argument");
			args->push_back(argument);
			metadata_args->push_back(TemplateArgumentMetadata(parameter, argument,
				integral_value, context, *substitutions));
			if(!parameter.name.empty()) (*substitutions)[parameter.name] = argument;
		}
		if(raw_index != raw_args.size())
			throw logic_error("too many template arguments");
	}
	string GeneratedSpecializationName(const TemplateDefinition& definition,
		const vector<string>& args, const vector<string>& metadata_args,
		const map<string, string>& substitutions, const string& context,
		const string& concrete_owner)
	{
		string local_name = definition.name;
		if(definition.class_template || definition.alias_template || definition.variable_template) {
			// An empty argument list is a real specialization for a parameter-pack
			// template.  Keep its generated entity distinct from the primary name;
			// otherwise `list<>` is rewritten to `list`, and a later pass treats the
			// primary template name as another template-id and recursively
			// materializes the same empty specialization.
			if(args.empty()) local_name += "_";
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
					const string argument_spelling = enum_argument ? metadata_args[i] : args[i];
					local_name += TypeSuffix(argument_spelling);
					// Function types and object types can otherwise collapse to the
					// same identifier suffix (`const int` vs `const int()`).
					if(definition.class_template && i < definition.parameters.size() &&
						definition.parameters[i].type &&
						argument_spelling.find('(') != string::npos)
						local_name += "_fn";
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
		// A member alias template is cached by its source definition and its
		// own arguments, but its body can also depend on the concrete enclosing
		// specialization.  Give those owner-bound aliases distinct entities so
		// `allocator_traits<standard_allocator<void>>::rebind_traits<double>`
		// cannot reuse the alias materialized for
		// `allocator_traits<simple_allocator<int>>::rebind_traits<double>`.
		if(definition.alias_template && !concrete_owner.empty())
			local_name += "__" + TypeSuffix(concrete_owner);
		return local_name;
	}
	string Instantiate(const TemplateDefinition& definition, const vector<string>& raw_args,
		const string& context, bool explicit_instantiation = false,
		const map<string, vector<string> >* pack_hints = 0,
		const map<string, string>* outer_substitutions = 0,
		const string* concrete_owner = 0);
	string MaterializeInstantiation(const TemplateDefinition& definition,
		const vector<string>& args, const vector<string>& metadata_args,
		map<string, string> substitutions,
		const map<string, PA19IntegralValue>& integral_substitutions,
		const map<string, vector<string> >& pack_substitutions,
		const string& context, bool explicit_instantiation, const string& key,
		const string& concrete_owner);
	void ReplayCachedInstantiation(const TemplateDefinition& definition,
		const vector<string>& args, const string& cached, const string& context,
		bool explicit_instantiation,
		const map<string, vector<string> >& pack_substitutions);
	void RegisterGeneratedSpecialization(const TemplateDefinition& definition,
		const vector<string>& metadata_args, const string& local_name);
	void AddConcreteOwnerSubstitutions(const string& concrete_owner,
		const string& context, map<string, string>* substitutions);
	bool ConcreteOwnerMatches(const TemplateDefinition& definition,
		const string& concrete_owner) const;
	string FindConcreteInstantiationOwner(const TemplateDefinition& definition,
			const map<string, string>& substitutions, const string& context,
			const string& requested_owner) const;
	string EmitInstantiation(const TemplateDefinition& definition,
		const vector<string>& args, const vector<string>& metadata_args,
		map<string, string> substitutions,
		const map<string, PA19IntegralValue>& integral_substitutions,
		const map<string, vector<string> >& pack_substitutions,
		const string& context, bool explicit_instantiation, const string& key,
		const string& local_name, const string& requested_owner);
