#pragma once

namespace file_dialog
{
std::filesystem::path render(std::string_view label);
void open(std::string_view label, std::vector<std::filesystem::path> extensions);
} // namespace file_dialog
