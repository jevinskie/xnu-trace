#pragma once

#include "common.h"

#include <string>

#include <os/log.h>
#include <os/signpost.h>

constexpr auto SUBSYSTEM = "vin.je.xnutracer";

class XNUTRACE_EXPORT Signpost {
public:
    Signpost(const std::string &category, const std::string &name, bool event = false);
    void start();
    void end();

private:
    os_log_t m_log;
    os_signpost_id_t m_id{OS_SIGNPOST_ID_NULL};
    const std::string m_name;
    const bool m_event;
    bool m_disabled{false};
};
