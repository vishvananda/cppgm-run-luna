#include "pa11_semantics_model.h"

using namespace std;

Type::Type(TypeKind type_kind, const string& type_name)
	: kind(type_kind), name(type_name), tag(), scoped_enum(false), complete(true),
	  underlying_explicit(false), is_const(false), is_volatile(false), child(), bound(-1),
	  parameters(), variadic(false), function_const(false), function_volatile(false),
	  function_lvalue_ref_qualified(false), function_rvalue_ref_qualified(false),
	  member_owner(), owned_scope(0),
	  underlying(), class_members(), direct_base(), direct_bases(), direct_base_offset(0),
	  direct_base_offsets(), object_size(0), object_alignment(1),
	  explicit_alignment(0), layout_complete(false), layout_in_progress(false),
	  is_union(false), enclosing_type(), dependent_base_lookup(false),
	  friend_access(), virtual_methods(),
	polymorphic(false), has_vpointer(false), template_specialization(false),
	  template_primary(), template_arguments(), template_parameter_names(),
	  template_parameter_packs(), template_empty_pack(false), has_deferred_constructor(false),
	  materialize_sizeof_address(false) {}

Binding::Binding(BindingKind binding_kind, const string& binding_name,
	const TypePtr& binding_type)
	: kind(binding_kind), name(binding_name), type(binding_type), has_value(false),
	  suppress_dump(false),
	  constant_value(), value(0),
	  type_override(), qualified_name(), injected_member(false), injected_object_name(),
	  injected_owner(), hidden_friend(false), friend_owner(), is_member(false),
	  is_static(false), is_virtual(false), is_pure(false), is_override(false),
	  is_final(false), noexcept_specified(false), member_owner(),
	  member_index(static_cast<size_t>(-1)),
	  access(), declaration() {}

Scope::Scope(ScopeKind scope_kind, const string& scope_name, Scope* scope_parent)
	: kind(scope_kind), name(scope_name), parent(scope_parent), owner_type(), inline_namespace(false),
	  bindings(), local_bindings(), children(), namespace_children(), namespace_aliases(),
	  using_directives(), qualified_prefix(scope_parent ? scope_parent->qualified_prefix : string())
{
	if (scope_parent && (scope_kind == SCOPE_NAMESPACE || scope_kind == SCOPE_CLASS) &&
		scope_name != "<unnamed>" && !scope_name.empty())
	{
		if (!qualified_prefix.empty()) qualified_prefix += "::";
		qualified_prefix += scope_name;
	}
}

Binding* Scope::local(const string& key)
{
	map<string, size_t>::iterator found = local_bindings.find(key);
	return found == local_bindings.end() ? 0 : &bindings[found->second];
}

const Binding* Scope::local(const string& key) const
{
	map<string, size_t>::const_iterator found = local_bindings.find(key);
	return found == local_bindings.end() ? 0 : &bindings[found->second];
}

Binding* Scope::add(Binding binding)
{
	if (binding.qualified_name.empty())
		binding.qualified_name = qualified_prefix.empty() ? binding.name :
			qualified_prefix + "::" + binding.name;
	const size_t index = bindings.size();
	bindings.push_back(binding);
	local_bindings[binding.name] = index;
	return &bindings[index];
}

Scope* Scope::child(ScopeKind child_kind, const string& child_name)
{
	children.push_back(unique_ptr<Scope>(new Scope(child_kind, child_name, this)));
	return children.back().get();
}

ConstantValue::ConstantValue(bool is_known, long long constant)
	: kind(is_known ? CONSTANT_INTEGRAL : CONSTANT_UNKNOWN),
	  integral(is_known ? PA19IntegralValue::Signed(constant, "long long", 64) :
		PA19IntegralValue()), value(constant), floating_known(false), floating(0),
	  type(), object(), pointer() {}

string LastComponent(const string& name)
{
	const size_t separator = name.rfind("::");
	return separator == string::npos ? name : name.substr(separator + 2);
}

string StripTypeMarker(const string& name)
{
	const string marker = "TT_IDENTIFIER:";
	if (name.compare(0, marker.size(), marker) == 0)
		return name.substr(marker.size());
	return name;
}

TypePtr Fundamental(const string& name)
{
	return TypePtr(new Type(TYPE_FUNDAMENTAL, name));
}

TypePtr CloneWithCv(const TypePtr& original, bool add_const, bool add_volatile)
{
	if (!original) return original;
	if (!add_const && !add_volatile) return original;
	if (original->kind == TYPE_LVALUE_REFERENCE || original->kind == TYPE_RVALUE_REFERENCE)
		return original;
	if (original->kind == TYPE_ARRAY)
	{
		TypePtr result(new Type(TYPE_ARRAY));
		result->bound = original->bound;
		result->child = CloneWithCv(original->child, add_const, add_volatile);
		return result;
	}
	TypePtr result(new Type(*original));
	result->is_const = result->is_const || add_const;
	result->is_volatile = result->is_volatile || add_volatile;
	return result;
}

TypePtr PointerTo(const TypePtr& pointee)
{
	if (pointee && (pointee->kind == TYPE_LVALUE_REFERENCE ||
		pointee->kind == TYPE_RVALUE_REFERENCE))
		throw logic_error("pointer to reference is invalid");
	TypePtr result(new Type(TYPE_POINTER));
	result->child = pointee;
	return result;
}

TypePtr ReferenceTo(TypeKind reference_kind, const TypePtr& referred)
{
	if (referred && (referred->kind == TYPE_LVALUE_REFERENCE ||
		referred->kind == TYPE_RVALUE_REFERENCE) &&
		(reference_kind == TYPE_LVALUE_REFERENCE ||
		 referred->kind == TYPE_LVALUE_REFERENCE))
		return TypePtr(new Type(*referred));
	TypePtr result(new Type(reference_kind));
	result->child = referred;
	return result;
}

TypePtr ArrayOf(long long bound, const TypePtr& element)
{
	TypePtr result(new Type(TYPE_ARRAY));
	result->bound = bound;
	result->child = element;
	return result;
}

TypePtr FunctionOf(const vector<TypePtr>& parameters, bool variadic,
	const TypePtr& result_type, bool function_const, bool function_volatile,
	bool function_lvalue_ref_qualified, bool function_rvalue_ref_qualified)
{
	TypePtr result(new Type(TYPE_FUNCTION));
	result->parameters = parameters;
	result->variadic = variadic;
	result->function_const = function_const;
	result->function_volatile = function_volatile;
	result->function_lvalue_ref_qualified = function_lvalue_ref_qualified;
	result->function_rvalue_ref_qualified = function_rvalue_ref_qualified;
	result->child = result_type;
	return result;
}

TypePtr MemberPointerTo(const TypePtr& owner, const TypePtr& target)
{
	TypePtr result(new Type(TYPE_MEMBER_POINTER));
	result->member_owner = owner;
	result->child = target;
	return result;
}

string CvPrefix(const TypePtr& type)
{
	if (!type) return string();
	string result;
	if (type->is_const) result += "const ";
	if (type->is_volatile) result += "volatile ";
	return result;
}

string TypeText(const TypePtr& type, bool extended)
{
	if (!type) return "<invalid type>";
	string result = CvPrefix(type);
	switch (type->kind)
	{
	case TYPE_FUNDAMENTAL: return result + type->name;
	case TYPE_CLASS: return result + type->tag + " " +
		(extended ? type->name : LastComponent(type->name));
	case TYPE_ENUM: return result + string(type->scoped_enum ? "enum class " : "enum ") +
		(extended ? type->name : LastComponent(type->name));
	case TYPE_TEMPLATE_PARAMETER: return result + "typename " + type->name;
	case TYPE_TEMPLATE_TEMPLATE_PARAMETER: return result + "template-parameter " + type->name;
	case TYPE_POINTER: return result + "pointer to " + TypeText(type->child, extended);
	case TYPE_LVALUE_REFERENCE: return result + "lvalue-reference to " + TypeText(type->child, extended);
	case TYPE_RVALUE_REFERENCE: return result + "rvalue-reference to " + TypeText(type->child, extended);
	case TYPE_ARRAY:
		{
			ostringstream bound;
			bound << type->bound;
			return result + "array of " + bound.str() + " " + TypeText(type->child, extended);
		}
	case TYPE_FUNCTION:
		{
			ostringstream output;
			output << result << "function of (";
			for (size_t i = 0; i < type->parameters.size(); ++i)
			{
				if (i != 0) output << ", ";
				output << TypeText(type->parameters[i], extended);
			}
			if (type->variadic)
			{
				if (!type->parameters.empty()) output << ", ";
				output << "...";
			}
			output << ")";
			if (extended && type->function_const) output << " const";
			if (extended && type->function_volatile) output << " volatile";
			if (extended && type->function_lvalue_ref_qualified) output << " &";
			if (extended && type->function_rvalue_ref_qualified) output << " &&";
			output << " returning " << TypeText(type->child, extended);
			return output.str();
		}
	case TYPE_MEMBER_POINTER:
		if (!extended) return result + "pointer to " + TypeText(type->child, false);
		return result + "member-pointer of " + TypeText(type->member_owner, true) +
			" to " + TypeText(type->child, true);
	}
	return "<invalid type>";
}

bool SameTypeIgnoringTopCv(const TypePtr& left, const TypePtr& right)
{
	if (left == right) return true;
	if (!left || !right || left->kind != right->kind) return false;
	Type left_copy = *left;
	Type right_copy = *right;
	left_copy.is_const = left_copy.is_volatile = false;
	right_copy.is_const = right_copy.is_volatile = false;
	return TypeText(TypePtr(new Type(left_copy)), true) ==
		TypeText(TypePtr(new Type(right_copy)), true);
}

string ScopeKindText(ScopeKind kind)
{
	switch (kind)
	{
	case SCOPE_NAMESPACE: return "namespace";
	case SCOPE_TEMPLATE_PARAMETERS: return "template-parameters";
	case SCOPE_CLASS: return "class";
	case SCOPE_ENUM: return "enum";
	case SCOPE_FUNCTION: return "function";
	case SCOPE_BLOCK: return "block";
	}
	return "unknown";
}

string BindingKindText(BindingKind kind)
{
	switch (kind)
	{
	case BIND_TYPE: return "type";
	case BIND_TYPE_ALIAS: return "type-alias";
	case BIND_ENUMERATOR: return "enumerator";
	case BIND_FUNCTION: return "function";
	case BIND_VARIABLE: return "variable";
	case BIND_PARAMETER: return "parameter";
	}
	return "variable";
}

string OperatorFromNode(const string& value)
{
	const size_t separator = value.find(':');
	return separator == string::npos ? value : value.substr(separator + 1);
}

bool HasKind(const CPPGMAstNodePtr& node, const string& kind)
{
	if (!node) return false;
	for (size_t i = 0; i < node->children.size(); ++i)
		if (node->children[i] && node->children[i]->kind == kind) return true;
	return false;
}

CPPGMAstNodePtr ChildOfKind(const CPPGMAstNodePtr& node, const string& kind)
{
	if (!node) return CPPGMAstNodePtr();
	for (size_t i = 0; i < node->children.size(); ++i)
		if (node->children[i] && node->children[i]->kind == kind) return node->children[i];
	return CPPGMAstNodePtr();
}

CPPGMAstNodePtr DescendantOfKind(const CPPGMAstNodePtr& node, const string& kind)
{
	if (!node) return CPPGMAstNodePtr();
	if (node->kind == kind) return node;
	for (size_t i = 0; i < node->children.size(); ++i)
	{
		CPPGMAstNodePtr found = DescendantOfKind(node->children[i], kind);
		if (found) return found;
	}
	return CPPGMAstNodePtr();
}

string FirstIdentifier(const CPPGMAstNodePtr& node)
{
	if (!node) return string();
	if (node->kind == "identifier") return node->value;
	for (size_t i = 0; i < node->children.size(); ++i)
	{
		const string found = FirstIdentifier(node->children[i]);
		if (!found.empty()) return found;
	}
	return string();
}

string ClassKey(const CPPGMAstNodePtr& node)
{
	const CPPGMAstNodePtr key = ChildOfKind(node, "class-key");
	if (!key) return "struct";
	const size_t separator = key->value.rfind(':');
	return separator == string::npos ? key->value : key->value.substr(separator + 1);
}

bool IsScopedEnum(const CPPGMAstNodePtr& node)
{
	const CPPGMAstNodePtr key = ChildOfKind(node, "enum-key");
	return key && (key->value.find(":class") != string::npos ||
		key->value.find(":struct") != string::npos);
}
