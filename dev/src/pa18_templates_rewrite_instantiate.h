#pragma once
	string NodeTypeSpelling(const CPPGMAstNodePtr& sequence) const
	{
		if(!sequence) return string();
		string result;
		for(size_t i = 0; i < sequence->children.size(); ++i) {
			const CPPGMAstNodePtr child = sequence->children[i];
			if(!child || (child->kind == "decl-specifier" &&
				(child->value == "KW_TYPEDEF:typedef" || child->value == "KW_STATIC:static"))) continue;
			if(child->kind != "decl-specifier" && child->kind != "type-name" &&
				child->kind != "type-specifier" && child->kind != "cv-qualifier") continue;
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
	bool EvaluateIntegralText(string raw, const string& context,
		const map<string, string>& substitutions, PA19IntegralValue* result) const
	{
		raw = CanonicalSpelling(ReplaceIdentifiers(raw, substitutions));
		PA19ConstantExpressionParser parser(constant_values_, substitutions,
			constant_type_sizes_, constant_type_alignments_, type_aliases_);
		if(parser.Evaluate(raw, result)) return true;
		if(raw.find("::") == string::npos && !context.empty()) {
			for(string current = context; ; ) {
				const string qualified = JoinPath(current, raw);
				if(parser.Evaluate(qualified, result)) return true;
				const size_t separator = current.rfind("::");
				if(separator == string::npos) break;
				current.erase(separator);
			}
		}
		return false;
	}
	void RecordConstantDeclaration(const CPPGMAstNodePtr& node, const string& context)
	{
		if(!node || node->kind != "simple-declaration" || node->children.empty()) return;
		const string specifiers = SpellNode(node->children[0]);
		if(specifiers.find("const") == string::npos && specifiers.find("constexpr") == string::npos) return;
		const string base_type = NodeTypeSpelling(node->children[0]);
		if(!PA19Type(base_type).integral) return;
		const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
		if(!list) return;
		for(size_t i = 0; i < list->children.size(); ++i) {
			const CPPGMAstNodePtr item = list->children[i];
			if(!item || item->children.size() < 2) continue;
			const string name = FirstIdentifierLocal(item->children[0]);
			const CPPGMAstNodePtr initializer = item->children[1];
			if(name.empty() || !initializer || initializer->children.empty()) continue;
			PA19IntegralValue value;
			if(!EvaluateIntegralText(SpellNode(initializer->children[0]), context,
				map<string,string>(), &value)) continue;
			const string qualified = JoinPath(context, name);
			constant_values_[qualified] = value;
			if(constant_values_.find(name) == constant_values_.end()) constant_values_[name] = value;
			const PA19IntegralType type = PA19Type(base_type);
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
				EvaluateIntegralText(SpellNode(enumerator->children[0]), context,
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
			for(size_t parameter = 0; parameter < member.parameters.size() &&
				parameter < parent_args.size(); ++parameter)
				if(member.parameters[parameter].type || !member.parameters[parameter].non_type_type.empty())
					substitutions[member.parameters[parameter].name] = parent_args[parameter];
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
			CPPGMAstNodePtr generated = TransformNode(member.declaration,
				generated_context, substitutions);
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
		raw = RewriteText(raw, context, substitutions, 0);
		raw = ReplaceIdentifiers(raw, substitutions);
		PA19IntegralValue value;
		if(!EvaluateIntegralText(raw, context, substitutions, &value))
			throw logic_error("non-type template argument is not an integral constant");
		string expected = RewriteText(parameter.non_type_type, context, substitutions, 0);
		expected = ResolveAlias(ReplaceIdentifiers(expected, substitutions), context);
		const PA19IntegralType expected_type = PA19Type(expected);
		if(expected_type.integral) value = PA19Convert(value, expected_type);
		if(typed_result) *typed_result = value;
		return IntegralValueSpelling(value);
	}
	void RegisterGeneratedConstants(
		const CPPGMAstNodePtr& generated, const string& generated_path)
	{
		if(!generated) return;
		if(generated->kind == "simple-declaration")
			RecordConstantDeclaration(generated, generated_path);
		if(generated->kind == "enum-specifier")
			RecordEnumConstants(generated, generated_path);
		for(size_t i = 0; i < generated->children.size(); ++i)
			if(generated->children[i] && generated->children[i]->kind != "compound-statement")
				RegisterGeneratedConstants(generated->children[i],
					generated->kind == "class-specifier" || generated->kind == "class-forward-declaration" ?
					JoinPath(generated_path, LastComponent(generated->value)) : generated_path);
	}
	CPPGMAstNodePtr TransformInstantiatedNode(const TemplateDefinition& definition,
		const string& context, const map<string, string>& substitutions,
		const map<string, PA19IntegralValue>& integral_substitutions)
	{
		const map<string, PA19IntegralValue> previous = active_integral_substitutions_;
		active_integral_substitutions_ = integral_substitutions;
		try {
			CPPGMAstNodePtr result = TransformNode(definition.declaration, context, substitutions);
			active_integral_substitutions_ = previous;
			return result;
		} catch(...) {
			active_integral_substitutions_ = previous;
			throw;
		}
	}
	void ResolveTemplateArguments(const TemplateDefinition& definition,
		const vector<string>& raw_args, const string& context,
		vector<string>* args, vector<string>* metadata_args,
		map<string, string>* substitutions,
		map<string, PA19IntegralValue>* integral_substitutions)
	{
		for(size_t i = 0; i < definition.parameters.size(); ++i) {
			const TemplateParameter& parameter = definition.parameters[i];
			string argument;
			if(i < raw_args.size() && !raw_args[i].empty()) argument = raw_args[i];
			else {
				map<string, string>::const_iterator substituted = substitutions->find(parameter.name);
				if(substituted != substitutions->end()) argument = substituted->second;
				map<string, PA19IntegralValue>::const_iterator integral =
					integral_substitutions->find(parameter.name);
				if(argument.empty() && integral != integral_substitutions->end())
					argument = IntegralValueSpelling(integral->second);
			}
			if(argument.empty()) argument = parameter.default_type;
			if(parameter.type) {
				argument = RewriteText(argument, context, *substitutions, 0);
				argument = NormalizeTypeArgument(argument);
				argument = NormalizeTypeArgument(ReplaceIdentifiers(argument, *substitutions));
				argument = ResolveAlias(argument, context);
				argument = RewriteText(argument, context, *substitutions, 0);
				argument = NormalizeTypeArgument(argument);
				argument = QualifyTypeArgument(argument, context, definition.owner);
			} else {
				PA19IntegralValue integral_value;
				argument = ResolveIntegralArgument(parameter, argument, context, *substitutions,
					&integral_value);
				(*integral_substitutions)[parameter.name] = integral_value;
			}
			if(argument.empty()) throw logic_error("missing template argument");
			args->push_back(argument);
			metadata_args->push_back(RestoreSpecializationSpelling(argument));
			(*substitutions)[parameter.name] = argument;
		}
		if(raw_args.size() > definition.parameters.size())
			throw logic_error("too many template arguments");
	}
	string Instantiate(const TemplateDefinition& definition, const vector<string>& raw_args,
		const string& context, bool explicit_instantiation = false)
	{
		if(definition.parameters.empty()) throw logic_error("template has no type parameters");
		vector<string> args, metadata_args;
		map<string, string> substitutions;
		map<string, PA19IntegralValue> integral_substitutions;
		ResolveTemplateArguments(definition, raw_args, context, &args, &metadata_args,
			&substitutions, &integral_substitutions);
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
		string local_name = definition.name;
		if(definition.class_template || definition.alias_template) {
			for(size_t i = 0; i < args.size(); ++i) {
				local_name += i == 0 ? "_" : "__";
				local_name += TypeSuffix(args[i]);
				if(i + 1 == args.size()) local_name += i == 0 ? "_" : "__";
			}
		} else if(definition.name.compare(0, 8, "operator") != 0) {
		// Keep each concrete function specialization distinct in its generated AST.
			for(size_t i = 0; i < args.size(); ++i) {
				local_name += i == 0 ? "__inst_" : "__";
				local_name += TypeSuffix(args[i]);
			}
		}
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
			definition.owner, substitutions, integral_substitutions);
		if(!generated) throw logic_error("unable to instantiate template");
		MarkGeneratedNode(generated, definition.qualified_name, metadata_args,
			explicit_instantiation);
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
