#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
using namespace std;
namespace pa18_templates_internal {
const TemplateDefinition* PA18TemplateExpander::SelectClassTemplateDefinition(
	const TemplateDefinition* primary, const vector<string>& arguments,
		const string& context) const
	{
		if(!primary) return primary;
		if(!primary->class_template && !primary->alias_template &&
			!primary->variable_template) return primary;
		const auto dependent_argument = [this, &context](const string& raw) {
			for(size_t position = 0; position < raw.size();) {
				if(!IsIdentifierCharacter(raw[position])) {
					++position;
					continue;
				}
				const size_t begin = position;
				while(position < raw.size() && IsIdentifierCharacter(raw[position])) ++position;
				const string word = raw.substr(begin, position - begin);
				if(!word.empty() && isdigit(static_cast<unsigned char>(word[0]))) continue;
				size_t next = position;
				while(next < raw.size() && isspace(static_cast<unsigned char>(raw[next]))) ++next;
				if(next + 1 < raw.size() && raw.compare(next, 2, "::") == 0) continue;
				// A qualified component can be a concrete static member of a known
				// class template (`trait<T>::value`).  It is not itself a dependent
				// template argument when the typed member index already contains that
				// member.  Keep unknown qualified members dependent so an unresolved
				// probe still routes through substitution failure.
				bool known_qualified_member = false;
				if(begin >= 2 && raw.compare(begin - 2, 2, "::") == 0) {
					const string owner = Trim(raw.substr(0, begin - 2));
					if(!owner.empty() &&
						IsKnownTypeSpelling(owner, context)) {
						string member_type;
						set<string> active_members;
						known_qualified_member = FindClassMemberType(owner, word,
							map<string, string>(), context, &member_type, &active_members,
							false) && !member_type.empty();
						if(!known_qualified_member) {
							const CPPGMAstNodePtr owner_declaration = FindClassDeclaration(
								owner, context);
							const function<bool(const CPPGMAstNodePtr&)> declares_member =
								[&](const CPPGMAstNodePtr& node) {
								if(!node) return false;
								if(node->kind == "enumerator" && LastComponent(node->value) == word)
									return true;
								if(node->kind == "alias-declaration" &&
									LastComponent(RemoveMarker(node->value)) == word) return true;
								if(node->kind == "simple-declaration") {
									const CPPGMAstNodePtr list = ChildOfKindLocal(node,
										"init-declarator-list");
									if(list) for(size_t item = 0; item < list->children.size(); ++item)
										if(list->children[item] && !list->children[item]->children.empty() &&
											LastComponent(FirstIdentifierLocal(
												list->children[item]->children[0])) == word) return true;
								}
								for(size_t child = 0; child < node->children.size(); ++child)
									if(declares_member(node->children[child])) return true;
								return false;
							};
						known_qualified_member = declares_member(owner_declaration);
						}
					}
				}
				const bool known = word == "typename" || word == "const" || word == "volatile" ||
					word == "true" || word == "false" || word == "void" || word == "bool" ||
					word == "char" || word == "short" || word == "int" || word == "long" ||
					word == "wchar_t" || word == "char16_t" || word == "char32_t" ||
					word == "float" || word == "double" ||
					word == "signed" || word == "unsigned" ||
					class_contexts_.find(word) != class_contexts_.end() ||
					class_declarations_.find(word) != class_declarations_.end() ||
					named_type_contexts_.find(word) != named_type_contexts_.end() ||
					FindClassDeclaration(word, context) != CPPGMAstNodePtr() ||
					definitions_by_name_.find(word) != definitions_by_name_.end() ||
					type_aliases_.find(word) != type_aliases_.end() ||
					known_qualified_member ||
					specialization_bases_.find(word) != specialization_bases_.end();
				if(!known) return true;
			}
			return false;
		};
		for(size_t argument = 0; argument < arguments.size(); ++argument)
			if(dependent_argument(arguments[argument])) return primary;
		vector<string> normalized_arguments;
		normalized_arguments.reserve(arguments.size());
		for(size_t argument = 0; argument < arguments.size(); ++argument) {
			string normalized = NormalizeTypeArgument(RestoreSpecializationSpelling(
				arguments[argument]));
			normalized_arguments.push_back(CollapseRepeatedQualifiedPath(
				CollapseRepeatedQualifier(normalized)));
		}
		// A selection may recursively ask for the same semantic family while a
		// partial specialization is being matched.  That is substitution failure,
		// not a successful primary-template fallback.  Keep the ordinary selection
		// key separate so distinct concrete arguments can still be evaluated.
		string selection_family = primary->qualified_name + "|" + context;
		for(size_t argument = 0; argument < normalized_arguments.size(); ++argument)
			selection_family += "|" + normalized_arguments[argument];
		if(!active_class_template_selection_families_.insert(selection_family).second)
			throw PA18SubstitutionFailure("recursive class template selection");
		struct SelectionFamilyScope {
			set<string>* active;
			string key;
			SelectionFamilyScope(set<string>* value, const string& name) : active(value), key(name) {}
			~SelectionFamilyScope() { active->erase(key); }
		} selection_family_scope(&active_class_template_selection_families_, selection_family);
		string selection_key = primary->qualified_name + "|" + context;
		for(size_t argument = 0; argument < normalized_arguments.size(); ++argument)
			selection_key += "|" + normalized_arguments[argument];
		if(!active_class_template_selections_.insert(selection_key).second)
			return primary;
		struct SelectionScope {
			set<string>* active;
			string key;
			SelectionScope(set<string>* value, const string& name) : active(value), key(name) {}
			~SelectionScope() { active->erase(key); }
		} selection_scope(&active_class_template_selections_, selection_key);
		vector<const TemplateDefinition*> candidates;
		map<string, vector<TemplateDefinition> >::const_iterator direct_candidates =
			class_specializations_.find(primary->qualified_name);
		if(direct_candidates != class_specializations_.end())
			for(size_t i = 0; i < direct_candidates->second.size(); ++i)
				candidates.push_back(&direct_candidates->second[i]);
		else {
			const auto strip_template_arguments = [](const string& raw) {
				string result;
				int depth = 0;
				for(size_t i = 0; i < raw.size(); ++i) {
					if(raw[i] == '<') { ++depth; continue; }
					if(raw[i] == '>') { if(depth > 0) --depth; continue; }
					if(depth == 0) result += raw[i];
				}
				return result;
			};
			const auto collapse_repeated_owner = [](string raw) {
				const size_t separator = raw.rfind("::");
				if(separator != string::npos &&
					LastComponent(raw.substr(0, separator)) == raw.substr(separator + 2))
					raw.erase(separator);
				return raw;
			};
			const string primary_owner = collapse_repeated_owner(
				strip_template_arguments(primary->owner));
			map<string, set<string> >::const_iterator indexed_groups =
				class_specialization_groups_by_name_.find(primary->name);
			if(indexed_groups != class_specialization_groups_by_name_.end())
				for(set<string>::const_iterator group = indexed_groups->second.begin();
					group != indexed_groups->second.end(); ++group) {
					map<string, vector<TemplateDefinition> >::const_iterator definitions =
						class_specializations_.find(*group);
					if(definitions == class_specializations_.end()) continue;
					for(size_t i = 0; i < definitions->second.size(); ++i) {
						const TemplateDefinition& candidate = definitions->second[i];
						const string candidate_owner = collapse_repeated_owner(
							strip_template_arguments(candidate.owner));
						if(candidate_owner == primary_owner) candidates.push_back(&candidate);
					}
				}
		}
		struct CandidateIdentity {
			string qualified_name;
			vector<string> patterns;
			vector<string> parameters;
			vector<string> packs;
			bool operator<(const CandidateIdentity& other) const
			{
				if(qualified_name != other.qualified_name)
					return qualified_name < other.qualified_name;
				if(patterns != other.patterns) return patterns < other.patterns;
				if(parameters != other.parameters) return parameters < other.parameters;
				return packs < other.packs;
			}
		};
		set<CandidateIdentity> candidate_identities;
		vector<const TemplateDefinition*> unique_candidates;
		for(size_t candidate = 0; candidate < candidates.size(); ++candidate) {
			CandidateIdentity identity;
			identity.qualified_name = candidates[candidate]->qualified_name;
			for(size_t pattern = 0; pattern < candidates[candidate]->specialization_pattern.size(); ++pattern)
				identity.patterns.push_back(CanonicalSpelling(
					candidates[candidate]->specialization_pattern[pattern]));
			identity.parameters = candidates[candidate]->specialization_parameters;
			identity.packs = candidates[candidate]->specialization_pack_names;
			if(candidate_identities.insert(identity).second)
				unique_candidates.push_back(candidates[candidate]);
		}
		candidates.swap(unique_candidates);
		if(candidates.empty()) return primary;
		vector<string> matching_arguments = arguments;
		for(size_t argument = 0; argument < matching_arguments.size(); ++argument) {
			// A template-template argument is a template entity, not the type
			// produced by resolving an alias body.  Preserve its identity while
			// matching a class partial specialization; resolving `pointer_member`
			// here would turn it into the dependent `T::pointer` and erase the
			// detection idiom's substitution boundary.
			if(argument < primary->parameters.size() &&
				primary->parameters[argument].template_template) continue;
			const string resolved = NormalizeTypeArgument(ResolveAlias(
				matching_arguments[argument], context));
			if(!resolved.empty()) matching_arguments[argument] = resolved;
		}
		map<string, string> default_substitutions;
		for(size_t parameter = 0; parameter < primary->parameters.size() && parameter < arguments.size(); ++parameter)
			if(!primary->parameters[parameter].name.empty()) default_substitutions[primary->parameters[parameter].name] = arguments[parameter];
		for(size_t parameter = arguments.size(); parameter < primary->parameters.size(); ++parameter) {
			if(primary->parameters[parameter].default_type.empty()) break;
			const string default_argument = NormalizeTypeArgument(ReplaceIdentifiers(primary->parameters[parameter].default_type, default_substitutions));
			matching_arguments.push_back(default_argument);
			if(!primary->parameters[parameter].name.empty()) default_substitutions[primary->parameters[parameter].name] = default_argument;
		}
		// A concrete non-type argument can still arrive here as an expression
		// (`(I > 1)` after the enclosing substitution).  Normalize it before
		// matching explicit value specializations; their parameter lists retain
		// the source parameter but do not carry a second dependent expression
		// from which MatchClassSpecializationPattern can infer the value.
		for(size_t argument = 0; argument < matching_arguments.size() &&
			argument < primary->parameters.size(); ++argument) {
			const TemplateParameter& parameter = primary->parameters[argument];
			if(parameter.type || parameter.template_template || parameter.non_type_type.empty()) continue;
			PA19IntegralValue value;
			if(!const_cast<PA18TemplateExpander*>(this)->EvaluateIntegralText(
				matching_arguments[argument], context, map<string, string>(), &value) ||
				!value.known) continue;
			const PA19IntegralType expected_type = PA19Type(ResolveAlias(
				parameter.non_type_type, context));
			if(expected_type.integral) value = PA19Convert(value, expected_type);
			matching_arguments[argument] = TemplateIntegralValueSpelling(value);
		}
		vector<const TemplateDefinition*> matched;
		for(size_t i = 0; i < candidates.size(); ++i) {
			bool matches = false;
			try {
				matches = MatchClassSpecializationPattern(*candidates[i],
					matching_arguments, 0, context);
			} catch(const PA18SubstitutionFailure&) {
				matches = false;
			}
			if(matches)
				matched.push_back(candidates[i]);
		}
		if(matched.empty()) return primary;
		const TemplateDefinition* selected = 0;
		for(size_t i = 0; i < matched.size(); ++i) {
			bool dominated = false;
			for(size_t j = 0; j < matched.size(); ++j) {
				if(i == j) continue;
				if(ClassPartialMoreSpecialized(*matched[j], *matched[i], context)) {
					dominated = true;
					break;
				}
			}
			if(dominated) continue;
			if(selected)
				throw PA18SubstitutionFailure(
					"ambiguous class template partial specialization");
			selected = matched[i];
		}
		if(selected) {
			return selected;
		}
		throw PA18SubstitutionFailure(
			"cyclic class template partial specialization ordering");
	}
} // namespace
