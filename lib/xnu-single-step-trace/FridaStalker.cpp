#include "common.h"

FridaStalker::FridaStalker() {
    gum_init_embedded();
    assert(gum_stalker_is_supported());
    m_stalker = gum_stalker_new();
}

FridaStalker::~FridaStalker() {
    g_object_unref(m_stalker);
    gum_deinit_embedded();
}

void FridaStalker::follow() {}

void FridaStalker::follow(GumThreadId thread_id) {}

void FridaStalker::unfollow() {}

void FridaStalker::unfollow(GumThreadId thread_id) {}
