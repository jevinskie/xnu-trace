#pragma once

#undef NDEBUG
#include <cassert>

#include "xnu-trace/xnu-trace.h"

#include <mach-o/dyld_images.h>
#include <mach-o/loader.h>

#include <BS_thread_pool.hpp>
#include <fmt/format.h>

namespace fs = std::filesystem;
using namespace std::string_literals;
using namespace lib_interval_tree;

extern XNUTracer *g_tracer;

template <typename T> size_t bytesizeof(const typename std::vector<T> &vec) {
    return sizeof(T) * vec.size();
}

template <typename U>
requires requires() {
    requires std::unsigned_integral<U>;
}
constexpr U roundup_pow2_mul(U num, std::size_t pow2_mul) {
    const U mask = static_cast<U>(pow2_mul) - 1;
    return (num + mask) & ~mask;
}

template <typename U>
requires requires() {
    requires std::unsigned_integral<U>;
}
constexpr U rounddown_pow2_mul(U num, std::size_t pow2_mul) {
    const U mask = static_cast<U>(pow2_mul) - 1;
    return num & ~mask;
}

// utils.cpp

void zstd_check(size_t retval, const std::string &msg);
uint32_t get_num_cores();
double timespec_diff(const timespec &a, const timespec &b);
void write_file(std::string path, const uint8_t *buf, size_t sz);
std::vector<uint8_t> read_file(std::string path);
std::string prot_to_str(vm_prot_t prot);
void hexdump(const void *data, size_t size);
std::string block_str(double percentage, unsigned int width = 80);

// mach.cpp

bool task_is_valid(task_t task);
std::vector<uint8_t> read_target(task_t target_task, uint64_t target_addr, uint64_t sz);

template <typename T>
std::vector<uint8_t> read_target(task_t target_task, const T *target_addr, uint64_t sz) {
    return read_target(target_task, (uint64_t)target_addr, sz);
}

std::string read_cstr_target(task_t target_task, uint64_t target_addr);
std::string read_cstr_target(task_t target_task, const char *target_addr);

void set_single_step_thread(thread_t thread, bool do_ss);
void set_single_step_task(task_t task, bool do_ss);

// macho.cpp

std::vector<segment_command_64> read_macho_segs_target(task_t target_task, uint64_t macho_hdr_addr);
std::vector<segment_command_64> read_macho_segs_target(task_t target_task,
                                                       const mach_header_64 *macho_hdr);
uint64_t get_text_size(const std::vector<segment_command_64> &segments);
uint64_t get_text_base(const std::vector<segment_command_64> &segments);
