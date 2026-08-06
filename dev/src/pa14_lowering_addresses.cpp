#include "pa14_lowering.h"

#include <functional>
#include <map>
#include <set>

using namespace std;

namespace cppgm_pa14_lowering {

string PA14Lowerer::EmitMemberAddress(const CPPGMAstNodePtr& node, Scope* scope,
                                      bool reference_projection)
{
    ExprInfo object_info;
	Binding* member = MemberBinding(node, scope, &object_info);
	if(!member) throw logic_error("unknown member");
    if(member->kind == BIND_FUNCTION) {
      if(member->is_static) {
        FunctionRecord* function = RecordForBinding(member);
        if(function) return function_address(function);
      }
      throw logic_error("member function is not an lvalue");
    }
    if(member->is_static) {
      GlobalRecord* global = EnsureStaticMemberStorage(member);
      if(!global) throw logic_error("static member has no storage");
      return global_address(global);
    }
    if(member->member_index == static_cast<size_t>(-1) || !member->member_owner ||
       member->member_index >= member->member_owner->class_members.size())
      throw logic_error("member has no layout record");
    const ClassMemberInfo& fact = member->member_owner->class_members[member->member_index];
    const string op = PA12Operator(node->value);
    TypePtr field_type = type_value(fact.type);
    bool hidden_base_projection = false;
    const auto use_hidden_base_projection = [&](const TypePtr& derived,
                                                 string* address) -> bool {
      if(!state_ || !derived || derived->kind != TYPE_CLASS || !address) return false;
      size_t virtual_index = 0;
      size_t nested_offset_probe = 0;
      bool found = false;
      for(; virtual_index < derived->virtual_base_types.size(); ++virtual_index)
        if(derived->virtual_base_types[virtual_index] &&
           (SameLayoutType(derived->virtual_base_types[virtual_index], member->member_owner) ||
            (virtual_index < derived->virtual_base_roots.size() &&
             derived->virtual_base_roots[virtual_index] &&
             FindVirtualBaseOffset(derived->virtual_base_roots[virtual_index],
               member->member_owner, &nested_offset_probe)))) {
          found = true;
          break;
        }
      if(!found) return false;
      string source_name;
      if(node->children.size() > 0 && node->children[0]) {
        source_name = node->children[0]->kind == "keyword-literal" ?
          (PA12Operator(node->children[0]->value) == "this" ? "this" : string()) :
          (node->children[0]->kind == "id-expression" ? node->children[0]->value : string());
      }
      if(source_name.empty()) return false;
      map<string, vector<string> >::const_iterator hidden =
        state_->virtual_base_hidden_by_source.find(source_name);
      if(hidden == state_->virtual_base_hidden_by_source.end() ||
         virtual_index >= hidden->second.size()) return false;
      *address = hidden->second[virtual_index];
      if(!address->empty() && (*address)[0] == '$')
        *address = emit_load(*address, PointerTo(Fundamental("char")));
      size_t relative = 0;
      const TypePtr root = virtual_index < derived->virtual_base_roots.size() &&
        derived->virtual_base_roots[virtual_index] ?
        derived->virtual_base_roots[virtual_index] : derived->virtual_base_types[virtual_index];
      if(root && !PA12SameType(root, member->member_owner, true) &&
         FindVirtualBaseOffset(root, member->member_owner, &relative)) {
        const string nested = new_temp();
        AddInstruction(nested +
          " = index i8 [projection=base_subobject] " + *address + ", " +
          integer_text(static_cast<long long>(relative)));
        // The hidden operand names the physical root view.  Keep the
        // intermediate nested-base projection visible as well as the
        // final zero-offset view: callers may use either typed subobject
        // boundary when lowering a member of a virtual base reached through
        // another virtual base.
        const string projected = new_temp();
        AddInstruction(projected +
          " = index i8 [projection=base_subobject] " + nested + ", 0");
        *address = projected;
        return true;
      }
      const string projected = new_temp();
      AddInstruction(projected +
        " = index i8 [projection=base_subobject] " + *address + ", 0");
      *address = projected;
      return true;
    };
    const string stable_key = StableMemberAddressKey(node, member, field_type);
    if(!stable_key.empty() && state_) {
      map<string, string>::const_iterator cached =
        state_->stable_member_addresses.find(stable_key);
      if(cached != state_->stable_member_addresses.end()) return cached->second;
    }
    string base;
    const bool post_call_member = node->children.size() > 0 && node->children[0] &&
      node->children[0]->kind == "call-expression";
    if(state_ && post_call_member) state_->post_call_unwind_requested = true;
    if(op == "->") {
      TypePtr object = expression_value_type(object_info);
      if(!object || object->kind != TYPE_POINTER) throw logic_error("arrow requires a pointer to class");
      object = type_value(object->child);
      hidden_base_projection = use_hidden_base_projection(object, &base);
      if(!hidden_base_projection) base = EmitValue(node->children[0], scope).operand;
    } else {
      TypePtr object = expression_value_type(object_info);
      const size_t object_temporary_mark = state_ ?
        state_->temporary_objects.size() : 0;
      object = type_value(object);
      if(object && object->kind == TYPE_POINTER) object = type_value(object->child);
      hidden_base_projection = use_hidden_base_projection(object, &base);
      if(!hidden_base_projection) base = EmitAddress(node->children[0], scope);
      // A class prvalue used as a member object creates a temporary before
      // the member projection itself.  If this projection is an argument to
      // an enclosing call, its cleanup region must already cover the field
      // address/load; leave the typed call context open for EmitChosenCall
      // to close around that enclosing call.
      if(state_ && !state_->constructor_unwind_active &&
         !state_->suppress_constructor_unwind &&
         !state_->defer_temporary_cleanup &&
         state_->temporary_objects.size() > object_temporary_mark &&
         object_info.category == "prvalue") {
        const vector<FunctionState::TemporaryObject> cleanup =
          CaptureLiveCleanupObjects();
        if(!cleanup.empty()) {
          BeginConstructorUnwind(cleanup, true);
          state_->pending_call_argument_context = true;
        }
      }
    }
    if(state_ && post_call_member) state_->post_call_unwind_requested = false;
    TypePtr object = expression_value_type(object_info);
    if(op == "->") object = object && object->kind == TYPE_POINTER ?
      type_value(object->child) : TypePtr();
    else if(object && object->kind == TYPE_POINTER) object = type_value(object->child);
    bool projected_injected_storage = false;
    if(member->injected_member && member->injected_owner &&
       (!object || !PA12SameType(object, member->injected_owner, true))) {
      bool found_injected_storage = false;
      if(object && object->kind == TYPE_CLASS) {
        for(size_t i = 0; i < object->class_members.size(); ++i) {
          const ClassMemberInfo& outer = object->class_members[i];
          if(!outer.name.empty() && outer.type) continue;
          if(outer.type && PA12SameType(type_value(outer.type),
                                        member->injected_owner, true)) {
            found_injected_storage = true;
            if(outer.offset != 0) {
              const string adjusted = new_temp();
              AddInstruction(adjusted + " = index i8 [projection=field] " + base + ", " +
                integer_text(outer.offset));
              base = adjusted;
              projected_injected_storage = true;
            }
            break;
          }
        }
      }
      if(!found_injected_storage)
        throw logic_error("anonymous member has no storage record");
    } else if(!hidden_base_projection) {
      // A reference returned from a call carries a virtual-base view through
      // an ABI boundary.  Preserve the final zero-offset subobject
      // projection when selecting a member from that returned view; a direct
      // lvalue already has its canonical base projection from EmitAddress.
      CPPGMAstNodePtr returned_expression = node->children.size() > 0 ?
        node->children[0] : CPPGMAstNodePtr();
      while(returned_expression &&
            returned_expression->kind == "parenthesized-expression" &&
            returned_expression->children.size() == 1 &&
            returned_expression->children[0])
        returned_expression = returned_expression->children[0];
      const bool returned_virtual_view = returned_expression &&
        (returned_expression->kind == "call-expression" ||
         returned_expression->kind == "binary-expression") &&
        object && object->kind == TYPE_CLASS;
      base = AdjustBaseAddress(base, object, member->member_owner,
        returned_virtual_view);
    }
    ApplyCapturedThisProjection(node, op, &base);
    if(state_ && state_->post_call_unwind_pending)
      AddInstruction("eh_try ^" + state_->post_call_unwind_dispatch);
    // An injected member of an anonymous union uses the union storage itself
    // when its layout offset is zero.  The injected binding carries the
    // outer member's offset in the projection above; applying a second
    // zero-offset field projection changes the canonical LowIR shape and,
    // more importantly, obscures that this is the union object address.
    const TypePtr injected_owner = type_value(member->injected_owner);
    if(member->injected_member && injected_owner && injected_owner->kind == TYPE_CLASS &&
       injected_owner->is_union && fact.offset == 0 &&
       projected_injected_storage) {
      if(!stable_key.empty() && state_) state_->stable_member_addresses[stable_key] = base;
      return base;
    }
    const string result = new_temp();
    const bool raw_bit_field = IsBitField(member) && op == ".";
    const bool reference_field = reference_projection && type_is_reference(fact.type);
    AddInstruction(result + " = index i8 " +
      (raw_bit_field ? string() :
       (reference_field ? "[projection=reference_field] " : "[projection=field] ")) +
      base + ", " +
      integer_text(fact.offset));
    if(!stable_key.empty() && state_) state_->stable_member_addresses[stable_key] = result;
    return result;
}
string PA14Lowerer::AdjustBaseAddress(const string& base, const TypePtr& raw_derived,
                                      const TypePtr& target,
                                      bool project_base_path)
{
    TypePtr derived = type_value(raw_derived);
    TypePtr wanted = type_value(target);
    if(!derived || !wanted || PA12SameType(derived, wanted, true)) return base;
    if(derived->kind != TYPE_CLASS || wanted->kind != TYPE_CLASS)
      throw logic_error("member owner is not a base class");
    if(!IsDerivedFrom(derived, wanted))
      throw logic_error("member owner is not a base class");
    vector<size_t> path;
    vector<TypePtr> path_types;
    vector<bool> path_virtual;
    set<const Type*> visited;
    function<bool(const TypePtr&)> find_base =
      [&](const TypePtr& current) {
        if(!current || !visited.insert(current.get()).second) return false;
        if(PA12SameType(current, wanted, true)) return true;
        if(!current->direct_bases.empty()) {
          for(size_t i = 0; i < current->direct_bases.size(); ++i) {
            const size_t base_offset = i < current->direct_base_offsets.size() ?
              current->direct_base_offsets[i] : (i == 0 ? current->direct_base_offset : 0);
            path.push_back(base_offset);
            path_types.push_back(type_value(current->direct_bases[i]));
            path_virtual.push_back(IsVirtualDirectBase(current, i));
            if(find_base(type_value(current->direct_bases[i]))) return true;
            path.pop_back();
            path_types.pop_back();
            path_virtual.pop_back();
          }
        } else if(current->direct_base) {
          path.push_back(current->direct_base_offset);
          path_types.push_back(type_value(current->direct_base));
          path_virtual.push_back(IsVirtualDirectBase(current, 0));
          if(find_base(type_value(current->direct_base))) return true;
          path.pop_back();
          path_types.pop_back();
          path_virtual.pop_back();
        }
        return false;
    };
    if(!find_base(derived)) throw logic_error("member owner is not a base class");
    string adjusted = base;
    size_t offset = 0;
    bool rooted_virtual = false;
    bool has_virtual_path = false;
    for(size_t i = 0; i < path_virtual.size(); ++i)
      if(path_virtual[i]) { has_virtual_path = true; break; }
    // A nested virtual-base path is not laid out by adding the offsets of
    // each intermediate view.  The complete object owns one physical slot
    // for the final virtual base, so use that typed layout fact directly
    // (E -> H -> B, for example, names E's B slot rather than H+ B's local
    // offset).  Construction/base-entry functions may instead provide the
    // slot as a hidden view; prefer that operand when it is available.
    size_t direct_virtual_offset = 0;
    if(has_virtual_path && FindVirtualBaseOffset(derived, wanted,
                                                 &direct_virtual_offset)) {
      size_t virtual_index = 0;
      for(; virtual_index < derived->virtual_base_types.size(); ++virtual_index)
        if(derived->virtual_base_types[virtual_index] &&
           SameLayoutType(derived->virtual_base_types[virtual_index], wanted))
          break;
      bool used_hidden = false;
      if(state_) {
        map<string, vector<string> >::const_iterator hidden;
        bool have_hidden = false;
        map<string, vector<string> >::const_iterator by_operand =
          state_->virtual_base_hidden_by_operand.find(base);
        if(by_operand != state_->virtual_base_hidden_by_operand.end()) {
          hidden = by_operand;
          have_hidden = true;
        } else if(base == "%this") {
          hidden = state_->virtual_base_hidden_by_source.find("this");
          have_hidden = hidden != state_->virtual_base_hidden_by_source.end();
        } else {
          for(map<string, vector<string> >::const_iterator source =
                state_->virtual_base_hidden_by_source.begin();
              source != state_->virtual_base_hidden_by_source.end(); ++source)
            if("$" + source->first == base) {
              hidden = source;
              have_hidden = true;
              break;
            }
        }
        if(have_hidden) {
          size_t hidden_index = static_cast<size_t>(-1);
          size_t hidden_relative = 0;
          for(size_t candidate = 0; candidate < derived->virtual_base_types.size(); ++candidate) {
            const TypePtr root = candidate < derived->virtual_base_roots.size() &&
              derived->virtual_base_roots[candidate] ?
              derived->virtual_base_roots[candidate] : derived->virtual_base_types[candidate];
            if(!root) continue;
            size_t relative = 0;
            if(SameLayoutType(root, wanted) ||
               FindVirtualBaseOffset(root, wanted, &relative)) {
              hidden_index = candidate;
              hidden_relative = relative;
              break;
            }
          }
          if(hidden_index != static_cast<size_t>(-1) &&
             hidden_index < hidden->second.size() &&
             !hidden->second[hidden_index].empty()) {
            adjusted = hidden->second[hidden_index];
            if(adjusted[0] == '$')
              adjusted = emit_load(adjusted, PointerTo(Fundamental("char")));
            if(hidden_relative != 0) {
              const string nested = new_temp();
              AddInstruction(nested + " = index i8 " + adjusted + ", " +
                integer_text(static_cast<long long>(hidden_relative)));
              adjusted = nested;
            }
            offset = 0;
            used_hidden = true;
            rooted_virtual = true;
          }
        }
      }
      if(!used_hidden) {
        offset = direct_virtual_offset;
        rooted_virtual = true;
        const bool construction_context = state_ && state_->record &&
          (state_->record->constructor || state_->record->construction_entry);
        if(derived->polymorphic && !construction_context &&
           virtual_index < derived->virtual_base_types.size()) {
          const string vptr = emit_load(base, PointerTo(Fundamental("char")));
          const string slot = new_temp();
          AddInstruction(slot + " = index i8 " + vptr + ", " +
            integer_text(-24 - static_cast<long long>(virtual_index * 8)));
          const string dynamic_offset = emit_load(slot,
            Fundamental("long int"));
          const string projected = new_temp();
          AddInstruction(projected + " = index i8 " + base + ", " + dynamic_offset);
          adjusted = projected;
          offset = 0;
        }
      }
    }
    for(size_t i = 0; i < path.size(); ++i) {
      if(rooted_virtual && has_virtual_path) break;
      if(path_virtual[i]) {
        size_t root_offset = 0;
        size_t root_index = 0;
        bool root_view = false;
        for(; root_index < derived->virtual_base_types.size(); ++root_index)
          if(derived->virtual_base_types[root_index] &&
             SameLayoutType(derived->virtual_base_types[root_index], path_types[i]) &&
             (root_index >= derived->virtual_base_roots.size() ||
              !derived->virtual_base_roots[root_index] ||
              SameLayoutType(derived->virtual_base_roots[root_index], path_types[i]))) {
            root_view = true;
            break;
          }
        if(root_view && FindVirtualBaseOffset(derived, path_types[i], &root_offset)) {
          rooted_virtual = true;
          bool used_hidden = false;
          if(state_) {
            map<string, vector<string> >::const_iterator hidden =
              state_->virtual_base_hidden_by_operand.find(base);
            if(hidden != state_->virtual_base_hidden_by_operand.end()) {
              size_t virtual_index = 0;
              bool found_index = false;
              for(; virtual_index < derived->virtual_base_types.size(); ++virtual_index)
                if(derived->virtual_base_types[virtual_index] &&
                   SameLayoutType(derived->virtual_base_types[virtual_index], path_types[i]) &&
                   (virtual_index >= derived->virtual_base_roots.size() ||
                    !derived->virtual_base_roots[virtual_index] ||
                    SameLayoutType(derived->virtual_base_roots[virtual_index], path_types[i]))) {
                  found_index = true;
                  break;
                }
              if(found_index && virtual_index < hidden->second.size()) {
                adjusted = hidden->second[virtual_index];
                if(adjusted.size() > 0 && adjusted[0] == '$')
                  adjusted = emit_load(adjusted, PointerTo(Fundamental("char")));
                offset = 0;
                used_hidden = true;
              }
            }
          }
          if(!used_hidden) {
            adjusted = base;
            offset = root_offset;
            // A virtual-base address in an already-constructed polymorphic
            // object is read from the active vtable.  Constructor entries
            // intentionally use their static typed layout: their vptr still
            // names a construction table while the base is being initialized.
            const bool construction_context = state_ && state_->record &&
              (state_->record->constructor || state_->record->construction_entry);
            if(derived->polymorphic && !construction_context) {
              const string vptr = emit_load(base,
                PointerTo(Fundamental("char")));
              const string slot = new_temp();
              const long long vbase_slot = -24 -
                static_cast<long long>(root_index * 8);
              AddInstruction(slot + " = index i8 " + vptr + ", " +
                integer_text(vbase_slot));
              const string dynamic_offset = emit_load(slot,
                Fundamental("long int"));
              adjusted = base;
              offset = 0;
              const string projected = new_temp();
              AddInstruction(projected +
                " = index i8 " + adjusted + ", " + dynamic_offset);
              adjusted = projected;
            }
          }
        } else {
          offset += path[i];
        }
      } else {
        offset += path[i];
      }
    }
    if(!project_base_path) {
      const string projected = new_temp();
      AddInstruction(projected + " = index i8 [projection=base_subobject] " + adjusted + ", " +
        integer_text(static_cast<long long>(offset)));
      return projected;
    }
    if(has_virtual_path && !rooted_virtual) {
      const string projected = new_temp();
      AddInstruction(projected + " = index i8 " + base + ", " +
        integer_text(static_cast<long long>(offset)));
      const string view = new_temp();
      AddInstruction(view +
        " = index i8 [projection=base_subobject] " + projected + ", 0");
      return view;
    }
    if(rooted_virtual) {
      const string projected = new_temp();
      // Keep the virtual-base displacement as a raw address calculation;
      // the typed subobject boundary is the following zero projection.
      AddInstruction(projected + " = index i8 " + adjusted + ", " +
        integer_text(static_cast<long long>(offset)));
      const string view = new_temp();
      AddInstruction(view +
        " = index i8 [projection=base_subobject] " + projected + ", 0");
      return view;
    }
    adjusted = base;
    for(size_t i = 0; i < path.size(); ++i) {
      const string projected = new_temp();
      AddInstruction(projected + " = index i8 [projection=base_subobject] " + adjusted + ", " +
        integer_text(static_cast<long long>(path[i])));
      adjusted = projected;
    }
    return adjusted;
  }

} // namespace cppgm_pa14_lowering
