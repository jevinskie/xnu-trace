#pragma once

#include "common.h"

#include "XNUCommpageTime.h"

#include <string>

#include <os/log.h>
#include <os/signpost.h>

constexpr auto SUBSYSTEM = "vin.je.xnutracer";

template <typename T>
concept StringU64Callback = requires(T cb) {
    { cb(uint64_t{}) } -> std::same_as<std::string>;
};

class XNUTRACE_EXPORT Signpost {
public:
    Signpost(const std::string &category, const std::string &name, bool event = false,
             std::string msg = "");
    void start(std::string msg = "");
    void end(std::string msg = "");
    template <StringU64Callback T> void end(T const &cb) {
        end(cb(xnu_commpage_time_atus_to_ns(xnu_commpage_time_atus() - m_start_atus)));
    };

private:
    os_log_t m_log;
    os_signpost_id_t m_id{OS_SIGNPOST_ID_NULL};
    const std::string m_name;
    uint64_t m_start_atus;
    const bool m_event;
    bool m_disabled{false};
};
