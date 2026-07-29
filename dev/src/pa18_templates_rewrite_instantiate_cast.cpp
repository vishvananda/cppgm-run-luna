#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
#include "pa18_templates_rewrite_instantiate.h"

using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::EvaluateIntegralTextCStyleCast(const string& raw,
	const string& context, const map<string, string>& substitutions,
	PA19IntegralValue* result)
{
	// The parser preserves a C-style integral cast as a leading parenthesized
	// type-id, while the expression evaluator otherwise treats that parenthesis
	// as an ordinary grouping expression.  Recognize only a prefix whose inner
	// spelling resolves to an integral type, so `(I) + 1` remains a normal
	// expression rather than being mistaken for a cast.
	if(raw.empty() || raw[0] != '(') return false;
	int depth = 0;
	size_t close = string::npos;
	for(size_t position = 0; position < raw.size(); ++position) {
		if(raw[position] == '(') ++depth;
		else if(raw[position] == ')' && --depth == 0) {
			close = position;
			break;
		}
	}
	if(close == string::npos || close + 1 >= raw.size()) return false;
	string target = CanonicalSpelling(raw.substr(1, close - 1));
	const string compact = [&target]() {
		string result;
		for(size_t i = 0; i < target.size(); ++i)
			if(!isspace(static_cast<unsigned char>(target[i]))) result += target[i];
		return result;
	}();
	if(compact == "unsignedlong") target = "unsigned long";
	else if(compact == "unsignedlonglong") target = "unsigned long long";
	else if(compact == "longlong") target = "long long";
	else if(compact == "signedlong") target = "signed long";
	else if(compact == "signedlonglong") target = "signed long long";
	else if(compact == "unsignedint") target = "unsigned int";
	else if(compact == "signedint") target = "signed int";
	else if(compact == "shortint") target = "short int";
	const PA19IntegralType target_type = PA19Type(ResolveAlias(target, context));
	if(!target_type.integral) return false;
	PA19IntegralValue converted;
	if(!EvaluateIntegralText(raw.substr(close + 1), context, substitutions, &converted))
		return false;
	*result = PA19Convert(converted, target_type);
	return result->known;
}

} // namespace pa18_templates_internal
