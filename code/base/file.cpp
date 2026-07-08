#include "base/maybe.h"
#include "base/file.h"

Maybe<std::string> read_file_to_string(const char *path) {
    if (path == nullptr) {
        return {};
    }

    auto file = std::ifstream{path, std::ios::binary};
    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (!file) {
        return {};
    }

    auto result = std::string(static_cast<usize>(size), '\0');
    if(!file.read(result.data(), result.size())) {
        return {};
    }
    return result;
}


Maybe<std::string> read_file_to_string(std::string_view path) {
    auto zero_terminated_path = create_temp_string(path.size());
    std::ranges::copy(path, zero_terminated_path.begin());
    return read_file_to_string(zero_terminated_path.c_str());
}