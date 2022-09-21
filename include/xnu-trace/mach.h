#pragma once

#include "common.h"

XNUTRACE_EXPORT bool task_is_valid(task_t task);
XNUTRACE_EXPORT pid_t pid_for_task(task_t task);
XNUTRACE_EXPORT integer_t get_suspend_count(task_t task);

XNUTRACE_EXPORT int64_t get_task_for_pid_count(task_t task);

XNUTRACE_EXPORT std::vector<uint8_t> read_target(task_t target_task, uint64_t target_addr,
                                                 uint64_t sz);

template <typename T>
XNUTRACE_EXPORT std::vector<uint8_t> read_target(task_t target_task, const T *target_addr,
                                                 uint64_t sz) {
    return read_target(target_task, (uint64_t)target_addr, sz);
}

XNUTRACE_EXPORT std::string read_cstr_target(task_t target_task, uint64_t target_addr);
XNUTRACE_EXPORT std::string read_cstr_target(task_t target_task, const char *target_addr);

XNUTRACE_EXPORT XNUTRACE_INLINE void set_single_step_thread(thread_t thread, bool do_ss);
XNUTRACE_EXPORT void set_single_step_task(task_t task, bool do_ss);