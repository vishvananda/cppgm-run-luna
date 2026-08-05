#include "pa11_semantics_analyzer.h"

TypePtr Analyzer::ResolveMemberPointerSpelling(Scope* from, const string& name) const
{
	const size_t marker = name.find("::*");
	if(marker == string::npos) return TypePtr();
	const size_t owner_open = name.rfind('(', marker);
	if(owner_open != string::npos) {
		const size_t owner_close = name.find(')', marker + 3);
		if(owner_close == string::npos) return TypePtr();
		const string owner_name = name.substr(owner_open + 1, marker - owner_open - 1);
		const string result_name = name.substr(0, owner_open);
		const string pointer_qualifiers = name.substr(marker + 3,
			owner_close - marker - 3);
		const string function_suffix = name.substr(owner_close + 1);
		if(owner_name.empty() || result_name.empty() || function_suffix.empty() ||
			function_suffix[0] != '(') return TypePtr();
		int depth = 0;
		size_t parameter_close = string::npos;
		for(size_t position = 0; position < function_suffix.size(); ++position) {
			if(function_suffix[position] == '(') ++depth;
			else if(function_suffix[position] == ')' && --depth == 0) {
				parameter_close = position;
				break;
			}
		}
		if(parameter_close == string::npos) return TypePtr();
		vector<string> parameter_names;
		string current;
		int angle_depth = 0, paren_depth = 0, bracket_depth = 0;
		const string parameter_text = function_suffix.substr(1, parameter_close - 1);
		for(size_t position = 0; position <= parameter_text.size(); ++position) {
			const char character = position < parameter_text.size() ?
				parameter_text[position] : ',';
			if(character == '<') ++angle_depth;
			else if(character == '>' && angle_depth > 0) --angle_depth;
			else if(character == '(') ++paren_depth;
			else if(character == ')' && paren_depth > 0) --paren_depth;
			else if(character == '[') ++bracket_depth;
			else if(character == ']' && bracket_depth > 0) --bracket_depth;
			if(character == ',' && angle_depth == 0 && paren_depth == 0 &&
				bracket_depth == 0) {
				while(!current.empty() && isspace(static_cast<unsigned char>(current[0])))
					current.erase(current.begin());
				while(!current.empty() && isspace(static_cast<unsigned char>(
					current[current.size() - 1]))) current.erase(current.size() - 1);
				if(!current.empty()) parameter_names.push_back(current);
				current.clear();
			} else current += character;
		}
		if(parameter_names.size() == 1 && parameter_names[0] == "void")
			parameter_names.clear();
		vector<TypePtr> parameters;
		for(size_t parameter = 0; parameter < parameter_names.size(); ++parameter)
			parameters.push_back(ResolveType(from, parameter_names[parameter]));
		const string function_qualifiers = function_suffix.substr(parameter_close + 1);
		const bool function_const = function_qualifiers.find("const") != string::npos;
		const bool function_volatile = function_qualifiers.find("volatile") != string::npos;
		const bool function_rvalue = function_qualifiers.find("&&") != string::npos;
		const bool function_lvalue = !function_rvalue && function_qualifiers.find('&') != string::npos;
		TypePtr function = FunctionOf(parameters, false, ResolveType(from, result_name),
			function_const, function_volatile, function_lvalue, function_rvalue);
		TypePtr result = MemberPointerTo(ResolveType(from, owner_name), function);
		return CloneWithCv(result, pointer_qualifiers.find("const") != string::npos,
			pointer_qualifiers.find("volatile") != string::npos);
	}
	const size_t owner_separator = name.rfind(' ', marker);
	const size_t owner_begin = owner_separator == string::npos ? 0 : owner_separator + 1;
	const string owner_name = name.substr(owner_begin, marker - owner_begin);
	const string result_name = name.substr(0, owner_begin);
	if(owner_name.empty() || result_name.empty()) return TypePtr();
	return MemberPointerTo(ResolveType(from, owner_name), ResolveType(from, result_name));
}

TypePtr Analyzer::ResolveDeclaratorSpelling(Scope* from, string name) const
{
	bool declarator_const = false, declarator_volatile = false;
	for(;;) {
		if(name.size() > 6 && name.compare(name.size() - 6, 6, " const") == 0) {
			declarator_const = true;
			name.erase(name.size() - 6);
			continue;
		}
		if(name.size() > 9 && name.compare(name.size() - 9, 9, " volatile") == 0) {
			declarator_volatile = true;
			name.erase(name.size() - 9);
			continue;
		}
		break;
	}
	TypeKind kind = TYPE_FUNDAMENTAL;
	if(name.size() >= 2 && name.compare(name.size() - 2, 2, "&&") == 0) {
		kind = TYPE_RVALUE_REFERENCE;
		name.erase(name.size() - 2);
	} else if(!name.empty() && name[name.size() - 1] == '&') {
		kind = TYPE_LVALUE_REFERENCE;
		name.erase(name.size() - 1);
	} else if(!name.empty() && name[name.size() - 1] == '*') {
		kind = TYPE_POINTER;
		name.erase(name.size() - 1);
	}
	if(kind == TYPE_FUNDAMENTAL) return TypePtr();
	while(!name.empty() && isspace(static_cast<unsigned char>(name[name.size() - 1])))
		name.erase(name.size() - 1);
	if(name.empty()) return TypePtr();
	TypePtr referred = ResolveType(from, name);
	TypePtr result = kind == TYPE_POINTER ? PointerTo(referred) : ReferenceTo(kind, referred);
	return CloneWithCv(result, declarator_const, declarator_volatile);
}
