#pragma once

#include "common.h"

class XNUTRACE_EXPORT XNUTracer {
public:
    struct opts {
        std::string trace_path;
        bool symbolicate;
        int compression_level;
        bool stream;
    };

    XNUTracer(task_t target_task, const opts &options);
    XNUTracer(pid_t target_pid, const opts &options);
    XNUTracer(std::string target_name, const opts &options);
    XNUTracer(std::vector<std::string> spawn_args, bool pipe_ctrl, bool disable_aslr,
              const opts &options);
    ~XNUTracer();

    void suspend();
    void resume();
    pid_t pid();
    dispatch_source_t proc_dispath_source();
    dispatch_source_t breakpoint_exception_port_dispath_source();
    dispatch_source_t pipe_dispatch_source();
    void set_single_step(bool do_single_step);
    XNUTRACE_INLINE TraceLog &logger();
    double elapsed_time() const;
    uint64_t context_switch_count_self() const;
    uint64_t context_switch_count_target() const;
    void handle_pipe();

private:
    void setup_breakpoint_exception_handler();
    void install_breakpoint_exception_handler();
    void uninstall_breakpoint_exception_handler();
    void setup_breakpoint_exception_port_dispath_source();
    void setup_proc_dispath_source();
    void setup_pipe_dispatch_source();
    void start_measuring_stats();
    void stop_measuring_stats();
    pid_t spawn_with_args(const std::vector<std::string> &spawn_args, bool pipe_ctrl,
                          bool disable_aslr);
    void common_ctor(bool pipe_ctrl, bool was_spawned, bool symbolicate);

private:
    task_t m_target_task{TASK_NULL};
    mach_port_t m_breakpoint_exc_port{MACH_PORT_NULL};
    mach_port_t m_orig_breakpoint_exc_port{MACH_PORT_NULL};
    exception_behavior_t m_orig_breakpoint_exc_behavior{0};
    thread_state_flavor_t m_orig_breakpoint_exc_flavor{0};
    dispatch_queue_t m_queue{nullptr};
    dispatch_source_t m_proc_source{nullptr};
    dispatch_source_t m_breakpoint_exc_source{nullptr};
    dispatch_source_t m_pipe_source{nullptr};
    std::optional<int> m_target2tracer_fd;
    std::optional<int> m_tracer2target_fd;
    bool m_single_stepping{};
    bool m_measuring_stats{};
    TraceLog m_log;
    double m_elapsed_time{};
    timespec m_start_time{};
    uint64_t m_target_total_csw{};
    int32_t m_target_start_num_csw{};
    uint64_t m_self_total_csw{};
    int32_t m_self_start_num_csw{};
    std::unique_ptr<MachORegions> m_macho_regions;
    std::unique_ptr<VMRegions> m_vm_regions;
    std::unique_ptr<Symbols> m_symbols;
};
