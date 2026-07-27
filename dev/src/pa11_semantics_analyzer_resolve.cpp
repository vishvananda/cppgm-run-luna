#include "pa11_semantics_analyzer.h"

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
	if ((add_const || add_volatile) && fundamental && !fundamental_words.empty())
		return CloneWithCv(Fundamental(FundamentalName(fundamental_words)),
			add_const, add_volatile);
	if (from && name.find("::") != string::npos &&
		IsDependentTemplateName(from, name))
		return TypePtr(new Type(TYPE_TEMPLATE_PARAMETER, name));
	const size_t template_open = name.find('<');
	if (template_open != string::npos && name.size() > template_open + 1 &&
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
