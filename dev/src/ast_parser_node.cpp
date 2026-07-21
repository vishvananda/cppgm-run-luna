#include "ast_parser.h"

using namespace std;

CPPGMAstNode::CPPGMAstNode(const string& kind, const string& value)
	: kind(kind), value(value), initializer_form(AST_INITIALIZER_NONE),
	  template_instantiation(false), explicit_instantiation(false),
	  dependent_base_lookup(false),
	  materialize_object_address(false),
	  materialize_object_name(),
	  template_primary(), template_arguments(), children() {}
