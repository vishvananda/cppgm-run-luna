#include "pa14_lowering.h"

#include <sstream>
#include <string>

using namespace std;

namespace cppgm_pa14_lowering {

string PA14Lowerer::EmitFunction(FunctionRecord& function)
{
    FunctionState state(this, &function);
    state_ = &state;
    infer_cache_.clear();
    PlanFunction(state);
    // Planning uses short-lived synthetic `this` nodes for implicit member
    // accesses.  Do not let their pointer-keyed inference entries leak into
    // the emission pass when the allocator reuses those addresses.
    infer_cache_.clear();
    state.environments.clear();
    state.environments.push_back(map<string, VariablePlan*>());
    const vector<string> names = ParameterNames(function);
    const size_t hidden_count = function.hidden_virtual_bases.size();
    const size_t ordinary_count = function.type->parameters.size() >= hidden_count ?
      function.type->parameters.size() - hidden_count : 0;
    map<size_t, vector<string> > hidden_slots_by_source;
    map<size_t, vector<string> > incoming_hidden_slots_by_source;
    map<size_t, vector<pair<string, size_t> > > synthesized_hidden_by_source;
    map<size_t, size_t> hidden_ordinals;
    const bool reference_copy_like = function.source_type &&
      function.source_type->parameters.size() == 1 &&
      function.source_type->parameters[0] &&
      type_is_reference(function.source_type->parameters[0]) &&
      function.member_owner &&
      (PA12SameType(type_value(function.source_type->parameters[0]->child),
                    type_value(function.member_owner), true) ||
       LastComponent(TypeQualifiedName(type_value(function.source_type->parameters[0]->child))) ==
       LastComponent(TypeQualifiedName(function.member_owner)));
    const bool reconstruct_copy_reference = function.base_entry &&
      function.constructor && reference_copy_like;
    for(size_t hidden = 0; hidden < hidden_count; ++hidden) {
      const size_t source = hidden < function.hidden_virtual_base_sources.size() ?
        function.hidden_virtual_base_sources[hidden] : 0;
      const size_t ordinal = hidden_ordinals[source]++;
      const bool this_source = function.member && !function.static_member &&
        source == (function.indirect_result ? 1 : 0);
      const string hidden_name = names[ordinary_count + hidden];
      const TypePtr source_type = source < ordinary_count ?
        LowParameterSourceType(function, source) : TypePtr();
      const bool pointer_reference = source_type && type_is_reference(source_type) &&
        source_type->child && type_value(source_type->child) &&
        type_value(source_type->child)->kind == TYPE_POINTER;
      if(reconstruct_copy_reference && type_is_reference(source_type)) {
        // The copy base-entry ABI exposes the virtual views in its typed
        // signature, but the reference implementation reconstructs those
        // views from the ordinary source object in the body.
        continue;
      } else if(this_source || !type_is_reference(source_type) || pointer_reference) {
        state.virtual_base_hidden_by_source[names[source]].push_back("%" + hidden_name);
      } else {
        const string slot = names[source] + "__pvb" +
          integer_text(static_cast<long long>(ordinal));
        state.special_slots.push_back(slot);
        state.special_slot_types[slot] = low_type(function.type->parameters[ordinary_count + hidden]);
        hidden_slots_by_source[source].push_back(slot);
        state.virtual_base_hidden_by_source[names[source]].push_back("$" + slot);
      }
    }
    // A usage-pruned reference ABI still needs a complete typed path while
    // lowering the body.  Reconstruct omitted virtual views from the
    // ordinary reference once, in declaration order, and keep them in named
    // slots so forwarded identifiers and member projections share the same
    // state as incoming hidden arguments.
    incoming_hidden_slots_by_source = hidden_slots_by_source;
    for(size_t source = 0; source < ordinary_count && source < names.size(); ++source) {
      const TypePtr source_type = LowParameterSourceType(function, source);
      const TypePtr carrier = virtual_base_carrier(source_type);
      if(!carrier || carrier->kind != TYPE_CLASS || !source_type ||
         !type_is_reference(source_type) || !source_type->child ||
         type_value(source_type->child)->kind != TYPE_CLASS) continue;
      const vector<TypePtr> all_bases = VirtualBaseTypes(carrier);
      if(all_bases.empty()) continue;
      bool has_incoming = false;
      for(size_t hidden = 0; hidden < hidden_count; ++hidden)
        if(hidden < function.hidden_virtual_base_sources.size() &&
           function.hidden_virtual_base_sources[hidden] == source) {
          has_incoming = true;
          break;
        }
      if(!has_incoming && !reconstruct_copy_reference) continue;
      vector<string> full_values;
      for(size_t base = 0; base < all_bases.size(); ++base) {
        size_t incoming = static_cast<size_t>(-1);
        size_t incoming_ordinal = 0;
        for(size_t hidden = 0; hidden < hidden_count; ++hidden) {
          if(hidden >= function.hidden_virtual_base_sources.size() ||
             function.hidden_virtual_base_sources[hidden] != source) continue;
          if(function.hidden_virtual_bases[hidden] &&
             PA12SameType(function.hidden_virtual_bases[hidden], all_bases[base], true)) {
            incoming = hidden;
            break;
          }
          ++incoming_ordinal;
        }
        if(reconstruct_copy_reference) incoming = static_cast<size_t>(-1);
        if(incoming != static_cast<size_t>(-1)) {
          const size_t source_ordinal = [&]() {
            size_t ordinal = 0;
            for(size_t hidden = 0; hidden < incoming; ++hidden)
              if(hidden < function.hidden_virtual_base_sources.size() &&
                 function.hidden_virtual_base_sources[hidden] == source) ++ordinal;
            return ordinal;
          }();
          map<string, vector<string> >::const_iterator mapped =
            state.virtual_base_hidden_by_source.find(names[source]);
          if(mapped != state.virtual_base_hidden_by_source.end() &&
             source_ordinal < mapped->second.size())
            full_values.push_back(mapped->second[source_ordinal]);
          else full_values.push_back(string());
        } else {
          const size_t offset = [&]() {
            size_t result = 0;
            FindVirtualBaseOffset(carrier, all_bases[base], &result);
            return result;
          }();
          const string slot = names[source] + "__pvb" +
            integer_text(static_cast<long long>(base));
          state.special_slots.push_back(slot);
          state.special_slot_types[slot] = low_type(PointerTo(Fundamental("char")));
          hidden_slots_by_source[source].push_back(slot);
          synthesized_hidden_by_source[source].push_back(make_pair(slot, offset));
          full_values.push_back("$" + slot);
        }
      }
      state.virtual_base_hidden_by_source[names[source]] = full_values;
    }
    for(size_t i = 0; i < state.variables.size(); ++i)
      if(state.variables[i].parameter) {
        state.environments.back()[state.variables[i].source_name] = &state.variables[i];
        state.variables[i].slot_declared = true;
        state.slot_order.push_back(FunctionState::SlotEntry(
          false, state.variables[i].slot_name, &state.variables[i]));
        // Keep a source parameter's hidden address slots adjacent to its
        // ordinary slot.  This is both easier to inspect and preserves the
        // typed source-parameter grouping in the canonical LowIR surface.
        for(map<size_t, vector<string> >::const_iterator it = hidden_slots_by_source.begin();
            it != hidden_slots_by_source.end(); ++it)
          if(it->first < names.size() && names[it->first] == state.variables[i].source_name)
            for(size_t hidden_slot = 0; hidden_slot < it->second.size(); ++hidden_slot)
              state.slot_order.push_back(FunctionState::SlotEntry(
                true, it->second[hidden_slot], 0));
      }
    const bool entry = function.qualified_name == "main";
    ostringstream header;
    header << "function @" << function.symbol << "(";
    for(size_t i = 0; i < function.type->parameters.size(); ++i) {
      if(i != 0) header << ", ";
      header << "%" << names[i] << " : " << low_type(function.type->parameters[i]);
      if(type_is_reference(function.type->parameters[i])) header << " [pass=reference]";
      else if(function.indirect_result && i == 0)
        header << " [pass=indirect_result]";
      else if(LowParameterIsByAddress(function, i))
        header << " [pass=by_address]";
    }
    header << ") -> " << low_type(function.type->child);
    vector<string> metadata;
    if(entry) {
      metadata.push_back("role=entry");
      metadata.push_back("binding=strong");
      metadata.push_back("keep_alias=yes");
    } else {
      if(function.variadic) metadata.push_back("arity=variadic");
      if(function.effects.empty() == false) metadata.push_back("effects=" + function.effects);
      if(function.unwind_no) metadata.push_back("unwind=no");
      if(function.noreturn) metadata.push_back("return=noreturn");
      metadata.push_back(function.lambda_function ? "binding=internal" :
        (function.weak_binding ? "binding=weak" : "binding=strong"));
      if(!function.lambda_function) {
        const string object = function.object_name.empty() ? function.symbol : function.object_name;
        if(!object.empty()) metadata.push_back("object=" + object);
      }
      if(function.object_root) metadata.push_back("object_root=yes");
      if(((function.value_special_member && function.copy_constructor &&
           function.defaulted) ||
          (function.constructor && function.implicit_constructor)) &&
         IsTrivialValueStorage(function.member_owner))
        metadata.push_back("trivial_lifecycle=yes");
    }
    if(!metadata.empty()) {
      header << " [";
      for(size_t i = 0; i < metadata.size(); ++i) {
        if(i != 0) header << ", ";
        header << metadata[i];
      }
      header << "]";
    }
    header << " {";

    AddBlock("entry");
    for(size_t i = 0; i < ordinary_count; ++i) {
      if(function.indirect_result && i == 0) continue;
      VariablePlan* parameter_plan = LocalForName(names[i]);
      if(!parameter_plan || parameter_plan->parameter_address) continue;
      const bool this_parameter = function.member && !function.static_member &&
        ((!function.indirect_result && i == 0) ||
         (function.indirect_result && i == 1));
      TypePtr source_parameter = LowParameterSourceType(function, i);
      if(!this_parameter && source_parameter && type_value(source_parameter) &&
         type_value(source_parameter)->kind == TYPE_CLASS &&
         !type_is_reference(source_parameter)) {
        TypePtr source_class = type_value(source_parameter);
        const bool empty_class = !IsInitializerListType(source_class) &&
          IsEmptyBaseStorage(source_class);
        if(empty_class) continue;
        const string address = local_address(parameter_plan);
        AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(source_parameter))) +
          "x" + integer_text(static_cast<long long>(type_alignment(source_parameter))) +
          " %" + names[i] + ", " + address);
      } else {
        emit_store(function.type->parameters[i], "%" + names[i],
          StorageForVariable(*parameter_plan));
      }
      map<size_t, vector<string> >::const_iterator hidden_slots =
        incoming_hidden_slots_by_source.find(i);
      if(hidden_slots != incoming_hidden_slots_by_source.end()) {
        const TypePtr carrier = virtual_base_carrier(
          LowParameterSourceType(function, i));
        bool nested_root_views = false;
        const auto has_nested_roots = [this](const TypePtr& candidate) {
          if(!candidate || candidate->kind != TYPE_CLASS) return false;
          for(size_t base = 0; base < candidate->virtual_base_types.size(); ++base)
            if(candidate->virtual_base_types[base] &&
               HasVirtualBases(candidate->virtual_base_types[base])) return true;
          for(size_t first = 0; first < candidate->virtual_base_roots.size(); ++first) {
            if(!candidate->virtual_base_roots[first]) continue;
            if(HasVirtualBases(candidate->virtual_base_roots[first])) return true;
            for(size_t second = first + 1;
                second < candidate->virtual_base_roots.size(); ++second)
              if(candidate->virtual_base_roots[second] &&
                 SameLayoutType(candidate->virtual_base_roots[first],
                                candidate->virtual_base_roots[second])) return true;
          }
          return false;
        };
        nested_root_views = has_nested_roots(carrier) ||
          has_nested_roots(type_value(function.member_owner));
        for(size_t hidden = 0; hidden < hidden_count; ++hidden)
          if(hidden < function.hidden_virtual_base_sources.size() &&
             function.hidden_virtual_base_sources[hidden] == i &&
             hidden < function.hidden_virtual_bases.size() &&
             function.hidden_virtual_bases[hidden] &&
             HasVirtualBases(function.hidden_virtual_bases[hidden]))
            nested_root_views = true;
        // A nested virtual-base carrier presents its inner view before the
        // root view in the reference ABI.  Independent virtual roots retain
        // source declaration order.  In both cases the source ordinal is
        // recomputed from the typed hidden-source list, not from emission
        // order.
        const auto emit_hidden_store = [&](size_t hidden) {
          if(hidden >= function.hidden_virtual_base_sources.size() ||
             function.hidden_virtual_base_sources[hidden] != i) return;
          size_t ordinal = 0;
          for(size_t prior = 0; prior < hidden; ++prior)
            if(prior < function.hidden_virtual_base_sources.size() &&
               function.hidden_virtual_base_sources[prior] == i) ++ordinal;
          const string hidden_name = names[ordinary_count + hidden];
          if(ordinal < hidden_slots->second.size())
            emit_store(PointerTo(Fundamental("char")), "%" + hidden_name,
              "$" + hidden_slots->second[ordinal]);
        };
        if(nested_root_views) {
          for(size_t reverse = hidden_count; reverse > 0; --reverse)
            emit_hidden_store(reverse - 1);
        } else {
          for(size_t hidden = 0; hidden < hidden_count; ++hidden)
            emit_hidden_store(hidden);
        }
      }
      map<size_t, vector<pair<string, size_t> > >::const_iterator synthesized =
        synthesized_hidden_by_source.find(i);
      if(synthesized != synthesized_hidden_by_source.end()) {
        bool nested_synthesized = false;
        for(size_t hidden = 0; hidden < hidden_count; ++hidden)
          if(hidden < function.hidden_virtual_base_sources.size() &&
             function.hidden_virtual_base_sources[hidden] == i &&
             hidden < function.hidden_virtual_bases.size() &&
             function.hidden_virtual_bases[hidden] &&
             HasVirtualBases(function.hidden_virtual_bases[hidden])) {
            nested_synthesized = true;
            break;
          }
        const size_t synthesized_count = synthesized->second.size();
        for(size_t position = 0; position < synthesized_count; ++position) {
          const size_t hidden = nested_synthesized ?
            synthesized_count - 1 - position : position;
          const string address = new_temp();
          AddInstruction(address + " = index i8 %" + names[i] + ", " +
            integer_text(static_cast<long long>(synthesized->second[hidden].second)));
          emit_store(PointerTo(Fundamental("char")), address,
            "$" + synthesized->second[hidden].first);
        }
      }
    }
    Scope* scope = FunctionScope();
    if(function.value_special_member && (function.defaulted || function.implicit_constructor) &&
       (!function.base_entry || !HasVirtualBases(type_value(function.member_owner))))
      EmitValueSpecialMemberBody(function, scope);
    else if(function.constructor && !function.aggregate_constructor)
      EmitConstructorInitializers(function, scope);
    if(function.aggregate_constructor) EmitAggregateConstructorBody(function, scope);
    if(function.destructor) EmitDestructorVTable(function, scope);
    CPPGMAstNodePtr body = ChildOfKind(function.node, "compound-statement");
    if(!body && function.node && function.node->children.size() > 2)
      body = function.node->children[2];
    if(body && !(function.value_special_member &&
                 (function.defaulted || function.implicit_constructor))) EmitStatement(body, scope);
    if(function.destructor) EmitDestructorBody(function, scope);
    if(!state.current->terminated) {
      // Parameters and locals with automatic storage are destroyed on an
      // implicit fall-through return just as they are on an explicit return.
      // Keep this in the typed live-object path so the same reverse-order and
      // base/member cleanup rules apply in both cases.
      EmitLiveDestructors(scope);
      if(low_type(function.type->child) == "void") Terminate("return void");
      else if(!state.return_object_slot.empty()) {
        TypePtr result_type = type_value(SourceReturnType(function));
        if(!result_type || result_type->kind != TYPE_CLASS)
          throw logic_error("indirect result slot has no class type");
        AddInstruction("zeroinit " +
          integer_text(static_cast<long long>(type_size(result_type))) + "x" +
          integer_text(static_cast<long long>(type_alignment(result_type))) +
          " $" + state.return_object_slot);
        Terminate("return " + low_type(function.type->child) + " $" +
          state.return_object_slot);
      } else Terminate("return " + low_type(function.type->child) + " 0");
    }

    for(size_t i = 0; i < state.variables.size(); ++i) {
      if(state.variables[i].slot_declared) continue;
      state.variables[i].slot_declared = true;
      if(state.return_slot_plan != &state.variables[i])
        state.slot_order.push_back(FunctionState::SlotEntry(
          false, state.variables[i].slot_name, &state.variables[i]));
    }
    ostringstream out;
    out << header.str() << "\n";
    bool emitted_slot = false;
    for(size_t i = 0; i < state.slot_order.size(); ++i) {
      const FunctionState::SlotEntry& entry = state.slot_order[i];
      if(entry.special) {
        emitted_slot = true;
        out << "  slot $" << entry.name << " : " <<
          state.special_slot_types[entry.name] << "\n";
      } else if(entry.variable && !entry.variable->global &&
                entry.variable != state.return_slot_plan) {
        emitted_slot = true;
        out << "  slot $" << entry.name << " : " <<
          storage_type(entry.variable->type) << "\n";
      }
    }
    if(emitted_slot) out << "\n";
    for(size_t i = 0; i < state.blocks.size(); ++i) {
      if(i != 0) out << "\n";
      out << "  block ^" << state.blocks[i].label << ":\n";
      for(size_t j = 0; j < state.blocks[i].lines.size(); ++j)
        out << state.blocks[i].lines[j] << "\n";
    }
    out << "}";
    // A member body that observes a nested virtual base may require the
    // copy-construction entry for the virtual carrier itself.  The ordinary
    // copy function is synthesized in typed state, while only its base entry
    // is reachable through the most-derived construction ABI.
    const TypePtr emitted_owner = type_value(function.member_owner);
    const bool demand_nested_copy_entry = function.member &&
      !function.static_member && !function.constructor && !function.destructor &&
      !function.hidden_virtual_bases.empty() && emitted_owner &&
      emitted_owner->kind == TYPE_CLASS && HasVirtualBases(emitted_owner);
    state_ = 0;
    if(demand_nested_copy_entry) {
      set<const Type*> seen_carriers;
      const vector<TypePtr> virtual_bases = VirtualBaseTypes(emitted_owner);
      for(size_t base = 0; base < virtual_bases.size(); ++base) {
        const TypePtr carrier = type_value(virtual_bases[base]);
        if(!carrier || carrier->kind != TYPE_CLASS || !HasVirtualBases(carrier) ||
           !seen_carriers.insert(carrier.get()).second) continue;
        FunctionRecord* copy = EnsureImplicitCopyConstructor(carrier, false);
        if(!copy || copy->deleted) continue;
        EnsureConstructorBaseEntry(copy);
        MarkFunctionNeeded(BaseEntryFor(copy));
      }
    }
    return out.str();
  }

} // namespace cppgm_pa14_lowering
