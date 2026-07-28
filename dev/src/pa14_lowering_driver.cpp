#include "pa14_lowering.h"

#include <algorithm>
#include <map>
#include <sstream>
#include <string>

using namespace std;

namespace cppgm_pa14_lowering {

void PA14Lowerer::IndexFriendOwners()
{
  friend_owner_index_.clear();
  for(map<const CPPGMAstNode*, TypePtr>::const_iterator it = analyzer_.class_types_.begin();
      it != analyzer_.class_types_.end(); ++it) {
    const TypePtr friend_owner = type_value(it->second);
    if(!friend_owner || friend_owner->kind != TYPE_CLASS ||
       friend_owner->friend_access.empty()) continue;
    for(TypePtr owner = friend_owner; owner; owner = type_value(owner->direct_base)) {
      vector<TypePtr>& entries = friend_owner_index_[owner.get()];
      if(find(entries.begin(), entries.end(), friend_owner) == entries.end())
        entries.push_back(friend_owner);
    }
  }
}

void PA14Lowerer::IndexClassTypesByName()
{
  class_types_by_name_.clear();
  for(map<const CPPGMAstNode*, TypePtr>::const_iterator it = analyzer_.class_types_.begin();
      it != analyzer_.class_types_.end(); ++it) {
    const TypePtr value = type_value(it->second);
    if(!value || value->kind != TYPE_CLASS) continue;
    const string names[] = {LastComponent(value->name),
      LastComponent(value->template_primary)};
    for(size_t name = 0; name < sizeof(names) / sizeof(*names); ++name) {
      if(names[name].empty()) continue;
      vector<TypePtr>& entries = class_types_by_name_[names[name]];
      if(find(entries.begin(), entries.end(), value) == entries.end())
        entries.push_back(value);
    }
  }
}

void PA14Lowerer::PrepareLoweringProgram()
{
  for(size_t i = 0; i < trees_.size(); ++i) {
    if(!trees_[i] || trees_[i]->kind != "translation-unit")
      throw logic_error("invalid translation unit for LowIR");
    for(size_t j = 0; j < trees_[i]->children.size(); ++j)
      program_->children.push_back(trees_[i]->children[j]);
  }
  analyzer_.Analyze(program_);
  IndexClassTypesByName();
  complete_template_object_uses_.clear();
  IndexCompleteTemplateObjectUses(program_);
  IndexFriendOwners();
  InstallBuiltins();
  CollectTopLevel(program_, analyzer_.global_.get());
  PreparePolymorphicModel();
  FinalizeSymbols();
  CollectStringLiterals(program_);
}

void PA14Lowerer::EmitInitialFunctionRoots(vector<string>& entries)
{
  // Emit ordinary roots first; calls from them establish the initial
  // demand-driven frontier for free templates and inline functions.
  for(size_t i = 0; i < functions_.size(); ++i) {
    FunctionRecord& function = functions_[i];
    if(!function.definition || function.member || function.template_instantiation ||
       (function.inline_definition && !function.needed) ||
       (function.hidden_friend && !function.needed)) continue;
    entries.push_back(EmitFunction(function));
    function.emitted = true;
  }
  // Constructors directly demanded by an ordinary root belong at that root's
  // frontier.  Constructors reached through generated functions use the
  // member fixed-point pass instead.
  const auto has_nested_callable_operation = [&](const FunctionRecord& constructor) {
    const TypePtr owner = type_value(constructor.member_owner);
    if(!owner || !owner->template_specialization) return false;
    bool nested_argument = false;
    for(size_t argument = 0; argument < owner->template_arguments.size(); ++argument)
      if(owner->template_arguments[argument].find('<') != string::npos) {
        nested_argument = true;
        break;
      }
    if(!nested_argument) return false;
    for(size_t candidate = 0; candidate < functions_.size(); ++candidate) {
      const FunctionRecord& operation = functions_[candidate];
      if(!operation.definition || !operation.member || operation.constructor ||
         !operation.needed || operation.emitted || operation.member_owner.get() != owner.get())
        continue;
      if(LastComponent(operation.qualified_name) == "operator()") return true;
    }
    return false;
  };
  bool added = true;
  while(added) {
    added = false;
    for(size_t i = 0; i < functions_.size(); ++i) {
      FunctionRecord& function = functions_[i];
      if(!function.definition || !function.member || !function.constructor ||
         !function.needed || function.emitted || !function.template_instantiation ||
         has_nested_callable_operation(function)) continue;
      entries.push_back(EmitFunction(function));
      function.emitted = true;
      added = true;
    }
  }
}

void PA14Lowerer::EmitNestedRootOperations(vector<string>& entries)
{
  const auto is_nested_reference_template_owner = [](const FunctionRecord& function) {
    const TypePtr owner = type_value(function.member_owner);
    if(!owner || !owner->template_specialization) return false;
    bool nested_argument = false;
    for(size_t argument = 0; argument < owner->template_arguments.size(); ++argument)
      if(owner->template_arguments[argument].find('<') != string::npos) {
        nested_argument = true;
        break;
      }
    if(!nested_argument) return false;
    for(size_t member = 0; member < owner->class_members.size(); ++member) {
      const ClassMemberInfo& field = owner->class_members[member];
      if(field.is_static || !field.type || !type_is_reference(field.type)) continue;
      const TypePtr value = type_value(field.type);
      if(value && value->kind == TYPE_CLASS && value->template_specialization) return true;
    }
    return LastComponent(function.qualified_name) == "operator()";
  };
  const auto is_nested_callable_operation = [](const FunctionRecord& function) {
    const TypePtr owner = type_value(function.member_owner);
    if(!owner || !owner->template_specialization ||
       LastComponent(function.qualified_name) != "operator()") return false;
    for(size_t argument = 0; argument < owner->template_arguments.size(); ++argument)
      if(owner->template_arguments[argument].find('<') != string::npos) return true;
    return false;
  };
  bool added = true;
  while(added) {
    added = false;
    for(size_t i = 0; i < functions_.size(); ++i) {
      FunctionRecord& function = functions_[i];
      if(!function.definition || !function.member || function.constructor ||
         !function.needed || function.emitted || !function.template_instantiation ||
         !is_nested_reference_template_owner(function)) continue;
      entries.push_back(EmitFunction(function));
      function.emitted = true;
      if(is_nested_callable_operation(function)) {
        for(size_t candidate = 0; candidate < functions_.size(); ++candidate) {
          FunctionRecord& constructor = functions_[candidate];
          if(!constructor.definition || !constructor.member || !constructor.constructor ||
             !constructor.needed || constructor.emitted || !constructor.template_instantiation ||
             constructor.member_owner.get() != function.member_owner.get()) continue;
          entries.push_back(EmitFunction(constructor));
          constructor.emitted = true;
        }
      }
      added = true;
    }
  }
}

void PA14Lowerer::EmitOrdinaryAndHiddenRoots(vector<string>& entries)
{
  bool added = true;
  while(added) {
    added = false;
    for(size_t i = 0; i < functions_.size(); ++i) {
      FunctionRecord& function = functions_[i];
      if(!function.definition || function.member || function.hidden_friend ||
         function.emitted || !function.needed ||
         (!function.template_instantiation && !function.inline_definition)) continue;
      entries.push_back(EmitFunction(function));
      function.emitted = true;
      added = true;
    }
  }
  // Ordinary roots can demand a hidden friend during emission.  Complete this
  // typed frontier before member ordering begins.
  for(size_t i = 0; i < functions_.size(); ++i) {
    FunctionRecord& function = functions_[i];
    if(!function.definition || !function.hidden_friend || !function.needed ||
       function.emitted) continue;
    entries.push_back(EmitFunction(function));
    function.emitted = true;
  }
}

void PA14Lowerer::EmitNeededOrdinary(vector<string>& entries)
{
  bool added = true;
  while(added) {
    added = false;
    for(size_t i = 0; i < functions_.size(); ++i) {
      FunctionRecord& function = functions_[i];
      if(!function.definition || function.member || function.hidden_friend ||
         function.emitted || !function.needed ||
         (!function.template_instantiation && !function.inline_definition)) continue;
      entries.push_back(EmitFunction(function));
      function.emitted = true;
      added = true;
    }
  }
}

vector<size_t> PA14Lowerer::MemberEmissionOrder() const
{
  const auto is_conversion_member = [this](const FunctionRecord& function) {
    if(!function.member || function.static_member || function.constructor ||
       function.destructor || !function.source_type) return false;
    const TypePtr source = function_target_type(function.source_type);
    if(!source || !source->parameters.empty()) return false;
    const string name = LastComponent(function.qualified_name);
    return name.size() > 8 && name.compare(0, 8, "operator") == 0;
  };
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
}

void PA14Lowerer::EmitMemberPass(vector<string>& entries)
{
  bool added = true;
  while(added) {
    added = false;
    const vector<size_t> order = MemberEmissionOrder();
    bool restart = false;
    for(size_t phase = 0; phase < 2 && !restart; ++phase) {
      for(size_t order_index = 0; order_index < order.size(); ++order_index) {
        FunctionRecord& function = functions_[order[order_index]];
        if(!function.definition || !function.member || !function.needed || function.emitted ||
           (phase == 0) != function.base_entry) continue;
        entries.push_back(EmitFunction(function));
        function.emitted = true;
        added = true;
        restart = true;
        break;
      }
    }
  }
}

void PA14Lowerer::EmitFinalEntries(vector<string>& entries, ostream& out,
	 size_t initial_global_count)
{
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
       function.deleted || function.object_name.empty()) continue;
    string base_object = function.object_name;
    const size_t constructor = base_object.find("C1");
    if(constructor == string::npos) continue;
    base_object.replace(constructor, 2, "C2");
    entries.push_back("alias object " + base_object + " = @" + function.symbol);
  }
  vector<string> globals;
  EmitGlobals(globals, initial_global_count, false);
  entries.insert(entries.begin(), globals.begin(), globals.end());
  vector<string> declarations;
  EmitDeclarations(declarations);
  entries.insert(entries.begin(), declarations.begin(), declarations.end());
  for(size_t i = 0; i < entries.size(); ++i) {
    if(i != 0) out << "\n";
    out << entries[i];
    if(entries[i].empty() || entries[i][entries[i].size() - 1] != '\n') out << "\n";
  }
}

void PA14Lowerer::Lower(ostream& out)
{
    PrepareLoweringProgram();
    vector<string> entries;
    EmitGlobals(entries);
    EmitPolymorphicGlobals(entries);
    const size_t initial_global_count = globals_.size();
    MarkHiddenFriendDependencies();
    EmitInitialFunctionRoots(entries);
    EmitNestedRootOperations(entries);
    EmitOrdinaryAndHiddenRoots(entries);
    EmitNeededOrdinary(entries);
    EmitMemberPass(entries);
    EmitNeededOrdinary(entries);
    EmitDynamicInitializers(entries);
    EmitNeededOrdinary(entries);
    EmitMemberPass(entries);
    // A generated member body can instantiate a free helper after the last
    // ordinary-demand sweep.  Close that typed demand frontier before final
    // declarations are emitted.
    EmitNeededOrdinary(entries);
    EmitFinalEntries(entries, out, initial_global_count);
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
