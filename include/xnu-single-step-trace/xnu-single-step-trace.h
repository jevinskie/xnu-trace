#pragma once

#include <pthread.h>
#include <string>
#include <unistd.h>

#include <dispatch/dispatch.h>
#include <mach/mach.h>

#define EXC_MSG_MAX_SIZE 4096

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

using dispatch_mig_callback_t = boolean_t (*)(mach_msg_header_t *message, mach_msg_header_t *reply);

extern "C" mach_msg_return_t dispatch_mig_server(dispatch_source_t ds, size_t maxmsgsz,
                                                 dispatch_mig_callback_t callback);

extern "C" boolean_t mach_exc_server(mach_msg_header_t *message, mach_msg_header_t *reply);

mach_port_t create_exception_port(task_t target_task, exception_mask_t exception_mask);
pthread_t run_exception_handler(mach_port_t exc_port, exc_handler_callback_t callback,
                                pthread_mutex_t *should_stop_mtx);
// void single_step_me();
void set_single_step_thread(thread_t thread, bool do_ss);
void set_single_step_task(task_t thread, bool do_ss);

pid_t pid_for_name(std::string process_name);

int64_t get_task_for_pid_count(task_t task);
