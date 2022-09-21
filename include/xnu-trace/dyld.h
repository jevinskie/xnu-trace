#pragma once

#include "common.h"

#include "MachORegions.h"

XNUTRACE_EXPORT std::vector<image_info> get_dyld_image_infos(task_t target_task);
