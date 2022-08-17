#include "xnu-single-step-trace/xnu-single-step-trace.h"

#undef NDEBUG
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>

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

#define EXC_MSG_MAX_SIZE 4096

using dispatch_mig_callback_t = boolean_t (*)(mach_msg_header_t *message, mach_msg_header_t *reply);

extern "C" mach_msg_return_t dispatch_mig_server(dispatch_source_t ds, size_t maxmsgsz,
                                                 dispatch_mig_callback_t callback);

extern "C" boolean_t mach_exc_server(mach_msg_header_t *message, mach_msg_header_t *reply);

template <typename T> size_t bytesizeof(const typename std::vector<T> &vec) {
    return sizeof(T) * vec.size();
}

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
    fmt::print(stderr, "exc pc: {:p}\n", (void *)opc);

    *new_state_count = old_state_count;
    *ns              = *os;

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
        if (proc_pidpath(pid, path_buf, sizeof(path_buf)) > 0) {
            std::filesystem::path path{path_buf};
            if (path.filename().string() == process_name) {
                matches.emplace_back(std::make_pair(path, pid));
            }
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

// XNUTracer class

XNUTracer::XNUTracer(task_t target_task) : m_target_task(target_task) {}

XNUTracer::XNUTracer(pid_t target_pid) {
    assert(task_for_pid(mach_task_self(), target_pid, &m_target_task) == KERN_SUCCESS);
}

XNUTracer::XNUTracer(std::string target_name) {
    const auto target_pid = pid_for_name(target_name);
    assert(task_for_pid(mach_task_self(), target_pid, &m_target_task) == KERN_SUCCESS);
}

void XNUTracer::install_exception_handler() {
    mach_port_t exc_port = MACH_PORT_NULL;
    const auto self_task = mach_task_self();

    // Create the mach port the exception messages will be sent to.
    const auto kr_alloc = mach_port_allocate(self_task, MACH_PORT_RIGHT_RECEIVE, &exc_port);
    assert(kr_alloc == KERN_SUCCESS && "Allocated mach exception port");

    // Insert a send right into the exception port that the kernel will use to
    // send the exception thread the exception messages.
    const auto kr_ins_right =
        mach_port_insert_right(self_task, exc_port, exc_port, MACH_MSG_TYPE_MAKE_SEND);
    assert(kr_ins_right == KERN_SUCCESS && "Inserted a SEND right into the exception port");

    // Tell the kernel what port to send exceptions to.
    const auto kr_set_exc = task_set_exception_ports(
        m_target_task, EXC_MASK_BREAKPOINT, exc_port,
        (exception_behavior_t)(EXCEPTION_STATE_IDENTITY | MACH_EXCEPTION_CODES),
        ARM_THREAD_STATE64);
    assert(kr_set_exc == KERN_SUCCESS && "Set the exception port to my custom handler");
}

void XNUTracer::setup_exception_port_dispath_source() {
    const auto queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    assert(queue);
    m_exc_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, m_exc_port, 0, queue);
    assert(m_exc_source);
    dispatch_source_set_event_handler(
        m_exc_source, ^{ dispatch_mig_server(m_exc_source, EXC_MSG_MAX_SIZE, mach_exc_server); });
}

dispatch_source_t XNUTracer::exception_port_dispath_source() {
    return m_exc_source;
}

void XNUTracer::uninstall_exception_handler() {}
