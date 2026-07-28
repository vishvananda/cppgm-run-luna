#include "pa11_semantics_analyzer.h"
#include "pa11_semantics_layout.h"
#include <cstdlib>
#include <functional>
void CollectSourceTypeAliasPaths(const CPPGMAstNodePtr& node, const string& scope,
	set<string>* paths);
bool ContainsSourceTypeAliasBase(const CPPGMAstNodePtr& node,
	const set<string>& paths);
namespace {
string AttributeNodeSpelling(const CPPGMAstNodePtr& node)
{
	if (!node) return string();
	if (node->children.empty()) {
		const size_t colon = node->value.find(':');
		if (colon != string::npos &&
			(node->value.compare(0, colon, "TT_") == 0 ||
			 node->value.compare(0, colon, "KW_") == 0 ||
			 node->value.compare(0, colon, "OP_") == 0))
			return node->value.substr(colon + 1);
		return node->value;
	}
	string result;
	for (size_t i = 0; i < node->children.size(); ++i) {
		const string child = AttributeNodeSpelling(node->children[i]);
		if (child.empty()) continue;
		if (!result.empty() && node->kind != "type-id" &&
			node->kind != "type-specifier-seq") result += ' ';
		result += child;
	}
	return result;
}
string StripTemplateArgumentsFromPath(const string& raw)
{
	string result;
	int depth = 0;
	for (size_t i = 0; i < raw.size(); ++i) {
		const char c = raw[i];
		if (c == '<') { ++depth; continue; }
		if (c == '>') { if (depth > 0) --depth; continue; }
		if (depth == 0) result += c;
	}
	return result;
}
CPPGMAstNodePtr FindReturnStatement(const CPPGMAstNodePtr& node)
{
	if (!node) return CPPGMAstNodePtr();
	if (node->kind == "return-statement") return node;
	for (size_t i = 0; i < node->children.size(); ++i) {
		CPPGMAstNodePtr found = FindReturnStatement(node->children[i]);
		if (found) return found;
	}
	return CPPGMAstNodePtr();
}
size_t TopLevelScopeSeparator(const string& raw)
{
	int angle_depth = 0;
	for (size_t i = 0; i + 1 < raw.size(); ++i) {
		if (raw[i] == '<') ++angle_depth;
		else if (raw[i] == '>' && angle_depth > 0) --angle_depth;
		else if (raw[i] == ':' && raw[i + 1] == ':' && angle_depth == 0)
			return i;
	}
	return string::npos;
}
}

Analyzer::Analyzer()
	: global_(new Scope(SCOPE_NAMESPACE, "<global>", 0)), anonymous_type_count_(0),
	  pending_class_layouts_() {}

size_t Analyzer::TypeSize(const TypePtr& type) const
{
	if (!type) return 0;
	switch (type->kind) {
	case TYPE_FUNDAMENTAL: return FundamentalSize(type->name);
	case TYPE_POINTER:
	case TYPE_LVALUE_REFERENCE:
	case TYPE_RVALUE_REFERENCE:
	case TYPE_MEMBER_POINTER: return 8;
	case TYPE_FUNCTION: return 4;
	case TYPE_ARRAY: return type->bound < 0 ? 0 : static_cast<size_t>(type->bound) * TypeSize(type->child);
	case TYPE_ENUM:
		if (!type->complete) throw logic_error("sizeof incomplete enum");
		return type->underlying ? TypeSize(type->underlying) : 4;
	case TYPE_CLASS:
		if (!type->layout_complete && type->owned_scope && type->owned_scope->owner_type &&
			type->owned_scope->owner_type.get() != type.get() &&
			type->owned_scope->owner_type->complete &&
			type->owned_scope->owner_type->layout_complete)
			return type->owned_scope->owner_type->object_size;
		if (!type->complete || !type->layout_complete)
			throw logic_error("sizeof incomplete class");
		return type->object_size;
	case TYPE_TEMPLATE_PARAMETER:
	case TYPE_TEMPLATE_TEMPLATE_PARAMETER: return 0;
	}
	return 0;
}

size_t Analyzer::TypeAlignment(const TypePtr& type) const
{
	if (!type) return 0;
	if (type->kind == TYPE_ARRAY) return TypeAlignment(type->child);
	if (type->kind == TYPE_CLASS && !type->layout_complete && type->owned_scope &&
		type->owned_scope->owner_type && type->owned_scope->owner_type.get() != type.get() &&
		type->owned_scope->owner_type->complete && type->owned_scope->owner_type->layout_complete)
		return type->owned_scope->owner_type->object_alignment;
	if (type->kind == TYPE_CLASS && !type->complete)
		throw logic_error("alignof incomplete class");
	if (type->kind == TYPE_CLASS && type->layout_complete)
		return type->object_alignment;
	if (type->kind == TYPE_ENUM && type->underlying) return TypeAlignment(type->underlying);
	return TypeSize(type);
}

TypePtr Analyzer::ExpressionCallType(const CPPGMAstNodePtr& expression,
	Scope* scope, size_t arity)
{
	if (!expression || expression->children.empty()) return TypePtr();
	const string spelled_name = expression->children[0]->value;
	// The parser keeps an explicitly specialized function template as one
	// id-expression (`test<T>`).  Binding lookup and the constant-template
	// registry are keyed by the callable's unqualified name, so remove only
	// the template argument suffix before selecting its overload.
	const size_t template_open = spelled_name.find('<');
	const string name = template_open == string::npos ? spelled_name :
		spelled_name.substr(0, template_open);
	vector<Binding*> candidates;
	for (Scope* current = scope; current; current = current->parent) {
		vector<Binding*> local;
		for (size_t i = 0; i < current->bindings.size(); ++i)
			if (current->bindings[i].name == name && current->bindings[i].kind == BIND_FUNCTION)
				local.push_back(&current->bindings[i]);
		if (!local.empty()) { candidates = local; break; }
	}
	map<string, vector<Binding*> >::const_iterator templates =
		constant_template_functions_.find(LastComponent(name));
	if (templates != constant_template_functions_.end())
		for (size_t i = 0; i < templates->second.size(); ++i)
			if (find(candidates.begin(), candidates.end(), templates->second[i]) == candidates.end())
				candidates.push_back(templates->second[i]);
	Binding* selected = 0;
	int selected_score = 1000000;
	for (size_t i = 0; i < candidates.size(); ++i) {
		Binding* candidate = candidates[i];
		if (!candidate->type || candidate->type->kind != TYPE_FUNCTION) continue;
		const TypePtr function = candidate->type;
		if ((!function->variadic && function->parameters.size() != arity) ||
			(function->variadic && function->parameters.size() > arity)) continue;
		int score = function->variadic ? 4 : 0;
		bool viable = true;
		for (size_t argument = 0; argument < arity; ++argument) {
			if (argument >= function->parameters.size()) { score += 8; continue; }
			TypePtr actual = ExpressionType(expression->children[1]->children[argument], scope);
			TypePtr formal = function->parameters[argument];
			while (formal && (formal->kind == TYPE_LVALUE_REFERENCE || formal->kind == TYPE_RVALUE_REFERENCE)) formal = formal->child;
			while (actual && (actual->kind == TYPE_LVALUE_REFERENCE || actual->kind == TYPE_RVALUE_REFERENCE)) actual = actual->child;
			const CPPGMAstNodePtr actual_expression = expression->children[1]->children[argument];
			const bool null_pointer_constant = actual_expression && ((actual_expression->kind == "literal" && (actual_expression->value == "0" || actual_expression->value == "0L" || actual_expression->value == "0LL" || actual_expression->value == "0u")) || (actual_expression->kind == "keyword-literal" && actual_expression->value.find("nullptr") != string::npos));
			if (formal && formal->kind == TYPE_TEMPLATE_PARAMETER) score += 10;
			else if (SameTypeIgnoringTopCv(actual, formal)) {} else if (null_pointer_constant && formal && formal->kind == TYPE_POINTER) score += 3;
			else if (actual && formal && actual->kind == TYPE_FUNDAMENTAL && formal->kind == TYPE_FUNDAMENTAL) score += 2;
			else { viable = false; break; }
		}
		if (viable && (!selected || score < selected_score)) {
			selected = candidate;
			selected_score = score;
		}
	}
	return selected ? selected->type->child : TypePtr();
}

void Analyzer::Analyze(const CPPGMAstNodePtr& tree)
{
	if (!tree || tree->kind != "translation-unit") throw logic_error("invalid translation unit");
	PredeclareGeneratedScopes(tree);
	set<string> source_type_alias_paths;
	// This pass precedes ordinary binding construction, so keep only the
	// scoped declaration-path facts needed by the early-layout decision.
	CollectSourceTypeAliasPaths(tree, string(), &source_type_alias_paths);
	// Only function-type local-class replay needs early layout; broad replay alters prior source-order facts.
	for (size_t i = 0; i < tree->children.size(); ++i)
		if (tree->children[i] && tree->children[i]->kind == "class-specifier" &&
			tree->children[i]->template_instantiation) {
			bool function_type_argument = false;
			for (size_t argument = 0;
				argument < tree->children[i]->template_arguments.size(); ++argument)
				if (tree->children[i]->template_arguments[argument].find('(') != string::npos &&
					tree->children[i]->template_arguments[argument].find(')') != string::npos &&
					tree->children[i]->template_arguments[argument].find("(&)") == string::npos) {
					function_type_argument = true;
					break;
				}
			const bool depends_on_source_alias = ContainsSourceTypeAliasBase(
				tree->children[i], source_type_alias_paths);
			if (function_type_argument && !depends_on_source_alias)
				Process(tree->children[i], global_.get());
		}
	for (size_t i = 0; i < tree->children.size(); ++i) Process(tree->children[i], global_.get());
	if (!pending_using_declarations_.empty()) {
		vector<pair<CPPGMAstNodePtr, Scope*> > pending; pending.swap(pending_using_declarations_); processing_pending_using_declarations_ = true;
		for (size_t i = 0; i < pending.size(); ++i) ProcessUsingDeclaration(pending[i].first, pending[i].second);
		processing_pending_using_declarations_ = false; }
	FinishPendingClassLayouts();
}

void Analyzer::Print(ostream& out) const
{
	out << "translation-unit\n";
	PrintScope(global_.get(), out, 1);
}

ConstantValue Analyzer::FromIntegralValue(const PA19IntegralValue& value) const
{
	if (!value.known) return ConstantValue();
	ConstantValue result;
	result.kind = ConstantValue::CONSTANT_INTEGRAL;
	result.integral = value;
	result.value = PA19Signed(value);
	result.type = Fundamental(value.type.name);
	return result;
}

ConstantValue Analyzer::FromFloatingValue(long double value, const TypePtr& type) const
{
	ConstantValue result;
	result.kind = ConstantValue::CONSTANT_FLOATING;
	result.floating_known = true;
	result.floating = value;
	result.type = type ? type : Fundamental("double");
	return result;
}

ConstantValue Analyzer::FromObjectValue(const TypePtr& type,
	const shared_ptr<ConstantObject>& object) const
{
	ConstantValue result;
	result.kind = ConstantValue::CONSTANT_OBJECT;
	result.type = type;
	result.object = object;
	return result;
}

ConstantValue Analyzer::FromPointerValue(const shared_ptr<ConstantPointer>& pointer,
	const TypePtr& type) const
{
	ConstantValue result;
	result.kind = ConstantValue::CONSTANT_POINTER;
	result.type = type;
	result.pointer = pointer;
	return result;
}

PA19IntegralValue Analyzer::ToIntegralValue(const ConstantValue& value) const
{
	return value.integral;
}

PA19IntegralValue Analyzer::ParseLiteralValue(const string& raw) const
{
	string spelling = raw;
	const size_t marker = spelling.find(':');
	if (marker != string::npos && spelling.substr(0, marker) == "TT_LITERAL")
		spelling.erase(0, marker + 1);
	if (spelling == "true" || spelling == "false")
		return PA19IntegralValue::Signed(spelling == "true", "bool", 1);
	PA19IntegralValue result;
	if (PA19DecodeCharacter(spelling, &result) || PA19ParseInteger(spelling, &result)) return result;
	throw logic_error("unsupported constant expression");
}

long long Analyzer::ParseLiteral(const string& raw) const
{
	return PA19Signed(ParseLiteralValue(raw));
}


bool Analyzer::HasTemplateParameterScope(Scope* scope) const
	{
		for (Scope* current = scope; current; current = current->parent)
			if (current->kind == SCOPE_TEMPLATE_PARAMETERS) return true;
		return false;
	}

bool Analyzer::IsDependentTemplateName(Scope* scope, const string& raw) const
	{
		string name = raw;
		while (name.compare(0, 2, "::") == 0) name.erase(0, 2);
		for (Scope* current = scope; current; current = current->parent) {
			if (current->kind == SCOPE_TEMPLATE_PARAMETERS)
				for (size_t i = 0; i < current->bindings.size(); ++i) {
					const string& parameter = current->bindings[i].name;
					for (size_t position = name.find(parameter);
						position != string::npos;
						position = name.find(parameter, position + parameter.size())) {
						const bool left = position == 0 ||
							(!isalnum(static_cast<unsigned char>(name[position - 1])) &&
							 name[position - 1] != '_');
						const size_t end = position + parameter.size();
						const bool right = end == name.size() ||
							(!isalnum(static_cast<unsigned char>(name[end])) &&
							 name[end] != '_');
						if (left && right) return true;
					}
				}
			if (current->kind == SCOPE_CLASS && current->owner_type)
				for (TypePtr base = current->owner_type->direct_base; base;
					base = base->direct_base)
					if (base->kind == TYPE_TEMPLATE_PARAMETER ||
						base->kind == TYPE_TEMPLATE_TEMPLATE_PARAMETER ||
						base->dependent_base_lookup) return true;
		}
		return false;
	}

namespace {

string TrimConversionType(string text)
{
	while(!text.empty() && isspace(static_cast<unsigned char>(text[0]))) text.erase(0, 1);
	while(!text.empty() && isspace(static_cast<unsigned char>(text[text.size() - 1]))) text.erase(text.size() - 1);
	return text;
}

string SpecialMemberName(const string& raw_name)
{
	const size_t operator_pos = raw_name.rfind("operator");
	if (operator_pos != string::npos)
	{
		string suffix = raw_name.substr(operator_pos + 8);
		while (!suffix.empty() && isspace(static_cast<unsigned char>(suffix[0])))
			suffix.erase(0, 1);
		return "operator" + suffix;
	}
	const size_t separator = raw_name.rfind("::");
	return separator == string::npos ? raw_name : raw_name.substr(separator + 2);
}

string SpecialMemberOwner(const string& raw_name)
{
	const size_t operator_pos = raw_name.find("operator");
	if (operator_pos != string::npos)
	{
		string owner = raw_name.substr(0, operator_pos);
		while (!owner.empty() && isspace(static_cast<unsigned char>(owner[owner.size() - 1])))
			owner.erase(owner.size() - 1, 1);
		if (owner.size() >= 2 && owner.substr(owner.size() - 2) == "::")
			owner.erase(owner.size() - 2);
		return owner;
	}
	const size_t separator = raw_name.rfind("::");
	return separator == string::npos ? string() : raw_name.substr(0, separator);
}

TypePtr ConversionTypeFromName(const Analyzer& analyzer, const string& raw_name,
	Scope* scope)
{
	const size_t operator_pos = raw_name.rfind("operator");
	if(operator_pos == string::npos) return TypePtr();
	string text = TrimConversionType(raw_name.substr(operator_pos + 8));
	if(text.empty() || string("+-*/%^&|=!<>~[],()").find(text[0]) != string::npos)
		return TypePtr();
	TypeKind reference_kind = TYPE_FUNDAMENTAL;
	if(text.size() >= 2 && text.substr(text.size() - 2) == "&&") {
		reference_kind = TYPE_RVALUE_REFERENCE;
		text = TrimConversionType(text.substr(0, text.size() - 2));
	} else if(!text.empty() && text[text.size() - 1] == '&') {
		reference_kind = TYPE_LVALUE_REFERENCE;
		text = TrimConversionType(text.substr(0, text.size() - 1));
	}
	unsigned int pointer_count = 0;
	while(!text.empty() && text[text.size() - 1] == '*') {
		++pointer_count;
		text = TrimConversionType(text.substr(0, text.size() - 1));
	}
	vector<string> words;
	string word;
	for(size_t i = 0; i <= text.size(); ++i) {
		const char c = i < text.size() ? text[i] : ' ';
		if(isspace(static_cast<unsigned char>(c))) {
			if(!word.empty()) { words.push_back(word); word.clear(); }
		} else word += c;
	}
	bool add_const = false;
	bool add_volatile = false;
	vector<string> type_words;
	for(size_t i = 0; i < words.size(); ++i) {
		if(words[i] == "const") add_const = true;
		else if(words[i] == "volatile") add_volatile = true;
		else type_words.push_back(words[i]);
	}
	if(type_words.empty()) return TypePtr();
	TypePtr result;
	bool fundamental = true;
	for(size_t i = 0; i < type_words.size(); ++i)
		if(!Analyzer::IsFundamentalWord(type_words[i])) fundamental = false;
	if(fundamental) result = Fundamental(Analyzer::FundamentalName(type_words));
	else {
		string base;
		for(size_t i = 0; i < type_words.size(); ++i) {
			if(type_words[i] == "::") {
				while(!base.empty() && base[base.size() - 1] == ' ')
					base.erase(base.size() - 1, 1);
				base += "::";
			} else {
				if(!base.empty() && base[base.size() - 1] != ':') base += " ";
				base += type_words[i];
			}
		}
		Analyzer::PathTarget target = analyzer.ResolvePath(scope, base);
		if(!target.binding || (target.binding->kind != BIND_TYPE &&
			 target.binding->kind != BIND_TYPE_ALIAS)) return TypePtr();
		result = target.binding->type;
	}
	result = CloneWithCv(result, add_const, add_volatile);
	for(unsigned int i = 0; i < pointer_count; ++i) result = PointerTo(result);
	if(reference_kind != TYPE_FUNDAMENTAL) result = ReferenceTo(reference_kind, result);
	return result;
}

// An empty class still has a nonzero complete-object size, but an empty base
// subobject is permitted to occupy no bytes (the empty-base optimization).
// Keep that distinction in the typed layout facts so derived members begin at
// offset zero without changing sizeof the empty class itself.
bool EmptyBaseStorage(const TypePtr& raw_type)
{
	if (!raw_type || raw_type->kind != TYPE_CLASS) return false;
	if (raw_type->polymorphic || raw_type->has_vpointer) return false;
	for (size_t i = 0; i < raw_type->class_members.size(); ++i)
	{
		const ClassMemberInfo& member = raw_type->class_members[i];
		if (!member.is_static && !member.name.empty()) return false;
	}
	return !raw_type->direct_base || EmptyBaseStorage(raw_type->direct_base);
}

bool IsValidAlignment(size_t alignment)
{
	return alignment == 0 || (alignment & (alignment - 1)) == 0;
}

TypePtr FunctionTypeForVirtual(const Binding& binding)
{
	if (!binding.type) return TypePtr();
	return binding.type->kind == TYPE_FUNCTION ? binding.type : TypePtr();
}

bool SameVirtualParameters(const TypePtr& left, const TypePtr& right)
{
	if (!left || !right || left->kind != TYPE_FUNCTION || right->kind != TYPE_FUNCTION ||
		left->variadic != right->variadic ||
		left->function_const != right->function_const ||
		left->function_volatile != right->function_volatile ||
		left->function_lvalue_ref_qualified != right->function_lvalue_ref_qualified ||
		left->function_rvalue_ref_qualified != right->function_rvalue_ref_qualified ||
		left->parameters.size() != right->parameters.size()) return false;
	for (size_t i = 0; i < left->parameters.size(); ++i)
		if (!SameLayoutType(left->parameters[i], right->parameters[i])) return false;
	return true;
}

bool IsDerivedClass(const TypePtr& derived, const TypePtr& base)
{
	for (TypePtr current = derived ? derived->direct_base : TypePtr(); current;
		current = current->direct_base)
		if (SameLayoutType(current, base)) return true;
	return false;
}

bool CovariantVirtualReturn(const TypePtr& derived, const TypePtr& base)
{
	if (!derived || !base || derived->kind != TYPE_FUNCTION || base->kind != TYPE_FUNCTION)
		return false;
	if (SameLayoutType(derived->child, base->child)) return true;
	const TypePtr derived_return = derived->child;
	const TypePtr base_return = base->child;
	if (!derived_return || !base_return ||
		(derived_return->kind != TYPE_POINTER && derived_return->kind != TYPE_LVALUE_REFERENCE) ||
		derived_return->kind != base_return->kind ||
		!derived_return->child || !base_return->child) return false;
	return derived_return->child->kind == TYPE_CLASS &&
		base_return->child->kind == TYPE_CLASS &&
	IsDerivedClass(derived_return->child, base_return->child);
}

bool IsDestructorName(const string& name)
{
	return name.size() > 1 && name[0] == '~';
}

bool SameVirtualSlot(const VirtualMethodInfo& slot, const Binding& binding)
{
	const TypePtr function = FunctionTypeForVirtual(binding);
	if (!function) return false;
	if (slot.destructor || IsDestructorName(binding.name))
		return slot.destructor && IsDestructorName(binding.name) &&
			SameVirtualParameters(slot.function, function);
	return slot.name == binding.name && SameVirtualParameters(slot.function, function) &&
		CovariantVirtualReturn(function, slot.function);
}

const VirtualMethodInfo* FindInheritedVirtual(const TypePtr& base,
	const Binding& binding)
{
	if (!base) return 0;
	for (size_t i = 0; i < base->virtual_methods.size(); ++i)
		if (SameVirtualSlot(base->virtual_methods[i], binding))
			return &base->virtual_methods[i];
	return 0;
}

void ApplyPolymorphicLayout(const TypePtr& type, bool empty_base,
	size_t base_size, size_t base_alignment, size_t* offset,
	size_t* maximum_alignment)
{
	if (!type || !type->has_vpointer || !offset || !maximum_alignment) return;
	const size_t pointer_size = 8;
	*maximum_alignment = max(*maximum_alignment, pointer_size);
	if (type->direct_base && !empty_base && !type->direct_base->polymorphic)
	{
		// PA17 puts the first vpointer at complete-object offset zero.  A
		// non-polymorphic direct base therefore starts after that pointer and
		// every base/member access must use the same typed adjustment.
		type->direct_base_offset = Analyzer::AlignUp(pointer_size,
			max<size_t>(1, base_alignment));
		*offset = type->direct_base_offset + base_size;
	}
	else if (!type->direct_base || empty_base)
	{
		type->direct_base_offset = 0;
		*offset = pointer_size;
	}
}

} // namespace

void Analyzer::ProcessFunctionDefinition(const CPPGMAstNodePtr& node, Scope* scope)
{
	if (node->children.size() < 3) throw logic_error("invalid function definition");
	SpecFacts facts;
	TypePtr base = TypeFromSpecSeq(node->children[0], scope, &facts);
	CPPGMAstNodePtr declarator = node->children[1];
	const string raw_name = FirstIdentifier(declarator);
	if (raw_name.empty()) throw logic_error("function has no name");
	Scope* declaration_scope = scope;
	Scope* lookup_scope = scope;
	TypePtr member_owner;
	string name = raw_name;
	const size_t separator = raw_name.rfind("::");
	if (separator != string::npos)
	{
		PathTarget owner = ResolvePath(scope, raw_name.substr(0, separator));
		if (owner.binding) member_owner = owner.binding->type;
		else if (owner.scope) member_owner = owner.scope->owner_type;
		if (member_owner && member_owner->kind == TYPE_CLASS && member_owner->owned_scope)
		{
			declaration_scope = member_owner->owned_scope;
			lookup_scope = declaration_scope;
			name = LastComponent(raw_name);
		}
	}
	else if (scope->kind == SCOPE_CLASS)
	{
		member_owner = scope->owner_type;
	}
	TypePtr function_type;
	try {
		// Keep the ordinary declaration lookup path for definitions whose
		// parameter and trailing-return types are already visible.  The bridge
		// below is only needed when a dependent owner member is the missing fact;
		// this preserves the earlier source-order behavior for ordinary members.
		function_type = BuildDeclarator(declarator, base, scope);
	} catch (const logic_error&) {
		if (!member_owner || member_owner->kind != TYPE_CLASS ||
			!member_owner->owned_scope) throw;
		lookup_scope = NewChild(scope, SCOPE_CLASS, name);
		lookup_scope->owner_type = member_owner;
		for (size_t binding = 0; binding < declaration_scope->bindings.size(); ++binding)
			lookup_scope->add(declaration_scope->bindings[binding]);
		function_type = BuildDeclarator(declarator, base, lookup_scope);
	}
	if ((!function_type || function_type->kind != TYPE_FUNCTION) &&
		member_owner && member_owner->kind == TYPE_CLASS && member_owner->owned_scope) {
		lookup_scope = NewChild(scope, SCOPE_CLASS, name);
		lookup_scope->owner_type = member_owner;
		for (size_t binding = 0; binding < declaration_scope->bindings.size(); ++binding)
			lookup_scope->add(declaration_scope->bindings[binding]);
		function_type = BuildDeclarator(declarator, base, lookup_scope);
	}
	if (!function_type || function_type->kind != TYPE_FUNCTION)
		throw logic_error("definition is not a function");
	Binding binding(BIND_FUNCTION, name, function_type);
	binding.hidden_friend = facts.is_friend;
	binding.friend_owner = facts.is_friend ? member_owner : TypePtr();
	binding.is_member = static_cast<bool>(member_owner) && !facts.is_friend;
	binding.is_static = facts.is_static;
	binding.member_owner = member_owner;
	binding.noexcept_specified = HasNodeValue(declarator, "function-qualifier", "noexcept");
	binding.is_virtual = facts.is_virtual;
	binding.is_override = HasNodeValue(declarator, "virt-specifier", "override");
	binding.is_final = HasNodeValue(declarator, "virt-specifier", "final");
	Binding* stored = declaration_scope->add(binding);
	stored->declaration = node;
	if (member_owner)
	{
		for (size_t prior = 0; prior + 1 < declaration_scope->bindings.size(); ++prior)
		{
			Binding& candidate = declaration_scope->bindings[prior];
			if (candidate.kind != BIND_FUNCTION || candidate.name != name ||
				!candidate.is_member ||
				TypeText(candidate.type, true) != TypeText(function_type, true)) continue;
			stored->is_virtual = stored->is_virtual || candidate.is_virtual;
			stored->is_pure = stored->is_pure || candidate.is_pure;
			stored->is_override = stored->is_override || candidate.is_override;
			stored->is_final = stored->is_final || candidate.is_final;
		}
	}
	Scope* function_scope = NewChild(lookup_scope, SCOPE_FUNCTION, name);
	function_scopes_[node.get()] = function_scope;
	AddFunctionParameters(function_scope, declarator, lookup_scope);
	ProcessCompound(node->children[2], function_scope);
	if (HasTemplateParameterScope(scope))
		ValidateNondependentTemplateNode(node->children[2], function_scope);
}

void Analyzer::ValidateNondependentTemplateNode(const CPPGMAstNodePtr& node,
	Scope* scope, const CPPGMAstNodePtr& parent, size_t child_index)
{
	if (!node) return;
	if (node->kind == "class-specifier" || node->kind == "class-forward-declaration")
		return;
	Scope* current = scope;
	if (node->kind == "compound-statement") {
		map<const CPPGMAstNode*, Scope*>::const_iterator found =
			compound_scopes_.find(node.get());
		if (found != compound_scopes_.end()) current = found->second;
	}
	if (node->kind == "id-expression") {
		const bool member_name = parent && parent->kind == "member-expression" &&
			child_index == 1;
		const bool type_name = parent && (parent->kind == "type-id" ||
			parent->kind == "type-specifier" || parent->kind == "base-name" ||
			parent->kind == "mem-initializer-id");
		const bool builtin_value_initialization = parent &&
			parent->kind == "call-expression" && child_index == 0 &&
			IsFundamentalWord(node->value);
			if (!member_name && !type_name &&
				!IsDependentTemplateName(current, node->value) &&
				!builtin_value_initialization && !ResolveBinding(current, node->value)) {
				throw logic_error("unknown nondependent template name: " + node->value);
			}
	}
	for (size_t i = 0; i < node->children.size(); ++i)
		ValidateNondependentTemplateNode(node->children[i], current, node, i);
}

void Analyzer::ProcessSimpleDeclaration(const CPPGMAstNodePtr& node, Scope* scope)
{
	if (node->children.empty()) throw logic_error("invalid simple declaration");
	SpecFacts facts;
	TypePtr base = TypeFromSpecSeq(node->children[0], scope, &facts);
	CPPGMAstNodePtr list = ChildOfKind(node, "init-declarator-list");
	if (!list) return;
	for (size_t i = 0; i < list->children.size(); ++i)
	{
		CPPGMAstNodePtr item = list->children[i];
		if (!item || item->children.empty()) continue;
		CPPGMAstNodePtr declarator = item->children[0];
		CPPGMAstNodePtr initializer = item->children.size() > 1 ? item->children[1] : CPPGMAstNodePtr();
		TypePtr type = BuildDeclarator(declarator, base, scope);
		// An out-of-class definition of an unknown-bound array supplies the
		// bound through its braced initializer.  Preserve that fact on the
		// semantic type so qualified uses such as `sizeof(T::member)` see the
		// completed static member rather than the declaration's zero bound.
		if(type->kind == TYPE_ARRAY && type->bound == 0 && initializer &&
			!initializer->children.empty() && initializer->children[0] &&
			initializer->children[0]->kind == "braced-init-list")
			type = ArrayOf(static_cast<long long>(initializer->children[0]->children.size()),
				type->child);
		if (facts.is_constexpr && type->kind != TYPE_FUNCTION) type = CloneWithCv(type, true, false);
		const string name = FirstIdentifier(declarator);
		if (name.empty()) continue;
		if (facts.is_constexpr && !initializer &&
			(scope->kind == SCOPE_FUNCTION || scope->kind == SCOPE_BLOCK))
			throw logic_error("constexpr local must be initialized");
		if (facts.is_typedef)
		{
			AddTypeBinding(scope, name, type, true);
			continue;
		}
		Binding binding(type->kind == TYPE_FUNCTION ? BIND_FUNCTION : BIND_VARIABLE, name, type);
		binding.declaration = node;
		binding.hidden_friend = facts.is_friend && type->kind == TYPE_FUNCTION;
		binding.friend_owner = binding.hidden_friend ?
			(scope && scope->kind == SCOPE_CLASS ? scope->owner_type : TypePtr()) : TypePtr();
		binding.is_member = binding.is_member && !binding.hidden_friend;
		if (type->kind == TYPE_FUNCTION)
		{
			binding.is_virtual = facts.is_virtual;
			binding.is_override = HasNodeValue(declarator, "virt-specifier", "override");
			binding.is_final = HasNodeValue(declarator, "virt-specifier", "final");
			binding.is_pure = IsPureInitializer(item);
		}
		ConstantValue evaluated_value;
		bool has_evaluated_value = false;
		if (type->kind != TYPE_LVALUE_REFERENCE && type->kind != TYPE_RVALUE_REFERENCE &&
			(initializer || (facts.is_const || facts.is_constexpr) &&
			(type->kind == TYPE_CLASS || type->kind == TYPE_ARRAY)) &&
			(facts.is_const || facts.is_constexpr))
		{
			CPPGMAstNodePtr expression = !initializer || initializer->children.empty() ?
				CPPGMAstNodePtr() : initializer->children[0];
			if (expression)
			{
				evaluated_value = EvaluateTyped(expression, scope, type);
				has_evaluated_value = evaluated_value.integral.known ||
					evaluated_value.floating_known ||
					(evaluated_value.kind == ConstantValue::CONSTANT_OBJECT && evaluated_value.object) ||
					(evaluated_value.kind == ConstantValue::CONSTANT_POINTER && evaluated_value.pointer);
				if (has_evaluated_value) {
					binding.has_value = evaluated_value.integral.known;
					binding.value = evaluated_value.value;
					binding.constant_value = evaluated_value.integral;
				}
			}
			else if (!expression)
			{
				evaluated_value = DefaultConstantValue(type, scope);
				has_evaluated_value = evaluated_value.kind != ConstantValue::CONSTANT_UNKNOWN;
				if (has_evaluated_value)
				{
					binding.has_value = evaluated_value.integral.known;
					binding.value = evaluated_value.value;
					binding.constant_value = evaluated_value.integral;
				}
			}
		}
		Binding* stored_binding = scope->add(binding);
		if (has_evaluated_value) constant_binding_values_[stored_binding] = evaluated_value;
		// Qualified static-member definitions are collected in the enclosing
		// scope, while lookup of `Owner::member` uses the owner class scope.
		// Keep both bindings and the owning ClassMemberInfo synchronized with
		// the completed definition's typed array (or scalar) type.
		const size_t separator = name.rfind("::");
		if(separator != string::npos) {
			PathTarget owner_target = ResolvePath(scope, name.substr(0, separator));
			TypePtr owner_type = owner_target.binding ? owner_target.binding->type :
				(owner_target.scope ? owner_target.scope->owner_type : TypePtr());
			if(owner_type && owner_type->kind == TYPE_CLASS && owner_type->owned_scope) {
				Binding* member = owner_type->owned_scope->local(name.substr(separator + 2));
				if(member && member->kind == BIND_VARIABLE && member->is_static) {
					member->type = type;
					if(member->member_index < owner_type->class_members.size()) {
						owner_type->class_members[member->member_index].type = type;
						if(initializer)
							owner_type->class_members[member->member_index].initializer = initializer;
					}
				}
			}
		}
	}
}

bool Analyzer::HasNodeValue(const CPPGMAstNodePtr& node, const string& kind,
	const string& value)
{
	if (!node) return false;
	if (node->kind == kind &&
		(node->value == value || node->value.find(":" + value) != string::npos))
		return true;
	for (size_t i = 0; i < node->children.size(); ++i)
		if (HasNodeValue(node->children[i], kind, value)) return true;
	return false;
}

bool Analyzer::IsPureInitializer(const CPPGMAstNodePtr& item)
{
	if (!item || item->children.size() < 2 || !item->children[1]) return false;
	const CPPGMAstNodePtr initializer = item->children[1];
	if (initializer->kind != "initializer" || initializer->children.size() != 1)
		return false;
	const CPPGMAstNodePtr expression = initializer->children[0];
	return expression && expression->kind == "literal" && expression->value == "0";
}

size_t Analyzer::AlignUp(size_t value, size_t alignment)
{
	if (alignment == 0) alignment = 1;
	const size_t remainder = value % alignment;
	return remainder == 0 ? value : value + alignment - remainder;
}

size_t Analyzer::AttributeAlignment(const CPPGMAstNodePtr& attribute, Scope* scope)
{
	if (!attribute || attribute->kind != "attribute" || attribute->children.size() != 1)
		throw logic_error("invalid alignment attribute");
	const CPPGMAstNodePtr argument = attribute->children[0];
	size_t alignment = 0;
	if (argument && argument->kind == "gnu-alignof-expression")
	{
		if (argument->children.size() != 1 || !argument->children[0] ||
			argument->children[0]->kind != "type-id")
			throw logic_error("invalid GNU alignment operand");
		TypePtr type = TypeFromTypeId(argument->children[0], scope);
		alignment = TypeAlignment(type);
		if (alignment == 0) throw logic_error("alignment type has no alignment");
	}
	else if (argument && argument->kind == "type-id")
	{
		try {
			TypePtr type = TypeFromTypeId(argument, scope);
			alignment = TypeAlignment(type);
			if (alignment == 0) throw logic_error("alignment type has no alignment");
		} catch (const logic_error&) {
			const string spelling = AttributeNodeSpelling(argument);
			CPPGMAstNodePtr expression(new CPPGMAstNode(
				!spelling.empty() && isdigit(static_cast<unsigned char>(spelling[0])) ?
					"literal" : "id-expression", spelling));
			ConstantValue value = Evaluate(expression, scope);
			if (!value.integral.known || value.value < 0)
				throw logic_error("alignment is not a non-negative constant");
			alignment = static_cast<size_t>(value.value);
		}
	}
	else
	{
		ConstantValue value = Evaluate(argument, scope);
		if (!value.integral.known || value.value < 0)
			throw logic_error("alignment is not a non-negative constant");
		alignment = static_cast<size_t>(value.value);
	}
	if (!IsValidAlignment(alignment)) throw logic_error("invalid alignment");
	return alignment;
}

void Analyzer::ProcessSpecialMember(const CPPGMAstNodePtr& node, Scope* scope)
{
	if (node->kind == "special-member-declaration")
	{
		CPPGMAstNodePtr declarator = ChildOfKind(node, "declarator");
		if (!declarator) throw logic_error("special member has no declarator");
		Scope* declaration_scope = scope;
		TypePtr member_owner;
		const string owner_name = SpecialMemberOwner(node->value);
		if (!owner_name.empty())
		{
			PathTarget owner = ResolvePath(scope,
				StripTemplateArgumentsFromPath(owner_name));
			if (owner.binding) member_owner = owner.binding->type;
			else if (owner.scope) member_owner = owner.scope->owner_type;
			if (member_owner && member_owner->kind == TYPE_CLASS && member_owner->owned_scope)
				declaration_scope = member_owner->owned_scope;
		}
		else if (scope->kind == SCOPE_CLASS)
			member_owner = scope->owner_type;
		if (!member_owner || member_owner->kind != TYPE_CLASS) return;
		const string name = SpecialMemberName(node->value);
		TypePtr conversion_type = ConversionTypeFromName(*this, name, declaration_scope);
		TypePtr function = BuildDeclarator(declarator,
			conversion_type ? conversion_type : Fundamental("void"), declaration_scope);
		const bool noexcept_specified = HasNodeValue(declarator,
			"function-qualifier", "noexcept");
		for (size_t i = 0; i < declaration_scope->bindings.size(); ++i)
		{
			Binding& existing = declaration_scope->bindings[i];
			if (existing.kind != BIND_FUNCTION || existing.name != name ||
				TypeText(existing.type, true) != TypeText(function, true)) continue;
			if (existing.noexcept_specified != noexcept_specified)
				throw logic_error("special-member exception specification mismatch");
			existing.noexcept_specified = noexcept_specified;
			existing.is_member = true;
			existing.is_static = false;
			existing.member_owner = member_owner;
			existing.is_virtual = existing.is_virtual ||
				HasNodeValue(node, "specifier", "virtual");
			existing.is_override = existing.is_override ||
				HasNodeValue(declarator, "virt-specifier", "override");
			existing.is_final = existing.is_final ||
				HasNodeValue(declarator, "virt-specifier", "final");
			existing.declaration = node;
			return;
		}
		Binding binding(BIND_FUNCTION, name, function);
		binding.is_member = true;
		binding.is_static = false;
		binding.member_owner = member_owner;
		binding.is_virtual = HasNodeValue(node, "specifier", "virtual");
		binding.is_override = HasNodeValue(declarator, "virt-specifier", "override");
		binding.is_final = HasNodeValue(declarator, "virt-specifier", "final");
		binding.noexcept_specified = noexcept_specified;
		binding.access = member_owner->tag == "class" ? "private" : "public";
		binding.declaration = node;
		declaration_scope->add(binding);
		return;
	}
	if (node->kind != "special-member-definition") return;
	CPPGMAstNodePtr declarator = ChildOfKind(node, "declarator");
	CPPGMAstNodePtr body = ChildOfKind(node, "compound-statement");
	if (!declarator || !body) return;
	Scope* declaration_scope = scope;
	TypePtr member_owner;
	const string owner_name = SpecialMemberOwner(node->value);
	if (!owner_name.empty())
	{
		PathTarget owner = ResolvePath(scope,
			StripTemplateArgumentsFromPath(owner_name));
		if (owner.binding) member_owner = owner.binding->type;
		else if (owner.scope) member_owner = owner.scope->owner_type;
		if (member_owner && member_owner->kind == TYPE_CLASS && member_owner->owned_scope)
			declaration_scope = member_owner->owned_scope;
	}
	else if (scope->kind == SCOPE_CLASS) member_owner = scope->owner_type;
	const string name = SpecialMemberName(node->value);
	TypePtr conversion_type = ConversionTypeFromName(*this, name, declaration_scope);
	TypePtr function = BuildDeclarator(declarator,
		conversion_type ? conversion_type : Fundamental("void"), declaration_scope);
	const bool noexcept_specified = HasNodeValue(declarator,
		"function-qualifier", "noexcept");
	for (size_t i = 0; i < declaration_scope->bindings.size(); ++i)
	{
		Binding& existing = declaration_scope->bindings[i];
		if (existing.kind != BIND_FUNCTION || existing.name != name ||
			TypeText(existing.type, true) != TypeText(function, true)) continue;
		if (existing.noexcept_specified != noexcept_specified)
			throw logic_error("special-member exception specification mismatch");
	}
	Binding* binding = declaration_scope->add(Binding(BIND_FUNCTION, name, function));
	binding->is_member = static_cast<bool>(member_owner);
	binding->is_static = false;
	binding->member_owner = member_owner;
	binding->is_virtual = HasNodeValue(node, "specifier", "virtual");
	binding->is_override = HasNodeValue(declarator, "virt-specifier", "override");
	binding->is_final = HasNodeValue(declarator, "virt-specifier", "final");
	binding->noexcept_specified = noexcept_specified;
	binding->access = member_owner && member_owner->tag == "class" ? "private" : "public";
	binding->declaration = node;
	Scope* function_scope = NewChild(declaration_scope, SCOPE_FUNCTION, name);
	function_scopes_[node.get()] = function_scope;
	AddFunctionParameters(function_scope, declarator, declaration_scope);
	ProcessCompound(body, function_scope);
}

void Analyzer::ApplyClassAttributes(const CPPGMAstNodePtr& node, const TypePtr& type,
	Scope* scope)
{
	if (!node || !type) return;
	for (size_t i = 0; i < node->children.size(); ++i)
	{
		const CPPGMAstNodePtr child = node->children[i];
		if (!child || child->kind != "attribute") continue;
		const size_t alignment = AttributeAlignment(child, scope);
		if (alignment > type->explicit_alignment) type->explicit_alignment = alignment;
	}
}

void Analyzer::ComputeClassLayout(const CPPGMAstNodePtr& node, const TypePtr& type,
	Scope* class_scope)
{
	if (!type) return;
	if (type->layout_in_progress)
		throw logic_error("recursive class layout");
	type->layout_in_progress = true;
	type->is_union = type->tag == "union";
	ApplyClassAttributes(node, type, class_scope ? class_scope->parent : 0);

	if (type->direct_base && type->direct_base->kind == TYPE_CLASS &&
		!type->direct_base->complete) {
		throw logic_error("incomplete direct base class");
	}
	const bool empty_base = type->direct_base && EmptyBaseStorage(type->direct_base);
	const bool owns_vpointer = type->has_vpointer;
	type->direct_base_offset = 0;
	type->direct_base_offsets.assign(type->direct_bases.size(), 0);
	size_t offset = type->direct_base && !empty_base ? TypeSize(type->direct_base) : 0;
	size_t maximum_alignment = type->direct_base && !empty_base ?
		TypeAlignment(type->direct_base) : 1;
	if (owns_vpointer) ApplyPolymorphicLayout(type, empty_base,
		type->direct_base && !empty_base ? TypeSize(type->direct_base) : 0,
		type->direct_base && !empty_base ? TypeAlignment(type->direct_base) : 1,
		&offset, &maximum_alignment);
	if (!type->direct_base_offsets.empty())
		type->direct_base_offsets[0] = type->direct_base_offset;
	for (size_t base_index = 1; base_index < type->direct_bases.size(); ++base_index) {
		const TypePtr base = type->direct_bases[base_index];
		if (base && base->kind == TYPE_CLASS && !base->complete)
			throw logic_error("incomplete direct base class");
		if (!base || EmptyBaseStorage(base)) {
			type->direct_base_offsets[base_index] = 0;
			continue;
		}
		const size_t base_alignment = max<size_t>(1, TypeAlignment(base));
		offset = AlignUp(offset, base_alignment);
		type->direct_base_offsets[base_index] = offset;
		maximum_alignment = max(maximum_alignment, base_alignment);
		offset += TypeSize(base);
	}
	size_t union_size = offset;

	ComputeClassMemberLayout(type, union_size, &offset, &maximum_alignment);
	type->object_alignment = max(maximum_alignment, type->explicit_alignment);
	if (type->object_alignment == 0) type->object_alignment = 1;
	if (type->class_members.empty() && type->direct_bases.empty() && !owns_vpointer) offset = 1;
	type->object_size = AlignUp(max<size_t>(1, offset), type->object_alignment);
	type->materialize_sizeof_address = false;
	if(type->template_specialization && !type->template_primary.empty())
		for(size_t member = 0; member < type->class_members.size() &&
			!type->materialize_sizeof_address; ++member) {
			const TypePtr member_type = type->class_members[member].type;
			if(!member_type || !member_type->template_specialization) continue;
			for(size_t argument = 0; argument < member_type->template_arguments.size(); ++argument)
				if(member_type->template_arguments[argument].find(type->template_primary) != string::npos) {
					type->materialize_sizeof_address = true;
					break;
				}
		}
	type->layout_complete = true;
	type->layout_in_progress = false;
	(void)class_scope;
}

namespace {

void RecordUsingAccess(const CPPGMAstNodePtr& node, Scope* class_scope,
	const string& access)
{
	CPPGMAstNodePtr target = ChildOfKind(node, "target");
	const string name = target ? LastComponent(target->value) : string();
	if (name.empty()) return;
	for (size_t i = 0; i < class_scope->bindings.size(); ++i)
	{
		Binding& binding = class_scope->bindings[i];
		if (binding.name != name || !binding.is_member) continue;
		binding.access = access;
		binding.declaration = node;
	}
}

void RecordBitField(Analyzer& analyzer, const CPPGMAstNodePtr& node,
	const TypePtr& type, Scope* class_scope, const string& access)
{
	if (node->children.size() < 2) return;
	Analyzer::SpecFacts facts;
	TypePtr field_type = analyzer.TypeFromSpecSeq(node->children[0], class_scope, &facts);
	const CPPGMAstNodePtr field = node->children[1];
	const CPPGMAstNodePtr declarator = field && !field->children.empty() ?
		field->children[0] : CPPGMAstNodePtr();
	const string name = FirstIdentifier(declarator);
	long long width = 0;
	if (field && field->children.size() > 1)
	{
		ConstantValue value = analyzer.Evaluate(field->children[1], class_scope);
		if (!value.integral.known) throw logic_error("bit-field width is not constant");
		width = value.value;
	}
	if (!name.empty())
	{
		Binding* binding = class_scope->local(name);
		if (!binding) binding = class_scope->add(Binding(BIND_VARIABLE, name, field_type));
		binding->type = field_type;
		binding->access = access;
		binding->declaration = node;
	}
	ClassMemberInfo member;
	member.name = name;
	member.type = field_type;
	member.bit_field = true;
	member.bit_width = width;
	type->class_members.push_back(member);
}

void RecordMemberIndices(const TypePtr& type, Scope* class_scope)
{
	for (size_t i = 0; i < type->class_members.size(); ++i)
	{
		ClassMemberInfo& member = type->class_members[i];
		if (member.name.empty()) continue;
		for (size_t j = 0; j < class_scope->bindings.size(); ++j)
		{
			Binding& binding = class_scope->bindings[j];
			if (binding.name != member.name || binding.kind != BIND_VARIABLE) continue;
			binding.is_member = true;
			binding.member_owner = type;
			binding.member_index = i;
		}
	}
}

} // namespace

void Analyzer::RecordClassDeclaration(const CPPGMAstNodePtr& child, const TypePtr& type,
	Scope* class_scope, const string& access)
{
	if (!child || !type || !class_scope || child->children.empty()) return;
	Analyzer::SpecFacts facts;
	TypePtr base = TypeFromSpecSeq(child->children[0], class_scope, &facts);
	CPPGMAstNodePtr list = ChildOfKind(child, "init-declarator-list");
	if (child->kind == "function-definition") list.reset();
	if (!list)
	{
		if (base && base->kind == TYPE_CLASS && base->is_union &&
			base->name.find("__anonymous_union_type__") == 0)
		{
			ClassMemberInfo member;
			member.type = base;
			type->class_members.push_back(member);
		}
		CPPGMAstNodePtr declarator = child->kind == "function-definition" && child->children.size() > 1 ?
			child->children[1] : CPPGMAstNodePtr();
		if (declarator)
		{
			const string name = FirstIdentifier(declarator);
			const TypePtr function_type = BuildDeclarator(declarator, base, class_scope);
			for (size_t k = 0; k < class_scope->bindings.size(); ++k)
			{
				Binding& binding = class_scope->bindings[k];
				if (binding.name != name || binding.kind != BIND_FUNCTION ||
					TypeText(binding.type, true) != TypeText(function_type, true)) continue;
				binding.access = access;
				binding.declaration = child;
				binding.hidden_friend = facts.is_friend;
				binding.friend_owner = facts.is_friend ? type : TypePtr();
				binding.is_member = !facts.is_friend;
				binding.is_static = facts.is_static;
				binding.member_owner = type;
				binding.is_virtual = binding.is_virtual || facts.is_virtual;
				binding.is_override = binding.is_override ||
					HasNodeValue(declarator, "virt-specifier", "override");
				binding.is_final = binding.is_final ||
					HasNodeValue(declarator, "virt-specifier", "final");
				binding.is_pure = binding.is_pure || false;
			}
		}
		return;
	}
	for (size_t j = 0; j < list->children.size(); ++j)
	{
		const CPPGMAstNodePtr item = list->children[j];
		if (!item || item->children.empty()) continue;
		const CPPGMAstNodePtr declarator = item->children[0];
		const string name = FirstIdentifier(declarator);
		TypePtr field_type = BuildDeclarator(declarator, base, class_scope);
		if (facts.is_friend)
		{
			if (!name.empty()) type->friend_access.push_back(FriendAccess(
				FriendAccess::FRIEND_FUNCTION, name, field_type));
			for (size_t k = 0; k < class_scope->bindings.size(); ++k)
			{
				Binding& binding = class_scope->bindings[k];
				if (binding.name != name || binding.kind != BIND_FUNCTION ||
					TypeText(binding.type, true) != TypeText(field_type, true)) continue;
				binding.is_member = false;
				binding.is_static = false;
				binding.member_owner.reset();
				binding.hidden_friend = true;
				binding.friend_owner = type;
				binding.access.clear();
			}
			continue;
		}
		for (size_t k = 0; k < class_scope->bindings.size(); ++k)
		{
			Binding& binding = class_scope->bindings[k];
			if (binding.name != name) continue;
			if (field_type->kind != TYPE_FUNCTION && !facts.is_typedef &&
				(binding.kind != BIND_VARIABLE || !SameTypeIgnoringTopCv(binding.type, field_type))) continue;
			// A constexpr data member is recorded with top-level const during
			// declaration processing, while this rebuilt declarator has the
			// underlying member type.  Ignore only that cv-only difference.
			if (field_type->kind == TYPE_FUNCTION &&
				TypeText(binding.type, true) != TypeText(field_type, true)) continue;
			binding.type = field_type;
			binding.access = access;
			binding.declaration = child;
			binding.is_member = true;
			binding.is_static = facts.is_static;
			binding.member_owner = type;
			binding.is_virtual = binding.is_virtual || facts.is_virtual;
			binding.is_override = binding.is_override ||
				HasNodeValue(declarator, "virt-specifier", "override");
			binding.is_final = binding.is_final ||
				HasNodeValue(declarator, "virt-specifier", "final");
			binding.is_pure = binding.is_pure || IsPureInitializer(item);
		}
		if (facts.is_typedef || field_type->kind == TYPE_FUNCTION || name.empty()) continue;
		ClassMemberInfo member;
		member.name = name;
		member.type = field_type;
		member.is_static = facts.is_static;
		member.is_mutable = facts.is_mutable;
		member.initializer = item->children.size() > 1 ? item->children[1] : CPPGMAstNodePtr();
		type->class_members.push_back(member);
	}
}

void Analyzer::RecordClassMembers(const CPPGMAstNodePtr& node, const TypePtr& type,
	Scope* scope, Scope* class_scope)
{
	if (!node || !type || !class_scope) return;
	string access = type->tag == "class" ? "private" : "public";
	for (size_t i = 0; i < node->children.size(); ++i)
	{
		const CPPGMAstNodePtr child = node->children[i];
		if (!child) continue;
		if (child->kind == "access-specifier")
		{
			const size_t colon = child->value.find(':');
			access = colon == string::npos ? child->value : child->value.substr(colon + 1);
			continue;
		}
		if (child->kind == "base-clause" || child->kind == "class-key" ||
			child->kind == "attribute" || child->kind == "empty-declaration") continue;
		if (child->kind == "using-declaration")
		{
			RecordUsingAccess(child, class_scope, access);
			continue;
		}
		if (child->kind == "bit-field-declaration")
		{
			RecordBitField(*this, child, type, class_scope, access);
			continue;
		}
		if (child->kind == "class-specifier")
		{
			TypePtr nested = ProcessClass(child, class_scope);
			if (nested && nested->is_union &&
				nested->name.find("__anonymous_union_type__") == 0)
			{
				ClassMemberInfo member;
				member.type = nested;
				type->class_members.push_back(member);
			}
			continue;
		}
		if (child->kind == "special-member-definition" ||
			child->kind == "special-member-declaration")
		{
			const string special_name = SpecialMemberName(child->value);
			for (size_t binding_index = 0; binding_index < class_scope->bindings.size();
				++binding_index)
			{
				Binding& binding = class_scope->bindings[binding_index];
				if (binding.kind == BIND_FUNCTION && binding.name == special_name &&
					binding.declaration.get() == child.get())
					binding.access = access;
			}
			continue;
		}
		if (child->kind == "simple-declaration" && !child->children.empty())
		{
			SpecFacts friend_facts;
			TypePtr friend_type = TypeFromSpecSeq(child->children[0], class_scope,
				&friend_facts);
			if (friend_facts.is_friend && friend_type &&
				friend_type->kind == TYPE_CLASS)
			{
				const string friend_name = LastComponent(friend_type->name);
				if (!friend_name.empty()) type->friend_access.push_back(FriendAccess(
					FriendAccess::FRIEND_CLASS, friend_name, friend_type));
				continue;
			}
		}
		if (child->kind != "simple-declaration" && child->kind != "function-definition") continue;
		RecordClassDeclaration(child, type, class_scope, access);
	}
	RecordMemberIndices(type, class_scope);

	// Build the effective single-inheritance virtual-slot map after all direct
	// member bindings have been recorded.  Keeping this map on Type makes the
	// later LowIR pass consume typed semantic facts rather than re-parsing the
	// source AST to rediscover overrides.
	type->virtual_methods.clear();
	if (type->direct_base)
		type->virtual_methods = type->direct_base->virtual_methods;
	for (size_t i = 0; i < class_scope->bindings.size(); ++i)
	{
		Binding& binding = class_scope->bindings[i];
		if (binding.kind != BIND_FUNCTION || !binding.is_member ||
			binding.is_static || binding.member_owner.get() != type.get()) continue;
		const TypePtr function = FunctionTypeForVirtual(binding);
		if (!function) continue;
		const VirtualMethodInfo* inherited = FindInheritedVirtual(
			type->direct_base, binding);
		if (binding.is_override && !inherited) {
			throw logic_error("override does not match a base virtual function: " + binding.name);
		}
		if (inherited && inherited->final)
			throw logic_error("override of final virtual function");
		if (inherited || binding.is_virtual || binding.is_pure || binding.is_override ||
			binding.is_final)
			binding.is_virtual = true;
		else continue;

		VirtualMethodInfo effective;
		effective.name = binding.name;
		effective.function = function;
		effective.binding = &binding;
		effective.owner = type;
		effective.destructor = IsDestructorName(binding.name) ||
			(inherited && inherited->destructor);
		effective.pure = binding.is_pure;
		effective.final = binding.is_final;
		bool replaced = false;
		for (size_t slot = 0; slot < type->virtual_methods.size(); ++slot)
			if (SameVirtualSlot(type->virtual_methods[slot], binding))
			{
				type->virtual_methods[slot] = effective;
				replaced = true;
				break;
			}
		if (!replaced) type->virtual_methods.push_back(effective);
	}
	type->polymorphic = !type->virtual_methods.empty();
	type->has_vpointer = type->polymorphic &&
		(!type->direct_base || !type->direct_base->polymorphic);
	(void)scope;
}

void Analyzer::PredeclareMaterializedNestedClasses(const CPPGMAstNodePtr& node,
	Scope* class_scope)
{
	bool has_materialized_member = node->template_instantiation ||
		node->explicit_specialization;
	if (!has_materialized_member)
		for (size_t i = 0; i < node->children.size(); ++i)
			if (node->children[i] && (node->children[i]->template_instantiation ||
				node->children[i]->explicit_specialization)) {
				has_materialized_member = true;
				break;
			}
	if (!has_materialized_member) return;
	for (size_t i = 0; i < node->children.size(); ++i) {
		const CPPGMAstNodePtr child = node->children[i];
		if (!child || (child->kind != "class-specifier" &&
			child->kind != "class-forward-declaration")) continue;
		const string nested_name = LastComponent(child->value);
		if (nested_name.empty() || class_scope->local(nested_name)) continue;
		TypePtr nested(new Type(TYPE_CLASS, nested_name));
		nested->tag = ClassKey(child);
		if (!class_scope->qualified_prefix.empty())
			nested->name = class_scope->qualified_prefix + "::" + nested_name;
		AddTypeBinding(class_scope, nested_name, nested);
	}
}

TypePtr Analyzer::ProcessClass(const CPPGMAstNodePtr& node, Scope* scope)
{
	map<const CPPGMAstNode*, TypePtr>::const_iterator cached = class_types_.find(node.get());
	if (cached != class_types_.end()) return cached->second;
	const string raw_name = node->value;
	const string name = LastComponent(raw_name);
	const bool anonymous = name.empty() || name == "<unnamed>";
	const string tag = ClassKey(node);
	if (anonymous)
	{
		const string generated = AnonymousTypeName(tag);
		TypePtr type(new Type(TYPE_CLASS, generated));
		type->tag = tag;
		class_types_[node.get()] = type;
		Scope* class_scope = ClassScope(type, scope, generated);
		for (size_t i = 0; i < node->children.size(); ++i)
			if (node->children[i]->kind != "class-key") Process(node->children[i], class_scope);
		type->class_members.clear();
		RecordClassMembers(node, type, scope, class_scope);
		ComputeClassLayout(node, type, class_scope);
		if (tag == "union")
			for (size_t i = 0; i < class_scope->bindings.size(); ++i)
				if (class_scope->bindings[i].kind != BIND_TYPE)
				{
					Binding injected = class_scope->bindings[i];
					injected.injected_member = true;
					injected.injected_owner = type;
					scope->add(injected);
				}
		return type;
	}
	const bool qualified_definition = TopLevelScopeSeparator(raw_name) != string::npos;
	Scope* owner = scope;
	if (qualified_definition)
	{
		const size_t separator = TopLevelScopeSeparator(raw_name);
		PathTarget prefix = ResolvePath(scope, raw_name.substr(0, separator));
		owner = prefix.binding ? ScopeForType(prefix.binding->type) : prefix.scope;
		if (!owner) throw logic_error("unknown class owner");
	}
	TypePtr type;
	Binding* existing = owner->local(name);
	if (qualified_definition && (!existing || existing->kind != BIND_TYPE ||
		!existing->type || existing->type->kind != TYPE_CLASS))
		throw logic_error("qualified class has no declaration");
	if (existing && existing->kind == BIND_TYPE && existing->type &&
		existing->type->kind == TYPE_CLASS)
		type = existing->type;
	else
	{
		type.reset(new Type(TYPE_CLASS, name));
		type->tag = tag;
		if (!owner->qualified_prefix.empty()) type->name = owner->qualified_prefix + "::" + name;
		AddTypeBinding(owner, name, type);
	}
	type->tag = tag;
	if(node->template_instantiation) {
		type->template_specialization = true;
		type->template_primary = node->template_primary;
		type->template_arguments = node->template_arguments;
	}
	type->dependent_base_lookup = node->dependent_base_lookup;
	type->complete = true;
	type->layout_complete = false;
	type->direct_base.reset();
	type->direct_bases.clear();
	type->direct_base_offsets.clear();
	type->virtual_methods.clear();
	type->polymorphic = false;
	type->has_vpointer = false;
	Scope* class_scope = ClassScope(type, owner, name);
	// Seed complete typed lookup for injected member-class specializations.
	PredeclareMaterializedNestedClasses(node, class_scope);
	for (size_t i = 0; i < node->children.size(); ++i)
	{
		const CPPGMAstNodePtr child = node->children[i];
		if (!child || child->kind != "base-clause") continue;
		for (size_t j = 0; j < child->children.size(); ++j)
		{
			const CPPGMAstNodePtr base = child->children[j];
			if (!base) continue;
			const CPPGMAstNodePtr base_name = ChildOfKind(base, "base-name");
			if (!base_name) continue;
			TypePtr resolved_base = ResolveType(owner, base_name->value);
			if (!resolved_base) continue;
			type->direct_bases.push_back(resolved_base);
			if (!type->direct_base) type->direct_base = resolved_base;
		}
		break;
	}
	// Generated class specializations can contain a member function before the
	// copied field declarations that function uses.  Populate those specialized
	// scopes first; retain the established source-order processing for ordinary
	// classes so declaration-order-sensitive diagnostics and ABI facts stay
	// unchanged for earlier assignments.
	if (node->template_instantiation)
		for (unsigned int pass = 0; pass < 2; ++pass)
			for (size_t i = 0; i < node->children.size(); ++i) {
				const CPPGMAstNodePtr child = node->children[i];
				if (!child || child->kind == "class-key") continue;
				const bool function_definition = child->kind == "function-definition" ||
					child->kind == "special-member-definition";
				if ((pass == 0) == function_definition) continue;
				Process(child, class_scope);
			}
	else
		for (size_t i = 0; i < node->children.size(); ++i)
			if (node->children[i] && node->children[i]->kind != "class-key")
				Process(node->children[i], class_scope);
	// direct_bases is the canonical base list; direct_base remains its first
	// element for the single-inheritance consumers retained from earlier PAs.
	type->class_members.clear();
	RecordClassMembers(node, type, owner, class_scope);
	if (node->template_instantiation && !LayoutDependenciesReady(type))
		pending_class_layouts_.push_back(PendingClassLayout(node, type, class_scope));
	else
		ComputeClassLayout(node, type, class_scope);
	class_types_[node.get()] = type;
	return type;
}

TypePtr Analyzer::ProcessForwardClass(const CPPGMAstNodePtr& node, Scope* scope)
{
	const string raw_name = node->value;
	const string name = LastComponent(raw_name);
	if (name.empty()) throw logic_error("anonymous class forward declaration");
	Scope* owner = scope;
	if (TopLevelScopeSeparator(raw_name) != string::npos)
	{
		const size_t separator = TopLevelScopeSeparator(raw_name);
		PathTarget prefix = ResolvePath(scope, raw_name.substr(0, separator));
		owner = prefix.binding ? ScopeForType(prefix.binding->type) : prefix.scope;
		if (!owner) throw logic_error("unknown forward class owner");
	}
	Binding* existing = owner->local(name);
	if (existing && existing->kind == BIND_TYPE)
	{
		if (existing->type && existing->type->kind == TYPE_CLASS)
			ApplyClassAttributes(node, existing->type, scope);
		return existing->type;
	}
	existing = ResolveBinding(scope, name);
	if (existing && (existing->kind == BIND_TYPE || existing->kind == BIND_TYPE_ALIAS) &&
		existing->type && existing->type->kind == TYPE_CLASS)
	{
		ApplyClassAttributes(node, existing->type, scope);
		return existing->type;
	}
	TypePtr type(new Type(TYPE_CLASS, name));
	type->tag = ClassKey(node);
	type->complete = false;
	ApplyClassAttributes(node, type, scope);
	if (!owner->qualified_prefix.empty()) type->name = owner->qualified_prefix + "::" + name;
	AddTypeBinding(owner, name, type);
	return type;
}
