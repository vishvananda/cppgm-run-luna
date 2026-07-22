#include "pa11_semantics_analyzer.h"
#include <cstdlib>
namespace {

CPPGMAstNodePtr ConstantInitializer(const CPPGMAstNodePtr& node)
{
	if (!node) return CPPGMAstNodePtr();
	if (node->kind == "initializer" || node->kind == "paren-initializer" ||
		node->kind == "default-argument" || node->kind == "initializer-clause")
		return node->children.empty() ? CPPGMAstNodePtr() : node->children[0];
	return node;
}

CPPGMAstNodePtr FunctionParameterClause(const CPPGMAstNodePtr& declaration)
{
	if (!declaration || declaration->children.size() < 2)
		return CPPGMAstNodePtr();
	return DescendantOfKind(declaration->children[1], "parameter-clause");
}

CPPGMAstNodePtr FunctionBody(const CPPGMAstNodePtr& declaration)
{
	return ChildOfKind(declaration, "compound-statement");
}

CPPGMAstNodePtr ConstantReturnStatement(const CPPGMAstNodePtr& node)
{
	if (!node) return CPPGMAstNodePtr();
	if (node->kind == "return-statement") return node;
	for (size_t i = 0; i < node->children.size(); ++i)
	{
		CPPGMAstNodePtr result = ConstantReturnStatement(node->children[i]);
		if (result) return result;
	}
	return CPPGMAstNodePtr();
}

bool ConstantTrue(const ConstantValue& value)
{
	if (value.integral.known) return PA19Raw(value.integral) != 0;
	if (value.floating_known) return value.floating != 0;
	if (value.kind == ConstantValue::CONSTANT_POINTER)
		return value.pointer && !value.pointer->null_pointer;
	return value.kind == ConstantValue::CONSTANT_OBJECT && value.object;
}

bool IsFloatingLiteral(const string& raw)
{
	string spelling = raw;
	const size_t marker = spelling.find(':');
	if (marker != string::npos && spelling.substr(0, marker) == "TT_LITERAL")
		spelling.erase(0, marker + 1);
	if (spelling == "true" || spelling == "false") return false;
	if (!spelling.empty() && (spelling[0] == '+' || spelling[0] == '-'))
		spelling.erase(0, 1);
	if (spelling.empty() || (!isdigit(static_cast<unsigned char>(spelling[0])) &&
		(spelling[0] != '.' || spelling.size() < 2 ||
		 !isdigit(static_cast<unsigned char>(spelling[1]))))) return false;
	return spelling.find('.') != string::npos || spelling.find('e') != string::npos ||
		spelling.find('E') != string::npos || spelling.find('p') != string::npos ||
		spelling.find('P') != string::npos;
}

long double ParseFloatingLiteral(const string& raw, TypePtr* type)
{
	string spelling = raw;
	const size_t marker = spelling.find(':');
	if (marker != string::npos && spelling.substr(0, marker) == "TT_LITERAL")
		spelling.erase(0, marker + 1);
	char suffix = 0;
	if (!spelling.empty() && (spelling[spelling.size() - 1] == 'f' ||
		spelling[spelling.size() - 1] == 'F' || spelling[spelling.size() - 1] == 'l' ||
		spelling[spelling.size() - 1] == 'L'))
	{
		suffix = spelling[spelling.size() - 1];
		spelling.erase(spelling.size() - 1);
	}
	char* end = 0;
	const long double result = strtold(spelling.c_str(), &end);
	if (!end || *end != '\0') throw logic_error("unsupported floating constant expression");
	if (type) *type = suffix == 'f' || suffix == 'F' ? Fundamental("float") :
		suffix == 'l' || suffix == 'L' ? Fundamental("long double") : Fundamental("double");
	return result;
}

long double NumericValue(const ConstantValue& value)
{
	return value.floating_known ? value.floating :
		static_cast<long double>(PA19Signed(value.integral));
}

bool NoexceptMemberCall(Analyzer& analyzer, const CPPGMAstNodePtr& call,
	Scope* scope, const CPPGMAstNodePtr& callee)
{
	TypePtr object = analyzer.ExpressionType(callee->children[0], scope);
	while (object && (object->kind == TYPE_LVALUE_REFERENCE ||
		object->kind == TYPE_RVALUE_REFERENCE || object->kind == TYPE_POINTER))
		object = object->child;
	const string member_name = callee->children[1]->value;
	for (TypePtr current = object; current; current = current->direct_base) {
		Scope* owner = analyzer.ScopeForType(current);
		if (!owner) continue;
		for (size_t i = 0; i < owner->bindings.size(); ++i) {
			Binding& candidate = owner->bindings[i];
			if (candidate.kind != BIND_FUNCTION || candidate.name != member_name ||
				!candidate.type || candidate.type->kind != TYPE_FUNCTION) continue;
			if (candidate.type->parameters.size() != (call->children.size() > 1 ?
				call->children[1]->children.size() : 0)) continue;
			bool matches = true;
			if (call->children.size() > 1)
				for (size_t argument = 0; argument < call->children[1]->children.size(); ++argument) {
					const CPPGMAstNodePtr actual = call->children[1]->children[argument];
					TypePtr actual_type;
					if (actual && actual->kind == "call-expression" && !actual->children.empty() &&
						actual->children[0] && actual->children[0]->kind == "id-expression") {
						Binding* named_type = analyzer.ResolveBinding(scope,
							actual->children[0]->value);
						if (named_type && (named_type->kind == BIND_TYPE ||
							named_type->kind == BIND_TYPE_ALIAS) ) actual_type = named_type->type;
					}
					if (!actual_type) actual_type = analyzer.ExpressionType(actual, scope);
					TypePtr formal = candidate.type->parameters[argument];
					while (formal && (formal->kind == TYPE_LVALUE_REFERENCE ||
						formal->kind == TYPE_RVALUE_REFERENCE)) formal = formal->child;
					if (actual_type && formal && actual_type->kind == TYPE_CLASS &&
						formal->kind == TYPE_CLASS && !SameTypeIgnoringTopCv(actual_type, formal))
						matches = false;
				}
			if (matches) return candidate.noexcept_specified || Analyzer::HasNodeValue(
				candidate.declaration, "function-qualifier", "noexcept");
		}
	}
	return false;
}

bool NoexceptCall(Analyzer& analyzer, const CPPGMAstNodePtr& call, Scope* scope)
{
	if (!call || call->kind != "call-expression" || call->children.empty()) return false;
	CPPGMAstNodePtr callee = call->children[0];
	if (!callee) return false;
	if (callee->kind == "member-expression" && callee->children.size() >= 2)
		return NoexceptMemberCall(analyzer, call, scope, callee);
	if (callee->kind == "call-expression" && !callee->children.empty() &&
		callee->children[0] && callee->children[0]->kind == "id-expression")
	{
		// A declval<T>() call is an unevaluated source of a T object.  The
		// parser retains the template-id in the id spelling, so recover the
		// class argument directly.  PA18 may instead replace that spelling with
		// a materialized function name; in that case its call result supplies the
		// same class type.
		const string raw = callee->children[0]->value;
		const size_t open = raw.find('<');
		const size_t close = raw.rfind('>');
		TypePtr object;
		if (raw.compare(0, open, "declval") == 0 && open != string::npos &&
			close != string::npos && close > open)
		{
			string type_name = raw.substr(open + 1, close - open - 1);
			while (!type_name.empty() && isspace(static_cast<unsigned char>(type_name[0])))
				type_name.erase(type_name.begin());
			while (!type_name.empty() && (type_name[type_name.size() - 1] == '&' ||
				type_name[type_name.size() - 1] == '*' ||
				isspace(static_cast<unsigned char>(type_name[type_name.size() - 1]))))
				type_name.erase(type_name.size() - 1);
			if (type_name.compare(0, 6, "const ") == 0) type_name.erase(0, 6);
			if (type_name.compare(0, 8, "volatile ") == 0) type_name.erase(0, 8);
			Binding* type_binding = analyzer.ResolveBinding(scope, type_name);
			object = type_binding && (type_binding->kind == BIND_TYPE ||
				type_binding->kind == BIND_TYPE_ALIAS) ? type_binding->type : TypePtr();
		}
		if (!object)
		{
			Binding* generated = analyzer.ResolveBinding(scope, raw);
			if (generated && generated->kind == BIND_FUNCTION && generated->type &&
				generated->type->kind == TYPE_FUNCTION)
				object = generated->type->child;
		}
		if (!object && raw.compare(0, 14, "declval__inst_") == 0)
		{
			string encoded = raw.substr(14);
			if (encoded.compare(0, 6, "const_") == 0) encoded.erase(0, 6);
			if (encoded.size() > 5 && encoded.compare(encoded.size() - 5, 5, "_rref") == 0)
				encoded.erase(encoded.size() - 5);
			else if (encoded.size() > 4 && encoded.compare(encoded.size() - 4, 4, "_ref") == 0)
				encoded.erase(encoded.size() - 4);
			else if (encoded.size() > 4 && encoded.compare(encoded.size() - 4, 4, "_ptr") == 0)
				encoded.erase(encoded.size() - 4);
			Binding* type_binding = analyzer.ResolveBinding(scope, encoded);
			if (type_binding && (type_binding->kind == BIND_TYPE ||
				type_binding->kind == BIND_TYPE_ALIAS)) object = type_binding->type;
		}
		while (object && (object->kind == TYPE_LVALUE_REFERENCE ||
			object->kind == TYPE_RVALUE_REFERENCE || object->kind == TYPE_POINTER))
			object = object->child;
		if (object && object->kind == TYPE_CLASS)
		{
			Scope* owner = analyzer.ScopeForType(object);
			const size_t arity = call->children.size() > 1 ?
				call->children[1]->children.size() : 0;
			if (owner)
				for (size_t i = 0; i < owner->bindings.size(); ++i)
				{
					Binding& candidate = owner->bindings[i];
					if (candidate.kind == BIND_FUNCTION && candidate.name == "operator()" &&
						candidate.type && candidate.type->kind == TYPE_FUNCTION &&
						candidate.type->parameters.size() == arity)
						return candidate.noexcept_specified || Analyzer::HasNodeValue(
							candidate.declaration, "function-qualifier", "noexcept");
				}
		}
		return false;
	}
	if (callee->kind != "id-expression") return false;
	Binding* binding = analyzer.ResolveBinding(scope, callee->value);
	if (!binding) return false;
	if (binding->kind == BIND_FUNCTION)
		return binding->noexcept_specified ||
			Analyzer::HasNodeValue(binding->declaration, "function-qualifier", "noexcept");
	if (binding->kind != BIND_TYPE && binding->kind != BIND_TYPE_ALIAS) return false;
	Scope* class_scope = analyzer.ScopeForType(binding->type);
	if (!class_scope) return false;
	const string constructor_name = LastComponent(binding->type->name);
	for (size_t i = 0; i < class_scope->bindings.size(); ++i)
	{
		Binding& candidate = class_scope->bindings[i];
		if (candidate.kind != BIND_FUNCTION || candidate.name != constructor_name) continue;
		if (candidate.noexcept_specified || Analyzer::HasNodeValue(candidate.declaration,
			"function-qualifier", "noexcept")) return true;
		if (Analyzer::HasNodeValue(candidate.declaration, "special-initializer", "default"))
			return true;
	}
	return false;
}

bool ConstantKnown(const ConstantValue& value)
{
	return value.integral.known || value.floating_known ||
		(value.kind == ConstantValue::CONSTANT_OBJECT && value.object) ||
		(value.kind == ConstantValue::CONSTANT_POINTER && value.pointer);
}

bool IsConstexprDeclaration(const CPPGMAstNodePtr& declaration)
{
	return Analyzer::HasNodeValue(declaration, "decl-specifier", "constexpr") ||
		Analyzer::HasNodeValue(declaration, "specifier", "constexpr");
}

TypePtr UnwrapConstantType(TypePtr type)
{
	while (type && (type->kind == TYPE_LVALUE_REFERENCE ||
		type->kind == TYPE_RVALUE_REFERENCE)) type = type->child;
	return type;
}

string StringSpelling(const string& raw)
{
	string spelling = raw;
	const size_t marker = spelling.find(':');
	if (marker != string::npos && spelling.substr(0, marker) == "TT_LITERAL")
		spelling.erase(0, marker + 1);
	if (spelling.size() >= 2 && spelling[0] == 'u' && spelling[1] == '8')
		spelling.erase(0, 2);
	else if (spelling.size() >= 1 && (spelling[0] == 'u' || spelling[0] == 'U' ||
		spelling[0] == 'L')) spelling.erase(0, 1);
	if (spelling.size() >= 2 && spelling[0] == '"' && spelling[spelling.size() - 1] == '"')
		spelling = spelling.substr(1, spelling.size() - 2);
	return spelling;
}

vector<unsigned long long> DecodeString(const string& raw)
{
	const string spelling = StringSpelling(raw);
	vector<unsigned long long> result;
	for (size_t i = 0; i < spelling.size(); ++i)
	{
		unsigned long long value = static_cast<unsigned char>(spelling[i]);
		if (spelling[i] == '\\' && i + 1 < spelling.size())
		{
			const char escaped = spelling[++i];
			switch (escaped)
			{
			case 'n': value = '\n'; break;
			case 'r': value = '\r'; break;
			case 't': value = '\t'; break;
			case '\\': value = '\\'; break;
			case '\'': value = '\''; break;
			case '"': value = '"'; break;
			case '0': value = 0; break;
			default: value = static_cast<unsigned char>(escaped); break;
			}
		}
		result.push_back(value);
	}
	return result;
}

string AssignmentOperator(const string& operation)
{
	if (operation == "+=") return "+";
	if (operation == "-=") return "-";
	if (operation == "*=") return "*";
	if (operation == "/=") return "/";
	if (operation == "%=") return "%";
	if (operation == "<<=") return "<<";
	if (operation == ">>=") return ">>";
	if (operation == "&=") return "&";
	if (operation == "|=") return "|";
	if (operation == "^=") return "^";
	return string();
}

}

bool Analyzer::ConstantFrameValue(const string& name, ConstantValue* value) const
{
	for (vector<map<string, ConstantValue> >::const_reverse_iterator frame =
		constant_frames_.rbegin(); frame != constant_frames_.rend(); ++frame)
	{
		map<string, ConstantValue>::const_iterator found = frame->find(name);
		if (found == frame->end()) continue;
		if (value) *value = found->second;
		return true;
	}
	return false;
}

bool Analyzer::ConstantPackValue(const string& name, vector<ConstantValue>* value) const
{
	for (vector<map<string, vector<ConstantValue> > >::const_reverse_iterator frame =
		constant_pack_frames_.rbegin(); frame != constant_pack_frames_.rend(); ++frame)
	{
		map<string, vector<ConstantValue> >::const_iterator found = frame->find(name);
		if (found == frame->end()) continue;
		if (value) *value = found->second;
		return true;
	}
	return false;
}

namespace {

bool FunctionAcceptsArity(const Binding& function, size_t argument_count)
{
	if (!function.type || function.type->kind != TYPE_FUNCTION) return false;
	const CPPGMAstNodePtr clause = FunctionParameterClause(function.declaration);
	bool pack = false;
	size_t fixed = 0;
	if (clause)
		for (size_t i = 0; i < clause->children.size(); ++i)
		{
			const CPPGMAstNodePtr parameter = clause->children[i];
			if (!parameter) continue;
			if (parameter->kind == "parameter-pack" || DescendantOfKind(parameter, "parameter-pack"))
			{
				pack = true;
				continue;
			}
			if (parameter->kind == "parameter-declaration") ++fixed;
		}
	if (pack) return argument_count >= fixed;
	if (function.type->parameters.size() == argument_count) return true;
	if (!clause || function.type->parameters.size() < argument_count) return false;
	return argument_count >= fixed;
}

void FindConstantFunctionsInScope(const Scope* scope, const string& name,
	size_t argument_count, vector<Binding*>* result)
{
	if (!scope || !result) return;
	for (size_t i = 0; i < scope->bindings.size(); ++i)
	{
		const Binding& candidate = scope->bindings[i];
		if (candidate.kind == BIND_FUNCTION &&
			(LastComponent(candidate.name) == LastComponent(name) || candidate.name == name) &&
			FunctionAcceptsArity(candidate, argument_count))
		{
			result->push_back(const_cast<Binding*>(&candidate));
		}
	}
	for (size_t i = 0; i < scope->children.size(); ++i)
		FindConstantFunctionsInScope(scope->children[i].get(), name, argument_count, result);
}

}

Binding* Analyzer::FindConstantFunction(const string& name, Scope* scope,
	size_t argument_count) const
{
	vector<Binding*> candidates;
	map<string, vector<Binding*> >::const_iterator templates =
		constant_template_functions_.find(LastComponent(name));
	if (templates != constant_template_functions_.end())
		for (size_t i = 0; i < templates->second.size(); ++i)
			if (FunctionAcceptsArity(*templates->second[i], argument_count))
				candidates.push_back(templates->second[i]);
	FindConstantFunctionsInScope(global_.get(), name, argument_count, &candidates);
	if (candidates.empty() && scope && scope != global_.get())
		FindConstantFunctionsInScope(scope, name, argument_count, &candidates);
	for (size_t i = 0; i < candidates.size(); ++i)
		if (candidates[i]->declaration && IsConstexprDeclaration(candidates[i]->declaration))
			return candidates[i];
	return candidates.empty() ? 0 : candidates[0];
}

void Analyzer::SetConstantFrameValue(const string& name, const ConstantValue& value)
{
	if (name.empty()) return;
	for (vector<map<string, ConstantValue> >::reverse_iterator frame =
		constant_frames_.rbegin(); frame != constant_frames_.rend(); ++frame)
	{
		if (frame->find(name) == frame->end()) continue;
		(*frame)[name] = value;
		return;
	}
	if (!constant_frames_.empty()) constant_frames_.back()[name] = value;
}

Analyzer::ConstantFlow Analyzer::EvaluateCompound(const CPPGMAstNodePtr& compound,
	Scope* scope)
{
	constant_frames_.push_back(map<string, ConstantValue>());
	constant_pack_frames_.push_back(map<string, vector<ConstantValue> >());
	ConstantFlow result;
	if (compound)
		for (size_t i = 0; i < compound->children.size(); ++i)
		{
			result = EvaluateStatement(compound->children[i], scope);
			if (result.kind != ConstantFlow::NORMAL) break;
		}
	constant_frames_.pop_back();
	constant_pack_frames_.pop_back();
	return result;
}

Analyzer::ConstantFlow Analyzer::EvaluateConditionStatement(
	const CPPGMAstNodePtr& statement, Scope* scope)
{
	CPPGMAstNodePtr condition = ChildOfKind(statement, "condition");
	CPPGMAstNodePtr selected;
	ConstantValue value;
	if (condition && condition->kind == "condition" && !condition->children.empty())
		condition = condition->children[0];
	const bool declaration = condition && condition->kind == "condition-declaration";
	if (declaration) constant_frames_.push_back(map<string, ConstantValue>());
	if (declaration && condition->children.size() >= 3)
	{
		const string name = FirstIdentifier(condition->children[1]);
		value = Evaluate(condition->children[2], scope);
		if (!name.empty()) constant_frames_.back()[name] = value;
	}
	else value = Evaluate(condition, scope);
	if (ConstantKnown(value))
		selected = ChildOfKind(statement, ConstantTrue(value) ? "then" : "else");
	ConstantFlow result;
	if (ConstantKnown(value) && selected && !selected->children.empty())
		result = EvaluateStatement(selected->children[0], scope);
	if (declaration) constant_frames_.pop_back();
	return result;
}

Analyzer::ConstantFlow Analyzer::EvaluateStatement(
	const CPPGMAstNodePtr& statement, Scope* scope)
{
	if (!statement) return ConstantFlow();
	if (statement->kind == "compound-statement")
	{
		Scope* block = scope;
		map<const CPPGMAstNode*, Scope*>::const_iterator found =
			compound_scopes_.find(statement.get());
		if (found != compound_scopes_.end()) block = found->second;
		return EvaluateCompound(statement, block);
	}
	if (statement->kind == "simple-declaration")
	{
		CPPGMAstNodePtr list = ChildOfKind(statement, "init-declarator-list");
		if (!list) return ConstantFlow();
		for (size_t i = 0; i < list->children.size(); ++i)
		{
			CPPGMAstNodePtr item = list->children[i];
			if (!item || item->children.empty()) continue;
			const string name = FirstIdentifier(item->children[0]);
			TypePtr declared_type;
			if (statement->children.size() > 0)
			{
				SpecFacts facts;
				declared_type = TypeFromSpecSeq(statement->children[0], scope, &facts);
				declared_type = BuildDeclarator(item->children[0], declared_type, scope);
			}
			ConstantValue value;
			if (item->children.size() > 1) value = EvaluateTyped(item->children[1], scope, declared_type);
			else value = DefaultConstantValue(declared_type, scope);
			if (!name.empty() && !constant_frames_.empty())
				constant_frames_.back()[name] = value;
		}
		return ConstantFlow();
	}
	if (statement->kind == "expression-statement")
	{
		if (!statement->children.empty()) Evaluate(statement->children[0], scope);
		return ConstantFlow();
	}
	if (statement->kind == "return-statement")
	{
		ConstantValue value;
		if (!statement->children.empty()) value = Evaluate(statement->children[0], scope);
		return ConstantFlow(ConstantFlow::RETURN, value);
	}
	if (statement->kind == "if-statement")
		return EvaluateConditionStatement(statement, scope);
	if (statement->kind == "break-statement")
		return ConstantFlow(ConstantFlow::BREAK);
	if (statement->kind == "continue-statement")
		return ConstantFlow(ConstantFlow::CONTINUE);
	if (statement->kind == "labeled-statement")
		return statement->children.empty() ? ConstantFlow() :
			EvaluateStatement(statement->children[0], scope);
	if (statement->kind == "static-assert-declaration")
	{
		if (!statement->children.empty()) Evaluate(statement->children[0], scope);
		return ConstantFlow();
	}
	if (statement->kind == "while-statement" || statement->kind == "do-statement")
	{
		const bool do_loop = statement->kind == "do-statement";
		CPPGMAstNodePtr condition = ChildOfKind(statement, "condition");
		CPPGMAstNodePtr body = ChildOfKind(statement, "body");
		if (!body) body = ChildOfKind(statement, "compound-statement");
		for (unsigned iteration = 0; iteration < kConstantLoopIterationLimit; ++iteration)
		{
			if (do_loop && iteration == 0) { /* execute the body below */ }
			else if (!do_loop || iteration != 0)
			{
				ConstantValue test = Evaluate(condition, scope);
				if (!ConstantKnown(test) || !ConstantTrue(test)) break;
			}
			ConstantFlow flow = body ? EvaluateStatement(body, scope) : ConstantFlow();
			if (flow.kind == ConstantFlow::RETURN) return flow;
			if (flow.kind == ConstantFlow::BREAK) break;
			if (flow.kind == ConstantFlow::CONTINUE) continue;
			if (do_loop && iteration == 0) continue;
		}
		return ConstantFlow();
	}
	if (statement->kind == "for-statement")
	{
		constant_frames_.push_back(map<string, ConstantValue>());
		CPPGMAstNodePtr initialization = ChildOfKind(statement, "for-init-statement");
		if (initialization && !initialization->children.empty())
			EvaluateStatement(initialization->children[0], scope);
		CPPGMAstNodePtr condition = ChildOfKind(statement, "condition");
		if (condition && condition->kind == "condition" && !condition->children.empty())
			condition = condition->children[0];
		CPPGMAstNodePtr iteration = ChildOfKind(statement, "iteration");
		if (iteration && iteration->kind == "iteration" && !iteration->children.empty())
			iteration = iteration->children[0];
		CPPGMAstNodePtr body = ChildOfKind(statement, "body");
		if (!body) body = ChildOfKind(statement, "compound-statement");
		ConstantFlow result;
		for (unsigned count = 0; count < kConstantLoopIterationLimit; ++count)
		{
			ConstantValue test;
			if (condition && condition->kind == "condition-declaration" &&
				condition->children.size() >= 3)
			{
				const string name = FirstIdentifier(condition->children[1]);
				test = Evaluate(condition->children[2], scope);
				if (!name.empty()) constant_frames_.back()[name] = test;
			}
			else test = condition ? Evaluate(condition, scope) :
				FromIntegralValue(PA19IntegralValue::Signed(1));
			if (!ConstantKnown(test) || !ConstantTrue(test)) break;
			result = body ? EvaluateStatement(body, scope) : ConstantFlow();
			if (result.kind == ConstantFlow::RETURN) break;
			if (result.kind == ConstantFlow::BREAK) { result.kind = ConstantFlow::NORMAL; break; }
			if (iteration) Evaluate(iteration, scope);
		}
		constant_frames_.pop_back();
		return result;
	}
	return ConstantFlow();
}

ConstantValue Analyzer::EvaluateFunctionCall(Binding* function,
	const CPPGMAstNodePtr& call, Scope* caller_scope,
	const ConstantValue& receiver, const TypePtr& expected_type)
{
	if (!function || function->kind != BIND_FUNCTION || !function->declaration ||
		!IsConstexprDeclaration(function->declaration))
		return ConstantValue();
	CPPGMAstNodePtr body = FunctionBody(function->declaration);
	if (!body) return ConstantValue();
	unsigned& depth = constant_function_depth_[function->declaration.get()];
	if (depth >= kConstantFunctionDepthLimit) return ConstantValue();
	CPPGMAstNodePtr arguments = call->children.size() > 1 ? call->children[1] : CPPGMAstNodePtr();
	vector<ConstantValue> values;
	if (arguments)
		for (size_t i = 0; i < arguments->children.size(); ++i)
		{
			CPPGMAstNodePtr argument_node = arguments->children[i];
			if (argument_node && argument_node->kind == "pack-expansion-expression" &&
				!argument_node->children.empty() && argument_node->children[0] &&
				argument_node->children[0]->kind == "id-expression")
			{
				vector<ConstantValue> expanded;
				if (ConstantPackValue(argument_node->children[0]->value, &expanded))
					values.insert(values.end(), expanded.begin(), expanded.end());
				continue;
			}
			values.push_back(Evaluate(argument_node, caller_scope));
		}
	constant_frames_.push_back(map<string, ConstantValue>());
	constant_pack_frames_.push_back(map<string, vector<ConstantValue> >());
	const bool has_receiver = receiver.kind != ConstantValue::CONSTANT_UNKNOWN;
	if (has_receiver) constant_receivers_.push_back(receiver);
	++depth;
	if (has_receiver) constant_frames_.back()["this"] = receiver;
	CPPGMAstNodePtr clause = FunctionParameterClause(function->declaration);
	const vector<TypePtr> parameters = function->type && function->type->kind == TYPE_FUNCTION ?
		function->type->parameters : vector<TypePtr>();
	size_t argument = 0;
	if (clause)
		for (size_t i = 0; i < clause->children.size(); ++i)
		{
			CPPGMAstNodePtr parameter = clause->children[i];
			if (!parameter || parameter->kind != "parameter-declaration") continue;
			const string name = parameter->children.size() > 1 ?
				FirstIdentifier(parameter->children[1]) : string();
			const bool parameter_pack = static_cast<bool>(DescendantOfKind(parameter, "parameter-pack"));
			if (parameter_pack)
			{
				vector<ConstantValue> expanded;
				while (argument < values.size()) expanded.push_back(values[argument++]);
				if (!name.empty()) constant_pack_frames_.back()[name] = expanded;
				continue;
			}
			ConstantValue value;
			if (argument < values.size()) value = values[argument++];
			else
			{
				CPPGMAstNodePtr default_node = ChildOfKind(parameter, "default-argument");
				value = default_node ? EvaluateTyped(ConstantInitializer(default_node), caller_scope,
					(i < parameters.size() ? parameters[i] : TypePtr())) :
					ConstantValue();
			}
			if (!name.empty()) constant_frames_.back()[name] = value;
			if (i < parameters.size() && parameters[i])
				constant_frames_.back()[name] = ConvertConstantValue(value, parameters[i], caller_scope);
		}
	Scope* body_scope = caller_scope;
	map<const CPPGMAstNode*, Scope*>::const_iterator body_scope_found =
		compound_scopes_.find(body.get());
	if (body_scope_found != compound_scopes_.end()) body_scope = body_scope_found->second;
	ConstantFlow flow = EvaluateCompound(body, body_scope);
	ConstantValue result = flow.kind == ConstantFlow::RETURN ? flow.value : ConstantValue();
	if (result.kind != ConstantValue::CONSTANT_UNKNOWN && function->type && function->type->child)
		result = ConvertConstantValue(result, expected_type ? expected_type : function->type->child,
			caller_scope);
	--depth;
	constant_frames_.pop_back();
	constant_pack_frames_.pop_back();
	if (has_receiver) constant_receivers_.pop_back();
	return result;
}

ConstantValue Analyzer::DefaultConstantValue(const TypePtr& raw_type, Scope* scope)
{
	TypePtr type = UnwrapConstantType(raw_type);
	if (!type) return ConstantValue();
	if (type->kind == TYPE_FUNDAMENTAL)
	{
		if (type->name == "float" || type->name == "double" || type->name == "long double")
			return FromFloatingValue(0, type);
		if (type->name == "nullptr_t")
		{
			shared_ptr<ConstantPointer> pointer(new ConstantPointer());
			pointer->null_pointer = true;
			return FromPointerValue(pointer, type);
		}
		const PA19IntegralType integral = PA19Type(TypeText(type, true));
		return integral.integral ? FromIntegralValue(PA19Convert(
			PA19IntegralValue::Signed(0), integral)) : ConstantValue();
	}
	if (type->kind == TYPE_POINTER)
	{
		shared_ptr<ConstantPointer> pointer(new ConstantPointer());
		pointer->null_pointer = true;
		return FromPointerValue(pointer, type);
	}
	if (type->kind == TYPE_ARRAY)
	{
		if (type->bound < 0) return ConstantValue();
		shared_ptr<ConstantObject> object(new ConstantObject());
		object->type = type;
		for (long long i = 0; i < type->bound; ++i)
			object->elements.push_back(DefaultConstantValue(type->child, scope));
		return FromObjectValue(type, object);
	}
	if (type->kind != TYPE_CLASS) return ConstantValue();
	shared_ptr<ConstantObject> object(new ConstantObject());
	object->type = type;
	if (type->direct_base)
	{
		ConstantValue base = DefaultConstantValue(type->direct_base, scope);
		if (base.object)
		{
			for (map<string, ConstantValue>::const_iterator member = base.object->members.begin();
				member != base.object->members.end(); ++member)
				object->members[member->first] = member->second;
		}
	}
	for (size_t i = 0; i < type->class_members.size(); ++i)
	{
		const ClassMemberInfo& member = type->class_members[i];
		if (member.is_static || member.name.empty()) continue;
		ConstantValue value;
		if (member.initializer)
			value = EvaluateTyped(ConstantInitializer(member.initializer), scope, member.type);
		if (!ConstantKnown(value)) value = DefaultConstantValue(member.type, scope);
		object->members[member.name] = value;
	}
	return FromObjectValue(type, object);
}

ConstantValue Analyzer::ConvertConstantValue(const ConstantValue& value,
	const TypePtr& raw_target, Scope* scope)
{
	TypePtr target = UnwrapConstantType(raw_target);
	if (!target || !ConstantKnown(value)) return ConstantValue();
	if (target->kind == TYPE_FUNDAMENTAL)
	{
		if (target->name == "float" || target->name == "double" || target->name == "long double")
			return FromFloatingValue(NumericValue(value), target);
		if (target->name == "bool" && value.kind == ConstantValue::CONSTANT_OBJECT && value.object)
		{
			for (TypePtr current = value.object->type; current; current = current->direct_base)
			{
				Scope* owner = ScopeForType(current);
				if (!owner) continue;
				for (size_t i = 0; i < owner->bindings.size(); ++i)
				{
					Binding& conversion = owner->bindings[i];
					if (conversion.kind != BIND_FUNCTION || !conversion.type ||
						conversion.type->kind != TYPE_FUNCTION || !conversion.type->child ||
						!(conversion.name == "operator bool" || conversion.name == "operatorbool" ||
							(conversion.type->child->kind == TYPE_FUNDAMENTAL &&
								 conversion.type->child->name == "bool"))) continue;
					CPPGMAstNodePtr synthetic(new CPPGMAstNode("call-expression"));
					synthetic->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
						"id-expression", conversion.name)));
					synthetic->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
						"argument-list")));
					ConstantValue converted = EvaluateFunctionCall(&conversion, synthetic, scope, value,
						 target);
					if (ConstantKnown(converted)) return ConvertConstantValue(converted, target, scope);
				}
			}
		}
		if (value.integral.known)
		{
			const PA19IntegralType integral = PA19Type(TypeText(target, true));
			return integral.integral ? FromIntegralValue(PA19Convert(value.integral, integral)) : ConstantValue();
		}
		if (value.floating_known)
			return FromIntegralValue(PA19IntegralValue::Signed(static_cast<long long>(value.floating),
				TypeText(target, true), PA19Type(TypeText(target, true)).bits));
		if (value.kind == ConstantValue::CONSTANT_POINTER)
			return FromIntegralValue(PA19IntegralValue::Signed(
				value.pointer && !value.pointer->null_pointer, "bool", 1));
		return ConstantValue();
	}
	if (target->kind == TYPE_POINTER && value.kind == ConstantValue::CONSTANT_POINTER)
	{
		ConstantValue result = value;
		result.type = target;
		return result;
	}
	if (target->kind == TYPE_ARRAY && value.kind == ConstantValue::CONSTANT_OBJECT)
	{
		ConstantValue result = value;
		result.type = target;
		if (result.object) result.object->type = target;
		return result;
	}
	if (target->kind == TYPE_CLASS && value.kind == ConstantValue::CONSTANT_OBJECT)
	{
		if (value.object->type && SameTypeIgnoringTopCv(value.object->type, target))
		{
			ConstantValue result = value;
			result.type = target;
			result.object->type = target;
			return result;
		}
		vector<ConstantValue> arguments;
		arguments.push_back(value);
		return EvaluateConstructor(target, arguments, scope);
	}
	return value;
}

ConstantValue Analyzer::EvaluateConstructor(const TypePtr& raw_type,
	const vector<ConstantValue>& arguments, Scope* caller_scope,
	const CPPGMAstNodePtr& call)
{
	TypePtr type = UnwrapConstantType(raw_type);
	if (!type || type->kind != TYPE_CLASS) return ConstantValue();
	ConstantValue initial = DefaultConstantValue(type, caller_scope);
	if (!initial.object) return initial;
	Scope* class_scope = ScopeForType(type);
	Binding* selected = 0;
	const string constructor_name = LastComponent(type->name);
	if (class_scope)
	{
		for (size_t i = 0; i < class_scope->bindings.size(); ++i)
		{
			Binding& candidate = class_scope->bindings[i];
			if (candidate.kind != BIND_FUNCTION || candidate.name != constructor_name ||
				!candidate.type || candidate.type->kind != TYPE_FUNCTION) continue;
			if (candidate.type->parameters.size() == arguments.size())
			{
				selected = &candidate;
				break;
			}
		}
	}
	if (!selected)
	{
		// A class without a user-declared constructor is an aggregate here.
		// Constructor arguments and braced elements follow declaration order.
		size_t argument = 0;
		for (size_t i = 0; i < type->class_members.size() && argument < arguments.size(); ++i)
		{
			const ClassMemberInfo& member = type->class_members[i];
			if (member.is_static || member.name.empty()) continue;
			initial.object->members[member.name] = ConvertConstantValue(arguments[argument++],
				member.type, caller_scope);
		}
		return initial;
	}
	if (HasNodeValue(selected->declaration, "special-initializer", "default")) return initial;
	CPPGMAstNodePtr declaration = selected->declaration;
	CPPGMAstNodePtr clause = FunctionParameterClause(declaration);
	constant_frames_.push_back(map<string, ConstantValue>());
	constant_pack_frames_.push_back(map<string, vector<ConstantValue> >());
	constant_receivers_.push_back(initial);
	if (clause)
	{
		size_t argument = 0;
		for (size_t i = 0; i < clause->children.size(); ++i)
		{
			CPPGMAstNodePtr parameter = clause->children[i];
			if (!parameter || parameter->kind != "parameter-declaration") continue;
			const string name = parameter->children.size() > 1 ?
				FirstIdentifier(parameter->children[1]) : string();
			if (name.empty()) continue;
			ConstantValue value = argument < arguments.size() ? arguments[argument++] :
				DefaultConstantValue(i < selected->type->parameters.size() ?
					selected->type->parameters[i] : TypePtr(), caller_scope);
			if (i < selected->type->parameters.size()) value = ConvertConstantValue(value,
				selected->type->parameters[i], caller_scope);
			constant_frames_.back()[name] = value;
		}
	}
	CPPGMAstNodePtr initializer = ChildOfKind(declaration, "ctor-initializer");
	if (initializer)
	{
		for (size_t i = 0; i < initializer->children.size(); ++i)
		{
			CPPGMAstNodePtr mem = initializer->children[i];
			if (!mem || mem->kind != "mem-initializer" || mem->children.empty()) continue;
			const string name = mem->children[0]->value;
			CPPGMAstNodePtr arguments_node = mem->children.size() > 1 ? mem->children[1] : CPPGMAstNodePtr();
			vector<ConstantValue> values;
			if (arguments_node)
				for (size_t argument = 0; argument < arguments_node->children.size(); ++argument)
					values.push_back(Evaluate(arguments_node->children[argument], caller_scope));
			if (type->direct_base && LastComponent(type->direct_base->name) == name)
			{
				ConstantValue base = values.size() == 1 ? ConvertConstantValue(values[0],
					type->direct_base, caller_scope) : EvaluateConstructor(type->direct_base, values, caller_scope);
				if (base.object)
					for (map<string, ConstantValue>::const_iterator member = base.object->members.begin();
						member != base.object->members.end(); ++member)
						initial.object->members[member->first] = member->second;
				continue;
			}
			for (size_t member = 0; member < type->class_members.size(); ++member)
				if (!type->class_members[member].is_static && type->class_members[member].name == name)
				{
					const TypePtr member_type = type->class_members[member].type;
					ConstantValue value = values.empty() ? DefaultConstantValue(member_type, caller_scope) :
						(member_type && member_type->kind == TYPE_CLASS ?
							EvaluateConstructor(member_type, values, caller_scope) :
							ConvertConstantValue(values[0], member_type, caller_scope));
					initial.object->members[name] = value;
				}
		}
	}
	CPPGMAstNodePtr body = FunctionBody(declaration);
	if (body)
	{
		Scope* body_scope = caller_scope;
		map<const CPPGMAstNode*, Scope*>::const_iterator found = compound_scopes_.find(body.get());
		if (found != compound_scopes_.end()) body_scope = found->second;
		EvaluateCompound(body, body_scope);
	}
	constant_receivers_.pop_back();
	constant_frames_.pop_back();
	constant_pack_frames_.pop_back();
	return initial;
}

ConstantValue Analyzer::EvaluateMemberValue(const CPPGMAstNodePtr& expression, Scope* scope)
{
	if (!expression || expression->children.size() < 2) return ConstantValue();
	CPPGMAstNodePtr base_node = expression->children[0];
	const string member_name = expression->children[1]->value;
	ConstantValue base;
	TypePtr object_type;
	if (base_node && base_node->kind == "id-expression")
	{
		Binding* named = ResolveBinding(scope, base_node->value);
		if (named && (named->kind == BIND_TYPE || named->kind == BIND_TYPE_ALIAS))
			object_type = named->type;
	}
	if (!object_type)
	{
		base = Evaluate(base_node, scope);
		if (base.object) object_type = base.object->type;
		else if (base.type) object_type = base.type;
		if (!object_type && base_node && base_node->kind == "id-expression" &&
			base_node->value.compare(0, 9, "decltype(") == 0)
		{
			const size_t open = base_node->value.find('(');
			const size_t close = base_node->value.rfind(')');
			if (open != string::npos && close != string::npos && close > open + 1)
			{
				string inner = base_node->value.substr(open + 1, close - open - 1);
				if (inner.size() >= 2 && inner.substr(inner.size() - 2) == "()")
					inner.erase(inner.size() - 2);
				Binding* named = ResolveBinding(scope, inner);
				if (named && (named->kind == BIND_TYPE || named->kind == BIND_TYPE_ALIAS))
					object_type = named->type;
			}
		}
		if (!object_type) object_type = ExpressionType(base_node, scope);
	}
	object_type = UnwrapConstantType(object_type);
	if (!object_type || object_type->kind != TYPE_CLASS) return ConstantValue();
	if (base.object)
	{
		map<string, ConstantValue>::const_iterator value = base.object->members.find(member_name);
		if (value != base.object->members.end()) return value->second;
	}
	for (TypePtr current = object_type; current; current = current->direct_base)
	{
		Scope* owner = ScopeForType(current);
		if (!owner) continue;
		Binding* binding = owner->local(member_name);
		if (!binding || binding->kind != BIND_VARIABLE) continue;
		if (!binding->is_static && base.object)
		{
			map<string, ConstantValue>::const_iterator value = base.object->members.find(member_name);
			if (value != base.object->members.end()) return value->second;
		}
		map<const Binding*, ConstantValue>::const_iterator stored = constant_binding_values_.find(binding);
		if (stored != constant_binding_values_.end()) return stored->second;
		CPPGMAstNodePtr declaration = binding->declaration;
		if (declaration)
		{
			CPPGMAstNodePtr list = ChildOfKind(declaration, "init-declarator-list");
			if (list)
				for (size_t i = 0; i < list->children.size(); ++i)
				{
					CPPGMAstNodePtr item = list->children[i];
					if (!item || item->children.empty() ||
						FirstIdentifier(item->children[0]) != member_name || item->children.size() < 2) continue;
					ConstantValue value = EvaluateTyped(ConstantInitializer(item->children[1]), scope,
						binding->type);
					if (ConstantKnown(value)) constant_binding_values_[binding] = value;
					return value;
				}
		}
	}
	return ConstantValue();
}

ConstantValue Analyzer::EvaluateMemberCall(const CPPGMAstNodePtr& call, Scope* scope)
{
	if (!call || call->children.empty() || !call->children[0] ||
		call->children[0]->kind != "member-expression") return ConstantValue();
	CPPGMAstNodePtr member = call->children[0];
	if (member->children.size() < 2) return ConstantValue();
	ConstantValue receiver = Evaluate(member->children[0], scope);
	TypePtr object_type = receiver.object ? receiver.object->type : receiver.type;
	object_type = UnwrapConstantType(object_type);
	if (!object_type || object_type->kind != TYPE_CLASS) return ConstantValue();
	const string name = member->children[1]->value;
	const size_t arity = call->children.size() > 1 ? call->children[1]->children.size() : 0;
	for (TypePtr current = object_type; current; current = current->direct_base)
	{
		Scope* owner = ScopeForType(current);
		if (!owner) continue;
		for (size_t i = 0; i < owner->bindings.size(); ++i)
		{
			Binding& candidate = owner->bindings[i];
			if (candidate.kind != BIND_FUNCTION || candidate.name != name || !candidate.type ||
				candidate.type->kind != TYPE_FUNCTION || candidate.type->parameters.size() != arity) continue;
			return EvaluateFunctionCall(&candidate, call, scope,
				candidate.is_static ? ConstantValue() : receiver);
		}
	}
	return ConstantValue();
}

ConstantValue Analyzer::EvaluateTyped(const CPPGMAstNodePtr& expression, Scope* scope,
	const TypePtr& raw_expected_type)
{
	if (!expression) return DefaultConstantValue(raw_expected_type, scope);
	TypePtr expected = UnwrapConstantType(raw_expected_type);
	if (expression->kind == "initializer" || expression->kind == "default-argument" ||
		expression->kind == "initializer-clause")
		return expression->children.empty() ? DefaultConstantValue(expected, scope) :
			EvaluateTyped(expression->children[0], scope, expected);
	if (expression->kind == "literal" && !expression->value.empty() &&
		(expression->value[0] == '"' || expression->value[0] == 'u' ||
		 expression->value[0] == 'U' || expression->value[0] == 'L') &&
		(expression->value.find('"') != string::npos))
	{
		vector<unsigned long long> characters = DecodeString(expression->value);
		TypePtr element = expected && expected->kind == TYPE_POINTER ? expected->child :
			(expected && expected->kind == TYPE_ARRAY ? expected->child : Fundamental("char"));
		TypePtr array_type = ArrayOf(static_cast<long long>(characters.size() + 1), element);
		shared_ptr<ConstantObject> object(new ConstantObject());
		object->type = array_type;
		for (size_t i = 0; i < characters.size(); ++i)
			object->elements.push_back(FromIntegralValue(PA19IntegralValue::Signed(
				static_cast<long long>(characters[i]), TypeText(element, true),
				PA19Type(TypeText(element, true)).bits)));
		object->elements.push_back(DefaultConstantValue(element, scope));
		if (expected && expected->kind == TYPE_POINTER)
		{
			shared_ptr<ConstantPointer> pointer(new ConstantPointer());
			pointer->object = object;
			return FromPointerValue(pointer, expected);
		}
		return FromObjectValue(expected && expected->kind == TYPE_ARRAY ? expected : array_type, object);
	}
	if (expected && expected->kind == TYPE_ARRAY && expression->kind == "braced-init-list")
	{
		shared_ptr<ConstantObject> object(new ConstantObject());
		object->type = expected;
		for (size_t i = 0; i < expression->children.size(); ++i)
			object->elements.push_back(EvaluateTyped(expression->children[i], scope, expected->child));
		return FromObjectValue(expected, object);
	}
	if (expected && expected->kind == TYPE_CLASS &&
		(expression->kind == "braced-init-list" || expression->kind == "paren-initializer"))
	{
		vector<ConstantValue> arguments;
		bool has_user_constructor = false;
		Scope* class_scope = ScopeForType(expected);
		const string constructor_name = LastComponent(expected->name);
		if (class_scope)
			for (size_t i = 0; i < class_scope->bindings.size(); ++i)
				if (class_scope->bindings[i].kind == BIND_FUNCTION &&
					class_scope->bindings[i].name == constructor_name)
					has_user_constructor = true;
		size_t member = 0;
		for (size_t i = 0; i < expression->children.size(); ++i)
		{
			while (member < expected->class_members.size() &&
				(expected->class_members[member].is_static ||
				 expected->class_members[member].name.empty())) ++member;
			if (!has_user_constructor && member < expected->class_members.size())
				arguments.push_back(EvaluateTyped(expression->children[i], scope,
					expected->class_members[member++].type));
			else arguments.push_back(Evaluate(expression->children[i], scope));
		}
		return EvaluateConstructor(expected, arguments, scope);
	}
	if (expected && expected->kind == TYPE_POINTER && expression->kind == "id-expression")
	{
		Binding* binding = ResolveBinding(scope, expression->value);
		if (binding && binding->kind == BIND_VARIABLE && binding->type)
		{
			TypePtr source = UnwrapConstantType(binding->type);
			if (source && source->kind == TYPE_ARRAY)
			{
				shared_ptr<ConstantObject> object;
				map<const Binding*, ConstantValue>::const_iterator stored =
					constant_binding_values_.find(binding);
				if (stored != constant_binding_values_.end() && stored->second.object)
					object = stored->second.object;
				if (!object) object = DefaultConstantValue(source, scope).object;
				if (object)
				{
					shared_ptr<ConstantPointer> pointer(new ConstantPointer());
					pointer->object = object;
					return FromPointerValue(pointer, expected);
				}
			}
		}
	}
	ConstantValue value = Evaluate(expression, scope);
	return expected ? ConvertConstantValue(value, expected, scope) : value;
}

ConstantValue Analyzer::Evaluate(const CPPGMAstNodePtr& expression, Scope* scope)
{
	if (!expression) return ConstantValue();
	if (expression->kind == "literal")
	{
		if (!expression->value.empty() && (expression->value[0] == '"' ||
			(expression->value.size() > 1 && (expression->value[0] == 'u' ||
			 expression->value[0] == 'U' || expression->value[0] == 'L') &&
			 expression->value.find('"') != string::npos)))
			return EvaluateTyped(expression, scope, TypePtr());
		if (IsFloatingLiteral(expression->value))
		{
			TypePtr type;
			return FromFloatingValue(ParseFloatingLiteral(expression->value, &type), type);
		}
		return FromIntegralValue(ParseLiteralValue(expression->value));
	}
	if (expression->kind == "keyword-literal")
	{
		const string op = OperatorFromNode(expression->value);
		if (op == "true" || op == "false")
			return FromIntegralValue(PA19IntegralValue::Signed(op == "true", "bool", 1));
		if (op == "nullptr")
		{
			shared_ptr<ConstantPointer> pointer(new ConstantPointer());
			pointer->null_pointer = true;
			return FromPointerValue(pointer, Fundamental("nullptr_t"));
		}
		return ConstantValue();
	}
	if (expression->kind == "id-expression")
	{
		ConstantValue frame_value;
		if (ConstantFrameValue(expression->value, &frame_value)) return frame_value;
		// A qualified namespace variable is a complete typed lookup path.
		if (expression->value.find("::") != string::npos)
		{
			Binding* qualified = ResolveBinding(scope, expression->value);
			if (qualified)
			{
				map<const Binding*, ConstantValue>::const_iterator stored =
					constant_binding_values_.find(qualified);
				if (stored != constant_binding_values_.end()) return stored->second;
				if (qualified->constant_value.known)
					return FromIntegralValue(qualified->constant_value);
			}
		}
		if (expression->value.find("::") != string::npos)
		{
			const size_t separator = expression->value.rfind("::");
			CPPGMAstNodePtr qualified_member(new CPPGMAstNode("member-expression"));
			qualified_member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
				"id-expression", expression->value.substr(0, separator))));
			qualified_member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
				"identifier", expression->value.substr(separator + 2))));
			ConstantValue member = EvaluateMemberValue(qualified_member, scope);
			if (ConstantKnown(member)) return member;
		}
		Binding* binding = ResolveBinding(scope, expression->value);
		map<const Binding*, ConstantValue>::const_iterator stored =
			constant_binding_values_.find(binding);
		if (stored != constant_binding_values_.end()) return stored->second;
		if (binding && binding->is_member && !binding->is_static && !constant_receivers_.empty())
		{
			const ConstantValue& receiver = constant_receivers_.back();
			if (receiver.object)
			{
				map<string, ConstantValue>::const_iterator member = receiver.object->members.find(binding->name);
				if (member != receiver.object->members.end()) return member->second;
			}
		}
		return !binding || !binding->constant_value.known ? ConstantValue() :
			FromIntegralValue(binding->constant_value);
	}
	if (expression->kind == "parenthesized-expression" || expression->kind == "initializer" ||
		expression->kind == "paren-initializer" || expression->kind == "initializer-clause" ||
		expression->kind == "default-argument")
		return expression->children.empty() ? ConstantValue() : Evaluate(expression->children[0], scope);
	if (expression->kind == "condition")
		return expression->children.empty() ? ConstantValue() : Evaluate(expression->children[0], scope);
	if (expression->kind == "braced-init-list" && expression->children.size() == 1)
		return Evaluate(expression->children[0], scope);
	if (expression->kind == "member-expression") return EvaluateMemberValue(expression, scope);
	if (expression->kind == "subscript-expression" && expression->children.size() >= 2)
	{
		ConstantValue base = Evaluate(expression->children[0], scope);
		ConstantValue index = Evaluate(expression->children[1], scope);
		if (!index.integral.known) return ConstantValue();
		const long long position = PA19Signed(index.integral);
		if (base.object && position >= 0 &&
			static_cast<size_t>(position) < base.object->elements.size())
			return base.object->elements[static_cast<size_t>(position)];
		if (base.kind == ConstantValue::CONSTANT_POINTER && base.pointer && base.pointer->object)
		{
			const long long offset = base.pointer->index + position;
			if (offset >= 0 && static_cast<size_t>(offset) < base.pointer->object->elements.size())
				return base.pointer->object->elements[static_cast<size_t>(offset)];
		}
		return ConstantValue();
	}
	if (expression->kind == "sizeof-pack-expression")
		return FromIntegralValue(PA19IntegralValue::Unsigned(
			static_cast<unsigned long long>(expression->value.empty() ? 0 : atoll(expression->value.c_str())),
			"unsigned long", 64));
	if (expression->kind == "sizeof-expression" || expression->kind == "type-trait-expression")
	{
		if (expression->children.empty()) return ConstantValue();
		if (expression->kind == "type-trait-expression" &&
			expression->value.find("NOEXCEPT") != string::npos)
			return FromIntegralValue(PA19IntegralValue::Signed(
				NoexceptCall(*this, expression->children[0], scope) ? 1 : 0, "bool", 1));
		TypePtr type = expression->children[0]->kind == "type-id" ?
			TypeFromTypeId(expression->children[0], scope) : ExpressionType(expression->children[0], scope);
		const bool alignment = expression->kind == "type-trait-expression";
		return FromIntegralValue(PA19IntegralValue::Unsigned(
			static_cast<unsigned long long>(alignment ? TypeAlignment(type) : TypeSize(type)),
			"unsigned long", 64));
	}
	if (expression->kind == "cast-expression")
	{
		if (expression->children.size() < 2) return ConstantValue();
		ConstantValue operand = Evaluate(expression->children[1], scope);
		TypePtr target = TypeFromTypeId(expression->children[0], scope);
		if (operand.floating_known && target && target->kind == TYPE_FUNDAMENTAL &&
			(target->name == "float" || target->name == "double" || target->name == "long double"))
			return FromFloatingValue(operand.floating, target);
		if (operand.integral.known && target && target->kind == TYPE_FUNDAMENTAL &&
			(target->name == "float" || target->name == "double" || target->name == "long double"))
			return FromFloatingValue(static_cast<long double>(PA19Signed(operand.integral)), target);
		if (!operand.integral.known) return ConstantValue();
		return FromIntegralValue(PA19Convert(operand.integral,
			PA19Type(TypeText(target, true))));
	}
	if (expression->kind == "call-expression" && !expression->children.empty())
	{
		if (expression->children[0] && expression->children[0]->kind == "member-expression")
			return EvaluateMemberCall(expression, scope);
		if (expression->children[0] && expression->children[0]->kind == "call-expression")
		{
			ConstantValue receiver = Evaluate(expression->children[0], scope);
			TypePtr object_type = receiver.object ? receiver.object->type : receiver.type;
			object_type = UnwrapConstantType(object_type);
			const size_t arity = expression->children.size() > 1 ?
				expression->children[1]->children.size() : 0;
			for (TypePtr current = object_type; current; current = current->direct_base)
			{
				Scope* owner = ScopeForType(current);
				if (!owner) continue;
				for (size_t i = 0; i < owner->bindings.size(); ++i)
				{
					Binding& candidate = owner->bindings[i];
					if (candidate.kind == BIND_FUNCTION && candidate.name == "operator()" &&
						candidate.type && candidate.type->kind == TYPE_FUNCTION &&
						candidate.type->parameters.size() == arity)
						return EvaluateFunctionCall(&candidate, expression, scope, receiver);
				}
			}
			return ConstantValue();
		}
		if (!expression->children[0] || expression->children[0]->kind != "id-expression")
			return ConstantValue();
		const string name = expression->children[0]->value;
		const PA19IntegralType cast_type = PA19Type(name);
		CPPGMAstNodePtr arguments = expression->children.size() > 1 ? expression->children[1] : CPPGMAstNodePtr();
		if (cast_type.integral && arguments && arguments->children.size() == 1)
		{
			ConstantValue operand = Evaluate(arguments->children[0], scope);
			return operand.integral.known ? FromIntegralValue(PA19Convert(operand.integral, cast_type)) :
				ConstantValue();
		}
		if ((name == "float" || name == "double" || name == "long double") &&
			arguments && arguments->children.size() == 1)
		{
			ConstantValue operand = Evaluate(arguments->children[0], scope);
			if (!ConstantKnown(operand)) return ConstantValue();
			return FromFloatingValue(NumericValue(operand), Fundamental(name));
		}
		string lookup_name = name;
		const size_t template_open = lookup_name.find('<');
		if (template_open != string::npos) lookup_name.erase(template_open);
		Binding* function = ResolveBinding(scope, lookup_name);
		const size_t argument_count = arguments ? arguments->children.size() : 0;
		Binding* constant_function = FindConstantFunction(lookup_name, scope, argument_count);
		const size_t generated_marker = lookup_name.find("__inst_");
		if (generated_marker != string::npos)
		{
			Binding* template_function = FindConstantFunction(lookup_name.substr(0, generated_marker),
				scope, argument_count);
			if (template_function) constant_function = template_function;
		}
		if (constant_function && (!function || function->kind != BIND_FUNCTION ||
			!FunctionAcceptsArity(*function, argument_count))) function = constant_function;
		if (function && (function->kind == BIND_TYPE || function->kind == BIND_TYPE_ALIAS) &&
			function->type && function->type->kind == TYPE_CLASS)
		{
			vector<ConstantValue> values;
			if (arguments)
				for (size_t i = 0; i < arguments->children.size(); ++i)
					if (!(arguments->children[i] && arguments->children[i]->kind == "braced-init-list" &&
						arguments->children[i]->children.empty()))
						values.push_back(Evaluate(arguments->children[i], scope));
			return EvaluateConstructor(function->type, values, scope, expression);
		}
		if (function && (function->kind == BIND_TYPE || function->kind == BIND_TYPE_ALIAS) &&
			function->type && arguments && arguments->children.size() == 1)
			return ConvertConstantValue(Evaluate(arguments->children[0], scope),
				function->type, scope);
		if (function && function->kind == BIND_FUNCTION)
		{
			if (function->is_member && function->member_owner &&
				function->member_owner->kind == TYPE_CLASS &&
				LastComponent(function->member_owner->name) == function->name)
			{
				vector<ConstantValue> values;
				if (arguments)
					for (size_t i = 0; i < arguments->children.size(); ++i)
						values.push_back(Evaluate(arguments->children[i], scope));
				return EvaluateConstructor(function->member_owner, values, scope, expression);
			}
			return EvaluateFunctionCall(function, expression, scope);
		}
	}
	if (expression->kind == "unary-expression")
	{
		if (expression->children.empty()) return ConstantValue();
		ConstantValue child = Evaluate(expression->children[0], scope);
		if (child.floating_known)
		{
			const string op = OperatorFromNode(expression->value);
			if (op == "+") return child;
			if (op == "-") return FromFloatingValue(-child.floating, child.type);
			if (op == "!") return FromIntegralValue(PA19IntegralValue::Signed(
				child.floating == 0, "int", 32));
			return ConstantValue();
		}
		if (!child.integral.known) return child;
		PA19IntegralValue value = child.integral;
		const string op = OperatorFromNode(expression->value);
		if (op == "+") return FromIntegralValue(PA19Promote(value));
		if (op == "-")
		{
			value = PA19Promote(value);
			const PA19IntegralType type = value.type;
			const unsigned long long raw = (0ULL - PA19Raw(value)) & PA19Mask(type.bits);
			return FromIntegralValue(type.is_unsigned ? PA19IntegralValue::Unsigned(raw, type.name, type.bits) :
				PA19IntegralValue::Signed(static_cast<long long>(raw), type.name, type.bits));
		}
		if (op == "!") return FromIntegralValue(PA19IntegralValue::Signed(!PA19Raw(value), "int", 32));
		if (op == "~")
		{
			value = PA19Promote(value);
			const PA19IntegralType type = value.type;
			const unsigned long long raw = (~PA19Raw(value)) & PA19Mask(type.bits);
			return FromIntegralValue(type.is_unsigned ? PA19IntegralValue::Unsigned(raw, type.name, type.bits) :
				PA19IntegralValue::Signed(static_cast<long long>(raw), type.name, type.bits));
		}
		return ConstantValue();
	}
	if (expression->kind == "conditional-expression" && expression->children.size() == 3)
	{
		ConstantValue condition = Evaluate(expression->children[0], scope);
		return !ConstantKnown(condition) ? ConstantValue() :
			Evaluate(expression->children[ConstantTrue(condition) ? 1 : 2], scope);
	}
	if (expression->kind == "assignment-expression" && expression->children.size() >= 2)
	{
		const string operation = OperatorFromNode(expression->value);
		ConstantValue right = Evaluate(expression->children[1], scope);
		if (operation != "=")
		{
			ConstantValue left = Evaluate(expression->children[0], scope);
			if (left.floating_known || right.floating_known)
			{
				if (!ConstantKnown(left) || !ConstantKnown(right)) return ConstantValue();
				const long double l = NumericValue(left), r = NumericValue(right);
				long double result = 0;
				const string op = AssignmentOperator(operation);
				if (op == "+") result = l + r;
				else if (op == "-") result = l - r;
				else if (op == "*") result = l * r;
				else if (op == "/") { if (r == 0) return ConstantValue(); result = l / r; }
				else return ConstantValue();
				right = FromFloatingValue(result, left.type);
			}
			else right = left.integral.known && right.integral.known ?
				FromIntegralValue(PA19Binary(AssignmentOperator(operation), left.integral, right.integral)) :
				ConstantValue();
		}
		if (expression->children[0]->kind == "id-expression")
			SetConstantFrameValue(expression->children[0]->value, right);
		return right;
	}
	if (expression->kind == "binary-expression" && expression->children.size() >= 2)
	{
		const string operation = OperatorFromNode(expression->value);
		ConstantValue left = Evaluate(expression->children[0], scope);
		if (!ConstantKnown(left)) return ConstantValue();
		if (operation == "&&" || operation == "and")
			return !ConstantTrue(left) ?
				FromIntegralValue(PA19IntegralValue::Signed(0, "int", 32)) :
				Evaluate(expression->children[1], scope);
		if (operation == "||" || operation == "or")
			return ConstantTrue(left) ?
				FromIntegralValue(PA19IntegralValue::Signed(1, "int", 32)) :
				Evaluate(expression->children[1], scope);
		ConstantValue right = Evaluate(expression->children[1], scope);
		if (!ConstantKnown(right)) return ConstantValue();
		if (left.kind == ConstantValue::CONSTANT_POINTER ||
			right.kind == ConstantValue::CONSTANT_POINTER)
		{
			if (operation == "==" || operation == "!=")
			{
				const bool left_null = left.kind == ConstantValue::CONSTANT_POINTER &&
					(!left.pointer || left.pointer->null_pointer);
				const bool right_null = right.kind == ConstantValue::CONSTANT_POINTER &&
					(!right.pointer || right.pointer->null_pointer);
				bool equal = left_null && right_null;
				if (!left_null && !right_null && left.pointer && right.pointer)
					equal = left.pointer->object == right.pointer->object &&
						left.pointer->index == right.pointer->index;
				return FromIntegralValue(PA19IntegralValue::Signed(
					operation == "==" ? equal : !equal, "bool", 1));
			}
			return ConstantValue();
		}
		if ((left.kind == ConstantValue::CONSTANT_OBJECT || right.kind == ConstantValue::CONSTANT_OBJECT) &&
			(operation == "==" || operation == "!=" || operation == "<" || operation == ">" ||
			 operation == "<=" || operation == ">="))
		{
			const string operator_name = "operator" + operation;
			TypePtr candidates[2] = { left.object ? left.object->type : TypePtr(),
				right.object ? right.object->type : TypePtr() };
			for (size_t owner_index = 0; owner_index < 2; ++owner_index)
				for (TypePtr current = candidates[owner_index]; current; current = current->direct_base)
				{
					Scope* owner = ScopeForType(current);
					if (!owner) continue;
					for (size_t i = 0; i < owner->bindings.size(); ++i)
					{
						Binding& candidate = owner->bindings[i];
						if (candidate.kind != BIND_FUNCTION || candidate.name != operator_name ||
							!candidate.type || candidate.type->kind != TYPE_FUNCTION ||
							candidate.type->parameters.size() != 2) continue;
						CPPGMAstNodePtr synthetic(new CPPGMAstNode("call-expression"));
						synthetic->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
							"id-expression", candidate.name)));
						CPPGMAstNodePtr arguments(new CPPGMAstNode("argument-list"));
						arguments->children.push_back(expression->children[0]);
						arguments->children.push_back(expression->children[1]);
						synthetic->children.push_back(arguments);
						return EvaluateFunctionCall(&candidate, synthetic, scope);
					}
				}
			// Non-member overloaded operators are associated with the argument
			// class but remain declared in the surrounding namespace.  Materialized
			// operator templates therefore need the namespace lookup path as well
			// as the class-member path above.
			Binding* namespace_candidate = FindConstantFunction(operator_name, scope, 2);
			if (namespace_candidate && !namespace_candidate->is_member &&
				namespace_candidate->type && namespace_candidate->type->kind == TYPE_FUNCTION &&
				namespace_candidate->type->parameters.size() == 2)
			{
				CPPGMAstNodePtr synthetic(new CPPGMAstNode("call-expression"));
				synthetic->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
					"id-expression", namespace_candidate->name)));
				CPPGMAstNodePtr arguments(new CPPGMAstNode("argument-list"));
				arguments->children.push_back(expression->children[0]);
				arguments->children.push_back(expression->children[1]);
				synthetic->children.push_back(arguments);
				return EvaluateFunctionCall(namespace_candidate, synthetic, scope);
			}
		}
		if (left.floating_known || right.floating_known)
		{
			const long double l = NumericValue(left), r = NumericValue(right);
			if (operation == "+") return FromFloatingValue(l + r, left.type);
			if (operation == "-") return FromFloatingValue(l - r, left.type);
			if (operation == "*") return FromFloatingValue(l * r, left.type);
			if (operation == "/") return r == 0 ? ConstantValue() : FromFloatingValue(l / r, left.type);
			if (operation == "==") return FromIntegralValue(PA19IntegralValue::Signed(l == r, "bool", 1));
			if (operation == "!=") return FromIntegralValue(PA19IntegralValue::Signed(l != r, "bool", 1));
			if (operation == "<") return FromIntegralValue(PA19IntegralValue::Signed(l < r, "bool", 1));
			if (operation == ">") return FromIntegralValue(PA19IntegralValue::Signed(l > r, "bool", 1));
			if (operation == "<=") return FromIntegralValue(PA19IntegralValue::Signed(l <= r, "bool", 1));
			if (operation == ">=") return FromIntegralValue(PA19IntegralValue::Signed(l >= r, "bool", 1));
			return ConstantValue();
		}
		const PA19IntegralValue binary = PA19Binary(operation, left.integral, right.integral);
		ConstantValue binary_value = FromIntegralValue(binary);
		return binary_value;
	}
	return ConstantValue();
}
