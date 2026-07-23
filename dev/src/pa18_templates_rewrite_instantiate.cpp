#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

#include <functional>
using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::MemberOwnerPattern(const TemplateDefinition& candidate,
	const TemplateDefinition& parent, const vector<string>& parent_args,
	map<string, string>* inferred) const
{
	// A member template declared inside a class template is collected under
	// the parser's class scope spelling.  For a primary class this can be the
	// class path itself or the repeated class component used by a member
	// declarator (for example `tuple::tuple::operator=`); it still denotes a
	// member of the concrete parent specialization.
	const string member_scope = JoinPath(parent.qualified_name, parent.name);
	if(candidate.owner == parent.qualified_name || candidate.owner == member_scope) {
		// An in-class member declaration has no separate owner pattern.  Its
		// template parameters describe the member itself, not the enclosing
		// class, so they must not be matched against the class arguments.
		if(inferred) inferred->clear();
		return true;
	}
	size_t angle = string::npos;
	for(size_t search = 0; ; ) {
		const size_t candidate_angle = candidate.owner.find('<', search);
		if(candidate_angle == string::npos) break;
		if(candidate.owner.substr(0, candidate_angle) == parent.qualified_name) {
			angle = candidate_angle;
			break;
		}
		search = candidate_angle + 1;
	}
	if(angle == string::npos)
		return false;
	string owner_arguments;
	size_t close = string::npos;
	if(!TemplateRange(candidate.owner, angle, &owner_arguments, &close)) return false;
	const vector<string> patterns = SplitTemplateArguments(owner_arguments);
	set<string> parameter_names;
	for(size_t i = 0; i < parent.parameters.size(); ++i)
		if(!parent.parameters[i].name.empty()) parameter_names.insert(
			parent.parameters[i].name);
	for(size_t i = 0; i < candidate.parameters.size(); ++i)
		if(!candidate.parameters[i].name.empty()) parameter_names.insert(
			candidate.parameters[i].name);
	map<string, string> local;
	size_t pattern_index = 0;
	size_t argument_index = 0;
	for(; pattern_index < patterns.size(); ++pattern_index) {
		const string pattern = CanonicalSpelling(patterns[pattern_index]);
		const bool pack = pattern.size() > 3 &&
			pattern.compare(pattern.size() - 3, 3, "...") == 0;
		if(pack && pattern_index + 1 == patterns.size()) {
			const string pack_name = CanonicalSpelling(pattern.substr(0, pattern.size() - 3));
			if(parameter_names.find(pack_name) == parameter_names.end()) return false;
			string combined;
			while(argument_index < parent_args.size()) {
				if(!combined.empty()) combined += ",";
				combined += parent_args[argument_index++];
			}
			local[pack_name] = combined;
			break;
		}
		if(argument_index >= parent_args.size() || !MatchTypePattern(pattern,
			parent_args[argument_index++], parameter_names, &local, parent.owner, true)) return false;
	}
	if(argument_index != parent_args.size()) return false;
	if(inferred) *inferred = local;
	return true;
}

string PA18TemplateExpander::MemberSignatureKey(const TemplateDefinition& candidate) const
{
	string result = LastComponent(candidate.name);
	const CPPGMAstNodePtr declarator = FunctionDeclarator(candidate.declaration);
	const CPPGMAstNodePtr clause = DescendantOfKind(declarator, "parameter-clause");
	if(!clause) return result;
	for(size_t i = 0; i < clause->children.size(); ++i)
		if(clause->children[i]) result += "|" + ParameterTypeSpelling(clause->children[i]);
	return result;
}

vector<const TemplateDefinition*> PA18TemplateExpander::MemberDefinitions(
	const TemplateDefinition& parent, const vector<string>& parent_args) const
{
	vector<const TemplateDefinition*> matches;
	for(map<string, TemplateDefinition>::const_iterator it = definitions_.begin();
		it != definitions_.end(); ++it) {
		const TemplateDefinition& candidate = it->second;
		if(candidate.class_template ||
			(candidate.declaration->kind != "simple-declaration" &&
				candidate.declaration->kind != "function-definition" &&
				candidate.declaration->kind != "special-member-definition")) continue;
		map<string, string> inferred;
		const bool matched = MemberOwnerPattern(candidate, parent, parent_args, &inferred);
		if(matched)
			matches.push_back(&candidate);
	}
	vector<const TemplateDefinition*> result;
	vector<string> signatures;
	vector<vector<string> > owner_patterns_by_match;
	vector<bool> comparable;
	signatures.reserve(matches.size());
	owner_patterns_by_match.reserve(matches.size());
	comparable.reserve(matches.size());
	const auto extract_owner_patterns = [&](const TemplateDefinition& candidate,
		vector<string>* patterns) {
		size_t angle = string::npos;
		for(size_t search = 0; ; ) {
			const size_t candidate_angle = candidate.owner.find('<', search);
			if(candidate_angle == string::npos) break;
			if(candidate.owner.substr(0, candidate_angle) == parent.qualified_name) {
				angle = candidate_angle;
				break;
			}
			search = candidate_angle + 1;
		}
		if(angle == string::npos) return false;
		string owner_arguments;
		size_t close = string::npos;
		if(!TemplateRange(candidate.owner, angle, &owner_arguments, &close)) return false;
		if(patterns) *patterns = SplitTemplateArguments(owner_arguments);
		return true;
	};
	for(size_t i = 0; i < matches.size(); ++i) {
		signatures.push_back(MemberSignatureKey(*matches[i]));
		vector<string> patterns;
		const bool parsed = extract_owner_patterns(*matches[i], &patterns);
		owner_patterns_by_match.push_back(patterns);
		comparable.push_back(parsed);
	}
	for(size_t i = 0; i < matches.size(); ++i) {
		if(!comparable[i]) {
			result.push_back(matches[i]);
			continue;
		}
		bool dominated = false;
		for(size_t j = 0; j < matches.size(); ++j) {
			if(i == j || signatures[i] != signatures[j] || !comparable[j]) continue;
			map<string, string> ignored;
			const bool other_more_specialized = MemberOwnerPattern(*matches[j], parent,
				owner_patterns_by_match[i], &ignored);
			ignored.clear();
			const bool candidate_more_specialized = MemberOwnerPattern(*matches[i], parent,
				owner_patterns_by_match[j], &ignored);
			// If this candidate accepts the other candidate's owner pattern while
			// the other candidate does not accept this one, the other pattern is
			// the more specialized member owner and this candidate is dominated.
			if(candidate_more_specialized && !other_more_specialized) {
				dominated = true;
				break;
			}
		}
		if(!dominated) result.push_back(matches[i]);
	}
	return result;
}

void PA18TemplateExpander::RecordConstantArrayDeclaration(
	const CPPGMAstNodePtr& node, const string& context,
	const map<string, string>& substitutions)
{
	if(!node || node->kind != "simple-declaration" || node->children.empty()) return;
	const string specifiers = SpellNode(node->children[0]);
	if(specifiers.find("constexpr") == string::npos &&
		specifiers.find("const") == string::npos) return;
	const string element_type = ResolveAlias(RewriteText(
		NodeTypeSpelling(node->children[0]), context, substitutions, 0), context);
	if(!PA19Type(element_type).integral) return;
	const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
	if(!list) return;
	for(size_t item_index = 0; item_index < list->children.size(); ++item_index) {
		const CPPGMAstNodePtr item = list->children[item_index];
		if(!item || item->children.size() < 2 || !item->children[0] ||
			!item->children[1]) continue;
		const string array_suffix = DeclaratorArraySuffix(item->children[0]);
		if(array_suffix.empty()) continue;
		const string name = LastComponent(FirstIdentifierLocal(item->children[0]));
		CPPGMAstNodePtr initializer = item->children[1];
		if(initializer->kind == "initializer" && initializer->children.size() == 1)
			initializer = initializer->children[0];
		if(name.empty() || !initializer || initializer->kind != "braced-init-list") continue;
		vector<PA19IntegralValue> values;
		for(size_t value_index = 0; value_index < initializer->children.size(); ++value_index) {
			PA19IntegralValue value;
			const string expression = ConstantExpressionSpelling(
				initializer->children[value_index]);
			if(!EvaluateIntegralText(expression, context, substitutions, &value)) {
				values.clear();
				break;
			}
			values.push_back(value);
		}
		if(values.empty() && !initializer->children.empty()) continue;
		const string qualified = JoinPath(context, name);
		constant_arrays_[qualified] = values;
		if(constant_arrays_.find(name) == constant_arrays_.end())
			constant_arrays_[name] = values;
	}
}

const vector<PA19IntegralValue>* PA18TemplateExpander::FindConstantArray(
	const string& raw, const string& context) const
{
	string name = CanonicalSpelling(raw);
	while(!name.empty() && name[0] == '&') name = CanonicalSpelling(name.substr(1));
	const size_t separator = name.rfind("::");
	if(separator != string::npos) name = name.substr(separator + 2);
	map<string, vector<PA19IntegralValue> >::const_iterator direct =
		constant_arrays_.find(name);
	if(direct != constant_arrays_.end()) return &direct->second;
	for(string current = context; ; ) {
		const string qualified = JoinPath(current, name);
		map<string, vector<PA19IntegralValue> >::const_iterator found =
			constant_arrays_.find(qualified);
		if(found != constant_arrays_.end()) return &found->second;
		if(current.empty()) break;
		const size_t parent = current.rfind("::");
		if(parent == string::npos) current.clear();
		else current.erase(parent);
	}
	return 0;
}

string PA18TemplateExpander::NormalizeIntegralExpression(string raw) const
{
	raw = CanonicalSpelling(raw);
	for(;;) {
		while(raw.size() >= 2 && raw[0] == '(') {
			int depth = 0; size_t matching = string::npos;
			for(size_t position = 0; position < raw.size(); ++position) {
				if(raw[position] == '(') ++depth;
				else if(raw[position] == ')' && --depth == 0) { matching = position; break; }
			}
			if(matching != raw.size() - 1) break;
			raw = CanonicalSpelling(raw.substr(1, raw.size() - 2));
		}
		int parentheses = 0, brackets = 0; size_t comma = string::npos;
		for(size_t position = 0; position < raw.size(); ++position) {
			if(raw[position] == '(') ++parentheses;
			else if(raw[position] == ')' && parentheses > 0) --parentheses;
			else if(raw[position] == '[') ++brackets;
			else if(raw[position] == ']' && brackets > 0) --brackets;
			else if(raw[position] == ',' && parentheses == 0 && brackets == 0) {
				comma = position; break;
			}
		}
		if(comma == string::npos) return CanonicalSpelling(raw);
		const string left = CanonicalSpelling(raw.substr(0, comma));
		if(left.find("(void)") == string::npos &&
			left.find("static_cast<void>") == string::npos) return CanonicalSpelling(raw);
		raw = CanonicalSpelling(raw.substr(comma + 1));
	}
}

bool PA18TemplateExpander::ExpandIntegralPackExpression(const string& raw,
	const string& context, const map<string, string>& substitutions,
	string* expanded)
{
	if(!expanded) return false;
	for(size_t ellipsis = raw.find("..."); ellipsis != string::npos;
		ellipsis = raw.find("...", ellipsis + 3)) {
		// `sizeof...(Pack)` is a value query, not a comma expansion.  It is
		// handled by the active-pack size path and its operand must remain
		// untouched while this expression is replayed.
		if(ellipsis + 3 < raw.size() && raw[ellipsis + 3] == '(' &&
			ellipsis >= 6 && raw.substr(ellipsis - 6, 6) == "sizeof") continue;
		int angle = 0, parentheses = 0, brackets = 0, braces = 0;
		size_t begin = ellipsis;
		while(begin > 0) {
			const char ch = raw[begin - 1];
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
			if(ch == ',' && angle == 0 && parentheses == 0 && brackets == 0 &&
				braces == 0) break;
			--begin;
		}
		if(begin < raw.size() && raw[begin] == ',') ++begin;
		else if(begin < raw.size() && raw[begin] == '(') ++begin;
		const string source_expression = CanonicalSpelling(raw.substr(begin,
			ellipsis - begin));
		if(source_expression.empty()) continue;

		string pack_name;
		for(size_t position = 0; position < source_expression.size();) {
			if(source_expression.compare(position, 9, "sizeof...") == 0) {
				const size_t open = position + 9;
				if(open < source_expression.size() && source_expression[open] == '(') {
					int depth = 0;
					for(size_t skip = open; skip < source_expression.size(); ++skip) {
						if(source_expression[skip] == '(') ++depth;
						else if(source_expression[skip] == ')' && --depth == 0) {
							position = skip + 1;
							break;
						}
					}
					if(position > open) continue;
				}
			}
			if(!IsIdentifierCharacter(source_expression[position])) {
				++position;
				continue;
			}
			const size_t word_begin = position;
			while(position < source_expression.size() &&
				IsIdentifierCharacter(source_expression[position])) ++position;
			const string word = source_expression.substr(word_begin,
				position - word_begin);
			if(active_pack_substitutions_.find(word) != active_pack_substitutions_.end() ||
				active_pack_identifier_substitutions_.find(word) !=
					active_pack_identifier_substitutions_.end()) {
				pack_name = word;
				break;
			}
		}
		if(pack_name.empty()) continue;
		const vector<string>* values = 0;
		map<string, vector<string> >::const_iterator typed =
			active_pack_substitutions_.find(pack_name);
		if(typed != active_pack_substitutions_.end()) values = &typed->second;
		else {
			map<string, vector<string> >::const_iterator named =
				active_pack_identifier_substitutions_.find(pack_name);
			if(named != active_pack_identifier_substitutions_.end()) values = &named->second;
		}
		if(!values) continue;
		string replacement;
		for(size_t value = 0; value < values->size(); ++value) {
			map<string, string> one = substitutions;
			one[pack_name] = (*values)[value];
			string element = RewriteText(source_expression, context, one, 0);
			if(!element.empty()) {
				if(!replacement.empty()) replacement += ',';
				replacement += CanonicalSpelling(element);
			}
		}
		*expanded = raw.substr(0, begin) + replacement + raw.substr(ellipsis + 3);
		return true;
	}
	return false;
}

bool PA18TemplateExpander::EvaluateActivePackSize(string raw,
	PA19IntegralValue* result) const
{
	raw = CanonicalSpelling(raw);
	if(raw.compare(0, 9, "sizeof...") != 0 || raw.size() < 11 ||
		raw[9] != '(' || raw[raw.size() - 1] != ')') return false;
	const string operand = CanonicalSpelling(raw.substr(10, raw.size() - 11));
	const vector<string>* selected = 0;
	map<string, vector<string> >::const_iterator typed =
		active_pack_substitutions_.find(operand);
	if(typed != active_pack_substitutions_.end()) selected = &typed->second;
	if(!selected) {
		map<string, vector<string> >::const_iterator named =
			active_pack_identifier_substitutions_.find(operand);
		if(named != active_pack_identifier_substitutions_.end()) selected = &named->second;
	}
	// Some older AST rewrite nodes have already replaced the pack identifier
	// with the scalar substitution used for its first element.  Recover that
	// identity only when the scalar is an exact member of a typed pack; never
	// infer a pack merely because it happens to be the sole active pack.
	if(!selected) for(map<string, vector<string> >::const_iterator pack =
		active_pack_substitutions_.begin(); pack != active_pack_substitutions_.end(); ++pack)
		if(!pack->second.empty() && NormalizeTypeArgument(operand) ==
			NormalizeTypeArgument(pack->second[0])) {
			selected = &pack->second;
			break;
		}
	if(!selected) for(map<string, vector<string> >::const_iterator pack =
		active_pack_identifier_substitutions_.begin();
		pack != active_pack_identifier_substitutions_.end(); ++pack)
		if(!pack->second.empty() && NormalizeTypeArgument(operand) ==
			NormalizeTypeArgument(pack->second[0])) {
			selected = &pack->second;
			break;
		}
	if(!selected) return false;
	*result = PA19IntegralValue::Unsigned(
		static_cast<unsigned long long>(selected->size()), "unsigned long", 64);
	return true;
}

string PA18TemplateExpander::RewriteActivePackSizes(string raw) const
{
	for(size_t search = raw.find("sizeof..."); search != string::npos; ) {
		const size_t open = search + 9;
		if(open >= raw.size() || raw[open] != '(') {
			search = raw.find("sizeof...", search + 9); continue;
		}
		int depth = 0; size_t close = string::npos;
		for(size_t position = open; position < raw.size(); ++position) {
			if(raw[position] == '(') ++depth;
			else if(raw[position] == ')' && --depth == 0) { close = position; break; }
		}
		if(close == string::npos) break;
		PA19IntegralValue count;
		const string expression = raw.substr(search, close - search + 1);
		if(EvaluateActivePackSize(expression, &count)) {
			const string replacement = IntegralValueSpelling(count);
			raw.replace(search, close - search + 1, replacement);
			search += replacement.size();
		} else search = raw.find("sizeof...", close + 1);
	}
	return raw;
}

bool PA18TemplateExpander::EvaluateUnqualifiedConstantMember(
	const string& raw, const string& context,
	const map<string, string>& substitutions, PA19IntegralValue* result)
{
	if(!result || raw.empty()) return false;
	// Collection visits the primary/partial declaration before it has a
	// concrete substitution scope.  Its dependent initializer must stay
	// deferred; attempting to resolve an unqualified member against that same
	// source declaration recursively re-enters this helper.
	if(context.find('<') != string::npos && !HasReplayContext(substitutions) &&
		active_pack_substitutions_.empty() &&
		active_pack_identifier_substitutions_.empty()) return false;
	for(size_t i = 0; i < raw.size(); ++i)
		if(!IsIdentifierCharacter(raw[i])) return false;
	map<string, vector<string> >::const_iterator owners =
		constant_member_owners_.find(raw);
	if(owners == constant_member_owners_.end()) return false;

	for(size_t owner_index = 0; owner_index < owners->second.size(); ++owner_index) {
		const string& owner_name = owners->second[owner_index];
		map<string, CPPGMAstNodePtr>::const_iterator candidate =
			class_declarations_.find(owner_name);
		if(candidate == class_declarations_.end() || !candidate->second) continue;
		const CPPGMAstNodePtr owner_declaration = candidate->second;
		for(size_t child = 0; child < owner_declaration->children.size(); ++child) {
			const CPPGMAstNodePtr member_declaration = owner_declaration->children[child];
			if(!member_declaration || member_declaration->kind != "simple-declaration" ||
				member_declaration->children.empty()) continue;
			const CPPGMAstNodePtr list = ChildOfKindLocal(member_declaration,
				"init-declarator-list");
			if(!list) continue;
			for(size_t item = 0; item < list->children.size(); ++item) {
				const CPPGMAstNodePtr declarator = list->children[item];
				if(!declarator || declarator->children.size() < 2 ||
					LastComponent(FirstIdentifierLocal(declarator->children[0])) != raw)
					continue;
				const CPPGMAstNodePtr initializer = declarator->children[1];
				if(!initializer || initializer->children.empty()) continue;

				map<string, string> member_substitutions = substitutions;
				map<string, string>::const_iterator base = specialization_bases_.find(
					LastComponent(candidate->first));
				map<string, vector<string> >::const_iterator arguments =
					specialization_arguments_.find(LastComponent(candidate->first));
				const TemplateDefinition* owner_definition = 0;
				map<string, vector<string> > owner_packs;
				if(base != specialization_bases_.end() && arguments != specialization_arguments_.end()) {
					owner_definition = FindDefinition(base->second, context);
					if(owner_definition) {
						size_t argument = 0;
						for(size_t parameter = 0; parameter < owner_definition->parameters.size();
							++parameter) {
							const TemplateParameter& item_parameter = owner_definition->parameters[parameter];
							if(item_parameter.pack) {
								vector<string> values;
								size_t trailing_fixed = 0;
								for(size_t later = parameter + 1;
									later < owner_definition->parameters.size(); ++later)
									if(!owner_definition->parameters[later].pack) ++trailing_fixed;
								const size_t available = arguments->second.size() > argument ?
									arguments->second.size() - argument : 0;
								const size_t count = available > trailing_fixed ?
									available - trailing_fixed : 0;
								for(size_t value = 0; value < count; ++value)
									values.push_back(arguments->second[argument++]);
								if(!item_parameter.name.empty()) {
									owner_packs[item_parameter.name] = values;
									if(!values.empty()) member_substitutions[item_parameter.name] = values[0];
									else member_substitutions.erase(item_parameter.name);
								}
								continue;
							}
							if(argument < arguments->second.size()) {
								if(!item_parameter.name.empty())
									member_substitutions[item_parameter.name] = arguments->second[argument];
								++argument;
							}
						}
					}
				}

				const map<string, vector<string> > previous_packs = active_pack_substitutions_;
				for(map<string, vector<string> >::const_iterator pack = owner_packs.begin();
					pack != owner_packs.end(); ++pack)
					if(!pack->first.empty()) active_pack_substitutions_[pack->first] = pack->second;
				const string expression = ConstantExpressionSpelling(initializer->children[0]);
				bool evaluated = false;
				if(expression != raw)
					evaluated = EvaluateIntegralText(expression, candidate->first,
						member_substitutions, result);
			active_pack_substitutions_ = previous_packs;
				if(evaluated) {
					const PA19IntegralType type = PA19Type(ResolveAlias(
						NodeTypeSpelling(member_declaration->children[0]), candidate->first));
					if(type.integral) *result = PA19Convert(*result, type);
					return result->known;
				}
			}
		}
	}
	return false;
}

bool PA18TemplateExpander::ExpandNamedIntegralOperands(
	const string& raw, const string& context,
	const map<string, string>& substitutions, PA19IntegralValue* result)
{
	if(!result) return false;
	string expanded = raw;
	bool changed = false;
	for(size_t position = 0; position < expanded.size();) {
		if(!IsIdentifierCharacter(expanded[position])) {
			++position;
			continue;
		}
		const size_t begin = position;
		while(position < expanded.size() && IsIdentifierCharacter(expanded[position])) ++position;
		const string word = expanded.substr(begin, position - begin);
		if(substitutions.find(word) != substitutions.end() ||
			active_pack_substitutions_.find(word) != active_pack_substitutions_.end() ||
			active_pack_identifier_substitutions_.find(word) !=
			active_pack_identifier_substitutions_.end()) continue;
		if(begin >= 2 && expanded.compare(begin - 2, 2, "::") == 0) continue;
		PA19IntegralValue value;
		if(!EvaluateUnqualifiedConstantMember(word, context, substitutions, &value)) continue;
		expanded.replace(begin, word.size(), IntegralValueSpelling(value));
		changed = true;
		position = begin + IntegralValueSpelling(value).size();
	}
	if(!changed) return false;
	PA19ConstantExpressionParser parser(constant_values_, substitutions,
		constant_type_sizes_, constant_type_alignments_, type_aliases_);
	return parser.Evaluate(expanded, result);
}

bool PA18TemplateExpander::EvaluateIntegralText(string raw, const string& context,
	const map<string, string>& substitutions, PA19IntegralValue* result)
{
	// A leading global-scope qualifier is semantically transparent once the
	// expression has been detached from its source namespace.  Keep the typed
	// constant tables canonical (they are indexed without that prefix) while
	// still accepting source spellings such as `::meta::detail::Trait<...>::value`.
	while(raw.compare(0, 2, "::") == 0) raw.erase(0, 2);
	if(context.find('<') != string::npos) {
		bool unresolved_scope = false;
		if(!HasReplayContext(substitutions)) {
			for(size_t position = 0; position < raw.size() && !unresolved_scope;) {
				if(!isalpha(static_cast<unsigned char>(raw[position])) && raw[position] != '_') {
					++position;
					continue;
				}
				const size_t begin = position;
				while(position < raw.size() && IsIdentifierCharacter(raw[position])) ++position;
				const string word = raw.substr(begin, position - begin);
				if(word != "true" && word != "false" && word != "sizeof")
					unresolved_scope = true;
			}
		}
		for(map<string,string>::const_iterator it = substitutions.begin();
			it != substitutions.end(); ++it)
			if(it->second.find("decltype") != string::npos ||
				it->second.find("...") != string::npos ||
				HasDependentVariableTemplate(it->second, context, substitutions)) {
				unresolved_scope = true;
				break;
			}
		if(unresolved_scope && raw.find("::value") != string::npos) {
			bool concrete_values = true;
			bool found_value = false;
			for(size_t marker = raw.find("::value"); marker != string::npos;
				marker = raw.find("::value", marker + 7)) {
				size_t begin = marker;
				if(begin > 0 && raw[begin - 1] == '>') {
					int angle_depth = 0;
					while(begin > 0) {
						const char character = raw[begin - 1];
						if(character == '>') ++angle_depth;
						else if(character == '<' && angle_depth > 0) {
							--angle_depth;
							--begin;
							if(angle_depth == 0) break;
							continue;
						}
						--begin;
					}
				}
				while(begin > 0 && (IsIdentifierCharacter(raw[begin - 1]) ||
					raw[begin - 1] == ':')) --begin;
				const string operand = raw.substr(begin, marker + 7 - begin);
				const string owner = operand.substr(0, operand.size() - 7);
				found_value = true;
				if(constant_values_.find(operand) != constant_values_.end()) continue;
				if(owner.find('<') != string::npos) {
					const string replayed_owner = CanonicalSpelling(RemoveMarker(
						RewriteText(owner, context, substitutions, 0)));
					if(replayed_owner != owner && replayed_owner.find('<') == string::npos)
						continue;
					concrete_values = false;
					break;
				}
				if(class_declarations_.find(owner) == class_declarations_.end() &&
					specialization_bases_.find(LastComponent(owner)) == specialization_bases_.end()) {
					concrete_values = false;
					break;
				}
			}
			if(found_value && concrete_values) unresolved_scope = false;
		}
		if(unresolved_scope) return false;
	}
	// Expand pack sizes while the operand still carries its source identifier.
	// The ordinary scalar substitution map intentionally keeps the first pack
	// element for compatibility with older rewrite paths; applying it first
	// would make `sizeof...(Args)` indistinguishable from a size query on that
	// element and would require a guessed pack or a registry-wide search.
	raw = CanonicalSpelling(raw);
	raw = CanonicalSpelling(RewriteActivePackSizes(raw));
	map<string, string> expression_substitutions = substitutions;
	for(map<string,string>::const_iterator it = substitutions.begin();
		it != substitutions.end(); ++it)
		if(raw.find(it->first + "...") != string::npos)
			expression_substitutions.erase(it->first);
	raw = CanonicalSpelling(ReplaceIdentifiersPreservingPackSizes(raw,
		expression_substitutions));
	raw = NormalizeIntegralExpression(raw);
	// A dependent member initializer can reach this evaluator before the
	// ordinary text rewriter has materialized the nested class whose `value`
	// it names.  Replay template-ids in the expression first; the resulting
	// generated spelling is then handled by the typed constant tables below.
	if(raw.find('<') != string::npos) {
		const string rewritten = CanonicalSpelling(RemoveMarker(
			RewriteText(raw, context, substitutions, 0)));
		if(rewritten != raw && EvaluateIntegralText(rewritten, context,
			substitutions, result)) return true;
	}
	// `sizeof` may take an overloaded function call as its operand.  The
	// ordinary constant-expression parser only has a type spelling at this
	// point, so use the decltype call resolver to select the viable template
	// overload and then measure its typed result.  This is particularly
	// important for arity probes whose functions return a reference to an
	// array (`char (&)[N]`).
	if(raw.compare(0, 7, "sizeof(") == 0 && !raw.empty() && raw[raw.size() - 1] == ')') {
		const string operand = raw.substr(7, raw.size() - 8);
		string call_type;
		if(FunctionCallResultType(operand, context, substitutions, &call_type)) {
			string resolved = CanonicalSpelling(RemoveMarker(RewriteText(
				call_type, context, substitutions, 0)));
			resolved = ResolveAlias(resolved, context);
			const size_t size = EstimateTypeSize(resolved, context);
			if(size) {
				*result = PA19IntegralValue::Unsigned(
					static_cast<unsigned long long>(size), "unsigned long", 64);
				return true;
			}
		}
	}
	if(EvaluateActivePackSize(raw, result)) return true;
	const size_t subscript_open = raw.find('[');
	if(subscript_open != string::npos && raw[raw.size() - 1] == ']' && subscript_open > 0) {
		PA19IntegralValue index;
		const string index_text = raw.substr(subscript_open + 1, raw.size() - subscript_open - 2);
		if(EvaluateIntegralText(index_text, context, substitutions, &index)) {
			const vector<PA19IntegralValue>* values = FindConstantArray(
				raw.substr(0, subscript_open), context);
			const long long offset = PA19Signed(index);
			if(values && offset >= 0 && static_cast<size_t>(offset) < values->size()) {
				*result = (*values)[static_cast<size_t>(offset)]; return result->known;
			}
		}
	}
	const size_t value_separator = raw.rfind("::value");
	if(value_separator != string::npos) {
		const string owner = raw.substr(0, value_separator);
		const string resolved_owner = CanonicalSpelling(ResolveAlias(owner, context));
		if(!resolved_owner.empty() && resolved_owner != owner) {
			PA19IntegralValue aliased_value;
			if(EvaluateIntegralText(resolved_owner + "::value", context,
				substitutions, &aliased_value)) {
				*result = aliased_value; return result->known;
			}
		}
	}
	const size_t functional_open = raw.find('(');
	const bool functional_cast = functional_open != string::npos &&
		!raw.empty() && raw[raw.size() - 1] == ')' && functional_open > 0;
	const size_t braced_open = raw.find('{');
	const bool braced_cast = braced_open != string::npos &&
		!raw.empty() && raw[raw.size() - 1] == '}' && braced_open > 0;
	if(functional_cast || braced_cast) {
		const size_t open = functional_cast ? functional_open : braced_open;
		const string target = ResolveAlias(CanonicalSpelling(raw.substr(0, open)), context);
		const PA19IntegralType target_type = PA19Type(target);
		if(target_type.integral) {
			const string operand = raw.substr(open + 1, raw.size() - open - 2);
			PA19IntegralValue converted;
			if(EvaluateIntegralText(operand, context, substitutions, &converted)) {
				*result = PA19Convert(converted, target_type); return result->known;
			}
		}
	}
	if(raw.compare(0, 2, "::") == 0) {
		string unscoped = raw;
		while(unscoped.compare(0, 2, "::") == 0) unscoped.erase(0, 2);
		map<string, PA19IntegralValue>::const_iterator known = constant_values_.find(unscoped);
		if(known != constant_values_.end()) { *result = known->second; return result->known; }
	}
	PA19ConstantExpressionParser parser(constant_values_, substitutions,
		constant_type_sizes_, constant_type_alignments_, type_aliases_);
	if(raw.find("::") == string::npos && !active_instantiation_name_.empty()) {
		map<string, PA19IntegralValue>::const_iterator scoped = constant_values_.find(
			JoinPath(active_instantiation_name_, raw));
		if(scoped != constant_values_.end() && scoped->second.known) {
			*result = scoped->second;
			return true;
		}
	}
	map<string, PA19IntegralValue>::const_iterator direct_value =
		constant_values_.find(raw);
	if(direct_value != constant_values_.end() && direct_value->second.known) {
		*result = direct_value->second;
		return true;
	}
	// Alias-template materialization can register the source owner as a class
	// declaration while the transformed node names the concrete target class.
	// Reconnect that owner spelling to the generated declaration before treating
	// the member as an unresolved expression.
	if(value_separator != string::npos) {
		const string owner = raw.substr(0, value_separator);
		map<string, CPPGMAstNodePtr>::const_iterator declaration =
			class_declarations_.find(owner);
		if(declaration != class_declarations_.end() && declaration->second) {
			const CPPGMAstNodePtr& generated_declaration = declaration->second;
			for(size_t child = 0; child < generated_declaration->children.size(); ++child) {
				const CPPGMAstNodePtr clause = generated_declaration->children[child];
				if(!clause || clause->kind != "base-clause") continue;
				for(size_t base = 0; base < clause->children.size(); ++base) {
					const CPPGMAstNodePtr base_name = ChildOfKindLocal(
						clause->children[base], "base-name");
					if(!base_name) continue;
					const string base_spelling = CanonicalSpelling(ReplaceIdentifiers(
						base_name->value, substitutions));
					const string base_member = base_spelling + raw.substr(value_separator);
					map<string, PA19IntegralValue>::const_iterator base_value =
						constant_values_.find(base_member);
					if(base_value != constant_values_.end() && base_value->second.known) {
						*result = base_value->second;
						return true;
					}
				}
			}
			string generated_name = CanonicalSpelling(RemoveMarker(declaration->second->value));
			while(generated_name.compare(0, 7, "struct ") == 0 ||
				generated_name.compare(0, 6, "class ") == 0 ||
				generated_name.compare(0, 6, "union ") == 0)
				generated_name = CanonicalSpelling(generated_name.substr(generated_name.find(' ') + 1));
			const string generated_member = generated_name + raw.substr(value_separator);
			map<string, PA19IntegralValue>::const_iterator generated_value =
				constant_values_.find(generated_member);
			if(generated_value != constant_values_.end() && generated_value->second.known) {
				*result = generated_value->second;
				return true;
			}
			const string local_member = LastComponent(generated_name) + raw.substr(value_separator);
			generated_value = constant_values_.find(local_member);
			if(generated_value != constant_values_.end() && generated_value->second.known) {
				*result = generated_value->second;
				return true;
			}
		}
	}
	const bool qualified_value_expression = raw.find("::value") != string::npos;
	if(!qualified_value_expression || constant_values_.find(raw) != constant_values_.end()) {
		if(parser.Evaluate(raw, result)) return true;
	}
	if(ExpandNamedIntegralOperands(raw, context, substitutions, result)) return true;
	if(EvaluateSourceIntegralExpression(raw, context, substitutions, result)) return true;
	if(EvaluateMaterializedTemplateValue(raw, context, substitutions, result)) return true;
	if(ExpandIntegralValueOperands(raw, context, substitutions, result)) return true;
	if(EvaluateInheritedIntegralValue(raw, context, substitutions, result)) return true;
	if(class_contexts_.find(raw) != class_contexts_.end()) {
		map<string, PA19IntegralValue>::const_iterator object_value =
			constant_values_.find(raw + "::value");
		if(object_value != constant_values_.end()) { *result = object_value->second; return result->known; }
	}
	if(raw.find("::") == string::npos && !context.empty())
		for(string current = context; ; ) {
			const string qualified = JoinPath(current, raw);
			if(parser.Evaluate(qualified, result)) return true;
			const size_t separator = current.rfind("::");
			if(separator == string::npos) break;
			current.erase(separator);
		}
	string owner = context, member = raw;
	const size_t separator = raw.rfind("::");
	if(separator != string::npos) { owner = raw.substr(0, separator); member = raw.substr(separator + 2); }
	CPPGMAstNodePtr declaration = FindClassDeclaration(owner, context);
	if(declaration) for(size_t i = 0; i < declaration->children.size(); ++i) {
		const CPPGMAstNodePtr child = declaration->children[i];
		if(!child || child->kind != "simple-declaration" || child->children.empty()) continue;
		const string specifiers = SpellNode(child->children[0]);
		if(specifiers.find("const") == string::npos && specifiers.find("constexpr") == string::npos) continue;
		const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
		if(!list) continue;
		for(size_t j = 0; j < list->children.size(); ++j) {
			const CPPGMAstNodePtr item = list->children[j];
			if(!item || item->children.size() < 2 ||
				LastComponent(FirstIdentifierLocal(item->children[0])) != LastComponent(member)) continue;
			const CPPGMAstNodePtr initializer = item->children[1];
			if(!initializer || initializer->children.empty()) continue;
			PA19IntegralValue value;
			if(!EvaluateIntegralText(ConstantExpressionSpelling(initializer->children[0]),
				owner, substitutions, &value)) continue;
			const PA19IntegralType type = PA19Type(ResolveAlias(NodeTypeSpelling(
				child->children[0]), owner));
			if(type.integral) value = PA19Convert(value, type);
			*result = value; return result->known;
		}
	}
	return false;
}

CPPGMAstNodePtr PA18TemplateExpander::FindSourceConstantFunction(
	string raw, const string& context) const
{
	raw = CanonicalSpelling(raw);
	const size_t call_open = raw.find('(');
	if(call_open != string::npos) raw.erase(call_open);
	const size_t template_open = raw.find('<');
	if(template_open != string::npos) raw.erase(template_open);
	while(!raw.empty() && raw[0] == ':') raw.erase(raw.begin());
	map<string, CPPGMAstNodePtr>::const_iterator direct = function_definitions_.find(raw);
	if(direct != function_definitions_.end()) return direct->second;
	for(string current = context; ; ) {
		const string candidate = JoinPath(current, raw);
		map<string, CPPGMAstNodePtr>::const_iterator found =
			function_definitions_.find(candidate);
		if(found != function_definitions_.end()) return found->second;
		if(current.empty()) break;
		const size_t separator = current.rfind("::");
		if(separator == string::npos) current.clear();
		else current.erase(separator);
	}
	CPPGMAstNodePtr found;
	for(map<string, CPPGMAstNodePtr>::const_iterator it = function_definitions_.begin();
		it != function_definitions_.end(); ++it)
		if(LastComponent(it->first) == LastComponent(raw)) {
			if(found) return CPPGMAstNodePtr();
			found = it->second;
		}
	if(found) return found;
	for(map<string, TemplateDefinition>::const_iterator definition = definitions_.begin();
		definition != definitions_.end(); ++definition) {
		const CPPGMAstNodePtr declaration = definition->second.declaration;
		if(!declaration) continue;
		if(declaration->kind == "function-definition" &&
			LastComponent(FirstIdentifierLocal(declaration->children.size() > 1 ?
				declaration->children[1] : CPPGMAstNodePtr())) == LastComponent(raw))
			return declaration;
		if(declaration->kind != "class-specifier" &&
			declaration->kind != "class-forward-declaration") continue;
		for(size_t child = 0; child < declaration->children.size(); ++child) {
			const CPPGMAstNodePtr member = declaration->children[child];
			if(member && member->kind == "function-definition" &&
				LastComponent(FirstIdentifierLocal(member->children.size() > 1 ?
					member->children[1] : CPPGMAstNodePtr())) == LastComponent(raw))
				return member;
		}
	}
	return found;
}

CPPGMAstNodePtr PA18TemplateExpander::SourceReturnExpression(
	const CPPGMAstNodePtr& function) const
{
	if(!function) return CPPGMAstNodePtr();
	CPPGMAstNodePtr body = ChildOfKindLocal(function, "compound-statement");
	if(!body) return CPPGMAstNodePtr();
	CPPGMAstNodePtr returned = DescendantOfKind(body, "return-statement");
	return returned && !returned->children.empty() ? returned->children[0] :
		CPPGMAstNodePtr();
}

bool PA18TemplateExpander::EvaluateSourceFunctionReturn(
	const CPPGMAstNodePtr& function, const string& context,
	const map<string, string>& substitutions, PA19IntegralValue* result)
{
	const CPPGMAstNodePtr expression = SourceReturnExpression(function);
	if(!expression) return false;
	if(expression->kind == "id-expression" && !context.empty()) {
		PA19IntegralValue qualified;
		if(EvaluateIntegralText(JoinPath(context, expression->value), context,
			substitutions, &qualified)) {
			*result = qualified;
			return true;
		}
	}
	return EvaluateIntegralText(ConstantExpressionSpelling(expression), context,
		substitutions, result);
}

bool PA18TemplateExpander::EvaluateSourceObjectMember(
	const string& raw, const string& context,
	const map<string, string>& substitutions, PA19IntegralValue* result)
{
	const size_t dot = raw.rfind("().");
	if(dot == string::npos) return false;
	size_t begin = dot;
	while(begin > 0 && IsIdentifierCharacter(raw[begin - 1])) --begin;
	if(begin != 0) return false;
	const size_t member_begin = dot + 3;
	if(member_begin >= raw.size()) return false;
	size_t member_end = member_begin;
	while(member_end < raw.size() && IsIdentifierCharacter(raw[member_end])) ++member_end;
	if(member_end != raw.size()) return false;
	const string function_name = raw.substr(begin, dot - begin + 2);
	const CPPGMAstNodePtr function = FindSourceConstantFunction(function_name, context);
	const CPPGMAstNodePtr returned = SourceReturnExpression(function);
	CPPGMAstNodePtr expression = returned;
	if(expression && expression->kind == "call-expression" &&
		expression->children.size() > 1 && expression->children[1] &&
		expression->children[1]->children.size() == 1 &&
		expression->children[1]->children[0] &&
		expression->children[1]->children[0]->kind == "braced-init-list")
		expression = expression->children[1]->children[0];
	if(!expression || expression->kind != "braced-init-list") return false;
	string return_type = NodeTypeSpelling(function->children.empty() ?
		CPPGMAstNodePtr() : function->children[0]);
	return_type = ResolveAlias(RewriteText(return_type, context, substitutions, 0), context);
	const CPPGMAstNodePtr declaration = FindClassDeclaration(return_type, context);
	if(!declaration) return false;
	size_t member = 0;
	for(size_t child = 0; child < declaration->children.size(); ++child) {
		const CPPGMAstNodePtr field = declaration->children[child];
		if(!field || field->kind != "simple-declaration" || field->children.empty()) continue;
		if(SpellNode(field->children[0]).find("static") != string::npos) continue;
		const CPPGMAstNodePtr list = ChildOfKindLocal(field, "init-declarator-list");
		if(!list) continue;
		for(size_t item = 0; item < list->children.size(); ++item) {
			const CPPGMAstNodePtr declarator = list->children[item];
			if(!declarator || declarator->children.empty()) continue;
			if(LastComponent(FirstIdentifierLocal(declarator->children[0])) ==
				raw.substr(member_begin)) {
				if(member >= expression->children.size()) return false;
				return EvaluateIntegralText(ConstantExpressionSpelling(
					expression->children[member]), context, substitutions, result);
			}
			++member;
		}
	}
	return false;
}

bool PA18TemplateExpander::EvaluateSourceClassTruth(
	string raw, const string& context,
	const map<string, string>& substitutions, PA19IntegralValue* result)
{
	raw = CanonicalSpelling(ReplaceIdentifiers(raw, substitutions));
	while(raw.compare(0, 6, "const ") == 0) raw = CanonicalSpelling(raw.substr(6));
	while(!raw.empty() && (raw[raw.size() - 1] == '&' || raw[raw.size() - 1] == '*'))
		raw.erase(raw.size() - 1);
	raw = CanonicalSpelling(raw);
	if(raw.size() >= 2 && (raw.substr(raw.size() - 2) == "{}" ||
		raw.substr(raw.size() - 2) == "()"))
		raw.erase(raw.size() - 2);
	raw = CanonicalSpelling(raw);
	const CPPGMAstNodePtr declaration = FindClassDeclaration(raw, context);
	if(!declaration) return false;
	// A constexpr conversion inherited from an integral-constant base is the
	// value of an otherwise empty class object.  The generated specialization
	// may not contain a copied conversion member, so consult the typed inherited
	// value only after establishing that this is a materialized class.
	if(class_contexts_.find(raw) != class_contexts_.end()) {
		PA19IntegralValue inherited;
		if(EvaluateInheritedIntegralValue(raw + "::value", context, substitutions,
			&inherited)) {
			*result = inherited;
			return result->known;
		}
	}
	for(size_t child = 0; child < declaration->children.size(); ++child) {
		const CPPGMAstNodePtr member = declaration->children[child];
		if(!member || (member->kind != "function-definition" &&
			member->kind != "special-member-definition") || member->children.size() < 2)
			continue;
		if(SpellNode(member).find("constexpr") == string::npos) continue;
		const string name = member->kind == "special-member-definition" ?
			member->value : LastComponent(FirstIdentifierLocal(member->children[1]));
		if(name.compare(0, 8, "operator") != 0) continue;
		if(EvaluateSourceFunctionReturn(member, raw, substitutions, result)) return true;
	}
	for(size_t child = 0; child < declaration->children.size(); ++child) {
		const CPPGMAstNodePtr clause = declaration->children[child];
		if(!clause || clause->kind != "base-clause") continue;
		for(size_t base = 0; base < clause->children.size(); ++base) {
			const CPPGMAstNodePtr base_name = ChildOfKindLocal(
				clause->children[base], "base-name");
			if(!base_name) continue;
			string spelling = RewriteText(base_name->value, context, substitutions, 0);
			spelling = ResolveAlias(ReplaceIdentifiers(spelling, substitutions), context);
			if(EvaluateSourceClassTruth(spelling, context, substitutions, result)) return true;
		}
	}
	return false;
}

bool PA18TemplateExpander::EvaluateSourceIntegralExpression(
	string raw, const string& context,
	const map<string, string>& substitutions, PA19IntegralValue* result)
{
	raw = CanonicalSpelling(ReplaceIdentifiers(raw, substitutions));
	if(EvaluateSourceArrayFunction(raw, context, substitutions, result)) return true;
	string expanded = raw;
	bool expanded_source_call = false;
	for(size_t search = 0; search < expanded.size(); ) {
		if(!IsIdentifierCharacter(expanded[search])) {
			++search;
			continue;
		}
		const size_t begin = search;
		while(search < expanded.size() && IsIdentifierCharacter(expanded[search])) ++search;
		if(search >= expanded.size() || expanded[search] != '(') continue;
		int depth = 0;
		size_t close = string::npos;
		for(size_t position = search; position < expanded.size(); ++position) {
			if(expanded[position] == '(') ++depth;
			else if(expanded[position] == ')' && --depth == 0) {
				close = position;
				break;
			}
		}
		if(close == string::npos) break;
		const string candidate = expanded.substr(begin, close - begin + 1);
		if(!FindSourceConstantFunction(expanded.substr(begin, search - begin), context)) {
			search = close + 1;
			continue;
		}
		PA19IntegralValue call_value;
		if(!EvaluateSourceArrayFunction(candidate, context, substitutions, &call_value)) {
			search = close + 1;
			continue;
		}
		const string replacement = IntegralValueSpelling(call_value);
		expanded.replace(begin, candidate.size(), replacement);
		expanded_source_call = true;
		search = begin + replacement.size();
	}
	if(expanded_source_call) {
		PA19ConstantExpressionParser parser(constant_values_, substitutions,
			constant_type_sizes_, constant_type_alignments_, type_aliases_);
		if(parser.Evaluate(expanded, result)) return true;
	}
	if(EvaluateSourceObjectMember(raw, context, substitutions, result)) return true;
	for(size_t marker = raw.find("()."); marker != string::npos; ) {
		size_t begin = marker;
		while(begin > 0 && IsIdentifierCharacter(raw[begin - 1])) --begin;
		size_t end = raw.find_first_not_of(
			"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_", marker + 3);
		if(end == string::npos) end = raw.size();
		if(end == marker + 3) break;
		const string member = raw.substr(begin, end - begin);
		PA19IntegralValue member_value;
		if(EvaluateSourceObjectMember(member, context, substitutions, &member_value)) {
			raw.replace(begin, end - begin, IntegralValueSpelling(member_value));
			PA19ConstantExpressionParser parser(constant_values_, substitutions,
				constant_type_sizes_, constant_type_alignments_, type_aliases_);
			if(parser.Evaluate(raw, result)) return true;
		} else marker = raw.find("().", marker + 3);
	}
	const size_t open = raw.find('(');
	if(open != string::npos && !raw.empty() && raw[raw.size() - 1] == ')' &&
		raw.find(',', open) == string::npos) {
		const string name = raw.substr(0, open);
		if(raw.substr(open + 1, raw.size() - open - 2).empty()) {
			const CPPGMAstNodePtr function = FindSourceConstantFunction(name, context);
			if(function && EvaluateSourceFunctionReturn(function, context, substitutions, result))
				return true;
		}
	}
	if(raw.size() >= 2 && (raw.substr(raw.size() - 2) == "{}" ||
		raw.substr(raw.size() - 2) == "()"))
		if(EvaluateSourceClassTruth(raw, context, substitutions, result)) return true;
	return false;
}

void PA18TemplateExpander::RecordTemplateArrayValues(
	const TemplateDefinition& definition, const vector<string>& arguments,
	const string& context, const map<string, string>& substitutions)
{
	if(!definition.declaration) return;
	for(size_t child_index = 0; child_index < definition.declaration->children.size(); ++child_index) {
		const CPPGMAstNodePtr child = definition.declaration->children[child_index];
		if(!child || child->kind != "simple-declaration" || child->children.empty() ||
			SpellNode(child->children[0]).find("constexpr") == string::npos) continue;
		const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
		if(!list) continue;
		for(size_t item_index = 0; item_index < list->children.size(); ++item_index) {
			const CPPGMAstNodePtr item = list->children[item_index];
			if(!item || item->children.size() < 2 || !item->children[0]) continue;
			if(DeclaratorArraySuffix(item->children[0]).empty()) continue;
			const string name = LastComponent(FirstIdentifierLocal(item->children[0]));
			CPPGMAstNodePtr initializer = item->children[1];
			if(initializer->kind == "initializer" && initializer->children.size() == 1)
				initializer = initializer->children[0];
			if(name.empty() || !initializer || initializer->kind != "braced-init-list") continue;
			vector<PA19IntegralValue> values;
			for(size_t value_index = 0; value_index < initializer->children.size(); ++value_index) {
				const CPPGMAstNodePtr element = initializer->children[value_index];
				CPPGMAstNodePtr expression = element;
				string pack_name;
				if(element && element->kind == "pack-expansion-expression" &&
					!element->children.empty()) {
					pack_name = PackExpansionIdentifier(element->children[0]);
					const string source_text = ConstantExpressionSpelling(element->children[0]);
					for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter)
						if(definition.parameters[parameter].pack &&
							source_text.find(definition.parameters[parameter].name) != string::npos)
							pack_name = definition.parameters[parameter].name;
				}
				if(!pack_name.empty()) {
					for(size_t argument = 0; argument < arguments.size(); ++argument) {
						map<string, string> one = substitutions;
						one[pack_name] = arguments[argument];
						string text = ConstantExpressionSpelling(expression->children.empty() ?
							CPPGMAstNodePtr() : expression->children[0]);
						text = RewriteText(text, context, one, 0);
						PA19IntegralValue value;
						if(!EvaluateIntegralText(text, context, one, &value)) {
							values.clear();
							break;
						}
						values.push_back(value);
					}
				} else {
					const string text = ConstantExpressionSpelling(expression);
					PA19IntegralValue value;
					if(!EvaluateIntegralText(text, context, substitutions, &value)) {
						values.clear();
						break;
					}
					values.push_back(value);
				}
			}
			if(values.empty() && !initializer->children.empty()) continue;
			constant_arrays_[name] = values;
			constant_arrays_[JoinPath(context, name)] = values;
		}
	}
}

void PA18TemplateExpander::ReplayCachedInstantiation(const TemplateDefinition& definition,
	const vector<string>& args, const string& cached, const string& context,
	bool explicit_instantiation, const map<string, vector<string> >& pack_substitutions)
{
	if(!definition.class_template) return;
	const map<string, vector<string> > previous_packs = active_pack_substitutions_;
	active_pack_substitutions_ = pack_substitutions;
	try {
		InstantiateRequestedNestedClasses(definition, args, cached, context);
		InstantiateMemberDefinitions(definition, args, cached, explicit_instantiation);
	} catch(...) {
		active_pack_substitutions_ = previous_packs;
		throw;
	}
	active_pack_substitutions_ = previous_packs;
}

void PA18TemplateExpander::RegisterGeneratedSpecialization(
	const TemplateDefinition& definition, const vector<string>& metadata_args,
	const string& local_name)
{
	if(!definition.class_template) return;
	specialization_bases_[local_name] = definition.qualified_name;
	specialization_arguments_[local_name] = metadata_args;
	vector<string>& indexed_names = specialization_names_by_base_[
		LastComponent(definition.qualified_name)];
	if(find(indexed_names.begin(), indexed_names.end(), local_name) == indexed_names.end())
		indexed_names.push_back(local_name);
	const string generated_owner = definition.lexical_owner.empty() ?
		definition.owner : definition.lexical_owner;
	if(class_contexts_.find(generated_owner) != class_contexts_.end()) return;
	vector<CPPGMAstNodePtr>& forwards = generated_namespace_forwards_[generated_owner];
	for(size_t i = 0; i < forwards.size(); ++i)
		if(forwards[i] && LastComponent(forwards[i]->value) == local_name) return;
	forwards.push_back(MakeForwardClass(local_name));
}

void PA18TemplateExpander::AddConcreteOwnerSubstitutions(
	const string& concrete_owner, const string& context,
	map<string, string>* substitutions)
{
	if(concrete_owner.empty() || !substitutions) return;
	map<string, string>::const_iterator owner_base = specialization_bases_.find(
		LastComponent(concrete_owner));
	map<string, vector<string> >::const_iterator owner_arguments =
		specialization_arguments_.find(LastComponent(concrete_owner));
	if(owner_base == specialization_bases_.end() ||
		owner_arguments == specialization_arguments_.end()) return;
	const TemplateDefinition* owner_definition = FindDefinition(owner_base->second, context);
	if(!owner_definition || !owner_definition->class_template) return;
	for(size_t parameter = 0; parameter < owner_definition->parameters.size() &&
		parameter < owner_arguments->second.size(); ++parameter)
		if(!owner_definition->parameters[parameter].name.empty())
			(*substitutions)[owner_definition->parameters[parameter].name] =
				owner_arguments->second[parameter];
}

bool PA18TemplateExpander::ConcreteOwnerMatches(
	const TemplateDefinition& definition, const string& concrete_owner) const
{
	if(concrete_owner.empty() || definition.owner.empty()) return false;
	map<string, string>::const_iterator base = specialization_bases_.find(
		LastComponent(concrete_owner));
	if(base == specialization_bases_.end()) return false;
	string source_owner = definition.owner;
	const size_t template_open = source_owner.find('<');
	if(template_open != string::npos) source_owner.erase(template_open);
	return LastComponent(base->second) == LastComponent(source_owner);
}

string PA18TemplateExpander::FindConcreteInstantiationOwner(
	const TemplateDefinition& definition, const map<string, string>& substitutions,
	const string& context, const string& requested_owner) const
{
	string concrete_owner;
	const string source_owner_name = LastComponent(definition.owner);
	for(map<string, string>::const_iterator substitution = substitutions.begin();
		substitution != substitutions.end(); ++substitution)
		if(substitution->first == source_owner_name &&
			class_contexts_.find(substitution->second) != class_contexts_.end()) {
			concrete_owner = substitution->second;
			break;
		}
	if(!concrete_owner.empty()) return concrete_owner;
	if(ConcreteOwnerMatches(definition, requested_owner) &&
		class_contexts_.find(requested_owner) != class_contexts_.end())
		return requested_owner;
	const size_t owner_open = definition.owner.find('<');
	string owner_arguments_text;
	size_t owner_close = string::npos;
	if(owner_open == string::npos || !TemplateRange(definition.owner, owner_open,
		&owner_arguments_text, &owner_close)) return string();
	const string owner_base = definition.owner.substr(0, owner_open);
	const vector<string> owner_arguments = SplitTemplateArguments(owner_arguments_text);
	map<string, vector<string> >::const_iterator names =
		specialization_names_by_base_.find(LastComponent(owner_base));
	if(names == specialization_names_by_base_.end()) return string();
	for(size_t name = 0; name < names->second.size(); ++name) {
		const string& candidate = names->second[name];
		map<string, string>::const_iterator base = specialization_bases_.find(candidate);
		map<string, vector<string> >::const_iterator arguments =
			specialization_arguments_.find(candidate);
		if(base == specialization_bases_.end() || arguments == specialization_arguments_.end() ||
			LastComponent(base->second) != LastComponent(owner_base) ||
			arguments->second.size() != owner_arguments.size() ||
			class_contexts_.find(candidate) == class_contexts_.end()) continue;
		bool same = true;
		for(size_t argument = 0; argument < owner_arguments.size(); ++argument) {
			const string expected = NormalizeTypeArgument(ResolveAlias(
				ReplaceIdentifiers(owner_arguments[argument], substitutions), context));
			if(NormalizeTypeArgument(arguments->second[argument]) != expected) {
				same = false;
				break;
			}
		}
		if(same) return candidate;
	}
	return string();
}

string PA18TemplateExpander::EmitInstantiation(const TemplateDefinition& definition,
	const vector<string>& args, const vector<string>& metadata_args,
	map<string, string> substitutions,
	const map<string, PA19IntegralValue>& integral_substitutions,
	const map<string, vector<string> >& pack_substitutions, const string& context,
	bool explicit_instantiation, const string& key, const string& local_name,
	const string& requested_owner)
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
	const string concrete_owner = definition.class_template ?
		FindConcreteInstantiationOwner(definition, substitutions, context, requested_owner) :
		((member_definition && ConcreteOwnerMatches(definition, requested_owner)) ||
		 (definition.alias_template && ConcreteOwnerMatches(definition, requested_owner)) ?
			requested_owner : string());
	if(definition.class_template) {
		class_contexts_.insert(JoinPath(definition.owner, local_name));
		class_contexts_.insert(JoinPath(generated_owner, local_name));
	}
	const string previous_instantiation_name = active_instantiation_name_;
	active_instantiation_name_ = definition.class_template ? local_name : string();
	CPPGMAstNodePtr generated;
	try {
		generated = TransformInstantiatedNode(definition, definition.owner,
			substitutions, integral_substitutions, pack_substitutions);
	} catch(...) {
		active_instantiation_name_ = previous_instantiation_name;
		throw;
	}
	active_instantiation_name_ = previous_instantiation_name;
	if(!generated) throw logic_error("unable to instantiate template");
	MarkGeneratedNode(generated, definition.qualified_name, metadata_args, explicit_instantiation);
	if(generated->kind == "simple-declaration") {
		const CPPGMAstNodePtr identifier = DescendantOfKind(generated, "identifier");
		if(identifier) identifier->value = local_name;
		RecordConstantDeclaration(generated, generated_owner);
	}
	if(definition.class_template)
		generated->dependent_base_lookup = DefinitionHasDependentBase(definition);
	if(!definition.class_template && !definition.alias_template && !member_definition)
		RenameGeneratedFunction(generated, local_name);
	if(definition.class_template || definition.alias_template) generated->value = local_name;
	if(definition.alias_template) {
		RegisterGeneratedTypeAlias(generated, generated_owner);
		// A member alias template is emitted from its source lexical owner, but
		// a template-template argument names it through the concrete enclosing
		// specialization.  Register both spellings so a later `::value` or type
		// lookup reaches the already rewritten alias target.
		set<string> concrete_owners;
		if(!concrete_owner.empty()) concrete_owners.insert(concrete_owner);
		for(map<string, string>::const_iterator substitution = substitutions.begin();
			substitution != substitutions.end(); ++substitution) {
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
	if(definition.class_template) {
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
	EnsureDeclarationDependencies(generated, definition.owner, generated_owner);
	// Text replay can encounter the same member template-id while resolving a
	// dependent type and give the detached definition its standalone cache name.
	// Once the entity is owned by a concrete class, the declaration must retain
	// the member spelling so class-scope lookup can bind it.
	if(member_definition) RenameGeneratedFunction(generated, LastComponent(definition.name));
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
	if(class_contexts_.find(context) != class_contexts_.end() && owner_is_context_ancestor &&
		context != definition.owner &&
		!recursive_context_argument) {
		generated_before_class_[context].push_back(generated);
	}
	else if(recursive_context_argument && definition.owner.empty() &&
		!PrefixComponent(context).empty())
		generated_before_class_[PrefixComponent(context)].push_back(generated);
	else {
		generated_by_owner_[concrete_owner.empty() ? generated_owner : concrete_owner].push_back(generated);
	}
	active_specializations_.erase(key);
	return local_name;
}

string PA18TemplateExpander::MaterializeInstantiation(const TemplateDefinition& definition,
	const vector<string>& args, const vector<string>& metadata_args,
	map<string, string> substitutions,
	const map<string, PA19IntegralValue>& integral_substitutions,
	const map<string, vector<string> >& pack_substitutions, const string& context,
	bool explicit_instantiation, const string& key, const string& concrete_owner)
{
	map<string, string>::const_iterator cached = specializations_.find(key);
	if(cached != specializations_.end()) {
		ReplayCachedInstantiation(definition, args, cached->second, context,
			explicit_instantiation, pack_substitutions);
		return cached->second;
	}
	const string local_name = GeneratedSpecializationName(definition, args,
		metadata_args, substitutions, context, concrete_owner);
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
		key, local_name, concrete_owner);
}

string PA18TemplateExpander::Instantiate(const TemplateDefinition& definition,
	const vector<string>& raw_args, const string& context, bool explicit_instantiation,
	const map<string, vector<string> >* pack_hints,
	const map<string, string>* outer_substitutions, const string* requested_owner)
{
	if(definition.parameters.empty()) throw logic_error("template has no type parameters");
	vector<string> args, metadata_args;
	map<string, string> substitutions = outer_substitutions ? *outer_substitutions :
		map<string, string>();
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
	return MaterializeInstantiation(definition, args, metadata_args, substitutions,
		integral_substitutions, pack_substitutions, context, explicit_instantiation, key,
		concrete_owner);
}

} // namespace pa18_templates_internal
