#include "common.h"

#include <spawn.h>

#ifndef _POSIX_SPAWN_DISABLE_ASLR
#define _POSIX_SPAWN_DISABLE_ASLR 0x0100
#endif

#define EXC_MSG_MAX_SIZE 4096

extern "C" char **environ;

using dispatch_mig_callback_t = boolean_t (*)(mach_msg_header_t *message, mach_msg_header_t *reply);

extern "C" mach_msg_return_t dispatch_mig_server(dispatch_source_t ds, size_t maxmsgsz,
                                                 dispatch_mig_callback_t callback);

extern "C" boolean_t mach_exc_trace_server(mach_msg_header_t *message, mach_msg_header_t *reply);

XNUTracer *g_tracer;

XNUTracer::XNUTracer(task_t target_task, const opts &options)
    : m_target_task(target_task), m_log{options.trace_path, options.compression_level,
                                        options.stream} {
    suspend();
    common_ctor(false, false, options.symbolicate);
}

XNUTracer::XNUTracer(pid_t target_pid, const opts &options)
    : m_log{options.trace_path, options.compression_level, options.stream} {
    const auto kr = task_for_pid(mach_task_self(), target_pid, &m_target_task);
    mach_check(kr, fmt::format("task_for_pid({:d}", target_pid));
    suspend();
    common_ctor(false, false, options.symbolicate);
}

XNUTracer::XNUTracer(std::string target_name, const opts &options)
    : m_log{options.trace_path, options.compression_level, options.stream} {
    const auto target_pid = pid_for_name(target_name);
    const auto kr         = task_for_pid(mach_task_self(), target_pid, &m_target_task);
    mach_check(kr, fmt::format("task_for_pid({:d}", target_pid));
    suspend();
    common_ctor(false, false, options.symbolicate);
}

XNUTracer::XNUTracer(std::vector<std::string> spawn_args, bool pipe_ctrl, bool disable_aslr,
                     const opts &options)
    : m_log{options.trace_path, options.compression_level, options.stream} {
    const auto target_pid = spawn_with_args(spawn_args, pipe_ctrl, disable_aslr);
    const auto kr         = task_for_pid(mach_task_self(), target_pid, &m_target_task);
    mach_check(kr, fmt::format("task_for_pid({:d}", target_pid));
    common_ctor(pipe_ctrl, true, options.symbolicate);
}

XNUTracer::~XNUTracer() {
    if (task_is_valid(m_target_task)) {
        set_single_step(false);
    }
    stop_measuring_stats();
    g_tracer                       = nullptr;
    const auto ninst               = logger().num_inst();
    const auto elapsed             = elapsed_time();
    const auto ninst_per_sec       = ninst / elapsed;
    const auto ncsw_self           = context_switch_count_self();
    const auto ncsw_per_sec_self   = ncsw_self / elapsed;
    const auto ncsw_target         = context_switch_count_target();
    const auto ncsw_per_sec_target = ncsw_target / elapsed;
    const auto ncsw_total          = ncsw_self + ncsw_target;
    const auto ncsw_per_sec_total  = ncsw_total / elapsed;
    const auto nbytes              = logger().num_bytes();
    // have to use format/locale, not print/locale
    const auto s = fmt::format(
        std::locale("en_US.UTF-8"),
        "XNUTracer traced {:Ld} instructions in {:0.3Lf} seconds ({:0.1Lf} / sec) over {:Ld} "
        "target / {:Ld} self / {:Ld} total contexts switches ({:0.1Lf} / {:0.1Lf} / {:0.1Lf} "
        "CSW/sec) and logged {:Ld} bytes ({:0.1Lf} / inst, {:0.1Lf} / sec)\n",
        ninst, elapsed, ninst_per_sec, ncsw_target, ncsw_self, ncsw_total, ncsw_per_sec_target,
        ncsw_per_sec_self, ncsw_per_sec_total, nbytes, (double)nbytes / ninst, nbytes / elapsed);
    fmt::print("{}\n", s);
    logger().write(*m_macho_regions, m_symbols.get());
    resume();
}

pid_t XNUTracer::spawn_with_args(const std::vector<std::string> &spawn_args, bool pipe_ctrl,
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
    if (pipe_ctrl) {
        int target2tracer_fds[2];
        assert(!pipe(target2tracer_fds));
        m_target2tracer_fd = target2tracer_fds[0];
        int tracer2target_fds[2];
        assert(!pipe(tracer2target_fds));
        m_tracer2target_fd = tracer2target_fds[1];
        posix_check(
            posix_spawn_file_actions_adddup2(&action, target2tracer_fds[1], pipe_target2tracer_fd),
            "dup pipe target2tracer");
        posix_check(
            posix_spawn_file_actions_adddup2(&action, tracer2target_fds[0], pipe_tracer2target_fd),
            "dup pipe tracer2target");
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
    assert(m_breakpoint_exc_port);
    // Tell the kernel what port to send breakpoint exceptions to.
    const auto kr_set_exc = task_set_exception_ports(
        m_target_task, EXC_MASK_BREAKPOINT, m_breakpoint_exc_port,
        (exception_behavior_t)(EXCEPTION_STATE_IDENTITY | MACH_EXCEPTION_CODES),
        ARM_THREAD_STATE64);
    mach_check(kr_set_exc, "install task_set_exception_ports");
}

void XNUTracer::setup_breakpoint_exception_port_dispath_source() {
    m_breakpoint_exc_source =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, m_breakpoint_exc_port, 0, m_queue);
    assert(m_breakpoint_exc_source);
    dispatch_source_set_event_handler(m_breakpoint_exc_source, ^{
        dispatch_mig_server(m_breakpoint_exc_source, EXC_MSG_MAX_SIZE, mach_exc_trace_server);
    });
}

void XNUTracer::setup_proc_dispath_source() {
    m_proc_source =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, pid(), DISPATCH_PROC_EXIT, m_queue);
    assert(m_proc_source);
}

void XNUTracer::setup_pipe_dispatch_source() {
    m_pipe_source =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, *m_target2tracer_fd, 0, m_queue);
    assert(m_pipe_source);
    dispatch_source_set_event_handler(m_pipe_source, ^{
        uint8_t rbuf;
        assert(read(*m_target2tracer_fd, &rbuf, 1) == 1);
        if (rbuf == 'y') {
            // dispatch here so any pending exceptions in the queue are processed first
            // this ensures we have handled all the single-step exceptions before we
            // uninstall our exception handler
            dispatch_async(m_queue, ^{
                set_single_step(true);
                if (get_suspend_count(m_target_task) == 0 && !m_measuring_stats) {
                    start_measuring_stats();
                }
                const uint8_t cbuf = 'c';
                assert(write(*m_tracer2target_fd, &cbuf, 1) == 1);
            });
        } else if (rbuf == 'n') {
            dispatch_async(m_queue, ^{
                set_single_step(false);
                if (get_suspend_count(m_target_task) == 0 && m_measuring_stats) {
                    stop_measuring_stats();
                }
                const uint8_t cbuf = 'c';
                assert(write(*m_tracer2target_fd, &cbuf, 1) == 1);
            });
        } else {
            assert(!"unhandled");
        }
    });
}

void XNUTracer::uninstall_breakpoint_exception_handler() {
    // Reset the original breakpoint exception port
    if (!task_is_valid(m_target_task)) {
        return;
    }
    const auto kr_set_exc =
        task_set_exception_ports(m_target_task, EXC_MASK_BREAKPOINT, m_orig_breakpoint_exc_port,
                                 m_orig_breakpoint_exc_behavior, m_orig_breakpoint_exc_flavor);
    mach_check(kr_set_exc, "uninstall task_set_exception_ports");
}

void XNUTracer::common_ctor(bool pipe_ctrl, bool was_spawned, bool symbolicate) {
    fmt::print("XNUTracer {:s} PID {:d}\n", was_spawned ? "spawned" : "attached to", pid());
    assert(!g_tracer);
    g_tracer        = this;
    m_macho_regions = std::make_unique<MachORegions>(m_target_task);
    m_vm_regions    = std::make_unique<VMRegions>(m_target_task);
    if (symbolicate) {
        m_symbols = std::make_unique<Symbols>(m_target_task);
    }
    m_queue = dispatch_queue_create("je.vin.tracer", DISPATCH_QUEUE_SERIAL);
    assert(m_queue);
    setup_proc_dispath_source();
    setup_breakpoint_exception_handler();
    if (pipe_ctrl) {
        setup_pipe_dispatch_source();
    }
    setup_breakpoint_exception_port_dispath_source();
    if (!pipe_ctrl) {
        set_single_step(true);
    }
}

dispatch_source_t XNUTracer::proc_dispath_source() {
    assert(m_proc_source);
    return m_proc_source;
}

dispatch_source_t XNUTracer::breakpoint_exception_port_dispath_source() {
    assert(m_breakpoint_exc_source);
    return m_breakpoint_exc_source;
}

dispatch_source_t XNUTracer::pipe_dispatch_source() {
    assert(m_pipe_source);
    return m_pipe_source;
}

void XNUTracer::set_single_step(const bool do_single_step) {
    suspend();
    if (do_single_step) {
        m_macho_regions->reset();
        m_vm_regions->reset();
        if (m_symbols) {
            m_symbols->reset();
        }
        install_breakpoint_exception_handler();
        set_single_step_task(m_target_task, true);
    } else {
        set_single_step_task(m_target_task, false);
        uninstall_breakpoint_exception_handler();
    }
    resume();
    m_single_stepping = do_single_step;
}

pid_t XNUTracer::pid() {
    return pid_for_task(m_target_task);
}

void XNUTracer::suspend() {
    assert(m_target_task);
    if (!task_is_valid(m_target_task)) {
        return;
    }
    const auto suspend_cnt = get_suspend_count(m_target_task);
    mach_check(task_suspend(m_target_task), "suspend() task_suspend");
    if (m_single_stepping && suspend_cnt == 1) {
        stop_measuring_stats();
    }
}

void XNUTracer::resume() {
    assert(m_target_task);
    if (!task_is_valid(m_target_task)) {
        return;
    }
    const auto suspend_cnt = get_suspend_count(m_target_task);
    if (m_single_stepping && suspend_cnt == 1) {
        start_measuring_stats();
    }
    mach_check(task_resume(m_target_task), "resume() task_resume");
}

__attribute__((always_inline)) TraceLog &XNUTracer::logger() {
    return m_log;
}

double XNUTracer::elapsed_time() const {
    return m_elapsed_time;
}

uint64_t XNUTracer::context_switch_count_self() const {
    return m_self_total_csw;
}

uint64_t XNUTracer::context_switch_count_target() const {
    return m_target_total_csw;
}

void XNUTracer::start_measuring_stats() {
    if (!m_measuring_stats) {
        posix_check(clock_gettime(CLOCK_MONOTONIC_RAW, &m_start_time), "clock_gettime");
        m_self_start_num_csw   = get_context_switch_count(getpid());
        m_target_start_num_csw = get_context_switch_count(pid());
        m_measuring_stats      = true;
    }
}

void XNUTracer::stop_measuring_stats() {
    if (m_measuring_stats) {
        timespec current_time;
        posix_check(clock_gettime(CLOCK_MONOTONIC_RAW, &current_time), "clock_gettime");
        m_elapsed_time += timespec_diff(current_time, m_start_time);
        m_self_total_csw += get_context_switch_count(getpid()) - m_self_start_num_csw;
        if (task_is_valid(m_target_task)) {
            m_target_total_csw += get_context_switch_count(pid()) - m_target_start_num_csw;
        }
        m_measuring_stats = false;
    }
}
