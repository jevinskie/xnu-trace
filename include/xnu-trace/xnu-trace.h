#pragma once

#if 0
#include <array>
#include <concepts>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

#include <dispatch/dispatch.h>
#include <frida-gum.h>
#include <interval-tree/interval_tree.hpp>
#include <mach/mach.h>
#include <pthash/pthash.hpp>
#endif

#include "xnu-trace-c.h"

#include "ARM64InstrHistogram.h"
#include "CompressedFile.h"
#include "FridaStalker.h"
#include "MachORegions.h"
#include "Symbols.h"
#include "TraceLog.h"
#include "VMRegions.h"
#include "XNUCommpageTime.h"
#include "XNUTracer.h"
#include "drcov.h"
#include "dyld.h"
#include "log_structs.h"
#include "mach.h"
#include "macho.h"
#include "proc.h"
#include "utils.h"
