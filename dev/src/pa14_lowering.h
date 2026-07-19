#pragma once

#include <iosfwd>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "pa12_semantics_support.h"

namespace cppgm_pa14_lowering {

using namespace std;

string trim_type_name(const string& name);
bool type_is_reference(const TypePtr& type);
TypePtr type_value(const TypePtr& type);
string last_component(const string& name);
string low_symbol_component(const string& name);
bool is_integral_type(const TypePtr& type);
bool is_arithmetic_type(const TypePtr& type);
bool is_floating_type(const TypePtr& type);
bool is_unsigned_type(const TypePtr& type);
string integer_text(long long value);
string strip_literal_suffix(string value);
long long parse_integer_literal(const string& raw, bool* okay = 0);
vector<unsigned char> decode_string_literal(const string& raw);
string canonical_literal(const string& raw, TypePtr* type_out = 0,
                         long long* constant = 0, bool* known = 0);

class PA14Lowerer
{
  struct ExprInfo
  {
    TypePtr type;
    string operand;
    string category;
    Binding* binding;
    vector<Binding*> candidates;
    bool null_pointer_constant;
    bool known_constant;
    long long constant;

    ExprInfo()
      : type(), category("prvalue"), binding(), candidates(),
        null_pointer_constant(false), known_constant(false), constant(0) {}
  };

  struct FunctionRecord
  {
    CPPGMAstNodePtr node;
    Scope* scope;
    TypePtr type;
    TypePtr source_type;
    TypePtr member_owner;
    string qualified_name;
    string symbol;
    bool definition;
    bool member;
    bool static_member;
    bool constructor;
    bool implicit_constructor;
    bool aggregate_constructor;
    bool destructor;
    bool needed;
    bool emitted;
    bool variadic;
    CPPGMAstNodePtr special_initializer;
    vector<CPPGMAstNodePtr> default_arguments;

    FunctionRecord()
      : node(), scope(), type(), source_type(), member_owner(), qualified_name(),
        symbol(), definition(false), member(false), static_member(false),
        constructor(false), implicit_constructor(false), aggregate_constructor(false),
        destructor(false), needed(false),
        emitted(false), variadic(false),
        special_initializer(), default_arguments() {}
  };

  struct GlobalRecord
  {
    CPPGMAstNodePtr node;
    Scope* scope;
    TypePtr type;
    string qualified_name;
    string symbol;
    CPPGMAstNodePtr initializer;
    bool internal;
    bool dynamic_initializer;
    bool dynamic_finalizer;

    GlobalRecord()
      : node(), scope(), type(), qualified_name(), symbol(), initializer(),
        internal(false), dynamic_initializer(false), dynamic_finalizer(false) {}
  };

  struct VariablePlan
  {
    string source_name;
    string slot_name;
    string initialization_address;
    TypePtr type;
    CPPGMAstNodePtr declarator;
    CPPGMAstNodePtr initializer;
    GlobalRecord* global;
  };

  struct Value
  {
    TypePtr type;
    string operand;
    bool lvalue;
    bool function;
    bool array;
    bool known_constant;
    long long constant;

    Value()
      : type(), operand(), lvalue(false), function(false), array(false),
        known_constant(false), constant(0) {}
  };

  struct CallChoice
  {
    Binding* binding;
    TypePtr function;
    CPPGMAstNodePtr object;
    bool direct;
    bool member;
    bool static_member;
    int worst;
    int total;

    CallChoice()
      : binding(), function(), object(), direct(false), member(false),
        static_member(false), worst(1000000), total(1000000) {}
  };

  struct Block
  {
    string label;
    vector<string> lines;
    bool terminated;

    explicit Block(const string& block_label = string())
      : label(block_label), lines(), terminated(false) {}
  };

  struct FunctionState
  {
    PA14Lowerer* owner;
    FunctionRecord* record;
    deque<VariablePlan> variables;
    map<const CPPGMAstNode*, VariablePlan*> plans;
    vector<string> special_slots;
    map<string, string> special_slot_types;
    vector<Block> blocks;
    Block* current;
    unsigned int next_temp;
    unsigned int next_label;
    unsigned int next_special;
    vector<map<string, VariablePlan*> > environments;
    map<string, unsigned int> variable_name_counts;
    set<string> reserved_value_names;
    map<const CPPGMAstNode*, string> case_labels;
    set<const CPPGMAstNode*> emitted_cases;
    map<string, string> named_labels;
    vector<string> switch_end_targets;
    vector<string> break_targets;
    vector<string> continue_targets;

    FunctionState(PA14Lowerer* lowerer, FunctionRecord* function)
      : owner(lowerer), record(function), variables(), plans(), special_slots(),
        special_slot_types(), blocks(), current(), next_temp(1), next_label(1),
        next_special(1), environments(), variable_name_counts(),
        reserved_value_names(), case_labels(), emitted_cases(), named_labels(),
        switch_end_targets(), break_targets(), continue_targets() {}
  };

  struct GlobalDataItem
  {
    string text;
    explicit GlobalDataItem(const string& value = string()) : text(value) {}
  };

  vector<CPPGMAstNodePtr> trees_;
  CPPGMAstNodePtr program_;
  Analyzer analyzer_;
  deque<FunctionRecord> functions_;
  deque<GlobalRecord> globals_;
  map<string, FunctionRecord*> function_by_key_;
  map<string, GlobalRecord*> global_by_key_;
  map<string, vector<unsigned char> > string_data_;
  map<string, string> string_symbols_;
  vector<string> string_order_;
  bool needs_init_helper_;
  bool needs_fini_helper_;
  FunctionState* state_;

public:
explicit PA14Lowerer(const vector<CPPGMAstNodePtr>& trees)
;

void Lower(ostream& out)

;

private:
static string function_key(const string& name, const TypePtr& type)

;

static string global_key(const string& name)

;

string low_type(const TypePtr& raw) const

;

size_t type_size(const TypePtr& type) const

;

size_t type_alignment(const TypePtr& type) const

;

string storage_type(const TypePtr& type) const

;

string qualified_name(Scope* scope, const string& raw) const

;

string declarator_name(const CPPGMAstNodePtr& node) const

;

TypePtr declared_type(const CPPGMAstNodePtr& node, Scope* scope,
                     Analyzer::SpecFacts* facts = 0)

;

TypePtr function_type(const TypePtr& raw) const

;

void CollectTopLevel(const CPPGMAstNodePtr& node, Scope* scope)

;

void CollectClassMembers(const CPPGMAstNodePtr& node, Scope* scope)

;

void CollectFunction(const CPPGMAstNodePtr& node, Scope* scope, bool definition)

;

void CollectSpecialMember(const CPPGMAstNodePtr& node, Scope* scope, bool definition)

;

void CollectImplicitConstructor(const TypePtr& owner, Scope* scope)

;

void CollectImplicitDestructor(const TypePtr& owner, Scope* scope)

;

void RememberDefaults(FunctionRecord* record, const CPPGMAstNodePtr& declarator)

;

void CollectSimpleDeclaration(const CPPGMAstNodePtr& node, Scope* scope)

;

bool HasStorageSpecifier(const CPPGMAstNodePtr& node, const string& word) const

;

void FinalizeSymbols()

;

void CollectStringLiterals(const CPPGMAstNodePtr& node, unsigned int braced_depth = 0)

;

FunctionRecord* FindFunction(const string& qname, const TypePtr& type) const

;

GlobalRecord* FindGlobal(const string& qname) const

;

void AppendBindings(Scope* scope, const string& name,
                    vector<Binding*>& result, set<Scope*>& visited) const

;

vector<Binding*> DirectBindings(Scope* scope, const string& name) const

;

vector<Binding*> LookupUnqualifiedAll(Scope* from, const string& name) const

;

Scope* ScopeComponent(Scope* current, const string& component,
                      bool first, bool absolute) const

;

vector<Binding*> Lookup(const string& raw, Scope* from) const

;

vector<Binding*> MemberBindings(const TypePtr& object, const string& name) const

;

Binding* MemberBinding(const CPPGMAstNodePtr& node, Scope* scope,
                       ExprInfo* object_info = 0)

;

TypePtr expression_value_type(const ExprInfo& info) const

;

TypePtr function_target_type(const TypePtr& type) const

;

ExprInfo InferLiteral(const CPPGMAstNodePtr& node, const TypePtr& expected) const

;

ExprInfo InferKeyword(const CPPGMAstNodePtr& node) const

;

VariablePlan* FindLocalPlan(const string& name) const

;

ExprInfo InferIdentifier(const CPPGMAstNodePtr& node, Scope* scope,
                         const TypePtr& expected) const

;

ExprInfo InferMember(const CPPGMAstNodePtr& node, Scope* scope) const

;

TypePtr IntegralPromotion(const TypePtr& raw) const

;

bool PointerCompatible(const TypePtr& source, const TypePtr& target) const

;

TypePtr CommonType(const TypePtr& left, const TypePtr& right,
                  const string& op = string()) const

;

int ConversionRank(const ExprInfo& source, const TypePtr& target) const

;

bool DirectFunctionName(const CPPGMAstNodePtr& callee, Scope* scope) const

;

FunctionRecord* RecordForBinding(Binding* binding) const

;

FunctionRecord* EnsureAggregateConstructor(const TypePtr& type)

;

bool HasDefaultArgument(Binding* binding, size_t index) const

;

bool HasConstructor(const TypePtr& type) const

;

bool HasDestructor(const TypePtr& type) const

;

bool IsBitField(Binding* binding, long long* bit_offset = 0,
                long long* bit_width = 0) const

;

CallChoice ChooseCall(const CPPGMAstNodePtr& expression, Scope* scope)

;

string new_temp()

;

string new_label(const string& prefix)

;

string new_special_slot(const string& prefix, const string& type)

;

void AddInstruction(const string& text)

;

void Terminate(const string& text)

;

Block* AddBlock(const string& label)

;

static bool block_is_terminated(const Block* block)

;

string parameter_name(const CPPGMAstNodePtr& declarator, size_t index) const

;

vector<string> ParameterNames(const FunctionRecord& function) const

;

CPPGMAstNodePtr InitializerExpression(const CPPGMAstNodePtr& initializer) const

;

long long BracedElementCount(const CPPGMAstNodePtr& initializer) const

;

TypePtr PlannedType(const CPPGMAstNodePtr& declaration,
                    const CPPGMAstNodePtr& declarator,
                    Scope* scope, const CPPGMAstNodePtr& initializer)

;

VariablePlan* AddVariablePlan(const string& name, const TypePtr& type,
                              const CPPGMAstNodePtr& declarator,
                              const CPPGMAstNodePtr& initializer)

;

void PlanSimpleDeclaration(const CPPGMAstNodePtr& node, Scope* scope)

;

void PlanCondition(const CPPGMAstNodePtr& condition, Scope* scope)

;

CPPGMAstNodePtr ChildNamed(const CPPGMAstNodePtr& node, const string& name) const

;

void PlanStatement(const CPPGMAstNodePtr& node, Scope* scope)

;

void PlanFunction(FunctionState& state)

;

string FunctionSymbolForBinding(Binding* binding, const TypePtr& fallback = TypePtr()) const

;

string GlobalSymbolForBinding(Binding* binding) const

;

VariablePlan* LocalForName(const string& name) const

;

string StorageForVariable(const VariablePlan& variable) const

;

Value ConvertValue(Value value, const TypePtr& target,
                   bool immediate_return = false)

;

string EmitTruthValue(const Value& value)

;

string InternString(const string& raw)

;

bool FoldInteger(const CPPGMAstNodePtr& node, Scope* scope,
                long long* result, TypePtr* type = 0)

;

  struct AddressInit
  {
    bool valid;
    bool function;
    string symbol;
    long long addend;
    AddressInit() : valid(false), function(false), symbol(), addend(0) {}
  };

AddressInit StaticAddress(const CPPGMAstNodePtr& expression, Scope* scope)

;

string GlobalMetadata(bool internal) const

;

string RenderStringGlobal(const string& symbol, const vector<unsigned char>& bytes) const

;

string RenderGlobal(GlobalRecord& global)

;

void EmitGlobals(vector<string>& entries)

;

void EmitDeclarations(vector<string>& entries)

;

Scope* FunctionScope() const

;

VariablePlan* BindPlan(const CPPGMAstNodePtr& declarator)

;

void BindSimpleDeclaration(const CPPGMAstNodePtr& node)

;

VariablePlan* BindCondition(const CPPGMAstNodePtr& condition)

;

void EnterEnvironment()
;
void LeaveEnvironment()
;

string emit_load(const string& address, const TypePtr& type)

;

void emit_store(const TypePtr& type, const string& value, const string& storage)

;

string local_address(VariablePlan* variable)

;

string global_address(GlobalRecord* global)

;

string function_address(FunctionRecord* function)

;

string EmitArrayDecay(const CPPGMAstNodePtr& node, Scope* scope)

;

string EmitSubscriptAddress(const CPPGMAstNodePtr& node, Scope* scope)

;

string EmitPointerOffset(const CPPGMAstNodePtr& node, Scope* scope)

;

string EmitAddress(const CPPGMAstNodePtr& node, Scope* scope)

;

string EmitMemberAddress(const CPPGMAstNodePtr& node, Scope* scope)

;

string AdjustBaseAddress(const string& base, const TypePtr& derived,
                         const TypePtr& target)

;

Value EmitIdentifier(const CPPGMAstNodePtr& node, Scope* scope,
                     const TypePtr& expected)

;

Value EmitUnary(const CPPGMAstNodePtr& node, Scope* scope,
                const TypePtr& expected)

;

void StoreLValue(const CPPGMAstNodePtr& node, Scope* scope,
                 const TypePtr& type, const string& value)

;

Value EmitBitFieldLoad(Binding* binding, const string& address,
                       const TypePtr& type, bool copy_result)

;

string PrepareBitFieldValue(Binding* binding, const TypePtr& type,
                            const string& value)

;

string MergeBitFieldValue(Binding* binding, const string& address,
                          const TypePtr& type, const string& value,
                          bool preserve)

;

void StoreBitField(Binding* binding, const string& address,
                   const TypePtr& type, const string& value,
                   bool initializing = false)

;

Value EmitAssignment(const CPPGMAstNodePtr& node, Scope* scope)

;

Value EmitUpdate(const CPPGMAstNodePtr& node, Scope* scope, bool address_only)

;

Value EmitCompare(const CPPGMAstNodePtr& node, Scope* scope)

;

Value EmitBinary(const CPPGMAstNodePtr& node, Scope* scope)

;

string EmitReferenceArgument(const CPPGMAstNodePtr& node, Scope* scope,
                             const TypePtr& target)

;

Value EmitCall(const CPPGMAstNodePtr& node, Scope* scope)

;

Value EmitConditionalValue(const CPPGMAstNodePtr& node, Scope* scope,
                            const TypePtr& expected)

;

string EmitConditionalAddress(const CPPGMAstNodePtr& node, Scope* scope)

;

string EmitLogicalRightTruth(const CPPGMAstNodePtr& node, Scope* scope)

;

Value EmitLogicalValue(const CPPGMAstNodePtr& node, Scope* scope)

;

void EmitCondition(const CPPGMAstNodePtr& node, Scope* scope,
                   const string& true_label, const string& false_label)

;

Value EmitValue(const CPPGMAstNodePtr& node, Scope* scope,
                const TypePtr& expected = TypePtr())

;

Value ValueFromInfo(const ExprInfo& info) const

;

Value ValueWithNullptr() const

;

void EmitInitializer(VariablePlan* variable, const CPPGMAstNodePtr& initializer,
                     Scope* scope)

;

bool EmitObjectConstructor(VariablePlan* variable, const TypePtr& object_type,
                           const vector<CPPGMAstNodePtr>& arguments, Scope* scope)

;

bool EmitConstructorAt(const TypePtr& object_type, const string& address,
                       const vector<CPPGMAstNodePtr>& arguments, Scope* scope)

;

bool EmitDestructorAt(const TypePtr& object_type, const string& address, Scope* scope)

;

void EmitConstructorInitializers(FunctionRecord& function, Scope* scope)

;

void EmitDestructorBody(FunctionRecord& function, Scope* scope)

;

void EmitLiveDestructors(Scope* scope)

;

void EmitAggregateConstructorBody(FunctionRecord& function, Scope* scope)

;

void EmitAggregateAt(const string& base, const TypePtr& type,
                     const CPPGMAstNodePtr& expression, Scope* scope,
                     const CPPGMAstNodePtr& refresh_node = CPPGMAstNodePtr())

;

void EmitAggregateArrayAt(const string& base, const TypePtr& type,
                          const CPPGMAstNodePtr& expression, Scope* scope)

;

void EmitAggregateClassFields(const string& base, const TypePtr& type,
                              const CPPGMAstNodePtr& expression, Scope* scope,
                              const CPPGMAstNodePtr& refresh_node,
                              size_t* child_index)

;

void EmitAggregateClassDefaults(const string& base, const TypePtr& type,
                                const CPPGMAstNodePtr& expression, Scope* scope,
                                const CPPGMAstNodePtr& refresh_node,
                                size_t child_index)

;

bool HasNonSizeofReference(const CPPGMAstNodePtr& node,
                           const string& name, bool inside_sizeof = false) const

;

bool StatementTerminates(const CPPGMAstNodePtr& node) const

;

void EmitReturn(const CPPGMAstNodePtr& node, Scope* scope)

;

void EmitIf(const CPPGMAstNodePtr& node, Scope* scope)

;

void EmitWhile(const CPPGMAstNodePtr& node, Scope* scope)

;

void EmitDo(const CPPGMAstNodePtr& node, Scope* scope)

;

void EmitFor(const CPPGMAstNodePtr& node, Scope* scope)

;

void CollectCaseNodes(const CPPGMAstNodePtr& node,
                      vector<CPPGMAstNodePtr>& cases) const

;

void CollectNamedLabels(const CPPGMAstNodePtr& node,
                        vector<string>& labels) const

;

bool HasBlockLabel(const string& label) const

;

void EmitCaseLabelAndBody(const CPPGMAstNodePtr& node, Scope* scope)

;

void EmitSwitchBody(const CPPGMAstNodePtr& node, Scope* scope)

;

void EmitSwitch(const CPPGMAstNodePtr& node, Scope* scope)

;

void EmitDiscard(const CPPGMAstNodePtr& node, Scope* scope)

;

void EmitStatement(const CPPGMAstNodePtr& node, Scope* scope)

;

string EmitFunction(FunctionRecord& function)

;

void EmitDynamicInitializers(vector<string>& entries)

;

void EmitGlobalInitializer(GlobalRecord& global, Scope* scope)

;

void EmitGlobalFinalizer(GlobalRecord& global, Scope* scope)

;











ExprInfo InferCall(const CPPGMAstNodePtr& node, Scope* scope)

;

ExprInfo InferUnary(const CPPGMAstNodePtr& node, Scope* scope)

;

ExprInfo InferBinary(const CPPGMAstNodePtr& node, Scope* scope)

;

ExprInfo Infer(const CPPGMAstNodePtr& node, Scope* scope,
              const TypePtr& expected = TypePtr())

;
};


} // namespace cppgm_pa14_lowering

void EmitPA14LowIR(const std::vector<CPPGMAstNodePtr>& translation_units,
                   std::ostream& out);
