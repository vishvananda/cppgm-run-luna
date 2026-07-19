#include "preprocessor_engine.h"

#include <ctime>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace std;

#include "ctrlexpr.h"
#include "macro_engine.h"

namespace {

typedef pair<unsigned long int, unsigned long int> PA5FileId;

extern "C" long int syscall(long int n, ...) throw ();

bool PA5GetFileId(const string& path, PA5FileId& out_fileid)
{
	struct
	{
		unsigned long int dev;
		unsigned long int ino;
		long int unused[16];
	} data;

	const int result = syscall(4, path.c_str(), &data);
	out_fileid = make_pair(data.dev, data.ino);
	return result == 0;
}

bool IsSpace(const MacroToken& token)
{
	return token.kind == TOKEN_SPACE || token.kind == TOKEN_NEWLINE;
}

bool IsIdentifier(const MacroToken& token)
{
	return token.kind == TOKEN_IDENTIFIER;
}

bool IsPunct(const MacroToken& token, const string& text)
{
	return token.kind == TOKEN_PUNCTUATOR && token.text == text;
}

size_t SkipSpaces(const vector<MacroToken>& tokens, size_t begin, size_t end)
{
	while (begin < end && IsSpace(tokens[begin])) ++begin;
	return begin;
}

string Reconstruct(const vector<MacroToken>& tokens, size_t begin, size_t end)
{
	string result;
	for (size_t i = begin; i < end; ++i) result += tokens[i].text;
	return result;
}

struct Directive
{
	bool present;
	bool null_directive;
	string name;
	size_t name_position;

	Directive() : present(false), null_directive(false), name(),
		name_position(0) {}
};

Directive FindDirective(const vector<MacroToken>& tokens, size_t begin,
	size_t end)
{
	Directive result;
	size_t position = SkipSpaces(tokens, begin, end);
	if (position >= end ||
		!(IsPunct(tokens[position], "#") || IsPunct(tokens[position], "%:")))
		return result;
	position = SkipSpaces(tokens, position + 1, end);
	if (position >= end)
	{
		result.present = true;
		result.null_directive = true;
		return result;
	}
	result.present = true;
	if (IsIdentifier(tokens[position]))
	{
		result.name = tokens[position].text;
		result.name_position = position;
	}
	return result;
}

bool IsConditionalDirective(const string& name)
{
	return name == "if" || name == "ifdef" || name == "ifndef" ||
		name == "elif" || name == "else" || name == "endif";
}

bool IsKnownDirective(const string& name)
{
	return IsConditionalDirective(name) || name == "include" ||
		name == "define" || name == "undef" || name == "line" ||
		name == "error" || name == "pragma";
}

struct ConditionalFrame
{
	bool parent_active;
	bool branch_taken;
	bool active;
	bool saw_else;

	ConditionalFrame(bool parent_active = true)
		: parent_active(parent_active), branch_taken(false), active(false),
		  saw_else(false) {}
};

bool IsActive(const vector<ConditionalFrame>& stack)
{
	return stack.empty() || stack.back().active;
}

string QuoteString(const string& value)
{
	string result = "\"";
	for (size_t i = 0; i < value.size(); ++i)
	{
		if (value[i] == '\\' || value[i] == '"') result.push_back('\\');
		result.push_back(value[i]);
	}
	result += "\"";
	return result;
}

string DecodeStringLiteral(const string& source)
{
	size_t quote = source.find('"');
	if (quote == string::npos || source.size() < quote + 2 ||
		source[source.size() - 1] != '"')
		throw logic_error("invalid string literal");
	string result;
	for (size_t i = quote + 1; i + 1 < source.size(); ++i)
	{
		if (source[i] != '\\')
		{
			result.push_back(source[i]);
			continue;
		}
		if (++i + 1 > source.size()) throw logic_error("invalid string escape");
		const char escaped = source[i];
		switch (escaped)
		{
		case '\\': result.push_back('\\'); break;
		case '"': result.push_back('"'); break;
		case '\'': result.push_back('\''); break;
		case 'n': result.push_back('\n'); break;
		case 'r': result.push_back('\r'); break;
		case 't': result.push_back('\t'); break;
		case 'a': result.push_back('\a'); break;
		case 'b': result.push_back('\b'); break;
		case 'f': result.push_back('\f'); break;
		case 'v': result.push_back('\v'); break;
		case '?': result.push_back('?'); break;
		default:
			if (escaped >= '0' && escaped <= '7')
			{
				int value = escaped - '0';
				for (int count = 1; count < 3 && i + 1 < source.size() &&
					source[i + 1] >= '0' && source[i + 1] <= '7'; ++count)
					value = value * 8 + (source[++i] - '0');
				result.push_back(static_cast<char>(value));
			}
			else if (escaped == 'x')
			{
				int value = 0;
				bool found = false;
				while (i + 1 < source.size())
				{
					const char c = source[i + 1];
					int digit = -1;
					if (c >= '0' && c <= '9') digit = c - '0';
					else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
					else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
					if (digit < 0) break;
					found = true;
					value = value * 16 + digit;
					++i;
				}
				if (!found) throw logic_error("invalid hex string escape");
				result.push_back(static_cast<char>(value));
			}
			else result.push_back(escaped);
		}
	}
	return result;
}

vector<MacroToken> ProtectedControlTokens(const vector<MacroToken>& tokens,
	size_t begin, size_t end, const string& file, int logical_line)
{
	vector<MacroToken> copy(tokens.begin() + begin, tokens.begin() + end);
	for (size_t i = 0; i < copy.size(); ++i)
	{
		copy[i].file = file;
		copy[i].line = logical_line;
	}
	for (size_t i = 0; i < copy.size(); ++i)
	{
		if (!IsIdentifier(copy[i]) || copy[i].text != "defined") continue;
		size_t operand = i + 1;
		while (operand < copy.size() && IsSpace(copy[operand])) ++operand;
		if (operand < copy.size() && IsPunct(copy[operand], "("))
		{
			++operand;
			while (operand < copy.size() && IsSpace(copy[operand])) ++operand;
		}
		if (operand >= copy.size()) continue;
		if (copy[operand].kind != TOKEN_IDENTIFIER &&
			copy[operand].kind != TOKEN_PUNCTUATOR)
			continue;
		copy[operand].protect_from_macro_expansion = true;
	}
	return copy;
}

class Preprocessor
{
public:
	Preprocessor(const string& date, const string& time)
		: date_(date), time_(time), output_(NULL), include_depth_(0)
	{
		DefinePredefined("__CPPGM__", "201303L");
		DefinePredefined("__cplusplus", "201103L");
		DefinePredefined("__STDC_HOSTED__", "1");
		DefinePredefined("__CPPGM_AUTHOR__", QuoteString("Vishvananda Abrams"));
		DefinePredefined("__DATE__", QuoteString(date_));
		DefinePredefined("__TIME__", QuoteString(time_));
	}

	void Process(const string& path, vector<PostPPToken>* output)
	{
		output_ = output;
		ProcessFile(path);
		output_ = NULL;
	}

private:
	MacroState macros_;
	string date_;
	string time_;
	set<PA5FileId> pragma_once_;
	vector<PostPPToken>* output_;
	int include_depth_;
	string line_file_;
	vector<string> saved_line_files_;

	void DefinePredefined(const string& name, const string& value)
	{
		macros_.Define("#define " + name + " " + value, "<predefined>", 1);
	}

	void ProcessFile(const string& path)
	{
		if (++include_depth_ > 256) throw logic_error("include nesting too deep");
		saved_line_files_.push_back(line_file_);
		line_file_ = path;
		ifstream input(path.c_str(), ios::in | ios::binary);
		if (!input)
		{
			--include_depth_;
			throw logic_error("unable to open include file");
		}
		ostringstream contents;
		contents << input.rdbuf();
		input.close();

		const vector<MacroToken> tokens = TokenizeMacroSource(contents.str(), path, 1);
		vector<ConditionalFrame> conditions;
		string text;
		int text_line = 1;
		bool have_text = false;
		int line_delta = 0;

		size_t position = 0;
		while (position < tokens.size())
		{
			size_t line_end = position;
			while (line_end < tokens.size() &&
				tokens[line_end].kind != TOKEN_NEWLINE) ++line_end;
			const int physical_line = line_end < tokens.size() ?
				tokens[line_end].line : (position < tokens.size() ? tokens[position].line : 1);
			const Directive directive = FindDirective(tokens, position, line_end);

			if (!directive.present)
			{
				if (IsActive(conditions))
				{
					if (!have_text)
					{
						text_line = physical_line;
						have_text = true;
					}
					text += Reconstruct(tokens, position, line_end);
					if (line_end < tokens.size()) text += "\n";
				}
				position = line_end < tokens.size() ? line_end + 1 : line_end;
				continue;
			}

			FlushText(&text, &have_text, text_line, line_delta, line_file_);
			HandleDirective(tokens, position, line_end, directive, physical_line,
				&line_delta, &conditions, line_file_);
			position = line_end < tokens.size() ? line_end + 1 : line_end;
		}

		FlushText(&text, &have_text, text_line, line_delta, line_file_);
		if (!conditions.empty()) throw logic_error("file completed with unmatched #if");
		line_file_ = saved_line_files_.back();
		saved_line_files_.pop_back();
		--include_depth_;
	}

	void FlushText(string* text, bool* have_text, int physical_line,
		int line_delta, const string& file)
	{
		if (!*have_text) return;
		vector<PostPPToken> expanded = macros_.Expand(*text, file,
			physical_line + line_delta);
		vector<PostPPToken> filtered = ExecutePragmaOperators(expanded, file);
		output_->insert(output_->end(), filtered.begin(), filtered.end());
		text->clear();
		*have_text = false;
	}

	vector<PostPPToken> ExecutePragmaOperators(
		const vector<PostPPToken>& input, const string& file)
	{
		vector<PostPPToken> result;
		for (size_t i = 0; i < input.size(); ++i)
		{
			if (input[i].kind != POST_PP_IDENTIFIER || input[i].source != "_Pragma")
			{
				result.push_back(input[i]);
				continue;
			}
			if (i + 3 >= input.size() || input[i + 1].source != "(" ||
				input[i + 2].kind != POST_PP_STRING || input[i + 3].source != ")")
				throw logic_error("invalid _Pragma operator");
			ExecutePragmaText(DecodeStringLiteral(input[i + 2].source), file);
			i += 3;
		}
		return result;
	}

	void ExecutePragmaText(const string& text, const string& file)
	{
		const vector<MacroToken> tokens = TokenizeMacroSource(
			"#pragma " + text + "\n", file, 1);
		const size_t end = tokens.size() > 0 ? tokens.size() - 1 : 0;
		const Directive directive = FindDirective(tokens, 0, end);
		if (!directive.present || directive.name != "pragma")
			throw logic_error("invalid _Pragma directive");
		HandlePragma(tokens, directive.name_position + 1, end, file);
	}

	void HandlePragma(const vector<MacroToken>& tokens, size_t begin, size_t end,
		const string& file)
	{
		size_t position = SkipSpaces(tokens, begin, end);
		if (position >= end || !IsIdentifier(tokens[position])) return;
		if (tokens[position].text != "once") return;
		PA5FileId id;
		if (PA5GetFileId(file, id)) pragma_once_.insert(id);
	}

	bool Evaluate(const vector<MacroToken>& tokens, size_t begin, size_t end,
		int logical_line, const string& file)
	{
		const vector<MacroToken> control_tokens = ProtectedControlTokens(
			tokens, begin, end, file, logical_line);
		vector<PostPPToken> expanded = macros_.Expand(control_tokens);
		set<string> defined;
		for (size_t i = 0; i < control_tokens.size(); ++i)
			if (control_tokens[i].protect_from_macro_expansion &&
				macros_.IsDefined(control_tokens[i].text))
				defined.insert(control_tokens[i].text);
		bool value = false;
		if (!EvaluateControlExpression(expanded, defined, &value))
			throw logic_error("error in controlling expression");
		return value;
	}

	void HandleConditional(const vector<MacroToken>& tokens, size_t begin,
		size_t end, const Directive& directive, int physical_line,
		int* line_delta, vector<ConditionalFrame>* conditions,
		const string& file)
	{
		(void)begin;
		const string& name = directive.name;
		if (name == "if" || name == "ifdef" || name == "ifndef")
		{
			const bool parent = IsActive(*conditions);
			ConditionalFrame frame(parent);
			if (parent)
			{
				size_t operand = directive.name_position + 1;
				if (name == "if")
					frame.active = Evaluate(tokens, operand, end,
						physical_line + *line_delta, file);
				else
				{
					operand = SkipSpaces(tokens, operand, end);
					if (operand >= end || !IsIdentifier(tokens[operand]))
						throw logic_error("invalid conditional identifier");
					const string identifier = tokens[operand++].text;
					operand = SkipSpaces(tokens, operand, end);
					if (operand != end) throw logic_error("extra conditional tokens");
					const bool value = macros_.IsDefined(identifier);
					frame.active = name == "ifdef" ? value : !value;
				}
			}
			frame.branch_taken = frame.active;
			conditions->push_back(frame);
			return;
		}

		if (conditions->empty()) throw logic_error("conditional directive without #if");
		ConditionalFrame& frame = conditions->back();
		if (name == "elif")
		{
			if (frame.saw_else) throw logic_error("#elif after #else");
			if (!frame.parent_active || frame.branch_taken)
			{
				frame.active = false;
				return;
			}
			frame.active = Evaluate(tokens, directive.name_position + 1, end,
				physical_line + *line_delta, file);
			if (frame.active) frame.branch_taken = true;
			return;
		}
		if (name == "else")
		{
			if (frame.saw_else) throw logic_error("duplicate #else");
			frame.saw_else = true;
			if (frame.parent_active && frame.branch_taken) frame.active = false;
			else
			{
				frame.active = frame.parent_active;
				frame.branch_taken = true;
			}
			return;
		}
		if (name == "endif")
		{
			conditions->pop_back();
			return;
		}
		throw logic_error("unknown conditional directive");
	}

	void HandleDirective(const vector<MacroToken>& tokens, size_t begin,
		size_t end, const Directive& directive, int physical_line,
		int* line_delta, vector<ConditionalFrame>* conditions,
		const string& file)
	{
		const bool active = IsActive(*conditions);
		if (IsConditionalDirective(directive.name))
		{
			HandleConditional(tokens, begin, end, directive, physical_line,
				line_delta, conditions, file);
			return;
		}
		if (!active || !directive.present) return;
		if (directive.null_directive) return;
		if (directive.name.empty()) throw logic_error("invalid preprocessing directive");
		if (!IsKnownDirective(directive.name)) throw logic_error("unknown preprocessing directive");

		const size_t rest = directive.name_position + 1;
		const int logical_line = physical_line + *line_delta;
		if (directive.name == "define")
		{
			macros_.Define(Reconstruct(tokens, begin, end), file, logical_line);
			return;
		}
		if (directive.name == "undef")
		{
			macros_.Undefine(Reconstruct(tokens, begin, end), file, logical_line);
			return;
		}
		if (directive.name == "error") throw logic_error("#error directive");
		if (directive.name == "pragma")
		{
			HandlePragma(tokens, rest, end, file);
			return;
		}
		if (directive.name == "include")
		{
			HandleInclude(tokens, begin, end, logical_line, file);
			return;
		}
		if (directive.name == "line")
		{
			HandleLine(tokens, begin, end, logical_line, physical_line,
				line_delta, file);
			return;
		}
		throw logic_error("invalid preprocessing directive");
	}

	void HandleLine(const vector<MacroToken>& tokens, size_t begin, size_t end,
		int logical_line, int physical_line, int* line_delta,
		const string& file)
	{
		vector<PostPPToken> expanded = macros_.Expand(
			Reconstruct(tokens, begin, end), file, logical_line);
		if (expanded.size() < 3 || expanded[0].source != "#" ||
			expanded[1].source != "line" || expanded[2].kind != POST_PP_NUMBER)
			throw logic_error("invalid #line directive");
		unsigned long value = 0;
		for (size_t i = 0; i < expanded[2].source.size(); ++i)
		{
			const char c = expanded[2].source[i];
			if (c < '0' || c > '9' || value > 100000000UL)
				throw logic_error("invalid #line number");
			value = value * 10 + static_cast<unsigned long>(c - '0');
		}
		if (value == 0 || value > 2147483647UL)
			throw logic_error("invalid #line number");
		if (expanded.size() == 4)
		{
			if (expanded[3].kind != POST_PP_STRING)
				throw logic_error("invalid #line filename");
			line_file_ = DecodeStringLiteral(expanded[3].source);
		}
		else if (expanded.size() != 3)
			throw logic_error("extra tokens after #line");
		*line_delta = static_cast<int>(value) - physical_line - 1;
	}

	void HandleInclude(const vector<MacroToken>& tokens, size_t begin,
		size_t end, int logical_line, const string& file)
	{
		vector<PostPPToken> expanded = macros_.Expand(
			Reconstruct(tokens, begin, end), file, logical_line);
		if (expanded.size() != 3 || expanded[0].source != "#" ||
			expanded[1].source != "include" ||
			(expanded[2].kind != POST_PP_HEADER && expanded[2].kind != POST_PP_STRING))
			throw logic_error("invalid include directive");
		string next;
		const string source = expanded[2].source;
		if (expanded[2].kind == POST_PP_HEADER)
		{
			if (source.size() < 2) throw logic_error("invalid header name");
			next = source.substr(1, source.size() - 2);
		}
		else next = DecodeStringLiteral(source);

		const string current_file = line_file_.empty() ? file : line_file_;
		string path_relative;
		const size_t slash = current_file.find_last_of('/');
		if (slash != string::npos) path_relative = current_file.substr(0, slash + 1) + next;
		string selected;
		PA5FileId id;
		if (!path_relative.empty() && PA5GetFileId(path_relative, id)) selected = path_relative;
		else if (PA5GetFileId(next, id)) selected = next;
		else throw logic_error("include file not found");
		if (pragma_once_.find(id) != pragma_once_.end()) return;
		ProcessFile(selected);
	}
};

string CurrentDate(const string& stamp)
{
	if (stamp.size() < 24) return string();
	return stamp.substr(4, 7) + " " + stamp.substr(20, 4);
}

string CurrentTime(const string& stamp)
{
	if (stamp.size() < 19) return string();
	return stamp.substr(11, 8);
}

} // namespace

vector<PostPPToken> PreprocessSourceFile(const string& path)
{
	time_t now = time(NULL);
	const char* stamp = asctime(localtime(&now));
	const string build_stamp = stamp == NULL ? string() : string(stamp);
	Preprocessor processor(CurrentDate(build_stamp), CurrentTime(build_stamp));
	vector<PostPPToken> tokens;
	processor.Process(path, &tokens);
	return tokens;
}
