#ifndef BASE_MAYBE_H
#define BASE_MAYBE_H

#include <cassert>
#include <concepts>
#include <functional>
#include <memory>

#include "base/concepts.h"
#include "base/panic.h"

template <typename T>
struct Maybe {
    constexpr Maybe() {
    }

    template <typename U>
    constexpr Maybe(U &&value)
        requires std::convertible_to<std::remove_cv_t<U>, T>
        : value_{std::forward<U>(value)}, valid_{true} {
    }

    constexpr Maybe(const Maybe &other) {
        if (other.is_valid()) {
            std::construct_at(&value_, other.value_);
            valid_ = true;
        }
    }

    constexpr Maybe &operator=(const Maybe &other) {
        return *std::construct_at(drop(), other);
    }

    constexpr Maybe(Maybe &&other) {
        if (other.is_valid()) {
            std::construct_at(&value_, std::move(other.value_));
            valid_ = true;
        }
    }

    constexpr Maybe &operator=(Maybe &&other) {
        assert(this != &other);
        return *std::construct_at(drop(), std::move(other));
    }

    constexpr ~Maybe() = default;

    constexpr ~Maybe()
        requires(!TriviallyDestructible<T>)
    {
        drop();
    }

    template <typename U>
    constexpr Maybe &operator=(U &&value)
        requires std::convertible_to<std::remove_cv_t<U>, T>
    {
        emplace(std::forward<U>(value));
        return *this;
    }

    [[nodiscard]] constexpr auto &&value(this auto &&self) {
        assert(self.is_valid());
        return std::forward_like<decltype(self)>(self.value_);
    }

    [[nodiscard]] constexpr auto &&expect(this auto &&self,
                                          std::string_view message) {
        if (!self.is_valid()) {
            panic("{}", message);
        }
        return std::forward_like<decltype(self)>(self.value_);
    }

    [[nodiscard]] constexpr auto &&operator*(this auto &&self) {
        return std::forward_like<decltype(self)>(self.value());
    }

    [[nodiscard]] constexpr auto *operator->(this auto &&self) {
        return &self.value();
    }

    [[nodiscard]] constexpr bool is_valid() const {
        return valid_;
    }

    [[nodiscard]] constexpr operator bool() const {
        return is_valid();
    }

    constexpr void reset() {
        drop();
        valid_ = false;
    }

    template <typename... Args>
    constexpr T &emplace(Args &&...args) {
        drop();
        std::construct_at(&value_, std::forward<Args>(args)...);
        valid_ = true;
        return value_;
    }

    template <typename U = T>
    [[nodiscard]] constexpr T value_or(this auto &&self, U &&default_value)
        requires std::convertible_to<std::remove_cv_t<U>, T>
    {
        if (self.is_valid()) {
            return std::forward_like<decltype(self)>(self.value_);
        }
        return std::forward<U>(default_value);
    }

    template <std::invocable Func>
    [[nodiscard]] constexpr T value_or_else(this auto &&self, Func &&func)
        requires std::convertible_to<std::invoke_result_t<Func>, T>
    {
        if (self.is_valid()) {
            return std::forward_like<decltype(self)>(self.value_);
        }
        return std::forward<Func>(func)();
    }

    template <typename Func>
    [[nodiscard]] auto transform(this auto &&self, Func &&func) {
        using SelfType = decltype(self);
        using ValueType = decltype(std::forward_like<SelfType>(self.value()));
        static_assert(std::invocable<Func, ValueType>);
        using ReturnType = std::invoke_result_t<Func, ValueType>;

        if (self.is_valid()) {
            return Maybe<ReturnType>{std::invoke(
                std::forward<Func>(func), static_cast<ValueType>(self.value_))};
        }
        return Maybe<ReturnType>{};
    }

private:
    constexpr Maybe *drop() {
        if constexpr (!TriviallyDestructible<T>) {
            if (is_valid()) {
                value_.~T();
            }
        }
        return this;
    }

    union {
        T value_;
    };

    bool valid_ = false;
};

template <typename T>
    requires std::is_pointer_v<T>
struct Maybe<T> {
    using value_type = std::remove_pointer_t<T>;

    constexpr Maybe() {
    }

    constexpr Maybe(value_type *value) : value_{value} {
    }

    Maybe &operator=(value_type *value) {
        emplace(value);
        return *this;
    }

    [[nodiscard]] constexpr value_type *value() const {
        assert(is_valid());
        return value_;
    }

    [[nodiscard]] constexpr value_type *expect(std::string_view message) const {
        if (!is_valid()) {
            panic("{}", message);
        }
        return value_;
    }

    [[nodiscard]] constexpr value_type *operator*() const {
        return value();
    }

    [[nodiscard]] constexpr value_type *operator->() const {
        return value();
    }

    [[nodiscard]] constexpr bool is_valid() const {
        return value_ != nullptr;
    }

    [[nodiscard]] constexpr operator bool() const {
        return is_valid();
    }

    constexpr void reset() {
        value_ = nullptr;
    }

    constexpr void emplace(value_type *value) {
        value_ = value;
    }

    [[nodiscard]] operator Maybe<const value_type *>() const {
        return Maybe<const value_type *>{value_};
    }

    template <typename U = T>
    [[nodiscard]] constexpr value_type value_or(U &&default_value)
        requires std::convertible_to<std::remove_cv_t<U>, value_type>
    {
        if (is_valid()) {
            return *value_;
        }
        return std::forward<U>(default_value);
    }

    template <std::invocable Func>
    [[nodiscard]] constexpr value_type value_or_else(Func &&func)
        requires std::convertible_to<std::invoke_result_t<Func>, value_type>
    {
        if (is_valid()) {
            return *value_;
        }
        return std::forward<Func>(func)();
    }

    template <typename Func>
    [[nodiscard]] auto transform(Func &&func) {
        static_assert(std::invocable<Func, value_type &>);
        using ReturnType = std::invoke_result_t<Func, value_type &>;

        if (is_valid()) {
            return Maybe<ReturnType>{
                std::invoke(std::forward<Func>(func), *value_)};
        }
        return Maybe<ReturnType>{};
    }

private:
    value_type *value_ = nullptr;
};

template <typename T>
    requires std::is_reference_v<T>
struct Maybe<T> {
    using value_type = std::remove_reference_t<T>;

    constexpr Maybe() {
    }

    constexpr Maybe(value_type &value) : value_{&value} {
    }

    Maybe &operator=(value_type &value) {
        emplace(value);
        return *this;
    }

    [[nodiscard]] constexpr value_type &value() const {
        assert(is_valid());
        return *value_;
    }

    [[nodiscard]] constexpr value_type &expect(std::string_view message) const {
        if (!is_valid()) {
            panic("{}", message);
        }
        return *value_;
    }

    [[nodiscard]] constexpr value_type &operator*() const {
        return value();
    }

    [[nodiscard]] constexpr value_type *operator->() const {
        return &value();
    }

    [[nodiscard]] constexpr bool is_valid() const {
        return value_ != nullptr;
    }

    [[nodiscard]] constexpr operator bool() const {
        return is_valid();
    }

    [[nodiscard]] operator Maybe<const value_type &>() const {
        if (is_valid()) {
            return Maybe<const value_type &>{*value_};
        }
        return {};
    }

    constexpr void reset() {
        value_ = nullptr;
    }

    constexpr void emplace(value_type &value) {
        value_ = &value;
    }

    template <typename U = T>
    [[nodiscard]] constexpr value_type value_or(U &&default_value)
        requires std::convertible_to<std::remove_cv_t<U>, value_type>
    {
        if (is_valid()) {
            return *value_;
        }
        return std::forward<U>(default_value);
    }

    template <std::invocable Func>
    [[nodiscard]] constexpr value_type value_or_else(Func &&func)
        requires std::convertible_to<std::invoke_result_t<Func>, value_type>
    {
        if (is_valid()) {
            return *value_;
        }
        return std::forward<Func>(func)();
    }

    template <typename Func>
    [[nodiscard]] auto transform(Func &&func) {
        static_assert(std::invocable<Func, value_type &>);
        using ReturnType = std::invoke_result_t<Func, value_type &>;

        if (is_valid()) {
            return Maybe<ReturnType>{
                std::invoke(std::forward<Func>(func), *value_)};
        }
        return Maybe<ReturnType>{};
    }

private:
    value_type *value_ = nullptr;
};

#endif