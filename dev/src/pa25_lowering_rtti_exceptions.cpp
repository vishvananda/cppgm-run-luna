#include "pa14_lowering.h"

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace cppgm_pa14_lowering {

void PA14Lowerer::EmitExceptionRttiDeclarations(vector<string>& entries,
                                                bool* has_fundamental,
                                                bool* has_pointer,
                                                bool* has_class,
                                                bool* has_si)
{
  for(map<string, TypePtr>::const_iterator it = demanded_exception_types_.begin();
      it != demanded_exception_types_.end(); ++it) {
    const TypePtr type = RttiValueType(it->second);
    if(!type) continue;
    if(type->kind == TYPE_FUNDAMENTAL) *has_fundamental = true;
    else if(type->kind == TYPE_POINTER || type->kind == TYPE_ARRAY) *has_pointer = true;
    else if(type->kind == TYPE_CLASS || type->kind == TYPE_ENUM) {
      *has_class = true;
      if(type->kind == TYPE_CLASS && type->direct_base) *has_si = true;
    }
  }
  vector<TypePtr> external_types;
  for(map<string, TypePtr>::const_iterator it = demanded_exception_types_.begin();
      it != demanded_exception_types_.end(); ++it) {
    const TypePtr type = RttiValueType(it->second);
    if(type && type->kind == TYPE_FUNDAMENTAL) external_types.push_back(type);
  }
  sort(external_types.begin(), external_types.end(),
    [this](const TypePtr& left, const TypePtr& right) {
      return RttiMangledName(left) < RttiMangledName(right);
    });
  for(size_t i = 0; i < external_types.size(); ++i) {
    const TypePtr type = external_types[i];
    entries.push_back("declare global @__external_rtti__" +
      low_symbol_component(trim_type_name(type->name)) +
      " [binding=strong, object=_ZTI" + RttiMangledName(type) + "]");
  }
}

void PA14Lowerer::EmitExceptionObjects(vector<string>& entries)
{
  vector<TypePtr> types;
  for(map<string, TypePtr>::const_iterator it = demanded_thrown_types_.begin();
      it != demanded_thrown_types_.end(); ++it)
    types.push_back(RttiValueType(it->second));
  sort(types.begin(), types.end(),
    [this](const TypePtr& left, const TypePtr& right) {
      if(RttiMangledName(left) != RttiMangledName(right))
        return RttiMangledName(left) < RttiMangledName(right);
      return RttiInfoSymbol(left) < RttiInfoSymbol(right);
    });
  for(size_t i = 0; i < types.size(); ++i) {
    const TypePtr type = types[i];
    if(!type) continue;
    const string symbol = type->kind == TYPE_FUNDAMENTAL ?
      "__ehobj_" + low_symbol_component(trim_type_name(type->name)) :
      "__ehobj_" + low_symbol_component(RttiSymbol(type).substr(7));
    ostringstream object;
    object << "global @" << symbol << " [binding=weak, object=@" << symbol << "] = {\n";
    object << "  zero " << type_size(type) << "\n}";
    entries.push_back(object.str());
  }
}

} // namespace cppgm_pa14_lowering
