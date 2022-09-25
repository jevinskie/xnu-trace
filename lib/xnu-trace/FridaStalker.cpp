#include "xnu-trace/FridaStalker.h"
#include "common-internal.h"

#include "xnu-trace/TraceLog.h"
#include "xnu-trace/xnu-trace-c.h"

#include <locale>

#include <mach/mach_init.h>

#undef G_DISABLE_ASSERT
#include <frida-gum.h>

FridaStalker::FridaStalker(const std::string &log_dir_path, bool symbolicate, int compression_level,
                           bool stream)
    : m_log{log_dir_path, compression_level, stream}, m_macho_regions{mach_task_self()},
      m_vm_regions{mach_task_self()} {
    gum_init_embedded();
    assert(gum_stalker_is_supported());
    m_stalker = gum_stalker_new();
    assert(m_stalker);
    gum_stalker_set_trust_threshold(m_stalker, 0);
    m_transformer = gum_stalker_transformer_make_from_callback(
        (GumStalkerTransformerCallback)transform_cb, (void *)this, nullptr);
    assert(m_transformer);
    if (symbolicate) {
        m_symbols = std::make_unique<Symbols>(mach_task_self());
    }
}

FridaStalker::~FridaStalker() {
    write_trace();
    g_object_unref(m_stalker);
    g_object_unref(m_transformer);
    gum_deinit_embedded();
    fmt::print("{:s}\n",
               fmt::format(std::locale("en_US.UTF-8"), "Number of instructions traced: {:Ld}\n",
                           logger().num_inst()));
}

void FridaStalker::follow() {
    gum_stalker_follow_me(m_stalker, m_transformer, nullptr);
}

void FridaStalker::follow(size_t thread_id) {
    gum_stalker_follow(m_stalker, thread_id, m_transformer, nullptr);
}

void FridaStalker::unfollow() {
    gum_stalker_unfollow_me(m_stalker);
}

void FridaStalker::unfollow(size_t thread_id) {
    gum_stalker_unfollow(m_stalker, thread_id);
}

void FridaStalker::write_trace() {
    logger().write(m_macho_regions, m_symbols.get());
}

TraceLog &FridaStalker::logger() {
    return m_log;
}

void FridaStalker::transform_cb(void *iterator, void *output, void *user_data) {
    (void)output;
    auto *it = (GumStalkerIterator *)iterator;
    while (gum_stalker_iterator_next(it, nullptr)) {
        gum_stalker_iterator_put_callout(it, (GumStalkerCallout)instruction_cb, user_data, nullptr);
        gum_stalker_iterator_keep(it);
    }
}

void FridaStalker::instruction_cb(void *context, void *user_data) {
    auto ctx  = (GumCpuContext *)context;
    auto thiz = (FridaStalker *)user_data;
    thiz->logger().log((uint32_t)gum_process_get_current_thread_id(), ctx->pc);
}

// C API

stalker_t create_stalker(const char *log_dir_path, int symbolicate, int compression_level,
                         int stream) {
    return (stalker_t) new FridaStalker{log_dir_path, !!symbolicate, compression_level, !!stream};
}

void destroy_stalker(stalker_t stalker) {
    delete (FridaStalker *)stalker;
}

void stalker_follow_me(stalker_t stalker) {
    ((FridaStalker *)stalker)->follow();
}

void stalker_follow_thread(stalker_t stalker, size_t thread_id) {
    ((FridaStalker *)stalker)->follow(thread_id);
}

void stalker_unfollow_me(stalker_t stalker) {
    ((FridaStalker *)stalker)->unfollow();
}

void stalker_unfollow_thread(stalker_t stalker, size_t thread_id) {
    ((FridaStalker *)stalker)->unfollow(thread_id);
}
