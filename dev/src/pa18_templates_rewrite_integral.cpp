#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

namespace {
size_t IdentifierTokenCount(const string& text, const string& identifier)
{
	if(identifier.empty()) return 0;
	size_t count = 0;
	for(size_t at = 0; at < text.size();) {
		if(!IsIdentifierCharacter(text[at])) {
			++at;
			continue;
		}
		const size_t begin = at;
		while(at < text.size() && IsIdentifierCharacter(text[at])) ++at;
		if(at - begin == identifier.size() &&
			text.compare(begin, identifier.size(), identifier) == 0) ++count;
	}
	return count;
}
bool HasClassScopeMemberUse(const CPPGMAstNodePtr& class_node, size_t declaration_index,
	const string& name)
{
	if(!class_node) return false;
	for(size_t sibling = 0; sibling < class_node->children.size(); ++sibling) {
		if(sibling == declaration_index) continue;
		const CPPGMAstNodePtr& node = class_node->children[sibling];
		if(!node || node->kind == "function-definition" ||
			node->kind == "special-member-definition" ||
			node->kind == "special-member-declaration") continue;
		if(IdentifierTokenCount(SpellNode(node), name) != 0) return true;
	}
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
			if(HasClassScopeMemberUse(definition.declaration, child, name)) {
				has_replayed_member_use = true;
				break;
			}
		}
	}
	if(!has_replayed_member_use) return;
	set<string> previous_unqualified_constants;
	for(map<string, PA19IntegralValue>::const_iterator value = constant_values_.begin();
		value != constant_values_.end(); ++value)
		if(value->first.find("::") == string::npos)
			previous_unqualified_constants.insert(value->first);
	for(size_t child = 0; child < definition.declaration->children.size(); ++child) {
		const CPPGMAstNodePtr declaration = definition.declaration->children[child];
		if(!declaration || declaration->kind != "simple-declaration") continue;
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
			if(HasClassScopeMemberUse(definition.declaration, child, name) &&
				constant_values_.find(qualified) != constant_values_.end())
				early_integral_members_.insert(qualified);
		}
	}
	for(map<string, PA19IntegralValue>::iterator value = constant_values_.begin();
		value != constant_values_.end(); ) {
		if(value->first.find("::") == string::npos &&
			previous_unqualified_constants.find(value->first) ==
				previous_unqualified_constants.end())
			constant_values_.erase(value++);
		else ++value;
	}
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
	if(!PA19Type(integral_base_type).integral) return;
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
		if(size_expression && initializer->kind == "initializer" &&
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
