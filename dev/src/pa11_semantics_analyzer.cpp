#include "pa11_semantics_analyzer.h"

size_t Analyzer::AlignUp(size_t value, size_t alignment)
{
	if (alignment == 0) alignment = 1;
	const size_t remainder = value % alignment;
	return remainder == 0 ? value : value + alignment - remainder;
}

size_t Analyzer::AttributeAlignment(const string& spelling, Scope* scope) const
{
	const size_t open = spelling.find('(');
	const size_t close = spelling.rfind(')');
	if (open == string::npos || close == string::npos || close <= open)
		return 0;
	string value = spelling.substr(open + 1, close - open - 1);
	while (!value.empty() && isspace(static_cast<unsigned char>(value[0]))) value.erase(value.begin());
	while (!value.empty() && isspace(static_cast<unsigned char>(value[value.size() - 1]))) value.erase(value.size() - 1);
	if (value.empty()) return 0;
	char* end = 0;
	errno = 0;
	const long long numeric = strtoll(value.c_str(), &end, 0);
	if (end != value.c_str() && *end == '\0' && errno != ERANGE && numeric > 0)
		return static_cast<size_t>(numeric);
	string compact;
	for (size_t i = 0; i < value.size(); ++i)
		if (!isspace(static_cast<unsigned char>(value[i]))) compact += value[i];
	if (compact == "bool" || compact == "char" || compact == "signedchar" ||
		compact == "unsignedchar") return 1;
	if (compact == "short" || compact == "shortint" || compact == "unsignedshort" ||
		compact == "unsignedshortint" || compact == "char16_t") return 2;
	if (compact == "int" || compact == "unsigned" || compact == "unsignedint" ||
		compact == "float" || compact == "char32_t" || compact == "wchar_t") return 4;
	if (compact == "long" || compact == "longint" || compact == "unsignedlong" ||
		compact == "unsignedlongint" || compact == "double" || compact == "longlong" ||
		compact == "longlongint" || compact == "unsignedlonglong" ||
		compact == "unsignedlonglongint") return 8;
	if (compact == "longdouble") return 16;
	try
	{
		TypePtr type = ResolveType(scope, value);
		return TypeAlignment(type);
	}
	catch (const exception&)
	{
		return 0;
	}
}

void Analyzer::ApplyClassAttributes(const CPPGMAstNodePtr& node, const TypePtr& type,
	Scope* scope)
{
	if (!node || !type) return;
	for (size_t i = 0; i < node->children.size(); ++i)
	{
		const CPPGMAstNodePtr child = node->children[i];
		if (!child || child->kind != "attribute") continue;
		const size_t alignment = AttributeAlignment(child->value, scope);
		if (alignment > type->explicit_alignment) type->explicit_alignment = alignment;
	}
}

void Analyzer::ComputeClassLayout(const CPPGMAstNodePtr& node, const TypePtr& type,
	Scope* class_scope)
{
	if (!type || type->layout_in_progress) return;
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
	string bit_unit_type;
	const bool union_type = type->is_union;
	for (size_t i = 0; i < type->class_members.size(); ++i)
	{
		ClassMemberInfo& member = type->class_members[i];
		if (member.is_static || !member.type) continue;
		const size_t member_size = TypeSize(member.type);
		const size_t member_alignment = max<size_t>(1, TypeAlignment(member.type));
		maximum_alignment = max(maximum_alignment, member_alignment);
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
			if (width < 0 || width > capacity)
				throw logic_error("invalid bit-field width");
			if (width == 0)
			{
				if (bits_used != 0) offset = bit_unit_offset + bit_unit_size;
				bits_used = 0;
				bit_unit_size = 0;
				bit_unit_type.clear();
				offset = AlignUp(offset, member_alignment);
				member.offset = static_cast<long long>(offset);
				member.bit_offset = 0;
				continue;
			}
			const string current_type = TypeText(member.type, true);
			if (bits_used == 0 || bit_unit_size != member_size ||
				bit_unit_alignment != member_alignment || bit_unit_type != current_type ||
				bits_used + width > capacity)
			{
				if (bits_used != 0) offset = bit_unit_offset + bit_unit_size;
				offset = AlignUp(offset, member_alignment);
				bit_unit_offset = offset;
				bit_unit_size = member_size;
				bit_unit_alignment = member_alignment;
				bit_unit_type = current_type;
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
				bit_unit_type.clear();
			}
			continue;
		}
		if (bits_used != 0)
		{
			offset = bit_unit_offset + bit_unit_size;
			bits_used = 0;
			bit_unit_size = 0;
			bit_unit_type.clear();
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
				binding->is_bit_field = true;
				binding->bit_width = width;
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
				binding->is_mutable_member = false;
				binding->access = access;
				binding->declaration = child;
				for (size_t k = 0; k < child->children[0]->children.size(); ++k)
				{
					const string& value = child->children[0]->children[k]->value;
					if (value.find(":static") != string::npos) binding->is_static_member = true;
					if (value.find(":mutable") != string::npos) binding->is_mutable_member = true;
				}
			}
			if (facts.is_typedef || field_type->kind == TYPE_FUNCTION || name.empty()) continue;
			ClassMemberInfo member;
			member.name = name;
			member.type = field_type;
			member.is_static = binding && binding->is_static_member;
			member.initializer = item->children.size() > 1 ? item->children[1] : CPPGMAstNodePtr();
			type->class_members.push_back(member);
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
	if (existing && existing->kind == BIND_TYPE) return existing->type;
	existing = ResolveBinding(scope, name);
	if (existing && (existing->kind == BIND_TYPE || existing->kind == BIND_TYPE_ALIAS) &&
		existing->type && existing->type->kind == TYPE_CLASS) return existing->type;
	TypePtr type(new Type(TYPE_CLASS, name));
	type->tag = ClassKey(node);
	type->complete = false;
	ApplyClassAttributes(node, type, scope);
	AddTypeBinding(scope, name, type);
	return type;
}
