#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
#include <cctype>
#include <functional>
#include <sstream>


using namespace std;

namespace pa18_templates_internal {

namespace {

string StringLiteralArrayReferenceType(const string& value)
{
	const size_t quote = value.find('"');
	if(quote == string::npos) return string();
	string element = "char";
	if(quote > 0) {
		const string prefix = value.substr(0, quote);
		if(prefix == "L") element = "wchar_t";
		else if(prefix == "u") element = "char16_t";
		else if(prefix == "U") element = "char32_t";
	}
	const size_t close = value.rfind('"');
	if(close == string::npos || close <= quote) return string();
	// Count decoded characters rather than source bytes so ordinary escapes do
	// not produce a false array bound.  The null terminator is part of the
	// language-defined array type.  The full literal decoder runs later in the
	// lowering pipeline; deduction only needs the element count here.
	size_t elements = 1;
	for(size_t i = quote + 1; i < close; ++i) {
		if(value[i] != '\\') {
			++elements;
			continue;
		}
		if(++i >= close) break;
		if(value[i] == 'u' || value[i] == 'U') {
			const size_t digits = value[i] == 'u' ? 4 : 8;
			i = min(close, i + digits);
			++elements;
		} else if(value[i] == 'x') {
			while(i + 1 < close && isxdigit(static_cast<unsigned char>(value[i + 1]))) ++i;
			++elements;
		} else if(value[i] >= '0' && value[i] <= '7') {
			for(size_t digit = 1; digit < 3 && i + 1 < close &&
				value[i + 1] >= '0' && value[i + 1] <= '7'; ++digit) ++i;
			++elements;
		} else ++elements;
	}
	ostringstream bound;
	bound << elements;
	return "const " + element + "(&)[" + bound.str() + "]";
}

bool ReferenceParameterPattern(const string& pattern)
{
	return (!pattern.empty() && pattern[pattern.size() - 1] == '&') ||
		pattern.find("(&") != string::npos || pattern.find("(& ") != string::npos;
}

bool IsLvalueTemplateArgument(const CPPGMAstNodePtr& expression)
{
	if(!expression) return false;
	if(expression->kind == "id-expression" || expression->kind == "member-expression" ||
		expression->kind == "subscript-expression" || expression->kind == "keyword-literal")
		return true;
	if(expression->kind == "parenthesized-expression" && !expression->children.empty())
		return IsLvalueTemplateArgument(expression->children[0]);
	if(expression->kind == "unary-expression" && !expression->children.empty())
		return RemoveMarker(expression->value) == "*" ||
			RemoveMarker(expression->value) == "++" ||
			RemoveMarker(expression->value) == "--";
	if(expression->kind == "binary-expression" && expression->children.size() >= 2 &&
		RemoveMarker(expression->value) == ",")
		return IsLvalueTemplateArgument(expression->children[1]);
	if(expression->kind == "conditional-expression" && expression->children.size() >= 3)
		return IsLvalueTemplateArgument(expression->children[1]) &&
			IsLvalueTemplateArgument(expression->children[2]);
	return false;
}

bool ConstReferenceParameterPattern(const string& pattern)
{
	for(size_t position = pattern.find("const"); position != string::npos;
		position = pattern.find("const", position + 5)) {
		const bool left = position == 0 || !IsIdentifierCharacter(pattern[position - 1]);
		const size_t end = position + 5;
		const bool right = end == pattern.size() || !IsIdentifierCharacter(pattern[end]);
		if(left && right) return true;
	}
	return false;
}

} // namespace

bool PA18TemplateExpander::IsKnownTypeSpelling(string raw, const string& context) const
{
	raw = CanonicalSpelling(raw);
	if(raw.empty()) return false;
	const string original = raw;
	const map<string, string>::const_iterator direct_alias = type_aliases_.find(original);
	const map<string, string>::const_iterator scoped_alias = type_aliases_.find(
		JoinPath(context, original));
	const map<string, vector<string> >::const_iterator named_aliases =
		type_aliases_by_name_.find(LastComponent(original));
	const bool named_alias = direct_alias != type_aliases_.end() ||
		scoped_alias != type_aliases_.end() ||
		(named_aliases != type_aliases_by_name_.end() && named_aliases->second.size() == 1);
	raw = CanonicalSpelling(ResolveAlias(raw, context));
	if(raw.empty()) return false;
	while(raw.compare(0, 6, "const ") == 0)
		raw = CanonicalSpelling(raw.substr(6));
	while(raw.compare(0, 9, "volatile ") == 0)
		raw = CanonicalSpelling(raw.substr(9));
	while(raw.size() > 6 && raw.compare(raw.size() - 6, 6, " const") == 0)
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 6));
	while(raw.size() > 9 && raw.compare(raw.size() - 9, 9, " volatile") == 0)
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 9));
	while(!raw.empty() && (raw[raw.size() - 1] == '&' || raw[raw.size() - 1] == '*'))
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 1));
	while(!raw.empty() && raw[raw.size() - 1] == ']') {
		const size_t open = raw.rfind('[');
		if(open == string::npos) break;
		raw = CanonicalSpelling(raw.substr(0, open));
	}
	if(named_alias || raw == "void" || raw == "nullptr_t" ||
		raw == "std::nullptr_t" || raw == "wchar_t" || raw == "char16_t" ||
		raw == "char32_t" || raw == "__int128" || raw == "unsigned __int128" ||
		IsBuiltinArithmeticType(raw) || class_contexts_.find(raw) != class_contexts_.end() ||
		FindClassDeclaration(raw, context)) return true;
	for(string current = context; ; ) {
		if(named_type_contexts_.find(JoinPath(current, raw)) != named_type_contexts_.end()) return true;
		if(current.empty()) break;
		const size_t separator = current.rfind("::");
		if(separator == string::npos) current.clear();
		else current.erase(separator);
	}
	return named_type_contexts_.find(raw) != named_type_contexts_.end();
}

bool PA18TemplateExpander::HasUnresolvedTemplateParameter(string raw,
	const string& context, const map<string, string>& substitutions) const
{
	raw = NormalizeTypeArgument(raw);
	raw = CanonicalSpelling(ReplaceIdentifiers(raw, substitutions));
	if(raw.empty()) return false;
	for(size_t position = 0; position < raw.size();) {
		if(!IsIdentifierCharacter(raw[position])) {
			++position;
			continue;
		}
		const size_t begin = position;
		while(position < raw.size() && IsIdentifierCharacter(raw[position])) ++position;
		const string word = raw.substr(begin, position - begin);
		if(template_parameter_names_.find(word) == template_parameter_names_.end()) continue;
		if(FindClassDeclaration(word, context) || FindDefinition(word, context) ||
			type_aliases_.find(word) != type_aliases_.end()) continue;
		return true;
	}
	return false;
}

bool PA18TemplateExpander::LookupVariableType(const string& name,
	const string& context, string* result) const
{
	if(!result) return false;
	const string key = LastComponent(RemoveMarker(name));
	if(key.empty()) return false;
	for(string current = context; ; ) {
		map<string, map<string, string> >::const_iterator scope =
			function_parameter_types_.find(current);
		if(scope != function_parameter_types_.end()) {
			map<string, string>::const_iterator found = scope->second.find(key);
			if(found != scope->second.end()) {
				*result = found->second;
				return true;
			}
		}
		if(current.empty()) break;
		const size_t separator = current.rfind("::");
		if(separator == string::npos) current.clear();
		else current.erase(separator);
	}
	// A replayed function-parameter pack gives later expanded identifiers
	// (`arg__pack2`, and so on) no ordinary declaration binding.  Pair the
	// generated identifier with its typed pack element before falling back to
	// translation-unit variable facts; otherwise every expanded argument is
	// inferred from the first pack element.
	for(map<string, vector<string> >::const_iterator identifiers =
		active_pack_identifier_substitutions_.begin();
		identifiers != active_pack_identifier_substitutions_.end(); ++identifiers) {
		const vector<string>& names = identifiers->second;
		vector<string>::const_iterator name = find(names.begin(), names.end(), key);
		if(name == names.end()) continue;
		map<string, vector<string> >::const_iterator values =
			active_function_pack_substitutions_.find(identifiers->first);
		const size_t index = static_cast<size_t>(name - names.begin());
		if(values != active_function_pack_substitutions_.end() &&
			index < values->second.size()) {
			*result = values->second[index];
			return true;
		}
	}
	map<string, string>::const_iterator found = variable_types_.find(key);
	if(found != variable_types_.end()) { *result = found->second; return true; }
	const string raw_name = RemoveMarker(name);
	map<string, string>::const_iterator enumerator = enumerator_types_.find(raw_name);
	if(enumerator == enumerator_types_.end()) enumerator = enumerator_types_.find(key);
	if(enumerator == enumerator_types_.end()) return false;
	*result = enumerator->second;
	return true;
}

bool PA18TemplateExpander::InferCallableObjectCall(const CPPGMAstNodePtr& call,
	const string& object_type, const map<string, string>& substitutions,
	const string& context, string* result) const
{
	if(!call || !result) return false;
	size_t argument_count = 0;
	if(call->children.size() > 1 && call->children[1] &&
		call->children[1]->kind == "argument-list")
		argument_count = call->children[1]->children.size();
	string initial = CanonicalSpelling(ReplaceIdentifiers(object_type, substitutions));
	while(initial.compare(0, 6, "const ") == 0)
		initial = CanonicalSpelling(initial.substr(6));
	while(initial.compare(0, 9, "volatile ") == 0)
		initial = CanonicalSpelling(initial.substr(9));
	while(!initial.empty() && (initial[initial.size() - 1] == '&' ||
		initial[initial.size() - 1] == '*'))
		initial.erase(initial.size() - 1);
	initial = CanonicalSpelling(initial);
	if(initial.empty()) return false;
	set<string> active;
	function<bool(const string&, const map<string, string>&, const string&)> inspect;
	inspect = [&](const string& raw_class, const map<string, string>& inherited,
		const string& inherited_scope) {
		string class_spelling = CanonicalSpelling(raw_class);
		while(class_spelling.compare(0, 6, "const ") == 0)
			class_spelling = CanonicalSpelling(class_spelling.substr(6));
		while(class_spelling.compare(0, 9, "volatile ") == 0)
			class_spelling = CanonicalSpelling(class_spelling.substr(9));
		while(!class_spelling.empty() && (class_spelling[class_spelling.size() - 1] == '&' ||
			class_spelling[class_spelling.size() - 1] == '*'))
			class_spelling.erase(class_spelling.size() - 1);
		class_spelling = CanonicalSpelling(class_spelling);
		if(class_spelling.empty()) return false;
		const string active_key = class_spelling + "|" + inherited_scope;
		if(!active.insert(active_key).second) return false;
		const size_t open = class_spelling.find('<');
		string class_base = class_spelling;
		vector<string> class_arguments;
		if(open != string::npos) {
			string argument_text;
			size_t close = string::npos;
			if(!TemplateRange(class_spelling, open, &argument_text, &close)) {
				active.erase(active_key);
				return false;
			}
			class_base = CanonicalSpelling(class_spelling.substr(0, open));
			class_arguments = SplitTemplateArguments(argument_text);
		}
		const TemplateDefinition* primary = FindDefinition(class_base, context);
		const TemplateDefinition* definition = primary && primary->class_template ?
			SelectClassTemplateDefinition(primary, class_arguments, context) : primary;
		CPPGMAstNodePtr declaration = definition && definition->declaration ?
			definition->declaration : FindClassDeclaration(class_spelling, context);
		if(!declaration) {
			active.erase(active_key);
			return false;
		}
		map<string, string> local = inherited;
		string scope = definition && !definition->qualified_name.empty() ?
			definition->qualified_name : inherited_scope;
		if(scope.empty()) scope = class_base;
		if(definition) {
			for(size_t parameter = 0; parameter < definition->parameters.size() &&
				parameter < class_arguments.size(); ++parameter) {
				const string& name = definition->parameters[parameter].name;
				if(name.empty()) continue;
				string value = CanonicalSpelling(class_arguments[parameter]);
				if(definition->partial_specialization && parameter <
					definition->specialization_pattern.size()) {
					const string pattern = CanonicalSpelling(
						definition->specialization_pattern[parameter]);
					if(pattern.size() > 1 && pattern[pattern.size() - 1] == '*' &&
						!value.empty() && value[value.size() - 1] == '*')
						value = CanonicalSpelling(value.substr(0, value.size() - 1));
				}
				local[name] = value;
			}
		}
		// Visit inherited arity-specific callable overloads before wrapper members.
		for(size_t child = 0; child < declaration->children.size(); ++child) {
			const CPPGMAstNodePtr& member = declaration->children[child];
			if(!member || member->kind != "base-clause") continue;
			for(size_t base_index = 0; base_index < member->children.size(); ++base_index) {
				const CPPGMAstNodePtr& specifier = member->children[base_index];
				const CPPGMAstNodePtr base_name = ChildOfKindLocal(specifier, "base-name");
				if(!base_name) continue;
				string base_spelling = NormalizeElaboratedSpelling(
					ReplaceIdentifiers(base_name->value, local), context);
				base_spelling = CanonicalSpelling(base_spelling);
				if(inspect(base_spelling, local, scope)) {
					active.erase(active_key);
					return true;
				}
			}
		}
		for(size_t child = 0; child < declaration->children.size(); ++child) {
			const CPPGMAstNodePtr& member = declaration->children[child];
			if(!member || (member->kind != "special-member-declaration" &&
				member->kind != "special-member-definition")) continue;
			string name = RemoveMarker(member->value);
			if(name.compare(0, 8, "operator") != 0) continue;
			string target = CanonicalSpelling(name.substr(8));
			if(target.empty() || target[0] == '(' || target[0] == '[') continue;
			target = CanonicalSpelling(ReplaceIdentifiers(target, local));
			target = CanonicalSpelling(ResolveAlias(target, scope));
			string return_type;
			vector<string> parameters;
			string direct_qualifiers;
			const bool function_pointer = SplitFunctionPointerType(target, &return_type,
				&parameters);
			const bool direct_function = !function_pointer && SplitDirectFunctionType(
				target, &return_type, &parameters, &direct_qualifiers);
			if((function_pointer || direct_function) && parameters.size() == argument_count) {
				*result = CanonicalSpelling(return_type);
				active.erase(active_key);
				return !result->empty();
			}
		}
		active.erase(active_key);
		return false;
	};
	return inspect(initial, substitutions, context);
}

bool PA18TemplateExpander::InferMemberArgument(const CPPGMAstNodePtr& expression,
	string* result, const map<string, string>& substitutions,
	const string& context) const
{
	if(!expression || expression->children.size() < 2) return false;
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
	} else {
		InferArgument(expression->children[0], &object_type, substitutions, context);
		if(object_type.empty() && expression->children[0] &&
			expression->children[0]->kind == "id-expression")
			object_type = CanonicalSpelling(ResolveAlias(
				expression->children[0]->value, context));
	}
	const string member = expression->children[1] ?
		LastComponent(expression->children[1]->value) : string();
	set<string> active;
	if(!object_type.empty() && !member.empty() && FindClassMemberType(
		object_type, member, substitutions, context, result, &active)) return true;
	if(!object_type.empty() && named_type_contexts_.find(
		CanonicalSpelling(ResolveAlias(object_type, context))) !=
		named_type_contexts_.end()) {
		*result = CanonicalSpelling(ResolveAlias(object_type, context));
		return true;
	}
	return false;
}

bool PA18TemplateExpander::InferIdentifierArgument(const CPPGMAstNodePtr& expression,
	string* result, const map<string, string>& substitutions,
	const string& context, FunctionSignature* function_signature) const
{
	if(!expression || expression->kind != "id-expression") return false;
	const string qualified_name = RemoveMarker(expression->value);
	const size_t qualified_separator = qualified_name.rfind("::");
	if(qualified_separator != string::npos) {
		const string qualified_owner = CanonicalSpelling(ResolveAlias(
			qualified_name.substr(0, qualified_separator), context));
		if(named_type_contexts_.find(qualified_owner) != named_type_contexts_.end()) {
			*result = qualified_owner;
			return true;
		}
	}
	// Function-local classes are promoted into translation-unit entities during
	// collection, so deduction must carry the promoted identity forward.
	for(string current = context; ; ) {
		map<string, string>::const_iterator promoted = local_class_names_.find(
			JoinPath(current, LastComponent(qualified_name)));
		if(promoted != local_class_names_.end() && !promoted->second.empty()) {
			*result = promoted->second;
			return true;
		}
		if(current.empty()) break;
		const size_t separator = current.rfind("::");
		if(separator == string::npos) current.clear();
		else current.erase(separator);
	}
	const string promoted_local = PromotedLocalClass(LastComponent(qualified_name), context);
	if(!promoted_local.empty()) {
		*result = promoted_local;
		return true;
	}
	// Prefer a parameter in the current class constructor over the collector's
	// translation-unit fallback map when source names are reused.
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
		vector<string>::const_iterator expanded_name = find(pack->second.begin(),
			pack->second.end(), expression->value);
		if(expanded_name == pack->second.end()) continue;
		const size_t element = static_cast<size_t>(expanded_name - pack->second.begin());
		map<string, vector<string> >::const_iterator typed_pack =
			active_function_pack_substitutions_.find(pack->first);
		if(typed_pack != active_function_pack_substitutions_.end() &&
			element < typed_pack->second.size()) {
			*result = typed_pack->second[element];
			return !result->empty();
		}
		string source;
		if(!LookupVariableType(pack->first, context, &source)) continue;
		*result = ReplaceIdentifiers(ResolveAlias(source, context), substitutions);
		if(!result->empty()) return true;
	}
	string variable_type;
	if(LookupVariableType(expression->value, context, &variable_type)) {
		string promoted_type = variable_type;
		const size_t array_suffix = promoted_type.find('[');
		const string variable_base = CanonicalSpelling(promoted_type.substr(0,
			array_suffix == string::npos ? promoted_type.size() : array_suffix));
		for(string current = context; ; ) {
			map<string, string>::const_iterator promoted = local_class_names_.find(
				JoinPath(current, variable_base));
			if(promoted != local_class_names_.end() && !promoted->second.empty()) {
				map<string, string> local_substitution;
				local_substitution[variable_base] = promoted->second;
				promoted_type = ReplaceIdentifiers(promoted_type, local_substitution);
				break;
			}
			if(current.empty()) break;
			const size_t separator = current.rfind("::");
			if(separator == string::npos) current.clear();
			else current.erase(separator);
		}
		*result = ReplaceIdentifiers(ResolveAlias(promoted_type, context), substitutions);
		return !result->empty();
	}
	const FunctionSignature* signature = FindFunctionSignature(expression->value, context);
	if(signature) {
		*result = FunctionSignatureType(*signature);
		if(function_signature) *function_signature = *signature;
		return true;
	}
	return false;
}

bool PA18TemplateExpander::InferCallMemberArgument(const CPPGMAstNodePtr& expression,
	string* result, const map<string, string>& substitutions,
	const string& context) const
{
	if(!expression || expression->children.empty() || !expression->children[0] ||
		expression->children[0]->kind != "member-expression") return false;
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
	return !object_type.empty() && !member.empty() && FindClassMemberType(
		object_type, member, substitutions, context, result, &active);
}

bool PA18TemplateExpander::InferCallArgument(const CPPGMAstNodePtr& expression,
	string* result, const map<string, string>& substitutions,
	const string& context) const
{
	if(InferCallMemberArgument(expression, result, substitutions, context)) return true;
	return InferCallIdentifierArgument(expression, result, substitutions, context);
}

bool PA18TemplateExpander::InferCastArgument(const CPPGMAstNodePtr& expression,
	string* result, const map<string, string>& substitutions,
	const string& context) const
{
	if(!expression || expression->kind != "cast-expression" ||
		expression->children.empty()) return false;
	const CPPGMAstNodePtr type_id = expression->children[0];
	if(!type_id || type_id->kind != "type-id") return false;
	string spelling = NormalizeTypeArgument(TypeIdSpelling(type_id));
	if (spelling.find('<') != string::npos) {
		string rewritten = const_cast<PA18TemplateExpander*>(this)->RewriteText(
			spelling, context, substitutions, 0);
		if (!rewritten.empty()) spelling = NormalizeTypeArgument(rewritten);
		spelling = NormalizeTypeArgument(ResolveAlias(spelling, context));
	}
	const CPPGMAstNodePtr specs = ChildOfKindLocal(type_id, "type-specifier-seq");
	bool expanded_function_pointer = false;
	if(specs) for(size_t child = 0; child < specs->children.size(); ++child) {
		const CPPGMAstNodePtr item = specs->children[child];
		if(item && item->kind == "type-name" &&
			RemoveMarker(item->value).find("(*)") != string::npos) {
			expanded_function_pointer = true;
			break;
		}
	}
	if(expanded_function_pointer && spelling.compare(0, 6, "const ") == 0) {
		spelling.erase(0, 6);
		if(!spelling.empty() && spelling[spelling.size() - 1] == '*') {
			spelling.erase(spelling.size() - 1);
			spelling = CanonicalSpelling(spelling + " const*");
		} else spelling = CanonicalSpelling(spelling + " const");
	} else if(expanded_function_pointer && spelling.compare(0, 9, "volatile ") == 0) {
		spelling.erase(0, 9);
		if(!spelling.empty() && spelling[spelling.size() - 1] == '*') {
			spelling.erase(spelling.size() - 1);
			spelling = CanonicalSpelling(spelling + " volatile*");
		} else spelling = CanonicalSpelling(spelling + " volatile");
	}
	*result = spelling;
	return !result->empty();
}

bool PA18TemplateExpander::InferArgument(const CPPGMAstNodePtr& expression,
	string* result, const map<string, string>& substitutions,
	const string& context, FunctionSignature* function_signature) const
	{
		if(!expression || !result) return false;
		if(function_signature) *function_signature = FunctionSignature();
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
		if(expression->kind == "parenthesized-expression" &&
			!expression->children.empty())
			return InferArgument(expression->children[0], result, substitutions,
				context, function_signature);
		if(expression->kind == "member-expression")
			return InferMemberArgument(expression, result, substitutions, context);
	if(InferCastArgument(expression, result, substitutions, context)) return true;
		if(expression->kind == "id-expression")
			return InferIdentifierArgument(expression, result, substitutions, context,
				function_signature);
		if(expression->kind == "call-expression")
			return InferCallArgument(expression, result, substitutions, context);
		if(expression->kind == "unary-expression" && !expression->children.empty()) {
			const string op = RemoveMarker(expression->value);
			if(op == "*") {
				string object_type;
				if(InferArgument(expression->children[0], &object_type, substitutions, context)) {
					set<string> active;
					if(FindClassMemberType(object_type, "operator*", substitutions, context,
						result, &active)) return true;
				}
			}
		}
		if(expression->kind == "binary-expression" &&
			InferBinaryArgument(expression, result, substitutions, context)) return true;
		if(expression->kind == "unary-expression" && !expression->children.empty()) {
			const string op = RemoveMarker(expression->value);
			if(op == "&") {
				FunctionSignature child_signature;
				if(!InferArgument(expression->children[0], result, substitutions, context,
					&child_signature)) return false;
				// InferIdentifierArgument already models a named function as its
				// decayed function-pointer type.  Taking its address preserves that
				// pointer; appending another `*` would incorrectly form a pointer to
				// the function pointer and lose the function type during deduction.
				if(child_signature.result_specifiers && child_signature.parameters)
					return true;
				*result = CanonicalSpelling(*result + "*");
				return true;
			}
			if(op == "*" && InferArgument(expression->children[0], result, substitutions, context)) {
				if(!result->empty() && result->at(result->size() - 1) == '*')
					result->erase(result->size() - 1);
				else if(result->size() > 6 && result->compare(result->size() - 6, 6, " const") == 0 &&
					result->size() > 6 && (*result)[result->size() - 7] == '*')
					result->erase(result->size() - 7, 1);
				else if(result->size() > 9 && result->compare(result->size() - 9, 9, " volatile") == 0 &&
					result->size() > 9 && (*result)[result->size() - 10] == '*')
					result->erase(result->size() - 10, 1);
				*result = CanonicalSpelling(*result + "&");
				return true;
			}
		}
		return false;
	}
bool PA18TemplateExpander::MergeInferredFunctionArgument(
	const TemplateDefinition& definition, const string& pattern, const string& type,
	const FunctionSignature& signature, const map<string, string>& substitutions,
	const string& context, const set<string>& parameter_names,
	map<string, string>* inferred, map<string, vector<string> >* inferred_packs,
	map<string, FunctionSignature>* inferred_functions,
	const map<string, vector<string> >* bound_pack_values,
	const set<string>* fixed_template_parameters) const
{
	map<string, string> one;
	string match_pattern = pattern;
	map<string, string> pattern_substitutions;
	bool alias_substitution = false;
	bool dependent_substitution = false;
	for(map<string, string>::const_iterator substitution = substitutions.begin();
		substitution != substitutions.end(); ++substitution)
		if(parameter_names.find(substitution->first) == parameter_names.end()) {
			pattern_substitutions[substitution->first] = substitution->second;
			const size_t position = pattern.find(substitution->first);
			const size_t end = position == string::npos ? string::npos :
				position + substitution->first.size();
			const bool left = position == 0 || position == string::npos ||
				!IsIdentifierCharacter(pattern[position - 1]);
			const bool right = position == string::npos || end == pattern.size() ||
				!IsIdentifierCharacter(pattern[end]);
			if(position != string::npos && left && right) {
				if(!FindDefinition(substitution->first, context) &&
					class_contexts_.find(substitution->first) == class_contexts_.end())
					dependent_substitution = true;
			}
			if(type_aliases_by_name_.find(substitution->first) != type_aliases_by_name_.end()) {
				if(position != string::npos && left && right) alias_substitution = true;
			}
		}
	for(map<string, string>::const_iterator value = inferred->begin();
		value != inferred->end(); ++value)
		pattern_substitutions[value->first] = value->second;
	bool rewrite_inferred = !inferred->empty();
	const auto template_shape = [this](string raw) -> pair<string, size_t> {
		raw = CanonicalSpelling(raw);
		while(raw.compare(0, 6, "const ") == 0 || raw.compare(0, 9, "volatile ") == 0)
			raw = CanonicalSpelling(raw.substr(raw.find(' ') + 1));
		while(!raw.empty() && (raw[raw.size() - 1] == '&' || raw[raw.size() - 1] == '*'))
			raw.erase(raw.size() - 1);
		const size_t open = raw.find('<');
		if(open == string::npos || raw.rfind('>') == string::npos)
			return make_pair(string(), static_cast<size_t>(0));
		return make_pair(LastComponent(raw.substr(0, open)),
			SplitTemplateArguments(raw.substr(open + 1, raw.rfind('>') - open - 1)).size());
	};
	if(rewrite_inferred && pattern.find('<') != string::npos && type.find('<') != string::npos) {
		const pair<string, size_t> pattern_shape = template_shape(pattern);
		const pair<string, size_t> actual_shape = template_shape(type);
		if(!pattern_shape.first.empty() && pattern_shape == actual_shape &&
			pattern.find("...") == string::npos) rewrite_inferred = false;
	}
	bool bound_pack_in_pattern = false;
	if(bound_pack_values) for(map<string, vector<string> >::const_iterator bound =
		bound_pack_values->begin(); bound != bound_pack_values->end() && !bound_pack_in_pattern;
		++bound) {
		if(bound->first.empty()) continue;
		for(size_t position = pattern.find(bound->first); position != string::npos;
			position = pattern.find(bound->first, position + bound->first.size())) {
			const bool left = position == 0 || !IsIdentifierCharacter(pattern[position - 1]);
			const size_t end = position + bound->first.size();
			const bool right = end == pattern.size() || !IsIdentifierCharacter(pattern[end]);
			if(left && right) { bound_pack_in_pattern = true; break; }
		}
	}
	const bool rewrite_pattern = !pattern_substitutions.empty() &&
		(alias_substitution || (rewrite_inferred && pattern.find("::") != string::npos) ||
		bound_pack_in_pattern || dependent_substitution);
	if(rewrite_pattern) {
		match_pattern = const_cast<PA18TemplateExpander*>(this)->RewriteText(
			pattern, context, pattern_substitutions, 0);
		match_pattern = NormalizeTypeArgument(ReplaceIdentifiers(match_pattern,
			pattern_substitutions));
		match_pattern = ResolveAlias(match_pattern, context);
		// A fixed enclosing-class binding can itself be an lvalue reference.  When
		// that binding fills a forwarding-reference parameter, textual substitution
		// produces `T& &&`; collapse the two reference layers before matching the
		// already typed call argument.
		bool fixed_lvalue_reference = false;
		for(map<string, string>::const_iterator substitution = pattern_substitutions.begin();
			substitution != pattern_substitutions.end(); ++substitution)
			if(!substitution->second.empty() &&
				substitution->second[substitution->second.size() - 1] == '&') {
				fixed_lvalue_reference = true;
				break;
			}
		if(fixed_lvalue_reference && match_pattern.size() >= 2 &&
			match_pattern.compare(match_pattern.size() - 2, 2, "&&") == 0)
			match_pattern.erase(match_pattern.size() - 1);
	}
	set<string> matching_parameter_names = parameter_names;
	string matching_pattern = match_pattern;
	for(map<string, string>::const_iterator binding = inferred->begin();
		binding != inferred->end(); ++binding) {
		if(!fixed_template_parameters || fixed_template_parameters->find(binding->first) ==
			fixed_template_parameters->end()) continue;
		bool present = false;
		for(size_t at = matching_pattern.find(binding->first); at != string::npos;
			at = matching_pattern.find(binding->first, at + binding->first.size())) {
			const bool left = at == 0 || !IsIdentifierCharacter(matching_pattern[at - 1]);
			const size_t end = at + binding->first.size();
			const bool right = end == matching_pattern.size() ||
				!IsIdentifierCharacter(matching_pattern[end]);
			if(left && right) { present = true; break; }
		}
		if(present) {
			map<string, string> fixed_binding;
			fixed_binding[binding->first] = binding->second;
			matching_pattern = ReplaceIdentifiers(matching_pattern, fixed_binding);
			matching_parameter_names.erase(binding->first);
		}
	}
	const bool matched = MatchTypePattern(matching_pattern, type,
		matching_parameter_names, &one, context);
	if(!matched) {
		const bool lvalue = match_pattern.size() > 0 && match_pattern[match_pattern.size() - 1] == '&' &&
			(match_pattern.size() < 2 || match_pattern[match_pattern.size() - 2] != '&');
		const bool rvalue = match_pattern.size() > 1 &&
			match_pattern.compare(match_pattern.size() - 2, 2, "&&") == 0;
		if(lvalue && match_pattern.find("const") == string::npos) return false;
		if(rvalue) {
			const string target = CanonicalSpelling(match_pattern.substr(0, match_pattern.size() - 2));
			string actual_type = CanonicalSpelling(type);
			while(!actual_type.empty() && actual_type[actual_type.size() - 1] == '&')
				actual_type = CanonicalSpelling(actual_type.substr(0, actual_type.size() - 1));
			if(!IsBuiltinArithmeticType(actual_type) ||
				!IsBuiltinArithmeticType(ReplaceIdentifiers(target, *inferred))) return false;
		}
		for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter)
			if(definition.parameters[parameter].name == pattern) return false;
		string fixed_pattern_source = match_pattern;
		map<string, string> fixed_substitutions;
		if(fixed_template_parameters) for(map<string, string>::const_iterator binding =
			inferred->begin(); binding != inferred->end(); ++binding)
			if(fixed_template_parameters->find(binding->first) != fixed_template_parameters->end())
				fixed_substitutions[binding->first] = binding->second;
		if(!fixed_substitutions.empty()) {
			const string substituted_pattern = NormalizeTypeArgument(ReplaceIdentifiers(
				match_pattern, fixed_substitutions));
			if(!HasUnresolvedTemplateParameter(substituted_pattern, context, fixed_substitutions))
				fixed_pattern_source = substituted_pattern;
		}
		string fixed_pattern = CanonicalSpelling(fixed_pattern_source);
		string fixed_actual = CanonicalSpelling(type);
		while(fixed_pattern.compare(0, 6, "const ") == 0)
			fixed_pattern = CanonicalSpelling(fixed_pattern.substr(6));
		while(fixed_actual.compare(0, 6, "const ") == 0)
			fixed_actual = CanonicalSpelling(fixed_actual.substr(6));
		while(fixed_pattern.size() > 6 && fixed_pattern.compare(fixed_pattern.size() - 6, 6, " const") == 0)
			fixed_pattern = CanonicalSpelling(fixed_pattern.substr(0, fixed_pattern.size() - 6));
		while(fixed_actual.size() > 6 && fixed_actual.compare(fixed_actual.size() - 6, 6, " const") == 0)
			fixed_actual = CanonicalSpelling(fixed_actual.substr(0, fixed_actual.size() - 6));
		if((fixed_pattern.find('*') != string::npos) != (fixed_actual.find('*') != string::npos)) return false;
		while(!fixed_pattern.empty() && (fixed_pattern[fixed_pattern.size() - 1] == '&' ||
			fixed_pattern[fixed_pattern.size() - 1] == '*')) fixed_pattern.erase(fixed_pattern.size() - 1);
		while(!fixed_actual.empty() && (fixed_actual[fixed_actual.size() - 1] == '&' ||
			fixed_actual[fixed_actual.size() - 1] == '*')) fixed_actual.erase(fixed_actual.size() - 1);
		while(fixed_pattern.size() > 6 && fixed_pattern.compare(fixed_pattern.size() - 6, 6, " const") == 0)
			fixed_pattern = CanonicalSpelling(fixed_pattern.substr(0, fixed_pattern.size() - 6));
		while(fixed_actual.size() > 6 && fixed_actual.compare(fixed_actual.size() - 6, 6, " const") == 0)
			fixed_actual = CanonicalSpelling(fixed_actual.substr(0, fixed_actual.size() - 6));
		// A nondependent parameter still participates in candidate viability.  Do
		// not admit every pair of known class types here: that turns an unrelated
		// overload into a successful deduction and can recursively re-enter the
		// same template through the generated call expression.  Reuse the typed
		// argument-compatibility check, which retains arithmetic conversions while
		// rejecting unrelated complete class objects.
		if(!HasUnresolvedTemplateParameter(fixed_pattern_source, context, fixed_substitutions))
			return FunctionArgumentViable(fixed_pattern_source, type, context);
		return !(FindClassDeclaration(fixed_pattern, context) &&
			FindClassDeclaration(fixed_actual, context) &&
			LastComponent(fixed_pattern) != LastComponent(fixed_actual));
	}
	if(inferred_functions && signature.result_specifiers && signature.parameters)
		for(map<string, string>::const_iterator binding = one.begin(); binding != one.end(); ++binding)
			if(binding->second == type || binding->first == pattern)
				(*inferred_functions)[binding->first] = signature;
	for(map<string, string>::const_iterator it = one.begin(); it != one.end(); ++it) {
		const size_t template_index = find_if(definition.parameters.begin(), definition.parameters.end(),
			[&](const TemplateParameter& candidate) { return candidate.name == it->first; }) -
			definition.parameters.begin();
		if(template_index < definition.parameters.size() && definition.parameters[template_index].pack) {
			string packed = it->second;
			if(packed.size() > 3 && packed.compare(packed.size() - 3, 3, "...") == 0) {
				const string prefix = CanonicalSpelling(packed.substr(0, packed.size() - 3));
				const bool dependent_pack = !prefix.empty() &&
					prefix.find_first_not_of(
						"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") == string::npos &&
					template_pack_names_.find(prefix) != template_pack_names_.end();
				if(!dependent_pack) packed = prefix;
			}
			const vector<string> values = SplitTemplateArguments(packed);
			if(!packed.empty())
				(*inferred_packs)[it->first].insert((*inferred_packs)[it->first].end(),
					values.begin(), values.end());
		} else {
			map<string, string>::const_iterator prior = inferred->find(it->first);
			if(prior != inferred->end() && CanonicalSpelling(ResolveAlias(prior->second, context)) !=
				CanonicalSpelling(ResolveAlias(it->second, context))) return false;
			(*inferred)[it->first] = it->second;
		}
	}
	return true;
}

bool PA18TemplateExpander::InferFunctionParameter(
	const TemplateDefinition& definition, const CPPGMAstNodePtr& parameter,
	const CPPGMAstNodePtr& parameter_list, size_t parameter_position,
	const CPPGMAstNodePtr& arguments, size_t* argument_index,
	const map<string, string>& substitutions, const string& context,
	const set<string>& parameter_names, map<string, string>* inferred,
	map<string, vector<string> >* inferred_packs, vector<string>* deferred_patterns,
	vector<CPPGMAstNodePtr>* deferred_arguments,
	map<string, FunctionSignature>* inferred_functions,
	const map<string, vector<string> >* bound_pack_values,
	map<string, vector<string> >* forwarding_pack_values,
	const set<string>* fixed_template_parameters) const
{
	if(!parameter || parameter->kind != "parameter-declaration" || !argument_index) return true;
	const string pattern = parameter->children.size() > 1 && parameter->children[1] &&
		ChildOfKindLocal(parameter->children[1], "nested-declarator") &&
		ChildOfKindLocal(parameter->children[1], "parameter-clause") ?
		FunctionTypeSpelling(parameter) : ParameterTypeSpelling(parameter);
	string deduction_pattern = pattern;
	bool dependent_parameter_pattern = false;
	for(size_t template_parameter = 0; template_parameter < definition.parameters.size();
		++template_parameter) {
		const string& name = definition.parameters[template_parameter].name;
		if(name.empty()) continue;
		for(size_t at = deduction_pattern.find(name); at != string::npos;
			at = deduction_pattern.find(name, at + name.size())) {
			const bool left = at == 0 || !IsIdentifierCharacter(deduction_pattern[at - 1]);
			const size_t end = at + name.size();
			const bool right = end == deduction_pattern.size() ||
				!IsIdentifierCharacter(deduction_pattern[end]);
			if(left && right) { dependent_parameter_pattern = true; break; }
		}
		if(dependent_parameter_pattern) break;
	}
	if(!dependent_parameter_pattern) {
		const string resolved_parameter_pattern = NormalizeTypeArgument(ResolveAlias(
			deduction_pattern, context));
		if(!resolved_parameter_pattern.empty()) deduction_pattern = resolved_parameter_pattern;
	}
	const bool pack_parameter = IsFunctionParameterPack(parameter);
	const vector<string>* explicit_pack_values = 0;
	string explicit_pack_name;
	if(pack_parameter && inferred_packs) for(size_t template_parameter = 0;
		template_parameter < definition.parameters.size(); ++template_parameter) {
		const TemplateParameter& candidate = definition.parameters[template_parameter];
		if(!candidate.pack || candidate.name.empty()) continue;
		for(size_t position = pattern.find(candidate.name); position != string::npos;
			position = pattern.find(candidate.name, position + candidate.name.size())) {
			const bool left = position == 0 || !IsIdentifierCharacter(pattern[position - 1]);
			const size_t end = position + candidate.name.size();
			const bool right = end == pattern.size() || !IsIdentifierCharacter(pattern[end]);
			if(left && right) {
				map<string, vector<string> >::const_iterator values = inferred_packs->find(candidate.name);
				if(values != inferred_packs->end()) {
					explicit_pack_name = candidate.name;
					explicit_pack_values = &values->second;
				}
				break;
			}
		}
		if(explicit_pack_values) break;
	}
	vector<string> bound_pack_names;
	if(pack_parameter && bound_pack_values) for(map<string, vector<string> >::const_iterator
		bound = bound_pack_values->begin(); bound != bound_pack_values->end(); ++bound) {
		if(bound->first.empty()) continue;
		for(size_t position = pattern.find(bound->first); position != string::npos;
			position = pattern.find(bound->first, position + bound->first.size())) {
			const bool left = position == 0 || !IsIdentifierCharacter(pattern[position - 1]);
			const size_t end = position + bound->first.size();
			const bool right = end == pattern.size() || !IsIdentifierCharacter(pattern[end]);
			if(left && right) {
				bound_pack_names.push_back(bound->first);
				break;
			}
		}
	}
	size_t trailing_fixed = 0;
	if(pack_parameter) for(size_t later = parameter_position + 1;
		later < parameter_list->children.size(); ++later)
		if(parameter_list->children[later] && parameter_list->children[later]->kind ==
			"parameter-declaration" && !IsFunctionParameterPack(parameter_list->children[later])) ++trailing_fixed;
	const size_t pack_count = pack_parameter && arguments->children.size() >=
		*argument_index + trailing_fixed ? arguments->children.size() - *argument_index - trailing_fixed : 0;
	size_t bound_pack_count = pack_count;
	if(pack_parameter && !bound_pack_names.empty() && bound_pack_values) {
		map<string, vector<string> >::const_iterator bound = bound_pack_values->find(
			bound_pack_names[0]);
		if(bound != bound_pack_values->end()) bound_pack_count = bound->second.size();
	}
	const size_t visits = pack_parameter ? bound_pack_count :
		(*argument_index < arguments->children.size() ? 1 : 0);
	if(explicit_pack_values && visits != explicit_pack_values->size()) return false;
	for(size_t visit = 0; visit < visits; ++visit) {
		if(*argument_index >= arguments->children.size()) return false;
		map<string, string> parameter_substitutions = substitutions;
		for(size_t bound = 0; bound < bound_pack_names.size(); ++bound) {
			const vector<string>& values = bound_pack_values->find(
				bound_pack_names[bound])->second;
			if(visit >= values.size()) return false;
			parameter_substitutions[bound_pack_names[bound]] = values[visit];
		}
		string type;
		const CPPGMAstNodePtr argument = arguments->children[*argument_index];
		FunctionSignature signature;
		bool inferred_argument = InferArgument(argument, &type, parameter_substitutions,
			context, &signature);
		if(inferred_argument && signature.result_specifiers && signature.parameters &&
			pattern.size() > 2 && pattern.compare(pattern.size() - 2, 2, "&&") == 0 &&
			IsLvalueTemplateArgument(argument))
			signature.lvalue_argument = true;
		const bool enumerator_prvalue = argument && argument->kind == "id-expression" &&
			enumerator_types_.find(RemoveMarker(argument->value)) != enumerator_types_.end();
		if(!pack_parameter && !pattern.empty() && pattern[pattern.size() - 1] == '&' &&
			(pattern.size() < 2 || pattern[pattern.size() - 2] != '&') &&
			!ConstReferenceParameterPattern(pattern) &&
			(!IsLvalueTemplateArgument(argument) || enumerator_prvalue) &&
			(!inferred_argument || type.empty() || type[type.size() - 1] != '&')) return false;
		// String literals are arrays, not pointers, when the parameter preserves
		// the reference.  Keep that typed fact for deduction; the ordinary
		// non-reference path retains the pointer spelling returned by
		// InferArgument and therefore still performs array-to-pointer decay.
		if(inferred_argument && argument && argument->kind == "literal" &&
			argument->value.find('"') != string::npos &&
			ReferenceParameterPattern(pattern)) {
			const string literal_array_reference = StringLiteralArrayReferenceType(argument->value);
			if(!literal_array_reference.empty()) type = literal_array_reference;
		}
		if(inferred_argument && (deduction_pattern.empty() ||
			deduction_pattern[deduction_pattern.size() - 1] != '&'))
			while(!type.empty() && type[type.size() - 1] == '&')
				type = CanonicalSpelling(type.substr(0, type.size() - 1));
		// Array expressions undergo the standard function-parameter decay before
		// template deduction.  Keep the promoted local-class element type while
		// changing only the array transport to a pointer (`T a[N]` -> `T*`).
		if(inferred_argument && (deduction_pattern.find('*') != string::npos ||
			!ReferenceParameterPattern(deduction_pattern))) {
			const size_t array = type.rfind('[');
			if(array != string::npos && !type.empty() && type[type.size() - 1] == ']')
				type = CanonicalSpelling(type.substr(0, array) + "*");
		}
		// A parameter declared as `T const s[]` is adjusted to `T const*`
		// before function-template deduction.  A reference-to-array parameter is
		// deliberately excluded: its bound and element type are the deduction
		// pattern, not a decay opportunity.
		const size_t array_pattern = deduction_pattern.rfind('[');
		const bool reference_array = deduction_pattern.find("(&)") != string::npos ||
			deduction_pattern.find("(& ") != string::npos;
		if(array_pattern != string::npos && !reference_array) {
			deduction_pattern = CanonicalSpelling(deduction_pattern.substr(0,
				array_pattern) + "*");
			const size_t array = type.find('[');
			if(array != string::npos)
				 type = CanonicalSpelling(type.substr(0, array) + "*");
		}
		// Keep source partial-ordering patterns strict, but apply the ordinary
		// call-deduction spelling rules here.  The semantic type model commonly
		// writes promoted unsigned/long values with an explicit `int`, and a
		// pointer-to-const parameter may accept an unqualified pointer argument.
		const auto normalize_builtin = [](string spelling) {
			string prefix;
			if(spelling.compare(0, 6, "const ") == 0) {
				prefix = "const "; spelling = CanonicalSpelling(spelling.substr(6));
			} else if(spelling.compare(0, 9, "volatile ") == 0) {
				prefix = "volatile "; spelling = CanonicalSpelling(spelling.substr(9));
			}
			if(spelling == "unsigned") spelling = "unsigned int";
			else if(spelling == "short") spelling = "short int";
			else if(spelling == "unsigned short") spelling = "unsigned short int";
			else if(spelling == "long") spelling = "long int";
			else if(spelling == "unsigned long") spelling = "unsigned long int";
			else if(spelling == "long long") spelling = "long long int";
			else if(spelling == "unsigned long long") spelling = "unsigned long long int";
			return CanonicalSpelling(prefix + spelling);
		};
		const auto normalize_pointer_pointee_cv = [](string spelling) {
			if(spelling.size() > 1 && spelling[spelling.size() - 1] == '*') {
				const string before = spelling.substr(0, spelling.size() - 1);
				if(before.size() > 6 && before.compare(before.size() - 6, 6, " const") == 0)
					return CanonicalSpelling("const " + before.substr(0, before.size() - 6) + "*");
				if(before.size() > 9 && before.compare(before.size() - 9, 9, " volatile") == 0)
					return CanonicalSpelling("volatile " + before.substr(0, before.size() - 9) + "*");
			}
			return spelling;
		};
		deduction_pattern = normalize_pointer_pointee_cv(
			normalize_builtin(deduction_pattern));
		string deduction_type = normalize_pointer_pointee_cv(normalize_builtin(type));
		if(deduction_pattern.compare(0, 6, "const ") == 0 &&
			deduction_pattern.size() > 6 && deduction_pattern[deduction_pattern.size() - 1] == '*' &&
			deduction_type.compare(0, 6, "const ") != 0)
			deduction_pattern.erase(0, 6);
		else if(deduction_pattern.compare(0, 9, "volatile ") == 0 &&
			deduction_pattern.size() > 9 && deduction_pattern[deduction_pattern.size() - 1] == '*' &&
			deduction_type.compare(0, 9, "volatile ") != 0)
			deduction_pattern.erase(0, 9);
		const vector<string> function_types = deduction_pattern.find(")(") != string::npos ?
			FunctionExpressionTypes(argument, context) : vector<string>();
		if(!function_types.empty()) {
			const bool overloaded = function_types.size() > 1 && argument &&
				(argument->kind == "id-expression" || (argument->kind == "unary-expression" &&
				 RemoveMarker(argument->value) == "&"));
			if(overloaded) {
				deferred_patterns->push_back(deduction_pattern);
				deferred_arguments->push_back(argument);
				inferred_argument = false;
			} else {
				type = function_types[0];
				inferred_argument = true;
			}
		}
		// A pointer to a dependent member typedef is an expected function
		// signature, not an ordinary object type.  Its overload-set argument
		// cannot be checked until later parameters have deduced the owner of
		// that typedef (for example `equality<TypeA>::type*` before `TypeA&`).
		if(inferred_argument && signature.result_specifiers && signature.parameters &&
			pattern.find("::type") != string::npos && pattern.find('*') != string::npos) {
			deferred_patterns->push_back(deduction_pattern);
			deferred_arguments->push_back(argument);
			inferred_argument = false;
		}
		if(inferred_argument && pattern.size() > 2 && pattern.compare(pattern.size() - 2, 2, "&&") == 0 &&
			argument && (argument->kind == "id-expression" || argument->kind == "member-expression" ||
				argument->kind == "subscript-expression") && !enumerator_prvalue) {
			while(!type.empty() && type[type.size() - 1] == '&') type.erase(type.size() - 1);
			type = CanonicalSpelling(type + "&");
			// Forwarding-reference deduction changes the typed argument used for
			// merging, not just the later replay fact.  Recompute the normalized
			// spelling after adding the lvalue reference so `T&&` with an lvalue
			// deduces `T = U&`, rather than materializing a stale `T = U` call.
				deduction_type = normalize_pointer_pointee_cv(normalize_builtin(type));
		}
		if(inferred_argument && forwarding_pack_values && pack_parameter &&
			pattern.size() > 2 && pattern.compare(pattern.size() - 2, 2, "&&") == 0) {
			string forwarding_name;
			for(size_t candidate = 0; candidate < definition.parameters.size(); ++candidate) {
				const TemplateParameter& template_parameter = definition.parameters[candidate];
				if(!template_parameter.pack || !template_parameter.type ||
					template_parameter.name.empty()) continue;
				const size_t position = pattern.find(template_parameter.name);
				if(position == string::npos ||
					(position > 0 && IsIdentifierCharacter(pattern[position - 1])) ||
					(position + template_parameter.name.size() < pattern.size() &&
						IsIdentifierCharacter(pattern[position + template_parameter.name.size()]))) continue;
				forwarding_name = template_parameter.name;
				break;
			}
			if(!forwarding_name.empty())
				(*forwarding_pack_values)[forwarding_name].push_back(type);
		}
		bool null_pointer_conversion = false;
		if(inferred_argument && argument && argument->kind == "literal" &&
			Trim(RemoveMarker(argument->value)) == "0" && fixed_template_parameters &&
			!fixed_template_parameters->empty() && deduction_pattern.find('*') != string::npos) {
			map<string, string> null_pointer_substitutions = parameter_substitutions;
			for(map<string, string>::const_iterator binding = inferred->begin();
				binding != inferred->end(); ++binding)
				null_pointer_substitutions[binding->first] = binding->second;
			string null_pointer_pattern = NormalizeTypeArgument(ReplaceIdentifiers(
				deduction_pattern, null_pointer_substitutions));
			null_pointer_pattern = const_cast<PA18TemplateExpander*>(this)->RewriteText(
				null_pointer_pattern, context, null_pointer_substitutions, 0);
			null_pointer_pattern = NormalizeTypeArgument(ReplaceIdentifiers(
				null_pointer_pattern, null_pointer_substitutions));
			null_pointer_conversion = null_pointer_pattern.find('*') != string::npos &&
				!HasUnresolvedTemplateParameter(null_pointer_pattern, context,
					null_pointer_substitutions);
		}
		if(inferred_argument && explicit_pack_values) {
			// Explicit template arguments own this pack.  Ordinary deduction must
			// not append the same element a second time, but it still has to prove
			// that the call argument matches the explicitly selected element.
			map<string, string> explicit_substitutions = parameter_substitutions;
			explicit_substitutions[explicit_pack_name] = (*explicit_pack_values)[visit];
			const string explicit_pattern = NormalizeTypeArgument(ReplaceIdentifiers(
				pattern, explicit_substitutions));
			set<string> explicit_parameter_names = parameter_names;
			explicit_parameter_names.erase(explicit_pack_name);
			map<string, string> ignored;
			if(!MatchTypePattern(explicit_pattern, type, explicit_parameter_names,
				&ignored, context)) return false;
		} else if(inferred_argument && !null_pointer_conversion) {
			if(!MergeInferredFunctionArgument(definition, deduction_pattern,
				deduction_type, signature, parameter_substitutions, context, parameter_names,
				inferred, inferred_packs, inferred_functions, bound_pack_values,
				fixed_template_parameters)) return false;
		}
		++*argument_index;
	}
	return true;
}

bool PA18TemplateExpander::CompleteFunctionArguments(
	const TemplateDefinition& definition, const vector<string>& deferred_patterns,
	const vector<CPPGMAstNodePtr>& deferred_arguments, const set<string>& parameter_names,
	map<string, string>* inferred, const map<string, vector<string> >& inferred_packs,
	vector<string>* result, map<string, vector<string> >* inferred_pack_values,
	const string& context,
	const map<string, vector<string> >* forwarding_pack_values) const
{
	for(size_t deferred = 0; deferred < deferred_patterns.size(); ++deferred) {
		bool matched = false;
		map<string, string> expected_substitutions = *inferred;
		string expected_pattern = ReplaceIdentifiersPreservingPackSizes(
			deferred_patterns[deferred], expected_substitutions);
		try {
			expected_pattern = CanonicalSpelling(ResolveAlias(
				const_cast<PA18TemplateExpander*>(this)->RewriteText(
					expected_pattern, context, expected_substitutions, 0), context));
		} catch(const PA18SubstitutionFailure&) {
			expected_pattern.clear();
		}
		if(expected_pattern.find("(*") != string::npos &&
			!expected_pattern.empty() && expected_pattern[expected_pattern.size() - 1] == '*')
			expected_pattern = CanonicalSpelling(expected_pattern.substr(0,
			expected_pattern.size() - 1));
		CPPGMAstNodePtr function_argument = deferred_arguments[deferred];
		if(function_argument && function_argument->kind == "unary-expression" &&
			RemoveMarker(function_argument->value) == "&" &&
			!function_argument->children.empty())
			function_argument = function_argument->children[0];
		if(!expected_pattern.empty() && function_argument &&
			function_argument->kind == "id-expression") {
			const vector<const TemplateDefinition*> candidates =
				FindFunctionDefinitions(function_argument->value, context);
			for(size_t candidate = 0; candidate < candidates.size() && !matched; ++candidate) {
				vector<string> ignored;
				const bool viable = InferFunctionFromExpected(*candidates[candidate], expected_pattern,
					&ignored, context);
				if(viable) matched = true;
			}
		}
		if(!matched) {
			const vector<string> function_types = FunctionExpressionTypes(
				deferred_arguments[deferred], context);
			map<string, string> selected_inferred;
			for(size_t candidate = 0; candidate < function_types.size(); ++candidate) {
				map<string, string> one = *inferred;
				if(!MatchTypePattern(deferred_patterns[deferred], function_types[candidate],
					parameter_names, &one, context)) continue;
				if(!matched) {
					selected_inferred = one;
					matched = true;
				} else if(one != selected_inferred) {
					// An overload set used as a function-template argument is
					// viable only when deduction identifies one unambiguous
					// template argument set.  Accepting the first overload turns
					// a nondeduced call into an arbitrary specialization.
					return false;
				}
			}
			if(matched) *inferred = selected_inferred;
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
			} else if(inferred_pack_values) (*inferred_pack_values)[parameter.name] = vector<string>();
			continue;
		}
		map<string, string>::const_iterator found = inferred->find(parameter.name);
		if(found != inferred->end()) result->push_back(found->second);
		else if(!parameter.default_type.empty()) {
			// Defaults are ordered: a later default may depend on a parameter
			// supplied by an earlier default (`Result = Ref`).  Keep each
			// completed default in the typed deduction map so both the result
			// vector and subsequent defaults see the same concrete argument.
			const string value = NormalizeTypeArgument(ReplaceIdentifiers(
				parameter.default_type, *inferred));
			(*inferred)[parameter.name] = value;
			result->push_back(value);
		}
		else return false;
	}
	return true;
}

bool PA18TemplateExpander::InferFunctionArguments(const TemplateDefinition& definition,
	const CPPGMAstNodePtr& call, vector<string>* result,
	const map<string, string>& substitutions, const string& context,
		const vector<string>* explicit_prefix,
		map<string, vector<string> >* inferred_pack_values,
		map<string, FunctionSignature>* inferred_function_values,
		const map<string, vector<string> >* bound_pack_values,
		map<string, vector<string> >* forwarding_pack_values) const
{
	try {
	if(inferred_function_values) inferred_function_values->clear();
	if(!call || call->children.size() < 2 || !result) return false;
	const CPPGMAstNodePtr declarator = FunctionDeclarator(definition.declaration);
	const CPPGMAstNodePtr parameters = DescendantOfKind(declarator, "parameter-clause");
	const CPPGMAstNodePtr arguments = call->children[1] && call->children[1]->kind == "argument-list" ?
		call->children[1] : ChildOfKindLocal(call->children[1], "argument-list");
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
	bool only_ellipsis = !parameters->children.empty();
	for(size_t parameter = 0; parameter < parameters->children.size(); ++parameter)
		if(!parameters->children[parameter] ||
			parameters->children[parameter]->kind != "ellipsis") {
			only_ellipsis = false;
			break;
		}
	map<string, string> inferred;
	map<string, vector<string> > inferred_packs;
	set<string> fixed_template_parameters;
	set<string> parameter_names;
	vector<string> deferred_patterns;
	vector<CPPGMAstNodePtr> deferred_arguments;
	for(size_t i = 0; i < definition.parameters.size(); ++i) {
		if(!definition.parameters[i].name.empty()) parameter_names.insert(definition.parameters[i].name);
	}
	if(bound_pack_values) for(map<string, vector<string> >::const_iterator bound =
		bound_pack_values->begin(); bound != bound_pack_values->end(); ++bound)
		parameter_names.erase(bound->first);
	size_t explicit_index = 0;
	bool explicit_pack_consumed = false;
	if(explicit_prefix) for(size_t parameter = 0;
		parameter < definition.parameters.size(); ++parameter) {
		const TemplateParameter& template_parameter = definition.parameters[parameter];
		if(template_parameter.pack) {
			bool pack_precedes_fixed = false;
			for(size_t later = parameter + 1; later < definition.parameters.size(); ++later) {
				if(!definition.parameters[later].pack) {
					pack_precedes_fixed = true;
					break;
				}
			}
			if(pack_precedes_fixed || explicit_index < explicit_prefix->size()) {
				vector<string>& values = inferred_packs[template_parameter.name];
				while(explicit_index < explicit_prefix->size())
					values.push_back((*explicit_prefix)[explicit_index++]);
				if(!template_parameter.name.empty())
					fixed_template_parameters.insert(template_parameter.name);
				if(pack_precedes_fixed) explicit_pack_consumed = true;
			}
		} else if(!explicit_pack_consumed && explicit_index < explicit_prefix->size() &&
				!template_parameter.name.empty()) {
				inferred[template_parameter.name] = (*explicit_prefix)[explicit_index++];
				fixed_template_parameters.insert(template_parameter.name);
			}
	}
	if(only_ellipsis && explicit_prefix) {
		if(explicit_prefix->size() > definition.parameters.size()) return false;
		for(size_t parameter = 0; parameter < explicit_prefix->size(); ++parameter)
			result->push_back((*explicit_prefix)[parameter]);
		for(size_t parameter = explicit_prefix->size();
			parameter < definition.parameters.size(); ++parameter) {
			if(definition.parameters[parameter].default_type.empty()) return false;
			result->push_back(definition.parameters[parameter].default_type);
		}
		return result->size() == definition.parameters.size();
	}
	if(only_ellipsis)
		return CompleteFunctionArguments(definition, deferred_patterns, deferred_arguments,
			parameter_names, &inferred, inferred_packs, result, inferred_pack_values,
			context, forwarding_pack_values);
	size_t argument_index = 0;
	for(size_t i = 0; i < parameters->children.size(); ++i) {
		const bool parameter_ok = InferFunctionParameter(definition, parameters->children[i], parameters, i, arguments, &argument_index,
			substitutions, context, parameter_names, &inferred, &inferred_packs,
			&deferred_patterns, &deferred_arguments, inferred_function_values,
			bound_pack_values, forwarding_pack_values,
			fixed_template_parameters.empty() ? 0 : &fixed_template_parameters);
		if(!parameter_ok) return false;
	}
	if(argument_index != arguments->children.size()) {
		return false;
	}
	const bool complete = CompleteFunctionArguments(definition, deferred_patterns, deferred_arguments,
		parameter_names, &inferred, inferred_packs, result, inferred_pack_values, context,
		forwarding_pack_values);
	return complete;
	} catch(const PA18SubstitutionFailure&) {
		throw;
	} catch(const logic_error& error) {
		throw PA18SubstitutionFailure(error.what());
	}
}

} // namespace pa18_templates_internal
