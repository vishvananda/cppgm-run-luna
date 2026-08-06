#pragma once
#include "pa11_semantics.h"
#include "pa19_constants.h"

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
struct ConstantObject;
struct ConstantPointer;
typedef shared_ptr<Type> TypePtr;

// A friend edge is a semantic relationship, not a source spelling. Keep the
// entity kind and typed target alongside the lookup identity so access checks
// can validate the selected class/function entity.
struct FriendAccess
{
	enum Kind { FRIEND_FUNCTION, FRIEND_CLASS };
	Kind kind;
	string name;
	TypePtr target;

	FriendAccess(Kind access_kind = FRIEND_FUNCTION,
		const string& access_name = string(), const TypePtr& access_target = TypePtr())
		: kind(access_kind), name(access_name), target(access_target) {}
};

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

// Each polymorphic direct base contributes a typed vtable view.  The primary
// view is still mirrored in Type::virtual_methods for earlier PA consumers;
// secondary views retain the base-specific slot map needed by multiple
// inheritance lowering.
struct VirtualTableView
{
	TypePtr base;
	size_t base_index;
	vector<VirtualMethodInfo> methods;

	VirtualTableView()
		: base(), base_index(static_cast<size_t>(-1)), methods() {}
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
	// All direct base subobjects, retained alongside direct_base for the
	// single-inheritance consumers from earlier assignments.
	vector<TypePtr> direct_bases;
	// Whether each direct base edge is virtual.  The edge fact belongs to the
	// derived class rather than the base type: the same base may occur once
	// non-virtually and once virtually in a class lattice.
	vector<bool> direct_base_virtual;
	// Access is an edge fact used by RTTI's base-class flags.
	vector<string> direct_base_access;
	// Offset of the direct base subobject within the complete object.  The
	// supported PA17 layout normally keeps a single-inheritance base at zero;
	// a class that introduces its first vpointer reserves offset zero for that
	// pointer and records the non-polymorphic base's adjusted address here.
	 size_t direct_base_offset;
	vector<size_t> direct_base_offsets;
	// The first polymorphic direct base supplies the primary object view even
	// when a non-polymorphic base is spelled first in the source list.
	size_t primary_base_index;
	// Complete-object virtual-base closure and its deterministic offsets.
	// `nonvirtual_size` is the storage occupied by this class's own
	// non-virtual subobjects and members when embedded in another object; it
	// deliberately excludes the virtual-base closure.
	vector<TypePtr> virtual_base_types;
	// The physical virtual-base root that contains each view above.  Nested
	// virtual bases share the root allocation but retain their own address
	// projection in the ABI closure.
	vector<TypePtr> virtual_base_roots;
	vector<size_t> virtual_base_offsets;
	size_t nonvirtual_size;
	size_t object_size;
	size_t object_alignment;
	size_t explicit_alignment;
	bool layout_complete;
	bool layout_in_progress;
	bool is_union;
	TypePtr enclosing_type;
	// True when this materialized class was defined with a dependent base.
	// Unqualified lookup in its template body must not inspect that base.
	bool dependent_base_lookup;
	vector<FriendAccess> friend_access;
	vector<VirtualMethodInfo> virtual_methods;
	vector<VirtualTableView> virtual_table_views;
	bool polymorphic;
	bool has_vpointer;
	bool template_specialization;
	string template_primary;
	vector<string> template_arguments;
	// Template parameter names are retained in declaration order, including
	// non-type parameters that do not have a Scope binding in the PA11 model.
	// Materialized dependent members use this typed frame to rebind base and
	// conversion-function types without reconstructing source text lookup.
	vector<string> template_parameter_names;
	// The corresponding pack flags let ABI lowering preserve a class
	// specialization's pack boundaries (for example `Signatures...`) when
	// re-encoding a nested concrete type.
	vector<bool> template_parameter_packs;
	bool template_empty_pack;
	// A materialized specialization may retain constructor-template lookup only
	// in PA18 typed state.  It is still non-aggregate per the source class.
	bool has_deferred_constructor;
	// A materialized specialization whose object layout depends on a nested
	// specialization of the current class needs its local object address
	// visible when lowering an unevaluated sizeof query.
	bool materialize_sizeof_address;

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
	bool has_value; bool suppress_dump;
	// PA19 owns the semantic constant.  `value` remains only as the signed
	// compatibility projection consumed by the pre-PA19 lowering paths.
	PA19IntegralValue constant_value;
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
	// Exception specifications are semantic facts of a declaration.  They are
	// kept separately from the callable type so PA18 can compare an
	// out-of-class definition with its earlier declaration.
	bool noexcept_specified;
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
	enum Kind { CONSTANT_UNKNOWN, CONSTANT_INTEGRAL, CONSTANT_FLOATING,
		CONSTANT_OBJECT, CONSTANT_POINTER };
	Kind kind;
	PA19IntegralValue integral;
	// Legacy PA11-PA14 consumers read the signed projection; PA19 evaluation
	// and conversion use `integral` as the sole typed owner.
	long long value;
	bool floating_known;
	long double floating;
	TypePtr type;
	shared_ptr<ConstantObject> object;
	shared_ptr<ConstantPointer> pointer;

	ConstantValue(bool is_known = false, long long constant = 0);
};

// PA20 constant evaluation retains aggregate identity and array elements as
// semantic values.  The LowIR layer may still lower these values into its
// existing object representation, but it no longer has to reconstruct them
// from source spelling.
struct ConstantObject
{
	TypePtr type;
	vector<ConstantValue> elements;
	map<string, ConstantValue> members;
};

struct ConstantPointer
{
	shared_ptr<ConstantObject> object;
	long long index;
	bool null_pointer;

	ConstantPointer()
		: object(), index(0), null_pointer(false) {}
};

string LastComponent(const string& name);
string StripTypeMarker(const string& name);
string TypeText(const TypePtr& type, bool extended = false);
bool SameTypeIgnoringTopCv(const TypePtr& left, const TypePtr& right);
// Keep the multi-base representation in one place.  `direct_base` remains a
// compatibility field for the single-inheritance consumers from earlier PAs,
// while these helpers make new semantic/lowering code consume every direct
// base and retain the old representation as a fallback for materialized types.
vector<TypePtr> DirectBaseTypes(const TypePtr& type);
vector<TypePtr> BaseTypeClosure(const TypePtr& type);
bool IsVirtualDirectBase(const TypePtr& type, size_t index);
size_t NonVirtualObjectSize(const TypePtr& type);
bool HasVirtualBases(const TypePtr& type);
vector<TypePtr> VirtualBaseTypes(const TypePtr& type);
bool FindVirtualBaseOffset(const TypePtr& type, const TypePtr& target,
	size_t* offset, size_t occurrence = 0);
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
