#include "pa11_semantics_analyzer.h"

#include <functional>

using namespace std;

TypePtr Analyzer::ExpressionCallType(const CPPGMAstNodePtr& expression,
	Scope* scope, size_t arity)
{
	if (!expression || expression->children.empty()) return TypePtr();
	const string spelled_name = expression->children[0]->value;
	// The parser keeps an explicitly specialized function template as one
	// id-expression (`test<T>`).  Binding lookup and the constant-template
	// registry are keyed by the callable's unqualified name, so remove only
	// the template argument suffix before selecting its overload.
	const size_t template_open = spelled_name.find('<');
	const string name = template_open == string::npos ? spelled_name :
		spelled_name.substr(0, template_open);
	// A class functional cast is represented by the parser as a call whose
	// callee is an id-expression.  Resolve its complete template-id before
	// looking for callable overloads so overload ranking sees the actual class
	// object (and its conversion operators), rather than the integer fallback.
	try {
		TypePtr constructed = ResolveType(scope, spelled_name);
		if(constructed && constructed->kind == TYPE_CLASS) return constructed;
	} catch (const logic_error&) {}
	vector<Binding*> candidates;
	for (Scope* current = scope; current; current = current->parent) {
		vector<Binding*> local;
		for (size_t i = 0; i < current->bindings.size(); ++i)
			if (current->bindings[i].name == name && current->bindings[i].kind == BIND_FUNCTION)
				local.push_back(&current->bindings[i]);
		if (!local.empty()) { candidates = local; break; }
	}
	map<string, vector<Binding*> >::const_iterator templates =
		constant_template_functions_.find(LastComponent(name));
	if (templates != constant_template_functions_.end())
		for (size_t i = 0; i < templates->second.size(); ++i)
			if (find(candidates.begin(), candidates.end(), templates->second[i]) == candidates.end())
				candidates.push_back(templates->second[i]);
	Binding* selected = 0;
	int selected_score = 1000000;
	const auto replace_template_identifiers = [](string raw,
		const map<string, string>& substitutions) {
		string result;
		for (size_t position = 0; position < raw.size();) {
			if (!(isalnum(static_cast<unsigned char>(raw[position])) ||
				raw[position] == '_')) {
				result += raw[position++];
				continue;
			}
			size_t end = position + 1;
			while (end < raw.size() && (isalnum(static_cast<unsigned char>(raw[end])) ||
				raw[end] == '_')) ++end;
			const string word = raw.substr(position, end - position);
			map<string, string>::const_iterator substitution = substitutions.find(word);
			result += substitution == substitutions.end() ? word : substitution->second;
			position = end;
		}
		return result;
	};
	const auto template_arguments = [](const string& raw, size_t open) {
		vector<string> result;
		if (open == string::npos || open >= raw.size() || raw[open] != '<') return result;
		int depth = 0;
		size_t close = string::npos;
		for (size_t position = open; position < raw.size(); ++position) {
			if (raw[position] == '<') ++depth;
			else if (raw[position] == '>' && --depth == 0) {
				close = position;
				break;
			}
		}
		if (close == string::npos) return result;
		string current;
		depth = 0;
		for (size_t position = open + 1; position < close; ++position) {
			const char character = raw[position];
			if (character == '<') ++depth;
			else if (character == '>') --depth;
			if (character == ',' && depth == 0) {
				while (!current.empty() && isspace(static_cast<unsigned char>(current[0])))
					current.erase(0, 1);
				while (!current.empty() && isspace(static_cast<unsigned char>(current[current.size() - 1])))
					current.erase(current.size() - 1);
				result.push_back(current);
				current.clear();
			} else current += character;
		}
		while (!current.empty() && isspace(static_cast<unsigned char>(current[0])))
			current.erase(0, 1);
		while (!current.empty() && isspace(static_cast<unsigned char>(current[current.size() - 1])))
			current.erase(current.size() - 1);
		if (!current.empty()) result.push_back(current);
		return result;
	};
	const auto specialization_substitutions = [this](TypePtr current) {
		map<string, string> result;
		if (!current || current->template_primary.empty() ||
			current->template_arguments.empty()) return result;
		Scope* lookup = ScopeForType(current);
		TypePtr primary;
		try { primary = ResolveType(lookup, current->template_primary); }
		catch (const logic_error&) {}
		if (!primary) return result;
		if (!primary->template_parameter_names.empty())
			for (size_t argument = 0; argument < primary->template_parameter_names.size() &&
				argument < current->template_arguments.size(); ++argument)
				result[primary->template_parameter_names[argument]] =
					current->template_arguments[argument];
		else if (primary->owned_scope && primary->owned_scope->parent) {
			size_t argument = 0;
			for (size_t binding = 0; binding < primary->owned_scope->parent->bindings.size() &&
				argument < current->template_arguments.size(); ++binding) {
				const Binding& parameter = primary->owned_scope->parent->bindings[binding];
				if (!parameter.type || (parameter.type->kind != TYPE_TEMPLATE_PARAMETER &&
					parameter.type->kind != TYPE_TEMPLATE_TEMPLATE_PARAMETER)) continue;
				result[parameter.name] = current->template_arguments[argument++];
			}
		}
		return result;
	};
	const auto unqualified_type_name = [](string raw) {
		while (raw.compare(0, 7, "struct ") == 0) raw.erase(0, 7);
		while (raw.compare(0, 6, "class ") == 0) raw.erase(0, 6);
		while (raw.compare(0, 6, "union ") == 0) raw.erase(0, 6);
		return raw;
	};
	const auto resolve_spelled = [this](string raw, TypePtr current) {
		Scope* lookup = ScopeForType(current);
		if (!lookup) lookup = global_.get();
		SpecFacts facts;
		try {
			TypePtr local = ResolveSpelledType(raw, lookup, facts);
			// A materialized dependent class retains the primary's conservative
			// lookup context.  Once its template identifiers have been rebound,
			// retry qualified names outside that dependent scope rather than
			// preserving a synthetic template-parameter type.
			if (local && local->kind != TYPE_TEMPLATE_PARAMETER &&
				local->kind != TYPE_TEMPLATE_TEMPLATE_PARAMETER) return local;
		}
		catch (const logic_error&) {
		}
		try {
			SpecFacts fallback_facts;
			return ResolveSpelledType(raw, global_.get(), fallback_facts);
		} catch (const logic_error&) { return TypePtr(); }
	};
	const auto convertible_types = [this](TypePtr source, TypePtr target) {
		while (source && (source->kind == TYPE_LVALUE_REFERENCE ||
			source->kind == TYPE_RVALUE_REFERENCE)) source = source->child;
		while (target && (target->kind == TYPE_LVALUE_REFERENCE ||
			target->kind == TYPE_RVALUE_REFERENCE)) target = target->child;
		if (!source || !target) return false;
		if (SameTypeIgnoringTopCv(source, target)) return true;
		if (source->kind == TYPE_POINTER && target->kind == TYPE_POINTER &&
			source->child && target->child) {
			if (target->child->kind == TYPE_FUNDAMENTAL && target->child->name == "void") return true;
			if (SameTypeIgnoringTopCv(source->child, target->child)) return true;
			if (source->child->kind == TYPE_CLASS && target->child->kind == TYPE_CLASS) {
				const vector<TypePtr> bases = BaseTypeClosure(source->child);
				for (size_t base = 1; base < bases.size(); ++base)
					if (SameTypeIgnoringTopCv(bases[base], target->child)) return true;
			}
			return false;
		}
		return source->kind == TYPE_FUNDAMENTAL && target->kind == TYPE_FUNDAMENTAL;
	};
	const auto concrete_boolean_arguments = [&](string raw, TypePtr current) {
		map<string, string> substitutions = specialization_substitutions(current);
		string result = replace_template_identifiers(raw, substitutions);
		for (size_t search = 0;;) {
			const string marker = "is_convertible_impl<";
			const size_t begin = result.find(marker, search);
			if (begin == string::npos) break;
			const size_t open = begin + marker.size() - 1;
			int depth = 0;
			size_t close = string::npos;
			for (size_t position = open; position < result.size(); ++position) {
				if (result[position] == '<') ++depth;
				else if (result[position] == '>' && --depth == 0) {
					close = position;
					break;
				}
			}
			if (close == string::npos || result.compare(close + 1, 7, "::value") != 0) break;
			const vector<string> arguments = template_arguments(result, open);
			if (arguments.size() != 2) { search = close + 1; continue; }
			TypePtr source = resolve_spelled(arguments[0], current);
			TypePtr target = resolve_spelled(arguments[1], current);
			if (!source || !target) { search = close + 1; continue; }
			const string replacement = convertible_types(source, target) ? "true" : "false";
			result.replace(begin, close + 1 + 7 - begin, replacement);
			search = begin + replacement.size();
		}
		return result;
	};
	const auto rebound_spelling = [&](string raw, TypePtr current) {
		return concrete_boolean_arguments(unqualified_type_name(
			replace_template_identifiers(StripTypeMarker(raw),
				specialization_substitutions(current))), current);
	};
	const function<TypePtr(const TypePtr&, TypePtr)> rebound_type =
		[&](const TypePtr& original, TypePtr current) {
		if (!original) return TypePtr();
		if (original->kind == TYPE_LVALUE_REFERENCE || original->kind == TYPE_RVALUE_REFERENCE) {
			TypePtr child = rebound_type(original->child, current);
			return child ? ReferenceTo(original->kind, child) : original;
		}
		if (original->kind == TYPE_POINTER) {
			TypePtr child = rebound_type(original->child, current);
			return child ? PointerTo(child) : original;
		}
		if (original->kind != TYPE_CLASS && original->kind != TYPE_TEMPLATE_PARAMETER) return original;
		TypePtr resolved = resolve_spelled(rebound_spelling(original->name, current), current);
		if (!resolved) return original;
		if (original->is_const || original->is_volatile)
			resolved = CloneWithCv(resolved, original->is_const, original->is_volatile);
		return resolved;
	};
	const auto has_class_conversion = [&](TypePtr source, TypePtr target) {
		while (target && (target->kind == TYPE_LVALUE_REFERENCE ||
			 target->kind == TYPE_RVALUE_REFERENCE)) target = target->child;
		const bool source_const = source && source->is_const;
		const bool source_volatile = source && source->is_volatile;
		vector<TypePtr> pending;
		set<const Type*> visited;
		int best_rank = 1000000;
		if (source) pending.push_back(source);
		while (!pending.empty()) {
			TypePtr current = pending.back();
			pending.pop_back();
			while (current && (current->kind == TYPE_LVALUE_REFERENCE ||
				current->kind == TYPE_RVALUE_REFERENCE)) current = current->child;
			if (!current || !visited.insert(current.get()).second) continue;
			Scope* owner = ScopeForType(current);
			if (owner) for (size_t member = 0; member < owner->bindings.size(); ++member) {
				const Binding& candidate = owner->bindings[member];
				if (candidate.kind != BIND_FUNCTION || !candidate.type ||
					candidate.type->kind != TYPE_FUNCTION || !candidate.type->child ||
					candidate.name.compare(0, 8, "operator") != 0) continue;
				if ((source_const && !candidate.type->function_const) ||
					(source_volatile && !candidate.type->function_volatile)) continue;
				TypePtr converted = rebound_type(candidate.type->child, current);
				while (converted && (converted->kind == TYPE_LVALUE_REFERENCE ||
					converted->kind == TYPE_RVALUE_REFERENCE)) converted = converted->child;
				// Conversion-function declarations have no written return type;
				// the parser preserves their target in the binding name while the
				// ordinary function type carries `void`.  Resolve that target in the
				// specialized owner's typed substitution frame.
				if (candidate.name.compare(0, 8, "operator") == 0 &&
					converted && converted->kind == TYPE_FUNDAMENTAL &&
					converted->name == "void")
					converted = resolve_spelled(rebound_spelling(candidate.name.substr(8), current), current);
				while (converted && (converted->kind == TYPE_LVALUE_REFERENCE ||
					converted->kind == TYPE_RVALUE_REFERENCE)) converted = converted->child;
				if (SameTypeIgnoringTopCv(converted, target)) {
					// For a non-const object, a non-const conversion member is the
					// better user-defined conversion when both conversion targets can
					// bind to the same const reference parameter.
					const int rank = (!source_const && candidate.type->function_const) ? 1 : 0;
					if (rank < best_rank) best_rank = rank;
				}
			}
			vector<TypePtr> bases = current->direct_bases;
			if (bases.empty() && current->direct_base) bases.push_back(current->direct_base);
			for (size_t base = 0; base < bases.size(); ++base) {
				TypePtr rebound = rebound_type(bases[base], current);
				pending.push_back(rebound ? rebound : bases[base]);
			}
		}
		return best_rank == 1000000 ? -1 : best_rank;
	};
	for (size_t i = 0; i < candidates.size(); ++i) {
		Binding* candidate = candidates[i];
		if (!candidate->type || candidate->type->kind != TYPE_FUNCTION) continue;
		const TypePtr function = candidate->type;
		if ((!function->variadic && function->parameters.size() != arity) ||
			(function->variadic && function->parameters.size() > arity)) continue;
		int score = function->variadic ? 4 : 0;
		bool viable = true;
		for (size_t argument = 0; argument < arity; ++argument) {
			if (argument >= function->parameters.size()) { score += 8; continue; }
			TypePtr actual = ExpressionType(expression->children[1]->children[argument], scope);
			TypePtr formal = function->parameters[argument];
			while (formal && (formal->kind == TYPE_LVALUE_REFERENCE || formal->kind == TYPE_RVALUE_REFERENCE)) formal = formal->child;
			while (actual && (actual->kind == TYPE_LVALUE_REFERENCE || actual->kind == TYPE_RVALUE_REFERENCE)) actual = actual->child;
			const CPPGMAstNodePtr actual_expression = expression->children[1]->children[argument];
			const bool null_pointer_constant = IsNullPointerConstantExpression(actual_expression, scope);
			if (formal && formal->kind == TYPE_TEMPLATE_PARAMETER) score += 10;
			else if (SameTypeIgnoringTopCv(actual, formal)) {} else if (null_pointer_constant && formal && formal->kind == TYPE_POINTER) score += 3;
			else if (actual && formal && actual->kind == TYPE_FUNDAMENTAL && formal->kind == TYPE_FUNDAMENTAL) score += 2;
			else {
				const int conversion_rank = has_class_conversion(actual, formal);
				if (conversion_rank >= 0) score += 5 + conversion_rank;
				else { viable = false; break; }
			}
		}
		if (viable && (!selected || score < selected_score)) {
			selected = candidate;
			selected_score = score;
		}
	}
	return selected ? selected->type->child : TypePtr();
}
