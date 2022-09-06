#include "common.h"

FridaStalker::FridaStalker(bool symbolicate)
    : m_macho_regions{mach_task_self()}, m_vm_regions{mach_task_self()} {
    gum_init_embedded();
    assert(gum_stalker_is_supported());
    m_stalker = gum_stalker_new();
    assert(m_stalker);
    gum_stalker_set_trust_threshold(m_stalker, 0);
    m_transformer =
        gum_stalker_transformer_make_from_callback(transform_cb, (gpointer)this, nullptr);
    assert(m_transformer);
    if (symbolicate) {
        m_symbols = std::make_unique<Symbols>(mach_task_self());
    }
}

FridaStalker::~FridaStalker() {
    g_object_unref(m_stalker);
    g_object_unref(m_transformer);
    gum_deinit_embedded();
    fmt::print("num_inst: {:d}\n", logger().num_inst());
}

void FridaStalker::follow() {
    gum_stalker_follow_me(m_stalker, m_transformer, nullptr);
}

void FridaStalker::follow(GumThreadId thread_id) {
    gum_stalker_follow(m_stalker, thread_id, m_transformer, nullptr);
}

void FridaStalker::unfollow() {
    gum_stalker_unfollow_me(m_stalker);
}

void FridaStalker::unfollow(GumThreadId thread_id) {
    gum_stalker_unfollow(m_stalker, thread_id);
}

void FridaStalker::write_trace(const std::string &trace_path) {
    logger().write_to_file(trace_path, m_macho_regions, m_symbols.get());
}

__attribute__((always_inline)) TraceLog &FridaStalker::logger() {
    return m_log;
}

void FridaStalker::transform_cb(GumStalkerIterator *iterator, GumStalkerOutput *output,
                                gpointer user_data) {
    (void)output;
    while (gum_stalker_iterator_next(iterator, nullptr)) {
        gum_stalker_iterator_put_callout(iterator, instruction_cb, user_data, nullptr);
        gum_stalker_iterator_keep(iterator);
    }
}

void FridaStalker::instruction_cb(GumCpuContext *context, gpointer user_data) {
    (void)context;
    auto thiz = (FridaStalker *)user_data;
    thiz->logger().log(gum_process_get_current_thread_id(), context->pc);
}

// C API

stalker_t create_stalker(int symbolicate) {
    return (stalker_t) new FridaStalker{!!symbolicate};
}

void destroy_stalker(stalker_t stalker) {
    delete (FridaStalker *)stalker;
}

void stalker_follow_me(stalker_t stalker) {
    ((FridaStalker *)stalker)->follow();
}

void stalker_follow_thread(stalker_t stalker, GumThreadId thread_id) {
    ((FridaStalker *)stalker)->follow(thread_id);
}

void stalker_unfollow_me(stalker_t stalker) {
    ((FridaStalker *)stalker)->unfollow();
}

void stalker_unfollow_thread(stalker_t stalker, GumThreadId thread_id) {
    ((FridaStalker *)stalker)->unfollow(thread_id);
}

void stalker_write_trace(stalker_t stalker, const char *path) {
    ((FridaStalker *)stalker)->write_trace(path);
}
