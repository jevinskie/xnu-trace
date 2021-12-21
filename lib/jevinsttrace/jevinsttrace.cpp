#include "jevinsttrace/jevinsttrace.h"

#include <cassert>
#include <cstdio>

#include <mach/exc.h>
#include <mach/exception.h>
#include <mach/mach.h>
#include <pthread.h>

#include "mach_exc.h"

extern "C" boolean_t mach_exc_server(mach_msg_header_t *, mach_msg_header_t *);

static exc_handler_callback_t exc_handler_callback;

void foo() {
    fprintf(stderr, "foo\n");
}

mach_port_t create_exception_port(exception_mask_t exception_mask) {
    mach_port_t exc_port = MACH_PORT_NULL;
    mach_port_t task     = mach_task_self();
    mach_port_t thread   = mach_thread_self();
    kern_return_t kr     = KERN_FAILURE;

    /* Create the mach port the exception messages will be sent to. */
    kr = mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &exc_port);
    assert(kr == KERN_SUCCESS && "Allocated mach exception port");

    /**
     * Insert a send right into the exception port that the kernel will use to
     * send the exception thread the exception messages.
     */
    kr = mach_port_insert_right(task, exc_port, exc_port, MACH_MSG_TYPE_MAKE_SEND);
    assert(kr == KERN_SUCCESS && "Inserted a SEND right into the exception port");

    /* Tell the kernel what port to send exceptions to. */
    kr = thread_set_exception_ports(
        thread, exception_mask, exc_port,
        (exception_behavior_t)(EXCEPTION_STATE_IDENTITY | MACH_EXCEPTION_CODES),
        ARM_THREAD_STATE64);
    assert(kr == KERN_SUCCESS && "Set the exception port to my custom handler");

    return exc_port;
}

extern "C" kern_return_t catch_mach_exception_raise_state(
    mach_port_t exception_port, exception_type_t exception, const mach_exception_data_t code,
    mach_msg_type_number_t code_count, int *flavor, const thread_state_t old_state,
    mach_msg_type_number_t old_state_count, thread_state_t new_state,
    mach_msg_type_number_t *new_state_count) {
#pragma unused(exception_port, exception, code, code_count, flavor, old_state, old_state_count,    \
               new_state, new_state_count)
    assert(!"catch_mach_exception_raise_state not to be called");
    return KERN_NOT_SUPPORTED;
}

extern "C" kern_return_t catch_mach_exception_raise_state_identity(
    mach_port_t exception_port, mach_port_t thread, mach_port_t task, exception_type_t exception,
    mach_exception_data_t code, mach_msg_type_number_t code_count, int *flavor,
    thread_state_t old_state, mach_msg_type_number_t old_state_count, thread_state_t new_state,
    mach_msg_type_number_t *new_state_count) {
#pragma unused(exception_port, thread, task, exception, code, code_count, flavor, old_state,       \
               old_state_count, new_state, new_state_count)
    assert(!"catch_mach_exception_raise_state_identity not to be called");
    return KERN_NOT_SUPPORTED;
}

// Handle EXCEPTION_DEFAULT behavior
extern "C" kern_return_t catch_mach_exception_raise(mach_port_t exception_port, mach_port_t thread,
                                                    mach_port_t task, exception_type_t exception,
                                                    mach_exception_data_t code,
                                                    mach_msg_type_number_t code_count) {
    // Do smart stuff here.
    fprintf(stderr, "My exception handler was called by exception_raise()\n");

    // Inform the kernel that we haven't handled the exception, and the
    // next handler should be called.
    return KERN_FAILURE;
}

/**
 * Thread to handle the mach exception.
 *
 * @param arg The exception port to wait for a message on.
 */
static void *exc_server_thread(void *arg) {
    mach_port_t exc_port = (mach_port_t)(uintptr_t)arg;

    /**
     * mach_msg_server_once is a helper function provided by libsyscall that
     * handles creating mach messages, blocks waiting for a message on the
     * exception port, calls mach_exc_server() to handle the exception, and
     * sends a reply based on the return value of mach_exc_server().
     */
#define MACH_MSG_REPLY_SIZE 4096
    kern_return_t kr = mach_msg_server_once(mach_exc_server, MACH_MSG_REPLY_SIZE, exc_port, 0);
    assert(kr == KERN_SUCCESS && "Received mach exception message");

    pthread_exit((void *)0);
    __builtin_unreachable();
}

void run_exception_handler(mach_port_t exc_port, exc_handler_callback_t callback) {
    exc_handler_callback = callback;

    pthread_t exc_thread;

    /* Spawn the exception server's thread. */
    int err = pthread_create(&exc_thread, (pthread_attr_t *)0, exc_server_thread,
                             (void *)(uintptr_t)exc_port);
    assert(!err && "Spawned exception server thread");

    /* No need to wait for the exception server to be joined when it exits. */
    pthread_detach(exc_thread);
}
