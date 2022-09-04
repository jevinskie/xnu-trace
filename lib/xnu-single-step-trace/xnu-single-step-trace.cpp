#include "xnu-single-step-trace/xnu-single-step-trace.h"

#undef NDEBUG
#include <algorithm>
#include <cassert>
#include <climits>
#include <concepts>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <spawn.h>
#include <vector>

#include <libproc.h>
#include <mach-o/dyld_images.h>
#include <mach-o/loader.h>
#include <mach/exc.h>
#include <mach/exception.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/task_info.h>
#include <mach/thread_status.h>
#include <pthread.h>
#include <sys/proc_info.h>

#include <CoreSymbolication/CoreSymbolication.h>

#include <fmt/format.h>
#include <interval-tree/interval_tree.hpp>

#include "mach_exc.h"

namespace fs = std::filesystem;
using namespace std::string_literals;
using namespace lib_interval_tree;

#define EXC_MSG_MAX_SIZE 4096

#ifndef _POSIX_SPAWN_DISABLE_ASLR
#define _POSIX_SPAWN_DISABLE_ASLR 0x0100
#endif

using dispatch_mig_callback_t = boolean_t (*)(mach_msg_header_t *message, mach_msg_header_t *reply);

extern "C" char **environ;

extern "C" mach_msg_return_t dispatch_mig_server(dispatch_source_t ds, size_t maxmsgsz,
                                                 dispatch_mig_callback_t callback);

extern "C" boolean_t mach_exc_server(mach_msg_header_t *message, mach_msg_header_t *reply);
