#pragma once

#include "common.h"

#include <experimental/fixed_capacity_vector>

struct log_msg;

struct log_arm64_cpu_context {
    uint64_t pc;   // off 0x000
    uint64_t sp;   // off 0x008
    uint64_t nzcv; // off 0x010

    uint64_t x[29]; // off 0x018
    uint64_t fp;    // off 0x100
    uint64_t lr;    // off 0x108

    uint128_t v[32]; // off 0x110
    // sz 0x310

    void update(const log_msg &msg);
};

static_assert(sizeof(log_arm64_cpu_context) % sizeof(uint64_t) == 0,
              "log_arm64_cpu_context not 8 byte aligned");

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
// │ ngc │ |c|s│b│   gc4   │   gc3   │   gc2   │   gc1   │   gc0   │
// └─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┘

constexpr int8_t rpc_num_changed_max = 5;

constexpr uint32_t rpc_num_changed(uint32_t reg_packed_changes) {
    return reg_packed_changes >> 29;
}

constexpr uint32_t rpc_num_fixed_changed(uint32_t reg_packed_changes) {
    return ((reg_packed_changes & (1 << 25)) >> 25) + ((reg_packed_changes & (1 << 26)) >> 26);
}

constexpr bool rpc_pc_branched(uint32_t reg_packed_changes) {
    return !!(reg_packed_changes & (1 << 25));
}

constexpr bool rpc_sp_changed(uint32_t reg_packed_changes) {
    return !!(reg_packed_changes & (1 << 26));
}

constexpr bool rpc_sync(uint32_t reg_packed_changes) {
    return !!(reg_packed_changes & (1 << 27));
}

constexpr uint32_t rpc_reg_idx(uint32_t reg_packed_changes, uint32_t changed_idx) {
    return (reg_packed_changes >> (5 * changed_idx)) & 0b1'1111;
}

constexpr uint32_t rpc_set_reg_idx(uint32_t reg_packed_changes, uint32_t changed_idx,
                                   uint32_t reg_idx) {
    return reg_packed_changes | (reg_idx << (5 * changed_idx));
}

constexpr uint32_t rpc_set_num_changed(uint32_t reg_packed_changes, uint32_t num_changed) {
    return reg_packed_changes | (num_changed << 29);
}

constexpr uint32_t rpc_set_pc_branched(uint32_t reg_packed_changes) {
    return reg_packed_changes | (1 << 25);
}

constexpr uint32_t rpc_set_sp_changed(uint32_t reg_packed_changes) {
    return reg_packed_changes | (1 << 26);
}

constexpr uint32_t rpc_set_sync(uint32_t reg_packed_changes) {
    return reg_packed_changes | (1 << 27);
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

struct log_msg {
    using changed_gpr_t = std::experimental::fixed_capacity_vector<std::pair<uint32_t, uint64_t>,
                                                                   rpc_num_changed_max>;
    using changed_vec_t = std::experimental::fixed_capacity_vector<std::pair<uint32_t, uint128_t>,
                                                                   rpc_num_changed_max>;
    uint32_t gpr_changed;
    uint32_t vec_changed;
    size_t size() const {
        if (is_sync_frame()) {
            return size_full_ctx;
        }
        return sizeof(*this) + num_fixed() * sizeof(uint64_t) + num_gpr() * sizeof(uint64_t) +
               num_vec() * sizeof(uint128_t);
    }
    uint32_t num_fixed() const {
        return rpc_num_fixed_changed(gpr_changed);
    }
    bool pc_branched() const {
        return rpc_pc_branched(gpr_changed);
    }
    bool sp_changed() const {
        return rpc_sp_changed(gpr_changed);
    }
    uint64_t pc() const {
        const auto off = sizeof(*this);
        return *(uint64_t *)((uintptr_t)this + off);
    }
    uint64_t sp() const {
        const auto off = sizeof(*this) + pc_branched() * sizeof(uint64_t);
        return *(uint64_t *)((uintptr_t)this + off);
    }
    uint32_t num_gpr() const {
        return rpc_num_changed(gpr_changed);
    }
    uint32_t gpr_idx(uint32_t change_idx) const {
        return rpc_reg_idx(gpr_changed, change_idx);
    }
    uint64_t gpr(uint32_t change_idx) const {
        const auto off = sizeof(*this) + num_fixed() * sizeof(uint64_t);
        return ((uint64_t *)((uintptr_t)this + off))[change_idx];
    }
    changed_gpr_t changed_gpr() const {
        changed_gpr_t res;
        for (uint32_t i = 0; i < num_gpr(); ++i) {
            res.emplace_back(std::make_pair(gpr_idx(i), gpr(i)));
        }
        return res;
    }
    uint32_t num_vec() const {
        return rpc_num_changed(vec_changed);
    }
    uint32_t vec_idx(uint32_t change_idx) const {
        return rpc_reg_idx(vec_changed, change_idx);
    }
    uint128_t vec(uint32_t change_idx) const {
        const auto off =
            sizeof(*this) + num_fixed() * sizeof(uint64_t) + num_gpr() * sizeof(uint64_t);
        return ((uint128_t *)((uintptr_t)this + off))[change_idx];
    }
    changed_vec_t changed_vec() const {
        changed_vec_t res;
        for (uint32_t i = 0; i < num_vec(); ++i) {
            res.emplace_back(std::make_pair(vec_idx(i), vec(i)));
        }
        return res;
    }
    bool is_sync_frame() const {
        return rpc_sync(gpr_changed);
    }
    static constexpr size_t size_max = 2 * sizeof(uint32_t) /* hdr */ +
                                       2 * sizeof(uint64_t) /* pc/sp */ +
                                       rpc_num_changed_max * sizeof(uint64_t) /* gpr */ +
                                       rpc_num_changed_max * sizeof(uint128_t) /* vec */;
    static constexpr uint64_t sync_frame_buf[] = {((uint64_t)0 << 32) | rpc_set_sync(0),
                                                  0x1b30'aabd'5359'4e43ULL /* SYNC */,
                                                  0x7699'0430'4a1b'4410ULL,
                                                  0x9c62'5989'63b9'7672ULL,
                                                  0x43d5'3630'a5ea'edd9ULL,
                                                  0x6dc3'de59'4553'5e98ULL,
                                                  0x6089'461f'fc1f'b52bULL,
                                                  0xa4e0'a6f1'2861'b739ULL,
                                                  0x3404'3c2d'70ca'6e6fULL,
                                                  0xd6e1'dba5'098c'd02aULL,
                                                  0xf2e6'b552'4519'baccULL,
                                                  0xfd91'ff9d'e376'3e78ULL,
                                                  0x77f0'f681'59e4'e2e8ULL,
                                                  0x3d5d'2cff'136d'f711ULL,
                                                  0xfee4'c678'6443'd6b8ULL,
                                                  0xf46d'78eb'e3ae'77ddULL,
                                                  0xb135'4b20'367b'48a2ULL,
                                                  0x9ba2'c577'f87b'0c83ULL};
    static constexpr size_t size_full_ctx =
        sizeof(sync_frame_buf) /* hdr/magic */ + sizeof(log_arm64_cpu_context) /* ctx */;
} __attribute__((packed, aligned(8)));

static_assert(sizeof(log_msg) == 2 * sizeof(uint32_t), "log_msg header is not 8 bytes");
static_assert(sizeof(log_msg) % sizeof(uint64_t) == 0, "log_msg not 8 byte aligned");
static_assert(sizeof(log_msg::sync_frame_buf) == log_msg::max_size,
              "log_msg::sync_frame_buf not max_size");

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
    log_arm64_cpu_context initial_ctx;
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
