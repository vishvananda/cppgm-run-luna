#include "pa14_lowering.h"

namespace cppgm_pa14_lowering {
void PA14Lowerer::CollectInheritedConstructors(const TypePtr& raw_owner, Scope* scope,
                                               const vector<TypePtr>& selected_bases)
{
    TypePtr owner = type_value(raw_owner);
    if(!owner || owner->kind != TYPE_CLASS || !scope) return;
    const vector<TypePtr> bases = selected_bases.empty() ?
      DirectBaseTypes(owner) : selected_bases;
    const string owner_name = LastComponent(owner->name);
    for(size_t base_index = 0; base_index < bases.size(); ++base_index) {
      TypePtr base = type_value(bases[base_index]);
      if(!base) continue;
      const string base_name = LastComponent(base->name);
      const vector<Binding*> inherited = MemberBindings(base, base_name);
      for(size_t i = 0; i < inherited.size(); ++i) {
      Binding* source_binding = inherited[i];
      if(!source_binding || source_binding->kind != BIND_FUNCTION) continue;
      TypePtr source_function = function_target_type(source_binding->type);
      if(!source_function) continue;
      FunctionRecord* source_record = RecordForBinding(source_binding);
      if(source_record && !source_record->constructor) continue;
      vector<string> source_names;
      vector<CPPGMAstNodePtr> default_arguments;
      bool explicit_constructor = false;
      bool unwind_no = false;
      if(source_record) {
        source_names = ParameterNames(*source_record);
        default_arguments = source_record->default_arguments;
        explicit_constructor = source_record->explicit_constructor;
        unwind_no = source_record->unwind_no;
        if(source_record->source_type) source_function = source_record->source_type;
      }
		// A materialized template constructor can carry a synthesized function
		// record whose parameter metadata has already fallen back to ABI names.
		// The inherited wrapper is still required to expose the source
		// declarator's names in LowIR, so recover them from the binding AST before
		// manufacturing its forwarding declaration.
		if(source_binding->declaration) {
			CPPGMAstNodePtr source_declarator;
			if(source_binding->declaration->kind == "function-definition" &&
				source_binding->declaration->children.size() > 1)
				source_declarator = source_binding->declaration->children[1];
			else source_declarator = ChildOfKind(source_binding->declaration, "declarator");
			const CPPGMAstNodePtr source_clause = source_declarator ?
				DescendantOfKind(source_declarator, "parameter-clause") :
				CPPGMAstNodePtr();
			if(source_clause) {
				if(source_names.size() < source_function->parameters.size() + 1)
					source_names.resize(source_function->parameters.size() + 1);
				for(size_t p = 0; p < source_function->parameters.size() &&
					p < source_clause->children.size(); ++p) {
					const size_t name_index = p + 1;
					if(!source_names[name_index].empty() &&
						source_names[name_index].compare(0, 7, "__param") != 0) continue;
					const CPPGMAstNodePtr parameter = source_clause->children[p];
					if(!parameter || parameter->children.size() < 2) continue;
					const string declared = declarator_name(parameter->children[1]);
					if(!declared.empty()) source_names[name_index] = declared;
				}
			}
		}
      const string qname = TypeQualifiedName(owner) + "::" +
        special_member_symbol_name(owner, owner_name);
      const string key = function_key(qname, source_function);
      if(function_by_key_.find(key) != function_by_key_.end()) continue;
      CPPGMAstNodePtr special(new CPPGMAstNode("special-member-definition", owner_name));
      CPPGMAstNodePtr declarator(new CPPGMAstNode("declarator"));
      declarator->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", owner_name)));
      CPPGMAstNodePtr clause(new CPPGMAstNode("parameter-clause"));
      for(size_t p = 0; p < source_function->parameters.size(); ++p) {
        CPPGMAstNodePtr parameter(new CPPGMAstNode("parameter-declaration"));
        parameter->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("type-specifier")));
        CPPGMAstNodePtr parameter_declarator(new CPPGMAstNode("declarator"));
        const size_t source_name_index = p + 1;
        const string parameter_name = source_name_index < source_names.size() &&
          !source_names[source_name_index].empty() ? source_names[source_name_index] :
          "__param" + integer_text(static_cast<long long>(source_name_index));
        parameter_declarator->children.push_back(CPPGMAstNodePtr(
          new CPPGMAstNode("identifier", parameter_name)));
        parameter->children.push_back(parameter_declarator);
        clause->children.push_back(parameter);
      }
      declarator->children.push_back(clause);
      special->children.push_back(declarator);
      CPPGMAstNodePtr ctor_initializer(new CPPGMAstNode("ctor-initializer"));
      CPPGMAstNodePtr mem_initializer(new CPPGMAstNode("mem-initializer"));
      mem_initializer->children.push_back(CPPGMAstNodePtr(
        new CPPGMAstNode("mem-initializer-id", base_name)));
      CPPGMAstNodePtr arguments(new CPPGMAstNode("paren-argument-list"));
      for(size_t p = 0; p < source_function->parameters.size(); ++p) {
        const size_t source_name_index = p + 1;
        const string parameter_name = source_name_index < source_names.size() &&
          !source_names[source_name_index].empty() ? source_names[source_name_index] :
          "__param" + integer_text(static_cast<long long>(source_name_index));
        CPPGMAstNodePtr argument(new CPPGMAstNode("id-expression", parameter_name));
		// An inherited forwarding constructor must preserve the reference
		// category when it initializes the concrete base.  Its parameter is an
		// lvalue expression inside the synthesized body, so model the source
		// rvalue-reference type as an explicit cast rather than attempting to
		// bind that lvalue directly to U&&.
		if(source_function->parameters[p] &&
			source_function->parameters[p]->kind == TYPE_RVALUE_REFERENCE &&
			source_binding->declaration) {
			CPPGMAstNodePtr source_declarator = ChildOfKind(
				source_binding->declaration, "declarator");
			CPPGMAstNodePtr source_clause = source_declarator ?
				DescendantOfKind(source_declarator, "parameter-clause") :
				CPPGMAstNodePtr();
			CPPGMAstNodePtr source_parameter = source_clause && p <
				source_clause->children.size() ? source_clause->children[p] :
				CPPGMAstNodePtr();
			if(source_parameter && source_parameter->children.size() >= 2) {
				CPPGMAstNodePtr type_id(new CPPGMAstNode("type-id"));
				CPPGMAstNodePtr specifiers(new CPPGMAstNode("type-specifier-seq"));
				if(source_parameter->children[0]) for(size_t specifier = 0;
					specifier < source_parameter->children[0]->children.size(); ++specifier) {
					const CPPGMAstNodePtr source_specifier =
						source_parameter->children[0]->children[specifier];
					if(!source_specifier) continue;
					string spelling = source_specifier->value;
					const size_t marker = spelling.find(':');
					if(marker != string::npos) spelling = spelling.substr(marker + 1);
					specifiers->children.push_back(CPPGMAstNodePtr(
						new CPPGMAstNode("type-name", spelling)));
				}
				type_id->children.push_back(specifiers);
				type_id->children.push_back(source_parameter->children[1]);
				CPPGMAstNodePtr cast(new CPPGMAstNode("cast-expression",
					"KW_STATIC_CAST:static_cast"));
				cast->children.push_back(type_id);
				cast->children.push_back(argument);
				argument = cast;
			}
		}
		arguments->children.push_back(argument);
      }
      mem_initializer->children.push_back(arguments);
      ctor_initializer->children.push_back(mem_initializer);
      special->children.push_back(ctor_initializer);
      special->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("compound-statement")));
		// This constructor is synthesized after PA11 has analyzed the source
		// tree, so it has no parser-created function scope.  Give its forwarding
		// parameters a typed scope of its own; otherwise the base mem-initializer
		// sees `__param1` as an unbound expression and rejects the inherited
		// constructor even though its semantic signature is viable.
		Scope* function_scope = analyzer_.NewChild(scope, SCOPE_FUNCTION, qname);
		for(size_t p = 0; p < source_function->parameters.size(); ++p) {
			const size_t source_name_index = p + 1;
			const string parameter_name = source_name_index < source_names.size() &&
				!source_names[source_name_index].empty() ? source_names[source_name_index] :
				"__param" + integer_text(static_cast<long long>(source_name_index));
			function_scope->add(Binding(BIND_PARAMETER, parameter_name,
				source_function->parameters[p]));
		}
		// Keep the synthetic node on the same lookup path as a parser-created
		// function.  FunctionScope() consults this map before the record fallback.
		analyzer_.function_scopes_[special.get()] = function_scope;
      vector<Binding*> existing = DirectBindings(scope, owner_name);
      Binding* binding = 0;
      for(size_t b = 0; b < existing.size(); ++b) {
        TypePtr candidate = function_target_type(existing[b]->type);
        if(existing[b]->kind == BIND_FUNCTION && candidate &&
           PA12SameType(candidate, source_function, false)) {
          binding = existing[b];
          break;
        }
      }
      if(!binding) {
        Binding imported(BIND_FUNCTION, owner_name, source_function);
        imported.is_member = true;
        imported.is_static = false;
        imported.member_owner = owner;
        imported.access = "public";
        imported.declaration = special;
        binding = scope->add(imported);
      } else {
        binding->type = source_function;
        binding->qualified_name = qname;
        binding->is_member = true;
        binding->is_static = false;
        binding->member_owner = owner;
        binding->declaration = special;
      }
      functions_.push_back(FunctionRecord());
      FunctionRecord* record = &functions_.back();
      function_by_key_[key] = record;
      vector<TypePtr> parameters;
      parameters.push_back(PointerTo(owner));
      parameters.insert(parameters.end(), source_function->parameters.begin(),
        source_function->parameters.end());
      record->node = special;
      record->scope = function_scope;
      record->source_type = source_function;
      record->type = FunctionOf(parameters, source_function->variadic,
        source_function->child, false, source_function->function_volatile);
      record->member_owner = owner;
      record->qualified_name = qname;
      record->member = true;
      record->static_member = false;
      record->constructor = true;
      record->inherited_constructor_wrapper = true;
      record->explicit_constructor = explicit_constructor;
      record->definition = true;
      record->unwind_no = unwind_no;
      record->special_initializer = ctor_initializer;
      record->default_arguments = default_arguments;
      record->template_instantiation = owner->template_specialization ||
        (source_record && source_record->template_instantiation);
      record->explicit_specialization = source_record &&
        source_record->explicit_specialization;
      record->weak_binding = record->template_instantiation;
      if(owner->template_specialization) {
        record->template_primary = owner->template_primary;
        record->template_arguments = owner->template_arguments;
      } else if(source_record) {
        record->template_primary = source_record->template_primary;
        record->template_arguments = source_record->template_arguments;
      }
      EnsureConstructorBaseEntry(record);
      }
    }
  }
} // namespace cppgm_pa14_lowering
