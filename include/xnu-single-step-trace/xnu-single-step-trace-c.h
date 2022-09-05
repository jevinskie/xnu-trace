#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <frida-gum.h>

typedef void *stalker_t;

stalker_t create_stalker(void);
void destroy_stalker(stalker_t stalker);
void stalker_follow_me(stalker_t stalker);
void stalker_follow_thread(stalker_t stalker, GumThreadId thread_id);
void stalker_unfollow_me(stalker_t stalker);
void stalker_unfollow_thread(stalker_t stalker, GumThreadId thread_id);

#ifdef __cplusplus
}
#endif
