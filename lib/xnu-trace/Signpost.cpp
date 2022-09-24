#include "xnu-trace/Signpost.h"
#include "common-internal.h"

#include <map>

static std::map<std::string, os_log_t> s_log_categories;

static void post(os_log_t log, os_signpost_type_t type, os_signpost_id_t id, const char *name) {
    uint8_t __attribute__((uninitialized, aligned(16))) os_fmt_buf[256];
    os_fmt_buf[0] = 0;
    _os_signpost_emit_with_name_impl(&__dso_handle, log, type, id, name, "", os_fmt_buf,
                                     sizeof(os_fmt_buf));
}

Signpost::Signpost(const std::string &category, const std::string &name, bool event)
    : m_name{name}, m_event{event} {
    if (!s_log_categories.contains(category)) {
        s_log_categories[category] = os_log_create(SUBSYSTEM, category.c_str());
    }
    m_log = s_log_categories[category];
    if (!os_signpost_enabled(m_log)) {
        m_disabled = true;
        return;
    }
    if (m_event) {
        post(m_log, OS_SIGNPOST_EVENT, OS_SIGNPOST_ID_EXCLUSIVE, m_name.c_str());
        return;
    }
    m_id = os_signpost_id_make_with_pointer(m_log, this);
    if (m_id == OS_SIGNPOST_ID_NULL || m_id == OS_SIGNPOST_ID_INVALID) {
        m_disabled = true;
        return;
    }
}

void Signpost::start() {
    if (m_disabled) {
        return;
    }
    post(m_log, OS_SIGNPOST_INTERVAL_BEGIN, m_id, m_name.c_str());
}

void Signpost::end() {
    if (m_disabled) {
        return;
    }
    post(m_log, OS_SIGNPOST_INTERVAL_END, m_id, m_name.c_str());
}
