#include <functional>
#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

namespace {

bool HasStaticMemberDeclaration(const CPPGMAstNodePtr& node, const string& name)
{
	if(!node || name.empty()) return false;
	if(node->kind == "simple-declaration" && !node->children.empty() &&
		SpellNode(node->children[0]).find("static") != string::npos) {
		const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
		if(list) for(size_t item = 0; item < list->children.size(); ++item) {
			const CPPGMAstNodePtr entry = list->children[item];
			if(!entry || entry->children.empty()) continue;
			if(LastComponent(FirstIdentifierLocal(entry->children[0])) == name) return true;
		}
	}
	for(size_t child = 0; child < node->children.size(); ++child)
		if(HasStaticMemberDeclaration(node->children[child], name)) return true;
	return false;
}

void MarkStaticGeneratedFunction(const CPPGMAstNodePtr& node)
{
	if(!node || node->kind != "function-definition" || node->children.empty() ||
		!node->children[0]) return;
	if(SpellNode(node->children[0]).find("static") != string::npos) return;
	node->children[0]->children.insert(node->children[0]->children.begin(),
		CPPGMAstNodePtr(new CPPGMAstNode("decl-specifier", "KW_STATIC:static")));
}

} // namespace

void PA18TemplateExpander::RestoreGeneratedMemberParameterNames(
	const TemplateDefinition& definition,
	const TemplateDefinition* member_owner_definition,
	const CPPGMAstNodePtr& generated)
{
	if(!member_owner_definition || generated->kind != "function-definition") return;
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
	owner_member = find_owner_member(member_owner_definition->declaration);
	const CPPGMAstNodePtr generated_clause = DescendantOfKind(
		FunctionDeclarator(generated), "parameter-clause");
	const CPPGMAstNodePtr owner_clause = owner_member ? DescendantOfKind(
		owner_member, "parameter-clause") : CPPGMAstNodePtr();
	if(owner_clause && generated_clause && owner_clause->children.size() ==
		generated_clause->children.size()) for(size_t parameter = 0;
		parameter < generated_clause->children.size(); ++parameter) {
		const string owner_name = ParameterIdentifier(owner_clause->children[parameter]);
		if(owner_name.empty() || !generated_clause->children[parameter]) continue;
		if(!ParameterIdentifier(generated_clause->children[parameter]).empty()) continue;
		CPPGMAstNodePtr declarator = ChildOfKindLocal(
			generated_clause->children[parameter], "declarator");
		if(!declarator) {
			declarator.reset(new CPPGMAstNode("declarator"));
			generated_clause->children[parameter]->children.push_back(declarator);
		}
		declarator->children.push_back(CPPGMAstNodePtr(
			new CPPGMAstNode("identifier", owner_name)));
	}
}

void PA18TemplateExpander::RegisterGeneratedTypeEntity(
	const TemplateDefinition& definition, const CPPGMAstNodePtr& generated,
	const string& generated_owner, const string& local_name,
	const string& concrete_owner, const map<string, string>& substitutions,
	const vector<string>& args,
	const map<string, vector<string> >& pack_substitutions,
	const string& context, bool explicit_instantiation)
{
	if(definition.alias_template) {
		RegisterGeneratedTypeAlias(generated, generated_owner);
		set<string> concrete_owners;
		if(!concrete_owner.empty()) concrete_owners.insert(concrete_owner);
		for(map<string, string>::const_iterator substitution = substitutions.begin();
			substitution != substitutions.end(); ++substitution) {
			const string value = CanonicalSpelling(substitution->second);
			if(specialization_bases_.find(LastComponent(value)) !=
				specialization_bases_.end() &&
				specialization_arguments_.find(LastComponent(value)) !=
				specialization_arguments_.end()) concrete_owners.insert(value);
			const size_t separator = substitution->second.rfind("::");
			if(separator == string::npos) continue;
			const string owner = substitution->second.substr(0, separator);
			if(specialization_bases_.find(LastComponent(owner)) !=
				specialization_bases_.end() &&
				specialization_arguments_.find(LastComponent(owner)) !=
				specialization_arguments_.end()) concrete_owners.insert(owner);
		}
		for(set<string>::const_iterator owner = concrete_owners.begin();
			owner != concrete_owners.end(); ++owner)
			RegisterGeneratedTypeAlias(generated, *owner);
	}
	if(!definition.class_template) return;
	const string generated_path = JoinPath(definition.owner, local_name);
	class_declarations_[generated_path] = generated;
	const string lexical_path = JoinPath(generated_owner, local_name);
	class_declarations_[lexical_path] = generated;
	class_contexts_.insert(generated_path);
	RegisterGeneratedConstants(generated, generated_path);
	if(!concrete_owner.empty()) {
		const string concrete_path = JoinPath(concrete_owner, local_name);
		class_declarations_[concrete_path] = generated;
		class_contexts_.insert(concrete_path);
		RegisterGeneratedConstants(generated, concrete_path);
	}
	const map<string, vector<string> > previous_packs = active_pack_substitutions_;
	active_pack_substitutions_ = pack_substitutions;
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
	string member_owner_name = definition.owner;
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
	string concrete_owner = definition.class_template ?
		FindConcreteInstantiationOwner(definition, substitutions, context, requested_owner) :
		((member_definition && ConcreteOwnerMatches(definition, requested_owner)) ||
		 (definition.alias_template && ConcreteOwnerMatches(definition, requested_owner)) ?
		 requested_owner : string());
	// A generated class can be reached through the lexical function path used
	// while replaying a functional cast (for example
	// `make_pair_int::pair_int_`), while PA14 indexes its declarations by the
	// materialized class identity (`pair_int_`).  Keep the typed specialization
	// owner canonical before registering generated member declarations.
	if(!concrete_owner.empty() && specialization_bases_.find(
		LastComponent(concrete_owner)) != specialization_bases_.end() &&
		class_contexts_.find(LastComponent(concrete_owner)) != class_contexts_.end())
		concrete_owner = LastComponent(concrete_owner);
	else if(!concrete_owner.empty() && specialization_bases_.find(
		LastComponent(concrete_owner)) != specialization_bases_.end()) {
		const string generated_namespace = PrefixComponent(generated_owner);
		for(set<string>::const_iterator candidate = class_contexts_.begin();
			candidate != class_contexts_.end(); ++candidate)
			if(LastComponent(*candidate) == LastComponent(concrete_owner) &&
				(generated_namespace.empty() || PrefixComponent(*candidate) == generated_namespace)) {
				concrete_owner = *candidate;
				break;
			}
	}
	if(definition.class_template) {
		class_contexts_.insert(JoinPath(definition.owner, local_name));
		class_contexts_.insert(JoinPath(generated_owner, local_name));
	}
	const string previous_instantiation_name = active_instantiation_name_;
	active_instantiation_name_ = definition.class_template ? local_name : string();
	CPPGMAstNodePtr generated;
	const string transform_context = concrete_owner.empty() ? definition.owner :
		concrete_owner;
	try {
		generated = TransformInstantiatedNode(definition, transform_context,
			substitutions, integral_substitutions, pack_substitutions,
			function_substitutions);
	} catch(...) {
		active_instantiation_name_ = previous_instantiation_name;
		throw;
	}
	active_instantiation_name_ = previous_instantiation_name;
	if(!generated) throw logic_error("unable to instantiate template");
	if(member_definition)
		RestoreGeneratedMemberParameterNames(definition, member_owner_definition, generated);
	if(member_definition && !definition.class_template && member_owner_definition &&
		HasStaticMemberDeclaration(member_owner_definition->declaration,
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
	const bool generated_special_member = generated->kind == "special-member-definition" ||
		generated->kind == "special-member-declaration";
	if(!definition.class_template && !definition.alias_template &&
		(!member_definition || concrete_owner.empty()) && !generated_special_member)
		RenameGeneratedFunction(generated, local_name);
	if(definition.class_template || definition.alias_template) generated->value = local_name;
	RegisterGeneratedTypeEntity(definition, generated, generated_owner, local_name,
		concrete_owner, substitutions, args, pack_substitutions, context,
		explicit_instantiation);
	EnsureDeclarationDependencies(generated, definition.owner, generated_owner);
	// Text replay can encounter the same member template-id while resolving a
	// dependent type and give the detached definition its standalone cache name.
	// Once the entity is owned by a concrete class, the declaration must retain
	// the member spelling so class-scope lookup can bind it.
	if(member_definition && !concrete_owner.empty()) {
		const bool special_member = generated->kind == "special-member-definition" ||
			generated->kind == "special-member-declaration";
		const bool operator_member = LastComponent(definition.name).compare(0, 8,
			"operator") == 0;
		if(special_member)
			generated->value = concrete_owner + "::" + LastComponent(concrete_owner);
		RenameGeneratedFunction(generated, special_member ?
			LastComponent(concrete_owner) : operator_member ? local_name :
			LastComponent(definition.name));
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
	const bool explicit_static_data = definition.explicit_specialization &&
		definition.variable_template && generated->kind == "simple-declaration";
	if(!explicit_static_data && class_contexts_.find(context) != class_contexts_.end() && owner_is_context_ancestor &&
		(!definition.class_template || !definition.owner.empty()) &&
		context != definition.owner &&
		!recursive_context_argument) {
		generated_before_class_[context].push_back(generated);
	}
	else if(recursive_context_argument && definition.owner.empty() &&
		!PrefixComponent(context).empty())
		generated_before_class_[PrefixComponent(context)].push_back(generated);
	else {
	string generated_function_owner = explicit_static_data ? PrefixComponent(definition.owner) :
		(concrete_owner.empty() ? generated_owner : concrete_owner);
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
	generated_by_owner_[generated_function_owner].push_back(generated);
	}
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
	const map<string, FunctionSignature>& function_substitutions)
{
	map<string, string>::const_iterator cached = specializations_.find(key);
	if(cached != specializations_.end()) {
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
	if(DeferIncompleteAliasClass(definition, args, context)) {
		const string generated_owner = definition.lexical_owner.empty() ?
			definition.owner : definition.lexical_owner;
		const string generated_path = JoinPath(definition.owner, local_name);
		const string lexical_path = JoinPath(generated_owner, local_name);
		class_contexts_.insert(generated_path);
		class_contexts_.insert(lexical_path);
		class_declarations_[generated_path] = MakeForwardClass(local_name);
		class_declarations_[lexical_path] = class_declarations_[generated_path];
		return local_name;
	}
	if(!active_specializations_.insert(key).second) return local_name;
	return EmitInstantiation(definition, args, metadata_args, substitutions,
		integral_substitutions, pack_substitutions, context, explicit_instantiation,
		key, local_name, concrete_owner, function_substitutions);
}

string PA18TemplateExpander::Instantiate(const TemplateDefinition& definition,
	const vector<string>& raw_args, const string& context, bool explicit_instantiation,
	const map<string, vector<string> >* pack_hints,
	const map<string, string>* outer_substitutions, const string* requested_owner,
	const map<string, FunctionSignature>* function_hints)
{
	if(definition.parameters.empty()) throw logic_error("template has no type parameters");
	vector<string> args, metadata_args;
	map<string, string> substitutions = outer_substitutions ? *outer_substitutions :
		map<string, string>();
	const map<string, FunctionSignature> function_substitutions = function_hints ?
		*function_hints : map<string, FunctionSignature>();
	string concrete_owner = requested_owner ? *requested_owner : active_concrete_owner_;
	if(!ConcreteOwnerMatches(definition, concrete_owner)) concrete_owner.clear();
	// A template-template argument can name a member alias on a concrete
	// specialization (for example `quote_X_::fn`).  Recover the outer class's
	// typed bindings before replaying the member alias body; the source member
	// definition itself only names those bindings as dependent parameters.
	if(outer_substitutions) for(map<string, string>::const_iterator outer =
		outer_substitutions->begin(); outer != outer_substitutions->end(); ++outer) {
		const size_t separator = outer->second.rfind("::");
		if(separator == string::npos) continue;
		const string owner = outer->second.substr(0, separator);
		AddConcreteOwnerSubstitutions(owner, context, &substitutions);
	}
	AddConcreteOwnerSubstitutions(concrete_owner, context, &substitutions);
	map<string, PA19IntegralValue> integral_substitutions;
	map<string, vector<string> > pack_substitutions;
	ResolveTemplateArguments(definition, raw_args, context, &args, &metadata_args,
		&substitutions, &integral_substitutions, &pack_substitutions, pack_hints);
	if(pack_hints) for(map<string, vector<string> >::const_iterator hint = pack_hints->begin();
		hint != pack_hints->end(); ++hint)
		if(!hint->first.empty()) pack_substitutions[hint->first] = hint->second;
	if(definition.partial_specialization) {
		map<string, string> specialized;
		if(!MatchClassSpecializationPattern(definition, args, &specialized, context))
			throw logic_error("class partial specialization does not match");
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
		concrete_owner, function_substitutions);
}


} // namespace pa18_templates_internal
