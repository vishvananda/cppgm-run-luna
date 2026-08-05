#include "pa14_lowering.h"

#include <functional>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace cppgm_pa14_lowering {

void PA14Lowerer::FinishExceptionHandlerForReturn()
{
  if(!state_ || !state_->exception_handler_active ||
     state_->exception_handler_rethrow) return;
  AddInstruction("eh_end");
  FunctionRecord* end_catch = FindFunction(
    "__external_runtime____cxa_end_catch",
    FunctionOf(vector<TypePtr>(), false, Fundamental("void"), false));
  if(!end_catch) throw logic_error("missing exception end-catch runtime");
  MarkFunctionNeeded(end_catch);
  AddInstruction("call void @" + end_catch->symbol + "()");
  state_->exception_handler_active = false;
}

void PA14Lowerer::FinishExceptionTryForReturn()
{
  if(!state_ || state_->exception_handler_active ||
     state_->exception_handler_rethrow) return;
  while(state_->exception_try_depth > 0) {
    AddInstruction("eh_end");
    --state_->exception_try_depth;
  }
  state_->exception_routes.clear();
}

void PA14Lowerer::EmitThrow(const CPPGMAstNodePtr& expression, Scope* scope)
{
  const TypePtr pointer = PointerTo(Fundamental("void"));
  const auto runtime = [this](const string& name, const TypePtr& type) -> FunctionRecord* {
    FunctionRecord* result = FindFunction(name, type);
    if(!result) throw logic_error("missing exception runtime function: " + name);
    MarkFunctionNeeded(result);
    return result;
  };
  const TypePtr void_type = Fundamental("void");
  const string fallback_type = low_type(state_->record->type->child);
  if(!expression) {
    FunctionRecord* rethrow = runtime(
      "__external_runtime____cxa_rethrow",
      FunctionOf(vector<TypePtr>(), false, void_type, false));
    AddInstruction("call void @" + rethrow->symbol + "()");
    state_->exception_handler_rethrow = true;
    state_->exception_handler_active = false;
    Terminate("return " + fallback_type + " 0");
    return;
  }

  const ExprInfo info = Infer(expression, scope);
  TypePtr exception_type = RttiValueType(expression_value_type(info));
  if(!exception_type) throw logic_error("throw expression has no type");
  if(exception_type->kind == TYPE_CLASS && exception_type->owned_scope) {
    const vector<Binding*> destructors = DirectBindings(exception_type->owned_scope,
      "~" + LastComponent(exception_type->name));
    for(size_t i = 0; i < destructors.size(); ++i) {
      Binding* destructor = destructors[i];
      FunctionRecord* record = RecordForBinding(destructor);
      if(!record || !record->destructor || !destructor->is_member) continue;
      if(!IsAccessible(destructor, scope))
        throw logic_error("inaccessible exception destructor");
    }
  }
  demanded_exception_types_[RttiMangledName(exception_type)] = exception_type;
  demanded_thrown_types_[RttiMangledName(exception_type)] = exception_type;
  has_rtti_syntax_ = true;
  EnsureRttiType(exception_type);

  FunctionRecord* allocate = runtime(
    "__external_runtime____cxa_allocate_exception",
    FunctionOf(vector<TypePtr>(1, Fundamental("unsigned long int")), false,
      pointer, false));
  FunctionRecord* throw_function = runtime(
    "__external_runtime____cxa_throw",
    FunctionOf(vector<TypePtr>(3, pointer), false, void_type, false));
  const vector<FunctionState::TemporaryObject> cleanup =
    CaptureLiveCleanupObjects();
  const bool source_try_active = state_->exception_try_depth != 0;
  const bool wrapped_allocation = source_try_active && exception_type->kind == TYPE_CLASS &&
    !cleanup.empty() &&
    !state_->suppress_constructor_unwind;
  string allocation_slot;
  string allocation_dispatch;
  if(wrapped_allocation) {
    allocation_slot = new_special_slot("throw_alloc", "ptr");
    allocation_dispatch = new_label("call_unwind_dispatch");
    const string allocation_end = new_label("throw_alloc_unwind_end");
    BeginConstructorUnwind(cleanup, true, allocation_dispatch, allocation_end);
  }
  const string allocated = new_temp();
  ostringstream allocate_call;
  allocate_call << allocated << " = call ptr @" << allocate->symbol << "(" <<
    static_cast<unsigned long long>(type_size(exception_type)) << ")";
  AddInstruction(allocate_call.str());
  string object_address = allocated;
  if(wrapped_allocation) {
    emit_store(pointer, allocated, "$" + allocation_slot);
    FinishConstructorUnwind(scope);
    object_address = emit_load("$" + allocation_slot, pointer);
  }

  if(exception_type->kind == TYPE_CLASS) {
    const bool previous_suppression = state_->suppress_constructor_unwind;
    state_->suppress_constructor_unwind = true;
    bool transferred = false;
    try {
      transferred = EmitObjectTransferAt(exception_type, object_address,
        expression, scope, true);
    } catch(...) {
      state_->suppress_constructor_unwind = previous_suppression;
      throw;
    }
    state_->suppress_constructor_unwind = previous_suppression;
    if(!transferred) throw logic_error("no viable exception object transfer");
  } else {
    Value value = EmitValue(expression, scope, exception_type);
    value = ConvertValue(value, exception_type, false, true);
    emit_store(exception_type, value.operand, object_address);
  }

  const bool wrapped_throw = source_try_active && exception_type->kind == TYPE_CLASS &&
    !cleanup.empty() && !allocation_dispatch.empty();
  if(wrapped_throw) AddInstruction("eh_try ^" + allocation_dispatch);
  const string rtti_address = new_temp();
  const string rtti_symbol = exception_type->kind == TYPE_FUNDAMENTAL ?
    "__external_rtti__" + low_symbol_component(trim_type_name(exception_type->name)) :
    RttiSymbol(exception_type);
  AddInstruction(rtti_address + " = addr @" + rtti_symbol);
  string destructor = "0";
  if(exception_type->kind == TYPE_CLASS && HasDestructor(exception_type) &&
     DestructorHasEffects(exception_type)) {
    CollectImplicitDestructor(exception_type, exception_type->owned_scope);
    vector<Binding*> candidates = MemberBindings(exception_type,
      "~" + LastComponent(exception_type->name));
    FunctionRecord* selected = 0;
    for(size_t i = 0; i < candidates.size(); ++i) {
      FunctionRecord* candidate = RecordForBinding(candidates[i]);
      if(!candidate || !candidate->destructor || candidate->deleting_entry ||
         candidate->base_entry) continue;
      selected = candidate;
      break;
    }
    if(selected) {
      MarkFunctionNeeded(selected);
      destructor = "addr @" + selected->symbol;
    }
  }
  string destructor_operand = destructor;
  if(destructor != "0") {
    const string destructor_address = new_temp();
    AddInstruction(destructor_address + " = " + destructor);
    destructor_operand = destructor_address;
  }
  AddInstruction("call void @" + throw_function->symbol + "(" +
    object_address + ", " + rtti_address + ", " + destructor_operand + ")");
  if(wrapped_throw) AddInstruction("eh_end");
  if(source_try_active) AddInstruction("eh_end");
  Terminate("return " + fallback_type + " 0");
}

void PA14Lowerer::EmitTryBlock(const CPPGMAstNodePtr& node, Scope* scope)
{
  if(!node || node->children.empty()) return;
  struct HandlerInfo {
    CPPGMAstNodePtr node;
    TypePtr type;
    TypePtr variable_type;
    bool ellipsis;
    unsigned int selector;
    string entry;
    string body;
    string next;
    string cleanup;
    string name;
  };
  vector<HandlerInfo> handlers;
  const auto nested_handler_count = [](const CPPGMAstNodePtr& root) -> size_t {
    function<size_t(const CPPGMAstNodePtr&)> visit;
    visit = [&](const CPPGMAstNodePtr& current) -> size_t {
      if(!current) return 0;
      size_t result = 0;
      if(current->kind == "try-block") {
        for(size_t child = 1; child < current->children.size(); ++child)
          if(current->children[child] && current->children[child]->kind == "handler") ++result;
      }
      for(size_t child = 0; child < current->children.size(); ++child)
        result += visit(current->children[child]);
      return result;
    };
    return visit(root);
  };

  const string dispatch = new_label("catch_dispatch");
  const unsigned int selector_base = state_->next_exception_selector;
  const size_t nested = nested_handler_count(node->children[0]);
  for(size_t i = 1; i < node->children.size(); ++i) {
    const CPPGMAstNodePtr handler = node->children[i];
    if(!handler || handler->kind != "handler" || handler->children.empty()) continue;
    HandlerInfo info;
    info.node = handler;
    const CPPGMAstNodePtr declaration = handler->children[0];
    info.ellipsis = declaration && !declaration->children.empty() &&
      declaration->children[0] && declaration->children[0]->kind == "ellipsis";
    if(!info.ellipsis) {
      if(!declaration || declaration->children.empty())
        throw logic_error("invalid exception declaration");
      info.type = analyzer_.TypeFromSpecSeq(declaration->children[0], scope);
      info.variable_type = info.type;
      if(declaration->children.size() > 1 && declaration->children[1]) {
        info.name = declarator_name(declaration->children[1]);
        info.type = analyzer_.BuildDeclarator(declaration->children[1], info.type, scope);
        info.variable_type = info.type;
      }
      info.type = RttiValueType(info.type);
      if(!info.type) throw logic_error("exception handler has no type");
      demanded_exception_types_[RttiMangledName(info.type)] = info.type;
      has_rtti_syntax_ = true;
      EnsureRttiType(info.type);
    }
    info.selector = static_cast<unsigned int>(selector_base + nested + handlers.size());
    info.entry = new_label("catch_entry");
    handlers.push_back(info);
  }
  if(handlers.empty()) throw logic_error("try block has no handlers");
  const string try_end = new_label("try_end");
  for(size_t i = 0; i < handlers.size(); ++i)
    handlers[i].selector = static_cast<unsigned int>(selector_base + nested + i);

  vector<FunctionState::ExceptionRoute> outer_routes = state_->exception_routes;
  vector<FunctionState::ExceptionRoute> routes;
  for(size_t i = 0; i < handlers.size(); ++i)
    routes.push_back(FunctionState::ExceptionRoute(handlers[i].type,
      handlers[i].ellipsis, handlers[i].selector, handlers[i].entry));
  AddInstruction("eh_try ^" + dispatch);
  state_->exception_routes.insert(state_->exception_routes.end(),
    routes.begin(), routes.end());
  ++state_->exception_try_depth;
  Scope* body_scope = scope;
  map<const CPPGMAstNode*, Scope*>::const_iterator body_found =
    analyzer_.compound_scopes_.find(node->children[0].get());
  if(body_found != analyzer_.compound_scopes_.end()) body_scope = body_found->second;
  EmitStatement(node->children[0], body_scope);
  state_->exception_routes = outer_routes;
  if(state_->exception_try_depth > 0) --state_->exception_try_depth;
  state_->next_exception_selector = selector_base +
    static_cast<unsigned int>(nested + handlers.size());
  if(!state_->current->terminated) {
    AddInstruction("eh_end");
    Terminate("jump ^" + try_end);
  }

  for(size_t i = 0; i < handlers.size(); ++i) {
    handlers[i].body = new_label("catch_body");
    handlers[i].next = new_label("catch_next");
    handlers[i].cleanup = new_label("catch_cleanup");
  }
  AddBlock(dispatch);
  for(size_t i = 0; i < handlers.size(); ++i) {
    if(handlers[i].ellipsis) AddInstruction("eh_catch_all, " +
      integer_text(static_cast<long long>(handlers[i].selector)));
    else {
      const TypePtr type = handlers[i].type;
      const string rtti = type && type->kind == TYPE_FUNDAMENTAL ?
        "__external_rtti__" + low_symbol_component(trim_type_name(type->name)) :
        RttiSymbol(type);
      AddInstruction("eh_catch @" + rtti + ", " +
        integer_text(static_cast<long long>(handlers[i].selector)));
    }
  }
  Terminate("jump ^" + handlers[0].entry);

  const TypePtr pointer = PointerTo(Fundamental("void"));
  for(size_t i = 0; i < handlers.size(); ++i) {
    HandlerInfo& handler = handlers[i];
    AddBlock(handler.entry);
    const string exception = new_temp();
    AddInstruction(exception + " = exception ptr");
    const string selector = new_temp();
    AddInstruction(selector + " = exception_selector i32");
    const string match = new_temp();
    AddInstruction(match + " = cmp eq i32 " + selector + ", " +
      integer_text(static_cast<long long>(handler.selector)));
    Terminate("branch " + match + ", ^" + handler.body + ", ^" + handler.next);

    AddBlock(handler.body);
    FunctionRecord* begin_catch = FindFunction(
      "__external_runtime____cxa_begin_catch",
      FunctionOf(vector<TypePtr>(1, pointer), false, pointer, false));
    if(!begin_catch) throw logic_error("missing exception begin-catch runtime");
    MarkFunctionNeeded(begin_catch);
    const string caught = new_temp();
    AddInstruction(caught + " = call ptr @" + begin_catch->symbol + "(" + exception + ")");
    VariablePlan* catch_plan = 0;
    if(!handler.name.empty()) {
      const string catch_slot = new_special_slot("catch", "ptr");
      AddInstruction("store ptr " + caught + ", $" + catch_slot);
      state_->environments.push_back(map<string, VariablePlan*>());
      catch_plan = AddVariablePlan(handler.name, handler.variable_type,
        CPPGMAstNodePtr(), CPPGMAstNodePtr());
      if(catch_plan) {
        catch_plan->slot_declared = true;
        state_->slot_order.push_back(FunctionState::SlotEntry(
          false, catch_plan->slot_name, catch_plan));
      }
    }
    AddInstruction("eh_cleanup ^" + handler.cleanup);
    if(catch_plan) {
      if(type_is_reference(catch_plan->type))
        emit_store(catch_plan->type, caught, StorageForVariable(*catch_plan));
      else {
        const string value = emit_load(caught, type_value(catch_plan->type));
        emit_store(catch_plan->type, value, StorageForVariable(*catch_plan));
      }
    }
    const bool previous_handler = state_->exception_handler_active;
    const bool previous_rethrow = state_->exception_handler_rethrow;
    state_->exception_handler_active = true;
    state_->exception_handler_rethrow = false;
    Scope* handler_scope = scope;
    if(handler.node->children.size() > 1) {
      map<const CPPGMAstNode*, Scope*>::const_iterator found =
        analyzer_.compound_scopes_.find(handler.node->children[1].get());
      if(found != analyzer_.compound_scopes_.end()) handler_scope = found->second;
      EmitStatement(handler.node->children[1], handler_scope);
    }
    if(!state_->current->terminated) {
      FinishExceptionHandlerForReturn();
      Terminate("jump ^" + try_end);
    }
    state_->exception_handler_active = previous_handler;
    state_->exception_handler_rethrow = previous_rethrow;
    if(catch_plan) state_->environments.pop_back();

    AddBlock(handler.cleanup);
    if(!outer_routes.empty()) {
      const FunctionState::ExceptionRoute& route = outer_routes.back();
      if(route.ellipsis) AddInstruction("eh_catch_all, " +
        integer_text(static_cast<long long>(route.selector)));
      else {
        const TypePtr type = route.type;
        const string rtti = type && type->kind == TYPE_FUNDAMENTAL ?
          "__external_rtti__" + low_symbol_component(trim_type_name(type->name)) :
          RttiSymbol(type);
        AddInstruction("eh_catch @" + rtti + ", " +
          integer_text(static_cast<long long>(route.selector)));
      }
    }
    FunctionRecord* end_catch = FindFunction(
      "__external_runtime____cxa_end_catch",
      FunctionOf(vector<TypePtr>(), false, Fundamental("void"), false));
    if(!end_catch) throw logic_error("missing exception end-catch runtime");
    MarkFunctionNeeded(end_catch);
    AddInstruction("call void @" + end_catch->symbol + "()");
    AddInstruction("eh_end");
    if(!outer_routes.empty()) {
      AddInstruction("eh_end");
      Terminate("jump ^" + outer_routes.back().entry);
    } else Terminate("resume");

    AddBlock(handler.next);
    if(!outer_routes.empty()) {
      EmitCleanupObjects(CaptureLiveCleanupObjects(), scope);
      Terminate("jump ^" + outer_routes.back().entry);
    } else Terminate("resume");
  }
  AddBlock(try_end);
}

} // namespace cppgm_pa14_lowering
