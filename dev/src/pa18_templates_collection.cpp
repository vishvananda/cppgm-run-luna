#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

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
		const string owner_name = PrefixComponent(type);
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

string QualifyAliasTarget(const string& target, const string& alias,
	const set<string>& class_contexts)
{
	const string owner = PrefixComponent(alias);
	return !owner.empty() && target.find("::") == string::npos &&
		class_contexts.find(JoinPath(owner, target)) != class_contexts.end() ?
		JoinPath(owner, target) : target;
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
	const TemplateDefinition& definition, const string& key)
{
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
		const size_t visible_end = qualified.find("::", visible_begin);
		if(visible_begin < qualified.size())
			using_directive_exports_[target].push_back(make_pair(
				qualified.substr(visible_begin,
					visible_end == string::npos ? string::npos : visible_end - visible_begin),
				key));
		component_begin = visible_begin;
	}
}

string PA18TemplateExpander::GeneratedOwner(const TemplateDefinition& definition) const
{
	return definition.lexical_owner.empty() ? definition.owner : definition.lexical_owner;
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
	for(size_t i = 0; i < sizeof(compact_fundamentals) / sizeof(compact_fundamentals[0]); ++i)
		if(raw == compact_fundamentals[i][0]) { raw = compact_fundamentals[i][1]; break; }
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
	return CanonicalSpelling(raw);
}

vector<string> SplitTemplateArguments(const string& raw)
{
	vector<string> result;
	string current;
	int angle = 0;
	vector<int> angle_parentheses;
	int parentheses = 0, brackets = 0;
	for(size_t i = 0; i < raw.size(); ++i) {
		const char ch = raw[i];
		if(ch == '(') ++parentheses;
		else if(ch == ')' && parentheses > 0) --parentheses;
		if(ch == '[') ++brackets;
		else if(ch == ']' && brackets > 0) --brackets;
		if(ch == '<' && IsTemplateAngleOpen(raw, i)) {
			++angle; angle_parentheses.push_back(parentheses);
		} else if(ch == '>' && angle > 0 && IsTemplateAngleClose(raw, i)) {
			const int opener_parentheses = angle_parentheses.empty() ? 0 : angle_parentheses.back();
			if(parentheses > opener_parentheses) continue;
			--angle;
			if(!angle_parentheses.empty()) angle_parentheses.pop_back();
		}
		if(ch == ',' && angle == 0 && parentheses == 0 && brackets == 0) {
			result.push_back(CanonicalSpelling(current)); current.clear();
		} else current += ch;
	}
	if(!current.empty() || !result.empty()) result.push_back(CanonicalSpelling(current));
	if(result.size() == 1 && result[0].empty()) result.clear();
	return result;
}

string ReplaceIdentifiersPreservingPackSizes(const string& raw,
	const map<string, string>& substitutions)
{
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
		result += ReplaceIdentifiers(raw.substr(cursor, search - cursor), substitutions);
		result += raw.substr(search, close - search + 1);
		cursor = close + 1;
		search = raw.find("sizeof...", cursor);
	}
	result += ReplaceIdentifiers(raw.substr(cursor), substitutions);
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
		const string specifiers = SpellNode(declaration->children[0]);
		if(specifiers.find("const") == string::npos &&
			specifiers.find("constexpr") == string::npos) continue;
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
	if((node->kind == "decl-specifier" || node->kind == "specifier") &&
		RemoveMarker(node->value) == wanted) return true;
	for(size_t child = 0; child < node->children.size(); ++child)
		if(HasDeclarationSpecifier(node->children[child], wanted)) return true;
	return false;
}

bool PA18TemplateExpander::HasStaticMember(const TemplateDefinition* definition,
	const string& owner, const string& name) const
{
	if(name.empty()) return false;
	if(definition && definition->static_members.find(name) !=
		definition->static_members.end()) return true;
	map<string, set<string> >::const_iterator indexed =
		static_members_by_class_.find(owner);
	return indexed != static_members_by_class_.end() &&
		indexed->second.find(name) != indexed->second.end();
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
	ValidateTemplateDiagnostics(input);
	for(size_t i = 0; i < input.size(); ++i)
		CollectLexical(input[i], string(), string());
	for(size_t i = 0; i < input.size(); ++i) Collect(input[i], string());
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
		if(!member_name && node->value.find("::") == string::npos &&
			node->value.find('<') == string::npos &&
			!ValidationDependentName(node->value, parameters) &&
			known_names.find(node->value) == known_names.end() &&
			node->value.compare(0, 8, "operator") != 0 &&
			node->value.compare(0, 10, "__builtin_") != 0)
			throw logic_error("unknown nondependent template name: " + node->value);
	}
	const bool class_node = node->kind == "class-specifier" ||
		node->kind == "class-forward-declaration";
	const string next_class = class_node ? LastComponent(node->value) : current_class;
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
			const string member = CanonicalSpelling(RemoveMarker(node->children[1]->value));
			const bool disambiguated = member.compare(0, 8, "template") == 0 &&
				(member.size() == 8 || isspace(static_cast<unsigned char>(member[8])));
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

bool PA18TemplateExpander::ValidationTypeArgument(const string& raw,
	const map<string, bool>& parameters) const
{
	string spelling = CanonicalSpelling(raw);
	if(spelling.size() >= 3 &&
		spelling.compare(spelling.size() - 3, 3, "...") == 0)
		spelling = CanonicalSpelling(spelling.substr(0, spelling.size() - 3));
	while(spelling.compare(0, 8, "typename") == 0 &&
		(spelling.size() == 8 || isspace(static_cast<unsigned char>(spelling[8]))))
		spelling = CanonicalSpelling(spelling.substr(8));
	map<string, bool>::const_iterator parameter = parameters.find(spelling);
	return parameter != parameters.end() && parameter->second;
}

void PA18TemplateExpander::ValidateTemplateArgumentKinds(
	const CPPGMAstNodePtr& node, const string& inherited_context,
	const map<string, bool>& inherited_parameters) const
{
	if(!node) return;
	string context = inherited_context;
	map<const CPPGMAstNode*, string>::const_iterator lexical = lexical_contexts_.find(node.get());
	if(lexical != lexical_contexts_.end()) context = lexical->second;
	map<string, bool> parameters = inherited_parameters;
	if(node->kind == "template-declaration" && node->children.size() > 1) {
		const vector<TemplateParameter> own = Parameters(node->children[0]);
		for(size_t i = 0; i < own.size(); ++i) parameters[own[i].name] = own[i].type;
		ValidateTemplateArgumentKinds(node->children[1], context, parameters);
		return;
	}
	if(node->kind == "type-name" || node->kind == "decl-specifier" ||
		node->kind == "type-specifier") {
		const string raw = RemoveMarker(node->value);
		const size_t open = raw.find('<');
		if(open != string::npos) {
			string base, argument_text;
			size_t begin = 0, close = string::npos;
			if(TemplateBase(raw, open, &begin, &base) &&
				TemplateRange(raw, open, &argument_text, &close)) {
				const TemplateDefinition* definition = FindDefinition(base, context);
				if(definition) {
					const vector<string> arguments = SplitTemplateArguments(argument_text);
					size_t parameter = 0;
					for(size_t argument = 0; argument < arguments.size(); ++argument) {
						while(parameter < definition->parameters.size() &&
							!definition->parameters[parameter].pack && argument > parameter) ++parameter;
						if(parameter >= definition->parameters.size()) break;
						const TemplateParameter& expected = definition->parameters[parameter];
						if(!expected.type && ValidationTypeArgument(arguments[argument], parameters))
							throw logic_error("type used as non-type template argument");
						if(!expected.pack) ++parameter;
					}
				}
			}
		}
	}
	for(size_t i = 0; i < node->children.size(); ++i)
		ValidateTemplateArgumentKinds(node->children[i], context, parameters);
}

string PA18TemplateExpander::ResolveAlias(string spelling, const string& context) const
{
	spelling = CanonicalSpelling(spelling);
	string cv_prefix;
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
		if(direct == type_aliases_.end()) {
			for(string current = context; direct == type_aliases_.end(); ) {
				const string candidate = JoinPath(current, spelling);
				direct = type_aliases_.find(candidate);
				const size_t separator = current.rfind("::");
				if(separator == string::npos) break;
				current.erase(separator);
			}
		}
		if(direct == type_aliases_.end()) {
			const string short_name = LastComponent(spelling);
			map<string, vector<string> >::const_iterator candidates = type_aliases_by_name_.find(short_name);
			if(spelling.find("::") == string::npos &&
				candidates != type_aliases_by_name_.end() && candidates->second.size() == 1)
				direct = type_aliases_.find(candidates->second[0]);
		}
		if(direct == type_aliases_.end()) {
			// Match a qualified alias through an inline namespace in the same
			// logical namespace.  The alias remains stored under its physical
			// owner, while source lookup is allowed to omit the inline component.
			const size_t raw_separator = TopLevelScopeSeparator(spelling);
			if(raw_separator != string::npos) {
				const string logical_owner = spelling.substr(0, raw_separator);
				const string logical_name = spelling.substr(raw_separator + 2);
				map<string, string>::const_iterator logical_match = type_aliases_.end();
				for(map<string, string>::const_iterator it = type_aliases_.begin();
					it != type_aliases_.end(); ++it) {
					if(LastComponent(it->first) != logical_name) continue;
					const string physical_owner = PrefixComponent(it->first);
					map<string, string>::const_iterator logical =
						lexical_namespace_logical_.find(physical_owner);
					if(logical == lexical_namespace_logical_.end() ||
						logical->second != logical_owner) continue;
					if(logical_match != type_aliases_.end()) {
						logical_match = type_aliases_.end();
						break;
					}
					logical_match = it;
				}
				if(logical_match != type_aliases_.end()) direct = logical_match;
			}
		}
		if(direct != type_aliases_.end()) {
			string target = QualifyAliasTarget(direct->second, direct->first, class_contexts_);
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
		const size_t separator = spelling.rfind("::");
		if(separator == string::npos) break;
		string class_key = spelling.substr(0, separator);
		const string member = spelling.substr(separator + 2);
		string member_type = MemberAliasType(class_key, member);
		if(member_type.empty()) {
			for(string current = context; member_type.empty() && !current.empty(); ) {
				member_type = MemberAliasType(JoinPath(current, class_key), member);
				const size_t parent = current.rfind("::");
				if(parent == string::npos) current.clear();
				else current.erase(parent);
			}
		}
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
		spelling = CanonicalSpelling(member_type);
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
	if(parameter_name.empty()) parameter_name = "function";
	CPPGMAstNodePtr result(new CPPGMAstNode("parameter-declaration"));
	result->children.push_back(CloneNode(signature.result_specifiers));
	CPPGMAstNodePtr declarator(new CPPGMAstNode("declarator"));
	CPPGMAstNodePtr nested(new CPPGMAstNode("nested-declarator"));
	CPPGMAstNodePtr inner(new CPPGMAstNode("declarator"));
	inner->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("ptr-operator",
		reference ? (rvalue_reference ? "OP_LAND:&&" : "OP_AMP:&") : "OP_STAR:*")));
	inner->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", parameter_name)));
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
