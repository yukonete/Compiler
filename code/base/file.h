#ifndef FILE_H_
#define FILE_H_

#include <string>
#include <filesystem>
#include <fstream>
#include "base/types.h"

struct ReadFileToStringResult {
    std::string content;
    bool ok = false;
};

inline ReadFileToStringResult read_file_to_string(const char *path) {
    auto err = std::error_code{};
    const auto file_size = std::filesystem::file_size(path, err);
    if (err) {
        return {};
    }

    auto file = std::ifstream{path, std::ios::binary};
    if (!file.is_open()) {
        return {};
    }

    auto result = ReadFileToStringResult{.content = std::string(file_size, '\0')};
    file.read(&result.content[0], file_size);
    if (!file.fail()) {
        result.ok = true;
    }
    return result;
}

#endif