#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "common-c.h"

template <typename T>
concept POD = std::is_trivial_v<T> && std::is_standard_layout_v<T>;

constexpr uint64_t PAGE_SZ      = 4 * 1024;
constexpr uint64_t PAGE_SZ_LOG2 = 12;
constexpr uint64_t PAGE_SZ_MASK = 0xfff;
