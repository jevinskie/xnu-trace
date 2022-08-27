#include "xnu-single-step-trace/xnu-single-step-trace.h"

#undef NDEBUG
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <spawn.h>
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

#ifndef _POSIX_SPAWN_DISABLE_ASLR
#define _POSIX_SPAWN_DISABLE_ASLR 0x0100
#endif

using dispatch_mig_callback_t = boolean_t (*)(mach_msg_header_t *message, mach_msg_header_t *reply);

extern "C" char **environ;

extern "C" mach_msg_return_t dispatch_mig_server(dispatch_source_t ds, size_t maxmsgsz,
                                                 dispatch_mig_callback_t callback);

extern "C" boolean_t mach_exc_server(mach_msg_header_t *message, mach_msg_header_t *reply);

static XNUTracer *g_tracer{nullptr};

template <typename T> size_t bytesizeof(const typename std::vector<T> &vec) {
    return sizeof(T) * vec.size();
}

static void posix_check(int retval, std::string msg) {
    if (retval) {
        fmt::print(stderr, "Error: '{:s}' retval: {:d} errno: {:d} description: '{:s}'\n", msg,
                   retval, errno, strerror(errno));
        exit(-1);
    }
}

static void mach_check(kern_return_t kr, std::string msg) {
    if (kr != KERN_SUCCESS) {
        fmt::print(stderr, "Error: '{:s}' retval: {:d} description: '{:s}'\n", msg, kr,
                   mach_error_string(kr));
        exit(-1);
    }
}

void set_single_step_thread(thread_t thread, bool do_ss) {
    arm_debug_state64_t dbg_state;
    mach_msg_type_number_t dbg_cnt = ARM_DEBUG_STATE64_COUNT;
    const auto kr_thread_get =
        thread_get_state(thread, ARM_DEBUG_STATE64, (thread_state_t)&dbg_state, &dbg_cnt);
    assert(kr_thread_get == KERN_SUCCESS);
    // mach_check(kr_thread_get,
    //            fmt::format("single_step({:s}) thread_get_state", do_ss ? "true" : "false"));

    dbg_state.__mdscr_el1 = (dbg_state.__mdscr_el1 & ~1) | do_ss;

    const auto kr_thread_set =
        thread_set_state(thread, ARM_DEBUG_STATE64, (thread_state_t)&dbg_state, dbg_cnt);
    assert(kr_thread_set == KERN_SUCCESS);
    // mach_check(kr_thread_set,
    //            fmt::format("single_step({:s}) thread_set_state", do_ss ? "true" : "false"));
}

void set_single_step_task(task_t task, bool do_ss) {
    arm_debug_state64_t dbg_state;
    mach_msg_type_number_t dbg_cnt = ARM_DEBUG_STATE64_COUNT;
    const auto kr_task_get =
        task_get_state(task, ARM_DEBUG_STATE64, (thread_state_t)&dbg_state, &dbg_cnt);
    mach_check(kr_task_get, "task_get_state");

    dbg_state.__mdscr_el1 = (dbg_state.__mdscr_el1 & ~1) | do_ss;

    const auto kr_task_set =
        task_set_state(task, ARM_DEBUG_STATE64, (thread_state_t)&dbg_state, dbg_cnt);
    mach_check(kr_task_set, "task_set_state");

    thread_act_array_t thread_list;
    mach_msg_type_number_t num_threads;
    const auto kr_threads = task_threads(task, &thread_list, &num_threads);
    mach_check(kr_threads, "task_threads");
    for (mach_msg_type_number_t i = 0; i < num_threads; ++i) {
        set_single_step_thread(thread_list[i], do_ss);
    }
    const auto kr_dealloc = vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                                          sizeof(thread_act_t) * num_threads);
    mach_check(kr_dealloc, "vm_deallocate");
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

    // const auto opc = arm_thread_state64_get_pc(*os);
    // fmt::print(stderr, "exc pc: {:p}\n", (void *)opc);

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
    mach_check(kr, "get_task_for_pid_count thread_info");
    return info.extmod_statistics.task_for_pid_count;
}

// XNUTracer class

XNUTracer::XNUTracer(task_t target_task) : m_target_task(target_task) {
    common_ctor();
}

XNUTracer::XNUTracer(pid_t target_pid) {
    const auto kr = task_for_pid(mach_task_self(), target_pid, &m_target_task);
    mach_check(kr, fmt::format("task_for_pid({:d}", target_pid));
    common_ctor();
}

XNUTracer::XNUTracer(std::string target_name) {
    const auto target_pid = pid_for_name(target_name);
    const auto kr         = task_for_pid(mach_task_self(), target_pid, &m_target_task);
    mach_check(kr, fmt::format("task_for_pid({:d}", target_pid));
    common_ctor();
}

XNUTracer::XNUTracer(std::vector<std::string> spawn_args, std::optional<int> pipe_fd,
                     bool disable_aslr)
    : m_pipe_write_fd{pipe_fd} {
    const auto target_pid = spawn_with_args(spawn_args, pipe_fd, disable_aslr);
    const auto kr         = task_for_pid(mach_task_self(), target_pid, &m_target_task);
    mach_check(kr, fmt::format("task_for_pid({:d}", target_pid));
    common_ctor();
    const auto kr_resume = task_resume(m_target_task);
    mach_check(kr_resume, "task_resume");
}

XNUTracer::~XNUTracer() {
    uninstall_breakpoint_exception_handler();
    g_tracer = nullptr;
}

pid_t XNUTracer::spawn_with_args(std::vector<std::string> spawn_args, std::optional<int> pipe_fd,
                                 bool disable_aslr) {
    pid_t target_pid{0};
    posix_spawnattr_t attr;
    posix_check(posix_spawnattr_init(&attr), "posix_spawnattr_init");
    short flags = POSIX_SPAWN_START_SUSPENDED | POSIX_SPAWN_CLOEXEC_DEFAULT;
    if (disable_aslr) {
        flags |= _POSIX_SPAWN_DISABLE_ASLR;
    }
    posix_check(posix_spawnattr_setflags(&attr, flags), "posix_spawnattr_setflags");

    posix_spawn_file_actions_t action;
    posix_check(posix_spawn_file_actions_init(&action), "actions init");
    posix_check(posix_spawn_file_actions_adddup2(&action, STDIN_FILENO, STDIN_FILENO), "dup stdin");
    posix_check(posix_spawn_file_actions_adddup2(&action, STDOUT_FILENO, STDOUT_FILENO),
                "dup stdout");
    posix_check(posix_spawn_file_actions_adddup2(&action, STDERR_FILENO, STDERR_FILENO),
                "dup stderr");
    if (pipe_fd != std::nullopt) {
        posix_check(posix_spawn_file_actions_adddup2(&action, *pipe_fd, STDERR_FILENO + 1),
                    "dup pipe");
    }

    assert(spawn_args.size() >= 1);
    std::vector<const char *> argv;
    for (const auto &arg : spawn_args) {
        argv.emplace_back(arg.c_str());
    }
    argv.emplace_back(nullptr);
    posix_check(posix_spawnp(&target_pid, spawn_args[0].c_str(), &action, &attr,
                             (char **)argv.data(), environ),
                "posix_spawnp");
    posix_check(posix_spawn_file_actions_destroy(&action), "posix_spawn_file_actions_destroy");
    posix_check(posix_spawnattr_destroy(&attr), "posix_spawnattr_destroy");
    return target_pid;
}

void XNUTracer::setup_breakpoint_exception_handler() {
    const auto self_task = mach_task_self();

    // Create the mach port the exception messages will be sent to.
    const auto kr_alloc =
        mach_port_allocate(self_task, MACH_PORT_RIGHT_RECEIVE, &m_breakpoint_exc_port);
    mach_check(kr_alloc, "mach_port_allocate");

    // Insert a send right into the exception port that the kernel will use to
    // send the exception thread the exception messages.
    const auto kr_ins_right = mach_port_insert_right(
        self_task, m_breakpoint_exc_port, m_breakpoint_exc_port, MACH_MSG_TYPE_MAKE_SEND);
    mach_check(kr_ins_right, "mach_port_insert_right");

    // Get the old breakpoint exception handler.
    mach_msg_type_number_t old_exc_count = 1;
    exception_mask_t old_exc_mask;
    const auto kr_get_exc =
        task_get_exception_ports(m_target_task, EXC_MASK_BREAKPOINT, &old_exc_mask, &old_exc_count,
                                 &m_orig_breakpoint_exc_port, &m_orig_breakpoint_exc_behavior,
                                 &m_orig_breakpoint_exc_flavor);
    mach_check(kr_get_exc, "install task_get_exception_ports");
    assert(old_exc_count == 1 && old_exc_mask == EXC_MASK_BREAKPOINT);
}

void XNUTracer::install_breakpoint_exception_handler() {
    // Tell the kernel what port to send breakpoint exceptions to.
    const auto kr_set_exc = task_set_exception_ports(
        m_target_task, EXC_MASK_BREAKPOINT, m_breakpoint_exc_port,
        (exception_behavior_t)(EXCEPTION_STATE_IDENTITY | MACH_EXCEPTION_CODES),
        ARM_THREAD_STATE64);
    mach_check(kr_set_exc, "install task_set_exception_ports");
}

void XNUTracer::setup_breakpoint_exception_port_dispath_source() {
    const auto queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    assert(queue);
    m_breakpoint_exc_source =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, m_breakpoint_exc_port, 0, queue);
    assert(m_breakpoint_exc_source);
    dispatch_source_set_event_handler(m_breakpoint_exc_source, ^{
        dispatch_mig_server(m_breakpoint_exc_source, EXC_MSG_MAX_SIZE, mach_exc_server);
    });
}

void XNUTracer::uninstall_breakpoint_exception_handler() {
    // Reset the original breakpoint exception port
    const auto kr_set_exc =
        task_set_exception_ports(m_target_task, EXC_MASK_BREAKPOINT, m_orig_breakpoint_exc_port,
                                 m_orig_breakpoint_exc_behavior, m_orig_breakpoint_exc_flavor);
    mach_check(kr_set_exc, "uninstall task_set_exception_ports");
}

void XNUTracer::common_ctor() {
    assert(!g_tracer);
    g_tracer = this;
    setup_breakpoint_exception_handler();
    install_breakpoint_exception_handler();
    setup_breakpoint_exception_port_dispath_source();
    set_single_step_task(m_target_task, true);
}

dispatch_source_t XNUTracer::breakpoint_exception_port_dispath_source() {
    return m_breakpoint_exc_source;
}

void XNUTracer::set_single_step(const bool do_single_step) {
    if (do_single_step) {
        setup_breakpoint_exception_handler();
        set_single_step_task(m_target_task, true);
    } else {
        set_single_step_task(m_target_task, false);
        uninstall_breakpoint_exception_handler();
    }
}
