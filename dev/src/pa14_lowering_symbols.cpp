#include "pa14_lowering.h"

#include <cctype>
#include <cstring>
#include <cstdlib>
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

bool abi_split_function_pointer_type(const string& raw, bool* reference,
                                     string* result, vector<string>* parameters)
{
  const string value = abi_trim(raw);
  size_t marker = value.find("(*");
  bool is_reference = false;
  if(marker == string::npos) {
    marker = value.find("(&");
    is_reference = marker != string::npos;
  }
  if(marker == string::npos) return false;
  const size_t declarator_close = value.find(')', marker + 2);
  if(declarator_close == string::npos || declarator_close + 1 >= value.size() ||
     value[declarator_close + 1] != '(') return false;
  const size_t parameter_open = declarator_close + 1;
  int depth = 0;
  size_t parameter_close = string::npos;
  for(size_t position = parameter_open; position < value.size(); ++position) {
    if(value[position] == '(') ++depth;
    else if(value[position] == ')' && --depth == 0) {
      parameter_close = position;
      break;
    }
  }
  if(parameter_close == string::npos ||
     !abi_trim(value.substr(parameter_close + 1)).empty()) return false;
  const string return_type = abi_trim(value.substr(0, marker));
  if(return_type.empty()) return false;
  if(reference) *reference = is_reference;
  if(result) *result = return_type;
  if(parameters) {
    *parameters = abi_split_arguments(value.substr(parameter_open + 1,
      parameter_close - parameter_open - 1));
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
	const string template_marker = "__pa18_abi_template_";
	if(value.compare(0, template_marker.size(), template_marker) == 0 &&
		value.size() > template_marker.size() + 2 &&
		value.compare(value.size() - 2, 2, "__") == 0) {
		const string digits = value.substr(template_marker.size(),
			value.size() - template_marker.size() - 2);
		bool numeric = !digits.empty();
		for(size_t i = 0; numeric && i < digits.size(); ++i)
			if(!isdigit(static_cast<unsigned char>(digits[i]))) numeric = false;
		if(numeric) {
			const size_t index = static_cast<size_t>(strtoul(digits.c_str(), 0, 10));
			return index == 0 ? "T_" : "T" + integer_text(
				static_cast<long long>(index - 1)) + "_";
		}
	}
	// A boolean non-type template argument is encoded as a typed literal in
	// the Itanium ABI.  PA18 keeps the source spelling (`true`/`false`) in
	// typed compiler state, so normalize it at the ABI boundary instead of
	// treating it as an identifier.
	if(value == "true") return "Lb1E";
	if(value == "false") return "Lb0E";
	string function_result;
	vector<string> function_parameters;
	bool function_reference = false;
	if(abi_split_function_pointer_type(value, &function_reference,
		&function_result, &function_parameters)) {
		string encoded = function_reference ? "R" : "P";
		encoded += "F" + abi_type_text(function_result);
		for(size_t parameter = 0; parameter < function_parameters.size(); ++parameter)
			encoded += abi_type_text(function_parameters[parameter]);
		return encoded + "E";
	}
	if(abi_split_direct_function_type(value, &function_result, &function_parameters)) {
		string encoded = "F" + abi_type_text(function_result);
		for(size_t parameter = 0; parameter < function_parameters.size(); ++parameter)
			encoded += abi_type_text(function_parameters[parameter]);
		return encoded + "E";
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
		if(!type->template_parameter_packs.empty()) {
			size_t argument = 0;
			for(size_t parameter = 0; parameter <
				type->template_parameter_packs.size(); ++parameter) {
				if(!type->template_parameter_packs[parameter]) {
					if(argument < type->template_arguments.size())
						final += abi_type_text(type->template_arguments[argument++]);
					continue;
				}
				const size_t trailing_fixed = type->template_parameter_packs.size() -
					parameter - 1;
				const size_t available = type->template_arguments.size() - argument;
				const size_t count = available > trailing_fixed ?
					available - trailing_fixed : 0;
				if(count != 0) {
					final += "J";
					for(size_t item = 0; item < count; ++item)
						final += abi_type_text(type->template_arguments[argument++]);
					final += "E";
				}
			}
			while(argument < type->template_arguments.size())
				final += abi_type_text(type->template_arguments[argument++]);
		} else for(size_t i = 0; i < type->template_arguments.size(); ++i)
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
    result += abi_type(raw->child);
    for(size_t i = 0; i < raw->parameters.size(); ++i) result += abi_type(raw->parameters[i]);
    if(raw->variadic) result += "z";
    return result + "E";
  }
  case TYPE_MEMBER_POINTER:
    return cv + "M" + abi_type(raw->member_owner) + abi_type(raw->child);
  case TYPE_CLASS:
    return cv + abi_type_components(raw);
  default: return cv + abi_qualified(raw->name);
  }
}

bool abi_pattern_mentions_parameter(const string& raw, const string& wanted);
bool abi_function_has_parameter_pack(const CPPGMAstNodePtr& node);

struct AbiMangleContext
{
  vector<string> substitutions;

  string substitution(const string& key) const
  {
    for(size_t index = 0; index < substitutions.size(); ++index)
      if(substitutions[index] == key) {
        if(index < 10) return "S" + integer_text(static_cast<long long>(index)) + "_";
        return "S" + string(1, static_cast<char>('A' + index - 10)) + "_";
      }
    return string();
  }

  void remember(const string& key)
  {
    if(key.empty() || !substitution(key).empty()) return;
    substitutions.push_back(key);
  }

  string encode_text(const string& raw,
                     const vector<bool>& parameter_packs = vector<bool>())
  {
    string value = abi_trim(raw);
    if(value.empty()) return "v";
    if(value == "true" || value == "false") return abi_type_text(value);
    const string existing = substitution("text:" + value);
    if(!existing.empty()) return existing;

    string function_result;
    vector<string> function_parameters;
    bool function_reference = false;
    if(abi_split_function_pointer_type(value, &function_reference,
        &function_result, &function_parameters)) {
      string encoded = function_reference ? "R" : "P";
      encoded += "F" + encode_text(function_result);
      for(size_t parameter = 0; parameter < function_parameters.size(); ++parameter)
        encoded += encode_text(function_parameters[parameter]);
      encoded += "E";
      remember("text:" + value);
      return encoded;
    }
    if(abi_split_direct_function_type(value, &function_result, &function_parameters)) {
      string encoded = "F" + encode_text(function_result);
      for(size_t parameter = 0; parameter < function_parameters.size(); ++parameter)
        encoded += encode_text(function_parameters[parameter]);
      encoded += "E";
      remember("text:" + value);
      return encoded;
    }

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
    if(!trailing_cv.empty()) return trailing_cv + encode_text(value);
    if(value[value.size() - 1] == '&') {
      const bool rvalue = value.size() > 1 && value[value.size() - 2] == '&';
      value.erase(value.size() - (rvalue ? 2 : 1));
      const string encoded = string(rvalue ? "O" : "R") + encode_text(value);
      remember("text:" + abi_trim(raw));
      return encoded;
    }
    if(value[value.size() - 1] == '*') {
      value.erase(value.size() - 1);
      const string encoded = "P" + encode_text(value);
      remember("text:" + abi_trim(raw));
      return encoded;
    }
    if(value.compare(0, 6, "const ") == 0)
      return "K" + encode_text(value.substr(6));
    if(value.compare(0, 9, "volatile ") == 0)
      return "V" + encode_text(value.substr(9));
    if(!abi_fundamental(value).empty()) return abi_fundamental(value);

    const vector<string> components = abi_split_qualified(abi_remove_tag(value));
    if(components.size() > 1) {
      string encoded = "N";
      for(size_t component = 0; component < components.size(); ++component)
        encoded += encode_component(components[component], vector<bool>());
      encoded += "E";
      remember("text:" + abi_trim(raw));
      return encoded;
    }
    return encode_component(components.empty() ? value : components[0],
      parameter_packs);
  }

  string encode_component(const string& raw, const vector<bool>& parameter_packs)
  {
    string value = abi_remove_tag(abi_trim(raw));
    const size_t open = value.find('<');
    if(open == string::npos) {
      const string key = "name:" + value;
      const string existing = substitution(key);
      if(!existing.empty()) return existing;
      const string encoded = integer_text(static_cast<long long>(value.size())) + value;
      remember(key);
      return encoded;
    }
    const size_t close = abi_matching_angle(value, open);
    if(close == string::npos) return abi_component(value);
    const string base = abi_trim(value.substr(0, open));
    const string base_key = "name:" + base;
    const string existing = substitution(base_key);
    string encoded = existing.empty() ?
      integer_text(static_cast<long long>(base.size())) + base : existing;
    if(existing.empty()) remember(base_key);
    encoded += "I";
    const vector<string> arguments = abi_split_arguments(value.substr(open + 1,
      close - open - 1));
    encode_arguments(&encoded, arguments, parameter_packs);
    encoded += "E";
    remember("text:" + value);
    return encoded;
  }

  void encode_arguments(string* output, const vector<string>& arguments,
                        const vector<bool>& parameter_packs)
  {
    size_t argument = 0;
    if(parameter_packs.empty()) {
      while(argument < arguments.size()) *output += encode_text(arguments[argument++]);
      return;
    }
    for(size_t parameter = 0; parameter < parameter_packs.size(); ++parameter) {
      if(!parameter_packs[parameter]) {
        if(argument < arguments.size()) *output += encode_text(arguments[argument++]);
        continue;
      }
      size_t trailing_fixed = 0;
      for(size_t later = parameter + 1; later < parameter_packs.size(); ++later)
        if(!parameter_packs[later]) ++trailing_fixed;
      const size_t available = arguments.size() - argument;
      const size_t count = available > trailing_fixed ? available - trailing_fixed : 0;
      if(count != 0) {
        *output += "J";
        for(size_t item = 0; item < count; ++item)
          *output += encode_text(arguments[argument++]);
        *output += "E";
      }
    }
    while(argument < arguments.size()) *output += encode_text(arguments[argument++]);
  }

  string type_key(const TypePtr& type) const
  {
    if(!type) return string();
    string key = "type:" + integer_text(static_cast<long long>(type->kind)) + ":" +
      type->name + ":" + type->template_primary;
    if(type->enclosing_type) key += ":<" + type_key(type->enclosing_type) + ">";
    for(size_t argument = 0; argument < type->template_arguments.size(); ++argument)
      key += ":" + type->template_arguments[argument];
    if(type->kind == TYPE_POINTER || type->kind == TYPE_LVALUE_REFERENCE ||
       type->kind == TYPE_RVALUE_REFERENCE) key += ":<" + type_key(type->child) + ">";
    return key;
  }

  string encode_type(const TypePtr& type, bool defer_full = false)
  {
    if(!type) return "v";
    if(type->kind == TYPE_FUNDAMENTAL) return abi_fundamental(type->name);
    if(type->kind == TYPE_POINTER || type->kind == TYPE_LVALUE_REFERENCE ||
       type->kind == TYPE_RVALUE_REFERENCE) {
      const string key = type_key(type);
      const string existing = substitution(key);
      if(!existing.empty()) return existing;
      const string prefix = type->kind == TYPE_POINTER ? "P" :
        (type->kind == TYPE_LVALUE_REFERENCE ? "R" : "O");
      const string encoded = prefix + encode_type(type->child);
      remember(key);
      return encoded;
    }
    if(type->kind == TYPE_FUNCTION) {
      const string key = type_key(type);
      const string existing = substitution(key);
      if(!existing.empty()) return existing;
      string encoded = "F" + encode_type(type->child);
      for(size_t parameter = 0; parameter < type->parameters.size(); ++parameter)
        encoded += encode_type(type->parameters[parameter]);
      if(type->variadic) encoded += "z";
      encoded += "E";
      remember(key);
      return encoded;
    }
    if(type->kind != TYPE_CLASS) return encode_text(type->name);
    const string base = type->template_primary.empty() ?
      abi_last_component(type->name) : abi_last_component(type->template_primary);
    const bool plain_class = !type->enclosing_type &&
      !type->template_specialization && type->template_arguments.empty();
    const string key = plain_class ? "name:" + base : type_key(type);
    const string existing = substitution(key);
    if(!existing.empty()) return existing;
    string encoded;
    if(type->enclosing_type) {
      const string enclosing_key = type_key(type->enclosing_type);
      encoded = "N" + encode_type(type->enclosing_type, true);
      const string name = abi_last_component(type->name);
      encoded += integer_text(static_cast<long long>(name.size())) + name + "E";
      remember("name:" + name);
      remember(key);
      remember(enclosing_key);
    } else {
      encoded = integer_text(static_cast<long long>(base.size())) + base;
      const string base_key = "name:" + base;
      if(substitution(base_key).empty()) remember(base_key);
      if(type->template_specialization || !type->template_arguments.empty()) {
        encoded += "I";
        encode_arguments(&encoded, type->template_arguments,
          type->template_parameter_packs);
        encoded += "E";
      }
		if(!defer_full && !plain_class) remember(key);
    }
    return encoded;
  }

  string encode_function_arguments(const CPPGMAstNodePtr& node,
                                   const vector<string>& arguments,
                                   const TypePtr& source)
  {
    if(!node || !abi_function_has_parameter_pack(node)) {
      string result;
      for(size_t argument = 0; argument < arguments.size(); ++argument)
        result += encode_text(arguments[argument]);
      return result;
    }
    const vector<string>& names = node->template_function_parameter_names;
    const vector<bool>& packs = node->template_function_parameter_packs;
    vector<TypePtr> typed(arguments.size());
    size_t actual = 0;
    if(source && node->template_function_patterns.size() == source->parameters.size() + 1)
      for(size_t parameter = 0; parameter < names.size() && actual < arguments.size();
          ++parameter) {
        size_t trailing_fixed = 0;
        for(size_t later = parameter + 1; later < packs.size(); ++later)
          if(!packs[later]) ++trailing_fixed;
        const size_t available = arguments.size() - actual;
        const size_t count = packs[parameter] ?
          (available > trailing_fixed ? available - trailing_fixed : 0) : 1;
        if(!packs[parameter] && parameter == 0 && source->child)
          typed[actual] = type_value(source->child);
        else if(packs[parameter]) {
          size_t matched = 0;
          for(size_t item = 0; item < source->parameters.size() && matched < count; ++item)
            if(abi_pattern_mentions_parameter(node->template_function_patterns[item + 1],
                names[parameter])) typed[actual + matched++] =
              type_value(source->parameters[item]);
        }
        actual += count;
      }
    string result;
    actual = 0;
    for(size_t parameter = 0; parameter < names.size(); ++parameter) {
      size_t trailing_fixed = 0;
      for(size_t later = parameter + 1; later < packs.size(); ++later)
        if(!packs[later]) ++trailing_fixed;
      const size_t available = arguments.size() - actual;
      const size_t count = packs[parameter] ?
        (available > trailing_fixed ? available - trailing_fixed : 0) : 1;
      if(packs[parameter]) {
        result += "J";
        for(size_t item = 0; item < count; ++item) {
          result += typed[actual] ? encode_type(typed[actual]) : encode_text(arguments[actual]);
          ++actual;
        }
        result += "E";
      } else if(actual < arguments.size()) {
        result += typed[actual] ? encode_type(typed[actual]) : encode_text(arguments[actual]);
        ++actual;
      }
    }
    while(actual < arguments.size())
      result += typed[actual] ? encode_type(typed[actual]) : encode_text(arguments[actual++]);
    return result;
  }
};

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

string abi_mark_template_pattern(string raw,
                                 const vector<string>& parameter_names)
{
  for(size_t position = 0; position < raw.size();) {
    if(!isalpha(static_cast<unsigned char>(raw[position])) &&
       raw[position] != '_') {
      ++position;
      continue;
    }
    const size_t begin = position++;
    while(position < raw.size() &&
          (isalnum(static_cast<unsigned char>(raw[position])) ||
           raw[position] == '_')) ++position;
    const string word = raw.substr(begin, position - begin);
    size_t parameter = 0;
    for(; parameter < parameter_names.size(); ++parameter)
      if(!parameter_names[parameter].empty() &&
         parameter_names[parameter] == word) break;
    if(parameter == parameter_names.size()) continue;
    const string marker = "__pa18_abi_template_" + integer_text(
      static_cast<long long>(parameter)) + "__";
    raw.replace(begin, word.size(), marker);
    position = begin + marker.size();
  }
  return raw;
}

string abi_template_pattern_type(const string& pattern,
                                 const vector<string>& parameter_names)
{
  return abi_type_text(abi_mark_template_pattern(pattern, parameter_names));
}

bool abi_pattern_mentions_parameter(const string& raw, const string& wanted)
{
  if(wanted.empty()) return false;
  for(size_t position = 0; position < raw.size();) {
    if(!isalpha(static_cast<unsigned char>(raw[position])) && raw[position] != '_') {
      ++position;
      continue;
    }
    const size_t begin = position++;
    while(position < raw.size() &&
          (isalnum(static_cast<unsigned char>(raw[position])) || raw[position] == '_'))
      ++position;
    if(raw.substr(begin, position - begin) == wanted) return true;
  }
  return false;
}

bool abi_function_has_parameter_pack(const CPPGMAstNodePtr& node)
{
  if(!node || node->template_function_parameter_names.empty() ||
     node->template_function_parameter_names.size() !=
       node->template_function_parameter_packs.size()) return false;
  for(size_t parameter = 0; parameter <
      node->template_function_parameter_packs.size(); ++parameter)
    if(node->template_function_parameter_packs[parameter]) return true;
  return false;
}

string abi_function_template_arguments(const CPPGMAstNodePtr& node,
                                       const vector<string>& arguments,
                                       const TypePtr& source)
{
	if(!node || !abi_function_has_parameter_pack(node)) {
		string result;
		for(size_t argument = 0; argument < arguments.size(); ++argument)
			result += abi_type_text(arguments[argument]);
		return result;
	}
	const vector<string>& names = node->template_function_parameter_names;
	const vector<bool>& packs = node->template_function_parameter_packs;
	vector<TypePtr> typed(arguments.size());
	if(source && node->template_function_patterns.size() == source->parameters.size() + 1) {
		size_t actual = 0;
		for(size_t parameter = 0; parameter < names.size() && actual < arguments.size();
			++parameter) {
			size_t trailing_fixed = 0;
			for(size_t later = parameter + 1; later < packs.size(); ++later)
				if(!packs[later]) ++trailing_fixed;
			const size_t available = arguments.size() - actual;
			const size_t count = packs[parameter] ?
				(available > trailing_fixed ? available - trailing_fixed : 0) : 1;
			if(!packs[parameter] && parameter == 0 && source->child)
				typed[actual] = type_value(source->child);
			else if(packs[parameter]) {
				size_t matched = 0;
				for(size_t item = 0; item < source->parameters.size() &&
					matched < count; ++item) {
					const string& pattern = node->template_function_patterns[item + 1];
					if(!abi_pattern_mentions_parameter(pattern, names[parameter])) continue;
					typed[actual + matched++] = type_value(source->parameters[item]);
				}
			}
			actual += count;
		}
	}
	const auto encode_argument = [&](size_t index) {
		return (index < typed.size() && typed[index]) ? abi_type(typed[index]) :
			abi_type_text(arguments[index]);
	};
	string result;
	size_t argument = 0;
	for(size_t parameter = 0; parameter < names.size(); ++parameter) {
	if(!packs[parameter]) {
			if(argument < arguments.size())
				result += encode_argument(argument++);
			continue;
		}
    size_t trailing_fixed = 0;
    for(size_t later = parameter + 1; later < packs.size(); ++later)
      if(!packs[later]) ++trailing_fixed;
		const size_t available = arguments.size() - argument;
		const size_t count = available > trailing_fixed ? available - trailing_fixed : 0;
	result += "J";
	for(size_t item = 0; item < count; ++item)
			result += encode_argument(argument++);
		result += "E";
	}
	while(argument < arguments.size())
		result += encode_argument(argument++);
	return result;
}

string abi_function_template_type(const CPPGMAstNodePtr& node,
                                  const TypePtr& source)
{
	if(!node || !abi_function_has_parameter_pack(node) ||
		node->template_function_patterns.size() !=
		  source->parameters.size() + 1) return string();
	const vector<string>& names = node->template_function_parameter_names;
	const vector<bool>& packs = node->template_function_parameter_packs;
	const vector<string>& patterns = node->template_function_patterns;
  string result = abi_template_pattern_type(patterns[0], names);
  for(size_t parameter = 0; parameter < source->parameters.size(); ++parameter) {
    bool expansion = false;
    for(size_t template_parameter = 0; template_parameter < packs.size();
        ++template_parameter)
      if(packs[template_parameter] && abi_pattern_mentions_parameter(
           patterns[parameter + 1], names[template_parameter])) {
        expansion = true;
        break;
      }
	if(expansion) result += "Dp";
    result += abi_template_pattern_type(patterns[parameter + 1], names);
  }
  return result;
}

} // namespace

string template_type_mangled_name(const TypePtr& type)
{
	return abi_type_components(type);
}

string template_type_mangled_name_with_substitutions(const TypePtr& type)
{
  if(!type) return "1X";
  AbiMangleContext context;
  return context.encode_type(type);
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
  if(!global.explicit_specialization && global.node &&
     !global.node->template_primary.empty() &&
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

string PA14Lowerer::RepeatedTemplateFunctionObjectName(const FunctionRecord& function) const
{
	if(function.member || function.member_owner || function.template_arguments.empty()) return string();
	bool repeated = false;
	for(size_t argument = 0; argument < function.template_arguments.size(); ++argument)
		for(size_t prior = 0; prior < argument; ++prior)
			if(function.template_arguments[argument] == function.template_arguments[prior]) repeated = true;
	const string primary = function.template_primary.empty() && function.node ?
		function.node->template_primary : function.template_primary;
	if(!repeated || primary.find("::") != string::npos || !function.node ||
		function.node->template_function_patterns.empty() ||
		function.node->template_function_parameter_names.empty()) return string();
	const vector<string> components = abi_split_qualified(primary);
	if(components.empty()) return string();
	string result = "_Z";
	if(components.size() > 1) result += "N";
	for(size_t component = 0; component < components.size(); ++component)
		result += abi_component(components[component]);
	result += "I";
	for(size_t argument = 0; argument < function.template_arguments.size(); ++argument)
		result += abi_type_text(function.template_arguments[argument]);
	result += "E";
	for(size_t pattern = 0; pattern < function.node->template_function_patterns.size(); ++pattern)
		result += abi_template_pattern_type(function.node->template_function_patterns[pattern],
			function.node->template_function_parameter_names);
	if(components.size() > 1) result += "E";
	// Dependent expression patterns need the full ABI expression encoder.  Do
	// not let their source spelling leak into LowIR object metadata; the
	// ordinary template path still provides a valid fallback for those cases.
	for(size_t i = 0; i < result.size(); ++i)
		if(!isalnum(static_cast<unsigned char>(result[i])) && result[i] != '_') return string();
	return result;
}

string PA14Lowerer::TemplateFunctionObjectName(const FunctionRecord& function) const
{
  if((!function.template_instantiation && !function.inline_definition) ||
     !function.source_type) return string();
  const TypePtr source = function_target_type(function.source_type);
  if(!source || source->kind != TYPE_FUNCTION) return string();
	const bool nested = function.member || (function.hidden_friend && function.member_owner);
	string result = "_Z";
	const string repeated_name = RepeatedTemplateFunctionObjectName(function);
	if(!repeated_name.empty()) return repeated_name;
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
    const bool contextual_member_template = function.member_template &&
      abi_function_has_parameter_pack(function.node);
    AbiMangleContext abi_context;
    string owner = contextual_member_template ?
      abi_context.encode_type(function.member_owner) :
      abi_nested_body(function.member_owner);
    if(owner.size() >= 2 && owner[0] == 'N' && owner[owner.size() - 1] == 'E')
      owner = owner.substr(1, owner.size() - 2);
    if(!contextual_member_template && function.member_template && function.member_owner &&
       function.member_owner->template_specialization &&
       function.member_owner->template_empty_pack) {
      const size_t close = owner.rfind('E');
      if(close != string::npos) owner.insert(close, "JE");
    }
    result += owner; result += terminal;
    if(function.member_template && !function.template_arguments.empty()) {
      result += "I";
		if(contextual_member_template) result += abi_context.encode_function_arguments(
			function.node, function.template_arguments, source);
		else result += abi_function_template_arguments(function.node,
			function.template_arguments, source);
      result += "E";
	}
	result += "E";
	if(function.member_template && !source->parameters.empty()) {
		const string source_template_type = abi_function_template_type(function.node,
			source);
		if(!source_template_type.empty()) result += source_template_type;
		else {
			result += abi_type(source->child);
			result += "T_";
			for(size_t i = 1; i < source->parameters.size(); ++i)
				result += abi_type(source->parameters[i]);
		}
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
  bool lambda_template_abi = false;
  if(function.template_instantiation && function.node &&
     function.node->source_token_begin != static_cast<size_t>(-1) &&
     function.node->source_token_end != static_cast<size_t>(-1))
    for(map<string, CPPGMAstNodePtr>::const_iterator lambda = lambda_closure_nodes_.begin();
        lambda != lambda_closure_nodes_.end(); ++lambda)
      if(lambda->second && lambda->second->source_token_begin >=
           function.node->source_token_begin &&
         lambda->second->source_token_end <= function.node->source_token_end) {
        lambda_template_abi = true;
        break;
      }
  const string abi_function_name = lambda_template_abi &&
    !function.template_primary.empty() ? function.template_primary : function.qualified_name;
  const vector<string> components = abi_split_qualified(abi_function_name);
  if(components.empty()) return string();
  if(components.size() > 1) result += "N";
  for(size_t i = 0; i + 1 < components.size(); ++i) result += abi_component(components[i]);
  result += abi_terminal(components.back(), source->child);
  if(function.template_instantiation && !function.template_arguments.empty()) {
    result += "I";
    for(size_t i = 0; i < function.template_arguments.size(); ++i)
      result += abi_type_text(function.template_arguments[i]);
    result += "E";
    if(!function.constructor && !function.destructor) {
      if(lambda_template_abi && function.node &&
         function.node->template_function_patterns.size() ==
         source->parameters.size() + 1) {
        for(size_t pattern = 0; pattern < function.node->template_function_patterns.size();
            ++pattern)
          result += abi_template_pattern_type(function.node->template_function_patterns[pattern],
            function.node->template_function_parameter_names);
      } else {
        result += abi_type(source->child);
      }
    }
    if(lambda_template_abi && function.node &&
       function.node->template_function_patterns.size() ==
       source->parameters.size() + 1) {
      if(components.size() > 1) result += "E";
      return result;
    }
  }
  if(components.size() > 1) result += "E";
  return result + abi_function_parameters(source);
}

void PA14Lowerer::FinalizeSymbols()
{
  map<string, unsigned int> overloads;
  for(size_t i = 0; i < functions_.size(); ++i) {
    FunctionRecord& function = functions_[i];
    // A member defined in a class is implicitly inline.  Most records carry
    // that fact from declaration collection; lambda-containing definitions
    // are materialized through the PA18 replay path, so recover this one
    // linkage fact from the durable closure spans without recursively walking
    // every function body.
    if(function.member && !function.inline_definition && !IsLambdaOperator(function) &&
       function.node && function.node->source_token_begin != static_cast<size_t>(-1) &&
       function.node->source_token_end != static_cast<size_t>(-1))
      for(map<string, CPPGMAstNodePtr>::const_iterator lambda = lambda_closure_nodes_.begin();
          lambda != lambda_closure_nodes_.end(); ++lambda)
        if(lambda->second && lambda->second->source_token_begin >=
             function.node->source_token_begin &&
           lambda->second->source_token_end <= function.node->source_token_end) {
          function.inline_definition = true;
          break;
        }
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
