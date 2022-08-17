#pragma once

#include <string>
#include <unistd.h>

#include <dispatch/dispatch.h>
#include <mach/mach.h>

mach_port_t create_exception_port(task_t target_task, exception_mask_t exception_mask);

void set_single_step_thread(thread_t thread, bool do_ss);
void set_single_step_task(task_t thread, bool do_ss);

pid_t pid_for_name(std::string process_name);

int64_t get_task_for_pid_count(task_t task);

class XNUTracer {
public:
    XNUTracer(task_t target_task);
    XNUTracer(pid_t target_pid);
    XNUTracer(std::string target_name);

    dispatch_source_t exception_port_dispath_source();

private:
    void install_exception_handler();
    void uninstall_exception_handler();
    void setup_exception_port_dispath_source();

private:
    task_t m_target_task{TASK_NULL};
    mach_port_t m_exc_port{MACH_PORT_NULL};
    dispatch_source_t m_exc_source{nullptr};
};
