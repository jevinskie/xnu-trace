#pragma once

#include "common.h"

enum class gpr_idx : uint8_t {
    x0,
    x1,
    x2,
    x3,
    x4,
    x5,
    x6,
    x7,
    x8,
    x9,
    x10,
    x11,
    x12,
    x13,
    x14,
    x15,
    x16,
    x17,
    x18,
    x19,
    x20,
    x21,
    x22,
    x23,
    x24,
    x25,
    x26,
    x27,
    x28,
    fp /* x29 */,
    lr /* x30 */,
    sp /* x31 */,
    pc // pc = "x32" 34th
};

constexpr uint8_t gpr_idx_sz = (uint8_t)gpr_idx::pc + 1; // pc = 33, sz = 34

enum class vec_idx : uint8_t {
    v0 = 0,
    v1,
    v2,
    v3,
    v4,
    v5,
    v6,
    v7,
    v8,
    v9,
    v10,
    v11,
    v12,
    v13,
    v14,
    v15,
    v16,
    v17,
    v18,
    v19,
    v20,
    v21,
    v22,
    v23,
    v24,
    v25,
    v26,
    v27,
    v28,
    v29,
    v30,
    v31 // v31 32nd
};

constexpr uint8_t vec_idx_sz = (uint8_t)vec_idx::v31 + 1; // v31 = 31, s = 32

struct log_msg_hdr {
    uint32_t gpr_changed;
    uint32_t vec_changed;
} __attribute__((packed));

struct log_region {
    uint64_t base;
    uint64_t size;
    uint64_t slide;
    uint8_t uuid[16];
    uint8_t digest_sha256[32];
    uint64_t is_jit;
    uint64_t path_len;
} __attribute__((packed));

struct log_sym {
    uint64_t base;
    uint64_t size;
    uint64_t name_len;
    uint64_t path_len;
} __attribute__((packed));

struct log_comp_hdr {
    uint64_t magic;
    uint64_t is_compressed;
    uint64_t header_size;
    uint64_t decompressed_size;
} __attribute__((packed));

struct log_thread_hdr {
    uint64_t thread_id;
    uint64_t num_inst;
} __attribute__((packed));

struct log_meta_hdr {
    uint64_t num_regions;
    uint64_t num_syms;
} __attribute__((packed));

struct log_macho_region_hdr {
    uint8_t digest_sha256[32];
} __attribute__((packed));

constexpr uint64_t log_meta_hdr_magic         = 0x8d3a'dfb8'4154'454dull; // 'META'
constexpr uint64_t log_thread_hdr_magic       = 0x8d3a'dfb8'4452'4854ull; // 'THRD'
constexpr uint64_t log_macho_region_hdr_magic = 0x8d3a'dfb8'4843'414dull; // 'MACH'
