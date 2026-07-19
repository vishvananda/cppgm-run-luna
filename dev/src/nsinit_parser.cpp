#include "nsinit_parser.h"

#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>
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
#include "posttoken_unicode.h"
#include "preprocessor_engine.h"
#include "pptoken_translation.h"
#include "nsinit_image.h"
#include "nsinit_literals.h"

using namespace std;

#include "nsinit_model.h"

namespace {

void AppendLE(vector<unsigned char>* bytes, unsigned long long value, size_t width)
{
	for (size_t i = 0; i < width; ++i)
	{
		bytes->push_back(static_cast<unsigned char>(value & 0xff));
		value >>= 8;
	}
}

class Parser
{
public:
	Parser(const vector<PostPPToken>& source, Program* program, int unit_id);
	void ParseTranslationUnit();

private:
	enum DeclaratorMode { NAMED_REQUIRED, NAMED_OPTIONAL, ABSTRACT_ONLY };

	vector<PostPPToken> tokens_;
	size_t position_;
	Program* program_;
	int unit_id_;
	Namespace* current_;

	const PostPPToken& Peek(size_t offset = 0) const;
	bool AtEnd() const;
	bool Is(const string& text) const;
	bool Take(const string& text);
	void Expect(const string& text);
	string TakeName();
	NamePath ParseNamePath();
	bool IsNamePathStart() const;

	Namespace* DeclarationNamespace(const NamePath& path);
	Namespace* ExpressionNamespace(const NamePath& path);
	bool IsInternal(const DeclSpec& spec, Namespace* scope) const;

	Type MakeFundamental(const vector<string>& words) const;
	DeclSpec ParseDeclSpecifierSeq();
	Type ParseTypeId();
	long long ParseArrayBound(Namespace* context);
	DeclaratorShape ParseDeclarator(DeclaratorMode mode);
	bool LooksLikeAbstractFunctionSuffix() const;
	void ParseParameterClause(vector<Type>* parameters, bool* variadic);
	Type AdjustParameter(const Type& type) const;
	Type ApplyDeclarator(const Type& base, const DeclaratorShape& shape) const;
	void ValidateType(const Type& type, bool object_type) const;

	ExprValue ParseExpression(Namespace* context);
	ExprValue ParseIdentifierExpression(const NamePath& path, Namespace* context);
	ExprValue ParseLiteralExpression(const PostPPToken& token);
	ExprValue ParseCharacterExpression(const PostPPToken& token);
	ExprValue ParseNumberExpression(const PostPPToken& token);
	ExprValue ParseStringExpression(const PostPPToken& token);
	ExprValue MakeEntityExpression(Entity* entity) const;
	ExprValue DereferenceReferenceExpression(Entity* entity) const;

	void ParseDeclaration();
	void ParseNamespaceDefinition(bool inline_namespace);
	void ParseNamespaceAliasDefinition();
	void ParseUsingDirective();
	void ParseUsingDeclaration();
	void ParseAliasDeclaration();
	void ParseStaticAssert();
	void ParseSimpleDeclaration();
	void ParseFunctionBody();

	void ProcessFunction(const DeclSpec& spec, const DeclaratorShape& shape,
		const Type& type, bool definition);
	void ProcessVariable(const DeclSpec& spec, const DeclaratorShape& shape,
		Type type, const ExprValue* initializer);
	void ProcessTypedef(const DeclSpec& spec, const DeclaratorShape& shape,
		const Type& type);

	void ApplyInitializer(Entity* entity, const Type& type, const ExprValue& value,
		Namespace* context);
	void ApplyReferenceInitializer(Entity* entity, const Type& type,
		const ExprValue& value);
	void ApplyPointerInitializer(Entity* entity, const Type& type,
		const ExprValue& value);
	void ApplyScalarInitializer(Entity* entity, const Type& type,
		const ExprValue& value);
	void ApplyArrayInitializer(Entity* entity, const Type& type,
		const ExprValue& value);

	bool IsConstObject(const Entity* entity) const;
	bool CompatibleReference(const Type& referred, const Type& source,
		bool source_lvalue) const;
	Address AddressOf(const ExprValue& value) const;
	Address AddressOfEntity(Entity* entity) const;
	vector<unsigned char> EncodeScalar(const Type& type,
		const ExprValue& value) const;
	void StoreIntegerConstant(Entity* entity, const Type& type,
		const ExprValue& value, const vector<unsigned char>& bytes);
	void StorePointerConstant(Entity* entity, const Address& address);
	void CheckStaticAssert(const ExprValue& value) const;
};

Parser::Parser(const vector<PostPPToken>& source, Program* program, int unit_id)
	: tokens_(), position_(0), program_(program), unit_id_(unit_id),
	  current_(program->root())
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

const PostPPToken& Parser::Peek(size_t offset) const
{
	static const PostPPToken eof(POST_PP_EOF);
	const size_t index = position_ + offset;
	return index < tokens_.size() ? tokens_[index] : eof;
}

bool Parser::AtEnd() const
{
	return position_ >= tokens_.size();
}

bool Parser::Is(const string& text) const
{
	return Peek().source == text;
}

bool Parser::Take(const string& text)
{
	if (!Is(text)) return false;
	++position_;
	return true;
}

void Parser::Expect(const string& text)
{
	if (!Take(text)) throw logic_error("nsinit: expected '" + text + "'");
}

string Parser::TakeName()
{
	if (!IsNameToken(Peek())) throw logic_error("nsinit: expected identifier");
	return tokens_[position_++].source;
}

NamePath Parser::ParseNamePath()
{
	NamePath result;
	result.absolute = Take("::");
	result.parts.push_back(TakeName());
	while (Take("::")) result.parts.push_back(TakeName());
	return result;
}

bool Parser::IsNamePathStart() const
{
	return IsNameToken(Peek()) || Is("::");
}

Namespace* Parser::DeclarationNamespace(const NamePath& path)
{
	if (path.parts.size() == 1 && !path.absolute) return current_;
	NamePath namespace_path = path;
	namespace_path.parts.pop_back();
	Namespace* target = namespace_path.parts.empty() ? program_->root() :
		program_->ResolveNamespacePath(namespace_path, current_, unit_id_);
	if (target == NULL) throw logic_error("unknown declaration namespace");
	if (!current_->global && !program_->IsEnclosing(target, current_) &&
		!program_->IsEnclosing(current_, target))
		throw logic_error("qualified declaration names a non-enclosing namespace");
	return target;
}

Namespace* Parser::ExpressionNamespace(const NamePath& path)
{
	if (path.parts.size() == 1 && !path.absolute) return current_;
	NamePath namespace_path = path;
	namespace_path.parts.pop_back();
	if (namespace_path.parts.empty()) return program_->root();
	Namespace* target = program_->ResolveNamespacePath(namespace_path, current_,
		unit_id_);
	if (target == NULL) throw logic_error("unknown namespace");
	return target;
}

bool IsConstQualified(const Type& type)
{
	if (type.is_const) return true;
	return type.kind == Type::ARRAY && type.child != NULL &&
		IsConstQualified(*type.child);
}

bool Parser::IsInternal(const DeclSpec& spec, Namespace* scope) const
{
	if (spec.is_extern) return false;
	if (spec.is_static || spec.is_constexpr) return true;
	if (scope->unnamed && !scope->global) return true;
	return IsConstQualified(spec.type);
}

Type Parser::MakeFundamental(const vector<string>& words) const
{
	bool unsig = false;
	bool signed_word = false;
	bool short_word = false;
	int long_count = 0;
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
		if (word == "unsigned") unsig = true;
		else if (word == "signed") signed_word = true;
		else if (word == "short") short_word = true;
		else if (word == "long") ++long_count;
		else if (word == "char") has_char = true;
		else if (word == "char16_t") has_char16 = true;
		else if (word == "char32_t") has_char32 = true;
		else if (word == "wchar_t") has_wchar = true;
		else if (word == "bool") has_bool = true;
		else if (word == "float") has_float = true;
		else if (word == "double") has_double = true;
		else if (word == "void") has_void = true;
	}
	if (has_char) return Type::Fundamental(signed_word ? "signed char" :
		unsig ? "unsigned char" : "char");
	if (has_char16) return Type::Fundamental("char16_t");
	if (has_char32) return Type::Fundamental("char32_t");
	if (has_wchar) return Type::Fundamental("wchar_t");
	if (has_bool) return Type::Fundamental("bool");
	if (has_float) return Type::Fundamental("float");
	if (has_double) return Type::Fundamental(long_count == 1 ? "long double" : "double");
	if (has_void) return Type::Fundamental("void");
	if (short_word) return Type::Fundamental(unsig ? "unsigned short int" : "short int");
	if (long_count >= 2) return Type::Fundamental(unsig ? "unsigned long long int" : "long long int");
	if (long_count == 1) return Type::Fundamental(unsig ? "unsigned long int" : "long int");
	if (unsig) return Type::Fundamental("unsigned int");
	return Type::Fundamental("int");
}

DeclSpec Parser::ParseDeclSpecifierSeq()
{
	DeclSpec result;
	vector<string> fundamentals;
	bool have_alias = false;
	bool constant = false;
	bool vol = false;
	while (true)
	{
		const string word = Peek().source;
		if (word == "static") { result.is_static = true; ++position_; continue; }
		if (word == "thread_local") { result.is_thread_local = true; ++position_; continue; }
		if (word == "extern") { result.is_extern = true; ++position_; continue; }
		if (word == "typedef") { result.is_typedef = true; ++position_; continue; }
		if (word == "constexpr") { result.is_constexpr = true; ++position_; continue; }
		if (word == "inline") { result.is_inline = true; ++position_; continue; }
		if (word == "const") { constant = true; ++position_; continue; }
		if (word == "volatile") { vol = true; ++position_; continue; }
		if (IsFundamentalWord(word)) { fundamentals.push_back(word); ++position_; continue; }
		if (fundamentals.empty() && !have_alias && IsNamePathStart())
		{
			const NamePath path = ParseNamePath();
			Entity* alias = program_->ResolveTypePath(path, current_, unit_id_);
			if (alias == NULL || alias->kind != TYPEDEF_ENTITY)
				throw logic_error("unknown typedef name");
			result.type = alias->type;
			have_alias = true;
			continue;
		}
		break;
	}
	if (!fundamentals.empty() && have_alias) throw logic_error("mixed type specifiers");
	if (fundamentals.empty() && !have_alias) throw logic_error("missing type specifier");
	if (!have_alias) result.type = MakeFundamental(fundamentals);
	result.type = AddCv(result.type, constant, vol);
	return result;
}

Type Parser::ParseTypeId()
{
	const DeclSpec spec = ParseDeclSpecifierSeq();
	DeclaratorShape shape;
	if (IsPointerOperator(Peek().source) || Is("[") || Is("("))
		shape = ParseDeclarator(ABSTRACT_ONLY);
	const Type type = ApplyDeclarator(spec.type, shape);
	ValidateType(type, false);
	return type;
}

long long Parser::ParseArrayBound(Namespace* context)
{
	if (Is("]")) return -1;
	const ExprValue value = ParseExpression(context);
	if (!value.constant || !value.integral || value.integer == 0 ||
		value.integer > static_cast<unsigned long long>(numeric_limits<long long>::max()))
		throw logic_error("array bound is not a positive constant expression");
	return static_cast<long long>(value.integer);
}

bool Parser::LooksLikeAbstractFunctionSuffix() const
{
	if (!Is("(")) return false;
	const string next = Peek(1).source;
	if (next == ")" || next == "..." || IsFundamentalWord(next) ||
		IsCvWord(next) || next == "typedef" || next == "static" ||
		next == "extern" || next == "thread_local" || next == "::")
		return true;
	if (!IsNameToken(Peek(1))) return false;
	if (Peek(2).source == "::") return true;
	return program_->LookupType(next, current_, unit_id_) != NULL;
}

void Parser::ParseParameterClause(vector<Type>* parameters, bool* variadic)
{
	if (Is(")")) return;
	if (Take("...")) { *variadic = true; return; }
	while (true)
	{
		const DeclSpec spec = ParseDeclSpecifierSeq();
		DeclaratorShape shape;
		if (IsPointerOperator(Peek().source) || IsNamePathStart() ||
			Is("[") || Is("("))
		{
			const DeclaratorMode mode = Is("(") && LooksLikeAbstractFunctionSuffix() ?
				ABSTRACT_ONLY : NAMED_OPTIONAL;
			shape = ParseDeclarator(mode);
		}
		Type parameter = ApplyDeclarator(spec.type, shape);
		if (IsFundamental(parameter, "void") && !shape.has_name &&
			shape.operations.empty() && parameters->empty() && !*variadic)
		{
			// A lone void is the spelling of an empty parameter list.
		}
		else
		{
			ValidateType(parameter, false);
			parameters->push_back(AdjustParameter(parameter));
		}
		if (Take(","))
		{
			if (Take("...")) { *variadic = true; return; }
			continue;
		}
		if (Take("...")) *variadic = true;
		return;
	}
}

Type Parser::AdjustParameter(const Type& type) const
{
	Type result = type;
	if (result.kind == Type::FUNCTION) result = Type::Pointer(result);
	else if (result.kind == Type::ARRAY) result = Type::Pointer(*result.child);
	if (result.kind != Type::LVALUE_REFERENCE &&
		result.kind != Type::RVALUE_REFERENCE)
		result = RemoveTopCv(result);
	return result;
}

DeclaratorShape Parser::ParseDeclarator(DeclaratorMode mode)
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
	DeclaratorShape inner;
	bool grouped = false;
	bool have_root = false;
	if (mode != ABSTRACT_ONLY && IsNamePathStart())
	{
		direct.has_name = true;
		direct.name = ParseNamePath();
		have_root = true;
	}
	else if (Is("(") && !(mode == ABSTRACT_ONLY && LooksLikeAbstractFunctionSuffix()))
	{
		Take("(");
		const DeclaratorMode inner_mode = mode == NAMED_REQUIRED ? NAMED_OPTIONAL : mode;
		inner = ParseDeclarator(inner_mode);
		Expect(")");
		grouped = true;
		have_root = inner.has_name || !inner.operations.empty();
	}
	else if (mode == NAMED_REQUIRED)
		throw logic_error("expected declarator-id");
	if (!have_root && mode == NAMED_REQUIRED)
		throw logic_error("expected declarator-id");

	Namespace* expression_context = current_;
	if (direct.has_name) expression_context = ExpressionNamespace(direct.name);
	else if (inner.has_name) expression_context = ExpressionNamespace(inner.name);
	vector<DeclaratorOp> suffixes;
	while (true)
	{
		if (Take("["))
		{
			DeclaratorOp array(DeclaratorOp::ARRAY);
			array.bound = ParseArrayBound(expression_context);
			Expect("]");
			suffixes.push_back(array);
			continue;
		}
		if (Take("("))
		{
			DeclaratorOp function(DeclaratorOp::FUNCTION);
			ParseParameterClause(&function.parameters, &function.variadic);
			Expect(")");
			suffixes.push_back(function);
			continue;
		}
		break;
	}

	DeclaratorShape result;
	result.has_name = direct.has_name ? true : inner.has_name;
	result.name = direct.has_name ? direct.name : inner.name;
	result.operations = prefix;
	if (grouped)
		result.operations.insert(result.operations.end(), suffixes.begin(), suffixes.end());
	else
		result.operations.insert(result.operations.end(), suffixes.begin(), suffixes.end());
	if (grouped)
		result.operations.insert(result.operations.end(), inner.operations.begin(),
			inner.operations.end());
	return result;
}

Type Parser::ApplyDeclarator(const Type& base, const DeclaratorShape& shape) const
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
		case DeclaratorOp::RVALUE_REFERENCE:
			if (result.kind == Type::LVALUE_REFERENCE ||
				result.kind == Type::RVALUE_REFERENCE)
				throw logic_error("invalid reference declarator");
			result = CollapseReference(op.kind == DeclaratorOp::LVALUE_REFERENCE ?
				Type::LVALUE_REFERENCE : Type::RVALUE_REFERENCE, result);
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

void Parser::ValidateType(const Type& type, bool object_type) const
{
	if (type.kind == Type::FUNDAMENTAL)
	{
		if (object_type && type.fundamental == "void")
			throw logic_error("object of void type");
		return;
	}
	if (type.kind == Type::POINTER)
	{
		if (type.child->kind == Type::LVALUE_REFERENCE ||
			type.child->kind == Type::RVALUE_REFERENCE)
			throw logic_error("invalid reference declarator");
		ValidateType(*type.child, false);
		return;
	}
	if (type.kind == Type::LVALUE_REFERENCE || type.kind == Type::RVALUE_REFERENCE)
	{
		if (type.child->kind == Type::FUNDAMENTAL && type.child->fundamental == "void")
			throw logic_error("invalid reference declarator");
		if (type.child->kind == Type::LVALUE_REFERENCE ||
			type.child->kind == Type::RVALUE_REFERENCE)
			throw logic_error("invalid reference declarator");
		ValidateType(*type.child, false);
		return;
	}
	if (type.kind == Type::ARRAY)
	{
		if (type.child->kind == Type::FUNCTION ||
			type.child->kind == Type::LVALUE_REFERENCE ||
			type.child->kind == Type::RVALUE_REFERENCE ||
			(type.child->kind == Type::FUNDAMENTAL &&
			 type.child->fundamental == "void"))
			throw logic_error("invalid array element type");
		ValidateType(*type.child, false);
		return;
	}
	if (type.kind == Type::FUNCTION)
	{
		if (type.child->kind == Type::ARRAY || type.child->kind == Type::FUNCTION)
			throw logic_error("invalid function return type");
		for (size_t i = 0; i < type.parameters.size(); ++i)
			ValidateType(type.parameters[i], false);
		ValidateType(*type.child, false);
	}
}

ExprValue Parser::ParseExpression(Namespace* context)
{
	if (Take("("))
	{
		ExprValue value = ParseExpression(context);
		Expect(")");
		return value;
	}
	if (Is("true") || Is("false"))
	{
		ExprValue value;
		value.type = Type::Fundamental("bool");
		value.constant = true;
		value.integral = true;
		value.integer = Is("true") ? 1 : 0;
		value.category = VALUE_PRVALUE;
		++position_;
		return value;
	}
	if (Is("nullptr"))
	{
		ExprValue value;
		value.type = Type::Fundamental("nullptr_t");
		value.constant = true;
		value.null_pointer = true;
		++position_;
		return value;
	}
	if (Peek().kind == POST_PP_NUMBER || Peek().kind == POST_PP_CHARACTER ||
		Peek().kind == POST_PP_USER_CHARACTER || Peek().kind == POST_PP_STRING ||
		Peek().kind == POST_PP_USER_STRING)
	{
		const PostPPToken token = Peek();
		++position_;
		return ParseLiteralExpression(token);
	}
	if (IsNamePathStart())
	{
		const NamePath path = ParseNamePath();
		return ParseIdentifierExpression(path, context);
	}
	throw logic_error("expected expression");
}

ExprValue Parser::ParseIdentifierExpression(const NamePath& path, Namespace* context)
{
	Entity* entity = NULL;
	if (path.parts.size() == 1 && !path.absolute)
		entity = program_->LookupEntity(path.parts[0], context, unit_id_);
	else entity = program_->ResolveEntityPath(path, context, unit_id_);
	if (entity == NULL || entity->kind == TYPEDEF_ENTITY)
		throw logic_error("unknown expression name");
	return MakeEntityExpression(entity);
}

ExprValue Parser::ParseLiteralExpression(const PostPPToken& token)
{
	if (token.kind == POST_PP_NUMBER) return ParseNumberExpression(token);
	if (token.kind == POST_PP_CHARACTER || token.kind == POST_PP_USER_CHARACTER)
		return ParseCharacterExpression(token);
	if (token.kind == POST_PP_STRING || token.kind == POST_PP_USER_STRING)
		return ParseStringExpression(token);
	throw logic_error("invalid literal expression");
}

ExprValue Parser::ParseCharacterExpression(const PostPPToken& token)
{
	const QuotedData quoted = ParseQuoted(token.source, true);
	if (!quoted.suffix.empty() || quoted.values.size() != 1)
		throw logic_error("invalid character literal");
	const int value = quoted.values[0];
	ExprValue result;
	result.category = VALUE_PRVALUE;
	result.constant = true;
	result.integral = true;
	result.integer = static_cast<unsigned long long>(value);
	if (quoted.encoding == LIT_UTF16)
	{
		if (value > 0xffff) throw logic_error("character does not fit char16_t");
		result.type = Type::Fundamental("char16_t");
	}
	else if (quoted.encoding == LIT_UTF32) result.type = Type::Fundamental("char32_t");
	else if (quoted.encoding == LIT_WCHAR) result.type = Type::Fundamental("wchar_t");
	else if (value <= 127) result.type = Type::Fundamental("char");
	else result.type = Type::Fundamental("int");
	return result;
}

ExprValue Parser::ParseNumberExpression(const PostPPToken& token)
{
	ExprValue result;
	result.category = VALUE_PRVALUE;
	result.constant = true;
	if (LooksFloating(token.source))
	{
		string suffix;
		string core = token.source;
		if (!core.empty() && (core[core.size() - 1] == 'f' ||
			core[core.size() - 1] == 'F' || core[core.size() - 1] == 'l' ||
			core[core.size() - 1] == 'L'))
		{
			suffix = core.substr(core.size() - 1);
			core.erase(core.size() - 1);
		}
		char* end = NULL;
		errno = 0;
		result.real = strtold(core.c_str(), &end);
		if (errno == ERANGE || end == core.c_str() || *end != '\0')
			throw logic_error("invalid floating literal");
		result.floating = true;
		result.type = Type::Fundamental(suffix.empty() ? "double" :
			(suffix == "f" || suffix == "F" ? "float" : "long double"));
		return result;
	}
	string suffix;
	int base = 10;
	const string core = IntegerCore(token.source, &suffix, &base);
	char* end = NULL;
	errno = 0;
	const unsigned long long value = strtoull(core.c_str(), &end, base);
	if (core.empty() || errno == ERANGE || end == core.c_str() || *end != '\0')
		throw logic_error("invalid integer literal");
	result.integral = true;
	result.integer = value;
	result.type = IntegerLiteralType(value, suffix, base);
	return result;
}

ExprValue Parser::ParseStringExpression(const PostPPToken& token)
{
	const QuotedData quoted = ParseQuoted(token.source, false);
	if (!quoted.suffix.empty()) throw logic_error("user-defined string literal");
	const EncodedString encoded = EncodeString(quoted);
	Type literal_type = encoded.type;
	literal_type = AddCv(literal_type, true, false);
	StringLiteral* string = program_->AddString(literal_type, encoded.bytes);
	ExprValue result;
	result.type = literal_type;
	result.category = VALUE_LVALUE;
	result.constant = true;
	result.has_address = true;
	result.address.string = string;
	result.string = string;
	return result;
}

ExprValue Parser::MakeEntityExpression(Entity* entity) const
{
	if (entity->kind == TYPEDEF_ENTITY) throw logic_error("typedef is not an expression");
	if (entity->kind == FUNCTION_ENTITY)
	{
		ExprValue result;
		result.type = entity->type;
		result.category = VALUE_LVALUE;
		result.constant = true;
		result.has_address = true;
		result.address = AddressOfEntity(entity);
		result.object = entity;
		return result;
	}
	if (entity->type.kind == Type::LVALUE_REFERENCE ||
		entity->type.kind == Type::RVALUE_REFERENCE)
		return DereferenceReferenceExpression(entity);
	ExprValue result;
	result.type = entity->type;
	result.category = VALUE_LVALUE;
	result.has_address = true;
	result.address = AddressOfEntity(entity);
	result.object = entity;
	if (entity->value.known)
	{
		result.constant = entity->value.usable;
		result.integral = entity->value.integral;
		result.floating = entity->value.floating;
		result.integer = entity->value.integer;
		result.real = entity->value.real;
	}
	if (entity->value.pointer)
	{
		result.has_pointer_value = true;
		result.pointer_value = entity->value.address;
	}
	return result;
}

ExprValue Parser::DereferenceReferenceExpression(Entity* entity) const
{
	if (!entity->has_reference_address)
		throw logic_error("reference has no initializer");
	const Type referred = *entity->type.child;
	ExprValue result;
	result.type = referred;
	result.category = VALUE_LVALUE;
	result.has_address = true;
	result.address = entity->reference_address;
	if (entity->reference_address.entity != NULL)
	{
		Entity* target = entity->reference_address.entity;
		result.object = target;
		if (target->value.known)
		{
			result.constant = target->value.usable;
			result.integral = target->value.integral;
			result.floating = target->value.floating;
			result.integer = target->value.integer;
			result.real = target->value.real;
		}
		if (target->value.pointer)
		{
			result.has_pointer_value = true;
			result.pointer_value = target->value.address;
		}
	}
	else if (entity->reference_address.temporary != NULL)
	{
		Temporary* temporary = entity->reference_address.temporary;
		if (temporary->initializer.kind == INIT_BYTES)
		{
			result.constant = false;
			result.integral = IsIntegral(referred);
		}
	}
	return result;
}

bool Parser::IsConstObject(const Entity* entity) const
{
	return entity->is_constexpr || IsConstQualified(entity->type);
}

Address Parser::AddressOfEntity(Entity* entity) const
{
	if (entity->type.kind == Type::LVALUE_REFERENCE ||
		entity->type.kind == Type::RVALUE_REFERENCE)
		return entity->reference_address;
	Address result;
	result.entity = entity;
	return result;
}

Address Parser::AddressOf(const ExprValue& value) const
{
	if (value.has_address) return value.address;
	if (value.has_pointer_value) return value.pointer_value;
	if (value.object) return AddressOfEntity(value.object);
	return Address();
}

bool Parser::CompatibleReference(const Type& referred, const Type& source,
	bool source_lvalue) const
{
	if (referred.kind != source.kind) return false;
	if (referred.kind == Type::FUNDAMENTAL)
	{
		if (referred.fundamental != source.fundamental) return false;
		if (source_lvalue && source.is_const && !referred.is_const) return false;
		return true;
	}
	if (referred.kind == Type::ARRAY)
		return referred.bound == source.bound &&
			CompatibleReference(*referred.child, *source.child, source_lvalue);
	if (referred.kind == Type::POINTER)
		return SameIgnoringTopCv(referred, source);
	return SameType(referred, source);
}

vector<unsigned char> Parser::EncodeScalar(const Type& type,
	const ExprValue& value) const
{
	if (type.kind != Type::FUNDAMENTAL) throw logic_error("scalar type required");
	const size_t width = FundamentalSize(type.fundamental);
	vector<unsigned char> bytes;
	if (value.floating)
	{
		if (type.fundamental == "float")
		{
			const float converted = static_cast<float>(value.real);
			bytes.resize(4);
			memcpy(bytes.data(), &converted, 4);
		}
		else if (type.fundamental == "double")
		{
			const double converted = static_cast<double>(value.real);
			bytes.resize(8);
			memcpy(bytes.data(), &converted, 8);
		}
		else
		{
			const long double converted = value.real;
			bytes.resize(16);
			memcpy(bytes.data(), &converted, 16);
		}
		return bytes;
	}
	if (!value.integral) throw logic_error("invalid scalar initializer");
	unsigned long long integer = value.integer;
	if (type.fundamental == "bool") integer = integer ? 1 : 0;
	bytes.reserve(width);
	AppendLE(&bytes, integer, width);
	return bytes;
}

void Parser::StoreIntegerConstant(Entity* entity, const Type& type,
	const ExprValue& value, const vector<unsigned char>& bytes)
{
	entity->value.known = true;
	entity->value.usable = value.constant && IsConstObject(entity);
	entity->value.integral = IsIntegral(type);
	entity->value.floating = IsFloating(type);
	entity->value.integer = value.integer;
	entity->value.real = value.real;
	entity->value.bytes = bytes;
	entity->initializer.kind = INIT_BYTES;
	entity->initializer.bytes = bytes;
}

void Parser::StorePointerConstant(Entity* entity, const Address& address)
{
	entity->value.known = true;
	entity->value.pointer = true;
	entity->value.usable = entity->is_constexpr;
	entity->value.address = address;
	entity->initializer.kind = INIT_ADDRESS;
	entity->initializer.address = address;
}

void Parser::ApplyScalarInitializer(Entity* entity, const Type& type,
	const ExprValue& value)
{
	if (value.null_pointer || (!value.integral && !value.floating))
		throw logic_error("invalid scalar initializer");
	if (!value.constant && value.object != NULL && !value.integral && !value.floating)
		throw logic_error("invalid scalar initializer");
	if (!value.constant)
	{
		entity->initializer = InitData();
		return;
	}
	const vector<unsigned char> bytes = EncodeScalar(type, value);
	StoreIntegerConstant(entity, type, value, bytes);
}

bool PointerPointeeCompatible(const Type& destination, const Type& source)
{
	if (destination.kind != Type::POINTER || source.kind != Type::POINTER)
		return false;
	const Type& left = *destination.child;
	const Type& right = *source.child;
	if (left.kind != right.kind) return false;
	if (left.kind == Type::FUNDAMENTAL)
		return left.fundamental == right.fundamental &&
			(!right.is_const || left.is_const || !left.is_volatile || right.is_volatile);
	return SameIgnoringTopCv(left, right);
}

void Parser::ApplyPointerInitializer(Entity* entity, const Type& type,
	const ExprValue& value)
{
	Address address;
	if (value.null_pointer || (value.integral && value.constant && value.integer == 0))
	{
		address = Address();
	}
	else if (value.type.kind == Type::FUNCTION)
	{
		if (!value.has_address || !SameIgnoringTopCv(*type.child, value.type))
			throw logic_error("invalid function pointer initializer");
		address = AddressOf(value);
	}
	else if (value.type.kind == Type::ARRAY)
	{
		if (!value.has_address) throw logic_error("invalid array pointer initializer");
		Type source_pointer = Type::Pointer(*value.type.child);
		if (!PointerPointeeCompatible(type, source_pointer))
			throw logic_error("incompatible pointer initializer");
		address = AddressOf(value);
	}
	else if (value.type.kind == Type::POINTER)
	{
		if (!PointerPointeeCompatible(type, value.type))
			throw logic_error("incompatible pointer initializer");
		if (!value.has_pointer_value) throw logic_error("unknown pointer initializer");
		address = value.pointer_value;
	}
	else throw logic_error("invalid pointer initializer");
	StorePointerConstant(entity, address);
}

void Parser::ApplyArrayInitializer(Entity* entity, const Type& type,
	const ExprValue& value)
{
	if (value.string == NULL) throw logic_error("invalid array initializer");
	const Type& source_element = *value.string->type.child;
	const Type& destination_element = *type.child;
	if (source_element.kind != Type::FUNDAMENTAL ||
		destination_element.kind != Type::FUNDAMENTAL)
		throw logic_error("invalid string array element type");
	const bool source_byte = source_element.fundamental == "char";
	const bool destination_byte = destination_element.fundamental == "char" ||
		destination_element.fundamental == "signed char" ||
		destination_element.fundamental == "unsigned char";
	if (source_byte != destination_byte &&
		(source_element.fundamental != destination_element.fundamental))
		throw logic_error("incompatible string array initializer");
	if (FundamentalSize(source_element.fundamental) !=
		FundamentalSize(destination_element.fundamental))
		throw logic_error("incompatible string array initializer");
	const size_t total = TypeSize(type);
	if (value.string->bytes.size() > total)
		throw logic_error("string initializer is too long");
	vector<unsigned char> bytes(total, 0);
	copy(value.string->bytes.begin(), value.string->bytes.end(), bytes.begin());
	entity->initializer.kind = INIT_BYTES;
	entity->initializer.bytes = bytes;
}

void Parser::ApplyReferenceInitializer(Entity* entity, const Type& type,
	const ExprValue& value)
{
	const Type& referred = *type.child;
	if (value.category == VALUE_LVALUE)
	{
		if (type.kind == Type::RVALUE_REFERENCE)
			throw logic_error("rvalue reference cannot bind lvalue");
		if (!value.has_address || !CompatibleReference(referred, value.type, true))
			throw logic_error("incompatible reference initializer");
		entity->reference_address = AddressOf(value);
		entity->has_reference_address = true;
		entity->initializer.kind = INIT_ADDRESS;
		entity->initializer.address = entity->reference_address;
		return;
	}
	if (type.kind == Type::LVALUE_REFERENCE && !referred.is_const)
		throw logic_error("non-const reference requires lvalue");
	if (referred.kind != value.type.kind &&
		!(IsIntegral(referred) && IsIntegral(value.type)) &&
		!(IsFloating(referred) && IsFloating(value.type)))
		throw logic_error("incompatible reference initializer");
	Temporary* temporary = program_->AddTemporary(referred);
	if (referred.kind == Type::FUNDAMENTAL)
	{
		if (!value.constant) temporary->initializer.kind = INIT_ZERO;
		else
		{
			temporary->initializer.kind = INIT_BYTES;
			temporary->initializer.bytes = EncodeScalar(referred, value);
		}
	}
	else if (referred.kind == Type::POINTER)
	{
		temporary->initializer.kind = INIT_ADDRESS;
		temporary->initializer.address = AddressOf(value);
	}
	else throw logic_error("unsupported reference temporary");
	entity->reference_address.temporary = temporary;
	entity->has_reference_address = true;
	entity->initializer.kind = INIT_ADDRESS;
	entity->initializer.address = entity->reference_address;
}

void Parser::ApplyInitializer(Entity* entity, const Type& type,
	const ExprValue& value, Namespace* context)
{
	(void)context;
	if (type.kind == Type::LVALUE_REFERENCE || type.kind == Type::RVALUE_REFERENCE)
	{
		ApplyReferenceInitializer(entity, type, value);
		return;
	}
	if (type.kind == Type::POINTER)
	{
		ApplyPointerInitializer(entity, type, value);
		return;
	}
	if (type.kind == Type::ARRAY)
	{
		ApplyArrayInitializer(entity, type, value);
		return;
	}
	if (type.kind == Type::FUNDAMENTAL)
	{
		ApplyScalarInitializer(entity, type, value);
		return;
	}
	throw logic_error("invalid object initializer");
}

void Parser::ProcessTypedef(const DeclSpec& spec, const DeclaratorShape& shape,
	const Type& type)
{
	Namespace* target = DeclarationNamespace(shape.name);
	ValidateType(type, false);
	if (!shape.has_name || shape.name.parts.empty())
		throw logic_error("typedef requires a name");
	program_->AddTypeDef(target, shape.name.parts.back(), type, unit_id_);
	(void)spec;
}

void Parser::ProcessFunction(const DeclSpec& spec, const DeclaratorShape& shape,
	const Type& type, bool definition)
{
	Namespace* target = DeclarationNamespace(shape.name);
	if (type.kind != Type::FUNCTION) throw logic_error("function body requires function type");
	ValidateType(type, false);
	const bool qualified = shape.name.absolute || shape.name.parts.size() > 1;
	const bool internal = !qualified &&
		(spec.is_static || (target->unnamed && !target->global));
	Entity* entity = program_->AddFunction(target, shape.name.parts.back(), type,
		internal, unit_id_);
	if (definition)
	{
		if (entity->function_definition && !(entity->is_inline && spec.is_inline))
			throw logic_error("duplicate function definition");
		entity->function_definition = true;
	}
	entity->is_inline = entity->is_inline || spec.is_inline;
}

bool HasDefinition(const DeclSpec& spec, bool has_initializer)
{
	return !spec.is_extern || has_initializer;
}

void Parser::ProcessVariable(const DeclSpec& spec, const DeclaratorShape& shape,
	Type type, const ExprValue* initializer)
{
	Namespace* target = DeclarationNamespace(shape.name);
	const bool has_init = initializer != NULL;
	if (spec.is_constexpr && !has_init)
		throw logic_error("constexpr object requires initializer");
	if (type.kind == Type::ARRAY && type.bound < 0 && has_init &&
		initializer->string != NULL)
		type = Type::Array(initializer->string->type.bound, *type.child);
	ValidateType(type, true);
	if (type.kind == Type::ARRAY && type.bound < 0 &&
		HasDefinition(spec, has_init))
		throw logic_error("object has incomplete type");
	const bool qualified = shape.name.absolute || shape.name.parts.size() > 1;
	const bool internal = !qualified && IsInternal(spec, target);
	Entity* entity = program_->AddVariable(target, shape.name.parts.back(), type,
		internal, unit_id_);
	entity->is_constexpr = entity->is_constexpr || spec.is_constexpr;
	entity->has_initializer = entity->has_initializer || has_init;
	if (!HasDefinition(spec, has_init)) return;
	if (entity->has_definition && !has_init)
		throw logic_error("duplicate variable definition");
	if (entity->has_definition && has_init)
		throw logic_error("duplicate variable definition");
	entity->has_definition = true;
	if (has_init)
	{
		ApplyInitializer(entity, entity->type, *initializer, target);
		return;
	}
	if (entity->type.kind == Type::LVALUE_REFERENCE ||
		entity->type.kind == Type::RVALUE_REFERENCE ||
		(entity->type.kind == Type::FUNDAMENTAL && IsConstQualified(entity->type)) ||
		(entity->type.kind == Type::ARRAY && IsConstQualified(entity->type)))
		throw logic_error("type cannot be default initialized");
	entity->initializer = InitData();
}

void Parser::ParseFunctionBody()
{
	Expect("{");
	Expect("}");
}

void Parser::ParseNamespaceDefinition(bool inline_namespace)
{
	Expect("namespace");
	Namespace* child = NULL;
	if (IsNameToken(Peek())) child = program_->NamedNamespace(current_, TakeName(),
		inline_namespace);
	else child = program_->UnnamedNamespace(current_, unit_id_, inline_namespace);
	Expect("{");
	Namespace* saved = current_;
	current_ = child;
	while (!Is("}"))
	{
		if (AtEnd()) throw logic_error("unterminated namespace");
		ParseDeclaration();
	}
	Expect("}");
	current_ = saved;
	Take(";");
}

void Parser::ParseNamespaceAliasDefinition()
{
	Expect("namespace");
	const string alias_name = TakeName();
	Expect("=");
	const NamePath path = ParseNamePath();
	Namespace* target = program_->ResolveNamespacePath(path, current_, unit_id_);
	if (target == NULL || current_->named_children.find(alias_name) !=
		current_->named_children.end() || current_->bindings.find(alias_name) !=
		current_->bindings.end())
		throw logic_error("namespace alias misuse");
	map<string, Namespace*>::iterator old = current_->namespace_aliases.find(alias_name);
	if (old != current_->namespace_aliases.end() && old->second != target)
		throw logic_error("namespace alias misuse");
	current_->namespace_aliases[alias_name] = target;
	Expect(";");
}

void Parser::ParseUsingDirective()
{
	Expect("using");
	Expect("namespace");
	const NamePath path = ParseNamePath();
	Namespace* target = program_->ResolveNamespacePath(path, current_, unit_id_);
	if (target == NULL) throw logic_error("unknown using namespace");
	for (size_t i = 0; i < current_->using_directives.size(); ++i)
		if (current_->using_directives[i] == target) { Expect(";"); return; }
	current_->using_directives.push_back(target);
	Expect(";");
}

void Parser::ParseUsingDeclaration()
{
	Expect("using");
	const NamePath path = ParseNamePath();
	Entity* target = program_->ResolveEntityPath(path, current_, unit_id_);
	if (target == NULL)
	{
		if (program_->ResolveNamespacePath(path, current_, unit_id_) != NULL)
			throw logic_error("namespace alias misuse");
		throw logic_error("unknown using target");
	}
	program_->AddImport(current_, target);
	Expect(";");
}

void Parser::ParseAliasDeclaration()
{
	Expect("using");
	const string alias_name = TakeName();
	Expect("=");
	const Type type = ParseTypeId();
	program_->AddTypeDef(current_, alias_name, type, unit_id_);
	Expect(";");
}

void Parser::CheckStaticAssert(const ExprValue& value) const
{
	if (!value.constant) throw logic_error("static_assert expression is not constant");
	if (value.null_pointer) throw logic_error("static_assert failed");
	if (value.integral && value.integer == 0) throw logic_error("static_assert failed");
	if (value.floating && value.real == 0) throw logic_error("static_assert failed");
	if (value.type.kind == Type::POINTER && value.has_pointer_value &&
		value.pointer_value.IsNull())
		throw logic_error("static_assert failed");
}

void Parser::ParseStaticAssert()
{
	Expect("static_assert");
	Expect("(");
	const ExprValue value = ParseExpression(current_);
	Expect(",");
	if (Peek().kind != POST_PP_STRING && Peek().kind != POST_PP_USER_STRING)
		throw logic_error("static_assert message is not a literal");
	++position_;
	Expect(")");
	Expect(";");
	CheckStaticAssert(value);
}

void Parser::ParseSimpleDeclaration()
{
	const DeclSpec spec = ParseDeclSpecifierSeq();
	while (true)
	{
		const DeclaratorShape shape = ParseDeclarator(NAMED_REQUIRED);
		Type type = ApplyDeclarator(spec.type, shape);
		if (spec.is_constexpr && type.kind != Type::FUNCTION)
			type = AddCv(type, true, false);
		Namespace* target = DeclarationNamespace(shape.name);
		ExprValue initializer;
		const ExprValue* initializer_ptr = NULL;
		if (Take("="))
		{
			initializer = ParseExpression(target);
			initializer_ptr = &initializer;
		}
		if (type.kind == Type::FUNCTION && Is("{"))
		{
			if (initializer_ptr != NULL || spec.is_typedef || Take(","))
				throw logic_error("invalid function definition");
			ProcessFunction(spec, shape, type, true);
			ParseFunctionBody();
			return;
		}
		if (spec.is_typedef) ProcessTypedef(spec, shape, type);
		else if (type.kind == Type::FUNCTION)
		{
			if (initializer_ptr != NULL) throw logic_error("function initializer");
			ProcessFunction(spec, shape, type, false);
		}
		else ProcessVariable(spec, shape, type, initializer_ptr);
		if (!Take(",")) break;
	}
	Expect(";");
}

void Parser::ParseDeclaration()
{
	if (Take(";")) return;
	if (Is("static_assert")) { ParseStaticAssert(); return; }
	if (Is("inline") && Peek(1).source == "namespace")
	{
		Take("inline");
		ParseNamespaceDefinition(true);
		return;
	}
	if (Is("namespace"))
	{
		if (IsNameToken(Peek(1)) && Peek(2).source == "=")
			ParseNamespaceAliasDefinition();
		else ParseNamespaceDefinition(false);
		return;
	}
	if (Is("using"))
	{
		if (Peek(1).source == "namespace") ParseUsingDirective();
		else if (IsNameToken(Peek(1)) && Peek(2).source == "=")
			ParseAliasDeclaration();
		else ParseUsingDeclaration();
		return;
	}
	ParseSimpleDeclaration();
}

void Parser::ParseTranslationUnit()
{
	while (!AtEnd()) ParseDeclaration();
}

} // namespace

void BuildNSInitImage(const vector<string>& source_paths,
	vector<unsigned char>* image)
{
	if (image == NULL) throw logic_error("missing image output");
	Program program;
	for (size_t i = 0; i < source_paths.size(); ++i)
	{
		const vector<PostPPToken> tokens = PreprocessSourceFile(source_paths[i]);
		if (!ValidatePostTokens(tokens))
			throw logic_error("invalid post-token sequence");
		Parser parser(tokens, &program, static_cast<int>(i));
		parser.ParseTranslationUnit();
	}
	BuildNSInitMockImage(&program, image);
}
