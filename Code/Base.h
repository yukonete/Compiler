#pragma once

#ifndef BASE_H
#define BASE_H

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <print>
#include <source_location>
#include <span>
#include <string>
#include <type_traits>
#include <stacktrace>
#include <cstdarg>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using f32 = float;
using f64 = double;

using isize = ptrdiff_t;
using usize = size_t;

using uintptr = uintptr_t ;
using intptr = intptr_t;

#define Kilobytes(value) ((value)*1024LL)
#define Megabytes(value) ((Kilobytes(value))*1024LL)
#define Gigabytes(value) ((Megabytes(value))*1024LL)
#define Terabytes(value) ((Gigabytes(value))*1024LL)

#define Assert(condition) assert(condition)

template <typename... Args>
[[noreturn]] void panic(std::format_string<Args...> fmt,
						std::source_location loc,
						Args&&... args)
{
	const auto msg = std::format(fmt, std::forward<Args>(args)...);
	std::println(stderr,
		"Panic: {}.\n"
		"At {}:{}.\n"
        "Stacktrace:\n"
        "{}", msg, loc.file_name(), loc.line(), std::stacktrace::current());
	std::terminate();
}

#define Panic(fmt, ...) panic(fmt, std::source_location::current(), ##__VA_ARGS__);

// Defer macro/thing from Jonathan Blow.
#define CONCAT_INTERNAL(x,y) x##y
#define CONCAT(x,y) CONCAT_INTERNAL(x,y)

template<typename T>
struct ExitScope {
	T lambda;
	ExitScope(T lambda) :lambda(lambda) {}
	~ExitScope() { lambda(); }
	ExitScope(const ExitScope&) = delete;
private:
	ExitScope& operator =(const ExitScope&) = delete;
};

class ExitScopeHelp {
public:
	template<typename T>
	ExitScope<T> operator+(T t) { return t; }
};

#define defer const auto& CONCAT(defer__, __LINE__) = ExitScopeHelp() + [&]()

inline bool IsPowerOfTwo(s64 value)
{
	return (value & (value - 1)) == 0;
}

inline isize AlignForward(isize pointer, isize alignment)
{
	Assert(IsPowerOfTwo(alignment));
	isize modulo = pointer % alignment;
	if (modulo == 0)
	{
		return pointer;
	}
	return pointer + (alignment - modulo);
}

class Arena
{
public:
	constexpr static isize default_size = Megabytes(2);
	constexpr static isize allocation_default_alignment = 2*sizeof(void*);

	Arena(isize size = default_size) : data_(std::make_unique<u8[]>(size)), size_{size} {};

	void* Alloc(isize size, isize alignment = allocation_default_alignment)
	{
		Assert(size >= 0 && alignment >= 0);

		if (size == 0)
		{
			return nullptr;
		}

		const auto base_adress = reinterpret_cast<isize>(data_.get());
		const auto current_pointer = base_adress + offset_;
		const auto mem_offset = AlignForward(current_pointer, alignment) - base_adress;

		if (mem_offset + size > size_)
		{
			Panic("Arena is out of memory");
		}

		offset_ = mem_offset + size;
		return data_.get() + mem_offset;
	};

	template <typename Item, typename ...Args> requires std::is_trivially_copyable_v<Item>
	Item* PushItem(Args&&... args)
	{
		auto pointer = static_cast<Item*>(Alloc(sizeof(Item), alignof(Item)));
		return new(pointer) Item{static_cast<Args&&>(args)...};
	}

	template <typename Item> requires std::is_trivially_copyable_v<Item>
	std::span<Item> PushArray(isize count)
	{
		return std::span<Item>
		{
			PushArrayPointer<Item>(count),
			static_cast<std::span<Item>::size_type>(count)
		};
	}

	template <typename Item> requires std::is_trivially_copyable_v<Item>
	std::span<Item> PushArray(std::span<Item> items)
	{
		auto result = PushArray<Item>(static_cast<isize>(items.size()));
		std::ranges::copy(items, result.begin());
		return result;
	}

	template <typename Item> requires std::is_trivially_copyable_v<Item>
	Item* PushArrayPointer(isize count)
	{
		Assert(count >= 0);
		auto pointer = static_cast<Item*>(Alloc(sizeof(Item) * count, alignof(Item)));
		for (int i = 0; i < count; ++i)
		{
			new(&pointer[i]) Item{};
		}
		return pointer;
	}

	void Clear()
	{
		offset_ = 0;
	}

private:
	std::unique_ptr<u8[]> data_;
	isize size_ = 0;
	isize offset_ = 0;
};

inline std::string ReadEntireFile(const std::string &path)
{
	const auto file_size = std::filesystem::file_size(path);
	auto file = std::ifstream{path, std::ios::binary};
	if (!file.is_open())
	{
		return {};
	}
	auto buf = std::string(file_size, '\0');
	file.read(&buf[0], file_size);
	return buf;
}

#endif
