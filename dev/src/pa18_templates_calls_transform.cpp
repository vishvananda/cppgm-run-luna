#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
using namespace std;
namespace pa18_templates_internal {

struct ExplicitCallSelection
{
	bool valid;
	const TemplateDefinition* definition;
	CPPGMAstNodePtr deduction_input;
	string base;
	string argument_text;
	ExplicitCallSelection() : valid(false), definition(0) {}
};

CPPGMAstNodePtr PA18TemplateExpander::BuildExplicitDeductionInput(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions)
{
			CPPGMAstNodePtr explicit_deduction_input = input;
			// An explicit call inside a function-template replay can expand an
			// enclosing function pack into several fixed parameters of an overload
			// (`impl<...>(..., forward<Args>(args)...)`).  Validate and deduce the
			// inner overload against that typed expanded argument list, not the
			// compact source pack node.
			try {
				if(input->children.size() > 1 && input->children[1] &&
					input->children[1]->kind == "argument-list") {
					CPPGMAstNodePtr expanded_arguments = CloneNode(input->children[1]);
					expanded_arguments->children.clear();
					for(size_t argument = 0; argument < input->children[1]->children.size(); ++argument) {
						const CPPGMAstNodePtr source_argument = input->children[1]->children[argument];
						if(!source_argument || source_argument->kind != "pack-expansion-expression") {
							expanded_arguments->children.push_back(CloneNode(source_argument));
							continue;
						}
						string pack_name;
						const string source_spelling = SpellNode(source_argument);
						for(map<string, vector<string> >::const_iterator pack =
							active_pack_substitutions_.begin(); pack != active_pack_substitutions_.end(); ++pack) {
							if(pack->first.empty()) continue;
							const size_t at = source_spelling.find(pack->first);
							if(at != string::npos && (at == 0 || !IsIdentifierCharacter(source_spelling[at - 1])) &&
								(at + pack->first.size() == source_spelling.size() ||
									!IsIdentifierCharacter(source_spelling[at + pack->first.size()]))) {
								pack_name = pack->first;
								break;
							}
						}
						map<string, vector<string> >::const_iterator values =
							active_pack_substitutions_.find(pack_name);
						if(values == active_pack_substitutions_.end()) continue;
						for(size_t value = 0; value < values->second.size(); ++value) {
							CPPGMAstNodePtr expanded = CloneNode(source_argument->children.empty() ?
								source_argument : source_argument->children[0]);
							map<string, string> one;
							one[pack_name] = values->second[value];
							function<void(const CPPGMAstNodePtr&)> substitute =
								[&](const CPPGMAstNodePtr& node) {
									if(!node) return;
									node->value = ReplaceIdentifiersPreservingPackSizes(node->value, one);
									for(size_t child = 0; child < node->children.size(); ++child)
										substitute(node->children[child]);
							};
							substitute(expanded);
							// The expanded forwarding call is a probe only.  Carry the
							// already typed pack element on the synthetic argument so
							// candidate validation does not have to materialize its
							// dependent `forward` helper first.
							expanded->inferred_type = values->second[value];
							expanded_arguments->children.push_back(expanded);
						}
					}
					if(!expanded_arguments->children.empty()) {
						explicit_deduction_input = CloneNode(input);
						explicit_deduction_input->children.resize(1);
						explicit_deduction_input->children.push_back(expanded_arguments);
					}
				}
			} catch(const PA18SubstitutionFailure&) {}

	return explicit_deduction_input;
}

ExplicitCallSelection PA18TemplateExpander::SelectExplicitCallDefinition(
	const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& input_callee,
	const string& context, const map<string, string>& substitutions)
{
	ExplicitCallSelection state;
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
			CPPGMAstNodePtr explicit_deduction_input = BuildExplicitDeductionInput(input, context, substitutions);
			string base;
			size_t begin = 0;
			string argument_text;
			size_t close = string::npos;
			const TemplateDefinition* explicit_definition = 0;
			if(TemplateBase(lookup_callee, open, &begin, &base) &&
						TemplateRange(lookup_callee, open, &argument_text, &close)) {
						explicit_definition = FindDefinition(base, context);
				if(explicit_definition && !explicit_definition->class_template) {
					vector<const TemplateDefinition*> overloads = FindFunctionDefinitions(base, context);
					RankFunctionTemplateCandidatesForCall(&overloads, explicit_deduction_input,
						context, substitutions);
					if(overloads.size() > 1) {
						const vector<string> raw_explicit_args = SplitTemplateArguments(argument_text);
						const TemplateDefinition* selected_overload = 0;
						for(size_t overload = 0; overload < overloads.size(); ++overload) {
							vector<string> trial_arguments;
							const bool valid_explicit = ValidateExplicitFunctionCandidate(*overloads[overload], explicit_deduction_input, context,
								substitutions, raw_explicit_args, &trial_arguments);
							if(valid_explicit) {
								bool prefer = !selected_overload;
								if(selected_overload) {
							const bool candidate_more = FunctionTemplateMoreSpecialized(
								*overloads[overload], *selected_overload, context);
							const bool selected_more = FunctionTemplateMoreSpecialized(
								*selected_overload, *overloads[overload], context);
							if(candidate_more != selected_more) prefer = candidate_more;
							else {
								const bool candidate_definition = overloads[overload]->declaration &&
									overloads[overload]->declaration->kind == "function-definition";
								const bool selected_definition = selected_overload->declaration &&
									selected_overload->declaration->kind == "function-definition";
								prefer = candidate_definition != selected_definition ?
									candidate_definition : overloads[overload]->parameters.size() >
									selected_overload->parameters.size();
							}
								}
								if(prefer) {
									selected_overload = overloads[overload];
								}
							}
						}
						if(selected_overload) explicit_definition = selected_overload;
					}
				}
			}

			if(explicit_definition && !explicit_definition->class_template) {
				state.valid = true;
				state.definition = explicit_definition;
				state.deduction_input = explicit_deduction_input;
				state.base = base;
				state.argument_text = argument_text;
			}
		}
	}
	return state;
}

bool PA18TemplateExpander::TransformExplicitFunctionCall(
	const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& input_callee,
	const string& context, const map<string, string>& substitutions,
	const CPPGMAstNodePtr& result)
{
	ExplicitCallSelection state = SelectExplicitCallDefinition(input, input_callee,
		context, substitutions);
	if(!state.valid) return false;
	const TemplateDefinition* explicit_definition = state.definition;
	const CPPGMAstNodePtr explicit_deduction_input = state.deduction_input;
	const string& base = state.base;
	const string& argument_text = state.argument_text;
			if(explicit_definition && !explicit_definition->class_template) {
				vector<string> explicit_args = SplitTemplateArguments(argument_text);
				map<string, string> explicit_substitutions = substitutions;
				// A pack element is already a typed argument from the caller's
				// scope.  When the instantiated body introduces a local typedef
				// with the same spelling, do not let explicit-argument replay
				// resolve that bound class through the new lexical alias.
				set<string> protected_bound_names;
				for(map<string, string>::iterator substitution =
					explicit_substitutions.begin(); substitution != explicit_substitutions.end();
					++substitution) {
					for(map<string, vector<string> >::const_iterator pack =
						active_pack_substitutions_.begin();
						pack != active_pack_substitutions_.end(); ++pack) {
						for(size_t value = 0; value < pack->second.size(); ++value) {
							const string bound = CanonicalSpelling(pack->second[value]);
							if(bound.empty() || bound.find("::") != string::npos ||
								CanonicalSpelling(substitution->second) != bound ||
								class_contexts_.find(bound) == class_contexts_.end() ||
								ResolveAlias(bound, context) == bound) continue;
								substitution->second = "::" + bound;
								protected_bound_names.insert(bound);
								value = pack->second.size();
						}
					}
				}
				for(set<string>::const_iterator name = protected_bound_names.begin();
					name != protected_bound_names.end(); ++name)
					explicit_substitutions.erase(*name);
				for(map<string, PA19IntegralValue>::const_iterator integral =
					active_integral_substitutions_.begin();
					integral != active_integral_substitutions_.end(); ++integral) {
					if(integral->second.known)
						explicit_substitutions[integral->first] =
							IntegralValueSpelling(integral->second);
				}
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
				if(PreserveUnresolvedExplicitTemplateCall(input, result, explicit_args, context, explicit_substitutions, substitutions)) return true;
				const TemplateDefinition* explicit_specialization =
					FindExplicitFunctionSpecialization(explicit_definition, explicit_args);
				if(explicit_specialization) {
					// Explicit function specializations are indexed by the primary and
					// template arguments.  Overloaded primaries can share that key, so
					// the specialization still has to be viable for the already selected
					// function-parameter list before it replaces the overload candidate.
					vector<string> specialization_arguments;
					const bool specialization_viable = ValidateExplicitFunctionCandidate(*explicit_specialization,
						explicit_deduction_input, context, substitutions, explicit_args,
						&specialization_arguments);
					if(specialization_viable)
						explicit_definition = explicit_specialization;
				}
				vector<string> complete_args;
				map<string, FunctionSignature> inferred_function_values;
				bool has_parameter_pack = false;
				size_t fixed_template_parameters = 0;
				for(size_t parameter = 0; parameter < explicit_definition->parameters.size(); ++parameter)
					if(explicit_definition->parameters[parameter].pack)
						has_parameter_pack = true;
					else ++fixed_template_parameters;
				const bool pack_precedes_fixed = HasPackBeforeFixed(*explicit_definition);
				size_t template_pack_count = 0;
				for(size_t parameter = 0; parameter < explicit_definition->parameters.size(); ++parameter)
					if(explicit_definition->parameters[parameter].pack) ++template_pack_count;
				const bool explicit_pack_elements = has_parameter_pack &&
					explicit_args.size() > fixed_template_parameters &&
					template_pack_count == 1;
				bool complete = !pack_precedes_fixed && (explicit_pack_elements ||
					(!has_parameter_pack && explicit_args.size() == explicit_definition->parameters.size()));
				map<string, vector<string> > inferred_pack_values;
				map<string, vector<string> > explicit_pack_values;
				if(complete) {
					complete_args = explicit_args;
					if(explicit_pack_elements) for(size_t parameter = 0;
						parameter < explicit_definition->parameters.size(); ++parameter)
						if(explicit_definition->parameters[parameter].pack &&
							!explicit_definition->parameters[parameter].name.empty())
							explicit_pack_values[explicit_definition->parameters[parameter].name] = explicit_args;
				}
					else try { complete = InferFunctionArguments(*explicit_definition, explicit_deduction_input, &complete_args, substitutions, context, &explicit_args, &inferred_pack_values, &inferred_function_values); }
					catch(const PA18SubstitutionFailure&) { complete = false; }
				if(complete && ValidateTemplateDefaults(*explicit_definition, complete_args,
					context, substitutions)) {
					try {
						const string requested_owner_name = explicit_definition->member_template ?
							active_instantiation_name_ : string();
						const string* requested_owner = requested_owner_name.empty() ? 0 : &requested_owner_name;
						if(!explicit_pack_values.empty()) inferred_pack_values = explicit_pack_values;
						const string local_name = Instantiate(*explicit_definition, complete_args, context,
							false, inferred_pack_values.empty() ? 0 : &inferred_pack_values, 0,
							requested_owner, &inferred_function_values);
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
						return true;
						} catch(const PA18SubstitutionFailure&) {}
				}
			}
	return false;

}

bool PA18TemplateExpander::TransformUnqualifiedMemberTemplateCall(
	const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& input_callee,
	const string& context, const map<string, string>& substitutions,
	const CPPGMAstNodePtr& result)
{
	// The parser leaves an unqualified explicit member-template-id as an
	// id-expression; replay it through typed `this` lookup before free lookup.
	if(input_callee && input_callee->kind == "id-expression") {
		const string raw_member_id = RemoveMarker(input_callee->value);
		// A qualified template-id already carries its lookup owner.  Treating it
		// as an unqualified member call would invent a `this` receiver in a free
		// function (and, in particular, turn `C::static_member<T>` into an
		// invalid non-static call).  Dependent qualified member-ids have their
		// own owner-aware replay path above.
		if(raw_member_id.find("::") != string::npos) return false;
		const size_t member_id_open = raw_member_id.find('<');
		if(member_id_open != string::npos) {
			string member_id_base, member_id_arguments; size_t member_id_begin = 0, member_id_close = string::npos;
			if(TemplateBase(raw_member_id, member_id_open, &member_id_begin, &member_id_base) &&
				TemplateRange(raw_member_id, member_id_open, &member_id_arguments, &member_id_close)) {
			const vector<const TemplateDefinition*> member_candidates = FindFunctionDefinitions(LastComponent(member_id_base), context);
			bool member_context = function_owners_.find(context) != function_owners_.end();
			for(string current = context; !member_context && !current.empty(); ) {
				if(class_contexts_.find(current) != class_contexts_.end() ||
					class_declarations_.find(current) != class_declarations_.end()) {
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
					const bool instantiated = InstantiateMemberCall(member_call, member, raw_member_id, context,
						substitutions);
					if(instantiated) {
						result->children = member_call->children;
						result->template_primary = member_call->template_primary; result->template_arguments = member_call->template_arguments;
						result->template_instantiation = true;
						result->inferred_type = member_call->inferred_type;
						return true;
					}
				}
			}
		}
	}
	return false;

}

void PA18TemplateExpander::TransformCallChildren(
	const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& result,
	const string& context, const map<string, string>& substitutions)
{
	for(size_t i = 0; i < input->children.size(); ++i) {
		const bool preserve_array_alias = i == 0 && input->value == "braced-construction" && input->children[i] &&
			input->children[i]->kind == "id-expression" &&
			substitutions.find(input->children[i]->value) != substitutions.end() &&
			IsArrayTypeAlias(input->children[i]->value, context);
		CPPGMAstNodePtr child = preserve_array_alias ? CloneNode(input->children[i]) :
			TransformNode(input->children[i], context, substitutions);
		if(child) result->children.push_back(child);
	}
}

CPPGMAstNodePtr PA18TemplateExpander::MaterializeStaticCastCall(
	const CPPGMAstNodePtr& result, CPPGMAstNodePtr result_callee,
	const string& context, const map<string, string>& substitutions)
{
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

	return result_callee;
}

bool PA18TemplateExpander::MaterializeNamedCallTarget(
	const CPPGMAstNodePtr& result, CPPGMAstNodePtr* result_callee_out,
	const string& context, const map<string, string>& substitutions,
	bool* constructor_replayed_out)
{
	CPPGMAstNodePtr result_callee = *result_callee_out;
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
					return true;
				}
			}
		}
	}
	bool replayed = false;
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
				replayed = InstantiateMemberCall(result, synthetic_member,
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
	*result_callee_out = result_callee;
	*constructor_replayed_out = replayed;
	return false;
}

CPPGMAstNodePtr PA18TemplateExpander::MaterializeOperatorCallTargets(
	const CPPGMAstNodePtr& result, const CPPGMAstNodePtr& input_callee,
	CPPGMAstNodePtr result_callee, const string& context,
	const map<string, string>& substitutions)
{
	if(result_callee && result_callee->kind == "member-expression" &&
		result_callee->children.size() >= 2 && result_callee->children[0] &&
		result_callee->children[1] &&
		result_callee->children[1]->value != "operator()") {
		// A data member whose class supplies a call operator is represented as a
		// member-expression callee (`this->functor(args...)`).  PA14 lowers the
		// eventual call through `operator()`, so instantiate a dependent member
		// operator before semantic lookup loses the callable object's template
		// arguments.
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

	return result_callee;
}

bool PA18TemplateExpander::MaterializeImplicitMemberCall(
	const CPPGMAstNodePtr& result, CPPGMAstNodePtr& result_callee,
	const CPPGMAstNodePtr& input_callee, const string& context,
	const map<string, string>& substitutions)
{
	bool implicit_member_instantiated = false;
	string original_member;
	if(input_callee && input_callee->kind == "member-expression" &&
		input_callee->children.size() >= 2 && input_callee->children[1])
		original_member = input_callee->children[1]->value;
	ResolveMemberFunctionArguments(result, context, substitutions);
	const bool operator_target = result_callee && result_callee->kind == "member-expression" &&
		result_callee->children.size() >= 2 && result_callee->children[1] &&
		(result_callee->children[1]->value == "operator()" ||
		 result_callee->children[1]->value.find("operator()__") == 0);
	if(result_callee && result_callee->kind == "member-expression" && !operator_target)
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
				class_declarations_.find(current) != class_declarations_.end() ||
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
			} else if(HasReplayContext(substitutions) && context.find('<') != string::npos) {
				// An ordinary member can hide a namespace function even when the
				// replay path has no member-template specialization to emit.  Keep
				// the lookup receiver in the typed AST in that case; leaving the
				// source id-expression bare would incorrectly fall through to free
				// lookup (and loses calls from explicit member specializations).
				string object_type;
				if(InferArgument(synthetic_object, &object_type, substitutions, context)) {
					string member_type;
					set<string> active;
					const bool found_member = FindClassMemberType(object_type, result_callee->value,
						substitutions, context, &member_type, &active, false);
					if(found_member) {
						CPPGMAstNodePtr qualified_member(new CPPGMAstNode("id-expression",
							object_type + "::" + result_callee->value));
						result->children[0] = qualified_member;
						result_callee = qualified_member;
						implicit_member_instantiated = true;
					}
				}
			}
		}
	}

	return implicit_member_instantiated;
}

map<string, vector<string> > PA18TemplateExpander::BuildOwnerPackValues(
	const string& qualified_callee_owner, const string& context) const
{
	map<string, vector<string> > result;
	if(qualified_callee_owner.empty()) return result;
	map<string, string>::const_iterator owner_base = specialization_bases_.find(
		LastComponent(qualified_callee_owner));
	map<string, vector<string> >::const_iterator owner_arguments =
		specialization_arguments_.find(LastComponent(qualified_callee_owner));
	if(owner_base == specialization_bases_.end() ||
		owner_arguments == specialization_arguments_.end()) return result;
	const TemplateDefinition* owner_definition = FindDefinition(owner_base->second, context);
	if(!owner_definition || !owner_definition->class_template) return result;
	size_t owner_argument = 0;
	for(size_t parameter = 0; parameter < owner_definition->parameters.size(); ++parameter) {
		const TemplateParameter& owner_parameter = owner_definition->parameters[parameter];
		if(!owner_parameter.pack) {
			if(owner_argument < owner_arguments->second.size()) ++owner_argument;
			continue;
		}
		size_t trailing_fixed = 0;
		for(size_t later = parameter + 1; later < owner_definition->parameters.size(); ++later)
			if(!owner_definition->parameters[later].pack) ++trailing_fixed;
		const size_t available = owner_arguments->second.size() > owner_argument ?
			owner_arguments->second.size() - owner_argument : 0;
		const size_t count = available > trailing_fixed ? available - trailing_fixed : 0;
		vector<string>& values = result[owner_parameter.name];
		for(size_t element = 0; element < count; ++element)
			values.push_back(owner_arguments->second[owner_argument++]);
	}
	const TemplateDefinition* selected_owner = SelectClassTemplateDefinition(
		owner_definition, owner_arguments->second, context);
	if(!selected_owner || !selected_owner->partial_specialization) return result;
	result.clear();
	size_t selected_argument = 0;
	for(size_t parameter = 0; parameter < selected_owner->specialization_parameter_details.size(); ++parameter) {
		const TemplateParameter& detail = selected_owner->specialization_parameter_details[parameter];
		if(!detail.pack) {
			if(selected_argument < owner_arguments->second.size()) ++selected_argument;
			continue;
		}
		size_t trailing_fixed = 0;
		for(size_t later = parameter + 1;
			later < selected_owner->specialization_parameter_details.size(); ++later)
			if(!selected_owner->specialization_parameter_details[later].pack) ++trailing_fixed;
		const size_t available = owner_arguments->second.size() > selected_argument ?
			owner_arguments->second.size() - selected_argument : 0;
		const size_t count = available > trailing_fixed ? available - trailing_fixed : 0;
		vector<string>& values = result[detail.name];
		for(size_t element = 0; element < count; ++element)
			values.push_back(owner_arguments->second[selected_argument++]);
	}
	return result;
}

string PA18TemplateExpander::MaterializedFunctionResultType(
	const TemplateDefinition& definition, const vector<string>& inferred,
	const string& context, const map<string, string>& substitutions,
	const map<string, vector<string> >& inferred_pack_values)
{
	if(!definition.declaration || definition.declaration->children.empty()) return string();
	map<string, string> return_substitutions = substitutions;
	for(size_t parameter = 0; parameter < definition.parameters.size() &&
		parameter < inferred.size(); ++parameter)
		if(!definition.parameters[parameter].name.empty())
			return_substitutions[definition.parameters[parameter].name] = inferred[parameter];
	string return_type = NodeTypeSpelling(definition.declaration->children[0]);
	return_type += ReturnDeclaratorSuffix(FunctionDeclarator(definition.declaration));
	const map<string, vector<string> > previous_packs = active_pack_substitutions_;
	for(map<string, vector<string> >::const_iterator pack = inferred_pack_values.begin();
		pack != inferred_pack_values.end(); ++pack)
		if(!pack->first.empty()) active_pack_substitutions_[pack->first] = pack->second;
	try {
		const string result = CanonicalSpelling(ResolveAlias(RewriteText(
			return_type, context, return_substitutions, 0), context));
		active_pack_substitutions_ = previous_packs;
		return result;
	} catch(...) {
		active_pack_substitutions_ = previous_packs;
		throw;
	}
}

void PA18TemplateExpander::ResolveSelectedFunctionArguments(
	const TemplateDefinition& definition, const CPPGMAstNodePtr& result,
	const vector<string>& inferred, const string& context,
	const map<string, string>& substitutions)
{
	if(!definition.declaration || definition.declaration->children.empty()) return;
	FunctionSignature signature;
	signature.result_specifiers = definition.declaration->children[0];
	signature.declarator = FunctionDeclarator(definition.declaration);
	signature.parameters = DescendantOfKind(signature.declarator, "parameter-clause");
	map<string, string> selected_substitutions = substitutions;
	for(size_t parameter = 0; parameter < definition.parameters.size() &&
		parameter < inferred.size(); ++parameter)
		if(!definition.parameters[parameter].name.empty())
			selected_substitutions[definition.parameters[parameter].name] = inferred[parameter];
	ResolveFunctionArguments(result, &signature, context, &selected_substitutions);
}

bool PA18TemplateExpander::MaterializeFreeFunctionCandidate(
	const TemplateDefinition* definition, const CPPGMAstNodePtr& result,
	const CPPGMAstNodePtr& result_callee, const string& callee_name,
	const string& qualified_callee_owner, const string& context,
	const map<string, string>& substitutions,
	const map<const TemplateDefinition*, string>& inherited_owners,
	const map<string, vector<string> >& owner_pack_values)
{
	vector<string> inferred;
	map<string, vector<string> > inferred_pack_values;
	map<string, FunctionSignature> inferred_function_values;
	map<string, vector<string> > forwarding_pack_values;
	map<string, string> candidate_substitutions = substitutions;
	if(definition->member_template && !qualified_callee_owner.empty())
		AddConcreteOwnerSubstitutions(qualified_callee_owner, context,
			&candidate_substitutions);
	try {
		if(!InferFunctionArguments(*definition, result, &inferred,
			candidate_substitutions, context, 0, &inferred_pack_values,
			&inferred_function_values, owner_pack_values.empty() ? 0 :
			&owner_pack_values, &forwarding_pack_values)) {
			return false;
		}
	} catch(const PA18SubstitutionFailure&) {
		return false;
	}
	if(!ValidateTemplateDefaults(*definition, inferred, context,
		candidate_substitutions)) {
		return false;
	}
	// Deleted declarations and immediate return constraints are collected as
	// typed candidate facts.  Do not recover either fact from a materialized
	// return spelling on this hot path.
	if(definition->deleted) return false;
	if(definition->immediate_return_constraint) try {
		if(FunctionResultType(*definition, inferred, context,
			&candidate_substitutions, 0, true).empty()) return false;
	} catch(const PA18SubstitutionFailure&) {
		return false;
	}
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
		map<string, vector<string> > instantiation_pack_hints = inferred_pack_values;
		for(map<string, vector<string> >::const_iterator owner_pack = owner_pack_values.begin();
			owner_pack != owner_pack_values.end(); ++owner_pack)
			instantiation_pack_hints[owner_pack->first] = owner_pack->second;
		const map<string, string>* materialization_substitutions =
			selected_definition->member_template && !qualified_callee_owner.empty() ?
			&candidate_substitutions : 0;
		const string local_name = Instantiate(*selected_definition, inferred, context, false,
			&instantiation_pack_hints, materialization_substitutions, requested_owner,
			&inferred_function_values, &forwarding_pack_values);
		const string inferred_result_type = MaterializedFunctionResultType(
			*selected_definition, inferred, context, substitutions, inferred_pack_values);
		const string qualifier = concrete_member_owner ? requested_owner_name :
			GeneratedFunctionQualifier(*definition, callee_name, context);
		const string emitted_name = concrete_member_owner ?
			LastComponent(selected_definition->name) : local_name;
		if(!inferred_result_type.empty()) result->inferred_type = inferred_result_type;
		result->template_primary = definition->qualified_name;
		result->template_arguments = inferred;
		result_callee->value = qualifier.empty() ? emitted_name : qualifier +
			"::" + emitted_name;
		ResolveSelectedFunctionArguments(*selected_definition, result, inferred,
			context, substitutions);
		return true;
	} catch(const PA18SubstitutionFailure&) {
		return false;
	}
}

bool PA18TemplateExpander::MaterializeFreeFunctionCandidates(
	const vector<const TemplateDefinition*>& definitions,
	const CPPGMAstNodePtr& result, const CPPGMAstNodePtr& result_callee,
	const string& callee_name, const string& qualified_callee_owner,
	const string& context, const map<string, string>& substitutions,
	const map<const TemplateDefinition*, string>& inherited_owners)
{
	const map<string, vector<string> > owner_pack_values =
		BuildOwnerPackValues(qualified_callee_owner, context);
	// Declaration/definition pairing is a lookup fact for this candidate set.
	// Index the definition signatures once instead of rescanning every candidate
	// for each declaration; this path is reached for ordinary calls and can sit
	// on the template-deduction hot path.
	set<string> defined_signatures;
	for(size_t candidate = 0; candidate < definitions.size(); ++candidate) {
		const TemplateDefinition* definition = definitions[candidate];
		if(definition && definition->declaration &&
			definition->declaration->kind == "function-definition")
			defined_signatures.insert(MemberSignatureKey(*definition));
	}
	for(size_t candidate = 0; candidate < definitions.size(); ++candidate) {
		const TemplateDefinition* definition = definitions[candidate];
		if(definition->declaration && definition->declaration->kind == "simple-declaration") {
			if(defined_signatures.find(MemberSignatureKey(*definition)) !=
				defined_signatures.end()) continue;
		}
		if(MaterializeFreeFunctionCandidate(definition, result, result_callee,
			callee_name, qualified_callee_owner, context, substitutions,
			inherited_owners, owner_pack_values)) return true;
	}
	return false;
}

void PA18TemplateExpander::RefinePartialSpecializationCallDefinitions(
	vector<const TemplateDefinition*>* definitions,
	const string& qualified_callee_owner, const string& callee_name,
	const string& context)
{
	if(!definitions || qualified_callee_owner.empty()) return;
	map<string, string>::const_iterator owner_base = specialization_bases_.find(
		LastComponent(qualified_callee_owner));
	map<string, vector<string> >::const_iterator owner_arguments =
		specialization_arguments_.find(LastComponent(qualified_callee_owner));
	if(owner_base == specialization_bases_.end() ||
		owner_arguments == specialization_arguments_.end()) return;
	const TemplateDefinition* primary = FindDefinition(owner_base->second, context);
	const TemplateDefinition* selected = primary && primary->class_template ?
		SelectClassTemplateDefinition(primary, owner_arguments->second, context) : 0;
	if(!selected || !selected->partial_specialization) return;
	const string selected_class_scope = JoinPath(selected->owner, selected->name);
	string selected_owner = JoinPath(selected_class_scope, selected->name) + "<";
	for(size_t argument = 0; argument < selected->specialization_pattern.size(); ++argument) {
		if(argument) selected_owner += ",";
		selected_owner += selected->specialization_pattern[argument];
	}
	selected_owner += ">";
	vector<const TemplateDefinition*> selected_definitions;
	const string member_name = LastComponent(callee_name);
	map<string, vector<string> >::const_iterator indexed =
		definitions_by_name_.find(member_name);
	if(indexed != definitions_by_name_.end()) for(size_t candidate = 0;
		candidate < indexed->second.size(); ++candidate) {
		map<string, TemplateDefinition>::const_iterator found = definitions_.find(
			indexed->second[candidate]);
		if(found == definitions_.end() || !found->second.member_template) continue;
		if(found->second.owner == selected_owner) selected_definitions.push_back(&found->second);
	}
	if(selected_definitions.empty()) return;
	vector<const TemplateDefinition*> filtered;
	string selected_base = selected_owner;
	const size_t selected_open = selected_base.find('<');
	if(selected_open != string::npos) selected_base.erase(selected_open);
	for(size_t candidate = 0; candidate < definitions->size(); ++candidate) {
		const TemplateDefinition* definition = (*definitions)[candidate];
		string definition_base = definition->owner;
		const size_t open = definition_base.find('<');
		if(open != string::npos) definition_base.erase(open);
		if(definition->member_template && definition_base == selected_base) continue;
		filtered.push_back(definition);
	}
	for(size_t selected_index = 0; selected_index < selected_definitions.size(); ++selected_index)
		if(find(filtered.begin(), filtered.end(), selected_definitions[selected_index]) == filtered.end())
			filtered.push_back(selected_definitions[selected_index]);
	definitions->swap(filtered);
}

void PA18TemplateExpander::MaterializeFreeFunctionCall(
	const CPPGMAstNodePtr& result, const CPPGMAstNodePtr& result_callee,
	bool constructor_replayed, bool implicit_member_instantiated,
	const string& context, const map<string, string>& substitutions)
{
	if(!constructor_replayed && !implicit_member_instantiated && result_callee &&
		result_callee->kind == "id-expression" &&
		result_callee->value.find('<') == string::npos) {
		const string callee_name = result_callee->value;
		vector<const TemplateDefinition*> definitions =
			FindFunctionDefinitions(callee_name, context);
		MaterializeOrdinaryCallConversions(callee_name, result, context, substitutions);
		map<const TemplateDefinition*, string> inherited_owners;
		const string qualified_callee_owner = PrefixComponent(callee_name);
		RefinePartialSpecializationCallDefinitions(&definitions,
			qualified_callee_owner, callee_name, context);
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
		const bool preserve_lookup_order = PreserveFunctionLookupOrder(
			definitions, context, substitutions);
		if(!preserve_lookup_order) SortFunctionTemplateCandidates(&definitions, context);
		if(!preserve_lookup_order && callee_name.compare(0, 8, "operator") != 0)
			RankFunctionTemplateCandidatesForCall(&definitions, result, context, substitutions);
		const bool inline_template_candidate = HasInlineTemplateCandidate(definitions, context);
		bool extern_template_candidate = false;
		for(size_t candidate = 0; candidate < definitions.size() && !extern_template_candidate;
			++candidate) {
			vector<string> inferred;
			try {
				if(!InferFunctionArguments(*definitions[candidate], result, &inferred,
					substitutions, context, 0)) continue;
			} catch(const PA18SubstitutionFailure&) {
				continue;
			}
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
			MaterializeFreeFunctionCandidates(definitions, result, result_callee,
				callee_name, qualified_callee_owner, context, substitutions,
				inherited_owners);
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
}

void PA18TemplateExpander::FinalizeCallResult(
	const CPPGMAstNodePtr& result, const CPPGMAstNodePtr& result_callee,
	const string& context, const map<string, string>& substitutions)
{
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
			result->inferred_type = QualifyTypeArgument(member_result_type,
				member_object_type, member_object_type, true);
	}
}

} // namespace pa18_templates_internal
