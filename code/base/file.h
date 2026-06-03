#ifndef FILE_H_
#define FILE_H_

#include <string>
#include <filesystem>
#include <fstream>
#include <optional>
#include "base/types.h"

inline std::optional<std::string> read_file_to_string(const char *path) {
    auto err = std::error_code{};
    const auto file_size = std::filesystem::file_size(path, err);
    if (err) {
        return {};
    }

    auto file = std::ifstream{path, std::ios::binary};
    if (!file.is_open()) {
        return {};
    }
 
    auto result = std::string(file_size, '\0');
    if (!file.read(&result[0], file_size)) {
        return {};
    }
    
    return result;
}

inline std::optional<std::string> read_file_to_string(std::string_view path) {
    auto zero_terminated_path = std::string{path};
    return read_file_to_string(zero_terminated_path.c_str());
}

#endif