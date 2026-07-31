#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;
namespace pa18_templates_internal {

namespace {

string ConversionOperatorPattern(const TemplateDefinition& definition)
{
	if(!definition.declaration) return string();
	string raw = definition.declaration->value;
	if(raw.empty()) {
		const CPPGMAstNodePtr identifier = DescendantOfKind(definition.declaration,
			"identifier");
		if(identifier) raw = identifier->value;
	}
	const size_t operator_position = raw.find("operator");
	if(operator_position == string::npos) return string();
	string suffix = raw.substr(operator_position + 8);
	while(!suffix.empty() && suffix[0] == ' ') suffix.erase(0, 1);
	if(suffix.empty() || string("+-*/%^&|=!<>~[],()").find(suffix[0]) != string::npos)
		return string();
	return CanonicalSpelling(suffix);
}

string ConversionOwnerBase(const TemplateDefinition& definition)
{
	string owner = definition.owner;
	const size_t operator_position = owner.find("operator");
	if(operator_position != string::npos) owner.erase(operator_position);
	while(owner.size() >= 2 && owner.compare(owner.size() - 2, 2, "::") == 0)
		owner.erase(owner.size() - 2);
	for(;;) {
		const string prefix = PrefixComponent(owner);
		if(prefix.empty() || LastComponent(prefix) != LastComponent(owner)) break;
		owner = prefix;
	}
	return owner;
}

string NormalizedConversionMemberName(const TemplateDefinition& definition)
{
	string name = LastComponent(definition.name);
	const size_t operator_position = name.find("operator");
	if(operator_position == string::npos) return name;
	string suffix = name.substr(operator_position + 8);
	while(!suffix.empty() && isspace(static_cast<unsigned char>(suffix[0])))
		suffix.erase(suffix.begin());
	return name.substr(0, operator_position + 8) + suffix;
}

// `InferFunctionArguments` returns deduced pack elements flattened in
// template-parameter order. A pack inherited from an enclosing replay is
// intentionally absent from that vector, so reconstruct the raw argument
// stream with empty slots for those typed bindings. ResolveTemplateArguments
// then fills those slots from its pack hints without mistaking the first
// inferred inner-pack element for an enclosing one.
vector<string> BuildInstantiationRawArguments(
	const TemplateDefinition& definition, const vector<string>& member_arguments,
	const map<string, vector<string> >& inferred_pack_values,
	const map<string, vector<string> >& bound_pack_values)
{
	vector<string> result;
	size_t member_index = 0;
	for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter) {
		const TemplateParameter& detail = definition.parameters[parameter];
		if(detail.pack) {
			map<string, vector<string> >::const_iterator bound =
				bound_pack_values.find(detail.name);
			if(bound != bound_pack_values.end()) {
				result.insert(result.end(), bound->second.size(), string());
				continue;
			}
			map<string, vector<string> >::const_iterator inferred =
				inferred_pack_values.find(detail.name);
			const size_t count = inferred == inferred_pack_values.end() ? 0 :
				inferred->second.size();
			for(size_t element = 0; element < count; ++element)
				if(member_index < member_arguments.size())
					result.push_back(member_arguments[member_index++]);
		} else if(member_index < member_arguments.size())
			result.push_back(member_arguments[member_index++]);
	}
	// Keep unusual explicit/defaulted paths lossless when their completed
	// vector is not represented by inferred_pack_values.
	while(member_index < member_arguments.size())
		result.push_back(member_arguments[member_index++]);
	return result;
}

} // namespace

struct MemberCallState
{
	CPPGMAstNodePtr call, callee;
	string original_member, context, expected_result, member_name, member_qualifier;
	string object_type, qualified_owner;
	map<string, string> substitutions, member_substitutions;
	vector<string> explicit_member_arguments, parent_arguments;
	bool explicit_instantiation, constructor_replay, object_const, object_volatile;
	const TemplateDefinition* parent;
	const vector<TemplateParameter>* enclosing_parameters;
	vector<const TemplateDefinition*> candidates, direct_candidates;
	map<const TemplateDefinition*, string> inherited_owners;
	map<const TemplateDefinition*, size_t> candidate_occurrences;
	MemberCallState(const CPPGMAstNodePtr& c, const CPPGMAstNodePtr& x,
		const string& member, const string& where,
		const map<string, string>& bindings, bool explicit_call, bool replay)
		: call(c), callee(x), original_member(member), context(where),
		  expected_result(), member_name(), member_qualifier(), object_type(),
		  qualified_owner(), substitutions(bindings), member_substitutions(),
		  explicit_member_arguments(), parent_arguments(),
		  explicit_instantiation(explicit_call), constructor_replay(replay),
		  object_const(false), object_volatile(false), parent(0),
		  enclosing_parameters(0), candidates(), direct_candidates(),
		  inherited_owners(), candidate_occurrences() {}
};

struct MemberCallCandidateState
{
	MemberCallState* owner;
	const TemplateDefinition* definition;
	TemplateDefinition inference_definition, materialization_definition;
	bool direct_member, inferred, restored_function_defaults;
	vector<string> member_arguments, explicit_arguments, conversion_explicit_arguments,
		instantiation_member_arguments;
	map<string, vector<string> > forwarding_pack_values;
	map<string, string> candidate_substitutions, deduction_substitutions;
	map<string, vector<string> > inferred_pack_values, bound_pack_values;
	map<string, FunctionSignature> inferred_function_values;
	string concrete_candidate_owner, generated_name;
	MemberCallCandidateState(MemberCallState* value, const TemplateDefinition* item)
		: owner(value), definition(item), inference_definition(),
		  materialization_definition(), direct_member(false), inferred(false),
		  restored_function_defaults(false), member_arguments(), explicit_arguments(),
		  conversion_explicit_arguments(), instantiation_member_arguments(),
		  forwarding_pack_values(), candidate_substitutions(),
		  deduction_substitutions(), inferred_pack_values(), bound_pack_values(),
		  inferred_function_values(), concrete_candidate_owner(), generated_name() {}
};

bool PA18TemplateExpander::ReplayMemberCall(
	const CPPGMAstNodePtr& call, const CPPGMAstNodePtr& callee,
	const string& original_member, const string& context,
	const map<string, string>& substitutions, bool explicit_instantiation,
	bool constructor_replay)
{
	if(!call || !callee) return false;
	MemberCallState state(call, callee, original_member, context, substitutions,
		explicit_instantiation, constructor_replay);
	const bool parsed = ParseMemberCall(&state);
	const bool object_resolved = parsed && ResolveMemberObject(&state);
	const bool owner_resolved = object_resolved && ResolveMemberOwner(&state);
	const bool candidates_collected = owner_resolved && CollectMemberCallCandidates(&state);
	if(!parsed || !object_resolved || !owner_resolved || !candidates_collected)
		return false;
	for(size_t candidate = 0; candidate < state.candidates.size(); ++candidate) {
		const bool tried = TryMemberCandidate(&state, candidate);
		if(tried) return true;
	}
	return false;
}

bool PA18TemplateExpander::TryMemberCandidate(MemberCallState* state,
	size_t candidate_index)
{
	MemberCallCandidateState candidate(state, state->candidates[candidate_index]);
	if(!PrepareMemberCandidate(state, candidate_index, &candidate)) return false;
	if(!PrepareMemberCandidateArguments(&candidate)) return false;
	BindExpectedMemberConversion(&candidate);
	if(!DeduceMemberCandidate(&candidate)) return false;
	return EmitMemberCandidate(&candidate);
}

bool PA18TemplateExpander::ParseMemberCall(MemberCallState* state)
{
	const CPPGMAstNodePtr& call = state->call;
	const CPPGMAstNodePtr& callee = state->callee;
	const string& original_member = state->original_member;
	const string& context = state->context;
	const map<string, string>& substitutions = state->substitutions;

	if(!call || !callee || callee->kind != "member-expression" ||
		callee->children.size() < 2 || !callee->children[1]) return false;
	// Conversion-function templates are selected from the destination type,
	// rather than from ordinary call arguments.  Initializer replay records
	// that expected result on the synthetic call node; retain it before a
	// selected specialization overwrites `inferred_type` with its result.
	state->expected_result = call->inferred_type;
	string member_spelling = original_member.empty() ? callee->children[1]->value :
		original_member;
	member_spelling = RemoveMarker(member_spelling);
	member_spelling = CanonicalSpelling(member_spelling);
	if(member_spelling.empty()) return false;
	string& member_name = state->member_name;
	member_name = member_spelling;
	string& member_qualifier = state->member_qualifier;
	vector<string>& explicit_member_arguments = state->explicit_member_arguments;
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

	return true;
}

bool PA18TemplateExpander::ResolveMemberObject(MemberCallState* state)
{
	const CPPGMAstNodePtr& call = state->call;
	const CPPGMAstNodePtr& callee = state->callee;
	const string& original_member = state->original_member;
	const string& context = state->context;
	const map<string, string>& substitutions = state->substitutions;

	string& object_type = state->object_type;
	if(callee->children[0] && callee->children[0]->kind == "keyword-literal" &&
		RemoveMarker(callee->children[0]->value) == "this") {
		map<string, string>::const_iterator function_owner = function_owners_.find(context);
		if(function_owner != function_owners_.end()) object_type = function_owner->second;
		for(string current = object_type.empty() ? context : string(); !current.empty(); ) {
			const TemplateDefinition* current_definition = FindDefinition(current, context);
			if(class_contexts_.find(current) != class_contexts_.end() ||
				class_declarations_.find(current) != class_declarations_.end() ||
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
	state->object_const = object_const;
	state->object_volatile = object_volatile;

	return true;
}

bool PA18TemplateExpander::ResolveMemberOwner(MemberCallState* state)
{
	const CPPGMAstNodePtr& call = state->call;
	const string& context = state->context;
	const map<string, string>& substitutions = state->substitutions;
	const string& object_type = state->object_type;
	const string& member_qualifier = state->member_qualifier;
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
		if(parent->partial_specialization) {
			// `parent_arguments` are the primary template's arguments, while
			// member lookup in a selected partial specialization needs bindings
			// from its pattern (for example `function<R()>` must bind `R`).
			map<string, string> specialized;
			if(MatchClassSpecializationPattern(*parent, parent_arguments,
				&specialized, context))
				for(map<string, string>::const_iterator binding = specialized.begin();
					binding != specialized.end(); ++binding)
					if(!binding->second.empty())
						member_substitutions[binding->first] = binding->second;
		} else {
			for(size_t parameter = 0; parameter < enclosing_parameters->size() &&
				parameter < parent_arguments.size(); ++parameter)
				if(!(*enclosing_parameters)[parameter].name.empty() &&
					!(*enclosing_parameters)[parameter].pack)
					member_substitutions[(*enclosing_parameters)[parameter].name] =
						parent_arguments[parameter];
		}
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


	state->parent = parent;
	state->parent_arguments = parent_arguments;
	state->member_substitutions = member_substitutions;
	state->enclosing_parameters = enclosing_parameters;
	state->qualified_owner = qualified_owner;

	return true;
}

bool PA18TemplateExpander::CollectMemberCallCandidates(MemberCallState* state)
{
	const string& member_qualifier = state->member_qualifier;
	const string& qualified_owner = state->qualified_owner;
	const bool constructor_replay = state->constructor_replay;
	const CPPGMAstNodePtr& call = state->call;
	const string& context = state->context;
	const map<string, string>& substitutions = state->substitutions;
	const string& object_type = state->object_type;
	const string& member_name = state->member_name;
	const bool object_const = state->object_const;
	const bool object_volatile = state->object_volatile;
	const TemplateDefinition* parent = state->parent;
	const vector<string>& parent_arguments = state->parent_arguments;
	const vector<TemplateParameter>* enclosing_parameters = state->enclosing_parameters;
	const map<string, string>& member_substitutions = state->member_substitutions;

	vector<const TemplateDefinition*>& candidates = state->candidates;
	map<string, vector<string> >::const_iterator indexed_members =
		definitions_by_name_.find(member_name);
	if(indexed_members != definitions_by_name_.end()) for(size_t indexed = 0;
		indexed < indexed_members->second.size(); ++indexed) {
		map<string, TemplateDefinition>::const_iterator it = definitions_.find(
			indexed_members->second[indexed]);
		if(it == definitions_.end()) continue;
		const TemplateDefinition& definition = it->second;
		string definition_member_base = LastComponent(definition.name);
		const size_t definition_member_angle = definition_member_base.find('<');
		if(definition_member_angle != string::npos)
			definition_member_base.erase(definition_member_angle);
		if(definition.class_template || definition.alias_template ||
			definition.variable_template || definition.parameters.empty() ||
			(definition_member_base != member_name &&
			 LastComponent(definition.name) != member_name &&
			 NormalizedConversionMemberName(definition) != member_name) ||
			!definition.declaration)
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
	// Qualified conversion targets contain `::` and the collection index stores
	// the target's final component as the member name (for example `view<U>`),
	// while the owner string retains the `operator target_ns` prefix.  Recover
	// those candidates from their typed declarators when ordinary member lookup
	// did not see the split spelling.
	for(map<string, TemplateDefinition>::const_iterator candidate = definitions_.begin();
		candidate != definitions_.end(); ++candidate) {
		const TemplateDefinition& definition = candidate->second;
		if(!definition.member_template || definition.parameters.empty() ||
			find(candidates.begin(), candidates.end(), &definition) != candidates.end()) continue;
		const string conversion_pattern = ConversionOperatorPattern(definition);
		string definition_member_base = LastComponent(definition.name);
		const size_t definition_member_angle = definition_member_base.find('<');
		if(definition_member_angle != string::npos)
			definition_member_base.erase(definition_member_angle);
		string conversion_member_base = conversion_pattern;
		const size_t conversion_member_angle = conversion_member_base.find('<');
		if(conversion_member_angle != string::npos)
			conversion_member_base.erase(conversion_member_angle);
		if(conversion_pattern.empty() || (definition_member_base != member_name &&
			LastComponent(definition.name) != member_name &&
			NormalizedConversionMemberName(definition) != member_name &&
			LastComponent(conversion_member_base) != member_name)) continue;
		string owner = ConversionOwnerBase(definition);
		const size_t owner_angle = owner.find('<');
		if(owner_angle != string::npos) owner.erase(owner_angle);
		string object_base = object_type;
		map<string, CPPGMAstNodePtr>::const_iterator object_declaration =
			class_declarations_.find(object_type);
		if(object_declaration != class_declarations_.end() && object_declaration->second &&
			!object_declaration->second->template_primary.empty())
			object_base = object_declaration->second->template_primary;
		else {
			map<string, string>::const_iterator object_specialization =
				specialization_bases_.find(LastComponent(object_type));
			if(object_specialization != specialization_bases_.end())
				object_base = object_specialization->second;
		}
		if(LastComponent(owner) != LastComponent(object_base)) continue;
		candidates.push_back(&definition);
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
	state->direct_candidates = candidates;
	const vector<const TemplateDefinition*>& direct_member_candidates = state->direct_candidates;
	const bool ordinary_callable = HasViableOrdinaryCallableMember(call, object_type, member_name,
		context, substitutions, object_const, object_volatile, constructor_replay);
	// Constructor overload resolution must see member-template constructors
	// alongside ordinary constructors.  A concrete fallback such as `long`
	// cannot suppress a template constructor whose substituted parameter is an
	// exact `int` match; ordinary member calls retain the fast path because
	// their non-template body is already sufficient.
	if(ordinary_callable && !constructor_replay) return false;
	vector<const TemplateDefinition*> inherited_candidates;
	set<string> inherited_active;
	map<const TemplateDefinition*, string> inherited_owners;
	CollectInheritedMemberTemplates(object_type, member_name, member_substitutions,
		context, &inherited_candidates, &inherited_active, &inherited_owners);
	for(size_t inherited = 0; inherited < inherited_candidates.size(); ++inherited)
		if(find(candidates.begin(), candidates.end(), inherited_candidates[inherited]) ==
			candidates.end() || inherited_owners.find(inherited_candidates[inherited]) !=
			inherited_owners.end()) candidates.push_back(inherited_candidates[inherited]);
	const auto has_ellipsis_parameter = [this](const TemplateDefinition* definition) {
		if(!definition || !definition->declaration) return false;
		const CPPGMAstNodePtr parameters = DescendantOfKind(
			FunctionDeclarator(definition->declaration), "parameter-clause");
		if(!parameters) return false;
		for(size_t parameter = 0; parameter < parameters->children.size(); ++parameter)
			if(parameters->children[parameter] &&
				parameters->children[parameter]->kind == "ellipsis") return true;
		return false;
	};
	stable_sort(candidates.begin(), candidates.end(), [this, &context, &has_ellipsis_parameter](const TemplateDefinition* left,
		const TemplateDefinition* right) {
		const bool left_ellipsis = has_ellipsis_parameter(left);
		const bool right_ellipsis = has_ellipsis_parameter(right);
		if(left_ellipsis != right_ellipsis) return !left_ellipsis;
		const bool left_more = left && right && FunctionTemplateMoreSpecialized(
			*left, *right, context);
		const bool right_more = left && right && FunctionTemplateMoreSpecialized(
			*right, *left, context);
		if(left_more != right_more) return left_more;
		const int left_score = MemberTemplatePatternScore(left);
		const int right_score = MemberTemplatePatternScore(right);
		if(left_score != right_score) return left_score > right_score;
		if(left->parameters.size() != right->parameters.size())
			return left->parameters.size() < right->parameters.size();
		return false;
	});
	if(candidates.empty()) return false;
	RankMemberCandidatesByClassExactness(&candidates, call, member_substitutions, context);
	state->inherited_owners = inherited_owners;

	return true;
}

bool PA18TemplateExpander::PrepareMemberCandidate(MemberCallState* state,
	size_t candidate_index, MemberCallCandidateState* candidate)
{
	const CPPGMAstNodePtr& call = candidate->owner->call;
	const CPPGMAstNodePtr& callee = candidate->owner->callee;
	const string& context = candidate->owner->context;
	const map<string, string>& substitutions = candidate->owner->substitutions;
	const string& object_type = candidate->owner->object_type;
	const string& member_name = candidate->owner->member_name;
	const string& member_qualifier = candidate->owner->member_qualifier;
	const string& expected_result = candidate->owner->expected_result;
	const bool object_const = candidate->owner->object_const;
	const bool object_volatile = candidate->owner->object_volatile;
	const bool explicit_instantiation = candidate->owner->explicit_instantiation;
	const TemplateDefinition* parent = candidate->owner->parent;
	const vector<string>& parent_arguments = candidate->owner->parent_arguments;
	const vector<TemplateParameter>* enclosing_parameters = candidate->owner->enclosing_parameters;
	const vector<const TemplateDefinition*>& candidates = candidate->owner->candidates;
	const vector<const TemplateDefinition*>& direct_member_candidates = candidate->owner->direct_candidates;
	const map<const TemplateDefinition*, string>& inherited_owners = candidate->owner->inherited_owners;
	map<string, string>& member_substitutions = candidate->owner->member_substitutions;

		const TemplateDefinition& definition = *candidate->definition;
		if(object_const || object_volatile) {
			const string qualifiers = DeclaratorSuffix(FunctionDeclarator(
				definition.declaration));
			const bool function_const = qualifiers.find("const") != string::npos;
			const bool function_volatile = qualifiers.find("volatile") != string::npos;
			if(object_const && !function_const) return false;
			if(object_volatile && !function_volatile) return false;
		}
		candidate->inference_definition = definition;
		TemplateDefinition& inference_definition = candidate->inference_definition;
		RestoreMemberTemplateDefaults(member_name, definition, &inference_definition);
		const size_t occurrence = state->candidate_occurrences[&definition]++;
		candidate->direct_member = occurrence == 0 &&
			find(direct_member_candidates.begin(), direct_member_candidates.end(),
					&definition) != direct_member_candidates.end();
		const bool direct_member = candidate->direct_member;
		map<string, string>& candidate_substitutions = candidate->candidate_substitutions;
		candidate_substitutions = state->member_substitutions;
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
				string alias_type = DeclaratorTypeSpelling(
					NodeTypeSpelling(declaration->children[0]), entry->children[0]);
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
			if(has_definition) return false;
		}
		if(definition.declaration->kind == "special-member-declaration") {
			bool has_definition = false;
			for(size_t other_index = 0; other_index < candidates.size(); ++other_index) {
				const TemplateDefinition& other = *candidates[other_index];
				if(other.declaration->kind != "special-member-definition") continue;
				string other_owner = LastComponent(ConversionOwnerBase(other));
				string definition_owner = LastComponent(ConversionOwnerBase(definition));
				const size_t other_angle = other_owner.find('<');
				const size_t definition_angle = definition_owner.find('<');
				if(other_angle != string::npos) other_owner.erase(other_angle);
				if(definition_angle != string::npos) definition_owner.erase(definition_angle);
				if(MemberSignatureKey(other) == MemberSignatureKey(definition) ||
					(ConversionOperatorPattern(other) == ConversionOperatorPattern(definition) &&
					 other_owner == definition_owner)) {
					has_definition = true;
					break;
				}
			}
			if(has_definition) return false;
		}

	return true;
}

bool PA18TemplateExpander::PrepareMemberCandidateArguments(
	MemberCallCandidateState* candidate)
{
	const TemplateDefinition& definition = *candidate->definition;
	const map<string, string>& candidate_substitutions = candidate->candidate_substitutions;
	const CPPGMAstNodePtr& call = candidate->owner->call;
	const CPPGMAstNodePtr& callee = candidate->owner->callee;
	const string& context = candidate->owner->context;
	const map<string, string>& substitutions = candidate->owner->substitutions;
	const string& object_type = candidate->owner->object_type;
	const string& member_name = candidate->owner->member_name;
	const string& member_qualifier = candidate->owner->member_qualifier;
	const string& expected_result = candidate->owner->expected_result;
	const bool object_const = candidate->owner->object_const;
	const bool object_volatile = candidate->owner->object_volatile;
	const bool explicit_instantiation = candidate->owner->explicit_instantiation;
	const TemplateDefinition* parent = candidate->owner->parent;
	const vector<string>& parent_arguments = candidate->owner->parent_arguments;
	const vector<TemplateParameter>* enclosing_parameters = candidate->owner->enclosing_parameters;
	const vector<const TemplateDefinition*>& candidates = candidate->owner->candidates;
	const vector<const TemplateDefinition*>& direct_member_candidates = candidate->owner->direct_candidates;
	const map<const TemplateDefinition*, string>& inherited_owners = candidate->owner->inherited_owners;
	map<string, string>& member_substitutions = candidate->owner->member_substitutions;

		vector<string>& member_arguments = candidate->member_arguments;
		map<string, vector<string> >& inferred_pack_values = candidate->inferred_pack_values;
		map<string, FunctionSignature>& inferred_function_values = candidate->inferred_function_values;
		vector<string>& explicit_arguments = candidate->explicit_arguments;
		explicit_arguments = candidate->owner->explicit_member_arguments;
		for(size_t argument = 0; argument < explicit_arguments.size(); ++argument) {
			explicit_arguments[argument] = NormalizeTypeArgument(RewriteText(
				explicit_arguments[argument], context, candidate_substitutions, 0));
			explicit_arguments[argument] = NormalizeTypeArgument(ReplaceIdentifiers(
				explicit_arguments[argument], candidate_substitutions));
			explicit_arguments[argument] = ResolveAlias(explicit_arguments[argument], context);
			explicit_arguments[argument] = QualifyTypeArgument(
				explicit_arguments[argument], context, definition.owner);
		}
		candidate->inferred = false;
		bool& inferred = candidate->inferred;
		map<string, string>& deduction_substitutions = candidate->deduction_substitutions;
		deduction_substitutions = candidate_substitutions;
		// Preserve concrete enclosing template bindings while replaying the
		// selected member's call-site argument types.  They provide typed owner
		// facts such as `String::const_iterator` without importing unrelated names.
		for(map<string, string>::const_iterator outer = substitutions.begin();
			outer != substitutions.end(); ++outer) {
			if(deduction_substitutions.find(outer->first) != deduction_substitutions.end())
				continue;
			const string concrete = CanonicalSpelling(ResolveAlias(outer->second, context));
			if(IsKnownTypeSpelling(concrete, context))
				deduction_substitutions[outer->first] = outer->second;
		}
		if(parent && !parent->name.empty())
			// The enclosing class name is useful while replaying the generated
			// member body, but it is not a template parameter.  Leaving it in the
			// deduction map makes textual substitution turn `iter<Buff, T>` into
			// `iter_<concrete-args><Buff, T>` before matching the member pattern.
			deduction_substitutions.erase(parent->name);
		map<string, vector<string> >& bound_pack_values = candidate->bound_pack_values;
		if(parent) {
			if(parent->partial_specialization) {
				// The concrete owner is keyed by the primary class's raw argument
				// list (for `Box<R(Args...)>` that is one function type), while a
				// member body needs the partial pattern's typed bindings.
				map<string, string> specialized;
				if(MatchClassSpecializationPattern(*parent, parent_arguments,
					&specialized, context)) {
					for(map<string, string>::const_iterator binding = specialized.begin();
						binding != specialized.end(); ++binding)
						if(!binding->second.empty())
							member_substitutions[binding->first] = binding->second;
						for(size_t pack = 0; pack < parent->specialization_pack_names.size();
							++pack) {
							const string& name = parent->specialization_pack_names[pack];
							if(name.empty()) continue;
							map<string, string>::const_iterator binding = specialized.find(name);
							if(binding == specialized.end()) continue;
							// Preserve an empty enclosing pack as a typed fact too.  If
							// the member reuses that spelling for a function pack, the
							// empty owner pack must clear the surrounding specialization's
							// active values during body replay.
							bound_pack_values[name] = binding->second.empty() ?
								vector<string>() : SplitTemplateArguments(binding->second);
					}
				}
			} else {
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
		}

	return true;
}

void PA18TemplateExpander::BindExpectedMemberConversion(
	MemberCallCandidateState* candidate)
{
	const TemplateDefinition& definition = *candidate->definition;
	map<string, string>& candidate_substitutions = candidate->candidate_substitutions;
	map<string, string>& deduction_substitutions = candidate->deduction_substitutions;
	const CPPGMAstNodePtr& call = candidate->owner->call;
	const CPPGMAstNodePtr& callee = candidate->owner->callee;
	const string& context = candidate->owner->context;
	const map<string, string>& substitutions = candidate->owner->substitutions;
	const string& object_type = candidate->owner->object_type;
	const string& member_name = candidate->owner->member_name;
	const string& member_qualifier = candidate->owner->member_qualifier;
	const string& expected_result = candidate->owner->expected_result;
	const bool object_const = candidate->owner->object_const;
	const bool object_volatile = candidate->owner->object_volatile;
	const bool explicit_instantiation = candidate->owner->explicit_instantiation;
	const TemplateDefinition* parent = candidate->owner->parent;
	const vector<string>& parent_arguments = candidate->owner->parent_arguments;
	const vector<TemplateParameter>* enclosing_parameters = candidate->owner->enclosing_parameters;
	const vector<const TemplateDefinition*>& candidates = candidate->owner->candidates;
	const vector<const TemplateDefinition*>& direct_member_candidates = candidate->owner->direct_candidates;
	const map<const TemplateDefinition*, string>& inherited_owners = candidate->owner->inherited_owners;
	map<string, string>& member_substitutions = candidate->owner->member_substitutions;

		vector<string>& conversion_explicit_arguments = candidate->conversion_explicit_arguments;
		const string conversion_pattern = ConversionOperatorPattern(definition);
		const bool conversion_operator = !conversion_pattern.empty();
		if(!expected_result.empty() && conversion_operator &&
			definition.member_template && definition.declaration &&
			!definition.declaration->children.empty()) {
			// The parser stores the conversion target as part of the declarator;
			// the first child of an out-of-class conversion definition is its
			// trailing cv-qualifier (`const`), not a normal return decl-specifier.
			// Recover the typed target directly from the conversion operator name.
				string result_pattern = conversion_pattern;
			try {
				result_pattern = CanonicalSpelling(ResolveAlias(RewriteText(
					result_pattern, context, candidate_substitutions, 0), context));
				set<string> parameter_names;
				for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter)
					if(!definition.parameters[parameter].name.empty())
						parameter_names.insert(definition.parameters[parameter].name);
				map<string, string> result_bindings;
				string matching_expected_result = expected_result;
				string expected_prefix;
				while(matching_expected_result.compare(0, 6, "const ") == 0 ||
					matching_expected_result.compare(0, 9, "volatile ") == 0) {
					const size_t space = matching_expected_result.find(' ');
					expected_prefix += matching_expected_result.substr(0, space + 1);
					matching_expected_result = CanonicalSpelling(matching_expected_result.substr(space + 1));
				}
				string expected_suffix;
				while(!matching_expected_result.empty() &&
					(matching_expected_result[matching_expected_result.size() - 1] == '&' ||
					 matching_expected_result[matching_expected_result.size() - 1] == '*')) {
					expected_suffix = string(1, matching_expected_result[matching_expected_result.size() - 1]) +
						expected_suffix;
					matching_expected_result = CanonicalSpelling(matching_expected_result.substr(0,
						matching_expected_result.size() - 1));
				}
				string expected_base;
				vector<string> expected_template_arguments;
				map<string, CPPGMAstNodePtr>::const_iterator expected_class =
					class_declarations_.find(matching_expected_result);
				if(expected_class != class_declarations_.end() && expected_class->second &&
					!expected_class->second->template_primary.empty()) {
					expected_base = expected_class->second->template_primary;
					expected_template_arguments = expected_class->second->template_arguments;
				} else for(map<string, string>::const_iterator candidate = specialization_bases_.begin();
					candidate != specialization_bases_.end(); ++candidate) {
					if(candidate->first != LastComponent(matching_expected_result) ||
						(LastComponent(candidate->second) != LastComponent(matching_expected_result) &&
						 PrefixComponent(candidate->second) != PrefixComponent(matching_expected_result))) continue;
					expected_base = candidate->second;
					map<string, vector<string> >::const_iterator arguments =
						specialization_arguments_.find(candidate->first);
					if(arguments != specialization_arguments_.end())
						expected_template_arguments = arguments->second;
					break;
				}
				if(!expected_base.empty() && !expected_template_arguments.empty()) {
					string concrete_expected = expected_base + "<";
					for(size_t argument = 0; argument < expected_template_arguments.size(); ++argument) {
						if(argument) concrete_expected += ",";
						concrete_expected += expected_template_arguments[argument];
					}
					concrete_expected += ">";
					matching_expected_result = expected_prefix + concrete_expected + expected_suffix;
				} else matching_expected_result = expected_prefix + matching_expected_result + expected_suffix;
				if(MatchTypePattern(result_pattern, matching_expected_result, parameter_names,
					&result_bindings, context)) {
					for(map<string, string>::const_iterator binding = result_bindings.begin();
						binding != result_bindings.end(); ++binding)
						deduction_substitutions[binding->first] = binding->second;
					for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter) {
						const string& name = definition.parameters[parameter].name;
						map<string, string>::const_iterator binding = result_bindings.find(name);
						if(binding == result_bindings.end()) break;
						conversion_explicit_arguments.push_back(NormalizeTypeArgument(
							ResolveAlias(binding->second, context)));
					}
				}
			} catch(const PA18SubstitutionFailure&) {}
		}

}

bool PA18TemplateExpander::DeduceMemberCandidate(
	MemberCallCandidateState* candidate)
{
	const TemplateDefinition& definition = *candidate->definition;
	TemplateDefinition& inference_definition = candidate->inference_definition;
	vector<string>& member_arguments = candidate->member_arguments;
	vector<string>& explicit_arguments = candidate->explicit_arguments;
	vector<string>& conversion_explicit_arguments = candidate->conversion_explicit_arguments;
	bool& inferred = candidate->inferred;
	map<string, string>& candidate_substitutions = candidate->candidate_substitutions;
	map<string, string>& deduction_substitutions = candidate->deduction_substitutions;
	map<string, vector<string> >& inferred_pack_values = candidate->inferred_pack_values;
	map<string, vector<string> >& bound_pack_values = candidate->bound_pack_values;
	map<string, FunctionSignature>& inferred_function_values = candidate->inferred_function_values;
	const CPPGMAstNodePtr& call = candidate->owner->call;
	const CPPGMAstNodePtr& callee = candidate->owner->callee;
	const string& context = candidate->owner->context;
	const map<string, string>& substitutions = candidate->owner->substitutions;
	const string& object_type = candidate->owner->object_type;
	const string& member_name = candidate->owner->member_name;
	const string& member_qualifier = candidate->owner->member_qualifier;
	const string& expected_result = candidate->owner->expected_result;
	const bool object_const = candidate->owner->object_const;
	const bool object_volatile = candidate->owner->object_volatile;
	const bool explicit_instantiation = candidate->owner->explicit_instantiation;
	const TemplateDefinition* parent = candidate->owner->parent;
	const vector<string>& parent_arguments = candidate->owner->parent_arguments;
	const vector<TemplateParameter>* enclosing_parameters = candidate->owner->enclosing_parameters;
	const vector<const TemplateDefinition*>& candidates = candidate->owner->candidates;
	const vector<const TemplateDefinition*>& direct_member_candidates = candidate->owner->direct_candidates;
	const map<const TemplateDefinition*, string>& inherited_owners = candidate->owner->inherited_owners;
	map<string, string>& member_substitutions = candidate->owner->member_substitutions;

	map<string, vector<string> >& forwarding_pack_values = candidate->forwarding_pack_values;
	candidate->materialization_definition = inference_definition;
	TemplateDefinition& materialization_definition = candidate->materialization_definition;
	candidate->restored_function_defaults = RestoreFunctionParameterDefaults(
		inference_definition, &materialization_definition);
	const bool restored_function_defaults = candidate->restored_function_defaults;
	const vector<string>* explicit_prefix = !conversion_explicit_arguments.empty() ?
		&conversion_explicit_arguments : (explicit_arguments.empty() ? 0 :
		&explicit_arguments);
	try {
		inferred = InferFunctionArguments(inference_definition, call, &member_arguments,
				deduction_substitutions, context, explicit_prefix, &inferred_pack_values,
				&inferred_function_values, &bound_pack_values, &forwarding_pack_values);
		} catch(const logic_error&) {
			inferred = false;
		}
		// The active enclosing pack is a typed replay fact for materialization.
		// Leave deduction free to inspect the concrete call arguments first: an
		// inner template may reuse the enclosing pack's spelling, while its own
		// pack still needs to be inferred from the call.
		for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter) {
			const TemplateParameter& detail = definition.parameters[parameter];
			if(!detail.pack || detail.name.empty() ||
				bound_pack_values.find(detail.name) != bound_pack_values.end()) continue;
			map<string, vector<string> >::const_iterator active =
				active_pack_substitutions_.find(detail.name);
			map<string, vector<string> >::const_iterator inferred_pack =
				inferred_pack_values.find(detail.name);
			if(active != active_pack_substitutions_.end() && !active->second.empty() &&
				(inferred_pack == inferred_pack_values.end() || inferred_pack->second.empty()))
				bound_pack_values[detail.name] = active->second;
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
		if(!inferred) return false;
		// Member-template deduction can produce a syntactically complete
		// argument vector while a defaulted enable-if is not viable.  Validate
		// that candidate before materialization; otherwise a constructor body
		// recursively selects itself (the inherited-constructor `tag` case).
	if(!ValidateTemplateDefaults(inference_definition, member_arguments, context,
			deduction_substitutions)) {
		return false;
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
		// Constructor templates commonly carry dependent default template
		// arguments (for example a pack-size predicate and an enable-if alias).
		// Once the object type and call arguments are concrete, those defaults are
		// resolved by Instantiate; their source spelling must not discard the
		// otherwise viable constructor before replay reaches that substitution
		// boundary.
		const bool constructor_template = !definition.owner.empty() &&
			LastComponent(definition.name) == LastComponent(definition.owner);
	if(dependent_member_arguments && !constructor_template) {
		return false;
	}
		vector<string>& instantiation_member_arguments = candidate->instantiation_member_arguments;
	instantiation_member_arguments = member_arguments;
		if(constructor_template && explicit_arguments.empty() &&
			!candidate->owner->constructor_replay) {
			size_t supplied = 0;
			for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter) {
				const TemplateParameter& formal = definition.parameters[parameter];
				if(!formal.default_type.empty()) break;
				if(formal.pack) {
					map<string, vector<string> >::const_iterator values =
						inferred_pack_values.find(formal.name);
					if(values != inferred_pack_values.end()) supplied += values->second.size();
				} else ++supplied;
			}
			if(supplied < instantiation_member_arguments.size())
				instantiation_member_arguments.resize(supplied);
		}


	return true;
}

bool PA18TemplateExpander::EmitMemberCandidate(
	MemberCallCandidateState* candidate)
{
	const TemplateDefinition& definition = *candidate->definition;
	const bool direct_member = candidate->direct_member;
	TemplateDefinition& inference_definition = candidate->inference_definition;
	TemplateDefinition& materialization_definition = candidate->materialization_definition;
	const bool restored_function_defaults = candidate->restored_function_defaults;
	vector<string>& member_arguments = candidate->member_arguments;
	vector<string>& instantiation_member_arguments = candidate->instantiation_member_arguments;
	map<string, vector<string> >& forwarding_pack_values = candidate->forwarding_pack_values;
	map<string, string>& candidate_substitutions = candidate->candidate_substitutions;
	map<string, vector<string> >& inferred_pack_values = candidate->inferred_pack_values;
	map<string, vector<string> >& bound_pack_values = candidate->bound_pack_values;
	map<string, FunctionSignature>& inferred_function_values = candidate->inferred_function_values;
	const CPPGMAstNodePtr& call = candidate->owner->call;
	const CPPGMAstNodePtr& callee = candidate->owner->callee;
	const string& context = candidate->owner->context;
	const map<string, string>& substitutions = candidate->owner->substitutions;
	const string& object_type = candidate->owner->object_type;
	const string& member_name = candidate->owner->member_name;
	const string& member_qualifier = candidate->owner->member_qualifier;
	const string& expected_result = candidate->owner->expected_result;
	const bool object_const = candidate->owner->object_const;
	const bool object_volatile = candidate->owner->object_volatile;
	const bool explicit_instantiation = candidate->owner->explicit_instantiation;
	const TemplateDefinition* parent = candidate->owner->parent;
	const vector<string>& parent_arguments = candidate->owner->parent_arguments;
	const vector<TemplateParameter>* enclosing_parameters = candidate->owner->enclosing_parameters;
	const vector<const TemplateDefinition*>& candidates = candidate->owner->candidates;
	const vector<const TemplateDefinition*>& direct_member_candidates = candidate->owner->direct_candidates;
	const map<const TemplateDefinition*, string>& inherited_owners = candidate->owner->inherited_owners;
	map<string, string>& member_substitutions = candidate->owner->member_substitutions;

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
				// The enclosing class binding is more specific than a same-named
				// pack inferred from the member call's arguments.
				instantiation_pack_hints[bound->first] = bound->second;
		vector<string> raw_instantiation_arguments = BuildInstantiationRawArguments(
			inference_definition, instantiation_member_arguments, inferred_pack_values,
			bound_pack_values);
		// Non-template members are materialized through this call path rather than
		// through the enclosing class replay.  Preserve the enclosing class's
		// typed packs for their bodies (`index_sequence_for<A...>` in a member of
		// `list<A...>`); the member's own argument vector contains no `A...` entry
		// from which Instantiate could recover them.
		if(concrete_owner) {
			map<string, string>::const_iterator owner_base = specialization_bases_.find(
				LastComponent(requested_owner));
			map<string, vector<string> >::const_iterator owner_args =
				specialization_arguments_.find(LastComponent(requested_owner));
			if(owner_base != specialization_bases_.end() &&
				owner_args != specialization_arguments_.end()) {
				const TemplateDefinition* owner_definition = FindDefinition(
					owner_base->second, context);
				if(owner_definition && owner_definition->class_template) {
					size_t owner_argument = 0;
					for(size_t parameter = 0; parameter < owner_definition->parameters.size();
						++parameter) {
						const TemplateParameter& detail = owner_definition->parameters[parameter];
						if(detail.pack) {
							size_t trailing_fixed = 0;
							for(size_t later = parameter + 1;
								later < owner_definition->parameters.size(); ++later)
								if(!owner_definition->parameters[later].pack) ++trailing_fixed;
							const size_t available = owner_args->second.size() > owner_argument ?
								owner_args->second.size() - owner_argument : 0;
							const size_t count = available > trailing_fixed ?
								available - trailing_fixed : 0;
							if(!detail.name.empty() &&
								instantiation_pack_hints.find(detail.name) ==
								instantiation_pack_hints.end()) {
								vector<string>& values = instantiation_pack_hints[detail.name];
								for(size_t element = 0; element < count; ++element)
									values.push_back(owner_args->second[owner_argument + element]);
							}
							owner_argument += count;
						} else if(owner_argument < owner_args->second.size()) ++owner_argument;
					}
				}
			}
		}
		string& generated_name = candidate->generated_name;
	const ConcreteOwnerContext previous_concrete_owner = active_concrete_owner_;
		if(requested_owner_pointer) SetActiveConcreteOwner(requested_owner, context);
		try {
			generated_name = Instantiate(restored_function_defaults ? materialization_definition :
				inference_definition, raw_instantiation_arguments, context,
				 explicit_instantiation, &instantiation_pack_hints, &candidate_substitutions,
				 requested_owner_pointer, &inferred_function_values,
					 &forwarding_pack_values);
		} catch(const logic_error&) {
			active_concrete_owner_ = previous_concrete_owner;
			return false;
		}
		active_concrete_owner_ = previous_concrete_owner;
		call->template_primary = definition.qualified_name;
		call->template_arguments = instantiation_member_arguments;
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
			const string result_context = definition.owner.empty() ? context : definition.owner;
			try {
				result_type = CanonicalSpelling(ResolveAlias(RewriteText(result_type, result_context,
					result_substitutions, 0), result_context));
				result_type = QualifyTypeArgument(result_type, result_context, result_context, true);
			} catch(const logic_error&) {
				return false;
			}
			call->inferred_type = result_type;
		}
	const bool static_member = definition.static_member;
		const bool generated_operator = member_name.compare(0, 8, "operator") == 0;
	const bool ordinary_class_member = !definition.owner.empty() &&
			definition.owner.find('<') == string::npos &&
			FindClassDeclaration(definition.owner, context) != CPPGMAstNodePtr() &&
			!definition.member_template;
	const string emitted_member_name = definition.member_template ? generated_name : member_name;
		if(static_member && definition.owner.find("::") == string::npos && concrete_owner) {
			call->children[0] = CPPGMAstNodePtr(new CPPGMAstNode("id-expression",
				requested_owner + "::" + emitted_member_name));
		} else callee->children[1]->value =
			(ordinary_class_member && !generated_operator) ? member_name :
			(concrete_owner && !generated_operator ?
				(definition.member_template ? generated_name : member_name) : generated_name);
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

} // namespace pa18_templates_internal
