#include "nsinit_image.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

using namespace std;

namespace {

void AppendPadding(vector<unsigned char>* image, size_t alignment)
{
	if (alignment == 0) throw logic_error("incomplete object alignment");
	while (image->size() % alignment != 0) image->push_back(0);
}

void AppendZeros(vector<unsigned char>* image, size_t size)
{
	image->insert(image->end(), size, 0);
}

size_t AddressOffset(const Address& address)
{
	if (address.entity != NULL)
		return static_cast<size_t>(static_cast<long long>(address.entity->offset) +
			address.addend);
	if (address.temporary != NULL)
		return static_cast<size_t>(static_cast<long long>(address.temporary->offset) +
			address.addend);
	if (address.string != NULL)
		return static_cast<size_t>(static_cast<long long>(address.string->offset) +
			address.addend);
	return 0;
}

void PatchAddress(vector<unsigned char>* image, size_t offset,
	const Address& address)
{
	unsigned long long value = static_cast<unsigned long long>(AddressOffset(address));
	for (size_t i = 0; i < 8; ++i)
	{
		if (offset + i >= image->size()) throw logic_error("relocation outside image");
		(*image)[offset + i] = static_cast<unsigned char>(value & 0xff);
		value >>= 8;
	}
}

void EmitEntity(const Entity& entity, vector<unsigned char>* image)
{
	AppendPadding(image, entity.kind == FUNCTION_ENTITY ? 4 : TypeAlignment(entity.type));
	const size_t offset = image->size();
	const_cast<Entity&>(entity).offset = offset;
	if (entity.kind == FUNCTION_ENTITY)
	{
		image->push_back('f');
		image->push_back('u');
		image->push_back('n');
		image->push_back('\0');
		return;
	}
	const size_t size = TypeSize(entity.type);
	AppendZeros(image, size);
	if (entity.initializer.kind == INIT_BYTES)
	{
		if (entity.initializer.bytes.size() != size)
			throw logic_error("initializer size mismatch");
		copy(entity.initializer.bytes.begin(), entity.initializer.bytes.end(),
			image->begin() + offset);
	}
}

void EmitTemporary(Temporary* temporary, vector<unsigned char>* image)
{
	AppendPadding(image, TypeAlignment(temporary->type));
	temporary->offset = image->size();
	AppendZeros(image, TypeSize(temporary->type));
	if (temporary->initializer.kind == INIT_BYTES)
	{
		if (temporary->initializer.bytes.size() != TypeSize(temporary->type))
			throw logic_error("temporary initializer size mismatch");
		copy(temporary->initializer.bytes.begin(), temporary->initializer.bytes.end(),
			image->begin() + temporary->offset);
	}
}

void EmitString(StringLiteral* string, vector<unsigned char>* image)
{
	AppendPadding(image, TypeAlignment(string->type));
	string->offset = image->size();
	image->insert(image->end(), string->bytes.begin(), string->bytes.end());
}

void PatchEntity(const Entity& entity, vector<unsigned char>* image)
{
	if (entity.kind != VARIABLE_ENTITY || !entity.has_definition) return;
	if (entity.initializer.kind == INIT_ADDRESS)
		PatchAddress(image, entity.offset, entity.initializer.address);
}

void PatchTemporary(const Temporary& temporary, vector<unsigned char>* image)
{
	if (temporary.initializer.kind == INIT_ADDRESS)
		PatchAddress(image, temporary.offset, temporary.initializer.address);
}

void EnsureEntry(Program* program)
{
	for (size_t i = 0; i < program->ordered_entities().size(); ++i)
		if (program->ordered_entities()[i]->kind == FUNCTION_ENTITY &&
			program->ordered_entities()[i]->name == "main") return;
	Type type = Type::Function(vector<Type>(), false, Type::Fundamental("void"));
	Entity* entry = program->AddFunction(program->root(), "__cppgm_entry", type,
		false, -1);
	entry->function_definition = true;
}

} // namespace

void BuildNSInitMockImage(Program* program, vector<unsigned char>* image)
{
	EnsureEntry(program);
	image->clear();
	image->push_back('P');
	image->push_back('A');
	image->push_back('8');
	image->push_back('\0');
	for (size_t i = 0; i < program->ordered_entities().size(); ++i)
	{
		Entity* entity = program->ordered_entities()[i];
		if (entity->kind == VARIABLE_ENTITY && !entity->has_definition) continue;
		if (entity->kind == VARIABLE_ENTITY && TypeSize(entity->type) == 0)
			throw logic_error("object has incomplete type");
		EmitEntity(*entity, image);
	}
	for (size_t i = 0; i < program->temporaries().size(); ++i)
		EmitTemporary(program->temporaries()[i].get(), image);
	for (size_t i = 0; i < program->strings().size(); ++i)
		EmitString(program->strings()[i].get(), image);
	for (size_t i = 0; i < program->ordered_entities().size(); ++i)
		PatchEntity(*program->ordered_entities()[i], image);
	for (size_t i = 0; i < program->temporaries().size(); ++i)
		PatchTemporary(*program->temporaries()[i], image);
}
