#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

CPPGMAstNodePtr PA18TemplateExpander::RewriteRegularNodeValue(
	const CPPGMAstNodePtr& input, const string& context,
	const map<string, string>& substitutions,
	const CPPGMAstNodePtr& result, string* promoted_name)
{
	bool template_replaced = false;
	if(PreserveDependentStaticDeclarator(input, context, substitutions, result,
		promoted_name)) return CPPGMAstNodePtr();
	const bool type_spelling = input->kind == "decl-specifier" ||
		input->kind == "type-name" || input->kind == "type-specifier" ||
		input->kind == "base-name";
	if(input->kind == "literal" && input->value.size() >= 2 &&
		input->value[0] == '"' && input->value[input->value.size() - 1] == '"')
		result->value = input->value;
	else result->value = RewriteText(input->value, context, substitutions, &template_replaced,
		!type_spelling, true,
		defer_type_only_class_definitions_ != 0);
	if((input->kind == "special-member-definition" ||
		input->kind == "special-member-declaration" || input->kind == "identifier") &&
		result->value.compare(0, 8, "operator") == 0) {
		const string target = result->value.substr(8);
		if(!target.empty() && IsIdentifierCharacter(target[0]) && target.find('<') != string::npos) {
			const string rewritten_target = RewriteText(target, context, substitutions, 0,
				true, true, defer_type_only_class_definitions_ != 0);
			if(!rewritten_target.empty()) result->value = "operator" + rewritten_target;
		}
	}
	if(input->kind == "id-expression") {
		const string name = RemoveMarker(result->value);
		const string promoted_local = PromotedLocalClass(name, context);
		if(!promoted_local.empty()) result->value = promoted_local;
	}
	if(PreserveEvaluatedDecltype(input, substitutions, result)) return result;
	if((input->kind == "special-member-definition" ||
		input->kind == "special-member-declaration") &&
		input->value.find("::") != string::npos && result->value.find("::") == string::npos)
		result->value += "::" + LastComponent(input->value);
	if(type_spelling && (result->value.find('[') != string::npos ||
		result->value.find("(&") != string::npos ||
		result->value.find("(*") != string::npos)) {
		bool preserved_template = false;
		const string preserved = RewriteText(input->value, context, substitutions,
			&preserved_template, false, false, defer_type_only_class_definitions_ != 0);
		if(!preserved.empty()) result->value = preserved;
	}
	if(input->kind == "id-expression") {
		map<string, PA19IntegralValue>::const_iterator typed =
			active_integral_substitutions_.find(RemoveMarker(input->value));
		if(typed != active_integral_substitutions_.end() && typed->second.known) {
			const string spelling = IntegralValueSpelling(typed->second);
			if(typed->second.type.name == "bool")
				return CPPGMAstNodePtr(new CPPGMAstNode("keyword-literal",
					PA19Raw(typed->second) ? "KW_TRUE:true" : "KW_FALSE:false"));
			CPPGMAstNodePtr literal(new CPPGMAstNode("literal", spelling));
			literal->initializer_form = input->initializer_form;
			return literal;
		}
		// A non-type pack expansion is carried through the ordinary substitution
		// map because each element is replayed as a fresh scalar spelling.  The
		// parser originally classified the pack identifier as an id-expression;
		// once its spelling is a concrete integer, retain the typed literal node so
		// aggregate initializers and global data lowering can fold it.
		const string spelling = RemoveMarker(result->value);
		size_t digit = (spelling.size() > 0 &&
			(spelling[0] == '+' || spelling[0] == '-')) ? 1 : 0;
		bool integer_spelling = digit < spelling.size();
		for(; integer_spelling && digit < spelling.size(); ++digit)
			if(!isdigit(static_cast<unsigned char>(spelling[digit]))) integer_spelling = false;
		if(integer_spelling)
			return CPPGMAstNodePtr(new CPPGMAstNode("literal", spelling));
	}
	if((type_spelling || input->kind == "id-expression") &&
		result->value.find('<') != string::npos)
		result->value = RewriteText(result->value, context, substitutions, &template_replaced,
			true, true,
			defer_type_only_class_definitions_ != 0);
	if(input->kind == "target") {
		const string raw_target = RemoveMarker(input->value);
		const size_t separator = raw_target.rfind("::");
		if(separator != string::npos && raw_target.substr(0, separator) ==
			raw_target.substr(separator + 2)) {
			map<string, string>::const_iterator alias = substitutions.find(
				raw_target.substr(0, separator));
			if(alias != substitutions.end() && !alias->second.empty())
				result->value = alias->second + "::" + LastComponent(alias->second);
		}
	}
	if(input->kind == "decl-specifier" || input->kind == "type-name" ||
		input->kind == "type-specifier" || input->kind == "base-name") {
		const size_t marker_colon = result->value.find(':');
		string marker;
		if(marker_colon != string::npos) {
			const string prefix = result->value.substr(0, marker_colon);
			if(prefix == "TT_IDENTIFIER" || prefix.compare(0, 3, "KW_") == 0 ||
				prefix.compare(0, 3, "OP_") == 0)
				marker = result->value.substr(0, marker_colon + 1);
		}
		const string spelling = RemoveMarker(result->value);
		string qualified = QualifyTypeArgument(spelling, context);
		string resolved = ResolveAlias(qualified, context);
		if(!HasReplayContext(substitutions) && input->value.find('<') == string::npos)
			resolved = qualified;
		if(resolved.find('<') != string::npos)
			resolved = RewriteText(resolved, context, substitutions, 0, true, true,
				defer_type_only_class_definitions_ != 0);
		if(resolved.find('(') != string::npos && resolved.find(')') != string::npos)
			resolved = qualified;
		if(resolved != qualified) qualified = resolved;
		if(qualified != spelling) result->value = marker + qualified;
		if(input->kind == "decl-specifier" && marker.empty() &&
			qualified != spelling && result->value.find(':') == string::npos)
			result->value = "TT_IDENTIFIER:" + qualified;
		if(input->kind == "decl-specifier" && marker.empty() &&
			(qualified.find(' ') != string::npos || qualified.find('*') != string::npos ||
				qualified.find('&') != string::npos || qualified.find('[') != string::npos))
			result->value = "TT_IDENTIFIER:" + qualified;
	}
	string promoted_local_class;
	if(input->kind == "class-specifier" || input->kind == "class-forward-declaration") {
		const string promoted_local = PromotedLocalClass(LastComponent(input->value), context);
		if(!promoted_local.empty()) {
			promoted_local_class = LastComponent(promoted_local);
			result->value = promoted_local_class;
		}
	}
	if(input->kind == "decl-specifier" && template_replaced &&
		result->value.find(':') == string::npos)
		result->value = "TT_IDENTIFIER:" + result->value;
	*promoted_name = promoted_local_class;
	return CPPGMAstNodePtr();
}

} // namespace pa18_templates_internal
