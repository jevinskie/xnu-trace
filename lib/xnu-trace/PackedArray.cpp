#include "xnu-trace/PackedArray.h"
#include "common-internal.h"

static uint64_t nbit_get(const uint8_t (&buf)[], size_t pos, uint8_t nbits) {
    const auto bit_idx = pos * nbits;
    return 0;
}

PackedArray::reference::reference(PackedArray &pa, size_t pos) : m_pos{pos}, m_pa{pa} {}

PackedArray::reference::operator uint64_t() const {}

PackedArray::PackedArray(size_t sz, uint8_t nbits) : m_sz{sz}, m_nbits{nbits} {
    assert(nbits > 0 && nbits <= 64);
    const auto total_num_bits = sz * nbits;
    const auto total_num_bytes =
        total_num_bits % 8 ? (size_t)(total_num_bits / 8.0 + 1) : total_num_bits / 8;
    m_buf = std::make_unique<uint8_t[]>(total_num_bytes);
}

static_assert(std::is_same_v<uint_n<0>, void>, "");
static_assert(std::is_same_v<uint_n<1>, uint8_t>, "");
static_assert(std::is_same_v<uint_n<9>, uint16_t>, "");
static_assert(std::is_same_v<uint_n<17>, uint32_t>, "");
static_assert(std::is_same_v<uint_n<33>, uint64_t>, "");
static_assert(std::is_same_v<uint_n<65>, void>, "");
