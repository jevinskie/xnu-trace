#include "common.h"

FridaStalker::FridaStalker() {
    gum_init_embedded();
    assert(gum_stalker_is_supported());
    m_stalker = gum_stalker_new();
    assert(m_stalker);
}

FridaStalker::~FridaStalker() {
    g_object_unref(m_stalker);
    gum_deinit_embedded();
}

void FridaStalker::follow() {
    gum_stalker_follow_me(m_stalker, nullptr, nullptr);
}

void FridaStalker::follow(GumThreadId thread_id) {
    gum_stalker_follow(m_stalker, thread_id, nullptr, nullptr);
}

void FridaStalker::unfollow() {
    gum_stalker_unfollow_me(m_stalker);
}

void FridaStalker::unfollow(GumThreadId thread_id) {
    gum_stalker_unfollow(m_stalker, thread_id);
}
