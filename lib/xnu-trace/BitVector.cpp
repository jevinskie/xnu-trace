#include "xnu-trace/BitVector.h"
#include "common-internal.h"

template class BitVectorBase<8, false>;
template class BitVectorBase<8, true>;
template class BitVectorBase<16, false>;
template class BitVectorBase<16, true>;
template class BitVectorBase<32, false>;
template class BitVectorBase<32, true>;
