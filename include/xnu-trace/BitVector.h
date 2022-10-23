#pragma once

#include "common.h"

#include "utils.h"

#undef NDEBUG
#include <cassert>
#include <memory>
#include <utility>

#include <boost/hana.hpp>
namespace hana = boost::hana;
#include <fmt/format.h>
#include <range/v3/iterator/basic_iterator.hpp>

namespace xnutrace::BitVector {

static constexpr uint8_t DefaultNBitsMax = 64;

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

template <typename T, typename IT>
static void insert_bits_atomic(T &orig_val, IT insert_val, uint8_t sb, uint8_t nbits) {
    orig_val.fetch_and(~bit_mask<T>(sb, sb + nbits));
    orig_val.fetch_or((T(insert_val) << sb));
}

template <typename T> static constexpr T sign_extend(T val, uint8_t nbits) {
    const T msb = 1 << (nbits - 1);
    return (val ^ msb) - msb;
}

template <typename T> class GetSetIdxBase {
public:
    using value_type = T;

    template <bool Const> struct RangeProxyCursor {
        using difference_type = ssize_t;
        using value_type      = T;

        std::conditional_t<Const, const GetSetIdxBase<T>, GetSetIdxBase<T>> *m_tbl{};
        size_t m_idx{};

        value_type read() const noexcept {
            return m_tbl->get(m_idx);
        }
        void write(value_type val) const noexcept {
            m_tbl->set(m_idx, val);
        }
        bool equal(RangeProxyCursor<Const> other) const noexcept {
            assert(m_tbl == other.m_tbl);
            return m_idx == other.m_idx;
        }
        void next() noexcept {
            ++m_idx;
        }
        void prev() noexcept {
            --m_idx;
        }
        void advance(ssize_t i) noexcept {
            m_idx += i;
        }
        ssize_t distance_to(RangeProxyCursor<Const> other) const noexcept {
            return other.m_idx - m_idx;
        }
    };

    GetSetIdxBase(size_t sz) : m_sz(sz) {}
    virtual ~GetSetIdxBase() {}
    virtual T get(size_t idx) const noexcept     = 0;
    virtual void set(size_t idx, T val) noexcept = 0;

    size_t size() const noexcept {
        return m_sz;
    }

    using iterator       = ranges::basic_iterator<RangeProxyCursor<false>>;
    using const_iterator = ranges::basic_iterator<RangeProxyCursor<true>>;
    iterator begin() {
        return iterator(RangeProxyCursor<false>{this, 0});
    }
    iterator end() {
        return iterator(RangeProxyCursor<false>{this, size()});
    }
    const_iterator begin() const {
        return cbegin();
    }
    const_iterator end() const {
        return cend();
    }
    const_iterator cbegin() const {
        return const_iterator(RangeProxyCursor<true>{&std::as_const(*this), 0});
    }
    const_iterator cend() const {
        return const_iterator(RangeProxyCursor<true>{&std::as_const(*this), size()});
    }

private:
    const size_t m_sz;
};

template <bool Signed = false, uint8_t NBitsMax = DefaultNBitsMax>
class BitVectorBase : public GetSetIdxBase<int_n<NBitsMax, Signed>> {
public:
    static_assert(NBitsMax > 0 && NBitsMax <= 64);
    using Base                     = GetSetIdxBase<int_n<NBitsMax, Signed>>;
    using RT                       = typename Base::value_type;
    static constexpr size_t RTBits = sizeofbits<RT>();

    uint8_t *data() {
        return m_buf.data();
    }
    const uint8_t *data() const {
        return m_buf.data();
    }

protected:
    // padded to 16 bytes (alignment)
    BitVectorBase(size_t sz, size_t byte_sz) : Base(sz), m_buf(roundup_pow2_mul(byte_sz, 16)) {}

    std::vector<uint8_t> &buf() {
        return m_buf;
    }
    const std::vector<uint8_t> &buf() const {
        return m_buf;
    }

private:
    std::vector<uint8_t> m_buf;
};

template <uint8_t NBits, bool Signed = false, uint8_t NBitsMax = DefaultNBitsMax>
class ExactBitVectorImpl : public BitVectorBase<Signed, NBitsMax> {
public:
    using Base = BitVectorBase<Signed, NBitsMax>;
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

    size_t bit_sz() const {
        return Base::size() * NBits;
    }
    static constexpr size_t byte_sz(size_t sz) {
        return sz * NBits / 8;
    }
};

template <bool Signed = false, uint8_t NBitsMax = DefaultNBitsMax>
class NonAtomicSingleBitVectorImpl : public BitVectorBase<Signed, NBitsMax> {
public:
    static constexpr uint8_t NBits = 1;
    using Base                     = BitVectorBase<Signed, NBitsMax>;
    using RT                       = typename Base::RT;
    using T                        = uint8_t;
    static constexpr size_t TBits  = sizeofbits<T>();

    NonAtomicSingleBitVectorImpl(size_t sz) : Base(sz, byte_sz(sz)) {}

    RT get(size_t idx) const noexcept final override {
        const auto widx = word_idx(idx);
        const auto bidx = inner_bit_idx(idx);
        return extract_bits(Base::data()[widx], bidx, bidx + NBits);
    }

    void set(size_t idx, RT val) noexcept final override {
        const auto widx = word_idx(idx);
        const auto bidx = inner_bit_idx(idx);
        const auto wptr = &Base::data()[widx];
        *wptr           = insert_bits(*wptr, T(val), bidx, NBits);
    }

    static constexpr size_t word_idx(size_t idx) {
        return idx / TBits;
    }

    static constexpr size_t inner_bit_idx(size_t idx) {
        return idx % TBits;
    }

    size_t bit_sz() const {
        return Base::size() * NBits;
    }
    static constexpr size_t byte_sz(size_t sz) {
        return roundup_pow2_mul(sz, TBits) / 8;
    }
};

template <bool Signed = false, uint8_t NBitsMax = DefaultNBitsMax>
class AtomicSingleBitVectorImpl : public BitVectorBase<Signed, NBitsMax> {
public:
    static constexpr uint8_t NBits = 1;
    using Base                     = BitVectorBase<Signed, NBitsMax>;
    using RT                       = typename Base::RT;
    using T                        = uint8_t;
    static constexpr size_t TBits  = sizeofbits<T>();
    using AT                       = std::atomic<T>;

    AtomicSingleBitVectorImpl(size_t sz) : Base(sz, byte_sz(sz)) {}

    RT get(size_t idx) const noexcept final override {
        const auto widx = word_idx(idx);
        const auto bidx = inner_bit_idx(idx);
        return extract_bits(Base::data()[widx], bidx, bidx + NBits);
    }

    void set(size_t idx, RT val) noexcept final override {
        const auto widx = word_idx(idx);
        const auto bidx = inner_bit_idx(idx);
        const auto wptr = (AT *)&Base::data()[widx];
        if (get(idx) != val) {
            wptr->fetch_xor(bit_mask<T>(bidx, bidx + NBits));
        }
    }

    static constexpr size_t word_idx(size_t idx) {
        return idx / TBits;
    }

    static constexpr size_t inner_bit_idx(size_t idx) {
        return idx % TBits;
    }

    size_t bit_sz() const {
        return Base::size() * NBits;
    }
    static constexpr size_t byte_sz(size_t sz) {
        return roundup_pow2_mul(sz, TBits) / 8;
    }
};

template <uint8_t NBits, bool Signed = false, uint8_t NBitsMax = DefaultNBitsMax>
class NonAtomicBitVectorImpl : public BitVectorBase<Signed, NBitsMax> {
public:
    using Base                      = BitVectorBase<Signed, NBitsMax>;
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

    size_t bit_sz() const {
        return Base::size() * NBits;
    }
    static constexpr size_t byte_sz(size_t sz) {
        const auto total_packed_bits = NBits * sz;
        const auto write_total_bit_sz =
            ((total_packed_bits + TBits + DTBits - 1) / DTBits) * DTBits;
        return write_total_bit_sz / 8;
    }
};

template <uint8_t NBits, bool Signed = false, uint8_t NBitsMax = DefaultNBitsMax>
class AtomicBitVectorImpl : public BitVectorBase<Signed, NBitsMax> {
public:
    using Base                      = BitVectorBase<Signed, NBitsMax>;
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

    size_t bit_sz() const {
        return Base::size() * NBits;
    }
    static constexpr size_t byte_sz(size_t sz) {
        const auto total_packed_bits = NBits * sz;
        const auto write_total_bit_sz =
            ((total_packed_bits + TBits + DTBits - 1) / DTBits) * DTBits;
        return write_total_bit_sz / 8;
    }
};

template <bool Signed = false, bool AtomicWrite = false, uint8_t NBitsMax = DefaultNBitsMax>
std::unique_ptr<BitVectorBase<Signed, NBitsMax>> BitVectorFactory(uint8_t nbits, size_t sz) {
    static_assert(NBitsMax > 0 && NBitsMax <= 64);
    assert(nbits > 0 && nbits <= NBitsMax);
    std::unique_ptr<BitVectorBase<Signed, NBitsMax>> res;
    if (nbits == 1) {
        if constexpr (!AtomicWrite) {
            res = std::make_unique<NonAtomicSingleBitVectorImpl<Signed, NBitsMax>>(sz);
        } else {
            res = std::make_unique<AtomicSingleBitVectorImpl<Signed, NBitsMax>>(sz);
        }
    } else if (nbits >= 8 && nbits <= 64 && is_pow2(nbits)) {
        if (nbits == 8) {
            if constexpr (NBitsMax >= 8) {
                res = std::make_unique<ExactBitVectorImpl<8, Signed, NBitsMax>>(sz);
            }
        } else if (nbits == 16) {
            if constexpr (NBitsMax >= 16) {
                res = std::make_unique<ExactBitVectorImpl<16, Signed, NBitsMax>>(sz);
            }
        } else if (nbits == 32) {
            if constexpr (NBitsMax >= 32) {
                res = std::make_unique<ExactBitVectorImpl<32, Signed, NBitsMax>>(sz);
            }
        } else if (nbits == 64) {
            if constexpr (NBitsMax >= 64) {
                res = std::make_unique<ExactBitVectorImpl<64, Signed, NBitsMax>>(sz);
            }
        }
    } else {
        const auto bit_tuple = hana::to_tuple(hana::range_c<uint8_t, 2, 64>);
        if constexpr (!AtomicWrite) {
            hana::for_each(bit_tuple, [&](const auto n) {
                if constexpr (NBitsMax >= n.value &&
                              !(n.value >= 8 && n.value <= 64 && is_pow2(n.value))) {
                    if (n.value == nbits) {
                        res =
                            std::make_unique<NonAtomicBitVectorImpl<n.value, Signed, NBitsMax>>(sz);
                    }
                }
            });
        } else {
            hana::for_each(bit_tuple, [&](const auto n) {
                if constexpr (NBitsMax >= n.value &&
                              !(n.value >= 8 && n.value <= 64 && is_pow2(n.value))) {
                    if (n.value == nbits) {
                        res = std::make_unique<AtomicBitVectorImpl<n.value, Signed, NBitsMax>>(sz);
                    }
                }
            });
        }
    }
    return res;
}

} // namespace xnutrace::BitVector

template <bool Signed = false, uint8_t NBitsMax = xnutrace::BitVector::DefaultNBitsMax>
using BitVector = xnutrace::BitVector::BitVectorBase<Signed, NBitsMax>;

template <uint8_t NBits, bool Signed = false,
          uint8_t NBitsMax = xnutrace::BitVector::DefaultNBitsMax>
using ExactBitVectorImpl = xnutrace::BitVector::ExactBitVectorImpl<NBits, Signed, NBitsMax>;

template <bool Signed = false, uint8_t NBitsMax = xnutrace::BitVector::DefaultNBitsMax>
using NonAtomicSingleBitVectorImpl =
    xnutrace::BitVector::NonAtomicSingleBitVectorImpl<Signed, NBitsMax>;

template <bool Signed = false, uint8_t NBitsMax = xnutrace::BitVector::DefaultNBitsMax>
using AtomicSingleBitVectorImpl = xnutrace::BitVector::AtomicSingleBitVectorImpl<Signed, NBitsMax>;

template <uint8_t NBits, bool Signed = false,
          uint8_t NBitsMax = xnutrace::BitVector::DefaultNBitsMax>
using NonAtomicBitVectorImpl = xnutrace::BitVector::NonAtomicBitVectorImpl<NBits, Signed, NBitsMax>;

template <uint8_t NBits, bool Signed = false,
          uint8_t NBitsMax = xnutrace::BitVector::DefaultNBitsMax>
using AtomicBitVectorImpl = xnutrace::BitVector::AtomicBitVectorImpl<NBits, Signed, NBitsMax>;

template <bool Signed = false, uint8_t NBitsMax = xnutrace::BitVector::DefaultNBitsMax>
std::unique_ptr<BitVector<Signed, NBitsMax>> BitVectorFactory(uint8_t nbits, size_t sz) {
    return xnutrace::BitVector::BitVectorFactory<Signed, false, NBitsMax>(nbits, sz);
}

template <bool Signed = false, uint8_t NBitsMax = xnutrace::BitVector::DefaultNBitsMax>
std::unique_ptr<BitVector<Signed, NBitsMax>> AtomicBitVectorFactory(uint8_t nbits, size_t sz) {
    return xnutrace::BitVector::BitVectorFactory<Signed, true, NBitsMax>(nbits, sz);
}