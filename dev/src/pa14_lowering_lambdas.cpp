#include "pa14_lowering.h"

#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace std;

namespace cppgm_pa14_lowering {

namespace {

CPPGMAstNodePtr LambdaDeclarator(const CPPGMAstNodePtr& lambda)
{
  return ChildOfKind(lambda, "lambda-declarator");
}

CPPGMAstNodePtr LambdaBody(const CPPGMAstNodePtr& lambda)
{
  return ChildOfKind(lambda, "compound-statement");
}

CPPGMAstNodePtr LambdaParameters(const CPPGMAstNodePtr& lambda)
{
  const CPPGMAstNodePtr declarator = LambdaDeclarator(lambda);
  return declarator ? ChildOfKind(declarator, "parameter-clause") :
    CPPGMAstNodePtr();
}

CPPGMAstNodePtr LambdaTrailingReturn(const CPPGMAstNodePtr& lambda)
{
  const CPPGMAstNodePtr declarator = LambdaDeclarator(lambda);
  return declarator ? ChildOfKind(declarator, "trailing-return-type") :
    CPPGMAstNodePtr();
}

bool LambdaHasCapture(const CPPGMAstNodePtr& lambda)
{
  if(!lambda || lambda->children.empty() || !lambda->children[0]) return false;
  const CPPGMAstNodePtr introducer = lambda->children[0];
  for(size_t i = 0; i < introducer->children.size(); ++i)
    if(introducer->children[i] && !introducer->children[i]->children.empty())
      return true;
  return false;
}

bool LambdaContainsClass(const CPPGMAstNodePtr& node)
{
  if(!node) return false;
  if(node->kind == "class-specifier" || node->kind == "class-forward-declaration")
    return true;
  for(size_t child = 0; child < node->children.size(); ++child)
    if(LambdaContainsClass(node->children[child])) return true;
  return false;
}

string LambdaParameterName(const CPPGMAstNodePtr& parameter, size_t index)
{
  if(parameter && parameter->children.size() > 1 && parameter->children[1]) {
    const string name = FirstIdentifier(parameter->children[1]);
    if(!name.empty()) return last_component(name);
  }
  return "__param" + integer_text(static_cast<long long>(index));
}

} // namespace

string PA14Lowerer::LambdaClosureName(const CPPGMAstNodePtr& lambda) const
{
  if(!lambda) return string();
  const pair<size_t, size_t> span(lambda->source_token_begin,
    lambda->source_token_end);
  map<pair<size_t, size_t>, string>::const_iterator found =
    lambda_closure_names_.find(span);
  return found == lambda_closure_names_.end() ? string() : found->second;
}

TypePtr PA14Lowerer::LambdaClosureType(const CPPGMAstNodePtr& lambda) const
{
  const string name = LambdaClosureName(lambda);
  if(name.empty()) return TypePtr();
  map<string, TypePtr>::const_iterator found = lambda_closure_types_.find(name);
  return found == lambda_closure_types_.end() ? TypePtr() : found->second;
}

bool PA14Lowerer::IsLambdaClosureType(const TypePtr& raw_type) const
{
  const TypePtr type = type_value(raw_type);
  if(!type || type->kind != TYPE_CLASS) return false;
  for(map<string, TypePtr>::const_iterator it = lambda_closure_types_.begin();
      it != lambda_closure_types_.end(); ++it)
    if(it->second && (it->second.get() == type.get() ||
       PA12SameType(it->second, type, true))) return true;
  return false;
}

bool PA14Lowerer::LambdaNeedsClosure(const CPPGMAstNodePtr& lambda,
                                     const TypePtr& expected) const
{
  if(!lambda || lambda->kind != "lambda-expression") return false;
  const TypePtr closure = LambdaClosureType(lambda);
  const TypePtr expected_value = type_value(expected);
  if(closure && expected_value && IsLambdaClosureType(expected_value) &&
     PA12SameType(closure, expected_value, true)) return true;
  if(LambdaHasCapture(lambda)) return true;
  if(lambda->materialize_object_address) return true;
  const CPPGMAstNodePtr body = LambdaBody(lambda);
  // A body-local type is part of the closure's semantic identity.  The free
  // function representation cannot carry the enclosing function's local
  // class scope through template replay.
  return LambdaContainsClass(body);
}

bool PA14Lowerer::EmitLambdaClosureValue(const CPPGMAstNodePtr& lambda,
                                         Scope* scope, const TypePtr& expected,
                                         Value* result)
{
  const TypePtr closure = LambdaClosureType(lambda);
  if(!closure || !LambdaNeedsClosure(lambda, expected) || !result) return false;
  const string slot = new_special_slot("lambda", low_type(closure));
  bool has_capture_field = false;
  for(size_t member = 0; member < closure->class_members.size(); ++member)
    if(closure->class_members[member].name.compare(0, 10, "__capture_") == 0) {
      has_capture_field = true;
      break;
    }
  if(has_capture_field) {
    const string address = new_temp();
    AddInstruction(address + " = addr $" + slot);
    InitializeLambdaClosureAt(closure, address, lambda, scope);
  }
  result->type = closure;
  result->operand = "$" + slot;
  return true;
}

bool PA14Lowerer::IsLambdaOperator(const FunctionRecord& function) const
{
  const TypePtr owner = type_value(function.member_owner);
  if(!function.member || !owner || owner->kind != TYPE_CLASS) return false;
  const string owner_name = last_component(owner->name);
    return owner_name.compare(0, 9, "__lambda_") == 0 &&
    last_component(function.qualified_name) == "operator()";
}

namespace {

const ClassMemberInfo* LambdaField(const TypePtr& owner, const string& name)
{
  if(!owner || owner->kind != TYPE_CLASS) return 0;
  for(size_t member = 0; member < owner->class_members.size(); ++member)
    if(owner->class_members[member].name == name) return &owner->class_members[member];
  return 0;
}

CPPGMAstNodePtr LambdaFieldExpression(const string& name)
{
  CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "OP_ARROW:->"));
  member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
    "keyword-literal", "KW_THIS:this")));
  member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
    "identifier", name)));
  return member;
}

} // namespace

void PA14Lowerer::InitializeLambdaClosureAt(
  const TypePtr& raw_closure, const string& destination,
  const CPPGMAstNodePtr& lambda, Scope* scope)
{
  TypePtr closure = type_value(raw_closure);
  if(!closure || closure->kind != TYPE_CLASS || !lambda) return;
  const TypePtr current_owner = state_ && state_->record &&
    IsLambdaOperator(*state_->record) ? type_value(state_->record->member_owner) : TypePtr();

  for(size_t member = 0; member < closure->class_members.size(); ++member) {
    const ClassMemberInfo& field = closure->class_members[member];
    if(field.name.compare(0, 10, "__capture_") != 0 || !field.type) continue;
    const string source_name = field.name.substr(10);
    const bool source_this = source_name == "this";
    CPPGMAstNodePtr source_node;
    bool source_is_field = false;
    const ClassMemberInfo* enclosing_field = current_owner ?
      LambdaField(current_owner, field.name) : 0;
    if(current_owner && IsLambdaOperator(*state_->record)) {
      const ClassMemberInfo* candidate = LambdaField(current_owner,
        string("__capture_") + source_name);
      if(candidate && (source_this || !FindLocalPlan(source_name))) {
        source_is_field = true;
        source_node = LambdaFieldExpression(candidate->name);
      }
    }
    if(!source_is_field) {
      source_node = CPPGMAstNodePtr(new CPPGMAstNode(
        source_this ? "keyword-literal" : "id-expression",
        source_this ? "KW_THIS:this" : source_name));
    }

    string field_address = destination;
    if(field.offset != 0) {
      field_address = new_temp();
      AddInstruction(field_address + " = index i8 " + destination + ", " +
        integer_text(field.offset));
    }
    const bool reference_capture = type_is_reference(field.type);
    if(reference_capture || source_is_field && enclosing_field &&
       type_is_reference(enclosing_field->type)) {
      // Reference captures store the address of the referent.  A nested
      // capture reads that address from the enclosing reference field, which
      // preserves the original object rather than pointing at the outer
      // closure's field slot.
      string address = EmitAddress(source_node, scope);
      emit_store(PointerTo(Fundamental("char")), address, field_address);
    } else {
      Value value = EmitValue(source_node, scope, type_value(field.type));
      emit_store(field.type, value.operand, field_address);
    }
  }
}

void PA14Lowerer::IndexLambdaClosures()
{
  lambda_closure_names_.clear();
  lambda_closure_types_.clear();
  lambda_closure_nodes_.clear();
  has_rtti_syntax_ = false;
  function<void(const CPPGMAstNodePtr&)> visit;
  visit = [&](const CPPGMAstNodePtr& node) {
    if(!node) return;
    if((node->kind == "type-trait-expression" &&
        node->value.find("typeid") != string::npos) ||
       (node->kind == "cast-expression" &&
        node->value.find("dynamic_cast") != string::npos))
      has_rtti_syntax_ = true;
    if(node->kind == "class-specifier") {
      const string name = last_component(node->value);
      if(name.compare(0, 9, "__lambda_") == 0) {
        lambda_closure_nodes_[name] = node;
        if(node->source_token_begin != static_cast<size_t>(-1) &&
           node->source_token_end != static_cast<size_t>(-1))
          lambda_closure_names_[make_pair(node->source_token_begin,
            node->source_token_end)] = name;
        map<const CPPGMAstNode*, TypePtr>::const_iterator type =
          analyzer_.class_types_.find(node.get());
        if(type != analyzer_.class_types_.end() && type->second)
          lambda_closure_types_[name] = type_value(type->second);
      }
    }
    for(size_t child = 0; child < node->children.size(); ++child)
      visit(node->children[child]);
  };
  visit(program_);
}

PA14Lowerer::FunctionRecord* PA14Lowerer::EnsureLambdaFunction(
    const CPPGMAstNodePtr& lambda, Scope* scope)
{
  if(!lambda || lambda->kind != "lambda-expression")
    throw logic_error("invalid lambda expression");
  map<const CPPGMAstNode*, FunctionRecord*>::const_iterator existing =
    lambda_functions_.find(lambda.get());
  if(existing != lambda_functions_.end()) return existing->second;
  if(LambdaHasCapture(lambda))
    throw logic_error("capturing lambda lowering is not available in this increment");

  Scope* lexical_scope = scope ? scope : analyzer_.global_.get();
  const CPPGMAstNodePtr clause = LambdaParameters(lambda);
  vector<TypePtr> parameters;
  bool variadic = false;
  vector<string> parameter_names;
  if(clause) for(size_t i = 0; i < clause->children.size(); ++i) {
    const CPPGMAstNodePtr parameter = clause->children[i];
    if(!parameter) continue;
    if(parameter->kind == "parameter-pack" || parameter->kind == "ellipsis") {
      variadic = true;
      continue;
    }
    if(parameter->kind != "parameter-declaration" || parameter->children.empty())
      continue;
    Analyzer::SpecFacts facts;
    TypePtr base = analyzer_.TypeFromSpecSeq(parameter->children[0], lexical_scope,
      &facts);
    TypePtr type = base;
    if(parameter->children.size() > 1 && parameter->children[1] &&
       (parameter->children[1]->kind == "declarator" ||
        parameter->children[1]->kind == "abstract-declarator"))
      type = analyzer_.BuildDeclarator(parameter->children[1], base, lexical_scope);
    parameters.push_back(type);
    parameter_names.push_back(LambdaParameterName(parameter, i));
  }

  string owner;
  if(state_ && state_->record)
    owner = low_symbol_component(state_->record->qualified_name);
  size_t begin = lambda->source_token_begin;
  size_t end = lambda->source_token_end;
  if(begin == static_cast<size_t>(-1) || end == static_cast<size_t>(-1)) {
    begin = next_lambda_serial_;
    end = next_lambda_serial_++;
  }
  ostringstream name;
  name << "__lambda_";
  if(!owner.empty()) name << owner << "_";
  name << "t" << begin << "_" << end;
  string qname = name.str();
  for(size_t collision = 2;; ++collision) {
    bool used = false;
    for(size_t i = 0; i < functions_.size(); ++i)
      if(functions_[i].qualified_name == qname) { used = true; break; }
    if(!used) break;
    qname = name.str() + "__ov" + integer_text(static_cast<long long>(collision));
  }

  // The synthetic function scope is created before the trailing return type
  // is resolved so `decltype(parameter)` sees the lambda's typed parameters.
  Scope* function_scope = analyzer_.NewChild(lexical_scope, SCOPE_FUNCTION, qname);
  for(size_t i = 0; i < parameters.size(); ++i)
    function_scope->add(Binding(BIND_PARAMETER, parameter_names[i], parameters[i]));

  TypePtr return_type = Fundamental("auto");
  const CPPGMAstNodePtr trailing = LambdaTrailingReturn(lambda);
  if(trailing && !trailing->children.empty())
    return_type = analyzer_.TypeFromTypeId(trailing->children[0], function_scope);
  TypePtr source_type = FunctionOf(parameters, variadic, return_type);

  // EmitFunction and PlanFunction intentionally consume the ordinary
  // function-definition shape.  Reuse the original parameter clause and body
  // so all existing statement planning and typed expression lowering remains
  // authoritative for the lambda body.
  CPPGMAstNodePtr definition(new CPPGMAstNode("function-definition"));
  CPPGMAstNodePtr specifiers(new CPPGMAstNode("decl-specifier-seq"));
  specifiers->children.push_back(CPPGMAstNodePtr(
    new CPPGMAstNode("decl-specifier", "KW_AUTO:auto")));
  CPPGMAstNodePtr declarator(new CPPGMAstNode("declarator"));
  declarator->children.push_back(CPPGMAstNodePtr(
    new CPPGMAstNode("identifier", qname)));
  if(clause) declarator->children.push_back(clause);
  definition->children.push_back(specifiers);
  definition->children.push_back(declarator);
  const CPPGMAstNodePtr body = LambdaBody(lambda);
  if(!body) throw logic_error("lambda has no compound body");
  definition->children.push_back(body);
  definition->source_token_begin = begin;
  definition->source_token_end = end;

  analyzer_.function_scopes_[definition.get()] = function_scope;
  functions_.push_back(FunctionRecord());
  FunctionRecord* record = &functions_.back();
  record->node = definition;
  record->scope = lexical_scope;
  record->source_type = source_type;
  record->type = source_type;
  record->qualified_name = qname;
  record->symbol = low_symbol_component(qname);
  record->definition = true;
  record->lambda_function = true;
  lambda_functions_[lambda.get()] = record;
  function_by_key_[function_key(qname, source_type)] = record;

  // A lambda with an omitted trailing return type is resolved as soon as it
  // is demanded.  This is needed for local lambdas, which are discovered
  // after the translation-unit auto-return pass has already run.
  if(!trailing) ResolveAutoFunctionReturn(*record);
  BuildFunctionABI(*record);
  return record;
}

} // namespace cppgm_pa14_lowering
