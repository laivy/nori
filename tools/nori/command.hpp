#pragma once
#include <string_view>

namespace command
{
void help();
void tree(std::string_view path);
void get(std::string_view path);
void set(std::string_view path, std::string_view value);
void del(std::string_view path);
void exists(std::string_view path);
void diff(std::string_view lhs_path, std::string_view rhs_path);
} // namespace command
