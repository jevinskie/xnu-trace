#pragma once

#include <mach/mach_port.h>

/**
 * Callback invoked by run_exception_handler() when a Mach exception is
 * received.
 *
 * @param task      the task causing the exception
 * @param thread    the task causing the exception
 * @param type      exception type received from the kernel
 * @param codes     exception codes received from the kernel
 *
 * @return      how much the exception handler should advance the program
 *              counter, in bytes (in order to move past the code causing the
 *              exception)
 */
using exc_handler_callback_t = size_t (*)(mach_port_t task, mach_port_t thread,
                                          exception_type_t type, mach_exception_data_t codes);

mach_port_t create_exception_port(exception_mask_t exception_mask);
void run_exception_handler(mach_port_t exc_port, exc_handler_callback_t callback);
// void single_step_me();
void set_single_step(thread_t thread, bool do_ss);
