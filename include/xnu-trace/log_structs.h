#pragma once

#include "common.h"

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
    static constexpr uint64_t magic = 0x8d3a'dfb8'4452'4854ull; // 'THRD'
} __attribute__((packed));

struct log_meta_hdr {
    uint64_t num_regions;
    uint64_t num_syms;
    static constexpr uint64_t magic = 0x8d3a'dfb8'4154'454dull; // 'META'
} __attribute__((packed));

struct log_macho_region_hdr {
    uint8_t digest_sha256[32];
    static constexpr uint64_t magic = 0x8d3a'dfb8'4843'414dull; // 'MACH'
} __attribute__((packed));

// clang-format off
enum class gpr_idx : uint8_t {
    x0, x1, x2, x3, x4, x5, x6, x7,
    x8, x9, x10, x11, x12, x13, x14, x15,
    x16, x17, x18, x19, x20, x21, x22, x23,
    x24, x25, x26, x27, x28, fp /* x29 */, lr /* x30 */, sp /* x31 */
};
// clang-format on

constexpr uint8_t gpr_idx_sz = (uint8_t)gpr_idx::sp + 1; // sz = 32

// 31  292827262524      2019      1514      10 9       5 4       0
// ┌─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┐
// │ ngc │   |s│b│   gc4   │   gc3   │   gc2   │   gc1   │   gc0   │
// └─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┘

constexpr int8_t rpc_num_changed_max = 5;

constexpr size_t rpc_changed_max_sz = sizeof(log_msg_hdr) + 2 * sizeof(uint64_t) /* pc/sp */ +
                                      rpc_num_changed_max * sizeof(uint64_t) /* gpr */ +
                                      rpc_num_changed_max * sizeof(uint128_t) /* vec */;

constexpr int8_t rpc_num_changed(uint32_t reg_packed_changes) {
    return reg_packed_changes >> 29;
}

constexpr bool rpc_pc_branched(uint32_t reg_packed_changes) {
    return !!(reg_packed_changes & (1 << 25));
}

constexpr bool rpc_sp_changed(uint32_t reg_packed_changes) {
    return !!(reg_packed_changes & (1 << 26));
}

constexpr int8_t rpc_reg_idx(uint32_t reg_packed_changes, uint8_t changed_idx) {
    return (reg_packed_changes >> (5 * changed_idx)) & 0b1'1111;
}

constexpr uint32_t rpc_set_reg_idx(uint32_t reg_packed_changes, uint8_t changed_idx,
                                   uint8_t reg_idx) {
    return reg_packed_changes | (reg_idx << (5 * changed_idx));
}

constexpr uint32_t rpc_set_num_changed(uint32_t reg_packed_changes, uint8_t num_changed) {
    return reg_packed_changes | (num_changed << 29);
}

constexpr uint32_t rpc_set_pc_branched(uint32_t reg_packed_changes) {
    return reg_packed_changes | (1 << 25);
}

constexpr uint32_t rpc_set_sp_changed(uint32_t reg_packed_changes) {
    return reg_packed_changes | (1 << 26);
}

// clang-format off
enum class vec_idx : uint8_t {
    v0 = 0, v1, v2, v3, v4, v5, v6, v7,
    v8, v9, v10, v11, v12, v13, v14, v15,
    v16, v17, v18, v19, v20, v21, v22, v23,
    v24, v25, v26, v27, v28, v29, v30, v31
};
// clang-format on

constexpr uint8_t vec_idx_sz = (uint8_t)vec_idx::v31 + 1; // sz = 32

constexpr size_t log_msg_size(const log_msg_hdr *msg_hdr) {
    size_t sz = sizeof(*msg_hdr);
    sz += rpc_pc_branched(msg_hdr->gpr_changed) * sizeof(uint64_t);
    sz += rpc_sp_changed(msg_hdr->gpr_changed) * sizeof(uint64_t);
    sz += rpc_num_changed(msg_hdr->gpr_changed) * sizeof(uint64_t);
    sz += rpc_num_changed(msg_hdr->vec_changed) * sizeof(uint128_t);
    return sz;
}
