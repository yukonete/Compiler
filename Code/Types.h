#pragma once

#include "Base.h"

namespace Types
{

enum class TypeKind
{
	invalid,

	boolean,
	integer,
	real,

	record,
	pointer
};

struct Type 
{
	TypeKind kind = TypeKind::invalid;
	s64 size = 0;
	s64 align = 0;
	Type(TypeKind kind_) : kind{kind_} {}
};

struct Pointer : public Type 
{
	Pointer() : Type(TypeKind::pointer) {}
	Type *type = nullptr;
};

struct RecordMember
{
	Type *type;
	std::string_view name;
};

struct Record : public Type
{
	Record() : Type(TypeKind::record) {}
	std::span<RecordMember> members;
};

}