#include "pa11_semantics_analyzer.h"

vector<string> Analyzer::SplitPath(const string& raw, bool* absolute) const
{
	string path = raw;
	const bool is_absolute = path.compare(0, 2, "::") == 0;
	if(is_absolute) path = path.substr(2);
	vector<string> parts;
	for(size_t begin = 0; begin <= path.size();) {
		const size_t end = path.find("::", begin);
		const string part = path.substr(begin, end == string::npos ?
			string::npos : end - begin);
		if(!part.empty()) parts.push_back(part);
		if(end == string::npos) break;
		begin = end + 2;
	}
	if(absolute) *absolute = is_absolute;
	return parts;
}

Analyzer::PathTarget Analyzer::ResolvePath(Scope* from, const string& raw) const
{
	bool absolute = false;
	const vector<string> parts = SplitPath(raw, &absolute);
	if(parts.empty()) return PathTarget();
	Scope* current_scope = absolute ? global_.get() : from;
	Binding* current_binding = 0;
	for(size_t i = 0; i < parts.size(); ++i) {
		const string& part = parts[i];
		if(current_binding) {
			current_scope = ScopeForType(current_binding->type);
			if(!current_scope) return PathTarget();
			current_binding = 0;
		}
		if(i == 0 && !absolute) {
			Binding* binding = LookupUnqualified(current_scope, part);
			if(binding) {
				current_binding = binding;
				if(i + 1 == parts.size()) return PathTarget(0, binding);
				continue;
			}
		}
		Scope* namespace_scope = (i == 0 && !absolute) ?
			FindNamespace(current_scope, part) : FindNamespaceDirect(current_scope, part);
		if(!namespace_scope && part == "<unnamed>" && current_scope)
			for(size_t using_index = 0;
				using_index < current_scope->using_directives.size(); ++using_index) {
				Scope* candidate = current_scope->using_directives[using_index];
				if(candidate && candidate->kind == SCOPE_NAMESPACE &&
					candidate->name == part) {
					namespace_scope = candidate;
					break;
				}
			}
		if(namespace_scope) {
			current_scope = namespace_scope;
			if(i + 1 == parts.size()) return PathTarget(namespace_scope, 0);
			continue;
		}
		Binding* binding = (i == 0 && !absolute) ?
			LookupUnqualified(current_scope, part) : LookupInNamespace(current_scope, part);
		if(!binding && current_scope && current_scope->kind == SCOPE_CLASS &&
			current_scope->owner_type)
			for(TypePtr base = current_scope->owner_type->direct_base; base;
				base = base->direct_base) {
				if(LastComponent(base->name) == part && base->owned_scope &&
					base->owned_scope->parent)
					binding = base->owned_scope->parent->local(part);
				if(!binding && base->owned_scope) binding = base->owned_scope->local(part);
				if(binding) break;
			}
		if(!binding) return PathTarget();
		if(i + 1 == parts.size()) return PathTarget(0, binding);
		current_binding = binding;
	}
	return current_binding ? PathTarget(0, current_binding) :
		PathTarget(current_scope, 0);
}

Scope* Analyzer::ResolveNamespace(Scope* from, const string& raw) const
{
	return ResolvePath(from, raw).scope;
}

Binding* Analyzer::ResolveBinding(Scope* from, const string& raw) const
{
	return ResolvePath(from, raw).binding;
}

TypePtr Analyzer::ResolveType(Scope* from, const string& raw) const
{
	const string name = StripTypeMarker(raw);
	vector<string> words;
	string word;
	for (size_t i = 0; i <= name.size(); ++i) {
		const char character = i < name.size() ? name[i] : ' ';
		if (isspace(static_cast<unsigned char>(character))) {
			if (!word.empty()) { words.push_back(word); word.clear(); }
		} else word += character;
	}
	bool add_const = false, add_volatile = false, fundamental = !words.empty();
	vector<string> fundamental_words;
	for (size_t i = 0; i < words.size(); ++i) {
		if (words[i] == "const") add_const = true;
		else if (words[i] == "volatile") add_volatile = true;
		else {
			fundamental_words.push_back(words[i]);
			if (!IsFundamentalWord(words[i])) fundamental = false;
		}
	}
	if (fundamental && !fundamental_words.empty())
		return CloneWithCv(Fundamental(FundamentalName(fundamental_words)),
			add_const, add_volatile);
	TypePtr member_pointer = ResolveMemberPointerSpelling(from, name);
	if(member_pointer) return member_pointer;
	TypePtr declarator = ResolveDeclaratorSpelling(from, name);
	if(declarator) return declarator;
	if (from && name.find("::") != string::npos && IsDependentTemplateName(from, name))
		return TypePtr(new Type(TYPE_TEMPLATE_PARAMETER, name));
	const size_t template_open = name.find('<'); if (template_open != string::npos && name.size() > template_open + 1 &&
		name[name.size() - 1] == '>')
	{
		const string primary_name = name.substr(0, template_open);
		TypePtr primary = ResolveType(from, primary_name);
		if (primary && (primary->kind == TYPE_CLASS ||
			primary->kind == TYPE_TEMPLATE_PARAMETER))
		{
			vector<string> arguments;
			string current;
			int depth = 0;
			for (size_t i = template_open + 1; i + 1 < name.size(); ++i)
			{
				const char ch = name[i];
				if (ch == '<') ++depth;
				else if (ch == '>' && depth > 0) --depth;
				if (ch == ',' && depth == 0)
				{
					while (!current.empty() && isspace(static_cast<unsigned char>(current[0])))
						current.erase(0, 1);
					while (!current.empty() && isspace(static_cast<unsigned char>(current[current.size() - 1])))
						current.erase(current.size() - 1);
					arguments.push_back(current);
					current.clear();
				}
				else current += ch;
			}
			while (!current.empty() && isspace(static_cast<unsigned char>(current[0])))
				current.erase(0, 1);
			while (!current.empty() && isspace(static_cast<unsigned char>(current[current.size() - 1])))
				current.erase(current.size() - 1);
			if (!current.empty()) arguments.push_back(current);
			TypePtr specialization(new Type(*primary));
			specialization->name = name;
			specialization->template_specialization = true;
			specialization->template_primary = primary->name;
			specialization->template_arguments = arguments;
			return specialization;
		}
	}
	if (name.find("::") == string::npos)
	{
		for (Scope* current = from; current; current = current->parent)
			for (size_t i = current->bindings.size(); i > 0; --i)
			{
				const Binding& candidate = current->bindings[i - 1];
				if (candidate.name == name && (candidate.kind == BIND_TYPE ||
					candidate.kind == BIND_TYPE_ALIAS) && AccessibleType(candidate, from))
					return candidate.type;
			}
		for (Scope* current = from; current; current = current->parent)
			if (current->kind == SCOPE_CLASS && current->owner_type)
				for (TypePtr base = current->owner_type->direct_base; base;
					base = base->direct_base)
				{
					if (LastComponent(base->name) == name) return base;
					if (!base->owned_scope) continue;
					for (size_t i = base->owned_scope->bindings.size(); i > 0; --i)
					{
						const Binding& candidate = base->owned_scope->bindings[i - 1];
						if (candidate.name == name && (candidate.kind == BIND_TYPE ||
							candidate.kind == BIND_TYPE_ALIAS) && AccessibleType(candidate, from))
							return candidate.type;
					}
				}
	}
	Binding* binding = ResolveBinding(from, name);
	if (!binding || (binding->kind != BIND_TYPE &&
		binding->kind != BIND_TYPE_ALIAS) || !AccessibleType(*binding, from))
	{
		if (!binding && name.find("::") != string::npos) {
			const size_t separator = name.rfind("::");
			PathTarget owner = ResolvePath(from, name.substr(0, separator));
			Scope* owner_scope = owner.scope;
			if (!owner_scope && owner.binding) owner_scope = ScopeForType(owner.binding->type);
			const string requested = LastComponent(name.substr(separator + 2));
			if (owner_scope) for (size_t i = 0; i < owner_scope->bindings.size(); ++i) {
				const Binding& candidate = owner_scope->bindings[i];
				TypePtr type = candidate.type;
				if ((candidate.kind != BIND_TYPE && candidate.kind != BIND_TYPE_ALIAS) || !type)
					continue;
				if (type->kind == TYPE_CLASS &&
					(LastComponent(type->name) == requested ||
					 LastComponent(type->template_primary) == requested))
					return type;
			}
		}
		throw logic_error("unknown type: " + raw);
	}
	return binding->type;
}
