#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

string PA18TemplateExpander::FunctionArgumentObjectType(string raw,
	const string& context) const
{
	// A dependent call result can retain the source template-id spelling while
	// a nondependent overload parameter has already been reduced through its
	// typedef alias.  Normalize concrete template-ids through the same typed
	// specialization table before comparing the object types.
	if(raw.find('<') != string::npos) try {
		raw = const_cast<PA18TemplateExpander*>(this)->RewriteText(raw, context,
		map<string, string>(), 0);
	} catch(const PA18SubstitutionFailure&) {
		// The caller will reject an actually unavailable operand below.
	}
	raw = CanonicalSpelling(ResolveAlias(raw, context));
	while(raw.compare(0, 6, "const ") == 0)
		raw = CanonicalSpelling(raw.substr(6));
	while(raw.compare(0, 9, "volatile ") == 0)
		raw = CanonicalSpelling(raw.substr(9));
	while(raw.size() > 6 && raw.compare(raw.size() - 6, 6, " const") == 0)
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 6));
	while(raw.size() > 9 && raw.compare(raw.size() - 9, 9, " volatile") == 0)
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 9));
	while(raw.size() >= 2 && raw.compare(raw.size() - 2, 2, "&&") == 0)
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 2));
	while(!raw.empty() && raw[raw.size() - 1] == '&')
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 1));
	// Removing a reference exposes the cv-qualifier on a spelling such as
	// `const T&`.  Normalize that qualifier after the reference transport has
	// been removed so class lookup compares the object type, not `T const`.
	while(raw.size() > 6 && raw.compare(raw.size() - 6, 6, " const") == 0)
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 6));
	while(raw.size() > 9 && raw.compare(raw.size() - 9, 9, " volatile") == 0)
		raw = CanonicalSpelling(raw.substr(0, raw.size() - 9));
	return raw;
}

bool PA18TemplateExpander::HasClassConversion(const string& expected,
	const string& actual, const string& context) const
{
	const CPPGMAstNodePtr declaration = FindClassDeclaration(actual, context);
	if(!declaration) return false;
	string class_name = CanonicalSpelling(actual);
	vector<string> class_arguments;
	string primary_name = class_name;
	const size_t open = class_name.find('<');
	if(open != string::npos) {
		string argument_text;
		size_t close = string::npos;
		if(!TemplateRange(class_name, open, &argument_text, &close)) return false;
		class_arguments = SplitTemplateArguments(argument_text);
		primary_name = CanonicalSpelling(class_name.substr(0, open));
	} else {
		map<string, vector<string> >::const_iterator generated_arguments =
			specialization_arguments_.find(LastComponent(class_name));
		map<string, string>::const_iterator generated_base =
			specialization_bases_.find(LastComponent(class_name));
		if(generated_arguments != specialization_arguments_.end())
			class_arguments = generated_arguments->second;
		if(generated_base != specialization_bases_.end() &&
			!generated_base->second.empty()) primary_name = generated_base->second;
	}
	const TemplateDefinition* primary = FindDefinition(primary_name, context);
	if(!primary) primary = FindDefinition(LastComponent(primary_name), context);
	map<string, string> class_substitutions;
	if(primary) for(size_t parameter = 0; parameter < primary->parameters.size() &&
		parameter < class_arguments.size(); ++parameter)
		if(!primary->parameters[parameter].name.empty())
			class_substitutions[primary->parameters[parameter].name] =
				class_arguments[parameter];
	const auto conversion_target_matches = [&](string target) {
		target = CanonicalSpelling(ReplaceIdentifiers(target, class_substitutions));
		if(target.empty()) return false;
		try {
			target = const_cast<PA18TemplateExpander*>(this)->RewriteText(
				target, context, class_substitutions, 0);
		} catch(const PA18SubstitutionFailure&) {
			return false;
		}
		return FunctionArgumentObjectType(target, context) == expected;
	};
	function<bool(const CPPGMAstNodePtr&)> visit = [&](const CPPGMAstNodePtr& node) {
		if(!node) return false;
		const string name = RemoveMarker(node->value);
		if(name.compare(0, 8, "operator") == 0 && name.size() > 8 &&
			conversion_target_matches(name.substr(8))) return true;
		for(size_t child = 0; child < node->children.size(); ++child)
			if(visit(node->children[child])) return true;
		return false;
	};
	return visit(declaration);
}

string PA18TemplateExpander::FunctionLookupContext(const string& context) const
{
	string generated_owner = active_instantiation_name_.empty() ?
		LastComponent(context) : active_instantiation_name_;
	map<string, string>::const_iterator generated_base = specialization_bases_.find(
		LastComponent(generated_owner));
	return generated_base == specialization_bases_.end() || generated_base->second.empty() ?
		context : generated_base->second;
}

bool PA18TemplateExpander::EvaluateNewExpression(const string& expression,
	const string& context, const map<string, string>& substitutions, string* result)
{
	if(!result) return false;
	size_t new_start = string::npos;
	if(expression.compare(0, 5, "::new") == 0 &&
		(expression.size() == 5 || !IsIdentifierCharacter(expression[5]))) new_start = 5;
	else if(expression.compare(0, 3, "new") == 0 &&
		(expression.size() == 3 || !IsIdentifierCharacter(expression[3]))) new_start = 3;
	if(new_start == string::npos) return false;
	string allocated = Trim(expression.substr(new_start));
	if(!allocated.empty() && allocated[0] == '(') {
		int depth = 0;
		size_t close = string::npos;
		for(size_t position = 0; position < allocated.size(); ++position) {
			if(allocated[position] == '(') ++depth;
			else if(allocated[position] == ')' && --depth == 0) {
				close = position;
				break;
			}
		}
		if(close == string::npos) return false;
		allocated = Trim(allocated.substr(close + 1));
	}
	int angle = 0;
	size_t initializer = allocated.size();
	for(size_t position = 0; position < allocated.size(); ++position) {
		const char ch = allocated[position];
		if(ch == '<' && IsTemplateAngleOpen(allocated, position)) ++angle;
		else if(ch == '>' && angle > 0 && IsTemplateAngleClose(allocated, position)) --angle;
		else if(angle == 0 && (ch == '(' || ch == '[' || ch == '{')) {
			initializer = position;
			break;
		}
	}
	allocated = Trim(allocated.substr(0, initializer));
	allocated = ResolveDecltypeTypeName(RewriteText(allocated, context, substitutions, 0),
		context, substitutions);
	if(!IsKnownTypeSpelling(allocated, context)) return false;
	string object_type = allocated;
	while(object_type.compare(0, 6, "const ") == 0)
		object_type = NormalizeTypeArgument(object_type.substr(6));
	while(object_type.compare(0, 9, "volatile ") == 0)
		object_type = NormalizeTypeArgument(object_type.substr(9));
	if(object_type == "void") return false;
	*result = NormalizeTypeArgument(allocated + "*");
	return true;
}

} // namespace pa18_templates_internal
