#pragma once

#include "common.h"

struct drcov_bb_t {
    uint32_t mod_off;
    uint16_t sz;
    uint16_t mod_id;
} __attribute__((packed));
