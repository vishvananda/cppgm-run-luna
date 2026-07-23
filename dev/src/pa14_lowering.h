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
string special_member_symbol_name(const TypePtr& owner, const string& name);
string low_symbol_component(const string& name);
bool is_integral_type(const TypePtr& type);
bool is_arithmetic_type(const TypePtr& type);
bool is_floating_type(const TypePtr& type);
bool is_unsigned_type(const TypePtr& type);
string integer_text(long long value);
string strip_literal_suffix(string value);
bool is_user_defined_string_literal(const string& raw);
string string_literal_core(const string& raw);
string string_literal_suffix(const string& raw);
long long parse_integer_literal(const string& raw, bool* okay = 0);
vector<unsigned char> decode_string_literal(const string& raw);
string canonical_literal(const string& raw, TypePtr* type_out = 0,
                         long long* constant = 0, bool* known = 0);
string template_type_mangled_name(const TypePtr& type);

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

  struct InferCacheEntry
  {
    CPPGMAstNodePtr node;
    Scope* scope;
    ExprInfo info;

    InferCacheEntry(const CPPGMAstNodePtr& cached_node = CPPGMAstNodePtr(),
                    Scope* cached_scope = 0, const ExprInfo& cached_info = ExprInfo())
      : node(cached_node), scope(cached_scope), info(cached_info) {}
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
    bool copy_constructor;
    bool move_constructor;
    bool copy_assignment;
    bool move_assignment;
    bool value_special_member;
    bool synthesized_value_member;
    bool hidden_friend;
    bool explicit_constructor;
	bool builtin;
	bool template_instantiation;
	bool member_template;
	bool extern_template;
	bool inline_definition;
	bool object_root;
	bool weak_binding;
	bool defaulted;
	bool deleted;
	bool destructor;
	bool deleting_entry;
	bool needed;
    bool emitted;
    bool variadic;
    bool unwind_no;
    bool noreturn;
    bool base_entry;
    bool indirect_result;
    string effects;
    string object_name;
	string template_primary;
	vector<string> template_arguments;
    string base_entry_for;
    vector<string> parameter_metadata;
    vector<bool> indirect_parameters;
    CPPGMAstNodePtr special_initializer;
    vector<CPPGMAstNodePtr> default_arguments;

    FunctionRecord()
      : node(), scope(), type(), source_type(), member_owner(), qualified_name(),
        symbol(), definition(false), member(false), static_member(false),
        constructor(false), implicit_constructor(false), aggregate_constructor(false),
        copy_constructor(false), move_constructor(false),
        copy_assignment(false), move_assignment(false), value_special_member(false),
        synthesized_value_member(false),
        hidden_friend(false),
        explicit_constructor(false),
		builtin(false),
		template_instantiation(false), member_template(false), extern_template(false), inline_definition(false), object_root(false), weak_binding(false),
        defaulted(false), deleted(false),
		destructor(false), deleting_entry(false), needed(false),
        emitted(false), variadic(false), unwind_no(false), noreturn(false), base_entry(false),
        indirect_result(false), effects(), object_name(), template_primary(), template_arguments(),
        base_entry_for(), parameter_metadata(),
        indirect_parameters(), special_initializer(), default_arguments() {}
  };

  struct GlobalRecord
  {
    CPPGMAstNodePtr node;
    Scope* scope;
    TypePtr type;
    string qualified_name;
    string symbol;
	string object_name;
	TypePtr template_owner;
	bool template_instantiation;
	bool explicit_specialization;
	bool weak_binding;
    CPPGMAstNodePtr initializer;
    bool declaration;
    bool internal;
    bool local_static;
    bool local_static_guard;
    bool thread_local_storage;
    bool tls_guard;
    bool dynamic_initializer;
    bool dynamic_finalizer;

    GlobalRecord()
      : node(), scope(), type(), qualified_name(), symbol(), object_name(), template_owner(),
        template_instantiation(false), explicit_specialization(false), weak_binding(false), initializer(),
        declaration(false), internal(false), local_static(false), local_static_guard(false),
        thread_local_storage(false), tls_guard(false),
        dynamic_initializer(false), dynamic_finalizer(false) {}
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
    bool parameter;
    bool parameter_address;
    bool slot_declared;
    string parameter_operand;
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
	bool conversion;
	// Qualified base calls retain each semantic base step in LowIR.  Ordinary
	// member lookup still uses the canonical aggregate adjustment.
	bool project_base_path;
	bool virtual_dispatch;
	size_t virtual_slot;
	TypePtr virtual_owner;
	int user_defined;
    int worst;
    int total;

    CallChoice()
      : binding(), function(), object(), direct(false), member(false),
		static_member(false), conversion(false), project_base_path(false),
		virtual_dispatch(false),
		virtual_slot(0), virtual_owner(), user_defined(1000000),
        worst(1000000), total(1000000) {}
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
    struct SlotEntry
    {
      bool special;
      string name;
      VariablePlan* variable;

      SlotEntry(bool is_special = false, const string& slot_name = string(),
                VariablePlan* slot_variable = 0)
        : special(is_special), name(slot_name), variable(slot_variable) {}
    };

    struct TemporaryObject
    {
      TypePtr type;
      string address;

      TemporaryObject(const TypePtr& object_type = TypePtr(),
                      const string& object_address = string())
        : type(object_type), address(object_address) {}
    };

    PA14Lowerer* owner;
    FunctionRecord* record;
    deque<VariablePlan> variables;
    map<const CPPGMAstNode*, VariablePlan*> plans;
    vector<string> special_slots;
    map<string, string> special_slot_types;
    vector<SlotEntry> slot_order;
    vector<TemporaryObject> temporary_objects;
    vector<Block> blocks;
    Block* current;
    unsigned int next_temp;
    unsigned int next_label;
    unsigned int next_special;
    vector<map<string, VariablePlan*> > environments;
    VariablePlan* return_slot_plan;
    string return_object_slot;
    map<string, unsigned int> variable_name_counts;
    set<string> reserved_value_names;
    map<const CPPGMAstNode*, string> case_labels;
    set<const CPPGMAstNode*> emitted_cases;
    map<string, string> named_labels;
    vector<string> switch_end_targets;
    vector<string> break_targets;
    vector<string> continue_targets;
	    bool unevaluated_context;

    FunctionState(PA14Lowerer* lowerer, FunctionRecord* function)
      : owner(lowerer), record(function), variables(), plans(), special_slots(),
        special_slot_types(), slot_order(), temporary_objects(), blocks(), current(), next_temp(1), next_label(1),
        next_special(1), environments(), return_slot_plan(0), return_object_slot(),
        variable_name_counts(),
        reserved_value_names(), case_labels(), emitted_cases(), named_labels(),
        switch_end_targets(), break_targets(), continue_targets(),
        unevaluated_context(false) {}
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
	map<const CPPGMAstNode*, GlobalRecord*> local_static_plans_;
	set<string> deferred_static_members_;
	bool needs_init_helper_;
	bool needs_fini_helper_;
	set<const Type*> emitted_vtables_;
	set<const Type*> external_vtables_;
	set<const Type*> emitted_rtti_;
	FunctionState* state_;
	  map<const CPPGMAstNode*, InferCacheEntry> infer_cache_;
	map<const Type*, vector<TypePtr> > friend_owner_index_;
	void IndexFriendOwners();

public:
explicit PA14Lowerer(const vector<CPPGMAstNodePtr>& trees)
;

void Lower(ostream& out);

private:
static string function_key(const string& name, const TypePtr& type);

static string global_key(const string& name);

string low_type(const TypePtr& raw) const;

size_t type_size(const TypePtr& type) const;

size_t type_alignment(const TypePtr& type) const;

string storage_type(const TypePtr& type) const;

string qualified_name(Scope* scope, const string& raw) const;

string TypeQualifiedName(const TypePtr& type) const;

string declarator_name(const CPPGMAstNodePtr& node) const;

TypePtr declared_type(const CPPGMAstNodePtr& node, Scope* scope,
                     Analyzer::SpecFacts* facts = 0);

TypePtr function_type(const TypePtr& raw) const;

  void CollectTopLevel(const CPPGMAstNodePtr& node, Scope* scope);

void InstallBuiltins();

bool HasNoexcept(const CPPGMAstNodePtr& node) const;

bool HasInline(const CPPGMAstNodePtr& node) const;

void CollectClassMembers(const CPPGMAstNodePtr& node, Scope* scope);

void BindClassMember(Binding* binding, bool is_static, const TypePtr& owner) const;

void CollectFunction(const CPPGMAstNodePtr& node, Scope* scope, bool definition);

void CollectLocalStatics(const CPPGMAstNodePtr& node, Scope* scope,
                         const string& function_name);

  void CollectSpecialMember(const CPPGMAstNodePtr& node, Scope* scope, bool definition);

void MarkHiddenFriendDependencies();

void MarkHiddenFriendDependencyNodes(const CPPGMAstNodePtr& node, Scope* scope);

void CollectInheritedConstructors(const TypePtr& owner, Scope* scope);

void EnsureConstructorBaseEntry(FunctionRecord* function);

void CollectImplicitConstructor(const TypePtr& owner, Scope* scope,
                                bool force = false);

void CollectImplicitDestructor(const TypePtr& owner, Scope* scope);

void RememberDefaults(FunctionRecord* record, const CPPGMAstNodePtr& declarator);

void CollectSimpleDeclaration(const CPPGMAstNodePtr& node, Scope* scope);
void CollectSimpleDeclarationItem(const CPPGMAstNodePtr& node, Scope* scope,
                                  const Analyzer::SpecFacts& facts,
                                  const CPPGMAstNodePtr& item);

void CollectGlobalDeclaration(const CPPGMAstNodePtr& node, Scope* scope,
                              const Analyzer::SpecFacts& facts,
                              const CPPGMAstNodePtr& item,
                              const CPPGMAstNodePtr& initializer,
                              const string& name, const TypePtr& type);

void DemandConstantObjectConstructors(const TypePtr& type,
                                      const CPPGMAstNodePtr& initializer);

bool PrepareGlobalDeclaration(const CPPGMAstNodePtr& node, Scope* scope,
                              const Analyzer::SpecFacts& facts,
                              const CPPGMAstNodePtr& initializer,
                              const string& name, const TypePtr& type,
                              GlobalRecord* record);

void StoreGlobalDeclaration(GlobalRecord& record, const TypePtr& record_value);

bool HasStorageSpecifier(const CPPGMAstNodePtr& node, const string& word) const;

void FinalizeSymbols();

string TemplateFunctionObjectName(const FunctionRecord& function) const;

string TemplateGlobalObjectName(const GlobalRecord& global) const;

void PreparePolymorphicModel();

void EmitPolymorphicGlobals(vector<string>& entries);

bool IsVirtualFunction(const FunctionRecord& function) const;

FunctionRecord* EnsureVirtualDestructor(const TypePtr& owner,
                                        const VirtualMethodInfo& slot,
                                        bool deleting);

FunctionRecord* EnsurePureVirtual(const VirtualMethodInfo& slot);

FunctionRecord* VirtualFunctionRecord(const TypePtr& owner,
                                      const VirtualMethodInfo& slot);

string TypeMangledName(const TypePtr& type) const;

string VTableSymbol(const TypePtr& type) const;

string VTableAddressSymbol(const TypePtr& type) const;

TypePtr SemanticType(const Type* raw_type) const;

vector<const Type*> OrderedTypes(const set<const Type*>& types) const;

bool ShouldUseExternalVtable(const TypePtr& type) const;

void EmitVPointerStore(const TypePtr& owner, const string& address);

bool VirtualSlotForCall(const TypePtr& object, Binding* binding,
                        size_t* slot, size_t* semantic_slot = 0) const;

bool VirtualDestructorDeletingSlot(const TypePtr& object,
                                   size_t* slot) const;

bool ContainsVirtualMemberCall(const CPPGMAstNodePtr& node,
                               const FunctionRecord& function);

void CollectStringLiterals(const CPPGMAstNodePtr& node, unsigned int braced_depth = 0,
                           bool local_static_context = false,
                           bool unevaluated_context = false,
                           bool local_array_context = false,
                           bool function_context = false);

FunctionRecord* FindFunction(const string& qname, const TypePtr& type) const;

GlobalRecord* FindGlobal(const string& qname) const;

GlobalRecord* EnsureStaticMemberStorage(Binding* binding, bool force_declaration = false);

void DemandTemplateStaticMembers(const TypePtr& raw_type);

void EnsureThreadLocalGuard(GlobalRecord* object);

void AppendBindings(Scope* scope, const string& name,
                    vector<Binding*>& result, set<Scope*>& visited) const;

vector<Binding*> DirectBindings(Scope* scope, const string& name) const;

Binding* ResolveDecltypeStaticMember(const string& spelling, Scope* scope) const;

vector<Binding*> LookupUnqualifiedAll(Scope* from, const string& name) const;

Scope* ScopeComponent(Scope* current, const string& component,
                      bool first, bool absolute) const;

vector<Binding*> Lookup(const string& raw, Scope* from) const;

vector<Binding*> OperatorCandidates(const string& name,
                                    const vector<ExprInfo>& arguments,
                                    Scope* scope) const;

void AppendAssociatedOperatorBindings(const TypePtr& type, const string& name,
                                      vector<Binding*>& result,
                                      set<const Type*>& visited_types,
                                      set<Scope*>& visited_scopes) const;

Scope* FindTypeOwnerScope(Scope* scope, const TypePtr& type) const;

vector<Binding*> MemberBindings(const TypePtr& object, const string& name) const;

bool IsAccessible(Binding* binding, Scope* scope) const;

void CheckTypeAccess(const CPPGMAstNodePtr& declaration, Scope* scope) const;

Binding* MemberBinding(const CPPGMAstNodePtr& node, Scope* scope,
                       ExprInfo* object_info = 0);

TypePtr expression_value_type(const ExprInfo& info) const;

TypePtr function_target_type(const TypePtr& type) const;

ExprInfo InferLiteral(const CPPGMAstNodePtr& node, const TypePtr& expected,
                      Scope* scope);

ExprInfo InferKeyword(const CPPGMAstNodePtr& node) const;

VariablePlan* FindLocalPlan(const string& name) const;

ExprInfo InferIdentifier(const CPPGMAstNodePtr& node, Scope* scope,
                         const TypePtr& expected) const;

ExprInfo InferMember(const CPPGMAstNodePtr& node, Scope* scope) const;

TypePtr IntegralPromotion(const TypePtr& raw) const;

bool PointerCompatible(const TypePtr& source, const TypePtr& target) const;

bool IsDerivedFrom(const TypePtr& derived, const TypePtr& base) const;

int BaseDistance(const TypePtr& derived, const TypePtr& base) const;

TypePtr CommonType(const TypePtr& left, const TypePtr& right,
                  const string& op = string()) const;

string OperatorFunctionName(const string& op) const;

int ConversionRank(const ExprInfo& source, const TypePtr& target) const;

int ConversionRankToClass(const ExprInfo& source, const TypePtr& target) const;

vector<Binding*> ConversionBindings(const TypePtr& source) const;

Binding* FindConversionOperator(const TypePtr& source, const TypePtr& target,
                                bool allow_explicit, int* rank = 0) const;

Binding* FindContextConversionOperator(const TypePtr& source,
                                       bool allow_explicit,
                                       bool boolean_context) const;

Binding* FindNamedConversionOperator(const TypePtr& source,
                                     const string& spelling, Scope* scope) const;

bool DirectFunctionName(const CPPGMAstNodePtr& callee, Scope* scope) const;

FunctionRecord* RecordForBinding(Binding* binding) const;

FunctionRecord* BaseEntryFor(FunctionRecord* function) const;

FunctionRecord* EnsureAggregateConstructor(const TypePtr& type);

void ClassifySpecialMember(FunctionRecord* record);

bool ClassHasDeclaredValueMember(const TypePtr& type) const;

bool ClassHasDeclaredMoveMember(const TypePtr& type) const;

bool ClassValueNeedsIndirect(const TypePtr& type) const;

bool IsEmptyBaseStorage(const TypePtr& type) const;

bool IsTrivialValueStorage(const TypePtr& type) const;

FunctionRecord* EnsureImplicitCopyConstructor(const TypePtr& type, bool move);

FunctionRecord* EnsureImplicitAssignment(const TypePtr& type, bool move);

FunctionRecord* FindValueMember(const TypePtr& type, bool move, bool assignment) const;

bool ValueOperationDeleted(const TypePtr& type, bool move, bool assignment,
                           FunctionRecord* ignored = 0) const;

void MarkValueMemberDeleted(FunctionRecord* record);

bool EmitObjectTransferAt(const TypePtr& target, const string& destination,
                          const CPPGMAstNodePtr& source, Scope* scope,
                          bool allow_explicit = true,
                          bool implicit_return_move = false);

bool EmitValueSpecialMemberBody(FunctionRecord& function, Scope* scope);

TypePtr SourceReturnType(const FunctionRecord& function) const;

bool LowParameterIsByAddress(const FunctionRecord& function, size_t index) const;

TypePtr LowParameterSourceType(const FunctionRecord& function, size_t index) const;

void BuildFunctionABI(FunctionRecord& function);

bool HasDefaultArgument(Binding* binding, size_t index) const;

bool HasConstructor(const TypePtr& type) const;

bool HasExplicitConstructor(const TypePtr& type) const;

bool HasUserProvidedConstructor(const TypePtr& type) const;

bool HasDefaultInitializationEffects(const TypePtr& type) const;

bool HasDefaultConstructionEffects(const TypePtr& type) const;

bool HasClassArrayMember(const TypePtr& type) const;

bool HasNonstaticMemberFunction(const TypePtr& type) const;

bool HasDestructor(const TypePtr& type) const;

bool DestructorHasEffects(const TypePtr& type) const;

bool IsBitField(Binding* binding, long long* bit_offset = 0,
                long long* bit_width = 0) const;

CallChoice ChooseCall(const CPPGMAstNodePtr& expression, Scope* scope);

CallChoice ChooseOperatorCall(const string& name,
                              const vector<CPPGMAstNodePtr>& arguments,
                              Scope* scope);

CPPGMAstNodePtr MakeMemberCall(const CPPGMAstNodePtr& object,
                               const string& name,
                               const vector<CPPGMAstNodePtr>& arguments) const;

string new_temp();

string new_label(const string& prefix);

string new_special_slot(const string& prefix, const string& type);

void AddInstruction(const string& text);

void Terminate(const string& text);

Block* AddBlock(const string& label);

static bool block_is_terminated(const Block* block);

string parameter_name(const CPPGMAstNodePtr& declarator, size_t index) const;

vector<string> ParameterNames(const FunctionRecord& function) const;

CPPGMAstNodePtr InitializerExpression(const CPPGMAstNodePtr& initializer) const;

long long BracedElementCount(const CPPGMAstNodePtr& initializer) const;

TypePtr PlannedType(const CPPGMAstNodePtr& declaration,
                    const CPPGMAstNodePtr& declarator,
                    Scope* scope, const CPPGMAstNodePtr& initializer);

VariablePlan* AddVariablePlan(const string& name, const TypePtr& type,
                              const CPPGMAstNodePtr& declarator,
                              const CPPGMAstNodePtr& initializer);

void PlanSimpleDeclaration(const CPPGMAstNodePtr& node, Scope* scope);

void PlanCondition(const CPPGMAstNodePtr& condition, Scope* scope);

CPPGMAstNodePtr ChildNamed(const CPPGMAstNodePtr& node, const string& name) const;

void PlanStatement(const CPPGMAstNodePtr& node, Scope* scope);

void PlanFunction(FunctionState& state);

CPPGMAstNodePtr FindDirectReturnExpression(const CPPGMAstNodePtr& node,
                                          unsigned int& count) const;

string FunctionSymbolForBinding(Binding* binding, const TypePtr& fallback = TypePtr()) const;

string GlobalSymbolForBinding(Binding* binding) const;

VariablePlan* LocalForName(const string& name) const;

string StorageForVariable(const VariablePlan& variable) const;

Value ConvertValue(Value value, const TypePtr& target,
                   bool immediate_return = false,
                   bool adjust_derived_pointer = false);

Value EmitConversionOperator(const CPPGMAstNodePtr& node, Scope* scope,
                             const TypePtr& target, bool allow_explicit);

Value EmitContextConversion(const CPPGMAstNodePtr& node, Scope* scope,
                            bool allow_explicit, bool boolean_context);

string EmitTruthValue(const Value& value);

string InternString(const string& raw);

bool FoldInteger(const CPPGMAstNodePtr& node, Scope* scope,
                long long* result, TypePtr* type = 0);

  struct AddressInit
  {
    bool valid;
    bool function;
    string symbol;
    long long addend;
    AddressInit() : valid(false), function(false), symbol(), addend(0) {}
  };

AddressInit StaticAddress(const CPPGMAstNodePtr& expression, Scope* scope);

string GlobalMetadata(bool internal) const;

string GlobalMetadata(const GlobalRecord& global) const;

string RenderStringGlobal(const string& symbol, const string& raw,
                          const vector<unsigned char>& bytes) const;

bool AppendConstantGlobalData(const TypePtr& type, const ConstantValue& value,
                             vector<GlobalDataItem>& items) const;

string RenderGlobal(GlobalRecord& global);

void EmitGlobals(vector<string>& entries, size_t begin = 0,
                bool include_strings = true);

void EmitDeclarations(vector<string>& entries);

Scope* FunctionScope() const;

VariablePlan* BindPlan(const CPPGMAstNodePtr& declarator);

void BindSimpleDeclaration(const CPPGMAstNodePtr& node);

VariablePlan* BindCondition(const CPPGMAstNodePtr& condition);

void EnterEnvironment()
;
void LeaveEnvironment()
;

string emit_load(const string& address, const TypePtr& type);

void emit_store(const TypePtr& type, const string& value, const string& storage);

string local_address(VariablePlan* variable);

string global_address(GlobalRecord* global);

string function_address(FunctionRecord* function);

string EmitArrayDecay(const CPPGMAstNodePtr& node, Scope* scope);

string EmitSubscriptAddress(const CPPGMAstNodePtr& node, Scope* scope);

string EmitPointerOffset(const CPPGMAstNodePtr& node, Scope* scope);

string EmitAddress(const CPPGMAstNodePtr& node, Scope* scope);

string EmitOperatorAddress(const CPPGMAstNodePtr& node, Scope* scope);

string EmitCallAddress(const CPPGMAstNodePtr& node, Scope* scope);

string EmitLiteralAddress(const CPPGMAstNodePtr& node);

string EmitMemberAddress(const CPPGMAstNodePtr& node, Scope* scope,
                         bool reference_projection = false);

string AdjustBaseAddress(const string& base, const TypePtr& derived,
                         const TypePtr& target,
                         bool project_base_path = false);

Value EmitIdentifier(const CPPGMAstNodePtr& node, Scope* scope,
                     const TypePtr& expected);

Value EmitUnary(const CPPGMAstNodePtr& node, Scope* scope,
                const TypePtr& expected);

void StoreLValue(const CPPGMAstNodePtr& node, Scope* scope,
                 const TypePtr& type, const string& value);

Value EmitBitFieldLoad(Binding* binding, const string& address,
                       const TypePtr& type, bool copy_result);

string PrepareBitFieldValue(Binding* binding, const TypePtr& type,
                            const string& value);

string MergeBitFieldValue(Binding* binding, const string& address,
                          const TypePtr& type, const string& value,
                          bool preserve);

void StoreBitField(Binding* binding, const string& address,
                   const TypePtr& type, const string& value,
                   bool initializing = false);

Value EmitAssignment(const CPPGMAstNodePtr& node, Scope* scope);

Value EmitUpdate(const CPPGMAstNodePtr& node, Scope* scope, bool address_only);

Value EmitCompare(const CPPGMAstNodePtr& node, Scope* scope);

Value EmitBinary(const CPPGMAstNodePtr& node, Scope* scope);

string EmitReferenceArgument(const CPPGMAstNodePtr& node, Scope* scope,
                             const TypePtr& target);

Value EmitObjectValueArgument(const CPPGMAstNodePtr& node, Scope* scope,
                              const TypePtr& target);

Value EmitCall(const CPPGMAstNodePtr& node, Scope* scope);

Value EmitChosenCall(const CallChoice& choice,
                     const CPPGMAstNodePtr& callee,
                     const vector<CPPGMAstNodePtr>& arguments,
                     Scope* scope,
                     const string& indirect_destination = string());

Value EmitOperatorCall(const string& name,
                       const vector<CPPGMAstNodePtr>& arguments,
                       Scope* scope);

Value EmitConditionalValue(const CPPGMAstNodePtr& node, Scope* scope,
                            const TypePtr& expected);

string EmitConditionalAddress(const CPPGMAstNodePtr& node, Scope* scope);

string EmitLogicalRightTruth(const CPPGMAstNodePtr& node, Scope* scope);

Value EmitLogicalValue(const CPPGMAstNodePtr& node, Scope* scope);

void EmitCondition(const CPPGMAstNodePtr& node, Scope* scope,
                   const string& true_label, const string& false_label);

Value EmitValue(const CPPGMAstNodePtr& node, Scope* scope,
                const TypePtr& expected = TypePtr());

Value EmitNewExpression(const CPPGMAstNodePtr& node, Scope* scope, const TypePtr& expected = TypePtr());
Value EmitDeleteExpression(const CPPGMAstNodePtr& node, Scope* scope);

Value ValueFromInfo(const ExprInfo& info) const;

Value ValueWithNullptr() const;

void EmitInitializer(VariablePlan* variable, const CPPGMAstNodePtr& initializer,
                     Scope* scope);

bool EmitObjectConstructor(VariablePlan* variable, const TypePtr& object_type,
                           const vector<CPPGMAstNodePtr>& arguments, Scope* scope,
                           bool allow_explicit = true);

bool EmitConstructorAt(const TypePtr& object_type, const string& address,
                       const vector<CPPGMAstNodePtr>& arguments, Scope* scope,
                       bool allow_explicit = true, bool base_entry = false,
                       bool allow_aggregate = false, bool force_move = false,
                       bool value_initialization = false);

string EmitTemporaryObjectAddress(const CPPGMAstNodePtr& node, Scope* scope,
                                  const string& prefix);

void RegisterTemporaryObject(const TypePtr& type, const string& address);

void EmitTemporaryDestructors(size_t mark, Scope* scope);

bool EmitDestructorAt(const TypePtr& object_type, const string& address, Scope* scope,
                      bool force_empty = false);

void EmitConstructorInitializers(FunctionRecord& function, Scope* scope);

void EmitDestructorBody(FunctionRecord& function, Scope* scope);

void EmitLiveDestructors(Scope* scope);

void EmitAggregateConstructorBody(FunctionRecord& function, Scope* scope);

void EmitAggregateAt(const string& base, const TypePtr& type,
                     const CPPGMAstNodePtr& expression, Scope* scope,
                     const CPPGMAstNodePtr& refresh_node = CPPGMAstNodePtr(),
                     long long refresh_offset = -1,
                     bool direct_first_field = false);

void EmitAggregateArrayAt(const string& base, const TypePtr& type,
                          const CPPGMAstNodePtr& expression, Scope* scope,
                          const CPPGMAstNodePtr& refresh_node = CPPGMAstNodePtr(),
                          long long refresh_offset = -1);

void EmitAggregateClassFields(const string& base, const TypePtr& type,
                              const CPPGMAstNodePtr& expression, Scope* scope,
                              const CPPGMAstNodePtr& refresh_node,
                              size_t* child_index,
                              bool direct_first_field = false);

bool EmitAggregateClassArrayField(const string& base, const TypePtr& type,
                                  const CPPGMAstNodePtr& child, Scope* scope,
                                  const CPPGMAstNodePtr& refresh_node,
                                  bool refresh_field_base, long long offset);

void EmitAggregateClassDefaults(const string& base, const TypePtr& type,
                                const CPPGMAstNodePtr& expression, Scope* scope,
                                const CPPGMAstNodePtr& refresh_node,
                                size_t child_index,
                                bool direct_first_field = false);

bool HasNonSizeofReference(const CPPGMAstNodePtr& node,
                           const string& name, bool inside_sizeof = false) const;

bool StatementTerminates(const CPPGMAstNodePtr& node) const;

void EmitReturn(const CPPGMAstNodePtr& node, Scope* scope);

void EmitIf(const CPPGMAstNodePtr& node, Scope* scope);

void EmitWhile(const CPPGMAstNodePtr& node, Scope* scope);

void EmitDo(const CPPGMAstNodePtr& node, Scope* scope);

void EmitFor(const CPPGMAstNodePtr& node, Scope* scope);

void CollectCaseNodes(const CPPGMAstNodePtr& node,
                      vector<CPPGMAstNodePtr>& cases) const;

void CollectNamedLabels(const CPPGMAstNodePtr& node,
                        vector<string>& labels) const;

bool HasBlockLabel(const string& label) const;

void EmitCaseLabelAndBody(const CPPGMAstNodePtr& node, Scope* scope);

void EmitSwitchBody(const CPPGMAstNodePtr& node, Scope* scope);

void EmitSwitch(const CPPGMAstNodePtr& node, Scope* scope);

void EmitDiscard(const CPPGMAstNodePtr& node, Scope* scope);

void EmitStatement(const CPPGMAstNodePtr& node, Scope* scope);

string EmitFunction(FunctionRecord& function);

void EmitDynamicInitializers(vector<string>& entries);

void EmitGlobalInitializer(GlobalRecord& global, Scope* scope);

void EmitLocalStaticInitialization(VariablePlan* variable, Scope* scope);

void EmitGlobalFinalizer(GlobalRecord& global, Scope* scope);

ExprInfo InferCall(const CPPGMAstNodePtr& node, Scope* scope);

TypePtr ConstructorObjectType(const CPPGMAstNodePtr& callee, Scope* scope) const;

TypePtr BuiltinCastType(const CPPGMAstNodePtr& callee, Scope* scope) const;

ExprInfo InferUnary(const CPPGMAstNodePtr& node, Scope* scope);

ExprInfo InferBinary(const CPPGMAstNodePtr& node, Scope* scope);

ExprInfo InferSubscript(const CPPGMAstNodePtr& node, Scope* scope);

ExprInfo Infer(const CPPGMAstNodePtr& node, Scope* scope,
              const TypePtr& expected = TypePtr());

ExprInfo InferUncached(const CPPGMAstNodePtr& node, Scope* scope,
                       const TypePtr& expected);

ExprInfo InferSizeofExpression(const CPPGMAstNodePtr& node, Scope* scope);

ExprInfo InferAllocation(const CPPGMAstNodePtr& node, Scope* scope);
};

} // namespace cppgm_pa14_lowering

void EmitPA14LowIR(const std::vector<CPPGMAstNodePtr>& translation_units,
                   std::ostream& out);
