#include "pa14_lowering.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

using namespace std;

namespace cppgm_pa14_lowering {

namespace {

bool PA14CvCompatible(const TypePtr& source, const TypePtr& target)
{
    if(!source || !target || source->kind != target->kind) return false;
    if(source->is_const && !target->is_const) return false;
    if(source->is_volatile && !target->is_volatile) return false;
    if(source->kind == TYPE_ARRAY)
        return (source->bound == target->bound || source->bound < 0 || target->bound < 0) &&
            PA14CvCompatible(source->child, target->child);
    if(source->kind == TYPE_POINTER || source->kind == TYPE_LVALUE_REFERENCE ||
       source->kind == TYPE_RVALUE_REFERENCE)
        return PA14CvCompatible(source->child, target->child);
    return true;
}

} // namespace

string PA14Lowerer::AdjustDerivedAddress(const string& base,
                                         const TypePtr& raw_derived,
                                         const TypePtr& raw_base)
{
    TypePtr derived = type_value(raw_derived);
    TypePtr base_type = type_value(raw_base);
    if(!derived || !base_type || PA12SameType(derived, base_type, true)) return base;
    if(derived->kind != TYPE_CLASS || base_type->kind != TYPE_CLASS ||
       !IsDerivedFrom(derived, base_type))
      throw logic_error("class reference cast is not a base downcast");
    vector<size_t> path;
    set<const Type*> visited;
    function<bool(const TypePtr&)> find_base = [&](const TypePtr& current) {
      if(!current || !visited.insert(current.get()).second) return false;
      if(PA12SameType(current, base_type, true)) return true;
      if(!current->direct_bases.empty()) {
        for(size_t i = 0; i < current->direct_bases.size(); ++i) {
          const size_t offset = i < current->direct_base_offsets.size() ?
            current->direct_base_offsets[i] : (i == 0 ? current->direct_base_offset : 0);
          path.push_back(offset);
          if(find_base(type_value(current->direct_bases[i]))) return true;
          path.pop_back();
        }
      } else if(current->direct_base) {
        path.push_back(current->direct_base_offset);
        if(find_base(type_value(current->direct_base))) return true;
        path.pop_back();
      }
      return false;
    };
    if(!find_base(derived)) throw logic_error("class reference cast is not a base downcast");
    size_t offset = 0;
    for(size_t i = 0; i < path.size(); ++i) offset += path[i];
    if(offset == 0) return base;
    const string adjusted = new_temp();
    AddInstruction(adjusted + " = index i8 " + base + ", -" +
      integer_text(static_cast<long long>(offset)));
    return adjusted;
  }

PA14Lowerer::Value PA14Lowerer::EmitIdentifier(const CPPGMAstNodePtr& node, Scope* scope,
                       const TypePtr& expected)
{
    if(node && !node->value.empty() &&
       (isdigit(static_cast<unsigned char>(node->value[0])) ||
        ((node->value[0] == '-' || node->value[0] == '+') && node->value.size() > 1 &&
         isdigit(static_cast<unsigned char>(node->value[1]))))) {
      TypePtr literal_type;
      long long literal_value = 0;
      bool literal_known = false;
      const string literal = canonical_literal(node->value, &literal_type,
        &literal_value, &literal_known);
      if(literal_known) {
        Value result;
        result.type = literal_type;
        result.operand = literal;
        result.known_constant = true;
        result.constant = literal_value;
        return result;
      }
    }
    Value result;
    VariablePlan* local = LocalForName(node->value);
    if(local) {
      if(local->type->kind == TYPE_ARRAY) {
        result.type = local->type;
        result.array = true;
        result.operand = EmitArrayDecay(node, scope);
        return result;
      }
      if(type_is_reference(local->type)) {
        TypePtr referred = local->type->child;
        const string address = local_address(local);
        if(referred && referred->kind == TYPE_ARRAY) {
          result.type = referred;
          result.array = true;
          const string decay = new_temp();
          AddInstruction(decay + " = unary decay ptr " + address);
          result.operand = decay;
          return result;
        }
        if(referred && referred->kind == TYPE_FUNCTION) {
          result.type = referred;
          result.function = true;
          result.operand = address;
          const string decay = new_temp();
          AddInstruction(decay + " = unary decay ptr " + result.operand);
          result.operand = decay;
          return result;
        }
        result.type = referred;
        result.operand = emit_load(address, referred);
        return result;
      }
      if(local->parameter_address) {
        result.type = local->type;
        result.operand = local->parameter_operand;
        result.lvalue = true;
        return result;
      }
      result.type = local->type;
      result.operand = emit_load(StorageForVariable(*local), local->type);
      return result;
    }
    const bool decltype_form = node->value.compare(0, 9, "decltype(") == 0;
    Binding* decltype_member = ResolveDecltypeStaticMember(node->value, scope);
    vector<Binding*> candidates = decltype_member ?
      vector<Binding*>(1, decltype_member) : Lookup(node->value, scope);
    if(candidates.empty()) throw logic_error("unknown identifier during lowering: " + node->value);
    if(candidates.size() > 1) {
      bool repeated_binding = true;
      for(size_t i = 1; i < candidates.size(); ++i)
        if(candidates[i] != candidates[0]) {
          repeated_binding = false;
          break;
        }
      if(repeated_binding) throw logic_error("ambiguous identifier during lowering");
    }
    Binding* binding = candidates.size() == 1 ? candidates[0] : 0;
    if(!binding && candidates.size() > 1) {
      bool duplicate_declarations = true;
      for(size_t i = 1; i < candidates.size(); ++i)
        if(candidates[i]->qualified_name != candidates[0]->qualified_name ||
           !PA12SameType(candidates[i]->type, candidates[0]->type, false)) {
          duplicate_declarations = false;
          break;
        }
      if(duplicate_declarations) binding = candidates[0];
    }
    if(expected && !binding) {
      TypePtr target = type_value(expected);
      int best = 1000000;
      for(size_t i = 0; i < candidates.size(); ++i) {
        TypePtr function = function_target_type(candidates[i]->type);
        if(!function) continue;
        ExprInfo source;
        source.type = function;
        source.category = "lvalue";
        const int rank = ConversionRank(source, target);
        if(rank >= 0 && rank < best) { best = rank; binding = candidates[i]; }
      }
    }
    if(!binding && candidates.size() == 1) binding = candidates[0];
	if(!binding) throw logic_error("ambiguous identifier during lowering");
	if(!IsAccessible(binding, scope)) throw logic_error("inaccessible member");
	const TypePtr binding_value_type = type_value(binding->type);
	const bool binding_integral = binding_value_type &&
		(is_integral_type(binding_value_type) ||
			(binding_value_type->kind == TYPE_FUNDAMENTAL &&
			 binding_value_type->name == "bool"));
	GlobalRecord* early_demanded_global = 0;
	if(binding->is_member && binding->is_static && binding->member_owner)
		early_demanded_global = EnsureStaticMemberStorage(binding);
	const bool primary_template_static_member = binding->kind == BIND_VARIABLE &&
		binding->is_member && binding->is_static && binding->member_owner &&
		binding->member_owner->template_specialization && binding->declaration &&
		binding->declaration->template_instantiation &&
		!binding->declaration->explicit_specialization;
	bool primary_has_explicit_specialization = false;
	if(primary_template_static_member && binding->member_owner) {
		const string member_name = LastComponent(binding->qualified_name);
		for(size_t global = 0; global < globals_.size(); ++global) {
			const GlobalRecord& candidate = globals_[global];
			if(!candidate.explicit_specialization || !candidate.template_owner ||
				LastComponent(candidate.qualified_name) != member_name ||
				candidate.template_owner->template_primary !=
					binding->member_owner->template_primary) continue;
			primary_has_explicit_specialization = true;
			break;
		}
	}
	const bool template_static_storage_override = binding->member_owner &&
		binding->member_owner->template_specialization &&
		((primary_template_static_member && primary_has_explicit_specialization) ||
		 (early_demanded_global && early_demanded_global->explicit_specialization));
	const bool explicit_specialized_static_storage = early_demanded_global &&
		early_demanded_global->explicit_specialization;
    const bool generated_member_template = early_demanded_global &&
		early_demanded_global->node &&
		early_demanded_global->node->template_instantiation &&
		early_demanded_global->node->template_primary.find("::") != string::npos;
    const bool dependent_static_initializer = early_demanded_global &&
		early_demanded_global->initializer &&
		DescendantOfKind(early_demanded_global->initializer, "sizeof-expression");
	const bool qualified_materialized_template_static = node &&
		node->value.find("::") != string::npos && early_demanded_global &&
		early_demanded_global->template_instantiation &&
		early_demanded_global->initializer && binding->member_owner &&
		binding->member_owner->template_specialization &&
		!binding->has_value &&
		(generated_member_template || dependent_static_initializer);
	const bool template_function_static_use = node && node->template_instantiation &&
		!node->template_primary.empty();
	const bool qualified_template_static_use = node &&
		node->value.find("::") != string::npos && early_demanded_global &&
		early_demanded_global->template_instantiation &&
		early_demanded_global->initializer && binding->member_owner &&
		binding->member_owner->template_specialization && binding->has_value &&
		binding_value_type && binding_value_type->kind == TYPE_FUNDAMENTAL &&
		binding_value_type->name == "bool" &&
		template_function_static_use;
	const bool materialized_template_static = qualified_materialized_template_static ||
		qualified_template_static_use;
	if(binding->kind == BIND_VARIABLE && binding->has_value && binding->declaration &&
		binding->declaration->template_instantiation && binding_integral &&
		!template_static_storage_override && !materialized_template_static) {
		result.type = binding->type;
		result.operand = integer_text(binding->value);
		result.known_constant = true;
		result.constant = binding->value;
		return result;
	}
	if(binding->kind == BIND_ENUMERATOR) {
      result.type = binding->type;
      result.operand = integer_text(binding->value);
      result.known_constant = binding->has_value;
      result.constant = binding->value;
      return result;
    }
	if(binding->is_member && binding->member_owner) {
      if(binding->kind == BIND_FUNCTION) {
        FunctionRecord* function = RecordForBinding(binding);
        if(!function) throw logic_error("unknown member function symbol during lowering");
        if(function->member) MarkFunctionNeeded(function);
        result.type = function->type;
        result.function = true;
        result.operand = function_address(function);
        return result;
      }
		result.type = binding->type;
		if(binding->is_static) {
		GlobalRecord* demanded_global = EnsureStaticMemberStorage(binding,
			decltype_form);
		if(binding->has_value && !decltype_form && !template_static_storage_override &&
			!materialized_template_static) {
          result.known_constant = true;
          result.constant = binding->value;
          result.operand = integer_text(result.constant);
          return result;
        }
        const TypePtr static_value_type = type_value(binding->type);
		if(!decltype_form && demanded_global && demanded_global->initializer && static_value_type &&
			static_value_type->is_const &&
			(!template_static_storage_override || explicit_specialized_static_storage) &&
			!materialized_template_static) {
			long long constant = 0;
          if(FoldInteger(InitializerExpression(demanded_global->initializer), scope,
              &constant, 0)) {
            result.known_constant = true;
            result.constant = constant;
            result.operand = integer_text(constant);
            return result;
			}
		}
        GlobalRecord* global_member = demanded_global ? demanded_global :
          FindGlobal(binding->qualified_name);
		if(!global_member) throw logic_error("unknown static member during lowering");
        result.type = global_member->type;
        result.operand = global_member->type->kind == TYPE_ARRAY ?
          EmitArrayDecay(node, scope) : emit_load("@" + global_member->symbol, global_member->type);
        result.array = global_member->type->kind == TYPE_ARRAY;
        return result;
      }
      CPPGMAstNodePtr this_node(new CPPGMAstNode("keyword-literal", "KW_THIS:this"));
      if(binding->name != node->value && node->value.find("::") != string::npos) {
        Value this_value = EmitValue(this_node, scope);
        TypePtr object = expression_value_type(Infer(this_node, scope));
        if(object && object->kind == TYPE_POINTER) object = type_value(object->child);
        string base = AdjustBaseAddress(this_value.operand, object, binding->member_owner);
        if(binding->member_index == static_cast<size_t>(-1) || !binding->member_owner ||
           binding->member_index >= binding->member_owner->class_members.size())
          throw logic_error("member has no layout record");
        const ClassMemberInfo& fact = binding->member_owner->class_members[binding->member_index];
        const string address = new_temp();
        const string projection = type_is_reference(fact.type) ?
          "[projection=reference_field] " : "[projection=field] ";
        AddInstruction(address + " = index i8 " + projection + base + ", " +
          integer_text(fact.offset));
        result.type = binding->type;
        if(type_is_reference(result.type)) result.type = result.type->child;
        if(object && object->is_const && !fact.is_mutable)
          result.type = CloneWithCv(result.type, true, object->is_volatile);
        if(IsBitField(binding)) {
          TypePtr read_type = expected ? type_value(expected) : result.type;
          result = EmitBitFieldLoad(binding, address, read_type, static_cast<bool>(expected));
        } else if(type_is_reference(fact.type)) {
          const string referred = emit_load(address, PointerTo(Fundamental("char")));
          result.operand = emit_load(referred, result.type);
        } else result.operand = emit_load(address, result.type);
        result.lvalue = false;
        return result;
      }
      CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "OP_ARROW:->"));
      member->children.push_back(this_node);
      member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", binding->name)));
      ExprInfo member_info = InferMember(member, scope);
      result.type = member_info.type;
      if(result.type && result.type->kind == TYPE_ARRAY) {
        result.array = true;
        result.operand = EmitArrayDecay(member, scope);
        return result;
      }
      const string address = EmitMemberAddress(member, scope, true);
      if(IsBitField(binding)) {
        TypePtr read_type = expected ? type_value(expected) : result.type;
        result = EmitBitFieldLoad(binding, address, read_type,
          static_cast<bool>(expected));
      } else if(type_is_reference(binding->type)) {
        const string referred = emit_load(address, PointerTo(Fundamental("char")));
        result.operand = emit_load(referred, result.type);
      } else
        result.operand = emit_load(address, result.type);
      result.lvalue = false;
      return result;
    }
    if(binding->kind == BIND_FUNCTION) {
      FunctionRecord* function = RecordForBinding(binding);
      if(!function) throw logic_error("unknown function symbol during lowering");
      result.type = function->type;
      result.function = true;
      result.operand = function_address(function);
      return result;
    }
    GlobalRecord* global = FindGlobal(binding->qualified_name);
    if(!global) throw logic_error("unknown global during lowering");
    result.type = global->type;
    if(global->type->kind == TYPE_ARRAY) {
      result.array = true;
      result.operand = EmitArrayDecay(node, scope);
    } else result.operand = emit_load("@" + global->symbol, global->type);
    return result;
  }

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
      // A user-defined assignment operator does not change the ABI's
      // register/aggregate classification for returning the object.  Only
      // copy/move construction (and the corresponding destruction rules)
      // make the value non-trivial for this storage decision.
      if(record->copy_assignment || record->move_assignment) continue;
      if(record->deleted) continue;
      if(!record->defaulted && !record->implicit_constructor) return false;
    }
    return true;
  }

bool PA14Lowerer::IsEmptyBaseStorage(const TypePtr& raw_type) const
{
    TypePtr type = type_value(raw_type);
    if(!type || type->kind != TYPE_CLASS) return false;
    if(type->polymorphic || type->has_vpointer) return false;
    for(size_t i = 0; i < type->class_members.size(); ++i) {
      const ClassMemberInfo& member = type->class_members[i];
      if(!member.is_static && member.type) return false;
    }
    return !type->direct_base || IsEmptyBaseStorage(type->direct_base);
  }

bool PA14Lowerer::ClassValueNeedsIndirect(const TypePtr& raw_type) const
{
    TypePtr type = type_value(raw_type);
    if(!type || type->kind != TYPE_CLASS) return false;
    if(type->is_union) return true;
    // A materialized class specialization carrying another specialization as
    // a template argument has ABI-sensitive nested object state.  Preserve
    // the indirect-result boundary instead of classifying the outer 16-byte
    // shell as a scalar aggregate.
    if(type->template_specialization) {
      bool nested_argument = false;
      for(size_t argument = 0; argument < type->template_arguments.size(); ++argument)
        if(type->template_arguments[argument].find('<') != string::npos) {
          nested_argument = true;
          break;
        }
          if(nested_argument)
        for(size_t member = 0; member < type->class_members.size(); ++member)
          if(!type->class_members[member].is_static && type->class_members[member].type) {
            const TypePtr member_type = type->class_members[member].type;
            const TypePtr member_value = type_value(member_type);
            if(type_is_reference(member_type) && member_value &&
               member_value->kind == TYPE_CLASS &&
               member_value->template_specialization)
              return true;
          }
    }
    // A concrete pair-like result with a declared move constructor and a
    // converting constructor template crosses the same non-trivial object
    // boundary as the source ABI.  The template constructor is not itself a
    // copy/move member, so it is intentionally not folded into
    // IsTrivialValueStorage; keep this typed classification local to the
    // result ABI decision.
    bool has_move_constructor_template = false;
    bool has_nonstatic_data_member = false;
    for(size_t member = 0; member < type->class_members.size(); ++member)
      if(!type->class_members[member].is_static &&
         type->class_members[member].type) {
        has_nonstatic_data_member = true;
        break;
      }
    if(type->owned_scope) for(size_t binding_index = 0;
        binding_index < type->owned_scope->bindings.size(); ++binding_index) {
      const Binding& binding = type->owned_scope->bindings[binding_index];
      if(binding.kind != BIND_FUNCTION) continue;
      FunctionRecord* record = RecordForBinding(const_cast<Binding*>(&binding));
      if(record && record->constructor &&
         !record->value_special_member) {
        has_move_constructor_template = true;
        break;
      }
    }
    if(has_move_constructor_template && has_nonstatic_data_member &&
       ClassHasDeclaredMoveMember(type)) return true;
    if(type_size(type) > 16) return true;
    bool base_only = type->direct_base != 0;
    for(size_t i = 0; i < type->class_members.size(); ++i) {
      const ClassMemberInfo& member = type->class_members[i];
      if(!member.is_static && member.type) { base_only = false; break; }
    }
    if(!IsTrivialValueStorage(type) &&
       !(base_only && !ClassHasDeclaredMoveMember(type))) return true;
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
    const string qname = owner->name + "::" +
      special_member_symbol_name(owner, name);
    const string key = function_key(qname,
      FunctionOf(vector<TypePtr>(1, parameter), false, Fundamental("void"), false));
    map<string, FunctionRecord*>::const_iterator found = function_by_key_.find(key);
    if(found != function_by_key_.end()) return found->second;
    const string parameter_name = "other";
    CPPGMAstNodePtr special = SyntheticValueMember(name, parameter_name, move, false);
    Binding binding(BIND_FUNCTION, name, FunctionOf(vector<TypePtr>(1, parameter), false,
      Fundamental("void"), false));
    binding.qualified_name = qname;
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
         !PA12SameType(type_value(function->parameters[0]), owner, true)) continue;
      FunctionRecord* candidate = RecordForBinding(candidates[i]);
      if(!candidate || candidate->member_template) continue;
      if(function->parameters[0]->kind == TYPE_RVALUE_REFERENCE) {
        if(move) return candidate;
      } else if(!copy_fallback) copy_fallback = candidate;
    }
    // A user-declared copy assignment may take the class by value, not only
    // by const lvalue reference.  It still suppresses implicit move
    // assignment, so never manufacture a competing rvalue-reference
    // candidate for that typed declaration.
    if(copy_fallback && !copy_fallback->synthesized_value_member) return copy_fallback;
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

vector<Binding*> PA14Lowerer::ConversionBindings(const TypePtr& raw_source) const
{
    vector<Binding*> result;
    set<Binding*> seen;
    TypePtr source = type_value(raw_source);
    for(TypePtr current = source; current && current->kind == TYPE_CLASS;
        current = type_value(current->direct_base)) {
      if(!current->owned_scope) continue;
      for(size_t i = 0; i < current->owned_scope->bindings.size(); ++i) {
        Binding* binding = const_cast<Binding*>(&current->owned_scope->bindings[i]);
        if(!binding || binding->kind != BIND_FUNCTION || !binding->is_member ||
           binding->is_static || !seen.insert(binding).second) continue;
        if(binding->name.compare(0, 8, "operator") != 0) continue;
        const string suffix = binding->name.substr(8);
        if(suffix.empty() || string("+-*/%^&|=!<>~[],()").find(suffix[0]) != string::npos)
          continue;
      TypePtr function = function_target_type(binding->type);
      if(!function || !function->parameters.empty()) continue;
      FunctionRecord* record = RecordForBinding(binding);
      if(!record || !record->member || record->static_member || record->deleted) continue;
      if(source->is_const && !function->function_const) continue;
      if(source->is_volatile && !function->function_volatile) continue;
        result.push_back(binding);
      }
    }
    return result;
  }

Binding* PA14Lowerer::FindConversionOperator(const TypePtr& raw_source,
                                              const TypePtr& raw_target,
                                              bool allow_explicit, int* rank) const
{
    if(rank) *rank = -1;
    TypePtr source = type_value(raw_source);
    TypePtr target = type_value(raw_target);
    if(!source || source->kind != TYPE_CLASS || !target) return 0;
    // An object is already in the target class for an identity conversion.
    // Looking through its conversion operators in that case is both
    // semantically wrong (the copy/move/reference path is preferred) and can
    // recurse when an operator's result is the same class.
    if(target->kind == TYPE_CLASS && PA12SameType(source, target, true))
      return 0;
    Binding* best = 0;
    int best_rank = 1000000;
    const vector<Binding*> candidates = ConversionBindings(source);
    for(size_t i = 0; i < candidates.size(); ++i) {
      Binding* binding = candidates[i];
      FunctionRecord* record = RecordForBinding(binding);
      TypePtr function = function_target_type(binding->type);
      if(!record || !function || (!allow_explicit && record->explicit_constructor)) continue;
      if(source->is_const && !function->function_const) continue;
      if(source->is_volatile && !function->function_volatile) continue;
      TypePtr result_type = function->child;
      TypePtr result_value = type_value(result_type);
      int standard = -1;
      if(result_value && result_value->kind == TYPE_CLASS && target->kind == TYPE_CLASS) {
        if(PA12SameType(result_value, target, false)) standard = 0;
        else if(PA12SameType(result_value, target, true)) standard = 1;
        else if(IsDerivedFrom(result_value, target)) standard = BaseDistance(result_value, target);
      } else if(result_value && result_value->kind == TYPE_CLASS) {
        // A conversion function already supplies the one user-defined
        // conversion allowed in an implicit conversion sequence.  Ranking a
        // class result against a scalar/pointer target by calling
        // ConversionRank again would permit a second conversion function and
        // can recurse indefinitely for conversion-function templates.
        standard = -1;
      } else if(result_value && result_value->kind != TYPE_CLASS && target->kind == TYPE_CLASS) {
        // A conversion function followed by a converting constructor would
        // require two user-defined conversions and is not viable here.
        standard = -1;
      } else {
        ExprInfo converted;
        converted.type = result_type;
        converted.category = type_is_reference(result_type) ?
          (result_type->kind == TYPE_LVALUE_REFERENCE ? "lvalue" : "xvalue") : "prvalue";
        standard = ConversionRank(converted, raw_target);
      }
      if(standard < 0) continue;
      const int object_rank = (function->function_const ? 1 : 0) +
        (function->function_volatile ? 1 : 0);
      const int candidate_rank = 3 + standard + object_rank;
      if(!best || candidate_rank < best_rank) {
        best = binding;
        best_rank = candidate_rank;
		} else if(candidate_rank == best_rank) {
			FunctionRecord* best_record = RecordForBinding(best);
			const bool candidate_template = record->member_template;
			const bool best_template = best_record && best_record->member_template;
        if(best_template && !candidate_template) {
          best = binding;
          continue;
        }
			if(!best_template && candidate_template) continue;
			TypePtr best_function = function_target_type(best->type);
			if(best_function && !function->function_const && best_function->function_const) {
				best = binding;
				continue;
			}
			if(best_function && function->function_const && !best_function->function_const)
				continue;
			if(!PA12SameType(function, function_target_type(best->type), false))
          throw logic_error("ambiguous conversion function");
      }
    }
    if(rank && best) *rank = best_rank;
    return best;
  }

Binding* PA14Lowerer::FindContextConversionOperator(const TypePtr& raw_source,
                                                     bool allow_explicit,
                                                     bool boolean_context) const
{
    TypePtr source = type_value(raw_source);
    if(!source || source->kind != TYPE_CLASS) return 0;
    if(boolean_context) {
      Binding* direct = FindConversionOperator(source, Fundamental("bool"),
        allow_explicit);
      if(direct) return direct;
    }
    Binding* best = 0;
    for(size_t i = 0; i < ConversionBindings(source).size(); ++i) {
      Binding* binding = ConversionBindings(source)[i];
      FunctionRecord* record = RecordForBinding(binding);
      TypePtr function = function_target_type(binding->type);
      TypePtr result = function ? type_value(function->child) : TypePtr();
      if(!record || !function || (!allow_explicit && record->explicit_constructor) || !result)
        continue;
      if((boolean_context && !is_arithmetic_type(result) && result->kind != TYPE_POINTER) ||
         (!boolean_context && !is_arithmetic_type(result) && result->kind != TYPE_POINTER) ||
         (source->is_const && !function->function_const)) continue;
      if(!best) best = binding;
    }
    return best;
  }

Binding* PA14Lowerer::FindNamedConversionOperator(const TypePtr& raw_source,
                                                   const string& spelling,
                                                   Scope* scope) const
{
    if(spelling.compare(0, 8, "operator") != 0) return 0;
    string target_spelling = spelling.substr(8);
    while(!target_spelling.empty() && target_spelling[0] == ' ')
      target_spelling.erase(0, 1);
    while(!target_spelling.empty() && target_spelling[target_spelling.size() - 1] == ' ')
      target_spelling.erase(target_spelling.size() - 1, 1);
    while(!target_spelling.empty()) {
      while(!target_spelling.empty() &&
            (target_spelling[target_spelling.size() - 1] == '&' ||
             target_spelling[target_spelling.size() - 1] == '*'))
        target_spelling.erase(target_spelling.size() - 1, 1);
      while(!target_spelling.empty() && target_spelling[target_spelling.size() - 1] == ' ')
        target_spelling.erase(target_spelling.size() - 1, 1);
      bool removed_cv = false;
      if(target_spelling.size() >= 5 &&
         target_spelling.compare(target_spelling.size() - 5, 5, "const") == 0) {
        target_spelling.erase(target_spelling.size() - 5);
        removed_cv = true;
      } else if(target_spelling.size() >= 8 &&
                target_spelling.compare(target_spelling.size() - 8, 8, "volatile") == 0) {
        target_spelling.erase(target_spelling.size() - 8);
        removed_cv = true;
      }
      while(!target_spelling.empty() && target_spelling[target_spelling.size() - 1] == ' ')
        target_spelling.erase(target_spelling.size() - 1, 1);
      if(!removed_cv) break;
    }
    if(target_spelling.empty()) return 0;
    vector<string> words;
    string word;
    for(size_t i = 0; i <= target_spelling.size(); ++i) {
      const char ch = i < target_spelling.size() ? target_spelling[i] : ' ';
      if(isspace(static_cast<unsigned char>(ch))) {
        if(!word.empty()) { words.push_back(word); word.clear(); }
      } else word += ch;
    }
    string resolved_spelling;
    for(size_t i = 0; i < words.size(); ++i) {
      if(words[i] == "const" || words[i] == "volatile" ||
         words[i] == "&" || words[i] == "&&" || words[i] == "*") continue;
      if(words[i] == "::") {
        while(!resolved_spelling.empty() && resolved_spelling[resolved_spelling.size() - 1] == ' ')
          resolved_spelling.erase(resolved_spelling.size() - 1, 1);
        resolved_spelling += "::";
      } else {
        if(!resolved_spelling.empty() && resolved_spelling[resolved_spelling.size() - 1] != ':')
          resolved_spelling += " ";
        resolved_spelling += words[i];
      }
    }
    if(resolved_spelling.empty()) return 0;
    CPPGMAstNodePtr target_node(new CPPGMAstNode("id-expression", resolved_spelling));
    TypePtr target = BuiltinCastType(target_node, scope);
    if(!target) {
      Analyzer::PathTarget resolved = analyzer_.ResolvePath(scope, resolved_spelling);
      if(resolved.binding && (resolved.binding->kind == BIND_TYPE ||
                              resolved.binding->kind == BIND_TYPE_ALIAS))
        target = resolved.binding->type;
    }
    if(!target) return 0;
    const vector<Binding*> candidates = ConversionBindings(raw_source);
    for(size_t i = 0; i < candidates.size(); ++i) {
      TypePtr function = function_target_type(candidates[i]->type);
      if(function && function->child &&
         PA12SameType(type_value(function->child), type_value(target), true))
        return candidates[i];
    }
    return 0;
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
    // A captureless lambda is still represented as a function pointer while
    // overload candidates are being ranked.  A materialized closure class is
    // the corresponding typed object parameter, not a user-defined
    // conversion through a constructor.  Recognize this identity here so
    // the selected call can materialize the final closure object exactly once.
    if(IsLambdaClosureType(target_value) && source_value->kind == TYPE_POINTER &&
       source_value->child && source_value->child->kind == TYPE_FUNCTION)
      return 0;
    // The recursive conversion hazard is specific to class values.  The
    // generic PA12 type relation deliberately ignores nested cv for some
    // callers, but that is not an identity conversion for pointer types
    // (e.g. `int**` to `const int**`).
    const bool same_class_value = source_value->kind == TYPE_CLASS &&
      target_value->kind == TYPE_CLASS &&
      PA12SameType(source_value, target_value, true);
    if(same_class_value && target->kind != TYPE_LVALUE_REFERENCE &&
       target->kind != TYPE_RVALUE_REFERENCE)
      return 0;
    if(same_class_value && target->kind == TYPE_LVALUE_REFERENCE &&
       source.category == "lvalue" &&
       (!source_value->is_const || target_value->is_const))
      return 0;
    if(same_class_value && target->kind == TYPE_RVALUE_REFERENCE &&
       source.category != "lvalue" &&
       (!source_value->is_const || target_value->is_const))
      return 0;
    if(target->kind == TYPE_RVALUE_REFERENCE && source.category == "lvalue" &&
       is_arithmetic_type(source_value) && is_arithmetic_type(target_value)) return 2;
    if(source_value->kind == TYPE_CLASS) {
      int conversion_rank = -1;
      if(FindConversionOperator(source_value, target, false, &conversion_rank))
        return conversion_rank;
    }
    if(target->kind == TYPE_LVALUE_REFERENCE || target->kind == TYPE_RVALUE_REFERENCE) {
      if(target->kind == TYPE_LVALUE_REFERENCE) {
        if(source.category == "lvalue") {
          // A reference to an array carries cv qualification on the element
          // type, not on the array wrapper.  Keep that typed conversion rule
          // visible here so string literals and other array lvalues can bind
          // to `T const&` without being decayed to a pointer first.
          if(source_value->kind == TYPE_ARRAY && target_value->kind == TYPE_ARRAY &&
             (source_value->bound < 0 || target_value->bound < 0) &&
             PA12SameType(source_value->child, target_value->child, true) &&
             PA14CvCompatible(source_value, target_value))
            return 1;
          if(source_value->kind == TYPE_ARRAY && target_value->kind == TYPE_ARRAY &&
             PA12SameType(source_value, target_value, true) &&
             PA14CvCompatible(source_value, target_value))
            return PA12SameType(source_value, target_value, false) ? 0 : 1;
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
        if(target_value->kind == TYPE_POINTER && source_value->kind == TYPE_FUNCTION &&
           target_value->child && target_value->child->kind == TYPE_FUNCTION &&
           PA12SameType(source_value, target_value->child, true)) return 1;
        if(is_arithmetic_type(source_value) && is_arithmetic_type(target_value)) return 2;
        return -1;
      }
      if(PA12SameType(source_value, target_value, true)) return 0;
      if(source_value->kind == TYPE_CLASS && target_value->kind == TYPE_CLASS &&
         IsDerivedFrom(source_value, target_value))
        return BaseDistance(source_value, target_value);
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
