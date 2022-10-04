#pragma once

#define XNUTRACE_EXPORT __attribute__((visibility("default")))
#define XNUTRACE_INLINE __attribute__((always_inline))
// #define XNUTRACE_INLINE
#define XNUTRACE_LIKELY(cond) __builtin_expect((cond), 1)
#define XNUTRACE_UNLIKELY(cond) __builtin_expect((cond), 0)
#define XNUTRACE_BREAK() __builtin_debugtrap()
#define XNUTRACE_ALIGNED(n) __attribute__((aligned(n)))
#define XNUTRACE_ASSUME_ALIGNED(ptr, n) __builtin_assume_aligned((ptr), n)
#define XNUTRACE_UNREACHABLE() __builtin_unreachable()
