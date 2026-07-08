#ifndef BASE_FILE_H
#define BASE_FILE_H

#include <string>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <string_view>
#include <cstdio>

#include "base/types.h"
#include "base/maybe.h"
#include "base/arena.h"

Maybe<std::string> read_file_to_string(const char *path);
Maybe<std::string> read_file_to_string(std::string_view path);

#endif