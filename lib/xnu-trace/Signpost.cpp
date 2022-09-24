#include "xnu-trace/Signpost.h"
#include "common-internal.h"

#include <map>

static std::map<std::string, os_log_t> s_log_categories;

Signpost::Signpost(const std::string &category, const std::string &name, bool event)
    : m_name{name}, m_event{event} {
    if (!s_log_categories.contains(category)) {
        s_log_categories[category] = os_log_create(SUBSYSTEM, category.c_str());
    }
    m_log = s_log_categories[category];
    m_id  = os_signpost_id_make_with_pointer(m_log, this);
    if (m_event) {
        // os_signpost_event_emit(m_log, m_id, name);
    }
}
