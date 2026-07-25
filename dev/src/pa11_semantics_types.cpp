#include "pa11_semantics_analyzer.h"

TypePtr Analyzer::ResolveArrayReferenceSpelledType(const string& spelling, Scope* scope,
	SpecFacts& info)
{
	const size_t marker = spelling.find("(&)");
	if(marker == string::npos || marker + 3 >= spelling.size() || spelling[marker + 3] != '[')
		return TypePtr();
	const string referred_spelling = spelling.substr(0, marker) + spelling.substr(marker + 3);
	return ReferenceTo(TYPE_LVALUE_REFERENCE, ResolveSpelledType(referred_spelling, scope, info));
}

TypePtr Analyzer::ResolveFunctionSpelledType(const string& spelling, Scope* scope,
	SpecFacts& info)
{
	const size_t pointer_open = spelling.find("(*");
	const size_t reference_open = spelling.find("(&");
	const bool function_pointer = pointer_open != string::npos;
	const bool function_reference = !function_pointer && reference_open != string::npos;
	const size_t function_open = function_pointer ? pointer_open : reference_open;
	const size_t parameters_open = function_open == string::npos ? string::npos :
		spelling.find(")(", function_open + 2);
	if ((!function_pointer && !function_reference) || parameters_open == string::npos ||
		spelling.empty() || spelling[spelling.size() - 1] != ')') return TypePtr();
	const string result_spelling = spelling.substr(0, function_open);
	const string parameter_spelling = spelling.substr(parameters_open + 2,
		spelling.size() - parameters_open - 3);
	vector<string> parameter_names;
	int angle_depth = 0, parenthesis_depth = 0, bracket_depth = 0;
	size_t begin = 0;
	for(size_t position = 0; position <= parameter_spelling.size(); ++position) {
		const char ch = position == parameter_spelling.size() ? ',' : parameter_spelling[position];
		if(ch == '<') ++angle_depth;
		else if(ch == '>' && angle_depth > 0) --angle_depth;
		else if(ch == '(') ++parenthesis_depth;
		else if(ch == ')' && parenthesis_depth > 0) --parenthesis_depth;
		else if(ch == '[') ++bracket_depth;
		else if(ch == ']' && bracket_depth > 0) --bracket_depth;
		if(ch != ',' || angle_depth != 0 || parenthesis_depth != 0 || bracket_depth != 0) continue;
		string parameter = parameter_spelling.substr(begin, position - begin);
		while(!parameter.empty() && isspace(static_cast<unsigned char>(parameter[0]))) parameter.erase(parameter.begin());
		while(!parameter.empty() && isspace(static_cast<unsigned char>(parameter[parameter.size() - 1]))) parameter.erase(parameter.size() - 1);
		parameter_names.push_back(parameter);
		begin = position + 1;
	}
	bool variadic = false;
	vector<TypePtr> parameters;
	for(size_t parameter = 0; parameter < parameter_names.size(); ++parameter) {
		if(parameter_names[parameter].empty()) continue;
		if(parameter_names[parameter] == "...") { variadic = true; continue; }
		SpecFacts parameter_info;
		parameters.push_back(ResolveSpelledType(parameter_names[parameter], scope, parameter_info));
	}
	SpecFacts result_info;
	TypePtr result_type = ResolveSpelledType(result_spelling, scope, result_info);
	TypePtr function = FunctionOf(parameters, variadic, result_type);
	return function_pointer ? PointerTo(function) : ReferenceTo(TYPE_LVALUE_REFERENCE, function);
}
