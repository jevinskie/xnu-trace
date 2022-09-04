#include "common.h"

// Handle EXCEPTION_STATE_IDENTIY behavior
extern "C" kern_return_t trace_catch_mach_exception_raise_state_identity(
    mach_port_t exception_port, mach_port_t thread, mach_port_t task, exception_type_t exception,
    mach_exception_data_t code, mach_msg_type_number_t code_count, int *flavor,
    thread_state_t old_state, mach_msg_type_number_t old_state_count, thread_state_t new_state,
    mach_msg_type_number_t *new_state_count) {
#pragma unused(exception_port, task, exception, code, code_count, flavor)

    auto os = (const arm_thread_state64_t *)old_state;
    auto ns = (arm_thread_state64_t *)new_state;

    const auto opc = arm_thread_state64_get_pc(*os);
    // fmt::print(stderr, "exc pc: {:p} {:p}\n", (void *)opc);

    *new_state_count = old_state_count;
    *ns              = *os;

    g_tracer->logger().log(thread, opc);

    set_single_step_thread(thread, true);

    return KERN_SUCCESS;
}

// Handle EXCEPTION_DEFAULT behavior
extern "C" kern_return_t trace_catch_mach_exception_raise(mach_port_t exception_port,
                                                          mach_port_t thread, mach_port_t task,
                                                          exception_type_t exception,
                                                          mach_exception_data_t code,
                                                          mach_msg_type_number_t code_count) {
#pragma unused(exception_port, thread, task, exception, code, code_count)
    assert(!"catch_mach_exception_raise not to be called");
    return KERN_NOT_SUPPORTED;
}

// Handle EXCEPTION_STATE behavior
extern "C" kern_return_t trace_catch_mach_exception_raise_state(
    mach_port_t exception_port, exception_type_t exception, const mach_exception_data_t code,
    mach_msg_type_number_t code_count, int *flavor, const thread_state_t old_state,
    mach_msg_type_number_t old_state_count, thread_state_t new_state,
    mach_msg_type_number_t *new_state_count) {
#pragma unused(exception_port, exception, code, code_count, flavor, old_state, old_state_count,    \
               new_state, new_state_count)
    assert(!"catch_mach_exception_raise_state not to be called");
    return KERN_NOT_SUPPORTED;
}
