#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
#include <functional>

using namespace pa18_templates_internal;

namespace {
string OrderingDeclaredName(const CPPGMAstNodePtr& node)
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

bool OrderingTypeDeclaration(const CPPGMAstNodePtr& node)
{
	if(!node) return false;
	if(node->kind == "class-specifier" || node->kind == "class-forward-declaration" ||
		node->kind == "enum-specifier" || node->kind == "alias-declaration") return true;
	return node->kind == "simple-declaration" && !node->children.empty() &&
		SpellNode(node->children[0]).find("typedef") != string::npos;
}

bool MentionsQualifiedGeneratedType(const CPPGMAstNodePtr& node,
	const string& type_name)
{
	if(!node || type_name.empty()) return false;
	const string spelling = CanonicalSpelling(RemoveMarker(node->value));
	if(spelling.find(type_name + "::") != string::npos) return true;
	for(size_t i = 0; i < node->children.size(); ++i)
		if(MentionsQualifiedGeneratedType(node->children[i], type_name)) return true;
	return false;
}

bool NamespacePathContains(const CPPGMAstNodePtr& node, const string& path)
{
	if(!node || node->kind != "namespace-definition" || path.empty()) return false;
	const size_t separator = path.find("::");
	const string head = path.substr(0, separator);
	if(node->value != head) return false;
	if(separator == string::npos) return true;
	const string remainder = path.substr(separator + 2);
	for(size_t i = 0; i < node->children.size(); ++i)
		if(NamespacePathContains(node->children[i], remainder)) return true;
	return false;
}

bool ContainsStaticAssert(const CPPGMAstNodePtr& node)
{
	if(!node) return false;
	if(node->kind == "static-assert-declaration") return true;
	for(size_t i = 0; i < node->children.size(); ++i)
		if(ContainsStaticAssert(node->children[i])) return true;
	return false;
}

bool NeedsPA18Expansion(const CPPGMAstNodePtr& node)
{
	if(!node) return false;
	if(node->kind == "literal" && !node->value.empty() &&
		isdigit(static_cast<unsigned char>(node->value[0])) &&
		node->value.find('_') != string::npos) return true;
	if(node->kind == "template-declaration" ||
		node->kind == "explicit-instantiation-declaration") return true;
	if((node->kind == "id-expression" || node->kind == "decl-specifier" ||
		node->kind == "type-name" || node->kind == "type-specifier" ||
		node->kind == "class-specifier" || node->kind == "class-forward-declaration" ||
		node->kind == "alias-declaration" || node->kind == "target") &&
		node->value.find('<') != string::npos) return true;
	for(size_t i = 0; i < node->children.size(); ++i)
		if(NeedsPA18Expansion(node->children[i])) return true;
	return false;
}

}

namespace pa18_templates_internal {

string PA18TemplateExpander::NormalizeElaboratedSpelling(string raw,
	const string& context) const
{
	raw = CanonicalSpelling(raw);
	const char* const keys[] = {"struct", "class", "union"};
	for(size_t position = 0; position < raw.size();) {
		if(position > 0 && IsIdentifierCharacter(raw[position - 1])) {
			++position;
			continue;
		}
		bool separated = false;
		for(size_t key = 0; key < sizeof(keys) / sizeof(keys[0]); ++key) {
			const string keyword = keys[key];
			if(raw.compare(position, keyword.size(), keyword) != 0 ||
				position + keyword.size() >= raw.size() ||
				!IsIdentifierCharacter(raw[position + keyword.size()])) continue;
			size_t end = position + keyword.size();
			while(end < raw.size() && IsIdentifierCharacter(raw[end])) ++end;
			const string candidate = raw.substr(position + keyword.size(),
				end - position - keyword.size());
			bool known = class_contexts_.find(candidate) != class_contexts_.end();
			for(string current = context; !known && !current.empty();) {
				if(class_contexts_.find(JoinPath(current, candidate)) != class_contexts_.end())
					known = true;
				const size_t separator = current.rfind("::");
				if(separator == string::npos) current.clear();
				else current.erase(separator);
			}
			if(known) {
				raw.insert(position + keyword.size(), " ");
				position += keyword.size() + 1;
				separated = true;
				break;
			}
		}
		if(!separated) ++position;
	}
	return raw;
}

void PA18TemplateExpander::CountNamespaceOccurrences(const CPPGMAstNodePtr& node,
	const string& context)
{
	if(!node) return;
	if(node->kind == "namespace-definition") {
		const string child_context = node->value.empty() ? context : JoinPath(context, node->value);
		++namespace_occurrences_[child_context];
		for(size_t i = 0; i < node->children.size(); ++i)
			if(node->children[i] && node->children[i]->kind != "inline")
				CountNamespaceOccurrences(node->children[i], child_context);
		return;
	}
	for(size_t i = 0; i < node->children.size(); ++i)
		CountNamespaceOccurrences(node->children[i], context);
}

CPPGMAstNodePtr PA18TemplateExpander::TransformTranslationUnit(
	const CPPGMAstNodePtr& input)
{
	if(!input || input->kind != "translation-unit") return CPPGMAstNodePtr();
	namespace_occurrences_.clear();
	CountNamespaceOccurrences(input, string());
	CollectVariables(input, string());
	CPPGMAstNodePtr result(new CPPGMAstNode("translation-unit"));
	map<string, string> top_level_substitutions;
	for(size_t i = 0; i < input->children.size(); ++i) {
		CPPGMAstNodePtr child = TransformNode(input->children[i], string(),
			top_level_substitutions);
		if(child) result->children.push_back(child);
		const CPPGMAstNodePtr original = input->children[i];
		if(original && original->kind == "simple-declaration" &&
			!original->children.empty() &&
			SpellNode(original->children[0]).find("typedef") != string::npos)
			RecordTypedefSubstitutions(original, string(), &top_level_substitutions);
		if(original && original->kind == "alias-declaration" &&
			!original->value.empty() && !original->children.empty()) {
			const string alias = original->value;
			const string value = RewriteText(TypeIdSpelling(original->children[0]),
				string(), top_level_substitutions, 0);
			if(!value.empty()) {
				top_level_substitutions[alias] = value;
				type_aliases_[alias] = value;
				vector<string>& aliases = type_aliases_by_name_[alias];
				if(find(aliases.begin(), aliases.end(), alias) == aliases.end())
					aliases.push_back(alias);
			}
		}
	}
	InjectGenerated(result, string(), string());
	vector<CPPGMAstNodePtr> generated_forwards;
	vector<CPPGMAstNodePtr> other_children;
	for(size_t child = 0; child < result->children.size(); ++child) {
		const CPPGMAstNodePtr& item = result->children[child];
		if(item && item->kind == "class-forward-declaration" &&
			specialization_bases_.find(LastComponent(item->value)) != specialization_bases_.end())
			generated_forwards.push_back(item);
		else other_children.push_back(item);
	}
	if(!generated_forwards.empty()) {
		generated_forwards.insert(generated_forwards.end(), other_children.begin(), other_children.end());
		result->children.swap(generated_forwards);
	}
	// If a materialized friend class derives from another materialized
	// specialization, the late replay path can leave their complete
	// definitions in source-use order rather than base order.  Repair that
	// generated inheritance chain from its layout dependencies.  The friend
	// predicate is a structured declaration fact, not a flattened source-text
	// acceptance gate; unrelated generated classes retain the established PA18
	// placement rules.
	vector<CPPGMAstNodePtr> complete_generated;
	for(size_t child = 0; child < result->children.size(); ++child) {
		const CPPGMAstNodePtr& item = result->children[child];
		if(item && item->kind == "class-specifier" && item->children.size() > 1 &&
			specialization_bases_.find(LastComponent(item->value)) !=
			specialization_bases_.end()) complete_generated.push_back(item);
	}
	set<string> inheritance_chain_names;
	for(size_t current = 0; current < complete_generated.size(); ++current)
		for(size_t base = 0; base < complete_generated.size(); ++base) {
			if(current == base) continue;
			vector<CPPGMAstNodePtr> dependency(1, complete_generated[base]);
			if(!MentionsGeneratedLayoutClass(complete_generated[current], dependency)) continue;
			inheritance_chain_names.insert(LastComponent(complete_generated[current]->value));
			inheritance_chain_names.insert(LastComponent(complete_generated[base]->value));
		}
	bool friend_inheritance_chain = false;
	for(size_t generated = 0; generated < complete_generated.size(); ++generated)
		if(inheritance_chain_names.find(LastComponent(complete_generated[generated]->value)) !=
			inheritance_chain_names.end() && HasFriendSpecifier(complete_generated[generated])) {
			friend_inheritance_chain = true;
			break;
		}
	if(!inheritance_chain_names.empty() && friend_inheritance_chain) {
		vector<CPPGMAstNodePtr> chain;
		for(size_t generated = 0; generated < complete_generated.size(); ++generated)
			if(inheritance_chain_names.find(LastComponent(complete_generated[generated]->value)) !=
				inheritance_chain_names.end()) chain.push_back(complete_generated[generated]);
		const vector<CPPGMAstNodePtr> ordered_chain = OrderGeneratedClasses(chain);
		size_t first_chain = result->children.size();
		size_t first_use = result->children.size();
		size_t dependency_end = 0;
		for(size_t child = 0; child < result->children.size(); ++child) {
			const CPPGMAstNodePtr& item = result->children[child];
			const bool chain_class = item && item->kind == "class-specifier" &&
				inheritance_chain_names.find(LastComponent(item->value)) !=
				inheritance_chain_names.end();
			if(chain_class) {
				first_chain = min(first_chain, child);
				continue;
			}
			for(size_t generated = 0; generated < ordered_chain.size(); ++generated) {
				const string name = LastComponent(ordered_chain[generated]->value);
				if(MentionsGeneratedType(item, name)) first_use = min(first_use, child);
				vector<CPPGMAstNodePtr> source(1, item);
				if(OrderingTypeDeclaration(item) && MentionsGeneratedLayoutClass(
					ordered_chain[generated], source))
					dependency_end = max(dependency_end, child + 1);
			}
		}
		size_t insertion_position = first_chain;
		if(first_use < insertion_position) insertion_position = first_use;
		if(dependency_end > insertion_position) insertion_position = dependency_end;
		vector<CPPGMAstNodePtr> reordered;
		bool inserted = false;
		for(size_t child = 0; child < result->children.size(); ++child) {
			const CPPGMAstNodePtr& item = result->children[child];
			if(!inserted && child >= insertion_position) {
				reordered.insert(reordered.end(), ordered_chain.begin(), ordered_chain.end());
				inserted = true;
			}
			const string name = item ? LastComponent(item->value) : string();
			if(item && (item->kind == "class-specifier" ||
				item->kind == "class-forward-declaration") &&
				inheritance_chain_names.find(name) != inheritance_chain_names.end()) continue;
			reordered.push_back(item);
		}
		if(!inserted) reordered.insert(reordered.end(), ordered_chain.begin(), ordered_chain.end());
		result->children.swap(reordered);
	}
	return result;
}

size_t PA18TemplateExpander::EstimateTypeSize(string raw, const string& context) const
{
	raw = CanonicalSpelling(raw);
	while(raw.compare(0, 8, "typename") == 0 &&
		(raw.size() == 8 || isspace(static_cast<unsigned char>(raw[8]))))
		raw = CanonicalSpelling(raw.substr(8));
	while(raw.compare(0, 6, "const ") == 0 ||
		raw.compare(0, 9, "volatile ") == 0) {
		raw = CanonicalSpelling(raw.substr(raw.find(' ') + 1));
	}
	const size_t array_open = raw.find('[');
	if(array_open != string::npos) {
		const size_t array_close = raw.find(']', array_open);
		if(array_close != string::npos) {
			const string bound_text = CanonicalSpelling(raw.substr(array_open + 1,
				array_close - array_open - 1));
			char* bound_end = 0;
			long count = strtol(bound_text.c_str(), &bound_end, 10);
			if(!bound_end || *bound_end != '\0') {
				PA19ConstantExpressionParser parser(constant_values_,
					map<string, string>(), constant_type_sizes_,
					constant_type_alignments_, type_aliases_);
				PA19IntegralValue bound;
				if(!parser.Evaluate(bound_text, &bound)) return 0;
				count = PA19Signed(bound);
			}
			if(count >= 0) {
				string element = CanonicalSpelling(raw.substr(0, array_open));
				const size_t reference = element.rfind("(&");
				if(reference != string::npos) element = CanonicalSpelling(element.substr(0, reference));
				if(count == 0) {
					const vector<PA19IntegralValue>* values = FindConstantArray(element, context);
					const size_t array_size = values ? EstimateTypeSize(element, context) : 0;
					if(values && !values->empty() && array_size &&
						array_size % values->size() == 0)
						return array_size / values->size();
					return 0;
				}
				return EstimateTypeSize(element, context) * static_cast<size_t>(count);
			}
		}
	}
	if(!raw.empty() && (raw[raw.size() - 1] == '*' || raw[raw.size() - 1] == '&')) return 8;
	const PA19IntegralType fundamental = PA19Type(raw);
	if(fundamental.integral) return fundamental.bits <= 8 ? 1 :
		fundamental.bits <= 16 ? 2 : fundamental.bits <= 32 ? 4 : 8;
	map<string, string>::const_iterator alias = type_aliases_.find(raw);
	if(alias != type_aliases_.end() && alias->second != raw)
		return EstimateTypeSize(alias->second, context);
	map<string, size_t>::const_iterator direct = constant_type_sizes_.find(raw);
	if(direct != constant_type_sizes_.end()) return direct->second;
	for(string current = context; ; ) {
		const string candidate = JoinPath(current, raw);
		map<string, size_t>::const_iterator found = constant_type_sizes_.find(candidate);
		if(found != constant_type_sizes_.end()) return found->second;
		if(current.empty()) break;
		const size_t separator = current.rfind("::");
		if(separator == string::npos) current.clear();
		else current.erase(separator);
	}
	return 0;
}

void PA18TemplateExpander::RecordClassTypeSize(const CPPGMAstNodePtr& node,
	const string& context, const string& class_path)
{
	if(!node || (node->kind != "class-specifier" &&
		node->kind != "class-forward-declaration")) return;
	size_t offset = 0;
	size_t alignment = 1;
	for(size_t i = 0; i < node->children.size(); ++i) {
		const CPPGMAstNodePtr child = node->children[i];
		if(!child || child->kind != "simple-declaration" || child->children.empty()) continue;
		const string specifiers = SpellNode(child->children[0]);
		if(specifiers.find("typedef") != string::npos ||
			specifiers.find("static") != string::npos) continue;
		const string base = NodeTypeSpelling(child->children[0]);
		const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
		if(!list) continue;
		for(size_t j = 0; j < list->children.size(); ++j) {
			const CPPGMAstNodePtr item = list->children[j];
			if(!item || item->children.empty()) continue;
			const CPPGMAstNodePtr declarator = item->children[0];
			if(DescendantOfKind(declarator, "parameter-clause")) continue;
			const string spelling = DeclaratorTypeSpelling(base, declarator);
			const size_t size = EstimateTypeSize(spelling, class_path);
			if(!size) continue;
			const size_t member_alignment = size > 8 ? 8 : size;
			alignment = max(alignment, member_alignment);
			offset = (offset + member_alignment - 1) / member_alignment * member_alignment;
			offset += size;
		}
	}
	if(!offset) offset = 1;
	offset = (offset + alignment - 1) / alignment * alignment;
	constant_type_sizes_[class_path] = offset;
	constant_type_alignments_[class_path] = alignment;
	// Function-local classes are promoted before the template rewriter sees
	// their uses.  Keep the computed layout under that promoted identity too,
	// so a deferred `sizeof(Local)` in a template default observes the same
	// complete type as the generated class declaration.
	map<string, string>::const_iterator promoted = local_class_names_.find(class_path);
	if(promoted != local_class_names_.end() && !promoted->second.empty()) {
		constant_type_sizes_[promoted->second] = offset;
		constant_type_alignments_[promoted->second] = alignment;
	}
	const string short_name = LastComponent(class_path);
	if(constant_type_sizes_.find(short_name) == constant_type_sizes_.end()) {
		constant_type_sizes_[short_name] = offset;
		constant_type_alignments_[short_name] = alignment;
	}
}

// Type spelling and alias helpers live out of line to keep the collection
// declaration compact while retaining access to its typed semantic state.
string PA18TemplateExpander::InheritedTypeName(const string& scope, const string& name,
	set<string>* active) const
{
	if(!active || scope.empty() || !active->insert(scope).second) return string();
	map<string, CPPGMAstNodePtr>::const_iterator found = class_declarations_.find(scope);
	if(found == class_declarations_.end() || !found->second) {
		active->erase(scope);
		return string();
	}
	const CPPGMAstNodePtr& declaration = found->second;
	for(size_t i = 0; i < declaration->children.size(); ++i) {
		const CPPGMAstNodePtr clause = declaration->children[i];
		if(!clause || clause->kind != "base-clause") continue;
		for(size_t j = 0; j < clause->children.size(); ++j) {
			const CPPGMAstNodePtr base = ChildOfKindLocal(clause->children[j], "base-name");
			if(!base) continue;
			const string base_name = CanonicalSpelling(base->value);
			if(named_type_contexts_.find(JoinPath(base_name, name)) !=
				named_type_contexts_.end() || type_aliases_.find(JoinPath(base_name, name)) !=
				type_aliases_.end()) {
				active->erase(scope);
				return JoinPath(base_name, name);
			}
			const string inherited = InheritedTypeName(base_name, name, active);
			if(!inherited.empty()) {
				active->erase(scope);
				return inherited;
			}
		}
	}
	active->erase(scope);
	return string();
}
string PA18TemplateExpander::QualifyTypeArgument(string spelling, const string& context,
	const string& template_owner) const
{
	spelling = CanonicalSpelling(spelling);
	while(spelling.compare(0, 8, "typename") == 0 &&
		(spelling.size() == 8 || isspace(static_cast<unsigned char>(spelling[8]))))
		spelling = CanonicalSpelling(spelling.substr(8));
	const char* const elaborated_keys[] = {"struct", "class", "union"};
	for(size_t key = 0; key < sizeof(elaborated_keys) / sizeof(elaborated_keys[0]); ++key) {
		const string prefix_key = elaborated_keys[key];
		if(spelling.compare(0, prefix_key.size(), prefix_key) == 0 &&
			spelling.size() > prefix_key.size()) {
			const string candidate = spelling.substr(prefix_key.size());
			if(class_contexts_.find(candidate) != class_contexts_.end()) {
				spelling = candidate;
				break;
			}
		}
	}
	while(spelling.compare(0, 7, "struct ") == 0 ||
		spelling.compare(0, 6, "class ") == 0 ||
		spelling.compare(0, 6, "union ") == 0) {
		const size_t space = spelling.find(' ');
		spelling = CanonicalSpelling(spelling.substr(space + 1));
	}
	string prefix;
	if(spelling.compare(0, 6, "const ") == 0) {
		prefix = "const ";
		spelling = CanonicalSpelling(spelling.substr(6));
	} else if(spelling.compare(0, 9, "volatile ") == 0) {
		prefix = "volatile ";
		spelling = CanonicalSpelling(spelling.substr(9));
	}
	if(named_type_contexts_.find(spelling) != named_type_contexts_.end()) {
		const string enum_owner = PrefixComponent(spelling);
		const bool visible_from_context = context == enum_owner ||
			(context.size() > enum_owner.size() &&
				context.compare(0, enum_owner.size(), enum_owner) == 0 &&
				context[enum_owner.size()] == ':');
		if(visible_from_context) spelling = LastComponent(spelling);
	}
	size_t suffix_begin = spelling.find_first_of("*&");
	string suffix;
	if(suffix_begin != string::npos) {
		suffix = spelling.substr(suffix_begin);
		spelling = CanonicalSpelling(spelling.substr(0, suffix_begin));
	}
	map<string, string>::const_iterator local = local_class_names_.find(
		JoinPath(context, spelling));
	if(local != local_class_names_.end()) spelling = local->second;
	const bool direct_function = SplitDirectFunctionType(spelling, 0, 0, 0);
	if(spelling.size() > 5 && spelling.compare(spelling.size() - 5, 5, "const") == 0 &&
		spelling.find("::") == string::npos && !direct_function) {
	spelling = CanonicalSpelling(spelling.substr(0, spelling.size() - 5));
	prefix = "const ";
	}
	const size_t template_open = spelling.find('<');
	if(template_open != string::npos &&
		spelling.substr(0, template_open).find("::") == string::npos) {
		{
			const TemplateDefinition* definition = FindDefinition(
				spelling.substr(0, template_open), context);
			if(definition && !definition->qualified_name.empty())
				spelling = definition->qualified_name + spelling.substr(template_open);
		}
	}
	if(spelling.find("::") != string::npos && spelling[0] != ':') {
		// During replay the lexical context still names the primary class
		// (`direct_heap`), while the nested declaration belongs to the concrete
		// class identity (`direct_heap_int_`).  Recover that owner from the
		// active specialization before ordinary namespace lookup, so aliases
		// such as `impl::dispatcher` stay tied to the generated class scope.
		if(!active_instantiation_name_.empty()) {
			const size_t nested_separator = spelling.find("::");
			const string first = spelling.substr(0, nested_separator);
			const string remainder = spelling.substr(nested_separator + 2);
			map<string, string>::const_iterator active_base = specialization_bases_.find(
				LastComponent(active_instantiation_name_));
			const TemplateDefinition* active_definition = active_base ==
				specialization_bases_.end() ? 0 : FindDefinition(active_base->second, context);
			const CPPGMAstNodePtr active_declaration = active_definition ?
				active_definition->declaration : CPPGMAstNodePtr();
			function<bool(const CPPGMAstNodePtr&, const string&)> has_nested =
				[&](const CPPGMAstNodePtr& node, const string& path) {
					if(!node) return false;
					for(size_t child = 0; child < node->children.size(); ++child) {
						const CPPGMAstNodePtr& candidate = node->children[child];
						if(!candidate || (candidate->kind != "class-specifier" &&
							candidate->kind != "class-forward-declaration")) continue;
						if(LastComponent(candidate->value) == path) return true;
						const size_t separator = path.find("::");
						if(separator != string::npos && LastComponent(candidate->value) ==
							path.substr(0, separator) && has_nested(candidate,
							path.substr(separator + 2))) return true;
					}
					return false;
				};
			if(active_declaration && has_nested(active_declaration, spelling))
				spelling = active_instantiation_name_ + "::" + spelling;
		}
	const size_t separator = spelling.find("::");
	const string first = spelling.substr(0, separator);
	const string remainder = spelling.substr(separator);
	for(string current = context; ; ) {
		const string candidate = JoinPath(current, first);
		if(class_contexts_.find(candidate) != class_contexts_.end()) {
			spelling = candidate + remainder;
			break;
		}
		if(current.empty()) break;
		const size_t parent = current.rfind("::");
		if(parent == string::npos) current.clear();
		else current.erase(parent);
	}
	}
	if(!template_owner.empty()) {
	const string owner_prefix = template_owner + "::";
	if(spelling.compare(0, owner_prefix.size(), owner_prefix) == 0) {
		// Keep a fully-qualified generated specialization when it is used as a
		// type argument in a different lexical owner.  Stripping `std::` from
		// `std::pair_X*` is only valid inside `std`; a generated class emitted
		// before a dependent class in another namespace still needs that owner.
		const bool generated_specialization =
			class_contexts_.find(spelling) != class_contexts_.end() &&
			specialization_bases_.find(LastComponent(spelling)) !=
				specialization_bases_.end();
		if(!generated_specialization) spelling.erase(0, owner_prefix.size());
	}
	}
	if(spelling.find("::") == string::npos && spelling.find('<') == string::npos) {
	const bool direct_global_class = class_declarations_.find(spelling) !=
		class_declarations_.end() || named_type_contexts_.find(spelling) !=
		named_type_contexts_.end();
	const bool generated_owner_context = specialization_bases_.find(
		LastComponent(context)) != specialization_bases_.end();
	bool generated_enclosing_qualified = false;
	if(generated_owner_context) {
		map<string, string>::const_iterator generated_base = specialization_bases_.find(
			LastComponent(context));
		const string enclosing_owner = generated_base == specialization_bases_.end() ?
			string() : PrefixComponent(generated_base->second);
		const string enclosing_candidate = enclosing_owner.empty() ? string() :
			JoinPath(enclosing_owner, spelling);
		if(!enclosing_candidate.empty() && (class_contexts_.find(enclosing_candidate) !=
			class_contexts_.end() || named_type_contexts_.find(enclosing_candidate) !=
			 named_type_contexts_.end() || class_declarations_.find(enclosing_candidate) !=
			class_declarations_.end())) {
			spelling = enclosing_candidate;
			generated_enclosing_qualified = true;
		}
	}
	if(generated_enclosing_qualified)
		return CanonicalSpelling(prefix + spelling + suffix);
	string current = context;
	for(;;) {
		map<string, CPPGMAstNodePtr>::const_iterator class_declaration =
			class_declarations_.find(current);
		if(class_declaration != class_declarations_.end() &&
			specialization_bases_.find(LastComponent(current)) != specialization_bases_.end()) {
			const CPPGMAstNodePtr& declaration = class_declaration->second;
			const string member_alias = MemberAliasType(current, spelling);
			if(!member_alias.empty()) {
				spelling = member_alias;
				break;
			}
			for(size_t i = 0; i < declaration->children.size(); ++i)
				if(declaration->children[i] && declaration->children[i]->kind == "enum-specifier" &&
					LastComponent(declaration->children[i]->value) == spelling) {
					spelling = current + "::" + spelling;
					return CanonicalSpelling(prefix + spelling + suffix);
				}
		}
		const string candidate = JoinPath(current, spelling);
		if(class_contexts_.find(candidate) != class_contexts_.end() ||
			named_type_contexts_.find(candidate) != named_type_contexts_.end() ||
			(generated_owner_context && FindClassDeclaration(candidate, context))) {
			const bool candidate_declared = class_declarations_.find(candidate) !=
				class_declarations_.end();
			if(direct_global_class && !candidate_declared) {
				if(current.empty()) break;
				const size_t parent = current.rfind("::");
				if(parent == string::npos) current.clear();
				else current.erase(parent);
				continue;
			}
			const bool function_scope = function_contexts_.find(context) !=
				function_contexts_.end();
			const bool same_template_owner = !template_owner.empty() &&
				PrefixComponent(candidate) == template_owner;
			spelling = (function_scope && !same_template_owner) ||
				(!template_owner.empty() && !same_template_owner) ? candidate :
				LastComponent(candidate);
			break;
		}
		if(current.empty()) break;
		const size_t separator = current.rfind("::");
		if(separator == string::npos) current.clear();
		else current.erase(separator);
	}
	if(spelling.find("::") == string::npos) {
		set<string> active;
		const string inherited = InheritedTypeName(context, spelling, &active);
		if(!inherited.empty()) spelling = inherited;
	}
	}
	const string result = CollapseRepeatedQualifier(CanonicalSpelling(prefix + spelling + suffix));
	// Template replay can derive the same concrete class through an alias or
	// through the generated owner name.  Reuse an existing specialization when
	// its primary and typed template arguments are identical; otherwise PA14
	// sees two distinct class types and rejects the corresponding conversion.
	map<string, string>::const_iterator generated = specialization_bases_.find(
		LastComponent(result));
	map<string, vector<string> >::const_iterator generated_arguments =
		specialization_arguments_.find(LastComponent(result));
	if(generated != specialization_bases_.end() && generated_arguments !=
		specialization_arguments_.end()) {
		const string generated_namespace = PrefixComponent(generated->second);
		const function<string(const string&)> identity =
			[&](const string& raw_value) {
				string value = CollapseRepeatedQualifier(CanonicalSpelling(raw_value));
				const size_t open = value.find('<');
				if(open != string::npos) {
					string argument_text;
					size_t close = string::npos;
					if(TemplateRange(value, open, &argument_text, &close)) {
						const vector<string> arguments = SplitTemplateArguments(argument_text);
						string normalized = LastComponent(value.substr(0, open)) + "<";
						for(size_t argument = 0; argument < arguments.size(); ++argument) {
							if(argument) normalized += ",";
							normalized += identity(arguments[argument]);
						}
						return normalized + ">";
					}
				}
				if(value.find("::") != string::npos) {
					const string owner = PrefixComponent(value);
					if(owner == generated_namespace) return LastComponent(value);
				}
				return value;
			};
		const vector<string>& wanted = generated_arguments->second;
		string best = generated->first;
		for(map<string, string>::const_iterator candidate = specialization_bases_.begin();
			candidate != specialization_bases_.end(); ++candidate) {
			if(candidate->second != generated->second) continue;
			map<string, vector<string> >::const_iterator candidate_arguments =
				specialization_arguments_.find(candidate->first);
			if(candidate_arguments == specialization_arguments_.end() ||
				candidate_arguments->second.size() != wanted.size()) continue;
			bool same = true;
			for(size_t argument = 0; argument < wanted.size(); ++argument)
				if(identity(candidate_arguments->second[argument]) != identity(wanted[argument])) {
					same = false;
					break;
				}
			if(same && candidate->first.size() < best.size()) best = candidate->first;
		}
		if(best != generated->first) {
			const string owner = PrefixComponent(result);
			return owner.empty() ? best : owner + "::" + best;
		}
	}
	return result;
}
string PA18TemplateExpander::DeclaratorSuffix(const CPPGMAstNodePtr& declarator) const
{
	if(!declarator) return string();
	string result;
	for(size_t i = 0; i < declarator->children.size(); ++i) {
		const CPPGMAstNodePtr child = declarator->children[i];
		if(!child) continue;
		if(child->kind == "ptr-operator") {
			if(child->value.find("&&") != string::npos) result += "&&";
			else if(child->value.find('&') != string::npos) result += '&';
			else result += '*';
		} else if(child->kind == "cv-qualifier") result += RemoveMarker(child->value);
	}
	return result;
}
string PA18TemplateExpander::ReturnDeclaratorSuffix(const CPPGMAstNodePtr& declarator) const
{
	if(!declarator) return string();
	string result;
	for(size_t i = 0; i < declarator->children.size(); ++i) {
		const CPPGMAstNodePtr child = declarator->children[i];
		if(!child) continue;
		if(child->kind == "ptr-operator") {
			if(child->value.find("&&") != string::npos) result += "&&";
			else if(child->value.find('&') != string::npos) result += '&';
			else result += '*';
		} else if(child->kind == "nested-declarator")
			result += ReturnDeclaratorSuffix(child);
	}
	return result;
}
string PA18TemplateExpander::DeclaratorArraySuffix(const CPPGMAstNodePtr& declarator) const
{
	if(!declarator) return string();
	string result;
	for(size_t i = 0; i < declarator->children.size(); ++i) {
		const CPPGMAstNodePtr child = declarator->children[i];
		if(!child || child->kind != "array-suffix") continue;
		result += '[';
		if(!child->children.empty()) result += ConstantExpressionSpelling(child->children[0]);
		result += ']';
	}
	return result;
}
string PA18TemplateExpander::TypeIdSpelling(const CPPGMAstNodePtr& type_id) const
{
	if(!type_id) return string();
	const CPPGMAstNodePtr specs = ChildOfKindLocal(type_id, "type-specifier-seq");
	const string base = NodeTypeSpelling(specs);
	const CPPGMAstNodePtr abstract = ChildOfKindLocal(type_id, "abstract-declarator");
	if(!abstract) return base;
	const CPPGMAstNodePtr nested = ChildOfKindLocal(abstract, "nested-declarator");
	const CPPGMAstNodePtr clause = nested ? ChildOfKindLocal(nested, "parameter-clause") :
		ChildOfKindLocal(abstract, "parameter-clause");
	if(nested && clause) {
		const CPPGMAstNodePtr inner = nested->children.empty() ? CPPGMAstNodePtr() :
			ChildOfKindLocal(nested, "abstract-declarator");
		string result = base;
		result += inner && DeclaratorSuffix(inner).find('&') != string::npos ? "(&)(" : "(*)(";
		for(size_t i = 0; i < clause->children.size(); ++i) {
			const CPPGMAstNodePtr parameter = clause->children[i];
			if(!parameter || parameter->kind != "parameter-declaration") continue;
			if(result[result.size() - 1] != '(') result += ',';
			result += ParameterTypeSpelling(parameter);
		}
		result += ')';
		return CanonicalSpelling(result);
	}
	return DeclaratorTypeSpelling(base, abstract);
}
CPPGMAstNodePtr PA18TemplateExpander::FunctionDeclarator(const CPPGMAstNodePtr& declaration) const
{
	if(!declaration) return CPPGMAstNodePtr();
	if(declaration->kind == "function-definition" && declaration->children.size() > 1)
		return declaration->children[1];
	if(declaration->kind == "special-member-definition" ||
		declaration->kind == "special-member-declaration")
		return ChildOfKindLocal(declaration, "declarator");
	if(declaration->kind == "simple-declaration") {
		const CPPGMAstNodePtr list = ChildOfKindLocal(declaration, "init-declarator-list");
		if(list && !list->children.empty() && list->children[0] &&
			!list->children[0]->children.empty()) return list->children[0]->children[0];
	}
	return CPPGMAstNodePtr();
}

bool PA18TemplateExpander::TypeOnlyNode(const CPPGMAstNodePtr& node) const
{
	if(!node || node->kind == "class-specifier" ||
		node->kind == "class-forward-declaration" || node->kind == "enum-specifier" ||
		node->kind == "template-declaration" || node->kind == "empty-declaration") return true;
	if(node->kind == "namespace-definition") {
		for(size_t i = 0; i < node->children.size(); ++i)
			if(node->children[i] && node->children[i]->kind != "inline" &&
				(node->children[i]->kind == "simple-declaration" ||
				 node->children[i]->kind == "function-definition" ||
				 node->children[i]->kind == "alias-declaration" ||
				 !TypeOnlyNode(node->children[i]))) return false;
		return true;
	}
	// Function declarations and ordinary object declarations can mention a
	// generated complete type.  They must therefore remain after generated
	// class materializations rather than being treated as type-only nodes.
	if(node->kind == "simple-declaration") {
		const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
		return list && !list->children.empty() && !list->children[0]->children.empty() &&
			ChildOfKindLocal(list->children[0]->children[0], "parameter-clause");
	}
	return false;
}

bool PA18TemplateExpander::MentionsGeneratedType(const CPPGMAstNodePtr& node,
	const string& type_name) const
{
	if(!node || type_name.empty()) return false;
	const string wanted = LastComponent(CanonicalSpelling(RemoveMarker(type_name)));
	string spelling = CanonicalSpelling(RemoveMarker(node->value));
	if(!spelling.empty()) {
		const size_t angle = spelling.find('<');
		const string bare = angle == string::npos ? spelling : spelling.substr(0, angle);
		if(bare == type_name || bare == wanted ||
			bare.compare(0, wanted.size() + 2, wanted + "::") == 0 ||
			bare.find("::" + wanted + "::") != string::npos ||
			(bare.size() > wanted.size() + 2 &&
			 bare.compare(bare.size() - wanted.size() - 2, wanted.size() + 2,
				"::" + wanted) == 0)) return true;
		for(size_t position = spelling.find(wanted); position != string::npos;
			position = spelling.find(wanted, position + wanted.size())) {
			const bool left_boundary = position == 0 ||
				!IsIdentifierCharacter(spelling[position - 1]);
			const size_t end = position + wanted.size();
			const bool right_boundary = end == spelling.size() ||
				!IsIdentifierCharacter(spelling[end]);
			if(left_boundary && right_boundary) return true;
		}
	}
	for(size_t i = 0; i < node->children.size(); ++i)
		if(MentionsGeneratedType(node->children[i], type_name)) return true;
	return false;
}

bool PA18TemplateExpander::MentionsGeneratedClass(const CPPGMAstNodePtr& node,
	const vector<CPPGMAstNodePtr>& generated) const
{
	if(!node) return false;
	for(size_t i = 0; i < generated.size(); ++i) {
		if(!generated[i]) continue;
		string type_name = LastComponent(generated[i]->value);
		if(type_name.empty()) type_name = OrderingDeclaredName(generated[i]);
		if(type_name.empty()) continue;
		if(MentionsGeneratedType(node, type_name)) {
			return true;
		}
		const string spelling = CanonicalSpelling(RemoveMarker(node->value));
		if(!spelling.empty()) {
			const string resolved = ResolveAlias(spelling, string());
			if(resolved != spelling && MentionsGeneratedType(
				CPPGMAstNodePtr(new CPPGMAstNode("type-name", resolved)), type_name))
				return true;
		}
	}
	for(size_t i = 0; i < node->children.size(); ++i)
		if(MentionsGeneratedClass(node->children[i], generated)) return true;
	return false;
}

bool PA18TemplateExpander::MentionsGeneratedLayoutClass(
	const CPPGMAstNodePtr& node, const vector<CPPGMAstNodePtr>& generated) const
{
	if(!node) return false;
	if(node->kind == "base-clause")
		return MentionsGeneratedClass(node, generated);
	if(node->kind == "simple-declaration") {
		if(node->children.empty()) return false;
		const string specifiers = SpellNode(node->children[0]);
		if(specifiers.find("typedef") != string::npos ||
			specifiers.find("static") != string::npos ||
			DescendantOfKind(node, "parameter-clause")) return false;
		return MentionsGeneratedClass(node->children[0], generated);
	}
	if(node->kind == "class-specifier" || node->kind == "class-forward-declaration") {
		for(size_t i = 0; i < node->children.size(); ++i)
			if(MentionsGeneratedLayoutClass(node->children[i], generated)) return true;
	}
	return false;
}

bool PA18TemplateExpander::MentionsTemplateId(const CPPGMAstNodePtr& node) const
{
	if(!node) return false;
	if(node->kind == "template-id") return true;
	const string spelling = RemoveMarker(node->value);
	if(spelling.find('<') != string::npos && spelling.find('>') != string::npos)
		return true;
	for(size_t i = 0; i < node->children.size(); ++i)
		if(MentionsTemplateId(node->children[i])) return true;
	return false;
}

vector<CPPGMAstNodePtr> PA18TemplateExpander::OrderGeneratedClasses(
	const vector<CPPGMAstNodePtr>& input) const
{
	vector<CPPGMAstNodePtr> result;
	vector<bool> placed(input.size(), false);
	for(size_t count = 0; count < input.size(); ++count) {
		int selected = -1;
		for(size_t i = 0; i < input.size(); ++i) {
			if(placed[i] || !input[i]) continue;
			bool ready = true;
			for(size_t j = 0; j < input.size(); ++j) {
				if(i == j || placed[j] || !input[j]) continue;
				if(LastComponent(input[i]->value) == LastComponent(input[j]->value)) continue;
				vector<CPPGMAstNodePtr> dependency(1, input[j]);
				if(MentionsGeneratedLayoutClass(input[i], dependency)) {
					ready = false;
					break;
				}
			}
			if(ready) {
				selected = static_cast<int>(i);
				break;
			}
		}
		if(selected < 0) {
			size_t best_qualified_dependencies = static_cast<size_t>(-1);
			for(size_t i = 0; i < input.size(); ++i) {
				if(placed[i] || !input[i]) continue;
				size_t qualified_dependencies = 0;
				for(size_t j = 0; j < input.size(); ++j) {
					if(i == j || placed[j] || !input[j]) continue;
					if(MentionsQualifiedGeneratedType(input[i],
						LastComponent(input[j]->value))) ++qualified_dependencies;
				}
				if(selected < 0 || qualified_dependencies < best_qualified_dependencies) {
					selected = static_cast<int>(i);
					best_qualified_dependencies = qualified_dependencies;
				}
			}
		}
		if(selected < 0) break;
		placed[static_cast<size_t>(selected)] = true;
		result.push_back(input[static_cast<size_t>(selected)]);
	}
	return result;
}

bool PA18TemplateExpander::HasExternalCompleteDependency(
	const CPPGMAstNodePtr& node, const string& owner, set<string>* dependencies) const
{
	if(!node || !dependencies) return false;
	for(map<string, CPPGMAstNodePtr>::const_iterator declaration =
		class_declarations_.begin(); declaration != class_declarations_.end(); ++declaration) {
		const CPPGMAstNodePtr& source = declaration->second;
		if(!source || source->kind != "class-specifier" || source->children.size() <= 1)
			continue;
		const string qualified = declaration->first;
		map<string, TemplateDefinition>::const_iterator source_template =
			definitions_.find(qualified);
		if(source_template != definitions_.end() && source_template->second.class_template)
			continue;
		if(!PrefixComponent(qualified).empty() &&
			PrefixComponent(qualified) == owner) continue;
		if(specialization_bases_.find(LastComponent(qualified)) != specialization_bases_.end()) continue;
		const string name = LastComponent(qualified);
		if(!name.empty() && ContainsName(node, name)) dependencies->insert(name);
	}
	return !dependencies->empty();
}

bool PA18TemplateExpander::DeclaresSourceType(const CPPGMAstNodePtr& node,
	const set<string>& names) const
{
	if(!node) return false;
	if(node->kind == "class-specifier" || node->kind == "class-forward-declaration")
		return names.find(LastComponent(node->value)) != names.end();
	for(size_t i = 0; i < node->children.size(); ++i)
		if(DeclaresSourceType(node->children[i], names)) return true;
	return false;
}

void PA18TemplateExpander::InsertDeferredGenerated(const CPPGMAstNodePtr& node)
{
	if(!node || node->kind != "translation-unit" || deferred_generated_by_owner_.empty()) return;
	vector<vector<CPPGMAstNodePtr> > insertions(node->children.size() + 1);
	for(map<string, vector<CPPGMAstNodePtr> >::iterator deferred =
		deferred_generated_by_owner_.begin(); deferred != deferred_generated_by_owner_.end(); ++deferred) {
		vector<CPPGMAstNodePtr> generated = OrderGeneratedClasses(deferred->second);
		if(generated.empty()) continue;
		CPPGMAstNodePtr wrapper = MakeNamespaceForward(deferred->first, generated);
		if(!wrapper) continue;
		set<string> dependencies = deferred_generated_dependencies_[deferred->first];
		size_t position = 0;
		for(size_t child = 0; child < node->children.size(); ++child)
			if(DeclaresSourceType(node->children[child], dependencies))
				position = child + 1;
		insertions[position].push_back(wrapper);
	}
	vector<CPPGMAstNodePtr> reordered;
	reordered.reserve(node->children.size() + insertions.size());
	for(size_t i = 0; i <= node->children.size(); ++i) {
		reordered.insert(reordered.end(), insertions[i].begin(), insertions[i].end());
		if(i < node->children.size()) reordered.push_back(node->children[i]);
	}
	node->children.swap(reordered);
	deferred_generated_by_owner_.clear();
	deferred_generated_dependencies_.clear();
}

void PA18TemplateExpander::InsertGenerated(vector<CPPGMAstNodePtr>* children,
	const string& owner)
{
	if(!children) return;
	map<string, vector<CPPGMAstNodePtr> >::iterator found = generated_by_owner_.find(owner);
	if(found == generated_by_owner_.end() || found->second.empty()) return;
	if(!owner.empty() && class_contexts_.find(owner) == class_contexts_.end()) {
		vector<CPPGMAstNodePtr> retained;
		for(size_t i = 0; i < found->second.size(); ++i) {
			const CPPGMAstNodePtr& generated = found->second[i];
			set<string> dependencies;
			if(generated && generated->kind == "class-specifier" &&
				HasExternalCompleteDependency(generated, owner, &dependencies)) {
				vector<CPPGMAstNodePtr>& deferred = deferred_generated_by_owner_[owner];
				deferred.push_back(generated);
				deferred_generated_dependencies_[owner].insert(dependencies.begin(), dependencies.end());
			} else retained.push_back(generated);
		}
		found->second.swap(retained);
		if(found->second.empty()) return;
	}
	vector<CPPGMAstNodePtr> generated_classes;
	vector<CPPGMAstNodePtr> generated_variables;
	vector<CPPGMAstNodePtr> generated_functions;
	const auto dependent_generated_class = [&](const CPPGMAstNodePtr& generated) {
		if(!generated || (generated->kind != "class-specifier" &&
			generated->kind != "class-forward-declaration")) return false;
		const string local_name = LastComponent(generated->value);
		map<string, string>::const_iterator base = specialization_bases_.find(local_name);
		map<string, vector<string> >::const_iterator arguments =
			specialization_arguments_.find(local_name);
		if(base == specialization_bases_.end() || arguments == specialization_arguments_.end())
			return false;
		map<string, TemplateDefinition>::const_iterator definition = definitions_.find(base->second);
		if(definition == definitions_.end()) return false;
		const auto known_context_entity = [&](const string& value) {
			if(constant_values_.find(value) != constant_values_.end() ||
				class_contexts_.find(value) != class_contexts_.end() ||
				named_type_contexts_.find(value) != named_type_contexts_.end()) return true;
			const auto matches_owner = [&](const string& candidate) {
				if(owner.empty() || candidate == JoinPath(owner, value)) return true;
				string logical_owner = owner;
				map<string, string>::const_iterator logical = lexical_namespace_logical_.find(owner);
				if(logical != lexical_namespace_logical_.end()) logical_owner = logical->second;
				if(candidate == JoinPath(logical_owner, value)) return true;
				for(map<string, string>::const_iterator physical = lexical_namespace_logical_.begin();
					physical != lexical_namespace_logical_.end(); ++physical)
					if(physical->second == owner && candidate == JoinPath(physical->first, value))
						return true;
				return false;
			};
			for(set<string>::const_iterator candidate = class_contexts_.begin();
				candidate != class_contexts_.end(); ++candidate) {
				if(LastComponent(*candidate) != value) continue;
				if(matches_owner(*candidate)) return true;
			}
			for(set<string>::const_iterator candidate = named_type_contexts_.begin();
				candidate != named_type_contexts_.end(); ++candidate) {
				if(LastComponent(*candidate) != value) continue;
				if(matches_owner(*candidate)) return true;
			}
			return false;
		};
		for(size_t argument = 0; argument < arguments->second.size(); ++argument) {
			string value = NormalizeElaboratedSpelling(arguments->second[argument], owner);
			value = CanonicalSpelling(value);
			const char* const elaborated_keys[] = {"struct ", "class ", "union ", "enum "};
			for(size_t key = 0; key < sizeof(elaborated_keys) / sizeof(elaborated_keys[0]); ++key) {
				const string keyword = elaborated_keys[key];
				if(value.compare(0, keyword.size(), keyword) == 0) {
					value = CanonicalSpelling(value.substr(keyword.size()));
					break;
				}
			}
			bool bare_identifier = !value.empty() &&
				(isalpha(static_cast<unsigned char>(value[0])) || value[0] == '_');
			for(size_t character = 1; bare_identifier && character < value.size(); ++character)
				if(!IsIdentifierCharacter(value[character])) bare_identifier = false;
			const bool builtin = value == "bool" || value == "char" || value == "double" ||
				value == "float" || value == "int" || value == "long" ||
				value == "short" || value == "signed" || value == "unsigned" ||
				value == "void" || value == "wchar_t" || value == "char16_t" ||
				value == "char32_t";
			if(!bare_identifier || builtin || value == "true" || value == "false") continue;
			if(known_context_entity(value)) continue;
			if(argument < definition->second.parameters.size() &&
				!definition->second.parameters[argument].type) {
				if(constant_values_.find(value) == constant_values_.end()) return true;
			} else return true;
		}
		return false;
	};
	for(size_t i = 0; i < found->second.size(); ++i) {
		const CPPGMAstNodePtr& generated = found->second[i];
		if(!generated) continue;
		if(dependent_generated_class(generated)) continue;
		if(generated->kind == "class-specifier" ||
			generated->kind == "class-forward-declaration" ||
			generated->kind == "alias-declaration") generated_classes.push_back(generated);
		else if(generated->kind == "simple-declaration") generated_variables.push_back(generated);
		else generated_functions.push_back(generated);
	}
	generated_classes = OrderGeneratedClasses(generated_classes);
	vector<CPPGMAstNodePtr> generated_forwards;
	for(size_t i = 0; i < generated_classes.size(); ++i)
		if(generated_classes[i]->kind == "class-specifier" ||
			generated_classes[i]->kind == "class-forward-declaration")
			generated_forwards.push_back(MakeForwardClass(generated_classes[i]->value));
	if(!generated_forwards.empty())
		children->insert(children->begin(), generated_forwards.begin(), generated_forwards.end());

	size_t default_position = 0;
	while(default_position < children->size()) {
		const CPPGMAstNodePtr& child = (*children)[default_position];
		if(child && (child->kind == "function-definition" ||
			child->kind == "special-member-definition")) break;
		if(!TypeOnlyNode(child)) break;
		++default_position;
	}
	if(default_position < children->size() && MentionsTemplateId((*children)[default_position]))
		default_position = children->size();
	set<string> generated_names;
	for(size_t i = 0; i < generated_classes.size(); ++i)
		if(generated_classes[i]) generated_names.insert(generated_classes[i]->value);
	vector<size_t> positions(generated_classes.size(), default_position);
	for(size_t i = 0; i < generated_classes.size(); ++i) {
		const CPPGMAstNodePtr& generated = generated_classes[i];
		if(!generated) continue;
		for(size_t child = 0; child < children->size(); ++child) {
			const CPPGMAstNodePtr& source = (*children)[child];
			if(!source) continue;
			const bool generated_forward =
				source->kind == "class-forward-declaration" &&
				generated_names.find(source->value) != generated_names.end();
			if(generated_forward) continue;
			const string declared = OrderingDeclaredName(source);
			if(OrderingTypeDeclaration(source) && !declared.empty() &&
				generated_names.find(declared) == generated_names.end()) {
				vector<CPPGMAstNodePtr> source_dependency(1, source);
				// Static member initializers still need the complete source class
				// declaration for qualified lookup (for example `no::value`), even
				// though static storage itself does not contribute to object layout.
				// Keep this dependency local to the generated specialization instead
				// of treating every static declaration as a layout dependency.
				const bool static_member_dependency = source->kind == "class-specifier" &&
					MentionsQualifiedGeneratedType(generated, declared);
				if(MentionsGeneratedLayoutClass(generated, source_dependency) ||
					static_member_dependency)
					positions[i] = max(positions[i], child + 1);
			}
			if(MentionsGeneratedType(source, LastComponent(generated->value)))
				positions[i] = min(positions[i], child);
		}
	}
	for(size_t i = 0; i < generated_classes.size(); ++i) {
		for(map<string, vector<CPPGMAstNodePtr> >::const_iterator other =
			generated_by_owner_.begin(); other != generated_by_owner_.end(); ++other) {
				if(other->first.empty() || other->first == owner) continue;
				string relative_owner = other->first;
				if(!owner.empty() && (other->first == owner ||
					(other->first.size() > owner.size() &&
					 other->first.compare(0, owner.size(), owner) == 0 &&
					 other->first[owner.size()] == ':')))
					relative_owner = other->first.substr(owner.size() + 2);
			for(size_t dependency = 0; dependency < other->second.size(); ++dependency) {
					if(!other->second[dependency]) continue;
					vector<CPPGMAstNodePtr> dependency_node(1, other->second[dependency]);
					if(!MentionsGeneratedClass(generated_classes[i], dependency_node)) continue;
					for(size_t child = 0; child < children->size(); ++child)
						if(NamespacePathContains((*children)[child], relative_owner))
							positions[i] = max(positions[i], child + 1);
			}
			}
		}
	// Keep generated dependencies in the same source slot when their source
	// dependencies would otherwise place the dependent before its materialized
	// prerequisite.  OrderGeneratedClasses already makes the vector topological.
	for(size_t i = 0; i < generated_classes.size(); ++i) {
		for(size_t prior = 0; prior < i; ++prior)
			if(generated_classes[i] && generated_classes[prior] &&
				LastComponent(generated_classes[i]->value) !=
				LastComponent(generated_classes[prior]->value) &&
				MentionsGeneratedType(generated_classes[i],
					LastComponent(generated_classes[prior]->value)))
				positions[prior] = min(positions[prior], positions[i]);
	}
	vector<vector<CPPGMAstNodePtr> > insertions(children->size() + 1);
	for(size_t i = 0; i < generated_classes.size(); ++i) {
		const size_t position = min(positions[i], children->size());
		insertions[position].push_back(generated_classes[i]);
	}
	vector<CPPGMAstNodePtr> reordered;
	reordered.reserve(children->size() + generated_classes.size());
	for(size_t i = 0; i <= children->size(); ++i) {
		reordered.insert(reordered.end(), insertions[i].begin(), insertions[i].end());
		if(i < children->size()) reordered.push_back((*children)[i]);
	}
	children->swap(reordered);

	size_t function_position = children->size();
	for(size_t i = 0; i < children->size(); ++i) {
		const string& kind = (*children)[i]->kind;
		if(kind == "static-assert-declaration" || kind == "function-definition" ||
			kind == "special-member-definition") {
			function_position = i;
			break;
		}
	}
	// A generated constexpr function can be referenced by a static assertion
	// nested in a materialized class.  The ordinary top-level scan above cannot
	// see that use, so make the generated definition visible before its first
	// lexical caller as well.  Class bodies that only contain ordinary function
	// calls remain in their original order so dependent nested types are visible
	// before a generated function signature names them.
	for(size_t child = 0; child < children->size(); ++child) {
		if((*children)[child] && (*children)[child]->kind == "class-specifier" &&
			!ContainsStaticAssert((*children)[child])) continue;
		for(size_t function = 0; function < generated_functions.size(); ++function) {
			const string name = DeclarationName(generated_functions[function]);
			if(!name.empty() && ContainsName((*children)[child], name)) {
				function_position = min(function_position, child);
				break;
			}
		}
	}
	if(!generated_functions.empty())
		children->insert(children->begin() + function_position,
			generated_functions.begin(), generated_functions.end());
	if(!generated_variables.empty()) {
		size_t variable_position = children->size();
		for(size_t i = 0; i < children->size(); ++i) {
			const string& kind = (*children)[i]->kind;
			if(kind == "static-assert-declaration" || kind == "function-definition" ||
				kind == "special-member-definition") {
				variable_position = i;
				break;
			}
		}
		children->insert(children->begin() + variable_position,
			generated_variables.begin(), generated_variables.end());
	}
}

void PA18TemplateExpander::InjectGenerated(const CPPGMAstNodePtr& node,
	const string& context, const string& lexical_context)
{
	if(!node) return;
	if(node->kind == "translation-unit") {
		vector<CPPGMAstNodePtr> namespace_forwards;
		set<string> consumed_forwards;
		for(map<string, vector<CPPGMAstNodePtr> >::const_iterator it =
			generated_namespace_forwards_.begin(); it != generated_namespace_forwards_.end(); ++it) {
			// When the source contains the owner namespace, defer insertion to
			// that namespace's first lexical occurrence.  This matters for an
			// inline namespace: the logical owner `s::c` is lexically reached
			// through `s::i::c`, and a sibling root wrapper would not make a
			// forward visible inside that actual scope.
			if(!it->first.empty() && early_namespace_forwards_.find(it->first) ==
				early_namespace_forwards_.end() && (namespace_occurrences_.find(it->first) !=
				namespace_occurrences_.end() || lexical_namespace_paths_.find(it->first) !=
				lexical_namespace_paths_.end())) continue;
			CPPGMAstNodePtr wrapper = MakeNamespaceForward(it->first, it->second);
			if(wrapper) {
				namespace_forwards.push_back(wrapper);
				synthetic_namespace_forwards_.insert(wrapper.get());
			}
			else node->children.insert(node->children.begin(), it->second.begin(), it->second.end());
			consumed_forwards.insert(it->first);
		}
		for(set<string>::const_iterator it = consumed_forwards.begin();
			it != consumed_forwards.end(); ++it) generated_namespace_forwards_.erase(*it);
		if(!namespace_forwards.empty())
			node->children.insert(node->children.begin(), namespace_forwards.begin(), namespace_forwards.end());
		InsertGenerated(&node->children, context);
		for(size_t i = 0; i < node->children.size(); ++i) {
			if(node->children[i] && node->children[i]->kind == "class-specifier" &&
				node->children[i]->children.size() > 1) {
				const string class_path = JoinPath(context, LastComponent(node->children[i]->value));
				map<string, vector<CPPGMAstNodePtr> >::iterator before =
					generated_before_class_.find(class_path);
				if(before != generated_before_class_.end()) {
					const size_t inserted = before->second.size();
					node->children.insert(node->children.begin() + i,
						before->second.begin(), before->second.end());
					generated_before_class_.erase(before);
					i += inserted;
				}
			}
			InjectGenerated(node->children[i], context, context);
		}
		InsertDeferredGenerated(node);
		return;
	}
	if(node->kind == "namespace-definition") {
		if(synthetic_namespace_forwards_.find(node.get()) !=
			synthetic_namespace_forwards_.end()) return;
		const string child_context = IsInlineNamespace(node) || node->value.empty() ?
			context : JoinPath(context, node->value);
		const string child_lexical_context = node->value.empty() ? lexical_context :
			JoinPath(lexical_context, node->value);
		map<string, vector<CPPGMAstNodePtr> >::iterator forwards =
			generated_namespace_forwards_.find(child_context);
		if(forwards == generated_namespace_forwards_.end())
			forwards = generated_namespace_forwards_.find(child_lexical_context);
		if(forwards != generated_namespace_forwards_.end()) {
			node->children.insert(node->children.begin(), forwards->second.begin(),
				forwards->second.end());
			generated_namespace_forwards_.erase(forwards);
		}
		bool last_namespace = true;
		map<string, size_t>::iterator occurrence = namespace_occurrences_.find(child_lexical_context);
		if(occurrence != namespace_occurrences_.end() && occurrence->second > 0)
			last_namespace = --occurrence->second == 0;
		if(last_namespace) InsertGenerated(&node->children, child_lexical_context);
		for(size_t i = 0; i < node->children.size(); ++i) {
			if(node->children[i] && node->children[i]->kind == "class-specifier" &&
				node->children[i]->children.size() > 1) {
				const string class_path = JoinPath(child_context, LastComponent(node->children[i]->value));
				map<string, vector<CPPGMAstNodePtr> >::iterator before =
					generated_before_class_.find(class_path);
				if(before != generated_before_class_.end()) {
					const size_t inserted = before->second.size();
					node->children.insert(node->children.begin() + i,
						before->second.begin(), before->second.end());
					generated_before_class_.erase(before);
					i += inserted;
				}
			}
			InjectGenerated(node->children[i], child_context, child_lexical_context);
		}
		return;
	}
	if(node->kind == "class-specifier" || node->kind == "class-forward-declaration") {
		const string class_context = JoinPath(context, LastComponent(node->value));
		map<string, vector<CPPGMAstNodePtr> >::iterator found =
			generated_by_owner_.find(class_context);
		if(node->kind == "class-specifier" &&
			found != generated_by_owner_.end() && !found->second.empty()) {
			vector<CPPGMAstNodePtr> generated_classes;
			vector<CPPGMAstNodePtr> generated_forwards;
			vector<CPPGMAstNodePtr> generated_functions;
			for(size_t i = 0; i < found->second.size(); ++i) {
				const CPPGMAstNodePtr& generated = found->second[i];
				if(!generated) continue;
				if(generated->kind == "class-forward-declaration")
					generated_forwards.push_back(generated);
				else if(generated->kind == "class-specifier" ||
					generated->kind == "alias-declaration") generated_classes.push_back(generated);
				else generated_functions.push_back(generated);
			}
			generated_classes = OrderGeneratedClasses(generated_classes);
			vector<CPPGMAstNodePtr> generated;
			generated.insert(generated.end(), generated_forwards.begin(), generated_forwards.end());
			generated.insert(generated.end(), generated_classes.begin(), generated_classes.end());
			generated.insert(generated.end(), generated_functions.begin(), generated_functions.end());
			set<string> complete_names;
			for(size_t i = 0; i < generated_classes.size(); ++i)
				if(generated_classes[i]->kind == "class-specifier" &&
					generated_classes[i]->children.size() > 1)
					complete_names.insert(generated_classes[i]->value);
			if(!complete_names.empty()) {
				for(size_t i = 0; i < node->children.size();) {
					const CPPGMAstNodePtr& child = node->children[i];
					if(child && child->kind == "class-forward-declaration" &&
						complete_names.find(child->value) != complete_names.end())
						node->children.erase(node->children.begin() + i);
					else ++i;
				}
			}
			size_t position = node->children.size();
			for(size_t i = 0; i < node->children.size(); ++i)
				if(node->children[i] && node->children[i]->kind == "class-key") {
					position = i + 1;
					break;
				}
			if(!generated.empty()) node->children.insert(node->children.begin() + position,
				generated.begin(), generated.end());
		}
		for(size_t i = 0; i < node->children.size(); ++i) {
			if(node->children[i] && node->children[i]->kind == "class-specifier" &&
				node->children[i]->children.size() > 1) {
				const string nested_path = JoinPath(class_context,
					LastComponent(node->children[i]->value));
				map<string, vector<CPPGMAstNodePtr> >::iterator before =
					generated_before_class_.find(nested_path);
				if(before != generated_before_class_.end()) {
					const size_t inserted = before->second.size();
					node->children.insert(node->children.begin() + i,
						before->second.begin(), before->second.end());
					generated_before_class_.erase(before);
					i += inserted;
				}
			}
			InjectGenerated(node->children[i], class_context,
				JoinPath(lexical_context, LastComponent(node->value)));
		}
		return;
	}
	for(size_t i = 0; i < node->children.size(); ++i)
		InjectGenerated(node->children[i], context, lexical_context);
}

void PA18TemplateExpander::CollectVariables(const CPPGMAstNodePtr& node,
	const string& context)
{
	if(!node) return;
	string function_context = context;
	if(node->kind == "function-definition") {
		const string function_name = DeclarationName(node);
		function_context = JoinPath(context, function_name);
		if(!function_name.empty() && LastComponent(context) == function_name)
			function_context = context;
	} else if(node->kind == "special-member-definition" ||
		node->kind == "special-member-declaration") {
		const string function_name = DeclarationName(node);
		if(!function_name.empty()) function_context = JoinPath(context, function_name);
	}
	if((node->kind == "function-definition" ||
		node->kind == "special-member-definition") && node->children.size() > 1) {
		const CPPGMAstNodePtr declarator = node->kind == "function-definition" ?
			node->children[1] : ChildOfKindLocal(node, "declarator");
		const CPPGMAstNodePtr parameters = DescendantOfKind(declarator,
			"parameter-clause");
		if(parameters) for(size_t i = 0; i < parameters->children.size(); ++i) {
			const CPPGMAstNodePtr parameter = parameters->children[i];
			if(!parameter || parameter->kind != "parameter-declaration" ||
				parameter->children.size() < 2) continue;
			const string name = FirstIdentifierLocal(parameter->children[1]);
			if(!name.empty()) {
				const string type = ParameterTypeSpelling(parameter);
				variable_types_[name] = type;
				function_parameter_types_[function_context][name] = type;
			}
		}
	}
	if(node->kind == "simple-declaration") {
		const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
		if(list) for(size_t i = 0; i < list->children.size(); ++i) {
			const CPPGMAstNodePtr item = list->children[i];
			if(!item || item->children.empty()) continue;
			const CPPGMAstNodePtr clause = DescendantOfKind(item->children[0],
				"parameter-clause");
			if(!clause) continue;
			for(size_t j = 0; j < clause->children.size(); ++j) {
				const CPPGMAstNodePtr parameter = clause->children[j];
				if(!parameter || parameter->kind != "parameter-declaration" ||
					parameter->children.size() < 2) continue;
				const string name = FirstIdentifierLocal(parameter->children[1]);
				if(!name.empty()) {
					const string type = ParameterTypeSpelling(parameter);
					variable_types_[name] = type;
					function_parameter_types_[JoinPath(context, DeclarationName(node))][name] = type;
				}
			}
		}
	}
	if(node->kind == "simple-declaration" && !node->children.empty()) {
		const CPPGMAstNodePtr specs = node->children[0];
		const string type = NodeTypeSpelling(specs);
		const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
		if(list) for(size_t i = 0; i < list->children.size(); ++i) {
			const CPPGMAstNodePtr item = list->children[i];
			if(!item || item->children.empty()) continue;
			const string name = FirstIdentifierLocal(item->children[0]);
			if(!name.empty() && !type.empty())
				variable_types_[name] = DeclaratorTypeSpelling(type, item->children[0]);
		}
	}
	for(size_t i = 0; i < node->children.size(); ++i) {
		const string child_context = node->kind == "function-definition" &&
			node->children[i] && node->children[i]->kind == "compound-statement" ?
			function_context : context;
		CollectVariables(node->children[i], child_context);
	}
}

} // namespace pa18_templates_internal

vector<CPPGMAstNodePtr> ExpandPA18Templates(const vector<CPPGMAstNodePtr>& translation_units)
{
	for(size_t i = 0; i < translation_units.size(); ++i)
		if(NeedsPA18Expansion(translation_units[i])) {
			PA18TemplateExpander expander;
			return expander.Run(translation_units);
		}
	return translation_units;
}
