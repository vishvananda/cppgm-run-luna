#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

	bool PA18TemplateExpander::FunctionCallResultType(string expression, const string& context, const map<string, string>& substitutions, string* result)
	{
		if(!result) return false;
		const bool generated_callee = expression.find("__ov") != string::npos ||
			expression.find("__inst_") != string::npos;
		const string call_key = "call|" + expression + "@" + context;
		if(!generated_callee && !active_function_results_.insert(call_key).second) return false;
		ActiveFunctionResultScope call_scope(this, generated_callee ? string() : call_key);
		// Expand a trailing function-parameter pack before splitting the
		// unevaluated call's arguments.  Otherwise `declval<_Args>()...` is
		// treated as one dependent operand and a valid return-type probe fails.
		expression = ExpandPackCallText(expression, active_pack_substitutions_);
		string callee, argument_text;
		if(!SplitTextCall(expression, &callee, &argument_text)) return false;
		callee = StripTextParentheses(callee);
		if(callee.empty()) return false;
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
			if(!explicit_definition) {
				vector<const TemplateDefinition*> inherited;
				set<string> active;
				map<const TemplateDefinition*, string> concrete_owners;
				CollectInheritedMemberTemplates(context, base, substitutions,
					function_context, &inherited, &active, &concrete_owners);
				if(!inherited.empty()) explicit_definition = inherited[0];
			}
		if(!explicit_definition || explicit_definition->class_template) return false;
			explicit_arguments = SplitTemplateArguments(base_arguments);
			ExpandExplicitFunctionArguments(base_arguments, function_context,
				substitutions, &explicit_arguments);
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
		if(generated_callee && GeneratedFunctionCallResultType(callee, function_context,
			substitutions, actual_types, result)) return true;
		if(candidates.empty()) {
			set<string> active;
			map<const TemplateDefinition*, string> concrete_owners;
			CollectInheritedMemberTemplates(context, callee, substitutions,
				function_context, &candidates, &active, &concrete_owners);
		}
		if(candidates.empty() && GeneratedFunctionCallResultType(callee, function_context,
			substitutions, actual_types, result)) return true;
		string selected_result;
		bool selected_ellipsis = true;
		for(size_t i = 0; i < candidates.size(); ++i) {
			const TemplateDefinition& definition = *candidates[i];
			vector<string> arguments;
			const bool complete = explicit_definition &&
				explicit_arguments.size() == definition.parameters.size();
			if(complete) arguments = explicit_arguments;
			else if(!InferFunctionTypeArguments(definition, actual_types, &arguments,
					substitutions, function_context, explicit_definition ? &explicit_arguments : 0))
				continue;
			try {
				if(!FunctionArgumentsViable(definition, arguments, actual_types,
					function_context)) continue;
				if(HasAbstractFunctionParameter(definition, arguments,
					function_context, substitutions)) continue;
			} catch(const PA18SubstitutionFailure&) {
				continue;
			}
			string candidate_result;
			try {
				candidate_result = FunctionResultType(definition, arguments,
					function_context, &substitutions);
				} catch(const PA18SubstitutionFailure&) { continue; }
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

} // namespace pa18_templates_internal
