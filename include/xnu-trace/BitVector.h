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
    const T mask      = high_mask ^ low_mask;
    // fmt::print("bm({:d}, {:d}) = {:#018b}\n", sb, eb, mask);
    // return high_mask ^ low_mask;
    return mask;
}

template <typename T> static constexpr T extract_bits(T val, uint8_t sb, uint8_t eb) {
    const T res = (val & bit_mask<T>(sb, eb)) >> sb;
    // fmt::print("eb({:d}, {:d}) = {:#018b}\n", sb, eb, res);
    return res;
}

template <typename T>
static constexpr T insert_bits(T orig_val, auto insert_val, uint8_t sb, uint8_t nbits) {
    const T orig_val_cleared = orig_val & ~bit_mask<T>(sb, sb + nbits);
    return orig_val_cleared | (insert_val << sb);
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

    NonAtomicBitVector(size_t sz) : Base(byte_sz(sz)) {
        fmt::print("NonAtomicBitVector NBits: {:d} sz: {:d} data: {:p}\n", NBits, sz,
                   fmt::ptr(Base::data()));
    }

    T get(size_t idx) const final override {
        T res;
        const auto sw_idx  = start_word_idx(idx);
        const auto ew_idx  = end_word_idx(idx);
        const auto sdw_idx = sw_idx / 2;
        const auto wptr    = &((T *)Base::data())[sw_idx];
        const auto dwptr   = (DT *)wptr;
        // fmt::print("get idx: {:d} sw_idx: {:d} ew_idx: {:d} sdw_idx: {:d} ptr: {:p}\n", idx,
        // sw_idx,
        //            ew_idx, sdw_idx, fmt::ptr(ptr));
        // const DT mixed_dword     = ((DT *)Base::data())[sdw_idx];
        const DT mixed_dword     = *dwptr;
        const auto sd_bidx       = start_bit_idx(idx) % DTBits;
        const auto ed_bidx       = end_bit_idx(idx) % DTBits;
        const auto extracted_val = (T)extract_bits(mixed_dword, sd_bidx, ed_bidx);
        if constexpr (!Signed) {
            res = extracted_val;
        } else {
            res = sign_extend(extracted_val, NBits);
        }
        IC("get"s, idx, sw_idx, ew_idx, sdw_idx, wptr, sd_bidx, ed_bidx);
        // fmt::print("sd_bidx: {:d} ed_bidx: {:d} res: {:#010x}\n", sd_bidx, ed_bidx, res);
        return res;
    }

    void set(size_t idx, T val) final override {
        const auto sb_idx  = start_bit_idx(idx);
        const auto eb_idx  = end_bit_idx(idx);
        const auto sw_idx  = start_word_idx(idx);
        const auto ew_idx  = end_word_idx(idx);
        const auto sdw_idx = sw_idx / 2;
        const auto wptr    = &((T *)Base::data())[sw_idx];
        const auto dwptr   = (DT *)wptr;
        // fmt::print(
        //     "set idx: {:d} sw_idx: {:d} ew_idx: {:d} sdw_idx: {:d} ptr: {:p} val: {:#010x}\n",
        //     idx, sw_idx, ew_idx, sdw_idx, fmt::ptr(ptr), val);
        // const DT mixed_dword     = ((DT *)Base::data())[sdw_idx];
        const DT mixed_dword     = *(DT *)dwptr;
        const auto sd_bidx       = start_bit_idx(idx) % DTBits;
        const auto ed_bidx       = end_bit_idx(idx) % DTBits;
        const DT new_mixed_dword = insert_bits(mixed_dword, val, sd_bidx, NBits);
        *dwptr                   = new_mixed_dword;
        IC("set"s, idx, sb_idx, eb_idx, sw_idx, ew_idx, sdw_idx, wptr, sd_bidx, ed_bidx);
        fmt::print("val:             {:#034b}\n", val);
        fmt::print("mixed_dword:     {:#066b}\n", mixed_dword);
        fmt::print("new_mixed_dword: {:#066b}\n", new_mixed_dword);
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
        return write_total_bit_sz / 8;
    }
};

template <uint8_t NBits, bool Signed> class AtomicBitVector : public BitVectorBase<NBits, Signed> {
public:
    using Base                      = BitVectorBase<NBits, Signed>;
    using T                         = typename Base::T;
    static constexpr size_t TBits   = Base::TBits;
    using QT                        = std::atomic<uint_n<sizeofbits<T>() * 4>>;
    static constexpr uint8_t QTBits = sizeofbits<QT>();
    static_assert_cond(NBits != 8 && NBits != 16 && NBits != 32);

    AtomicBitVector(size_t sz) : Base(byte_sz(sz)) {}

    T get(size_t idx) const final override {
        return 0;
    }

    void set(size_t idx, T val) final override {
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
    static_assert_cond(NBitsMax > 0 && NBitsMax <= 32);
    BitVector(uint8_t nbits, size_t sz) {
        if (nbits >= 8 && is_pow2(nbits)) {
            if (nbits == 8) {
                m_bv = std::make_unique<ExactBitVector<8, Signed>>(sz);
            } else if (nbits == 16) {
                m_bv = std::make_unique<ExactBitVector<16, Signed>>(sz);
            } else if (nbits == 32) {
                m_bv = std::make_unique<ExactBitVector<32, Signed>>(sz);
            }
        } else {
            const auto bit_tuple = hana::to_tuple(hana::range_c<uint8_t, 1, 32>);
            if constexpr (!AtomicWrite) {
                hana::for_each(bit_tuple, [&](const auto n) {
                    if (n.value == nbits) {
                        m_bv = std::make_unique<NonAtomicBitVector<n.value, Signed>>(sz);
                    }
                });
            } else {
                hana::for_each(bit_tuple, [&](const auto n) {
                    if (n.value == nbits) {
                        m_bv = std::make_unique<AtomicBitVector<n.value, Signed>>(sz);
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
    std::unique_ptr<BitVectorBase<NBitsMax, Signed>> m_bv;
};

} // namespace xnutrace::BitVector

template <uint8_t NBits, bool Signed = false>
using BitVectorBase = xnutrace::BitVector::BitVectorBase<NBits, Signed>;

template <uint8_t NBits, bool Signed = false>
using ExactBitVector = xnutrace::BitVector::ExactBitVector<NBits, Signed>;

template <uint8_t NBits, bool Signed = false>
using NonAtomicBitVector = xnutrace::BitVector::NonAtomicBitVector<NBits, Signed>;

template <uint8_t NBits, bool Signed = false>
using AtomicBitVector = xnutrace::BitVector::AtomicBitVector<NBits, Signed>;

template <uint8_t NBits, bool Signed = false>
using BitVector = xnutrace::BitVector::BitVector<NBits, Signed>;