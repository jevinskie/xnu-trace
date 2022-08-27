#pragma once

#include <ctime>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <unistd.h>
#include <vector>

#include <dispatch/dispatch.h>
#include <mach/mach.h>

constexpr int pipe_tracer2target_fd = STDERR_FILENO + 1;
constexpr int pipe_target2tracer_fd = STDERR_FILENO + 2;

void pipe_set_single_step(bool do_ss);

void set_single_step_thread(thread_t thread, bool do_ss);
void set_single_step_task(task_t thread, bool do_ss);

pid_t pid_for_name(std::string process_name);

int64_t get_task_for_pid_count(task_t task);

void audit_token_for_task(task_t task, audit_token_t *token);
pid_t pid_for_task(task_t task);

class XNUTracer {
public:
    XNUTracer(task_t target_task);
    XNUTracer(pid_t target_pid);
    XNUTracer(std::string target_name);
    XNUTracer(std::vector<std::string> spawn_args, bool pipe_ctrl = false,
              bool disable_aslr = true);
    ~XNUTracer();

    void suspend();
    void resume(const bool allow_dead = false);
    pid_t pid();
    dispatch_source_t proc_dispath_source();
    dispatch_source_t breakpoint_exception_port_dispath_source();
    dispatch_source_t pipe_dispatch_source();
    void set_single_step(bool do_single_step);
    uint64_t num_inst() const;
    void log_inst(const std::span<uint8_t> log_buf);
    double elapsed_time() const;
    void handle_pipe();

private:
    void setup_breakpoint_exception_handler();
    void install_breakpoint_exception_handler();
    void uninstall_breakpoint_exception_handler(bool allow_dead = false);
    void setup_breakpoint_exception_port_dispath_source();
    void setup_proc_dispath_source();
    void setup_pipe_dispatch_source();
    void setup_regions();
    pid_t spawn_with_args(const std::vector<std::string> &spawn_args, bool pipe_ctrl,
                          bool disable_aslr);
    void common_ctor(bool pipe_ctrl);

private:
    struct region {
        uint64_t base;
        uint64_t size;
        std::filesystem::path path;
        auto operator<=>(const region &rhs) const {
            return base <=> rhs.base;
        }
    };
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
    uint64_t m_num_inst{0};
    std::vector<uint8_t> m_log_buf;
    timespec m_start_time{};
    std::vector<region> m_regions;
};
