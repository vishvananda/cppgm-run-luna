#include "pa14_lowering.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <sstream>
#include <string>

using namespace std;

namespace cppgm_pa14_lowering {

namespace {

bool PA14ContainsKind(const CPPGMAstNodePtr& node, const string& kind)
{
  if(!node) return false;
  if(node->kind == kind) return true;
  for(size_t child = 0; child < node->children.size(); ++child)
    if(PA14ContainsKind(node->children[child], kind)) return true;
  return false;
}

string PA14NodeValues(const CPPGMAstNodePtr& node)
{
  if(!node) return string();
  string result = node->value;
  for(size_t child = 0; child < node->children.size(); ++child) {
    const string value = PA14NodeValues(node->children[child]);
    if(!value.empty()) {
      if(!result.empty()) result += ' ';
      result += value;
    }
  }
  return result;
}

string PA14TypeUseName(const CPPGMAstNodePtr& node)
{
  if(!node) return string();
  if(node->kind == "type-name") return node->value;
  if(node->kind == "decl-specifier") {
    const size_t marker = node->value.find(':');
    return marker == string::npos ? node->value : node->value.substr(marker + 1);
  }
  for(size_t child = 0; child < node->children.size(); ++child) {
    const string result = PA14TypeUseName(node->children[child]);
    if(!result.empty()) return result;
  }
  return string();
}

string PA14TypeUseKey(string raw)
{
  while(!raw.empty() && (raw[0] == ':' || isspace(static_cast<unsigned char>(raw[0]))))
    raw.erase(0, 1);
  const size_t identifier_marker = raw.find(':');
  if(identifier_marker != string::npos &&
     raw.compare(0, identifier_marker, "TT_IDENTIFIER") == 0)
    raw.erase(0, identifier_marker + 1);
  while(raw.compare(0, 8, "typename ") == 0) raw.erase(0, 8);
  const size_t template_open = raw.find('<');
  if(template_open != string::npos) raw.erase(template_open);
  const size_t scope_marker = raw.find(':');
  if(scope_marker != string::npos && scope_marker + 1 < raw.size() &&
     raw[scope_marker + 1] == ':') raw.erase(0, scope_marker + 2);
  return LastComponent(raw);
}

} // namespace

void PA14Lowerer::IndexCompleteTemplateObjectUses(
    const CPPGMAstNodePtr& node, bool class_member_declaration)
{
  if(!node) return;
  const auto mark_complete_type = [&](const TypePtr& raw_type,
    set<const Type*>* parameter_uses) {
    set<const Type*> visited;
    function<void(const TypePtr&)> mark = [&](const TypePtr& raw_current) {
      const TypePtr current = type_value(raw_current);
      if(!current || current->kind != TYPE_CLASS ||
         !visited.insert(current.get()).second) return;
      complete_template_object_uses_.insert(current.get());
      if(parameter_uses) parameter_uses->insert(current.get());
      for(size_t base = 0; base < current->direct_bases.size(); ++base)
        mark(current->direct_bases[base]);
      if(current->direct_bases.empty()) mark(current->direct_base);
    };
    mark(raw_type);
  };
  if(node->kind == "parameter-declaration" && !node->children.empty()) {
    const CPPGMAstNodePtr declarator = node->children.size() > 1 ?
      node->children[1] : CPPGMAstNodePtr();
    if(!PA14ContainsKind(declarator, "ptr-operator") &&
       !PA14ContainsKind(declarator, "array-suffix")) {
      const string key = PA14TypeUseKey(PA14TypeUseName(node->children[0]));
      map<string, vector<TypePtr> >::const_iterator parameter_type =
        class_types_by_name_.find(key);
      if(parameter_type != class_types_by_name_.end())
        for(size_t type = 0; type < parameter_type->second.size(); ++type)
          mark_complete_type(parameter_type->second[type],
            &complete_template_parameter_uses_);
    }
  }
  if(node->kind == "new-expression" || node->kind == "call-expression") {
    const CPPGMAstNodePtr type_id = node->kind == "new-expression" ?
      ChildOfKind(node, "type-id") : CPPGMAstNodePtr();
    const string callee = node->kind == "call-expression" && !node->children.empty() &&
      node->children[0] && node->children[0]->kind == "id-expression" ?
      node->children[0]->value : string();
    const string raw_name = node->kind == "new-expression" ?
      PA14TypeUseName(type_id) : callee;
    map<string, vector<TypePtr> >::const_iterator found =
      class_types_by_name_.find(PA14TypeUseKey(raw_name));
    if(found != class_types_by_name_.end())
      for(size_t type = 0; type < found->second.size(); ++type)
        mark_complete_type(found->second[type], 0);
  }
  if(node->kind == "simple-declaration" && !class_member_declaration &&
     !node->children.empty() &&
     PA14NodeValues(node->children[0]).find("typedef") == string::npos &&
     !node->template_instantiation) {
    const CPPGMAstNodePtr list = ChildOfKind(node, "init-declarator-list");
    for(size_t item = 0; list && item < list->children.size(); ++item) {
      const CPPGMAstNodePtr entry = list->children[item];
      if(!entry || entry->children.empty()) continue;
      const CPPGMAstNodePtr declarator = entry->children[0];
      if(!declarator || declarator_name(declarator).find("::") != string::npos ||
         PA14ContainsKind(declarator, "ptr-operator") ||
         PA14ContainsKind(declarator, "parameter-clause")) continue;
      const string name = PA14TypeUseKey(PA14TypeUseName(node->children[0]));
      map<string, vector<TypePtr> >::const_iterator found =
        class_types_by_name_.find(name);
      if(found != class_types_by_name_.end())
        for(size_t type = 0; type < found->second.size(); ++type) {
          const TypePtr value = found->second[type];
          if(value->template_specialization && value->template_primary.empty()) continue;
          mark_complete_type(value, 0);
        }
    }
  }
  for(size_t child = 0; child < node->children.size(); ++child) {
    bool child_member_declaration = false;
    if(node->kind == "class-specifier" && node->children[child]) {
      const string kind = node->children[child]->kind;
      child_member_declaration = kind != "function-definition" &&
        kind != "special-member-definition" &&
        kind != "special-member-declaration";
    } else if(node->kind == "template-declaration") {
      child_member_declaration = class_member_declaration;
    }
    IndexCompleteTemplateObjectUses(node->children[child],
      child_member_declaration);
  }
}

void PA14Lowerer::IndexFriendOwners()
{
  friend_owner_index_.clear();
  for(map<const CPPGMAstNode*, TypePtr>::const_iterator it = analyzer_.class_types_.begin();
      it != analyzer_.class_types_.end(); ++it) {
    const TypePtr friend_owner = type_value(it->second);
    if(!friend_owner || friend_owner->kind != TYPE_CLASS ||
       friend_owner->friend_access.empty()) continue;
	    const vector<TypePtr> owners = BaseTypeClosure(friend_owner);
	    for(size_t owner_index = 0; owner_index < owners.size(); ++owner_index) {
	      TypePtr owner = owners[owner_index];
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

void PA14Lowerer::IndexMaterializedMemberObjectUses()
{
  materialized_member_object_uses_.clear();
  for(map<const CPPGMAstNode*, TypePtr>::const_iterator it = analyzer_.class_types_.begin();
      it != analyzer_.class_types_.end(); ++it) {
    const TypePtr container = type_value(it->second);
      if(!container || container->kind != TYPE_CLASS || container->template_specialization)
        continue;
	  if(complete_template_object_uses_.find(container.get()) ==
		  complete_template_object_uses_.end()) continue;
    for(size_t member = 0; member < container->class_members.size(); ++member) {
      const ClassMemberInfo& field = container->class_members[member];
      if(field.is_static || !field.type) continue;
      TypePtr field_type = type_value(field.type);
      while(field_type && field_type->kind == TYPE_ARRAY)
        field_type = type_value(field_type->child);
      if(field_type && field_type->kind == TYPE_CLASS)
        materialized_member_object_uses_.insert(field_type.get());
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
  IndexLambdaClosures();
  IndexClassTypesByName();
  complete_template_object_uses_.clear();
  complete_template_parameter_uses_.clear();
  IndexCompleteTemplateObjectUses(program_);
  IndexMaterializedMemberObjectUses();
  IndexFriendOwners();
  InstallBuiltins();
  CollectTopLevel(program_, analyzer_.global_.get());
  ResolveAutoFunctionReturns();
  if(has_rtti_syntax_)
    IndexRttiUses(program_, analyzer_.global_.get());
  demanded_exception_types_.clear();
  demanded_thrown_types_.clear();
  IndexExceptionUses(program_, analyzer_.global_.get());
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
       function.lambda_function ||
       (function.inline_definition && !function.needed) ||
       (function.hidden_friend && !function.needed)) continue;
    entries.push_back(EmitFunction(function));
    function.emitted = true;
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
    while(true) {
      size_t selected = functions_.size();
      for(size_t i = 0; i < functions_.size(); ++i) {
        FunctionRecord& candidate = functions_[i];
        if(!candidate.definition || candidate.member || candidate.hidden_friend ||
           candidate.emitted || !candidate.needed ||
           (!candidate.template_instantiation && !candidate.inline_definition &&
            !candidate.lambda_function)) continue;
        if(selected == functions_.size() || candidate.needed_order <
           functions_[selected].needed_order) selected = i;
      }
      if(selected == functions_.size()) break;
      FunctionRecord& function = functions_[selected];
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
         (!function.template_instantiation && !function.inline_definition &&
          !function.lambda_function)) continue;
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
  vector<size_t> lifecycle_positions;
  vector<size_t> lifecycle_order;
  bool nested_template_lifecycle = false;
  for(size_t position = 0; position < order.size(); ++position) {
    const FunctionRecord& function = functions_[order[position]];
    if(function.constructor || function.destructor) {
      lifecycle_positions.push_back(position);
      lifecycle_order.push_back(order[position]);
      const TypePtr owner = type_value(function.member_owner);
      if(owner && owner->template_specialization)
        for(size_t argument = 0; argument < owner->template_arguments.size(); ++argument)
          if(owner->template_arguments[argument].find('<') != string::npos)
            nested_template_lifecycle = true;
    }
  }
  if(nested_template_lifecycle) {
  stable_sort(lifecycle_order.begin(), lifecycle_order.end(), [this](size_t left,
                                                                       size_t right) {
    const size_t unset = static_cast<size_t>(-1);
    const size_t left_order = functions_[left].needed_order;
    const size_t right_order = functions_[right].needed_order;
    if((left_order != unset) != (right_order != unset))
      return left_order != unset;
    if(left_order != unset && left_order != right_order)
      return left_order < right_order;
    return left < right;
  });
  for(size_t position = 0; position < lifecycle_positions.size(); ++position)
    order[lifecycle_positions[position]] = lifecycle_order[position];
  }
  OrderLambdaMembers(order);
  return order;
}

void PA14Lowerer::OrderLambdaMembers(vector<size_t>& order) const
{
  // Lambda closure operators are collected from generated class shells.  A
  // replayed class-return operator can otherwise land after the pointer
  // operators and the value-type constructor records that its body demands.
  // Keep the lambda operator frontier contiguous and deterministic: the
  // class-valued closures establish their result type first, followed by the
  // pointer-valued callable members, while ordinary lifecycle records retain
  // their relative order after that frontier.
  vector<size_t> lambda_positions;
  vector<size_t> lambda_order;
  for(size_t position = 0; position < order.size(); ++position)
    if(IsLambdaOperator(functions_[order[position]])) {
      lambda_positions.push_back(position);
      lambda_order.push_back(order[position]);
    }
  if(lambda_positions.size() > 1) {
    const auto lambda_order_key = [this](size_t index) {
      const TypePtr owner = type_value(functions_[index].member_owner);
      const string owner_name = owner ? LastComponent(owner->name) :
        functions_[index].qualified_name;
      const size_t replay = owner_name.find("__inst__");
      if(replay == string::npos) return string("0|") + owner_name;
      const size_t result = owner_name.find("_Result", replay + 8);
      const string group = owner_name.substr(replay + 8,
        result == string::npos ? string::npos : result - (replay + 8));
      return string("1|") + group + "|" + owner_name.substr(0, replay);
    };
    stable_sort(lambda_order.begin(), lambda_order.end(), [this, &lambda_order_key](size_t left,
                                                                  size_t right) {
      const FunctionRecord& left_function = functions_[left];
      const FunctionRecord& right_function = functions_[right];
      const TypePtr left_result = left_function.source_type &&
        left_function.source_type->child ?
        type_value(left_function.source_type->child) : TypePtr();
      const TypePtr right_result = right_function.source_type &&
        right_function.source_type->child ?
        type_value(right_function.source_type->child) : TypePtr();
      const bool left_class = left_result && left_result->kind == TYPE_CLASS;
      const bool right_class = right_result && right_result->kind == TYPE_CLASS;
      if(left_class != right_class) return left_class;
      const string left_key = lambda_order_key(left);
      const string right_key = lambda_order_key(right);
      if(left_key != right_key) return left_key < right_key;
      return left_function.qualified_name < right_function.qualified_name;
    });
    const size_t first_lambda_position = lambda_positions.front();
    vector<size_t> reordered;
    reordered.reserve(order.size());
    for(size_t position = 0; position < order.size(); ++position) {
      if(position == first_lambda_position)
        reordered.insert(reordered.end(), lambda_order.begin(), lambda_order.end());
      if(find(lambda_positions.begin(), lambda_positions.end(), position) ==
         lambda_positions.end()) reordered.push_back(order[position]);
    }
    order.swap(reordered);
  }
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
    const size_t initial_global_count = globals_.size();
    MarkHiddenFriendDependencies();
    EmitInitialFunctionRoots(entries);
    EmitNestedRootOperations(entries);
    // Close the member frontier created directly by the initial roots before
    // discovering the next free-function frontier.  Later replayed demands
    // are ordered separately once those roots have been emitted.
    EmitMemberPass(entries);
    EmitOrdinaryAndHiddenRoots(entries);
    // A generated class used only inside an elided initializer can be
    // collected during the replayed translation-unit walk.  Apply that typed
    // demand after ordinary roots have established their own frontier.
    for(size_t global = 0; global < globals_.size(); ++global) {
      GlobalRecord& record = globals_[global];
      if(record.demand_constant_constructors && record.initializer)
        DemandConstantObjectConstructors(record.type, record.initializer,
                                          record.scope);
    }
    EmitMemberPass(entries);
    EmitNeededOrdinary(entries);
    EmitMemberPass(entries);
    EmitNeededOrdinary(entries);
    EmitDynamicInitializers(entries);
    EmitNeededOrdinary(entries);
    EmitMemberPass(entries);
    // Constructor and virtual-call lowering discovers the concrete
    // polymorphic owners after the initial semantic preparation pass.  Emit
    // the vtable group only after that typed demand frontier is closed so
    // complete-object constructors cannot reference a table that was never
    // selected as a root.
    EmitPolymorphicGlobals(entries);
    EmitMemberPass(entries);
    EmitNeededOrdinary(entries);
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
