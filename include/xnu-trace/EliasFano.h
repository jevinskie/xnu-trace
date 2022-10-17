#pragma once

#include "common.h"

#include "BitVector.h"

#include <cmath>
#include <span>

template <uint8_t NBitsMax> class XNUTRACE_EXPORT EliasFanoSequence {
public:
    using T = typename BitVector<NBitsMax>::RT;

    template <typename IT>
    EliasFanoSequence(std::span<const IT> sorted_seq)
        : m_n(sorted_seq.size()), m_m{sorted_seq[sorted_seq.size() - 1]}, m_cln(ceil(log2(m_n))),
          m_clm(ceil(log2(m_m))), m_clmdn(ceil(log2(m_m) - log2(m_n))) {
        static_assert(sizeof(IT) <= sizeof(T));
        m_lo = BitVectorFactory<NBitsMax>(m_clmdn, m_n);
    }

    T size() const noexcept {
        return m_n;
    }
    T max() const noexcept {
        return m_m;
    }

private:
    const T m_n;
    const T m_m;
    const uint8_t m_cln;
    const uint8_t m_clm;
    const uint8_t m_clmdn;
    std::unique_ptr<BitVector<NBitsMax>> m_lo;
};
