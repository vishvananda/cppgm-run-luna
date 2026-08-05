#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::InferCallIdentifierArgument(
	const CPPGMAstNodePtr& expression, string* result,
	const map<string, string>& substitutions, const string& context) const
{
	if(!expression || expression->children.empty() || !expression->children[0] ||
		expression->children[0]->kind != "id-expression") return false;
	// A callable data member has a typed operator() result; do not let an
	// unknown operand trigger unrelated free-function template deduction.
	string callable_object_type;
	if(LookupVariableType(expression->children[0]->value, context,
		&callable_object_type)) {
		callable_object_type = NormalizeTypeArgument(ResolveAlias(
			ReplaceIdentifiers(callable_object_type, substitutions), context));
		string callable_result;
		set<string> callable_active;
		if(FindClassMemberType(callable_object_type, "operator()", substitutions,
			context, &callable_result, &callable_active) && !callable_result.empty()) {
			*result = callable_result;
			return true;
		}
		if(InferCallableObjectCall(expression, callable_object_type, substitutions,
			context, &callable_result) && !callable_result.empty()) {
			*result = callable_result;
			return true;
		}
	}
	if(!expression->template_primary.empty() && !expression->template_arguments.empty()) {
		const vector<const TemplateDefinition*> materialized =
			FindFunctionDefinitions(expression->template_primary, context);
		for(size_t candidate = 0; candidate < materialized.size(); ++candidate) {
			const TemplateDefinition* definition = materialized[candidate];
			if(!definition || definition->parameters.size() !=
				expression->template_arguments.size() || !definition->declaration ||
				definition->declaration->children.empty()) continue;
			map<string, string> function_substitutions;
			for(size_t parameter = 0; parameter < definition->parameters.size(); ++parameter)
				if(!definition->parameters[parameter].name.empty())
					function_substitutions[definition->parameters[parameter].name] =
						expression->template_arguments[parameter];
				const CPPGMAstNodePtr declarator = FunctionDeclarator(definition->declaration);
				string return_type = NodeTypeSpelling(definition->declaration->children[0]) +
					DeclaratorSuffix(declarator);
				PA18TemplateExpander* replay = const_cast<PA18TemplateExpander*>(this);
				const map<string, vector<string> > previous_packs =
					replay->active_pack_substitutions_;
				size_t argument_index = 0;
				for(size_t parameter = 0; parameter < definition->parameters.size(); ++parameter) {
					const TemplateParameter& template_parameter = definition->parameters[parameter];
					if(template_parameter.pack) {
						size_t trailing_fixed = 0;
						for(size_t later = parameter + 1; later < definition->parameters.size(); ++later)
							if(!definition->parameters[later].pack) ++trailing_fixed;
						const size_t available = expression->template_arguments.size() > argument_index ?
							expression->template_arguments.size() - argument_index : 0;
						const size_t count = available > trailing_fixed ? available - trailing_fixed : 0;
						vector<string> values;
						for(size_t element = 0; element < count; ++element)
							values.push_back(expression->template_arguments[argument_index++]);
						if(!template_parameter.name.empty())
							replay->active_pack_substitutions_[template_parameter.name] = values;
					} else if(argument_index < expression->template_arguments.size()) ++argument_index;
				}
				string resolved_return;
				const string return_context = definition->owner.empty() ?
					context : definition->owner;
				try {
					// Keep pack markers intact until the typed pack binding above has
					// expanded them; scalar replacement would turn `T...` into the
					// invalid spelling `int...`.
					resolved_return = ReplaceIdentifiersPreservingPackSizes(return_type,
						function_substitutions);
					resolved_return = replay->RewriteText(resolved_return, return_context,
						function_substitutions, 0);
				} catch(...) {
					replay->active_pack_substitutions_ = previous_packs;
					throw;
				}
				replay->active_pack_substitutions_ = previous_packs;
				*result = CanonicalSpelling(ResolveAlias(resolved_return, return_context));
			if(!result->empty()) return true;
		}
	}
	// Before the enclosing explicit call is transformed, a nested explicit
	// template-id still has its source spelling (`declval<int>`) and therefore
	// has no materialized `template_primary` fact yet.  Resolve that call through
	// the same typed candidate-selection and completed-argument path used by
	// normal call transformation; the enclosing deduction probe then sees its
	// real trailing return type rather than a first-match spelling guess.
	string explicit_result;
	PA18TemplateExpander* replay = const_cast<PA18TemplateExpander*>(this);
	if(replay->ResolveExplicitTemplateCallResult(expression,
		expression->children[0], context, substitutions, &explicit_result)) {
		*result = explicit_result;
		return true;
	}
	const string member = LastComponent(expression->children[0]->value);
	string owner;
	for(string current = context; ; ) {
		const TemplateDefinition* current_definition = FindDefinition(current, context);
		if(class_contexts_.find(current) != class_contexts_.end() ||
			class_declarations_.find(current) != class_declarations_.end() ||
			(current_definition && current_definition->class_template)) {
			owner = current;
			break;
		}
		if(current.empty()) break;
		const size_t separator = current.rfind("::");
		if(separator == string::npos) current.clear();
		else current.erase(separator);
	}
	set<string> active;
	string member_type;
	if(!owner.empty() && !member.empty() && FindClassMemberType(
		owner, member, substitutions, context, &member_type, &active)) {
		string callable_result;
		// A callable data member is itself resolved through ordinary class-member
		// lookup.  This is important for dependent wrapper members such as
		// `PrevType const& prev_`: the member lookup above recovers the concrete
		// object type, after which its non-special `operator()` must determine the
		// call expression's result type before surrounding operator overload
		// deduction runs.
		set<string> callable_active;
		if(FindClassMemberType(member_type, "operator()", substitutions, context,
			&callable_result, &callable_active) && !callable_result.empty()) {
			*result = callable_result;
			return true;
		}
		if(InferCallableObjectCall(expression, member_type, substitutions,
			context, &callable_result)) {
			*result = callable_result;
			return true;
		}
		*result = member_type;
		return true;
	}
	const string callee = LastComponent(expression->children[0]->value);
	string nested_class;
	const string qualified_callee = expression->children[0]->value;
	if(class_contexts_.find(qualified_callee) != class_contexts_.end() ||
		class_declarations_.find(qualified_callee) != class_declarations_.end())
		nested_class = qualified_callee;
	for(string current = context; nested_class.empty(); ) {
		const string candidate = JoinPath(current, callee);
		if(class_contexts_.find(candidate) != class_contexts_.end() ||
			class_declarations_.find(candidate) != class_declarations_.end()) {
			nested_class = candidate;
			break;
		}
		if(current.empty()) break;
		const size_t separator = current.rfind("::");
		if(separator == string::npos) current.clear();
		else current.erase(separator);
	}
	if(!nested_class.empty()) {
		*result = nested_class;
		return true;
	}
	const FunctionSignature* signature = FindFunctionSignature(
		expression->children[0]->value, context);
	if(signature && signature->result_specifiers) {
		const string return_type = NodeTypeSpelling(signature->result_specifiers) +
			ReturnDeclaratorSuffix(signature->declarator);
		*result = CanonicalSpelling(ResolveAlias(ReplaceIdentifiers(
			return_type, substitutions), context));
		if(!result->empty()) return true;
	}
	const string fallback = ResolveAlias(expression->children[0]->value, context);
	if(!IsKnownTypeSpelling(fallback, context)) return false;
	*result = fallback;
	return true;
}


} // namespace pa18_templates_internal
