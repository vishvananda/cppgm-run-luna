#include <functional>
#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::EvaluateUnqualifiedConstantMember(
	const string& raw, const string& context,
	const map<string, string>& substitutions, PA19IntegralValue* result,
	const string& preferred_owner)
{
	if(!result || raw.empty()) return false;
	const string query_key = raw + "\x1f" + context + "\x1f" + preferred_owner;
	if(!active_integral_queries_.insert(query_key).second) return false;
	struct IntegralQueryScope {
		set<string>* active; string key;
		IntegralQueryScope(set<string>* value, const string& name)
			: active(value), key(name) {}
		~IntegralQueryScope() { active->erase(key); }
	} query_scope(&active_integral_queries_, query_key);
	// Collection visits the primary/partial declaration before it has a
	// concrete substitution scope.  Its dependent initializer must stay
	// deferred; attempting to resolve an unqualified member against that same
	// source declaration recursively re-enters this helper.
	if(context.find('<') != string::npos && !HasReplayContext(substitutions) &&
		active_pack_substitutions_.empty() &&
		active_pack_identifier_substitutions_.empty()) return false;
	for(size_t i = 0; i < raw.size(); ++i)
		if(!IsIdentifierCharacter(raw[i])) return false;
	map<string, vector<string> >::const_iterator owners =
		constant_member_owners_.find(raw);
	if(owners == constant_member_owners_.end()) return false;
	vector<size_t> owner_order;
	for(size_t owner = 0; owner < owners->second.size(); ++owner)
		owner_order.push_back(owner);
	if(!preferred_owner.empty()) for(size_t owner = 0; owner < owner_order.size(); ++owner) {
		const string& owner_name = owners->second[owner_order[owner]];
		if(owner_name != preferred_owner) continue;
		if(owner != 0) swap(owner_order[0], owner_order[owner]);
		break;
	}
	for(size_t ordered = 0; ordered < owner_order.size(); ++ordered) {
		const size_t owner_index = owner_order[ordered];
		const string& owner_name = owners->second[owner_index];
		if(!preferred_owner.empty() && owner_name != preferred_owner) {
			// The source class that owns the replayed specialization is the only
			// valid unqualified scope for this targeted member-argument lookup.
			continue;
		}
		map<string, CPPGMAstNodePtr>::const_iterator candidate =
			class_declarations_.find(owner_name);
		if(candidate == class_declarations_.end() || !candidate->second) continue;
		const CPPGMAstNodePtr owner_declaration = candidate->second;
		for(size_t child = 0; child < owner_declaration->children.size(); ++child) {
			const CPPGMAstNodePtr member_declaration = owner_declaration->children[child];
			if(!member_declaration || member_declaration->kind != "simple-declaration" ||
				member_declaration->children.empty()) continue;
			const CPPGMAstNodePtr list = ChildOfKindLocal(member_declaration,
				"init-declarator-list");
			if(!list) continue;
			for(size_t item = 0; item < list->children.size(); ++item) {
				const CPPGMAstNodePtr declarator = list->children[item];
				if(!declarator || declarator->children.size() < 2 ||
					LastComponent(FirstIdentifierLocal(declarator->children[0])) != raw)
					continue;
				const CPPGMAstNodePtr initializer = declarator->children[1];
				if(!initializer || initializer->children.empty()) continue;
				map<string, string> member_substitutions = substitutions;
				map<string, string>::const_iterator base = specialization_bases_.find(
					LastComponent(candidate->first));
				map<string, vector<string> >::const_iterator arguments =
					specialization_arguments_.find(LastComponent(candidate->first));
				const TemplateDefinition* owner_definition = 0;
				map<string, vector<string> > owner_packs;
				if(base != specialization_bases_.end() && arguments != specialization_arguments_.end()) {
					owner_definition = FindDefinition(base->second, context);
					if(owner_definition) {
						size_t argument = 0;
						for(size_t parameter = 0; parameter < owner_definition->parameters.size();
							++parameter) {
							const TemplateParameter& item_parameter = owner_definition->parameters[parameter];
							if(item_parameter.pack) {
								vector<string> values;
								size_t trailing_fixed = 0;
								for(size_t later = parameter + 1;
									later < owner_definition->parameters.size(); ++later)
									if(!owner_definition->parameters[later].pack) ++trailing_fixed;
								const size_t available = arguments->second.size() > argument ?
									arguments->second.size() - argument : 0;
								const size_t count = available > trailing_fixed ?
									available - trailing_fixed : 0;
								for(size_t value = 0; value < count; ++value)
									values.push_back(arguments->second[argument++]);
								if(!item_parameter.name.empty()) {
									owner_packs[item_parameter.name] = values;
									if(!values.empty()) member_substitutions[item_parameter.name] = values[0];
									else member_substitutions.erase(item_parameter.name);
								}
								continue;
							}
							if(argument < arguments->second.size()) {
								if(!item_parameter.name.empty())
									member_substitutions[item_parameter.name] = arguments->second[argument];
								++argument;
							}
						}
					}
				}
				const map<string, vector<string> > previous_packs = active_pack_substitutions_;
				for(map<string, vector<string> >::const_iterator pack = owner_packs.begin();
					pack != owner_packs.end(); ++pack)
					if(!pack->first.empty()) active_pack_substitutions_[pack->first] = pack->second;
				const string expression = ConstantExpressionSpelling(initializer->children[0]);
				bool evaluated = EvaluatePreferredOwnerConstantExpression(expression, raw,
					preferred_owner, member_substitutions, result);
				if(expression != raw && !evaluated)
					evaluated = EvaluateIntegralText(expression, candidate->first,
						member_substitutions, result);
				active_pack_substitutions_ = previous_packs;
				if(evaluated) {
					const PA19IntegralType type = PA19Type(ResolveAlias(
						NodeTypeSpelling(member_declaration->children[0]), candidate->first));
					if(type.integral) *result = PA19Convert(*result, type);
					return result->known;
				}
			}
		}
	}
	return false;
}

bool PA18TemplateExpander::EvaluateQualifiedConstantMember(
	const string& raw, const map<string, string>& substitutions,
	PA19IntegralValue* result)
{
	if(!result) return false;
	const size_t separator = raw.rfind("::");
	if(separator == string::npos || separator == 0 || separator + 2 >= raw.size()) return false;
	const string owner = raw.substr(0, separator);
	const string member = raw.substr(separator + 2);
	for(size_t character = 0; character < member.size(); ++character)
		if(!IsIdentifierCharacter(member[character])) return false;
	string specialization_owner = owner;
	if(owner.find('<') == string::npos) {
		map<string, string>::const_iterator generated_base = specialization_bases_.find(
			LastComponent(owner));
		map<string, vector<string> >::const_iterator generated_arguments =
			specialization_arguments_.find(LastComponent(owner));
		if(generated_base != specialization_bases_.end() &&
			generated_arguments != specialization_arguments_.end()) {
			specialization_owner = generated_base->second;
			if(specialization_owner.find('<') == string::npos) {
				specialization_owner += "<";
				for(size_t argument = 0; argument < generated_arguments->second.size(); ++argument)
					specialization_owner += (argument ? "," : "") + generated_arguments->second[argument];
				specialization_owner += ">";
			}
		}
	}
	const size_t owner_open = specialization_owner.find('<');
	if(owner_open != string::npos) {
		string owner_base, owner_argument_text;
		size_t owner_begin = 0, owner_close = string::npos;
		if(TemplateBase(specialization_owner, owner_open, &owner_begin, &owner_base) &&
			TemplateRange(specialization_owner, owner_open, &owner_argument_text, &owner_close)) {
			const TemplateDefinition* primary = FindDefinition(owner_base, owner);
			if(!primary) primary = FindDefinition(LastComponent(owner_base), owner);
			if(primary && primary->class_template) {
				const vector<string> arguments = SplitTemplateArguments(owner_argument_text);
				const TemplateDefinition* selected = SelectClassTemplateDefinition(
					primary, arguments, owner);
				if(selected) {
					map<string, string> local = substitutions;
					if(selected->partial_specialization) {
						map<string, string> inferred;
						if(!MatchClassSpecializationPattern(*selected, arguments, &inferred, owner))
							selected = 0;
						else for(map<string, string>::const_iterator binding = inferred.begin();
							binding != inferred.end(); ++binding) local[binding->first] = binding->second;
					} else for(size_t parameter = 0; parameter < selected->parameters.size() &&
						parameter < arguments.size(); ++parameter)
						if(!selected->parameters[parameter].name.empty())
							local[selected->parameters[parameter].name] = arguments[parameter];
					if(selected) {
						CPPGMAstNodePtr declaration = selected->declaration;
						if(declaration) for(size_t child = 0; child < declaration->children.size(); ++child) {
							CPPGMAstNodePtr member_declaration = declaration->children[child];
							while(member_declaration && member_declaration->kind == "template-declaration" &&
								member_declaration->children.size() > 1)
								member_declaration = member_declaration->children[1];
							if(!member_declaration || member_declaration->kind != "simple-declaration" ||
								member_declaration->children.empty() ||
								(!HasDeclarationSpecifier(member_declaration->children[0], "const") &&
									!HasDeclarationSpecifier(member_declaration->children[0], "constexpr"))) continue;
							const CPPGMAstNodePtr list = ChildOfKindLocal(member_declaration,
								"init-declarator-list");
							if(!list) continue;
							for(size_t item = 0; item < list->children.size(); ++item) {
								const CPPGMAstNodePtr declarator = list->children[item];
								if(!declarator || declarator->children.size() < 2 ||
									LastComponent(FirstIdentifierLocal(declarator->children[0])) != member ||
									!declarator->children[1] || declarator->children[1]->children.empty()) continue;
								if(EvaluateIntegralText(ConstantExpressionSpelling(
									declarator->children[1]->children[0]), owner, local, result)) {
									const PA19IntegralType type = PA19Type(ResolveAlias(
										NodeTypeSpelling(member_declaration->children[0]), owner));
									if(type.integral) *result = PA19Convert(*result, type);
									return result->known;
								}
							}
						}
					}
				}
			}
		}
	}
	// A qualified lookup on a materialized owner must not fall through to an
	// unrelated unqualified constant with the same member name.  This matters
	// for primary/partial pairs such as `make_if_<T>::applied`: the primary has
	// no `applied` member, while a previously replayed true specialization does.
	string preferred_owner;
	if(class_declarations_.find(owner) != class_declarations_.end() ||
		specialization_bases_.find(LastComponent(owner)) != specialization_bases_.end())
		preferred_owner = owner;
	else {
		const size_t owner_open = owner.find('<');
		string owner_arguments_text;
		size_t owner_close = string::npos;
		if(owner_open != string::npos && TemplateRange(owner, owner_open,
			&owner_arguments_text, &owner_close)) {
			string materialized_owner;
			if(ResolveMaterializedClassOwner(owner.substr(0, owner_open),
				SplitTemplateArguments(owner_arguments_text), owner,
				&materialized_owner, substitutions))
				preferred_owner = materialized_owner;
		}
	}
	return EvaluateUnqualifiedConstantMember(member, owner, substitutions,
		result, preferred_owner);
}

} // namespace pa18_templates_internal
