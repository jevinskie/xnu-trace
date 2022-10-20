#include "xnu-trace/xnu-trace.h"

#include <cstdint>

#include <benchmark/benchmark.h>

static uint64_t hash_n(uint8_t nbits, uint64_t val) {
    return xxhash3_64::hash(val) & xnutrace::BitVector::bit_mask<uint64_t>(0, nbits);
}

__attribute__((noinline)) static uint64_t
test_non_atomic_bitvector(const std::unique_ptr<BitVector<>> &bv, size_t sz) {
    size_t i     = 0;
    uint64_t res = 0;
    for (size_t i = 0; i < 512 * 1024 * 1024; ++i) {
        res ^= bv->get(i % sz);
    }
    return res;
}

int main() {
    constexpr uint8_t nbits = 31;
    constexpr size_t sz     = 128 * 1024 * 1024 / sizeof(uint32_t);
    auto bv                 = BitVectorFactory<>(nbits, sz);
    for (size_t i = 0; i < sz; ++i) {
        bv->set(i, hash_n(i, nbits));
    }
    const auto res = test_non_atomic_bitvector(bv, sz);
    return (int)res;
}
