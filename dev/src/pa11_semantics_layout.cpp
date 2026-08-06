#include "pa11_semantics_analyzer.h"
#include "pa11_semantics_layout.h"
#include <functional>

bool Analyzer::LayoutDependenciesReady(const TypePtr& type) const
{
	if (!type) return true;
	function<bool(const TypePtr&)> layout_ready =
		[&layout_ready](const TypePtr& dependency) -> bool {
			if (!dependency) return true;
			switch (dependency->kind) {
			case TYPE_CLASS: return dependency->complete && dependency->layout_complete;
			case TYPE_ENUM: return dependency->complete;
			case TYPE_ARRAY: return dependency->bound < 0 ||
				layout_ready(dependency->child);
			case TYPE_TEMPLATE_PARAMETER:
			case TYPE_TEMPLATE_TEMPLATE_PARAMETER: return false;
			default: return true;
			}
		};
	if (type->kind != TYPE_CLASS || !type->complete) return false;
	for (size_t base = 0; base < type->direct_bases.size(); ++base)
		if (!layout_ready(type->direct_bases[base])) return false;
	for (size_t member = 0; member < type->class_members.size(); ++member)
		if (!type->class_members[member].is_static &&
			!layout_ready(type->class_members[member].type)) return false;
	return true;
}

void Analyzer::FinishPendingClassLayouts()
{
	while (!pending_class_layouts_.empty())
	{
		bool progress = false;
		for (size_t i = 0; i < pending_class_layouts_.size(); )
		{
			PendingClassLayout pending = pending_class_layouts_[i];
			if (!LayoutDependenciesReady(pending.type))
			{
				++i;
				continue;
			}
			ComputeClassLayout(pending.node, pending.type, pending.scope);
			pending_class_layouts_.erase(pending_class_layouts_.begin() + i);
			progress = true;
		}
		if (progress) continue;
		// Preserve the ordinary incomplete-class diagnostic when a generated
		// specialization has an irreducible by-value dependency cycle.
		const PendingClassLayout pending = pending_class_layouts_.front();
		ComputeClassLayout(pending.node, pending.type, pending.scope);
		break;
	}
}

void Analyzer::ComputeClassMemberLayout(const TypePtr& type, size_t union_size,
	size_t* offset, size_t* maximum_alignment)
{
	if (!type || !offset || !maximum_alignment) return;
	size_t bit_unit_offset = 0;
	size_t bit_unit_size = 0;
	size_t bit_unit_alignment = 1;
	long long bits_used = 0;
	TypePtr bit_unit_type;
	const bool union_type = type->is_union;
	for (size_t i = 0; i < type->class_members.size(); ++i)
	{
		ClassMemberInfo& member = type->class_members[i];
		if (member.is_static || !member.type) continue;
		const size_t member_size = TypeSize(member.type);
		const size_t member_alignment = max<size_t>(1, TypeAlignment(member.type));
		*maximum_alignment = max(*maximum_alignment, member_alignment);
		if (member.bit_field)
		{
			if (!IsBitFieldType(member.type))
				throw logic_error("bit-field type is not integral or enum");
			if (member_size > static_cast<size_t>(numeric_limits<long long>::max() / 8))
				throw logic_error("bit-field storage unit is too large");
			const long long capacity = static_cast<long long>(member_size * 8);
			if (member.bit_width < 0 || member.bit_width > capacity)
				throw logic_error("invalid bit-field width");
			if (member.bit_width == 0 && !member.name.empty())
				throw logic_error("named zero-width bit-field");
		}
		if (union_type)
		{
			member.offset = 0;
			if (member.bit_field) member.bit_offset = 0;
			union_size = max(union_size, member_size);
			continue;
		}
		if (member.bit_field)
		{
			const long long width = member.bit_width;
			const long long capacity = static_cast<long long>(member_size * 8);
			if (width == 0)
			{
				if (bits_used != 0) *offset = bit_unit_offset + bit_unit_size;
				bits_used = 0;
				bit_unit_size = 0;
				bit_unit_type.reset();
				*offset = AlignUp(*offset, member_alignment);
				member.offset = static_cast<long long>(*offset);
				member.bit_offset = 0;
				continue;
			}
			if (bits_used == 0 || bit_unit_size != member_size ||
				bit_unit_alignment != member_alignment || !SameLayoutType(bit_unit_type, member.type) ||
				bits_used + width > capacity)
			{
				if (bits_used != 0) *offset = bit_unit_offset + bit_unit_size;
				*offset = AlignUp(*offset, member_alignment);
				bit_unit_offset = *offset;
				bit_unit_size = member_size;
				bit_unit_alignment = member_alignment;
				bit_unit_type = member.type;
				bits_used = 0;
			}
			member.offset = static_cast<long long>(bit_unit_offset);
			member.bit_offset = bits_used;
			bits_used += width;
			if (bits_used == capacity)
			{
				*offset = bit_unit_offset + bit_unit_size;
				bits_used = 0;
				bit_unit_size = 0;
				bit_unit_type.reset();
			}
			continue;
		}
		if (bits_used != 0)
		{
			*offset = bit_unit_offset + bit_unit_size;
			bits_used = 0;
			bit_unit_size = 0;
			bit_unit_type.reset();
		}
		*offset = AlignUp(*offset, member_alignment);
		member.offset = static_cast<long long>(*offset);
		member.bit_offset = 0;
		*offset += member_size;
	}
	if (bits_used != 0) *offset = bit_unit_offset + bit_unit_size;
	if (union_type) *offset = union_size;
}

namespace {

string PA27AttributeNodeSpelling(const CPPGMAstNodePtr& node)
{
	if (!node) return string();
	if (node->children.empty()) {
		const size_t colon = node->value.find(':');
		if (colon != string::npos &&
			(node->value.compare(0, colon, "TT_") == 0 ||
			 node->value.compare(0, colon, "KW_") == 0 ||
			 node->value.compare(0, colon, "OP_") == 0))
			return node->value.substr(colon + 1);
		return node->value;
	}
	string result;
	for (size_t i = 0; i < node->children.size(); ++i) {
		const string child = PA27AttributeNodeSpelling(node->children[i]);
		if (child.empty()) continue;
		if (!result.empty() && node->kind != "type-id" &&
			node->kind != "type-specifier-seq") result += ' ';
		result += child;
	}
	return result;
}

bool PA27EmptyBaseStorage(const TypePtr& raw_type)
{
	if (!raw_type || raw_type->kind != TYPE_CLASS || raw_type->polymorphic ||
		raw_type->has_vpointer) return false;
	for (size_t i = 0; i < raw_type->class_members.size(); ++i)
		if (!raw_type->class_members[i].is_static &&
			!raw_type->class_members[i].name.empty()) return false;
	const vector<TypePtr> bases = DirectBaseTypes(raw_type);
	for (size_t i = 0; i < bases.size(); ++i)
		if (!PA27EmptyBaseStorage(bases[i])) return false;
	return true;
}

TypePtr PA27FunctionTypeForVirtual(const Binding& binding)
{
	return binding.type && binding.type->kind == TYPE_FUNCTION ? binding.type : TypePtr();
}

bool PA27SameVirtualParameters(const TypePtr& left, const TypePtr& right)
{
	if (!left || !right || left->kind != TYPE_FUNCTION ||
		right->kind != TYPE_FUNCTION || left->variadic != right->variadic ||
		left->function_const != right->function_const ||
		left->function_volatile != right->function_volatile ||
		left->function_lvalue_ref_qualified != right->function_lvalue_ref_qualified ||
		left->function_rvalue_ref_qualified != right->function_rvalue_ref_qualified ||
		left->parameters.size() != right->parameters.size()) return false;
	for (size_t i = 0; i < left->parameters.size(); ++i)
		if (!SameLayoutType(left->parameters[i], right->parameters[i])) return false;
	return true;
}

bool PA27IsDerivedClass(const TypePtr& derived, const TypePtr& base)
{
	const vector<TypePtr> bases = BaseTypeClosure(derived);
	for (size_t i = 1; i < bases.size(); ++i)
		if (SameLayoutType(bases[i], base)) return true;
	return false;
}

bool PA27CovariantVirtualReturn(const TypePtr& derived, const TypePtr& base)
{
	if (!derived || !base || derived->kind != TYPE_FUNCTION ||
		base->kind != TYPE_FUNCTION) return false;
	if (SameLayoutType(derived->child, base->child)) return true;
	const TypePtr derived_return = derived->child;
	const TypePtr base_return = base->child;
	if (!derived_return || !base_return ||
		(derived_return->kind != TYPE_POINTER &&
		 derived_return->kind != TYPE_LVALUE_REFERENCE) ||
		derived_return->kind != base_return->kind || !derived_return->child ||
		!base_return->child) return false;
	return derived_return->child->kind == TYPE_CLASS &&
		base_return->child->kind == TYPE_CLASS &&
		PA27IsDerivedClass(derived_return->child, base_return->child);
}

bool PA27IsDestructorName(const string& name)
{
	return name.size() > 1 && name[0] == '~';
}

bool PA27SameVirtualSlot(const VirtualMethodInfo& slot, const Binding& binding)
{
	const TypePtr function = PA27FunctionTypeForVirtual(binding);
	if (!function) return false;
	if (slot.destructor || PA27IsDestructorName(binding.name))
		return slot.destructor && PA27IsDestructorName(binding.name) &&
			PA27SameVirtualParameters(slot.function, function);
	return slot.name == binding.name &&
		PA27SameVirtualParameters(slot.function, function) &&
		PA27CovariantVirtualReturn(function, slot.function);
}

void PA27AppendVirtualBaseOccurrence(vector<TypePtr>* result,
	vector<TypePtr>* roots, const TypePtr& candidate, const TypePtr& root)
{
	if (!result || !roots || !candidate) return;
	for (size_t i = 0; i < result->size(); ++i)
		if (SameLayoutType((*result)[i], candidate)) return;
	result->push_back(candidate);
	roots->push_back(root ? root : candidate);
}

void PA27AppendVirtualBaseClosure(const TypePtr& type, vector<TypePtr>* result,
	vector<TypePtr>* roots)
{
	if (!type || !result || !roots) return;
	const vector<TypePtr> bases = DirectBaseTypes(type);
	for (size_t i = 0; i < bases.size(); ++i) {
		const TypePtr base = bases[i];
		if (!base) continue;
		if (IsVirtualDirectBase(type, i)) {
			PA27AppendVirtualBaseOccurrence(result, roots, base, base);
			const vector<TypePtr> nested = VirtualBaseTypes(base);
			for (size_t j = 0; j < nested.size(); ++j)
				PA27AppendVirtualBaseOccurrence(result, roots, nested[j], base);
		} else {
			const vector<TypePtr> inherited = VirtualBaseTypes(base);
			const vector<TypePtr> inherited_roots = base->virtual_base_roots;
			for (size_t j = 0; j < inherited.size(); ++j) {
				const TypePtr root = j < inherited_roots.size() &&
					inherited_roots[j] ? inherited_roots[j] : inherited[j];
				PA27AppendVirtualBaseOccurrence(result, roots, inherited[j], root);
			}
		}
	}
}

size_t PA27EmbeddedBaseSize(const TypePtr& base)
{
	return !base || PA27EmptyBaseStorage(base) ? 0 :
		NonVirtualObjectSize(base);
}

bool PA27EmptyVirtualBaseShell(const TypePtr& raw_type)
{
	if (!raw_type || raw_type->kind != TYPE_CLASS || raw_type->polymorphic ||
		raw_type->has_vpointer) return false;
	for (size_t i = 0; i < raw_type->class_members.size(); ++i)
		if (!raw_type->class_members[i].is_static && raw_type->class_members[i].type)
			return false;
	const vector<TypePtr> bases = DirectBaseTypes(raw_type);
	for (size_t i = 0; i < bases.size(); ++i)
		if (!IsVirtualDirectBase(raw_type, i)) return false;
	return true;
}

}

bool Analyzer::ComputeVirtualClassLayout(const TypePtr& type)
{
	if (!type) return false;
	type->virtual_base_types.clear();
	type->virtual_base_roots.clear();
	PA27AppendVirtualBaseClosure(type, &type->virtual_base_types,
		&type->virtual_base_roots);
	type->virtual_base_offsets.assign(type->virtual_base_types.size(), 0);
	if (type->virtual_base_types.empty()) return false;
	type->direct_base_offset = 0;
	type->direct_base_offsets.assign(type->direct_bases.size(), 0);
	size_t offset = 0;
	size_t maximum_alignment = 1;
	const bool owns_vpointer = type->has_vpointer;
	const size_t primary = type->primary_base_index;
	if (owns_vpointer) {
		offset = 8;
		maximum_alignment = 8;
	} else if (primary != static_cast<size_t>(-1) &&
		primary < type->direct_bases.size()) {
		const TypePtr first = type->direct_bases[primary];
		if (first && !PA27EmptyBaseStorage(first)) {
			maximum_alignment = max(maximum_alignment, max<size_t>(1, TypeAlignment(first)));
			type->direct_base_offsets[primary] = 0;
			offset = PA27EmbeddedBaseSize(first);
		}
	}
	for (size_t base_index = 0; base_index < type->direct_bases.size(); ++base_index) {
		const TypePtr base = type->direct_bases[base_index];
		if (base && base->kind == TYPE_CLASS && !base->complete)
			throw logic_error("incomplete direct base class");
		if (IsVirtualDirectBase(type, base_index)) continue;
		if (!base || PA27EmptyBaseStorage(base)) {
			type->direct_base_offsets[base_index] = 0;
			continue;
		}
		const size_t alignment = max<size_t>(1, TypeAlignment(base));
		if (!owns_vpointer && base_index == primary)
			type->direct_base_offsets[base_index] = 0;
		else {
			offset = AlignUp(offset, alignment);
			type->direct_base_offsets[base_index] = offset;
		}
		maximum_alignment = max(maximum_alignment, alignment);
		offset = max(offset, type->direct_base_offsets[base_index] +
			PA27EmbeddedBaseSize(base));
	}
	if (!type->direct_base_offsets.empty())
		type->direct_base_offset = type->direct_base_offsets[0];
	size_t union_size = offset;
	ComputeClassMemberLayout(type, union_size, &offset, &maximum_alignment);
	const size_t nonvirtual_content_end = offset;
	if (offset == 0) {
		offset = 4;
		maximum_alignment = max<size_t>(maximum_alignment, 4);
	}
	type->object_alignment = max(maximum_alignment, type->explicit_alignment);
	if (type->object_alignment == 0) type->object_alignment = 1;
	type->nonvirtual_size = AlignUp(max<size_t>(1, offset), type->object_alignment);
	offset = type->nonvirtual_size;
	set<const Type*> allocated_roots;
	for (size_t virtual_index = 0;
		virtual_index < type->virtual_base_types.size(); ++virtual_index) {
		const TypePtr base = type->virtual_base_types[virtual_index];
		if (!base) continue;
		const TypePtr root = virtual_index < type->virtual_base_roots.size() &&
			type->virtual_base_roots[virtual_index] ?
			type->virtual_base_roots[virtual_index] : base;
		if (allocated_roots.find(base.get()) != allocated_roots.end()) {
			size_t root_offset = 0;
			for (size_t previous = 0; previous < virtual_index; ++previous)
				if (type->virtual_base_roots[previous] &&
					SameLayoutType(type->virtual_base_roots[previous], root)) {
					root_offset = type->virtual_base_offsets[previous];
					break;
				}
			size_t relative = 0;
			if (!SameLayoutType(root, base) &&
				!FindVirtualBaseOffset(root, base, &relative)) relative = 0;
			type->virtual_base_offsets[virtual_index] = root_offset + relative;
			continue;
		}
		allocated_roots.insert(base.get());
		if (PA27EmptyVirtualBaseShell(base)) {
			const size_t empty_offset = nonvirtual_content_end == 0 ?
				1 : type->nonvirtual_size;
			type->virtual_base_offsets[virtual_index] = empty_offset;
			offset = max(offset, empty_offset + 1);
			continue;
		}
		const size_t alignment = max<size_t>(1, TypeAlignment(base));
		offset = AlignUp(offset, alignment);
		type->virtual_base_offsets[virtual_index] = offset;
		const size_t storage_size = HasVirtualBases(base) ?
			max<size_t>(1, NonVirtualObjectSize(base)) : TypeSize(base);
		offset += storage_size;
		maximum_alignment = max(maximum_alignment, alignment);
	}
	for (size_t base_index = 0; base_index < type->direct_bases.size(); ++base_index)
		if (IsVirtualDirectBase(type, base_index))
			for (size_t virtual_index = 0;
				virtual_index < type->virtual_base_types.size(); ++virtual_index)
				if (SameLayoutType(type->direct_bases[base_index],
					type->virtual_base_types[virtual_index])) {
					type->direct_base_offsets[base_index] =
						type->virtual_base_offsets[virtual_index];
					break;
				}
	type->object_alignment = max(maximum_alignment, type->explicit_alignment);
	if (type->object_alignment == 0) type->object_alignment = 1;
	type->object_size = AlignUp(max<size_t>(1, offset), type->object_alignment);
	type->materialize_sizeof_address = false;
	type->layout_complete = true;
	type->layout_in_progress = false;
	return true;
}

void Analyzer::FinalizeVirtualClassMembers(const TypePtr& type, Scope* class_scope)
{
	type->virtual_methods.clear();
	type->virtual_table_views.clear();
	type->primary_base_index = static_cast<size_t>(-1);
	for (size_t base = 0; base < type->direct_bases.size(); ++base)
		if (type->direct_bases[base] && type->direct_bases[base]->polymorphic) {
			const bool virtual_edge = IsVirtualDirectBase(type, base);
			if (!virtual_edge && type->primary_base_index == static_cast<size_t>(-1)) {
				type->primary_base_index = base;
				type->virtual_methods = type->direct_bases[base]->virtual_methods;
			}
			VirtualTableView view;
			view.base = type->direct_bases[base];
			view.base_index = base;
			view.methods = type->direct_bases[base]->virtual_methods;
			type->virtual_table_views.push_back(view);
		}
	const size_t primary_view = type->primary_base_index;
	for (size_t i = 0; i < class_scope->bindings.size(); ++i) {
		Binding& binding = class_scope->bindings[i];
		if (binding.kind != BIND_FUNCTION || !binding.is_member || binding.is_static ||
			binding.member_owner.get() != type.get()) continue;
		const TypePtr function = PA27FunctionTypeForVirtual(binding);
		if (!function) continue;
		bool inherited = false;
		bool inherited_destructor = false;
		for (size_t view = 0; view < type->virtual_table_views.size(); ++view)
			for (size_t slot = 0; slot < type->virtual_table_views[view].methods.size(); ++slot)
				if (PA27SameVirtualSlot(type->virtual_table_views[view].methods[slot], binding)) {
					inherited = true;
					inherited_destructor = inherited_destructor ||
						type->virtual_table_views[view].methods[slot].destructor;
				}
		if (binding.is_override && !inherited)
			throw logic_error("override does not match a base virtual function: " + binding.name);
		if (inherited)
			for (size_t view = 0; view < type->virtual_table_views.size(); ++view)
				for (size_t slot = 0; slot < type->virtual_table_views[view].methods.size(); ++slot)
					if (PA27SameVirtualSlot(type->virtual_table_views[view].methods[slot], binding) &&
						type->virtual_table_views[view].methods[slot].final)
						throw logic_error("override of final virtual function");
		if (!(inherited || binding.is_virtual || binding.is_pure || binding.is_override ||
			binding.is_final)) continue;
		binding.is_virtual = true;
		VirtualMethodInfo effective;
		effective.name = binding.name;
		effective.function = function;
		effective.binding = &binding;
		effective.owner = type;
		effective.destructor = PA27IsDestructorName(binding.name) || inherited_destructor;
		effective.pure = binding.is_pure;
		effective.final = binding.is_final;
		bool replaced = false;
		for (size_t view = 0; view < type->virtual_table_views.size(); ++view)
			for (size_t slot = 0; slot < type->virtual_table_views[view].methods.size(); ++slot)
				if (PA27SameVirtualSlot(type->virtual_table_views[view].methods[slot], binding)) {
					type->virtual_table_views[view].methods[slot] = effective;
					replaced = true;
				}
		for (size_t slot = 0; slot < type->virtual_methods.size(); ++slot)
			if (PA27SameVirtualSlot(type->virtual_methods[slot], binding)) {
				type->virtual_methods[slot] = effective;
				replaced = true;
			}
		if (!replaced || (!inherited && primary_view != static_cast<size_t>(-1)) ||
			(inherited && primary_view == static_cast<size_t>(-1))) {
			type->virtual_methods.push_back(effective);
			for (size_t view = 0; view < type->virtual_table_views.size(); ++view)
				if (type->virtual_table_views[view].base_index == primary_view)
					type->virtual_table_views[view].methods.push_back(effective);
		}
	}
	type->polymorphic = !type->virtual_methods.empty();
	type->has_vpointer = type->polymorphic &&
		(type->primary_base_index == static_cast<size_t>(-1));
}

void Analyzer::RecordDirectClassBases(const CPPGMAstNodePtr& node,
	const TypePtr& type, Scope* owner)
{
	for (size_t i = 0; i < node->children.size(); ++i) {
		const CPPGMAstNodePtr child = node->children[i];
		if (!child || child->kind != "base-clause") continue;
		for (size_t j = 0; j < child->children.size(); ++j) {
			const CPPGMAstNodePtr base = child->children[j];
			if (!base) continue;
			const CPPGMAstNodePtr base_name = ChildOfKind(base, "base-name");
			if (!base_name) continue;
			TypePtr resolved_base = ResolveType(owner, base_name->value);
			if (!resolved_base) continue;
			type->direct_bases.push_back(resolved_base);
			type->direct_base_virtual.push_back(HasKind(base, "virtual"));
			string access = type->tag == "class" ? "private" : "public";
			for (size_t k = 0; k < base->children.size(); ++k) {
				const string spelling = PA27AttributeNodeSpelling(base->children[k]);
				if (spelling.find("public") != string::npos ||
					spelling.find("protected") != string::npos ||
					spelling.find("private") != string::npos) {
					access = spelling.find("public") != string::npos ? "public" :
						spelling.find("protected") != string::npos ? "protected" : "private";
					break;
				}
			}
			type->direct_base_access.push_back(access);
			if (!type->direct_base) type->direct_base = resolved_base;
		}
		break;
	}
}
