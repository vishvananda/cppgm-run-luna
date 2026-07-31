#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

string PA18TemplateExpander::FunctionPointerAliasSpelling(const string& spelling,
	const string& context) const
{
	string result;
	const string canonical = CanonicalSpelling(spelling);
	if(canonical.empty() || canonical[canonical.size() - 1] != '*') return result;
	const string pointee = CanonicalSpelling(canonical.substr(0, canonical.size() - 1));
	const string resolved_pointee = CanonicalSpelling(ResolveAlias(pointee, context));
	string direct_result;
	vector<string> direct_parameters;
	string direct_qualifiers;
	if(SplitDirectFunctionType(resolved_pointee, &direct_result, &direct_parameters,
		&direct_qualifiers) || SplitFunctionPointerType(resolved_pointee, &direct_result,
		&direct_parameters)) result = pointee + "*";
	if(result.empty()) return result;
	string matching_alias;
	for(map<string, string>::const_iterator alias = type_aliases_.begin();
		alias != type_aliases_.end(); ++alias) {
		const string name = LastComponent(alias->first);
		if(name.empty() || CanonicalSpelling(ResolveAlias(name, context)) != resolved_pointee)
			continue;
		if(!matching_alias.empty() && matching_alias != name) return result;
		matching_alias = name;
	}
	return matching_alias.empty() ? result : matching_alias + "*";
}

bool PA18TemplateExpander::IsValidFunctionAddressTemplateArgument(
	const string& raw, const string& expected, const string& context,
	const map<string, string>& substitutions) const
{
	string address = CanonicalSpelling(ReplaceIdentifiers(raw, substitutions));
	if(address.size() < 4 || address[0] != '&') return false;
	const size_t separator = address.rfind("::");
	if(separator == string::npos || separator + 2 >= address.size()) return false;
	string owner = CanonicalSpelling(ResolveAlias(address.substr(1, separator - 1), context));
	const string member = LastComponent(address.substr(separator + 2));
	map<string, string>::const_iterator owner_base = specialization_bases_.find(LastComponent(owner));
	map<string, vector<string> >::const_iterator owner_arguments =
		specialization_arguments_.find(LastComponent(owner));
	string expected_result;
	vector<string> expected_parameters;
	if(!SplitFunctionPointerType(expected, &expected_result, &expected_parameters)) return false;
	const vector<const TemplateDefinition*> candidates = FindFunctionDefinitions(member, owner);
	for(size_t candidate = 0; candidate < candidates.size(); ++candidate) {
		if(!candidates[candidate] || candidates[candidate]->class_template) continue;
		vector<string> arguments;
		string source_expected = RestoreSpecializationSpelling(expected);
		const CPPGMAstNodePtr candidate_declarator = FunctionDeclarator(
			candidates[candidate]->declaration);
		if(owner_base != specialization_bases_.end() &&
			owner_arguments != specialization_arguments_.end()) {
			string source_owner = owner_base->second + "<";
			for(size_t argument = 0; argument < owner_arguments->second.size(); ++argument) {
				if(argument) source_owner += ", ";
				source_owner += RestoreSpecializationSpelling(owner_arguments->second[argument]);
			}
			source_owner += ">";
			string candidate_result;
			vector<string> candidate_parameters;
			if(candidate_declarator && !candidates[candidate]->declaration->children.empty()) {
				candidate_result = NodeTypeSpelling(candidates[candidate]->declaration->children[0]) +
					DeclaratorSuffix(candidate_declarator);
				const CPPGMAstNodePtr clause = DescendantOfKind(candidate_declarator,
					"parameter-clause");
				if(clause) for(size_t parameter = 0; parameter < clause->children.size(); ++parameter)
					if(clause->children[parameter] && clause->children[parameter]->kind ==
						"parameter-declaration") candidate_parameters.push_back(
						ParameterTypeSpelling(clause->children[parameter]));
			}
			if(CanonicalSpelling(candidate_result) == LastComponent(owner_base->second))
				candidate_result = source_owner;
			set<string> parameter_names;
			for(size_t parameter = 0; parameter < candidates[candidate]->parameters.size(); ++parameter)
				if(!candidates[candidate]->parameters[parameter].name.empty())
					parameter_names.insert(candidates[candidate]->parameters[parameter].name);
			map<string, string> inferred;
			string source_result;
			vector<string> source_parameters;
			if(SplitFunctionPointerType(source_expected, &source_result, &source_parameters) &&
				candidate_parameters.size() == source_parameters.size() &&
				MatchTypePattern(candidate_result, source_result, parameter_names, &inferred, context)) {
				bool matched = true;
				for(size_t parameter = 0; parameter < candidate_parameters.size(); ++parameter)
					if(!MatchTypePattern(candidate_parameters[parameter], source_parameters[parameter],
						parameter_names, &inferred, context)) {
						matched = false;
						break;
					}
				if(matched) for(size_t parameter = 0; parameter < candidates[candidate]->parameters.size(); ++parameter) {
					const TemplateParameter& formal = candidates[candidate]->parameters[parameter];
					if(formal.name.empty() || inferred.find(formal.name) != inferred.end() ||
						!formal.default_type.empty()) continue;
					matched = false;
					break;
				}
				if(matched) return true;
			}
		}
		if((InferFunctionFromExpected(*candidates[candidate], expected, &arguments, context) ||
			InferFunctionFromExpected(*candidates[candidate], source_expected, &arguments, context))) {
			return true;
		}
	}
	return false;
}

bool PA18TemplateExpander::ValidateTemplateDefaults(
	const TemplateDefinition& definition, const vector<string>& arguments,
	const string& context, const map<string, string>& substitutions)
{
	map<string, string> local = substitutions;
	vector<bool> defaulted;
	map<string, vector<string> > packs;
	size_t argument = 0;
	for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter) {
		const TemplateParameter& item = definition.parameters[parameter];
		if(item.pack) {
			size_t trailing_fixed = 0;
			for(size_t later = parameter + 1; later < definition.parameters.size(); ++later)
				if(!definition.parameters[later].pack) ++trailing_fixed;
			const size_t available = arguments.size() > argument ?
				arguments.size() - argument : 0;
			const size_t count = available > trailing_fixed ? available - trailing_fixed : 0;
			for(size_t value = 0; value < count; ++value)
				packs[item.name].push_back(arguments[argument++]);
			if(!item.name.empty() && !packs[item.name].empty())
				local[item.name] = packs[item.name][0];
			continue;
		}
		if(argument >= arguments.size()) continue;
		const string expected_default = CanonicalSpelling(ReplaceIdentifiers(
			item.default_type, local));
		const bool is_default = !item.default_type.empty() &&
			CanonicalSpelling(arguments[argument]) == expected_default;
		defaulted.push_back(is_default);
		if(!item.name.empty()) local[item.name] = arguments[argument];
		++argument;
	}
	const map<string, vector<string> > previous_packs = active_pack_substitutions_;
	for(map<string, vector<string> >::const_iterator pack = packs.begin();
		pack != packs.end(); ++pack)
		if(!pack->first.empty()) active_pack_substitutions_[pack->first] = pack->second;
	try {
		argument = 0;
		size_t default_index = 0;
		for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter) {
			const TemplateParameter& item = definition.parameters[parameter];
			if(item.pack) {
				size_t trailing_fixed = 0;
				for(size_t later = parameter + 1; later < definition.parameters.size(); ++later)
					if(!definition.parameters[later].pack) ++trailing_fixed;
				const size_t available = arguments.size() > argument ?
					arguments.size() - argument : 0;
				argument += available > trailing_fixed ? available - trailing_fixed : 0;
				continue;
			}
			if(argument >= arguments.size()) continue;
			if(default_index < defaulted.size() && defaulted[default_index]) {
				// Keep a function-pack operand intact while checking a defaulted
					// enable-if.  The scalar binding stored in `local` is only the
					// first pack element; replacing `Args` in `Args&&...` here would
					// turn the whole condition into `first&&&...` and discard the
					// typed pack before the evaluator can expand it.
				string declared = CanonicalSpelling(ReplaceIdentifiersPreservingPackSizes(
					item.non_type_type, local));
				size_t open = declared.find('<');
				const bool logical_non_type = !item.type &&
					(declared.find("&&") != string::npos || declared.find("||") != string::npos);
				if(logical_non_type && open != string::npos) {
					string base, argument_text; size_t begin = 0, close = string::npos;
					if(TemplateBase(declared, open, &begin, &base) &&
						TemplateRange(declared, open, &argument_text, &close) &&
						(close + 7 == declared.size() && declared.compare(close + 1, 6, "::type") == 0) &&
						(LastComponent(base) == "enable_if" ||
							LastComponent(base) == "enable_if_t")) {
						const vector<string> condition = SplitTemplateArguments(argument_text);
						PA19IntegralValue enabled;
						if(!condition.empty() && EvaluateIntegralText(condition[0], context,
							local, &enabled) && enabled.known && PA19Raw(enabled) == 0) {
							active_pack_substitutions_ = previous_packs;
							return false;
						}
					}
				}
				if(item.type) RewriteText(arguments[argument], context, local, 0);
				else if(logical_non_type) {
					PA19IntegralValue value;
					ResolveIntegralArgument(item, arguments[argument], context, local, &value);
				}
			}
			if(!item.name.empty()) local[item.name] = arguments[argument];
			++argument;
			++default_index;
		}
		} catch(const PA18SubstitutionFailure&) {
		active_pack_substitutions_ = previous_packs;
		return false;
	} catch(const logic_error&) {
		active_pack_substitutions_ = previous_packs;
		return false;
	}
	active_pack_substitutions_ = previous_packs;
	return true;
}

bool PA18TemplateExpander::ResolvePointerOrReferenceArgument(
	const TemplateParameter& parameter, const string& raw, const string& context,
	const map<string, string>& substitutions, string* result) const
{
	if(!result) return false;
	string declared_type = CanonicalSpelling(ReplaceIdentifiers(
		parameter.non_type_type, substitutions));
	declared_type = CanonicalSpelling(ResolveAlias(declared_type, context));
	string function_result, function_qualifiers;
	vector<string> function_parameters;
	const bool direct_function_parameter = SplitDirectFunctionType(declared_type,
		&function_result, &function_parameters, &function_qualifiers);
	const bool function_pointer_parameter = SplitFunctionPointerType(declared_type,
		&function_result, &function_parameters);
	string source_function_result, source_function_qualifiers;
	vector<string> source_function_parameters;
	const bool source_direct_function_parameter = SplitDirectFunctionType(
		CanonicalSpelling(parameter.non_type_type), &source_function_result,
		&source_function_parameters, &source_function_qualifiers);
	const bool source_function_pointer_parameter = SplitFunctionPointerType(
		CanonicalSpelling(parameter.non_type_type), &source_function_result,
		&source_function_parameters);
	bool pointer_or_reference = false;
	int angle = 0, parentheses = 0;
	for(size_t position = 0; position < declared_type.size(); ++position) {
		const char ch = declared_type[position];
		if(ch == '<' && IsTemplateAngleOpen(declared_type, position)) ++angle;
		else if(ch == '>' && angle > 0 && IsTemplateAngleClose(declared_type, position)) --angle;
		else if(ch == '(') ++parentheses;
		else if(ch == ')' && parentheses > 0) --parentheses;
		else if(angle == 0 && parentheses == 0 && (ch == '*' || ch == '&')) {
			pointer_or_reference = true;
			break;
		}
	}
	// A function non-type parameter is adjusted to a pointer type by the
	// language, but its source spelling may still be either `R(Args...)` or
	// `R (*)(Args...)`.  The pointer is nested in the declarator parentheses in
	// the latter form, so the top-level scan above deliberately cannot find it.
	if(source_direct_function_parameter || source_function_pointer_parameter)
		pointer_or_reference = true;
	if(!pointer_or_reference) return false;
	const bool source_explicit_pointer = parameter.non_type_type.find('*') != string::npos ||
		parameter.non_type_type.find('&') != string::npos;
	string raw_shape = CanonicalSpelling(RemoveMarker(raw));
	while(!raw_shape.empty() && raw_shape[0] == '=')
		raw_shape = CanonicalSpelling(raw_shape.substr(1));
	bool raw_identifier = !raw_shape.empty() &&
		(isalpha(static_cast<unsigned char>(raw_shape[0])) || raw_shape[0] == '_');
	for(size_t position = 1; raw_identifier && position < raw_shape.size(); ++position)
		if(!IsIdentifierCharacter(raw_shape[position])) raw_identifier = false;
	const bool raw_address_expression = raw_shape == "0" || raw_shape == "nullptr" ||
		raw_shape == "__nullptr" || (!raw_shape.empty() && raw_shape[0] == '&') ||
		raw_shape.find("static_cast<") == 0 || raw_shape.find("reinterpret_cast<") == 0 ||
		raw_shape.find("const_cast<") == 0 || raw_shape.find("dynamic_cast<") == 0 ||
		raw_shape.find("*)") != string::npos;
	const FunctionSignature* named_function = raw_identifier ?
		FindFunctionSignature(raw_shape, context) : 0;
	const bool raw_address = raw_address_expression || (raw_identifier &&
		variable_types_.find(raw_shape) != variable_types_.end() &&
		CanonicalSpelling(variable_types_.find(raw_shape)->second) != "bool") ||
		(function_pointer_parameter || direct_function_parameter) && named_function;
	if(!source_explicit_pointer && !raw_address) return false;
	string pointer_argument = raw_shape;
	try {
		const string rewritten = const_cast<PA18TemplateExpander*>(this)->RewriteText(
			pointer_argument, context, substitutions, 0);
		if(!rewritten.empty()) pointer_argument = CanonicalSpelling(rewritten);
	} catch(const PA18SubstitutionFailure&) {
	}
	if(raw_identifier) {
		map<string, string>::const_iterator qualified = variable_qualified_names_.find(raw_shape);
		if(qualified != variable_qualified_names_.end()) pointer_argument = qualified->second;
	} else if(pointer_argument.size() > 1 && pointer_argument[0] == '&' &&
		IsIdentifierCharacter(pointer_argument[1])) {
		const string target = pointer_argument.substr(1);
		map<string, string>::const_iterator qualified = variable_qualified_names_.find(target);
		if(qualified != variable_qualified_names_.end()) pointer_argument = "&" + qualified->second;
	}
	// The address operator belongs to the template argument spelling, but the
	// instantiated call body names the adjusted function-pointer value.  Keep
	// the callable identity in the AST and let PA18's typed call fact preserve
	// the required indirect address/decay sequence in PA14.
	if(pointer_argument.size() > 1 && pointer_argument[0] == '&' &&
		FindFunctionSignature(pointer_argument.substr(1), context))
		pointer_argument.erase(0, 1);
	const bool null_pointer_constant = raw_shape == "0" || raw_shape == "nullptr" ||
		raw_shape == "__nullptr";
	const bool address_constant = raw_address_expression && !null_pointer_constant;
	const bool reference_parameter = declared_type.find('&') != string::npos &&
		declared_type.find('*') == string::npos;
	if(reference_parameter ? (!raw_identifier || null_pointer_constant || address_constant) :
		(!null_pointer_constant && !address_constant && !raw_identifier))
		throw PA18SubstitutionFailure("invalid pointer or reference non-type argument: " + raw);
	*result = pointer_argument;
	return true;
}

string PA18TemplateExpander::NormalizeIntegralArgumentExpression(
	const string& raw, const string& context) const
{
	string evaluator_raw = raw;
	if(!evaluator_raw.empty() && evaluator_raw[0] == '(') {
		int depth = 0;
		size_t close = string::npos;
		for(size_t position = 0; position < evaluator_raw.size(); ++position) {
			if(evaluator_raw[position] == '(') ++depth;
			else if(evaluator_raw[position] == ')' && --depth == 0) {
				close = position;
				break;
			}
		}
		if(close != string::npos && close + 1 < evaluator_raw.size()) {
			const string cast_type = CanonicalSpelling(evaluator_raw.substr(1, close - 1));
			const bool type_like = cast_type.find("::") != string::npos ||
				IsKnownEnumType(cast_type, context);
			if(type_like) evaluator_raw = evaluator_raw.substr(close + 1);
		}
	}
	return evaluator_raw;
}

string PA18TemplateExpander::MaterializedTypeBase(string spelling) const
{
	spelling = CanonicalSpelling(spelling);
	while(spelling.compare(0, 6, "const ") == 0)
		spelling = CanonicalSpelling(spelling.substr(6));
	while(spelling.compare(0, 9, "volatile ") == 0)
		spelling = CanonicalSpelling(spelling.substr(9));
	while(!spelling.empty() && (spelling[spelling.size() - 1] == '*' ||
		spelling[spelling.size() - 1] == '&'))
		spelling = CanonicalSpelling(spelling.substr(0, spelling.size() - 1));
	while(spelling.size() > 6 && spelling.compare(spelling.size() - 6, 6, " const") == 0)
		spelling = CanonicalSpelling(spelling.substr(0, spelling.size() - 6));
	while(spelling.size() > 9 && spelling.compare(spelling.size() - 9, 9, " volatile") == 0)
		spelling = CanonicalSpelling(spelling.substr(0, spelling.size() - 9));
	if(spelling.empty() || spelling.find_first_not_of(
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != string::npos)
		return string();
	return spelling;
}

bool PA18TemplateExpander::PreservesMaterializedTypeName(const string& spelling,
	const map<string, string>& substitutions, const string& context) const
{
	const string base = MaterializedTypeBase(spelling);
	if(base.empty()) return false;
	map<string, string>::const_iterator collision = substitutions.find(base);
	if(collision == substitutions.end() || collision->second == base) return false;
	for(map<string, string>::const_iterator substitution = substitutions.begin();
		substitution != substitutions.end(); ++substitution)
		if(substitution->first != base && substitution->second == base) return true;
	return class_contexts_.find(base) != class_contexts_.end() ||
		FindClassDeclaration(base, context);
}

bool PA18TemplateExpander::IsSubstitutedTypeName(const string& spelling,
	const map<string, string>& substitutions, const string& context) const
{
	const string base = MaterializedTypeBase(spelling);
	if(base.empty() || (class_contexts_.find(base) == class_contexts_.end() &&
		!FindClassDeclaration(base, context))) return false;
	for(map<string, string>::const_iterator substitution = substitutions.begin();
		substitution != substitutions.end(); ++substitution)
		if(substitution->first != base && substitution->second == base) return true;
	return false;
}

void PA18TemplateExpander::ResolveTemplateArguments(const TemplateDefinition& definition,
	const vector<string>& raw_args, const string& context,
	vector<string>* args, vector<string>* metadata_args,
	map<string, string>* substitutions,
	map<string, PA19IntegralValue>* integral_substitutions,
	map<string, vector<string> >* pack_substitutions,
		const map<string, vector<string> >* pack_hints)
{
	size_t raw_index = 0;
	for(size_t i = 0; i < definition.parameters.size(); ++i) {
		const TemplateParameter& parameter = definition.parameters[i];
		if(parameter.pack) {
			vector<string> values;
			size_t trailing_fixed = 0;
			for(size_t later = i + 1; later < definition.parameters.size(); ++later)
				if(!definition.parameters[later].pack) ++trailing_fixed;
			const size_t available = raw_args.size() > raw_index ?
				raw_args.size() - raw_index : 0;
			size_t count = available > trailing_fixed ? available - trailing_fixed : 0;
			if(pack_hints && !parameter.name.empty()) {
				map<string, vector<string> >::const_iterator hint =
					pack_hints->find(parameter.name);
				if(hint != pack_hints->end()) count = hint->second.size();
			}
			for(size_t element = 0; element < count; ++element) {
				string argument = raw_index < raw_args.size() ? raw_args[raw_index++] : string();
				if(argument.empty() && pack_hints && !parameter.name.empty()) {
					map<string, vector<string> >::const_iterator hint = pack_hints->find(parameter.name);
					if(hint != pack_hints->end() && element < hint->second.size()) argument = hint->second[element];
				}
				if(argument.empty()) throw logic_error("missing template pack argument");
				PA19IntegralValue integral_value;
				if(parameter.template_template) {
					string normalized;
					if(!CompatibleTemplateTemplateArgument(parameter, argument, context,
						*substitutions, &normalized))
						throw logic_error("template-template argument does not match");
					argument = normalized;
				} else if(parameter.type) {
					argument = RewriteText(argument, context, *substitutions, 0);
					argument = NormalizeTypeArgument(argument);
				if(!PreservesMaterializedTypeName(argument, *substitutions, context)) argument = NormalizeTypeArgument(ReplaceIdentifiers(argument, *substitutions));
					const string function_pointer_alias = FunctionPointerAliasSpelling(argument, context);
					argument = ResolveAlias(argument, context);
					if(!PreservesMaterializedTypeName(argument, *substitutions, context)) argument = RewriteText(argument, context, *substitutions, 0);
					argument = NormalizeTypeArgument(argument);
					if(!function_pointer_alias.empty()) argument = function_pointer_alias;
				argument = QualifyTypeArgument(argument, context, definition.owner);
					if(!function_pointer_alias.empty()) argument = function_pointer_alias;
				} else {
					try {
						argument = ResolveIntegralArgument(parameter, argument, context,
							*substitutions, &integral_value);
						} catch(const PA18SubstitutionFailure& error) { throw PA18SubstitutionFailure("definition=" + definition.qualified_name + " " + error.what());
						} catch(const logic_error& error) { throw logic_error("definition=" + definition.qualified_name + " " + error.what()); }
		if(!parameter.name.empty()) (*integral_substitutions)[parameter.name] = integral_value;
		}
				if(argument.empty()) throw logic_error("missing template argument");
				values.push_back(argument);
				args->push_back(argument);
				metadata_args->push_back(TemplateArgumentMetadata(parameter, argument,
					integral_value, context, *substitutions));
			}
			if(!parameter.name.empty()) {
				if(pack_substitutions) (*pack_substitutions)[parameter.name] = values;
				if(!values.empty()) (*substitutions)[parameter.name] = values[0];
				else substitutions->erase(parameter.name);
			}
			continue;
		}
		string argument, source_type_argument;
		PA19IntegralValue integral_value; bool from_default = false;
		if(raw_index < raw_args.size() && !raw_args[raw_index].empty()) source_type_argument = argument = raw_args[raw_index++];
		else if(!parameter.name.empty()) {
			map<string, string>::const_iterator substituted = substitutions->find(parameter.name);
			if(substituted != substitutions->end()) argument = substituted->second;
			map<string, PA19IntegralValue>::const_iterator integral = integral_substitutions->find(parameter.name);
			if(argument.empty() && integral != integral_substitutions->end()) argument = TemplateIntegralValueSpelling(integral->second);
		}
		if(argument.empty()) { argument = parameter.default_type; from_default = !argument.empty(); }
		if(!parameter.default_type.empty() && argument == parameter.default_type)
			from_default = true;
		const string argument_context = from_default && !definition.owner.empty() ? definition.owner : context;
		const bool preserve_source_type = parameter.type &&
			IsSubstitutedTypeName(source_type_argument, *substitutions, context);
		if(parameter.template_template) {
			string normalized;
			if(!CompatibleTemplateTemplateArgument(parameter, argument, context,
				*substitutions, &normalized))
				throw logic_error("template-template argument does not match");
			argument = normalized;
		} else if(parameter.type) {
			argument = ExpandPackCallText(argument, *pack_substitutions);
			if(!preserve_source_type)
				argument = RewriteText(argument, context, *substitutions, 0);
			argument = NormalizeTypeArgument(argument);
			if(!PreservesMaterializedTypeName(argument, *substitutions, context))
				argument = NormalizeTypeArgument(ReplaceIdentifiers(argument, *substitutions));
			string function_pointer_alias = FunctionPointerAliasSpelling(source_type_argument, context); if(function_pointer_alias.empty()) function_pointer_alias = FunctionPointerAliasSpelling(argument, context);
				argument = ResolveAlias(argument, context);
			if(!preserve_source_type && !PreservesMaterializedTypeName(argument, *substitutions, context))
				argument = RewriteText(argument, context, *substitutions, 0);
			argument = NormalizeTypeArgument(argument);
			if(!function_pointer_alias.empty()) argument = function_pointer_alias;
			argument = QualifyTypeArgument(argument, context, definition.owner);
			if(!function_pointer_alias.empty()) argument = function_pointer_alias;
		} else {
			try {
				try {
					argument = ResolveIntegralArgument(parameter, argument, argument_context,
						*substitutions, &integral_value);
				} catch(const PA18SubstitutionFailure&) {
					if(definition.owner.empty() || definition.owner == context) throw;
					argument = ResolveIntegralArgument(parameter, argument, definition.owner,
						*substitutions, &integral_value);
				}
			} catch(const PA18SubstitutionFailure& error) { throw PA18SubstitutionFailure("definition=" + definition.qualified_name + " " + error.what());
			} catch(const logic_error& error) { throw logic_error("definition=" + definition.qualified_name + " " + error.what()); }
			if(!parameter.name.empty()) (*integral_substitutions)[parameter.name] = integral_value;
		}
		if(definition.alias_template && parameter.type && !source_type_argument.empty() && !ResolveAlias(source_type_argument, context).empty() && ResolveAlias(source_type_argument, context).back() == '&') argument = source_type_argument;
		if(argument.empty()) throw logic_error("missing template argument");
		args->push_back(argument); metadata_args->push_back(TemplateArgumentMetadata(parameter, argument,
			integral_value, context, *substitutions));
		if(!parameter.name.empty()) (*substitutions)[parameter.name] = argument; }
	if(raw_index != raw_args.size()) throw logic_error("too many template arguments");
}

} // namespace pa18_templates_internal
