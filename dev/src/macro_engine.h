#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "posttoken_lexer.h"

class MacroBlockedNames
{
public:
	MacroBlockedNames() : bits_(new std::vector<std::uint64_t>()) {}

	bool Contains(std::size_t id) const
	{
		const std::size_t word = id / 64;
		return word < bits_->size() &&
			((*bits_)[word] & (std::uint64_t(1) << (id % 64))) != 0;
	}

	void Add(std::size_t id)
	{
		const std::size_t word = id / 64;
		std::shared_ptr<std::vector<std::uint64_t> > copy(
			new std::vector<std::uint64_t>(*bits_));
		if (copy->size() <= word) copy->resize(word + 1, 0);
		(*copy)[word] |= std::uint64_t(1) << (id % 64);
		bits_ = copy;
	}

	void Merge(const MacroBlockedNames& other)
	{
		if (other.bits_->empty()) return;
		std::shared_ptr<std::vector<std::uint64_t> > copy(
			new std::vector<std::uint64_t>(*bits_));
		if (copy->size() < other.bits_->size())
			copy->resize(other.bits_->size(), 0);
		for (std::size_t i = 0; i < other.bits_->size(); ++i)
			(*copy)[i] |= (*other.bits_)[i];
		bits_ = copy;
	}

private:
	std::shared_ptr<const std::vector<std::uint64_t> > bits_;
};

enum MacroTokenKind
{
	TOKEN_SPACE,
	TOKEN_NEWLINE,
	TOKEN_IDENTIFIER,
	TOKEN_NUMBER,
	TOKEN_CHARACTER,
	TOKEN_USER_CHARACTER,
	TOKEN_STRING,
	TOKEN_USER_STRING,
	TOKEN_PUNCTUATOR,
	TOKEN_HEADER,
	TOKEN_NON_WHITESPACE,
	TOKEN_PASTE,
	TOKEN_PLACEMARKER
};

struct MacroToken
{
	MacroTokenKind kind;
	std::string text;
	std::string file;
	int line;
	MacroBlockedNames blocked;
	bool from_argument;
	bool from_replacement;
	const void* replacement_owner;

	MacroToken(MacroTokenKind kind = TOKEN_NON_WHITESPACE,
		const std::string& text = std::string(),
		const std::string& file = std::string(), int line = 1)
		: kind(kind), text(text), file(file), line(line),
		  from_argument(false), from_replacement(false),
		  replacement_owner(NULL)
	{}
};

std::vector<MacroToken> TokenizeMacroSource(
	const std::string& input, const std::string& file = std::string(),
	int line = 1);

class MacroState
{
public:
	MacroState();
	~MacroState();

	MacroState(const MacroState&) = delete;
	MacroState& operator=(const MacroState&) = delete;

	void Define(const std::string& source, const std::string& file = std::string(),
		int line = 1);
	void Undefine(const std::string& source, const std::string& file = std::string(),
		int line = 1);
	bool IsDefined(const std::string& name) const;
	std::vector<PostPPToken> Expand(const std::string& source,
		const std::string& file = std::string(), int line = 1);

private:
	void* implementation_;
};

void RunMacro(const std::string& input);
