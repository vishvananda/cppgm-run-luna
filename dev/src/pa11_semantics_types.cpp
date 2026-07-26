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
	// A substituted forwarding reference can leave an outer reference after
	// the complete function-pointer spelling (`int(*)()&`).  That suffix is a
	// reference to the pointer object, not a function cv/ref qualifier.  Peel
	// it before parsing the function type and restore it around the resulting
	// pointer below.
	string core = spelling;
	TypeKind outer_reference = TYPE_FUNDAMENTAL;
	if (core.size() > 2 && core.compare(core.size() - 2, 2, "&&") == 0) {
		outer_reference = TYPE_RVALUE_REFERENCE;
		core.erase(core.size() - 2);
	} else if (!core.empty() && core[core.size() - 1] == '&') {
		outer_reference = TYPE_LVALUE_REFERENCE;
		core.erase(core.size() - 1);
	}
	bool pointer_const = false;
	bool pointer_volatile = false;
	if (core.size() > 6 && core.compare(core.size() - 6, 6, " const") == 0) {
		pointer_const = true;
		core.erase(core.size() - 6);
	} else if (core.size() > 5 && core.compare(core.size() - 5, 5, "const") == 0) {
		pointer_const = true;
		core.erase(core.size() - 5);
	} else if (core.size() > 9 && core.compare(core.size() - 9, 9, " volatile") == 0) {
		pointer_volatile = true;
		core.erase(core.size() - 9);
	} else if (core.size() > 8 && core.compare(core.size() - 8, 8, "volatile") == 0) {
		pointer_volatile = true;
		core.erase(core.size() - 8);
	}
	const size_t pointer_open = core.find("(*");
	const size_t reference_open = core.find("(&");
	const bool function_pointer = pointer_open != string::npos;
	const bool function_reference = !function_pointer && reference_open != string::npos;
	const size_t direct_open = !function_pointer && !function_reference ?
		core.find('(') : string::npos;
	const bool direct_function = !function_pointer && !function_reference &&
		direct_open != string::npos && direct_open > 0 &&
		core[core.size() - 1] == ')' && core.find(')', 0) == core.size() - 1 &&
		core.substr(0, direct_open) != "decltype" &&
		core.substr(0, direct_open) != "sizeof" &&
		core.substr(0, direct_open) != "alignof" &&
		core.substr(0, direct_open) != "new";
	const size_t function_open = function_pointer ? pointer_open :
		function_reference ? reference_open : direct_open;
	const size_t parameters_open = function_open == string::npos ? string::npos :
		(direct_function ? function_open : core.find(")(", function_open + 2));
	if ((!function_pointer && !function_reference && !direct_function) ||
		parameters_open == string::npos ||
		core.empty() || core[core.size() - 1] != ')') return TypePtr();
	const string result_spelling = core.substr(0, function_open);
	const size_t parameter_begin = direct_function ? parameters_open + 1 : parameters_open + 2;
	const string parameter_spelling = core.substr(parameter_begin,
		core.size() - parameter_begin - 1);
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
	TypePtr result = function_pointer ? PointerTo(function) :
		function_reference ? ReferenceTo(TYPE_LVALUE_REFERENCE, function) : function;
	if(function_pointer && (pointer_const || pointer_volatile))
		result = CloneWithCv(result, pointer_const, pointer_volatile);
	return outer_reference == TYPE_FUNDAMENTAL ? result :
		ReferenceTo(outer_reference, result);
}
