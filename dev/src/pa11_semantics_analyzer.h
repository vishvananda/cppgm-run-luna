#pragma once
#include "pa11_semantics_model.h"
#include "pa11_semantics_layout.h"
#include "pa19_constants.h"
class Analyzer {
public:
	Analyzer(); void Analyze(const CPPGMAstNodePtr& tree);
	void PredeclareGeneratedScopes(const CPPGMAstNodePtr& tree);
	void Print(ostream& out) const;
	void PrintSemantics(const CPPGMAstNodePtr& tree, ostream& out);
	unique_ptr<Scope> global_;
	unsigned int anonymous_type_count_;
	map<const CPPGMAstNode*, Scope*> function_scopes_;
	map<const CPPGMAstNode*, Scope*> compound_scopes_;
	map<const CPPGMAstNode*, Scope*> namespace_scopes_;
	map<const CPPGMAstNode*, TypePtr> class_types_;
	map<const CPPGMAstNode*, TypePtr> enum_types_;
	vector<PendingClassLayout> pending_class_layouts_;
	vector<pair<CPPGMAstNodePtr, Scope*> > pending_using_declarations_; bool processing_pending_using_declarations_ = false;
	map<const Binding*, ConstantValue> constant_binding_values_;
	map<string, vector<Binding*> > constant_template_functions_;
	vector<map<string, ConstantValue> > constant_frames_;
	vector<map<string, vector<ConstantValue> > > constant_pack_frames_;
	vector<ConstantValue> constant_receivers_;
	map<const CPPGMAstNode*, unsigned> constant_function_depth_;
	static const unsigned kConstantFunctionDepthLimit = 512;
	static const unsigned kConstantLoopIterationLimit = 100000;
	struct ConstantFlow
	{
		enum Kind { NORMAL, RETURN, BREAK, CONTINUE };
		Kind kind;
		ConstantValue value;
		ConstantFlow(Kind flow_kind = NORMAL, const ConstantValue& flow_value = ConstantValue())
			: kind(flow_kind), value(flow_value) {}
	};
	static void Indent(ostream& out, unsigned int indentation)
	{
		for (unsigned int i = 0; i < indentation; ++i) out << "  ";
	}
	void PrintScope(const Scope* scope, ostream& out, unsigned int indentation) const
	{
		Indent(out, indentation);
		out << "scope " << ScopeKindText(scope->kind);
		if (scope->kind != SCOPE_TEMPLATE_PARAMETERS && scope->kind != SCOPE_BLOCK)
			out << " " << scope->name;
		out << "\n";
		for (size_t i = 0; i < scope->bindings.size(); ++i)
		{
			const Binding& binding = scope->bindings[i];
			Indent(out, indentation + 1);
			out << BindingKindText(binding.kind) << " " << binding.name << " ";
			if (!binding.type_override.empty()) out << binding.type_override;
			else out << TypeText(binding.type);
			if (binding.kind == BIND_ENUMERATOR)
				out << " " << binding.value;
			out << "\n";
		}
		for (size_t i = 0; i < scope->children.size(); ++i)
			PrintScope(scope->children[i].get(), out, indentation + 1);
	}
	Scope* NewChild(Scope* parent, ScopeKind kind, const string& name)
	{
		return parent->child(kind, name);
	}
	Scope* FindNamespaceDirect(Scope* from, const string& name,
		set<Scope*>& visited) const
	{
		if (!from || !visited.insert(from).second) return 0;
		map<string, Scope*>::const_iterator child = from->namespace_children.find(name);
		if (child != from->namespace_children.end()) return child->second;
		map<string, Scope*>::const_iterator alias = from->namespace_aliases.find(name);
		if (alias != from->namespace_aliases.end()) return alias->second;
		for (size_t i = 0; i < from->using_directives.size(); ++i)
		{
			Scope* found = FindNamespaceDirect(from->using_directives[i], name, visited);
			if (found) return found;
		}
		return 0;
	}
	Scope* FindNamespaceDirect(Scope* from, const string& name) const
	{
		set<Scope*> visited;
		return FindNamespaceDirect(from, name, visited);
	}
	Scope* FindNamespace(Scope* from, const string& name) const
	{
		for (Scope* current = from; current != 0; current = current->parent)
		{
			Scope* found = FindNamespaceDirect(current, name);
			if (found) return found;
		}
		return 0;
	}
	Binding* LookupUnqualified(Scope* from, const string& name) const
	{
		for (Scope* current = from; current != 0; current = current->parent)
		{
			Binding* direct = current->local(name);
			if (direct) return direct;
			for (size_t i = 0; i < current->using_directives.size(); ++i)
			{
				Binding* imported = LookupInNamespace(current->using_directives[i], name);
				if (imported) return imported;
			}
		}
		return 0;
	}
	Binding* LookupInNamespace(Scope* scope, const string& name,
		set<Scope*>& visited) const
	{
		if (!scope || !visited.insert(scope).second) return 0;
		Binding* direct = scope->local(name);
		if (direct) return direct;
		for (size_t i = 0; i < scope->using_directives.size(); ++i)
		{
			Binding* imported = LookupInNamespace(scope->using_directives[i], name, visited);
			if (imported) return imported;
		}
		return 0;
	}
	Binding* LookupInNamespace(Scope* scope, const string& name) const
	{
		set<Scope*> visited;
		return LookupInNamespace(scope, name, visited);
	}
	Scope* ScopeForType(const TypePtr& type) const
	{
		if (!type) return 0;
		if (type->kind == TYPE_CLASS || type->kind == TYPE_ENUM)
			return type->owned_scope;
		return 0;
	}
	struct PathTarget
	{
		Scope* scope;
		Binding* binding;
		PathTarget(Scope* target_scope = 0, Binding* target_binding = 0)
			: scope(target_scope), binding(target_binding) {}
	};
	vector<string> SplitPath(const string& raw, bool* absolute = 0) const
	{
		string path = raw;
		bool is_absolute = path.compare(0, 2, "::") == 0;
		if (is_absolute) path = path.substr(2);
		vector<string> parts;
		size_t begin = 0;
		while (begin <= path.size())
		{
			const size_t end = path.find("::", begin);
			string part = path.substr(begin, end == string::npos ? string::npos : end - begin);
			if (!part.empty()) parts.push_back(part);
			if (end == string::npos) break;
			begin = end + 2;
		}
		if (absolute) *absolute = is_absolute;
		return parts;
	}
	PathTarget ResolvePath(Scope* from, const string& raw) const
	{
		bool absolute = false;
		const vector<string> parts = SplitPath(raw, &absolute);
		if (parts.empty()) return PathTarget();
		Scope* current_scope = absolute ? global_.get() : from;
		Binding* current_binding = 0;
		for (size_t i = 0; i < parts.size(); ++i)
		{
			const string& part = parts[i];
			if (current_binding)
			{
				current_scope = ScopeForType(current_binding->type);
				if (!current_scope) return PathTarget();
				current_binding = 0;
			}
			if (i == 0 && !absolute)
			{
				Binding* binding = LookupUnqualified(current_scope, part);
				if (binding)
				{
					current_binding = binding;
					if (i + 1 == parts.size()) return PathTarget(0, binding);
					continue;
				}
			}
			Scope* namespace_scope = (i == 0 && !absolute) ?
				FindNamespace(current_scope, part) : FindNamespaceDirect(current_scope, part);
			if (namespace_scope)
			{
				current_scope = namespace_scope;
				if (i + 1 == parts.size()) return PathTarget(namespace_scope, 0);
				continue;
			}
			Binding* binding = (i == 0 && !absolute) ?
				LookupUnqualified(current_scope, part) : LookupInNamespace(current_scope, part);
			if (!binding && current_scope && current_scope->kind == SCOPE_CLASS &&
				current_scope->owner_type)
				for (TypePtr base = current_scope->owner_type->direct_base; base;
					base = base->direct_base)
				{
					if (LastComponent(base->name) == part && base->owned_scope &&
						base->owned_scope->parent)
						binding = base->owned_scope->parent->local(part);
					if (!binding && base->owned_scope) binding = base->owned_scope->local(part);
					if (binding) break;
				}
			if (!binding) return PathTarget();
			if (i + 1 == parts.size()) return PathTarget(0, binding);
			current_binding = binding;
		}
		return current_binding ? PathTarget(0, current_binding) : PathTarget(current_scope, 0);
	}
	Scope* ResolveNamespace(Scope* from, const string& raw) const
	{
		PathTarget target = ResolvePath(from, raw);
		return target.scope;
	}
	Binding* ResolveBinding(Scope* from, const string& raw) const
	{
		PathTarget target = ResolvePath(from, raw);
		return target.binding;
	}
	bool AccessibleType(const Binding& binding, Scope* from) const
	{
		if(!binding.is_member || binding.access.empty() || binding.access == "public") return true;
		for(Scope* current = from; current; current = current->parent)
			if(current->kind == SCOPE_CLASS && current->owner_type) {
				if(current->owner_type == binding.member_owner) return true;
				if(binding.access == "protected")
					for(TypePtr base = current->owner_type->direct_base; base; base = base->direct_base)
						if(base == binding.member_owner) return true;
			}
		return false;
	}
	TypePtr ResolveType(Scope* from, const string& raw) const;
	static bool IsFundamentalWord(const string& word)
	{
		return word == "bool" || word == "char" || word == "char16_t" ||
			word == "char32_t" || word == "double" || word == "float" ||
			word == "int" || word == "long" || word == "short" ||
			word == "signed" || word == "unsigned" || word == "void" ||
			word == "wchar_t" || word == "nullptr_t" || word == "auto";
	}
	static string FundamentalName(const vector<string>& words)
	{
		bool is_unsigned = false;
		bool is_signed = false;
		int long_count = 0;
		bool is_short = false;
		string base;
		for (size_t i = 0; i < words.size(); ++i)
		{
			if (words[i] == "unsigned") is_unsigned = true;
			else if (words[i] == "signed") is_signed = true;
			else if (words[i] == "long") ++long_count;
			else if (words[i] == "short") is_short = true;
			else if (words[i] != "int") base = words[i];
		}
		if (base.empty()) base = "int";
		if (base == "int" || base == "char")
		{
			string result;
			if (is_unsigned) result = "unsigned ";
			else if (is_signed) result = "signed ";
			if (is_short) result += "short int";
			else if (long_count >= 2) result += "long long int";
			else if (long_count == 1) result += "long int";
			else if (base == "char") result += "char";
			else result += "int";
			return result;
		}
		if (base == "double" && long_count != 0) return "long double";
		return base;
	}
	static bool IsCvNode(const CPPGMAstNodePtr& node, const string& word)
	{
		return node && ((node->kind == "cv-qualifier" || node->kind == "decl-specifier") &&
			node->value.find(":" + word) != string::npos);
	}
	static bool HasNodeValue(const CPPGMAstNodePtr& node, const string& kind,
		const string& value);
	static bool IsPureInitializer(const CPPGMAstNodePtr& item);
	struct SpecFacts
	{
		bool is_typedef;
		bool is_constexpr;
		bool is_const;
		bool is_volatile;
		bool is_static;
		bool is_mutable;
		bool is_friend;
		bool is_virtual;
		vector<string> fundamental_words;
		TypePtr named_type;
		SpecFacts() : is_typedef(false), is_constexpr(false), is_const(false),
			is_volatile(false), is_static(false), is_mutable(false), is_friend(false),
			is_virtual(false), fundamental_words(), named_type() {}
	};
	TypePtr ResolveFunctionSpelledType(const string& spelling, Scope* scope, SpecFacts& info); TypePtr ResolveArrayReferenceSpelledType(const string& spelling, Scope* scope, SpecFacts& info);
	TypePtr TypeFromDecltype(const CPPGMAstNodePtr& node, Scope* scope)
	{
		if (!node || node->children.empty()) throw logic_error("invalid decltype");
		const CPPGMAstNodePtr expression = node->children[0];
		if (expression && expression->kind == "keyword-literal" &&
			expression->value.find(":nullptr") != string::npos)
			return Fundamental("nullptr_t");
		if (expression && (expression->kind == "sizeof-expression" ||
			expression->kind == "sizeof-pack-expression" ||
			expression->kind == "type-trait-expression"))
			return Fundamental("unsigned long int");
		Binding* binding = 0;
		if (expression->kind == "id-expression")
			binding = ResolveBinding(scope, expression->value);
		else if (expression->kind == "parenthesized-expression" &&
			!expression->children.empty() && expression->children[0] &&
			expression->children[0]->kind == "id-expression")
			binding = ResolveBinding(scope, expression->children[0]->value);
		TypePtr result;
		if (binding) result = binding->type;
		else result = ExpressionType(expression, scope);
		if (!result) throw logic_error("unknown decltype expression");
		if (expression->kind == "parenthesized-expression")
		{
			const CPPGMAstNodePtr inner = expression->children.empty() ?
				CPPGMAstNodePtr() : expression->children[0];
			if (inner && inner->kind == "id-expression")
			{
				Binding* inner_binding = ResolveBinding(scope, inner->value);
				if (inner_binding && (inner_binding->kind == BIND_VARIABLE ||
					inner_binding->kind == BIND_PARAMETER))
					result = ReferenceTo(TYPE_LVALUE_REFERENCE, result);
			}
		}
		return result;
	}
	TypePtr ResolveSpelledType(string spelling, Scope* scope, SpecFacts& info)
	{
		while (!spelling.empty() && isspace(static_cast<unsigned char>(spelling[0]))) spelling.erase(0, 1);
		while (!spelling.empty() && isspace(static_cast<unsigned char>(spelling[spelling.size() - 1])))
			spelling.erase(spelling.size() - 1, 1);
		spelling = StripTypeMarker(spelling); TypePtr function_type = ResolveFunctionSpelledType(spelling, scope, info); if (function_type) return function_type;
		if(TypePtr array_reference = ResolveArrayReferenceSpelledType(spelling, scope, info)) return array_reference;
		if (spelling.compare(0, 8, "typename ") == 0) spelling = spelling.substr(8);
		while (spelling.compare(0, 7, "static ") == 0) { info.is_static = true; spelling = spelling.substr(7); } while (spelling.compare(0, 10, "constexpr ") == 0) { info.is_constexpr = true; spelling = spelling.substr(10); }
		bool leading_const = false; bool leading_volatile = false;
		if (spelling.compare(0, 6, "const ") == 0) {
			leading_const = true;
			spelling = spelling.substr(6);
		} else if (spelling.compare(0, 9, "volatile ") == 0) {
			leading_volatile = true;
			spelling = spelling.substr(9);
		}
		vector<char> suffixes;
		vector<long long> array_bounds;
		for (;;) {
			while (!spelling.empty() && isspace(static_cast<unsigned char>(spelling[spelling.size() - 1])))
				spelling.erase(spelling.size() - 1, 1);
			if (spelling.size() >= 5 && spelling.compare(spelling.size() - 5, 5, "const") == 0 &&
				(spelling.size() == 5 || spelling[spelling.size() - 6] == ' ' ||
					(spelling[spelling.size() - 6] != '_' &&
					 IsFundamentalWord(spelling.substr(0, spelling.size() - 5))))) {
				info.is_const = true;
				spelling.erase(spelling.size() - 5);
				continue;
			}
			if (spelling.size() >= 8 && spelling.compare(spelling.size() - 8, 8, "volatile") == 0) {
				info.is_volatile = true;
				spelling.erase(spelling.size() - 8);
				continue;
			}
			if (!spelling.empty() && (spelling[spelling.size() - 1] == '*' ||
				spelling[spelling.size() - 1] == '&')) {
				suffixes.push_back(spelling[spelling.size() - 1]);
				spelling.erase(spelling.size() - 1);
				continue;
			}
			if (!spelling.empty() && spelling[spelling.size() - 1] == ']') {
				const size_t open = spelling.rfind('[');
				if (open == string::npos) break;
				string bound_text = spelling.substr(open + 1,
					spelling.size() - open - 2);
				long long bound = -1; if (!bound_text.empty()) {
					string numeric_bound = bound_text; while (!numeric_bound.empty() && string("uUlL").find(numeric_bound[numeric_bound.size() - 1]) != string::npos)
						numeric_bound.erase(numeric_bound.size() - 1);
					char* end = 0;
					const long long parsed = strtoll(numeric_bound.c_str(), &end, 0);
					if (end && *end == '\0') bound = parsed;
					else {
						CPPGMAstNodePtr bound_expression(new CPPGMAstNode(
							"id-expression", bound_text));
						try {
							ConstantValue value = Evaluate(bound_expression, scope);
							if (value.integral.known) bound = PA19Signed(value.integral);
						} catch (...) {}
						if (bound < 0) break;
					}
				}
				array_bounds.push_back(bound); spelling.erase(open);
				continue;
			}
			break;
		}
		while (!spelling.empty() && isspace(static_cast<unsigned char>(spelling[0]))) spelling.erase(0, 1);
		vector<string> words;
		string word;
		for (size_t i = 0; i <= spelling.size(); ++i) {
			if (i == spelling.size() || isspace(static_cast<unsigned char>(spelling[i]))) {
				if (!word.empty()) { words.push_back(word); word.clear(); }
			} else word += spelling[i];
		}
		TypePtr base;
		bool all_fundamental = !words.empty();
		for (size_t i = 0; i < words.size(); ++i)
			if (!IsFundamentalWord(words[i])) all_fundamental = false;
		if (all_fundamental) base = Fundamental(FundamentalName(words));
		else base = ResolveType(scope, spelling);
		if (leading_const) base = CloneWithCv(base, true, false);
		if (leading_volatile) base = CloneWithCv(base, false, true);
		for (size_t i = suffixes.size(); i > 0; --i) {
			if (suffixes[i - 1] == '*') base = PointerTo(base);
			else base = ReferenceTo(TYPE_LVALUE_REFERENCE, base);
		}
		for (size_t i = array_bounds.size(); i > 0; --i)
			base = ArrayOf(array_bounds[i - 1], base);
		return base;
	}
	TypePtr TypeFromSpecSeq(const CPPGMAstNodePtr& sequence, Scope* scope,
		SpecFacts* facts = 0)
	{
		if (!sequence) throw logic_error("missing declaration specifiers");
		SpecFacts local_facts;
		SpecFacts& info = facts ? *facts : local_facts;
		vector<string> fundamentals;
		for (size_t i = 0; i < sequence->children.size(); ++i)
		{
			const CPPGMAstNodePtr child = sequence->children[i];
			if (!child) continue;
			if (child->kind == "class-specifier")
			{
				info.named_type = ProcessClass(child, scope);
				continue;
			}
			if (child->kind == "class-forward-declaration")
			{
				info.named_type = ProcessForwardClass(child, scope);
				continue;
			}
			if (child->kind == "decltype-specifier")
			{
				info.named_type = TypeFromDecltype(child, scope);
				continue;
			}
			if (child->kind == "enum-specifier")
			{
				info.named_type = ProcessEnum(child, scope);
				continue;
			}
			if (child->kind == "type-name")
			{
				info.named_type = ResolveSpelledType(child->value, scope, info);
				continue;
			}
			if (child->kind == "type-specifier")
			{
				const size_t colon = child->value.find(':');
				const string word = colon == string::npos ? child->value : child->value.substr(colon + 1);
				if (IsFundamentalWord(word)) fundamentals.push_back(word);
				continue;
			}
			if (child->kind == "cv-qualifier")
			{
				if (child->value.find(":const") != string::npos) info.is_const = true;
				if (child->value.find(":volatile") != string::npos) info.is_volatile = true;
				continue;
			}
			if (child->kind != "decl-specifier") continue;
			const string value = child->value;
			if (value == "KW_TYPEDEF:typedef") info.is_typedef = true;
			else if (value == "KW_CONSTEXPR:constexpr") info.is_constexpr = true;
			else if (value == "KW_INLINE:inline") {
			}
			else if (value == "KW_STATIC:static") info.is_static = true;
			else if (value == "KW_MUTABLE:mutable") info.is_mutable = true;
			else if (value == "KW_FRIEND:friend" || value == "friend") info.is_friend = true;
			else if (value == "KW_EXTERN:extern") {
			}
			else if (value == "KW_THREAD_LOCAL:thread_local") {
			}
			else if (value == "KW_VIRTUAL:virtual") info.is_virtual = true;
			else if (value == "KW_CONST:const") info.is_const = true;
			else if (value == "KW_VOLATILE:volatile") info.is_volatile = true;
			else
			{
				const size_t colon = value.find(':');
				string word = colon == string::npos ? value : value.substr(colon + 1);
				while (!word.empty() && isspace(static_cast<unsigned char>(word[0])))
					word.erase(0, 1);
				while (!word.empty() && isspace(static_cast<unsigned char>(word[word.size() - 1])))
					word.erase(word.size() - 1, 1);
				if (word.compare(0, 7, "friend ") == 0) {
					info.is_friend = true;
					word = word.substr(7);
					while (!word.empty() && isspace(static_cast<unsigned char>(word[0])))
						word.erase(word.begin());
				}
				const bool compound_leading_cv =
					(value.find("TT_IDENTIFIER:") == 0 &&
					 ((word.compare(0, 6, "const ") == 0 && word.size() > 6) ||
					  (word.compare(0, 9, "volatile ") == 0 && word.size() > 9)) &&
					 (word.find('*') != string::npos || word.find('&') != string::npos ||
					  word.find("::") != string::npos || word.find(' ') != string::npos));
				if (compound_leading_cv) {
					info.named_type = ResolveSpelledType(word, scope, info);
					continue;
				}
			if (word.compare(0, 6, "const ") == 0) {
					info.is_const = true;
					word = word.substr(6);
					while (!word.empty() && isspace(static_cast<unsigned char>(word[0]))) word.erase(0, 1);
				}
				if (word.compare(0, 9, "volatile ") == 0) {
					info.is_volatile = true;
					word = word.substr(9);
					while (!word.empty() && isspace(static_cast<unsigned char>(word[0]))) word.erase(0, 1);
				}
				if (IsFundamentalWord(word)) fundamentals.push_back(word);
				else if (value.find("decltype(") == 0)
					info.named_type = TypeFromDecltype(child, scope);
				else if (value.find("TT_IDENTIFIER:") == 0)
					info.named_type = ResolveSpelledType(word, scope, info);
				else if (value.find("::") != string::npos) {
					string qualified = value;
					if (qualified.compare(0, 6, "const ") == 0) {
						info.is_const = true;
						qualified = qualified.substr(6);
					}
					if (qualified.compare(0, 9, "volatile ") == 0) {
						info.is_volatile = true;
						qualified = qualified.substr(9);
					}
					info.named_type = ResolveSpelledType(StripTypeMarker(qualified), scope, info);
				}
				else
					info.named_type = ResolveSpelledType(word, scope, info);
			}
		}
		TypePtr result = info.named_type;
		if (!result && !fundamentals.empty()) result = Fundamental(FundamentalName(fundamentals));
		if (!result) throw logic_error("declaration has no type");
		return CloneWithCv(result, info.is_const, info.is_volatile);
	}
	TypePtr TypeFromTypeId(const CPPGMAstNodePtr& type_id, Scope* scope)
	{
		if (!type_id || type_id->kind != "type-id" || type_id->children.empty())
			throw logic_error("invalid type-id");
		SpecFacts facts;
		TypePtr base = TypeFromSpecSeq(type_id->children[0], scope, &facts);
		if (type_id->children.size() > 1)
			base = BuildDeclarator(type_id->children[1], base, scope);
		return base;
	}
	ConstantValue FromIntegralValue(const PA19IntegralValue& value) const;
	ConstantValue FromFloatingValue(long double value, const TypePtr& type = TypePtr()) const;
	ConstantValue FromObjectValue(const TypePtr& type,
		const shared_ptr<ConstantObject>& object) const;
	ConstantValue FromPointerValue(const shared_ptr<ConstantPointer>& pointer,
		const TypePtr& type = TypePtr()) const;
	PA19IntegralValue ToIntegralValue(const ConstantValue& value) const;
	PA19IntegralValue ParseLiteralValue(const string& raw) const;
	long long ParseLiteral(const string& raw) const;
	ConstantValue Evaluate(const CPPGMAstNodePtr& expression, Scope* scope);
	ConstantValue EvaluateTyped(const CPPGMAstNodePtr& expression, Scope* scope,
		const TypePtr& expected_type);
	ConstantValue EvaluateFunctionCall(Binding* function,
		const CPPGMAstNodePtr& call, Scope* caller_scope,
		const ConstantValue& receiver = ConstantValue(),
		const TypePtr& expected_type = TypePtr());
	ConstantValue EvaluateConstructor(const TypePtr& type,
		const vector<ConstantValue>& arguments, Scope* caller_scope,
		const CPPGMAstNodePtr& call = CPPGMAstNodePtr());
	ConstantValue DefaultConstantValue(const TypePtr& type, Scope* scope);
	ConstantValue ConvertConstantValue(const ConstantValue& value,
		const TypePtr& target, Scope* scope);
	ConstantValue EvaluateMemberCall(const CPPGMAstNodePtr& call, Scope* scope);
	ConstantValue EvaluateMemberValue(const CPPGMAstNodePtr& expression, Scope* scope);
	ConstantFlow EvaluateStatement(const CPPGMAstNodePtr& statement, Scope* scope);
	ConstantFlow EvaluateCompound(const CPPGMAstNodePtr& compound, Scope* scope);
	ConstantFlow EvaluateConditionStatement(const CPPGMAstNodePtr& statement,
		Scope* scope);
	bool ConstantFrameValue(const string& name, ConstantValue* value) const;
	bool ConstantPackValue(const string& name, vector<ConstantValue>* value) const;
	void SetConstantFrameValue(const string& name, const ConstantValue& value);
	Binding* FindConstantFunction(const string& name, Scope* scope,
		size_t argument_count) const;
	size_t FundamentalSize(const string& name) const
	{
		if (name == "char" || name == "signed char" || name == "unsigned char" || name == "bool") return 1;
		if (name == "short int" || name == "unsigned short int" || name == "char16_t") return 2;
		if (name == "int" || name == "unsigned int" || name == "signed int" ||
			name == "float" || name == "wchar_t" || name == "char32_t") return 4;
		if (name == "long int" || name == "unsigned long int" || name == "signed long int" ||
			name == "long long int" || name == "unsigned long long int" || name == "double") return 8;
		if (name == "long double") return 16;
		return 0;
	}
	size_t TypeSize(const TypePtr& type) const
	{
		if (!type) return 0;
		switch (type->kind)
		{
		case TYPE_FUNDAMENTAL: return FundamentalSize(type->name);
		case TYPE_POINTER:
		case TYPE_LVALUE_REFERENCE:
		case TYPE_RVALUE_REFERENCE:
		case TYPE_MEMBER_POINTER: return 8;
		case TYPE_FUNCTION: return 4;
		case TYPE_ARRAY: return type->bound < 0 ? 0 : static_cast<size_t>(type->bound) * TypeSize(type->child);
		case TYPE_ENUM:
			if (!type->complete) throw logic_error("sizeof incomplete enum");
			return type->underlying ? TypeSize(type->underlying) : 4;
		case TYPE_CLASS:
			if (!type->complete || !type->layout_complete) {
				throw logic_error("sizeof incomplete class");
			}
			return type->object_size;
		case TYPE_TEMPLATE_PARAMETER:
		case TYPE_TEMPLATE_TEMPLATE_PARAMETER: return 0;
		}
		return 0;
	}
	size_t TypeAlignment(const TypePtr& type) const
	{
		if (!type) return 0;
		if (type->kind == TYPE_ARRAY) return TypeAlignment(type->child);
		if (type->kind == TYPE_CLASS && !type->complete)
			throw logic_error("alignof incomplete class");
		if (type->kind == TYPE_CLASS && type->layout_complete)
			return type->object_alignment;
		if (type->kind == TYPE_ENUM && type->underlying) return TypeAlignment(type->underlying);
		return TypeSize(type);
	}
	TypePtr ExpressionType(const CPPGMAstNodePtr& expression, Scope* scope,
		size_t requested_arguments = static_cast<size_t>(-1))
	{
		if (!expression) throw logic_error("invalid expression type");
		if (expression->kind == "id-expression")
		{
			Binding* binding = ResolveBinding(scope, expression->value);
			if (!binding && expression->value.find('<') != string::npos) {
				try { return ResolveType(scope, expression->value); }
				catch (const logic_error&) {}
				// A call expression stores an explicitly specialized function
				// template as an id-expression child (`test<T>`).  It is not a
				// type-id, so recover the ordinary function binding after the
				// template-id type probe fails.
				const size_t open = expression->value.find('<');
				if (open != string::npos)
					binding = ResolveBinding(scope, expression->value.substr(0, open));
			}
			if (!binding) throw logic_error("unknown expression name");
			return binding->type;
		}
		if (expression->kind == "parenthesized-expression" && !expression->children.empty())
			return ExpressionType(expression->children[0], scope, requested_arguments);
		if (expression->kind == "member-expression" && expression->children.size() >= 2)
		{
			TypePtr object = ExpressionType(expression->children[0], scope);
			while (object && (object->kind == TYPE_LVALUE_REFERENCE ||
				object->kind == TYPE_RVALUE_REFERENCE || object->kind == TYPE_POINTER))
				object = object->child;
			if (!object || object->kind != TYPE_CLASS) return Fundamental("int");
			const string member_name = expression->children[1]->value;
			Binding* selected = 0;
			size_t selected_score = static_cast<size_t>(-1);
			for (TypePtr current = object; current; current = current->direct_base)
			{
				Scope* owner = ScopeForType(current);
				if (!owner) continue;
				for (size_t i = 0; i < owner->bindings.size(); ++i)
				{
					Binding& candidate = owner->bindings[i];
					if (candidate.name != member_name || !candidate.type) continue;
					if (candidate.kind == BIND_FUNCTION && candidate.type->kind == TYPE_FUNCTION)
					{
						if (object->is_const && !candidate.type->function_const) continue;
						if (object->is_volatile && !candidate.type->function_volatile) continue;
						const size_t arity = candidate.type->parameters.size();
						if (requested_arguments != static_cast<size_t>(-1) &&
							((!candidate.type->variadic && arity != requested_arguments) ||
							 (candidate.type->variadic && arity > requested_arguments))) continue;
						size_t score = requested_arguments == static_cast<size_t>(-1) ?
							arity : (candidate.type->variadic ? arity + 1 : 0);
						if (!object->is_const && candidate.type->function_const) score += 2;
						if (!selected || score < selected_score) {
							selected = &candidate;
							selected_score = score;
						}
					}
					else if (!selected) selected = &candidate;
				}
			}
			if (selected) return selected->type;
			return Fundamental("int");
		}
		if (expression->kind == "sizeof-expression" ||
			expression->kind == "sizeof-pack-expression" ||
			expression->kind == "type-trait-expression")
			return Fundamental("unsigned long int");
		if (expression->kind == "cast-expression" && expression->children.size() >= 2)
			return TypeFromTypeId(expression->children[0], scope);
		if (expression->kind == "call-expression" && !expression->children.empty())
		{
			size_t arity = static_cast<size_t>(-1);
			if (expression->children.size() > 1 && expression->children[1])
				arity = expression->children[1]->children.size();
			TypePtr callee = ExpressionType(expression->children[0], scope, arity);
			return callee && callee->kind == TYPE_FUNCTION ? callee->child : Fundamental("int");
		}
		if (expression->kind == "literal" || expression->kind == "keyword-literal") return Fundamental("int");
		if (expression->kind == "binary-expression" && !expression->children.empty())
			return ExpressionType(expression->children[0], scope);
		return Fundamental("int");
	}
	struct ParameterFacts
	{
		vector<TypePtr> types;
		bool variadic;
		ParameterFacts() : types(), variadic(false) {}
	};
	ParameterFacts Parameters(const CPPGMAstNodePtr& clause, Scope* scope)
	{
		ParameterFacts result;
		if (!clause) return result;
		for (size_t i = 0; i < clause->children.size(); ++i)
		{
			const CPPGMAstNodePtr parameter = clause->children[i];
			if (!parameter) continue;
			if (parameter->kind == "parameter-pack" || parameter->kind == "ellipsis")
			{
				result.variadic = true;
				continue;
			}
			if (parameter->kind != "parameter-declaration") continue;
			if (parameter->children.empty()) throw logic_error("invalid parameter");
			SpecFacts facts;
			TypePtr base = TypeFromSpecSeq(parameter->children[0], scope, &facts);
			TypePtr type = base;
			if (parameter->children.size() > 1 && parameter->children[1] &&
				(parameter->children[1]->kind == "declarator" ||
				 parameter->children[1]->kind == "abstract-declarator"))
			{
				type = BuildDeclarator(parameter->children[1], base, scope);
			}
			result.types.push_back(type);
		}
		if (!result.variadic && result.types.size() == 1 &&
			result.types[0]->kind == TYPE_FUNDAMENTAL && result.types[0]->name == "void" &&
			!result.types[0]->is_const && !result.types[0]->is_volatile)
			result.types.clear();
		return result;
	}
	TypePtr ApplySuffix(const CPPGMAstNodePtr& suffix, const TypePtr& base, Scope* scope)
	{
		if (suffix->kind == "array-suffix")
		{
			long long bound = 0;
			if (!suffix->children.empty())
			{
				ConstantValue value = Evaluate(suffix->children[0], scope);
				if (!value.integral.known) throw logic_error("array bound is not constant");
				bound = value.value;
			}
			return ArrayOf(bound, base);
		}
		if (suffix->kind == "parameter-clause")
		{
			ParameterFacts parameters = Parameters(suffix, scope);
			return FunctionOf(parameters.types, parameters.variadic, base);
		}
		return base;
	}
	TypePtr BuildDeclarator(const CPPGMAstNodePtr& declarator,
		const TypePtr& base, Scope* scope)
	{
		if (!declarator) return base;
		vector<CPPGMAstNodePtr> pointers;
		vector<CPPGMAstNodePtr> suffixes;
		CPPGMAstNodePtr nested;
		bool function_const = false;
		bool function_volatile = false;
		bool function_lvalue_ref_qualified = false;
		bool function_rvalue_ref_qualified = false;
		bool saw_function_suffix = false;
		for (size_t i = 0; i < declarator->children.size(); ++i)
		{
			const CPPGMAstNodePtr child = declarator->children[i];
			if (!child) continue;
			if (child->kind == "ptr-operator") pointers.push_back(child);
			else if (child->kind == "cv-qualifier")
			{
				if (saw_function_suffix && child->value.find(":const") != string::npos)
					function_const = true;
				else if (saw_function_suffix && child->value.find(":volatile") != string::npos)
					function_volatile = true;
				else if (!saw_function_suffix) pointers.push_back(child);
			}
			else if (child->kind == "ref-qualifier")
			{
				if (child->value.find("&&") != string::npos)
					function_rvalue_ref_qualified = true;
				else function_lvalue_ref_qualified = true;
			}
			else if (child->kind == "nested-declarator") nested = child->children.empty() ?
				CPPGMAstNodePtr() : child->children[0];
			else if (child->kind == "array-suffix" || child->kind == "parameter-clause")
			{
				suffixes.push_back(child);
				if (child->kind == "parameter-clause") saw_function_suffix = true;
			}
		}
		if (nested)
		{
			TypePtr outer = base;
			for (size_t i = 0; i < pointers.size(); ++i)
			{
				if (pointers[i]->kind == "ptr-operator")
				{
					if (pointers[i]->value.find("::*") != string::npos)
						outer = MemberPointerTo(ResolveType(scope, pointers[i]->value.substr(0,
							pointers[i]->value.find("::*"))), outer);
					else if (pointers[i]->value.find("&") != string::npos &&
						pointers[i]->value.find("*") == string::npos)
						outer = ReferenceTo(pointers[i]->value.find("&&") != string::npos ?
							TYPE_RVALUE_REFERENCE : TYPE_LVALUE_REFERENCE, outer);
					else outer = PointerTo(outer);
				}
				else outer = CloneWithCv(outer,
					pointers[i]->value.find(":const") != string::npos,
					pointers[i]->value.find(":volatile") != string::npos);
			}
			for (size_t i = suffixes.size(); i-- > 0;) outer = ApplySuffix(suffixes[i], outer, scope);
			TypePtr result = BuildDeclarator(nested, outer, scope);
			if (function_const && result->kind == TYPE_FUNCTION) result->function_const = true;
			if (function_volatile && result->kind == TYPE_FUNCTION) result->function_volatile = true;
			if (function_const && result->kind == TYPE_MEMBER_POINTER && result->child &&
				result->child->kind == TYPE_FUNCTION) result->child->function_const = true;
			if (function_volatile && result->kind == TYPE_MEMBER_POINTER && result->child &&
				result->child->kind == TYPE_FUNCTION) result->child->function_volatile = true;
			if (result->kind == TYPE_FUNCTION)
			{
				result->function_lvalue_ref_qualified = function_lvalue_ref_qualified;
				result->function_rvalue_ref_qualified = function_rvalue_ref_qualified;
			}
			else if (result->kind == TYPE_MEMBER_POINTER && result->child &&
				result->child->kind == TYPE_FUNCTION)
			{
				result->child->function_lvalue_ref_qualified = function_lvalue_ref_qualified;
				result->child->function_rvalue_ref_qualified = function_rvalue_ref_qualified;
			}
			return result;
		}
		TypePtr result = base;
		for (size_t i = 0; i < pointers.size(); ++i)
		{
			if (pointers[i]->kind == "ptr-operator")
			{
				if (pointers[i]->value.find("::*") != string::npos)
					result = MemberPointerTo(ResolveType(scope, pointers[i]->value.substr(0,
						pointers[i]->value.find("::*"))), result);
				else if (pointers[i]->value.find("&") != string::npos &&
					pointers[i]->value.find("*") == string::npos)
					result = ReferenceTo(pointers[i]->value.find("&&") != string::npos ?
						TYPE_RVALUE_REFERENCE : TYPE_LVALUE_REFERENCE, result);
				else result = PointerTo(result);
			}
			else result = CloneWithCv(result,
				pointers[i]->value.find(":const") != string::npos,
				pointers[i]->value.find(":volatile") != string::npos);
		}
		for (size_t i = suffixes.size(); i-- > 0;) result = ApplySuffix(suffixes[i], result, scope);
		if (function_const && result->kind == TYPE_FUNCTION) result->function_const = true;
		if (function_volatile && result->kind == TYPE_FUNCTION) result->function_volatile = true;
		if (result->kind == TYPE_FUNCTION)
		{
			result->function_lvalue_ref_qualified = function_lvalue_ref_qualified;
			result->function_rvalue_ref_qualified = function_rvalue_ref_qualified;
		}
		else if (result->kind == TYPE_MEMBER_POINTER && result->child &&
			result->child->kind == TYPE_FUNCTION)
		{
			result->child->function_lvalue_ref_qualified = function_lvalue_ref_qualified;
			result->child->function_rvalue_ref_qualified = function_rvalue_ref_qualified;
		}
		return result;
	}
	void AddTypeBinding(Scope* scope, const string& name, const TypePtr& type,
		bool alias = false, const string& override_text = string())
	{
		Binding* existing = scope->local(name);
		if (existing && (existing->kind == BIND_TYPE || existing->kind == BIND_TYPE_ALIAS))
		{
			existing->type = type;
			if (!override_text.empty()) existing->type_override = override_text;
			return;
		}
		Binding binding(alias ? BIND_TYPE_ALIAS : BIND_TYPE, name, type);
		binding.type_override = override_text;
		scope->add(binding);
	}
	Scope* ClassScope(const TypePtr& type, Scope* parent, const string& name)
	{
		if (type->owned_scope) return type->owned_scope;
		Scope* result = NewChild(parent, SCOPE_CLASS, name);
		result->owner_type = type;
		if (parent && parent->kind == SCOPE_CLASS) type->enclosing_type = parent->owner_type;
		type->owned_scope = result;
		return result;
	}
	string AnonymousTypeName(const string& tag)
	{
		const string stem = tag == "union" ? "union" :
			tag == "enum" ? "enum" : "class";
		ostringstream generated;
		generated << "__anonymous_" << stem << "_type__0_"
			<< (10 + anonymous_type_count_++);
		return generated.str();
	}
	static size_t AlignUp(size_t value, size_t alignment)
	;
	size_t AttributeAlignment(const CPPGMAstNodePtr& attribute, Scope* scope)
	;
	void ApplyClassAttributes(const CPPGMAstNodePtr& node, const TypePtr& type,
		Scope* scope)
	;
	void ComputeClassLayout(const CPPGMAstNodePtr& node, const TypePtr& type,
		Scope* class_scope)
	;
	void ComputeClassMemberLayout(const TypePtr& type, size_t union_size,
		size_t* offset, size_t* maximum_alignment)
	;
	void RecordClassMembers(const CPPGMAstNodePtr& node, const TypePtr& type,
		Scope* scope, Scope* class_scope)
	;
	void RecordClassDeclaration(const CPPGMAstNodePtr& child, const TypePtr& type,
		Scope* class_scope, const string& access)
	;
	TypePtr ProcessClass(const CPPGMAstNodePtr& node, Scope* scope); void PredeclareMaterializedNestedClasses(const CPPGMAstNodePtr& node, Scope* class_scope);
	TypePtr ProcessForwardClass(const CPPGMAstNodePtr& node, Scope* scope);
	TypePtr ProcessEnum(const CPPGMAstNodePtr& node, Scope* scope)
	{
		map<const CPPGMAstNode*, TypePtr>::const_iterator cached = enum_types_.find(node.get());
		if (cached != enum_types_.end()) return cached->second;
		const bool scoped = IsScopedEnum(node);
		const bool has_body = HasKind(node, "enumerator");
		if (!scoped && !has_body) throw logic_error("opaque unscoped enum is unsupported");
		const string raw_name = node->value;
		const string name = LastComponent(raw_name);
		const bool anonymous = name.empty();
		if (name.empty())
		{
			if (!has_body) throw logic_error("anonymous enum without definition");
		}
		Scope* owner = scope;
		bool qualified_definition = raw_name.find("::") != string::npos;
		TypePtr type;
		string override_text;
		if (qualified_definition)
		{
			const size_t separator = raw_name.rfind("::");
			owner = ResolveNamespace(scope, raw_name.substr(0, separator));
			if (!owner) {
				PathTarget prefix = ResolvePath(scope, raw_name.substr(0, separator));
				owner = prefix.binding ? ScopeForType(prefix.binding->type) : 0;
			}
			if (!owner) throw logic_error("unknown enum owner");
			Binding* existing = owner->local(name);
			if (!existing || (existing->kind != BIND_TYPE && existing->kind != BIND_TYPE_ALIAS))
				throw logic_error("qualified enum has no declaration");
			type = existing->type;
			override_text = string(scoped ? "enum class " : "enum ") + raw_name;
		}
		else
		{
			if (anonymous)
			{
				type.reset(new Type(TYPE_ENUM, AnonymousTypeName("enum")));
				type->scoped_enum = scoped;
				type->underlying = Fundamental("int");
			}
			else
			{
				Binding* existing = scope->local(name);
				if (existing && existing->kind == BIND_TYPE && existing->type &&
					existing->type->kind == TYPE_ENUM)
					type = existing->type;
				else
				{
						type.reset(new Type(TYPE_ENUM, name));
						if (!scope->qualified_prefix.empty()) type->name = scope->qualified_prefix + "::" + name;
					type->scoped_enum = scoped;
					type->underlying = Fundamental("int");
					AddTypeBinding(scope, name, type);
				}
			}
		}
		type->kind = TYPE_ENUM;
		type->name = anonymous ? type->name : name;
		type->scoped_enum = scoped;
		CPPGMAstNodePtr underlying_node = ChildOfKind(node, "type-id");
		if (underlying_node) {
			TypePtr underlying = TypeFromTypeId(underlying_node, scope);
			if (type->underlying_explicit && type->underlying && type->complete && !TypeText(type->underlying).empty() &&
				TypeText(type->underlying) != TypeText(underlying) && !has_body)
				throw logic_error("conflicting enum underlying type");
			type->underlying = underlying;
			type->underlying_explicit = true;
		}
		type->complete = true;
		if (!qualified_definition && !anonymous) AddTypeBinding(scope, name, type, false);
			if (qualified_definition)
			{
				AddTypeBinding(scope, raw_name, type, false, override_text);
			}
		Scope* enum_scope = 0;
		if (scoped && (has_body || !qualified_definition))
		{
			if (!qualified_definition && type->owned_scope)
				enum_scope = type->owned_scope;
			else
				enum_scope = NewChild(qualified_definition ? scope : owner, SCOPE_ENUM,
					qualified_definition ? raw_name : name);
		}
		if (enum_scope && (!type->owned_scope || qualified_definition))
			type->owned_scope = enum_scope;
		long long next_value = 0;
		for (size_t i = 0; i < node->children.size(); ++i)
		{
			const CPPGMAstNodePtr enumerator = node->children[i];
			if (!enumerator || enumerator->kind != "enumerator") continue;
			long long value = next_value;
			PA19IntegralValue constant_value = PA19IntegralValue::Signed(value, "int", 32);
			if (!enumerator->children.empty())
			{
				ConstantValue evaluated = Evaluate(enumerator->children[0], enum_scope ? enum_scope : owner);
				if (!evaluated.integral.known) throw logic_error("enum value is not constant");
				value = evaluated.value;
				constant_value = evaluated.integral;
			}
			Binding binding(BIND_ENUMERATOR, enumerator->value, type);
			binding.has_value = true;
			binding.value = value;
			binding.constant_value = constant_value;
			if (qualified_definition) binding.type_override = override_text;
			if (scoped) enum_scope->add(binding);
			else owner->add(binding);
			next_value = value + 1;
		}
		enum_types_[node.get()] = type;
		return type;
	}
	void ProcessUsingDeclaration(const CPPGMAstNodePtr& node, Scope* scope);
	void ProcessNamespace(const CPPGMAstNodePtr& node, Scope* scope);
	void ProcessNamespaceAlias(const CPPGMAstNodePtr& node, Scope* scope)
	{
		const CPPGMAstNodePtr target_node = ChildOfKind(node, "target");
		if (!target_node) throw logic_error("invalid namespace alias");
		if (scope->local(node->value) ||
			scope->namespace_children.find(node->value) != scope->namespace_children.end() ||
			scope->namespace_aliases.find(node->value) != scope->namespace_aliases.end())
			throw logic_error("namespace alias conflicts with declaration");
		Scope* target = ResolveNamespace(scope, target_node->value);
		if (!target) throw logic_error("namespace alias target is not a namespace");
		scope->namespace_aliases[node->value] = target;
	}
	bool InMaterializedTemplateMember(Scope* scope) const
	{
		for (Scope* current = scope; current; current = current->parent)
			if (current->kind == SCOPE_CLASS && current->owner_type &&
				current->owner_type->template_specialization)
				return true;
		return false;
	}
	void ProcessStaticAssert(const CPPGMAstNodePtr& node, Scope* scope)
	{
		if (node->children.empty()) throw logic_error("invalid static assertion");
		ConstantValue value;
		try {
			value = Evaluate(node->children[0], scope);
		} catch (const logic_error& error) {
			if (InMaterializedTemplateMember(scope) &&
				string(error.what()).find("incomplete class") != string::npos)
				return;
			throw;
		}
		const bool known = value.integral.known || value.floating_known ||
			(value.kind == ConstantValue::CONSTANT_OBJECT && value.object) ||
			(value.kind == ConstantValue::CONSTANT_POINTER && value.pointer);
		bool truth = false;
		if (value.integral.known) truth = PA19Raw(value.integral) != 0;
		else if (value.floating_known) truth = value.floating != 0;
		else if (value.kind == ConstantValue::CONSTANT_POINTER)
			truth = value.pointer && !value.pointer->null_pointer;
		else if (value.kind == ConstantValue::CONSTANT_OBJECT) truth = true;
		if (!known || !truth) {
			throw logic_error("static assertion failed");
		}
	}
	void ProcessCompound(const CPPGMAstNodePtr& node, Scope* parent)
	{
		Scope* block = NewChild(parent, SCOPE_BLOCK, string());
		compound_scopes_[node.get()] = block;
		for (size_t i = 0; i < node->children.size(); ++i) Process(node->children[i], block);
	}
	void AddFunctionParameters(Scope* function_scope, const CPPGMAstNodePtr& declarator,
		Scope* lookup_scope)
	{
		CPPGMAstNodePtr clause = ChildOfKind(declarator, "parameter-clause");
		if (!clause)
		{
			CPPGMAstNodePtr nested = ChildOfKind(declarator, "nested-declarator");
			if (nested && !nested->children.empty()) clause = ChildOfKind(nested->children[0], "parameter-clause");
		}
		if (!clause) return;
		for (size_t i = 0; i < clause->children.size(); ++i)
		{
			CPPGMAstNodePtr parameter = clause->children[i];
			if (!parameter || parameter->kind != "parameter-declaration" || parameter->children.empty()) continue;
			string name;
			TypePtr type = TypeFromSpecSeq(parameter->children[0], lookup_scope);
			if (parameter->children.size() > 1 && parameter->children[1])
			{
				name = FirstIdentifier(parameter->children[1]);
				type = BuildDeclarator(parameter->children[1], type, lookup_scope);
			}
			if (!name.empty()) function_scope->add(Binding(BIND_PARAMETER, name, type));
		}
	}
	void ProcessFunctionDefinition(const CPPGMAstNodePtr& node, Scope* scope);
	void ProcessSimpleDeclaration(const CPPGMAstNodePtr& node, Scope* scope);
	void ProcessSpecialMember(const CPPGMAstNodePtr& node, Scope* scope);
	bool LayoutDependenciesReady(const TypePtr& type) const; void FinishPendingClassLayouts();
	bool HasTemplateParameterScope(Scope* scope) const;
	bool IsDependentTemplateName(Scope* scope, const string& raw) const;
	void ValidateNondependentTemplateNode(const CPPGMAstNodePtr& node,
		Scope* scope, const CPPGMAstNodePtr& parent = CPPGMAstNodePtr(),
		size_t child_index = static_cast<size_t>(-1));
	void ProcessTemplate(const CPPGMAstNodePtr& node, Scope* scope);
	void Process(const CPPGMAstNodePtr& node, Scope* scope)
	{
		if (!node) return;
		if (node->kind == "namespace-definition") return ProcessNamespace(node, scope);
		if (node->kind == "namespace-alias-definition") return ProcessNamespaceAlias(node, scope);
		if (node->kind == "using-directive")
		{
			CPPGMAstNodePtr target = ChildOfKind(node, "target");
			Scope* namespace_scope = target ? ResolveNamespace(scope, target->value) : 0;
			if (!namespace_scope) throw logic_error("using target is not a namespace");
			scope->using_directives.push_back(namespace_scope);
			return;
		}
		if (node->kind == "using-declaration") return ProcessUsingDeclaration(node, scope);
		if (node->kind == "alias-declaration")
		{
			if (node->children.empty()) throw logic_error("invalid alias declaration");
			for (Scope* current = scope; current; current = current->parent)
				if (current->kind == SCOPE_TEMPLATE_PARAMETERS && current->local(node->value))
					throw logic_error("alias shadows a template parameter: " + node->value);
			AddTypeBinding(scope, node->value, TypeFromTypeId(node->children[0], scope), true);
			return;
		}
		if (node->kind == "template-declaration") return ProcessTemplate(node, scope);
		if (node->kind == "class-forward-declaration") { ProcessForwardClass(node, scope); return; }
		if (node->kind == "class-specifier") { ProcessClass(node, scope); return; }
		if (node->kind == "enum-specifier") { ProcessEnum(node, scope); return; }
		if (node->kind == "simple-declaration" || node->kind == "bit-field-declaration")
			return ProcessSimpleDeclaration(node, scope);
		if (node->kind == "function-definition") return ProcessFunctionDefinition(node, scope);
		if (node->kind == "static-assert-declaration") return ProcessStaticAssert(node, scope);
		if (node->kind == "compound-statement") return ProcessCompound(node, scope);
		if (node->kind == "linkage-specification" || node->kind == "explicit-instantiation-declaration")
		{
			for (size_t i = 0; i < node->children.size(); ++i) Process(node->children[i], scope);
			return;
		}
		if (node->kind == "special-member-definition" || node->kind == "special-member-declaration")
			return ProcessSpecialMember(node, scope);
		if (node->kind == "__bit-field-list")
		{
			for (size_t i = 0; i < node->children.size(); ++i) Process(node->children[i], scope);
			return;
		}
		if (node->kind == "access-specifier" || node->kind == "empty-declaration" ||
			node->kind == "base-clause") return;
		for (size_t i = 0; i < node->children.size(); ++i)
			if (node->children[i] && (node->children[i]->kind == "compound-statement" ||
				node->children[i]->kind == "simple-declaration" ||
				node->children[i]->kind == "static-assert-declaration"))
				Process(node->children[i], scope);
	}
};
