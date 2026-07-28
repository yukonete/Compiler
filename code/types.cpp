#include "types.h"
#include "base/util.h"
#include "entity.h"

namespace Typing {

static void append(std::string &out, std::string_view str) {
    out += str;
}

template <typename T>
void appendf(std::string &out, const T &value) {
    std::format_to(std::back_inserter(out), "{}", value);
} 

void Type::dump(std::string &out) const {
    switch (kind) {
        using enum Type::Kind;

        case BASIC: {
            auto basic = as<BasicType>();
            append(out, basic->name);
            return;
        }
        case POINTER: {
            auto pointer = as<PointerType>();
            append(out, "*");
            pointer->type->dump(out);
            return;
        }
        case ARRAY: {
            auto array = as<ArrayType>();
            append(out, "[");
            appendf(out, array->count);
            append(out, "]");
            array->type->dump(out);
            return;
        }
        case SLICE: {
            auto slice = as<SliceType>();
            append(out, "[");
            append(out, "]");
            slice->type->dump(out);
            return;
        }
        case STRUCT: {
            auto type = as<StructType>();
            append(out, "struct{");
            for (auto i : indices(type->members.size())) {
                if (i != 0) {
                    append(out, " ");
                }
                auto member = type->members[i];
                append(out, member->name());
                append(out, ": ");
                member->type->dump(out);
                append(out, ";");
            }
            append(out, "}");
            return;
        }
        case NAMED: {
            auto named = as<NamedType>();
            append(out, named->name);
            return;
        }
        case PROCEDURE: {
            auto proc = as<ProcedureType>();
            append(out, "fn(");
            for (auto i : indices(proc->parameters.size())) {
                if (i != 0) {
                    append(out, ", ");
                }
                auto parameter = proc->parameters[i];
                parameter->dump(out);
            }
            append(out, ")");
            if (proc->return_type) {
                append(out, " -> ");
                proc->return_type->dump(out);
            }
            return;
        }
    }
}


Type *Type::get_base_type() {
    if (is<NamedType>()) {
        return as<NamedType>()->type->get_base_type();
    }
    return this;
}

const Type *Type::get_base_type() const {
    if (is<NamedType>()) {
        return as<NamedType>()->type->get_base_type();
    }
    return this;
}

Type *Type::get_core_type() {
    auto base = get_base_type();
    if (base->is<ArrayType>()) {
        return base->as<ArrayType>()->type;
    }
    if (base->is<SliceType>()) {
        return base->as<SliceType>()->type;
    }
    if (base->is<PointerType>()) {
        return base->as<PointerType>()->type;
    }
    return base;
}

const Type *Type::get_core_type() const {
    auto base = get_base_type();
    if (base->is<ArrayType>()) {
        return base->as<ArrayType>()->type;
    }
    if (base->is<SliceType>()) {
        return base->as<SliceType>()->type;
    }
    if (base->is<PointerType>()) {
        return base->as<PointerType>()->type;
    }
    return base;
}

bool Type::is_integer() const {
    auto base = get_base_type();
    if (base->is<BasicType>()) {
        auto basic = base->as<BasicType>();
        if (has_flag(basic->basic_flags, BasicType::BasicFlags::INTEGER)) {
            return true;
        }
    }
    return false;
}

bool Type::is_pointer() const {
    auto base = get_base_type();
    return base->is<PointerType>();    
}

bool Type::is_unsigned() const {
    auto base = get_base_type();
    if (base->is<BasicType>()) {
        auto basic = base->as<BasicType>();
        if (has_flag(basic->basic_flags, BasicType::BasicFlags::UNSIGNED)) {
            return true;
        }
    }
    return false;
}

bool Type::is_float() const {
    auto base = get_base_type();
    if (base->is<BasicType>()) {
        auto basic = base->as<BasicType>();
        if (has_flag(basic->basic_flags, BasicType::BasicFlags::FLOAT)) {
            return true;
        }
    }
    return false;
}

bool Type::is_numeric() const {
    auto base = get_base_type();
    if (base->is<BasicType>()) {
        auto basic = base->as<BasicType>();
        if (has_flag(basic->basic_flags, BasicType::BasicFlags::INTEGER) ||
            has_flag(basic->basic_flags, BasicType::BasicFlags::FLOAT)) {
            return true;
        }
    }
    return false;
}

bool Type::is_bool() const {
    auto base = get_base_type();
    if (base->is<BasicType>()) {
        auto basic = base->as<BasicType>();
        if (has_flag(basic->basic_flags, BasicType::BasicFlags::BOOL)) {
            return true;
        }
    }
    return false;
}

bool Type::is_bad() const {
    auto base = get_base_type();
    if (base->is<BasicType>()) {
        auto basic = base->as<BasicType>();
        if (basic->basic_flags == BasicType::BasicFlags::BAD) {
            return true;
        }
    }
    return false;
}

bool Type::is_basic() const {
    auto base = get_base_type();
    return base->is<BasicType>();
}

bool Type::is_string() const {
    auto base = get_base_type();
    if (base->is<BasicType>()) {
        auto basic = base->as<BasicType>();
        if (has_flag(basic->basic_flags, BasicType::BasicFlags::STRING)) {
            return true;
        }
    }
    return false;
}

bool Type::is_array() const {
    auto base = get_base_type();
    return base->is<ArrayType>();
}

bool Type::is_struct() const {
    auto base = get_base_type();
    return base->is<StructType>();
}

bool Type::is_slice() const {
    auto base = get_base_type();
    return base->is<SliceType>();
}

bool Type::is_procedure() const {
    auto base = get_base_type();
    return base->is<ProcedureType>();
}

bool Type::is_convertible_to(const Type *type) const {
    auto base = get_base_type();
    type = type->get_base_type();
    if (are_types_the_same(base, type)) {
        return true;
    }

    if (base->kind != type->kind) {
        return false;
    }

    switch (base->kind) {
        using enum Type::Kind;

        case BASIC: {
            if (base->is_bad() || type->is_bad()) {
                // Only one of the types is bad because of check before
                return false;
            }
            if (base->is_numeric()) {
                if (type->is_numeric()) {
                    return true;
                }
                return false;
            }
            if (base->is_bool()) {
                if (type->is_integer()) {
                    return true;
                }
                return false;
            }
            return false;
        }

        case POINTER:
        case ARRAY:
        case SLICE:
        case STRUCT:
        case PROCEDURE:
        case NAMED:
            return false;
        default: panic("Type not handled");
    }
}

bool are_types_the_same(const Type *a, const Type *b) {
    if (a == b) {
        return true;
    }

    if (a == nullptr || b == nullptr) {
        return false;
    }

    while (a->is<NamedType>()) {
        auto named_type = a->as<NamedType>(); 
        auto entity = named_type->entity;
        if (entity->is_alias) {
            a = named_type->type;
        } else {
            break;
        }
    }

    while (b->is<NamedType>()) {
        auto named_type = b->as<NamedType>(); 
        auto entity = named_type->entity;
        if (entity->is_alias) {
            b = named_type->type;
        } else {
            break;
        }
    }

    if (a == b) {
        return true;
    }

    if (a->kind != b->kind) {
        return false;
    }

    switch (a->kind) {
        using enum Type::Kind;
        
        case NAMED:
        case BASIC: {
            // If types are the same, check for a == b above should have already returned true
            return false;
        }
        case POINTER: {
            auto pointer_a = a->as<PointerType>();    
            auto pointer_b = b->as<PointerType>();
            return are_types_the_same(pointer_a->type, pointer_b->type);    
        }
        case ARRAY: {
            auto array_a = a->as<ArrayType>();    
            auto array_b = b->as<ArrayType>();
            if (array_a->count != array_b->count) {
                return false;
            }
            if (!are_types_the_same(array_a->type, array_b->type)) {
                return false;
            }
            return true; 
        }
        case SLICE: {
            auto slice_a = a->as<SliceType>();    
            auto slice_b = b->as<SliceType>();
            return are_types_the_same(slice_a->type, slice_b->type); 
        }
        case STRUCT: {
            auto struct_a = a->as<StructType>();    
            auto struct_b = b->as<StructType>();
            if (struct_a->members.size() != struct_b->members.size()) {
                return false;
            }
            for (usize i = 0; i < struct_a->members.size(); ++i) {
                auto member_a = struct_a->members[i];
                auto member_b = struct_b->members[i];
                if (member_a->declaration->identifier->token.value != member_b->declaration->identifier->token.value) {
                    return false;
                }
                if (!are_types_the_same(member_a->type, member_b->type)) {
                    return false;
                }
            }
            return true;
        }
        case PROCEDURE: {
            auto proc_a = a->as<ProcedureType>();    
            auto proc_b = b->as<ProcedureType>();
            if (proc_a->parameters.size() != proc_b->parameters.size()) {
                return false;
            }
            for (usize i = 0; i < proc_a->parameters.size(); ++i) {
                auto parameter_a = proc_a->parameters[i];
                auto parameter_b = proc_b->parameters[i];
                if (!are_types_the_same(parameter_a, parameter_b)) {
                    return false;
                }
            }
            if (proc_a->return_type && proc_b->return_type) {
                return are_types_the_same(*proc_a->return_type, *proc_b->return_type);
            }
            return static_cast<bool>(proc_a->return_type) == static_cast<bool>(proc_b->return_type);
        }
    }
    panic("Type is not handled");
}

std::string type_to_string(Type *type) {
    if (type == nullptr) {
        return {};
    }
    auto result = std::string {};
    type->dump(result);
    return result;
}

}