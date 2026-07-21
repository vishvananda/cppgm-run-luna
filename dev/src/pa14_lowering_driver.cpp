#include "pa14_lowering.h"

#include <algorithm>
#include <map>
#include <sstream>
#include <string>

using namespace std;

namespace cppgm_pa14_lowering {

void PA14Lowerer::Lower(ostream& out)
{
    for(size_t i = 0; i < trees_.size(); ++i) {
      if(!trees_[i] || trees_[i]->kind != "translation-unit")
        throw logic_error("invalid translation unit for LowIR");
      for(size_t j = 0; j < trees_[i]->children.size(); ++j)
        program_->children.push_back(trees_[i]->children[j]);
    }
    analyzer_.Analyze(program_);
    InstallBuiltins();
    CollectTopLevel(program_, analyzer_.global_.get());
    PreparePolymorphicModel();
    FinalizeSymbols();
    CollectStringLiterals(program_);

    vector<string> entries;
    EmitGlobals(entries);
    EmitPolymorphicGlobals(entries);
    MarkHiddenFriendDependencies();
    // Emit ordinary functions first so their calls establish the roots of
    // the demand-driven member-function set.  Member bodies can in turn
    // reach other member bodies, so walk that set to a fixed point.
    for(size_t i = 0; i < functions_.size(); ++i) {
      if(!functions_[i].definition || functions_[i].member ||
         (functions_[i].hidden_friend && !functions_[i].needed)) continue;
      entries.push_back(EmitFunction(functions_[i]));
      functions_[i].emitted = true;
    }
    // Ordinary roots can demand a hidden friend while they are emitted.  A
    // second demand pass keeps those friends out of the ordinary-function
    // root set while still preserving their transitive call dependencies.
    for(size_t i = 0; i < functions_.size(); ++i) {
      FunctionRecord& function = functions_[i];
      if(!function.definition || !function.hidden_friend || !function.needed ||
         function.emitted) continue;
      entries.push_back(EmitFunction(function));
      function.emitted = true;
    }
    const auto is_conversion_member = [&](const FunctionRecord& function) {
      if(!function.member || function.static_member || function.constructor ||
         function.destructor || !function.source_type) return false;
      const TypePtr source = function_target_type(function.source_type);
      if(!source || !source->parameters.empty()) return false;
      const string name = LastComponent(function.qualified_name);
      return name.size() > 8 && name.compare(0, 8, "operator") == 0;
    };
    const auto member_emission_order = [&]() {
      vector<size_t> order;
      map<const Type*, vector<size_t> > conversion_groups;
      for(size_t i = 0; i < functions_.size(); ++i) {
        order.push_back(i);
        if(is_conversion_member(functions_[i]))
          conversion_groups[functions_[i].member_owner.get()].push_back(i);
      }
      for(map<const Type*, vector<size_t> >::iterator group = conversion_groups.begin();
          group != conversion_groups.end(); ++group) {
        vector<size_t>& members = group->second;
        stable_sort(members.begin(), members.end(), [&](size_t left, size_t right) {
          const TypePtr left_type = function_target_type(functions_[left].source_type);
          const TypePtr right_type = function_target_type(functions_[right].source_type);
          if(left_type->function_const != right_type->function_const)
            return left_type->function_const;
          return left < right;
        });
        vector<size_t> positions;
        for(size_t position = 0; position < order.size(); ++position)
          for(size_t member = 0; member < members.size(); ++member)
            if(order[position] == members[member]) {
              positions.push_back(position);
              break;
            }
        for(size_t member = 0; member < positions.size(); ++member)
          order[positions[member]] = members[member];
      }
      return order;
    };
    bool added_member = true;
    while(added_member) {
      added_member = false;
      const vector<size_t> order = member_emission_order();
      for(size_t phase = 0; phase < 2; ++phase) {
        for(size_t order_index = 0; order_index < order.size(); ++order_index) {
          const size_t i = order[order_index];
          FunctionRecord& function = functions_[i];
          if(!function.definition || !function.member || !function.needed || function.emitted)
            continue;
          if((phase == 0) != function.base_entry) continue;
          entries.push_back(EmitFunction(function));
          function.emitted = true;
          added_member = true;
        }
      }
    }
    EmitDynamicInitializers(entries);
    added_member = true;
    while(added_member) {
      added_member = false;
      const vector<size_t> order = member_emission_order();
      for(size_t phase = 0; phase < 2; ++phase) {
        for(size_t order_index = 0; order_index < order.size(); ++order_index) {
          const size_t i = order[order_index];
          FunctionRecord& function = functions_[i];
          if(!function.definition || !function.member || !function.needed || function.emitted)
            continue;
          if((phase == 0) != function.base_entry) continue;
          entries.push_back(EmitFunction(function));
          function.emitted = true;
          added_member = true;
        }
      }
    }
    bool added_hidden = true;
    while(added_hidden) {
      added_hidden = false;
      for(size_t i = 0; i < functions_.size(); ++i) {
        FunctionRecord& function = functions_[i];
        if(!function.definition || !function.hidden_friend || !function.needed || function.emitted)
          continue;
        entries.push_back(EmitFunction(function));
        function.emitted = true;
        added_hidden = true;
      }
    }

    for(size_t i = 0; i < functions_.size(); ++i) {
      const FunctionRecord& function = functions_[i];
      if(!function.definition || !function.emitted || function.base_entry || !function.constructor ||
         function.deleted || !function.template_instantiation || function.object_name.empty())
        continue;
      string base_object = function.object_name;
      const size_t constructor = base_object.find("C1");
      if(constructor == string::npos) continue;
      base_object.replace(constructor, 2, "C2");
      entries.push_back("alias object " + base_object + " = @" + function.symbol);
    }
    vector<string> declarations;
    EmitDeclarations(declarations);
    entries.insert(entries.begin(), declarations.begin(), declarations.end());

    for(size_t i = 0; i < entries.size(); ++i) {
      if(i != 0) out << "\n";
      out << entries[i];
      if(entries[i].empty() || entries[i][entries[i].size() - 1] != '\n') out << "\n";
    }
  }

void PA14Lowerer::MarkHiddenFriendDependencies()
{
    for(size_t i = 0; i < functions_.size(); ++i) {
      FunctionRecord& function = functions_[i];
      if(!function.definition || !function.hidden_friend || !function.node) continue;
      map<const CPPGMAstNode*, Scope*>::const_iterator found =
        analyzer_.function_scopes_.find(function.node.get());
      Scope* scope = found == analyzer_.function_scopes_.end() ? function.scope : found->second;
      MarkHiddenFriendDependencyNodes(function.node, scope);
    }
  }

void PA14Lowerer::MarkHiddenFriendDependencyNodes(const CPPGMAstNodePtr& node,
                                                   Scope* scope)
{
    if(!node) return;
    if(node->kind == "compound-statement") {
      map<const CPPGMAstNode*, Scope*>::const_iterator compound =
        analyzer_.compound_scopes_.find(node.get());
      if(compound != analyzer_.compound_scopes_.end()) scope = compound->second;
    }
    if(node->kind == "binary-expression" && node->children.size() >= 2) {
      const string name = OperatorFunctionName(PA12Operator(node->value));
      if(!name.empty()) {
        vector<CPPGMAstNodePtr> arguments;
        arguments.push_back(node->children[0]);
        arguments.push_back(node->children[1]);
        (void)ChooseOperatorCall(name, arguments, scope);
      }
    } else if(node->kind == "unary-expression" && !node->children.empty()) {
      const string name = OperatorFunctionName(PA12Operator(node->value));
      if(!name.empty()) {
        vector<CPPGMAstNodePtr> arguments;
        arguments.push_back(node->children[0]);
        (void)ChooseOperatorCall(name, arguments, scope);
      }
    } else if(node->kind == "postfix-expression" && !node->children.empty()) {
      const string name = OperatorFunctionName(PA12Operator(node->value));
      if(!name.empty()) {
        vector<CPPGMAstNodePtr> arguments;
        arguments.push_back(node->children[0]);
        arguments.push_back(CPPGMAstNodePtr(new CPPGMAstNode("literal", "0")));
        (void)ChooseOperatorCall(name, arguments, scope);
      }
    } else if(node->kind == "assignment-expression" && node->children.size() >= 2) {
      const string name = OperatorFunctionName(PA12Operator(node->value));
      if(!name.empty()) {
        vector<CPPGMAstNodePtr> arguments;
        arguments.push_back(node->children[0]);
        arguments.push_back(node->children[1]);
        (void)ChooseOperatorCall(name, arguments, scope);
      }
    }
    for(size_t i = 0; i < node->children.size(); ++i)
      MarkHiddenFriendDependencyNodes(node->children[i], scope);
  }

} // namespace cppgm_pa14_lowering
