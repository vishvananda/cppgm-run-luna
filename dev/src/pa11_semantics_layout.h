#pragma once

inline bool SameLayoutType(const TypePtr& left, const TypePtr& right)
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

inline bool IsBitFieldType(const TypePtr& type)
{
	if (!type) return false;
	if (type->kind == TYPE_ENUM) return true;
	if (type->kind != TYPE_FUNDAMENTAL) return false;
	return type->name != "float" && type->name != "double" &&
		type->name != "long double" && type->name != "void" &&
		type->name != "nullptr_t";
}
