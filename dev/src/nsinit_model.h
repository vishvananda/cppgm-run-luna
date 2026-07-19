#pragma once

#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "posttoken_lexer.h"

using namespace std;

// PA8's semantic model is kept in a small internal interface so the parser
// and image writer can share typed facts without passing presentation strings
// between phases.  The implementation lives in nsinit_model.cpp; keeping the
// ownership and lookup operations out of this header makes the boundary clear
// and avoids recompiling the model into every PA8 translation unit.

struct Type
{
	enum Kind
	{
		FUNDAMENTAL, POINTER, LVALUE_REFERENCE, RVALUE_REFERENCE, ARRAY, FUNCTION
	};

	Kind kind;
	string fundamental;
	bool is_const, is_volatile;
	shared_ptr<Type> child;
	long long bound;
	vector<Type> parameters;
	bool variadic;

	Type();
	static Type Fundamental(const string& name);
	static Type Pointer(const Type& pointee, bool constant = false,
		bool vol = false);
	static Type Reference(Kind ref_kind, const Type& referred);
	static Type Array(long long array_bound, const Type& element);
	static Type Function(const vector<Type>& params, bool is_variadic,
		const Type& result_type);
};

Type AddCv(const Type& type, bool constant, bool vol);
Type RemoveTopCv(const Type& type);
Type CollapseReference(Type::Kind ref_kind, const Type& type);
bool SameType(const Type& left, const Type& right);
bool SameIgnoringTopCv(const Type& left, const Type& right);
bool SameFunctionSignature(const Type& left, const Type& right);
Type MergeType(const Type& old_type, const Type& new_type);
bool IsFundamental(const Type& type, const string& name);
bool IsIntegral(const Type& type);
bool IsFloating(const Type& type);
size_t FundamentalSize(const string& name);
size_t TypeSize(const Type& type);
size_t TypeAlignment(const Type& type);

struct Namespace;
struct Entity;
struct Temporary;
struct StringLiteral;

struct Address
{
	Entity* entity;
	Temporary* temporary;
	StringLiteral* string;
	long long addend;

	Address();
	bool IsNull() const;
};

enum InitKind { INIT_ZERO, INIT_BYTES, INIT_ADDRESS };

struct InitData
{
	InitKind kind;
	vector<unsigned char> bytes;
	Address address;

	InitData();
};

struct ConstantData
{
	bool known, usable, pointer, integral, floating;
	unsigned long long integer;
	long double real;
	Address address;
	vector<unsigned char> bytes;

	ConstantData();
};

enum EntityKind { TYPEDEF_ENTITY, VARIABLE_ENTITY, FUNCTION_ENTITY };

struct Entity
{
	EntityKind kind;
	string name;
	Type type;
	Namespace* scope;
	int unit_id;
	bool internal_linkage, has_definition, has_initializer;
	bool is_constexpr, is_inline, function_definition;
	InitData initializer;
	ConstantData value;
	Address reference_address;
	bool has_reference_address;
	size_t offset;

	Entity(EntityKind entity_kind, const string& entity_name, const Type& entity_type,
		Namespace* entity_scope, int entity_unit, bool internal);
};

struct Temporary
{
	Type type;
	InitData initializer;
	size_t offset;

	Temporary(const Type& temporary_type);
};

struct StringLiteral
{
	Type type;
	vector<unsigned char> bytes;
	size_t offset;

	StringLiteral(const Type& literal_type,
		const vector<unsigned char>& literal_bytes);
};

struct Namespace
{
	string name;
	bool global, unnamed;
	int unit_id;
	bool inline_namespace;
	Namespace* parent;
	vector<unique_ptr<Namespace> > children;
	map<string, Namespace*> named_children;
	map<string, Namespace*> namespace_aliases;
	vector<Namespace*> using_directives;
	map<string, vector<Entity*> > bindings;
	vector<unique_ptr<Entity> > entities;

	Namespace(const string& namespace_name, bool is_global, bool is_unnamed,
		int namespace_unit, bool is_inline, Namespace* namespace_parent);
};

struct NamePath
{
	bool absolute;
	vector<string> parts;

	NamePath();
};

struct DeclaratorOp
{
	enum Kind { POINTER, LVALUE_REFERENCE, RVALUE_REFERENCE, ARRAY, FUNCTION };
	Kind kind;
	bool is_const, is_volatile;
	long long bound;
	vector<Type> parameters;
	bool variadic;

	DeclaratorOp(Kind operation);
};

struct DeclaratorShape
{
	vector<DeclaratorOp> operations;
	bool has_name;
	NamePath name;

	DeclaratorShape();
};

struct DeclSpec
{
	Type type;
	bool is_typedef, is_constexpr, is_inline;
	bool is_static, is_thread_local, is_extern;

	DeclSpec();
};

enum ValueCategory { VALUE_PRVALUE, VALUE_LVALUE, VALUE_XVALUE };

struct ExprValue
{
	Type type;
	ValueCategory category;
	bool constant, integral, floating, null_pointer;
	unsigned long long integer;
	long double real;
	bool has_address;
	Address address;
	bool has_pointer_value;
	Address pointer_value;
	Entity* object;
	StringLiteral* string;

	ExprValue();
};

bool IsKeyword(const string& text);
bool IsIdentifier(const PostPPToken& token);
bool IsFundamentalWord(const string& text);
bool IsCvWord(const string& text);
bool IsNameToken(const PostPPToken& token);
bool IsPointerOperator(const string& text);

class Program
{
public:
	Program();

	Namespace* root() const;
	vector<Entity*>& ordered_entities();
	vector<unique_ptr<Temporary> >& temporaries();
	vector<unique_ptr<StringLiteral> >& strings();

	Namespace* NamedNamespace(Namespace* parent, const string& name,
		bool inline_namespace);
	Namespace* UnnamedNamespace(Namespace* parent, int unit_id,
		bool inline_namespace);
	Namespace* LookupNamespace(const string& name, Namespace* from,
		int unit_id) const;
	Entity* LookupEntity(const string& name, Namespace* from, int unit_id) const;
	Entity* LookupType(const string& name, Namespace* from, int unit_id) const;
	Namespace* ResolveNamespacePath(const NamePath& path, Namespace* from,
		int unit_id) const;
	Entity* ResolveTypePath(const NamePath& path, Namespace* from,
		int unit_id) const;
	Entity* ResolveEntityPath(const NamePath& path, Namespace* from,
		int unit_id) const;
	bool IsEnclosing(Namespace* possible_parent, Namespace* scope) const;

	Entity* AddTypeDef(Namespace* scope, const string& name, const Type& type,
		int unit_id);
	Entity* AddVariable(Namespace* scope, const string& name, const Type& type,
		bool internal, int unit_id);
	Entity* AddFunction(Namespace* scope, const string& name, const Type& type,
		bool internal, int unit_id);
	void AddImport(Namespace* scope, Entity* entity);
	Temporary* AddTemporary(const Type& type);
	StringLiteral* AddString(const Type& type,
		const vector<unsigned char>& bytes);

private:
	unique_ptr<Namespace> root_;
	vector<Entity*> ordered_entities_;
	vector<unique_ptr<Temporary> > temporaries_;
	vector<unique_ptr<StringLiteral> > strings_;

	bool VisibleChild(const Namespace* child, int unit_id) const;
	Namespace* FindNamespaceIn(Namespace* scope, const string& name,
		int unit_id, set<Namespace*>* visited) const;
	Entity* FindEntityIn(Namespace* scope, const string& name, int unit_id,
		set<Namespace*>* visited) const;
	Entity* FindTypeIn(Namespace* scope, const string& name, int unit_id,
		set<Namespace*>* visited) const;
	Entity* AddOrdinary(Namespace* scope, const string& name, const Type& type,
		EntityKind kind, bool internal, int unit_id);
};
