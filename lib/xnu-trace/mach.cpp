#include "common.h"

bool task_is_valid(task_t task) {
    if (!MACH_PORT_VALID(task)) {
        return false;
    }
    pid_t pid;
    return pid_for_task(task, &pid) == KERN_SUCCESS;
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

std::string read_cstr_target(task_t target_task, uint64_t target_addr) {
    std::vector<uint8_t> buf;
    do {
        const auto end_addr =
            target_addr % PAGE_SZ ? roundup_pow2_mul(target_addr, PAGE_SZ) : target_addr + PAGE_SZ;
        const auto smol_buf = read_target(target_task, target_addr, end_addr - target_addr);
        buf.insert(buf.end(), smol_buf.cbegin(), smol_buf.cend());
        target_addr = end_addr;
    } while (std::find(buf.cbegin(), buf.cend(), '\0') == buf.cend());
    return {(char *)buf.data()};
}

std::string read_cstr_target(task_t target_task, const char *target_addr) {
    return read_cstr_target(target_task, (uint64_t)target_addr);
}

integer_t get_suspend_count(task_t task) {
    task_basic_info_64_data_t info;
    mach_msg_type_number_t cnt = TASK_BASIC_INFO_64_COUNT;
    const auto kr              = task_info(task, TASK_BASIC_INFO_64, (task_info_t)&info, &cnt);
    mach_check(kr, "get_suspend_count task_info");
    return info.suspend_count;
}

pid_t pid_for_task(task_t task) {
    assert(task);
    int pid;
    mach_check(pid_for_task(task, &pid), "pid_for_task");
    return (pid_t)pid;
}

int64_t get_task_for_pid_count(task_t task) {
    struct task_extmod_info info;
    mach_msg_type_number_t count = TASK_EXTMOD_INFO_COUNT;
    const auto kr                = task_info(task, TASK_EXTMOD_INFO, (task_info_t)&info, &count);
    mach_check(kr, "get_task_for_pid_count thread_info");
    return info.extmod_statistics.task_for_pid_count;
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
    mach_check(kr_task_get, "set_single_step_task task_get_state");

    dbg_state.__mdscr_el1 = (dbg_state.__mdscr_el1 & ~1) | do_ss;

    const auto kr_task_set =
        task_set_state(task, ARM_DEBUG_STATE64, (thread_state_t)&dbg_state, dbg_cnt);
    mach_check(kr_task_set, "set_single_step_task task_set_state");

    thread_act_array_t thread_list;
    mach_msg_type_number_t num_threads;
    const auto kr_threads = task_threads(task, &thread_list, &num_threads);
    mach_check(kr_threads, "set_single_step_task task_threads");
    for (mach_msg_type_number_t i = 0; i < num_threads; ++i) {
        set_single_step_thread(thread_list[i], do_ss);
    }
    const auto kr_dealloc = vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                                          sizeof(thread_act_t) * num_threads);
    mach_check(kr_dealloc, "set_single_step_task vm_deallocate");
}
