#include "pa11_semantics_analyzer.h"

namespace {

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

}

ConstantValue Analyzer::FromIntegralValue(const PA19IntegralValue& value) const
{
	if (!value.known) return ConstantValue();
	ConstantValue result(true, PA19Signed(value));
	result.unsigned_value = PA19Raw(value);
	result.is_unsigned = value.is_unsigned;
	result.bits = value.bits;
	result.type_name = value.type;
	return result;
}

PA19IntegralValue Analyzer::ToIntegralValue(const ConstantValue& value) const
{
	if (!value.known) return PA19IntegralValue();
	PA19IntegralValue result;
	result.known = true;
	result.is_unsigned = value.is_unsigned;
	result.bits = value.bits == 0 ? 64 : value.bits;
	result.raw = value.unsigned_value;
	result.type = value.type_name.empty() ?
		(value.is_unsigned ? "unsigned long long" : "long long") : value.type_name;
	return result;
}

PA19IntegralValue Analyzer::ParseLiteralValue(const string& raw) const
{
	PA19IntegralValue result;
	if (PA19DecodeCharacter(raw, &result) || PA19ParseInteger(raw, &result)) return result;
	throw logic_error("unsupported constant expression");
}

long long Analyzer::ParseLiteral(const string& raw) const
{
	return PA19Signed(ParseLiteralValue(raw));
}

ConstantValue Analyzer::Evaluate(const CPPGMAstNodePtr& expression, Scope* scope)
{
	if (!expression) return ConstantValue();
	if (expression->kind == "literal") return FromIntegralValue(ParseLiteralValue(expression->value));
	if (expression->kind == "keyword-literal")
		return FromIntegralValue(PA19IntegralValue::Signed(
			OperatorFromNode(expression->value) == "true" ? 1 : 0, "bool", 1));
	if (expression->kind == "id-expression")
	{
		Binding* binding = ResolveBinding(scope, expression->value);
		if (!binding || !binding->has_value) return ConstantValue();
		ConstantValue result(true, binding->value);
		result.unsigned_value = binding->unsigned_value;
		result.is_unsigned = binding->value_is_unsigned;
		result.bits = binding->value_bits;
		result.type_name = binding->value_type.empty() ? TypeText(binding->type, true) : binding->value_type;
		return result;
	}
	if (expression->kind == "parenthesized-expression")
		return expression->children.empty() ? ConstantValue() : Evaluate(expression->children[0], scope);
	if (expression->kind == "subscript-expression" && expression->children.size() >= 2 &&
		expression->children[0] && expression->children[0]->kind == "literal") {
		ConstantValue index = Evaluate(expression->children[1], scope);
		if (!index.known) return ConstantValue();
		ostringstream spelling;
		spelling << expression->children[0]->value << "[" << PA19Raw(ToIntegralValue(index)) << "]";
		map<string, PA19IntegralValue> constants;
		map<string, string> substitutions;
		PA19ConstantExpressionParser parser(constants, substitutions);
		PA19IntegralValue value;
		return parser.Evaluate(spelling.str(), &value) ? FromIntegralValue(value) : ConstantValue();
	}
	if (expression->kind == "sizeof-expression" || expression->kind == "type-trait-expression")
	{
		if (expression->children.empty()) return ConstantValue();
		const CPPGMAstNodePtr child = expression->children[0];
		TypePtr type;
		if (child->kind == "type-id") type = TypeFromTypeId(child, scope);
		else type = ExpressionType(child, scope);
		const bool align = expression->kind == "type-trait-expression";
		return FromIntegralValue(PA19IntegralValue::Unsigned(
			static_cast<unsigned long long>(align ? TypeAlignment(type) : TypeSize(type)),
			"unsigned long", 64));
	}
	if (expression->kind == "cast-expression")
	{
		if (expression->children.size() < 2) return ConstantValue();
		const ConstantValue operand = Evaluate(expression->children[1], scope);
		if (!operand.known) return ConstantValue();
		const TypePtr target = TypeFromTypeId(expression->children[0], scope);
		return FromIntegralValue(PA19Convert(ToIntegralValue(operand), PA19Type(TypeText(target, true))));
	}
	if (expression->kind == "call-expression" && expression->children.size() >= 2 &&
		expression->children[0] && expression->children[0]->kind == "id-expression" &&
		expression->children[1] && expression->children[1]->kind == "paren-argument-list" &&
		!expression->children[1]->children.empty()) {
		const PA19IntegralType target = PA19Type(expression->children[0]->value);
		if (target.integral) {
			const ConstantValue operand = Evaluate(expression->children[1]->children[0], scope);
			return operand.known ? FromIntegralValue(PA19Convert(ToIntegralValue(operand), target)) : ConstantValue();
		}
	}
	if (expression->kind == "unary-expression")
	{
		if (expression->children.empty()) return ConstantValue();
		ConstantValue child = Evaluate(expression->children[0], scope);
		if (!child.known) return child;
		PA19IntegralValue value = ToIntegralValue(child);
		const string op = OperatorFromNode(expression->value);
		if (op == "+") return FromIntegralValue(PA19Promote(value));
		if (op == "-") {
			value = PA19Promote(value);
			const PA19IntegralType type = PA19Type(value.type);
			const unsigned long long raw = (0ULL - PA19Raw(value)) & PA19Mask(type.bits);
			return FromIntegralValue(type.is_unsigned ? PA19IntegralValue::Unsigned(raw, type.name, type.bits) :
				PA19IntegralValue::Signed(static_cast<long long>(raw), type.name, type.bits));
		}
		if (op == "!") return FromIntegralValue(PA19IntegralValue::Signed(!PA19Raw(value), "int", 32));
		if (op == "~") {
			value = PA19Promote(value);
			const PA19IntegralType type = PA19Type(value.type);
			const unsigned long long raw = (~PA19Raw(value)) & PA19Mask(type.bits);
			return FromIntegralValue(type.is_unsigned ? PA19IntegralValue::Unsigned(raw, type.name, type.bits) :
				PA19IntegralValue::Signed(static_cast<long long>(raw), type.name, type.bits));
		}
		return ConstantValue();
	}
	if (expression->kind == "conditional-expression" && expression->children.size() == 3)
	{
		ConstantValue condition = Evaluate(expression->children[0], scope);
		return !condition.known ? ConstantValue() :
			Evaluate(expression->children[PA19Raw(ToIntegralValue(condition)) ? 1 : 2], scope);
	}
	if (expression->kind == "binary-expression" || expression->kind == "assignment-expression")
	{
		if (expression->children.size() < 2) return ConstantValue();
		ConstantValue left = Evaluate(expression->children[0], scope);
		ConstantValue right = Evaluate(expression->children[1], scope);
		if (!left.known || !right.known) return ConstantValue();
		return FromIntegralValue(PA19Binary(OperatorFromNode(expression->value),
			ToIntegralValue(left), ToIntegralValue(right)));
	}
	return ConstantValue();
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

void Analyzer::ProcessUsingDeclaration(const CPPGMAstNodePtr& node, Scope* scope)
	{
		CPPGMAstNodePtr target_node = ChildOfKind(node, "target");
		if (!target_node) throw logic_error("invalid using declaration");
		const string target_name = target_node->value;
		if (target_name.find('<') != string::npos)
			throw logic_error("using declaration cannot name template-id");
		const size_t target_separator = target_name.rfind("::");
		const string target_owner_name = target_separator == string::npos ? string() :
			LastComponent(target_name.substr(0, target_separator));
		vector<Binding*> targets;
		if (target_separator != string::npos)
		{
			PathTarget owner = ResolvePath(scope, target_name.substr(0, target_separator));
			Scope* owner_scope = owner.scope;
			if (!owner_scope && owner.binding) owner_scope = ScopeForType(owner.binding->type);
			if (owner_scope)
				for (size_t i = 0; i < owner_scope->bindings.size(); ++i)
					if (owner_scope->bindings[i].name == LastComponent(target_name))
						targets.push_back(&owner_scope->bindings[i]);
		}
		if (targets.empty()) {
			Binding* target = ResolveBinding(scope, target_name);
			if (target) targets.push_back(target);
		}
		if (targets.empty()) throw logic_error("using target is not a declaration");
		for (size_t target_index = 0; target_index < targets.size(); ++target_index)
		{
			Binding imported = *targets[target_index];
			const bool constructor_target = imported.kind == BIND_FUNCTION &&
				scope && scope->kind == SCOPE_CLASS && scope->owner_type &&
				!target_owner_name.empty() && LastComponent(target_name) == target_owner_name;
			imported.name = constructor_target ? LastComponent(scope->owner_type->name) :
				LastComponent(target_name);
			// Scope::add preserves an already-qualified binding name.  An imported
			// constructor is a declaration in the derived class for PA11 lookup,
			// so let the destination scope form its qualified identity.  Ordinary
			// using-declarations retain the source identity for overload lowering.
			if (constructor_target) imported.qualified_name.clear();
			scope->add(imported);
		}
	}
void Analyzer::ProcessNamespace(const CPPGMAstNodePtr& node, Scope* scope)
	{
		const string name = node->value;
		if (name != "<unnamed>" && (scope->local(name) ||
			scope->namespace_aliases.find(name) != scope->namespace_aliases.end()))
			throw logic_error("namespace conflicts with declaration");
		Scope* namespace_scope = 0;
		map<string, Scope*>::iterator found = scope->namespace_children.find(name);
		if (name != "<unnamed>" && found != scope->namespace_children.end())
			namespace_scope = found->second;
		else
		{
			namespace_scope = NewChild(scope, SCOPE_NAMESPACE, name);
			if (name != "<unnamed>") scope->namespace_children[name] = namespace_scope;
		}
		if (name == "<unnamed>") {
			// Keep anonymous-namespace identity in the typed scope path.  The
			// enclosing namespace still exposes the scope through its using path,
			// while symbol lowering needs the stable internal namespace component.
			size_t occurrence = 0;
			for (size_t child = 0; child < scope->children.size(); ++child)
				if (scope->children[child] &&
					scope->children[child]->kind == SCOPE_NAMESPACE &&
					scope->children[child]->name == "<unnamed>") ++occurrence;
			ostringstream suffix;
			suffix << occurrence;
			const string component = "_GLOBAL__N_" + suffix.str();
			namespace_scope->qualified_prefix = scope->qualified_prefix.empty() ?
				component : scope->qualified_prefix + "::" + component;
		}
		if (name == "<unnamed>") scope->using_directives.push_back(namespace_scope);
		namespace_scopes_[node.get()] = namespace_scope;
			namespace_scope->inline_namespace = HasKind(node, "inline");
			if (namespace_scope->inline_namespace) {
				bool already_visible = false;
				for (size_t i = 0; i < scope->using_directives.size(); ++i)
					if (scope->using_directives[i] == namespace_scope) already_visible = true;
				if (!already_visible) scope->using_directives.push_back(namespace_scope);
			}
		for (size_t i = 0; i < node->children.size(); ++i)
			if (node->children[i]->kind != "inline") Process(node->children[i], namespace_scope);
	}

namespace {

bool SameLayoutType(const TypePtr& left, const TypePtr& right)
{
	if (left == right) return true;
	if (!left || !right || left->kind != right->kind ||
		left->is_const != right->is_const || left->is_volatile != right->is_volatile)
		return false;
	switch (left->kind)
	{
	case TYPE_FUNDAMENTAL:
	case TYPE_TEMPLATE_PARAMETER:
	case TYPE_TEMPLATE_TEMPLATE_PARAMETER:
		return left->name == right->name;
	case TYPE_CLASS:
		return left->name == right->name && left->tag == right->tag;
	case TYPE_ENUM:
		return left->name == right->name && left->scoped_enum == right->scoped_enum;
	case TYPE_POINTER:
	case TYPE_LVALUE_REFERENCE:
	case TYPE_RVALUE_REFERENCE:
		return SameLayoutType(left->child, right->child);
	case TYPE_ARRAY:
		return left->bound == right->bound &&
			SameLayoutType(left->child, right->child);
	case TYPE_FUNCTION:
		if (left->variadic != right->variadic ||
			left->function_const != right->function_const ||
			left->function_volatile != right->function_volatile ||
			!SameLayoutType(left->child, right->child) ||
			left->parameters.size() != right->parameters.size()) return false;
		for (size_t i = 0; i < left->parameters.size(); ++i)
			if (!SameLayoutType(left->parameters[i], right->parameters[i])) return false;
		return true;
	case TYPE_MEMBER_POINTER:
		return SameLayoutType(left->member_owner, right->member_owner) &&
			SameLayoutType(left->child, right->child);
	}
	return false;
}

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

bool IsBitFieldType(const TypePtr& type)
{
	if (!type) return false;
	if (type->kind == TYPE_ENUM) return true;
	if (type->kind != TYPE_FUNDAMENTAL) return false;
	return type->name != "float" && type->name != "double" &&
		type->name != "long double" && type->name != "void" &&
		type->name != "nullptr_t";
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
	TypePtr function_type = BuildDeclarator(declarator, base, scope);
	if (!function_type || function_type->kind != TYPE_FUNCTION)
		throw logic_error("definition is not a function");
	Scope* declaration_scope = scope;
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
			name = LastComponent(raw_name);
		}
	}
	else if (scope->kind == SCOPE_CLASS)
	{
		member_owner = scope->owner_type;
	}
	Binding binding(BIND_FUNCTION, name, function_type);
	binding.hidden_friend = facts.is_friend;
	binding.friend_owner = facts.is_friend ? member_owner : TypePtr();
	binding.is_member = static_cast<bool>(member_owner) && !facts.is_friend;
	binding.is_static = facts.is_static;
	binding.member_owner = member_owner;
	binding.is_virtual = facts.is_virtual;
	binding.is_override = HasNodeValue(declarator, "virt-specifier", "override");
	binding.is_final = HasNodeValue(declarator, "virt-specifier", "final");
	Binding* stored = declaration_scope->add(binding);
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
	Scope* function_scope = NewChild(declaration_scope, SCOPE_FUNCTION, name);
	function_scopes_[node.get()] = function_scope;
	AddFunctionParameters(function_scope, declarator, declaration_scope);
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
		if (!member_name && !type_name && !IsDependentTemplateName(current, node->value) &&
			!ResolveBinding(current, node->value))
			throw logic_error("unknown nondependent template name: " + node->value);
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
		TypePtr type = BuildDeclarator(declarator, base, scope);
		if (facts.is_constexpr && type->kind != TYPE_FUNCTION) type = CloneWithCv(type, true, false);
		const string name = FirstIdentifier(declarator);
		if (name.empty()) continue;
		CPPGMAstNodePtr initializer = item->children.size() > 1 ? item->children[1] : CPPGMAstNodePtr();
		if (facts.is_typedef)
		{
			AddTypeBinding(scope, name, type, true);
			continue;
		}
		Binding binding(type->kind == TYPE_FUNCTION ? BIND_FUNCTION : BIND_VARIABLE, name, type);
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
		if (initializer && (facts.is_const || facts.is_constexpr))
		{
			CPPGMAstNodePtr expression = initializer->children.empty() ?
				CPPGMAstNodePtr() : initializer->children[0];
			const bool floating_literal = expression && expression->kind == "literal" &&
				(expression->value.find('.') != string::npos ||
					expression->value.find('e') != string::npos ||
					expression->value.find('E') != string::npos ||
					expression->value.find('p') != string::npos ||
					expression->value.find('P') != string::npos);
			const bool string_literal = expression && expression->kind == "literal" &&
				!expression->value.empty() && expression->value[0] == '"';
			if (!floating_literal && !string_literal)
			{
				ConstantValue value = Evaluate(expression, scope);
				if (value.known) {
					binding.has_value = true;
					binding.value = value.value;
					binding.unsigned_value = value.unsigned_value;
					binding.value_is_unsigned = value.is_unsigned;
					binding.value_bits = value.bits;
					binding.value_type = value.type_name;
				}
			}
		}
		scope->add(binding);
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
		TypePtr type = TypeFromTypeId(argument, scope);
		alignment = TypeAlignment(type);
		if (alignment == 0) throw logic_error("alignment type has no alignment");
	}
	else
	{
		ConstantValue value = Evaluate(argument, scope);
		if (!value.known || value.value < 0)
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
		!type->direct_base->complete)
		throw logic_error("incomplete direct base class");
	const bool empty_base = type->direct_base && EmptyBaseStorage(type->direct_base);
	const bool owns_vpointer = type->has_vpointer;
	type->direct_base_offset = 0;
	size_t offset = type->direct_base && !empty_base ? TypeSize(type->direct_base) : 0;
	size_t maximum_alignment = type->direct_base && !empty_base ?
		TypeAlignment(type->direct_base) : 1;
	if (owns_vpointer) ApplyPolymorphicLayout(type, empty_base,
		type->direct_base && !empty_base ? TypeSize(type->direct_base) : 0,
		type->direct_base && !empty_base ? TypeAlignment(type->direct_base) : 1,
		&offset, &maximum_alignment);
	size_t union_size = offset;

	size_t bit_unit_offset = 0;
	size_t bit_unit_size = 0;
	size_t bit_unit_alignment = 1;
	long long bits_used = 0;
	TypePtr bit_unit_type;
	const bool union_type = type->is_union;
	for (size_t i = 0; i < type->class_members.size(); ++i)
	{
		ClassMemberInfo& member = type->class_members[i];
		if (member.is_static || !member.type) continue;
		const size_t member_size = TypeSize(member.type);
		const size_t member_alignment = max<size_t>(1, TypeAlignment(member.type));
		maximum_alignment = max(maximum_alignment, member_alignment);
		if (member.bit_field)
		{
			if (!IsBitFieldType(member.type))
				throw logic_error("bit-field type is not integral or enum");
			if (member_size > static_cast<size_t>(numeric_limits<long long>::max() / 8))
				throw logic_error("bit-field storage unit is too large");
			const long long capacity = static_cast<long long>(member_size * 8);
			if (member.bit_width < 0 || member.bit_width > capacity)
				throw logic_error("invalid bit-field width");
			if (member.bit_width == 0 && !member.name.empty())
				throw logic_error("named zero-width bit-field");
		}
		if (union_type)
		{
			member.offset = 0;
			if (member.bit_field) member.bit_offset = 0;
			union_size = max(union_size, member_size);
			continue;
		}
		if (member.bit_field)
		{
			const long long width = member.bit_width;
			const long long capacity = static_cast<long long>(member_size * 8);
			if (width == 0)
			{
				if (bits_used != 0) offset = bit_unit_offset + bit_unit_size;
				bits_used = 0;
				bit_unit_size = 0;
				bit_unit_type.reset();
				offset = AlignUp(offset, member_alignment);
				member.offset = static_cast<long long>(offset);
				member.bit_offset = 0;
				continue;
			}
			if (bits_used == 0 || bit_unit_size != member_size ||
				bit_unit_alignment != member_alignment || !SameLayoutType(bit_unit_type, member.type) ||
				bits_used + width > capacity)
			{
				if (bits_used != 0) offset = bit_unit_offset + bit_unit_size;
				offset = AlignUp(offset, member_alignment);
				bit_unit_offset = offset;
				bit_unit_size = member_size;
				bit_unit_alignment = member_alignment;
				bit_unit_type = member.type;
				bits_used = 0;
			}
			member.offset = static_cast<long long>(bit_unit_offset);
			member.bit_offset = bits_used;
			bits_used += width;
			if (bits_used == capacity)
			{
				offset = bit_unit_offset + bit_unit_size;
				bits_used = 0;
				bit_unit_size = 0;
				bit_unit_type.reset();
			}
			continue;
		}
		if (bits_used != 0)
		{
			offset = bit_unit_offset + bit_unit_size;
			bits_used = 0;
			bit_unit_size = 0;
			bit_unit_type.reset();
		}
		offset = AlignUp(offset, member_alignment);
		member.offset = static_cast<long long>(offset);
		member.bit_offset = 0;
		offset += member_size;
	}
	if (bits_used != 0) offset = bit_unit_offset + bit_unit_size;
	if (union_type) offset = union_size;
	type->object_alignment = max(maximum_alignment, type->explicit_alignment);
	if (type->object_alignment == 0) type->object_alignment = 1;
	if (type->class_members.empty() && !type->direct_base && !owns_vpointer) offset = 1;
	type->object_size = AlignUp(max<size_t>(1, offset), type->object_alignment);
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
		if (!value.known) throw logic_error("bit-field width is not constant");
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
			if (!name.empty()) type->friend_names.push_back(name);
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
				if (binding.name != name ||
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
				if (!friend_name.empty()) type->friend_names.push_back(friend_name);
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
	const bool qualified_definition = raw_name.find("::") != string::npos;
	Scope* owner = scope;
	if (qualified_definition)
	{
		const size_t separator = raw_name.rfind("::");
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
	type->virtual_methods.clear();
	type->polymorphic = false;
	type->has_vpointer = false;
	Scope* class_scope = ClassScope(type, owner, name);
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
			type->direct_base = ResolveType(owner, base_name->value);
			break;
		}
		break;
	}
	for (size_t i = 0; i < node->children.size(); ++i)
		if (node->children[i]->kind != "class-key") Process(node->children[i], class_scope);
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
			type->direct_base = ResolveType(owner, base_name->value);
			break;
		}
		break;
	}
	type->class_members.clear();
	RecordClassMembers(node, type, owner, class_scope);
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
	if (raw_name.find("::") != string::npos)
	{
		const size_t separator = raw_name.rfind("::");
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
