#include "pa14_lowering.h"

#include <set>

using namespace std;

namespace cppgm_pa14_lowering {

void PA14Lowerer::EmitConstructorInitializers(FunctionRecord& function, Scope* scope)
{
    TypePtr owner = type_value(function.member_owner);
    if(!owner) return;
    CPPGMAstNodePtr this_node(new CPPGMAstNode("keyword-literal", "KW_THIS:this"));
    set<string> initialized_members;
    TypePtr base = type_value(owner->direct_base);
    bool explicitly_initialized_base = false;
    if(base && function.special_initializer) {
      for(size_t i = 0; i < function.special_initializer->children.size(); ++i) {
        CPPGMAstNodePtr initializer = function.special_initializer->children[i];
        if(!initializer || initializer->kind != "mem-initializer") continue;
        CPPGMAstNodePtr name_node = ChildOfKind(initializer, "mem-initializer-id");
        bool matches_base = name_node &&
          (LastComponent(name_node->value) == LastComponent(base->name) ||
           name_node->value == base->name);
        if(name_node && !matches_base) {
          Analyzer::PathTarget alias = analyzer_.ResolvePath(scope, LastComponent(name_node->value));
          TypePtr alias_type = alias.binding ? type_value(alias.binding->type) : TypePtr();
          matches_base = alias_type && alias_type == base;
        }
        if(matches_base) {
          explicitly_initialized_base = true;
          break;
        }
      }
    }
    if(base && !explicitly_initialized_base && HasConstructor(base)) {
      const string this_address = EmitValue(this_node, scope).operand;
      const string base_address = AdjustBaseAddress(this_address, owner, base);
      (void)EmitConstructorAt(base, base_address, vector<CPPGMAstNodePtr>(), scope,
        true, true);
    }
    vector<CPPGMAstNodePtr> ordered_initializers;
    set<const CPPGMAstNode*> used_initializers;
    if(function.special_initializer) {
      for(size_t i = 0; i < function.special_initializer->children.size(); ++i) {
        CPPGMAstNodePtr initializer = function.special_initializer->children[i];
        if(!initializer || initializer->kind != "mem-initializer") continue;
        CPPGMAstNodePtr name_node = ChildOfKind(initializer, "mem-initializer-id");
        const string name = name_node ? LastComponent(name_node->value) : string();
        if(base && (name == LastComponent(base->name) || name == base->name)) {
          ordered_initializers.push_back(initializer);
          used_initializers.insert(initializer.get());
        }
      }
      for(size_t m = 0; m < owner->class_members.size(); ++m) {
        const ClassMemberInfo& member_fact = owner->class_members[m];
        if(member_fact.is_static || member_fact.name.empty() || !member_fact.type) continue;
        CPPGMAstNodePtr selected;
        for(size_t i = 0; i < function.special_initializer->children.size(); ++i) {
          CPPGMAstNodePtr initializer = function.special_initializer->children[i];
          CPPGMAstNodePtr name_node = ChildOfKind(initializer, "mem-initializer-id");
          if(!initializer || initializer->kind != "mem-initializer" || !name_node) continue;
          if(LastComponent(name_node->value) == member_fact.name) {
            selected = initializer;
            break;
          }
        }
        if(selected) {
          ordered_initializers.push_back(selected);
          used_initializers.insert(selected.get());
          continue;
        }
        if(!member_fact.initializer) continue;
        CPPGMAstNodePtr synthetic(new CPPGMAstNode("mem-initializer"));
        synthetic->children.push_back(CPPGMAstNodePtr(
          new CPPGMAstNode("mem-initializer-id", member_fact.name)));
        CPPGMAstNodePtr expression = InitializerExpression(member_fact.initializer);
        if(expression && expression->kind == "braced-init-list") {
          synthetic->children.push_back(expression);
        } else if(member_fact.initializer->children.size() == 1 &&
                  member_fact.initializer->children[0] &&
                  member_fact.initializer->children[0]->kind == "paren-initializer") {
          CPPGMAstNodePtr arguments(new CPPGMAstNode("paren-argument-list"));
          arguments->children = member_fact.initializer->children[0]->children;
          synthetic->children.push_back(arguments);
        } else if(expression) {
          CPPGMAstNodePtr arguments(new CPPGMAstNode("paren-argument-list"));
          arguments->children.push_back(expression);
          synthetic->children.push_back(arguments);
        }
        ordered_initializers.push_back(synthetic);
      }
      for(size_t i = 0; i < function.special_initializer->children.size(); ++i) {
        CPPGMAstNodePtr initializer = function.special_initializer->children[i];
        if(initializer && initializer->kind == "mem-initializer" &&
           used_initializers.find(initializer.get()) == used_initializers.end())
          ordered_initializers.push_back(initializer);
      }
    }
    for(size_t i = 0; i < ordered_initializers.size(); ++i) {
      CPPGMAstNodePtr initializer = ordered_initializers[i];
      if(!initializer || initializer->kind != "mem-initializer") continue;
      CPPGMAstNodePtr name_node = ChildOfKind(initializer, "mem-initializer-id");
      if(!name_node) continue;
      const string name = LastComponent(name_node->value);
      CPPGMAstNodePtr argument_node = ChildOfKind(initializer, "paren-argument-list");
      if(!argument_node) argument_node = ChildOfKind(initializer, "braced-init-list");
      vector<CPPGMAstNodePtr> arguments = argument_node ? argument_node->children :
        vector<CPPGMAstNodePtr>();
      TypePtr named_base;
      if(base && (name == LastComponent(base->name) || name == base->name)) named_base = base;
      if(base && !named_base) {
        Analyzer::PathTarget alias = analyzer_.ResolvePath(scope, name);
        TypePtr alias_type = alias.binding ? type_value(alias.binding->type) : TypePtr();
        if(alias_type && alias_type == base) named_base = base;
      }
      if(named_base) {
        const string this_address = EmitValue(this_node, scope).operand;
        const string base_address = AdjustBaseAddress(this_address, owner, named_base);
        if(!EmitConstructorAt(named_base, base_address, arguments, scope, true, true))
          throw logic_error("base mem-initializer has no constructor");
        continue;
      }
      vector<Binding*> fields = DirectBindings(owner->owned_scope, name);
      Binding* field = 0;
      for(size_t j = 0; j < fields.size(); ++j)
        if(fields[j]->kind == BIND_VARIABLE && fields[j]->is_member && !fields[j]->is_static) {
          field = fields[j];
          break;
        }
      if(!field) throw logic_error("unknown mem-initializer");
      initialized_members.insert(name);
      TypePtr field_type = type_value(field->type);
      Value value;
      string reference_source;
      if(!arguments.empty() && type_is_reference(field->type)) {
        reference_source = EmitReferenceArgument(arguments[0], scope, field->type);
      } else if(!(field_type && field_type->kind == TYPE_CLASS && !arguments.empty()) &&
                !arguments.empty()) {
        if(arguments.size() != 1) throw logic_error("member mem-initializer has too many arguments");
        if(type_is_reference(field->type))
          reference_source = EmitReferenceArgument(arguments[0], scope, field->type);
        else {
          value = EmitValue(arguments[0], scope, field->type);
          value = ConvertValue(value, field->type, false, true);
        }
      }
      CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "OP_ARROW:->"));
      member->children.push_back(this_node);
      member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode("identifier", name)));
      if(IsBitField(field) && !arguments.empty()) {
        bool preserve = false;
        if(field->member_owner && field->member_index != static_cast<size_t>(-1) &&
           field->member_index < field->member_owner->class_members.size()) {
          const long long offset = field->member_owner->class_members[field->member_index].offset;
          for(set<string>::const_iterator it = initialized_members.begin();
              it != initialized_members.end() && !preserve; ++it) {
            if(*it == name) continue;
            vector<Binding*> prior = DirectBindings(owner->owned_scope, *it);
            for(size_t j = 0; j < prior.size(); ++j)
              if(IsBitField(prior[j]) && prior[j]->member_owner &&
                 prior[j]->member_index != static_cast<size_t>(-1) &&
                 prior[j]->member_index < prior[j]->member_owner->class_members.size() &&
                 prior[j]->member_owner->class_members[prior[j]->member_index].offset == offset) {
                preserve = true;
                break;
              }
          }
        }
        string stored;
        if(preserve) {
          const string read_address = EmitMemberAddress(member, scope);
          stored = MergeBitFieldValue(field, read_address, field->type, value.operand, true);
        } else stored = PrepareBitFieldValue(field, field->type, value.operand);
        const string store_address = EmitMemberAddress(member, scope);
        emit_store(field->type, stored, store_address);
        continue;
      }
      const string address = EmitMemberAddress(member, scope);
      if(field_type && field_type->kind == TYPE_ARRAY && arguments.empty() &&
         field_type->bound >= 0 && field_type->child &&
         type_value(field_type->child) && type_value(field_type->child)->kind != TYPE_CLASS) {
        for(size_t element_index = 0;
            element_index < static_cast<size_t>(field_type->bound); ++element_index) {
          const string member_address = element_index == 0 ? address :
            EmitMemberAddress(member, scope);
          const string decay = new_temp();
          AddInstruction(decay + " = unary decay ptr " + member_address);
          const string element = new_temp();
          AddInstruction(element + " = index " + low_type(field_type->child) + " " +
            decay + ", " + integer_text(static_cast<long long>(element_index)));
          emit_store(field_type->child, "0", element);
        }
        continue;
      }
      if(field_type && field_type->kind == TYPE_CLASS && arguments.empty()) {
        const vector<Binding*> constructors =
          MemberBindings(field_type, LastComponent(field_type->name));
        if(!constructors.empty()) {
          // An explicitly empty mem-initializer is value-initialization.  The
          // object ABI models its zero-initialization before invoking the
          // selected default constructor.  Keep this scoped to a real
          // constructor candidate; aggregate/base fallback below has its
          // own typed member initialization rules.
          emit_store(Fundamental("long int"), "0", address);
          if(EmitConstructorAt(field_type, address, arguments, scope)) continue;
        }
        CPPGMAstNodePtr empty(new CPPGMAstNode("braced-init-list"));
        TypePtr nested_base = type_value(field_type->direct_base);
        if(nested_base) {
          const string base_address = address;
          const vector<Binding*> base_constructors =
            MemberBindings(nested_base, LastComponent(nested_base->name));
          if(!base_constructors.empty() && EmitConstructorAt(nested_base,
                                                               base_address, arguments, scope,
                                                               true, true)) {
            // The base constructor performed its own initialization.
          } else EmitAggregateAt(base_address, nested_base, empty, scope,
                                 CPPGMAstNodePtr(), -1, true);
        }
        EmitAggregateAt(address, field_type, empty, scope);
        continue;
      }
      if(field_type && field_type->kind == TYPE_CLASS && !arguments.empty()) {
        if(EmitConstructorAt(field_type, address, arguments, scope)) continue;
        if(argument_node && argument_node->kind == "braced-init-list") {
          CPPGMAstNodePtr aggregate(new CPPGMAstNode("braced-init-list"));
          aggregate->children = arguments;
          EmitAggregateAt(address, field_type, aggregate, scope, member);
          continue;
        }
      }
      if(arguments.empty()) {
        if(!field_type || field_type->kind != TYPE_CLASS)
          emit_store(field->type, "0", address);
        continue;
      }
      if(type_is_reference(field->type)) {
        emit_store(PointerTo(Fundamental("char")), reference_source, address);
      } else if(IsBitField(field)) {
        StoreBitField(field, address, field->type, value.operand, true);
      } else {
        emit_store(field->type, value.operand, address);
      }
    }
    for(size_t i = 0; i < owner->class_members.size(); ++i) {
      const ClassMemberInfo& member_fact = owner->class_members[i];
      if(member_fact.is_static || member_fact.name.empty() || !member_fact.type ||
         initialized_members.find(member_fact.name) != initialized_members.end()) continue;
      vector<Binding*> fields = DirectBindings(owner->owned_scope, member_fact.name);
      Binding* field = 0;
      for(size_t j = 0; j < fields.size(); ++j)
        if(fields[j]->kind == BIND_VARIABLE && fields[j]->is_member && !fields[j]->is_static) {
          field = fields[j];
          break;
        }
      if(!field) continue;
      TypePtr field_type = type_value(field->type);
      CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "OP_ARROW:->"));
      member->children.push_back(this_node);
      member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
        "identifier", member_fact.name)));
      vector<CPPGMAstNodePtr> arguments;
      CPPGMAstNodePtr expression;
      if(member_fact.initializer) {
        if(!member_fact.initializer->children.empty() && member_fact.initializer->children[0] &&
           member_fact.initializer->children[0]->kind == "paren-initializer") {
          arguments = member_fact.initializer->children[0]->children;
          if(arguments.size() == 1) expression = arguments[0];
        } else {
          expression = InitializerExpression(member_fact.initializer);
          if(expression && expression->kind == "braced-init-list")
            arguments = expression->children;
          else if(expression) arguments.push_back(expression);
        }
        if(expression && expression->kind == "call-expression" &&
           !expression->children.empty() && expression->children[0] &&
           expression->children[0]->kind == "id-expression" && field_type &&
           field_type->kind == TYPE_CLASS &&
           LastComponent(field_type->name) == expression->children[0]->value) {
          CPPGMAstNodePtr argument_list = expression->children.size() > 1 ?
            expression->children[1] : CPPGMAstNodePtr();
          arguments = argument_list ? argument_list->children : vector<CPPGMAstNodePtr>();
          expression.reset();
        }
      }
      const bool empty_value_initializer = expression &&
        ((expression->kind == "braced-init-list" && expression->children.empty()) ||
         (expression->kind == "paren-initializer" && expression->children.empty()));
      if(arguments.empty() && (!field_type || field_type->kind != TYPE_CLASS) &&
         !empty_value_initializer) continue;
      if(arguments.empty() && field_type && field_type->kind == TYPE_CLASS &&
         !empty_value_initializer && field_type->class_members.empty() &&
         MemberBindings(field_type, LastComponent(field_type->name)).empty()) continue;
      if(IsBitField(field)) {
        if(arguments.size() != 1) throw logic_error("default member initializer has too many arguments");
        Value value = EmitValue(arguments[0], scope, field->type);
        value = ConvertValue(value, field->type, false, true);
        bool preserve = false;
        if(field->member_owner && field->member_index != static_cast<size_t>(-1) &&
           field->member_index < field->member_owner->class_members.size()) {
          const long long offset = field->member_owner->class_members[field->member_index].offset;
          for(set<string>::const_iterator it = initialized_members.begin();
              it != initialized_members.end() && !preserve; ++it) {
            if(*it == member_fact.name) continue;
            vector<Binding*> prior = DirectBindings(owner->owned_scope, *it);
            for(size_t j = 0; j < prior.size(); ++j)
              if(IsBitField(prior[j]) && prior[j]->member_owner &&
                 prior[j]->member_index != static_cast<size_t>(-1) &&
                 prior[j]->member_index < prior[j]->member_owner->class_members.size() &&
                 prior[j]->member_owner->class_members[prior[j]->member_index].offset == offset) {
                preserve = true;
                break;
              }
          }
        }
        string stored;
        if(preserve) {
          const string read_address = EmitMemberAddress(member, scope);
          stored = MergeBitFieldValue(field, read_address, field->type, value.operand, true);
        } else stored = PrepareBitFieldValue(field, field->type, value.operand);
        const string store_address = EmitMemberAddress(member, scope);
        emit_store(field->type, stored, store_address);
        initialized_members.insert(member_fact.name);
        continue;
      }
      const string address = EmitMemberAddress(member, scope);
      if(field_type && field_type->kind == TYPE_CLASS &&
         EmitConstructorAt(field_type, address, arguments, scope)) {
        initialized_members.insert(member_fact.name);
        continue;
      }
      if(expression && expression->kind == "braced-init-list" &&
         field_type && (field_type->kind == TYPE_CLASS || field_type->kind == TYPE_ARRAY)) {
        EmitAggregateAt(address, field_type, expression, scope, member);
        initialized_members.insert(member_fact.name);
        continue;
      }
      if(arguments.empty()) {
        if(empty_value_initializer && (!field_type || field_type->kind != TYPE_CLASS))
          emit_store(field->type,
            field_type && field_type->kind == TYPE_POINTER ? "nullptr" : "0", address);
        if(empty_value_initializer) initialized_members.insert(member_fact.name);
        continue;
      }
      if(arguments.size() != 1) throw logic_error("default member initializer has too many arguments");
      Value value = type_is_reference(field->type) ? Value() :
        EmitValue(arguments[0], scope, field->type);
      if(type_is_reference(field->type)) {
        emit_store(PointerTo(Fundamental("char")),
          EmitReferenceArgument(arguments[0], scope, field->type), address);
      } else if(IsBitField(field)) {
        StoreBitField(field, address, field->type, value.operand, true);
      } else {
        value = ConvertValue(value, field->type, false, true);
        emit_store(field->type, value.operand, address);
      }
      initialized_members.insert(member_fact.name);
    }

    // A synthesized or user-defined constructor still has to initialize
    // class subobjects for which no mem-initializer or default member
    // initializer was written.  Arrays are walked element-wise so their
    // constructors participate in the same lifetime state as scalar members.
    for(size_t i = 0; i < owner->class_members.size(); ++i) {
      const ClassMemberInfo& member_fact = owner->class_members[i];
      if(member_fact.is_static || member_fact.name.empty() || !member_fact.type ||
         initialized_members.find(member_fact.name) != initialized_members.end()) continue;
      vector<Binding*> fields = DirectBindings(owner->owned_scope, member_fact.name);
      Binding* field = 0;
      for(size_t j = 0; j < fields.size(); ++j)
        if(fields[j]->kind == BIND_VARIABLE && fields[j]->is_member && !fields[j]->is_static) {
          field = fields[j];
          break;
        }
      if(!field) continue;
      TypePtr member_type = type_value(field->type);
      if(member_type && member_type->kind == TYPE_ARRAY) {
        TypePtr element_type = type_value(member_type->child);
        if(!element_type || element_type->kind != TYPE_CLASS || member_type->bound < 0) continue;
        if(element_type->class_members.empty() && !element_type->direct_base &&
           MemberBindings(element_type, LastComponent(element_type->name)).empty())
          continue;
        CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "OP_ARROW:->"));
        member->children.push_back(this_node);
        member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
          "identifier", member_fact.name)));
        for(size_t element_index = 0;
            element_index < static_cast<size_t>(member_type->bound); ++element_index) {
          const string array_address = EmitMemberAddress(member, scope);
          const string decay = new_temp();
          AddInstruction(decay + " = unary decay ptr " + array_address);
          const string offset = new_temp();
          AddInstruction(offset + " = binary mul i64 " +
            integer_text(static_cast<long long>(element_index)) + ", " +
            integer_text(static_cast<long long>(type_size(element_type))));
          const string element = new_temp();
          AddInstruction(element + " = index i8 " + decay + ", " + offset);
          (void)EmitConstructorAt(element_type, element,
            vector<CPPGMAstNodePtr>(), scope);
        }
        continue;
      }
      if(member_type && member_type->kind == TYPE_CLASS) {
        if(member_type->class_members.empty() && !member_type->direct_base &&
           MemberBindings(member_type, LastComponent(member_type->name)).empty())
          continue;
        CPPGMAstNodePtr member(new CPPGMAstNode("member-expression", "OP_ARROW:->"));
        member->children.push_back(this_node);
        member->children.push_back(CPPGMAstNodePtr(new CPPGMAstNode(
          "identifier", member_fact.name)));
        const string address = EmitMemberAddress(member, scope);
        (void)EmitConstructorAt(member_type, address,
          vector<CPPGMAstNodePtr>(), scope);
      }
    }
  }

} // namespace cppgm_pa14_lowering
