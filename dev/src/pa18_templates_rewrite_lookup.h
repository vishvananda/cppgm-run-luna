#pragma once
	bool IsOrdinaryTemplateUsingTarget(const string& raw_target,
		const string& context) const
	{
		if(LastComponent(raw_target).compare(0, 8, "operator") == 0) return false;
		const TemplateDefinition* direct = FindDefinition(raw_target, context);
		if(direct && !direct->class_template) return true;
		const string suffix = "::" + raw_target;
		map<string, vector<string> >::const_iterator indexed =
			definitions_by_name_.find(LastComponent(raw_target));
		if(indexed == definitions_by_name_.end()) return false;
		for(size_t i = 0; i < indexed->second.size(); ++i) {
			map<string, TemplateDefinition>::const_iterator it =
				definitions_.find(indexed->second[i]);
			if(it == definitions_.end() || it->second.class_template) continue;
			if(it->second.qualified_name == raw_target ||
				(it->second.qualified_name.size() > suffix.size() &&
				 it->second.qualified_name.compare(it->second.qualified_name.size() - suffix.size(),
				 suffix.size(), suffix) == 0)) return true;
		}
		return false;
	}
	bool IsConstructorUsingTarget(const string& raw_target) const { const size_t separator = raw_target.rfind("::"); if(separator == string::npos) return false; string owner = raw_target.substr(0, separator); const size_t open = owner.find('<'); if(open != string::npos) owner.erase(open); return LastComponent(owner) == LastComponent(raw_target); }
	bool IsGeneratedMemberTemplateUsingTarget(const string& raw_target,
		const string& context, const map<string, string>& substitutions) const
	{
		const size_t separator = raw_target.rfind("::");
		if(separator == string::npos) return false;
		string owner = CanonicalSpelling(ReplaceIdentifiers(
			raw_target.substr(0, separator), substitutions));
		owner = CanonicalSpelling(ResolveAlias(owner, context));
		const string member = LastComponent(raw_target);
		map<string, string>::const_iterator generated = specialization_bases_.find(
			LastComponent(owner));
		if(generated == specialization_bases_.end()) return false;
		const string source_owner = LastComponent(generated->second);
		map<string, vector<string> >::const_iterator indexed_members =
			definitions_by_name_.find(member);
		if(indexed_members == definitions_by_name_.end()) return false;
		for(size_t indexed = 0; indexed < indexed_members->second.size(); ++indexed) {
			map<string, TemplateDefinition>::const_iterator it = definitions_.find(
				indexed_members->second[indexed]);
			if(it == definitions_.end()) continue;
			const TemplateDefinition& definition = it->second;
			if(definition.class_template || definition.alias_template ||
				definition.variable_template || definition.parameters.empty() ||
				LastComponent(definition.name) != member) continue;
			string definition_owner = definition.owner;
			const size_t angle = definition_owner.find('<');
			if(angle != string::npos) definition_owner.erase(angle);
			if(LastComponent(definition_owner) == source_owner) return true;
		}
		return false;
	}
	string GeneratedFunctionQualifier(const TemplateDefinition& definition,
		const string& raw_callee, const string& context) const
	{
		string qualifier = PrefixComponent(raw_callee);
		if(!qualifier.empty()) return qualifier;
		// A friend function template is declared lexically inside its class but
		// belongs to the surrounding namespace.  The generated declaration is
		// collected with the class as its replay owner so it can see the class's
		// private state; its call spelling must nevertheless remain unqualified
		// here, allowing PA14's friend-aware collection to assign the namespace
		// qualified symbol.
		if(definition.friend_declaration)
			return string();
		const string owner = GeneratedOwner(definition);
		bool visible = owner.empty() || context == owner ||
			(context.size() > owner.size() && context.compare(0, owner.size(), owner) == 0 &&
			 context[owner.size()] == ':');
		const string current = PrefixComponent(context);
		map<string, string>::const_iterator specialized = specialization_bases_.find(LastComponent(current));
		if(specialized != specialization_bases_.end() && specialized->second == owner) visible = true;
		return visible ? string() : owner;
	}

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
		// PA10 may preserve adjacent closing template delimiters as `>>`.
		// IsTemplateAngleClose intentionally rejects some `>>` spellings that
		// could be a shift expression; inside a known nested template range, a
		// consecutive run followed by a delimiter or cv/function qualifier is a
		// closure run instead.  Recognize the whole run so both delimiters update
		// the nesting depth and the outer text does not retain a stray `>`.
		const auto nested_close_run = [&raw](size_t position) {
			if(position >= raw.size() || raw[position] != '>') return false;
			size_t begin = position;
			while(begin > 0 && raw[begin - 1] == '>') --begin;
			size_t end = position;
			while(end + 1 < raw.size() && raw[end + 1] == '>') ++end;
			if(end == begin) return false;
			size_t after = end + 1;
			while(after < raw.size() && isspace(static_cast<unsigned char>(raw[after]))) ++after;
			if(after == raw.size()) return true;
			if(string(",;):]}&*:=").find(raw[after]) != string::npos) return true;
			if(!isalpha(static_cast<unsigned char>(raw[after])) && raw[after] != '_')
				return false;
			size_t word_end = after + 1;
			while(word_end < raw.size() && IsIdentifierCharacter(raw[word_end])) ++word_end;
			const string word = raw.substr(after, word_end - after);
			return word == "const" || word == "volatile" || word == "noexcept";
		};
		int depth = 0;
		int parentheses = 0;
		int brackets = 0;
		vector<int> angle_parentheses;
		for(size_t i = open; i < raw.size(); ++i) {
			if(raw[i] == '(') ++parentheses;
			else if(raw[i] == ')' && parentheses > 0) --parentheses;
			else if(raw[i] == '[') ++brackets;
			else if(raw[i] == ']' && brackets > 0) --brackets;
			if(raw[i] == '<' && IsTemplateAngleOpen(raw, i)) {
				++depth;
				angle_parentheses.push_back(parentheses);
		} else if(raw[i] == '>' && (IsTemplateAngleClose(raw, i) ||
			(depth > 0 && nested_close_run(i)))) {
				const int opener_parentheses = angle_parentheses.empty() ? 0 :
					angle_parentheses.back();
				if(parentheses > opener_parentheses) continue;
				--depth;
				if(!angle_parentheses.empty()) angle_parentheses.pop_back();
				if(depth == 0) {
					if(arguments) *arguments = raw.substr(open + 1, i - open - 1);
					if(close_out) *close_out = i;
					return true;
				}
			}
		}
		// PA10's declaration spelling can omit the final `>` when a
		// less-than expression is used as the first non-type argument.  The
		// source parser has nevertheless preserved the complete argument text;
		// treat the end of that spelling as the template range so PA18 can
		// materialize the declaration normally.
		if(depth == 1 && open < raw.size()) {
			if(arguments) *arguments = raw.substr(open + 1);
			if(close_out) *close_out = raw.size() - 1;
			return true;
		}
		return false;
	}

	bool TemplateBase(const string& raw, size_t open, size_t* begin, string* base) const
	{
		const size_t literal_operator = raw.rfind("operator\"\"", open);
		if(literal_operator != string::npos && literal_operator + 10 <= open) {
			bool suffix = literal_operator + 10 < open;
			for(size_t i = literal_operator + 10; i < open; ++i)
				if(!IsIdentifierCharacter(raw[i])) suffix = false;
			if(suffix) {
				if(begin) *begin = literal_operator;
				if(base) *base = raw.substr(literal_operator, open - literal_operator);
				return true;
			}
		}
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
			// In-class member templates are sometimes collected through the
			// class-definition context (`Owner::Owner`) even though lookup starts
			// from the ordinary class scope (`Owner`).  Keep that typed owner alias
			// visible for dependent calls such as `check<F>(0)`.
			if(!current.empty()) {
				const string repeated = JoinPath(current, JoinPath(current, raw_name));
				found = definitions_.find(repeated);
				if(found != definitions_.end()) return &found->second;
				if(LastComponent(current) != LastComponent(raw_name)) {
					const string normalized_repeated = JoinPath(current,
						JoinPath(LastComponent(current), raw_name));
					found = definitions_.find(normalized_repeated);
					if(found != definitions_.end()) return &found->second;
				}
			}
			const size_t separator = current.rfind("::");
			if(separator == string::npos) break;
			current.erase(separator);
		}
		// An explicit using-declaration is an unambiguous typed import even when
		// other definitions share the same short name.  Search the current scope
		// and its enclosing scopes before applying the conservative short-name
		// fallback below.  Function-template imports remain on the ordinary call
		// lookup path; this table is only a type/alias lookup aid.
		for(string current = context; ; ) {
			map<string, vector<const TemplateDefinition*> >::const_iterator imports =
				using_declaration_targets_.find(current);
			if(imports != using_declaration_targets_.end()) {
				const TemplateDefinition* imported = 0;
				for(size_t index = 0; index < imports->second.size(); ++index) {
					const TemplateDefinition* candidate = imports->second[index];
					if(!candidate || candidate->name != LastComponent(raw_name)) continue;
					if(imported && imported != candidate) {
						imported = 0;
						break;
					}
					imported = candidate;
				}
				if(imported) return imported;
			}
			if(current.empty()) break;
			const size_t separator = current.rfind("::");
			if(separator == string::npos) current.clear();
			else current.erase(separator);
		}
		map<string, vector<string> >::const_iterator by_name = definitions_by_name_.find(LastComponent(raw_name));
		if(by_name != definitions_by_name_.end() && by_name->second.size() == 1) {
			map<string, TemplateDefinition>::const_iterator found = definitions_.find(by_name->second[0]);
			if(found != definitions_.end()) return &found->second;
		}
		// Unqualified lookup from a logical namespace must also see entities
		// collected under an inline namespace's physical path.  The ordinary
		// candidate walk above only probes `lib::basic_json`, while collection
		// intentionally stores the declaration as `lib::abi::basic_json` so its
		// emitted owner remains stable.  Recover that relationship from the
		// typed lexical namespace map instead of inventing a second definition.
		string logical_context = context;
		for(string current = context; ; ) {
			map<string, string>::const_iterator logical = lexical_namespace_logical_.find(current);
			if(logical != lexical_namespace_logical_.end()) {
				logical_context = logical->second;
				break;
			}
			if(current.empty()) break;
			const size_t separator = current.rfind("::");
			if(separator == string::npos) current.clear();
			else current.erase(separator);
		}
		if(raw_name.find("::") == string::npos && !logical_context.empty()) {
			const TemplateDefinition* logical_match = 0;
			for(map<string, TemplateDefinition>::const_iterator it = definitions_.begin();
				it != definitions_.end(); ++it) {
				if(LastComponent(it->second.qualified_name) != raw_name) continue;
				const string physical_owner = PrefixComponent(it->second.qualified_name);
				map<string, string>::const_iterator logical = lexical_namespace_logical_.find(
					physical_owner);
				if(logical == lexical_namespace_logical_.end() ||
					logical->second != logical_context) continue;
				if(logical_match && logical_match->qualified_name != it->second.qualified_name)
					return 0;
				logical_match = &it->second;
			}
			if(logical_match) return logical_match;
		}
		// An inline namespace contributes its declarations to the enclosing
		// namespace for lookup.  Collection keeps the physical namespace in the
		// typed definition key (`lib::abi::map`) so generated declarations remain
		// deterministic; resolve a logical spelling such as `lib::map` against
		// that physical key here instead of duplicating the entity.
		const size_t raw_separator = TopLevelScopeSeparator(raw_name);
		if(raw_separator != string::npos) {
			const string logical_owner = raw_name.substr(0, raw_separator);
			const string logical_name = raw_name.substr(raw_separator + 2);
			const TemplateDefinition* logical_match = 0;
			for(map<string, TemplateDefinition>::const_iterator it = definitions_.begin();
				it != definitions_.end(); ++it) {
				if(LastComponent(it->second.qualified_name) != logical_name) continue;
				const string physical_owner = PrefixComponent(it->second.qualified_name);
				map<string, string>::const_iterator logical =
					lexical_namespace_logical_.find(physical_owner);
				if(logical == lexical_namespace_logical_.end() ||
					logical->second != logical_owner) continue;
				if(logical_match) return 0;
				logical_match = &it->second;
			}
			if(logical_match) return logical_match;
		}
		return 0;
	}
	const TemplateDefinition* FindExplicitFunctionSpecialization(
		const string& raw_name, const vector<string>& arguments,
		const string& context) const
	{
		const TemplateDefinition* primary = FindDefinition(raw_name, context);
		if(!primary || primary->class_template) return 0;
		map<string, TemplateDefinition>::const_iterator found =
			explicit_function_specializations_.find(
				PA18ExplicitSpecializationKey(primary->qualified_name, arguments));
		return found == explicit_function_specializations_.end() ? 0 : &found->second;
	}

	vector<const TemplateDefinition*> FindFunctionDefinitions(string raw_name,
		const string& context) const
	{
		vector<const TemplateDefinition*> result;
		while(!raw_name.empty() && raw_name[0] == ':') raw_name.erase(0, 1);
		set<string> candidates;
		for(string current = context; ; ) {
			candidates.insert(JoinPath(current, raw_name));
			// Member templates are collected with the class name repeated in their
			// owner (`scope::Class::test`), while an unevaluated call inside the
			// class is looked up from `scope::Class`.  Add that member scope before
			// falling back to unrelated same-named namespace overloads.
			const TemplateDefinition* current_class = FindDefinition(
				LastComponent(current), PrefixComponent(current));
			if(class_contexts_.find(current) != class_contexts_.end() ||
				(current_class && current_class->class_template &&
					current_class->qualified_name == current))
				candidates.insert(JoinPath(current,
					JoinPath(LastComponent(current), raw_name)));
			if(!current.empty()) candidates.insert(JoinPath(current,
				JoinPath(current, raw_name)));
			if(current.empty()) break;
			const size_t separator = current.rfind("::");
			if(separator == string::npos) current.clear();
			else current.erase(separator);
		}
	// A replayed class body keeps its concrete generated owner in `context`,
	// while member function templates remain indexed under the source class
	// owner.  Carry that typed specialization mapping into lookup so unevaluated
	// calls such as `sizeof(test<T>(0))` see the source overload set.
	map<string, string>::const_iterator generated_owner =
		specialization_bases_.find(LastComponent(context));
	if(generated_owner != specialization_bases_.end()) {
		string source_owner = generated_owner->second;
		const size_t source_open = source_owner.find('<');
		if(source_open != string::npos) source_owner.erase(source_open);
		if(!source_owner.empty()) {
			candidates.insert(JoinPath(source_owner, raw_name));
			candidates.insert(JoinPath(source_owner,
				JoinPath(LastComponent(source_owner), raw_name)));
		}
	}
	candidates.insert(raw_name);
		const string short_name = LastComponent(raw_name);
		map<string, vector<string> >::const_iterator indexed =
			definitions_by_name_.find(short_name);
		if(indexed == definitions_by_name_.end()) return result;
		for(size_t indexed_definition = 0; indexed_definition < indexed->second.size();
			++indexed_definition) {
			map<string, TemplateDefinition>::const_iterator it = definitions_.find(
				indexed->second[indexed_definition]);
			if(it == definitions_.end() || it->second.class_template ||
				candidates.find(it->second.qualified_name) == candidates.end()) continue;
			bool duplicate = false;
			for(size_t i = 0; i < result.size(); ++i)
				if(result[i] == &it->second) duplicate = true;
			if(!duplicate) result.push_back(&it->second);
		}
		if(!result.empty()) return result;
		// A generated body can retain the physical inline-namespace context even
		// though the source declaration was collected under its logical owner.
		// Re-run the owner filter through the namespace map before falling back to
		// unrelated same-named overloads.
		string logical_context = PrefixComponent(context);
		map<string, string>::const_iterator context_logical =
			lexical_namespace_logical_.find(logical_context);
		if(context_logical != lexical_namespace_logical_.end())
			logical_context = context_logical->second;
		for(size_t indexed_definition = 0; indexed_definition < indexed->second.size();
			++indexed_definition) {
			map<string, TemplateDefinition>::const_iterator it = definitions_.find(
				indexed->second[indexed_definition]);
			if(it == definitions_.end() || it->second.class_template) continue;
			string owner = PrefixComponent(it->second.qualified_name);
			map<string, string>::const_iterator owner_logical =
				lexical_namespace_logical_.find(owner);
			if(owner_logical != lexical_namespace_logical_.end()) owner = owner_logical->second;
			if(owner == logical_context ||
				(logical_context.size() > owner.size() && logical_context.compare(0,
					owner.size(), owner) == 0 && logical_context[owner.size()] == ':')) {
				result.push_back(&it->second);
			}
		}
		if(!result.empty()) return result;
		for(size_t indexed_definition = 0; indexed_definition < indexed->second.size();
			++indexed_definition) {
			map<string, TemplateDefinition>::const_iterator it = definitions_.find(
				indexed->second[indexed_definition]);
			if(it != definitions_.end() && !it->second.class_template)
				result.push_back(&it->second);
		}
		return result;
	}
