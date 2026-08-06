#include "pa14_lowering.h"

#include <functional>
#include <cctype>
#include <set>

using namespace std;

namespace cppgm_pa14_lowering {

bool PA14Lowerer::HasDefaultInitializationEffects(const TypePtr& raw_type) const
{
    TypePtr type = type_value(raw_type);
    if(!type) return false;
    if(type->kind == TYPE_ARRAY)
      return type->bound != 0 && HasDefaultInitializationEffects(type->child);
    if(type->kind != TYPE_CLASS) return true;
    if(type->polymorphic) return true;
    if(HasVirtualBases(type)) return true;
    const vector<Binding*> constructors =
      MemberBindings(type, LastComponent(type->name));

    for(size_t i = 0; i < constructors.size(); ++i) {
      FunctionRecord* record = RecordForBinding(constructors[i]);
      if(record && record->constructor && !record->implicit_constructor &&
         !record->defaulted) {
        CPPGMAstNodePtr body = record->node ?
          ChildOfKind(record->node, "compound-statement") : CPPGMAstNodePtr();
        return true;
      }
    }
    const vector<TypePtr> direct_bases = DirectBaseTypes(type);
    for(size_t base = 0; base < direct_bases.size(); ++base)
      if(HasDefaultInitializationEffects(direct_bases[base])) return true;
    for(size_t i = 0; i < type->class_members.size(); ++i) {
      const ClassMemberInfo& member = type->class_members[i];
      if(member.is_static || !member.type) continue;
      if(HasDefaultInitializationEffects(member.type)) return true;
    }
    return false;
  }

bool PA14Lowerer::HasElidedTemplateInitialization(const TypePtr& raw_type) const
{
    TypePtr type = type_value(raw_type);
    if(!type || type->kind != TYPE_CLASS || !type->template_specialization ||
       !type->owned_scope ||
       !IsEmptyBaseStorage(type) || !IsTrivialValueStorage(type)) return false;
    const vector<Binding*> constructors =
      MemberBindings(type, LastComponent(type->name));
    bool forwarding = false;
    for(size_t i = 0; i < constructors.size(); ++i) {
      FunctionRecord* record = RecordForBinding(constructors[i]);
      if(!record || !record->constructor || record->implicit_constructor ||
         record->defaulted) continue;
      if(!record->source_type || record->source_type->parameters.empty()) return false;
      for(size_t parameter = 0; parameter < record->source_type->parameters.size(); ++parameter) {
        const TypePtr parameter_type = record->source_type->parameters[parameter];
        const TypePtr parameter_value = type_value(parameter_type);
        if(type_is_reference(parameter_type) || !parameter_value ||
           parameter_value->kind != TYPE_CLASS) return false;
      }
      CPPGMAstNodePtr body = record->node ?
        ChildOfKind(record->node, "compound-statement") : CPPGMAstNodePtr();
      if(body && !body->children.empty()) return false;
      if(record->special_initializer &&
         DescendantOfKind(record->special_initializer, "call-expression")) return false;
      forwarding = true;
    }
    return forwarding;
  }

bool PA14Lowerer::TryElideEmptyClassConversion(
  const TypePtr& target, const TypePtr& source,
  const CPPGMAstNodePtr& expression, Scope* scope)
{
  if(!expression || expression->kind != "id-expression" ||
     !target || !source || !IsEmptyBaseStorage(target) ||
     !IsTrivialValueStorage(target) || !IsEmptyBaseStorage(source) ||
     !IsTrivialValueStorage(source) || type_value(source)->is_volatile) return false;
  Binding* conversion = FindConversionOperator(source, target, true);
  if(!conversion) return false;
  FunctionRecord* record = RecordForBinding(conversion);
  if(!record || record->deleted || record->indirect_result || !record->node)
    return false;
  TypePtr conversion_type = function_target_type(record->type);
  if(!conversion_type || !conversion_type->child ||
     !PA12SameType(type_value(conversion_type->child), type_value(target), true))
    return false;
  CPPGMAstNodePtr body = ChildOfKind(record->node, "compound-statement");
  if(!body || body->children.size() != 1 || !body->children[0] ||
     body->children[0]->kind != "return-statement" ||
     body->children[0]->children.size() != 1) return false;
  CPPGMAstNodePtr returned = body->children[0]->children[0];
  if(!returned || returned->kind != "call-expression" ||
     returned->children.empty()) return false;
  CPPGMAstNodePtr arguments = returned->children.size() > 1 ?
    returned->children[1] : CPPGMAstNodePtr();
  if(arguments && !arguments->children.empty()) return false;
  TypePtr constructed = ConstructorObjectType(returned->children[0], scope);
  if(!constructed || !PA12SameType(type_value(constructed), type_value(target), true) ||
     HasDefaultInitializationEffects(target) || HasDestructor(target)) return false;
  MarkFunctionNeeded(record);
  FunctionRecord* base_entry = BaseEntryFor(record);
  if(base_entry) MarkFunctionNeeded(base_entry);
  return true;
}

static string CompactConstructorTypeSpelling(const string& raw)
{
    string result;
    for(size_t i = 0; i < raw.size(); ++i)
      if(!isspace(static_cast<unsigned char>(raw[i]))) result += raw[i];
    return result;
}

static string TypedConstructorTypeSpelling(const TypePtr& raw_type)
{
    TypePtr type = type_value(raw_type);
    if(!type) return string();
    if(type->template_specialization && !type->template_primary.empty()) {
      string result = type->template_primary + "<";
      for(size_t argument = 0; argument < type->template_arguments.size(); ++argument) {
        if(argument != 0) result += ",";
        result += type->template_arguments[argument];
      }
      return result + ">";
    }
    return type->name;
}

static bool ConstructorInitializerSpellingMatchesType(
    const string& raw_name, const TypePtr& target_type)
{
    if(!target_type) return false;
    const string candidate = CompactConstructorTypeSpelling(raw_name);
    if(candidate.empty()) return false;
    const string typed = CompactConstructorTypeSpelling(
      TypedConstructorTypeSpelling(target_type));
    if(candidate == typed || candidate == CompactConstructorTypeSpelling(target_type->name))
      return true;
    return false;
}

bool PA14Lowerer::ConstructorInitializerNamesType(const string& raw_name,
                                                  const TypePtr& target_type,
                                                  Scope* scope) const
{
    if(!target_type) return false;
    // Resolve the initializer through the semantic type table first.  The
    // spelling fallback is only needed for generated class identities whose
    // lowered name intentionally differs from the source template-id.
    try {
      TypePtr resolved = type_value(analyzer_.ResolveType(scope, raw_name));
      if(PA12SameType(resolved, type_value(target_type), true)) return true;
    } catch(const logic_error&) {
      // An unresolved dependent spelling is not a hard constructor error;
      // the generated-name comparison below handles materialized bases.
    }
    return ConstructorInitializerSpellingMatchesType(raw_name, target_type);
}

string PA14Lowerer::EmitTemporaryObjectAddress(const CPPGMAstNodePtr& node,
                                               Scope* scope,
                                               const string& prefix,
                                               bool force_empty)
{
    if(!node || node->kind != "call-expression" || node->children.empty())
      throw logic_error("invalid temporary object expression");
    TypePtr object_type = ConstructorObjectType(node->children[0], scope);
    if(!object_type) throw logic_error("temporary expression is not a class construction");
    CollectImplicitConstructor(object_type, object_type->owned_scope, true);
    const CPPGMAstNodePtr argument_list = node->children.size() > 1 ?
      node->children[1] : CPPGMAstNodePtr();
    vector<CPPGMAstNodePtr> arguments = argument_list ?
      argument_list->children : vector<CPPGMAstNodePtr>();
    if(node->value == "braced-construction" && arguments.size() == 1 &&
       arguments[0] && arguments[0]->kind == "braced-init-list")
      arguments = arguments[0]->children;
    const bool pending_constructor_context = state_ &&
      state_->pending_constructor_unwind_start;
    const bool pending_argument_unwind = pending_constructor_context &&
      !state_->pending_constructor_unwind_suppress_temporary;
    const string pending_dispatch = pending_constructor_context ?
      state_->pending_constructor_unwind_dispatch : string();
    const string pending_end = pending_constructor_context ?
      state_->pending_constructor_unwind_end : string();
    if(pending_argument_unwind) {
      BeginConstructorUnwind(CaptureLiveCleanupObjects(), false,
        pending_dispatch, pending_end);
      state_->pending_constructor_unwind_start = false;
      state_->pending_constructor_unwind_suppress_temporary = false;
      state_->pending_constructor_unwind_dispatch.clear();
      state_->pending_constructor_unwind_end.clear();
    }
    const string slot = new_special_slot(prefix, low_type(object_type));
    const string address = new_temp();
    const bool previous_temporary_construction = state_ && state_->temporary_construction;
    const bool previous_suppress_constructor_unwind = state_ &&
      state_->suppress_constructor_unwind;
    const bool previous_defer_temporary_cleanup = state_ &&
      state_->defer_temporary_cleanup;
    bool construction_no_throw = false;
    bool found_constructor = false;
    const vector<Binding*> constructors = MemberBindings(object_type,
      LastComponent(object_type->name));
    for(size_t i = 0; i < constructors.size(); ++i) {
      FunctionRecord* candidate = RecordForBinding(constructors[i]);
      if(!candidate || !candidate->constructor || candidate->deleted) continue;
      found_constructor = true;
      if(!candidate->unwind_no) {
        construction_no_throw = false;
        found_constructor = false;
        break;
      }
      construction_no_throw = true;
    }
    // A directly bound temporary is protected from the beginning of its
    // construction expression, including formation of its storage address.
    // Nested default-argument temporaries are different: before the nested
    // object has completed there is no nested destructor to run, and the
    // enclosing constructor call will open the region once this temporary is
    // live.  Keep that distinction in the typed lowering state.
    bool preopened_constructor_unwind = false;
    if(state_ && !previous_temporary_construction &&
       !pending_constructor_context &&
       !state_->constructor_unwind_active &&
       !state_->suppress_constructor_unwind && HasDestructor(object_type) &&
       (!construction_no_throw || !DirectBaseTypes(object_type).empty())) {
      BeginConstructorUnwind(CaptureLiveCleanupObjects(), false);
      preopened_constructor_unwind = true;
    }
    const bool suppress_pending_default = state_ &&
      state_->pending_constructor_unwind_start &&
      state_->pending_constructor_unwind_suppress_temporary;
    const vector<FunctionState::TemporaryObject> pending_default_cleanup =
      suppress_pending_default ? CaptureLiveCleanupObjects() :
      vector<FunctionState::TemporaryObject>();
    bool pending_default_context_started = false;
    if(suppress_pending_default && state_ &&
       !state_->constructor_unwind_active &&
       !state_->suppress_constructor_unwind &&
       !pending_default_cleanup.empty()) {
      BeginConstructorUnwind(pending_default_cleanup, true);
      pending_default_context_started = true;
    }
    const bool suppress_nested_empty_constructor = state_ &&
      previous_temporary_construction &&
      CaptureLiveCleanupObjects().empty();
    if(state_) state_->temporary_construction = true;
    if(suppress_pending_default || suppress_nested_empty_constructor)
      state_->suppress_constructor_unwind = true;
    if(state_) state_->defer_temporary_cleanup = true;
    AddInstruction(address + " = addr $" + slot);
    const bool constructed = EmitConstructorAt(object_type, address, arguments, scope,
                          true, false, false, false, arguments.empty());
    if(state_) state_->temporary_construction = previous_temporary_construction;
    if(state_) state_->suppress_constructor_unwind = previous_suppress_constructor_unwind;
    if(state_) state_->defer_temporary_cleanup = previous_defer_temporary_cleanup;
    if(preopened_constructor_unwind && state_ &&
       state_->constructor_unwind_active)
      FinishConstructorUnwind(scope);
    if(!constructed)
      throw logic_error("no viable temporary object construction");
    // A user-declared destructor is still a lifetime boundary when its body is
    // empty.  Keep that typed fact on the temporary so PA25 can emit the same
    // normal and exceptional cleanup action for reference-bound and value
    // temporaries alike.
    RegisterTemporaryObject(object_type, address, force_empty,
      found_constructor && construction_no_throw);
    if(pending_default_context_started && state_ &&
       state_->constructor_unwind_active) {
      state_->constructor_unwind_cleanup = pending_default_cleanup;
      FinishConstructorUnwind(scope);
    }
    if(state_ && state_->pending_constructor_unwind_start) {
      const bool preserve_pending_default_context = suppress_pending_default;
      const vector<FunctionState::TemporaryObject> cleanup =
        CaptureLiveCleanupObjects();
      const bool close_empty_default_context = suppress_pending_default &&
        state_->constructor_unwind_active &&
        state_->constructor_unwind_cleanup.empty();
      if(close_empty_default_context) {
        FinishConstructorUnwind(scope);
      } else if(!cleanup.empty()) {
        if(state_->pending_constructor_unwind_dispatch.empty() &&
           !suppress_pending_default) {
          state_->pending_constructor_unwind_dispatch =
            new_label("call_unwind_dispatch");
          state_->pending_constructor_unwind_end =
            new_label("call_unwind_end");
        }
        if(state_->constructor_unwind_active) {
          if(!suppress_pending_default) {
            state_->constructor_unwind_cleanup = cleanup;
            state_->constructor_unwind_call = true;
          }
        } else if(!suppress_pending_default) BeginConstructorUnwind(cleanup, true,
          state_->pending_constructor_unwind_dispatch,
          state_->pending_constructor_unwind_end);
      }
      if(!preserve_pending_default_context) {
        state_->pending_constructor_unwind_start = false;
        state_->pending_constructor_unwind_suppress_temporary = false;
        state_->pending_constructor_unwind_dispatch.clear();
        state_->pending_constructor_unwind_end.clear();
      }
    }
    return address;
}

PA14Lowerer::Value PA14Lowerer::EmitObjectValueArgument(
    const CPPGMAstNodePtr& node, Scope* scope, const TypePtr& target)
{
    TypePtr object_type = type_value(target);
    if(!object_type || object_type->kind != TYPE_CLASS)
      return EmitValue(node, scope, target);
    // Parentheses do not introduce another object boundary.  In particular,
    // the direct-initialization spelling `X x((Y()))` still constructs the
    // value argument in its final ABI object.  Looking only at the wrapper
    // makes the ordinary address path materialize a second temporary and then
    // passes the uninitialized argument slot to the constructor.
    CPPGMAstNodePtr source_node = node;
    while(source_node && source_node->kind == "parenthesized-expression" &&
          source_node->children.size() == 1 && source_node->children[0])
      source_node = source_node->children[0];
    if(source_node && source_node->kind == "lambda-expression") {
      const TypePtr closure = LambdaClosureType(source_node);
      if(closure && PA12SameType(closure, object_type, true)) {
        const string slot = new_special_slot("argobj", low_type(object_type));
        const string address = new_temp();
        AddInstruction(address + " = addr $" + slot);
        InitializeLambdaClosureAt(closure, address, source_node, scope);
        Value result;
        result.type = object_type;
        result.operand = "$" + slot;
        return result;
      }
    }
    const string slot = new_special_slot("argobj", low_type(object_type));
    const string address = new_temp();
    AddInstruction(address + " = addr $" + slot);
    const ExprInfo source_info = Infer(node, scope);
    const TypePtr source_value = expression_value_type(source_info);
    if(source_value && source_value->kind == TYPE_CLASS &&
       !PA12SameType(source_value, object_type, true)) {
      Binding* conversion = FindConversionOperator(source_value, object_type, true);
      if(conversion) {
        // A class-by-value parameter owns a separate object.  Materialize a
        // user-defined conversion result first, then copy it into the ABI
        // argument object; passing the destination as an indirect conversion
        // result would elide this C++11 object boundary.
        Value converted = EmitConversionOperator(node, scope, object_type, true);
        if(converted.operand.empty()) return converted;
        AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(object_type))) +
          "x" + integer_text(static_cast<long long>(type_alignment(object_type))) +
          " " + converted.operand + ", " + address);
        Value result;
        result.type = object_type;
        result.operand = "$" + slot;
        return result;
      }
    }
    const bool empty_storage = IsEmptyBaseStorage(object_type);
    TypePtr source_type;
    if(empty_storage) {
      const ExprInfo source_info = Infer(node, scope);
      source_type = expression_value_type(source_info);
    }
    // Empty classes still run converting constructors.  The ordinary empty
    // storage shortcut below only needs an address for an exact or derived
    // object; a cross-specialization class value must first initialize the
    // target object so its constructor side effects and typed ABI entry are
    // preserved.
    if(empty_storage && source_type && source_type->kind == TYPE_CLASS &&
       !PA12SameType(source_type, object_type, true) &&
       !IsDerivedFrom(source_type, object_type)) {
      if(EmitObjectTransferAt(object_type, address, node, scope, true)) {
        Value result;
        result.type = object_type;
        result.operand = "$" + slot;
        return result;
      }
    }
    if(empty_storage && IsTrivialValueStorage(object_type)) {
      const TypePtr constructed = source_node && source_node->kind == "call-expression" &&
        !source_node->children.empty() ? ConstructorObjectType(source_node->children[0], scope) : TypePtr();
      if((constructed && PA12SameType(constructed, object_type, true)) ||
         (source_node && source_node->kind == "call-expression" && source_type &&
          source_type->kind == TYPE_CLASS &&
          PA12SameType(source_type, object_type, true))) {
        if(!EmitObjectTransferAt(object_type, address, source_node, scope, true))
          return EmitValue(node, scope, target);
      } else {
        const string source_address = EmitAddress(node, scope);
        if(source_type && source_type->kind == TYPE_CLASS &&
           IsDerivedFrom(source_type, object_type))
          (void)AdjustBaseAddress(source_address, source_type, object_type);
      }
    }
    else if(!EmitObjectTransferAt(object_type, address, node, scope, true))
      return EmitValue(node, scope, target);
    Value result;
    result.type = object_type;
    result.operand = "$" + slot;
    return result;
  }


bool PA14Lowerer::EmitValueSpecialMemberBody(FunctionRecord& function, Scope* scope)
{
    if(!function.value_special_member || (!function.defaulted &&
       !function.implicit_constructor)) return false;
    TypePtr owner = type_value(function.member_owner);
    if(!owner) return false;
    const vector<string> names = ParameterNames(function);
    const size_t source_index = function.member && !function.static_member ? 1 : 0;
    if(source_index >= names.size()) throw logic_error("value member has no source parameter");
    CPPGMAstNodePtr this_node(new CPPGMAstNode("keyword-literal", "KW_THIS:this"));
    const string source_storage = "$" + names[source_index];
    const bool assignment = function.copy_assignment || function.move_assignment;
    const bool move = function.move_constructor || function.move_assignment;
    const vector<TypePtr> direct_bases = DirectBaseTypes(owner);
    bool has_empty_direct_base = false;
    for(size_t base = 0; base < direct_bases.size(); ++base)
      if(IsEmptyBaseStorage(direct_bases[base])) {
        has_empty_direct_base = true;
        break;
      }
    bool has_bit_field = false;
    bool has_reference_member = false;
    for(size_t i = 0; i < owner->class_members.size(); ++i)
      if(owner->class_members[i].bit_field) has_bit_field = true;
      else if(owner->class_members[i].type &&
              type_is_reference(owner->class_members[i].type))
        has_reference_member = true;
    const bool defer_destination = assignment && has_empty_direct_base;
    const bool defer_bit_field_destination = assignment && has_bit_field;
    string destination;
    string source;
    if(IsEmptyBaseStorage(owner)) {
      // Empty class/base subobjects have no payload.  Their special members
      // still return the ABI this pointer for assignment, but must not copy
      // the complete-object size byte used by standalone empty objects.
      if(assignment) {
        const string result = EmitValue(this_node, scope).operand;
        Terminate("return ptr " + result);
      }
      return true;
    }
    if(!defer_destination && !defer_bit_field_destination)
      destination = EmitValue(this_node, scope).operand;
    bool defaulted_copy_storage = function.copy_assignment && function.defaulted &&
      direct_bases.empty();
    if(defaulted_copy_storage) {
      for(size_t i = 0; i < owner->class_members.size(); ++i) {
        const ClassMemberInfo& member = owner->class_members[i];
        if(member.is_static || !member.type) continue;
        if(member.bit_field || !IsTrivialValueStorage(member.type)) {
          defaulted_copy_storage = false;
          break;
        }
      }
    }
    // A class with only scalar payload still has a non-trivial lifetime when
    // its destructor is user-declared.  Its implicit copy constructor may
    // copy that payload as one object, while nested class members must retain
    // their own typed copy-constructor boundaries.
    bool byte_copyable_owner = direct_bases.empty() && !owner->polymorphic &&
      !has_bit_field && !has_reference_member;
    for(size_t i = 0; byte_copyable_owner && i < owner->class_members.size(); ++i) {
      const ClassMemberInfo& member = owner->class_members[i];
      if(member.is_static || !member.type) continue;
      if(!IsTrivialValueStorage(member.type)) byte_copyable_owner = false;
    }
    set<long long> copied_bit_offsets;
    if(((IsTrivialValueStorage(owner) || byte_copyable_owner) && !has_reference_member &&
        !(assignment && has_bit_field)) ||
       defaulted_copy_storage) {
      source = emit_load(source_storage, PointerTo(Fundamental("char")));
      AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(owner))) +
        "x" + integer_text(static_cast<long long>(type_alignment(owner))) + " " +
        source + ", " + destination);
    } else {
      if(!function.special_initializer || !HasVirtualBases(owner))
      for(size_t base_index = 0; base_index < direct_bases.size(); ++base_index) {
        TypePtr base = type_value(direct_bases[base_index]);
        if(!base || IsEmptyBaseStorage(base)) continue;
        if(destination.empty()) destination = EmitValue(this_node, scope).operand;
        const string destination_base = AdjustBaseAddress(destination, owner, base);
        source = emit_load(source_storage, PointerTo(Fundamental("char")));
        const string source_base = AdjustBaseAddress(source, owner, base);
        if(IsTrivialValueStorage(base)) {
          AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(base))) +
            "x" + integer_text(static_cast<long long>(type_alignment(base))) + " " +
            source_base + ", " + destination_base);
        } else {
          FunctionRecord* base_record = assignment ?
            EnsureImplicitAssignment(base, move) : EnsureImplicitCopyConstructor(base, move);
          if(!base_record || base_record->deleted)
            throw logic_error("value member has deleted base operation");
          // A defaulted special member invokes the base-subobject
          // constructor entry.  User-written constructors use the normal
          // complete-object entry, even when they happen to initialize a
          // base by delegation.
          if(!assignment && function.defaulted &&
             (function.copy_constructor || function.move_constructor) &&
             !BaseEntryFor(base_record))
            EnsureConstructorBaseEntry(base_record);
          MarkFunctionNeeded(base_record);
          FunctionRecord* base_call = BaseEntryFor(base_record);
          if(base_call) MarkFunctionNeeded(base_call);
          if(!base_call) base_call = base_record;
          const string base_arguments = destination_base + ", " + source_base;
          if(assignment) {
            const string result = new_temp();
            AddInstruction(result + " = call ptr @" + base_call->symbol + "(" +
              base_arguments + ")");
          } else AddInstruction("call void @" + base_call->symbol + "(" +
            base_arguments + ")");
        }
      }
      long long trivial_prefix_size = 0;
      for(size_t i = 0; i < owner->class_members.size(); ++i) {
        const ClassMemberInfo& member = owner->class_members[i];
        if(member.is_static || !member.type || member.name.empty()) continue;
        TypePtr member_type = type_value(member.type);
        if(member.bit_field || type_is_reference(member.type) ||
           !IsTrivialValueStorage(member_type)) {
          trivial_prefix_size = member.offset;
          break;
        }
      }
      if(trivial_prefix_size > 0) {
        if(destination.empty()) destination = EmitValue(this_node, scope).operand;
        if(source.empty()) source = emit_load(source_storage, PointerTo(Fundamental("char")));
        AddInstruction("copyobj " + integer_text(trivial_prefix_size) + "x" +
          integer_text(static_cast<long long>(type_alignment(owner))) + " " +
          source + ", " + destination);
        // The non-trivial suffix gets the normal typed member-operation
        // sequence, whose fresh loads keep the source/destination lifetime
        // visible in LowIR.
        destination.clear();
        source.clear();
      }
      for(size_t i = 0; i < owner->class_members.size(); ++i) {
        const ClassMemberInfo& member = owner->class_members[i];
        if(member.is_static || !member.type || member.name.empty()) continue;
        if(trivial_prefix_size > 0 && member.offset < trivial_prefix_size) continue;
        TypePtr member_type = type_value(member.type);
        if(member.bit_field) {
          if(copied_bit_offsets.find(member.offset) != copied_bit_offsets.end()) continue;
          copied_bit_offsets.insert(member.offset);
          if(source.empty()) source = emit_load(source_storage, PointerTo(Fundamental("char")));
          const string source_member = new_temp();
          AddInstruction(source_member + " = index i8 " + source + ", " +
            integer_text(member.offset));
          const string loaded = emit_load(source_member, member.type);
          const string destination_object = emit_load(
            "$" + names[0], PointerTo(Fundamental("char")));
          const string destination_member = new_temp();
          AddInstruction(destination_member + " = index i8 " + destination_object + ", " +
            integer_text(member.offset));
          emit_store(member.type, loaded, destination_member);
          continue;
        }
        if(assignment && has_empty_direct_base && member_type &&
           member_type->kind != TYPE_CLASS && member_type->kind != TYPE_ARRAY) {
          if(source.empty())
            source = emit_load(source_storage, PointerTo(Fundamental("char")));
          const string source_member = new_temp();
          AddInstruction(source_member + " = index i8 [projection=field] " + source +
            ", " + integer_text(member.offset));
          const string loaded = emit_load(source_member, member.type);
          const string destination_object = emit_load(
            "$" + names[0], PointerTo(Fundamental("char")));
          const string destination_member = new_temp();
          AddInstruction(destination_member +
            " = index i8 [projection=field] " + destination_object + ", " +
            integer_text(member.offset));
          emit_store(member.type, loaded, destination_member);
          continue;
        }
        if(type_is_reference(member.type)) {
          if(source.empty())
            source = emit_load(source_storage, PointerTo(Fundamental("char")));
          const string source_member = new_temp();
          AddInstruction(source_member +
            " = index i8 [projection=reference_field] " + source + ", " +
            integer_text(member.offset));
          const string referred = emit_load(source_member,
            PointerTo(Fundamental("char")));
          if(destination.empty()) destination = EmitValue(this_node, scope).operand;
          const string destination_member = new_temp();
          AddInstruction(destination_member +
            " = index i8 [projection=field] " + destination + ", " +
            integer_text(member.offset));
          emit_store(PointerTo(Fundamental("char")), referred, destination_member);
          continue;
        }
        if(destination.empty()) destination = EmitValue(this_node, scope).operand;
        const string destination_member = new_temp();
        AddInstruction(destination_member + " = index i8 [projection=field] " +
          destination + ", " + integer_text(member.offset));
        if(source.empty()) source = emit_load(source_storage, PointerTo(Fundamental("char")));
        const string source_member = new_temp();
        AddInstruction(source_member + " = index i8 [projection=field] " + source +
          ", " + integer_text(member.offset));
        if(member_type && member_type->kind == TYPE_CLASS &&
           !IsTrivialValueStorage(member_type)) {
          if(assignment && move) {
            FunctionRecord* member_move_constructor =
              FindValueMember(member_type, true, false);
            if(member_move_constructor && !member_move_constructor->deleted &&
               !member_move_constructor->defaulted)
              MarkFunctionNeeded(member_move_constructor);
          }
          FunctionRecord* member_record = assignment ?
            EnsureImplicitAssignment(member_type, move) :
            EnsureImplicitCopyConstructor(member_type, move);
          if(!member_record || member_record->deleted)
            throw logic_error("value member has deleted class operation");
          MarkFunctionNeeded(member_record);
          const string member_arguments = destination_member + ", " + source_member;
          if(assignment) {
            const string result = new_temp();
            AddInstruction(result + " = call ptr @" + member_record->symbol + "(" +
              member_arguments + ")");
          } else AddInstruction("call void @" + member_record->symbol + "(" +
            member_arguments + ")");
        } else if(member_type && member_type->kind == TYPE_ARRAY &&
                  member_type->child && !IsTrivialValueStorage(member_type->child)) {
          throw logic_error("nontrivial class array value member is not lowered yet");
        } else {
          AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(member.type))) +
            "x" + integer_text(static_cast<long long>(type_alignment(member.type))) + " " +
            source_member + ", " + destination_member);
        }
      }
    }
    if(owner->polymorphic) {
      const string vptr_destination = destination.empty() || !direct_bases.empty() ?
        EmitValue(this_node, scope).operand : destination;
      EmitVPointerStore(owner, vptr_destination);
    }
    if(assignment) {
      // Keep the ABI result tied to the stored this pointer.  Besides
      // matching the reference return convention, this avoids treating the
      // destination address temporary as an independently returned value.
      const string result = EmitValue(this_node, scope).operand;
      Terminate("return ptr " + result);
    }
    return true;
  }

void PA14Lowerer::EmitConstructorInitializers(FunctionRecord& function, Scope* scope)
{
    TypePtr owner = type_value(function.member_owner);
    if(!owner) return;
    CPPGMAstNodePtr this_node(new CPPGMAstNode("keyword-literal", "KW_THIS:this"));
    set<string> initialized_members;
    const vector<TypePtr> direct_bases = DirectBaseTypes(owner);
    TypePtr base = direct_bases.empty() ? TypePtr() : type_value(direct_bases[0]);
    const auto matching_direct_base = [&](const string& spelling) {
      for(size_t candidate = 0; candidate < direct_bases.size(); ++candidate) {
        TypePtr current = type_value(direct_bases[candidate]);
        if(!current) continue;
        if(ConstructorInitializerNamesType(spelling, current, scope) ||
           LastComponent(spelling) == LastComponent(current->name) ||
           spelling == current->name)
          return current;
        Analyzer::PathTarget alias = analyzer_.ResolvePath(scope,
          LastComponent(spelling));
        if(!alias.binding || (alias.binding->kind != BIND_TYPE &&
                              alias.binding->kind != BIND_TYPE_ALIAS))
          continue;
        TypePtr alias_type = alias.binding ? type_value(alias.binding->type) : TypePtr();
        if(alias_type && alias_type == current) return current;
      }
      return TypePtr();
    };
    const auto has_explicit_direct_base = [&](const TypePtr& target) {
      if(!target || !function.special_initializer) return false;
      for(size_t initializer = 0;
          initializer < function.special_initializer->children.size(); ++initializer) {
        CPPGMAstNodePtr item = function.special_initializer->children[initializer];
        if(!item || item->kind != "mem-initializer") continue;
        CPPGMAstNodePtr id = ChildOfKind(item, "mem-initializer-id");
        if(id && matching_direct_base(id->value) == target) return true;
      }
      return false;
    };
    const auto demand_complete_default_constructor = [&](const TypePtr& target) {
      if(!target || !HasUserProvidedConstructor(target)) return;
      const vector<Binding*> candidates = MemberBindings(target,
        LastComponent(target->name));
      for(size_t candidate = 0; candidate < candidates.size(); ++candidate) {
        Binding* binding = candidates[candidate];
        FunctionRecord* record = RecordForBinding(binding);
        TypePtr signature = binding ? function_target_type(binding->type) : TypePtr();
        if(!record || !record->constructor || record->deleted || !signature) continue;
        bool defaultable = true;
        for(size_t parameter = 0; parameter < signature->parameters.size(); ++parameter)
          if(!HasDefaultArgument(binding, parameter)) { defaultable = false; break; }
        if(defaultable) {
          MarkFunctionNeeded(record);
          return;
        }
      }
    };
    bool delegating = false;
    if(function.special_initializer) {
      for(size_t i = 0; i < function.special_initializer->children.size(); ++i) {
        CPPGMAstNodePtr initializer = function.special_initializer->children[i];
        CPPGMAstNodePtr name_node = ChildOfKind(initializer, "mem-initializer-id");
        if(name_node && LastComponent(name_node->value) == LastComponent(owner->name)) {
          delegating = true;
          break;
        }
      }
    }
    // A generated specialization can retain a dependent-base marker even
    // after its concrete class body has been replayed.  In that case the
    // base lookup was intentionally deferred; emitting an implicit base
    // constructor call here would manufacture a synthetic base-entry
    // function from an unresolved template path.  Explicit base
    // mem-initializers are still handled below once their target is known.
	const bool defer_dependent_base = base && owner->dependent_base_lookup &&
		!HasUserProvidedConstructor(base);
	if(defer_dependent_base) {
      // Keep definitions for concrete class subobjects in a deferred base's
      // replayed body.  The base call itself is intentionally postponed, but
      // an explicitly defined member constructor is still part of the
      // materialized template closure (and is observable in the object
      // surface even when no call can be formed yet).
      for(size_t member = 0; member < base->class_members.size(); ++member) {
        const ClassMemberInfo& member_fact = base->class_members[member];
        if(member_fact.is_static || !member_fact.type) continue;
        TypePtr member_type = type_value(member_fact.type);
        while(member_type && member_type->kind == TYPE_ARRAY)
          member_type = type_value(member_type->child);
        if(!member_type || member_type->kind != TYPE_CLASS) continue;
        const vector<Binding*> member_constructors = MemberBindings(
          member_type, LastComponent(member_type->name));
        for(size_t candidate = 0; candidate < member_constructors.size(); ++candidate) {
          FunctionRecord* record = RecordForBinding(member_constructors[candidate]);
          if(record && record->constructor && record->definition &&
             !record->implicit_constructor)
            MarkFunctionNeeded(record);
        }
      }
    }
	if(!delegating) {
      // The complete constructor owns each virtual base.  Base-entry
      // constructors deliberately skip this phase: their hidden arguments
      // name the already-created virtual subobjects in the most-derived
      // object and are forwarded to their non-virtual base entries below.
      if(!function.base_entry) {
        set<const Type*> constructed_virtual_bases;
        for(size_t virtual_index = 0;
            virtual_index < owner->virtual_base_types.size(); ++virtual_index) {
          TypePtr virtual_base = type_value(owner->virtual_base_types[virtual_index]);
          if(!virtual_base || !constructed_virtual_bases.insert(virtual_base.get()).second)
            continue;
          if(!HasConstructor(virtual_base) && HasDefaultConstructionEffects(virtual_base))
            CollectImplicitConstructor(virtual_base, virtual_base->owned_scope, true);
          if(!HasConstructor(virtual_base) ||
             (IsEmptyBaseStorage(virtual_base) &&
              !HasDefaultConstructionEffects(virtual_base) &&
              !HasUserProvidedConstructor(virtual_base))) continue;
          demand_complete_default_constructor(virtual_base);
          const string this_address = EmitValue(this_node, scope).operand;
          const string base_address = AdjustBaseAddress(this_address, owner, virtual_base);
          const map<const Type*, string> saved_pending = state_ ?
            state_->pending_constructor_virtual_base_arguments :
            map<const Type*, string>();
          if(state_) {
            state_->pending_constructor_virtual_base_arguments.clear();
            // A virtual root constructor can itself have virtual bases.  Its
            // complete-constructor call is entered on the root view, while
            // the hidden arguments must still name the corresponding views
            // in the most-derived object.
            const vector<TypePtr> nested_bases = VirtualBaseTypes(virtual_base);
            for(size_t nested = 0; nested < nested_bases.size(); ++nested) {
              TypePtr nested_base = nested_bases[nested];
              if(!nested_base || PA12SameType(nested_base, virtual_base, true))
                continue;
              const string nested_address = AdjustBaseAddress(
                EmitValue(this_node, scope).operand, owner, nested_base);
              state_->pending_constructor_virtual_base_arguments[nested_base.get()] =
                nested_address;
            }
          }
          (void)EmitConstructorAt(virtual_base, base_address,
            vector<CPPGMAstNodePtr>(), scope, true, true);
          if(state_) state_->pending_constructor_virtual_base_arguments = saved_pending;
        }
      }
      // When a source constructor has a mem-initializer list, the ordered
      // initializer sequence below owns all direct-base calls.  Running the
      // implicit-base pass as well would initialize an omitted base once
      // before the explicit list and once again at its declaration-order
      // position.
      if(!function.special_initializer)
      for(size_t base_index = 0; base_index < direct_bases.size(); ++base_index) {
        TypePtr current_base = type_value(direct_bases[base_index]);
        if(!current_base) continue;
        // Complete constructors initialize virtual direct bases in the
        // ownership pass above.  Base entries likewise receive their
        // already-created virtual views from hidden arguments.
        if(IsVirtualDirectBase(owner, base_index)) continue;
        if(!HasConstructor(current_base) && HasDefaultConstructionEffects(current_base))
          CollectImplicitConstructor(current_base, current_base->owned_scope, true);
        const bool defer_current_base = current_base == base && defer_dependent_base;
        const bool explicit_current_base = has_explicit_direct_base(current_base);
        if(defer_current_base || explicit_current_base || !HasConstructor(current_base) ||
           (IsEmptyBaseStorage(current_base) &&
            !HasDefaultConstructionEffects(current_base) &&
            !HasUserProvidedConstructor(current_base))) continue;
        if(HasVirtualBases(owner))
          demand_complete_default_constructor(current_base);
        const string this_address = EmitValue(this_node, scope).operand;
        const string base_address = AdjustBaseAddress(this_address, owner, current_base);
        const map<const Type*, string> saved_pending = state_ ?
          state_->pending_constructor_virtual_base_arguments :
          map<const Type*, string>();
        if(state_) {
          state_->pending_constructor_virtual_base_arguments.clear();
          const vector<TypePtr> hidden_bases = VirtualBaseTypes(current_base);
          for(size_t hidden = 0; hidden < hidden_bases.size(); ++hidden) {
            TypePtr hidden_base = hidden_bases[hidden];
            if(!hidden_base) continue;
            string hidden_address;
            if(function.base_entry) {
              map<string, vector<string> >::const_iterator incoming =
                state_->virtual_base_hidden_by_source.find("this");
              size_t owner_index = 0;
              bool found_owner_index = false;
              for(; owner_index < owner->virtual_base_types.size(); ++owner_index)
                if(owner->virtual_base_types[owner_index] &&
                   SameLayoutType(owner->virtual_base_types[owner_index], hidden_base)) {
                  found_owner_index = true;
                  break;
                }
              if(incoming != state_->virtual_base_hidden_by_source.end() &&
                 found_owner_index && owner_index < incoming->second.size())
                hidden_address = incoming->second[owner_index];
            } else {
              // Defer this projection until EmitConstructorAt has evaluated
              // the ordinary arguments.  The hidden view is rooted in the
              // base-subobject operand, so AppendVirtualBaseCallArguments
              // can form it from that typed source without changing the ABI.
              hidden_address = "__deferred_constructor_virtual_base__";
            }
            if(!hidden_address.empty())
              state_->pending_constructor_virtual_base_arguments[hidden_base.get()] =
                hidden_address;
          }
        }
        (void)EmitConstructorAt(current_base, base_address,
          vector<CPPGMAstNodePtr>(), scope, true, true);
        if(state_) state_->pending_constructor_virtual_base_arguments = saved_pending;
      }
    }
    vector<CPPGMAstNodePtr> ordered_initializers;
    set<const CPPGMAstNode*> used_initializers;
    if(function.special_initializer) {
      // C++ initializes direct bases in declaration order, independently of
      // the order in the mem-initializer list.  Keep explicit and implicit
      // base actions in one typed sequence; otherwise an explicit first base
      // is delayed until after the implicit second base.
      for(size_t base_index = 0; base_index < direct_bases.size(); ++base_index) {
        const TypePtr direct_base = type_value(direct_bases[base_index]);
        if(!direct_base) continue;
        CPPGMAstNodePtr selected;
        for(size_t i = 0; i < function.special_initializer->children.size(); ++i) {
          CPPGMAstNodePtr initializer = function.special_initializer->children[i];
          if(!initializer || initializer->kind != "mem-initializer") continue;
          CPPGMAstNodePtr name_node = ChildOfKind(initializer, "mem-initializer-id");
          if(name_node && matching_direct_base(name_node->value) == direct_base) {
            selected = initializer;
            break;
          }
        }
        if(IsVirtualDirectBase(owner, base_index)) {
          if(selected) used_initializers.insert(selected.get());
          continue;
        }
        if(selected) {
          ordered_initializers.push_back(selected);
          used_initializers.insert(selected.get());
          continue;
        }
        if(!HasDefaultConstructionEffects(direct_base)) continue;
        CPPGMAstNodePtr synthetic(new CPPGMAstNode("mem-initializer"));
        synthetic->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
          "mem-initializer-id", LastComponent(direct_base->name))));
        ordered_initializers.push_back(synthetic);
      }
      for(size_t m = 0; m < owner->class_members.size(); ++m) {
        const ClassMemberInfo& member_fact = owner->class_members[m];
        if(member_fact.is_static || member_fact.name.empty() || !member_fact.type) continue;
        CPPGMAstNodePtr selected;
        for(size_t i = 0; i < function.special_initializer->children.size(); ++i) {
          CPPGMAstNodePtr initializer = function.special_initializer->children[i];
          CPPGMAstNodePtr name_node = ChildOfKind(initializer, "mem-initializer-id");
          if(!initializer || initializer->kind != "mem-initializer" || !name_node) continue;
          if(LastComponent(name_node->value) == member_fact.name) {
            selected = initializer;
            break;
          }
        }
        if(selected) {
          ordered_initializers.push_back(selected);
          used_initializers.insert(selected.get());
          continue;
        }
        if(!member_fact.initializer) continue;
        CPPGMAstNodePtr synthetic(new CPPGMAstNode("mem-initializer"));
        synthetic->children.push_back(CPPGMAstNodePtr(
          new CPPGMAstNode("mem-initializer-id", member_fact.name)));
        CPPGMAstNodePtr expression = InitializerExpression(member_fact.initializer);
        if(expression && expression->kind == "braced-init-list") {
          synthetic->children.push_back(expression);
        } else if(member_fact.initializer->children.size() == 1 &&
                  member_fact.initializer->children[0] &&
                  member_fact.initializer->children[0]->kind == "paren-initializer") {
          CPPGMAstNodePtr arguments(new CPPGMAstNode("paren-argument-list"));
          arguments->children = member_fact.initializer->children[0]->children;
          synthetic->children.push_back(arguments);
        } else if(expression) {
          CPPGMAstNodePtr arguments(new CPPGMAstNode("paren-argument-list"));
          arguments->children.push_back(expression);
          synthetic->children.push_back(arguments);
        }
        ordered_initializers.push_back(synthetic);
      }
      for(size_t i = 0; i < function.special_initializer->children.size(); ++i) {
        CPPGMAstNodePtr initializer = function.special_initializer->children[i];
        if(initializer && initializer->kind == "mem-initializer" &&
           used_initializers.find(initializer.get()) == used_initializers.end())
          ordered_initializers.push_back(initializer);
      }
    }
    bool vptr_stored = false;
    for(size_t i = 0; i < ordered_initializers.size(); ++i) {
      CPPGMAstNodePtr initializer = ordered_initializers[i];
      if(!initializer || initializer->kind != "mem-initializer") continue;
      CPPGMAstNodePtr name_node = ChildOfKind(initializer, "mem-initializer-id");
      if(!name_node) continue;
      const string name = LastComponent(name_node->value);
      CPPGMAstNodePtr argument_node = ChildOfKind(initializer, "paren-argument-list");
      if(!argument_node) argument_node = ChildOfKind(initializer, "braced-init-list");
      vector<CPPGMAstNodePtr> arguments = argument_node ? argument_node->children :
        vector<CPPGMAstNodePtr>();
      TypePtr named_direct_base = matching_direct_base(name_node->value);
      const bool is_base_initializer = named_direct_base != TypePtr();
      if(owner->polymorphic && !vptr_stored && !delegating &&
         !is_base_initializer && name != LastComponent(owner->name)) {
        EmitVPointerStore(owner, EmitValue(this_node, scope).operand);
        vptr_stored = true;
      }
      if(name == LastComponent(owner->name)) {
        const string this_address = EmitValue(this_node, scope).operand;
        if(!EmitConstructorAt(owner, this_address, arguments, scope, true))
          throw logic_error("delegating constructor has no viable target");
        continue;
      }
      TypePtr named_base;
      if(is_base_initializer) named_base = named_direct_base;
      else if(base && (name == LastComponent(base->name) ||
                       name == base->name)) named_base = base;
      if(!named_base) {
        Analyzer::PathTarget alias = analyzer_.ResolvePath(scope, name);
        if(alias.binding && (alias.binding->kind == BIND_TYPE ||
                             alias.binding->kind == BIND_TYPE_ALIAS)) {
          TypePtr alias_type = type_value(alias.binding->type);
          if(alias_type) named_base = matching_direct_base(alias_type->name);
        }
      }
      if(named_base) {
        if(arguments.empty() && IsEmptyBaseStorage(named_base) &&
           !HasDefaultConstructionEffects(named_base) &&
           !HasUserProvidedConstructor(named_base))
          continue;
        const string this_address = EmitValue(this_node, scope).operand;
        const string base_address = AdjustBaseAddress(this_address, owner, named_base);
        if(arguments.size() == 1 && arguments[0] &&
           arguments[0]->kind != "braced-init-list") {
          const TypePtr argument_type = expression_value_type(Infer(arguments[0], scope));
          if(argument_type && argument_type->kind == TYPE_CLASS &&
             ((PA12SameType(argument_type, named_base, true) &&
               (Analyzer::HasNodeValue(function.node, "decl-specifier", "constexpr") ||
                Analyzer::HasNodeValue(function.node, "specifier", "constexpr"))) ||
              IsDerivedFrom(argument_type, named_base)) &&
             (!IsEmptyBaseStorage(named_base) ||
              !HasUserProvidedConstructor(named_base)) &&
             EmitObjectTransferAt(named_base, base_address, arguments[0], scope, true))
            continue;
        }
        const map<const Type*, string> saved_pending = state_ ?
          state_->pending_constructor_virtual_base_arguments :
          map<const Type*, string>();
        if(state_) {
          state_->pending_constructor_virtual_base_arguments.clear();
          const vector<TypePtr> hidden_bases = VirtualBaseTypes(named_base);
          for(size_t hidden = 0; hidden < hidden_bases.size(); ++hidden) {
            TypePtr hidden_base = hidden_bases[hidden];
            if(!hidden_base) continue;
            const string hidden_address = "__deferred_constructor_virtual_base__";
            state_->pending_constructor_virtual_base_arguments[hidden_base.get()] =
              hidden_address;
          }
        }
        const bool constructed = EmitConstructorAt(named_base, base_address,
          arguments, scope, true, true);
        if(state_) state_->pending_constructor_virtual_base_arguments = saved_pending;
        if(!constructed)
          throw logic_error("base mem-initializer has no constructor");
        continue;
      }
      vector<Binding*> fields = DirectBindings(owner->owned_scope, name);
      Binding* field = 0;
      for(size_t j = 0; j < fields.size(); ++j)
        if(fields[j]->kind == BIND_VARIABLE && fields[j]->is_member && !fields[j]->is_static) {
          field = fields[j];
          break;
        }
      if(!field) throw logic_error("unknown mem-initializer");
      initialized_members.insert(name);
      TypePtr field_type = type_value(field->type);
      Value value;
      string reference_source;
      if(!arguments.empty() && type_is_reference(field->type)) {
        reference_source = EmitReferenceArgument(arguments[0], scope, field->type);
		} else if(!(field_type && field_type->kind == TYPE_CLASS && !arguments.empty()) &&
				!(field_type && field_type->kind == TYPE_ARRAY && argument_node &&
					argument_node->kind == "braced-init-list") && !arguments.empty()) {
        if(arguments.size() != 1) throw logic_error("member mem-initializer has too many arguments");
        if(type_is_reference(field->type))
          reference_source = EmitReferenceArgument(arguments[0], scope, field->type);
        else {
          value = EmitValue(arguments[0], scope, field->type);
          if(value.known_constant && is_integral_type(value.type) &&
             is_integral_type(field->type) &&
             type_size(field->type) > type_size(value.type) &&
             !is_unsigned_type(field->type)) {
            value.type = field->type;
            value.operand = integer_text(value.constant);
          } else value = ConvertValue(value, field->type, false, true);
        }
      }
      CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "OP_ARROW:->"));
      member->children.push_back(this_node);
      member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", name)));
      if(IsBitField(field) && !arguments.empty()) {
        bool preserve = false;
        if(field->member_owner && field->member_index != static_cast<size_t>(-1) &&
           field->member_index < field->member_owner->class_members.size()) {
          const long long offset = field->member_owner->class_members[field->member_index].offset;
          for(set<string>::const_iterator it = initialized_members.begin();
              it != initialized_members.end() && !preserve; ++it) {
            if(*it == name) continue;
            vector<Binding*> prior = DirectBindings(owner->owned_scope, *it);
            for(size_t j = 0; j < prior.size(); ++j)
              if(IsBitField(prior[j]) && prior[j]->member_owner &&
                 prior[j]->member_index != static_cast<size_t>(-1) &&
                 prior[j]->member_index < prior[j]->member_owner->class_members.size() &&
                 prior[j]->member_owner->class_members[prior[j]->member_index].offset == offset) {
                preserve = true;
                break;
              }
          }
        }
        string stored;
        if(preserve) {
          const string read_address = EmitMemberAddress(member, scope);
          stored = MergeBitFieldValue(field, read_address, field->type, value.operand, true);
        } else stored = PrepareBitFieldValue(field, field->type, value.operand);
        const string store_address = EmitMemberAddress(member, scope);
        emit_store(field->type, stored, store_address);
        continue;
      }
      if(field_type && field_type->kind == TYPE_CLASS &&
         !type_is_reference(field->type) && arguments.empty()) {
        if(HasDefaultConstructionEffects(field_type))
          CollectImplicitConstructor(field_type, field_type->owned_scope, true);
        const vector<Binding*> constructors =
          MemberBindings(field_type, LastComponent(field_type->name));
        if(field_type->is_union && constructors.empty()) continue;
      }
		const string address = EmitMemberAddress(member, scope);
		if(field_type && field_type->kind == TYPE_ARRAY && argument_node &&
			argument_node->kind == "braced-init-list") {
			EmitAggregateAt(address, field_type, argument_node, scope, member);
			continue;
		}
		if(field_type && field_type->kind == TYPE_ARRAY && arguments.empty() &&
         field_type->bound >= 0 && field_type->child &&
         type_value(field_type->child) && type_value(field_type->child)->kind != TYPE_CLASS) {
        for(size_t element_index = 0;
            element_index < static_cast<size_t>(field_type->bound); ++element_index) {
          const string member_address = element_index == 0 ? address :
            EmitMemberAddress(member, scope);
          const string decay = new_temp();
          AddInstruction(decay + " = unary decay ptr " + member_address);
          const string element = new_temp();
          AddInstruction(element + " = index " + low_type(field_type->child) + " " +
            decay + ", " + integer_text(static_cast<long long>(element_index)));
          emit_store(field_type->child, "0", element);
        }
        continue;
      }
      if(field_type && field_type->kind == TYPE_CLASS &&
         !type_is_reference(field->type) && arguments.empty()) {
        const vector<Binding*> constructors =
          MemberBindings(field_type, LastComponent(field_type->name));
        if(field_type->is_union && constructors.empty()) continue;
        if(!constructors.empty()) {
          // An explicitly empty mem-initializer is value-initialization.  The
          // object ABI models its zero-initialization before invoking the
          // selected default constructor.  Keep this scoped to a real
          // constructor candidate; aggregate/base fallback below has its
          // own typed member initialization rules.
          if(!HasUserProvidedConstructor(field_type))
            emit_store(Fundamental("long int"), "0", address);
          if(EmitConstructorAt(field_type, address, arguments, scope)) continue;
        }
        CPPGMAstNodePtr empty(new CPPGMAstNode("braced-init-list"));
        const vector<TypePtr> nested_bases = DirectBaseTypes(field_type);
        for(size_t base_index = 0; base_index < nested_bases.size(); ++base_index) {
          TypePtr nested_base = type_value(nested_bases[base_index]);
          if(!nested_base) continue;
          const size_t base_offset = base_index < field_type->direct_base_offsets.size() ?
            field_type->direct_base_offsets[base_index] :
            (base_index == 0 ? field_type->direct_base_offset : 0);
          const string base_address = base_offset == 0 ? address :
            AdjustBaseAddress(address, field_type, nested_base);
          const vector<Binding*> base_constructors =
            MemberBindings(nested_base, LastComponent(nested_base->name));
          if(!base_constructors.empty() && EmitConstructorAt(nested_base,
                                                               base_address, arguments, scope,
                                                               true, true)) {
            // The base constructor performed its own initialization.
          } else EmitAggregateAt(base_address, nested_base, empty, scope,
                                 CPPGMAstNodePtr(), -1, true);
        }
        EmitAggregateAt(address, field_type, empty, scope,
                        CPPGMAstNodePtr(), -1, true);
        continue;
      }
      if(field_type && field_type->kind == TYPE_CLASS &&
         !type_is_reference(field->type) && !arguments.empty()) {
        // Copying an empty, trivially stored class has no object bytes to
        // transfer.  Still form the destination member address above, but do
        // not evaluate a reference source merely to feed an elided transfer.
        // This preserves constructor ABI demand while avoiding a spurious
        // load from forwarding parameters in generated empty functors.
        const bool empty_trivial_field = IsEmptyBaseStorage(field_type) &&
          IsTrivialValueStorage(field_type) &&
          !HasUserProvidedConstructor(field_type);
        if((HasElidedTemplateInitialization(field_type) || empty_trivial_field) &&
           arguments.size() == 1 && arguments[0]) {
          TypePtr source_type;
          if(arguments[0]->kind == "id-expression") {
            VariablePlan* source_local = LocalForName(arguments[0]->value);
            if(source_local) source_type = type_value(source_local->type);
            if(!source_type) {
              Analyzer::PathTarget source = analyzer_.ResolvePath(scope, arguments[0]->value);
              if(source.binding) source_type = type_value(source.binding->type);
            }
          }
          if(!source_type) source_type = expression_value_type(Infer(arguments[0], scope));
          if(source_type && source_type->kind == TYPE_CLASS) continue;
        }
        if(function.base_entry && IsEmptyBaseStorage(field_type)) continue;
        if(arguments.size() == 1 && arguments[0] &&
           arguments[0]->kind != "braced-init-list" &&
           EmitObjectTransferAt(field_type, address, arguments[0], scope, true))
          continue;
        if(EmitConstructorAt(field_type, address, arguments, scope)) continue;
        if(argument_node && argument_node->kind == "braced-init-list") {
          CPPGMAstNodePtr aggregate(new CPPGMAstNode("braced-init-list"));
          aggregate->children = arguments;
          EmitAggregateAt(address, field_type, aggregate, scope, member);
          continue;
        }
      }
      if(arguments.empty()) {
        if(!field_type || field_type->kind != TYPE_CLASS)
          emit_store(field->type, "0", address);
        continue;
      }
      if(type_is_reference(field->type)) {
        emit_store(PointerTo(Fundamental("char")), reference_source, address);
      } else if(IsBitField(field)) {
        StoreBitField(field, address, field->type, value.operand, true);
      } else {
        emit_store(field->type, value.operand, address);
      }
    }
	if(owner->is_union) return;
	if(function.construction_entry && !delegating) {
		const vector<string> constructor_names = ParameterNames(function);
		const size_t vtt_index = (function.indirect_result ? 1 : 0) +
			(function.member && !function.static_member ? 1 : 0);
		if(vtt_index >= constructor_names.size())
			throw logic_error("construction entry has no VTT parameter");
		const string this_address = EmitValue(this_node, scope).operand;
		const string construction_vptr = new_temp();
		AddInstruction(construction_vptr + " = load ptr %" +
			constructor_names[vtt_index]);
		emit_store(PointerTo(Fundamental("char")), construction_vptr, this_address);
		const size_t hidden_count = function.hidden_virtual_bases.size();
		const size_t hidden_begin = function.type->parameters.size() >= hidden_count ?
			function.type->parameters.size() - hidden_count : function.type->parameters.size();
		const vector<RenderedVirtualTableView> construction_views =
			RenderedVirtualTableViews(owner);
		for(size_t view = 0; view < construction_views.size(); ++view) {
			const RenderedVirtualTableView& rendered = construction_views[view];
			if(!rendered.base || rendered.offset == 0) continue;
			string base_address;
			for(size_t hidden = 0; hidden < hidden_count; ++hidden) {
				if(hidden_begin + hidden >= constructor_names.size() ||
					!function.hidden_virtual_bases[hidden] ||
					!PA12SameType(function.hidden_virtual_bases[hidden], rendered.base, true)) continue;
				base_address = new_temp();
				AddInstruction(base_address + " = index i8 %" +
					constructor_names[hidden_begin + hidden] + ", 0");
				break;
			}
			if(base_address.empty()) {
				const string object_address = new_temp();
				AddInstruction(object_address + " = load ptr $this");
				base_address = new_temp();
				AddInstruction(base_address + " = index i8 [projection=base_subobject] " +
					object_address + ", " + integer_text(static_cast<long long>(rendered.offset)));
			}
			const string vtt_element = new_temp();
			AddInstruction(vtt_element + " = index i8 %" +
				constructor_names[vtt_index] + ", " +
				integer_text(static_cast<long long>(ConstructionVttViewIndex(owner, view) * 8)));
			const string view_vptr = new_temp();
			AddInstruction(view_vptr + " = load ptr " + vtt_element);
			emit_store(PointerTo(Fundamental("char")), view_vptr, base_address);
		}
		vptr_stored = true;
	} else if(owner->polymorphic && !vptr_stored && !delegating) {
		EmitVPointerStore(owner, EmitValue(this_node, scope).operand);
		vptr_stored = true;
	}
    for(size_t i = 0; i < owner->class_members.size(); ++i) {
      const ClassMemberInfo& member_fact = owner->class_members[i];
      if(member_fact.is_static || member_fact.name.empty() || !member_fact.type ||
         initialized_members.find(member_fact.name) != initialized_members.end()) continue;
      vector<Binding*> fields = DirectBindings(owner->owned_scope, member_fact.name);
      Binding* field = 0;
      for(size_t j = 0; j < fields.size(); ++j)
        if(fields[j]->kind == BIND_VARIABLE && fields[j]->is_member && !fields[j]->is_static) {
          field = fields[j];
          break;
        }
      if(!field) continue;
      TypePtr field_type = type_value(field->type);
      CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "OP_ARROW:->"));
      member->children.push_back(this_node);
      member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
        "identifier", member_fact.name)));
      vector<CPPGMAstNodePtr> arguments;
      CPPGMAstNodePtr expression;
      if(member_fact.initializer) {
        if(!member_fact.initializer->children.empty() && member_fact.initializer->children[0] &&
           member_fact.initializer->children[0]->kind == "paren-initializer") {
          arguments = member_fact.initializer->children[0]->children;
          if(arguments.size() == 1) expression = arguments[0];
        } else {
          expression = InitializerExpression(member_fact.initializer);
          if(expression && expression->kind == "braced-init-list")
            arguments = expression->children;
          else if(expression) arguments.push_back(expression);
        }
        if(expression && expression->kind == "call-expression" &&
           !expression->children.empty() && expression->children[0] &&
           expression->children[0]->kind == "id-expression" && field_type &&
           field_type->kind == TYPE_CLASS &&
           LastComponent(field_type->name) == expression->children[0]->value) {
          CPPGMAstNodePtr argument_list = expression->children.size() > 1 ?
            expression->children[1] : CPPGMAstNodePtr();
          arguments = argument_list ? argument_list->children : vector<CPPGMAstNodePtr>();
          expression.reset();
        }
      }
      const bool empty_value_initializer = expression &&
        ((expression->kind == "braced-init-list" && expression->children.empty()) ||
         (expression->kind == "paren-initializer" && expression->children.empty()));
      if(arguments.empty() && (!field_type || field_type->kind != TYPE_CLASS) &&
         !empty_value_initializer) continue;
      if(arguments.empty() && field_type && field_type->kind == TYPE_CLASS &&
         !empty_value_initializer && field_type->class_members.empty() &&
         MemberBindings(field_type, LastComponent(field_type->name)).empty()) continue;
      if(arguments.empty() && field_type && field_type->kind == TYPE_CLASS &&
         !empty_value_initializer && !HasDefaultConstructionEffects(field_type) &&
         !HasExplicitConstructor(field_type)) continue;
      if(arguments.empty() && !empty_value_initializer && field_type &&
         field_type->kind == TYPE_CLASS && field_type->is_union &&
         !HasDefaultConstructionEffects(field_type)) {
        bool has_user_constructor = false;
        const vector<Binding*> union_constructors = MemberBindings(
          field_type, LastComponent(field_type->name));
        for(size_t j = 0; j < union_constructors.size(); ++j) {
          FunctionRecord* union_record = RecordForBinding(union_constructors[j]);
          if(union_record && union_record->constructor &&
             !union_record->implicit_constructor &&
             !union_record->aggregate_constructor) {
            has_user_constructor = true;
            break;
          }
        }
        if(!has_user_constructor) continue;
      }
      if(IsBitField(field)) {
        if(arguments.size() != 1) throw logic_error("default member initializer has too many arguments");
        Value value = EmitValue(arguments[0], scope, field->type);
        value = ConvertValue(value, field->type, false, true);
        bool preserve = false;
        if(field->member_owner && field->member_index != static_cast<size_t>(-1) &&
           field->member_index < field->member_owner->class_members.size()) {
          const long long offset = field->member_owner->class_members[field->member_index].offset;
          for(set<string>::const_iterator it = initialized_members.begin();
              it != initialized_members.end() && !preserve; ++it) {
            if(*it == member_fact.name) continue;
            vector<Binding*> prior = DirectBindings(owner->owned_scope, *it);
            for(size_t j = 0; j < prior.size(); ++j)
              if(IsBitField(prior[j]) && prior[j]->member_owner &&
                 prior[j]->member_index != static_cast<size_t>(-1) &&
                 prior[j]->member_index < prior[j]->member_owner->class_members.size() &&
                 prior[j]->member_owner->class_members[prior[j]->member_index].offset == offset) {
                preserve = true;
                break;
              }
          }
        }
        string stored;
        if(preserve) {
          const string read_address = EmitMemberAddress(member, scope);
          stored = MergeBitFieldValue(field, read_address, field->type, value.operand, true);
        } else stored = PrepareBitFieldValue(field, field->type, value.operand);
        const string store_address = EmitMemberAddress(member, scope);
        emit_store(field->type, stored, store_address);
        initialized_members.insert(member_fact.name);
        continue;
      }
      const string address = EmitMemberAddress(member, scope);
      if(field_type && field_type->kind == TYPE_CLASS &&
         !type_is_reference(field->type) &&
         arguments.size() == 1 && arguments[0] &&
         EmitObjectTransferAt(field_type, address, arguments[0], scope, true)) {
        initialized_members.insert(member_fact.name);
        continue;
      }
      if(field_type && field_type->kind == TYPE_CLASS &&
         EmitConstructorAt(field_type, address, arguments, scope)) {
        initialized_members.insert(member_fact.name);
        continue;
      }
      if(expression && expression->kind == "braced-init-list" &&
         field_type && (field_type->kind == TYPE_CLASS || field_type->kind == TYPE_ARRAY)) {
        EmitAggregateAt(address, field_type, expression, scope, member);
        initialized_members.insert(member_fact.name);
        continue;
      }
      if(arguments.empty()) {
        if(empty_value_initializer && (!field_type || field_type->kind != TYPE_CLASS))
          emit_store(field->type,
            field_type && field_type->kind == TYPE_POINTER ? "nullptr" : "0", address);
        if(empty_value_initializer) initialized_members.insert(member_fact.name);
        continue;
      }
      if(arguments.size() != 1) throw logic_error("default member initializer has too many arguments");
      Value value = type_is_reference(field->type) ? Value() :
        EmitValue(arguments[0], scope, field->type);
      if(type_is_reference(field->type)) {
        emit_store(PointerTo(Fundamental("char")),
          EmitReferenceArgument(arguments[0], scope, field->type), address);
      } else if(IsBitField(field)) {
        StoreBitField(field, address, field->type, value.operand, true);
      } else {
        if(value.known_constant && is_integral_type(value.type) &&
           is_integral_type(field->type) &&
           type_size(field->type) > type_size(value.type) &&
           !is_unsigned_type(field->type)) {
          value.type = field->type;
          value.operand = integer_text(value.constant);
        } else value = ConvertValue(value, field->type, false, true);
        emit_store(field->type, value.operand, address);
      }
      initialized_members.insert(member_fact.name);
    }

    if(owner->is_union) return;

    // A synthesized or user-defined constructor still has to initialize
    // class subobjects for which no mem-initializer or default member
    // initializer was written.  Arrays are walked element-wise so their
    // constructors participate in the same lifetime state as scalar members.
    for(size_t i = 0; i < owner->class_members.size(); ++i) {
      const ClassMemberInfo& member_fact = owner->class_members[i];
      if(member_fact.is_static || member_fact.name.empty() || !member_fact.type ||
         initialized_members.find(member_fact.name) != initialized_members.end()) continue;
      vector<Binding*> fields = DirectBindings(owner->owned_scope, member_fact.name);
      Binding* field = 0;
      for(size_t j = 0; j < fields.size(); ++j)
        if(fields[j]->kind == BIND_VARIABLE && fields[j]->is_member && !fields[j]->is_static) {
          field = fields[j];
          break;
        }
      if(!field) continue;
      TypePtr member_type = type_value(field->type);
      if(member_type && member_type->kind == TYPE_ARRAY) {
        TypePtr element_type = type_value(member_type->child);
        if(!element_type || element_type->kind != TYPE_CLASS || member_type->bound < 0) continue;
        if(!HasDefaultConstructionEffects(element_type) &&
           !HasExplicitConstructor(element_type)) continue;
        if(element_type->class_members.empty() && DirectBaseTypes(element_type).empty() &&
           MemberBindings(element_type, LastComponent(element_type->name)).empty())
          continue;
        CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "OP_ARROW:->"));
        member->children.push_back(this_node);
        member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
          "identifier", member_fact.name)));
        for(size_t element_index = 0;
            element_index < static_cast<size_t>(member_type->bound); ++element_index) {
          const string array_address = EmitMemberAddress(member, scope);
          const string decay = new_temp();
          AddInstruction(decay + " = unary decay ptr " + array_address);
          const string offset = new_temp();
          AddInstruction(offset + " = binary mul i64 " +
            integer_text(static_cast<long long>(element_index)) + ", " +
            integer_text(static_cast<long long>(type_size(element_type))));
          const string element = new_temp();
          AddInstruction(element + " = index i8 " + decay + ", " + offset);
          (void)EmitConstructorAt(element_type, element,
            vector<CPPGMAstNodePtr>(), scope);
        }
        continue;
      }
      if(member_type && member_type->kind == TYPE_CLASS) {
        if(member_type->is_union &&
           !HasDefaultConstructionEffects(member_type)) {
          bool has_user_constructor = false;
          const vector<Binding*> union_constructors = MemberBindings(
            member_type, LastComponent(member_type->name));
          for(size_t j = 0; j < union_constructors.size(); ++j) {
            FunctionRecord* union_record = RecordForBinding(union_constructors[j]);
            if(union_record && union_record->constructor &&
               !union_record->implicit_constructor &&
               !union_record->aggregate_constructor) {
              has_user_constructor = true;
              break;
            }
          }
          if(!has_user_constructor) continue;
        }
        if(!HasDefaultConstructionEffects(member_type) &&
           !HasExplicitConstructor(member_type)) continue;
        if(member_type->class_members.empty() && DirectBaseTypes(member_type).empty() &&
           MemberBindings(member_type, LastComponent(member_type->name)).empty())
          continue;
        CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "OP_ARROW:->"));
        member->children.push_back(this_node);
        member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
          "identifier", member_fact.name)));
        const string address = EmitMemberAddress(member, scope);
        (void)EmitConstructorAt(member_type, address,
          vector<CPPGMAstNodePtr>(), scope);
      }
    }
  }


} // namespace cppgm_pa14_lowering
