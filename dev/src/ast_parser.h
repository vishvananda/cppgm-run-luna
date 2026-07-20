#pragma once

#include <cstddef>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "posttoken_lexer.h"

struct CPPGMAstNode;
typedef std::shared_ptr<CPPGMAstNode> CPPGMAstNodePtr;

enum CPPGMAstInitializerForm
{
	AST_INITIALIZER_NONE,
	AST_INITIALIZER_COPY,
	AST_INITIALIZER_DIRECT_LIST,
	AST_INITIALIZER_DIRECT_PAREN
};

struct CPPGMAstNode
{
	std::string kind;
	std::string value;
	CPPGMAstInitializerForm initializer_form;
	std::vector<CPPGMAstNodePtr> children;

	CPPGMAstNode(const std::string& kind = std::string(),
		const std::string& value = std::string());
};

CPPGMAstNodePtr ParsePA10TranslationUnit(const std::vector<PostPPToken>& tokens);
void PrintPA10Ast(const CPPGMAstNodePtr& node, std::ostream& output,
	unsigned int indentation = 0);

namespace cppgm_pa10 {

enum AstTokenKind
{
	AST_IDENTIFIER,
	AST_LITERAL,
	AST_PUNCTUATOR,
	AST_RSHIFT_1,
	AST_RSHIFT_2,
	AST_EOF
};

struct NameFacts
{
	bool class_name;
	bool template_name;
	bool typedef_name;
	bool enum_name;
	bool namespace_name;

	NameFacts()
		: class_name(false), template_name(false), typedef_name(false),
		  enum_name(false), namespace_name(false) {}
};

struct Token
{
	AstTokenKind kind;
	std::string text;
	NameFacts names;

	Token(AstTokenKind kind = AST_EOF, const std::string& text = std::string())
		: kind(kind), text(text), names() {}
};

class Parser
{
public:
	explicit Parser(const std::vector<Token>& tokens);
	CPPGMAstNodePtr Parse();
	static std::vector<Token> Normalize(const std::vector<PostPPToken>& input);

private:
	struct Mark
	{
		std::size_t position;
		int angle_depth;
		int ordinary_depth;
		std::vector<int> angle_floors;
	};

	const std::vector<Token>& tokens_;
	std::size_t position_;
	int angle_depth_;
	int ordinary_depth_;
	std::vector<int> angle_floors_;
	std::set<std::string> types_;
	std::set<std::string> templates_;
	std::set<std::string> namespaces_;
	std::set<std::string> value_names_;
	std::string current_class_;

	Mark Save() const;
	void Restore(const Mark& mark);
	const Token& Peek(std::size_t offset = 0) const;
	bool AtEnd() const;
	bool Is(const std::string& text) const;
	bool Take(const std::string& text);
	bool TakeIdentifier(std::string* text = 0);
	bool TakeLiteral(std::string* text = 0);
	bool TakeCloseAngle();
	void EnterAngle();
	void LeaveAngle();
	bool TakeShiftRight();
	bool CloseAngleBlocked() const;

	CPPGMAstNodePtr Node(const std::string& kind,
		const std::string& value = std::string()) const;
	void Add(const CPPGMAstNodePtr& parent, const CPPGMAstNodePtr& child) const;
	void RegisterType(const std::string& name);
	void RegisterTemplate(const std::string& name);
	bool IsTypeStart() const;
	bool IsNamedTypeStart() const;
	bool IsFundamental(const std::string& text) const;
	bool IsStorageOrFunctionSpecifier(const std::string& text) const;
	bool IsCv(const std::string& text) const;
	std::string TokenLabel(const std::string& text) const;

	CPPGMAstNodePtr ParseDeclaration(bool member_context = false);
	CPPGMAstNodePtr ParseNamespaceDefinition();
	CPPGMAstNodePtr ParseNamespaceAliasDefinition();
	CPPGMAstNodePtr ParseUsingDeclaration(bool directive);
	CPPGMAstNodePtr ParseLinkageSpecification();
	CPPGMAstNodePtr ParseTemplateDeclaration(bool member_context = false);
	CPPGMAstNodePtr ParseExplicitInstantiation();
	CPPGMAstNodePtr ParseClassSpecifier(bool declaration_context = true,
		const std::vector<CPPGMAstNodePtr>& leading_attributes =
			std::vector<CPPGMAstNodePtr>());
	CPPGMAstNodePtr ParseClassMember();
	CPPGMAstNodePtr ParseBitFieldDeclaration();
	CPPGMAstNodePtr ParseEnumSpecifier(bool declaration_context = true);
	CPPGMAstNodePtr ParseStaticAssertDeclaration();
	CPPGMAstNodePtr ParseSimpleOrFunctionDeclaration(bool member_context = false);
	CPPGMAstNodePtr ParseSpecialMember(bool definition, bool member_context);
	CPPGMAstNodePtr ParseDeclSpecifierSeq(bool type_id_context = false);
	CPPGMAstNodePtr ParseDeclSpecifier(bool type_id_context = false);
	CPPGMAstNodePtr ParseTypeSpecifierSeq();
	CPPGMAstNodePtr ParseTypeSpecifier();
	CPPGMAstNodePtr ParseTypeId();
	CPPGMAstNodePtr ParseDeclarator(bool allow_abstract = false);
	CPPGMAstNodePtr ParseDeclaratorCore(bool allow_abstract);
	CPPGMAstNodePtr ParsePtrOperator();
	CPPGMAstNodePtr ParseParametersAndQualifiers();
	CPPGMAstNodePtr ParseParameterClause();
	CPPGMAstNodePtr ParseParameterDeclaration();
	CPPGMAstNodePtr ParseAbstractDeclarator();
	CPPGMAstNodePtr ParseInitializer();
	CPPGMAstNodePtr ParseInitializerClause();
	CPPGMAstNodePtr ParseBracedInitList();
	CPPGMAstNodePtr ParseCtorInitializer();
	CPPGMAstNodePtr ParseMemInitializer();
	CPPGMAstNodePtr ParseBaseClause();
	CPPGMAstNodePtr ParseTemplateParameterClause();
	CPPGMAstNodePtr ParseTemplateParameter();
	CPPGMAstNodePtr ParseTypeParameter();
	CPPGMAstNodePtr ParseTemplateArgumentList();
	CPPGMAstNodePtr ParseTemplateArgument();

	CPPGMAstNodePtr ParseCompoundStatement();
	CPPGMAstNodePtr ParseStatement();
	CPPGMAstNodePtr ParseLabeledStatement();
	CPPGMAstNodePtr ParseSelectionStatement();
	CPPGMAstNodePtr ParseIterationStatement();
	CPPGMAstNodePtr ParseJumpStatement();
	CPPGMAstNodePtr ParseTryBlock();
	CPPGMAstNodePtr ParseHandler();
	CPPGMAstNodePtr ParseExceptionDeclaration();
	CPPGMAstNodePtr ParseExpressionStatement();
	CPPGMAstNodePtr ParseCondition();

	CPPGMAstNodePtr ParseExpression();
	CPPGMAstNodePtr ParseAssignmentExpression();
	CPPGMAstNodePtr ParseConditionalExpression();
	CPPGMAstNodePtr ParseBinaryExpression(int level);
	CPPGMAstNodePtr ParseUnaryExpression();
	CPPGMAstNodePtr ParsePostfixExpression();
	CPPGMAstNodePtr ParsePostfixSuffix(const CPPGMAstNodePtr& expression);
	CPPGMAstNodePtr ParsePrimaryExpression();
	CPPGMAstNodePtr ParseCallSuffix(const CPPGMAstNodePtr& callee,
		bool builtin_style);
	CPPGMAstNodePtr ParseLambdaExpression();
	CPPGMAstNodePtr ParseNewExpression();
	CPPGMAstNodePtr ParseNewInitializer();
	CPPGMAstNodePtr ParseDeleteExpression();
	CPPGMAstNodePtr ParseTypeTraitExpression();
	CPPGMAstNodePtr ParseKeywordCastExpression();

	CPPGMAstNodePtr ParseIdExpression();
	bool ParseOperatorName(std::string* value, bool allow_template);
	bool ParseName(std::string* value, bool allow_operator = true,
		bool allow_template = true);
	bool ParseIdentifierName(std::string* value);
	bool ParseTemplateSuffix(std::string* value);
	CPPGMAstNodePtr ParseDecltypeSpecifier();
	CPPGMAstNodePtr ParseNamedType(bool type_node);
	bool LooksLikeTypeName(const Token& token) const;
	void SkipAttributes(std::vector<CPPGMAstNodePtr>* captured = 0);
	bool ParseFunctionSuffixes(const CPPGMAstNodePtr& declarator);

};

} // namespace cppgm_pa10
