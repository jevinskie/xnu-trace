#include <jevinsttrace/jevinsttrace.h>

#include <cassert>
#include <cstdint>
#include <cstdio>

#include <mach/mach.h>

#include <fmt/core.h>

static size_t exc_handler_cb(__unused mach_port_t task, __unused mach_port_t thread,
                             exception_type_t type, mach_exception_data_t codes_64) {
    fprintf(stderr, "exc_handler called\n");
    return 4;
}

int main(void) {
    fmt::print(stderr, "jevinsttrace-util begin\n");

    mach_port_t exc_port = create_exception_port(EXC_MASK_ALL);
    assert(exc_port);
    run_exception_handler(exc_port, exc_handler_cb);
    set_single_step(mach_thread_self(), true);
    // auto crash_ptr = (volatile uint8_t *)0xdeadbeef;
    // *crash_ptr     = 42;

    // fmt::print(stderr, "crash_ptr: {:d}\n", *crash_ptr);

    fmt::print(stderr, "jevinsttrace-util end\n");
    return 0;
}
