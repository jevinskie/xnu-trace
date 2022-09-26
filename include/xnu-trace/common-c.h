#pragma once

#define XNUTRACE_EXPORT __attribute__((visibility("default")))
#define XNUTRACE_INLINE __attribute__((always_inline))
#define XNUTRACE_LIKELY(cond) __builtin_expect((cond), 1)
#define XNUTRACE_UNLIKELY(cond) __builtin_expect((cond), 0)
