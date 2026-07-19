// PA13 LowIR text parser, validator, and CY86 adapter.

#include "exceptions.h"
#include "tool_help_text.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace std;

namespace {

struct LowError : runtime_error
{
  explicit LowError(const string & message) : runtime_error(message) {}
};

static string trim(const string & source)
{
  size_t first = 0;
  while(first < source.size() && isspace(static_cast<unsigned char>(source[first]))) ++first;
  size_t last = source.size();
  while(last > first && isspace(static_cast<unsigned char>(source[last - 1]))) --last;
  return source.substr(first, last - first);
}

static string strip_comment(const string & source)
{
  bool quote = false;
  for(size_t i = 0; i < source.size(); ++i) {
    if(source[i] == '"') quote = !quote;
    if(!quote && source[i] == '#') return source.substr(0, i);
    if(!quote && source[i] == '/' && i + 1 < source.size() && source[i + 1] == '/') {
      return source.substr(0, i);
    }
  }
  return source;
}

static vector<string> lex_line(string source)
{
  // Debug locations are transport metadata.  The PA13 adapter does not emit
  // object debug information, but removing the suffix here keeps the actual
  // LowIR statement parser independent of its punctuation.
  const size_t dbg = source.find("!dbg");
  if(dbg != string::npos) source.erase(dbg);

  vector<string> result;
  for(size_t i = 0; i < source.size();) {
    if(isspace(static_cast<unsigned char>(source[i]))) { ++i; continue; }
    if(source[i] == '-' && i + 1 < source.size() && source[i + 1] == '>') {
      result.push_back("->"); i += 2; continue;
    }
    const string punctuation = "[](),:{}=+<>";
    if(punctuation.find(source[i]) != string::npos || source[i] == '-') {
      result.push_back(string(1, source[i++]));
      continue;
    }
    size_t start = i;
    while(i < source.size() &&
          !isspace(static_cast<unsigned char>(source[i])) &&
          punctuation.find(source[i]) == string::npos && source[i] != '-') ++i;
    if(start == i) { result.push_back(string(1, source[i++])); }
    else result.push_back(source.substr(start, i - start));
  }
  return result;
}

struct Cursor
{
  vector<string> tokens;
  size_t position;

  explicit Cursor(const vector<string> & input) : tokens(input), position(0) {}

  bool empty() const { return position >= tokens.size(); }
  string peek(size_t offset = 0) const
  {
    return position + offset < tokens.size() ? tokens[position + offset] : string();
  }
  string take()
  {
    if(empty()) throw LowError("unexpected end of LowIR statement");
    return tokens[position++];
  }
  void expect(const string & token)
  {
    if(take() != token) throw LowError("unexpected LowIR token");
  }
  bool take_if(const string & token)
  {
    if(peek() == token) { ++position; return true; }
    return false;
  }
};

static bool starts_with(const string & value, const string & prefix)
{
  return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

struct Type
{
  string text;
  size_t object_bytes;
  size_t object_alignment;

  Type() : text(), object_bytes(0), object_alignment(0) {}
  explicit Type(const string & value) : text(value), object_bytes(0), object_alignment(0) {}

  bool operator==(const Type & other) const { return text == other.text; }
  bool operator!=(const Type & other) const { return !(*this == other); }
  bool is_void() const { return text == "void"; }
  bool is_float() const { return text == "f32" || text == "f64" || text == "f80"; }
  bool is_f80() const { return text == "f80"; }
  bool is_pointer() const { return text == "ptr"; }
  bool is_object() const { return starts_with(text, "obj<"); }
  bool is_integer() const
  {
    return text == "i1" || text == "i8" || text == "u8" ||
           text == "i16" || text == "u16" || text == "i32" ||
           text == "u32" || text == "i64" || text == "u64";
  }
  int width() const
  {
    if(text == "i1") return 1;
    if(text == "i8" || text == "u8") return 8;
    if(text == "i16" || text == "u16") return 16;
    if(text == "i32" || text == "u32") return 32;
    if(text == "i64" || text == "u64" || text == "ptr") return 64;
    if(text == "f32") return 32;
    if(text == "f64") return 64;
    if(text == "f80") return 80;
    return 0;
  }
  size_t storage_size() const
  {
    if(is_void()) return 0;
    if(is_f80()) return 16;
    if(is_object()) return object_bytes;
    if(width() <= 8) return 1;
    if(width() <= 16) return 2;
    if(width() <= 32) return 4;
    return 8;
  }
  size_t storage_alignment() const
  {
    if(is_object()) return object_alignment;
    if(is_f80()) return 8;
    return storage_size() == 0 ? 1 : min<size_t>(storage_size(), 8);
  }
};

static Type parse_type(Cursor & cursor)
{
  const string first = cursor.take();
  if(first == "obj") {
    cursor.expect("<");
    const string span = cursor.take();
    cursor.expect(">");
    const size_t x = span.find('x');
    if(x == string::npos || x == 0 || x + 1 == span.size()) throw LowError("invalid object type");
    char * end = NULL;
    const unsigned long bytes = strtoul(span.substr(0, x).c_str(), &end, 10);
    if(!end || *end != '\0') throw LowError("invalid object type");
    const unsigned long alignment = strtoul(span.substr(x + 1).c_str(), &end, 10);
    if(!end || *end != '\0' || bytes == 0 || alignment == 0) throw LowError("invalid object type");
    Type result("obj<" + span + ">");
    result.object_bytes = bytes;
    result.object_alignment = alignment;
    return result;
  }
  return Type(first);
}

struct Value
{
  enum Kind { TEMP, SLOT, GLOBAL, BLOCK, INTEGER, FLOAT, NULLPTR, UNKNOWN } kind;
  string text;

  Value() : kind(UNKNOWN), text() {}
  Value(Kind value_kind, const string & value_text) : kind(value_kind), text(value_text) {}
};

static bool looks_float(const string & value)
{
  return value.find('.') != string::npos || value.find('e') != string::npos ||
         value.find('E') != string::npos || (!value.empty() &&
         (value[value.size() - 1] == 'f' || value[value.size() - 1] == 'F' ||
          value[value.size() - 1] == 'L' || value[value.size() - 1] == 'l'));
}

static Value parse_literal_or_value(Cursor & cursor)
{
  bool negative = cursor.take_if("-");
  const string token = cursor.take();
  if(token == "nullptr") return Value(Value::NULLPTR, "0");
  if(!token.empty() && token[0] == '%') return Value(Value::TEMP, token.substr(1));
  if(!token.empty() && token[0] == '$') return Value(Value::SLOT, token.substr(1));
  if(!token.empty() && token[0] == '@') return Value(Value::GLOBAL, token.substr(1));
  if(!token.empty() && token[0] == '^') return Value(Value::BLOCK, token.substr(1));
  if(negative) {
    if(looks_float(token)) return Value(Value::FLOAT, "-" + token);
    return Value(Value::INTEGER, "-" + token);
  }
  if(looks_float(token)) return Value(Value::FLOAT, token);
  char * end = NULL;
  errno = 0;
  (void)strtoll(token.c_str(), &end, 0);
  if(end && *end == '\0' && errno != ERANGE) return Value(Value::INTEGER, token);
  return Value(Value::UNKNOWN, token);
}

static Value parse_named_value(Cursor & cursor)
{
  return parse_literal_or_value(cursor);
}

typedef map<string, string> Metadata;

static Metadata parse_metadata(Cursor & cursor)
{
  Metadata result;
  if(!cursor.take_if("[")) return result;
  while(true) {
    const string key = cursor.take();
    cursor.expect("=");
    const string value = cursor.take();
    if(result.find(key) != result.end()) throw LowError("duplicate metadata");
    result[key] = value;
    if(cursor.take_if("]")) break;
    cursor.expect(",");
  }
  return result;
}

struct Parameter
{
  string name;
  Type type;
  Metadata metadata;
};

static vector<Parameter> parse_parameters(Cursor & cursor)
{
  vector<Parameter> result;
  cursor.expect("(");
  if(cursor.take_if(")")) return result;
  while(true) {
    const string name = cursor.take();
    if(name.empty() || name[0] != '%') throw LowError("expected parameter");
    cursor.expect(":");
    Parameter parameter;
    parameter.name = name.substr(1);
    parameter.type = parse_type(cursor);
    parameter.metadata = parse_metadata(cursor);
    result.push_back(parameter);
    if(cursor.take_if(")")) break;
    cursor.expect(",");
  }
  return result;
}

struct Signature
{
  vector<Parameter> parameters;
  Type return_type;
  Metadata metadata;
  bool present;
  Signature() : parameters(), return_type(), metadata(), present(false) {}
};

static Signature parse_call_signature(Cursor & cursor)
{
  Signature result;
  if(!cursor.take_if("as")) return result;
  result.present = true;
  result.parameters = parse_parameters(cursor);
  cursor.expect("->");
  result.return_type = parse_type(cursor);
  result.metadata = parse_metadata(cursor);
  return result;
}

struct DataItem
{
  enum Kind { SCALAR, ADDRESS, ZERO } kind;
  Type type;
  string literal;
  string symbol;
  long long addend;
  size_t zero_bytes;
  DataItem() : kind(SCALAR), type(), literal(), symbol(), addend(0), zero_bytes(0) {}
};

struct Global
{
  string name;
  bool declaration;
  bool structured;
  bool has_type;
  Type type;
  Metadata metadata;
  enum InitKind { INIT_ZERO, INIT_SCALAR, INIT_ADDRESS } init_kind;
  string init_literal;
  string init_symbol;
  long long init_addend;
  vector<DataItem> data;
  Global()
    : name(), declaration(false), structured(false), has_type(false), type(), metadata(),
      init_kind(INIT_ZERO), init_literal(), init_symbol(), init_addend(0), data() {}
};

enum InstructionKind
{
  IK_CONST, IK_COPY, IK_ADDR, IK_LOAD, IK_ATOMIC_LOAD, IK_STORE, IK_ATOMIC_STORE,
  IK_ATOMIC_EXCHANGE, IK_ATOMIC_COMPARE_EXCHANGE, IK_ATOMIC_ADD_FETCH,
  IK_ATOMIC_THREAD_FENCE, IK_ATOMIC_SIGNAL_FENCE, IK_INDEX, IK_UNARY, IK_BINARY,
  IK_CMP, IK_CONVERT, IK_CALL, IK_COPYOBJ, IK_ZEROINIT, IK_JUMP, IK_BRANCH,
  IK_SWITCH, IK_RETURN, IK_EH_TRY, IK_EH_CLEANUP, IK_EH_END, IK_THROW,
  IK_EXCEPTION, IK_RESUME, IK_EH_CATCH, IK_EH_FILTER, IK_EH_CATCH_ALL
};

struct Instruction
{
  InstructionKind kind;
  string dest;
  Type type;
  Type source_type;
  string op;
  vector<Value> operands;
  vector<pair<Value, string> > cases;
  string label;
  string label2;
  string label3;
  size_t bytes;
  size_t alignment;
  Signature signature;
  Metadata metadata;
  Instruction()
    : kind(IK_CONST), dest(), type(), source_type(), op(), operands(), cases(), label(),
      label2(), label3(), bytes(0), alignment(0), signature(), metadata() {}
};

struct Block
{
  string name;
  vector<Instruction> instructions;
};

struct Function
{
  string name;
  bool declaration;
  vector<Parameter> parameters;
  Type return_type;
  Metadata metadata;
  vector<pair<string, Type> > slots;
  vector<Block> blocks;
  Function() : name(), declaration(false), parameters(), return_type(), metadata(), slots(), blocks() {}
};

struct Alias
{
  string object_name;
  string target;
};

struct Program
{
  vector<Global> globals;
  vector<Function> functions;
  vector<Alias> aliases;
};

static string symbol_name(const string & token, char prefix)
{
  if(token.empty() || token[0] != prefix) throw LowError("invalid LowIR symbol");
  return token.substr(1);
}

static long long parse_integer(const string & text)
{
  char * end = NULL;
  errno = 0;
  const long long result = strtoll(text.c_str(), &end, 0);
  if(!end || *end != '\0' || errno == ERANGE) throw LowError("invalid integer literal");
  return result;
}

static pair<size_t, size_t> parse_span(const string & text)
{
  const size_t x = text.find('x');
  if(x == string::npos || x == 0 || x + 1 == text.size()) throw LowError("invalid storage span");
  char * end = NULL;
  const unsigned long bytes = strtoul(text.substr(0, x).c_str(), &end, 10);
  if(!end || *end != '\0') throw LowError("invalid storage span");
  const unsigned long alignment = strtoul(text.substr(x + 1).c_str(), &end, 10);
  if(!end || *end != '\0') throw LowError("invalid storage span");
  return make_pair(static_cast<size_t>(bytes), static_cast<size_t>(alignment));
}

static string remove_debug_suffix(const string & line)
{
  const size_t position = line.find("!dbg");
  return position == string::npos ? line : line.substr(0, position);
}

class LowParser
{
  vector<string> lines_;
  size_t line_number_;
  Program program_;

  static string source_line(const string & source)
  {
    return trim(strip_comment(remove_debug_suffix(source)));
  }

  static void require_end(Cursor & cursor)
  {
    if(!cursor.empty()) throw LowError("trailing LowIR tokens");
  }

  static string parse_symbol_after(Cursor & cursor, char prefix)
  {
    return symbol_name(cursor.take(), prefix);
  }

  static void parse_function_header(Cursor & cursor, Function & function, bool definition)
  {
    cursor.expect(definition ? "function" : "function");
    function.name = parse_symbol_after(cursor, '@');
    function.parameters = parse_parameters(cursor);
    cursor.expect("->");
    function.return_type = parse_type(cursor);
    function.metadata = parse_metadata(cursor);
    function.declaration = !definition;
    if(definition) cursor.expect("{");
    require_end(cursor);
  }

  static void parse_global_declaration(Cursor & cursor, Global & global)
  {
    cursor.expect("declare"); cursor.expect("global");
    global.name = parse_symbol_after(cursor, '@');
    global.declaration = true;
    if(cursor.take_if(":")) { global.type = parse_type(cursor); global.has_type = true; }
    global.metadata = parse_metadata(cursor);
    require_end(cursor);
  }

  static void parse_function_declaration(Cursor & cursor, Function & function)
  {
    cursor.expect("declare");
    parse_function_header(cursor, function, false);
  }

  static long long parse_optional_addend(Cursor & cursor)
  {
    if(cursor.take_if("+")) return parse_integer(cursor.take());
    if(cursor.take_if("-")) return -parse_integer(cursor.take());
    return 0;
  }

  static void parse_global_definition_header(Cursor & cursor, Global & global, bool structured)
  {
    cursor.expect("global");
    global.name = parse_symbol_after(cursor, '@');
    if(cursor.take_if("readonly")) global.metadata["storage"] = "readonly";
    if(structured) {
      global.structured = true;
      global.metadata = parse_metadata(cursor);
      cursor.expect("="); cursor.expect("{");
      require_end(cursor);
      return;
    }
    cursor.expect(":");
    global.type = parse_type(cursor); global.has_type = true;
    global.metadata = parse_metadata(cursor);
    cursor.expect("=");
    if(cursor.take_if("zero")) {
      global.init_kind = Global::INIT_ZERO;
    } else if(cursor.take_if("addr")) {
      const string target = cursor.take();
      if(target.empty() || (target[0] != '@')) throw LowError("invalid global address initializer");
      global.init_kind = Global::INIT_ADDRESS;
      global.init_symbol = target.substr(1);
      global.init_addend = parse_optional_addend(cursor);
    } else {
      const Value literal = parse_literal_or_value(cursor);
      if(literal.kind != Value::INTEGER && literal.kind != Value::FLOAT &&
         literal.kind != Value::NULLPTR) throw LowError("invalid global initializer");
      global.init_kind = Global::INIT_SCALAR;
      global.init_literal = literal.text;
    }
    require_end(cursor);
  }

  static DataItem parse_data_item(Cursor & cursor)
  {
    DataItem item;
    if(cursor.take_if("ptr")) {
      cursor.expect("addr");
      const string target = cursor.take();
      if(target.empty() || target[0] != '@') throw LowError("invalid data address");
      item.kind = DataItem::ADDRESS;
      item.type = Type("ptr");
      item.symbol = target.substr(1);
      item.addend = parse_optional_addend(cursor);
      require_end(cursor);
      return item;
    }
    if(cursor.take_if("zero")) {
      item.kind = DataItem::ZERO;
      item.zero_bytes = static_cast<size_t>(parse_integer(cursor.take()));
      require_end(cursor);
      return item;
    }
    item.type = parse_type(cursor);
    const Value literal = parse_literal_or_value(cursor);
    if(literal.kind != Value::INTEGER && literal.kind != Value::FLOAT &&
       literal.kind != Value::NULLPTR) throw LowError("invalid structured data item");
    item.kind = DataItem::SCALAR;
    item.literal = literal.text;
    require_end(cursor);
    return item;
  }

  static Instruction parse_instruction(const string & source)
  {
    Cursor cursor(lex_line(source));
    Instruction instruction;
    if(cursor.empty()) throw LowError("empty instruction");

    string first = cursor.peek();
    if(!first.empty() && first[0] == '%') {
      instruction.dest = symbol_name(cursor.take(), '%');
      cursor.expect("=");
      first = cursor.peek();
    }

    if(first == "const" || first == "copy" || first == "addr" || first == "load" ||
       first == "atomic_load" || first == "index" || first == "unary" ||
       first == "binary" || first == "cmp" || first == "convert" || first == "call" ||
       first == "atomic_exchange" || first == "atomic_add_fetch" ||
       first == "atomic_compare_exchange" || first == "exception") {
      const string opcode = cursor.take();
      if(opcode == "const") {
        instruction.kind = IK_CONST; instruction.type = parse_type(cursor);
        instruction.operands.push_back(parse_literal_or_value(cursor));
      } else if(opcode == "copy") {
        instruction.kind = IK_COPY; instruction.type = parse_type(cursor);
        instruction.operands.push_back(parse_named_value(cursor));
      } else if(opcode == "addr") {
        instruction.kind = IK_ADDR; instruction.type = Type("ptr");
        instruction.operands.push_back(parse_named_value(cursor));
      } else if(opcode == "load" || opcode == "atomic_load") {
        instruction.kind = opcode == "load" ? IK_LOAD : IK_ATOMIC_LOAD;
        instruction.type = parse_type(cursor);
        instruction.operands.push_back(parse_named_value(cursor));
        if(instruction.kind == IK_ATOMIC_LOAD) {
          cursor.expect(","); instruction.operands.push_back(parse_literal_or_value(cursor));
        }
      } else if(opcode == "index") {
        instruction.kind = IK_INDEX; instruction.type = parse_type(cursor);
        instruction.metadata = parse_metadata(cursor);
        instruction.operands.push_back(parse_named_value(cursor));
        cursor.expect(","); instruction.operands.push_back(parse_literal_or_value(cursor));
      } else if(opcode == "unary") {
        instruction.kind = IK_UNARY; instruction.op = cursor.take();
        instruction.type = parse_type(cursor);
        instruction.operands.push_back(parse_named_value(cursor));
      } else if(opcode == "binary" || opcode == "cmp") {
        instruction.kind = opcode == "binary" ? IK_BINARY : IK_CMP;
        instruction.op = cursor.take(); instruction.type = parse_type(cursor);
        instruction.operands.push_back(parse_literal_or_value(cursor));
        cursor.expect(","); instruction.operands.push_back(parse_literal_or_value(cursor));
      } else if(opcode == "convert") {
        instruction.kind = IK_CONVERT; instruction.op = cursor.take();
        instruction.type = parse_type(cursor); instruction.source_type = parse_type(cursor);
        instruction.operands.push_back(parse_literal_or_value(cursor));
      } else if(opcode == "atomic_exchange" || opcode == "atomic_add_fetch") {
        instruction.kind = opcode == "atomic_exchange" ? IK_ATOMIC_EXCHANGE : IK_ATOMIC_ADD_FETCH;
        instruction.type = parse_type(cursor);
        instruction.operands.push_back(parse_named_value(cursor)); cursor.expect(",");
        instruction.operands.push_back(parse_literal_or_value(cursor)); cursor.expect(",");
        instruction.operands.push_back(parse_literal_or_value(cursor));
      } else if(opcode == "atomic_compare_exchange") {
        instruction.kind = IK_ATOMIC_COMPARE_EXCHANGE; instruction.type = parse_type(cursor);
        for(int i = 0; i < 5; ++i) {
          instruction.operands.push_back(parse_literal_or_value(cursor));
          if(i != 4) cursor.expect(",");
        }
      } else if(opcode == "exception") {
        instruction.kind = IK_EXCEPTION; instruction.type = parse_type(cursor);
      } else { // call
        instruction.kind = IK_CALL; instruction.type = parse_type(cursor);
        instruction.operands.push_back(parse_named_value(cursor));
        cursor.expect("(");
        if(!cursor.take_if(")")) {
          while(true) {
            instruction.operands.push_back(parse_literal_or_value(cursor));
            if(cursor.take_if(")")) break;
            cursor.expect(",");
          }
        }
        instruction.signature = parse_call_signature(cursor);
      }
      require_end(cursor);
      return instruction;
    }

    const string opcode = cursor.take();
    if(opcode == "store" || opcode == "atomic_store") {
      instruction.kind = opcode == "store" ? IK_STORE : IK_ATOMIC_STORE;
      instruction.type = parse_type(cursor);
      instruction.operands.push_back(parse_literal_or_value(cursor)); cursor.expect(",");
      instruction.operands.push_back(parse_named_value(cursor));
      if(instruction.kind == IK_ATOMIC_STORE) {
        cursor.expect(","); instruction.operands.push_back(parse_literal_or_value(cursor));
      }
    } else if(opcode == "atomic_thread_fence" || opcode == "atomic_signal_fence") {
      instruction.kind = opcode == "atomic_thread_fence" ? IK_ATOMIC_THREAD_FENCE : IK_ATOMIC_SIGNAL_FENCE;
      instruction.operands.push_back(parse_literal_or_value(cursor));
    } else if(opcode == "call") {
      instruction.kind = IK_CALL; cursor.expect("void"); instruction.type = Type("void");
      instruction.operands.push_back(parse_named_value(cursor)); cursor.expect("(");
      if(!cursor.take_if(")")) {
        while(true) {
          instruction.operands.push_back(parse_literal_or_value(cursor));
          if(cursor.take_if(")")) break;
          cursor.expect(",");
        }
      }
      instruction.signature = parse_call_signature(cursor);
    } else if(opcode == "copyobj") {
      instruction.kind = IK_COPYOBJ;
      const pair<size_t, size_t> span = parse_span(cursor.take());
      instruction.bytes = span.first; instruction.alignment = span.second;
      instruction.operands.push_back(parse_named_value(cursor)); cursor.expect(",");
      instruction.operands.push_back(parse_named_value(cursor));
    } else if(opcode == "zeroinit") {
      instruction.kind = IK_ZEROINIT;
      const pair<size_t, size_t> span = parse_span(cursor.take());
      instruction.bytes = span.first; instruction.alignment = span.second;
      instruction.operands.push_back(parse_named_value(cursor));
    } else if(opcode == "jump") {
      instruction.kind = IK_JUMP; instruction.label = symbol_name(cursor.take(), '^');
    } else if(opcode == "branch") {
      instruction.kind = IK_BRANCH; instruction.operands.push_back(parse_named_value(cursor));
      cursor.expect(","); instruction.label = symbol_name(cursor.take(), '^');
      cursor.expect(","); instruction.label2 = symbol_name(cursor.take(), '^');
    } else if(opcode == "switch") {
      instruction.kind = IK_SWITCH; instruction.operands.push_back(parse_named_value(cursor));
      cursor.expect(","); instruction.label = symbol_name(cursor.take(), '^');
      while(!cursor.empty()) {
        cursor.expect(",");
        const Value value = parse_literal_or_value(cursor);
        cursor.expect(":");
        instruction.cases.push_back(make_pair(value, symbol_name(cursor.take(), '^')));
      }
    } else if(opcode == "return") {
      instruction.kind = IK_RETURN; instruction.type = parse_type(cursor);
      if(!cursor.empty()) instruction.operands.push_back(parse_literal_or_value(cursor));
    } else if(opcode == "eh_try" || opcode == "eh_cleanup") {
      instruction.kind = opcode == "eh_try" ? IK_EH_TRY : IK_EH_CLEANUP;
      instruction.label = symbol_name(cursor.take(), '^');
    } else if(opcode == "eh_end") {
      instruction.kind = IK_EH_END;
    } else if(opcode == "throw") {
      instruction.kind = IK_THROW; instruction.type = parse_type(cursor);
      instruction.operands.push_back(parse_literal_or_value(cursor));
    } else if(opcode == "resume") {
      instruction.kind = IK_RESUME;
    } else if(opcode == "eh_catch") {
      instruction.kind = IK_EH_CATCH; instruction.operands.push_back(parse_named_value(cursor));
    } else if(opcode == "eh_filter") {
      instruction.kind = IK_EH_FILTER;
      while(!cursor.empty()) {
        if(cursor.take_if(",")) continue;
        instruction.operands.push_back(parse_named_value(cursor));
      }
    } else if(opcode == "eh_catch_all") {
      instruction.kind = IK_EH_CATCH_ALL;
    } else {
      throw LowError("unknown LowIR instruction");
    }
    require_end(cursor);
    return instruction;
  }

  void parse_function_body(Function & function, size_t & index)
  {
    Block * current = NULL;
    for(++index; index < lines_.size(); ++index) {
      const string line = source_line(lines_[index]);
      if(line.empty()) continue;
      if(line == "}") return;
      const vector<string> tokens = lex_line(line);
      if(tokens.empty()) continue;
      if(tokens[0] == "slot") {
        Cursor cursor(tokens); cursor.take();
        const string name = symbol_name(cursor.take(), '$'); cursor.expect(":");
        function.slots.push_back(make_pair(name, parse_type(cursor))); require_end(cursor); continue;
      }
      if(tokens[0] == "block") {
        Cursor cursor(tokens); cursor.take();
        Block block; block.name = symbol_name(cursor.take(), '^'); cursor.expect(":"); require_end(cursor);
        function.blocks.push_back(block); current = &function.blocks.back(); continue;
      }
      if(!current) throw LowError("instruction before block");
      current->instructions.push_back(parse_instruction(line));
    }
    throw LowError("unterminated function");
  }

  void parse_structured_global(Global & global, size_t & index)
  {
    for(++index; index < lines_.size(); ++index) {
      const string line = source_line(lines_[index]);
      if(line.empty()) continue;
      if(line == "}") return;
      Cursor cursor(lex_line(line));
      global.data.push_back(parse_data_item(cursor));
    }
    throw LowError("unterminated structured global");
  }

public:
  explicit LowParser(const vector<string> & lines) : lines_(lines), line_number_(0), program_() {}

  Program parse()
  {
    for(size_t index = 0; index < lines_.size(); ++index) {
      line_number_ = index + 1;
      const string line = source_line(lines_[index]);
      if(line.empty()) continue;
      Cursor cursor(lex_line(line));
      if(cursor.empty()) continue;
      const string first = cursor.peek();
      if(first == "declare") {
        if(cursor.peek(1) == "global") {
          Global global; parse_global_declaration(cursor, global); program_.globals.push_back(global);
        } else if(cursor.peek(1) == "function") {
          Function function; parse_function_declaration(cursor, function); program_.functions.push_back(function);
        } else throw LowError("invalid declaration");
      } else if(first == "global") {
        const vector<string> tokens = lex_line(line);
        const bool structured = find(tokens.begin(), tokens.end(), "{") != tokens.end();
        Global global; Cursor global_cursor(tokens);
        parse_global_definition_header(global_cursor, global, structured);
        program_.globals.push_back(global);
        if(structured) parse_structured_global(program_.globals.back(), index);
      } else if(first == "function") {
        Function function; Cursor function_cursor(lex_line(line));
        parse_function_header(function_cursor, function, true);
        program_.functions.push_back(function);
        parse_function_body(program_.functions.back(), index);
      } else if(first == "alias") {
        cursor.take(); cursor.expect("object");
        Alias alias; alias.object_name = cursor.take(); cursor.expect("=");
        alias.target = cursor.take();
        if(alias.target.empty() || alias.target[0] != '@') throw LowError("invalid object alias");
        alias.target = alias.target.substr(1); require_end(cursor); program_.aliases.push_back(alias);
      } else throw LowError("invalid top-level LowIR item");
    }
    return program_;
  }
};

static vector<string> read_lines(const vector<string> & paths)
{
  vector<string> lines;
  for(size_t i = 0; i < paths.size(); ++i) {
    ifstream input(paths[i].c_str());
    if(!input) throw LowError("cannot open LowIR input");
    string line;
    while(getline(input, line)) lines.push_back(line);
  }
  return lines;
}

static bool valid_type(const Type & type, bool allow_void = true)
{
  if(type.is_void()) return allow_void;
  if(type.is_integer() || type.is_float() || type.is_pointer()) return true;
  if(type.is_object() && type.object_bytes > 0 && type.object_alignment > 0) return true;
  return false;
}

static bool is_power_of_two(size_t value)
{
  return value != 0 && (value & (value - 1)) == 0;
}

static void validate_symbol_metadata(const Metadata & metadata, bool function, bool declaration)
{
  static const set<string> function_keys = {"role", "linkage", "binding", "object", "tls_for",
                                            "keep_alias", "prefer_local", "trivial_lifecycle",
                                            "arity", "effects", "unwind", "return"};
  static const set<string> global_keys = {"role", "linkage", "binding", "object", "keep_alias",
                                          "prefer_local", "storage"};
  const set<string> & allowed = function ? function_keys : global_keys;
  for(Metadata::const_iterator it = metadata.begin(); it != metadata.end(); ++it) {
    if(allowed.find(it->first) == allowed.end()) throw LowError("unknown metadata key");
    const string & key = it->first;
    const string & value = it->second;
    if(key == "linkage" && value != "c" && value != "cpp") throw LowError("invalid linkage");
    if(key == "binding" && value != "internal" && value != "strong" && value != "weak")
      throw LowError("invalid binding");
    if((key == "keep_alias" || key == "prefer_local" || key == "trivial_lifecycle") &&
       value != "yes" && value != "no") throw LowError("invalid boolean metadata");
    if(key == "storage" && value != "readonly" && value != "thread_local")
      throw LowError("invalid storage");
    if(key == "arity" && value != "fixed" && value != "variadic" && value != "prototype_relaxed")
      throw LowError("invalid arity");
    if(key == "effects" && value != "readnone" && value != "readonly" && value != "readwrite")
      throw LowError("invalid effects");
    if(key == "unwind" && value != "may" && value != "no") throw LowError("invalid unwind");
    if(key == "return" && value != "returns" && value != "noreturn") throw LowError("invalid return");
    if(key == "role") {
      const set<string> function_roles = {"entry", "init", "fini", "eh_unhandled",
        "eh_allocate_exception", "eh_begin_catch", "eh_call_unexpected", "eh_current_exception_type",
        "eh_end_catch", "eh_rethrow", "eh_throw", "eh_personality", "eh_resume"};
      const set<string> global_roles = {"eh_top", "eh_value", "eh_type"};
      const set<string> & roles = function ? function_roles : global_roles;
      if(roles.find(value) == roles.end()) throw LowError("invalid role");
    }
    if(!function && (key == "tls_for" || key == "trivial_lifecycle" || key == "arity" ||
                     key == "effects" || key == "unwind" || key == "return"))
      throw LowError("invalid global metadata");
    if(declaration && key == "keep_alias") throw LowError("invalid declaration metadata");
  }
}

static void validate_parameters(const vector<Parameter> & parameters, const Type & return_type)
{
  set<string> names;
  bool indirect_result = false;
  for(size_t i = 0; i < parameters.size(); ++i) {
    const Parameter & parameter = parameters[i];
    if(!valid_type(parameter.type, false) || !names.insert(parameter.name).second)
      throw LowError("invalid or duplicate parameter");
    for(Metadata::const_iterator it = parameter.metadata.begin(); it != parameter.metadata.end(); ++it) {
      if(it->first != "pass" && it->first != "capture" && it->first != "access" && it->first != "alias")
        throw LowError("unknown parameter metadata");
      if(it->first == "pass" && it->second != "direct" && it->second != "indirect_result" &&
         it->second != "by_address" && it->second != "reference" && it->second != "decay")
        throw LowError("invalid parameter passing mode");
      if(it->first == "capture" && it->second != "nocapture" && it->second != "maycapture")
        throw LowError("invalid capture mode");
      if(it->first == "access" && it->second != "none" && it->second != "read" &&
         it->second != "write" && it->second != "readwrite") throw LowError("invalid access mode");
      if(it->first == "alias" && it->second != "noalias") throw LowError("invalid alias mode");
      if(it->first == "pass" && it->second != "direct" && !parameter.type.is_pointer())
        throw LowError("non-pointer indirect parameter");
      if(it->first != "pass" && !parameter.type.is_pointer()) throw LowError("pointer metadata on scalar");
      if(it->first == "pass" && it->second == "indirect_result") {
        if(i != 0 || !return_type.is_void() || indirect_result) throw LowError("invalid indirect result");
        indirect_result = true;
      }
    }
  }
}

static string function_role(const Function & function)
{
  Metadata::const_iterator role = function.metadata.find("role");
  return role != function.metadata.end() ? role->second :
         (function.name == "main" ? "entry" : (function.name == "__cppgm_init" ? "init" :
          (function.name == "__cppgm_fini" ? "fini" : string())));
}

static bool is_terminator(InstructionKind kind)
{
  return kind == IK_JUMP || kind == IK_BRANCH || kind == IK_SWITCH || kind == IK_RETURN ||
         kind == IK_THROW || kind == IK_RESUME;
}

class Validator
{
  const Program & program_;
  map<string, bool> globals_;
  map<string, bool> functions_;

  static bool same_scalar_family(const Type & left, const Type & right)
  {
    return left == right || (left.is_integer() && right.is_integer()) ||
           (left.is_float() && right.is_float()) || (left.is_pointer() && right.is_pointer());
  }

  static bool type_of_value(const Value & value, const map<string, Type> & temps,
                            const map<string, Type> & slots, Type * result)
  {
    if(value.kind == Value::TEMP) {
      map<string, Type>::const_iterator it = temps.find(value.text);
      if(it == temps.end()) return false;
      *result = it->second;
      return true;
    }
    if(value.kind == Value::SLOT) {
      map<string, Type>::const_iterator it = slots.find(value.text);
      if(it == slots.end()) return false;
      *result = it->second;
      return true;
    }
    if(value.kind == Value::GLOBAL) { *result = Type("ptr"); return true; }
    if(value.kind == Value::NULLPTR) { *result = Type("ptr"); return true; }
    if(value.kind == Value::FLOAT) { *result = Type("f64"); return true; }
    if(value.kind == Value::INTEGER) { *result = Type("i64"); return true; }
    return false;
  }

  void validate_instruction(const Function & function, const Block & block,
                            const Instruction & instruction, map<string, Type> & temps,
                            const map<string, Type> & slots) const
  {
    (void)function; (void)block;
    Type value_type;
    for(size_t i = 0; i < instruction.operands.size(); ++i) {
      // The order fields on atomics are integer literals, not value operands.
      if((instruction.kind == IK_ATOMIC_LOAD && i == 1) ||
         (instruction.kind == IK_ATOMIC_STORE && i == 2) ||
         (instruction.kind == IK_ATOMIC_EXCHANGE && i == 2) ||
         (instruction.kind == IK_ATOMIC_ADD_FETCH && i == 2) ||
         (instruction.kind == IK_ATOMIC_COMPARE_EXCHANGE && (i == 3 || i == 4))) continue;
      if(!type_of_value(instruction.operands[i], temps, slots, &value_type))
        throw LowError("undefined LowIR value");
    }
    if(!instruction.dest.empty()) {
      Type result = instruction.type;
      if(instruction.kind == IK_CMP || instruction.kind == IK_ATOMIC_COMPARE_EXCHANGE) result = Type("i64");
      if(instruction.kind == IK_ADDR || instruction.kind == IK_INDEX) result = Type("ptr");
      if(!valid_type(result, false)) throw LowError("invalid instruction result type");
      if(temps.find(instruction.dest) != temps.end()) throw LowError("duplicate temporary");
      temps[instruction.dest] = result;
    }
    if(instruction.kind == IK_INDEX) {
      const Metadata::const_iterator projection = instruction.metadata.find("projection");
      if(projection != instruction.metadata.end() && projection->second != "array_element" &&
         projection->second != "field" && projection->second != "base_subobject" &&
         projection->second != "reference_field") throw LowError("invalid index projection");
    }
    if(instruction.kind == IK_UNARY) {
      if(instruction.op == "decay" && (instruction.type != Type("ptr"))) throw LowError("invalid decay");
      if(instruction.op != "neg" && instruction.op != "not" && instruction.op != "bitnot" &&
         instruction.op != "bswap" && instruction.op != "decay") throw LowError("invalid unary op");
      if(instruction.op == "bswap" && instruction.type.width() != 16 && instruction.type.width() != 32 &&
         instruction.type.width() != 64) throw LowError("invalid bswap width");
    }
    if(instruction.kind == IK_CONVERT) {
      const int dst = instruction.type.width(); const int src = instruction.source_type.width();
      if(!dst || !src) throw LowError("invalid conversion type");
      if((instruction.op == "sext" || instruction.op == "zext") &&
         (!instruction.type.is_integer() || !instruction.source_type.is_integer() || dst <= src))
        throw LowError("invalid integer widening conversion");
      if(instruction.op == "trunc" && (!instruction.type.is_integer() || !instruction.source_type.is_integer() ||
         dst >= src)) throw LowError("invalid integer truncation");
      if((instruction.op == "fpext" && (!instruction.type.is_float() || !instruction.source_type.is_float() || dst <= src)) ||
         (instruction.op == "fptrunc" && (!instruction.type.is_float() || !instruction.source_type.is_float() || dst >= src)))
        throw LowError("invalid floating conversion");
      if((instruction.op == "sitofp" || instruction.op == "uitofp") &&
         (!instruction.type.is_float() || !instruction.source_type.is_integer())) throw LowError("invalid int-float conversion");
      if((instruction.op == "fptosi" || instruction.op == "fptoui") &&
         (!instruction.type.is_integer() || !instruction.source_type.is_float())) throw LowError("invalid float-int conversion");
    }
    if(instruction.kind == IK_CALL) {
      if(instruction.operands.empty()) throw LowError("invalid call");
      const Value & callee = instruction.operands[0];
      if((callee.kind == Value::TEMP || callee.kind == Value::GLOBAL) && !instruction.signature.present) {
        if(callee.kind == Value::TEMP) throw LowError("missing indirect call signature");
      }
      if(instruction.signature.present) {
        validate_parameters(instruction.signature.parameters, instruction.signature.return_type);
        validate_symbol_metadata(instruction.signature.metadata, true, false);
      }
    }
    if(instruction.kind == IK_COPYOBJ || instruction.kind == IK_ZEROINIT) {
      if(instruction.bytes == 0 || !is_power_of_two(instruction.alignment)) throw LowError("invalid storage span");
    }
  }

public:
  explicit Validator(const Program & program) : program_(program), globals_(), functions_() {}

  void validate()
  {
    set<string> symbols;
    set<string> aliases;
    for(size_t i = 0; i < program_.globals.size(); ++i) {
      const Global & global = program_.globals[i];
      if(!symbols.insert(global.name).second) throw LowError("duplicate top-level symbol");
      globals_[global.name] = !global.declaration;
      validate_symbol_metadata(global.metadata, false, global.declaration);
      if(!global.declaration && (!global.structured && !valid_type(global.type, false)))
        throw LowError("invalid global type");
      if(global.structured && global.data.empty()) throw LowError("empty structured global");
      for(size_t j = 0; j < global.data.size(); ++j) {
        if(global.data[j].kind == DataItem::SCALAR && !valid_type(global.data[j].type, false))
          throw LowError("invalid global data type");
      }
    }
    for(size_t i = 0; i < program_.functions.size(); ++i) {
      const Function & function = program_.functions[i];
      if(!symbols.insert(function.name).second) throw LowError("duplicate top-level symbol");
      functions_[function.name] = !function.declaration;
      validate_symbol_metadata(function.metadata, true, function.declaration);
      if(!valid_type(function.return_type, true)) throw LowError("invalid return type");
      validate_parameters(function.parameters, function.return_type);
      if(function.declaration) continue;
      if(function.blocks.empty()) throw LowError("function has no blocks");
      set<string> slots_seen;
      map<string, Type> slots;
      for(size_t j = 0; j < function.slots.size(); ++j) {
        if(!slots_seen.insert(function.slots[j].first).second || !valid_type(function.slots[j].second, false))
          throw LowError("duplicate or invalid slot");
        slots[function.slots[j].first] = function.slots[j].second;
      }
      set<string> blocks_seen;
      for(size_t j = 0; j < function.blocks.size(); ++j) {
        const Block & block = function.blocks[j];
        if(!blocks_seen.insert(block.name).second || block.instructions.empty()) throw LowError("invalid block");
        bool terminated = false;
        map<string, Type> temps;
        // Parameters are available in every block and are checked in source order.
        for(size_t p = 0; p < function.parameters.size(); ++p) temps[function.parameters[p].name] = function.parameters[p].type;
        for(size_t b = 0; b < function.blocks.size(); ++b) {
          if(b == j) break;
          for(size_t k = 0; k < function.blocks[b].instructions.size(); ++k) {
            const Instruction & previous = function.blocks[b].instructions[k];
            if(!previous.dest.empty()) {
              Type result = previous.type;
              if(previous.kind == IK_CMP || previous.kind == IK_ATOMIC_COMPARE_EXCHANGE) result = Type("i64");
              if(previous.kind == IK_ADDR || previous.kind == IK_INDEX) result = Type("ptr");
              temps[previous.dest] = result;
            }
          }
        }
        for(size_t k = 0; k < block.instructions.size(); ++k) {
          const Instruction & instruction = block.instructions[k];
          if(terminated) throw LowError("instruction after terminator");
          validate_instruction(function, block, instruction, temps, slots);
          if(is_terminator(instruction.kind)) terminated = true;
        }
        if(!terminated || !is_terminator(block.instructions.back().kind)) throw LowError("missing terminator");
      }
      for(size_t j = 0; j < function.blocks.size(); ++j) {
        const Block & block = function.blocks[j];
        for(size_t k = 0; k < block.instructions.size(); ++k) {
          const Instruction & instruction = block.instructions[k];
          if((instruction.kind == IK_JUMP && blocks_seen.find(instruction.label) == blocks_seen.end()) ||
             (instruction.kind == IK_BRANCH && (blocks_seen.find(instruction.label) == blocks_seen.end() ||
                                                blocks_seen.find(instruction.label2) == blocks_seen.end())) ||
             (instruction.kind == IK_SWITCH && blocks_seen.find(instruction.label) == blocks_seen.end()))
            throw LowError("undefined block target");
          for(size_t c = 0; c < instruction.cases.size(); ++c)
            if(blocks_seen.find(instruction.cases[c].second) == blocks_seen.end()) throw LowError("undefined switch target");
        }
      }
    }
    for(size_t i = 0; i < program_.globals.size(); ++i) {
      const Global & global = program_.globals[i];
      if(global.declaration) continue;
      if(global.init_kind == Global::INIT_ADDRESS && symbols.find(global.init_symbol) == symbols.end())
        throw LowError("undefined global initializer target");
      for(size_t j = 0; j < global.data.size(); ++j)
        if(global.data[j].kind == DataItem::ADDRESS && symbols.find(global.data[j].symbol) == symbols.end())
          throw LowError("undefined structured data target");
    }
    for(size_t i = 0; i < program_.aliases.size(); ++i) {
      const Alias & alias = program_.aliases[i];
      if(!aliases.insert(alias.object_name).second || symbols.find(alias.target) == symbols.end())
        throw LowError("invalid object alias");
    }
    for(size_t i = 0; i < program_.functions.size(); ++i) {
      const Function & function = program_.functions[i];
      if(function.declaration) continue;
      for(size_t b = 0; b < function.blocks.size(); ++b) {
        for(size_t k = 0; k < function.blocks[b].instructions.size(); ++k) {
          const Instruction & instruction = function.blocks[b].instructions[k];
          for(size_t o = 0; o < instruction.operands.size(); ++o) {
            if(instruction.operands[o].kind == Value::GLOBAL &&
               globals_.find(instruction.operands[o].text) == globals_.end() &&
               functions_.find(instruction.operands[o].text) == functions_.end())
              throw LowError("undefined top-level operand");
          }
          if(instruction.kind == IK_CALL && !instruction.operands.empty() &&
             instruction.operands[0].kind == Value::GLOBAL &&
             functions_.find(instruction.operands[0].text) == functions_.end() &&
             !instruction.signature.present)
            throw LowError("indirect global call is missing signature");
        }
      }
    }
    for(size_t i = 0; i < program_.functions.size(); ++i) {
      const Function & function = program_.functions[i];
      Metadata::const_iterator tls = function.metadata.find("tls_for");
      if(tls != function.metadata.end()) {
        if(tls->second.empty() || tls->second[0] != '@') throw LowError("invalid tls_for");
        const string target = tls->second.substr(1);
        bool thread_local_global = false;
        for(size_t g = 0; g < program_.globals.size(); ++g) {
          if(program_.globals[g].name == target && program_.globals[g].metadata.find("storage") != program_.globals[g].metadata.end() &&
             program_.globals[g].metadata.find("storage")->second == "thread_local") thread_local_global = true;
        }
        if(!thread_local_global) throw LowError("tls_for target is not thread local");
      }
    }
    set<string> roles;
    for(size_t i = 0; i < program_.globals.size(); ++i) {
      Metadata::const_iterator role = program_.globals[i].metadata.find("role");
      if(role != program_.globals[i].metadata.end() && !roles.insert(role->second).second)
        throw LowError("duplicate singleton role");
    }
    for(size_t i = 0; i < program_.functions.size(); ++i) {
      Metadata::const_iterator role = program_.functions[i].metadata.find("role");
      if(role != program_.functions[i].metadata.end() && !roles.insert(role->second).second)
        throw LowError("duplicate singleton role");
    }
    bool entry = false;
    for(size_t i = 0; i < program_.functions.size(); ++i)
      if(!program_.functions[i].declaration && function_role(program_.functions[i]) == "entry") entry = true;
    if(!entry) throw LowError("missing LowIR entry function");
  }
};

struct Location
{
  Type type;
  int offset;
  Location() : type(), offset(0) {}
  Location(const Type & value_type, int value_offset) : type(value_type), offset(value_offset) {}
};

static int rounded_storage(const Type & type)
{
  const size_t size = type.storage_size();
  if(size == 0) return 0;
  return static_cast<int>(((size + 7) / 8) * 8);
}

static string mem_at(const string & base, int offset)
{
  ostringstream result;
  result << "[" << base;
  if(offset > 0) result << "+" << offset;
  else if(offset < 0) result << offset;
  result << "]";
  return result.str();
}

static string reg_name(char name, int width)
{
  ostringstream result; result << name << width; return result.str();
}

static string signed_literal(const string & value)
{
  if(value == "-0") return "0";
  return value;
}

static string f80_literal(const string & value)
{
  if(!value.empty() && (value[value.size() - 1] == 'L' || value[value.size() - 1] == 'l')) return value;
  return value + "L";
}

static string output_label_function(const string & name) { return "fn__" + name; }
static string output_label_global(const string & name) { return "g__" + name; }

static string addend_text(const string & symbol, long long addend)
{
  if(addend == 0) return symbol;
  return symbol + (addend > 0 ? "+" : "") + to_string(addend);
}

class FunctionEmitter
{
  const Program & program_;
  const Function & function_;
  const map<string, const Function *> & function_map_;
  const map<string, const Global *> & global_map_;
  map<string, Location> locations_;
  map<string, Location> slots_;
  map<string, Location> params_;
  int next_offset_;
  int frame_size_;
  bool has_f80_scratch_;
  int scratch0_, scratch1_, scratch2_;
  int f80_return_temp_;
  int hidden_return_offset_;
  vector<string> output_;
  int * eh_serial_;

  void line(const string & text) { output_.push_back("\t" + text + ";"); }
  void label(const string & text) { output_.push_back(text + ":"); }

  int allocate(const Type & type)
  {
    next_offset_ += rounded_storage(type);
    return -next_offset_;
  }

  Location location_for(const Value & value) const
  {
    if(value.kind == Value::TEMP) {
      map<string, Location>::const_iterator it = locations_.find(value.text);
      if(it == locations_.end()) throw LowError("unknown temporary");
      return it->second;
    }
    if(value.kind == Value::SLOT) {
      map<string, Location>::const_iterator it = slots_.find(value.text);
      if(it == slots_.end()) throw LowError("unknown slot");
      return it->second;
    }
    throw LowError("value has no local location");
  }

  static string immediate(const Value & value)
  {
    if(value.kind == Value::INTEGER || value.kind == Value::FLOAT || value.kind == Value::NULLPTR)
      return signed_literal(value.text);
    throw LowError("expected immediate");
  }

  bool is_direct_object_value(const Value & value) const
  {
    if(value.kind != Value::TEMP && value.kind != Value::SLOT) return false;
    return location_for(value).type.is_object();
  }

  string location_memory(const Location & location) const { return mem_at("bp", location.offset); }

  void load_scalar(const Value & value, const Type & type, char target)
  {
    const int width = type.width() == 1 ? 8 : type.width();
    const string reg = reg_name(target, width);
    if(value.kind == Value::INTEGER || value.kind == Value::NULLPTR) {
      // LowIR integer literals enter the CY86 lowering through a 64-bit
      // machine value.  Narrowing happens when the value is stored or when
      // the narrow register alias is consumed.
      line("move64 " + reg_name(target, 64) + " " + immediate(value));
    } else if(value.kind == Value::FLOAT) {
      line("move" + to_string(width) + " " + reg + " " + immediate(value));
    } else if(value.kind == Value::TEMP || value.kind == Value::SLOT) {
      const Location location = location_for(value);
      line("move" + to_string(width) + " " + reg + " " + location_memory(location));
    } else if(value.kind == Value::GLOBAL) {
      line("move" + to_string(width) + " " + reg + " [" + output_label_global(value.text) + "]");
    } else throw LowError("cannot load scalar value");
  }

  void store_scalar(const Location & destination, const Type & type, char source)
  {
    const int width = type.width() == 1 ? 8 : type.width();
    line("move" + to_string(width) + " " + location_memory(destination) + " " + reg_name(source, width));
  }

  void load_pointer_address(const Value & value, char target)
  {
    if(value.kind == Value::GLOBAL) {
      if(function_map_.find(value.text) != function_map_.end())
        line("move64 " + reg_name(target, 64) + " " + output_label_function(value.text));
      else line("move64 " + reg_name(target, 64) + " " + output_label_global(value.text));
    }
    else if(value.kind == Value::TEMP || value.kind == Value::SLOT) {
      const Location location = location_for(value);
      if(location.type.is_object() || location.type.is_f80())
        line("isub64 " + reg_name(target, 64) + " bp " + to_string(-location.offset));
      else line("move64 " + reg_name(target, 64) + " " + location_memory(location));
    } else throw LowError("expected address value");
  }

  void load_addressable(const Value & value, char target)
  {
    if(value.kind == Value::SLOT) {
      const Location location = location_for(value);
      line("isub64 " + reg_name(target, 64) + " bp " + to_string(-location.offset));
    } else if(value.kind == Value::GLOBAL) {
      if(function_map_.find(value.text) != function_map_.end())
        line("move64 " + reg_name(target, 64) + " " + output_label_function(value.text));
      else line("move64 " + reg_name(target, 64) + " " + output_label_global(value.text));
    } else if(value.kind == Value::TEMP) {
      const Location location = location_for(value);
      if(location.type.is_object() || location.type.is_f80())
        line("isub64 " + reg_name(target, 64) + " bp " + to_string(-location.offset));
      else line("move64 " + reg_name(target, 64) + " " + location_memory(location));
    } else throw LowError("invalid addressable operand");
  }

  void store_result_scalar(const string & dest, const Type & type, char source)
  {
    if(dest.empty()) return;
    const map<string, Location>::const_iterator found = locations_.find(dest);
    if(found == locations_.end()) throw LowError("missing result location");
    store_scalar(found->second, type, source);
  }

  void emit_canonical_bool(const string & dest)
  {
    line("move64 x64 0"); line("move8 x8 z8");
    store_result_scalar(dest, Type("i64"), 'x');
  }

  void emit_f80_pad(int base)
  {
    line("move64 z64 0");
    line("move32 " + mem_at("bp", base + 10) + " z32");
    line("move16 " + mem_at("bp", base + 14) + " z16");
  }

  void copy_f80_memory(int source_base, int destination_base)
  {
    line("move64 z64 " + mem_at("bp", source_base));
    line("move64 " + mem_at("bp", destination_base) + " z64");
    line("move64 z64 " + mem_at("bp", source_base + 8));
    line("move64 " + mem_at("bp", destination_base + 8) + " z64");
  }

  void load_f80_to_scratch(const Value & value, int scratch)
  {
    if(value.kind == Value::INTEGER || value.kind == Value::FLOAT || value.kind == Value::NULLPTR) {
      line("move80 " + mem_at("bp", scratch) + " " + f80_literal(immediate(value)));
      emit_f80_pad(scratch);
      return;
    }
    Location location = location_for(value);
    if(!location.type.is_f80()) throw LowError("expected f80 value");
    line("isub64 x64 bp " + to_string(-location.offset));
    line("move64 z64 [x64]"); line("move64 " + mem_at("bp", scratch) + " z64");
    line("move64 z64 [x64+8]"); line("move64 " + mem_at("bp", scratch + 8) + " z64");
  }

  void store_f80_from_scratch(const string & dest, int scratch)
  {
    const Location location = locations_.find(dest)->second;
    copy_f80_memory(scratch, location.offset);
  }

  void load_f80_storage(const Value & storage, int scratch)
  {
    if(storage.kind == Value::GLOBAL) {
      line("move64 x64 " + output_label_global(storage.text));
      line("move64 z64 [x64]"); line("move64 " + mem_at("bp", scratch) + " z64");
      line("move64 z64 [x64+8]"); line("move64 " + mem_at("bp", scratch + 8) + " z64");
    } else if(storage.kind == Value::SLOT) {
      const Location location = location_for(storage);
      copy_f80_memory(location.offset, scratch);
    } else {
      load_pointer_address(storage, 'x');
      line("move64 z64 [x64]"); line("move64 " + mem_at("bp", scratch) + " z64");
      line("move64 z64 [x64+8]"); line("move64 " + mem_at("bp", scratch + 8) + " z64");
    }
  }

  void store_f80_storage(const Value & storage, int scratch)
  {
    if(storage.kind == Value::GLOBAL) {
      line("move64 x64 " + output_label_global(storage.text));
      line("move64 z64 " + mem_at("bp", scratch)); line("move64 [x64] z64");
      line("move64 z64 " + mem_at("bp", scratch + 8)); line("move64 [x64+8] z64");
    } else if(storage.kind == Value::SLOT) {
      const Location location = location_for(storage);
      copy_f80_memory(scratch, location.offset);
    } else {
      load_pointer_address(storage, 'x');
      line("move64 z64 " + mem_at("bp", scratch)); line("move64 [x64] z64");
      line("move64 z64 " + mem_at("bp", scratch + 8)); line("move64 [x64+8] z64");
    }
  }

  void emit_copy_bytes(const Value & source, const Value & destination, size_t bytes,
                       bool source_object = false)
  {
    load_pointer_address(destination, 'x');
    if(source_object) load_addressable(source, 'y');
    else load_pointer_address(source, 'y');
    size_t copied = 0;
    while(copied + 8 <= bytes) {
      line("move64 z64 [y64]"); line("move64 [x64] z64"); copied += 8;
      if(copied < bytes) { line("iadd64 x64 x64 8"); line("iadd64 y64 y64 8"); }
    }
    if(copied + 4 <= bytes) {
      line("move32 z32 [y64]"); line("move32 [x64] z32"); copied += 4;
      if(copied < bytes) { line("iadd64 x64 x64 4"); line("iadd64 y64 y64 4"); }
    }
    if(copied + 2 <= bytes) {
      line("move16 z16 [y64]"); line("move16 [x64] z16"); copied += 2;
      if(copied < bytes) { line("iadd64 x64 x64 2"); line("iadd64 y64 y64 2"); }
    }
    if(copied < bytes) line("move8 [x64] [y64]");
  }

  void emit_zero_bytes(const Value & destination, size_t bytes)
  {
    if(destination.kind == Value::SLOT || is_direct_object_value(destination)) load_addressable(destination, 'x');
    else load_pointer_address(destination, 'x');
    line("move64 z64 0");
    size_t zeroed = 0;
    while(zeroed + 8 <= bytes) {
      line("move64 [x64] z64"); zeroed += 8;
      if(zeroed < bytes) line("iadd64 x64 x64 8");
    }
    if(zeroed + 4 <= bytes) {
      line("move32 [x64] z32"); zeroed += 4;
      if(zeroed < bytes) line("iadd64 x64 x64 4");
    }
    if(zeroed + 2 <= bytes) {
      line("move16 [x64] z16"); zeroed += 2;
      if(zeroed < bytes) line("iadd64 x64 x64 2");
    }
    if(zeroed < bytes) line("move8 [x64] z8");
  }

  void emit_index(const Instruction & instruction)
  {
    const Value & base = instruction.operands[0];
    if(is_direct_object_value(base)) load_addressable(base, 'y');
    else load_pointer_address(base, 'y');
    load_scalar(instruction.operands[1], Type("i64"), 'x');
    const size_t element_size = instruction.type.storage_size();
    if(element_size != 1) {
      line("move64 z64 " + to_string(element_size));
      line("smul64 x64 x64 z64");
    }
    line("iadd64 x64 y64 x64");
    store_result_scalar(instruction.dest, Type("ptr"), 'x');
  }

  void emit_binary(const Instruction & instruction)
  {
    const int width = instruction.type.width() == 1 ? 8 : instruction.type.width();
    if(instruction.type.is_f80()) {
      load_f80_to_scratch(instruction.operands[0], scratch0_);
      load_f80_to_scratch(instruction.operands[1], scratch1_);
      line("f" + instruction.op + "80 " + mem_at("bp", scratch2_) + " " +
           mem_at("bp", scratch0_) + " " + mem_at("bp", scratch1_));
      emit_f80_pad(scratch2_);
      store_f80_from_scratch(instruction.dest, scratch2_);
      return;
    }
    load_scalar(instruction.operands[0], instruction.type, 'y');
    load_scalar(instruction.operands[1], instruction.type, 'x');
    string opcode;
    bool shift = false;
    if(instruction.op == "add") opcode = "iadd";
    else if(instruction.op == "sub") opcode = "isub";
    else if(instruction.op == "mul") opcode = "smul";
    else if(instruction.op == "div") opcode = "sdiv";
    else if(instruction.op == "mod") opcode = "smod";
    else if(instruction.op == "udiv") opcode = "udiv";
    else if(instruction.op == "umod") opcode = "umod";
    else if(instruction.op == "and") opcode = "and";
    else if(instruction.op == "or") opcode = "or";
    else if(instruction.op == "xor") opcode = "xor";
    else if(instruction.op == "shl") { opcode = "lshift"; shift = true; line("move64 z64 x64"); line("move8 x8 z8"); }
    else if(instruction.op == "shr") { opcode = "srshift"; shift = true; line("move64 z64 x64"); line("move8 x8 z8"); }
    else if(instruction.op == "ushr") { opcode = "urshift"; shift = true; line("move64 z64 x64"); line("move8 x8 z8"); }
    else throw LowError("invalid binary operation");
    if(instruction.type.is_float()) opcode = "f" + instruction.op;
    line(opcode + to_string(width) + " " + reg_name('x', width) + " " +
         reg_name('y', width) + " " + (shift ? string("x8") : reg_name('x', width)));
    store_result_scalar(instruction.dest, instruction.type, 'x');
  }

  void emit_compare(const Instruction & instruction)
  {
    if(instruction.type.is_f80()) {
      load_f80_to_scratch(instruction.operands[0], scratch0_);
      load_f80_to_scratch(instruction.operands[1], scratch1_);
      line("f" + instruction.op + "80 z8 " + mem_at("bp", scratch0_) + " " + mem_at("bp", scratch1_));
      emit_canonical_bool(instruction.dest);
      return;
    }
    const int width = instruction.type.width() == 1 ? 8 : instruction.type.width();
    if(width <= 16) {
      if(instruction.operands[0].kind == Value::TEMP || instruction.operands[0].kind == Value::SLOT)
        line("move64 y64 0");
      if(instruction.operands[1].kind == Value::TEMP || instruction.operands[1].kind == Value::SLOT)
        line("move64 x64 0");
    }
    load_scalar(instruction.operands[0], instruction.type, 'y');
    load_scalar(instruction.operands[1], instruction.type, 'x');
    string predicate = instruction.op;
    string opcode;
    if(predicate == "eq") opcode = "eq";
    else if(predicate == "ne") opcode = "ne";
    else if(predicate == "lt") opcode = "lt";
    else if(predicate == "le") opcode = "le";
    else if(predicate == "gt") opcode = "gt";
    else if(predicate == "ge") opcode = "ge";
    else if(predicate == "ult") opcode = "ult";
    else if(predicate == "ule") opcode = "ule";
    else if(predicate == "ugt") opcode = "ugt";
    else if(predicate == "uge") opcode = "uge";
    else throw LowError("invalid comparison predicate");
    if(instruction.type.is_float()) line("f" + opcode + to_string(width) + " z8 " + reg_name('y', width) + " " + reg_name('x', width));
    else {
      string prefix = predicate == "ult" || predicate == "ule" || predicate == "ugt" || predicate == "uge" ? "" :
                      (predicate == "lt" || predicate == "le" || predicate == "gt" || predicate == "ge" ? "s" : "i");
      line(prefix + opcode + to_string(width) + " z8 " + reg_name('y', width) + " " + reg_name('x', width));
    }
    emit_canonical_bool(instruction.dest);
  }

  void emit_convert(const Instruction & instruction)
  {
    const Type & dst = instruction.type; const Type & src = instruction.source_type;
    if(dst.is_float() && src.is_float()) {
      if(src.is_f80() || dst.is_f80()) {
        if(src.is_f80()) load_f80_to_scratch(instruction.operands[0], scratch0_);
        else {
          load_scalar(instruction.operands[0], src, 'x');
          line(src.text + "convf80 " + mem_at("bp", scratch0_) + " " + reg_name('x', src.width()));
          emit_f80_pad(scratch0_);
        }
        if(dst.is_f80()) store_f80_from_scratch(instruction.dest, scratch0_);
        else {
          line("f80conv" + dst.text + " " + location_memory(locations_.find(instruction.dest)->second) + " " + mem_at("bp", scratch0_));
        }
      } else {
        load_scalar(instruction.operands[0], src, 'x');
        // CY86 performs f32/f64 conversion through f80 storage.
        line(src.text + "convf80 " + mem_at("bp", scratch0_) + " " + reg_name('x', src.width()));
        emit_f80_pad(scratch0_);
        line("f80conv" + dst.text + " " + location_memory(locations_.find(instruction.dest)->second) + " " + mem_at("bp", scratch0_));
      }
      return;
    }
    if(dst.is_float() && src.is_integer()) {
      load_scalar(instruction.operands[0], src, 'x');
      line((instruction.op == "sitofp" ? "s" : "u") + to_string(src.width()) + "convf80 " +
           mem_at("bp", scratch0_) + " x64");
      emit_f80_pad(scratch0_);
      if(dst.is_f80()) store_f80_from_scratch(instruction.dest, scratch0_);
      else line("f80conv" + dst.text + " " + location_memory(locations_.find(instruction.dest)->second) + " " + mem_at("bp", scratch0_));
      return;
    }
    if(dst.is_integer() && src.is_float()) {
      if(src.is_f80()) load_f80_to_scratch(instruction.operands[0], scratch0_);
      else {
        load_scalar(instruction.operands[0], src, 'x');
        line(src.text + "convf80 " + mem_at("bp", scratch0_) + " " + reg_name('x', src.width()));
        emit_f80_pad(scratch0_);
      }
      line(string("f80conv") + (instruction.op == "fptosi" ? "s" : "u") + to_string(dst.width()) + " " +
           location_memory(locations_.find(instruction.dest)->second) + " " + mem_at("bp", scratch0_));
      return;
    }
    // Integer width changes use a shift pair for sign extension and a narrow
    // memory/register move for zero extension and truncation.
    load_scalar(instruction.operands[0], src, 'x');
    if(instruction.op == "sext") {
      const int shift = 64 - src.width();
      line("move8 t8 " + to_string(shift));
      line("lshift64 x64 x64 t8"); line("srshift64 x64 x64 t8");
      store_result_scalar(instruction.dest, dst, 'x');
    } else if(instruction.op == "zext") {
      if(src.width() < 64 && instruction.operands[0].kind != Value::INTEGER &&
         instruction.operands[0].kind != Value::NULLPTR) line("move64 x64 " + reg_name('x', src.width()));
      store_result_scalar(instruction.dest, dst, 'x');
    } else if(instruction.op == "trunc") {
      store_result_scalar(instruction.dest, dst, 'x');
    } else throw LowError("invalid conversion");
  }

  void emit_call(const Instruction & instruction)
  {
    const Value & callee = instruction.operands[0];
    const Function * target = NULL;
    if(callee.kind == Value::GLOBAL) {
      map<string, const Function *>::const_iterator found = function_map_.find(callee.text);
      if(found != function_map_.end()) target = found->second;
    }
    const bool direct_object_return = instruction.type.is_object() || instruction.type.is_f80();
    size_t arg_begin = 1;
    const bool indirect = callee.kind == Value::TEMP || (callee.kind == Value::GLOBAL && !target);
    const size_t arg_count = instruction.operands.size() - arg_begin;
    size_t stack_count = arg_count > 4 ? arg_count - 4 : 0;
    if(indirect) ++stack_count;
    if(indirect) {
      if(callee.kind == Value::GLOBAL && function_map_.find(callee.text) == function_map_.end())
        load_scalar(callee, Type("ptr"), 'x');
      else load_pointer_address(callee, 'x');
    }
    for(size_t i = 0; i < stack_count; ++i) line("isub64 sp sp 8");
    if(indirect) line("move64 [sp] x64");

    if(direct_object_return) {
      const Location result = locations_.find(instruction.dest)->second;
      line("isub64 x64 bp " + to_string(-result.offset));
      line("move64 x64 x64");
    }

    vector<char> registers; registers.push_back('x'); registers.push_back('y'); registers.push_back('z'); registers.push_back('t');
    size_t register_index = direct_object_return ? 1 : 0;
    for(size_t i = 0; i < arg_count; ++i) {
      const Value & argument = instruction.operands[arg_begin + i];
      Type argument_type;
      if(instruction.signature.present && i < instruction.signature.parameters.size()) argument_type = instruction.signature.parameters[i].type;
      else if(target && i < target->parameters.size()) argument_type = target->parameters[i].type;
      else if(argument.kind == Value::TEMP || argument.kind == Value::SLOT) argument_type = location_for(argument).type;
      else argument_type = argument.kind == Value::FLOAT ? Type("f64") : Type("i64");
      const Parameter * parameter = NULL;
      if(instruction.signature.present && i < instruction.signature.parameters.size()) parameter = &instruction.signature.parameters[i];
      else if(target && i < target->parameters.size()) parameter = &target->parameters[i];
      const bool by_address = parameter && parameter->metadata.find("pass") != parameter->metadata.end() &&
        parameter->metadata.find("pass")->second != "direct";
      if(i < 4) {
        const char reg = registers[register_index++];
        if((by_address || argument_type.is_f80() || argument_type.is_object()) &&
           (argument.kind == Value::SLOT || argument.kind == Value::TEMP)) {
          load_addressable(argument, 'x');
          if(reg != 'x') line("move64 " + reg_name(reg, 64) + " x64");
          else line("move64 x64 x64");
        }
        else if(by_address || argument_type.is_f80() || argument_type.is_object()) load_addressable(argument, reg);
        else load_scalar(argument, argument_type, reg);
      } else {
        if(by_address || argument_type.is_f80() || argument_type.is_object()) load_addressable(argument, 'x');
        else load_scalar(argument, argument_type, 'x');
        line("move64 [sp] 0");
        line("move64 [sp] x64");
      }
    }
    if(indirect) {
      line("call [sp]");
    } else line("call " + output_label_function(callee.text));
    for(size_t i = 0; i < stack_count; ++i) line("iadd64 sp sp 8");
    if(!instruction.dest.empty() && !direct_object_return && instruction.type != Type("void"))
      store_result_scalar(instruction.dest, instruction.type, 'x');
  }

  void emit_atomic_compare_exchange(const Instruction & instruction)
  {
    load_pointer_address(instruction.operands[0], 'y');
    load_pointer_address(instruction.operands[1], 'z');
    line("move64 t64 [y64]");
    line("move64 x64 [z64]");
    line("ieq64 x8 t64 x64");
    const int serial = (*eh_serial_)++; // shared monotonically increasing internal labels are not EH-only.
    const string success = "__atomic_cmpxchg_success__" + to_string(serial);
    const string end = "__atomic_cmpxchg_end__" + to_string((*eh_serial_)++);
    line("jumpif x8 " + success);
    line("move64 [z64] t64"); line("move64 x64 0"); store_result_scalar(instruction.dest, Type("i64"), 'x');
    line("jump " + end);
    label(success);
    load_scalar(instruction.operands[2], instruction.type, 'x');
    line("move64 [y64] x64"); line("move64 x64 1"); store_result_scalar(instruction.dest, Type("i64"), 'x');
    label(end);
  }

  void emit_eh_dispatch()
  {
    const int serial = (*eh_serial_)++;
    const string handler = "__eh_handler__" + to_string(serial);
    const string unhandled = "__eh_unhandled__" + to_string((*eh_serial_)++);
    line("move64 x64 [g____cppgm_eh_top]");
    line("ieq64 z8 x64 0");
    line("jumpif z8 " + unhandled);
    label(handler);
    line("move64 y64 [x64]");
    line("move64 [g____cppgm_eh_top] y64");
    line("move64 z64 [x64+8]");
    line("move64 bp [x64+16]");
    line("move64 sp [x64+24]");
    line("jump z64");
    label(unhandled);
    line("move64 x64 [g____cppgm_eh_value]");
    line("call fn____cppgm_eh_unhandled");
    line("syscall1 t64 60 x64");
    output_.push_back("");
  }

  void emit_eh_setup(const string & target)
  {
    line("isub64 sp sp 32");
    line("move64 z64 [g____cppgm_eh_top]");
    line("move64 [sp] z64");
    line("move64 z64 " + output_label_function(function_.name) + "__" + target);
    line("move64 [sp+8] z64");
    line("move64 [sp+16] bp");
    line("move64 z64 sp");
    line("iadd64 z64 z64 32");
    line("move64 [sp+24] z64");
    line("move64 z64 sp");
    line("move64 [g____cppgm_eh_top] z64");
  }

  void emit_eh_end()
  {
    line("move64 x64 [g____cppgm_eh_top]");
    line("move64 y64 [x64]");
    line("move64 [g____cppgm_eh_top] y64");
    line("move64 sp x64");
    line("iadd64 sp sp 32");
  }

  void emit_instruction(const Instruction & instruction)
  {
    switch(instruction.kind) {
      case IK_CONST:
        if(instruction.type.is_f80()) {
          line("move80 " + mem_at("bp", scratch0_) + " " + f80_literal(immediate(instruction.operands[0])));
          emit_f80_pad(scratch0_); store_f80_from_scratch(instruction.dest, scratch0_);
        } else { load_scalar(instruction.operands[0], instruction.type, 'x'); store_result_scalar(instruction.dest, instruction.type, 'x'); }
        break;
      case IK_COPY:
        if(instruction.type.is_f80()) { load_f80_to_scratch(instruction.operands[0], scratch0_); store_f80_from_scratch(instruction.dest, scratch0_); }
        else { load_scalar(instruction.operands[0], instruction.type, 'x'); store_result_scalar(instruction.dest, instruction.type, 'x'); }
        break;
      case IK_ADDR:
        load_addressable(instruction.operands[0], 'x'); store_result_scalar(instruction.dest, Type("ptr"), 'x'); break;
      case IK_LOAD:
      case IK_ATOMIC_LOAD:
        if(instruction.type.is_f80()) { load_f80_storage(instruction.operands[0], scratch0_); store_f80_from_scratch(instruction.dest, scratch0_); }
        else if(instruction.operands[0].kind == Value::GLOBAL) { load_scalar(instruction.operands[0], instruction.type, 'x'); store_result_scalar(instruction.dest, instruction.type, 'x'); }
        else if(instruction.operands[0].kind == Value::SLOT) { const Location l = location_for(instruction.operands[0]); line("move" + to_string(instruction.type.width()) + " " + reg_name('x', instruction.type.width()) + " " + location_memory(l)); store_result_scalar(instruction.dest, instruction.type, 'x'); }
        else {
          const char pointer_reg = instruction.kind == IK_ATOMIC_LOAD ? 'y' : 'x';
          load_pointer_address(instruction.operands[0], pointer_reg);
          line("move" + to_string(instruction.type.width()) + " " + reg_name('x', instruction.type.width()) + " [" + reg_name(pointer_reg, 64) + "]");
          store_result_scalar(instruction.dest, instruction.type, 'x');
        }
        break;
      case IK_STORE:
      case IK_ATOMIC_STORE:
        if(instruction.type.is_f80()) { load_f80_to_scratch(instruction.operands[0], scratch0_); store_f80_storage(instruction.operands[1], scratch0_); }
        else {
          if(instruction.operands[1].kind == Value::GLOBAL) { load_scalar(instruction.operands[0], instruction.type, 'x'); line("move" + to_string(instruction.type.width()) + " [" + output_label_global(instruction.operands[1].text) + "] " + reg_name('x', instruction.type.width())); }
          else if(instruction.operands[1].kind == Value::SLOT) { load_scalar(instruction.operands[0], instruction.type, 'x'); line("move" + to_string(instruction.type.width()) + " " + location_memory(location_for(instruction.operands[1])) + " " + reg_name('x', instruction.type.width())); }
          else {
            if(instruction.kind == IK_ATOMIC_STORE) {
              load_pointer_address(instruction.operands[1], 'y');
              load_scalar(instruction.operands[0], instruction.type, 'x');
            } else {
              load_scalar(instruction.operands[0], instruction.type, 'x');
              load_pointer_address(instruction.operands[1], 'y');
            }
            line("move" + to_string(instruction.type.width()) + " [y64] " + reg_name('x', instruction.type.width()));
          }
        }
        break;
      case IK_INDEX: emit_index(instruction); break;
      case IK_UNARY:
        if(instruction.op == "decay") { load_scalar(instruction.operands[0], Type("ptr"), 'x'); store_result_scalar(instruction.dest, Type("ptr"), 'x'); }
        else if(instruction.type.is_f80()) {
          load_f80_to_scratch(instruction.operands[0], scratch0_);
          if(instruction.op == "neg") {
            line("move80 " + mem_at("bp", scratch1_) + " 0.0L"); emit_f80_pad(scratch1_);
            line("fsub80 " + mem_at("bp", scratch2_) + " " + mem_at("bp", scratch1_) + " " + mem_at("bp", scratch0_));
            emit_f80_pad(scratch2_); store_f80_from_scratch(instruction.dest, scratch2_);
          } else throw LowError("unsupported f80 unary op");
        } else if(instruction.op == "neg") {
          load_scalar(instruction.operands[0], instruction.type, 'x'); line("move64 y64 0");
          line("isub" + to_string(instruction.type.width()) + " " + reg_name('x', instruction.type.width()) + " " + reg_name('y', instruction.type.width()) + " " + reg_name('x', instruction.type.width()));
          store_result_scalar(instruction.dest, instruction.type, 'x');
        } else if(instruction.op == "not") {
          load_scalar(instruction.operands[0], instruction.type, 'x');
          line("ieq" + to_string(instruction.type.width()) + " z8 " + reg_name('x', instruction.type.width()) + " 0");
          emit_canonical_bool(instruction.dest);
        } else if(instruction.op == "bitnot") {
          load_scalar(instruction.operands[0], instruction.type, 'x');
          line("not" + to_string(instruction.type.width()) + " " + reg_name('x', instruction.type.width()) + " " + reg_name('x', instruction.type.width()));
          store_result_scalar(instruction.dest, instruction.type, 'x');
        } else if(instruction.op == "bswap") {
          const int width = instruction.type.width();
          if(width == 16) { line("move64 x64 0"); load_scalar(instruction.operands[0], instruction.type, 'x'); line("bswap16 x16 x16"); line("move16 " + location_memory(locations_.find(instruction.dest)->second) + " x16"); }
          else { load_scalar(instruction.operands[0], instruction.type, 'x'); line("bswap" + to_string(width) + " " + reg_name('x', width) + " " + reg_name('x', width)); store_result_scalar(instruction.dest, instruction.type, 'x'); }
        } else throw LowError("invalid unary operation");
        break;
      case IK_BINARY: emit_binary(instruction); break;
      case IK_CMP: emit_compare(instruction); break;
      case IK_CONVERT: emit_convert(instruction); break;
      case IK_CALL: emit_call(instruction); break;
      case IK_COPYOBJ: emit_copy_bytes(instruction.operands[0], instruction.operands[1], instruction.bytes, is_direct_object_value(instruction.operands[0])); break;
      case IK_ZEROINIT: emit_zero_bytes(instruction.operands[0], instruction.bytes); break;
      case IK_ATOMIC_EXCHANGE:
        load_pointer_address(instruction.operands[0], 'y'); load_scalar(instruction.operands[1], instruction.type, 'x');
        line("move64 t64 [y64]"); line("move64 [y64] x64"); line("move64 x64 0"); line("move64 x64 t64");
        store_result_scalar(instruction.dest, instruction.type, 'x'); break;
      case IK_ATOMIC_ADD_FETCH:
        load_pointer_address(instruction.operands[0], 'y');
        line("move64 x64 [y64]"); load_scalar(instruction.operands[1], instruction.type, 'z');
        line("iadd64 x64 x64 z64"); line("move64 [y64] x64"); store_result_scalar(instruction.dest, instruction.type, 'x'); break;
      case IK_ATOMIC_COMPARE_EXCHANGE: emit_atomic_compare_exchange(instruction); break;
      case IK_ATOMIC_THREAD_FENCE:
      case IK_ATOMIC_SIGNAL_FENCE: break;
      case IK_JUMP: line("jump " + output_label_function(function_.name) + "__" + instruction.label); break;
      case IK_BRANCH:
        load_scalar(instruction.operands[0], Type("i64"), 'x'); line("ieq64 z8 x64 0");
        line("jumpif z8 " + output_label_function(function_.name) + "__" + instruction.label2);
        line("jump " + output_label_function(function_.name) + "__" + instruction.label); break;
      case IK_SWITCH:
        load_scalar(instruction.operands[0], Type("i64"), 'x');
        for(size_t i = 0; i < instruction.cases.size(); ++i) {
          load_scalar(instruction.cases[i].first, Type("i64"), 't');
          line("ieq64 z8 x64 t64");
          line("jumpif z8 " + output_label_function(function_.name) + "__" + instruction.cases[i].second);
        }
        line("jump " + output_label_function(function_.name) + "__" + instruction.label); break;
      case IK_RETURN:
        if(instruction.type.is_void()) { line("jump " + output_label_function(function_.name) + "__epilogue"); break; }
        if(instruction.type.is_f80()) {
          const Location source = location_for(instruction.operands[0]);
          const int temp = f80_return_temp_;
          line("isub64 x64 bp " + to_string(-source.offset));
          line("move64 z64 [x64]"); line("move64 " + mem_at("bp", temp) + " z64");
          line("move64 z64 [x64+8]"); line("move64 " + mem_at("bp", temp + 8) + " z64");
          line("move64 x64 " + location_memory(Location(Type("ptr"), hidden_return_offset_)));
          line("move64 z64 " + mem_at("bp", temp)); line("move64 [x64] z64");
          line("move64 z64 " + mem_at("bp", temp + 8)); line("move64 [x64+8] z64");
        } else if(instruction.type.is_object()) {
          load_addressable(instruction.operands[0], 'x');
          line("move64 y64 " + location_memory(Location(Type("ptr"), hidden_return_offset_)));
          line("move64 z64 [x64]"); line("move64 [y64] z64");
          size_t copied = 8;
          while(copied < instruction.type.object_bytes) {
            line("move64 z64 [x64+" + to_string(copied) + "]");
            line("move64 [y64+" + to_string(copied) + "] z64"); copied += 8;
          }
        } else { load_scalar(instruction.operands[0], instruction.type, 'x'); }
        line("jump " + output_label_function(function_.name) + "__epilogue"); break;
      case IK_EH_TRY:
      case IK_EH_CLEANUP: emit_eh_setup(instruction.label); break;
      case IK_EH_END: emit_eh_end(); break;
      case IK_THROW:
        load_scalar(instruction.operands[0], instruction.type, 'x'); line("move64 [g____cppgm_eh_value] x64"); emit_eh_dispatch(); break;
      case IK_RESUME: emit_eh_dispatch(); break;
      case IK_EXCEPTION: line("move64 x64 [g____cppgm_eh_value]"); store_result_scalar(instruction.dest, instruction.type, 'x'); break;
      case IK_EH_CATCH:
      case IK_EH_FILTER:
      case IK_EH_CATCH_ALL: break;
    }
  }

  void setup_locations()
  {
    next_offset_ = 0; locations_.clear(); slots_.clear(); params_.clear(); hidden_return_offset_ = 0;
    const bool hidden_return = function_.return_type.is_f80() || function_.return_type.is_object();
    if(hidden_return) hidden_return_offset_ = allocate(Type("ptr"));
    for(size_t i = 0; i < function_.parameters.size(); ++i) {
      const Location location(function_.parameters[i].type, allocate(function_.parameters[i].type));
      params_[function_.parameters[i].name] = location; locations_[function_.parameters[i].name] = location;
    }
    for(size_t i = 0; i < function_.slots.size(); ++i) {
      const Location location(function_.slots[i].second, allocate(function_.slots[i].second));
      slots_[function_.slots[i].first] = location;
    }
    for(size_t b = 0; b < function_.blocks.size(); ++b) {
      for(size_t i = 0; i < function_.blocks[b].instructions.size(); ++i) {
        const Instruction & instruction = function_.blocks[b].instructions[i];
        if(instruction.dest.empty()) continue;
        Type type = instruction.type;
        if(instruction.kind == IK_CMP || instruction.kind == IK_ATOMIC_COMPARE_EXCHANGE) type = Type("i64");
        if(instruction.kind == IK_ADDR || instruction.kind == IK_INDEX) type = Type("ptr");
        if(type.is_void()) continue;
        if(locations_.find(instruction.dest) == locations_.end())
          locations_[instruction.dest] = Location(type, allocate(type));
      }
    }
    bool f80 = function_.return_type.is_f80();
    bool integer_conversion_scratch = false;
    for(map<string, Location>::const_iterator it = locations_.begin(); it != locations_.end(); ++it)
      if(it->second.type.is_f80()) f80 = true;
    for(size_t b = 0; b < function_.blocks.size(); ++b) {
      for(size_t i = 0; i < function_.blocks[b].instructions.size(); ++i) {
        if(function_.blocks[b].instructions[i].source_type.is_f80() || function_.blocks[b].instructions[i].type.is_f80()) f80 = true;
        if(function_.blocks[b].instructions[i].kind == IK_CONVERT &&
           (function_.blocks[b].instructions[i].op == "sext" || function_.blocks[b].instructions[i].op == "zext" ||
            function_.blocks[b].instructions[i].op == "trunc")) integer_conversion_scratch = true;
      }
    }
    f80_return_temp_ = 0;
    if(function_.return_type.is_f80()) f80_return_temp_ = allocate(Type("f80"));
    has_f80_scratch_ = f80 && !function_.return_type.is_f80();
    if(has_f80_scratch_) {
      scratch0_ = -(next_offset_ + 16); scratch1_ = -(next_offset_ + 32); scratch2_ = -(next_offset_ + 48);
      frame_size_ = next_offset_ + 64;
    } else if(function_.return_type.is_f80()) {
      scratch0_ = scratch1_ = scratch2_ = 0; frame_size_ = next_offset_ + 48;
    } else { scratch0_ = scratch1_ = scratch2_ = 0; frame_size_ = next_offset_ + (integer_conversion_scratch ? 64 : 0); }
  }

  void emit_param_spills()
  {
    size_t incoming = 0;
    if(hidden_return_offset_ != 0) {
      line("move64 " + location_memory(Location(Type("ptr"), hidden_return_offset_)) + " x64"); incoming = 1;
    }
    const vector<char> regs = {'x', 'y', 'z', 't'};
    for(size_t i = 0; i < function_.parameters.size(); ++i, ++incoming) {
      const Location location = params_[function_.parameters[i].name];
      const bool stack_source = incoming >= regs.size();
      const string source = stack_source ? string("x64") : reg_name(regs[incoming], 64);
      if(stack_source) line("move64 x64 " + mem_at("bp", 16 + static_cast<int>((incoming - 4) * 8)));
      if(location.type.is_f80()) {
        line("move64 x64 " + source);
        line("move64 z64 [x64]"); line("move64 " + mem_at("bp", location.offset) + " z64");
        line("move64 z64 [x64+8]"); line("move64 " + mem_at("bp", location.offset + 8) + " z64");
      } else if(location.type.is_object()) {
        line("move64 x64 " + source);
        size_t copied = 0;
        while(copied < location.type.object_bytes) {
          line("move64 z64 [x64+" + to_string(copied) + "]");
          line("move64 " + mem_at("bp", location.offset + static_cast<int>(copied)) + " z64"); copied += 8;
        }
      } else line("move64 " + location_memory(location) + " " + source);
    }
  }

public:
  FunctionEmitter(const Program & program, const Function & function,
                  const map<string, const Function *> & functions,
                  const map<string, const Global *> & globals, int * serial)
    : program_(program), function_(function), function_map_(functions), global_map_(globals), locations_(), slots_(), params_(),
      next_offset_(0), frame_size_(0), has_f80_scratch_(false), scratch0_(0), scratch1_(0), scratch2_(0),
      f80_return_temp_(0), hidden_return_offset_(0), output_(), eh_serial_(serial) { setup_locations(); }

  vector<string> emit()
  {
    label(output_label_function(function_.name));
    line("isub64 sp sp 8"); line("move64 [sp] bp"); line("move64 bp sp");
    if(frame_size_ > 0) line("isub64 sp sp " + to_string(frame_size_));
    emit_param_spills();
    for(size_t b = 0; b < function_.blocks.size(); ++b) {
      label(output_label_function(function_.name) + "__" + function_.blocks[b].name);
      for(size_t i = 0; i < function_.blocks[b].instructions.size(); ++i) emit_instruction(function_.blocks[b].instructions[i]);
    }
    label(output_label_function(function_.name) + "__epilogue");
    line("move64 sp bp"); line("move64 bp [sp]"); line("iadd64 sp sp 8"); line("ret");
    return output_;
  }
};

static string format_signed_word(uint64_t value)
{
  return to_string(static_cast<int64_t>(value));
}

static vector<unsigned char> f80_bytes(const string & literal)
{
  string value = literal;
  if(!value.empty() && (value[value.size() - 1] == 'L' || value[value.size() - 1] == 'l')) value.erase(value.size() - 1);
  char * end = NULL;
  const long double number = strtold(value.c_str(), &end);
  if(!end || *end != '\0') throw LowError("invalid f80 literal");
  vector<unsigned char> bytes(16, 0);
  const size_t copy = min(sizeof(number), bytes.size());
  memcpy(&bytes[0], &number, copy);
  for(size_t i = 10; i < bytes.size(); ++i) bytes[i] = 0;
  return bytes;
}

static size_t data_alignment(const Type & type)
{
  if(type.is_f80()) return 8;
  return type.storage_alignment();
}

static void emit_data_scalar(vector<string> & output, const Type & type, const string & literal)
{
  const int width = type.width();
  if(type.is_f80()) {
    const vector<unsigned char> bytes = f80_bytes(literal);
    uint64_t low = 0; uint16_t high = 0;
    memcpy(&low, &bytes[0], 8); memcpy(&high, &bytes[8], 2);
    output.push_back("\tdata64 " + format_signed_word(low) + ";");
    output.push_back("\tdata16 " + to_string(static_cast<int16_t>(high)) + ";");
    for(int i = 10; i < 16; ++i) output.push_back("\tdata8 " + to_string(bytes[i]) + ";");
  } else {
    const string opcode = "data" + to_string(width == 1 ? 8 : width);
    output.push_back("\t" + opcode + " " + signed_literal(literal) + ";");
  }
}

static void emit_global_data(vector<string> & output, const Global & global,
                             const map<string, const Function *> & functions,
                             const map<string, const Global *> & globals)
{
  output.push_back(output_label_global(global.name) + ":");
  size_t offset = 0;
  if(!global.structured) {
    if(global.init_kind == Global::INIT_ADDRESS) {
      const string target = functions.find(global.init_symbol) != functions.end() ?
        output_label_function(global.init_symbol) : output_label_global(global.init_symbol);
      output.push_back("\tdata64 " + addend_text(target, global.init_addend) + ";");
      return;
    }
    emit_data_scalar(output, global.type, global.init_kind == Global::INIT_ZERO ? "0" : global.init_literal);
    return;
  }
  for(size_t i = 0; i < global.data.size(); ++i) {
    const DataItem & item = global.data[i];
    if(item.kind == DataItem::ZERO) {
      for(size_t z = 0; z < item.zero_bytes; ++z) output.push_back("\tdata8 0;");
      offset += item.zero_bytes; continue;
    }
    const size_t alignment = data_alignment(item.type);
    while(alignment > 1 && offset % alignment != 0) { output.push_back("\tdata8 0;"); ++offset; }
    if(item.kind == DataItem::ADDRESS) {
      const string target = functions.find(item.symbol) != functions.end() ?
        output_label_function(item.symbol) : output_label_global(item.symbol);
      (void)globals;
      output.push_back("\tdata64 " + addend_text(target, item.addend) + ";");
    }
    else emit_data_scalar(output, item.type, item.literal);
    offset += item.type.is_f80() ? 16 : item.type.storage_size();
  }
}

static vector<string> build_cy86(const Program & program)
{
  map<string, const Function *> functions;
  map<string, const Global *> globals;
  for(size_t i = 0; i < program.functions.size(); ++i) if(!program.functions[i].declaration) functions[program.functions[i].name] = &program.functions[i];
  for(size_t i = 0; i < program.globals.size(); ++i) if(!program.globals[i].declaration) globals[program.globals[i].name] = &program.globals[i];

  string entry, init, fini;
  for(map<string, const Function *>::const_iterator it = functions.begin(); it != functions.end(); ++it) {
    const string role = function_role(*it->second);
    if(role == "entry") entry = it->first;
    else if(role == "init") init = it->first;
    else if(role == "fini") fini = it->first;
  }
  if(entry.empty()) throw LowError("missing LowIR entry function");
  bool has_eh = false;
  for(map<string, const Function *>::const_iterator it = functions.begin(); it != functions.end(); ++it)
    for(size_t b = 0; b < it->second->blocks.size(); ++b)
      for(size_t i = 0; i < it->second->blocks[b].instructions.size(); ++i)
        if(it->second->blocks[b].instructions[i].kind == IK_EH_TRY || it->second->blocks[b].instructions[i].kind == IK_EH_CLEANUP ||
           it->second->blocks[b].instructions[i].kind == IK_THROW || it->second->blocks[b].instructions[i].kind == IK_RESUME ||
           it->second->blocks[b].instructions[i].kind == IK_EXCEPTION) has_eh = true;

  vector<string> output;
  output.push_back("start:"); output.push_back("\tmove64 bp sp;");
  if(!init.empty()) output.push_back("\tcall " + output_label_function(init) + ";");
  output.push_back("\tcall " + output_label_function(entry) + ";");
  if(!fini.empty()) {
    output.push_back("\tisub64 sp sp 8;"); output.push_back("\tmove64 [sp] x64;");
    output.push_back("\tcall " + output_label_function(fini) + ";");
    output.push_back("\tmove64 x64 [sp];"); output.push_back("\tiadd64 sp sp 8;");
  }
  output.push_back("\tsyscall1 t64 60 x64;");
  int serial = 0;
  for(size_t i = 0; i < program.functions.size(); ++i) if(!program.functions[i].declaration) {
    output.push_back("");
    const vector<string> body = FunctionEmitter(program, program.functions[i], functions, globals, &serial).emit();
    output.insert(output.end(), body.begin(), body.end());
  }
  if(has_eh) {
    output.push_back(""); output.push_back("fn____cppgm_eh_unhandled:"); output.push_back("\tsyscall1 t64 60 x64;");
  }
  for(size_t i = 0; i < program.globals.size(); ++i) if(!program.globals[i].declaration) {
    output.push_back(""); emit_global_data(output, program.globals[i], functions, globals);
  }
  if(has_eh) {
    output.push_back(""); output.push_back("g____cppgm_eh_top:"); output.push_back("\tdata64 0;");
    output.push_back(""); output.push_back("g____cppgm_eh_value:"); output.push_back("\tdata64 0;");
  }
  string result;
  for(size_t i = 0; i < output.size(); ++i) { result += output[i]; result += "\n"; }
  return vector<string>(1, result);
}

static vector<string> collect_args(int argc, char ** argv)
{
  vector<string> args;
  for(int i = 1; i < argc; ++i) args.push_back(argv[i]);
  return args;
}

static bool has_help_arg(const vector<string> & args)
{
  for(size_t i = 0; i < args.size(); ++i) if(args[i] == "--help" || args[i] == "-h") return true;
  return false;
}

static void parse_invocation(const vector<string> & args, string & outfile, vector<string> & inputs)
{
  if(args.size() < 3 || args[0] != "-o") throw LowError("invalid usage");
  outfile = args[1]; inputs.assign(args.begin() + 2, args.end());
}

static int run_lowir2cy86(const vector<string> & args)
{
  if(has_help_arg(args)) { cout << lowir2cy86_help_text(); return EXIT_SUCCESS; }
  string outfile; vector<string> inputs; parse_invocation(args, outfile, inputs);
  LowParser parser(read_lines(inputs));
  const Program program = parser.parse();
  Validator validator(program); validator.validate();
  const vector<string> generated = build_cy86(program);
  ofstream output(outfile.c_str());
  if(!output) throw LowError("cannot open CY86 output");
  output << generated[0];
  if(!output) throw LowError("cannot write CY86 output");
  return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char ** argv)
{
  try { return run_lowir2cy86(collect_args(argc, argv)); }
  catch(const exception & error) {
    cerr << "ERROR: " << error.what() << endl;
    return EXIT_FAILURE;
  }
}
