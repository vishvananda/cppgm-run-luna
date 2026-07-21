#include "pa14_lowering.h"
#include <iomanip>

using namespace std;

namespace cppgm_pa14_lowering {

bool PA14Lowerer::AppendConstantGlobalData(const TypePtr& raw_type,
                                           const ConstantValue& value,
                                           vector<GlobalDataItem>& items) const
{
    TypePtr type = type_value(raw_type);
    if(!type) return false;
    if(type->kind == TYPE_ARRAY) {
      if(!value.object) return false;
      const size_t count = type->bound > 0 || value.object->elements.empty() ?
        (type->bound >= 0 ? static_cast<size_t>(type->bound) :
          value.object->elements.size()) : value.object->elements.size();
      for(size_t i = 0; i < count; ++i) {
        ConstantValue element;
        if(i < value.object->elements.size()) element = value.object->elements[i];
        if(!AppendConstantGlobalData(type->child, element, items))
          items.push_back(GlobalDataItem("zero " +
            integer_text(static_cast<long long>(type_size(type->child)))));
      }
      if(count == 0 && type_size(type) != 0)
        items.push_back(GlobalDataItem("zero " +
          integer_text(static_cast<long long>(type_size(type)))));
      return true;
    }
    if(type->kind == TYPE_CLASS) {
      if(!value.object) return false;
      const size_t start = items.size();
      if(type->class_members.empty()) {
        for(map<string, ConstantValue>::const_iterator member =
              value.object->members.begin(); member != value.object->members.end(); ++member)
          if(!AppendConstantGlobalData(member->second.type, member->second, items))
            items.push_back(GlobalDataItem("zero " +
              integer_text(static_cast<long long>(type_size(member->second.type)))));
        if(items.size() == start && type_size(type) != 0)
          items.push_back(GlobalDataItem("zero " +
            integer_text(static_cast<long long>(type_size(type)))));
        return true;
      }
      for(size_t i = 0; i < type->class_members.size(); ++i) {
        const ClassMemberInfo& member = type->class_members[i];
        if(member.is_static || member.name.empty()) continue;
        map<string, ConstantValue>::const_iterator found =
          value.object->members.find(member.name);
        const ConstantValue member_value = found == value.object->members.end() ?
          ConstantValue() : found->second;
        if(!AppendConstantGlobalData(member.type, member_value, items))
          items.push_back(GlobalDataItem("zero " +
            integer_text(static_cast<long long>(type_size(member.type)))));
      }
      if(items.size() == start && type_size(type) != 0)
        items.push_back(GlobalDataItem("zero " +
          integer_text(static_cast<long long>(type_size(type)))));
      return true;
    }
    if(value.integral.known) {
      items.push_back(GlobalDataItem(low_type(type) + " " +
        integer_text(PA19Signed(value.integral))));
      return true;
    }
    if(value.floating_known) {
      ostringstream text;
      text << setprecision(18) << value.floating;
      if(type->kind == TYPE_FUNDAMENTAL) {
        if(type->name == "float") text << "f";
        else if(type->name == "long double") text << "L";
      }
      items.push_back(GlobalDataItem(low_type(type) + " " + text.str()));
      return true;
    }
    if(value.kind == ConstantValue::CONSTANT_POINTER ||
       type->kind == TYPE_POINTER || type->kind == TYPE_FUNCTION) {
      items.push_back(GlobalDataItem("zero " +
        integer_text(static_cast<long long>(type_size(type)))));
      return true;
    }
    items.push_back(GlobalDataItem("zero " +
      integer_text(static_cast<long long>(type_size(type)))));
    return true;
  }

} // namespace cppgm_pa14_lowering
