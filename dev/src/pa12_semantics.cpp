#include "pa12_semantics_support.h"
class PA12Printer
{
public:
	explicit PA12Printer(Analyzer& analyzer)
		: a_(analyzer), out_(), constructed_types_(), display_type_names_(),
		  local_union_count_(0), anonymous_union_count_(0), anonymous_enum_count_(0) {}
	void Print(const CPPGMAstNodePtr& tree, ostream& out)
	{
		out_ = &out;
		*out_ << "translation-unit\n";
		for (size_t i = 0; i < tree->children.size(); ++i)
			PrintTop(tree->children[i], a_.global_.get(), 1, string());
		for (size_t i = 0; i < constructed_types_.size(); ++i)
			PrintSyntheticConstructor(constructed_types_[i], 1);
	}
private:
	Analyzer& a_;
	ostream* out_;
	vector<TypePtr> constructed_types_;
	map<const Type*, string> display_type_names_;
	unsigned int local_union_count_;
	unsigned int anonymous_union_count_;
	unsigned int anonymous_enum_count_;
	void Indent(unsigned int indentation)
	{
		Analyzer::Indent(*out_, indentation);
	}
	string TypeTextPA12(const TypePtr& type)
	{
		return TypeText(DisplayType(type), true);
	}
	string DisplayName(const TypePtr& type)
	{
		if (!type) return string();
		map<const Type*, string>::const_iterator found = display_type_names_.find(type.get());
		if (found != display_type_names_.end()) return found->second;
		if (type->kind == TYPE_ENUM && type->name.find("__anonymous_enum") == 0)
		{
			ostringstream generated; generated << "__anonymous_enum" << ++anonymous_enum_count_;
			return display_type_names_[type.get()] = generated.str();
		}
		return type->name;
	}
	TypePtr DisplayType(const TypePtr& type)
	{
		if (!type) return type;
		TypePtr result = PA12AdjustedType(type);
		if ((type->kind == TYPE_ENUM && type->name.find("__anonymous_enum") == 0) ||
			(type->kind == TYPE_CLASS && type->tag == "union" &&
			 type->name.find("__anonymous_union_type__") == 0))
		{
			result.reset(new Type(*result));
			result->name = DisplayName(type);
		}
		if (result->kind == TYPE_FUNCTION) { for (size_t i = 0; i < result->parameters.size(); ++i) result->parameters[i] = DisplayType(type->parameters[i]); result->child = DisplayType(type->child); }
		else if (result->kind == TYPE_POINTER || result->kind == TYPE_LVALUE_REFERENCE || result->kind == TYPE_RVALUE_REFERENCE || result->kind == TYPE_ARRAY) result->child = DisplayType(type->child);
		else if (result->kind == TYPE_MEMBER_POINTER) { result->member_owner = DisplayType(type->member_owner); result->child = DisplayType(type->child); }
		return result;
	}
	void AppendBindings(Scope* scope, const string& name, vector<Binding*>& result,
		set<Scope*>& visited)
	{
		if (!scope || !visited.insert(scope).second) return;
		for (size_t i = 0; i < scope->bindings.size(); ++i)
			if (scope->bindings[i].name == name) result.push_back(&scope->bindings[i]);
		for (size_t i = 0; i < scope->using_directives.size(); ++i)
			AppendBindings(scope->using_directives[i], name, result, visited);
	}
	vector<Binding*> DirectBindings(Scope* scope, const string& name)
	{
		vector<Binding*> result;
		if (!scope) return result;
		for (size_t i = 0; i < scope->bindings.size(); ++i)
			if (scope->bindings[i].name == name) result.push_back(&scope->bindings[i]);
		return result;
	}
	vector<Binding*> LookupUnqualifiedAll(Scope* from, const string& name)
	{
		for (Scope* current = from; current != 0; current = current->parent)
		{
			vector<Binding*> direct = DirectBindings(current, name);
			if (!direct.empty()) return direct;
			vector<Binding*> imported;
			set<Scope*> visited;
			for (size_t i = 0; i < current->using_directives.size(); ++i)
				AppendBindings(current->using_directives[i], name, imported, visited);
			if (!imported.empty()) return imported;
		}
		return vector<Binding*>();
	}
	Scope* ScopeComponent(Scope* current, const string& component, bool first,
		bool absolute)
	{
		Scope* namespace_scope = (first && !absolute) ?
			a_.FindNamespace(current, component) :
			a_.FindNamespaceDirect(current, component);
		if (namespace_scope) return namespace_scope;
		vector<Binding*> bindings = (first && !absolute) ?
			LookupUnqualifiedAll(current, component) : DirectBindings(current, component);
		for (size_t i = 0; i < bindings.size(); ++i)
		{
			if (bindings[i]->kind == BIND_TYPE || bindings[i]->kind == BIND_TYPE_ALIAS)
				return a_.ScopeForType(bindings[i]->type);
		}
		return 0;
	}
	vector<Binding*> Lookup(const string& raw, Scope* from)
	{
		bool absolute = false;
		const vector<string> parts = a_.SplitPath(raw, &absolute);
		if (parts.empty()) return vector<Binding*>(); if (parts.size() == 1 && !absolute)
			return LookupUnqualifiedAll(from, parts[0]);
		Scope* current = absolute ? a_.global_.get() : from;
		for (size_t i = 0; i + 1 < parts.size(); ++i)
		{
			current = ScopeComponent(current, parts[i], i == 0, absolute);
			if (!current) return vector<Binding*>();
		}
		vector<Binding*> result;
		set<Scope*> visited;
		AppendBindings(current, parts.back(), result, visited);
		return result;
	}
	TypePtr FindType(Scope* scope, const string& raw)
	{
		const string name = raw.find("TT_IDENTIFIER:") == 0 ?
			raw.substr(string("TT_IDENTIFIER:").size()) : raw;
		if (name == "bool" || name == "char" || name == "char16_t" ||
			name == "char32_t" || name == "double" || name == "float" ||
			name == "int" || name == "long" || name == "short" ||
			name == "signed" || name == "unsigned" || name == "void" ||
			name == "wchar_t" || name == "nullptr_t") return Fundamental(name);
		return a_.ResolveType(scope, name);
	}
	TypePtr DeclaredType(const CPPGMAstNodePtr& node, Scope* scope,
		Analyzer::SpecFacts* facts = 0)
	{
		if (!node || node->children.empty()) throw logic_error("invalid declaration");
		Analyzer::SpecFacts local;
		Analyzer::SpecFacts& info = facts ? *facts : local;
		TypePtr base = a_.TypeFromSpecSeq(node->children[0], scope, &info);
		if (node->children.size() > 1 && node->children[1] &&
			(node->children[1]->kind == "declarator" ||
			 node->children[1]->kind == "abstract-declarator"))
			base = a_.BuildDeclarator(node->children[1], base, scope);
		return base;
	}
	string DeclaratorName(const CPPGMAstNodePtr& node) const
	{
		return FirstIdentifier(node);
	}
	TypePtr FunctionType(const TypePtr& type) const
	{
		TypePtr value = PA12AdjustedType(PA12ValueType(type));
		if (value && value->kind == TYPE_FUNCTION) return value;
		if (value && value->kind == TYPE_POINTER && value->child &&
			value->child->kind == TYPE_FUNCTION) return value->child;
		return TypePtr();
	}
	TypePtr QualifiedMemberFunctionType(const string& raw_name,
		const TypePtr& raw_type, Scope* scope)
	{
		TypePtr function = FunctionType(raw_type);
		const size_t separator = raw_name.rfind("::");
		if (!function || separator == string::npos) return TypePtr();
		TypePtr owner;
		try { owner = FindType(scope, raw_name.substr(0, separator)); }
		catch (const exception&) { return TypePtr(); }
		if (!owner || owner->kind != TYPE_CLASS) return TypePtr();
		vector<TypePtr> parameters;
		TypePtr this_type = function->function_const ? CloneWithCv(owner, true, false) : owner;
		parameters.push_back(PointerTo(this_type));
		parameters.insert(parameters.end(), function->parameters.begin(), function->parameters.end());
		return FunctionOf(parameters, function->variadic, function->child, false);
	}
	TypePtr FunctionalCastType(const CPPGMAstNodePtr& callee, Scope* scope)
	{
		if (!callee) return TypePtr(); if (callee->kind == "id-expression" && IsTypeName(scope, callee->value))
			return FindType(scope, callee->value);
		if (callee->kind == "decltype-specifier") return a_.TypeFromDecltype(callee, scope);
		return TypePtr();
	}
	TypePtr ExpressionValueType(const PA12ExprInfo& info) const
	{
		return PA12ValueType(info.type);
	}
	bool IsTypeName(Scope* scope, const string& name)
	{
		if (name == "bool" || name == "char" || name == "char16_t" ||
			name == "char32_t" || name == "double" || name == "float" ||
			name == "int" || name == "long" || name == "short" ||
			name == "signed" || name == "unsigned" || name == "void" ||
			name == "wchar_t" || name == "nullptr_t") return true;
		try { return static_cast<bool>(FindType(scope, name)); }
		catch (const exception&) { return false; }
	}
	long long IntegerValue(const string& raw, bool* valid = 0) const
	{
		string value = raw;
		while (!value.empty() && (value[value.size() - 1] == 'u' ||
			value[value.size() - 1] == 'U' || value[value.size() - 1] == 'l' ||
			value[value.size() - 1] == 'L')) value.erase(value.size() - 1);
		if (value.empty()) { if (valid) *valid = false; return 0; }
		char* end = 0;
		errno = 0;
		const long long result = strtoll(value.c_str(), &end, 0);
		const bool okay = errno != ERANGE && end != value.c_str() && *end == '\0';
		if (valid) *valid = okay;
		return okay ? result : 0;
	}
	TypePtr LiteralType(const string& raw, long long* value = 0,
		bool* known = 0) const
	{
		if (value) *value = 0;
		if (known) *known = false;
		if (raw.empty()) return Fundamental("int");
		string number = raw;
		const bool quoted = raw[0] == '"' || raw[0] == '\'' ||
			(raw.size() > 1 && (raw[0] == 'u' || raw[0] == 'U' || raw[0] == 'L') &&
			(raw[1] == '"' || raw[1] == '\''));
		if (quoted)
		{
			if (raw.find('"') != string::npos)
			{
				string prefix;
				if (raw[0] == 'u' || raw[0] == 'U' || raw[0] == 'L') prefix = raw.substr(0, 1);
				string element = prefix == "u" ? "char16_t" :
					prefix == "U" ? "char32_t" : prefix == "L" ? "wchar_t" : "char";
				long long length = 0;
				bool escaped = false;
				for (size_t i = prefix.size() + 1; i + 1 < raw.size(); ++i)
				{
					if (escaped) { escaped = false; continue; }
					if (raw[i] == '\\') { escaped = true; continue; }
					++length;
				}
				return ArrayOf(length + 1, CloneWithCv(Fundamental(element), true, false));
			}
			return Fundamental("char");
		}
		const bool hexadecimal = raw.size() > 2 && raw[0] == '0' &&
			(raw[1] == 'x' || raw[1] == 'X');
		const bool floating = raw.find('.') != string::npos ||
			(!hexadecimal && (raw.find('e') != string::npos || raw.find('E') != string::npos)) ||
			(hexadecimal && (raw.find('.') != string::npos || raw.find('p') != string::npos || raw.find('P') != string::npos));
		if (floating)
		{
			const char suffix = raw[raw.size() - 1];
			return suffix == 'f' || suffix == 'F' ? Fundamental("float") :
				suffix == 'l' || suffix == 'L' ? Fundamental("long double") : Fundamental("double");
		}
		bool unsigned_value = false;
		for (size_t i = 0; i < raw.size(); ++i)
			if (raw[i] == 'u' || raw[i] == 'U') unsigned_value = true;
		unsigned int long_count = 0;
		for (size_t i = 0; i < raw.size(); ++i)
			if (raw[i] == 'l' || raw[i] == 'L') ++long_count;
		bool okay = false;
		const long long parsed = IntegerValue(raw, &okay);
		if (value) *value = parsed;
		if (known) *known = okay; if (long_count >= 2) return Fundamental(unsigned_value ? "unsigned long long int" : "long long int");
		if (long_count == 1) return Fundamental(unsigned_value ? "unsigned long int" : "long int"); return Fundamental(unsigned_value ? "unsigned int" : "int");
	}
	bool IsConstCompatible(const TypePtr& source, const TypePtr& target) const
	{
		if (!source || !target || source->kind != target->kind) return false; if (target->is_const == false && source->is_const) return false;
		if (target->is_volatile == false && source->is_volatile) return false;
		return true;
	}
	bool PointerCompatible(const TypePtr& source, const TypePtr& target) const
	{
		if (!source || !target || source->kind != TYPE_POINTER || target->kind != TYPE_POINTER)
			return false;
		TypePtr s = source->child;
		TypePtr t = target->child;
		if (PA12SameType(s, t, true)) return IsConstCompatible(s, t); if (t && t->kind == TYPE_FUNDAMENTAL && t->name == "void")
			return !s || (s->kind != TYPE_FUNCTION && s->kind != TYPE_MEMBER_POINTER) &&
				(!s || (!s->is_const || t->is_const) && (!s->is_volatile || t->is_volatile));
		return false;
	}
	TypePtr IntegralPromotion(const TypePtr& type) const
	{
		TypePtr value = PA12ValueType(type);
		if (value && value->kind == TYPE_ENUM && !value->scoped_enum) return Fundamental("int"); if (!value || value->kind != TYPE_FUNDAMENTAL) return value;
		if (value->name == "bool" || value->name == "char" || value->name == "signed char" ||
			value->name == "unsigned char" || value->name == "short" ||
			value->name == "short int" || value->name == "unsigned short" ||
			value->name == "unsigned short int" || value->name == "char16_t")
			return Fundamental("int");
		return value;
	}
	int ConversionRank(const PA12ExprInfo& source, const TypePtr& target) const
	{
		if (!target || !source.type) return -1;
		TypePtr source_value = PA12ValueType(source.type);
		TypePtr target_value = PA12ValueType(target);
		if (!source_value || !target_value) return -1;
		if (target->kind == TYPE_LVALUE_REFERENCE || target->kind == TYPE_RVALUE_REFERENCE)
		{
			if (target->kind == TYPE_LVALUE_REFERENCE)
			{
				if (source.category == "lvalue")
				{
					if (!target_value->is_const && source_value->is_const) return -1;
					if (!target_value->is_volatile && source_value->is_volatile) return -1;
					if (PA12SameType(source_value, target_value, true) &&
						IsConstCompatible(source_value, target_value))
						return PA12SameType(source_value, target_value, false) ? 0 : 1;
					if (source_value->kind == TYPE_POINTER && target_value->kind == TYPE_POINTER &&
						PointerCompatible(source_value, target_value)) return 1;
					return -1;
				}
				if (target_value->is_const &&
					(PA12SameType(source_value, target_value, true) ||
					 (source_value->kind == TYPE_POINTER && target_value->kind == TYPE_POINTER &&
					  PointerCompatible(source_value, target_value)))) return 2;
				return -1;
			}
			if (source.category == "lvalue")
			{
				if (source_value->kind == TYPE_FUNCTION && target_value->kind == TYPE_FUNCTION &&
					PA12SameType(source_value, target_value, true)) return 1;
				if (PA12IsArithmetic(source_value) && PA12IsArithmetic(target_value)) return 2;
				return -1;
			}
			if (PA12SameType(source_value, target_value, true) &&
				(!source_value->is_const || target_value->is_const))
				return source.category == "xvalue" ? 0 : 1;
			if (source_value->kind == TYPE_FUNCTION && target_value->kind == TYPE_FUNCTION &&
				PA12SameType(source_value, target_value, true)) return 0;
			return -1;
		}
		if (target_value->kind == TYPE_POINTER)
		{
			if (source.null_pointer_constant || source_value->kind == TYPE_FUNDAMENTAL &&
				source_value->name == "nullptr_t") return 2;
			if (source_value->kind == TYPE_ARRAY &&
				PA12SameType(source_value->child, target_value->child, true)) return 0;
			if (source_value->kind == TYPE_FUNCTION && target_value->child &&
				target_value->child->kind == TYPE_FUNCTION &&
				PA12SameType(source_value, target_value->child, true)) return 0;
				if (source_value->kind == TYPE_POINTER)
				{
					if (PA12SameType(source_value, target_value, false)) return 0;
					if (PointerCompatible(source_value, target_value)) return 1;
			}
			return -1;
		}
		if (target_value->kind == TYPE_FUNDAMENTAL && target_value->name == "nullptr_t")
			return source.null_pointer_constant || source_value->name == "nullptr_t" ?
				(source_value->name == "nullptr_t" ? 0 : 2) : -1;
		if (target_value->kind == TYPE_FUNDAMENTAL && target_value->name == "bool" &&
			source_value->kind == TYPE_POINTER) return 3;
		if (PA12SameType(source_value, target_value, false)) return 0;
		if (PA12SameType(source_value, target_value, true)) return 1;
		if (PA12IsArithmetic(source_value) && PA12IsArithmetic(target_value))
		{
			TypePtr promoted = IntegralPromotion(source_value);
			if (PA12SameType(promoted, target_value, true)) return 1;
			return 2;
		}
		if (source_value->kind == TYPE_FUNCTION && target_value->kind == TYPE_FUNCTION &&
			PA12SameType(source_value, target_value, true)) return 0;
		if (source_value->kind == target_value->kind && source_value->kind == TYPE_CLASS &&
			PA12SameType(source_value, target_value, true)) return 0;
		return -1;
	}
	TypePtr CommonType(const TypePtr& left, const TypePtr& right,
		const string& op = string()) const
	{
		TypePtr l = PA12ValueType(left);
		TypePtr r = PA12ValueType(right);
		if (!l || !r) return Fundamental("int");
		if (PA12SameType(l, r, true))
		{
			if (l->kind == TYPE_POINTER && r->kind == TYPE_POINTER &&
				(!l->child->is_const && r->child->is_const)) return r;
			if (l->kind == TYPE_POINTER && l->child && r->child &&
				(l->child->is_const || r->child->is_const))
			{
				TypePtr result(new Type(*l));
				result->child = CloneWithCv(l->child, l->child->is_const || r->child->is_const,
					l->child->is_volatile || r->child->is_volatile);
				return result;
			}
			if (l->is_const || l->is_volatile)
			{
				TypePtr result(new Type(*l));
				result->is_const = false;
				result->is_volatile = false;
				return result;
			}
			return l;
		}
		if (l->kind == TYPE_POINTER && r->kind == TYPE_POINTER)
		{
			if (PointerCompatible(l, r)) return r;
			if (PointerCompatible(r, l)) return l;
		}
		if (l->kind == TYPE_POINTER && r->kind == TYPE_FUNDAMENTAL && r->name == "nullptr_t") return l;
		if (r->kind == TYPE_POINTER && l->kind == TYPE_FUNDAMENTAL && l->name == "nullptr_t") return r;
		if (PA12IsArithmetic(l) && PA12IsArithmetic(r))
		{
			if (l->name == "long double" || r->name == "long double") return Fundamental("long double");
			if (l->name == "double" || r->name == "double") return Fundamental("double");
			if (l->name == "float" || r->name == "float") return Fundamental("float");
			TypePtr lp = IntegralPromotion(l), rp = IntegralPromotion(r);
			if (lp->name == "unsigned int" && rp->name == "int") return lp;
			if (rp->name == "unsigned int" && lp->name == "int") return rp;
			if (lp->name == "long int" || rp->name == "long int" ||
				lp->name == "unsigned long int" || rp->name == "unsigned long int")
				return Fundamental((lp->name.find("unsigned") != string::npos ||
					rp->name.find("unsigned") != string::npos) ? "unsigned long int" : "long int");
			return Fundamental("int");
		}
		(void)op;
		return l;
	}
		PA12CallChoice ChooseCall(const string& raw_name, Scope* scope,
			const vector<PA12ExprInfo>& arguments)
		{
		PA12CallChoice best;
		const vector<Binding*> candidates = Lookup(raw_name, scope);
		bool found = false;
		for (size_t i = 0; i < candidates.size(); ++i)
		{
			Binding* binding = candidates[i];
			TypePtr function = FunctionType(binding->type);
			if (!function) continue;
			if (arguments.size() > function->parameters.size() && !function->variadic) continue;
			if (arguments.size() < function->parameters.size()) continue;
			int worst = 0;
			int total = 0;
			bool viable = true;
			for (size_t arg = 0; arg < arguments.size(); ++arg)
			{
				int rank = 100;
				if (arg < function->parameters.size()) rank = ConversionRank(arguments[arg], function->parameters[arg]);
				else if (!function->variadic) rank = -1;
				if (rank < 0) { viable = false; break; }
				worst = max(worst, rank);
				total += rank;
			}
			if (!viable) continue;
			if (!found || worst < best.worst || (worst == best.worst && total < best.total))
			{
				found = true;
				best.binding = binding;
				best.function = function;
				best.worst = worst;
				best.total = total;
			}
			else if (worst == best.worst && total == best.total)
			{
				if (!PA12SameType(best.function, function, false))
					throw logic_error("ambiguous overload");
			}
		}
		if (!found) throw logic_error("no viable function");
		return best;
	}
	bool DirectFunctionName(const CPPGMAstNodePtr& callee, Scope* scope)
	{
		if (!callee || callee->kind != "id-expression") return false;
		const vector<Binding*> bindings = Lookup(callee->value, scope);
		for (size_t i = 0; i < bindings.size(); ++i)
			if (bindings[i]->kind == BIND_FUNCTION && FunctionType(bindings[i]->type)) return true;
		return false;
	}
	Binding* ChooseFunctionForTarget(const vector<Binding*>& candidates,
		const TypePtr& expected)
	{
		Binding* result = 0;
		int best_rank = 1000000;
		for (size_t i = 0; i < candidates.size(); ++i)
		{
			TypePtr function = FunctionType(candidates[i]->type);
			if (!function) continue;
			PA12ExprInfo source;
			source.type = function;
			source.category = "lvalue";
			const int rank = ConversionRank(source, expected);
			if (rank >= 0 && rank < best_rank)
			{
				result = candidates[i];
				best_rank = rank;
			}
			else if (rank >= 0 && rank == best_rank)
				throw logic_error("ambiguous function target");
		}
		return result;
	}
	PA12ExprInfo InferLiteral(const CPPGMAstNodePtr& expression, const TypePtr& expected) {
		PA12ExprInfo result; long long value = 0; bool known = false;
		result.type = LiteralType(expression->value, &value, &known);
		result.category = result.type->kind == TYPE_ARRAY ? "lvalue" : "prvalue";
		result.null_pointer_constant = known && value == 0 &&
			result.type->kind == TYPE_FUNDAMENTAL && result.type->name != "float" &&
			result.type->name != "double" && result.type->name != "long double" &&
			result.type->name != "void" && result.type->name != "nullptr_t";
		result.known_constant = known; result.constant = value;
		if (expected && result.null_pointer_constant &&
			(expected->kind == TYPE_POINTER ||
			 (expected->kind == TYPE_FUNDAMENTAL && expected->name == "nullptr_t")))
			result.type = expected->kind == TYPE_FUNDAMENTAL && expected->name == "nullptr_t" ?
				Fundamental("nullptr_t") : expected;
		return result;
	}
	PA12ExprInfo InferKeyword(const CPPGMAstNodePtr& expression) {
		PA12ExprInfo result; const string op = PA12Operator(expression->value);
		if (op == "nullptr") result.type = Fundamental("nullptr_t");
		else if (op == "true" || op == "false") result.type = Fundamental("bool");
		else result.type = Fundamental("int");
		result.category = "prvalue"; return result;
	}
	PA12ExprInfo InferIdentifier(const CPPGMAstNodePtr& expression, Scope* scope, const TypePtr& expected) {
		PA12ExprInfo result; result.candidates = Lookup(expression->value, scope);
		if (expected && !result.candidates.empty())
		{
			Binding* target = ChooseFunctionForTarget(result.candidates, expected);
			if (target) result.binding = target;
		}
		if (result.binding) result.candidates.clear();
		if (result.candidates.empty() && !result.binding)
		{
			result.candidates = Lookup(expression->value, scope);
				if (result.candidates.empty()) throw logic_error("unknown expression name");
		}
		if (!result.binding && result.candidates.size() == 1) result.binding = result.candidates[0];
		if (result.binding && result.binding->kind == BIND_ENUMERATOR)
		{
			result.type = result.binding->type;
			result.category = "prvalue";
			result.known_constant = result.binding->has_value;
			result.constant = result.binding->value;
			return result;
		}
		if (result.binding)
		{
			result.type = PA12AdjustedType(result.binding->type);
			if (PA12IsReference(result.type)) result.type = result.type->child;
			result.category = "lvalue";
			return result;
		}
		if (!result.candidates.empty())
		{
			result.type = FunctionType(result.candidates[0]->type);
			if (!result.type) result.type = result.candidates[0]->type;
			result.category = "lvalue";
			return result;
		}
		throw logic_error("unresolved expression name");
	}
	TypePtr AddressOfType(const CPPGMAstNodePtr& child_expression, const PA12ExprInfo& child, Scope* scope) {
		if (child.binding && child_expression && child_expression->kind == "id-expression")
		{
			TypePtr member_function = QualifiedMemberFunctionType(child_expression->value,
				child.type, scope);
			if (member_function)
			{
				const size_t separator = child_expression->value.rfind("::");
				TypePtr owner = FindType(scope, child_expression->value.substr(0, separator));
				return MemberPointerTo(owner, FunctionType(child.type));
			}
		}
		return PointerTo(ExpressionValueType(child));
	}
	PA12ExprInfo InferCall(const CPPGMAstNodePtr& expression, Scope* scope) {
		PA12ExprInfo result; const CPPGMAstNodePtr callee_node = expression->children.empty() ?
			CPPGMAstNodePtr() : expression->children[0];
		const CPPGMAstNodePtr arguments_node = expression->children.size() > 1 ?
			expression->children[1] : CPPGMAstNodePtr();
		if (callee_node && callee_node->kind == "id-expression" &&
			callee_node->value == "__builtin_constant_p")
		{
			result.type = Fundamental("int");
			result.category = "prvalue";
			return result;
		}
		TypePtr functional_type = FunctionalCastType(callee_node, scope);
		if (functional_type)
		{
			result.type = functional_type;
			result.category = "prvalue";
			return result;
		}
		vector<PA12ExprInfo> arguments;
		if (arguments_node)
			for (size_t i = 0; i < arguments_node->children.size(); ++i)
				arguments.push_back(Infer(arguments_node->children[i], scope));
		PA12CallChoice choice;
		if (DirectFunctionName(callee_node, scope))
			choice = ChooseCall(callee_node->value, scope, arguments);
		else
		{
			PA12ExprInfo callee = Infer(callee_node, scope);
			choice.function = FunctionType(callee.type);
			if (!choice.function) throw logic_error("expression is not callable");
		}
		result.type = choice.function->child;
		if (result.type && result.type->kind == TYPE_LVALUE_REFERENCE) result.category = "lvalue";
		else if (result.type && result.type->kind == TYPE_RVALUE_REFERENCE) result.category = "xvalue";
		else result.category = "prvalue";
		return result;
	}
	PA12ExprInfo InferUnary(const CPPGMAstNodePtr& expression, Scope* scope) {
		PA12ExprInfo result; const string op = PA12Operator(expression->value);
		PA12ExprInfo child = Infer(expression->children[0], scope);
		TypePtr value = ExpressionValueType(child);
		if (op == "&") result.type = AddressOfType(expression->children[0], child, scope);
		else if (op == "*")
		{
			if (value && (value->kind == TYPE_POINTER || value->kind == TYPE_ARRAY))
				result.type = value->child;
			else throw logic_error("cannot dereference expression");
			result.category = "lvalue";
		}
		else if (op == "!") result.type = Fundamental("bool");
		else if (op == "++" || op == "--") result.type = value;
		else result.type = IntegralPromotion(value);
		if (op != "*") result.category = op == "++" || op == "--" ? "lvalue" : "prvalue";
		return result;
	}
	PA12ExprInfo InferPostfix(const CPPGMAstNodePtr& expression, Scope* scope) {
		PA12ExprInfo result; PA12ExprInfo child = Infer(expression->children[0], scope);
		result.type = ExpressionValueType(child); result.category = "prvalue"; return result;
	}
	PA12ExprInfo InferBinary(const CPPGMAstNodePtr& expression, Scope* scope) {
		PA12ExprInfo result; const string op = PA12Operator(expression->value);
		PA12ExprInfo left = Infer(expression->children[0], scope);
		PA12ExprInfo right = Infer(expression->children[1], scope);
		if (op == ",") { result.type = right.type; result.category = right.category; return result; }
		if (op == "&&" || op == "||" || op == "and" || op == "or" ||
			op == "==" || op == "!=" || op == "not_eq" || op == "<" || op == ">" ||
			op == "<=" || op == ">=") result.type = Fundamental("bool");
		else if (op == "-" && ExpressionValueType(left)->kind == TYPE_POINTER &&
			ExpressionValueType(right)->kind == TYPE_POINTER)
			result.type = Fundamental("long int");
		else if ((op == "+" || op == "-") && ExpressionValueType(left)->kind == TYPE_ARRAY)
			result.type = PointerTo(ExpressionValueType(left)->child);
		else if ((op == "+" || op == "-") && ExpressionValueType(left)->kind == TYPE_POINTER)
			result.type = ExpressionValueType(left);
		else if (op == "+" && (ExpressionValueType(right)->kind == TYPE_POINTER ||
			ExpressionValueType(right)->kind == TYPE_ARRAY))
			result.type = ExpressionValueType(right)->kind == TYPE_ARRAY ?
				PointerTo(ExpressionValueType(right)->child) : ExpressionValueType(right);
		else result.type = CommonType(left.type, right.type, op);
		result.category = "prvalue"; return result;
	}
	PA12ExprInfo InferAssignment(const CPPGMAstNodePtr& expression, Scope* scope) {
		PA12ExprInfo result; PA12ExprInfo left = Infer(expression->children[0], scope); PA12ExprInfo right = Infer(expression->children[1], scope);
		if (left.category != "lvalue") throw logic_error("assignment requires lvalue");
		if (PA12HasConst(left.type) || PA12HasVolatile(left.type) &&
			PA12Operator(expression->value) != "=")
			throw logic_error("assignment requires modifiable lvalue");
		result.type = ExpressionValueType(left); result.category = "lvalue";
		(void)right;
		return result;
	}
	PA12ExprInfo InferConditional(const CPPGMAstNodePtr& expression, Scope* scope) {
		PA12ExprInfo result; PA12ExprInfo when_true = Infer(expression->children[1], scope); PA12ExprInfo when_false = Infer(expression->children[2], scope);
		if (when_true.null_pointer_constant && ExpressionValueType(when_false) &&
			ExpressionValueType(when_false)->kind == TYPE_POINTER)
			result.type = ExpressionValueType(when_false);
		else if (when_false.null_pointer_constant && ExpressionValueType(when_true) &&
			ExpressionValueType(when_true)->kind == TYPE_POINTER)
			result.type = ExpressionValueType(when_true);
		else result.type = CommonType(when_true.type, when_false.type);
		result.category = PA12SameType(when_true.type, when_false.type, false) &&
			when_true.category == "lvalue" && when_false.category == "lvalue" ?
			"lvalue" : "prvalue";
		return result;
	}
	PA12ExprInfo InferSubscript(const CPPGMAstNodePtr& expression, Scope* scope) {
		PA12ExprInfo result; PA12ExprInfo base = Infer(expression->children[0], scope); TypePtr value = ExpressionValueType(base);
		if ((!value || (value->kind != TYPE_ARRAY && value->kind != TYPE_POINTER)) &&
			expression->children.size() > 1)
		{
			PA12ExprInfo index = Infer(expression->children[1], scope);
			TypePtr index_value = ExpressionValueType(index);
			if (index_value && (index_value->kind == TYPE_ARRAY ||
				index_value->kind == TYPE_POINTER)) value = index_value;
		}
		if (value && (value->kind == TYPE_ARRAY || value->kind == TYPE_POINTER))
			result.type = value->child;
		else throw logic_error("subscript requires array or pointer");
		result.category = "lvalue"; return result;
	}
	PA12ExprInfo InferCast(const CPPGMAstNodePtr& expression, Scope* scope) {
		PA12ExprInfo result; result.type = a_.TypeFromTypeId(expression->children[0], scope);
		result.category = PA12IsReference(result.type) ?
			(result.type->kind == TYPE_LVALUE_REFERENCE ? "lvalue" : "xvalue") : "prvalue";
		return result;
	}
	PA12ExprInfo InferTypeTrait() {
		PA12ExprInfo result; result.type = Fundamental("unsigned long int");
		result.category = "prvalue"; return result;
	}
	PA12ExprInfo InferMember(const CPPGMAstNodePtr& expression, Scope* scope) {
		PA12ExprInfo result; PA12ExprInfo base = Infer(expression->children[0], scope);
		TypePtr object = ExpressionValueType(base); const string op = PA12Operator(expression->value);
		if (op == "->" && object && object->kind == TYPE_POINTER) object = object->child;
		if (!object || object->kind != TYPE_CLASS || !object->owned_scope ||
			expression->children.size() < 2) throw logic_error("unknown member");
		const string member = expression->children[1]->value;
		vector<Binding*> fields = DirectBindings(object->owned_scope, PA12LastComponent(member));
		if (fields.empty()) throw logic_error("unknown member");
		result.binding = fields.back();
		result.type = result.binding->type; if (PA12IsReference(result.type)) result.type = result.type->child;
		if (object->is_const && result.type && result.binding->kind != BIND_FUNCTION)
			result.type = CloneWithCv(result.type, true, result.type->is_volatile);
		result.category = "lvalue"; return result;
	}
	PA12ExprInfo InferBraced(const CPPGMAstNodePtr&, const TypePtr& expected) {
		PA12ExprInfo result; result.type = expected ? expected : Fundamental("int");
		result.category = "lvalue"; return result;
	}
	PA12ExprInfo Infer(const CPPGMAstNodePtr& expression, Scope* scope, const TypePtr& expected = TypePtr()) {
		if (!expression) throw logic_error("missing expression");
		const string kind = expression->kind;
		if (kind == "literal") return InferLiteral(expression, expected);
		if (kind == "keyword-literal") return InferKeyword(expression);
		if (kind == "id-expression") return InferIdentifier(expression, scope, expected);
		if (kind == "parenthesized-expression")
			return expression->children.empty() ? PA12ExprInfo() :
				Infer(expression->children[0], scope, expected);
		if (kind == "call-expression") return InferCall(expression, scope);
		if (kind == "unary-expression") return InferUnary(expression, scope);
		if (kind == "postfix-expression") return InferPostfix(expression, scope);
		if (kind == "binary-expression") return InferBinary(expression, scope);
		if (kind == "assignment-expression") return InferAssignment(expression, scope);
		if (kind == "conditional-expression") return InferConditional(expression, scope);
		if (kind == "subscript-expression") return InferSubscript(expression, scope);
		if (kind == "cast-expression") return InferCast(expression, scope);
		if (kind == "sizeof-expression" || kind == "type-trait-expression")
			return InferTypeTrait();
		if (kind == "member-expression") return InferMember(expression, scope);
		if (kind == "braced-init-list") return InferBraced(expression, expected);
		throw logic_error("unsupported expression");
	}
	void ExprHeader(unsigned int indentation, const string& kind, const PA12ExprInfo& info,
		const string& value = string())
	{
		Indent(indentation);
		*out_ << kind << " " << info.category << " " << TypeTextPA12(info.type);
		if (!value.empty()) *out_ << " " << value;
		*out_ << "\n";
	}
	void PrintExpr(const CPPGMAstNodePtr& expression, Scope* scope,
		unsigned int indentation, const TypePtr& expected = TypePtr())
	{
		if (!expression) return; if (expression->kind == "parenthesized-expression")
		{
			if (!expression->children.empty()) PrintExpr(expression->children[0], scope, indentation, expected);
			return;
		}
		if (expression->kind == "literal" || expression->kind == "keyword-literal")
			return PrintLiteral(expression, scope, indentation, expected);
		if (expression->kind == "id-expression") return PrintIdentifier(expression, scope, indentation, expected);
		if (expression->kind == "call-expression") return PrintCall(expression, scope, indentation, expected);
		if (expression->kind == "unary-expression") return PrintUnary(expression, scope, indentation, expected);
		if (expression->kind == "postfix-expression") return PrintPostfix(expression, scope, indentation, expected);
		if (expression->kind == "binary-expression") return PrintBinary(expression, scope, indentation, expected);
		if (expression->kind == "assignment-expression") return PrintAssignment(expression, scope, indentation, expected);
		if (expression->kind == "conditional-expression") return PrintConditional(expression, scope, indentation, expected);
		if (expression->kind == "subscript-expression") return PrintSubscript(expression, scope, indentation, expected);
		if (expression->kind == "cast-expression") return PrintCast(expression, scope, indentation, expected);
		if (expression->kind == "sizeof-expression" || expression->kind == "type-trait-expression")
			return PrintTypeTrait(expression, scope, indentation, expected);
		if (expression->kind == "member-expression") return PrintMember(expression, scope, indentation, expected);
		if (expression->kind == "braced-init-list") return PrintBraced(expression, scope, indentation, expected);
		throw logic_error("unsupported expression output");
	}
	void PrintLiteral(const CPPGMAstNodePtr& expression, Scope* scope,
		unsigned int indentation, const TypePtr& expected)
	{
		PA12ExprInfo info = Infer(expression, scope, expected);
		ExprHeader(indentation, "literal", info, expression->value);
	}
	void PrintIdentifier(const CPPGMAstNodePtr& expression, Scope* scope,
		unsigned int indentation, const TypePtr& expected)
	{
		PA12ExprInfo info = Infer(expression, scope, expected);
		if (info.binding && info.binding->kind == BIND_ENUMERATOR)
		{
			PA12ExprInfo literal;
			literal.type = info.binding->type;
			literal.category = "prvalue";
			ostringstream value;
			value << info.binding->value;
			ExprHeader(indentation, "literal", literal, value.str());
			return;
		}
		if (info.binding && info.binding->injected_member && !info.binding->injected_object_name.empty())
		{
			PA12ExprInfo object;
			object.type = info.binding->injected_owner;
			object.category = "lvalue";
			ExprHeader(indentation, "member-expression", info, expression->value);
			ExprHeader(indentation + 1, "id-expression", object, info.binding->injected_object_name);
			return;
		}
		if (!info.binding && info.candidates.size() > 1)
			throw logic_error("overloaded function name needs a target");
		ExprHeader(indentation, "id-expression", info, expression->value);
	}
	void PrintCall(const CPPGMAstNodePtr& expression, Scope* scope,
		unsigned int indentation, const TypePtr& expected)
	{
		const CPPGMAstNodePtr callee_node = expression->children.empty() ? CPPGMAstNodePtr() : expression->children[0];
		const CPPGMAstNodePtr arguments_node = expression->children.size() > 1 ? expression->children[1] : CPPGMAstNodePtr();
		if (callee_node && callee_node->kind == "id-expression" && callee_node->value == "__builtin_constant_p")
		{
			PA12ExprInfo info;
			info.type = Fundamental("int");
			info.category = "prvalue";
			bool known = false;
			if (arguments_node && !arguments_node->children.empty())
			{
				ConstantValue value = a_.Evaluate(arguments_node->children[0], scope);
					known = value.integral.known;
			}
			ExprHeader(indentation, "literal", info, known ? "1" : "0");
			return;
		}
		TypePtr functional_type = FunctionalCastType(callee_node, scope);
		if (functional_type)
		{
			PA12ExprInfo info = Infer(expression, scope, expected);
			if ((!arguments_node || arguments_node->children.empty()) &&
				(info.type->kind == TYPE_FUNDAMENTAL || info.type->kind == TYPE_ENUM))
			{
				ExprHeader(indentation, "literal", info, "0");
				return;
			}
			ExprHeader(indentation, "cast-expression", info);
			if (arguments_node)
				for (size_t i = 0; i < arguments_node->children.size(); ++i)
					PrintExpr(arguments_node->children[i], scope, indentation + 1);
			return;
		}
		vector<PA12ExprInfo> argument_infos;
		if (arguments_node)
			for (size_t i = 0; i < arguments_node->children.size(); ++i)
				argument_infos.push_back(Infer(arguments_node->children[i], scope));
		PA12CallChoice choice;
		bool direct = DirectFunctionName(callee_node, scope);
		if (direct) choice = ChooseCall(callee_node->value, scope, argument_infos);
		else
		{
			PA12ExprInfo callee = Infer(callee_node, scope);
			choice.function = FunctionType(callee.type);
			if (!choice.function) throw logic_error("expression is not callable");
		}
		PA12ExprInfo info = Infer(expression, scope, expected);
		ExprHeader(indentation, "call-expression", info);
		if (direct)
		{
				Indent(indentation + 1);
			*out_ << "callee " << PA12PublicQualifiedName(choice.binding->qualified_name) << " " <<
				TypeTextPA12(choice.function) << "\n";
		}
		else PrintExpr(callee_node, scope, indentation + 1);
		if (arguments_node)
			for (size_t i = 0; i < arguments_node->children.size(); ++i)
			{
				TypePtr parameter = choice.function && i < choice.function->parameters.size() ?
					choice.function->parameters[i] : TypePtr();
				PrintExpr(arguments_node->children[i], scope, indentation + 1, parameter);
			}
	}
	void PrintUnary(const CPPGMAstNodePtr& expression, Scope* scope,
		unsigned int indentation, const TypePtr& expected)
	{
		PA12ExprInfo info = Infer(expression, scope, expected);
		ExprHeader(indentation, "unary-expression", info, expression->value);
		CPPGMAstNodePtr child_node = expression->children.empty() ? CPPGMAstNodePtr() : expression->children[0];
		TypePtr member_function = child_node && child_node->kind == "id-expression" ?
			QualifiedMemberFunctionType(child_node->value, Infer(child_node, scope).type, scope) : TypePtr();
		if (member_function)
		{
			PA12ExprInfo child_info;
			child_info.type = member_function;
			child_info.category = "lvalue";
			ExprHeader(indentation + 1, "id-expression", child_info, child_node->value);
		}
		else PrintExpr(child_node, scope, indentation + 1);
	}
	void PrintPostfix(const CPPGMAstNodePtr& expression, Scope* scope,
		unsigned int indentation, const TypePtr& expected)
	{
		PA12ExprInfo info = Infer(expression, scope, expected);
		ExprHeader(indentation, "postfix-expression", info, expression->value);
		PrintExpr(expression->children[0], scope, indentation + 1);
	}
	void PrintBinary(const CPPGMAstNodePtr& expression, Scope* scope,
		unsigned int indentation, const TypePtr& expected)
	{
		PA12ExprInfo info = Infer(expression, scope, expected);
		ExprHeader(indentation, "binary-expression", info, expression->value);
		PrintExpr(expression->children[0], scope, indentation + 1);
		PrintExpr(expression->children[1], scope, indentation + 1);
	}
	void PrintAssignment(const CPPGMAstNodePtr& expression, Scope* scope,
		unsigned int indentation, const TypePtr& expected)
	{
		PA12ExprInfo info = Infer(expression, scope, expected);
		ExprHeader(indentation, "assignment-expression", info, expression->value);
		PA12ExprInfo left = Infer(expression->children[0], scope);
		PrintExpr(expression->children[0], scope, indentation + 1);
		PrintExpr(expression->children[1], scope, indentation + 1,
			PA12Operator(expression->value) == "=" ? ExpressionValueType(left) : TypePtr());
	}
	void PrintConditional(const CPPGMAstNodePtr& expression, Scope* scope,
		unsigned int indentation, const TypePtr& expected)
	{
		PA12ExprInfo info = Infer(expression, scope, expected);
		ExprHeader(indentation, "conditional-expression", info);
		for (size_t i = 0; i < expression->children.size(); ++i)
			PrintExpr(expression->children[i], scope, indentation + 1);
	}
	void PrintSubscript(const CPPGMAstNodePtr& expression, Scope* scope,
		unsigned int indentation, const TypePtr& expected)
	{
		PA12ExprInfo info = Infer(expression, scope, expected);
		ExprHeader(indentation, "subscript-expression", info);
		PA12ExprInfo first = Infer(expression->children[0], scope);
		TypePtr first_value = ExpressionValueType(first);
		if (first_value && (first_value->kind == TYPE_ARRAY || first_value->kind == TYPE_POINTER))
		{
			PrintExpr(expression->children[0], scope, indentation + 1);
			PrintExpr(expression->children[1], scope, indentation + 1);
		}
		else
		{
			PrintExpr(expression->children[1], scope, indentation + 1);
			PrintExpr(expression->children[0], scope, indentation + 1);
		}
	}
	void PrintCast(const CPPGMAstNodePtr& expression, Scope* scope,
		unsigned int indentation, const TypePtr& expected)
	{
		PA12ExprInfo info = Infer(expression, scope, expected);
		if (PA12IsReference(info.type) && expression->children.size() > 1)
		{
			CPPGMAstNodePtr child = expression->children[1];
			if (child && child->kind == "parenthesized-expression" && !child->children.empty())
				child = child->children[0];
			if (child && child->kind == "id-expression")
			{
				ExprHeader(indentation, "id-expression", info, child->value);
				return;
			}
		}
		ExprHeader(indentation, "cast-expression", info, expression->value);
		if (expression->children.size() > 1)
			PrintExpr(expression->children[1], scope, indentation + 1);
	}
	void PrintTypeTrait(const CPPGMAstNodePtr& expression, Scope* scope,
		unsigned int indentation, const TypePtr& expected)
	{
		PA12ExprInfo info = Infer(expression, scope, expected);
		ExprHeader(indentation, expression->kind, info, expression->value);
	}
	void PrintMember(const CPPGMAstNodePtr& expression, Scope* scope,
		unsigned int indentation, const TypePtr& expected)
	{
		PA12ExprInfo info = Infer(expression, scope, expected);
		string member_value = expression->value;
		if (expression->children.size() > 1)
		{
			const size_t colon = member_value.find(':');
			if (colon != string::npos) member_value = member_value.substr(0, colon + 1) + expression->children[1]->value;
		}
		ExprHeader(indentation, "member-expression", info, member_value);
		PrintExpr(expression->children[0], scope, indentation + 1);
	}
	void PrintBraced(const CPPGMAstNodePtr& expression, Scope* scope,
		unsigned int indentation, const TypePtr& expected)
	{
		PA12ExprInfo info = Infer(expression, scope, expected);
		ExprHeader(indentation, "braced-init-list", info);
		for (size_t i = 0; i < expression->children.size(); ++i)
			PrintExpr(expression->children[i], scope, indentation + 1);
	}
	void EnsureBinding(Scope* scope, const string& name, const TypePtr& type,
		BindingKind kind)
	{
		if (!scope || name.empty()) return; vector<Binding*> direct = DirectBindings(scope, name);
		for (size_t i = 0; i < direct.size(); ++i)
			if (direct[i]->kind == kind && PA12SameType(direct[i]->type, type, false)) return;
		Binding binding(kind, name, type);
		scope->add(binding);
	}
	bool IsAnonymousUnionSpecifier(const CPPGMAstNodePtr& specs, TypePtr* type = 0)
	{
		if (!specs) return false;
		for (size_t i = 0; i < specs->children.size(); ++i)
		{
			CPPGMAstNodePtr child = specs->children[i];
			if (child && child->kind == "class-specifier" &&
				ClassKey(child) == "union" && (child->value.empty() || child->value == "<unnamed>"))
			{
				if (type)
				{
					map<const CPPGMAstNode*, TypePtr>::iterator found = a_.class_types_.find(child.get());
					if (found != a_.class_types_.end()) *type = found->second;
				}
				return true;
			}
		}
		return false;
	}
	void NormalizeAnonymousUnion(const TypePtr& type, bool has_named_storage)
	{
		if (!type || type->kind != TYPE_CLASS || type->tag != "union") return;
		if (type->name.find("__anonymous_union_type__") != 0) return;
		if (display_type_names_.find(type.get()) != display_type_names_.end()) return;
		ostringstream generated;
		if (has_named_storage) generated << "__local_type" << ++local_union_count_;
		else { const unsigned int index = ++anonymous_union_count_; generated << "__anonymous_union_type__" << 6 + index << "_" << 16 + index; }
		display_type_names_[type.get()] = generated.str();
	}
	string AnonymousUnionStorageName(const TypePtr& type)
	{
		const string type_name = DisplayName(type);
		const string marker = "__anonymous_union_type__";
		return type_name.find(marker) == 0 ?
			"__anonymous_union_storage__" + type_name.substr(marker.size()) :
			"__anonymous_union_storage";
	}
	void PrintInitializer(const CPPGMAstNodePtr& initializer, Scope* scope,
		unsigned int indentation, const TypePtr& target)
	{
		if (!initializer || initializer->children.empty()) return; CPPGMAstNodePtr child = initializer->children[0];
		if (!child || child->kind == "special-initializer") return;
		if (child->kind == "paren-initializer")
		{
			if (!child->children.empty()) PrintExpr(child->children[0], scope, indentation, target);
			return;
		}
		PrintExpr(child, scope, indentation, target);
	}
	void PrintConstructorAction(const TypePtr& type, const string& object_name,
		Scope* scope, unsigned int indentation)
	{
		const string class_name = DisplayName(type);
		TypePtr display_type(new Type(*type)); display_type->name = class_name;
		const string short_name = PA12LastComponent(class_name);
		const string constructor = class_name + "::" + short_name;
		TypePtr constructor_type = FunctionOf(vector<TypePtr>(1, PointerTo(display_type)), false,
			Fundamental("void"));
		Indent(indentation);
		*out_ << "constructor-action " << constructor << "\n";
		PA12ExprInfo call;
		call.type = Fundamental("void");
		call.category = "prvalue";
		ExprHeader(indentation + 1, "call-expression", call);
		Indent(indentation + 2);
		*out_ << "callee " << constructor << " " << TypeTextPA12(constructor_type) << "\n";
		PA12ExprInfo address;
		address.type = PointerTo(display_type);
		address.category = "prvalue";
		ExprHeader(indentation + 2, "unary-expression", address, "OP_AMP:&");
		PA12ExprInfo object;
		object.type = display_type;
		object.category = "lvalue";
		ExprHeader(indentation + 3, "id-expression", object, object_name);
		(void)scope;
	}
	void NoteConstructor(const TypePtr& type)
	{
		for (size_t i = 0; i < constructed_types_.size(); ++i)
			if (constructed_types_[i].get() == type.get()) return;
		constructed_types_.push_back(type);
	}
	void PrintSimple(const CPPGMAstNodePtr& node, Scope* scope,
		unsigned int indentation, bool with_wrapper)
	{
		if (!node || node->children.empty()) return;
		Analyzer::SpecFacts facts;
		TypePtr base = a_.TypeFromSpecSeq(node->children[0], scope, &facts);
		CPPGMAstNodePtr list = ChildOfKind(node, "init-declarator-list");
		if (!list)
		{
			if (IsAnonymousUnionSpecifier(node->children[0])) return;
			return;
		}
		if (facts.is_typedef)
		{
			for (size_t i = 0; i < list->children.size(); ++i)
			{
				CPPGMAstNodePtr item = list->children[i];
				if (!item || item->children.empty()) continue;
				TypePtr type = a_.BuildDeclarator(item->children[0], base, scope);
				const string name = DeclaratorName(item->children[0]);
				Indent(indentation);
				*out_ << "type-alias " << name << " " << TypeTextPA12(type) << "\n";
				a_.AddTypeBinding(scope, name, type, true);
			}
			return;
		}
		if (with_wrapper)
		{
			Indent(indentation);
			*out_ << "simple-declaration\n";
			++indentation;
		}
		TypePtr anonymous_union;
		const bool has_anonymous_union = IsAnonymousUnionSpecifier(node->children[0], &anonymous_union);
		if (has_anonymous_union) NormalizeAnonymousUnion(anonymous_union, !list->children.empty());
		for (size_t i = 0; i < list->children.size(); ++i)
		{
			CPPGMAstNodePtr item = list->children[i];
			if (!item || item->children.empty()) continue;
			CPPGMAstNodePtr declarator = item->children[0];
			TypePtr type = a_.BuildDeclarator(declarator, base, scope);
			TypePtr display_type = PA12AdjustedType(type);
			const string name = DeclaratorName(declarator);
			if (name.empty()) continue;
			if (facts.is_constexpr && type->kind != TYPE_FUNCTION) type = CloneWithCv(type, true, false);
			const bool function = display_type->kind == TYPE_FUNCTION;
			EnsureBinding(scope, name, type, function ? BIND_FUNCTION : BIND_VARIABLE);
			string display_name = name;
			if (function)
			{
				const vector<Binding*> direct = DirectBindings(scope, name);
				for (size_t binding_index = 0; binding_index < direct.size(); ++binding_index)
					if (direct[binding_index]->kind == BIND_FUNCTION &&
						direct[binding_index]->qualified_name.find("::") != string::npos)
					{
						display_name = PA12PublicQualifiedName(direct[binding_index]->qualified_name);
						break;
					}
			}
			Indent(indentation);
			*out_ << (function ? "function-declaration " : "variable ") << display_name << " " <<
				TypeTextPA12(display_type) << "\n";
			CPPGMAstNodePtr initializer = item->children.size() > 1 ? item->children[1] : CPPGMAstNodePtr();
			if (type->kind == TYPE_CLASS && initializer && !initializer->children.empty())
			{
				CPPGMAstNodePtr initial = initializer->children[0];
				if (initial && initial->kind == "paren-initializer" && !initial->children.empty())
					initial = initial->children[0];
				if (initial && initial->kind == "call-expression" &&
					!initial->children.empty() && initial->children[0] &&
					initial->children[0]->kind == "id-expression" &&
					FunctionalCastType(initial->children[0], scope) &&
					(initial->children.size() < 2 || initial->children[1]->children.empty()))
					throw logic_error("most vexing declaration is not an object");
			}
			if (!function && type->kind == TYPE_CLASS && !initializer)
			{
				PrintConstructorAction(type, name, scope, indentation + 1);
				NoteConstructor(type);
			}
			else if (!function) PrintInitializer(initializer, scope, indentation + 1, type);
		}
	}
	void PrintAlias(const CPPGMAstNodePtr& node, Scope* scope, unsigned int indentation)
	{
		if (!node || node->children.empty()) throw logic_error("invalid alias declaration");
		TypePtr type = a_.TypeFromTypeId(node->children[0], scope);
		Indent(indentation);
		*out_ << "type-alias " << node->value << " " << TypeTextPA12(type) << "\n";
		a_.AddTypeBinding(scope, node->value, type, true);
	}
	void PrintCondition(const CPPGMAstNodePtr& condition, Scope* scope,
		unsigned int indentation)
	{
		if (!condition) return; if (condition->kind != "condition-declaration")
		{
			PrintExpr(condition, scope, indentation);
			return;
		}
		if (condition->children.size() < 3) throw logic_error("invalid condition declaration");
		Analyzer::SpecFacts facts;
		TypePtr base = a_.TypeFromSpecSeq(condition->children[0], scope, &facts);
		TypePtr type = a_.BuildDeclarator(condition->children[1], base, scope);
		const string name = DeclaratorName(condition->children[1]);
		EnsureBinding(scope, name, type, BIND_VARIABLE);
		Indent(indentation);
		*out_ << "condition-declaration\n";
		Indent(indentation + 1);
		*out_ << "variable " << name << " " << TypeTextPA12(type) << "\n";
		PrintInitializer(condition->children[2], scope, indentation + 2, type);
	}
	void PrintCompound(const CPPGMAstNodePtr& node, Scope* parent,
		unsigned int indentation, const TypePtr& return_type)
	{
		Scope* block = parent;
		map<const CPPGMAstNode*, Scope*>::iterator found = a_.compound_scopes_.find(node.get());
		if (found != a_.compound_scopes_.end() && found->second->parent == parent) block = found->second;
		else block = a_.NewChild(parent, SCOPE_BLOCK, string());
		Indent(indentation);
		*out_ << "compound-statement\n";
		for (size_t i = 0; i < node->children.size(); ++i)
			PrintStatement(node->children[i], block, indentation + 1, return_type);
	}
	void PrintAnonymousUnionStatement(const CPPGMAstNodePtr& node, Scope* scope,
		unsigned int indentation)
	{
		map<const CPPGMAstNode*, TypePtr>::iterator found = a_.class_types_.find(node.get());
		if (found == a_.class_types_.end()) return;
		TypePtr type = found->second;
		NormalizeAnonymousUnion(type, false);
		const string storage = AnonymousUnionStorageName(type);
		for (size_t i = 0; i < scope->bindings.size(); ++i)
			if (scope->bindings[i].injected_member &&
				scope->bindings[i].injected_owner.get() == type.get())
				scope->bindings[i].injected_object_name = storage;
		EnsureBinding(scope, storage, type, BIND_VARIABLE);
		Indent(indentation);
		*out_ << "simple-declaration\n";
		Indent(indentation + 1);
		*out_ << "variable " << storage << " " << TypeTextPA12(type) << "\n";
		PrintConstructorAction(type, storage, scope, indentation + 2);
		NoteConstructor(type);
	}
	void PrintStatement(const CPPGMAstNodePtr& node, Scope* scope,
		unsigned int indentation, const TypePtr& return_type)
	{
		if (!node) return;
		if (node->kind == "compound-statement") return PrintCompound(node, scope, indentation, return_type);
		if (node->kind == "simple-declaration" || node->kind == "bit-field-declaration")
			return PrintSimple(node, scope, indentation, true);
		if (node->kind == "alias-declaration") return PrintAlias(node, scope, indentation);
		if (node->kind == "class-specifier") return PrintAnonymousUnionStatement(node, scope, indentation);
		if (node->kind == "enum-specifier")
		{
			Indent(indentation);
			*out_ << "simple-declaration\n";
			return;
		}
		if (node->kind == "namespace-alias-definition" || node->kind == "using-declaration" ||
			node->kind == "using-directive") return;
		if (node->kind == "expression-statement") return PrintExpressionStatement(node, scope, indentation);
		if (node->kind == "return-statement") return PrintReturnStatement(node, scope, indentation, return_type);
		if (node->kind == "if-statement") return PrintIfStatement(node, scope, indentation, return_type);
		if (node->kind == "while-statement") return PrintWhileStatement(node, scope, indentation, return_type);
		if (node->kind == "do-statement") return PrintDoStatement(node, scope, indentation, return_type);
		if (node->kind == "for-statement") return PrintForStatement(node, scope, indentation, return_type);
		if (node->kind == "switch-statement") return PrintSwitchStatement(node, scope, indentation, return_type);
		if (node->kind == "case-statement") return PrintCaseStatement(node, scope, indentation, return_type);
		if (node->kind == "default-statement") return PrintDefaultStatement(node, scope, indentation, return_type);
		if (node->kind == "break-statement" || node->kind == "continue-statement")
		{
			Indent(indentation);
			*out_ << node->kind << "\n";
			return;
		}
		if (node->kind == "labeled-statement")
			return node->children.empty() ? void() : PrintStatement(node->children[0], scope, indentation, return_type);
		throw logic_error("unsupported statement");
	}
	void PrintExpressionStatement(const CPPGMAstNodePtr& node, Scope* scope,
		unsigned int indentation)
	{
		Indent(indentation);
		*out_ << "expression-statement\n";
		if (!node->children.empty()) PrintExpr(node->children[0], scope, indentation + 1);
	}
	void PrintReturnStatement(const CPPGMAstNodePtr& node, Scope* scope,
		unsigned int indentation, const TypePtr& return_type)
	{
		Indent(indentation);
		*out_ << "return-statement\n";
		if (!node->children.empty()) PrintExpr(node->children[0], scope, indentation + 1,
			PA12ValueType(return_type));
	}
	void PrintIfStatement(const CPPGMAstNodePtr& node, Scope* scope,
		unsigned int indentation, const TypePtr& return_type)
	{
		Scope* condition_scope = a_.NewChild(scope, SCOPE_BLOCK, string());
		Indent(indentation);
		*out_ << "if-statement\n";
		CPPGMAstNodePtr condition = ChildOfKind(node, "condition");
		Indent(indentation + 1);
		*out_ << "condition\n";
		if (condition && !condition->children.empty())
			PrintCondition(condition->children[0], condition_scope, indentation + 2);
		CPPGMAstNodePtr then_node = ChildOfKind(node, "then");
		if (then_node && !then_node->children.empty())
		{
			Indent(indentation + 1);
			*out_ << "then\n";
			PrintStatement(then_node->children[0], condition_scope, indentation + 2, return_type);
		}
		CPPGMAstNodePtr else_node = ChildOfKind(node, "else");
		if (else_node && !else_node->children.empty())
		{
			Indent(indentation + 1);
			*out_ << "else\n";
			PrintStatement(else_node->children[0], condition_scope, indentation + 2, return_type);
		}
	}
	void PrintWhileStatement(const CPPGMAstNodePtr& node, Scope* scope,
		unsigned int indentation, const TypePtr& return_type)
	{
		Scope* condition_scope = a_.NewChild(scope, SCOPE_BLOCK, string());
		Indent(indentation);
		*out_ << "while-statement\n";
		if (!node->children.empty() && node->children[0]->kind == "condition")
		{
			Indent(indentation + 1);
			*out_ << "condition\n";
			if (!node->children[0]->children.empty())
				PrintCondition(node->children[0]->children[0], condition_scope, indentation + 2);
		}
		if (node->children.size() > 1) PrintStatement(node->children[1], condition_scope, indentation + 1, return_type);
	}
	void PrintDoStatement(const CPPGMAstNodePtr& node, Scope* scope,
		unsigned int indentation, const TypePtr& return_type)
	{
		Scope* statement_scope = a_.NewChild(scope, SCOPE_BLOCK, string());
		Indent(indentation);
		*out_ << "do-statement\n";
		if (!node->children.empty()) PrintStatement(node->children[0], statement_scope, indentation + 1, return_type);
		if (node->children.size() > 1)
		{
			Indent(indentation + 1);
			*out_ << "condition\n";
			if (!node->children[1]->children.empty())
				PrintCondition(node->children[1]->children[0], statement_scope, indentation + 2);
		}
	}
	void PrintForStatement(const CPPGMAstNodePtr& node, Scope* scope,
		unsigned int indentation, const TypePtr& return_type)
	{
		Scope* loop_scope = a_.NewChild(scope, SCOPE_BLOCK, string());
		Indent(indentation);
		*out_ << "for-statement\n";
		if (!node->children.empty())
		{
			Indent(indentation + 1);
			*out_ << "for-init-statement\n";
			if (!node->children[0]->children.empty())
				PrintStatement(node->children[0]->children[0], loop_scope, indentation + 2, return_type);
		}
		size_t index = 1;
		if (index < node->children.size() && node->children[index]->kind == "condition")
		{
			Indent(indentation + 1);
			*out_ << "condition\n";
			if (!node->children[index]->children.empty())
				PrintCondition(node->children[index]->children[0], loop_scope, indentation + 2);
			++index;
		}
		if (index < node->children.size() && node->children[index]->kind == "iteration")
		{
			Indent(indentation + 1);
			*out_ << "iteration\n";
			if (!node->children[index]->children.empty())
				PrintExpr(node->children[index]->children[0], loop_scope, indentation + 2);
			++index;
		}
		if (index < node->children.size()) PrintStatement(node->children[index], loop_scope, indentation + 1, return_type);
	}
	void PrintSwitchStatement(const CPPGMAstNodePtr& node, Scope* scope,
		unsigned int indentation, const TypePtr& return_type)
	{
		Scope* condition_scope = a_.NewChild(scope, SCOPE_BLOCK, string());
		Indent(indentation);
		*out_ << "switch-statement\n";
		if (!node->children.empty() && node->children[0]->kind == "condition")
		{
			Indent(indentation + 1);
			*out_ << "condition\n";
			if (!node->children[0]->children.empty())
				PrintCondition(node->children[0]->children[0], condition_scope, indentation + 2);
		}
		if (node->children.size() > 1) PrintStatement(node->children[1], condition_scope, indentation + 1, return_type);
	}
	void PrintCaseStatement(const CPPGMAstNodePtr& node, Scope* scope,
		unsigned int indentation, const TypePtr& return_type)
	{
		Indent(indentation);
		*out_ << "case-statement\n";
		if (!node->children.empty()) PrintExpr(node->children[0], scope, indentation + 1);
		if (node->children.size() > 1) PrintStatement(node->children[1], scope, indentation + 1, return_type);
	}
	void PrintDefaultStatement(const CPPGMAstNodePtr& node, Scope* scope,
		unsigned int indentation, const TypePtr& return_type)
	{
		Indent(indentation);
		*out_ << "default-statement\n";
		if (!node->children.empty()) PrintStatement(node->children[0], scope, indentation + 1, return_type);
	}
	void PrintFunctionDefinition(const CPPGMAstNodePtr& node, Scope* scope,
		unsigned int indentation)
	{
		if (!node || node->children.size() < 3) throw logic_error("invalid function definition"); Analyzer::SpecFacts facts;
		TypePtr base = a_.TypeFromSpecSeq(node->children[0], scope, &facts);
		CPPGMAstNodePtr declarator = node->children[1];
		TypePtr type = a_.BuildDeclarator(declarator, base, scope);
		type = PA12AdjustedType(type);
		if (!type || type->kind != TYPE_FUNCTION) throw logic_error("definition is not a function");
		const string name = DeclaratorName(declarator);
		Indent(indentation);
		const string display_name = scope && !scope->qualified_prefix.empty() ? PA12PublicQualifiedName(scope->qualified_prefix + "::" + PA12LastComponent(name)) : PA12LastComponent(name);
		*out_ << "function-definition " << display_name << " " <<
			TypeTextPA12(type) << "\n";
		CPPGMAstNodePtr clause = ChildOfKind(declarator, "parameter-clause");
		size_t parameter_index = 0;
		if (clause)
			for (size_t i = 0; i < clause->children.size(); ++i)
			{
				CPPGMAstNodePtr parameter = clause->children[i];
				if (!parameter || parameter->kind != "parameter-declaration") continue;
				string parameter_name;
				if (parameter->children.size() > 1 && parameter->children[1])
					parameter_name = DeclaratorName(parameter->children[1]);
				TypePtr parameter_type = parameter_index < type->parameters.size() ?
					type->parameters[parameter_index] : DeclaredType(parameter, scope);
				Indent(indentation + 1);
				*out_ << "parameter " << parameter_name << " " << TypeTextPA12(parameter_type) << "\n";
				++parameter_index;
			}
		Scope* function_scope = scope;
		map<const CPPGMAstNode*, Scope*>::iterator found = a_.function_scopes_.find(node.get());
		if (found != a_.function_scopes_.end()) function_scope = found->second;
		PrintCompound(node->children[2], function_scope, indentation + 1,
			PA12ValueType(type->child));
	}
	void PrintSyntheticConstructor(const TypePtr& type, unsigned int indentation)
	{
		const string class_name = DisplayName(type);
		TypePtr display_type(new Type(*type)); display_type->name = class_name;
		const string constructor = class_name + "::" + PA12LastComponent(class_name);
		TypePtr function = FunctionOf(vector<TypePtr>(1, PointerTo(display_type)), false, Fundamental("void"));
		Indent(indentation);
		*out_ << "function-definition " << constructor << " " << TypeTextPA12(function) << "\n";
		Indent(indentation + 1);
		*out_ << "parameter this " << TypeTextPA12(PointerTo(display_type)) << "\n";
		Indent(indentation + 1);
		*out_ << "compound-statement\n";
	}
	void PrintTop(const CPPGMAstNodePtr& node, Scope* scope,
		unsigned int indentation, const string& ignored)
	{
		(void)ignored;
		if (!node) return;
		if (node->kind == "namespace-definition")
		{
			Indent(indentation);
			*out_ << "namespace-definition " << node->value << "\n";
			Scope* child_scope = scope;
			map<const CPPGMAstNode*, Scope*>::iterator found = a_.namespace_scopes_.find(node.get());
			if (found != a_.namespace_scopes_.end()) child_scope = found->second;
			for (size_t i = 0; i < node->children.size(); ++i)
				if (node->children[i] && node->children[i]->kind != "inline")
					PrintTop(node->children[i], child_scope, indentation + 1, string());
			return;
		}
		if (node->kind == "function-definition")
		{
			PrintFunctionDefinition(node, scope, indentation);
			return;
		}
		if (node->kind == "simple-declaration" || node->kind == "bit-field-declaration")
		{
			PrintSimple(node, scope, indentation, false);
			return;
		}
		if (node->kind == "alias-declaration")
		{
			PrintAlias(node, scope, indentation);
			return;
		}
		if (node->kind == "linkage-specification" || node->kind == "explicit-instantiation-declaration")
		{
			for (size_t i = 0; i < node->children.size(); ++i)
				PrintTop(node->children[i], scope, indentation, string());
			return;
		}
		if (node->kind == "template-declaration") {
			if (node->children.size() > 1) {
				Scope* template_scope = a_.NewChild(scope, SCOPE_TEMPLATE_PARAMETERS, string()); const CPPGMAstNodePtr list = ChildOfKind(node->children[0], "template-parameter-list");
				if (list) for (size_t parameter_index = 0; parameter_index < list->children.size(); ++parameter_index) {
					const CPPGMAstNodePtr parameter = list->children[parameter_index]; if (!parameter || parameter->kind != "type-parameter") continue; const string name = FirstIdentifier(parameter); if (name.empty()) continue;
					const bool template_template = HasKind(parameter, "template-template-parameter"); a_.AddTypeBinding(template_scope, name, TypePtr(new Type(template_template ? TYPE_TEMPLATE_TEMPLATE_PARAMETER : TYPE_TEMPLATE_PARAMETER, name)));
				}
				PrintTop(node->children[1], template_scope, indentation, string()); }
			return; }
		if (node->kind == "special-member-definition") return;
		if (node->kind == "empty-declaration" || node->kind == "namespace-alias-definition" ||
			node->kind == "using-directive" || node->kind == "using-declaration" ||
			node->kind == "class-forward-declaration" || node->kind == "class-specifier" ||
			node->kind == "enum-specifier" || node->kind == "static-assert-declaration") return;
		throw logic_error("unsupported top-level declaration");
	}
}; void Analyzer::PrintSemantics(const CPPGMAstNodePtr& tree, ostream& out) { PA12Printer printer(*this); printer.Print(tree, out); }
void EmitPA12Semantics(const CPPGMAstNodePtr& translation_unit, ostream& out) { Analyzer analyzer; analyzer.Analyze(translation_unit); ostringstream buffer; analyzer.PrintSemantics(translation_unit, buffer); out << buffer.str(); }
