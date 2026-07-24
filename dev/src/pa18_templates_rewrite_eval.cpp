#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

#include <functional>

using namespace std;

namespace pa18_templates_internal {

bool PA18TemplateExpander::EvaluateSourceArrayFunction(
	string raw, const string& context,
	const map<string, string>& substitutions, PA19IntegralValue* result)
{
	if(!result) return false;
	string callee, arguments_text;
	if(!SplitTextCall(raw, &callee, &arguments_text)) return false;
	const vector<string> source_arguments = SplitTemplateArguments(arguments_text);
	CPPGMAstNodePtr function = FindSourceConstantFunction(callee, context);
	// The source evaluator sees constexpr overloads before ordinary lowering.
	// Select a template overload whose function parameter clause can accept the
	// current arity; otherwise a same-named zero-argument base case can be
	// selected by FindSourceConstantFunction.
	const vector<const TemplateDefinition*> overloads =
		FindFunctionDefinitions(callee, context);
	for(size_t candidate = 0; candidate < overloads.size(); ++candidate) {
		const TemplateDefinition* definition = overloads[candidate];
		if(!definition || !definition->declaration ||
			SpellNode(definition->declaration->children.empty() ?
				CPPGMAstNodePtr() : definition->declaration->children[0]).find(
					"constexpr") == string::npos) continue;
		const CPPGMAstNodePtr declarator = FunctionDeclarator(definition->declaration);
		const CPPGMAstNodePtr parameters = DescendantOfKind(declarator,
			"parameter-clause");
		size_t total = 0, required = 0;
		if(!FunctionParameterCounts(parameters, &total, &required)) continue;
		bool has_pack = false;
		if(parameters) for(size_t parameter = 0; parameter < parameters->children.size(); ++parameter)
			if(IsFunctionParameterPack(parameters->children[parameter])) has_pack = true;
		if(source_arguments.size() < required ||
			(!has_pack && source_arguments.size() > total)) continue;
		function = definition->declaration;
		break;
	}
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
	typedef map<string, vector<SourceValue> > PackFrame;
	vector<Frame> frames;
	vector<PackFrame> pack_frames;
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
	std::function<CPPGMAstNodePtr(const string&, size_t)> select_function;
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

	select_function = [&](const string& raw_name, size_t arity) {
		const vector<const TemplateDefinition*> candidates =
			FindFunctionDefinitions(raw_name, context);
		for(size_t candidate = 0; candidate < candidates.size(); ++candidate) {
			const TemplateDefinition* definition = candidates[candidate];
			if(!definition || !definition->declaration) continue;
			const CPPGMAstNodePtr declaration = definition->declaration;
			if(SpellNode(declaration->children.empty() ? CPPGMAstNodePtr() :
				declaration->children[0]).find("constexpr") == string::npos) continue;
			const CPPGMAstNodePtr parameters = DescendantOfKind(
				FunctionDeclarator(declaration), "parameter-clause");
			size_t total = 0, required = 0;
			if(!FunctionParameterCounts(parameters, &total, &required)) continue;
			bool has_pack = false;
			if(parameters) for(size_t parameter = 0; parameter < parameters->children.size(); ++parameter)
				if(IsFunctionParameterPack(parameters->children[parameter])) has_pack = true;
			if(arity >= required && (has_pack || arity <= total)) return declaration;
		}
		// Non-template constexpr overloads are collected separately from template
		// definitions.  They are especially important as the base case of a
		// recursive constexpr function whose other overload owns a parameter pack.
		for(map<string, CPPGMAstNodePtr>::const_iterator ordinary =
			function_definitions_.begin(); ordinary != function_definitions_.end(); ++ordinary) {
			if(LastComponent(ordinary->first) != LastComponent(raw_name) ||
				!ordinary->second || ordinary->second->children.size() < 3 ||
				SpellNode(ordinary->second->children[0]).find("constexpr") == string::npos)
				continue;
			const CPPGMAstNodePtr declarator = FunctionDeclarator(ordinary->second);
			const CPPGMAstNodePtr parameters = DescendantOfKind(declarator,
				"parameter-clause");
			size_t total = 0, required = 0;
			if(!FunctionParameterCounts(parameters, &total, &required) ||
				arity < required || arity > total) continue;
			return ordinary->second;
		}
		return FindSourceConstantFunction(raw_name, context);
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
			const string target_spelling = TypeIdSpelling(node->children[0]);
			const PA19IntegralType target = PA19Type(ResolveAlias(
				target_spelling, context));
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
					if(argument && argument->kind == "pack-expansion-expression") {
						string pack_name = PackExpansionIdentifier(
							argument->children.empty() ? CPPGMAstNodePtr() :
							argument->children[0]);
						// This evaluator binds function-parameter packs in its own
						// `pack_frames`; the rewriter's active typed-pack map is
						// intentionally empty while a dependent constexpr body is
						// inspected.  Recover a direct identifier for that local
						// frame without weakening template replay's pack matching.
						if(pack_name.empty() && !argument->children.empty() &&
							argument->children[0] &&
							argument->children[0]->kind == "id-expression")
							pack_name = RemoveMarker(argument->children[0]->value);
						bool found_pack = false;
						for(vector<PackFrame>::reverse_iterator frame = pack_frames.rbegin();
							frame != pack_frames.rend() && !found_pack; ++frame) {
							map<string, vector<SourceValue> >::const_iterator values =
								frame->find(pack_name);
							if(values == frame->end()) continue;
							arguments.insert(arguments.end(), values->second.begin(), values->second.end());
							found_pack = true;
						}
						if(!found_pack) return SourceValue();
						continue;
					}
					arguments.push_back(evaluate(argument));
				}
			const CPPGMAstNodePtr target = select_function(
				node->children[0]->value, arguments.size());
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
		pack_frames.push_back(PackFrame());
		const CPPGMAstNodePtr clause = target->children.size() > 1 ?
			DescendantOfKind(target->children[1], "parameter-clause") : CPPGMAstNodePtr();
		size_t argument = 0;
		if(clause) for(size_t i = 0; i < clause->children.size(); ++i) {
			const CPPGMAstNodePtr parameter = clause->children[i];
			if(!parameter || parameter->kind != "parameter-declaration") continue;
			const string name = parameter->children.size() > 1 ?
				FirstIdentifierLocal(parameter->children[1]) : string();
			if(IsFunctionParameterPack(parameter)) {
				vector<SourceValue>& values = pack_frames.back()[name];
				while(argument < arguments.size()) values.push_back(arguments[argument++]);
				continue;
			}
			if(!name.empty() && argument < arguments.size()) frames.back()[name] = arguments[argument];
			++argument;
		}
		const SourceFlow flow = evaluate_statement(ChildOfKindLocal(target, "compound-statement"));
		if(flow.kind == SourceFlow::RETURN) value = flow.value;
		pack_frames.pop_back();
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

} // namespace pa18_templates_internal
