#include "pa11_semantics.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace {

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
	TYPE_FUNCTION
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
	Scope* owned_scope;
	TypePtr underlying;

	Type(TypeKind type_kind = TYPE_FUNDAMENTAL,
		const string& type_name = string())
		: kind(type_kind), name(type_name), tag(), scoped_enum(false),
		  complete(true), underlying_explicit(false), is_const(false), is_volatile(false), child(),
		  bound(-1), parameters(), variadic(false), owned_scope(0),
		  underlying() {}
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

	Binding(BindingKind binding_kind = BIND_VARIABLE,
		const string& binding_name = string(), const TypePtr& binding_type = TypePtr())
		: kind(binding_kind), name(binding_name), type(binding_type),
		  has_value(false), value(0), type_override() {}
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

	Scope(ScopeKind scope_kind, const string& scope_name, Scope* scope_parent)
		: kind(scope_kind), name(scope_name), parent(scope_parent),
		  inline_namespace(false), bindings(), local_bindings(), children(),
		  namespace_children(), namespace_aliases(), using_directives() {}

	Binding* local(const string& key)
	{
		map<string, size_t>::iterator found = local_bindings.find(key);
		return found == local_bindings.end() ? 0 : &bindings[found->second];
	}

	const Binding* local(const string& key) const
	{
		map<string, size_t>::const_iterator found = local_bindings.find(key);
		return found == local_bindings.end() ? 0 : &bindings[found->second];
	}

	Binding* add(Binding binding)
	{
		const size_t index = bindings.size();
		bindings.push_back(binding);
		local_bindings[binding.name] = index;
		return &bindings[index];
	}

	Scope* child(ScopeKind child_kind, const string& child_name)
	{
		children.push_back(unique_ptr<Scope>(new Scope(child_kind, child_name, this)));
		return children.back().get();
	}
};

struct ConstantValue
{
	bool known;
	long long value;

	ConstantValue(bool is_known = false, long long constant = 0)
		: known(is_known), value(constant) {}
};

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

string TypeText(const TypePtr& type);

TypePtr Fundamental(const string& name)
{
	return TypePtr(new Type(TYPE_FUNDAMENTAL, name));
}

TypePtr CloneWithCv(const TypePtr& original, bool add_const, bool add_volatile)
{
	if (!original) return original;
	if (!add_const && !add_volatile) return original;
	if (original->kind == TYPE_LVALUE_REFERENCE ||
		original->kind == TYPE_RVALUE_REFERENCE)
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
		referred->kind == TYPE_RVALUE_REFERENCE))
	{
		if (reference_kind == TYPE_LVALUE_REFERENCE ||
			referred->kind == TYPE_LVALUE_REFERENCE)
			return TypePtr(new Type(*referred));
	}
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
	const TypePtr& result_type)
{
	TypePtr result(new Type(TYPE_FUNCTION));
	result->parameters = parameters;
	result->variadic = variadic;
	result->child = result_type;
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

string TypeText(const TypePtr& type)
{
	if (!type) return "<invalid type>";
	string result = CvPrefix(type);
	switch (type->kind)
	{
	case TYPE_FUNDAMENTAL:
		return result + type->name;
	case TYPE_CLASS:
		return result + type->tag + " " + type->name;
	case TYPE_ENUM:
		return result + string(type->scoped_enum ? "enum class " : "enum ") + type->name;
	case TYPE_TEMPLATE_PARAMETER:
		return result + "typename " + type->name;
	case TYPE_TEMPLATE_TEMPLATE_PARAMETER:
		return result + "template-parameter " + type->name;
	case TYPE_POINTER:
		return result + "pointer to " + TypeText(type->child);
	case TYPE_LVALUE_REFERENCE:
		return result + "lvalue-reference to " + TypeText(type->child);
	case TYPE_RVALUE_REFERENCE:
		return result + "rvalue-reference to " + TypeText(type->child);
	case TYPE_ARRAY:
		{
			ostringstream bound;
			bound << type->bound;
			return result + "array of " + bound.str() + " " + TypeText(type->child);
		}
	case TYPE_FUNCTION:
		{
			ostringstream output;
			output << result << "function of (";
			for (size_t i = 0; i < type->parameters.size(); ++i)
			{
				if (i != 0) output << ", ";
				output << TypeText(type->parameters[i]);
			}
			if (type->variadic)
			{
				if (!type->parameters.empty()) output << ", ";
				output << "...";
			}
			output << ") returning " << TypeText(type->child);
			return output.str();
		}
	}
	return "<invalid type>";
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
		if (node->children[i] && node->children[i]->kind == kind)
			return node->children[i];
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

class Analyzer
{
public:
	Analyzer()
		: global_(new Scope(SCOPE_NAMESPACE, "<global>", 0)),
		  anonymous_union_count_(0) {}

	void Analyze(const CPPGMAstNodePtr& tree)
	{
		if (!tree || tree->kind != "translation-unit")
			throw logic_error("invalid translation unit");
		for (size_t i = 0; i < tree->children.size(); ++i)
			Process(tree->children[i], global_.get());
	}

	void Print(ostream& out) const
	{
		out << "translation-unit\n";
		PrintScope(global_.get(), out, 1);
	}

private:
	unique_ptr<Scope> global_;
	unsigned int anonymous_union_count_;

	static void Indent(ostream& out, unsigned int indentation)
	{
		for (unsigned int i = 0; i < indentation; ++i) out << "  ";
	}

	void PrintScope(const Scope* scope, ostream& out, unsigned int indentation) const
	{
		Indent(out, indentation);
		out << "scope " << ScopeKindText(scope->kind);
		if (scope->kind != SCOPE_TEMPLATE_PARAMETERS && scope->kind != SCOPE_BLOCK)
			out << " " << scope->name;
		out << "\n";
		for (size_t i = 0; i < scope->bindings.size(); ++i)
		{
			const Binding& binding = scope->bindings[i];
			Indent(out, indentation + 1);
			out << BindingKindText(binding.kind) << " " << binding.name << " ";
			if (!binding.type_override.empty()) out << binding.type_override;
			else out << TypeText(binding.type);
			if (binding.kind == BIND_ENUMERATOR)
				out << " " << binding.value;
			out << "\n";
		}
		for (size_t i = 0; i < scope->children.size(); ++i)
			PrintScope(scope->children[i].get(), out, indentation + 1);
	}

	Scope* NewChild(Scope* parent, ScopeKind kind, const string& name)
	{
		return parent->child(kind, name);
	}

	Scope* FindNamespaceDirect(Scope* from, const string& name) const
	{
		map<string, Scope*>::const_iterator child = from->namespace_children.find(name);
		if (child != from->namespace_children.end()) return child->second;
		map<string, Scope*>::const_iterator alias = from->namespace_aliases.find(name);
		if (alias != from->namespace_aliases.end()) return alias->second;
		for (size_t i = 0; i < from->using_directives.size(); ++i)
		{
			Scope* found = FindNamespaceDirect(from->using_directives[i], name);
			if (found) return found;
		}
		return 0;
	}

	Scope* FindNamespace(Scope* from, const string& name) const
	{
		for (Scope* current = from; current != 0; current = current->parent)
		{
			Scope* found = FindNamespaceDirect(current, name);
			if (found) return found;
		}
		return 0;
	}

	Binding* FindLocalBinding(Scope* from, const string& name) const
	{
		return from ? from->local(name) : 0;
	}

	Binding* LookupUnqualified(Scope* from, const string& name) const
	{
		for (Scope* current = from; current != 0; current = current->parent)
		{
			Binding* direct = current->local(name);
			if (direct) return direct;
			for (size_t i = 0; i < current->using_directives.size(); ++i)
			{
				Binding* imported = LookupInNamespace(current->using_directives[i], name);
				if (imported) return imported;
			}
		}
		return 0;
	}

	Binding* LookupInNamespace(Scope* scope, const string& name) const
	{
		if (!scope) return 0;
		Binding* direct = scope->local(name);
		if (direct) return direct;
		for (size_t i = 0; i < scope->using_directives.size(); ++i)
		{
			Binding* imported = LookupInNamespace(scope->using_directives[i], name);
			if (imported) return imported;
		}
		return 0;
	}

	Scope* ScopeForType(const TypePtr& type) const
	{
		if (!type) return 0;
		if (type->kind == TYPE_CLASS || type->kind == TYPE_ENUM)
			return type->owned_scope;
		return 0;
	}

	struct PathTarget
	{
		Scope* scope;
		Binding* binding;
		PathTarget(Scope* target_scope = 0, Binding* target_binding = 0)
			: scope(target_scope), binding(target_binding) {}
	};

	vector<string> SplitPath(const string& raw, bool* absolute = 0) const
	{
		string path = raw;
		bool is_absolute = path.compare(0, 2, "::") == 0;
		if (is_absolute) path = path.substr(2);
		vector<string> parts;
		size_t begin = 0;
		while (begin <= path.size())
		{
			const size_t end = path.find("::", begin);
			string part = path.substr(begin, end == string::npos ? string::npos : end - begin);
			if (!part.empty()) parts.push_back(part);
			if (end == string::npos) break;
			begin = end + 2;
		}
		if (absolute) *absolute = is_absolute;
		return parts;
	}

	PathTarget ResolvePath(Scope* from, const string& raw) const
	{
		bool absolute = false;
		const vector<string> parts = SplitPath(raw, &absolute);
		if (parts.empty()) return PathTarget();
		Scope* current_scope = absolute ? global_.get() : from;
		Binding* current_binding = 0;
		for (size_t i = 0; i < parts.size(); ++i)
		{
			const string& part = parts[i];
			if (current_binding)
			{
				current_scope = ScopeForType(current_binding->type);
				if (!current_scope) return PathTarget();
				current_binding = 0;
			}
			if (i == 0 && !absolute)
			{
				Binding* binding = LookupUnqualified(current_scope, part);
				if (binding)
				{
					current_binding = binding;
					if (i + 1 == parts.size()) return PathTarget(0, binding);
					continue;
				}
			}
			Scope* namespace_scope = (i == 0 && !absolute) ?
				FindNamespace(current_scope, part) : FindNamespaceDirect(current_scope, part);
			if (namespace_scope)
			{
				current_scope = namespace_scope;
				if (i + 1 == parts.size()) return PathTarget(namespace_scope, 0);
				continue;
			}
			Binding* binding = (i == 0 && !absolute) ?
				LookupUnqualified(current_scope, part) : LookupInNamespace(current_scope, part);
			if (!binding) return PathTarget();
			if (i + 1 == parts.size()) return PathTarget(0, binding);
			current_binding = binding;
		}
		return current_binding ? PathTarget(0, current_binding) : PathTarget(current_scope, 0);
	}

	Scope* ResolveNamespace(Scope* from, const string& raw) const
	{
		PathTarget target = ResolvePath(from, raw);
		return target.scope;
	}

	Binding* ResolveBinding(Scope* from, const string& raw) const
	{
		PathTarget target = ResolvePath(from, raw);
		return target.binding;
	}

	TypePtr ResolveType(Scope* from, const string& raw) const
	{
		Binding* binding = ResolveBinding(from, StripTypeMarker(raw));
		if (!binding || (binding->kind != BIND_TYPE &&
			binding->kind != BIND_TYPE_ALIAS))
			throw logic_error("unknown type: " + raw);
		return binding->type;
	}

	static bool IsFundamentalWord(const string& word)
	{
		return word == "bool" || word == "char" || word == "char16_t" ||
			word == "char32_t" || word == "double" || word == "float" ||
			word == "int" || word == "long" || word == "short" ||
			word == "signed" || word == "unsigned" || word == "void" ||
			word == "wchar_t";
	}

	static string FundamentalName(const vector<string>& words)
	{
		bool is_unsigned = false;
		bool is_signed = false;
		int long_count = 0;
		bool is_short = false;
		string base;
		for (size_t i = 0; i < words.size(); ++i)
		{
			if (words[i] == "unsigned") is_unsigned = true;
			else if (words[i] == "signed") is_signed = true;
			else if (words[i] == "long") ++long_count;
			else if (words[i] == "short") is_short = true;
			else if (words[i] != "int") base = words[i];
		}
		if (base.empty()) base = "int";
		if (base == "int" || base == "char")
		{
			string result;
			if (is_unsigned) result = "unsigned ";
			else if (is_signed) result = "signed ";
			if (is_short) result += "short int";
			else if (long_count >= 2) result += "long long int";
			else if (long_count == 1) result += "long int";
			else if (base == "char") result += "char";
			else result += "int";
			return result;
		}
		return base;
	}

	static bool IsCvNode(const CPPGMAstNodePtr& node, const string& word)
	{
		return node && ((node->kind == "cv-qualifier" || node->kind == "decl-specifier") &&
			node->value.find(":" + word) != string::npos);
	}

	struct SpecFacts
	{
		bool is_typedef;
		bool is_constexpr;
		bool is_const;
		bool is_volatile;
		vector<string> fundamental_words;
		TypePtr named_type;
		SpecFacts()
			: is_typedef(false), is_constexpr(false), is_const(false),
			  is_volatile(false), fundamental_words(), named_type() {}
	};

	TypePtr TypeFromDecltype(const CPPGMAstNodePtr& node, Scope* scope)
	{
		if (!node || node->children.empty()) throw logic_error("invalid decltype");
		const CPPGMAstNodePtr expression = node->children[0];
		Binding* binding = 0;
		if (expression->kind == "id-expression")
			binding = ResolveBinding(scope, expression->value);
		else if (expression->kind == "parenthesized-expression" &&
			!expression->children.empty() && expression->children[0] &&
			expression->children[0]->kind == "id-expression")
			binding = ResolveBinding(scope, expression->children[0]->value);
		if (!binding)
			throw logic_error("unknown decltype expression");
		TypePtr result = binding->type;
		if (expression->kind == "parenthesized-expression")
		{
			const CPPGMAstNodePtr inner = expression->children.empty() ?
				CPPGMAstNodePtr() : expression->children[0];
			if (inner && inner->kind == "id-expression")
			{
				Binding* inner_binding = ResolveBinding(scope, inner->value);
				if (inner_binding && (inner_binding->kind == BIND_VARIABLE ||
					inner_binding->kind == BIND_PARAMETER))
					result = ReferenceTo(TYPE_LVALUE_REFERENCE, result);
			}
		}
		return result;
	}

	TypePtr TypeFromSpecSeq(const CPPGMAstNodePtr& sequence, Scope* scope,
		SpecFacts* facts = 0)
	{
		if (!sequence) throw logic_error("missing declaration specifiers");
		SpecFacts local_facts;
		SpecFacts& info = facts ? *facts : local_facts;
		vector<string> fundamentals;
		for (size_t i = 0; i < sequence->children.size(); ++i)
		{
			const CPPGMAstNodePtr child = sequence->children[i];
			if (!child) continue;
			if (child->kind == "class-specifier")
			{
				info.named_type = ProcessClass(child, scope);
				continue;
			}
			if (child->kind == "enum-specifier")
			{
				info.named_type = ProcessEnum(child, scope);
				continue;
			}
			if (child->kind == "type-name")
			{
				info.named_type = ResolveType(scope, child->value);
				continue;
			}
			if (child->kind == "type-specifier")
			{
				const size_t colon = child->value.find(':');
				const string word = colon == string::npos ? child->value : child->value.substr(colon + 1);
				if (IsFundamentalWord(word)) fundamentals.push_back(word);
				continue;
			}
			if (child->kind == "cv-qualifier")
			{
				if (child->value.find(":const") != string::npos) info.is_const = true;
				if (child->value.find(":volatile") != string::npos) info.is_volatile = true;
				continue;
			}
			if (child->kind != "decl-specifier") continue;
			const string value = child->value;
			if (value == "KW_TYPEDEF:typedef") info.is_typedef = true;
			else if (value == "KW_CONSTEXPR:constexpr") info.is_constexpr = true;
			else if (value == "KW_CONST:const") info.is_const = true;
			else if (value == "KW_VOLATILE:volatile") info.is_volatile = true;
			else
			{
				const size_t colon = value.find(':');
				const string word = colon == string::npos ? value : value.substr(colon + 1);
				if (IsFundamentalWord(word)) fundamentals.push_back(word);
				else if (value.find("decltype(") == 0)
					info.named_type = TypeFromDecltype(child, scope);
				else if (value.find("TT_IDENTIFIER:") == 0 || value.find("::") != string::npos)
					info.named_type = ResolveType(scope, StripTypeMarker(value));
			}
		}
		TypePtr result = info.named_type;
		if (!result && !fundamentals.empty()) result = Fundamental(FundamentalName(fundamentals));
		if (!result) throw logic_error("declaration has no type");
		return CloneWithCv(result, info.is_const, info.is_volatile);
	}

	TypePtr TypeFromTypeId(const CPPGMAstNodePtr& type_id, Scope* scope)
	{
		if (!type_id || type_id->kind != "type-id" || type_id->children.empty())
			throw logic_error("invalid type-id");
		SpecFacts facts;
		TypePtr base = TypeFromSpecSeq(type_id->children[0], scope, &facts);
		if (type_id->children.size() > 1)
			base = BuildDeclarator(type_id->children[1], base, scope);
		return base;
	}

	long long ParseLiteral(const string& raw) const
	{
		string value = raw;
		while (!value.empty() &&
			(value[value.size() - 1] == 'u' || value[value.size() - 1] == 'U' ||
			 value[value.size() - 1] == 'l' || value[value.size() - 1] == 'L'))
			value.erase(value.size() - 1);
		if (value.empty()) throw logic_error("invalid integer literal");
		errno = 0;
		char* end = 0;
		const long long result = strtoll(value.c_str(), &end, 0);
		if (errno == ERANGE || end == value.c_str() || *end != '\0')
			throw logic_error("unsupported constant expression");
		return result;
	}

	ConstantValue Evaluate(const CPPGMAstNodePtr& expression, Scope* scope)
	{
		if (!expression) return ConstantValue();
		if (expression->kind == "literal") return ConstantValue(true, ParseLiteral(expression->value));
		if (expression->kind == "keyword-literal")
			return ConstantValue(true, OperatorFromNode(expression->value) == "true" ? 1 : 0);
		if (expression->kind == "id-expression")
		{
			Binding* binding = ResolveBinding(scope, expression->value);
			if (!binding || !binding->has_value) return ConstantValue();
			return ConstantValue(true, binding->value);
		}
		if (expression->kind == "parenthesized-expression")
			return expression->children.empty() ? ConstantValue() : Evaluate(expression->children[0], scope);
		if (expression->kind == "sizeof-expression" || expression->kind == "type-trait-expression")
		{
			if (expression->children.empty()) return ConstantValue();
			const CPPGMAstNodePtr child = expression->children[0];
			TypePtr type;
			if (child->kind == "type-id") type = TypeFromTypeId(child, scope);
			else type = ExpressionType(child, scope);
			const bool align = expression->kind == "type-trait-expression";
			return ConstantValue(true, static_cast<long long>(align ? TypeAlignment(type) : TypeSize(type)));
		}
		if (expression->kind == "cast-expression")
		{
			if (expression->children.size() < 2) return ConstantValue();
			return Evaluate(expression->children[1], scope);
		}
		if (expression->kind == "unary-expression")
		{
			if (expression->children.empty()) return ConstantValue();
			ConstantValue child = Evaluate(expression->children[0], scope);
			if (!child.known) return child;
			const string op = OperatorFromNode(expression->value);
			if (op == "+") return child;
			if (op == "-") return ConstantValue(true, -child.value);
			if (op == "!") return ConstantValue(true, !child.value);
			if (op == "~") return ConstantValue(true, ~child.value);
			return ConstantValue();
		}
		if (expression->kind == "conditional-expression" && expression->children.size() == 3)
		{
			ConstantValue condition = Evaluate(expression->children[0], scope);
			return !condition.known ? ConstantValue() :
				Evaluate(expression->children[condition.value ? 1 : 2], scope);
		}
		if (expression->kind == "binary-expression" || expression->kind == "assignment-expression")
		{
			if (expression->children.size() < 2) return ConstantValue();
			ConstantValue left = Evaluate(expression->children[0], scope);
			ConstantValue right = Evaluate(expression->children[1], scope);
			if (!left.known || !right.known) return ConstantValue();
			const string op = OperatorFromNode(expression->value);
			if (op == "+") return ConstantValue(true, left.value + right.value);
			if (op == "-") return ConstantValue(true, left.value - right.value);
			if (op == "*") return ConstantValue(true, left.value * right.value);
			if (op == "/") return right.value == 0 ? ConstantValue() : ConstantValue(true, left.value / right.value);
			if (op == "%") return right.value == 0 ? ConstantValue() : ConstantValue(true, left.value % right.value);
			if (op == "==") return ConstantValue(true, left.value == right.value);
			if (op == "!=") return ConstantValue(true, left.value != right.value);
			if (op == "<") return ConstantValue(true, left.value < right.value);
			if (op == ">") return ConstantValue(true, left.value > right.value);
			if (op == "<=") return ConstantValue(true, left.value <= right.value);
			if (op == ">=") return ConstantValue(true, left.value >= right.value);
			if (op == "&&" || op == "and") return ConstantValue(true, left.value && right.value);
			if (op == "||" || op == "or") return ConstantValue(true, left.value || right.value);
			if (op == "&" || op == "bitand") return ConstantValue(true, left.value & right.value);
			if (op == "|" || op == "bitor") return ConstantValue(true, left.value | right.value);
			if (op == "^") return ConstantValue(true, left.value ^ right.value);
			if (op == "<<") return ConstantValue(true, left.value << right.value);
			if (op == ">>") return ConstantValue(true, left.value >> right.value);
			if (op == ",") return right;
			return ConstantValue();
		}
		return ConstantValue();
	}

	size_t FundamentalSize(const string& name) const
	{
		if (name == "char" || name == "signed char" || name == "unsigned char" || name == "bool") return 1;
		if (name == "short int" || name == "unsigned short int" || name == "char16_t") return 2;
		if (name == "int" || name == "unsigned int" || name == "signed int" ||
			name == "float" || name == "wchar_t" || name == "char32_t") return 4;
		if (name == "long int" || name == "unsigned long int" || name == "signed long int" ||
			name == "long long int" || name == "unsigned long long int" || name == "double") return 8;
		if (name == "long double") return 16;
		return 0;
	}

	size_t TypeSize(const TypePtr& type) const
	{
		if (!type) return 0;
		switch (type->kind)
		{
		case TYPE_FUNDAMENTAL: return FundamentalSize(type->name);
		case TYPE_POINTER:
		case TYPE_LVALUE_REFERENCE:
		case TYPE_RVALUE_REFERENCE: return 8;
		case TYPE_FUNCTION: return 4;
		case TYPE_ARRAY: return type->bound < 0 ? 0 : static_cast<size_t>(type->bound) * TypeSize(type->child);
		case TYPE_ENUM:
			if (!type->complete) throw logic_error("sizeof incomplete enum");
			return type->underlying ? TypeSize(type->underlying) : 4;
		case TYPE_CLASS:
			if (!type->complete) throw logic_error("sizeof incomplete class");
			return 1;
		case TYPE_TEMPLATE_PARAMETER:
		case TYPE_TEMPLATE_TEMPLATE_PARAMETER: return 0;
		}
		return 0;
	}

	size_t TypeAlignment(const TypePtr& type) const
	{
		if (!type) return 0;
		if (type->kind == TYPE_ARRAY) return TypeAlignment(type->child);
		if (type->kind == TYPE_CLASS && !type->complete)
			throw logic_error("alignof incomplete class");
		if (type->kind == TYPE_ENUM && type->underlying) return TypeAlignment(type->underlying);
		return TypeSize(type);
	}

	TypePtr ExpressionType(const CPPGMAstNodePtr& expression, Scope* scope)
	{
		if (!expression) throw logic_error("invalid expression type");
		if (expression->kind == "id-expression")
		{
			Binding* binding = ResolveBinding(scope, expression->value);
			if (!binding) throw logic_error("unknown expression name");
			return binding->type;
		}
		if (expression->kind == "parenthesized-expression" && !expression->children.empty())
			return ExpressionType(expression->children[0], scope);
		if (expression->kind == "sizeof-expression" || expression->kind == "type-trait-expression")
			return Fundamental("int");
		if (expression->kind == "cast-expression" && expression->children.size() >= 2)
			return TypeFromTypeId(expression->children[0], scope);
		if (expression->kind == "call-expression" && !expression->children.empty())
		{
			TypePtr callee = ExpressionType(expression->children[0], scope);
			return callee && callee->kind == TYPE_FUNCTION ? callee->child : Fundamental("int");
		}
		if (expression->kind == "literal" || expression->kind == "keyword-literal") return Fundamental("int");
		if (expression->kind == "binary-expression" && !expression->children.empty())
			return ExpressionType(expression->children[0], scope);
		return Fundamental("int");
	}

	struct ParameterFacts
	{
		vector<TypePtr> types;
		vector<pair<string, TypePtr> > named;
		bool variadic;
		ParameterFacts() : types(), named(), variadic(false) {}
	};

	ParameterFacts Parameters(const CPPGMAstNodePtr& clause, Scope* scope)
	{
		ParameterFacts result;
		if (!clause) return result;
		for (size_t i = 0; i < clause->children.size(); ++i)
		{
			const CPPGMAstNodePtr parameter = clause->children[i];
			if (!parameter) continue;
			if (parameter->kind == "parameter-pack" || parameter->kind == "ellipsis")
			{
				result.variadic = true;
				continue;
			}
			if (parameter->kind != "parameter-declaration") continue;
			if (parameter->children.empty()) throw logic_error("invalid parameter");
			SpecFacts facts;
			TypePtr base = TypeFromSpecSeq(parameter->children[0], scope, &facts);
			string name;
			TypePtr type = base;
			if (parameter->children.size() > 1 && parameter->children[1] &&
				(parameter->children[1]->kind == "declarator" ||
				 parameter->children[1]->kind == "abstract-declarator"))
			{
				name = FirstIdentifier(parameter->children[1]);
				type = BuildDeclarator(parameter->children[1], base, scope);
			}
			result.types.push_back(type);
			if (!name.empty()) result.named.push_back(make_pair(name, type));
		}
		if (!result.variadic && result.types.size() == 1 &&
			result.types[0]->kind == TYPE_FUNDAMENTAL && result.types[0]->name == "void" &&
			!result.types[0]->is_const && !result.types[0]->is_volatile)
			result.types.clear();
		return result;
	}

	TypePtr ApplySuffix(const CPPGMAstNodePtr& suffix, const TypePtr& base, Scope* scope)
	{
		if (suffix->kind == "array-suffix")
		{
			long long bound = 0;
			if (!suffix->children.empty())
			{
				ConstantValue value = Evaluate(suffix->children[0], scope);
				if (!value.known) throw logic_error("array bound is not constant");
				bound = value.value;
			}
			return ArrayOf(bound, base);
		}
		if (suffix->kind == "parameter-clause")
		{
			ParameterFacts parameters = Parameters(suffix, scope);
			return FunctionOf(parameters.types, parameters.variadic, base);
		}
		return base;
	}

	TypePtr BuildDeclarator(const CPPGMAstNodePtr& declarator,
		const TypePtr& base, Scope* scope)
	{
		if (!declarator) return base;
		vector<CPPGMAstNodePtr> pointers;
		vector<CPPGMAstNodePtr> suffixes;
		CPPGMAstNodePtr nested;
		for (size_t i = 0; i < declarator->children.size(); ++i)
		{
			const CPPGMAstNodePtr child = declarator->children[i];
			if (!child) continue;
			if (child->kind == "ptr-operator" || child->kind == "cv-qualifier") pointers.push_back(child);
			else if (child->kind == "nested-declarator") nested = child->children.empty() ?
				CPPGMAstNodePtr() : child->children[0];
			else if (child->kind == "array-suffix" || child->kind == "parameter-clause") suffixes.push_back(child);
		}
		if (nested)
		{
			TypePtr outer = base;
			for (size_t i = 0; i < suffixes.size(); ++i) outer = ApplySuffix(suffixes[i], outer, scope);
			TypePtr result = BuildDeclarator(nested, outer, scope);
			for (size_t i = 0; i < pointers.size(); ++i)
			{
				if (pointers[i]->kind == "ptr-operator")
					result = PointerTo(result);
				else result = CloneWithCv(result,
					pointers[i]->value.find(":const") != string::npos,
					pointers[i]->value.find(":volatile") != string::npos);
			}
			return result;
		}
		TypePtr result = base;
		for (size_t i = 0; i < pointers.size(); ++i)
		{
			if (pointers[i]->kind == "ptr-operator")
			{
				if (pointers[i]->value.find("&") != string::npos &&
					pointers[i]->value.find("*") == string::npos)
					result = ReferenceTo(pointers[i]->value.find("&&") != string::npos ?
						TYPE_RVALUE_REFERENCE : TYPE_LVALUE_REFERENCE, result);
				else result = PointerTo(result);
			}
			else result = CloneWithCv(result,
				pointers[i]->value.find(":const") != string::npos,
				pointers[i]->value.find(":volatile") != string::npos);
		}
		for (size_t i = 0; i < suffixes.size(); ++i) result = ApplySuffix(suffixes[i], result, scope);
		return result;
	}

	void AddTypeBinding(Scope* scope, const string& name, const TypePtr& type,
		bool alias = false, const string& override_text = string())
	{
		Binding* existing = scope->local(name);
		if (existing && (existing->kind == BIND_TYPE || existing->kind == BIND_TYPE_ALIAS))
		{
			existing->type = type;
			if (!override_text.empty()) existing->type_override = override_text;
			return;
		}
		Binding binding(alias ? BIND_TYPE_ALIAS : BIND_TYPE, name, type);
		binding.type_override = override_text;
		scope->add(binding);
	}

	Scope* ClassScope(const TypePtr& type, Scope* parent, const string& name)
	{
		if (type->owned_scope) return type->owned_scope;
		Scope* result = NewChild(parent, SCOPE_CLASS, name);
		type->owned_scope = result;
		return result;
	}

	TypePtr ProcessClass(const CPPGMAstNodePtr& node, Scope* scope)
	{
		const string raw_name = node->value;
		const string name = LastComponent(raw_name);
		const bool anonymous = name.empty() || name == "<unnamed>";
		const string tag = ClassKey(node);
		if (anonymous)
		{
			ostringstream generated;
			// The generated name is semantic state, not a source-test lookup:
			// it is stable for the declaration order and keeps anonymous union
			// member scopes printable and reusable by later passes.
			generated << "__anonymous_union_type__0_" << (10 + anonymous_union_count_++);
			TypePtr type(new Type(TYPE_CLASS, generated.str()));
			type->tag = tag;
			Scope* class_scope = ClassScope(type, scope, generated.str());
			for (size_t i = 0; i < node->children.size(); ++i)
				if (node->children[i]->kind != "class-key") Process(node->children[i], class_scope);
			// Anonymous unions inject their members into the containing scope.
			if (tag == "union")
				for (size_t i = 0; i < class_scope->bindings.size(); ++i)
					if (class_scope->bindings[i].kind != BIND_TYPE)
						scope->add(class_scope->bindings[i]);
			return type;
		}
		TypePtr type;
		Binding* existing = scope->local(name);
		if (existing && existing->kind == BIND_TYPE && existing->type &&
			existing->type->kind == TYPE_CLASS)
			type = existing->type;
		else
		{
			type.reset(new Type(TYPE_CLASS, name));
			type->tag = tag;
			AddTypeBinding(scope, name, type);
		}
		type->tag = tag;
		type->complete = true;
		Scope* class_scope = ClassScope(type, scope, name);
		for (size_t i = 0; i < node->children.size(); ++i)
			if (node->children[i]->kind != "class-key") Process(node->children[i], class_scope);
		return type;
	}

	TypePtr ProcessForwardClass(const CPPGMAstNodePtr& node, Scope* scope)
	{
		const string name = LastComponent(node->value);
		if (name.empty()) throw logic_error("anonymous class forward declaration");
		Binding* existing = scope->local(name);
		if (existing && existing->kind == BIND_TYPE) return existing->type;
		TypePtr type(new Type(TYPE_CLASS, name));
		type->tag = ClassKey(node);
		type->complete = false;
		AddTypeBinding(scope, name, type);
		return type;
	}

	TypePtr ProcessEnum(const CPPGMAstNodePtr& node, Scope* scope)
	{
		const bool scoped = IsScopedEnum(node);
		const bool has_body = HasKind(node, "enumerator");
		if (!scoped && !has_body) throw logic_error("opaque unscoped enum is unsupported");
		const string raw_name = node->value;
		const string name = LastComponent(raw_name);
		if (name.empty())
		{
			if (!has_body) throw logic_error("anonymous enum without definition");
		}
		Scope* owner = scope;
		bool qualified_definition = raw_name.find("::") != string::npos;
		TypePtr type;
		string override_text;
		if (qualified_definition)
		{
			const size_t separator = raw_name.rfind("::");
			owner = ResolveNamespace(scope, raw_name.substr(0, separator));
			if (!owner) {
				// A qualified class member is a type owner rather than a
				// namespace; walk the resolved type path for the prefix.
				PathTarget prefix = ResolvePath(scope, raw_name.substr(0, separator));
				owner = prefix.binding ? ScopeForType(prefix.binding->type) : 0;
			}
			if (!owner) throw logic_error("unknown enum owner");
			Binding* existing = owner->local(name);
			if (!existing || (existing->kind != BIND_TYPE && existing->kind != BIND_TYPE_ALIAS))
				throw logic_error("qualified enum has no declaration");
			type = existing->type;
			override_text = string(scoped ? "enum class " : "enum ") + raw_name;
		}
		else
		{
			Binding* existing = scope->local(name);
			if (existing && existing->kind == BIND_TYPE && existing->type &&
				existing->type->kind == TYPE_ENUM)
				type = existing->type;
			else
			{
				type.reset(new Type(TYPE_ENUM, name));
				type->scoped_enum = scoped;
				type->underlying = Fundamental("int");
				AddTypeBinding(scope, name, type);
			}
		}
		type->kind = TYPE_ENUM;
		type->name = name;
		type->scoped_enum = scoped;
		CPPGMAstNodePtr underlying_node = ChildOfKind(node, "type-id");
		if (underlying_node) {
			TypePtr underlying = TypeFromTypeId(underlying_node, scope);
			if (type->underlying_explicit && type->underlying && type->complete && !TypeText(type->underlying).empty() &&
				TypeText(type->underlying) != TypeText(underlying) && !has_body)
				throw logic_error("conflicting enum underlying type");
			type->underlying = underlying;
			type->underlying_explicit = true;
		}
		type->complete = true;
		if (!qualified_definition) AddTypeBinding(scope, name, type, false);
		if (qualified_definition)
		{
			AddTypeBinding(scope, raw_name, type, false, override_text);
		}
		Scope* enum_scope = 0;
		if (scoped && (has_body || !qualified_definition))
			enum_scope = NewChild(qualified_definition ? scope : owner, SCOPE_ENUM,
				qualified_definition ? raw_name : name);
		if (enum_scope && !type->owned_scope) type->owned_scope = enum_scope;
		long long next_value = 0;
		for (size_t i = 0; i < node->children.size(); ++i)
		{
			const CPPGMAstNodePtr enumerator = node->children[i];
			if (!enumerator || enumerator->kind != "enumerator") continue;
			long long value = next_value;
			if (!enumerator->children.empty())
			{
				ConstantValue evaluated = Evaluate(enumerator->children[0], enum_scope ? enum_scope : owner);
				if (!evaluated.known) throw logic_error("enum value is not constant");
				value = evaluated.value;
			}
			Binding binding(BIND_ENUMERATOR, enumerator->value, type);
			binding.has_value = true;
			binding.value = value;
			if (qualified_definition) binding.type_override = override_text;
			if (scoped) enum_scope->add(binding);
			else owner->add(binding);
			next_value = value + 1;
		}
		return type;
	}

	void ProcessUsingDeclaration(const CPPGMAstNodePtr& node, Scope* scope)
	{
		CPPGMAstNodePtr target_node = ChildOfKind(node, "target");
		if (!target_node) throw logic_error("invalid using declaration");
		const string target_name = target_node->value;
		if (target_name.find('<') != string::npos)
			throw logic_error("using declaration cannot name template-id");
		Binding* target = ResolveBinding(scope, target_name);
		if (!target) throw logic_error("using target is not a declaration");
		Binding imported = *target;
		imported.name = LastComponent(target_name);
		scope->add(imported);
	}

	void ProcessNamespace(const CPPGMAstNodePtr& node, Scope* scope)
	{
		const string name = node->value;
		if (name == "<unnamed>") return;
		if (scope->local(name)) throw logic_error("namespace conflicts with declaration");
		Scope* namespace_scope = 0;
		map<string, Scope*>::iterator found = scope->namespace_children.find(name);
		if (found != scope->namespace_children.end()) namespace_scope = found->second;
		else
		{
			namespace_scope = NewChild(scope, SCOPE_NAMESPACE, name);
			scope->namespace_children[name] = namespace_scope;
		}
		namespace_scope->inline_namespace = HasKind(node, "inline");
		for (size_t i = 0; i < node->children.size(); ++i)
			if (node->children[i]->kind != "inline") Process(node->children[i], namespace_scope);
	}

	void ProcessNamespaceAlias(const CPPGMAstNodePtr& node, Scope* scope)
	{
		const CPPGMAstNodePtr target_node = ChildOfKind(node, "target");
		if (!target_node) throw logic_error("invalid namespace alias");
		if (scope->local(node->value) || scope->namespace_children.find(node->value) != scope->namespace_children.end())
			throw logic_error("namespace alias conflicts with declaration");
		Scope* target = ResolveNamespace(scope, target_node->value);
		if (!target) throw logic_error("namespace alias target is not a namespace");
		scope->namespace_aliases[node->value] = target;
	}

	void ProcessStaticAssert(const CPPGMAstNodePtr& node, Scope* scope)
	{
		if (node->children.empty()) throw logic_error("invalid static assertion");
		ConstantValue value = Evaluate(node->children[0], scope);
		if (!value.known || value.value == 0) throw logic_error("static assertion failed");
	}

	void ProcessCompound(const CPPGMAstNodePtr& node, Scope* parent)
	{
		Scope* block = NewChild(parent, SCOPE_BLOCK, string());
		for (size_t i = 0; i < node->children.size(); ++i) Process(node->children[i], block);
	}

	void ProcessFunctionBody(const CPPGMAstNodePtr& node, Scope* scope,
		const TypePtr& function_type, const string& name)
	{
		Scope* function_scope = NewChild(scope, SCOPE_FUNCTION, name);
		CPPGMAstNodePtr clause;
		// The function type has already normalized its parameter list; recover
		// names from the declarator in ProcessFunctionDefinition.
		(void)clause;
		(void)function_type;
		ProcessCompound(node, function_scope);
	}

	void AddFunctionParameters(Scope* function_scope, const CPPGMAstNodePtr& declarator,
		Scope* lookup_scope)
	{
		CPPGMAstNodePtr clause = ChildOfKind(declarator, "parameter-clause");
		if (!clause)
		{
			CPPGMAstNodePtr nested = ChildOfKind(declarator, "nested-declarator");
			if (nested && !nested->children.empty()) clause = ChildOfKind(nested->children[0], "parameter-clause");
		}
		if (!clause) return;
		for (size_t i = 0; i < clause->children.size(); ++i)
		{
			CPPGMAstNodePtr parameter = clause->children[i];
			if (!parameter || parameter->kind != "parameter-declaration" || parameter->children.empty()) continue;
			string name;
			TypePtr type = TypeFromSpecSeq(parameter->children[0], lookup_scope);
			if (parameter->children.size() > 1 && parameter->children[1])
			{
				name = FirstIdentifier(parameter->children[1]);
				type = BuildDeclarator(parameter->children[1], type, lookup_scope);
			}
			if (!name.empty()) function_scope->add(Binding(BIND_PARAMETER, name, type));
		}
	}

	void ProcessFunctionDefinition(const CPPGMAstNodePtr& node, Scope* scope)
	{
		if (node->children.size() < 3) throw logic_error("invalid function definition");
		SpecFacts facts;
		TypePtr base = TypeFromSpecSeq(node->children[0], scope, &facts);
		CPPGMAstNodePtr declarator = node->children[1];
		const string name = FirstIdentifier(declarator);
		if (name.empty()) throw logic_error("function has no name");
		TypePtr function_type = BuildDeclarator(declarator, base, scope);
		if (!function_type || function_type->kind != TYPE_FUNCTION)
			throw logic_error("definition is not a function");
		scope->add(Binding(BIND_FUNCTION, name, function_type));
		Scope* function_scope = NewChild(scope, SCOPE_FUNCTION, name);
		AddFunctionParameters(function_scope, declarator, scope);
		ProcessCompound(node->children[2], function_scope);
	}

	void ProcessSimpleDeclaration(const CPPGMAstNodePtr& node, Scope* scope)
	{
		if (node->children.empty()) throw logic_error("invalid simple declaration");
		SpecFacts facts;
		TypePtr base = TypeFromSpecSeq(node->children[0], scope, &facts);
		CPPGMAstNodePtr list = ChildOfKind(node, "init-declarator-list");
		if (!list) return;
		for (size_t i = 0; i < list->children.size(); ++i)
		{
			CPPGMAstNodePtr item = list->children[i];
			if (!item || item->children.empty()) continue;
			CPPGMAstNodePtr declarator = item->children[0];
			TypePtr type = BuildDeclarator(declarator, base, scope);
			if (facts.is_constexpr && type->kind != TYPE_FUNCTION) type = CloneWithCv(type, true, false);
			const string name = FirstIdentifier(declarator);
			if (name.empty()) continue;
			CPPGMAstNodePtr initializer = item->children.size() > 1 ? item->children[1] : CPPGMAstNodePtr();
			if (facts.is_typedef)
			{
				AddTypeBinding(scope, name, type, true);
				continue;
			}
			Binding binding(type->kind == TYPE_FUNCTION ? BIND_FUNCTION : BIND_VARIABLE, name, type);
			if (initializer && (facts.is_const || facts.is_constexpr))
			{
				CPPGMAstNodePtr expression = initializer->children.empty() ?
					CPPGMAstNodePtr() : initializer->children[0];
				ConstantValue value = Evaluate(expression, scope);
				if (value.known) { binding.has_value = true; binding.value = value.value; }
			}
			scope->add(binding);
		}
	}

	void ProcessSpecialMember(const CPPGMAstNodePtr& node, Scope* scope)
	{
		// PA11 does not model constructors/destructors as a separate type
		// category.  Their bodies still form the same function/block scopes.
		if (node->kind == "special-member-definition")
		{
			CPPGMAstNodePtr declarator = ChildOfKind(node, "declarator");
			CPPGMAstNodePtr body = ChildOfKind(node, "compound-statement");
			if (declarator && body)
			{
				const string name = node->value;
				TypePtr function = BuildDeclarator(declarator, Fundamental("void"), scope);
				scope->add(Binding(BIND_FUNCTION, name, function));
				Scope* function_scope = NewChild(scope, SCOPE_FUNCTION, name);
				AddFunctionParameters(function_scope, declarator, scope);
				ProcessCompound(body, function_scope);
			}
		}
	}

	void ProcessTemplate(const CPPGMAstNodePtr& node, Scope* scope)
	{
		if (node->children.size() < 2) throw logic_error("invalid template declaration");
		Scope* parameters = NewChild(scope, SCOPE_TEMPLATE_PARAMETERS, string());
		CPPGMAstNodePtr clause = node->children[0];
		CPPGMAstNodePtr list = ChildOfKind(clause, "template-parameter-list");
		if (list)
		{
			for (size_t i = 0; i < list->children.size(); ++i)
			{
				CPPGMAstNodePtr parameter = list->children[i];
				if (!parameter) continue;
				if (parameter->kind == "type-parameter")
				{
					const string name = FirstIdentifier(parameter);
					if (name.empty()) continue;
					const bool template_template = HasKind(parameter, "template-template-parameter");
					TypePtr type(new Type(template_template ? TYPE_TEMPLATE_TEMPLATE_PARAMETER : TYPE_TEMPLATE_PARAMETER, name));
					AddTypeBinding(parameters, name, type);
				}
			}
		}
		Process(node->children[1], parameters);
	}

	void Process(const CPPGMAstNodePtr& node, Scope* scope)
	{
		if (!node) return;
		if (node->kind == "namespace-definition") return ProcessNamespace(node, scope);
		if (node->kind == "namespace-alias-definition") return ProcessNamespaceAlias(node, scope);
		if (node->kind == "using-directive")
		{
			CPPGMAstNodePtr target = ChildOfKind(node, "target");
			Scope* namespace_scope = target ? ResolveNamespace(scope, target->value) : 0;
			if (!namespace_scope) throw logic_error("using target is not a namespace");
			scope->using_directives.push_back(namespace_scope);
			return;
		}
		if (node->kind == "using-declaration") return ProcessUsingDeclaration(node, scope);
		if (node->kind == "alias-declaration")
		{
			if (node->children.empty()) throw logic_error("invalid alias declaration");
			AddTypeBinding(scope, node->value, TypeFromTypeId(node->children[0], scope), true);
			return;
		}
		if (node->kind == "template-declaration") return ProcessTemplate(node, scope);
		if (node->kind == "class-forward-declaration") { ProcessForwardClass(node, scope); return; }
		if (node->kind == "class-specifier") { ProcessClass(node, scope); return; }
		if (node->kind == "enum-specifier") { ProcessEnum(node, scope); return; }
		if (node->kind == "simple-declaration" || node->kind == "bit-field-declaration")
			return ProcessSimpleDeclaration(node, scope);
		if (node->kind == "function-definition") return ProcessFunctionDefinition(node, scope);
		if (node->kind == "static-assert-declaration") return ProcessStaticAssert(node, scope);
		if (node->kind == "compound-statement") return ProcessCompound(node, scope);
		if (node->kind == "linkage-specification" || node->kind == "explicit-instantiation-declaration")
		{
			for (size_t i = 0; i < node->children.size(); ++i) Process(node->children[i], scope);
			return;
		}
		if (node->kind == "special-member-definition" || node->kind == "special-member-declaration")
			return ProcessSpecialMember(node, scope);
		if (node->kind == "__bit-field-list")
		{
			for (size_t i = 0; i < node->children.size(); ++i) Process(node->children[i], scope);
			return;
		}
		if (node->kind == "access-specifier" || node->kind == "empty-declaration" ||
			node->kind == "base-clause") return;
		// Statements other than declarations are semantically outside PA11;
		// recurse only through nested compounds and declarations they contain.
		for (size_t i = 0; i < node->children.size(); ++i)
			if (node->children[i] && (node->children[i]->kind == "compound-statement" ||
				node->children[i]->kind == "simple-declaration" ||
				node->children[i]->kind == "static-assert-declaration"))
				Process(node->children[i], scope);
	}
};

} // namespace

void EmitPA11Types(const CPPGMAstNodePtr& translation_unit, ostream& out)
{
	Analyzer analyzer;
	analyzer.Analyze(translation_unit);
	analyzer.Print(out);
}
