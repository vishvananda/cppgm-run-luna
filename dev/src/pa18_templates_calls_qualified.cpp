#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::HasViableOrdinaryCallableMember(
	const CPPGMAstNodePtr& call, const string& object_type, const string& member_name,
	const string& context, const map<string, string>& substitutions)
{
	const CPPGMAstNodePtr ordinary_class = FindClassDeclaration(object_type, context);
	if(!ordinary_class || call->children.size() <= 1 || !call->children[1]) return false;
	const CPPGMAstNodePtr arguments = call->children[1]->kind == "argument-list" ?
		call->children[1] : ChildOfKindLocal(call->children[1], "argument-list");
	if(!arguments) return false;
	for(size_t member = 0; member < ordinary_class->children.size(); ++member) {
		const CPPGMAstNodePtr declaration = ordinary_class->children[member];
		if(!declaration || (declaration->kind != "function-definition" &&
			declaration->kind != "simple-declaration") ||
			LastComponent(DeclarationName(declaration)) != member_name) continue;
		const CPPGMAstNodePtr parameters = DescendantOfKind(
			FunctionDeclarator(declaration), "parameter-clause");
		if(!parameters || parameters->children.size() != arguments->children.size()) continue;
		bool callable_argument = false, viable = true;
		for(size_t argument = 0; argument < arguments->children.size(); ++argument) {
			const CPPGMAstNodePtr parameter = parameters->children[argument];
			if(!parameter || parameter->kind != "parameter-declaration") { viable = false; break; }
			string actual;
			FunctionSignature actual_signature;
			if(!InferArgument(arguments->children[argument], &actual, substitutions,
				context, &actual_signature)) { viable = false; break; }
			if(actual_signature.result_specifiers && actual_signature.parameters)
				callable_argument = true;
			map<string, string> ignored;
			if(!MatchTypePattern(FunctionTypeSpelling(parameter), actual,
				set<string>(), &ignored, context)) { viable = false; break; }
		}
		if(viable && callable_argument) return true;
	}
	return false;
}

bool PA18TemplateExpander::TransformQualifiedMemberTemplateCall(
	const CPPGMAstNodePtr& input, const CPPGMAstNodePtr& input_callee,
	const string& context, const map<string, string>& substitutions,
	const CPPGMAstNodePtr& result)
{
	if(!input || !input_callee || input_callee->kind != "id-expression" || !result)
		return false;
	const string raw = RemoveMarker(input_callee->value);
	const size_t outer_open = raw.find('<');
	if(outer_open == string::npos) return false;
	size_t outer_begin = 0, outer_close = string::npos;
	string outer_base, outer_arguments;
	const bool outer_ok = TemplateBase(raw, outer_open, &outer_begin, &outer_base);
	const bool outer_range = outer_ok && TemplateRange(raw, outer_open, &outer_arguments, &outer_close);
	if(!outer_ok || !outer_range || outer_close + 2 >= raw.size() ||
		raw.compare(outer_close + 1, 2, "::") != 0)
		return false;
	const size_t member_start = outer_close + 3;
	const size_t member_open = raw.find('<', member_start);
	if(member_open == string::npos) return false;
	size_t member_begin = 0, member_close = string::npos;
	string member_base, member_arguments;
	const bool member_base_ok = TemplateBase(raw, member_open, &member_begin, &member_base);
	const bool member_range_ok = member_base_ok && TemplateRange(raw, member_open,
		&member_arguments, &member_close);
	if(!member_base_ok || !member_range_ok || member_base.empty()) return false;
	member_base = LastComponent(member_base);
	member_begin = member_start;

	const string owner_source = raw.substr(outer_begin,
		outer_close - outer_begin + 1);
	string owner = CanonicalSpelling(RewriteText(owner_source, context,
		substitutions, 0));
	owner = CanonicalSpelling(ResolveAlias(owner, context));
	owner = CanonicalSpelling(QualifyTypeArgument(owner, context));
	if(owner.empty()) return false;
	map<string, string>::const_iterator owner_base = specialization_bases_.find(
		LastComponent(owner));
	map<string, vector<string> >::const_iterator owner_arguments =
		specialization_arguments_.find(LastComponent(owner));
	if(owner_base == specialization_bases_.end() ||
		owner_arguments == specialization_arguments_.end()) return false;

	// A qualified member-template-id needs the concrete class specialization as
	// its lookup receiver.  Transform only the call arguments before asking the
	// ordinary member candidate path to materialize the body.
	for(size_t child = 1; child < input->children.size(); ++child) {
		CPPGMAstNodePtr transformed = TransformNode(input->children[child],
			context, substitutions);
		if(transformed) result->children.push_back(transformed);
	}
	CPPGMAstNodePtr object(new CPPGMAstNode("id-expression"));
	object->inferred_type = owner;
	CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "."));
	member->children.push_back(object);
	member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier",
		raw.substr(member_begin, member_close - member_begin + 1))));
	result->children.insert(result->children.begin(), member);
	const ConcreteOwnerContext previous_owner = active_concrete_owner_;
	SetActiveConcreteOwner(owner, context);
	bool instantiated = false;
	try {
		instantiated = InstantiateMemberCall(result, member,
			raw.substr(member_begin, member_close - member_begin + 1),
			context, substitutions);
	} catch(...) {
		active_concrete_owner_ = previous_owner;
		throw;
	}
	active_concrete_owner_ = previous_owner;
	if(!instantiated) return false;
	result->template_instantiation = true;
	result->children[0] = CPPGMAstNodePtr(new CPPGMAstNode("id-expression",
		owner + "::" + LastComponent(member_base)));
	return true;
}

} // namespace pa18_templates_internal
