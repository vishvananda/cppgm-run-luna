#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
using namespace std;
namespace pa18_templates_internal {
namespace {
bool ContainsIdentifierToken(const string& text, const string& identifier)
{
	if(identifier.empty()) return false;
	for(size_t at = 0; at < text.size();) {
		if(!IsIdentifierCharacter(text[at])) { ++at; continue; }
		const size_t begin = at;
		while(at < text.size() && IsIdentifierCharacter(text[at])) ++at;
		if(at - begin == identifier.size() &&
			text.compare(begin, identifier.size(), identifier) == 0) return true;
	}
	return false;
}
} // namespace
string PA18TemplateExpander::RewriteText(string raw, const string& context,
	const map<string, string>& substitutions, bool* template_replaced,
	bool resolve_alias, bool resolve_member, bool defer_class_definition)
{
	if(template_replaced) *template_replaced = false;
	const string source_spelling = raw;
	bool materialized_member_type = false;
	bool preserved_static_member = false;
	raw = NormalizeElaboratedSpelling(raw, context);
	if(!resolve_alias && resolve_member && active_instantiation_name_.empty() &&
		raw.find("::") == string::npos && raw.find('<') == string::npos &&
		raw.compare(0, 14, "TT_IDENTIFIER:") == 0) {
		const string member_name = RemoveMarker(raw);
		bool local_type_alias = false;
		for(string current = context; !current.empty() && !local_type_alias; ) {
			if(type_aliases_.find(JoinPath(current, member_name)) != type_aliases_.end())
				local_type_alias = true;
			const size_t separator = current.rfind("::");
			if(separator == string::npos) current.clear();
			else current.erase(separator);
		}
		map<string, string>::const_iterator local_alias = substitutions.find(member_name);
		if(local_type_alias && local_alias != substitutions.end()) return local_alias->second;
		for(string current = context; !current.empty(); ) {
			string member_type;
			set<string> active;
			FindClassMemberType(current, member_name, substitutions, context, &member_type,
				&active, true);
			if(member_type.empty()) member_type = MemberAliasType(current, member_name);
			if(!member_type.empty()) {
				raw = member_type;
				break;
			}
			const size_t separator = current.rfind("::");
			if(separator == string::npos) current.clear();
			else current.erase(separator);
		}
	}
	for(size_t qualifier = 0; qualifier < 2; ++qualifier) {
		const string word = qualifier == 0 ? "const" : "volatile";
		for(size_t at = raw.find(word); at != string::npos; ) {
			const size_t original_end = at + word.size();
			const bool begins_in_identifier = at > 0 &&
				IsIdentifierCharacter(raw[at - 1]);
			const bool ends_as_word = original_end == raw.size() ||
				!IsIdentifierCharacter(raw[original_end]);
			if(begins_in_identifier && ends_as_word) {
				size_t begin = at;
				while(begin > 0 && (IsIdentifierCharacter(raw[begin - 1]) ||
					raw[begin - 1] == ':')) --begin;
				const string prefix = CanonicalSpelling(raw.substr(begin, at - begin));
				if(IsKnownTypeSpelling(ReplaceIdentifiers(prefix, substitutions), context)) {
					raw.insert(at, " ");
					at = raw.find(word, original_end + 1);
					continue;
				}
			}
			at = raw.find(word, original_end);
		}
	}
	for(size_t template_marker = raw.find("::template ");
		template_marker != string::npos;
		template_marker = raw.find("::template ", template_marker))
		raw.erase(template_marker + 2, 9);
	for(map<string, vector<string> >::const_iterator active_pack =
		active_pack_substitutions_.begin(); active_pack != active_pack_substitutions_.end();
		++active_pack) {
		if(active_pack->first.empty()) continue;
		const string token = active_pack->first + "...";
		if(raw == token) continue;
		string expanded;
		for(size_t element = 0; element < active_pack->second.size(); ++element) {
			if(!expanded.empty()) expanded += ',';
			expanded += active_pack->second[element];
		}
		for(size_t at = raw.find(token); at != string::npos;) {
			raw.replace(at, token.size(), expanded);
			if(expanded.empty()) {
				if(at < raw.size() && raw[at] == ',') raw.erase(at, 1);
				else if(at > 0 && raw[at - 1] == ',') raw.erase(--at, 1);
			}
			at = raw.find(token, at + expanded.size());
		}
	}
	raw = CanonicalSpelling(RewriteActivePackSizes(raw));
		if(raw.compare(0, 8, "operator") == 0) {
			const string suffix = raw.substr(8);
			const string rewritten_suffix = ReplaceIdentifiers(suffix, substitutions);
			if(rewritten_suffix != suffix) {
				raw = "operator" + rewritten_suffix;
				if(template_replaced) *template_replaced = true;
			}
		}
	raw = RewriteDecltypeText(raw, context, substitutions, template_replaced);
	if(resolve_member) {
		const size_t owner_separator = TopLevelScopeSeparator(raw);
		if(owner_separator != string::npos && owner_separator + 2 < raw.size()) {
			const string owner = raw.substr(0, owner_separator);
			const string member = raw.substr(owner_separator + 2);
			const string resolved_owner = ResolveAlias(owner, context);
			if(!resolved_owner.empty() && resolved_owner != owner)
				raw = resolved_owner + "::" + member;
		}
	}
	if(resolve_member) for(map<string, string>::const_iterator current = substitutions.begin();
		current != substitutions.end(); ++current) {
		if(current->first.empty() || current->second.find('<') == string::npos) continue;
		const string marker = current->first + "::template";
		for(size_t at = raw.find(marker); at != string::npos; at = raw.find(marker, at + current->second.size())) {
			if(at > 0 && IsIdentifierCharacter(raw[at - 1])) continue;
			raw.replace(at, current->first.size(), current->second);
			const size_t owner_open = raw.find('<', at);
			if(owner_open == string::npos) break;
			string owner_base;
			size_t owner_begin = 0, owner_close = string::npos;
			string owner_arguments;
			if(!TemplateBase(raw, owner_open, &owner_begin, &owner_base) ||
				!TemplateRange(raw, owner_open, &owner_arguments, &owner_close)) break;
			if(RewriteConcreteNestedMember(&raw, owner_begin, owner_close, owner_base,
				context, substitutions, template_replaced, 0)) break;
		}
	}
	if(resolve_member) for(map<string, string>::const_iterator current = substitutions.begin();
		current != substitutions.end(); ++current) {
		if(current->first.empty() || current->second.empty() ||
			current->second.find('<') != string::npos) continue;
		map<string, string>::const_iterator base = specialization_bases_.find(
			LastComponent(current->second));
		map<string, vector<string> >::const_iterator arguments = specialization_arguments_.find(
			LastComponent(current->second));
		if(base == specialization_bases_.end() || arguments == specialization_arguments_.end()) continue;
		const TemplateDefinition* definition = FindDefinition(base->second, context);
		if(!definition || !definition->class_template) continue;
		const string token = current->first + "::";
		for(size_t at = raw.find(token); at != string::npos; at = raw.find(token, at)) {
			size_t end = at + token.size();
		while(end < raw.size() && IsIdentifierCharacter(raw[end])) ++end;
			if(end == at + token.size()) break;
			const string member = raw.substr(at + token.size(), end - at - token.size());
				string member_type;
				set<string> member_active;
				FindClassMemberType(current->second, member, substitutions, context,
					&member_type, &member_active, true);
				if(member_type.empty()) member_type = TemplateMemberType(*definition,
					arguments->second, member, context);
			if(member_type.empty()) {
				bool nested_class = false;
				if(definition->declaration) for(size_t child = 0;
					child < definition->declaration->children.size(); ++child) {
					const CPPGMAstNodePtr candidate = definition->declaration->children[child];
					if(!candidate || (candidate->kind != "class-specifier" &&
						candidate->kind != "class-forward-declaration") ||
						LastComponent(candidate->value) != member) continue;
					nested_class = true;
					break;
				}
					if(nested_class) {
					requested_nested_classes_[definition->qualified_name].insert(member);
					requested_nested_classes_[LastComponent(definition->qualified_name)].insert(member);
					InstantiateNestedClass(*definition, arguments->second,
						current->second, member, context);
				}
					break;
				}
				if(!member_type.empty()) { string member_context = context; map<string, string>::const_iterator owner = specialization_bases_.find(LastComponent(current->second));
					if(owner != specialization_bases_.end() && !PrefixComponent(owner->second).empty()) member_context = PrefixComponent(owner->second);
					member_type = QualifyTypeArgument(member_type, member_context, member_context, true); }
				size_t replacement_begin = at;
			size_t word_begin = at;
			while(word_begin > 0 && isspace(static_cast<unsigned char>(raw[word_begin - 1]))) --word_begin;
			if(word_begin >= 8 && raw.compare(word_begin - 8, 8, "typename") == 0 &&
				(word_begin == 8 || !IsIdentifierCharacter(raw[word_begin - 9])))
				replacement_begin = word_begin - 8;
			if(at >= 2 && raw.compare(at - 2, 2, "::") == 0) {
				size_t owner_begin = at - 2;
				int owner_angle = 0;
				while(owner_begin > 0) {
					const char ch = raw[owner_begin - 1];
					if(ch == '>') ++owner_angle;
					else if(ch == '<' && owner_angle > 0) --owner_angle;
					else if(owner_angle == 0 && (isspace(static_cast<unsigned char>(ch)) ||
						ch == ',' || ch == '(')) break;
					--owner_begin;
				}
				const string owner_spelling = raw.substr(owner_begin, at - 2 - owner_begin);
				bool materialized_owner = specialization_bases_.find(
					LastComponent(owner_spelling)) != specialization_bases_.end();
				if(!materialized_owner && owner_spelling.find('<') != string::npos) {
					string owner_base;
					size_t owner_begin_marker = 0;
					const size_t owner_open = owner_spelling.find('<');
					if(TemplateBase(owner_spelling, owner_open, &owner_begin_marker, &owner_base)) {
						const TemplateDefinition* owner_definition = FindDefinition(owner_base, context);
						materialized_owner = owner_definition && owner_definition->class_template;
					}
				}
				if(materialized_owner) {
					replacement_begin = owner_begin;
					while(replacement_begin > 0 &&
						isspace(static_cast<unsigned char>(raw[replacement_begin - 1]))) --replacement_begin;
					const size_t prefix_end = replacement_begin;
					size_t prefix_begin = prefix_end;
					while(prefix_begin > 0 && IsIdentifierCharacter(raw[prefix_begin - 1])) --prefix_begin;
					if(prefix_end > prefix_begin &&
						(raw.substr(prefix_begin, prefix_end - prefix_begin) == "const" ||
						 raw.substr(prefix_begin, prefix_end - prefix_begin) == "volatile"))
						replacement_begin = owner_begin;
					if(replacement_begin >= 8 && raw.compare(replacement_begin - 8, 8,
						"typename") == 0 && (replacement_begin == 8 ||
						!IsIdentifierCharacter(raw[replacement_begin - 9])))
						replacement_begin -= 8;
				}
			}
			raw.replace(replacement_begin, end - replacement_begin, member_type);
			materialized_member_type = true;
			at = replacement_begin + member_type.size();
		}
	}
		for(size_t search = 0; search < raw.size(); ++search) {
			if(raw[search] != '<') continue;
			size_t begin = 0;
			string base;
			if(!TemplateBase(raw, search, &begin, &base)) continue;
		string arguments_text;
		size_t close = string::npos;
		const bool has_range = TemplateRange(raw, search, &arguments_text, &close);
		if(!has_range) continue;
		vector<string> current_arguments = SplitTemplateArguments(arguments_text);
		if(!active_pack_substitutions_.empty()) {
			vector<string> expanded_current_arguments;
			for(size_t argument = 0; argument < current_arguments.size(); ++argument) {
				const string source_argument = CanonicalSpelling(current_arguments[argument]);
				if(source_argument.size() > 3 && source_argument.compare(
					source_argument.size() - 3, 3, "...") == 0) {
					const string pack_name = source_argument.substr(0,
						source_argument.size() - 3);
					map<string, vector<string> >::const_iterator pack =
						active_pack_substitutions_.find(pack_name);
					if(pack != active_pack_substitutions_.end()) {
						for(size_t value = 0; value < pack->second.size(); ++value)
							expanded_current_arguments.push_back(pack->second[value]);
						continue;
					}
				}
				expanded_current_arguments.push_back(current_arguments[argument]);
			}
			current_arguments.swap(expanded_current_arguments);
		}
		if(resolve_member && RewriteMemberTemplateAliasApplication(&raw, begin, close,
			base, context, substitutions, template_replaced, &search)) continue;
		bool replaced_current_specialization = false;
		map<string, vector<string> >::const_iterator generated_names =
			specialization_names_by_base_.find(LastComponent(base));
		if(generated_names != specialization_names_by_base_.end())
		for(size_t generated_index = 0; generated_index < generated_names->second.size();
			++generated_index) {
			const string& generated_name = generated_names->second[generated_index];
			map<string, string>::const_iterator generated_base =
				specialization_bases_.find(generated_name);
			map<string, vector<string> >::const_iterator generated =
				specialization_arguments_.find(generated_name);
			const bool generated_context =
				class_contexts_.find(generated_name) != class_contexts_.end() ||
				(current_arguments.empty() &&
				 class_contexts_.find(JoinPath(context, generated_name)) != class_contexts_.end());
			if(generated_base == specialization_bases_.end() ||
				!generated_context ||
				generated == specialization_arguments_.end() ||
				generated->second.size() != current_arguments.size()) continue;
			bool same_arguments = true;
			for(size_t argument = 0; argument < current_arguments.size(); ++argument) {
				const string actual = NormalizeTypeArgument(ReplaceIdentifiers(
					CanonicalSpelling(current_arguments[argument]), substitutions));
				const string expected = NormalizeTypeArgument(CanonicalSpelling(
					generated->second[argument]));
				if(actual != expected) {
					same_arguments = false;
					break;
				}
			}
			const bool qualified_member = close + 2 < raw.size() &&
				raw.compare(close + 1, 2, "::") == 0;
			if(same_arguments && !qualified_member) {
				raw.replace(begin, close - begin + 1, generated_name);
				if(template_replaced) *template_replaced = true;
				replaced_current_specialization = true;
				break;
			}
		}
		if(replaced_current_specialization) {
			search = begin + raw.size();
			continue;
		}
		map<string, string>::const_iterator current_substitution =
			substitutions.find(base);
		string current_name = current_substitution == substitutions.end() ? string() :
			current_substitution->second;
		if(current_name.empty() && !active_instantiation_name_.empty()) {
			map<string, string>::const_iterator active_base = specialization_bases_.find(
				LastComponent(active_instantiation_name_));
			if(active_base != specialization_bases_.end()) {
				string active_source = active_base->second;
				const size_t active_open = active_source.find('<');
				if(active_open != string::npos) active_source.erase(active_open);
				if(LastComponent(active_source) == LastComponent(base))
					current_name = LastComponent(active_instantiation_name_);
			}
		}
		if(!current_name.empty() && current_name.find('<') == string::npos) {
			bool current_class = class_contexts_.find(current_name) != class_contexts_.end();
			if(!current_class && !context.empty())
				current_class = class_contexts_.find(JoinPath(context, current_name)) !=
					class_contexts_.end();
			bool same_current_specialization = false;
			map<string, vector<string> >::const_iterator current_key =
				specialization_arguments_.find(LastComponent(current_name));
			if(current_class && current_key != specialization_arguments_.end() &&
				current_key->second.size() == current_arguments.size()) {
				map<string, string> current_bindings = substitutions;
				map<string, vector<string> > current_packs;
				map<string, string>::const_iterator current_base =
					specialization_bases_.find(LastComponent(current_name));
				const TemplateDefinition* current_primary = current_base ==
					specialization_bases_.end() ? 0 : FindDefinition(current_base->second, context);
				if(current_primary) for(size_t parameter = 0;
					parameter < current_primary->parameters.size() &&
					parameter < current_key->second.size(); ++parameter)
					if(!current_primary->parameters[parameter].name.empty())
						current_bindings[current_primary->parameters[parameter].name] =
							current_key->second[parameter];
				if(current_primary) {
					map<string, vector<TemplateDefinition> >::const_iterator partials =
						class_specializations_.find(current_primary->qualified_name);
					if(partials != class_specializations_.end()) {
						for(size_t partial = 0; partial < partials->second.size(); ++partial) {
							map<string, string> partial_bindings;
							bool matched_partial = false;
							try {
								matched_partial = MatchClassSpecializationPattern(
									partials->second[partial], current_key->second,
									&partial_bindings, context);
							} catch(const PA18SubstitutionFailure&) {
								matched_partial = false;
							}
							if(matched_partial) {
								for(map<string, string>::const_iterator binding = partial_bindings.begin();
									binding != partial_bindings.end(); ++binding)
									current_bindings[binding->first] = binding->second;
							}
							if(matched_partial) {
								for(size_t pack = 0; pack < partials->second[partial].specialization_pack_names.size(); ++pack) {
									const string& pack_name = partials->second[partial].specialization_pack_names[pack];
									map<string, string>::const_iterator binding = partial_bindings.find(pack_name);
									if(binding != partial_bindings.end() && !binding->second.empty())
										current_packs[pack_name] = SplitTemplateArguments(binding->second);
									else current_packs[pack_name] = vector<string>();
								}
							}
						}
					}
				}
				 same_current_specialization = true;
				for(size_t argument = 0; argument < current_arguments.size(); ++argument) {
					string source_argument = CanonicalSpelling(current_arguments[argument]);
					for(map<string, vector<string> >::const_iterator pack = current_packs.begin();
						pack != current_packs.end(); ++pack) {
						string expanded;
						for(size_t value = 0; value < pack->second.size(); ++value) {
							if(!expanded.empty()) expanded += ',';
							expanded += pack->second[value];
						}
						const string token = pack->first + "...";
						for(size_t at = source_argument.find(token); at != string::npos;
							at = source_argument.find(token, at + expanded.size()))
							source_argument.replace(at, token.size(), expanded);
					}
					const string actual = QualifyTypeArgument(NormalizeTypeArgument(
						ReplaceIdentifiers(source_argument, current_bindings)), context);
						const string expected = NormalizeTypeArgument(
							CanonicalSpelling(current_key->second[argument]));
						const bool same_spelling = actual == expected ||
							NormalizeTypeArgument(RestoreSpecializationSpelling(actual)) ==
							NormalizeTypeArgument(RestoreSpecializationSpelling(expected));
						if(!same_spelling) {
							same_current_specialization = false;
							break;
						}
				}
			}
			if(current_class && same_current_specialization) {
				raw.replace(begin, close - begin + 1, current_name);
				if(template_replaced) *template_replaced = true;
				search = begin + current_name.size();
				continue;
			}
		}
			const TemplateDefinition* definition = FindDefinition(base, context);
			string lookup_base = base;
			const TemplateDefinition* active_nested_parent = 0;
			vector<string> active_nested_parent_arguments;
			string inferred_nested_owner;
			map<string, vector<string> > inferred_nested_parent_packs;
			map<string, string>::const_iterator qualified_alias = substitutions.find(base);
			if(qualified_alias != substitutions.end() && !qualified_alias->second.empty()) {
				const TemplateDefinition* substituted_definition = FindDefinition(
					qualified_alias->second, context);
				if(qualified_alias->second.find("::") != string::npos ||
					(substituted_definition && (substituted_definition->class_template ||
						substituted_definition->alias_template ||
						substituted_definition->variable_template)))
					lookup_base = qualified_alias->second;
			}
			if(!definition) {
				const size_t separator = base.find("::");
				if(separator != string::npos) {
					const map<string, string>::const_iterator alias = substitutions.find(
						base.substr(0, separator));
					if(alias != substitutions.end()) {
						lookup_base = alias->second + base.substr(separator);
						definition = FindDefinition(lookup_base, context);
					}
				}
			}
			if(lookup_base != base) definition = FindDefinition(lookup_base, context);
			if(definition && !definition->class_template && !definition->alias_template &&
				!definition->variable_template) {
				const vector<const TemplateDefinition*> overloads = FindFunctionDefinitions(
					lookup_base, context);
				if(overloads.size() > 1) {
					const TemplateDefinition* viable_definition = SelectFunctionTemplateOverload(
						raw, lookup_base, current_arguments, context, substitutions, overloads);
					if(viable_definition) definition = viable_definition;
				}
			}
				if(base.rfind("::") != string::npos) {
					const size_t nested_separator = base.rfind("::");
					if(nested_separator != string::npos) {
						const string source_owner = base.substr(0, nested_separator);
						// source owner and enclosing non-type bindings are lost.
						string concrete_owner;
						map<string, string>::const_iterator owner_binding =
							substitutions.find(source_owner);
						if(owner_binding != substitutions.end())
							concrete_owner = owner_binding->second;
						else concrete_owner = ResolveAlias(source_owner, context);
						if(concrete_owner.empty()) concrete_owner = source_owner;
					map<string, string>::const_iterator generated_owner =
						specialization_bases_.find(LastComponent(concrete_owner));
					map<string, vector<string> >::const_iterator owner_arguments =
						specialization_arguments_.find(LastComponent(concrete_owner));
					if(generated_owner != specialization_bases_.end() &&
						owner_arguments != specialization_arguments_.end()) {
						string owner_source = generated_owner->second;
						const size_t owner_source_open = owner_source.find('<');
						if(owner_source_open != string::npos) owner_source.erase(owner_source_open);
						const TemplateDefinition* owner_definition = FindDefinition(
							owner_source, context);
							if(!owner_definition)
								owner_definition = FindDefinition(LastComponent(owner_source), context);
							if(owner_definition && owner_definition->class_template) {
								const TemplateDefinition* selected_owner = SelectClassTemplateDefinition(
									owner_definition, owner_arguments->second, context);
							if(selected_owner) {
								const TemplateDefinition* nested = FindNestedDefinition(*selected_owner,
									LastComponent(base));
								if(nested) {
									definition = nested;
									active_nested_parent = selected_owner;
									active_nested_parent_arguments = owner_arguments->second;
									inferred_nested_owner = concrete_owner;
									if(selected_owner->partial_specialization) {
										map<string, string> specialized;
										if(MatchClassSpecializationPattern(*selected_owner,
											active_nested_parent_arguments, &specialized, context))
											for(size_t pack = 0; pack < selected_owner->specialization_pack_names.size(); ++pack) {
												const string& name = selected_owner->specialization_pack_names[pack];
												map<string, string>::const_iterator binding = specialized.find(name);
												inferred_nested_parent_packs[name] = binding == specialized.end() ||
													binding->second.empty() ? vector<string>() :
													SplitTemplateArguments(binding->second);
											}
									} else {
										size_t parent_argument = 0;
										for(size_t parent_parameter = 0;
											parent_parameter < selected_owner->parameters.size(); ++parent_parameter) {
												const TemplateParameter& parameter = selected_owner->parameters[parent_parameter];
												if(parameter.pack) {
													vector<string>& values = inferred_nested_parent_packs[parameter.name];
													while(parent_argument < active_nested_parent_arguments.size())
														values.push_back(active_nested_parent_arguments[parent_argument++]);
												} else if(parent_argument < active_nested_parent_arguments.size()) ++parent_argument;
										}
									}
								}
							}
						}
					}
				}
			}
			// A member-template body is replayed with the concrete enclosing class as
			// its active owner.  An unqualified nested class template can therefore
			// have several source definitions with the same short name (one per
			// partial specialization), so ordinary name lookup is intentionally not
			// sufficient here.  Select the nested definition through the typed owner
			// specialization before materializing its own arguments.
			if(!definition && !active_concrete_owner_.name.empty() &&
				base.find("::") == string::npos) {
				const TemplateDefinition* owner_definition =
					active_concrete_owner_.definition;
				if(owner_definition && owner_definition->class_template) {
					const TemplateDefinition* selected_owner = SelectClassTemplateDefinition(
						owner_definition, active_concrete_owner_.arguments, context);
					if(selected_owner) {
						const TemplateDefinition* nested = FindNestedDefinition(*selected_owner,
							LastComponent(base));
						if(nested) {
							definition = nested;
							active_nested_parent = selected_owner;
							active_nested_parent_arguments = active_concrete_owner_.arguments;
						}
					}
				}
			}
			if(!definition) continue;
			vector<string> raw_template_args = SplitTemplateArguments(arguments_text);
				// A pack expansion inside a template-id can leave a synthetic
				// trailing empty component (`F<T, Pack...>` with an empty pack).
				// Remove that artifact at the point where the expansion is consumed;
				// the general argument splitter must continue preserving source
				// spellings for other parser paths.
			while(!raw_template_args.empty() && raw_template_args.back().empty())
				raw_template_args.pop_back();
		vector<string> args;
				bool deferred_pack_argument = false;
			for(size_t raw_argument = 0; raw_argument < raw_template_args.size(); ++raw_argument) {
				const string source_argument = CanonicalSpelling(raw_template_args[raw_argument]);
					if(source_argument.size() > 3 &&
					source_argument.compare(source_argument.size() - 3, 3, "...") == 0) {
					const string prefix = source_argument.substr(0, source_argument.size() - 3);
						vector<string> pack_names;
						bool known_pack = false;
						// A pack expansion such as `(sizeof...(I1) + I2)...`
						// mentions two packs, but only I2 is expanded by the
						// trailing ellipsis.  Do not let the identifier inside the
						// sizeof-pack operand select the wrong pack or get replaced
						// by the scalar first element.
						for(size_t character = 0; character < prefix.size();) {
							if(prefix.compare(character, 9, "sizeof...") == 0) {
								const size_t open = character + 9;
								if(open < prefix.size() && prefix[open] == '(') {
									int depth = 0;
									for(size_t skip = open; skip < prefix.size(); ++skip) {
										if(prefix[skip] == '(') ++depth;
										else if(prefix[skip] == ')' && --depth == 0) {
											character = skip + 1;
											break;
										}
									}
									if(character > open) continue;
								}
							}
							if(!IsIdentifierCharacter(prefix[character])) { ++character; continue; }
							const size_t begin_name = character;
						while(character < prefix.size() && IsIdentifierCharacter(prefix[character])) ++character;
						const string word = prefix.substr(begin_name, character - begin_name);
						if(active_pack_substitutions_.find(word) != active_pack_substitutions_.end()) {
							if(find(pack_names.begin(), pack_names.end(), word) == pack_names.end())
								pack_names.push_back(word);
							continue;
						}
						if(IsTemplatePackName(*definition, word)) known_pack = true;
					}
						if(!pack_names.empty()) {
							const vector<string>& first_pack = active_pack_substitutions_.find(
								pack_names[0])->second;
							for(size_t pack_index = 1; pack_index < pack_names.size(); ++pack_index)
								if(active_pack_substitutions_.find(pack_names[pack_index])->second.size() !=
									first_pack.size())
									throw PA18SubstitutionFailure("pack expansion length mismatch");
							for(size_t element = 0; element < first_pack.size(); ++element) {
								map<string, string> one = substitutions;
								for(size_t pack_index = 0; pack_index < pack_names.size(); ++pack_index)
									one[pack_names[pack_index]] = active_pack_substitutions_.find(
										pack_names[pack_index])->second[element];
								const string expanded_argument = CollapseReferenceSpelling(
									ReplaceIdentifiersPreservingPackSizes(prefix, one));
								// A pack binding can itself still be a dependent
								// expansion while a partial specialization is being
								// replayed (`Bytes` bound to `Bytes..., 7`).  Do not
								// turn that into a concrete nested template-id; leave
								// the whole use for the later concrete replay.
								if(expanded_argument.find("...") != string::npos) {
									deferred_pack_argument = true;
									break;
								}
								args.push_back(expanded_argument);
							}
							if(deferred_pack_argument) break;
						}
						continue;
					if(known_pack) {
						// This is a dependent template-id encountered while collecting
						// the source declaration.  There is no concrete pack to expand
						// yet; leave the spelling intact for the instantiation replay.
						deferred_pack_argument = true;
						break;
					}
					}
				args.push_back(source_argument);
					}
			if(deferred_pack_argument) {
				search = close + 1;
				continue;
			}
		for(size_t i = 0; i < args.size(); ++i) {
			const string source_argument = args[i]; const bool preserve_elaborated_type_owner = IsElaboratedTypeArgumentSpelling(source_argument);
			const string substituted_source_argument = CanonicalSpelling(CollapseRepeatedQualifiedPath(CollapseRepeatedQualifier(ReplaceIdentifiersPreservingPackSizes(source_argument, substitutions))));
			// A self-containing template-id replacement has no finite textual
			// rewrite.  Drop only that binding for this nested rewrite; all semantic
			// deduction facts remain in the caller's typed substitution map.
			const map<string, string>* argument_substitutions = &substitutions;
			map<string, string> protected_substitutions;
			for(map<string, string>::const_iterator substitution = substitutions.begin();
				substitution != substitutions.end(); ++substitution) {
				const string& replacement = substitution->second;
				if(replacement.find('<') == string::npos || replacement.find('>') == string::npos ||
					!ContainsIdentifierToken(source_argument, substitution->first) ||
					!ContainsIdentifierToken(replacement, substitution->first)) continue;
				if(argument_substitutions == &substitutions) {
					protected_substitutions = substitutions;
					argument_substitutions = &protected_substitutions;
				}
				protected_substitutions.erase(substitution->first);
			}
				const string rewrite_source = CanonicalSpelling(CollapseRepeatedQualifiedPath(CollapseRepeatedQualifier(argument_substitutions == &substitutions ? args[i] : substituted_source_argument)));
					if(i < definition->parameters.size() &&
						definition->parameters[i].template_template) {
						string normalized;
						if(!CompatibleTemplateTemplateArgument(definition->parameters[i], args[i],
							context, substitutions, &normalized))
							throw PA18SubstitutionFailure("template-template argument does not match");
					args[i] = normalized;
					continue;
				}
				// Normalize non-type arguments while their source expression and
				// active packs are still intact.  Rewriting a nested alias first
				// turns `P<T>::value...` into a generated first-element spelling
				// and loses the pack expansion before the typed evaluator can
				// replay it.
					if(i < definition->parameters.size() &&
						!definition->parameters[i].type) {
						const string dependent_expression = CanonicalSpelling(args[i]);
						const size_t sizeof_pack = dependent_expression.find("sizeof...");
						if(sizeof_pack != string::npos) {
							const size_t open = sizeof_pack + 9;
							const size_t close = dependent_expression.find(')', open);
							if(open < dependent_expression.size() &&
								dependent_expression[open] == '(' && close != string::npos) {
								const string pack_name = CanonicalSpelling(
									dependent_expression.substr(open + 1, close - open - 1));
								if(!pack_name.empty() &&
									active_pack_substitutions_.find(pack_name) ==
										active_pack_substitutions_.end() &&
									active_pack_identifier_substitutions_.find(pack_name) ==
										active_pack_identifier_substitutions_.end() &&
									template_pack_names_.find(pack_name) !=
										template_pack_names_.end()) {
									deferred_pack_argument = true;
					break; }
							}
						}
						bool unresolved_scope = !HasReplayContext(substitutions);
						for(map<string,string>::const_iterator scope = substitutions.begin();
							scope != substitutions.end() && !unresolved_scope; ++scope)
							if(scope->second.find("decltype") != string::npos ||
								scope->second.find("...") != string::npos ||
								HasDependentVariableTemplate(scope->second, context, substitutions))
								unresolved_scope = true;
						const bool bare_dependent_argument = !args[i].empty() &&
							(isalpha(static_cast<unsigned char>(args[i][0])) || args[i][0] == '_') &&
							args[i].find_first_not_of(
								"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") ==
								string::npos && args[i] != "true" && args[i] != "false" &&
							substitutions.find(args[i]) == substitutions.end() &&
								active_integral_substitutions_.find(args[i]) ==
									active_integral_substitutions_.end() &&
									variable_types_.find(args[i]) == variable_types_.end() && constant_values_.find(args[i]) == constant_values_.end() &&
									FindFunctionSignature(args[i], context) == 0;
						if(bare_dependent_argument) unresolved_scope = true;
							if(unresolved_scope &&
								substitutions.find(CanonicalSpelling(args[i])) == substitutions.end()) {
								bool dependent_name = false;
							for(size_t character = 0; character < args[i].size();) {
								if(!IsIdentifierCharacter(args[i][character])) {
									++character;
									continue;
								}
								const size_t begin_name = character;
								while(character < args[i].size() &&
									IsIdentifierCharacter(args[i][character])) ++character;
					const string word = args[i].substr(begin_name,
						character - begin_name);
					bool resolved_identifier = false;
					map<string, string>::const_iterator substituted_word = substitutions.find(word);
					if(substituted_word != substitutions.end()) {
						const string& value = substituted_word->second;
						resolved_identifier = value.find("decltype") == string::npos &&
							value.find("...") == string::npos &&
							!HasDependentVariableTemplate(value, context, substitutions);
					}
					map<string, PA19IntegralValue>::const_iterator integral_word =
						active_integral_substitutions_.find(word);
					if(integral_word != active_integral_substitutions_.end() &&
						integral_word->second.known) resolved_identifier = true;
					if(resolved_identifier) continue;
					bool lexical_parameter = false;
								for(map<string, TemplateDefinition>::const_iterator candidate = definitions_.begin();
									candidate != definitions_.end() && !lexical_parameter; ++candidate) {
									const string& qualified = candidate->second.qualified_name;
									const bool in_context = context == qualified ||
										(context.size() > qualified.size() + 2 &&
										 context.compare(0, qualified.size(), qualified) == 0 &&
										 context[qualified.size()] == ':' &&
										 context[qualified.size() + 1] == ':');
									if(!in_context) continue;
									for(size_t parameter = 0; parameter < candidate->second.parameters.size(); ++parameter)
										if(candidate->second.parameters[parameter].name == word) {
											lexical_parameter = true;
											break;
										}
								}
					if(word != "true" && word != "false" && word != "sizeof" &&
						lexical_parameter) {
									dependent_name = true;
									break;
									}
								}
								// A bare, otherwise unknown identifier in a non-type
								// argument is a dependent template parameter while a
								// declaration is being replayed.  Do not try to
								// materialize the enclosing template-id with that
								// spelling (`function_action<I, Ret>`); the concrete
								// substitution will be installed when the partial
								// specialization is selected.
				if(!dependent_name && !args[i].empty() &&
					(isalpha(static_cast<unsigned char>(args[i][0])) || args[i][0] == '_') &&
					args[i].find_first_not_of(
					"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") ==
					string::npos && args[i] != "true" && args[i] != "false" &&
							constant_values_.find(args[i]) == constant_values_.end() &&
							active_integral_substitutions_.find(args[i]) ==
							active_integral_substitutions_.end() &&
							variable_types_.find(args[i]) == variable_types_.end() &&
							FindFunctionSignature(args[i], context) == 0)
					dependent_name = true;
					// A nested template-id can carry a member-function parameter
					// (`accepts<Args&&...>::value`) without exposing that name as
					// the whole argument.  Consult the typed template-parameter index
					// before attempting integral evaluation of the dependent member.
					if(!dependent_name && HasUnresolvedTemplateParameter(args[i], context,
						substitutions)) dependent_name = true;
					// A constexpr member call in a class-template body is dependent even
					// when the call's spelling is not itself a template parameter.  Defer
					// it until the enclosing class arguments are installed; otherwise the
					// primary body tries to evaluate `enabled()` with no binding for `T`.
					if(!dependent_name && unresolved_scope) {
						const size_t call_open = args[i].find('(');
					if(call_open != string::npos) {
						const string callee = LastComponent(args[i].substr(0, call_open));
						for(string current = context; !current.empty() && !dependent_name; ) {
							const TemplateDefinition* owner = FindDefinition(current, context);
							if(owner && owner->class_template && owner->declaration)
								for(size_t child = 0; child < owner->declaration->children.size(); ++child) {
									const CPPGMAstNodePtr member = owner->declaration->children[child];
									if(member && member->kind == "function-definition" &&
										member->children.size() > 1 &&
										LastComponent(FirstIdentifierLocal(member->children[1])) == callee) {
										dependent_name = true;
										break;
									}
								}
							if(dependent_name) break;
							const size_t separator = current.rfind("::");
							if(separator == string::npos) current.clear();
							else current.erase(separator);
						}
						if(!dependent_name && context.empty())
							for(map<string, TemplateDefinition>::const_iterator candidate = definitions_.begin();
								candidate != definitions_.end() && !dependent_name; ++candidate) {
								if(!candidate->second.class_template || !candidate->second.declaration) continue;
								for(size_t child = 0; child < candidate->second.declaration->children.size(); ++child) {
									const CPPGMAstNodePtr member = candidate->second.declaration->children[child];
									if(member && member->kind == "function-definition" &&
										member->children.size() > 1 &&
										LastComponent(FirstIdentifierLocal(member->children[1])) == callee) {
										dependent_name = true;
										break;
									}
								}
							}
					}
					}
					if(dependent_name) {
								deferred_pack_argument = true;
								break;
							}
					}
					// The surrounding replay context can already be concrete while a
					// member-function template's non-type default still contains its
					// own parameter pack.  The lexical scan above is conditional on
					// unresolved class replay, so apply the typed dependency boundary
					// after that scope before integral evaluation as well.
					if(HasUnresolvedTemplateParameter(args[i], context, substitutions)) {
						deferred_pack_argument = true;
						break;
					}
					PA19IntegralValue value;
				try {
					args[i] = ResolveIntegralArgument(definition->parameters[i],
						args[i], context, substitutions, &value);
				} catch(const PA18SubstitutionFailure& error) {
					throw PA18SubstitutionFailure("definition=" + definition->qualified_name +
								" " + error.what());
						} catch(const logic_error& error) {
							throw logic_error("definition=" + definition->qualified_name +
								" " + error.what());
						}
						continue;
				}
					// Failure while forming an alias argument is the substitution
					// boundary itself.  In particular, `void_t<Op<T>>` must reject
					// the partial specialization when `Op<T>` is invalid; turning
					// the failed operand into `void` would incorrectly select it.
					const bool defer_nested_class_argument = defer_class_definition ||
						(i < definition->parameters.size() && definition->parameters[i].type &&
							source_argument != substituted_source_argument);
					args[i] = NormalizeTypeArgument(RewriteText(rewrite_source, context,
							*argument_substitutions, 0, true, true, defer_nested_class_argument));
					// The first rewrite is a typed substitution boundary.  If a source
					// parameter was materialized to a class whose spelling is also an
					// enclosing parameter name (`Property -> Vertex`, alongside
					// `Vertex -> unsigned long`), a second scalar pass must not rewrite
					// the class result again.  Preserve only names introduced by a
					// substitution present in this source argument; a source-level use
					// of `Vertex` itself still follows the ordinary outer binding.
					map<string, string> second_pass_substitutions = *argument_substitutions;
					for(map<string, string>::const_iterator substitution =
						argument_substitutions->begin(); substitution != argument_substitutions->end();
						++substitution) {
						if(substitution->first.empty() || substitution->second.empty() ||
							!ContainsIdentifierToken(source_argument, substitution->first) ||
							substitution->second.find_first_not_of(
								"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != string::npos)
							continue;
						const map<string, string>::const_iterator introduced =
							argument_substitutions->find(substitution->second);
						const bool introduced_collision = introduced != argument_substitutions->end() &&
							introduced->second != introduced->first &&
							!ContainsIdentifierToken(source_argument, substitution->second);
						if(class_contexts_.find(substitution->second) != class_contexts_.end() ||
							FindClassDeclaration(substitution->second, context) || introduced_collision)
							second_pass_substitutions.erase(substitution->second);
					}
					args[i] = CollapseReferenceSpelling(ReplaceIdentifiers(args[i],
						second_pass_substitutions));
				// Keep a typedef spelling for a pointer to a function type while
				// selecting a class partial specialization.  Expanding
				// `formatter_function*` to `string_like*` before matching
				// `Wrapper<Formatter*>` loses the complete function type and
				// incorrectly binds Formatter to its return type.
				const auto function_pointer_alias_spelling = [this, &context](
					const string& spelling) {
				string result;
				const string canonical = CanonicalSpelling(spelling);
				if(canonical.empty() || canonical[canonical.size() - 1] != '*') return result;
				const string pointee = CanonicalSpelling(canonical.substr(0, canonical.size() - 1));
					const string resolved_pointee = CanonicalSpelling(ResolveAlias(pointee, context));
					string direct_result;
					vector<string> direct_parameters;
					string direct_qualifiers;
					if(SplitDirectFunctionType(resolved_pointee, &direct_result,
						&direct_parameters, &direct_qualifiers) ||
						SplitFunctionPointerType(resolved_pointee, &direct_result,
							&direct_parameters))
						result = pointee + "*";
				return result;
				};
				string function_pointer_alias = function_pointer_alias_spelling(source_argument);
				if(function_pointer_alias.empty())
					function_pointer_alias = function_pointer_alias_spelling(substituted_source_argument);
				if(!function_pointer_alias.empty()) args[i] = function_pointer_alias;
				else args[i] = ResolveAlias(args[i], context);
						args[i] = NormalizeTypeArgument(RewriteText(rewrite_source, context,
							*argument_substitutions, 0, true, true, defer_nested_class_argument));
					if(function_pointer_alias.empty()) args[i] = ResolveAlias(args[i], context);
					else args[i] = function_pointer_alias;
					args[i] = QualifyTypeArgument(args[i], context, definition->owner,
						preserve_elaborated_type_owner);
					// Preserve a typedef spelling that denotes a reference while
					// replaying an alias template.  Substituting its expanded
					// `int&` spelling into `const T` would incorrectly turn the
					// source-level `const Alias` into `const int&`; C++ ignores
					// that top-level cv on the reference alias.
					if(definition->alias_template && i < definition->parameters.size() &&
						definition->parameters[i].type &&
						!source_argument.empty() &&
						!ResolveAlias(source_argument, context).empty() &&
						ResolveAlias(source_argument, context).back() == '&')
						args[i] = source_argument;
				}
			if(deferred_pack_argument) { search = close + 1;
					continue;
				}
				const size_t supplied_template_arguments = args.size();
				bool default_substitution_failure = false;
				if(args.size() < definition->parameters.size()) {
				map<string, string> default_substitutions = substitutions;
				for(size_t i = 0; i < args.size() && i < definition->parameters.size(); ++i)
					if(!definition->parameters[i].name.empty())
						default_substitutions[definition->parameters[i].name] = args[i];
				for(size_t i = args.size(); i < definition->parameters.size(); ++i) {
					// A parameter pack may have an empty concrete binding.  Its
					// scalar substitution is only the first pack element and must
					// not be reused as a synthesized default argument (`F<>` would
					// otherwise become `F<first>` during replay).
					if(definition->parameters[i].pack) continue;
					string argument;
					// An omitted argument is supplied by this template's own default,
					// not by an unrelated caller binding with the same spelling.  For
					// example, the `T` in `enable_if_t<B>` must use its declared
					// `void` default even when the enclosing function also has `T`.
					if(!definition->parameters[i].default_type.empty()) {
						try {
							argument = RewriteText(definition->parameters[i].default_type, context,
								default_substitutions, 0);
						} catch(const PA18SubstitutionFailure&) {
							// A dependent default template argument is part of the
							// candidate's substitution context.  Leave this template-id
							// for call resolution, where the candidate can be discarded
							// and an overload/fallback can continue.
							argument.clear();
							default_substitution_failure = true;
							break;
						}
					}
					if(argument.empty()) break;
					argument = NormalizeTypeArgument(ReplaceIdentifiers(argument,
						default_substitutions));
					argument = ResolveAlias(argument, context);
					if(i < definition->parameters.size() &&
						definition->parameters[i].template_template) {
						string normalized;
						if(!CompatibleTemplateTemplateArgument(definition->parameters[i], argument,
							context, default_substitutions, &normalized))
						throw PA18SubstitutionFailure("template-template argument does not match");
						argument = normalized;
					}
					argument = QualifyTypeArgument(argument, context, definition->owner);
					args.push_back(argument);
					if(!definition->parameters[i].name.empty())
						default_substitutions[definition->parameters[i].name] = argument;
					}
				}
					if(default_substitution_failure) {
						// Keep an explicit function-template-id alive long enough for
						// overload replay to try the next declaration.  A failed default
						// is SFINAE on this candidate, not a failure of the whole call.
						args.resize(supplied_template_arguments);
						if(definition->class_template || definition->alias_template ||
							definition->variable_template) {
							search = close + 1;
							continue;
						}
					}
				bool missing_required_template_argument = false;
				for(size_t i = args.size(); i < definition->parameters.size(); ++i)
					if(!definition->parameters[i].pack &&
						definition->parameters[i].default_type.empty()) {
						missing_required_template_argument = true;
						break;
					}
				if(missing_required_template_argument) {
					search = close + 1;
					continue;
				}
				// A type template-id may be encountered while forming the signature of
				// another dependent call.  Its argument is not a concrete type until the
				// enclosing replay installs the bindings; materializing it here would
				// register a bogus specialization such as `view<T>` in the source pass.
				// Defer only when the typed argument still contains an indexed template
				// parameter; unknown-but-concrete local classes remain materializable.
				bool unresolved_type_argument = false;
				for(size_t i = 0; i < args.size() && i < definition->parameters.size(); ++i) {
					if(!definition->parameters[i].type) continue;
					const bool concrete_function_type =
						SplitDirectFunctionType(args[i], 0, 0, 0) ||
						SplitFunctionPointerType(args[i], 0, 0);
					if(((args[i].find("...") != string::npos && !concrete_function_type) ||
						HasUnresolvedTemplateParameter(args[i], context, substitutions))) {
						unresolved_type_argument = true;
						break;
					}
				}
					if(unresolved_type_argument) {
					search = close + 1;
					continue;
				}
				// An explicit function-template prefix is not a complete function
				// specialization.  The remaining parameters are deduced from the call
				// argument (including a type pack such as `get<0>(tuple<T...>&)`), so
				// preserve the template-id for the call replay instead of registering
				// a declaration with only the explicit prefix bound.
			const bool unresolved_template_arguments = HasUnresolvedTemplateParameter(
				arguments_text, context, substitutions);
				if(!default_substitution_failure && !definition->class_template &&
					!definition->alias_template &&
				!definition->variable_template && args.size() < definition->parameters.size()) {
					search = close + 1;
					continue;
				}
			// Keep an alias/variable template with an unresolved pack as a deduction
			// pattern.  Materializing its target with an empty pack changes
			// `index_sequence<I...>` into a concrete zero-element class and loses the
			// non-type pack before the actual argument is matched.  Omitted concrete
			// defaults still take the normal alias-expansion path.
			if((definition->alias_template || definition->variable_template) &&
				args.size() < definition->parameters.size() && unresolved_template_arguments) {
				search = close + 1;
				continue;
			}
				// Resolve non-type arguments in the surrounding substitution scope
			// before Instantiate creates its fresh local map.  This preserves
			// member constants such as `num` while materializing a nested
			// specialization inside `ratio1<N>`.
			map<string, string> argument_substitutions = substitutions;
			for(size_t i = 0; i < args.size() && i < definition->parameters.size(); ++i) {
				if(!definition->parameters[i].type) {
					PA19IntegralValue value;
					try {
						args[i] = ResolveIntegralArgument(definition->parameters[i], args[i],
							context, argument_substitutions, &value);
					} catch(const PA18SubstitutionFailure& error) {
						throw PA18SubstitutionFailure("definition=" + definition->qualified_name +
							" " + error.what());
					} catch(const logic_error& error) {
						throw logic_error("definition=" + definition->qualified_name +
							" " + error.what());
					}
				}
				if(!definition->parameters[i].name.empty())
					argument_substitutions[definition->parameters[i].name] = args[i];
			}
				const TemplateDefinition* selected_definition = SelectClassTemplateDefinition(
					definition, args, context);
				definition = selected_definition;
			// Resolve a concrete nested owner before the generic member lookup so
			// dependent outer packs remain represented by the materialized class.
			const bool dependent_nested_member = close + 2 < raw.size() &&
				raw.compare(close + 1, 2, "::") == 0 &&
				raw.find("::template", close + 1) != string::npos;
				if((resolve_member || dependent_nested_member) && definition->class_template &&
					inferred_nested_owner.empty() &&
					close + 2 < raw.size() && raw.compare(close + 1, 2, "::") == 0) {
					bool nested_rewritten = false;
					try {
						nested_rewritten = RewriteConcreteNestedMember(
							&raw, begin, close, base, context, substitutions, template_replaced, &search);
					} catch(const PA18SubstitutionFailure&) {
						nested_rewritten = false;
					}
					if(nested_rewritten) continue;
				}
				bool resolved_template_member = false;
				if(resolve_member && definition->class_template && inferred_nested_owner.empty() &&
					close + 2 < raw.size() && raw.compare(close + 1, 2, "::") == 0) {
					try {
						resolved_template_member = RewriteResolvedTemplateMember(
							&raw, begin, close, context, substitutions, definition, args,
							template_replaced, &search);
					} catch(const PA18SubstitutionFailure&) {
						resolved_template_member = false;
					}
				}
				if(resolved_template_member) continue;
			map<string, string> instantiation_substitutions = substitutions;
			if(active_nested_parent) {
				for(size_t parameter = 0; parameter < active_nested_parent->parameters.size() &&
					parameter < active_nested_parent_arguments.size(); ++parameter)
					if(!active_nested_parent->parameters[parameter].name.empty())
						instantiation_substitutions[active_nested_parent->parameters[parameter].name] =
							active_nested_parent_arguments[parameter];
				if(!active_nested_parent->name.empty())
					instantiation_substitutions[active_nested_parent->name] =
						active_concrete_owner_.name;
			}
				string concrete_owner_for_instantiation;
			const size_t base_separator = base.rfind("::");
			if(base_separator != string::npos) {
				const string concrete_owner = base.substr(0, base_separator);
				if(class_contexts_.find(concrete_owner) != class_contexts_.end() &&
					specialization_bases_.find(LastComponent(concrete_owner)) !=
					specialization_bases_.end())
					concrete_owner_for_instantiation = concrete_owner;
			}
			// Once an enclosing dependent owner has already been materialized, a
			// later replay can see `owner_X::member_template<...>` instead of the
			// original `Owner<T>::member_template<...>` spelling.  Preserve that
			// concrete owner as typed state so a member partial specialization is
			// emitted inside the enclosing class rather than into its source owner.
				if(begin >= 2 && raw.compare(begin - 2, 2, "::") == 0) {
				size_t owner_end = begin - 2;
				size_t owner_begin = owner_end;
				while(owner_begin > 0 && IsIdentifierCharacter(raw[owner_begin - 1])) --owner_begin;
				const string concrete_owner = raw.substr(owner_begin, owner_end - owner_begin);
				if(class_contexts_.find(concrete_owner) != class_contexts_.end() &&
					specialization_bases_.find(LastComponent(concrete_owner)) !=
					specialization_bases_.end())
					concrete_owner_for_instantiation = concrete_owner;
			}
			// When a member alias is reached through a generated enclosing class,
			// the source member definition has only its own template parameters.
			// Reconstruct the outer class's typed bindings from the generated owner
			// before replaying the alias body; otherwise `Allocator` (and friends)
			// remains unresolved and the generated alias is registered under the
			// source owner instead of the concrete specialization.
			const string concrete_owner = PrefixComponent(lookup_base);
			map<string, string>::const_iterator concrete_base =
				specialization_bases_.find(LastComponent(concrete_owner));
			if(!concrete_owner.empty() && concrete_base != specialization_bases_.end() &&
				LastComponent(concrete_base->second) == LastComponent(definition->owner)) {
				map<string, vector<string> >::const_iterator concrete_arguments =
					specialization_arguments_.find(LastComponent(concrete_owner));
				const TemplateDefinition* owner_definition = FindDefinition(
					concrete_base->second, context);
				if(concrete_arguments != specialization_arguments_.end() && owner_definition) {
					for(size_t parameter = 0; parameter < owner_definition->parameters.size() &&
						parameter < concrete_arguments->second.size(); ++parameter)
						if(!owner_definition->parameters[parameter].name.empty())
							instantiation_substitutions[owner_definition->parameters[parameter].name] =
								concrete_arguments->second[parameter];
						concrete_owner_for_instantiation = concrete_owner;
				}
			}
				if(!inferred_nested_owner.empty())
					concrete_owner_for_instantiation = inferred_nested_owner;
				vector<const TemplateDefinition*> instantiation_candidates;
				if(definition && !definition->class_template && !definition->alias_template &&
					!definition->variable_template) {
					// The call-argument probe above has already selected the viable
					// overload.  Keep that typed decision first when replaying the
					// function body; the registry order is source-registration order,
					// not overload ranking order, and a variadic tag-dispatch candidate
					// would otherwise be skipped or replaced by a later fallback.
					bool definition_has_pack = false;
					for(size_t parameter = 0; parameter < definition->parameters.size(); ++parameter)
						if(definition->parameters[parameter].pack) definition_has_pack = true;
					if(args.size() <= definition->parameters.size() || definition_has_pack)
						instantiation_candidates.push_back(definition);
					const vector<const TemplateDefinition*> overloads = FindFunctionDefinitions(
						base, context);
					for(size_t overload = 0; overload < overloads.size(); ++overload) {
						if(!overloads[overload] || overloads[overload]->class_template ||
							overloads[overload]->alias_template ||
							overloads[overload]->variable_template ||
							find(instantiation_candidates.begin(), instantiation_candidates.end(),
								overloads[overload]) != instantiation_candidates.end()) continue;
						bool candidate_has_pack = false;
						for(size_t parameter = 0; parameter < overloads[overload]->parameters.size(); ++parameter)
							if(overloads[overload]->parameters[parameter].pack) candidate_has_pack = true;
						if(args.size() > overloads[overload]->parameters.size() && !candidate_has_pack) continue;
							if(find(instantiation_candidates.begin(), instantiation_candidates.end(),
								overloads[overload]) == instantiation_candidates.end())
								instantiation_candidates.push_back(overloads[overload]);
					}
					if(default_substitution_failure) {
						// The compact registry can give same-spelled function templates
						// the same qualified name while retaining distinct declaration
						// keys.  Only use the expanded declaration set after a dependent
						// default has actually failed; doing this for every function call
						// would replay recursive probes more than once.
						const string base_owner = PrefixComponent(base);
						for(map<string, TemplateDefinition>::const_iterator candidate = definitions_.begin();
							candidate != definitions_.end(); ++candidate) {
							if(candidate->second.class_template || candidate->second.alias_template ||
								candidate->second.variable_template || candidate->second.name != LastComponent(base) ||
								(!base_owner.empty() && candidate->second.owner != base_owner) ||
								(base_owner.empty() && !candidate->second.owner.empty()) ||
								args.size() > candidate->second.parameters.size()) continue;
							if(find(instantiation_candidates.begin(), instantiation_candidates.end(),
								&candidate->second) == instantiation_candidates.end())
								instantiation_candidates.push_back(&candidate->second);
						}
					}
				}
			if(instantiation_candidates.empty()) instantiation_candidates.push_back(definition);
			const ConcreteOwnerContext previous_concrete_owner = active_concrete_owner_;
			if(!concrete_owner_for_instantiation.empty())
				SetActiveConcreteOwner(concrete_owner_for_instantiation, context);
			string requested_owner_name = active_concrete_owner_.name;
			if(requested_owner_name.empty() && definition->member_template &&
				!active_instantiation_name_.empty())
				requested_owner_name = active_instantiation_name_;
			const string* requested_owner = requested_owner_name.empty() ? 0 :
				&requested_owner_name;
				string local_name;
					try {
							string last_candidate_failure;
							bool selected_candidate = false;
								for(size_t candidate = 0; candidate < instantiation_candidates.size(); ++candidate) {
								try {
									// Validate the substituted function result before replaying
									// the body.  A non-type result such as `sizeof(T)` is a
									// substitution boundary; instantiating it first can recurse
									// through the same unevaluated call instead of selecting the
									// fallback overload.
									const TemplateDefinition* candidate_definition =
										instantiation_candidates[candidate];
									CPPGMAstNodePtr candidate_declaration = candidate_definition->declaration;
								while(candidate_declaration && candidate_declaration->kind == "template-declaration" &&
									candidate_declaration->children.size() > 1)
									candidate_declaration = candidate_declaration->children[1];
									const bool named_constructor = !candidate_definition->owner.empty() &&
										LastComponent(candidate_definition->name) ==
										LastComponent(candidate_definition->owner);
									const bool constructor_candidate = named_constructor ||
										(candidate_declaration &&
											(candidate_declaration->kind == "special-member-definition" ||
											 candidate_declaration->kind == "special-member-declaration"));
									// A class-template rewrite reaches this loop as well when a
									// class object is formed without a function overload.  It has
									// no function result type to substitute; only function
									// candidates need the expression-SFINAE result check.
									const bool type_candidate = candidate_definition->class_template ||
										candidate_definition->alias_template || candidate_definition->variable_template;
									const string candidate_result = constructor_candidate || type_candidate ? "candidate" :
										FunctionResultType(*candidate_definition, args, context,
											&instantiation_substitutions);
						if(candidate_result.empty())
										throw PA18SubstitutionFailure("function result substitution failed");
									local_name = Instantiate(*instantiation_candidates[candidate], args, context, false,
									inferred_nested_parent_packs.empty() ? 0 : &inferred_nested_parent_packs,
									&instantiation_substitutions, requested_owner, 0, 0,
									defer_class_definition);
									definition = instantiation_candidates[candidate];
							selected_candidate = true;
							break;
							} catch(const PA18SubstitutionFailure& error) {
							last_candidate_failure = error.what();
								}
							}
							if(!selected_candidate)
								throw PA18SubstitutionFailure(last_candidate_failure.empty() ?
									"function template overload substitution failed" : last_candidate_failure);
						} catch(const PA18SubstitutionFailure&) {
							active_concrete_owner_ = previous_concrete_owner;
							throw;
						} catch(const logic_error&) {
							active_concrete_owner_ = previous_concrete_owner;
							throw;
						} catch(...) {
							active_concrete_owner_ = previous_concrete_owner;
							throw;
						}
			active_concrete_owner_ = previous_concrete_owner;
			// A bare class template-id in a declaration type (notably a typedef)
			// does not instantiate the class definition.  Member-qualified uses do
			// require the concrete class and are the source-order event relevant to
			// a later explicit specialization.
			if(resolve_member && definition->class_template && close + 2 < raw.size() &&
				raw.compare(close + 1, 2, "::") == 0) {
				instantiated_class_specializations_.insert(
					MakeClassSpecializationIdentity(*definition, args, context));
			}
		string replacement = local_name;
		const string qualifier = PrefixComponent(lookup_base);
		// A template found through a using-declaration may be spelled without
		// its declaring namespace (int_ in boost::mpl::aux).  The generated
		// specialization lives in that namespace, so retain the typed owner in
		// the replacement.  Unnamed namespaces already carry their physical
		// identity in the generated name and must not be qualified again.
		string generated_qualifier = qualifier;
		const string physical_owner = definition->lexical_owner.empty() ?
			definition->owner : definition->lexical_owner;
		map<string, string>::const_iterator inline_owner =
			lexical_namespace_logical_.find(physical_owner);
		if(!qualifier.empty() && inline_owner != lexical_namespace_logical_.end() &&
			inline_owner->second == qualifier)
			generated_qualifier = physical_owner;
		if(generated_qualifier.empty() && !definition->owner.empty() &&
			definition->owner.find("<unnamed>") == string::npos &&
			class_contexts_.find(definition->owner) == class_contexts_.end())
			generated_qualifier = definition->owner;
		if(!generated_qualifier.empty()) {
			replacement = generated_qualifier + "::" + local_name;
		}
		if(definition->alias_template) {
				// Alias-template instantiation is a type substitution, not a new
				// nominal type.  Keep the generated alias declaration registered for
				// lookup, but feed its rewritten target into the surrounding template
				// argument (otherwise `same<T,T>` sees `f_X_` and `X` as different
				// types even though the former aliases the latter).
				const string concrete_target = ResolveAlias(replacement, context);
				if(concrete_target != replacement) replacement = concrete_target;
				else {
					for(map<string, string>::const_iterator substitution = substitutions.begin();
						substitution != substitutions.end() && replacement == local_name; ++substitution) {
						const size_t separator = substitution->second.rfind("::");
						if(separator == string::npos) continue;
						const string owner = substitution->second.substr(0, separator);
						if(specialization_bases_.find(LastComponent(owner)) ==
							specialization_bases_.end()) continue;
						const string concrete_alias = JoinPath(owner, local_name);
						const string concrete_value = ResolveAlias(concrete_alias, context);
						if(concrete_value != concrete_alias) replacement = concrete_value;
					}
				}
					if(!definition->owner.empty() || !definition->lexical_owner.empty()) {
						const string source_owner = definition->lexical_owner.empty() ?
							definition->owner : definition->lexical_owner;
						const string source_alias = JoinPath(source_owner, local_name);
						const string source_target = ResolveAlias(source_alias, context);
						if(source_target != source_alias) replacement = source_target;
						else if(source_owner != definition->owner && !definition->owner.empty()) {
							const string qualified_source_alias = JoinPath(definition->owner, local_name);
							const string qualified_source_target = ResolveAlias(
								qualified_source_alias, context);
							if(qualified_source_target != qualified_source_alias)
								replacement = qualified_source_target;
						}
					}
		}
		if(close + 1 < raw.size() && IsIdentifierCharacter(raw[close + 1]) &&
				!replacement.empty() && IsIdentifierCharacter(replacement[replacement.size() - 1]))
				replacement += ' ';
			raw.replace(begin, close - begin + 1, replacement);
			if(template_replaced) *template_replaced = true;
			search = begin + replacement.size();
		}
	map<string, string> final_substitutions = substitutions;
	ProtectMaterializedSubstitutions(source_spelling, raw, context, substitutions,
		materialized_member_type, &final_substitutions);
	raw = ReplaceIdentifiersPreservingPackSizes(raw, final_substitutions);
	// A concrete generated owner can appear without its source template-id after
	// an earlier member substitution (`list2<...>::child0` -> `expr_X::member`).
	// Resolve those typed member aliases in a second pass so a chain such as
	// `Args::child0::proto_grammar` is reduced from the inside out.
	const auto next_scope_separator = [this](const string& text, size_t start) {
		int angle = 0;
		for(size_t position = 0; position + 1 < text.size(); ++position) {
			if(text[position] == '<' && IsTemplateAngleOpen(text, position)) ++angle;
			else if(text[position] == '>' && angle > 0 && IsTemplateAngleClose(text, position)) --angle;
			if(position >= start && angle == 0 && text.compare(position, 2, "::") == 0)
				return position;
		}
		return string::npos;
	};
	if(resolve_member) for(size_t separator = next_scope_separator(raw, 0);
		separator != string::npos; ) {
		size_t member_begin = separator + 2;
		while(member_begin < raw.size() && isspace(static_cast<unsigned char>(raw[member_begin])))
			++member_begin;
		if(member_begin >= raw.size() || !IsIdentifierCharacter(raw[member_begin])) {
			separator = next_scope_separator(raw, separator + 2);
			continue;
		}
		size_t member_end = member_begin + 1;
		while(member_end < raw.size() && IsIdentifierCharacter(raw[member_end])) ++member_end;
		size_t owner_end = separator;
		while(owner_end > 0 && isspace(static_cast<unsigned char>(raw[owner_end - 1]))) --owner_end;
		while(owner_end > 0 && (raw[owner_end - 1] == '&' || raw[owner_end - 1] == '*')) --owner_end;
		while(owner_end > 0 && isspace(static_cast<unsigned char>(raw[owner_end - 1]))) --owner_end;
		size_t owner_begin = owner_end;
		if(owner_begin > 0 && raw[owner_begin - 1] == '>') {
			int nested_angle = 0;
			while(owner_begin > 0) {
				const char ch = raw[owner_begin - 1];
				if(ch == '>') ++nested_angle;
				else if(ch == '<' && nested_angle > 0) {
					--nested_angle;
					if(nested_angle == 0) {
						--owner_begin;
						break;
					}
				}
				--owner_begin;
			}
			while(owner_begin > 0 && IsIdentifierCharacter(raw[owner_begin - 1])) --owner_begin;
		} else while(owner_begin > 0 && IsIdentifierCharacter(raw[owner_begin - 1])) --owner_begin;
		if(owner_begin == owner_end) {
			separator = next_scope_separator(raw, member_end);
			continue;
		}
		while(owner_begin >= 2 && raw.compare(owner_begin - 2, 2, "::") == 0) {
			size_t component_begin = owner_begin - 2;
			while(component_begin > 0 &&
				IsIdentifierCharacter(raw[component_begin - 1])) --component_begin;
			owner_begin = component_begin;
		}
		const string owner = raw.substr(owner_begin, separator - owner_begin);
		string owner_key = CanonicalSpelling(owner);
		while(!owner_key.empty() && (owner_key[owner_key.size() - 1] == '&' ||
			owner_key[owner_key.size() - 1] == '*')) owner_key.erase(owner_key.size() - 1);
		while(owner_key.compare(0, 6, "const ") == 0)
			owner_key = CanonicalSpelling(owner_key.substr(6));
		while(owner_key.compare(0, 9, "volatile ") == 0)
			owner_key = CanonicalSpelling(owner_key.substr(9));
		const bool known_owner = specialization_bases_.find(LastComponent(owner_key)) !=
			specialization_bases_.end() || owner.find('<') != string::npos;
		if(!known_owner) {
			separator = next_scope_separator(raw, member_end);
			continue;
		}
		string member_type;
		set<string> member_active;
	const string member_name = raw.substr(member_begin, member_end - member_begin);
		string lookup_owner = owner;
		// The generated owner can have been formed by a prior scalar pass over a
		// nested dependent type (`vector<Property>` becoming `vector<unsigned
		// long>`).  Recover the source owner from this RewriteText boundary so
		// member lookup can apply the typed `Property -> Vertex` binding once.
		const size_t source_separator = next_scope_separator(source_spelling, 0);
		if(source_separator != string::npos) {
			size_t source_member_begin = source_separator + 2;
			while(source_member_begin < source_spelling.size() &&
				isspace(static_cast<unsigned char>(source_spelling[source_member_begin]))) ++source_member_begin;
			size_t source_member_end = source_member_begin;
			while(source_member_end < source_spelling.size() &&
				IsIdentifierCharacter(source_spelling[source_member_end])) ++source_member_end;
			if(source_spelling.substr(source_member_begin, source_member_end - source_member_begin) ==
				member_name) {
				const string source_owner = source_spelling.substr(0, source_separator);
				if(source_owner.find('<') != string::npos)
					lookup_owner = source_owner;
			}
		}
		const bool found_member = FindClassMemberType(lookup_owner, member_name,
			substitutions, context, &member_type, &member_active, true);
		if(!found_member || member_type.empty()) {
			separator = next_scope_separator(raw, member_end);
			continue;
		}
		const bool static_member_expression = HasStaticMember(0, owner_key, member_name) ||
			HasStaticMember(0, owner, member_name) ||
			HasStaticMember(0, LastComponent(owner_key), member_name);
		if(static_member_expression) {
			preserved_static_member = true;
			separator = next_scope_separator(raw, member_end);
			continue;
		}
		materialized_member_type = true;
		if(member_type.find("::") == string::npos) {
			const string resolved_member_type = ResolveAlias(member_type, context);
			if(!resolved_member_type.empty()) member_type = resolved_member_type;
		}
		size_t replacement_begin = owner_begin;
		while(replacement_begin > 0 && isspace(static_cast<unsigned char>(raw[replacement_begin - 1])))
			--replacement_begin;
		const size_t prefix_end = replacement_begin;
		size_t prefix_begin = prefix_end;
		while(prefix_begin > 0 && IsIdentifierCharacter(raw[prefix_begin - 1])) --prefix_begin;
		if(prefix_end > prefix_begin &&
			(raw.substr(prefix_begin, prefix_end - prefix_begin) == "const" ||
			 raw.substr(prefix_begin, prefix_end - prefix_begin) == "volatile"))
			replacement_begin = owner_begin;
		if(replacement_begin >= 8 && raw.compare(replacement_begin - 8, 8, "typename") == 0 &&
			(replacement_begin == 8 || !IsIdentifierCharacter(raw[replacement_begin - 9])))
			replacement_begin -= 8;
		raw.replace(replacement_begin, member_end - replacement_begin,
			NormalizeTypeArgument(member_type));
		if(template_replaced) *template_replaced = true;
		separator = next_scope_separator(raw, replacement_begin + member_type.size());
		}
			raw = CollapseReferenceSpelling(raw);
		if(preserved_static_member) return raw;
		if(!resolve_alias || raw.find("::") == string::npos) return raw;
		if(constant_values_.find(raw) != constant_values_.end()) {
			return raw;
		}
		const string result = ResolveAlias(raw, context);
		return result;
	}
} // namespace pa18_templates_internal
