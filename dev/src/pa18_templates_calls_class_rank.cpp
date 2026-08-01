#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"
using namespace std;
namespace pa18_templates_internal {

namespace {

bool MemberPatternHasTopLevelConst(const string& raw)
{
	int angle_depth = 0;
	for(size_t position = 0; position < raw.size();) {
		if(raw[position] == '<' && IsTemplateAngleOpen(raw, position)) {
			++angle_depth; ++position; continue;
		}
		if(raw[position] == '>' && angle_depth > 0 &&
			IsTemplateAngleClose(raw, position)) {
			--angle_depth; ++position; continue;
		}
		if(angle_depth == 0 && IsIdentifierCharacter(raw[position])) {
			const size_t begin = position++;
			while(position < raw.size() && IsIdentifierCharacter(raw[position])) ++position;
			if(raw.substr(begin, position - begin) == "const") return true;
			continue;
		}
		++position;
	}
	return false;
}

int MemberReferenceBindingPenalty(const string& pattern,
	const CPPGMAstNodePtr& argument, const string& actual)
{
	const string normalized = CanonicalSpelling(pattern);
	const bool lvalue_reference = normalized.size() >= 1 &&
		normalized[normalized.size() - 1] == '&' &&
		(normalized.size() < 2 || normalized[normalized.size() - 2] != '&');
	const bool rvalue_reference = normalized.size() >= 2 &&
		normalized.compare(normalized.size() - 2, 2, "&&") == 0;
	if(!lvalue_reference && !rvalue_reference) return 0;
	const bool lvalue = IsLvalueTemplateArgument(argument);
	const bool actual_const = MemberPatternHasTopLevelConst(CanonicalSpelling(actual));
	if(rvalue_reference) return lvalue ? 2 : 0;
	if(!lvalue) return 0;
	const bool const_reference = MemberPatternHasTopLevelConst(normalized);
	return const_reference == actual_const ? 0 : 1;
}

}

void PA18TemplateExpander::RankMemberCandidatesByClassExactness(
	vector<const TemplateDefinition*>* candidates, const CPPGMAstNodePtr& call,
	const map<string, string>& member_substitutions, const string& context)
{
	if(!candidates || !call || candidates->empty()) return;
	map<const TemplateDefinition*, int> exact_class_penalty;
	map<const TemplateDefinition*, bool> class_comparison;
	map<const TemplateDefinition*, int> reference_penalty;
	const CPPGMAstNodePtr arguments = call->children.size() > 1 && call->children[1] &&
		call->children[1]->kind == "argument-list" ? call->children[1] : CPPGMAstNodePtr();
	if(arguments) for(size_t candidate = 0; candidate < candidates->size(); ++candidate) {
		const TemplateDefinition* definition = (*candidates)[candidate];
		const CPPGMAstNodePtr parameters = DescendantOfKind(
			FunctionDeclarator(definition->declaration), "parameter-clause");
		if(!parameters) continue;
		int penalty = 0; bool compared = false; size_t argument = 0;
		for(size_t parameter = 0; parameter < parameters->children.size(); ++parameter) {
			const CPPGMAstNodePtr node = parameters->children[parameter];
			if(!node || node->kind != "parameter-declaration") continue;
			if(IsFunctionParameterPack(node) || argument >= arguments->children.size()) break;
			const string pattern = ParameterTypeSpelling(node);
			bool dependent = false;
			for(size_t item = 0; item < definition->parameters.size() && !dependent; ++item) {
				const string& name = definition->parameters[item].name;
				for(size_t at = name.empty() ? string::npos : pattern.find(name);
					at != string::npos; at = pattern.find(name, at + name.size())) {
					const bool left = at == 0 || !IsIdentifierCharacter(pattern[at - 1]);
					const size_t end = at + name.size();
					if(left && (end == pattern.size() || !IsIdentifierCharacter(pattern[end]))) {
						dependent = true; break;
					}
				}
			}
			string actual;
			bool inferred = false;
			try {
				inferred = InferArgument(arguments->children[argument], &actual,
					member_substitutions, context);
			} catch(const PA18SubstitutionFailure&) {}
			if(inferred) {
				// Member-template overloads use the same forwarding/reference
				// distinction as operators.  In particular, `U&` must beat
				// `U const&` for a non-const lvalue, while the const overload wins
				// for a const lvalue; leaving this ranking operator-only lets the
				// first source declaration capture both calls.
				reference_penalty[definition] += MemberReferenceBindingPenalty(
					pattern, arguments->children[argument], actual);
				if(!dependent) try {
					const string expected = CanonicalSpelling(ResolveAlias(ReplaceIdentifiers(
						RewriteText(pattern, context, member_substitutions, 0),
						member_substitutions), context));
					const string expected_object = FunctionArgumentObjectType(expected, context);
					const string actual_object = FunctionArgumentObjectType(actual, context);
					if(FindClassDeclaration(expected_object, context) &&
						FindClassDeclaration(actual_object, context)) {
						compared = true;
						if(expected_object != actual_object) ++penalty;
					}
				} catch(const PA18SubstitutionFailure&) {}
			}
			++argument;
		}
		exact_class_penalty[definition] = penalty;
		class_comparison[definition] = compared;
	}
	bool choice = false;
	bool reference_choice = false;
	for(size_t left = 0; left < candidates->size(); ++left) for(size_t right = left + 1;
		right < candidates->size(); ++right)
		if(reference_penalty[(*candidates)[left]] != reference_penalty[(*candidates)[right]])
			reference_choice = true;
	for(size_t left = 0; left < candidates->size(); ++left) for(size_t right = left + 1;
		right < candidates->size(); ++right)
		if(class_comparison[(*candidates)[left]] && class_comparison[(*candidates)[right]] &&
			exact_class_penalty[(*candidates)[left]] != exact_class_penalty[(*candidates)[right]]) choice = true;
	if(choice || reference_choice) stable_sort(candidates->begin(), candidates->end(),
		[&exact_class_penalty, &class_comparison, &reference_penalty](const TemplateDefinition* left,
			const TemplateDefinition* right) {
			if(reference_penalty[left] != reference_penalty[right])
				return reference_penalty[left] < reference_penalty[right];
			if(class_comparison[left] && class_comparison[right] &&
				exact_class_penalty[left] != exact_class_penalty[right])
				return exact_class_penalty[left] < exact_class_penalty[right];
			return false;
		});
}
} // namespace pa18_templates_internal
