#pragma once

#include "common.h"

#include "Atomic.h"
#include "utils.h"

#include <memory>
#include <type_traits>

#include <icecream.hpp>

template <typename T> static constexpr T extract_bits(T val, uint8_t sb, uint8_t eb) {
    return (val >> sb) & ((1 << (eb - sb)) - 1);
}

template <typename T> static constexpr T sign_extend(T val, uint8_t nbits) {
    const T msb = 1 << (nbits - 1);
    return (val ^ msb) - msb;
}

template <uint8_t NBits, bool Signed = false> class BitVectorBase {
public:
    static_assert_cond(NBits > 0 && NBits <= 32);
    using T                             = int_n<NBits, Signed>;
    static constexpr size_t TBits       = sizeofbits<T>();
    virtual T get(size_t idx) const     = 0;
    virtual void set(size_t idx, T val) = 0;
    virtual ~BitVectorBase() {}

protected:
    BitVectorBase(size_t byte_sz) : m_buf(byte_sz) {}

    std::vector<uint8_t> &buf() {
        return m_buf;
    }
    const std::vector<uint8_t> &buf() const {
        return m_buf;
    }

    uint8_t *data() {
        return m_buf.data();
    }
    const uint8_t *data() const {
        return m_buf.data();
    }

private:
    std::vector<uint8_t> m_buf;
};

template <uint8_t NBits, bool Signed> class ExactBitVector : public BitVectorBase<NBits, Signed> {
public:
    using Base = BitVectorBase<NBits, Signed>;
    using T    = typename Base::T;
    static_assert_cond(NBits >= 8 && is_pow2(NBits));

    ExactBitVector(size_t sz) : Base(byte_sz(sz)) {}

    T get(size_t idx) const final override {
        return ((T *)Base::data())[idx];
    }

    void set(size_t idx, T val) final override {
        ((T *)Base::data())[idx] = val;
    }

    static constexpr size_t byte_sz(size_t sz) {
        return sz * NBits / 8;
    }
};

template <uint8_t NBits, bool Signed>
class NonAtomicBitVector : public BitVectorBase<NBits, Signed> {
public:
    using Base                      = BitVectorBase<NBits, Signed>;
    using T                         = typename Base::T;
    static constexpr size_t TBits   = Base::TBits;
    using DT                        = uint_n<sizeofbits<T>() * 2>;
    static constexpr uint8_t DTBits = sizeofbits<DT>();
    static_assert_cond(NBits != 8 && NBits != 16 && NBits != 32);

    NonAtomicBitVector(size_t sz) : Base(byte_sz(sz)) {}

    T get(size_t idx) const final override {
        const auto sw_idx = start_word_idx(idx);
        const auto ew_idx = end_word_idx(idx);
        if (sw_idx == ew_idx) {
            const T mixed_word       = ((T *)Base::data())[sw_idx];
            const auto s_bidx        = start_bit_idx(idx) % TBits;
            const auto e_bidx        = end_bit_idx(idx) % TBits;
            const auto extracted_val = extract_bits(mixed_word, s_bidx, e_bidx);
            if constexpr (!Signed) {
                return extracted_val;
            } else {
                return sign_extend(extracted_val, NBits);
            }
        } else {
            const auto sdw_idx       = sw_idx / 2;
            const DT mixed_dword     = ((DT *)Base::data())[sdw_idx];
            const auto sd_bidx       = start_bit_idx(idx) % DTBits;
            const auto ed_bidx       = end_bit_idx(idx) % DTBits;
            const auto extracted_val = (T)extract_bits(mixed_dword, sd_bidx, ed_bidx);
            if constexpr (!Signed) {
                return extracted_val;
            } else {
                return sign_extend(extracted_val, NBits);
            }
        }
    }

    void set(size_t idx, T val) final override {
        ((T *)Base::data())[idx] = val;
    }

    static constexpr size_t start_bit_idx(size_t idx) {
        return NBits * idx;
    }

    static constexpr size_t end_bit_idx(size_t idx) {
        return NBits * (idx + 1);
    }

    static constexpr size_t start_word_idx(size_t idx) {
        return start_bit_idx(idx) / TBits;
    }

    static constexpr size_t end_word_idx(size_t idx) {
        return end_bit_idx(idx) / TBits;
    }

    static constexpr size_t start_dword_idx(size_t idx) {
        return start_bit_idx(idx) / DTBits;
    }

    static constexpr size_t end_dword_idx(size_t idx) {
        return end_bit_idx(idx) / DTBits;
    }

    static constexpr size_t byte_sz(size_t sz) {
        const auto total_packed_bits  = NBits * sz;
        const auto write_total_bit_sz = ((total_packed_bits + DTBits - 1) / DTBits) * DTBits;
        const auto num_bytes          = write_total_bit_sz / 8;
        return num_bytes;
    }
};

template <uint8_t NBitsMax, bool Signed = false, bool AtomicWrite = false>
class XNUTRACE_EXPORT BitVector {
public:
    using T = int_n<NBitsMax, Signed>;
    static_assert_cond(NBitsMax > 0 && NBitsMax <= 32);
    BitVector(uint8_t nbits, size_t sz) {
        if (nbits >= 8 && is_pow2(nbits)) {
            m_bv = std::make_unique<ExactBitVector<NBitsMax, Signed>>(sz);
        }
    }
    T get(size_t idx) const {
        return m_bv->get(idx);
    }
    void set(size_t idx, T val) {
        m_bv->set(idx, val);
    }

private:
    std::unique_ptr<BitVectorBase<NBitsMax, Signed>> m_bv;
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
        return buf_sz;
    }

private:
    std::vector<T> m_buf;
    size_t m_sz;
    uint8_t m_nbits;
};

template <uint8_t NBitsMax, bool Signed>
using AtomicBitVectorMega = BitVectorMega<NBitsMax, Signed, true>;
