#include "xnu-single-step-trace/xnu-single-step-trace.h"

#undef NDEBUG
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>

#include <dispatch/dispatch.h>
#include <libproc.h>
#include <mach/exc.h>
#include <mach/exception.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/task_info.h>
#include <mach/thread_status.h>
#include <pthread.h>
#include <sys/proc_info.h>

#include <fmt/format.h>

#include "mach_exc.h"

template <typename T> size_t bytesizeof(const typename std::vector<T> &vec) {
    return sizeof(T) * vec.size();
}

static exc_handler_callback_t exc_handler_callback;

mach_port_t create_exception_port(task_t target_task, exception_mask_t exception_mask) {
    mach_port_t exc_port = MACH_PORT_NULL;
    mach_port_t task     = mach_task_self();
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
    kr = task_set_exception_ports(
        target_task, exception_mask, exc_port,
        (exception_behavior_t)(EXCEPTION_STATE_IDENTITY | MACH_EXCEPTION_CODES),
        ARM_THREAD_STATE64);
    assert(kr == KERN_SUCCESS && "Set the exception port to my custom handler");

    return exc_port;
}

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

void set_single_step_thread(thread_t thread, bool do_ss) {
    arm_debug_state64_t dbg_state;
    mach_msg_type_number_t dbg_cnt = ARM_DEBUG_STATE64_COUNT;
    const auto kret_get =
        thread_get_state(thread, ARM_DEBUG_STATE64, (thread_state_t)&dbg_state, &dbg_cnt);
    assert(kret_get == KERN_SUCCESS);

    dbg_state.__mdscr_el1 = (dbg_state.__mdscr_el1 & ~1) | do_ss;

    const auto kret_set =
        thread_set_state(thread, ARM_DEBUG_STATE64, (thread_state_t)&dbg_state, dbg_cnt);
    assert(kret_set == KERN_SUCCESS);
}

void set_single_step_task(task_t task, bool do_ss) {
    thread_act_array_t thread_list;
    mach_msg_type_number_t num_threads = 0;
    assert(task_threads(task, &thread_list, &num_threads) == KERN_SUCCESS);
    for (mach_msg_type_number_t i = 0; i < num_threads; ++i) {
        set_single_step_thread(thread_list[i], do_ss);
    }
}

static unsigned int num_exc;

extern "C" kern_return_t trace_catch_mach_exception_raise_state_identity(
    mach_port_t exception_port, mach_port_t thread, mach_port_t task, exception_type_t exception,
    mach_exception_data_t code, mach_msg_type_number_t code_count, int *flavor,
    thread_state_t old_state, mach_msg_type_number_t old_state_count, thread_state_t new_state,
    mach_msg_type_number_t *new_state_count) {
#pragma unused(exception_port, thread, task, exception, code, code_count, flavor, old_state_count, \
               new_state, new_state_count)

    auto os = (const arm_thread_state64_t *)old_state;
    // auto ns = (arm_thread_state64_t *)new_state;

    const auto opc = arm_thread_state64_get_pc(*os);
    // const auto npc = opc + 4;

    fmt::print(stderr, "exc pc: {:p}\n", (void *)opc);

    // *new_state_count = old_state_count;
    // *ns              = *os;

    // ns->__pc = npc;

    set_single_step_thread(thread, false);

    ++num_exc;

    if (num_exc > 16) {
        fmt::print("bailing due to num_exc count\n");
        exit(0);
    }

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
    kern_return_t kr = mach_msg_server(mach_exc_server, EXC_MSG_MAX_SIZE, exc_port, 0);
    assert(kr == KERN_SUCCESS && "Received mach exception message");

    fmt::print("exc_server_thread ran\n");
    return nullptr;
}

pthread_t run_exception_handler(mach_port_t exc_port, exc_handler_callback_t callback,
                                pthread_mutex_t *should_stop_mtx) {
    (void)should_stop_mtx;
    exc_handler_callback = callback;

    pthread_t exc_thread;

    /* Spawn the exception server's thread. */
    const auto err = pthread_create(&exc_thread, (pthread_attr_t *)0, exc_server_thread,
                                    (void *)(uintptr_t)exc_port);
    assert(!err && "Spawned exception server thread");

    /* No need to wait for the exception server to be joined when it exits. */
    pthread_detach(exc_thread);

    return exc_thread;
}

pid_t pid_for_name(std::string process_name) {
    const auto proc_num = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0) / sizeof(pid_t);
    std::vector<pid_t> pids;
    pids.resize(proc_num);
    const auto actual_proc_num =
        proc_listpids(PROC_ALL_PIDS, 0, pids.data(), (int)bytesizeof(pids)) / sizeof(pid_t);
    assert(actual_proc_num > 0);
    pids.resize(actual_proc_num);
    std::vector<std::pair<std::string, pid_t>> matches;
    for (const auto pid : pids) {
        if (!pid) {
            continue;
        }
        char path_buf[PROC_PIDPATHINFO_MAXSIZE];
        assert(proc_pidpath(pid, path_buf, sizeof(path_buf)) > 0);
        std::filesystem::path path{path_buf};
        if (path.filename().string() == process_name) {
            matches.emplace_back(std::make_pair(path, pid));
        }
    }
    if (!matches.size()) {
        fmt::print(stderr, "Couldn't find process named '{:s}'\n", process_name);
        exit(-1);
    } else if (matches.size() > 1) {
        fmt::print(stderr, "Found multiple processes named '{:s}'\n", process_name);
        exit(-1);
    }
    return matches[0].second;
}

int64_t get_task_for_pid_count(task_t task) {
    struct task_extmod_info info;
    mach_msg_type_number_t count = TASK_EXTMOD_INFO_COUNT;
    const auto kr                = task_info(task, TASK_EXTMOD_INFO, (task_info_t)&info, &count);
    assert(kr == KERN_SUCCESS);
    return info.extmod_statistics.task_for_pid_count;
}

#if 0
void single_stepper_thread(void *arg) {
    thread_t vic_thread = (thread_t)(uintptr_t)arg;
    set_single_step(vic_thread, true);
    pthread_exit((void *)0);
    __builtin_unreachable();
}

void single_step_me() {
    thread_t thread_self = mach_thread_self();

    pthread_t ss_thread;

    const auto err = pthread_create(&ss_thread, (pthread_attr_t *)0, single_stepper_thread,
                             (void *)(uintptr_t)thread_self);
    assert(!err && "Spawned exception server thread");

    /* No need to wait for the exception server to be joined when it exits. */
    pthread_detach(exc_thread);
}
#endif
