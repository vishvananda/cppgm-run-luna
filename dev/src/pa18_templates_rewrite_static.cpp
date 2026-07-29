#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::FindVariableTemplateMemberType(
	const string& raw_class, const string& member,
	const map<string, string>& substitutions, const string& context,
	string* result)
{
	if(!result) return false;
	*result = string();
	string member_spelling = CanonicalSpelling(member);
	while(member_spelling.compare(0, 9, "template ") == 0)
		member_spelling = CanonicalSpelling(member_spelling.substr(9));
	const size_t member_open = member_spelling.find('<');
	string member_base;
	string member_argument_text;
	size_t member_begin = 0, member_close = string::npos;
	if(member_open == string::npos || !TemplateBase(member_spelling, member_open,
		&member_begin, &member_base) || !TemplateRange(member_spelling, member_open,
		&member_argument_text, &member_close) || member_close + 1 != member_spelling.size())
		return false;
	vector<string> member_arguments = SplitTemplateArguments(member_argument_text);
	for(size_t argument = 0; argument < member_arguments.size(); ++argument) {
		member_arguments[argument] = NormalizeTypeArgument(ReplaceIdentifiersPreservingPackSizes(
			member_arguments[argument], substitutions));
		try {
			member_arguments[argument] = NormalizeTypeArgument(ResolveAlias(RewriteText(
				member_arguments[argument], context, substitutions, 0, false, false), context));
		} catch(const PA18SubstitutionFailure&) {
			return false;
		}
	}
	string owner = CanonicalSpelling(ReplaceIdentifiersPreservingPackSizes(
		raw_class, substitutions));
	while(owner.compare(0, 6, "const ") == 0)
		owner = CanonicalSpelling(owner.substr(6));
	while(owner.compare(0, 9, "volatile ") == 0)
		owner = CanonicalSpelling(owner.substr(9));
	while(!owner.empty() && (owner[owner.size() - 1] == '&' ||
		owner[owner.size() - 1] == '*')) owner.erase(owner.size() - 1);
	owner = CanonicalSpelling(owner);
	try {
		const string rewritten_owner = CanonicalSpelling(RewriteText(owner, context,
			substitutions, 0, true, false));
		if(!rewritten_owner.empty()) owner = rewritten_owner;
	} catch(const PA18SubstitutionFailure&) {}
	if(owner.find("::") == string::npos) {
		const string resolved_owner = CanonicalSpelling(ResolveAlias(owner, context));
		if(!resolved_owner.empty()) owner = resolved_owner;
	}
	const TemplateDefinition* owner_definition = 0;
	vector<string> owner_arguments;
	string owner_base = owner;
	const size_t owner_open = owner.find('<');
	if(owner_open != string::npos) {
		string owner_argument_text;
		size_t owner_begin = 0, owner_close = string::npos;
		if(!TemplateBase(owner, owner_open, &owner_begin, &owner_base) ||
			!TemplateRange(owner, owner_open, &owner_argument_text, &owner_close)) return false;
		owner_arguments = SplitTemplateArguments(owner_argument_text);
		owner_definition = FindDefinition(owner_base, context);
	} else {
		map<string, string>::const_iterator generated_base = specialization_bases_.find(
			LastComponent(owner));
		map<string, vector<string> >::const_iterator generated_arguments =
			specialization_arguments_.find(LastComponent(owner));
		if(generated_base != specialization_bases_.end()) {
			owner_base = generated_base->second;
			const size_t generated_open = owner_base.find('<');
			if(generated_open != string::npos) owner_base.erase(generated_open);
		}
		if(generated_arguments != specialization_arguments_.end())
			owner_arguments = generated_arguments->second;
		owner_definition = FindDefinition(owner_base, context);
	}
	if(!owner_definition || !owner_definition->class_template) {
		owner_definition = FindDefinition(LastComponent(owner_base), context);
		if(!owner_definition || !owner_definition->class_template) return false;
	}
	const TemplateDefinition* selected_owner = SelectClassTemplateDefinition(
		owner_definition, owner_arguments, context);
	if(!selected_owner) selected_owner = owner_definition;
	CPPGMAstNodePtr declaration = FindClassDeclaration(owner, context);
	if(!declaration || declaration->children.size() <= 1)
		declaration = selected_owner->declaration;
	if(!declaration) return false;
	map<string, string> local = substitutions;
	if(!owner_definition->name.empty()) local[owner_definition->name] = owner;
	for(size_t parameter = 0; parameter < selected_owner->parameters.size() &&
		parameter < owner_arguments.size(); ++parameter)
		if(!selected_owner->parameters[parameter].name.empty())
			local[selected_owner->parameters[parameter].name] = owner_arguments[parameter];
	const TemplateDefinition* member_definition = 0;
	CPPGMAstNodePtr member_declaration;
	for(size_t child_index = 0; child_index < declaration->children.size(); ++child_index) {
		CPPGMAstNodePtr child = declaration->children[child_index];
		CPPGMAstNodePtr direct = child;
		while(direct && direct->kind == "template-declaration" && direct->children.size() > 1)
			direct = direct->children[1];
		if(!direct || direct->kind != "simple-declaration" || direct->children.empty()) continue;
		const CPPGMAstNodePtr list = ChildOfKindLocal(direct, "init-declarator-list");
		if(!list) continue;
		bool found = false;
		for(size_t item = 0; item < list->children.size(); ++item) {
			const CPPGMAstNodePtr declarator = list->children[item];
			if(declarator && !declarator->children.empty() &&
				LastComponent(FirstIdentifierLocal(declarator->children[0])) == member_base) {
				found = true;
				break;
			}
		}
		if(!found) continue;
		member_declaration = direct;
		map<const CPPGMAstNode*, TemplateDefinition>::const_iterator indexed =
			template_definitions_by_declaration_.find(direct.get());
		if(indexed != template_definitions_by_declaration_.end())
			member_definition = &indexed->second;
		break;
	}
	if(!member_declaration || !member_definition || !member_definition->variable_template)
		return false;
	size_t member_argument = 0;
	for(size_t parameter = 0; parameter < member_definition->parameters.size(); ++parameter) {
		const TemplateParameter& item = member_definition->parameters[parameter];
		if(item.pack) {
			string combined;
			while(member_argument < member_arguments.size()) {
				if(!combined.empty()) combined += ',';
				combined += member_arguments[member_argument++];
			}
			if(!item.name.empty()) local[item.name] = combined;
			continue;
		}
		string argument;
		if(member_argument < member_arguments.size()) argument = member_arguments[member_argument++];
		else if(!item.default_type.empty()) {
			argument = ReplaceIdentifiersPreservingPackSizes(item.default_type, local);
			try {
				argument = NormalizeTypeArgument(RewriteText(argument, context, local, 0));
			} catch(const PA18SubstitutionFailure&) {
				return false;
			}
		} else return false;
		if(!item.name.empty()) local[item.name] = argument;
	}
	const CPPGMAstNodePtr list = ChildOfKindLocal(member_declaration, "init-declarator-list");
	if(!list || member_declaration->children.empty()) return false;
	const string base = NodeTypeSpelling(member_declaration->children[0]);
	for(size_t item = 0; item < list->children.size(); ++item) {
		const CPPGMAstNodePtr declarator = list->children[item];
		if(!declarator || declarator->children.empty() ||
			LastComponent(FirstIdentifierLocal(declarator->children[0])) != member_base) continue;
		string type = DeclaratorTypeSpelling(base, declarator->children[0]);
		type = ReplaceIdentifiersPreservingPackSizes(type, local);
		try {
			type = RewriteText(type, context, local, 0, false, false);
		} catch(const PA18SubstitutionFailure&) {
			return false;
		}
		*result = CanonicalSpelling(ReplaceIdentifiers(type, local));
		return !result->empty();
	}
	return false;
}

bool PA18TemplateExpander::FindSourceStaticArrayOwner(
	const string& raw, const string& context, string* generated_owner)
{
	if(generated_owner) generated_owner->clear();
	const size_t separator = raw.rfind("::");
	if(separator == string::npos) return false;
	const string source_owner = raw.substr(0, separator);
	const size_t source_open = source_owner.find('<');
	const string source_base = source_open == string::npos ? source_owner :
		source_owner.substr(0, source_open);
	const TemplateDefinition* source_definition = FindDefinition(source_base, context);
	if(!source_definition || !source_definition->class_template) return false;
	CPPGMAstNodePtr declaration = FindClassDeclaration(source_owner, context);
	if(!declaration) declaration = source_definition->declaration;
	bool source_static_array = false;
	if(declaration) for(size_t child = 0;
		child < declaration->children.size() && !source_static_array; ++child) {
		CPPGMAstNodePtr item = declaration->children[child];
		while(item && item->kind == "template-declaration" && item->children.size() > 1)
			item = item->children[1];
		if(!item || item->kind != "simple-declaration" || item->children.empty() ||
			(!HasDeclarationSpecifier(item->children[0], "static") &&
			 !HasDeclarationSpecifier(item->children[0], "constexpr"))) continue;
		const CPPGMAstNodePtr list = ChildOfKindLocal(item, "init-declarator-list");
		for(size_t index = 0; list && index < list->children.size(); ++index) {
			const CPPGMAstNodePtr declarator = list->children[index];
			if(!declarator || declarator->children.empty() ||
				LastComponent(FirstIdentifierLocal(declarator->children[0])) !=
				LastComponent(raw) || DescendantOfKind(declarator, "parameter-clause") ||
				DeclaratorArraySuffix(declarator->children[0]).find_first_not_of("[]") == string::npos) continue;
			source_static_array = true;
		}
	}
	if(!source_static_array || source_open == string::npos) return source_static_array;
	string source_arguments_text;
	size_t source_close = string::npos;
	if(!TemplateRange(source_owner, source_open, &source_arguments_text, &source_close))
		return source_static_array;
	const vector<string> source_arguments = SplitTemplateArguments(source_arguments_text);
	map<string, vector<string> >::const_iterator names =
		specialization_names_by_base_.find(LastComponent(source_base));
	if(names != specialization_names_by_base_.end()) for(size_t candidate = 0;
		candidate < names->second.size() && generated_owner && generated_owner->empty(); ++candidate) {
		const string& generated = names->second[candidate];
		map<string, string>::const_iterator base = specialization_bases_.find(generated);
		map<string, vector<string> >::const_iterator arguments = specialization_arguments_.find(generated);
		if(base == specialization_bases_.end() || arguments == specialization_arguments_.end() ||
			arguments->second.size() != source_arguments.size() ||
			class_contexts_.find(generated) == class_contexts_.end() ||
			LastComponent(base->second) != LastComponent(source_base)) continue;
		bool same = true;
		for(size_t argument = 0; argument < source_arguments.size(); ++argument)
			if(NormalizeTypeArgument(RestoreSpecializationSpelling(arguments->second[argument])) !=
				NormalizeTypeArgument(source_arguments[argument])) { same = false; break; }
		if(same) *generated_owner = generated;
	}
	if(generated_owner && generated_owner->empty()) try {
		*generated_owner = Instantiate(*source_definition, source_arguments, context);
	} catch(const PA18SubstitutionFailure&) {}
	return source_static_array;
}

void PA18TemplateExpander::RecoverDependentSizeofArrayType(
	const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& result) const
{
	if(!input || input->kind != "sizeof-expression" || input->children.size() != 1 ||
		!input->children[0] || input->children[0]->kind != "type-id" ||
		result->children.size() != 1 || !result->children[0] ||
		result->children[0]->kind != "type-id" ||
		SpellNode(input->children[0]).find("::") == string::npos) return;
	CPPGMAstNodePtr transformed = result->children[0];
	for(size_t child = 0; child < transformed->children.size(); ++child)
		if(transformed->children[child] &&
			transformed->children[child]->kind == "abstract-declarator" &&
			DescendantOfKind(transformed->children[child], "array-suffix")) {
			transformed->children.erase(transformed->children.begin() + child);
			function<void(const CPPGMAstNodePtr&)> strip_extent =
				[&](const CPPGMAstNodePtr& node) {
					if(!node) return;
					if(node->kind == "type-name") {
						const size_t marker = node->value.find(':');
						const string prefix = marker == string::npos ? string() :
							node->value.substr(0, marker + 1);
						string spelling = RemoveMarker(node->value);
						const size_t extent = spelling.rfind('[');
						if(extent != string::npos && !spelling.empty() &&
							spelling[spelling.size() - 1] == ']')
							node->value = prefix + CanonicalSpelling(spelling.substr(0, extent));
					}
					for(size_t nested = 0; nested < node->children.size(); ++nested)
						strip_extent(node->children[nested]);
				};
			strip_extent(transformed);
			break;
		}
}

} // namespace pa18_templates_internal
