#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

string PA18TemplateExpander::FunctionResultType(const TemplateDefinition& definition,
	const vector<string>& arguments, const string& context,
	const map<string, string>* outer_substitutions,
	const vector<string>* explicit_prefix, bool validate_immediate_return)
{
	if(!definition.declaration || definition.declaration->children.empty()) return string();
	ostringstream result_key_stream;
	result_key_stream << definition.qualified_name << "@" << definition.declaration.get()
		<< "|" << context;
	for(size_t argument = 0; argument < arguments.size(); ++argument)
		result_key_stream << "|" << CanonicalSpelling(arguments[argument]);
	const string result_key = result_key_stream.str();
	if(!active_function_results_.insert(result_key).second) return string();
	ActiveFunctionResultScope result_scope(this, result_key);
	map<string, string> local = outer_substitutions ? *outer_substitutions :
		map<string, string>();
	const map<string, vector<string> > previous_packs = active_pack_substitutions_;
	map<string, size_t> explicit_pack_counts;
	if(explicit_prefix) {
		size_t explicit_index = 0;
		bool explicit_pack_consumed = false;
		for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter) {
			const TemplateParameter& detail = definition.parameters[parameter];
			if(detail.pack) {
				bool pack_precedes_fixed = false;
				for(size_t later = parameter + 1; later < definition.parameters.size(); ++later)
					if(!definition.parameters[later].pack) {
						pack_precedes_fixed = true;
						break;
					}
				if(pack_precedes_fixed || explicit_index < explicit_prefix->size()) {
					explicit_pack_counts[detail.name] = explicit_prefix->size() - explicit_index;
					explicit_index = explicit_prefix->size();
					if(pack_precedes_fixed) explicit_pack_consumed = true;
				} else if(pack_precedes_fixed) explicit_pack_counts[detail.name] = 0;
			} else if(!explicit_pack_consumed &&
				explicit_index < explicit_prefix->size()) ++explicit_index;
		}
		if(explicit_index != explicit_prefix->size()) {
			active_pack_substitutions_ = previous_packs;
			return string();
		}
	}
	size_t argument_index = 0;
	for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter) {
		const TemplateParameter& detail = definition.parameters[parameter];
		if(detail.pack) {
			size_t count = 0;
			map<string, size_t>::const_iterator explicit_count =
				explicit_pack_counts.find(detail.name);
			if(explicit_count != explicit_pack_counts.end()) count = explicit_count->second;
			else {
				size_t trailing_fixed = 0, trailing_known_pack = 0;
				for(size_t later = parameter + 1; later < definition.parameters.size(); ++later) {
					if(!definition.parameters[later].pack) ++trailing_fixed;
					else {
						map<string, size_t>::const_iterator known = explicit_pack_counts.find(
							definition.parameters[later].name);
						if(known != explicit_pack_counts.end()) trailing_known_pack += known->second;
					}
				}
				const size_t available = arguments.size() > argument_index ?
					arguments.size() - argument_index : 0;
				const size_t reserved = trailing_fixed + trailing_known_pack;
				count = available > reserved ? available - reserved : 0;
			}
			vector<string> values;
			for(size_t value = 0; value < count; ++value)
				values.push_back(arguments[argument_index + value]);
			if(!detail.name.empty()) {
				active_pack_substitutions_[detail.name] = values;
				if(!values.empty()) local[detail.name] = values[0];
				else local.erase(detail.name);
			}
			argument_index += count;
		} else {
			if(argument_index < arguments.size() && !detail.name.empty())
				local[detail.name] = arguments[argument_index];
			if(argument_index < arguments.size()) ++argument_index;
		}
	}
	for(map<string, string>::iterator binding = local.begin(); binding != local.end(); ++binding) {
		string value = CanonicalSpelling(binding->second);
		set<string> seen;
		while(!value.empty() && seen.insert(value).second) {
			if(class_contexts_.find(value) != class_contexts_.end() ||
				FindClassDeclaration(value, context)) break;
			map<string, string>::const_iterator next = local.find(value);
			if(next == local.end() || next->first == binding->first) break;
			value = CanonicalSpelling(next->second);
		}
		binding->second = value;
	}
	const CPPGMAstNodePtr declarator = FunctionDeclarator(definition.declaration);
	const string result_context = definition.owner.empty() ? context : definition.owner;
	string result;
	const CPPGMAstNodePtr trailing_return = ChildOfKindLocal(declarator,
		"trailing-return-type");
	if(trailing_return) {
		const CPPGMAstNodePtr type_id = ChildOfKindLocal(trailing_return, "type-id");
		result = TypeIdSpelling(type_id);
	} else {
		result = NodeTypeSpelling(definition.declaration->children[0]);
		result += ReturnDeclaratorSuffix(declarator);
	}
	const auto has_disabled_enable_if = [&](const string& spelling) {
		for(size_t search = 0; search < spelling.size();) {
			const size_t marker = spelling.find("enable_if", search);
			if(marker == string::npos) break;
			if(marker > 0 && IsIdentifierCharacter(spelling[marker - 1])) {
				search = marker + 9;
				continue;
			}
			size_t open = marker;
			while(open < spelling.size() && IsIdentifierCharacter(spelling[open])) ++open;
			while(open < spelling.size() && isspace(static_cast<unsigned char>(spelling[open]))) ++open;
			if(open >= spelling.size() || spelling[open] != '<') {
				search = marker + 9;
				continue;
			}
			string base, arguments_text;
			size_t begin = 0, close = string::npos;
			if(!TemplateBase(spelling, open, &begin, &base)) {
				search = marker + 9;
				continue;
			}
			bool range_ok = TemplateRange(spelling, open, &arguments_text, &close);
			if(!range_ok) {
				int depth = 0;
				for(size_t position = open; position < spelling.size(); ++position) {
					if(spelling[position] == '<') ++depth;
					else if(spelling[position] == '>' && depth > 0 && --depth == 0) {
						arguments_text = spelling.substr(open + 1,
							position - open - 1);
						close = position;
						range_ok = true;
						break;
					}
				}
			}
			if(!range_ok) {
				search = marker + 9;
				continue;
			}
			if(LastComponent(base) == "enable_if" || LastComponent(base) == "enable_if_c" ||
				LastComponent(base) == "enable_if_t") {
				const vector<string> condition = SplitTemplateArguments(arguments_text);
				if(!condition.empty() && condition[0].find("...") == string::npos &&
					!HasUnresolvedTemplateParameter(condition[0], result_context, local)) {
					PA19IntegralValue enabled;
					if(EvaluateIntegralText(condition[0], result_context, local, &enabled) &&
						enabled.known && PA19Raw(enabled) == 0) return true;
				}
			}
			search = close == string::npos ? marker + 9 : close + 1;
		}
		return false;
	};
	if(validate_immediate_return && has_disabled_enable_if(
		ReplaceIdentifiersPreservingPackSizes(result, local))) {
		active_pack_substitutions_ = previous_packs;
		return string();
	}
	try {
		result = RewriteText(result, result_context, local, 0);
	} catch(...) {
		active_pack_substitutions_ = previous_packs;
		throw;
	}
	if(validate_immediate_return && has_disabled_enable_if(result)) {
		active_pack_substitutions_ = previous_packs;
		return string();
	}
	result = CollapseReferenceSpelling(ReplaceIdentifiers(result, local));
	result = ResolveDecltypeTypeName(result, result_context, local);
	active_pack_substitutions_ = previous_packs;
	return NormalizeTypeArgument(result);
}

bool PA18TemplateExpander::InferFunctionTypeArguments(const TemplateDefinition& definition,
	const vector<string>& actual_types, vector<string>* result,
	const map<string, string>& substitutions, const string& context,
	const vector<string>* explicit_prefix)
{
	if(!result || definition.class_template) return false;
	const CPPGMAstNodePtr declarator = FunctionDeclarator(definition.declaration);
	const CPPGMAstNodePtr parameters = DescendantOfKind(declarator, "parameter-clause");
	if(!parameters) return false;
	size_t parameter_count = 0, required_parameters = 0;
	bool has_parameter_pack = false, has_ellipsis = false;
	for(size_t i = 0; i < parameters->children.size(); ++i) {
		const CPPGMAstNodePtr parameter = parameters->children[i];
		if(!parameter) continue;
		if(parameter->kind == "ellipsis") { has_ellipsis = true; continue; }
		if(parameter->kind != "parameter-declaration") continue;
		++parameter_count;
		if(IsFunctionParameterPack(parameter)) has_parameter_pack = true;
		else if(!FunctionParameterHasDefault(definition, i)) ++required_parameters;
	}
	if(actual_types.size() < required_parameters ||
		(!has_parameter_pack && !has_ellipsis && actual_types.size() > parameter_count)) return false;
	map<string, string> inferred;
	set<string> parameter_names, pack_parameter_names;
	for(size_t i = 0; i < definition.parameters.size(); ++i) {
		if(definition.parameters[i].name.empty()) continue;
		parameter_names.insert(definition.parameters[i].name);
		if(definition.parameters[i].pack) pack_parameter_names.insert(definition.parameters[i].name);
	}
	map<string, vector<string> > inferred_packs;
	if(explicit_prefix) {
		// Explicit template arguments use the same left-to-right pack rule as
		// ordinary call deduction.  A pack before a later fixed parameter absorbs
		// the remaining explicit prefix; the later fixed parameter is then
		// deduced from the function arguments.  Counting only the later fixed
		// parameters here incorrectly binds `void` to `Initiation` in
		// `async_initiate<WaitToken, void>(...)` and drops the `Signatures` pack.
		size_t explicit_index = 0;
		bool explicit_pack_consumed = false;
		for(size_t i = 0; i < definition.parameters.size(); ++i) {
			const TemplateParameter& parameter = definition.parameters[i];
			if(parameter.pack) {
				bool pack_precedes_fixed = false;
				for(size_t later = i + 1; later < definition.parameters.size(); ++later)
					if(!definition.parameters[later].pack) {
						pack_precedes_fixed = true;
						break;
					}
				if(pack_precedes_fixed || explicit_index < explicit_prefix->size()) {
					vector<string>& values = inferred_packs[parameter.name];
					while(explicit_index < explicit_prefix->size())
						values.push_back((*explicit_prefix)[explicit_index++]);
					if(pack_precedes_fixed) explicit_pack_consumed = true;
				}
			} else if(!explicit_pack_consumed && explicit_index < explicit_prefix->size()) {
				if(!parameter.name.empty())
					inferred[parameter.name] = (*explicit_prefix)[explicit_index++];
			}
		}
		if(explicit_index != explicit_prefix->size()) return false;
	}
	size_t actual = 0;
	for(size_t i = 0; i < parameters->children.size(); ++i) {
		const CPPGMAstNodePtr parameter = parameters->children[i];
		if(!parameter || parameter->kind != "parameter-declaration") continue;
		const string pattern = ParameterTypeSpelling(parameter);
		if(IsFunctionParameterPack(parameter)) {
			size_t trailing_fixed = 0;
			for(size_t later = i + 1; later < parameters->children.size(); ++later)
				if(parameters->children[later] &&
					parameters->children[later]->kind == "parameter-declaration" &&
					!IsFunctionParameterPack(parameters->children[later])) ++trailing_fixed;
			const size_t available = actual_types.size() > actual ? actual_types.size() - actual : 0;
			const size_t visits = available > trailing_fixed ? available - trailing_fixed : 0;
			string pack_pattern = pattern;
			if(pack_pattern.size() >= 3 &&
				pack_pattern.compare(pack_pattern.size() - 3, 3, "...") == 0)
				pack_pattern.erase(pack_pattern.size() - 3);
			for(size_t visit = 0; visit < visits; ++visit) {
				map<string, string> one;
				if(!MatchTypePattern(pack_pattern, actual_types[actual + visit],
					parameter_names, &one, context)) return false;
				for(map<string, string>::const_iterator binding = one.begin();
					binding != one.end(); ++binding) {
					if(pack_parameter_names.find(binding->first) != pack_parameter_names.end())
						inferred_packs[binding->first].push_back(binding->second);
					else {
						map<string, string>::const_iterator prior = inferred.find(binding->first);
						if(prior != inferred.end() && CanonicalSpelling(ResolveAlias(
							prior->second, context)) != CanonicalSpelling(ResolveAlias(
							binding->second, context))) return false;
						inferred[binding->first] = binding->second;
					}
				}
			}
			actual += visits;
			continue;
		}
		if(actual >= actual_types.size()) break;
		bool dependent = false;
		for(size_t p = 0; p < definition.parameters.size() && !dependent; ++p) {
			const string& name = definition.parameters[p].name;
			for(size_t at = 0; at + name.size() <= pattern.size(); ++at)
				if(pattern.compare(at, name.size(), name) == 0 &&
					(at == 0 || !IsIdentifierCharacter(pattern[at - 1])) &&
					(at + name.size() == pattern.size() ||
						!IsIdentifierCharacter(pattern[at + name.size()]))) {
					dependent = true;
					break;
				}
		}
		if(dependent) {
			const string dependent_pattern = CanonicalSpelling(pattern);
			const string actual_type = CollapseReferenceSpelling(actual_types[actual]);
			if(dependent_pattern.size() > 2 &&
				dependent_pattern.compare(dependent_pattern.size() - 2, 2, "&&") == 0 &&
				!actual_type.empty() && actual_type[actual_type.size() - 1] == '&') {
				const string base = CanonicalSpelling(dependent_pattern.substr(
					0, dependent_pattern.size() - 2));
				if(parameter_names.find(base) != parameter_names.end()) inferred[base] = actual_type;
				else if(!MatchTypePattern(dependent_pattern, actual_type,
					parameter_names, &inferred, context)) return false;
			} else if(!MatchTypePattern(dependent_pattern, actual_type,
				parameter_names, &inferred, context)) return false;
		}
		++actual;
	}
	for(size_t i = 0; i < definition.parameters.size(); ++i) {
		if(definition.parameters[i].pack) {
			map<string, vector<string> >::const_iterator found = inferred_packs.find(
				definition.parameters[i].name);
			if(found != inferred_packs.end())
				result->insert(result->end(), found->second.begin(), found->second.end());
			continue;
		}
		map<string, string>::const_iterator found = inferred.find(definition.parameters[i].name);
		if(found != inferred.end()) result->push_back(found->second);
		else if(!definition.parameters[i].default_type.empty())
			result->push_back(RewriteText(definition.parameters[i].default_type,
				context, inferred, 0));
		else return false;
	}
	(void)substitutions;
	return true;
}

} // namespace pa18_templates_internal
