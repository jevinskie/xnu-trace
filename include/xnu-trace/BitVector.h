#pragma once

#include "common.h"

#include "Atomic.h"
#include "utils.h"

#include <memory>
#include <type_traits>

#include <boost/hana.hpp>
namespace hana = boost::hana;
#include <fmt/format.h>
#include <icecream.hpp>

namespace xnutrace::BitVector {

template <typename T> static constexpr T bit_mask(uint8_t sb, uint8_t eb) {
    const T high_mask = (T{1} << eb) - 1;
    const T low_mask  = (T{1} << sb) - 1;
    return high_mask ^ low_mask;
}

template <typename T> static constexpr T extract_bits(T val, uint8_t sb, uint8_t eb) {
    return (val & bit_mask<T>(sb, eb)) >> sb;
}

template <typename T, typename IT>
static constexpr T insert_bits(T orig_val, IT insert_val, uint8_t sb, uint8_t nbits) {
    const T orig_val_cleared = orig_val & ~bit_mask<T>(sb, sb + nbits);
    return orig_val_cleared | (T(insert_val) << sb);
}

template <typename T> static constexpr T sign_extend(T val, uint8_t nbits) {
    const T msb = 1 << (nbits - 1);
    return (val ^ msb) - msb;
}

template <bool Signed = false, uint8_t RTBits_ = 32> class BitVectorBase {
public:
    static_assert(RTBits_ >= 8 && RTBits_ <= 64 && is_pow2(RTBits_));
    using RT                       = int_n<RTBits_, Signed>;
    static constexpr size_t RTBits = sizeofbits<RT>();

    virtual ~BitVectorBase() {}
    virtual RT get(size_t idx) const     = 0;
    virtual void set(size_t idx, RT val) = 0;

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

template <uint8_t NBits, bool Signed> class ExactBitVectorImpl : public BitVectorBase<Signed> {
public:
    using Base = BitVectorBase<Signed>;
    using RT   = typename Base::RT;
    using T    = int_n<NBits, Signed>;
    static_assert(NBits <= Base::RTBits);
    static_assert(NBits >= 8 && is_pow2(NBits));

    ExactBitVectorImpl(size_t sz) : Base(byte_sz(sz)) {}

    RT get(size_t idx) const final override {
        return ((T *)Base::data())[idx];
    }

    void set(size_t idx, RT val) final override {
        ((T *)Base::data())[idx] = T(val);
    }

    static constexpr size_t byte_sz(size_t sz) {
        return sz * NBits / 8;
    }
};

template <uint8_t NBits, bool Signed> class NonAtomicBitVectorImpl : public BitVectorBase<Signed> {
public:
    using Base                      = BitVectorBase<Signed>;
    using RT                        = typename Base::RT;
    using T                         = int_n<NBits, Signed>;
    static constexpr size_t TBits   = sizeofbits<T>();
    using DT                        = uint_n<sizeofbits<T>() * 2>;
    static constexpr uint8_t DTBits = sizeofbits<DT>();
    static_assert(NBits <= Base::RTBits);
    static_assert(NBits != 8 && NBits != 16 && NBits != 32);

    NonAtomicBitVectorImpl(size_t sz) : Base(byte_sz(sz)) {}

    RT get(size_t idx) const final override {
        T res;
        const auto widx       = word_idx(idx);
        const auto bidx       = inner_bit_idx(idx);
        const auto wptr       = &((T *)Base::data())[widx];
        const DT mixed_dword  = *(DT *)wptr;
        const T extracted_val = (T)extract_bits(mixed_dword, bidx, bidx + NBits);
        if constexpr (!Signed) {
            res = extracted_val;
        } else {
            res = sign_extend(extracted_val, NBits);
        }
        return res;
    }

    void set(size_t idx, RT val) final override {
        const auto widx          = word_idx(idx);
        const auto bidx          = inner_bit_idx(idx);
        const auto wptr          = &((T *)Base::data())[widx];
        const DT mixed_dword     = *(DT *)wptr;
        const DT new_mixed_dword = insert_bits(mixed_dword, T(val), bidx, NBits);
        *(DT *)wptr              = new_mixed_dword;
    }

    static constexpr size_t bit_idx(size_t idx) {
        return NBits * idx;
    }

    static constexpr size_t word_idx(size_t idx) {
        return bit_idx(idx) / TBits;
    }

    static constexpr size_t even_inner_bit_idx(size_t idx) {
        return bit_idx(idx) % (2 * TBits);
    }

    static constexpr size_t odd_inner_bit_idx(size_t idx) {
        return (bit_idx(idx) + TBits) % (2 * TBits);
    }

    static constexpr size_t inner_bit_idx(size_t idx) {
        if (word_idx(idx) % 2 == 0) {
            return even_inner_bit_idx(idx);
        } else {
            return odd_inner_bit_idx(idx);
        }
    }

    static constexpr size_t byte_sz(size_t sz) {
        const auto total_packed_bits  = NBits * sz;
        const auto write_total_bit_sz = ((total_packed_bits + DTBits - 1) / DTBits) * DTBits;
        return write_total_bit_sz / 8;
    }
};

template <uint8_t NBits, bool Signed> class AtomicBitVectorImpl : public BitVectorBase<Signed> {
public:
    using Base                      = BitVectorBase<Signed>;
    using RT                        = typename Base::RT;
    using T                         = int_n<NBits, Signed>;
    static constexpr size_t TBits   = sizeofbits<T>();
    using QT                        = std::atomic<uint_n<sizeofbits<T>() * 4>>;
    static constexpr uint8_t QTBits = sizeofbits<QT>();
    static_assert(NBits <= Base::RTBits);
    static_assert(NBits != 8 && NBits != 16 && NBits != 32);

    AtomicBitVectorImpl(size_t sz) : Base(byte_sz(sz)) {}

    RT get(size_t idx) const final override {
        (void)idx;
        assert(!"AtomicBitVectorImpl::get() not implemented");
        return 0;
    }

    void set(size_t idx, RT val) final override {
        (void)idx;
        (void)val;
        assert(!"AtomicBitVectorImpl::set() not implemented");
        return;
    }

    static constexpr size_t byte_sz(size_t sz) {
        const auto total_packed_bits  = NBits * sz;
        const auto write_total_bit_sz = ((total_packed_bits + QTBits - 1) / QTBits) * QTBits;
        return write_total_bit_sz / 8;
    }
};

template <uint8_t NBitsMax, bool Signed = false, bool AtomicWrite = false>
class XNUTRACE_EXPORT BitVector {
public:
    using T = int_n<NBitsMax, Signed>;
    static_assert(NBitsMax > 0 && NBitsMax <= 32);
    BitVector(uint8_t nbits, size_t sz) {
        assert(nbits > 0 && nbits <= NBitsMax);
        if (nbits >= 8 && nbits <= 32 && is_pow2(nbits)) {
            if (nbits == 8) {
                m_bv = std::make_unique<ExactBitVectorImpl<8, Signed>>(sz);
            } else if (nbits == 16) {
                m_bv = std::make_unique<ExactBitVectorImpl<16, Signed>>(sz);
            } else if (nbits == 32) {
                m_bv = std::make_unique<ExactBitVectorImpl<32, Signed>>(sz);
            }
        } else {
            const auto bit_tuple = hana::to_tuple(hana::range_c<uint8_t, 1, 32>);
            if constexpr (!AtomicWrite) {
                hana::for_each(bit_tuple, [&](const auto n) {
                    if constexpr (!(n.value >= 8 && n.value <= 32 && is_pow2(n.value))) {
                        if (n.value == nbits) {
                            m_bv = std::make_unique<NonAtomicBitVectorImpl<n.value, Signed>>(sz);
                        }
                    }
                });
            } else {
                hana::for_each(bit_tuple, [&](const auto n) {
                    if constexpr (!(n.value >= 8 && n.value <= 32 && is_pow2(n.value))) {
                        if (n.value == nbits) {
                            m_bv = std::make_unique<AtomicBitVectorImpl<n.value, Signed>>(sz);
                        }
                    }
                });
            }
        }
    }
    T get(size_t idx) const {
        return m_bv->get(idx);
    }
    void set(size_t idx, T val) {
        m_bv->set(idx, val);
    }

private:
    std::unique_ptr<BitVectorBase<Signed>> m_bv;
};

} // namespace xnutrace::BitVector

template <uint8_t NBits, bool Signed = false>
using ExactBitVectorImpl = xnutrace::BitVector::ExactBitVectorImpl<NBits, Signed>;

template <uint8_t NBits, bool Signed = false>
using NonAtomicBitVectorImpl = xnutrace::BitVector::NonAtomicBitVectorImpl<NBits, Signed>;

template <uint8_t NBits, bool Signed = false>
using AtomicBitVectorImpl = xnutrace::BitVector::AtomicBitVectorImpl<NBits, Signed>;

template <uint8_t NBits, bool Signed = false>
using BitVector = xnutrace::BitVector::BitVector<NBits, Signed, false>;

template <uint8_t NBits, bool Signed = false>
using AtomicBitVector = xnutrace::BitVector::BitVector<NBits, Signed, true>;