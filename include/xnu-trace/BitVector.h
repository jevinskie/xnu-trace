#pragma once

#include "common.h"

#include "utils.h"

#undef NDEBUG
#include <cassert>
#include <memory>

#include <boost/hana.hpp>
namespace hana = boost::hana;

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

template <typename T, bool Signed = false> class GetSetIdxBase {
public:
    using type = T;
    GetSetIdxBase(size_t sz) : m_sz(sz) {}
    virtual ~GetSetIdxBase() {}
    virtual T get(size_t idx) const noexcept     = 0;
    virtual void set(size_t idx, T val) noexcept = 0;

    size_t size() const noexcept {
        return m_sz;
    }

private:
    const size_t m_sz;
};

template <uint8_t NBitsMax, bool Signed = false>
class BitVectorBase : public GetSetIdxBase<int_n<NBitsMax, Signed>> {
public:
    static_assert(NBitsMax > 0 && NBitsMax <= 64);
    using Base                     = GetSetIdxBase<int_n<NBitsMax, Signed>>;
    using RT                       = typename Base::type;
    static constexpr size_t RTBits = sizeofbits<RT>();

protected:
    BitVectorBase(size_t sz, size_t byte_sz) : Base(sz), m_buf(byte_sz) {}

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

template <uint8_t NBitsMax, uint8_t NBits, bool Signed>
class ExactBitVectorImpl : public BitVectorBase<NBitsMax, Signed> {
public:
    using Base = BitVectorBase<NBitsMax, Signed>;
    using RT   = typename Base::RT;
    using T    = int_n<NBits, Signed>;
    static_assert(NBits <= Base::RTBits);
    static_assert(NBits >= 8 && is_pow2(NBits));

    ExactBitVectorImpl(size_t sz) : Base(sz, byte_sz(sz)) {}

    RT get(size_t idx) const noexcept final override {
        return ((T *)Base::data())[idx];
    }

    void set(size_t idx, RT val) noexcept final override {
        ((T *)Base::data())[idx] = T(val);
    }

    static constexpr size_t byte_sz(size_t sz) {
        return sz * NBits / 8;
    }
};

template <uint8_t NBitsMax, uint8_t NBits, bool Signed>
class NonAtomicBitVectorImpl : public BitVectorBase<NBitsMax, Signed> {
public:
    using Base                      = BitVectorBase<NBitsMax, Signed>;
    using RT                        = typename Base::RT;
    using T                         = int_n<NBits, Signed>;
    static constexpr size_t TBits   = sizeofbits<T>();
    using DT                        = uint_n<sizeofbits<T>() * 2>;
    static constexpr uint8_t DTBits = sizeofbits<DT>();
    static_assert(NBits <= Base::RTBits);
    static_assert(NBits != 8 && NBits != 16 && NBits != 32 && NBits != 64);

    NonAtomicBitVectorImpl(size_t sz) : Base(sz, byte_sz(sz)) {}

    RT get(size_t idx) const noexcept final override {
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

    void set(size_t idx, RT val) noexcept final override {
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

template <uint8_t NBitsMax, uint8_t NBits, bool Signed>
class AtomicBitVectorImpl : public BitVectorBase<NBitsMax, Signed> {
public:
    using Base                      = BitVectorBase<NBitsMax, Signed>;
    using RT                        = typename Base::RT;
    using T                         = int_n<NBits, Signed>;
    static constexpr size_t TBits   = sizeofbits<T>();
    using DT                        = std::atomic<uint_n<sizeofbits<T>() * 2>>;
    static constexpr uint8_t DTBits = sizeofbits<DT>();
    static_assert(NBits <= Base::RTBits);
    static_assert(NBits != 8 && NBits != 16 && NBits != 32 && NBits != 64);

    AtomicBitVectorImpl(size_t sz) : Base(sz, byte_sz(sz)) {}

    RT get(size_t idx) const noexcept final override {
        (void)idx;
        assert(!"AtomicBitVectorImpl::get() not implemented");
        return 0;
    }

    void set(size_t idx, RT val) noexcept final override {
        (void)idx;
        (void)val;
        assert(!"AtomicBitVectorImpl::set() not implemented");
        return;
    }

    static constexpr size_t byte_sz(size_t sz) {
        const auto total_packed_bits  = NBits * sz;
        const auto write_total_bit_sz = ((total_packed_bits + DTBits - 1) / DTBits) * DTBits;
        return write_total_bit_sz / 8;
    }
};

template <uint8_t NBitsMax, bool Signed = false, bool AtomicWrite = false>
std::unique_ptr<BitVectorBase<NBitsMax, Signed>> BitVectorFactory(uint8_t nbits, size_t sz) {
    static_assert(NBitsMax > 0 && NBitsMax <= 64);
    assert(nbits > 0 && nbits <= NBitsMax);
    std::unique_ptr<BitVectorBase<NBitsMax, Signed>> res;
    if (nbits >= 8 && nbits <= 64 && is_pow2(nbits)) {
        if (nbits == 8) {
            if constexpr (NBitsMax >= 8) {
                res = std::make_unique<ExactBitVectorImpl<NBitsMax, 8, Signed>>(sz);
            }
        } else if (nbits == 16) {
            if constexpr (NBitsMax >= 16) {

                res = std::make_unique<ExactBitVectorImpl<NBitsMax, 16, Signed>>(sz);
            }
        } else if (nbits == 32) {
            if constexpr (NBitsMax >= 32) {
                res = std::make_unique<ExactBitVectorImpl<NBitsMax, 32, Signed>>(sz);
            }
        } else if (nbits == 64) {
            if constexpr (NBitsMax >= 64) {
                res = std::make_unique<ExactBitVectorImpl<NBitsMax, 64, Signed>>(sz);
            }
        }
    } else {
        const auto bit_tuple = hana::to_tuple(hana::range_c<uint8_t, 1, 64>);
        if constexpr (!AtomicWrite) {
            hana::for_each(bit_tuple, [&](const auto n) {
                if constexpr (!(n.value >= 8 && n.value <= 64 && is_pow2(n.value))) {
                    if (n.value == nbits) {
                        if constexpr (NBitsMax >= n.value) {
                            res =
                                std::make_unique<NonAtomicBitVectorImpl<NBitsMax, n.value, Signed>>(
                                    sz);
                        }
                    }
                }
            });
        } else {
            hana::for_each(bit_tuple, [&](const auto n) {
                if constexpr (!(n.value >= 8 && n.value <= 64 && is_pow2(n.value))) {
                    if (n.value == nbits) {
                        if constexpr (NBitsMax >= n.value) {
                            res = std::make_unique<AtomicBitVectorImpl<NBitsMax, n.value, Signed>>(
                                sz);
                        }
                    }
                }
            });
        }
    }
    return res;
}

} // namespace xnutrace::BitVector

template <uint8_t NBitsMax, bool Signed = false>
using BitVector = xnutrace::BitVector::BitVectorBase<NBitsMax, Signed>;

template <uint8_t NBitsMax, uint8_t NBits, bool Signed = false>
using ExactBitVectorImpl = xnutrace::BitVector::ExactBitVectorImpl<NBitsMax, NBits, Signed>;

template <uint8_t NBitsMax, uint8_t NBits, bool Signed = false>
using NonAtomicBitVectorImpl = xnutrace::BitVector::NonAtomicBitVectorImpl<NBitsMax, NBits, Signed>;

template <uint8_t NBitsMax, uint8_t NBits, bool Signed = false>
using AtomicBitVectorImpl = xnutrace::BitVector::AtomicBitVectorImpl<NBitsMax, NBits, Signed>;

template <uint8_t NBitsMax, bool Signed = false>
std::unique_ptr<BitVector<NBitsMax, Signed>> BitVectorFactory(uint8_t nbits, size_t sz) {
    return xnutrace::BitVector::BitVectorFactory<NBitsMax, Signed, false>(nbits, sz);
}

template <uint8_t NBitsMax, bool Signed = false>
std::unique_ptr<BitVector<NBitsMax, Signed>> AtomicBitVectorFactory(uint8_t nbits, size_t sz) {
    return xnutrace::BitVector::BitVectorFactory<NBitsMax, Signed, true>(nbits, sz);
}