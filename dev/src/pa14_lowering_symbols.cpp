#include "pa14_lowering.h"

#include <cctype>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace std;

namespace cppgm_pa14_lowering {

namespace {

string abi_trim(string value)
{
  size_t begin = 0;
  while(begin < value.size() && isspace(static_cast<unsigned char>(value[begin]))) ++begin;
  size_t end = value.size();
  while(end > begin && isspace(static_cast<unsigned char>(value[end - 1]))) --end;
  return value.substr(begin, end - begin);
}

string abi_remove_tag(string value)
{
  value = abi_trim(value);
  const char* tags[] = {"struct ", "class ", "union ", "enum "};
  for(size_t i = 0; i < sizeof(tags) / sizeof(tags[0]); ++i)
    if(value.compare(0, strlen(tags[i]), tags[i]) == 0)
      return abi_trim(value.substr(strlen(tags[i])));
  return value;
}

string abi_last_component(const string& value)
{
  const size_t separator = value.rfind("::");
  return separator == string::npos ? value : value.substr(separator + 2);
}

size_t abi_matching_angle(const string& value, size_t open)
{
  int depth = 0;
  for(size_t i = open; i < value.size(); ++i) {
    if(value[i] == '<') ++depth;
    else if(value[i] == '>' && --depth == 0) return i;
  }
  return string::npos;
}

vector<string> abi_split_arguments(const string& value)
{
  vector<string> result;
  string current;
  int angle = 0;
  int parentheses = 0;
  int brackets = 0;
  for(size_t i = 0; i < value.size(); ++i) {
    const char ch = value[i];
    if(ch == '<') ++angle;
    else if(ch == '>' && angle > 0) --angle;
    else if(ch == '(') ++parentheses;
    else if(ch == ')' && parentheses > 0) --parentheses;
    else if(ch == '[') ++brackets;
    else if(ch == ']' && brackets > 0) --brackets;
    if(ch == ',' && angle == 0 && parentheses == 0 && brackets == 0) {
      result.push_back(abi_trim(current));
      current.clear();
    } else current += ch;
  }
  if(!current.empty() || !result.empty()) result.push_back(abi_trim(current));
  return result;
}

vector<string> abi_split_qualified(const string& value)
{
  vector<string> result;
  string current;
  int angle = 0;
  for(size_t i = 0; i < value.size(); ++i) {
    const char ch = value[i];
    if(ch == '<') ++angle;
    else if(ch == '>' && angle > 0) --angle;
    if(ch == ':' && i + 1 < value.size() && value[i + 1] == ':' && angle == 0) {
      if(!current.empty()) result.push_back(abi_trim(current));
      current.clear();
      ++i;
    } else current += ch;
  }
  if(!current.empty()) result.push_back(abi_trim(current));
  return result;
}

string abi_fundamental(const string& raw)
{
  const string name = trim_type_name(abi_remove_tag(raw));
  if(name == "void") return "v";
  if(name == "bool") return "b";
  if(name == "char") return "c";
  if(name == "signed char") return "a";
  if(name == "unsigned char") return "h";
  if(name == "short" || name == "short int" || name == "signed short" ||
     name == "signed short int") return "s";
  if(name == "unsigned short" || name == "unsigned short int") return "t";
  if(name == "int" || name == "signed" || name == "signed int") return "i";
  if(name == "unsigned" || name == "unsigned int") return "j";
  if(name == "long" || name == "long int" || name == "signed long" ||
     name == "signed long int") return "l";
  if(name == "unsigned long" || name == "unsigned long int") return "m";
  if(name == "long long" || name == "long long int" || name == "signed long long" ||
     name == "signed long long int") return "x";
  if(name == "unsigned long long" || name == "unsigned long long int") return "y";
  if(name == "float") return "f";
  if(name == "double") return "d";
  if(name == "long double") return "e";
  if(name == "wchar_t") return "w";
  if(name == "char16_t") return "Ds";
  if(name == "char32_t") return "Di";
  if(name == "nullptr_t") return "Dn";
  return string();
}

string abi_type_text(const string& raw);

bool abi_split_direct_function_type(const string& raw, string* result,
                                    vector<string>* parameters)
{
  const string value = abi_trim(raw);
  int angle = 0;
  size_t open = string::npos;
  for(size_t position = 0; position < value.size(); ++position) {
    if(value[position] == '<') ++angle;
    else if(value[position] == '>' && angle > 0) --angle;
    else if(value[position] == '(' && angle == 0) {
      open = position;
      break;
    }
  }
  if(open == string::npos) return false;
  const string prefix = abi_trim(value.substr(0, open));
  if(prefix.empty() || prefix.find("(*") != string::npos ||
     prefix.find("(&") != string::npos) return false;
  int depth = 0;
  size_t close = string::npos;
  for(size_t position = open; position < value.size(); ++position) {
    if(value[position] == '(') ++depth;
    else if(value[position] == ')' && --depth == 0) {
      close = position;
      break;
    }
  }
  if(close == string::npos || !abi_trim(value.substr(close + 1)).empty()) return false;
  if(result) *result = prefix;
  if(parameters) {
    *parameters = abi_split_arguments(value.substr(open + 1, close - open - 1));
    if(parameters->size() == 1 && (*parameters)[0] == "void") parameters->clear();
  }
  return true;
}

string abi_component(const string& raw)
{
  string value = abi_remove_tag(raw);
  const size_t open = value.find('<');
  if(open == string::npos) return integer_text(static_cast<long long>(value.size())) + value;
  const size_t close = abi_matching_angle(value, open);
  if(close == string::npos) return integer_text(static_cast<long long>(value.size())) + value;
  const string base = abi_trim(value.substr(0, open));
  string result = integer_text(static_cast<long long>(base.size())) + base + "I";
  const vector<string> arguments = abi_split_arguments(value.substr(open + 1, close - open - 1));
  for(size_t i = 0; i < arguments.size(); ++i) result += abi_type_text(arguments[i]);
  return result + "E";
}

string abi_qualified(const string& raw)
{
  string value = abi_remove_tag(raw);
  while(!value.empty() && value[0] == ':') value.erase(value.begin());
  const vector<string> components = abi_split_qualified(value);
  if(components.empty()) return "1X";
  if(components.size() == 1) return abi_component(components[0]);
  string result = "N";
  for(size_t i = 0; i < components.size(); ++i) result += abi_component(components[i]);
  return result + "E";
}

string abi_type_text(const string& raw)
{
  string value = abi_trim(raw);
  if(value.empty()) return "v";
	// A boolean non-type template argument is encoded as a typed literal in
	// the Itanium ABI.  PA18 keeps the source spelling (`true`/`false`) in
	// typed compiler state, so normalize it at the ABI boundary instead of
	// treating it as an identifier.
	if(value == "true") return "Lb1E";
	if(value == "false") return "Lb0E";
	string function_result;
	vector<string> function_parameters;
	if(abi_split_direct_function_type(value, &function_result, &function_parameters)) {
		string encoded = "F";
		for(size_t parameter = 0; parameter < function_parameters.size(); ++parameter)
			encoded += abi_type_text(function_parameters[parameter]);
		return encoded + "E" + abi_type_text(function_result);
	}
	// PA19 preserves an enum non-type argument as `EnumType value` so the
	// source type remains available for specialization names.  In the ABI it
	// is a typed non-type literal, not a named type containing a space.
	const size_t typed_value_separator = value.rfind(' ');
	if(typed_value_separator != string::npos && typed_value_separator > 0) {
		const string enum_type = abi_trim(value.substr(0, typed_value_separator));
		string enum_value = abi_trim(value.substr(typed_value_separator + 1));
		const bool signed_value = !enum_value.empty() &&
			enum_value[0] == '-';
		const size_t first_digit = (!enum_value.empty() &&
			(enum_value[0] == '-' || enum_value[0] == '+')) ? 1 : 0;
		bool numeric_value = first_digit < enum_value.size();
		for(size_t i = first_digit; numeric_value && i < enum_value.size(); ++i)
			if(!isdigit(static_cast<unsigned char>(enum_value[i]))) numeric_value = false;
		if(numeric_value) {
			while(!enum_value.empty() && (enum_value[enum_value.size() - 1] == 'u' ||
				enum_value[enum_value.size() - 1] == 'U' ||
				enum_value[enum_value.size() - 1] == 'l' ||
				enum_value[enum_value.size() - 1] == 'L')) enum_value.erase(enum_value.size() - 1);
			if(!enum_value.empty()) {
				const string enum_encoding = abi_fundamental(enum_type).empty() ?
					abi_qualified(enum_type) : abi_fundamental(enum_type);
				return "L" + enum_encoding +
					(signed_value ? "n" : "") + enum_value.substr(first_digit) + "E";
			}
		}
	}
	// Non-type integral template arguments use the Itanium literal encoding;
	// treating a digit as an identifier produces `I1818` instead of `ILi8ELi8E`.
  if((value[0] == '-' || value[0] == '+' ||
		isdigit(static_cast<unsigned char>(value[0]))) &&
		(value[0] == '-' || value[0] == '+' ||
		isdigit(static_cast<unsigned char>(value[0])))) {
		size_t first_digit = value[0] == '-' || value[0] == '+' ? 1 : 0;
		if(first_digit < value.size() &&
			isdigit(static_cast<unsigned char>(value[first_digit]))) {
			string number = value.substr(first_digit);
			while(!number.empty() && (number[number.size() - 1] == 'u' ||
				number[number.size() - 1] == 'U' || number[number.size() - 1] == 'l' ||
				number[number.size() - 1] == 'L')) number.erase(number.size() - 1);
			if(!number.empty()) return string("Li") +
				(value[0] == '-' ? "n" : "") + number + "E";
		}
	}
	// Type spelling can place cv-qualifiers after a pointer (`T* volatile`)
	// even though the ABI encoding places the qualifier before the pointer.
	// Strip trailing qualifiers before decomposing the pointer, otherwise the
	// raw source spelling is mistaken for a named type and leaks spaces into
	// the object symbol.
	string trailing_cv;
	for(;;) {
		if(value.size() > 6 && value.compare(value.size() - 6, 6, " const") == 0) {
			value.erase(value.size() - 6);
			trailing_cv = "K" + trailing_cv;
		} else if(value.size() > 9 &&
			value.compare(value.size() - 9, 9, " volatile") == 0) {
			value.erase(value.size() - 9);
			trailing_cv = "V" + trailing_cv;
		} else break;
	}
	if(!trailing_cv.empty()) return trailing_cv + abi_type_text(value);
  if(value[value.size() - 1] == '&') {
    const bool rvalue = value.size() > 1 && value[value.size() - 2] == '&';
    value.erase(value.size() - (rvalue ? 2 : 1));
    return string(rvalue ? "O" : "R") + abi_type_text(value);
  }
  if(value[value.size() - 1] == '*') {
    value.erase(value.size() - 1);
    return "P" + abi_type_text(value);
  }
  if(value[value.size() - 1] == ']') {
    const size_t open = value.rfind('[');
    if(open != string::npos) {
      const string bound = abi_trim(value.substr(open + 1,
        value.size() - open - 2));
      return "A" + bound + "_" + abi_type_text(value.substr(0, open));
    }
  }
  if(value.compare(0, 6, "const ") == 0)
    return "K" + abi_type_text(value.substr(6));
  if(value.compare(0, 9, "volatile ") == 0)
    return "V" + abi_type_text(value.substr(9));
  const string fundamental = abi_fundamental(value);
  if(!fundamental.empty()) return fundamental;
  return abi_qualified(value);
}

string abi_type_components(const TypePtr& type)
{
  vector<string> components;
  if(!type) return "1X";
  string qualified = type->owned_scope && !type->owned_scope->qualified_prefix.empty() ?
    type->owned_scope->qualified_prefix : type->name;
  if(type->enclosing_type) {
    const string enclosing = abi_type_components(type->enclosing_type);
    if(enclosing.size() >= 2 && enclosing[0] == 'N' && enclosing[enclosing.size() - 1] == 'E')
      qualified = enclosing.substr(1, enclosing.size() - 2) + abi_component(abi_last_component(qualified));
    else qualified = enclosing + abi_component(abi_last_component(qualified));
    return "N" + qualified + "E";
  }
  if(type->template_specialization && !type->template_primary.empty()) {
    const vector<string> primary = abi_split_qualified(type->template_primary);
    for(size_t i = 0; i + 1 < primary.size(); ++i) components.push_back(abi_component(primary[i]));
    const string base = primary.empty() ? abi_last_component(qualified) : primary.back();
    string final = integer_text(static_cast<long long>(base.size())) + base + "I";
    for(size_t i = 0; i < type->template_arguments.size(); ++i)
      final += abi_type_text(type->template_arguments[i]);
    components.push_back(final + "E");
  } else {
    const vector<string> names = abi_split_qualified(qualified);
    for(size_t i = 0; i < names.size(); ++i) components.push_back(abi_component(names[i]));
  }
  string result;
  for(size_t i = 0; i < components.size(); ++i) result += components[i];
  return result;
}

string abi_nested_body(const TypePtr& type)
{
  string encoded = abi_type_components(type);
  if(encoded.size() >= 2 && encoded[0] == 'N' && encoded[encoded.size() - 1] == 'E')
    encoded = encoded.substr(1, encoded.size() - 2);
  return encoded;
}

string abi_type(const TypePtr& raw)
{
  if(!raw) return "v";
  string cv;
  if(raw->is_const) cv += "K";
  if(raw->is_volatile) cv += "V";
  switch(raw->kind) {
  case TYPE_FUNDAMENTAL: return cv + abi_fundamental(raw->name);
  case TYPE_ENUM: return cv + abi_qualified(raw->name);
  case TYPE_POINTER: return cv + "P" + abi_type(raw->child);
  case TYPE_LVALUE_REFERENCE: return cv + "R" + abi_type(raw->child);
  case TYPE_RVALUE_REFERENCE: return cv + "O" + abi_type(raw->child);
  case TYPE_ARRAY:
    return cv + "A" + (raw->bound < 0 ? string() : integer_text(raw->bound)) + "_" +
      abi_type(raw->child);
  case TYPE_FUNCTION: {
    string result = cv + "F";
    for(size_t i = 0; i < raw->parameters.size(); ++i) result += abi_type(raw->parameters[i]);
    if(raw->variadic) result += "z";
    return result + "E" + abi_type(raw->child);
  }
  case TYPE_MEMBER_POINTER:
    return cv + "M" + abi_type(raw->member_owner) + abi_type(raw->child);
  case TYPE_CLASS:
    return cv + abi_type_components(raw);
  default: return cv + abi_qualified(raw->name);
  }
}

string abi_terminal(const string& name, const TypePtr& result)
{
  static const char* const names[][2] = {
    {"operator=", "aS"}, {"operator+", "pl"}, {"operator+=", "pL"},
    {"operator-", "mi"}, {"operator-=", "mI"},
    {"operator*", "ml"}, {"operator/", "dv"}, {"operator%", "rm"},
    {"operator<<", "ls"}, {"operator>>", "rs"}, {"operator<", "lt"},
    {"operator>", "gt"}, {"operator<=", "le"}, {"operator>=", "ge"},
    {"operator==", "eq"}, {"operator!=", "ne"}, {"operator&", "an"},
    {"operator|", "or"}, {"operator^", "eo"}, {"operator&&", "aa"},
    {"operator||", "oo"}, {"operator!", "nt"}, {"operator~", "co"},
    {"operator++", "pp"}, {"operator--", "mm"}, {"operator->", "pt"},
    {"operator->*", "pm"}, {"operator,", "cm"}, {"operator[]", "ix"},
    {"operator()", "cl"}, {"operator new", "nw"}, {"operatornew", "nw"},
    {"operator new[]", "na"}, {"operatornew[]", "na"},
    {"operator delete", "dl"}, {"operatordelete", "dl"},
    {"operator delete[]", "da"}, {"operatordelete[]", "da"}
  };
  for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    if(name == names[i][0]) return names[i][1];
  if(name.compare(0, 8, "operator") == 0 && name.size() > 8)
    return "cv" + abi_type(result);
  return integer_text(static_cast<long long>(name.size())) + name;
}

size_t abi_named_type_count(const string& raw)
{
  string value = abi_trim(raw);
  const char* qualifiers[] = {"const ", "volatile ", "struct ", "class ",
    "union ", "enum "};
  bool removed = true;
  while(removed) {
    removed = false;
    for(size_t i = 0; i < sizeof(qualifiers) / sizeof(qualifiers[0]); ++i)
      if(value.compare(0, strlen(qualifiers[i]), qualifiers[i]) == 0) {
        value = abi_trim(value.substr(strlen(qualifiers[i])));
        removed = true;
        break;
      }
  }
  if(value.empty() || !abi_fundamental(value).empty()) return 0;
  if(value[value.size() - 1] == '&')
    return abi_named_type_count(value.substr(0, value.size() - 1));
  if(value[value.size() - 1] == '*')
    return abi_named_type_count(value.substr(0, value.size() - 1));
  const size_t open = value.find('<');
  if(open == string::npos) return 1;
  const size_t close = abi_matching_angle(value, open);
  if(close == string::npos) return 1;
  size_t result = 1;
  const vector<string> arguments = abi_split_arguments(value.substr(open + 1,
    close - open - 1));
  for(size_t i = 0; i < arguments.size(); ++i)
    result += abi_named_type_count(arguments[i]);
  return result;
}

string abi_generated_fundamental_name(string raw)
{
  while(!raw.empty() && raw[raw.size() - 1] == '_') raw.erase(raw.size() - 1);
  if(raw == "bool" || raw == "char" || raw == "signed_char" ||
     raw == "unsigned_char" || raw == "short" || raw == "short_int" ||
     raw == "unsigned_short" || raw == "unsigned_short_int" ||
     raw == "int" || raw == "signed_int" || raw == "unsigned" ||
     raw == "unsigned_int" || raw == "long" || raw == "long_int" ||
     raw == "unsigned_long" || raw == "unsigned_long_int" ||
     raw == "long_long" || raw == "long_long_int" ||
     raw == "unsigned_long_long" || raw == "unsigned_long_long_int" ||
     raw == "float" || raw == "double" || raw == "long_double" ||
     raw == "char16_t" || raw == "char32_t" || raw == "wchar_t" ||
     raw == "nullptr_t") {
    for(size_t i = 0; i < raw.size(); ++i)
      if(raw[i] == '_') raw[i] = ' ';
    if(raw == "unsigned") raw = "unsigned int";
    if(raw == "short") raw = "short int";
    if(raw == "long") raw = "long int";
    if(raw == "signed char") return "c";
    if(raw == "unsigned char") return "h";
    return abi_fundamental(raw);
  }
  return string();
}

string abi_cross_template_specialization(const TypePtr& raw, const TypePtr& owner)
{
  const TypePtr value = type_value(raw);
  const TypePtr owner_value = type_value(owner);
  if(!value || !owner_value || value->kind != TYPE_CLASS ||
     owner_value->kind != TYPE_CLASS || !owner_value->template_specialization ||
     owner_value->template_primary.empty()) return string();
  const string owner_primary = abi_last_component(owner_value->template_primary);
  if(value->template_specialization && !value->template_primary.empty() &&
     abi_last_component(value->template_primary) == owner_primary &&
     value->template_arguments.size() != owner_value->template_arguments.size())
    return string();
  if(value->template_specialization && !value->template_primary.empty() &&
     abi_last_component(value->template_primary) == owner_primary) {
    bool same_arguments = value->template_arguments.size() ==
      owner_value->template_arguments.size();
    for(size_t argument = 0; same_arguments &&
        argument < value->template_arguments.size(); ++argument)
      if(abi_trim(value->template_arguments[argument]) !=
         abi_trim(owner_value->template_arguments[argument])) same_arguments = false;
    if(!same_arguments) {
      string result = "S_I";
      for(size_t argument = 0; argument < value->template_arguments.size(); ++argument)
        result += abi_type_text(value->template_arguments[argument]);
      return result + "E";
    }
  }
  const string raw_name = abi_last_component(value->name);
  const string prefix = owner_primary + "_";
  if(raw_name.compare(0, prefix.size(), prefix) != 0) return string();
  const string argument = abi_generated_fundamental_name(raw_name.substr(prefix.size()));
  return argument.empty() ? string() : "S_I" + argument + "E";
}

bool abi_template_substitution_index(const TypePtr& raw, const TypePtr& owner,
                                     size_t* index)
{
  if(!raw || !owner || !owner->template_specialization ||
     owner->template_arguments.empty() || !index) return false;
  const TypePtr value = type_value(raw);
  if(value && (value->kind == TYPE_FUNDAMENTAL || value->kind == TYPE_ENUM)) return false;
  const string raw_type = abi_type(raw);
  string unqualified_raw = raw_type;
  while(!unqualified_raw.empty() &&
        (unqualified_raw[0] == 'K' || unqualified_raw[0] == 'V'))
    unqualified_raw.erase(unqualified_raw.begin());
  size_t next = 0;
  for(size_t i = 0; i < owner->template_arguments.size(); ++i) {
    const string argument = abi_trim(owner->template_arguments[i]);
    const string argument_type = abi_type_text(argument);
    const size_t nested = abi_named_type_count(argument);
    if(raw_type == argument_type || unqualified_raw == argument_type) {
      // Template-id arguments encode their nested named arguments before
      // the outer specialization; a plain named argument has no such
      // prefix.  The matched argument therefore starts after nested-1
      // substitutions, while the enclosing sequence advances by all of
      // them.
      *index = next + (nested == 0 ? 0 : nested - 1);
      return true;
    }
    next += nested;
  }
  const TypePtr owner_value = type_value(owner);
  if(owner_value && PA12SameType(type_value(raw), owner_value, true)) {
    *index = next;
    return true;
  }
  return false;
}

string abi_substitution(size_t index)
{
  return "S" + integer_text(static_cast<long long>(index)) + "_";
}

string abi_member_parameter_type(const TypePtr& raw, const TypePtr& owner)
{
  if(!raw) return "v";
  string cv;
  if(raw->is_const) cv += "K";
  if(raw->is_volatile) cv += "V";
  if(raw->kind == TYPE_LVALUE_REFERENCE || raw->kind == TYPE_RVALUE_REFERENCE) {
    size_t substitution = 0;
    const string reference = raw->kind == TYPE_LVALUE_REFERENCE ? "R" : "O";
    const string cross_specialization = abi_cross_template_specialization(raw->child, owner);
    if(!cross_specialization.empty())
      return cv + reference +
        string(raw->child && raw->child->is_const ? "K" : "") +
        (raw->child && raw->child->is_volatile ? "V" : "") +
        cross_specialization;
    if(abi_template_substitution_index(type_value(raw->child), owner,
                                       &substitution))
      return cv + reference +
        string(raw->child && raw->child->is_const ? "K" : "") +
        (raw->child && raw->child->is_volatile ? "V" : "") +
        abi_substitution(substitution);
    return cv + reference + abi_member_parameter_type(raw->child, owner);
  }
  if(raw->kind == TYPE_POINTER) {
    size_t substitution = 0;
    const string cross_specialization = abi_cross_template_specialization(raw->child, owner);
    if(!cross_specialization.empty()) return cv + "P" + cross_specialization;
    if(abi_template_substitution_index(type_value(raw->child), owner,
                                       &substitution))
      return cv + "P" + abi_substitution(substitution);
    return cv + "P" + abi_member_parameter_type(raw->child, owner);
  }
  const string cross_specialization = abi_cross_template_specialization(raw, owner);
  if(!cross_specialization.empty()) return cv + cross_specialization;
  size_t substitution = 0;
  if(abi_template_substitution_index(raw, owner, &substitution))
    return cv + abi_substitution(substitution);
  return abi_type(raw);
}

string abi_function_parameters(const TypePtr& source, const TypePtr& owner = TypePtr())
{
  if(!source || source->parameters.empty()) return "v";
  string result;
  for(size_t i = 0; i < source->parameters.size(); ++i)
    result += owner ? abi_member_parameter_type(source->parameters[i], owner) :
      abi_type(source->parameters[i]);
  return result;
}

} // namespace

string template_type_mangled_name(const TypePtr& type)
{
	return abi_type_components(type);
}

string PA14Lowerer::TemplateGlobalObjectName(const GlobalRecord& global) const
{
  if(!global.template_instantiation) return string();
  const size_t separator = global.qualified_name.rfind("::");
  const string generated_member = separator == string::npos ? global.qualified_name :
    global.qualified_name.substr(separator + 2);
  string member = generated_member;
  string owner;
  if(global.template_owner) owner = abi_nested_body(global.template_owner);
  else if(separator != string::npos) {
    const vector<string> components = abi_split_qualified(
      global.qualified_name.substr(0, separator));
    for(size_t i = 0; i < components.size(); ++i) owner += abi_component(components[i]);
  }
  if(owner.empty()) return string();
  if(global.node && !global.node->template_primary.empty() &&
     !global.node->template_arguments.empty()) {
    const string primary_member = abi_last_component(global.node->template_primary);
    if(generated_member == primary_member ||
       generated_member.compare(0, primary_member.size() + 1,
         primary_member + "_") == 0) {
      member = primary_member;
      string result = "_ZN" + owner +
        integer_text(static_cast<long long>(member.size())) + member + "I";
      for(size_t argument = 0; argument < global.node->template_arguments.size(); ++argument) {
        const string raw = abi_trim(global.node->template_arguments[argument]);
        const size_t raw_open = raw.find('<');
        const string raw_base = raw_open == string::npos ? raw : raw.substr(0, raw_open);
        const string owner_primary = global.template_owner &&
          !global.template_owner->template_primary.empty() ?
          abi_last_component(global.template_owner->template_primary) : string();
        const string owner_name = global.template_owner ?
          abi_last_component(global.template_owner->name) : string();
        if(global.template_owner && !owner_primary.empty() &&
           (raw == owner_primary || raw == owner_name || raw_base == owner_primary ||
            raw_base == owner_name || abi_last_component(raw) == owner_primary ||
            abi_last_component(raw) == owner_name)) {
          result += "S_I";
          for(size_t owner_argument = 0; owner_argument <
              global.template_owner->template_arguments.size(); ++owner_argument)
            result += abi_type_text(global.template_owner->template_arguments[owner_argument]);
          result += "E";
        } else result += abi_type_text(raw);
      }
      return result + "EE";
    }
  }
  return "_ZN" + owner + integer_text(static_cast<long long>(member.size())) + member + "E";
}

string PA14Lowerer::TemplateFunctionObjectName(const FunctionRecord& function) const
{
  if((!function.template_instantiation && !function.inline_definition) ||
     !function.source_type) return string();
  const TypePtr source = function_target_type(function.source_type);
  if(!source || source->kind != TYPE_FUNCTION) return string();
  const bool nested = function.member || (function.hidden_friend && function.member_owner);
  string result = "_Z";
  string terminal;
  if(function.constructor)
    terminal = function.base_entry ? "C2" : "C1";
  else if(function.destructor)
    terminal = function.deleting_entry ? "D0" : (function.base_entry ? "D2" : "D1");
  else {
    string member_name = abi_last_component(function.qualified_name);
    if(function.member_template && !function.template_primary.empty())
      member_name = abi_last_component(function.template_primary);
    terminal = abi_terminal(member_name, source->child);
  }

  if(nested) {
    result += "N";
    if(source->function_const) result += "K";
    if(source->function_volatile) result += "V";
    string owner = abi_nested_body(function.member_owner);
    // A class template with an empty parameter pack still carries the
    // Itanium ABI pack marker in a nested member-template name.  The typed
    // specialization model has the concrete owner arguments but elides that
    // empty pack; retain the marker for the generated function entity.
    if(function.member_template && function.member_owner &&
       function.member_owner->template_specialization &&
       function.member_owner->template_empty_pack) {
      const size_t close = owner.rfind('E');
      if(close != string::npos) owner.insert(close, "JE");
    }
    result += owner;
    result += terminal;
    if(function.member_template && !function.template_arguments.empty()) {
      result += "I";
      for(size_t i = 0; i < function.template_arguments.size(); ++i)
        result += abi_type_text(function.template_arguments[i]);
      result += "E";
    }
    result += "E";
    if(function.member_template && !source->parameters.empty()) {
      // The concrete argument types are retained for lowering, while the
      // mangled member-template signature refers to its original template
      // parameters.
      result += abi_type(source->child);
      result += "T_";
      for(size_t i = 1; i < source->parameters.size(); ++i)
        result += abi_type(source->parameters[i]);
    } else {
      if(function.member_template && source->parameters.empty() && source->child &&
         type_value(source->child) && type_value(source->child)->kind == TYPE_CLASS) {
        size_t substitution = 0;
        const bool returns_owner = function.member_owner &&
          PA12SameType(type_value(source->child), type_value(function.member_owner), true);
        if(returns_owner)
          result += abi_substitution(0);
        else if(abi_template_substitution_index(type_value(source->child),
                                                 function.member_owner, &substitution))
          result += abi_substitution(substitution);
      }
      result += abi_function_parameters(source, function.member_owner);
    }
    return result;
  }

  const vector<string> components = abi_split_qualified(function.qualified_name);
  if(components.empty()) return string();
  if(components.size() > 1) result += "N";
  for(size_t i = 0; i + 1 < components.size(); ++i) result += abi_component(components[i]);
  result += abi_terminal(components.back(), source->child);
    if(function.template_instantiation && !function.template_arguments.empty()) {
    result += "I";
    for(size_t i = 0; i < function.template_arguments.size(); ++i)
      result += abi_type_text(function.template_arguments[i]);
    result += "E";
    if(!function.constructor && !function.destructor) result += abi_type(source->child);
  }
  if(components.size() > 1) result += "E";
  return result + abi_function_parameters(source);
}

void PA14Lowerer::FinalizeSymbols()
{
  map<string, unsigned int> overloads;
  for(size_t i = 0; i < functions_.size(); ++i) {
    FunctionRecord& function = functions_[i];
    BuildFunctionABI(function);
    string base = low_symbol_component(function.qualified_name);
    unsigned int& count = overloads[base];
    ++count;
    if(!function.builtin || function.symbol.empty())
      function.symbol = count == 1 ? base : base + "__ov" + integer_text(count);
    // The ordinary source spelling has no separator (`operatornew`) in
    // the AST.  Preserve the ABI names for user-provided placement
    // allocation functions so calls remain ordinary typed calls while the
    // emitted object metadata still describes the C++ runtime entry point.
    const TypePtr function_type_value = function_target_type(function.source_type);
    const bool operator_new = LastComponent(function.qualified_name) == "operatornew";
    const bool operator_delete = LastComponent(function.qualified_name) == "operatordelete";
    if(!function.builtin && function_type_value &&
       (operator_new || operator_delete) && function_type_value->parameters.size() == 2) {
      const TypePtr second = type_value(function_type_value->parameters[1]);
      if(operator_new && second && second->kind == TYPE_POINTER &&
         type_value(second->child) && type_value(second->child)->kind == TYPE_FUNDAMENTAL &&
         type_value(second->child)->name == "void")
        function.object_name = "_ZnwmPv";
      else if(operator_delete && second && second->kind == TYPE_POINTER &&
              type_value(second->child) && type_value(second->child)->kind == TYPE_FUNDAMENTAL &&
              type_value(second->child)->name == "void")
        function.object_name = "_ZdlPvS_";
    }
	if(function.template_instantiation || function.inline_definition) {
		if(!function.extern_template) function.weak_binding = true;
      if(function.object_name.empty()) function.object_name = TemplateFunctionObjectName(function);
    }
  }
  for(size_t i = 0; i < globals_.size(); ++i) {
    globals_[i].symbol = low_symbol_component(globals_[i].qualified_name);
    if(globals_[i].template_instantiation) {
      globals_[i].weak_binding = true;
      if(globals_[i].object_name.empty()) globals_[i].object_name = TemplateGlobalObjectName(globals_[i]);
    }
  }
}

} // namespace cppgm_pa14_lowering
