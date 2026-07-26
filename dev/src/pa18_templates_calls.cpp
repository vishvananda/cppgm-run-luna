#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
using namespace std;
namespace pa18_templates_internal {
bool PA18TemplateExpander::InstantiateMemberCall(const CPPGMAstNodePtr& call,
	const CPPGMAstNodePtr& callee, const string& original_member,
	const string& context,
	const map<string, string>& substitutions,
	bool explicit_instantiation)
{
	if(!call || !callee || callee->kind != "member-expression" ||
		callee->children.size() < 2 || !callee->children[1]) return false;
	string member_spelling = original_member.empty() ? callee->children[1]->value :
		original_member;
	member_spelling = RemoveMarker(member_spelling);
	member_spelling = CanonicalSpelling(member_spelling);
	if(member_spelling.empty()) return false;
	string member_name = member_spelling;
	string member_qualifier;
	vector<string> explicit_member_arguments;
	const size_t member_open = member_spelling.find('<');
	// Operator names contain `<` as part of the operator token (`operator<<`),
	// not as a template-id delimiter.  Let ordinary member lookup see those
	// names; explicit operator template-ids are handled by the parsed member
	// spelling when a real range is present.
	const size_t qualified_template_separator = member_open == string::npos ?
		string::npos : member_spelling.find("::", member_open);
	if(member_open != string::npos && qualified_template_separator != string::npos &&
		member_spelling.compare(0, 8, "operator") != 0) {
		// `base<T>::operator=` is a qualified member name, not a member
		// template-id named `base`.  Keep the dependent owner separate so the
		// inherited-member replay can materialize the operator on that base.
		member_qualifier = member_spelling.substr(0, qualified_template_separator);
		member_name = LastComponent(member_spelling.substr(
			qualified_template_separator + 2));
	} else if(member_open != string::npos && member_spelling.compare(0, 8, "operator") != 0) {
		string member_base;
		string member_argument_text;
		size_t member_begin = 0;
		size_t member_close = string::npos;
		if(!TemplateBase(member_spelling, member_open, &member_begin, &member_base) ||
			!TemplateRange(member_spelling, member_open, &member_argument_text,
				&member_close)) return false;
		member_name = LastComponent(member_base);
		const size_t qualifier_separator = member_base.rfind("::");
		if(qualifier_separator != string::npos)
			member_qualifier = member_base.substr(0, qualifier_separator);
		explicit_member_arguments = SplitTemplateArguments(member_argument_text);
	} else {
		member_name = LastComponent(member_name);
		const size_t qualifier_separator = member_spelling.rfind("::");
		if(qualifier_separator != string::npos)
			member_qualifier = member_spelling.substr(0, qualifier_separator);
	}
	if(member_name.empty()) return false;
	string object_type;
	if(callee->children[0] && callee->children[0]->kind == "keyword-literal" &&
		RemoveMarker(callee->children[0]->value) == "this") {
		map<string, string>::const_iterator function_owner = function_owners_.find(context);
		if(function_owner != function_owners_.end()) object_type = function_owner->second;
		for(string current = object_type.empty() ? context : string(); !current.empty(); ) {
			const TemplateDefinition* current_definition = FindDefinition(current, context);
			if(class_contexts_.find(current) != class_contexts_.end() ||
				(current_definition && current_definition->class_template)) {
				object_type = current;
				break;
			}
			if(current.empty()) break;
			const size_t separator = current.rfind("::");
			if(separator == string::npos) current.clear();
			else current.erase(separator);
		}
		if(object_type.empty()) object_type = context;
	} else if(!InferArgument(callee->children[0], &object_type, substitutions, context))
		return false;
	bool object_const = false;
	bool object_volatile = false;
	string object_cv_probe = CanonicalSpelling(ResolveAlias(RewriteText(
		object_type, context, substitutions, 0), context));
	while(!object_cv_probe.empty() && (object_cv_probe[object_cv_probe.size() - 1] == '&' ||
		object_cv_probe[object_cv_probe.size() - 1] == '*'))
		object_cv_probe = CanonicalSpelling(object_cv_probe.substr(0, object_cv_probe.size() - 1));
	while(object_cv_probe.compare(0, 6, "const ") == 0) {
		object_const = true;
		object_cv_probe = CanonicalSpelling(object_cv_probe.substr(6));
	}
	while(object_cv_probe.compare(0, 9, "volatile ") == 0) {
		object_volatile = true;
		object_cv_probe = CanonicalSpelling(object_cv_probe.substr(9));
	}
	while(object_cv_probe.size() > 6 && object_cv_probe.compare(
		object_cv_probe.size() - 6, 6, " const") == 0) {
		object_const = true;
		object_cv_probe = CanonicalSpelling(object_cv_probe.substr(0,
			object_cv_probe.size() - 6));
	}
	while(object_cv_probe.size() > 9 && object_cv_probe.compare(
		object_cv_probe.size() - 9, 9, " volatile") == 0) {
		object_volatile = true;
		object_cv_probe = CanonicalSpelling(object_cv_probe.substr(0,
			object_cv_probe.size() - 9));
	}
	object_type = CanonicalSpelling(RewriteText(object_type, context, substitutions, 0));
	object_type = ResolveAlias(object_type, context);
	while(object_type.compare(0, 6, "const ") == 0 ||
		object_type.compare(0, 9, "volatile ") == 0)
		object_type = CanonicalSpelling(object_type.substr(object_type.find(' ') + 1));
	while(!object_type.empty() && (object_type[object_type.size() - 1] == '*' ||
		object_type[object_type.size() - 1] == '&'))
		object_type = CanonicalSpelling(object_type.substr(0, object_type.size() - 1));
	while(object_type.size() > 6 && object_type.compare(object_type.size() - 6, 6,
		" const") == 0)
		object_type = CanonicalSpelling(object_type.substr(0, object_type.size() - 6));
	while(object_type.size() > 9 && object_type.compare(object_type.size() - 9, 9,
		" volatile") == 0)
		object_type = CanonicalSpelling(object_type.substr(0, object_type.size() - 9));
	if(object_type.empty()) return false;
	const TemplateDefinition* parent = 0;
	vector<string> parent_arguments;
	map<string, string> member_substitutions = substitutions;
	map<string, string>::const_iterator owner_base = specialization_bases_.find(
		LastComponent(object_type));
	map<string, vector<string> >::const_iterator owner_arguments =
		specialization_arguments_.find(LastComponent(object_type));
	if(owner_base != specialization_bases_.end() &&
		owner_arguments != specialization_arguments_.end()) {
		parent = FindDefinition(owner_base->second, context);
		parent_arguments = owner_arguments->second;
		if(parent && parent->class_template) {
			const TemplateDefinition* selected_parent = SelectClassTemplateDefinition(
				parent, parent_arguments, context);
			if(selected_parent) parent = selected_parent;
		}
	} else {
		string source_owner = object_type;
		const size_t source_open = source_owner.find('<');
		if(source_open != string::npos) source_owner.erase(source_open);
		const TemplateDefinition* candidate_parent = FindDefinition(source_owner, context);
		if(candidate_parent && candidate_parent->class_template) parent = candidate_parent;
	}
	const vector<TemplateParameter>* enclosing_parameters = parent ?
		(parent->partial_specialization && !parent->specialization_parameter_details.empty() ?
			&parent->specialization_parameter_details : &parent->parameters) : 0;
	if(parent) {
		// A member template-id in the definition of a function template still
		// needs the dependent-name `template` disambiguator.  Do not turn a
		// dependent `Box<Tag>` object into a concrete member specialization merely
		// because its member has explicit arguments; the normal dependent-name
		// validation must reject the missing keyword.
		if(parent_arguments.empty() && object_type.find('<') != string::npos) {
			for(size_t parameter = 0; parameter < parent->parameters.size(); ++parameter) {
				const string& name = parent->parameters[parameter].name;
				if(name.empty()) continue;
				for(size_t position = object_type.find(name); position != string::npos;
					position = object_type.find(name, position + name.size())) {
					const bool left = position == 0 || !IsIdentifierCharacter(object_type[position - 1]);
					const size_t end = position + name.size();
					const bool right = end == object_type.size() ||
						!IsIdentifierCharacter(object_type[end]);
					if(left && right) return false;
				}
			}
		}
		for(size_t parameter = 0; parameter < enclosing_parameters->size() &&
			parameter < parent_arguments.size(); ++parameter)
			if(!(*enclosing_parameters)[parameter].name.empty() &&
				!(*enclosing_parameters)[parameter].pack)
				member_substitutions[(*enclosing_parameters)[parameter].name] =
					parent_arguments[parameter];
		if(!parent->name.empty()) member_substitutions[parent->name] = object_type;
	}
	string qualified_owner;
	if(!member_qualifier.empty()) {
		qualified_owner = CanonicalSpelling(RewriteText(member_qualifier, context,
			member_substitutions, 0));
		qualified_owner = CanonicalSpelling(ReplaceIdentifiers(qualified_owner,
			member_substitutions));
		qualified_owner = CanonicalSpelling(ResolveAlias(qualified_owner, context));
		const size_t owner_open = qualified_owner.find('<');
		if(owner_open != string::npos) qualified_owner.erase(owner_open);
		qualified_owner = LastComponent(qualified_owner);
	}

	vector<const TemplateDefinition*> candidates;
	map<string, vector<string> >::const_iterator indexed_members =
		definitions_by_name_.find(member_name);
	if(indexed_members != definitions_by_name_.end()) for(size_t indexed = 0;
		indexed < indexed_members->second.size(); ++indexed) {
		map<string, TemplateDefinition>::const_iterator it = definitions_.find(
			indexed_members->second[indexed]);
		if(it == definitions_.end()) continue;
		const TemplateDefinition& definition = it->second;
		if(definition.class_template || definition.alias_template ||
			definition.variable_template || definition.parameters.empty() ||
			LastComponent(definition.name) != member_name || !definition.declaration)
			continue;
		const bool declaration_kind = definition.declaration->kind == "function-definition" ||
			definition.declaration->kind == "simple-declaration" ||
			definition.declaration->kind == "special-member-definition";
		if(!declaration_kind) continue;
		bool owner_matches = false;
		if(!qualified_owner.empty()) {
			string source_owner = definition.owner;
			const size_t source_open = source_owner.find('<');
			if(source_open != string::npos) source_owner.erase(source_open);
			owner_matches = source_owner == qualified_owner ||
				LastComponent(source_owner) == qualified_owner;
		} else if(parent) owner_matches = MemberOwnerPattern(definition, *parent,
			parent_arguments, 0);
		else {
			string source_owner = definition.owner;
			const size_t source_open = source_owner.find('<');
			if(source_open != string::npos) source_owner.erase(source_open);
			owner_matches = source_owner == object_type ||
				LastComponent(source_owner) == LastComponent(object_type);
		}
		if(owner_matches) candidates.push_back(&definition);
	}
	if(parent && parent->partial_specialization) {
		const string member_scope = JoinPath(parent->qualified_name, parent->name);
		vector<const TemplateDefinition*> specialized_candidates;
		for(size_t candidate = 0; candidate < candidates.size(); ++candidate) {
			const string owner = candidates[candidate]->owner;
			const size_t angle = owner.find('<');
			if(angle != string::npos && owner.substr(0, angle) == member_scope)
				specialized_candidates.push_back(candidates[candidate]);
		}
		if(!specialized_candidates.empty()) candidates.swap(specialized_candidates);
	}
	const vector<const TemplateDefinition*> direct_member_candidates = candidates;
	if(HasViableOrdinaryCallableMember(call, object_type, member_name,
		context, substitutions)) return false;
	vector<const TemplateDefinition*> inherited_candidates;
	set<string> inherited_active;
	map<const TemplateDefinition*, string> inherited_owners;
	CollectInheritedMemberTemplates(object_type, member_name, member_substitutions,
		context, &inherited_candidates, &inherited_active, &inherited_owners);
	for(size_t inherited = 0; inherited < inherited_candidates.size(); ++inherited)
		if(find(candidates.begin(), candidates.end(), inherited_candidates[inherited]) ==
			candidates.end() || inherited_owners.find(inherited_candidates[inherited]) !=
			inherited_owners.end()) candidates.push_back(inherited_candidates[inherited]);
	stable_sort(candidates.begin(), candidates.end(), [this](const TemplateDefinition* left,
		const TemplateDefinition* right) { const int left_score = MemberTemplatePatternScore(left);
		const int right_score = MemberTemplatePatternScore(right); if(left_score != right_score) { return left_score > right_score; } if(left->parameters.size() != right->parameters.size()) { return left->parameters.size() < right->parameters.size(); } return false; }); if(candidates.empty()) return false;
	RankMemberCandidatesByClassExactness(&candidates, call, member_substitutions, context);
	map<const TemplateDefinition*, size_t> candidate_occurrences;
	for(size_t candidate_index = 0; candidate_index < candidates.size();
		++candidate_index) {
		const TemplateDefinition& definition = *candidates[candidate_index];
		if(object_const || object_volatile) {
			const string qualifiers = DeclaratorSuffix(FunctionDeclarator(
				definition.declaration));
			const bool function_const = qualifiers.find("const") != string::npos;
			const bool function_volatile = qualifiers.find("volatile") != string::npos;
			if(object_const && !function_const) continue;
			if(object_volatile && !function_volatile) continue;
		}
		TemplateDefinition inference_definition = definition;
		RestoreMemberTemplateDefaults(member_name, definition, &inference_definition);
		const size_t occurrence = candidate_occurrences[&definition]++;
		const bool direct_member = occurrence == 0 &&
			find(direct_member_candidates.begin(), direct_member_candidates.end(),
					&definition) != direct_member_candidates.end();
		map<string, string> candidate_substitutions = member_substitutions;
		// The caller's replay map can contain value-name rewrites introduced by
		// a local using-declaration.  Those names belong to the call site, not to
		// the selected member's lexical body; carrying them into a callable
		// object's operator() can rewrite an inner namespace function into the
		// outer object itself.  Retain only enclosing/member template bindings;
		// concrete class aliases are added from the selected owner below.
		set<string> body_bindings;
		for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter)
			if(!definition.parameters[parameter].name.empty())
				body_bindings.insert(definition.parameters[parameter].name);
		if(parent) {
			for(size_t parameter = 0; parameter < enclosing_parameters->size(); ++parameter)
				if(!(*enclosing_parameters)[parameter].name.empty())
					body_bindings.insert((*enclosing_parameters)[parameter].name);
			if(!parent->name.empty()) body_bindings.insert(parent->name);
		}
		for(map<string, string>::iterator binding = candidate_substitutions.begin();
			binding != candidate_substitutions.end(); ) {
			if(body_bindings.find(binding->first) == body_bindings.end())
				candidate_substitutions.erase(binding++);
			else ++binding;
		}
		map<const TemplateDefinition*, string>::const_iterator candidate_owner =
			inherited_owners.find(&definition);
		const string concrete_candidate_owner = !direct_member &&
			candidate_owner != inherited_owners.end() && !candidate_owner->second.empty() ?
			candidate_owner->second : object_type;
		// An inherited member template is deduced against the specialization that
		// actually declares it, not against the most-derived object.  Recover that
		// declaring specialization's template arguments before expanding aliases
		// such as `key_type` in its parameter list.
		map<string, string>::const_iterator inherited_base = specialization_bases_.find(
			LastComponent(concrete_candidate_owner));
		map<string, vector<string> >::const_iterator inherited_arguments =
			specialization_arguments_.find(LastComponent(concrete_candidate_owner));
		if(!direct_member && candidate_owner != inherited_owners.end() &&
			inherited_base != specialization_bases_.end() &&
			inherited_arguments != specialization_arguments_.end()) {
			const TemplateDefinition* inherited_definition = FindDefinition(
				inherited_base->second, context);
			if(inherited_definition && inherited_definition->class_template) {
				for(size_t parameter = 0; parameter < inherited_definition->parameters.size() &&
					parameter < inherited_arguments->second.size(); ++parameter)
					if(!inherited_definition->parameters[parameter].name.empty())
						candidate_substitutions[inherited_definition->parameters[parameter].name] =
							inherited_arguments->second[parameter];
				if(!inherited_definition->name.empty())
					candidate_substitutions[inherited_definition->name] = concrete_candidate_owner;
			}
		}
		const CPPGMAstNodePtr concrete_candidate_declaration =
			FindClassDeclaration(concrete_candidate_owner, context);
		if(concrete_candidate_declaration) for(size_t member = 0;
			member < concrete_candidate_declaration->children.size(); ++member) {
			const CPPGMAstNodePtr declaration = concrete_candidate_declaration->children[member];
			if(!declaration || declaration->kind != "simple-declaration" ||
				declaration->children.empty() || SpellNode(declaration->children[0]).find(
					"typedef") == string::npos) continue;
			const CPPGMAstNodePtr list = ChildOfKindLocal(declaration,
				"init-declarator-list");
			if(!list) continue;
			for(size_t item = 0; item < list->children.size(); ++item) {
				const CPPGMAstNodePtr entry = list->children[item];
				if(!entry || entry->children.empty()) continue;
				const string alias = FirstIdentifierLocal(entry->children[0]);
				if(alias.empty()) continue;
				string alias_type = NodeTypeSpelling(declaration->children[0]) +
					DeclaratorSuffix(entry->children[0]);
				alias_type = CanonicalSpelling(ResolveAlias(ReplaceIdentifiers(
					RewriteText(alias_type, concrete_candidate_owner,
						candidate_substitutions, 0), candidate_substitutions), context));
				if(!alias_type.empty()) candidate_substitutions[alias] = alias_type;
			}
		}
		if(definition.declaration->kind == "simple-declaration") {
			bool has_definition = false;
			for(size_t other_index = 0; other_index < candidates.size(); ++other_index) {
				const TemplateDefinition& other = *candidates[other_index];
				if(other.declaration->kind == "function-definition" &&
					( MemberSignatureKey(other) == MemberSignatureKey(definition) ||
						(definition.friend_declaration &&
							LastComponent(other.name) == LastComponent(definition.name)))) {
					has_definition = true;
					break;
				}
			}
			if(has_definition) continue;
		}
		vector<string> member_arguments;
		map<string, vector<string> > inferred_pack_values;
		map<string, FunctionSignature> inferred_function_values;
		vector<string> explicit_arguments = explicit_member_arguments;
		for(size_t argument = 0; argument < explicit_arguments.size(); ++argument) {
			explicit_arguments[argument] = NormalizeTypeArgument(RewriteText(
				explicit_arguments[argument], context, candidate_substitutions, 0));
			explicit_arguments[argument] = NormalizeTypeArgument(ReplaceIdentifiers(
				explicit_arguments[argument], candidate_substitutions));
			explicit_arguments[argument] = ResolveAlias(explicit_arguments[argument], context);
			explicit_arguments[argument] = QualifyTypeArgument(
				explicit_arguments[argument], context, definition.owner);
		}
		const vector<string>* explicit_prefix = explicit_arguments.empty() ? 0 :
			&explicit_arguments;
		bool inferred = false;
		map<string, string> deduction_substitutions = candidate_substitutions;
		if(parent && !parent->name.empty())
			// The enclosing class name is useful while replaying the generated
			// member body, but it is not a template parameter.  Leaving it in the
			// deduction map makes textual substitution turn `iter<Buff, T>` into
			// `iter_<concrete-args><Buff, T>` before matching the member pattern.
			deduction_substitutions.erase(parent->name);
		map<string, vector<string> > bound_pack_values;
		if(parent) {
			size_t parent_argument = 0;
			for(size_t parent_parameter_index = 0;
				parent_parameter_index < enclosing_parameters->size(); ++parent_parameter_index) {
				const TemplateParameter& parent_parameter =
					(*enclosing_parameters)[parent_parameter_index];
				if(parent_parameter.pack) {
					size_t trailing_fixed = 0;
					for(size_t later = parent_parameter_index + 1;
						later < enclosing_parameters->size(); ++later)
						if(!(*enclosing_parameters)[later].pack) ++trailing_fixed;
					const size_t available = parent_arguments.size() > parent_argument ?
						parent_arguments.size() - parent_argument : 0;
					const size_t count = available > trailing_fixed ?
						available - trailing_fixed : 0;
					vector<string>& values = bound_pack_values[parent_parameter.name];
					for(size_t element = 0; element < count; ++element)
						values.push_back(parent_arguments[parent_argument++]);
				} else if(parent_argument < parent_arguments.size()) ++parent_argument;
			}
		}
	map<string, vector<string> > forwarding_pack_values;
	try {
		inferred = InferFunctionArguments(inference_definition, call, &member_arguments,
				deduction_substitutions, context, explicit_prefix, &inferred_pack_values,
				&inferred_function_values, &bound_pack_values, &forwarding_pack_values);
		} catch(const logic_error&) {
			inferred = false;
		}
		// An explicit member-template-id already fixes every template parameter.
		// Function-pointer expressions can still be intentionally deferred by the
		// general deduction path (notably an address of an overloaded function),
		// but that must not prevent the explicit specialization from being
		// materialized under its concrete member owner.
		if(!inferred && !explicit_arguments.empty() &&
			explicit_arguments.size() == definition.parameters.size() &&
			find_if(definition.parameters.begin(), definition.parameters.end(),
				[](const TemplateParameter& parameter) { return parameter.pack; }) ==
				definition.parameters.end()) {
			member_arguments = explicit_arguments;
			inferred = true;
		}
		if(!inferred) {
			continue;
		}
		bool dependent_member_arguments = false;
		for(size_t parameter = 0; parameter < definition.parameters.size() &&
			!dependent_member_arguments; ++parameter) {
			const string& name = definition.parameters[parameter].name;
			if(name.empty()) continue;
			for(size_t argument = 0; argument < member_arguments.size(); ++argument)
				for(size_t at = member_arguments[argument].find(name); at != string::npos;
					at = member_arguments[argument].find(name, at + name.size())) {
					const bool left = at == 0 || !IsIdentifierCharacter(
						member_arguments[argument][at - 1]);
					const size_t end = at + name.size();
					const bool right = end == member_arguments[argument].size() ||
						!IsIdentifierCharacter(member_arguments[argument][end]);
					if(left && right) { dependent_member_arguments = true; break; }
				}
		}
		if(dependent_member_arguments) continue;

		string requested_owner = object_type;
		if(!active_instantiation_name_.empty()) {
			map<string, string>::const_iterator active_base = specialization_bases_.find(
				LastComponent(active_instantiation_name_));
			if(active_base != specialization_bases_.end() &&
				LastComponent(active_base->second) == LastComponent(definition.owner))
				requested_owner = active_instantiation_name_;
		}
		map<const TemplateDefinition*, string>::const_iterator inherited_owner =
			inherited_owners.find(&definition);
		if(!direct_member && inherited_owner != inherited_owners.end() &&
			!inherited_owner->second.empty())
			requested_owner = inherited_owner->second;
		const bool concrete_owner = specialization_bases_.find(
			LastComponent(requested_owner)) != specialization_bases_.end() &&
			specialization_arguments_.find(LastComponent(requested_owner)) !=
				specialization_arguments_.end();
		const string* requested_owner_pointer = concrete_owner ? &requested_owner : 0;
		map<string, vector<string> > instantiation_pack_hints = inferred_pack_values;
		for(map<string, vector<string> >::const_iterator bound = bound_pack_values.begin();
			bound != bound_pack_values.end(); ++bound)
			instantiation_pack_hints[bound->first].insert(
				instantiation_pack_hints[bound->first].end(), bound->second.begin(),
				bound->second.end());
		string generated_name;
		const ConcreteOwnerContext previous_concrete_owner = active_concrete_owner_;
		if(requested_owner_pointer) SetActiveConcreteOwner(requested_owner, context);
		try {
		generated_name = Instantiate(definition, member_arguments, context,
			explicit_instantiation,
				&instantiation_pack_hints, &candidate_substitutions,
				requested_owner_pointer, &inferred_function_values,
				&forwarding_pack_values);
		} catch(const logic_error&) {
			active_concrete_owner_ = previous_concrete_owner;
			continue;
		}
		active_concrete_owner_ = previous_concrete_owner;
		call->template_primary = definition.qualified_name;
		call->template_arguments = member_arguments;
		map<string, string> result_substitutions = candidate_substitutions;
		for(size_t parameter = 0; parameter < definition.parameters.size() &&
			parameter < member_arguments.size(); ++parameter)
			if(!definition.parameters[parameter].name.empty())
				result_substitutions[definition.parameters[parameter].name] =
					member_arguments[parameter];
		map<const TemplateDefinition*, string>::const_iterator result_owner =
			inherited_owners.find(&definition);
		string result_owner_name = !direct_member && result_owner != inherited_owners.end() ?
			result_owner->second : object_type;
		map<string, string>::const_iterator result_base = specialization_bases_.find(
			LastComponent(result_owner_name));
		map<string, vector<string> >::const_iterator result_arguments =
			specialization_arguments_.find(LastComponent(result_owner_name));
		if(result_base != specialization_bases_.end() &&
			result_arguments != specialization_arguments_.end()) {
			const TemplateDefinition* owner_definition = FindDefinition(result_base->second,
				context);
			if(owner_definition) for(size_t parameter = 0;
				parameter < owner_definition->parameters.size() &&
				parameter < result_arguments->second.size(); ++parameter)
				if(!owner_definition->parameters[parameter].name.empty())
					result_substitutions[owner_definition->parameters[parameter].name] =
						result_arguments->second[parameter];
		}
		if(definition.declaration && !definition.declaration->children.empty()) {
			string result_type = NodeTypeSpelling(definition.declaration->children[0]);
			const CPPGMAstNodePtr result_declarator = FunctionDeclarator(definition.declaration);
			result_type += ReturnDeclaratorSuffix(result_declarator);
			result_type = CanonicalSpelling(ResolveAlias(RewriteText(result_type, context,
				result_substitutions, 0), context));
			call->inferred_type = result_type;
		}
	const bool static_member = definition.static_member;
		const bool generated_operator = member_name.compare(0, 8, "operator") == 0;
	const bool ordinary_class_member = !definition.owner.empty() &&
			definition.owner.find('<') == string::npos &&
			FindClassDeclaration(definition.owner, context) != CPPGMAstNodePtr() &&
			!definition.member_template;
		if(static_member && definition.owner.find("::") == string::npos && concrete_owner) {
			call->children[0] = CPPGMAstNodePtr(new CPPGMAstNode("id-expression",
				definition.owner + "::" + member_name));
		} else callee->children[1]->value =
			(ordinary_class_member && !generated_operator) ? member_name :
			(concrete_owner && !generated_operator ?
				member_name : generated_name);
		if(!member_qualifier.empty() && concrete_owner && !static_member) {
			// Preserve a dependent qualified-base call as a qualified generated
			// function.  Leaving it as `this->operator=...` redispatches through
			// the derived class during PA14 lookup and loses the selected base
			// specialization.
			call->children[0] = CPPGMAstNodePtr(new CPPGMAstNode("id-expression",
				requested_owner + "::" + generated_name));
		}
		return true;
	}
	return false;
}

void PA18TemplateExpander::CollectInheritedMemberTemplates(const string& raw_class,
	const string& member, const map<string, string>& substitutions,
	const string& context, vector<const TemplateDefinition*>* result,
	set<string>* active, map<const TemplateDefinition*, string>* concrete_owners)
{
	if(raw_class.empty() || member.empty() || !result || !active) return;
	string class_key = CanonicalSpelling(ReplaceIdentifiers(raw_class, substitutions));
	class_key = CanonicalSpelling(ResolveAlias(class_key, context));
	while(class_key.compare(0, 6, "const ") == 0 ||
		class_key.compare(0, 9, "volatile ") == 0)
		class_key = CanonicalSpelling(class_key.substr(class_key.find(' ') + 1));
	while(!class_key.empty() && (class_key[class_key.size() - 1] == '&' ||
		class_key[class_key.size() - 1] == '*'))
		class_key = CanonicalSpelling(class_key.substr(0, class_key.size() - 1));
	if(class_key.empty() || !active->insert(class_key + "|" + member).second) return;

	map<string, string> class_substitutions = substitutions;
	map<string, string>::const_iterator generated_base = specialization_bases_.find(
		LastComponent(class_key));
	map<string, vector<string> >::const_iterator generated_arguments =
		specialization_arguments_.find(LastComponent(class_key));
	if(generated_base != specialization_bases_.end() &&
		generated_arguments != specialization_arguments_.end()) {
		const TemplateDefinition* source = FindDefinition(generated_base->second, context);
		if(source && source->class_template) {
			for(size_t parameter = 0; parameter < source->parameters.size() &&
				parameter < generated_arguments->second.size(); ++parameter)
				if(!source->parameters[parameter].name.empty())
					class_substitutions[source->parameters[parameter].name] =
						generated_arguments->second[parameter];
			if(!source->name.empty()) class_substitutions[source->name] = class_key;
		}
	}
	CPPGMAstNodePtr declaration = FindClassDeclaration(class_key, context);
	if(!declaration) {
		active->erase(class_key + "|" + member);
		return;
	}
	const string declaration_context = PrefixComponent(class_key).empty() ?
		context : PrefixComponent(class_key);
	for(size_t child = 0; child < declaration->children.size(); ++child) {
		const CPPGMAstNodePtr clause = declaration->children[child];
		if(!clause || clause->kind != "base-clause") continue;
		for(size_t base_index = 0; base_index < clause->children.size(); ++base_index) {
			const CPPGMAstNodePtr base_specifier = clause->children[base_index];
			const CPPGMAstNodePtr base_name = ChildOfKindLocal(base_specifier, "base-name");
			if(!base_name) continue;
			string base_spelling = CanonicalSpelling(RemoveMarker(RewriteText(
				base_name->value, declaration_context, class_substitutions, 0)));
			base_spelling = CanonicalSpelling(ReplaceIdentifiers(base_spelling,
				class_substitutions));
			base_spelling = CanonicalSpelling(ResolveAlias(base_spelling,
				declaration_context));
			string base_lookup = base_spelling;
			vector<string> base_arguments;
			const TemplateDefinition* base_definition = 0;
			bool base_lookup_generated = false;
			const size_t open = base_spelling.find('<');
			if(open != string::npos) {
				string argument_text;
				size_t close = string::npos;
				if(!TemplateRange(base_spelling, open, &argument_text, &close)) continue;
				base_lookup = CanonicalSpelling(base_spelling.substr(0, open));
				base_definition = FindDefinition(base_lookup, declaration_context);
				base_arguments = SplitTemplateArguments(argument_text);
				if(base_definition) for(size_t argument = 0; argument < base_arguments.size(); ++argument) {
					base_arguments[argument] = NormalizeTypeArgument(RewriteText(
						base_arguments[argument], declaration_context, class_substitutions, 0));
					base_arguments[argument] = NormalizeTypeArgument(ReplaceIdentifiers(
						base_arguments[argument], class_substitutions));
					base_arguments[argument] = ResolveAlias(base_arguments[argument],
						declaration_context);
					base_arguments[argument] = QualifyTypeArgument(base_arguments[argument],
						declaration_context, base_definition->owner);
				}
			}
			if(!base_definition) {
				base_definition = FindDefinition(base_lookup, declaration_context);
				if(base_definition && base_definition->class_template)
					base_lookup = base_definition->qualified_name;
			}
			if(!base_definition) {
				map<string, string>::const_iterator generated = specialization_bases_.find(
					LastComponent(base_lookup));
				map<string, vector<string> >::const_iterator generated_args =
					specialization_arguments_.find(LastComponent(base_lookup));
				if(generated != specialization_bases_.end() &&
					generated_args != specialization_arguments_.end()) {
					base_definition = FindDefinition(generated->second, declaration_context);
					if(base_definition) {
						base_arguments = generated_args->second;
						base_lookup_generated = true;
					}
				}
			}
			map<string, string> base_substitutions = class_substitutions;
			if(base_definition) for(size_t parameter = 0;
				parameter < base_definition->parameters.size() &&
				parameter < base_arguments.size(); ++parameter)
				if(!base_definition->parameters[parameter].name.empty())
					base_substitutions[base_definition->parameters[parameter].name] =
						base_arguments[parameter];
			map<string, vector<string> >::const_iterator indexed_members =
				definitions_by_name_.find(member);
			if(indexed_members != definitions_by_name_.end()) for(size_t indexed = 0;
				indexed < indexed_members->second.size(); ++indexed) {
				map<string, TemplateDefinition>::const_iterator it = definitions_.find(
					indexed_members->second[indexed]);
				if(it == definitions_.end()) continue;
				const TemplateDefinition& candidate = it->second;
				if(candidate.class_template || candidate.alias_template ||
					candidate.variable_template || candidate.parameters.empty() ||
					LastComponent(candidate.name) != member || !candidate.declaration)
					continue;
				const bool declaration_kind = candidate.declaration->kind == "function-definition" ||
					candidate.declaration->kind == "simple-declaration" ||
					candidate.declaration->kind == "special-member-definition";
				if(!declaration_kind) continue;
				string owner = candidate.owner;
				const size_t owner_open = owner.find('<');
				if(owner_open != string::npos) owner.erase(owner_open);
				bool matches = false;
				if(base_definition && base_definition->class_template)
					matches = MemberOwnerPattern(candidate, *base_definition,
						base_arguments, 0);
				else matches = owner == base_lookup ||
					LastComponent(owner) == LastComponent(base_lookup);
				if(!matches) continue;
				if(concrete_owners && base_lookup_generated &&
					concrete_owners->find(&candidate) == concrete_owners->end())
					(*concrete_owners)[&candidate] = base_lookup;
				if(find(result->begin(), result->end(), &candidate) == result->end())
					result->push_back(&candidate);
			}
			string recursive_base = base_lookup;
			if(!base_lookup_generated && !base_arguments.empty() && base_definition &&
				base_definition->class_template) {
				recursive_base += "<";
				for(size_t argument = 0; argument < base_arguments.size(); ++argument) {
					if(argument) recursive_base += ",";
					recursive_base += base_arguments[argument];
				}
				recursive_base += ">";
			}
			CollectInheritedMemberTemplates(recursive_base, member, base_substitutions,
				declaration_context, result, active, concrete_owners);
		}
	}
	active->erase(class_key + "|" + member);
}

CPPGMAstNodePtr PA18TemplateExpander::TransformCallExpression(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions)
{
	CPPGMAstNodePtr result(new CPPGMAstNode(input->kind, input->value));
	result->initializer_form = input->initializer_form;
	result->template_instantiation = input->template_instantiation;
	result->explicit_specialization = input->explicit_specialization;
	result->explicit_instantiation = input->explicit_instantiation;
	result->extern_instantiation = input->extern_instantiation;
	result->dependent_base_lookup = input->dependent_base_lookup;
	result->materialize_object_address = input->materialize_object_address;
	result->materialize_object_name = input->materialize_object_name;
	result->inferred_type = input->inferred_type;
	result->source_token_begin = input->source_token_begin;
	result->source_token_end = input->source_token_end;
	result->template_primary = input->template_primary;
	result->template_arguments = input->template_arguments;
	CPPGMAstNodePtr input_callee = input->children.empty() ? CPPGMAstNodePtr() :
		input->children[0];
	if(input_callee && input_callee->kind == "parenthesized-expression" &&
		input_callee->children.size() == 1 && input_callee->children[0] &&
		input_callee->children[0]->kind == "id-expression")
		input_callee = input_callee->children[0];
	if(input_callee && input_callee->kind == "id-expression" &&
		TransformQualifiedMemberTemplateCall(input, input_callee, context,
			substitutions, result))
		return result;
	// The parser leaves an unqualified explicit member-template-id as an
	// id-expression; replay it through typed `this` lookup before free lookup.
	if(input_callee && input_callee->kind == "id-expression") {
		const string raw_member_id = RemoveMarker(input_callee->value);
		const size_t member_id_open = raw_member_id.find('<');
		if(member_id_open != string::npos) {
			string member_id_base, member_id_arguments; size_t member_id_begin = 0, member_id_close = string::npos;
			if(TemplateBase(raw_member_id, member_id_open, &member_id_begin, &member_id_base) &&
				TemplateRange(raw_member_id, member_id_open, &member_id_arguments, &member_id_close)) {
			const vector<const TemplateDefinition*> member_candidates = FindFunctionDefinitions(LastComponent(member_id_base), context);
			bool member_context = !active_static_member_ &&
				function_owners_.find(context) != function_owners_.end();
			for(string current = context; !active_static_member_ &&
				!member_context && !current.empty(); ) {
				if(class_contexts_.find(current) != class_contexts_.end()) {
					member_context = true;
					break;
				}
				const size_t separator = current.rfind("::");
				if(separator == string::npos) current.clear();
					else current.erase(separator);
				}
				bool has_member_candidate = false;
				for(size_t candidate = 0; candidate < member_candidates.size(); ++candidate)
					if(member_candidates[candidate]->member_template) { has_member_candidate = true; break; }
				if(member_context && has_member_candidate) {
					CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "."));
					member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
						"keyword-literal", "KW_THIS:this")));
					member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
						"identifier", raw_member_id)));
					CPPGMAstNodePtr member_call(new CPPGMAstNode("call-expression")); member_call->children.push_back(member); CPPGMAstNodePtr member_arguments(new CPPGMAstNode("argument-list"));
					if(input->children.size() > 1 && input->children[1]) {
						CPPGMAstNodePtr transformed_arguments = TransformNode(input->children[1],
							context, substitutions);
						if(transformed_arguments) member_arguments = transformed_arguments;
					}
					member_call->children.push_back(member_arguments);
					if(InstantiateMemberCall(member_call, member, raw_member_id, context,
						substitutions)) {
						result->children = member_call->children;
						result->template_primary = member_call->template_primary; result->template_arguments = member_call->template_arguments;
						result->template_instantiation = true;
						result->inferred_type = member_call->inferred_type;
						return result;
					}
				}
			}
		}
	}
	if(input_callee && input_callee->kind == "id-expression") {
		const string raw_callee = input_callee->value;
		string lookup_callee = raw_callee;
		const size_t qualifier_separator = lookup_callee.find("::");
		if(qualifier_separator != string::npos) {
			const map<string, string>::const_iterator alias = substitutions.find(
				lookup_callee.substr(0, qualifier_separator));
			if(alias != substitutions.end()) lookup_callee = alias->second +
				lookup_callee.substr(qualifier_separator);
		}
		const size_t open = lookup_callee.find('<');
		if(open != string::npos) {
			string base;
			size_t begin = 0;
			string argument_text;
			size_t close = string::npos;
			const TemplateDefinition* explicit_definition = 0;
			if(TemplateBase(lookup_callee, open, &begin, &base) &&
				TemplateRange(lookup_callee, open, &argument_text, &close))
				explicit_definition = FindDefinition(base, context);
			if(explicit_definition && !explicit_definition->class_template) {
				const vector<const TemplateDefinition*> overloads = FindFunctionDefinitions(base, context);
				if(overloads.size() > 1) {
					const vector<string> raw_explicit_args = SplitTemplateArguments(argument_text);
					for(size_t overload = 0; overload < overloads.size(); ++overload) {
						vector<string> trial_arguments;
						if(ValidateExplicitFunctionCandidate(*overloads[overload], input, context,
							substitutions, raw_explicit_args, &trial_arguments)) {
							explicit_definition = overloads[overload];
							break;
						}
					}
				}
			}
			if(explicit_definition && !explicit_definition->class_template) {
				vector<string> explicit_args = SplitTemplateArguments(argument_text);
				map<string, string> explicit_substitutions = substitutions;
				for(map<string, PA19IntegralValue>::const_iterator integral =
					active_integral_substitutions_.begin();
					integral != active_integral_substitutions_.end(); ++integral)
					if(integral->second.known)
						explicit_substitutions[integral->first] =
							IntegralValueSpelling(integral->second);
				for(size_t i = 0; i < explicit_args.size(); ++i) {
					explicit_args[i] = NormalizeTypeArgument(RewriteText(
						explicit_args[i], context, explicit_substitutions, 0));
					explicit_args[i] = NormalizeTypeArgument(ReplaceIdentifiers(
						explicit_args[i], explicit_substitutions));
					explicit_args[i] = ResolveAlias(explicit_args[i], context);
					explicit_args[i] = NormalizeTypeArgument(RewriteText(
						explicit_args[i], context, explicit_substitutions, 0));
					explicit_args[i] = ResolveAlias(explicit_args[i], context);
					explicit_args[i] = QualifyTypeArgument(explicit_args[i], context,
						explicit_definition->owner);
				}
				const TemplateDefinition* explicit_specialization =
					FindExplicitFunctionSpecialization(base, explicit_args, context);
				if(explicit_specialization) explicit_definition = explicit_specialization;
				vector<string> complete_args;
				map<string, FunctionSignature> inferred_function_values;
				bool has_parameter_pack = false;
				size_t fixed_template_parameters = 0;
				for(size_t parameter = 0; parameter < explicit_definition->parameters.size(); ++parameter)
					if(explicit_definition->parameters[parameter].pack)
						has_parameter_pack = true;
					else ++fixed_template_parameters;
				const bool pack_precedes_fixed = HasPackBeforeFixed(*explicit_definition);
				const bool explicit_pack_elements = has_parameter_pack &&
					explicit_args.size() > fixed_template_parameters;
				bool complete = !pack_precedes_fixed && (explicit_pack_elements ||
					(!has_parameter_pack && explicit_args.size() == explicit_definition->parameters.size()));
				if(complete) complete_args = explicit_args;
				else try { complete = InferFunctionArguments(*explicit_definition, input, &complete_args, substitutions, context, &explicit_args, 0, &inferred_function_values); }
				catch(const PA18SubstitutionFailure&) { complete = false; }
				if(complete) {
					try {
						const string requested_owner_name = explicit_definition->member_template ?
							active_instantiation_name_ : string();
						const string* requested_owner = requested_owner_name.empty() ? 0 : &requested_owner_name;
						const string local_name = Instantiate(*explicit_definition, complete_args, context,
							false, 0, 0, requested_owner, &inferred_function_values);
						result->template_primary = explicit_definition->qualified_name;
						result->template_arguments = complete_args;
						const string qualifier = PrefixComponent(base);
						CPPGMAstNodePtr callee(new CPPGMAstNode("id-expression",
							qualifier.empty() ? local_name : qualifier + "::" + local_name));
						result->children.push_back(callee);
						for(size_t i = 1; i < input->children.size(); ++i) {
							CPPGMAstNodePtr child = TransformNode(input->children[i], context,
								substitutions);
							if(child) result->children.push_back(child);
						}
						return result;
					} catch(const PA18SubstitutionFailure&) {}
				}
			}
		}
	}
	for(size_t i = 0; i < input->children.size(); ++i) {
		const bool preserve_array_alias = i == 0 && input->value == "braced-construction" && input->children[i] &&
			input->children[i]->kind == "id-expression" &&
			substitutions.find(input->children[i]->value) != substitutions.end() &&
			IsArrayTypeAlias(input->children[i]->value, context);
		CPPGMAstNodePtr child = preserve_array_alias ? CloneNode(input->children[i]) :
			TransformNode(input->children[i], context, substitutions);
		if(child) result->children.push_back(child);
	}
	CPPGMAstNodePtr result_callee = result->children.empty() ? CPPGMAstNodePtr() :
		result->children[0];
	if(result_callee && result_callee->kind == "parenthesized-expression" &&
		result_callee->children.size() == 1 && result_callee->children[0] &&
		result_callee->children[0]->kind == "id-expression") {
		result_callee = result_callee->children[0];
		result->children[0] = result_callee;
	}
	// A forwarding call commonly keeps its callable object as a static-cast
	// expression (`static_cast<F&&>(function)(argument)`).  Its operator()
	// template is a member candidate even though the callee is not an
	// id-expression, so materialize that candidate before PA14 sees the call.
	if(result_callee && result_callee->kind == "cast-expression" &&
		RemoveMarker(result_callee->value) == "static_cast") {
		CPPGMAstNodePtr operator_member(new CPPGMAstNode("member-expression", "."));
		operator_member->children.push_back(result_callee);
		operator_member->children.push_back(CPPGMAstNodePtr(
			new CPPGMAstNode("identifier", "operator()")));
		if(InstantiateMemberCall(result, operator_member, "operator()", context,
			substitutions)) {
			result->children[0] = operator_member;
			result_callee = operator_member;
		}
	}
	// A constructor's function-pointer parameter supplies the expected
	// signature for an otherwise overloaded function template argument.  The
	// class specialization has already been rewritten at this point, so use
	// its concrete constructor declaration before ordinary call deduction.
	ResolveClassConstructorFunctionArguments(result, context);
	// RewriteText may have already collapsed a qualified class-template-id to
	// its generated concrete owner while retaining a member-template call as
	// `Owner_generated::member`.  Recover the typed owner here; otherwise the
	// ordinary free-function lookup sees only the generated spelling and loses
	// the selected member specialization (notably a partial class
	// specialization reached from a recursive member body).
	if(result_callee && result_callee->kind == "id-expression") {
		const string qualified = RemoveMarker(result_callee->value);
		const size_t separator = qualified.rfind("::");
		if(separator != string::npos) {
			const string owner = qualified.substr(0, separator);
			const string member_name = LastComponent(qualified.substr(separator + 2));
			string qualified_owner = owner;
			if(!owner.empty()) qualified_owner = CanonicalSpelling(
				QualifyTypeArgument(owner, context));
			if(!qualified_owner.empty() && !member_name.empty() &&
				(specialization_bases_.find(LastComponent(qualified_owner)) !=
					specialization_bases_.end() || class_contexts_.find(qualified_owner) !=
					class_contexts_.end())) {
				CPPGMAstNodePtr object(new CPPGMAstNode("id-expression"));
				object->inferred_type = qualified_owner;
				CPPGMAstNodePtr synthetic_member(new CPPGMAstNode("member-expression", "."));
				synthetic_member->children.push_back(object);
				synthetic_member->children.push_back(CPPGMAstNodePtr(
					new CPPGMAstNode("identifier", member_name)));
				if(InstantiateMemberCall(result, synthetic_member, member_name,
					context, substitutions)) {
					result->children[0] = CPPGMAstNodePtr(new CPPGMAstNode(
						"id-expression", qualified_owner + "::" +
							synthetic_member->children[1]->value));
					result->template_instantiation = true;
					return result;
				}
			}
		}
	}
	bool constructor_replayed = false;
	string constructor_type;
	if(result_callee && result_callee->kind == "id-expression") {
		constructor_type = result_callee->value;
		// A constructor used as a functional cast is commonly unqualified inside
		// its class definition.  Resolve that spelling to the owning class before
		// asking the member-template index for a constructor specialization.
		if(constructor_type.find("::") == string::npos) {
			for(string current = context; ; ) {
				const string candidate = JoinPath(current, constructor_type);
				// FindClassDeclaration's short-name fallback can misidentify
				// `begin::iterator` as global `iterator`; replay needs an exact owner.
				if(class_contexts_.find(candidate) != class_contexts_.end() ||
					class_declarations_.find(candidate) != class_declarations_.end()) {
					constructor_type = candidate;
					break;
				}
				if(current.empty()) break;
				const size_t separator = current.rfind("::");
				if(separator == string::npos) current.clear();
				else current.erase(separator);
			}
		}
		// A class specialization can retain a member-template constructor until
		// its argument list supplies the missing template arguments.  Replay that
		// constructor through the same owner-aware member-template path used for
		// `object.member(args...)`; the synthetic object carries only a typed
		// semantic fact and is never emitted into the transformed AST.
			map<string, string>::const_iterator constructor_base =
				specialization_bases_.find(LastComponent(constructor_type));
			const bool constructor_candidate = constructor_base != specialization_bases_.end() ||
				class_contexts_.find(constructor_type) != class_contexts_.end() ||
				FindClassDeclaration(constructor_type, context);
			if(constructor_candidate) {
				CPPGMAstNodePtr synthetic_object(new CPPGMAstNode("id-expression"));
				synthetic_object->inferred_type = constructor_type;
				CPPGMAstNodePtr synthetic_member(new CPPGMAstNode("member-expression", "."));
				synthetic_member->children.push_back(synthetic_object);
				synthetic_member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
					"identifier", constructor_base == specialization_bases_.end() ?
						LastComponent(constructor_type) : LastComponent(constructor_base->second))));
				constructor_replayed = InstantiateMemberCall(result, synthetic_member,
					constructor_base == specialization_bases_.end() ?
						LastComponent(constructor_type) : LastComponent(constructor_base->second),
					context, substitutions);
			}
		CPPGMAstNodePtr operator_member(new CPPGMAstNode("member-expression", "."));
		operator_member->children.push_back(result_callee);
		operator_member->children.push_back(CPPGMAstNodePtr(
			new CPPGMAstNode("identifier", "operator()")));
		const vector<const TemplateDefinition*> named_functions =
			FindFunctionDefinitions(RemoveMarker(result_callee->value), context);
		bool visible_named_function = false;
		for(size_t named = 0; named < named_functions.size(); ++named) {
			const string owner = PrefixComponent(named_functions[named]->qualified_name);
			if(owner.empty() || context == owner || (context.size() > owner.size() &&
				context.compare(0, owner.size(), owner) == 0 && context[owner.size()] == ':')) {
				visible_named_function = true;
				break;
			}
		}
		if(!visible_named_function && InstantiateMemberCall(result, operator_member, "operator()", context,
			substitutions)) {
			result->children[0] = operator_member;
			result_callee = operator_member;
		}
	}
	if(result_callee && result_callee->kind == "call-expression") {
		// A braced functional construction such as `identity{}(value)` has a
		// call-expression as its callee.  Give member-template call operators the
		// same owner-aware materialization path as `object.operator()(value)`;
		// ordinary call operators remain available to PA14's callable-object path.
		CPPGMAstNodePtr operator_member(new CPPGMAstNode("member-expression", "."));
		operator_member->children.push_back(result_callee);
		operator_member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
			"identifier", "operator()")));
		if(InstantiateMemberCall(result, operator_member, "operator()", context,
			substitutions)) {
			result->children[0] = operator_member;
			result_callee = operator_member;
		}
	}
	if(input_callee && input_callee->kind == "id-expression" &&
		input_callee->value.compare(0, 7, "super::") == 0 &&
		result_callee && result_callee->kind == "id-expression") {
		string this_type;
		vector<const TemplateDefinition*> inherited;
		set<string> inherited_active;
		map<const TemplateDefinition*, string> inherited_owners;
		const CPPGMAstNodePtr this_expression(new CPPGMAstNode(
			"keyword-literal", "KW_THIS:this"));
		InferArgument(this_expression, &this_type, substitutions, context);
		CollectInheritedMemberTemplates(this_type, LastComponent(result_callee->value),
			substitutions, context, &inherited, &inherited_active, &inherited_owners);
		string base_owner;
		for(size_t inherited_index = 0; inherited_index < inherited.size(); ++inherited_index) {
			map<const TemplateDefinition*, string>::const_iterator owner =
				inherited_owners.find(inherited[inherited_index]);
			if(owner != inherited_owners.end() && !owner->second.empty()) {
				base_owner = owner->second;
				break;
			}
		}
		if(base_owner.empty()) {
			const CPPGMAstNodePtr declaration = FindClassDeclaration(this_type, context);
			if(declaration) for(size_t child = 0; child < declaration->children.size() &&
				base_owner.empty(); ++child) {
				const CPPGMAstNodePtr clause = declaration->children[child];
				if(!clause || clause->kind != "base-clause") continue;
				for(size_t base = 0; base < clause->children.size() && base_owner.empty(); ++base) {
					const CPPGMAstNodePtr base_name = ChildOfKindLocal(
						clause->children[base], "base-name");
					if(!base_name) continue;
					base_owner = CanonicalSpelling(ResolveAlias(ReplaceIdentifiers(
						RewriteText(base_name->value, context, substitutions, 0),
						substitutions), context));
				}
			}
		}
		CPPGMAstNodePtr base_object = this_expression;
		if(!base_owner.empty()) {
			CPPGMAstNodePtr type_id(new CPPGMAstNode("type-id"));
			CPPGMAstNodePtr specifiers(new CPPGMAstNode("type-specifier-seq"));
			specifiers->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
				"type-name", base_owner)));
			type_id->children.push_back(specifiers);
			CPPGMAstNodePtr abstract(new CPPGMAstNode("abstract-declarator"));
			abstract->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
				"ptr-operator", "OP_AMP:&")));
			type_id->children.push_back(abstract);
			CPPGMAstNodePtr cast(new CPPGMAstNode("cast-expression",
				"KW_STATIC_CAST:static_cast"));
			cast->children.push_back(type_id);
			CPPGMAstNodePtr dereference(new CPPGMAstNode("unary-expression", "OP_STAR:*"));
			dereference->children.push_back(this_expression);
			cast->children.push_back(dereference);
			base_object = cast;
		}
		CPPGMAstNodePtr base_member(new CPPGMAstNode("member-expression",
			base_owner.empty() ? "->" : "."));
		base_member->children.push_back(base_object);
		base_member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
			"identifier", LastComponent(result_callee->value))));
		const bool instantiated_base_call = InstantiateMemberCall(result, base_member,
			LastComponent(result_callee->value),
			context, substitutions);
		if(instantiated_base_call || !base_owner.empty()) {
			// A qualified base call is an implicit-this member call.  Preserve the
			// selected concrete owner in the callee so PA14 performs the required
			// base projection and does not redispatch the name against the current
			// derived specialization.
			if(!base_owner.empty()) {
				CPPGMAstNodePtr qualified_base(new CPPGMAstNode("id-expression",
					base_owner + "::" + LastComponent(result_callee->value)));
				result->children[0] = qualified_base;
				result_callee = qualified_base;
			} else {
				result->children[0] = base_member;
				result_callee = base_member;
			}
		}
	}
	if(result_callee && result_callee->kind == "parenthesized-expression" &&
		result_callee->children.size() == 1 && result_callee->children[0] &&
		result_callee->children[0]->kind == "call-expression") {
		CPPGMAstNodePtr operator_member(new CPPGMAstNode("member-expression", "."));
		operator_member->children.push_back(result_callee->children[0]);
		operator_member->children.push_back(CPPGMAstNodePtr(
			new CPPGMAstNode("identifier", "operator()")));
		result->children[0] = operator_member;
		result_callee = operator_member;
		InstantiateMemberCall(result, result_callee, "operator()", context, substitutions);
	}
	bool implicit_member_instantiated = false;
	string original_member;
	if(input_callee && input_callee->kind == "member-expression" &&
		input_callee->children.size() >= 2 && input_callee->children[1])
		original_member = input_callee->children[1]->value;
	ResolveMemberFunctionArguments(result, context, substitutions);
	if(result_callee && result_callee->kind == "member-expression")
		InstantiateMemberCall(result, result_callee, original_member, context, substitutions);
	ResolveMemberFunctionArguments(result, context, substitutions);
	if(result_callee && result_callee->kind == "id-expression" &&
		result_callee->value.find("::") == string::npos) {
		map<string, string>::const_iterator function_owner = function_owners_.find(context);
		bool member_function_context = !active_static_member_ &&
			function_owner != function_owners_.end() &&
			!function_owner->second.empty();
		if(!active_static_member_ && !member_function_context)
			for(string current = context; !current.empty(); ) {
			const TemplateDefinition* current_definition = FindDefinition(current, context);
			if(class_contexts_.find(current) != class_contexts_.end() ||
				(current_definition && current_definition->class_template)) {
				member_function_context = true;
				break;
			}
			const size_t separator = current.rfind("::");
			if(separator == string::npos) current.clear();
			else current.erase(separator);
		}
		if(member_function_context) {
			CPPGMAstNodePtr synthetic_object(new CPPGMAstNode("keyword-literal",
				"KW_THIS:this"));
			CPPGMAstNodePtr synthetic_member(new CPPGMAstNode("member-expression", "."));
			synthetic_member->children.push_back(synthetic_object);
			synthetic_member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
				"identifier", result_callee->value)));
			if(InstantiateMemberCall(result, synthetic_member, result_callee->value,
				context, substitutions)) {
				result_callee->value = synthetic_member->children[1]->value;
				implicit_member_instantiated = true;
			}
		}
	}
	if(!constructor_replayed && !implicit_member_instantiated && result_callee &&
		result_callee->kind == "id-expression" &&
		result_callee->value.find('<') == string::npos) {
			const string callee_name = result_callee->value;
			vector<const TemplateDefinition*> definitions =
				FindFunctionDefinitions(callee_name, context);
		map<const TemplateDefinition*, string> inherited_owners;
		const string qualified_callee_owner = PrefixComponent(callee_name);
		if(!qualified_callee_owner.empty()) {
			vector<const TemplateDefinition*> inherited;
			set<string> active;
			CollectInheritedMemberTemplates(qualified_callee_owner,
				LastComponent(callee_name), substitutions, context, &inherited,
				&active, &inherited_owners);
			for(size_t inherited_index = 0; inherited_index < inherited.size(); ++inherited_index)
				if(find(definitions.begin(), definitions.end(), inherited[inherited_index]) ==
					definitions.end()) definitions.push_back(inherited[inherited_index]);
		}
		const bool preserve_lookup_order = PreserveFunctionLookupOrder(definitions, context, substitutions);
		if(!preserve_lookup_order) SortFunctionTemplateCandidates(&definitions, context);
		if(!preserve_lookup_order && callee_name.compare(0, 8, "operator") != 0) RankFunctionTemplateCandidatesForCall(&definitions, result, context, substitutions);
		const bool inline_template_candidate = HasInlineTemplateCandidate(definitions, context);
		bool extern_template_candidate = false;
		for(size_t candidate = 0; candidate < definitions.size() && !extern_template_candidate;
			++candidate) {
			vector<string> inferred;
			try { if(!InferFunctionArguments(*definitions[candidate], result, &inferred, substitutions, context, 0)) continue; }
			catch(const PA18SubstitutionFailure&) { continue; }
			ostringstream request_key;
			request_key << definitions[candidate]->qualified_name << "@" <<
				definitions[candidate]->declaration.get();
			for(size_t argument = 0; argument < inferred.size(); ++argument)
				request_key << "|" << CanonicalSpelling(inferred[argument]);
			if(extern_instantiation_keys_.find(request_key.str()) !=
				extern_instantiation_keys_.end()) extern_template_candidate = true;
		}
		if(!HasMaterializedMemberFunction(callee_name, context) &&
			(!HasExactOrdinaryMatch(result, callee_name, substitutions, context) ||
				inline_template_candidate || extern_template_candidate))
			for(size_t candidate = 0; candidate < definitions.size(); ++candidate) {
				const TemplateDefinition* definition = definitions[candidate];
				if(definition->declaration && definition->declaration->kind == "simple-declaration") {
					bool has_definition = false;
					for(size_t other = 0; other < definitions.size(); ++other) {
						const TemplateDefinition* replacement = definitions[other];
						if(replacement == definition || !replacement->declaration ||
							replacement->declaration->kind != "function-definition") continue;
						if(MemberSignatureKey(*replacement) == MemberSignatureKey(*definition)) {
							has_definition = true;
							break;
						}
					}
					if(has_definition) continue;
				}
				vector<string> inferred; map<string, vector<string> > inferred_pack_values; map<string, FunctionSignature> inferred_function_values; map<string, vector<string> > forwarding_pack_values; bool inferred_ok = false;
				try { inferred_ok = InferFunctionArguments(*definition, result, &inferred, substitutions, context, 0, &inferred_pack_values, &inferred_function_values, 0, &forwarding_pack_values); }
				catch(const PA18SubstitutionFailure&) { inferred_ok = false; }
				if(!inferred_ok) continue;
				const TemplateDefinition* selected_definition =
					FindExplicitFunctionSpecialization(definition->qualified_name, inferred, context);
				if(!selected_definition) selected_definition = definition;
				string requested_owner_name = qualified_callee_owner;
				map<const TemplateDefinition*, string>::const_iterator inherited_owner =
					inherited_owners.find(selected_definition);
				if(inherited_owner != inherited_owners.end() && !inherited_owner->second.empty())
					requested_owner_name = inherited_owner->second;
				const bool concrete_member_owner = !requested_owner_name.empty() &&
					class_contexts_.find(requested_owner_name) != class_contexts_.end() &&
					specialization_bases_.find(LastComponent(requested_owner_name)) !=
					specialization_bases_.end() && !selected_definition->owner.empty();
				const string* requested_owner = concrete_member_owner ? &requested_owner_name : 0;
				try {
					const string local_name = Instantiate(*selected_definition, inferred, context, false,
						&inferred_pack_values, 0, requested_owner, &inferred_function_values,
						&forwarding_pack_values);
					string inferred_result_type;
						if(!selected_definition->owner.empty() && selected_definition->declaration &&
							!selected_definition->declaration->children.empty()) {
							map<string, string> return_substitutions = substitutions;
							for(size_t parameter = 0; parameter < selected_definition->parameters.size() &&
								parameter < inferred.size(); ++parameter)
								if(!selected_definition->parameters[parameter].name.empty())
									return_substitutions[selected_definition->parameters[parameter].name] =
										inferred[parameter];
							string return_type = NodeTypeSpelling(
								selected_definition->declaration->children[0]);
							return_type += ReturnDeclaratorSuffix(
								FunctionDeclarator(selected_definition->declaration));
							// Return-type inference replays the source declaration outside
							// EmitInstantiation's pack scope.  Install the typed function-pack
							// bindings here as well, so `holder<T...>` is expanded before a
							// dependent alias such as `alt_t<I, holder<T...>>` is resolved.
							const map<string, vector<string> > previous_return_packs =
								active_pack_substitutions_;
							for(map<string, vector<string> >::const_iterator pack =
								inferred_pack_values.begin(); pack != inferred_pack_values.end(); ++pack)
								if(!pack->first.empty()) active_pack_substitutions_[pack->first] =
									pack->second;
							try {
								inferred_result_type = CanonicalSpelling(ResolveAlias(RewriteText(
									return_type, context, return_substitutions, 0), context));
							} catch(...) {
								active_pack_substitutions_ = previous_return_packs;
								throw;
							}
							active_pack_substitutions_ = previous_return_packs;
						}
					const string qualifier = concrete_member_owner ? requested_owner_name :
						GeneratedFunctionQualifier(*definition, callee_name, context);
					const string emitted_name = concrete_member_owner ?
						LastComponent(selected_definition->name) : local_name;
					result->inferred_type = inferred_result_type;
					result->template_primary = definition->qualified_name;
					result->template_arguments = inferred;
					result_callee->value = qualifier.empty() ? emitted_name : qualifier +
						"::" + emitted_name;
					break;
				} catch(const PA18SubstitutionFailure&) { continue; }
			}
		if(definitions.empty()) {
			const FunctionSignature* signature = FindFunctionSignature(callee_name, context);
			if(signature && callee_name.find("::") == string::npos &&
				class_contexts_.find(context) == class_contexts_.end() &&
				!HasReplayContext(substitutions)) {
				map<string, vector<string> >::const_iterator names =
					function_signatures_by_name_.find(LastComponent(callee_name));
				if(names != function_signatures_by_name_.end())
					for(size_t name = 0; name < names->second.size(); ++name) {
						const string& qualified = names->second[name];
						map<string, FunctionSignature>::const_iterator found =
							function_signatures_.find(qualified);
						if(found != function_signatures_.end() && &found->second == signature &&
							class_contexts_.find(PrefixComponent(qualified)) == class_contexts_.end() &&
							function_contexts_.find(PrefixComponent(qualified)) == function_contexts_.end()) {
							result->children[0]->value = qualified;
							break;
						}
					}
			}
			ResolveFunctionArguments(result, signature, context);
		}
	}
	if(!result->children.empty() && result->children[0] &&
		result->children[0]->kind == "id-expression") {
		string& callee = result->children[0]->value;
		const size_t separator = callee.find("::");
		if(separator != string::npos) {
			const string owner = callee.substr(0, separator);
			if(callee.compare(separator + 2, owner.size() + 2, owner + "::") == 0)
				callee.erase(separator + 2, owner.size() + 2);
		}
	}
	if(result_callee && result_callee->kind == "member-expression" &&
		result_callee->children.size() >= 2 && result_callee->children[1]) {
		string member_object_type;
		string member_result_type;
		set<string> member_active;
		if(InferArgument(result_callee->children[0], &member_object_type,
			substitutions, context) && FindClassMemberType(member_object_type,
			LastComponent(result_callee->children[1]->value), substitutions, context,
			&member_result_type, &member_active) && !member_result_type.empty())
			result->inferred_type = member_result_type;
	}
	return result;
}
} // namespace pa18_templates_internal
