#include "pa14_lowering.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace std;

namespace cppgm_pa14_lowering {

namespace {

bool HasIntegralTemplateArgument(const TypePtr& raw_type)
{
  const TypePtr type = type_value(raw_type);
  if(!type || !type->template_specialization) return false;
  for(size_t i = 0; i < type->template_arguments.size(); ++i) {
    PA19IntegralValue value;
    if(PA19ParseInteger(type->template_arguments[i], &value) ||
       PA19DecodeCharacter(type->template_arguments[i], &value)) return true;
    const string argument = PA19Compact(type->template_arguments[i]);
    if(argument == "true" || argument == "false") return true;
  }
  return false;
}

} // namespace

PA14Lowerer::Value PA14Lowerer::EmitNewExpression(const CPPGMAstNodePtr& node,
                                                  Scope* scope,
                                                  const TypePtr& expected)
{
    (void)expected;
    if(!node) throw logic_error("missing new-expression during LowIR lowering");
    CPPGMAstNodePtr type_id;
    CPPGMAstNodePtr placement;
    CPPGMAstNodePtr initializer;
    for(size_t i = 0; i < node->children.size(); ++i) {
      const CPPGMAstNodePtr child = node->children[i];
      if(!child) continue;
      if(child->kind == "type-id") type_id = child;
      else if(child->kind == "placement") placement = child;
      else if(child->kind == "initializer" || child->kind == "braced-init-list")
        initializer = child;
    }
    if(!type_id) throw logic_error("new-expression has no allocated type");

    CPPGMAstNodePtr declarator;
    CPPGMAstNodePtr array_suffix;
    bool value_initialize = false;
    for(size_t i = 0; i < type_id->children.size(); ++i) {
      const CPPGMAstNodePtr child = type_id->children[i];
      if(!child || child->kind != "abstract-declarator") continue;
      declarator = child;
      for(size_t j = 0; j < child->children.size(); ++j) {
        if(!child->children[j]) continue;
        if(child->children[j]->kind == "array-suffix") array_suffix = child->children[j];
        else if(child->children[j]->kind == "parameter-clause") value_initialize = true;
      }
    }
    const bool array_new = static_cast<bool>(array_suffix);
    TypePtr allocated_type;
    try {
      allocated_type = type_value(analyzer_.TypeFromTypeId(type_id, scope));
    } catch(const logic_error&) {
      if(!array_new || type_id->children.empty()) throw;
      allocated_type = type_value(analyzer_.TypeFromSpecSeq(type_id->children[0], scope));
    }
    if(!allocated_type) throw logic_error("new-expression has no allocated type");

    TypePtr element_type;
    if(array_new) {
      // A dynamic bound is not part of the analyzer's declarator type.  The
      // new-type-id still has a typed specifier sequence, which is the
      // element type needed by allocation, construction, and delete[].
      element_type = type_value(analyzer_.TypeFromSpecSeq(type_id->children[0], scope));
      if(allocated_type->kind == TYPE_ARRAY && allocated_type->child &&
         allocated_type->child->kind != TYPE_FUNCTION)
        element_type = type_value(allocated_type->child);
      if(!element_type) throw logic_error("array new-expression has no element type");
    }

    CPPGMAstNodePtr bound_node;
    bool fixed_count = false;
    long long fixed_elements = 0;
    Value dynamic_count;
    if(array_new) {
      if(array_suffix && !array_suffix->children.empty()) {
        bound_node = array_suffix->children[0];
        ExprInfo bound_info = Infer(bound_node, scope);
        fixed_count = bound_info.known_constant;
        if(fixed_count) fixed_elements = bound_info.constant;
      } else fixed_count = true;
      if(fixed_count && fixed_elements < 0)
        throw logic_error("negative array bound in new-expression");
      if(!fixed_count) dynamic_count = EmitValue(bound_node, scope);
    }

    vector<CPPGMAstNodePtr> placement_arguments;
    if(placement) {
      for(size_t i = 0; i < placement->children.size(); ++i) {
        const CPPGMAstNodePtr child = placement->children[i];
        if(!child) continue;
        if(child->kind == "paren-argument-list" || child->kind == "argument-list" ||
           child->kind == "braced-init-list") {
          for(size_t j = 0; j < child->children.size(); ++j)
            placement_arguments.push_back(child->children[j]);
        } else placement_arguments.push_back(child);
      }
    }

    vector<CPPGMAstNodePtr> allocation_arguments;
    CPPGMAstNodePtr size(new CPPGMAstNode("literal", "1"));
    Value allocation_size;
    string size_slot;
    if(array_new && fixed_count) {
      const size_t element_size = type_size(element_type);
      const long long bytes = static_cast<long long>(fixed_elements) *
        static_cast<long long>(element_size) +
        (type_value(element_type)->kind == TYPE_CLASS ? 8 : 0);
      size->value = integer_text(bytes);
    } else if(!array_new) {
      size->value = integer_text(static_cast<long long>(type_size(allocated_type)));
    } else {
      const TypePtr count_type = type_value(dynamic_count.type);
      if(!count_type || !is_integral_type(count_type))
        throw logic_error("array bound is not integral");
      Value total = dynamic_count;
      const size_t element_size = type_size(element_type);
      if(element_size != 1) {
        const string scaled = new_temp();
        AddInstruction(scaled + " = binary mul " + low_type(count_type) + " " +
          dynamic_count.operand + ", " + integer_text(static_cast<long long>(element_size)));
        total.operand = scaled;
      }
      if(type_value(element_type)->kind == TYPE_CLASS) {
        const string with_cookie = new_temp();
        AddInstruction(with_cookie + " = binary add " + low_type(count_type) + " " +
          total.operand + ", 8");
        total.operand = with_cookie;
      }
      allocation_size = ConvertValue(total, Fundamental("long int"), false, true);
      size_slot = new_special_slot("array_new_size", "i64");
      emit_store(Fundamental("long int"), allocation_size.operand, "$" + size_slot);
    }
    allocation_arguments.push_back(size);
    allocation_arguments.insert(allocation_arguments.end(), placement_arguments.begin(),
      placement_arguments.end());

    string allocator_name = array_new ? "operatornew[]" : "operatornew";
    TypePtr allocation_class = array_new ? type_value(element_type) : allocated_type;
    if(allocation_class && allocation_class->kind == TYPE_CLASS) {
      const vector<Binding*> members = MemberBindings(allocation_class, allocator_name);
      bool class_allocator = false;
      for(size_t i = 0; i < members.size(); ++i)
        if(members[i] && members[i]->kind == BIND_FUNCTION && members[i]->is_static) {
          class_allocator = true;
          break;
      }
      if(class_allocator)
        allocator_name = TypeQualifiedName(allocation_class) + "::" + allocator_name;
    }
    CPPGMAstNodePtr callee(new CPPGMAstNode("id-expression", allocator_name));
    CPPGMAstNodePtr arguments(new CPPGMAstNode("paren-argument-list"));
    arguments->children = allocation_arguments;
    CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
    call->children.push_back(callee);
    call->children.push_back(arguments);
    CallChoice allocation_choice = ChooseCall(call, scope);
    if(!allocation_choice.binding)
      throw logic_error("new-expression has no viable allocation function");
    // EmitChosenCall evaluates AST arguments itself.  Dynamic array bounds
    // have already been evaluated above, so feed the saved size operand to a
    // small direct-call bridge and evaluate only placement arguments here.
    Value allocated;
    if(array_new && !fixed_count) {
      FunctionRecord* record = RecordForBinding(allocation_choice.binding);
      if(!record || allocation_choice.function->parameters.empty())
        throw logic_error("new-expression has no allocation record");
      record->needed = true;
      FunctionRecord* base_entry = BaseEntryFor(record);
      if(base_entry) base_entry->needed = true;
      Value first = ConvertValue(allocation_size,
        allocation_choice.function->parameters[0], false, true);
      vector<string> operands;
      operands.push_back(first.operand);
      vector<CPPGMAstNodePtr> args = placement_arguments;
      while(args.size() + 1 < allocation_choice.function->parameters.size()) {
        const size_t index = args.size() + 1;
        if(index >= record->default_arguments.size() || !record->default_arguments[index]) break;
        args.push_back(InitializerExpression(record->default_arguments[index]));
      }
      for(size_t i = 0; i < args.size(); ++i) {
        const size_t parameter = i + 1;
        TypePtr target = parameter < allocation_choice.function->parameters.size() ?
          allocation_choice.function->parameters[parameter] : TypePtr();
        if(target && type_is_reference(target))
          operands.push_back(EmitReferenceArgument(args[i], scope, target));
        else {
          Value value = EmitValue(args[i], scope, target);
          if(target) value = ConvertValue(value, target, false, true);
          operands.push_back(value.operand);
        }
      }
      string text = "call " + low_type(record->type->child) + " @" + record->symbol + "(";
      for(size_t i = 0; i < operands.size(); ++i) {
        if(i) text += ", ";
        text += operands[i];
      }
      text += ")";
      if(low_type(record->type->child) == "void") {
        AddInstruction(text);
        allocated.type = allocation_choice.function->child;
      } else {
        allocated.type = allocation_choice.function->child;
        allocated.operand = new_temp();
        AddInstruction(allocated.operand + " = " + text);
      }
    } else allocated = EmitChosenCall(allocation_choice, callee, allocation_arguments, scope);

    vector<CPPGMAstNodePtr> initializer_arguments;
    bool has_initializer = initializer.get() != 0;
    if(initializer) {
      if(initializer->kind == "initializer" && !initializer->children.empty()) {
        const CPPGMAstNodePtr init = initializer->children[0];
        if(init && (init->kind == "paren-initializer" ||
                    init->kind == "braced-init-list"))
          initializer_arguments = init->children;
        else if(init) initializer_arguments.push_back(init);
      } else if(initializer->kind == "braced-init-list") {
        initializer_arguments = initializer->children;
      }
    }

    if(array_new) {
      const bool class_array = type_value(element_type)->kind == TYPE_CLASS;
      string init_label;
      string end_label;
      bool nothrow = allocation_choice.function && allocation_choice.function->parameters.size() >= 2;
      if(nothrow) {
        const TypePtr parameter = allocation_choice.function->parameters[1];
        const TypePtr parameter_value = type_value(parameter);
        nothrow = type_is_reference(parameter) && parameter_value &&
          parameter_value->kind == TYPE_CLASS && LastComponent(parameter_value->name) == "nothrow_t";
      }
      if(nothrow) {
        init_label = new_label("new_array_init");
        end_label = new_label("new_array_end");
        const string nonnull = new_temp();
        AddInstruction(nonnull + " = cmp ne ptr " + allocated.operand + ", 0");
        Terminate("branch " + nonnull + ", ^" + init_label + ", ^" + end_label);
        AddBlock(init_label);
      }

      Value element_count;
      string element_base = allocated.operand;
      if(class_array) {
        const string total = !fixed_count ? emit_load("$" + size_slot,
          Fundamental("long int")) : string();
        const string user_pointer = new_temp();
        AddInstruction(user_pointer + " = index i8 " + allocated.operand + ", 8");
        element_base = user_pointer;
        if(fixed_count) {
          element_count.type = Fundamental("long int");
          element_count.operand = new_temp();
          element_count.known_constant = true;
          element_count.constant = fixed_elements;
          AddInstruction(element_count.operand + " = const i64 " +
            integer_text(fixed_elements));
        } else {
          const string payload = new_temp();
          AddInstruction(payload + " = binary sub i64 " + total + ", 8");
          element_count.type = Fundamental("long int");
          element_count.operand = new_temp();
          AddInstruction(element_count.operand + " = binary udiv i64 " + payload + ", " +
            integer_text(static_cast<long long>(type_size(element_type))));
        }
        emit_store(Fundamental("long int"), element_count.operand, allocated.operand);
        const bool needs_constructor = HasDefaultInitializationEffects(element_type) &&
          HasConstructor(element_type);
        if(needs_constructor) {
          if(fixed_count) {
            element_count.operand = new_temp();
            AddInstruction(element_count.operand + " = const i64 " +
              integer_text(fixed_elements));
          }
          const string index_slot = new_special_slot("array_new_index", "i64");
          emit_store(Fundamental("long int"), "0", "$" + index_slot);
          const string condition = new_label("array_new_ctor_cond");
          const string body = new_label("array_new_ctor_body");
          const string finish = new_label("array_new_ctor_end");
          Terminate("jump ^" + condition);
          AddBlock(condition);
          const string index = emit_load("$" + index_slot, Fundamental("long int"));
          const string test = new_temp();
          AddInstruction(test + " = cmp ult i64 " + index + ", " + element_count.operand);
          Terminate("branch " + test + ", ^" + body + ", ^" + finish);
          AddBlock(body);
          const string offset = new_temp();
          AddInstruction(offset + " = binary mul i64 " + index + ", " +
            integer_text(static_cast<long long>(type_size(element_type))));
          const string element = new_temp();
          AddInstruction(element + " = index i8 " + element_base + ", " + offset);
          if(!EmitConstructorAt(element_type, element, vector<CPPGMAstNodePtr>(), scope))
            throw logic_error("array new-expression has no default constructor");
          const string next = new_temp();
          AddInstruction(next + " = binary add i64 " + index + ", 1");
          emit_store(Fundamental("long int"), next, "$" + index_slot);
          Terminate("jump ^" + condition);
          AddBlock(finish);
        }
      } else if(value_initialize) {
        string total_bytes;
        if(!fixed_count) total_bytes = emit_load("$" + size_slot,
          Fundamental("long int"));
        else {
          total_bytes = new_temp();
          AddInstruction(total_bytes + " = const i64 " + integer_text(
            fixed_elements * static_cast<long long>(type_size(element_type))));
        }
        const string offset_slot = new_special_slot("zeroinit_offset", "i64");
        emit_store(Fundamental("long int"), "0", "$" + offset_slot);
        const string condition = new_label("zeroinit_cond");
        const string body = new_label("zeroinit_body");
        const string finish = new_label("zeroinit_end");
        Terminate("jump ^" + condition);
        AddBlock(condition);
        const string offset = emit_load("$" + offset_slot, Fundamental("long int"));
        const string test = new_temp();
        AddInstruction(test + " = cmp ult i64 " + offset + ", " + total_bytes);
        Terminate("branch " + test + ", ^" + body + ", ^" + finish);
        AddBlock(body);
        const string address = new_temp();
        AddInstruction(address + " = index i8 " + allocated.operand + ", " + offset);
        Value zero;
        zero.type = Fundamental("int");
        zero.operand = "0";
        zero.known_constant = true;
        zero.constant = 0;
        TypePtr value_type = type_value(element_type);
        if(value_type && (is_integral_type(value_type) ||
                          (value_type->kind == TYPE_FUNDAMENTAL &&
                           value_type->name == "bool"))) {
          zero.type = value_type;
          zero.operand = "0";
        } else if(value_type && value_type->kind == TYPE_POINTER) {
          zero.type = value_type;
          zero.operand = "nullptr";
        } else zero = ConvertValue(zero, element_type, false, true);
        if(value_type && value_type->kind == TYPE_FUNDAMENTAL &&
           value_type->name == "bool")
          AddInstruction("store i8 0, " + address);
        else emit_store(element_type, zero.operand, address);
        const string next = new_temp();
        AddInstruction(next + " = binary add i64 " + offset + ", " +
          integer_text(static_cast<long long>(type_size(element_type))));
        emit_store(Fundamental("long int"), next, "$" + offset_slot);
        Terminate("jump ^" + condition);
        AddBlock(finish);
      }
      if(nothrow) {
        if(!state_->current->terminated) Terminate("jump ^" + end_label);
        AddBlock(end_label);
      }
      Value result;
      result.type = PointerTo(element_type);
      result.operand = class_array ? element_base : allocated.operand;
      return result;
    }

    bool nothrow = false;
    if(allocation_choice.function && allocation_choice.function->parameters.size() >= 2) {
      const TypePtr parameter = allocation_choice.function->parameters[1];
      const TypePtr parameter_value = type_value(parameter);
      nothrow = type_is_reference(parameter) && parameter_value &&
        parameter_value->kind == TYPE_CLASS &&
        LastComponent(parameter_value->name) == "nothrow_t";
    }
    const string init_label = nothrow ? new_label("new_init") : string();
    const string end_label = nothrow ? new_label("new_end") : string();
    if(nothrow) {
      const string nonnull = new_temp();
      AddInstruction(nonnull + " = cmp ne ptr " + allocated.operand + ", 0");
      Terminate("branch " + nonnull + ", ^" + init_label + ", ^" + end_label);
      AddBlock(init_label);
    }
    if(allocated_type->kind == TYPE_CLASS) {
      if(!EmitConstructorAt(allocated_type, allocated.operand,
                            initializer_arguments, scope))
        throw logic_error("new-expression has no viable constructor");
    } else if(!initializer_arguments.empty()) {
      if(initializer_arguments.size() != 1)
        throw logic_error("scalar new-expression has too many initializers");
      Value source = EmitValue(initializer_arguments[0], scope, allocated_type);
      source = ConvertValue(source, allocated_type, false, true);
      emit_store(allocated_type, source.operand, allocated.operand);
    } else if(has_initializer) {
      Value zero;
      zero.type = Fundamental("int");
      zero.operand = "0";
      zero.known_constant = true;
      zero.constant = 0;
      zero = ConvertValue(zero, allocated_type, false, true);
      emit_store(allocated_type, zero.operand, allocated.operand);
    }
    if(nothrow) {
      if(!state_->current->terminated) Terminate("jump ^" + end_label);
      AddBlock(end_label);
    }
    Value result;
    result.type = PointerTo(allocated_type);
    result.operand = allocated.operand;
    return result;
  }

PA14Lowerer::Value PA14Lowerer::EmitDeleteExpression(const CPPGMAstNodePtr& node,
                                                     Scope* scope)
{
    if(!node) throw logic_error("missing delete-expression during LowIR lowering");
    CPPGMAstNodePtr pointer_node;
    bool array_delete = false;
    for(size_t i = 0; i < node->children.size(); ++i) {
      const CPPGMAstNodePtr child = node->children[i];
      if(!child) continue;
      if(child->kind == "array-delete") array_delete = true;
      else if(child->kind != "global-scope") pointer_node = child;
    }
    if(!pointer_node) throw logic_error("delete-expression has no operand");
    ExprInfo pointer_info = Infer(pointer_node, scope);
    TypePtr pointer_type = expression_value_type(pointer_info);
    Value pointer = EmitValue(pointer_node, scope);
    TypePtr object_type;
    if(pointer_type && pointer_type->kind == TYPE_POINTER)
      object_type = type_value(pointer_type->child);
    else if(pointer_type && pointer_type->kind != TYPE_FUNDAMENTAL)
      throw logic_error("delete-expression operand is not a pointer");

    const string deallocator_name = array_delete ? "operatordelete[]" : "operatordelete";
    const string deallocator_symbol = [&]() -> string {
      CPPGMAstNodePtr callee(new CPPGMAstNode("id-expression", deallocator_name));
      CPPGMAstNodePtr arguments(new CPPGMAstNode("paren-argument-list"));
      arguments->children.push_back(pointer_node);
      CPPGMAstNodePtr call(new CPPGMAstNode("call-expression"));
      call->children.push_back(callee);
      call->children.push_back(arguments);
      CallChoice choice = ChooseCall(call, scope);
      if(!choice.binding || !choice.function)
        throw logic_error("delete-expression has no viable deallocation function");
      FunctionRecord* record = RecordForBinding(choice.binding);
      if(!record) throw logic_error("delete-expression has no deallocation symbol");
      record->needed = true;
      FunctionRecord* base_entry = BaseEntryFor(record);
      if(base_entry) base_entry->needed = true;
      return record->symbol;
    }();

    const bool class_delete = object_type && object_type->kind == TYPE_CLASS && !array_delete;
    const bool class_array_delete = object_type && object_type->kind == TYPE_CLASS && array_delete;
    if(class_delete) {
      const string nonnull_label = new_label("delete_nonnull");
      const string end_label = new_label("delete_end");
      const string nonnull = new_temp();
      AddInstruction(nonnull + " = cmp ne ptr " + pointer.operand + ", 0");
      Terminate("branch " + nonnull + ", ^" + nonnull_label + ", ^" + end_label);
      AddBlock(nonnull_label);
      if(object_type->polymorphic) {
        size_t deleting_slot = 0;
        if(VirtualDestructorDeletingSlot(object_type, &deleting_slot)) {
          const string vptr = emit_load(pointer.operand,
            PointerTo(Fundamental("char")));
          const string entry_address = new_temp();
          AddInstruction(entry_address + " = index i8 " + vptr + ", " +
            integer_text(static_cast<long long>(deleting_slot * 8)));
          const string entry = emit_load(entry_address,
            PointerTo(Fundamental("char")));
          AddInstruction("call void " + entry + "(" + pointer.operand + ") as (%arg0 : ptr) -> void");
        } else {
          (void)EmitDestructorAt(object_type, pointer.operand, scope);
          AddInstruction("call void @" + deallocator_symbol + "(" + pointer.operand + ")");
        }
      } else {
        (void)EmitDestructorAt(object_type, pointer.operand, scope);
        AddInstruction("call void @" + deallocator_symbol + "(" + pointer.operand + ")");
      }
      Terminate("jump ^" + end_label);
      AddBlock(end_label);
    } else if(class_array_delete) {
      const string nonnull_label = new_label("array_delete_nonnull");
      const string end_label = new_label("array_delete_end");
      const string nonnull = new_temp();
      AddInstruction(nonnull + " = cmp ne ptr " + pointer.operand + ", 0");
      Terminate("branch " + nonnull + ", ^" + nonnull_label + ", ^" + end_label);
      AddBlock(nonnull_label);
      const string raw = new_temp();
      AddInstruction(raw + " = index i8 " + pointer.operand + ", -8");
      const TypePtr count_type = Fundamental("long int");
      const string count = emit_load(raw, count_type);
      if(DestructorHasEffects(object_type)) {
        const string index_slot = new_special_slot("array_delete_index", "i64");
        emit_store(count_type, count, "$" + index_slot);
        const string condition = new_label("array_delete_dtor_cond");
        const string body = new_label("array_delete_dtor_body");
        const string finish = new_label("array_delete_dtor_end");
        Terminate("jump ^" + condition);
        AddBlock(condition);
        const string index = emit_load("$" + index_slot, count_type);
        const string test = new_temp();
        AddInstruction(test + " = cmp ne i64 " + index + ", 0");
        Terminate("branch " + test + ", ^" + body + ", ^" + finish);
        AddBlock(body);
        const string previous = new_temp();
        AddInstruction(previous + " = binary sub i64 " + index + ", 1");
        emit_store(count_type, previous, "$" + index_slot);
        const string offset = new_temp();
        AddInstruction(offset + " = binary mul i64 " + previous + ", " +
          integer_text(static_cast<long long>(type_size(object_type))));
        const string element = new_temp();
        AddInstruction(element + " = index i8 " + pointer.operand + ", " + offset);
        (void)EmitDestructorAt(object_type, element, scope);
        Terminate("jump ^" + condition);
        AddBlock(finish);
      }
      AddInstruction("call void @" + deallocator_symbol + "(" + raw + ")");
      Terminate("jump ^" + end_label);
      AddBlock(end_label);
    } else {
      AddInstruction("call void @" + deallocator_symbol + "(" + pointer.operand + ")");
    }
    Value result;
    result.type = Fundamental("void");
    return result;
  }

bool PA14Lowerer::EmitObjectTransferAt(const TypePtr& raw_target,
                                       const string& destination,
                                       const CPPGMAstNodePtr& source,
                                       Scope* scope, bool allow_explicit,
                                       bool implicit_return_move)
{
    TypePtr target = type_value(raw_target);
    if(!target || target->kind != TYPE_CLASS || !source) return false;
    if(source->kind == "braced-init-list") {
      const vector<CPPGMAstNodePtr> arguments = source->children;
      const bool allow_aggregate = !arguments.empty() &&
        EnsureAggregateConstructor(target);
      return EmitConstructorAt(target, destination, arguments, scope,
        allow_explicit, false, allow_aggregate);
    }
    if(source->kind == "conditional-expression") {
      ExprInfo conditional_info = Infer(source, scope);
      TypePtr conditional_type = expression_value_type(conditional_info);
      if(conditional_type && conditional_type->kind == TYPE_CLASS &&
         (PA12SameType(conditional_type, target, true) ||
          IsDerivedFrom(conditional_type, target))) {
        const string then_label = new_label("condobj_then");
        const string else_label = new_label("condobj_else");
        const string end_label = new_label("condobj_end");
        EmitCondition(source->children[0], scope, then_label, else_label);
        AddBlock(then_label);
        if(!EmitObjectTransferAt(target, destination, source->children[1], scope,
                                 allow_explicit)) return false;
        if(!state_->current->terminated) Terminate("jump ^" + end_label);
        AddBlock(else_label);
        if(!EmitObjectTransferAt(target, destination, source->children[2], scope,
                                 allow_explicit)) return false;
        if(!state_->current->terminated) Terminate("jump ^" + end_label);
        AddBlock(end_label);
        return true;
      }
    }
    if(source->kind == "cast-expression" && source->children.size() > 1) {
      TypePtr cast_type = analyzer_.TypeFromTypeId(source->children[0], scope);
      if(cast_type && type_value(cast_type) &&
         type_value(cast_type)->kind == TYPE_CLASS &&
         PA12SameType(type_value(cast_type), target, true)) {
        vector<CPPGMAstNodePtr> arguments;
        // Preserve a reference cast's value category.  Dropping the cast
        // turns static_cast<T&&>(object) back into an lvalue and selects the
        // copy constructor instead of the move constructor.
        const bool move = cast_type->kind == TYPE_RVALUE_REFERENCE;
        if(type_is_reference(cast_type)) {
          FunctionRecord* value_member = EnsureImplicitCopyConstructor(target, move);
          if(value_member && value_member->deleted) return false;
          if(PA12SameType(type_value(cast_type), target, true) &&
             IsTrivialValueStorage(target) &&
             !(move && ClassHasDeclaredMoveMember(target))) {
            const string source_address = EmitAddress(source->children[1], scope);
            AddInstruction("copyobj " +
              integer_text(static_cast<long long>(type_size(target))) + "x" +
              integer_text(static_cast<long long>(type_alignment(target))) + " " +
              source_address + ", " + destination);
            return true;
          }
        }
        arguments.push_back(type_is_reference(cast_type) ? source : source->children[1]);
        return EmitConstructorAt(target, destination, arguments, scope, true);
      }
    }
    TypePtr constructed = source->kind == "call-expression" &&
      !source->children.empty() ? ConstructorObjectType(source->children[0], scope) : TypePtr();
    if(constructed && PA12SameType(constructed, target, true)) {
      CPPGMAstNodePtr argument_list = source->children.size() > 1 ?
        source->children[1] : CPPGMAstNodePtr();
      vector<CPPGMAstNodePtr> arguments = argument_list ? argument_list->children :
        vector<CPPGMAstNodePtr>();
      if(source->value == "braced-construction" && arguments.size() == 1 &&
         arguments[0] && arguments[0]->kind == "braced-init-list")
        arguments = arguments[0]->children;
      if(arguments.empty())
        CollectImplicitConstructor(constructed, constructed->owned_scope, true);
      const bool allow_aggregate = !arguments.empty() &&
        EnsureAggregateConstructor(constructed);
      const bool value_initialization = arguments.empty() && HasConstructor(constructed) &&
        (source->value == "braced-construction" || HasIntegralTemplateArgument(constructed));
      return EmitConstructorAt(target, destination, arguments, scope, allow_explicit,
        false, allow_aggregate, false, value_initialization);
    }
    if(!constructed && source->kind == "call-expression") {
      CallChoice choice = ChooseCall(source, scope);
      FunctionRecord* function = choice.binding ? RecordForBinding(choice.binding) : 0;
      TypePtr result_type = expression_value_type(Infer(source, scope));
      if(function && function->indirect_result && result_type &&
         PA12SameType(result_type, target, true)) {
        CPPGMAstNodePtr argument_list = source->children.size() > 1 ?
          source->children[1] : CPPGMAstNodePtr();
        vector<CPPGMAstNodePtr> arguments = argument_list ? argument_list->children :
          vector<CPPGMAstNodePtr>();
        EmitChosenCall(choice, source->children[0], arguments, scope, destination);
        return true;
      }
    }
    ExprInfo source_info = Infer(source, scope);
    TypePtr source_type = expression_value_type(source_info);
    if(!source_type || source_type->kind != TYPE_CLASS) {
      vector<CPPGMAstNodePtr> arguments;
      arguments.push_back(source);
      return EmitConstructorAt(target, destination, arguments, scope, true);
    }
    // A class-valued conversion is still a constructor boundary in C++11:
    // materialize the conversion result, then initialize the destination
    // through its copy/move constructor.  Passing destination directly as
    // the conversion's ABI result slot skips that boundary and changes both
    // object lifetime and the generated call sequence.
    if(target && target->kind == TYPE_CLASS) {
      vector<CPPGMAstNodePtr> constructor_arguments;
      constructor_arguments.push_back(source);
      // This is the copy-initialization boundary between two class values.
      // Its destination constructor set excludes explicit constructors even
      // when the surrounding lowering path permits explicit initialization.
      if(!PA12SameType(source_type, target, true)) {
        bool constructed = false;
        try {
          constructed = EmitConstructorAt(target, destination,
            constructor_arguments, scope, false);
        } catch(const PA14NoViableConstructor&) {
          // A failed destination-constructor candidate permits the source
          // conversion-function candidate.  Ambiguity and other lowering
          // failures remain hard errors and are deliberately not caught.
        }
        if(constructed) return true;
      }
      Binding* conversion = FindConversionOperator(source_type, target, true);
      if(conversion) {
        FunctionRecord* conversion_record = RecordForBinding(conversion);
        CallChoice choice;
        choice.binding = conversion;
        choice.function = function_target_type(conversion->type);
        choice.object = source;
        choice.direct = true;
        choice.member = true;
        choice.static_member = false;
        choice.conversion = true;
        const Value converted = EmitChosenCall(choice, CPPGMAstNodePtr(),
          vector<CPPGMAstNodePtr>(), scope, destination);
        if(conversion_record && conversion_record->indirect_result) return true;
        if(converted.operand.empty()) return false;
        AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(target))) +
          "x" + integer_text(static_cast<long long>(type_alignment(target))) +
          " " + converted.operand + ", " + destination);
        return true;
      }
    }
    const bool same_type = PA12SameType(source_type, target, true);
    const bool move = source_info.category == "xvalue" || implicit_return_move;
    const bool template_context = type_value(target)->template_specialization ||
      (scope && scope->owner_type &&
       type_value(scope->owner_type)->template_specialization) ||
      (state_ && state_->record && state_->record->template_instantiation) ||
      (state_ && state_->record && state_->record->member_owner &&
       state_->record->member_owner->template_specialization);
    if(same_type && source_info.category == "lvalue" && template_context &&
       IsEmptyBaseStorage(target) && IsTrivialValueStorage(target)) {
      // Even an empty object reference must be evaluated.  This matters when
      // the lvalue is the result of a comma expression whose discarded side
      // contains an aggregate construction.
      VariablePlan* source_local = source->kind == "id-expression" ?
        FindLocalPlan(source->value) : 0;
      if((source_local && (source_local->parameter || implicit_return_move)) ||
         (source->kind == "binary-expression" && PA12Operator(source->value) == ","))
        (void)EmitAddress(source, scope);
      return true;
    }
    if(source_type && source_type->kind == TYPE_CLASS &&
       IsDerivedFrom(source_type, target)) {
      FunctionRecord* target_copy = FindValueMember(target, false, false);
      bool has_single_argument_conversion = false;
      const vector<Binding*> target_constructors =
        MemberBindings(target, LastComponent(target->name));
      for(size_t i = 0; i < target_constructors.size(); ++i) {
        FunctionRecord* candidate = RecordForBinding(target_constructors[i]);
        TypePtr candidate_type = function_target_type(target_constructors[i]->type);
        if(candidate && candidate->constructor && !candidate->copy_constructor &&
           !candidate->move_constructor && candidate_type &&
           candidate_type->parameters.size() == 1) {
          has_single_argument_conversion = true;
          break;
        }
      }
      if(IsTrivialValueStorage(target) && target_copy && !target_copy->deleted &&
         (target_copy->defaulted || target_copy->implicit_constructor ||
          target_copy->synthesized_value_member) && !has_single_argument_conversion &&
         (source_info.category == "lvalue" || source_info.category == "xvalue")) {
        string source_address = AdjustBaseAddress(EmitAddress(source, scope),
          source_type, target);
        AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(target))) +
          "x" + integer_text(static_cast<long long>(type_alignment(target))) + " " +
          source_address + ", " + destination);
        return true;
      }
      bool empty_target = !target->direct_base;
      for(size_t member_index = 0; member_index < target->class_members.size();
          ++member_index)
        if(!target->class_members[member_index].is_static &&
           !target->class_members[member_index].name.empty()) {
          empty_target = false;
          break;
        }
      if(empty_target) return true;
      vector<CPPGMAstNodePtr> constructor_arguments;
      constructor_arguments.push_back(source);
      if(EmitConstructorAt(target, destination, constructor_arguments, scope,
                           allow_explicit)) return true;
      string source_address;
      if(source_info.category == "lvalue" || source_info.category == "xvalue")
        source_address = EmitAddress(source, scope);
      else if(source->kind == "call-expression" &&
              ConstructorObjectType(source->children.empty() ?
                CPPGMAstNodePtr() : source->children[0], scope))
        source_address = EmitTemporaryObjectAddress(source, scope, "tmpobj");
      else
        source_address = EmitAddress(source, scope);
      source_address = AdjustBaseAddress(source_address, source_type, target);
      if(IsTrivialValueStorage(target)) {
        AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(target))) +
          "x" + integer_text(static_cast<long long>(type_alignment(target))) + " " +
          source_address + ", " + destination);
        return true;
      }
    }
    if(same_type && ValueOperationDeleted(target, move, false)) return false;
    if(same_type && IsTrivialValueStorage(target) &&
       !(move && ClassHasDeclaredMoveMember(target))) {
      FunctionRecord* copy_member = FindValueMember(target, false, false);
      if(copy_member && source->kind == "unary-expression") {
        copy_member->needed = true;
        FunctionRecord* base_entry = BaseEntryFor(copy_member);
        if(base_entry) base_entry->needed = true;
      }
      string source_operand;
      if(source_info.category == "lvalue" || source_info.category == "xvalue")
        source_operand = EmitAddress(source, scope);
      else source_operand = EmitValue(source, scope, target).operand;
      AddInstruction("copyobj " + integer_text(static_cast<long long>(type_size(target))) +
        "x" + integer_text(static_cast<long long>(type_alignment(target))) + " " +
        source_operand + ", " + destination);
      return true;
    }
    if(same_type) {
      FunctionRecord* value_member = EnsureImplicitCopyConstructor(target, move);
      if(value_member && value_member->deleted) return false;
    }
    vector<CPPGMAstNodePtr> arguments;
    arguments.push_back(source);
    if(!EmitConstructorAt(target, destination, arguments, scope, allow_explicit,
                          false, false, implicit_return_move)) {
      if(same_type && !move) {
        FunctionRecord* value_member = EnsureImplicitCopyConstructor(target, false);
        if(value_member && !value_member->deleted)
          return EmitConstructorAt(target, destination, arguments, scope, allow_explicit);
      }
      return false;
    }
    return true;
  }

void PA14Lowerer::EmitAggregateArrayAt(const string& base, const TypePtr& raw_type,
                                        const CPPGMAstNodePtr& expression, Scope* scope,
                                        const CPPGMAstNodePtr& refresh_node,
                                        long long refresh_offset,
                                        bool direct_elements)
{
    TypePtr type = type_value(raw_type);
    if(!type || type->kind != TYPE_ARRAY || !expression ||
       expression->kind != "braced-init-list") return;
    const size_t element_count = type->bound >= 0 ?
      static_cast<size_t>(type->bound) : expression->children.size();
    for(size_t i = 0; i < element_count; ++i) {
      const bool refresh_element = refresh_node && i > 0;
      string array_base = refresh_element ? EmitAddress(refresh_node, scope) : base;
      if(refresh_element && refresh_offset >= 0) {
        const string refreshed = new_temp();
        AddInstruction(refreshed + " = index i8 " + array_base + ", " +
          integer_text(refresh_offset));
        array_base = refreshed;
      }
      string element;
      if(direct_elements) {
        element = i == 0 ? array_base : new_temp();
        if(i > 0) AddInstruction(element + " = index i8 " + array_base + ", " +
          integer_text(static_cast<long long>(i * type_size(type->child))));
      } else {
        const string decay = new_temp();
        AddInstruction(decay + " = unary decay ptr " + array_base);
        element = new_temp();
        AddInstruction(element + " = index " + low_type(type->child) + " " +
          decay + ", " + integer_text(static_cast<long long>(i)));
      }
      const CPPGMAstNodePtr child = i < expression->children.size() ?
        expression->children[i] : CPPGMAstNodePtr();
      TypePtr child_type = type_value(type->child);
      if(!child) {
        if(child_type && child_type->kind == TYPE_CLASS) {
          vector<CPPGMAstNodePtr> no_arguments;
          if(EmitConstructorAt(child_type, element, no_arguments, scope)) continue;
          CPPGMAstNodePtr empty(new CPPGMAstNode("braced-init-list"));
          EmitAggregateAt(element, child_type, empty, scope);
        } else if(child_type && child_type->kind == TYPE_ARRAY) {
          CPPGMAstNodePtr empty(new CPPGMAstNode("braced-init-list"));
          EmitAggregateAt(element, child_type, empty, scope);
        } else if(child_type) emit_store(child_type, "0", element);
        continue;
      }
      if(child_type && child_type->kind == TYPE_CLASS) {
        vector<CPPGMAstNodePtr> arguments = child && child->kind == "braced-init-list" ?
          child->children : vector<CPPGMAstNodePtr>();
        if(arguments.empty() && child && child->kind != "braced-init-list")
          arguments.push_back(child);
        if(EmitConstructorAt(child_type, element, arguments, scope,
                             true, false, true)) continue;
        if(child && child->kind == "braced-init-list") {
          EmitAggregateAt(element, child_type, child, scope);
          continue;
        }
      }
      if(child_type && child_type->kind == TYPE_ARRAY &&
         child && child->kind == "braced-init-list") {
        EmitAggregateAt(element, child_type, child, scope);
        continue;
      }
      if(type_is_reference(type->child)) {
        emit_store(PointerTo(Fundamental("char")),
          EmitReferenceArgument(child, scope, type->child), element);
        continue;
      }
      Value value = EmitValue(child, scope, child_type);
      if(value.known_constant && is_integral_type(value.type) &&
         is_integral_type(child_type)) {
        value.type = child_type;
        value.operand = integer_text(value.constant);
      } else value = ConvertValue(value, child_type);
      emit_store(child_type, value.operand, element);
    }
  }

bool PA14Lowerer::EmitAggregateClassArrayField(const string& base, const TypePtr& child_type,
                                               const CPPGMAstNodePtr& child, Scope* scope,
                                               const CPPGMAstNodePtr& refresh_node,
                                               bool refresh_field_base, long long offset)
{
    if(!child_type || child_type->kind != TYPE_ARRAY || !child ||
       child->kind == "braced-init-list" || child_type->bound < 0) return false;
    const bool string_child = child->kind == "literal" && !child->value.empty() &&
      child->value[0] == '"';
    const vector<unsigned char> bytes = string_child ? decode_string_literal(child->value) :
      vector<unsigned char>();
    Value scalar_value;
    if(!string_child) scalar_value = EmitValue(child, scope, child_type->child);
    for(size_t element_index = 0;
        element_index < static_cast<size_t>(child_type->bound); ++element_index) {
      const string field_base = refresh_field_base ? EmitAddress(refresh_node, scope) : base;
      const string field = new_temp();
      AddInstruction(field + " = index i8 [projection=field] " + field_base + ", " +
        integer_text(offset));
      const string decay = new_temp();
      AddInstruction(decay + " = unary decay ptr " + field);
      const string element = new_temp();
      AddInstruction(element + " = index " + low_type(child_type->child) + " " +
        decay + ", " + integer_text(static_cast<long long>(element_index)));
      if(string_child) {
        const unsigned char byte = element_index < bytes.size() ? bytes[element_index] : 0;
        emit_store(child_type->child, integer_text(byte), element);
      } else if(element_index == 0) {
        if(scalar_value.known_constant && is_integral_type(scalar_value.type) &&
           is_integral_type(child_type->child)) {
          scalar_value.type = child_type->child;
          scalar_value.operand = integer_text(scalar_value.constant);
        } else scalar_value = ConvertValue(scalar_value, child_type->child);
        emit_store(child_type->child, scalar_value.operand, element);
      } else emit_store(child_type->child, "0", element);
    }
    return true;
  }

void PA14Lowerer::PrecomputeAggregateChildValues(const TypePtr& child_type,
                                                 const CPPGMAstNodePtr& child,
                                                 Scope* scope,
                                                 vector<const CPPGMAstNode*>* computed)
{
    if(!state_ || !child_type || child_type->kind != TYPE_CLASS || !child ||
       child->kind != "braced-init-list" || !computed) return;
    size_t nested_child = 0;
    for(size_t member = 0; member < child_type->class_members.size() &&
        nested_child < child->children.size(); ++member) {
      const ClassMemberInfo& nested_field = child_type->class_members[member];
      if(nested_field.is_static || !nested_field.type) continue;
      const CPPGMAstNodePtr nested_expression = child->children[nested_child++];
      if(!nested_expression || nested_expression->kind != "call-expression" ||
         state_->aggregate_precomputed_values.find(nested_expression.get()) !=
           state_->aggregate_precomputed_values.end()) continue;
      Value precomputed;
      precomputed.type = nested_field.type;
      if(type_is_reference(nested_field.type)) {
        precomputed.operand = EmitReferenceArgument(nested_expression, scope,
          nested_field.type);
        precomputed.lvalue = true;
      } else precomputed = EmitValue(nested_expression, scope);
      state_->aggregate_precomputed_values[nested_expression.get()] = precomputed;
      computed->push_back(nested_expression.get());
    }
  }

void PA14Lowerer::ClearAggregateChildValues(
  const vector<const CPPGMAstNode*>& computed)
{
    if(!state_) return;
    for(size_t i = 0; i < computed.size(); ++i)
      state_->aggregate_precomputed_values.erase(computed[i]);
  }

string PA14Lowerer::AggregateFieldBase(const string& base,
                                       bool refresh_field_base,
                                       const CPPGMAstNodePtr& refresh_node,
                                       Scope* scope, const string& refresh_base,
                                       long long refresh_offset,
                                       string* synthetic_refresh_base)
{
    if(!refresh_field_base) return base;
    if(refresh_node) return EmitAddress(refresh_node, scope);
    if(refresh_base.empty()) return base;
    if(synthetic_refresh_base && synthetic_refresh_base->empty()) {
      *synthetic_refresh_base = new_temp();
      AddInstruction(*synthetic_refresh_base +
        " = index i8 [projection=field] " + refresh_base + ", " +
        integer_text(refresh_offset));
    }
    return synthetic_refresh_base ? *synthetic_refresh_base : base;
  }

void PA14Lowerer::EmitAggregateClassBitField(
  const string& base, bool refresh_field_base,
  const CPPGMAstNodePtr& refresh_node, const string& refresh_base,
  long long refresh_offset, string* synthetic_refresh_base,
  const TypePtr& child_type,
  const CPPGMAstNodePtr& child, Scope* scope, Binding* member_binding,
  const ClassMemberInfo& member, bool* storage_initialized,
  const Value* precomputed_value)
{
    Value value = precomputed_value ? *precomputed_value :
      EmitValue(child, scope, child_type);
    if(value.known_constant && is_integral_type(value.type) &&
       is_integral_type(child_type)) {
      value.type = child_type;
      value.operand = integer_text(value.constant);
    } else value = ConvertValue(value, child_type);
    string stored;
    if(*storage_initialized) {
      const string field_base = AggregateFieldBase(base, refresh_field_base,
        refresh_node, scope, refresh_base, refresh_offset, synthetic_refresh_base);
      const string read_address = new_temp();
      AddInstruction(read_address + " = index i8 " + field_base + ", " +
        integer_text(member.offset));
      stored = MergeBitFieldValue(member_binding, read_address, member.type,
        value.operand, true);
    } else stored = PrepareBitFieldValue(member_binding, member.type, value.operand);
    const string field_base = AggregateFieldBase(base, refresh_field_base,
      refresh_node, scope, refresh_base, refresh_offset, synthetic_refresh_base);
    const string store_address = new_temp();
    AddInstruction(store_address + " = index i8 " + field_base + ", " +
      integer_text(member.offset));
    emit_store(member.type, stored, store_address);
    *storage_initialized = true;
  }

void PA14Lowerer::EmitAggregateClassReference(const string& base,
                                              bool refresh_field_base,
                                              const CPPGMAstNodePtr& refresh_node,
                                              const string& refresh_base,
                                              long long refresh_offset,
                                              string* synthetic_refresh_base,
                                              const TypePtr& member_type,
                                              const CPPGMAstNodePtr& child,
                                              Scope* scope,
                                              const ClassMemberInfo& member)
{
    string reference;
    map<const CPPGMAstNode*, Value>::const_iterator cached = state_ ?
      state_->aggregate_precomputed_values.find(child.get()) :
      map<const CPPGMAstNode*, Value>::const_iterator();
    if(state_ && cached != state_->aggregate_precomputed_values.end())
      reference = cached->second.operand;
    else reference = EmitReferenceArgument(child, scope, member_type);
    const string field_base = AggregateFieldBase(base, refresh_field_base,
      refresh_node, scope, refresh_base, refresh_offset, synthetic_refresh_base);
    const string field = new_temp();
    AddInstruction(field + " = index i8 [projection=field] " + field_base + ", " +
      integer_text(member.offset));
    emit_store(PointerTo(Fundamental("char")), reference, field);
  }

bool PA14Lowerer::EmitAggregateClassObject(
  const string& base, const TypePtr& child_type, const CPPGMAstNodePtr& child,
  const CPPGMAstNodePtr& expression, Scope* scope,
  const CPPGMAstNodePtr& refresh_node, const string& field,
  const string& member_name, size_t current_child_index, size_t* child_index,
  long long member_offset, const vector<const CPPGMAstNode*>& computed)
{
    vector<CPPGMAstNodePtr> arguments = child && child->kind == "braced-init-list" ?
      child->children : vector<CPPGMAstNodePtr>();
    if(arguments.empty() && child && child->kind != "braced-init-list")
      arguments.push_back(child);
    const bool has_cached_child = state_ &&
      state_->aggregate_precomputed_values.find(child.get()) !=
        state_->aggregate_precomputed_values.end();
    if(child && child->kind != "braced-init-list" && !has_cached_child) {
      const ExprInfo child_info = Infer(child, scope);
      const TypePtr child_value_type = expression_value_type(child_info);
      if(child_value_type && child_value_type->kind == TYPE_CLASS &&
         (PA12SameType(child_value_type, child_type, true) ||
          IsDerivedFrom(child_value_type, child_type)) &&
         EmitObjectTransferAt(child_type, field, child, scope, true)) return true;
    }
    if(EmitConstructorAt(child_type, field, arguments, scope)) return true;
    if(child && child->kind == "braced-init-list") {
      CPPGMAstNodePtr child_refresh;
      if(refresh_node) {
        child_refresh.reset(new CPPGMAstNode("member-expression", "OP_DOT:."));
        child_refresh->children.push_back(refresh_node);
        child_refresh->children.push_back(CPPGMAstNodePtr(
          new CPPGMAstNode("identifier", member_name)));
      }
      EmitAggregateAt(field, child_type, child, scope, child_refresh);
      ClearAggregateChildValues(computed);
      return true;
    }
    FunctionRecord* nested_aggregate = 0;
    const vector<Binding*> nested_bindings =
      MemberBindings(child_type, LastComponent(child_type->name));
    for(size_t i = 0; i < nested_bindings.size(); ++i) {
      FunctionRecord* nested_record = RecordForBinding(nested_bindings[i]);
      if(nested_record && nested_record->aggregate_constructor) {
        nested_aggregate = nested_record;
        break;
      }
    }
    if(!nested_aggregate) return false;
    const size_t nested_start = current_child_index;
    *child_index = nested_start;
    CPPGMAstNodePtr nested_refresh;
    if(refresh_node) {
      nested_refresh.reset(new CPPGMAstNode("member-expression", "OP_DOT:."));
      nested_refresh->children.push_back(refresh_node);
      nested_refresh->children.push_back(CPPGMAstNodePtr(
        new CPPGMAstNode("identifier", member_name)));
    }
    EmitAggregateClassFields(field, child_type, expression, scope, nested_refresh,
      child_index, false, nested_refresh ? string() : base,
      nested_refresh ? -1 : member_offset);
    const size_t nested_consumed = *child_index - nested_start;
    EmitAggregateClassDefaults(field, child_type, expression, scope,
      nested_refresh, nested_consumed);
    if(nested_consumed != 0) return true;
    *child_index = current_child_index + 1;
    return false;
  }

void PA14Lowerer::EmitAggregateClassFields(const string& base, const TypePtr& raw_type,
                                           const CPPGMAstNodePtr& expression, Scope* scope,
                                           const CPPGMAstNodePtr& refresh_node,
                                           size_t* child_index,
                                           bool direct_first_field,
                                           const string& refresh_base,
                                           long long refresh_offset)
{
    TypePtr type = type_value(raw_type);
    if(!type || type->kind != TYPE_CLASS || !expression ||
       expression->kind != "braced-init-list" || !child_index) return;
    bool bitfield_storage_initialized = false;
    string synthetic_refresh_base;
    const auto aggregate_refresh_base = [&]() {
      if(refresh_node) return EmitAddress(refresh_node, scope);
      if(refresh_base.empty()) return base;
      if(synthetic_refresh_base.empty()) {
        synthetic_refresh_base = new_temp();
        AddInstruction(synthetic_refresh_base + " = index i8 [projection=field] " +
          refresh_base + ", " + integer_text(refresh_offset));
      }
      return synthetic_refresh_base;
    };
    for(size_t i = 0; i < type->class_members.size() &&
        *child_index < expression->children.size(); ++i) {
      const ClassMemberInfo& member = type->class_members[i];
      if(member.is_static || !member.type) continue;
      const size_t current_child_index = *child_index;
      CPPGMAstNodePtr child = expression->children[(*child_index)++];
      const bool refresh_field_base = (refresh_node || !refresh_base.empty()) &&
        (current_child_index > 0 || base.empty());
      TypePtr child_type = type_value(member.type);
      if(member.name.empty()) {
        if(child_type && child_type->kind == TYPE_CLASS && child_type->is_union) {
          const string field_base = refresh_field_base ? aggregate_refresh_base() : base;
          const string field = new_temp();
          AddInstruction(field + " = index i8 [projection=field] " + field_base + ", " +
            integer_text(member.offset));
          EmitAggregateAt(field, child_type, child, scope,
                          CPPGMAstNodePtr(), -1, false);
        }
        continue;
      }
      vector<const CPPGMAstNode*> computed;
      PrecomputeAggregateChildValues(child_type, child, scope, &computed);
      Value precomputed_value;
      const bool precompute_address_of = child && child->kind == "unary-expression" &&
        PA12Operator(child->value) == "&" && child_type &&
        child_type->kind != TYPE_CLASS && child_type->kind != TYPE_ARRAY;
      if(precompute_address_of) precomputed_value = EmitValue(child, scope, child_type);
      const bool precompute_scalar = child && !precompute_address_of &&
        child->kind != "braced-init-list" && child_type &&
        child_type->kind != TYPE_CLASS && child_type->kind != TYPE_ARRAY &&
        !type_is_reference(member.type);
      if(precompute_scalar) precomputed_value = EmitValue(child, scope, child_type);
      const bool have_precomputed_value = precompute_address_of || precompute_scalar;
      Binding* member_binding = 0;
      const vector<Binding*> member_bindings = DirectBindings(type->owned_scope, member.name);
      for(size_t j = 0; j < member_bindings.size(); ++j)
        if(member_bindings[j]->kind == BIND_VARIABLE && member_bindings[j]->is_member &&
           !member_bindings[j]->is_static) { member_binding = member_bindings[j]; break; }
      if(member_binding && member_binding->injected_member && child &&
         child->kind == "braced-init-list" && child->children.size() == 1)
        child = child->children[0];
      if(member_binding && IsBitField(member_binding)) {
        EmitAggregateClassBitField(base, refresh_field_base, refresh_node, refresh_base,
          refresh_offset, &synthetic_refresh_base, child_type, child, scope,
          member_binding, member, &bitfield_storage_initialized,
          have_precomputed_value ? &precomputed_value : 0);
        continue;
      }
      if(type_is_reference(member.type)) {
        EmitAggregateClassReference(base, refresh_field_base, refresh_node, refresh_base,
          refresh_offset, &synthetic_refresh_base, member.type, child, scope, member);
        continue;
      }
      if(EmitAggregateClassArrayField(base, child_type, child, scope, refresh_node,
                                      refresh_field_base, member.offset)) continue;
      const string field_base = refresh_field_base ? aggregate_refresh_base() : base;
      string field;
      if(direct_first_field && current_child_index == 0 && member.offset == 0)
        field = field_base;
      else {
        field = new_temp();
        AddInstruction(field + " = index i8 [projection=field] " + field_base + ", " +
          integer_text(member.offset));
      }
      if(refresh_node && refresh_node->kind == "id-expression" &&
         child_type && child_type->kind == TYPE_CLASS &&
         !child_type->template_specialization && !HasDefaultConstructionEffects(child_type) &&
         !HasExplicitConstructor(child_type) && state_)
        state_->stable_member_addresses[refresh_node->value + "." + member.name] = field;
      if(child_type && child_type->kind == TYPE_CLASS &&
         EmitAggregateClassObject(base, child_type, child, expression, scope,
           refresh_node, field, member.name, current_child_index, child_index,
           member.offset, computed)) continue;
      if(child_type && child_type->kind == TYPE_ARRAY &&
         child && child->kind == "braced-init-list") {
        EmitAggregateAt(field, child_type, child, scope, refresh_node, member.offset);
        continue;
      }
      Value value;
      map<const CPPGMAstNode*, Value>::const_iterator cached_value = state_ ?
        state_->aggregate_precomputed_values.find(child.get()) :
        map<const CPPGMAstNode*, Value>::const_iterator();
      if(have_precomputed_value) value = precomputed_value;
      else if(state_ && cached_value != state_->aggregate_precomputed_values.end())
        value = cached_value->second;
      else value = EmitValue(child, scope, child_type);
      ClearAggregateChildValues(computed);
      if(value.known_constant && is_integral_type(value.type) &&
         is_integral_type(child_type)) {
        value.type = child_type;
        value.operand = integer_text(value.constant);
      } else value = ConvertValue(value, child_type);
      emit_store(child_type, value.operand, field);
    }
  }

void PA14Lowerer::EmitAggregateClassDefaults(const string& base, const TypePtr& raw_type,
                                             const CPPGMAstNodePtr& expression, Scope* scope,
                                             const CPPGMAstNodePtr& refresh_node,
                                             size_t child_index,
                                             bool direct_first_field)
{
    TypePtr type = type_value(raw_type);
    if(!type || type->kind != TYPE_CLASS || !expression ||
       expression->kind != "braced-init-list") return;
    size_t consumed = 0;
    for(size_t i = 0; i < type->class_members.size(); ++i) {
      const ClassMemberInfo& member = type->class_members[i];
      if(member.is_static || member.name.empty() || !member.type) continue;
      const size_t member_position = consumed++;
      // A union aggregate initializes at most its first active member.  The
      // other members share the same storage and must not be zeroed or
      // constructed after that initialization.
      if(type->is_union && member_position > 0) break;
      if(member_position < child_index) continue;
      const vector<Binding*> member_bindings =
        DirectBindings(type->owned_scope, member.name);
      bool injected_member = false;
      for(size_t j = 0; j < member_bindings.size(); ++j)
        if(member_bindings[j]->kind == BIND_VARIABLE &&
           member_bindings[j]->injected_member) { injected_member = true; break; }
      if(injected_member) continue;
      TypePtr member_type = type_value(member.type);
      if(!member.initializer && member_type && member_type->kind == TYPE_CLASS &&
         member_type->class_members.empty() && !member_type->direct_base &&
         MemberBindings(member_type, LastComponent(member_type->name)).empty()) continue;
      const string field_base = refresh_node ? EmitAddress(refresh_node, scope) : base;
      string field;
      if(direct_first_field && member_position == 0 && member.offset == 0)
        field = field_base;
      else {
        field = new_temp();
        AddInstruction(field + " = index i8 [projection=field] " + field_base + ", " +
          integer_text(member.offset));
      }
      CPPGMAstNodePtr member_expression = member.initializer ?
        InitializerExpression(member.initializer) : CPPGMAstNodePtr();
      vector<CPPGMAstNodePtr> arguments;
      if(member.initializer && !member.initializer->children.empty() &&
         member.initializer->children[0] &&
         member.initializer->children[0]->kind == "paren-initializer")
        arguments = member.initializer->children[0]->children;
      else if(member_expression && member_expression->kind == "braced-init-list")
        arguments = member_expression->children;
      else if(member_expression) arguments.push_back(member_expression);
      if(member_type && member_type->kind == TYPE_CLASS) {
        if(EmitConstructorAt(member_type, field, arguments, scope)) continue;
        if(member_expression && member_expression->kind == "braced-init-list") {
          EmitAggregateAt(field, member_type, member_expression, scope);
          continue;
        }
      }
      if(type_is_reference(member.type)) {
        if(member_expression) emit_store(PointerTo(Fundamental("char")),
          EmitReferenceArgument(member_expression, scope, member.type), field);
        continue;
      }
      if(member_expression) {
        Value value = EmitValue(member_expression, scope, member.type);
        value = ConvertValue(value, member.type);
        emit_store(member.type, value.operand, field);
      } else if(!member_type || member_type->kind != TYPE_CLASS) {
        emit_store(member.type,
          member_type && member_type->kind == TYPE_POINTER ? "nullptr" : "0", field);
      }
    }
  }

void PA14Lowerer::EmitAggregateAt(const string& base, const TypePtr& raw_type,
                                  const CPPGMAstNodePtr& expression, Scope* scope,
                                  const CPPGMAstNodePtr& refresh_node,
                                  long long refresh_offset,
                                  bool direct_first_field)
{
    if(!expression) return;
    TypePtr type = type_value(raw_type);
    if(!type) return;
    if(expression->kind == "parenthesized-expression" && !expression->children.empty()) {
      EmitAggregateAt(base, type, expression->children[0], scope,
        refresh_node, refresh_offset, direct_first_field);
      return;
    }
    if(type->kind == TYPE_ARRAY && expression->kind == "braced-init-list") {
      EmitAggregateArrayAt(base, type, expression, scope, refresh_node, refresh_offset);
      return;
    }
    if(type->kind != TYPE_CLASS || expression->kind != "braced-init-list") return;
    size_t child_index = 0;
    EmitAggregateClassFields(base, type, expression, scope, refresh_node, &child_index,
      direct_first_field);
    EmitAggregateClassDefaults(base, type, expression, scope, refresh_node, child_index,
      direct_first_field);
  }

void PA14Lowerer::EmitAggregateConstructorBody(FunctionRecord& function, Scope* scope)
{
    TypePtr owner = type_value(function.member_owner);
    if(!owner) return;
    const vector<string> names = ParameterNames(function);
    CPPGMAstNodePtr this_node(new CPPGMAstNode("keyword-literal", "KW_THIS:this"));
    size_t parameter = 1;
    for(size_t i = 0; i < owner->class_members.size(); ++i) {
      const ClassMemberInfo& member = owner->class_members[i];
      if(member.is_static || member.name.empty() || !member.type) continue;
      if(parameter >= names.size()) break;
      const bool by_address = LowParameterIsByAddress(function, parameter) &&
        type_value(member.type) && type_value(member.type)->kind == TYPE_CLASS;
      string value;
      if(!by_address) value = emit_load("$" + names[parameter], member.type);
      const string this_address = EmitValue(this_node, scope).operand;
      const string address = new_temp();
      AddInstruction(address + " = index i8 " + this_address + ", " +
        integer_text(member.offset));
      if(by_address) {
        FunctionRecord* value_member = EnsureImplicitCopyConstructor(member.type, true);
        if(!value_member || value_member->deleted)
          value_member = EnsureImplicitCopyConstructor(member.type, false);
        if(value_member && !value_member->deleted) {
          value_member->needed = true;
          FunctionRecord* call_member = value_member;
          const bool need_base_entry = !BaseEntryFor(value_member) &&
            !value_member->template_instantiation && !function.template_instantiation &&
            !function.synthesized_value_member && !function.aggregate_constructor &&
            (!function.member_owner || !function.member_owner->template_specialization);
          if(need_base_entry)
            EnsureConstructorBaseEntry(value_member);
          FunctionRecord* base_member = BaseEntryFor(value_member);
          if(base_member) {
            base_member->needed = true;
            call_member = base_member;
          }
          AddInstruction("call void @" + call_member->symbol + "(" + address + ", %" +
            names[parameter] + ")");
          ++parameter;
          continue;
        }
      }
      Binding* binding = 0;
      vector<Binding*> candidates = DirectBindings(owner->owned_scope, member.name);
      for(size_t j = 0; j < candidates.size(); ++j)
        if(candidates[j]->kind == BIND_VARIABLE && candidates[j]->is_member &&
           !candidates[j]->is_static) { binding = candidates[j]; break; }
      if(binding && IsBitField(binding)) StoreBitField(binding, address, member.type, value, true);
      else emit_store(member.type, value, address);
      ++parameter;
    }
  }

void PA14Lowerer::EmitDestructorBody(FunctionRecord& function, Scope* scope)
{
    TypePtr owner = type_value(function.member_owner);
    if(!owner) return;
    CPPGMAstNodePtr this_node(new CPPGMAstNode("keyword-literal", "KW_THIS:this"));
    if(owner->polymorphic)
      EmitVPointerStore(owner, EmitValue(this_node, scope).operand);
    for(size_t i = owner->class_members.size(); i > 0; --i) {
      const ClassMemberInfo& member = owner->class_members[i - 1];
      TypePtr member_type = type_value(member.type);
      if(member.is_static || member.name.empty() || !member_type) continue;
      CPPGMAstNodePtr expression(new CPPGMAstNode("member-expression", "OP_ARROW:->"));
      expression->children.push_back(this_node);
      expression->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", member.name)));
      if(member_type->kind == TYPE_ARRAY) {
        TypePtr element_type = type_value(member_type->child);
        if(!element_type || element_type->kind != TYPE_CLASS || member_type->bound < 0) continue;
        for(size_t element_index = static_cast<size_t>(member_type->bound);
            element_index > 0; --element_index) {
          const string array_address = EmitMemberAddress(expression, scope);
          const string decay = new_temp();
          AddInstruction(decay + " = unary decay ptr " + array_address);
          const string offset = new_temp();
          AddInstruction(offset + " = binary mul i64 " +
            integer_text(static_cast<long long>(element_index - 1)) + ", " +
            integer_text(static_cast<long long>(type_size(element_type))));
          const string element = new_temp();
          AddInstruction(element + " = index i8 " + decay + ", " + offset);
          (void)EmitDestructorAt(element_type, element, scope);
        }
      } else if(member_type->kind == TYPE_CLASS) {
        if(!HasDestructor(member_type)) continue;
        const string address = EmitMemberAddress(expression, scope);
        (void)EmitDestructorAt(member_type, address, scope);
      }
    }
    TypePtr base = type_value(owner->direct_base);
    if(base) {
      const string this_address = EmitValue(this_node, scope).operand;
      const string base_address = AdjustBaseAddress(this_address, owner, base);
      // A virtual base destructor still has to run when its body is empty;
      // the derived destructor owns the base-subobject transition.
      (void)EmitDestructorAt(base, base_address, scope, true);
    }
    if(function.deleting_entry) {
      TypePtr parameter = PointerTo(Fundamental("void"));
      FunctionRecord* deallocator = FindFunction("operatordelete",
        FunctionOf(vector<TypePtr>(1, parameter), false, Fundamental("void"), false));
      if(deallocator) {
        deallocator->needed = true;
        const string address = EmitValue(this_node, scope).operand;
        AddInstruction("call void @" + deallocator->symbol + "(" + address + ")");
      }
    }
  }

void PA14Lowerer::EmitLiveDestructors(Scope* scope)
{
    for(size_t i = state_->variables.size(); i > 0; --i) {
      VariablePlan& variable = state_->variables[i - 1];
      if(state_->return_slot_plan == &variable) continue;
      bool live = false;
      for(size_t environment_index = 0;
          environment_index < state_->environments.size() && !live;
          ++environment_index) {
        const map<string, VariablePlan*>& environment =
          state_->environments[environment_index];
        for(map<string, VariablePlan*>::const_iterator it = environment.begin();
            it != environment.end(); ++it)
          if(it->second == &variable) { live = true; break; }
      }
      if(!live) continue;
      // A reference parameter is an alias, not an object with storage owned
      // by the current function.  Its referred class must not be destroyed
      // when the reference leaves scope.
      if(type_is_reference(variable.type)) continue;
      TypePtr object_type = type_value(variable.type);
      if(!object_type) continue;
      if(object_type->kind == TYPE_CLASS) {
        if(variable.parameter ? !HasDestructor(object_type) :
           (!HasDestructor(object_type) || !DestructorHasEffects(object_type))) continue;
        (void)EmitDestructorAt(object_type, local_address(&variable), scope,
          variable.parameter);
        continue;
      }
      TypePtr element_type = object_type->child ? type_value(object_type->child) : TypePtr();
      if(object_type->kind != TYPE_ARRAY || object_type->bound < 0 ||
         !element_type || element_type->kind != TYPE_CLASS ||
         (variable.parameter ? !HasDestructor(element_type) :
          (!HasDestructor(element_type) || !DestructorHasEffects(element_type)))) continue;
      for(size_t element_index = 0;
          element_index < static_cast<size_t>(object_type->bound); ++element_index) {
        const string base = local_address(&variable);
        const string decay = new_temp();
        AddInstruction(decay + " = unary decay ptr " + base);
        const string element = new_temp();
        AddInstruction(element + " = index i8 " + decay + ", " +
          integer_text(static_cast<long long>(element_index)));
        (void)EmitDestructorAt(element_type, element, scope, variable.parameter);
      }
    }
  }

void PA14Lowerer::RegisterTemporaryObject(const TypePtr& type, const string& address)
{
    if(!state_ || !type || address.empty()) return;
    if(!HasDestructor(type)) return;
    state_->temporary_objects.push_back(FunctionState::TemporaryObject(type, address));
  }

void PA14Lowerer::EmitTemporaryDestructors(size_t mark, Scope* scope)
{
    if(!state_) return;
    for(size_t i = state_->temporary_objects.size(); i > mark; --i) {
      const FunctionState::TemporaryObject& temporary = state_->temporary_objects[i - 1];
      (void)EmitDestructorAt(temporary.type, temporary.address, scope);
    }
    state_->temporary_objects.resize(mark);
  }

} // namespace cppgm_pa14_lowering
