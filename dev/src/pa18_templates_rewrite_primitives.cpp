#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

string PA18TemplateExpander::NodeTypeSpelling(const CPPGMAstNodePtr& sequence) const
{
    if(!sequence) return string();
    string result;
    for(size_t i = 0; i < sequence->children.size(); ++i) {
        const CPPGMAstNodePtr child = sequence->children[i];
        if(!child) continue;
        string spelling = RemoveMarker(child->value);
        if(spelling == "friend") continue;
        if(spelling.compare(0, 7, "friend ") == 0) spelling.erase(0, 7);
		if(child->kind == "decl-specifier" &&
		   (spelling == "typedef" || spelling == "static" || spelling == "inline" ||
			spelling == "constexpr" || spelling == "extern" ||
			spelling == "thread_local" || spelling == "register" || spelling == "mutable" ||
			spelling == "virtual" || spelling == "explicit"))
            continue;
        if(child->kind == "class-forward-declaration" || child->kind == "class-specifier")
            spelling = LastComponent(RemoveMarker(child->value));
        else if(child->kind != "decl-specifier" && child->kind != "type-name" &&
                child->kind != "type-specifier" && child->kind != "decltype-specifier" &&
                child->kind != "cv-qualifier") continue;
        if(spelling.empty()) continue;
        if(!result.empty()) result += ' ';
        result += spelling;
    }
    return CanonicalSpelling(result);
}

string PA18TemplateExpander::IntegralValueSpelling(const PA19IntegralValue& value) const
{
    if(!value.known) return string();
    const PA19IntegralType type = value.type;
    if(type.name == "bool") return PA19Raw(value) ? "true" : "false";
    ostringstream result;
    if(type.is_unsigned) result << PA19Raw(value);
    else result << PA19Signed(value);
    if(type.is_unsigned) {
        if(type.bits > 32) result << "ULL";
        else result << "u";
    } else if(type.bits > 32) result << "LL";
    return result.str();
}

string PA18TemplateExpander::TemplateIntegralValueSpelling(const PA19IntegralValue& value) const
{
    if(!value.known) return string();
    if(value.type.name == "bool") return PA19Raw(value) ? "true" : "false";
    ostringstream result;
    if(value.type.is_unsigned) result << PA19Raw(value);
    else result << PA19Signed(value);
    return result.str();
}

string PA18TemplateExpander::ConstantExpressionSpelling(const CPPGMAstNodePtr& node) const
{
    if(!node) return string();
    if(node->kind == "literal" || node->kind == "keyword-literal" ||
       node->kind == "id-expression" || node->kind == "template-id")
        return RemoveMarker(node->value);
    if(node->kind == "parenthesized-expression" && !node->children.empty())
        return "(" + ConstantExpressionSpelling(node->children[0]) + ")";
    if(node->kind == "unary-expression" && !node->children.empty())
        return RemoveMarker(node->value) + ConstantExpressionSpelling(node->children[0]);
    if((node->kind == "binary-expression" || node->kind == "assignment-expression") &&
       node->children.size() >= 2)
        return "(" + ConstantExpressionSpelling(node->children[0]) + " " +
            RemoveMarker(node->value) + " " + ConstantExpressionSpelling(node->children[1]) + ")";
    if(node->kind == "conditional-expression" && node->children.size() >= 3)
        return "(" + ConstantExpressionSpelling(node->children[0]) + " ? " +
            ConstantExpressionSpelling(node->children[1]) + " : " +
            ConstantExpressionSpelling(node->children[2]) + ")";
    if(node->kind == "sizeof-expression" && !node->children.empty())
        return "sizeof(" + ConstantExpressionSpelling(node->children[0]) + ")";
    if(node->kind == "type-trait-expression" && !node->children.empty())
        return "alignof(" + SpellNode(node->children[0]) + ")";
    if(node->kind == "sizeof-pack-expression" && !node->children.empty())
        return "sizeof...(" + ConstantExpressionSpelling(node->children[0]) + ")";
    if(node->kind == "cast-expression" && node->children.size() >= 2)
        return "static_cast<" + SpellNode(node->children[0]) + ">(" +
            ConstantExpressionSpelling(node->children[1]) + ")";
    if(node->kind == "subscript-expression" && node->children.size() >= 2)
        return ConstantExpressionSpelling(node->children[0]) + "[" +
            ConstantExpressionSpelling(node->children[1]) + "]";
    if(node->kind == "call-expression" && !node->children.empty()) {
        string result = ConstantExpressionSpelling(node->children[0]) + "(";
        if(node->children.size() > 1 && node->children[1]) {
            const CPPGMAstNodePtr arguments = node->children[1];
            for(size_t i = 0; i < arguments->children.size(); ++i) {
                if(i) result += ", ";
                result += ConstantExpressionSpelling(arguments->children[i]);
            }
        }
        return result + ")";
    }
    if(node->kind == "member-expression" && node->children.size() >= 2)
        return ConstantExpressionSpelling(node->children[0]) + RemoveMarker(node->value) +
            ConstantExpressionSpelling(node->children[1]);
    return SpellNode(node);
}

bool PA18TemplateExpander::EvaluateInheritedSourceMember(
    const string& raw, const string& context,
    const map<string, string>& substitutions, PA19IntegralValue* result)
{
    if(!result) return false;
    const size_t separator = raw.rfind("::");
    if(separator == string::npos || separator == 0 || separator + 2 >= raw.size()) return false;
    const string owner = raw.substr(0, separator);
    const string member = raw.substr(separator + 2);
    for(size_t character = 0; character < member.size(); ++character)
        if(!IsIdentifierCharacter(member[character])) return false;
    const CPPGMAstNodePtr declaration = FindClassDeclaration(owner, context);
    if(!declaration) return false;
    for(size_t child = 0; child < declaration->children.size(); ++child) {
        const CPPGMAstNodePtr clause = declaration->children[child];
        if(!clause || clause->kind != "base-clause") continue;
        for(size_t base = 0; base < clause->children.size(); ++base) {
            const CPPGMAstNodePtr base_name = ChildOfKindLocal(
                clause->children[base], "base-name");
            if(!base_name) continue;
            string base_spelling = CanonicalSpelling(ReplaceIdentifiers(
                base_name->value, substitutions));
            base_spelling = CanonicalSpelling(RemoveMarker(RewriteText(
                base_spelling, context, substitutions, 0)));
            base_spelling = ResolveAlias(base_spelling, context);
            if(base_spelling.empty() || base_spelling.find("...") != string::npos) continue;
            if(EvaluateIntegralText(base_spelling + "::" + member, context,
                substitutions, result) && result->known) return true;
        }
    }
    return false;
}

bool PA18TemplateExpander::EvaluatePreferredOwnerConstantExpression(
    const string& expression, const string& raw, const string& preferred_owner,
    const map<string, string>& substitutions, PA19IntegralValue* result)
{
    if(!result || expression == raw || preferred_owner.empty() ||
        expression.find("::") == string::npos) return false;
    string expanded = CanonicalSpelling(ReplaceIdentifiers(expression, substitutions));
    bool expanded_any = false;
    for(size_t marker = expanded.find("::"); marker != string::npos; ) {
        if(marker == 0) { marker = expanded.find("::", marker + 2); continue; }
        const size_t member_begin = marker + 2;
        size_t member_end = member_begin;
        while(member_end < expanded.size() && IsIdentifierCharacter(expanded[member_end])) ++member_end;
        if(member_end == member_begin) { marker = expanded.find("::", marker + 2); continue; }
        size_t begin = marker;
        if(begin > 0 && expanded[begin - 1] == '>') {
            int angle_depth = 0;
            while(begin > 0) {
                const char character = expanded[begin - 1];
                if(character == '>') ++angle_depth;
                else if(character == '<' && angle_depth > 0) {
                    --angle_depth; --begin;
                    if(angle_depth == 0) break;
                    continue;
                }
                --begin;
            }
        }
        while(begin > 0 && (IsIdentifierCharacter(expanded[begin - 1]) ||
            expanded[begin - 1] == ':')) --begin;
        const string operand = expanded.substr(begin, member_end - begin);
        PA19IntegralValue operand_value;
        if(!EvaluateInheritedSourceMember(operand, preferred_owner, substitutions,
            &operand_value)) {
            marker = expanded.find("::", member_end);
            continue;
        }
        expanded.replace(begin, member_end - begin, IntegralValueSpelling(operand_value));
        expanded_any = true;
        marker = expanded.find("::", begin);
    }
    if(!expanded_any) return false;
    PA19ConstantExpressionParser parser(constant_values_, substitutions,
        constant_type_sizes_, constant_type_alignments_, type_aliases_);
    return parser.Evaluate(expanded, result);
}

void PA18TemplateExpander::RecordEnumConstants(const CPPGMAstNodePtr& node,
    const string& context)
{
    if(!node || node->kind != "enum-specifier") return;
    long long next = 0;
    const string enum_name = LastComponent(node->value);
    for(size_t i = 0; i < node->children.size(); ++i) {
        const CPPGMAstNodePtr enumerator = node->children[i];
        if(!enumerator || enumerator->kind != "enumerator") continue;
        PA19IntegralValue value = PA19IntegralValue::Signed(next, "int", 32);
        bool evaluated = false;
        if(!enumerator->children.empty()) {
            const string expression = ConstantExpressionSpelling(enumerator->children[0]);
            bool dependent = HasUnresolvedTemplateParameter(expression, context,
                map<string, string>());
            const TemplateDefinition* owner = FindDefinition(context, context);
            if(owner) for(size_t parameter = 0; parameter < owner->parameters.size() &&
                !dependent; ++parameter) {
                const string& name = owner->parameters[parameter].name;
                if(name.empty()) continue;
                for(size_t at = expression.find(name); at != string::npos;
                    at = expression.find(name, at + name.size())) {
                    const bool left = at == 0 || !IsIdentifierCharacter(expression[at - 1]);
                    const size_t end = at + name.size();
                    const bool right = end == expression.size() ||
                        !IsIdentifierCharacter(expression[end]);
                    if(left && right) { dependent = true; break; }
                }
            }
            if(!dependent) evaluated = EvaluateIntegralText(expression, context,
                map<string,string>(), &value);
        }
        if(evaluated && !enumerator->children.empty())
            enumerator->children[0] = CPPGMAstNodePtr(new CPPGMAstNode(
                "literal", TemplateIntegralValueSpelling(value)));
        const string unqualified = JoinPath(context, enumerator->value);
        const string enum_type = enum_name.empty() ? string("int") :
            JoinPath(context, enum_name);
        enumerator_types_[unqualified] = enum_type;
        if(enumerator_types_.find(enumerator->value) == enumerator_types_.end())
            enumerator_types_[enumerator->value] = enum_type;
        if(!enum_name.empty()) enumerator_types_[JoinPath(JoinPath(context, enum_name),
            enumerator->value)] = enum_type;
        constant_values_[unqualified] = value;
        if(constant_values_.find(enumerator->value) == constant_values_.end())
            constant_values_[enumerator->value] = value;
        if(!enum_name.empty()) constant_values_[JoinPath(JoinPath(context, enum_name),
            enumerator->value)] = value;
        next = PA19Signed(value) + 1;
    }
}

bool PA18TemplateExpander::FunctionParameterCounts(const CPPGMAstNodePtr& parameters,
    size_t* total, size_t* required) const
{
    if(!parameters || !total || !required) return false;
    *total = 0;
    *required = 0;
    for(size_t i = 0; i < parameters->children.size(); ++i) {
        const CPPGMAstNodePtr parameter = parameters->children[i];
        if(!parameter || parameter->kind != "parameter-declaration") continue;
        ++*total;
        if(!ChildOfKindLocal(parameter, "default-argument")) ++*required;
    }
    return true;
}

string PA18TemplateExpander::ResolveGeneratedFunctionOwner(const string& owner,
    const string& context, string* child_context) const
{
    if(class_contexts_.find(owner) != class_contexts_.end()) {
        if(child_context) *child_context = owner;
        return owner;
    }
    for(string current = context; ; ) {
        const string candidate = JoinPath(current, owner);
        if(class_contexts_.find(candidate) != class_contexts_.end()) {
            if(child_context) *child_context = candidate;
            return candidate;
        }
        if(current.empty()) break;
        const size_t separator = current.rfind("::");
        if(separator == string::npos) current.clear();
        else current.erase(separator);
    }
    return owner;
}

}
