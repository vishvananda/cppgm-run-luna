#include "nsinit_model.h"

using namespace std;

Type::Type()
	: kind(FUNDAMENTAL), fundamental(), is_const(false), is_volatile(false),
	  child(), bound(-1), parameters(), variadic(false)
{}

Type Type::Fundamental(const string& name)
{
	Type result;
	result.fundamental = name;
	return result;
}

Type Type::Pointer(const Type& pointee, bool constant, bool vol)
{
	Type result;
	result.kind = POINTER;
	result.is_const = constant;
	result.is_volatile = vol;
	result.child.reset(new Type(pointee));
	return result;
}

Type Type::Reference(Kind ref_kind, const Type& referred)
{
	Type result;
	result.kind = ref_kind;
	result.child.reset(new Type(referred));
	return result;
}

Type Type::Array(long long array_bound, const Type& element)
{
	Type result;
	result.kind = ARRAY;
	result.bound = array_bound;
	result.child.reset(new Type(element));
	return result;
}

Type Type::Function(const vector<Type>& params, bool is_variadic,
	const Type& result_type)
{
	Type result;
	result.kind = FUNCTION;
	result.parameters = params;
	result.variadic = is_variadic;
	result.child.reset(new Type(result_type));
	return result;
}

Type AddCv(const Type& type, bool constant, bool vol)
{
	if (!constant && !vol) return type;
	if (type.kind == Type::LVALUE_REFERENCE ||
		type.kind == Type::RVALUE_REFERENCE)
		return type;
	if (type.kind == Type::ARRAY)
		return Type::Array(type.bound, AddCv(*type.child, constant, vol));
	Type result = type;
	result.is_const = result.is_const || constant;
	result.is_volatile = result.is_volatile || vol;
	return result;
}

Type RemoveTopCv(const Type& type)
{
	Type result = type;
	result.is_const = false;
	result.is_volatile = false;
	return result;
}

Type CollapseReference(Type::Kind ref_kind, const Type& type)
{
	if (type.kind != Type::LVALUE_REFERENCE &&
		type.kind != Type::RVALUE_REFERENCE)
		return Type::Reference(ref_kind, type);
	const Type referred = *type.child;
	if (ref_kind == Type::LVALUE_REFERENCE ||
		type.kind == Type::LVALUE_REFERENCE)
		return Type::Reference(Type::LVALUE_REFERENCE, referred);
	return Type::Reference(Type::RVALUE_REFERENCE, referred);
}

bool SameType(const Type& left, const Type& right)
{
	if (left.kind != right.kind || left.is_const != right.is_const ||
		left.is_volatile != right.is_volatile)
		return false;
	if (left.kind == Type::FUNDAMENTAL)
		return left.fundamental == right.fundamental;
	if (left.kind == Type::ARRAY && left.bound != right.bound) return false;
	if (left.kind == Type::FUNCTION)
	{
		if (left.variadic != right.variadic ||
			left.parameters.size() != right.parameters.size())
			return false;
		for (size_t i = 0; i < left.parameters.size(); ++i)
			if (!SameType(left.parameters[i], right.parameters[i])) return false;
	}
	return !left.child || (right.child && SameType(*left.child, *right.child));
}

bool SameIgnoringTopCv(const Type& left, const Type& right)
{
	return SameType(RemoveTopCv(left), RemoveTopCv(right));
}

bool SameFunctionSignature(const Type& left, const Type& right)
{
	if (left.kind != Type::FUNCTION || right.kind != Type::FUNCTION ||
		left.variadic != right.variadic ||
		left.parameters.size() != right.parameters.size())
		return false;
	for (size_t i = 0; i < left.parameters.size(); ++i)
		if (!SameType(left.parameters[i], right.parameters[i])) return false;
	return true;
}

Type MergeType(const Type& old_type, const Type& new_type)
{
	if (SameType(old_type, new_type)) return old_type;
	if (old_type.kind == Type::ARRAY && new_type.kind == Type::ARRAY &&
		old_type.bound < 0 && new_type.bound >= 0 &&
		SameType(*old_type.child, *new_type.child))
		return new_type;
	if (old_type.kind == Type::ARRAY && new_type.kind == Type::ARRAY &&
		new_type.bound < 0 && old_type.bound >= 0 &&
		SameType(*old_type.child, *new_type.child))
		return old_type;
	throw logic_error("conflicting declaration type");
}

bool IsFundamental(const Type& type, const string& name)
{
	return type.kind == Type::FUNDAMENTAL && type.fundamental == name;
}

bool IsIntegral(const Type& type)
{
	if (type.kind != Type::FUNDAMENTAL) return false;
	return type.fundamental == "signed char" || type.fundamental == "short int" ||
		type.fundamental == "int" || type.fundamental == "long int" ||
		type.fundamental == "long long int" ||
		type.fundamental == "unsigned char" ||
		type.fundamental == "unsigned short int" ||
		type.fundamental == "unsigned int" ||
		type.fundamental == "unsigned long int" ||
		type.fundamental == "unsigned long long int" ||
		type.fundamental == "char" || type.fundamental == "char16_t" ||
		type.fundamental == "char32_t" || type.fundamental == "wchar_t" ||
		type.fundamental == "bool";
}

bool IsFloating(const Type& type)
{
	return type.kind == Type::FUNDAMENTAL &&
		(type.fundamental == "float" || type.fundamental == "double" ||
		 type.fundamental == "long double");
}

size_t FundamentalSize(const string& name)
{
	if (name == "signed char" || name == "unsigned char" || name == "char" ||
		name == "bool") return 1;
	if (name == "short int" || name == "unsigned short int" ||
		name == "char16_t") return 2;
	if (name == "int" || name == "unsigned int" || name == "wchar_t" ||
		name == "char32_t" || name == "float") return 4;
	if (name == "long int" || name == "long long int" ||
		name == "unsigned long int" || name == "unsigned long long int" ||
		name == "double") return 8;
	if (name == "long double") return 16;
	if (name == "nullptr_t") return 8;
	return 0;
}

size_t TypeSize(const Type& type)
{
	switch (type.kind)
	{
	case Type::FUNDAMENTAL: return FundamentalSize(type.fundamental);
	case Type::POINTER:
	case Type::LVALUE_REFERENCE:
	case Type::RVALUE_REFERENCE: return 8;
	case Type::FUNCTION: return 4;
	case Type::ARRAY:
		return type.bound < 0 ? 0 : type.bound * TypeSize(*type.child);
	}
	return 0;
}

size_t TypeAlignment(const Type& type)
{
	switch (type.kind)
	{
	case Type::FUNDAMENTAL: return FundamentalSize(type.fundamental);
	case Type::POINTER:
	case Type::LVALUE_REFERENCE:
	case Type::RVALUE_REFERENCE: return 8;
	case Type::FUNCTION: return 4;
	case Type::ARRAY: return TypeAlignment(*type.child);
	}
	return 0;
}

Address::Address()
	: entity(NULL), temporary(NULL), string(NULL), addend(0)
{}

bool Address::IsNull() const
{
	return entity == NULL && temporary == NULL && string == NULL;
}

InitData::InitData() : kind(INIT_ZERO), bytes(), address() {}

ConstantData::ConstantData()
	: known(false), usable(false), pointer(false), integral(false),
	  floating(false), integer(0), real(0), address(), bytes()
{}

Entity::Entity(EntityKind entity_kind, const string& entity_name,
	const Type& entity_type, Namespace* entity_scope, int entity_unit,
	bool internal)
	: kind(entity_kind), name(entity_name), type(entity_type), scope(entity_scope),
	  unit_id(entity_unit), internal_linkage(internal), has_definition(false),
	  has_initializer(false), is_constexpr(false), is_inline(false),
	  function_definition(false), initializer(), value(), reference_address(),
	  has_reference_address(false), offset(0)
{}

Temporary::Temporary(const Type& temporary_type)
	: type(temporary_type), initializer(), offset(0)
{}

StringLiteral::StringLiteral(const Type& literal_type,
	const vector<unsigned char>& literal_bytes)
	: type(literal_type), bytes(literal_bytes), offset(0)
{}

Namespace::Namespace(const string& namespace_name, bool is_global,
	bool is_unnamed, int namespace_unit, bool is_inline,
	Namespace* namespace_parent)
	: name(namespace_name), global(is_global), unnamed(is_unnamed),
	  unit_id(namespace_unit), inline_namespace(is_inline), parent(namespace_parent),
	  children(), named_children(), namespace_aliases(), using_directives(),
	  bindings(), entities()
{}

NamePath::NamePath() : absolute(false), parts() {}

DeclaratorOp::DeclaratorOp(Kind operation)
	: kind(operation), is_const(false), is_volatile(false), bound(-1),
	  parameters(), variadic(false)
{}

DeclaratorShape::DeclaratorShape() : operations(), has_name(false), name() {}

DeclSpec::DeclSpec()
	: type(), is_typedef(false), is_constexpr(false), is_inline(false),
	  is_static(false), is_thread_local(false), is_extern(false)
{}

ExprValue::ExprValue()
	: type(), category(VALUE_PRVALUE), constant(false), integral(false),
	  floating(false), null_pointer(false), integer(0), real(0),
	  has_address(false), address(), has_pointer_value(false), pointer_value(),
	  object(NULL), string(NULL)
{}

bool IsKeyword(const string& text)
{
	static const set<string> keywords = {
		"alignas", "alignof", "asm", "auto", "bool", "break", "case", "catch",
		"char", "char16_t", "char32_t", "class", "const", "constexpr", "continue",
		"double", "else", "enum", "extern", "false", "float", "for", "friend",
		"goto", "if", "inline", "int", "long", "mutable", "namespace", "new",
		"nullptr", "operator", "private", "protected", "public", "register",
		"return", "short", "signed", "sizeof", "static", "static_assert", "struct",
		"switch", "template", "this", "thread_local", "throw", "true", "try",
		"typedef", "typeid", "typename", "union", "unsigned", "using", "virtual",
		"void", "volatile", "wchar_t", "while"
	};
	return keywords.find(text) != keywords.end();
}

bool IsIdentifier(const PostPPToken& token)
{
	return token.kind == POST_PP_IDENTIFIER && !IsKeyword(token.source);
}

bool IsFundamentalWord(const string& text)
{
	static const set<string> words = {
		"char", "char16_t", "char32_t", "wchar_t", "bool", "short", "int",
		"long", "signed", "unsigned", "float", "double", "void"
	};
	return words.find(text) != words.end();
}

bool IsCvWord(const string& text)
{
	return text == "const" || text == "volatile";
}

bool IsNameToken(const PostPPToken& token)
{
	return IsIdentifier(token);
}

bool IsPointerOperator(const string& text)
{
	return text == "*" || text == "&" || text == "&&";
}

Program::Program()
	: root_(new Namespace(string(), true, true, -1, false, NULL)),
	  ordered_entities_(), temporaries_(), strings_()
{}

Namespace* Program::root() const
{
	return root_.get();
}

vector<Entity*>& Program::ordered_entities()
{
	return ordered_entities_;
}

vector<unique_ptr<Temporary> >& Program::temporaries()
{
	return temporaries_;
}

vector<unique_ptr<StringLiteral> >& Program::strings()
{
	return strings_;
}

Namespace* Program::NamedNamespace(Namespace* parent, const string& name,
	bool inline_namespace)
{
	if (parent->namespace_aliases.find(name) != parent->namespace_aliases.end() ||
		parent->bindings.find(name) != parent->bindings.end())
		throw logic_error("namespace alias misuse");
	map<string, Namespace*>::iterator found = parent->named_children.find(name);
	if (found != parent->named_children.end())
	{
		if (inline_namespace && !found->second->inline_namespace)
			throw logic_error("extension namespace cannot be inline");
		return found->second;
	}
	Namespace* child = new Namespace(name, false, false, -1, inline_namespace, parent);
	parent->children.push_back(unique_ptr<Namespace>(child));
	parent->named_children[name] = child;
	return child;
}

Namespace* Program::UnnamedNamespace(Namespace* parent, int unit_id,
	bool inline_namespace)
{
	for (size_t i = 0; i < parent->children.size(); ++i)
	{
		Namespace* child = parent->children[i].get();
		if (!child->unnamed || child->unit_id != unit_id) continue;
		if (inline_namespace && !child->inline_namespace)
			throw logic_error("extension namespace cannot be inline");
		return child;
	}
	Namespace* child = new Namespace(string(), false, true, unit_id,
		inline_namespace, parent);
	parent->children.push_back(unique_ptr<Namespace>(child));
	return child;
}

bool Program::VisibleChild(const Namespace* child, int unit_id) const
{
	return !child->unnamed || child->unit_id == unit_id;
}

Namespace* Program::FindNamespaceIn(Namespace* scope, const string& name,
	int unit_id, set<Namespace*>* visited) const
{
	if (scope == NULL || !visited->insert(scope).second) return NULL;
	map<string, Namespace*>::const_iterator named = scope->named_children.find(name);
	if (named != scope->named_children.end()) return named->second;
	map<string, Namespace*>::const_iterator alias = scope->namespace_aliases.find(name);
	if (alias != scope->namespace_aliases.end()) return alias->second;
	for (size_t i = 0; i < scope->children.size(); ++i)
	{
		Namespace* child = scope->children[i].get();
		if ((!child->unnamed && !child->inline_namespace) ||
			!VisibleChild(child, unit_id)) continue;
		Namespace* result = FindNamespaceIn(child, name, unit_id, visited);
		if (result != NULL) return result;
	}
	for (size_t i = 0; i < scope->using_directives.size(); ++i)
	{
		Namespace* result = FindNamespaceIn(scope->using_directives[i], name,
			unit_id, visited);
		if (result != NULL) return result;
	}
	return NULL;
}

Entity* Program::FindEntityIn(Namespace* scope, const string& name,
	int unit_id, set<Namespace*>* visited) const
{
	if (scope == NULL || !visited->insert(scope).second) return NULL;
	map<string, vector<Entity*> >::const_iterator direct =
		scope->bindings.find(name);
	if (direct != scope->bindings.end())
		for (size_t i = 0; i < direct->second.size(); ++i)
			if (direct->second[i]->kind != TYPEDEF_ENTITY &&
				(!direct->second[i]->internal_linkage ||
				 direct->second[i]->unit_id == unit_id))
				return direct->second[i];
	for (size_t i = 0; i < scope->children.size(); ++i)
	{
		Namespace* child = scope->children[i].get();
		if ((!child->unnamed && !child->inline_namespace) ||
			!VisibleChild(child, unit_id)) continue;
		Entity* result = FindEntityIn(child, name, unit_id, visited);
		if (result != NULL) return result;
	}
	for (size_t i = 0; i < scope->using_directives.size(); ++i)
	{
		Entity* result = FindEntityIn(scope->using_directives[i], name,
			unit_id, visited);
		if (result != NULL) return result;
	}
	return NULL;
}

Entity* Program::FindTypeIn(Namespace* scope, const string& name,
	int unit_id, set<Namespace*>* visited) const
{
	if (scope == NULL || !visited->insert(scope).second) return NULL;
	map<string, vector<Entity*> >::const_iterator direct =
		scope->bindings.find(name);
	if (direct != scope->bindings.end())
		for (size_t i = 0; i < direct->second.size(); ++i)
			if (direct->second[i]->kind == TYPEDEF_ENTITY)
				return direct->second[i];
	for (size_t i = 0; i < scope->children.size(); ++i)
	{
		Namespace* child = scope->children[i].get();
		if ((!child->unnamed && !child->inline_namespace) ||
			!VisibleChild(child, unit_id)) continue;
		Entity* result = FindTypeIn(child, name, unit_id, visited);
		if (result != NULL) return result;
	}
	for (size_t i = 0; i < scope->using_directives.size(); ++i)
	{
		Entity* result = FindTypeIn(scope->using_directives[i], name,
			unit_id, visited);
		if (result != NULL) return result;
	}
	return NULL;
}

Namespace* Program::LookupNamespace(const string& name, Namespace* from,
	int unit_id) const
{
	set<Namespace*> visited;
	for (Namespace* scope = from; scope != NULL; scope = scope->parent)
	{
		Namespace* result = FindNamespaceIn(scope, name, unit_id, &visited);
		if (result != NULL) return result;
	}
	return NULL;
}

Entity* Program::LookupEntity(const string& name, Namespace* from,
	int unit_id) const
{
	set<Namespace*> visited;
	for (Namespace* scope = from; scope != NULL; scope = scope->parent)
	{
		Entity* result = FindEntityIn(scope, name, unit_id, &visited);
		if (result != NULL) return result;
	}
	return NULL;
}

Entity* Program::LookupType(const string& name, Namespace* from,
	int unit_id) const
{
	set<Namespace*> visited;
	for (Namespace* scope = from; scope != NULL; scope = scope->parent)
	{
		Entity* result = FindTypeIn(scope, name, unit_id, &visited);
		if (result != NULL) return result;
	}
	return NULL;
}

Namespace* Program::ResolveNamespacePath(const NamePath& path, Namespace* from,
	int unit_id) const
{
	if (path.parts.empty()) return NULL;
	Namespace* result = NULL;
	if (path.absolute)
	{
		set<Namespace*> visited;
		result = FindNamespaceIn(root_.get(), path.parts[0], unit_id, &visited);
	}
	else result = LookupNamespace(path.parts[0], from, unit_id);
	for (size_t i = 1; result != NULL && i < path.parts.size(); ++i)
	{
		set<Namespace*> visited;
		result = FindNamespaceIn(result, path.parts[i], unit_id, &visited);
	}
	return result;
}

Entity* Program::ResolveTypePath(const NamePath& path, Namespace* from,
	int unit_id) const
{
	if (path.parts.empty()) return NULL;
	if (path.parts.size() == 1 && !path.absolute)
		return LookupType(path.parts[0], from, unit_id);
	NamePath namespace_path = path;
	namespace_path.parts.pop_back();
	Namespace* scope = namespace_path.parts.empty() ? root_.get() :
		ResolveNamespacePath(namespace_path, from, unit_id);
	if (scope == NULL) return NULL;
	set<Namespace*> visited;
	return FindTypeIn(scope, path.parts.back(), unit_id, &visited);
}

Entity* Program::ResolveEntityPath(const NamePath& path, Namespace* from,
	int unit_id) const
{
	if (path.parts.empty()) return NULL;
	if (path.parts.size() == 1 && !path.absolute)
		return LookupEntity(path.parts[0], from, unit_id);
	NamePath namespace_path = path;
	namespace_path.parts.pop_back();
	Namespace* scope = namespace_path.parts.empty() ? root_.get() :
		ResolveNamespacePath(namespace_path, from, unit_id);
	if (scope == NULL) return NULL;
	set<Namespace*> visited;
	return FindEntityIn(scope, path.parts.back(), unit_id, &visited);
}

bool Program::IsEnclosing(Namespace* possible_parent, Namespace* scope) const
{
	for (Namespace* cursor = scope; cursor != NULL; cursor = cursor->parent)
		if (cursor == possible_parent) return true;
	return false;
}

Entity* Program::AddTypeDef(Namespace* scope, const string& name,
	const Type& type, int unit_id)
{
	if (scope->named_children.find(name) != scope->named_children.end() ||
		scope->namespace_aliases.find(name) != scope->namespace_aliases.end())
		throw logic_error("namespace alias misuse");
	vector<Entity*>& entries = scope->bindings[name];
	for (size_t i = 0; i < entries.size(); ++i)
	{
		if (entries[i]->kind != TYPEDEF_ENTITY)
			throw logic_error("conflicting declaration");
		if (SameType(entries[i]->type, type)) return entries[i];
		throw logic_error("conflicting typedef declaration");
	}
	Entity* entity = new Entity(TYPEDEF_ENTITY, name, type, scope, -1, false);
	scope->entities.push_back(unique_ptr<Entity>(entity));
	entries.push_back(entity);
	(void)unit_id;
	return entity;
}

Entity* Program::AddOrdinary(Namespace* scope, const string& name,
	const Type& type, EntityKind kind, bool internal, int unit_id)
{
	if (scope->named_children.find(name) != scope->named_children.end() ||
		scope->namespace_aliases.find(name) != scope->namespace_aliases.end())
		throw logic_error("namespace alias misuse");
	vector<Entity*>& entries = scope->bindings[name];
	Entity* match = NULL;
	for (size_t i = 0; i < entries.size(); ++i)
	{
		Entity* entry = entries[i];
		if (entry->kind == TYPEDEF_ENTITY || entry->kind != kind)
			throw logic_error("conflicting declaration");
		if (kind == FUNCTION_ENTITY)
		{
			if (!SameFunctionSignature(entry->type, type)) continue;
			if (entry->internal_linkage != internal ||
				(internal && entry->unit_id != unit_id)) continue;
			if (!SameType(entry->type, type))
				throw logic_error("conflicting function declaration");
		}
		else if (entry->internal_linkage != internal ||
			(internal && entry->unit_id != unit_id)) continue;
		match = entry;
		break;
	}
	if (match != NULL)
	{
		match->type = MergeType(match->type, type);
		return match;
	}
	Entity* entity = new Entity(kind, name, type, scope,
		internal ? unit_id : -1, internal);
	scope->entities.push_back(unique_ptr<Entity>(entity));
	entries.push_back(entity);
	ordered_entities_.push_back(entity);
	return entity;
}

Entity* Program::AddVariable(Namespace* scope, const string& name,
	const Type& type, bool internal, int unit_id)
{
	return AddOrdinary(scope, name, type, VARIABLE_ENTITY, internal, unit_id);
}

Entity* Program::AddFunction(Namespace* scope, const string& name,
	const Type& type, bool internal, int unit_id)
{
	return AddOrdinary(scope, name, type, FUNCTION_ENTITY, internal, unit_id);
}

void Program::AddImport(Namespace* scope, Entity* entity)
{
	if (entity == NULL) throw logic_error("unknown using target");
	vector<Entity*>& entries = scope->bindings[entity->name];
	for (size_t i = 0; i < entries.size(); ++i)
		if (entries[i] == entity) return;
	for (size_t i = 0; i < entries.size(); ++i)
		if (entries[i]->kind != entity->kind)
			throw logic_error("conflicting using declaration");
	entries.push_back(entity);
}

Temporary* Program::AddTemporary(const Type& type)
{
	Temporary* result = new Temporary(type);
	temporaries_.push_back(unique_ptr<Temporary>(result));
	return result;
}

StringLiteral* Program::AddString(const Type& type,
	const vector<unsigned char>& bytes)
{
	StringLiteral* result = new StringLiteral(type, bytes);
	strings_.push_back(unique_ptr<StringLiteral>(result));
	return result;
}
