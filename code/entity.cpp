#include "entity.h"

namespace Typing {

std::string_view Entity::name() const {
    constexpr std::string_view NO_NAME = "unnamed entity";
    if (declaration != nullptr) {
        return declaration->identifier->token.value;
    }
    if (is<NamedTypeEntity>()) {
        auto named = as<NamedTypeEntity>();
        auto t = named->type;
        if (t == nullptr) {
            return NO_NAME;
        }
        if (t->is_basic()) {
            return t->as<BasicType>()->name;
        }
        if (t->is<NamedType>()) {
            return t->as<NamedType>()->name;
        }
    }
    return NO_NAME;
}

std::string Entity::full_name() const {
    auto result = std::string{};
    if (parent) {
        result = parent->full_name();
        result += ".";
    }
    result += std::string{name()};
    return result;
}


}