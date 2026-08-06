#include "pa14_lowering.h"

namespace cppgm_pa14_lowering {

vector<string> PA14Lowerer::ParameterNames(const FunctionRecord& function) const
{
    vector<string> result;
    const size_t hidden_count = function.hidden_virtual_bases.size();
    const size_t ordinary_count = function.type &&
      function.type->parameters.size() >= hidden_count ?
      function.type->parameters.size() - hidden_count : 0;
    if(function.indirect_result) result.push_back("ret");
    if(function.member && !function.static_member) result.push_back("this");
    if(function.vtt_parameter) result.push_back("__vtt");
    CPPGMAstNodePtr declarator;
    if(function.node) declarator = function.constructor || function.destructor ||
      function.value_special_member ?
      ChildOfKind(function.node, "declarator") :
      (function.node->children.size() > 1 ? function.node->children[1] : CPPGMAstNodePtr());
    CPPGMAstNodePtr clause = declarator ? DescendantOfKind(declarator, "parameter-clause") :
      CPPGMAstNodePtr();
    CPPGMAstNodePtr declaration_clause;
    if(function.member && function.member_owner && function.source_type) {
      const TypePtr source = function_target_type(function.source_type);
      const vector<Binding*> candidates = MemberBindings(function.member_owner,
        LastComponent(function.qualified_name));
      for(size_t candidate = 0; candidate < candidates.size() && !declaration_clause;
          ++candidate) {
        Binding* binding = candidates[candidate];
        TypePtr binding_type = binding ? function_target_type(binding->type) : TypePtr();
        if(!binding || binding->kind != BIND_FUNCTION || !binding->declaration ||
           !binding_type || !PA12SameType(binding_type, source, false)) continue;
        CPPGMAstNodePtr binding_declarator;
        if(binding->declaration->kind == "function-definition" &&
           binding->declaration->children.size() > 1)
          binding_declarator = binding->declaration->children[1];
        else if(binding->declaration->kind == "simple-declaration") {
          const CPPGMAstNodePtr list = ChildOfKind(binding->declaration,
            "init-declarator-list");
          if(list) for(size_t item = 0; item < list->children.size(); ++item)
            if(list->children[item] && !list->children[item]->children.empty() &&
               LastComponent(declarator_name(list->children[item]->children[0])) ==
                 LastComponent(function.qualified_name)) {
              binding_declarator = list->children[item]->children[0];
              break;
            }
        }
        declaration_clause = binding_declarator ?
          DescendantOfKind(binding_declarator, "parameter-clause") :
          CPPGMAstNodePtr();
      }
    }
    size_t index = (function.member && !function.static_member ? 1 : 0) +
      (function.vtt_parameter ? 1 : 0);
    if(clause) {
      for(size_t i = 0; i < clause->children.size(); ++i) {
        CPPGMAstNodePtr parameter = clause->children[i];
        if(!parameter || parameter->kind != "parameter-declaration") continue;
        CPPGMAstNodePtr declarator = parameter->children.size() > 1 ? parameter->children[1] : CPPGMAstNodePtr();
        const size_t parameter_index = index++;
        string name = parameter_name(declarator, parameter_index);
        if(name.find("__param") == 0 && declaration_clause &&
           i < declaration_clause->children.size()) {
          const CPPGMAstNodePtr declaration_parameter = declaration_clause->children[i];
          const CPPGMAstNodePtr declaration_declarator = declaration_parameter &&
            declaration_parameter->children.size() > 1 ?
            declaration_parameter->children[1] : CPPGMAstNodePtr();
          const string declared_name = parameter_name(declaration_declarator,
            parameter_index);
          if(declared_name.find("__param") != 0) name = declared_name;
        }
        if(function.builtin && name.find("__param") == 0)
          name = "arg" + integer_text(static_cast<long long>(parameter_index));
        result.push_back(name);
      }
    }
    while(result.size() < ordinary_count) {
      const size_t parameter_index = index++;
      result.push_back((function.builtin ? "arg" : "__param") +
        integer_text(static_cast<long long>(parameter_index)));
    }
    map<string, size_t> hidden_ordinals;
    const bool reference_copy_like = function.source_type &&
      function.source_type->parameters.size() == 1 &&
      function.source_type->parameters[0] &&
      type_is_reference(function.source_type->parameters[0]) &&
      function.member_owner &&
      (PA12SameType(type_value(function.source_type->parameters[0]->child),
                    type_value(function.member_owner), true) ||
       LastComponent(TypeQualifiedName(type_value(function.source_type->parameters[0]->child))) ==
       LastComponent(TypeQualifiedName(function.member_owner)));
    for(size_t hidden = 0; hidden < hidden_count; ++hidden) {
      const size_t source = hidden < function.hidden_virtual_base_sources.size() ?
        function.hidden_virtual_base_sources[hidden] : 0;
      const bool this_source = function.member && !function.static_member &&
        source == (function.indirect_result ? 1 : 0);
      const string prefix = (this_source ||
        (function.base_entry && function.constructor && reference_copy_like)) ?
        "__vbptr" : "__pvbptr";
      const size_t ordinal = hidden_ordinals[prefix]++;
      result.push_back(prefix +
        integer_text(static_cast<long long>(ordinal)));
    }
    return result;
  }

} // namespace cppgm_pa14_lowering
