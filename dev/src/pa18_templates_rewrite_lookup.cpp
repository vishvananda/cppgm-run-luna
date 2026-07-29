#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
using namespace std;

namespace pa18_templates_internal {

const TemplateDefinition* PA18TemplateExpander::FindDefinition(string raw_name, const string& context) const
{
		raw_name = Trim(raw_name);
		while(!raw_name.empty() && raw_name[0] == ':') raw_name.erase(0, 1);
		map<string, TemplateDefinition>::const_iterator direct = definitions_.find(raw_name);
		if(direct != definitions_.end()) return &direct->second;
		for(string current = context; ; ) {
			const string candidate = JoinPath(current, raw_name);
			map<string, TemplateDefinition>::const_iterator found = definitions_.find(candidate);
			if(found != definitions_.end()) return &found->second;
			// In-class member templates are sometimes collected through the
			// class-definition context (`Owner::Owner`) even though lookup starts
			// from the ordinary class scope (`Owner`).  Keep that typed owner alias
			// visible for dependent calls such as `check<F>(0)`.
			if(!current.empty()) {
				const string repeated = JoinPath(current, JoinPath(current, raw_name));
				found = definitions_.find(repeated);
				if(found != definitions_.end()) return &found->second;
				if(LastComponent(current) != LastComponent(raw_name)) {
					const string normalized_repeated = JoinPath(current,
						JoinPath(LastComponent(current), raw_name));
					found = definitions_.find(normalized_repeated);
					if(found != definitions_.end()) return &found->second;
				}
			}
			const size_t separator = current.rfind("::");
			if(separator == string::npos) break;
			current.erase(separator);
		}
		// A namespace using-directive contributes a namespace-qualified lookup
		// candidate.  Keep this separate from the short-name fallback so a direct
		// declaration still wins, and resolve an explicit using-declaration in the
		// imported namespace with its typed target rather than guessing by name.
		for(string current = context; ; ) {
			map<string, vector<string> >::const_iterator directives =
				using_namespace_directives_.find(current);
			if(directives != using_namespace_directives_.end())
				for(size_t directive = 0; directive < directives->second.size(); ++directive) {
					const string candidate = JoinPath(directives->second[directive], raw_name);
					map<string, TemplateDefinition>::const_iterator found =
						definitions_.find(candidate);
					if(found != definitions_.end()) return &found->second;
					const size_t separator = TopLevelScopeSeparator(candidate);
					if(separator == string::npos) continue;
					const string imported_scope = candidate.substr(0, separator);
					map<string, vector<const TemplateDefinition*> >::const_iterator imports =
						using_declaration_targets_.find(imported_scope);
					if(imports == using_declaration_targets_.end()) continue;
					const TemplateDefinition* imported = 0;
					for(size_t index = 0; index < imports->second.size(); ++index) {
						const TemplateDefinition* target = imports->second[index];
						if(!target || target->name != LastComponent(raw_name)) continue;
						if(imported && imported != target) { imported = 0; break; }
						imported = target;
					}
					if(imported) return imported;
				}
			if(current.empty()) break;
			const size_t separator = current.rfind("::");
			if(separator == string::npos) current.clear();
			else current.erase(separator);
		}

		// An explicit using-declaration is an unambiguous typed import even when
		// other definitions share the same short name.  Search the current scope
		// and its enclosing scopes before applying the conservative short-name
		// fallback below.  Function-template imports remain on the ordinary call
		// lookup path; this table is only a type/alias lookup aid.
		for(string current = context; ; ) {
			map<string, vector<const TemplateDefinition*> >::const_iterator imports =
				using_declaration_targets_.find(current);
			if(imports != using_declaration_targets_.end()) {
				const TemplateDefinition* imported = 0;
				for(size_t index = 0; index < imports->second.size(); ++index) {
					const TemplateDefinition* candidate = imports->second[index];
					if(!candidate || candidate->name != LastComponent(raw_name)) continue;
					if(imported && imported != candidate) {
						imported = 0;
						break;
					}
					imported = candidate;
				}
				if(imported) return imported;
			}
			if(current.empty()) break;
			const size_t separator = current.rfind("::");
			if(separator == string::npos) current.clear();
			else current.erase(separator);
		}
		map<string, vector<string> >::const_iterator by_name = definitions_by_name_.find(LastComponent(raw_name));
		if(by_name != definitions_by_name_.end() && by_name->second.size() == 1) {
			map<string, TemplateDefinition>::const_iterator found = definitions_.find(by_name->second[0]);
			if(found != definitions_.end()) return &found->second;
		}
		// Unqualified lookup from a logical namespace must also see entities
		// collected under an inline namespace's physical path.  The ordinary
		// candidate walk above only probes `lib::basic_json`, while collection
		// intentionally stores the declaration as `lib::abi::basic_json` so its
		// emitted owner remains stable.  Recover that relationship from the
		// typed lexical namespace map instead of inventing a second definition.
		string logical_context = context;
		for(string current = context; ; ) {
			map<string, string>::const_iterator logical = lexical_namespace_logical_.find(current);
			if(logical != lexical_namespace_logical_.end()) {
				logical_context = logical->second;
				break;
			}
			if(current.empty()) break;
			const size_t separator = current.rfind("::");
			if(separator == string::npos) current.clear();
			else current.erase(separator);
		}
		if(raw_name.find("::") == string::npos && !logical_context.empty()) {
			const TemplateDefinition* logical_match = 0;
			for(map<string, TemplateDefinition>::const_iterator it = definitions_.begin();
				it != definitions_.end(); ++it) {
				if(LastComponent(it->second.qualified_name) != raw_name) continue;
				const string physical_owner = PrefixComponent(it->second.qualified_name);
				map<string, string>::const_iterator logical = lexical_namespace_logical_.find(
					physical_owner);
				if(logical == lexical_namespace_logical_.end() ||
					logical->second != logical_context) continue;
				if(logical_match && logical_match->qualified_name != it->second.qualified_name)
					return 0;
				logical_match = &it->second;
			}
			if(logical_match) return logical_match;
		}
		// An inline namespace contributes its declarations to the enclosing
		// namespace for lookup.  Collection keeps the physical namespace in the
		// typed definition key (`lib::abi::map`) so generated declarations remain
		// deterministic; resolve a logical spelling such as `lib::map` against
		// that physical key here instead of duplicating the entity.
		const size_t raw_separator = TopLevelScopeSeparator(raw_name);
		if(raw_separator != string::npos) {
			const string logical_owner = raw_name.substr(0, raw_separator);
			const string logical_name = raw_name.substr(raw_separator + 2);
			const TemplateDefinition* logical_match = 0;
			for(map<string, TemplateDefinition>::const_iterator it = definitions_.begin();
				it != definitions_.end(); ++it) {
				if(LastComponent(it->second.qualified_name) != logical_name) continue;
				const string physical_owner = PrefixComponent(it->second.qualified_name);
				map<string, string>::const_iterator logical =
					lexical_namespace_logical_.find(physical_owner);
				if(logical == lexical_namespace_logical_.end() ||
					logical->second != logical_owner) continue;
				if(logical_match) return 0;
				logical_match = &it->second;
			}
			if(logical_match) return logical_match;
		}
		return 0;
}

} // namespace pa18_templates_internal
