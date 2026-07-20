#include "pa14_lowering.h"

#include <map>
#include <string>
#include <vector>

using namespace std;

namespace cppgm_pa14_lowering {

bool PA14Lowerer::ClassHasDeclaredValueMember(const TypePtr& raw_type) const
{
    TypePtr type = type_value(raw_type);
    if(!type || type->kind != TYPE_CLASS || !type->owned_scope) return false;
    for(size_t i = 0; i < type->owned_scope->bindings.size(); ++i) {
      const Binding& binding = type->owned_scope->bindings[i];
      if(binding.kind != BIND_FUNCTION) continue;
      FunctionRecord* record = RecordForBinding(const_cast<Binding*>(&binding));
      if(record && record->value_special_member && !record->synthesized_value_member)
        return true;
    }
    return false;
  }

bool PA14Lowerer::ClassHasDeclaredMoveMember(const TypePtr& raw_type) const
{
    TypePtr type = type_value(raw_type);
    if(!type || type->kind != TYPE_CLASS || !type->owned_scope) return false;
    const string name = LastComponent(type->name);
    const vector<Binding*> candidates = DirectBindings(type->owned_scope, name);
    for(size_t i = 0; i < candidates.size(); ++i) {
      TypePtr function = function_target_type(candidates[i]->type);
      if(!function || function->parameters.size() != 1 ||
         function->parameters[0]->kind != TYPE_RVALUE_REFERENCE) continue;
      FunctionRecord* record = RecordForBinding(candidates[i]);
      if(record && record->constructor && record->move_constructor &&
         !record->synthesized_value_member) return true;
    }
    return false;
  }

PA14Lowerer::FunctionRecord* PA14Lowerer::FindValueMember(
  const TypePtr& raw_type, bool move, bool assignment) const
{
    TypePtr owner = type_value(raw_type);
    if(!owner || owner->kind != TYPE_CLASS || !owner->owned_scope) return 0;
    const string name = assignment ? "operator=" : LastComponent(owner->name);
    const vector<Binding*> candidates = DirectBindings(owner->owned_scope, name);
    for(size_t i = 0; i < candidates.size(); ++i) {
      Binding* binding = candidates[i];
      if(!binding || binding->kind != BIND_FUNCTION) continue;
      TypePtr function = function_target_type(binding->type);
      if(!function || function->parameters.empty() ||
         !type_is_reference(function->parameters[0]) ||
         !PA12SameType(type_value(function->parameters[0]), owner, true)) continue;
      const bool candidate_move = function->parameters[0]->kind == TYPE_RVALUE_REFERENCE;
      if(candidate_move == move) return RecordForBinding(binding);
    }
    return 0;
  }

bool PA14Lowerer::ValueOperationDeleted(const TypePtr& raw_type, bool move,
                                        bool assignment,
                                        FunctionRecord* ignored) const
{
    TypePtr type = type_value(raw_type);
    if(!type) return false;
    if(type->kind == TYPE_ARRAY) return ValueOperationDeleted(type->child, move,
      assignment, ignored);
    if(type->kind != TYPE_CLASS) return assignment && type->is_const;
    FunctionRecord* candidate = FindValueMember(type, move, assignment);
    if(candidate && candidate != ignored) {
      if(candidate->deleted) return true;
      if(candidate->defaulted || candidate->synthesized_value_member)
        return ValueOperationDeleted(type, move, assignment, candidate);
      return false;
    }
    if(!candidate) {
      if(move) {
        FunctionRecord* fallback = FindValueMember(type, false, assignment);
        if(fallback && fallback != ignored) {
          if(fallback->deleted) return true;
          if(fallback->defaulted || fallback->synthesized_value_member)
            return ValueOperationDeleted(type, false, assignment, fallback);
          return false;
        }
      }
      bool declared_move = false;
      const string name = assignment ? "operator=" : LastComponent(type->name);
      const vector<Binding*> candidates = DirectBindings(type->owned_scope, name);
      for(size_t i = 0; i < candidates.size(); ++i) {
        FunctionRecord* record = RecordForBinding(candidates[i]);
        if(record && !record->synthesized_value_member &&
           ((assignment && record->move_assignment) ||
            (!assignment && record->move_constructor))) {
          declared_move = true;
          break;
        }
      }
      if(!move && declared_move) return true;
    }
    if(assignment && type->direct_base &&
       ValueOperationDeleted(type->direct_base, move, true)) return true;
    for(size_t i = 0; i < type->class_members.size(); ++i) {
      const ClassMemberInfo& member = type->class_members[i];
      if(member.is_static || !member.type) continue;
      if(assignment && (type_is_reference(member.type) || member.type->is_const)) return true;
      if(ValueOperationDeleted(member.type, move, assignment)) return true;
    }
    return false;
  }

void PA14Lowerer::MarkValueMemberDeleted(FunctionRecord* record)
{
    if(!record || !record->value_special_member ||
       (!record->defaulted && !record->synthesized_value_member)) return;
    const bool assignment = record->copy_assignment || record->move_assignment;
    const bool move = record->move_constructor || record->move_assignment;
    record->deleted = record->deleted ||
      ValueOperationDeleted(record->member_owner, move, assignment, record);
  }

bool PA14Lowerer::IsTrivialValueStorage(const TypePtr& raw_type) const
{
    TypePtr type = type_value(raw_type);
    if(!type) return false;
    if(type->kind == TYPE_ARRAY) return IsTrivialValueStorage(type->child);
    if(type->kind != TYPE_CLASS) return type->kind != TYPE_FUNCTION &&
      type->kind != TYPE_MEMBER_POINTER;
    if(type->direct_base && !IsTrivialValueStorage(type->direct_base)) return false;
    for(size_t i = 0; i < type->class_members.size(); ++i) {
      const ClassMemberInfo& member = type->class_members[i];
      if(member.is_static || !member.type) continue;
      if(!IsTrivialValueStorage(member.type)) return false;
    }
    for(size_t i = 0; i < type->owned_scope->bindings.size(); ++i) {
      const Binding& binding = type->owned_scope->bindings[i];
      if(binding.kind != BIND_FUNCTION) continue;
      FunctionRecord* record = RecordForBinding(const_cast<Binding*>(&binding));
      if(!record || !record->value_special_member) continue;
      if(record->deleted) continue;
      if(!record->defaulted && !record->implicit_constructor) return false;
    }
    return true;
  }

bool PA14Lowerer::ClassValueNeedsIndirect(const TypePtr& raw_type) const
{
    TypePtr type = type_value(raw_type);
    if(!type || type->kind != TYPE_CLASS) return false;
    if(type_size(type) > 16) return true;
    if(!IsTrivialValueStorage(type)) return true;
    if(type->direct_base && ClassValueNeedsIndirect(type->direct_base)) return true;
    for(size_t i = 0; i < type->class_members.size(); ++i) {
      const ClassMemberInfo& member = type->class_members[i];
      if(member.is_static || !member.type) continue;
      TypePtr member_type = type_value(member.type);
      if(member_type && member_type->kind == TYPE_CLASS &&
         ClassValueNeedsIndirect(member_type)) return true;
      if(member_type && member_type->kind == TYPE_ARRAY && member_type->child &&
         ClassValueNeedsIndirect(member_type->child)) return true;
    }
    return false;
  }

TypePtr PA14Lowerer::SourceReturnType(const FunctionRecord& function) const
{
    return function.source_type ? function.source_type->child :
      (function.type ? function.type->child : TypePtr());
  }

bool PA14Lowerer::LowParameterIsByAddress(const FunctionRecord& function,
                                          size_t index) const
{
    return index < function.indirect_parameters.size() &&
      function.indirect_parameters[index];
  }

TypePtr PA14Lowerer::LowParameterSourceType(const FunctionRecord& function,
                                            size_t index) const
{
    size_t low_index = index;
    if(function.indirect_result) {
      if(low_index == 0) return TypePtr();
      --low_index;
    }
    if(function.member && !function.static_member) {
      if(low_index == 0) return function.member_owner;
      --low_index;
    }
    if(!function.source_type || low_index >= function.source_type->parameters.size())
      return function.type && index < function.type->parameters.size() ?
        function.type->parameters[index] : TypePtr();
    return function.source_type->parameters[low_index];
  }

void PA14Lowerer::BuildFunctionABI(FunctionRecord& function)
{
    if(function.builtin || !function.source_type) return;
    TypePtr source = function.source_type;
    if(!source || source->kind != TYPE_FUNCTION) return;
    const TypePtr result = source->child;
    function.indirect_result = result && !type_is_reference(result) &&
      type_value(result) && type_value(result)->kind == TYPE_CLASS &&
      ClassValueNeedsIndirect(result);
    vector<TypePtr> parameters;
    vector<bool> indirect;
    if(function.indirect_result) {
      parameters.push_back(PointerTo(type_value(result)));
      indirect.push_back(false);
    }
    if(function.member && !function.static_member) {
      TypePtr this_parameter = function.type && !function.type->parameters.empty() ?
        function.type->parameters[0] : PointerTo(function.member_owner);
      parameters.push_back(this_parameter);
      indirect.push_back(false);
    }
    for(size_t i = 0; i < source->parameters.size(); ++i) {
      TypePtr parameter = source->parameters[i];
      const bool by_address = parameter && !type_is_reference(parameter) &&
        type_value(parameter) && type_value(parameter)->kind == TYPE_CLASS &&
        ClassValueNeedsIndirect(parameter);
      parameters.push_back(by_address ? PointerTo(type_value(parameter)) : parameter);
      indirect.push_back(by_address);
    }
    function.indirect_parameters = indirect;
    function.type = FunctionOf(parameters, source->variadic,
      function.indirect_result ? Fundamental("void") : result, false);
  }

namespace {

CPPGMAstNodePtr SyntheticValueParameter(const string& name, bool move)
{
  CPPGMAstNodePtr parameter(new CPPGMAstNode("parameter-declaration"));
  parameter->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("type-specifier")));
  CPPGMAstNodePtr declarator(new CPPGMAstNode("declarator"));
  declarator->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
    "ptr-operator", move ? "OP_LAND:&&" : "OP_AMP:&")));
  declarator->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", name)));
  parameter->children.push_back(declarator);
  return parameter;
}

CPPGMAstNodePtr SyntheticValueMember(const string& name, const string& parameter,
                                     bool move, bool assignment)
{
  CPPGMAstNodePtr special(new CPPGMAstNode("special-member-definition", name));
  CPPGMAstNodePtr declarator(new CPPGMAstNode("declarator"));
  declarator->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", name)));
  CPPGMAstNodePtr clause(new CPPGMAstNode("parameter-clause"));
  clause->children.push_back(SyntheticValueParameter(parameter, move));
  declarator->children.push_back(clause);
  special->children.push_back(declarator);
  special->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("compound-statement")));
  (void)assignment;
  return special;
}

} // namespace

PA14Lowerer::FunctionRecord* PA14Lowerer::EnsureImplicitCopyConstructor(
  const TypePtr& raw_type, bool move)
{
    TypePtr owner = type_value(raw_type);
    if(!owner || owner->kind != TYPE_CLASS || !owner->owned_scope) return 0;
    const string name = LastComponent(owner->name);
    const vector<Binding*> candidates = DirectBindings(owner->owned_scope, name);
    const TypePtr parameter = move ? ReferenceTo(TYPE_RVALUE_REFERENCE, owner) :
      ReferenceTo(TYPE_LVALUE_REFERENCE, CloneWithCv(owner, true, false));
    const bool want_move = move;
    for(size_t i = 0; i < candidates.size(); ++i) {
      TypePtr function = function_target_type(candidates[i]->type);
      FunctionRecord* candidate_record = RecordForBinding(candidates[i]);
      if(!function || function->parameters.empty() ||
         !type_is_reference(function->parameters[0]) ||
         !PA12SameType(type_value(function->parameters[0]), owner, true)) continue;
      if(function->parameters.size() > 1) {
        if(!candidate_record) continue;
        bool defaults = true;
        for(size_t p = 1; p < function->parameters.size(); ++p)
          if(p >= candidate_record->default_arguments.size() ||
             !candidate_record->default_arguments[p]) { defaults = false; break; }
        if(!defaults) continue;
      }
      const bool candidate_move = function->parameters[0]->kind == TYPE_RVALUE_REFERENCE;
      if(candidate_move != want_move) continue;
      return candidate_record;
    }
    if(move && ClassHasDeclaredValueMember(owner)) return 0;
    const string key = function_key(owner->name + "::" + name,
      FunctionOf(vector<TypePtr>(1, parameter), false, Fundamental("void"), false));
    map<string, FunctionRecord*>::const_iterator found = function_by_key_.find(key);
    if(found != function_by_key_.end()) return found->second;
    const string parameter_name = "other";
    CPPGMAstNodePtr special = SyntheticValueMember(name, parameter_name, move, false);
    Binding binding(BIND_FUNCTION, name, FunctionOf(vector<TypePtr>(1, parameter), false,
      Fundamental("void"), false));
    binding.qualified_name = owner->name + "::" + name;
    binding.is_member = true;
    binding.is_static = false;
    binding.member_owner = owner;
    binding.access = "public";
    binding.declaration = special;
    Binding* stored = owner->owned_scope->add(binding);
    (void)stored;
    functions_.push_back(FunctionRecord());
    FunctionRecord* record = &functions_.back();
    function_by_key_[key] = record;
    record->node = special;
    record->scope = owner->owned_scope;
    record->source_type = binding.type;
    record->member_owner = owner;
    record->qualified_name = binding.qualified_name;
    record->member = true;
    record->static_member = false;
    record->constructor = true;
    record->implicit_constructor = true;
    record->copy_constructor = !move;
    record->move_constructor = move;
    record->value_special_member = true;
    record->synthesized_value_member = true;
    record->defaulted = true;
    record->definition = true;
    record->unwind_no = IsTrivialValueStorage(owner);
    vector<TypePtr> low_parameters;
    low_parameters.push_back(PointerTo(owner));
    low_parameters.push_back(parameter);
    record->type = FunctionOf(low_parameters, false, Fundamental("void"), false);
    BuildFunctionABI(*record);
    MarkValueMemberDeleted(record);
    record->symbol = low_symbol_component(record->qualified_name);
    unsigned int suffix = 2;
    for(;; ++suffix) {
      bool collision = false;
      for(size_t i = 0; i + 1 < functions_.size(); ++i)
        if(functions_[i].symbol == record->symbol) { collision = true; break; }
      if(!collision) break;
      record->symbol = low_symbol_component(record->qualified_name) + "__ov" +
        integer_text(static_cast<long long>(suffix));
    }
    return record;
  }

PA14Lowerer::FunctionRecord* PA14Lowerer::EnsureImplicitAssignment(
  const TypePtr& raw_type, bool move)
{
    TypePtr owner = type_value(raw_type);
    if(!owner || owner->kind != TYPE_CLASS || !owner->owned_scope) return 0;
    const string name = "operator=";
    const vector<Binding*> candidates = DirectBindings(owner->owned_scope, name);
    FunctionRecord* copy_fallback = 0;
    for(size_t i = 0; i < candidates.size(); ++i) {
      if(candidates[i]->kind != BIND_FUNCTION) continue;
      TypePtr function = function_target_type(candidates[i]->type);
      if(!function || function->parameters.empty() ||
         !type_is_reference(function->parameters[0]) ||
         !PA12SameType(type_value(function->parameters[0]), owner, true)) continue;
      FunctionRecord* candidate = RecordForBinding(candidates[i]);
      if(!candidate) continue;
      if(function->parameters[0]->kind == TYPE_RVALUE_REFERENCE) {
        if(move) return candidate;
      } else if(!copy_fallback) copy_fallback = candidate;
    }
    if(move && ClassHasDeclaredValueMember(owner)) return copy_fallback;
    const TypePtr parameter = move ? ReferenceTo(TYPE_RVALUE_REFERENCE, owner) :
      ReferenceTo(TYPE_LVALUE_REFERENCE, CloneWithCv(owner, true, false));
    const TypePtr result = ReferenceTo(TYPE_LVALUE_REFERENCE, owner);
    const TypePtr source = FunctionOf(vector<TypePtr>(1, parameter), false, result, false);
    const string qname = owner->name + "::" + name;
    const string key = function_key(qname, source);
    map<string, FunctionRecord*>::const_iterator found = function_by_key_.find(key);
    if(found != function_by_key_.end()) return found->second;
    CPPGMAstNodePtr special = SyntheticValueMember(name, "other", move, true);
    Binding binding(BIND_FUNCTION, name, source);
    binding.qualified_name = qname;
    binding.is_member = true;
    binding.is_static = false;
    binding.member_owner = owner;
    binding.access = "public";
    binding.declaration = special;
    owner->owned_scope->add(binding);
    functions_.push_back(FunctionRecord());
    FunctionRecord* record = &functions_.back();
    function_by_key_[key] = record;
    record->node = special;
    record->scope = owner->owned_scope;
    record->source_type = source;
    record->member_owner = owner;
    record->qualified_name = qname;
    record->member = true;
    record->static_member = false;
    record->copy_assignment = !move;
    record->move_assignment = move;
    record->value_special_member = true;
    record->synthesized_value_member = true;
    record->defaulted = true;
    record->definition = true;
    record->unwind_no = IsTrivialValueStorage(owner);
    vector<TypePtr> low_parameters;
    low_parameters.push_back(PointerTo(owner));
    low_parameters.push_back(parameter);
    record->type = FunctionOf(low_parameters, false, result, false);
    BuildFunctionABI(*record);
    MarkValueMemberDeleted(record);
    record->symbol = low_symbol_component(record->qualified_name);
    unsigned int suffix = 2;
    for(;; ++suffix) {
      bool collision = false;
      for(size_t i = 0; i + 1 < functions_.size(); ++i)
        if(functions_[i].symbol == record->symbol) { collision = true; break; }
      if(!collision) break;
      record->symbol = low_symbol_component(record->qualified_name) + "__ov" +
        integer_text(static_cast<long long>(suffix));
    }
    return record;
  }

int PA14Lowerer::ConversionRankToClass(const ExprInfo& source,
                                       const TypePtr& target) const
{
    const TypePtr target_value = type_value(target);
    if(!target_value || target_value->kind != TYPE_CLASS) return -1;
    const vector<Binding*> constructors =
      MemberBindings(target_value, last_component(target_value->name));
    for(size_t i = 0; i < constructors.size(); ++i) {
      Binding* binding = constructors[i];
      if(!binding || binding->kind != BIND_FUNCTION || !binding->is_member ||
         binding->is_static) continue;
      FunctionRecord* record = RecordForBinding(binding);
      if(!record || !record->constructor || record->deleted ||
         record->explicit_constructor) continue;
      TypePtr function = function_target_type(binding->type);
      if(!function || function->parameters.empty()) continue;
      if(type_is_reference(function->parameters[0]) &&
         PA12SameType(type_value(function->parameters[0]), target_value, true)) continue;
      const int first_rank = ConversionRank(source, function->parameters[0]);
      if(first_rank < 0) continue;
      bool defaults = true;
      for(size_t p = 1; p < function->parameters.size(); ++p)
        if(!HasDefaultArgument(binding, p)) { defaults = false; break; }
      if(defaults) return 3 + first_rank;
    }
    return -1;
  }

int PA14Lowerer::ConversionRank(const ExprInfo& source, const TypePtr& target) const
{
    if(!target || !source.type) return -1;
    TypePtr source_value = type_value(source.type);
    TypePtr target_value = type_value(target);
    if(!source_value || !target_value) return -1;
    if(target->kind == TYPE_LVALUE_REFERENCE || target->kind == TYPE_RVALUE_REFERENCE) {
      if(target->kind == TYPE_LVALUE_REFERENCE) {
        if(source.category == "lvalue") {
          if(!target_value->is_const && source_value->is_const) return -1;
          if(PA12SameType(source_value, target_value, true)) return 0;
          if(IsDerivedFrom(source_value, target_value))
            return BaseDistance(source_value, target_value);
          if(is_arithmetic_type(source_value) && is_arithmetic_type(target_value) &&
             target_value->is_const) return 2;
        }
        if(target_value->is_const &&
           PA12SameType(source_value, target_value, true)) return 1;
        if(target_value->is_const && source_value->kind == TYPE_CLASS &&
           target_value->kind == TYPE_CLASS &&
           IsDerivedFrom(source_value, target_value))
          return BaseDistance(source_value, target_value);
        if(target_value->kind == TYPE_CLASS && target_value->is_const) {
          const vector<Binding*> constructors =
            MemberBindings(target_value, last_component(target_value->name));
          for(size_t i = 0; i < constructors.size(); ++i) {
            Binding* binding = constructors[i];
            if(!binding || binding->kind != BIND_FUNCTION ||
               !binding->is_member || binding->is_static) continue;
            FunctionRecord* record = RecordForBinding(binding);
            if(!record || !record->constructor || record->deleted ||
               record->explicit_constructor) continue;
            TypePtr function = function_target_type(binding->type);
            if(!function || function->parameters.empty()) continue;
            if(type_is_reference(function->parameters[0]) &&
               PA12SameType(type_value(function->parameters[0]), target_value, true)) continue;
            const int first_rank = ConversionRank(source, function->parameters[0]);
            if(first_rank < 0) continue;
            bool defaults = true;
            for(size_t p = 1; p < function->parameters.size(); ++p)
              if(!HasDefaultArgument(binding, p)) { defaults = false; break; }
            if(defaults) return 3 + first_rank;
          }
        }
        const bool derived_pointer = source_value->kind == TYPE_POINTER &&
          target_value->kind == TYPE_POINTER &&
          IsDerivedFrom(source_value->child, target_value->child);
        const bool derived_object = source_value->kind == TYPE_CLASS &&
          target_value->kind == TYPE_CLASS &&
          IsDerivedFrom(source_value, target_value);
        if(target_value->is_const &&
           (PA12SameType(source_value, target_value, true) ||
            (is_arithmetic_type(source_value) && is_arithmetic_type(target_value)) ||
            derived_pointer || derived_object)) {
          return derived_object ? BaseDistance(source_value, target_value) : 2;
        }
        return -1;
      }
      if(source.category == "lvalue") {
        if(target_value->kind == TYPE_POINTER && source_value->kind == TYPE_ARRAY &&
           source_value->child && target_value->child &&
           PA12SameType(source_value->child, target_value->child, true)) return 1;
        return -1;
      }
      if(PA12SameType(source_value, target_value, true)) return 0;
      if(target_value->kind == TYPE_CLASS) {
        const vector<Binding*> constructors =
          MemberBindings(target_value, last_component(target_value->name));
        for(size_t i = 0; i < constructors.size(); ++i) {
          Binding* binding = constructors[i];
          if(!binding || binding->kind != BIND_FUNCTION ||
             !binding->is_member || binding->is_static) continue;
          FunctionRecord* record = RecordForBinding(binding);
          if(!record || !record->constructor || record->deleted ||
             record->explicit_constructor) continue;
          TypePtr function = function_target_type(binding->type);
          if(!function || function->parameters.empty()) continue;
          if(type_is_reference(function->parameters[0]) &&
             PA12SameType(type_value(function->parameters[0]), target_value, true)) continue;
          const int first_rank = ConversionRank(source, function->parameters[0]);
          if(first_rank < 0) continue;
          bool defaults = true;
          for(size_t p = 1; p < function->parameters.size(); ++p)
            if(!HasDefaultArgument(binding, p)) { defaults = false; break; }
          if(defaults) return 2 + first_rank;
        }
      }
      return is_arithmetic_type(source_value) && is_arithmetic_type(target_value) ? 1 : -1;
    }
    if(target_value->kind == TYPE_POINTER) {
      if(source.null_pointer_constant ||
         (source_value->kind == TYPE_FUNDAMENTAL && source_value->name == "nullptr_t")) return 2;
      if(source_value->kind == TYPE_ARRAY &&
         PA12SameType(source_value->child, target_value->child, true)) return 0;
      if(source_value->kind == TYPE_FUNCTION && target_value->child &&
         target_value->child->kind == TYPE_FUNCTION &&
         PA12SameType(source_value, target_value->child, true)) return 0;
      if(source_value->kind == TYPE_POINTER) {
        if(PA12SameType(source_value, target_value, false)) return 0;
        if(source_value->child && target_value->child &&
           source_value->child->kind == TYPE_POINTER &&
           target_value->child->kind == TYPE_POINTER &&
           !PA12SameType(source_value->child, target_value->child, false) &&
           PA12SameType(source_value->child, target_value->child, true)) return -1;
        if(PA12SameType(source_value, target_value, true)) return 1;
        if(source_value->child && target_value->child &&
           IsDerivedFrom(source_value->child, target_value->child))
          return BaseDistance(source_value->child, target_value->child);
        if(target_value->child && target_value->child->kind == TYPE_FUNDAMENTAL &&
           target_value->child->name == "void") return 2;
      }
      return -1;
    }
    if(target_value->kind == TYPE_FUNDAMENTAL && target_value->name == "nullptr_t")
      return source.null_pointer_constant || source_value->name == "nullptr_t" ? 1 : -1;
    if(target_value->kind == TYPE_FUNDAMENTAL && target_value->name == "bool" &&
       source_value->kind == TYPE_POINTER) return 3;
    if(PA12SameType(source_value, target_value, false)) return 0;
    if(PA12SameType(source_value, target_value, true)) return 1;
    if(source_value->kind == TYPE_CLASS && target_value->kind == TYPE_CLASS &&
       IsDerivedFrom(source_value, target_value))
      return BaseDistance(source_value, target_value);
    if(is_arithmetic_type(source_value) && is_arithmetic_type(target_value)) {
      if(source_value->kind == TYPE_ENUM && !source_value->scoped_enum &&
         target_value->kind == TYPE_FUNDAMENTAL && target_value->name == "int") return 1;
      return 2;
    }
    if(source_value->kind == TYPE_FUNCTION && target_value->kind == TYPE_FUNCTION &&
       PA12SameType(source_value, target_value, true)) return 0;
    if(target_value->kind == TYPE_CLASS)
      return ConversionRankToClass(source, target);
    return -1;
  }

} // namespace cppgm_pa14_lowering
