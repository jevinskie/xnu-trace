#include "xnu-trace/xnu-trace.h"

#undef NDEBUG
#include <cassert>
#include <unistd.h>

#include <fmt/format.h>

int main(void) {

    auto timeout_done = XNUFastTimeout(1'000'000'000);
    for (int i = 0; i < 25 && !timeout_done.done(); ++i) {
        fmt::print("done xnu_commpage_time_atus(): {:d}\n", xnu_commpage_time_atus());
        usleep(250'000);
    }

    auto timeout_check = XNUFastTimeout(1'000'000'000, [&]() {
        assert(!"timed out");
    });
    for (int i = 0; i < 25; ++i) {
        fmt::print("check xnu_commpage_time_atus(): {:d}\n", xnu_commpage_time_atus());
        usleep(250'000);
        timeout_check.check();
    }
    return 0;
}
