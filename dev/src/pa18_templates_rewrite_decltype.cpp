#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

string PA18TemplateExpander::FunctionLookupContext(const string& context) const
{
	string generated_owner = active_instantiation_name_.empty() ?
		LastComponent(context) : active_instantiation_name_;
	map<string, string>::const_iterator generated_base = specialization_bases_.find(
		LastComponent(generated_owner));
	return generated_base == specialization_bases_.end() || generated_base->second.empty() ?
		context : generated_base->second;
}

bool PA18TemplateExpander::EvaluateNewExpression(const string& expression,
	const string& context, const map<string, string>& substitutions, string* result)
{
	if(!result) return false;
	size_t new_start = string::npos;
	if(expression.compare(0, 5, "::new") == 0 &&
		(expression.size() == 5 || !IsIdentifierCharacter(expression[5]))) new_start = 5;
	else if(expression.compare(0, 3, "new") == 0 &&
		(expression.size() == 3 || !IsIdentifierCharacter(expression[3]))) new_start = 3;
	if(new_start == string::npos) return false;
	string allocated = Trim(expression.substr(new_start));
	if(!allocated.empty() && allocated[0] == '(') {
		int depth = 0;
		size_t close = string::npos;
		for(size_t position = 0; position < allocated.size(); ++position) {
			if(allocated[position] == '(') ++depth;
			else if(allocated[position] == ')' && --depth == 0) {
				close = position;
				break;
			}
		}
		if(close == string::npos) return false;
		allocated = Trim(allocated.substr(close + 1));
	}
	int angle = 0;
	size_t initializer = allocated.size();
	for(size_t position = 0; position < allocated.size(); ++position) {
		const char ch = allocated[position];
		if(ch == '<' && IsTemplateAngleOpen(allocated, position)) ++angle;
		else if(ch == '>' && angle > 0 && IsTemplateAngleClose(allocated, position)) --angle;
		else if(angle == 0 && (ch == '(' || ch == '[' || ch == '{')) {
			initializer = position;
			break;
		}
	}
	allocated = Trim(allocated.substr(0, initializer));
	allocated = ResolveDecltypeTypeName(RewriteText(allocated, context, substitutions, 0),
		context, substitutions);
	if(!IsKnownTypeSpelling(allocated, context)) return false;
	string object_type = allocated;
	while(object_type.compare(0, 6, "const ") == 0)
		object_type = NormalizeTypeArgument(object_type.substr(6));
	while(object_type.compare(0, 9, "volatile ") == 0)
		object_type = NormalizeTypeArgument(object_type.substr(9));
	if(object_type == "void") return false;
	*result = NormalizeTypeArgument(allocated + "*");
	return true;
}

const TemplateDefinition* PA18TemplateExpander::FindExplicitFunctionTemplate(
	const string& base, const string& context) const
{
	const TemplateDefinition* direct = FindDefinition(base, context);
	if(direct) return direct;
	const vector<const TemplateDefinition*> visible = FindFunctionDefinitions(base, context);
	for(size_t candidate = 0; candidate < visible.size(); ++candidate)
		if(visible[candidate]->member_template) return visible[candidate];
	return visible.empty() ? 0 : visible[0];
}

bool PA18TemplateExpander::ResolveCallableTemporaryCallResult(
	const string& callee, const string& function_context, const string& context,
	const map<string, string>& substitutions, const vector<string>& actual_types,
	string* result)
{
	if(!result) return false;
	string object_type;
	if(!FunctionCallResultType(callee, function_context, substitutions, &object_type))
		return false;
	string normalized_object = NormalizeTypeArgument(ResolveAlias(
		ReplaceIdentifiers(object_type, substitutions), context));
	while(!normalized_object.empty() &&
		(normalized_object[normalized_object.size() - 1] == '&' ||
		 normalized_object[normalized_object.size() - 1] == '*'))
		normalized_object.erase(normalized_object.size() - 1);
	normalized_object = CanonicalSpelling(normalized_object);
	const CPPGMAstNodePtr declaration = FindClassDeclaration(normalized_object, context);
	if(!declaration) return false;
	for(size_t member = 0; member < declaration->children.size(); ++member) {
		const CPPGMAstNodePtr candidate = declaration->children[member];
		if(!candidate || (candidate->kind != "simple-declaration" &&
			candidate->kind != "special-member-declaration" &&
			candidate->kind != "special-member-definition")) continue;
		const string name = candidate->kind == "simple-declaration" ?
			DeclarationName(candidate) : RemoveMarker(candidate->value);
		if(name.compare(0, 8, "operator") != 0) continue;
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
	const CPPGMAstNodePtr declaration = FindClassDeclaration(callable_type, context);
	if(declaration) for(size_t member = 0; member < declaration->children.size(); ++member) {
		const CPPGMAstNodePtr candidate = declaration->children[member];
		if(!candidate || candidate->kind != "function-definition" ||
			candidate->children.size() < 2 ||
			LastComponent(FirstIdentifierLocal(candidate->children[1])) != "operator()") continue;
		*result = NormalizeTypeArgument(RewriteText(
			NodeTypeSpelling(candidate->children[0]), context, substitutions, 0));
		return !result->empty();
	}
	return false;
}

bool PA18TemplateExpander::ResolveConstructedCallResult(
	const string& callee, const string& context,
	const map<string, string>& substitutions, const vector<string>& actual_types,
	string* result)
{
	if(!result || callee.find('.') != string::npos || callee.find("->") != string::npos)
		return false;
	const string constructed = ResolveDecltypeTypeName(callee, context, substitutions);
	if(!IsKnownTypeSpelling(constructed, context)) return false;
	bool viable = actual_types.empty() && IsDefaultConstructibleType(constructed, context);
	const CPPGMAstNodePtr declaration = FindClassDeclaration(constructed, context);
	if(declaration) for(size_t member = 0;
		member < declaration->children.size() && !viable; ++member) {
		const CPPGMAstNodePtr candidate = declaration->children[member];
		if(!candidate || (candidate->kind != "special-member-declaration" &&
			candidate->kind != "special-member-definition") ||
			LastComponent(RemoveMarker(candidate->value)) != LastComponent(constructed)) continue;
		const CPPGMAstNodePtr clause = DescendantOfKind(
			FunctionDeclarator(candidate), "parameter-clause");
		if(!clause) continue;
		size_t fixed = 0, required = 0;
		bool ellipsis = false;
		for(size_t parameter = 0; parameter < clause->children.size(); ++parameter) {
			const CPPGMAstNodePtr item = clause->children[parameter];
			if(!item) continue;
			if(item->kind == "ellipsis") { ellipsis = true; break; }
			if(item->kind != "parameter-declaration") continue;
			++fixed;
			if(!ChildOfKindLocal(item, "default-argument")) ++required;
		}
		if(actual_types.size() < required || (!ellipsis && actual_types.size() > fixed))
			continue;
		viable = ellipsis;
		if(!viable) {
			size_t argument = 0;
			for(size_t parameter = 0;
				parameter < clause->children.size() && argument < actual_types.size();
				++parameter) {
				const CPPGMAstNodePtr item = clause->children[parameter];
				if(!item || item->kind != "parameter-declaration") continue;
				if(!FunctionArgumentViable(ParameterTypeSpelling(item),
					actual_types[argument++], context)) { viable = false; break; }
				viable = true;
			}
		}
	}
	if(!viable) return false;
	*result = NormalizeTypeArgument(constructed);
	return !result->empty();
}

string CollapseRepeatedQualifier(string raw)
{
	for(size_t position = 0; position < raw.size();) {
		if(!IsIdentifierCharacter(raw[position]) ||
			(position > 0 && IsIdentifierCharacter(raw[position - 1]))) {
			++position;
			continue;
		}
		size_t first_end = position;
		while(first_end < raw.size() && IsIdentifierCharacter(raw[first_end])) ++first_end;
		if(first_end + 2 > raw.size() || raw.compare(first_end, 2, "::") != 0) {
			position = first_end;
			continue;
		}
		const string component = raw.substr(position, first_end - position);
		const size_t second_begin = first_end + 2;
		if(raw.compare(second_begin, component.size(), component) != 0 ||
			(second_begin + component.size()) + 2 > raw.size() ||
			raw.compare(second_begin + component.size(), 2, "::") != 0) {
			position = first_end + 2;
			continue;
		}
		raw.erase(second_begin, component.size() + 2);
		position = position + component.size() + 2;
	}
	return raw;
}

size_t TopLevelScopeSeparator(const string& raw)
{
	int angle = 0;
	int parentheses = 0;
	int brackets = 0;
	size_t result = string::npos;
	for(size_t i = 0; i < raw.size(); ++i) {
		const char ch = raw[i];
		if(ch == '<' && IsTemplateAngleOpen(raw, i)) ++angle;
		else if(ch == '>' && angle > 0) --angle;
		else if(ch == '(') ++parentheses;
		else if(ch == ')' && parentheses > 0) --parentheses;
		else if(ch == '[') ++brackets;
		else if(ch == ']' && brackets > 0) --brackets;
		else if(ch == ':' && i + 1 < raw.size() && raw[i + 1] == ':' &&
			angle == 0 && parentheses == 0 && brackets == 0) {
			result = i;
			++i;
		}
	}
	return result;
}

string PA18TemplateExpander::FinishTemplateMemberType(const string& active_key,
	const map<string, vector<string> >& previous_packs, const string& value)
{
	active_pack_substitutions_ = previous_packs;
	active_template_member_types_.erase(active_key);
	string result = NormalizeTypeArgument(CollapseRepeatedQualifier(value));
	string cv_prefix;
	while(result.compare(0, 6, "const ") == 0) {
		cv_prefix += "const ";
		result = NormalizeTypeArgument(result.substr(6));
	}
	while(result.compare(0, 9, "volatile ") == 0) {
		cv_prefix += "volatile ";
		result = NormalizeTypeArgument(result.substr(9));
	}
	string suffix;
	while(!result.empty() && (result[result.size() - 1] == '&' ||
		result[result.size() - 1] == '*')) {
		suffix = result[result.size() - 1] + suffix;
		result.erase(result.size() - 1);
	}
	result = NormalizeTypeArgument(result);
	map<string, string>::const_iterator generated = specialization_bases_.find(
		LastComponent(result));
	if(result.find("::") == string::npos && generated != specialization_bases_.end()) {
		const string generated_owner = PrefixComponent(generated->second);
		if(!generated_owner.empty()) result = generated_owner + "::" + result;
	}
	return NormalizeTypeArgument(cv_prefix + result + suffix);
}

void PA18TemplateExpander::PrepareTemplateMemberSubstitutions(
	const TemplateDefinition& definition, const vector<string>& arguments,
	const string& context, map<string, string>* local)
{
	if(!local) return;
	for(size_t i = 0; i < definition.parameters.size() && i < arguments.size(); ++i)
		if(!definition.parameters[i].name.empty())
			(*local)[definition.parameters[i].name] = arguments[i];
	// `arguments` is flattened, so a primary parameter pack cannot be
	// represented by the scalar substitution above.  Template member lookup
	// rewrites the base specialization before TransformInstantiatedNode has a
	// chance to install its pack map; install the same typed collection here so
	// `Base<Args...>::member` preserves every argument.
	size_t argument_index = 0;
	for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter) {
		const TemplateParameter& detail = definition.parameters[parameter];
		if(detail.pack) {
			size_t trailing_fixed = 0;
			for(size_t later = parameter + 1; later < definition.parameters.size(); ++later)
				if(!definition.parameters[later].pack) ++trailing_fixed;
			const size_t available = arguments.size() > argument_index ?
				arguments.size() - argument_index : 0;
			const size_t count = available > trailing_fixed ? available - trailing_fixed : 0;
			vector<string> values;
			for(size_t value = 0; value < count; ++value)
				values.push_back(arguments[argument_index + value]);
			if(!detail.name.empty()) {
				active_pack_substitutions_[detail.name] = values;
				if(values.empty()) local->erase(detail.name);
				else (*local)[detail.name] = values[0];
			}
			argument_index += count;
		} else {
			if(argument_index < arguments.size() && !detail.name.empty())
				(*local)[detail.name] = arguments[argument_index];
			++argument_index;
		}
	}
	if(definition.partial_specialization) {
		map<string, string> specialized;
		if(MatchClassSpecializationPattern(definition, arguments, &specialized, context)) {
			for(map<string, string>::const_iterator binding = specialized.begin();
				binding != specialized.end(); ++binding) (*local)[binding->first] = binding->second;
			for(size_t pack = 0; pack < definition.specialization_pack_names.size(); ++pack) {
				const string& name = definition.specialization_pack_names[pack];
				map<string, string>::const_iterator binding = specialized.find(name);
				vector<string> values;
				if(binding != specialized.end() && !binding->second.empty())
					values = SplitTemplateArguments(binding->second);
				if(!name.empty()) {
					active_pack_substitutions_[name] = values;
					if(values.empty()) local->erase(name);
					else (*local)[name] = values[0];
				}
			}
		}
	}
	map<string, vector<string> >::const_iterator generated_names =
		specialization_names_by_base_.find(LastComponent(definition.qualified_name));
	if(generated_names == specialization_names_by_base_.end()) return;
	for(size_t generated_index = 0; generated_index < generated_names->second.size();
		++generated_index) {
		const string& generated_name = generated_names->second[generated_index];
		map<string, string>::const_iterator generated_base = specialization_bases_.find(generated_name);
		map<string, vector<string> >::const_iterator generated =
			specialization_arguments_.find(generated_name);
		if(generated_base == specialization_bases_.end() || generated == specialization_arguments_.end() ||
			generated->second.size() != arguments.size()) continue;
		bool same_arguments = true;
		for(size_t argument = 0; argument < arguments.size(); ++argument)
			if(NormalizeTypeArgument(CanonicalSpelling(generated->second[argument])) !=
				NormalizeTypeArgument(CanonicalSpelling(arguments[argument]))) {
				same_arguments = false;
				break;
			}
		if(same_arguments) {
			(*local)[definition.name] = generated_name;
			break;
		}
	}
}

string PA18TemplateExpander::RewriteTemplateMemberSpelling(
	const TemplateDefinition& definition, const vector<string>& arguments, string spelling,
	const string& context, const map<string, string>& local)
{
	map<string, string> pack_counts;
	size_t argument_index = 0;
	for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter) {
		if(!definition.parameters[parameter].pack) {
			if(argument_index < arguments.size()) ++argument_index;
			continue;
		}
		size_t trailing_fixed = 0;
		for(size_t later = parameter + 1; later < definition.parameters.size(); ++later)
			if(!definition.parameters[later].pack) ++trailing_fixed;
		const size_t available = arguments.size() > argument_index ?
			arguments.size() - argument_index : 0;
		const size_t count_value = available > trailing_fixed ? available - trailing_fixed : 0;
		ostringstream count_stream;
		count_stream << count_value;
		if(!definition.parameters[parameter].name.empty())
			pack_counts[definition.parameters[parameter].name] = count_stream.str();
		argument_index += count_value;
	}
	for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter) {
		if(!definition.parameters[parameter].pack) continue;
		if(definition.parameters[parameter].name.empty()) continue;
		const string token = "sizeof...(" + definition.parameters[parameter].name + ")";
		const string count = pack_counts[definition.parameters[parameter].name];
		for(size_t position = spelling.find(token); position != string::npos;
			position = spelling.find(token, position + count.size()))
			spelling.replace(position, token.size(), count);
	}
	spelling = RewriteText(spelling, context, local, 0);
	for(size_t open = spelling.find('['); open != string::npos;) {
		const size_t close = spelling.find(']', open + 1);
		if(close == string::npos) break;
		const string bound = CanonicalSpelling(spelling.substr(open + 1, close - open - 1));
		PA19IntegralValue bound_value;
		if(!bound.empty() && EvaluateIntegralText(bound, context, local, &bound_value)) {
				const string replacement = IntegralValueSpelling(bound_value);
			spelling.replace(open + 1, close - open - 1, replacement);
			open += replacement.size() + 2;
		} else open = spelling.find('[', close + 1);
	}
	return NormalizeTypeArgument(ResolveAlias(ReplaceIdentifiers(spelling, local), context));
}

bool PA18TemplateExpander::FindDirectTemplateMemberType(
	const TemplateDefinition& definition, const vector<string>& arguments, const string& member,
	const string& context, map<string, string>* local, string* result)
{
	if(!local || !result || !definition.declaration) return false;
	bool has_direct_member = false;
	for(size_t i = 0; i < definition.declaration->children.size(); ++i) {
		const CPPGMAstNodePtr child = definition.declaration->children[i];
		if(!child) continue;
		if(child->kind == "alias-declaration" && child->value == member) {
			has_direct_member = true;
			break;
		}
		if(child->kind != "simple-declaration" || child->children.empty() ||
			SpellNode(child->children[0]).find("typedef") == string::npos) continue;
		const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
		if(!list) continue;
		for(size_t j = 0; j < list->children.size(); ++j) {
			const CPPGMAstNodePtr item = list->children[j];
			if(item && !item->children.empty() &&
				LastComponent(FirstIdentifierLocal(item->children[0])) == member) {
				has_direct_member = true;
				break;
			}
		}
		if(has_direct_member) break;
	}
	if(!has_direct_member) return false;
	for(size_t i = 0; i < definition.declaration->children.size(); ++i) {
		const CPPGMAstNodePtr child = definition.declaration->children[i];
		if(!child) continue;
		if(child->kind == "alias-declaration" && !child->children.empty()) {
			const string spelling = RewriteTemplateMemberSpelling(definition, arguments,
				TypeIdSpelling(child->children[0]), context, *local);
			(*local)[child->value] = spelling;
			if(child->value == member) {
				*result = spelling;
				return true;
			}
			continue;
		}
		if(child->kind != "simple-declaration" || child->children.empty() ||
			SpellNode(child->children[0]).find("typedef") == string::npos) continue;
		const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
		if(!list) continue;
		for(size_t j = 0; j < list->children.size(); ++j) {
			const CPPGMAstNodePtr item = list->children[j];
			if(!item || item->children.empty() ||
				LastComponent(FirstIdentifierLocal(item->children[0])).empty()) continue;
			const string name = LastComponent(FirstIdentifierLocal(item->children[0]));
			const string spelling = RewriteTemplateMemberSpelling(definition, arguments,
				DeclaratorTypeSpelling(NodeTypeSpelling(child->children[0]),
					item->children[0]), context, *local);
			(*local)[name] = spelling;
			if(name == member) {
				*result = spelling;
				return true;
			}
		}
	}
	return false;
}

bool PA18TemplateExpander::FindInheritedTemplateMemberType(
	const TemplateDefinition& definition, const string& member, const string& context,
	const map<string, string>& local, string* result)
{
	if(!result || !definition.declaration) return false;
	for(size_t child_index = 0; child_index < definition.declaration->children.size(); ++child_index) {
		const CPPGMAstNodePtr clause = definition.declaration->children[child_index];
		if(!clause || clause->kind != "base-clause") continue;
		for(size_t base_index = 0; base_index < clause->children.size(); ++base_index) {
			const CPPGMAstNodePtr base_name = ChildOfKindLocal(clause->children[base_index], "base-name");
			if(!base_name) continue;
			// Keep a pack marker intact until the base argument list has been
			// split.  Replacing `B` in `Base<B...>` first turns it into
			// `Base<first...>`, losing the collection needed by inherited member
			// lookup.
			const string source_base = CanonicalSpelling(base_name->value);
			const size_t source_open = source_base.find('<');
			string base_spelling = source_open == string::npos ?
				CanonicalSpelling(ReplaceIdentifiers(source_base, local)) : source_base;
			const size_t open = base_spelling.find('<');
			if(open == string::npos) continue;
			string argument_text;
			size_t close = string::npos;
			if(!TemplateRange(base_spelling, open, &argument_text, &close)) continue;
			const string base_name_spelling = CanonicalSpelling(ReplaceIdentifiers(
				base_spelling.substr(0, open), local));
			const TemplateDefinition* base_definition = FindDefinition(base_name_spelling, definition.owner);
			if(!base_definition || !base_definition->class_template) continue;
			const vector<string> raw_base_arguments = SplitTemplateArguments(argument_text);
			vector<string> base_arguments;
			for(size_t raw_argument = 0; raw_argument < raw_base_arguments.size(); ++raw_argument) {
				const string source_argument = CanonicalSpelling(raw_base_arguments[raw_argument]);
				if(source_argument.size() > 3 &&
					source_argument.compare(source_argument.size() - 3, 3, "...") == 0) {
					const string prefix = source_argument.substr(0, source_argument.size() - 3);
					string pack_name;
					for(size_t character = 0; character < prefix.size();) {
						if(!IsIdentifierCharacter(prefix[character])) { ++character; continue; }
						const size_t begin = character;
						while(character < prefix.size() && IsIdentifierCharacter(prefix[character])) ++character;
						const string word = prefix.substr(begin, character - begin);
						if(active_pack_substitutions_.find(word) != active_pack_substitutions_.end()) {
							pack_name = word;
							break;
						}
					}
					map<string, vector<string> >::const_iterator pack =
						active_pack_substitutions_.find(pack_name);
					if(pack != active_pack_substitutions_.end()) {
						for(size_t element = 0; element < pack->second.size(); ++element) {
							map<string, string> one = local;
							one[pack_name] = pack->second[element];
							base_arguments.push_back(CollapseReferenceSpelling(
								ReplaceIdentifiers(prefix, one)));
						}
						continue;
					}
				}
				base_arguments.push_back(raw_base_arguments[raw_argument]);
			}
			for(size_t argument = 0; argument < base_arguments.size(); ++argument) {
				base_arguments[argument] = NormalizeTypeArgument(RewriteText(
					base_arguments[argument], context, local, 0, false, false));
				base_arguments[argument] = NormalizeTypeArgument(ReplaceIdentifiers(
					base_arguments[argument], local));
				const bool template_entity = argument < base_definition->parameters.size() &&
					base_definition->parameters[argument].template_template;
				if(!template_entity)
					base_arguments[argument] = ResolveAlias(base_arguments[argument], context);
				if(argument < base_definition->parameters.size() &&
					base_definition->parameters[argument].type)
					base_arguments[argument] = QualifyTypeArgument(base_arguments[argument],
						context, base_definition->owner);
			}
			const TemplateDefinition* selected = SelectClassTemplateDefinition(
				base_definition, base_arguments, context);
			if(!selected) continue;
			const string generated = Instantiate(*selected, base_arguments, context);
			const string generated_path = JoinPath(selected->owner, generated);
			set<string> active;
			if(FindClassMemberType(generated_path, member, map<string, string>(), context,
				result, &active, true)) return true;
		}
	}
	return false;
}

bool PA18TemplateExpander::RewriteConcreteNestedMember(
	string* raw, size_t begin, size_t close, const string& base, const string& context,
	const map<string, string>& substitutions, bool* template_replaced, size_t* search)
{
	if(!raw || close + 2 >= raw->size() || raw->compare(close + 1, 2, "::") != 0) {
		return false;
	}
	size_t concrete_member_end = close + 3;
	while(concrete_member_end < raw->size() &&
		IsIdentifierCharacter((*raw)[concrete_member_end])) ++concrete_member_end;
	const string nested_member = raw->substr(close + 3,
		concrete_member_end - (close + 3));
	string owner_spelling = base;
	string nested_name;
	const size_t owner_separator = base.rfind("::");
	if(owner_separator != string::npos && base.find('<') == string::npos) {
		nested_name = base.substr(owner_separator + 2);
		owner_spelling = ResolveAlias(base.substr(0, owner_separator), context);
	}
	// TemplateBase returns only the identifier before the current argument list.
	// For `Owner<T>::template Nested<U>::member`, the current owner spelling is
	// therefore reconstructed from the range being rewritten before looking up
	// its nested definition.
	if(owner_spelling.find('<') == string::npos && begin < raw->size() && close < raw->size())
		owner_spelling = raw->substr(begin, close - begin + 1);
	const size_t owner_open = owner_spelling.find('<');
	string owner_arguments_text;
	size_t owner_close = string::npos;
	if(owner_open == string::npos || !TemplateRange(owner_spelling, owner_open,
		&owner_arguments_text, &owner_close)) {
		return false;
	}
	const string owner_base = owner_spelling.substr(0, owner_open);
	if(nested_name.empty() && owner_close + 2 < owner_spelling.size() &&
		owner_spelling.compare(owner_close + 1, 2, "::") == 0)
		nested_name = owner_spelling.substr(owner_close + 3);
	size_t nested_qualifier_start = close + 3;
	while(nested_qualifier_start < raw->size() && isspace(
		static_cast<unsigned char>((*raw)[nested_qualifier_start]))) ++nested_qualifier_start;
	const bool has_nested_template = raw->compare(nested_qualifier_start, 8,
		"template") == 0 && (nested_qualifier_start + 8 == raw->size() ||
		!IsIdentifierCharacter((*raw)[nested_qualifier_start + 8]));
	const bool has_plain_nested_template = concrete_member_end < raw->size() &&
		(*raw)[concrete_member_end] == '<';
	const bool nested_template_id = has_nested_template || has_plain_nested_template;
	if(nested_name.empty() && !nested_template_id)
		nested_name = nested_member;
	const TemplateDefinition* owner_definition = FindDefinition(owner_base, context);
	if(!owner_definition || !owner_definition->class_template ||
		(!nested_template_id && nested_name.empty()) || nested_member.empty()) return false;
	vector<string> owner_arguments = SplitTemplateArguments(owner_arguments_text);
	for(size_t owner_argument = 0; owner_argument < owner_arguments.size(); ++owner_argument) {
		owner_arguments[owner_argument] = NormalizeTypeArgument(RewriteText(
			owner_arguments[owner_argument], context, substitutions, 0, false, false));
		owner_arguments[owner_argument] = NormalizeTypeArgument(ReplaceIdentifiers(
			owner_arguments[owner_argument], substitutions));
		const bool template_entity = owner_argument < owner_definition->parameters.size() &&
			owner_definition->parameters[owner_argument].template_template;
		if(!template_entity)
			owner_arguments[owner_argument] = ResolveAlias(owner_arguments[owner_argument], context);
	}
	const TemplateDefinition* selected_owner = SelectClassTemplateDefinition(
		owner_definition, owner_arguments, context);
	if(!selected_owner) return false;
	string owner_local_name;
	for(map<string, vector<string> >::const_iterator existing =
		specialization_arguments_.begin(); existing != specialization_arguments_.end() &&
		owner_local_name.empty(); ++existing) {
		map<string, string>::const_iterator existing_base = specialization_bases_.find(
			existing->first);
		if(existing_base == specialization_bases_.end() ||
			(existing_base->second != selected_owner->qualified_name &&
			 LastComponent(existing_base->second) != LastComponent(selected_owner->qualified_name)) ||
			existing->second.size() != owner_arguments.size()) continue;
		bool same = true;
		for(size_t argument = 0; argument < owner_arguments.size(); ++argument)
			if(NormalizeTypeArgument(CanonicalSpelling(existing->second[argument])) !=
				NormalizeTypeArgument(CanonicalSpelling(owner_arguments[argument]))) {
				same = false;
				break;
			}
		if(same) owner_local_name = existing->first;
	}
	if(owner_local_name.empty()) owner_local_name = Instantiate(*selected_owner,
		owner_arguments, context);
	// A dependent nested class template has one more level than the ordinary
	// `Owner<T>::member` form handled below.  Materialize the nested
	// specialization first so the enclosing member lookup sees the concrete
	// inherited typedef.  This is the typed equivalent of resolving
	// `Cases::template case_<Tag>::proto_grammar` before replaying the outer
	// template-id that contains it.
	size_t nested_qualifier = close + 1;
	while(nested_qualifier < raw->size() && isspace(
		static_cast<unsigned char>((*raw)[nested_qualifier]))) ++nested_qualifier;
	if(nested_template_id && nested_qualifier + 1 < raw->size() &&
		raw->compare(nested_qualifier, 2, "::") == 0) {
		size_t nested_start = nested_qualifier + 2;
		while(nested_start < raw->size() && isspace(
			static_cast<unsigned char>((*raw)[nested_start]))) ++nested_start;
		if(raw->compare(nested_start, 8, "template") == 0 &&
			(nested_start + 8 == raw->size() ||
			 !IsIdentifierCharacter((*raw)[nested_start + 8]))) {
			nested_start += 8;
			while(nested_start < raw->size() && isspace(
				static_cast<unsigned char>((*raw)[nested_start]))) ++nested_start;
		}
		size_t nested_open = raw->find('<', nested_start);
		size_t nested_begin = 0;
		string nested_base;
		string nested_arguments_text;
		size_t nested_close = string::npos;
		const bool nested_base_found = nested_open != string::npos &&
			TemplateBase(*raw, nested_open, &nested_begin, &nested_base);
		const bool nested_range_found = nested_base_found && TemplateRange(*raw, nested_open,
			&nested_arguments_text, &nested_close);
		if(nested_base_found && nested_begin != nested_start) {
			nested_base = raw->substr(nested_start, nested_open - nested_start);
			nested_begin = nested_start;
		}
		if(nested_base_found && nested_begin == nested_start && nested_range_found) {
			size_t member_separator = nested_close + 1;
			while(member_separator < raw->size() && isspace(
				static_cast<unsigned char>((*raw)[member_separator]))) ++member_separator;
			if(member_separator + 1 < raw->size() &&
					raw->compare(member_separator, 2, "::") == 0) {
				size_t member_begin = member_separator + 2;
				while(member_begin < raw->size() && isspace(
					static_cast<unsigned char>((*raw)[member_begin]))) ++member_begin;
				size_t member_end = member_begin;
				while(member_end < raw->size() && IsIdentifierCharacter((*raw)[member_end]))
					++member_end;
				const string nested_member = raw->substr(member_begin,
					member_end - member_begin);
				const TemplateDefinition* nested_definition = FindNestedDefinition(
						*selected_owner, nested_base);
				if(nested_definition && !nested_member.empty()) {
					map<string, string> nested_substitutions = substitutions;
					// The nested definition is collected under the source owner
					// (`cases::cases<Char, Gram>`), while this replay starts from its
					// generated owner (`cases_char_type__grammar_char_type_`).  Carry
					// the owner specialization's typed bindings explicitly; otherwise
					// inherited members such as `case_<sequence_tag>::proto_grammar`
					// are transformed with the source spelling `Gram` still present.
					AddConcreteOwnerSubstitutions(owner_local_name, context,
						&nested_substitutions);
					for(size_t parameter = 0; parameter < selected_owner->parameters.size() &&
						parameter < owner_arguments.size(); ++parameter)
						if(!selected_owner->parameters[parameter].name.empty())
							nested_substitutions[selected_owner->parameters[parameter].name] =
								owner_arguments[parameter];
					map<string, string> owner_specialized;
					if(selected_owner->partial_specialization &&
						MatchClassSpecializationPattern(*selected_owner, owner_arguments,
							&owner_specialized, context))
						for(map<string, string>::const_iterator binding = owner_specialized.begin();
							binding != owner_specialized.end(); ++binding)
							nested_substitutions[binding->first] = binding->second;
					nested_substitutions[selected_owner->name] = owner_local_name;
					vector<string> nested_arguments = SplitTemplateArguments(
						nested_arguments_text);
					for(size_t argument = 0; argument < nested_arguments.size(); ++argument) {
						nested_arguments[argument] = NormalizeTypeArgument(RewriteText(
							nested_arguments[argument], context, nested_substitutions, 0,
							false, false));
						nested_arguments[argument] = NormalizeTypeArgument(ReplaceIdentifiers(
							nested_arguments[argument], nested_substitutions));
					}
					const TemplateDefinition* selected_nested = SelectClassTemplateDefinition(
						nested_definition, nested_arguments, context);
					if(selected_nested) {
						const string nested_owner = owner_local_name;
						const string nested_local_name = Instantiate(*selected_nested,
							nested_arguments, context, false, 0, &nested_substitutions,
							&nested_owner);
						const string concrete_nested = JoinPath(owner_local_name,
							nested_local_name);
						const bool static_member =
							nested_definition->static_members.find(nested_member) !=
								nested_definition->static_members.end() ||
							selected_nested->static_members.find(nested_member) !=
								selected_nested->static_members.end();
						if(static_member) {
							raw->replace(begin, nested_close - begin + 1,
								owner_local_name + "::" + nested_local_name);
							if(template_replaced) *template_replaced = true;
							if(search) *search = begin + owner_local_name.size() + 2 + nested_local_name.size();
							return true;
						}
						string concrete_member;
						set<string> nested_active;
						const bool concrete_found = FindClassMemberType(concrete_nested, nested_member,
							nested_substitutions, context, &concrete_member, &nested_active,
							false) && !concrete_member.empty();
						if(concrete_found) {
							const string member_context = selected_nested->qualified_name.empty() ?
								context : selected_nested->qualified_name;
							concrete_member = NormalizeTypeArgument(RewriteText(
								concrete_member, member_context, nested_substitutions, 0));
							size_t replacement_begin = begin;
							while(replacement_begin > 0 && isspace(static_cast<unsigned char>(
								(*raw)[replacement_begin - 1]))) --replacement_begin;
							if(replacement_begin >= 8 && raw->compare(replacement_begin - 8,
								8, "typename") == 0 && (replacement_begin == 8 ||
								!IsIdentifierCharacter((*raw)[replacement_begin - 9])))
								replacement_begin -= 8;
							raw->replace(replacement_begin, member_end - replacement_begin,
								concrete_member);
							if(template_replaced) *template_replaced = true;
							if(search) *search = replacement_begin + concrete_member.size();
							return true;
						}
					}
				}
			}
		}
	}
	InstantiateNestedClass(*selected_owner, owner_arguments, owner_local_name, nested_name, context);
	const string concrete_nested = JoinPath(owner_local_name, nested_name);
	map<string, string> concrete_substitutions = substitutions;
	map<string, string> owner_specialized;
	if(selected_owner->partial_specialization &&
		MatchClassSpecializationPattern(*selected_owner, owner_arguments, &owner_specialized, context))
		for(map<string, string>::const_iterator binding = owner_specialized.begin();
			binding != owner_specialized.end(); ++binding)
			concrete_substitutions[binding->first] = binding->second;
	string concrete_member;
	set<string> concrete_active;
	const bool concrete_found = FindClassMemberType(concrete_nested, nested_member,
		concrete_substitutions, context, &concrete_member, &concrete_active, true) &&
		!concrete_member.empty();
	if(!concrete_found) return false;
	const string member_context = selected_owner->qualified_name.empty() ? context :
		selected_owner->qualified_name;
	concrete_member = NormalizeTypeArgument(RewriteText(concrete_member, member_context,
		concrete_substitutions, 0));
	// `TemplateBase` points at the nested component after a dependent
	// qualifier, so `Ptr::template rebind<U>::other` would otherwise become
	// `Ptr::template standard_allocator_double_`.  The materialized member
	// type already contains the enclosing specialization's identity; consume
	// the qualifier through `template` as part of this replacement.
	size_t replacement_begin = begin;
	size_t qualifier = begin;
	while(qualifier > 0 && isspace(static_cast<unsigned char>((*raw)[qualifier - 1])))
		--qualifier;
	if(qualifier >= 8 && raw->compare(qualifier - 8, 8, "template") == 0) {
		qualifier -= 8;
		while(qualifier > 0 && isspace(static_cast<unsigned char>((*raw)[qualifier - 1])))
			--qualifier;
		if(qualifier >= 2 && raw->compare(qualifier - 2, 2, "::") == 0) {
			qualifier -= 2;
			while(qualifier > 0 && isspace(static_cast<unsigned char>((*raw)[qualifier - 1])))
				--qualifier;
			while(qualifier > 0) {
				size_t component_end = qualifier;
				while(qualifier > 0 && IsIdentifierCharacter((*raw)[qualifier - 1])) --qualifier;
				if(component_end == qualifier || qualifier < 2 ||
					raw->compare(qualifier - 2, 2, "::") != 0) break;
				qualifier -= 2;
				while(qualifier > 0 && isspace(static_cast<unsigned char>((*raw)[qualifier - 1])))
					--qualifier;
			}
			replacement_begin = qualifier;
		}
	}
	raw->replace(replacement_begin, concrete_member_end - replacement_begin, concrete_member);
	if(template_replaced) *template_replaced = true;
	if(search) *search = replacement_begin + concrete_member.size();
	return true;
}

string PA18TemplateExpander::TemplateMemberType(const TemplateDefinition& definition,
	const vector<string>& arguments, const string& member, const string& context)
{
	if(!definition.declaration) return string();
	ostringstream key_stream;
	key_stream << definition.qualified_name;
	for(size_t argument = 0; argument < arguments.size(); ++argument)
		key_stream << "|" << CanonicalSpelling(arguments[argument]);
	key_stream << "|" << member;
	const string active_key = key_stream.str();
	if(!active_template_member_types_.insert(active_key).second) return string();
	const map<string, vector<string> > previous_packs = active_pack_substitutions_;
	map<string, string> local;
	PrepareTemplateMemberSubstitutions(definition, arguments, context, &local);
	if(definition.class_template && !definition.name.empty() &&
		local.find(definition.name) == local.end()) {
		const string generated = Instantiate(definition, arguments, context);
		if(!generated.empty()) local[definition.name] = generated;
	}
	string result;
	if(!FindDirectTemplateMemberType(definition, arguments, member, context, &local, &result)) {
		map<string, string>::const_iterator found = local.find(member);
		if(found != local.end()) result = found->second;
		else FindInheritedTemplateMemberType(definition, member, context, local, &result);
	}
	const string finished = FinishTemplateMemberType(active_key, previous_packs, result);
	return finished;
}

} // namespace pa18_templates_internal
