#include "ast_parser.h"

using namespace std;

CPPGMAstNode::CPPGMAstNode(const string& kind, const string& value)
	: kind(kind), value(value), initializer_form(AST_INITIALIZER_NONE),
	  template_instantiation(false), explicit_specialization(false),
	  explicit_instantiation(false), extern_instantiation(false),
	  synthetic_namespace_forward(false), dependent_base_lookup(false),
	  materialize_object_address(false),
	  has_deferred_constructor(false),
	  materialize_object_name(),
	 inferred_type(),
	 indirect_function_call(false),
	 explicit_typename(false),
	  source_token_begin(static_cast<size_t>(-1)),
	  source_token_end(static_cast<size_t>(-1)),
	  template_primary(), template_arguments(), template_empty_pack(false), children() {}
