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

struct log_msg_hdr {
    uint64_t pc;
} __attribute__((packed));

struct log_region {
    uint64_t base;
    uint64_t size;
    uint64_t path_len;
} __attribute__((packed));

struct log_thread_hdr {
    uint32_t thread_id;
    uint64_t thread_log_sz;
} __attribute__((packed));

struct log_hdr {
    uint64_t num_regions;
} __attribute__((packed));

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

void write_file(std::string path, const uint8_t *buf, size_t sz);
std::vector<uint8_t> read_file(std::string path);
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
    MachORegions(const log_region *region_buf, uint64_t num_regions);
    void reset();
    const std::vector<image_info> &regions() const;
    image_info lookup(uint64_t addr);

private:
    const task_t m_target_task{};
    std::vector<image_info> m_regions;
};

class TraceLog {
public:
    TraceLog();
    TraceLog(const std::string &log_path);
    __attribute__((always_inline)) void log(thread_t thread, uint64_t pc);
    void write_to_file(const std::string &path, const MachORegions &macho_regions);
    uint64_t num_inst() const;
    size_t num_bytes() const;
    const MachORegions &macho_regions() const;
    const std::map<uint32_t, std::vector<log_msg_hdr>> &parsed_logs() const;

private:
    uint64_t m_num_inst{};
    std::unique_ptr<MachORegions> m_macho_regions;
    std::map<uint32_t, std::vector<uint8_t>> m_log_bufs;
    std::map<uint32_t, std::vector<log_msg_hdr>> m_parsed_logs;
};

class XNUTracer {
public:
    XNUTracer(task_t target_task, std::optional<std::filesystem::path> trace_path);
    XNUTracer(pid_t target_pid, std::optional<std::filesystem::path> trace_path);
    XNUTracer(std::string target_name, std::optional<std::filesystem::path> trace_path);
    XNUTracer(std::vector<std::string> spawn_args, std::optional<std::filesystem::path> trace_path,
              bool pipe_ctrl = false, bool disable_aslr = true);
    ~XNUTracer();

    void suspend();
    void resume();
    pid_t pid();
    dispatch_source_t proc_dispath_source();
    dispatch_source_t breakpoint_exception_port_dispath_source();
    dispatch_source_t pipe_dispatch_source();
    void set_single_step(bool do_single_step);
    __attribute__((always_inline)) TraceLog &logger();
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
    void common_ctor(bool pipe_ctrl, bool was_spawned);

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
    TraceLog m_log{};
    double m_elapsed_time{};
    timespec m_start_time{};
    uint64_t m_target_total_csw{};
    int32_t m_target_start_num_csw{};
    uint64_t m_self_total_csw{};
    int32_t m_self_start_num_csw{};
    std::unique_ptr<MachORegions> m_macho_regions;
    std::unique_ptr<VMRegions> m_vm_regions;
    std::optional<std::filesystem::path> m_trace_path;
};
