#include <functional>
#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

namespace {

bool HasDependentIdentifierToken(const string& text, const string& identifier)
{
	if(identifier.empty()) return false;
	for(size_t at = text.find(identifier); at != string::npos;
		at = text.find(identifier, at + identifier.size())) {
		const bool left = at == 0 || !IsIdentifierCharacter(text[at - 1]);
		const size_t end = at + identifier.size();
		if(left && (end == text.size() || !IsIdentifierCharacter(text[end]))) return true;
	}
	return false;
}

bool HasDependentTypeAlias(const TemplateDefinition& definition)
{
	if(!definition.declaration) return false;
	const auto dependent = [&definition](const string& spelling) {
		// A plain alias such as `typedef R type` can be reconstructed from the
		// concrete template binding without a class-body fixed point.  Deferring
		// those aliases turns ordinary traits (`make_`, `next`, and similar) into
		// self-referential forwards.  Only qualified dependent members need the
		// type-only shell.
		if(spelling.find("::") == string::npos &&
			spelling.find("typename") == string::npos &&
			spelling.find('<') == string::npos) return false;
		for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter)
			if(!definition.parameters[parameter].name.empty() &&
				HasDependentIdentifierToken(spelling, definition.parameters[parameter].name))
				return true;
		return false;
	};
	for(size_t child_index = 0; child_index < definition.declaration->children.size();
		++child_index) {
		CPPGMAstNodePtr child = definition.declaration->children[child_index];
		while(child && child->kind == "template-declaration" &&
			child->children.size() > 1) child = child->children[1];
		if(!child) continue;
		if(child->kind == "alias-declaration" && !child->children.empty() &&
			dependent(SpellNode(child))) return true;
		if(child->kind != "simple-declaration" || child->children.empty() ||
			!HasDeclarationSpecifier(child->children[0], "typedef")) continue;
		const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
		if(!list) continue;
		for(size_t item = 0; item < list->children.size(); ++item) {
			const CPPGMAstNodePtr declarator = list->children[item];
			if(declarator && !declarator->children.empty() &&
				dependent(SpellNode(child))) return true;
		}
	}
	return false;
}

} // namespace

bool PA18TemplateExpander::IsDirectCvQualifiedAliasTarget(
	const CPPGMAstNodePtr& declaration,
	const vector<TemplateParameter>& parameters) const
{
	if(!declaration || declaration->children.empty()) return false;
	const string target = CanonicalSpelling(TypeIdSpelling(declaration->children[0]));
	for(size_t parameter = 0; parameter < parameters.size(); ++parameter) {
		const TemplateParameter& detail = parameters[parameter];
		if(!detail.type || detail.name.empty()) continue;
		if(target == "const " + detail.name || target == detail.name + " const" ||
			target == "volatile " + detail.name ||
			target == detail.name + " volatile") return true;
	}
	return false;
}

void PA18TemplateExpander::CheckExplicitSpecializationOrder(
	const CPPGMAstNodePtr& input, const string& context)
{
	if(input->children.size() <= 1 || !input->children[0] || !input->children[1] ||
		!Parameters(input->children[0]).empty()) return;
	const CPPGMAstNodePtr declaration = input->children[1];
	if(declaration->kind == "special-member-definition" ||
		declaration->kind == "special-member-declaration") {
		const string member = CanonicalSpelling(declaration->value);
		const size_t separator = member.rfind("::");
		if(separator != string::npos) {
			const string owner = member.substr(0, separator);
			const size_t owner_open = owner.find('<');
			string owner_base, owner_arguments;
			size_t owner_begin = 0, owner_close = string::npos;
			if(owner_open != string::npos && TemplateBase(owner, owner_open,
				&owner_begin, &owner_base) && TemplateRange(owner, owner_open,
				&owner_arguments, &owner_close)) {
				const vector<string> arguments = SplitTemplateArguments(owner_arguments);
				map<string, vector<TemplateDefinition> >::const_iterator candidates =
					class_specializations_.find(owner_base);
				if(candidates != class_specializations_.end())
					for(size_t candidate_index = 0; candidate_index < candidates->second.size();
						++candidate_index) {
						const TemplateDefinition& candidate = candidates->second[candidate_index];
						if(!candidate.specialization_parameters.empty() ||
							candidate.specialization_pattern.size() != arguments.size()) continue;
						bool same = true;
						for(size_t argument = 0; argument < arguments.size(); ++argument)
							if(NormalizeTypeArgument(CanonicalSpelling(
								candidate.specialization_pattern[argument])) !=
								NormalizeTypeArgument(CanonicalSpelling(arguments[argument]))) {
								same = false;
								break;
							}
						if(same) throw logic_error(
							"member of explicit class specialization must not use template<>");
					}
			}
		}
	}
	if(declaration->kind != "class-specifier" &&
		declaration->kind != "class-forward-declaration") return;
	const string raw = CanonicalSpelling(DeclarationName(input->children[1]));
	const size_t open = raw.find('<');
	string base, argument_text;
	size_t begin = 0, close = string::npos;
	if(open == string::npos || !TemplateBase(raw, open, &begin, &base) ||
		!TemplateRange(raw, open, &argument_text, &close)) return;
	const TemplateDefinition* primary = FindDefinition(base, context);
	if(!primary || !primary->class_template) return;
	const vector<string> arguments = SplitTemplateArguments(argument_text);
	const ClassSpecializationIdentity specialization_key =
		MakeClassSpecializationIdentity(*primary, arguments, context);
	if(instantiated_class_specializations_.find(specialization_key) !=
		instantiated_class_specializations_.end())
		throw logic_error("explicit specialization after instantiation");
}

namespace {

string GeneratedOrderingDeclaredName(const CPPGMAstNodePtr& node)
{
	if(!node) return string();
	if(node->kind == "class-specifier" || node->kind == "class-forward-declaration" ||
		node->kind == "enum-specifier" || node->kind == "alias-declaration")
		return LastComponent(node->value);
	if(node->kind != "simple-declaration") return string();
	const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
	if(!list || list->children.empty() || !list->children[0] ||
		list->children[0]->children.empty()) return string();
	return LastComponent(FirstIdentifierLocal(list->children[0]->children[0]));
}

bool GeneratedOrderingTypeDeclaration(const CPPGMAstNodePtr& node)
{
	if(!node) return false;
	if(node->kind == "class-specifier" || node->kind == "class-forward-declaration" ||
		node->kind == "enum-specifier" || node->kind == "alias-declaration") return true;
	return node->kind == "simple-declaration" && !node->children.empty() &&
		SpellNode(node->children[0]).find("typedef") != string::npos;
}

void MarkStaticGeneratedFunction(const CPPGMAstNodePtr& node)
{
	if(!node || node->kind != "function-definition" || node->children.empty() ||
		!node->children[0]) return;
	if(HasDeclarationSpecifier(node->children[0], "static")) return;
	node->children[0]->children.insert(node->children[0]->children.begin(),
		CPPGMAstNodePtr(new CPPGMAstNode("decl-specifier", "KW_STATIC:static")));
}

} // namespace

void PA18TemplateExpander::AdjustGeneratedFunctionPosition(
	const vector<CPPGMAstNodePtr>& generated,
	const vector<CPPGMAstNodePtr>& children, size_t* position) const
{
	if(!position) return;
	for(size_t child = 0; child < children.size(); ++child) {
		if(!children[child]) continue;
		for(size_t function = 0; function < generated.size(); ++function) {
			const string name = DeclarationName(generated[function]);
			const string primary = generated[function] ? LastComponent(
				generated[function]->template_primary) : string();
			if((!name.empty() && ContainsName(children[child], name)) ||
				(!primary.empty() && ContainsName(children[child], primary)))
				*position = min(*position, child);
		}
	}
	for(size_t child = 0; child < children.size(); ++child) {
		if(!GeneratedOrderingTypeDeclaration(children[child])) continue;
		const string declared = GeneratedOrderingDeclaredName(children[child]);
		if(declared.empty()) continue;
		for(size_t function = 0; function < generated.size(); ++function) {
			const string primary = generated[function] ? LastComponent(
				generated[function]->template_primary) : string();
			if(primary == declared || MentionsGeneratedType(generated[function], declared))
				*position = max(*position, child + 1);
		}
	}
}

void PA18TemplateExpander::RewriteInlineGeneratedNames(
	const CPPGMAstNodePtr& node, const string& logical_owner,
	const string& physical_owner)
{
	if(!node || logical_owner.empty() || physical_owner.empty() ||
		logical_owner == physical_owner) return;
	set<string> names;
	for(map<string, TemplateDefinition>::const_iterator definition = definitions_.begin();
		definition != definitions_.end(); ++definition) {
		const TemplateDefinition& value = definition->second;
		const string lexical_owner = value.lexical_owner.empty() ? value.owner :
			value.lexical_owner;
		if(value.owner == logical_owner && lexical_owner == physical_owner)
			names.insert(LastComponent(value.name));
	}
	map<string, vector<CPPGMAstNodePtr> >::const_iterator generated =
		generated_by_owner_.find(physical_owner);
	if(generated != generated_by_owner_.end()) for(size_t i = 0; i < generated->second.size(); ++i) {
		const CPPGMAstNodePtr& value = generated->second[i];
		if(value) {
			const string name = LastComponent(value->value);
			if(!name.empty()) names.insert(name);
			const string primary = LastComponent(value->template_primary);
			if(!primary.empty()) names.insert(primary);
		}
	}
	if(names.empty()) return;
	std::function<void(const CPPGMAstNodePtr&)> rewrite =
		[&](const CPPGMAstNodePtr& current) {
		if(!current) return;
		for(set<string>::const_iterator name = names.begin(); name != names.end(); ++name) {
			const string from = logical_owner + "::" + *name;
			const string to = physical_owner + "::" + *name;
			for(size_t at = current->value.find(from); at != string::npos;
				at = current->value.find(from, at + to.size())) {
				if((at == 0 || !IsIdentifierCharacter(current->value[at - 1])) &&
					(at + from.size() == current->value.size() ||
						!IsIdentifierCharacter(current->value[at + from.size()])))
					current->value.replace(at, from.size(), to);
			}
		}
		for(size_t child = 0; child < current->children.size(); ++child)
			rewrite(current->children[child]);
		};
	rewrite(node);
}

void PA18TemplateExpander::RestoreGeneratedMemberParameterNames(
	const TemplateDefinition& definition,
	const TemplateDefinition* member_owner_definition,
	const CPPGMAstNodePtr& generated)
{
	if(generated->kind != "function-definition") return;
	const CPPGMAstNodePtr source_declarator = FunctionDeclarator(definition.declaration);
	const CPPGMAstNodePtr source_clause = DescendantOfKind(source_declarator,
		"parameter-clause");
	const string member_name = LastComponent(definition.name);
	CPPGMAstNodePtr owner_member;
	function<CPPGMAstNodePtr(const CPPGMAstNodePtr&)> find_owner_member =
		[&](const CPPGMAstNodePtr& node) -> CPPGMAstNodePtr {
			if(!node) return CPPGMAstNodePtr();
			if(node->kind == "simple-declaration" ||
				node->kind == "function-definition") {
				const CPPGMAstNodePtr declarator = FunctionDeclarator(node);
				const CPPGMAstNodePtr clause = DescendantOfKind(declarator,
					"parameter-clause");
				if(declarator && clause && LastComponent(
					FirstIdentifierLocal(declarator)) == member_name && source_clause &&
					clause->children.size() == source_clause->children.size()) {
					bool same_signature = true;
					for(size_t parameter = 0; parameter < clause->children.size(); ++parameter) {
						if(!clause->children[parameter] || !source_clause->children[parameter] ||
							CanonicalSpelling(ParameterTypeSpelling(clause->children[parameter])) !=
							CanonicalSpelling(ParameterTypeSpelling(source_clause->children[parameter]))) {
							same_signature = false;
							break;
						}
					}
					if(same_signature) return declarator;
				}
			}
			for(size_t child = 0; child < node->children.size(); ++child) {
				CPPGMAstNodePtr found = find_owner_member(node->children[child]);
				if(found) return found;
			}
			return CPPGMAstNodePtr();
		};
	const CPPGMAstNodePtr owner_declaration = member_owner_definition ?
		member_owner_definition->declaration : FindClassDeclaration(
		definition.owner, definition.owner);
	if(!owner_declaration) return;
	owner_member = find_owner_member(owner_declaration);
	const CPPGMAstNodePtr generated_clause = DescendantOfKind(
		FunctionDeclarator(generated), "parameter-clause");
	const CPPGMAstNodePtr owner_clause = owner_member ? DescendantOfKind(
		owner_member, "parameter-clause") : CPPGMAstNodePtr();
	const bool preserve_unnamed_parameters = definition.owner.find("::") != string::npos &&
		definition.owner.find('<') != string::npos;
	if(owner_clause && generated_clause && owner_clause->children.size() ==
		generated_clause->children.size()) for(size_t parameter = 0;
		parameter < generated_clause->children.size(); ++parameter) {
		if(!source_clause || !source_clause->children[parameter] ||
			(ParameterIdentifier(source_clause->children[parameter]).empty() &&
				preserve_unnamed_parameters)) continue;
		const string source_name = ParameterIdentifier(source_clause->children[parameter]);
		const string owner_name = source_name.empty() ?
			ParameterIdentifier(owner_clause->children[parameter]) : source_name;
		if(owner_name.empty() || !generated_clause->children[parameter]) continue;
		const string generated_name = ParameterIdentifier(generated_clause->children[parameter]);
		if(!generated_name.empty() && generated_name.compare(0, 7, "__param") != 0) continue;
		CPPGMAstNodePtr declarator = ChildOfKindLocal(
			generated_clause->children[parameter], "declarator");
		if(!declarator) {
			declarator.reset(new CPPGMAstNode("declarator"));
			generated_clause->children[parameter]->children.push_back(declarator);
		}
		if(generated_name.empty()) declarator->children.push_back(CPPGMAstNodePtr(
			new CPPGMAstNode("identifier", owner_name)));
		else RenameParameterIdentifier(declarator, owner_name);
	}
}

bool PA18TemplateExpander::HasUnavailableGeneratedMemberType(string raw,
	const string& context, const map<string, string>& substitutions) const
{
	raw = CanonicalSpelling(RemoveMarker(raw));
	if(raw.empty() || HasUnresolvedTemplateParameter(raw, context, substitutions)) return false;
	// This probe is itself used to decide whether a class body can be deferred.
	// Resolving the owner may therefore revisit the same probe while materializing
	// a nested member template.  Treat the re-entry as an unavailable member and
	// let the enclosing candidate defer/fail at its normal substitution boundary;
	// otherwise the type-only query recursively manufactures the same owner.
	const string query_key = raw + "|" + context;
	if(!active_unavailable_member_type_queries_.insert(query_key).second) return true;
	struct UnavailableMemberQueryScope {
		set<string>* active;
		string key;
		UnavailableMemberQueryScope(set<string>* value, const string& name)
			: active(value), key(name) {}
		~UnavailableMemberQueryScope() { active->erase(key); }
	} query_scope(&active_unavailable_member_type_queries_, query_key);
	const size_t separator = TopLevelScopeSeparator(raw);
	if(separator == string::npos) return false;
	const string owner = raw.substr(0, separator);
	const string member = raw.substr(separator + 2);
	// A member-template specialization is a valid typed owner even before its
	// generated class shell has been indexed.  Recover the source owner directly
	// so the fixed-point check does not defer a callable whose `impl` body can be
	// replayed normally.
	const size_t owner_separator = TopLevelScopeSeparator(owner);
	if(owner_separator != string::npos) {
		const string outer_owner = owner.substr(0, owner_separator);
		string nested_owner = owner.substr(owner_separator + 2);
		const size_t nested_open = nested_owner.find('<');
		if(nested_open != string::npos) nested_owner.erase(nested_open);
		const TemplateDefinition* outer_definition = FindDefinition(outer_owner, context);
		if(!outer_definition) {
			map<string, string>::const_iterator generated = specialization_bases_.find(
				LastComponent(outer_owner));
			if(generated != specialization_bases_.end())
				outer_definition = FindDefinition(generated->second, context);
		}
		if(outer_definition && FindNestedDefinition(*outer_definition,
			LastComponent(nested_owner))) return false;
	}
	// Only a qualified member of a known type can be a dependent member probe.
	// Namespace-qualified ordinary types such as `regex_constants::enum_value`
	// must remain on the normal function-template path.
	if(!IsKnownTypeSpelling(owner, context)) {
		// A source template-id with a qualified member (notably a member
		// template such as `C<T>::impl<U>`) can be known only after its enclosing
		// replay is complete.  Keep that query in the deferred fixed point rather
		// than treating the recursive lookup as an ordinary namespace name.
		return owner.find('<') != string::npos;
	}
	// A generated alias may expose a qualified member that is neither a
	// materialized specialization nor a source class member.  Treat that as
	// substitution failure before RegisterGeneratedTypeEntity publishes the
	// failed alias.  This is the SFINAE boundary for probes such as
	// `typename E1::missing`; leaving the node registered would make PA11
	// diagnose the discarded candidate as a top-level unknown type.
	const TemplateDefinition* complete_type = FindDefinition(raw, context);
	if((complete_type && (complete_type->class_template ||
		complete_type->alias_template || complete_type->variable_template)) ||
		type_aliases_.find(raw) != type_aliases_.end() ||
		class_declarations_.find(raw) != class_declarations_.end()) {
		return false;
	}
	const string nested_entity = JoinPath(owner, member);
	if(class_contexts_.find(nested_entity) != class_contexts_.end() ||
		class_declarations_.find(nested_entity) != class_declarations_.end()) {
		return false;
	}
	string member_type;
	set<string> active_members;
	if(FindClassMemberType(owner, member, substitutions, context,
		&member_type, &active_members, false) && !member_type.empty()) {
		return false;
	}
	if(!MemberAliasType(owner, member).empty()) {
		return false;
	}
	return true;
}

bool PA18TemplateExpander::MentionsGeneratedTypeOutsideFunctionBodies(
	const CPPGMAstNodePtr& node, const string& type_name) const
{
	if(!node || type_name.empty() || node->kind == "compound-statement" ||
		node->kind == "function-definition" || node->kind == "special-member-definition" ||
		node->kind == "special-member-declaration") return false;
	const string wanted = LastComponent(CanonicalSpelling(RemoveMarker(type_name)));
	if((node->kind == "class-specifier" || node->kind == "class-forward-declaration") &&
		LastComponent(CanonicalSpelling(RemoveMarker(node->value))) == wanted) return false;
	const string spelling = CanonicalSpelling(RemoveMarker(node->value));
	if(!spelling.empty()) {
		const size_t angle = spelling.find('<');
		const string bare = angle == string::npos ? spelling : spelling.substr(0, angle);
		if(bare == type_name || bare == wanted || bare.compare(0, wanted.size() + 2,
			wanted + "::") == 0 || bare.find("::" + wanted + "::") != string::npos ||
			(bare.size() > wanted.size() + 2 && bare.compare(bare.size() - wanted.size() - 2,
			wanted.size() + 2, "::" + wanted) == 0)) return true;
		for(size_t position = spelling.find(wanted); position != string::npos;
			position = spelling.find(wanted, position + wanted.size())) {
			const bool left = position == 0 || !IsIdentifierCharacter(spelling[position - 1]);
			const size_t end = position + wanted.size();
			if(left && (end == spelling.size() || !IsIdentifierCharacter(spelling[end]))) return true;
		}
	}
	for(size_t child = 0; child < node->children.size(); ++child)
		if(MentionsGeneratedTypeOutsideFunctionBodies(node->children[child], type_name)) return true;
	return false;
}

bool PA18TemplateExpander::HasDeferredDependentClassMember(
	const TemplateDefinition& definition, const string& context,
	const map<string, string>& substitutions) const
{
	for(size_t node = 0; node < definition.dependent_member_type_nodes.size(); ++node) {
		const CPPGMAstNodePtr& candidate = definition.dependent_member_type_nodes[node];
		map<string, string> protected_substitutions = substitutions;
		ProtectMaterializedTemplateBases(RemoveMarker(candidate->value), context,
			substitutions, &protected_substitutions);
		string raw = CanonicalSpelling(ReplaceIdentifiersPreservingPackSizes(
			RemoveMarker(candidate->value), protected_substitutions));
		if(raw.compare(0, 8, "typename") == 0 &&
			(raw.size() == 8 || isspace(static_cast<unsigned char>(raw[8]))))
			raw = CanonicalSpelling(raw.substr(8));
		if(HasUnavailableGeneratedMemberType(raw, context, substitutions)) return true;
	}
	return false;
}

bool PA18TemplateExpander::HasDeferredTypeMember(
	const TemplateDefinition& definition, const string& context,
	const map<string, string>& substitutions) const
{
	for(size_t node = 0; node < definition.dependent_type_member_nodes.size(); ++node) {
		const CPPGMAstNodePtr& candidate = definition.dependent_type_member_nodes[node];
		map<string, string> protected_substitutions = substitutions;
		ProtectMaterializedTemplateBases(RemoveMarker(candidate->value), context,
			substitutions, &protected_substitutions);
		string raw = CanonicalSpelling(ReplaceIdentifiersPreservingPackSizes(
			RemoveMarker(candidate->value), protected_substitutions));
		if(raw.compare(0, 8, "typename") == 0 &&
			(raw.size() == 8 || isspace(static_cast<unsigned char>(raw[8]))))
			raw = CanonicalSpelling(raw.substr(8));
		if(HasUnavailableGeneratedMemberType(raw, context, substitutions)) return true;
	}
	return false;
}

bool PA18TemplateExpander::GeneratedNodeHasUnavailableMemberType(
	const CPPGMAstNodePtr& node, const string& context,
	const map<string, string>& substitutions) const
{
	// Function bodies are not part of template argument substitution or
	// overload viability.  In particular, a qualified call inside a replayed
	// body can be parsed as a type-shaped member expression even though its
	// explicitly specialized target is valid; probing it here would discard the
	// enclosing function before the body is replayed.
	if(!node || node->kind == "compound-statement") return false;
	if(node->kind == "decl-specifier" || node->kind == "type-name" ||
		node->kind == "type-specifier" || node->kind == "decltype-specifier" ||
		node->kind == "base-name") {
		const string probe = CanonicalSpelling(ReplaceIdentifiersPreservingPackSizes(
			RemoveMarker(node->value), substitutions));
		if(HasUnavailableGeneratedMemberType(probe, context, substitutions)) return true;
	}
	for(size_t child = 0; child < node->children.size(); ++child)
		if(GeneratedNodeHasUnavailableMemberType(node->children[child], context, substitutions)) return true;
	return false;
}


void PA18TemplateExpander::RegisterGeneratedTypeEntity(
	const TemplateDefinition& definition, const CPPGMAstNodePtr& generated,
	const string& generated_owner, const string& local_name,
	const string& concrete_owner, const map<string, string>& substitutions,
	const vector<string>& args,
		const map<string, vector<string> >& pack_substitutions,
		const string& context, bool explicit_instantiation)
{
	RegisterGeneratedAliasEntity(definition, generated, generated_owner, local_name,
		concrete_owner, substitutions, args, pack_substitutions, context);
	if(!definition.class_template) return;
	const string generated_path = JoinPath(GeneratedOwner(definition), local_name);
	class_declarations_[generated_path] = generated;
	RememberClassPath(generated_path);
	if(generated->kind == "class-specifier" ||
		generated->kind == "class-forward-declaration") {
		const string previous_constant_owner = active_instantiation_name_;
		active_instantiation_name_.clear();
		for(size_t child = 0; child < generated->children.size(); ++child)
			if(generated->children[child] &&
				generated->children[child]->kind == "simple-declaration")
				RecordConstantDeclaration(generated->children[child], generated_path,
					substitutions);
		active_instantiation_name_ = previous_constant_owner;
	}
	// Generated nested class specializations have concrete array bounds and
	// therefore a layout that was unavailable while collecting the dependent
	// primary.  Record that typed layout under the generated identity so a
	// later unevaluated call such as sizeof(test<T>(0)) can use its result.
	RecordClassTypeSize(generated, generated_path, generated_path);
	set<string> generated_static_members;
	IndexStaticMembers(generated, generated_static_members);
	static_members_by_class_[generated_path] = generated_static_members;
	const string lexical_path = JoinPath(generated_owner, local_name);
	class_declarations_[lexical_path] = generated;
	RememberClassPath(lexical_path);
	RecordClassTypeSize(generated, lexical_path, lexical_path);
	static_members_by_class_[lexical_path] = generated_static_members;
	const string previous_instantiation_name = active_instantiation_name_;
	active_instantiation_name_.clear();
	RegisterGeneratedConstants(generated, generated_path);
	active_instantiation_name_ = previous_instantiation_name;
	if(!concrete_owner.empty()) {
		const string concrete_path = JoinPath(concrete_owner, local_name);
		class_declarations_[concrete_path] = generated;
		static_members_by_class_[concrete_path] = generated_static_members;
		RememberClassPath(concrete_path);
		RecordClassTypeSize(generated, concrete_path, concrete_path);
		active_instantiation_name_.clear();
		RegisterGeneratedConstants(generated, concrete_path);
		active_instantiation_name_ = previous_instantiation_name;
	}
	const map<string, vector<string> > previous_packs = active_pack_substitutions_;
	// A member template is replayed inside the enclosing class specialization.
	// Keep the enclosing typed packs visible while installing the member's own
	// packs; otherwise an expansion such as `is_convertible<U, T>...` loses the
	// class pack `T` and unequal pack lengths are incorrectly accepted.
	for(map<string, vector<string> >::const_iterator pack = pack_substitutions.begin();
		pack != pack_substitutions.end(); ++pack)
		active_pack_substitutions_[pack->first] = pack->second;
	try {
		InstantiateRequestedNestedClasses(definition, args, local_name, context);
		InstantiateMemberDefinitions(definition, args, local_name, explicit_instantiation);
	} catch(...) {
		active_pack_substitutions_ = previous_packs;
		throw;
	}
	active_pack_substitutions_ = previous_packs;
}

string PA18TemplateExpander::EmitInstantiation(const TemplateDefinition& definition,
	const vector<string>& args, const vector<string>& metadata_args,
	map<string, string> substitutions,
	const map<string, PA19IntegralValue>& integral_substitutions,
	const map<string, vector<string> >& pack_substitutions, const string& context,
	bool explicit_instantiation, const string& key, const string& local_name,
	const string& requested_owner,
		const map<string, FunctionSignature>& function_substitutions)
{
	const string generated_owner = definition.lexical_owner.empty() ?
		definition.owner : definition.lexical_owner;
	// Dependent declarations are collected without ordinary semantic traversal;
	// retain their elaborated class dependencies before body replay normalizes
	// the source spelling.
	EnsureTemplateDeclarationDependencies(definition, generated_owner);
	const bool static_member = definition.static_member;
	const bool flattened_static_member = static_member &&
		definition.owner.find("::") == string::npos && !requested_owner.empty();
	const bool free_generated_member = definition.friend_declaration;
	string member_owner_name = definition.owner;
	// Collection records an unqualified friend in the lexical class context.
	// A class-template replay can contribute that class component twice
	// (`C::C::operator==`).  Normalize the enclosing class before deriving the
	// namespace owner; otherwise the materialized friend is queued as a member
	// of `C` instead of as a namespace-scope function.
	if(free_generated_member) {
		const string parent_owner = PrefixComponent(member_owner_name);
		if(!parent_owner.empty() && LastComponent(member_owner_name) ==
			LastComponent(parent_owner)) member_owner_name = parent_owner;
	}
	const string entity_owner = free_generated_member ? PrefixComponent(member_owner_name) :
		generated_owner;
	const size_t member_owner_angle = member_owner_name.find('<');
	if(member_owner_angle != string::npos) member_owner_name.erase(member_owner_angle);
	const TemplateDefinition* member_owner_definition = member_owner_name.empty() ? 0 :
		FindDefinition(member_owner_name, context);
	if(!member_owner_definition && !member_owner_name.empty())
		member_owner_definition = FindDefinition(LastComponent(member_owner_name), context);
	const bool member_definition = !definition.owner.empty() &&
		((member_owner_definition && member_owner_definition->class_template) ||
		 class_contexts_.find(member_owner_name) != class_contexts_.end() ||
		 FindClassDeclaration(member_owner_name, context));
	const bool ordinary_class_member = member_definition &&
		member_owner_name.find('<') == string::npos &&
		FindClassDeclaration(member_owner_name, context) != CPPGMAstNodePtr() &&
		!definition.member_template &&
		LastComponent(definition.name).compare(0, 8, "operator") != 0 &&
		!free_generated_member;
	string concrete_owner = definition.class_template ?
		FindConcreteInstantiationOwner(definition, substitutions, context, requested_owner) :
		((member_definition && ConcreteOwnerMatches(definition, requested_owner)) ||
		 (definition.alias_template && ConcreteOwnerMatches(definition, requested_owner)) ?
		 requested_owner : string());
	if(free_generated_member) concrete_owner.clear();
	// A hidden friend is emitted at namespace scope, but its return type still
	// names the current specialization of the declaring class.  The call-site
	// substitution map normally contains only the friend's own template
	// parameters (for example `AnyExecutor`); recover the enclosing class's
	// typed owner from those values before replaying the friend body.
	if(free_generated_member) {
		const string source_owner = member_owner_name;
		const string source_name = LastComponent(source_owner);
		for(map<string, string>::const_iterator substitution = substitutions.begin();
			substitution != substitutions.end(); ++substitution) {
			const string candidate = CanonicalSpelling(substitution->second);
			map<string, string>::const_iterator candidate_base =
				specialization_bases_.find(LastComponent(candidate));
			if(candidate_base == specialization_bases_.end() ||
				LastComponent(candidate_base->second) != source_name) continue;
			AddConcreteOwnerSubstitutions(candidate, context, &substitutions, true);
			break;
		}
	}
	// A generated class can be reached through the lexical function path used
	// while replaying a functional cast (for example
	// `make_pair_int::pair_int_`), while PA14 indexes its declarations by the
	// materialized class identity (`pair_int_`).  Keep the typed specialization
	// owner canonical before registering generated member declarations.
	if(!concrete_owner.empty() && specialization_bases_.find(
		LastComponent(concrete_owner)) != specialization_bases_.end()) {
		string source_owner = definition.owner;
		const size_t source_angle = source_owner.find('<');
		if(source_angle != string::npos) source_owner.erase(source_angle);
		const string source_namespace = PrefixComponent(PrefixComponent(source_owner));
		const string generated_namespace = PrefixComponent(generated_owner);
		string matched_context;
		if(!source_namespace.empty())
			for(set<string>::const_iterator candidate = class_contexts_.begin();
				candidate != class_contexts_.end(); ++candidate)
				if(LastComponent(*candidate) == LastComponent(concrete_owner) &&
					PrefixComponent(*candidate) == source_namespace) {
					matched_context = *candidate;
					break;
				}
		if(!matched_context.empty()) concrete_owner = matched_context;
		else if(class_contexts_.find(LastComponent(concrete_owner)) != class_contexts_.end())
			concrete_owner = LastComponent(concrete_owner);
		else for(set<string>::const_iterator candidate = class_contexts_.begin();
			candidate != class_contexts_.end(); ++candidate)
			if(LastComponent(*candidate) == LastComponent(concrete_owner) &&
				(generated_namespace.empty() || PrefixComponent(*candidate) == generated_namespace)) {
				concrete_owner = *candidate;
				break;
			}
	}
	if(definition.class_template) {
		RememberClassPath(JoinPath(GeneratedOwner(definition), local_name));
		RememberClassPath(JoinPath(generated_owner, local_name));
	}
	const string previous_instantiation_name = active_instantiation_name_;
	const string enclosing_instantiation_name = active_instantiation_name_;
	const bool previous_static_member = active_static_member_;
	active_instantiation_name_ = definition.class_template ? local_name : string();
	active_static_member_ = member_definition && definition.static_member;
	const size_t previous_type_only_depth = defer_type_only_class_definitions_;
	defer_type_only_class_definitions_ = 0;
	CPPGMAstNodePtr generated;
	const string transform_context = concrete_owner.empty() ? definition.owner :
		concrete_owner;
	InstallImplicitNestedForwards(definition, CPPGMAstNodePtr(), generated_owner,
		local_name);
	try {
		generated = TransformInstantiatedNode(definition, transform_context,
			substitutions, integral_substitutions, pack_substitutions,
			function_substitutions);
	} catch(const PA18SubstitutionFailure& failure) {
		active_instantiation_name_ = previous_instantiation_name;
		active_static_member_ = previous_static_member;
		defer_type_only_class_definitions_ = previous_type_only_depth;
		throw;
	} catch(...) {
		active_instantiation_name_ = previous_instantiation_name;
		active_static_member_ = previous_static_member;
		defer_type_only_class_definitions_ = previous_type_only_depth;
		throw;
	}
	active_instantiation_name_ = previous_instantiation_name;
	active_static_member_ = previous_static_member;
	defer_type_only_class_definitions_ = previous_type_only_depth;
	if(!generated) throw logic_error("unable to instantiate template");
	if(definition.class_template && !definition.parameters.empty() &&
		definition.parameters.back().pack && args.size() < definition.parameters.size())
		generated->template_empty_pack = true;
	if(definition.friend_declaration) {
		// Friend definitions are emitted at namespace scope.  A parameter that
		// names a nested class is nevertheless looked up in the declaring class
		// in its source form; preserve that association in the detached AST.
		const CPPGMAstNodePtr friend_clause = DescendantOfKind(
			FunctionDeclarator(generated), "parameter-clause");
		string friend_context = definition.lexical_owner.empty() ? definition.owner :
			definition.lexical_owner;
		if(friend_clause) for(size_t parameter = 0; parameter < friend_clause->children.size();
			++parameter) {
			const CPPGMAstNodePtr parameter_node = friend_clause->children[parameter];
			if(!parameter_node || parameter_node->kind != "parameter-declaration" ||
				parameter_node->children.empty()) continue;
			string raw = CanonicalSpelling(ParameterTypeSpelling(parameter_node));
			while(raw.compare(0, 6, "const ") == 0)
				raw = CanonicalSpelling(raw.substr(6));
			while(raw.compare(0, 9, "volatile ") == 0)
				raw = CanonicalSpelling(raw.substr(9));
			while(!raw.empty() && (raw[raw.size() - 1] == '&' ||
				raw[raw.size() - 1] == '*')) raw.erase(raw.size() - 1);
			raw = CanonicalSpelling(raw);
			const size_t raw_open = raw.find('<');
			const string raw_base = raw_open == string::npos ? raw : raw.substr(0, raw_open);
			if(raw_base.empty() || raw_base.find("::") != string::npos) continue;
			string qualified_base;
			for(string current = friend_context; ; ) {
				const string candidate = JoinPath(current, raw_base);
				if(class_declarations_.find(candidate) != class_declarations_.end() ||
					class_contexts_.find(candidate) != class_contexts_.end()) {
					qualified_base = candidate;
					break;
				}
				if(current.empty()) break;
				const size_t separator = current.rfind("::");
				if(separator == string::npos) current.clear();
				else current.erase(separator);
			}
			if(qualified_base.empty()) continue;
			const CPPGMAstNodePtr specifiers = parameter_node->children[0];
			if(!specifiers || specifiers->kind != "decl-specifier-seq") continue;
			for(size_t specifier = 0; specifier < specifiers->children.size(); ++specifier) {
				const CPPGMAstNodePtr type = specifiers->children[specifier];
				if(!type || type->kind != "decl-specifier") continue;
				const string marker = type->value.find(':') == string::npos ? string() :
					type->value.substr(0, type->value.find(':') + 1);
				const string spelling = RemoveMarker(type->value);
				if(spelling != raw_base && (raw_open == string::npos ||
					spelling.compare(0, raw_base.size(), raw_base) != 0)) continue;
				type->value = marker + qualified_base + (raw_open == string::npos ?
					string() : raw.substr(raw_open));
			}
		}
	}
	// The replayed friend is detached from its declaring class and queued at
	// namespace scope.  Keep the source friend marker for class-scope hidden
	// lookup, but remove it from the materialized free definition so PA14
	// collects the generated function as an ordinary namespace declaration.
	if(definition.friend_declaration && generated->kind == "function-definition" &&
		!generated->children.empty() && generated->children[0] &&
		generated->children[0]->kind == "decl-specifier-seq") {
		CPPGMAstNodePtr specifiers = generated->children[0];
		for(size_t specifier = 0; specifier < specifiers->children.size();) {
			const CPPGMAstNodePtr item = specifiers->children[specifier];
			if(item && item->kind == "decl-specifier" &&
				(RemoveMarker(item->value) == "friend" || item->value == "friend"))
				specifiers->children.erase(specifiers->children.begin() + specifier);
			else ++specifier;
		}
	}
	if(!definition.class_template && !definition.variable_template &&
		GeneratedNodeHasUnavailableMemberType(
		generated, transform_context, substitutions)) {
		throw PA18SubstitutionFailure("dependent type substitution failed");
	}
	// Function parameter arrays are adjusted to pointers by [dcl.fct].  Keep
	// the source declarator's array spelling while deduction is running (the
	// bound may itself be a template argument), then materialize the adjusted
	// typed declarator on the generated function.  A nested declarator denotes
	// a reference or pointer to an array and must remain unchanged.
	if(generated->kind == "function-definition" || generated->kind == "simple-declaration") {
		const CPPGMAstNodePtr generated_declarator = FunctionDeclarator(generated);
		const CPPGMAstNodePtr generated_clause = DescendantOfKind(
			generated_declarator, "parameter-clause");
		if(generated_clause) for(size_t parameter = 0;
			parameter < generated_clause->children.size(); ++parameter) {
			const CPPGMAstNodePtr item = generated_clause->children[parameter];
			if(!item || item->kind != "parameter-declaration" || item->children.size() < 2 ||
				!item->children[1]) continue;
			const CPPGMAstNodePtr declarator = item->children[1];
			bool nested = false;
			for(size_t child = 0; child < declarator->children.size(); ++child)
				if(declarator->children[child] &&
					declarator->children[child]->kind == "nested-declarator") {
					nested = true;
					break;
				}
			if(nested) continue;
			for(size_t child = 0; child < declarator->children.size(); ++child)
				if(declarator->children[child] &&
					declarator->children[child]->kind == "array-suffix") {
					declarator->children.erase(declarator->children.begin() + child);
					declarator->children.insert(declarator->children.begin(),
						CPPGMAstNodePtr(new CPPGMAstNode("ptr-operator", "OP_STAR:*")));
					break;
				}
		}
	}
	if(member_definition)
		RestoreGeneratedMemberParameterNames(definition, member_owner_definition, generated);
	if(member_definition && !definition.class_template &&
		HasStaticMember(member_owner_definition, member_owner_name,
			LastComponent(definition.name)))
		MarkStaticGeneratedFunction(generated);
	MarkGeneratedNode(generated, definition.qualified_name, metadata_args,
		explicit_instantiation, definition.explicit_specialization);
	if(generated->kind == "simple-declaration") {
		const CPPGMAstNodePtr identifier = DescendantOfKind(generated, "identifier");
		if(identifier) identifier->value = local_name;
		RecordConstantDeclaration(generated, generated_owner);
	}
	if(definition.class_template)
		generated->dependent_base_lookup = DefinitionHasDependentBase(definition);
	const string generated_definition_name = LastComponent(definition.name);
	const bool operator_member = generated_definition_name.compare(0, 8,
		"operator") == 0;
	const bool conversion_operator = operator_member && generated_definition_name.size() > 8 &&
		(IsIdentifierCharacter(generated_definition_name[8]) ||
		 generated_definition_name[8] == ' ');
	const bool generated_special_member = generated->kind == "special-member-definition" ||
		generated->kind == "special-member-declaration";
	const bool explicit_static_data = definition.explicit_specialization &&
		definition.variable_template && generated->kind == "simple-declaration";
	const bool explicit_member_definition = definition.explicit_specialization &&
		member_definition && !definition.class_template && !explicit_static_data &&
		member_owner_definition;
	if(!definition.class_template && !definition.alias_template &&
		(!member_definition || (concrete_owner.empty() && !ordinary_class_member)) &&
		!generated_special_member)
		RenameGeneratedFunction(generated, local_name);
	if(definition.class_template || definition.alias_template) generated->value = local_name;
	RegisterGeneratedTypeEntity(definition, generated, generated_owner, local_name,
		concrete_owner, substitutions, args, pack_substitutions, context,
		explicit_instantiation);
	InstallImplicitNestedForwards(definition, generated, generated_owner, local_name);
	EnsureDeclarationDependencies(generated, definition.owner, generated_owner);
	if(definition.class_template)
		RecordTemplateArrayValues(definition, args, context, substitutions,
			pack_substitutions);
	// Text replay can encounter the same member template-id while resolving a
	// dependent type and give the detached definition its standalone cache name.
	// Once the entity is owned by a concrete class, the declaration must retain
	// the member spelling so class-scope lookup can bind it.
	if(member_definition && !concrete_owner.empty()) {
		const bool special_member = generated->kind == "special-member-definition" ||
			generated->kind == "special-member-declaration";
		const bool constructor_special_member = special_member && !conversion_operator;
		if(constructor_special_member)
			generated->value = concrete_owner + "::" + LastComponent(concrete_owner);
		if(!explicit_member_definition)
			RenameGeneratedFunction(generated, constructor_special_member ?
				LastComponent(concrete_owner) : definition.member_template ? local_name :
				operator_member ? local_name : LastComponent(definition.name));
		if(conversion_operator) {
			const CPPGMAstNodePtr conversion_declarator = FunctionDeclarator(generated);
			const CPPGMAstNodePtr conversion_identifier = DescendantOfKind(
				conversion_declarator, "identifier");
			const string generated_conversion_name = LastComponent(generated->value);
			if(conversion_identifier && generated_conversion_name.compare(0, 8,
				"operator") == 0)
				conversion_identifier->value = generated_conversion_name;
		}
		if(definition.explicit_specialization && generated->kind == "simple-declaration") {
			const CPPGMAstNodePtr identifier = DescendantOfKind(generated, "identifier");
			if(identifier)
				identifier->value = concrete_owner + "::" + LastComponent(definition.name);
		}
	}
	for(size_t i = 0; i < args.size(); ++i)
		EnsureForwardClass(args[i], context, generated_owner);
	bool recursive_context_argument = false;
	for(size_t i = 0; i < args.size(); ++i)
		if(LastComponent(args[i]) == LastComponent(context) && !context.empty())
			recursive_context_argument = true;
	const string definition_owner = definition.lexical_owner.empty() ? definition.owner :
		definition.lexical_owner;
	const bool owner_is_context_ancestor = definition_owner.empty() || context == definition_owner ||
		(context.size() > definition_owner.size() && context.compare(0,
			definition_owner.size(), definition_owner) == 0 &&
			context[definition_owner.size()] == ':');
	if(!explicit_static_data && !definition.owner.empty() &&
		class_contexts_.find(definition_owner) != class_contexts_.end() &&
		class_contexts_.find(context) != class_contexts_.end() && owner_is_context_ancestor &&
		(!definition.class_template || !definition.owner.empty()) &&
		context != definition.owner &&
		!recursive_context_argument) {
		string before_context = context;
		// A dependent type discovered while replaying a class can carry the
		// source class component twice (X::X).  The insertion map is keyed by
		// the actual class path (X), so normalize that lexical replay spelling
		// before queuing the generated prerequisite.
		const string context_parent = PrefixComponent(context);
		if(!context_parent.empty() && LastComponent(context) == LastComponent(context_parent))
			before_context = context_parent;
		if(before_context != context)
			generated_by_owner_[generated_owner].push_back(generated);
		else
			generated_before_class_[before_context].push_back(generated);
	}
	else if(recursive_context_argument && definition.owner.empty() &&
		!PrefixComponent(context).empty())
		generated_before_class_[PrefixComponent(context)].push_back(generated);
	else {
	string generated_function_owner = explicit_static_data ? PrefixComponent(definition.owner) :
		explicit_member_definition ? GeneratedOwner(*member_owner_definition) :
		flattened_static_member ? (concrete_owner.empty() ? definition.owner : concrete_owner) :
		(concrete_owner.empty() ? entity_owner : concrete_owner);
	if(!concrete_owner.empty() && generated_function_owner.find("::") == string::npos &&
		specialization_bases_.find(LastComponent(concrete_owner)) ==
			specialization_bases_.end()) {
		string lexical_prefix = PrefixComponent(definition.owner);
		if(!lexical_prefix.empty() && LastComponent(lexical_prefix) ==
			LastComponent(definition.owner))
			lexical_prefix = PrefixComponent(lexical_prefix);
		if(!lexical_prefix.empty()) generated_function_owner = JoinPath(
			lexical_prefix, generated_function_owner);
	}
	// Materialized free function templates participate in later deduction just
	// like source functions.  Keep their transformed signature in typed state
	// so a generated pack-expanded call can use its concrete return type.
	if(!definition.class_template && !definition.alias_template &&
		!definition.member_template &&
		(generated->kind == "function-definition" ||
		 generated->kind == "simple-declaration")) {
		RecordFunctionSignature(generated, generated_function_owner);
		if(generated_owner != generated_function_owner)
		RecordFunctionSignature(generated, generated_owner);
	}
	generated_by_owner_[generated_function_owner].push_back(generated);
	if(!enclosing_instantiation_name.empty() && generated_owner.empty() &&
		generated->kind != "class-specifier" &&
		generated->kind != "class-forward-declaration")
		generated_before_class_[enclosing_instantiation_name].push_back(generated);
	}
	// Some declarator suffixes are cloned while a generated class is replayed,
	// so their bound does not pass through the ordinary child transform.  Finish
	// those bounds after all generated constants and dependencies are indexed,
	// before the declaration reaches PA11.
	std::function<void(const CPPGMAstNodePtr&)> materialize_array_bounds =
		[&](const CPPGMAstNodePtr& node) {
			if(!node) return;
			if(node->kind == "array-suffix" && !node->children.empty() &&
				node->children[0]) {
				PA19IntegralValue bound;
				if(EvaluateIntegralText(ConstantExpressionSpelling(node->children[0]),
					transform_context, substitutions, &bound))
					node->children[0] = CPPGMAstNodePtr(new CPPGMAstNode(
					"literal", IntegralValueSpelling(bound)));
			}
			for(size_t child = 0; child < node->children.size(); ++child)
				materialize_array_bounds(node->children[child]);
		};
	materialize_array_bounds(generated);
	active_specializations_.erase(key);
	return local_name;
}

string PA18TemplateExpander::MaterializeExternInstantiation(
	const TemplateDefinition& definition, const vector<string>& args,
	const vector<string>& metadata_args, map<string, string> substitutions,
	const map<string, PA19IntegralValue>& integral_substitutions,
	const map<string, vector<string> >& pack_substitutions,
	const string& context, const string& key,
	const map<string, FunctionSignature>& function_substitutions)
{
	map<string, string>::const_iterator cached = specializations_.find(key);
	if(cached != specializations_.end()) return cached->second;
	CPPGMAstNodePtr generated = TransformInstantiatedNode(definition,
		definition.owner, substitutions, integral_substitutions, pack_substitutions,
		function_substitutions);
	if(!generated || generated->kind != "function-definition" ||
		generated->children.size() < 2)
		throw logic_error("unable to materialize extern template declaration");
	CPPGMAstNodePtr declaration(new CPPGMAstNode("simple-declaration"));
	declaration->children.push_back(CloneNode(generated->children[0]));
	CPPGMAstNodePtr list(new CPPGMAstNode("init-declarator-list"));
	CPPGMAstNodePtr item(new CPPGMAstNode("init-declarator"));
	item->children.push_back(CloneNode(generated->children[1]));
	list->children.push_back(item);
	declaration->children.push_back(list);
	const string local_name = definition.name;
	RenameGeneratedFunction(declaration, local_name);
	MarkGeneratedNode(declaration, definition.qualified_name, metadata_args);
	declaration->extern_instantiation = true;
	declaration->template_primary = definition.qualified_name;
	declaration->template_arguments = metadata_args;
	const string owner = definition.lexical_owner.empty() ? definition.owner :
		definition.lexical_owner;
	generated_by_owner_[owner].push_back(declaration);
	extern_instantiation_declarations_[key] = declaration;
	specializations_[key] = local_name;
	EnsureDeclarationDependencies(declaration, definition.owner, owner);
	(void)args;
	(void)context;
	return local_name;
}

string PA18TemplateExpander::MaterializeInstantiation(const TemplateDefinition& definition,
	const vector<string>& args, const vector<string>& metadata_args,
	map<string, string> substitutions,
	const map<string, PA19IntegralValue>& integral_substitutions,
	const map<string, vector<string> >& pack_substitutions, const string& context,
	bool explicit_instantiation, const string& key, const string& concrete_owner,
	const map<string, FunctionSignature>& function_substitutions,
		bool defer_class_definition)
{
	map<string, string>::const_iterator cached = specializations_.find(key);
	// A cached name can be observed while its body is still being replayed.
	// Do not run ReplayCachedInstantiation recursively for that same semantic
	// specialization; the enclosing replay owns completion of the fixed point.
	if(cached != specializations_.end() && active_specializations_.find(key) !=
		active_specializations_.end()) return cached->second;
	if(cached != specializations_.end() && definition.class_template &&
		!defer_class_definition && deferred_type_only_class_instantiations_.find(key) !=
		deferred_type_only_class_instantiations_.end()) {
		specializations_.erase(cached);
		deferred_type_only_class_instantiations_.erase(key);
		deferred_class_instantiations_.erase(key);
	} else if(cached != specializations_.end() && definition.class_template &&
		!defer_class_definition && deferred_class_instantiations_.find(key) !=
		deferred_class_instantiations_.end() &&
		!HasDeferredTypeMember(definition, context, substitutions)) {
		specializations_.erase(cached);
		deferred_class_instantiations_.erase(key);
	} else if(cached != specializations_.end()) {
		map<string, CPPGMAstNodePtr>::const_iterator extern_declaration =
			extern_instantiation_declarations_.find(key);
		if(explicit_instantiation && extern_declaration !=
			extern_instantiation_declarations_.end()) {
			for(map<string, vector<CPPGMAstNodePtr> >::iterator owner =
				generated_by_owner_.begin(); owner != generated_by_owner_.end(); ++owner)
				for(size_t node = 0; node < owner->second.size(); ++node)
					if(owner->second[node] == extern_declaration->second) {
						owner->second.erase(owner->second.begin() + node);
						break;
					}
			extern_instantiation_declarations_.erase(extern_declaration);
			specializations_.erase(cached);
		} else {
			if(definition.class_template && HasDeferredDependentClassMember(
				definition, context, substitutions)) return cached->second;
			ReplayCachedInstantiation(definition, args, cached->second, context,
				explicit_instantiation, pack_substitutions);
			return cached->second;
		}
	}
	const string local_name = GeneratedSpecializationName(definition, args,
		metadata_args, substitutions, context, concrete_owner,
		explicit_instantiation);
	RegisterGeneratedSpecialization(definition, metadata_args, local_name);
	if(definition.class_template) substitutions[definition.name] = local_name;
	specializations_[key] = local_name;
	const bool deferred_member = defer_class_definition && definition.class_template &&
		HasDeferredDependentClassMember(definition, context, substitutions);
	// A type-only alias use may still name a dependent member (`C<T>::type`)
	// whose enclosing class body must be replayed later.  Keep only those
	// member-bearing class templates as typed forwards; ordinary value-like
	// traits still need their complete body for conversions and static storage.
	const bool deferred_type_only = defer_class_definition && definition.class_template &&
		HasDependentTypeAlias(definition);
	if(deferred_member || deferred_type_only) {
		const string generated_owner = definition.lexical_owner.empty() ?
			definition.owner : definition.lexical_owner;
		const string generated_path = JoinPath(GeneratedOwner(definition), local_name);
		const string lexical_path = JoinPath(generated_owner, local_name);
		class_declarations_[generated_path] = MakeForwardClass(local_name);
		class_declarations_[lexical_path] = class_declarations_[generated_path];
		RememberClassPath(generated_path);
		RememberClassPath(lexical_path);
		// A deferred nested class can be requested through a concrete enclosing
		// specialization.  Keep the typed shell visible at that owner too; the
		// source-only entries above are insufficient for a qualified lookup such
		// as `ConcreteOuter::member_template<Args>`.
		if(!concrete_owner.empty()) {
			const string concrete_path = JoinPath(concrete_owner, local_name);
			class_declarations_[concrete_path] = class_declarations_[generated_path];
			RememberClassPath(concrete_path);
		}
		// Keep the internal declaration as a forward so a non-static member query
		// can still complete the deferred specialization.  The emitted replay,
		// however, needs the static declarations that PA14 sees during expression
		// lookup; publish a shallow class shell containing only those safe members.
		CPPGMAstNodePtr deferred_shell = MakeForwardClass(local_name);
		deferred_shell->kind = "class-specifier";
		struct IntegralSubstitutionScope {
			map<string, PA19IntegralValue>& active;
			const map<string, PA19IntegralValue> saved;
			IntegralSubstitutionScope(map<string, PA19IntegralValue>& target,
				const map<string, PA19IntegralValue>& replacement)
				: active(target), saved(target) { active = replacement; }
			~IntegralSubstitutionScope() { active = saved; }
		} integral_scope(active_integral_substitutions_, integral_substitutions);
		for(size_t child = 0; definition.declaration &&
			child < definition.declaration->children.size(); ++child) {
			CPPGMAstNodePtr declaration = definition.declaration->children[child];
			while(declaration && declaration->kind == "template-declaration" &&
				declaration->children.size() > 1)
				declaration = declaration->children[1];
			const bool safe_enum = declaration && declaration->kind == "enum-specifier";
			if(!safe_enum && (!declaration || declaration->kind != "simple-declaration" ||
				declaration->children.empty() ||
				(!HasDeclarationSpecifier(declaration->children[0], "const") &&
					!HasDeclarationSpecifier(declaration->children[0], "constexpr")))) continue;
			CPPGMAstNodePtr transformed;
			try {
				transformed = TransformNode(declaration, generated_path, substitutions);
			} catch(const PA18SubstitutionFailure&) {}
			if(transformed) deferred_shell->children.push_back(transformed);
		}
		generated_by_owner_[generated_owner].push_back(deferred_shell);
		if(!concrete_owner.empty() && concrete_owner != generated_owner)
			generated_by_owner_[concrete_owner].push_back(deferred_shell);
		// A deferred shell is sufficient for type lookup, but a later expression
		// can still name a static integral member of that shell (`int_<N>::value`).
		// Record those source constants under the generated identities now so PA14
		// can bind the qualified expression without forcing the recursive class body
		// to replay prematurely.
		const string previous_constant_owner = active_instantiation_name_;
		active_instantiation_name_.clear();
		if(definition.declaration) for(size_t child = 0;
			child < definition.declaration->children.size(); ++child) {
			CPPGMAstNodePtr declaration = definition.declaration->children[child];
			while(declaration && declaration->kind == "template-declaration" &&
				declaration->children.size() > 1)
				declaration = declaration->children[1];
			if(!declaration || declaration->kind != "simple-declaration") continue;
			RecordConstantDeclaration(declaration, generated_path, substitutions);
			if(lexical_path != generated_path)
				RecordConstantDeclaration(declaration, lexical_path, substitutions);
			if(!concrete_owner.empty())
				RecordConstantDeclaration(declaration,
					JoinPath(concrete_owner, local_name), substitutions);
		}
		active_instantiation_name_ = previous_constant_owner;
		deferred_class_instantiations_.insert(key);
		if(deferred_type_only) deferred_type_only_class_instantiations_.insert(key);
		return local_name;
	}
	if(DeferIncompleteAliasClass(definition, args, context)) {
		const string generated_owner = definition.lexical_owner.empty() ?
			definition.owner : definition.lexical_owner;
		const string generated_path = JoinPath(GeneratedOwner(definition), local_name);
		const string lexical_path = JoinPath(generated_owner, local_name);
		class_declarations_[generated_path] = MakeForwardClass(local_name);
		class_declarations_[lexical_path] = class_declarations_[generated_path];
		RememberClassPath(generated_path);
		RememberClassPath(lexical_path);
		deferred_class_instantiations_.insert(key);
		return local_name;
	}
	if(!active_specializations_.insert(key).second) {
		return local_name;
	}
	try {
		return EmitInstantiation(definition, args, metadata_args, substitutions,
			integral_substitutions, pack_substitutions, context, explicit_instantiation,
			key, local_name, concrete_owner, function_substitutions);
	} catch(const PA18SubstitutionFailure&) {
		active_specializations_.erase(key);
		map<string, string>::iterator cached_result = specializations_.find(key);
		if(cached_result != specializations_.end() && cached_result->second == local_name)
			specializations_.erase(cached_result);
		throw;
	} catch(...) {
		active_specializations_.erase(key);
		map<string, string>::iterator cached_result = specializations_.find(key);
		if(cached_result != specializations_.end() && cached_result->second == local_name)
			specializations_.erase(cached_result);
		throw;
	}
}

void PA18TemplateExpander::RecoverNestedVectorArgument(
	const TemplateDefinition& definition, vector<string>* arguments,
	const string& context) const
{
	if(definition.name != "vector" || !arguments || arguments->size() != 1 ||
		active_instantiation_name_.empty()) return;
	const string active_name = LastComponent(active_instantiation_name_);
	map<string, vector<string> >::const_iterator active_arguments =
		specialization_arguments_.find(active_name);
	map<string, string>::const_iterator active_base =
		specialization_bases_.find(active_name);
	if(active_arguments == specialization_arguments_.end() ||
		active_base == specialization_bases_.end()) return;
	string source_name = active_base->second;
	const size_t source_open = source_name.find('<');
	if(source_open != string::npos) source_name.erase(source_open);
	const TemplateDefinition* source_definition = FindDefinition(source_name, context);
	if(!source_definition || !source_definition->class_template) return;
	const string raw_argument = NormalizeTypeArgument(CanonicalSpelling((*arguments)[0]));
	size_t matching_parameter = source_definition->parameters.size();
	for(size_t parameter = 0; parameter < source_definition->parameters.size() &&
		parameter < active_arguments->second.size(); ++parameter)
		if(NormalizeTypeArgument(CanonicalSpelling(active_arguments->second[parameter])) ==
			raw_argument) matching_parameter = parameter;
	if(matching_parameter >= source_definition->parameters.size()) return;
	for(size_t parameter = matching_parameter; parameter > 0; --parameter) {
		const string candidate = NormalizeTypeArgument(CanonicalSpelling(
			active_arguments->second[parameter - 1]));
		if(candidate.empty() || candidate == raw_argument ||
			candidate.find_first_not_of(
				"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != string::npos ||
			(class_contexts_.find(candidate) == class_contexts_.end() &&
				!FindClassDeclaration(candidate, context))) continue;
		(*arguments)[0] = candidate;
		break;
	}
}

void PA18TemplateExpander::AddConcreteOwnerSubstitutions(
	const string& concrete_owner, const string& context,
	map<string, string>* substitutions, bool bind_source_owner,
	map<string, vector<string> >* pack_substitutions)
{
	if(concrete_owner.empty() || !substitutions) return;
	map<string, string>::const_iterator owner_base = specialization_bases_.find(
		LastComponent(concrete_owner));
	map<string, vector<string> >::const_iterator owner_arguments =
		specialization_arguments_.find(LastComponent(concrete_owner));
	if(owner_base == specialization_bases_.end() ||
		owner_arguments == specialization_arguments_.end()) return;
	const TemplateDefinition* owner_definition = FindDefinition(owner_base->second, context);
	if(!owner_definition || !owner_definition->class_template) return;
	const TemplateDefinition* selected_owner = SelectClassTemplateDefinition(
		owner_definition, owner_arguments->second, context);
	if(selected_owner) owner_definition = selected_owner;
	// A member template's defaults are written with the source class name
	// (`decltype(prop::member<E>())`).  Bind that name to the concrete owner
	// while resolving the member's own arguments; parameter bindings alone leave
	// the source owner as an empty outer substitution.
	if(bind_source_owner && !owner_definition->name.empty())
		(*substitutions)[owner_definition->name] = concrete_owner;
	for(size_t parameter = 0; parameter < owner_definition->parameters.size() &&
		parameter < owner_arguments->second.size(); ++parameter)
		if(!owner_definition->parameters[parameter].name.empty())
			(*substitutions)[owner_definition->parameters[parameter].name] =
				owner_arguments->second[parameter];
	if(owner_definition->partial_specialization) {
		map<string, string> specialized;
		if(MatchClassSpecializationPattern(*owner_definition, owner_arguments->second,
			&specialized, context))
			for(map<string, string>::const_iterator binding = specialized.begin();
				binding != specialized.end(); ++binding)
				(*substitutions)[binding->first] = binding->second;
		if(pack_substitutions) for(size_t pack = 0;
			pack < owner_definition->specialization_pack_names.size(); ++pack) {
			const string& name = owner_definition->specialization_pack_names[pack];
			map<string, string>::const_iterator binding = specialized.find(name);
			(*pack_substitutions)[name] = binding == specialized.end() || binding->second.empty() ?
				vector<string>() : SplitTemplateArguments(binding->second);
		}
	}
}

void PA18TemplateExpander::InstallConcreteOwnerPacks(
	const string& concrete_owner, const string& context,
	map<string, string>* substitutions,
	map<string, vector<string> >* pack_substitutions, bool bind_source_owner)
{
	map<string, vector<string> > owner_packs;
	if(!concrete_owner.empty())
		AddConcreteOwnerSubstitutions(concrete_owner, context, substitutions,
			bind_source_owner, &owner_packs);
	for(map<string, vector<string> >::const_iterator pack = owner_packs.begin();
		pack != owner_packs.end(); ++pack) {
		if(!pack->first.empty()) active_pack_substitutions_[pack->first] = pack->second;
		if(pack_substitutions && pack_substitutions->find(pack->first) == pack_substitutions->end())
			(*pack_substitutions)[pack->first] = pack->second;
	}
}

string PA18TemplateExpander::Instantiate(const TemplateDefinition& definition,
	const vector<string>& raw_args, const string& context, bool explicit_instantiation,
	const map<string, vector<string> >* pack_hints,
	const map<string, string>* outer_substitutions, const string* requested_owner,
	const map<string, FunctionSignature>* function_hints,
	const map<string, vector<string> >* forwarding_pack_hints,
	bool defer_class_definition)
{
	if(definition.parameters.empty()) throw logic_error("template has no type parameters");
	vector<string> args, metadata_args;
	map<string, string> substitutions = outer_substitutions ? *outer_substitutions :
		map<string, string>();
	if(definition.class_template)
		for(size_t argument = 0; argument < raw_args.size(); ++argument) {
			if(raw_args[argument].find("...") == string::npos) continue;
			const bool concrete_function_type =
				SplitDirectFunctionType(raw_args[argument], 0, 0, 0) ||
				SplitFunctionPointerType(raw_args[argument], 0, 0);
			if(!concrete_function_type || HasUnresolvedTemplateParameter(raw_args[argument],
				context, substitutions))
				throw PA18SubstitutionFailure("dependent template pack argument");
		}
	const map<string, FunctionSignature> function_substitutions = function_hints ?
		*function_hints : map<string, FunctionSignature>();
	string concrete_owner = requested_owner ? *requested_owner :
		active_concrete_owner_.name;
	if(!ConcreteOwnerMatches(definition, concrete_owner)) concrete_owner.clear();
	// Completing a class body while replaying that same class template must not
	// recursively complete the next specialization named by a dependent alias
	// (`int_<N>::next` is the minimal example).  Keep those nested specializations
	// as typed shells until a later, non-recursive lookup actually needs them.
	const map<string, string>::const_iterator active_base =
		specialization_bases_.find(LastComponent(active_instantiation_name_));
	if(!active_instantiation_name_.empty() && definition.class_template &&
		active_base != specialization_bases_.end() &&
		LastComponent(active_base->second) == LastComponent(definition.qualified_name))
		defer_class_definition = true;
	// A top-level class template owns its own generated name, but a nested
	// class template also needs the enclosing source class binding while its
	// body is replayed.  Without that distinction a binding inherited from an
	// unrelated nested argument (for example `ListSet -> call_X`) survives into
	// `ListSet<T>::impl` and corrupts the dependent owner spelling.
	const bool bind_enclosing_owner = !definition.class_template ||
		definition.owner.find('<') != string::npos;
	InstallOuterOwnerSubstitutions(outer_substitutions, context, &substitutions,
		bind_enclosing_owner);
	AddConcreteOwnerSubstitutions(concrete_owner, context, &substitutions,
		bind_enclosing_owner);
	map<string, PA19IntegralValue> integral_substitutions;
	map<string, vector<string> > pack_substitutions;
	// Default non-type arguments can contain sizeof...(Pack).  Argument
	// resolution happens before TransformInstantiatedNode installs the replay
	// packs, so expose the caller's typed pack hints for this boundary and
	// restore the surrounding replay state afterward.
	const map<string, vector<string> > previous_argument_packs =
		active_pack_substitutions_;
	InstallConcreteOwnerPacks(concrete_owner, context, &substitutions,
		&pack_substitutions, bind_enclosing_owner);
	if(pack_hints) for(map<string, vector<string> >::const_iterator hint = pack_hints->begin(); hint != pack_hints->end(); ++hint) if(!hint->first.empty()) active_pack_substitutions_[hint->first] = hint->second;
	try {
		ResolveTemplateArguments(definition, raw_args, context, &args, &metadata_args,
			&substitutions, &integral_substitutions, &pack_substitutions, pack_hints);
	} catch(...) {
		active_pack_substitutions_ = previous_argument_packs;
		throw;
	}
	active_pack_substitutions_ = previous_argument_packs;
	// A dependent member lookup can reach a class template's primary
	// definition after the matching partial specialization has already been
	// selected elsewhere.  Re-entering the primary would reuse the same nominal
	// generated class name and overwrite the partial body (for example, turning
	// `is_pair_like<pair<...>>` from its `true_type` partial back into the
	// primary `false_type`).  Route the request through the typed partial before
	// registering any primary specialization.
	if(definition.class_template && !definition.partial_specialization) {
		const TemplateDefinition* selected = SelectClassTemplateDefinition(
			&definition, args, context);
		if(selected && selected != &definition)
			return Instantiate(*selected, args, context, explicit_instantiation,
				pack_hints, outer_substitutions, requested_owner, function_hints,
				forwarding_pack_hints, defer_class_definition);
	}
	if(pack_hints) for(map<string, vector<string> >::const_iterator hint = pack_hints->begin();
		hint != pack_hints->end(); ++hint)
		if(!hint->first.empty()) pack_substitutions[hint->first] = hint->second;
	ApplyForwardingPackHints(forwarding_pack_hints, &pack_substitutions);
	if(definition.partial_specialization) {
		map<string, string> specialized;
		// A dependent partial-pattern mismatch is substitution failure.  It must
		// stay inside candidate viability instead of becoming an internal error.
		if(!MatchClassSpecializationPattern(definition, args, &specialized, context))
			throw PA18SubstitutionFailure("class partial specialization does not match");
		for(map<string, string>::const_iterator it = specialized.begin(); it != specialized.end(); ++it)
			substitutions[it->first] = it->second;
		for(size_t pack_index = 0; pack_index < definition.specialization_pack_names.size(); ++pack_index) {
			const string& pack_name = definition.specialization_pack_names[pack_index];
			map<string, string>::const_iterator binding = specialized.find(pack_name);
			vector<string> values;
			if(binding != specialized.end() && !binding->second.empty())
				values = SplitTemplateArguments(binding->second);
			if(!pack_name.empty()) {
				pack_substitutions[pack_name] = values;
				if(values.empty()) substitutions.erase(pack_name);
				else substitutions[pack_name] = values[0];
			}
		}
	}
	ostringstream definition_key;
	definition_key << definition.qualified_name << "@" << definition.declaration.get();
	string key = definition_key.str();
	for(size_t i = 0; i < args.size(); ++i) key += "|" + CanonicalSpelling(args[i]);
	const string request_key = key;
	if(!concrete_owner.empty()) key += "|owner=" + CanonicalSpelling(concrete_owner);
	if(!explicit_instantiation && extern_instantiation_keys_.find(request_key) !=
		extern_instantiation_keys_.end())
		return MaterializeExternInstantiation(definition, args, metadata_args,
			substitutions, integral_substitutions, pack_substitutions, context, key,
			function_substitutions);
	return MaterializeInstantiation(definition, args, metadata_args, substitutions,
		integral_substitutions, pack_substitutions, context, explicit_instantiation, key,
		concrete_owner, function_substitutions, defer_class_definition);
}


} // namespace pa18_templates_internal
