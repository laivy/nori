#pragma once

#include <string_view>

void arm_console_fault(std::string_view mode);
bool was_console_fault_triggered();
