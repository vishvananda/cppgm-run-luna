#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::InferArgument(const CPPGMAstNodePtr& expression,
	string* result, const map<string, string>& substitutions,
	const string& context) const
	{
		if(!expression || !result) return false;
		if(!expression->inferred_type.empty()) {
			*result = expression->inferred_type;
			return true;
		}
		if(expression->kind == "literal") {
			const string value = expression->value;
			if(value.find('"') != string::npos) *result = "const char*";
			else if(value.find('\'') != string::npos) *result = "char";
			else *result = InferLiteralArgumentType(value);
			return true;
		}
		if(expression->kind == "keyword-literal") {
			if(RemoveMarker(expression->value) == "this") {
				string object_type;
				map<string, string>::const_iterator function_owner = function_owners_.find(context);
				if(function_owner != function_owners_.end()) object_type = function_owner->second;
				for(string current = object_type.empty() ? context : string(); !current.empty(); ) {
					const TemplateDefinition* current_definition = FindDefinition(current, context);
					if(class_contexts_.find(current) != class_contexts_.end() ||
						(current_definition && current_definition->class_template)) {
						object_type = current;
						break;
					}
					const size_t separator = current.rfind("::");
					if(separator == string::npos) current.clear();
					else current.erase(separator);
				}
				if(object_type.empty()) object_type = context;
				*result = CanonicalSpelling(object_type);
				return !result->empty();
			}
			*result = "bool";
			return true;
		}
		if(expression->kind == "member-expression" && expression->children.size() >= 2) {
			string object_type;
			if(expression->children[0] && expression->children[0]->kind == "keyword-literal" &&
				RemoveMarker(expression->children[0]->value) == "this") {
				map<string, string>::const_iterator function_owner = function_owners_.find(context);
				if(function_owner != function_owners_.end()) object_type = function_owner->second;
				for(string current = object_type.empty() ? context : string(); !current.empty(); ) {
					const TemplateDefinition* current_definition = FindDefinition(current, context);
					if(class_contexts_.find(current) != class_contexts_.end() ||
						(current_definition && current_definition->class_template)) {
						object_type = current;
						break;
					}
					const size_t separator = current.rfind("::");
					if(separator == string::npos) current.clear();
					else current.erase(separator);
				}
				if(object_type.empty()) object_type = context;
			}
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
				const string return_type = NodeTypeSpelling(signature->result_specifiers) +
					ReturnDeclaratorSuffix(signature->declarator);
				*result = CanonicalSpelling(ResolveAlias(ReplaceIdentifiers(
					return_type, substitutions), context));
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
			if(!definition.parameters[i].name.empty()) parameter_names.insert(
				definition.parameters[i].name);
			if(explicit_prefix && i < explicit_prefix->size() &&
				!definition.parameters[i].pack && !definition.parameters[i].name.empty())
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
				// Function-parameter deduction applies the by-value adjustment to an
				// xvalue/lvalue argument before matching the parameter pattern.  The
				// typed return replay preserves `move_ptr`'s `&&`, but a parameter such
				// as `const Base<T>*` must deduce from the pointer object, not from the
				// reference category of the expression that produced it.
				if(inferred_argument && (pattern.empty() || pattern[pattern.size() - 1] != '&'))
					while(type.size() > 0 && type[type.size() - 1] == '&')
						type = CanonicalSpelling(type.substr(0, type.size() - 1));
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
				if(inferred_argument && pattern.size() > 2 &&
					pattern.compare(pattern.size() - 2, 2, "&&") == 0 &&
					argument_expression &&
					(argument_expression->kind == "id-expression" ||
						argument_expression->kind == "member-expression" ||
						argument_expression->kind == "subscript-expression"))
					if(type.empty() || type[type.size() - 1] != '&')
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
						const bool lvalue_reference_pattern = match_pattern.size() > 0 &&
							match_pattern[match_pattern.size() - 1] == '&' &&
							(match_pattern.size() < 2 || match_pattern[match_pattern.size() - 2] != '&');
						const bool rvalue_reference_pattern = match_pattern.size() > 1 &&
							match_pattern.compare(match_pattern.size() - 2, 2, "&&") == 0;
						if(lvalue_reference_pattern && match_pattern.find("const") == string::npos)
							return false;
						if(rvalue_reference_pattern) {
							const string reference_target = CanonicalSpelling(match_pattern.substr(
								0, match_pattern.size() - 2));
							if(!IsBuiltinArithmeticType(type) ||
								!IsBuiltinArithmeticType(ReplaceIdentifiers(reference_target, inferred))) return false;
						}
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

} // namespace pa18_templates_internal
