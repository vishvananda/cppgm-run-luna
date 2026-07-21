#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::MatchTypePattern(string pattern, string actual,
	const set<string>& parameter_names, map<string, string>* inferred,
	const string& context) const
{
		pattern = CanonicalSpelling(pattern);
		actual = ResolveAlias(actual, context);
		pattern = CanonicalSpelling(pattern);
		const bool pattern_pointer = !pattern.empty() && pattern[pattern.size() - 1] == '*';
		const bool actual_pointer = !actual.empty() && actual[actual.size() - 1] == '*';
		const bool pattern_cv_qualified =
			pattern.compare(0, 6, "const ") == 0 ||
			pattern.compare(0, 9, "volatile ") == 0;
		// For T*, cv-qualification before the pointed-to type belongs to T.
		// It must survive deduction; stripping it here previously made a
		// candidate deduced from T* and const T& appear consistent when it was
		// not.  A pattern that explicitly spells const T* still consumes that
		// qualification as part of the pattern.
		const bool preserve_pointee_cv = pattern_pointer && actual_pointer &&
			!pattern_cv_qualified;
		while(pattern.compare(0, 6, "const ") == 0) pattern = CanonicalSpelling(pattern.substr(6));
		while(pattern.compare(0, 9, "volatile ") == 0) pattern = CanonicalSpelling(pattern.substr(9));
		if(!preserve_pointee_cv) {
			while(actual.compare(0, 6, "const ") == 0) actual = CanonicalSpelling(actual.substr(6));
			while(actual.compare(0, 9, "volatile ") == 0) actual = CanonicalSpelling(actual.substr(9));
		}
		for(;;) {
			bool removed = false;
			if(pattern.size() > 6 && pattern.compare(pattern.size() - 6, 6, " const") == 0) {
				pattern = CanonicalSpelling(pattern.substr(0, pattern.size() - 6));
				removed = true;
			} else if(pattern.size() > 9 &&
				pattern.compare(pattern.size() - 9, 9, " volatile") == 0) {
				pattern = CanonicalSpelling(pattern.substr(0, pattern.size() - 9));
				removed = true;
			}
			if(!removed) break;
		}
		for(;;) {
			bool removed = false;
			if(actual.size() > 6 && actual.compare(actual.size() - 6, 6, " const") == 0) {
				actual = CanonicalSpelling(actual.substr(0, actual.size() - 6));
				removed = true;
			} else if(actual.size() > 9 &&
				actual.compare(actual.size() - 9, 9, " volatile") == 0) {
				actual = CanonicalSpelling(actual.substr(0, actual.size() - 9));
				removed = true;
			}
			if(!removed) break;
		}
		// A forwarding-reference pattern deduces the underlying parameter as
		// an lvalue reference when the argument is an lvalue.  Handle the
		// two-character `&&` suffix before ordinary reference stripping;
		// otherwise `Args&&` becomes `Args&` and is no longer recognized as the
		// template parameter name.
		if(pattern.size() > 2 && pattern.compare(pattern.size() - 2, 2, "&&") == 0) {
			const string base = CanonicalSpelling(pattern.substr(0, pattern.size() - 2));
			if(parameter_names.find(base) != parameter_names.end()) {
				(*inferred)[base] = actual;
				return true;
			}
		}
		if(!pattern.empty() && pattern[pattern.size() - 1] == '&') {
			pattern.erase(pattern.size() - 1);
			if(!actual.empty() && actual[actual.size() - 1] == '&') actual.erase(actual.size() - 1);
		}
		if(!pattern.empty() && pattern[pattern.size() - 1] == '*') {
			if(actual.empty() || actual[actual.size() - 1] != '*') return false;
			pattern.erase(pattern.size() - 1);
			actual.erase(actual.size() - 1);
		}
		pattern = CanonicalSpelling(pattern);
		actual = CanonicalSpelling(actual);
		while(pattern.size() > 6 && pattern.compare(pattern.size() - 6, 6, " const") == 0)
			pattern = CanonicalSpelling(pattern.substr(0, pattern.size() - 6));
		while(pattern.size() > 9 && pattern.compare(pattern.size() - 9, 9, " volatile") == 0)
			pattern = CanonicalSpelling(pattern.substr(0, pattern.size() - 9));
		while(actual.size() > 6 && actual.compare(actual.size() - 6, 6, " const") == 0)
			actual = CanonicalSpelling(actual.substr(0, actual.size() - 6));
		while(actual.size() > 9 && actual.compare(actual.size() - 9, 9, " volatile") == 0)
			actual = CanonicalSpelling(actual.substr(0, actual.size() - 9));
		// Function-pointer parameters carry their nested declarator in the
		// spelling (`R(*)(A...)`).  Match the return type and each parameter
		// recursively so a dependent parameter such as `T` can be deduced from
		// the function pointer supplied at the call site.
		const size_t pattern_function = pattern.find(")(");
		const size_t actual_function = actual.find(")(");
		if(pattern_function != string::npos && actual_function != string::npos &&
			pattern[pattern.size() - 1] == ')' && actual[actual.size() - 1] == ')') {
			const size_t pattern_pointer = pattern.rfind("(*", pattern_function);
			const size_t actual_pointer = actual.rfind("(*", actual_function);
			const size_t pattern_reference = pattern.rfind("(&", pattern_function);
			const size_t actual_reference = actual.rfind("(&", actual_function);
			const size_t pattern_owner = pattern_pointer != string::npos ? pattern_pointer : pattern_reference;
			const size_t actual_owner = actual_pointer != string::npos ? actual_pointer : actual_reference;
			if(pattern_owner == string::npos || actual_owner == string::npos ||
				pattern.substr(pattern_owner, pattern_function - pattern_owner + 1) !=
				actual.substr(actual_owner, actual_function - actual_owner + 1)) return false;
			const string pattern_result = pattern.substr(0, pattern_owner);
			const string actual_result = actual.substr(0, actual_owner);
			const vector<string> pattern_parameters = SplitTemplateArguments(pattern.substr(
				pattern_function + 2, pattern.size() - pattern_function - 3));
			const vector<string> actual_parameters = SplitTemplateArguments(actual.substr(
				actual_function + 2, actual.size() - actual_function - 3));
			if(pattern_parameters.size() != actual_parameters.size() ||
				!MatchTypePattern(pattern_result, actual_result, parameter_names, inferred, context)) return false;
			for(size_t parameter = 0; parameter < pattern_parameters.size(); ++parameter)
				if(!MatchTypePattern(pattern_parameters[parameter], actual_parameters[parameter],
					parameter_names, inferred, context)) return false;
			return true;
		}
		if(parameter_names.find(pattern) != parameter_names.end()) {
			map<string, string>::const_iterator prior = inferred->find(pattern);
			if(prior != inferred->end() &&
				CanonicalSpelling(ResolveAlias(prior->second, context)) !=
				CanonicalSpelling(ResolveAlias(actual, context))) return false;
			(*inferred)[pattern] = actual;
			return true;
		}
		const size_t pattern_open = pattern.find('<');
		if(pattern_open != string::npos) {
			string pattern_arguments;
			size_t pattern_close = string::npos;
			if(!TemplateRange(pattern, pattern_open, &pattern_arguments, &pattern_close)) return false;
			const size_t actual_open = actual.find('<');
			const vector<string> pattern_parts = SplitTemplateArguments(pattern_arguments);
			vector<string> actual_parts;
			if(actual_open == string::npos) {
				map<string, vector<string> >::const_iterator specialization =
					specialization_arguments_.find(LastComponent(actual));
				map<string, string>::const_iterator base = specialization_bases_.find(LastComponent(actual));
				if(specialization == specialization_arguments_.end() || base == specialization_bases_.end() ||
					LastComponent(base->second) != LastComponent(pattern.substr(0, pattern_open))) return false;
				if(pattern_parts.empty()) return true;
				actual_parts = specialization->second;
			} else {
				string actual_arguments;
				size_t actual_close = string::npos;
				if(!TemplateRange(actual, actual_open, &actual_arguments, &actual_close)) return false;
				const string pattern_base = LastComponent(pattern.substr(0, pattern_open));
				const string actual_base = LastComponent(actual.substr(0, actual_open));
				if(pattern_base != actual_base) {
					// A pointer to a derived class can bind to a dependent base
					// pointer.  Recover the concrete base spelling from the actual
					// class template before matching its arguments (for example,
					// vector<int>* against store<N,U>*).
					const TemplateDefinition* actual_definition = FindDefinition(
						actual.substr(0, actual_open), context);
					if(actual_definition && actual_definition->class_template &&
						actual_definition->declaration) {
						const vector<string> concrete_parts =
							SplitTemplateArguments(actual_arguments);
						map<string, string> class_substitutions;
						for(size_t parameter = 0; parameter < actual_definition->parameters.size() &&
							parameter < concrete_parts.size(); ++parameter)
							class_substitutions[actual_definition->parameters[parameter].name] =
								concrete_parts[parameter];
						for(size_t child = 0; child < actual_definition->declaration->children.size(); ++child) {
							const CPPGMAstNodePtr clause = actual_definition->declaration->children[child];
							if(!clause || clause->kind != "base-clause") continue;
							for(size_t base_index = 0; base_index < clause->children.size(); ++base_index) {
								const CPPGMAstNodePtr specifier = clause->children[base_index];
								const CPPGMAstNodePtr base_name = ChildOfKindLocal(specifier, "base-name");
								if(!base_name) continue;
								const string concrete_base = CanonicalSpelling(ReplaceIdentifiers(
									base_name->value, class_substitutions));
								if(MatchTypePattern(pattern, concrete_base, parameter_names,
									inferred, context)) return true;
							}
						}
					}
					return false;
				}
				actual_parts = SplitTemplateArguments(actual_arguments);
			}
			if(pattern_parts.size() == 1 && pattern_parts[0].size() > 3 &&
				pattern_parts[0].compare(pattern_parts[0].size() - 3, 3, "...") == 0) {
				const string pack_pattern = CanonicalSpelling(pattern_parts[0].substr(
					0, pattern_parts[0].size() - 3));
				if(parameter_names.find(pack_pattern) == parameter_names.end()) return false;
				string combined;
				for(size_t i = 0; i < actual_parts.size(); ++i) {
					map<string, string> one;
					if(!MatchTypePattern(pack_pattern, actual_parts[i], parameter_names,
						&one, context)) return false;
					map<string, string>::const_iterator value = one.find(pack_pattern);
					if(value == one.end()) return false;
					if(!combined.empty()) combined += ",";
					combined += value->second;
				}
				if(inferred) (*inferred)[pack_pattern] = combined;
				return true;
			}
			if(pattern_parts.size() != actual_parts.size()) return false;
			for(size_t i = 0; i < pattern_parts.size(); ++i)
				if(!MatchTypePattern(pattern_parts[i], actual_parts[i], parameter_names, inferred, context))
					return false;
			return true;
		}
		return pattern == actual;
}

bool PA18TemplateExpander::InferArgument(const CPPGMAstNodePtr& expression,
	string* result, const map<string, string>& substitutions,
	const string& context) const
{
		if(!expression || !result) return false;
		if(expression->kind == "literal") {
			const string value = expression->value;
			if(value.find('"') != string::npos) *result = "const char*";
			else if(value.find('\'') != string::npos) *result = "char";
			else *result = InferLiteralArgumentType(value);
			return true;
		}
		if(expression->kind == "keyword-literal") {
			*result = "bool";
			return true;
		}
		if(expression->kind == "member-expression" && expression->children.size() >= 2) {
			string object_type;
			if(expression->children[0] && expression->children[0]->kind == "keyword-literal" &&
				RemoveMarker(expression->children[0]->value) == "this") object_type = context;
			else InferArgument(expression->children[0], &object_type, substitutions, context);
			const string member = expression->children[1] ?
				LastComponent(expression->children[1]->value) : string();
			set<string> active;
			if(!object_type.empty() && !member.empty() && FindClassMemberType(
				object_type, member, substitutions, context, result, &active)) return true;
		}
		if(expression->kind == "cast-expression" && !expression->children.empty()) {
			const CPPGMAstNodePtr type_id = expression->children[0];
			if(type_id && type_id->kind == "type-id") {
				*result = NormalizeTypeArgument(TypeIdSpelling(type_id));
				return !result->empty();
			}
		}
		if(expression->kind == "id-expression") {
			// The same source identifier can be reused by a later local
			// declaration.  Prefer the parameter belonging to the current class
			// constructor over the collector's translation-unit fallback map.
			const CPPGMAstNodePtr current_class = FindClassDeclaration(context, context);
			if(current_class) for(size_t member = 0; member < current_class->children.size(); ++member) {
				const CPPGMAstNodePtr declaration = current_class->children[member];
				if(!declaration || declaration->kind != "special-member-definition") continue;
				const CPPGMAstNodePtr clause = DescendantOfKind(declaration, "parameter-clause");
				if(!clause) continue;
				for(size_t parameter = 0; parameter < clause->children.size(); ++parameter) {
					const CPPGMAstNodePtr candidate = clause->children[parameter];
					if(!candidate || candidate->kind != "parameter-declaration" ||
						candidate->children.size() < 2 ||
						FirstIdentifierLocal(candidate->children[1]) != expression->value) continue;
					*result = CanonicalSpelling(ReplaceIdentifiers(
						ParameterTypeSpelling(candidate), substitutions));
					return !result->empty();
				}
			}
			for(map<string, vector<string> >::const_iterator pack =
				active_pack_identifier_substitutions_.begin();
				pack != active_pack_identifier_substitutions_.end(); ++pack) {
				if(find(pack->second.begin(), pack->second.end(), expression->value) ==
					pack->second.end()) continue;
				map<string, string>::const_iterator source = variable_types_.find(pack->first);
				if(source == variable_types_.end()) continue;
				*result = ReplaceIdentifiers(ResolveAlias(source->second, context), substitutions);
				if(!result->empty()) return true;
			}
			map<string, string>::const_iterator found = variable_types_.find(LastComponent(expression->value));
			if(found != variable_types_.end()) {
				*result = ReplaceIdentifiers(ResolveAlias(found->second, context), substitutions);
				return true;
			}
			const string marker = FunctionMarker(expression->value, context);
			if(!marker.empty()) {
				*result = marker;
				return true;
			}
		}
		if(expression->kind == "call-expression" && !expression->children.empty() && expression->children[0] && expression->children[0]->kind == "member-expression") {
			const CPPGMAstNodePtr callee = expression->children[0];
			string object_type;
			if(callee->children.size() >= 2) {
				if(callee->children[0] && callee->children[0]->kind == "keyword-literal" &&
					RemoveMarker(callee->children[0]->value) == "this") object_type = context;
				else InferArgument(callee->children[0], &object_type, substitutions, context);
			}
			const string member = callee->children.size() > 1 && callee->children[1] ?
				LastComponent(callee->children[1]->value) : string();
			set<string> active;
			if(!object_type.empty() && !member.empty() && FindClassMemberType(
				object_type, member, substitutions, context, result, &active)) return true;
		}
		if(expression->kind == "call-expression" && !expression->children.empty() && expression->children[0] && expression->children[0]->kind == "id-expression") {
			if(!expression->template_primary.empty() && !expression->template_arguments.empty()) {
				const vector<const TemplateDefinition*> materialized =
					FindFunctionDefinitions(expression->template_primary, context);
				for(size_t candidate = 0; candidate < materialized.size(); ++candidate) {
					const TemplateDefinition* definition = materialized[candidate];
					if(!definition || definition->parameters.size() !=
						expression->template_arguments.size() || !definition->declaration ||
						definition->declaration->children.empty()) continue;
					map<string, string> function_substitutions;
					for(size_t parameter = 0; parameter < definition->parameters.size(); ++parameter)
						if(!definition->parameters[parameter].name.empty())
							function_substitutions[definition->parameters[parameter].name] =
								expression->template_arguments[parameter];
					const CPPGMAstNodePtr declarator = FunctionDeclarator(definition->declaration);
					string return_type = NodeTypeSpelling(definition->declaration->children[0]) +
						DeclaratorSuffix(declarator);
					*result = CollapseReferenceSpelling(ReplaceIdentifiers(
						return_type, function_substitutions));
					if(!result->empty()) return true;
				}
			}
			const string member = LastComponent(expression->children[0]->value);
			string owner; for(string current = context; ; ) {
				if(class_contexts_.find(current) != class_contexts_.end()) { owner = current; break; }
				if(current.empty()) break;
				const size_t separator = current.rfind("::");
				if(separator == string::npos) current.clear(); else current.erase(separator);
			}
			set<string> active;
			if(!owner.empty() && !member.empty() && FindClassMemberType(
				owner, member, substitutions, context, result, &active)) return true;
		}
		if(expression->kind == "call-expression" && !expression->children.empty() &&
			expression->children[0] && expression->children[0]->kind == "id-expression") {
			const string callee = LastComponent(expression->children[0]->value);
			string nested_class;
			const string qualified_callee = expression->children[0]->value;
			if(class_contexts_.find(qualified_callee) != class_contexts_.end() ||
				class_declarations_.find(qualified_callee) != class_declarations_.end())
				nested_class = qualified_callee;
			for(string current = context; nested_class.empty(); ) {
				const string candidate = JoinPath(current, callee);
				if(class_contexts_.find(candidate) != class_contexts_.end() || class_declarations_.find(candidate) != class_declarations_.end()) { nested_class = candidate; break; }
				if(current.empty()) break;
				const size_t separator = current.rfind("::");
				if(separator == string::npos) current.clear(); else current.erase(separator);
			}
			if(!nested_class.empty()) {
				*result = nested_class;
				return true;
			}
			const FunctionSignature* signature = FindFunctionSignature(
				expression->children[0]->value, context);
			if(signature && signature->result_specifiers) {
				*result = NodeTypeSpelling(signature->result_specifiers);
				if(!result->empty()) return true;
			}
			// A type functional-cast such as T() or a local typedef cast is
			// represented as a call-expression by the parser.  Preserve the
			// original spelling fallback after class-constructor and ordinary
			// function-result lookup so deduction can resolve the type alias.
			*result = ResolveAlias(expression->children[0]->value, context);
			return !result->empty();
		}
		if(expression->kind == "binary-expression" &&
			InferBinaryArgument(expression, result, substitutions, context)) return true;
		if(expression->kind == "unary-expression" && !expression->children.empty()) {
			const string op = RemoveMarker(expression->value);
			if(op == "&" && InferArgument(expression->children[0], result, substitutions, context)) {
				*result = CanonicalSpelling(*result + "*");
				return true;
			}
			if(op == "*" && InferArgument(expression->children[0], result, substitutions, context)) {
				if(!result->empty() && result->at(result->size() - 1) == '*')
					result->erase(result->size() - 1);
				*result = CanonicalSpelling(*result + "&");
				return true;
			}
		}
		return false;
}

bool PA18TemplateExpander::InferFunctionArguments(const TemplateDefinition& definition,
	const CPPGMAstNodePtr& call, vector<string>* result,
	const map<string, string>& substitutions, const string& context,
	const vector<string>* explicit_prefix,
	map<string, vector<string> >* inferred_pack_values) const
{
		if(!call || call->children.size() < 2 || !result) return false;
		const CPPGMAstNodePtr declarator = FunctionDeclarator(definition.declaration);
		const CPPGMAstNodePtr parameters = DescendantOfKind(declarator, "parameter-clause");
		const CPPGMAstNodePtr arguments = call->children[1] &&
			call->children[1]->kind == "argument-list" ? call->children[1] :
			ChildOfKindLocal(call->children[1], "argument-list");
		if(!parameters || !arguments) return false;
		bool has_pack = false;
		size_t required_parameters = 0;
		for(size_t i = 0; i < parameters->children.size(); ++i) {
			const CPPGMAstNodePtr parameter = parameters->children[i];
			if(!parameter || parameter->kind != "parameter-declaration") continue;
			if(IsFunctionParameterPack(parameter)) has_pack = true;
			else if(!ChildOfKindLocal(parameter, "default-argument")) ++required_parameters;
		}
		if(arguments->children.size() < required_parameters ||
			(!has_pack && arguments->children.size() > parameters->children.size())) return false;
		map<string, string> inferred;
		map<string, vector<string> > inferred_packs;
		set<string> parameter_names;
		vector<string> deferred_function_patterns;
		vector<CPPGMAstNodePtr> deferred_function_arguments;
		for(size_t i = 0; i < definition.parameters.size(); ++i) {
			parameter_names.insert(definition.parameters[i].name);
			if(explicit_prefix && i < explicit_prefix->size() && !definition.parameters[i].pack)
				inferred[definition.parameters[i].name] = (*explicit_prefix)[i];
		}
		size_t argument_index = 0;
		for(size_t i = 0; i < parameters->children.size(); ++i) {
			const CPPGMAstNodePtr parameter = parameters->children[i];
			if(!parameter || parameter->kind != "parameter-declaration") continue;
			const string pattern = parameter->children.size() > 1 && parameter->children[1] &&
				ChildOfKindLocal(parameter->children[1], "nested-declarator") &&
				ChildOfKindLocal(parameter->children[1], "parameter-clause") ?
				FunctionTypeSpelling(parameter) : ParameterTypeSpelling(parameter);
			const bool pack_parameter = IsFunctionParameterPack(parameter);
			size_t trailing_fixed = 0;
			if(pack_parameter) for(size_t later = i + 1; later < parameters->children.size(); ++later)
				if(parameters->children[later] && parameters->children[later]->kind ==
					"parameter-declaration" && !IsFunctionParameterPack(parameters->children[later]))
					++trailing_fixed;
			const size_t pack_count = pack_parameter && arguments->children.size() >= argument_index + trailing_fixed ?
				arguments->children.size() - argument_index - trailing_fixed : 0;
			const size_t visits = pack_parameter ? pack_count :
				(argument_index < arguments->children.size() ? 1 : 0);
			for(size_t visit = 0; visit < visits; ++visit) {
				string type;
				const CPPGMAstNodePtr argument_expression = arguments->children[argument_index];
				bool inferred_argument = InferArgument(
					argument_expression, &type, substitutions, context);
				const vector<string> function_types = pattern.find(")(") != string::npos ?
					FunctionExpressionTypes(argument_expression, context) : vector<string>();
				if(!function_types.empty()) {
					// An overloaded function name has no single expression type.  Defer
					// its deduction until the other call arguments establish the shared
					// template parameter (for example apply(square, 4)).
					if(function_types.size() > 1 && argument_expression &&
						(argument_expression->kind == "id-expression" ||
						 (argument_expression->kind == "unary-expression" &&
						  RemoveMarker(argument_expression->value) == "&"))) {
						deferred_function_patterns.push_back(pattern);
						deferred_function_arguments.push_back(argument_expression);
						inferred_argument = false;
					} else {
						type = function_types[0];
						inferred_argument = true;
					}
				}
				if(inferred_argument && pack_parameter && pattern.size() > 2 &&
					pattern.compare(pattern.size() - 2, 2, "&&") == 0 &&
					argument_expression &&
					(argument_expression->kind == "id-expression" ||
					 argument_expression->kind == "member-expression" ||
					 argument_expression->kind == "subscript-expression"))
					type = CanonicalSpelling(type + "&");
				if(inferred_argument) {
					map<string, string> one;
					string match_pattern = pattern;
					if(!inferred.empty() && pattern.find('<') != string::npos &&
						pattern.find("::") != string::npos) {
						map<string, string> pattern_substitutions = substitutions;
						for(map<string, string>::const_iterator inferred_value = inferred.begin();
							inferred_value != inferred.end(); ++inferred_value)
							pattern_substitutions[inferred_value->first] = inferred_value->second;
						// Resolving a dependent member alias may materialize its class
						// template, so use the expander's normal stateful rewriter while
						// deducing this candidate.
						match_pattern = const_cast<PA18TemplateExpander*>(this)->RewriteText(
							pattern, context, pattern_substitutions, 0);
						match_pattern = NormalizeTypeArgument(ReplaceIdentifiers(
							match_pattern, pattern_substitutions));
						match_pattern = ResolveAlias(match_pattern, context);
					}
					const bool matched = MatchTypePattern(match_pattern, type, parameter_names, &one, context);
					if(!matched) {
						bool dependent = false;
						for(size_t p = 0; p < definition.parameters.size(); ++p)
							if(definition.parameters[p].name == pattern) dependent = true;
						if(dependent) return false;
						string fixed_pattern = CanonicalSpelling(match_pattern);
						string fixed_actual = CanonicalSpelling(type);
						while(fixed_pattern.compare(0, 6, "const ") == 0)
							fixed_pattern = CanonicalSpelling(fixed_pattern.substr(6));
						while(fixed_actual.compare(0, 6, "const ") == 0)
							fixed_actual = CanonicalSpelling(fixed_actual.substr(6));
						while(fixed_pattern.size() > 6 &&
							fixed_pattern.compare(fixed_pattern.size() - 6, 6, " const") == 0)
							fixed_pattern = CanonicalSpelling(fixed_pattern.substr(0, fixed_pattern.size() - 6));
						while(fixed_actual.size() > 6 &&
							fixed_actual.compare(fixed_actual.size() - 6, 6, " const") == 0)
							fixed_actual = CanonicalSpelling(fixed_actual.substr(0, fixed_actual.size() - 6));
						if((fixed_pattern.find('*') != string::npos) !=
							(fixed_actual.find('*') != string::npos)) return false;
						while(!fixed_pattern.empty() && (fixed_pattern[fixed_pattern.size() - 1] == '&' ||
							fixed_pattern[fixed_pattern.size() - 1] == '*'))
							fixed_pattern.erase(fixed_pattern.size() - 1);
						while(!fixed_actual.empty() && (fixed_actual[fixed_actual.size() - 1] == '&' ||
							fixed_actual[fixed_actual.size() - 1] == '*'))
							fixed_actual.erase(fixed_actual.size() - 1);
						while(fixed_pattern.size() > 6 &&
							fixed_pattern.compare(fixed_pattern.size() - 6, 6, " const") == 0)
							fixed_pattern = CanonicalSpelling(fixed_pattern.substr(0, fixed_pattern.size() - 6));
						while(fixed_actual.size() > 6 &&
							fixed_actual.compare(fixed_actual.size() - 6, 6, " const") == 0)
							fixed_actual = CanonicalSpelling(fixed_actual.substr(0, fixed_actual.size() - 6));
						if(FindClassDeclaration(fixed_pattern, context) &&
							FindClassDeclaration(fixed_actual, context) &&
							LastComponent(fixed_pattern) != LastComponent(fixed_actual)) return false;
					} else {
						for(map<string, string>::const_iterator it = one.begin(); it != one.end(); ++it) {
							const size_t template_index = find_if(definition.parameters.begin(),
								definition.parameters.end(), [&](const TemplateParameter& candidate) {
									return candidate.name == it->first;
								}) - definition.parameters.begin();
							if(template_index < definition.parameters.size() &&
								definition.parameters[template_index].pack) {
								const vector<string> values = SplitTemplateArguments(it->second);
								if(values.empty()) inferred_packs[it->first].push_back(it->second);
								else inferred_packs[it->first].insert(
									inferred_packs[it->first].end(), values.begin(), values.end());
							}
						else {
							map<string, string>::const_iterator prior = inferred.find(it->first);
							if(prior != inferred.end() &&
								CanonicalSpelling(ResolveAlias(prior->second, context)) !=
								CanonicalSpelling(ResolveAlias(it->second, context))) return false;
							inferred[it->first] = it->second;
						}
						}
					}
					}
				++argument_index;
			}
		}
		if(argument_index != arguments->children.size()) return false;
		for(size_t deferred = 0; deferred < deferred_function_patterns.size(); ++deferred) {
			const vector<string> function_types = FunctionExpressionTypes(
				deferred_function_arguments[deferred], context);
			bool matched = false;
			for(size_t candidate = 0; candidate < function_types.size(); ++candidate) {
				map<string, string> one = inferred;
				if(!MatchTypePattern(deferred_function_patterns[deferred], function_types[candidate],
					parameter_names, &one, context)) continue;
				inferred = one;
				matched = true;
				break;
			}
			if(!matched) return false;
		}
		for(size_t i = 0; i < definition.parameters.size(); ++i) {
			const TemplateParameter& parameter = definition.parameters[i];
			if(parameter.pack) {
				map<string, vector<string> >::const_iterator found = inferred_packs.find(parameter.name);
				if(found != inferred_packs.end()) {
					result->insert(result->end(), found->second.begin(), found->second.end());
					if(inferred_pack_values) (*inferred_pack_values)[parameter.name] = found->second;
				} else if(inferred_pack_values)
					(*inferred_pack_values)[parameter.name] = vector<string>();
				continue;
			}
			map<string, string>::const_iterator found = inferred.find(parameter.name);
			if(found != inferred.end()) result->push_back(found->second);
			else if(!parameter.default_type.empty()) result->push_back(
				ReplaceIdentifiers(parameter.default_type, inferred));
			else return false;
		}
		return true;
}

string PA18TemplateExpander::RewriteText(string raw, const string& context,
	const map<string, string>& substitutions, bool* template_replaced,
	bool resolve_alias, bool resolve_member)
{
		if(template_replaced) *template_replaced = false;
		if(raw.compare(0, 8, "operator") == 0) {
			const string suffix = raw.substr(8);
			map<string, string>::const_iterator operator_substitution = substitutions.find(suffix);
			if(operator_substitution != substitutions.end()) {
				raw = "operator" + operator_substitution->second;
				if(template_replaced) *template_replaced = true;
			}
		}
		raw = RewriteDecltypeText(raw, context, substitutions, template_replaced);
		for(size_t search = 0; search < raw.size(); ++search) {
			if(raw[search] != '<') continue;
			size_t begin = 0;
			string base;
			if(!TemplateBase(raw, search, &begin, &base)) continue;
		string arguments_text;
		size_t close = string::npos;
		const bool has_range = TemplateRange(raw, search, &arguments_text, &close);
		if(!has_range) continue;
		// A specialization that is already materialized is also a valid current
		// instantiation when the reference appears through a nested dependent
		// type.  In that path the local substitution map may contain only `T`,
		// so the primary template name is not otherwise rewritten and would start
		// the same instantiation again.
		const vector<string> current_arguments = SplitTemplateArguments(arguments_text);
		bool replaced_current_specialization = false;
		for(map<string, vector<string> >::const_iterator generated =
			specialization_arguments_.begin(); generated != specialization_arguments_.end();
			++generated) {
			map<string, string>::const_iterator generated_base =
				specialization_bases_.find(generated->first);
			if(generated_base == specialization_bases_.end() ||
				class_contexts_.find(generated->first) == class_contexts_.end() ||
				LastComponent(generated_base->second) != LastComponent(base) ||
				generated->second.size() != current_arguments.size()) continue;
			bool same_arguments = true;
			for(size_t argument = 0; argument < current_arguments.size(); ++argument) {
				const string actual = NormalizeTypeArgument(ReplaceIdentifiers(
					CanonicalSpelling(current_arguments[argument]), substitutions));
				const string expected = NormalizeTypeArgument(CanonicalSpelling(
					generated->second[argument]));
				if(actual != expected) {
					same_arguments = false;
					break;
				}
			}
			const bool qualified_member = close + 2 < raw.size() &&
				raw.compare(close + 1, 2, "::") == 0;
			if(same_arguments && !qualified_member) {
				raw.replace(begin, close - begin + 1, generated->first);
				if(template_replaced) *template_replaced = true;
				replaced_current_specialization = true;
				break;
			}
		}
		if(replaced_current_specialization) {
			search = begin + raw.size();
			continue;
		}
		// A qualified reference to the class currently being materialized must
		// resolve to that concrete class.  Looking up the primary template again
		// would recursively instantiate the same specialization while rewriting
		// members such as `trait<T>::value`.
		map<string, string>::const_iterator current_substitution =
			substitutions.find(base);
		if(current_substitution != substitutions.end() &&
			current_substitution->second.find('<') == string::npos) {
			string current_name = current_substitution->second;
			bool current_class = class_contexts_.find(current_name) != class_contexts_.end();
			if(!current_class && !context.empty())
				current_class = class_contexts_.find(JoinPath(context, current_name)) !=
					class_contexts_.end();
			if(current_class) {
				raw.replace(begin, close - begin + 1, current_name);
				if(template_replaced) *template_replaced = true;
				search = begin + current_name.size();
				continue;
			}
		}
			const TemplateDefinition* definition = FindDefinition(base, context);
			string lookup_base = base;
			map<string, string>::const_iterator qualified_alias = substitutions.find(base);
			if(qualified_alias != substitutions.end() &&
				qualified_alias->second.find("::") != string::npos)
				lookup_base = qualified_alias->second;
			if(!definition) {
				const size_t separator = base.find("::");
				if(separator != string::npos) {
					const map<string, string>::const_iterator alias = substitutions.find(
						base.substr(0, separator));
					if(alias != substitutions.end()) {
						lookup_base = alias->second + base.substr(separator);
						definition = FindDefinition(lookup_base, context);
					}
				}
			}
			if(lookup_base != base) definition = FindDefinition(lookup_base, context);
			if(!definition) continue;
			const vector<string> raw_template_args = SplitTemplateArguments(arguments_text);
			vector<string> args;
			for(size_t raw_argument = 0; raw_argument < raw_template_args.size(); ++raw_argument) {
				const string source_argument = CanonicalSpelling(raw_template_args[raw_argument]);
				if(source_argument.size() > 3 &&
					source_argument.compare(source_argument.size() - 3, 3, "...") == 0) {
					const string prefix = source_argument.substr(0, source_argument.size() - 3);
					string pack_name;
					for(size_t character = 0; character < prefix.size();) {
						if(!IsIdentifierCharacter(prefix[character])) { ++character; continue; }
						const size_t begin_name = character;
						while(character < prefix.size() && IsIdentifierCharacter(prefix[character])) ++character;
						const string word = prefix.substr(begin_name, character - begin_name);
						if(active_pack_substitutions_.find(word) != active_pack_substitutions_.end()) {
							pack_name = word;
							break;
						}
					}
					map<string, vector<string> >::const_iterator pack =
						active_pack_substitutions_.find(pack_name);
					if(pack != active_pack_substitutions_.end()) {
						for(size_t element = 0; element < pack->second.size(); ++element) {
							map<string, string> one = substitutions;
							one[pack_name] = pack->second[element];
							args.push_back(CollapseReferenceSpelling(
								ReplaceIdentifiers(prefix, one)));
						}
						continue;
					}
				}
				args.push_back(raw_template_args[raw_argument]);
			}
			for(size_t i = 0; i < args.size(); ++i) {
				args[i] = NormalizeTypeArgument(RewriteText(args[i], context, substitutions, 0));
				args[i] = NormalizeTypeArgument(ReplaceIdentifiers(args[i], substitutions));
				args[i] = ResolveAlias(args[i], context);
				args[i] = NormalizeTypeArgument(RewriteText(args[i], context, substitutions, 0));
				args[i] = ResolveAlias(args[i], context);
				args[i] = QualifyTypeArgument(args[i], context, definition->owner);
			}
			if(args.size() < definition->parameters.size()) {
				map<string, string> default_substitutions = substitutions;
				for(size_t i = 0; i < args.size() && i < definition->parameters.size(); ++i)
					default_substitutions[definition->parameters[i].name] = args[i];
				for(size_t i = args.size(); i < definition->parameters.size(); ++i) {
					string argument;
					map<string, string>::const_iterator substituted = default_substitutions.find(
						definition->parameters[i].name);
					if(substituted != default_substitutions.end()) argument = substituted->second;
					else if(!definition->parameters[i].default_type.empty())
						argument = RewriteText(definition->parameters[i].default_type, context,
							default_substitutions, 0);
					if(argument.empty()) break;
					argument = NormalizeTypeArgument(ReplaceIdentifiers(argument,
						default_substitutions));
					argument = ResolveAlias(argument, context);
					argument = QualifyTypeArgument(argument, context, definition->owner);
					args.push_back(argument);
					default_substitutions[definition->parameters[i].name] = argument;
				}
			}
			// Resolve non-type arguments in the surrounding substitution scope
			// before Instantiate creates its fresh local map.  This preserves
			// member constants such as `num` while materializing a nested
			// specialization inside `ratio1<N>`.
			map<string, string> argument_substitutions = substitutions;
			for(size_t i = 0; i < args.size() && i < definition->parameters.size(); ++i) {
				if(!definition->parameters[i].type) {
					PA19IntegralValue value;
					args[i] = ResolveIntegralArgument(definition->parameters[i], args[i],
						context, argument_substitutions, &value);
				}
				argument_substitutions[definition->parameters[i].name] = args[i];
			}
			if(!definition->alias_template)
				definition = SelectClassTemplateDefinition(definition, args, context);
			if(resolve_member && definition->class_template && close + 2 < raw.size() &&
				raw.compare(close + 1, 2, "::") == 0) {
				RecordTemplateArrayValues(*definition, args, context, substitutions);
				size_t nested_begin = close + 3;
				while(nested_begin < raw.size() && IsIdentifierCharacter(raw[nested_begin])) ++nested_begin;
				const string nested = raw.substr(close + 3, nested_begin - (close + 3));
				if(!nested.empty()) {
					const string member_type = TemplateMemberType(*definition, args, nested, context);
					if(!member_type.empty() && member_type.find('[') == string::npos) {
						raw.replace(begin, nested_begin - begin, member_type);
						if(template_replaced) *template_replaced = true;
						search = begin + member_type.size();
						continue;
					}
					requested_nested_classes_[definition->qualified_name].insert(nested);
					requested_nested_classes_[LastComponent(definition->qualified_name)].insert(nested);
				}
			}
			const string local_name = Instantiate(*definition, args, context);
			string replacement = local_name;
			const string qualifier = PrefixComponent(lookup_base);
			if(!qualifier.empty()) replacement = qualifier + "::" + local_name;
			if(close + 1 < raw.size() && IsIdentifierCharacter(raw[close + 1]) &&
				!replacement.empty() && IsIdentifierCharacter(replacement[replacement.size() - 1]))
				replacement += ' ';
			raw.replace(begin, close - begin + 1, replacement);
			if(template_replaced) *template_replaced = true;
			search = begin + replacement.size();
		}
		raw = ReplaceIdentifiers(raw, substitutions);
		if(!resolve_alias || raw.find("::") == string::npos) return raw;
		// A qualified static integral member is an expression here, not a type
		// alias.  Keep its registered spelling intact so a dependent non-type
		// default can be evaluated by the typed constant table.
		if(constant_values_.find(raw) != constant_values_.end()) return raw;
		return ResolveAlias(raw, context);
}

bool PA18TemplateExpander::TransformPackChild(
	const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& original_child,
	const string& child_context,
	const map<string, string>& substitutions,
	map<string, string>* local_substitutions,
	const CPPGMAstNodePtr& result)
{
	if(input->kind == "base-clause" && original_child &&
		original_child->kind == "base-specifier" &&
		ChildOfKindLocal(original_child, "pack-expansion")) {
		const CPPGMAstNodePtr original_base = ChildOfKindLocal(original_child, "base-name");
		const string pack_name = PackExpansionIdentifier(original_base);
		map<string, vector<string> >::const_iterator pack =
			active_pack_substitutions_.find(pack_name);
		if(pack != active_pack_substitutions_.end()) {
			for(size_t element = 0; element < pack->second.size(); ++element) {
				CPPGMAstNodePtr expanded = CloneNode(original_child);
				RemoveParameterPackMarkers(expanded);
				const CPPGMAstNodePtr base = ChildOfKindLocal(expanded, "base-name");
				if(base && original_base) {
					map<string, string> one = substitutions;
					one[pack_name] = pack->second[element];
					base->value = ReplaceIdentifiers(original_base->value, one);
				}
				CPPGMAstNodePtr child = TransformNode(expanded, child_context,
					substitutions);
				if(child) result->children.push_back(child);
			}
		}
		return true;
	}
	if(original_child && original_child->kind == "pack-expansion-expression") {
		ExpandPackChild(input, original_child, child_context, substitutions,
			local_substitutions, result);
		return true;
	}
	if(input->kind == "parameter-clause" && original_child &&
		original_child->kind == "parameter-declaration" &&
		IsFunctionParameterPack(original_child)) {
		const string pack_name = PackExpansionIdentifier(original_child);
		map<string, vector<string> >::const_iterator values =
			active_pack_substitutions_.find(pack_name);
		if(values != active_pack_substitutions_.end()) {
			const string identifier = ParameterIdentifier(original_child);
			vector<string>& expanded_identifiers =
				active_pack_identifier_substitutions_[identifier];
			for(size_t element = 0; element < values->second.size(); ++element) {
				ostringstream pack_suffix;
				pack_suffix << element + 1;
				const string expanded_name = identifier.empty() || element == 0 ? identifier :
					identifier + "__pack" + pack_suffix.str();
				if(!identifier.empty()) expanded_identifiers.push_back(expanded_name);
				map<string, string> one = substitutions;
				one[pack_name] = values->second[element];
				if(!identifier.empty()) one[identifier] = expanded_name;
				CPPGMAstNodePtr copy = CloneNode(original_child);
				RemoveParameterPackMarkers(copy);
				CPPGMAstNodePtr child = TransformNode(copy, child_context, one);
				if(child) result->children.push_back(child);
			}
			return true;
		}
	}
	return false;
}
void PA18TemplateExpander::TransformRegularChildren(const CPPGMAstNodePtr& input,
	const string& child_context, const string& function_context,
	const map<string, string>& substitutions,
	map<string, string>* local_substitutions,
	const CPPGMAstNodePtr& result)
{
	for(size_t i = 0; i < input->children.size(); ++i) {
		const CPPGMAstNodePtr original_child = input->children[i];
		if(TransformPackChild(input, original_child, child_context, substitutions,
			local_substitutions, result)) continue;
					if(input->kind == "decl-specifier" && input->value.find("decltype(") != string::npos &&
						original_child && (original_child->kind == "call-expression" || original_child->kind == "binary-expression")) continue;
				if(SkipUnusedNestedClass(input, original_child, child_context, substitutions, i)) continue;
			if(original_child && original_child->kind == "namespace-alias-definition") {
				const CPPGMAstNodePtr target = ChildOfKindLocal(original_child, "target");
				if(target && !target->value.empty() && local_substitutions)
					(*local_substitutions)[original_child->value] = RewriteText(
						target->value, child_context, *local_substitutions, 0, false);
				continue;
			}
				const string node_context = input->kind == "function-definition" && original_child && original_child->kind == "compound-statement" ? function_context : child_context;
				const CPPGMAstNodePtr using_target = original_child && original_child->kind == "using-declaration" ? ChildOfKindLocal(original_child, "target") : CPPGMAstNodePtr();
			const bool drop_function_using = using_target && IsOrdinaryTemplateUsingTarget(
				using_target->value, node_context) && class_contexts_.find(node_context) == class_contexts_.end(); CPPGMAstNodePtr child;
			if(input->kind == "using-declaration" && original_child && original_child->kind == "target") {
				child = CloneNode(original_child);
				const string raw_target = original_child->value;
				const size_t separator = raw_target.rfind("::");
				if(separator != string::npos &&
					raw_target.substr(0, separator) == raw_target.substr(separator + 2)) {
					map<string, string>::const_iterator alias = local_substitutions->find(
						raw_target.substr(0, separator));
					if(alias != local_substitutions->end() && !alias->second.empty())
						child->value = alias->second + "::" + LastComponent(alias->second);
					else child->value = RewriteText(raw_target, node_context,
						*local_substitutions, 0, false, false);
				} else child->value = RewriteText(raw_target, node_context,
					*local_substitutions, 0, false, false);
				} else child = TransformNode(original_child, node_context, *local_substitutions);
				if(child && input->kind == "array-suffix" && !child->children.empty() &&
					child->children[0]) {
					PA19IntegralValue bound;
					const string expression = ConstantExpressionSpelling(child->children[0]);
					if(EvaluateIntegralText(expression, node_context, *local_substitutions, &bound))
						child->children[0] = CPPGMAstNodePtr(new CPPGMAstNode(
							"literal", IntegralValueSpelling(bound)));
				}
				if(child && input->kind == "class-specifier" &&
					child->kind == "simple-declaration")
					RecordConstantArrayDeclaration(child, child_context,
						*local_substitutions);
				if(!child && input->kind == "decl-specifier-seq" && original_child &&
					(original_child->kind == "class-specifier" ||
						original_child->kind == "class-forward-declaration")) {
					map<string, string>::const_iterator promoted = local_class_names_.find(
						JoinPath(child_context, LastComponent(original_child->value)));
					if(promoted != local_class_names_.end())
						result->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
							"decl-specifier", "TT_IDENTIFIER:" + LastComponent(promoted->second))));
				}
				if(child && !drop_function_using && !(input->kind == "compound-statement" && !substitutions.empty() &&
					(original_child->kind == "alias-declaration" || (original_child->kind == "simple-declaration" &&
					SpellNode(original_child->children.empty() ? CPPGMAstNodePtr() : original_child->children[0]).find("typedef") != string::npos)))) result->children.push_back(child);
				if(original_child && original_child->kind == "alias-declaration" && !original_child->value.empty() &&
					!original_child->children.empty())
				{
					(*local_substitutions)[original_child->value] = RewriteText(
						TypeIdSpelling(original_child->children[0]), child_context,
						*local_substitutions, 0);
				}
			if(original_child && original_child->kind == "using-declaration") {
				const CPPGMAstNodePtr target = ChildOfKindLocal(original_child, "target");
				if(target && !target->value.empty()) {
					const string target_name = LastComponent(target->value);
					const string owner = PrefixComponent(target->value);
					const CPPGMAstNodePtr rewritten = child ?
						ChildOfKindLocal(child, "target") : CPPGMAstNodePtr();
					bool function_target = false;
					if(!owner.empty()) {
						const string target_suffix = "::" + target->value;
						for(map<string, TemplateDefinition>::const_iterator candidate = definitions_.begin();
							candidate != definitions_.end(); ++candidate)
							if(!candidate->second.class_template &&
								(candidate->second.qualified_name == target->value ||
									(candidate->second.qualified_name.size() > target_suffix.size() &&
									 candidate->second.qualified_name.compare(
										candidate->second.qualified_name.size() - target_suffix.size(),
										target_suffix.size(), target_suffix) == 0))) {
								function_target = true;
								break;
							}
						const string signature_suffix = "::" + target->value;
						for(map<string, FunctionSignature>::const_iterator candidate = function_signatures_.begin();
							candidate != function_signatures_.end(); ++candidate)
							if(candidate->first == target->value ||
								(candidate->first.size() > signature_suffix.size() &&
								 candidate->first.compare(candidate->first.size() - signature_suffix.size(),
									signature_suffix.size(), signature_suffix) == 0)) {
								function_target = true;
								break;
							}
						const string rewritten_owner = rewritten ? PrefixComponent(rewritten->value) : owner;
						const CPPGMAstNodePtr owner_declaration = FindClassDeclaration(rewritten_owner.empty() ? owner : rewritten_owner, child_context);
						if(owner_declaration) for(size_t member = 0; member < owner_declaration->children.size(); ++member) {
								const CPPGMAstNodePtr declaration = owner_declaration->children[member];
								if(declaration && declaration->kind == "function-definition" &&
									declaration->children.size() > 1 &&
									LastComponent(FirstIdentifierLocal(declaration->children[1])) == target_name) {
									function_target = true;
									break;
								}
							}
					}
					if(!owner.empty() && !function_target)
						(*local_substitutions)[target_name] = rewritten &&
							!rewritten->value.empty() ? rewritten->value : target->value;
				}
				}
			if(original_child && original_child->kind == "using-directive") RecordUsingDirective(original_child, local_substitutions);
			if(original_child && original_child->kind == "simple-declaration" && !original_child->children.empty() &&
				SpellNode(original_child->children[0]).find("typedef") != string::npos &&
				(!substitutions.empty() || input->value.find('<') != string::npos))
				RecordTypedefSubstitutions(original_child, child_context, local_substitutions);
		}
}

CPPGMAstNodePtr PA18TemplateExpander::RewriteRegularNodeValue(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions,
	const CPPGMAstNodePtr& result, string* promoted_name)
{
		bool template_replaced = false;
		const bool type_spelling = input->kind == "decl-specifier" ||
			input->kind == "type-name" || input->kind == "type-specifier";
	result->value = RewriteText(input->value, context, substitutions,
			&template_replaced, !type_spelling, true);
		// Non-type template substitutions are semantic values, not names.  Keep
		// them as literal AST nodes so PA11/lowering resolve the same typed fact
		// that selected the specialization.
		if(input->kind == "id-expression") {
			map<string, PA19IntegralValue>::const_iterator typed =
				active_integral_substitutions_.find(RemoveMarker(input->value));
			if(typed != active_integral_substitutions_.end() && typed->second.known) {
				const string spelling = IntegralValueSpelling(typed->second);
				if(typed->second.type.name == "bool")
					return CPPGMAstNodePtr(new CPPGMAstNode("keyword-literal",
						PA19Raw(typed->second) ? "KW_TRUE:true" : "KW_FALSE:false"));
				CPPGMAstNodePtr literal(new CPPGMAstNode("literal", spelling));
				literal->initializer_form = input->initializer_form;
				return literal;
			}
		}
		if((type_spelling || input->kind == "id-expression") &&
			result->value.find('<') != string::npos)
			result->value = RewriteText(result->value, context, substitutions, &template_replaced);
		if(input->kind == "target") {
			const string raw_target = RemoveMarker(input->value);
			const size_t separator = raw_target.rfind("::");
			if(separator != string::npos && raw_target.substr(0, separator) ==
				raw_target.substr(separator + 2)) {
				map<string, string>::const_iterator alias = substitutions.find(
					raw_target.substr(0, separator));
				if(alias != substitutions.end() && !alias->second.empty())
					result->value = alias->second + "::" + LastComponent(alias->second);
			}
		}
		if(input->kind == "decl-specifier" || input->kind == "type-name" ||
			input->kind == "type-specifier") {
			const size_t marker_colon = result->value.find(':');
			string marker;
			if(marker_colon != string::npos) {
				const string prefix = result->value.substr(0, marker_colon);
				if(prefix == "TT_IDENTIFIER" || prefix.compare(0, 3, "KW_") == 0 ||
					prefix.compare(0, 3, "OP_") == 0)
					marker = result->value.substr(0, marker_colon + 1);
			}
			const string spelling = RemoveMarker(result->value);
			string qualified = QualifyTypeArgument(spelling, context);
			string resolved = ResolveAlias(qualified, context);
			if(substitutions.empty() && input->value.find('<') == string::npos)
				resolved = qualified;
			if(resolved.find('<') != string::npos)
				resolved = RewriteText(resolved, context, substitutions, 0);
			if(resolved.find('(') != string::npos && resolved.find(')') != string::npos)
				resolved = qualified;
			if(resolved != qualified) qualified = resolved;
			if(qualified != spelling) result->value = marker + qualified;
			if(input->kind == "decl-specifier" && marker.empty() &&
				qualified != spelling && result->value.find(':') == string::npos)
				result->value = "TT_IDENTIFIER:" + qualified;
			if(input->kind == "decl-specifier" && marker.empty() &&
				qualified.find(' ') != string::npos)
				result->value = "TT_IDENTIFIER:" + qualified;
		}
		string promoted_local_class;
		if((input->kind == "class-specifier" || input->kind == "class-forward-declaration") &&
			function_contexts_.find(context) != function_contexts_.end()) {
			map<string, string>::const_iterator local = local_class_names_.find(
				JoinPath(context, LastComponent(input->value)));
			if(local != local_class_names_.end()) {
				promoted_local_class = LastComponent(local->second);
				result->value = promoted_local_class;
			}
		}
		if(input->kind == "decl-specifier" && template_replaced &&
			result->value.find(':') == string::npos)
			result->value = "TT_IDENTIFIER:" + result->value;
	*promoted_name = promoted_local_class;
	return CPPGMAstNodePtr();
}
CPPGMAstNodePtr PA18TemplateExpander::FinishRegularNode(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions,
	const CPPGMAstNodePtr& result, const string& promoted_local_class)
{
		string child_context = context;
		if(input->kind == "class-specifier" || input->kind == "class-forward-declaration")
			child_context = JoinPath(context, LastComponent(input->value));
		string function_context = context;
		if(input->kind == "function-definition") {
			const string function_name = DeclarationName(input);
			string function_owner;
			if(input->children.size() > 1 && input->children[1]) {
				const string qualified_name = RewriteText(
					FirstIdentifierLocal(input->children[1]), context, substitutions, 0,
					false, false);
				function_owner = PrefixComponent(qualified_name);
				if(!function_owner.empty()) function_owner = ResolveGeneratedFunctionOwner(
					function_owner, context, &child_context);
			}
			function_context = JoinPath(function_owner.empty() ? context : function_owner,
				function_name);
			if(!function_name.empty() && LastComponent(context) == function_name)
				function_context = context;
		}
		map<string, string> local_substitutions = substitutions;
		TransformRegularChildren(input, child_context, function_context, substitutions,
			&local_substitutions, result);
		if((input->kind == "parameter-declaration" || input->kind == "type-id") &&
			!substitutions.empty())
			for(map<string, string>::const_iterator substitution = substitutions.begin();
				substitution != substitutions.end(); ++substitution)
				if(!substitution->second.empty() && substitution->second[substitution->second.size() - 1] == '&' &&
					ContainsName(input, substitution->first) &&
					ContainsName(input, "&&"))
					CollapseForwardingReference(result);
		RewriteTemplateInitializer(input, context, substitutions, result);
		if(input->kind == "simple-declaration") ReifyReferenceType(result);
		if(input->kind == "binary-expression") {
			InstantiateOperatorTemplate(result, context, substitutions);
			RewriteOperatorFunctionArgument(result, context, substitutions);
		}
		if(!promoted_local_class.empty()) {
			// A local class is promoted into a translation-unit class so the
			// later semantic pass can name it.  Its constructor declaration must
			// follow that promoted identity as well; retaining the source-local
			// `pair_like` name makes the generated class appear to have no viable
			// constructor when it is initialized.
			for(size_t child = 0; child < result->children.size(); ++child) {
				const CPPGMAstNodePtr member = result->children[child];
				if(!member || (member->kind != "special-member-definition" &&
					member->kind != "special-member-declaration")) continue;
				member->value = promoted_local_class;
				const CPPGMAstNodePtr declarator = FunctionDeclarator(member);
				RenameParameterIdentifier(declarator, promoted_local_class);
			}
			const map<string, string>::const_iterator owner = function_owners_.find(context);
			generated_by_owner_[owner == function_owners_.end() ? PrefixComponent(context) :
				owner->second].push_back(result);
			return CPPGMAstNodePtr();
		}
	return result;
}
CPPGMAstNodePtr PA18TemplateExpander::TransformRegularNode(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions)
{
		CPPGMAstNodePtr user_defined_literal = RewriteUserDefinedIntegerLiteral(
			input, context, substitutions);
		if(user_defined_literal) return user_defined_literal;
		if(input->kind == "sizeof-pack-expression") {
			const string name = PackExpansionIdentifier(
				input->children.empty() ? CPPGMAstNodePtr() : input->children[0]);
			map<string, vector<string> >::const_iterator typed =
				active_pack_substitutions_.find(name);
			map<string, vector<string> >::const_iterator named =
				active_pack_identifier_substitutions_.find(name);
			const vector<string>* values = typed != active_pack_substitutions_.end() ?
				&typed->second : named != active_pack_identifier_substitutions_.end() ?
				&named->second : 0;
			if(values) {
				ostringstream count;
				count << values->size();
				CPPGMAstNodePtr result(new CPPGMAstNode("sizeof-pack-expression",
					count.str()));
	return result;
	}
		}
		CPPGMAstNodePtr template_call = RewriteTemplateCastCall(input, context, substitutions);
		if(template_call) return template_call;
		CPPGMAstNodePtr result(new CPPGMAstNode(input->kind, input->value));
		result->initializer_form = input->initializer_form;
		result->dependent_base_lookup = input->dependent_base_lookup;
		result->materialize_object_address = input->materialize_object_address;
		result->source_token_begin = input->source_token_begin;
		result->source_token_end = input->source_token_end;
		if(input->kind == "simple-declaration" &&
			SpellNode(input).find("decltype(") != string::npos) {
			result->materialize_object_address = true;
			const string spelling = SpellNode(input);
			const size_t open = spelling.find("decltype(");
			const size_t close = open == string::npos ? string::npos :
				spelling.find(')', open + 9);
			if(close != string::npos) {
				const string expression = spelling.substr(open + 9, close - open - 9);
				const size_t call = expression.find('(');
				if(call != string::npos)
					result->materialize_object_name = expression.substr(0, call);
			}
		}
	string promoted_local_class;
	CPPGMAstNodePtr rewritten = RewriteRegularNodeValue(
		input, context, substitutions, result, &promoted_local_class);
	if(rewritten) return rewritten;
	return FinishRegularNode(input, context, substitutions, result,
		promoted_local_class);
}

} // namespace pa18_templates_internal
