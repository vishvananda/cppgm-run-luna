#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

#include <cctype>
#include <sstream>

using namespace std;

namespace pa18_templates_internal {

namespace {

CPPGMAstNodePtr LambdaDeclaratorNode(const CPPGMAstNodePtr& lambda)
{
  return ChildOfKindLocal(lambda, "lambda-declarator");
}

CPPGMAstNodePtr LambdaParameterClauseNode(const CPPGMAstNodePtr& lambda)
{
  const CPPGMAstNodePtr declarator = LambdaDeclaratorNode(lambda);
  return declarator ? ChildOfKindLocal(declarator, "parameter-clause") :
    CPPGMAstNodePtr();
}

CPPGMAstNodePtr LambdaTrailingReturnNode(const CPPGMAstNodePtr& lambda)
{
  const CPPGMAstNodePtr declarator = LambdaDeclaratorNode(lambda);
  return declarator ? ChildOfKindLocal(declarator, "trailing-return-type") :
    CPPGMAstNodePtr();
}

CPPGMAstNodePtr LambdaBodyNode(const CPPGMAstNodePtr& lambda)
{
  return ChildOfKindLocal(lambda, "compound-statement");
}

bool LambdaBodyContainsClass(const CPPGMAstNodePtr& node)
{
  if(!node) return false;
  if(node->kind == "class-specifier" || node->kind == "class-forward-declaration")
    return true;
  for(size_t child = 0; child < node->children.size(); ++child)
    if(LambdaBodyContainsClass(node->children[child])) return true;
  return false;
}

bool LambdaHasCaptureNode(const CPPGMAstNodePtr& lambda)
{
  if(!lambda || lambda->children.empty() || !lambda->children[0]) return false;
  const CPPGMAstNodePtr introducer = lambda->children[0];
  for(size_t child = 0; child < introducer->children.size(); ++child)
    if(introducer->children[child] &&
       !introducer->children[child]->children.empty()) return true;
  return false;
}

string LambdaNameComponent(const string& raw)
{
  string result;
  for(size_t i = 0; i < raw.size(); ++i) {
    const unsigned char ch = static_cast<unsigned char>(raw[i]);
    if(isalnum(ch) || raw[i] == '_') result += raw[i];
    else if(raw[i] == ':' && i + 1 < raw.size() && raw[i + 1] == ':') {
      result += "__";
      ++i;
    } else result += '_';
  }
  return result;
}

string FunctionContextForLambda(const CPPGMAstNodePtr& node,
  const string& context, const string& inherited)
{
  if(!node || node->kind != "function-definition" || node->children.size() < 2)
    return inherited;
  const string raw_name = FirstIdentifierLocal(node->children[1]);
  const string name = LastComponent(raw_name);
  if(name.empty()) return inherited;
  string owner = PrefixComponent(raw_name);
  if(owner.empty()) owner = context;
  if(!owner.empty() && LastComponent(owner) == name) return owner;
  return JoinPath(owner, name);
}

string CaptureFieldName(const LambdaCaptureSpec& capture)
{
  return string("__capture_") + (capture.this_capture ? "this" : capture.name);
}

string CaptureNameFromNode(const CPPGMAstNodePtr& node)
{
  if(!node) return string();
  return RemoveMarker(node->value);
}

void AppendUniqueName(const string& name, vector<string>* names,
                      set<string>* seen)
{
  if(name.empty() || !names || !seen || !seen->insert(name).second) return;
  names->push_back(name);
}

void CollectLambdaDeclarations(const CPPGMAstNodePtr& node,
                               set<string>* names)
{
  if(!node || !names) return;
  if(node->kind == "parameter-declaration" && node->children.size() > 1) {
    const string name = FirstIdentifierLocal(node->children[1]);
    if(!name.empty()) names->insert(LastComponent(RemoveMarker(name)));
  } else if(node->kind == "simple-declaration") {
    const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
    if(list) for(size_t child = 0; child < list->children.size(); ++child) {
      const CPPGMAstNodePtr item = list->children[child];
      if(!item || item->children.empty()) continue;
      const string name = FirstIdentifierLocal(item->children[0]);
      if(!name.empty()) names->insert(LastComponent(RemoveMarker(name)));
    }
  }
  for(size_t child = 0; child < node->children.size(); ++child)
    CollectLambdaDeclarations(node->children[child], names);
}

void CollectLambdaReferences(const CPPGMAstNodePtr& node,
                             vector<string>* names, set<string>* seen)
{
  if(!node || !names || !seen) return;
  if(node->kind == "id-expression") {
    const string name = LastComponent(RemoveMarker(node->value));
    AppendUniqueName(name, names, seen);
  } else if(node->kind == "keyword-literal" &&
            RemoveMarker(node->value) == "this")
    AppendUniqueName("this", names, seen);
  for(size_t child = 0; child < node->children.size(); ++child)
    CollectLambdaReferences(node->children[child], names, seen);
}

void CollectVariableNames(const CPPGMAstNodePtr& node, set<string>* names)
{
  if(!node || !names) return;
  if(node->kind == "parameter-declaration" && node->children.size() > 1) {
    const string name = FirstIdentifierLocal(node->children[1]);
    if(!name.empty()) names->insert(LastComponent(RemoveMarker(name)));
  } else if(node->kind == "simple-declaration") {
    const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
    if(list) for(size_t child = 0; child < list->children.size(); ++child) {
      const CPPGMAstNodePtr item = list->children[child];
      if(!item || item->children.empty()) continue;
      // A function declaration has a parameter-clause in its declarator;
      // its name is not an object that a lambda's default capture can bind.
      if(ChildOfKindLocal(item->children[0], "parameter-clause") ||
         DescendantOfKind(item->children[0], "parameter-clause")) continue;
      const string name = FirstIdentifierLocal(item->children[0]);
      if(!name.empty()) names->insert(LastComponent(RemoveMarker(name)));
    }
  }
  for(size_t child = 0; child < node->children.size(); ++child)
    CollectVariableNames(node->children[child], names);
}

bool IsAutoSpelling(const string& raw)
{
  return CanonicalSpelling(raw) == "auto" ||
    CanonicalSpelling(raw) == "const auto";
}

string StripReferenceSuffix(string raw)
{
  raw = CanonicalSpelling(raw);
  while(!raw.empty() && raw[raw.size() - 1] == '&') raw.erase(raw.size() - 1);
  return CanonicalSpelling(raw);
}

void SplitSimpleType(string raw, string* base, size_t* pointer_count)
{
  if(base) base->clear();
  if(pointer_count) *pointer_count = 0;
  raw = StripReferenceSuffix(raw);
  string compact;
  for(size_t i = 0; i < raw.size(); ++i) {
    if(raw[i] == '*') {
      if(pointer_count) ++*pointer_count;
    } else compact += raw[i];
  }
  if(base) *base = CanonicalSpelling(compact);
}

CPPGMAstNodePtr MakeDeclSpecifierSequence(const string& raw)
{
  CPPGMAstNodePtr sequence(new CPPGMAstNode("decl-specifier-seq"));
  string current;
  vector<string> words;
  for(size_t i = 0; i <= raw.size(); ++i) {
    if(i == raw.size() || isspace(static_cast<unsigned char>(raw[i]))) {
      if(!current.empty()) { words.push_back(current); current.clear(); }
    } else current += raw[i];
  }
  if(words.empty()) words.push_back("void");
  for(size_t i = 0; i < words.size(); ++i) {
    const string& word = words[i];
    string value = word;
    if(word == "const" || word == "volatile" || word == "static" ||
       word == "mutable" || word == "unsigned" || word == "signed" ||
       word == "short" || word == "long" || word == "int" ||
       word == "bool" || word == "char" || word == "float" ||
       word == "double" || word == "void") {
      string keyword = "KW_";
      if(word == "const") keyword += "CONST";
      else if(word == "volatile") keyword += "VOLATILE";
      else if(word == "static") keyword += "STATIC";
      else if(word == "mutable") keyword += "MUTABLE";
      else if(word == "unsigned") keyword += "UNSIGNED";
      else if(word == "signed") keyword += "SIGNED";
      else if(word == "short") keyword += "SHORT";
      else if(word == "long") keyword += "LONG";
      else if(word == "int") keyword += "INT";
      else if(word == "bool") keyword += "BOOL";
      else if(word == "char") keyword += "CHAR";
      else if(word == "float") keyword += "FLOAT";
      else if(word == "double") keyword += "DOUBLE";
      else keyword += "VOID";
      value = keyword + ":" + word;
    } else value = "TT_IDENTIFIER:" + word;
    sequence->children.push_back(CPPGMAstNodePtr(
      new CPPGMAstNode("decl-specifier", value)));
  }
  return sequence;
}

CPPGMAstNodePtr MakeSimpleCaptureField(const LambdaCaptureSpec& capture)
{
  string base;
  size_t pointers = 0;
  SplitSimpleType(capture.type, &base, &pointers);
  if(base.empty()) base = "void";
  if(capture.this_capture) ++pointers;
  CPPGMAstNodePtr declaration(new CPPGMAstNode("simple-declaration"));
  declaration->children.push_back(MakeDeclSpecifierSequence(base));
  CPPGMAstNodePtr list(new CPPGMAstNode("init-declarator-list"));
  CPPGMAstNodePtr item(new CPPGMAstNode("init-declarator"));
  CPPGMAstNodePtr declarator(new CPPGMAstNode("declarator"));
  for(size_t pointer = 0; pointer < pointers; ++pointer)
    declarator->children.push_back(CPPGMAstNodePtr(
      new CPPGMAstNode("ptr-operator", "OP_STAR:*")));
  if(capture.by_reference)
    declarator->children.push_back(CPPGMAstNodePtr(
      new CPPGMAstNode("ptr-operator", "OP_AMP:&")));
  declarator->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
    "identifier", CaptureFieldName(capture))));
  item->children.push_back(declarator);
  list->children.push_back(item);
  declaration->children.push_back(list);
  return declaration;
}

CPPGMAstNodePtr MakeFunctionCaptureField(const LambdaCaptureSpec& capture,
                                         const CPPGMAstNodePtr& lambda)
{
  const CPPGMAstNodePtr declarator_source = LambdaDeclaratorNode(lambda);
  const CPPGMAstNodePtr clause = LambdaParameterClauseNode(lambda);
  const CPPGMAstNodePtr trailing = LambdaTrailingReturnNode(lambda);
  string return_type;
  if(trailing && !trailing->children.empty())
    return_type = SpellNode(trailing->children[0]);
  if(return_type.empty()) return_type = "void";
  CPPGMAstNodePtr declaration(new CPPGMAstNode("simple-declaration"));
  declaration->children.push_back(MakeDeclSpecifierSequence(return_type));
  CPPGMAstNodePtr list(new CPPGMAstNode("init-declarator-list"));
  CPPGMAstNodePtr item(new CPPGMAstNode("init-declarator"));
  CPPGMAstNodePtr outer(new CPPGMAstNode("declarator"));
  CPPGMAstNodePtr nested(new CPPGMAstNode("nested-declarator"));
  CPPGMAstNodePtr inner(new CPPGMAstNode("declarator"));
  inner->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
    "ptr-operator", "OP_STAR:*")));
  if(capture.by_reference)
    inner->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
      "ptr-operator", "OP_AMP:&")));
  inner->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
    "identifier", CaptureFieldName(capture))));
  nested->children.push_back(inner);
  outer->children.push_back(nested);
  if(clause) outer->children.push_back(CloneNode(clause));
  item->children.push_back(outer);
  list->children.push_back(item);
  declaration->children.push_back(list);
  (void)declarator_source;
  return declaration;
}

CPPGMAstNodePtr CaptureMemberExpression(const LambdaCaptureSpec& capture)
{
  CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "OP_ARROW:->"));
  member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
    "keyword-literal", "KW_THIS:this")));
  member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
    "identifier", CaptureFieldName(capture))));
  return member;
}

bool HasCapture(const vector<LambdaCaptureSpec>& captures, const string& name)
{
  for(size_t i = 0; i < captures.size(); ++i)
    if((captures[i].this_capture ? "this" : captures[i].name) == name) return true;
  return false;
}

const LambdaCaptureSpec* FindCapture(const vector<LambdaCaptureSpec>& captures,
                                     const string& name)
{
  for(size_t i = 0; i < captures.size(); ++i)
    if((captures[i].this_capture ? "this" : captures[i].name) == name)
      return &captures[i];
  return 0;
}

void RewriteCapturedLambdaBody(const CPPGMAstNodePtr& node,
                               const vector<LambdaCaptureSpec>& captures)
{
  if(!node) return;
  // A nested lambda has its own closure class and its own source capture
  // list.  Its body is rewritten when that class is prepared; applying the
  // enclosing closure's fields here would change the nested capture source.
  if(node->kind == "lambda-expression") return;
  for(size_t child = 0; child < node->children.size(); ++child) {
    CPPGMAstNodePtr& current = node->children[child];
    if(!current) continue;
    if(current->kind == "id-expression") {
      const string name = LastComponent(RemoveMarker(current->value));
      const LambdaCaptureSpec* capture = FindCapture(captures, name);
      if(capture) current = CaptureMemberExpression(*capture);
    } else if(current->kind == "keyword-literal" &&
              RemoveMarker(current->value) == "this") {
      const LambdaCaptureSpec* capture = FindCapture(captures, "this");
      if(capture) current = CaptureMemberExpression(*capture);
    } else RewriteCapturedLambdaBody(current, captures);
  }
}

} // namespace

bool ContainsLambdaExpression(const CPPGMAstNodePtr& node)
{
  if(!node) return false;
  if(node->kind == "lambda-expression") return true;
  for(size_t child = 0; child < node->children.size(); ++child)
    if(ContainsLambdaExpression(node->children[child])) return true;
  return false;
}

bool NeedsPA18Expansion(const CPPGMAstNodePtr& node)
{
  if(!node) return false;
  if(node->kind == "lambda-expression") {
    if(!node->children.empty() && node->children[0])
      for(size_t child = 0; child < node->children[0]->children.size(); ++child)
        if(node->children[0]->children[child] &&
           !node->children[0]->children[child]->children.empty()) return true;
    function<bool(const CPPGMAstNodePtr&)> body_class =
      [&body_class](const CPPGMAstNodePtr& current) -> bool {
        if(!current) return false;
        if(current->kind == "class-specifier" ||
           current->kind == "class-forward-declaration") return true;
        for(size_t child = 0; child < current->children.size(); ++child)
          if(body_class(current->children[child])) return true;
        return false;
      };
    for(size_t child = 0; child < node->children.size(); ++child)
      if(node->children[child] && node->children[child]->kind == "compound-statement" &&
         body_class(node->children[child])) return true;
  }
  if(node->kind == "literal" && !node->value.empty() &&
     isdigit(static_cast<unsigned char>(node->value[0])) &&
     node->value.find('_') != string::npos) return true;
  if(node->kind == "template-declaration" ||
     node->kind == "explicit-instantiation-declaration") return true;
  if((node->kind == "id-expression" || node->kind == "decl-specifier" ||
      node->kind == "type-name" || node->kind == "type-specifier" ||
      node->kind == "class-specifier" || node->kind == "class-forward-declaration" ||
      node->kind == "alias-declaration" || node->kind == "target") &&
     node->value.find('<') != string::npos) return true;
  for(size_t i = 0; i < node->children.size(); ++i)
    if(NeedsPA18Expansion(node->children[i])) return true;
  return false;
}

void PA18TemplateExpander::SelectCapturedLambdaPackMember(
  const CPPGMAstNodePtr& node, size_t index) const
{
  if(!node) return;
  if(node->kind == "member-expression" && node->children.size() > 1 &&
     node->children[1] && node->children[1]->kind == "identifier") {
    const string field = RemoveMarker(node->children[1]->value);
    const string prefix = "__capture_";
    if(field.compare(0, prefix.size(), prefix) == 0) {
      const string name = field.substr(prefix.size());
      map<string, vector<string> >::const_iterator identifiers =
        active_pack_identifier_substitutions_.find(name);
      if(identifiers != active_pack_identifier_substitutions_.end() &&
         index < identifiers->second.size())
        node->children[1]->value = prefix + identifiers->second[index];
    }
  }
  for(size_t child = 0; child < node->children.size(); ++child)
    SelectCapturedLambdaPackMember(node->children[child], index);
}

string PA18TemplateExpander::LambdaClassName(
  const CPPGMAstNodePtr& lambda, const string& function_context)
{
  map<const CPPGMAstNode*, string>::const_iterator existing =
    lambda_class_names_.find(lambda.get());
  if(existing != lambda_class_names_.end()) return existing->second;
  size_t begin = lambda ? lambda->source_token_begin : static_cast<size_t>(-1);
  size_t end = lambda ? lambda->source_token_end : static_cast<size_t>(-1);
  if(begin == static_cast<size_t>(-1) || end == static_cast<size_t>(-1)) {
    begin = next_lambda_serial_;
    end = next_lambda_serial_++;
  }
  ostringstream base;
  base << "__lambda_";
  const string owner = LambdaNameComponent(function_context);
  if(!owner.empty()) base << owner << "_";
  base << "t" << begin << "_" << end;
  string result = base.str();
  for(size_t collision = 2; lambda_class_name_set_.find(result) !=
      lambda_class_name_set_.end(); ++collision) {
    ostringstream suffix;
    suffix << collision;
    result = base.str() + "__ov" + suffix.str();
  }
  lambda_class_name_set_.insert(result);
  lambda_class_names_[lambda.get()] = result;
  if(lambda && lambda->source_token_begin != static_cast<size_t>(-1) &&
     lambda->source_token_end != static_cast<size_t>(-1))
    lambda_class_names_by_span_[make_pair(lambda->source_token_begin,
      lambda->source_token_end)] = result;
  return result;
}

namespace {

string LambdaReplayIdentity(const map<string, string>& substitutions)
{
  ostringstream spelling;
  for(map<string, string>::const_iterator substitution = substitutions.begin();
      substitution != substitutions.end(); ++substitution) {
    if(substitution->first.empty() || substitution->second.empty()) continue;
    spelling << '|' << substitution->first << '=' <<
      CanonicalSpelling(substitution->second);
  }
  return spelling.str();
}

}

CPPGMAstNodePtr PA18TemplateExpander::BuildLambdaClass(
  const CPPGMAstNodePtr& lambda, const string& class_name) const
{
  CPPGMAstNodePtr result(new CPPGMAstNode("class-specifier", class_name));
  result->source_token_begin = lambda ? lambda->source_token_begin : static_cast<size_t>(-1);
  result->source_token_end = lambda ? lambda->source_token_end : static_cast<size_t>(-1);
  result->children.push_back(CPPGMAstNodePtr(
    new CPPGMAstNode("class-key", "KW_STRUCT:struct")));

  CPPGMAstNodePtr function(new CPPGMAstNode("function-definition"));
  CPPGMAstNodePtr specifiers(new CPPGMAstNode("decl-specifier-seq"));
  specifiers->children.push_back(CPPGMAstNodePtr(
    new CPPGMAstNode("decl-specifier", "KW_AUTO:auto")));
  CPPGMAstNodePtr declarator(new CPPGMAstNode("declarator"));
  declarator->children.push_back(CPPGMAstNodePtr(
    new CPPGMAstNode("identifier", "operator()")));
  const CPPGMAstNodePtr clause = LambdaParameterClauseNode(lambda);
  // The source grammar omits the lambda-declarator entirely for a lambda
  // without an explicit parameter list (`[&] { ... }`).  Its call operator
  // still has an empty parameter-clause; retaining that fact is required for
  // the synthetic member definition to be recognized as a function.
  if(clause) declarator->children.push_back(CloneNode(clause));
  else declarator->children.push_back(CPPGMAstNodePtr(
    new CPPGMAstNode("parameter-clause")));
  const CPPGMAstNodePtr trailing = LambdaTrailingReturnNode(lambda);
  if(trailing) declarator->children.push_back(CloneNode(trailing));
  // A non-mutable lambda has a const call operator.  This matters when a
  // closure is passed through a `const F&` template parameter.
  declarator->children.push_back(CPPGMAstNodePtr(
    new CPPGMAstNode("cv-qualifier", "KW_CONST:const")));
  function->children.push_back(specifiers);
  function->children.push_back(declarator);
  const CPPGMAstNodePtr body = LambdaBodyNode(lambda);
  if(body) function->children.push_back(CloneNode(body));
  function->source_token_begin = result->source_token_begin;
  function->source_token_end = result->source_token_end;
  result->children.push_back(function);
  return result;
}

void PA18TemplateExpander::CollectLambdaClasses(
  const CPPGMAstNodePtr& node, const string& context,
  const string& function_context, vector<CPPGMAstNodePtr>* generated,
  bool call_argument, bool assignment_rhs, bool template_context)
{
  if(!node) return;
  const bool child_template_context = template_context ||
    node->kind == "template-declaration";
  string child_context = context;
  string child_function_context = function_context;
  if(node->kind == "namespace-definition") {
    child_context = node->value.empty() ? context : JoinPath(context, node->value);
  } else if(node->kind == "class-specifier" ||
            node->kind == "class-forward-declaration") {
    child_context = JoinPath(context, LastComponent(node->value));
  } else if(node->kind == "function-definition") {
    child_function_context = FunctionContextForLambda(node, context,
      function_context);
  }
  if(node->kind == "lambda-expression") {
    const bool body_class = LambdaBodyContainsClass(LambdaBodyNode(node));
    const bool needs_class = call_argument || assignment_rhs ||
      LambdaHasCaptureNode(node) || body_class;
	if(needs_class) {
		// Rewriting may promote a function-local class out of the lambda body.
		// Preserve the fact that the closure still carries that local type's
		// identity for PA14's typed lambda materialization decision.
		if(body_class) {
			node->materialize_object_address = true;
			// A local class declaration inside a lambda can have lifetime effects
			// even when its value is never read.  Mark its automatic object
			// declarations so PA14 evaluates the object address while retaining the
			// existing demand-driven constructor selection.
			function<void(const CPPGMAstNodePtr&)> mark_local_objects;
			mark_local_objects = [&](const CPPGMAstNodePtr& current) {
				if(!current) return;
				if(current->kind == "simple-declaration" && !current->children.empty() &&
					!HasDeclarationSpecifier(current->children[0], "typedef"))
					current->materialize_object_address = true;
				for(size_t child = 0; child < current->children.size(); ++child)
					mark_local_objects(current->children[child]);
			};
			mark_local_objects(LambdaBodyNode(node));
		}
      const string class_name = LambdaClassName(node, child_function_context);
      string capture_context = child_function_context;
      const string operator_suffix = "::operator()";
      if(capture_context.size() > operator_suffix.size() &&
         capture_context.compare(capture_context.size() - operator_suffix.size(),
           operator_suffix.size(), operator_suffix) == 0) {
        const string parent = capture_context.substr(0,
          capture_context.size() - operator_suffix.size());
        map<string, string>::const_iterator inherited =
          lambda_capture_contexts_.find(parent);
        if(inherited != lambda_capture_contexts_.end())
          capture_context = inherited->second;
      }
      lambda_capture_contexts_[class_name] = capture_context;
      lambda_source_nodes_[class_name] = node;
      if(template_context) lambda_deferred_classes_.insert(class_name);
      if(generated && !template_context &&
         lambda_class_names_.find(node.get()) != lambda_class_names_.end())
        generated->push_back(BuildLambdaClass(node, class_name));
      child_function_context = class_name + "::operator()";
    }
  }
  for(size_t child = 0; child < node->children.size(); ++child) {
    const bool child_call_argument = call_argument ||
      node->kind == "argument-list" || node->kind == "paren-argument-list";
    const bool child_assignment_rhs = assignment_rhs ||
      (node->kind == "assignment-expression" && child == 1);
    CollectLambdaClasses(node->children[child], child_context,
      child_function_context, generated, child_call_argument,
      child_assignment_rhs, child_template_context);
  }
}

void PA18TemplateExpander::PrepareLambdaClasses(vector<CPPGMAstNodePtr>* trees)
{
  lambda_class_names_.clear();
  lambda_class_names_by_span_.clear();
  lambda_class_name_set_.clear();
  lambda_capture_contexts_.clear();
  lambda_source_nodes_.clear();
  lambda_deferred_classes_.clear();
  lambda_replay_names_.clear();
  lambda_replay_bases_.clear();
  active_lambda_replay_names_.clear();
  next_lambda_serial_ = 0;
  if(!trees) return;
  for(size_t tree = 0; tree < trees->size(); ++tree) {
    CPPGMAstNodePtr& input = (*trees)[tree];
    if(!input || input->kind != "translation-unit") continue;
    vector<CPPGMAstNodePtr> generated;
    CollectLambdaClasses(input, string(), string(), &generated);
    input->children.insert(input->children.end(), generated.begin(), generated.end());
  }
}

vector<CPPGMAstNodePtr> PA18TemplateExpander::PrepareLambdaWorkingTrees(
  const vector<CPPGMAstNodePtr>& input)
{
  bool has_lambda = false;
  for(size_t i = 0; i < input.size(); ++i)
    has_lambda = has_lambda || ContainsLambdaExpression(input[i]);
  vector<CPPGMAstNodePtr> working = input;
  if(!has_lambda) {
    PrepareLambdaClasses(&working);
    return working;
  }
  working.clear();
  for(size_t i = 0; i < input.size(); ++i) working.push_back(CloneNode(input[i]));
  PrepareLambdaClasses(&working);
  return working;
}

void PA18TemplateExpander::CollectExplicitLambdaCaptures(
  const CPPGMAstNodePtr& lambda, vector<LambdaCaptureSpec>* captures,
  set<string>* names, bool* has_default, bool* default_reference) const
{
  if(!lambda || !captures || !names || !has_default || !default_reference ||
     lambda->children.empty() || !lambda->children[0]) return;
  const CPPGMAstNodePtr introducer = lambda->children[0];
  for(size_t child = 0; child < introducer->children.size(); ++child) {
    const CPPGMAstNodePtr capture = introducer->children[child];
    if(!capture || capture->kind != "lambda-capture") continue;
    for(size_t item = 0; item < capture->children.size(); ++item) {
      const CPPGMAstNodePtr entry = capture->children[item];
      if(!entry) continue;
      if(entry->kind == "capture-default") {
        *has_default = true;
        *default_reference = RemoveMarker(entry->value) == "&";
      } else if(entry->kind == "capture-list") {
        for(size_t listed = 0; listed < entry->children.size(); ++listed) {
          const CPPGMAstNodePtr explicit_capture = entry->children[listed];
          if(!explicit_capture || explicit_capture->kind != "capture") continue;
          const string name = CaptureNameFromNode(explicit_capture);
          const bool is_this = name == "this";
          bool by_reference = false;
          for(size_t marker = 0; marker < explicit_capture->children.size(); ++marker)
            if(explicit_capture->children[marker] &&
               explicit_capture->children[marker]->kind == "capture-reference")
              by_reference = true;
          if(!is_this && names->insert(name).second)
            captures->push_back(LambdaCaptureSpec(name, false, by_reference));
          else if(is_this && names->insert("this").second)
            captures->push_back(LambdaCaptureSpec("this", true, false));
        }
      }
    }
  }
}

void PA18TemplateExpander::AppendDefaultLambdaCaptures(
  const CPPGMAstNodePtr& lambda, const string& context,
  const set<string>& declared_variables, vector<LambdaCaptureSpec>* captures,
  set<string>* names, bool default_reference) const
{
  if(!lambda || !captures || !names) return;
  set<string> owner_member_names;
  const string owner_context = PrefixComponent(context);
  const CPPGMAstNodePtr owner = owner_context.empty() ? CPPGMAstNodePtr() :
    FindClassDeclaration(owner_context, context);
  if(owner) for(size_t child = 0; child < owner->children.size(); ++child) {
    CPPGMAstNodePtr member = owner->children[child];
    if(!member) continue;
    while(member->kind == "template-declaration" && member->children.size() > 1)
      member = member->children[1];
    if(member->kind == "function-definition" ||
       member->kind == "special-member-definition")
      owner_member_names.insert(LastComponent(FirstIdentifierLocal(
        member->children.size() > 1 ? member->children[1] : member)));
  }
  set<string> declarations;
  CollectLambdaDeclarations(LambdaParameterClauseNode(lambda), &declarations);
  CollectLambdaDeclarations(LambdaBodyNode(lambda), &declarations);
  vector<string> references;
  set<string> seen_references;
  CollectLambdaReferences(LambdaBodyNode(lambda), &references, &seen_references);
  const map<string, map<string, string> >::const_iterator scoped =
    function_parameter_types_.find(context);
  for(size_t reference = 0; reference < references.size(); ++reference) {
    const string& name = references[reference];
    if(name == "this" || owner_member_names.find(name) != owner_member_names.end()) {
      if(names->insert("this").second)
        captures->push_back(LambdaCaptureSpec("this", true, false));
      continue;
    }
    if(declarations.find(name) != declarations.end()) continue;
    const bool known = (scoped != function_parameter_types_.end() &&
      scoped->second.find(name) != scoped->second.end()) ||
      declared_variables.find(name) != declared_variables.end();
    if(known && names->insert(name).second)
      captures->push_back(LambdaCaptureSpec(name, false, default_reference));
  }
}

void PA18TemplateExpander::ExpandLambdaCapturePacks(
  vector<LambdaCaptureSpec>* captures) const
{
  if(!captures) return;
  vector<LambdaCaptureSpec> expanded;
  for(size_t capture = 0; capture < captures->size(); ++capture) {
    const LambdaCaptureSpec& item = (*captures)[capture];
    map<string, vector<string> >::const_iterator identifiers =
      active_pack_identifier_substitutions_.find(item.name);
    if(identifiers == active_pack_identifier_substitutions_.end() ||
       identifiers->second.empty()) {
      expanded.push_back(item);
      continue;
    }
    map<string, vector<string> >::const_iterator types =
      active_function_pack_substitutions_.find(item.name);
    for(size_t element = 0; element < identifiers->second.size(); ++element) {
      LambdaCaptureSpec item_copy = item;
      item_copy.name = identifiers->second[element];
      if(types != active_function_pack_substitutions_.end() &&
         element < types->second.size()) item_copy.type = types->second[element];
      expanded.push_back(item_copy);
    }
  }
  captures->swap(expanded);
}

vector<LambdaCaptureSpec> PA18TemplateExpander::CollectLambdaCaptureSpecs(
  const CPPGMAstNodePtr& lambda, const string& context,
  const set<string>& declared_variables) const
{
  vector<LambdaCaptureSpec> captures;
  set<string> names;
  bool has_default = false;
  bool default_reference = true;
  CollectExplicitLambdaCaptures(lambda, &captures, &names, &has_default,
    &default_reference);
  if(has_default) AppendDefaultLambdaCaptures(lambda, context,
    declared_variables, &captures, &names, default_reference);
  ExpandLambdaCapturePacks(&captures);
  return captures;
}

void PA18TemplateExpander::ResolveLambdaCaptureTypes(
  const CPPGMAstNodePtr& tree, const string& context,
  vector<LambdaCaptureSpec>* captures) const
{
  if(!tree || !captures) return;
  function<CPPGMAstNodePtr(const string&)> find_initializer;
  find_initializer = [&](const string& name) -> CPPGMAstNodePtr {
    CPPGMAstNodePtr result;
    function<void(const CPPGMAstNodePtr&)> visit;
    visit = [&](const CPPGMAstNodePtr& node) {
      if(!node || result) return;
      if(node->kind == "simple-declaration") {
        const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
        if(list) for(size_t item = 0; item < list->children.size(); ++item) {
          const CPPGMAstNodePtr declarator = list->children[item];
          if(!declarator || declarator->children.empty() ||
             LastComponent(FirstIdentifierLocal(declarator->children[0])) != name) continue;
          if(declarator->children.size() > 1)
            result = DescendantOfKind(declarator->children[1], "lambda-expression");
          if(result) return;
        }
      }
      for(size_t child = 0; child < node->children.size(); ++child) visit(node->children[child]);
    };
    visit(tree);
    return result;
  };
  for(size_t capture = 0; capture < captures->size(); ++capture) {
    LambdaCaptureSpec& item = (*captures)[capture];
    if(item.this_capture) { item.type = PrefixComponent(context); continue; }
    map<string, map<string, string> >::const_iterator scoped =
      function_parameter_types_.find(context);
    if(scoped != function_parameter_types_.end()) {
      map<string, string>::const_iterator found = scoped->second.find(item.name);
      if(found != scoped->second.end()) item.type = found->second;
    }
    if(item.type.empty()) {
      map<string, string>::const_iterator found = variable_types_.find(item.name);
      if(found != variable_types_.end()) item.type = found->second;
    }
    if(IsAutoSpelling(item.type)) item.function_source = find_initializer(item.name);
    if(IsAutoSpelling(item.type) && item.function_source) item.type.clear();
  }
}

void PA18TemplateExpander::InstallLambdaCaptureFields(
  const CPPGMAstNodePtr& class_node, const string& context,
  vector<LambdaCaptureSpec>* captures) const
{
  if(!class_node || !captures) return;
  vector<CPPGMAstNodePtr> fields;
  for(size_t capture = 0; capture < captures->size(); ++capture)
    fields.push_back((*captures)[capture].function_source ?
      MakeFunctionCaptureField((*captures)[capture], (*captures)[capture].function_source) :
      MakeSimpleCaptureField((*captures)[capture]));
  size_t function_position = class_node->children.size();
  for(size_t child = 0; child < class_node->children.size(); ++child)
    if(class_node->children[child] &&
       class_node->children[child]->kind == "function-definition") {
      function_position = child;
      break;
    }
  if(function_position >= class_node->children.size()) return;
  class_node->children.insert(class_node->children.begin() + function_position,
    fields.begin(), fields.end());
  const CPPGMAstNodePtr operator_node = class_node->children[
    function_position + fields.size()];
  const CPPGMAstNodePtr body = LambdaBodyNode(operator_node);
  RewriteCapturedLambdaBody(body, *captures);
  const string owner = PrefixComponent(context);
  if(!HasCapture(*captures, "this") || owner.empty()) return;
  const CPPGMAstNodePtr owner_declaration = FindClassDeclaration(owner, context);
  set<string> member_names;
  if(owner_declaration) for(size_t child = 0;
      child < owner_declaration->children.size(); ++child) {
    const CPPGMAstNodePtr member = owner_declaration->children[child];
    if(!member) continue;
    CPPGMAstNodePtr declaration = member;
    while(declaration->kind == "template-declaration" && declaration->children.size() > 1)
      declaration = declaration->children[1];
    if(declaration->kind == "function-definition" ||
       declaration->kind == "special-member-definition")
      member_names.insert(LastComponent(FirstIdentifierLocal(
        declaration->children.size() > 1 ? declaration->children[1] : declaration)));
  }
  const LambdaCaptureSpec* this_spec = FindCapture(*captures, "this");
  function<void(const CPPGMAstNodePtr&)> qualify_members;
  qualify_members = [&](const CPPGMAstNodePtr& node) {
    if(!node || node->kind == "lambda-expression") return;
    for(size_t child = 0; child < node->children.size(); ++child) {
      CPPGMAstNodePtr& current = node->children[child];
      if(!current) continue;
      if(current->kind == "id-expression" && this_spec &&
         member_names.find(LastComponent(RemoveMarker(current->value))) != member_names.end()) {
        CPPGMAstNodePtr qualified(new CPPGMAstNode("member-expression", "OP_ARROW:->"));
        qualified->children.push_back(CaptureMemberExpression(*this_spec));
        qualified->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
          "identifier", LastComponent(RemoveMarker(current->value)))));
        current = qualified;
        continue;
      }
      qualify_members(current);
    }
  };
  qualify_members(body);
}

void PA18TemplateExpander::PrepareLambdaClassFields(
  const CPPGMAstNodePtr& tree)
{
  if(!tree) return;
  map<string, CPPGMAstNodePtr> classes;
  function<void(const CPPGMAstNodePtr&)> index_classes;
  index_classes = [&](const CPPGMAstNodePtr& node) {
    if(!node) return;
    if(node->kind == "class-specifier" &&
       LastComponent(node->value).compare(0, 9, "__lambda_") == 0)
      classes[LastComponent(node->value)] = node;
    for(size_t child = 0; child < node->children.size(); ++child)
      index_classes(node->children[child]);
  };
  index_classes(tree);
  set<string> declared_variables;
  CollectVariableNames(tree, &declared_variables);
  for(map<string, CPPGMAstNodePtr>::const_iterator source =
      lambda_source_nodes_.begin(); source != lambda_source_nodes_.end(); ++source) {
    map<string, CPPGMAstNodePtr>::const_iterator class_it = classes.find(source->first);
    if(class_it == classes.end() || !class_it->second || !source->second) continue;
    const string context = lambda_capture_contexts_[source->first];
    vector<LambdaCaptureSpec> captures = CollectLambdaCaptureSpecs(source->second,
      context, declared_variables);
    ResolveLambdaCaptureTypes(tree, context, &captures);
    InstallLambdaCaptureFields(class_it->second, context, &captures);
  }
}

void PA18TemplateExpander::MaterializeDeferredLambdaClasses(
  const CPPGMAstNodePtr& source, const string& context,
  const map<string, string>& substitutions)
{
  if(!source || lambda_deferred_classes_.empty()) return;
  vector<pair<string, CPPGMAstNodePtr> > lambdas;
  set<const CPPGMAstNode*> seen;
  function<void(const CPPGMAstNodePtr&)> collect;
  collect = [&](const CPPGMAstNodePtr& node) {
    if(!node) return;
    if(node->kind == "lambda-expression" && seen.insert(node.get()).second) {
      map<const CPPGMAstNode*, string>::const_iterator name =
        lambda_class_names_.find(node.get());
      if(name != lambda_class_names_.end() &&
         lambda_deferred_classes_.find(name->second) !=
           lambda_deferred_classes_.end() &&
         !HasUnresolvedTemplateParameter(SpellNode(node), context,
           substitutions))
        lambdas.push_back(make_pair(name->second, node));
    }
    for(size_t child = 0; child < node->children.size(); ++child)
      collect(node->children[child]);
  };
  collect(source);
  if(lambdas.empty()) return;

  // The deferred shell is transformed before the owning function's parameter
  // clause is replayed.  Install the concrete parameter identifiers for this
  // short replay window so pack-expanded captured member expressions and the
  // closure fields use the same element names.
  const map<string, vector<string> > saved_pack_identifiers =
    active_pack_identifier_substitutions_;
  const CPPGMAstNodePtr source_parameters = DescendantOfKind(
    FunctionDeclarator(source), "parameter-clause");
  if(source_parameters) for(size_t parameter = 0;
      parameter < source_parameters->children.size(); ++parameter) {
    const CPPGMAstNodePtr parameter_node = source_parameters->children[parameter];
    if(!parameter_node || parameter_node->kind != "parameter-declaration" ||
       !IsFunctionParameterPack(parameter_node)) continue;
    const string identifier = ParameterIdentifier(parameter_node);
    if(identifier.empty()) continue;
    map<string, vector<string> >::const_iterator values =
      active_function_pack_substitutions_.find(identifier);
    if(values == active_function_pack_substitutions_.end()) continue;
    vector<string>& expanded = active_pack_identifier_substitutions_[identifier];
    expanded.clear();
    for(size_t element = 0; element < values->second.size(); ++element) {
      ostringstream suffix;
      suffix << element + 1;
      expanded.push_back(element == 0 ? identifier :
        identifier + "__pack" + suffix.str());
    }
  }

  CPPGMAstNodePtr preparation(new CPPGMAstNode("translation-unit"));
  vector<CPPGMAstNodePtr> shells;
  const string replay_identity = LambdaReplayIdentity(substitutions);
  const string replay_suffix = replay_identity.empty() ? string() :
    TypeSuffix(replay_identity);
  for(size_t lambda = 0; lambda < lambdas.size(); ++lambda) {
    const string base_name = lambdas[lambda].first;
    string class_name = base_name;
    if(!replay_suffix.empty()) {
      const string replay_key = base_name + "|" + replay_suffix;
      map<string, string>::const_iterator replay = lambda_replay_names_.find(
        replay_key);
      if(replay != lambda_replay_names_.end()) class_name = replay->second;
      else if(lambda_replay_bases_.insert(base_name).second) {
        class_name = base_name;
        lambda_replay_names_[replay_key] = class_name;
      }
      else {
        class_name += "__inst_" + replay_suffix;
        lambda_replay_names_[replay_key] = class_name;
        lambda_class_name_set_.insert(class_name);
      }
    }
    lambda_capture_contexts_[class_name] = lambda_capture_contexts_[base_name];
    lambda_source_nodes_[class_name] = lambdas[lambda].second;
    if(lambdas[lambda].second->source_token_begin != static_cast<size_t>(-1) &&
       lambdas[lambda].second->source_token_end != static_cast<size_t>(-1))
      active_lambda_replay_names_[make_pair(
        lambdas[lambda].second->source_token_begin,
        lambdas[lambda].second->source_token_end)] = class_name;
    CPPGMAstNodePtr shell = BuildLambdaClass(lambdas[lambda].second,
      class_name);
    if(class_name != base_name) {
      const size_t synthetic_source = (static_cast<size_t>(1) << 60) +
        next_lambda_serial_++;
      shell->source_token_begin = synthetic_source;
      shell->source_token_end = synthetic_source;
      if(shell->children.size() > 1 && shell->children[1]) {
        shell->children[1]->source_token_begin = synthetic_source;
        shell->children[1]->source_token_end = synthetic_source;
      }
    }
    shells.push_back(shell);
    preparation->children.push_back(shell);
  }
  // Include the source function while preparing default captures so local
  // declarations and their typed facts remain visible to the capture pass.
  preparation->children.push_back(CloneNode(source));
  PrepareLambdaClassFields(preparation);
	// The deferred closure is needed while its owning function's trailing return
	// type is being replayed.  InjectGenerated runs only after that transformation,
	// so register the prepared shell's class and operator signature immediately;
	// this lets decltype(std::declval<closure>()(...)) perform ordinary typed
	// callable lookup during the same instantiation.
  for(size_t shell = 0; shell < shells.size(); ++shell)
			Collect(shells[shell], string());

  for(size_t shell = 0; shell < shells.size(); ++shell) {
    CPPGMAstNodePtr transformed = TransformNode(shells[shell], context,
      substitutions);
    if(transformed) deferred_generated_by_owner_[string()].push_back(transformed);
  }
  active_pack_identifier_substitutions_ = saved_pack_identifiers;
}

} // namespace pa18_templates_internal
