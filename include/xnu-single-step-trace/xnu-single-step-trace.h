#pragma once

#include <ctime>
#include <filesystem>
#include <map>
#include <memory>
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
pid_t pid_for_task(task_t task);

bool task_is_valid(task_t task);
int64_t get_task_for_pid_count(task_t task);
int32_t get_context_switch_count(pid_t pid);
integer_t get_suspend_count(task_t task);

void hexdump(const void *data, size_t size);

std::vector<uint8_t> read_target(task_t target_task, uint64_t target_addr, uint64_t sz);

struct image_info {
    uint64_t base;
    uint64_t size;
    std::filesystem::path path;
    auto operator<=>(const image_info &rhs) const {
        return base <=> rhs.base;
    }
};

std::vector<image_info> get_dyld_image_infos(task_t target_task);

struct region {
    uint64_t base;
    uint64_t size;
    uint64_t depth;
    std::optional<std::filesystem::path> path;
    vm_prot_t prot;
    bool submap;
    auto operator<=>(const region &rhs) const {
        return base <=> rhs.base;
    }
};

class VMRegions {
public:
    VMRegions(task_t target_task);

    void reset();

private:
    const task_t m_target_task;
    std::vector<region> m_all_regions;
};

class MachORegions {
public:
    MachORegions(task_t target_task);
    void reset();
    image_info lookup(uint64_t addr);

private:
    const task_t m_target_task;
    std::vector<image_info> m_regions;
};

class XNUTracer {
public:
    XNUTracer(task_t target_task);
    XNUTracer(pid_t target_pid);
    XNUTracer(std::string target_name);
    XNUTracer(std::vector<std::string> spawn_args, bool pipe_ctrl = false,
              bool disable_aslr = true);
    ~XNUTracer();

    void suspend();
    void resume();
    pid_t pid();
    dispatch_source_t proc_dispath_source();
    dispatch_source_t breakpoint_exception_port_dispath_source();
    dispatch_source_t pipe_dispatch_source();
    void set_single_step(bool do_single_step);
    void log_inst(const std::span<uint8_t> log_buf);
    uint64_t num_inst() const;
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
    void common_ctor(bool pipe_ctrl);

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
    uint64_t m_num_inst{};
    std::vector<uint8_t> m_log_buf;
    double m_elapsed_time{};
    timespec m_start_time{};
    uint64_t m_target_total_csw{};
    int32_t m_target_start_num_csw{};
    uint64_t m_self_total_csw{};
    int32_t m_self_start_num_csw{};
    std::unique_ptr<MachORegions> m_macho_regions;
    std::unique_ptr<VMRegions> m_vm_regions;
};
