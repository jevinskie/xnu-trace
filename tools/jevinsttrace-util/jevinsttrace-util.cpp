#include <jevinsttrace/jevinsttrace.h>

#include <cassert>
#include <cstdint>
#include <cstdio>

static size_t exc_handler_cb(__unused mach_port_t task, __unused mach_port_t thread,
                             exception_type_t type, mach_exception_data_t codes_64) {
    fprintf(stderr, "exc_handler called\n");
    return 4;
}

int main(void) {
    foo();
    mach_port_t exc_port = create_exception_port(EXC_MASK_ALL);
    assert(exc_port);
    run_exception_handler(exc_port, exc_handler_cb);
    auto crash_ptr = (uint8_t *)0xdeadbeef;
    *crash_ptr     = 42;
    return 0;
}
