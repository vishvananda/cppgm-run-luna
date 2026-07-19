#include "pa11_semantics_analyzer.h"

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

} // namespace

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
	if (argument && argument->kind == "type-id")
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
	size_t offset = type->direct_base ? TypeSize(type->direct_base) : 0;
	size_t maximum_alignment = type->direct_base ? TypeAlignment(type->direct_base) : 1;
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
	if (type->class_members.empty() && !type->direct_base) offset = 1;
	type->object_size = AlignUp(max<size_t>(1, offset), type->object_alignment);
	type->layout_complete = true;
	type->layout_in_progress = false;
	(void)class_scope;
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
		if (child->kind == "bit-field-declaration")
		{
			if (child->children.size() < 2) continue;
			Analyzer::SpecFacts facts;
			TypePtr field_type = TypeFromSpecSeq(child->children[0], class_scope, &facts);
			const CPPGMAstNodePtr field = child->children[1];
			const CPPGMAstNodePtr declarator = field && !field->children.empty() ? field->children[0] : CPPGMAstNodePtr();
			const string name = FirstIdentifier(declarator);
			long long width = 0;
			if (field && field->children.size() > 1)
			{
				ConstantValue value = Evaluate(field->children[1], class_scope);
				if (!value.known) throw logic_error("bit-field width is not constant");
				width = value.value;
			}
			if (!name.empty())
			{
				Binding* binding = class_scope->local(name);
				if (!binding) binding = class_scope->add(Binding(BIND_VARIABLE, name, field_type));
				binding->type = field_type;
				binding->access = access;
				binding->declaration = child;
			}
			ClassMemberInfo member;
			member.name = name;
			member.type = field_type;
			member.bit_field = true;
			member.bit_width = width;
			type->class_members.push_back(member);
			continue;
		}
		if (child->kind == "special-member-definition" ||
			child->kind == "special-member-declaration") continue;
		if (child->kind != "simple-declaration" && child->kind != "function-definition") continue;
		if (child->children.empty()) continue;
		Analyzer::SpecFacts facts;
		TypePtr base = TypeFromSpecSeq(child->children[0], class_scope, &facts);
		CPPGMAstNodePtr list = ChildOfKind(child, "init-declarator-list");
		if (child->kind == "function-definition") list.reset();
		if (!list)
		{
			CPPGMAstNodePtr declarator = child->kind == "function-definition" && child->children.size() > 1 ?
				child->children[1] : CPPGMAstNodePtr();
			if (declarator)
			{
				const string name = FirstIdentifier(declarator);
				Binding* binding = class_scope->local(name);
				if (binding) { binding->access = access; binding->declaration = child; }
			}
			continue;
		}
		for (size_t j = 0; j < list->children.size(); ++j)
		{
			const CPPGMAstNodePtr item = list->children[j];
			if (!item || item->children.empty()) continue;
			const CPPGMAstNodePtr declarator = item->children[0];
			const string name = FirstIdentifier(declarator);
			TypePtr field_type = BuildDeclarator(declarator, base, class_scope);
			Binding* binding = class_scope->local(name);
			if (binding)
			{
				binding->type = field_type;
				binding->access = access;
				binding->declaration = child;
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
	for (size_t i = 0; i < type->class_members.size(); ++i)
	{
		ClassMemberInfo& member = type->class_members[i];
		if (member.name.empty()) continue;
		Binding* binding = class_scope->local(member.name);
		if (binding)
		{
			binding->member_owner = type;
			binding->member_index = i;
		}
	}
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
	TypePtr type;
	Binding* existing = scope->local(name);
	if (existing && existing->kind == BIND_TYPE && existing->type &&
		existing->type->kind == TYPE_CLASS)
		type = existing->type;
	else
	{
		type.reset(new Type(TYPE_CLASS, name));
		type->tag = tag;
		if (!scope->qualified_prefix.empty()) type->name = scope->qualified_prefix + "::" + name;
		AddTypeBinding(scope, name, type);
	}
	type->tag = tag;
	type->complete = true;
	type->layout_complete = false;
	type->direct_base.reset();
	Scope* class_scope = ClassScope(type, scope, name);
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
			type->direct_base = ResolveType(scope, base_name->value);
			break;
		}
		break;
	}
	type->class_members.clear();
	RecordClassMembers(node, type, scope, class_scope);
	ComputeClassLayout(node, type, class_scope);
	class_types_[node.get()] = type;
	return type;
}

TypePtr Analyzer::ProcessForwardClass(const CPPGMAstNodePtr& node, Scope* scope)
{
	const string name = LastComponent(node->value);
	if (name.empty()) throw logic_error("anonymous class forward declaration");
	Binding* existing = scope->local(name);
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
	AddTypeBinding(scope, name, type);
	return type;
}
