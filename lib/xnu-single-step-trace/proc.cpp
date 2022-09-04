#include "common.h"

#include <libproc.h>

int32_t get_context_switch_count(pid_t pid) {
    proc_taskinfo ti;
    const auto res = proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &ti, sizeof(ti));
    if (res != sizeof(ti)) {
        fmt::print(stderr, "get_context_switch_count proc_pidinfo returned {:d}", res);
    }
    return ti.pti_csw;
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
