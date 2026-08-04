#include "pa14_lowering.h"

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

string LambdaParameterName(const CPPGMAstNodePtr& parameter, size_t index)
{
  if(parameter && parameter->children.size() > 1 && parameter->children[1]) {
    const string name = FirstIdentifier(parameter->children[1]);
    if(!name.empty()) return last_component(name);
  }
  return "__param" + integer_text(static_cast<long long>(index));
}

} // namespace

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
