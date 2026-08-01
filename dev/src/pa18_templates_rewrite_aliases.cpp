#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

void PA18TemplateExpander::RecordTypedefSubstitutions(
	const CPPGMAstNodePtr& original_child, const string& child_context,
	map<string, string>* local_substitutions)
{
	const CPPGMAstNodePtr list = ChildOfKindLocal(original_child, "init-declarator-list");
	if(!list) return;
	for(size_t item_index = 0; item_index < list->children.size(); ++item_index) {
		const CPPGMAstNodePtr item = list->children[item_index];
		if(!item || item->children.empty()) continue;
		const string alias_name = FirstIdentifierLocal(item->children[0]);
		if(alias_name.empty()) continue;
		const string raw_type = DeclaratorTypeSpelling(
			NodeTypeSpelling(original_child->children[0]), item->children[0]);
		const string rewritten_type = RewriteText(raw_type, child_context,
			*local_substitutions, 0);
		if(raw_type.empty() || rewritten_type.empty()) continue;
		string materialized_type = rewritten_type;
		// Preserve evaluated bounds when a local typedef is used through a type-id.
		for(size_t search_end = materialized_type.size(); search_end > 0;) {
			const size_t close = materialized_type.rfind(']', search_end - 1);
			if(close == string::npos || close + 1 != search_end) break;
			int bracket_depth = 0;
			size_t open = string::npos;
			for(size_t position = close + 1; position-- > 0;) {
				if(materialized_type[position] == ']') ++bracket_depth;
				else if(materialized_type[position] == '[' && --bracket_depth == 0) {
					open = position;
					break;
				}
			}
			if(open == string::npos || open + 1 == close) break;
			PA19IntegralValue bound;
			const string bound_text = materialized_type.substr(open + 1,
				close - open - 1);
			if(!EvaluateIntegralText(bound_text, child_context,
				*local_substitutions, &bound)) break;
			materialized_type.replace(open + 1, close - open - 1,
				IntegralValueSpelling(bound));
			search_end = open;
		}
		(*local_substitutions)[alias_name] = materialized_type;
		const string qualified_alias = JoinPath(child_context, alias_name);
		type_aliases_[qualified_alias] = materialized_type;
		vector<string>& aliases = type_aliases_by_name_[alias_name];
		if(find(aliases.begin(), aliases.end(), qualified_alias) == aliases.end())
			aliases.push_back(qualified_alias);
	}
}

CPPGMAstNodePtr PA18TemplateExpander::ReplayAliasTemplateDeclaration(
	const CPPGMAstNodePtr& input, const map<string, string>& substitutions)
{
	CPPGMAstNodePtr cloned = CloneNode(input);
	if(!cloned || active_pack_substitutions_.empty()) return cloned;
	const function<string(const string&)> expand_captured_packs =
		[&](string raw) {
		for(map<string, vector<string> >::const_iterator pack =
			active_pack_substitutions_.begin(); pack != active_pack_substitutions_.end(); ++pack) {
			if(pack->first.empty()) continue;
			const string token = pack->first + "...";
			string expanded;
			for(size_t value = 0; value < pack->second.size(); ++value) {
				if(!expanded.empty()) expanded += ',';
				expanded += pack->second[value];
			}
			for(size_t at = raw.find(token); at != string::npos;) {
				raw.replace(at, token.size(), expanded);
				at = raw.find(token, at + expanded.size());
			}
		}
		return RewriteActivePackSizes(ReplaceIdentifiersPreservingPackSizes(raw,
			substitutions));
	};
	const function<void(const CPPGMAstNodePtr&)> rewrite_alias =
		[&](const CPPGMAstNodePtr& node) {
		if(!node) return;
		if(node->kind == "type-name" || node->kind == "decl-specifier" ||
			node->kind == "type-specifier" || node->kind == "decltype-specifier") {
			const size_t colon = node->value.find(':');
			const string prefix = colon == string::npos ? string() : node->value.substr(0, colon);
			const bool token_marker = prefix == "TT_IDENTIFIER" || prefix == "TT_LITERAL" ||
				prefix.compare(0, 3, "KW_") == 0 || prefix.compare(0, 3, "OP_") == 0;
			const bool replay_suffix = colon != string::npos && !token_marker &&
				(colon + 1 >= node->value.size() || node->value[colon + 1] != ':') &&
				prefix.find('<') != string::npos;
			const string marker = token_marker ? node->value.substr(0, colon + 1) : string();
			const string spelling = replay_suffix ? node->value.substr(colon + 1) :
				(token_marker ? node->value.substr(colon + 1) : RemoveMarker(node->value));
			bool captured = replay_suffix;
			for(map<string, vector<string> >::const_iterator pack =
				active_pack_substitutions_.begin(); pack != active_pack_substitutions_.end(); ++pack)
				if(!pack->first.empty() && spelling.find(pack->first + "...") != string::npos) {
					captured = true;
					break;
				}
			if(captured) node->value = marker + expand_captured_packs(spelling);
		}
		for(size_t child = 0; child < node->children.size(); ++child)
			rewrite_alias(node->children[child]);
	};
	rewrite_alias(cloned->children.size() > 1 ? cloned->children[1] : cloned);
	return cloned;
}

} // namespace pa18_templates_internal
