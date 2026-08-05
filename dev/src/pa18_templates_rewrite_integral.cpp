#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::EvaluateIntegralTextSpecialForms(const string& raw,
	const string& context, const map<string, string>& substitutions,
	PA19IntegralValue* result)
{
	// A dependent type-id can contain `::value` as part of a source template
	// body, but it is not an integral expression until its enclosing class
	// specialization supplies the template parameters.  Do not send that
	// spelling through RewriteText: member replay would try to materialize the
	// same dependent owner while evaluating the value that selects it.
	if(raw.find("typename") != string::npos && HasUnresolvedTemplateParameter(
		raw, context, substitutions)) return false;
	if(EvaluateVariableTemplateValue(raw, context, substitutions, result)) return true;
	// The detection idiom commonly spells an always-symmetric probe as
	// `is_same<decltype(expr), decltype(expr)>::value`.  Rewriting either
	// operand through the full member-replay path while selecting the enclosing
	// partial specialization re-enters that same selection.  Once the typed
	// substitutions are installed, identical decltype spellings are already a
	// complete proof of equality and can be recorded without replaying them.
	const size_t same_value_separator = raw.rfind("::value");
	if(same_value_separator != string::npos &&
		raw.size() == same_value_separator + 7) {
		const string same_head = raw.substr(0, same_value_separator);
		const size_t same_open = same_head.find('<');
		string same_base, same_arguments;
		size_t same_begin = 0, same_close = string::npos;
		if(same_open != string::npos &&
			TemplateBase(same_head, same_open, &same_begin, &same_base) &&
			TemplateRange(same_head, same_open, &same_arguments, &same_close) &&
			LastComponent(same_base) == "is_same") {
			const vector<string> same_parts = SplitTemplateArguments(same_arguments);
			if(same_parts.size() == 2) {
				const string left = CanonicalSpelling(ReplaceIdentifiersPreservingPackSizes(
					same_parts[0], substitutions));
				const string right = CanonicalSpelling(ReplaceIdentifiersPreservingPackSizes(
					same_parts[1], substitutions));
				if(NormalizeTypeArgument(left) == NormalizeTypeArgument(right)) {
					*result = PA19IntegralValue::Signed(1, "bool", 1);
					return true;
				}
			}
		}
	}
	// A concrete trait value may already have been recorded under the generated
	// specialization identity.  Resolve that typed fact before falling back to
	// RewriteText: replaying `Trait<Args>::value` while selecting another trait
	// partial can ask for the same enclosing selection again.  The source
	// argument list is compared against the registry's restored spelling, so
	// generated names such as `fork_t_0_` remain nominally equivalent to their
	// source `fork_t<0>` type.
	const size_t generated_value_separator = raw.rfind("::");
	if(generated_value_separator != string::npos &&
		generated_value_separator + 2 < raw.size()) {
	const string generated_member = raw.substr(generated_value_separator + 2);
	const string generated_owner = raw.substr(0, generated_value_separator);
		const size_t generated_open = generated_owner.find('<');
		string generated_base, generated_arguments;
		size_t generated_begin = 0, generated_close = string::npos;
		if(raw.find_first_of("=^&|") == string::npos &&
			generated_owner.find("typename") == string::npos &&
			generated_owner.find("is_convertible_") == string::npos &&
			generated_owner.find_first_of("=^&|") == string::npos &&
			generated_open != string::npos &&
			TemplateBase(generated_owner, generated_open, &generated_begin, &generated_base) &&
			TemplateRange(generated_owner, generated_open, &generated_arguments, &generated_close)) {
			const TemplateDefinition* generated_primary = FindDefinition(generated_base, context);
			if(generated_primary && generated_primary->class_template) {
				const vector<string> requested = SplitTemplateArguments(generated_arguments);
				const auto normalized_requested = [this, &substitutions](const string& raw_argument) {
					string argument = CanonicalSpelling(ReplaceIdentifiersPreservingPackSizes(
						raw_argument, substitutions));
					const size_t open = argument.find('<');
					if(open != string::npos) {
						string base, arguments;
						size_t begin = 0, close = string::npos;
						if(TemplateBase(argument, open, &begin, &base) &&
							TemplateRange(argument, open, &arguments, &close) &&
							LastComponent(base) == "decay_t") {
							const vector<string> parts = SplitTemplateArguments(arguments);
							if(parts.size() == 1) argument = parts[0];
						}
					}
					return CollapseRepeatedQualifiedPath(CollapseRepeatedQualifier(
						NormalizeTypeArgument(RestoreSpecializationSpelling(argument))));
				};
				vector<string> normalized_requested_arguments;
				for(size_t argument = 0; argument < requested.size(); ++argument)
					normalized_requested_arguments.push_back(normalized_requested(requested[argument]));
				for(map<string, string>::const_iterator candidate = specialization_bases_.begin();
					candidate != specialization_bases_.end(); ++candidate) {
					string candidate_base = candidate->second;
					const size_t candidate_open = candidate_base.find('<');
					if(candidate_open != string::npos) candidate_base.erase(candidate_open);
					if(candidate_base != generated_primary->qualified_name) continue;
					map<string, vector<string> >::const_iterator candidate_arguments =
						specialization_arguments_.find(candidate->first);
					if(candidate_arguments == specialization_arguments_.end() ||
						candidate_arguments->second.size() < normalized_requested_arguments.size()) continue;
					bool matches = true;
					for(size_t argument = 0; argument < normalized_requested_arguments.size(); ++argument)
						if(normalized_requested_arguments[argument] != normalized_requested(
							candidate_arguments->second[argument])) {
							matches = false;
							break;
						}
					if(!matches) continue;
					for(size_t argument = normalized_requested_arguments.size();
						argument < candidate_arguments->second.size(); ++argument)
						if(argument >= generated_primary->parameters.size() ||
							generated_primary->parameters[argument].default_type.empty()) {
							matches = false;
							break;
						}
					if(!matches) continue;
					const string value_suffix = candidate->first + "::" + generated_member;
					for(map<string, PA19IntegralValue>::const_iterator value = constant_values_.begin();
						value != constant_values_.end(); ++value)
						if(value->first.size() >= value_suffix.size() &&
							value->first.compare(value->first.size() - value_suffix.size(),
								value_suffix.size(), value_suffix) == 0 && value->second.known) {
							*result = value->second;
							return true;
						}
				}
				if(generated_member == "is_valid" || generated_member == "value") {
					vector<string> completed_arguments = normalized_requested_arguments;
					while(completed_arguments.size() < generated_primary->parameters.size()) {
						const size_t parameter = completed_arguments.size();
						if(generated_primary->parameters[parameter].default_type.empty()) break;
						completed_arguments.push_back(CanonicalSpelling(ReplaceIdentifiers(
							generated_primary->parameters[parameter].default_type, substitutions)));
					}
					const auto source_member_value = [this, &context, &generated_member](
						const TemplateDefinition& definition, const map<string, string>& bindings,
						PA19IntegralValue* value) {
						if(!definition.declaration) return false;
						for(size_t child = 0; child < definition.declaration->children.size(); ++child) {
							const CPPGMAstNodePtr declaration = definition.declaration->children[child];
							if(!declaration || declaration->kind != "simple-declaration" ||
								declaration->children.empty()) continue;
							const CPPGMAstNodePtr list = ChildOfKindLocal(declaration,
								"init-declarator-list");
							if(!list) continue;
							for(size_t item = 0; item < list->children.size(); ++item) {
								const CPPGMAstNodePtr declarator = list->children[item];
								if(!declarator || declarator->children.size() < 2 ||
									LastComponent(FirstIdentifierLocal(declarator->children[0])) !=
										LastComponent(generated_member)) continue;
								const CPPGMAstNodePtr initializer = declarator->children[1];
								if(!initializer || initializer->children.empty()) continue;
								const string expression = ConstantExpressionSpelling(initializer->children[0]);
								if(expression.find("sizeof(") != string::npos || expression.find("test<") != string::npos) return false;
								return const_cast<PA18TemplateExpander*>(this)->EvaluateIntegralText(
									expression, context, bindings, value);
							}
						}
						return false;
					};
					map<string, vector<TemplateDefinition> >::const_iterator source_candidates =
						class_specializations_.find(generated_primary->qualified_name);
					bool matched_partial = false;
					if(source_candidates != class_specializations_.end() &&
						completed_arguments.size() == generated_primary->parameters.size())
						for(size_t candidate = 0; candidate < source_candidates->second.size(); ++candidate) {
							const TemplateDefinition& source = source_candidates->second[candidate];
							if(!source.partial_specialization) continue;
							map<string, string> bindings;
							bool matches = false;
							try {
								matches = MatchClassSpecializationPattern(source,
									completed_arguments, &bindings, context);
							} catch(const PA18SubstitutionFailure&) {
								matches = false;
							}
							if(!matches) continue;
							matched_partial = true;
							const bool source_value = source_member_value(source, bindings, result);
							if(source_value) return true;
						}
					if(!matched_partial) {
						map<string, string> no_bindings;
						const bool source_value = source_member_value(*generated_primary, no_bindings, result);
						if(source_value) return true;
					}
				}
			}
		}
		// A generated specialization is often already rewritten into a qualified
		// local class name by the surrounding class replay (`query_fn::Trait_X`),
		// so there is no template-id left for the argument comparison above.  The
		// local name is still a typed specialization identity; recover its member
		// fact from the same constant registry using a suffix match.
		if(raw.find_first_of("=^&|") == string::npos &&
			generated_open == string::npos &&
			generated_owner.find("is_convertible_") == string::npos &&
			generated_owner.find_first_of("=^&|") == string::npos) {
			const string local_owner = LastComponent(generated_owner);
			const string member_suffix = string("::") + local_owner + "::" + generated_member;
			for(map<string, PA19IntegralValue>::const_iterator value = constant_values_.begin();
				value != constant_values_.end(); ++value) {
				const bool exact = value->first == local_owner + "::" + generated_member;
				const bool qualified = value->first.size() >= member_suffix.size() &&
					value->first.compare(value->first.size() - member_suffix.size(),
						member_suffix.size(), member_suffix) == 0;
				if((exact || qualified) && value->second.known) {
					*result = value->second;
					return true;
				}
			}
			// A generated class can have a typed specialization identity without
			// having recorded its static member yet.  Reconstruct the source
			// template-id from that identity and let the ordinary source-member
			// replay evaluate the primary or selected partial declaration.
			map<string, string>::const_iterator source_base =
				specialization_bases_.find(local_owner);
			map<string, vector<string> >::const_iterator source_arguments =
				specialization_arguments_.find(local_owner);
			if(source_base != specialization_bases_.end() &&
				source_arguments != specialization_arguments_.end()) {
				string source_owner = source_base->second;
				const size_t source_open = source_owner.find('<');
				if(source_open != string::npos) source_owner.erase(source_open);
				string source_id = source_owner + "<";
				for(size_t argument = 0; argument < source_arguments->second.size(); ++argument) {
					if(argument) source_id += ",";
					source_id += source_arguments->second[argument];
				}
				source_id += ">::" + generated_member;
				if(source_id != generated_owner + "::" + generated_member &&
					EvaluateIntegralText(source_id, context, substitutions, result))
					{
					return true;
					}
			}
		}
	}
	if(raw.find('<') != string::npos) {
		const string rewritten = CanonicalSpelling(RemoveMarker(
			RewriteText(raw, context, substitutions, 0)));
		if(rewritten != raw && EvaluateIntegralText(rewritten, context,
			substitutions, result)) return true;
	}
	string expanded_size;
	if(EvaluateExpandedSizeofText(raw, context, substitutions, result, &expanded_size)) return true;
	if(raw.compare(0, 7, "sizeof(") == 0 && !raw.empty() && raw[raw.size() - 1] == ')') {
		const string operand = raw.substr(7, raw.size() - 8);
		string call_type;
		if(FunctionCallResultType(operand, context, substitutions, &call_type)) {
			string resolved = CanonicalSpelling(RemoveMarker(RewriteText(
				call_type, context, substitutions, 0)));
		resolved = ResolveAlias(resolved, context);
			const size_t size = EstimateTypeSize(resolved, context);
			if(size) {
				*result = PA19IntegralValue::Unsigned(
					static_cast<unsigned long long>(size), "unsigned long", 64);
				return true;
			}
		}
	}
	bool invalid_sizeof = false;
	for(size_t search = expanded_size.find("sizeof("); search != string::npos && !invalid_sizeof; ) {
		const size_t open = search + 6;
		int depth = 0; size_t close = string::npos;
		for(size_t position = open; position < expanded_size.size(); ++position) {
			if(expanded_size[position] == '(') ++depth;
			else if(expanded_size[position] == ')' && --depth == 0) { close = position; break; }
		}
		if(close == string::npos) break;
		string operand = CanonicalSpelling(ReplaceIdentifiers(
			expanded_size.substr(open + 1, close - open - 1), substitutions));
		while(operand.compare(0, 6, "const ") == 0)
			operand = CanonicalSpelling(operand.substr(6));
		while(operand.compare(0, 9, "volatile ") == 0)
			operand = CanonicalSpelling(operand.substr(9));
		const string resolved_operand = ResolveAlias(operand, context);
		if(resolved_operand == "void" || resolved_operand == "const void" ||
			resolved_operand == "volatile void") invalid_sizeof = true;
		map<string, CPPGMAstNodePtr>::const_iterator incomplete =
			class_declarations_.find(resolved_operand);
		if(incomplete != class_declarations_.end() && incomplete->second &&
			incomplete->second->kind == "class-forward-declaration") invalid_sizeof = true;
		search = expanded_size.find("sizeof(", close + 1);
	}
	if(invalid_sizeof) return true;
	if(EvaluateActivePackSize(raw, result)) return true;
	const size_t subscript_open = raw.find('[');
	if(subscript_open != string::npos && raw[raw.size() - 1] == ']' && subscript_open > 0) {
		PA19IntegralValue index;
		const string index_text = raw.substr(subscript_open + 1, raw.size() - subscript_open - 2);
		if(EvaluateIntegralText(index_text, context, substitutions, &index)) {
			const vector<PA19IntegralValue>* values = FindConstantArray(
				raw.substr(0, subscript_open), context);
			const long long offset = PA19Signed(index);
			if(values && offset >= 0 && static_cast<size_t>(offset) < values->size()) {
				*result = (*values)[static_cast<size_t>(offset)]; return result->known;
			}
		}
	}
	const size_t value_separator = raw.rfind("::value");
	if(value_separator != string::npos) {
		const string owner = raw.substr(0, value_separator);
		const string resolved_owner = CanonicalSpelling(ResolveAlias(owner, context));
		if(!resolved_owner.empty() && resolved_owner != owner) {
			PA19IntegralValue aliased_value;
			if(EvaluateIntegralText(resolved_owner + "::value", context,
				substitutions, &aliased_value)) {
				*result = aliased_value; return result->known;
			}
		}
	}
	const size_t functional_open = raw.find('(');
	const bool functional_cast = functional_open != string::npos &&
		!raw.empty() && raw[raw.size() - 1] == ')' && functional_open > 0;
	const size_t braced_open = raw.find('{');
	const bool braced_cast = braced_open != string::npos &&
		!raw.empty() && raw[raw.size() - 1] == '}' && braced_open > 0;
	if(functional_cast || braced_cast) {
		const size_t open = functional_cast ? functional_open : braced_open;
		const string target = ResolveAlias(CanonicalSpelling(raw.substr(0, open)), context);
		const PA19IntegralType target_type = PA19Type(target);
		if(target_type.integral) {
			const string operand = raw.substr(open + 1, raw.size() - open - 2);
			PA19IntegralValue converted;
			if(EvaluateIntegralText(operand, context, substitutions, &converted)) {
				*result = PA19Convert(converted, target_type); return result->known;
			}
		}
	}
	if(EvaluateIntegralTextCStyleCast(raw, context, substitutions, result)) return true;
	if(raw.compare(0, 2, "::") == 0) {
		string unscoped = raw;
		while(unscoped.compare(0, 2, "::") == 0) unscoped.erase(0, 2);
		map<string, PA19IntegralValue>::const_iterator known = constant_values_.find(unscoped);
		if(known != constant_values_.end()) { *result = known->second; return result->known; }
	}
	return false;
}
namespace {
void CollectIdentifierTokens(const string& text, set<string>* names)
{
	if(!names) return;
	for(size_t at = 0; at < text.size();) {
		if(!IsIdentifierCharacter(text[at])) {
			++at;
			continue;
		}
		const size_t begin = at;
		while(at < text.size() && IsIdentifierCharacter(text[at])) ++at;
		names->insert(text.substr(begin, at - begin));
	}
}

void CollectClassScopeIdentifierNames(const CPPGMAstNodePtr& node,
	set<string>* names)
{
	if(!node || !names) return;
	const bool spelling_node = node->kind == "identifier" ||
		node->kind == "id-expression" || node->kind == "type-name" ||
		node->kind == "decl-specifier" || node->kind == "type-specifier" ||
		node->kind == "base-name" || node->kind == "decltype-specifier" ||
		node->kind == "template-id";
	if(spelling_node) CollectIdentifierTokens(RemoveMarker(node->value), names);
	for(size_t argument = 0; argument < node->template_arguments.size(); ++argument)
		CollectIdentifierTokens(RemoveMarker(node->template_arguments[argument]), names);
	for(size_t child = 0; child < node->children.size(); ++child)
		CollectClassScopeIdentifierNames(node->children[child], names);
}

void IndexClassScopeIdentifierUses(const CPPGMAstNodePtr& class_node,
	map<string, set<size_t> >* uses)
{
	if(!class_node || !uses) return;
	for(size_t child = 0; child < class_node->children.size(); ++child) {
		const CPPGMAstNodePtr& declaration = class_node->children[child];
		if(!declaration || declaration->kind == "function-definition" ||
			declaration->kind == "special-member-definition" ||
			declaration->kind == "special-member-declaration") continue;
		set<string> names;
		CollectClassScopeIdentifierNames(declaration, &names);
		for(set<string>::const_iterator name = names.begin(); name != names.end(); ++name)
			(*uses)[*name].insert(child);
	}
}

bool HasOtherClassScopeUse(const map<string, set<size_t> >& uses,
	size_t declaration_index, const string& name)
{
	map<string, set<size_t> >::const_iterator found = uses.find(name);
	if(found == uses.end()) return false;
	for(set<size_t>::const_iterator declaration = found->second.begin();
		declaration != found->second.end(); ++declaration)
		if(*declaration != declaration_index) return true;
	return false;
}
}

bool PA18TemplateExpander::ContainsSizeOrAlignExpression(
	const CPPGMAstNodePtr& node)
{
	if(!node) return false;
	if(node->kind == "sizeof-expression" ||
		node->kind == "sizeof-pack-expression") return true;
	if(node->kind == "type-trait-expression" &&
		RemoveMarker(node->value) == "alignof") return true;
	for(size_t child = 0; child < node->children.size(); ++child)
		if(ContainsSizeOrAlignExpression(node->children[child])) return true;
	return false;
}

void PA18TemplateExpander::RegisterEarlyIntegralMembers(
	const TemplateDefinition& definition, const string& context,
	const map<string, string>& substitutions)
{
	if(!definition.class_template || !definition.declaration ||
		(definition.declaration->kind != "class-specifier" &&
			definition.declaration->kind != "class-forward-declaration")) return;
	map<string, set<size_t> > class_scope_identifier_uses;
	IndexClassScopeIdentifierUses(definition.declaration,
		&class_scope_identifier_uses);
	bool has_replayed_member_use = false;
	for(size_t child = 0; child < definition.declaration->children.size() &&
		!has_replayed_member_use; ++child) {
		const CPPGMAstNodePtr declaration = definition.declaration->children[child];
		if(!declaration || declaration->kind != "simple-declaration") continue;
		const CPPGMAstNodePtr list = ChildOfKindLocal(declaration,
			"init-declarator-list");
		if(!list) continue;
		for(size_t item = 0; item < list->children.size(); ++item) {
			const CPPGMAstNodePtr declarator = list->children[item];
			if(!declarator || declarator->children.empty()) continue;
			const string name = FirstIdentifierLocal(declarator->children[0]);
			if(!HasDeclarationSpecifier(declaration->children[0], "const")) continue;
			if(HasOtherClassScopeUse(class_scope_identifier_uses, child, name)) {
				has_replayed_member_use = true;
				break;
			}
		}
	}
	if(!has_replayed_member_use) return;
	set<string> temporary_unqualified_constants;
	for(size_t child = 0; child < definition.declaration->children.size(); ++child) {
		const CPPGMAstNodePtr declaration = definition.declaration->children[child];
		if(!declaration || declaration->kind != "simple-declaration") continue;
		const bool constant_declaration = !declaration->children.empty() &&
			(HasDeclarationSpecifier(declaration->children[0], "const") ||
				HasDeclarationSpecifier(declaration->children[0], "constexpr"));
		if(constant_declaration) {
			const CPPGMAstNodePtr list = ChildOfKindLocal(declaration,
				"init-declarator-list");
			if(list) for(size_t item = 0; item < list->children.size(); ++item) {
				const CPPGMAstNodePtr declarator = list->children[item];
				if(!declarator || declarator->children.empty()) continue;
				const string name = FirstIdentifierLocal(declarator->children[0]);
				if(!name.empty() && constant_values_.find(name) == constant_values_.end())
					temporary_unqualified_constants.insert(name);
			}
		}
		RecordConstantDeclaration(declaration, context, substitutions);
		const CPPGMAstNodePtr list = ChildOfKindLocal(declaration,
			"init-declarator-list");
		if(!list || !HasDeclarationSpecifier(declaration->children[0], "const"))
			continue;
		for(size_t item = 0; item < list->children.size(); ++item) {
			const CPPGMAstNodePtr declarator = list->children[item];
			if(!declarator || declarator->children.empty()) continue;
			const string name = FirstIdentifierLocal(declarator->children[0]);
			const string owner = active_instantiation_name_.empty() ? context :
				active_instantiation_name_;
			const string qualified = JoinPath(owner, name);
			if(HasOtherClassScopeUse(class_scope_identifier_uses, child, name) &&
				constant_values_.find(qualified) != constant_values_.end())
				early_integral_members_.insert(qualified);
		}
	}
	for(set<string>::const_iterator name = temporary_unqualified_constants.begin();
		name != temporary_unqualified_constants.end(); ++name)
		constant_values_.erase(*name);
}

void PA18TemplateExpander::RecordConstantDeclaration(
	const CPPGMAstNodePtr& node, const string& context,
	const map<string, string>& substitutions)
{
	if(!node || node->kind != "simple-declaration" || node->children.empty()) return;
	RecordConstantArrayDeclaration(node, context, substitutions);
	if(!HasDeclarationSpecifier(node->children[0], "const") &&
		!HasDeclarationSpecifier(node->children[0], "constexpr")) return;
	const string base_type = NodeTypeSpelling(node->children[0]);
	const string resolved_base_type = ResolveAlias(ReplaceIdentifiers(base_type, substitutions), context);
	string integral_base_type = CanonicalSpelling(resolved_base_type);
	while(integral_base_type.compare(0, 6, "const ") == 0 ||
		integral_base_type.compare(0, 9, "volatile ") == 0)
		integral_base_type = CanonicalSpelling(integral_base_type.substr(
			integral_base_type.find(' ') + 1));
	while(integral_base_type.size() > 6 &&
		integral_base_type.compare(integral_base_type.size() - 6, 6, " const") == 0)
		integral_base_type = CanonicalSpelling(integral_base_type.substr(0,
			integral_base_type.size() - 6));
	while(integral_base_type.size() > 9 &&
		integral_base_type.compare(integral_base_type.size() - 9, 9, " volatile") == 0)
		integral_base_type = CanonicalSpelling(integral_base_type.substr(0,
			integral_base_type.size() - 9));
	// Scoped and unscoped enum names are integral constant types too.  Their
	// enumerators are already recorded by RecordEnumConstants; retain a
	// constexpr enum-typed member's initializer under the generated owner so a
	// later replay can compare facts such as `overload == call_member` without
	// re-entering class-template selection.
	const bool enum_constant = IsKnownEnumType(integral_base_type, context);
	if(!PA19Type(integral_base_type).integral && !enum_constant) return;
	const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
	if(!list) return;
	for(size_t i = 0; i < list->children.size(); ++i) {
		const CPPGMAstNodePtr item = list->children[i];
		if(!item || item->children.size() < 2 || !item->children[0]) continue;
		if(!DeclaratorArraySuffix(item->children[0]).empty()) continue;
		const string name = FirstIdentifierLocal(item->children[0]);
		const CPPGMAstNodePtr initializer = item->children[1];
		if(name.empty() || !initializer || initializer->children.empty()) continue;
		PA19IntegralValue value;
		const CPPGMAstNodePtr expression = initializer->children[0];
		const string expression_text = ConstantExpressionSpelling(expression);
		if(!HasReplayContext(substitutions) && HasUnresolvedTemplateParameter(expression_text, context, substitutions)) continue;
		if(!HasReplayContext(substitutions) && expression_text.find("decltype(") != string::npos) continue;
		if(!EvaluateIntegralText(expression_text, context, substitutions, &value)) continue;
		const bool size_expression = ContainsSizeOrAlignExpression(expression);
		// Keep a replayed enum-valued member literal as well.  The lowering
		// phase needs the typed constant when a later constexpr condition reads
		// that member through a generated specialization (for example
		// `call_traits::overload != ill_formed`).
		if((size_expression || enum_constant) && initializer->kind == "initializer" &&
			initializer->children.size() == 1)
			initializer->children[0] = CPPGMAstNodePtr(new CPPGMAstNode("literal",
				TemplateIntegralValueSpelling(value)));
		const string qualified = JoinPath(
			active_instantiation_name_.empty() ? context : active_instantiation_name_, name);
		constant_values_[qualified] = value;
		if(constant_values_.find(name) == constant_values_.end()) constant_values_[name] = value;
		const PA19IntegralType type = PA19Type(integral_base_type);
		if(type.integral) {
			constant_type_sizes_[qualified] = type.bits <= 8 ? 1 : type.bits <= 16 ? 2 : type.bits <= 32 ? 4 : 8;
			constant_type_alignments_[qualified] = constant_type_sizes_[qualified];
		}
	}
}

}
