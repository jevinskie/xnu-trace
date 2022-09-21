#pragma once

#include <sys/types.h>
#include <unistd.h>

#include "common-c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PIPE_TRACER2TARGET_FD (STDERR_FILENO + 1)
#define PIPE_TARGET2TRACER_FD (STDERR_FILENO + 2)

XNUTRACE_EXPORT void pipe_set_single_step(int do_ss);

typedef void *stalker_t;

XNUTRACE_EXPORT stalker_t create_stalker(const char *log_dir_path, int symbolicate,
                                         int compression_level, int stream);

XNUTRACE_EXPORT void destroy_stalker(stalker_t stalker);

XNUTRACE_EXPORT void stalker_follow_me(stalker_t stalker);

XNUTRACE_EXPORT void stalker_follow_thread(stalker_t stalker, size_t thread_id);

XNUTRACE_EXPORT void stalker_unfollow_me(stalker_t stalker);

XNUTRACE_EXPORT void stalker_unfollow_thread(stalker_t stalker, size_t thread_id);

#ifdef __cplusplus
}
#endif
