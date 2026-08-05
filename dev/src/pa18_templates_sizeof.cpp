#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;
namespace pa18_templates_internal {

bool PA18TemplateExpander::EvaluateExpandedSizeofText(const string& raw,
	const string& context, const map<string, string>& substitutions,
	PA19IntegralValue* result, string* expanded)
{
	string expanded_size = raw;
	bool expanded_size_any = false;
	for(size_t search = expanded_size.find("sizeof("); search != string::npos; ) {
		const size_t open = search + 6;
		int depth = 0; size_t close = string::npos;
		for(size_t position = open; position < expanded_size.size(); ++position) {
			if(expanded_size[position] == '(') ++depth;
			else if(expanded_size[position] == ')' && --depth == 0) { close = position; break; }
		}
		if(close == string::npos) break;
		const string operand = expanded_size.substr(open + 1, close - open - 1);
		string type_operand = ResolveAlias(CanonicalSpelling(ReplaceIdentifiers(operand,
			substitutions)), context);
		map<string, CPPGMAstNodePtr>::const_iterator declaration =
			class_declarations_.find(type_operand);
		const bool incomplete = declaration != class_declarations_.end() && declaration->second &&
			declaration->second->kind == "class-forward-declaration";
		size_t size = incomplete ? 0 : EstimateTypeSize(type_operand, context);
		string call_type;
		const bool call_result = !size && (operand.find('(') != string::npos ||
			operand.find('{') != string::npos) && operand.find("**") == string::npos &&
			FunctionCallResultType(operand, context, substitutions, &call_type);
		if(call_result) {
			call_type = ResolveAlias(CanonicalSpelling(RemoveMarker(RewriteText(
				call_type, context, substitutions, 0))), context);
			while(call_type.size() > 1 && call_type[call_type.size() - 1] == '&')
				call_type = CanonicalSpelling(call_type.substr(0, call_type.size() - 1));
			size = EstimateTypeSize(call_type, context);
		}
		if(!size) {
			// `sizeof` may contain an overloaded operator expression rather
			// than a call-expression.  The typed expression rewriter already
			// resolves that operator result (including using-directive lookup),
			// so use its object type before treating the operand as dependent.
			const string expression_type = ExpressionTypeSpelling(operand, context,
				substitutions);
			if(!expression_type.empty()) {
				string resolved_expression_type = ResolveAlias(
					CanonicalSpelling(expression_type), context);
				while(resolved_expression_type.size() > 1 &&
					(resolved_expression_type[resolved_expression_type.size() - 1] == '&' ||
					 resolved_expression_type[resolved_expression_type.size() - 1] == '*'))
					resolved_expression_type = CanonicalSpelling(
						resolved_expression_type.substr(0, resolved_expression_type.size() - 1));
				size = EstimateTypeSize(resolved_expression_type, context);
			}
		}
		if(size) {
			const string replacement = IntegralValueSpelling(PA19IntegralValue::Unsigned(
				static_cast<unsigned long long>(size), "unsigned long", 64));
			expanded_size.replace(search, close - search + 1, replacement);
			expanded_size_any = true;
			search = expanded_size.find("sizeof(", search + replacement.size());
			continue;
		}
		search = expanded_size.find("sizeof(", close + 1);
	}
	if(expanded_size_any && expanded_size != raw) {
		PA19ConstantExpressionParser parser(constant_values_, substitutions,
			constant_type_sizes_, constant_type_alignments_, type_aliases_);
		if(parser.Evaluate(expanded_size, result)) return true;
	}
	if(expanded) *expanded = expanded_size;
	return false;
}
}
