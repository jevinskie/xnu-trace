#pragma once

#include "common.h"

#include "Atomic.h"
#include "utils.h"

#include <memory>
#include <type_traits>

#include <icecream.hpp>

template <uint8_t NBits, bool Signed = false> class ExactBitVector;

template <uint8_t NBitsMax, bool Signed = false> class BitVectorBase {
public:
    static_assert_cond(NBitsMax > 0 && NBitsMax <= 32);
    using T = int_n<NBitsMax, Signed>;
    virtual T get(size_t idx);
    virtual T set(size_t idx, T val);
    virtual ~BitVectorBase() {}

private:
    std::vector<uint8_t> m_buf;
};

template <uint8_t NBits, bool Signed> class ExactBitVector : public BitVectorBase<NBits, Signed> {
public:
    static_assert_cond(NBits >= 8 && is_pow2(NBits));

private:
};

template <uint8_t NBitsMax, bool Signed = false, bool AtomicWrite = false> class BitVector {
public:
    static_assert_cond(NBitsMax > 0 && NBitsMax <= 32);
    BitVector(uint8_t nbits, size_t sz) {
        if (nbits >= 8 && is_pow2(nbits)) {
            m_bv = std::make_unique<ExactBitVector<NBitsMax, Signed>>(sz);
        }
    }

private:
    std::unique_ptr<BitVectorBase<NBitsMax, Signed>> *m_bv;
};

template <uint8_t NBitsMax, bool Signed = false, bool AtomicWrite = false> class BitVectorMega {
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

    BitVectorMega(uint8_t nbits, size_t sz) : m_nbits{nbits}, m_sz{sz} {
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

template <uint8_t NBitsMax, bool Signed>
using AtomicBitVectorMega = BitVectorMega<NBitsMax, Signed, true>;
