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

TEST_CASE("insert_bits", TS) {
    REQUIRE(BV::insert_bits<uint32_t>(0, 0b1u, 0, 1) == 0b1);
    REQUIRE(BV::insert_bits<uint32_t>(0, 0b10u, 1, 2) == 0b100);
    REQUIRE(BV::insert_bits<uint32_t>(0, 0b1000'0000u, 1, 8) == 0b1'0000'0000);

    REQUIRE(BV::insert_bits<uint32_t>(1, 0b11u, 1, 2) == 0b111);
    REQUIRE(BV::insert_bits<uint32_t>(1, 0b110u, 1, 3) == 0b1101);
    REQUIRE(BV::insert_bits<uint32_t>(1, 0b1'1000'0000u, 1, 9) == 0b1'1000'00001);
}

TEST_CASE("exact_impl", TS) {
    constexpr uint8_t nbits = 16;
    constexpr size_t sz     = 8;
    auto bv                 = ExactBitVectorImpl<nbits>(sz);
    for (size_t i = 0; i < sz; ++i) {
        bv.set(i, hash_n(nbits, i));
    }

    for (size_t i = 0; i < sz; ++i) {
        REQUIRE(bv.get(i) == hash_n(nbits, i));
    }
}

TEST_CASE("exact", TS) {
    constexpr uint8_t nbits = 16;
    constexpr size_t sz     = 8;
    auto bv                 = BitVectorFactory<>(nbits, sz);
    for (size_t i = 0; i < sz; ++i) {
        bv->set(i, hash_n(nbits, i));
    }

    for (size_t i = 0; i < sz; ++i) {
        REQUIRE(bv->get(i) == hash_n(nbits, i));
    }
}

TEST_CASE("exact_signed", TS) {
    constexpr uint8_t nbits = 32;
    constexpr size_t sz     = 8;
    auto bv                 = BitVectorFactory<true>(nbits, sz);
    for (size_t i = 0; i < sz; ++i) {
        bv->set(i, make_signed_v(i) - (sz / 2));
    }

    for (size_t i = 0; i < sz; ++i) {
        REQUIRE(bv->get(i) == make_signed_v(i) - (sz / 2));
    }
}

TEST_CASE("non_atomic_smol_all_ones_impl", TS) {
    constexpr uint8_t nbits = 15;
    constexpr size_t sz     = 4;
    auto bv                 = NonAtomicBitVectorImpl<nbits>(sz);
    for (size_t i = 0; i < sz; ++i) {
        bv.set(i, BV::bit_mask<uint32_t>(0, nbits));
    }

    for (size_t i = 0; i < sz; ++i) {
        REQUIRE(bv.get(i) == BV::bit_mask<uint32_t>(0, nbits));
    }
}

TEST_CASE("non_atomic_mid_all_ones_impl", TS) {
    constexpr uint8_t nbits = 31;
    constexpr size_t sz     = 4;
    auto bv                 = NonAtomicBitVectorImpl<nbits>(sz);
    for (size_t i = 0; i < sz; ++i) {
        bv.set(i, BV::bit_mask<uint32_t>(0, nbits));
    }

    for (size_t i = 0; i < sz; ++i) {
        REQUIRE(bv.get(i) == BV::bit_mask<uint32_t>(0, nbits));
    }
}

TEST_CASE("non_atomic_mid_all_ones", TS) {
    constexpr uint8_t nbits = 31;
    constexpr size_t sz     = 4;
    auto bv                 = BitVectorFactory<>(nbits, sz);
    for (size_t i = 0; i < sz; ++i) {
        bv->set(i, BV::bit_mask<uint32_t>(0, nbits));
    }

    for (size_t i = 0; i < sz; ++i) {
        REQUIRE(bv->get(i) == BV::bit_mask<uint32_t>(0, nbits));
    }
}

TEST_CASE("non_atomic_mid_signed", TS) {
    constexpr uint8_t nbits = 31;
    constexpr size_t sz     = 8;
    auto bv                 = BitVectorFactory<true>(nbits, sz);
    for (size_t i = 0; i < sz; ++i) {
        bv->set(i, make_signed_v(i) - (sz / 2));
    }

    for (size_t i = 0; i < sz; ++i) {
        REQUIRE(bv->get(i) == make_signed_v(i) - (sz / 2));
    }
}

// TEST_CASE("non_atomic_smol", TS) {
//     constexpr uint8_t nbits = 31;
//     constexpr size_t sz     = 4;
//     auto bv                 = NonAtomicBitVectorImpl<nbits, false>(sz);
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
//     auto bv                 = NonAtomicBitVectorImpl<nbits, false>(sz);
//     for (size_t i = 0; i < sz; ++i) {
//         bv.set(i, hash_n(nbits, i));
//     }

//     for (size_t i = 0; i < sz; ++i) {
//         REQUIRE(bv.get(i) == hash_n(nbits, i));
//     }
// }
