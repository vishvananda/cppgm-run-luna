#include "pa14_lowering.h"

#include <map>
#include <string>
#include <vector>

using namespace std;

namespace cppgm_pa14_lowering {

void PA14Lowerer::RegisterTemporaryObject(const TypePtr& type, const string& address,
                                          bool force_empty,
                                          bool construction_no_throw)
{
    if(!state_ || !type || address.empty()) return;
    if(!HasDestructor(type)) return;
  state_->temporary_objects.push_back(
      FunctionState::TemporaryObject(type, address, force_empty,
        construction_no_throw));
}

void PA14Lowerer::EmitTemporaryDestructors(size_t mark, Scope* scope)
{
    if(!state_) return;
    for(size_t i = state_->temporary_objects.size(); i > mark; --i) {
      const FunctionState::TemporaryObject& temporary = state_->temporary_objects[i - 1];
      (void)EmitDestructorAt(temporary.type, temporary.address, scope,
        temporary.force_empty);
    }
    state_->temporary_objects.resize(mark);
}

vector<PA14Lowerer::FunctionState::TemporaryObject>
PA14Lowerer::CaptureLiveCleanupObjects(const string& exclude_address)
{
    vector<FunctionState::TemporaryObject> result;
    if(!state_) return result;
    for(size_t i = state_->temporary_objects.size(); i > 0; --i) {
      const FunctionState::TemporaryObject& temporary =
        state_->temporary_objects[i - 1];
      // A temporary whose destructor has no observable body/member/base
      // effects does not need an EH region merely because its type has a
      // declared destructor.  Keep the explicit force-empty marker intact:
      // PA25's managed temporary paths deliberately preserve those calls.
      if(!temporary.force_empty && !DestructorHasEffects(temporary.type))
        continue;
      result.push_back(temporary);
    }

    for(size_t i = state_->variables.size(); i > 0; --i) {
      VariablePlan& variable = state_->variables[i - 1];
      if(&variable == state_->constructing_variable) continue;
      bool live = false;
      for(size_t environment = 0;
          environment < state_->environments.size() && !live; ++environment) {
        const map<string, VariablePlan*>& bindings = state_->environments[environment];
        for(map<string, VariablePlan*>::const_iterator binding = bindings.begin();
            binding != bindings.end(); ++binding)
          if(binding->second == &variable) { live = true; break; }
      }
      if(!live || type_is_reference(variable.type)) continue;
      TypePtr object_type = type_value(variable.type);
      if(!object_type) continue;
      if(object_type->kind == TYPE_CLASS) {
        if(!HasDestructor(object_type) ||
           (!variable.parameter && !DestructorHasEffects(object_type))) continue;
        // A local address is deliberately resolved in the cleanup block, not
        // while capturing the snapshot.  Resolving it here would add an
        // observable address calculation to the normal path and would make
        // the exceptional path unable to reproduce the same typed address.
        result.push_back(FunctionState::TemporaryObject(
          object_type, string(), variable.parameter, false, &variable));
        continue;
      }
      TypePtr element_type = object_type->child ? type_value(object_type->child) : TypePtr();
      if(object_type->kind != TYPE_ARRAY || object_type->bound < 0 ||
         !element_type || element_type->kind != TYPE_CLASS ||
         !HasDestructor(element_type) ||
         (!variable.parameter && !DestructorHasEffects(element_type))) continue;
      // Array cleanup is emitted by the existing live-destructor path.  Keep
      // the array out of this scalar snapshot until the call-boundary helper
      // grows a typed element projection; the scalar/object cases are the
      // PA25 temporary boundary handled here.
    }
    (void)exclude_address;
    return result;
}

void PA14Lowerer::BeginConstructorUnwind(
  const vector<FunctionState::TemporaryObject>& cleanup, bool call_context,
  const string& dispatch, const string& end)
{
    if(!state_ || state_->constructor_unwind_active) return;
    state_->constructor_unwind_cleanup = cleanup;
    state_->constructor_unwind_call = call_context;
    state_->constructor_unwind_shared_dispatch = false;
    state_->constructor_unwind_dispatch = dispatch;
    // A source try-handler is part of the dispatch destination.  Reusing a
    // cleanup-only cache entry across two lexical handlers would preserve the
    // destructor list but send the exception to the wrong catch entry.
    for(size_t cached = 0; cached < state_->unwind_dispatch_cache.size() &&
        state_->exception_routes.empty(); ++cached) {
      const vector<FunctionState::TemporaryObject>& cached_cleanup =
        state_->unwind_dispatch_cache[cached].cleanup;
      if(cached_cleanup.size() != cleanup.size()) continue;
      bool same_cleanup = true;
      for(size_t object = 0; object < cleanup.size(); ++object) {
        const FunctionState::TemporaryObject& left = cleanup[object];
        const FunctionState::TemporaryObject& right = cached_cleanup[object];
        if(left.type.get() != right.type.get() || left.address != right.address ||
           left.force_empty != right.force_empty ||
           left.construction_unwind_no != right.construction_unwind_no ||
           left.variable != right.variable) {
          same_cleanup = false;
          break;
        }
      }
      if(same_cleanup && !cached_cleanup.empty()) {
        state_->constructor_unwind_dispatch =
          state_->unwind_dispatch_cache[cached].dispatch;
        state_->constructor_unwind_shared_dispatch = true;
        break;
      }
    }
    if(state_->constructor_unwind_dispatch.empty())
      state_->constructor_unwind_dispatch = new_label("call_unwind_dispatch");
    state_->constructor_unwind_end = state_->constructor_unwind_shared_dispatch ?
      string() : (end.empty() ? new_label("call_unwind_end") : end);
    AddInstruction("eh_try ^" + state_->constructor_unwind_dispatch);
    state_->constructor_unwind_active = true;
}

void PA14Lowerer::FinishConstructorUnwind(Scope* scope, bool use_current_cleanup)
{
    if(!state_ || !state_->constructor_unwind_active) return;
    const vector<FunctionState::TemporaryObject> cleanup = use_current_cleanup ?
      CaptureLiveCleanupObjects() : state_->constructor_unwind_cleanup;
    const string dispatch = state_->constructor_unwind_dispatch;
    const string end = state_->constructor_unwind_end;
    const bool shared_dispatch = state_->constructor_unwind_shared_dispatch;
    const vector<FunctionState::ExceptionRoute> exception_routes =
      state_->exception_routes;
    state_->constructor_unwind_active = false;
    state_->constructor_unwind_call = false;
    state_->constructor_unwind_shared_dispatch = false;
    state_->constructor_unwind_cleanup.clear();
    state_->constructor_unwind_dispatch.clear();
    state_->constructor_unwind_end.clear();
    AddInstruction("eh_end");
    if(shared_dispatch) return;
    Terminate("jump ^" + end);
    AddBlock(dispatch);
    if(!exception_routes.empty()) {
      const FunctionState::ExceptionRoute& route = exception_routes.back();
      if(route.ellipsis) AddInstruction("eh_catch_all, " +
        integer_text(static_cast<long long>(route.selector)));
      else {
        const string rtti = route.type && route.type->kind == TYPE_FUNDAMENTAL ?
          "__external_rtti__" + low_symbol_component(trim_type_name(route.type->name)) :
          RttiSymbol(route.type);
        AddInstruction("eh_catch @" + rtti + ", " +
          integer_text(static_cast<long long>(route.selector)));
      }
      AddInstruction("eh_cleanup");
      EmitCleanupObjects(cleanup, scope);
      AddInstruction("eh_end");
      AddInstruction("eh_end");
      Terminate("jump ^" + route.entry);
    } else {
      EmitCleanupObjects(cleanup, scope);
      Terminate("resume");
    }
    AddBlock(end);
    if(!cleanup.empty())
      state_->unwind_dispatch_cache.push_back(
        FunctionState::UnwindDispatchCacheEntry(cleanup, dispatch));
}

void PA14Lowerer::StartPendingReferenceUnwind(Scope* scope)
{
    (void)scope;
    if(!state_ || !state_->pending_reference_unwind_start) return;
    const vector<FunctionState::TemporaryObject> cleanup =
      CaptureLiveCleanupObjects();
    if(!cleanup.empty())
      BeginConstructorUnwind(cleanup, false,
        state_->pending_reference_unwind_dispatch,
        state_->pending_reference_unwind_end);
    state_->pending_reference_unwind_start = false;
    state_->pending_reference_unwind_dispatch.clear();
    state_->pending_reference_unwind_end.clear();
}

void PA14Lowerer::EmitCleanupObjects(
  const vector<FunctionState::TemporaryObject>& objects, Scope* scope)
{
    for(size_t i = 0; i < objects.size(); ++i) {
      const FunctionState::TemporaryObject& object = objects[i];
      string address = object.address;
      if(object.variable) address = local_address(object.variable);
      if(!address.empty())
        (void)EmitDestructorAt(object.type, address, scope, object.force_empty);
    }
}

bool PA14Lowerer::BeginAggregateMemberUnwind(const TypePtr& member_type)
{
    if(!state_ || !member_type || member_type->kind != TYPE_CLASS ||
       state_->constructor_unwind_active || state_->suppress_constructor_unwind ||
       !HasConstructor(member_type)) return false;
    const vector<FunctionState::TemporaryObject> cleanup =
      CaptureLiveCleanupObjects();
    if(cleanup.empty()) return false;
    BeginConstructorUnwind(cleanup, true);
    return true;
}

void PA14Lowerer::FinishAggregateMemberUnwind(bool started, Scope* scope)
{
    if(started && state_ && state_->constructor_unwind_active)
      FinishConstructorUnwind(scope);
}

bool PA14Lowerer::HasLiveCleanupObjects(const string& exclude_address)
{
    return !CaptureLiveCleanupObjects(exclude_address).empty();
}


} // namespace cppgm_pa14_lowering
