#pragma once

#include "common.h"

#include "Atomic.h"
#include "utils.h"

#include <type_traits>

#include <icecream.hpp>

template <uint8_t NBitsMax, bool Signed = false, bool AtomicWrite = false> class BitVector {
public:
    static_assert_cond(NBitsMax > 0 && NBitsMax <= 32);
    using T                        = int_n<NBitsMax, Signed>;
    static constexpr uint8_t TBits = sizeofbits<T>();
    using _DT                      = uint_n<sizeofbits<T>() * 2>;
    using DT                       = std::conditional_t<AtomicWrite, void, _DT>;
    static constexpr uint8_t DBits = AtomicWrite ? 0 : sizeofbits<_DT>();
    using _QT                      = uint_n<sizeofbits<T>() * 4>;
    using AQT                      = std::conditional_t<AtomicWrite, std::atomic<_QT>, void>;
    static constexpr uint8_t QBits = AtomicWrite ? sizeofbits<_QT>() : 0;

    BitVector(uint8_t nbits, size_t sz) : m_nbits{nbits}, m_sz{sz} {
        assert(nbits > 0 && nbits <= NBitsMax);
    }

    static constexpr bool exact_fit(uint8_t nbits) {
        return nbits >= 8 && is_pow2(nbits);
    }

    static size_t buf_size(uint8_t nbits, size_t sz) {
        if (exact_fit(nbits)) {
            return sz;
        }
        const auto total_packed_bits = nbits * sz;
        const auto write_bits        = AtomicWrite ? QBits : DBits;
        const auto write_total_bit_sz =
            ((total_packed_bits + write_bits - 1) / write_bits) * write_bits;
        const auto buf_sz = write_total_bit_sz / TBits;
        IC(AtomicWrite, type_name<T>(), NBitsMax, TBits, nbits, sz, total_packed_bits, write_bits,
           write_total_bit_sz, buf_sz);
        // fmt::print("TBits: {:d} nbits: {:d} sz: {:d} total_packed_bits: {:d} write_bits:
        // {:d}\n");
        return buf_sz;
    }

private:
    std::vector<T> m_buf;
    size_t m_sz;
    uint8_t m_nbits;
};

template <uint8_t NBitsMax, bool Signed> using AtomicBitVector = BitVector<NBitsMax, Signed, true>;
