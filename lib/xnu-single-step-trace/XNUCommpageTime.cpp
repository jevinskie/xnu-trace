#include "common.h"

#include <cstddef>

#if !defined(__APPLE__) && !defined(__arm64__)
#error bad platform
#endif

struct commpage_timeofday_data {
    uint64_t TimeStamp_tick;
    uint64_t TimeStamp_sec;
    uint64_t TimeStamp_frac;
    uint64_t Ticks_scale;
    uint64_t Ticks_per_sec;
};

constexpr uint64_t _COMM_PAGE_START_ADDRESS     = 0x0000'000F'FFFF'C000ull;
constexpr uint64_t _COMM_PAGE_NEWTIMEOFDAY_DATA = _COMM_PAGE_START_ADDRESS + 0x120;
constexpr uint64_t _COMM_PAGE_NEWTIMEOFDAY_SECONDS =
    _COMM_PAGE_NEWTIMEOFDAY_DATA + offsetof(commpage_timeofday_data, TimeStamp_sec);

uint64_t xnu_commpage_time_seconds() {
    return *(volatile const uint64_t *)_COMM_PAGE_NEWTIMEOFDAY_SECONDS;
}
