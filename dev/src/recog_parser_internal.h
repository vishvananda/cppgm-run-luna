#pragma once

#include <cstddef>
#include <set>
#include <string>
#include <stdexcept>
#include <vector>

#include "posttoken_lexer.h"

namespace recog_pa6 {
using namespace std;

bool IsLiteralKind(PostPPTokenKind kind);
bool IsKeyword(const string& text);
bool IsIdentifierToken(const struct RecognizerToken& token);
bool IsLiteralToken(const struct RecognizerToken& token);
bool IsEmptyStringLiteral(const struct RecognizerToken& token);
bool IsZeroLiteral(const struct RecognizerToken& token);

enum RecognizerTokenKind
{
	RK_IDENTIFIER,
	RK_LITERAL,
	RK_PUNCTUATOR,
	RK_RSHIFT_1,
	RK_RSHIFT_2,
	RK_EOF
};

struct NameFacts
{
	bool class_name;
	bool template_name;
	bool typedef_name;
	bool enum_name;
	bool namespace_name;

	NameFacts() : class_name(false), template_name(false), typedef_name(false),
		enum_name(false), namespace_name(false) {}
};

struct RecognizerToken
{
	RecognizerTokenKind kind;
	string text;
	NameFacts names;

	RecognizerToken(RecognizerTokenKind kind = RK_EOF,
		const string& text = string()) : kind(kind), text(text), names() {}
};

vector<RecognizerToken> NormalizeTokens(const vector<PostPPToken>& input);

class Parser
{
public:
	explicit Parser(const vector<RecognizerToken>& tokens);

	bool Parse();

private:
	struct Mark
	{
		size_t position;
		int angle_depth;
		int ordinary_depth;
		vector<int> angle_floors;
	};

	const vector<RecognizerToken>& tokens_;
	size_t position_;
	int angle_depth_;
	int ordinary_depth_;
	vector<int> angle_floors_;

	Mark Save() const;
	void Restore(const Mark& mark);
	void EnterAngle();
	void LeaveAngle();
	const RecognizerToken& Peek(size_t offset = 0) const;
	bool AtEnd() const;
	bool Is(const string& text) const;
	bool Take(const string& text);
	bool TakeIdentifier();
	bool TakeLiteral();
	bool TakeCloseAngle();
	bool TakeShiftRight();
	bool CloseAngleBlocked() const;

	bool IsOperatorAlias(const string& text, const string& spelling) const;
	bool TakeOperator(const string& spelling);
	bool TakeAnyOperator(const set<string>& spellings);

	// Translation unit and declarations.
	bool ParseTranslationUnit();
	bool ParseDeclaration();
	bool ParseBlockDeclaration();
	bool ParseSimpleDeclaration();
	bool ParseEmptyDeclaration();
	bool ParseAttributeDeclaration();
	bool ParseStaticAssertDeclaration();
	bool ParseAliasDeclaration();
	bool ParseAsmDefinition();
	bool ParseNamespaceDefinition();
	bool ParseNamespaceAliasDefinition();
	bool ParseQualifiedNamespaceSpecifier();
	bool ParseUsingDeclaration();
	bool ParseUsingDirective();
	bool ParseLinkageSpecification();
	bool ParseOpaqueEnumDeclaration();
	bool ParseTemplateDeclaration();
	bool ParseExplicitInstantiation();
	bool ParseExplicitSpecialization();
	bool ParseFunctionDefinition();

	// Statements.
	bool ParseStatement();
	bool ParseLabeledStatement();
	bool ParseExpressionStatement();
	bool ParseCompoundStatement();
	bool ParseSelectionStatement();
	bool ParseIterationStatement();
	bool ParseJumpStatement();
	bool ParseCondition();
	bool ParseConditionDeclaration();
	bool ParseForRangeDeclaration();
	bool ParseForRangeInitializer();
	bool ParseTryBlock();
	bool ParseFunctionTryBlock();
	bool ParseHandler();
	bool ParseExceptionDeclaration();

	// Expressions.
	bool ParseExpression();
	bool ParseAssignmentExpression();
	bool ParseConditionalExpression();
	bool ParseLogicalOrExpression();
	bool ParseLogicalAndExpression();
	bool ParseInclusiveOrExpression();
	bool ParseExclusiveOrExpression();
	bool ParseAndExpression();
	bool ParseEqualityExpression();
	bool ParseRelationalExpression();
	bool ParseShiftExpression();
	bool ParseAdditiveExpression();
	bool ParseMultiplicativeExpression();
	bool ParsePMExpression();
	bool ParseCastExpression();
	bool ParseUnaryExpression();
	bool ParsePostfixExpression();
	bool ParsePostfixRoot();
	bool ParsePostfixSuffix();
	bool ParsePrimaryExpression();
	bool ParseLambdaExpression();
	bool ParseLambdaIntroducer();
	bool ParseLambdaCapture();
	bool ParseCaptureDefault();
	bool ParseCaptureList();
	bool ParseCapture();
	bool ParseLambdaDeclarator();
	bool ParseNewExpression();
	bool ParseNewDeclarator();
	bool ParseNewInitializer();
	bool ParseDeleteExpression();
	bool ParseNoexceptExpression();
	bool ParseThrowExpression();
	bool ParseExpressionList();

	// Names and mock lookup.
	bool ParseIdExpression();
	bool ParseUnqualifiedId();
	bool ParseQualifiedId();
	bool ParseTemplateId();
	bool ParseSimpleTemplateId();
	bool ParseTemplateName();
	bool ParseClassName();
	bool ParseEnumName();
	bool ParseTypedefName();
	bool ParseNamespaceName();
	bool ParseTypeName();
	bool ParseNestedNameSpecifier();
	bool ParseDecltypeSpecifier();
	bool ParseTypenameSpecifier();
	bool ParsePseudoDestructorName();
	bool ParseOperatorFunctionId();
	bool ParseConversionFunctionId();
	bool ParseConversionTypeId();
	bool ParseLiteralOperatorId();

	// Type specifiers and declarators.
	bool ParseDeclSpecifierSeq();
	bool ParseDeclSpecifier();
	bool ParseTypeSpecifierSeq();
	bool ParseTrailingTypeSpecifierSeq();
	bool ParseTypeSpecifier();
	bool ParseTrailingTypeSpecifier();
	bool ParseSimpleTypeSpecifier();
	bool ParseElaboratedTypeSpecifier();
	bool ParseClassSpecifier();
	bool ParseClassHead();
	bool ParseEnumSpecifier();
	bool ParseEnumHead();
	bool ParseEnumKey();
	bool ParseEnumBase();
	bool ParseEnumeratorList();
	bool ParseBaseClause();
	bool ParseBaseSpecifierList();
	bool ParseBaseSpecifier();
	bool ParseClassOrDecltype();
	bool ParseAccessSpecifier();
	bool ParseDeclarator();
	bool ParsePtrDeclarator();
	bool ParseNoptrDeclarator();
	bool ParseNoptrDeclaratorRoot();
	bool ParseNoptrDeclaratorSuffix();
	bool ParseParametersAndQualifiers();
	bool ParsePtrOperator();
	bool ParseDeclaratorId();
	bool ParseTypeId();
	bool ParseAbstractDeclarator();
	bool ParsePtrAbstractDeclarator();
	bool ParseNoptrAbstractDeclarator();
	bool ParseNoptrAbstractDeclaratorRoot();
	bool ParseAbstractPackDeclarator();
	bool ParseParameterDeclarationClause();
	bool ParseParameterDeclarationList();
	bool ParseParameterDeclaration();
	bool ParseTrailingReturnType();
	bool ParseVirtSpecifier();
	bool ParseRefQualifier();

	// Initializers and function/class/template support.
	bool ParseInitializer();
	bool ParseBraceOrEqualInitializer();
	bool ParseInitializerClause();
	bool ParseInitializerList();
	bool ParseInitializerClauseDots();
	bool ParseBracedInitList();
	bool ParseFunctionBody();
	bool ParseCtorInitializer();
	bool ParseMemInitializerList();
	bool ParseMemInitializer();
	bool ParseMemInitializerId();
	bool ParseMemberSpecification();
	bool ParseMemberDeclaration();
	bool ParseMemberDeclaratorList();
	bool ParseMemberDeclarator();
	bool ParsePureSpecifier();
	bool ParseTemplateParameterList();
	bool ParseTemplateParameter();
	bool ParseTypeParameter();
	bool ParseTemplateArgumentList();
	bool ParseTemplateArgumentDots();
	bool ParseTemplateArgument();
	bool ParseCloseAngleBracket();
	bool ParseExceptionSpecification();

	// Attributes and balanced tokens.
	bool ParseAttributeSpecifier();
	bool ParseAlignmentSpecifier();
	bool ParseAttributeList();
	bool ParseAttributePart();
	bool ParseAttribute();
	bool ParseAttributeToken();
	bool ParseAttributeScopedToken();
	bool ParseAttributeArgumentClause();
	bool ParseBalancedToken();

	bool ParseRepeatedAttributes();
	bool ParseOptionalInitializer();
	bool StartsTypeSpecifier() const;
	bool StartsDeclaration() const;
};

} // namespace recog_pa6
