#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
#include <functional>
using namespace std;

namespace pa18_templates_internal {

string PA18TemplateExpander::MemberAddressExpressionType(const string& expression,
	const string& context, const map<string, string>& substitutions) const
{
	if(expression.size() <= 1 || expression[0] != '&') return string();
	const string target = Trim(expression.substr(1));
	const size_t separator = TopLevelScopeSeparator(target);
	if(separator == string::npos) return string();
	string owner = CanonicalSpelling(ReplaceIdentifiersPreservingPackSizes(
		target.substr(0, separator), substitutions));
	try {
		owner = CanonicalSpelling(ResolveAlias(const_cast<PA18TemplateExpander*>(this)->RewriteText(
			owner, context, substitutions, 0), context));
	} catch(const PA18SubstitutionFailure&) {}
	if(owner.empty()) return string();
	CPPGMAstNodePtr member(new CPPGMAstNode("id-expression",
		JoinPath(owner, target.substr(separator + 2))));
	CPPGMAstNodePtr address(new CPPGMAstNode("unary-expression", "OP_AMP:&"));
	address->children.push_back(member);
	const vector<string> candidates = FunctionExpressionTypes(address, context);
	string selected;
	for(size_t candidate = 0; candidate < candidates.size(); ++candidate) {
		string result_type, candidate_owner, qualifiers;
		vector<string> parameters;
		if(!SplitMemberPointerType(candidates[candidate], &result_type, &candidate_owner,
			&parameters, &qualifiers, 0)) continue;
		candidate_owner = CanonicalSpelling(ResolveAlias(candidate_owner, context));
		if(candidate_owner != owner) continue;
		if(!selected.empty() && selected != candidates[candidate]) return string();
		selected = candidates[candidate];
	}
	return selected;
}

bool PA18TemplateExpander::EvaluateMemberPointerStaticCast(const string& expression,
	const string& context, const map<string, string>& substitutions, string* result)
{
	string expanded = ExpandPackCallText(expression, active_pack_substitutions_);
	string cast_type, cast_operand;
	if(!SplitStaticCast(expanded, &cast_type, &cast_operand)) return false;
	string resolved = NormalizeTypeArgument(ReplaceIdentifiers(cast_type, substitutions));
	try { resolved = NormalizeTypeArgument(RewriteText(resolved, context, substitutions, 0)); }
	catch(const PA18SubstitutionFailure&) {}
	resolved = NormalizeTypeArgument(ResolveAlias(resolved, context));
	if(resolved.empty() || HasUnresolvedTemplateParameter(resolved, context, substitutions))
		return false;
	string member_result, member_owner, member_qualifiers;
	vector<string> member_parameters;
	if(!SplitMemberPointerType(resolved, &member_result, &member_owner,
		&member_parameters, &member_qualifiers, 0)) return false;
	if(result) *result = resolved;
	return true;
}

void PA18TemplateExpander::BindFunctionOwnerSubstitutions(
	const string& context, map<string, string>* local) const
{
	if(!local) return;
	string owner_context = context;
	vector<string> owner_arguments;
	const size_t owner_open = owner_context.find('<');
	if(owner_open != string::npos) {
		string owner_argument_text; size_t owner_close = string::npos;
		if(TemplateRange(owner_context, owner_open, &owner_argument_text, &owner_close)) {
			owner_arguments = SplitTemplateArguments(owner_argument_text);
			owner_context = CanonicalSpelling(owner_context.substr(0, owner_open));
		}
	} else {
		map<string, vector<string> >::const_iterator recorded_arguments =
			specialization_arguments_.find(LastComponent(owner_context));
		map<string, string>::const_iterator recorded_base =
			specialization_bases_.find(LastComponent(owner_context));
		if(recorded_arguments != specialization_arguments_.end())
			owner_arguments = recorded_arguments->second;
		if(recorded_base != specialization_bases_.end()) owner_context = recorded_base->second;
	}
	const size_t owner_scope = TopLevelScopeSeparator(owner_context);
	if(owner_scope != string::npos) owner_context.erase(owner_scope);
	const TemplateDefinition* owner_definition = FindDefinition(owner_context, context);
	if(!owner_definition) owner_definition = FindDefinition(LastComponent(owner_context), context);
	if(owner_definition && owner_definition->class_template)
		for(size_t parameter = 0; parameter < owner_definition->parameters.size() &&
			parameter < owner_arguments.size(); ++parameter)
			if(!owner_definition->parameters[parameter].name.empty() &&
				local->find(owner_definition->parameters[parameter].name) == local->end())
				(*local)[owner_definition->parameters[parameter].name] =
					ReplaceIdentifiers(owner_arguments[parameter], *local);
}

bool PA18TemplateExpander::ValidateDefaultMemberAddress(
	const string& declared, const string& context,
	const map<string, string>& substitutions) const
{
	const size_t address_begin = declared.find('&');
	if(address_begin == string::npos) return true;
	size_t address_end = address_begin + 1;
	while(address_end < declared.size() &&
		(IsIdentifierCharacter(declared[address_end]) || declared[address_end] == ':'))
		++address_end;
	const string address = declared.substr(address_begin, address_end - address_begin);
	if(address.find("::") == string::npos) return true;
	try {
		return IsValidFunctionAddressTemplateArgument(address, "T", context, substitutions);
	} catch(const PA18SubstitutionFailure&) {
		return false;
	}
}

bool PA18TemplateExpander::GeneratedFunctionCallResultType(
	const string& callee, const string& function_context,
	const map<string, string>& substitutions,
	const vector<string>& actual_types, string* result)
{
	if(!result) return false;
	string generated_ellipsis_result;
	// A dependent member call can retain the source object's dot-qualified
	// spelling after its member template has been generated. Lookup indexes
	// generated declarations by the member component itself.
	string callee_name = LastComponent(callee);
	const size_t object_separator = callee_name.rfind('.');
	if(object_separator != string::npos)
		callee_name.erase(0, object_separator + 1);
	vector<string> generated_owner_order;
	set<string> generated_owner_seen;
	const string source_context = FunctionLookupContext(function_context);
	for(map<string, string>::const_iterator substitution = substitutions.begin();
		substitution != substitutions.end(); ++substitution) {
		const string candidate = CanonicalSpelling(substitution->second);
		map<string, string>::const_iterator base = specialization_bases_.find(
			LastComponent(candidate));
		if(base == specialization_bases_.end() ||
			LastComponent(base->second) != LastComponent(source_context) ||
			generated_by_owner_.find(candidate) == generated_by_owner_.end() ||
			!generated_owner_seen.insert(candidate).second) continue;
		generated_owner_order.push_back(candidate);
	}
	const size_t preferred_owner_count = generated_owner_order.size();
	for(map<string, vector<CPPGMAstNodePtr> >::const_iterator owner =
		generated_by_owner_.begin(); owner != generated_by_owner_.end(); ++owner)
		if(generated_owner_seen.insert(owner->first).second)
			generated_owner_order.push_back(owner->first);
	for(size_t owner_index = 0; owner_index < generated_owner_order.size(); ++owner_index) {
		map<string, vector<CPPGMAstNodePtr> >::const_iterator owner =
			generated_by_owner_.find(generated_owner_order[owner_index]);
		if(owner == generated_by_owner_.end()) continue;
		for(size_t generated = 0; generated < owner->second.size(); ++generated) {
			const CPPGMAstNodePtr declaration = owner->second[generated];
			if(!declaration || (declaration->kind != "function-definition" &&
				declaration->kind != "simple-declaration") ||
				DeclarationName(declaration) != callee_name &&
				(declaration->template_primary.empty() ||
					LastComponent(declaration->template_primary) != callee_name)) continue;
			const CPPGMAstNodePtr declarator = FunctionDeclarator(declaration);
			const CPPGMAstNodePtr clause = DescendantOfKind(declarator, "parameter-clause");
			if(!declarator || !clause) continue;
			const string declaration_context = owner->first.empty() ?
				function_context : owner->first;
			bool has_ellipsis = false;
			size_t parameter_count = 0;
			bool viable = true;
			for(size_t parameter = 0; parameter < clause->children.size(); ++parameter) {
				const CPPGMAstNodePtr item = clause->children[parameter];
				if(!item) continue;
				if(item->kind == "ellipsis") {
					has_ellipsis = true;
					continue;
				}
				if(item->kind != "parameter-declaration") continue;
				if(IsAbstractObjectSpelling(ParameterTypeSpelling(item), declaration_context)) {
					viable = false;
					break;
				}
				if(parameter_count >= actual_types.size() || !FunctionArgumentViable(
					ParameterTypeSpelling(item), actual_types[parameter_count], declaration_context)) {
					viable = false;
					break;
				}
				++parameter_count;
			}
			if(!viable || (!has_ellipsis && parameter_count != actual_types.size()) ||
				(has_ellipsis && parameter_count > actual_types.size())) {
			// A generated non-ellipsis specialization can be discarded by
			// substitution (notably when its parameter would contain an abstract
			// array).  The source overload set may still provide a viable ellipsis
			// fallback that was never materialized because the rejected candidate
			// was the first lookup result.
			if(!viable && declaration && !declaration->template_primary.empty() &&
				!declaration->template_arguments.empty()) {
				const vector<const TemplateDefinition*> fallbacks =
					FindFunctionDefinitions(LastComponent(declaration->template_primary),
						declaration_context);
				for(size_t fallback = 0; fallback < fallbacks.size(); ++fallback) {
					const TemplateDefinition* source = fallbacks[fallback];
					if(!source || source->parameters.size() !=
						declaration->template_arguments.size()) continue;
					const CPPGMAstNodePtr source_clause = DescendantOfKind(
						FunctionDeclarator(source->declaration), "parameter-clause");
					bool source_ellipsis = false;
					if(source_clause) for(size_t parameter = 0;
						parameter < source_clause->children.size(); ++parameter)
						if(source_clause->children[parameter] &&
							source_clause->children[parameter]->kind == "ellipsis") {
							source_ellipsis = true;
							break;
						}
					if(!source_ellipsis) continue;
					try {
						const string fallback_result = FunctionResultType(*source,
							declaration->template_arguments, declaration_context, &substitutions);
						if(!fallback_result.empty()) {
							generated_ellipsis_result = fallback_result;
							break;
						}
					} catch(const PA18SubstitutionFailure&) {}
				}
			}
			continue;
			}
			string generated_result = NodeTypeSpelling(declaration->children.empty() ?
				CPPGMAstNodePtr() : declaration->children[0]) + DeclaratorSuffix(declarator);
			// The generated declaration's return type is written relative to its
			// lexical owner, not the call site.
			generated_result = RewriteText(generated_result, declaration_context,
				substitutions, 0);
			generated_result = NormalizeTypeArgument(ResolveAlias(
				ReplaceIdentifiers(generated_result, substitutions), declaration_context));
			if(generated_result.empty()) continue;
			if(!has_ellipsis) {
				*result = generated_result;
				return true;
			}
			if(generated_ellipsis_result.empty()) generated_ellipsis_result = generated_result;
		}
		if(preferred_owner_count && owner_index + 1 == preferred_owner_count) {
			if(generated_ellipsis_result.empty()) return false;
			*result = generated_ellipsis_result;
			return true;
		}
	}
	if(generated_ellipsis_result.empty()) return false;
	*result = generated_ellipsis_result;
	return true;
}
string PA18TemplateExpander::FunctionArgumentObjectType(string raw,
	const string& context) const
{
	// A dependent call result can retain the source template-id spelling while
	// a nondependent overload parameter has already been reduced through its
	// typedef alias.  Normalize concrete template-ids through the same typed
	// specialization table before comparing the object types.
	if(raw.find('<') != string::npos) try {
		raw = const_cast<PA18TemplateExpander*>(this)->RewriteText(raw, context,
		map<string, string>(), 0);
	} catch(const PA18SubstitutionFailure&) {
		// The caller will reject an actually unavailable operand below.
	}
	raw = CanonicalSpelling(ResolveAlias(raw, context));
	while(raw.compare(0, 6, "const ") == 0)
		raw = CanonicalSpelling(raw.substr(6));
	while(raw.compare(0, 9, "volatile ") == 0)
		raw = CanonicalSpelling(raw.substr(9));
	while(raw.size() > 6 && raw.compare(raw.size() - 6, 6, " const") == 0)
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 6));
	while(raw.size() > 9 && raw.compare(raw.size() - 9, 9, " volatile") == 0)
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 9));
	raw = CanonicalSpelling(ResolveAlias(raw, context));
	while(raw.size() >= 2 && raw.compare(raw.size() - 2, 2, "&&") == 0)
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 2));
	while(!raw.empty() && raw[raw.size() - 1] == '&')
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 1));
	// Removing a reference exposes the cv-qualifier on a spelling such as
	// `const T&`.  Normalize that qualifier after the reference transport has
	// been removed so class lookup compares the object type, not `T const`.
	while(raw.size() > 6 && raw.compare(raw.size() - 6, 6, " const") == 0)
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 6));
	while(raw.size() > 9 && raw.compare(raw.size() - 9, 9, " volatile") == 0)
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 9));
	// Resolve aliases after reference/cv transport has been removed.  A
	// parameter such as `file_info const&` cannot be looked up as an alias
	// while the suffix is still attached; leaving the alias spelling here makes
	// unrelated concrete class specializations look convertible.
	raw = CanonicalSpelling(ResolveAlias(raw, context));
	return raw;
}
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

void PA18TemplateExpander::ExpandExplicitFunctionArguments(const string& raw,
	const string& context, const map<string, string>& substitutions,
	vector<string>* result)
{
	if(!result) return;
	const vector<string> explicit_arguments = SplitTemplateArguments(raw);
	vector<string> expanded;
	for(size_t i = 0; i < explicit_arguments.size(); ++i) {
		string argument = explicit_arguments[i];
		const bool pack_expansion = argument.size() >= 3 &&
			argument.compare(argument.size() - 3, 3, "...") == 0;
		if(pack_expansion) {
			argument.erase(argument.size() - 3);
			argument = CanonicalSpelling(argument);
			const vector<string>* values = 0;
			for(map<string, vector<string> >::const_iterator pack =
				active_pack_substitutions_.begin();
				pack != active_pack_substitutions_.end() && !values; ++pack) {
				map<string, string>::const_iterator substitution = substitutions.find(pack->first);
				if(argument == pack->first || (substitution != substitutions.end() &&
					argument == CanonicalSpelling(substitution->second)) ||
					(!pack->second.empty() && argument == CanonicalSpelling(pack->second[0])))
					values = &pack->second;
			}
			for(map<string, vector<string> >::const_iterator pack =
				active_function_pack_substitutions_.begin();
				pack != active_function_pack_substitutions_.end() && !values; ++pack)
				if(argument == pack->first || (!pack->second.empty() &&
					argument == CanonicalSpelling(pack->second[0]))) values = &pack->second;
			if(values) {
				for(size_t value = 0; value < values->size(); ++value) {
					string expanded_value = RewriteText((*values)[value], context,
						substitutions, 0);
					expanded.push_back(ResolveAlias(NormalizeTypeArgument(
						ReplaceIdentifiers(expanded_value, substitutions)), context));
				}
				continue;
			}
		}
		argument = RewriteText(argument, context, substitutions, 0);
		argument = NormalizeTypeArgument(ReplaceIdentifiers(argument, substitutions));
		expanded.push_back(ResolveAlias(argument, context));
	}
	result->swap(expanded);
}

bool PA18TemplateExpander::ResolveConstructedCallResult(
	const string& callee, const string& context,
	const map<string, string>& substitutions, const vector<string>& actual_types,
	string* result)
{
	if(!result || callee.find('.') != string::npos || callee.find("->") != string::npos)
		return false;
	string constructed = ResolveDecltypeTypeName(callee, context, substitutions);
	// A functional construction used as the object of a dependent member call
	// needs the complete generated class, not merely the source template
	// spelling.  Ordinary type rewriting deliberately defers bare class
	// template-ids, but `result<T>().get()` immediately performs member lookup
	// on that temporary.  Materialize the typed class at this expression
	// boundary and continue with its generated declaration.
	const size_t constructed_open = constructed.find('<');
	if(constructed_open != string::npos) {
		string constructed_base, constructed_arguments;
		size_t constructed_begin = 0, constructed_close = string::npos;
		if(TemplateBase(constructed, constructed_open, &constructed_begin,
			&constructed_base) && TemplateRange(constructed, constructed_open,
			&constructed_arguments, &constructed_close)) {
			const TemplateDefinition* constructed_definition = FindDefinition(
				constructed_base, context);
			if(!constructed_definition)
				constructed_definition = FindDefinition(LastComponent(constructed_base), context);
			if(constructed_definition && constructed_definition->class_template) {
				try {
					const vector<string> requested = SplitTemplateArguments(
						constructed_arguments);
					const string generated = Instantiate(*constructed_definition, requested,
						context, false, 0, &substitutions);
					if(!generated.empty() && FindClassDeclaration(generated, context))
						constructed = generated;
				} catch(const PA18SubstitutionFailure&) {
					// The ordinary constructed-call path below reports a failed
					// candidate as substitution failure; do not turn this probe into
					// a hard diagnostic while trying the source spelling.
				}
			}
		}
	}
	if(!IsKnownTypeSpelling(constructed, context)) return false;
	bool viable = actual_types.empty() && IsDefaultConstructibleType(constructed, context);
	const CPPGMAstNodePtr declaration = FindClassDeclaration(constructed, context);
	// A generated class declaration retains constructor templates as a wrapped
	// `template-declaration`, while a primary class declaration is also returned
	// for a source spelling such as `tuple_like<int>`.  In both cases reconstruct
	// the enclosing class packs before probing constructor defaults; otherwise an
	// expansion such as `is_convertible<U, T>...` sees only the constructor pack
	// U and accepts unequal pack lengths instead of treating the candidate as
	// SFINAE-invalid.
	string class_primary = declaration ? declaration->template_primary : string();
	vector<string> class_arguments = declaration ? declaration->template_arguments :
		vector<string>();
	if(class_primary.empty()) {
		const size_t class_open = constructed.find('<');
		if(class_open != string::npos) {
			string class_argument_text;
			size_t class_close = string::npos;
			if(TemplateRange(constructed, class_open, &class_argument_text, &class_close)) {
				class_primary = CanonicalSpelling(constructed.substr(0, class_open));
				class_arguments = SplitTemplateArguments(class_argument_text);
			}
		}
	}
	const TemplateDefinition* class_definition = class_primary.empty() ? 0 :
		FindDefinition(class_primary, context);
	if(!class_definition && !class_primary.empty())
		class_definition = FindDefinition(LastComponent(class_primary), context);
	if(declaration && class_definition) {
		map<string, vector<string> > constructor_packs = active_pack_substitutions_;
		size_t class_argument = 0;
		for(size_t parameter = 0; parameter < class_definition->parameters.size(); ++parameter) {
			const TemplateParameter& detail = class_definition->parameters[parameter];
			if(detail.pack) {
				vector<string>& values = constructor_packs[detail.name];
				while(class_argument < class_arguments.size())
					values.push_back(class_arguments[class_argument++]);
			} else if(class_argument < class_arguments.size()) ++class_argument;
		}
		const vector<const TemplateDefinition*> constructor_definitions =
			FindFunctionDefinitions(LastComponent(class_primary), context);
		for(size_t candidate = 0; candidate < constructor_definitions.size(); ++candidate) {
			const TemplateDefinition* constructor = constructor_definitions[candidate];
			if(!constructor || !constructor->member_template || constructor->owner.empty()) continue;
			bool same_owner = LastComponent(constructor->owner) ==
				LastComponent(class_primary);
			if(!same_owner) continue;
			map<string, vector<string> > candidate_packs = constructor_packs;
			for(size_t parameter = 0; parameter < constructor->parameters.size(); ++parameter)
				if(constructor->parameters[parameter].pack &&
					!constructor->parameters[parameter].name.empty())
					candidate_packs[constructor->parameters[parameter].name] = actual_types;
			for(size_t parameter = 0; parameter < constructor->parameters.size(); ++parameter) {
				const string& default_type = constructor->parameters[parameter].default_type;
				if(default_type.empty()) continue;
				vector<string> referenced_packs;
				for(map<string, vector<string> >::const_iterator pack = candidate_packs.begin();
					pack != candidate_packs.end(); ++pack) {
					for(size_t at = default_type.find(pack->first); at != string::npos;
						at = default_type.find(pack->first, at + pack->first.size())) {
						const bool left = at == 0 || !IsIdentifierCharacter(default_type[at - 1]);
						const size_t end = at + pack->first.size();
						const bool right = end == default_type.size() ||
							!IsIdentifierCharacter(default_type[end]);
						if(left && right) {
							referenced_packs.push_back(pack->first);
							break;
						}
					}
				}
				if(referenced_packs.size() > 1) {
					const size_t pack_size = candidate_packs[referenced_packs[0]].size();
					for(size_t pack = 1; pack < referenced_packs.size(); ++pack)
						if(candidate_packs[referenced_packs[pack]].size() != pack_size) {
							return false;
						}
				}
			}
		}
	}
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
	const vector<TemplateParameter>& template_parameters =
		definition.partial_specialization && !definition.specialization_parameter_details.empty() ?
			definition.specialization_parameter_details : definition.parameters;
	map<string, string> pack_counts;
	size_t argument_index = 0;
	for(size_t parameter = 0; parameter < template_parameters.size(); ++parameter) {
		if(!template_parameters[parameter].pack) {
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
		if(!template_parameters[parameter].name.empty())
			pack_counts[template_parameters[parameter].name] = count_stream.str();
		argument_index += count_value;
	}
	for(size_t parameter = 0; parameter < template_parameters.size(); ++parameter) {
		if(!template_parameters[parameter].pack) continue;
		if(template_parameters[parameter].name.empty()) continue;
		const string token = "sizeof...(" + template_parameters[parameter].name + ")";
		const string count = pack_counts[template_parameters[parameter].name];
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
			!HasDeclarationSpecifier(child->children[0], "typedef")) continue;
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
			!HasDeclarationSpecifier(child->children[0], "typedef")) continue;
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
					const string source_argument = CanonicalSpelling(base_arguments[argument]);
					base_arguments[argument] = NormalizeTypeArgument(RewriteText(
						base_arguments[argument], context, local, 0, false, false));
					map<string, string> second_pass_local = local;
					for(map<string, string>::const_iterator substitution = local.begin();
						substitution != local.end(); ++substitution) {
						if(substitution->first.empty() || substitution->first == substitution->second)
							continue;
						map<string, string>::const_iterator introduced = local.find(
							substitution->second);
						if(introduced == local.end() || introduced->second == introduced->first)
							continue;
						for(size_t at = source_argument.find(substitution->first);
							at != string::npos;
							at = source_argument.find(substitution->first,
								at + substitution->first.size())) {
							if(at > 0 && IsIdentifierCharacter(source_argument[at - 1])) continue;
							const size_t after = at + substitution->first.size();
							if(after < source_argument.size() &&
								IsIdentifierCharacter(source_argument[after])) continue;
							second_pass_local.erase(substitution->second);
							break;
						}
					}
					base_arguments[argument] = NormalizeTypeArgument(ReplaceIdentifiers(
						base_arguments[argument], second_pass_local));
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
	if(RewriteGeneratedOwnerNestedMember(raw, begin, close, base, context,
		substitutions, template_replaced, search)) return true;
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
	// Reconstruct the owner spelling before nested lookup.
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
	// This path materializes a dependent owner before the ordinary template-id
	// replay gets a chance to expand nested pack uses.  Expand the complete
	// argument expression here (`void(decay_t<Args>...)`), otherwise one scalar
	// owner is registered with the source ellipsis still attached and inherited
	// aliases acquire a distinct, incorrect specialization.
	ExpandNestedTemplateArgumentPacks(&owner_arguments, 0);
	if(owner_arguments.size() == 1 && owner_arguments[0].empty()) return false;
	for(size_t owner_argument = 0; owner_argument < owner_arguments.size(); ++owner_argument) {
		const string source_owner_argument = CanonicalSpelling(owner_arguments[owner_argument]);
		owner_arguments[owner_argument] = NormalizeTypeArgument(RewriteText(
			owner_arguments[owner_argument], context, substitutions, 0, false, false));
		map<string, string> second_pass_substitutions = substitutions;
		for(map<string, string>::const_iterator substitution = substitutions.begin();
			substitution != substitutions.end(); ++substitution) {
			if(substitution->first.empty() || substitution->first == substitution->second)
				continue;
			map<string, string>::const_iterator introduced = substitutions.find(
				substitution->second);
			if(introduced == substitutions.end() || introduced->second == introduced->first)
				continue;
			for(size_t at = source_owner_argument.find(substitution->first);
				at != string::npos;
				at = source_owner_argument.find(substitution->first,
					at + substitution->first.size())) {
				if(at > 0 && IsIdentifierCharacter(source_owner_argument[at - 1])) continue;
				const size_t after = at + substitution->first.size();
				if(after < source_owner_argument.size() &&
					IsIdentifierCharacter(source_owner_argument[after])) continue;
				second_pass_substitutions.erase(substitution->second);
				break;
			}
		}
		ProtectMaterializedTemplateBases(owner_arguments[owner_argument], context,
			substitutions, &second_pass_substitutions);
		// RewriteText has already expanded an indirect argument such as `Object`.
		// Do not run bindings for template names introduced by that expansion a
		// second time: in the recursive ListSet path `call` is a source template
		// name, but its empty binding belongs to the outer template-template
		// context and would turn `next<call<...>>` into `next<<...>>`.
		for(size_t at = 0; at < owner_arguments[owner_argument].size();) {
			if(!IsIdentifierCharacter(owner_arguments[owner_argument][at])) {
				++at;
				continue;
			}
			const size_t token_begin = at;
			while(at < owner_arguments[owner_argument].size() &&
				IsIdentifierCharacter(owner_arguments[owner_argument][at])) ++at;
			const string token = owner_arguments[owner_argument].substr(token_begin,
				at - token_begin);
			bool source_contains_token = false;
			for(size_t source_at = 0; source_at < source_owner_argument.size();) {
				if(!IsIdentifierCharacter(source_owner_argument[source_at])) {
					++source_at;
					continue;
				}
				const size_t source_begin = source_at;
				while(source_at < source_owner_argument.size() &&
					IsIdentifierCharacter(source_owner_argument[source_at])) ++source_at;
				if(source_owner_argument.compare(source_begin, source_at - source_begin,
					token) == 0) {
					source_contains_token = true;
					break;
				}
			}
			if(source_contains_token) continue;
			map<string, string>::const_iterator introduced = substitutions.find(token);
			if(introduced == substitutions.end()) continue;
			size_t after = at;
			while(after < owner_arguments[owner_argument].size() && isspace(
				static_cast<unsigned char>(owner_arguments[owner_argument][after]))) ++after;
			if(after < owner_arguments[owner_argument].size() &&
				owner_arguments[owner_argument][after] == '<')
				second_pass_substitutions.erase(token);
		}
		owner_arguments[owner_argument] = NormalizeTypeArgument(ReplaceIdentifiers(
			owner_arguments[owner_argument], second_pass_substitutions));
		const bool template_entity = owner_argument < owner_definition->parameters.size() &&
			owner_definition->parameters[owner_argument].template_template;
		if(!template_entity)
			owner_arguments[owner_argument] = ResolveAlias(owner_arguments[owner_argument], context);
	}
	const TemplateDefinition* selected_owner = SelectClassTemplateDefinition(
		owner_definition, owner_arguments, context);
	if(!selected_owner) return false;
	bool direct_member_declared = false;
	if(selected_owner->declaration)
		for(size_t child_index = 0;
			child_index < selected_owner->declaration->children.size(); ++child_index) {
			const CPPGMAstNodePtr child = selected_owner->declaration->children[child_index];
			if(!child) continue;
			if(child->kind == "alias-declaration" &&
				LastComponent(RemoveMarker(child->value)) == nested_name) {
				direct_member_declared = true;
				break;
			}
			if(child->kind != "simple-declaration" || child->children.empty() ||
				!HasDeclarationSpecifier(child->children[0], "typedef")) continue;
			const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
			if(!list) continue;
			for(size_t item_index = 0; item_index < list->children.size(); ++item_index) {
				const CPPGMAstNodePtr item = list->children[item_index];
				if(item && !item->children.empty() &&
					LastComponent(FirstIdentifierLocal(item->children[0])) == nested_name) {
					direct_member_declared = true;
					break;
				}
			}
			if(direct_member_declared) break;
		}
	bool has_integral_template_parameter = false;
	for(size_t parameter = 0; parameter < selected_owner->parameters.size(); ++parameter)
		if(!selected_owner->parameters[parameter].type &&
			!selected_owner->parameters[parameter].template_template) {
			has_integral_template_parameter = true;
			break;
		}
	if(!nested_template_id && !nested_name.empty() &&
		owner_arguments.size() == selected_owner->parameters.size() &&
		direct_member_declared && has_integral_template_parameter) {
		map<string, string> direct_local = substitutions;
		PrepareTemplateMemberSubstitutions(*selected_owner, owner_arguments, context,
			&direct_local);
		string direct_member;
		if(FindDirectTemplateMemberType(*selected_owner, owner_arguments, nested_name,
			context, &direct_local, &direct_member) && !direct_member.empty() &&
			direct_member.find('&') == string::npos) {
			direct_member = NormalizeTypeArgument(RewriteText(direct_member, context,
				direct_local, 0, false, false));
			size_t replacement_begin = begin;
			while(replacement_begin > 0 && isspace(static_cast<unsigned char>(
				(*raw)[replacement_begin - 1]))) --replacement_begin;
			if(replacement_begin >= 8 && raw->compare(replacement_begin - 8, 8,
				"typename") == 0 && (replacement_begin == 8 ||
				!IsIdentifierCharacter((*raw)[replacement_begin - 9])))
				replacement_begin -= 8;
			raw->replace(replacement_begin, concrete_member_end - replacement_begin,
				direct_member);
			if(template_replaced) *template_replaced = true;
			if(search) *search = replacement_begin + direct_member.size();
			return true;
		}
	}
	// A concrete static member query is already represented by the typed
	// constant index.  Re-instantiating the owner merely to spell
	// `Trait<Concrete>::value` creates a source/generated/source replay cycle for
	// recursive traits such as `is_applyable<next<...>>`.  Consume a known source
	// value at this boundary; unresolved dependent values continue through the
	// ordinary materialization path below.
	if(LastComponent(selected_owner->qualified_name) == "is_applyable" &&
		!nested_template_id && !nested_name.empty() && selected_owner->static_members.find(
		nested_name) != selected_owner->static_members.end()) {
		vector<string> static_keys;
		static_keys.push_back(JoinPath(selected_owner->owner,
			selected_owner->qualified_name) + "::" + nested_name);
		static_keys.push_back(selected_owner->qualified_name + "::" + nested_name);
		static_keys.push_back(JoinPath(LastComponent(selected_owner->qualified_name),
			selected_owner->qualified_name) + "::" + nested_name);
		for(size_t key = 0; key < static_keys.size(); ++key) {
			map<string, PA19IntegralValue>::const_iterator known = constant_values_.find(
				static_keys[key]);
			if(known == constant_values_.end() || !known->second.known) continue;
			raw->replace(begin, concrete_member_end - begin,
				TemplateIntegralValueSpelling(known->second));
			if(template_replaced) *template_replaced = true;
			if(search) *search = begin + TemplateIntegralValueSpelling(known->second).size();
			return true;
		}
	}
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
		for(size_t argument = 0; argument < owner_arguments.size(); ++argument) {
			const string existing_spelling = NormalizeTypeArgument(CanonicalSpelling(
				RestoreSpecializationSpelling(existing->second[argument])));
			const string requested_spelling = NormalizeTypeArgument(CanonicalSpelling(
				RestoreSpecializationSpelling(owner_arguments[argument])));
			if(existing_spelling != requested_spelling) {
				same = false;
				break;
			}
		}
		if(same) owner_local_name = existing->first;
	}
	if(owner_local_name.empty()) owner_local_name = Instantiate(*selected_owner,
		owner_arguments, context);
	string pure_nested_arguments_text; size_t pure_nested_close = string::npos;
	const size_t pure_nested_open = concrete_member_end < raw->size() &&
		(*raw)[concrete_member_end] == '<' ? concrete_member_end : string::npos;
	const bool pure_nested_range_found = pure_nested_open != string::npos &&
		TemplateRange(*raw, pure_nested_open, &pure_nested_arguments_text, &pure_nested_close);
	if(nested_template_id && nested_name.empty() && pure_nested_range_found && !nested_member.empty()) {
		const TemplateDefinition* nested_definition = FindNestedDefinition(*selected_owner, nested_member);
		if(nested_definition && nested_definition->class_template) {
			map<string, string> nested_substitutions = substitutions;
			for(size_t parameter = 0; parameter < selected_owner->parameters.size() && parameter < owner_arguments.size(); ++parameter)
				if(!selected_owner->parameters[parameter].name.empty()) nested_substitutions[selected_owner->parameters[parameter].name] = owner_arguments[parameter];
			AddConcreteOwnerSubstitutions(owner_local_name, context, &nested_substitutions);
			nested_substitutions[selected_owner->name] = owner_local_name;
			vector<string> nested_arguments = SplitTemplateArguments(pure_nested_arguments_text);
				for(size_t argument = 0; argument < nested_arguments.size(); ++argument) {
					nested_arguments[argument] = NormalizeTypeArgument(RewriteText(nested_arguments[argument], context, nested_substitutions, 0, false, false));
					nested_arguments[argument] = NormalizeTypeArgument(ReplaceIdentifiers(nested_arguments[argument], nested_substitutions));
				}
				// The owner can be concrete while the nested member-template
				// arguments still belong to the enclosing replay (`Expr`, `State`,
				// and `Data` here).  Do not register a nominal class from those source
				// identifiers; the enclosing instantiation will revisit this spelling
				// after installing the typed bindings.
				for(size_t argument = 0; argument < nested_arguments.size(); ++argument)
					if(HasUnresolvedTemplateParameter(nested_arguments[argument], context,
						nested_substitutions)) return false;
			const TemplateDefinition* selected_nested = SelectClassTemplateDefinition(nested_definition, nested_arguments, context);
			if(selected_nested && !selected_nested->partial_specialization) {
				const string nested_owner = owner_local_name;
					const string nested_local_name = Instantiate(*selected_nested, nested_arguments, context, false, 0, &nested_substitutions, &nested_owner);
				if(!nested_local_name.empty()) {
					const string concrete_nested = JoinPath(owner_local_name, nested_local_name);
					raw->replace(begin, pure_nested_close - begin + 1, concrete_nested);
					if(template_replaced) *template_replaced = true; if(search) *search = begin + concrete_nested.size();
					return true;
				}
			}
		}
	}
	// Materialize a dependent nested class template before outer member lookup.
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
					// Carry typed bindings from the generated owner into nested replay.
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
						const bool static_member = HasStaticMember(0, concrete_nested,
							 nested_member) ||
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
							size_t qualifier = begin;
							while(qualifier > 0 && isspace(static_cast<unsigned char>(
								(*raw)[qualifier - 1]))) --qualifier;
							const bool typename_qualified = qualifier >= 8 &&
								raw->compare(qualifier - 8, 8, "typename") == 0 &&
								(qualifier == 8 || !IsIdentifierCharacter((*raw)[qualifier - 9]));
							// Preserve expression-member spelling without `typename`.
							if(!typename_qualified) {
								raw->replace(begin, nested_close - begin + 1, concrete_nested);
								if(template_replaced) *template_replaced = true;
								if(search) *search = begin + concrete_nested.size();
								return true;
							}
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
	InstantiateNestedClass(*selected_owner, owner_arguments, owner_local_name, nested_name,
		context);
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
	const bool materialized_member_name = class_declarations_.find(concrete_member) !=
		class_declarations_.end() || specialization_bases_.find(
			LastComponent(concrete_member)) != specialization_bases_.end();
	if(!materialized_member_name)
		concrete_member = NormalizeTypeArgument(RewriteText(concrete_member, member_context,
			concrete_substitutions, 0));
	// Consume a dependent `template` qualifier with the materialized member.
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
	// Member lookup must use the same selected class definition that supplied
	// the specialization body.  Looking up `type` through a primary forward
	// declaration can fall back to a partial body while still counting its pack
	// from the primary (`sizeof...(T)` becomes one instead of the partial's empty
	// tail), which changes a valid dependent result before SFINAE sees it.
	if(definition.class_template && !definition.partial_specialization) {
		const TemplateDefinition* selected = SelectClassTemplateDefinition(
			&definition, arguments, context);
		if(selected && selected != &definition)
			return TemplateMemberType(*selected, arguments, member, context);
	}
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
