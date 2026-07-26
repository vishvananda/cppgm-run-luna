#include "ast_parser.h"

using namespace std;

CPPGMAstNode::CPPGMAstNode(const string& kind, const string& value)
	: kind(kind), value(value), initializer_form(AST_INITIALIZER_NONE),
	  template_instantiation(false), explicit_specialization(false),
	  explicit_instantiation(false), extern_instantiation(false),
	  synthetic_namespace_forward(false), dependent_base_lookup(false),
	  materialize_object_address(false),
	  materialize_object_name(),
	 inferred_type(),
	 explicit_typename(false),
	  source_token_begin(static_cast<size_t>(-1)),
	  source_token_end(static_cast<size_t>(-1)),
	  template_primary(), template_arguments(), children() {}
