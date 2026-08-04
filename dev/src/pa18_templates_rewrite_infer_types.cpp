#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
#include <functional>
using namespace std;
namespace pa18_templates_internal {

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
			string class_candidate = current;
			const string prefix = PrefixComponent(current);
			if(!prefix.empty()) class_candidate = prefix;
			string class_base = class_candidate;
			const size_t class_open = class_base.find('<');
			if(class_open != string::npos) class_base.erase(class_open);
			const TemplateDefinition* current_definition = FindDefinition(class_candidate, context);
			if(!current_definition && class_base != class_candidate)
				current_definition = FindDefinition(class_base, context);
			if(class_contexts_.find(class_candidate) != class_contexts_.end() ||
				class_contexts_.find(class_base) != class_contexts_.end() ||
				class_declarations_.find(class_candidate) != class_declarations_.end() ||
				class_declarations_.find(class_base) != class_declarations_.end() ||
				(current_definition && current_definition->class_template)) {
				object_type = class_candidate;
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
	string lookup_object_type = object_type;
	if(!substitutions.empty()) {
		const size_t object_open = object_type.find('<');
		const string object_base = object_open == string::npos ? object_type :
			object_type.substr(0, object_open);
		map<string, string>::const_iterator concrete = substitutions.find(object_base);
		if(object_open != string::npos && concrete != substitutions.end() &&
			(specialization_bases_.find(LastComponent(concrete->second)) !=
				specialization_bases_.end() || class_declarations_.find(concrete->second) !=
				class_declarations_.end())) lookup_object_type = concrete->second;
		else if(object_open != string::npos) {
			map<string, string> lookup_substitutions = substitutions;
			lookup_substitutions.erase(object_base);
			lookup_object_type = CanonicalSpelling(ReplaceIdentifiersPreservingPackSizes(
				object_type, lookup_substitutions));
		}
	}
	if(!lookup_object_type.empty() && !member.empty() && FindClassMemberType(
		lookup_object_type, member, substitutions, context, result, &active)) {
		return true;
	}
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
	// `variable_types_` is a translation-unit fallback and intentionally keeps
	// the spelling of template parameters from every collected function.  It
	// must not hide a visible namespace function when a later call is replayed
	// from another scope (`handler` in `main` can otherwise acquire the
	// unrelated `Handler&&` type of a member template parameter).  A variable in
	// an actually visible function scope still wins, as required by ordinary
	// name hiding.
	const string variable_name = LastComponent(qualified_name);
	bool scoped_variable = false;
	for(string current = context; ; ) {
		map<string, map<string, string> >::const_iterator scope =
			function_parameter_types_.find(current);
		if(scope != function_parameter_types_.end() && scope->second.find(variable_name) !=
			scope->second.end()) {
			scoped_variable = true;
			break;
		}
		if(current.empty()) break;
		const size_t separator = current.rfind("::");
		if(separator == string::npos) current.clear();
		else current.erase(separator);
	}
	// Namespace and local objects are indexed by their qualified typed name;
	// unlike the translation-unit fallback map, that fact is sufficient to
	// preserve an object imported through an enclosing namespace (including an
	// anonymous namespace).  Do not let an unrelated function signature hide
	// such an object when the name is replayed from another function.
	map<string, string>::const_iterator qualified_variable =
		variable_qualified_names_.find(variable_name);
	if(!scoped_variable && qualified_variable != variable_qualified_names_.end()) {
		const string variable_owner = PrefixComponent(qualified_variable->second);
		for(string current = context; ; ) {
			if(variable_owner == current || variable_owner == JoinPath(current, "<unnamed>")) {
				scoped_variable = true;
				break;
			}
			map<string, string>::const_iterator logical_owner =
				lexical_namespace_logical_.find(variable_owner);
			if(logical_owner != lexical_namespace_logical_.end() &&
				logical_owner->second == current) {
				scoped_variable = true;
				break;
			}
			if(current.empty()) break;
			const size_t separator = current.rfind("::");
			if(separator == string::npos) current.clear();
			else current.erase(separator);
		}
	}
	const FunctionSignature* visible_function = FindFunctionSignature(
		expression->value, context);
	if(!scoped_variable && visible_function) {
		*result = FunctionSignatureType(*visible_function);
		if(function_signature) *function_signature = *visible_function;
		return true;
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
	const FunctionSignature* signature = visible_function ? visible_function :
		FindFunctionSignature(expression->value, context);
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
	if(object_type.empty() || member.empty() || !FindClassMemberType(
		object_type, member, substitutions, context, result, &active)) return false;
	// A member function's return declarator is collected relative to its class
	// scope, so a nested class result may still be the bare spelling `executor`.
	// Deduction happens at the call site, where that name must retain its owner;
	// otherwise a constructor template receives an unrelated nominal type (or a
	// fundamental fallback) instead of `pool::executor`.
	string member_suffix;
	string member_base = *result;
	const size_t suffix_begin = member_base.find_first_of("*&[");
	if(suffix_begin != string::npos) {
		member_suffix = member_base.substr(suffix_begin);
		member_base = CanonicalSpelling(member_base.substr(0, suffix_begin));
	}
	if(member_base.find("::") == string::npos && !member_base.empty()) {
		const string qualified_member = JoinPath(object_type, member_base);
		if(FindClassDeclaration(qualified_member, context) != CPPGMAstNodePtr() ||
			class_contexts_.find(qualified_member) != class_contexts_.end())
			*result = qualified_member + member_suffix;
	}
	return true;
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
	const string& context, FunctionSignature* function_signature,
	bool this_function_argument) const
	{
	if(!expression || !result) return false;
	if(function_signature) *function_signature = FunctionSignature();
	if(!expression->inferred_type.empty()) {
		*result = expression->inferred_type;
		return true;
	}
	if(expression->kind == "lambda-expression") {
		map<const CPPGMAstNode*, string>::const_iterator closure =
			lambda_class_names_.find(expression.get());
		if(closure != lambda_class_names_.end()) {
			*result = closure->second;
			return true;
		}
		if(expression->source_token_begin != static_cast<size_t>(-1) &&
		   expression->source_token_end != static_cast<size_t>(-1)) {
			map<pair<size_t, size_t>, string>::const_iterator by_span =
				lambda_class_names_by_span_.find(make_pair(
					expression->source_token_begin, expression->source_token_end));
			if(by_span != lambda_class_names_by_span_.end()) {
				*result = by_span->second;
				return true;
			}
		}
		if(!this_function_argument) return false;
		// A captureless lambda which is used as a constructor-template argument
		// remains on PA14's function-pointer representation.  Such a lambda is
		// deliberately not assigned a synthetic closure class, but template
		// deduction still needs the callable's concrete function type.  Recover
		// the small typed signature here from its parameter list and return body;
		// the lowering pass performs the authoritative auto-return resolution when
		// it materializes the lambda function.
		const CPPGMAstNodePtr declarator = ChildOfKindLocal(expression,
			"lambda-declarator");
		const CPPGMAstNodePtr parameters = declarator ? ChildOfKindLocal(
			declarator, "parameter-clause") : CPPGMAstNodePtr();
		vector<string> parameter_types;
		if(parameters) for(size_t parameter = 0; parameter < parameters->children.size();
			++parameter) {
			const CPPGMAstNodePtr item = parameters->children[parameter];
			if(!item || item->kind != "parameter-declaration") continue;
			parameter_types.push_back(ParameterTypeSpelling(item));
		}
		string return_type;
		const CPPGMAstNodePtr trailing = declarator ? ChildOfKindLocal(declarator,
			"trailing-return-type") : CPPGMAstNodePtr();
		if(trailing && !trailing->children.empty())
			return_type = TypeIdSpelling(trailing->children[0]);
		const CPPGMAstNodePtr body = ChildOfKindLocal(expression,
			"compound-statement");
		function<string(const CPPGMAstNodePtr&)> infer_lambda_expression;
		infer_lambda_expression = [&](const CPPGMAstNodePtr& node) -> string {
			if(!node) return string();
			if(node->kind == "id-expression" && parameters) {
				const string name = LastComponent(RemoveMarker(node->value));
				for(size_t parameter = 0; parameter < parameters->children.size();
					++parameter) {
					const CPPGMAstNodePtr item = parameters->children[parameter];
					if(!item || item->kind != "parameter-declaration" ||
						item->children.size() < 2) continue;
					if(LastComponent(RemoveMarker(FirstIdentifierLocal(
						item->children[1]))) == name)
						return ParameterTypeSpelling(item);
				}
			}
			if(node->kind == "literal") return InferLiteralArgumentType(node->value);
			if(node->kind == "binary-expression" && node->children.size() >= 2) {
				const string op = RemoveMarker(node->value);
				if(op == "==" || op == "!=" || op == "<" || op == ">" ||
					op == "<=" || op == ">=" || op == "&&" || op == "||") return "bool";
				if(op == ",") return infer_lambda_expression(node->children[1]);
				string left = infer_lambda_expression(node->children[0]);
				if(!left.empty()) return left;
				return infer_lambda_expression(node->children[1]);
			}
			if(node->kind == "conditional-expression" && node->children.size() >= 3) {
				string branch = infer_lambda_expression(node->children[1]);
				if(!branch.empty()) return branch;
				return infer_lambda_expression(node->children[2]);
			}
			if(node->kind == "unary-expression" && !node->children.empty())
				return infer_lambda_expression(node->children[0]);
			string inferred;
			return InferArgument(node, &inferred, substitutions, context) ? inferred : string();
		};
		function<void(const CPPGMAstNodePtr&)> find_return;
		find_return = [&](const CPPGMAstNodePtr& node) {
			if(!node || !return_type.empty()) return;
			if(node->kind == "lambda-expression") return;
			if(node->kind == "return-statement" && !node->children.empty() &&
				node->children[0]) {
				const string inferred = infer_lambda_expression(node->children[0]);
				if(!inferred.empty()) return_type = inferred;
			}
			for(size_t child = 0; child < node->children.size(); ++child)
				find_return(node->children[child]);
		};
		if(return_type.empty()) find_return(body);
		if(return_type.empty()) return_type = "void";
		string signature = CanonicalSpelling(return_type + "(*) (");
		for(size_t parameter = 0; parameter < parameter_types.size(); ++parameter) {
			if(parameter) signature += ',';
			signature += parameter_types[parameter];
		}
		*result = CanonicalSpelling(signature + ")");
		return !result->empty();
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
					string class_candidate = current;
					const string prefix = PrefixComponent(current);
					if(!prefix.empty()) class_candidate = prefix;
					string class_base = class_candidate;
					const size_t class_open = class_base.find('<');
					if(class_open != string::npos) class_base.erase(class_open);
					const TemplateDefinition* current_definition = FindDefinition(class_candidate, context);
					if(!current_definition && class_base != class_candidate)
						current_definition = FindDefinition(class_base, context);
					if(class_contexts_.find(class_candidate) != class_contexts_.end() ||
						class_contexts_.find(class_base) != class_contexts_.end() ||
						class_declarations_.find(class_candidate) != class_declarations_.end() ||
						class_declarations_.find(class_base) != class_declarations_.end() ||
						(current_definition && current_definition->class_template)) {
						object_type = class_candidate;
						break;
					}
					const size_t separator = current.rfind("::");
					if(separator == string::npos) current.clear();
					else current.erase(separator);
				}
				if(object_type.empty()) object_type = context;
				if(!substitutions.empty()) {
					string rewritten = ReplaceIdentifiersPreservingPackSizes(object_type, substitutions);
					try {
						rewritten = const_cast<PA18TemplateExpander*>(this)->RewriteText(
							rewritten, context, substitutions, 0);
					} catch(const PA18SubstitutionFailure&) {}
					rewritten = CanonicalSpelling(ResolveAlias(rewritten, context));
					const size_t generated_open = rewritten.find('<');
					if(generated_open != string::npos &&
						specialization_bases_.find(LastComponent(rewritten.substr(0,
							generated_open))) != specialization_bases_.end())
						rewritten.erase(generated_open);
					if(!rewritten.empty()) object_type = rewritten;
				}
				*result = CanonicalSpelling(object_type +
					(this_function_argument ? "*" : string()));
				return !result->empty();
			}
			*result = "bool";
			return true;
		}
		if(expression->kind == "parenthesized-expression" &&
			!expression->children.empty())
			return InferArgument(expression->children[0], result, substitutions,
				context, function_signature, this_function_argument);
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
} // namespace
