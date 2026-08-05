#include "pa14_lowering.h"

using namespace std;

namespace cppgm_pa14_lowering {

vector<Binding*> PA14Lowerer::MemberBindings(const TypePtr& raw_object,
                                             const string& name) const
{
    TypePtr object = type_value(raw_object);
    if(object && object->kind == TYPE_POINTER) object = type_value(object->child);
    if(!object || object->kind != TYPE_CLASS || !object->owned_scope)
      return vector<Binding*>();
    const auto deduplicate = [](const vector<Binding*>& input) {
      vector<Binding*> result;
      for(size_t candidate = 0; candidate < input.size(); ++candidate) {
        Binding* current = input[candidate];
        bool repeated = false;
        for(size_t existing = 0; existing < result.size(); ++existing) {
          Binding* prior = result[existing];
          if(!current || !prior || current->kind != prior->kind ||
             current->name != prior->name || current->member_index != prior->member_index ||
             current->type.get() != prior->type.get()) continue;
          if(current->member_owner && prior->member_owner &&
             current->member_owner->name == prior->member_owner->name)
            repeated = true;
          else if(!current->member_owner && !prior->member_owner)
            repeated = true;
          if(repeated) break;
        }
        if(!repeated) result.push_back(current);
      }
      return result;
    };
    vector<Binding*> direct;
    const vector<Binding*> all_direct = DirectBindings(object->owned_scope, last_component(name));
	for(size_t i = 0; i < all_direct.size(); ++i)
	  if(!all_direct[i]->hidden_friend) direct.push_back(all_direct[i]);
    if(direct.empty() && name.size() > 1 && name[0] == '~') {
      const string actual = "~" + LastComponent(object->name);
      if(actual != last_component(name)) {
        const vector<Binding*> destructor_bindings =
          DirectBindings(object->owned_scope, actual);
        for(size_t i = 0; i < destructor_bindings.size(); ++i)
          if(!destructor_bindings[i]->hidden_friend) direct.push_back(destructor_bindings[i]);
      }
    }
    // A using-declaration stores an imported copy in the derived scope.  Its
    // member_owner identifies the declaring base, so consult that typed base
    // scope again instead of letting the single imported source declaration
    // hide materialized overloads (including const/non-const member-template
    // specializations) added to the base later in collection.
    if(!direct.empty()) {
      vector<Binding*> imported;
      for(size_t i = 0; i < direct.size(); ++i) {
        TypePtr imported_owner = type_value(direct[i]->member_owner);
        if(!imported_owner || imported_owner.get() == object.get()) continue;
        if(!imported_owner->owned_scope) continue;
        const vector<Binding*> base_bindings = DirectBindings(
          imported_owner->owned_scope, last_component(name));
		for(size_t base = 0; base < base_bindings.size(); ++base) {
			if(base_bindings[base]->hidden_friend ||
				find(imported.begin(), imported.end(), base_bindings[base]) != imported.end()) continue;
			bool same_import = false;
			for(size_t existing = 0; existing < direct.size(); ++existing)
				if(direct[existing]->kind == base_bindings[base]->kind &&
					direct[existing]->type.get() == base_bindings[base]->type.get()) {
					same_import = true;
					break;
				}
			if(!same_import) imported.push_back(base_bindings[base]);
		}
      }
      if(!imported.empty()) {
        // Keep the imported declaration first: a public using-declaration may
        // deliberately re-expose a protected base member.  The base entries
        // are supplemental overloads, not replacements for that access
        // adjustment.
        for(size_t base = 0; base < imported.size(); ++base)
          if(find(direct.begin(), direct.end(), imported[base]) == direct.end())
            direct.push_back(imported[base]);
      }
      return deduplicate(direct);
    }
    const vector<TypePtr> bases = !object->direct_bases.empty() ?
      object->direct_bases : (object->direct_base ?
        vector<TypePtr>(1, object->direct_base) : vector<TypePtr>());
    vector<Binding*> inherited;
    for(size_t base_index = 0; base_index < bases.size(); ++base_index) {
      const TypePtr direct_base = type_value(bases[base_index]);
      vector<Binding*> from_base = MemberBindings(direct_base, name);
      inherited.insert(inherited.end(), from_base.begin(), from_base.end());
      const TypePtr malformed_base = direct_base;
      if(from_base.empty() && malformed_base && malformed_base->template_specialization &&
         !malformed_base->template_primary.empty()) {
        bool has_pack_marker = false;
        for(size_t argument = 0; argument < malformed_base->template_arguments.size(); ++argument)
          if(malformed_base->template_arguments[argument].find("...") != string::npos) {
            has_pack_marker = true;
            break;
          }
        if(!has_pack_marker) continue;
        const string primary = LastComponent(malformed_base->template_primary);
        map<string, vector<TypePtr> >::const_iterator candidates =
          class_types_by_name_.find(primary);
        if(candidates != class_types_by_name_.end()) {
          const auto normalize_argument = [](string value) {
            value = trim_type_name(value);
            for(size_t marker = value.find("..."); marker != string::npos;
                marker = value.find("...", marker)) value.erase(marker, 3);
            return trim_type_name(value);
          };
          for(size_t candidate = 0; candidate < candidates->second.size(); ++candidate) {
            const TypePtr& specialization = candidates->second[candidate];
            if(!specialization || specialization.get() == malformed_base.get() ||
               !specialization->template_specialization ||
               LastComponent(specialization->template_primary) != primary ||
               specialization->template_arguments.size() !=
                 malformed_base->template_arguments.size()) continue;
            bool same = true;
            for(size_t argument = 0; argument < specialization->template_arguments.size();
                ++argument)
              if(normalize_argument(specialization->template_arguments[argument]) !=
                 normalize_argument(malformed_base->template_arguments[argument])) {
                same = false;
                break;
              }
            if(!same) continue;
            vector<Binding*> materialized = MemberBindings(specialization, name);
            if(!materialized.empty()) {
              inherited.insert(inherited.end(), materialized.begin(), materialized.end());
              break;
            }
          }
        }
      }
    }
    if(!inherited.empty()) return deduplicate(inherited);
    return vector<Binding*>();
}

} // namespace cppgm_pa14_lowering
