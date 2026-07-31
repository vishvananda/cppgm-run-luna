#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

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
			const bool already_qualified = i >= 2 && replaced.size() >= 2 &&
				replaced.compare(replaced.size() - 2, 2, "::") == 0;
			size_t replacement_end = end;
			while(replacement_end < segment.size() &&
				isspace(static_cast<unsigned char>(segment[replacement_end]))) ++replacement_end;
			const bool replacement_has_template_head = found != substitutions.end() &&
				found->second.find('<') != string::npos && replacement_end < segment.size() &&
				segment[replacement_end] == '<';
			if(found != substitutions.end() && !pack_operand && !already_qualified &&
				!replacement_has_template_head &&
				!found->second.empty())
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

} // namespace pa18_templates_internal
