#include "xnu-trace/BitVector.h"

XNUTRACE_NOINLINE
uint64_t psum_get(NonAtomicBitVectorImpl<17> &bv, size_t num) {
    uint64_t sum = 0;
    for (size_t i = 0; i < num; ++i) {
        sum += bv.get(i);
    }
    return sum;
}

XNUTRACE_NOINLINE
uint64_t psum_op(NonAtomicBitVectorImpl<17> &bv, size_t num) {
    uint64_t sum = 0;
    for (size_t i = 0; i < num; ++i) {
        sum += bv[i];
    }
    return sum;
}

XNUTRACE_NOINLINE
uint64_t psum_get_te(BitVector<> &bv, size_t num) {
    uint64_t sum = 0;
    for (size_t i = 0; i < num; ++i) {
        sum += bv.get(i);
    }
    return sum;
}

XNUTRACE_NOINLINE
uint64_t psum_op_te(BitVector<> &bv, size_t num) {
    uint64_t sum = 0;
    for (size_t i = 0; i < num; ++i) {
        sum += bv[i];
    }
    return sum;
}

int main() {
    auto bv = NonAtomicBitVectorImpl<17>(8);
    return psum_get(bv, 5) + psum_op(bv, 5) + psum_get_te(bv, 5) + psum_op_te(bv, 5);
}
