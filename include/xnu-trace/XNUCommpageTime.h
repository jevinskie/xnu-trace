#pragma once

#include "common.h"

#include "utils.h"

#include <mach/mach_time.h>

XNUTRACE_EXPORT XNUTRACE_INLINE uint64_t xnu_commpage_time_seconds();
XNUTRACE_EXPORT XNUTRACE_INLINE uint64_t xnu_commpage_time_atus();
XNUTRACE_EXPORT XNUTRACE_INLINE uint64_t xnu_commpage_time_atus_to_ns(uint64_t atus);

template <typename T>
concept VoidVoidCallback = requires(T cb) {
    { cb() } -> std::same_as<void>;
};

static inline void xnu_fast_timeout_dummy_cb(){};

template <VoidVoidCallback T = decltype(xnu_fast_timeout_dummy_cb)>
class XNUTRACE_EXPORT XNUFastTimeout {
public:
    XNUFastTimeout(uint64_t nanoseconds, T const &cb) : m_cb(cb) {
        mach_timebase_info_data_t tb_info;
        mach_check(mach_timebase_info(&tb_info), "XNUFastTimeout mach_timebase_info");
        const auto num_atus = (double)nanoseconds * tb_info.denom / tb_info.numer;
        m_end_deadline      = xnu_commpage_time_atus() + num_atus;
    };
    XNUFastTimeout(uint64_t nanoseconds) : XNUFastTimeout(nanoseconds, xnu_fast_timeout_dummy_cb){};
    XNUTRACE_INLINE void check() const {
        if (__builtin_expect(xnu_commpage_time_atus() >= m_end_deadline, 0)) {
            m_cb();
        }
    };
    XNUTRACE_INLINE bool done() const {
        return xnu_commpage_time_atus() >= m_end_deadline;
    };

private:
    uint64_t m_end_deadline;
    T const &m_cb;
};