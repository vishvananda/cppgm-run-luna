#pragma once
#include "pa11_semantics_analyzer.h"

struct PA12ExprInfo
{
	TypePtr type;
	string category;
	Binding* binding;
	vector<Binding*> candidates;
	bool null_pointer_constant;
	bool known_constant;
	long long constant;

	PA12ExprInfo()
		: type(), category("prvalue"), binding(), candidates(),
		  null_pointer_constant(false), known_constant(false), constant(0) {}
};

struct PA12CallChoice
{
	Binding* binding;
	TypePtr function;
	int worst;
	int total;

	PA12CallChoice()
		: binding(), function(), worst(1000000), total(1000000) {}
};

bool PA12IsReference(const TypePtr& type)
{
	return type && (type->kind == TYPE_LVALUE_REFERENCE ||
		type->kind == TYPE_RVALUE_REFERENCE);
}

TypePtr PA12ValueType(const TypePtr& type)
{
	return PA12IsReference(type) ? type->child : type;
}

TypePtr PA12AdjustedType(const TypePtr& type)
{
	if (!type) return type;
	if (type->kind == TYPE_FUNCTION)
	{
		TypePtr result(new Type(*type));
		result->parameters.clear();
		for (size_t i = 0; i < type->parameters.size(); ++i)
		{
			TypePtr parameter = PA12AdjustedType(type->parameters[i]);
			if (parameter && parameter->kind == TYPE_ARRAY)
				parameter = PointerTo(parameter->child);
			result->parameters.push_back(parameter);
		}
		result->child = PA12AdjustedType(type->child);
		return result;
	}
	if (type->kind == TYPE_POINTER || type->kind == TYPE_LVALUE_REFERENCE ||
		type->kind == TYPE_RVALUE_REFERENCE || type->kind == TYPE_ARRAY)
	{
		TypePtr result(new Type(*type));
		result->child = PA12AdjustedType(type->child);
		return result;
	}
	if (type->kind == TYPE_MEMBER_POINTER)
	{
		TypePtr result(new Type(*type));
		result->member_owner = PA12AdjustedType(type->member_owner);
		result->child = PA12AdjustedType(type->child);
		return result;
	}
	return type;
}

bool PA12IsIntegral(const TypePtr& type)
{
	if (!type) return false;
	if (type->kind == TYPE_ENUM) return !type->scoped_enum;
	return type->kind == TYPE_FUNDAMENTAL &&
		type->name != "bool" && type->name != "float" &&
		type->name != "double" && type->name != "long double" &&
		type->name != "void" && type->name != "nullptr_t";
}

bool PA12IsArithmetic(const TypePtr& type)
{
	return PA12IsIntegral(type) || (type && type->kind == TYPE_FUNDAMENTAL &&
		(type->name == "bool" || type->name == "float" || type->name == "double" ||
		 type->name == "long double"));
}

bool PA12HasConst(const TypePtr& type)
{
	return type && type->is_const;
}

bool PA12HasVolatile(const TypePtr& type)
{
	return type && type->is_volatile;
}

bool PA12SameType(const TypePtr& left, const TypePtr& right, bool ignore_cv = true)
{
	if (!left || !right) return left == right;
	if (left->kind != right->kind) return false;
	if (!ignore_cv && (left->is_const != right->is_const ||
		left->is_volatile != right->is_volatile)) return false;
	switch (left->kind)
	{
	case TYPE_FUNDAMENTAL:
	case TYPE_CLASS:
	case TYPE_ENUM:
	case TYPE_TEMPLATE_PARAMETER:
	case TYPE_TEMPLATE_TEMPLATE_PARAMETER:
		return left->name == right->name && left->tag == right->tag &&
			(left->kind != TYPE_ENUM || left->scoped_enum == right->scoped_enum);
	case TYPE_POINTER:
	case TYPE_LVALUE_REFERENCE:
	case TYPE_RVALUE_REFERENCE:
	case TYPE_ARRAY:
		return left->bound == right->bound && PA12SameType(left->child, right->child, ignore_cv);
	case TYPE_FUNCTION:
		if (left->variadic != right->variadic || left->function_const != right->function_const ||
			left->parameters.size() != right->parameters.size() ||
			!PA12SameType(left->child, right->child, ignore_cv)) return false;
		for (size_t i = 0; i < left->parameters.size(); ++i)
			if (!PA12SameType(left->parameters[i], right->parameters[i], ignore_cv)) return false;
		return true;
	case TYPE_MEMBER_POINTER:
		return PA12SameType(left->member_owner, right->member_owner, ignore_cv) &&
			PA12SameType(left->child, right->child, ignore_cv);
	}
	return false;
}

string PA12Operator(const string& value)
{
	const size_t colon = value.find(':');
	return colon == string::npos ? value : value.substr(colon + 1);
}

string PA12LastComponent(const string& value)
{
	const size_t separator = value.rfind("::");
	return separator == string::npos ? value : value.substr(separator + 2);
}
