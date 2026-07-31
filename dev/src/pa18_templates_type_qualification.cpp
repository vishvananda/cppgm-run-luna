#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

string CollapseRepeatedQualifier(string raw)
{
	for(size_t position = 0; position < raw.size();) {
		if(!IsIdentifierCharacter(raw[position]) ||
			(position > 0 && IsIdentifierCharacter(raw[position - 1]))) {
			++position;
			continue;
		}
		size_t first_end = position;
		while(first_end < raw.size() && IsIdentifierCharacter(raw[first_end])) ++first_end;
		if(first_end + 2 > raw.size() || raw.compare(first_end, 2, "::") != 0) {
			position = first_end;
			continue;
		}
		const string component = raw.substr(position, first_end - position);
		const size_t second_begin = first_end + 2;
		if(raw.compare(second_begin, component.size(), component) != 0 ||
			(second_begin + component.size()) + 2 > raw.size() ||
			raw.compare(second_begin + component.size(), 2, "::") != 0) {
			position = first_end + 2;
			continue;
		}
		raw.erase(second_begin, component.size() + 2);
		position = position + component.size() + 2;
	}
	return raw;
}

string PA18TemplateExpander::QualifyTypeArgument(string spelling, const string& context,
	const string& template_owner, bool preserve_nested_namespace) const
{
	if(!context.empty() && (context[0] == '!' || context[0] == '~' ||
		context[0] == '+' || context[0] == '-')) {
		string lookup_context = context;
		while(!lookup_context.empty() && (lookup_context[0] == '!' ||
			lookup_context[0] == '~' || lookup_context[0] == '+' ||
			lookup_context[0] == '-')) lookup_context.erase(lookup_context.begin());
		if(lookup_context != context)
			return QualifyTypeArgument(spelling, lookup_context, template_owner,
				preserve_nested_namespace);
	}
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
	const auto known_type = [this](const string& candidate) {
		return class_contexts_.find(candidate) != class_contexts_.end() ||
			named_type_contexts_.find(candidate) != named_type_contexts_.end() ||
			class_declarations_.find(candidate) != class_declarations_.end();
	};
	// Resolve a type through the indexed declaration paths.  The lexical walk
	// keeps ordinary lookup precedence; the unique indexed suffix is only used
	// when a dependent replay has lost its namespace prefix.
	const auto indexed_type_path = [&](const string& raw_name, const string& primary,
		const string& secondary, string* resolved) {
		if(!resolved) return false;
		const size_t open = raw_name.find('<');
		const string base = open == string::npos ? raw_name : raw_name.substr(0, open);
		const string short_name = LastComponent(base);
		const auto has_repeated_component = [](const string& path) {
			string previous;
			for(size_t component = 0; component < path.size();) {
				while(component < path.size() && path[component] == ':') ++component;
				const size_t end = path.find("::", component);
				const string current = path.substr(component, end == string::npos ?
					string::npos : end - component);
				if(!current.empty() && current == previous) return true;
				previous = current;
				if(end == string::npos) break;
				component = end + 2;
			}
			return false;
		};
		if(has_repeated_component(base)) return false;
		string top = base;
		if(top.compare(0, 2, "::") == 0) top.erase(0, 2);
		const size_t top_separator = top.find("::");
		if(top_separator != string::npos) top.erase(top_separator);
		map<string, vector<string> >::const_iterator indexed =
			class_paths_by_name_.find(short_name);
		if(indexed == class_paths_by_name_.end()) return false;
		const auto try_scope = [&](const string& scope) {
			for(string current = scope; ; ) {
				const string candidate = JoinPath(current, base);
				if(known_type(candidate)) {
					*resolved = candidate;
					return true;
				}
				if(current.empty()) break;
				const size_t parent = current.rfind("::");
				if(parent == string::npos) current.clear();
				else current.erase(parent);
			}
			return false;
		};
		if(try_scope(primary) || try_scope(secondary)) return true;
		const auto try_short_scope = [&](const string& scope) {
			for(string current = scope; ; ) {
				const string candidate = JoinPath(current, short_name);
				if(known_type(candidate)) {
					*resolved = candidate;
					return true;
				}
				if(current.empty()) break;
				const size_t parent = current.rfind("::");
				if(parent == string::npos) current.clear();
				else current.erase(parent);
			}
			return false;
		};
		if(try_short_scope(primary) || try_short_scope(secondary)) return true;
		// A bare dependent name must not be rebound to an arbitrary class that
		// merely shares its short name.  Only a qualified spelling may use the
		// narrowed suffix index after lexical lookup has failed.
		if(base.find("::") == string::npos) return false;
		string match;
		const string suffix = base.compare(0, 2, "::") == 0 ? base.substr(2) : base;
		const string redundant_prefix = top + "::" + top + "::";
		for(size_t path = 0; path < indexed->second.size(); ++path) {
			const string& candidate = indexed->second[path];
			if(has_repeated_component(candidate)) continue;
			if(candidate.compare(0, redundant_prefix.size(), redundant_prefix) == 0) continue;
			if(candidate == suffix || (candidate.size() > suffix.size() &&
				candidate.compare(candidate.size() - suffix.size(), suffix.size(), suffix) == 0 &&
				candidate[candidate.size() - suffix.size() - 1] == ':')) {
				if(match.empty()) match = candidate;
				else if(match != candidate) return false;
			}
		}
		if(match.empty()) {
			string compatible;
			for(size_t path = 0; path < indexed->second.size(); ++path) {
				const string& candidate = indexed->second[path];
				if(has_repeated_component(candidate)) continue;
				if(candidate.compare(0, redundant_prefix.size(), redundant_prefix) == 0) continue;
				string candidate_top = candidate;
				const size_t separator = candidate_top.find("::");
				if(separator != string::npos) candidate_top.erase(separator);
				if(candidate_top != top) continue;
				if(!compatible.empty() && compatible != candidate) return false;
				compatible = candidate;
			}
			match = compatible;
		}
		if(match.empty()) return false;
		*resolved = match;
		return true;
	};
	string indexed_spelling;
	if(spelling.find("::") != string::npos &&
		indexed_type_path(spelling, template_owner, context, &indexed_spelling)) {
		const size_t open = spelling.find('<');
		spelling = indexed_spelling + (open == string::npos ? string() : spelling.substr(open));
	}
	if(spelling.find("::") == string::npos) {
		map<string, vector<string> >::const_iterator indexed_name =
			class_paths_by_name_.find(spelling);
		string anonymous_candidate;
		if(indexed_name != class_paths_by_name_.end())
			for(size_t path = 0; path < indexed_name->second.size(); ++path) {
				const string& candidate = indexed_name->second[path];
				const size_t separator = candidate.rfind("::");
				const string owner = separator == string::npos ? string() :
					candidate.substr(0, separator);
				if(owner.find("<unnamed>") == string::npos) continue;
				map<string, string>::const_iterator logical =
					lexical_namespace_logical_.find(owner);
				if(logical == lexical_namespace_logical_.end()) continue;
				const bool visible = context == logical->second ||
					(context.size() > logical->second.size() &&
					 context.compare(0, logical->second.size(), logical->second) == 0 &&
					 context[logical->second.size()] == ':');
				if(!visible) continue;
				if(!anonymous_candidate.empty() && anonymous_candidate != candidate)
					anonymous_candidate.clear();
				else anonymous_candidate = candidate;
			}
		if(!anonymous_candidate.empty()) spelling = anonymous_candidate;
	}
	const string promoted_local = PromotedLocalClass(spelling, context);
	if(!promoted_local.empty()) spelling = promoted_local;
	if(spelling.find("::") == string::npos && spelling.find('<') == string::npos) {
		map<string, string>::const_iterator generated_base =
			specialization_bases_.find(LastComponent(spelling));
		map<string, vector<string> >::const_iterator generated_arguments =
			specialization_arguments_.find(LastComponent(spelling));
		if(generated_base != specialization_bases_.end() &&
			generated_arguments != specialization_arguments_.end()) {
			const string raw_generated_owner = PrefixComponent(generated_base->second);
			const size_t raw_repeated_separator = raw_generated_owner.rfind("::");
			const bool repeated_generated_owner = raw_repeated_separator != string::npos &&
				LastComponent(raw_generated_owner.substr(0, raw_repeated_separator)) ==
				raw_generated_owner.substr(raw_repeated_separator + 2);
			string generated_owner = raw_generated_owner;
			const size_t repeated_separator = generated_owner.rfind("::");
			if(repeated_separator != string::npos &&
				LastComponent(generated_owner.substr(0, repeated_separator)) ==
				generated_owner.substr(repeated_separator + 2))
				generated_owner.erase(repeated_separator);
			if(!repeated_generated_owner && generated_owner.find("::") != string::npos &&
				generated_owner.find('<') == string::npos) {
				const string owner_candidate = JoinPath(generated_owner, spelling);
				if(!generated_owner.empty() && known_type(owner_candidate)) spelling = owner_candidate;
				else {
					map<string, vector<string> >::const_iterator paths =
						class_paths_by_name_.find(LastComponent(spelling));
					if(paths != class_paths_by_name_.end()) {
						string selected;
						for(size_t path = 0; path < paths->second.size(); ++path) {
							const string& candidate = paths->second[path];
							if(!known_type(candidate) ||
								PrefixComponent(candidate) != generated_owner) continue;
							if(selected.empty() || candidate < selected) selected = candidate;
						}
						if(!selected.empty()) spelling = selected;
					}
				}
			}
		}
	}
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
		// An elaborated type can introduce an implicit member class while a
		// class template is replayed (for example `struct PrivateNat` inside
		// `Holder<T>`).  The source declaration is indexed under `Holder`, but
		// the type argument belongs to the concrete `Holder_T` specialization.
		// Carry that typed owner into an alias specialization instead of leaving
		// a bare name for the namespace-scope alias body to resolve.
		if(!active_instantiation_name_.empty()) {
			map<string, string>::const_iterator active_base = specialization_bases_.find(
				LastComponent(active_instantiation_name_));
			const string source_owner = PrefixComponent(spelling);
			if(active_base != specialization_bases_.end() &&
				!source_owner.empty() && source_owner == active_base->second &&
				class_declarations_.find(spelling) != class_declarations_.end())
				spelling = active_instantiation_name_ + "::" + LastComponent(spelling);
		}
	const size_t separator = spelling.find("::");
	const string first = spelling.substr(0, separator);
	const string remainder = spelling.substr(separator);
		for(string current = context; ; ) {
			const string candidate = JoinPath(current, first);
			const string full_candidate = candidate + remainder;
			const bool nested_type = class_contexts_.find(full_candidate) !=
				class_contexts_.end() || named_type_contexts_.find(full_candidate) !=
				named_type_contexts_.end() || class_declarations_.find(full_candidate) !=
				class_declarations_.end();
			if(class_contexts_.find(candidate) != class_contexts_.end() ||
				(preserve_nested_namespace && nested_type)) {
				spelling = full_candidate;
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
		const string owner_relative = spelling.substr(owner_prefix.size());
		if(!generated_specialization && (!preserve_nested_namespace ||
			owner_relative.find("::") == string::npos))
			spelling.erase(0, owner_prefix.size());
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
		{
			return CanonicalSpelling(prefix + spelling + suffix);
		}
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
			spelling = preserve_nested_namespace || (function_scope && !same_template_owner) ||
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
	string result = CollapseRepeatedQualifier(CanonicalSpelling(prefix + spelling + suffix));
	const size_t nested_open = result.find('<');
	if((context.find("<unnamed>") != string::npos ||
		result.find("<unnamed>") != string::npos) && nested_open != string::npos) {
		string nested_arguments;
		size_t nested_close = string::npos;
		if(TemplateRange(result, nested_open, &nested_arguments, &nested_close)) {
			const vector<string> nested_parts = SplitTemplateArguments(nested_arguments);
			string qualified_result = result.substr(0, nested_open) + "<";
			for(size_t nested = 0; nested < nested_parts.size(); ++nested) {
				if(nested) qualified_result += ",";
				qualified_result += QualifyTypeArgument(nested_parts[nested], context,
					template_owner, preserve_nested_namespace);
			}
			qualified_result += ">" + result.substr(nested_close + 1);
			result = CollapseRepeatedQualifier(CanonicalSpelling(qualified_result));
		}
	}
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
						// The template-id's trailing cv/ref/pointer is part of the
						// specialization argument identity.  Dropping it made
						// `Trait<X>` and `Trait<X const>` reuse the first generated
						// class while replaying a qualified dependent type.
						return normalized + ">" + CanonicalSpelling(value.substr(close + 1));
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
		map<string, vector<string> >::const_iterator names =
			specialization_names_by_base_.find(LastComponent(generated->second));
		if(names != specialization_names_by_base_.end()) for(size_t name = 0;
			name < names->second.size(); ++name) {
			const string& candidate_name = names->second[name];
			map<string, string>::const_iterator candidate = specialization_bases_.find(
				candidate_name);
			if(candidate == specialization_bases_.end() || candidate->second != generated->second)
				continue;
			map<string, vector<string> >::const_iterator candidate_arguments =
				specialization_arguments_.find(candidate_name);
			if(candidate_arguments == specialization_arguments_.end() ||
				candidate_arguments->second.size() != wanted.size()) continue;
			bool same = true;
			for(size_t argument = 0; argument < wanted.size(); ++argument)
				if(identity(candidate_arguments->second[argument]) != identity(wanted[argument])) {
					same = false;
					break;
				}
			if(same && candidate_name.size() < best.size()) best = candidate_name;
		}
	if(best != generated->first) {
			const string owner = PrefixComponent(result);
			return owner.empty() ? best : owner + "::" + best;
		}
	}
	return result;
}

} // namespace pa18_templates_internal
