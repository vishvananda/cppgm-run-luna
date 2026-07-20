#pragma once
#include "pa11_semantics.h"

#include <cerrno>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <deque>
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
struct ClassMemberInfo;
struct Binding;
typedef shared_ptr<Type> TypePtr;

// The PA15 object model keeps layout facts in the semantic type rather than
// rediscovering them from LowIR spelling.  This is deliberately small at the
// PA11/PA14 handoff; later stages can extend the member record with access and
// lifetime facts without changing the source AST boundary.
struct ClassMemberInfo
{
	string name;
	TypePtr type;
	long long offset;
	long long bit_offset;
	long long bit_width;
	bool bit_field;
	bool is_static;
	bool is_mutable;
	CPPGMAstNodePtr initializer;

	ClassMemberInfo()
		: name(), type(), offset(0), bit_offset(0), bit_width(0),
		  bit_field(false), is_static(false), is_mutable(false), initializer() {}
};

// A virtual slot is a semantic class fact.  It records the effective
// declaration for one logical slot after single-inheritance overrides have
// been applied; the LowIR layer expands a virtual destructor into its two
// ABI entries when it renders a vtable.
struct VirtualMethodInfo
{
	string name;
	TypePtr function;
	Binding* binding;
	TypePtr owner;
	bool destructor;
	bool pure;
	bool final;

	VirtualMethodInfo()
		: name(), function(), binding(0), owner(), destructor(false),
		  pure(false), final(false) {}
};

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
	bool function_volatile;
	bool function_lvalue_ref_qualified;
	bool function_rvalue_ref_qualified;
	TypePtr member_owner;
	Scope* owned_scope;
	TypePtr underlying;
	vector<ClassMemberInfo> class_members;
	TypePtr direct_base;
	// Offset of the direct base subobject within the complete object.  The
	// supported PA17 layout normally keeps a single-inheritance base at zero;
	// a class that introduces its first vpointer reserves offset zero for that
	// pointer and records the non-polymorphic base's adjusted address here.
	size_t direct_base_offset;
	size_t object_size;
	size_t object_alignment;
	size_t explicit_alignment;
	bool layout_complete;
	bool layout_in_progress;
	bool is_union;
	TypePtr enclosing_type;
	vector<string> friend_names;
	vector<VirtualMethodInfo> virtual_methods;
	bool polymorphic;
	bool has_vpointer;
	bool template_specialization;
	string template_primary;
	vector<string> template_arguments;

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
	// A friend declaration is introduced into the enclosing class scope for
	// semantic lookup, but it is not an ordinary member.  Keep its associated
	// class typed so PA15 ADL can find hidden friends without making them
	// visible to ordinary unqualified lookup.
	bool hidden_friend;
	TypePtr friend_owner;
	bool is_member;
	bool is_static;
	bool is_virtual;
	bool is_pure;
	bool is_override;
	bool is_final;
	// ClassMemberInfo is the canonical owner of layout and member-kind facts.
	// Bindings retain lookup identity and refer to that record through a stable
	// owner/index pair rather than a pointer into a relocatable member vector.
	TypePtr member_owner;
	size_t member_index;
	string access;
	CPPGMAstNodePtr declaration;

	Binding(BindingKind binding_kind = BIND_VARIABLE,
		const string& binding_name = string(), const TypePtr& binding_type = TypePtr());
};

struct Scope
{
	ScopeKind kind;
	string name;
	Scope* parent;
	// Class scopes retain their owning type for typed member lowering.
	TypePtr owner_type;
	bool inline_namespace;
	// Lowering may append synthesized constructors, destructors, and aggregate
	// bindings after expression facts have retained Binding pointers.  A deque
	// keeps those semantic identities stable while preserving indexed lookup.
	deque<Binding> bindings;
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
	const TypePtr& result_type, bool function_const = false,
	bool function_volatile = false,
	bool function_lvalue_ref_qualified = false,
	bool function_rvalue_ref_qualified = false);
TypePtr MemberPointerTo(const TypePtr& owner, const TypePtr& target);
string CvPrefix(const TypePtr& type);
string ScopeKindText(ScopeKind kind);
string BindingKindText(BindingKind kind);
string OperatorFromNode(const string& value);
bool HasKind(const CPPGMAstNodePtr& node, const string& kind);
CPPGMAstNodePtr ChildOfKind(const CPPGMAstNodePtr& node, const string& kind);
CPPGMAstNodePtr DescendantOfKind(const CPPGMAstNodePtr& node, const string& kind);
string FirstIdentifier(const CPPGMAstNodePtr& node);
string ClassKey(const CPPGMAstNodePtr& node);
bool IsScopedEnum(const CPPGMAstNodePtr& node);
