#include "nsdecl_parser.h"

#include <cstdlib>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "posttoken_semantics.h"
#include "preprocessor_engine.h"

using namespace std;

namespace {

struct Type
{
	enum Kind
	{
		FUNDAMENTAL,
		POINTER,
		LVALUE_REFERENCE,
		RVALUE_REFERENCE,
		ARRAY,
		FUNCTION
	};

	Kind kind;
	string fundamental;
	bool is_const;
	bool is_volatile;
	shared_ptr<Type> child;
	long long array_bound;
	vector<Type> parameters;
	bool variadic;

	Type()
		: kind(FUNDAMENTAL), fundamental(), is_const(false),
		  is_volatile(false), child(), array_bound(-1), parameters(),
		  variadic(false)
	{}

	static Type Fundamental(const string& name)
	{
		Type result;
		result.kind = FUNDAMENTAL;
		result.fundamental = name;
		return result;
	}

	static Type Pointer(const Type& pointee, bool is_const = false,
		bool is_volatile = false)
	{
		Type result;
		result.kind = POINTER;
		result.is_const = is_const;
		result.is_volatile = is_volatile;
		result.child.reset(new Type(pointee));
		return result;
	}

	static Type Reference(Kind reference_kind, const Type& referred)
	{
		Type result;
		result.kind = reference_kind;
		result.child.reset(new Type(referred));
		return result;
	}

	static Type Array(long long bound, const Type& element)
	{
		Type result;
		result.kind = ARRAY;
		result.array_bound = bound;
		result.child.reset(new Type(element));
		return result;
	}

	static Type Function(const vector<Type>& params, bool is_variadic,
		const Type& result_type)
	{
		Type result;
		result.kind = FUNCTION;
		result.parameters = params;
		result.variadic = is_variadic;
		result.child.reset(new Type(result_type));
		return result;
	}

	string ToString() const
	{
		string prefix;
		if (is_const) prefix += "const ";
		if (is_volatile) prefix += "volatile ";
		switch (kind)
		{
		case FUNDAMENTAL:
			return prefix + fundamental;
		case POINTER:
			return prefix + "pointer to " + child->ToString();
		case LVALUE_REFERENCE:
			return prefix + "lvalue-reference to " + child->ToString();
		case RVALUE_REFERENCE:
			return prefix + "rvalue-reference to " + child->ToString();
		case ARRAY:
		{
			ostringstream text;
			text << prefix << "array of ";
			if (array_bound < 0) text << "unknown bound of ";
			else text << array_bound << " ";
			text << child->ToString();
			return text.str();
		}
		case FUNCTION:
		{
			ostringstream text;
			text << prefix << "function of (";
			for (size_t i = 0; i < parameters.size(); ++i)
			{
				if (i != 0) text << ", ";
				text << parameters[i].ToString();
			}
			if (variadic)
			{
				if (!parameters.empty()) text << ", ";
				text << "...";
			}
			text << ") returning " << child->ToString();
			return text.str();
		}
		}
		return string();
	}
};

Type AddCv(const Type& type, bool is_const, bool is_volatile)
{
	if (!is_const && !is_volatile) return type;
	if (type.kind == Type::LVALUE_REFERENCE ||
		type.kind == Type::RVALUE_REFERENCE)
		return type;
	if (type.kind == Type::ARRAY)
		return Type::Array(type.array_bound,
			AddCv(*type.child, is_const, is_volatile));
	Type result = type;
	result.is_const = result.is_const || is_const;
	result.is_volatile = result.is_volatile || is_volatile;
	return result;
}

Type RemoveTopCv(const Type& type)
{
	Type result = type;
	result.is_const = false;
	result.is_volatile = false;
	return result;
}

Type CollapseReference(Type::Kind reference_kind, const Type& type)
{
	if (type.kind != Type::LVALUE_REFERENCE &&
		type.kind != Type::RVALUE_REFERENCE)
		return Type::Reference(reference_kind, type);

	const Type referred = *type.child;
	if (reference_kind == Type::LVALUE_REFERENCE ||
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
	if (left.kind == Type::ARRAY && left.array_bound != right.array_bound)
		return false;
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

Type MergeDeclarationType(const Type& old_type, const Type& new_type)
{
	if (old_type.kind != new_type.kind) return old_type;
	if (old_type.kind == Type::ARRAY)
	{
		long long bound = old_type.array_bound;
		if (bound < 0 && new_type.array_bound >= 0)
			bound = new_type.array_bound;
		return Type::Array(bound,
			MergeDeclarationType(*old_type.child, *new_type.child));
	}
	if (old_type.kind == Type::FUNCTION)
		return SameType(old_type, new_type) ? old_type : old_type;
	return old_type;
}

enum EntityKind
{
	TYPEDEF_ENTITY,
	VARIABLE_ENTITY,
	FUNCTION_ENTITY
};

struct Entity
{
	EntityKind kind;
	string name;
	Type type;

	Entity(EntityKind kind, const string& name, const Type& type)
		: kind(kind), name(name), type(type)
	{}
};

struct Namespace
{
	string name;
	bool unnamed;
	bool inline_namespace;
	Namespace* parent;
	vector<Namespace*> children;
	map<string, Namespace*> named_children;
	map<string, Namespace*> namespace_aliases;
	vector<Namespace*> using_directives;
	map<string, vector<Entity*> > bindings;
	vector<Entity*> owned_entities;
	vector<Entity*> variables;
	vector<Entity*> functions;

	Namespace(const string& name, bool unnamed, bool inline_namespace,
		Namespace* parent)
		: name(name), unnamed(unnamed), inline_namespace(inline_namespace),
		  parent(parent), children(), named_children(), namespace_aliases(),
		  using_directives(), bindings(), owned_entities(), variables(),
		  functions()
	{}

	~Namespace()
	{
		for (size_t i = 0; i < children.size(); ++i) delete children[i];
		for (size_t i = 0; i < owned_entities.size(); ++i)
			delete owned_entities[i];
	}

	Entity* AddTypeDef(const string& entity_name, const Type& type)
	{
		vector<Entity*>& entries = bindings[entity_name];
		for (size_t i = 0; i < entries.size(); ++i)
		{
			if (entries[i]->kind == TYPEDEF_ENTITY)
			{
				entries[i]->type = type;
				return entries[i];
			}
		}
		Entity* entity = new Entity(TYPEDEF_ENTITY, entity_name, type);
		owned_entities.push_back(entity);
		entries.push_back(entity);
		return entity;
	}

	Entity* AddOrdinary(const string& entity_name, const Type& type)
	{
		const EntityKind kind = type.kind == Type::FUNCTION ?
			FUNCTION_ENTITY : VARIABLE_ENTITY;
		vector<Entity*>& entries = bindings[entity_name];
		for (size_t i = 0; i < entries.size(); ++i)
		{
			if (entries[i]->kind != kind) continue;
			entries[i]->type = MergeDeclarationType(entries[i]->type, type);
			return entries[i];
		}

		Entity* entity = new Entity(kind, entity_name, type);
		owned_entities.push_back(entity);
		entries.push_back(entity);
		if (kind == FUNCTION_ENTITY) functions.push_back(entity);
		else variables.push_back(entity);
		return entity;
	}

	void AddImport(Entity* entity)
	{
		if (entity == NULL) return;
		vector<Entity*>& entries = bindings[entity->name];
		for (size_t i = 0; i < entries.size(); ++i)
			if (entries[i] == entity) return;
		entries.push_back(entity);
	}
};

struct NamePath
{
	bool absolute;
	vector<string> parts;

	NamePath() : absolute(false), parts() {}
};

struct DeclaratorOp
{
	enum Kind { POINTER, LVALUE_REFERENCE, RVALUE_REFERENCE, ARRAY, FUNCTION };
	Kind kind;
	bool is_const;
	bool is_volatile;
	long long bound;
	vector<Type> parameters;
	bool variadic;

	DeclaratorOp(Kind kind)
		: kind(kind), is_const(false), is_volatile(false), bound(-1),
		  parameters(), variadic(false)
	{}
};

struct DeclaratorShape
{
	vector<DeclaratorOp> operations;
	bool has_name;
	NamePath name;

	DeclaratorShape() : operations(), has_name(false), name() {}
};

struct DeclSpec
{
	Type type;
	bool is_typedef;

	DeclSpec() : type(), is_typedef(false) {}
};

bool IsKeyword(const string& text)
{
	static const set<string> keywords = {
		"alignas", "alignof", "asm", "auto", "bool", "break", "case",
		"catch", "char", "char16_t", "char32_t", "class", "const",
		"constexpr", "continue", "decltype", "default", "delete", "do",
		"double", "else", "enum", "extern", "false", "float", "for",
		"friend", "goto", "if", "inline", "int", "long", "mutable",
		"namespace", "new", "nullptr", "operator", "private", "protected",
		"public", "register", "return", "short", "signed", "sizeof",
		"static", "struct", "switch", "template", "this", "thread_local",
		"throw", "true", "try", "typedef", "typeid", "typename", "union",
		"unsigned", "using", "virtual", "void", "volatile", "wchar_t",
		"while", "and", "and_eq", "bitand", "bitor", "compl", "not",
		"not_eq", "or", "or_eq", "xor", "xor_eq"
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

bool IsStorageWord(const string& text)
{
	return text == "static" || text == "thread_local" || text == "extern";
}

bool IsPointerOperator(const string& text)
{
	return text == "*" || text == "&" || text == "&&";
}

class Parser
{
public:
	Parser(const vector<PostPPToken>& source, Namespace* root)
		: tokens_(), position_(0), root_(root), current_(root)
	{
		for (size_t i = 0; i < source.size(); ++i)
		{
			if (source[i].kind == POST_PP_EOF) continue;
			PostPPToken token = source[i];
			if (token.kind == POST_PP_PUNCTUATOR)
			{
				if (token.source == "<:") token.source = "[";
				else if (token.source == ":>") token.source = "]";
				else if (token.source == "<%") token.source = "{";
				else if (token.source == "%>") token.source = "}";
			}
			tokens_.push_back(token);
		}
	}

	void ParseTranslationUnit()
	{
		while (!AtEnd()) ParseDeclaration(current_);
	}

private:
	enum DeclaratorMode { NAMED_REQUIRED, NAMED_OPTIONAL, ABSTRACT_ONLY };

	vector<PostPPToken> tokens_;
	size_t position_;
	Namespace* root_;
	Namespace* current_;

	const PostPPToken& Peek(size_t offset = 0) const
	{
		static const PostPPToken eof(POST_PP_EOF);
		const size_t index = position_ + offset;
		return index < tokens_.size() ? tokens_[index] : eof;
	}

	bool AtEnd() const { return position_ >= tokens_.size(); }

	bool Is(const string& text) const { return Peek().source == text; }

	bool Take(const string& text)
	{
		if (!Is(text)) return false;
		++position_;
		return true;
	}

	void Expect(const string& text)
	{
		if (!Take(text))
			throw logic_error("nsdecl: expected '" + text + "'");
	}

	bool IsNameToken(const PostPPToken& token) const
	{
		return IsIdentifier(token);
	}

	string TakeName()
	{
		if (!IsNameToken(Peek()))
			throw logic_error("nsdecl: expected identifier");
		return tokens_[position_++].source;
	}

	NamePath ParseNamePath()
	{
		NamePath result;
		result.absolute = Take("::");
		result.parts.push_back(TakeName());
		while (Take("::")) result.parts.push_back(TakeName());
		return result;
	}

	bool IsNamePathStart() const
	{
		return IsNameToken(Peek()) || Is("::");
	}

	Namespace* CreateNamedNamespace(Namespace* parent, const string& name,
		bool inline_namespace)
	{
		map<string, Namespace*>::iterator found = parent->named_children.find(name);
		if (found != parent->named_children.end())
		{
			if (inline_namespace) found->second->inline_namespace = true;
			return found->second;
		}
		Namespace* child = new Namespace(name, false, inline_namespace, parent);
		parent->children.push_back(child);
		parent->named_children[name] = child;
		return child;
	}

	Namespace* CreateUnnamedNamespace(Namespace* parent, bool inline_namespace)
	{
		for (size_t i = 0; i < parent->children.size(); ++i)
		{
			if (!parent->children[i]->unnamed) continue;
			if (inline_namespace) parent->children[i]->inline_namespace = true;
			return parent->children[i];
		}
		Namespace* child = new Namespace(string(), true, inline_namespace, parent);
		parent->children.push_back(child);
		return child;
	}

	Namespace* FindNamespaceIn(Namespace* scope, const string& name,
		set<Namespace*>* visited) const
	{
		if (scope == NULL || !visited->insert(scope).second) return NULL;
		map<string, Namespace*>::const_iterator named =
			scope->named_children.find(name);
		if (named != scope->named_children.end()) return named->second;
		map<string, Namespace*>::const_iterator alias =
			scope->namespace_aliases.find(name);
		if (alias != scope->namespace_aliases.end()) return alias->second;

		for (size_t i = 0; i < scope->children.size(); ++i)
		{
			Namespace* child = scope->children[i];
			if (!child->unnamed && !child->inline_namespace) continue;
			Namespace* result = FindNamespaceIn(child, name, visited);
			if (result != NULL) return result;
		}
		for (size_t i = 0; i < scope->using_directives.size(); ++i)
		{
			Namespace* result = FindNamespaceIn(scope->using_directives[i], name,
				visited);
			if (result != NULL) return result;
		}
		return NULL;
	}

	Entity* FindEntityIn(Namespace* scope, const string& name,
		set<Namespace*>* visited) const
	{
		if (scope == NULL || !visited->insert(scope).second) return NULL;
		map<string, vector<Entity*> >::const_iterator direct =
			scope->bindings.find(name);
		if (direct != scope->bindings.end() && !direct->second.empty())
			return direct->second.front();

		for (size_t i = 0; i < scope->children.size(); ++i)
		{
			Namespace* child = scope->children[i];
			if (!child->unnamed && !child->inline_namespace) continue;
			Entity* result = FindEntityIn(child, name, visited);
			if (result != NULL) return result;
		}
		for (size_t i = 0; i < scope->using_directives.size(); ++i)
		{
			Entity* result = FindEntityIn(scope->using_directives[i], name,
				visited);
			if (result != NULL) return result;
		}
		return NULL;
	}

	Entity* FindTypeIn(Namespace* scope, const string& name,
		set<Namespace*>* visited) const
	{
		if (scope == NULL || !visited->insert(scope).second) return NULL;
		map<string, vector<Entity*> >::const_iterator direct =
			scope->bindings.find(name);
		if (direct != scope->bindings.end())
		{
			for (size_t i = 0; i < direct->second.size(); ++i)
				if (direct->second[i]->kind == TYPEDEF_ENTITY)
					return direct->second[i];
		}

		for (size_t i = 0; i < scope->children.size(); ++i)
		{
			Namespace* child = scope->children[i];
			if (!child->unnamed && !child->inline_namespace) continue;
			Entity* result = FindTypeIn(child, name, visited);
			if (result != NULL) return result;
		}
		for (size_t i = 0; i < scope->using_directives.size(); ++i)
		{
			Entity* result = FindTypeIn(scope->using_directives[i], name,
				visited);
			if (result != NULL) return result;
		}
		return NULL;
	}

	Entity* LookupType(const string& name, Namespace* from) const
	{
		set<Namespace*> visited;
		for (Namespace* scope = from; scope != NULL; scope = scope->parent)
		{
			Entity* result = FindTypeIn(scope, name, &visited);
			if (result != NULL) return result;
		}
		return NULL;
	}

	Entity* LookupEntity(const string& name, Namespace* from) const
	{
		set<Namespace*> visited;
		for (Namespace* scope = from; scope != NULL; scope = scope->parent)
		{
			Entity* result = FindEntityIn(scope, name, &visited);
			if (result != NULL) return result;
		}
		return NULL;
	}

	Namespace* LookupNamespace(const string& name, Namespace* from) const
	{
		set<Namespace*> visited;
		for (Namespace* scope = from; scope != NULL; scope = scope->parent)
		{
			Namespace* result = FindNamespaceIn(scope, name, &visited);
			if (result != NULL) return result;
		}
		return NULL;
	}

	Namespace* ResolveNamespacePath(const NamePath& path, Namespace* from) const
	{
		if (path.parts.empty()) return NULL;
		Namespace* result = NULL;
		if (path.absolute)
		{
			set<Namespace*> visited;
			result = FindNamespaceIn(root_, path.parts[0], &visited);
		}
		else
			result = LookupNamespace(path.parts[0], from);
		if (result == NULL) return NULL;
		for (size_t i = 1; i < path.parts.size(); ++i)
		{
			set<Namespace*> visited;
			result = FindNamespaceIn(result, path.parts[i], &visited);
			if (result == NULL) return NULL;
		}
		return result;
	}

	Entity* ResolveTypePath(const NamePath& path, Namespace* from) const
	{
		if (path.parts.empty()) return NULL;
		if (path.parts.size() == 1 && !path.absolute)
			return LookupType(path.parts[0], from);

		NamePath namespace_path = path;
		namespace_path.parts.pop_back();
		Namespace* scope = NULL;
		if (namespace_path.parts.empty()) scope = root_;
		else scope = ResolveNamespacePath(namespace_path, from);
		if (scope == NULL) return NULL;
		set<Namespace*> visited;
		return FindTypeIn(scope, path.parts.back(), &visited);
	}

	Entity* ResolveEntityPath(const NamePath& path, Namespace* from) const
	{
		if (path.parts.empty()) return NULL;
		if (path.parts.size() == 1 && !path.absolute)
			return LookupEntity(path.parts[0], from);
		NamePath namespace_path = path;
		namespace_path.parts.pop_back();
		Namespace* scope = namespace_path.parts.empty() ? root_ :
			ResolveNamespacePath(namespace_path, from);
		if (scope == NULL) return NULL;
		set<Namespace*> visited;
		return FindEntityIn(scope, path.parts.back(), &visited);
	}

	Namespace* TargetNamespaceForName(const NamePath& path) const
	{
		if (path.parts.size() == 1 && !path.absolute) return current_;
		NamePath namespace_path = path;
		namespace_path.parts.pop_back();
		if (namespace_path.parts.empty()) return root_;
		return ResolveNamespacePath(namespace_path, current_);
	}

	Type MakeFundamental(const vector<string>& words) const
	{
		bool is_signed = false;
		bool is_unsigned = false;
		bool is_short = false;
		int long_count = 0;
		bool has_int = false;
		bool has_char = false;
		bool has_char16 = false;
		bool has_char32 = false;
		bool has_wchar = false;
		bool has_bool = false;
		bool has_float = false;
		bool has_double = false;
		bool has_void = false;

		for (size_t i = 0; i < words.size(); ++i)
		{
			const string& word = words[i];
			if (word == "signed") is_signed = true;
			else if (word == "unsigned") is_unsigned = true;
			else if (word == "short") is_short = true;
			else if (word == "long") ++long_count;
			else if (word == "int") has_int = true;
			else if (word == "char") has_char = true;
			else if (word == "char16_t") has_char16 = true;
			else if (word == "char32_t") has_char32 = true;
			else if (word == "wchar_t") has_wchar = true;
			else if (word == "bool") has_bool = true;
			else if (word == "float") has_float = true;
			else if (word == "double") has_double = true;
			else if (word == "void") has_void = true;
		}

		if (has_char) return Type::Fundamental(is_signed ? "signed char" :
			is_unsigned ? "unsigned char" : "char");
		if (has_char16) return Type::Fundamental("char16_t");
		if (has_char32) return Type::Fundamental("char32_t");
		if (has_wchar) return Type::Fundamental("wchar_t");
		if (has_bool) return Type::Fundamental("bool");
		if (has_float) return Type::Fundamental("float");
		if (has_double)
			return Type::Fundamental(long_count == 1 ? "long double" : "double");
		if (has_void) return Type::Fundamental("void");
		if (is_short) return Type::Fundamental(is_unsigned ?
			"unsigned short int" : "short int");
		if (long_count >= 2) return Type::Fundamental(is_unsigned ?
			"unsigned long long int" : "long long int");
		if (long_count == 1) return Type::Fundamental(is_unsigned ?
			"unsigned long int" : "long int");
		if (is_unsigned) return Type::Fundamental("unsigned int");
		(void)has_int;
		return Type::Fundamental("int");
	}

	DeclSpec ParseDeclSpecifierSeq()
	{
		DeclSpec result;
		vector<string> fundamental;
		bool have_named_type = false;
		bool is_const = false;
		bool is_volatile = false;

		while (true)
		{
			const string word = Peek().source;
			if (IsStorageWord(word))
			{
				++position_;
				continue;
			}
			if (word == "typedef")
			{
				result.is_typedef = true;
				++position_;
				continue;
			}
			if (word == "const")
			{
				is_const = true;
				++position_;
				continue;
			}
			if (word == "volatile")
			{
				is_volatile = true;
				++position_;
				continue;
			}
			if (IsFundamentalWord(word))
			{
				fundamental.push_back(word);
				++position_;
				continue;
			}
			if (fundamental.empty() && !have_named_type && IsNamePathStart())
			{
				const NamePath path = ParseNamePath();
				Entity* entity = ResolveTypePath(path, current_);
				if (entity == NULL || entity->kind != TYPEDEF_ENTITY)
					throw logic_error("nsdecl: unknown typedef name");
				result.type = entity->type;
				have_named_type = true;
				continue;
			}
			break;
		}

		if (!fundamental.empty() && have_named_type)
			throw logic_error("nsdecl: mixed type specifiers");
		if (!have_named_type && fundamental.empty())
			throw logic_error("nsdecl: missing type specifier");
		if (!have_named_type) result.type = MakeFundamental(fundamental);
		result.type = AddCv(result.type, is_const, is_volatile);
		return result;
	}

	long long ParseArrayBound()
	{
		if (Is("]")) return -1;
		if (Peek().kind != POST_PP_NUMBER)
			throw logic_error("nsdecl: array bound is not a literal");
		const string text = Peek().source;
		++position_;
		char* end = NULL;
		unsigned long long value = strtoull(text.c_str(), &end, 0);
		if (end == text.c_str() || *end != '\0' || value == 0 ||
			value > static_cast<unsigned long long>(numeric_limits<long long>::max()))
			throw logic_error("nsdecl: invalid array bound");
		return static_cast<long long>(value);
	}

	bool LooksLikeAbstractFunctionSuffix() const
	{
		if (!Is("(")) return false;
		const string next = Peek(1).source;
		const bool starts_type = IsFundamentalWord(next) || IsCvWord(next) ||
			next == "typedef" || next == "static" || next == "extern" ||
			next == "thread_local";
		return next == ")" || next == "..." || starts_type ||
			Peek(1).kind == POST_PP_IDENTIFIER || next == "::";
	}

	DeclaratorShape ParseDeclarator(DeclaratorMode mode)
	{
		vector<DeclaratorOp> prefix;
		while (IsPointerOperator(Peek().source))
		{
			const string op = Peek().source;
			++position_;
			DeclaratorOp::Kind kind = DeclaratorOp::POINTER;
			if (op == "&") kind = DeclaratorOp::LVALUE_REFERENCE;
			else if (op == "&&") kind = DeclaratorOp::RVALUE_REFERENCE;
			DeclaratorOp parsed(kind);
			while (IsCvWord(Peek().source))
			{
				if (Take("const")) parsed.is_const = true;
				else if (Take("volatile")) parsed.is_volatile = true;
			}
			prefix.push_back(parsed);
		}

		DeclaratorShape direct;
		bool have_root = false;
		if (mode != ABSTRACT_ONLY && IsNamePathStart())
		{
			direct.has_name = true;
			direct.name = ParseNamePath();
			have_root = true;
		}
		else if (Take("("))
		{
			const bool grouped = mode != ABSTRACT_ONLY ||
				IsPointerOperator(Peek().source);
			if (grouped)
			{
				const DeclaratorMode inner_mode = mode == NAMED_REQUIRED ?
					NAMED_OPTIONAL : mode;
				direct = ParseDeclarator(inner_mode);
				Expect(")");
				have_root = direct.has_name || !direct.operations.empty();
			}
			else
			{
				--position_;
			}
		}
		else if (mode == NAMED_REQUIRED)
			throw logic_error("nsdecl: expected declarator-id");

		if (!have_root && mode == NAMED_REQUIRED)
			throw logic_error("nsdecl: expected declarator-id");

		while (true)
		{
			if (Take("["))
			{
				DeclaratorOp array(DeclaratorOp::ARRAY);
				array.bound = ParseArrayBound();
				Expect("]");
				direct.operations.insert(direct.operations.begin(), array);
				continue;
			}
			if (Take("("))
			{
				DeclaratorOp function(DeclaratorOp::FUNCTION);
				ParseParameterClause(&function.parameters, &function.variadic);
				Expect(")");
				direct.operations.insert(direct.operations.begin(), function);
				continue;
			}
			break;
		}

		DeclaratorShape result = direct;
		result.operations.insert(result.operations.begin(), prefix.begin(),
			prefix.end());
		return result;
	}

	void ParseParameterClause(vector<Type>* parameters, bool* variadic)
	{
		if (Is(")")) return;
		if (Take("..."))
		{
			*variadic = true;
			return;
		}

		while (true)
		{
			DeclSpec spec = ParseDeclSpecifierSeq();
			DeclaratorShape shape;
			if (IsPointerOperator(Peek().source) || IsNamePathStart() ||
				Is("[") || Is("("))
			{
				const DeclaratorMode mode = Is("(") &&
					LooksLikeAbstractFunctionSuffix() ? ABSTRACT_ONLY :
					NAMED_OPTIONAL;
				shape = ParseDeclarator(mode);
			}
			Type parameter = ApplyDeclarator(spec.type, shape);

			// A lone void is the spelling of an empty parameter list.
			if (!(parameter.kind == Type::FUNDAMENTAL &&
				parameter.fundamental == "void" && parameters->empty() &&
				!shape.has_name && shape.operations.empty()))
				parameters->push_back(AdjustParameter(parameter));

			if (Take(","))
			{
				if (Take("..."))
				{
					*variadic = true;
					return;
				}
				continue;
			}
			if (Take("...")) *variadic = true;
			return;
		}
	}

	Type AdjustParameter(const Type& parameter) const
	{
		Type result = parameter;
		if (result.kind == Type::FUNCTION)
			result = Type::Pointer(result);
		else if (result.kind == Type::ARRAY)
			result = Type::Pointer(*result.child);
		return RemoveTopCv(result);
	}

	Type ApplyDeclarator(const Type& base, const DeclaratorShape& shape) const
	{
		Type result = base;
		for (size_t i = 0; i < shape.operations.size(); ++i)
		{
			const DeclaratorOp& op = shape.operations[i];
			switch (op.kind)
			{
			case DeclaratorOp::POINTER:
				result = Type::Pointer(result, op.is_const, op.is_volatile);
				break;
			case DeclaratorOp::LVALUE_REFERENCE:
				result = CollapseReference(Type::LVALUE_REFERENCE, result);
				break;
			case DeclaratorOp::RVALUE_REFERENCE:
				result = CollapseReference(Type::RVALUE_REFERENCE, result);
				break;
			case DeclaratorOp::ARRAY:
				result = Type::Array(op.bound, result);
				break;
			case DeclaratorOp::FUNCTION:
				result = Type::Function(op.parameters, op.variadic, result);
				break;
			}
		}
		return result;
	}

	void ParseNamespaceDefinition(Namespace* parent, bool inline_namespace)
	{
		Expect("namespace");
		Namespace* child = NULL;
		if (IsNameToken(Peek()))
			child = CreateNamedNamespace(parent, TakeName(), inline_namespace);
		else
			child = CreateUnnamedNamespace(parent, inline_namespace);
		Expect("{");
		Namespace* saved = current_;
		current_ = child;
		while (!Is("}"))
		{
			if (AtEnd()) throw logic_error("nsdecl: unterminated namespace");
			ParseDeclaration(child);
		}
		Expect("}");
		current_ = saved;
		Take(";");
	}

	void ParseNamespaceAliasDefinition(Namespace* parent)
	{
		Expect("namespace");
		const string alias_name = TakeName();
		Expect("=");
		const NamePath path = ParseNamePath();
		Namespace* target = ResolveNamespacePath(path, parent);
		if (target == NULL) throw logic_error("nsdecl: unknown namespace");
		parent->namespace_aliases[alias_name] = target;
		Expect(";");
	}

	void ParseUsingDirective(Namespace* parent)
	{
		Expect("using");
		Expect("namespace");
		const NamePath path = ParseNamePath();
		Namespace* target = ResolveNamespacePath(path, parent);
		if (target == NULL) throw logic_error("nsdecl: unknown namespace");
		for (size_t i = 0; i < parent->using_directives.size(); ++i)
			if (parent->using_directives[i] == target) { Expect(";"); return; }
		parent->using_directives.push_back(target);
		Expect(";");
	}

	void ParseUsingDeclaration(Namespace* parent)
	{
		Expect("using");
		const NamePath path = ParseNamePath();
		Entity* target = ResolveEntityPath(path, parent);
		if (target == NULL) throw logic_error("nsdecl: unknown using target");
		parent->AddImport(target);
		Expect(";");
	}

	void ParseAliasDeclaration(Namespace* parent)
	{
		Expect("using");
		const string alias_name = TakeName();
		Expect("=");
		const Type type = ParseTypeId();
		parent->AddTypeDef(alias_name, type);
		Expect(";");
	}

	Type ParseTypeId()
	{
		const DeclSpec spec = ParseDeclSpecifierSeq();
		DeclaratorShape shape;
		if (IsPointerOperator(Peek().source) || Is("[") || Is("("))
			shape = ParseDeclarator(ABSTRACT_ONLY);
		return ApplyDeclarator(spec.type, shape);
	}

	void ParseSimpleDeclaration(Namespace* parent)
	{
		const DeclSpec spec = ParseDeclSpecifierSeq();
		while (true)
		{
			const DeclaratorShape shape = ParseDeclarator(NAMED_REQUIRED);
			const Type type = ApplyDeclarator(spec.type, shape);
			Namespace* target = TargetNamespaceForName(shape.name);
			if (target == NULL) throw logic_error("nsdecl: unknown declaration namespace");
			const string entity_name = shape.name.parts.back();
			if (spec.is_typedef) target->AddTypeDef(entity_name, type);
			else target->AddOrdinary(entity_name, type);
			if (!Take(",")) break;
		}
		Expect(";");
		(void)parent;
	}

	void ParseDeclaration(Namespace* parent)
	{
		if (Take(";")) return;
		if (Is("inline"))
		{
			Take("inline");
			if (!Is("namespace"))
				throw logic_error("nsdecl: inline declaration is not a namespace");
			ParseNamespaceDefinition(parent, true);
			return;
		}
		if (Is("namespace"))
		{
			if (IsNameToken(Peek(1)) && Peek(2).source == "=")
			{
				ParseNamespaceAliasDefinition(parent);
				return;
			}
			ParseNamespaceDefinition(parent, false);
			return;
		}
		if (Is("using"))
		{
			if (Is("using") && Peek(1).source == "namespace")
				ParseUsingDirective(parent);
			else if (IsNameToken(Peek(1)) && Peek(2).source == "=")
				ParseAliasDeclaration(parent);
			else
				ParseUsingDeclaration(parent);
			return;
		}
		ParseSimpleDeclaration(parent);
	}

	};

void EmitNamespace(const Namespace& scope, ostream& out)
{
	if (scope.unnamed) out << "start unnamed namespace\n";
	else out << "start namespace " << scope.name << "\n";
	if (scope.inline_namespace) out << "inline namespace\n";
	for (size_t i = 0; i < scope.variables.size(); ++i)
		out << "variable " << scope.variables[i]->name << " "
			<< scope.variables[i]->type.ToString() << "\n";
	for (size_t i = 0; i < scope.functions.size(); ++i)
		out << "function " << scope.functions[i]->name << " "
			<< scope.functions[i]->type.ToString() << "\n";
	for (size_t i = 0; i < scope.children.size(); ++i)
		EmitNamespace(*scope.children[i], out);
	out << "end namespace\n";
}

} // namespace

void EmitNSDeclTranslationUnit(const string& source_path, ostream& out)
{
	const vector<PostPPToken> tokens = PreprocessSourceFile(source_path);
	if (!ValidatePostTokens(tokens))
		throw logic_error("nsdecl: invalid post-token sequence");

	Namespace root(string(), true, false, NULL);
	Parser parser(tokens, &root);
	parser.ParseTranslationUnit();
	if (!tokens.empty())
	{
		// ParseTranslationUnit intentionally accepts only the PA7 grammar; the
		// parser's normal loop must have consumed every translated token.
		// (The EOF token is synthetic and is not present in this stream.)
	}

	EmitNamespace(root, out);
}
