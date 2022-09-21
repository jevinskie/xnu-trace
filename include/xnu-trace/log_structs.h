#pragma once

#include "common.h"

struct log_msg_hdr {
    uint64_t pc;
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
