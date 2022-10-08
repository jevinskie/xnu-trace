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
    constexpr uint8_t nbits = 31;
    constexpr size_t sz     = 4;
    auto bv                 = NonAtomicBitVector<nbits, false>(sz);
    // for (size_t i = 0; i < NUM_ELEM; ++i) {
    //     bv.set(i, hash_n(nbits, i));
    // }

    // for (size_t i = 0; i < NUM_ELEM; ++i) {
    //     REQUIRE(bv.get(i) == hash_n(nbits, i));
    // }
}

TEST_CASE("non_atomic_thicc", TS) {
    constexpr uint8_t nbits = 31;
    constexpr size_t sz     = 124;
    auto bv                 = NonAtomicBitVector<nbits, false>(sz);
    // for (size_t i = 0; i < NUM_ELEM; ++i) {
    //     bv.set(i, hash_n(nbits, i));
    // }

    // for (size_t i = 0; i < NUM_ELEM; ++i) {
    //     REQUIRE(bv.get(i) == hash_n(nbits, i));
    // }
}

// TEST_CASE("exact_fit", TS) {
//     const auto ebv = BitVectorMega<16, false>{16, 0};
//     REQUIRE(ebv.exact_fit(16));
//     const auto ebv2 = BitVectorMega<16, false>{8, 0};
//     REQUIRE(ebv.exact_fit(8));
//     const auto ebv3 = BitVectorMega<32, false>{16, 0};
//     REQUIRE(ebv.exact_fit(16));
//     const auto nebv = BitVectorMega<16, false>{15, 0};
//     REQUIRE_FALSE(nebv.exact_fit(15));
//     const auto nebv2 = BitVectorMega<16, false>{15, 0};
//     REQUIRE_FALSE(nebv2.exact_fit(7));
//     const auto nebv3 = BitVectorMega<32, false>{24, 0};
//     REQUIRE_FALSE(nebv3.exact_fit(24));
//     const auto nebv4 = BitVectorMega<32, false>{4, 0};
//     REQUIRE_FALSE(nebv4.exact_fit(4));
// }

// TEST_CASE("buf_size", TS) {
//     const auto ebv = BitVectorMega<16, false>{16, 0};
//     REQUIRE(ebv.buf_size(16, 4) == 4);
//     const auto bv = BitVectorMega<16, false>{15, 0};
//     REQUIRE(bv.buf_size(15, 3) == 3);
//     const auto abv = AtomicBitVectorMega<16, false>{15, 0};
//     REQUIRE(abv.buf_size(15, 3) == 4);
// }

// TEST_CASE("build_bv", TS) {
//     auto bv = BitVectorMega<32, false>{31, 243};
// }

// TEST_CASE("build_abv", TS) {
//     auto abv = AtomicBitVectorMega<32, false>{31, 243};
// }
