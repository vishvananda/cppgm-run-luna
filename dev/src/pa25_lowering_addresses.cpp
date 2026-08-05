#include "pa14_lowering.h"

#include <functional>
#include <string>

using namespace std;

namespace cppgm_pa14_lowering {

string PA14Lowerer::StableMemberAddressKey(const CPPGMAstNodePtr& node,
                                           Binding* member,
                                           const TypePtr& field_type) const
{
    if(!state_ || !state_->record || !member || !node || !field_type ||
       field_type->kind != TYPE_CLASS ||
       !state_->record->template_instantiation &&
       !(state_->record->member_owner &&
         state_->record->member_owner->template_specialization) ||
       field_type->template_specialization ||
       HasDefaultConstructionEffects(field_type) ||
       HasExplicitConstructor(field_type) || PA12Operator(node->value) != ".")
      return string();
    function<string(const CPPGMAstNodePtr&)> path;
    path = [&path](const CPPGMAstNodePtr& value) -> string {
      if(!value) return string();
      if(value->kind == "id-expression") return value->value;
      if(value->kind == "parenthesized-expression" && value->children.size() == 1)
        return path(value->children[0]);
      if(value->kind == "member-expression" && value->children.size() >= 2 &&
         value->children[1]) {
        const string base = path(value->children[0]);
        return base.empty() ? string() : base + "." + value->children[1]->value;
      }
      return string();
    };
    const string base = node->children.empty() ? string() :
      path(node->children[0]);
    return base.empty() ? string() : base + "." + member->name;
}

} // namespace cppgm_pa14_lowering
