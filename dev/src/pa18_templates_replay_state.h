#pragma once
#include <stdexcept>
#include <string>
#include <vector>

namespace pa18_templates_internal {

struct TemplateDefinition;

class PA18SubstitutionFailure : public std::logic_error
{
public:
	explicit PA18SubstitutionFailure(const std::string& message) :
		std::logic_error(message) {}
};

class PA18RecursiveClassTemplateSelection : public PA18SubstitutionFailure
{
public:
	PA18RecursiveClassTemplateSelection() :
		PA18SubstitutionFailure("recursive class template selection") {}
};

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
