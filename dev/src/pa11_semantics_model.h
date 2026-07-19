#pragma once
#include "pa11_semantics.h"

#include <cerrno>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

struct Scope;
struct Type;
typedef shared_ptr<Type> TypePtr;

enum TypeKind
{
	TYPE_FUNDAMENTAL,
	TYPE_CLASS,
	TYPE_ENUM,
	TYPE_TEMPLATE_PARAMETER,
	TYPE_TEMPLATE_TEMPLATE_PARAMETER,
	TYPE_POINTER,
	TYPE_LVALUE_REFERENCE,
	TYPE_RVALUE_REFERENCE,
	TYPE_ARRAY,
	TYPE_FUNCTION,
	TYPE_MEMBER_POINTER
};

struct Type
{
	TypeKind kind;
	string name;
	string tag;
	bool scoped_enum;
	bool complete;
	bool underlying_explicit;
	bool is_const;
	bool is_volatile;
	TypePtr child;
	long long bound;
	vector<TypePtr> parameters;
	bool variadic;
	bool function_const;
	TypePtr member_owner;
	Scope* owned_scope;
	TypePtr underlying;

	Type(TypeKind type_kind = TYPE_FUNDAMENTAL,
		const string& type_name = string());
};

enum ScopeKind
{
	SCOPE_NAMESPACE,
	SCOPE_TEMPLATE_PARAMETERS,
	SCOPE_CLASS,
	SCOPE_ENUM,
	SCOPE_FUNCTION,
	SCOPE_BLOCK
};

enum BindingKind
{
	BIND_TYPE,
	BIND_TYPE_ALIAS,
	BIND_ENUMERATOR,
	BIND_FUNCTION,
	BIND_VARIABLE,
	BIND_PARAMETER
};

struct Binding
{
	BindingKind kind;
	string name;
	TypePtr type;
	bool has_value;
	long long value;
	string type_override;
	string qualified_name;
	bool injected_member;
	string injected_object_name;
	TypePtr injected_owner;

	Binding(BindingKind binding_kind = BIND_VARIABLE,
		const string& binding_name = string(), const TypePtr& binding_type = TypePtr());
};

struct Scope
{
	ScopeKind kind;
	string name;
	Scope* parent;
	bool inline_namespace;
	vector<Binding> bindings;
	map<string, size_t> local_bindings;
	vector<unique_ptr<Scope> > children;
	map<string, Scope*> namespace_children;
	map<string, Scope*> namespace_aliases;
	vector<Scope*> using_directives;
	string qualified_prefix;

	Scope(ScopeKind scope_kind, const string& scope_name, Scope* scope_parent);
	Binding* local(const string& key);
	const Binding* local(const string& key) const;
	Binding* add(Binding binding);
	Scope* child(ScopeKind child_kind, const string& child_name);
};

struct ConstantValue
{
	bool known;
	long long value;

	ConstantValue(bool is_known = false, long long constant = 0);
};

string LastComponent(const string& name);
string StripTypeMarker(const string& name);
string TypeText(const TypePtr& type, bool extended = false);
TypePtr Fundamental(const string& name);
TypePtr CloneWithCv(const TypePtr& original, bool add_const, bool add_volatile);
TypePtr PointerTo(const TypePtr& pointee);
TypePtr ReferenceTo(TypeKind reference_kind, const TypePtr& referred);
TypePtr ArrayOf(long long bound, const TypePtr& element);
TypePtr FunctionOf(const vector<TypePtr>& parameters, bool variadic,
	const TypePtr& result_type, bool function_const = false);
TypePtr MemberPointerTo(const TypePtr& owner, const TypePtr& target);
string CvPrefix(const TypePtr& type);
string ScopeKindText(ScopeKind kind);
string BindingKindText(BindingKind kind);
string OperatorFromNode(const string& value);
bool HasKind(const CPPGMAstNodePtr& node, const string& kind);
CPPGMAstNodePtr ChildOfKind(const CPPGMAstNodePtr& node, const string& kind);
string FirstIdentifier(const CPPGMAstNodePtr& node);
string ClassKey(const CPPGMAstNodePtr& node);
bool IsScopedEnum(const CPPGMAstNodePtr& node);
