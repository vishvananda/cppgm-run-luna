#include "pa11_semantics_analyzer.h"
#include "pa11_semantics_layout.h"

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
