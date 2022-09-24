#include "xnu-trace/xnu-trace.h"

#undef NDEBUG
#include <cassert>
#include <vector>

#include <fmt/format.h>

int main(int argc, const char **argv) {
    if (argc != 2) {
        fmt::print("usage: {:s} <keys.bin>\n");
        return -1;
    }

    Signpost("mph", "mph build start", true);

    const auto buf     = read_file(argv[1]);
    const auto raw_buf = (uint64_t *)buf.data();
    const auto nkeys   = buf.size() / sizeof(uint64_t);
    std::vector<uint64_t> keys;
    keys.reserve(nkeys);
    for (size_t i = 0; i < nkeys; ++i) {
        keys.emplace_back(raw_buf[i]);
    }

    auto mph = MinimalPerfectHash<uint64_t>{};
    mph.build(keys);

    return 0;
}
