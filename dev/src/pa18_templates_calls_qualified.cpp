#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

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
