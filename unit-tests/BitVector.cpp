#include "xnu-trace/xnu-trace.h"

#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>

#define TS "[BitVector]"

namespace BV = xnutrace::BitVector;

static uint64_t hash_n(uint8_t nbits, uint64_t val) {
    return xxhash3_64::hash(val) & BV::bit_mask<uint64_t>(0, nbits);
}

TEST_CASE("bit_mask", TS) {
    REQUIRE(BV::bit_mask<uint32_t>(0, 1) == 0b1);
    REQUIRE(BV::bit_mask<uint32_t>(0, 2) == 0b11);
    REQUIRE(BV::bit_mask<uint32_t>(0, 3) == 0b111);
    REQUIRE(BV::bit_mask<uint32_t>(1, 4) == 0b1110);
    REQUIRE(BV::bit_mask<uint32_t>(2, 4) == 0b1100);
    REQUIRE(BV::bit_mask<uint32_t>(4, 8) == 0b1111'0000);
}

TEST_CASE("extract_bits", TS) {
    REQUIRE(BV::extract_bits<uint32_t>(BV::bit_mask<uint32_t>(0, 1), 0, 1) ==
            BV::bit_mask<uint32_t>(0, 1));
    REQUIRE(BV::extract_bits<uint32_t>(BV::bit_mask<uint32_t>(0, 2), 0, 2) ==
            BV::bit_mask<uint32_t>(0, 2));
    REQUIRE(BV::extract_bits<uint32_t>(BV::bit_mask<uint32_t>(0, 3), 0, 3) ==
            BV::bit_mask<uint32_t>(0, 3));
    REQUIRE(BV::extract_bits<uint32_t>(BV::bit_mask<uint32_t>(1, 4), 1, 4) ==
            BV::bit_mask<uint32_t>(1, 4) >> 1);
    REQUIRE(BV::extract_bits<uint32_t>(BV::bit_mask<uint32_t>(4, 8), 4, 8) ==
            BV::bit_mask<uint32_t>(4, 8) >> 4);

    REQUIRE(BV::extract_bits<uint32_t>(0b1, 0, 1) == 0b1);
    REQUIRE(BV::extract_bits<uint32_t>(0b10, 1, 2) == 0b1);
    REQUIRE(BV::extract_bits<uint32_t>(0b1000'0000, 7, 8) == 0b1);

    REQUIRE(BV::extract_bits<uint32_t>(0b11, 0, 2) == 0b11);
    REQUIRE(BV::extract_bits<uint32_t>(0b110, 1, 3) == 0b11);
    REQUIRE(BV::extract_bits<uint32_t>(0b1'1000'0000, 7, 9) == 0b11);

    REQUIRE(BV::extract_bits<uint32_t>(0b101, 0, 3) == 0b101);
    REQUIRE(BV::extract_bits<uint32_t>(0b1010, 1, 4) == 0b101);
    //                                   98'7654'3210
    REQUIRE(BV::extract_bits<uint32_t>(0b10'1000'0000, 7, 10) == 0b101);
    REQUIRE_FALSE(BV::extract_bits<uint32_t>(0b10'1000'0000, 7, 9) == 0b101);

    //                                   8'7654'3210
    REQUIRE(BV::extract_bits<uint32_t>(0b1'0101'0000, 4, 9) == 0b1'0101);
}

TEST_CASE("exact", TS) {
    constexpr uint8_t nbits = 16;
    constexpr size_t sz     = 16;
    auto bv                 = ExactBitVector<nbits, false>(sz);
    for (size_t i = 0; i < sz; ++i) {
        bv.set(i, hash_n(nbits, i));
    }

    for (size_t i = 0; i < sz; ++i) {
        REQUIRE(bv.get(i) == hash_n(nbits, i));
    }
}

TEST_CASE("non_atomic_smol_all_ones", TS) {
    constexpr uint8_t nbits = 31;
    constexpr size_t sz     = 4;
    auto bv                 = NonAtomicBitVector<nbits, false>(sz);
    for (size_t i = 0; i < sz; ++i) {
        bv.set(i, BV::bit_mask<uint32_t>(0, nbits));
    }

    for (size_t i = 0; i < sz; ++i) {
        REQUIRE(bv.get(i) == BV::bit_mask<uint32_t>(0, nbits));
    }
}

// TEST_CASE("non_atomic_smol", TS) {
//     constexpr uint8_t nbits = 31;
//     constexpr size_t sz     = 4;
//     auto bv                 = NonAtomicBitVector<nbits, false>(sz);
//     for (size_t i = 0; i < sz; ++i) {
//         bv.set(i, hash_n(nbits, i));
//     }

//     for (size_t i = 0; i < sz; ++i) {
//         REQUIRE(bv.get(i) == hash_n(nbits, i));
//     }
// }

// TEST_CASE("non_atomic_thicc", TS) {
//     constexpr uint8_t nbits = 31;
//     constexpr size_t sz     = 17;
//     auto bv                 = NonAtomicBitVector<nbits, false>(sz);
//     for (size_t i = 0; i < sz; ++i) {
//         bv.set(i, hash_n(nbits, i));
//     }

//     for (size_t i = 0; i < sz; ++i) {
//         REQUIRE(bv.get(i) == hash_n(nbits, i));
//     }
// }
