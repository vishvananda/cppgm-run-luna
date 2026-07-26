#pragma once

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct PA19IntegralType
{
	bool integral;
	bool is_unsigned;
	unsigned bits;
	unsigned rank;
	std::string name;

	PA19IntegralType()
		: integral(false), is_unsigned(false), bits(0), rank(0), name() {}
};

inline PA19IntegralType PA19Type(const std::string& raw);

// PA19 keeps compile-time integral facts separate from source spelling.  The
// expander needs the value before PA11 sees an instantiated tree, so this
// small value model is shared by template argument normalization and the
// ordinary semantic evaluator.  `raw` is always the value in the declared
// width; signedness and width are owned by the typed integral type, rather
// than being rediscovered from a type spelling at every operation boundary.
struct PA19IntegralValue
{
	bool known;
	PA19IntegralType type;
	unsigned long long raw;

	PA19IntegralValue()
		: known(false), type(), raw(0) {}

	static PA19IntegralValue Signed(long long value, const std::string& name = "int",
		unsigned width = 32)
	{
		PA19IntegralValue result;
		result.known = true;
		result.type = PA19Type(name);
		result.type.is_unsigned = false;
		result.type.bits = width;
		result.raw = static_cast<unsigned long long>(value);
		return result;
	}

	static PA19IntegralValue Unsigned(unsigned long long value,
		const std::string& name = "unsigned int", unsigned width = 32)
	{
		PA19IntegralValue result;
		result.known = true;
		result.type = PA19Type(name);
		result.type.is_unsigned = true;
		result.type.bits = width;
		result.raw = value;
		return result;
	}
};

inline std::string PA19Trim(const std::string& raw)
{
	size_t begin = 0;
	while(begin < raw.size() && std::isspace(static_cast<unsigned char>(raw[begin]))) ++begin;
	size_t end = raw.size();
	while(end > begin && std::isspace(static_cast<unsigned char>(raw[end - 1]))) --end;
	return raw.substr(begin, end - begin);
}

inline std::string PA19Compact(const std::string& raw)
{
	std::string result;
	bool space = false;
	for(size_t i = 0; i < raw.size(); ++i) {
		const unsigned char ch = static_cast<unsigned char>(raw[i]);
		if(std::isspace(ch)) {
			space = true;
			continue;
		}
		if(space && !result.empty() &&
			(std::isalnum(static_cast<unsigned char>(result[result.size() - 1])) ||
			 result[result.size() - 1] == '_') &&
			(std::isalnum(ch) || ch == '_')) result += ' ';
		space = false;
		result += static_cast<char>(ch);
	}
	return PA19Trim(result);
}

inline bool PA19IdentifierCharacter(char ch)
{
	return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

inline PA19IntegralType PA19Type(const std::string& raw)
{
	PA19IntegralType result;
	std::string name = PA19Compact(raw);
	while(name.compare(0, 10, "constexpr ") == 0) name = PA19Compact(name.substr(10));
	while(name.compare(0, 7, "static ") == 0) name = PA19Compact(name.substr(7));
	while(name.compare(0, 6, "const ") == 0) name = PA19Compact(name.substr(6));
	while(name.compare(0, 9, "volatile ") == 0) name = PA19Compact(name.substr(9));
	// On the Linux x86_64 target, sizeof has the unsigned-long size_t type.
	// Keep that fact available to template non-type argument validation when a
	// source typedef spells size_t as decltype(sizeof(...)).
	if(name.compare(0, 16, "decltype(sizeof(") == 0 && name.size() > 17 &&
		name.substr(name.size() - 2) == "))") {
		result.integral = true; result.is_unsigned = true; result.bits = 64;
		result.rank = 5; result.name = "unsigned long";
	} else if(name == "bool") {
		result.integral = true; result.bits = 1; result.rank = 1; result.name = name;
	} else if(name == "char") {
		result.integral = true; result.bits = 8; result.rank = 2; result.name = name;
	} else if(name == "signed char") {
		result.integral = true; result.bits = 8; result.rank = 2; result.name = name;
	} else if(name == "unsigned char") {
		result.integral = true; result.is_unsigned = true; result.bits = 8; result.rank = 2; result.name = name;
	} else if(name == "short" || name == "short int" || name == "signed short" ||
		name == "signed short int") {
		result.integral = true; result.bits = 16; result.rank = 3; result.name = name;
	} else if(name == "unsigned short" || name == "unsigned short int") {
		result.integral = true; result.is_unsigned = true; result.bits = 16; result.rank = 3; result.name = name;
	} else if(name == "int" || name == "signed" || name == "signed int") {
		result.integral = true; result.bits = 32; result.rank = 4; result.name = name == "signed" ? "int" : name;
	} else if(name == "unsigned" || name == "unsigned int") {
		result.integral = true; result.is_unsigned = true; result.bits = 32; result.rank = 4; result.name = name;
	} else if(name == "long" || name == "long int" || name == "signed long" ||
		name == "signed long int") {
		result.integral = true; result.bits = 64; result.rank = 5; result.name = name == "long" ? "long int" : name;
	} else if(name == "unsigned long" || name == "unsigned long int") {
		result.integral = true; result.is_unsigned = true; result.bits = 64; result.rank = 5; result.name = name;
	} else if(name == "long long" || name == "long long int" ||
		name == "signed long long" || name == "signed long long int") {
		result.integral = true; result.bits = 64; result.rank = 6; result.name = name;
	} else if(name == "unsigned long long" || name == "unsigned long long int") {
		result.integral = true; result.is_unsigned = true; result.bits = 64; result.rank = 6; result.name = name;
	} else if(name == "wchar_t") {
		// The PA19 host model deliberately follows the signed 32-bit wchar_t
		// used by the rest of this compiler.
		result.integral = true; result.bits = 32; result.rank = 4; result.name = name;
	} else if(name == "char16_t") {
		result.integral = true; result.bits = 16; result.rank = 3; result.name = name;
	} else if(name == "char32_t") {
		result.integral = true; result.is_unsigned = true; result.bits = 32; result.rank = 4; result.name = name;
	}
	return result;
}

inline unsigned long long PA19Mask(unsigned bits)
{
	return bits >= 64 ? ~0ULL : (bits == 0 ? 0ULL : ((1ULL << bits) - 1ULL));
}

inline unsigned long long PA19Raw(const PA19IntegralValue& value)
{
	return value.raw & PA19Mask(value.type.bits);
}

inline long long PA19Signed(const PA19IntegralValue& value)
{
	const unsigned long long raw = PA19Raw(value);
	if(value.type.name == "bool") return raw != 0;
	if(value.type.bits == 0 || value.type.bits >= 64) return static_cast<long long>(raw);
	const unsigned long long sign = 1ULL << (value.type.bits - 1);
	if((raw & sign) == 0) return static_cast<long long>(raw);
	return static_cast<long long>(raw | ~PA19Mask(value.type.bits));
}

inline PA19IntegralValue PA19Convert(const PA19IntegralValue& value,
	const PA19IntegralType& type)
{
	if(!value.known || !type.integral) return PA19IntegralValue();
	unsigned long long raw = PA19Raw(value);
	// Widening a signed value sign-extends before the destination-width mask;
	// truncating and unsigned conversions retain the ordinary bit pattern.
	if(type.bits > value.type.bits && !value.type.is_unsigned)
		raw = static_cast<unsigned long long>(PA19Signed(value));
	raw &= PA19Mask(type.bits);
	if(type.is_unsigned) return PA19IntegralValue::Unsigned(raw, type.name, type.bits);
	return PA19IntegralValue::Signed(static_cast<long long>(raw), type.name, type.bits);
}

inline PA19IntegralValue PA19Promote(const PA19IntegralValue& value)
{
	if(!value.known) return value;
	const PA19IntegralType type = value.type;
	if(!type.integral || type.rank >= 4) return value;
	// All ordinary narrow integral types fit in the signed host int model.
	return PA19IntegralValue::Signed(PA19Signed(value), "int", 32);
}

inline PA19IntegralType PA19CommonType(const PA19IntegralValue& left,
	const PA19IntegralValue& right)
{
	const PA19IntegralValue a = PA19Promote(left);
	const PA19IntegralValue b = PA19Promote(right);
	const PA19IntegralType at = a.type;
	const PA19IntegralType bt = b.type;
	PA19IntegralType result;
	result.integral = at.integral && bt.integral;
	if(!result.integral) return result;
	if(at.is_unsigned == bt.is_unsigned) {
		result = at.rank >= bt.rank ? at : bt;
		return result;
	}
	const PA19IntegralType& uns = at.is_unsigned ? at : bt;
	const PA19IntegralType& sig = at.is_unsigned ? bt : at;
	if(uns.rank >= sig.rank || uns.bits >= sig.bits) {
		result = uns;
		return result;
	}
	result = sig;
	return result;
}

inline PA19IntegralValue PA19Binary(const std::string& operation,
	const PA19IntegralValue& left_raw, const PA19IntegralValue& right_raw)
{
	if(!left_raw.known || !right_raw.known) return PA19IntegralValue();
	const PA19IntegralValue left = PA19Promote(left_raw);
	const PA19IntegralValue right = PA19Promote(right_raw);
	// Shift expressions keep the promoted type of the left operand.  The
	// right operand is only a shift count; applying the usual arithmetic
	// conversions to both operands would incorrectly turn `1LL << n` into an
	// unsigned-long result whenever `n` came from sizeof.
	if(operation == "<<" || operation == ">>") {
		const unsigned long long shift = PA19Raw(right);
		const PA19IntegralType type = left.type;
		if(!type.integral || shift >= type.bits) return PA19IntegralValue();
		const unsigned long long raw = PA19Raw(left);
		const unsigned long long shifted = operation == "<<" ?
			(raw << shift) & PA19Mask(type.bits) :
			(type.is_unsigned ? raw >> shift :
				static_cast<unsigned long long>(PA19Signed(left) >> shift));
		return type.is_unsigned ?
			PA19IntegralValue::Unsigned(shifted, type.name, type.bits) :
			PA19IntegralValue::Signed(static_cast<long long>(shifted), type.name, type.bits);
	}
	const PA19IntegralType common = PA19CommonType(left, right);
	if(!common.integral) return PA19IntegralValue();
	const PA19IntegralValue a = PA19Convert(left, common);
	const PA19IntegralValue b = PA19Convert(right, common);
	const unsigned long long ar = PA19Raw(a);
	const unsigned long long br = PA19Raw(b);
	const long long as = PA19Signed(a);
	const long long bs = PA19Signed(b);
	const unsigned long long result_mask = PA19Mask(common.bits);
	if(operation == "&&" || operation == "and")
		return PA19IntegralValue::Signed((ar != 0 && br != 0) ? 1 : 0, "int", 32);
	if(operation == "||" || operation == "or")
		return PA19IntegralValue::Signed((ar != 0 || br != 0) ? 1 : 0, "int", 32);
	if(operation == "==") return PA19IntegralValue::Signed(ar == br, "int", 32);
	if(operation == "!=") return PA19IntegralValue::Signed(ar != br, "int", 32);
	if(operation == "<") return PA19IntegralValue::Signed(common.is_unsigned ? ar < br : as < bs, "int", 32);
	if(operation == ">") return PA19IntegralValue::Signed(common.is_unsigned ? ar > br : as > bs, "int", 32);
	if(operation == "<=") return PA19IntegralValue::Signed(common.is_unsigned ? ar <= br : as <= bs, "int", 32);
	if(operation == ">=") return PA19IntegralValue::Signed(common.is_unsigned ? ar >= br : as >= bs, "int", 32);
	if(operation == ",") return right_raw;
	if(operation == "&") return common.is_unsigned ? PA19IntegralValue::Unsigned(ar & br, common.name, common.bits) : PA19IntegralValue::Signed(static_cast<long long>(ar & br), common.name, common.bits);
	if(operation == "|") return common.is_unsigned ? PA19IntegralValue::Unsigned(ar | br, common.name, common.bits) : PA19IntegralValue::Signed(static_cast<long long>(ar | br), common.name, common.bits);
	if(operation == "^") return common.is_unsigned ? PA19IntegralValue::Unsigned(ar ^ br, common.name, common.bits) : PA19IntegralValue::Signed(static_cast<long long>(ar ^ br), common.name, common.bits);
	unsigned long long raw = 0;
	if(operation == "+") raw = ar + br;
	else if(operation == "-") raw = ar - br;
	else if(operation == "*") raw = ar * br;
	else if(operation == "/") {
		if(br == 0) return PA19IntegralValue();
		raw = common.is_unsigned ? ar / br : static_cast<unsigned long long>(as / bs);
	} else if(operation == "%") {
		if(br == 0) return PA19IntegralValue();
		raw = common.is_unsigned ? ar % br : static_cast<unsigned long long>(as % bs);
	} else return PA19IntegralValue();
	raw &= result_mask;
	return common.is_unsigned ? PA19IntegralValue::Unsigned(raw, common.name, common.bits) :
		PA19IntegralValue::Signed(static_cast<long long>(raw), common.name, common.bits);
}

inline bool PA19ParseInteger(const std::string& raw, PA19IntegralValue* result)
{
	if(!result) return false;
	std::string value = PA19Trim(raw);
	if(value.empty()) return false;
	while(!value.empty() && value[value.size() - 1] == ';') value.erase(value.size() - 1);
	const bool negative = !value.empty() && value[0] == '-';
	const bool positive = !value.empty() && value[0] == '+';
	if(negative || positive) value.erase(0, 1);
	std::string suffix;
	while(!value.empty() && (value[value.size() - 1] == 'u' || value[value.size() - 1] == 'U' ||
		value[value.size() - 1] == 'l' || value[value.size() - 1] == 'L')) {
		suffix = value[value.size() - 1] + suffix;
		value.erase(value.size() - 1);
	}
	if(value.empty()) return false;
	int base = 10;
	size_t begin = 0;
	if(value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
		base = 16; begin = 2;
	} else if(value.size() > 2 && value[0] == '0' && (value[1] == 'b' || value[1] == 'B')) {
		base = 2; begin = 2;
	} else if(value.size() > 1 && value[0] == '0') base = 8;
	unsigned long long number = 0;
	for(size_t i = begin; i < value.size(); ++i) {
		const char ch = value[i];
		unsigned digit = 0;
		if(ch >= '0' && ch <= '9') digit = static_cast<unsigned>(ch - '0');
		else if(ch >= 'a' && ch <= 'f') digit = static_cast<unsigned>(ch - 'a' + 10);
		else if(ch >= 'A' && ch <= 'F') digit = static_cast<unsigned>(ch - 'A' + 10);
		else return false;
		if(digit >= static_cast<unsigned>(base)) return false;
		number = number * static_cast<unsigned>(base) + digit;
	}
	const bool uns = suffix.find('u') != std::string::npos || suffix.find('U') != std::string::npos;
	const bool ll = suffix.find("ll") != std::string::npos || suffix.find("LL") != std::string::npos ||
		(suffix.size() == 2 && suffix.find('l') != std::string::npos && suffix.find('L') != std::string::npos);
	const bool one_long = !ll && (suffix.find('l') != std::string::npos ||
		suffix.find('L') != std::string::npos);
	std::string type;
	unsigned bits = 32;
	if(ll) { bits = 64; type = uns ? "unsigned long long" : "long long"; }
	else if(one_long) { bits = 64; type = uns ? "unsigned long" : "long int"; }
	else if(uns) type = "unsigned int";
	else if(base != 10 && number > 0x7fffffffULL) type = number <= 0xffffffffULL ? "unsigned int" : "unsigned long long";
	else if(number > 0x7fffffffULL) { bits = 64; type = "long long"; }
	else type = "int";
	if(uns) *result = PA19IntegralValue::Unsigned(negative ? 0ULL - number : number, type, bits);
	else *result = PA19IntegralValue::Signed(negative ? -static_cast<long long>(number) :
		static_cast<long long>(number), type, bits);
	return true;
}

inline bool PA19DecodeCharacter(const std::string& raw, PA19IntegralValue* result)
{
	if(!result) return false;
	std::string value = PA19Trim(raw);
	std::string type = "char";
	if(value.compare(0, 2, "L'") == 0) { type = "wchar_t"; value.erase(0, 1); }
	else if(value.compare(0, 2, "u'") == 0) { type = "char16_t"; value.erase(0, 1); }
	else if(value.compare(0, 2, "U'") == 0) { type = "char32_t"; value.erase(0, 1); }
	if(value.size() < 2 || value[0] != '\'' || value[value.size() - 1] != '\'') return false;
	value = value.substr(1, value.size() - 2);
	unsigned long long code = 0;
	if(value.empty()) return false;
	if(value[0] != '\\') code = static_cast<unsigned char>(value[0]);
	else if(value.size() == 2) {
		switch(value[1]) {
		case 'a': code = 7; break; case 'b': code = 8; break; case 'f': code = 12; break;
		case 'n': code = 10; break; case 'r': code = 13; break; case 't': code = 9; break;
		case 'v': code = 11; break; case '0': code = 0; break; case '\\': code = '\\'; break;
		case '\'': code = '\''; break; case '"': code = '"'; break;
		default: return false;
		}
	} else if(value[1] == 'x') {
		std::string digits = value.substr(2);
		for(size_t i = 0; i < digits.size(); ++i) {
			const char ch = digits[i]; unsigned digit;
			if(ch >= '0' && ch <= '9') digit = ch - '0';
			else if(ch >= 'a' && ch <= 'f') digit = ch - 'a' + 10;
			else if(ch >= 'A' && ch <= 'F') digit = ch - 'A' + 10;
			else return false;
			code = code * 16 + digit;
		}
	} else {
		for(size_t i = 1; i < value.size(); ++i) {
			if(value[i] < '0' || value[i] > '7') return false;
			code = code * 8 + static_cast<unsigned>(value[i] - '0');
		}
	}
	const PA19IntegralType info = PA19Type(type);
	*result = info.is_unsigned ? PA19IntegralValue::Unsigned(code, type, info.bits) :
		PA19IntegralValue::Signed(static_cast<long long>(code), type, info.bits);
	return true;
}

class PA19ConstantExpressionParser
{
public:
	PA19ConstantExpressionParser(const std::map<std::string, PA19IntegralValue>& constants,
		const std::map<std::string, std::string>& substitutions,
		const std::map<std::string, size_t>& type_sizes = std::map<std::string, size_t>(),
		const std::map<std::string, size_t>& type_alignments = std::map<std::string, size_t>(),
		const std::map<std::string, std::string>& type_aliases = std::map<std::string, std::string>())
		: constants_(constants), substitutions_(substitutions), type_sizes_(type_sizes),
		  type_alignments_(type_alignments), type_aliases_(type_aliases), text_(), position_(0) {}

	bool Evaluate(const std::string& text, PA19IntegralValue* result)
	{
		text_ = PA19Trim(text); position_ = 0;
		if(!ParseConditional(result)) return false;
		Skip();
		return position_ == text_.size() && result && result->known;
	}

private:
	const std::map<std::string, PA19IntegralValue>& constants_;
	const std::map<std::string, std::string>& substitutions_;
	const std::map<std::string, size_t>& type_sizes_;
	const std::map<std::string, size_t>& type_alignments_;
	const std::map<std::string, std::string>& type_aliases_;
	std::string text_;
	size_t position_;

	void Skip() { while(position_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[position_]))) ++position_; }
	bool Take(const std::string& token)
	{
		Skip();
		if(text_.compare(position_, token.size(), token) != 0) return false;
		position_ += token.size();
		return true;
	}
	bool IdentifierStart(char ch) const { return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_'; }
	std::string ReadIdentifier()
	{
		Skip();
		const size_t start = position_;
		if(position_ < text_.size() && text_[position_] == ':') {
			if(position_ + 1 >= text_.size() || text_[position_ + 1] != ':') return std::string();
			position_ += 2;
		}
		if(position_ >= text_.size() || !IdentifierStart(text_[position_])) { position_ = start; return std::string(); }
		const size_t first_end = [&]() {
			size_t cursor = position_;
			while(cursor < text_.size() && (IdentifierStart(text_[cursor]) ||
				std::isdigit(static_cast<unsigned char>(text_[cursor])))) ++cursor;
			return cursor;
		}();
		const std::string first_word = text_.substr(start + (start + 1 < first_end &&
			text_[start] == ':' ? 2 : 0), first_end - (start + (start + 1 < first_end &&
			text_[start] == ':' ? 2 : 0)));
		const bool cast_keyword = first_word == "static_cast" || first_word == "const_cast" ||
			first_word == "reinterpret_cast" || first_word == "dynamic_cast";
		while(position_ < text_.size()) {
			while(position_ < text_.size() && (IdentifierStart(text_[position_]) ||
				std::isdigit(static_cast<unsigned char>(text_[position_])))) ++position_;
			Skip();
			if(!cast_keyword && position_ < text_.size() && text_[position_] == '<') {
				const size_t angle_start = position_;
				int depth = 0;
				do {
					if(text_[position_] == '<') ++depth;
					else if(text_[position_] == '>') --depth;
					++position_;
				} while(position_ < text_.size() && depth > 0);
				if(depth != 0) { position_ = angle_start; break; }
			}
			if(position_ + 1 < text_.size() && text_[position_] == ':' && text_[position_ + 1] == ':') {
				position_ += 2; Skip();
				if(position_ >= text_.size() || !IdentifierStart(text_[position_])) { position_ = start; return std::string(); }
				continue;
			}
			break;
		}
		return text_.substr(start, position_ - start);
	}

	int Precedence()
	{
		Skip();
		if(text_.compare(position_, 2, "||") == 0 || text_.compare(position_, 3, "or ") == 0) return 1;
		if(text_.compare(position_, 2, "&&") == 0 || text_.compare(position_, 4, "and ") == 0) return 2;
		if(text_.compare(position_, 1, "|") == 0) return 3;
		if(text_.compare(position_, 1, "^") == 0) return 4;
		if(text_.compare(position_, 1, "&") == 0) return 5;
		if(text_.compare(position_, 2, "==") == 0 || text_.compare(position_, 2, "!=") == 0) return 6;
		if(text_.compare(position_, 2, "<=") == 0 || text_.compare(position_, 2, ">=") == 0) return 7;
		// Shift tokens must win over the single-character relational check.
		if(text_.compare(position_, 2, "<<") == 0 || text_.compare(position_, 2, ">>") == 0) return 8;
		if(text_.compare(position_, 1, "<") == 0 || text_.compare(position_, 1, ">") == 0) return 7;
		if(text_.compare(position_, 1, "+") == 0 || text_.compare(position_, 1, "-") == 0) return 9;
		if(text_.compare(position_, 1, "*") == 0 || text_.compare(position_, 1, "/") == 0 ||
			text_.compare(position_, 1, "%") == 0) return 10;
		return 0;
	}
	std::string OperatorAt(int precedence)
	{
		Skip();
		static const char* const operators[][2] = {
			{"||", "or"}, {"&&", "and"}, {"|", "bitor"}, {"^", "xor"}, {"&", "bitand"},
			{"==", "=="}, {"!=", "!="}, {"<=", "<="}, {">=", ">="}, {"<", "<"}, {">", ">"},
			{"<<", "<<"}, {">>", ">>"}, {"+", "+"}, {"-", "-"}, {"*", "*"}, {"/", "/"}, {"%", "%"}
		};
		static const int levels[] = {1,2,3,4,5,6,6,7,7,7,7,8,8,9,9,10,10,10};
		for(size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); ++i)
			if(levels[i] == precedence) {
				const std::string primary = operators[i][0];
				if(text_.compare(position_, primary.size(), primary) == 0) { position_ += primary.size(); return primary; }
				const std::string keyword = operators[i][1];
				if((keyword == "and" || keyword == "or" || keyword == "bitand" || keyword == "bitor" || keyword == "xor") &&
					text_.compare(position_, keyword.size(), keyword) == 0 &&
					(position_ + keyword.size() == text_.size() || !PA19IdentifierCharacter(text_[position_ + keyword.size()]))) {
					position_ += keyword.size(); return keyword == "and" ? "&&" : keyword == "or" ? "||" : keyword == "bitand" ? "&" : keyword == "bitor" ? "|" : "^";
				}
			}
		return std::string();
	}
	bool ParseConditional(PA19IntegralValue* result)
	{
		if(!ParseBinary(1, result)) return false;
		if(Take("?")) {
			PA19IntegralValue when_true, when_false;
			if(!ParseConditional(&when_true) || !Take(":") || !ParseConditional(&when_false)) return false;
			if(!result->known) *result = PA19IntegralValue();
			else *result = result->raw ? when_true : when_false;
		}
		return true;
	}
	bool ParseBinary(int minimum, PA19IntegralValue* result)
	{
		if(!ParseUnary(result)) return false;
		for(;;) {
			const int precedence = Precedence();
			if(precedence < minimum) return true;
			const std::string operation = OperatorAt(precedence);
			PA19IntegralValue right;
			if(!ParseBinary(precedence + 1, &right)) return false;
			*result = PA19Binary(operation, *result, right);
		}
	}
	bool ParseUnary(PA19IntegralValue* result)
	{
		if(Take("+")) { if(!ParseUnary(result)) return false; *result = PA19Promote(*result); return result->known; }
		if(Take("-")) { if(!ParseUnary(result)) return false; *result = PA19Promote(*result); if(!result->known) return false; const PA19IntegralType t = result->type; const unsigned long long raw = (0ULL - PA19Raw(*result)) & PA19Mask(t.bits); *result = t.is_unsigned ? PA19IntegralValue::Unsigned(raw, t.name, t.bits) : PA19IntegralValue::Signed(static_cast<long long>(raw), t.name, t.bits); return true; }
		if(Take("!")) { if(!ParseUnary(result)) return false; *result = PA19IntegralValue::Signed(!result->raw, "int", 32); return true; }
		if(Take("~")) { if(!ParseUnary(result)) return false; *result = PA19Promote(*result); if(!result->known) return false; const PA19IntegralType t = result->type; const unsigned long long raw = (~PA19Raw(*result)) & PA19Mask(t.bits); *result = t.is_unsigned ? PA19IntegralValue::Unsigned(raw, t.name, t.bits) : PA19IntegralValue::Signed(static_cast<long long>(raw), t.name, t.bits); return true; }
		return ParsePrimary(result);
	}
	bool ParseString(std::vector<unsigned long long>* values, std::string* type)
	{
		Skip();
		const size_t start = position_;
		if(position_ < text_.size() && text_[position_] == 'L') { if(type) *type = "wchar_t"; ++position_; }
		else if(position_ < text_.size() && text_[position_] == 'u') { if(type) *type = "char16_t"; ++position_; }
		else if(position_ < text_.size() && text_[position_] == 'U') { if(type) *type = "char32_t"; ++position_; }
		else if(type) *type = "char";
		if(position_ >= text_.size() || text_[position_] != '"') { position_ = start; return false; }
		++position_;
		while(position_ < text_.size() && text_[position_] != '"') {
			if(text_[position_] == '\\' && position_ + 1 < text_.size()) {
				++position_; const char escaped = text_[position_++];
				switch(escaped) { case '0': values->push_back(0); break; case 'a': values->push_back(7); break; case 'b': values->push_back(8); break; case 'n': values->push_back(10); break; case 'r': values->push_back(13); break; case 't': values->push_back(9); break; case 'v': values->push_back(11); break; default: values->push_back(static_cast<unsigned char>(escaped)); break; }
			} else values->push_back(static_cast<unsigned char>(text_[position_++]));
		}
		if(position_ >= text_.size() || text_[position_] != '"') { position_ = start; return false; }
		++position_;
		values->push_back(0);
		return true;
	}
	bool ParsePrimary(PA19IntegralValue* result)
	{
		if(Take("(")) { if(!ParseConditional(result) || !Take(")")) return false; return true; }
		std::vector<unsigned long long> string_values; std::string string_type;
		const size_t string_start = position_;
		if(ParseString(&string_values, &string_type)) {
			if(Take("[")) { PA19IntegralValue index; if(!ParseConditional(&index) || !Take("]") || !index.known || index.raw >= string_values.size()) return false; const PA19IntegralType type = PA19Type(string_type); *result = type.is_unsigned ? PA19IntegralValue::Unsigned(string_values[static_cast<size_t>(index.raw)], string_type, type.bits) : PA19IntegralValue::Signed(static_cast<long long>(string_values[static_cast<size_t>(index.raw)]), string_type, type.bits); return true; }
			position_ = string_start; return false;
		}
		Skip();
		if(position_ < text_.size() && (text_[position_] == '\'' ||
			((text_[position_] == 'L' || text_[position_] == 'u' || text_[position_] == 'U') &&
			 position_ + 1 < text_.size() && text_[position_ + 1] == '\''))) {
			const size_t start = position_;
			if(text_[position_] != '\'') ++position_;
			++position_;
			while(position_ < text_.size()) {
				if(text_[position_] == '\\') { position_ += position_ + 1 < text_.size() ? 2 : 1; continue; }
				if(text_[position_] == '\'') { ++position_; break; }
				++position_;
			}
			if(PA19DecodeCharacter(text_.substr(start, position_ - start), result)) return true;
			position_ = start;
		}
		if(position_ < text_.size() && (std::isdigit(static_cast<unsigned char>(text_[position_])) || text_[position_] == '.')) {
			const size_t start = position_;
			while(position_ < text_.size() && (std::isalnum(static_cast<unsigned char>(text_[position_])) || text_[position_] == 'x' || text_[position_] == 'X')) ++position_;
			return PA19ParseInteger(text_.substr(start, position_ - start), result);
		}
		std::string name = ReadIdentifier();
		if(name.empty()) return false;
		// The AST can spell a functional cast as `long long int(value)` or
		// `unsigned int(value)`.  ReadIdentifier deliberately stops at spaces,
		// so recover the complete fundamental type here while preserving the
		// cursor when the following words are not part of a cast type.
		for(;;) {
			const size_t before = position_;
			const std::string next = ReadIdentifier();
			if(next.empty()) { position_ = before; break; }
			const std::string candidate = name + " " + next;
			if(!PA19Type(candidate).integral) { position_ = before; break; }
			name = candidate;
		}
		return ParseNamedPrimary(name, result);
	}
	bool ParseNamedPrimary(const std::string& name, PA19IntegralValue* result)
	{
		const std::string bare = name.compare(0, 2, "::") == 0 ? name.substr(2) : name;
		if(bare == "true") { *result = PA19IntegralValue::Signed(1, "bool", 1); return true; }
		if(bare == "false") { *result = PA19IntegralValue::Signed(0, "bool", 1); return true; }
		if(bare == "sizeof" || bare == "alignof" || bare == "__alignof") {
			if(!Take("(")) return false;
			const size_t begin = position_;
			int depth = 1;
			while(position_ < text_.size() && depth) {
				if(text_[position_] == '(') ++depth;
				else if(text_[position_] == ')') --depth;
				++position_;
			}
			if(depth) return false;
			std::string operand = PA19Compact(text_.substr(begin, position_ - begin - 1));
			std::map<std::string,std::string>::const_iterator operand_alias =
				type_aliases_.find(operand);
			if(operand_alias != type_aliases_.end()) operand = operand_alias->second;
			const std::map<std::string,size_t>& table =
				bare == "sizeof" ? type_sizes_ : type_alignments_;
			std::map<std::string,size_t>::const_iterator found = table.find(operand);
			size_t size = found == table.end() ? 0 : found->second;
			if(size == 0) {
				const PA19IntegralType type = PA19Type(operand);
				size = type.integral ? (type.bits <= 8 ? 1 :
					type.bits <= 16 ? 2 : type.bits <= 32 ? 4 : 8) : 0;
			}
			if(size == 0) return false;
			*result = PA19IntegralValue::Unsigned(
				static_cast<unsigned long long>(size), "unsigned long", 64);
			return true;
		}
	// Casts are parsed after the identifier has been read.  A template-id
	// whose value is in the constant table follows the same lookup path.
		if((bare == "static_cast" || bare == "const_cast" || bare == "reinterpret_cast" || bare == "dynamic_cast") && Take("<")) {
			const size_t begin = position_; int depth = 1; while(position_ < text_.size() && depth) { if(text_[position_] == '<') ++depth; else if(text_[position_] == '>') --depth; ++position_; } if(depth || position_ == 0) return false; std::string target = PA19Compact(text_.substr(begin, position_ - begin - 1)); std::map<std::string,std::string>::const_iterator alias = type_aliases_.find(target); if(alias != type_aliases_.end()) target = alias->second; if(!Take("(")) return false; PA19IntegralValue operand; if(!ParseConditional(&operand) || !Take(")")) return false; const PA19IntegralType type = PA19Type(target); *result = PA19Convert(operand, type); return result->known;
		}
		if(Take("(")) {
			PA19IntegralValue operand;
			if(!ParseConditional(&operand) || !Take(")")) return false;
			std::string cast_name = name;
			std::map<std::string,std::string>::const_iterator alias = type_aliases_.find(cast_name);
			if(alias != type_aliases_.end()) cast_name = alias->second;
			const PA19IntegralType type = PA19Type(cast_name);
			if(type.integral) { *result = PA19Convert(operand, type); return true; }
			return false;
		}
		std::string substituted = name;
		for(std::map<std::string,std::string>::const_iterator it = substitutions_.begin(); it != substitutions_.end(); ++it) {
			std::string rebuilt;
			for(size_t i = 0; i < substituted.size();) { if(PA19IdentifierCharacter(substituted[i])) { size_t end = i + 1; while(end < substituted.size() && PA19IdentifierCharacter(substituted[end])) ++end; const std::string word = substituted.substr(i, end - i); rebuilt += word == it->first ? it->second : word; i = end; } else { rebuilt += substituted[i++]; } }
			substituted = rebuilt;
		}
		if(substituted.compare(0, 2, "::") == 0) substituted.erase(0, 2);
		// In the supported metaprogramming subset, conversion of an
		// integral-constant temporary (`B{}`) is its typed `value` member.
		// The expander has already materialized that member by this point.
		if(Take("{")) {
			if(!Take("}")) return false;
			std::map<std::string, PA19IntegralValue>::const_iterator object_value =
				constants_.find(substituted + "::value");
			if(object_value == constants_.end())
				object_value = constants_.find(name + "::value");
			if(object_value == constants_.end()) return false;
			*result = object_value->second;
			return result->known;
		}
		std::map<std::string, PA19IntegralValue>::const_iterator found = constants_.find(substituted);
		// A qualified lookup must not silently resolve to an unrelated
		// unqualified member with the same spelling.  In particular, a
		// generated trait may have a `value` member whose result differs from
		// another integral_constant already in the table.
		if(found == constants_.end() && substituted.find("::") == std::string::npos &&
			name.find("::") == std::string::npos)
			found = constants_.find(name);
		// Keep the legacy unqualified recovery for named constants such as
		// `const_min`; generated trait `::value` members are deliberately
		// excluded so inherited-value resolution can select the correct base.
		if(found == constants_.end()) {
			const size_t separator = substituted.rfind("::");
			if(separator != std::string::npos &&
				substituted.substr(separator + 2) != "value")
				found = constants_.find(substituted.substr(separator + 2));
		}
		if(found == constants_.end()) return false;
		*result = found->second;
		return result->known;
	}
};
