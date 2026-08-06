#include "pa14_lowering.h"

using namespace std;

namespace cppgm_pa14_lowering {

void PA14Lowerer::EnsureConstructorBaseEntry(FunctionRecord* function)
{
    if(!function || !function->constructor || function->deleted) return;
    FunctionRecord* existing = BaseEntryFor(function);
    if(existing) {
      existing->definition = function->definition;
      existing->node = function->node;
      existing->scope = function->scope;
      existing->type = function->type;
      existing->source_type = function->source_type;
      existing->special_initializer = function->special_initializer;
      existing->default_arguments = function->default_arguments;
      existing->copy_constructor = function->copy_constructor;
      existing->move_constructor = function->move_constructor;
      existing->value_special_member = function->value_special_member;
      existing->synthesized_value_member = function->synthesized_value_member;
      existing->defaulted = function->defaulted;
      existing->deleted = function->deleted;
      existing->template_instantiation = function->template_instantiation;
      existing->explicit_specialization = function->explicit_specialization;
      existing->weak_binding = function->weak_binding;
      existing->out_of_class_definition = function->out_of_class_definition;
      existing->inherited_constructor_wrapper = function->inherited_constructor_wrapper;
      existing->template_primary = function->template_primary;
      existing->template_arguments = function->template_arguments;
      existing->construction_entry = function->member_owner &&
        function->member_owner->polymorphic &&
        HasVirtualBases(function->member_owner);
      existing->vtt_parameter = existing->construction_entry;
      BuildFunctionABI(*existing);
      return;
    }
    if(function->defaulted &&
       (!function->definition || !function->out_of_class_definition) &&
       !HasVirtualBases(function->member_owner)) return;
    FunctionRecord base_entry;
    base_entry.node = function->node;
    base_entry.scope = function->scope;
    base_entry.type = function->type;
    base_entry.source_type = function->source_type;
    base_entry.member_owner = function->member_owner;
    string base_qname = function->qualified_name;
    unsigned int overload = 0;
    for(size_t i = 0; i < functions_.size(); ++i)
      if(!functions_[i].base_entry && functions_[i].qualified_name == function->qualified_name)
        ++overload;
    if(overload > 1) {
      // Constructor overload numbering is a semantic ABI fact, not an
      // artifact of the order in which PA11 discovers the declarations.  A
      // default constructor remains the primary entry even when the copy
      // constructor was collected first; the copy/move overload is the
      // second constructor entry.
      unsigned int stable_overload = overload;
      if(function->copy_constructor || function->move_constructor) stable_overload = 2;
      else if(function->source_type && function->source_type->parameters.empty())
        stable_overload = 1;
      if(stable_overload > 1)
        base_qname += "__ov" + integer_text(static_cast<long long>(stable_overload));
    }
    base_entry.qualified_name = base_qname + "__base_entry";
    base_entry.definition = function->definition;
    base_entry.member = function->member;
    base_entry.static_member = function->static_member;
    base_entry.constructor = true;
    base_entry.implicit_constructor = function->implicit_constructor;
    base_entry.explicit_constructor = function->explicit_constructor;
    base_entry.aggregate_constructor = function->aggregate_constructor;
    base_entry.copy_constructor = function->copy_constructor;
    base_entry.move_constructor = function->move_constructor;
    base_entry.value_special_member = function->value_special_member;
    base_entry.synthesized_value_member = function->synthesized_value_member;
    base_entry.defaulted = function->defaulted;
    base_entry.deleted = function->deleted;
    base_entry.unwind_no = function->unwind_no;
    base_entry.base_entry = true;
    base_entry.base_entry_for = function->qualified_name;
    base_entry.special_initializer = function->special_initializer;
    base_entry.default_arguments = function->default_arguments;
    base_entry.template_instantiation = function->template_instantiation;
    base_entry.explicit_specialization = function->explicit_specialization;
    base_entry.weak_binding = function->weak_binding;
    base_entry.out_of_class_definition = function->out_of_class_definition;
    base_entry.inherited_constructor_wrapper = function->inherited_constructor_wrapper;
    base_entry.template_primary = function->template_primary;
    base_entry.template_arguments = function->template_arguments;
    base_entry.construction_entry = base_entry.member_owner &&
      base_entry.member_owner->polymorphic &&
      HasVirtualBases(base_entry.member_owner);
    base_entry.vtt_parameter = base_entry.construction_entry;
    BuildFunctionABI(base_entry);
    const string base_symbol = low_symbol_component(base_entry.qualified_name);
    base_entry.symbol = base_symbol;
    unsigned int suffix = 1;
    while(true) {
      bool collision = false;
      for(size_t i = 0; i < functions_.size(); ++i)
        if(functions_[i].symbol == base_entry.symbol) { collision = true; break; }
      if(!collision) break;
      base_entry.symbol = base_symbol + "__ov" + integer_text(static_cast<long long>(++suffix));
    }
    functions_.push_back(base_entry);
  }

} // namespace cppgm_pa14_lowering
