#pragma once
#include <string>
#include <vector>

namespace pa18_templates_internal {

struct TemplateDefinition;

struct ConcreteOwnerContext
{
	std::string name;
	const TemplateDefinition* definition;
	std::vector<std::string> arguments;
	ConcreteOwnerContext() : name(), definition(0), arguments() {}
};

struct IntegralEvaluationKey
{
	std::string expression, context, owner;
	IntegralEvaluationKey(const std::string& expression_value = std::string(),
		const std::string& context_value = std::string(),
		const std::string& owner_value = std::string())
		: expression(expression_value), context(context_value), owner(owner_value) {}
	bool operator<(const IntegralEvaluationKey& other) const
	{
		if(expression != other.expression) return expression < other.expression;
		if(context != other.context) return context < other.context;
		return owner < other.owner;
	}
};

} // namespace pa18_templates_internal
