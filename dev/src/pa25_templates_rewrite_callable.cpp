#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

#include <functional>
#include <string>
#include <vector>

using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::ResolveCallableTemporaryCallResult(
	const string& callee, const string& function_context, const string& context,
	const map<string, string>& substitutions, const vector<string>& actual_types,
	string* result, const string* known_object_type)
{
	if(!result) return false;
	const auto infer_auto_operator_result = [this, &substitutions, &context](
		const TemplateDefinition* candidate, string* inferred) {
		if(!candidate || !inferred || !candidate->declaration ||
			candidate->declaration->children.empty()) return false;
		const string declared = CanonicalSpelling(NodeTypeSpelling(
			candidate->declaration->children[0]));
		if(declared != "auto" && declared != "const auto") return false;
		const CPPGMAstNodePtr body = ChildOfKindLocal(candidate->declaration,
			"compound-statement");
		if(!body) return false;
		const string return_context = candidate->owner.empty() ? context : candidate->owner;
		function<bool(const CPPGMAstNodePtr&)> find_return;
		find_return = [&](const CPPGMAstNodePtr& node) {
			if(!node) return false;
			if(node->kind == "lambda-expression") return false;
			if(node->kind == "return-statement" && !node->children.empty() &&
				node->children[0]) {
				string value;
				if(InferArgument(node->children[0], &value, substitutions,
					return_context) && !value.empty()) {
					*inferred = NormalizeTypeArgument(value);
					return true;
				}
			}
			for(size_t child = 0; child < node->children.size(); ++child)
				if(find_return(node->children[child])) return true;
			return false;
		};
		return find_return(body);
	};
	string object_type = known_object_type ? *known_object_type : string();
	if(!known_object_type && !FunctionCallResultType(callee, function_context,
		substitutions, &object_type)) return false;
	string normalized_object = NormalizeTypeArgument(ResolveAlias(
		ReplaceIdentifiers(object_type, substitutions), context));
	while(!normalized_object.empty() &&
		(normalized_object[normalized_object.size() - 1] == '&' ||
		 normalized_object[normalized_object.size() - 1] == '*'))
		normalized_object.erase(normalized_object.size() - 1);
	normalized_object = CanonicalSpelling(normalized_object);
	const CPPGMAstNodePtr declaration = FindClassDeclaration(normalized_object, context);
	if(!declaration) return false;
	const vector<const TemplateDefinition*> call_operators =
		FindFunctionDefinitions("operator()", normalized_object);
	for(size_t candidate = 0; candidate < call_operators.size(); ++candidate) {
		vector<string> arguments;
		if(!InferFunctionTypeArguments(*call_operators[candidate], actual_types,
			&arguments, substitutions, function_context)) continue;
		const string callable_result = FunctionResultType(*call_operators[candidate],
			arguments, function_context, &substitutions);
		string resolved_result = callable_result;
		if((resolved_result.empty() || resolved_result == "auto" ||
			resolved_result == "const auto") && infer_auto_operator_result(
			call_operators[candidate], &resolved_result)) {}
		if(!resolved_result.empty() && resolved_result != "auto" &&
			resolved_result != "const auto") {
			*result = resolved_result;
			return true;
		}
	}
	for(size_t member = 0; member < declaration->children.size(); ++member) {
		CPPGMAstNodePtr candidate = declaration->children[member];
		if(!candidate || (candidate->kind != "simple-declaration" &&
			candidate->kind != "function-definition" &&
			candidate->kind != "template-declaration" &&
			candidate->kind != "special-member-declaration" &&
			candidate->kind != "special-member-definition")) continue;
		if(candidate->kind == "template-declaration" && candidate->children.size() > 1)
			candidate = candidate->children[1];
		if(!candidate || (candidate->kind != "simple-declaration" &&
			candidate->kind != "function-definition" &&
			candidate->kind != "special-member-declaration" &&
			candidate->kind != "special-member-definition")) continue;
		const string name = candidate->kind == "simple-declaration" ?
			DeclarationName(candidate) : (!candidate->value.empty() ?
				RemoveMarker(candidate->value) : LastComponent(FirstIdentifierLocal(
					FunctionDeclarator(candidate))));
		if(name.compare(0, 8, "operator") != 0) continue;
		if(name == "operator()" && candidate->kind == "function-definition" &&
			!candidate->children.empty()) {
			const string declared = CanonicalSpelling(NodeTypeSpelling(candidate->children[0]));
			if(declared == "auto" || declared == "const auto") {
				const CPPGMAstNodePtr body = ChildOfKindLocal(candidate,
					"compound-statement");
				function<bool(const CPPGMAstNodePtr&)> find_return;
				find_return = [&](const CPPGMAstNodePtr& node) {
					if(!node) return false;
					if(node->kind == "lambda-expression") return false;
					if(node->kind == "return-statement" && !node->children.empty() &&
						node->children[0]) {
						string inferred;
						if(InferArgument(node->children[0], &inferred, substitutions,
							context) && !inferred.empty()) {
							*result = NormalizeTypeArgument(inferred);
							return true;
						}
					}
					for(size_t child = 0; child < node->children.size(); ++child)
						if(find_return(node->children[child])) return true;
					return false;
				};
				if(body && find_return(body)) return true;
			}
		}
		if(candidate->kind == "simple-declaration") {
			const CPPGMAstNodePtr declarator = FunctionDeclarator(candidate);
			const CPPGMAstNodePtr parameters = DescendantOfKind(declarator,
				"parameter-clause");
			if(!declarator || !parameters) continue;
			vector<string> parameter_types;
			for(size_t parameter = 0; parameter < parameters->children.size(); ++parameter) {
				const CPPGMAstNodePtr item = parameters->children[parameter];
				if(item && item->kind == "parameter-declaration")
					parameter_types.push_back(ParameterTypeSpelling(item));
			}
			if(parameter_types.size() != actual_types.size()) continue;
			bool viable = true;
			for(size_t parameter = 0; parameter < parameter_types.size(); ++parameter)
				if(!FunctionArgumentViable(parameter_types[parameter], actual_types[parameter],
					context)) { viable = false; break; }
			if(!viable) continue;
			*result = NormalizeTypeArgument(ResolveAlias(
				NodeTypeSpelling(candidate->children.empty() ? CPPGMAstNodePtr() :
					candidate->children[0]) + ReturnDeclaratorSuffix(declarator),
				context));
				return !result->empty();
			}
			if(name == "operator()") {
				const CPPGMAstNodePtr declarator = FunctionDeclarator(candidate);
				const CPPGMAstNodePtr parameters = DescendantOfKind(declarator,
					"parameter-clause");
				if(!declarator || !parameters) continue;
				vector<string> parameter_types;
				for(size_t parameter = 0; parameter < parameters->children.size(); ++parameter) {
					const CPPGMAstNodePtr item = parameters->children[parameter];
					if(item && item->kind == "parameter-declaration")
						parameter_types.push_back(ParameterTypeSpelling(item));
				}
				if(parameter_types.size() != actual_types.size()) continue;
				bool viable = true;
				for(size_t parameter = 0; parameter < parameter_types.size(); ++parameter)
					if(!FunctionArgumentViable(parameter_types[parameter],
						actual_types[parameter], context)) { viable = false; break; }
				if(!viable) continue;
				string callable_result = NormalizeTypeArgument(ResolveAlias(
					NodeTypeSpelling(candidate->children.empty() ? CPPGMAstNodePtr() :
						candidate->children[0]) + ReturnDeclaratorSuffix(declarator),
					context));
				if(callable_result == "auto" || callable_result == "const auto") {
					const CPPGMAstNodePtr body = ChildOfKindLocal(candidate,
						"compound-statement");
					function<bool(const CPPGMAstNodePtr&)> find_return;
					find_return = [&](const CPPGMAstNodePtr& node) {
						if(!node) return false;
						if(node->kind == "lambda-expression") return false;
						if(node->kind == "return-statement" && !node->children.empty() &&
							node->children[0]) {
							string inferred;
							if(InferArgument(node->children[0], &inferred, substitutions,
								context) && !inferred.empty()) {
								callable_result = NormalizeTypeArgument(inferred);
								return true;
							}
						}
						for(size_t child = 0; child < node->children.size(); ++child)
							if(find_return(node->children[child])) return true;
						return false;
					};
					if(body) find_return(body);
				}
				*result = callable_result;
				return !result->empty();
			}
			string target = CanonicalSpelling(name.substr(8));
		if(target.empty() || target[0] == '(' || target[0] == '[') continue;
		target = CanonicalSpelling(ResolveAlias(
			ReplaceIdentifiers(target, substitutions), normalized_object));
		string return_type, qualifiers;
		vector<string> parameters;
		bool function_type = SplitFunctionPointerType(target, &return_type, &parameters);
		if(!function_type) function_type = SplitDirectFunctionType(target, &return_type,
			&parameters, &qualifiers);
		if(!function_type || parameters.size() != actual_types.size()) continue;
		bool viable = true;
		for(size_t argument = 0; argument < parameters.size(); ++argument)
			if(!FunctionArgumentViable(parameters[argument], actual_types[argument],
				context)) { viable = false; break; }
		if(!viable) continue;
		*result = NormalizeTypeArgument(ResolveAlias(
			ReplaceIdentifiers(return_type, substitutions), normalized_object));
		return !result->empty();
	}
	return false;
}

bool PA18TemplateExpander::ResolveCallableVariableCallResult(
	const string& callee, const string& function_context, const string& context,
	const map<string, string>& substitutions, const vector<string>& actual_types,
	string* result)
{
	if(!result) return false;
	const auto infer_auto_operator_result = [this, &substitutions, &context](
		const TemplateDefinition* candidate, string* inferred) {
		if(!candidate || !inferred || !candidate->declaration ||
			candidate->declaration->children.empty()) return false;
		const string declared = CanonicalSpelling(NodeTypeSpelling(
			candidate->declaration->children[0]));
		if(declared != "auto" && declared != "const auto") return false;
		const CPPGMAstNodePtr body = ChildOfKindLocal(candidate->declaration,
			"compound-statement");
		if(!body) return false;
		const string return_context = candidate->owner.empty() ? context : candidate->owner;
		function<bool(const CPPGMAstNodePtr&)> find_return;
		find_return = [&](const CPPGMAstNodePtr& node) {
			if(!node) return false;
			if(node->kind == "lambda-expression") return false;
			if(node->kind == "return-statement" && !node->children.empty() &&
				node->children[0]) {
				string value;
				if(InferArgument(node->children[0], &value, substitutions,
					return_context) && !value.empty()) {
					*inferred = NormalizeTypeArgument(value);
					return true;
				}
			}
			for(size_t child = 0; child < node->children.size(); ++child)
				if(find_return(node->children[child])) return true;
			return false;
		};
		return find_return(body);
	};
	string variable_type;
	if(!LookupVariableType(callee, context, &variable_type)) return false;
	string callable_type = NormalizeTypeArgument(ResolveAlias(
		ReplaceIdentifiers(variable_type, substitutions), context));
	while(!callable_type.empty() && (callable_type[callable_type.size() - 1] == '&' ||
		callable_type[callable_type.size() - 1] == '*') &&
		callable_type.find("(*") != string::npos) callable_type.erase(callable_type.size() - 1);
	string callable_result;
	vector<string> callable_parameters;
	bool function_pointer = SplitFunctionPointerType(callable_type,
		&callable_result, &callable_parameters);
	string callable_qualifiers;
	if(!function_pointer) function_pointer = SplitDirectFunctionType(callable_type,
		&callable_result, &callable_parameters, &callable_qualifiers);
	if(function_pointer && callable_parameters.size() == actual_types.size()) {
		bool viable = true;
		for(size_t argument = 0; argument < actual_types.size(); ++argument)
			if(!FunctionArgumentViable(RewriteText(callable_parameters[argument],
				function_context, substitutions, 0), actual_types[argument],
				function_context)) { viable = false; break; }
		if(viable) {
			*result = NormalizeTypeArgument(ResolveAlias(
				ReplaceIdentifiers(callable_result, substitutions), function_context));
			return !result->empty();
		}
	}
	const vector<const TemplateDefinition*> call_operators =
		FindFunctionDefinitions("operator()", callable_type);
	for(size_t candidate = 0; candidate < call_operators.size(); ++candidate) {
		vector<string> arguments;
		if(!InferFunctionTypeArguments(*call_operators[candidate], actual_types,
			&arguments, substitutions, function_context)) continue;
		string callable_result = FunctionResultType(*call_operators[candidate],
			arguments, function_context, &substitutions);
		if((callable_result.empty() || callable_result == "auto" ||
			callable_result == "const auto") && infer_auto_operator_result(
			call_operators[candidate], &callable_result)) {}
		if(!callable_result.empty() && callable_result != "auto" &&
			callable_result != "const auto") {
			*result = callable_result;
			return true;
		}
	}
	const CPPGMAstNodePtr declaration = FindClassDeclaration(callable_type, context);
	if(declaration) for(size_t member = 0; member < declaration->children.size(); ++member) {
		const CPPGMAstNodePtr candidate = declaration->children[member];
		if(!candidate || candidate->kind != "function-definition" ||
			candidate->children.size() < 2 ||
			LastComponent(FirstIdentifierLocal(candidate->children[1])) != "operator()") continue;
		string callable_result = NormalizeTypeArgument(RewriteText(
			NodeTypeSpelling(candidate->children[0]), context, substitutions, 0));
		if((callable_result.empty() || callable_result == "auto" ||
			callable_result == "const auto") && candidate->kind == "function-definition")
			infer_auto_operator_result(FindFunctionDefinitions("operator()",
				callable_type).empty() ? 0 : FindFunctionDefinitions("operator()",
				callable_type)[0], &callable_result);
		*result = callable_result;
		return !result->empty();
	}
	return false;
}


} // namespace pa18_templates_internal
