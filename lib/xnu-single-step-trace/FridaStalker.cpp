#include "common.h"

#define GUM_TYPE_TRACER_STALKER_TRANSFORMER (gum_tracer_stalker_transformer_get_type())
GUM_DECLARE_FINAL_TYPE(GumTracerStalkerTransformer, gum_tracer_stalker_transformer, GUM,
                       TRACER_STALKER_TRANSFORMER, GObject)

struct _GumTracerStalkerTransformer {
    GObject parent;
};

static void tracer_cb(GumCpuContext *context, gpointer user_data) {
    // fmt::print(".");
    // printf(".");
    // __builtin_debugtrap();
}

static void gum_tracer_stalker_transformer_transform_block(GumStalkerTransformer *transformer,
                                                           GumStalkerIterator *iterator,
                                                           GumStalkerOutput *output) {
    while (gum_stalker_iterator_next(iterator, nullptr)) {
        // fmt::print(".");
        gum_stalker_iterator_put_callout(iterator, tracer_cb, nullptr, nullptr);
        gum_stalker_iterator_keep(iterator);
    }
}

static void gum_tracer_stalker_transformer_iface_init(gpointer g_iface, gpointer iface_data) {
    auto *iface = (GumStalkerTransformerInterface *)g_iface;

    iface->transform_block = gum_tracer_stalker_transformer_transform_block;
}

G_DEFINE_TYPE_EXTENDED(GumTracerStalkerTransformer, gum_tracer_stalker_transformer, G_TYPE_OBJECT,
                       0,
                       G_IMPLEMENT_INTERFACE(GUM_TYPE_STALKER_TRANSFORMER,
                                             gum_tracer_stalker_transformer_iface_init))

static void gum_tracer_stalker_transformer_class_init(GumTracerStalkerTransformerClass *klass) {}

static void gum_tracer_stalker_transformer_init(GumTracerStalkerTransformer *self) {}

static GumStalkerTransformer *gum_stalker_transformer_make_tracer(void) {
    return (GumStalkerTransformer *)g_object_new(GUM_TYPE_TRACER_STALKER_TRANSFORMER, nullptr);
}

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
    auto *trace_transformer = gum_stalker_transformer_make_tracer();
    assert(trace_transformer);
    gum_stalker_follow_me(m_stalker, trace_transformer, nullptr);
}

void FridaStalker::follow(GumThreadId thread_id) {
    auto *trace_transformer = gum_stalker_transformer_make_tracer();
    assert(trace_transformer);
    gum_stalker_follow(m_stalker, thread_id, trace_transformer, nullptr);
}

void FridaStalker::unfollow() {
    gum_stalker_unfollow_me(m_stalker);
}

void FridaStalker::unfollow(GumThreadId thread_id) {
    gum_stalker_unfollow(m_stalker, thread_id);
}
