#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

string PA18TemplateExpander::RewriteText(string raw, const string& context,
	const map<string, string>& substitutions, bool* template_replaced,
	bool resolve_alias, bool resolve_member)
{
	if(template_replaced) *template_replaced = false;
	raw = NormalizeElaboratedSpelling(raw, context);
	// Expand a type pack embedded in a direct function type before parsing any
	// nested template-id: `R(Args...)` becomes `R(A,B)`.  A standalone pack
	// expansion stays on the existing path that emits separate outer arguments.
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
		if(raw.compare(0, 8, "operator") == 0) {
			const string suffix = raw.substr(8);
			map<string, string>::const_iterator operator_substitution = substitutions.find(suffix);
			if(operator_substitution != substitutions.end()) {
				raw = "operator" + operator_substitution->second;
				if(template_replaced) *template_replaced = true;
			}
		}
	raw = RewriteDecltypeText(raw, context, substitutions, template_replaced);
	// A qualified use may start from a non-template alias owner rather than a
	// scalar substitution (`lib::ordered_json::object_t`).  Resolve that owner
	// first, then let the ordinary template-id/member path materialize the
	// concrete class and its member type.
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
			const string member_type = TemplateMemberType(*definition, arguments->second,
				member, context);
			if(member_type.empty()) break;
			size_t replacement_begin = at;
			size_t word_begin = at;
			while(word_begin > 0 && isspace(static_cast<unsigned char>(raw[word_begin - 1]))) --word_begin;
			if(word_begin >= 8 && raw.compare(word_begin - 8, 8, "typename") == 0 &&
				(word_begin == 8 || !IsIdentifierCharacter(raw[word_begin - 9])))
				replacement_begin = word_begin - 8;
			// The substituted alias can be reached through a materialized owner,
			// e.g. `tree_int_::traits_type::base_ptr`.  TemplateMemberType returns
			// the actual member type (`node_base*`); retaining the generated owner
			// would manufacture the unrelated nested type `tree_int_::node_base`.
			// Drop that owner only when it is a known materialized specialization.
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
					if(replacement_begin >= 8 && raw.compare(replacement_begin - 8, 8,
						"typename") == 0 && (replacement_begin == 8 ||
						!IsIdentifierCharacter(raw[replacement_begin - 9])))
						replacement_begin -= 8;
				}
			}
			raw.replace(replacement_begin, end - replacement_begin, member_type);
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
		// A specialization that is already materialized is also a valid current
		// instantiation when the reference appears through a nested dependent
		// type.  In that path the local substitution map may contain only `T`,
		// so the primary template name is not otherwise rewritten and would start
		// the same instantiation again.
		const vector<string> current_arguments = SplitTemplateArguments(arguments_text);
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
		// A qualified reference to the class currently being materialized must
		// resolve to that concrete class.  Looking up the primary template again
		// would recursively instantiate the same specialization while rewriting
		// members such as `trait<T>::value`.
		map<string, string>::const_iterator current_substitution =
			substitutions.find(base);
		if(current_substitution != substitutions.end() &&
			current_substitution->second.find('<') == string::npos) {
			string current_name = current_substitution->second;
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
					if(partials != class_specializations_.end())
						for(size_t partial = 0; partial < partials->second.size(); ++partial) {
							map<string, string> partial_bindings;
							const bool matched_partial = MatchClassSpecializationPattern(
								partials->second[partial], current_key->second,
								&partial_bindings, context);
							if(matched_partial)
								for(map<string, string>::const_iterator binding = partial_bindings.begin();
									binding != partial_bindings.end(); ++binding)
									current_bindings[binding->first] = binding->second;
							if(matched_partial)
									for(size_t pack = 0; pack < partials->second[partial].specialization_pack_names.size(); ++pack) {
										const string& pack_name = partials->second[partial].specialization_pack_names[pack];
										map<string, string>::const_iterator binding = partial_bindings.find(pack_name);
										if(binding != partial_bindings.end() && !binding->second.empty())
											current_packs[pack_name] = SplitTemplateArguments(binding->second);
										else current_packs[pack_name] = vector<string>();
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
						string pack_name;
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
							pack_name = word;
							break;
						}
						if(IsTemplatePackName(*definition, word)) {
							pack_name = word;
							known_pack = true;
							break;
						}
					}
					map<string, vector<string> >::const_iterator pack =
						active_pack_substitutions_.find(pack_name);
						if(pack != active_pack_substitutions_.end()) {
							for(size_t element = 0; element < pack->second.size(); ++element) {
								map<string, string> one = substitutions;
								one[pack_name] = pack->second[element];
								args.push_back(CollapseReferenceSpelling(
									ReplaceIdentifiersPreservingPackSizes(prefix, one)));
						}
						continue;
					}
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
				if(i < definition->parameters.size() &&
					definition->parameters[i].template_template) {
					string normalized;
					if(!CompatibleTemplateTemplateArgument(definition->parameters[i], args[i],
						context, substitutions, &normalized))
						throw logic_error("template-template argument does not match");
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
							constant_values_.find(args[i]) == constant_values_.end();
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
									active_integral_substitutions_.end())
									dependent_name = true;
								if(dependent_name) {
								deferred_pack_argument = true;
								break;
							}
							}
							PA19IntegralValue value;
							try {
							args[i] = ResolveIntegralArgument(definition->parameters[i],
								args[i], context, substitutions, &value);
						} catch(const logic_error& error) {
							throw logic_error("definition=" + definition->qualified_name +
								" " + error.what());
						}
						continue;
				}
				args[i] = NormalizeTypeArgument(RewriteText(args[i], context, substitutions, 0));
				args[i] = CollapseReferenceSpelling(ReplaceIdentifiers(args[i], substitutions));
				args[i] = ResolveAlias(args[i], context);
				args[i] = NormalizeTypeArgument(RewriteText(args[i], context, substitutions, 0));
				args[i] = ResolveAlias(args[i], context);
					args[i] = QualifyTypeArgument(args[i], context, definition->owner);
				}
				if(deferred_pack_argument) {
					search = close + 1;
					continue;
				}
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
					map<string, string>::const_iterator substituted = default_substitutions.find(
						definition->parameters[i].name);
					if(substituted != default_substitutions.end()) argument = substituted->second;
					else if(!definition->parameters[i].default_type.empty())
						argument = RewriteText(definition->parameters[i].default_type, context,
							default_substitutions, 0);
					if(argument.empty()) break;
					argument = NormalizeTypeArgument(ReplaceIdentifiers(argument,
						default_substitutions));
					argument = ResolveAlias(argument, context);
					if(i < definition->parameters.size() &&
						definition->parameters[i].template_template) {
						string normalized;
						if(!CompatibleTemplateTemplateArgument(definition->parameters[i], argument,
							context, default_substitutions, &normalized))
						throw logic_error("template-template argument does not match");
						argument = normalized;
					}
					argument = QualifyTypeArgument(argument, context, definition->owner);
					args.push_back(argument);
					if(!definition->parameters[i].name.empty())
						default_substitutions[definition->parameters[i].name] = argument;
				}
				}
				// A type template-id may be encountered while forming the signature of
				// another dependent call.  Its argument is not a concrete type until the
				// enclosing replay installs the bindings; materializing it here would
				// register a bogus specialization such as `view<T>` in the source pass.
				// Defer only when the typed argument still contains an indexed template
				// parameter; unknown-but-concrete local classes remain materializable.
				bool unresolved_type_argument = false;
				for(size_t i = 0; i < args.size() && i < definition->parameters.size(); ++i)
					if(definition->parameters[i].type &&
						HasUnresolvedTemplateParameter(args[i], context, substitutions)) {
						unresolved_type_argument = true;
						break;
					}
				if(unresolved_type_argument) {
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
					} catch(const logic_error& error) {
						throw logic_error("definition=" + definition->qualified_name +
							" " + error.what());
					}
				}
				if(!definition->parameters[i].name.empty())
					argument_substitutions[definition->parameters[i].name] = args[i];
			}
				definition = SelectClassTemplateDefinition(definition, args, context);
			// Resolve a concrete nested owner before the generic member lookup so
			// dependent outer packs remain represented by the materialized class.
			const bool dependent_nested_member = close + 2 < raw.size() &&
				raw.compare(close + 1, 2, "::") == 0 &&
				raw.find("::template", close + 1) != string::npos;
				if((resolve_member || dependent_nested_member) && definition->class_template &&
					close + 2 < raw.size() && raw.compare(close + 1, 2, "::") == 0) {
					const bool nested_rewritten = RewriteConcreteNestedMember(
						&raw, begin, close, base, context, substitutions, template_replaced, &search);
					if(nested_rewritten) continue;
				}
			if(resolve_member && definition->class_template && close + 2 < raw.size() &&
				raw.compare(close + 1, 2, "::") == 0) {
				RecordTemplateArrayValues(*definition, args, context, substitutions);
				size_t nested_begin = close + 3;
				while(nested_begin < raw.size() && IsIdentifierCharacter(raw[nested_begin])) ++nested_begin;
					const string nested = raw.substr(close + 3, nested_begin - (close + 3));
					if(!nested.empty()) {
						const string member_type = TemplateMemberType(*definition, args, nested, context);
						if(!member_type.empty() && member_type.find('[') == string::npos) {
						// The template base can begin after a dependent
						// `Owner::template` qualifier.  The resolved member type
						// already names the materialized owner, so retain neither
						// the dependent qualifier nor the `template` keyword.
						size_t replacement_begin = begin;
						size_t qualifier = begin;
						while(qualifier > 0 && isspace(static_cast<unsigned char>(raw[qualifier - 1])))
							--qualifier;
						if(qualifier >= 8 && raw.compare(qualifier - 8, 8, "template") == 0) {
							qualifier -= 8;
							while(qualifier > 0 && isspace(static_cast<unsigned char>(raw[qualifier - 1])))
								--qualifier;
							if(qualifier >= 2 && raw.compare(qualifier - 2, 2, "::") == 0) {
								qualifier -= 2;
								while(qualifier > 0 && isspace(static_cast<unsigned char>(raw[qualifier - 1])))
									--qualifier;
								while(qualifier > 0) {
									size_t component_end = qualifier;
									while(qualifier > 0 && IsIdentifierCharacter(raw[qualifier - 1])) --qualifier;
									if(component_end == qualifier || qualifier < 2 ||
										raw.compare(qualifier - 2, 2, "::") != 0) break;
									qualifier -= 2;
									while(qualifier > 0 && isspace(static_cast<unsigned char>(raw[qualifier - 1])))
										--qualifier;
								}
								replacement_begin = qualifier;
							}
						}
						raw.replace(replacement_begin, nested_begin - replacement_begin, member_type);
						if(template_replaced) *template_replaced = true;
						search = replacement_begin + member_type.size();
						continue;
					}
						requested_nested_classes_[definition->qualified_name].insert(nested);
						requested_nested_classes_[LastComponent(definition->qualified_name)].insert(nested);
				}
			}
			map<string, string> instantiation_substitutions = substitutions;
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
			const string previous_concrete_owner = active_concrete_owner_;
			if(!concrete_owner_for_instantiation.empty())
				active_concrete_owner_ = concrete_owner_for_instantiation;
			const string* requested_owner = active_concrete_owner_.empty() ? 0 :
				&active_concrete_owner_;
			string local_name;
			try {
				local_name = Instantiate(*definition, args, context, false, 0,
					&instantiation_substitutions, requested_owner);
			} catch(...) {
				active_concrete_owner_ = previous_concrete_owner;
				throw;
			}
			active_concrete_owner_ = previous_concrete_owner;
			string replacement = local_name;
			const string qualifier = PrefixComponent(lookup_base);
			if(!qualifier.empty()) replacement = qualifier + "::" + local_name;
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
				if(replacement == local_name && !definition->owner.empty()) {
					const string source_alias = JoinPath(definition->owner, local_name);
					const string source_target = ResolveAlias(source_alias, context);
					if(source_target != source_alias) replacement = source_target;
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
	for(map<string, string>::const_iterator substitution = substitutions.begin();
		substitution != substitutions.end(); ++substitution) {
		if(specialization_bases_.find(LastComponent(substitution->second)) ==
			specialization_bases_.end()) continue;
		for(size_t at = raw.find(substitution->first); at != string::npos;
			at = raw.find(substitution->first, at + substitution->first.size())) {
			if(at > 0 && IsIdentifierCharacter(raw[at - 1])) continue;
			size_t after = at + substitution->first.size();
			while(after < raw.size() && isspace(static_cast<unsigned char>(raw[after]))) ++after;
			if(after < raw.size() && raw[after] == '<') {
				final_substitutions.erase(substitution->first);
				break;
			}
		}
	}
	raw = ReplaceIdentifiersPreservingPackSizes(raw, final_substitutions);
	// A concrete generated owner can appear without its source template-id after
	// an earlier member substitution (`list2<...>::child0` -> `expr_X::member`).
	// Resolve those typed member aliases in a second pass so a chain such as
	// `Args::child0::proto_grammar` is reduced from the inside out.
	if(resolve_member) for(size_t separator = raw.find("::"); separator != string::npos; ) {
		size_t member_begin = separator + 2;
		while(member_begin < raw.size() && isspace(static_cast<unsigned char>(raw[member_begin])))
			++member_begin;
		if(member_begin >= raw.size() || !IsIdentifierCharacter(raw[member_begin])) {
			separator = raw.find("::", separator + 2);
			continue;
		}
		size_t member_end = member_begin + 1;
		while(member_end < raw.size() && IsIdentifierCharacter(raw[member_end])) ++member_end;
		size_t owner_begin = separator;
		while(owner_begin > 0 && isspace(static_cast<unsigned char>(raw[owner_begin - 1]))) --owner_begin;
		while(owner_begin > 0 && IsIdentifierCharacter(raw[owner_begin - 1])) --owner_begin;
		if(owner_begin == separator) {
			separator = raw.find("::", member_end);
			continue;
		}
		// The backwards scan above finds the generated class component, but a
		// materialized specialization may still be spelled with its namespace
		// prefix (`detail::core_int_`).  Consume those leading scope components
		// too, otherwise replacing `core_int_::member` leaves `detail::` in
		// front of the member's actual type (`detail::int`).
		while(owner_begin >= 2 && raw.compare(owner_begin - 2, 2, "::") == 0) {
			size_t component_begin = owner_begin - 2;
			while(component_begin > 0 &&
				IsIdentifierCharacter(raw[component_begin - 1])) --component_begin;
			owner_begin = component_begin;
		}
		const string owner = raw.substr(owner_begin, separator - owner_begin);
		const bool known_owner = specialization_bases_.find(LastComponent(owner)) !=
			specialization_bases_.end();
		if(!known_owner) {
			separator = raw.find("::", member_end);
			continue;
		}
		string member_type;
		set<string> member_active;
		if(!FindClassMemberType(owner, raw.substr(member_begin, member_end - member_begin),
			substitutions, context, &member_type, &member_active, true) || member_type.empty()) {
			separator = raw.find("::", member_end);
			continue;
		}
		size_t replacement_begin = owner_begin;
		while(replacement_begin > 0 && isspace(static_cast<unsigned char>(raw[replacement_begin - 1])))
			--replacement_begin;
		if(replacement_begin >= 8 && raw.compare(replacement_begin - 8, 8, "typename") == 0 &&
			(replacement_begin == 8 || !IsIdentifierCharacter(raw[replacement_begin - 9])))
			replacement_begin -= 8;
		raw.replace(replacement_begin, member_end - replacement_begin,
			NormalizeTypeArgument(member_type));
		if(template_replaced) *template_replaced = true;
		separator = raw.find("::", replacement_begin + member_type.size());
	}
	if(!resolve_alias || raw.find("::") == string::npos) return raw;
		// A qualified static integral member is an expression here, not a type
		// alias.  Keep its registered spelling intact so a dependent non-type
		// default can be evaluated by the typed constant table.
		if(constant_values_.find(raw) != constant_values_.end()) return raw;
		return ResolveAlias(raw, context);
	}

} // namespace pa18_templates_internal
