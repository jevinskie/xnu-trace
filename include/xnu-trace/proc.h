#pragma once

#include "common.h"

#include <string>

XNUTRACE_EXPORT pid_t pid_for_name(std::string process_name);
XNUTRACE_EXPORT int32_t get_context_switch_count(pid_t pid);
