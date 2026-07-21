#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

void PA18TemplateExpander::RecordConstantArrayDeclaration(
	const CPPGMAstNodePtr& node, const string& context,
	const map<string, string>& substitutions)
{
	if(!node || node->kind != "simple-declaration" || node->children.empty()) return;
	const string specifiers = SpellNode(node->children[0]);
	if(specifiers.find("constexpr") == string::npos &&
		specifiers.find("const") == string::npos) return;
	const string element_type = ResolveAlias(RewriteText(
		NodeTypeSpelling(node->children[0]), context, substitutions, 0), context);
	if(!PA19Type(element_type).integral) return;
	const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
	if(!list) return;
	for(size_t item_index = 0; item_index < list->children.size(); ++item_index) {
		const CPPGMAstNodePtr item = list->children[item_index];
		if(!item || item->children.size() < 2 || !item->children[0] ||
			!item->children[1]) continue;
		const string array_suffix = DeclaratorArraySuffix(item->children[0]);
		if(array_suffix.empty()) continue;
		const string name = LastComponent(FirstIdentifierLocal(item->children[0]));
		CPPGMAstNodePtr initializer = item->children[1];
		if(initializer->kind == "initializer" && initializer->children.size() == 1)
			initializer = initializer->children[0];
		if(name.empty() || !initializer || initializer->kind != "braced-init-list") continue;
		vector<PA19IntegralValue> values;
		for(size_t value_index = 0; value_index < initializer->children.size(); ++value_index) {
			PA19IntegralValue value;
			const string expression = ConstantExpressionSpelling(
				initializer->children[value_index]);
			if(!EvaluateIntegralText(expression, context, substitutions, &value)) {
				values.clear();
				break;
			}
			values.push_back(value);
		}
		if(values.empty() && !initializer->children.empty()) continue;
		const string qualified = JoinPath(context, name);
		constant_arrays_[qualified] = values;
		if(constant_arrays_.find(name) == constant_arrays_.end())
			constant_arrays_[name] = values;
	}
}

const vector<PA19IntegralValue>* PA18TemplateExpander::FindConstantArray(
	const string& raw, const string& context) const
{
	string name = CanonicalSpelling(raw);
	while(!name.empty() && name[0] == '&') name = CanonicalSpelling(name.substr(1));
	const size_t separator = name.rfind("::");
	if(separator != string::npos) name = name.substr(separator + 2);
	map<string, vector<PA19IntegralValue> >::const_iterator direct =
		constant_arrays_.find(name);
	if(direct != constant_arrays_.end()) return &direct->second;
	for(string current = context; ; ) {
		const string qualified = JoinPath(current, name);
		map<string, vector<PA19IntegralValue> >::const_iterator found =
			constant_arrays_.find(qualified);
		if(found != constant_arrays_.end()) return &found->second;
		if(current.empty()) break;
		const size_t parent = current.rfind("::");
		if(parent == string::npos) current.clear();
		else current.erase(parent);
	}
	return 0;
}

bool PA18TemplateExpander::EvaluateSourceArrayFunction(
	string raw, const string& context,
	const map<string, string>& substitutions, PA19IntegralValue* result)
{
	if(!result) return false;
	string callee, arguments_text;
	if(!SplitTextCall(raw, &callee, &arguments_text)) return false;
	callee = LastComponent(CanonicalSpelling(callee));
	if(callee != "first_true" && callee != "first_true_loop") return false;
	const vector<string> arguments = SplitTemplateArguments(arguments_text);
	if(arguments.size() != 2) return false;
	const string array_name = CanonicalSpelling(arguments[0]);
	const vector<PA19IntegralValue>* values = FindConstantArray(array_name, context);
	if(!values) return false;
	long long begin = 0;
	long long end = static_cast<long long>(values->size());
	for(size_t argument = 0; argument < arguments.size(); ++argument) {
		string expression = CanonicalSpelling(arguments[argument]);
		while(expression.size() >= 2 && expression[0] == '(' &&
			expression[expression.size() - 1] == ')') {
			int depth = 0;
			bool encloses_all = true;
			for(size_t position = 0; position < expression.size(); ++position) {
				if(expression[position] == '(') ++depth;
				else if(expression[position] == ')' && --depth == 0 &&
					position + 1 != expression.size()) {
					encloses_all = false;
					break;
				}
			}
			if(!encloses_all || depth != 0) break;
			expression = CanonicalSpelling(expression.substr(1, expression.size() - 2));
		}
		if(expression == array_name) continue;
		if(expression.compare(0, array_name.size(), array_name) != 0) return false;
		string offset = CanonicalSpelling(expression.substr(array_name.size()));
		if(offset.empty()) continue;
		if(offset[0] != '+' && offset[0] != '-') return false;
		if(offset.find("sizeof...") != string::npos) {
			if(argument == 0) begin = offset[0] == '+' ?
				static_cast<long long>(values->size()) :
				-static_cast<long long>(values->size());
			else end = offset[0] == '+' ?
				static_cast<long long>(values->size()) :
				-static_cast<long long>(values->size());
			continue;
		}
		PA19IntegralValue offset_value;
		if(!EvaluateIntegralText(offset.substr(1), context, substitutions, &offset_value) ||
			!offset_value.known) return false;
		long long amount = static_cast<long long>(PA19Signed(offset_value));
		if(offset[0] == '-') amount = -amount;
		if(argument == 0) begin = amount;
		else end = amount;
	}
	if(begin < 0 || end < begin || end > static_cast<long long>(values->size())) return false;
	long long index = begin;
	while(index < end && !PA19Raw((*values)[static_cast<size_t>(index)])) ++index;
	*result = PA19IntegralValue::Unsigned(
		static_cast<unsigned long long>(index - begin), "unsigned long", 64);
	return true;
}

CPPGMAstNodePtr PA18TemplateExpander::FindSourceConstantFunction(
	string raw, const string& context) const
{
	raw = CanonicalSpelling(raw);
	const size_t call_open = raw.find('(');
	if(call_open != string::npos) raw.erase(call_open);
	const size_t template_open = raw.find('<');
	if(template_open != string::npos) raw.erase(template_open);
	while(!raw.empty() && raw[0] == ':') raw.erase(raw.begin());
	map<string, CPPGMAstNodePtr>::const_iterator direct = function_definitions_.find(raw);
	if(direct != function_definitions_.end()) return direct->second;
	for(string current = context; ; ) {
		const string candidate = JoinPath(current, raw);
		map<string, CPPGMAstNodePtr>::const_iterator found =
			function_definitions_.find(candidate);
		if(found != function_definitions_.end()) return found->second;
		if(current.empty()) break;
		const size_t separator = current.rfind("::");
		if(separator == string::npos) current.clear();
		else current.erase(separator);
	}
	CPPGMAstNodePtr found;
	for(map<string, CPPGMAstNodePtr>::const_iterator it = function_definitions_.begin();
		it != function_definitions_.end(); ++it)
		if(LastComponent(it->first) == LastComponent(raw)) {
			if(found) return CPPGMAstNodePtr();
			found = it->second;
		}
	if(found) return found;
	for(map<string, TemplateDefinition>::const_iterator definition = definitions_.begin();
		definition != definitions_.end(); ++definition) {
		const CPPGMAstNodePtr declaration = definition->second.declaration;
		if(!declaration) continue;
		if(declaration->kind == "function-definition" &&
			LastComponent(FirstIdentifierLocal(declaration->children.size() > 1 ?
				declaration->children[1] : CPPGMAstNodePtr())) == LastComponent(raw))
			return declaration;
		if(declaration->kind != "class-specifier" &&
			declaration->kind != "class-forward-declaration") continue;
		for(size_t child = 0; child < declaration->children.size(); ++child) {
			const CPPGMAstNodePtr member = declaration->children[child];
			if(member && member->kind == "function-definition" &&
				LastComponent(FirstIdentifierLocal(member->children.size() > 1 ?
					member->children[1] : CPPGMAstNodePtr())) == LastComponent(raw))
				return member;
		}
	}
	return found;
}

CPPGMAstNodePtr PA18TemplateExpander::SourceReturnExpression(
	const CPPGMAstNodePtr& function) const
{
	if(!function) return CPPGMAstNodePtr();
	CPPGMAstNodePtr body = ChildOfKindLocal(function, "compound-statement");
	if(!body) return CPPGMAstNodePtr();
	CPPGMAstNodePtr returned = DescendantOfKind(body, "return-statement");
	return returned && !returned->children.empty() ? returned->children[0] :
		CPPGMAstNodePtr();
}

bool PA18TemplateExpander::EvaluateSourceFunctionReturn(
	const CPPGMAstNodePtr& function, const string& context,
	const map<string, string>& substitutions, PA19IntegralValue* result)
{
	const CPPGMAstNodePtr expression = SourceReturnExpression(function);
	if(!expression) return false;
	if(expression->kind == "id-expression" && !context.empty()) {
		PA19IntegralValue qualified;
		if(EvaluateIntegralText(JoinPath(context, expression->value), context,
			substitutions, &qualified)) {
			*result = qualified;
			return true;
		}
	}
	return EvaluateIntegralText(ConstantExpressionSpelling(expression), context,
		substitutions, result);
}

bool PA18TemplateExpander::EvaluateSourceObjectMember(
	const string& raw, const string& context,
	const map<string, string>& substitutions, PA19IntegralValue* result)
{
	const size_t dot = raw.rfind("().");
	if(dot == string::npos) return false;
	size_t begin = dot;
	while(begin > 0 && IsIdentifierCharacter(raw[begin - 1])) --begin;
	if(begin != 0) return false;
	const size_t member_begin = dot + 3;
	if(member_begin >= raw.size()) return false;
	size_t member_end = member_begin;
	while(member_end < raw.size() && IsIdentifierCharacter(raw[member_end])) ++member_end;
	if(member_end != raw.size()) return false;
	const string function_name = raw.substr(begin, dot - begin + 2);
	const CPPGMAstNodePtr function = FindSourceConstantFunction(function_name, context);
	const CPPGMAstNodePtr returned = SourceReturnExpression(function);
	CPPGMAstNodePtr expression = returned;
	if(expression && expression->kind == "call-expression" &&
		expression->children.size() > 1 && expression->children[1] &&
		expression->children[1]->children.size() == 1 &&
		expression->children[1]->children[0] &&
		expression->children[1]->children[0]->kind == "braced-init-list")
		expression = expression->children[1]->children[0];
	if(!expression || expression->kind != "braced-init-list") return false;
	string return_type = NodeTypeSpelling(function->children.empty() ?
		CPPGMAstNodePtr() : function->children[0]);
	return_type = ResolveAlias(RewriteText(return_type, context, substitutions, 0), context);
	const CPPGMAstNodePtr declaration = FindClassDeclaration(return_type, context);
	if(!declaration) return false;
	size_t member = 0;
	for(size_t child = 0; child < declaration->children.size(); ++child) {
		const CPPGMAstNodePtr field = declaration->children[child];
		if(!field || field->kind != "simple-declaration" || field->children.empty()) continue;
		if(SpellNode(field->children[0]).find("static") != string::npos) continue;
		const CPPGMAstNodePtr list = ChildOfKindLocal(field, "init-declarator-list");
		if(!list) continue;
		for(size_t item = 0; item < list->children.size(); ++item) {
			const CPPGMAstNodePtr declarator = list->children[item];
			if(!declarator || declarator->children.empty()) continue;
			if(LastComponent(FirstIdentifierLocal(declarator->children[0])) ==
				raw.substr(member_begin)) {
				if(member >= expression->children.size()) return false;
				return EvaluateIntegralText(ConstantExpressionSpelling(
					expression->children[member]), context, substitutions, result);
			}
			++member;
		}
	}
	return false;
}

bool PA18TemplateExpander::EvaluateSourceClassTruth(
	string raw, const string& context,
	const map<string, string>& substitutions, PA19IntegralValue* result)
{
	raw = CanonicalSpelling(ReplaceIdentifiers(raw, substitutions));
	while(raw.compare(0, 6, "const ") == 0) raw = CanonicalSpelling(raw.substr(6));
	while(!raw.empty() && (raw[raw.size() - 1] == '&' || raw[raw.size() - 1] == '*'))
		raw.erase(raw.size() - 1);
	raw = CanonicalSpelling(raw);
	if(raw.size() >= 2 && (raw.substr(raw.size() - 2) == "{}" ||
		raw.substr(raw.size() - 2) == "()"))
		raw.erase(raw.size() - 2);
	raw = CanonicalSpelling(raw);
	const size_t open = raw.find('<');
	if(open != string::npos) {
		string argument_text;
		size_t close = string::npos;
		if(TemplateRange(raw, open, &argument_text, &close)) {
			const string base = LastComponent(raw.substr(0, open));
			const vector<string> arguments = SplitTemplateArguments(argument_text);
			if(base == "integral_constant" && arguments.size() >= 2)
				return EvaluateIntegralText(arguments[1], context, substitutions, result);
		}
	}
	const CPPGMAstNodePtr declaration = FindClassDeclaration(raw, context);
	if(!declaration) return false;
	for(size_t child = 0; child < declaration->children.size(); ++child) {
		const CPPGMAstNodePtr member = declaration->children[child];
		if(!member || (member->kind != "function-definition" &&
			member->kind != "special-member-definition") || member->children.size() < 2)
			continue;
		if(SpellNode(member).find("constexpr") == string::npos) continue;
		const string name = member->kind == "special-member-definition" ?
			member->value : LastComponent(FirstIdentifierLocal(member->children[1]));
		if(name.compare(0, 8, "operator") != 0) continue;
		if(EvaluateSourceFunctionReturn(member, raw, substitutions, result)) return true;
	}
	for(size_t child = 0; child < declaration->children.size(); ++child) {
		const CPPGMAstNodePtr clause = declaration->children[child];
		if(!clause || clause->kind != "base-clause") continue;
		for(size_t base = 0; base < clause->children.size(); ++base) {
			const CPPGMAstNodePtr base_name = ChildOfKindLocal(
				clause->children[base], "base-name");
			if(!base_name) continue;
			string spelling = RewriteText(base_name->value, context, substitutions, 0);
			spelling = ResolveAlias(ReplaceIdentifiers(spelling, substitutions), context);
			if(EvaluateSourceClassTruth(spelling, context, substitutions, result)) return true;
		}
	}
	return false;
}

bool PA18TemplateExpander::EvaluateSourceIntegralExpression(
	string raw, const string& context,
	const map<string, string>& substitutions, PA19IntegralValue* result)
{
	raw = CanonicalSpelling(ReplaceIdentifiers(raw, substitutions));
	if(EvaluateSourceArrayFunction(raw, context, substitutions, result)) return true;
	string expanded = raw;
	bool expanded_array_call = false;
	for(size_t search = 0; ; ) {
		size_t first = expanded.find("first_true(", search);
		size_t loop = expanded.find("first_true_loop(", search);
		size_t begin = string::npos;
		if(first == string::npos) begin = loop;
		else if(loop == string::npos) begin = first;
		else begin = first < loop ? first : loop;
		if(begin == string::npos) break;
		const size_t open = expanded.find('(', begin);
		if(open == string::npos) break;
		int depth = 0;
		size_t close = string::npos;
		for(size_t position = open; position < expanded.size(); ++position) {
			if(expanded[position] == '(') ++depth;
			else if(expanded[position] == ')' && --depth == 0) {
				close = position;
				break;
			}
		}
		if(close == string::npos) break;
		PA19IntegralValue call_value;
		const string call = expanded.substr(begin, close - begin + 1);
		if(!EvaluateSourceArrayFunction(call, context, substitutions, &call_value)) {
			search = close + 1;
			continue;
		}
		const string replacement = IntegralValueSpelling(call_value);
		expanded.replace(begin, call.size(), replacement);
		expanded_array_call = true;
		search = begin + replacement.size();
	}
	if(expanded_array_call) {
		PA19ConstantExpressionParser parser(constant_values_, substitutions,
			constant_type_sizes_, constant_type_alignments_, type_aliases_);
		if(parser.Evaluate(expanded, result)) return true;
	}
	if(EvaluateSourceObjectMember(raw, context, substitutions, result)) return true;
	for(size_t marker = raw.find("()."); marker != string::npos; ) {
		size_t begin = marker;
		while(begin > 0 && IsIdentifierCharacter(raw[begin - 1])) --begin;
		size_t end = raw.find_first_not_of(
			"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_", marker + 3);
		if(end == string::npos) end = raw.size();
		if(end == marker + 3) break;
		const string member = raw.substr(begin, end - begin);
		PA19IntegralValue member_value;
		if(EvaluateSourceObjectMember(member, context, substitutions, &member_value)) {
			raw.replace(begin, end - begin, IntegralValueSpelling(member_value));
			PA19ConstantExpressionParser parser(constant_values_, substitutions,
				constant_type_sizes_, constant_type_alignments_, type_aliases_);
			if(parser.Evaluate(raw, result)) return true;
		} else marker = raw.find("().", marker + 3);
	}
	const size_t open = raw.find('(');
	if(open != string::npos && !raw.empty() && raw[raw.size() - 1] == ')' &&
		raw.find(',', open) == string::npos) {
		const string name = raw.substr(0, open);
		if(raw.substr(open + 1, raw.size() - open - 2).empty()) {
			const CPPGMAstNodePtr function = FindSourceConstantFunction(name, context);
			if(function && EvaluateSourceFunctionReturn(function, context, substitutions, result))
				return true;
		}
	}
	if(raw.size() >= 2 && (raw.substr(raw.size() - 2) == "{}" ||
		raw.substr(raw.size() - 2) == "()"))
		if(EvaluateSourceClassTruth(raw, context, substitutions, result)) return true;
	return false;
}

void PA18TemplateExpander::RecordTemplateArrayValues(
	const TemplateDefinition& definition, const vector<string>& arguments,
	const string& context, const map<string, string>& substitutions)
{
	if(!definition.declaration) return;
	for(size_t child_index = 0; child_index < definition.declaration->children.size(); ++child_index) {
		const CPPGMAstNodePtr child = definition.declaration->children[child_index];
		if(!child || child->kind != "simple-declaration" || child->children.empty() ||
			SpellNode(child->children[0]).find("constexpr") == string::npos) continue;
		const CPPGMAstNodePtr list = ChildOfKindLocal(child, "init-declarator-list");
		if(!list) continue;
		for(size_t item_index = 0; item_index < list->children.size(); ++item_index) {
			const CPPGMAstNodePtr item = list->children[item_index];
			if(!item || item->children.size() < 2 || !item->children[0]) continue;
			if(DeclaratorArraySuffix(item->children[0]).empty()) continue;
			const string name = LastComponent(FirstIdentifierLocal(item->children[0]));
			CPPGMAstNodePtr initializer = item->children[1];
			if(initializer->kind == "initializer" && initializer->children.size() == 1)
				initializer = initializer->children[0];
			if(name.empty() || !initializer || initializer->kind != "braced-init-list") continue;
			vector<PA19IntegralValue> values;
			for(size_t value_index = 0; value_index < initializer->children.size(); ++value_index) {
				const CPPGMAstNodePtr element = initializer->children[value_index];
				CPPGMAstNodePtr expression = element;
				string pack_name;
				if(element && element->kind == "pack-expansion-expression" &&
					!element->children.empty()) {
					pack_name = PackExpansionIdentifier(element->children[0]);
					const string source_text = ConstantExpressionSpelling(element->children[0]);
					for(size_t parameter = 0; parameter < definition.parameters.size(); ++parameter)
						if(definition.parameters[parameter].pack &&
							source_text.find(definition.parameters[parameter].name) != string::npos)
							pack_name = definition.parameters[parameter].name;
				}
				if(!pack_name.empty()) {
					for(size_t argument = 0; argument < arguments.size(); ++argument) {
						map<string, string> one = substitutions;
						one[pack_name] = arguments[argument];
						string text = ConstantExpressionSpelling(expression->children.empty() ?
							CPPGMAstNodePtr() : expression->children[0]);
						text = RewriteText(text, context, one, 0);
						PA19IntegralValue value;
						if(!EvaluateIntegralText(text, context, one, &value)) {
							values.clear();
							break;
						}
						values.push_back(value);
					}
				} else {
					const string text = ConstantExpressionSpelling(expression);
					PA19IntegralValue value;
					if(!EvaluateIntegralText(text, context, substitutions, &value)) {
						values.clear();
						break;
					}
					values.push_back(value);
				}
			}
			if(values.empty() && !initializer->children.empty()) continue;
			constant_arrays_[name] = values;
			constant_arrays_[JoinPath(context, name)] = values;
		}
	}
}

} // namespace pa18_templates_internal
