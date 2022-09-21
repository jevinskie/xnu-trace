#pragma once

#include "common.h"

#include <vector>

#include <mach-o/loader.h>
#include <mach/mach_types.h>

XNUTRACE_EXPORT std::vector<segment_command_64> read_macho_segs_target(task_t target_task,
                                                                       uint64_t macho_hdr_addr);
XNUTRACE_EXPORT std::vector<segment_command_64>
read_macho_segs_target(task_t target_task, const mach_header_64 *macho_hdr);

XNUTRACE_EXPORT uint64_t get_text_size(const std::vector<segment_command_64> &segments);
XNUTRACE_EXPORT uint64_t get_text_base(const std::vector<segment_command_64> &segments);