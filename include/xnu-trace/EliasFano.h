#pragma once

#include "common.h"

#include "BitVector.h"

#include <span>

class EliasFanoSequence {
public:
    using T = BitVector<>::RT;

    template <typename IT>
    EliasFanoSequence(std::span<const IT> sorted_seq)
        : m_sz{sorted_seq.size()}, m_max{sorted_seq[sorted_seq.size() - 1]} {
        static_assert(sizeof(IT) <= sizeof(T));
    }

    T size() const noexcept {
        return m_sz;
    }

    T max() const noexcept {
        return m_max;
    }

private:
    const T m_sz;
    const T m_max;
};
