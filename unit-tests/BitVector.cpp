#include "xnu-trace/xnu-trace.h"

#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>

#define TS "[BitVector]"

static constexpr size_t NUM_ELEM  = 4 * 64 * 1024;
static constexpr uint8_t NUM_BITS = 18;

static uint64_t hash_n(uint8_t nbits, uint64_t val) {
    return xxhash3_64::hash(val) & ((nbits << 1) - 1);
}

TEST_CASE("exact", TS) {
    constexpr uint8_t nbits = 32;
    auto bv                 = ExactBitVector<nbits, false>(NUM_ELEM);
    for (size_t i = 0; i < NUM_ELEM; ++i) {
        bv.set(i, hash_n(nbits, i));
    }

    for (size_t i = 0; i < NUM_ELEM; ++i) {
        REQUIRE(bv.get(i) == hash_n(nbits, i));
    }
}

TEST_CASE("non_atomic_smol", TS) {
    constexpr uint8_t nbits = 15;
    constexpr size_t sz     = 4;
    auto bv                 = NonAtomicBitVector<nbits, false>(sz);
    for (size_t i = 0; i < sz; ++i) {
        bv.set(i, hash_n(nbits, i));
    }

    for (size_t i = 0; i < sz; ++i) {
        REQUIRE(bv.get(i) == hash_n(nbits, i));
    }
}

TEST_CASE("non_atomic_thicc", TS) {
    constexpr uint8_t nbits = 15;
    constexpr size_t sz     = 17;
    auto bv                 = NonAtomicBitVector<nbits, false>(sz);
    for (size_t i = 0; i < sz; ++i) {
        bv.set(i, hash_n(nbits, i));
    }

    for (size_t i = 0; i < sz; ++i) {
        REQUIRE(bv.get(i) == hash_n(nbits, i));
    }
}
