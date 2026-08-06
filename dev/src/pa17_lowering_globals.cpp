#include "pa14_lowering.h"

#include <functional>
#include <map>
#include <set>
#include <sstream>

using namespace std;

namespace cppgm_pa14_lowering {

namespace {

bool IsDestructorSlot(const VirtualMethodInfo& slot)
{
  return slot.destructor || (slot.name.size() > 1 && slot.name[0] == '~');
}

bool RttiNeedsTypeMangledClassName(const TypePtr& type)
{
  if(!type || type->kind != TYPE_CLASS || !type->template_specialization)
    return false;
  for(size_t argument = 0; argument < type->template_arguments.size(); ++argument)
    if(type->template_arguments[argument].find('[') != string::npos) return true;
  return false;
}

}

void PA14Lowerer::EmitPolymorphicGlobals(vector<string>& entries)
{
  bool changed = true;
  while(changed) {
    changed = false;
    vector<const Type*> current;
    current.insert(current.end(), emitted_vtables_.begin(), emitted_vtables_.end());
    current.insert(current.end(), external_vtables_.begin(), external_vtables_.end());
    for(size_t current_index = 0; current_index < current.size(); ++current_index) {
      const TypePtr owner = SemanticType(current[current_index]);
      if(!owner) continue;
      const vector<TypePtr> bases = DirectBaseTypes(owner);
      for(size_t base_index = 0; base_index < bases.size(); ++base_index) {
        const TypePtr base = bases[base_index];
        if(!base || !base->polymorphic) continue;
        if(ShouldUseExternalVtable(base)) {
          if(external_vtables_.insert(base.get()).second) changed = true;
        } else if(emitted_vtables_.insert(base.get()).second) changed = true;
      }
    }
  }
  for(set<const Type*>::const_iterator it = emitted_vtables_.begin(); it != emitted_vtables_.end(); ++it)
    EnsureRttiType(SemanticType(*it));
  for(set<const Type*>::const_iterator it = external_vtables_.begin(); it != external_vtables_.end(); ++it)
    EnsureRttiType(SemanticType(*it));
  if (emitted_vtables_.empty() && external_vtables_.empty() && demanded_rtti_types_.empty() &&
      demanded_exception_types_.empty() && !has_dynamic_cast_void_) return;
  bool has_class = false, has_fundamental = false, has_pointer = false;
  bool has_si = false, has_vmi = false;
  const auto class_uses_si_rtti = [&](const TypePtr& type) {
    const vector<TypePtr> bases = DirectBaseTypes(type);
    return type && type->kind == TYPE_CLASS && bases.size() == 1 &&
      !IsVirtualDirectBase(type, 0) &&
      !ShouldUseExternalVtable(type) &&
      (type->direct_base_offsets.empty() || type->direct_base_offsets[0] == 0);
  };
  for(map<string, TypePtr>::const_iterator it = demanded_rtti_types_.begin(); it != demanded_rtti_types_.end(); ++it) {
    const TypePtr type = RttiValueType(it->second);
    if(!type) continue;
    if(type->kind == TYPE_FUNDAMENTAL) has_fundamental = true;
    else if(type->kind == TYPE_POINTER || type->kind == TYPE_ARRAY) has_pointer = true;
    else if(type->kind == TYPE_CLASS || type->kind == TYPE_ENUM) {
      has_class = true;
      if(type->kind == TYPE_CLASS) {
        if(class_uses_si_rtti(type)) has_si = true;
        else if(!DirectBaseTypes(type).empty() && !ShouldUseExternalVtable(type))
          has_vmi = true;
      }
    }
  }
  EmitExceptionRttiDeclarations(entries, &has_fundamental, &has_pointer, &has_class, &has_si);
	if(has_dynamic_cast_void_) entries.push_back("declare global @__external_rtti__void [binding=strong, object=_ZTIv]");
  if(has_fundamental)
    entries.push_back("declare global @__external_rtti_vtable____fundamental_type_info [binding=strong, object=_ZTVN10__cxxabiv123__fundamental_type_infoE]");
  if(has_pointer)
    entries.push_back("declare global @__external_rtti_vtable____pointer_type_info [binding=strong, object=_ZTVN10__cxxabiv119__pointer_type_infoE]");
  if(has_class)
    entries.push_back("declare global @__external_rtti_vtable____class_type_info [binding=strong, object=_ZTVN10__cxxabiv117__class_type_infoE]");
  if(has_si)
    entries.push_back("declare global @__external_rtti_vtable____si_class_type_info [binding=strong, object=_ZTVN10__cxxabiv120__si_class_type_infoE]");
  if(has_vmi)
    entries.push_back("declare global @__external_rtti_vtable____vmi_class_type_info [binding=strong, object=_ZTVN10__cxxabiv121__vmi_class_type_infoE]");

  const vector<const Type*> emitted_types = OrderedTypes(emitted_vtables_), external_types = OrderedTypes(external_vtables_);
  for (size_t type_index = 0; type_index < external_types.size(); ++type_index) {
    const TypePtr type = SemanticType(external_types[type_index]);
    entries.push_back("declare global @__external_vtable__" + low_symbol_component(TypeQualifiedName(type)) + " [binding=strong, object=_ZTV" + TypeMangledName(type) + "]");
  }

  vector<TypePtr> ordered_rtti_types;
  for(map<string, TypePtr>::const_iterator it = demanded_rtti_types_.begin();
      it != demanded_rtti_types_.end(); ++it)
    ordered_rtti_types.push_back(RttiValueType(it->second));
  sort(ordered_rtti_types.begin(), ordered_rtti_types.end(),
    [this](const TypePtr& left, const TypePtr& right) {
      if(RttiMangledName(left) != RttiMangledName(right))
        return RttiMangledName(left) < RttiMangledName(right);
      return RttiInfoSymbol(left) < RttiInfoSymbol(right);
    });
  for (size_t type_index = 0; type_index < ordered_rtti_types.size(); ++type_index) {
    const TypePtr type = ordered_rtti_types[type_index];
    if(!type) continue;
    const string info_symbol = RttiInfoSymbol(type);
    const string rtti_symbol = RttiSymbol(type);
    const string mangled = RttiMangledName(type);
    ostringstream name;
    name << "global @" << info_symbol << " [storage=readonly, binding=weak, object=_ZTS" << mangled << "] = {\n";
    for (size_t i = 0; i < mangled.size(); ++i)
      name << "  i8 " << static_cast<unsigned int>(static_cast<unsigned char>(mangled[i])) << "\n";
    name << "  i8 0\n}";
    entries.push_back(name.str());
    ostringstream rtti;
    rtti << "global @" << rtti_symbol <<
      " [storage=readonly, binding=weak, object=_ZTI" << mangled << "] = {\n";
    if(type->kind == TYPE_FUNDAMENTAL)
      rtti << "  ptr addr @__external_rtti_vtable____fundamental_type_info + 16\n";
    else if(type->kind == TYPE_POINTER || type->kind == TYPE_ARRAY ||
            type->kind == TYPE_FUNCTION || type->kind == TYPE_MEMBER_POINTER)
      rtti << "  ptr addr @__external_rtti_vtable____pointer_type_info + 16\n";
    else if (class_uses_si_rtti(type))
      rtti << "  ptr addr @__external_rtti_vtable____si_class_type_info + 16\n";
    else if (type->kind == TYPE_CLASS && !DirectBaseTypes(type).empty() &&
             !ShouldUseExternalVtable(type))
      rtti << "  ptr addr @__external_rtti_vtable____vmi_class_type_info + 16\n";
    else
      rtti << "  ptr addr @__external_rtti_vtable____class_type_info + 16\n";
    rtti << "  ptr addr @" << info_symbol << "\n";
    if(type->kind == TYPE_POINTER || type->kind == TYPE_ARRAY ||
       type->kind == TYPE_FUNCTION || type->kind == TYPE_MEMBER_POINTER) {
      const bool incomplete_pointee = type->child &&
        RttiNeedsTypeMangledClassName(RttiValueType(type->child));
      rtti << "  i32 " << (incomplete_pointee ? 8 : 0) << "\n";
      rtti << "  ptr addr @" << RttiSymbol(type->child) << "\n";
    } else if (type->kind == TYPE_CLASS && !DirectBaseTypes(type).empty() &&
               !ShouldUseExternalVtable(type)) {
      const vector<TypePtr> bases = DirectBaseTypes(type);
      if (class_uses_si_rtti(type)) {
        rtti << "  ptr addr @" << RttiSymbol(bases[0]) << "\n";
      } else {
        rtti << "  i32 0\n";
        rtti << "  i32 " << static_cast<unsigned long long>(bases.size()) << "\n";
        for(size_t base = 0; base < bases.size(); ++base) {
          const size_t offset = base < type->direct_base_offsets.size() ?
            type->direct_base_offsets[base] : 0;
          long long flags;
          if(IsVirtualDirectBase(type, base)) {
            // Virtual-base RTTI edges encode the vtable slot used to find
            // the base view.  The supported ABI places the first such slot
            // at -24 from the address point.
            flags = (static_cast<long long>(-24) << 8) | 3;
          } else {
            const bool public_edge = base < type->direct_base_access.size() &&
              type->direct_base_access[base] == "public";
            flags = static_cast<long long>(offset << 8) | (public_edge ? 2 : 0);
          }
          rtti << "  ptr addr @" << RttiSymbol(bases[base]) << "\n";
          rtti << "  i64 " << flags << "\n";
        }
      }
    }
    rtti << "}";
    entries.push_back(rtti.str());
  }
  for (size_t type_index = 0; type_index < emitted_types.size(); ++type_index) {
    const TypePtr type = SemanticType(emitted_types[type_index]);
    const string mangled = TypeMangledName(type);
    bool strong = false;
    for (size_t i = 0; i < functions_.size(); ++i) {
      const FunctionRecord& function = functions_[i];
      if (!function.definition || type_value(function.member_owner) != type ||
          !function.member || !IsVirtualFunction(function)) continue;
      if (function.node && FirstIdentifier(function.node).find("::") != string::npos)
        strong = true;
    }
    ostringstream table;
    table << "global @" << VTableSymbol(type) <<
      " [storage=readonly, binding=" << (strong ? "strong" : "weak") <<
      ", object=_ZTV" << mangled << "] = {\n";
    if(HasVirtualBases(type) && !type->virtual_base_offsets.empty())
      table << "  i64 " << static_cast<unsigned long long>(type->virtual_base_offsets[0]) << "\n";
    table << "  i64 0\n  ptr addr @" << RttiSymbol(type) << "\n";
    for (size_t i = 0; i < type->virtual_methods.size(); ++i) {
      const VirtualMethodInfo& slot = type->virtual_methods[i];
      if (slot.pure) {
        FunctionRecord* pure = EnsurePureVirtual(slot);
        table << "  ptr addr @" << (pure ? pure->symbol : "__cxa_pure_virtual") << "\n";
        continue;
      }
      if (IsDestructorSlot(slot)) {
        FunctionRecord* complete = EnsureVirtualDestructor(type, slot, false);
        FunctionRecord* deleting = EnsureVirtualDestructor(type, slot, true);
        table << "  ptr addr @" << (complete ? complete->symbol : "__missing_destructor") << "\n";
        table << "  ptr addr @" << (deleting ? deleting->symbol : "__missing_deleting_destructor") << "\n";
        continue;
      }
      FunctionRecord* function = VirtualFunctionRecord(type, slot);
      if (!function) {
        table << "  ptr addr @__cxa_pure_virtual\n";
      } else {
        MarkFunctionNeeded(function);
        table << "  ptr addr @" << function->symbol << "\n";
      }
    }
    table << "}";
    entries.push_back(table.str());
  }
  // Virtual-base construction uses a VTT rather than the final most-derived
  // tables.  Keep these entries as typed ABI globals: a base-entry constructor
  // receives the address of its VTT element, installs the construction primary
  // view, and then installs each nested virtual-base view from the following
  // element.
  set<string> emitted_view_adjustors;
  const auto construction_view_adjustor = [&](FunctionRecord* target,
                                               long long adjustment) -> string {
    if(!target || adjustment == 0) return target ? target->symbol : string();
    const string direction = adjustment < 0 ? "neg" : "pos";
    const long long magnitude = adjustment < 0 ? -adjustment : adjustment;
    const string symbol = "_" + target->symbol +
      "__vtable_return_adjust__this_" + direction +
      integer_text(magnitude) + "__return_pos0";
    if(emitted_view_adjustors.insert(symbol).second) {
      ostringstream thunk;
      const TypePtr function_type = target->type;
      const TypePtr result_type = function_type ? function_type->child : TypePtr();
      thunk << "function @" << symbol << "(";
      if(function_type)
        for(size_t parameter = 0; parameter < function_type->parameters.size(); ++parameter) {
          if(parameter != 0) thunk << ", ";
          thunk << "%arg" << static_cast<unsigned long long>(parameter) << " : " <<
            low_type(function_type->parameters[parameter]);
        }
      thunk << ") -> " << low_type(result_type) << " [binding=weak] {\n";
      thunk << "  block ^entry:\n";
      thunk << "    %t1 = index i8 %arg0, " << integer_text(adjustment) << "\n";
      if(result_type && low_type(result_type) != "void") {
        thunk << "    %t2 = call " << low_type(result_type) << " @" << target->symbol <<
          "(%t1";
        if(function_type)
          for(size_t parameter = 1; parameter < function_type->parameters.size(); ++parameter)
            thunk << ", %arg" << static_cast<unsigned long long>(parameter);
        thunk << ")\n    return " << low_type(result_type) << " %t2\n";
      } else {
        thunk << "    call void @" << target->symbol << "(%t1";
        if(function_type)
          for(size_t parameter = 1; parameter < function_type->parameters.size(); ++parameter)
            thunk << ", %arg" << static_cast<unsigned long long>(parameter);
        thunk << ")\n    return void\n";
      }
      thunk << "}";
      entries.push_back(thunk.str());
    }
    MarkFunctionNeeded(target);
    return symbol;
  };
  const auto append_construction_methods = [&](ostringstream& table,
                                                const TypePtr& dispatch_owner,
                                                const vector<VirtualMethodInfo>& methods,
                                                long long adjustment) {
    for(size_t method_index = 0; method_index < methods.size(); ++method_index) {
      const VirtualMethodInfo& method = methods[method_index];
      if(method.pure) {
        FunctionRecord* pure = EnsurePureVirtual(method);
        table << "  ptr addr @" << (pure ? pure->symbol : "__cxa_pure_virtual") << "\n";
      } else if(IsDestructorSlot(method)) {
        FunctionRecord* complete = EnsureVirtualDestructor(dispatch_owner, method, false);
        FunctionRecord* deleting = EnsureVirtualDestructor(dispatch_owner, method, true);
        table << "  ptr addr @" << (complete ?
          construction_view_adjustor(complete, adjustment) : "__missing_destructor") << "\n";
        table << "  ptr addr @" << (deleting ?
          construction_view_adjustor(deleting, adjustment) : "__missing_deleting_destructor") << "\n";
      } else {
        FunctionRecord* function = VirtualFunctionRecord(dispatch_owner, method);
        table << "  ptr addr @" << (function ? function->symbol : "__cxa_pure_virtual") << "\n";
      }
    }
  };
  const auto construction_symbol = [&](const TypePtr& owner, const TypePtr& base,
                                        size_t base_index, size_t stage) {
    return low_symbol_component(TypeQualifiedName(owner)) + "____construction__" +
      low_symbol_component(TypeQualifiedName(base)) + "__" +
      integer_text(static_cast<long long>(base_index)) + "__s" +
      integer_text(static_cast<long long>(stage)) + "__vtable";
  };
  for(size_t type_index = 0; type_index < emitted_types.size(); ++type_index) {
    const TypePtr owner = SemanticType(emitted_types[type_index]);
    if(!owner || !HasVirtualBases(owner)) continue;
    const vector<TypePtr> direct_bases = DirectBaseTypes(owner);
    const auto child_physical_offset = [&](const TypePtr& parent,
                                            size_t parent_offset,
                                            size_t child_index) {
      const TypePtr child = child_index < DirectBaseTypes(parent).size() ?
        type_value(DirectBaseTypes(parent)[child_index]) : TypePtr();
      const size_t edge = child_index < parent->direct_base_offsets.size() ?
        parent->direct_base_offsets[child_index] : 0;
      size_t virtual_offset = 0;
      if(child && IsVirtualDirectBase(parent, child_index) &&
         FindVirtualBaseOffset(owner, child, &virtual_offset))
        return virtual_offset;
      return parent_offset + edge;
    };
    function<void(const TypePtr&, size_t)> emit_construction_group;
    emit_construction_group = [&](const TypePtr& base, size_t physical_offset) {
      if(!base) return;
      size_t virtual_offset = 0;
      if(!base->virtual_base_types.empty())
        FindVirtualBaseOffset(owner, base->virtual_base_types[0], &virtual_offset);
      const size_t construction_virtual_offset = physical_offset == 0 ?
        virtual_offset : (!base->virtual_base_offsets.empty() ?
          base->virtual_base_offsets[0] : 0);
      ostringstream primary;
      primary << "global @" << construction_symbol(owner, base, physical_offset, 0) <<
        " [storage=readonly, binding=weak, object=@" <<
        construction_symbol(owner, base, physical_offset, 0) << "] = {\n";
      primary << "  i64 " << static_cast<unsigned long long>(construction_virtual_offset) << "\n";
      primary << "  i64 " << (physical_offset == 0 ? 0 :
        -static_cast<long long>(physical_offset)) << "\n  ptr addr @" <<
        RttiSymbol(base) << "\n";
      append_construction_methods(primary, base, base->virtual_methods, 0);
      primary << "}";
      entries.push_back(primary.str());

      const vector<TypePtr> nested_bases = DirectBaseTypes(base);
      for(size_t nested_index = 0; nested_index < nested_bases.size(); ++nested_index) {
        const TypePtr nested = type_value(nested_bases[nested_index]);
        if(!nested || !nested->polymorphic || !HasVirtualBases(nested)) continue;
        emit_construction_group(nested,
          child_physical_offset(base, physical_offset, nested_index));
      }

      const vector<RenderedVirtualTableView> base_views = RenderedVirtualTableViews(base);
      for(size_t view_index = 0; view_index < base_views.size(); ++view_index) {
        const RenderedVirtualTableView& view = base_views[view_index];
        if(!view.base) continue;
        size_t view_physical_offset = 0;
        if(!FindVirtualBaseOffset(owner, view.base, &view_physical_offset))
          view_physical_offset = physical_offset + view.offset;
        const string symbol = construction_symbol(owner, base, physical_offset,
          view_index + 1);
        ostringstream nested;
        nested << "global @" << symbol <<
          " [storage=readonly, binding=weak, object=@" << symbol << "] = {\n";
        size_t base_virtual_offset = 0;
        const bool virtual_view = FindVirtualBaseOffset(base, view.base,
          &base_virtual_offset);
        nested << "  i64 " << (virtual_view ? 0 : view_physical_offset) <<
          "\n  i64 -" << static_cast<unsigned long long>(view_physical_offset) << "\n";
        nested << "  ptr addr @" << RttiSymbol(view.base) << "\n";
        const long long view_adjustment = -static_cast<long long>(virtual_view ?
          base_virtual_offset : view.offset);
        append_construction_methods(nested, base, view.methods, view_adjustment);
        nested << "}";
        entries.push_back(nested.str());
      }
    };
    for(size_t base_index = 0; base_index < direct_bases.size(); ++base_index) {
      const TypePtr base = type_value(direct_bases[base_index]);
      if(!base || !base->polymorphic || !HasVirtualBases(base)) continue;
      const size_t physical_offset = base_index < owner->direct_base_offsets.size() ?
        owner->direct_base_offsets[base_index] : 0;
      emit_construction_group(base, physical_offset);
    }

    ostringstream vtt;
    vtt << "global @" << VttSymbol(owner) << " [object_root=yes] = {\n";
    vtt << "  ptr addr @" << VTableAddressSymbol(owner) << " + " <<
      (HasVirtualBases(owner) ? 24 : 16) << "\n";
    function<void(const TypePtr&, size_t)> append_vtt_group;
    append_vtt_group = [&](const TypePtr& base, size_t physical_offset) {
      vtt << "  ptr addr @" << construction_symbol(owner, base, physical_offset, 0) <<
        " + " << (HasVirtualBases(base) ? 24 : 16) << "\n";
      const vector<TypePtr> nested_bases = DirectBaseTypes(base);
      for(size_t nested_index = 0; nested_index < nested_bases.size(); ++nested_index) {
        const TypePtr nested = type_value(nested_bases[nested_index]);
        if(!nested || !nested->polymorphic || !HasVirtualBases(nested)) continue;
        append_vtt_group(nested,
          child_physical_offset(base, physical_offset, nested_index));
      }
      const vector<RenderedVirtualTableView> base_views = RenderedVirtualTableViews(base);
      for(size_t view_index = 0; view_index < base_views.size(); ++view_index)
        vtt << "  ptr addr @" << construction_symbol(owner, base, physical_offset,
          view_index + 1) << " + " << (HasVirtualBases(base) ? 24 : 16) << "\n";
    };
    for(size_t base_index = 0; base_index < direct_bases.size(); ++base_index) {
      const TypePtr base = type_value(direct_bases[base_index]);
      if(!base || !base->polymorphic || !HasVirtualBases(base)) continue;
      const size_t physical_offset = base_index < owner->direct_base_offsets.size() ?
        owner->direct_base_offsets[base_index] : 0;
      append_vtt_group(base, physical_offset);
    }
    const vector<RenderedVirtualTableView> owner_views = RenderedVirtualTableViews(owner);
    for(size_t view_index = 0; view_index < owner_views.size(); ++view_index)
      vtt << "  ptr addr @" << VTableViewSymbol(owner, owner_views[view_index].base,
        owner_views[view_index].offset) << " + " <<
        (HasVirtualBases(owner) ? 24 : 16) << "\n";
    vtt << "}";
    entries.push_back(vtt.str());
  }
  const auto subobject_offsets = [&](const TypePtr& raw_target,
                                     const TypePtr& raw_owner) {
    vector<size_t> result_offsets;
    const TypePtr target = type_value(raw_target);
    const TypePtr owner = type_value(raw_owner);
    if(!target || !owner) return result_offsets;
    size_t virtual_target_offset = 0;
    if(FindVirtualBaseOffset(owner, target, &virtual_target_offset)) {
      result_offsets.push_back(virtual_target_offset);
      return result_offsets;
    }
    set<pair<const Type*, size_t> > visited;
    function<void(const TypePtr&, size_t)> collect;
    collect = [&](const TypePtr& current, size_t offset) {
      if(!current || !visited.insert(make_pair(current.get(), offset)).second) return;
      if(SameLayoutType(current, target)) {
        result_offsets.push_back(offset);
        return;
      }
      const vector<TypePtr> bases = DirectBaseTypes(current);
      for(size_t base = 0; base < bases.size(); ++base) {
        if(!bases[base]) continue;
        const size_t edge = base < current->direct_base_offsets.size() ?
          current->direct_base_offsets[base] : (base == 0 ? current->direct_base_offset : 0);
        collect(bases[base], offset + edge);
      }
    };
    collect(owner, 0);
    return result_offsets;
  };
  const auto view_adjustor = [&](FunctionRecord* target, long long adjustment) -> string {
    if(!target || adjustment == 0) return target ? target->symbol : string();
    const string direction = adjustment < 0 ? "neg" : "pos";
    const long long magnitude = adjustment < 0 ? -adjustment : adjustment;
    const string symbol = "_" + target->symbol +
      "__vtable_return_adjust__this_" + direction +
      integer_text(magnitude) + "__return_pos0";
    if(emitted_view_adjustors.insert(symbol).second) {
      ostringstream thunk;
      const TypePtr function_type = target->type;
      const TypePtr result_type = function_type ? function_type->child : TypePtr();
      thunk << "function @" << symbol << "(";
      if(function_type)
        for(size_t parameter = 0; parameter < function_type->parameters.size(); ++parameter) {
          if(parameter != 0) thunk << ", ";
          thunk << "%arg" << static_cast<unsigned long long>(parameter) << " : " <<
            low_type(function_type->parameters[parameter]);
        }
      thunk << ") -> " << low_type(result_type) << " [binding=weak] {\n";
      thunk << "  block ^entry:\n";
      thunk << "    %t1 = index i8 %arg0, " <<
        integer_text(adjustment) << "\n";
      if(result_type && low_type(result_type) != "void") {
        thunk << "    %t2 = call " << low_type(result_type) << " @" << target->symbol << "(%t1";
        if(function_type)
          for(size_t parameter = 1; parameter < function_type->parameters.size(); ++parameter)
            thunk << ", %arg" << static_cast<unsigned long long>(parameter);
        thunk << ")\n";
        thunk << "    return " << low_type(result_type) << " %t2\n";
      } else {
        thunk << "    call void @" << target->symbol << "(%t1";
        if(function_type)
          for(size_t parameter = 1; parameter < function_type->parameters.size(); ++parameter)
            thunk << ", %arg" << static_cast<unsigned long long>(parameter);
        thunk << ")\n";
        thunk << "    return void\n";
      }
      thunk << "}";
      entries.push_back(thunk.str());
    }
    MarkFunctionNeeded(target);
    return symbol;
  };
  for(size_t type_index = 0; type_index < emitted_types.size(); ++type_index) {
    const TypePtr owner = SemanticType(emitted_types[type_index]);
    if(!owner) continue;
    const vector<RenderedVirtualTableView> views = RenderedVirtualTableViews(owner);
    for(size_t view_index = 0; view_index < views.size(); ++view_index) {
      const RenderedVirtualTableView& view = views[view_index];
      if(!view.base || view.offset == 0) continue;
      const size_t offset = view.offset;
      ostringstream table;
      table << "global @" << VTableViewSymbol(owner, view.base, offset) <<
        " [storage=readonly, binding=weak, object=@" <<
        VTableViewSymbol(owner, view.base, offset) << "] = {\n";
      if(HasVirtualBases(owner)) {
        size_t virtual_offset = 0;
        const bool virtual_view = FindVirtualBaseOffset(owner, view.base,
          &virtual_offset);
        table << "  i64 " << (virtual_view ? 0 : offset) << "\n";
      }
      table << "  i64 -" << static_cast<unsigned long long>(offset) << "\n";
      table << "  ptr addr @" << RttiSymbol(owner) << "\n";
      for(size_t slot_index = 0; slot_index < view.methods.size(); ++slot_index) {
        const VirtualMethodInfo& slot = view.methods[slot_index];
        FunctionRecord* target = 0;
        if(IsDestructorSlot(slot)) {
          target = EnsureVirtualDestructor(owner, slot, false);
        } else if(!slot.pure) {
          target = VirtualFunctionRecord(owner, slot);
        }
        long long adjustment = 0;
        if(target && target->member_owner) {
          const vector<size_t> target_offsets = subobject_offsets(
            type_value(target->member_owner), owner);
          size_t target_offset = target_offsets.empty() ? 0 : target_offsets[0];
          for(size_t target_index = 0; target_index < target_offsets.size(); ++target_index)
            if(target_offsets[target_index] == offset) {
              target_offset = offset;
              break;
            }
          adjustment = static_cast<long long>(target_offset) -
            static_cast<long long>(offset);
        }
        if(slot.pure) {
          FunctionRecord* pure = EnsurePureVirtual(slot);
          table << "  ptr addr @" << (pure ? pure->symbol : "__cxa_pure_virtual") << "\n";
          continue;
        }
        if(IsDestructorSlot(slot)) {
          FunctionRecord* complete = target;
          FunctionRecord* deleting = EnsureVirtualDestructor(owner, slot, true);
          table << "  ptr addr @" << view_adjustor(complete, adjustment) << "\n";
          long long deleting_adjustment = adjustment;
          if(deleting && deleting->member_owner) {
            const vector<size_t> deleting_offsets = subobject_offsets(
              type_value(deleting->member_owner), owner);
            size_t deleting_offset = deleting_offsets.empty() ? 0 : deleting_offsets[0];
            for(size_t deleting_index = 0; deleting_index < deleting_offsets.size(); ++deleting_index)
              if(deleting_offsets[deleting_index] == offset) {
                deleting_offset = offset;
                break;
              }
            deleting_adjustment = static_cast<long long>(deleting_offset) -
              static_cast<long long>(offset);
          }
          table << "  ptr addr @" << view_adjustor(deleting, deleting_adjustment) << "\n";
          continue;
        }
        FunctionRecord* function = target;
        if(!function) {
          table << "  ptr addr @__cxa_pure_virtual\n";
        } else {
          table << "  ptr addr @" << view_adjustor(function, adjustment) << "\n";
        }
      }
      table << "}";
      entries.push_back(table.str());
    }
  }
  EmitExceptionObjects(entries);
}

} // namespace cppgm_pa14_lowering
