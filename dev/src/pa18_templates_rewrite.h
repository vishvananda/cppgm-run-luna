#pragma once
#include "pa18_templates_rewrite_lookup.h"
#include "pa18_templates_rewrite_decltype.h"
#include "pa18_templates_rewrite_instantiate.h"
bool MatchTypePattern(string pattern, string actual,
		const set<string>& parameter_names, map<string, string>* inferred,
		const string& context) const
	{
		pattern = CanonicalSpelling(pattern);
		actual = ResolveAlias(actual, context);
		pattern = CanonicalSpelling(pattern);
		while(pattern.compare(0, 6, "const ") == 0) pattern = CanonicalSpelling(pattern.substr(6));
		while(pattern.compare(0, 9, "volatile ") == 0) pattern = CanonicalSpelling(pattern.substr(9));
		while(actual.compare(0, 6, "const ") == 0) actual = CanonicalSpelling(actual.substr(6));
		while(actual.compare(0, 9, "volatile ") == 0) actual = CanonicalSpelling(actual.substr(9));
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
		if(parameter_names.find(pattern) != parameter_names.end()) {
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
				actual_parts = specialization->second;
			} else {
				string actual_arguments;
				size_t actual_close = string::npos;
				if(!TemplateRange(actual, actual_open, &actual_arguments, &actual_close)) return false;
				if(LastComponent(pattern.substr(0, pattern_open)) !=
					LastComponent(actual.substr(0, actual_open))) return false;
				actual_parts = SplitTemplateArguments(actual_arguments);
			}
			if(pattern_parts.size() != actual_parts.size()) return false;
			for(size_t i = 0; i < pattern_parts.size(); ++i)
				if(!MatchTypePattern(pattern_parts[i], actual_parts[i], parameter_names, inferred, context))
					return false;
			return true;
		}
		return pattern == actual;
	}
	CPPGMAstNodePtr FindClassDeclaration(string raw_class, const string& context) const
	{
		raw_class = CanonicalSpelling(raw_class);
		while(!raw_class.empty() && (raw_class[raw_class.size() - 1] == '&' ||
			raw_class[raw_class.size() - 1] == '*')) raw_class.erase(raw_class.size() - 1);
		raw_class = CanonicalSpelling(raw_class);
		map<string, CPPGMAstNodePtr>::const_iterator concrete = class_declarations_.find(raw_class);
		if(concrete != class_declarations_.end()) return concrete->second;
		map<string, string>::const_iterator specialization = specialization_bases_.find(
			LastComponent(raw_class));
		if(specialization != specialization_bases_.end()) raw_class = specialization->second;
		map<string, CPPGMAstNodePtr>::const_iterator direct = class_declarations_.find(raw_class);
		if(direct != class_declarations_.end()) return direct->second;
		for(string current = context; ; ) {
			const string candidate = JoinPath(current, raw_class);
			map<string, CPPGMAstNodePtr>::const_iterator found = class_declarations_.find(candidate);
			if(found != class_declarations_.end()) return found->second;
			if(current.empty()) break;
			const size_t separator = current.rfind("::");
			if(separator == string::npos) current.clear();
			else current.erase(separator);
		}
		CPPGMAstNodePtr result;
		for(map<string, CPPGMAstNodePtr>::const_iterator it = class_declarations_.begin();
			it != class_declarations_.end(); ++it) {
			if(LastComponent(it->first) != LastComponent(raw_class)) continue;
			if(result) return CPPGMAstNodePtr();
			result = it->second;
		}
		if(result) return result;
		const TemplateDefinition* definition = FindDefinition(raw_class, context);
		return definition && definition->class_template ? definition->declaration :
			CPPGMAstNodePtr();
	}
	bool FindClassMemberType(const string& raw_class, const string& member,
		const map<string, string>& substitutions, const string& context,
		string* result, set<string>* active) const
	{
		if(!result || !active) return false;
		string class_key = CanonicalSpelling(raw_class);
		bool object_const = false;
		while(class_key.compare(0, 6, "const ") == 0) {
			object_const = true;
			class_key = CanonicalSpelling(class_key.substr(6));
		}
		while(class_key.compare(0, 9, "volatile ") == 0)
			class_key = CanonicalSpelling(class_key.substr(9));
		while(!class_key.empty() && (class_key[class_key.size() - 1] == '&' ||
			class_key[class_key.size() - 1] == '*')) class_key.erase(class_key.size() - 1);
		class_key = CanonicalSpelling(class_key);
		if(class_declarations_.find(class_key) == class_declarations_.end()) {
			map<string, string>::const_iterator specialization = specialization_bases_.find(
				LastComponent(class_key));
			if(specialization != specialization_bases_.end()) class_key = specialization->second;
		}
		const string active_key = class_key + "|" + member;
		if(!active->insert(active_key).second) return false;
		CPPGMAstNodePtr declaration = FindClassDeclaration(class_key, context);
		if(!declaration) {
			active->erase(active_key);
			return false;
		}
		const string declaration_context = PrefixComponent(class_key).empty() ?
			context : PrefixComponent(class_key);
		string fallback_type;
		for(size_t i = 0; i < declaration->children.size(); ++i) {
			const CPPGMAstNodePtr child = declaration->children[i];
			if(!child) continue;
			if(child->kind == "function-definition" && child->children.size() > 1 &&
				LastComponent(FirstIdentifierLocal(child->children[1])) == member) {
				string type = NodeTypeSpelling(child->children[0]) +
					DeclaratorSuffix(child->children[1]);
				const bool function_const = DeclaratorSuffix(child->children[1]).find("const") != string::npos;
				if(function_const != object_const) {
					if(!object_const && fallback_type.empty())
						fallback_type = type;
					continue;
				}
				*result = CanonicalSpelling(ReplaceIdentifiers(type, substitutions));
				active->erase(active_key);
				return !result->empty();
			}
			if(child->kind != "simple-declaration" || child->children.empty()) continue;
			const string base = NodeTypeSpelling(child->children[0]);
			const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
			if(!list) continue;
			for(size_t item = 0; item < list->children.size(); ++item) {
				const CPPGMAstNodePtr init = list->children[item];
				if(!init || init->children.empty()) continue;
				if(LastComponent(FirstIdentifierLocal(init->children[0])) != member) continue;
				*result = CanonicalSpelling(ReplaceIdentifiers(
					base + DeclaratorSuffix(init->children[0]), substitutions));
				active->erase(active_key);
				return !result->empty();
			}
		}
		if(!fallback_type.empty()) {
			*result = CanonicalSpelling(ReplaceIdentifiers(fallback_type, substitutions));
			active->erase(active_key);
			return !result->empty();
		}
		for(size_t i = 0; i < declaration->children.size(); ++i) {
			const CPPGMAstNodePtr child = declaration->children[i];
			if(!child || child->kind != "base-clause") continue;
			for(size_t base_index = 0; base_index < child->children.size(); ++base_index) {
				const CPPGMAstNodePtr base_specifier = child->children[base_index];
				const CPPGMAstNodePtr base_name = ChildOfKindLocal(base_specifier, "base-name");
				if(!base_name) continue;
				string base_spelling = ReplaceIdentifiers(base_name->value, substitutions);
				base_spelling = CanonicalSpelling(base_spelling);
				const size_t open = base_spelling.find('<');
				const TemplateDefinition* base_definition = 0;
				map<string, string> base_substitutions;
				string base_lookup = base_spelling;
				if(open != string::npos) {
					string argument_text;
					size_t close = string::npos;
					if(!TemplateRange(base_spelling, open, &argument_text, &close)) continue;
					base_lookup = base_spelling.substr(0, open);
					base_definition = FindDefinition(base_lookup, declaration_context);
					if(base_definition) {
						const vector<string> arguments = SplitTemplateArguments(argument_text);
						for(size_t parameter = 0; parameter < base_definition->parameters.size() &&
							parameter < arguments.size(); ++parameter)
							base_substitutions[base_definition->parameters[parameter].name] =
								NormalizeTypeArgument(ReplaceIdentifiers(arguments[parameter], substitutions));
					}
				}
				if(base_definition) base_lookup = base_definition->qualified_name;
				if(FindClassMemberType(base_lookup, member, base_substitutions,
					declaration_context, result, active)) {
					active->erase(active_key);
					return true;
				}
			}
		}
		active->erase(active_key);
		return false;
	}
	bool InferArgument(const CPPGMAstNodePtr& expression, string* result,
		const map<string, string>& substitutions, const string& context) const
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
				*result = NormalizeTypeArgument(SpellNode(type_id));
				return !result->empty();
			}
		}
		if(expression->kind == "id-expression") {
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
		if(expression->kind == "call-expression" && !expression->children.empty() &&
			expression->children[0] && expression->children[0]->kind == "member-expression") {
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
		if(expression->kind == "call-expression" && !expression->children.empty() &&
			expression->children[0] && expression->children[0]->kind == "id-expression") {
			const string member = LastComponent(expression->children[0]->value);
			const string owner = PrefixComponent(context);
			set<string> active;
			if(!owner.empty() && !member.empty() && FindClassMemberType(
				owner, member, substitutions, context, result, &active)) return true;
		}
		if(expression->kind == "call-expression" && !expression->children.empty() &&
			expression->children[0] && expression->children[0]->kind == "id-expression") {
			const string callee = LastComponent(expression->children[0]->value);
			const string nested_class = JoinPath(context, callee);
			if(class_contexts_.find(nested_class) != class_contexts_.end())
				*result = nested_class;
			else
				*result = ResolveAlias(expression->children[0]->value, context);
			return true;
		}
		if(expression->kind == "binary-expression" && expression->children.size() >= 2) {
			const string op = RemoveMarker(expression->value);
			if(op == "&&" || op == "||" || op == "==" || op == "!=" ||
				op == "<" || op == ">" || op == "<=" || op == ">=") {
				*result = "bool";
				return true;
			}
			if(op == "-") {
				// A free binary difference used as a function-template argument
				// is commonly the iterator/range reduction shape.  Its result is
				// the ordinary arithmetic value even when both operands are class
				// iterators.
				*result = "int";
				return true;
			}
			if(InferArgument(expression->children[0], result, substitutions, context)) return true;
		}
		if(expression->kind == "unary-expression" && !expression->children.empty()) {
			const string op = RemoveMarker(expression->value);
			if(op == "&" && InferArgument(expression->children[0], result, substitutions, context)) {
				*result = CanonicalSpelling(*result + "*");
				return true;
			}
		}
		return false;
	}
	bool InferFunctionArguments(const TemplateDefinition& definition,
		const CPPGMAstNodePtr& call, vector<string>* result,
		const map<string, string>& substitutions, const string& context,
		const vector<string>* explicit_prefix = 0) const
	{
		if(!call || call->children.size() < 2 || !result) return false;
		const CPPGMAstNodePtr declarator = FunctionDeclarator(definition.declaration);
		const CPPGMAstNodePtr parameters = DescendantOfKind(declarator, "parameter-clause");
		const CPPGMAstNodePtr arguments = call->children[1] &&
			call->children[1]->kind == "argument-list" ? call->children[1] :
			ChildOfKindLocal(call->children[1], "argument-list");
		if(!parameters || !arguments) return false;
		map<string, string> inferred;
		set<string> parameter_names;
		for(size_t i = 0; i < definition.parameters.size(); ++i) {
			parameter_names.insert(definition.parameters[i].name);
			if(explicit_prefix && i < explicit_prefix->size())
				inferred[definition.parameters[i].name] = (*explicit_prefix)[i];
		}
		size_t argument_index = 0;
		for(size_t i = 0; i < parameters->children.size(); ++i) {
			const CPPGMAstNodePtr parameter = parameters->children[i];
			if(!parameter || parameter->kind != "parameter-declaration") continue;
			if(argument_index >= arguments->children.size()) break;
			string type;
			const bool inferred_argument = InferArgument(arguments->children[argument_index], &type, substitutions, context);
			if(inferred_argument) {
				const string pattern = ParameterTypeSpelling(parameter);
				bool dependent = false;
				for(size_t p = 0; p < definition.parameters.size(); ++p) {
					const string& name = definition.parameters[p].name;
					for(size_t at = 0; at + name.size() <= pattern.size(); ++at)
						if(pattern.compare(at, name.size(), name) == 0 &&
							(at == 0 || !IsIdentifierCharacter(pattern[at - 1])) &&
							(at + name.size() == pattern.size() ||
							 !IsIdentifierCharacter(pattern[at + name.size()]))) {
							dependent = true;
							break;
						}
					if(dependent) break;
				}
				if(dependent && !MatchTypePattern(pattern, type, parameter_names, &inferred, context))
					return false;
			}
			++argument_index;
		}
		for(size_t i = 0; i < definition.parameters.size(); ++i) {
			map<string, string>::const_iterator found = inferred.find(definition.parameters[i].name);
			if(found != inferred.end()) result->push_back(found->second);
			else if(!definition.parameters[i].default_type.empty()) result->push_back(definition.parameters[i].default_type);
			else return false;
		}
		return true;
	}
	bool SplitFunctionPointerType(string raw, string* result_type,
		vector<string>* parameters) const
	{
		raw = CanonicalSpelling(raw);
		const size_t separator = raw.find(")(");
		if(separator == string::npos || raw.empty() || raw[raw.size() - 1] != ')') return false;
		const size_t pointer = raw.rfind("(*", separator);
		const size_t reference = raw.rfind("(&", separator);
		const size_t owner = pointer != string::npos ? pointer : reference;
		if(owner == string::npos) return false;
		if(result_type) *result_type = CanonicalSpelling(raw.substr(0, owner));
		if(parameters) *parameters = SplitTemplateArguments(raw.substr(separator + 2,
			raw.size() - separator - 3));
		return result_type == 0 || !result_type->empty();
	}
	bool InferFunctionFromExpected(const TemplateDefinition& definition,
		string expected, vector<string>* result, const string& context) const
	{
		if(!result || definition.class_template) return false;
		string expected_result;
		vector<string> expected_parameters;
		expected = ResolveAlias(expected, context);
		if(!SplitFunctionPointerType(expected, &expected_result, &expected_parameters)) return false;
		const CPPGMAstNodePtr declarator = FunctionDeclarator(definition.declaration);
		if(!declarator || !definition.declaration || definition.declaration->children.empty()) return false;
		const CPPGMAstNodePtr parameter_clause = DescendantOfKind(declarator, "parameter-clause");
		if(!parameter_clause || parameter_clause->children.size() != expected_parameters.size()) return false;
		set<string> parameter_names;
		for(size_t i = 0; i < definition.parameters.size(); ++i)
			parameter_names.insert(definition.parameters[i].name);
		map<string, string> inferred;
		const string result_pattern = NodeTypeSpelling(definition.declaration->children[0]) +
			DeclaratorSuffix(declarator);
		if(!MatchTypePattern(result_pattern, expected_result, parameter_names, &inferred, context)) return false;
		size_t expected_index = 0;
		for(size_t i = 0; i < parameter_clause->children.size(); ++i) {
			const CPPGMAstNodePtr parameter = parameter_clause->children[i];
			if(!parameter || parameter->kind != "parameter-declaration") continue;
			if(!MatchTypePattern(ParameterTypeSpelling(parameter), expected_parameters[expected_index++],
				parameter_names, &inferred, context)) return false;
		}
		for(size_t i = 0; i < definition.parameters.size(); ++i) {
			map<string, string>::const_iterator found = inferred.find(definition.parameters[i].name);
			if(found != inferred.end()) result->push_back(found->second);
			else if(!definition.parameters[i].default_type.empty()) result->push_back(definition.parameters[i].default_type);
			else return false;
		}
		return true;
	}
	void RewriteOperatorFunctionArgument(const CPPGMAstNodePtr& expression,
		const string& context, const map<string, string>& substitutions)
	{
		if(!expression || expression->children.size() < 2 ||
			RemoveMarker(expression->value) != "<<") return;
		CPPGMAstNodePtr argument = expression->children[1];
		if(argument && argument->kind == "unary-expression" &&
			RemoveMarker(argument->value) == "&" && !argument->children.empty())
			argument = argument->children[0];
		if(!argument || argument->kind != "id-expression") return;
		string object_type;
		if(!InferArgument(expression->children[0], &object_type, substitutions, context)) return;
		CPPGMAstNodePtr declaration = FindClassDeclaration(object_type, context);
		if(!declaration) return;
		string expected;
		for(size_t i = 0; i < declaration->children.size(); ++i) {
			const CPPGMAstNodePtr child = declaration->children[i];
			if(!child || child->kind != "function-definition" || child->children.size() < 2 ||
				LastComponent(FirstIdentifierLocal(child->children[1])) != "operator<<") continue;
			const CPPGMAstNodePtr clause = DescendantOfKind(child->children[1], "parameter-clause");
			if(!clause || clause->children.empty()) continue;
			const CPPGMAstNodePtr parameter = clause->children[0];
			expected = FunctionTypeSpelling(parameter);
			break;
		}
		if(expected.empty()) return;
		expected = RewriteText(expected, context, substitutions, 0);
		const vector<const TemplateDefinition*> candidates =
			FindFunctionDefinitions(argument->value, context);
		for(size_t i = 0; i < candidates.size(); ++i) {
			vector<string> inferred;
			if(!InferFunctionFromExpected(*candidates[i], expected, &inferred, context)) continue;
			argument->value = Instantiate(*candidates[i], inferred, context);
			return;
		}
	}
	bool IsBuiltinLogicalType(string raw) const
	{
		raw = CanonicalSpelling(raw);
		while(raw.compare(0, 6, "const ") == 0)
			raw = CanonicalSpelling(raw.substr(6));
		while(raw.compare(0, 9, "volatile ") == 0)
			raw = CanonicalSpelling(raw.substr(9));
		if(raw.find('*') != string::npos) return true;
		return raw == "bool" || raw == "char" || raw == "signed char" ||
			raw == "unsigned char" || raw == "short" || raw == "short int" ||
			raw == "unsigned short" || raw == "unsigned short int" ||
			raw == "int" || raw == "unsigned" || raw == "unsigned int" ||
			raw == "long" || raw == "long int" || raw == "unsigned long" ||
			raw == "unsigned long int" || raw == "long long" ||
			raw == "long long int" || raw == "unsigned long long" ||
			raw == "unsigned long long int" || raw == "float" ||
			raw == "double" || raw == "long double" || raw == "nullptr_t";
	}
	void InstantiateOperatorTemplate(const CPPGMAstNodePtr& expression,
		const string& context, const map<string, string>& substitutions)
	{
		if(!expression || expression->children.size() < 2) return;
		const string operation = RemoveMarker(expression->value);
		if(operation.empty() || operation == ",") return;
		if((operation == "&&" || operation == "||") && expression->children.size() >= 2) {
			string left, right;
			if(InferArgument(expression->children[0], &left, substitutions, context) &&
				InferArgument(expression->children[1], &right, substitutions, context) &&
				IsBuiltinLogicalType(left) && IsBuiltinLogicalType(right)) return;
		}
		const vector<const TemplateDefinition*> candidates = FindFunctionDefinitions(
			"operator" + operation, context);
		if(candidates.empty()) return;
		CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
		call->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
			"id-expression", "operator" + operation)));
		CPPGMAstNodePtr arguments(new CPPGMAstNode("argument-list"));
		arguments->children.push_back(expression->children[0]);
		arguments->children.push_back(expression->children[1]);
		call->children.push_back(arguments);
		for(size_t i = 0; i < candidates.size(); ++i) {
			vector<string> inferred;
			if(!InferFunctionArguments(*candidates[i], call, &inferred,
				substitutions, context)) continue;
			Instantiate(*candidates[i], inferred, context);
			return;
		}
	}
	string RewriteDecltypeText(string raw, const string& context,
		const map<string, string>& substitutions, bool* template_replaced)
	{
		for(size_t search = 0; search + 9 <= raw.size(); ++search) {
			const size_t decltype_start = raw.find("decltype(", search);
			if(decltype_start == string::npos) break;
			search = decltype_start;
			int depth = 0;
			size_t close = string::npos;
			for(size_t i = search + 8; i < raw.size(); ++i) {
				if(raw[i] == '(') ++depth;
				else if(raw[i] == ')' && --depth == 0) {
					close = i;
					break;
				}
			}
			if(close == string::npos) continue;
			const string expression = CanonicalSpelling(raw.substr(search + 9,
				close - search - 9));
			string type;
			if(!EvaluateDecltypeExpression(expression, context, substitutions, &type) ||
				type.empty()) {
				search = close;
				continue;
			}
			raw.replace(search, close - search + 1, type);
			if(template_replaced) *template_replaced = true;
			search += type.size();
		}
		return raw;
	}
	string RewriteText(string raw, const string& context, const map<string, string>& substitutions,
		bool* template_replaced, bool resolve_alias = true, bool resolve_member = true)
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
			if(!TemplateRange(raw, search, &arguments_text, &close)) continue;
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
			vector<string> args = SplitTemplateArguments(arguments_text);
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
				if(resolve_member && definition->class_template && close + 2 < raw.size() &&
				raw.compare(close + 1, 2, "::") == 0) {
				size_t nested_begin = close + 3;
				while(nested_begin < raw.size() && IsIdentifierCharacter(raw[nested_begin])) ++nested_begin;
				const string nested = raw.substr(close + 3, nested_begin - (close + 3));
				if(!nested.empty()) {
					const string member_type = TemplateMemberType(*definition, args, nested, context);
					if(!member_type.empty()) {
						raw.replace(begin, nested_begin - begin, member_type);
						if(template_replaced) *template_replaced = true;
						search = begin + member_type.size();
						continue;
					}
					requested_nested_classes_[definition->qualified_name].insert(nested);
					requested_nested_classes_[LastComponent(definition->qualified_name)].insert(nested);
				}
			}
			const string local_name = definition->class_template || !definition->class_template ?
				Instantiate(*definition, args, context) : string();
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
		return !resolve_alias || raw.find("::") == string::npos ? raw : ResolveAlias(raw, context);
	}
	CPPGMAstNodePtr TransformCallExpression(const CPPGMAstNodePtr& input,
		const string& context, const map<string, string>& substitutions)
	{
		CPPGMAstNodePtr result(new CPPGMAstNode(input->kind, input->value)); result->initializer_form = input->initializer_form;
		result->template_instantiation = input->template_instantiation; result->template_primary = input->template_primary; result->template_arguments = input->template_arguments;
		CPPGMAstNodePtr input_callee = input->children.empty() ? CPPGMAstNodePtr() : input->children[0];
		if(input_callee && input_callee->kind == "parenthesized-expression" &&
			input_callee->children.size() == 1 && input_callee->children[0] &&
			input_callee->children[0]->kind == "id-expression")
			input_callee = input_callee->children[0];
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
					vector<string> explicit_args = SplitTemplateArguments(argument_text);
					for(size_t i = 0; i < explicit_args.size(); ++i) {
						explicit_args[i] = NormalizeTypeArgument(RewriteText(
							explicit_args[i], context, substitutions, 0));
						explicit_args[i] = NormalizeTypeArgument(ReplaceIdentifiers(
							explicit_args[i], substitutions));
						explicit_args[i] = ResolveAlias(explicit_args[i], context);
						explicit_args[i] = NormalizeTypeArgument(RewriteText(
							explicit_args[i], context, substitutions, 0));
						explicit_args[i] = ResolveAlias(explicit_args[i], context);
						explicit_args[i] = QualifyTypeArgument(explicit_args[i], context,
							explicit_definition->owner);
					}
					vector<string> complete_args;
					bool complete = explicit_args.size() == explicit_definition->parameters.size();
					if(complete) complete_args = explicit_args;
					else if(explicit_args.size() < explicit_definition->parameters.size())
						complete = InferFunctionArguments(*explicit_definition, input,
							&complete_args, substitutions, context, &explicit_args);
					if(complete) {
						const string local_name = Instantiate(*explicit_definition, complete_args, context);
							const string qualifier = PrefixComponent(base);
						CPPGMAstNodePtr callee(new CPPGMAstNode("id-expression",
							qualifier.empty() ? local_name : qualifier + "::" + local_name));
						result->children.push_back(callee);
						for(size_t i = 1; i < input->children.size(); ++i) {
							CPPGMAstNodePtr child = TransformNode(input->children[i], context, substitutions);
							if(child) result->children.push_back(child);
						}
						return result;
					}
				}
			}
		}
		for(size_t i = 0; i < input->children.size(); ++i) {
			CPPGMAstNodePtr child = TransformNode(input->children[i], context, substitutions); if(child) result->children.push_back(child);
		}
		CPPGMAstNodePtr result_callee = result->children.empty() ? CPPGMAstNodePtr() : result->children[0];
		if(result_callee && result_callee->kind == "parenthesized-expression" &&
			result_callee->children.size() == 1 && result_callee->children[0] &&
			result_callee->children[0]->kind == "id-expression") {
			result_callee = result_callee->children[0];
			result->children[0] = result_callee;
		}
		if(result_callee && result_callee->kind == "id-expression" &&
			result_callee->value.find('<') == string::npos) {
				const string callee_name = result_callee->value;
				const vector<const TemplateDefinition*> definitions =
					FindFunctionDefinitions(callee_name, context);
			if(!HasExactOrdinaryMatch(result, callee_name, substitutions, context))
				for(size_t candidate = 0; candidate < definitions.size(); ++candidate) {
					const TemplateDefinition* definition = definitions[candidate];
					vector<string> inferred;
					const bool inferred_ok = InferFunctionArguments(*definition, result, &inferred,
						substitutions, context);
					if(inferred_ok) {
						const string local_name = Instantiate(*definition, inferred, context);
							const string qualifier = GeneratedFunctionQualifier(*definition,
								callee_name, context);
						result_callee->value = qualifier.empty() ? local_name : qualifier + "::" + local_name;
						break;
					}
				}
			if(definitions.empty()) {
				const FunctionSignature* signature = FindFunctionSignature(callee_name, context);
					if(signature && callee_name.find("::") == string::npos &&
						class_contexts_.find(context) == class_contexts_.end() && substitutions.empty()) {
					for(map<string, FunctionSignature>::const_iterator found = function_signatures_.begin();
						found != function_signatures_.end(); ++found)
						if(&found->second == signature &&
						class_contexts_.find(PrefixComponent(found->first)) == class_contexts_.end() &&
							function_contexts_.find(PrefixComponent(found->first)) == function_contexts_.end()) {
							result->children[0]->value = found->first;
							break;
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
		return result;
	}
	void ResolveFunctionArguments(const CPPGMAstNodePtr& result,
		const FunctionSignature* signature, const string& context)
	{
		if(!signature || !signature->parameters || result->children.size() < 2 ||
			!result->children[1] || result->children[1]->kind != "argument-list") return;
		const CPPGMAstNodePtr result_arguments = result->children[1];
		size_t argument = 0;
		for(size_t parameter = 0; parameter < signature->parameters->children.size() &&
			argument < result_arguments->children.size(); ++parameter) {
			const CPPGMAstNodePtr parameter_node = signature->parameters->children[parameter];
			if(!parameter_node || parameter_node->kind != "parameter-declaration") continue;
			const string expected = FunctionTypeSpelling(parameter_node);
			CPPGMAstNodePtr argument_node = result_arguments->children[argument];
			if(argument_node && argument_node->kind == "unary-expression" &&
				RemoveMarker(argument_node->value) == "&" && !argument_node->children.empty())
				argument_node = argument_node->children[0];
			if(argument_node && argument_node->kind == "id-expression") {
				const vector<const TemplateDefinition*> function_candidates =
					FindFunctionDefinitions(argument_node->value, context);
				for(size_t candidate = 0; candidate < function_candidates.size(); ++candidate) {
					vector<string> inferred;
					if(InferFunctionFromExpected(*function_candidates[candidate],
						expected, &inferred, context)) {
						const string local_name = Instantiate(*function_candidates[candidate], inferred, context);
						result_arguments->children[argument]->value = local_name;
						break;
					}
				}
			}
			++argument;
		}
	}
	bool SkipUnusedNestedClass(const CPPGMAstNodePtr& input,
		const CPPGMAstNodePtr& original_child, const string& child_context,
		const map<string, string>& substitutions, size_t index) const
	{
		if(!input || !original_child ||
			(input->kind != "class-specifier" && input->kind != "class-forward-declaration") ||
			(PrefixComponent(input->value).find('<') == string::npos && substitutions.empty()) ||
			(original_child->kind != "class-specifier" &&
				original_child->kind != "class-forward-declaration")) return false;
		const string nested_name = LastComponent(original_child->value);
		const string class_key = JoinPath(child_context, LastComponent(input->value));
		map<string, set<string> >::const_iterator requested = requested_nested_classes_.find(class_key);
		if(requested == requested_nested_classes_.end())
			requested = requested_nested_classes_.find(LastComponent(input->value));
		if(requested != requested_nested_classes_.end() &&
			requested->second.find(nested_name) != requested->second.end()) return false;
		for(size_t sibling = 0; sibling < input->children.size(); ++sibling)
			if(sibling != index && input->children[sibling] &&
				input->children[sibling]->kind != "class-key" &&
				ContainsName(input->children[sibling], nested_name)) return false;
		bool has_sibling = false;
		for(size_t sibling = 0; sibling < input->children.size(); ++sibling)
			if(sibling != index && input->children[sibling] &&
				input->children[sibling]->kind != "class-key") has_sibling = true;
		return has_sibling;
	}
	void TransformRegularChildren(const CPPGMAstNodePtr& input,
		const string& child_context, const string& function_context,
		const map<string, string>& substitutions,
		map<string, string>* local_substitutions,
		const CPPGMAstNodePtr& result)
	{
		for(size_t i = 0; i < input->children.size(); ++i) {
			const CPPGMAstNodePtr original_child = input->children[i];
			// Once a dependent decltype has been reduced to a concrete typed
			// spelling, its preserved expression subtree is only syntax history.
			// Re-transforming that subtree would instantiate helper calls such as
			// `declval<F>()` as ordinary declarations and lose function-pointer
			// declarator structure.
			if(input->kind == "decl-specifier" &&
				input->value.find("decltype(") != string::npos &&
				original_child && (original_child->kind == "call-expression" ||
					original_child->kind == "binary-expression")) continue;
				if(SkipUnusedNestedClass(input, original_child, child_context, substitutions, i)) continue;
			if(original_child && original_child->kind == "namespace-alias-definition") {
				const CPPGMAstNodePtr target = ChildOfKindLocal(original_child, "target");
				if(target && !target->value.empty() && local_substitutions)
					(*local_substitutions)[original_child->value] = RewriteText(
						target->value, child_context, *local_substitutions, 0, false);
				continue;
			}
			const string node_context = input->kind == "function-definition" &&
				original_child && original_child->kind == "compound-statement" ?
				function_context : child_context;
			CPPGMAstNodePtr child;
			if(input->kind == "using-declaration" && original_child &&
				original_child->kind == "target") {
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
				if(!child && input->kind == "decl-specifier-seq" && original_child &&
					(original_child->kind == "class-specifier" ||
						original_child->kind == "class-forward-declaration")) {
					map<string, string>::const_iterator promoted = local_class_names_.find(
						JoinPath(child_context, LastComponent(original_child->value)));
					if(promoted != local_class_names_.end())
						result->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
							"decl-specifier", "TT_IDENTIFIER:" + LastComponent(promoted->second))));
				}
				if(child && !(input->kind == "compound-statement" && !substitutions.empty() &&
				(original_child->kind == "alias-declaration" ||
					(original_child->kind == "simple-declaration" &&
					 SpellNode(original_child->children.empty() ? CPPGMAstNodePtr() :
						original_child->children[0]).find("typedef") != string::npos))))
				result->children.push_back(child);
			if(original_child && original_child->kind == "alias-declaration" &&
				!original_child->value.empty() && !original_child->children.empty() &&
				(!substitutions.empty() || input->value.find('<') != string::npos))
				(*local_substitutions)[original_child->value] = RewriteText(
					TypeIdSpelling(original_child->children[0]), child_context,
					*local_substitutions, 0);
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
						const CPPGMAstNodePtr owner_declaration = FindClassDeclaration(
							rewritten_owner.empty() ? owner : rewritten_owner, child_context);
						if(owner_declaration) for(size_t member = 0;
							member < owner_declaration->children.size(); ++member) {
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
			if(original_child && original_child->kind == "using-directive")
				RecordUsingDirective(original_child, local_substitutions);
			if(original_child && original_child->kind == "simple-declaration" &&
				!original_child->children.empty() &&
				SpellNode(original_child->children[0]).find("typedef") != string::npos &&
				(!substitutions.empty() || input->value.find('<') != string::npos))
				RecordTypedefSubstitutions(original_child, child_context, local_substitutions);
		}
	}
	void RecordUsingDirective(const CPPGMAstNodePtr& original_child,
		map<string, string>* local_substitutions)
	{
		const CPPGMAstNodePtr target = ChildOfKindLocal(original_child, "target");
		if(!target || target->value.empty()) return;
		const string prefix = target->value + "::";
		for(set<string>::const_iterator type = class_contexts_.begin();
			type != class_contexts_.end(); ++type) {
			if(type->compare(0, prefix.size(), prefix) != 0) continue;
			const string relative = type->substr(prefix.size());
			const size_t separator = relative.find("::");
			const string visible = relative.substr(0, separator);
			if(!visible.empty() && local_substitutions->find(visible) == local_substitutions->end())
				(*local_substitutions)[visible] = target->value + "::" + visible;
		}
		for(map<string, TemplateDefinition>::const_iterator definition = definitions_.begin();
			definition != definitions_.end(); ++definition) {
			if(definition->second.qualified_name.compare(0, prefix.size(), prefix) != 0) continue;
			const string relative = definition->second.qualified_name.substr(prefix.size());
			const size_t separator = relative.find("::");
			const string visible = relative.substr(0, separator);
			if(!visible.empty() && local_substitutions->find(visible) == local_substitutions->end())
				(*local_substitutions)[visible] = target->value + "::" + visible;
		}
	}
	void RecordTypedefSubstitutions(const CPPGMAstNodePtr& original_child,
		const string& child_context, map<string, string>* local_substitutions)
	{
		const CPPGMAstNodePtr list = ChildOfKindLocal(original_child, "init-declarator-list");
		if(!list) return;
		for(size_t item_index = 0; item_index < list->children.size(); ++item_index) {
			const CPPGMAstNodePtr item = list->children[item_index];
			if(!item || item->children.empty()) continue;
			const string alias_name = FirstIdentifierLocal(item->children[0]);
			if(!alias_name.empty()) (*local_substitutions)[alias_name] = RewriteText(
				NodeTypeSpelling(original_child->children[0]) +
				DeclaratorSuffix(item->children[0]), child_context, *local_substitutions, 0);
		}
	}
	void RewriteTemplateInitializer(const CPPGMAstNodePtr& input,
		const string& context, const map<string, string>& substitutions,
		const CPPGMAstNodePtr& result)
	{
		if(!input || !result || input->kind != "simple-declaration") return;
		const CPPGMAstNodePtr original_list = ChildOfKindLocal(input, "init-declarator-list");
		const CPPGMAstNodePtr transformed_list = ChildOfKindLocal(result, "init-declarator-list");
		if(!original_list || !transformed_list || original_list->children.size() != 1 ||
			transformed_list->children.size() != 1) return;
		const CPPGMAstNodePtr original_item = original_list->children[0];
		const CPPGMAstNodePtr transformed_item = transformed_list->children[0];
		if(!original_item || !transformed_item || original_item->children.empty() ||
			transformed_item->children.empty()) return;
		const CPPGMAstNodePtr original_declarator = original_item->children[0];
		const CPPGMAstNodePtr transformed_declarator = transformed_item->children[0];
		const CPPGMAstNodePtr parameter_clause = ChildOfKindLocal(
			original_declarator, "parameter-clause");
		if(!parameter_clause || parameter_clause->children.size() != 1 ||
			!parameter_clause->children[0]) return;
		const CPPGMAstNodePtr parameter = parameter_clause->children[0];
		const CPPGMAstNodePtr parameter_specs = parameter->children.empty() ?
			CPPGMAstNodePtr() : parameter->children[0];
		if(!parameter_specs || parameter_specs->children.empty()) return;
		const string raw_type = RemoveMarker(parameter_specs->children[0]->value);
		const size_t open = raw_type.find('<');
		if(open == string::npos) return;
		string arguments;
		size_t close = string::npos;
		string base;
		size_t begin = 0;
		if(!TemplateBase(raw_type, open, &begin, &base) ||
			!TemplateRange(raw_type, open, &arguments, &close) ||
			!FindDefinition(base, context)) return;
		const string callee = RewriteText(raw_type, context, substitutions, 0);
		if(callee.empty() || callee == raw_type) return;
		string argument_name = FirstIdentifierLocal(parameter->children.size() > 1 ?
			parameter->children[1] : CPPGMAstNodePtr());
		if(argument_name.empty()) return;
		CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
		call->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
			"id-expression", callee)));
		CPPGMAstNodePtr call_arguments(new CPPGMAstNode("argument-list"));
		call_arguments->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
			"id-expression", argument_name)));
		call->children.push_back(call_arguments);
		for(size_t i = 0; i < transformed_declarator->children.size();) {
			if(transformed_declarator->children[i] &&
				transformed_declarator->children[i]->kind == "parameter-clause")
				transformed_declarator->children.erase(
					transformed_declarator->children.begin() + i);
			else ++i;
		}
		transformed_item->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("initializer")));
		transformed_item->children.back()->children.push_back(call);
	}
	CPPGMAstNodePtr TransformRegularNode(const CPPGMAstNodePtr& input,
		const string& context, const map<string, string>& substitutions)
	{
		CPPGMAstNodePtr template_call = RewriteTemplateCastCall(input, context, substitutions);
		if(template_call) return template_call;
		CPPGMAstNodePtr result(new CPPGMAstNode(input->kind, input->value));
		result->initializer_form = input->initializer_form;
		bool template_replaced = false;
		const bool type_spelling = input->kind == "decl-specifier" ||
			input->kind == "type-name" || input->kind == "type-specifier";
		result->value = RewriteText(input->value, context, substitutions,
			&template_replaced, !type_spelling, true);
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
		RewriteTemplateInitializer(input, context, substitutions, result);
		if(input->kind == "simple-declaration") ReifyReferenceType(result);
		if(input->kind == "binary-expression") {
			InstantiateOperatorTemplate(result, context, substitutions);
			RewriteOperatorFunctionArgument(result, context, substitutions);
		}
		if(!promoted_local_class.empty()) {
			const map<string, string>::const_iterator owner = function_owners_.find(context);
			generated_by_owner_[owner == function_owners_.end() ? PrefixComponent(context) :
				owner->second].push_back(result);
			return CPPGMAstNodePtr();
		}
		return result;
	}
	CPPGMAstNodePtr TransformNode(const CPPGMAstNodePtr& input, const string& context,
		const map<string, string>& substitutions)
	{
		if(!input) return CPPGMAstNodePtr();
		if(input->kind == "template-declaration") {
			if(input->children.size() > 1 && input->children[1] &&
				(input->children[1]->kind == "class-specifier" ||
					input->children[1]->kind == "class-forward-declaration"))
				return PrefixComponent(input->children[1]->value).find('<') == string::npos ?
					MakeClassShell(LastComponent(input->children[1]->value)) :
					CPPGMAstNodePtr();
			return CPPGMAstNodePtr();
		}
		if(input->kind == "using-declaration") {
			const CPPGMAstNodePtr target = ChildOfKindLocal(input, "target");
			if(target && IsOrdinaryTemplateUsingTarget(target->value, context))
				return CPPGMAstNodePtr();
		}
		if(input->kind == "parameter-declaration" && !input->children.empty() &&
			input->children[0] && input->children[0]->kind == "decl-specifier-seq") {
			for(size_t i = 0; i < input->children[0]->children.size(); ++i) {
				const CPPGMAstNodePtr specifier = input->children[0]->children[i];
				if(!specifier) continue;
				const string name = RemoveMarker(specifier->value);
				map<string, string>::const_iterator substitution = substitutions.find(name);
				if(substitution == substitutions.end()) continue;
				map<string, string>::const_iterator marker = function_marker_names_.find(substitution->second);
				if(marker == function_marker_names_.end()) continue;
				map<string, FunctionSignature>::const_iterator signature = function_signatures_.find(marker->second);
				if(signature != function_signatures_.end())
					return FunctionParameter(input, signature->second, substitution->second);
				const FunctionSignature* recovered = FindFunctionSignature(marker->second, context);
				if(recovered) return FunctionParameter(input, *recovered, substitution->second);
			}
		}
		if(input->kind == "namespace-definition") return TransformNamespace(input, context, substitutions);
		if(input->kind == "call-expression") return TransformCallExpression(input, context, substitutions);
		return TransformRegularNode(input, context, substitutions);
	}
};
} // namespace
