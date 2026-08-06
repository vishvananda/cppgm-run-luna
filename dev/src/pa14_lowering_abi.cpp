#include "pa14_lowering.h"

#include <functional>
#include <map>

using namespace std;

namespace cppgm_pa14_lowering {

void PA14Lowerer::BuildFunctionABI(FunctionRecord& function)
{
    function.hidden_virtual_bases.clear();
    function.hidden_virtual_base_sources.clear();
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
    if(function.vtt_parameter) {
      parameters.push_back(PointerTo(Fundamental("char")));
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
    const size_t this_index = function.indirect_result ? 1 : 0;
    // A base-entry constructor is entered on a base subobject and must receive
    // the virtual-base addresses from the most-derived object.  Complete
    // constructors own those subobjects and ordinary member/destructor
    // functions use the normal `this` ABI; their virtual-base observations
    // are carried by ordinary reference/pointer parameters instead.
    const bool reference_copy_like = function.source_type &&
      function.source_type->parameters.size() == 1 &&
      function.source_type->parameters[0] &&
      type_is_reference(function.source_type->parameters[0]) &&
      function.member_owner &&
      PA12SameType(type_value(function.source_type->parameters[0]->child),
                   type_value(function.member_owner), true);
    const bool base_entry_virtual_views = function.member && !function.static_member &&
      function.base_entry && function.member_owner && HasVirtualBases(function.member_owner) &&
      ((function.constructor &&
        (function.construction_entry ||
         (!function.copy_constructor && !function.move_constructor &&
          !reference_copy_like))) ||
       function.destructor);
    if(base_entry_virtual_views) {
      const vector<TypePtr> bases = VirtualBaseTypes(function.member_owner);
      for(size_t i = 0; i < bases.size(); ++i) {
        if(!bases[i]) continue;
        function.hidden_virtual_bases.push_back(bases[i]);
        function.hidden_virtual_base_sources.push_back(this_index);
        parameters.push_back(PointerTo(Fundamental("char")));
        indirect.push_back(false);
      }
    }
    bool uses_virtual_base_member = false;
    if(function.member && !function.static_member && !function.constructor &&
       !function.destructor && function.member_owner &&
       HasVirtualBases(function.member_owner) && function.node && function.scope) {
      CPPGMAstNodePtr body = ChildOfKind(function.node, "compound-statement");
      if(!body && function.node->children.size() > 2)
        body = function.node->children[2];
      std::function<void(const CPPGMAstNodePtr&)> scan_member_uses;
      scan_member_uses = [&](const CPPGMAstNodePtr& node) {
        if(!node || uses_virtual_base_member) return;
        if(node->kind == "cast-expression" && node->children.size() > 1 &&
           PA12Operator(node->value) == "static_cast" && node->children[0]) {
          const TypePtr cast_type = analyzer_.TypeFromTypeId(
            node->children[0], function.scope);
          const TypePtr cast_value = type_value(cast_type);
          size_t virtual_offset = 0;
          if(cast_value && cast_value->kind == TYPE_CLASS &&
             FindVirtualBaseOffset(function.member_owner, cast_value,
                                   &virtual_offset))
            uses_virtual_base_member = true;
        }
        if((node->kind == "id-expression" || node->kind == "identifier") &&
           !node->value.empty()) {
          vector<Binding*> bindings = Lookup(node->value, function.scope);
          if(bindings.empty()) bindings = MemberBindings(function.member_owner, node->value);
          for(size_t binding = 0; binding < bindings.size(); ++binding) {
            Binding* candidate = bindings[binding];
            if(!candidate || !candidate->is_member || candidate->is_static ||
               !candidate->member_owner) continue;
            size_t virtual_offset = 0;
            if(FindVirtualBaseOffset(function.member_owner,
                                     type_value(candidate->member_owner),
                                     &virtual_offset)) {
              uses_virtual_base_member = true;
              break;
            }
          }
        }
        for(size_t child = 0; child < node->children.size(); ++child)
          scan_member_uses(node->children[child]);
      };
      scan_member_uses(body);
    }
    if(uses_virtual_base_member) {
      const vector<TypePtr> bases = VirtualBaseTypes(function.member_owner);
      for(size_t i = 0; i < bases.size(); ++i) {
        if(!bases[i]) continue;
        function.hidden_virtual_bases.push_back(bases[i]);
        function.hidden_virtual_base_sources.push_back(this_index);
        parameters.push_back(PointerTo(Fundamental("char")));
        indirect.push_back(false);
      }
    }
    // Ordinary parameters are lowered from a potentially adjusted class
    // view.  Carry one address per virtual edge occurrence after all source
    // parameters, preserving the source parameter grouping in the typed
    // metadata even when two edges share one physical subobject.
    const size_t ordinary_index = this_index +
      (function.member && !function.static_member ? 1 : 0) +
      (function.vtt_parameter ? 1 : 0);
    for(size_t i = 0; i < source->parameters.size(); ++i) {
      const TypePtr carrier = virtual_base_carrier(source->parameters[i]);
      if(!carrier || carrier->kind != TYPE_CLASS) continue;
      vector<TypePtr> bases = VirtualBaseTypes(carrier);
      // A by-value pointer view carries the root virtual-base address needed
      // to rebase that pointer.  A reference-to-pointer forwarding parameter
      // retains the complete typed path because it may be forwarded without
      // another source-object evaluation.
      const TypePtr parameter = source->parameters[i];
      if(parameter && parameter->kind == TYPE_POINTER) {
        vector<TypePtr> roots;
        const vector<TypePtr> root_types = carrier->virtual_base_roots;
        for(size_t base = 0; base < bases.size(); ++base) {
          const TypePtr root = base < root_types.size() && root_types[base] ?
            root_types[base] : bases[base];
          bool seen_root = false;
          for(size_t prior = 0; prior < roots.size(); ++prior)
            if(PA12SameType(roots[prior], root, true)) { seen_root = true; break; }
          if(!seen_root) roots.push_back(root);
        }
        bases = roots;
      }
      // A reference parameter ABI only needs incoming virtual views that the
      // function can observe directly.  Keep the complete path in the
      // lowering state (EmitFunction materializes omitted views from the
      // ordinary reference), but avoid widening a declaration merely because
      // the referenced class has other virtual bases.  Forwarding wrappers
      // without a direct member observation retain the full path so that they
      // can pass it on to a callee.
      if(parameter && type_is_reference(parameter) &&
         type_value(parameter->child) &&
         type_value(parameter->child)->kind == TYPE_CLASS &&
         function.node && function.scope) {
        CPPGMAstNodePtr declarator = function.constructor || function.destructor ||
          function.value_special_member ? ChildOfKind(function.node, "declarator") :
          (function.node->children.size() > 1 ? function.node->children[1] :
           CPPGMAstNodePtr());
        CPPGMAstNodePtr clause = declarator ? DescendantOfKind(declarator,
          "parameter-clause") : CPPGMAstNodePtr();
        string parameter_name;
        size_t parameter_position = i;
        if(clause) {
          size_t seen = 0;
          for(size_t child = 0; child < clause->children.size(); ++child) {
            CPPGMAstNodePtr declaration = clause->children[child];
            if(!declaration || declaration->kind != "parameter-declaration") continue;
            if(seen++ != parameter_position) continue;
            CPPGMAstNodePtr parameter_declarator = declaration->children.size() > 1 ?
              declaration->children[1] : CPPGMAstNodePtr();
            parameter_name = parameter_name.empty() ?
              this->parameter_name(parameter_declarator, i) : parameter_name;
            break;
          }
        }
        if(!parameter_name.empty()) {
          set<const Type*> used;
          std::function<void(const CPPGMAstNodePtr&)> scan =
            [&](const CPPGMAstNodePtr& node) {
              if(!node) return;
              if(node->kind == "member-expression" && node->children.size() > 1) {
                CPPGMAstNodePtr object = node->children[0];
                if(object && object->kind == "id-expression" &&
                   object->value == parameter_name) {
                  const vector<Binding*> members = MemberBindings(carrier,
                    node->children[1]->value);
                  for(size_t member = 0; member < members.size(); ++member) {
                    Binding* binding = members[member];
                    if(binding && binding->member_owner &&
                       binding->kind != BIND_FUNCTION)
                      used.insert(type_value(binding->member_owner).get());
                  }
                }
              }
              for(size_t child = 0; child < node->children.size(); ++child)
                scan(node->children[child]);
            };
          CPPGMAstNodePtr scan_root = ChildOfKind(function.node,
            "compound-statement");
          if(!scan_root && function.node->children.size() > 2)
            scan_root = function.node->children[2];
          scan(scan_root);
          if(!used.empty()) {
            vector<TypePtr> observed;
            for(size_t base = 0; base < bases.size(); ++base) {
              if(bases[base] && used.find(type_value(bases[base]).get()) != used.end())
                observed.push_back(bases[base]);
            }
            if(!observed.empty()) bases = observed;
          }
        }
      }
      for(size_t base = 0; base < bases.size(); ++base) {
        if(!bases[base]) continue;
        function.hidden_virtual_bases.push_back(bases[base]);
        function.hidden_virtual_base_sources.push_back(ordinary_index + i);
        parameters.push_back(PointerTo(Fundamental("char")));
        indirect.push_back(false);
      }
    }
    function.indirect_parameters = indirect;
    function.type = FunctionOf(parameters, source->variadic,
      function.indirect_result ? Fundamental("void") : result, false);
  }

bool PA14Lowerer::TryCompleteConstructorVirtualBaseArgument(
  FunctionRecord& function, const TypePtr& target, size_t source,
  vector<string>& operands, Scope* scope)
{
    const size_t this_source = function.indirect_result ? 1 : 0;
    if(!state_ || !function.member || function.static_member ||
       function.constructor || function.destructor || !function.member_owner ||
       source != this_source || !state_->record || !state_->record->constructor)
      return false;
    const TypePtr owner = type_value(function.member_owner);
    size_t offset = 0;
    if(!owner || !FindVirtualBaseOffset(owner, target, &offset)) return false;
    CPPGMAstNodePtr this_node(new CPPGMAstNode("keyword-literal", "this"));
    string complete_this = EmitValue(this_node, scope).operand;
    if(offset != 0) {
      const string adjusted = new_temp();
      AddInstruction(adjusted + " = index i8 " + complete_this + ", " +
        integer_text(static_cast<long long>(offset)));
      complete_this = adjusted;
    }
    operands.push_back(complete_this);
    return true;
}

bool PA14Lowerer::TryForwardedVirtualBaseArgument(
  FunctionRecord& function, const TypePtr& target, size_t source,
  const vector<CPPGMAstNodePtr>& arguments, Scope* scope, TypePtr* carrier,
  string* operand, string* projection_operand,
  bool* complete_argument_projection, bool* null_pointer)
{
    if(!carrier || !operand || !projection_operand ||
       !complete_argument_projection || !null_pointer) return false;
    const TypePtr source_type = LowParameterSourceType(function, source);
    *carrier = virtual_base_carrier(source_type);
    *projection_operand = *operand;
    *complete_argument_projection = false;
    *null_pointer = false;
    if(state_) {
      map<string, vector<string> >::const_iterator by_operand =
        state_->virtual_base_hidden_by_operand.find(*operand);
      if(by_operand != state_->virtual_base_hidden_by_operand.end()) {
        for(size_t mapped = 0; mapped < function.hidden_virtual_bases.size(); ++mapped)
          if(function.hidden_virtual_bases[mapped] &&
             PA12SameType(function.hidden_virtual_bases[mapped], target, true) &&
             mapped < by_operand->second.size() &&
             !by_operand->second[mapped].empty()) {
            *operand = by_operand->second[mapped];
            return true;
          }
      }
    }
    const size_t ordinary_base = (function.indirect_result ? 1 : 0) +
      (function.member && !function.static_member ? 1 : 0) +
      (function.vtt_parameter ? 1 : 0);
    if(source < ordinary_base) return false;
    const size_t argument = source - ordinary_base;
    if(argument >= arguments.size() || !arguments[argument]) return false;
    ExprInfo info = Infer(arguments[argument], scope);
    const TypePtr info_type = type_value(info.type);
    *null_pointer = info.null_pointer_constant ||
      (info_type && info_type->kind == TYPE_FUNDAMENTAL &&
       info_type->name == "nullptr_t") || info.operand == "nullptr";
    if(!*null_pointer && arguments[argument]->kind == "id-expression" && state_) {
      const TypePtr actual = virtual_base_carrier(info.type);
      map<string, vector<string> >::const_iterator forwarded =
        state_->virtual_base_hidden_by_source.find(arguments[argument]->value);
      if(actual && actual->kind == TYPE_CLASS &&
         forwarded != state_->virtual_base_hidden_by_source.end()) {
        const vector<TypePtr> actual_bases = VirtualBaseTypes(actual);
        for(size_t actual_index = 0; actual_index < actual_bases.size(); ++actual_index)
          if(actual_bases[actual_index] &&
             PA12SameType(actual_bases[actual_index], target, true) &&
             actual_index < forwarded->second.size()) {
            *operand = forwarded->second[actual_index];
            if(!operand->empty() && (*operand)[0] == '$')
              *operand = emit_load(*operand, PointerTo(Fundamental("char")));
            return true;
          }
      }
    }
    if(!*null_pointer && *carrier && (*carrier)->kind == TYPE_CLASS) {
      TypePtr actual = expression_value_type(info);
      if(actual && actual->kind == TYPE_CLASS &&
         IsDerivedFrom(actual, *carrier) &&
         !PA12SameType(actual, *carrier, true)) {
        *carrier = actual;
        *projection_operand = EmitAddress(arguments[argument], scope);
        *complete_argument_projection = true;
      } else if(actual && actual->kind == TYPE_POINTER && actual->child &&
                type_value(actual->child)->kind == TYPE_CLASS &&
                IsDerivedFrom(type_value(actual->child), *carrier) &&
                !PA12SameType(type_value(actual->child), *carrier, true)) {
        *carrier = type_value(actual->child);
      }
    }
    return false;
}

void PA14Lowerer::ProjectVirtualBaseArgument(
  const TypePtr& carrier, const TypePtr& target,
  const string& projection_operand, bool complete_argument_projection,
  string* operand)
{
    if(!carrier || !operand) return;
    size_t offset = 0;
    if(!FindVirtualBaseOffset(carrier, target, &offset)) return;
    size_t virtual_index = 0;
    for(; virtual_index < carrier->virtual_base_types.size(); ++virtual_index)
      if(carrier->virtual_base_types[virtual_index] &&
         SameLayoutType(carrier->virtual_base_types[virtual_index], target))
        break;
    const bool construction_context = state_ && state_->record &&
      (state_->record->constructor || state_->record->construction_entry);
    if(carrier->polymorphic && !complete_argument_projection &&
       !construction_context && virtual_index < carrier->virtual_base_types.size()) {
      const string vptr = emit_load(projection_operand,
        PointerTo(Fundamental("char")));
      const string slot = new_temp();
      AddInstruction(slot + " = index i8 " + vptr + ", " +
        integer_text(-24 - static_cast<long long>(virtual_index * 8)));
      const string dynamic_offset = emit_load(slot,
        Fundamental("long int"));
      const string adjusted = new_temp();
      AddInstruction(adjusted + " = index i8 " + projection_operand + ", " +
        dynamic_offset);
      *operand = adjusted;
    } else {
      const string adjusted = new_temp();
      AddInstruction(adjusted +
        " = index i8 [projection=base_subobject] " + projection_operand + ", " +
        integer_text(static_cast<long long>(offset)));
      *operand = adjusted;
    }
}

void PA14Lowerer::AppendVirtualBaseCallArguments(
  FunctionRecord& function, vector<string>& operands,
  const vector<CPPGMAstNodePtr>& arguments, Scope* scope)
{
    const string deferred_constructor_virtual_base =
      "__deferred_constructor_virtual_base__";
    map<size_t, string> cached_source_operands;
    for(size_t hidden = 0; hidden < function.hidden_virtual_bases.size(); ++hidden) {
      if(hidden >= function.hidden_virtual_base_sources.size()) break;
      const size_t source = function.hidden_virtual_base_sources[hidden];
      if(source >= operands.size())
        throw logic_error("hidden virtual-base source has no operand");
      const TypePtr target = function.hidden_virtual_bases[hidden];
      if(TryCompleteConstructorVirtualBaseArgument(function, target, source,
                                                   operands, scope))
        continue;
      string operand = operands[source];
      bool overridden = false;
      bool deferred_constructor_virtual_base_pending = false;
      if(state_) {
        map<const Type*, string>::const_iterator pending =
          state_->pending_constructor_virtual_base_arguments.find(target.get());
        if(pending != state_->pending_constructor_virtual_base_arguments.end()) {
          if(pending->second != deferred_constructor_virtual_base) {
            operand = pending->second;
            overridden = true;
          } else deferred_constructor_virtual_base_pending = true;
        }
      }
      if(deferred_constructor_virtual_base_pending) {
        CPPGMAstNodePtr this_node(new CPPGMAstNode("keyword-literal", "this"));
        const string this_address = EmitValue(this_node, scope).operand;
        const TypePtr construction_owner = state_ && state_->record &&
          state_->record->member_owner ?
          type_value(state_->record->member_owner) : function.member_owner;
        operand = AdjustBaseAddress(this_address, construction_owner, target);
        overridden = true;
      }
      if(!overridden) {
        TypePtr carrier;
        string projection_operand;
        bool complete_argument_projection = false;
        bool null_pointer = false;
        if(TryForwardedVirtualBaseArgument(function, target, source, arguments,
                                            scope, &carrier, &operand,
                                            &projection_operand,
                                            &complete_argument_projection,
                                            &null_pointer)) {
          operands.push_back(operand);
          continue;
        }
        const TypePtr source_type = LowParameterSourceType(function, source);
        if(!overridden && source_type && type_is_reference(source_type) &&
           source_type->child && type_value(source_type->child) &&
           type_value(source_type->child)->kind == TYPE_POINTER) {
          map<size_t, string>::const_iterator cached =
            cached_source_operands.find(source);
          if(cached != cached_source_operands.end()) operand = cached->second;
          else {
            operand = emit_load(operand, PointerTo(Fundamental("char")));
            cached_source_operands[source] = operand;
          }
          projection_operand = operand;
        }
        if(!null_pointer)
          ProjectVirtualBaseArgument(carrier, target, projection_operand,
                                     complete_argument_projection, &operand);
      }
      operands.push_back(operand);
    }
  }

} // namespace cppgm_pa14_lowering
