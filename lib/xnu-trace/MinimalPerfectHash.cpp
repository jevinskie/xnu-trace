#include "xnu-trace/MinimalPerfectHash.h"
#include "common-internal.h"

#define XXH_INLINE_ALL
#define XXH_NAMESPACE xnu_trace_mph_
#include <xxhash.h>

uint64_t xxhash_64::hash(uint64_t val, uint64_t seed) {
    return XXH64(reinterpret_cast<char const *>(&val), sizeof(val), seed);
}

template <typename KeyT, typename Hasher>
MinimalPerfectHash<KeyT, Hasher>::MinimalPerfectHash(const std::vector<KeyT> &keys) {}

template <typename KeyT, typename Hasher>
uint32_t MinimalPerfectHash<KeyT, Hasher>::lookup(KeyT key) {
    return 0;
}
