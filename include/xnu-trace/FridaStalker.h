#pragma once

#include "common.h"

#include "MachORegions.h"
#include "Symbols.h"
#include "TraceLog.h"
#include "VMRegions.h"

#include <sys/types.h>

struct _GumStalker;
typedef _GumStalker GumStalker;
struct _GumStalkerTransformer;
typedef _GumStalkerTransformer GumStalkerTransformer;

class XNUTRACE_EXPORT FridaStalker {
public:
    FridaStalker(const std::string &log_dir_path, bool symbolicate, int compression_level,
                 bool stream);
    ~FridaStalker();
    void follow();
    void follow(size_t thread_id);
    void unfollow();
    void unfollow(size_t thread_id);
    XNUTRACE_INLINE TraceLog &logger();

private:
    void write_trace();
    static void transform_cb(void *iterator, void *output, void *user_data);
    static void instruction_cb(void *context, void *user_data);

    GumStalker *m_stalker;
    GumStalkerTransformer *m_transformer;
    TraceLog m_log;
    MachORegions m_macho_regions;
    VMRegions m_vm_regions;
    std::unique_ptr<Symbols> m_symbols;
};
