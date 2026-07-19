#pragma once

#include <string>
#include <vector>

enum PostPPTokenKind
{
	POST_PP_IDENTIFIER,
	POST_PP_NUMBER,
	POST_PP_CHARACTER,
	POST_PP_USER_CHARACTER,
	POST_PP_STRING,
	POST_PP_USER_STRING,
	POST_PP_PUNCTUATOR,
	POST_PP_HEADER,
	POST_PP_NON_WHITESPACE,
	POST_PP_EOF
};

struct PostPPToken
{
	PostPPTokenKind kind;
	std::string source;

	PostPPToken(PostPPTokenKind kind = POST_PP_EOF,
		const std::string& source = std::string())
		: kind(kind), source(source)
	{}
};

std::vector<PostPPToken> LexPostPPSource(const std::string& input);
