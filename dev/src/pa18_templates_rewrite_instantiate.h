#pragma once
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
			if(candidate.class_template || candidate.declaration->kind !=
				"simple-declaration") continue;
			const size_t angle = candidate.owner.find('<');
			const size_t close = angle == string::npos ? string::npos :
				candidate.owner.find('>', angle);
			if(angle != string::npos && close != string::npos &&
				candidate.owner.substr(0, angle) == parent.qualified_name &&
				candidate.owner.find("::", close) == string::npos)
				result.push_back(&candidate);
		}
		return result;
	}
	void InstantiateMemberDefinitions(const TemplateDefinition& parent,
		const vector<string>& parent_args, const string& parent_local_name)
	{
		const vector<const TemplateDefinition*> members = MemberDefinitions(parent);
		for(size_t i = 0; i < members.size(); ++i) {
			const TemplateDefinition& member = *members[i];
			const string key = member.qualified_name + "@" + parent_local_name;
			if(!materialized_member_definitions_.insert(key).second) continue;
			map<string, string> substitutions;
			for(size_t parameter = 0; parameter < member.parameters.size() &&
				parameter < parent_args.size(); ++parameter)
				if(member.parameters[parameter].type)
					substitutions[member.parameters[parameter].name] = parent_args[parameter];
			substitutions[parent.name] = parent_local_name;
			CPPGMAstNodePtr generated = TransformNode(member.declaration,
				parent_local_name, substitutions);
			if(!generated) continue;
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
			if(nested->parameters[i].type)
				substitutions[nested->parameters[i].name] = parent_args[i];
		substitutions[parent.name] = parent_local_name;
		const vector<const TemplateDefinition*> candidates = NestedDefinitions(parent);
		for(size_t i = 0; i < candidates.size(); ++i) {
			if(candidates[i]->name == nested_name ||
				!MentionsGeneratedType(nested->declaration, candidates[i]->name)) continue;
			InstantiateNestedClass(parent, parent_args, parent_local_name,
				candidates[i]->name, context);
		}
		CPPGMAstNodePtr generated = TransformNode(nested->declaration,
			parent_local_name, substitutions);
		if(!generated) return;
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
			argument = ResolveAlias(argument, context);
			argument = RewriteText(argument, context, substitutions, 0);
			argument = NormalizeTypeArgument(argument);
			argument = QualifyTypeArgument(argument, context, definition.owner);
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
		if(cached != specializations_.end()) {
			if(definition.class_template) {
				InstantiateRequestedNestedClasses(definition, args, cached->second, context);
				InstantiateMemberDefinitions(definition, args, cached->second);
			}
			return cached->second;
		}
		string local_name = definition.name;
		if(definition.class_template || definition.alias_template) {
			for(size_t i = 0; i < args.size(); ++i) {
				local_name += i == 0 ? "_" : "__";
				local_name += TypeSuffix(args[i]);
			}
		}
	const string generated_owner = definition.lexical_owner.empty() ?
		definition.owner : definition.lexical_owner;
	if(definition.class_template) {
		specialization_bases_[local_name] = definition.qualified_name;
		specialization_arguments_[local_name] = args;
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
		CPPGMAstNodePtr generated = TransformNode(definition.declaration, definition.owner, substitutions);
		if(!generated) throw logic_error("unable to instantiate template");
		if(definition.class_template || definition.alias_template) generated->value = local_name;
		if(definition.class_template) {
			const string generated_path = JoinPath(definition.owner, local_name);
			class_declarations_[generated_path] = generated;
			const string lexical_path = JoinPath(
				definition.lexical_owner.empty() ? definition.owner : definition.lexical_owner,
				local_name);
			class_declarations_[lexical_path] = generated;
			class_contexts_.insert(generated_path);
			InstantiateRequestedNestedClasses(definition, args, local_name, context);
			InstantiateMemberDefinitions(definition, args, local_name);
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
