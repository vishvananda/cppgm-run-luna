#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

#include <functional>

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
	const CPPGMAstNodePtr function = FindSourceConstantFunction(callee, context);
	if(!function || function->children.size() < 3 ||
		SpellNode(function->children[0]).find("constexpr") == string::npos)
		return false;

	struct SourceValue {
		PA19IntegralValue integral;
		bool pointer;
		string array;
		long long index;

		SourceValue() : integral(), pointer(false), array(), index(0) {}
	};
	struct SourceFlow {
		enum Kind { NORMAL, RETURN, BREAK, CONTINUE };
		Kind kind;
		SourceValue value;

		SourceFlow(Kind flow = NORMAL, const SourceValue& result = SourceValue())
			: kind(flow), value(result) {}
	};
	typedef map<string, SourceValue> Frame;
	vector<Frame> frames;
	map<const CPPGMAstNode*, unsigned> depths;

	const auto known = [](const SourceValue& value) {
		return value.integral.known || value.pointer;
	};
	const auto truth = [&](const SourceValue& value) {
		if(value.integral.known) return PA19Raw(value.integral) != 0;
		return value.pointer;
	};
	const auto array_key = [](string name) {
		name = CanonicalSpelling(name);
		while(!name.empty() && name[0] == '&') name = CanonicalSpelling(name.substr(1));
		const size_t separator = name.rfind("::");
		return separator == string::npos ? name : name.substr(separator + 2);
	};

	std::function<SourceValue(const string&)> source_text_value;
	std::function<SourceValue(const CPPGMAstNodePtr&)> evaluate;
	std::function<SourceFlow(const CPPGMAstNodePtr&)> evaluate_statement;
	std::function<SourceValue(const CPPGMAstNodePtr&, const vector<SourceValue>&)> evaluate_function;
	const auto operator_from_node = [](const string& value) {
		const size_t separator = value.find(':');
		return separator == string::npos ? value : value.substr(separator + 1);
	};
	const auto assignment_operator = [](const string& operation) {
		if(operation == "+=") return string("+");
		if(operation == "-=") return string("-");
		if(operation == "*=") return string("*");
		if(operation == "/=") return string("/");
		if(operation == "%=") return string("%");
		if(operation == "<<=") return string("<<");
		if(operation == ">>=") return string(">>");
		if(operation == "&=") return string("&");
		if(operation == "|=") return string("|");
		if(operation == "^=") return string("^");
		return string();
	};

	const auto lookup = [&](const string& raw_name) {
		SourceValue value;
		const string name = CanonicalSpelling(raw_name);
		for(vector<Frame>::reverse_iterator frame = frames.rbegin();
			frame != frames.rend(); ++frame) {
			map<string, SourceValue>::const_iterator found = frame->find(name);
			if(found != frame->end()) return found->second;
		}
		if(FindConstantArray(name, context)) {
			value.pointer = true;
			value.array = array_key(name);
			return value;
		}
		PA19IntegralValue integral;
		if(EvaluateIntegralText(name, context, substitutions, &integral))
			value.integral = integral;
		return value;
	};
	const auto set_value = [&](const string& raw_name, const SourceValue& value) {
		const string name = CanonicalSpelling(raw_name);
		for(vector<Frame>::reverse_iterator frame = frames.rbegin();
			frame != frames.rend(); ++frame) {
			if(frame->find(name) == frame->end()) continue;
			(*frame)[name] = value;
			return;
		}
		if(!frames.empty()) frames.back()[name] = value;
	};
	const auto pointer_element = [&](const SourceValue& pointer) {
		SourceValue value;
		if(!pointer.pointer) return value;
		const vector<PA19IntegralValue>* array = FindConstantArray(pointer.array, context);
		if(!array || pointer.index < 0 ||
			static_cast<size_t>(pointer.index) >= array->size()) return value;
		value.integral = (*array)[static_cast<size_t>(pointer.index)];
		return value;
	};

	source_text_value = [&](const string& raw_text) {
		SourceValue value;
		string text = CanonicalSpelling(ReplaceIdentifiers(raw_text, substitutions));
		while(text.size() >= 2 && text[0] == '(' && text[text.size() - 1] == ')') {
			int depth = 0;
			bool encloses_all = true;
			for(size_t i = 0; i < text.size(); ++i) {
				if(text[i] == '(') ++depth;
				else if(text[i] == ')' && --depth == 0 && i + 1 != text.size()) {
					encloses_all = false;
					break;
				}
			}
			if(!encloses_all || depth != 0) break;
			text = CanonicalSpelling(text.substr(1, text.size() - 2));
		}
		if(FindConstantArray(text, context)) {
			value.pointer = true;
			value.array = array_key(text);
			return value;
		}
		int depth = 0;
		for(size_t i = 0; i < text.size(); ++i) {
			if(text[i] == '(') ++depth;
			else if(text[i] == ')') --depth;
			if(depth != 0 || i == 0 || (text[i] != '+' && text[i] != '-')) continue;
			const string base = CanonicalSpelling(text.substr(0, i));
			const vector<PA19IntegralValue>* array = FindConstantArray(base, context);
			if(!array) continue;
			long long offset = 0;
			const string raw_offset = CanonicalSpelling(text.substr(i + 1));
			if(raw_offset.find("sizeof...") != string::npos) {
				offset = static_cast<long long>(array->size());
			} else {
				PA19IntegralValue offset_value;
				if(!EvaluateIntegralText(raw_offset, context, substitutions, &offset_value))
					return SourceValue();
				offset = static_cast<long long>(PA19Signed(offset_value));
			}
			if(text[i] == '-') offset = -offset;
			value.pointer = true;
			value.array = array_key(base);
			value.index = offset;
			return value;
		}
		PA19IntegralValue integral;
		if(EvaluateIntegralText(text, context, substitutions, &integral))
			value.integral = integral;
		return value;
	};

	evaluate = [&](const CPPGMAstNodePtr& node) {
		SourceValue value;
		if(!node) return value;
		if(node->kind == "literal") {
			if(!PA19DecodeCharacter(node->value, &value.integral) &&
				!PA19ParseInteger(node->value, &value.integral))
				value = source_text_value(node->value);
			return value;
		}
		if(node->kind == "keyword-literal") {
			const string spelling = RemoveMarker(node->value);
			if(spelling == "true" || spelling == "false")
				value.integral = PA19IntegralValue::Signed(spelling == "true", "bool", 1);
			return value;
		}
		if(node->kind == "id-expression") return lookup(node->value);
		if(node->kind == "parenthesized-expression" || node->kind == "initializer" ||
			node->kind == "paren-initializer" || node->kind == "initializer-clause" ||
			node->kind == "condition")
			return node->children.empty() ? value : evaluate(node->children[0]);
		if(node->kind == "subscript-expression" && node->children.size() >= 2) {
			SourceValue base = evaluate(node->children[0]);
			SourceValue index = evaluate(node->children[1]);
			if(!base.pointer || !index.integral.known) return SourceValue();
			base.index += PA19Signed(index.integral);
			return pointer_element(base);
		}
		if(node->kind == "cast-expression" && node->children.size() >= 2) {
			SourceValue operand = evaluate(node->children[1]);
			const PA19IntegralType target = PA19Type(ResolveAlias(
				NodeTypeSpelling(node->children[0]), context));
			if(!operand.integral.known || !target.integral) return SourceValue();
			value.integral = PA19Convert(operand.integral, target);
			return value;
		}
		if(node->kind == "unary-expression" && !node->children.empty()) {
			const string op = operator_from_node(node->value);
			if(op == "*") return pointer_element(evaluate(node->children[0]));
			if(op == "++" || op == "--") {
				const CPPGMAstNodePtr operand_node = node->children[0];
				if(!operand_node || operand_node->kind != "id-expression") return SourceValue();
				SourceValue current = lookup(operand_node->value);
				if(current.pointer) current.index += op == "++" ? 1 : -1;
				else if(current.integral.known) current.integral = PA19Binary(
					op == "++" ? "+" : "-", current.integral,
					PA19IntegralValue::Signed(1));
				else return SourceValue();
				set_value(operand_node->value, current);
				return current;
			}
			SourceValue operand = evaluate(node->children[0]);
			if(!operand.integral.known) return SourceValue();
			if(op == "!") value.integral = PA19IntegralValue::Signed(
				!PA19Raw(operand.integral), "int", 32);
			else if(op == "+") value.integral = PA19Promote(operand.integral);
			else if(op == "-") {
				const PA19IntegralValue promoted = PA19Promote(operand.integral);
				const PA19IntegralType type = promoted.type;
				const unsigned long long raw_value = (0ULL - PA19Raw(promoted)) &
					PA19Mask(type.bits);
				value.integral = type.is_unsigned ? PA19IntegralValue::Unsigned(
					raw_value, type.name, type.bits) : PA19IntegralValue::Signed(
					static_cast<long long>(raw_value), type.name, type.bits);
			} else if(op == "~") {
				const PA19IntegralValue promoted = PA19Promote(operand.integral);
				const PA19IntegralType type = promoted.type;
				const unsigned long long raw_value = (~PA19Raw(promoted)) & PA19Mask(type.bits);
				value.integral = type.is_unsigned ? PA19IntegralValue::Unsigned(
					raw_value, type.name, type.bits) : PA19IntegralValue::Signed(
					static_cast<long long>(raw_value), type.name, type.bits);
			} else return SourceValue();
			return value;
		}
		if(node->kind == "assignment-expression" && node->children.size() >= 2) {
			const string op = operator_from_node(node->value);
			SourceValue right = evaluate(node->children[1]);
			if(op != "=") {
				SourceValue left = evaluate(node->children[0]);
				if(!left.integral.known || !right.integral.known) return SourceValue();
				right.integral = PA19Binary(assignment_operator(op), left.integral,
					right.integral);
			}
			if(node->children[0]->kind == "id-expression")
				set_value(node->children[0]->value, right);
			return right;
		}
		if(node->kind == "conditional-expression" && node->children.size() >= 3) {
			SourceValue condition = evaluate(node->children[0]);
			if(!known(condition)) return SourceValue();
			return evaluate(node->children[truth(condition) ? 1 : 2]);
		}
		if(node->kind == "binary-expression" && node->children.size() >= 2) {
			const string op = operator_from_node(node->value);
			SourceValue left = evaluate(node->children[0]);
			if((op == "&&" || op == "and") && known(left) && !truth(left)) {
				value.integral = PA19IntegralValue::Signed(0, "int", 32);
				return value;
			}
			if((op == "||" || op == "or") && known(left) && truth(left)) {
				value.integral = PA19IntegralValue::Signed(1, "int", 32);
				return value;
			}
			SourceValue right = evaluate(node->children[1]);
			if(!known(left) || !known(right)) return SourceValue();
			if(left.pointer || right.pointer) {
				if((op == "==" || op == "!=") && left.pointer && right.pointer) {
					const bool equal = left.array == right.array && left.index == right.index;
					value.integral = PA19IntegralValue::Signed(op == "==" ? equal : !equal,
						"bool", 1);
					return value;
				}
				if(left.pointer && right.integral.known && (op == "+" || op == "-")) {
					left.index += (op == "+" ? 1 : -1) * PA19Signed(right.integral);
					return left;
				}
				if(left.pointer && right.pointer && op == "-") {
					value.integral = PA19IntegralValue::Signed(left.index - right.index);
					return value;
				}
				return SourceValue();
			}
			value.integral = PA19Binary(op, left.integral, right.integral);
			return value;
		}
		if(node->kind == "call-expression" && !node->children.empty() &&
			node->children[0] && node->children[0]->kind == "id-expression") {
			vector<SourceValue> arguments;
			if(node->children.size() > 1 && node->children[1])
				for(size_t i = 0; i < node->children[1]->children.size(); ++i) {
					const CPPGMAstNodePtr argument = node->children[1]->children[i];
					if(argument && argument->kind == "pack-expansion-expression") return SourceValue();
					arguments.push_back(evaluate(argument));
				}
			const CPPGMAstNodePtr target = FindSourceConstantFunction(
				node->children[0]->value, context);
			return target ? evaluate_function(target, arguments) : SourceValue();
		}
		return source_text_value(ConstantExpressionSpelling(node));
	};

	evaluate_statement = [&](const CPPGMAstNodePtr& node) {
		if(!node) return SourceFlow();
		if(node->kind == "compound-statement") {
			frames.push_back(Frame());
			SourceFlow flow;
			for(size_t i = 0; i < node->children.size(); ++i) {
				flow = evaluate_statement(node->children[i]);
				if(flow.kind != SourceFlow::NORMAL) break;
			}
			frames.pop_back();
			return flow;
		}
		if(node->kind == "simple-declaration") {
			const CPPGMAstNodePtr list = ChildOfKindLocal(node, "init-declarator-list");
			if(!list) return SourceFlow();
			for(size_t i = 0; i < list->children.size(); ++i) {
				const CPPGMAstNodePtr item = list->children[i];
				if(!item || item->children.empty()) continue;
				const string name = FirstIdentifierLocal(item->children[0]);
				SourceValue value = item->children.size() > 1 ? evaluate(item->children[1]) : SourceValue();
				if(!name.empty() && !frames.empty()) frames.back()[name] = value;
			}
			return SourceFlow();
		}
		if(node->kind == "expression-statement") {
			if(!node->children.empty()) evaluate(node->children[0]);
			return SourceFlow();
		}
		if(node->kind == "return-statement")
			return SourceFlow(SourceFlow::RETURN,
				node->children.empty() ? SourceValue() : evaluate(node->children[0]));
		if(node->kind == "break-statement") return SourceFlow(SourceFlow::BREAK);
		if(node->kind == "continue-statement") return SourceFlow(SourceFlow::CONTINUE);
		if(node->kind == "if-statement") {
			const CPPGMAstNodePtr condition_wrapper = ChildOfKindLocal(node, "condition");
			const SourceValue condition = condition_wrapper && !condition_wrapper->children.empty() ?
				evaluate(condition_wrapper->children[0]) : SourceValue();
			if(!known(condition)) return SourceFlow();
			const CPPGMAstNodePtr selected = ChildOfKindLocal(node,
				truth(condition) ? "then" : "else");
			return selected && !selected->children.empty() ?
				evaluate_statement(selected->children[0]) : SourceFlow();
		}
		if(node->kind == "while-statement" || node->kind == "do-statement") {
			const bool do_loop = node->kind == "do-statement";
			const CPPGMAstNodePtr condition_wrapper = ChildOfKindLocal(node, "condition");
			const CPPGMAstNodePtr body = ChildOfKindLocal(node, "body") ?
				ChildOfKindLocal(node, "body") : ChildOfKindLocal(node, "compound-statement");
			for(unsigned i = 0; i < 100000; ++i) {
				if(!do_loop || i != 0) {
					const SourceValue condition = condition_wrapper && !condition_wrapper->children.empty() ?
						evaluate(condition_wrapper->children[0]) : SourceValue();
					if(!known(condition) || !truth(condition)) break;
				}
				SourceFlow flow = body ? evaluate_statement(body) : SourceFlow();
				if(flow.kind == SourceFlow::RETURN) return flow;
				if(flow.kind == SourceFlow::BREAK) break;
				if(flow.kind != SourceFlow::CONTINUE && do_loop && i == 0) continue;
			}
			return SourceFlow();
		}
		return SourceFlow();
	};

	evaluate_function = [&](const CPPGMAstNodePtr& target,
		const vector<SourceValue>& arguments) {
		SourceValue value;
		if(!target || target->children.size() < 3) return value;
		unsigned& depth = depths[target.get()];
		if(depth++ >= 512) { --depth; return value; }
		frames.push_back(Frame());
		const CPPGMAstNodePtr clause = target->children.size() > 1 ?
			DescendantOfKind(target->children[1], "parameter-clause") : CPPGMAstNodePtr();
		size_t argument = 0;
		if(clause) for(size_t i = 0; i < clause->children.size(); ++i) {
			const CPPGMAstNodePtr parameter = clause->children[i];
			if(!parameter || parameter->kind != "parameter-declaration") continue;
			const string name = parameter->children.size() > 1 ?
				FirstIdentifierLocal(parameter->children[1]) : string();
			if(!name.empty() && argument < arguments.size()) frames.back()[name] = arguments[argument];
			++argument;
		}
		const SourceFlow flow = evaluate_statement(ChildOfKindLocal(target, "compound-statement"));
		if(flow.kind == SourceFlow::RETURN) value = flow.value;
		frames.pop_back();
		--depth;
		return value;
	};

	const vector<string> argument_text = SplitTemplateArguments(arguments_text);
	vector<SourceValue> arguments;
	for(size_t i = 0; i < argument_text.size(); ++i)
		arguments.push_back(source_text_value(argument_text[i]));
	const SourceValue value = evaluate_function(function, arguments);
	if(!value.integral.known) return false;
	*result = value.integral;
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
	bool expanded_source_call = false;
	for(size_t search = 0; search < expanded.size(); ) {
		if(!IsIdentifierCharacter(expanded[search])) {
			++search;
			continue;
		}
		const size_t begin = search;
		while(search < expanded.size() && IsIdentifierCharacter(expanded[search])) ++search;
		if(search >= expanded.size() || expanded[search] != '(') continue;
		int depth = 0;
		size_t close = string::npos;
		for(size_t position = search; position < expanded.size(); ++position) {
			if(expanded[position] == '(') ++depth;
			else if(expanded[position] == ')' && --depth == 0) {
				close = position;
				break;
			}
		}
		if(close == string::npos) break;
		const string candidate = expanded.substr(begin, close - begin + 1);
		if(!FindSourceConstantFunction(expanded.substr(begin, search - begin), context)) {
			search = close + 1;
			continue;
		}
		PA19IntegralValue call_value;
		if(!EvaluateSourceArrayFunction(candidate, context, substitutions, &call_value)) {
			search = close + 1;
			continue;
		}
		const string replacement = IntegralValueSpelling(call_value);
		expanded.replace(begin, candidate.size(), replacement);
		expanded_source_call = true;
		search = begin + replacement.size();
	}
	if(expanded_source_call) {
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
