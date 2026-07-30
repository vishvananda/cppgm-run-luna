#include "pa14_lowering.h"

#include <set>

using namespace std;

namespace cppgm_pa14_lowering {

string PA14Lowerer::EmitTemporaryObjectAddress(const CPPGMAstNodePtr& node,
                                               Scope* scope,
                                               const string& prefix)
{
    if(!node || node->kind != "call-expression" || node->children.empty())
      throw logic_error("invalid temporary object expression");
    TypePtr object_type = ConstructorObjectType(node->children[0], scope);
    if(!object_type) throw logic_error("temporary expression is not a class construction");
    CollectImplicitConstructor(object_type, object_type->owned_scope, true);
    const string slot = new_special_slot(prefix, low_type(object_type));
    const string address = new_temp();
    AddInstruction(address + " = addr $" + slot);
    const CPPGMAstNodePtr argument_list = node->children.size() > 1 ?
      node->children[1] : CPPGMAstNodePtr();
    vector<CPPGMAstNodePtr> arguments = argument_list ?
      argument_list->children : vector<CPPGMAstNodePtr>();
    if(node->value == "braced-construction" && arguments.size() == 1 &&
       arguments[0] && arguments[0]->kind == "braced-init-list")
      arguments = arguments[0]->children;
    if(!EmitConstructorAt(object_type, address, arguments, scope,
                          true, false, false, false, arguments.empty()))
      throw logic_error("no viable temporary object construction");
    RegisterTemporaryObject(object_type, address);
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

bool PA14Lowerer::EmitDestructorAt(const TypePtr& raw_object_type, const string& address,
                                   Scope* scope, bool force_empty)
{
    TypePtr object_type = type_value(raw_object_type);
    if(!object_type || object_type->kind != TYPE_CLASS) return false;
    const string name = "~" + LastComponent(object_type->name);
    vector<Binding*> candidates = MemberBindings(object_type, name);
    for(size_t i = 0; i < candidates.size(); ++i) {
      Binding* binding = candidates[i];
      if(binding->kind != BIND_FUNCTION || !binding->is_member || binding->is_static) continue;
      FunctionRecord* record = RecordForBinding(binding);
      if(!record || !record->destructor) continue;
      if(!HasDestructor(object_type)) continue;
      if(!force_empty && !DestructorHasEffects(object_type)) continue;
      record->needed = true;
      FunctionRecord* base_entry = BaseEntryFor(record);
      FunctionRecord* call_record = object_type->polymorphic && force_empty && base_entry ?
        base_entry : record;
      if(base_entry) base_entry->needed = true;
      AddInstruction("call void @" + call_record->symbol + "(" + address + ")");
      return true;
    }
    (void)scope;
    return false;
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
    bool has_bit_field = false;
    bool has_reference_member = false;
    for(size_t i = 0; i < owner->class_members.size(); ++i)
      if(owner->class_members[i].bit_field) has_bit_field = true;
      else if(owner->class_members[i].type &&
              type_is_reference(owner->class_members[i].type))
        has_reference_member = true;
    const bool defer_destination = assignment && owner->direct_base &&
      IsEmptyBaseStorage(owner->direct_base);
    const bool defer_bit_field_destination = assignment && has_bit_field;
    string destination;
    if(!defer_destination && !defer_bit_field_destination)
      destination = EmitValue(this_node, scope).operand;
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
    bool defaulted_copy_storage = function.copy_assignment && function.defaulted &&
      !owner->direct_base;
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
    set<long long> copied_bit_offsets;
    if((IsTrivialValueStorage(owner) && !has_reference_member &&
        !(assignment && has_bit_field)) ||
       defaulted_copy_storage) {
      source = emit_load(source_storage, PointerTo(Fundamental("char")));
      AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(owner))) +
        "x" + integer_text(static_cast<long long>(type_alignment(owner))) + " " +
        source + ", " + destination);
    } else {
      if(owner->direct_base) {
        TypePtr base = type_value(owner->direct_base);
        if(!IsEmptyBaseStorage(base)) {
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
            base_record->needed = true;
            FunctionRecord* base_call = BaseEntryFor(base_record);
            if(base_call) base_call->needed = true;
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
        if(assignment && owner->direct_base &&
           IsEmptyBaseStorage(owner->direct_base) && member_type &&
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
              member_move_constructor->needed = true;
          }
          FunctionRecord* member_record = assignment ?
            EnsureImplicitAssignment(member_type, move) :
            EnsureImplicitCopyConstructor(member_type, move);
          if(!member_record || member_record->deleted)
            throw logic_error("value member has deleted class operation");
          member_record->needed = true;
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
    TypePtr base = type_value(owner->direct_base);
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
    bool explicitly_initialized_base = false;
    if(base && function.special_initializer) {
      for(size_t i = 0; i < function.special_initializer->children.size(); ++i) {
        CPPGMAstNodePtr initializer = function.special_initializer->children[i];
        if(!initializer || initializer->kind != "mem-initializer") continue;
        CPPGMAstNodePtr name_node = ChildOfKind(initializer, "mem-initializer-id");
        bool matches_base = name_node &&
          (LastComponent(name_node->value) == LastComponent(base->name) ||
           name_node->value == base->name);
        if(name_node && !matches_base) {
          Analyzer::PathTarget alias = analyzer_.ResolvePath(scope, LastComponent(name_node->value));
          TypePtr alias_type = alias.binding ? type_value(alias.binding->type) : TypePtr();
          matches_base = alias_type && alias_type == base;
        }
        if(matches_base) {
          explicitly_initialized_base = true;
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
            record->needed = true;
        }
      }
    }
	if(base && !defer_dependent_base && !delegating &&
       !explicitly_initialized_base && HasConstructor(base) &&
       (!IsEmptyBaseStorage(base) || HasDefaultConstructionEffects(base) ||
        HasUserProvidedConstructor(base))) {
      const string this_address = EmitValue(this_node, scope).operand;
      const string base_address = AdjustBaseAddress(this_address, owner, base);
      (void)EmitConstructorAt(base, base_address, vector<CPPGMAstNodePtr>(), scope,
        true, true);
    }
    vector<CPPGMAstNodePtr> ordered_initializers;
    set<const CPPGMAstNode*> used_initializers;
    if(function.special_initializer) {
      for(size_t i = 0; i < function.special_initializer->children.size(); ++i) {
        CPPGMAstNodePtr initializer = function.special_initializer->children[i];
        if(!initializer || initializer->kind != "mem-initializer") continue;
        CPPGMAstNodePtr name_node = ChildOfKind(initializer, "mem-initializer-id");
        const string name = name_node ? LastComponent(name_node->value) : string();
        if(base && (name == LastComponent(base->name) || name == base->name)) {
          ordered_initializers.push_back(initializer);
          used_initializers.insert(initializer.get());
        }
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
      const bool is_base_initializer = base &&
        (name == LastComponent(base->name) || name == base->name);
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
      if(base && (name == LastComponent(base->name) || name == base->name)) named_base = base;
      if(base && !named_base) {
        Analyzer::PathTarget alias = analyzer_.ResolvePath(scope, name);
        TypePtr alias_type = alias.binding ? type_value(alias.binding->type) : TypePtr();
        if(alias_type && alias_type == base) named_base = base;
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
        if(!EmitConstructorAt(named_base, base_address, arguments, scope, true, true))
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
        TypePtr nested_base = type_value(field_type->direct_base);
        if(nested_base) {
          const string base_address = address;
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
	if(owner->polymorphic && !vptr_stored && !delegating) {
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
        if(element_type->class_members.empty() && !element_type->direct_base &&
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
        if(member_type->class_members.empty() && !member_type->direct_base &&
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
