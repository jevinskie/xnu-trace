#include "xnu-trace/BitVector.h"

int main() {
    auto bv = NonAtomicBitVectorImpl<17>(8);
    bv[0]   = 1;
    return bv[0] == 1;
}
