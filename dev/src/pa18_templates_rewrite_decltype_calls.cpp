#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::HasClassConversion(const string& expected,
	const string& actual, const string& context) const
{
	const string wanted = FunctionArgumentObjectType(expected, context);
	set<string> active_classes;
	function<bool(const string&)> visit_class = [&](const string& raw_class) {
		string class_name = CanonicalSpelling(raw_class);
		while(class_name.compare(0, 6, "const ") == 0)
			class_name = CanonicalSpelling(class_name.substr(6));
		while(!class_name.empty() && (class_name[class_name.size() - 1] == '&' ||
			class_name[class_name.size() - 1] == '*'))
			class_name.erase(class_name.size() - 1);
		class_name = CanonicalSpelling(class_name);
		if(class_name.empty() || !active_classes.insert(class_name).second) return false;
		const CPPGMAstNodePtr declaration = FindClassDeclaration(class_name, context);
		if(!declaration) {
			active_classes.erase(class_name);
			return false;
		}
		vector<string> class_arguments;
		string primary_name = class_name;
		const size_t open = class_name.find('<');
		if(open != string::npos) {
			string argument_text;
			size_t close = string::npos;
			if(!TemplateRange(class_name, open, &argument_text, &close)) {
				active_classes.erase(class_name);
				return false;
			}
			class_arguments = SplitTemplateArguments(argument_text);
			primary_name = CanonicalSpelling(class_name.substr(0, open));
		} else {
			map<string, vector<string> >::const_iterator generated_arguments =
				specialization_arguments_.find(LastComponent(class_name));
			map<string, string>::const_iterator generated_base =
				specialization_bases_.find(LastComponent(class_name));
			if(generated_arguments != specialization_arguments_.end())
				class_arguments = generated_arguments->second;
			if(generated_base != specialization_bases_.end() &&
				!generated_base->second.empty()) primary_name = generated_base->second;
		}
		const TemplateDefinition* primary = FindDefinition(primary_name, context);
		if(!primary) primary = FindDefinition(LastComponent(primary_name), context);
		map<string, string> class_substitutions;
		if(primary) for(size_t parameter = 0; parameter < primary->parameters.size() &&
			parameter < class_arguments.size(); ++parameter)
			if(!primary->parameters[parameter].name.empty())
				class_substitutions[primary->parameters[parameter].name] =
					class_arguments[parameter];
		function<bool(const CPPGMAstNodePtr&)> visit = [&](const CPPGMAstNodePtr& node) {
			if(!node) return false;
			const string name = RemoveMarker(node->value);
			if(name.compare(0, 8, "operator") == 0 && name.size() > 8) {
				string target = CanonicalSpelling(ReplaceIdentifiers(name.substr(8),
					class_substitutions));
				if(!target.empty()) try {
					target = const_cast<PA18TemplateExpander*>(this)->RewriteText(
						target, context, class_substitutions, 0);
					if(FunctionArgumentObjectType(target, context) == wanted) return true;
				} catch(const PA18SubstitutionFailure&) {}
			}
			if(node->kind == "base-clause") for(size_t child = 0;
				child < node->children.size(); ++child) {
				const CPPGMAstNodePtr base_name = ChildOfKindLocal(
					node->children[child], "base-name");
				if(!base_name) continue;
				string base = CanonicalSpelling(ReplaceIdentifiers(base_name->value,
					class_substitutions));
				try {
					base = CanonicalSpelling(ReplaceIdentifiers(
						const_cast<PA18TemplateExpander*>(this)->RewriteText(
							base, context, class_substitutions, 0), class_substitutions));
					base = CanonicalSpelling(ResolveAlias(base, context));
				} catch(const PA18SubstitutionFailure&) { continue; }
				if(visit_class(base)) return true;
			}
			for(size_t child = 0; child < node->children.size(); ++child)
				if(visit(node->children[child])) return true;
			return false;
		};
		const bool found = visit(declaration);
		active_classes.erase(class_name);
		return found;
	};
	return visit_class(actual);
}
bool PA18TemplateExpander::FunctionArgumentViable(const string& parameter,
	const string& actual, const string& context) const
{
	const string expected = FunctionArgumentObjectType(parameter, context);
	const string received = FunctionArgumentObjectType(actual, context);
	if(expected.empty() || received.empty()) return false;
	if(expected == received) return true;
	// A declaration can spell a class parameter relative to its owning
	// template while the inferred argument carries the qualified owner.  Use
	// the typed name resolver before treating the two class objects as
	// different; this does not collapse distinct template specializations.
	const string qualified_expected = CanonicalSpelling(QualifyTypeArgument(
		expected, context));
	const string qualified_received = CanonicalSpelling(QualifyTypeArgument(
		received, context));
	if(!qualified_expected.empty() && qualified_expected == qualified_received)
		return true;
	if(expected.find('<') == string::npos && received.find('<') == string::npos) {
		const CPPGMAstNodePtr expected_declaration = FindClassDeclaration(expected, context);
		const CPPGMAstNodePtr received_declaration = FindClassDeclaration(received, context);
		if(expected_declaration && expected_declaration == received_declaration) return true;
	}
	if(IsBuiltinArithmeticType(expected) && IsBuiltinArithmeticType(received))
		return true;
	if(IsBuiltinArithmeticType(expected) && FindClassDeclaration(received, context))
		return false;
	// Pointer arguments are not an unconstrained class conversion boundary.
	// In particular, a pointer to one sibling class must not bind to a pointer
	// to another sibling merely because both pointees are known classes.  This
	// distinction is observable in the standard detection idiom used by the
	// structured-bool replay cases.
	const size_t expected_pointer = expected.rfind('*');
	const size_t received_pointer = received.rfind('*');
	if(expected_pointer != string::npos && received_pointer != string::npos) {
		const auto pointer_target = [](string raw) {
			raw.erase(raw.rfind('*'));
			return CanonicalSpelling(raw);
		};
		const string expected_target = pointer_target(expected);
		const string received_target = pointer_target(received);
		if(expected_target.find("__") != string::npos ||
			received_target.find("__") != string::npos) return true;
		if(specialization_arguments_.find(LastComponent(expected_target)) !=
			specialization_arguments_.end() || specialization_arguments_.find(
			LastComponent(received_target)) != specialization_arguments_.end()) return true;
		if(expected_target == received_target) return true;
		if(expected_target == "void") return true;
		const string qualified_expected_target = CanonicalSpelling(QualifyTypeArgument(
			expected_target, context));
		const string qualified_received_target = CanonicalSpelling(QualifyTypeArgument(
			received_target, context));
		if(qualified_expected_target == qualified_received_target) return true;
		const auto expected_declaration = FindClassDeclaration(qualified_expected_target, context);
		const auto received_declaration = FindClassDeclaration(qualified_received_target, context);
		// An unresolved template-template pointer remains a deduction pattern,
		// not a concrete unrelated-class conversion.  Leave that case viable for
		// the later template-argument matcher.
		if(!expected_declaration || !received_declaration) return true;
		set<string> visited;
		function<bool(const string&)> derives = [&](const string& derived) {
			const string key = CanonicalSpelling(derived);
			if(!visited.insert(key).second) return false;
			if(key == qualified_expected_target) return true;
			const CPPGMAstNodePtr declaration = FindClassDeclaration(key, context);
			if(!declaration) return false;
			for(size_t child = 0; child < declaration->children.size(); ++child) {
				const CPPGMAstNodePtr base_clause = declaration->children[child];
				if(!base_clause || base_clause->kind != "base-clause") continue;
				for(size_t base = 0; base < base_clause->children.size(); ++base) {
					const CPPGMAstNodePtr base_name = ChildOfKindLocal(
						base_clause->children[base], "base-name");
					if(!base_name) continue;
					string base_spelling = CanonicalSpelling(base_name->value);
					base_spelling = CanonicalSpelling(QualifyTypeArgument(base_spelling, context));
					if(derives(base_spelling)) return true;
				}
			}
			return false;
		};
		return derives(qualified_received_target);
	}
	// A hidden friend may deliberately take a lightweight proxy object while
	// callers probe it with the associated property type.  Model the ordinary
	// converting-constructor path before looking for conversion operators on the
	// received class; both are user-defined conversions in the candidate probe.
	const CPPGMAstNodePtr expected_class = FindClassDeclaration(expected, context);
	if(expected_class) {
		const string expected_name = LastComponent(expected);
		for(size_t child = 0; child < expected_class->children.size(); ++child) {
			const CPPGMAstNodePtr member = expected_class->children[child];
			if(!member || (member->kind != "special-member-definition" &&
				member->kind != "special-member-declaration") ||
				LastComponent(RemoveMarker(member->value)) != expected_name) continue;
			const CPPGMAstNodePtr parameters = DescendantOfKind(
				FunctionDeclarator(member), "parameter-clause");
			if(!parameters || parameters->children.empty()) continue;
			const CPPGMAstNodePtr first = parameters->children[0];
			if(first && first->kind == "parameter-declaration" &&
				FunctionArgumentViable(ParameterTypeSpelling(first), received, context)) return true;
		}
	}
	// Expression-SFINAE needs to reject an attempted conversion between two
	// unrelated complete class types.  The typed class conversion index admits
	// only a conversion operator whose target matches the expected object.
	const bool direct_parameter = expected.find('*') == string::npos &&
		expected.find('&') == string::npos;
	const bool direct_actual = received.find('*') == string::npos &&
		received.find('&') == string::npos;
	if(direct_parameter && direct_actual &&
		FindClassDeclaration(expected, context) &&
		FindClassDeclaration(received, context))
		return HasClassConversion(expected, received, context);
	return true;
}

bool PA18TemplateExpander::IsDeletedFunctionCall(const string& callee,
	const string& context) const
{
	const vector<const TemplateDefinition*> candidates = FindFunctionDefinitions(callee,
		context);
	if(!candidates.empty()) {
		for(size_t candidate = 0; candidate < candidates.size(); ++candidate) {
			if(!candidates[candidate] || !candidates[candidate]->deleted) return false;
		}
		return true;
	}
	const FunctionSignature* signature = FindFunctionSignature(callee, context);
	return signature && signature->deleted;
}

void PA18TemplateExpander::ApplyFriendClassSubstitutions(
	const TemplateDefinition& definition, const vector<string>& actual_types,
	const string& context, map<string, string>* substitutions) const
{
	if(!substitutions || !definition.friend_declaration) return;
	string friend_owner = definition.lexical_owner.empty() ? definition.owner :
		definition.lexical_owner;
	const size_t owner_angle = friend_owner.find('<');
	if(owner_angle != string::npos) friend_owner.erase(owner_angle);
	friend_owner = CanonicalSpelling(friend_owner);
	for(size_t actual = 0; actual < actual_types.size(); ++actual) {
		string spelling = CanonicalSpelling(actual_types[actual]);
		while(spelling.compare(0, 6, "const ") == 0)
			spelling = CanonicalSpelling(spelling.substr(6));
		while(spelling.compare(0, 9, "volatile ") == 0)
			spelling = CanonicalSpelling(spelling.substr(9));
		while(!spelling.empty() && (spelling[spelling.size() - 1] == '&' ||
			spelling[spelling.size() - 1] == '*')) spelling.erase(spelling.size() - 1);
		spelling = CanonicalSpelling(spelling);
		const CPPGMAstNodePtr class_node = FindClassDeclaration(spelling, context);
		if(!class_node || class_node->template_primary.empty() ||
			class_node->template_arguments.empty()) continue;
		const TemplateDefinition* class_definition = FindDefinition(
			class_node->template_primary, context);
		if(!class_definition || !class_definition->class_template ||
			(!friend_owner.empty() && LastComponent(friend_owner) !=
				LastComponent(class_definition->qualified_name))) continue;
		if(!friend_owner.empty() && class_definition->qualified_name != friend_owner &&
			PrefixComponent(class_definition->qualified_name) != PrefixComponent(friend_owner))
			continue;
		size_t argument = 0;
		for(size_t parameter = 0; parameter < class_definition->parameters.size() &&
			argument < class_node->template_arguments.size(); ++parameter) {
			const TemplateParameter& detail = class_definition->parameters[parameter];
			if(detail.name.empty()) {
				if(!detail.pack) ++argument;
				continue;
			}
			if(detail.pack) {
				if(argument < class_node->template_arguments.size())
					(*substitutions)[detail.name] = class_node->template_arguments[argument];
				break;
			}
			(*substitutions)[detail.name] = class_node->template_arguments[argument++];
		}
		return;
	}
}

	bool PA18TemplateExpander::FunctionCallResultType(string expression, const string& context, const map<string, string>& substitutions, string* result)
	{
		if(!result) return false;
		const bool generated_callee = expression.find("__ov") != string::npos ||
			expression.find("__inst_") != string::npos;
		const string call_key = "call|" + expression + "@" + context;
		if(!generated_callee && !active_function_results_.insert(call_key).second) {
			return false;
		}
		ActiveFunctionResultScope call_scope(this, generated_callee ? string() : call_key);
		// Expand a trailing function-parameter pack before splitting the
		// unevaluated call's arguments.  Otherwise `declval<_Args>()...` is
		// treated as one dependent operand and a valid return-type probe fails.
		expression = ExpandPackCallText(expression, active_pack_substitutions_);
		string callee, argument_text;
		if(!SplitTextCall(expression, &callee, &argument_text)) {
			return false;
		}
		callee = StripTextParentheses(callee);
		if(callee.empty()) {
			return false;
		}
		// The declval helper is intentionally declaration-only.  Its only
		// semantic effect in an unevaluated operand is to transport the requested
		// type as an xvalue (with the usual reference collapsing), so do not make
		// callable lookup depend on materializing an otherwise body-less helper.
		const size_t declval_open = callee.find('<');
		if(declval_open != string::npos) {
			string declval_base, declval_arguments;
			size_t declval_begin = 0, declval_close = string::npos;
			if(TemplateBase(callee, declval_open, &declval_begin, &declval_base) &&
				LastComponent(declval_base) == "declval" &&
				TemplateRange(callee, declval_open, &declval_arguments, &declval_close) &&
				declval_close + 1 == callee.size()) {
				const vector<string> requested = SplitTemplateArguments(declval_arguments);
				if(requested.size() == 1) {
					string type = NormalizeTypeArgument(ReplaceIdentifiers(
						requested[0], substitutions));
					type = NormalizeTypeArgument(ResolveAlias(type, context));
					if(!type.empty()) {
						if(type.size() < 1 || (type[type.size() - 1] != '&' &&
							type.compare(type.size() >= 2 ? type.size() - 2 : 0, 2, "&&") != 0))
							type += "&&";
						*result = CollapseReferenceSpelling(type);
						return true;
					}
				}
			}
		}
		const string function_context = FunctionLookupContext(context);
		string nested_object_type;
		bool has_nested_object_type = false;
		if(callee[callee.size() - 1] == ')') {
			string returned;
			if(FunctionCallResultType(callee, function_context, substitutions, &returned)) {
				returned = ResolveAlias(CollapseReferenceSpelling(returned), context);
				nested_object_type = returned;
				has_nested_object_type = true;
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
		// A qualified call whose owner is a dependent class or alias template is
		// still a member lookup.  The first '<' in a spelling such as
		// `enable_if_t<...>::initiate(...)` belongs to the owner, so treating it as
		// an explicit function-template-id makes the alias itself look callable and
		// loses the actual `async_result<...>::initiate` overload set.
		vector<const TemplateDefinition*> qualified_candidates;
		string qualified_member_name;
		string qualified_owner;
		bool qualified_member_call = false;
		vector<const TemplateDefinition*> dot_member_candidates;
		string dot_member_name;
		string dot_member_owner;
		bool dot_member_call = false;
		const size_t qualified_separator = TopLevelScopeSeparator(callee);
		if(qualified_separator != string::npos) {
			const string owner_spelling = callee.substr(0, qualified_separator);
			qualified_member_name = callee.substr(qualified_separator + 2);
			if(!qualified_member_name.empty()) {
				qualified_member_call = true;
				qualified_owner = CanonicalSpelling(ReplaceIdentifiers(owner_spelling,
					substitutions));
				try {
					qualified_owner = RewriteText(qualified_owner, function_context,
						substitutions, 0);
				} catch(const PA18SubstitutionFailure&) {}
				qualified_owner = CanonicalSpelling(ResolveAlias(ReplaceIdentifiers(
					qualified_owner, substitutions), function_context));
				qualified_candidates = FindFunctionDefinitions(qualified_member_name,
					qualified_owner);
				if(qualified_candidates.empty())
					qualified_candidates = FindFunctionDefinitions(qualified_member_name,
						owner_spelling);
			}
		}
		vector<string> explicit_arguments;
		const TemplateDefinition* explicit_definition = 0;
		string explicit_base_name;
		const size_t member_template_open = qualified_member_call ?
			qualified_member_name.find('<') : string::npos;
		const size_t template_open = qualified_member_call ? member_template_open :
			callee.find('<');
		size_t final_member_separator = callee.rfind('.');
		const size_t final_arrow = callee.rfind("->");
		if(final_arrow != string::npos && final_arrow > final_member_separator)
			final_member_separator = final_arrow;
		const bool explicit_member_template = final_member_separator != string::npos &&
			callee.find('<', final_member_separator + 1) != string::npos;
		const bool has_member_operator = (callee.find('.') != string::npos ||
			callee.find("->") != string::npos) && callee.find(".~") == string::npos &&
			!explicit_member_template;
	const bool callee_is_static_cast = callee.compare(0, 12, "static_cast<") == 0;
		if(template_open != string::npos && !callee_is_static_cast &&
			!has_member_operator) {
			string base_arguments, base;
			size_t template_close = string::npos, begin = 0;
			const string& template_source = qualified_member_call ? qualified_member_name : callee;
			if(!TemplateBase(template_source, template_open, &begin, &base) ||
				!TemplateRange(template_source, template_open, &base_arguments, &template_close)) {
				return false;
			}
				explicit_base_name = base;
				explicit_definition = FindExplicitFunctionTemplate(base,
					qualified_member_call && !qualified_owner.empty() ? qualified_owner :
					function_context);
			if(!explicit_definition) {
				vector<const TemplateDefinition*> inherited;
				set<string> active;
				map<const TemplateDefinition*, string> concrete_owners;
				if(qualified_member_call && !qualified_owner.empty())
					CollectInheritedMemberTemplates(qualified_owner, base, substitutions,
						function_context, &inherited, &active, &concrete_owners);
				else CollectInheritedMemberTemplates(context, base, substitutions,
					function_context, &inherited, &active, &concrete_owners);
				if(!inherited.empty()) explicit_definition = inherited[0];
			}
			if(!explicit_definition) return false;
			if(explicit_definition->class_template) {
				// A class-template-id followed by `()` is a functional cast.  It is
				// not a function-template call, so let the typed constructor probe
				// below decide whether the class can be initialized.
				explicit_definition = 0;
				explicit_base_name.clear();
			} else {
				explicit_arguments = SplitTemplateArguments(base_arguments);
				ExpandExplicitFunctionArguments(base_arguments, function_context,
					substitutions, &explicit_arguments);
				// A template-template argument denotes the template entity itself,
				// not the type produced by resolving its alias body.  In particular,
				// resolving `F` after `F -> two` turns the fixed-arity alias into its
				// target `A` and loses the arity check for `G<T...>`.
				const vector<string> source_explicit = SplitTemplateArguments(base_arguments);
				for(size_t explicit_index = 0; explicit_index < explicit_arguments.size() &&
					explicit_index < source_explicit.size() && explicit_definition &&
					explicit_index < explicit_definition->parameters.size(); ++explicit_index)
					if(explicit_definition->parameters[explicit_index].template_template) {
						const string normalized = NormalizeTemplateTemplateArgument(
							source_explicit[explicit_index], function_context, substitutions);
						if(!normalized.empty()) explicit_arguments[explicit_index] = normalized;
					}
			}
		}
		vector<string> actual_types;
		const vector<string> actual_expressions = SplitCallArguments(argument_text);
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
							if(actual.empty()) {
								return false;
							}
						actual_types.push_back(actual);
					}
					continue;
				}
			}
			const string actual = ExpressionTypeSpelling(actual_expression, function_context,
				substitutions);
			if(actual.empty()) {
				return false;
			}
			actual_types.push_back(actual);
		}
		// A member call in an unevaluated operand is retained as `object.member` in
		// the compact PA10 spelling.  Resolve the object through typed expression
		// lookup, then use the concrete class owner for member-template candidates;
		// treating the whole spelling as an unqualified function name otherwise
		// finds only an unrelated `declval` helper.
		int member_angle = 0, member_parentheses = 0, member_brackets = 0;
		size_t dot_separator = string::npos, dot_separator_size = 0;
		for(size_t position = 0; position < callee.size(); ++position) {
			const char ch = callee[position];
			if(ch == '(') ++member_parentheses;
			else if(ch == ')' && member_parentheses > 0) --member_parentheses;
			else if(ch == '[') ++member_brackets;
			else if(ch == ']' && member_brackets > 0) --member_brackets;
			else if(ch == '<' && IsTemplateAngleOpen(callee, position)) ++member_angle;
			else if(ch == '>' && member_angle > 0 && IsTemplateAngleClose(callee, position)) --member_angle;
			if(member_angle != 0 || member_parentheses != 0 || member_brackets != 0) continue;
			if(ch == '.') {
				dot_separator = position;
				dot_separator_size = 1;
			} else if(callee.compare(position, 2, "->") == 0) {
				dot_separator = position;
				dot_separator_size = 2;
			}
		}
		// Nested template closers can leave the compact angle-depth scan one level
		// deep at the member separator (`declval<conditional_t<...>>().require` is
		// the common case).  The syntax still unambiguously identifies the final
		// dot/arrow as member access, so recover it when the balanced scan missed it.
		if(dot_separator == string::npos && has_member_operator) {
			const size_t fallback_arrow = callee.rfind("->");
			const size_t fallback_dot = callee.rfind('.');
			if(fallback_arrow != string::npos && fallback_arrow > fallback_dot) {
				dot_separator = fallback_arrow;
				dot_separator_size = 2;
			} else if(fallback_dot != string::npos) {
				dot_separator = fallback_dot;
				dot_separator_size = 1;
			}
		}
		if(!explicit_member_template && dot_separator != string::npos &&
			dot_separator + dot_separator_size < callee.size()) {
			const string object_expression = Trim(callee.substr(0, dot_separator));
			string member_spelling = Trim(callee.substr(dot_separator + dot_separator_size));
			const size_t member_open = member_spelling.find('<');
			if(member_open != string::npos) member_spelling.erase(member_open);
			string object_type = ExpressionTypeSpelling(object_expression, function_context,
				substitutions);
			const bool complex_member_object = object_expression.find('<') != string::npos ||
				object_expression.find('(') != string::npos || object_expression.find("::") != string::npos ||
				HasUnresolvedTemplateParameter(object_type, function_context, substitutions);
			object_type = FunctionArgumentObjectType(object_type, function_context);
			if(complex_member_object && !object_type.empty() && !member_spelling.empty() && member_spelling[0] != '~') {
				dot_member_owner = object_type;
				dot_member_name = member_spelling;
				dot_member_candidates = FindFunctionDefinitions(dot_member_name, dot_member_owner);
				// FindFunctionDefinitions intentionally has a short-name fallback for
				// source/generated owners.  Once the object type is concrete, prefer
				// candidates owned by that class (or by its recorded source primary) so
				// an unrelated same-named member cannot win before inherited lookup.
				vector<const TemplateDefinition*> exact_candidates;
				for(size_t candidate = 0; candidate < dot_member_candidates.size(); ++candidate) {
					const TemplateDefinition* definition = dot_member_candidates[candidate];
					if(!definition) continue;
					bool exact = definition->owner == dot_member_owner ||
						LastComponent(definition->owner) == LastComponent(dot_member_owner);
					map<string, string>::const_iterator source_base = specialization_bases_.find(
						LastComponent(dot_member_owner));
					if(source_base != specialization_bases_.end())
						exact = exact || definition->owner == source_base->second ||
							LastComponent(definition->owner) == LastComponent(source_base->second);
					if(exact) exact_candidates.push_back(definition);
				}
				if(!exact_candidates.empty()) dot_member_candidates.swap(exact_candidates);
				dot_member_call = true;
			}
		}
		// The `<...>` in `declval<T>().member(...)` belongs to the object
		// expression, not to the member callee.  Discard the earlier unqualified
		// explicit-template interpretation once dot lookup has identified the
		// member call.
		if(dot_member_call) {
			explicit_definition = 0;
				explicit_arguments.clear();
				explicit_base_name.clear();
		}
		if(callee[callee.size() - 1] == ')' && ResolveCallableTemporaryCallResult(callee,
			function_context, context, substitutions, actual_types, result,
			has_nested_object_type ? &nested_object_type : 0)) return true;
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
				try {
					if(!InferFunctionTypeArguments(*call_operators[candidate], actual_types,
						&arguments, substitutions, function_context)) continue;
				} catch(const PA18SubstitutionFailure&) {
					// A dependent default or return type can fail while probing one
					// callable.  That removes this candidate; it is not a failure of
					// the surrounding unevaluated expression.
					continue;
				}
				*result = FunctionResultType(*call_operators[candidate], arguments,
					function_context, &substitutions);
				if(!result->empty()) return true;
			}
		}
		vector<const TemplateDefinition*> candidates;
		if(explicit_definition) {
			candidates = qualified_member_call && !qualified_owner.empty() ?
				FindFunctionDefinitions(explicit_base_name, qualified_owner) :
				FindFunctionDefinitions(explicit_base_name, function_context);
			if(candidates.empty()) candidates.push_back(explicit_definition);
		}
		else if(qualified_member_call) candidates = qualified_candidates;
		else if(dot_member_call) candidates = dot_member_candidates;
		else candidates = FindFunctionDefinitions(callee, function_context);
		// Ordinary (non-template) members are kept in the typed signature index,
		// not in TemplateDefinition lookup.  A dependent `decltype` probe can
		// still call one after its object type has become concrete, so consult that
		// index when member-template lookup has no candidate.
		if(candidates.empty() && dot_member_call) {
			const FunctionSignature* ordinary = FindFunctionSignature(dot_member_name,
				dot_member_owner);
			if(ordinary && !ordinary->deleted && ordinary->parameters &&
				ordinary->result_specifiers) {
				size_t actual = 0;
				bool viable = true, ellipsis = false;
				for(size_t parameter = 0; parameter < ordinary->parameters->children.size();
					++parameter) {
					const CPPGMAstNodePtr node = ordinary->parameters->children[parameter];
					if(!node) continue;
					if(node->kind == "ellipsis") {
						ellipsis = true;
						continue;
					}
					if(node->kind != "parameter-declaration") continue;
					if(actual >= actual_types.size()) {
						if(!ChildOfKindLocal(node, "default-argument")) viable = false;
						continue;
					}
					if(!FunctionArgumentViable(ParameterTypeSpelling(node),
						actual_types[actual++], function_context)) viable = false;
				}
				if(actual != actual_types.size() && !ellipsis) viable = false;
				if(viable) {
					string ordinary_result = NodeTypeSpelling(ordinary->result_specifiers) +
						ReturnDeclaratorSuffix(ordinary->declarator);
					ordinary_result = NormalizeTypeArgument(ResolveAlias(RewriteText(
						ordinary_result, function_context, substitutions, 0), function_context));
					if(!ordinary_result.empty()) {
						*result = ordinary_result;
						return true;
					}
				}
			}
		}
		if(generated_callee && GeneratedFunctionCallResultType(callee, context,
			substitutions, actual_types, result)) return true;
		if(candidates.empty()) {
			set<string> active;
			map<const TemplateDefinition*, string> concrete_owners;
				if(qualified_member_call && !qualified_owner.empty())
					CollectInheritedMemberTemplates(qualified_owner, qualified_member_name,
						substitutions, function_context, &candidates, &active, &concrete_owners);
				else if(dot_member_call && !dot_member_owner.empty())
					CollectInheritedMemberTemplates(dot_member_owner, dot_member_name,
						substitutions, function_context, &candidates, &active, &concrete_owners);
				else CollectInheritedMemberTemplates(context, callee, substitutions,
				function_context, &candidates, &active, &concrete_owners);
		}
		if(candidates.empty() && GeneratedFunctionCallResultType(callee, context,
			substitutions, actual_types, result)) return true;
		string selected_result;
		bool selected_ellipsis = true;
		bool all_candidates_deleted = !candidates.empty();
		for(size_t i = 0; i < candidates.size(); ++i) {
			const TemplateDefinition& definition = *candidates[i];
			// A deleted function may still be found by ordinary overload lookup, but
			// selecting it in an unevaluated call is an invalid expression-SFINAE
			// probe.  Reject only this candidate and continue looking for the fallback;
			// the declaration itself must never be materialized as the probe result.
			if(definition.deleted) continue;
			all_candidates_deleted = false;
			map<string, string> candidate_substitutions = substitutions;
			ApplyFriendClassSubstitutions(definition, actual_types, function_context,
				&candidate_substitutions);
				vector<string> arguments;
			const bool complete = explicit_definition &&
				explicit_arguments.size() == definition.parameters.size();
			if(complete) arguments = explicit_arguments;
				else {
					try {
						if(!InferFunctionTypeArguments(definition, actual_types, &arguments,
							candidate_substitutions, function_context,
							explicit_definition ? &explicit_arguments : 0)) continue;
					} catch(const PA18SubstitutionFailure&) {
						continue;
					}
				}
				try {
					if(!FunctionArgumentsViable(definition, arguments, actual_types,
					function_context, &candidate_substitutions,
					explicit_definition ? &explicit_arguments : 0)) {
						continue;
					}
				if(HasAbstractFunctionParameter(definition, arguments,
					function_context, candidate_substitutions)) {
					continue;
				}
					} catch(const PA18SubstitutionFailure&) {
						continue;
					}
			string candidate_result;
			try {
				candidate_result = FunctionResultType(definition, arguments,
					function_context, &candidate_substitutions,
					explicit_definition ? &explicit_arguments : 0);
				} catch(const PA18SubstitutionFailure&) {
					continue;
				}
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
		if(all_candidates_deleted) return false;
		if(ResolveConstructedCallResult(callee, context, substitutions, actual_types, result)) return true;
		if(!explicit_definition) {
			string selected_result;
			bool selected_ellipsis = true;
			bool selected_any = false;
			auto template_component = [](string raw) {
				const size_t open = raw.find('<');
				if(open != string::npos) raw.erase(open);
				return LastComponent(raw);
			};
			string generated_source_owner;
			map<string, string> generated_owner_substitutions = substitutions;
			if(qualified_member_call) {
				map<string, string>::const_iterator generated_base =
					specialization_bases_.find(LastComponent(qualified_owner));
				if(generated_base != specialization_bases_.end()) {
					generated_source_owner = generated_base->second;
					vector<string> generated_arguments;
					map<string, vector<string> >::const_iterator recorded_arguments =
						specialization_arguments_.find(LastComponent(qualified_owner));
					if(recorded_arguments != specialization_arguments_.end())
						generated_arguments = recorded_arguments->second;
					const size_t open = generated_source_owner.find('<');
					if(open != string::npos || !generated_arguments.empty()) {
						string argument_text;
						size_t close = string::npos;
						if(open != string::npos)
							TemplateRange(generated_source_owner, open, &argument_text, &close);
						if(open == string::npos || close != string::npos) {
							const string primary_name = open == string::npos ? generated_source_owner :
								generated_source_owner.substr(0, open);
							const TemplateDefinition* primary = FindDefinition(primary_name,
								function_context);
							if(!primary) primary = FindDefinition(LastComponent(primary_name),
								function_context);
							const vector<string> arguments = generated_arguments.empty() ?
								SplitTemplateArguments(argument_text) : generated_arguments;
							if(primary) for(size_t parameter = 0;
								parameter < primary->parameters.size() && parameter < arguments.size();
								++parameter)
								if(!primary->parameters[parameter].name.empty())
									generated_owner_substitutions[primary->parameters[parameter].name] =
										arguments[parameter];
						}
					}
				}
			}
			int selected_score = -1;
			const auto ordinary_argument_score = [this, &function_context](
				const string& parameter, const string& actual) {
				const string expected = FunctionArgumentObjectType(parameter, function_context);
				const string received = FunctionArgumentObjectType(actual, function_context);
				if(expected.empty() || received.empty()) return 0;
				if(expected == received) return 4;
				const size_t expected_pointer = expected.rfind('*');
				const size_t received_pointer = received.rfind('*');
				if(expected_pointer != string::npos && received_pointer != string::npos) {
					const string expected_target = CanonicalSpelling(expected.substr(0, expected_pointer));
					const string received_target = CanonicalSpelling(received.substr(0, received_pointer));
					if(expected_target == "void") return 1;
					const string qualified_expected = CanonicalSpelling(QualifyTypeArgument(
						expected_target, function_context));
					const string qualified_received = CanonicalSpelling(QualifyTypeArgument(
						received_target, function_context));
					if(!qualified_expected.empty() && qualified_expected == qualified_received) return 3;
				}
				return 1;
			};
			for(map<string, vector<FunctionSignature> >::const_iterator overload =
				function_overloads_.begin(); overload != function_overloads_.end(); ++overload) {
				bool matches = overload->first == callee;
				if(!matches && qualified_member_call) {
					const size_t separator = overload->first.rfind("::");
					if(separator != string::npos &&
						overload->first.substr(separator + 2) == qualified_member_name) {
						const string candidate_owner = overload->first.substr(0, separator);
						matches = generated_source_owner.empty() ?
							(candidate_owner == qualified_owner ||
								template_component(candidate_owner) == template_component(qualified_owner)) :
							template_component(candidate_owner) == template_component(generated_source_owner);
					}
				}
				if(!matches) continue;
				for(size_t candidate = 0; candidate < overload->second.size(); ++candidate) {
					const FunctionSignature& signature = overload->second[candidate];
					const CPPGMAstNodePtr parameters = signature.parameters;
					bool ellipsis = false, viable = true;
					int candidate_score = 0;
					size_t actual = 0;
					if(parameters) for(size_t parameter = 0; parameter < parameters->children.size();
						++parameter) {
						const CPPGMAstNodePtr item = parameters->children[parameter];
						if(item && item->kind == "ellipsis") {
							ellipsis = true;
							break;
						}
						if(!item || item->kind != "parameter-declaration") continue;
						if(IsFunctionParameterPack(item)) {
							ellipsis = true;
							break;
						}
						if(actual >= actual_types.size()) {
							if(!ChildOfKindLocal(item, "default-argument")) viable = false;
							continue;
						}
						string parameter_type = ParameterTypeSpelling(item);
						try {
							parameter_type = NormalizeTypeArgument(ReplaceIdentifiers(
								RewriteText(parameter_type, function_context,
									generated_owner_substitutions, 0), generated_owner_substitutions));
						} catch(const PA18SubstitutionFailure&) {
							viable = false;
						}
						if(viable && HasUnresolvedTemplateParameter(parameter_type,
							function_context, generated_owner_substitutions)) viable = false;
						const bool parameter_viable = viable && FunctionArgumentViable(parameter_type,
							actual_types[actual], function_context);
						if(!parameter_viable) viable = false;
						else candidate_score += ordinary_argument_score(parameter_type,
							actual_types[actual]);
						++actual;
						}
					if(signature.deleted || !viable || actual != actual_types.size() && !ellipsis)
						continue;
					string ordinary_result = NodeTypeSpelling(signature.result_specifiers) +
						ReturnDeclaratorSuffix(signature.declarator);
					try {
						ordinary_result = NormalizeTypeArgument(ResolveAlias(RewriteText(
							ordinary_result, function_context, generated_owner_substitutions, 0),
							function_context));
					} catch(const PA18SubstitutionFailure&) { ordinary_result.clear(); }
					if(ordinary_result.empty()) continue;
					if(!selected_any || candidate_score > selected_score ||
						(candidate_score == selected_score && selected_ellipsis && !ellipsis)) {
						selected_result = ordinary_result;
						selected_ellipsis = ellipsis;
						selected_score = candidate_score;
						selected_any = true;
					}
				}
			}
			if(selected_any) { *result = selected_result; return true; }
			const FunctionSignature* signature = FindFunctionSignature(callee, context);
			if(signature && !signature->deleted && signature->result_specifiers) {
				*result = NodeTypeSpelling(signature->result_specifiers) +
					ReturnDeclaratorSuffix(signature->declarator);
				return !result->empty();
			}
			if(ResolveCallableVariableCallResult(callee, function_context, context,
				substitutions, actual_types, result)) return true;
		}
		return false;
	}

} // namespace pa18_templates_internal
