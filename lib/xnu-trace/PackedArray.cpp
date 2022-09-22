#include "xnu-trace/PackedArray.h"
#include "common-internal.h"

static_assert(std::is_same_v<uint_n<0>, void>, "");
static_assert(std::is_same_v<uint_n<1>, uint8_t>, "");
static_assert(std::is_same_v<uint_n<9>, uint16_t>, "");
static_assert(std::is_same_v<uint_n<17>, uint32_t>, "");
static_assert(std::is_same_v<uint_n<33>, uint64_t>, "");
static_assert(std::is_same_v<uint_n<65>, void>, "");
