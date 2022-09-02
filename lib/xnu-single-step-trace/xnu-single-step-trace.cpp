#include "xnu-single-step-trace/xnu-single-step-trace.h"

#undef NDEBUG
#include <algorithm>
#include <cassert>
#include <climits>
#include <concepts>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <spawn.h>
#include <vector>

#include <libproc.h>
#include <mach-o/dyld_images.h>
#include <mach-o/loader.h>
#include <mach/exc.h>
#include <mach/exception.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/task_info.h>
#include <mach/thread_status.h>
#include <pthread.h>
#include <sys/proc_info.h>

#include <CoreSymbolication/CoreSymbolication.h>

#include <fmt/format.h>
#include <interval-tree/interval_tree.hpp>

#include "mach_exc.h"

namespace fs = std::filesystem;
using namespace std::string_literals;
using namespace lib_interval_tree;

#define EXC_MSG_MAX_SIZE 4096

#ifndef _POSIX_SPAWN_DISABLE_ASLR
#define _POSIX_SPAWN_DISABLE_ASLR 0x0100
#endif

using dispatch_mig_callback_t = boolean_t (*)(mach_msg_header_t *message, mach_msg_header_t *reply);

extern "C" char **environ;

extern "C" mach_msg_return_t dispatch_mig_server(dispatch_source_t ds, size_t maxmsgsz,
                                                 dispatch_mig_callback_t callback);

extern "C" boolean_t mach_exc_server(mach_msg_header_t *message, mach_msg_header_t *reply);

static XNUTracer *g_tracer;

template <typename T> size_t bytesizeof(const typename std::vector<T> &vec) {
    return sizeof(T) * vec.size();
}

template <typename U>
requires requires() {
    requires std::unsigned_integral<U>;
}
constexpr U roundup_pow2_mul(U num, std::size_t pow2_mul) {
    const U mask = static_cast<U>(pow2_mul) - 1;
    return (num + mask) & ~mask;
}

static double timespec_diff(const timespec &a, const timespec &b) {
    timespec c;
    c.tv_sec  = a.tv_sec - b.tv_sec;
    c.tv_nsec = a.tv_nsec - b.tv_nsec;
    if (c.tv_nsec < 0) {
        --c.tv_sec;
        c.tv_nsec += 1'000'000'000L;
    }
    return c.tv_sec + ((double)c.tv_nsec / 1'000'000'000L);
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

bool task_is_valid(task_t task) {
    if (!MACH_PORT_VALID(task)) {
        return false;
    }
    pid_t pid;
    return pid_for_task(task, &pid) == KERN_SUCCESS;
}

void write_file(std::string path, const uint8_t *buf, size_t sz) {
    const auto fh = fopen(path.c_str(), "wb");
    assert(fh);
    assert(fwrite(buf, sz, 1, fh) == 1);
    assert(!fclose(fh));
}

std::vector<uint8_t> read_file(std::string path) {
    std::vector<uint8_t> res;
    const auto fh = fopen(path.c_str(), "rb");
    assert(fh);
    assert(!fseek(fh, 0, SEEK_END));
    const auto sz = ftell(fh);
    assert(!fseek(fh, 0, SEEK_SET));
    res.resize(sz);
    assert(fread(res.data(), sz, 1, fh) == 1);
    assert(!fclose(fh));
    return res;
}

std::vector<uint8_t> read_target(task_t target_task, uint64_t target_addr, uint64_t sz) {
    std::vector<uint8_t> res;
    res.resize(sz);
    vm_size_t vm_sz = sz;
    const auto kr   = vm_read_overwrite(target_task, (vm_address_t)target_addr, sz,
                                        (vm_address_t)res.data(), &vm_sz);
    mach_check(kr, "vm_read_overwrite");
    assert(vm_sz == sz);
    return res;
}

template <typename T>
std::vector<uint8_t> read_target(task_t target_task, const T *target_addr, uint64_t sz) {
    return read_target(target_task, (uint64_t)target_addr, sz);
}

std::string read_cstr_target(task_t target_task, uint64_t target_addr) {
    std::vector<uint8_t> buf;
    constexpr size_t pgsz = 4096;
    do {
        const auto end_addr =
            target_addr % pgsz ? roundup_pow2_mul(target_addr, pgsz) : target_addr + pgsz;
        const auto smol_buf = read_target(target_task, target_addr, end_addr - target_addr);
        buf.insert(buf.end(), smol_buf.cbegin(), smol_buf.cend());
        target_addr = end_addr;
    } while (std::find(buf.cbegin(), buf.cend(), '\0') == buf.cend());
    return {(char *)buf.data()};
}

std::string read_cstr_target(task_t target_task, const char *target_addr) {
    return read_cstr_target(target_task, (uint64_t)target_addr);
}

std::string prot_to_str(vm_prot_t prot) {
    std::string s;
    s += prot & VM_PROT_READ ? "r" : "-";
    s += prot & VM_PROT_WRITE ? "w" : "-";
    s += prot & VM_PROT_EXECUTE ? "x" : "-";
    s += prot & ~(VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE) ? "*" : " ";
    return s;
}

std::vector<region> get_vm_regions(task_t target_task) {
    std::vector<region> res;
    vm_address_t addr = 0;
    kern_return_t kr;
    natural_t depth = 0;
    while (1) {
        vm_size_t sz{};
        vm_region_submap_info_64 info{};
        mach_msg_type_number_t cnt = VM_REGION_SUBMAP_INFO_COUNT_64;
        kr = vm_region_recurse_64(target_task, &addr, &sz, &depth, (vm_region_recurse_info_t)&info,
                                  &cnt);
        if (kr != KERN_SUCCESS) {
            if (kr != KERN_INVALID_ADDRESS) {
                fmt::print("Error: '{:s}' retval: {:d} description: '{:s}'\n", "get_vm_regions", kr,
                           mach_error_string(kr));
            }
            break;
        }
#if 0
        if (info.protection & 1 && sz && !info.is_submap) {
            const auto buf = read_target(target_task, addr, 128);
            hexdump(buf.data(), buf.size());
        }
#endif
        res.emplace_back(region{.base   = addr,
                                .size   = sz,
                                .depth  = depth,
                                .prot   = info.protection,
                                .submap = !!info.is_submap,
                                .tag    = info.user_tag});
        if (info.is_submap) {
            depth += 1;
            continue;
        }
        addr += sz;
    }

    std::sort(res.begin(), res.end());

#if 0
    for (const auto &map : res) {
        std::string l;
        std::fill_n(std::back_inserter(l), map.depth, '\t');
        l += fmt::format("{:#018x}-{:#018x} {:s} {:#x} {:#04x}", map.base, map.base + map.size,
                         prot_to_str(map.prot), map.size, map.tag);
        fmt::print("{:s}\n", l);
    }
#endif

    return res;
}

std::vector<segment_command_64> read_macho_segs_target(task_t target_task,
                                                       uint64_t macho_hdr_addr) {
    std::vector<segment_command_64> segs;
    const auto hdr_buf = read_target(target_task, macho_hdr_addr, sizeof(mach_header_64));
    const auto hdr     = (mach_header_64 *)hdr_buf.data();
    const auto cmd_buf =
        read_target(target_task, macho_hdr_addr + sizeof(mach_header_64), hdr->sizeofcmds);
    const auto end_of_lc = (load_command *)(cmd_buf.data() + hdr->sizeofcmds);
    for (auto lc = (load_command *)cmd_buf.data(); lc < end_of_lc;
         lc      = (load_command *)((uint8_t *)lc + lc->cmdsize)) {
        if (lc->cmd != LC_SEGMENT_64) {
            continue;
        }
        const auto seg = (segment_command_64 *)lc;
        if (!strncmp(seg->segname, "__PAGEZERO", sizeof(seg->segname))) {
            continue;
        }
        segs.emplace_back(*seg);
    }
    return segs;
}

std::vector<segment_command_64> read_macho_segs_target(task_t target_task,
                                                       const mach_header_64 *macho_hdr) {
    return read_macho_segs_target(target_task, (uint64_t)macho_hdr);
}

uint64_t get_text_size(const std::vector<segment_command_64> &segments) {
    uint32_t num_exec_segs = 0;
    uint64_t exec_file_sz;
    for (const auto &seg : segments) {
        if (seg.maxprot & VM_PROT_EXECUTE) {
            ++num_exec_segs;
            exec_file_sz = seg.vmsize;
        }
    }
    assert(num_exec_segs == 1);
    return exec_file_sz;
}

std::vector<image_info> get_dyld_image_infos(task_t target_task) {
    std::vector<image_info> res;
    task_dyld_info_data_t dyld_info;
    mach_msg_type_number_t cnt = TASK_DYLD_INFO_COUNT;
    mach_check(task_info(target_task, TASK_DYLD_INFO, (task_info_t)&dyld_info, &cnt),
               "task_info dyld info");
    assert(dyld_info.all_image_info_format == TASK_DYLD_ALL_IMAGE_INFO_64);
    const auto all_info_buf =
        read_target(target_task, dyld_info.all_image_info_addr, dyld_info.all_image_info_size);
    const dyld_all_image_infos *all_img_infos = (dyld_all_image_infos *)all_info_buf.data();

    if (!all_img_infos->infoArray) {
        return res;
    }

    res.emplace_back(image_info{.base = (uint64_t)all_img_infos->dyldImageLoadAddress,
                                .size = get_text_size(read_macho_segs_target(
                                    target_task, (uint64_t)all_img_infos->dyldImageLoadAddress)),
                                .path = read_cstr_target(target_task, all_img_infos->dyldPath)});

    const auto infos_buf = read_target(target_task, all_img_infos->infoArray,
                                       all_img_infos->infoArrayCount * sizeof(dyld_image_info));
    const auto img_infos = std::span<const dyld_image_info>{(dyld_image_info *)infos_buf.data(),
                                                            all_img_infos->infoArrayCount};
    for (const auto &img_info : img_infos) {
        const auto path = res.emplace_back(
            image_info{.base = (uint64_t)img_info.imageLoadAddress,
                       .size = get_text_size(read_macho_segs_target(
                           target_task, (uint64_t)img_info.imageLoadAddress)),
                       .path = read_cstr_target(target_task, img_info.imageFilePath)});
    }

    std::sort(res.begin(), res.end());

#if 0
    for (const auto &img_info : res) {
        fmt::print("img_info base: {:#018x} sz: {:#010x} path: '{:s}'\n", img_info.base,
                   img_info.size, img_info.path.string());
    }
#endif

    return res;
}

void pipe_set_single_step(bool do_ss) {
    const uint8_t wbuf = do_ss ? 'y' : 'n';
    assert(write(pipe_target2tracer_fd, &wbuf, 1) == 1);
    uint8_t rbuf = 0;
    assert(read(pipe_tracer2target_fd, &rbuf, 1) == 1);
    assert(rbuf == 'c');
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

pid_t pid_for_task(task_t task) {
    assert(task);
    int pid;
    mach_check(pid_for_task(task, &pid), "pid_for_task");
    return (pid_t)pid;
}

int32_t get_context_switch_count(pid_t pid) {
    proc_taskinfo ti;
    const auto res = proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &ti, sizeof(ti));
    if (res != sizeof(ti)) {
        fmt::print(stderr, "get_context_switch_count proc_pidinfo returned {:d}", res);
    }
    return ti.pti_csw;
}

integer_t get_suspend_count(task_t task) {
    task_basic_info_64_data_t info;
    mach_msg_type_number_t cnt = TASK_BASIC_INFO_64_COUNT;
    const auto kr              = task_info(task, TASK_BASIC_INFO_64, (task_info_t)&info, &cnt);
    mach_check(kr, "get_suspend_count task_info");
    return info.suspend_count;
}

// https://gist.github.com/ccbrown/9722406
void hexdump(const void *data, size_t size) {
    char ascii[17];
    size_t i, j;
    ascii[16] = '\0';
    for (i = 0; i < size; ++i) {
        printf("%02X ", ((unsigned char *)data)[i]);
        if (((unsigned char *)data)[i] >= ' ' && ((unsigned char *)data)[i] <= '~') {
            ascii[i % 16] = ((unsigned char *)data)[i];
        } else {
            ascii[i % 16] = '.';
        }
        if ((i + 1) % 8 == 0 || i + 1 == size) {
            printf(" ");
            if ((i + 1) % 16 == 0) {
                printf("|  %s \n", ascii);
            } else if (i + 1 == size) {
                ascii[(i + 1) % 16] = '\0';
                if ((i + 1) % 16 <= 8) {
                    printf(" ");
                }
                for (j = (i + 1) % 16; j < 16; ++j) {
                    printf("   ");
                }
                printf("|  %s \n", ascii);
            }
        }
    }
}

void set_single_step_thread(thread_t thread, bool do_ss) {
    // fmt::print("thread {} ss: {}\n", thread, do_ss);

    mach_msg_type_number_t dbg_cnt = ARM_DEBUG_STATE64_COUNT;

#ifdef READ_DEBUG_STATE
    arm_debug_state64_t dbg_state;
    const auto kr_thread_get =
        thread_get_state(thread, ARM_DEBUG_STATE64, (thread_state_t)&dbg_state, &dbg_cnt);
    assert(kr_thread_get == KERN_SUCCESS);
    // mach_check(kr_thread_get,
    //            fmt::format("single_step({:s}) thread_get_state", do_ss ? "true" : "false"));
#else
    arm_debug_state64_t dbg_state{};
#endif

    dbg_state.__mdscr_el1 = (dbg_state.__mdscr_el1 & ~1) | do_ss;

    const auto kr_thread_set =
        thread_set_state(thread, ARM_DEBUG_STATE64, (thread_state_t)&dbg_state, dbg_cnt);
    assert(kr_thread_set == KERN_SUCCESS);
    // mach_check(kr_thread_set,
    //            fmt::format("single_step({:s}) thread_set_state", do_ss ? "true" : "false"));
}

void set_single_step_task(task_t task, bool do_ss) {
    // fmt::print("task {} ss: {}\n", task, do_ss);

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

    const auto opc = arm_thread_state64_get_pc(*os);
    // fmt::print(stderr, "exc pc: {:p} {:p}\n", (void *)opc);

    *new_state_count = old_state_count;
    *ns              = *os;

    g_tracer->logger().log(thread, opc);

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

// XNUTracer class

XNUTracer::XNUTracer(task_t target_task, std::optional<fs::path> trace_path)
    : m_target_task(target_task), m_trace_path{trace_path} {
    suspend();
    common_ctor(false, false);
}

XNUTracer::XNUTracer(pid_t target_pid, std::optional<fs::path> trace_path)
    : m_trace_path{trace_path} {
    const auto kr = task_for_pid(mach_task_self(), target_pid, &m_target_task);
    mach_check(kr, fmt::format("task_for_pid({:d}", target_pid));
    suspend();
    common_ctor(false, false);
}

XNUTracer::XNUTracer(std::string target_name, std::optional<fs::path> trace_path)
    : m_trace_path{trace_path} {
    const auto target_pid = pid_for_name(target_name);
    const auto kr         = task_for_pid(mach_task_self(), target_pid, &m_target_task);
    mach_check(kr, fmt::format("task_for_pid({:d}", target_pid));
    suspend();
    common_ctor(false, false);
}

XNUTracer::XNUTracer(std::vector<std::string> spawn_args, std::optional<fs::path> trace_path,
                     bool pipe_ctrl, bool disable_aslr)
    : m_trace_path{trace_path} {
    const auto target_pid = spawn_with_args(spawn_args, pipe_ctrl, disable_aslr);
    const auto kr         = task_for_pid(mach_task_self(), target_pid, &m_target_task);
    mach_check(kr, fmt::format("task_for_pid({:d}", target_pid));
    common_ctor(pipe_ctrl, true);
}

XNUTracer::~XNUTracer() {
    uninstall_breakpoint_exception_handler();
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
    if (m_trace_path != std::nullopt) {
        logger().write_to_file(m_trace_path->string(), *m_macho_regions);
    }
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
        dispatch_mig_server(m_breakpoint_exc_source, EXC_MSG_MAX_SIZE, mach_exc_server);
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
            set_single_step(true);
            if (get_suspend_count(m_target_task) == 0 && !m_measuring_stats) {
                start_measuring_stats();
            }
        } else if (rbuf == 'n') {
            set_single_step(false);
            if (get_suspend_count(m_target_task) == 0 && m_measuring_stats) {
                stop_measuring_stats();
            }
        } else {
            assert(!"unhandled");
        }
        const uint8_t cbuf = 'c';
        assert(write(*m_tracer2target_fd, &cbuf, 1) == 1);
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

void XNUTracer::common_ctor(bool pipe_ctrl, bool was_spawned) {
    fmt::print("XNUTracer {:s} PID {:d}\n", was_spawned ? "spawned" : "attached to", pid());
    assert(!g_tracer);
    g_tracer        = this;
    m_macho_regions = std::make_unique<MachORegions>(m_target_task);
    m_vm_regions    = std::make_unique<VMRegions>(m_target_task);
    m_queue         = dispatch_queue_create("je.vin.tracer", DISPATCH_QUEUE_SERIAL);
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
    if (do_single_step) {
        m_macho_regions->reset();
        m_vm_regions->reset();
        install_breakpoint_exception_handler();
        set_single_step_task(m_target_task, true);
    } else {
        set_single_step_task(m_target_task, false);
        uninstall_breakpoint_exception_handler();
    }
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

VMRegions::VMRegions(task_t target_task) : m_target_task{target_task} {
    reset();
}

void VMRegions::reset() {
    mach_check(task_suspend(m_target_task), "region reset suspend");
    m_all_regions = get_vm_regions(m_target_task);

    mach_check(task_resume(m_target_task), "region reset resume");
}

MachORegions::MachORegions(task_t target_task) : m_target_task{target_task} {
    reset();
}

MachORegions::MachORegions(const log_region *region_buf, uint64_t num_regions) {
    for (uint64_t i = 0; i < num_regions; ++i) {
        const char *path_ptr = (const char *)(region_buf + 1);
        std::string path{path_ptr, region_buf->path_len};
        m_regions.emplace_back(
            image_info{.base = region_buf->base, .size = region_buf->size, .path = path});
        region_buf =
            (log_region *)((uint8_t *)region_buf + sizeof(*region_buf) + region_buf->path_len);
    }
    std::sort(m_regions.begin(), m_regions.end());
}

void MachORegions::reset() {
    assert(m_target_task);
    mach_check(task_suspend(m_target_task), "region reset suspend");
    m_regions = get_dyld_image_infos(m_target_task);
    std::vector<uint64_t> region_bases;
    for (const auto &region : m_regions) {
        region_bases.emplace_back(region.base);
    }
    int num_jit_regions = 0;
    for (const auto &vm_region : get_vm_regions(m_target_task)) {
        if (!(vm_region.prot & VM_PROT_EXECUTE)) {
            continue;
        }
        if (!(vm_region.prot & VM_PROT_READ)) {
            fmt::print("found XO page at {:#018x}\n", vm_region.base);
        }
        if (std::find(region_bases.cbegin(), region_bases.cend(), vm_region.base) !=
            region_bases.cend()) {
            continue;
        }
        if (vm_region.tag != 0xFF) {
            continue;
        }
        m_regions.emplace_back(image_info{
            .base = vm_region.base,
            .size = vm_region.size,
            .path = fmt::format("/tmp/pid-{:d}-jit-region-{:d}-tag-{:02x}",
                                pid_for_task(m_target_task), num_jit_regions, vm_region.tag)});
        ++num_jit_regions;
    }
    std::sort(m_regions.begin(), m_regions.end());
    mach_check(task_resume(m_target_task), "region reset resume");
}

image_info MachORegions::lookup(uint64_t addr) const {
    for (const auto &img_info : m_regions) {
        if (img_info.base <= addr && addr <= img_info.base + img_info.size) {
            return img_info;
        }
    }
    assert(!"no region found");
}

std::pair<image_info, size_t> MachORegions::lookup_idx(uint64_t addr) const {
    size_t idx = 0;
    for (const auto &img_info : m_regions) {
        if (img_info.base <= addr && addr <= img_info.base + img_info.size) {
            return std::make_pair(img_info, idx);
        }
        ++idx;
    }
    assert(!"no region found");
}

const std::vector<image_info> &MachORegions::regions() const {
    return m_regions;
}

TraceLog::TraceLog() {
    // nothing to do
}

TraceLog::TraceLog(const std::string &log_path) {
    const auto trace_buf = read_file(log_path);
    const auto trace_hdr = (log_hdr *)trace_buf.data();

    auto region_ptr = (log_region *)((uint8_t *)trace_hdr + sizeof(*trace_hdr));
    m_macho_regions = std::make_unique<MachORegions>(region_ptr, trace_hdr->num_regions);
    for (uint64_t i = 0; i < trace_hdr->num_regions; ++i) {
        region_ptr =
            (log_region *)((uint8_t *)region_ptr + sizeof(*region_ptr) + region_ptr->path_len);
    }

    const auto thread_hdr_end = (log_thread_hdr *)(trace_buf.data() + trace_buf.size());
    auto thread_hdr           = (log_thread_hdr *)region_ptr;
    while (thread_hdr < thread_hdr_end) {
        std::vector<log_msg_hdr> thread_log;
        const auto thread_log_start = (uint8_t *)thread_hdr + sizeof(*thread_hdr);
        const auto thread_log_end   = thread_log_start + thread_hdr->thread_log_sz;
        auto inst_hdr               = (log_msg_hdr *)thread_log_start;
        const auto inst_hdr_end     = (log_msg_hdr *)thread_log_end;
        while (inst_hdr < inst_hdr_end) {
            thread_log.emplace_back(*inst_hdr);
            inst_hdr = inst_hdr + 1;
        }
        m_parsed_logs.emplace(std::make_pair(thread_hdr->thread_id, thread_log));
        thread_hdr = (log_thread_hdr *)thread_log_end;
    }
}

uint64_t TraceLog::num_inst() const {
    return m_num_inst;
}

size_t TraceLog::num_bytes() const {
    size_t sz = 0;
    for (const auto &thread_log : m_log_bufs) {
        sz += thread_log.second.size();
    }
    return sz;
}

const MachORegions &TraceLog::macho_regions() const {
    assert(m_macho_regions);
    return *m_macho_regions;
}

const std::map<uint32_t, std::vector<log_msg_hdr>> &TraceLog::parsed_logs() const {
    return m_parsed_logs;
}

__attribute__((always_inline)) void TraceLog::log(thread_t thread, uint64_t pc) {
    const auto msg_hdr = log_msg_hdr{.pc = pc};
    std::copy((uint8_t *)&msg_hdr, (uint8_t *)&msg_hdr + sizeof(msg_hdr),
              std::back_inserter(m_log_bufs[thread]));
    ++m_num_inst;
}

void TraceLog::write_to_file(const std::string &path, const MachORegions &macho_regions) {
    const auto fh = fopen(path.c_str(), "wb");
    assert(fh);

    const log_hdr hdr_buf{.num_regions = macho_regions.regions().size()};
    assert(fwrite(&hdr_buf, sizeof(hdr_buf), 1, fh) == 1);

    for (const auto &region : macho_regions.regions()) {
        const log_region region_buf{
            .base = region.base, .size = region.size, .path_len = region.path.string().size()};
        assert(fwrite(&region_buf, sizeof(region_buf), 1, fh) == 1);
        assert(fwrite(region.path.c_str(), region.path.string().size(), 1, fh) == 1);
    }

    for (const auto &thread_buf_pair : m_log_bufs) {
        const auto tid           = thread_buf_pair.first;
        const auto buf           = thread_buf_pair.second;
        const auto thread_log_sz = buf.size() * sizeof(decltype(buf)::value_type);
        const log_thread_hdr thread_hdr{.thread_id = tid, .thread_log_sz = thread_log_sz};
        assert(fwrite(&thread_hdr, sizeof(thread_hdr), 1, fh) == 1);
        assert(fwrite(buf.data(), buf.size(), 1, fh) == 1);
    }

    assert(!fclose(fh));
}

std::vector<bb_t> extract_bbs_from_pc_trace(const std::span<const uint64_t> &pcs) {
    std::vector<bb_t> bbs;

    uint64_t bb_start = pcs[0];
    uint64_t last_pc  = pcs[0] - 4;
    for (const auto pc : pcs) {
        if (last_pc + 4 != pc) {
            bbs.emplace_back(bb_t{.pc = bb_start, .sz = (uint32_t)(last_pc + 4 - bb_start)});
            bb_start = pc;
        }
        last_pc = pc;
    }
    if (bb_start != last_pc) {
        bbs.emplace_back(bb_t{.pc = bb_start, .sz = (uint32_t)(last_pc + 4 - bb_start)});
    }
    return bbs;
}

std::vector<uint64_t> extract_pcs_from_trace(const std::span<const log_msg_hdr> &msgs) {
    std::vector<uint64_t> pcs;
    for (const auto &msg : msgs) {
        pcs.emplace_back(msg.pc);
    }
    return pcs;
}
