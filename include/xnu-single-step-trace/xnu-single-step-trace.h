#pragma once

#include <optional>
#include <string>
#include <unistd.h>
#include <vector>

#include <dispatch/dispatch.h>
#include <mach/mach.h>

void set_single_step_thread(thread_t thread, bool do_ss);
void set_single_step_task(task_t thread, bool do_ss);

pid_t pid_for_name(std::string process_name);

int64_t get_task_for_pid_count(task_t task);

class XNUTracer {
public:
    XNUTracer(task_t target_task);
    XNUTracer(pid_t target_pid);
    XNUTracer(std::string target_name);
    XNUTracer(std::vector<std::string> spawn_args, std::optional<int> pipe_write_fd,
              bool disable_aslr = true);
    ~XNUTracer();

    dispatch_source_t breakpoint_exception_port_dispath_source();
    void set_single_step(const bool do_single_step);

private:
    void setup_breakpoint_exception_handler();
    void install_breakpoint_exception_handler();
    void uninstall_breakpoint_exception_handler();
    void setup_breakpoint_exception_port_dispath_source();
    pid_t spawn_with_args(std::vector<std::string> spawn_args, std::optional<int> pipe_fd,
                          bool disable_aslr);
    void common_ctor(const bool free_running);

private:
    task_t m_target_task{TASK_NULL};
    mach_port_t m_breakpoint_exc_port{MACH_PORT_NULL};
    mach_port_t m_orig_breakpoint_exc_port{MACH_PORT_NULL};
    exception_behavior_t m_orig_breakpoint_exc_behavior{0};
    thread_state_flavor_t m_orig_breakpoint_exc_flavor{0};
    dispatch_source_t m_breakpoint_exc_source{nullptr};
    std::optional<int> m_pipe_write_fd;
};
