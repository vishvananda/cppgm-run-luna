#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
using namespace std;

namespace pa18_templates_internal {

bool IsTemplateQualifiedIdentifier(const string& raw, size_t identifier_begin)
{
	size_t qualifier_end = identifier_begin;
	while(qualifier_end > 0 &&
		isspace(static_cast<unsigned char>(raw[qualifier_end - 1]))) --qualifier_end;
	if(qualifier_end < 8 || raw.compare(qualifier_end - 8, 8, "template") != 0)
		return false;
	return qualifier_end == 8 ||
		!IsIdentifierCharacter(raw[qualifier_end - 9]);
}

bool IsDeletedFunctionDeclaration(const CPPGMAstNodePtr& declaration)
{
	const CPPGMAstNodePtr deleted = DescendantOfKind(declaration, "special-initializer");
	return deleted && RemoveMarker(deleted->value) == "delete";
}

void PA18TemplateExpander::EnsureTypeDependency(const string& spelling, const string& context,
		const string& owner)
	{
		// PA10 preserves an elaborated type used as a template argument (for
		// example `If<false, struct PrivateNat, int>`).  Register that
		// incomplete class before alias replay removes the elaborated keyword.
		// Restrict this recovery to an actual template-argument list: a source
		// declaration such as `struct box` must not manufacture a competing
		// forward that can change later partial-specialization lookup.
		const string source = CanonicalSpelling(spelling);
		const char* const elaborated[] = {"struct ", "class ", "union "};
		for(size_t key = 0; key < sizeof(elaborated) / sizeof(elaborated[0]); ++key) {
			const string marker = elaborated[key];
			for(size_t at = source.find(marker); at != string::npos;
				at = source.find(marker, at + marker.size())) {
				if(at > 0 && IsIdentifierCharacter(source[at - 1])) continue;
				int angle_depth = 0;
				for(size_t position = 0; position < at; ++position) {
					if(source[position] == '<') ++angle_depth;
					else if(source[position] == '>' && angle_depth > 0) --angle_depth;
				}
				if(angle_depth == 0) continue;
				size_t begin = at + marker.size();
				while(begin < source.size() && isspace(static_cast<unsigned char>(source[begin]))) ++begin;
				size_t end = begin;
				while(end < source.size() && (IsIdentifierCharacter(source[end]) ||
					source[end] == ':')) ++end;
				if(end == begin) continue;
				const string candidate = CanonicalSpelling(source.substr(begin, end - begin));
				if(candidate.empty() || candidate.find('<') != string::npos) continue;
				bool known = class_contexts_.find(candidate) != class_contexts_.end() ||
					class_declarations_.find(candidate) != class_declarations_.end();
				string qualified = candidate;
				for(string current = context; !known && !current.empty();) {
					const string scoped = JoinPath(current, candidate);
					if(class_contexts_.find(scoped) != class_contexts_.end() ||
						class_declarations_.find(scoped) != class_declarations_.end()) {
						qualified = scoped;
						known = true;
					}
					const size_t separator = current.rfind("::");
					if(separator == string::npos) current.clear();
					else current.erase(separator);
				}
				if(known) continue;
				string declaration_owner = context;
				if(function_contexts_.find(declaration_owner) != function_contexts_.end())
					declaration_owner = PrefixComponent(declaration_owner);
				qualified = declaration_owner.empty() ? candidate :
					JoinPath(declaration_owner, candidate);
				if(class_contexts_.find(qualified) != class_contexts_.end()) continue;
				const CPPGMAstNodePtr forward = MakeForwardClass(LastComponent(candidate));
				class_declarations_[qualified] = forward;
				RememberClassPath(qualified);
				vector<CPPGMAstNodePtr>& forwards = generated_namespace_forwards_[
					PrefixComponent(qualified)];
				bool already_queued = false;
				for(size_t item = 0; item < forwards.size(); ++item)
					if(forwards[item] && LastComponent(forwards[item]->value) ==
						LastComponent(candidate)) already_queued = true;
				if(!already_queued) forwards.push_back(forward);
			}
		}
		const string qualified = QualifyTypeArgument(
			NormalizeElaboratedSpelling(spelling, context), context);
		if(!qualified.empty()) EnsureForwardClass(qualified, context, owner);
	}
	void PA18TemplateExpander::EnsureTemplateDeclarationDependencies(
		const TemplateDefinition& definition, const string& owner)
	{
		for(size_t dependency = 0;
			dependency < definition.declaration_type_dependencies.size(); ++dependency) {
			const TemplateTypeDependency& item =
				definition.declaration_type_dependencies[dependency];
			EnsureTypeDependency(item.spelling, item.context, owner);
		}
	}
	void PA18TemplateExpander::EnsureDeclarationDependencies(const CPPGMAstNodePtr& node,
		const string& context, const string& owner)
	{
		if(!node) return;
		string child_context = context;
		if(node->kind == "class-specifier" || node->kind == "class-forward-declaration")
			child_context = JoinPath(context, LastComponent(node->value));
		if(node->kind == "function-definition" && !node->children.empty()) {
			EnsureTypeDependency(NodeTypeSpelling(node->children[0]), context, owner);
			if(node->children.size() > 1) {
				const CPPGMAstNodePtr clause = DescendantOfKind(node->children[1], "parameter-clause");
				if(clause) for(size_t i = 0; i < clause->children.size(); ++i)
					if(clause->children[i] && clause->children[i]->kind == "parameter-declaration")
						EnsureTypeDependency(ParameterTypeSpelling(clause->children[i]), context, owner);
			}
		}
		if(node->kind == "simple-declaration" && !node->children.empty())
			EnsureTypeDependency(NodeTypeSpelling(node->children[0]), context, owner);
		if(node->kind == "base-name") EnsureTypeDependency(node->value, context, owner);
		for(size_t i = 0; i < node->children.size(); ++i)
			EnsureDeclarationDependencies(node->children[i], child_context, owner);
	}

void PA18TemplateExpander::RememberClassPath(const string& path)
{
	if(path.empty()) return;
	class_contexts_.insert(path);
	const string name = LastComponent(path);
	if(name.empty()) return;
	vector<string>& paths = class_paths_by_name_[name];
	if(find(paths.begin(), paths.end(), path) == paths.end()) paths.push_back(path);
}

void PA18TemplateExpander::EnsureForwardClass(const string& spelling,
	const string& context, const string& owner)
{
	string type = CanonicalSpelling(spelling);
	while(type.compare(0, 6, "const ") == 0)
		type = CanonicalSpelling(type.substr(6));
	while(type.compare(0, 9, "volatile ") == 0)
		type = CanonicalSpelling(type.substr(9));
	while(!type.empty() && (type[type.size() - 1] == '*' ||
		type[type.size() - 1] == '&'))
		type = CanonicalSpelling(type.substr(0, type.size() - 1));
	const size_t separator = type.find("::");
	if(class_contexts_.find(type) == class_contexts_.end()) return;
	if(separator != string::npos && class_declarations_.find(type) !=
		class_declarations_.end() && class_contexts_.find(type.substr(0, separator)) !=
		class_contexts_.end()) return;
	if(class_declarations_.find(type) != class_declarations_.end() &&
		class_contexts_.find(context) == class_contexts_.end()) {
		string logical_owner = owner;
		map<string, string>::const_iterator logical = lexical_namespace_logical_.find(owner);
		if(logical != lexical_namespace_logical_.end()) logical_owner = logical->second;
		if(logical_owner == PrefixComponent(type) || owner == PrefixComponent(type)) return;
	}
	if(separator == string::npos) {
		vector<CPPGMAstNodePtr>& forwards = generated_namespace_forwards_[string()];
		for(size_t i = 0; i < forwards.size(); ++i)
			if(LastComponent(forwards[i]->value) == LastComponent(type)) return;
		forwards.push_back(MakeForwardClass(LastComponent(type)));
		return;
	}
	const string top = type.substr(0, separator);
	if(class_contexts_.find(top) == class_contexts_.end()) {
		string owner_name = PrefixComponent(type);
		// An inline namespace keeps the logical spelling (`lib::T`) visible to
		// source lookup while generated declarations are emitted in its physical
		// scope (`lib::abi`).  A dependency forward must follow that physical
		// owner; otherwise PA11 creates a second incomplete `lib::T` type before
		// it sees the complete `lib::abi::T` specialization.
		map<string, string>::const_iterator physical_logical =
			lexical_namespace_logical_.find(owner);
		if(physical_logical != lexical_namespace_logical_.end() &&
			physical_logical->second == owner_name) {
			// `owner` is the generated lexical owner supplied by the replay.
			// Keep the namespace path used by the dependency queue physical.
			owner_name = owner;
		}
		// A qualified nested type can have a namespace as its first component
		// while its immediate owner is a class.  Route that forward into the
		// class replay queue instead of wrapping the class name as a namespace.
		if(!owner_name.empty() && (class_contexts_.find(owner_name) !=
			class_contexts_.end() || class_declarations_.find(owner_name) !=
			class_declarations_.end() || FindClassDeclaration(owner_name, context))) {
			vector<CPPGMAstNodePtr>& class_forwards = generated_by_owner_[owner_name];
			for(size_t i = 0; i < class_forwards.size(); ++i)
				if(LastComponent(class_forwards[i]->value) == LastComponent(type)) return;
			class_forwards.push_back(MakeForwardClass(LastComponent(type)));
			return;
		}
		vector<CPPGMAstNodePtr>& forwards = generated_namespace_forwards_[owner_name];
		if(owner != owner_name) early_namespace_forwards_.insert(owner_name);
		for(size_t i = 0; i < forwards.size(); ++i)
			if(LastComponent(forwards[i]->value) == LastComponent(type)) return;
		forwards.push_back(MakeForwardClass(LastComponent(type)));
		return;
	}
	// A dependency which names a nested class of the specialization being
	// materialized is already supplied by that specialization's class shell.
	// Building another shell for the top component here would create a
	// duplicate member class beside the real one.
	if(!context.empty() && top == PrefixComponent(context)) return;
	if(!generated_forward_classes_.insert(top).second) return;
	CPPGMAstNodePtr forward = MakeClassShell(top);
	string remainder = type.substr(separator + 2);
	CPPGMAstNodePtr current = forward;
	while(!remainder.empty()) {
		const size_t next = remainder.find("::");
		const string part = remainder.substr(0, next);
		if(part.empty()) break;
		CPPGMAstNodePtr nested = next == string::npos ? MakeForwardClass(part) :
			CPPGMAstNodePtr(new CPPGMAstNode("class-specifier", part));
		if(next != string::npos)
			nested->children.push_back(CPPGMAstNodePtr(
				new CPPGMAstNode("class-key", "KW_STRUCT:struct")));
		current->children.push_back(nested);
		current = nested;
		if(next == string::npos) break;
		remainder.erase(0, next + 2);
	}
	if(class_contexts_.find(context) != class_contexts_.end() && context != owner)
		generated_before_class_[context].push_back(forward);
	else
		generated_by_owner_[owner].push_back(forward);
}

bool LooksLikeRelationalLessThan(const string& raw, size_t position)
{
	int enclosing_parentheses = 0;
	for(size_t i = 0; i < position; ++i) {
		if(raw[i] == '(') ++enclosing_parentheses;
		else if(raw[i] == ')' && enclosing_parentheses > 0) --enclosing_parentheses;
	}
	if(enclosing_parentheses == 0) {
		const bool spaced_before = position > 0 &&
			isspace(static_cast<unsigned char>(raw[position - 1]));
		const bool spaced_after = position + 1 < raw.size() &&
			isspace(static_cast<unsigned char>(raw[position + 1]));
		size_t next = position + 1;
		while(next < raw.size() && isspace(static_cast<unsigned char>(raw[next]))) ++next;
		const bool equality = next < raw.size() && raw[next] == '=';
		string right_word;
		if(next < raw.size() && (isalpha(static_cast<unsigned char>(raw[next])) || raw[next] == '_')) {
			size_t end = next + 1;
			while(end < raw.size() && IsIdentifierCharacter(raw[end])) ++end;
			right_word = raw.substr(next, end - next);
		}
		size_t left_end = position;
		while(left_end > 0 && isspace(static_cast<unsigned char>(raw[left_end - 1]))) --left_end;
		const bool left_is_closed = left_end > 0 && raw[left_end - 1] == ')';
		size_t left_begin = left_end;
		while(left_begin > 0 && IsIdentifierCharacter(raw[left_begin - 1])) --left_begin;
		const string left_word = raw.substr(left_begin, left_end - left_begin);
		const bool parameter_like = left_word.size() == 1 &&
			isupper(static_cast<unsigned char>(left_word[0]));
		const bool size_word = right_word == "sizeof" || right_word == "alignof" ||
			right_word == "__alignof" || right_word == "decltype";
		if(parameter_like && next < raw.size() &&
			(isdigit(static_cast<unsigned char>(raw[next])) || raw[next] == '.')) {
			for(size_t scan = next; scan < raw.size(); ++scan) {
				if(raw[scan] == '+' || raw[scan] == '-' || raw[scan] == '*' ||
					raw[scan] == '/' || raw[scan] == '%') return true;
				if(raw[scan] == ',' || raw[scan] == '>') break;
			}
		}
		if(equality || (spaced_before && spaced_after && size_word &&
			(left_is_closed || parameter_like))) return true;
	}
	if(enclosing_parentheses == 0) return false;
	int nested_parentheses = 0, angle = 1;
	for(size_t i = position + 1; i < raw.size(); ++i) {
		if(raw[i] == '(') { ++nested_parentheses; continue; }
		if(raw[i] == ')') {
			if(nested_parentheses > 0) { --nested_parentheses; continue; }
			return angle != 0;
		}
		if(raw[i] == '<' && IsTemplateAngleOpen(raw, i)) { ++angle; continue; }
		if(raw[i] == '>' && IsTemplateAngleClose(raw, i) && angle > 0 && --angle == 0)
			return false;
	}
	return angle != 0;
}

string PA18TemplateExpander::QualifyAliasTarget(const string& target,
	const string& alias) const
{
	const string owner = PrefixComponent(alias);
	// Qualified names in an alias declaration are still relative to the alias's
	// namespace.  For example, `traits::has_description` may target
	// `detail::has_description<T>`, which denotes `traits::detail::...`, not a
	// global `detail::...`.  Qualify only when the typed owner contains that
	// entity so ordinary globally qualified-looking spellings remain unchanged.
	if(owner.empty() || target.empty() || target[0] == ':') return target;
	const string qualified_target = JoinPath(owner, target.substr(0, target.find('<')));
	const bool typed_target = definitions_.find(qualified_target) != definitions_.end() ||
		class_declarations_.find(qualified_target) != class_declarations_.end();
	return typed_target ? JoinPath(owner, target) : target;
}

bool HasFriendSpecifier(const CPPGMAstNodePtr& node)
{
	if(!node) return false;
	if((node->kind == "decl-specifier" || node->kind == "decl-specifier-seq") &&
		(node->value == "KW_FRIEND:friend" || node->value == "friend")) return true;
	for(size_t i = 0; i < node->children.size(); ++i)
		if(HasFriendSpecifier(node->children[i])) return true;
	return false;
}

void PA18TemplateExpander::IndexUsingDirectiveDefinition(
	const TemplateDefinition& definition)
{
	const TemplateDefinition* typed_definition = &definition;
	const string& qualified = definition.qualified_name;
	size_t component_begin = 0;
	string target;
	while(true) {
		const size_t separator = qualified.find("::", component_begin);
		if(separator == string::npos) break;
		const string component = qualified.substr(component_begin,
			separator - component_begin);
		if(component.empty()) break;
		if(!target.empty()) target += "::";
		target += component;
		const size_t visible_begin = separator + 2;
		if(visible_begin < qualified.size()) {
			vector<const TemplateDefinition*>& exports = using_directive_exports_[target];
			if(find(exports.begin(), exports.end(), typed_definition) == exports.end())
				exports.push_back(typed_definition);
		}
		component_begin = visible_begin;
	}
}

string PA18TemplateExpander::GeneratedOwner(const TemplateDefinition& definition) const
{
	return definition.lexical_owner.empty() ? definition.owner : definition.lexical_owner;
}

void PA18TemplateExpander::ResolveUsingDeclarationTargets()
{
	using_declaration_targets_.clear();
	using_member_template_targets_.clear();
	for(map<string, vector<string> >::const_iterator scope =
		pending_using_declarations_.begin();
		scope != pending_using_declarations_.end(); ++scope) {
		vector<const TemplateDefinition*>& targets =
			using_declaration_targets_[scope->first];
		for(size_t index = 0; index < scope->second.size(); ++index) {
			string target_name = scope->second[index];
			while(!target_name.empty() && target_name[0] == ':') target_name.erase(0, 1);
			map<string, TemplateDefinition>::const_iterator definition =
				definitions_.find(target_name);
			if(definition != definitions_.end() &&
				(definition->second.class_template || definition->second.alias_template ||
				 definition->second.variable_template) &&
				find(targets.begin(), targets.end(), &definition->second) == targets.end())
				targets.push_back(&definition->second);
			if(class_contexts_.find(scope->first) == class_contexts_.end()) continue;
			const size_t separator = target_name.rfind("::");
			if(separator == string::npos || separator + 2 >= target_name.size()) continue;
			string target_owner = target_name.substr(0, separator);
			while(!target_owner.empty() && target_owner[0] == ':') target_owner.erase(0, 1);
			const string resolved_target_owner = CanonicalSpelling(ResolveAlias(
				target_owner, scope->first));
			if(!resolved_target_owner.empty()) target_owner = resolved_target_owner;
			const size_t target_open = target_owner.find('<');
			if(target_open != string::npos) target_owner.erase(target_open);
			const string target_member = target_name.substr(separator + 2);
			bool dependent_owner = false;
			for(map<string, TemplateDefinition>::const_iterator owner = definitions_.begin();
				owner != definitions_.end() && !dependent_owner; ++owner) {
				if(!owner->second.class_template ||
					(owner->second.qualified_name != scope->first &&
						LastComponent(owner->second.qualified_name) != LastComponent(scope->first))) continue;
				for(size_t parameter = 0; parameter < owner->second.parameters.size(); ++parameter)
					if(owner->second.parameters[parameter].type &&
						owner->second.parameters[parameter].name == target_owner) {
							dependent_owner = true;
							break;
						}
				}
				map<string, vector<string> >::const_iterator indexed = definitions_by_name_.find(
				LastComponent(target_member));
			if(indexed == definitions_by_name_.end()) continue;
			for(size_t candidate_index = 0; candidate_index < indexed->second.size();
				++candidate_index) {
				map<string, TemplateDefinition>::const_iterator candidate = definitions_.find(
					indexed->second[candidate_index]);
				if(candidate == definitions_.end() || !candidate->second.member_template ||
					candidate->second.class_template || candidate->second.alias_template ||
					candidate->second.variable_template ||
					LastComponent(candidate->second.name) != LastComponent(target_member)) continue;
				string candidate_owner = candidate->second.owner;
				const size_t candidate_open = candidate_owner.find('<');
				if(candidate_open != string::npos) candidate_owner.erase(candidate_open);
				const bool owner_matches = candidate_owner == target_owner ||
					(target_owner.find("::") == string::npos &&
						LastComponent(candidate_owner) == LastComponent(target_owner)) ||
					candidate_owner == scope->first ||
					(scope->first.find("::") == string::npos &&
						LastComponent(candidate_owner) == LastComponent(scope->first));
				if(!owner_matches && !dependent_owner) continue;
				vector<const TemplateDefinition*>& member_targets =
					using_member_template_targets_[scope->first];
				if(find(member_targets.begin(), member_targets.end(), &candidate->second) ==
					member_targets.end()) member_targets.push_back(&candidate->second);
			}
		}
	}
	pending_using_declarations_.clear();
}

bool PA18TemplateExpander::HasUsingMemberTemplate(const string& context,
	const string& member) const
{
	for(map<string, vector<const TemplateDefinition*> >::const_iterator imported =
		using_member_template_targets_.begin(); imported != using_member_template_targets_.end(); ++imported)
		for(size_t index = 0; index < imported->second.size(); ++index) {
			const TemplateDefinition* definition = imported->second[index];
			if(!definition || LastComponent(definition->name) != member) continue;
			string owner = definition->owner;
			const size_t open = owner.find('<');
			if(open != string::npos) owner.erase(open);
			if(owner == context || (context.find("::") == string::npos &&
				LastComponent(owner) == LastComponent(context))) return true;
		}
	for(string current = context; ; ) {
		map<string, vector<const TemplateDefinition*> >::const_iterator imported =
			using_member_template_targets_.find(current);
		if(imported != using_member_template_targets_.end())
			for(size_t index = 0; index < imported->second.size(); ++index)
				if(imported->second[index] && LastComponent(imported->second[index]->name) == member)
					return true;
		if(current.empty()) break;
		const size_t separator = current.rfind("::");
		if(separator == string::npos) current.clear();
		else current.erase(separator);
	}
	return false;
}

bool PA18TemplateExpander::PreserveInlineGeneratedOrder(
	const vector<CPPGMAstNodePtr>& generated_classes, const string& owner) const
{
	map<string, string>::const_iterator inline_owner = lexical_namespace_logical_.find(owner);
	if(inline_owner != lexical_namespace_logical_.end() && inline_owner->second != owner)
		return true;
	if(generated_classes.empty()) return false;
	for(size_t i = 0; i < generated_classes.size(); ++i) {
		if(!generated_classes[i] || generated_classes[i]->kind != "class-specifier") return false;
		map<string, string>::const_iterator base = specialization_bases_.find(
			LastComponent(generated_classes[i]->value));
		const TemplateDefinition* definition = base == specialization_bases_.end() ?
			0 : FindDefinition(base->second, owner);
		map<string, string>::const_iterator logical = definition ?
			lexical_namespace_logical_.find(definition->lexical_owner) :
			lexical_namespace_logical_.end();
		if(!definition || definition->lexical_owner.empty() ||
			definition->lexical_owner == definition->owner ||
			logical == lexical_namespace_logical_.end() ||
			logical->second != definition->owner) return false;
	}
	return true;
}

bool PA18TemplateExpander::HasInlineTemplateCandidate(
	const vector<const TemplateDefinition*>& definitions, const string& context) const
{
	const string context_owner = PrefixComponent(context);
	for(size_t candidate = 0; candidate < definitions.size(); ++candidate) {
		const TemplateDefinition& definition = *definitions[candidate];
		map<string, string>::const_iterator logical = lexical_namespace_logical_.find(
			definition.lexical_owner);
		if(logical != lexical_namespace_logical_.end() &&
			logical->second == definition.owner &&
			(definition.owner == context_owner ||
				(context_owner.size() > definition.owner.size() &&
				 context_owner.compare(0, definition.owner.size(), definition.owner) == 0 &&
				 context_owner[definition.owner.size()] == ':')))
			return true;
	}
	return false;
}

string NormalizeTypeArgument(string raw)
{
	raw = CanonicalSpelling(raw);
	static const char* const cv_words[] = {"const", "volatile"};
	for(size_t word_index = 0; word_index < 2; ++word_index) {
		const string word = cv_words[word_index];
		for(size_t at = raw.find(word); at != string::npos;
			at = raw.find(word, at + word.size() + 1)) {
			const bool attached_to_pointer = at > 0 &&
				(raw[at - 1] == '*' || raw[at - 1] == '&');
			const bool attached_to_identifier = at > 1 &&
				IsIdentifierCharacter(raw[at - 1]) &&
				!IsIdentifierCharacter(raw[at - 2]);
			if(attached_to_pointer || attached_to_identifier) {
				const size_t end = at + word.size();
				if(end == raw.size() || !IsIdentifierCharacter(raw[end])) {
					size_t next = end;
					while(next < raw.size() && isspace(static_cast<unsigned char>(raw[next]))) ++next;
					const bool followed_by_cv = next < raw.size() &&
						((raw.compare(next, 5, "const") == 0 &&
							(next + 5 == raw.size() || !IsIdentifierCharacter(raw[next + 5]))) ||
						 (raw.compare(next, 8, "volatile") == 0 &&
							(next + 8 == raw.size() || !IsIdentifierCharacter(raw[next + 8]))));
					if(end == raw.size() || raw[end] == '*' || raw[end] == '&' || followed_by_cv) {
						raw.insert(at, " ");
						at += 1;
					}
				}
			}
		}
	}
	raw = CanonicalSpelling(raw);
	for(size_t k = 0; k < 2; ++k) { const string keyword = k ? "volatile" : "const";
		for(size_t p = raw.find(keyword); p != string::npos; p = raw.find(keyword, p + keyword.size() + 1))
			if(p > 0 && IsIdentifierCharacter(raw[p - 1]) && (p < 2 || !IsIdentifierCharacter(raw[p - 2]))) raw.insert(p, " "); }
	raw = CanonicalSpelling(raw);
	static const char* const compact_fundamentals[][2] = {
		{"short", "short int"}, {"long", "long int"}, {"unsigned", "unsigned int"},
		{"signed", "signed int"}, {"unsignedlong", "unsigned long"},
		{"unsignedlonglong", "unsigned long long"}, {"unsignedint", "unsigned int"},
		{"unsignedlongint", "unsigned long int"}, {"unsignedlonglongint", "unsigned long long int"},
		{"unsignedshort", "unsigned short"}, {"unsignedchar", "unsigned char"},
		{"signedlong", "signed long"}, {"signedint", "signed int"},
		{"signedshort", "signed short"}, {"signedchar", "signed char"},
		{"longlong", "long long"}, {"longlongint", "long long int"}, {"longdouble", "long double"}
	};
	for(size_t i = 0; i < sizeof(compact_fundamentals) / sizeof(compact_fundamentals[0]); ++i) {
		const string compact = compact_fundamentals[i][0];
		if(raw.compare(0, compact.size(), compact) != 0) continue;
		const size_t after = compact.size();
		if(after < raw.size() && IsIdentifierCharacter(raw[after])) continue;
		// A compact fundamental prefix is only a recovery for a declarator
		// suffix (`unsignedint[4]`, for example).  Do not reinterpret a source
		// spelling that continues with another type word; parser replay can
		// legitimately concatenate those words before normalization.
		if(after < raw.size() && raw[after] != '[' && raw[after] != '*' &&
			raw[after] != '&') continue;
		raw = compact_fundamentals[i][1] + raw.substr(after);
		break;
	}
	const size_t duplicate_const = raw.find("const const ");
	if(duplicate_const != string::npos) { raw.erase(duplicate_const + 6, 6);
		const size_t pointer = raw.rfind('*');
		if(pointer != string::npos && raw.rfind("const") < pointer) raw += " const"; }
	const size_t duplicate_volatile = raw.find("volatile volatile ");
	if(duplicate_volatile != string::npos) { raw.erase(duplicate_volatile + 9, 9);
		const size_t pointer = raw.rfind('*');
		if(pointer != string::npos && raw.rfind("volatile") < pointer) raw += " volatile"; }
	if(raw.size() > 5 && raw.compare(raw.size() - 5, 5, "const") == 0 &&
		raw.find(' ') == string::npos && raw.find('_') == string::npos &&
		raw.find('(') == string::npos) {
		const size_t qualifier = raw.size() - 5;
		if(qualifier > 0 && raw[qualifier - 1] == '*') raw.insert(qualifier, " ");
		else raw = "const " + raw.substr(0, qualifier);
	} else if(raw.size() > 8 && raw.compare(raw.size() - 8, 8, "volatile") == 0 &&
		raw.find(' ') == string::npos && raw.find('_') == string::npos &&
		raw.find('(') == string::npos) {
		const size_t qualifier = raw.size() - 8;
		if(qualifier > 0 && raw[qualifier - 1] == '*') raw.insert(qualifier, " ");
		else raw = "volatile " + raw.substr(0, qualifier);
	}
	// Substituting an array type into a trailing reference declarator can leave
	// the reference on the element side (`char&[N]`).  Preserve the declarator
	// shape used by the parser (`char(&)[N]`) before this spelling becomes a
	// specialization key or a generated declaration type.
	for(size_t reference = raw.find("&["); reference != string::npos;
		reference = raw.find("&[", reference + 1)) {
		if(reference == 0 || raw[reference - 1] == '(') continue;
		raw.replace(reference, 2, "(&)[");
	}
	return CanonicalSpelling(raw);
}

string PA18ExplicitSpecializationKey(const string& qualified_name,
	const vector<string>& arguments)
{
	string result = qualified_name;
	for(size_t i = 0; i < arguments.size(); ++i) {
		string argument = NormalizeTypeArgument(arguments[i]);
		if(argument == "unsigned int") argument = "unsigned";
		else if(argument == "short int") argument = "short";
		else if(argument == "unsigned short int") argument = "unsigned short";
		else if(argument == "long int") argument = "long";
		else if(argument == "unsigned long int") argument = "unsigned long";
		else if(argument == "long long int") argument = "long long";
		else if(argument == "unsigned long long int") argument = "unsigned long long";
		result += "|" + argument;
	}
	return result;
}

vector<string> SplitTemplateArguments(const string& raw)
{
	vector<string> result;
	string current;
	int angle = 0;
	vector<int> angle_parentheses;
	vector<bool> synthetic_angles;
	int parentheses = 0, brackets = 0;
	for(size_t i = 0; i < raw.size(); ++i) {
		const char ch = raw[i];
		if(ch == '(') ++parentheses;
		else if(ch == ')' && parentheses > 0) --parentheses;
		if(ch == '[') ++brackets;
		else if(ch == ']' && brackets > 0) --brackets;
		bool nested_template_open = ch == '<' && IsTemplateAngleOpen(raw, i);
		bool synthetic_nested_template = false;
		if(nested_template_open) {
			// TemplateRange can hand us an argument list whose final `>` was
			// consumed as the enclosing delimiter.  In a dependent comparison
			// such as `Count < 2, int`, the compact spelling therefore contains an
			// apparent nested `<` but no matching close.  Do not let that operator
			// hide the comma separating the enclosing template arguments.
			int candidate_depth = 1;
			bool has_close = false;
			for(size_t scan = i + 1; scan < raw.size(); ++scan) {
				if(raw[scan] == '<' && IsTemplateAngleOpen(raw, scan)) ++candidate_depth;
				else if(raw[scan] == '>' && candidate_depth > 0 &&
					IsTemplateAngleClose(raw, scan) && --candidate_depth == 0) {
					has_close = true;
					break;
				}
			}
			nested_template_open = has_close;
			// A qualified anonymous-namespace type contains the literal
			// `<unnamed>` marker, which is deliberately not an angle delimiter.
			// If a surrounding replay has already consumed the nested template's
			// closing `>`, retain its commas as nested until the next outer comma
			// and restore that missing delimiter below.
			if(!nested_template_open && raw.find("<unnamed>", i + 1) != string::npos) {
				nested_template_open = true;
				synthetic_nested_template = true;
			}
		}
		if(nested_template_open) {
			++angle; angle_parentheses.push_back(parentheses);
			synthetic_angles.push_back(synthetic_nested_template);
		} else if(ch == '>' && angle > 0 && IsTemplateAngleClose(raw, i)) {
			const int opener_parentheses = angle_parentheses.empty() ? 0 : angle_parentheses.back();
			if(parentheses > opener_parentheses) continue;
			--angle;
			if(!angle_parentheses.empty()) angle_parentheses.pop_back();
			if(!synthetic_angles.empty()) synthetic_angles.pop_back();
		}
		if(ch == ',' && angle == 1 && parentheses == 0 && brackets == 0 &&
			!synthetic_angles.empty() && synthetic_angles.back()) {
			current += '>';
			--angle;
			angle_parentheses.pop_back();
			synthetic_angles.pop_back();
			result.push_back(CanonicalSpelling(current));
			current.clear();
			continue;
		}
		if(ch == ',' && angle == 0 && parentheses == 0 && brackets == 0) {
			result.push_back(CanonicalSpelling(current)); current.clear();
		} else current += ch;
	}
	if(!current.empty() || !result.empty()) result.push_back(CanonicalSpelling(current));
	if(result.size() == 1 && result[0].empty()) result.clear();
	return result;
}

string PreservePackSubstitution(const string& word, const string& replacement,
	bool pack_operand)
{
	// The source operand already owns its ellipsis; avoid turning `I` -> `I1...`
	// into the malformed `I1......` during dependent alias replay.
	if(pack_operand && replacement.size() > 3 &&
		replacement.compare(replacement.size() - 3, 3, "...") == 0)
		return replacement.substr(0, replacement.size() - 3);
	return word;
}

string ReplaceIdentifiersPreservingPackSizes(const string& raw,
	const map<string, string>& substitutions)
{
	// ReplaceIdentifiers must not turn a pack operand into a scalar followed by
	// an ellipsis (`_Tail...` -> `double...`).  The ellipsis belongs to the
	// complete operand and is expanded later from typed pack state.  Keep the
	// ordinary identifier replacement rules, including compact cv spellings,
	// for every non-pack occurrence.
	const auto replace_segment = [&](const string& segment) {
		// The ellipsis applies to the complete preceding expression, not only to
		// an identifier immediately before it.  Preserve identifiers inside an
		// expression such as `((void)Pack, true)...` so typed pack replay can
		// expand each element before scalar substitution runs.
		vector<pair<size_t, size_t> > pack_spans;
		for(size_t ellipsis = segment.find("..."); ellipsis != string::npos;
			ellipsis = segment.find("...", ellipsis + 3)) {
			if(ellipsis >= 6 && segment.substr(ellipsis - 6, 6) == "sizeof") continue;
			int angle = 0, parentheses = 0, brackets = 0, braces = 0;
			size_t begin = ellipsis;
			while(begin > 0) {
				const char ch = segment[begin - 1];
				if(ch == '>') ++angle;
				else if(ch == '<' && angle > 0) --angle;
				else if(ch == ')') ++parentheses;
				else if(ch == '(') {
					if(parentheses > 0) --parentheses;
					else if(angle == 0 && brackets == 0 && braces == 0) break;
				}
				else if(ch == ']') ++brackets;
				else if(ch == '[' && brackets > 0) --brackets;
				else if(ch == '}') ++braces;
				else if(ch == '{' && braces > 0) --braces;
				if(ch == ',' && angle == 0 && parentheses == 0 &&
					brackets == 0 && braces == 0) break;
				--begin;
			}
			if(begin < segment.size() && (segment[begin] == ',' || segment[begin] == '(')) ++begin;
			if(begin < ellipsis) pack_spans.push_back(make_pair(begin, ellipsis));
		}
		string replaced;
		for(size_t i = 0; i < segment.size();) {
			if(!IsIdentifierCharacter(segment[i])) {
				replaced += segment[i++];
				continue;
			}
			size_t end = i + 1;
			while(end < segment.size() && IsIdentifierCharacter(segment[end])) ++end;
			const string word = segment.substr(i, end - i);
			size_t after = end;
			while(after < segment.size() && isspace(static_cast<unsigned char>(segment[after]))) ++after;
			bool pack_operand = after + 3 <= segment.size() &&
				segment.compare(after, 3, "...") == 0;
			if(!pack_operand) for(size_t span = 0; span < pack_spans.size(); ++span)
				if(i >= pack_spans[span].first && i < pack_spans[span].second) {
					pack_operand = true;
					break;
				}
			map<string, string>::const_iterator found = substitutions.find(word);
			const bool already_qualified = i >= 2 && replaced.size() >= 2 && replaced.compare(replaced.size() - 2, 2, "::") == 0;
			const bool dependent_template_member = IsTemplateQualifiedIdentifier(segment, i);
			if(found != substitutions.end() && !pack_operand && !already_qualified && !dependent_template_member)
				replaced += found->second;
			else if(found != substitutions.end())
				replaced += PreservePackSubstitution(word, found->second, pack_operand);
			else {
				bool compact_substitution = false;
				if(!pack_operand) for(map<string,string>::const_iterator it = substitutions.begin();
					it != substitutions.end(); ++it) {
					if(it->first.empty() || word.size() <= it->first.size()) continue;
					if(word.compare(0, it->first.size(), it->first) == 0) {
						const string suffix = word.substr(it->first.size());
						if(suffix == "const" || suffix == "volatile") {
							replaced += it->second + " " + suffix;
							compact_substitution = true;
							break;
						}
					}
					if(word.compare(word.size() - it->first.size(), it->first.size(), it->first) == 0) {
						const string prefix = word.substr(0, word.size() - it->first.size());
						if(prefix == "const" || prefix == "volatile") {
							replaced += prefix + " " + it->second;
							compact_substitution = true;
							break;
						}
					}
				}
				if(!compact_substitution) replaced += word;
			}
			i = end;
		}
		return replaced;
	};
	string result;
	size_t cursor = 0;
	for(size_t search = raw.find("sizeof..."); search != string::npos; ) {
		const size_t open = search + 9;
		if(open >= raw.size() || raw[open] != '(') {
			search = raw.find("sizeof...", search + 9);
			continue;
		}
		int depth = 0;
		size_t close = string::npos;
		for(size_t position = open; position < raw.size(); ++position) {
			if(raw[position] == '(') ++depth;
			else if(raw[position] == ')' && --depth == 0) {
				close = position;
				break;
			}
		}
		if(close == string::npos) break;
		result += replace_segment(raw.substr(cursor, search - cursor));
		result += raw.substr(search, close - search + 1);
		cursor = close + 1;
		search = raw.find("sizeof...", cursor);
	}
	result += replace_segment(raw.substr(cursor));
	return result;
}

void PA18TemplateExpander::IndexConstantMembers(const CPPGMAstNodePtr& node,
	const string& owner)
{
	if(!node || owner.empty() ||
		(node->kind != "class-specifier" && node->kind != "class-forward-declaration")) return;
	for(size_t child = 0; child < node->children.size(); ++child) {
		const CPPGMAstNodePtr declaration = node->children[child];
		if(!declaration || declaration->kind != "simple-declaration" ||
			declaration->children.empty()) continue;
		if(!HasDeclarationSpecifier(declaration->children[0], "const") &&
			!HasDeclarationSpecifier(declaration->children[0], "constexpr")) continue;
		const CPPGMAstNodePtr list = ChildOfKindLocal(declaration,
			"init-declarator-list");
		if(!list) continue;
		for(size_t item = 0; item < list->children.size(); ++item) {
			const CPPGMAstNodePtr declarator = list->children[item];
			if(!declarator || declarator->children.empty()) continue;
			const string name = LastComponent(FirstIdentifierLocal(
				declarator->children[0]));
			if(name.empty()) continue;
			vector<string>& owners = constant_member_owners_[name];
			if(find(owners.begin(), owners.end(), owner) == owners.end())
				owners.push_back(owner);
		}
	}
}

bool HasDeclarationSpecifier(const CPPGMAstNodePtr& node, const string& wanted)
{
	if(!node) return false;
	if((node->kind == "decl-specifier" || node->kind == "specifier" ||
		node->kind == "cv-qualifier") &&
		RemoveMarker(node->value) == wanted) return true;
	for(size_t child = 0; child < node->children.size(); ++child)
		if(HasDeclarationSpecifier(node->children[child], wanted)) return true;
	return false;
}

void PA18TemplateExpander::IndexDependentMemberTypeNodes(
	const CPPGMAstNodePtr& node, vector<CPPGMAstNodePtr>& nodes,
	vector<CPPGMAstNodePtr>& type_nodes) const
{
	if(!node || node->kind == "function-definition" ||
		node->kind == "special-member-definition" ||
		node->kind == "special-member-declaration" ||
		node->kind == "compound-statement") return;
	if(node->kind == "decl-specifier" || node->kind == "type-name" ||
		node->kind == "type-specifier" || node->kind == "decltype-specifier" ||
		node->kind == "base-name") {
		nodes.push_back(node);
		if(LastComponent(CanonicalSpelling(RemoveMarker(node->value))) == "type")
			type_nodes.push_back(node);
	}
	for(size_t child = 0; child < node->children.size(); ++child)
		IndexDependentMemberTypeNodes(node->children[child], nodes, type_nodes);
}

void PA18TemplateExpander::SetActiveConcreteOwner(const string& owner,
	const string& context)
{
	active_concrete_owner_ = ConcreteOwnerContext();
	if(owner.empty()) return;
	active_concrete_owner_.name = owner;
	map<string, string>::const_iterator base = specialization_bases_.find(
		LastComponent(owner));
	map<string, vector<string> >::const_iterator arguments =
		specialization_arguments_.find(LastComponent(owner));
	if(base != specialization_bases_.end())
		active_concrete_owner_.definition = FindDefinition(base->second, context);
	if(arguments != specialization_arguments_.end())
		active_concrete_owner_.arguments = arguments->second;
}

ClassSpecializationIdentity PA18TemplateExpander::MakeClassSpecializationIdentity(
	const TemplateDefinition& definition, const vector<string>& arguments,
	const string& context) const
{
	const TemplateDefinition* primary = FindDefinition(definition.qualified_name,
		context);
	if(!primary || !primary->class_template) primary = &definition;
	vector<string> canonical_arguments;
	canonical_arguments.reserve(arguments.size());
	for(size_t argument = 0; argument < arguments.size(); ++argument)
		canonical_arguments.push_back(NormalizeTypeArgument(
			CanonicalSpelling(arguments[argument])));
	return ClassSpecializationIdentity(primary, canonical_arguments);
}

void PA18TemplateExpander::IndexStaticMembers(const CPPGMAstNodePtr& node,
	set<string>& members) const
{
	if(!node || (node->kind != "class-specifier" &&
		node->kind != "class-forward-declaration")) return;
	for(size_t child = 0; child < node->children.size(); ++child) {
		const CPPGMAstNodePtr declaration = node->children[child];
		if(!declaration) continue;
		if(declaration->kind == "template-declaration" && declaration->children.size() > 1) {
			CPPGMAstNodePtr templated = declaration->children[1];
			while(templated && templated->kind == "template-declaration" &&
				templated->children.size() > 1)
				templated = templated->children[1];
			if(templated && !templated->children.empty() &&
				HasDeclarationSpecifier(templated->children[0], "static")) {
				const string name = LastComponent(DeclarationName(templated));
				if(!name.empty()) members.insert(name);
			}
			continue;
		}
		if(declaration->kind == "function-definition" && !declaration->children.empty() &&
			HasDeclarationSpecifier(declaration->children[0], "static")) {
			const string name = LastComponent(DeclarationName(declaration));
			if(!name.empty()) members.insert(name);
			continue;
		}
		if(declaration->kind != "simple-declaration" ||
			declaration->children.empty() ||
			(!HasDeclarationSpecifier(declaration->children[0], "static") &&
			 !HasDeclarationSpecifier(declaration->children[0], "constexpr"))) continue;
		const CPPGMAstNodePtr list = ChildOfKindLocal(declaration,
			"init-declarator-list");
		if(!list) continue;
		for(size_t item = 0; item < list->children.size(); ++item) {
			const CPPGMAstNodePtr declarator = list->children[item];
			if(!declarator || declarator->children.empty()) continue;
			const string name = LastComponent(FirstIdentifierLocal(
				declarator->children[0]));
			if(!name.empty()) members.insert(name);
		}
	}
}

vector<CPPGMAstNodePtr> PA18TemplateExpander::Run(
	const vector<CPPGMAstNodePtr>& input)
{
	active_static_member_ = false;
	generated_by_primary_.clear();
	source_order_.clear();
	size_t source_order = 0;
	function<void(const CPPGMAstNodePtr&)> index_source_order =
		[&](const CPPGMAstNodePtr& node) { if(!node) return; source_order_[node.get()] = source_order++;
			for(size_t child = 0; child < node->children.size(); ++child) index_source_order(node->children[child]); };
	for(size_t i = 0; i < input.size(); ++i) index_source_order(input[i]);
	active_source_order_ = static_cast<size_t>(-1);
	ValidateTemplateDiagnostics(input);
	for(size_t i = 0; i < input.size(); ++i)
		CollectLexical(input[i], string(), string());
	for(size_t i = 0; i < input.size(); ++i) Collect(input[i], string());
	ResolveUsingDeclarationTargets();
	IndexOrdinaryConversionDefinitions();
	for(size_t i = 0; i < input.size(); ++i)
		ValidateTemplateArgumentKinds(input[i], string(), map<string, bool>());
	vector<CPPGMAstNodePtr> result;
	for(size_t i = 0; i < input.size(); ++i) {
		CPPGMAstNodePtr tree = TransformTranslationUnit(input[i]);
		if(tree) result.push_back(tree);
	}
	return result;
}

string PA18TemplateExpander::StripTemplateArgumentsForValidation(
	const string& raw) const
{
	string result;
	int depth = 0;
	for(size_t i = 0; i < raw.size(); ++i) {
		if(raw[i] == '<') { ++depth; continue; }
		if(raw[i] == '>') { if(depth > 0) --depth; continue; }
		if(depth == 0) result += raw[i];
	}
	return result;
}

bool PA18TemplateExpander::ValidationHasNoexcept(
	const CPPGMAstNodePtr& node) const
{
	if(!node) return false;
	if(node->kind == "function-qualifier" && node->value == "noexcept") return true;
	for(size_t i = 0; i < node->children.size(); ++i)
		if(ValidationHasNoexcept(node->children[i])) return true;
	return false;
}

void PA18TemplateExpander::CollectValidationNames(
	const CPPGMAstNodePtr& node, set<string>& names) const
{
	if(!node) return;
	if(node->kind == "identifier" && !node->value.empty())
		names.insert(LastComponent(RemoveMarker(node->value)));
	if(node->kind == "class-specifier" ||
		node->kind == "class-forward-declaration" ||
		node->kind == "function-definition" ||
		node->kind == "special-member-definition" ||
		node->kind == "special-member-declaration" ||
		node->kind == "alias-declaration") {
		const string name = DeclarationName(node);
		if(!name.empty()) names.insert(name);
	}
	if(node->kind == "simple-declaration") {
		const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
		if(list) for(size_t i = 0; i < list->children.size(); ++i)
			if(list->children[i] && !list->children[i]->children.empty()) {
				const string name = FirstIdentifierLocal(list->children[i]->children[0]);
				if(!name.empty()) names.insert(LastComponent(name));
			}
	}
	if(node->kind == "parameter-declaration" && node->children.size() > 1) {
		const string name = FirstIdentifierLocal(node->children[1]);
		if(!name.empty()) names.insert(LastComponent(name));
	}
	if(node->kind == "enumerator" && !node->value.empty())
		names.insert(LastComponent(node->value));
	for(size_t i = 0; i < node->children.size(); ++i)
		CollectValidationNames(node->children[i], names);
}

bool PA18TemplateExpander::ValidationDependentName(const string& raw,
	const set<string>& parameters) const
{
	if(raw.find("::") != string::npos || raw.find('<') != string::npos) {
		for(set<string>::const_iterator parameter = parameters.begin();
			parameter != parameters.end(); ++parameter) {
			if(parameter->empty()) continue;
			for(size_t position = raw.find(*parameter);
				position != string::npos;
				position = raw.find(*parameter, position + parameter->size())) {
				const bool left = position == 0 ||
					!IsIdentifierCharacter(raw[position - 1]);
				const size_t end = position + parameter->size();
				const bool right = end == raw.size() ||
					!IsIdentifierCharacter(raw[end]);
				if(left && right) return true;
			}
		}
	}
	return parameters.find(raw) != parameters.end();
}

bool ValidationBuiltinTypeName(const string& raw)
{
	const string value = RemoveMarker(raw);
	return value == "bool" || value == "char" || value == "char16_t" ||
		value == "char32_t" || value == "double" || value == "float" ||
		value == "int" || value == "long" || value == "short" ||
		value == "signed" || value == "unsigned" || value == "void" ||
		value == "wchar_t";
}

string ValidationTopLevelPrefix(const string& raw)
{
	int angle_depth = 0;
	size_t separator = string::npos;
	for(size_t position = 0; position + 1 < raw.size(); ++position) {
		if(raw[position] == '<' && (position + 1 >= raw.size() || raw[position + 1] != '='))
			++angle_depth;
		else if(raw[position] == '>' && angle_depth > 0 &&
			(position + 1 >= raw.size() || raw[position + 1] != '=')) --angle_depth;
		if(angle_depth == 0 && raw.compare(position, 2, "::") == 0)
			separator = position;
	}
	return separator == string::npos ? string() : raw.substr(0, separator);
}

void PA18TemplateExpander::ValidateTemplateNode(const CPPGMAstNodePtr& node,
	const set<string>& parameters, const set<string>& known_names,
	const string& current_class, bool in_function,
	map<string, bool>& special_members,
	const CPPGMAstNodePtr& parent, size_t child_index) const
{
	if(!node) return;
	if(node->kind == "alias-declaration" &&
		parameters.find(LastComponent(node->value)) != parameters.end())
		throw logic_error("alias shadows a template parameter: " + node->value);
	if(node->kind == "decl-specifier") {
		const string raw_type = CanonicalSpelling(RemoveMarker(node->value));
		const bool has_typename = node->explicit_typename ||
			raw_type.compare(0, 8, "typename") == 0 &&
			(raw_type.size() == 8 || isspace(static_cast<unsigned char>(raw_type[8])));
		bool sibling_typename = false;
		if(parent) for(size_t sibling = 0; sibling < child_index &&
			sibling < parent->children.size(); ++sibling)
			if(parent->children[sibling] &&
				RemoveMarker(parent->children[sibling]->value) == "typename") {
				sibling_typename = true;
				break;
			}
		// A qualified value used inside a template argument, such as
		// `enable_if_t<Pred::value, int>`, is not a dependent qualified type-id
		// and does not require `typename`.  Find the separator only outside the
		// template argument angle brackets.
		const string dependent_qualifier = ValidationTopLevelPrefix(raw_type);
		const string dependent_base = LastComponent(StripTemplateArgumentsForValidation(
			dependent_qualifier));
		const string current_base = LastComponent(StripTemplateArgumentsForValidation(
			current_class));
		const bool current_specialization = !current_class.empty() &&
			dependent_base == current_base;
		const bool decltype_specifier = raw_type.compare(0, 9, "decltype(") == 0;
		if(!decltype_specifier && !has_typename && !sibling_typename && !current_specialization &&
			!dependent_qualifier.empty() &&
			ValidationDependentName(dependent_qualifier, parameters)) {
			throw logic_error("dependent qualified type requires typename");
		}
	}
	string member_key;
	if(node->kind == "special-member-declaration" ||
		node->kind == "special-member-definition") {
		string owner = current_class;
		if(owner.empty() && node->value.find("::") != string::npos)
			owner = LastComponent(StripTemplateArgumentsForValidation(
				PrefixComponent(node->value)));
		member_key = owner.empty() ? LastComponent(node->value) :
			owner + "::" + LastComponent(node->value);
		const bool noexcept_specified = ValidationHasNoexcept(node);
		map<string, bool>::const_iterator prior = special_members.find(member_key);
		if(prior != special_members.end() && prior->second != noexcept_specified)
			throw logic_error("special-member exception specification mismatch");
		special_members[member_key] = noexcept_specified;
	}
	if(node->kind == "id-expression" && in_function) {
		const bool member_name = parent && parent->kind == "member-expression" &&
			child_index == 1;
		const bool builtin_value_initialization = parent &&
			parent->kind == "call-expression" && child_index == 0 &&
			ValidationBuiltinTypeName(node->value);
		if(!member_name && node->value.find("::") == string::npos &&
			node->value.find('<') == string::npos &&
			!ValidationDependentName(node->value, parameters) &&
			!builtin_value_initialization &&
			known_names.find(node->value) == known_names.end() &&
			node->value.compare(0, 8, "operator") != 0 &&
			node->value.compare(0, 10, "__builtin_") != 0)
			throw logic_error("unknown nondependent template name: " + node->value);
	}
	const bool class_node = node->kind == "class-specifier" ||
		node->kind == "class-forward-declaration";
	string next_class = class_node ? LastComponent(node->value) : current_class;
	if(!class_node && (node->kind == "special-member-definition" ||
		node->kind == "special-member-declaration") &&
		node->value.find("::") != string::npos)
		next_class = LastComponent(StripTemplateArgumentsForValidation(
		PrefixComponent(node->value)));
	if(!class_node && node->kind == "function-definition" &&
		next_class.empty() && node->children.size() > 1) {
		const string function_spelling = FirstIdentifierLocal(node->children[1]);
		if(function_spelling.find("::") != string::npos)
			next_class = LastComponent(StripTemplateArgumentsForValidation(
				PrefixComponent(function_spelling)));
	}
	const bool function_node = node->kind == "function-definition" ||
		node->kind == "special-member-definition";
	for(size_t i = 0; i < node->children.size(); ++i)
		ValidateTemplateNode(node->children[i], parameters, known_names,
			next_class, in_function || function_node, special_members, node, i);
}

void PA18TemplateExpander::ValidateDependentMemberTemplateNode(
	const CPPGMAstNodePtr& node, const set<string>& parameters,
	const map<string, string>& variables) const
{
	if(!node) return;
	set<string> local_parameters = parameters;
	map<string, string> local_variables = variables;
	if(node->kind == "template-declaration" && node->children.size() > 1) {
		const vector<TemplateParameter> own = Parameters(node->children[0]);
		for(size_t i = 0; i < own.size(); ++i)
			if(!own[i].name.empty()) local_parameters.insert(own[i].name);
		ValidateDependentMemberTemplateNode(node->children[1], local_parameters,
			local_variables);
		return;
	}
	if(node->kind == "function-definition" && node->children.size() > 1) {
		const CPPGMAstNodePtr clause = DescendantOfKind(node->children[1],
			"parameter-clause");
		if(clause) for(size_t i = 0; i < clause->children.size(); ++i) {
			const CPPGMAstNodePtr parameter = clause->children[i];
			if(!parameter || parameter->kind != "parameter-declaration" ||
				parameter->children.size() < 2) continue;
			const string name = FirstIdentifierLocal(parameter->children[1]);
			if(!name.empty()) local_variables[name] = ParameterTypeSpelling(parameter);
		}
	}
	if(node->kind == "member-expression" && node->children.size() >= 2 &&
		node->children[0] && node->children[0]->kind == "id-expression" &&
		node->children[1] && node->children[1]->kind == "identifier" &&
		node->children[1]->value.find('<') != string::npos) {
		map<string, string>::const_iterator variable = local_variables.find(
			LastComponent(node->children[0]->value));
		if(variable != local_variables.end() &&
			ValidationDependentName(variable->second, local_parameters)) {
			const string raw_member = RemoveMarker(node->children[1]->value);
			const string member = CanonicalSpelling(raw_member);
			const bool disambiguated = raw_member.compare(0, 8, "template") == 0 &&
				(raw_member.size() == 8 || isspace(static_cast<unsigned char>(raw_member[8])));
			if(!disambiguated)
				throw logic_error("dependent member template requires template keyword");
		}
	}
	for(size_t i = 0; i < node->children.size(); ++i)
		ValidateDependentMemberTemplateNode(node->children[i], local_parameters,
			local_variables);
}

void PA18TemplateExpander::ValidateTemplateDiagnostics(
	const vector<CPPGMAstNodePtr>& input) const
{
	set<string> known_names;
	for(size_t i = 0; i < input.size(); ++i) CollectValidationNames(input[i], known_names);
	map<string, bool> special_members;
	for(size_t i = 0; i < input.size(); ++i)
		ValidateTemplateDiagnosticsNode(input[i], known_names, special_members);
}

void PA18TemplateExpander::ValidateTemplateDiagnosticsNode(
	const CPPGMAstNodePtr& node, const set<string>& known_names,
	map<string, bool>& special_members) const
{
	if(!node) return;
	if(node->kind == "template-declaration" && node->children.size() > 1) {
		set<string> parameters;
		const vector<TemplateParameter> values = Parameters(node->children[0]);
		for(size_t i = 0; i < values.size(); ++i) parameters.insert(values[i].name);
		ValidateTemplateNode(node->children[1], parameters, known_names,
			string(), false, special_members);
		ValidateDependentMemberTemplateNode(node->children[1], parameters,
			map<string, string>());
		return;
	}
	for(size_t i = 0; i < node->children.size(); ++i)
		ValidateTemplateDiagnosticsNode(node->children[i], known_names, special_members);
}

bool PA18TemplateExpander::FindLogicalNamespaceAlias(const string& spelling,
	string* alias_key) const
{
	const size_t separator = TopLevelScopeSeparator(spelling);
	if(!alias_key || separator == string::npos) return false;
	const string logical_owner = spelling.substr(0, separator);
	const string logical_name = spelling.substr(separator + 2);
	map<string, string>::const_iterator match = type_aliases_.end();
	for(map<string, string>::const_iterator it = type_aliases_.begin();
		it != type_aliases_.end(); ++it) {
		if(LastComponent(it->first) != logical_name) continue;
		const string physical_owner = PrefixComponent(it->first);
		map<string, string>::const_iterator logical =
			lexical_namespace_logical_.find(physical_owner);
		if(logical == lexical_namespace_logical_.end() ||
			logical->second != logical_owner) continue;
		if(match != type_aliases_.end()) return false;
		match = it;
	}
	if(match == type_aliases_.end()) return false;
	*alias_key = match->first;
	return true;
}

bool PA18TemplateExpander::IsArrayTypeAlias(const string& alias_name,
	const string& context) const
{
	for(string current = context; ; ) {
		map<string, string>::const_iterator alias = type_aliases_.find(
			JoinPath(current, alias_name));
		if(alias != type_aliases_.end() && alias->second.find('[') != string::npos)
			return true;
		if(current.empty()) break;
		const size_t separator = current.rfind("::");
		if(separator == string::npos) current.clear();
		else current.erase(separator);
	}
	return false;
}

bool PA18TemplateExpander::HasPackBeforeFixed(const TemplateDefinition& definition) const
{
	for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter)
		if(definition.parameters[parameter].pack)
			for(size_t later = parameter + 1; later < definition.parameters.size(); ++later)
				if(!definition.parameters[later].pack) return true;
	return false;
}

bool PA18TemplateExpander::ResolveGeneratedMemberAlias(const string& class_key,
	const string& member, const string& context, string* member_type) const
{
	map<string, string>::const_iterator generated_base =
		specialization_bases_.find(LastComponent(class_key));
	map<string, vector<string> >::const_iterator generated_arguments =
		specialization_arguments_.find(LastComponent(class_key));
	if(generated_base == specialization_bases_.end() ||
		generated_arguments == specialization_arguments_.end()) return false;
	string source_class = generated_base->second;
	if(source_class.find('<') == string::npos) {
		source_class += "<";
		for(size_t argument = 0; argument < generated_arguments->second.size(); ++argument)
			source_class += (argument ? "," : "") + generated_arguments->second[argument];
		source_class += ">";
	}
	map<string, string> source_substitutions;
	set<string> source_active;
	bool found = FindClassMemberType(source_class, member, source_substitutions,
		context, member_type, &source_active, true);
	if(!found && member_type->empty() && !PrefixComponent(class_key).empty()) {
		const string scoped_source = JoinPath(PrefixComponent(class_key), source_class);
		source_substitutions.clear();
		source_active.clear();
		found = FindClassMemberType(scoped_source, member, source_substitutions,
			context, member_type, &source_active, true);
	}
	return found;
}

bool PA18TemplateExpander::ResolveContextMemberAlias(const string& class_key,
	const string& member, const string& context, string* member_type) const
{
	for(string current = context; member_type && member_type->empty() && !current.empty(); ) {
		*member_type = MemberAliasType(JoinPath(current, class_key), member);
		const size_t parent = current.rfind("::");
		if(parent == string::npos) current.clear();
		else current.erase(parent);
	}
	return member_type && !member_type->empty();
}

string PA18TemplateExpander::ResolveAlias(string spelling, const string& context) const
{
	const bool reference_alias_specialization =
		reference_alias_specializations_.find(LastComponent(spelling)) !=
		reference_alias_specializations_.end();
	spelling = CanonicalSpelling(spelling);
	string cv_prefix;
	bool resolved_reference_alias = false;
	if(spelling.compare(0, 6, "const ") == 0) {
		cv_prefix = "const ";
		spelling = CanonicalSpelling(spelling.substr(6));
	} else if(spelling.compare(0, 9, "volatile ") == 0) {
		cv_prefix = "volatile ";
		spelling = CanonicalSpelling(spelling.substr(9));
	}
	string suffix;
	while(!spelling.empty() && (spelling[spelling.size() - 1] == '*' ||
		spelling[spelling.size() - 1] == '&')) {
		suffix = spelling[spelling.size() - 1] + suffix;
		spelling.erase(spelling.size() - 1);
		spelling = CanonicalSpelling(spelling);
	}
	string array_suffix;
	while(!spelling.empty() && spelling[spelling.size() - 1] == ']') {
		const size_t open = spelling.rfind('[');
		if(open == string::npos) break;
		array_suffix = spelling.substr(open) + array_suffix;
		spelling.erase(open);
		spelling = CanonicalSpelling(spelling);
	}
	set<string> seen;
	for(size_t depth = 0; depth < 16; ++depth) {
		if(!seen.insert(spelling).second) break;
		map<string, string>::const_iterator direct = type_aliases_.find(spelling);
		const bool known_class_name = spelling.find("::") == string::npos &&
			FindClassDeclaration(spelling, context);
		if(direct == type_aliases_.end()) {
			for(string current = context; direct == type_aliases_.end(); ) {
				const string candidate = JoinPath(current, spelling);
				direct = type_aliases_.find(candidate);
				const size_t separator = current.rfind("::");
				if(separator == string::npos) break;
				current.erase(separator);
			}
		}
		if(direct == type_aliases_.end() && !known_class_name) {
			string alias_path;
			if(FindUnqualifiedGeneratedAliasPath(spelling, context, &alias_path))
				direct = type_aliases_.find(alias_path);
		}
		if(direct == type_aliases_.end()) {
			string logical_alias;
			if(FindLogicalNamespaceAlias(spelling, &logical_alias))
				direct = type_aliases_.find(logical_alias);
		}
		if(direct != type_aliases_.end()) {
			resolved_reference_alias = true;
			string target = QualifyAliasTarget(direct->second, direct->first);
			const size_t owner_separator = spelling.rfind("::");
			if(owner_separator != string::npos) {
				const string owner = spelling.substr(0, owner_separator);
				const size_t target_open = target.find('<');
				if(target_open != string::npos && target.find("::") == string::npos) {
					const string target_base = target.substr(0, target_open);
					const TemplateDefinition* target_definition = FindDefinition(target_base, owner);
					if(target_definition)
						target = target_definition->qualified_name + target.substr(target_open);
				}
				map<string, CPPGMAstNodePtr>::const_iterator declaration =
					class_declarations_.find(owner);
				if(declaration != class_declarations_.end())
					target = QualifyNestedMembers(target, owner, declaration->second);
			}
				spelling = CanonicalSpelling(target); continue;
		}
		// Split qualified members at a scope separator outside nested templates.
		const size_t separator = TopLevelScopeSeparator(spelling);
		if(separator == string::npos) break;
		string class_key = spelling.substr(0, separator);
		const string member = spelling.substr(separator + 2);
		string member_type = MemberAliasType(class_key, member);
		if(member_type.empty()) {
			map<string, string> member_substitutions;
			set<string> member_active;
			FindClassMemberType(class_key, member, member_substitutions, context,
				&member_type, &member_active, true);
		}
		if(member_type.empty()) ResolveGeneratedMemberAlias(class_key, member, context, &member_type);
		if(member_type.empty()) ResolveContextMemberAlias(class_key, member, context, &member_type);
		if(member_type.empty()) {
			const size_t owner_separator = class_key.rfind("::");
			if(owner_separator != string::npos) {
				const string owner_key = class_key.substr(0, owner_separator);
				const string owner_member = class_key.substr(owner_separator + 2);
				const string owner_type = MemberAliasType(owner_key, owner_member);
				if(!owner_type.empty()) {
					spelling = owner_type + "::" + member;
					continue;
				}
			}
		}
		if(member_type.empty()) break;
		if(member_type.find("::") == string::npos) {
			const string resolved_member = ResolveAlias(member_type, context);
			if(!resolved_member.empty()) member_type = resolved_member;
		}
		spelling = CanonicalSpelling(member_type);
	}
	if(resolved_reference_alias && suffix.empty() &&
		!spelling.empty() && spelling[spelling.size() - 1] == '&') cv_prefix.clear();
	if(reference_alias_specialization) {
		while(spelling.compare(0, 6, "const ") == 0)
			spelling = CanonicalSpelling(spelling.substr(6));
		while(spelling.compare(0, 9, "volatile ") == 0)
			spelling = CanonicalSpelling(spelling.substr(9));
	}
	return CanonicalSpelling(cv_prefix + spelling + suffix + array_suffix);
}

bool PA18TemplateExpander::ContainsName(const CPPGMAstNodePtr& node,
	const string& name) const
{
	if(!node || name.empty()) return false;
	const string value = RemoveMarker(node->value);
	if(value == name || LastComponent(value) == name ||
		value.find(name + "::") != string::npos) return true;
	for(size_t i = 0; i < node->children.size(); ++i)
		if(ContainsName(node->children[i], name)) return true;
	return false;
}

CPPGMAstNodePtr PA18TemplateExpander::FunctionParameter(
	const CPPGMAstNodePtr& original, const FunctionSignature& signature) const
{
	if(!original || original->children.empty() || !signature.result_specifiers ||
		!signature.parameters) return CPPGMAstNodePtr();
	string parameter_name;
	bool reference = false;
	bool rvalue_reference = false;
	if(original->children.size() > 1 && original->children[1]) {
		parameter_name = FirstIdentifierLocal(original->children[1]);
		for(size_t i = 0; i < original->children[1]->children.size(); ++i) {
			const CPPGMAstNodePtr child = original->children[1]->children[i];
			if(!child || child->kind != "ptr-operator") continue;
			if(child->value.find("&") != string::npos) {
				reference = true;
				rvalue_reference = child->value.find("&&") != string::npos;
			}
		}
	}
	if(rvalue_reference && signature.lvalue_argument) rvalue_reference = false;
	CPPGMAstNodePtr result(new CPPGMAstNode("parameter-declaration"));
	result->children.push_back(CloneNode(signature.result_specifiers));
	CPPGMAstNodePtr declarator(new CPPGMAstNode("declarator"));
	CPPGMAstNodePtr nested(new CPPGMAstNode("nested-declarator"));
	CPPGMAstNodePtr inner(new CPPGMAstNode("declarator"));
	inner->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("ptr-operator",
		reference ? (rvalue_reference ? "OP_LAND:&&" : "OP_AMP:&") : "OP_STAR:*")));
	if(!parameter_name.empty()) inner->children.push_back(CPPGMAstNodePtr(
		new CPPGMAstNode("identifier", parameter_name)));
	nested->children.push_back(inner);
	declarator->children.push_back(nested);
	declarator->children.push_back(CloneNode(signature.parameters));
	result->children.push_back(declarator);
	return result;
}

CPPGMAstNodePtr PA18TemplateExpander::MakeForwardClass(const string& name) const
{
	CPPGMAstNodePtr result(new CPPGMAstNode("class-forward-declaration", name));
	result->children.push_back(CPPGMAstNodePtr(
		new CPPGMAstNode("class-key", "KW_STRUCT:struct")));
	return result;
}

} // namespace pa18_templates_internal
